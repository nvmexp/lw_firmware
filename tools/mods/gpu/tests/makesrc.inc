#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2011-2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

cpp_files                           += gpu/tests/js_gputest.cpp
cpp_files                           += gpu/tests/gpumtest.cpp
cpp_files                           += gpu/tests/matshelp.cpp
cpp_files                           += gpu/tests/thermalres/thermres.cpp
cpp_files                           += gpu/tests/thermalres/thermresgpu.cpp
cpp_files                           += gpu/tests/thermalres/thermressoc.cpp

cpp_files$(ONLY_DGPU)               += gpu/tests/b1remap.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/iobandwidth.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/c2c/c2cbw.cpp
cpp_files                           += gpu/tests/cpyengtest.cpp

ifneq (,$(filter $(BUILD_OS), linuxmfg sim))
    cpp_files$(ONLY_DGPU)           += gpu/tests/selwrecpy.cpp
endif

cpp_files$(ONLY_DGPU)               += gpu/tests/perf/perfpunish.cpp
cpp_files                           += gpu/tests/perf/perfswitch.cpp
cpp_files                           += gpu/tests/perf/pwm.cpp
ifeq "$(BUILD_OS)" "cheetah"
    cpp_files                       += gpu/tests/perf/elpg_android.cpp
else
    cpp_files                       += gpu/tests/perf/elpg.cpp
endif
cpp_files                           += gpu/tests/fastmats.cpp
cpp_files$(ONLY_PC)                 += gpu/tests/fusesensetest.cpp
cpp_files$(ONLY_PC)                 += gpu/tests/fusecheck.cpp
cpp_files$(ONLY_PC)                 += gpu/tests/checkconfig2.cpp
cpp_files$(ONLY_PC)                 += gpu/tests/secfault.cpp
cpp_files                           += gpu/tests/i2m.cpp
cpp_files                           += gpu/tests/linetest.cpp
cpp_files                           += gpu/tests/lsfalcons.cpp
cpp_files                           += gpu/tests/memscrub.cpp
cpp_files                           += gpu/tests/mmerand.cpp
ifeq "$(INCLUDE_MME64)" "true"
    cpp_files                       += gpu/tests/mme/mme64igen.cpp
    cpp_files                       += gpu/tests/mme/mme64macro.cpp
    cpp_files                       += gpu/tests/mme/mme64manager.cpp
    cpp_files                       += gpu/tests/mme/mme64rand.cpp
    cpp_files                       += gpu/tests/mme/mme64sim.cpp
endif
ifeq "$(INCLUDE_MUCC)" "true"
    cpp_files                       += gpu/tests/mucctest.cpp
endif
cpp_files                           += gpu/tests/lwenc.cpp
cpp_files$(ONLY_ARM)$(ONLY_IGPU)    += gpu/tests/lwjpg.cpp
cpp_files                           += gpu/tests/perf/pwrregcheck.cpp

ifdef TEGRA_TOP
    $(o_dir)/lwenc$(obj_suffix): include_dirs += $(TEGRA_TOP)/multimedia/tvmr/falconip/msenc/drv2ucode
else
    $(o_dir)/lwenc$(obj_suffix): include_dirs += $(DRIVERS_DIR)/common/msenc/drv2ucode
endif

ifeq "$(BUILD_OS)" "cheetah"
    ifndef TEGRA_TOP
        pvs_release_files += $(DRIVERS_DIR)/common/lwenc5/lwhost_lwenc050.fw
        pvs_release_files += $(DRIVERS_DIR)/common/lwenc6/lwhost_lwenc061.fw
        pvs_release_files += $(DRIVERS_DIR)/common/lwenc7/lwhost_lwenc070.fw
    endif
endif

cpp_files                           += gpu/tests/random2d.cpp
basic_release_files                 += gpu/js/random2d.js

cpp_files$(ONLY_DGPU)               += gpu/tests/idt/idt.cpp
ifeq "$(BUILD_OS)" "win32"
    cpp_files$(ONLY_DGPU)           += gpu/tests/winmods/winmods_sanity.cpp
endif

cpp_files$(ONLY_ARM)$(ONLY_IGPU)               += gpu/tests/tegravic.cpp
cpp_files$(ONLY_ARM)$(ONLY_IGPU)               += gpu/tests/tegravic_vic4.cpp
cpp_files$(ONLY_ARM)$(ONLY_IGPU)               += gpu/tests/vic_ldc_tnr.cpp
cpp_files$(ONLY_ARM)$(ONLY_IGPU)               += gpu/tests/vic_ldc_tnr_helper.cpp
basic_release_files$(ONLY_ARM)$(ONLY_IGPU)     += gpu/tests/traces/vic_tnr3_input_image.bin
basic_release_files$(ONLY_ARM)$(ONLY_IGPU)     += gpu/tests/traces/vic_tnr3_denoise_input_image.bin
basic_release_files$(ONLY_ARM)$(ONLY_IGPU)     += gpu/tests/traces/vic_tnr3_preAlpha_image.bin
basic_release_files$(ONLY_ARM)$(ONLY_IGPU)     += gpu/tests/traces/vic_ldc_sparseWrapImage.bin

cpp_files                           += gpu/tests/gputracetest.cpp
cpp_files                           += gpu/tests/settempctrl.cpp
cpp_files                           += gpu/tests/traceplayer/trace_loader.cpp
cpp_files                           += gpu/tests/traceplayer/trace_parser.cpp
cpp_files                           += gpu/tests/traceplayer/trace_player_engine.cpp
cpp_files                           += gpu/tests/traceplayer/trace_crc.cpp
cpp_files                           += gpu/tests/traceplayer/trace_surface_accessor.cpp

pvs_release_files$(ONLY_PC)         += gpu/tests/traces/minigine_pascal_a.tgz
pvs_release_files                   += gpu/tests/traces/mini_heaven_volta_a.tgz
pvs_release_files                   += gpu/tests/traces/mini_heaven_ampere_b.tgz
pvs_release_files                   += gpu/tests/traces/mini_heaven_t234.tgz

# Workaround for some files using ptr_vector which uses auto_ptr
# auto_ptr is deprecated in C++17 and by default MSVC doesn't expose it
# unless a special define is used
ifeq "$(HOST_OS)" "win32"
    auto_ptr_war_files              += gpu/tests/lwdec.cpp
    auto_ptr_war_files              += gpu/tests/lwencoders/ienginewrapper.cpp
    auto_ptr_war_files              += gpu/tests/lwencoders/cla0b0engine.cpp
    auto_ptr_war_files              += gpu/tests/lwencoders/clb0b0engine.cpp
    auto_ptr_war_files              += gpu/tests/lwencoders/clb6b0engine.cpp
    auto_ptr_war_files              += gpu/tests/lwencoders/clc0b0socengine.cpp
    auto_ptr_war_files              += gpu/tests/lwencoders/clc1b0engine.cpp
    auto_ptr_war_files              += gpu/tests/lwencoders/clc2b0engine.cpp
    auto_ptr_war_files              += gpu/tests/lwencoders/clc4b0engine.cpp
    auto_ptr_war_files              += gpu/tests/lwencoders/h264syntax.cpp
    auto_ptr_war_files              += gpu/tests/lwencoders/h264parser.cpp
    auto_ptr_war_files              += gpu/tests/lwencoders/h265syntax.cpp
    auto_ptr_war_files              += gpu/tests/lwencoders/h265parser.cpp
    auto_ptr_war_files              += gpu/tests/lwencoders/vc1syntax.cpp
    auto_ptr_war_files              += gpu/tests/lwencoders/vc1parser.cpp
