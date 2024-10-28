/*
 * Copyright (c) 2022 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 */

#ifndef ZEPHYR_SUPP_MGMT_H
#define ZEPHYR_SUPP_MGMT_H

#include <zephyr/net/wifi_mgmt.h>

#define MAX_SSID_LEN 32
#define MAC_ADDR_LEN 6

enum requested_ops
{
    ADD = 0,
    REMOVE,
    CONNECT,
    DISCONNECT,
    RECONNECT,
    AUTOCONNECT,
    ROAM,
    FT_DS,
#ifdef CONFIG_WPA_SUPP_WPS
    WPS_PBC,
    WPS_PIN,
    WPS_CANCEL,
#endif
    START,
    STOP
};

int zephyr_supp_init(void (*msg_cb)(const char *txt, size_t len));

// int zephyr_supp_status(const struct device *dev);

int zephyr_supp_scan(const struct device *dev, wlan_scan_params_v2_t *params);

int zephyr_supp_add_network(const struct device *dev, struct wlan_network *network);
/**
 * @brief Request a connection
 *
 * @param iface_name: Wi-Fi interface name to use
 * @param params: Connection details
 *
 * @return: 0 for OK; -1 for ERROR
 */
int zephyr_supp_connect(const struct device *dev, struct wlan_network *network);
/**
 * @brief Forces station to disconnect and stops any subsequent scan
 *  or connection attempts
 *
 * @param iface_name: Wi-Fi interface name to use
 *
 * @return: 0 for OK; -1 for ERROR
 */
int zephyr_supp_disconnect(const struct device *dev);

int zephyr_supp_remove_network(const struct device *dev, struct wlan_network *network);

int zephyr_supp_reassociate(const struct device *dev);

int zephyr_supp_autoconnect(const struct device *dev, int enable);

/**
 * @brief
 *
 * @param iface_name: Wi-Fi interface name to use
 * @param wifi_iface_status: Status structure to fill
 *
 * @return: 0 for OK; -1 for ERROR
 */
int zephyr_supp_network_status(const struct device *dev, struct wlan_network *network);

int zephyr_supp_send_neighbor_rep(const struct device *dev, char *pssid, int lci, int civic);

int zephyr_supp_send_btm_query(const struct device *dev, int query_reason);

int zephyr_supp_roam(const struct device *dev, unsigned char *bssid);

int zephyr_supp_ft_ds(const struct device *dev, unsigned char *bssid);

int zephyr_supp_get_sta_info(const struct device *dev, unsigned char *sta_addr, unsigned char *is_11n_enabled);

#ifdef CONFIG_WPA_SUPP_WPS
int zephyr_supp_start_wps_pbc(const struct device *dev, int is_ap);
int zephyr_supp_start_wps_pin(const struct device *dev, const char *pin, int is_ap);
int zephyr_supp_wps_pin_valid(const struct device *dev, const unsigned char *pin);
int zephyr_supp_wps_generate_pin(const struct device *dev, unsigned int *pin);
int zephyr_supp_cancel_wps(const struct device *dev, int is_ap);
#endif

int zephyr_supp_start_ap(const struct device *dev, struct wlan_network *network);

void zephyr_supp_set_ap_bw(const struct device *dev, unsigned char bw);

int zephyr_supp_set_ap_country(const struct device *dev, const char *country, const unsigned char country3);

int zephyr_supp_stop_ap(const struct device *dev);

int zephyr_supp_req_status(enum requested_ops ops);

int zephyr_supp_deinit(void);

#if 0
/**
 * @brief Request a connection
 *
 * @param iface_name: Wi-Fi interface name to use
 * @param params: Connection details
 *
 * @return: 0 for OK; -1 for ERROR
 */
int zephyr_supp_connect(const struct device *dev, struct wifi_connect_req_params *params);
/**
 * @brief Forces station to disconnect and stops any subsequent scan
 *  or connection attempts
 *
 * @param iface_name: Wi-Fi interface name to use
 *
 * @return: 0 for OK; -1 for ERROR
 */
int zephyr_supp_disconnect(const struct device *dev);
#endif

/**
 * @brief
 *
 * @param iface_name: Wi-Fi interface name to use
 * @param wifi_iface_status: Status structure to fill
 *
 * @return: 0 for OK; -1 for ERROR
 */
int zephyr_supp_status(const struct device *dev, struct wifi_iface_status *status);

#endif /* ZEPHYR_SUPP_MGMT_H */
