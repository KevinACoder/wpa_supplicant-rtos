/*
 * @file
 * @brief Public control API of the wpa_supplicant port (any thread).
 *
 * @date 08.09.2026
 * @author zhugengyu
 */

#ifndef WPA_EMBOX_API_H_
#define WPA_EMBOX_API_H_

/* spawn the supplicant thread; idempotent */
int wpa_embox_start(void);
int wpa_embox_started(void);

/* WPA2-PSK: the request is marshalled to the supplicant thread and
 * this returns once it was picked up (or timed out). */
int wpa_embox_connect(const char *ssid, const char *psk);
/* same, with a locked BSSID (6 bytes); NULL bssid == unconstrained */
int wpa_embox_connect_bssid(const char *ssid, const char *psk,
	const unsigned char *bssid);
int wpa_embox_disconnect(void);
int wpa_embox_status(void);

/* registers the "wlan0" netdev with the embox network stack */
int wlan_netdev_ensure(void);
/* bring the wlan interface up on the calling thread */
int wlan_embox_ensure_up(void);

#endif /* WPA_EMBOX_API_H_ */
