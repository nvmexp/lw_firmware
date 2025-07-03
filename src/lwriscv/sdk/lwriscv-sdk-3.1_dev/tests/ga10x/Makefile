ROOT=$(LWRDIR)
include config.mk

SUBDIRS:=loader testapp

all: install
upload: install
	scp $(INSTALL_DIR)/* xterm:scratch2/ga10x-loader-test

SUBDIRS_B:=$(addsuffix .build, $(SUBDIRS))
SUBDIRS_C:= $(addsuffix .clean, $(SUBDIRS))
SUBDIRS_I:= $(addsuffix .install, $(SUBDIRS))

build: $(SUBDIRS_B)
clean: $(SUBDIRS_C)
install: $(SUBDIRS_I)

.PHONY: all upload build install clean

%.build:
	$(MAKE) -C $* build

%.clean:
	$(MAKE) -C $* clean

%.install: %.build
	$(MAKE) -C $* install
