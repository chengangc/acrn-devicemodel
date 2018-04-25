BASEDIR 	:= $(shell pwd)

LIBS		= -lpthread -lxml2 -lcrypto -lrt -lsystemd -ltelemetry
CFLAGS 		+= $(INCLUDE)
CFLAGS 		+= -g -O0 -std=gnu11
CFLAGS 		+= -ffunction-sections -fdata-sections

LDFLAGS 	+= $(LIBS) -Wl,--gc-sections

TARGET		= bin/acrnprobe

all: check_dirs $(TARGET)

obj/%.o:%.c
	$(CC) -c $(CFLAGS) $< -o $@

bin/acrnprobe: obj/main.o
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
