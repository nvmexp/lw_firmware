#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 1999-2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

#-------------------------------------------------------------------------------
# MODS EUD packaging Makefile -- should only be run under Linux
# See //sw/<tree>/diag/mods/makedefs.inc for more information about the MODS
# makefiles.
#-------------------------------------------------------------------------------

BUILD_CFG := release
base_name := liblwvs-diagnostic

MODS_DIR := ../../
LWDA_DRIVER_DIR := $(MODS_DIR)../../drivers/gpgpu/

include $(MODS_DIR)/makedefs.inc

DIAG_C := $(gen_cpp_dir)/$(base_name).c
c_files := $(DIAG_C)
gen_c_files := $(DIAG_C)
CFLAGS  += -fPIC
BIN2C := $(LWDA_DRIVER_DIR)/compiler/gpgpu/export/bin/x86_64_Linux_develop/bin2c

LIBFILE := $(o_dir)/$(base_name)$(dll_suffix)

all: $(LIBFILE)

install: all
	$(CP) $(LIBFILE) $(MODS_RUNSPACE)/$(base_name)$(dll_suffix).1

$(DIAG_C): $(EUD_TGZ)
	$(MKDIR_P) $(gen_cpp_dir)
	$(BIN2C) -c --name eud_bytes -t int $^ > $@
	echo "const unsigned int EUD_SIZE = sizeof(eud_bytes) / sizeof(eud_bytes[0]);" >> $@

$(LIBFILE): $(EUD_TGZ) $(o_files)
	$(MKDIR_P) $(o_dir)
	$(CC) -o $@ -Wl,-soname,$(basename $@) $(o_files) -shared $(LDFLAGS)
ifeq "$(BUILD_CFG)" "release"
	$(STRIP) $@
endif

clean:
	$(RM_RF) $(gen_cpp_dir)
	$(RM_RF) $(o_dir)

include $(MODS_DIR)/makerule.inc

.PHONY: all clean install
