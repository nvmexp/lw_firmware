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

cpp_files                           += gpu/tests/gputest.cpp
cpp_files                           += gpu/tests/gputestc.cpp
cpp_files                           += gpu/tests/jsgputst.cpp
cpp_files$(ONLY_DGPU)               += gpu/tests/gfe_pkey.cpp

# maybe these should be moved to gpu directory instead of core?
cpp_files                           += core/tests/goldtest.cpp
cpp_files                           += core/utility/golden.cpp # Majorly relies on stubs
cpp_files                           += core/utility/golddb.cpp # Ok on its own, but not useful
cpp_files                           += core/utility/goldsurf.cpp

ifeq "$(BUILD_OS)" "win32"
    cpp_files                       += win32/OverlayWindow.cpp
    cpp_files                       += win32/win32disp_common.cpp
    cpp_files                       += win32/win32lwdisp_imp.cpp
    cpp_files                       += win32/win32lwapi_utils.cpp
    cpp_files                       += win32/win32disp_x86.cpp
endif #(BUILD_OS)

cpp_files                           += gpu/channel.cpp
cpp_files                           += gpu/ch_fermi.cpp
cpp_files                           += gpu/ch_kepler.cpp
cpp_files                           += gpu/ch_volta.cpp
cpp_files                           += gpu/ch_ampere.cpp
cpp_files                           += gpu/ch_hopper.cpp
cpp_files                           += gpu/ch_blackwell.cpp
cpp_files                           += gpu/ch_amodel.cpp
ifeq "$(BUILD_OS)" "cheetah"
    cpp_files                       += gpu/ch_android.cpp
    cpp_files                       += gpu/ch_tegra_thi.cpp
    cpp_files                       += gpu/tegralwrm.cpp
endif
cpp_files                           += gpu/dispchan.cpp
cpp_files                           += gpu/dispchan0300.cpp
cpp_files                           += gpu/chanwrap.cpp

cpp_files                           += gpu/gpu.cpp
cpp_files                           += gpu/gpudev.cpp
cpp_files                           += gpu/gpumgr.cpp
cpp_files                           += gpu/gpusbdev.cpp
cpp_files                           += gpu/boarddef.cpp
cpp_files                           += gpu/lwrm.cpp
cpp_files                           += gpu/ism/gpuism.cpp
cpp_files                           += gpu/ism/gmxxxism.cpp
cpp_files                           += gpu/ism/gm2xxism.cpp
cpp_files                           += gpu/ism/lwswitchism.cpp
cpp_files$(ONLY_IGPU)               += gpu/ism/tegraism.cpp
cpp_files$(ONLY_IGPU)               += gpu/ism/t194ism.cpp
cpp_files$(ONLY_IGPU)               += gpu/ism/t234ism.cpp
cpp_files$(ONLY_PASCAL)             += gpu/ism/gpxxxism.cpp
cpp_files$(ONLY_VOLTA)              += gpu/ism/gvxxxism.cpp
cpp_files$(ONLY_TURING)             += gpu/ism/tuxxxism.cpp
cpp_files$(ONLY_AMPERE)             += gpu/ism/gaxxxism.cpp
cpp_files$(ONLY_AMPERE)             += gpu/ism/ga10xism.cpp
cpp_files$(ONLY_HOPPER)             += gpu/ism/ghxxxism.cpp
cpp_files$(ONLY_ADA)                += gpu/ism/ad10xism.cpp
cpp_files$(ONLY_BLACKWELL)          += gpu/ism/gbxxxism.cpp
ifeq "$(BUILD_OS)" "cheetah"
    DISPLAY_LWRMAPI_H = $(gen_cpp_dir)/dispLwRmApi.h
    gen_h_files += $(DISPLAY_LWRMAPI_H)
    LWRMAPI_H = $(SDK_DIR)/lwpu/inc/lwRmApi.h

    $(DISPLAY_LWRMAPI_H):$(LWRMAPI_H)
	sed -e 's/LwRm\([^(]\+\)(\([^)]*\));/DispLwRm\1(\2);/' \
	$< > $@

    $(o_dir)/lwrm$(obj_suffix): $(gen_cpp_dir)/dispLwRmApi.h
endif

cpp_files$(ONLY_HOPPER)             += gpu/floorsweep/hopperfs.cpp
cpp_files$(ONLY_AMPERE)             += gpu/floorsweep/amperefs.cpp
cpp_files$(ONLY_ADA)                += gpu/floorsweep/ad10xfs.cpp
cpp_files$(ONLY_T234)               += gpu/floorsweep/ga10bfs.cpp
cpp_files$(ONLY_AMPERE)             += gpu/floorsweep/ga10xfs.cpp
cpp_files$(ONLY_TURING)             += gpu/floorsweep/tu10xfs.cpp
cpp_files$(ONLY_VOLTA)              += gpu/floorsweep/gv10xfs.cpp
cpp_files$(ONLY_T194)               += gpu/floorsweep/gv11bfs.cpp
cpp_files                           += gpu/floorsweep/gp10xfs.cpp
cpp_files$(ONLY_PASCAL)             += gpu/floorsweep/gp100fs.cpp
cpp_files$(ONLY_PASCAL)             += gpu/floorsweep/gp104fs.cpp
cpp_files$(ONLY_PASCAL)             += gpu/floorsweep/gp107fs.cpp
cpp_files                           += gpu/floorsweep/gm20xfs.cpp
cpp_files                           += gpu/floorsweep/gm10xfs.cpp
cpp_files                           += gpu/floorsweep/fermifs.cpp
cpp_files                           += gpu/floorsweep/nullfs.cpp
cpp_files                           += gpu/floorsweep/floorsweepimpl.cpp
ifeq "$(INCLUDE_FSLIB)" "true"
    cpp_files                       += gpu/floorsweep/fslib_interface.cpp
endif

cpp_files                           += gpu/pcie/pcieimpl.cpp
cpp_files                           += gpu/pcie/simpcie.cpp
cpp_files$(ONLY_IGPU)               += gpu/pcie/socpcie.cpp
cpp_files                           += gpu/pcie/fermipcie.cpp
cpp_files                           += gpu/pcie/keplerpcie.cpp
cpp_files                           += gpu/pcie/maxwellpcie.cpp
cpp_files                           += gpu/pcie/pascalpcie.cpp
cpp_files                           += gpu/pcie/voltapcie.cpp
cpp_files$(ONLY_TURING)             += gpu/pcie/turingpcie.cpp
cpp_files$(ONLY_AMPERE)             += gpu/pcie/amperepcie.cpp
cpp_files$(ONLY_AMPERE)             += gpu/pcie/ga10xpcie.cpp
cpp_files$(ONLY_HOPPER)             += gpu/pcie/hopperpcie.cpp
cpp_files$(ONLY_T256)               += gpu/pcie/ad10bpcie.cpp
cpp_files                           += gpu/pcie/pcicfg.cpp
cpp_files                           += gpu/pcie/pcicfggpu.cpp
cpp_files                           += gpu/pcie/turingpcicfg.cpp

cpp_files                           += gpu/i2c/i2cimpl.cpp
cpp_files                           += gpu/i2c/gpui2c.cpp

cpp_files                           += gpu/gpio/gpioimpl.cpp
cpp_files                           += gpu/gpio/gpugpio.cpp
cpp_files                           += gpu/c2c/hopperc2c.cpp

ifeq "$(INCLUDE_XUSB)" "true"
    cpp_files                       += gpu/usb/hostctrl/xusbhostctrlimpl.cpp
    cpp_files                       += gpu/usb/hostctrl/turingxusb.cpp
    cpp_files                       += gpu/usb/portpolicyctrl/portpolicyctrlimpl.cpp
    cpp_files                       += gpu/usb/portpolicyctrl/turingppc.cpp
    cpp_files                       += gpu/usb/portpolicyctrl/voltappc.cpp
    cpp_files                       += gpu/usb/usbdatatransfer.cpp
    cpp_files                       += gpu/usb/functionaltestboard/uartftb.cpp
    cpp_files                       += gpu/usb/functionaltestboard/vdmftb.cpp
endif

cpp_files                           += gpu/interface/vbios_preos_pbi.cpp

cpp_files                           += gpu/gcx/gcximpl.cpp

cpp_files                           += gpu/js_disp.cpp

cpp_files                           += gpu/js_gpu.cpp
cpp_files                           += gpu/js_gpusb.cpp
cpp_files                           += gpu/js_gpudv.cpp
cpp_files                           += gpu/js_gpumg.cpp
cpp_files                           += gpu/js_fb.cpp
cpp_files                           += gpu/js_ecov.cpp

cpp_files$(ONLY_IGPU)               += gpu/dummygpu.cpp
ifeq "$(LWCFG_GLOBAL_GPU_IMPL_G000)" "1"
    cpp_files                       += gpu/amodgpu.cpp
