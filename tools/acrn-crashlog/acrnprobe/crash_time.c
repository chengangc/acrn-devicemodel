/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
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

#include <stdio.h>
#include <time.h>
#include "crash_time.h"

unsigned long long get_uptime(void)
{
	 static long long time_ns = -1;
	 struct timespec ts;

	 clock_gettime(CLOCK_BOOTTIME, &ts);
	 time_ns = (long long)ts.tv_sec * 1000000000LL +
		   (long long)ts.tv_nsec;

	return time_ns;
}

int get_uptime_string(char newuptime[24], int *hours)
{
	long long tm;
	int seconds, minutes;

	tm = get_uptime();

	*hours = (int)(tm / 1000000000LL);
	seconds = *hours % 60; *hours /= 60;
	minutes = *hours % 60; *hours /= 60;

	return snprintf(newuptime, 24, "%04d:%02d:%02d", *hours,
			minutes, seconds);
}

int get_current_time_long(char buf[32])
{
	time_t t;
	struct tm *time_val;

	time(&t);
	time_val = localtime((const time_t *)&t);

	strftime(buf, 32, "%Y-%m-%d/%H:%M:%S  ", time_val);

	return 0;
}
