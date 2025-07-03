#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2018 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#
# uproc/acr/build/Makefile
#
# Run FUB mcheck and other source level checks in DVS
#

LW_SOURCE ?= $(shell cd ../../..; pwd)

CHECKFUB_PERL          ?= $(if $(findstring undefined,$(origin PERL)), perl, $(PERL))
CHECKFUB_ROOT          ?= $(LW_SOURCE)/uproc/fub

check_fub:
	$(RM) -f mcheck_fub.succeeded
	$(CHECKFUB_PERL) $(CHECKFUB_ROOT)/build/mcheck/mcheck.pl --profile
	touch mcheck_fub.succeeded

# test targets
.PHONY: check_fub
