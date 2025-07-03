# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

#-------------------------------------------------------------------------------
# MODS mdiag tests library make file.
# See //sw/<tree>/diag/mods/makedefs.inc for more information about the MODS
# makefiles.
#-------------------------------------------------------------------------------

INCLUDE_GPU ?= true
INCLUDE_MDIAGUTL ?= true

MODS_DIR = ..
dll_build = false
base_name = libmdiag

include $(MODS_DIR)/makedefs.inc

# Do not install library to improve build times
install_library := false

ifeq "$(toolchain)" "msvc"
    CFLAGS += /EHsc
    defines += -DWIN32 -D_WINDOWS
else
    CPPFLAGS += -Werror
endif
ifneq "$(INCLUDE_GPU)" "true"
    $(error mdiag support with INCLUDE_GPU!=true has been retired)
endif

ifeq "$(toolchain)" "gnu"
    CFLAGS += -fvisibility=hidden
endif

ifneq (,$(filter $(BUILD_OS), winsim winmfg win32))
    INCLUDE_MDIAGUTL = false
endif

ifeq "$(BUILD_ARCH)" "ppc64le"
    INCLUDE_MDIAGUTL = false
endif

# Some of the shared makefiles depend on LW_SOURCE, which may not be set
ifdef CYGWIN_MAKE
    LW_SOURCE ?= $(shell $(MODS_DIR_UNIX)/tools/abs2rel.sh "$(MODS_DIR_UNIX)/../..")
else
    LW_SOURCE ?= $(abspath $(MODS_DIR))/../..
endif
LW_RMSHARED_DISABLE_LWLOCKVAL := 1
LW_RMSHARED_DISABLE_LWTLS := 1
LW_RMSHARED_DISABLE_RESSERV := 1
include $(LW_SOURCE)/drivers/common/shared/shared.lwmk
# Append the shared sources/includes/defines to the main variables

defines += $(addprefix -D,$(LW_RMSHARED_DEFINES))
include_dirs += $(LW_RMSHARED_INCLUDES)
c_files += $(LW_RMSHARED_SOURCES)

defines += -DINCLUDE_GPU=1
ifeq "$(INCLUDE_LWSWITCH)" "true"
    defines += -DINCLUDE_LWSWITCH
endif
ifeq "$(INCLUDE_LWLINK)" "true"
    defines += -DINCLUDE_LWLINK
endif
ifeq "$(INCLUDE_MDIAGUTL)" "true"
    defines += -DINCLUDE_MDIAGUTL=1
endif

defines += -DMDIAG_LIBRARY
defines += -DXML_STATIC
defines += -DINCLUDE_STD_NAMESPACE

include_dirs += $(MODS_DIR)
include_dirs += $(MODS_DIR)/../js1.7

include_dirs += ../../../drivers/common/inc
include_dirs += ../../../drivers/common/shared/inc
include_dirs += ../../../drivers/common/inc/swref
include_dirs += ../../../drivers/common/inc/hwref
include_dirs += ../../../sdk/lwpu/inc
include_dirs += ../../../drivers/resman/interface
include_dirs += ../../../drivers/resman/kernel/inc
include_dirs += ../../../drivers/resman/inc/libraries
include_dirs += ../../../drivers/resman/arch/lwalloc/mods/inc
include_dirs += ../../../drivers/resman/arch/lwalloc/common/inc
ifeq "$(INCLUDE_LWSWITCH)" "true"
    include_dirs += ../../../drivers/lwswitch/interface
endif
ifeq "$(INCLUDE_LWLINK)" "true"
    include_dirs += ../../../drivers/lwlink/interface
endif
include_dirs += $(MODS_DIR)/../utils
include_dirs += advschd
include_dirs += advschd/unittest
include_dirs += $(BOOST_DIR)

ifeq "$(base_os)" "linux"
    ifneq (,$(filter $(BUILD_ARCH), amd64 ppc64le))
        include_dirs += $(BUILD_TOOLS_DIR)/libpython-3.6.3/Include
        include_dirs += $(BUILD_TOOLS_DIR)/pybind11-2.2.3/include
    endif
endif

# Always include the OS's modsinc.h
force_include_files = $(base_os)/modsinc.h
force_include_files += core/include/jsprotect.h