endif
cpp_files$(ONLY_MAXWELL)            += gpu/gm10xgpu.cpp
cpp_files$(ONLY_MAXWELL)            += gpu/gm107gpu.cpp
cpp_files$(ONLY_MAXWELL)            += gpu/gm108gpu.cpp
cpp_files$(ONLY_MAXWELL)            += gpu/gm20xgpu.cpp
cpp_files$(ONLY_MAXWELL)            += gpu/gm200gpu.cpp
cpp_files$(ONLY_MAXWELL)            += gpu/gm204gpu.cpp
cpp_files$(ONLY_MAXWELL)            += gpu/gm206gpu.cpp
cpp_files$(ONLY_T194)               += gpu/nogpu.cpp
cpp_files$(ONLY_PASCAL)             += gpu/gp10xgpu.cpp
cpp_files$(ONLY_PASCAL)             += gpu/gp100gpu.cpp
cpp_files$(ONLY_PASCAL)             += gpu/gp102gpu.cpp
cpp_files$(ONLY_PASCAL)             += gpu/gp104gpu.cpp
cpp_files$(ONLY_PASCAL)             += gpu/gp106gpu.cpp
cpp_files$(ONLY_PASCAL)             += gpu/gp107gpu.cpp
cpp_files$(ONLY_PASCAL)             += gpu/gp108gpu.cpp
cpp_files                           += gpu/voltagpu.cpp
cpp_files$(ONLY_VOLTA)              += gpu/gv100gpu.cpp
cpp_files$(ONLY_T194)               += gpu/gv11bgpu.cpp
cpp_files$(ONLY_TURING)             += gpu/turinggpu.cpp
cpp_files$(ONLY_TURING)             += gpu/tu10xgpu.cpp
cpp_files$(ONLY_TURING)             += gpu/tu100gpu.cpp
cpp_files$(ONLY_TURING)             += gpu/tu102gpu.cpp
cpp_files$(ONLY_TURING)             += gpu/tu104gpu.cpp
cpp_files$(ONLY_TURING)             += gpu/tu106gpu.cpp
cpp_files$(ONLY_TURING)             += gpu/tu11xgpu.cpp
cpp_files$(ONLY_TU116)              += gpu/tu116gpu.cpp
cpp_files$(ONLY_TU117)              += gpu/tu117gpu.cpp
cpp_files$(ONLY_AMPERE)             += gpu/amperegpu.cpp
cpp_files$(ONLY_AMPERE)             += gpu/ga10xgpu.cpp
cpp_files$(ONLY_GA100)              += gpu/ga100gpu.cpp
cpp_files$(ONLY_GA102)              += gpu/ga102gpu.cpp
cpp_files$(ONLY_GA103)              += gpu/ga103gpu.cpp
cpp_files$(ONLY_GA104)              += gpu/ga104gpu.cpp
cpp_files$(ONLY_GA106)              += gpu/ga106gpu.cpp
cpp_files$(ONLY_GA107)              += gpu/ga107gpu.cpp
cpp_files$(ONLY_T234)               += gpu/ga10bgpu.cpp
cpp_files$(ONLY_T234)               += gpu/ga10fgpu.cpp
cpp_files$(ONLY_T234)               += gpu/ga11bgpu.cpp
cpp_files$(ONLY_ADA)                += gpu/adagpu.cpp
cpp_files$(ONLY_ADA)                += gpu/ad102gpu.cpp
cpp_files$(ONLY_T256)               += gpu/ad10bgpu.cpp
cpp_files$(ONLY_HOPPER)             += gpu/hoppergpu.cpp
cpp_files$(ONLY_GH100)              += gpu/gh100gpu.cpp
cpp_files$(ONLY_GH202)              += gpu/gh202gpu.cpp
cpp_files$(ONLY_BLACKWELL)          += gpu/blackwellgpu.cpp
cpp_files$(ONLY_GB100)              += gpu/gb100gpu.cpp

cpp_files                           += gpu/display/displaycleanup.cpp
cpp_files$(ONLY_PC)                 += gpu/display/js_cctx.cpp
cpp_files                           += gpu/display/display.cpp
ifeq "$(include_azalia_support)" "true"
    cpp_files                       += gpu/display/gpuaudio.cpp
endif
cpp_files                           += gpu/display/dispfact.cpp
cpp_files                           += gpu/display/dpmgr.cpp
cpp_files                           += gpu/display/dputil.cpp
cpp_files                           += gpu/display/disp_visualcomponent.cpp
cpp_files                           += gpu/display/dispmgr.cpp
cpp_files                           += gpu/display/js_dctx.cpp
cpp_files                           += gpu/display/rastersettings.cpp
cpp_files$                          += gpu/display/modeset_utils.cpp
cpp_files                           += gpu/display/evo_cctx.cpp
cpp_files                           += gpu/display/evo_chns.cpp
cpp_files                           += gpu/display/evo_disp.cpp
cpp_files                           += gpu/display/lwdisplay/iot_config_parser.cpp
cpp_files                           += gpu/display/lwdisplay/iot_superswitch.cpp
cpp_files                           += gpu/display/lwdisplay/js_lwcctx.cpp
cpp_files                           += gpu/display/lwdisplay/js_lwdisp.cpp
cpp_files                           += gpu/display/lwdisplay/lwdisp_cctx.cpp
cpp_files                           += gpu/display/lwdisplay/lw_disp.cpp
cpp_files                           += gpu/display/lwdisplay/lw_disp_c3.cpp
cpp_files                           += gpu/display/lwdisplay/lw_disp_c5.cpp
cpp_files                           += gpu/display/lwdisplay/lw_disp_c6.cpp
cpp_files                           += gpu/display/lwdisplay/lw_disp_c7.cpp
cpp_files                           += gpu/display/lwdisplay/lw_disp_c8.cpp
cpp_files                           += gpu/display/lwdisplay/lw_disp_crc_c3.cpp
cpp_files                           += gpu/display/lwdisplay/lw_disp_crc_handler_c3.cpp
cpp_files                           += gpu/display/lwdisplay/lw_disp_crc_c6.cpp
cpp_files                           += gpu/display/lwdisplay/lw_disp_imp.cpp
cpp_files                           += gpu/display/lwdisplay/lw_disp_lut_generator.cpp
cpp_files                           += gpu/display/lwdisplay/lw_disp_regs_c5.cpp
cpp_files                           += gpu/display/lwdisplay/lw_disp_udta.cpp
cpp_files                           += gpu/display/lwdisplay/lw_disp_udta_c3.cpp
cpp_files                           += gpu/display/lwdisplay/lw_disp_utils.cpp
cpp_files                           += gpu/display/lwdisplay/lwdisp_fmt_coeff.cpp
cpp_files                           += gpu/display/lwdisplay/lwdisp_stgs.cpp
cpp_files                           += gpu/display/lwdisplay/lwdisp_panelmgr.cpp
cpp_files                           += gpu/display/lwdisplay/lw_disp_dp_panel_crc.cpp
cpp_files                           += gpu/display/lwdisplay/udta2lwdlib_if.cpp
cpp_files                           += gpu/display/dpc/dpc_file_parser.cpp
cpp_files                           += gpu/display/dpc/dpc_configurator.cpp
cpp_files                           += gpu/display/dpc/dpc_config_gen.cpp

cpp_files                           += gpu/display/evo_dp.cpp
cpp_files$(ONLY_DGPU)               += gpu/display/evo_g82.cpp
cpp_files$(ONLY_DGPU)               += gpu/display/evogf119.cpp
cpp_files$(ONLY_DGPU)               += gpu/display/evogk104.cpp
cpp_files$(ONLY_DGPU)               += gpu/display/evogk110.cpp
cpp_files$(ONLY_DGPU)               += gpu/display/evogm107.cpp
cpp_files$(ONLY_DGPU)               += gpu/display/evogm200.cpp
cpp_files$(ONLY_DGPU)               += gpu/display/evogp107.cpp
cpp_files$(ONLY_DGPU)               += gpu/display/evogp100.cpp
cpp_files                           += gpu/display/evo_stgs.cpp
cpp_files                           += gpu/display/imp_utils.cpp

cpp_files                           += gpu/display/modsedid.cpp
cpp_files                           += gpu/display/nul_disp.cpp
cpp_files                           += gpu/display/dpusub.cpp
cpp_files                           += gpu/display/crc_checker_dev.cpp
cpp_files                           += gpu/display/tcon_device.cpp
cpp_files                           += gpu/display/statecache_C3.cpp
ifeq "$(BUILD_OS)" "cheetah"
    cpp_files                       += gpu/display/e1850_android.cpp
else
    cpp_files                       += gpu/display/e1850_rm.cpp
endif

ifeq "$(BUILD_OS)" "cheetah"
    cpp_files                       += gpu/display/tegra_disp_lwdc.cpp
    cpp_files                       += gpu/display/tegra_disp_lwdc_t210.cpp
    cpp_files                       += gpu/display/tegra_disp_lwdc_t214.cpp
    cpp_files                       += gpu/display/tegra_disp_lwdc_t186.cpp
    cpp_files                       += gpu/display/tegra_disp_lwdc_t194.cpp
    cpp_files                       += gpu/display/tegralwdc/liblwdcif.cpp
    cpp_files                       += gpu/display/tegralwdc/tegra_lwdc_imp.cpp
    cpp_files                       += gpu/display/tegralwdc/tegra_lwdc_imp_t186.cpp
    cpp_files                       += gpu/display/tegralwdc/tegra_lwdc_dispstate.cpp
    cpp_files                       += gpu/display/tegralwdc/tegra_lwdc_utils.cpp
    cpp_files                       += gpu/utility/tegragsyncdev.cpp
endif

cpp_files$(ONLY_IGPU)               += gpu/display/tegra_disp.cpp


cpp_files                           += gpu/tempctrl/tempctrl.cpp
cpp_files                           += gpu/tempctrl/hwaccess/ipmitempctrl.cpp
ifneq "$(BUILD_OS)" "linuxmfg"
    cpp_files                       += gpu/tempctrl/hwaccess/ipmitempctrl_stub.cpp
endif

cpp_files                           += gpu/framebuf/framebuf.cpp
cpp_files                           += gpu/framebuf/gpufb.cpp
cpp_files                           += gpu/framebuf/amapv1fb.cpp
cpp_files                           += gpu/framebuf/amapv2fb.cpp
cpp_files                           += gpu/framebuf/nofb.cpp
cpp_files                           += gpu/framebuf/vmfb.cpp
cpp_files$(ONLY_IGPU)               += gpu/framebuf/tegrafb.cpp
cpp_files                           += gpu/framebuf/gf100fb.cpp
cpp_files$(ONLY_DGPU)               += gpu/framebuf/gf114fb.cpp
cpp_files$(ONLY_DGPU)               += gpu/framebuf/amodfb.cpp
cpp_files                           += gpu/framebuf/gf100adr.cpp
cpp_files                           += gpu/framebuf/gk107fb.cpp
cpp_files                           += gpu/framebuf/gklit1fb.cpp
cpp_files                           += gpu/framebuf/gk110fb.cpp
cpp_files                           += gpu/framebuf/gk208fb.cpp
cpp_files                           += gpu/framebuf/gm10xfb.cpp
cpp_files                           += gpu/framebuf/gm20xfb.cpp
cpp_files                           += gpu/framebuf/gmlitxadr.cpp
cpp_files                           += gpu/framebuf/gmlit1adr.cpp
cpp_files                           += gpu/framebuf/gmlit2adr.cpp
cpp_files                           += gpu/framebuf/gk107adr.cpp
cpp_files                           += gpu/framebuf/gklit1adr.cpp
cpp_files                           += gpu/framebuf/gklit2adr.cpp
cpp_files                           += gpu/framebuf/gk208adr.cpp

