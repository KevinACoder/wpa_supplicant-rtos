#!/bin/sh
set -eu
cd "$(dirname "$0")/../../.."
embox_src=${EMBOX_SRC:-../src}
test_build=$(mktemp -d)
trap 'rm -rf "$test_build"' EXIT HUP INT TERM
# Only the adjacent network services are doubles. sk_buff is the real
# Embox public type and netdev_bridge.c is compiled without alteration.
for header in net/l2/ethernet.h net/l3/arp.h net/netdevice.h net/inetdevice.h net/l0/net_entry.h port/port.h; do
    mkdir -p "$test_build/include/$(dirname "$header")"
    touch "$test_build/include/$header"
done
${CC:-cc} -std=gnu11 -g -Wall -Wextra -fsanitize=address,undefined \
    -I"$test_build/include" -idirafter "$embox_src/src/include" \
    port/embox/tests/netdev_test.c -o "$test_build/netdev_test"
"$test_build/netdev_test"
