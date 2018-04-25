objects = log_sys.o

all: check_obj $(objects)
check_obj:
	@if [ ! -d obj ]; then \
		mkdir -p obj/; \
	fi

$(objects):%.o:%.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o obj/$@

.PHONY:clean
clean:
	@echo "Clean objects and binaries"
	@if [ -d obj ]; then \
		find obj -name "*.o" -exec $(RM) {} \; 2>&1 || exit 0; \
	fi
	@if [ -d obj ]; then \
		$(RM) -r obj ; \
	fi
