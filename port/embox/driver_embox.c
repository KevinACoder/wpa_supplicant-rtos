/*
 * @file
 * @brief wpa_supplicant driver wrapper for the net80211 port.
 *
 * Modeled on src/drivers/driver_bsd.c, but instead of ioctls and the
 * routing socket it calls the net80211 entry points of the port
 * directly and raises events through the eloop event queue. The
 * net80211 state machine owns the MLME: associate() programs the
 * desired SSID/BSSID/RSN parameters and joins the BSS, auth/assoc
 * frames are built by net80211, and state transitions come back
 * through the wrapped ic_newstate hook.
 *
 * @date 08.09.2026
 * @author zhugengyu
 */

/* The net80211/BSD world must come first: the compat shadow types
 * (kmutex_t, callout_t, ...) have to exist before the driver headers
 * use them, exactly like urtwn_reg.c orders its includes. */
#include <sys/queue.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/rndsource.h>
#include <sys/mbuf.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>
#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_proto.h>
#include <net80211/ieee80211_node.h>
#include <net80211/ieee80211_crypto.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <driver/urtwn/rtwnreg.h>
#include <driver/urtwn/if_urtwnreg.h>
#include <driver/urtwn/if_urtwnvar.h>
#include <port/port.h>

/* Then the hostap world; WPA_OUI_TYPE is defined differently on the
 * two sides and this unit needs neither. */
#undef WPA_OUI_TYPE
#undef free
#undef malloc
#undef realloc
#include <includes.h>
#include "utils/common.h"
#include "utils/eloop.h"
#include "common/ieee802_11_defs.h"
#include "drivers/driver.h"

#include "wpa_embox_glue.h"
#include "wpa_embox_api.h"

struct embox_drv_data {
	void *ctx; /* wpa_s */
	struct urtwn_softc *sc;
	struct wpa_driver_capa capa;
	int associated;
	int if_up;
};

/* single adapter: the net80211 ic_newstate hook has no priv slot */
static struct embox_drv_data g_drv;
static int (*g_prev_newstate)(struct ieee80211com *,
    enum ieee80211_state, int);

/* ------------------------------------------------------------------ */
/* net80211 wiring */

extern struct urtwn_softc *urtwn_reg_softc;
extern void wlan_urtwn_up(void);

/* bring the interface up on the calling (shell) thread; urtwn_init can
 * block on the USB workers, which must not run on the supplicant thread */
int wlan_embox_ensure_up(void)
{
	if (urtwn_reg_softc == NULL) {
		return -1;
	}
	wlan_urtwn_up();
	return 0;
}

/*
 * Runs in the urtwn taskq thread as part of the newstate chain.
 * nstate is the state the protocol layer is migrating to: S_RUN means
 * the AP accepted our association, S_AUTH after having been
 * associated means the AP sent deauth/disassoc.
 */
static int embox_newstate(struct ieee80211com *ic,
    enum ieee80211_state nstate, int arg) {
	enum ieee80211_state ostate = ic->ic_state;
	int ret;

	ret = g_prev_newstate(ic, nstate, arg);

	if (g_drv.ctx == NULL) {
		return ret;
	}
	if (nstate == IEEE80211_S_RUN && ostate != IEEE80211_S_RUN) {
		g_drv.associated = 1;
		/* bsd raises EVENT_ASSOC with no extra data; the
		 * supplicant picks bssid/ssid through get_bssid */
		wpa_embox_send_event(g_drv.ctx, EVENT_ASSOC, NULL);
	} else if ((nstate == IEEE80211_S_AUTH ||
		       nstate == IEEE80211_S_INIT) &&
		   (ostate == IEEE80211_S_RUN ||
		       ostate == IEEE80211_S_ASSOC)) {
		union wpa_event_data data;
		u8 bssid[ETH_ALEN];

		g_drv.associated = 0;
		memset(&data, 0, sizeof(data));
		os_memcpy(bssid,
		    ic->ic_bss != NULL ? ic->ic_bss->ni_bssid :
					 (const u8 *) etherbroadcastaddr,
		    ETH_ALEN);
		data.deauth_info.addr = bssid;
		data.deauth_info.reason_code = (u16) arg;
		wpa_embox_send_event(g_drv.ctx,
		    nstate == IEEE80211_S_INIT ? EVENT_DISASSOC :
						 EVENT_DEAUTH,
		    &data);
	}
	return ret;
}

