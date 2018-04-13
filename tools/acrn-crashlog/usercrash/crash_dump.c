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
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include "log_sys.h"

#define GET_TID "/bin/cat /proc/%d/status | /bin/grep -w Tgid | "\
	"awk -F ':' '{print $2}' | sed 's/\t//g'"
#define GET_COMM "/bin/ls -l /proc/%d/exe | awk -F '->' '{print $2}' | "\
	"sed 's/ //g'"
#define GET_K_STACK "/bin/cat /proc/%d/stack"
#define GET_MAPS "/bin/cat /proc/%d/maps"
#define GET_OPEN_FILES "/bin/ls -l /proc/%d/fd | awk -F ' ' '{if(NR>1) "\
	"{print $9 $10 $11}}'"
#define GET_GDB_INFO "/usr/bin/gdb %s -batch "\
			"-ex 'bt' "\
			"-ex 'printf \"\nRegisters\n\"' "\
			"-ex 'info registers' "\
			"-ex 'printf \"\n\nMemory near rax\n\"' "\
			"-ex 'x/16x $rax-0x20' "\
			"-ex 'x/48x $rax' "\
			"-ex 'printf \"\n\nMemory near rbx\n\"' "\
			"-ex 'x/16x $rbx-0x20' "\
			"-ex 'x/48x $rbx' "\
			"-ex 'printf \"\n\nMemory near rcx\n\"' "\
			"-ex 'x/16x $rcx-0x20' "\
			"-ex 'x/48x $rcx' "\
			"-ex 'printf \"\n\nMemory near rdx\n\"' "\
			"-ex 'x/16x $rdx-0x20' "\
			"-ex 'x/48x $rdx' "\
			"-ex 'printf \"\n\nMemory near rsi\n\"' "\
			"-ex 'x/16x $rsi-0x20' "\
			"-ex 'x/48x $rsi' "\
			"-ex 'printf \"\n\nMemory near rdi\n\"' "\
			"-ex 'x/16x $rdi-0x20' "\
			"-ex 'x/48x $rdi' "\
			"-ex 'printf \"\n\nMemory near rbp\n\"' "\
			"-ex 'x/16x $rbp-0x20' "\
			"-ex 'x/48x $rbp' "\
			"-ex 'printf \"\n\nMemory near rsp\n\"' "\
			"-ex 'x/16x $rsp-0x20' "\
			"-ex 'x/48x $rsp' "\
			"-ex 'printf \"\n\ncode around rip\n\"' "\
			"-ex 'x/8i $rip-0x20' "\
			"-ex 'x/48i $rip' "\
			"-ex 'printf \"\nThreads\n\n\"' "\
			"-ex 'info threads' "\
			"-ex 'printf \"\nThreads backtrace\n\n\"' "\
			"-ex 'thread apply all bt' "\
			"-ex 'quit'"