$(addprefix $(o_dir)/,$(notdir $(auto_ptr_war_files:.cpp=$(obj_suffix)))): CFLAGS += -D_HAS_AUTO_PTR_ETC=1
endif

# LWDEC test files
cpp_files                           += gpu/tests/lwdec.cpp
cpp_files                           += gpu/tests/lwencoders/ienginewrapper.cpp
cpp_files                           += gpu/tests/lwencoders/cla0b0engine.cpp
cpp_files                           += gpu/tests/lwencoders/clb0b0engine.cpp
cpp_files                           += gpu/tests/lwencoders/clb6b0engine.cpp
cpp_files                           += gpu/tests/lwencoders/clc0b0socengine.cpp
cpp_files                           += gpu/tests/lwencoders/clc1b0engine.cpp
cpp_files                           += gpu/tests/lwencoders/clc2b0engine.cpp
cpp_files                           += gpu/tests/lwencoders/clc4b0engine.cpp
ifeq "$(BUILD_OS)" "cheetah"
    cpp_files                       += gpu/tests/tegralwdec.cpp
    cpp_files                       += gpu/tests/video/tegralwjpg.cpp
    cpp_files                       += gpu/tests/video/tegralwofa.cpp
    cpp_files                       += gpu/tests/video/tegralwenc.cpp
    cpp_files                       += gpu/tests/video/video_utils.cpp
    CLIENTPATHS                     += //sw/mods/traces/video/jpg/...
    basic_release_files             += $(SHARED_MODS_FILES_DIR)/traces/video/jpg/lwjpg.bin
endif
CLIENTPATHS                         += //sw/mods/traces/video/lwdec/...
CLIENTPATHS                         += //sw/mods/scripts/...
basic_release_files                 += $(SHARED_MODS_FILES_DIR)/traces/video/lwdec/lwdec.bin
pvs_exclude_files                   += $(SHARED_MODS_FILES_DIR)/traces/video/lwdec/lwdec.bin
unencrypted_internal_files          += $(SHARED_MODS_FILES_DIR)/scripts/fetch_video_traces_using_gpu.py
unencrypted_internal_files          += $(SHARED_MODS_FILES_DIR)/scripts/video_traces_versions.json

# H.264 support for LWDEC
cpp_files                           += gpu/tests/lwencoders/h264syntax.cpp
cpp_files                           += gpu/tests/lwencoders/h264parser.cpp

# H.265 support for LWDEC
cpp_files                           += gpu/tests/lwencoders/h265syntax.cpp
cpp_files                           += gpu/tests/lwencoders/h265parser.cpp

# VC1 support for LWDEC
cpp_files                           += gpu/tests/lwencoders/vc1syntax.cpp
cpp_files                           += gpu/tests/lwencoders/vc1parser.cpp

cpp_files                           += gpu/tests/nwfmats.cpp

cpp_files$(ONLY_DGPU)               += gpu/tests/gsp/gspsanity.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/gsp/falcon.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/gsp/turingfalcon.cpp

cpp_files$(ONLY_DGPU)               += gpu/tests/cl88b4.cpp
cpp_files$(ONLY_PC)                 += gpu/tests/perf/clkpulse.cpp
cpp_files$(ONLY_PC)                 += gpu/tests/eccfb.cpp
cpp_files$(ONLY_PC)                 += gpu/tests/eccinjection.cpp
cpp_files                           += gpu/tests/eccl2.cpp
cpp_files                           += gpu/tests/configurable_display/check_panel_status.cpp
cpp_files                           += gpu/tests/configurable_display/configurable_display_base.cpp
cpp_files                           += gpu/tests/configurable_display/configurable_display_htol.cpp
cpp_files                           += gpu/tests/configurable_display/configurable_display_interactive.cpp
cpp_files                           += gpu/tests/configurable_display/gsync_crc_checker.cpp
cpp_files                           += gpu/tests/configurable_display/gsync_utils.cpp
cpp_files                           += gpu/tests/configurable_display/sor_loopback_cdt.cpp
cpp_files                           += gpu/tests/configurable_display/check_hdcp_cdt.cpp
cpp_files                           += gpu/tests/configurable_display/validate_imp.cpp
cpp_files                           += gpu/tests/configurable_display/validate_vrr.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/gsynclink.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/gsyncmem.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/evo_lwrs.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/evo_ovrl.cpp
cpp_files$(ONLY_PC)                 += gpu/tests/evo_sli.cpp
cpp_files$(ONLY_PC)                 += gpu/tests/sliloopback.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/sorloopback.cpp
cpp_files                           += gpu/tests/loopbacktester.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/wbor.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/lwdisplayhtol.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/lwdisplayrandom/lwdrcoverage.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/lwdisplayrandom/lwdisplayrandom.cpp
cpp_files$                          += gpu/tests/lwdisplayrandomv2/lwdisplayrandom_v2.cpp
cpp_files$                          += gpu/tests/lwdisplayrandomv2/lwdrcoverage_v2.cpp
cpp_files$                          += gpu/tests/lwdisplayrandomv2/lwdrpickers_v2.cpp
cpp_files$                          += gpu/tests/lwdisplayrandomv2/lwdrtracecache.cpp
cpp_files$                          += gpu/tests/lwdisplayrandomv2/surfcache.cpp
cpp_files$                          += gpu/tests/lwdisplayrandomv2/lwdrpclkpicker.cpp
cpp_files$                          += gpu/tests/lwdisplayrandomv2/lwdrutils.cpp
cpp_files                           += gpu/tests/lwdisplaydpc.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/evo_thread.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/evo_htol.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/vga_hw.cpp
cpp_files                           += gpu/tests/disp_crcchecker.cpp
ifeq "$(BUILD_OS)" "linuxmfg"
    cpp_files$(ONLY_DGPU)           += gpu/tests/dynamicdisplaymux.cpp
    cpp_files$(ONLY_DGPU)           += gpu/tests/dynamicdisplaymuxhelper.cpp
    cpp_files$(ONLY_DGPU)           += gpu/tests/panelfirmwareverif.cpp
endif
cpp_files$(ONLY_DGPU)               += gpu/tests/fpk_test.cpp
cpp_files                           += gpu/tests/hdcpkey.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/hdcpupstream.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/i2c/i2ctest.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/i2c/i2cdcbsanitytest.cpp
cpp_files                           += gpu/tests/kfusesanity.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/panelcheck.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/tconstress.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/mlwimage.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/lwsrsystemcheck.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/perf/checkavfs.cpp
cpp_files                           += gpu/tests/lwspec.cpp

cpp_files$(ONLY_PC)                 += gpu/tests/mats.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/checkdma.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/msenc.cpp

