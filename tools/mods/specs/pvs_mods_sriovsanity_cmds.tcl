proc get_pvs_mods_sriovsanity_cmds { machinename } {

    if {$machinename == "ausvrl8718"} {
        return [list \
            "./mods -mle_v -b bypass.INTERNAL.bin -b bypass.bin gpugen.js -time -pvs_mode PC_Modstest -pvs_machine ausvrl8718 -pvs_sanity MODS -run_on_error @bringup.arg -readspec vsmc_vf.spc" \
            "./smc_sriov -mle_v -b bypass.INTERNAL.bin -b bypass.bin gputest.js -time -run_on_error @bringup.arg -disable_pex_link_sanity_check -skip_pertest_pexcheck -skip_pertest_pex_speed_check -shuffle_vf_tests -loops 50 -start_loop 49 -timeout_ms 10000 -ignore_hwfs_event THERMAL_2 -ignore_hwfs_event THERMAL_3 -pf \"-fundamental_reset -smc_partitions part:half:1-0-S2:part:quarter:0-2:part:eighth:1 -disable_ecc -readspec vsmc_pf.spc -power_cap_tgp_mw 150000  -bg_int_temp 100 -bg_power 500\" -vf \"-readspec vsmc_vf.spc\" -no_rcwd"
        ]
    }

    return 1
}
