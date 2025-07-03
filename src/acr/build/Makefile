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
# Run ACR mcheck and other source level checks in DVS
#

LW_SOURCE ?= $(shell cd ../../..; pwd)

CHECKACR_PERL          ?= $(if $(findstring undefined,$(origin PERL)), perl, $(PERL))
CHECKACR_ROOT          ?= $(LW_SOURCE)/uproc/acr

check_acr:
	$(RM) -f mcheck_acr.succeeded
	$(CHECKACR_PERL) $(CHECKACR_ROOT)/build/mcheck/mcheck.pl --profile
	touch mcheck_acr.succeeded

# test targets
.PHONY: check_acr