ifeq "$(INCLUDE_LWLINK)" "true"
    cpp_files                       += gpu/tests/lwlink/opticaleyegrading.cpp
    cpp_files                       += gpu/tests/lwlink/lwlinktest.cpp
    cpp_files                       += gpu/tests/lwlink/lwldatatest/lwldt_priv.cpp
    cpp_files                       += gpu/tests/lwlink/lwldatatest/lwldt_test_route.cpp
    cpp_files                       += gpu/tests/lwlink/lwldatatest/lwldt_test_mode.cpp
    cpp_files                       += gpu/tests/lwlink/lwldatatest/lwldt_alloc_mgr.cpp
    cpp_files                       += gpu/tests/lwlink/lwldatatest.cpp
    cpp_files                       += gpu/tests/lwlink/lwlbwstress/lwlbws_test_mode.cpp
    cpp_files                       += gpu/tests/lwlink/lwlbwstress/lwlbws_test_route.cpp
    cpp_files                       += gpu/tests/lwlink/lwlbwstress/lwlbws_alloc_mgr.cpp
    ifeq "$(INCLUDE_LWSWITCH)" "true"
        cpp_files                   += gpu/tests/lwlink/lwlbwstress_trex.cpp
        cpp_files                   += gpu/tests/lwlink/lwlbwstress_trex/lwlbws_trex_test_route.cpp
        cpp_files                   += gpu/tests/lwlink/lwlbwstress_trex/lwlbws_trex_test_mode.cpp
        ifeq "$(LWCFG_GLOBAL_LWSWITCH_IMPL_LS10)" "1"
            cpp_files               += gpu/tests/lwlink/lwlinkmulticasttrex.cpp
        endif
    endif
    cpp_files                       += gpu/tests/lwlink/lwlbwstress.cpp
    cpp_files                       += gpu/tests/lwlink/lwlinkeyediagram.cpp
    cpp_files$(ONLY_DGPU)           += gpu/tests/i2c/i2cverifydevices.cpp
    cpp_files                       += gpu/tests/lwlink/lwlinkstate.cpp
    cpp_files                       += gpu/tests/lwlink/lwlvalidatelinks.cpp
    cpp_files                       += gpu/tests/lwlink/lwlinkiobist.cpp
    ifeq "$(BUILD_OS)" "cheetah"
        cpp_files                   += gpu/tests/lwlink/lwlbwstress_tegra/lwlbwst_test_route.cpp
        cpp_files                   += gpu/tests/lwlink/lwlbwstress_tegra/lwlbwst_test_mode.cpp
        cpp_files                   += gpu/tests/lwlink/lwlbwstress_tegra/lwlbwst_alloc_mgr.cpp
        cpp_files                   += gpu/tests/lwlink/lwlbwstress_tegra.cpp
    endif
    cpp_files$(ONLY_DGPU)           += gpu/tests/verify_uphy_bkv.cpp
endif

cpp_files                           += gpu/tests/quick_slowdown.cpp
cpp_files$(ONLY_PC)                 += gpu/tests/mscgmats.cpp
cpp_files$(ONLY_PC)                 += gpu/tests/optimus.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/gpureset.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/flreset.cpp
cpp_files$(ONLY_PC)                 += gpu/tests/perf/gpugcx.cpp
cpp_files                           += gpu/tests/pcie/pextest.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/pcie/bar1p2pbw.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/pcie/gen2link.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/pcie/pexbandwidth.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/pcie/pexaspm.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/pcie/pexmargin.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/pcie/pcieeyediagram.cpp
ifneq (,$(filter $(BUILD_ARCH), armv7 aarch64))
    basic_release_files$(ONLY_DGPU)  += gpu/js/tegrapcie.js
endif

cpp_files$(ONLY_PC)                 += gpu/tests/sec2.cpp
cpp_files$(ONLY_PC)                 += gpu/tests/stereo.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/validsku.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/validsku2.cpp

cpp_files$(ONLY_DGPU)               += gpu/tests/perf/gpuplllocktest.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/perf/clkmon.cpp

cpp_files                           += gpu/tests/sriovsync.cpp

ifneq (,$(filter $(BUILD_OS),linuxmfg linux))
    cpp_files$(ONLY_PC)             += gpu/tests/smbpbi.cpp
    cpp_files$(ONLY_DGPU)           += gpu/tests/perf/zpicycle.cpp
endif

ifeq "$(INCLUDE_XUSB)" "true"
    cpp_files$(ONLY_DGPU)                     += gpu/tests/usb/usbbandwidth.cpp
    cpp_files$(ONLY_DGPU)                     += gpu/tests/usb/xusbeyediagram.cpp
endif

ifeq "$(INCLUDE_MATHS_IST)" "true"
    cpp_files$(ONLY_DGPU)                     += gpu/tests/ist/ist.cpp
    cpp_files$(ONLY_DGPU)                     += gpu/tests/ist/ist_init.cpp
    cpp_files$(ONLY_DGPU)                     += gpu/tests/ist/ist_mal_handler.cpp
    cpp_files$(ONLY_DGPU)                     += gpu/tests/ist/ist_mem_device.cpp
    cpp_files$(ONLY_DGPU)                     += gpu/tests/ist/js_ist.cpp
    cpp_files$(ONLY_DGPU)                     += gpu/tests/ist/ist_smbus.cpp
    basic_release_files$(ONLY_DGPU)           += gpu/js/mathslink.js
    basic_release_files$(ONLY_DGPU)           += gpu/js/mathslink_args.js
    ifeq "$(INCLUDE_MODS_GTEST)" "true"
        cpp_files += unittests/gtest/mods/gpu/tests/ist/ist_mem_device_ut.cpp
    endif
endif

cpp_files$(ONLY_ARM)$(ONLY_IGPU)              += gpu/tests/tsec.cpp
cpp_files$(ONLY_ARM)$(ONLY_IGPU)              += gpu/tests/tegratsec.cpp
cpp_files$(ONLY_ARM)$(ONLY_IGPU)              += gpu/tests/tsec/cl95a1_secopuc.cpp
internal_tsec_files$(ONLY_ARM)$(ONLY_IGPU)    += $(o_dir)/tsec.mp2
internal_tsec_files$(ONLY_ARM)$(ONLY_IGPU)    += $(o_dir)/tsec.vc1
internal_tsec_files$(ONLY_ARM)$(ONLY_IGPU)    += $(o_dir)/tsec.264
internal_release_files$(ONLY_ARM)$(ONLY_IGPU) += $(o_dir)/tsec.bin

cpp_files$(ONLY_ARM)$(ONLY_IGPU)              += gpu/tests/tegradisptestcomn.cpp
cpp_files$(ONLY_ARM)$(ONLY_IGPU)              += gpu/tests/tegrawin.cpp
cpp_files$(ONLY_ARM)$(ONLY_IGPU)              += gpu/tests/tegradisprandom.cpp
ifeq "$(BUILD_OS)" "cheetah"
    cpp_files                                 += gpu/tests/tegragsynclink.cpp
endif
cpp_files$(ONLY_DGPU)                         += gpu/tests/tegracec.cpp

LWBINS_DIR ?= gpu/tests/traces

