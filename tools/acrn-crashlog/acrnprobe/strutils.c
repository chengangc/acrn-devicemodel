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

#include <string.h>
#include <errno.h>

/**
 * Get the length of line.
 *
 * @param str Start address of line.
 *
 * @return the length of line if successful, or -1 if not.
 *	   This function return length of string if string doesn't contain \n.
 */
int strlinelen(char *str)
{
	char *tag;

	if (!str)
		return -1;

	tag = strchr(str, '\n');
	if (tag)
		return tag - str + 1;

	return strlen(str);
}

/**
 * Find the last occurrence of the substring str in the string s.
 *
 * @param s Range of search.
 * @param substr String to be found.
 *
 * @return a pointer to the beginning of the substring,
 *	   or NULL if the substring is not found.
 */
char *strrstr(char *s, char *substr)
{
	char *p;
	int len = strlen(s);

	for (p = s + len - 1; p >= s; p--) {
		if ((*p == *substr) &&
		    (memcmp(p, substr, strlen(substr)) == 0))
			return p;
	}

	return NULL;
}

char *next_line(char *buf)
{
	char *p;
	int len = strlen(buf);

	if (len <= 1)
		return NULL;
	for (p = buf; p < buf + len; p++) {
		if (*p == '\n') {
			if (p >= buf + len - 1)
				return NULL;
			else
				return p + 1;
		}
	}

	return NULL;
}

void strtriml(char *str)
{
	char *p = str;

	while (*p == ' ')
		p++;
	memmove(str, p, strlen(p) + 1);
}

void strtrimr(char *str)
{
	char *end = str + strlen(str) - 1;

	while (*end == ' ' && end >= str) {
		*end = 0;
		end--;
	}
}

void strtrim(char *str)
{
	strtriml(str);
	strtrimr(str);
}

int strcnt(char *str, char c)
{
	int cnt = 0;

	if (!str)
		return -EINVAL;
	while (*str) {
		if (*str == c)
			cnt++;
		str++;
	}

	return cnt;
}
