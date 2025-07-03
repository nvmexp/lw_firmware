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
# pmu_sw/build/Makefile
#
# Run PMU mcheck and other source level checks in DVS
#

LW_SOURCE ?= $(shell cd ../..; pwd)

CHECKPMU_PERL          ?= $(if $(findstring undefined,$(origin PERL)), perl, $(PERL))
CHECKPMU_ROOT          ?= $(LW_SOURCE)/pmu_sw

# run by AS2
check_pmu:
	rm -f check_pmu.succeeded
	$(CHECKPMU_PERL) $(CHECKPMU_ROOT)/build/mcheck/mcheck.pl --profile
	touch check_pmu.succeeded

# run by DVS (Disable snapshots has that doesn't work on DVS machines)
check_pmu_dvs:
	rm -f check_pmu.succeeded
	$(CHECKPMU_PERL) $(CHECKPMU_ROOT)/build/mcheck/mcheck.pl --profile --no-snapshots
	touch check_pmu.succeeded

# test targets
.PHONY: check_pmu
