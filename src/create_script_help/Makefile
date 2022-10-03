CC = gcc
ifneq (,$(findstring k,$(MAKEFLAGS)))
CFLAGS = -Wall -W -O0 -g -fcommon
else
CFLAGS = -Wall -W -Wno-deprecated-declarations -fcommon
endif

bindir = ../../bin/

default: create_script_help

create_script_help : create_script_help.c
	$(CC) $(CFLAGS) create_script_help.c -o $(bindir)create_script_help

clean:
	rm -f $(bindir)/create_script_help