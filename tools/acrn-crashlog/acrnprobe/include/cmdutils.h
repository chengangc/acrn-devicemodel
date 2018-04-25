/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

int execv_outfile(char *argv[], char *outfile);
int debugfs_cmd(char *loop_dev, char *cmd, char *outfile);
int exec_outfile(char *outfile, char *fmt, ...);
char *exec_outmem(char *fmt, ...);
