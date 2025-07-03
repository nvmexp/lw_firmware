# Copyright (c) 2021, LWPU CORPORATION.  All Rights Reserved.
#
# LWPU Corporation and its licensors retain all intellectual property and
# proprietary rights in and to this software and related documentation.  Any
# use, reproduction, disclosure or distribution of this software and related
# documentation without an express license agreement from LWPU Corporation
# is strictly prohibited.

# Need to do this redirection because I need to set LIBCCC_CHIP as 'ad102'
# in the profile since the manuals are picked from 'ad102', but libCCC config
# will remain the same across chips.
include $(LIBCCC_CONFIGS)/$(LIBCCC_CHIP_FAMILY)/ad10x_sechub_se.mk