ifeq "$(INCLUDE_LWDA)" "true"
    cpp_files                                 += gpu/tests/lwdastst.cpp
    cpp_files                                 += gpu/tests/lwdamemtest.cpp
    cpp_files                                 += gpu/tests/lwca/lwdawrap.cpp

    sm_50_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/gpusync50.lwbin
    sm_52_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/gpusync52.lwbin
    sm_53_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/gpusync53.lwbin
    sm_60_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/gpusync60.lwbin
    sm_61_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/gpusync61.lwbin
    sm_70_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/gpusync70.lwbin
    sm_72_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/gpusync72.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/gpusync75.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/gpusync80.lwbin
    sm_86_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/gpusync86.lwbin
    sm_87_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/gpusync87.lwbin
    sm_89_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/gpusync89.lwbin
    sm_90_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/gpusync90.lwbin

    cpp_files                                      += gpu/tests/lwca/lwda_instr_pulse.cpp
    sm_70_basic_lwbin_files$(ONLY_DGPU)            += $(LWBINS_DIR)/instrpulse70.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)            += $(LWBINS_DIR)/instrpulse75.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)            += $(LWBINS_DIR)/instrpulse80.lwbin
    sm_86_basic_lwbin_files$(ONLY_DGPU)            += $(LWBINS_DIR)/instrpulse86.lwbin
    sm_87_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU) += $(LWBINS_DIR)/instrpulse87.lwbin
    sm_89_basic_lwbin_files$(ONLY_DGPU)            += $(LWBINS_DIR)/instrpulse89.lwbin
    sm_90_basic_lwbin_files$(ONLY_DGPU)            += $(LWBINS_DIR)/instrpulse90.lwbin

ifeq "$(INCLUDE_LWLWVID)" "true"
    cpp_files                               += gpu/tests/lwvidlwdec.cpp
    cpp_files                               += gpu/tests/lwvidlwenc.cpp
    cpp_files                               += gpu/tests/lwvidlwjpg.cpp
    cpp_files                               += gpu/tests/lwvidlwofa.cpp
    $(o_dir)/lwvidlwdec$(obj_suffix): include_dirs += $(DRIVERS_DIR)/multimedia
    $(o_dir)/lwvidlwdec$(obj_suffix): include_dirs += $(o_dir)/../liblwlwvid_gen
    $(o_dir)/lwvidlwenc$(obj_suffix): include_dirs += $(DRIVERS_DIR)/multimedia
    $(o_dir)/lwvidlwenc$(obj_suffix): include_dirs += $(o_dir)/../liblwlwvid_gen
    $(o_dir)/lwvidlwjpg$(obj_suffix): include_dirs += $(DRIVERS_DIR)/multimedia/codecs/lwca/lwdecstill/parser/inc
    $(o_dir)/lwvidlwjpg$(obj_suffix): include_dirs += $(o_dir)/../liblwlwvid_gen
    $(o_dir)/lwvidlwofa$(obj_suffix): include_dirs += $(DRIVERS_DIR)/multimedia
    $(o_dir)/lwvidlwofa$(obj_suffix): include_dirs += $(DRIVERS_DIR)/multimedia/OpticalFlow/OFAPI/inc
    $(o_dir)/lwvidlwofa$(obj_suffix): include_dirs += $(DRIVERS_DIR)/multimedia/OpticalFlow/OFAPI/interface
    $(o_dir)/lwvidlwofa$(obj_suffix): include_dirs += $(BUILD_TOOLS_DIR)/lpng1251
    $(o_dir)/lwvidlwofa$(obj_suffix): include_dirs += core/include

    CLIENTPATHS += //sw/mods/traces/video/ofa/...
    CLIENTPATHS += //sw/mods/traces/video/jpg/...
    VIDEO_TRACES_DIR ?= $(SHARED_MODS_FILES_DIR)/traces/video
    basic_release_files$(ONLY_DGPU) += $(VIDEO_TRACES_DIR)/ofa/lwofa.bin
    pvs_exclude_files               += $(VIDEO_TRACES_DIR)/ofa/lwofa.bin

    internal_release_files += gpu/tests/traces/Jpg/176x144_422_ref.yuv
    internal_release_files += gpu/tests/traces/Jpg/AVI_mjpeg_pcm_ref.yuv

    basic_release_files += $(VIDEO_TRACES_DIR)/jpg/lwjpg.bin
    pvs_exclude_files   += $(VIDEO_TRACES_DIR)/jpg/lwjpg.bin
endif

cpp_files$(ONLY_DGPU)                  += gpu/tests/lwca/lwda_peer_atomics.cpp
sm_60_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/peeratomic60.lwbin
sm_70_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/peeratomic70.lwbin
sm_75_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/peeratomic75.lwbin
sm_80_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/peeratomic80.lwbin
sm_86_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/peeratomic86.lwbin
sm_89_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/peeratomic89.lwbin
sm_90_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/peeratomic90.lwbin

cpp_files$(ONLY_DGPU)                  += gpu/tests/lwca/lwda_c2c_bandwidth.cpp
sm_90_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/c2cbandwidth90.lwbin

ifeq "$(INCLUDE_LWLINK)" "true"

    cpp_files$(ONLY_DGPU)                  += gpu/tests/lwca/lwda_lwlink_p2p_bandwidth.cpp
    sm_70_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/lwlinkp2pbandwidth70.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/lwlinkp2pbandwidth75.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/lwlinkp2pbandwidth80.lwbin
    sm_86_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/lwlinkp2pbandwidth86.lwbin
    sm_89_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/lwlinkp2pbandwidth89.lwbin
    sm_90_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/lwlinkp2pbandwidth90.lwbin

    cpp_files$(ONLY_DGPU)                  += gpu/tests/lwca/lwlwl/lwlwl_test_mode.cpp
    cpp_files$(ONLY_DGPU)                  += gpu/tests/lwca/lwlwl/lwlwl_test_route.cpp
    cpp_files$(ONLY_DGPU)                  += gpu/tests/lwca/lwlwl/lwlwl_alloc_mgr.cpp
    cpp_files$(ONLY_DGPU)                  += gpu/tests/lwca/lwda_lwlink.cpp
    sm_70_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/lwlink70.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/lwlink75.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/lwlink80.lwbin
    sm_86_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/lwlink86.lwbin
    sm_89_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/lwlink89.lwbin
    sm_90_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/lwlink90.lwbin
