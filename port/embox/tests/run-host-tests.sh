#!/bin/sh
set -eu
cd "$(dirname "$0")/../../.."
test_build=$(mktemp -d)
trap 'rm -rf "$test_build"' EXIT HUP INT TERM
${CC:-cc} -std=gnu11 -g -fsanitize=address,undefined \
    -include port/embox/wpa_embox_config.h \
    -Iport/embox/tests/include -Isrc -Isrc/utils \
    port/embox/tests/eloop_test.c port/embox/eloop_embox.c \
    -o "$test_build/eloop_test"
"$test_build/eloop_test"
echo "eloop idle dispatch and timer fairness: PASS"
sh port/embox/tests/run-netdev-test.sh
