#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2017 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#
# uproc/selwrescrub/build/Makefile
#
# Run SELWRESCRUB mcheck and other source level checks in DVS
#

LW_SOURCE ?= $(shell cd ../../..; pwd)

CHECKSELWRESCRUB_PERL          ?= $(if $(findstring undefined,$(origin PERL)), perl, $(PERL))
CHECKSELWRESCRUB_ROOT          ?= $(LW_SOURCE)/uproc/selwrescrub

check_selwrescrub:
	rm -f check_selwrescrub.succeeded
	$(CHECKSELWRESCRUB_PERL) $(CHECKSELWRESCRUB_ROOT)/build/mcheck/mcheck.pl --profile
	touch check_selwrescrub.succeeded

# test targets
.PHONY: check_selwrescrub