cpp_files                           += gpu/fuse/fusefactory.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/gpufuse.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/fusesrc.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/js_fuse.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/gm10x_fuse.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/gm20x_fuse.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/gp10x_fuse.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/gv100_fuse.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/newfuse.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/fusetypes.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/fuseutils.cpp
cpp_files$(ONLY_ADA)                += gpu/fuse/hwaccess/ad10xfuseaccessor.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/hwaccess/amperefuseaccessor.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/hwaccess/ga10xfuseaccessor.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/hwaccess/gpufuseaccessor.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/hwaccess/hopperfuseaccessor.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/hwaccess/fpfaccessor.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/hwaccess/riraccessor.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/hwaccess/dummyfuseaccessor.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/encoding/splitmacroencoder.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/encoding/columnfuseencoder.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/encoding/ga10xffencoder.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/encoding/hopperffencoder.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/encoding/maxwellffencoder.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/encoding/iffencoder.cpp
cpp_files$(ONLY_ADA)                += gpu/fuse/encoding/ad10xiffencoder.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/encoding/hopperiffencoder.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/encoding/rirencoder.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/skuhandling/basicskuhandler.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/downbin/fselement.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/downbin/fsgroup.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/downbin/fsset.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/downbin/fsmask.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/downbin/downbinimpl.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/downbin/ga10xdownbinimpl.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/downbin/hopperdownbinimpl.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/downbin/downbinfs.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/downbin/downbinelementpickers.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/downbin/downbingrouppickers.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/downbin/downbinrulesimpl.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/iffrecord.cpp
cpp_files$(ONLY_DGPU)               += gpu/fuse/hopperiffrecord.cpp

cpp_files                           += gpu/fuse/gf100ecid.cpp

cpp_files                           += gpu/perfmon/gpupm.cpp
cpp_files$(ONLY_MAXWELL)            += gpu/perfmon/gm10xpm.cpp
cpp_files$(ONLY_PASCAL)             += gpu/perfmon/gp10xpm.cpp
cpp_files$(ONLY_VOLTA)              += gpu/perfmon/gv10xpm.cpp
cpp_files$(ONLY_TURING)             += gpu/perfmon/tu10xpm.cpp

cpp_files                           += gpu/repair/hbm/gpu_interface/ampere_hbm_interface.cpp
cpp_files                           += gpu/repair/hbm/gpu_interface/gpu_hbm_interface.cpp
cpp_files                           += gpu/repair/hbm/gpu_interface/volta_hbm_interface.cpp
cpp_files                           += gpu/repair/hbm/hbm_interface/hbm_interface.cpp
cpp_files                           += gpu/repair/hbm/hbm_interface/hbm2_interface.cpp
cpp_files                           += gpu/repair/hbm/hbm_interface/vendor/samsung/samsung_hbm2_bdie_interface.cpp
cpp_files                           += gpu/repair/hbm/hbm_interface/vendor/samsung/samsung_hbm2_xdie_interface.cpp
cpp_files                           += gpu/repair/hbm/hbm_interface/vendor/samsung/samsung_hbm2_xdie_ga100_interface.cpp
cpp_files                           += gpu/repair/hbm/hbm_interface/vendor/samsung/samsung_hbm2_xdie_gv100_interface.cpp
cpp_files                           += gpu/repair/hbm/hbm_interface/vendor/samsung/samsung_hbm2e_ga100_interface.cpp
cpp_files                           += gpu/repair/hbm/hbm_interface/vendor/samsung/samsung_hbm2e_interface.cpp
cpp_files                           += gpu/repair/hbm/hbm_interface/vendor/skhynix/skhynix_hbm_interface.cpp
cpp_files                           += gpu/repair/hbm/hbm_interface/vendor/skhynix/skhynix_hbm2e_interface.cpp
cpp_files                           += gpu/repair/hbm/hbm_interface/vendor/micron/micron_hbm2e_interface.cpp
cpp_files                           += gpu/repair/hbm/hbm_wir.cpp
cpp_files                           += gpu/repair/gddr/gddr_interface/gddr_interface.cpp
cpp_files                           += gpu/repair/gddr/gddr_interface/vendor/samsung/samsung_gddr6_interface.cpp
cpp_files                           += gpu/repair/gddr/gddr_interface/vendor/samsung/samsung_gddr6_ga10x_interface.cpp
cpp_files                           += gpu/repair/mem_repair_config.cpp
cpp_files                           += gpu/repair/repair_module.cpp
cpp_files                           += gpu/repair/repair_module_gddr.cpp
cpp_files                           += gpu/repair/repair_module_hbm.cpp
cpp_files                           += gpu/repair/repair_module_mem.cpp
cpp_files                           += gpu/repair/repair_module_tpc.cpp
cpp_files                           += gpu/repair/repair_module_commands.cpp
cpp_files                           += gpu/repair/repair_module_js_wrapper.cpp
cpp_files                           += gpu/repair/repair_util.cpp
cpp_files                           += gpu/repair/row_remapper/row_remapper.cpp
cpp_files                           += gpu/repair/tpc_repair/tpc_repair.cpp

ifeq "$(INCLUDE_RMTEST)" "true"
cpp_files                           += gpu/tests/rm/utility/xmlnode.cpp
cpp_files                           += gpu/tests/rm/utility/xmlwrapper.cpp
cpp_files                           += gpu/tests/rm/utility/crccomparison.cpp
cpp_files                           += gpu/tests/rm/utility/dtiutils.cpp
cpp_files                           += gpu/tests/rm/utility/hdcputils.cpp
endif

ifeq "$(INCLUDE_UNITTEST)" "true"
    cpp_files                       += gpu/unittests/unit_framebuf.cpp
    cpp_files                       += gpu/unittests/unit_surffill.cpp
    internal_release_files$(ONLY_PC)+= gpu/js/unit_downbin.js
    internal_release_files$(ONLY_PC)+= gpu/js/unit_downbin2.js
    internal_release_files$(ONLY_PC)+= gpu/js/unit_fslib.js
endif

cpp_files                           += gpu/perf/avfssub.cpp
cpp_files                           += gpu/perf/clockthr.cpp
cpp_files                           += gpu/perf/gf100thr.cpp
cpp_files                           += gpu/perf/gm10xpmu.cpp
cpp_files                           += gpu/perf/gm20xthr.cpp
cpp_files                           += gpu/perf/gp100thr.cpp
cpp_files                           += gpu/perf/js_avfs.cpp
cpp_files                           += gpu/perf/js_perf.cpp
cpp_files                           += gpu/perf/js_pmu.cpp
cpp_files                           += gpu/perf/js_power.cpp
cpp_files                           += gpu/perf/js_therm.cpp
cpp_files                           += gpu/perf/js_volt.cpp
ifeq "$(include_smb_support)" "true"
    cpp_files                       += gpu/perf/js_zpi.cpp
endif
cpp_files                           += gpu/perf/perfprofiler.cpp
cpp_files                           += gpu/perf/perfsub.cpp
cpp_files                           += gpu/perf/perfsub_20.cpp
cpp_files                           += gpu/perf/perfsub_30.cpp
cpp_files                           += gpu/perf/perfsub_40.cpp
ifeq "$(BUILD_OS)" "cheetah"
    cpp_files                       += gpu/perf/perfsub_android.cpp
endif
cpp_files$(ONLY_DGPU)               += gpu/perf/perfsub_socclientrm.cpp
cpp_files                           += gpu/perf/perfutil.cpp
cpp_files                           += gpu/perf/pmusub.cpp
cpp_files                           += gpu/perf/powermgmtutil.cpp
cpp_files                           += gpu/perf/pwrsub.cpp
cpp_files                           += gpu/perf/pwrwait.cpp
cpp_files                           += gpu/perf/thermsub.cpp
ifneq "$(BUILD_OS)" "linuxmfg"
    cpp_files                       += gpu/perf/thrmipmistub.cpp
endif
cpp_files                           += gpu/perf/voltshmo.cpp
cpp_files                           += gpu/perf/voltsub.cpp
cpp_files                           += gpu/perf/voltageregulator.cpp
cpp_files                           += gpu/utility/atomfsm.cpp
cpp_files                           += gpu/utility/atomwrap.cpp
cpp_files                           += gpu/utility/blacklist.cpp
cpp_files                           += gpu/utility/blocklin.cpp
cpp_files                           += gpu/utility/chansub.cpp
cpp_files                           += gpu/utility/chanwmgr.cpp
cpp_files                           += gpu/utility/commonjtmgr.cpp
cpp_files                           += gpu/utility/dispatcher.cpp
cpp_files                           += gpu/utility/ecccount.cpp
cpp_files                           += gpu/utility/eclwtil.cpp
cpp_files                           += gpu/utility/edccount.cpp
cpp_files                           += gpu/utility/fakeproc.cpp
cpp_files                           += gpu/utility/falconimpl.cpp
cpp_files                           += gpu/utility/falconucode.cpp
cpp_files                           += gpu/utility/gm20xfalcon.cpp
cpp_files                           += gpu/utility/ga10xfalcon.cpp
cpp_files                           += gpu/utility/rngpickr.cpp
cpp_files                           += gpu/utility/nfysema.cpp
cpp_files                           += gpu/utility/gpuerrcount.cpp
cpp_files                           += gpu/utility/gpugsurf.cpp
cpp_files                           += gpu/utility/gpuwm.cpp
cpp_files                           += gpu/utility/gpuutils.cpp
cpp_files                           += gpu/utility/gralloc.cpp
cpp_files                           += gpu/utility/grcaps.cpp
cpp_files                           += gpu/utility/gsyncdisp.cpp
cpp_files                           += gpu/utility/gsyncdev.cpp
cpp_files                           += gpu/utility/gsynccrc.cpp
cpp_files                           += gpu/utility/hwmon.cpp
cpp_files                           += gpu/utility/js_const.cpp
cpp_files                           += gpu/utility/js_testdevice.cpp
cpp_files                           += gpu/utility/mapfb.cpp
cpp_files                           += gpu/utility/dmawrap.cpp
cpp_files                           += gpu/utility/mmeigen.cpp
cpp_files                           += gpu/utility/mmesim.cpp
cpp_files                           += gpu/utility/modscnsl.cpp
cpp_files                           += gpu/utility/notifier.cpp
cpp_files                           += gpu/utility/onedgoldensurfaces.cpp
cpp_files                           += gpu/utility/pmgrmutex.cpp
cpp_files                           += gpu/utility/pwmutil.cpp
cpp_files                           += gpu/utility/pcie/pexdev.cpp
cpp_files                           += gpu/utility/sec2rtossub.cpp
cpp_files                           += gpu/utility/rchelper.cpp
cpp_files                           += gpu/utility/gpurectfill.cpp

ifneq (,$(filter $(BUILD_OS), linux linuxmfg sim))
    cpp_files$(ONLY_DGPU)           += gpu/utility/aes_gcm/aes.cpp
    cpp_files$(ONLY_DGPU)           += gpu/utility/aes_gcm/gcm.cpp
endif

cpp_files$(ONLY_DGPU)               += gpu/tests/rm/utility/changrp.cpp