endif

    cpp_files                                     += gpu/tests/lwca/lwda_cbu.cpp
    sm_70_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/cbu70.lwbin
    sm_72_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/cbu72.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/cbu75.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/cbu80.lwbin
    sm_86_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/cbu86.lwbin
    sm_87_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/cbu87.lwbin
    sm_89_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/cbu89.lwbin
    sm_90_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/cbu90.lwbin

    cpp_files$(ONLY_PC)                           += gpu/tests/lwca/lwda_mats.cpp
    sm_50_basic_lwbin_files$(ONLY_PC)             += $(LWBINS_DIR)/lwmats50.lwbin
    sm_52_basic_lwbin_files$(ONLY_PC)             += $(LWBINS_DIR)/lwmats52.lwbin
    sm_60_basic_lwbin_files$(ONLY_PC)             += $(LWBINS_DIR)/lwmats60.lwbin
    sm_70_basic_lwbin_files$(ONLY_PC)             += $(LWBINS_DIR)/lwmats70.lwbin
    sm_75_basic_lwbin_files$(ONLY_PC)             += $(LWBINS_DIR)/lwmats75.lwbin
    sm_80_basic_lwbin_files$(ONLY_PC)             += $(LWBINS_DIR)/lwmats80.lwbin
    sm_86_basic_lwbin_files$(ONLY_PC)             += $(LWBINS_DIR)/lwmats86.lwbin
    sm_89_basic_lwbin_files$(ONLY_PC)             += $(LWBINS_DIR)/lwmats89.lwbin
    sm_90_basic_lwbin_files$(ONLY_PC)             += $(LWBINS_DIR)/lwmats90.lwbin

    cpp_files                                    += gpu/tests/lwca/nwlwmats.cpp
    sm_50_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/ncmats50.lwbin
    sm_52_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/ncmats52.lwbin
    sm_53_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/ncmats53.lwbin
    sm_60_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/ncmats60.lwbin
    sm_61_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/ncmats61.lwbin
    sm_70_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/ncmats70.lwbin
    sm_72_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/ncmats72.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/ncmats75.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/ncmats80.lwbin
    sm_86_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/ncmats86.lwbin
    sm_87_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/ncmats87.lwbin
    sm_89_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/ncmats89.lwbin
    sm_90_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/ncmats90.lwbin

    cpp_files                                     += gpu/tests/lwca/lwdaxbar.cpp
    sm_60_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/xbar1_60.lwbin
    sm_61_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/xbar1_61.lwbin
    sm_70_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/xbar1_70.lwbin
    sm_72_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/xbar1_72.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/xbar1_75.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/xbar1_80.lwbin
    sm_86_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/xbar1_86.lwbin
    sm_87_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/xbar1_87.lwbin
    sm_89_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/xbar1_89.lwbin
    sm_90_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/xbar1_90.lwbin

    cpp_files                              += gpu/tests/lwca/lwrowhammer.cpp
    sm_50_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/lwrowhammer50.lwbin
    sm_52_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/lwrowhammer52.lwbin

    cpp_files$(ONLY_DGPU)                  += gpu/tests/lwca/lwda_mext.cpp
    sm_50_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/lwmext50.lwbin
    sm_52_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/lwmext52.lwbin
    sm_60_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/lwmext60.lwbin
    sm_70_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/lwmext70.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/lwmext75.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/lwmext80.lwbin
    sm_86_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/lwmext86.lwbin
    sm_89_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/lwmext89.lwbin
    sm_90_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/lwmext90.lwbin

    cpp_files                              += gpu/tests/lwca/lwda_grf.cpp
    sm_50_basic_lwbin_files$(ONLY_DGPU)    += $(LWBINS_DIR)/lwgrf50.lwbin

    cpp_files                                     += gpu/tests/lwca/dpstress.cpp
    sm_50_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/dpstrs50.lwbin
    sm_52_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/dpstrs52.lwbin
    sm_53_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/dpstrs53.lwbin
    sm_60_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/dpstrs60.lwbin
    sm_70_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/dpstrs70.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/dpstrs75.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/dpstrs80.lwbin
    sm_86_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/dpstrs86.lwbin
    sm_89_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/dpstrs89.lwbin
    sm_90_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/dpstrs90.lwbin

    cpp_files                                     += gpu/tests/lwca/lwda_linpack.cpp
    cpp_files                                     += gpu/tests/lwca/lwda_linpack_sgemm.cpp
    cpp_files                                     += gpu/tests/lwca/lwda_linpack_dgemm.cpp
    cpp_files                                     += gpu/tests/lwca/lwda_linpack_hgemm.cpp
    cpp_files                                     += gpu/tests/lwca/lwda_linpack_hmmagemm.cpp
    cpp_files                                     += gpu/tests/lwca/lwda_linpack_igemm.cpp
    cpp_files                                     += gpu/tests/lwca/lwda_linpack_immagemm.cpp
    cpp_files                                     += gpu/tests/lwca/lwda_linpack_cliff.cpp
ifeq "$(INCLUDE_LWDART)" "true"
    cpp_files                                     += gpu/tests/lwca/lwda_linpack_cask.cpp
    cpp_files                                     += gpu/tests/lwca/lwda_linpack_stress.cpp
