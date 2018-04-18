/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <malloc.h>
#include <stdlib.h>
#include "event.h"
#include "load_conf.h"
#include "channels.h"
#include "fsutils.h"
#include "log_sys.h"

#define WDT_TIMEOUT 300

static struct event_t *last_e;
static int event_processing;

/**
 * Handle watchdog expire.
 *
 * @param signal Signal which triggered this function.
 */
static void wdt_timeout(int signal)
{
	struct event_t *e;
	struct crash_t *crash;
	struct info_t *info;
	int count;

	if (signal == SIGALRM) {
		LOGE("haven't received heart beat(%ds) for %ds, killing self\n",
		     HEART_BEAT, WDT_TIMEOUT);

		if (event_processing) {
			LOGE("event (%d, %s) processing...\n",
			     last_e->event_type, last_e->path);
			free(last_e);
		}

		count = events_count();
		LOGE("total %d unhandled events :\n", count);

		while (count-- && (e = event_dequeue())) {
			switch (e->event_type) {
			case CRASH:
				crash = (struct crash_t *)e->private;
				LOGE("CRASH (%s, %s)\n", (char *)crash->name,
				     e->path);
				break;
			case INFO:
				info = (struct info_t *)e->private;
				LOGE("INFO (%s)\n", (char *)info->name);
				break;
			case UPTIME:
				LOGE("UPTIME\n");
				break;
			case HEART_BEAT:
				LOGE("HEART_BEAT\n");
				break;
			case REBOOT:
				LOGE("REBOOT\n");
				break;
			default:
				LOGE("error event type %d\n", e->event_type);
			}
			free(e);
		}

		raise(SIGKILL);
	}
}

/**
 * Fed watchdog.
 *
 * @param timeout When the watchdog expire next time.
 */
static void watchdog_fed(int timeout)
{
	struct itimerval new_value;
	int ret;

	memset(&new_value, 0, sizeof(new_value));

	new_value.it_value.tv_sec = timeout;
	ret = setitimer(ITIMER_REAL, &new_value, NULL);
	if (ret < 0) {
		LOGE("setitimer failed, error (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

/**
 * Initialize watchdog. This watchdog is used to monitor event handler.
 *
 * @param timeout When the watchdog expire next time.
 */
static void watchdog_init(int timeout)
{
	struct itimerval new_value;
	int ret;

	signal(SIGALRM, wdt_timeout);
	memset(&new_value, 0, sizeof(new_value));

	new_value.it_value.tv_sec = timeout;
	ret = setitimer(ITIMER_REAL, &new_value, NULL);
	if (ret < 0) {
		LOGE("setitimer failed, error (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

/**
 * Process each event in event queue.
 * Note that currently event handler is single threaded.
 */
static void *event_handle(void *unused __attribute__((unused)))
{
	int id;
	struct sender_t *sender;
	struct event_t *e;

	while ((e = event_dequeue())) {
		/* here we only handle internal event */
		if (e->event_type == HEART_BEAT) {
			watchdog_fed(WDT_TIMEOUT);
			free(e);
			continue;
		}

		last_e = malloc(sizeof(*e) + e->len);
		if (last_e == NULL) {
			LOGE("malloc failed, error (%s)\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		event_processing = 1;
		memcpy(last_e, e, sizeof(*e) + e->len);

		for_each_sender(id, sender) {
			if (!sender)
				continue;

			sender->send(e);
		}

		if ((e->dir))
			free(e->dir);
		free(e);
		event_processing = 0;
		free(last_e);
	}

	LOGE("something goes error, event_handle exit\n");
	return NULL;
}

/**
 * Initialize event handler.
 */
int init_event_handler(void)
{
	int ret;
	pthread_t pid;

	watchdog_init(WDT_TIMEOUT);
	ret = create_detached_thread(&pid, &event_handle, NULL);
	if (ret) {
		LOGE("create event handler failed (%s)\n", strerror(errno));
		return -1;
	}
	return 0;
}
