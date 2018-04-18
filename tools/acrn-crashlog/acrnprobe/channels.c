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
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <errno.h>
#include <sys/inotify.h>
#include <sys/epoll.h>
#include <time.h>
#include <signal.h>
#include "load_conf.h"
#include "event.h"
#include "fsutils.h"
#include "strutils.h"
#include "channels.h"
#include "log_sys.h"

#define POLLING_TIMER_SIG 0xCEAC

static void channel_oneshot(char *name);
static void channel_polling(char *name);
static void channel_inotify(char *name);

/**
 * @brief structure containing implementation of each channel.
 *
 * This structure describes all channels, all channel_* functions would
 * called by main thread in order.
 */
static struct channel_t channels[] = {
	{"oneshot", 0, channel_oneshot},
	{"polling", 0, channel_polling},
	{"inotify", 0, channel_inotify},
};

#define for_each_channel(i, channel) \
	for (i = 0, channel = &channels[0]; \
	     i < (int)ARRAY_SIZE(channels); \
	     i++, channel++)

/**
 * Helper function to create a event and fill event_t structure.
 *
 * @param event_type Type of this event.
 * @param channel Channel where the event comes from.
 * @param private The corresponding configuration info to the event.
 * @param watchfd For watch channel, so far, only used by inotify.
 * @param path File which trigger this event.
 * @param len Length of path.
 *
 * @return a pointer to the filled event if successful,
 *  or NULL on error.
 */
static struct event_t *create_event(enum event_type_t event_type,
				char *channel, void *private,
				int watchfd, char *path, int len)
{
	struct event_t *e;

	if (len < 0)
		return NULL;

	e = malloc(sizeof(*e) + len + 1);
	if (e) {
		memset(e, 0, sizeof(*e) + len + 1);
		e->watchfd = watchfd;
		e->channel = channel;
		e->private = (void *)private;
		e->event_type = event_type;
		if (path && (len > 0)) {
			e->len = len;
			strncpy(e->path, path, len);
		}
	} else {
		LOGE("malloc failed, error (%s)\n", strerror(errno));
	}

	return e;
}

/**
 * Only check once when process startup
 *
 * @param cname Name of channel.
 */
static void channel_oneshot(char *cname)
{
	int id;
	struct crash_t *crash;
	struct info_t *info;
	struct event_t *e;

	LOGD("initializing channel %s ...\n", cname);

	e = create_event(REBOOT, cname, NULL, 0, NULL, 0);
	if (e)
		event_enqueue(e);


	for_each_crash(id, crash) {
		if (!crash || !is_root_crash(crash))
			continue;

		if (strcmp(crash->channel, cname))
			continue;

		if (crash->trigger &&
		    !strcmp("file", crash->trigger->type) &&
		    file_exists(crash->trigger->path)) {
			e = create_event(CRASH, cname, (void *)crash,
					 0, crash->trigger->path,
					 strlen(crash->trigger->path));
			if (e)
				event_enqueue(e);
		}
	}

	for_each_info(id, info) {
		if (!info)
			continue;

		if (strcmp(info->channel, cname))
			continue;

		if (info->trigger &&
		    !strcmp("file", info->trigger->type) &&
		    file_exists(info->trigger->path)) {
			e = create_event(INFO, cname, (void *)info,
					 0, NULL, 0);
			if (e)
				event_enqueue(e);
		}
	}
}

/* TODO: implement multiple polling jobs */
static struct polling_job_t {
	timer_t timerid;
	uint32_t timer_val;

	enum event_type_t type;
	void (*fn)(union sigval v);
} vm_job;

/**
 * Callback thread of a polling job.
 */
static void polling_vm(union sigval v __attribute__((unused)))
{

	struct event_t *e = create_event(VM, "polling", NULL, 0, NULL, 0);

	if (e)
		event_enqueue(e);
}

/**
 * Setup a timer with specific loop time. The callback fn will be performed
 * after timer expire.
 *
 * @param pjob Polling_job filled by caller.
 *
 * @return 0 if successful, or -1 if not.
 */
static int create_polling_job(struct polling_job_t *pjob)
{
	struct sigevent sig_evt;
	struct itimerspec timer_val;

	memset(&sig_evt, 0, sizeof(struct sigevent));
	sig_evt.sigev_value.sival_int = POLLING_TIMER_SIG;
	sig_evt.sigev_notify = SIGEV_THREAD;
	sig_evt.sigev_notify_function = pjob->fn;

	if (timer_create(CLOCK_REALTIME, &sig_evt, &pjob->timerid) == -1) {
		LOGE("timer_create failed.\n");
		return -1;
	}

	memset(&timer_val, 0, sizeof(struct itimerspec));
	timer_val.it_value.tv_sec = pjob->timer_val;
	timer_val.it_interval.tv_sec = pjob->timer_val;

	if (timer_settime(pjob->timerid, 0, &timer_val, NULL) == -1) {
		LOGE("timer_settime failed.\n");
		timer_delete(pjob->timerid);
		return -1;
	}

	return 0;
}

