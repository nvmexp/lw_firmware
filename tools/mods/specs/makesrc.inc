#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#
ifeq "$(INCLUDE_GPU)" "true"
    internal_release_files$(ONLY_PC) += specs/bringup.arg
    internal_release_files$(ONLY_PC) += specs/dev.arg

    pvs_release_files += specs/instinsys.spc
    pvs_release_files += specs/pvs_mods_sriovsanity_cmds.tcl
endif

ifeq "$(INCLUDE_GPU_SPECIFIC)" "true"
    internal_release_files$(ONLY_PC) += specs/tu10xfmod.spc
endif

ifneq (,$(filter $(BUILD_OS), winsim sim))
    ifeq "$(INCLUDE_DVS_SANITY)" "true"
        internal_release_files$(ONLY_PC) += specs/tu10xfmod.spc
    endif
endif

ifeq "$(INCLUDE_DFPGA)" "true"
    internal_release_files$(ONLY_PC) += specs/tu10x_dfpga.spc
endif

basic_release_files$(ONLY_DGPU)     += specs/base.spc
pvs_only_files                      += specs/dvs.spc
internal_release_files              += specs/dvs_mfg.spc
internal_release_files$(ONLY_DGPU)  += specs/dvs_perf.spc
internal_release_files              += specs/tu10xemu.spc
internal_release_files              += specs/ga10xemu.spc
internal_release_files              += specs/gh100emu.spc
internal_release_files              += specs/vsmc_pf.spc
internal_release_files              += specs/vsmc_vf.spc

ifeq "$(BUILD_OS)" "cheetah"
    internal_release_files          += specs/t234_bft.spc
    internal_release_files          += specs/t234_ift.spc
endif