/*
 * The supplicant scans with ap_scan=1: scan2() starts the net80211
 * scan and this poller reports completion (F_SCAN cleared), whether it
 * ended with candidates or not.
 */
static void embox_scan_poll(void *eloop_ctx, void *timeout_ctx) {
	struct ieee80211com *ic = &g_drv.sc->sc_ic;
	static int polls;

	(void) eloop_ctx;
	(void) timeout_ctx;

	if ((ic->ic_flags & IEEE80211_F_SCAN) == 0) {
		wpa_printf(MSG_DEBUG, "embox: scan done");
		wpa_embox_send_event(g_drv.ctx, EVENT_SCAN_RESULTS, NULL);
		return;
	}
	if (++polls > 150) { /* ~30 s cap */
		polls = 0;
		wpa_printf(MSG_DEBUG, "embox: scan poll timeout");
		wpa_embox_send_event(g_drv.ctx, EVENT_SCAN_RESULTS, NULL);
		return;
	}
	eloop_register_timeout(0, 200000, embox_scan_poll, NULL, NULL);
}

/* ------------------------------------------------------------------ */
/* EAPOL delivery (usb worker context, copies only) */

static void embox_eapol_rx(const uint8_t src[6], const uint8_t *buf,
    size_t len, void *arg) {
	struct embox_drv_data *drv = arg;
	union wpa_event_data data;

	if (drv->ctx == NULL) {
		return;
	}
	memset(&data, 0, sizeof(data));
	data.eapol_rx.src = src;
	data.eapol_rx.data = buf;
	data.eapol_rx.data_len = len;
	wpa_embox_send_event(drv->ctx, EVENT_EAPOL_RX, &data);
}

/* ------------------------------------------------------------------ */
/* ops */

static void *embox_global_init(void *ctx) {
	(void) ctx;
	/* the single adapter is the global state */
	return &g_drv;
}

static void embox_global_deinit(void *priv) {
	(void) priv;
}

static void *embox_init2(void *ctx, const char *ifname, void *global_priv) {
	struct ieee80211com *ic;

	(void) ifname;
	(void) global_priv;

	memset(&g_drv, 0, sizeof(g_drv));

	g_drv.sc = urtwn_reg_softc;
	if (g_drv.sc == NULL) {
		wpa_printf(MSG_ERROR, "embox: no urtwn device attached");
		return NULL;
	}
	g_drv.ctx = ctx;

	ic = &g_drv.sc->sc_ic;
	/* the supplicant owns BSS selection; set it before the interface
	 * comes up so the auto-scan in urtwn_init does not start */
	ic->ic_roaming = IEEE80211_ROAMING_MANUAL;
	g_prev_newstate = ic->ic_newstate;
	ic->ic_newstate = embox_newstate;

	wlan_port_set_eapol_rx(embox_eapol_rx, &g_drv);
	wlan_netdev_ensure();

	/* one-shot bring-up: firmware load + power on */
	wlan_urtwn_up();
	g_drv.if_up = 1;

	g_drv.capa.key_mgmt = WPA_KEY_MGMT_PSK;
	g_drv.capa.enc = WPA_DRIVER_CAPA_ENC_CCMP |
			 WPA_DRIVER_CAPA_ENC_TKIP;
	g_drv.capa.auth = WPA_DRIVER_AUTH_OPEN | WPA_DRIVER_AUTH_SHARED;
	g_drv.capa.max_scan_ssids = 1;

	return &g_drv;
}

static void embox_deinit(void *priv) {
	struct embox_drv_data *drv = priv;
	struct ieee80211com *ic;

	if (drv->sc != NULL) {
		ic = &drv->sc->sc_ic;
		if (ic->ic_newstate == embox_newstate) {
			ic->ic_newstate = g_prev_newstate;
		}
	}
	wlan_port_set_eapol_rx(NULL, NULL);
	memset(&g_drv, 0, sizeof(g_drv));
}

