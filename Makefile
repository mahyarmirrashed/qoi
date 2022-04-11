CC      := gcc
CFLAGS  := -Wall -Wpedantic -Wextra -Wextra -DNDEBUG
RM      := rm -rf

.PHONY: main

main: main.c qoi.o
	@echo Generating: $@
	$(CC) $(CFLAGS) main.c qoi.o -o $@

qoi.o: qoi.c qoi.h
	@echo Generating: $@
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) main qoi.o
