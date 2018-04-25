#
# ACRN-Crashlog Makefile
#

BASEDIR := $(shell pwd)
CC	:= gcc
RM	= rm

CFLAGS	:= -Wall -Wextra -pedantic
CFLAGS	+= -m64 -D_GNU_SOURCE -DDEBUG
INCLUDE := -I $(BASEDIR)/common/include
export INCLUDE

.PHONY:all
all:
	$(MAKE) -C common
	$(MAKE) -C acrnprobe
	$(MAKE) -C usercrash

.PHONY:clean
clean:
	cd acrnprobe && $(MAKE) clean
	cd common && $(MAKE) clean
	cd usercrash && $(MAKE) clean