ifeq "$(INCLUDE_RMTEST)" "true"
    cpp_files                       += gpu/utility/rmclkutil.cpp
endif
cpp_files                           += gpu/utility/rmctrlutils.cpp
cpp_files                           += gpu/utility/rppgimpl.cpp
cpp_files                           += gpu/utility/tpcpgimpl.cpp
cpp_files                           += gpu/utility/lpwrctrlimpl.cpp
cpp_files                           += gpu/utility/runlist.cpp
cpp_files                           += gpu/utility/runlwrap.cpp
cpp_files                           += gpu/utility/semawrap.cpp
cpp_files                           += gpu/utility/subdevfb.cpp
cpp_files                           += gpu/utility/subdevgr.cpp
cpp_files                           += gpu/utility/falconecc.cpp
cpp_files                           += gpu/utility/hshubecc.cpp
cpp_files                           += gpu/utility/pcibusecc.cpp
cpp_files                           += gpu/utility/surf2d.cpp
cpp_files                           += gpu/utility/surffill.cpp
cpp_files                           += gpu/utility/surffmt.cpp
cpp_files                           += gpu/utility/surfrdwr.cpp
cpp_files                           += gpu/utility/surfcomp.cpp
cpp_files                           += gpu/utility/l2prefetch.cpp
cpp_files                           += gpu/utility/js_surfcopy.cpp
ifeq "$(BUILD_OS)" "cheetah"
    cpp_files                       += gpu/utility/syncpoint_android.cpp
else
    cpp_files                       += gpu/utility/syncpoint.cpp
endif
cpp_files                           += gpu/utility/tsg.cpp
cpp_files                           += gpu/utility/subcontext.cpp
cpp_files                           += gpu/utility/userdalloc.cpp
cpp_files                           += gpu/utility/smlwtil.cpp
cpp_files                           += gpu/utility/tpceccerror.cpp

ifeq "$(include_smb_support)" "true"
    cpp_files                       += gpu/utility/smbec.cpp
    cpp_files                       += gpu/perf/zpi.cpp
endif

cpp_files                           += gpu/utility/vgpu.cpp
ifeq "$(BUILD_OS)" "linuxmfg"
    cpp_files                       += linux/thrmipmi.cpp
    cpp_files                       += linux/ipmitempctrl_impl.cpp
    cpp_files                       += linux/genericipmi.cpp
else
    cpp_files                       += device/utility/genericipmistub.cpp
endif
cpp_files                           += device/utility/js_ipmi.cpp

cpp_files                           += gpu/utility/bglogger/bglogger.cpp
cpp_files                           += gpu/utility/bglogger/bgmonitor.cpp
cpp_files                           += gpu/utility/bglogger/cpuusagemonitor.cpp
cpp_files                           += gpu/utility/bglogger/pergpumonitor.cpp
cpp_files                           += gpu/utility/bglogger/perlwswitchmonitor.cpp
cpp_files                           += gpu/utility/bglogger/corevoltagemonitor.cpp
cpp_files                           += gpu/utility/bglogger/lwrrentmonitor.cpp
cpp_files                           += gpu/utility/bglogger/exttempmonitor.cpp
cpp_files                           += gpu/utility/bglogger/tempctrlmonitor.cpp
cpp_files                           += gpu/utility/bglogger/fanmonitor.cpp
cpp_files                           += gpu/utility/bglogger/gpuclocksmonitor.cpp
cpp_files                           += gpu/utility/bglogger/gpuusagemonitor.cpp
cpp_files                           += gpu/utility/bglogger/voltageregulatormonitor.cpp
cpp_files                           += gpu/utility/bglogger/memtempmonitor.cpp
cpp_files                           += gpu/utility/bglogger/dramtempmonitor.cpp
cpp_files                           += gpu/utility/bglogger/gputempmonitor.cpp
cpp_files                           += gpu/utility/bglogger/inttempmonitor.cpp
cpp_files                           += gpu/utility/bglogger/bjttempmonitor.cpp
cpp_files                           += gpu/utility/bglogger/ipmitempmonitor.cpp
cpp_files                           += gpu/utility/bglogger/lwswitchtempmonitor.cpp
cpp_files                           += gpu/utility/bglogger/lwswitchvoltmonitor.cpp
cpp_files                           += gpu/utility/bglogger/pciespeedmonitor.cpp
cpp_files                           += gpu/utility/bglogger/powermonitor.cpp
cpp_files                           += gpu/utility/bglogger/smbustempmonitor.cpp
cpp_files                           += gpu/utility/bglogger/soctempmonitor.cpp
cpp_files                           += gpu/utility/bglogger/testnummonitor.cpp
cpp_files                           += gpu/utility/bglogger/grstatusmonitor.cpp
cpp_files                           += gpu/utility/bglogger/testfixturetemperaturemonitor.cpp
cpp_files                           += gpu/utility/bglogger/testfixturefanmonitor.cpp
cpp_files                           += gpu/utility/bglogger/speedomonitor.cpp
cpp_files                           += gpu/utility/bglogger/cpmmonitor.cpp
cpp_files                           += gpu/utility/bglogger/volterramonitor.cpp
cpp_files                           += gpu/utility/bglogger/thermalslowdownmonitor.cpp
cpp_files                           += gpu/utility/bglogger/gpclimitsmonitor.cpp
cpp_files                           += gpu/utility/bglogger/edppmonitor.cpp
cpp_files                           += gpu/utility/bglogger/ipmilwstommonitor.cpp
cpp_files                           += gpu/utility/bglogger/fancoolerstatusmonitor.cpp
cpp_files                           += gpu/utility/bglogger/fanpolicystatusmonitor.cpp
cpp_files                           += gpu/utility/bglogger/fanarbiterstatusmonitor.cpp
cpp_files                           += gpu/utility/bglogger/cwclimitsmonitor.cpp
cpp_files                           += gpu/utility/bglogger/voltsensedmonitor.cpp
cpp_files                           += gpu/utility/bglogger/bgramassistengaged.cpp
cpp_files                           += gpu/utility/bglogger/powerpolicieslogger.cpp
cpp_files                           += gpu/utility/bglogger/gpiomonitor.cpp
cpp_files                           += gpu/utility/speedo.cpp
cpp_files                           += gpu/utility/vbios.cpp
cpp_files                           += gpu/utility/ecov_verifier.cpp
cpp_files                           += gpu/utility/event_cover.cpp
cpp_files                           += gpu/utility/zbccolwertutil.cpp
cpp_files                           += gpu/utility/testdevice.cpp
cpp_files                           += gpu/utility/testdevicemgr.cpp
cpp_files                           += gpu/utility/lwlink/lwlinkdevif/lwl_devif_mgr.cpp
cpp_files                           += gpu/utility/lwlink/lwlinkdevif/lwl_devif_fact.cpp
cpp_files                           += gpu/utility/lwlink/lwlinkdevif/lwl_devif.cpp
cpp_files                           += gpu/utility/lwlink/lwlinkdevif/lwl_gpu_devif.cpp
cpp_files                           += gpu/utility/fsp.cpp
cpp_files                           += gpu/utility/hulkprocessing/hulkloader.cpp
cpp_files$(ONLY_AMPERE)             += gpu/utility/hulkprocessing/ampere/amperehulkloader.cpp
cpp_files$(ONLY_GA100)              += gpu/utility/hulkprocessing/ampere/ga100hulkloader.cpp
ifeq "$(INCLUDE_LWSWITCH)$(INCLUDE_LWLINK)$(LWCFG_GLOBAL_LWSWITCH_IMPL_LR10)" "truetrue1"
    cpp_files                       += gpu/utility/hulkprocessing/limerockhulkloader.cpp
endif
ifeq "$(filter $(BUILD_OS), cheetah linda winda)" ""
    cpp_files                       += gpu/utility/hulkprocessing/hulklicenseverifier.cpp
    CLIENTPATHS                     += //sw/$(BRANCH)/uproc/fbflcn/inc/vbios/...
    include_dirs                    += $(UPROC_DIR)/fbflcn/inc/vbios
else
    cpp_files                       += diag/mods/stubs/hulklicenseverifier_stub.cpp
endif

include_dirs += $(SDK_DIR)/vmioplugin/inc
ifeq "$(INCLUDE_VGPU_PLUGIN)" "true"
    include_dirs += $(MODS_OUTPUT_DIR)/$(rel_o_dir)/liblwidia-vgpu_gen
else
    CLIENTPATHS += //sw/$(BRANCH)/drivers/vgpu/mods/...
    include_dirs += gpu/vmiop
endif
cpp_files                           += gpu/vmiop/vmiopelw.cpp
cpp_files                           += gpu/vmiop/vmiopelwmgr.cpp
cpp_files                           += gpu/vmiop/js_vmiopelwmgr.cpp

cpp_files                           += gpu/uphy/uphyreglogger.cpp
-include $(gen_cpp_dir)/uphy_makesrc.inc