cpp_files += advschd/pmaction.cpp
cpp_files += advschd/pmchan.cpp
cpp_files += advschd/pmvaspace.cpp
cpp_files += advschd/pmvftest.cpp
cpp_files += advschd/pmsmcengine.cpp
cpp_files += advschd/pmchwrap.cpp
cpp_files += advschd/pmevent.cpp
cpp_files += advschd/pmevntmn.cpp
cpp_files += advschd/pmgilder.cpp
cpp_files += advschd/pmparser.cpp
cpp_files += advschd/pmpotevt.cpp
cpp_files += advschd/pmsurf.cpp
cpp_files += advschd/pmtest.cpp
cpp_files += advschd/pmtrigg.cpp
cpp_files += advschd/pmutils.cpp
cpp_files += advschd/policymn.cpp
cpp_files += pm_lwchn.cpp
cpp_files += gpu/dmabuf.cpp
cpp_files += gpu/lwsurf.cpp
cpp_files += gpu/surfmgr.cpp
cpp_files += gpu/zlwll.cpp
cpp_files += iommu/iommudrv.cpp
cpp_files += iommu/iommu_cpumodeldrv.cpp
cpp_files += iommu/smmudrv.cpp
cpp_files += resource/lwgpu/crcchain.cpp
cpp_files += resource/lwgpu/dmagf100ce.cpp
cpp_files += resource/lwgpu/dmavoltace.cpp
cpp_files += resource/lwgpu/dmaturingce.cpp
cpp_files += resource/lwgpu/dmaloader.cpp
cpp_files += resource/lwgpu/dmard.cpp
cpp_files += resource/lwgpu/dmasurfr.cpp
cpp_files += resource/lwgpu/gpucrccalc.cpp
cpp_files += resource/lwgpu/sclnrd.cpp
cpp_files += resource/lwgpu/SurfaceDumper.cpp
cpp_files += resource/lwgpu/SurfaceReader.cpp
cpp_files += resource/lwgpu/surfasm.cpp
cpp_files += resource/lwgpu/verif/CrcChecker.cpp
cpp_files += resource/lwgpu/verif/CrcSelfgildChecker.cpp
cpp_files += resource/lwgpu/verif/DmaUtils.cpp
cpp_files += resource/lwgpu/verif/DumpUtils.cpp
cpp_files += resource/lwgpu/verif/GpuVerif.cpp
cpp_files += resource/lwgpu/verif/InterruptChecker.cpp
cpp_files += resource/lwgpu/verif/LwlCounters.cpp
cpp_files += resource/lwgpu/verif/ProfileKey.cpp
cpp_files += resource/lwgpu/verif/ReferenceChecker.cpp
cpp_files += resource/lwgpu/verif/SurfBufInfo.cpp
cpp_files += resource/lwgpu/verif/VerifUtils.cpp
cpp_files += tests/conlwrrentTest.cpp
cpp_files += tests/loopingtest.cpp
cpp_files += tests/test.cpp
cpp_files += tests/test_state_report.cpp
cpp_files += tests/testdir.cpp
cpp_files += tests/gpu/lwgpu_basetest.cpp
cpp_files += tests/gpu/lwgpu_ch_helper.cpp
cpp_files += tests/gpu/lwgpu_single.cpp
cpp_files += tests/gpu/lwgpu_subchannel.cpp
cpp_files += tests/gpu/lwgpu_tsg.cpp
cpp_files += tests/gpu/2d/lwgpu_2dtest.cpp
cpp_files += tests/gpu/2d/t5d.cpp
cpp_files += tests/gpu/2d/verif2d/classobj.cpp
cpp_files += tests/gpu/2d/verif2d/func.cpp
cpp_files += tests/gpu/2d/verif2d/gpu.cpp
cpp_files += tests/gpu/2d/verif2d/lex.cpp
cpp_files += tests/gpu/2d/verif2d/nodes.cpp
cpp_files += tests/gpu/2d/verif2d/parse.cpp
cpp_files += tests/gpu/2d/verif2d/surf.cpp
cpp_files += tests/gpu/2d/verif2d/twod.cpp
cpp_files += tests/gpu/2d/verif2d/v2d.cpp
cpp_files += tests/gpu/2d/verif2d/verif2d.cpp
cpp_files += tests/gpu/disp/vga_rma.cpp
cpp_files += tests/gpu/disp/vstack.cpp
cpp_files += tests/gpu/fb/basic_read_write.cpp
cpp_files += tests/gpu/fb/deadlock_cpu.cpp
cpp_files += tests/gpu/fb/enhanced_read_write.cpp
cpp_files += tests/gpu/fb/fb_common.cpp
cpp_files += tests/gpu/fb/fb_hub_mmu_ilwalidate.cpp
cpp_files += tests/gpu/fb/host_all_size.cpp
cpp_files += tests/gpu/fb/max_memory.cpp
cpp_files += tests/gpu/fb/max_memory2.cpp
cpp_files += tests/gpu/fb/no_swizzle.cpp
cpp_files += tests/gpu/fb/partition_white_box.cpp
cpp_files += tests/gpu/fb/read_write_ordering.cpp
cpp_files += tests/gpu/fb/sparsetex_copy_amap.cpp
cpp_files += tests/gpu/fb/sysmembar_client_cpu.cpp
cpp_files += tests/gpu/fb/sysmembar_l2_l2b.cpp
cpp_files += tests/gpu/fb/sysmembar_l2_l2b_no_p2p.cpp
cpp_files += tests/gpu/fb/amap/FBConfig.cpp
cpp_files += tests/gpu/fb/amap/FermiAddressMapping.cpp
cpp_files += tests/gpu/fb/amap/PhysicalAddress.cpp
cpp_files += tests/gpu/host/bl_remapper.cpp
cpp_files += tests/gpu/host/fb_flush.cpp
cpp_files += tests/gpu/host/frontdoor.cpp
cpp_files += tests/gpu/host/host_bar1_perf.cpp
cpp_files += tests/gpu/host/host_test_util.cpp
cpp_files += tests/gpu/host/semaphore_bashing.cpp
cpp_files += tests/gpu/host/fermi/fermi_host_bar1_perf.cpp
cpp_files += tests/gpu/host/kepler/kepler_bl_remapper.cpp
cpp_files += tests/gpu/host/pascal/pascal_bl_remapper.cpp
cpp_files += tests/gpu/host/turing/turing_bl_remapper.cpp
cpp_files += tests/gpu/misc/check_clamp.cpp
cpp_files += tests/gpu/misc/check_connect.cpp
cpp_files += tests/gpu/misc/check_dedicated_overt_gk110.cpp
cpp_files += tests/gpu/misc/check_tach_count_gk110.cpp
cpp_files += tests/gpu/misc/check_gsp2host_status_idle.cpp
cpp_files += tests/gpu/misc/check_lce2host_status_idle.cpp
cpp_files += tests/gpu/misc/deepL1_gen3.cpp
cpp_files += tests/gpu/misc/elpg_powergv_kepler.cpp
cpp_files += tests/gpu/misc/fbpa_cattrip.cpp
cpp_files += tests/gpu/misc/gc6plus.cpp
cpp_files += tests/gpu/misc/gc6plus_helpers.cpp
cpp_files += tests/gpu/misc/gc6plus_maxwell.cpp
cpp_files += tests/gpu/misc/gc6plus_misc.cpp
cpp_files += tests/gpu/misc/goodn_rst_reinit.cpp
cpp_files += tests/gpu/misc/dvco_sanity.cpp
cpp_files += tests/gpu/misc/idle_flipped.cpp
cpp_files += tests/gpu/misc/ad10x_hotrst_reinit.cpp
cpp_files += tests/gpu/misc/hotrst_reinit.cpp
cpp_files += tests/gpu/misc/failsafe_overt.cpp
cpp_files += tests/gpu/misc/therm_overt_legacy.cpp
cpp_files += tests/gpu/misc/therm_overt_latch.cpp
cpp_files += tests/gpu/misc/xbashlwvdd_overt.cpp
cpp_files += tests/gpu/misc/i2cslave_fermi.cpp
cpp_files += tests/gpu/misc/ad10x_jtag_reg_reset_check.cpp
cpp_files += tests/gpu/misc/jtag_reg_reset_check.cpp
cpp_files += tests/gpu/misc/jtag2icd_lwdec.cpp
cpp_files += tests/gpu/misc/jtag2icd_lwenc.cpp
cpp_files += tests/gpu/misc/jtag2icd_pwr.cpp
cpp_files += tests/gpu/misc/l1_substate.cpp
cpp_files += tests/gpu/misc/maxwell_rtl_clocks.cpp
cpp_files += tests/gpu/misc/memsys_island.cpp
cpp_files += tests/gpu/misc/misctestlist.cpp
cpp_files += tests/gpu/misc/MultiTexReg.cpp
cpp_files += tests/gpu/misc/notpll_test_kepler.cpp
cpp_files += tests/gpu/misc/powerCtrlTest_maxwell.cpp
cpp_files += tests/gpu/misc/VR.cpp
cpp_files += tests/gpu/misc/t_vr_input_side_sensing.cpp
cpp_files += tests/gpu/misc/regmap_helper.cpp
cpp_files += tests/gpu/misc/RegTestGpu.cpp
cpp_files += tests/gpu/misc/smartfan_sci.cpp
cpp_files += tests/gpu/misc/stop_clocks.cpp
cpp_files += tests/gpu/misc/thermal_shutdown_latch.cpp
cpp_files += tests/gpu/misc/thermalctrl_maxwell.cpp
cpp_files += tests/gpu/misc/pwr_tsense.cpp
cpp_files += tests/gpu/misc/fb_util.cpp
cpp_files += tests/gpu/misc/uart2icd_pwr.cpp
cpp_files += tests/gpu/misc/unalmem_kepler.cpp
cpp_files += tests/gpu/misc/gk104/unalmem_kepler_gk104.cpp
cpp_files += tests/gpu/misc/vgaBase.cpp
cpp_files += tests/gpu/misc/vid_pwm.cpp
cpp_files += tests/gpu/misc/vld_fs_units.cpp
cpp_files += tests/gpu/misc/wsp2pwr_conn.cpp
cpp_files += tests/gpu/mmu/vpr.cpp
cpp_files += tests/gpu/trace_3d/ampere_support.cpp
cpp_files += tests/gpu/trace_3d/ArmVAPrintf.cpp
cpp_files += tests/gpu/trace_3d/bufferdumper.cpp
cpp_files += tests/gpu/trace_3d/ce_support.cpp
cpp_files += tests/gpu/trace_3d/hopper_support.cpp
ifneq "$(toolchain)" "msvc"
    cpp_files += tests/gpu/trace_3d/compbits.cpp
    cpp_files += tests/gpu/trace_3d/compbits_ut.cpp
    cpp_files += tests/gpu/trace_3d/compbits_lib.cpp
    cpp_files += tests/gpu/trace_3d/compbits_maxwell.cpp
    cpp_files += tests/gpu/trace_3d/compbits_pascal.cpp
