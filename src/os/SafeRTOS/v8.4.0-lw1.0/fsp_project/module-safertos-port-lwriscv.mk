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
$(info Including:  module-safertos-port-lwriscv.mk)
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
MODULE_SAFERTOS_PORT_LWRISCV_NAME   := SAFERTOS_PORT_LWRISCV

#
# Define a something similar to header guard protection...for makefiles instead.
#   This definition must exist for all 'module-soc_*.mk' files.
MODULE_SAFERTOS_PORT_LWRISCV_DEFINED    := 1

#
# Check for sub-make module dependencies.  For each of the module dependencies
#   defined below, they must exist in the build before this (current) sub-make
#   is called.
CONFIG_SAFERTOS_PORT_DEPENDS        := SAFERTOS_CORE_LWRISCV SAFERTOS_PORT_LWRISCV
$(foreach _,$(CONFIG_SAFERTOS_PORT_DEPENDS),$(eval $(call CHECK_MAKEFILE_DEFINED,$_)))

#
# C Source
MODULE_SAFERTOS_PORT_LWRISCV_C_SRC   =
MODULE_SAFERTOS_PORT_LWRISCV_C_SRC  += $(SAFERTOS_PORT_DIR)/port.c
MODULE_SAFERTOS_PORT_LWRISCV_C_SRC  += $(SAFERTOS_PORT_DIR)/portisr.c
MODULE_SAFERTOS_PORT_LWRISCV_C_SRC  += $(SAFERTOS_PORT_DIR)/heap_4.c
MODULE_SAFERTOS_PORT_LWRISCV_C_SRC  += $(SAFERTOS_PORT_DIR)/shared.c

#
# Assembly source
MODULE_SAFERTOS_PORT_LWRISCV_ASM_SRC     =
MODULE_SAFERTOS_PORT_LWRISCV_ASM_SRC    += $(SAFERTOS_PORT_DIR)/portasm.S

#
# Special C Flags
MODULE_SAFERTOS_PORT_LWRISCV_C_FLAGS     =
MODULE_SAFERTOS_PORT_LWRISCV_C_FLAGS    += -DLWRISCV_MTIME_TICK_SHIFT=$(LWRISCV_MTIME_TICK_SHIFT)

#
# Special ASM Flags
MODULE_SAFERTOS_PORT_LWRISCV_ASM_FLAGS =
