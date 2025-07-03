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

ifneq "$(BUILD_OS)" "cheetah"
    # enable rmconfig build to generate pdb.h required
    # to get/set any PDB property from inside rm test
    TRIGGER_RMCFG = true
endif

include_dirs += gpu/tests/rm/utility
include_dirs += gpu/tests/rm/dispstatecheck
include_dirs += gpu/tests/rm/clk/uct
include_dirs += gpu/tests/rm/generated/displayimp
include_dirs += gpu/tests/rm/imp/config

# RPC generated header file
include_dirs += $(MODS_OUTPUT_DIR)/$(rel_o_dir)/librm_gen

ifeq "$(TRIGGER_RMCFG)" "true"
    include_dirs += $(rmconfig_files_dir)
endif

cpp_files += gpu/tests/rmtest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/bif/rmt_ctrl0080bif.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/bif/rmt_pbicmdtest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/bus/rmt_bar2fault.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/ce/rmt_cl85b5.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/ce/rmt_dmacopydecompress.cpp
ifneq "$(BUILD_OS)" "win32"
    cpp_files$(ONLY_DGPU) += gpu/tests/rm/ce/rmt_selwrecopy.cpp
    cpp_files$(ONLY_DGPU) += gpu/tests/rm/sec2/rmt_sec2worklaunch.cpp
