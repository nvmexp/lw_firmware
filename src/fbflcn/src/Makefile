#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2010-2011 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# This is just a wrapper for makefile.lwmk so that running 'make' on the FBFALCON
# source "just-works".
#

.PHONY: all build rel release debug install clean clobber dummy

#
# Alias all real targets to a single dummy target that does nothing but
# redirect to the lwmake makefile.
#

all:     dummy
build:   dummy
rel:     dummy
release: dummy
debug:   dummy
install: dummy
clean:   dummy
clobber: dummy

LWMAKE ?= lwmake

dummy:
	$(LWMAKE) $(MAKECMDGOALS)


vbios.o: ./vbios.bin
	$(FALCONOBJCOPY)  --rename-section .data=.vbios  -I binary -B falcon -O elf32-falcon vbios.bin vbios.o


