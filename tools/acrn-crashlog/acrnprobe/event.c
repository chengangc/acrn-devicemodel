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
#include <sys/queue.h>
#include <pthread.h>
#include "event.h"
#include "log_sys.h"

static pthread_mutex_t eq_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pcond = PTHREAD_COND_INITIALIZER;

/**
 * Push an event to event_queue.
 *
 * @param event Event to process.
 */
void event_enqueue(struct event_t *event)
{
	pthread_mutex_lock(&eq_mtx);
	TAILQ_INSERT_TAIL(&event_q, event, entries);
	pthread_cond_signal(&pcond);
	LOGD("enqueue %d, (%d)%s\n", event->event_type, event->len,
	     event->path);
	pthread_mutex_unlock(&eq_mtx);
}

/**
 * Count the number of events in event_queue.
 *
 * @return count.
 */
int events_count(void)
{
	struct event_t *e;
	int count = 0;

	pthread_mutex_lock(&eq_mtx);
	TAILQ_FOREACH(e, &event_q, entries)
		count++;
	pthread_mutex_unlock(&eq_mtx);

	return count;
}

/**
 * Dequeue an event from event_queue.
 *
 * @return the dequeued event.
 */
struct event_t *event_dequeue(void)
{
	struct event_t *e;

	pthread_mutex_lock(&eq_mtx);
	while (TAILQ_EMPTY(&event_q))
		pthread_cond_wait(&pcond, &eq_mtx);
	e = TAILQ_FIRST(&event_q);
	TAILQ_REMOVE(&event_q, e, entries);
	LOGD("dequeue %d, (%d)%s\n", e->event_type, e->len, e->path);
	pthread_mutex_unlock(&eq_mtx);

	return e;
}

/**
 * Initailize event_queue.
 */
void init_event(void)
{
	TAILQ_INIT(&event_q);
}
