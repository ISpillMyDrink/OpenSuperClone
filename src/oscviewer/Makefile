GTKVER = 2

CC = gcc
ifneq (,$(findstring k,$(MAKEFLAGS)))
CFLAGS = -Wall -W -O0 -g -fcommon
else
CFLAGS = -Wall -W -Wno-deprecated-declarations -fcommon
endif
SSLFLAGS = -lssl -lcrypto
USBFLAGS = -lusb
CURLFLAGS = -lcurl
GTKFLAGS = `pkg-config --cflags --libs gtk+-$(GTKVER).0` -export-dynamic

bindir = ../../bin/

default: oscviewer

oscviewer : oscviewer$(GTKVER).c
	xxd -i oscviewer$(GTKVER).glade oscviewer$(GTKVER)_glade.h
	$(CC) $(CFLAGS) oscviewer$(GTKVER).c -o $(bindir)oscviewer $(GTKFLAGS) $(CURLFLAGS)

clean:
	rm -f $(bindir)oscviewer
	rm -f oscviewer_help.h
	rm -f oscviewer2_glade.h
	rm -f oscviewer3_glade.h