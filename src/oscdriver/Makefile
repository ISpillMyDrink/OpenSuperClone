KMOD = oscdriver

KVERSION = $(shell uname -r)

obj-m = $(KMOD).o
MAKE_FLAGS = -C
KDIR = /lib/modules/$(KVERSION)/build

IOCTL = oscdriverc
MMAP_M = oscdrivermap_m
MMAP_TB = oscdrivermap_tb
MMAP_MDB = oscdrivermap_mdb

RMFLAGS = -vR

TMPS =  $(KMOD).mod \
        $(KMOD).mod.*  \
       .$(KMOD).*.cmd  \
        built-in.o     \
       .built-in.*     \
        modules.order  \
        Module.symvers \
       .tmp_versions

default:
	$(MAKE) $(MAKE_FLAGS) $(KDIR) M=$$PWD

.PHONY: all clean

all: default

clean:
	$(RM) $(RMFLAGS) $(KMOD).ko $(KMOD).o $(TMPS)

install:
	insmod $(KMOD).ko ioctl=$(IOCTL) mmap_m=$(MMAP_M) mmap_tb=$(MMAP_TB) mmap_mdb=$(MMAP_MDB)

uninstall:
	rmmod $(KMOD)