ifeq "$(INCLUDE_LWLINK)" "true"
    cpp_files                       += gpu/reghal/lwlinkreghal.cpp
    cpp_files$(ONLY_DGPU)           += gpu/fuse/lwlink_fuse.cpp
    cpp_files$(ONLY_DGPU)           += gpu/fuse/hwaccess/lwswitchfuseaccessor.cpp
    cpp_files$(ONLY_DGPU)           += gpu/fuse/hwaccess/lrfuseaccessor.cpp
    cpp_files$(ONLY_DGPU)           += gpu/fuse/hwaccess/lsfuseaccessor.cpp
    cpp_files                       += gpu/lwlink/lwlinkimpl.cpp
    cpp_files                       += gpu/lwlink/pascallwlink.cpp
    cpp_files                       += gpu/lwlink/voltalwlink.cpp
    cpp_files                       += gpu/lwlink/turinglwlink.cpp
    cpp_files                       += gpu/lwlink/amperelwlink.cpp
    cpp_files                       += gpu/lwlink/ga10xlwlink.cpp
    cpp_files$(ONLY_HOPPER)         += gpu/lwlink/hopperlwlink.cpp
    cpp_files$(ONLY_ARM)            += gpu/lwlink/xavierlwlink.cpp
    ifneq "$(BUILD_OS)" "cheetah"
        cpp_files$(ONLY_ARM)        += gpu/lwlink/xavier_mfg_lwlink.cpp
    else
        cpp_files                   += gpu/lwlink/xavier_tegra_lwlink.cpp
    endif
    cpp_files                       += gpu/lwlink/simlwlink.cpp

    cpp_files                       += gpu/lwlink/tpcounters/lwlthroughputcountersimpl.cpp
    cpp_files                       += gpu/lwlink/tpcounters/lwlthroughputcounters_v1.cpp
    cpp_files                       += gpu/lwlink/tpcounters/lwlthroughputcounters_v2.cpp
    cpp_files                       += gpu/lwlink/tpcounters/lwlthroughputcounters_v2sw.cpp
    cpp_files                       += gpu/lwlink/tpcounters/lwlthroughputcounters_v3.cpp
    cpp_files                       += gpu/lwlink/tpcounters/lwlthroughputcounters_v4.cpp

    cpp_files                       += gpu/utility/lwlink/lwlinkdev.cpp
    cpp_files                       += gpu/utility/lwlink/lwlerrorcollector.cpp
    cpp_files                       += gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr.cpp
    cpp_files                       += gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr_impl.cpp
    cpp_files$(ONLY_DGPU)           += gpu/utility/lwlink/lwlinkdevif/lwl_libif.cpp
    ifeq "$(INCLUDE_IBMNPU)" "true"
        cpp_files$(ONLY_DGPU)       += gpu/lwlink/ibmnpulwlink.cpp
        cpp_files$(ONLY_DGPU)       += gpu/pcie/ibmnpupcie.cpp
        ifeq "$(LWCFG_GLOBAL_LWSWITCH_IMPL_LR10)" "1"
            cpp_files$(ONLY_DGPU)   += gpu/lwlink/ibmnpulwlink_p9p.cpp
            cpp_files$(ONLY_DGPU)   += gpu/utility/lwlink/lwlinkdevif/lwl_ibmnpu_p9p_dev.cpp
        endif
        cpp_files$(ONLY_DGPU)       += gpu/utility/lwlink/lwlinkdevif/lwl_ibmnpu_dev.cpp
        cpp_files$(ONLY_DGPU)       += gpu/utility/lwlink/lwlinkdevif/lwl_ibmnpu_p8p_dev.cpp
        cpp_files$(ONLY_DGPU)       += gpu/utility/lwlink/lwlinkdevif/lwl_ibmnpu_devif.cpp
        cpp_files$(ONLY_DGPU)       += gpu/utility/lwlink/lwlinkdevif/lwl_ibmnpu_libif_user.cpp
    endif
    cpp_files                       += gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr_auto.cpp
    ifeq "$(use_mods_lwlink)" "true"
        cpp_files$(ONLY_DGPU)       += gpu/utility/lwlink/lwlinkdevif/lwl_lwlink_libif_user.cpp
    else
        cpp_files$(ONLY_DGPU)       += gpu/utility/lwlink/lwlinkdevif/lwl_lwlink_libif_kernel.cpp
    endif
    ifeq "$(INCLUDE_FM)" "true"
        cpp_files                   += gpu/utility/lwlink/lwlinkdevif/lwl_fabricmanager_libif_user.cpp
    endif
    cpp_files$(ONLY_ARM)            += gpu/utility/lwlink/lwlinkdevif/lwl_tegra_libif_kernel.cpp
    ifneq "$(BUILD_OS)" "cheetah"
        cpp_files$(ONLY_ARM)        += gpu/utility/lwlink/lwlinkdevif/lwl_tegramfg_devif.cpp
        cpp_files$(ONLY_ARM)        += gpu/utility/lwlink/lwlinkdevif/lwl_tegramfg_dev.cpp
    endif
    cpp_files                       += gpu/utility/lwlink/lwlinkdevif/lwl_sim_dev.cpp
    cpp_files                       += gpu/utility/lwlink/lwlinkdevif/lwl_simgpu_dev.cpp
    ifeq "$(INCLUDE_EBRIDGE)" "true"
        cpp_files                   += gpu/utility/lwlink/lwlinkdevif/lwl_ebridge_devif.cpp
    endif
    ifeq "$(INCLUDE_LWSWITCH)" "true"
        cpp_files                      += gpu/lwlink/lwswitchlwlink.cpp
        cpp_files                      += gpu/lwlink/simlwswitchlwlink.cpp
        ifeq "$(LWCFG_GLOBAL_LWSWITCH_IMPL_LR10)" "1"
            cpp_files                  += gpu/utility/lwlink/lwlinkdevif/lwl_limerock_dev.cpp
            cpp_files                  += gpu/lwlink/limerocklwlink.cpp
            cpp_files                  += gpu/lwlink/simlimerocklwlink.cpp
            cpp_files                  += gpu/lwlink/trexlwlink.cpp
        endif
        ifeq "$(LWCFG_GLOBAL_LWSWITCH_IMPL_LS10)" "1"
            cpp_files                  += gpu/utility/lwlink/lwlinkdevif/lwl_laguna_dev.cpp
            cpp_files                  += gpu/lwlink/lagunalwlink.cpp
        endif
        cpp_files                      += gpu/utility/lwswitchfalcon.cpp
        cpp_files                      += gpu/utility/lwlink/lwlinkdevif/lwl_lwswitch_dev.cpp
        cpp_files                      += gpu/utility/lwlink/lwlinkdevif/lwl_simlwswitch_dev.cpp
        cpp_files                      += gpu/utility/lwlink/lwlinkdevif/lwl_lwswitch_devif.cpp
        cpp_files                      += gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr_protobuf.cpp
        ifeq "$(use_mods_lwswitch)" "true"
            cpp_files                  += gpu/utility/lwlink/lwlinkdevif/lwl_lwswitch_libif_user.cpp
        else
            cpp_files                  += gpu/utility/lwlink/lwlinkdevif/lwl_lwswitch_libif_kernel.cpp
        endif
        cpp_files                      += gpu/pcie/lwswitchpcie.cpp
        ifeq "$(LWCFG_GLOBAL_LWSWITCH_IMPL_LR10)" "1"
            cpp_files                      += gpu/pcie/limerockpcie.cpp
        endif
        ifeq "$(LWCFG_GLOBAL_LWSWITCH_IMPL_LS10)" "1"
            cpp_files                      += gpu/pcie/lagunapcie.cpp
        endif
        cpp_files                      += gpu/i2c/lwswitchi2c.cpp
        cpp_files                      += gpu/gpio/lwswitchgpio.cpp
    endif
    ifeq "$(ONLY_ARM)" ""
        TEGRA_LWLINK_HEADER_DIR = $(RMAPI_TEGRA_MIRROR)/include/outmirror/tegra_top/core-private/include
        $(o_dir)/lwl_tegra_libif_kernel$(obj_suffix): include_dirs += $(TEGRA_LWLINK_HEADER_DIR)
        $(o_dir)/xavierlwlink$(obj_suffix):           include_dirs += $(TEGRA_LWLINK_HEADER_DIR)
        $(o_dir)/lwl_tegra_libif_kernel$(obj_suffix): submake.rmapi_tegra_mirror.build
        $(o_dir)/xavierlwlink$(obj_suffix):           submake.rmapi_tegra_mirror.build
    endif
else
    cpp_files                       += stub/lwlinkdev_stub.cpp
    cpp_files                       += stub/lwlinkimpl_stub.cpp
endif
ifeq "$(INCLUDE_LWDA)" "true"
    cpp_files                       += gpu/utility/lwsurf.cpp
else
    cpp_files                       += stub/lwsurf_stub.cpp
endif
cpp_files                           += gpu/perfmon/js_perfmon.cpp
cpp_files                           += gpu/utility/hbmdev.cpp
cpp_files                           += gpu/utility/protmanager.cpp
cpp_files                           += gpu/hbm/hbmimpl.cpp
cpp_files                           += gpu/hbm/hbm_spec_devid.cpp
cpp_files                           += gpu/hbm/pascalhbm.cpp
cpp_files                           += gpu/hbm/voltahbm.cpp
cpp_files                           += gpu/hbm/mbist/mbist.cpp
cpp_files                           += gpu/hbm/mbist/mbistsamsung.cpp
cpp_files                           += gpu/hbm/mbist/mbistskhynix.cpp

# modeset.nxt code
c_files                             += gpu/display/modeset.nxt/CODE/digmode.c
c_files                             += gpu/display/modeset.nxt/CODE/dmt.c
c_files                             += gpu/display/modeset.nxt/CODE/edid.c
c_files                             += gpu/display/modeset.nxt/CODE/gtf.c
c_files                             += gpu/display/modeset.nxt/CODE/gtfmathc.c
c_files                             += gpu/display/modeset.nxt/CODE/modeset.c
c_files                             += gpu/display/modeset.nxt/CODE/restime.c
c_files                             += gpu/display/modeset.nxt/CODE/timing.c
c_files                             += gpu/display/modeset.nxt/CODE/utils.c
c_files                             += gpu/display/modeset.nxt/mods/osapi.c

ifeq "$(filter 0 1.% 2.% 3.% 4.2.%,$(toolchain_rev))" ""
    $(o_dir)/restime$(obj_suffix): warnings += -Wno-cast-qual
endif

ifneq "$(filter 7.%,$(toolchain_rev))" ""
    # Macros in modeset library don't check the return value of snprintf
    # and gcc 7 issues a warning if the printed string exceeds storage size.
    $(o_dir)/lwt_tv$(obj_suffix):          warnings += -Wno-format-truncation
    $(o_dir)/lwt_edidext_861$(obj_suffix): warnings += -Wno-format-truncation
    $(o_dir)/lwt_edid$(obj_suffix):        warnings += -Wno-format-truncation
endif

# modeset code
-include $(DRIVERS_DIR)/modeset/hdmipacket/hdmipacketsrc.inc

SOFTFLOAT_DIR = $(DRIVERS_DIR)/common/softfloat
-include $(SOFTFLOAT_DIR)/lwpu/softfloatsrc.inc


c_files                             += $(addprefix $(DRIVERS_DIR)/modeset/hdmipacket/,$(HDMIPACKET_SOURCES))
c_files                             += $(SOFTFLOAT_SOURCES)
c_files                             += $(DRIVERS_DIR)/modeset/timing/lwt_util.c
c_files                             += $(DRIVERS_DIR)/modeset/timing/lwt_dsc_pps.c
c_files                             += $(DRIVERS_DIR)/modeset/timing/lwt_cvt.c
c_files                             += $(DRIVERS_DIR)/modeset/timing/lwt_dmt.c
c_files                             += $(DRIVERS_DIR)/modeset/timing/lwt_gtf.c
c_files                             += $(DRIVERS_DIR)/modeset/timing/lwt_tv.c
c_files                             += $(DRIVERS_DIR)/modeset/timing/lwt_edidext_861.c
c_files                             += $(DRIVERS_DIR)/modeset/timing/lwt_edidext_displayid.c
c_files                             += $(DRIVERS_DIR)/modeset/timing/lwt_edidext_displayid20.c
c_files                             += $(DRIVERS_DIR)/modeset/timing/lwt_edid.c
c_files                             += $(DRIVERS_DIR)/modeset/timing/lwt_displayid20.c

