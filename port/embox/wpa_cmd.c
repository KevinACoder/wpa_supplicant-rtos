/*
 * @file
 * @brief wpa command: drive the supplicant from the shell.
 *
 * The PSK is only ever a command line argument, nothing is stored.
 *
 * @date 08.09.2026
 * @author zhugengyu
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "wpa_embox_api.h"

static void usage(const char *argv0) {
	printf("usage: %s start | status | connect <ssid> <psk> [bssid] | "
	       "disconnect\n", 
	    argv0);
}

int main(int argc, char **argv) {
	int ret;

	if (argc < 2) {
		usage(argv[0]);
		return 0;
	}

	if (strcmp(argv[1], "start") == 0) {
		ret = wpa_embox_start();
		printf("wpa: start %s\n",
		    ret == 0 ? "ok" : "failed");
		return 0;
	}
	if (strcmp(argv[1], "status") == 0) {
		ret = wpa_embox_status();
		if (ret == -EAGAIN) {
			printf("wpa: supplicant busy\n");
		}
		return 0;
	}
	if (strcmp(argv[1], "connect") == 0 && (argc == 4 || argc == 5)) {
		if (!wpa_embox_started()) {
			/* interface up happens on this (shell) thread: urtwn_init
			 * blocks on the USB workers and must not run on the
			 * supplicant thread */
			wlan_embox_ensure_up();
			wpa_embox_start();
		}
		if (argc == 5) {
			unsigned b[6];

			if (sscanf(argv[4],
			    "%x:%x:%x:%x:%x:%x",
			    &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) == 6) {
				unsigned char mac[6];

				for (int i = 0; i < 6; i++) {
					mac[i] = (unsigned char) b[i];
				}
				ret = wpa_embox_connect_bssid(argv[2],
				    argv[3], mac);
			} else {
				printf("wpa: bad bssid %s\n", argv[4]);
				return 1;
			}
		} else {
			ret = wpa_embox_connect(argv[2], argv[3]);
		}
		if (ret == 0) {
			printf("wpa: connecting to \"%s\"\n", argv[2]);
		} else {
			printf("wpa: connect failed (%d)\n", ret);
		}
		return 0;
	}
	if (strcmp(argv[1], "disconnect") == 0) {
		ret = wpa_embox_disconnect();
		printf("wpa: disconnect %s\n",
		    ret == 0 ? "ok" : "failed/busy");
		return 0;
	}

	usage(argv[0]);
	return 0;
}
