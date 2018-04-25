/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "client.h"
#include "crash_dump.h"
#include "log_sys.h"

/**
 * Usercrash works as C/S model: debugger can works as usercrash client
 * only when happens crash event. For each time, debugger receives 4 params and
 * sends connect event to server, then it receives file fd from server to fill
 * crash info into the file. After this work is done, it will notify server that
 * dump work is completed.
 * Debugger can also work without server when uses "debugger pid" commands to
 * debug the running process. This function will dump the process info on the
 * screen, also you can relocate the info to a file.
 */
static void print_usage(void)
{
	printf("debugger - tool to dump process info of a running process. ");
	printf("It's the client role of usercrash.\n");
	printf("[Usage]\n");
	printf("\t--shell cmd, debugger <pid>  (root role to run)\n");
	printf("\t--coredump, debugger <pid> <tgid> <comm> <sig> ");
	printf("(root role to run)\n");
	printf("[Option]\n");
	printf("\t-h: print this usage message\n");
	printf("\t-v: print debugger version\n");
}

int main(int argc, char *argv[])
{
	int pid;
	int sig;
	int out_fd;
	int sock;
	int ret;

	if (argc > 1) {
		if (strcmp(argv[1], "-v") == 0) {
			printf("debugger version is 1.0\n");
			return 0;
		}
		if (strcmp(argv[1], "-h") == 0) {
			print_usage();
			return 0;
		}
	} else
		print_usage();

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