# modeset code dependencies
include_dirs                        += $(SOFTFLOAT_DIR)/lwpu
include_dirs                        += $(SOFTFLOAT_DIR)/source
include_dirs                        += $(SOFTFLOAT_DIR)/source/include
include_dirs                        += $(SOFTFLOAT_DIR)/source/8086-SSE

defines                             += -DSOFTFLOAT_ROUND_ODD=1
defines                             += -DSOFTFLOAT_FAST_DIV32TO16=1
defines                             += -DSOFTFLOAT_FAST_DIV64TO32=1
defines                             += -DDP_OPTION_FAKE_MODEL_ENABLED=1
defines                             += -DDP_OPTION_IP_SENSITIVE=1
defines                             += -DLWT_IP_SENSITIVE=1
defines                             += -DLWHDMIPKT_IP_SENSITIVE=1

# SoftFloat source files hit warnings about applying unary minus operator on unsigned value
# The code is functionally correct. Since this is external (Open Source) code, try to maintain
# original version, we are not making changes on it. Disable compiler warnings instead.
ifneq (,$(filter $(BUILD_OS), sim linuxmfg cheetah linux))
    $(o_dir)/f64_to_i64_r_minMag$(obj_suffix):  warnings += -Wno-sign-compare
    $(o_dir)/s_roundPackToF16$(obj_suffix):     warnings += -Wno-unused-label
    $(o_dir)/s_roundPackToF32$(obj_suffix):     warnings += -Wno-unused-label
    $(o_dir)/s_roundPackToF64$(obj_suffix):     warnings += -Wno-unused-label
endif
ifeq "$(BUILD_OS)" "macosxmfg"
    $(o_dir)/f64_to_i64_r_minMag$(obj_suffix):  warnings += -Wno-sign-compare
endif
ifneq (,$(filter $(BUILD_OS), winmfg winsim win32))
    $(addprefix $(o_dir)/,$(notdir $(SOFTFLOAT_SOURCES:.c=$(obj_suffix)))): warnings += -wd4244 -wd4146
endif

#-------------------------------------------------------------------------------
#
# Some platforms don't support the OpenGL tests yet.
#
#-------------------------------------------------------------------------------
ifeq "$(INCLUDE_OGL)" "true"

    defines                         += -DINCLUDE_OGL=1

    include_dirs                    += $(DRIVERS_DIR)/OpenGL/include
    include_dirs                    += $(DRIVERS_DIR)/OpenGL/lwInc
    include_dirs                    += $(DRIVERS_DIR)/OpenGL/win/dgl
    include_dirs                    += opengl
    CLIENTPATHS                     += //sw/$(BRANCH)/drivers/OpenGL/...
    CLIENTPATHS                     += //sw/$(BRANCH)/drivers/rtcore/...
    CLIENTPATHS                     += //sw/$(BRANCH)/drivers/cglang/...
    CLIENTPATHS                     += //sw/$(BRANCH)/drivers/unix/allocator/...

    cpp_files                       += opengl/glgsurf.cpp
    cpp_files                       += opengl/glgpsurf.cpp
    cpp_files                       += opengl/js_mgl.cpp
    cpp_files                       += opengl/mglutil.cpp
    cpp_files                       += opengl/mgl_dtop.cpp
    cpp_files                       += opengl/mgl_gpu.cpp
    cpp_files                       += opengl/mglcoverage.cpp

    ifeq "$(INCLUDE_VULKAN)" "true"
        include_dirs                += $(DRIVERS_DIR)/OpenGL/vulkan/include
        include_dirs                += vulkan

        basic_release_files         += vulkan/vktextures.bin

        cpp_files                   += vulkan/util.cpp
        cpp_files                   += vulkan/vkgoldensurfaces.cpp
        cpp_files                   += vulkan/vkimage.cpp
        cpp_files                   += vulkan/vklayers.cpp
        cpp_files                   += vulkan/vkbuffer.cpp
        cpp_files                   += vulkan/vktexture.cpp
        cpp_files                   += vulkan/vkframebuffer.cpp
        cpp_files                   += vulkan/vkutil.cpp
        cpp_files                   += vulkan/vkerror.cpp
        cpp_files                   += vulkan/vkrenderpass.cpp
        cpp_files                   += vulkan/vkdescriptor.cpp
        cpp_files                   += vulkan/swapchain.cpp
        cpp_files                   += vulkan/vkcmdbuffer.cpp
        cpp_files                   += vulkan/vkfence.cpp
        cpp_files                   += vulkan/vksemaphore.cpp
        cpp_files                   += vulkan/vkpipeline.cpp
        cpp_files                   += vulkan/vkshader.cpp
        cpp_files                   += vulkan/vkmods.cpp
        cpp_files                   += vulkan/vkdev.cpp
        cpp_files                   += vulkan/vkphysdev.cpp
        cpp_files                   += vulkan/vkinstance.cpp
        cpp_files                   += vulkan/vksampler.cpp
        cpp_files                   += vulkan/vktexfill.cpp
        cpp_files                   += vulkan/js_vkutil.cpp
        cpp_files                   += vulkan/vkquery.cpp
        cpp_files                   += vulkan/vkasgen.cpp
        cpp_files                   += vulkan/shared_sources/lwbc7/FastCompressBC7.cpp
    endif # INCLUDE_VULKAN
endif  # INCLUDE_OGL

ifeq "$(INCLUDE_VKLAYERS)" "true"
    CLIENTPATHS += //sw/mods/vulkan/validationlayers/...
    ifeq "$(VK_LAYERS_USE_LWMAKE)" "true"
        internal_release_files += $(GEN_LIB_DIR)/lwmake/khronos_validation/libVkLayer_khronos_validation.so
    else
        internal_release_files += $(GEN_LIB_DIR)/libVkLayer_khronos_validation/libVkLayer_khronos_validation.so
    endif
endif

ifeq "$(INCLUDE_VGPU_PLUGIN)" "true"
    dll_release_files += $(GEN_LIB_DIR)/liblwidia-vgpu/liblwidia-vgpu$(dll_suffix)
endif

basic_release_files                 += gpu/js/comngpu.js
basic_release_files                 += gpu/js/drf.h
basic_release_files                 += gpu/js/fpk_comm.h
basic_release_files$(ONLY_DGPU)     += gpu/js/boards.js
basic_release_files                 += gpu/js/edid.js
basic_release_files                 += gpu/js/gpuargs.js
basic_release_files                 += gpu/js/gpudecls.js
ifneq (,$(filter $(BUILD_ARCH), ppc64le aarch64))
    basic_release_files             += gpu/js/nputest.js
    unencrypted_rel_files           += gpu/js/npudecls.js
endif
tegra_release_files                 += gpu/js/tegraperf.js
basic_release_files                 += gpu/js/gpulist.js
basic_release_files                 += gpu/js/resumehandler.js
basic_release_files                 += gpu/js/boostbase.js
basic_release_files                 += gpu/js/hbmtemptest.js
basic_release_files                 += gpu/js/perfpunish.js
basic_release_files                 += gpu/js/perfswitch.js
basic_release_files                 += gpu/js/pwmtest.js
basic_release_files                 += gpu/js/clockstest.js
basic_release_files                 += gpu/js/vkstress.js
basic_release_files$(ONLY_PC)       += gpu/js/compute.spc
basic_release_files$(ONLY_PC)       += gpu/js/linpack_tests.js
basic_release_files                 += gpu/js/powerbalancing.js
basic_release_files                 += gpu/js/isenseadc.js
basic_release_files                 += gpu/js/checkinputvoltage.js
basic_release_files$(ONLY_PC)       += gpu/js/gpiotest.js
basic_release_files                 += gpu/js/systhermsanity.js
basic_release_files$(ONLY_PC)       += gpu/js/oqa.spc
basic_release_files                 += gpu/js/thermresgpu.js
ifeq "$(BUILD_OS)" "cheetah"
    basic_release_files             += gpu/js/cheetah.spc
    internal_release_files          += gpu/js/tegrasim.spc
endif
internal_release_files$(ONLY_ARM)   += gpu/js/tegra_f_v_adjust.spc
internal_release_files              += gpu/js/controlrun.spc
basic_release_files$(ONLY_PC)       += gpu/js/boarddb.spc
basic_release_files$(ONLY_DGPU)     += gpu/js/idt.spc
basic_release_files                 += gpu/js/idt.js
pvs_only_files                      += gpu/js/modstest.js
internal_release_files              += gpu/js/ps30test.js
internal_release_files              += gpu/js/ismtest.js
basic_release_files                 += gpu/js/perflim.js
internal_release_files              += gpu/js/usr.spc
internal_release_files              += gpu/js/emu.spc
basic_release_files                 += gpu/js/pstate.js
ifeq "$(INCLUDE_GPUTEST)" "true"
    basic_release_files             += gpu/js/gputest.js
    pvs_release_files               += gpu/js/pvstest.js
    PVSTEST_MS                      ?= gpu/js/pvstest_ms.js
    pvs_release_files               += $(PVSTEST_MS)
endif
basic_release_files                 += gpu/js/thermcal.js
basic_release_files                 += gpu/js/c2c.js
basic_release_files                 += gpu/js/lwlink.js
basic_release_files                 += gpu/js/lwlink_dln_dump.js
basic_release_files$(ONLY_DGPU)     += gpu/js/hbm.js
basic_release_files$(ONLY_DGPU)     += gpu/js/repair.js
basic_release_files$(ONLY_DGPU)     += gpu/js/fpf.js
pvs_only_files                      += gpu/js/bgtests.spc
basic_release_files                 += gpu/display/dpc/1920x1080_2Head1Or.dpc
internal_release_files$(ONLY_DGPU)  += gpu/display/dpc/7680x4320_8K_2Head1Or_DPHBR3.dpc
internal_release_files$(ONLY_DGPU)  += gpu/display/dpc/7680x4320_8K_2Head1Or_FRL.dpc
internal_release_files$(ONLY_DGPU)  += gpu/display/dpc/3840x2160_4K_TMDS_A.dpc
internal_release_files              += gpu/display/dpc/1024x768_volta.dpc
internal_release_files              += gpu/display/dpc/1024x768_turing.dpc
internal_release_files              += gpu/display/dpc/1024x768_turing_TMDS_only.dpc
internal_release_files              += gpu/display/dpc/1024x768_2Head1Or_mst_multi_stream.dpc
internal_release_files$             += gpu/display/dpc/1024x768_ampere.dpc
pvs_only_files                      += gpu/display/dpc/3840x2160_2Head1Or_Frl.dpc
ifeq "$(INCLUDE_MUCC)" "true"
    basic_release_files$(ONLY_DGPU)     += gpu/js/mucc.js
    internal_release_files$(ONLY_DGPU)  += ../mucc/atetranslator/mucc2js.py
    exelwtable_files                    += ../mucc/atetranslator/mucc2js.py
    internal_release_files$(ONLY_DGPU)  += ../mucc/atetranslator/muccatetranslator.py
    exelwtable_files                    += ../mucc/atetranslator/muccatetranslator.py
    pvs_release_files$(ONLY_DGPU)       += ../mucc/sample.mucc
    internal_release_files$(ONLY_DGPU)  += ../mucc/sample.mucc
