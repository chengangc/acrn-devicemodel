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
#include <malloc.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "event.h"
#include "load_conf.h"
#include "channels.h"
#include "event_handler.h"
#include "history.h"
#include "property.h"
#include "crash_reclassify.h"
#include "sender.h"
#include "log_sys.h"

#define CONFIG_INSTALL "/usr/share/defaults/telemetrics/acrnprobe.xml"
#define CONFIG_CUSTOMIZE "/etc/acrnprobe.xml"

static void uptime(struct sender_t *sender)
{
	int fd;
	int frequency;
	struct uptime_t *uptime;

	uptime = sender->uptime;
	frequency = atoi(uptime->frequency);
	sleep(frequency);
	fd = open(uptime->path, O_RDWR | O_CREAT, 0666);
	if (fd < 0)
		LOGE("open uptime with (%d, %s) failed, error (%s)\n",
				atoi(uptime->frequency), uptime->path,
				strerror(errno));
	else
		close(fd);
}

int main(int argc, char *argv[])
{
	int ret;
	int id;
	struct sender_t *sender;
	char *config_path[2] = {CONFIG_CUSTOMIZE,
				CONFIG_INSTALL};

	if (argc == 2) {
		ret = load_conf(argv[1]);
	} else {
		if (file_exists(config_path[0]))
			ret = load_conf(config_path[0]);
		else
			ret = load_conf(config_path[1]);
	}
	if (ret)
		return -1;

	init_crash_reclassify();
	ret = init_sender();
	if (ret)
		return -1;

	init_event();
	ret = init_event_handler();
	if (ret)
		return -1;

	ret = init_channels();
	if (ret)
		return -1;

	while (1) {
		for_each_sender(id, sender) {
			if (!sender)
				continue;
			uptime(sender);
		}
	}

	return 0;
}
