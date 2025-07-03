# Configuration file for DPFGA per CL sanity in DVS

proc dfpga_get_num_cmds {} {
    return 7
}

proc dfpga_get_cmd { idx } {
    switch $idx {
        0 { return "-chip hwchip2.so -gpubios 0 tu102f_fpga_sddr3_nopll_no_ifr.rom gpugen.js -readspec tu10x_dfpga.spc -spec dvsSpecTests -suppress_gpio_intr_err_log 0x1 -force_fb -regwr_post_vbios +begin 0x00616668 0xffffffff 0x00616E68 0xffffffff 0x00617668 0xffffffff 0x00617E68 0xffffffff +end" }
        1 { return "-chip hwchip2.so -gpubios 0 tu102f_fpga_sddr3_nopll_dp-mst_no_ifr.rom gpugen.js -disable_composition_mode -disable_mods_console -testarg 304 SkipHeadMask 0xc -testargstr 304 SkipSetModes \"1,17-19\" -readspec tu10x_dfpga.spc -spec dvsSpecTests -suppress_gpio_intr_err_log 0x1 -force_fb -regwr_post_vbios +begin 0x00616668 0xffffffff 0x00616E68 0xffffffff 0x00617668 0xffffffff 0x00617E68 0xffffffff +end" }
        2 { return "-chip hwchip2.so -gpubios 0 tu102f_fpga_sddr3_nopll_no_ifr.rom gputest.js -readspec tu10x_dfpga.spc -spec dvsSpecTests -suppress_gpio_intr_err_log 0x1 -force_fb -regwr_post_vbios +begin 0x00616668 0xffffffff 0x00616E68 0xffffffff 0x00617668 0xffffffff 0x00617E68 0xffffffff +end" }
        3 { return "-chip hwchip2.so -gpubios 0 tu102f_fpga_sddr3_nopll_dp-mst_no_ifr.rom gputest.js -test 304 -disable_composition_mode -disable_mods_console -testarg 304 SkipHeadMask 0xc -testargstr 304 SkipSetModes \"1,17-19\" -readspec tu10x_dfpga.spc -spec dvsSpecTests -suppress_gpio_intr_err_log 0x1 -force_fb -regwr_post_vbios +begin 0x00616668 0xffffffff 0x00616E68 0xffffffff 0x00617668 0xffffffff 0x00617E68 0xffffffff +end" }
        4 { return "-chip hwchip2.so -gpubios 0 tu102f_fpga_sddr3_nopll_no_ifr.rom rmtest.js -base -forcetestname CycleLwDisp2 -rmkey RMBandwidthFeature 0x40400 -protocol \"DP_A,DP_B\" -headwin \"0-0-800-600\" -heads \"0\" -sors \"0\" -resolution \"800x600x32x60\" -devid_check_ignore -force_fb -pmu_instloc_inst vid -pmu_bootstrap_mode 1 -disable_acr -disable_dpu_sc_dma -lsfm_disable_mask 0xffffffff -outputfilename disp_crc -exelwte_rm_devinit_on_pmu 0 -disp_ignore_imp -timeout_ms 5000 -gc6_level_override 2 -onlyConnectedDisplays -only_family turing -regwr_post_vbios +begin 0x00616668 0xffffffff +end" }
        5 { return "-chip hwchip2.so -gpubios 0 tu102f_fpga_sddr3_nopll_no_ifr.rom rmtest.js -base -forcetestname CycleLwDisp2 -rmkey RMBandwidthFeature 0x40400 -protocol \"DP_A,DP_B\" -headwin \"0-0-800-600\" -heads \"0\" -sors \"0\" -resolution \"800x600x32x60\" -DpMode \"SST\" -enable_mode \"SDR\" -inputLutLwrve \"3\" -outputLutLwrve \"4\" -devid_check_ignore -force_fb -pmu_instloc_inst vid -pmu_bootstrap_mode 1 -disable_acr -disable_dpu_sc_dma -lsfm_disable_mask 0xffffffff -outputfilename disp_crc -exelwte_rm_devinit_on_pmu 0 -disp_ignore_imp -timeout_ms 5000 -gc6_level_override 2 -onlyConnectedDisplays -only_family turing -regwr_post_vbios +begin 0x00616668 0xffffffff +end" }
        6 { return "-chip hwchip2.so -gpubios 0 tu102f_fpga_sddr3_nopll_no_ifr.rom rmtest.js -base -forcetestname CycleLwDisp2 -rmkey RMBandwidthFeature 0x40400 -protocol \"SINGLE_TMDS_A,SINGLE_TMDS_B,DUAL_TMDS\" -headwin \"2-0-800-600\" -heads \"2\" -sors \"2\" -resolution \"800x600x32x60\" -devid_check_ignore -force_fb -pmu_instloc_inst vid -pmu_bootstrap_mode 1 -disable_acr -disable_dpu_sc_dma -lsfm_disable_mask 0xffffffff -outputfilename dvi_crc -exelwte_rm_devinit_on_pmu 0 -disp_ignore_imp -timeout_ms 5000 -gc6_level_override 2 -onlyConnectedDisplays -only_family turing" }
    }
    return ""
}