endif
cpp_files$(ONLY_DGPU) += gpu/tests/rm/classlist/rmt_allocobj.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/rmt_clktest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/rmt_clksanity.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/rmt_fermiclkpath.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/rmt_fermiclksanity.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/rmt_fermiclkstress.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/rmt_ndivslowdown.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/rmt_mclkswitch.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/rmt_fermipstateswitch.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/rmt_universalclocktest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/compute/rmt_computeswmethod.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/conf_compute/rmt_cb33.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/conf_compute/rmt_ccbasic.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/conf_compute/rmt_cclaunch.cpp
cpp_files$(ONLY_HOPPER) += gpu/tests/rm/conf_compute/rmt_ccspdm.cpp
cpp_files$(ONLY_HOPPER) += gpu/tests/rm/conf_compute/spdm/rmt_ccspdm_tests.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/ctrl/rmt_ctrlsys.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/ctrl/rmt_eventbuffer.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/rmt_cl9072.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/rmt_hdmiuserspacetest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/rmt_ctrldisp.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/rmt_detectedid.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/rmt_dptest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/rmt_hybridpadtest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/rmt_repeatertest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/rmt_srmtest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/rmt_colorFormatN64bitTest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/rmt_dputest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/displess/rmt_dispbroken.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/rmt_glitchlessmclkswitchrl.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/dispinfra_samples/displayFaking/rmt_sampleDisplayFakingTest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/dispinfra_samples/hdcp/rmt_hdcpSampleTest.cpp
cpp_files$(ONLY_PC) += gpu/tests/rm/dispinfra_samples/LwHdcpApiTest/rmt_lwHdcpApiTest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/dispinfra_samples/modeset/rmt_simpleModesetTest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/rmt_basicWborTest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/rmt_wborMultiPassRigorousTest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/rmt_mscgWbor.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/rmt_crossbar.cpp
cpp_files$(ONLY_PC) += gpu/tests/rm/disp/rmt_dpoptimalpowerforpadlinkPLL.cpp
cpp_files$(ONLY_PC) += gpu/tests/rm/dispinfra_samples/monitorOff/rmt_sampleMonitorOff.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/dispinfra_samples/overlay/rmt_overlayTest.cpp
cpp_files$(ONLY_PC) += gpu/tests/rm/dispinfra_samples/rmt_dtiapitest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/dma/rmt_alloccx2.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/dma/rmt_channelMemInfo.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/ecc/rmt_eccfbfullscantest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/ecc/rmt_eccfbfault.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/ecc/rmt_eccasyncscrubstresstest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/ecc/rmt_eccisrordertest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/ecc/rmt_eccgrerrorinjectiontest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/emp/rmt_emp.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/utility/rmt_flagutil.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/utility/rmt_random.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/utility/rmt_memalloc.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/utility/rmt_memmap.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/utility/rmt_vaspace.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/vaspace/rmt_randomva.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/vaspace/rmt_multiva.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/vaspace/rmt_vaspaceinterop.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/dma/rmt_ctrl0080dma.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/dma/rmt_ctrl2080dma.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/dma/rmt_sparseaccess.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/dma/rmt_sparsevirtual.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/memsys/rmt_acrverify.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_bar1.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_cl0070.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_cl50a0.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_cl907f.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_cl9096.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_cl00f1.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_comprzbctest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_comp.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_ctag.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_ctrl2080fb.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_ctrl208ffbio.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_dupobj.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_fbregions.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/ecc/rmt_eccscrubbeddeallocationtest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_gpucachetest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_lddm.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_mapmem.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_mapmemdma.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_mlimitcheck.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_mmutester.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_outofmem.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_safe64bitaddrop.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_simplmap.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_tc.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_tlbstress.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_verifcomptag.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_virt2phys.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_virtualcontext.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_zerocache.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_bar1limit.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fb/rmt_ctrl0041.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fifo/rmt_active3d.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fifo/rmt_gpufifostress.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fifo/rmt_runlist.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fifo/rmt_cl826f.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fifo/rmt_cla06f.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fifo/rmt_syncpt.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fifo/rmt_enginerr.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fifo/rmt_ctrl2080fifo.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fifo/rmt_changrp.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fifo/rmt_runlistsaverestore.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fifo/rmt_turingqosintr.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fifo/rmt_grpscg.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fifo/rmt_preemption.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fifo/rmt_clc361.cpp
cpp_files$(ONLY_PC) += gpu/tests/rm/dispinfra_samples/rmt_ForceQuiescent.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fifo/rmt_hostscheduling.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fifo/subcontext/rmt_subcontext.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fifo/subcontext/rmt_tpcpartition.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/fifo/subcontext/rmt_watermark.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/gpu/rmt_cl208f.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/gpu/rmt_ctrl2080gpu.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/gpu/rmt_privsectest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/gpu/rmt_privregaccesstest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/gpu/rmt_ctrl0000gpugetuuidinfo.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/gpu/rmt_regopstest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/memsys/rmt_acrtest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/gr/rmt_grswmethod.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/utility/smutil.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/utility/smutilvolta.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/gr/rmt_smpcctrl.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/gr/rmt_preemptctrls.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/gr/rmt_ctrl0080gr.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/gr/rmt_ctrl2080gr.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/gr/rmt_deferredapi.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/gr/rmt_alphabetacb.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/gr/rmt_alphabetacbpascal.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/gr/rmt_grgoldenctxctrl.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/i2c/rmt_ctrli2c.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/spi/rmt_spi.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/imp/rmt_modeset.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/intr/rmt_cl005d.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/intr/rmt_cl906f.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/intr/rmt_ctrl0080intr.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/sec2/rmt_cl95a1sec2.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/sec2/rmt_cl95a1sec2data.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/sec2/rmt_cl95a1sec2basic.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/sec2/rmt_cl95a1sec2apm.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/sec2/rmt_cl95a1sec2gfe.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/sec2/rmt_cl95a1sec2hwv.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/sec2/rmt_cl95a1sec2spdm.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/sec2/rmt_cl95a1sec2vpr.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/sec2/rmt_cl95a1sec2pr.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/sec2/rmt_sec2uprocrtosbasic.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/sec2/rmt_sec2uprocrtoshsswitch.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/sec2/rmt_sec2uprocrtosmutex.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/sec2/rmt_sec2uprocrtosmscg.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/sec2/rmt_sec2uprocrtosrttimer.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/gsp/rmt_gspuprocrtosbasic.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/gsp/rmt_gspuprocrtoscommon.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/gsp/rmt_gspuprocrtoshsswitch.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/gsp/rmt_gspuprocrtosmutex.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/gsp/rmt_gspuprocrtosmscg.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/gsp/rmt_gspuprocrtosrttimer.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/utility/uprocrtos.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/utility/uproctest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/utility/uprocsec2rtos.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/utility/uprocgsprtos.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/sec2/rmt_sec2rtosfakeidle.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/sec2/rmt_sec2rtosmultiplechannel.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/sec2/rmt_cl95a1sec2lwsr.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/se/rmt_sebasic.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/rmt_dpuveriffeaturetest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/pmgr/rmt_pmgrmutexveriftest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/pmgr/rmt_jspmgrtest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/mxm/rmt_mxm.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/perf/rmt_cl844c.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/perf/rmt_perfvf.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/pmu/rmt_pmufuzz.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/pmu/rmt_cl85b6.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/pmu/rmt_ctrl2080mc.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/pmu/rmt_elpg.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/lpwr/rmt_lpwr.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/lpwr/rmt_gpcrg.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/prodtest/rmt_platformcheck.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/prodtest/rmt_prodtest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/psdl/rmt_psdl.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/dmemva/rmt_dmemva.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/dmemva/rmt_dmemva_test_cases.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/dmemva/rmt_dmemva_test_cases_pascal.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/dmemva/rmt_dmemva_test_cases_volta.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/dmemva/rmt_dmemva_test_cases_turing.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/pwrmgmt/rmt_s3s4gpufbcheck.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/pwrmgmt/rmt_suspendresumegpu.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/rc/rmt_lsresetpathverif.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/sampletest/rmt_samplenewtest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/sampletest/rmt_sampleone.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/subdevengines/rmt_subdeviceengines.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/lwlink/rmt_lwlinkverify.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/sli/rmt_p2p.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/sli/rmt_p2plargemem.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/sli/rmt_sliapi.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/sw_test/rmt_cl007d.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/syscon/rmt_gsync.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/syscon/rmt_syscon.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/therm/rmt_therm.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/tmr/rmt_ctrl2080tmr.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/utility/mmuutil.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/utility/rmtestutils.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/vp/rmt_ctrl0080vp.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/dispinfra_samples/rmt_testdisp.cpp
cpp_files$            += gpu/tests/rm/imp/rmt_imptest.cpp
c_files$              += gpu/tests/rm/generated/displayimp/display_imp_core.c
cpp_files$(ONLY_DGPU) += gpu/tests/rm/dispinfra_samples/CRC/rmt_CrcSampleTest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/dispinfra_samples/LUT/rmt_LUTSampleTest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/dispinfra_samples/rmt_DmiMemAcc.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/rmt_cycleresmulti.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/rmt_cycledispcombi.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/rmt_cycledisp.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/rmt_hdmi2.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/pgstats.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/rmt_dispctxdma.cpp
cpp_files$(ONLY_PC) += gpu/tests/rm/disp/rmt_rigorousModesetTest.cpp
cpp_files$(ONLY_PC) += gpu/tests/rm/disp/rmt_dp2test.cpp
cpp_files$(ONLY_PC) += gpu/tests/rm/disp/rmt_dp12mst.cpp
cpp_files$(ONLY_PC) += gpu/tests/rm/disp/rmt_dp12sst.cpp
cpp_files$(ONLY_PC) += gpu/tests/rm/disp/rmt_dpflush.cpp
cpp_files$(ONLY_PC) += gpu/tests/rm/disp/rmt_dp12lanebwcounttest.cpp
cpp_files$(ONLY_PC) += gpu/tests/rm/disp/rmt_dp12allconnectortest.cpp
cpp_files$(ONLY_PC) += gpu/tests/rm/disp/rmt_dp12hotplugtest.cpp
cpp_files$(ONLY_PC) += gpu/tests/rm/disp/rmt_dp2sst.cpp
cpp_files$(ONLY_PC) += gpu/tests/rm/disp/rmt_dp2mst.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/msenc/rmt_cl90b7.cpp
cpp_files$(ONLY_PC) += gpu/tests/rm/disp/rmt_setMsaProperties.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/pmgr/rmt_gpiosanity.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/utility/rmtstream.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/utility/rmtstring.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/utility/clk/rmhalmodule.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/utility/clk/clkbase.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/utility/clk/clk_gf100.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/utility/clk/clk_gf117.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/utility/clk/clk_gk104.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/utility/clk/clk_gk20a.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/dispstatecheck/dispstate.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/rmt_midframe.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/rmt_pstatewithmodeset.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/rmt_sorsplit.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/rmt_vpllspreadtest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/uct/uctchip.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/uct/uctdomain.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/uct/uctexception.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/uct/uctfield.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/uct/uctfilereader.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/uct/uctoperator.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/uct/uctpstate.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/uct/uctresult.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/uct/uctstatement.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/uct/ucttrialspec.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/uct/ucttestreference.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/uct/uctpartialpstate.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/uct/uctvflwrve.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/uct/uctsinespec.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/clk/uct/ucttestspec.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/lwdec/rmt_lwdec.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/lwdec/rmt_pexaspm.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/lwdec/rmt_pexaslm.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/lwdec/rmt_pexlinkspeed.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/lwdec/rmt_lwdecbinary.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/lwdec/rmt_ut_lwdec.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/lwjpg/rmt_lwjpg.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/ofa/rmt_ofa.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/lwlink/rmt_lwlinktest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/lwlink/rmt_lwlinklowpowertest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/deferred_api/rmt_cl5080.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/ctrl/rmt_flcndmem.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/vpr/rmt_vprhwscrubberbasictest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/vpr/rmt_vprfault.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/LwDisplay/rmt_channelAllocationTest.cpp
cpp_files             += gpu/tests/rm/disp/LwDisplay/rmt_lwdispbringuppass1.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/LwDisplay/rmt_LwDispBasicWindowSanity.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/LwDisplay/rmt_LwDispWindowTest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/LwDisplay/rmt_cyclelwdisp.cpp
cpp_files             += gpu/tests/rm/disp/LwDisplay/rmt_cyclelwdisp2.cpp
cpp_files$            += gpu/tests/rm/disp/LwDisplay/rmt_LwVRRDispTest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/LwDisplay/rmt_LwDisplayLowPower.cpp
cpp_files             += gpu/tests/rm/disp/LwDisplay/modesetlib.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/LwDisplay/parsejsonconfig.cpp
cpp_files$            += gpu/tests/rm/disp/LwDisplay/rmt_LwDpTest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/LwDisplay/rmt_lwdispjsoncfg.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/LwDisplay/rmt_lwdispchannelexceptiontest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/LwDisplay/rmt_lwhdmiuserspacetest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/disp/LwDisplay/rmt_VbiosRmTransitionTest.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/ctrl/rmt_ctrlc365.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/vbios/rmt_getimagehash.cpp
#cpp_files += gpu/tests/rm/fb/rmt_vmmutester.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/volt/rmt_volt.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/gr/rmt_grmgrctrl.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/gr/rmt_eventbufferfecs.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/pmgr/rmt_pmgr.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/spdm/responder/rmt_spdmresponder.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/spdm/responder/rmt_spdmresponder_algorithms.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/spdm/responder/rmt_spdmresponder_capabilities.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/spdm/responder/rmt_spdmresponder_certificate.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/spdm/responder/rmt_spdmresponder_digests.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/spdm/responder/rmt_spdmresponder_keyexchangefinish.cpp
cpp_files$(ONLY_DGPU) += gpu/tests/rm/spdm/responder/rmt_spdmresponder_version.cpp

