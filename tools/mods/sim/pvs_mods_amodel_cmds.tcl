proc get_pvs_mods_amodel_cmds { queue } {
    add_log "get_pvs_mods_amodel_cmds: Searching for: $queue"

    if {$queue == "ad102 video"} {
        return [list \
            "./mods gputest.js -readspec ad102amod.spc -spec dvsSpecVideoLwjpg" \
            "./mods gputest.js -readspec ad102amod.spc -spec dvsSpecVideoLwofa" \
            "./mods gputest.js -readspec ad102amod.spc -spec dvsSpecVideoLwenc" \
            "./mods gputest.js -readspec ad102amod.spc -spec dvsSpecVideoLwdecLegacy"
        ]
    }
    if {$queue == "gh100 video"} {
        return [list \
            "./mods gputest.js -readspec gh100amod.spc -spec dvsSpecVideo"
        ]
    }
    if {$queue == "ga102 opengl"} {
        return [list \
            "./mods gputest.js -readspec ga102amod.spc -spec dvsSpecGlrShort"
        ]
    }
    if {$queue == "ga102 glrOcg_0"} {
        return [list \
            "./mods gputest.js -readspec ga102amod.spc -spec dvsSpecGlrOcg_0"
        ]
    }
    if {$queue == "ga102 glrOcg_1"} {
        return [list \
            "./mods gputest.js -readspec ga102amod.spc -spec dvsSpecGlrOcg_1"
        ]
    }
    if {$queue == "ga102 glrOcg_2"} {
        return [list \
            "./mods gputest.js -readspec ga102amod.spc -spec dvsSpecGlrOcg_2"
        ]
    }
    if {$queue == "ga102 glrOcg_3"} {
        return [list \
            "./mods gputest.js -readspec ga102amod.spc -spec dvsSpecGlrOcg_3"
        ]
    }
    if {$queue == "ga102 glrOcg_4"} {
        return [list \
            "./mods gputest.js -readspec ga102amod.spc -spec dvsSpecGlrOcg_4"
        ]
    }
    if {$queue == "ga102 glrOcg_5"} {
        return [list \
            "./mods gputest.js -readspec ga102amod.spc -spec dvsSpecGlrOcg_5"
        ]
    }
    if {$queue == "ga102 glrOcg_6"} {
        return [list \
            "./mods gputest.js -readspec ga102amod.spc -spec dvsSpecGlrOcg_6"
        ]
    }
    if {$queue == "ga100 opengl"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec dvsSpecGlrShort"
        ]
    }
    if {$queue == "ga100 glrOcg_0"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec dvsSpecGlrOcg_0"
        ]
    }
    if {$queue == "ga100 glrOcg_1"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec dvsSpecGlrOcg_1"
        ]
    }
    if {$queue == "ga100 glrOcg_2"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec dvsSpecGlrOcg_2"
        ]
    }
    if {$queue == "ga100 glrOcg_3"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec dvsSpecGlrOcg_3"
        ]
    }
    if {$queue == "ga100 glrOcg_4"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec dvsSpecGlrOcg_4"
        ]
    }
    if {$queue == "ga100 glrOcg_5"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec dvsSpecGlrOcg_5"
        ]
    }
    if {$queue == "ga100 glrOcg_6"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec dvsSpecGlrOcg_6"
        ]
    }
    if {$queue == "ga100 notest"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec dvsSpecNoTest"
        ]
    }
    if {$queue == "ga100 special"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec dvsSpecSpecial"
        ]
    }
    if {$queue == "ga100 standard"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec pvsSpecStandard"
        ]
    }
    if {$queue == "ga100 cbu_0"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec pvsSpecCbu_0"
        ]
    }
    if {$queue == "ga100 cbu_1"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec pvsSpecCbu_1"
        ]
    }
    if {$queue == "ga100 lwdaRandom"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec pvsSpecLwdaRandom"
        ]
    }
    if {$queue == "ga100 lwdaMatsRandom"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec pvsSpecLwdaMatsRandom"
        ]
    }
    if {$queue == "ga100 linpack"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec pvsSpecLinpack"
        ]
    }
    if {$queue == "ga100 instInSys"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec pvsSpecInstInSys"
        ]
    }
    if {$queue == "ga100 fbBroken"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec pvsSpecFbBroken"
        ]
    }
    if {$queue == "ga100 glrShort"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec dvsSpecGlrShort"
        ]
    }
    if {$queue == "ga100 glStress"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec pvsSpecGlStress"
        ]
    }
    if {$queue == "ga100 glrA8R8G8B8_0"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec pvsSpecGlrA8R8G8B8_0"
        ]
    }
    if {$queue == "ga100 glrR5G6B5_0"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec pvsSpecGlrR5G6B5_0"
        ]
    }
    if {$queue == "ga100 glrFsaa2x_0"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec pvsSpecGlrFsaa2x_0"
        ]
    }
    if {$queue == "ga100 glrFsaa4x_0"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec pvsSpecGlrFsaa4x_0"
        ]
    }
    if {$queue == "ga100 glrMrtRgbU_0"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec pvsSpecGlrMrtRgbU_0"
        ]
    }
    if {$queue == "ga100 glrMrtRgbF_0"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec pvsSpecGlrMrtRgbF_0"
        ]
    }
    if {$queue == "ga100 mme64"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec pvsSpecMme64"
        ]
    }
    if {$queue == "ga100 unitTests"} {
        return [list \
            "./mods modstest.js -readspec ga100amod.spc -spec pvsSpelwnitTests"
        ]
    }
    if {$queue == "ga100 video"} {
        return [list \
            "./mods gpugen.js -readspec amodel_lwenc.spc -spec pvsSpecLwEnc" \
            "./mods gputest.js -readspec amodel_lwenc.spc -spec pvsSpecLwEnc"
        ]
    }
    if {$queue == "ga100 vkStress"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec pvsSpecVkStress"
        ]
    }
    if {$queue == "ga100 vkStressMesh"} {
        return [list \
            "./mods gputest.js -readspec ga100amod.spc -spec pvsSpecVkStressMesh"
        ]
    }

    return 1
}