static int embox_get_bssid(void *priv, u8 *bssid) {
	struct embox_drv_data *drv = priv;
	struct ieee80211com *ic = &drv->sc->sc_ic;

	if (!drv->associated) {
		return -1;
	}
	os_memcpy(bssid, ic->ic_bss->ni_bssid, ETH_ALEN);
	return 0;
}

static int embox_get_ssid(void *priv, u8 *ssid) {
	struct embox_drv_data *drv = priv;
	struct ieee80211com *ic = &drv->sc->sc_ic;

	if (!drv->associated) {
		return -1;
	}
	os_memcpy(ssid, ic->ic_bss->ni_essid, ic->ic_bss->ni_esslen);
	return (int) ic->ic_bss->ni_esslen;
}

static int embox_set_countermeasures(void *priv, int enabled) {
	(void) priv;
	(void) enabled;
	return 0;
}

static int embox_scan2(void *priv, struct wpa_driver_scan_params *params) {
	struct embox_drv_data *drv = priv;
	struct ieee80211com *ic = &drv->sc->sc_ic;

	if (params->num_ssids > 0 && params->ssids[0].ssid_len > 0) {
		/* directed probe for the requested SSID */
		memcpy(ic->ic_des_essid, params->ssids[0].ssid,
		    params->ssids[0].ssid_len);
		ic->ic_des_esslen = params->ssids[0].ssid_len;
	} else {
		ic->ic_des_esslen = 0;
	}

	wpa_printf(MSG_DEBUG, "embox: scan requested");
	ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
	eloop_register_timeout(0, 200000, embox_scan_poll, NULL, NULL);
	return 0;
}

/*
 * One scan result: SSID IE + supported rates IE + the RSN/WPA IE the
 * stack kept from the beacon, laid out after the wpa_scan_res header.
 */
static struct wpa_scan_res *embox_scan_entry(struct ieee80211_node *ni) {
	struct wpa_scan_res *r;
	struct ieee80211_rateset *rs = &ni->ni_rates;
	size_t ssid_ie_len, rates_ie_len, wpa_ie_len, ie_len, len;
	u8 *ie;
	int i;

	ssid_ie_len = 2 + ni->ni_esslen;
	rates_ie_len = rs->rs_nrates > 0 ? 2 + rs->rs_nrates : 0;
	wpa_ie_len = ni->ni_wpa_ie != NULL ? 2 + ni->ni_wpa_ie[1] : 0;
	ie_len = ssid_ie_len + rates_ie_len + wpa_ie_len;

	len = sizeof(struct wpa_scan_res) + ie_len;
	r = os_zalloc(len);
	if (r == NULL) {
		return NULL;
	}
	os_memcpy(r->bssid, ni->ni_bssid, ETH_ALEN);
	r->freq = ni->ni_chan != NULL ? ni->ni_chan->ic_freq : 0;
	r->beacon_int = ni->ni_intval;
	r->caps = ni->ni_capinfo;
	r->level = ni->ni_rssi;
	r->ie_len = ie_len;
	r->beacon_ie_len = 0;

	ie = (u8 *) (r + 1);
	if (ni->ni_esslen > 0) {
		*ie++ = WLAN_EID_SSID;
		*ie++ = (u8) ni->ni_esslen;
		os_memcpy(ie, ni->ni_essid, ni->ni_esslen);
		ie += ni->ni_esslen;
	}
	if (rates_ie_len > 0) {
		*ie++ = WLAN_EID_SUPP_RATES;
		*ie++ = (u8) rs->rs_nrates;
		for (i = 0; i < rs->rs_nrates; i++) {
			*ie++ = rs->rs_rates[i];
		}
	}
	if (wpa_ie_len > 0) {
		*ie++ = ni->ni_wpa_ie[0];
		*ie++ = ni->ni_wpa_ie[1];
		os_memcpy(ie, ni->ni_wpa_ie + 2, ni->ni_wpa_ie[1]);
		ie += ni->ni_wpa_ie[1];
	}
	return r;
}