endif
    sm_50_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/linpack50.lwbin
    sm_52_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/linpack52.lwbin
    sm_53_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/linpack53.lwbin
    sm_60_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/linpack60.lwbin
    sm_61_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/linpack61.lwbin
    sm_70_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/linpack70.lwbin
    sm_72_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/linpack72.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/linpack75.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/linpack80.lwbin
    sm_86_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/linpack86.lwbin
    sm_87_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/linpack87.lwbin
    sm_89_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/linpack89.lwbin
    sm_90_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/linpack90.lwbin

    cpp_files                                     += gpu/tests/lwca/ttuutils.cpp
    cpp_files                                     += gpu/tests/lwca/lwda_ttu_stress.cpp
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/ttustress75.lwbin
    sm_86_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/ttustress86.lwbin
    sm_87_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/ttustress87.lwbin
    sm_89_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/ttustress89.lwbin

    cpp_files                                     += gpu/tests/lwca/lwda_tiled_mats.cpp
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/tiledmats80.lwbin
    sm_90_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/tiledmats90.lwbin

    cpp_files                                     += gpu/tests/lwca/lwda_l1tag.cpp
    sm_70_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/l1tag70.lwbin
    sm_72_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/l1tag72.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/l1tag75.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/l1tag80.lwbin
    sm_86_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/l1tag86.lwbin
    sm_87_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/l1tag87.lwbin
    sm_89_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/l1tag89.lwbin
    sm_90_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/l1tag90.lwbin

    cpp_files                                     += gpu/tests/lwca/lwda_gxbar.cpp
    sm_90_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/gxbar90.lwbin

    cpp_files                                     += gpu/tests/lwca/lwda_cilp.cpp
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/cilpstress80.lwbin
    sm_86_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/cilpstress86.lwbin
    sm_89_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/cilpstress89.lwbin
    sm_90_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/cilpstress90.lwbin

    cpp_files                                     += gpu/tests/lwca/lwda_l2noc.cpp
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/l2noc80.lwbin
    sm_90_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/l2noc90.lwbin

    sm_50_basic_lwbin_files                       += $(LWBINS_DIR)/arch_dgemm_nt50.lwbin
    sm_50_basic_lwbin_files                       += $(LWBINS_DIR)/arch_sgemm_nt50.lwbin
    sm_53_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/arch_hgemm_nt53.lwbin
    sm_53_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/arch_cliff_A10_nt53.lwbin
    sm_53_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/arch_cliff_A30_nt53.lwbin
    sm_60_basic_lwbin_files                       += $(LWBINS_DIR)/arch_dgemm_nt60.lwbin
    sm_60_basic_lwbin_files                       += $(LWBINS_DIR)/arch_sgemm_nt60.lwbin
    sm_60_basic_lwbin_files                       += $(LWBINS_DIR)/arch_hgemm_nt60.lwbin
    sm_60_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_cliff_A10_nt60.lwbin
    sm_60_basic_lwbin_files                       += $(LWBINS_DIR)/arch_cliff_A30_nt60.lwbin
    sm_61_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_igemm_nt61.lwbin
    sm_70_basic_lwbin_files                       += $(LWBINS_DIR)/arch_dgemm_nt70.lwbin
    sm_70_basic_lwbin_files                       += $(LWBINS_DIR)/arch_sgemm_nt70.lwbin
    sm_70_basic_lwbin_files                       += $(LWBINS_DIR)/arch_hgemm_nt70.lwbin
    sm_70_basic_lwbin_files                       += $(LWBINS_DIR)/arch_igemm_nt70.lwbin
    sm_70_basic_lwbin_files                       += $(LWBINS_DIR)/arch_cliff_A30_nt70.lwbin
    sm_70_basic_lwbin_files                       += $(LWBINS_DIR)/arch_h884gemm_256x128_nt70.lwbin
    sm_70_basic_lwbin_files                       += $(LWBINS_DIR)/arch_h884gemm_128x128_nt70.lwbin
    sm_70_basic_lwbin_files                       += $(LWBINS_DIR)/arch_fp16_s884gemm_fp16_256x128_nt70.lwbin
    sm_70_basic_lwbin_files                       += $(LWBINS_DIR)/arch_fp16_s884gemm_fp16_128x128_nt70.lwbin
    sm_72_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/arch_int8_i8816gemm_256x128_nt72.lwbin
    sm_72_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/arch_int8_i8816gemm_128x128_nt72.lwbin
    sm_72_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/arch_int32_i8816gemm_256x128_nt72.lwbin
    sm_72_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/arch_int32_i8816gemm_128x128_nt72.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_dgemm_nt75.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_sgemm_nt75.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_hgemm_nt75.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_igemm_nt75.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_int8_i8816gemm_256x128_nt75.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_int8_i8816gemm_128x128_nt75.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_int32_i8816gemm_256x128_nt75.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_int32_i8816gemm_128x128_nt75.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_h884gemm_256x128_nt75.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_h884gemm_128x128_nt75.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_h1688gemm_256x128_nt75.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_h1688gemm_128x128_nt75.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_fp16_s884gemm_fp16_256x128_nt75.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_fp16_s884gemm_fp16_128x128_nt75.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_fp16_s1688gemm_fp16_256x128_nt75.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_fp16_s1688gemm_fp16_128x128_nt75.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_dgemm_nt80.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_sgemm_nt80.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_hgemm_nt80.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_igemm_nt80.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_int8_i16832gemm_256x128_nt80.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_int8_i16832gemm_128x128_nt80.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_int32_i16832gemm_256x128_nt80.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_int32_i16832gemm_128x128_nt80.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_h16816gemm_256x128_nt80.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_h16816gemm_128x128_nt80.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_fp16_s16816gemm_fp16_256x128_nt80.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/arch_fp16_s16816gemm_fp16_128x128_nt80.lwbin

    cpp_files                                     += gpu/tests/lwca/lwda_yield.cpp
    sm_70_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/yield70.lwbin
    sm_72_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/yield72.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/yield75.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/yield80.lwbin
    sm_86_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/yield86.lwbin
    sm_87_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/yield87.lwbin
    sm_89_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/yield89.lwbin
    sm_90_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/yield90.lwbin
    cpp_files                                     += gpu/tests/lwca/lwda_subctx.cpp
    sm_70_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/subctx70.lwbin
    sm_72_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/subctx72.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/subctx75.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/subctx80.lwbin
    sm_86_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/subctx86.lwbin
    sm_87_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/subctx87.lwbin
    sm_89_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/subctx89.lwbin
    sm_90_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/subctx90.lwbin

    cpp_files                                     += gpu/tests/lwca/lwda_stress_fermi.cpp
    sm_50_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/cstrsf50.lwbin
    sm_52_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/cstrsf52.lwbin
    sm_53_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/cstrsf53.lwbin
    sm_60_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/cstrsf60.lwbin
    sm_70_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/cstrsf70.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/cstrsf75.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/cstrsf80.lwbin
    sm_86_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/cstrsf86.lwbin
    sm_87_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/cstrsf87.lwbin
    sm_89_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/cstrsf89.lwbin
    sm_90_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/cstrsf90.lwbin

    cpp_files                                     += gpu/tests/lwca/lwdarndm.cpp
    cpp_files                                     += gpu/tests/lwca/lwdarndm_atom.cpp
    cpp_files                                     += gpu/tests/lwca/lwdarndm_branch.cpp
    cpp_files                                     += gpu/tests/lwca/lwdarndm_debug.cpp
    cpp_files                                     += gpu/tests/lwca/lwdarndm_gwc.cpp
    cpp_files                                     += gpu/tests/lwca/lwdarndm_matrixinit.cpp
    cpp_files                                     += gpu/tests/lwca/lwdarndm_seq.cpp
    cpp_files                                     += gpu/tests/lwca/lwdarndm_stress.cpp
    cpp_files                                     += gpu/tests/lwca/lwdarndm_surface.cpp
    cpp_files                                     += gpu/tests/lwca/lwdarndm_texture.cpp
    cpp_files                                     += gpu/tests/lwca/lwgsurf.cpp
    sm_50_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/crnd50.lwbin
    sm_52_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/crnd52.lwbin
    sm_53_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/crnd53.lwbin
    sm_60_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/crnd60.lwbin
    sm_61_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/crnd61.lwbin
    sm_70_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/crnd70.lwbin
    sm_72_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/crnd72.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/crnd75.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/crnd80.lwbin
    sm_86_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/crnd86.lwbin
    sm_87_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/crnd87.lwbin
    sm_89_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/crnd89.lwbin
    sm_90_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/crnd90.lwbin

    basic_release_files                           += gpu/js/lwr_comm.h

    cpp_files$(ONLY_PC)                           += gpu/tests/lwca/bitlanetest.cpp
    internal_release_files$(ONLY_PC)              += gpu/js/bitlane.js
    sm_50_internal_lwbin_files$(ONLY_PC)          += $(LWBINS_DIR)/bitlan50.lwbin
    sm_52_internal_lwbin_files$(ONLY_PC)          += $(LWBINS_DIR)/bitlan52.lwbin
    sm_60_internal_lwbin_files$(ONLY_PC)          += $(LWBINS_DIR)/bitlan60.lwbin
    sm_70_internal_lwbin_files$(ONLY_PC)          += $(LWBINS_DIR)/bitlan70.lwbin

    cpp_files                                     += gpu/tests/lwca/lwdaeccl2.cpp
    sm_60_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/eccl260.lwbin
    sm_70_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/eccl270.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/eccl275.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/eccl280.lwbin

    cpp_files                                     += gpu/tests/lwca/eccsm.cpp
    sm_60_basic_lwbin_files$(ONLY_PC)             += $(LWBINS_DIR)/eccsm60.lwbin

    cpp_files                                     += gpu/tests/lwca/eccsmshm.cpp
    sm_60_basic_lwbin_files$(ONLY_PC)             += $(LWBINS_DIR)/eccshm60.lwbin
    sm_70_basic_lwbin_files$(ONLY_PC)             += $(LWBINS_DIR)/eccshm70.lwbin

    cpp_files                                     += gpu/tests/lwca/ecctex.cpp
    sm_60_basic_lwbin_files$(ONLY_PC)             += $(LWBINS_DIR)/ecctex60.lwbin

    cpp_files                                     += gpu/tests/lwca/lwda_radixsort.cpp
    sm_50_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/radixsort50.lwbin
    sm_52_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/radixsort52.lwbin
    sm_60_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/radixsort60.lwbin
    sm_70_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/radixsort70.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/radixsort75.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/radixsort80.lwbin
    sm_86_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/radixsort86.lwbin
    sm_89_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/radixsort89.lwbin
    sm_90_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/radixsort90.lwbin
    $(o_dir)/lwda_radixsort$(obj_suffix): include_dirs += gpu/tests/lwca/radixsort

    sm_50_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/lwsurf50.lwbin
    sm_52_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/lwsurf52.lwbin
    sm_53_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/lwsurf53.lwbin
    sm_60_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/lwsurf60.lwbin
    sm_70_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/lwsurf70.lwbin
    sm_72_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/lwsurf72.lwbin
    sm_75_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/lwsurf75.lwbin
    sm_80_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/lwsurf80.lwbin
    sm_86_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/lwsurf86.lwbin
    sm_87_basic_lwbin_files$(ONLY_ARM)$(ONLY_IGPU)+= $(LWBINS_DIR)/lwsurf87.lwbin
    sm_89_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/lwsurf89.lwbin
    sm_90_basic_lwbin_files$(ONLY_DGPU)           += $(LWBINS_DIR)/lwsurf90.lwbin

    cpp_files                                     += gpu/tests/lwca/merc_regfile.cpp
    sm_90_basic_lwbin_files                       += $(LWBINS_DIR)/rfstress90.lwbin

    basic_release_files$(ONLY_PC)                += gpu/js/lwdatest.js

    basic_release_files                          += $(o_dir)/lwca.bin

    add_basic_lwbins = $(foreach sm_arch,$(1), $($(sm_arch)_basic_lwbin_files))
    add_internal_lwbins = $(foreach sm_arch,$(1), $($(sm_arch)_internal_lwbin_files))

    basic_lwbin_files += $(call add_basic_lwbins,$(LWDA_REL_ARCHS))
    internal_lwbin_files += $(call add_internal_lwbins,$(LWDA_REL_ARCHS))
    ifeq "$(INCLUDE_LWDA_DEV_ARCHS)" "true"
        basic_lwbin_files += $(call add_basic_lwbins,$(LWDA_DEV_ARCHS))
        internal_lwbin_files += $(call add_internal_lwbins,$(LWDA_DEV_ARCHS))
    endif
    internal_lwbin_files :=
