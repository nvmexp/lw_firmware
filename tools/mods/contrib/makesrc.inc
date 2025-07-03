#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

ifeq "$(INCLUDE_GPU)" "true"
    basic_release_files                  += $(CONTRIB_DIR_UNIX)/bb_perfvec.js
    basic_release_files                  += $(CONTRIB_DIR_UNIX)/hbmtemptest_inputs.js
    basic_release_files                  += $(CONTRIB_DIR_UNIX)/pexaspm_chipsetfilter.js
    basic_release_files                  += $(CONTRIB_DIR_UNIX)/pwrregcheck_cfg.json
    internal_release_files$(ONLY_PC)     += $(CONTRIB_DIR_UNIX)/fmaxfs.js
    internal_release_files$(ONLY_PC)     += $(CONTRIB_DIR_UNIX)/inputs.js
    internal_release_files$(ONLY_PC)     += $(CONTRIB_DIR_UNIX)/e812B00.js
    basic_release_files$(ONLY_PC)        += $(CONTRIB_DIR_UNIX)/tdpperfconfig.js
    basic_release_files$(ONLY_PC)        += $(CONTRIB_DIR_UNIX)/i2cbgstressconfig.js

    # Display testing files, owned by Gurucharan Nilajagi and Fangyu Meng
    internal_release_files$(ONLY_PC)    += $(CONTRIB_DIR_UNIX)/disp/dprun.js
    internal_release_files$(ONLY_PC)    += $(CONTRIB_DIR_UNIX)/disp/hcDW25x16.png
    internal_release_files$(ONLY_PC)    += $(CONTRIB_DIR_UNIX)/disp/hcsxga.png
    internal_release_files$(ONLY_PC)    += $(CONTRIB_DIR_UNIX)/disp/hlwxga.png
    internal_release_files$(ONLY_PC)    += $(CONTRIB_DIR_UNIX)/disp/hcvga.png
    internal_release_files$(ONLY_PC)    += $(CONTRIB_DIR_UNIX)/disp/hcxga.png
    internal_release_files$(ONLY_PC)    += $(CONTRIB_DIR_UNIX)/disp/rsxga.png
    internal_release_files$(ONLY_PC)    += $(CONTRIB_DIR_UNIX)/disp/ruxga.png
    internal_release_files$(ONLY_PC)    += $(CONTRIB_DIR_UNIX)/disp/rvga.png
    internal_release_files$(ONLY_PC)    += $(CONTRIB_DIR_UNIX)/disp/rxga.png
    internal_release_files$(ONLY_PC)    += $(CONTRIB_DIR_UNIX)/disp/tmdsrun.js

    # Panel firmware verification file, owned by Martin Hsu
    basic_release_files$(ONLY_PC) += $(CONTRIB_DIR_UNIX)/disp/PanelChecksums.txt

    basic_release_files$(ONLY_PC) += $(CONTRIB_DIR_UNIX)/tesla/tesla.spc
    basic_release_files$(ONLY_PC) += $(CONTRIB_DIR_UNIX)/tesla/tesla_fct.spc
    basic_release_files$(ONLY_PC) += $(CONTRIB_DIR_UNIX)/tesla/tesla_eft.spc
    basic_release_files$(ONLY_PC) += $(CONTRIB_DIR_UNIX)/tesla/tesla_oqa.spc
    basic_release_files$(ONLY_PC) += $(CONTRIB_DIR_UNIX)/tesla/tesla_dgx.spc
    basic_release_files$(ONLY_PC) += $(CONTRIB_DIR_UNIX)/tesla/tesla_boards.spc
    basic_release_files$(ONLY_PC) += $(CONTRIB_DIR_UNIX)/dgx/dgxmfg.spc
    basic_release_files$(ONLY_PC) += $(CONTRIB_DIR_UNIX)/dgx/dgxqual.spc
    basic_release_files$(ONLY_PC) += $(CONTRIB_DIR_UNIX)/dgx/dgxfielddiag.spc
    basic_release_files$(ONLY_PC) += $(CONTRIB_DIR_UNIX)/dgx/dgxcommon.spc
    basic_release_files$(ONLY_PC) += $(CONTRIB_DIR_UNIX)/dgx/dgxmfgexp.spc
    basic_release_files$(ONLY_PC) += $(CONTRIB_DIR_UNIX)/dgx/dgxmfggora.spc
    basic_release_files$(ONLY_PC) += $(CONTRIB_DIR_UNIX)/dgx/dgxmfgzabawa_hgx2tester.spc
    basic_release_files$(ONLY_PC) += $(CONTRIB_DIR_UNIX)/dgx/dgxqualexp.spc
    basic_release_files$(ONLY_PC) += $(CONTRIB_DIR_UNIX)/dgx/dgxmfgbenthos.spc
    basic_release_files$(ONLY_PC) += $(CONTRIB_DIR_UNIX)/specs/lwswitch.spc
    basic_release_files$(ONLY_PC) += $(CONTRIB_DIR_UNIX)/cgx/akukumfg.spc
    basic_release_files$(ONLY_PC) += $(CONTRIB_DIR_UNIX)/specs/nbmfg.spc

    pvs_release_files += $(CONTRIB_DIR_UNIX)/pvs/failing_lanes_test.js
    pvs_release_files += $(CONTRIB_DIR_UNIX)/pvs/failing_rows_test.js

    ifeq "$(BUILD_OS)" "sim"
        internal_release_files += $(CONTRIB_DIR_UNIX)/regress_sriov_tu102
        exelwtable_files       += $(CONTRIB_DIR_UNIX)/regress_sriov_tu102
        internal_release_files += $(CONTRIB_DIR_UNIX)/regress_sriov_ga100
        exelwtable_files       += $(CONTRIB_DIR_UNIX)/regress_sriov_ga100
        internal_release_files += $(CONTRIB_DIR_UNIX)/regress_sriov_gh100
        exelwtable_files       += $(CONTRIB_DIR_UNIX)/regress_sriov_gh100
    else
        internal_release_files += $(CONTRIB_DIR_UNIX)/specs/sriov.spc
        internal_release_files += $(CONTRIB_DIR_UNIX)/regress_sriov_hw
        exelwtable_files       += $(CONTRIB_DIR_UNIX)/regress_sriov_hw
    endif

    ifeq "$(STEALTH_MODE)" "629"
        basic_release_files += $(CONTRIB_DIR_UNIX)/multifieldiag.sh
        basic_release_files += $(CONTRIB_DIR_UNIX)/tesla/pg506sku200_lwspec.json
        basic_release_files += $(CONTRIB_DIR_UNIX)/tesla/pg506sku202_lwspec.json
        basic_release_files += $(CONTRIB_DIR_UNIX)/tesla/pg509sku200_lwspec.json
        basic_release_files += $(CONTRIB_DIR_UNIX)/tesla/p1001sku200_lwspec.json
        exelwtable_files    += $(CONTRIB_DIR_UNIX)/multifieldiag.sh
    endif


    internal_release_files$(ONLY_PC)     += $(CONTRIB_DIR_UNIX)/nmeas/nmeastest.js
    unencrypted_internal_files$(ONLY_PC) += $(CONTRIB_DIR_UNIX)/nmeas/nmeasconfig.js
    internal_release_files$(ONLY_PC)     += $(CONTRIB_DIR_UNIX)/nmeas/nmeas_lite_v3.js
    internal_release_files$(ONLY_PC)     += $(CONTRIB_DIR_UNIX)/nmeas/ga100/ga100_nmeas.js
    internal_release_files$(ONLY_PC)     += $(CONTRIB_DIR_UNIX)/nmeas/ga100/ga100_nmeas.spc
    internal_release_files$(ONLY_PC)     += $(CONTRIB_DIR_UNIX)/nmeas/ga100/ga100_nmeas.arg

    # File containing the mapping between gpu and its video trace version.
    unencrypted_internal_files += $(CONTRIB_DIR_UNIX)/video_traces_config/gpu_to_video_traces_version_mapping.json