/**
 * Setup polling jobs. These jobs running with fixed time interval.
 *
 * @param cname Name of channel.
 */
static void channel_polling(char *cname)
{
	int id;
	int ret;
	struct vm_t *vm;

	LOGD("initializing channel %s ...\n", cname);

	/* one job for all vm polling*/
	for_each_vm(id, vm) {
		if (!vm)
			continue;

		if (strncmp(vm->channel, "polling", strlen(vm->channel)))
			continue;

		vm_job.timer_val = atoi(vm->interval);
	}

	LOGD("start polling job with %ds\n", vm_job.timer_val);
	vm_job.fn = polling_vm;
	vm_job.type = VM;
	ret = create_polling_job(&vm_job);
	if (ret < 0) {
		LOGE("create polling job failed\n, error (%s)\n",
		     strerror(errno));
		exit(EXIT_FAILURE);
	}
}

/**
 * Helper function for watch needed channels.
 * Set the watched fd to channel structure.
 *
 * @param event_type Type of this event.
 */
static void prepare_to_wait(int fd, char *cname)
{
	int id;
	struct channel_t *channel;

	for_each_channel(id, channel)
		if (cname == channel->name) {
			LOGI("with (%s, %d) succuessed\n", cname, fd);
			channel->fd = fd;
		}
}

/**
 * Setup inotify, watch the changes of dir/file.
 *
 * @param cname Name of channel.
 */
static void channel_inotify(char *cname)
{
	int inotify_fd;
	int id;
	struct crash_t *crash;
	struct sender_t *sender;
	struct uptime_t *uptime;
	struct trigger_t *trigger;

	LOGD("initializing channel %s ...\n", cname);

	/* use this func to get "return 0" from read */
	inotify_fd = inotify_init1(IN_NONBLOCK);
	if (inotify_fd < 0) {
		LOGE("inotify init fail, %s\n", strerror(errno));
		return;
	}

	for_each_crash(id, crash) {
		if (!crash || !is_root_crash(crash))
			continue;

		if (strcmp(crash->channel, cname))
			continue;

		if (!crash->trigger)
			continue;

		trigger = crash->trigger;
		if (!strcmp("dir", trigger->type)) {
			if (directory_exists(trigger->path)) {
				crash->wd = inotify_add_watch(inotify_fd,
							      trigger->path,
							      BASE_DIR_MASK);
				if (crash->wd < 0) {
					LOGE("add %s failed, error (%s)\n",
					     trigger->path, strerror(errno));
					return;
				}
				LOGI("add %s succuessed\n", trigger->path);
			} else {
				LOGW("path to watch (%s) isn't exsits\n",
				     trigger->path);
			}
		}
		/* TODO: else for other types to use channel inotify */
	}
	/* add uptime path for each sender */
	for_each_sender(id, sender) {
		if (!sender)
			continue;

		uptime = sender->uptime;
		uptime->wd = inotify_add_watch(inotify_fd, uptime->path,
					       UPTIME_MASK);
		if (uptime->wd < 0) {
			LOGE("add %s failed, error (%s)\n",
			     uptime->path, strerror(errno));
			return;
		}
		LOGI("add %s succuessed\n", uptime->path);
	}

	prepare_to_wait(inotify_fd, cname);
}

/**
 * Handle inotify events, read out all events and enqueue.
 *
 * @param channel Channel structure of inotify.
 *
 * @return 0 if successful, or -1 if not.
 */
