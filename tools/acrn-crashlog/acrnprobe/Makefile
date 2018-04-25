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
	obj/fsutils.o
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
