proc get_modssim_mfg_cmds { queue } {

    if {$queue == "tu104_queue1"} {
        return [list \
            "exec ./mods -a -chip tulit2_fmodel_64.so -gpubios 0 tu104_sim_gddr5.rom gpugen.js -chipargs \"-chipPOR=tu104\" -readspec tu10xfmod.spc -spec dvsSpecNoDisplay -null_display >& sim.log" \
            "exec ./mods -a -chip tulit2_fmodel_64.so -gpubios 0 tu104_sim_gddr5.rom gputest.js -chipargs \"-chipPOR=tu104\" -readspec tu10xfmod.spc -spec dvsSpecNoDisplay -null_display >>& sim.log"
        ]
    }
    if {$queue == "tu102_queue1"} {
        return [list \
            "exec ./mods -a -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6_dcb-ultimate.rom  gpugen.js -pmu_bootstrap_mode 1 -chipargs \"-chipPOR=tu102\" -readspec tu10xfmod.spc -spec dvsSpecWithDisplay -lwlink_force_disable -testarg 304 SkipHeadMask 0x3 >& sim.log" \
            "exec ./mods -a -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6_dcb-ultimate.rom  gputest.js -pmu_bootstrap_mode 1 -chipargs \"-chipPOR=tu102\" -readspec tu10xfmod.spc -spec dvsSpecWithDisplay -lwlink_force_disable -testarg 304 SkipHeadMask 0x3 >>& sim.log" \
        ]
    }
    if {$queue == "gv100_queue1"} {
        return [list \
            "exec ./mods -a -chip gvlit1_fmodel_64.so -gpubios 0 gv100_sim_hbm.rom gpugen.js -chipargs \"-chipPOR=gv100\" -readspec gv100sim.spc -spec dvsSpecNoDisplay -null_display >& sim.log" \
            "exec ./mods -a -chip gvlit1_fmodel_64.so -gpubios 0 gv100_sim_hbm.rom gputest.js -chipargs \"-chipPOR=gv100\" -readspec gv100sim.spc -spec dvsSpecNoDisplay -null_display >>& sim.log"
        ]
    }
    if {$queue == "gv100_queue2"} {
        return [list \
            "exec ./mods -a -chip gvlit1_fmodel_64.so -gpubios 0 gv100_sim_hbm_dcb-ultimate.rom gpugen.js -chipargs \"-chipPOR=gv100\" -readspec gv100sim.spc -spec dvsSpecWithDisplay -pmu_bootstrap_mode 1 >& sim.log" \
            "exec ./mods -a -chip gvlit1_fmodel_64.so -gpubios 0 gv100_sim_hbm_dcb-ultimate.rom gputest.js -chipargs \"-chipPOR=gv100\" -readspec gv100sim.spc -spec dvsSpecWithDisplay -pmu_bootstrap_mode 1 >>& sim.log" \
            "exec ./mods -a -chip gvlit1_fmodel_64.so -gpubios 0 gv100_sim_hbm.rom gpugen.js -chipargs -chipPOR=gv100 -readspec gv100sim.spc -spec dvsSpecSpecial -null_display >>& sim.log"
        ]
    }
    if {$queue == "gv100_queue3"} {
        return [list \
            "exec ./mods -a -chip gvlit1_fmodel_64.so -gpubios 0 gv100_sim_hbm.rom gpugen.js -chipargs \"-chipPOR=gv100\" -readspec gv100sim.spc -spec dvsSpecNoDisplayB -null_display >& sim.log" \
            "exec ./mods -a -chip gvlit1_fmodel_64.so -gpubios 0 gv100_sim_hbm.rom gputest.js -chipargs \"-chipPOR=gv100\" -readspec gv100sim.spc -spec dvsSpecNoDisplayB -null_display >>& sim.log"
        ]
    }

    return "please_update_modssim_mfg_cmds"
}
