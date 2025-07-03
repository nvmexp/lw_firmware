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
$(info Including:  module-safertos-core-lwriscv.mk)
endif

#
# Table of Contents:
#   TOC:  Define the module name
#   TOC:  Declare the module dependencies and check them
#   TOC:  Declare the source directories, which automatically adds them to VPATH
#   TOC:  Declare the include file directories needed by this module
#   TOC:  Declare the C language files
#   TOC:  Declare the Assembly language files
#   TOC:  Declare C-Flags used by this module
#   TOC:  Declare ASM-Flags used by this module

#
# Define module name:
#   Once the module name is defined, it will be added to the MODULE_NAMES
#   list.  This can be done manually (like it is here), or automatically
#   on a per-application basis.
MODULE_SAFERTOS_CORE_LWRISCV_NAME  := SAFERTOS_CORE_LWRISCV

#
# Define a something similar to header guard protection...for makefiles instead.
#   This definition must exist for all 'module-soc_*.mk' files.
MODULE_SAFERTOS_CORE_LWRISCV_DEFINED  := 1

#
# Check for sub-make module dependencies.  For each of the module dependencies
#   defined below, they must exist in the build before this (current) sub-make
#   is called.
CONFIG_SAFERTOS_CORE_DEPENDS  := SAFERTOS_CORE_LWRISCV
$(foreach _,$(CONFIG_SAFERTOS_CORE_DEPENDS),$(eval $(call CHECK_MAKEFILE_DEFINED,$_)))

#
# C Source
ifndef MODULE_SAFERTOS_CORE_LWRISCV_C_SRC
    MODULE_SAFERTOS_CORE_LWRISCV_C_SRC   =
    MODULE_SAFERTOS_CORE_LWRISCV_C_SRC  += $(SAFERTOS_DIR)/list.c
    MODULE_SAFERTOS_CORE_LWRISCV_C_SRC  += $(SAFERTOS_DIR)/queue.c
    MODULE_SAFERTOS_CORE_LWRISCV_C_SRC  += $(SAFERTOS_DIR)/semaphore.c
    MODULE_SAFERTOS_CORE_LWRISCV_C_SRC  += $(SAFERTOS_DIR)/task.c
    MODULE_SAFERTOS_CORE_LWRISCV_C_SRC  += $(SAFERTOS_DIR)/eventgroups.c
    MODULE_SAFERTOS_CORE_LWRISCV_C_SRC  += $(SAFERTOS_DIR)/eventpoll.c
    MODULE_SAFERTOS_CORE_LWRISCV_C_SRC  += $(SAFERTOS_DIR)/mutex.c
endif

#
# Assembly source
MODULE_SAFERTOS_CORE_LWRISCV_ASM_SRC     =

#
# Special C Flags
MODULE_SAFERTOS_CORE_LWRISCV_C_FLAGS     =

#
# Special ASM Flags
MODULE_SAFERTOS_CORE_LWRISCV_ASM_FLAGS   =
