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
$(info Including:  module-safertos-port-lwriscv-config.mk)
endif

#
# Table of Contents:
#   TOC:  Declare the module dependencies and check them
#   TOC:  Declare the source directories, which automatically adds them to VPATH
#   TOC:  Declare the include file directories needed by this module

#
# Define a something similar to header guard protection...for makefiles instead.
#   This definition must exist for all 'module-soc_*.mk' files.
MODULE_SAFERTOS_PORT_LWRISCV_CONFIG         := 1

#
# Location of source code files
MODULE_SAFERTOS_PORT_LWRISCV_SOURCE_DIR  = $(SAFERTOS_PORT_DIR)

#
# Common includes '-I <INCLUDEDIR>'
MODULE_SAFERTOS_PORT_LWRISCV_INCLUDES   += -I $(SAFERTOS_PORT_DIR)

#
# Configuration options.  These can be specified to control how the firmware
# is built.  Setting the option=1 (e.g. DEBUG=1) will enable the option and
# setting the option to 0 (e.g. DEBUG=0) will disable the option.
#
# LWRISCV_MTIME_TICK_SHIFT Serves as a divisor to help callwlate clock
#               rate.
#
LWRISCV_MTIME_TICK_SHIFT    ?= 0
