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
bindir = ../../bin/

default: clone

clone : opensuperclone.c
	xxd -i opensupertool_help.txt opensupertool_help.h
	xxd -i opensuperclone_help.txt opensuperclone_help.h
	xxd -i opensuperclone$(GTKVER).glade opensuperclone$(GTKVER)_glade.h
	xxd -i opensuperclone_EULA.txt opensuperclone_EULA.h
	xxd -i $(driverdir)opensuperclone_driver.c opensuperclone_driver.h
	$(CC) $(CFLAGS) opensuperclone.c commands.c io.c tool.c usbrelay.c common.c clone_gui$(GTKVER).c clone_gui_language.c -o $(bindir)opensuperclone $(GTKFLAGS) $(CURLFLAGS) $(USBFLAGS) $(SSLFLAGS)

clean:
	rm -f $(bindir)opensuperclone
	rm -f opensupertool_help.h
	rm -f opensuperclone_help.h
	rm -f opensuperclone_glade.h
	rm -f opensuperclone2_glade.h
	rm -f opensuperclone3_glade.h
	rm -f opensuperclone_EULA.h
	rm -f opensuperclone_driver.h