endif

ifeq "$(INCLUDE_OGL)" "true"
    cpp_files$(ONLY_PC)             += opengl/tests/applegl.cpp
    cpp_files                       += opengl/tests/astccache.cpp
    cpp_files                       += opengl/tests/glrandom.cpp
    cpp_files                       += opengl/tests/lwprhelper.cpp
    cpp_files                       += opengl/tests/gllwpr.cpp
    cpp_files                       += opengl/tests/glrhelp.cpp
    cpp_files                       += opengl/tests/glstress.cpp
    cpp_files                       += opengl/tests/glcombust.cpp
    cpp_files                       += opengl/tests/glcompute.cpp
    cpp_files                       += opengl/tests/glcompute_default_shaders.cpp
    cpp_files                       += opengl/tests/gldirected.cpp
    cpp_files                       += opengl/tests/glinferno.cpp
    cpp_files                       += opengl/tests/vertexfs.cpp
    cpp_files                       += opengl/tests/powertst.cpp
    cpp_files                       += opengl/tests/glhist.cpp
    cpp_files                       += opengl/tests/glfuzz.cpp
    cpp_files                       += opengl/tests/r_fog.cpp
    cpp_files                       += opengl/tests/r_frag.cpp
    cpp_files                       += opengl/tests/r_geom.cpp
    cpp_files                       += opengl/tests/r_light.cpp
    cpp_files                       += opengl/tests/r_misc.cpp
    cpp_files                       += opengl/tests/r_primmo.cpp
    cpp_files                       += opengl/tests/r_tex.cpp
    cpp_files                       += opengl/tests/r_vx.cpp
    cpp_files                       += opengl/tests/r_vxfmt.cpp
    cpp_files                       += opengl/tests/r_pixels.cpp

    cpp_files                       += opengl/tests/r_gpupgm.cpp
    cpp_files                       += opengl/tests/gpupgm.cpp
    cpp_files                       += opengl/tests/vxgpupgm.cpp
    cpp_files                       += opengl/tests/frgpupgm.cpp
    cpp_files                       += opengl/tests/gmgpupgm.cpp
    cpp_files                       += opengl/tests/tessgpupgm.cpp

    ifeq "$(INCLUDE_VULKAN)" "true"
        cpp_files                   += vulkan/tests/vktriangle.cpp
        cpp_files                   += vulkan/tests/vkfusion.cpp
        cpp_files                   += vulkan/tests/vkfusion/vkfusion_job.cpp
        cpp_files                   += vulkan/tests/vkfusion/vkfusion_graphics.cpp
        cpp_files                   += vulkan/tests/vkfusion/vkfusion_raytracing.cpp
        cpp_files                   += vulkan/tests/vkfusion/vkfusion_mats.cpp
        cpp_files                   += vulkan/tests/vkstress.cpp
        cpp_files                   += vulkan/tests/vkstress/vkstressray.cpp
    endif

    ifeq "$(INCLUDE_ASTC_FILES)" "true"
        astc_files                  += gpu/tests/traces/astc/bird.astc
        astc_files                  += gpu/tests/traces/astc/clouds.astc
        astc_files                  += gpu/tests/traces/astc/lwbes.astc
        astc_files                  += gpu/tests/traces/astc/lwrves.astc
        astc_files                  += gpu/tests/traces/astc/drops.astc
        astc_files                  += gpu/tests/traces/astc/festival.astc
        astc_files                  += gpu/tests/traces/astc/forest.astc
        astc_files                  += gpu/tests/traces/astc/fractals.astc
        astc_files                  += gpu/tests/traces/astc/lines.astc
        astc_files                  += gpu/tests/traces/astc/logo.astc
        astc_files                  += gpu/tests/traces/astc/smudges.astc
        astc_files                  += gpu/tests/traces/astc/splats.astc
        astc_files                  += gpu/tests/traces/astc/squares.astc
        astc_files                  += gpu/tests/traces/astc/tiedie.astc
        basic_release_files         += $(o_dir)/astc.bin
    endif
    ifneq (,$(filter $(BUILD_OS), sim winsim))
        ifeq "$(INCLUDE_LWOGTEST)" "true"
            internal_release_files      += gpu/js/lwogtest.js
            cpp_files                   += opengl/tests/lwogtest.cpp
            CLIENTPATHS                 += //sw/apps/gpu/drivers/opengl/include/...
            CLIENTPATHS                 += //sw/apps/gpu/drivers/opengl/lwogtest/...
        else
            ifeq "$(STUB_LWOGTEST)" "true"
                internal_release_files  += gpu/js/lwogtest.js
                cpp_files               += opengl/tests/lwogtest.cpp
            endif
        endif
    endif

    basic_release_files             += gpu/js/glr_comm.h
    basic_release_files             += gpu/js/glrandom.js
    internal_release_files          += gpu/js/glr_fa.js

endif  # INCLUDE_OGL

basic_release_files$(ONLY_PC)       += gpu/js/tunetrim.js
basic_release_files$(ONLY_PC)       += gpu/js/tunevolt.js

# files distributed to lwstomers but not part of Official/Qual
internal_release_files$(ONLY_PC)    += gpu/js/speedo.js
unpackaged_release_files$(ONLY_PC)  += $(PKG_OUT_DIR)/$(VERSION).speedo.jse

bypass_files                         += $(o_dir)/bypass.bin
bypass_files                         += $(o_dir)/bypass.INTERNAL.bin
unpackaged_release_files             += $(patsubst bypass%bin,$(PKG_OUT_DIR)/$(VERSION).bypass%bin,$(notdir $(bypass_files)))