endif
ifeq "$(BUILD_OS)" "sim"
    internal_release_files              += gpu/js/png-to-tga.js
    internal_release_files              += gpu/display/dpc/600x40_2Head1Or_DSC_fmodel.dpc
    internal_release_files              += gpu/display/dpc/200x80_fmodel.dpc
    internal_release_files              += gpu/display/dpc/128x112_fmodel.dpc
endif

ifeq "$(INCLUDE_MODS_GTEST)" "true"
    internal_release_files              += gpu/display/dpc/200x80_fmodel.dpc
    internal_release_files              += gpu/display/dpc/256x48_minimal.dpc
    internal_release_files              += gpu/display/dpc/400x80_dsc_fmodel.dpc
    internal_release_files              += gpu/display/dpc/5120x2880_5K_Dual_SST.dpc
    internal_release_files              += gpu/display/dpc/Test46_StressModes_TMDS_A.dpc
    internal_release_files              += gpu/display/dpc/Test46_StressModes_DP_SST.dpc
endif

ifeq "$(BUILD_OS)" "cheetah"
internal_release_files      += gpu/display/dpc/640x480_dp_dsc.dpc
internal_release_files      += gpu/display/dpc/image640x480.png
internal_release_files      += gpu/display/dpc/1440x288_2Head1Or_dp_dsc.dpc
internal_release_files      += gpu/display/dpc/3840_2160_2h1or_dp_dsc.dpc
internal_release_files      += gpu/display/dpc/image1920x2160_1.png
internal_release_files      += gpu/display/dpc/image1920x2160_2.png
internal_release_files      += gpu/display/dpc/image1440x288.png
internal_release_files      += gpu/display/dpc/1440x480_2Head1Or_dp_dsc.dpc
internal_release_files      += gpu/display/dpc/600x40_2Head1Or_dp_dsc_vdk.dpc
internal_release_files      += gpu/display/dpc/720x480_hdmifrl_dsc.dpc
internal_release_files      += gpu/display/dpc/720x480_hdmifrl_dsc_yuv444.dpc
internal_release_files      += gpu/display/dpc/200x80_tmds_vdk.dpc
internal_release_files      += gpu/display/dpc/image200x80.png
internal_release_files      += gpu/display/dpc/160x120_tmds_vdk.dpc
internal_release_files      += gpu/display/dpc/image160x120.png
internal_release_files      += gpu/display/dpc/640x480_tmds_rotflip.dpc
internal_release_files      += gpu/display/dpc/160x120_dp_mst_vdk.dpc
internal_release_files      += gpu/display/dpc/200x80_tmds_frl_vdk.dpc
internal_release_files      += gpu/display/dpc/800x600_t234_tmds.dpc
internal_release_files      += gpu/display/dpc/800x600_t234_dp.dpc
internal_release_files      += gpu/display/dpc/800x600_t23x_dp_mst.dpc
internal_release_files      += gpu/display/dpc/7680x4320_t23x_8K_2Head1Or_DPHBR3.dpc
endif

ifeq "$(INCLUDE_GPU_SPECIFIC)" "true"
pvs_release_files$(ONLY_DGPU)     += $(CHIP_XML_DIR)/xml_v1/gm107_f.xml
pvs_release_files$(ONLY_DGPU)     += $(CHIP_XML_DIR)/xml_v1/gm108_f.xml
pvs_release_files$(ONLY_DGPU)     += $(CHIP_XML_DIR)/xml_v1/gm200_f.json
pvs_release_files$(ONLY_DGPU)     += $(CHIP_XML_DIR)/xml_v1/gm204_f.json
pvs_release_files$(ONLY_DGPU)     += $(CHIP_XML_DIR)/xml_v1/gm206_f.json
pvs_release_files$(ONLY_DGPU)       += $(CHIP_XML_DIR)/xml_v1/gp108_f.json
pvs_release_files$(ONLY_DGPU)       += $(CHIP_XML_DIR)/xml_v1/tu102_f.json
pvs_release_files$(ONLY_DGPU)       += $(CHIP_XML_DIR)/xml_v1/ga104_f.json
pvs_release_files$(ONLY_DGPU)       += $(CHIP_XML_DIR)/xml_v1/ga106_f.json
CLIENTPATHS                         += //sw/mods/chipfuse/...
endif

ifeq "$(INCLUDE_GPU)" "true"
    pvs_release_files$(ONLY_PC) += $(HULK_DIR)/gp100_ecc_int_vs.js
    pvs_release_files$(ONLY_PC) += $(HULK_DIR)/gp104_instinsys_hulk_license.js
    pvs_release_files$(ONLY_PC) += $(HULK_DIR)/gp106_instinsys_hulk_license.js
    pvs_release_files$(ONLY_PC) += $(HULK_DIR)/ausvrl8216_ecc_hulk_license.js
    pvs_release_files$(ONLY_PC) += $(HULK_DIR)/HULK_005ff2c000000019e2021591_006002c000000019e2021591_00a0544000000019e1070991_0140344000000019e1070991_ECC_MemQual_ID809.js
    pvs_release_files$(ONLY_PC) += $(HULK_DIR)/HULK_00e0104000000019e1832191_ECC_I2CC_ID1100.js
    pvs_release_files$(ONLY_PC) += $(HULK_DIR)/HULK_MemQual_10025000000002160471DD1_10231.hulk
    pvs_release_files$(ONLY_PC) += $(HULK_DIR)/HULK_00c011c00000005e334309c4_016001c00000005e334309c4_MemQual_ID18468.hulk
    pvs_release_files$(ONLY_PC) += $(HULK_DIR)/HULK_00c011c00000005e334309c4_016001c00000005e334309c4_MemQual_ID18468.xml
    pvs_release_files$(ONLY_PC) += $(HULK_DIR)/HULK_15f7_15fa_ECC_I2CC_OverT_ID1415.js
    pvs_release_files$(ONLY_PC) += $(HULK_DIR)/tu102_instinsys_hulk_license.js
    pvs_release_files$(ONLY_PC) += $(HULK_DIR)/tu102_ecc_int_vs_hulk_license.js
    pvs_release_files$(ONLY_PC) += $(HULK_DIR)/HULK_016040c00000002a31e005c4_010020c00000002a31e005c4_MemQual_ID18463.hulk
    CLIENTPATHS       += //sw/mods/hulk/...

    ifeq "$(INCLUDE_GPU_SPECIFIC)" "true"
        # RM perf value files.  Need to include it here because it is read from C++ within a test
        # when certain command line options are present and is not optional like other pvs files
        pvs_release_files$(ONLY_PC) += $(PVS_STAT_DIR)/ausvrl8767_rmperf_t147.js
    endif
endif

