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
# uproc/sec2/build/Makefile
#
# Run SEC2 mcheck and other source level checks in DVS
#

LW_SOURCE ?= $(shell cd ../../..; pwd)

CHECKSEC2_PERL          ?= $(if $(findstring undefined,$(origin PERL)), perl, $(PERL))
CHECKSEC2_ROOT          ?= $(LW_SOURCE)/uproc/sec2

# run by AS2
check_sec2:
	rm -f check_sec2.succeeded
	$(CHECKSEC2_PERL) $(CHECKSEC2_ROOT)/build/mcheck/mcheck.pl --profile
	touch check_sec2.succeeded

# run by DVS (Disable snapshots has that doesn't work on DVS machines)
check_sec2_dvs:
	rm -f check_sec2.succeeded
	$(CHECKSEC2_PERL) $(CHECKSEC2_ROOT)/build/mcheck/mcheck.pl --profile --no-snapshots
	touch check_sec2.succeeded

# test targets
.PHONY: check_sec2
