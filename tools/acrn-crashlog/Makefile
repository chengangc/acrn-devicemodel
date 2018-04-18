#
# ACRN-Crashlog Makefile
#

MAJOR_VERSION=0
MINOR_VERSION=1
RC_VERSION=1

CC	:= gcc
RM	= rm

CFLAGS	:= -Wall -Wextra -pedantic
CFLAGS	+= -m64 -D_GNU_SOURCE -DDEBUG

.PHONY:all
all:
	$(MAKE) -C acrnprobe
	$(MAKE) -C usercrash

.PHONY:clean
clean:
	cd acrnprobe && $(MAKE) clean
	cd usercrash && $(MAKE) clean

.PHONY:install
install:
	cd acrnprobe && $(MAKE) install
	cd usercrash && $(MAKE) install

.PHONY:uninstall
uninstall:
	cd acrnprobe && $(MAKE) uninstall
	cd usercrash && $(MAKE) uninstall
