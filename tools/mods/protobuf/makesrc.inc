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
ifeq "$(INCLUDE_LWSWITCH)" "true"
    # filter the files below from lwlink_topology_json_files that don't have .topo extenson.
    lwlink_topology_files             += $(o_dir)/quad.topo
    lwlink_topology_files             += $(o_dir)/dual_quad.topo
    lwlink_topology_files             += $(o_dir)/dgx-2.topo.bin
    # dgx-2-optimal.json is an addition to dgx-2.topo.bin that specifies
    # the maximum traffic configuration for that topology file. In order to use
    # it, specify UseOptimal test argument to any LwLink bandwidth stress. The
    # test will discard .topo.* in the end of the topology file name, will add
    # -optimal.json to the result and will try to read this file to configure
    # the test for maximum traffic. The rule for naming this file is defined in
    # the SetupLwLinkDataTestCommon function inside gpudecls.js.
    internal_lwlink_topology_files    += $(o_dir)/fsf_loopback_1gpu.topo
    internal_lwlink_topology_files    += $(o_dir)/emu_loopback_one_trunk_gang.topo
    internal_lwlink_topology_files    += $(o_dir)/emu_loopback_one_trunk.topo
    internal_lwlink_topology_files    += $(o_dir)/emu_loopback.topo
    internal_lwlink_topology_files    += $(o_dir)/emu_loopback_two_trunk.topo
    internal_lwlink_topology_files    += $(o_dir)/emu_trivial.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm0_many_to_one_10.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm0_many_to_one_11.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm0_many_to_one_12.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm0_many_to_one_13.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm0_many_to_one_14.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm0_many_to_one_15.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm0_many_to_one_16.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm0_many_to_one_17.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm0_many_to_one_6.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm0_many_to_one_7.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm0_many_to_one_8.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm0_many_to_one_9.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm0_eight_gang_0.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm0_eight_gang_1.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm0_eight_gang_2.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm0_eight_gang_3.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm0_eight_gang_4.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm0_eight_gang_5.topo
    internal_lwlink_topology_files    += $(o_dir)/switchonly.topo
    internal_lwlink_topology_files    += $(o_dir)/lrsimple.topo
    internal_lwlink_topology_files    += $(o_dir)/lremu_5ports.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm2gpu.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm2gpu_allports.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm0loopback.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm1loopback.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm0snake.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm1snake.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm2gpu_folws.topo
    internal_lwlink_topology_files    += $(o_dir)/simple16_17.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm0snake_nea_ports_6_7_14_15.topo
    internal_lwlink_topology_files    += $(o_dir)/shtol.topo
    internal_lwlink_topology_files    += $(o_dir)/shtol_loopback_e3600_0.topo
    internal_lwlink_topology_files    += $(o_dir)/shtol_snake_nea_e3600_0.topo
    internal_lwlink_topology_files    += $(o_dir)/sxm0loopback_fullfabric.topo
    lwlink_topology_files             += $(o_dir)/lrsxm0loopback.topo
    lwlink_topology_files             += $(o_dir)/lrsxm0snake.topo
    internal_lwlink_topology_files    += $(o_dir)/lrsxm0stacked.topo
    internal_lwlink_topology_files    += $(o_dir)/lrsxm0snake_many.topo
    lwlink_topology_files             += $(o_dir)/lrsxm1loopback.topo
    lwlink_topology_files             += $(o_dir)/lrsxm1snake.topo
    internal_lwlink_topology_files    += $(o_dir)/lrsxm1stacked.topo
    internal_lwlink_topology_files    += $(o_dir)/lrsxm1snake_many.topo
    lwlink_topology_files             += $(o_dir)/lrsxm2gpu.topo
    lwlink_topology_files             += $(o_dir)/lrsxm2gpu_allports.topo
    lwlink_topology_files             += $(o_dir)/lrswitchonly.topo
    internal_lwlink_topology_files    += $(o_dir)/lrswitchonly_sxm0.topo
    internal_lwlink_topology_files    += $(o_dir)/lrswitchonly_sxm1.topo
    internal_lwlink_topology_files    += $(o_dir)/lrswitchonly_examax.topo
    lwlink_topology_files             += $(o_dir)/lrtrexloopback.topo
    ifeq "$(LWCFG_GLOBAL_LWSWITCH_IMPL_LS10)" "1"
        internal_lwlink_topology_files += $(o_dir)/lstrexsimple.topo
        internal_lwlink_topology_files += $(o_dir)/lstrexmulticastsimple.topo
        internal_lwlink_topology_files += $(o_dir)/lssimple.topo
        internal_lwlink_topology_files += $(o_dir)/lsemuswitchonly.topo
        internal_lwlink_topology_files += $(o_dir)/lsemusimple.topo
        internal_lwlink_topology_files += $(o_dir)/lsemutrexsimple.topo
        internal_lwlink_topology_files += $(o_dir)/lsemutrexmulticastsimple.topo

        lwlink_topology_files          += $(o_dir)/lsswitchonly_lbosfp.topo
        lwlink_topology_files          += $(o_dir)/lsswitchonly_lbosfp_F2.topo
        lwlink_topology_files          += $(o_dir)/lsswitchonly_loosfp.topo
        internal_lwlink_topology_files += $(o_dir)/lsswitchonly_noosfp.topo
        lwlink_topology_files          += $(o_dir)/lssxm0loopback.topo
        internal_lwlink_topology_files += $(o_dir)/lssxm0snake_noosfp.topo
        internal_lwlink_topology_files += $(o_dir)/lssxm0minisnake.topo
        lwlink_topology_files          += $(o_dir)/lssxm1loopback.topo
        internal_lwlink_topology_files += $(o_dir)/lssxm1snake_noosfp.topo
        internal_lwlink_topology_files += $(o_dir)/lssxm2gpu_noosfp.topo
        lwlink_topology_files          += $(o_dir)/lstrexloopback.topo
        lwlink_topology_files          += $(o_dir)/lstrexsnake.topo
        lwlink_topology_files          += $(o_dir)/lstrexsnake_F2.topo
        lwlink_topology_files          += $(o_dir)/lstrexmulticast64.topo
        lwlink_topology_files          += $(o_dir)/lstrexmulticast44_F2.topo
        internal_lwlink_topology_files += $(o_dir)/lstrexmulticast32.topo
        internal_lwlink_topology_files += $(o_dir)/lstrexmulticast16.topo
        internal_lwlink_topology_files += $(o_dir)/lstrexmulticast8.topo
        internal_lwlink_topology_files += $(o_dir)/lstrexmulticast4.topo
    endif
    basic_release_files                += $(o_dir)/lwlinktopofiles.bin
    internal_release_files             += $(o_dir)/lwlinktopofiles.INTERNAL.bin

    ifneq (,$(filter $(BUILD_OS), winsim sim))
        internal_release_files$(ONLY_PC) += protobuf/lrsimple.ini
        internal_release_files$(ONLY_PC) += protobuf/lremu_5ports.ini
        ifeq "$(LWCFG_GLOBAL_LWSWITCH_IMPL_LS10)" "1"
            internal_release_files$(ONLY_PC) += protobuf/lstrexsimple.ini
            internal_release_files$(ONLY_PC) += protobuf/lssimple.ini
        endif
    endif

    install : $(o_dir)/lwlinktopofiles.bin $(o_dir)/lwlinktopofiles.INTERNAL.bin
    zip : $(o_dir)/lwlinktopofiles.bin $(o_dir)/lwlinktopofiles.INTERNAL.bin
    zip_pvs : $(o_dir)/lwlinktopofiles.bin $(o_dir)/lwlinktopofiles.INTERNAL.bin

    $(o_dir)/switchonly.topo : protobuf/switchonly.topo
	    $(Q)$(CP_IF_CHANGED) $(CP_RM_DEST) $< $@

    $(o_dir)/%.topo : protobuf/%.json submake.protoutilhost.build_all
	    @$(ECHO) "Creating $@"
	    $(Q)$(MODS_OUTPUT_DIR)/$(rel_o_dir_host)/protoutil_host/protoutil_host create $< $@

    # There is what appears to be a very highly intermittent issue with creating the tar packages
    # which has lwrrently only been seen in the AS2 builds.  Very infrequently tar will report that
    # a file has changed while the tar was created.  All dependencies in the Makefile appear to be
    # correct.  There are likely one of 2 causes
    #
    #    1. A sync issue with file systems.  If the tar starts and the last modified time of the
    #       file changes between the time the tar starts and it finished it will get this error.
    #       It is possible file time updates on network mounted shares may lag behind file creation
    #       (so the contents of the file is consistent but the time update causes tar to fail)
    #    2. A bug in the LSF version of tar (1.23) that was fixed in tar version 1.26.
    #
    # Assuming that one of the above is true, move the file changed error to a warning and ignore
    # a return code of one from the tar (which indicates tar exited with a warning)
    tar_ignore_file_changed =                                                          \
        tar $(POSIX_TAR) --warning=no-file-changed -czf $@ -C $(o_dir) $(notdir $(1)); \
            exitcode=$$?;                                                              \
            if [ "$$exitcode" != "1" ] && [ "$$exitcode" != "0" ]; then                \
                exit $$exitcode;                                                       \
            else                                                                       \
                exit 0;                                                                \
            fi
    $(o_dir)/lwlinktopofiles.bin : $(lwlink_topology_files)
	    @$(ECHO) "Creating lwlinktopofiles.bin"
	    @$(call tar_ignore_file_changed, $(lwlink_topology_files))

    $(o_dir)/lwlinktopofiles.INTERNAL.bin : $(internal_lwlink_topology_files)
	    @$(ECHO) "Creating lwlinktopofiles.INTERNAL.bin"
	    @$(call tar_ignore_file_changed, $(internal_lwlink_topology_files))

    # Filter out *-optimal.json from the dependencies of
    # lwlinktopofiles.json.tgz, because it's not a JSON file for .topo files
    # generation. Also filter out *.topo.bin files, because they are not JSON
    # files either and they are not generated from JSON files.
    #
    # Pull the JSON files directly from the protobuf directory rather than
    # copying them to the o_dir first
    lwlink_topology_json_files = $(addprefix protobuf/,\
                                     $(notdir \
                                         $(filter-out $(o_dir)/switchonly.json\
                                                      $(o_dir)/%-optimal.json\
                                                      $(o_dir)/%.topo.bin\
                                                    , $(patsubst %.topo, %.json\
                                                               , $(lwlink_topology_files)\
                                                                 $(internal_lwlink_topology_files)\
                                                       )\
                                          )\
                                      )\
                                  )

    all_gen_files += $(o_dir)/lwlinktopofiles.json.tgz

    internal_release_files$(ONLY_DGPU)  +=  $(o_dir)/lwlinktopofiles.json.tgz
    $(o_dir)/lwlinktopofiles.json.tgz : $(lwlink_topology_json_files)
	    @$(ECHO) "Creating lwlinktopofiles.json.tgz"
	    @tar $(POSIX_TAR) -czf $@ -C protobuf $(notdir $(lwlink_topology_json_files))
endif
