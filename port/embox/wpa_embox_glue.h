/*
 * @file
 * @brief Internal glue between the embox port pieces: the eloop event
 * queue lives in supp_main_embox.c and the eloop only wakes on it.
 *
 * @date 08.09.2026
 * @author zhugengyu
 */

#ifndef WPA_EMBOX_GLUE_H_
#define WPA_EMBOX_GLUE_H_

/* one queued driver event; the producer copies the pointer payloads
 * (supp_main_embox.c), the eloop thread hands it to
 * wpa_supplicant_event() and frees the copy */
struct wpa_supplicant_event_msg {
	void *ctx;
	int event; /* enum wpa_event_type; -1 = wake-up only */
	void *data;
};

/* driver events are queued for the eloop thread */
int wpa_embox_send_event(void *ctx, int event, const void *data);
int wpa_embox_send_dummy_event(void);
void wpa_embox_process_events(void);

/* the queue wakeup used by eloop_register_timeout */
void wpa_embox_wake_loop(void);

#endif /* WPA_EMBOX_GLUE_H_ */
