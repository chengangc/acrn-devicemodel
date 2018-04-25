all: check_obj debugger usercrashd

LIBS = -levent -lpthread

usercrashd: obj/usercrashd.o
	$(CC) -g $(CFLAGS) $(LIBS) $(INCLUDE) $^ -o obj/bin/$@ -lsystemd

debugger: obj/debugger.o
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