endif
cpp_files += tests/gpu/trace_3d/fermi_compute_support.cpp
cpp_files += tests/gpu/trace_3d/fermi_support.cpp
cpp_files += tests/gpu/trace_3d/gf11x_support.cpp
cpp_files += tests/gpu/trace_3d/gputrace.cpp
cpp_files += tests/gpu/trace_3d/kepler_compute_support.cpp
cpp_files += tests/gpu/trace_3d/volta_compute_support.cpp
cpp_files += tests/gpu/trace_3d/kepler_support.cpp
cpp_files += tests/gpu/trace_3d/mailbox.cpp
cpp_files += tests/gpu/trace_3d/masfermi.cpp
cpp_files += tests/gpu/trace_3d/maxwell_support.cpp
cpp_files += tests/gpu/trace_3d/blackwell_support.cpp
cpp_files += tests/gpu/trace_3d/msdec_support.cpp
cpp_files += tests/gpu/trace_3d/pascal_support.cpp
cpp_files += tests/gpu/trace_3d/peer_mod.cpp
cpp_files += tests/gpu/trace_3d/plgnsprt.cpp
cpp_files += tests/gpu/trace_3d/plugin_dispmgr.cpp
cpp_files += tests/gpu/trace_3d/plugin_jsmgr.cpp
cpp_files += tests/gpu/trace_3d/pm_t3d.cpp
cpp_files += tests/gpu/trace_3d/pmu_support.cpp
cpp_files += tests/gpu/trace_3d/selfgild.cpp
cpp_files += tests/gpu/trace_3d/slisurf.cpp
cpp_files += tests/gpu/trace_3d/p2psurf.cpp
cpp_files += tests/gpu/trace_3d/t3dmssge.cpp
cpp_files += tests/gpu/trace_3d/t3dscssr.cpp
cpp_files += tests/gpu/trace_3d/t3plugin.cpp
cpp_files += tests/gpu/trace_3d/teegroups.cpp
cpp_files += tests/gpu/trace_3d/trace_3d.cpp
cpp_files += tests/gpu/trace_3d/t3dparams.cpp
cpp_files += tests/gpu/trace_3d/parsefile.cpp
cpp_files += tests/gpu/trace_3d/pe_configurefile.cpp
cpp_files += tests/gpu/trace_3d/tracechan.cpp
cpp_files += tests/gpu/trace_3d/watermarkimpl.cpp
cpp_files += tests/gpu/trace_3d/tracesubchan.cpp
cpp_files += tests/gpu/trace_3d/tracemod.cpp
cpp_files += tests/gpu/trace_3d/traceop.cpp
cpp_files += tests/gpu/trace_3d/tracerel.cpp
cpp_files += tests/gpu/trace_3d/tracetsg.cpp
cpp_files += tests/gpu/trace_3d/turing_support.cpp
cpp_files += tests/gpu/trace_3d/twod_support.cpp
#cpp_files += tests/gpu/trace_3d/utsprt.cpp
cpp_files += tests/gpu/trace_3d/volta_support.cpp
cpp_files += tests/gpu/trace_3d/vp2_support.cpp
#cpp_files += tests/gpu/unittest/uttrace3dtest.cpp
#cpp_files += tests/gpu/unittest/utparsesinglecommand.cpp
#cpp_files += tests/gpu/unittest/utparserelocsize.cpp
#cpp_files += tests/gpu/unittest/utparseuint32.cpp
#cpp_files += tests/gpu/unittest/utparseuint64.cpp
cpp_files += utils/buf.cpp
cpp_files += utils/buffinfo.cpp
cpp_files += utils/crc.cpp
cpp_files += utils/cregctrl.cpp
cpp_files += utils/cregexcl.cpp
cpp_files += utils/gr_random.cpp
cpp_files += utils/GZFileIO.cpp
cpp_files += utils/kepler_tex.cpp
cpp_files += utils/maxwell_tex.cpp
cpp_files += utils/hopper_tex.cpp
cpp_files += utils/mdiag_xml.cpp
cpp_files += utils/mdiagsurf.cpp
cpp_files += utils/cpumodel.cpp
cpp_files += utils/perf_mon.cpp
cpp_files += utils/pm_mux_mgr.cpp
cpp_files += utils/pm_rpt.cpp
cpp_files += utils/profile.cpp
cpp_files += utils/randstrm.cpp
cpp_files += utils/raster_types.cpp
cpp_files += utils/StopWatch.cpp
cpp_files += utils/tex.cpp
cpp_files += utils/tracefil.cpp
cpp_files += utils/lwgpu_classes.cpp
cpp_files += utils/utils.cpp
cpp_files += utils/xml_node.cpp
cpp_files += utils/xml_wrapper.cpp
cpp_files += utils/sharedsurfacecontroller.cpp
cpp_files += utils/mmuutils.cpp
cpp_files += utils/mmufaultbuffers.cpp
cpp_files += utils/vaspaceutils.cpp
cpp_files += simctx.cpp
cpp_files += tests/rtapi/rtapitest.cpp