#define strcat_fmt(buf, fmt, ...) \
(__extension__ \
({ \
	char __buf[2048]; \
	snprintf(__buf, sizeof(__buf), fmt, ##__VA_ARGS__); \
	strcat(buf, __buf); \
}) \
)

#define DEBUGGER_SIGNAL (__SIGRTMIN + 3)
#define DUMP_FILE "/tmp/core"
#define BUFFER_SIZE 8196

char commands[2048];
char param[2048];
int sig;

static int system_status(const char *command)
{
	pid_t status;

	status = system(command);
	if (status == -1) {
		LOGE("system error\n");
		goto fail;
	} else {
		if (WIFEXITED(status)) {
			if (WEXITSTATUS(status) != 0) {
				LOGE("Script failed, exit code: %d\n",
					WEXITSTATUS(status));
				goto fail;
			}
		}
	}
	return 0;
fail:
	return -1;
}

static void _LOG(int fd, const char *fmt, ...)
{
	char buf[512];
	va_list ap;
	size_t len;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	len = strlen(buf);
	if (len <= 0)
		return;

	write(fd, buf, len);
}

static int cmd_system(const char *command, char *comm)
{
	char buf[1024];
	FILE *fpRead;

	fpRead = popen(command, "r");
	if (!fpRead) {
		LOGE("popen error\n");
		return -1;
	}
	memset(buf, '\0', sizeof(buf));
	if (fgets(buf, sizeof(buf), fpRead) != NULL) {
		strcpy(comm, buf);
	} else {
		LOGE("get commds is empty\n");
		pclose(fpRead);
		return -1;
	}

	pclose(fpRead);

	return 0;
}

static const char *get_signame(int sig)
{
	switch (sig) {
	case SIGABRT:
		return "SIGABRT";
	case SIGBUS:
		return "SIGBUS";
	case SIGFPE:
		return "SIGFPE";
	case SIGILL:
		return "SIGILL";
	case SIGSEGV:
		return "SIGSEGV";
#if defined(SIGSTKFLT)
	case SIGSTKFLT:
		return "SIGSTKFLT";
#endif
	case SIGSTOP:
		return "SIGSTOP";
	case SIGSYS:
		return "SIGSYS";
	case SIGTRAP:
		return "SIGTRAP";
	case DEBUGGER_SIGNAL:
		return "<debuggerd signal>";
	default:
		return "?";
	}
}

static int saveDump(const char *filename)
{
	char input_buffer[BUFFER_SIZE];
	/* open dump file in write and binary mode */
	FILE *dump_file = fopen(filename, "wb");

	if (dump_file != NULL) {
		size_t read;

		/* Read from STDIN and write to dump file */
		while (1) {
			read = fread(input_buffer, 1, BUFFER_SIZE, stdin);
			if (read > 0) {
				if (fwrite(input_buffer, 1, read, dump_file) <=
							0) {
					LOGE("fwrite error\n");
					goto fail;
				}
				continue;
			} else if (read == 0) {
				break;
			}

			LOGE("fread error\n");
			goto fail;
		}
		fclose(dump_file);
		return 0;
	}

	LOGE("fopen core file failed\n");
	return -1;

fail:
	fclose(dump_file);
	return -1;
}

static int log_dump(int pid, int tgid, char *comm, int sig, int out_fd)
{
	_LOG(out_fd, "*** *** *** *** *** *** *** *** *** *** *** *** *** ");
	_LOG(out_fd, "*** *** ***\n");
	_LOG(out_fd, "pid: %d, tgid: %d, comm: %s\n\n\n", pid, tgid, comm);
	_LOG(out_fd, "signal: %d (%s)\n", sig, get_signame(sig));
	_LOG(out_fd, "Stack:\n\n");

	memset(commands, 0, sizeof(commands));
	strcat_fmt(commands, GET_K_STACK, pid);
	system_status(commands);

	_LOG(out_fd, "\nMaps:\n\n");
	memset(commands, 0, sizeof(commands));
	strcat_fmt(commands, GET_MAPS, pid);
	system_status(commands);

	_LOG(out_fd, "\nOpen files:\n\n");
	memset(commands, 0, sizeof(commands));
	strcat_fmt(commands, GET_OPEN_FILES, pid);
	system_status(commands);

	_LOG(out_fd, "\nBackTrace:\n\n");
	memset(commands, 0, sizeof(commands));
	if (sig == DEBUGGER_SIGNAL) {
		snprintf(param, sizeof(param), "-p %d", pid);
	} else {
		snprintf(param, sizeof(param), "%s %s", comm, DUMP_FILE);
		if (saveDump(DUMP_FILE) == -1) {
			LOGE("save core file failed\n");
			return -1;
		}
	}
	strcat_fmt(commands, GET_GDB_INFO, param);
	system_status(commands);

	return 0;
}

void crash_dump(int pid, int sig, int out_fd)
{
	char comm[1024];
	int o_fd;
	int tgid;
	char result[16];

	memset(commands, 0, sizeof(commands));
	memset(comm, 0, sizeof(comm));
	strcat_fmt(commands, GET_COMM, pid);
	if (cmd_system(commands, comm)) {
		LOGE("cmd_system execute error\n");
		return;
	}
	comm[strlen(comm)-1] = '\0';

	memset(commands, 0, sizeof(commands));
	strcat_fmt(commands, GET_TID, pid);
	if (cmd_system(commands, result)) {
		LOGE("cmd_system execute error\n");
		return;
	}
	tgid = atoi(result);
	if (!sig) {
		sig = DEBUGGER_SIGNAL;
		if (log_dump(pid, tgid, comm, sig, out_fd)) {
			LOGE("dump log file failed\n");
			return;
		}
	} else {
		o_fd = dup(STDOUT_FILENO);
		if (-1 == dup2(out_fd, STDOUT_FILENO)) {
			LOGE("Can't redirect fd error!!!\n");
			return;
		}
		if (log_dump(pid, tgid, comm, sig, out_fd))
			LOGE("dump log file failed\n");
		if (-1 == dup2(o_fd, STDOUT_FILENO)) {
			LOGE("Recover fd error.\n");
			return;
		}
	}
}