struct embox_scan_collect {
	struct wpa_scan_results *res;
	int overflow;
};

/* ieee80211_iter_func signature: (void *arg, struct ieee80211_node *) */
static void embox_collect_cb(void *arg, struct ieee80211_node *ni) {
	struct embox_scan_collect *collect = arg;
	struct wpa_scan_res *r;

	if (collect->res->num >= 64) {
		collect->overflow = 1;
		return;
	}
	r = embox_scan_entry(ni);
	if (r == NULL) {
		return;
	}
	collect->res->res[collect->res->num++] = r;
}

static struct wpa_scan_results *embox_get_scan_results2(void *priv) {
	struct embox_drv_data *drv = priv;
	struct ieee80211com *ic = &drv->sc->sc_ic;
	struct wpa_scan_results *res;
	struct embox_scan_collect collect;

	res = os_zalloc(sizeof(*res));
	if (res == NULL) {
		return NULL;
	}
	res->res = os_calloc(64, sizeof(struct wpa_scan_res *));
	if (res->res == NULL) {
		os_free(res);
		return NULL;
	}

	collect.res = res;
	collect.overflow = 0;
	ieee80211_iterate_nodes(&ic->ic_scan, embox_collect_cb, &collect);
	if (collect.overflow) {
		wpa_printf(MSG_INFO, "embox: scan table truncated");
	}
	return res;
}

static int embox_deauthenticate(void *priv, const u8 *addr,
    u16 reason_code) {
	struct embox_drv_data *drv = priv;
	struct ieee80211com *ic = &drv->sc->sc_ic;

	(void) addr;
	ieee80211_new_state(ic, IEEE80211_S_INIT, reason_code);
	return 0;
}

static int embox_associate(void *priv,
    struct wpa_driver_associate_params *params) {
	struct embox_drv_data *drv = priv;
	struct ieee80211com *ic = &drv->sc->sc_ic;
	struct ieee80211_node *ni;
	u8 *opt_ie;

	if (params->ssid == NULL || params->ssid_len == 0 ||
	    params->ssid_len > sizeof(ic->ic_des_essid)) {
		return -1;
	}

	/* desired SSID (and BSSID when the supplicant picked one) */
	memset(ic->ic_des_essid, 0, sizeof(ic->ic_des_essid));
	memcpy(ic->ic_des_essid, params->ssid, params->ssid_len);
	ic->ic_des_esslen = params->ssid_len;
	ic->ic_flags &= ~IEEE80211_F_DESBSSID;
	if (params->bssid != NULL) {
		IEEE80211_ADDR_COPY(ic->ic_des_bssid, params->bssid);
		ic->ic_flags |= IEEE80211_F_DESBSSID;
	}

	/* privacy/WPA mode flags, the ioctl equivalents */
	ic->ic_flags &= ~(IEEE80211_F_WPA1 | IEEE80211_F_WPA2);
	if (params->wpa_ie != NULL && params->wpa_ie_len > 0) {
		ic->ic_flags |= IEEE80211_F_WPA |
		    (params->wpa_ie[0] == WLAN_EID_RSN ? IEEE80211_F_WPA2
						       : IEEE80211_F_WPA1);
	}
	if (params->pairwise_suite != WPA_CIPHER_NONE ||
	    params->group_suite != WPA_CIPHER_NONE) {
		ic->ic_flags |= IEEE80211_F_PRIVACY;
	} else {
		ic->ic_flags &= ~IEEE80211_F_PRIVACY;
	}
	if (params->drop_unencrypted) {
		ic->ic_flags |= IEEE80211_F_DROPUNENC;
	} else {
		ic->ic_flags &= ~IEEE80211_F_DROPUNENC;
	}

	/* the RSN IE travels in ic_opt_ie: the assoc req builder appends
	 * it verbatim (the SIOCS80211 OPTIE path) */
	opt_ie = NULL;
	if (params->wpa_ie != NULL && params->wpa_ie_len > 0) {
		opt_ie = os_memdup(params->wpa_ie, params->wpa_ie_len);
		if (opt_ie == NULL) {
			return -1;
		}
	}
	if (ic->ic_opt_ie != NULL) {
		os_free(ic->ic_opt_ie);
	}
	ic->ic_opt_ie = opt_ie;
	ic->ic_opt_ie_len = opt_ie != NULL ? params->wpa_ie_len : 0;

	/* join the BSS the supplicant selected: net80211 refetches the
	 * node from the scan table and runs AUTH -> ASSOC -> RUN */
	ni = params->bssid != NULL ?
	    ieee80211_find_node(&ic->ic_scan, params->bssid) : NULL;
	if (ni == NULL) {
		wpa_printf(MSG_INFO,
		    "embox: bss not in scan table, rescanning");
		return -1;
	}
	/* sta_join consumes the reference */
	ieee80211_sta_join(ic, ni);
	return 0;
}

