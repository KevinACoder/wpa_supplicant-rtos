/*
 * @file
 * @brief Supplicant bring-up and the driver event queue for Embox.
 *
 * One kernel thread runs wpa_supplicant_init/add_iface/run; driver
 * threads only queue deep-copied events (wpa_embox_send_event), which
 * eloop_run() drains on the supplicant thread.  Control requests from
 * the shell (connect/status/...) are marshalled onto the eloop thread
 * as immediate timeouts so the supplicant structures are only ever
 * touched there.
 *
 * @date 08.09.2026
 * @author zhugengyu
 */

#include <includes.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "utils/common.h"
#include "utils/eloop.h"
#include "l2_packet/l2_packet.h"
#include "drivers/driver.h"
#include "wpa_supplicant_i.h"
#include "wpa_supplicant/config.h"
#include "wpa_supplicant/bss.h"

#include <kernel/time/time.h>
#include <kernel/thread.h>
#include <kernel/thread/sync/mutex.h>
#include <kernel/thread/sync/semaphore.h>

#include "wpa_embox_glue.h"
#include "wpa_supplicant/wmm_ac.h"

#define WPA_EMBOX_QUEUE_LEN 32
#define WPA_EMBOX_IFNAME "wlan0"

static struct wpa_global *wpa_embox_global;
static struct wpa_supplicant *wpa_embox_wpa_s;

/* the driver event queue */
static struct wpa_supplicant_event_msg *
    wpa_embox_queue[WPA_EMBOX_QUEUE_LEN];
static int wpa_embox_queue_head;
static int wpa_embox_queue_tail;
static struct mutex wpa_embox_queue_mtx;
static struct sem wpa_embox_queue_sem;

/* marshalled control jobs */
struct wpa_embox_job {
	void (*fn)(void *arg);
	void *arg;
	int done;
};

static struct mutex wpa_embox_job_mtx;
static struct wpa_embox_job *wpa_embox_job;
static struct sem wpa_embox_job_done;
static volatile int wpa_embox_ready;

/* the ops table lives in driver_embox.c; it reaches wpa_supplicant.c
 * through drivers.c (CONFIG_DRIVER_EMBOX) */

/* ------------------------------------------------------------------ */
/* driver event queue */

static void wpa_embox_event_copy_free(int event,
    union wpa_event_data *data) {

	if (event == EVENT_EAPOL_RX) {
		os_free((void *) data->eapol_rx.src);
		os_free((void *) data->eapol_rx.data);
	} else if (event == EVENT_DEAUTH) {
		os_free((void *) data->deauth_info.addr);
		os_free((void *) data->deauth_info.ie);
	} else if (event == EVENT_ASSOC) {
		os_free((void *) data->assoc_info.addr);
	}
}

int wpa_embox_send_event(void *ctx, int event, const void *data) {
	struct wpa_supplicant_event_msg *msg;
	union wpa_event_data *copy = NULL;

	msg = os_zalloc(sizeof(*msg));
	if (msg == NULL) {
		return -1;
	}
	msg->ctx = ctx;
	msg->event = event;

	if (data != NULL) {
		copy = os_zalloc(sizeof(*copy));
		if (copy == NULL) {
			os_free(msg);
			return -1;
		}
		os_memcpy(copy, data, sizeof(*copy));

		/* duplicate the pointer payloads the producer does not
		 * own after returning */
		if (event == EVENT_EAPOL_RX) {
			uint8_t *src = os_memdup(copy->eapol_rx.src,
			    ETH_ALEN);
			uint8_t *buf = os_memdup(copy->eapol_rx.data,
			    copy->eapol_rx.data_len);

			if (src == NULL || buf == NULL) {
				os_free(src);
				os_free(buf);
				os_free(copy);
				os_free(msg);
				return -1;
			}
			copy->eapol_rx.src = src;
			copy->eapol_rx.data = buf;
		} else if (event == EVENT_DEAUTH) {
			uint8_t *addr = os_memdup(copy->deauth_info.addr,
			    ETH_ALEN);

			if (addr == NULL) {
				os_free(copy);
				os_free(msg);
				return -1;
			}
			copy->deauth_info.addr = addr;
		} else if (event == EVENT_ASSOC) {
			uint8_t *addr = NULL;

			if (copy->assoc_info.addr != NULL) {
				addr = os_memdup(copy->assoc_info.addr,
				    ETH_ALEN);
				if (addr == NULL) {
					os_free(copy);
					os_free(msg);
					return -1;
				}
			}
			copy->assoc_info.addr = addr;
		} else {
			/* events with scalar-only payloads arrive with
			 * data == NULL from the driver */
			os_free(copy);
			copy = NULL;
		}
	}
	msg->data = copy;

	mutex_lock(&wpa_embox_queue_mtx);
	if ((wpa_embox_queue_tail + 1) % WPA_EMBOX_QUEUE_LEN ==
	    wpa_embox_queue_head) {
		mutex_unlock(&wpa_embox_queue_mtx);
		wpa_printf(MSG_ERROR,
		    "wpa_embox: event queue full, dropping event %d",
		    event);
		wpa_embox_event_copy_free(event, copy);
		os_free(copy);
		os_free(msg);
		return -1;
	}
	wpa_embox_queue[wpa_embox_queue_tail] = msg;
	wpa_embox_queue_tail =
	    (wpa_embox_queue_tail + 1) % WPA_EMBOX_QUEUE_LEN;
	mutex_unlock(&wpa_embox_queue_mtx);

	semaphore_leave(&wpa_embox_queue_sem);
	return 0;
}