endif
ifeq "$(INCLUDE_MDIAG)" "true"
#The following scripts are added for ISM MODS API verification on RTL
    internal_release_files += $(CONTRIB_DIR_UNIX)/ism/ism_api_verf.js
    internal_release_files += $(CONTRIB_DIR_UNIX)/ism/read_ism_speedos.js
    internal_release_files += $(CONTRIB_DIR_UNIX)/ism/read_ism_speedos_header.js
endif

ifeq "$(base_os)" "win32"
    # On Windows, o_dir is already relative
    CONTRIB_OUTPUT_DIR = $(o_dir)
else
    CONTRIB_OUTPUT_DIR = $(shell tools/abs2rel.sh $(o_dir))
endif

ifeq "$(INCLUDE_LWSWITCH)" "true"
    # filter the files below from lwlink_topology_json_files that don't have .topo extenson.
    lwlink_topology_files             += $(o_dir)/dgx-2.topo.bin
    # dgx-2-optimal.json is an addition to dgx-2.topo.bin that specifies
    # the maximum traffic configuration for that topology file. In order to use
    # it, specify UseOptimal test argument to any LwLink bandwidth stress. The
    # test will discard .topo.* in the end of the topology file name, will add
    # -optimal.json to the result and will try to read this file to configure
    # the test for maximum traffic. The rule for naming this file is defined in
    # the SetupLwLinkDataTestCommon function inside gpudecls.js.
    lwlink_topology_files             += $(o_dir)/dgx-2-optimal.json
    lwlink_topology_files             += $(o_dir)/dgx-2_16.topo.bin
    lwlink_topology_files             += $(o_dir)/dgx-2_16-optimal.json
    lwlink_topology_files             += $(o_dir)/hgx2BaseBoardBottomTrayLB.topo.bin
    lwlink_topology_files             += $(o_dir)/hgx2BaseBoardTopTrayLB.topo.bin
    lwlink_topology_files             += $(o_dir)/hgx2BothBaseBoardLB.topo.bin
    lwlink_topology_files             += $(o_dir)/hgx2BothBaseBoardWOLB.topo.bin
    lwlink_topology_files             += $(o_dir)/hgx2BaseBoardBottomTrayLB-optimal.json
    lwlink_topology_files             += $(o_dir)/hgx2BaseBoardTopTrayLB-optimal.json
    lwlink_topology_files             += $(o_dir)/hgx2BaseBoardBottomTrayWOLB.topo.bin
    lwlink_topology_files             += $(o_dir)/hgx2BaseBoardTopTrayWOLB.topo.bin
    lwlink_topology_files             += $(o_dir)/hgx2BothBaseBoardConnected.topo.bin
    lwlink_topology_files             += $(o_dir)/hgx2BaseBoardBottomTrayWOLB-optimal.json
    lwlink_topology_files             += $(o_dir)/hgx2BaseBoardTopTrayWOLB-optimal.json
    lwlink_topology_files             += $(o_dir)/hgx2BothBaseBoardConnected-optimal.json
    lwlink_topology_files             += $(o_dir)/zabawaBaseBoardBottomTrayLB.topo.bin
    lwlink_topology_files             += $(o_dir)/zabawaBaseBoardTopTrayLB.topo.bin
    lwlink_topology_files             += $(o_dir)/zabawaBothBaseBoardConnected.topo.bin
    lwlink_topology_files             += $(o_dir)/zabawaBothBaseBoardWOLB.topo.bin
    lwlink_topology_files             += $(o_dir)/zabawaBaseBoardBottomTrayWOLB.topo.bin
    lwlink_topology_files             += $(o_dir)/zabawaBaseBoardTopTrayWOLB.topo.bin
    lwlink_topology_files             += $(o_dir)/zabawaBothBaseBoardLB.topo.bin
    lwlink_topology_files             += $(o_dir)/zabawaBaseBoardBottomTrayLoopout.topo.bin
    lwlink_topology_files             += $(o_dir)/zabawaBaseBoardTopTrayLoopout.topo.bin
    lwlink_topology_files             += $(o_dir)/zabawaBothBaseBoardLoopout.topo.bin
    lwlink_topology_files             += $(o_dir)/zabawa2BaseBoardBottomTrayE4593.topo.bin
    lwlink_topology_files             += $(o_dir)/zabawa2BaseBoardTopTrayE4593.topo.bin
    lwlink_topology_files             += $(o_dir)/zabawa2BothBaseBoardE4593.topo.bin
    lwlink_topology_files             += $(o_dir)/zabawaBaseBoardBottomTrayLB-optimal.json
    lwlink_topology_files             += $(o_dir)/zabawaBaseBoardBottomTrayWOLB-optimal.json
    lwlink_topology_files             += $(o_dir)/zabawaBaseBoardTopTrayLB-optimal.json
    lwlink_topology_files             += $(o_dir)/zabawaBaseBoardTopTrayWOLB-optimal.json
    lwlink_topology_files             += $(o_dir)/zabawaBothBaseBoardConnected-optimal.json
    lwlink_topology_files             += $(o_dir)/zabawaBothBaseBoardLB-optimal.json
    lwlink_topology_files             += $(o_dir)/zabawaBothBaseBoardWOLB-optimal.json
    lwlink_topology_files             += $(o_dir)/zabawaBaseBoardBottomTrayLoopout-optimal.json
    lwlink_topology_files             += $(o_dir)/zabawaBaseBoardTopTrayLoopout-optimal.json
    lwlink_topology_files             += $(o_dir)/zabawaBothBaseBoardLoopout-optimal.json
    lwlink_topology_files             += $(o_dir)/zabawa2BaseBoardBottomTrayE4593-optimal.json
    lwlink_topology_files             += $(o_dir)/zabawa2BaseBoardTopTrayE4593-optimal.json
    lwlink_topology_files             += $(o_dir)/zabawa2BothBaseBoardE4593-optimal.json

    $(o_dir)/%.topo.bin : $(CONTRIB_DIR_UNIX)/dgx/%.topo.bin
	    @$(ECHO) "Creating $@"
	    $(Q)$(CP_IF_CHANGED) $(CP_RM_DEST) $< $@

    $(o_dir)/%-optimal.json : $(CONTRIB_DIR_UNIX)/dgx/%.topo.bin submake.protoutilhost.build_all
	    @$(ECHO) "Creating $@"
	    $(Q)$(MODS_OUTPUT_DIR)/$(rel_o_dir_host)/protoutil_host/protoutil_host maxtraffic $< $@

    all_gen_files += $(o_dir)/dgx-2-optimal.json
    all_gen_files += $(o_dir)/dgx-2_16-optimal.json
    all_gen_files += $(o_dir)/hgx2BaseBoardBottomTrayLB-optimal.json
    all_gen_files += $(o_dir)/hgx2BaseBoardTopTrayLB-optimal.json
    all_gen_files += $(o_dir)/hgx2BaseBoardBottomTrayWOLB-optimal.json
    all_gen_files += $(o_dir)/hgx2BaseBoardTopTrayWOLB-optimal.json
    all_gen_files += $(o_dir)/hgx2BothBaseBoardConnected-optimal.json
    all_gen_files += $(o_dir)/zabawaBaseBoardBottomTrayLB-optimal.json
    all_gen_files += $(o_dir)/zabawaBaseBoardBottomTrayWOLB-optimal.json
    all_gen_files += $(o_dir)/zabawaBaseBoardTopTrayLB-optimal.json
    all_gen_files += $(o_dir)/zabawaBaseBoardTopTrayWOLB-optimal.json
    all_gen_files += $(o_dir)/zabawaBothBaseBoardConnected-optimal.json
    all_gen_files += $(o_dir)/zabawaBothBaseBoardLB-optimal.json
    all_gen_files += $(o_dir)/zabawaBothBaseBoardWOLB-optimal.json
    all_gen_files += $(o_dir)/zabawaBaseBoardBottomTrayLoopout-optimal.json
    all_gen_files += $(o_dir)/zabawaBothBaseBoardLoopout-optimal.json
    all_gen_files += $(o_dir)/zabawaBaseBoardTopTrayLoopout-optimal.json
    all_gen_files += $(o_dir)/zabawa2BaseBoardBottomTrayE4593-optimal.json
    all_gen_files += $(o_dir)/zabawa2BaseBoardTopTrayE4593-optimal.json
    all_gen_files += $(o_dir)/zabawa2BothBaseBoardE4593-optimal.json

    $(o_dir)/%.json : $(CONTRIB_DIR_UNIX)/protobuf/%.json
endif
