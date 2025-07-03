proc get_pvs_mods_fmodel_cmds { queue } {
    add_log "get_pvs_mods_fmodel_cmds: Searching for: $queue"

    if {$queue == "gh100 notest"} {
        return [list \
            "./mods -chip ghlit1_fmodel_64.so -gpubios 0 gh100_sim_hbm.rom gputest.js -readspec gh100fmod.spc -spec specNoTest"
        ]
    }
    if {$queue == "gh100 instInSys"} {
        return [list \
            "./mods -chip ghlit1_fmodel_64.so -gpubios 0 gh100_sim_hbm.rom gputest.js -readspec gh100fmod.spc -spec specInstInSys"
        ]
    }
    if {$queue == "gh100 fbBroken"} {
        return [list \
            "./mods -chip ghlit1_fmodel_64.so -gpubios 0 gh100_sim_hbm.rom gputest.js -readspec gh100fmod.spc -spec specFbBroken"
        ]
    }
    if {$queue == "gh100 linpack"} {
        return [list \
            "./mods -chip ghlit1_fmodel_64.so -gpubios 0 gh100_sim_hbm.rom gputest.js -readspec gh100fmod.spc -spec specLinpack"
        ]
    }
    if {$queue == "gh100_ls10 lwlink"} {
        return [list \
            "./mods -chip ghlit1_fmodel_64.so -gpubios 0 gh100_sim_hbm.rom gputest.js -multichip_paths ghlit1_fmodel_64.so:lslit2_fmodel_64.so -readspec ls10fmod.spc -spec dvsSpecLwLinkBwStress"
        ]
    }
    if {$queue == "ls10 lwlinkTrex"} {
        return [list \
            "./mods -chip lslit2_fmodel_64.so gputest.js -readspec ls10fmod.spc -spec dvsSpecLwLinkTrex"
        ]
    }
    if {$queue == "ga100 notest"} {
        return [list \
            "./mods -chip galit1_fmodel_64.so -gpubios 0 ga100_sim_hbm.rom gputest.js -readspec ga100fmod.spc -spec dvsSpecNoTest"
        ]
    }
    if {$queue == "ga100 special"} {
        return [list \
            "./mods -chip galit1_fmodel_64.so -gpubios 0 ga100_sim_hbm.rom gputest.js -readspec ga100fmod.spc -spec dvsSpecSpecial"
        ]
    }
    if {$queue == "ga100 standard"} {
        return [list \
            "./mods -chip galit1_fmodel_64.so -gpubios 0 ga100_sim_hbm.rom gputest.js -readspec ga100fmod.spc -spec pvsSpecStandard"
        ]
    }
    if {$queue == "ga100 cbu_0"} {
        return [list \
            "./mods -chip galit1_fmodel_64.so -gpubios 0 ga100_sim_hbm.rom gputest.js -readspec ga100fmod.spc -spec pvsSpecCbu_0"
        ]
    }
    if {$queue == "ga100 cbu_1"} {
        return [list \
            "./mods -chip galit1_fmodel_64.so -gpubios 0 ga100_sim_hbm.rom gputest.js -readspec ga100fmod.spc -spec pvsSpecCbu_1"
        ]
    }
    if {$queue == "ga100 lwdaRandom"} {
        return [list \
            "./mods -chip galit1_fmodel_64.so -gpubios 0 ga100_sim_hbm.rom gputest.js -readspec ga100fmod.spc -spec pvsSpecLwdaRandom"
        ]
    }
    if {$queue == "ga100 lwdaMatsRandom"} {
        return [list \
            "./mods -chip galit1_fmodel_64.so -gpubios 0 ga100_sim_hbm.rom gputest.js -readspec ga100fmod.spc -spec pvsSpecLwdaMatsRandom"
        ]
    }
    if {$queue == "ga100 linpack"} {
        return [list \
            "./mods -chip galit1_fmodel_64.so -gpubios 0 ga100_sim_hbm.rom gputest.js -readspec ga100fmod.spc -spec pvsSpecLinpack -smc_partitions part:full:1"
        ]
    }
    if {$queue == "ga100 instInSys"} {
        return [list \
            "./mods -chip galit1_fmodel_64.so -gpubios 0 ga100_sim_hbm.rom gputest.js -readspec ga100fmod.spc -spec pvsSpecInstInSys"
        ]
    }
    if {$queue == "ga100 fbBroken"} {
        return [list \
            "./mods -chip galit1_fmodel_64.so -gpubios 0 ga100_sim_hbm.rom gputest.js -readspec ga100fmod.spc -spec pvsSpecFbBroken"
        ]
    }
    if {$queue == "ga100 glrShort"} {
        return [list \
            "./mods -chip galit1_fmodel_64.so -gpubios 0 ga100_sim_hbm.rom gputest.js -readspec ga100fmod.spc -spec dvsSpecGlrShort"
        ]
    }
    if {$queue == "ga100 glStress"} {
        return [list \
            "./mods -chip galit1_fmodel_64.so -gpubios 0 ga100_sim_hbm.rom gputest.js -readspec ga100fmod.spc -spec pvsSpecGlStress"
        ]
    }
    if {$queue == "ga100 glrA8R8G8B8_0"} {
        return [list \
            "./mods -chip galit1_fmodel_64.so -gpubios 0 ga100_sim_hbm.rom gputest.js -readspec ga100fmod.spc -spec pvsSpecGlrA8R8G8B8_0"
        ]
    }
    if {$queue == "ga100 glrR5G6B5_0"} {
        return [list \
            "./mods -chip galit1_fmodel_64.so -gpubios 0 ga100_sim_hbm.rom gputest.js -readspec ga100fmod.spc -spec pvsSpecGlrR5G6B5_0"
        ]
    }
    if {$queue == "ga100 glrFsaa2x_0"} {
        return [list \
            "./mods -chip galit1_fmodel_64.so -gpubios 0 ga100_sim_hbm.rom gputest.js -readspec ga100fmod.spc -spec pvsSpecGlrFsaa2x_0"
        ]
    }
    if {$queue == "ga100 glrFsaa4x_0"} {
        return [list \
            "./mods -chip galit1_fmodel_64.so -gpubios 0 ga100_sim_hbm.rom gputest.js -readspec ga100fmod.spc -spec pvsSpecGlrFsaa4x_0"
        ]
    }
    if {$queue == "ga100 glrMrtRgbU_0"} {
        return [list \
            "./mods -chip galit1_fmodel_64.so -gpubios 0 ga100_sim_hbm.rom gputest.js -readspec ga100fmod.spc -spec pvsSpecGlrMrtRgbU_0"
        ]
    }
    if {$queue == "ga100 glrMrtRgbF_0"} {
        return [list \
            "./mods -chip galit1_fmodel_64.so -gpubios 0 ga100_sim_hbm.rom gputest.js -readspec ga100fmod.spc -spec pvsSpecGlrMrtRgbF_0"
        ]
    }
    if {$queue == "ga100 mme64"} {
        return [list \
            "./mods -chip galit1_fmodel_64.so -gpubios 0 ga100_sim_hbm.rom gputest.js -readspec ga100fmod.spc -spec pvsSpecMme64"
        ]
    }
    if {$queue == "ga100 unitTests"} {
        return [list \
            "./mods -chip galit1_fmodel_64.so -gpubios 0 ga100_sim_hbm.rom modstest.js -readspec ga100fmod.spc -spec pvsSpelwnitTests"
        ]
    }
    if {$queue == "ga100 video"} {
        return [list \
            "./mods -chip galit1_fmodel_64.so -gpubios 0 ga100_sim_hbm.rom gputest.js -readspec ga100fmod.spc -spec pvsSpecVideo"
        ]
    }
    if {$queue == "ga100 vkStress"} {
        return [list \
            "./mods -chip galit1_fmodel_64.so -gpubios 0 ga100_sim_hbm.rom gputest.js -readspec ga100fmod.spc -spec pvsSpecVkStress"
        ]
    }
    if {$queue == "ga100 vkStressMesh"} {
        return [list \
            "./mods -chip galit1_fmodel_64.so -gpubios 0 ga100_sim_hbm.rom gputest.js -readspec ga100fmod.spc -spec pvsSpecVkStressMesh"
        ]
    }

    if {$queue == "ga102 basic"} {
        return [list \
            "./mods -chip galit2_fmodel_64.so -gpubios 0 ga102_sim_gddr6.rom gputest.js -readspec ga102fmod.spc -spec specBasic"
        ]
    }
    if {$queue == "ga102 glrShort"} {
        return [list \
            "./mods -chip galit2_fmodel_64.so -gpubios 0 ga102_sim_gddr6.rom gputest.js -readspec ga102fmod.spc -spec specGlrShort"
        ]
    }
    if {$queue == "ga102 display"} {
        return [list \
            "./mods -gpubios 0 ga102_sim_gddr6_dcb-ultimate-qt.rom -chip galit2_fmodel_64.so gputest.js -readspec ga102fmod.spc -spec specDisp -skip 327"
        ]
    }
    if {$queue == "ga102 Linpack"} {
        return [list \
            "./mods -gpubios 0 ga102_sim_gddr6.rom -chip galit2_fmodel_64.so gputest.js -readspec ga102fmod.spc -spec specLinpack"
        ]
    }
    if {$queue == "ga102 LwdaRandom"} {
        return [list \
            "./mods -gpubios 0 ga102_sim_gddr6.rom -chip galit2_fmodel_64.so gputest.js -readspec ga102fmod.spc -spec specLwdaRandom"
        ]
    }

    if {$queue == "ad10x notest"} {
        return [list \
            "./mods -chip adlit1_fmodel_64.so -gpubios 0 ad102_sim_gddr6.rom gputest.js -readspec ad102fmod.spc -spec specNoTest"
        ]
    }
    if {$queue == "ad10x display"} {
        return [list \
            "./mods -gpubios 0 ad102_sim_gddr6x_dcb-ultimate.rom -chip adlit1_fmodel_64.so gputest.js -readspec ad102fmod.spc -spec specDisp -skip 327"
        ]
    }
    if {$queue == "ad10x instInSys"} {
        return [list \
            "./mods -chip adlit1_fmodel_64.so -gpubios 0 ad102_sim_gddr6.rom gputest.js -readspec ad102fmod.spc -spec specInstInSys"
        ]
    }
    if {$queue == "ad10x fbBroken"} {
        return [list \
            "./mods -chip adlit1_fmodel_64.so -gpubios 0 ad102_sim_gddr6.rom gputest.js -readspec ad102fmod.spc -spec specFbBroken"
        ]
    }
    if {$queue == "ad10x linpack"} {
        return [list \
            "./mods -chip adlit1_fmodel_64.so -gpubios 0 ad102_sim_gddr6.rom gputest.js -readspec ad102fmod.spc -spec specLinpack"
        ]
    }

    if {$queue == "tu102 notest"} {
        return [list \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom  gpugen.js -chipargs -chipPOR=tu102  -null_display -readspec tu10xfmod.spc -notest"
        ]
    }

    if {$queue == "tu102 standard"} {
        return [list \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom  gpugen.js -chipargs -chipPOR=tu102  -null_display -readspec tu10xfmod.spc -spec pvsSpecStandard" \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom gputest.js -chipargs -chipPOR=tu102  -null_display -readspec tu10xfmod.spc -spec pvsSpecStandard"
        ]
    }

    if {$queue == "tu102 display"} {
        return [list \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6_dcb-ultimate.rom  gpugen.js -pmu_bootstrap_mode 1 -chipargs -chipPOR=tu102 -chipargs -idle_timeout=0 -readspec tu10xfmod.spc -spec dvsSpecWithDisplay -lwlink_force_disable" \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6_dcb-ultimate.rom gputest.js -pmu_bootstrap_mode 1 -chipargs -chipPOR=tu102 -chipargs -idle_timeout=0 -readspec tu10xfmod.spc -spec dvsSpecWithDisplay -lwlink_force_disable"
        ]
    }

    if {$queue == "tu102 lwdaRandom"} {
        return [list \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom  gpugen.js -chipargs -chipPOR=tu102 -null_display -lwlink_force_disable -readspec tu10xfmod.spc -spec pvsSpecLwdaRandom"
        ]
    }

    if {$queue == "tu102 lwdaMatsRandom"} {
        return [list \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom gputest.js -chipargs -chipPOR=tu102 -null_display -lwlink_force_disable -no_gold -readspec tu10xfmod.spc -spec pvsSpecLwdaMatsRandom"
        ]
    }

    if {$queue == "tu102 xbar"} {
        return [list \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom gputest.js -chipargs -chipPOR=tu102 -null_display -lwlink_force_disable -no_gold -readspec tu10xfmod.spc -spec pvsSpecXbar"
        ]
    }

    if {$queue == "tu102 cbu_0"} {
        return [list \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom gputest.js -chipargs -chipPOR=tu102 -null_display -lwlink_force_disable -no_gold -readspec tu10xfmod.spc -spec pvsSpecCbu_0"
        ]
    }

    if {$queue == "tu102 cbu_1"} {
        return [list \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom gputest.js -chipargs -chipPOR=tu102 -null_display -lwlink_force_disable -no_gold -readspec tu10xfmod.spc -spec pvsSpecCbu_1"
        ]
    }

    if {$queue == "tu102 ttu"} {
        return [list \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom  gputest.js -chipargs -chipPOR=tu102 -null_display -lwlink_force_disable -no_gold -readspec tu10xfmod.spc -spec pvsSpecTTU"
        ]
    }

    if {$queue == "tu102 linpack"} {
        return [list \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom gputest.js -chipargs -chipPOR=tu102 -null_display -lwlink_force_disable -no_gold -readspec tu10xfmod.spc -spec pvsSpecLinpack"
        ]
    }

    if {$queue == "tu102 glStress"} {
        return [list \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom gputest.js -chipargs -chipPOR=tu102 -null_display -lwlink_force_disable -no_gold -readspec tu10xfmod.spc -spec pvsSpecGlStress"
        ]
    }

    if {$queue == "tu102 glMrtRgbU_0"} {
        return [list \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom  gpugen.js -chipargs -chipPOR=tu102 -null_display -lwlink_force_disable -readspec tu10xfmod.spc -spec pvsSpecGlrMrtRgbU_0"
        ]
    }

    if {$queue == "tu102 glMrtRgbF_0"} {
        return [list \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom  gpugen.js -chipargs -chipPOR=tu102 -null_display -lwlink_force_disable -readspec tu10xfmod.spc -spec pvsSpecGlrMrtRgbF_0"
        ]
    }

    if {$queue == "tu102 glA8R8G8B8_0"} {
        return [list \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom  gpugen.js -chipargs -chipPOR=tu102 -null_display -lwlink_force_disable -readspec tu10xfmod.spc -spec pvsSpecGlrA8R8G8B8_0"
        ]
    }

    if {$queue == "tu102 glR5G6B5_0"} {
        return [list \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom  gpugen.js -chipargs -chipPOR=tu102 -null_display -lwlink_force_disable -readspec tu10xfmod.spc -spec pvsSpecGlrR5G6B5_0"
        ]
    }

    if {$queue == "tu102 glFsaa2x_0"} {
        return [list \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom  gpugen.js -chipargs -chipPOR=tu102 -null_display -lwlink_force_disable -readspec tu10xfmod.spc -spec pvsSpecGlrFsaa2x_0"
        ]
    }

    if {$queue == "tu102 glFsaa4x_0"} {
        return [list \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom  gpugen.js -chipargs -chipPOR=tu102 -null_display -lwlink_force_disable -readspec tu10xfmod.spc -spec pvsSpecGlrFsaa4x_0"
        ]
    }

    if {$queue == "tu102 instInSys"} {
        return [list \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom gputest.js -chipargs -chipPOR=tu102 -null_display -lwlink_force_disable -no_gold -readspec tu10xfmod.spc -spec pvsSpecInstInSys"
        ]
    }

    if {$queue == "tu102 fbBroken"} {
        return [list \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom  gpugen.js -chipargs -chipPOR=tu102 -null_display -lwlink_force_disable -readspec tu10xfmod.spc -spec pvsSpecFbBroken" \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom gputest.js -chipargs -chipPOR=tu102 -null_display -lwlink_force_disable -readspec tu10xfmod.spc -spec pvsSpecFbBroken"
        ]
    }

    if {$queue == "tu102 vkStress"} {
        return [list \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom gputest.js -chipargs -chipPOR=tu102 -null_display -lwlink_force_disable -no_gold -readspec tu10xfmod.spc -spec pvsSpecVkStress"
        ]
    }

    if {$queue == "tu102 vkStressMesh"} {
        return [list \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom gputest.js -chipargs -chipPOR=tu102 -null_display -lwlink_force_disable -no_gold -readspec tu10xfmod.spc -spec pvsSpecVkStressMesh"
        ]
    }

    if {$queue == "tu102 video"} {
        return [list \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom  gpugen.js -chipargs -chipPOR=tu102 -null_display -lwlink_force_disable -readspec tu10xfmod.spc -spec pvsSpecVideo" \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom gputest.js -chipargs -chipPOR=tu102 -null_display -lwlink_force_disable -readspec tu10xfmod.spc -spec pvsSpecVideo"
        ]
    }

    if {$queue == "tu102 unitTests"} {
        return [list \
            "./sim.pl -fmod64 -chip tu102 modstest.js -null_display -readspec tu10xfmod.spc -spec pvsSpelwnitTests"
        ]
    }

    if {$queue == "tu102 mme64"} {
        return [list \
            "./mods -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom gputest.js -chipargs -chipPOR=tu102 -null_display -lwlink_force_disable -no_gold -readspec tu10xfmod.spc -spec pvsSpecMme64"
        ]
    }

    if {$queue == "tu102 sriov"} {
        return [list \
            "./regress_sriov_tu102 gpugen.js pvsSpecStandard" \
            "./regress_sriov_tu102 gputest.js pvsSpecStandard" \
        ]
    }

    if {$queue == "gh100 sriov"} {
        return [list \
            "./regress_sriov_gh100 gputest.js specSriovBasic"
        ]
    }

    return 1
}
