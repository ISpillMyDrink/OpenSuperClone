
# add -g to CFLAGS for extra debugging
# use -k command line option to enable debugging

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

driverdir = driver/
bindir = ../bin/

all:  hddsuperclone hddscviewer
clone: hddsuperclone
viewer: hddscviewer

create_script_help : create_script_help.c
	$(CC) $(CFLAGS) create_script_help.c -o $(bindir)create_script_help

hddsuperclone : hddsuperclone.c
	xxd -i hddsupertool_help.txt hddsupertool_help.h
	xxd -i hddsuperclone_help.txt hddsuperclone_help.h
	xxd -i hddsuperclone$(GTKVER).glade hddsuperclone$(GTKVER)_glade.h
	xxd -i hddsuperclone_EULA.txt hddsuperclone_EULA.h
	xxd -i $(driverdir)hddsuperclone_driver.c hddsuperclone_driver.h
	$(CC) $(CFLAGS) hddsuperclone.c commands.c io.c tool.c usbrelay.c common.c clone_gui$(GTKVER).c clone_gui_language.c -o $(bindir)hddsuperclone $(GTKFLAGS) $(CURLFLAGS) $(USBFLAGS) $(SSLFLAGS)

hddscviewer : hddscviewer$(GTKVER).c
	xxd -i hddscviewer$(GTKVER).glade hddscviewer$(GTKVER)_glade.h
	$(CC) $(CFLAGS) hddscviewer$(GTKVER).c -o $(bindir)hddscviewer $(GTKFLAGS) $(CURLFLAGS)

clean:
	rm -f $(bindir)hddsupertool
	rm -f hddsupertool_help.h
	rm -f hddsuperclone_help.h
	rm -f hddsuperclone_glade.h
	rm -f hddsuperclone2_glade.h
	rm -f hddsuperclone3_glade.h
	rm -f hddsuperclone_EULA.h
	rm -f hddsuperclone_driver.h
	rm -f $(bindir)hddscviewer
	rm -f hddscviewer_help.h
	rm -f hddscviewer2_glade.h
	rm -f hddscviewer3_glade.h
	rm -f $(bindir)create_script_help