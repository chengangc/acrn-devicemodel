all: check_obj debugger usercrashd

CURRDIR := $(shell pwd)
INCLUDE += -I $(CURRDIR)/include/

LIBS = -levent -lpthread

usercrashd: obj/protocol.o obj/usercrashd.o $(CURRDIR)/../common/obj/log_sys.o
	$(CC) -g $(CFLAGS) $(LIBS) $(INCLUDE) $^ -o obj/bin/$@ -lsystemd

debugger: obj/protocol.o obj/client.o obj/crash_dump.o obj/debugger.o \
		$(CURRDIR)/../common/obj/log_sys.o
	$(CC) -g $(CFLAGS) $(INCLUDE) $^ -o obj/bin/$@ -lsystemd

obj/%.o:%.c
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ -c $<


check_obj:
	@if [ ! -d obj/bin ]; then \
		mkdir -p obj/bin; \
	fi

.PHONY:clean
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

.PHONY:install
install:
	@install -d $(DESTDIR)/usr/bin/
	@install -p -D -m 0755 obj/bin/debugger $(DESTDIR)/usr/bin/
	@install -p -D -m 0755 obj/bin/usercrashd $(DESTDIR)/usr/bin/
	@install -d $(DESTDIR)/usr/lib/systemd/system/
	@install -p -D -m 0755 usercrash.service $(DESTDIR)/usr/lib/systemd/system/

.PHONY:uninstall
uninstall:
	@if [ -e "$(DESTDIR)/usr/bin/debugger" ];then \
		$(RM) $(DESTDIR)/usr/bin/debugger; \
	fi
	@if [ -e "$(DESTDIR)/usr/bin/usercrashd" ];then \
		$(RM) $(DESTDIR)/usr/bin/usercrashd; \
	fi
	@if [ -e "$(DESTDIR)/usr/lib/systemd/system/usercrash.service" ];then \
		$(RM) $(DESTDIR)/usr/lib/systemd/system/usercrash.service; \
	fi