ifeq "$(INCLUDE_MDIAG)" "true"
    cpp_files$(ONLY_DGPU) += gpu/tests/rm/perf/rmt_hostperfmon.cpp
endif
ifeq "$(INCLUDE_LWDA)" "true"
    cpp_files$(ONLY_DGPU) += gpu/tests/rm/compute/rmt_ctrlctest.cpp
endif
ifeq "$(INCLUDE_UNITTEST)" "true"
    cpp_files$(ONLY_DGPU) += gpu/tests/rm/utility/unittests/unit_process.cpp
endif

ifeq "$(BUILD_OS)" "win32"
    cpp_files$(ONLY_DGPU) += gpu/tests/rm/sli/rmt_sli.cpp
    cpp_files$(ONLY_DGPU) += gpu/tests/rm/perf/rmt_perfmoncheck.cpp
endif

ifeq "$(INCLUDE_LWLINK)" "true"
    cpp_files$(ONLY_DGPU) += gpu/tests/rm/lwlink/rmt_ibmnputest.cpp
endif

CLIENTPATHS += //sw/mods/rmtest/edids/...
CLIENTPATHS += //sw/mods/rmtest/DTI/...


ifeq "$(BUILD_OS)" "win32"
    ifeq "$(BUILD_ARCH)" "x86"
        rmtest_release_files += $(BUILD_TOOLS_DIR)/sdk/DirectX_Mar2009/Utilities/bin/x86/D3DX9_41.dll
    else
        rmtest_release_files += $(BUILD_TOOLS_DIR)/sdk/DirectX_Mar2009/Utilities/bin/x64/d3dx9_41.dll
    endif
    # balls.exe app needed for PerfMonCheckTest
    rmtest_release_files += gpu/tests/rm/apps/win/balls.exe

    # TODO: are these perl binaries still needed?
    rmtest_release_files += gpu/tests/rm/tools/perl/perl.exe
    rmtest_release_files += gpu/tests/rm/tools/perl/perl510.dll
    rmtest_release_directories += gpu/tests/rm/tools/perl/lib