static int receive_inotify_events(struct channel_t *channel)
{
	int len;
	int read_left;
	char buf[256];
	char *p;
	struct event_t *e;
	struct inotify_event *ievent;
	enum event_type_t event_type;
	void *private;

	read_left = 0;
	while (1) {
		len = read(channel->fd, (char *)&buf[read_left],
			   (int)sizeof(buf) - read_left);
		if (len < 0) {
			if (errno == EAGAIN)
				break;
			LOGE("read fail with (%d, %p, %d), error: %s\n",
			     channel->fd, (char *)&buf[read_left],
			     (int)sizeof(buf) - read_left, strerror(errno));
			return -1;
		}
		if (len == 0)
			break;

		for (p = buf; p < buf + read_left + len;) {
			if (p + sizeof(struct inotify_event) >
			    &buf[0] + len + read_left) {
				/* we dont recv the entire inotify_event yet */
				break;
			}

			/* then we can get len */
			ievent = (struct inotify_event *)p;
			if (p + sizeof(struct inotify_event) + ievent->len >
						&buf[0] + len + read_left) {
				/* we dont recv the entire
				 * inotify_event + name
				 */
				break;
			}
			/* we have a entire event, send it... */
			event_type = get_conf_by_wd(ievent->wd, &private);
			if (event_type == UNKNOWN) {
				LOGE("get a unknown event\n");
			} else {
				e = create_event(event_type, channel->name,
						 private, channel->fd,
						 ievent->name, ievent->len);
				if (e)
					event_enqueue(e);
			}
			/* next event start */
			p += sizeof(struct inotify_event) + ievent->len;
		}
		/* move the bytes that have been read out to the head of buf,
		 * and let the rest of the buf do recv continually
		 */
		read_left = &buf[0] + len + read_left - p;
		memmove(buf, p, read_left);
	}

	return 0;
}

/**
 * Push a HEART_BEAT event to event_queue.
 */
static void heart_beat(void)
{
	struct event_t *e = create_event(HEART_BEAT, NULL, NULL, 0, NULL, 0);

	if (e)
		event_enqueue(e);
}

/**
 * Wait events asynchronously for all watch needed channels.
 */
static void *wait_events(void *unused __attribute__((unused)))
{
	int epfd;
	int id;
	int ret;
	struct channel_t *channel;
	struct epoll_event ev, *events;

	epfd = epoll_create(MAXEVENTS + 1);
	if (epfd < 0) {
		LOGE("epoll_create failed, exiting\n");
		exit(EXIT_FAILURE);
	}

	ev.events = EPOLLIN | EPOLLET;
	for_each_channel(id, channel) {
		if (channel->fd <= 0)
			continue;

		ev.data.fd = channel->fd;
		ret = epoll_ctl(epfd, EPOLL_CTL_ADD, channel->fd, &ev);
		if (ret < 0) {
			LOGE("epoll_ctl failed, exiting\n");
			exit(EXIT_FAILURE);
		} else
			LOGD("add (%d) to epoll for (%s)\n", channel->fd,
			     channel->name);
	}

	events = calloc(MAXEVENTS, sizeof(ev));
	if (events == NULL) {
		LOGE("calloc failed, error (%s)\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	while (1) {
		int i;
		int n;

		n = epoll_wait(epfd, events, MAXEVENTS, HEART_RATE);
		heart_beat();
		for (i = 0; i < n; i++) {
			for_each_channel(id, channel)
				if (channel->fd == events[i].data.fd) {
					if (events[i].events & EPOLLERR ||
					    !(events[i].events & EPOLLIN)) {
						LOGE("error ev, channel:%s\n",
						     channel->name);
						continue;
					}
					/* Until now, we only have
					 * inotify channel to wait
					 */
					receive_inotify_events(channel);
				}
		}
	}
}

/**
 * Create a detached thread.
 * Once these thread exit, their resources would be released immediately.
 *
 * @param[out] pid Pid of new thread.
 * @param fn The entry of new thread.
 * @param arg The arg of fn.
 *
 * @return 0 if successful, or errno if not.
 */
int create_detached_thread(pthread_t *pid, void *(*fn)(void *), void *arg)
{
	int ret;
	pthread_attr_t attr;

	ret = pthread_attr_init(&attr);
	if (ret) {
		LOGE("pthread attr init failed\n, error (%s)\n",
		     strerror(ret));
		return ret;
	}

	ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (ret) {
		LOGE("pthread attr setdetachstate failed\n, error (%s)\n",
		     strerror(ret));
		goto fail;
	}

	ret = pthread_create(pid, &attr, fn, arg);
	if (ret) {
		LOGE("pthread create failed\n, error (%s)\n",
		     strerror(ret));
	}
fail:
	pthread_attr_destroy(&attr);

	return ret;
}

/**
 * Initailize all channels, performing channel_* in channels one by one.
 *
 * @return 0 if successful, or errno if not.
 */
int init_channels(void)
{
	pthread_t pid;
	int id;
	int ret;
	struct channel_t *channel;

	for_each_channel(id, channel) {
		channel->channel_fn(channel->name);
	}

	ret = create_detached_thread(&pid, &wait_events, NULL);
	if (ret) {
		LOGE("create wait_events fail, ret (%s)\n", strerror(ret));
		return ret;
	}

	return 0;
}