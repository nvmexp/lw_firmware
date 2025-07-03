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
# uproc/soe/build/Makefile
#
# Run SOE mcheck and other source level checks in DVS
#

LW_SOURCE ?= $(shell cd ../../..; pwd)

CHECKSOE_PERL          ?= $(if $(findstring undefined,$(origin PERL)), perl, $(PERL))
CHECKSOE_ROOT          ?= $(LW_SOURCE)/uproc/soe

# run by AS2
check_soe:
	rm -f check_soe.succeeded
	$(CHECKSOE_PERL) $(CHECKSOE_ROOT)/build/mcheck/mcheck.pl --profile
	touch check_soe.succeeded

# run by DVS (Disable snapshots has that doesn't work on DVS machines)
check_soe_dvs:
	rm -f check_soe.succeeded
	$(CHECKSOE_PERL) $(CHECKSOE_ROOT)/build/mcheck/mcheck.pl --profile --no-snapshots
	touch check_soe.succeeded

# test targets
.PHONY: check_soe