ifeq "$(INCLUDE_BOARDS_DB)" "true"
    defines += -DINCLUDE_BOARDS_DB=1
    CLIENTPATHS                     += //sw/mods/boardsdb/...

    basic_release_files             += $(o_dir)/boards.db
    all_gen_files                   += $(o_dir)/boards.db

    # Include all boards.db files for stealth and sanity builds only
    ifeq "$(INCLUDE_GPU_SPECIFIC)" "false"
        boards_db_files := $(BOARDS_DB_DIR)/boards.db
    else ifeq "$(BUILD_CFG)" "sanity"
        INCLUDE_ALL_BOARDS_DB ?= true
    else ifneq "$(STEALTH_MODE)" ""
        INCLUDE_ALL_BOARDS_DB ?= true
    endif

    INCLUDE_ALL_BOARDS_DB ?= false

    ifeq "$(INCLUDE_ALL_BOARDS_DB)" "true"
        boards_db_files := $(wildcard $(BOARDS_DB_DIR)/*.db)
    endif

install : $(o_dir)/boards.db
zip_pvs_nodep : $(o_dir)/boards.db
endif

$(o_dir)/boards.db : $(boards_db_files)
	@$(ECHO) "Creating $@"
	$(Q)$(PERL) -ne 'BEGIN{open BOARDS_FH,">", shift @ARGV or die "Error creating boards.db file: $$!\n"} print BOARDS_FH $$_;' $@ $^

# Default display image
basic_release_files                 += gpu/tests/traces/default.bin

# fuse read falcon
basic_release_files$(ONLY_DGPU)     += gpu/fuse/fuseread_gm206.bin
basic_release_files$(ONLY_DGPU)     += gpu/fuse/fuseread_gp10x.bin

# FUB binary
basic_release_files$(ONLY_DGPU)     += gpu/fuse/fub_ga100.bin
# RIR ilwalidation binary
basic_release_files$(ONLY_DGPU)     += gpu/fuse/frb_ga100.bin

internal_release_files              += gpu/tests/traces/blkwhite.mcs

internal_release_files              += gpu/js/boardhal.js
internal_release_files              += gpu/js/fermihal.js
internal_release_files$(ONLY_DGPU)  += gpu/js/fuseutil.js
internal_release_files$(ONLY_DGPU)  += gpu/js/fuseutil2.js
internal_release_files$(ONLY_DGPU)  += gpu/js/fuseblow.js
internal_release_files$(ONLY_DGPU)  += gpu/js/fuseblow2.js
internal_release_files$(ONLY_DGPU)  += gpu/js/downbin.js
internal_release_files$(ONLY_DGPU)  += gpu/js/downbin2.js
internal_release_files              += gpu/js/gpugen.js
internal_release_files              += gpu/js/gpuleak.js
internal_release_files              += gpu/js/svcmods.js

ifeq "$(sim_build)" "true"
    basic_release_files             += gpu/js/simargs.js
else ifeq "$(INCLUDE_RMTEST)" "true"
    internal_release_files          += gpu/js/simargs.js
endif

internal_release_files              += gpu/js/boardbin.js
internal_release_files              += gpu/js/comnsoc.js
internal_release_files              += gpu/js/dispt_sr.h
internal_release_files              += gpu/js/disputil.js
internal_release_files              += gpu/js/gpuindex.js
internal_release_files              += gpu/js/help.js
internal_release_files              += gpu/js/instrdef.h
internal_release_files              += gpu/js/jsvp.h
internal_release_files              += gpu/js/zpi.js

ifeq "$(sim_build)" "true"
    internal_release_files          += gpu/js/udta.js
    internal_release_files          += sim/modssim_mfg_cmds.tcl
    internal_release_files          += sim/pvs_mods_fmodel_cmds.tcl
endif

ifeq "$(BUILD_OS)" "linuxmfg"
    internal_release_files          += tools/modsi.sh
    exelwtable_files                += tools/modsi.sh
endif

pvs_release_files$(ONLY_PC)         += gpu/js/stealth_end_user.js
pvs_release_files$(ONLY_PC)         += gpu/js/629.spc

ifeq "$(sim_build)" "true"

ifeq "$(INCLUDE_DFPGA)" "true"
    internal_release_files$(ONLY_PC) += sim/dfpga_cmds.tcl
    internal_release_files$(ONLY_PC) += sim/dfpga_cmds_ga10x.tcl
    internal_release_files$(ONLY_PC) += sim/dfpga_cmds_ad10x.tcl
    internal_release_files$(ONLY_PC) += sim/resources/tu102f_fpga_sddr3_nopll_no_ifr.rom
    internal_release_files$(ONLY_PC) += sim/resources/tu102f_fpga_sddr3_nopll_dp-mst_no_ifr.rom
    internal_release_files$(ONLY_PC) += sim/resources/ga102f_fpga_sddr3_nopll_dcb41.rom
    internal_release_files$(ONLY_PC) += sim/resources/ga102f_fpga_sddr3_nopll_dcb41_2head1or_dualsst.rom
    internal_release_files$(ONLY_PC) += gpu/display/dpc/800x600_dfpga.dpc
    internal_release_files$(ONLY_PC) += gpu/display/dpc/test_ampere.dpc
    internal_release_files$(ONLY_PC) += gpu/display/dpc/ga10x_qt_dfpga_tmds_b.dpc
    internal_release_files$(ONLY_PC) += gpu/display/dpc/ga10x_qt_dfpga_dp_2H1OR.dpc
endif

ifneq (,$(filter $(BUILD_OS), winsim sim))
    ifeq "$(INCLUDE_DVS_SANITY)" "true"
        internal_release_files$(ONLY_PC) += sim/resources/gv100_sim_hbm.rom
        internal_release_files$(ONLY_PC) += sim/resources/gv100_sim_hbm_dcb-ultimate.rom
        internal_release_files$(ONLY_PC) += sim/resources/tu102_sim_gddr5.rom
        internal_release_files$(ONLY_PC) += sim/resources/tu102_sim_gddr5_dcb-ultimate.rom
        internal_release_files$(ONLY_PC) += sim/resources/tu102_sim_gddr6.rom
        internal_release_files$(ONLY_PC) += sim/resources/tu102_sim_gddr6_dcb-ultimate.rom
        internal_release_files$(ONLY_PC) += $(CHIP_XML_DIR)/xml_v1/tu102_f.json
        internal_release_files$(ONLY_PC) += sim/resources/ga100_sim_hbm.rom
        internal_release_files$(ONLY_PC) += sim/resources/ga102_sim_gddr6.rom
        internal_release_files$(ONLY_PC) += sim/resources/ga102_sim_gddr6_dcb-ultimate.rom
        internal_release_files$(ONLY_PC) += sim/resources/ga102_sim_gddr6_dcb-ultimate-qt.rom
        internal_release_files$(ONLY_PC) += sim/resources/ad102_sim_gddr6.rom
        internal_release_files$(ONLY_PC) += sim/resources/ad102_sim_gddr6x_dcb-ultimate.rom
        internal_release_files$(ONLY_PC) += sim/resources/gh100_sim_hbm.rom
        internal_release_files$(ONLY_PC) += specs/gv100sim.spc
        internal_release_files$(ONLY_PC) += specs/ga100fmod.spc
        internal_release_files$(ONLY_PC) += specs/ga102fmod.spc
        internal_release_files$(ONLY_PC) += specs/gh100fmod.spc
        internal_release_files$(ONLY_PC) += specs/ls10fmod.spc
        internal_release_files$(ONLY_PC) += specs/ad102fmod.spc
    endif
endif

ifeq "$(INCLUDE_GPU_SPECIFIC)" "true"
    internal_release_files$(ONLY_PC) += specs/gv100sim.spc
    internal_release_files$(ONLY_PC) += specs/ga100fmod.spc
    internal_release_files$(ONLY_PC) += specs/ga102fmod.spc
    internal_release_files$(ONLY_PC) += specs/gh100fmod.spc
    internal_release_files$(ONLY_PC) += specs/ls10fmod.spc
    internal_release_files$(ONLY_PC) += specs/ad102fmod.spc

    include $(MODS_DIR)/gpu/gfw/makeroms.inc
endif

endif

ifeq "$(da_build)" "true"
    pvs_release_files      += tools/dvsamodel.py
    exelwtable_files       += tools/dvsamodel.py
    internal_release_files += specs/ad102amod.spc
    internal_release_files += specs/ga100amod.spc
    internal_release_files += specs/ga100lwofa.spc
    internal_release_files += specs/ga100lwjpg.spc
    internal_release_files += specs/ga102amod.spc
    internal_release_files += specs/gh100amod.spc
    internal_release_files += specs/amodel_lwenc.spc
    internal_release_files += sim/pvs_mods_amodel_cmds.tcl
    internal_release_files += sim/sim.pl
    exelwtable_files       += sim/sim.pl
endif

ifeq "$(INCLUDE_REFS)" "true"
    internal_release_files$(ONLY_IGPU) += mdiag/generated/gm20b/gm20b_ref.txt.gz
    internal_release_files$(ONLY_IGPU) += mdiag/generated/gm21b/gm21b_ref.txt.gz
    internal_release_files$(ONLY_IGPU) += mdiag/generated/t194/t194_ref.txt.gz
    internal_release_files$(ONLY_IGPU) += mdiag/generated/t234/t234_ref.txt.gz
    internal_release_files$(ONLY_IGPU) += mdiag/generated/t239/t239_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/gm107/gm107_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/gm108/gm108_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/gm200/gm200_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/gm204/gm204_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/gm206/gm206_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/gp100/gp100_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/gp102/gp102_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/gp104/gp104_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/gp106/gp106_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/gp107/gp107_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/gp108/gp108_ref.txt.gz
    released_ref_files$(ONLY_PC)       += mdiag/generated/gv100/gv100_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/gv11b/gv11b_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/tu102/tu102_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/tu104/tu104_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/tu106/tu106_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/tu116/tu116_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/tu117/tu117_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/ga100/ga100_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/ga102/ga102_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/ga103/ga103_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/ga104/ga104_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/ga106/ga106_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/ga107/ga107_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/ga10b/ga10b_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/ga11b/ga11b_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/ga10f/ga10f_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/ad102/ad102_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/ad103/ad103_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/ad104/ad104_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/ad106/ad106_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/ad107/ad107_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/ad10b/ad10b_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/gh100/gh100_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/gh202/gh202_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/gb100/gb100_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/gb102/gb102_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/lr10/lr10_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/ls10/ls10_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/ig000/ig000_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/g000/g000_ref.txt.gz
    internal_release_files$(ONLY_PC)   += mdiag/generated/s000/s000_ref.txt.gz
    internal_release_files             += $(released_ref_files)
endif

ifeq "$(INCLUDE_MDIAG)" "true"
    internal_release_files$(ONLY_PC) += mdiag/gk107/AMODEL_trap_handler.bin
    internal_release_files$(ONLY_PC) += mdiag/gmlit1/gmlit1_trap_handler.bin
    internal_release_files$(ONLY_PC) += mdiag/gmlit2/gmlit2_trap_handler.bin
    internal_release_files$(ONLY_PC) += mdiag/gmlit3/gmlit3_trap_handler.bin
    internal_release_files$(ONLY_PC) += mdiag/gmlit4/gmlit4_trap_handler.bin
    internal_release_files$(ONLY_PC) += mdiag/gmlit6/gmlit6_trap_handler.bin
    internal_release_files           += mdiag/t210/T210_trap_handler.bin
    internal_release_files           += mdiag/t186/T186_trap_handler.bin
endif

ifeq "$(BUILD_TEGRA_EMBEDDED)" "true"
    include gpu/embedded/makesrc.inc
endif

# This stuff should be moved to mdiag or maybe even deleted?
ifeq "$(INCLUDE_MDIAG)" "true"
    ifeq (,$(filter $(BUILD_ARCH),arm armv7))
        cpp_files$(ONLY_DGPU)               += gpu/tests/disp_test.cpp
        cpp_files$(ONLY_DGPU)               += gpu/tests/disp_vga.cpp
        cpp_files$(ONLY_DGPU)               += gpu/tests/disp_vga_gt21x.cpp
        cpp_files$(ONLY_DGPU)               += gpu/tests/disp_vga_gf10x.cpp
        cpp_files$(ONLY_DGPU)               += gpu/tests/disp_poll.cpp
        cpp_files$(ONLY_DGPU)               += gpu/tests/disp_crc.cpp
        cpp_files$(ONLY_DGPU)               += gpu/tests/disp_js.cpp
        cpp_files$(ONLY_DGPU)               += gpu/tests/disp_collect.cpp
        cpp_files$(ONLY_DGPU)               += gpu/tests/disptest/display_base.cpp
        cpp_files$(ONLY_DGPU)               += gpu/tests/disptest/display_01xx.cpp
        cpp_files$(ONLY_DGPU)               += gpu/tests/disptest/display_015x.cpp
        cpp_files$(ONLY_DGPU)               += gpu/tests/disptest/display_02xx.cpp
        cpp_files$(ONLY_DGPU)               += gpu/tests/disptest/display_021x.cpp
        cpp_files$(ONLY_DGPU)               += gpu/tests/disptest/display_022x.cpp
        cpp_files$(ONLY_DGPU)               += gpu/tests/disptest/display_024x.cpp
        cpp_files$(ONLY_DGPU)               += gpu/tests/disptest/display_025x.cpp
        cpp_files$(ONLY_DGPU)               += gpu/tests/disptest/display_027x.cpp
        cpp_files$(ONLY_DGPU)               += gpu/tests/disptest/display_028x.cpp
    endif
endif
