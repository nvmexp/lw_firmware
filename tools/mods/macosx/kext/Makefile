#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2011 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

CONFIG = Release
TARGET = modskext
OBJROOT ?= build/$(CONFIG)
SYMROOT = $(OBJROOT)/sym
KEXT = modskext.kext
SDK = macosx
DSTROOT ?= .

all:
	@echo "Building modskext..."
	xcodebuild -target $(TARGET) -configuration $(CONFIG) -sdk $(SDK) SYMROOT=$(SYMROOT) OBJROOT=$(OBJROOT)

.PHONY: clean install build_install

build_install: install

clean:
	@echo "Cleaning modskext..."
	xcodebuild -target $(TARGET) -configuration $(CONFIG) -sdk $(SDK) clean SYMROOT=$(SYMROOT) OBJROOT=$(OBJROOT)

install:
	@echo "Building and installing modskext (SYMROOT: $(SYMROOT), DSTROOT: $(DSTROOT))..."
	xcodebuild -target $(TARGET) -configuration $(CONFIG) -sdk $(SDK) install SYMROOT=$(SYMROOT) OBJROOT=$(OBJROOT) DSTROOT=$(DSTROOT)
