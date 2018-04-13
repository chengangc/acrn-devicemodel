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

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include "usercrashd.h"
#include "packettype.h"
#include "protocol.h"
#include "log_sys.h"

/**
 * @sockfd: the socket fd.
 * set_timeout is used to set timeout for the sockfd, in case client is blocked
 * when client cannot receive the data from server, or send data to server
 */
static int set_timeout(int sockfd)
{
	struct timeval timeout = {50, 0};

	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
				sizeof(timeout)) != 0) {
		LOGE("failed to set receive timeout\n");
		return -1;
	}
	if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout,
				sizeof(timeout)) != 0) {
		LOGE("failed to set send timeout\n");
		return -1;
	}
	return sockfd;
}

/**
 * @usercrashd_socket: client socket fd pointer, get the value from
 * usercrashd_connect function.
 * @output_fd: this fd is created and sent by server, used to store crash file
 * logs in client.
 * usercrashd_connect is used to connect to server, and get the crash file fd
 * from server
 */
bool usercrashd_connect(int pid, int *usercrashd_socket,
		int *output_fd, char *name)
{
	int sockfd;
	int tmp_output_fd;
	ssize_t rc;
	int flags;
	struct crash_packet packet = {0};

	sockfd = socket_local_client(SOCKET_NAME, SOCK_SEQPACKET);
	if (sockfd == -1) {
		LOGE("failed to connect to usercrashd, error (%s)\n",
			strerror(errno));
		return false;
	}
	packet.packet_type = kDumpRequest;
	packet.pid = pid;
	strncpy(packet.name, name, COMM_NAME_LEN - 1);
	if (write(set_timeout(sockfd), &packet, sizeof(packet)) !=
				sizeof(packet)) {
		LOGE("failed to write DumpRequest packet, error (%s)\n",
			strerror(errno));
		goto fail;
	}

	rc = recv_fd(set_timeout(sockfd), &packet, sizeof(packet),
				&tmp_output_fd);
	if (rc == -1) {
		LOGE("failed to read response to DumpRequest packet, ");
		LOGE("error (%s)\n", strerror(errno));
		close(sockfd);
		return false;
	} else if (rc != sizeof(packet)) {
		LOGE("received DumpRequest response packet of incorrect ");
		LOGE("length (expected %lu, got %ld)\n", sizeof(packet), rc);
		goto fail;
	}
	if (packet.packet_type != kPerformDump) {
		LOGE("unexpected dump response:%d\n", packet.packet_type);
		goto fail;
	}

	/* Make the fd O_APPEND so that our output is guaranteed to be at the
	 * end of a file. (This also makes selinux rules consistent, because
	 * selinux distinguishes between writing to a regular fd, and writing
	 * to an fd with O_APPEND)
	 */
	flags = fcntl(tmp_output_fd, F_GETFL);
	if (fcntl(tmp_output_fd, F_SETFL, flags | O_APPEND) != 0) {
		LOGE("failed to set output fd flags, error (%s)\n",
					strerror(errno));
		goto fail;
	}

	*usercrashd_socket = sockfd;
	*output_fd = tmp_output_fd;

	return true;
fail:
	close(sockfd);
	close(tmp_output_fd);
	return false;

}

/**
 * @usercrashd_socket: client socket fd, used to communicate with server.
 * usercrashd_notify_completion is used to tell the server it has done the dump,
 * the server will pop another crash from the queue and execute the dump process
 */
bool usercrashd_notify_completion(int usercrashd_socket)
{
	struct crash_packet packet = {0};

	packet.packet_type = kCompletedDump;
	if (write(set_timeout(usercrashd_socket), &packet,
				sizeof(packet)) != sizeof(packet)) {
		return false;
	}
	return true;
}
