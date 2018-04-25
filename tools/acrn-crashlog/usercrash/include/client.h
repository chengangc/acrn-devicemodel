/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CLIENT_H
#define CLIENT_H

#define COMM_NAME_LEN 64
#define SOCKET_NAME "user_crash"

#include <stdio.h>
#include <stdbool.h>

enum CrashPacketType {
	/* Initial request from crash_dump */
	kDumpRequest = 0,

	/* Notification of a completed crash dump */
	kCompletedDump,

	/* Responses to kRequest */
	kPerformDump
};

struct crash_packet {
	enum CrashPacketType packet_type;
	int pid;
	char name[COMM_NAME_LEN];
};

/**
 * @usercrashd_socket: client socket fd pointer, get the value from
 *			usercrashd_connect function.
 * @output_fd: this fd is created and sent by server, used to store
 *			crash file logs in client
 */
bool usercrashd_connect(int pid, int *usercrashd_socket, int *output_fd,
			char *name);

/* @usercrashd_socket: client socket fd, used to communicate with server */
bool usercrashd_notify_completion(int usercrashd_socket);

#endif
