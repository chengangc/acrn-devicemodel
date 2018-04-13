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
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "usercrashd.h"
#include "log_sys.h"

static void print_usage(void)
{
	LOGI("Usage:\n");
	LOGI("\t--shell cmd, debugger <pid>\n");
	LOGI("\t--coredump, debugger <pid> <tgid> <comm> <sig>\n");
}

int main(int argc, char *argv[])
{
	int pid;
	int sig;
	int out_fd;
	int sock;
	int ret;

	if (getuid() != 0) {
		LOGE("failed to execute debugger, root is required\n");
		exit(EXIT_FAILURE);
	}

	if (argc == 2) {
		/* it's from shell cmd */
		pid = atoi(argv[1]);
		crash_dump(pid, 0, STDOUT_FILENO);
	} else if (argc == 4) {
		/* it's from coredump */
		pid = atoi(argv[1]);
		sig = atoi(argv[3]);
		ret = usercrashd_connect(pid, &sock, &out_fd, argv[2]);
		if (!ret) {
			LOGE("usercrashd_connect failed, error (%s)\n",
					strerror(errno));
			exit(EXIT_FAILURE);
		}
		crash_dump(pid, sig, out_fd);
		close(out_fd);
		if (!usercrashd_notify_completion(sock)) {
			LOGE("failed to notify usercrashd of completion");
			close(sock);
			exit(EXIT_FAILURE);
		}
		close(sock);
	} else {
		print_usage();
		return 0;
	}

	return 0;
}
