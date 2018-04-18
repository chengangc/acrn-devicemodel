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
#include <stdarg.h>
#include <string.h>
#include <systemd/sd-journal.h>

#ifndef __LOG_SYS_H__
#define __LOG_SYS_H__

#define MAX_LOG_LEN 1024
#define LOG_LEVEL LOG_WARNING

static inline void do_log(int level,
#ifdef DEBUG
			const char *func, int line,
#endif
			...)
{
	va_list args;
	char *fmt;
	char log[MAX_LOG_LEN] = {0};
#ifdef DEBUG
	char header_fmt[] = "<%-20s%d>: ";
#endif

	if (level > LOG_LEVEL)
		return;

#ifdef DEBUG
	va_start(args, line);
#else
	va_start(args, level);
#endif
	fmt = va_arg(args, char *);
	if (!fmt)
		return;
#ifdef DEBUG
	/* header */
	snprintf(log, sizeof(log) - 1, header_fmt, func, line);
#endif
	/* msg */
	vsnprintf(log + strlen(log), sizeof(log) - strlen(log) - 1, fmt, args);
	va_end(args);

	sd_journal_print(level, log);
}

#ifdef DEBUG
#define LOGE(...) \
		do_log(LOG_ERR, __func__, __LINE__,  __VA_ARGS__)

#define LOGW(...) \
		do_log(LOG_WARNING, __func__, __LINE__, __VA_ARGS__)

#define LOGI(...) \
		do_log(LOG_INFO, __func__, __LINE__, __VA_ARGS__)

#define LOGD(...) \
		do_log(LOG_DEBUG, __func__, __LINE__, __VA_ARGS__)
#else
#define LOGE(...) \
		do_log(LOG_ERR, __VA_ARGS__)

#define LOGW(...) \
		do_log(LOG_WARNING, __VA_ARGS__)

#define LOGI(...) \
		do_log(LOG_INFO, __VA_ARGS__)

#define LOGD(...) \
		do_log(LOG_DEBUG, __VA_ARGS__)
#endif

#endif
