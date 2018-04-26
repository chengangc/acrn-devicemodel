BASEDIR 	:= $(shell pwd)

LIBS		= -lpthread -lxml2 -lcrypto -lrt -lsystemd -ltelemetry
INCLUDE    	+= -I $(BASEDIR)/include -I /usr/include/libxml2
CFLAGS 		+= $(INCLUDE)
CFLAGS 		+= -g -O0 -std=gnu11
CFLAGS 		+= -ffunction-sections -fdata-sections

LDFLAGS 	+= $(LIBS) -Wl,--gc-sections

TARGET		= bin/acrnprobe

all: check_dirs $(TARGET)

obj/%.o:%.c
	$(CC) -c $(CFLAGS) $< -o $@

bin/acrnprobe: obj/main.o \
	$(BASEDIR)/../common/obj/log_sys.o \
	obj/load_conf.o \
	obj/channels.o \
	obj/event.o \
	obj/event_handler.o \
	obj/crash_reclassify.o \
	obj/sender.o \
	obj/strutils.o \
	obj/cmdutils.o \
	obj/fsutils.o \
	obj/crash_time.o \
	obj/startupreason.o \
	obj/property.o \
	obj/crashutils.o \
	obj/history.o \
	obj/vm_history.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	@echo "Clean objects and binaries"
	@if [ -d obj ]; then \
	    find obj -name "*.o" -exec $(RM) {} \; 2>&1 || exit 0; \
	fi
	@if [ -d bin ]; then \
		$(RM) -r bin ; \
	fi
	@if [ -d obj ]; then \
		$(RM) -r obj ; \
	fi

check_dirs:
	@if [ ! -d bin ]; then \
		mkdir bin ; \
	fi
	@if [ ! -d obj ]; then \
		mkdir obj ; \
	fi

.PHONY:install
install:
	@install -d $(DESTDIR)/usr/bin/
	@install -p -D -m 0755 bin/acrnprobe $(DESTDIR)/usr/bin/
	@install -p -D -m 0755 acrnprobe_prepare.sh $(DESTDIR)/usr/bin/
	@install -d $(DESTDIR)/usr/share/defaults/telemetrics/
	@install -p -D -m 0755 acrnprobe.xml $(DESTDIR)/usr/share/defaults/telemetrics/
	@install -d $(DESTDIR)/usr/lib/systemd/system/
	@install -p -D -m 0755 acrnprobe.service $(DESTDIR)/usr/lib/systemd/system/
	@install -p -D -m 0755 prepare.service $(DESTDIR)/usr/lib/systemd/system/

.PHONY:uninstall
uninstall:
	@if [ -e "$(DESTDIR)/usr/bin/acrnprobe" ];then \
		$(RM) $(DESTDIR)/usr/bin/acrnprobe; \
	fi
	@if [ -e "$(DESTDIR)/etc/acrnprobe.xml" ];then \
		$(RM) $(DESTDIR)/etc/acrnprobe.xml; \
	fi
	@if [ -e "$(DESTDIR)/usr/lib/systemd/system/acrnprobe.service" ];then \
		$(RM) $(DESTDIR)/usr/lib/systemd/system/acrnprobe.service; \
	fi
	@if [ -e "$(DESTDIR)/usr/lib/systemd/system/prepare.service" ];then \
		$(RM) $(DESTDIR)/usr/lib/systemd/system/prepare.service; \
	fi
	@if [ -e "$(DESTDIR)/usr/local/bin/acrnprobe_prepare.sh" ];then \
		$(RM) $(DESTDIR)/usr/local/bin/acrnprobe_prepare.sh; \
	fi
