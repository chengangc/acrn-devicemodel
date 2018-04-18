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

#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <malloc.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/wait.h>
#include "cmdutils.h"
#include "strutils.h"
#include "log_sys.h"

int execv_outfile(char *argv[], char *outfile)
{
	pid_t pid;

	pid = fork();
	if (pid < 0) {
		LOGE("fork error (%s)\n", strerror(errno));
		return -errno;
	} else if (pid == 0) {
		int res;
		int fd;

		if (outfile) {
			fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0664);
			if (fd < 0) {
				LOGE("open error (%s)\n", strerror(errno));
				return -errno;
			}
			res = dup2(fd, STDOUT_FILENO);
			if (res < 0) {
				close(fd);
				LOGE("dup2 error (%s)\n", strerror(errno));
				return -errno;
			}
		}

		res = execvp(argv[0], argv);
		if (res == -1) {
			LOGE("execvp (%s) failed, error (%s)\n", argv[0],
			     strerror(errno));
			return -errno;
		}
	} else {
		pid_t res;
		int status;
		int exit_status;

		res = waitpid(pid, &status, 0);
		if (res == -1) {
			LOGE("waitpid failed, error (%s)\n", strerror(errno));
			return -errno;
		}

		if (WIFEXITED(status)) {
			exit_status = WEXITSTATUS(status);
			if (!exit_status)
				LOGI("%s exited, status=0\n", argv[0]);
			else
				LOGE("%s exited, status=%d\n", argv[0],
				     exit_status);
			return exit_status;
		} else if (WIFSIGNALED(status)) {
			LOGE("%s killed by signal %d\n", argv[0],
			     WTERMSIG(status));
		} else if (WIFSTOPPED(status)) {
			LOGE("%s stopped by signal %d\n", argv[0],
			     WSTOPSIG(status));
		}

	}

	return -1;
}

int debugfs_cmd(char *loop_dev, char *cmd, char *outfile)
{
	char *argv[5] = {"debugfs", "-R", NULL, NULL, 0};

	argv[2] = cmd;
	argv[3] = loop_dev;
	return  execv_outfile(argv, outfile);
}

int exec_outfile(char *outfile, char *fmt, ...)
{
	va_list args;
	char *cmd;
	char *start;
	char *p;
	int ret;
	int argc;
	char **argv;
	int i = 0;

	va_start(args, fmt);
	ret = vasprintf(&cmd, fmt, args);
	va_end(args);
	if (ret < 0)
		return ret;

	strtrim(cmd);
	argc = strcnt(cmd, ' ') + 1;

	argv = (char **)calloc(argc + 1, sizeof(char *));
	if (argv == NULL) {
		free(cmd);
		LOGE("calloc failed, error (%s)\n", strerror(errno));
		return -1;
	}

	/* string to argv[] */
	start = cmd;
	argv[i++] = start;
	while (start && (p = strchr(start, ' '))) {
		argv[i++] = p + 1;
		*p = 0;
		if (*(p + 1) != '"')
			start = p + 1;
		else
			start = strchr(p + 2, '"');
	}

	ret = execv_outfile(argv, outfile);

	free(argv);
	free(cmd);

	return ret;
}

char *exec_outmem(char *fmt, ...)
{
	va_list args;
	char *cmd;
	FILE *pp;
	char *out = NULL;
	char *new;
	char tmp[1024];
	int presize = 1024;
	int newsize = 0;
	int len = 0;
	int ret;

	va_start(args, fmt);
	ret = vasprintf(&cmd, fmt, args);
	va_end(args);
	if (ret < 0)
		return NULL;

	pp = popen(cmd, "r");
	if (!pp)
		goto free_cmd;

	out = malloc(presize);
	if (!out)
		goto end;

	while (fgets(tmp, sizeof(tmp), pp) != NULL) {
		newsize += strlen(tmp);
		if (newsize + 1 > presize) {
			presize = newsize + 1;
			new = realloc(out, presize);
			if (!new) {
				if (out)
					free(out);
				goto end;
			} else {
				out = new;
			}
		}
		strcpy(out + len, tmp);

		len = strlen(out);
	}

end:
	pclose(pp);
free_cmd:
	free(cmd);

	return out;
}
