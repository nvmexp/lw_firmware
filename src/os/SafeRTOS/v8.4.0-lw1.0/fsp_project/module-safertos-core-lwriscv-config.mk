#
# Copyright (c) 2020 LWPU CORPORATION.  All rights reserved.
#
# LWPU CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from LWPU CORPORATION is strictly prohibited.
#

#
# Print makefile diagnostic message
ifeq ($(VERBOSE), 1)
$(info Including:  module-safertos-core-lwriscv-config.mk)
endif

#
# Table of Contents:
#   TOC:  Declare the module dependencies and check them
#   TOC:  Declare the source directories, which automatically adds them to VPATH
#   TOC:  Declare the include file directories needed by this module

#
# Define a something similar to header guard protection...for makefiles instead.
#   This definition must exist for all 'module-soc_*.mk' files.
MODULE_SAFERTOS_CORE_LWRISCV_CONFIG          := 1

#
# Location of source code files
MODULE_SAFERTOS_CORE_LWRISCV_SOURCE_DIR      = $(SAFERTOS_DIR)

#
# Common includes '-I <INCLUDEDIR>'
LW_SOURCE                                    = $(P4ROOT)/sw/dev/gpu_drv/chips_a
MODULE_SAFERTOS_CORE_LWRISCV_INCLUDES        = -I $(SAFERTOS_DIR)/include
MODULE_SAFERTOS_CORE_LWRISCV_INCLUDES       += -I $(LW_SOURCE)/lwpu/inc
MODULE_SAFERTOS_CORE_LWRISCV_INCLUDES       += -I $(LW_SOURCE)/drivers/common/inc/swref/ampere/ga102
MODULE_SAFERTOS_CORE_LWRISCV_INCLUDES       += -I $(LW_SOURCE)/drivers/common/inc/swref/disp/v04_00
MODULE_SAFERTOS_CORE_LWRISCV_INCLUDES       += -I $(LW_SOURCE)/drivers/common/inc/hwref/ampere/ga102
MODULE_SAFERTOS_CORE_LWRISCV_INCLUDES       += -I $(LW_SOURCE)/drivers/common/inc/hwref/disp/v04_00
MODULE_SAFERTOS_CORE_LWRISCV_INCLUDES       += -I $(LW_SOURCE)/drivers/common/inc
MODULE_SAFERTOS_CORE_LWRISCV_INCLUDES       += -I $(LW_SOURCE)/drivers/resman/arch/lwalloc/common/inc
MODULE_SAFERTOS_CORE_LWRISCV_INCLUDES       += -I $(LW_SOURCE)/sdk/lwpu/inc
MODULE_SAFERTOS_CORE_LWRISCV_INCLUDES       += -I $(LW_SOURCE)/uproc/libs/cmn/inc
MODULE_SAFERTOS_CORE_LWRISCV_INCLUDES       += -I $(LW_SOURCE)/uproc/libs/lwos/dev/inc
MODULE_SAFERTOS_CORE_LWRISCV_INCLUDES       += -I $(LW_SOURCE)/uproc/lwriscv/inc
MODULE_SAFERTOS_CORE_LWRISCV_INCLUDES       += -I $(LW_SOURCE)/drivers/resman/arch/lwalloc/common/inc/gsp
MODULE_SAFERTOS_CORE_LWRISCV_INCLUDES       += -I $(LW_SOURCE)/uproc/disp/dpu/inc
MODULE_SAFERTOS_CORE_LWRISCV_INCLUDES       += -I $(LW_SOURCE)/uproc/gsp/inc
