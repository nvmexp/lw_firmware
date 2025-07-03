#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2021 by LWPU Corporation.  All rights reserved.  All
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

CHECKSOERISCV_PERL          ?= $(if $(findstring undefined,$(origin PERL)), perl, $(PERL))
CHECKSOERISCV_ROOT          ?= $(LW_SOURCE)/uproc/soe_riscv

# run by AS2
check_soe_riscv:
	rm -f check_soe_riscv.succeeded
	$(CHECKSOERISCV_PERL) $(CHECKSOERISCV_ROOT)/build/mcheck/mcheck.pl --profile
	touch check_soe_riscv.succeeded

# run by DVS (Disable snapshots has that doesn't work on DVS machines)
check_soe_riscv_dvs:
	rm -f check_soe_riscv.succeeded
	$(CHECKSOERISCV_PERL) $(CHECKSOERISCV_ROOT)/build/mcheck/mcheck.pl --profile --no-snapshots
	touch check_soe_riscv.succeeded

# test targets
.PHONY: check_soe_riscv
