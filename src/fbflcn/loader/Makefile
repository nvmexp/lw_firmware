#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2011 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# This is just a wrapper for makefile.lwmk so that running 'make' on the loader
# program "just-works". See comments at the top of makefile.lwmk for additional
# information on the build.
#
        
.SILENT:

.PHONY: all build clean clobber

all: build

# redirect everything to lwmake (uses makefile.lwmk)
build:
	lwmake build

clean:
	lwmake clean

clobber:
	lwmake clobber