int wpa_embox_send_dummy_event(void) {
	return wpa_embox_send_event(NULL, -1, NULL);
}

void wpa_embox_process_events(void) {

	for (;;) {
		struct wpa_supplicant_event_msg *msg;

		mutex_lock(&wpa_embox_queue_mtx);
		if (wpa_embox_queue_head == wpa_embox_queue_tail) {
			mutex_unlock(&wpa_embox_queue_mtx);
			return;
		}
		msg = wpa_embox_queue[wpa_embox_queue_head];
		wpa_embox_queue_head =
		    (wpa_embox_queue_head + 1) % WPA_EMBOX_QUEUE_LEN;
		mutex_unlock(&wpa_embox_queue_mtx);

		if (msg->event >= 0 && msg->ctx != NULL) {
			wpa_supplicant_event(msg->ctx, msg->event,
			    msg->data);
		}
		wpa_embox_event_copy_free(msg->event, msg->data);
		os_free(msg->data);
		os_free(msg);
	}
}

/* ------------------------------------------------------------------ */
/* marshalled control jobs */

static void wpa_embox_job_handler(void *eloop_ctx, void *timeout_ctx) {
	struct wpa_embox_job *job = timeout_ctx;

	(void) eloop_ctx;

	job->fn(job->arg);

	mutex_lock(&wpa_embox_job_mtx);
	job->done = 1;
	wpa_embox_job = NULL;
	mutex_unlock(&wpa_embox_job_mtx);
	semaphore_leave(&wpa_embox_job_done);
}

static int wpa_embox_job_run(void (*fn)(void *), void *arg,
    unsigned timeout_ms) {
	struct wpa_embox_job job;

	job.fn = fn;
	job.arg = arg;
	job.done = 0;

	mutex_lock(&wpa_embox_job_mtx);
	if (wpa_embox_job != NULL) {
		mutex_unlock(&wpa_embox_job_mtx);
		return -EBUSY;
	}
	wpa_embox_job = &job;
	mutex_unlock(&wpa_embox_job_mtx);

	eloop_register_timeout(0, 0, wpa_embox_job_handler, NULL, &job);

	/* wait on the supplicant thread finishing the job */
	{
		struct timespec deadline;

		clock_gettime(CLOCK_REALTIME, &deadline);
		deadline.tv_sec += timeout_ms / 1000;
		deadline.tv_nsec += (long) (timeout_ms % 1000) *
		    NSEC_PER_MSEC;
		if (deadline.tv_nsec >= (long) NSEC_PER_SEC) {
			deadline.tv_sec++;
			deadline.tv_nsec -= NSEC_PER_SEC;
		}
		semaphore_timedwait(&wpa_embox_job_done, &deadline);
	}

	mutex_lock(&wpa_embox_job_mtx);
	if (!job.done) {
		/* timed out: leave it registered, eloop will still run
		 * it and the leave() will target a stale waiter count */
		wpa_embox_job = NULL;
		mutex_unlock(&wpa_embox_job_mtx);
		return -EAGAIN;
	}
	mutex_unlock(&wpa_embox_job_mtx);
	return 0;
}