static int embox_set_key(void *priv, struct wpa_driver_set_key_params *p) {
	struct embox_drv_data *drv = priv;
	struct ieee80211com *ic = &drv->sc->sc_ic;
	struct ieee80211_key *wk;
	struct ieee80211_node *ni;
	int is_group;

	if (p->alg == WPA_ALG_NONE) {
		return 0; /* SW crypto: rekeying overwrites in place */
	}
	if (p->alg != WPA_ALG_CCMP || p->key == NULL ||
	    p->key_len != 16) {
		wpa_printf(MSG_INFO, "embox: unsupported key alg %d", p->alg);
		return -1;
	}
	is_group = p->addr != NULL &&
	    (p->addr[0] & 0x01) != 0; /* mcast/bcast bit: GTK */

	ieee80211_key_update_begin(ic);
	if (is_group) {
		wk = &ic->ic_nw_keys[p->key_idx];
	} else {
		ni = ieee80211_ref_node(ic->ic_bss);
		wk = &ni->ni_ucastkey;
	}

	if (ieee80211_crypto_newkey(ic, IEEE80211_CIPHER_AES_CCM,
		IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV, wk)) {
		wk->wk_keylen = (u_int) p->key_len;
		wk->wk_keyrsc = 0;
		wk->wk_keytsc = 0;
		memset(wk->wk_key, 0, sizeof(wk->wk_key));
		os_memcpy(wk->wk_key, p->key, p->key_len);
		if (p->seq != NULL && p->seq_len > 0 &&
		    p->seq_len <= sizeof(u64)) {
			u64 pn = 0;
			int i;

			for (i = p->seq_len - 1; i >= 0; i--) {
				pn = (pn << 8) | p->seq[i];
			}
			wk->wk_keyrsc = pn;
		}
		ieee80211_crypto_setkey(ic, wk,
		    is_group ? (u8 *) etherbroadcastaddr
			     : ic->ic_bss->ni_macaddr);
		if (is_group && p->set_tx) {
			ic->ic_def_txkey = p->key_idx;
		}
	} else {
		wpa_printf(MSG_ERROR, "embox: newkey failed");
	}
	ieee80211_key_update_end(ic);
	if (!is_group) {
		ieee80211_free_node(ni);
	}

	wpa_printf(MSG_DEBUG, "embox: %s key %d installed",
	    is_group ? "group" : "pairwise", p->key_idx);
	return 0;
}

static int embox_get_capa(void *priv, struct wpa_driver_capa *capa) {
	struct embox_drv_data *drv = priv;

	*capa = drv->capa;
	return 0;
}

const struct wpa_driver_ops wpa_driver_embox_ops = {
	.name = "embox",
	.desc = "net80211 port on Embox",
	.global_init = embox_global_init,
	.global_deinit = embox_global_deinit,
	.init2 = embox_init2,
	.deinit = embox_deinit,
	.get_bssid = embox_get_bssid,
	.get_ssid = embox_get_ssid,
	.set_countermeasures = embox_set_countermeasures,
	.scan2 = embox_scan2,
	.get_scan_results2 = embox_get_scan_results2,
	.deauthenticate = embox_deauthenticate,
	.associate = embox_associate,
	.get_capa = embox_get_capa,
	.set_key = embox_set_key,
};
