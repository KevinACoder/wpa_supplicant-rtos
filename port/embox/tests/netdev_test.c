/**
 * @file
 * @brief Verify Ethernet frame boundaries and skb ownership at the bridge.
 * @author zhugengyu
 * @date 09.09.2026
 */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <net/skbuff.h>

#define ETH_HEADER_SIZE  14
#define ETH_ALEN         6
#define ARP_HRD_ETHERNET 1
#define IFF_BROADCAST    2
#define IFF_RUNNING      64
struct net_driver {
	int (*xmit)(struct net_device *, struct sk_buff *);
};
struct net_device {
	unsigned mtu, hdr_len, addr_len, type, flags;
	uint8_t dev_addr[6];
	uint8_t broadcast[6];
	const struct net_driver *drv_ops;
	const void *ops;
	struct {
		unsigned tx_err, tx_packets, tx_bytes;
		unsigned rx_dropped, rx_packets, rx_bytes;
	} stats;
};
static const int ethernet_ops;
static int wlan_port_xmit(const uint8_t *, size_t);
static struct net_device *netdev_alloc(const char *,
    int (*)(struct net_device *), int);
static int inetdev_register_dev(struct net_device *);
static void netdev_free(struct net_device *);
static int wlan_port_get_hwaddr(uint8_t *);
static void wlan_port_set_data_rx(void (*)(const uint8_t *, size_t, void *),
    void *);
static void netif_rx(struct sk_buff *);

#include "../netdev_bridge.c"

/* A buffer owner and its frame deliberately have different addresses,
 * as in Embox skb_data.c. Use the real public sk_buff declaration. */
struct test_storage {
	unsigned owner[8];
	uint8_t bytes[1600];
};
static struct net_device test_dev;
static unsigned freed, sent;
static int send_result, allocation_failure;
static uint8_t expected[128];
static size_t expected_len;
static void (*rx_hook)(const uint8_t *, size_t, void *);
static void *rx_arg;

struct sk_buff *skb_alloc(size_t len) {
	struct sk_buff *skb;
	struct test_storage *storage;
	if (allocation_failure)
		return NULL;
	skb = calloc(1, sizeof(*skb));
	storage = calloc(1, sizeof(*storage));
	assert(skb && storage && len <= sizeof(storage->bytes));
	memset(storage->owner, 0xa5, sizeof(storage->owner));
	skb->data = (void *)storage;
	skb->mac.raw = storage->bytes;
	skb->len = len;
	return skb;
}
void skb_free(struct sk_buff *skb) {
	assert(skb);
	free(skb->data);
	free(skb);
	freed++;
}
static int wlan_port_xmit(const uint8_t *frame, size_t len) {
	assert(len == expected_len);
	assert(memcmp(frame, expected, len) == 0);
	sent++;
	return send_result;
}
static struct net_device *netdev_alloc(const char *name,
    int (*setup)(struct net_device *), int extra) {
	assert(strcmp(name, "wlan0") == 0 && extra == 0);
	assert(setup(&test_dev) == 0);
	return &test_dev;
}
static int inetdev_register_dev(struct net_device *dev) {
	assert(dev == &test_dev);
	return 0;
}
static void netdev_free(struct net_device *dev) {
	assert(dev == &test_dev);
}
static int wlan_port_get_hwaddr(uint8_t *addr) {
	memcpy(addr, expected + 6, 6);
	return 0;
}
static void wlan_port_set_data_rx(void (*fn)(const uint8_t *, size_t, void *),
    void *arg) {
	rx_hook = fn;
	rx_arg = arg;
}
static void netif_rx(struct sk_buff *skb) {
	assert(skb->dev == &test_dev && skb->len == expected_len);
	assert(memcmp(skb->mac.raw, expected, expected_len) == 0);
	skb_free(skb);
}
int main(void) {
	for (unsigned i = 0; i < sizeof(expected); i++)
		expected[i] = (i * 37 + 11) & 255;
	assert(wlan_netdev_ensure() == 0);
	assert(test_dev.mtu == 1500 && test_dev.hdr_len == 14);
	for (unsigned i = 0; i < ETH_ALEN; i++) {
		assert(test_dev.broadcast[i] == 0xff);
	}
	for (unsigned n = 0; n < 2; n++) {
		struct sk_buff *skb;
		expected_len = n ? sizeof(expected) : 42; /* ARP-sized and IP-sized frames */
		skb = skb_alloc(expected_len);
		memcpy(skb->mac.raw, expected, expected_len);
		send_result = n ? -1 : 0;
		assert(test_dev.drv_ops->xmit(&test_dev, skb) == (n ? -EIO : 0));
		assert(freed == n + 1 && sent == n + 1);
	}
	assert(test_dev.stats.tx_packets == 1 && test_dev.stats.tx_bytes == 42);
	assert(test_dev.stats.tx_err == 1);
	assert(test_dev.drv_ops->xmit(&test_dev, NULL) == -EINVAL);
	rx_hook(expected, expected_len, rx_arg);
	assert(freed == 3 && test_dev.stats.rx_packets == 1);
	allocation_failure = 1;
	rx_hook(expected, expected_len, rx_arg);
	assert(freed == 3 && test_dev.stats.rx_dropped == 1);
	puts("netdev frame boundaries and skb ownership: PASS");
	return 0;
}
