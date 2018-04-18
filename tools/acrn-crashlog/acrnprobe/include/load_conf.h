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

#ifndef __LOAD_CONF_H__
#define __LOAD_CONF_H__

#include <stdio.h>
#include <sys/queue.h>
#include <openssl/sha.h>
#include "event.h"

#define CONTENT_MAX 10
#define EXPRESSION_MAX 5
#define LOG_MAX 20
#define TRIGGER_MAX 20
#define SENDER_MAX 3
#define DATA_MAX 3
#define CRASH_MAX 20
#define INFO_MAX 20
#define VM_MAX 4
#define VM_EVENT_TYPE_MAX 20

struct trigger_t {
	char *name;
	char *type;
	char *path;
};

struct vm_t {
	char *name;
	char *channel;
	char *interval;
	char *syncevent[VM_EVENT_TYPE_MAX];

	int online;
	unsigned long history_size[SENDER_MAX];
	char last_synced_line_key[SENDER_MAX][SHA_DIGEST_LENGTH + 1];
};

struct log_t {
	char *name;
	char *type;
	char *path;
	char *lines;

	void (*get)(struct log_t *, void *);
};

struct crash_t {
	char *name;
	char *channel;
	char *interval;
	struct trigger_t *trigger;
	char *content[CONTENT_MAX];
	char *mightcontent[EXPRESSION_MAX][CONTENT_MAX];
	struct log_t *log[LOG_MAX];
	char *data[DATA_MAX];

	struct crash_t *parents;

	TAILQ_ENTRY(crash_t) entries;
	TAILQ_HEAD(, crash_t) children;

	int wd;
	int level;
	struct crash_t *(*reclassify)(struct crash_t *, char*, char**, char**,
				char**);
};

struct info_t {
	char *name;
	char *channel;
	char *interval;
	struct trigger_t *trigger;
	struct log_t *log[LOG_MAX];
};

struct uptime_t {
	char *name;
	char *frequency;
	char *eventhours;

	int wd;
	char *path;
};

struct sender_t {
	char *name;
	char *outdir;
	char *maxcrashdirs;
	char *maxlines;
	char *spacequota;
	struct uptime_t *uptime;

	void (*send)(struct event_t *);
	char *log_vmrecordid;
	int sw_updated; /* each sender has their own record */
};

struct conf_t {
	struct sender_t *sender[SENDER_MAX];
	struct vm_t *vm[VM_MAX];
	struct trigger_t *trigger[TRIGGER_MAX];
	struct log_t *log[LOG_MAX];
	struct crash_t *crash[CRASH_MAX];
	struct info_t *info[INFO_MAX];
} conf;

#define for_each_sender(id, sender) \
		for (id = 0, sender = conf.sender[0]; \
		     id < SENDER_MAX; \
		     id++, sender = conf.sender[id])

#define for_each_trigger(id, trigger) \
		for (id = 0, trigger = conf.trigger[0]; \
		     id < TRIGGER_MAX; \
		     id++, trigger = conf.trigger[id])

#define for_each_vm(id, vm) \
		for (id = 0, vm = conf.vm[0]; \
		     id < VM_MAX; \
		     id++, vm = conf.vm[id])

#define for_each_syncevent_vm(id, event, vm) \
		for (id = 0, event = vm->syncevent[0]; \
		     id < VM_EVENT_TYPE_MAX; \
		     id++, event = vm->syncevent[id])

#define for_each_info(id, info) \
		for (id = 0, info = conf.info[0]; \
		     id < INFO_MAX; \
		     id++, info = conf.info[id])

#define for_each_log(id, log) \
		for (id = 0, log = conf.log[0]; \
		     id < LOG_MAX; \
		     id++, log = conf.log[id])

#define for_each_crash(id, crash) \
		for (id = 0, crash = conf.crash[0]; \
		     id < CRASH_MAX; \
		     id++, crash = conf.crash[id])

#define for_each_log_collect(id, log, type) \
		for (id = 0, log = type->log[0]; \
		     id < LOG_MAX; \
		     id++, log = type->log[id])

#define for_each_content_crash(id, content, crash) \
		for (id = 0, content = crash->content[0]; \
		     id < CONTENT_MAX; \
		     id++, content = crash->content[id])

#define for_each_content_expression(id, content, exp) \
		for (id = 0, content = exp[0]; \
		     id < CONTENT_MAX; \
		     id++, content = exp[id])

#define exp_valid(exp) \
(__extension__ \
({ \
		int _ret = 0; \
		int _id; \
		char *content; \
		for_each_content_expression(_id, content, exp) { \
			if (content) \
				_ret = 1; \
		} \
		_ret; \
}) \
)

#define for_each_expression_crash(id, exp, crash) \
		for (id = 0, exp = crash->mightcontent[0]; \
		     id < EXPRESSION_MAX; \
		     id++, exp = crash->mightcontent[id])

#define for_crash_children(crash, tcrash) \
			TAILQ_FOREACH(crash, &tcrash->children, entries)

#define is_leaf_crash(crash) \
			(crash && TAILQ_EMPTY(&crash->children))

#define is_root_crash(crash) \
			(crash && crash->parents == NULL)

#define to_collect_logs(type) \
(__extension__ \
({ \
		int _id; \
		int _ret = 0; \
		for (_id = 0; _id < LOG_MAX; _id++) \
			if (type->log[_id]) \
				_ret = 1; \
		_ret; \
}) \
)

int load_conf(char *path);
struct trigger_t *get_trigger_by_name(char *name);
struct log_t *get_log_by_name(char *name);
int sender_id(struct sender_t *sender);
struct sender_t *get_sender_by_name(char *name);
enum event_type_t get_conf_by_wd(int wd, void **private);
struct crash_t *get_crash_by_wd(int wd);
int crash_depth(struct crash_t *tcrash);

#endif
