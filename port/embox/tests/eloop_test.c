/**
 * @file
 * @brief Exercise the actual event loop with a controllable clock.
 * @author zhugengyu
 * @date 09.09.2026
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "utils/common.h"
#include "utils/eloop.h"

static unsigned ticks, iterations, calls, batches;
static int mode;

void *os_zalloc(size_t len) { return calloc(1, len); }
int os_get_reltime(struct os_reltime *t) {
	t->sec = ticks / 1000;
	t->usec = (ticks % 1000) * 1000;
	return 0;
}
int ksleep(unsigned ms) {
	assert(ms <= 20);
	ticks += ms;
	assert(ticks < 10000);
	return 0;
}

static void timer(void *ctx, void *arg) {
	(void) ctx;
	(void) arg;
	calls++;
	if (mode == 1 && calls < 64) {
		assert(eloop_register_timeout(0, 0, timer, NULL, NULL) == 0);
	}
}

void wpa_embox_process_events(void) {
	batches++;
}
void wpa_embox_process_jobs(void) {
	iterations++;
	/* An idle daemon must still accept a subsequent shell request. */
	if (mode == 0 && iterations == 3) {
		assert(eloop_register_timeout(0, 50000, timer, NULL, NULL) == 0);
	}
	if ((mode == 0 && calls == 1) || (mode == 1 && calls == 64)) {
		eloop_terminate();
	}
}

int main(void) {
	struct os_reltime remaining;
	assert(eloop_init() == 0);
	assert(eloop_register_timeout(1, 0, timer, NULL, NULL) == 0);
	assert(eloop_cancel_timeout_one(timer, NULL, NULL, &remaining) == 1);
	assert(remaining.sec == 1 && remaining.usec == 0);
	eloop_run();
	assert(calls == 1 && ticks >= 110 && batches >= 3);
	eloop_destroy();

	mode = 1;
	calls = batches = iterations = 0;
	assert(eloop_init() == 0);
	assert(eloop_register_timeout(0, 0, timer, NULL, NULL) == 0);
	eloop_run();
	assert(calls == 64 && batches >= 4);
	eloop_destroy();
	return 0;
}