endif

ifneq "$(BUILD_OS)" "cheetah"
    ifeq "$(base_os)" "linux"
        # Scripts needed for policy manager based ELPG tests.
        rmtest_release_files += gpu/tests/rm/policymanager/elpg_deterministic.sh
    endif

    # fbimp directory needed for ImpTest
    rmtest_release_directories += gpu/tests/rm/fbimp
endif

# command files that contain commands to run rmtests in dvs
rmtest_release_files += gpu/tests/rm/tools/modsrm.cmd
rmtest_release_files += gpu/tests/rm/tools/modssim_rm.cmd
rmtest_release_files += gpu/tests/rm/tools/modssim_rm_xh.cmd

# directories needed for DTI tests
rmtest_release_directories += gpu/tests/rm/disp/dispinfradata
rmtest_release_directories += $(MODS_RMTEST_DIR)/DTI
rmtest_release_directories += $(MODS_RMTEST_DIR)/edids

ifneq "$(BUILD_OS)" "cheetah"
    # directory needed for SM2 tests
    rmtest_release_directories += $(DRIVERS_DIR)/resman/sm2tests

    # directory needed for UniversalClockTest
    rmtest_release_directories += gpu/tests/rm/clk/ctp

    ifeq "$(INCLUDE_GPU_SPECIFIC)" "true"
        # data files needed for ProdTest
        rmtest_release_files += gpu/tests/rm/generated/r_gf100.dat
        rmtest_release_files += gpu/tests/rm/generated/r_gf100b.dat
        rmtest_release_files += gpu/tests/rm/generated/r_gf104.dat
        rmtest_release_files += gpu/tests/rm/generated/r_gf104b.dat
        rmtest_release_files += gpu/tests/rm/generated/r_gf106.dat
        rmtest_release_files += gpu/tests/rm/generated/r_gf106b.dat
        rmtest_release_files += gpu/tests/rm/generated/r_gf108.dat
        rmtest_release_files += gpu/tests/rm/generated/r_gf117.dat
        rmtest_release_files += gpu/tests/rm/generated/r_gf119.dat
        rmtest_release_files += gpu/tests/rm/generated/r_gk104.dat
        rmtest_release_files += gpu/tests/rm/generated/r_gk107.dat
        rmtest_release_files += gpu/tests/rm/generated/r_gk110.dat
        rmtest_release_files += gpu/tests/rm/generated/r_gk110b.dat
        rmtest_release_files += gpu/tests/rm/generated/r_gk110c.dat
        rmtest_release_files += gpu/tests/rm/generated/r_gk208.dat
        rmtest_release_files += gpu/tests/rm/generated/r_t124.dat
        rmtest_release_files += gpu/tests/rm/generated/r_t132.dat
        rmtest_release_files += gpu/tests/rm/generated/r_t210.dat
        rmtest_release_files += gpu/tests/rm/generated/r_t186.dat
        rmtest_release_files += gpu/tests/rm/generated/r_gm107.dat
        rmtest_release_files += gpu/tests/rm/generated/r_gm108.dat
        rmtest_release_files += gpu/tests/rm/generated/r_gm200.dat
        rmtest_release_files += gpu/tests/rm/generated/r_gm204.dat
        rmtest_release_files += gpu/tests/rm/generated/r_gm206.dat
        rmtest_release_files += gpu/tests/rm/generated/r_gp104.dat
        rmtest_release_files += gpu/tests/rm/generated/r_tu102.dat
        rmtest_release_files += gpu/tests/rm/generated/r_tu104.dat
        rmtest_release_files += gpu/tests/rm/generated/r_tu106.dat
        rmtest_release_files += gpu/tests/rm/generated/r_tu116.dat
        rmtest_release_files += gpu/tests/rm/generated/r_tu117.dat
        rmtest_release_files += gpu/tests/rm/generated/r_ga100.dat
        rmtest_release_files += gpu/tests/rm/generated/r_ga102.dat
        rmtest_release_files += gpu/tests/rm/generated/r_ga103.dat
        rmtest_release_files += gpu/tests/rm/generated/r_ga104.dat
        rmtest_release_files += gpu/tests/rm/generated/r_ga106.dat
        rmtest_release_files += gpu/tests/rm/generated/r_ga107.dat
        rmtest_release_files += gpu/tests/rm/generated/r_ga10b.dat
        rmtest_release_files += gpu/tests/rm/generated/r_gh100.dat
        rmtest_release_files += gpu/tests/rm/generated/r_gh202.dat

        # Chip specific args file needed for SM2 fmodel sanities - DVS & fmodel update
        rmtest_release_files += gpu/js/rmsimsanitytu10xfmodel.args
        rmtest_release_files += gpu/js/rmsimsanityga100fmodel.args
        rmtest_release_files += gpu/js/rmsimsanityga102fmodel.args
        rmtest_release_files += gpu/js/rmsimsanitygh100fmodel_fs.args
        rmtest_release_files += gpu/js/dwa_multiple_windows_multiple_heads.args
        rmtest_release_files += gpu/js/dwa_multiple_windows_single_head.args

        # filter files needed for ProdTest
        rmtest_release_files += gpu/tests/traces/filter_regs_end_r_gf119.txt
        rmtest_release_files += gpu/tests/traces/filter_regs_mid_r_gf119.txt
        rmtest_release_files += gpu/tests/traces/skip_regs_r_gf119.txt
        rmtest_release_files += gpu/tests/traces/filter_regs_end_r_gf117.txt
        rmtest_release_files += gpu/tests/traces/filter_regs_mid_r_gf117.txt
        rmtest_release_files += gpu/tests/traces/skip_regs_r_gf117.txt
        rmtest_release_files += gpu/tests/traces/filter_regs_end_r_gk107.txt
        rmtest_release_files += gpu/tests/traces/filter_regs_mid_r_gk107.txt
        rmtest_release_files += gpu/tests/traces/skip_regs_r_gk107.txt
        rmtest_release_files += gpu/tests/traces/filter_regs_end_r_gk104.txt
        rmtest_release_files += gpu/tests/traces/filter_regs_mid_r_gk104.txt
        rmtest_release_files += gpu/tests/traces/skip_regs_r_gk104.txt
        rmtest_release_files += gpu/tests/traces/filter_regs_end_r_gk110.txt
        rmtest_release_files += gpu/tests/traces/filter_regs_mid_r_gk110.txt
        rmtest_release_files += gpu/tests/traces/skip_regs_r_gk110.txt
        rmtest_release_files += gpu/tests/traces/filter_regs_end_r_gk110b.txt
        rmtest_release_files += gpu/tests/traces/filter_regs_mid_r_gk110b.txt
        rmtest_release_files += gpu/tests/traces/skip_regs_r_gk110b.txt
        rmtest_release_files += gpu/tests/traces/filter_regs_end_r_gk110c.txt
        rmtest_release_files += gpu/tests/traces/filter_regs_mid_r_gk110c.txt
        rmtest_release_files += gpu/tests/traces/skip_regs_r_gk110c.txt
        rmtest_release_files += gpu/tests/traces/filter_regs_end_r_gk208.txt
        rmtest_release_files += gpu/tests/traces/filter_regs_mid_r_gk208.txt
        rmtest_release_files += gpu/tests/traces/skip_regs_r_gk208.txt
        rmtest_release_files += gpu/tests/traces/filter_regs_mid_r_gm107.txt
        rmtest_release_files += gpu/tests/traces/skip_regs_r_gm107.txt
        rmtest_release_files += gpu/tests/traces/filter_regs_mid_r_gm108.txt
        rmtest_release_files += gpu/tests/traces/skip_regs_r_gm108.txt
        rmtest_release_files += gpu/tests/traces/filter_regs_mid_r_gm200.txt
        rmtest_release_files += gpu/tests/traces/skip_regs_r_gm200.txt
        rmtest_release_files += gpu/tests/traces/filter_regs_mid_r_gm204.txt
        rmtest_release_files += gpu/tests/traces/skip_regs_r_gm204.txt
        rmtest_release_files += gpu/tests/traces/filter_regs_mid_r_gm206.txt
        rmtest_release_files += gpu/tests/traces/skip_regs_r_gm206.txt
        rmtest_release_files += gpu/tests/traces/filter_regs_end_r_gp100.txt
        rmtest_release_files += gpu/tests/traces/filter_regs_mid_r_gp100.txt
        rmtest_release_files += gpu/tests/traces/skip_regs_r_gp100.txt
        rmtest_release_files += gpu/tests/traces/filter_regs_end_r_gp104.txt
        rmtest_release_files += gpu/tests/traces/filter_regs_mid_r_gp104.txt
        rmtest_release_files += gpu/tests/traces/skip_regs_r_gp104.txt
    endif
endif

# rmtest statistics report
rmtest_release_files += gpu/tests/rm/tools/rmtest_stat.pl
exelwtable_files     += gpu/tests/rm/tools/rmtest_stat.pl
rmtest_release_files += gpu/tests/rm/tools/rmtest_run.pl
exelwtable_files     += gpu/tests/rm/tools/rmtest_run.pl

internal_release_files += gpu/js/rmtest.js
internal_release_files += gpu/js/rmtestDisableTest.js
internal_release_files += gpu/js/rmtSampleDTITest.js

ifneq "$(BUILD_OS)" "cheetah"
    internal_release_files += gpu/js/rmt_mdst.js
    internal_release_files += gpu/js/rmtclktest.js
    internal_release_files += gpu/js/rmtoverrideimp.js
    internal_release_files += gpu/js/sampledticonfig.js
    internal_release_files += $(SDK_DIR)/lwpu/inc/ctrl/ctrl5070/ctrl5070impoverrides.h
    internal_release_files += gpu/js/emp.js
    internal_release_files += gpu/js/pmgr.js
endif
