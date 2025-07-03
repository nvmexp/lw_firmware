#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2011-2018 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

#
# This makefile defines test-specific variables, then includes the common
# bootloader test makefile
#

SRC            := src/start.S $(APP_COMMON_SW)/src/mpu_main.c
SRC_PARSED     := 

include $(LW_SOURCE)/uproc/lwriscv/bootloader_tests/common/config/profiles.mk