internal_release_files              += gpu/js/heattest.js
internal_release_files              += gpu/js/tcycle.js

internal_release_files$(ONLY_PC)             += gpu/js/checker.js
internal_release_files                       += gpu/js/pattern.js
internal_release_files                       += gpu/js/rerun.js
internal_release_files                       += gpu/js/shmob.js
internal_release_files                       += gpu/js/smplrmt.js
basic_release_files                          += gpu/js/gshmoo.js
internal_release_files                       += gpu/js/gshmoo2.js
internal_release_files$(ONLY_DGPU)           += gpu/js/shmoo.txt
internal_release_files$(ONLY_DGPU)           += gpu/js/vkstress_shmoo.txt
internal_release_files$(ONLY_ARM)$(ONLY_IGPU)+= gpu/js/tegra_gpu_shmoo.txt
internal_release_files$(ONLY_DGPU)           += gpu/js/xvereset.js
internal_release_files$(ONLY_DGPU)           += gpu/js/slifstest.js

ifeq "$(INCLUDE_LWDA)" "true"
    install : $(o_dir)/lwca.bin
    zip_pvs_nodep : $(o_dir)/lwca.bin
    all_gen_files += $(o_dir)/lwca.bin

    ifneq "$(internal_lwbin_files)" ""
        install : $(o_dir)/lwca.INTERNAL.bin
        zip_pvs_nodep : $(o_dir)/lwca.INTERNAL.bin
        all_gen_files += $(o_dir)/lwca.INTERNAL.bin
        internal_release_files += $(o_dir)/lwca.INTERNAL.bin
    endif
endif

$(o_dir)/lwca.bin : $(basic_lwbin_files)
ifeq "$(REDACT_STRINGS)" "true"
$(o_dir)/lwca.bin : $(REDACT_RANDOMS) $(GUARDWORD_FILE) $(GUARDWORD_IGNORE)
endif
	@$(ECHO) "Creating lwca.bin"
	@$(TAR) $(POSIX_TAR) -czf $@ -C $(LWBINS_DIR)/ $(notdir $(basic_lwbin_files))
ifeq "$(REDACT_STRINGS)" "true"
	$(Q)$(REDACT_SCRIPT) --no-delete --guardword "$(DRIVERS_DIR)/common/build" "$(GUARDWORD_FILE)" "$(ENABLE_GUARDWORD)" $@
endif

$(o_dir)/lwca.INTERNAL.bin : $(internal_lwbin_files)
ifeq "$(REDACT_STRINGS)" "true"
$(o_dir)/lwca.INTERNAL.bin : $(REDACT_RANDOMS)
endif
	@$(ECHO) "Creating lwca.INTERNAL.bin"
	@$(TAR) $(POSIX_TAR) -czf $@ -C $(LWBINS_DIR)/ $(notdir $(internal_lwbin_files))
ifeq "$(REDACT_STRINGS)" "true"
	$(Q)$(REDACT_SCRIPT) --no-delete $@
endif

ifeq "$(INCLUDE_OGL)" "true"
ifeq "$(INCLUDE_ASTC_FILES)" "true"
install       :  $(o_dir)/astc.bin
zip           :  $(o_dir)/astc.bin
zip_pvs_nodep :  $(o_dir)/astc.bin
all_gen_files += $(o_dir)/astc.bin
endif
endif

$(o_dir)/astc.bin : $(astc_files)
	@$(ECHO) "Creating astc.bin"
	@$(TAR) $(POSIX_TAR) -czf $@ -C gpu/tests/traces/astc $(notdir $(astc_files))

$(o_dir)/%.264 : gpu/tests/traces/%.264
	$(Q)$(CP_IF_CHANGED) $(CP_RM_DEST) $< $@
$(o_dir)/%.jpg : gpu/tests/traces/%.jpg
	$(Q)$(CP_IF_CHANGED) $(CP_RM_DEST) $< $@
$(o_dir)/%.vc1 : gpu/tests/traces/%.vc1
	$(Q)$(CP_IF_CHANGED) $(CP_RM_DEST) $< $@
$(o_dir)/%.vp8 : gpu/tests/traces/%.vp8
	$(Q)$(CP_IF_CHANGED) $(CP_RM_DEST) $< $@
$(o_dir)/%.mp2 : gpu/tests/traces/%.mp2
	$(Q)$(CP_IF_CHANGED) $(CP_RM_DEST) $< $@

ifeq "" "$(ONLY_ARM)$(ONLY_IGPU)"
install : $(o_dir)/tsec.bin
zip_pvs_nodep : $(o_dir)/tsec.bin
endif

all_gen_files$(ONLY_ARM)$(ONLY_IGPU)+= $(o_dir)/tsec.bin $(internal_tsec_files)
$(o_dir)/tsec.bin : $(internal_tsec_files)
	@$(ECHO) "Creating tsec.bin"
	@$(TAR) $(POSIX_TAR) -czf $@ -C $(o_dir) $(notdir $(internal_tsec_files))

gen_js_files += $(o_dir)/bypass.bin $(o_dir)/bypass.INTERNAL.bin
$(o_dir)/bypass%bin $(o_dir)/bypass.INTERNAL%bin: $(MODS_DIR_UNIX)/tools/genfiles.py FORCE
	@$(ECHO) "Creating bypass files"
	@$(PYTHON3) $(MODS_DIR)/tools/genfiles.py        \
             -b$(o_dir)/bypass$*bin                 \
             g_Version,$(VERSION)                   \
             g_Changelist,$(CHANGELIST)

$(o_dir)/%.raw : gpu/tests/traces/%.raw
	$(Q)$(CP_IF_CHANGED) $(CP_RM_DEST) $< $@
$(o_dir)/%.hcb : gpu/tests/traces/%.hcb
	$(Q)$(CP_IF_CHANGED) $(CP_RM_DEST) $< $@
$(o_dir)/%.vcb : gpu/tests/traces/%.vcb
	$(Q)$(CP_IF_CHANGED) $(CP_RM_DEST) $< $@

basic_cde_files                += $(o_dir)/compressed_320x240.raw
basic_cde_files                += $(o_dir)/compressed_320x240.hcb
basic_cde_files                += $(o_dir)/compressed_320x240.vcb
basic_cde_files                += $(o_dir)/compressed_640x480.raw
basic_cde_files                += $(o_dir)/compressed_640x480.hcb
basic_cde_files                += $(o_dir)/compressed_640x480.vcb
basic_cde_files                += $(o_dir)/compressed_800x600.raw
basic_cde_files                += $(o_dir)/compressed_800x600.hcb
basic_cde_files                += $(o_dir)/compressed_800x600.vcb
basic_release_files$(ONLY_ARM)$(ONLY_IGPU)+= $(o_dir)/cdeimages.bin

ifneq (,$(filter $(BUILD_ARCH), armv7 aarch64))
install : $(o_dir)/cdeimages.bin
zip_pvs_nodep : $(o_dir)/cdeimages.bin
endif

all_gen_files$(ONLY_ARM)$(ONLY_IGPU)+= $(o_dir)/cdeimages.bin $(basic_cde_files)
$(o_dir)/cdeimages.bin : $(basic_cde_files)
	@$(ECHO) "Creating cdeimages.bin"
	@$(TAR) $(POSIX_TAR) -czf $@ -C $(o_dir) $(notdir $(basic_cde_files))
