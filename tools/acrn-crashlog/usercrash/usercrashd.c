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

/*
 * Copyright (C) 2018 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <malloc.h>

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/thread.h>


#include "handler.h"
#include "protocol.h"
#include "packettype.h"
#include "log_sys.h"

/**
 * Ownership of Crash is a bit messy.
 * It's either owned by an active event that must have a timeout, or owned by
 * queued_requests, in the case that multiple crashes come in at the same time.
 */

struct crash_node {
	int crash_fd;
	int pid;
	int out_fd;
	char name[COMM_NAME_LEN];
	struct event *crash_event;
	char crash_path[PATH_MAX];

	TAILQ_ENTRY(crash_node) entries;
};

static const char usercrash_directory[] = "/var/log/usercrashes";
static size_t usercrash_count = 50;
static int next_usercrash;

static size_t kMaxConcurrentDumps = 1;
static size_t num_concurrent_dumps;
TAILQ_HEAD(, crash_node) queue_t;
static pthread_mutex_t queue_mtx = PTHREAD_MUTEX_INITIALIZER;


static void push_back(struct crash_node *crash);
static struct crash_node *pop_front(void);

/* Forward declare the callbacks so they can be placed in a sensible order */
static void crash_accept_cb(struct evconnlistener *listener,
		evutil_socket_t sockfd, struct sockaddr*, int, void*);
static void crash_request_cb(evutil_socket_t sockfd, short ev, void *arg);
static void crash_completed_cb(evutil_socket_t sockfd, short ev, void *arg);

static void free_crash(struct crash_node *crash)
{
	event_free(crash->crash_event);
	close(crash->crash_fd);
	free(crash);
}

static void push_back(struct crash_node *crash)
{
	pthread_mutex_lock(&queue_mtx);
	TAILQ_INSERT_TAIL(&queue_t, crash, entries);
	pthread_mutex_unlock(&queue_mtx);
}

static struct crash_node *pop_front(void)
{
	struct crash_node *crash = NULL;

	pthread_mutex_lock(&queue_mtx);
	if (!TAILQ_EMPTY(&queue_t)) {
		crash = TAILQ_FIRST(&queue_t);
		TAILQ_REMOVE(&queue_t, crash, entries);
	}
	pthread_mutex_unlock(&queue_mtx);

	return crash;
}

static void find_oldest_usercrash(void)
{
	size_t oldest_usercrash = 0;
	time_t oldest_time = LONG_MAX;
	size_t i = 0;
	char path[PATH_MAX];
	struct stat st;

	for (i = 0; i < usercrash_count; ++i) {
		snprintf(path, sizeof(path), "%s/usercrash_%02lu",
				usercrash_directory, i);
		if (stat(path, &st) != 0) {
			if (errno == ENOENT) {
				oldest_usercrash = i;
				break;
			}

			LOGE("failed to stat %s\n", path);
			continue;
		}

		if (st.st_mtime < oldest_time) {
			oldest_usercrash = i;
			oldest_time = st.st_mtime;
		}
	}
	next_usercrash = oldest_usercrash;
}

static int get_usercrash(struct crash_node *crash)
{
	/* If kMaxConcurrentDumps is greater than 1, then theoretically the
	 * same filename could be handed out to multiple processes. Unlink and
	 * create the file, instead of using O_TRUNC, to avoid two processes
	 * interleaving their output
	 */
	int result;
	char file_name[PATH_MAX];

	snprintf(file_name, sizeof(file_name), "%s/usercrash_%02d",
				usercrash_directory, next_usercrash);

	if (unlink(file_name) != 0 && errno != ENOENT) {
		LOGE("failed to unlink usercrash at %s\n", file_name);
		return -1;
	}
	result = open(file_name, O_CREAT | O_WRONLY, 0644);
	if (result == -1) {
		LOGE("failed to create usercrash at %s\n", file_name);
		return -1;

	}
	next_usercrash = (next_usercrash + 1) % usercrash_count;
	crash->out_fd = result;
	strncpy(crash->crash_path, file_name, PATH_MAX - 1);

	return 0;
}

static void perform_request(struct crash_node *crash)
{
	ssize_t rc;
	struct crash_packet response = {0};

	if (get_usercrash(crash)) {
		LOGE("usercrashd exit for open usercrash file failed\n");
		exit(EXIT_FAILURE);
	}

	LOGI("Prepare to write the '%s' log to %s\n",
				crash->name, crash->crash_path);
	response.packet_type = kPerformDump;
	rc = send_fd(crash->crash_fd, &response,
				sizeof(response), crash->out_fd);
	close(crash->out_fd);
	if (rc == -1) {
		LOGE("failed to send response to CrashRequest\n");
		goto fail;
	} else if (rc != sizeof(response)) {
		LOGE("crash socket write returned short\n");
		goto fail;
	} else {
		struct timeval timeout = { 100, 0 };
		struct event_base *base = event_get_base(crash->crash_event);

		event_assign(crash->crash_event, base, crash->crash_fd,
				EV_TIMEOUT | EV_READ,
				crash_completed_cb, crash);
		event_add(crash->crash_event, &timeout);
	}

	++num_concurrent_dumps;
	return;

fail:
	free_crash(crash);
}

static void dequeue_requests(void)
{
	while (!TAILQ_EMPTY(&queue_t) &&
				num_concurrent_dumps < kMaxConcurrentDumps) {
		struct crash_node *next_crash = pop_front();

		perform_request(next_crash);
	}
}