/* ------------------------------------------------------------------ */
/* control jobs (run on the eloop thread) */

struct wpa_embox_connect_req {
	char ssid[33];
	char psk[65];
};

/* the job mutex serializes control requests, so a single slot is fine;
 * it must outlive wpa_embox_job_run() in case the caller times out
 * while the eloop thread is still about to run the job */
static struct wpa_embox_connect_req wpa_embox_connect_req;

static void wpa_embox_do_connect(void *arg) {
	struct wpa_embox_connect_req *req = arg;
	struct wpa_ssid *ssid;

	if (wpa_embox_wpa_s == NULL) {
		wpa_printf(MSG_ERROR, "wpa_embox: not started");
		return;
	}

	/* one active network: drop previous selections */
	wpa_supplicant_select_network(wpa_embox_wpa_s, NULL);

	ssid = wpa_supplicant_add_network(wpa_embox_wpa_s);
	if (ssid == NULL) {
		wpa_printf(MSG_ERROR, "wpa_embox: add_network failed");
		return;
	}
	ssid->ssid = (u8 *) os_strdup(req->ssid);
	if (ssid->ssid == NULL) {
		wpa_supplicant_remove_network(wpa_embox_wpa_s, ssid->id);
		return;
	}
	ssid->ssid_len = strlen(req->ssid);
	ssid->scan_ssid = 1;
	ssid->key_mgmt = WPA_KEY_MGMT_PSK;
	ssid->proto = WPA_PROTO_RSN | WPA_PROTO_WPA;
	ssid->pairwise_cipher = WPA_CIPHER_CCMP | WPA_CIPHER_TKIP;
	ssid->group_cipher = WPA_CIPHER_CCMP | WPA_CIPHER_TKIP;
	ssid->passphrase = os_strdup(req->psk);
	if (ssid->passphrase == NULL) {
		wpa_printf(MSG_ERROR, "wpa_embox: psk alloc failed");
		wpa_supplicant_remove_network(wpa_embox_wpa_s, ssid->id);
		return;
	}
	wpa_config_update_psk(ssid);
	ssid->disabled = 0;

	wpa_config_update_prio_list(wpa_embox_wpa_s->conf);
	wpa_supplicant_select_network(wpa_embox_wpa_s, ssid);
	wpa_printf(MSG_INFO, "wpa_embox: connecting to \"%s\"",
	    req->ssid);
}

static void wpa_embox_do_disconnect(void *arg) {
	(void) arg;

	if (wpa_embox_wpa_s == NULL) {
		return;
	}
	wpa_supplicant_select_network(wpa_embox_wpa_s, NULL);
	wpa_printf(MSG_INFO, "wpa_embox: disconnected");
}

static void wpa_embox_do_status(void *arg) {
	struct wpa_supplicant *wpa_s = wpa_embox_wpa_s;
	const char *state_name;

	(void) arg;

	if (wpa_s == NULL) {
		printf("wpa: supplicant not started\n");
		return;
	}
	state_name = wpa_supplicant_state_txt(wpa_s->wpa_state);
	printf("wpa: state=%s", state_name);
	if (wpa_s->current_bss != NULL && wpa_s->current_ssid != NULL) {
		printf(" ssid=\"%.*s\" bssid=" MACSTR " freq=%d",
		    wpa_s->current_ssid->ssid_len,
		    (const char *) wpa_s->current_ssid->ssid,
		    MAC2STR(wpa_s->current_bss->bssid),
		    wpa_s->current_bss->freq);
	}
	printf("\n");
}

/* ------------------------------------------------------------------ */
/* public API (any thread) */

int wpa_embox_connect(const char *ssid, const char *psk) {

	if (wpa_embox_wpa_s == NULL || ssid == NULL || psk == NULL ||
	    strlen(ssid) >= sizeof(wpa_embox_connect_req.ssid) ||
	    (strlen(psk) != 0 && strlen(psk) != 64 &&
		strlen(psk) < 8)) {
		return -EINVAL;
	}
	os_strlcpy(wpa_embox_connect_req.ssid, ssid,
	    sizeof(wpa_embox_connect_req.ssid));
	os_strlcpy(wpa_embox_connect_req.psk, psk,
	    sizeof(wpa_embox_connect_req.psk));

	return wpa_embox_job_run(wpa_embox_do_connect,
	    &wpa_embox_connect_req, 5000);
}