ifneq "$(INCLUDE_MDIAGUTL)" "true"
    cpp_files += utl/utlstub.cpp
endif

ifeq "$(base_os)" "linux"
    ifeq "$(BUILD_ARCH)" "amd64"
        include_dirs   += tests/rtapi/elfloader
        defines        += -DELF_LOADER
        cpp_files      += tests/rtapi/elfloader/elfloader.cpp
    endif
endif

ifneq "$(filter 7.% 9.%,$(toolchain_rev))" ""
    $(o_dir)/util-compiler-switch$(obj_suffix): CFLAGS += -Wno-frame-address
endif

.DEFAULT_GOAL := build
SKIP_REGHAL_SRC = true
DRIVERS_DIR ?= ../../../drivers

LWCFG_ROOT      = $(DRIVERS_DIR)/common/build
LWCFG_OUTPUTDIR = $(gen_cpp_dir)
include $(LWCFG_ROOT)/lwconfig.lwmk
include_dirs += $(LWCFG_OUTPUTDIR)

GEN_LIB_DIR = $(MODS_OUTPUT_DIR)/$(rel_o_dir)
include ../gpu/reghal/makesrc.inc

# Generic rules for a library build
include $(MODS_DIR_UNIX)/makelib.inc
include $(MODS_DIR_UNIX)/makerule.inc

# Include dependency information.
-include $(depend_files)