static void crash_accept_cb(struct evconnlistener *listener,
			evutil_socket_t sockfd,
		struct sockaddr *sa __attribute__((unused)),
		int socklen __attribute__((unused)),
		void *user_data __attribute__((unused)))
{
	struct event_base *base = evconnlistener_get_base(listener);
	struct crash_node *crash = (struct crash_node *)malloc(sizeof(struct crash_node));

	struct timeval timeout = { 1, 0 };
	struct event *crash_event = event_new(base, sockfd,
			EV_TIMEOUT | EV_READ, crash_request_cb, crash);
	crash->crash_fd = sockfd;
	crash->crash_event = crash_event;
	event_add(crash_event, &timeout);
}

static void crash_request_cb(evutil_socket_t sockfd, short ev, void *arg)
{
	ssize_t rc;
	struct crash_node *crash = arg;
	struct crash_packet request = {0};

	if ((ev & EV_TIMEOUT) != 0) {
		LOGE("crash request timed out\n");
		goto fail;
	} else if ((ev & EV_READ) == 0) {
		LOGE("usercrashd received unexpected event ");
		LOGE("from crash socket\n");
		goto fail;
	}

	rc = read(sockfd, &request, sizeof(request));
	if (rc == -1) {
		LOGE("failed to read from crash socket\n");
		goto fail;
	} else if (rc != sizeof(request)) {
		LOGE("crash socket received short read of length %lu ", rc);
		LOGE("(expected %lu)\n", sizeof(request));
		goto fail;
	}

	if (request.packet_type != kDumpRequest) {
		LOGE("unexpected crash packet type, expected kDumpRequest, ");
		LOGE("received %d\n", request.packet_type);
		goto fail;
	}
	crash->pid = request.pid;
	strncpy(crash->name, request.name, COMM_NAME_LEN - 1);

	LOGI("received crash request from pid %d, name: %s\n",
				crash->pid, crash->name);

	if (num_concurrent_dumps == kMaxConcurrentDumps) {
		LOGI("enqueueing crash request for pid %d\n", crash->pid);
		push_back(crash);
	} else {
		perform_request(crash);
	}

	return;

fail:
	free_crash(crash);
}

static void crash_completed_cb(evutil_socket_t sockfd, short ev, void *arg)
{
	ssize_t rc;
	struct crash_node *crash = arg;
	struct crash_packet request = {0};

	--num_concurrent_dumps;

	if ((ev & EV_TIMEOUT) != 0) {
		LOGE("error for crash request timed out, error (%s)\n",
					strerror(errno));
		goto fail;
	} else if ((ev & EV_READ) == 0) {
		LOGE("usercrashd received unexpected event ");
		LOGE("from crash socket\n");
		goto fail;
	}

	rc = read(sockfd, &request, sizeof(request));
	if (rc == -1) {
		LOGE("failed to read from crash socket\n");
		goto fail;
	} else if (rc != sizeof(request)) {
		LOGE("crash socket received short read of length %lu, ", rc);
		LOGE("(expected %lu)\n", sizeof(request));
		goto fail;
	}

	if (request.packet_type != kCompletedDump) {
		LOGE("unexpected crash packet type, expected kCompletedDump, ");
		LOGE("received %d\n", request.packet_type);
		goto fail;
	}

	if (crash->crash_path) {
		LOGI("usercrash log written to: %s, ", crash->crash_path);
		LOGI("crash process name is: %s\n", crash->name);
	}

fail:
	free_crash(crash);
	/* If there's something queued up, let them proceed */
	dequeue_requests();
}

static void sig_handler(int sig)
{
	LOGE("received fatal signal: %d\n", sig);
	exit(EXIT_FAILURE);
}

int main(void)
{
	char socket_path[128];
	DIR *dir;
	int fd;
	int crash_socket;
	struct sigaction action;
	struct event_base *base;
	struct evconnlistener *listener;

	if (getuid() != 0) {
		LOGE("failed to boot usercrashd, root is required\n");
		exit(EXIT_FAILURE);
	}

	dir = opendir(usercrash_directory);
	if (dir == NULL) {
		fd = mkdir(usercrash_directory, 0755);
		if (fd == -1) {
			LOGE("create log folder failed, error (%s)\n",
					strerror(errno));
			exit(EXIT_FAILURE);
		}
		if (chmod(usercrash_directory, 0755) == -1) {
			LOGE("failed to change usercrash folder priority, ");
			LOGE("error (%s)\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
	} else {
		closedir(dir);
	}

	/* Don't try to connect to ourselves if we crash */
	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = sig_handler;
	debuggerd_register_handlers(&action);

	find_oldest_usercrash();

	crash_socket = linux_get_control_socket(SOCKET_NAME);

	if (crash_socket == -1) {
		LOGE("failed to get socket from init, error (%s)\n",
					strerror(errno));
		exit(EXIT_FAILURE);
	}
	snprintf(socket_path, sizeof(socket_path), "%s/%s",
			RESERVED_SOCKET_PREFIX, SOCKET_NAME);
	if (chmod(socket_path, 0622) == -1) {
		LOGE("failed to change usercrashd_crash priority\n");
		exit(EXIT_FAILURE);
	}

	evutil_make_socket_nonblocking(crash_socket);

	base = event_base_new();
	if (!base) {
		LOGE("failed to create event_base\n");
		exit(EXIT_FAILURE);
	}

	TAILQ_INIT(&queue_t);
	listener = evconnlistener_new(base, crash_accept_cb, NULL,
			LEV_OPT_CLOSE_ON_FREE, -1, crash_socket);
	if (!listener) {
		LOGE("failed to create evconnlistener: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	LOGI("usercrashd successfully initialized\n");
	event_base_dispatch(base);

	evconnlistener_free(listener);
	event_base_free(base);

	return 0;
}