int wpa_embox_disconnect(void) {
	return wpa_embox_job_run(wpa_embox_do_disconnect, NULL, 5000);
}

int wpa_embox_status(void) {
	return wpa_embox_job_run(wpa_embox_do_status, NULL, 5000);
}



/* ---- hostap references not pulled by the PSK-only build set ---- */

struct l2_packet_data *l2_packet_init_bridge(const char *br_ifname,
    const char *ifname, const u8 *own_addr, unsigned short protocol,
    void (*rx_callback)(void *ctx, const u8 *src_addr, const u8 *buf,
	size_t len),
    void *rx_callback_ctx, int l2_hdr)
{
	(void) br_ifname;
	return l2_packet_init(ifname, own_addr, protocol, rx_callback,
	    rx_callback_ctx, l2_hdr);
}

void wmm_ac_notify_assoc(struct wpa_supplicant *wpa_s, const u8 *ies,
    size_t ies_len, const struct wmm_params *wmm_params)
{
	(void) wpa_s;
	(void) ies;
	(void) ies_len;
	(void) wmm_params;
}

void wmm_ac_notify_disassoc(struct wpa_supplicant *wpa_s)
{
	(void) wpa_s;
}

void wmm_ac_clear_saved_tspecs(struct wpa_supplicant *wpa_s)
{
	(void) wpa_s;
}

int wmm_ac_restore_tspecs(struct wpa_supplicant *wpa_s)
{
	(void) wpa_s;
	return 0;
}

void wmm_ac_save_tspecs(struct wpa_supplicant *wpa_s)
{
	(void) wpa_s;
}

void wmm_ac_rx_action(struct wpa_supplicant *wpa_s, const u8 *da,
    const u8 *sa, const u8 *data, size_t len)
{
	(void) wpa_s;
	(void) da;
	(void) sa;
	(void) data;
	(void) len;
}

int wpa_embox_started(void) {
	return wpa_embox_wpa_s != NULL;
}

/* ------------------------------------------------------------------ */
/* supplicant thread */

static void *wpa_embox_supplicant_thread(void *arg) {
	struct wpa_params params;
	struct wpa_interface iface;

	(void) arg;

	memset(&params, 0, sizeof(params));
	params.wpa_debug_level = CONFIG_WPA_SUPP_DEBUG_LEVEL;

	wpa_embox_global = wpa_supplicant_init(&params);
	if (wpa_embox_global == NULL) {
		wpa_printf(MSG_ERROR, "wpa_embox: init failed");
		return NULL;
	}

	memset(&iface, 0, sizeof(iface));
	iface.ifname = WPA_EMBOX_IFNAME;
	iface.driver = "embox";

	wpa_embox_wpa_s = wpa_supplicant_add_iface(wpa_embox_global,
	    &iface, NULL);
	if (wpa_embox_wpa_s == NULL) {
		wpa_printf(MSG_ERROR, "wpa_embox: add_iface failed");
		wpa_supplicant_deinit(wpa_embox_global);
		wpa_embox_global = NULL;
		return NULL;
	}
	wpa_embox_ready = 1;

	wpa_printf(MSG_INFO, "wpa_embox: supplicant running");
	wpa_supplicant_run(wpa_embox_global);

	wpa_supplicant_remove_iface(wpa_embox_global, wpa_embox_wpa_s, 0);
	wpa_supplicant_deinit(wpa_embox_global);
	wpa_embox_wpa_s = NULL;
	wpa_embox_global = NULL;
	return NULL;
}

int wpa_embox_start(void) {
	void *thr;

	int spin = 0;

	if (wpa_embox_global != NULL) {
		return 0;
	}
	wpa_embox_ready = 0;

	thr = thread_create(THREAD_FLAG_DETACHED,
	    wpa_embox_supplicant_thread, NULL);
	if (thr == NULL) {
		return -1;
	}
	thread_launch(thr);

	/* poll until add_iface finished (plain flag, no locks) */
	while (!wpa_embox_ready && spin < 3000000) {
		spin++;
	}
	if (!wpa_embox_ready) {
		return -1;
	}
	return 0;
}
