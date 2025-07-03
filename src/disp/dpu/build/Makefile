#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2016 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#
# uproc/disp/dpu/build/Makefile
#
# Run DPU mcheck and other source level checks in DVS
#

LW_SOURCE ?= $(shell cd ../../../..; pwd)

CHECKDPU_PERL          ?= $(if $(findstring undefined,$(origin PERL)), perl, $(PERL))
CHECKDPU_ROOT          ?= $(LW_SOURCE)/uproc/disp/dpu

check_dpu:
	rm -f check_dpu.succeeded
	$(CHECKDPU_PERL) $(CHECKDPU_ROOT)/build/mcheck/mcheck.pl --profile
	touch check_dpu.succeeded

# test targets
.PHONY: check_dpu
