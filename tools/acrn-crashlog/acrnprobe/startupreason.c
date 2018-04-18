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

#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>
#include "fsutils.h"
#include "startupreason.h"
#include "log_sys.h"

#define MAX_KERNEL_COMMAND_LINE_SIZE 4096
#define CURRENT_KERNEL_CMDLINE "/proc/cmdline"

static int get_cmdline_bootreason(char *bootreason)
{
	int res, len = MAX_KERNEL_COMMAND_LINE_SIZE;
	char *p, *p1, *p2;
	char *cmdline;
	const char key[] = "bootreason=";

	cmdline = malloc(len);
	if (!cmdline) {
		LOGE("failed to allocate memory to read %s\n",
		     CURRENT_KERNEL_CMDLINE);
		return -1;
	}
	res = file_read_string(CURRENT_KERNEL_CMDLINE, cmdline);
	if (res <= 0) {
		LOGE("failed to read file %s - %s\n",
		     CURRENT_KERNEL_CMDLINE, strerror(errno));
		free(cmdline);
		return -1;
	}

	p = strstr(cmdline, key);
	if (!p) {
		free(cmdline);
		return 0;
	}
	p += strlen(key);
	p1 = strstr(p, " ");
	p2 = strstr(p, "\n");
	if (p2 && !p1)
		*p2 = '\0';
	else if (p2 && p2 < p1)
		*p2 = '\0';
	else if (p1)
		*p1 = '\0';

	strncpy(bootreason, p, strlen(p) + 1);
	free(cmdline);
	return strlen(bootreason);
}

static void get_default_bootreason(char *bootreason)
{
	int ret;
	unsigned int i;
	char bootreason_prop[MAX_KERNEL_COMMAND_LINE_SIZE];

	ret = get_cmdline_bootreason(bootreason_prop);
	if (ret <= 0)
		return;

	for (i = 0; i < strlen(bootreason_prop); i++)
		bootreason[i] = toupper(bootreason_prop[i]);
	bootreason[i] = '\0';

}

void read_startupreason(char *startupreason)
{
	strcpy(startupreason, "UNKNOWN");
	get_default_bootreason(startupreason);
}
