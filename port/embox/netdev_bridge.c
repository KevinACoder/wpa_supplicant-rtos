/*
 * @file
 * @brief Present the net80211 interface to the embox network stack.
 *
 * One plain ethernet net_device ("wlan0"): TX goes out through the
 * port xmit hook (encrypted/encapsulated by net80211 on the way), RX
 * frames arrive from the if_percpuq_enqueue hook as complete ethernet
 * frames and are handed to netif_rx as skbs.
 *
 * @date 08.09.2026
 * @author zhugengyu
 */

#include <errno.h>
#include <string.h>

#include <net/l2/ethernet.h>
#include <net/l3/arp.h>
#include <net/netdevice.h>
#include <net/inetdevice.h>
#include <net/skbuff.h>
#include <net/l0/net_entry.h>

#include <port/port.h>
#include "wpa_embox_api.h"

static struct net_device *wlan_netdev;

static int wlan_netdev_xmit(struct net_device *dev, struct sk_buff *skb) {
	int ret;

	if (skb == NULL) {
		return -EINVAL;
	}
	ret = wlan_port_xmit((const uint8_t *) skb->data, skb->len);
	if (ret < 0) {
		dev->stats.tx_err++;
		skb_free(skb);
		return -EIO;
	}
	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;
	skb_free(skb);
	return 0;
}

static const struct net_driver wlan_netdev_ops = {
	.xmit = wlan_netdev_xmit,
};

/* runs in the USB worker context: copy, hand over, return */
static void wlan_netdev_data_rx(const uint8_t *frame, size_t len,
    void *arg) {
	struct net_device *dev = arg;
	struct sk_buff *skb;

	if (dev == NULL || len == 0) {
		return;
	}
	skb = skb_alloc(len);
	if (skb == NULL) {
		dev->stats.rx_dropped++;
		return;
	}
	memcpy(skb->mac.raw, frame, len);
	skb->dev = dev;
	netif_rx(skb);
	dev->stats.rx_packets++;
	dev->stats.rx_bytes += len;
}

static int wlan_netdev_setup(struct net_device *dev) {
	dev->mtu = 1500;
	dev->hdr_len = ETH_HEADER_SIZE;
	dev->addr_len = ETH_ALEN;
	dev->type = ARP_HRD_ETHERNET;
	dev->flags = IFF_BROADCAST | IFF_RUNNING;
	dev->drv_ops = &wlan_netdev_ops;
	dev->ops = &ethernet_ops;
	return 0;
}

/*
 * Called once the urtwn interface exists (from the driver init);
 * safe to call repeatedly. Also refreshes the MAC address, which is
 * only known after the firmware attach.
 */
int wlan_netdev_ensure(void) {
	uint8_t hwaddr[6];
	int ret;

	if (wlan_netdev != NULL) {
		if (wlan_port_get_hwaddr(hwaddr) == 0) {
			memcpy(wlan_netdev->dev_addr, hwaddr, ETH_ALEN);
		}
		return 0;
	}
	wlan_netdev = netdev_alloc("wlan0", &wlan_netdev_setup, 0);
	if (wlan_netdev == NULL) {
		return -ENOMEM;
	}
	if (wlan_port_get_hwaddr(hwaddr) == 0) {
		memcpy(wlan_netdev->dev_addr, hwaddr, ETH_ALEN);
	}
	ret = netdev_register(wlan_netdev);
	if (ret != 0) {
		wlan_netdev = NULL;
		return ret;
	}
	ret = inetdev_register_dev(wlan_netdev);
	if (ret != 0) {
		return ret;
	}
	wlan_port_set_data_rx(wlan_netdev_data_rx, wlan_netdev);
	return 0;
}
