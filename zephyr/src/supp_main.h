/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef __SUPP_MAIN_H_
#define __SUPP_MAIN_H_

struct wpa_supplicant_event_msg
{
    /* Dummy messages to unblock select */
    int hostapd;
    bool ignore_msg;
    void *ctx;
    unsigned int event;
    void *data;
};
int send_wpa_supplicant_event(const struct wpa_supplicant_event_msg *msg);
int start_wpa_supplicant(char *iface_name);
int stop_wpa_supplicant(void);

int start_hostapd(char *iface_name);
int send_hostapd_event(const struct wpa_supplicant_event_msg *msg);
struct hostapd_iface *hostapd_get_interface(const char *ifname);
#endif /* __SUPP_MAIN_H_ */
