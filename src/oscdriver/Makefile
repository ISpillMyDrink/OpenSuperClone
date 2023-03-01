KMOD = oscdriver

KERNELRELEASE ?= $(shell uname -r)

obj-m = $(KMOD).o
MAKE_FLAGS = -C
KDIR = /lib/modules/$(KERNELRELEASE)/build

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
	insmod $(KMOD).ko

uninstall:
	rmmod $(KMOD)
