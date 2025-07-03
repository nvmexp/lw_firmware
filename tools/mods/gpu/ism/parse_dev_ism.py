#! /usr/bin/elw python
#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2013-2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

# Parse a hardware's dev_ism.h file into a mods GpuIsm::IsmChain []
# to be loaded by mods at runtime.
#
#  NOTE : This python script is *Linux ONLY*
# This script runs on the following assumptions when decoding the contents of 
# the dev_ism.h file created by the ref2h process.
# 
# 1. The GpuName is embedded in all of the defines (ie GM107/GM108/GM200 etc)
#  LW_ISM_CHAIN_F0_0_FBP0_CLUSTER_BIN_METAL_GM107_ID
#  LW_ISM_CHAIN_F0_0_FBP0_CLUSTER_BIN_METAL_GM107_WIDTH
#  LW_ISM_CHAIN_F0_0_FBP0_CLUSTER_BIN_METAL_GM107_CHIPLET_SEL
#  LW_ISM_CHAIN_F0_0_FBP0_CLUSTER_OTHER_GM107_MINI_2CLK_KF0ZR_LTCCLK_NOEG_XBARCLK_NOBG
#  etc.
# 2. The define with each chain's instruction ID ends with GpuName_ID (ie GM107_ID).
#  LW_ISM_CHAIN_F0_0_FBP0_CLUSTER_BIN_METAL_GM107_ID
# 3. The define with each chain's width ends with GpuName_WIDTH (ie GM107_WIDTH).
#  LW_ISM_CHAIN_F0_0_FBP0_CLUSTER_BIN_METAL_GM107_WIDTH
# 4. The define with each chain's chiplet select ends with GpuName_CHIPLET_SEL (ie GM107_CHIPLET_SEL).
#  LW_ISM_CHAIN_F0_0_FBP0_CLUSTER_BIN_METAL_GM107_CHIPLET_SEL
# 5. Each of the ISM offset defines have ONLY one of the following strings 
#    encoded in them to identify the type of ISM:
#  _CTRL
#  _CTRL2
#  _1CLK
#  _2CLK
#  _1CLK_DBG
#  _ROSC_COMP
#  _ROSC_BIN
#  _ROSC_BIN_METAL
#  _ROSC_BIN_AGING
#  _VSENSE_DDLY
#  _TSOSC_A
#  _1CLK_NO_EXT_RO
#  _2CLK_DBG
#  _1CLK_NO_EXT_RO_DBG
#  _2CLK_NO_EXT_RO
#  _NMEAS
#  _NMEAS3
import os, sys, struct, getopt, subprocess, re, datetime
g_BadIsmChains = 0;
g_MaxIsmsInOneChain = 0;
g_Verbose = False;
# A list to describe the regexpression to use for each GpuIsm::IsmTypes enum.
# Note: When using the regular expression parser if you have similar stings you 
#       must list that larger string first. ie _CTRL2 must be before _CTRL, &
#       _TSOSC_A2 must be before _TSOSC_A.
g_IsmPatternMatch = [ 
    # _<ISM_TYPE_NAME>
    #   regex                   , Mods GpuIsm::type             , count
    [ "_1CLK_NO_EXT_RO_DBG",    "LW_ISM_MINI_1clk_no_ext_ro_dbg", 0],
    [ "_1CLK_NO_EXT_RO",        "LW_ISM_MINI_1clk_no_ext_ro"    , 0],
    [ "_1CLK_DBG",              "LW_ISM_MINI_1clk_dbg"          , 0],
    [ "_1CLK",                  "LW_ISM_MINI_1clk"              , 0],
    [ "_2CLK_NO_EXT_RO",        "LW_ISM_MINI_2clk_no_ext_ro"    , 0],
    [ "_2CLK_DBG_V3",           "LW_ISM_MINI_2clk_dbg_v3"       , 0],
    [ "_2CLK_DBG_V2",           "LW_ISM_MINI_2clk_dbg_v2"       , 0],
    [ "_2CLK_DBG",              "LW_ISM_MINI_2clk_dbg"          , 0],
    [ "_2CLK_V2",               "LW_ISM_MINI_2clk_v2"           , 0],
    [ "_2CLK",                  "LW_ISM_MINI_2clk"              , 0],
    [ "_ROSC_BIN_METAL",        "LW_ISM_ROSC_bin_metal"         , 0],
    [ "_ROSC_BIN_AGING_V1",     "LW_ISM_ROSC_bin_aging_v1"      , 0],
    [ "_ROSC_BIN_AGING",        "LW_ISM_ROSC_bin_aging"         , 0],
    [ "_ROSC_BIN_V4",           "LW_ISM_ROSC_bin_v4"            , 0],
    [ "_ROSC_BIN_V3",           "LW_ISM_ROSC_bin_v3"            , 0],
    [ "_ROSC_BIN_V2",           "LW_ISM_ROSC_bin_v2"            , 0],
    [ "_ROSC_BIN_V1",           "LW_ISM_ROSC_bin_v1"            , 0],
    [ "_ROSC_BIN",              "LW_ISM_ROSC_bin"               , 0],
    [ "_ROSC_COMP_V2",          "LW_ISM_ROSC_comp_v2"           , 0],
    [ "_ROSC_COMP_V1",          "LW_ISM_ROSC_comp_v1"           , 0],
    [ "_ROSC_COMP",             "LW_ISM_ROSC_comp"              , 0],
    [ "_VNSENSE_DDLY",          "LW_ISM_VNSENSE_ddly"           , 0],
    [ "_TSOSC_A4",              "LW_ISM_TSOSC_a4"               , 0],
    [ "_TSOSC_A3",              "LW_ISM_TSOSC_a3"               , 0],
    [ "_TSOSC_A2",              "LW_ISM_TSOSC_a2"               , 0],
    [ "_TSOSC_A",               "LW_ISM_TSOSC_a"                , 0],
    [ "_NMEAS_LITE_V9",         "LW_ISM_NMEAS_lite_v9"          , 0],
    [ "_NMEAS_LITE_V8",         "LW_ISM_NMEAS_lite_v8"          , 0],
    [ "_NMEAS_LITE_V7",         "LW_ISM_NMEAS_lite_v7"          , 0],
    [ "_NMEAS_LITE_V6",         "LW_ISM_NMEAS_lite_v6"          , 0],
    [ "_NMEAS_LITE_V5",         "LW_ISM_NMEAS_lite_v5"          , 0],
    [ "_NMEAS_LITE_V4",         "LW_ISM_NMEAS_lite_v4"          , 0],
    [ "_NMEAS_LITE_V3",         "LW_ISM_NMEAS_lite_v3"          , 0],
    [ "_NMEAS_LITE_V2",         "LW_ISM_NMEAS_lite_v2"          , 0],
    [ "_NMEAS_LITE",            "LW_ISM_NMEAS_lite"             , 0],
    [ "_NMEAS4",                "LW_ISM_NMEAS_v4"               , 0],
    [ "_NMEAS3",                "LW_ISM_NMEAS_v3"               , 0],
    [ "_NMEAS",                 "LW_ISM_NMEAS_v2"               , 0],
    [ "_HOLD",                  "LW_ISM_HOLD"                   , 0],
    [ "_AGING_V4",              "LW_ISM_aging_v4"               , 0],
    [ "_AGING_V3",              "LW_ISM_aging_v3"               , 0],
    [ "_AGING_V2",              "LW_ISM_aging_v2"               , 0],
    [ "_AGING_V1",              "LW_ISM_aging_v1"               , 0],
    [ "_AGING",                 "LW_ISM_aging"                  , 0],
    [ "_CPM_V6",                "LW_ISM_cpm_v6"                 , 0],
    [ "_CPM_V5",                "LW_ISM_cpm_v5"                 , 0],
    [ "_CPM_V4",                "LW_ISM_cpm_v4"                 , 0],
    [ "_CPM_V3",                "LW_ISM_cpm_v3"                 , 0],
    [ "_CPM_V2",                "LW_ISM_cpm_v2"                 , 0],
    [ "_CPM_V1",                "LW_ISM_cpm_v1"                 , 0],
    [ "_CPM",                   "LW_ISM_cpm"                    , 0],
    [ "_IMON",                  "LW_ISM_imon"                   , 0],
    [ "_IMON_V2",               "LW_ISM_imon_v2"                , 0],
    # Ada special MACROS
    [ "LWRRENT_SINK",           "LW_ISM_lwrrent_sink"           , 0],
    # Maxwell has some special defines for NMEAS_v2 that need to be included as part of the 
    # general parser.
    [ "SFE_CTRL",            "LW_ISM_NMEAS_v2"                   , 0],
    [ "VST_CTRL",            "LW_ISM_NMEAS_v2"                   , 0],
    [ "VST_DATA",            "LW_ISM_NMEAS_v2"                   , 0],
    # move the _CTRL? tags at the bottom because on Turing they defined some non-control macros
    # with _CTRL as part of the name. Ugh... I'll see if I can get them to fix that.
    [ "_CTRL4",              "LW_ISM_ctrl_v4"                , 0],
    [ "_CTRL3",              "LW_ISM_ctrl_v3"                , 0],
    [ "_CTRL2",              "LW_ISM_ctrl_v2"                , 0],
    [ "_CTRL",               "LW_ISM_ctrl"                   , 0]
]

g_IsmChainTypeFlags = [
    #   regex                   , Mods Ism::IsmChainFlags
    [ "FCCPLEX",                  "Ism::FAST_CPU_CHAIN" ],
    [ "SCCPLEX",                  "Ism::SLOW_CPU_CHAIN" ]
]

# default locations and filenames when running in batchMode.
g_IsmFiles = [
    # Device,     input filename,              output filename,   Is LwSwitch dev?
     ["GM107",    "maxwell/gm107/dev_ism.h",   "gm107_ism.cpp",   False]
    ,["GM108",    "maxwell/gm108/dev_ism.h",   "gm108_ism.cpp",   False]
    ,["GM200",    "maxwell/gm200/dev_ism.h",   "gm200_ism.cpp",   False]
    ,["GM204",    "maxwell/gm204/dev_ism.h",   "gm204_ism.cpp",   False]
    ,["GM206",    "maxwell/gm206/dev_ism.h",   "gm206_ism.cpp",   False]
    ,["T234",     "t23x/t234/dev_ism.h",       "t234_ism.cpp",    False]
    ,["T194",     "t19x/t194/dev_ism.h",       "t194_ism.cpp",    False]
    ,["GP100",    "pascal/gp100/dev_ism.h",    "gp100_ism.cpp",   False]
    ,["GP102",    "pascal/gp102/dev_ism.h",    "gp102_ism.cpp",   False]
    ,["GP104",    "pascal/gp104/dev_ism.h",    "gp104_ism.cpp",   False]
    ,["GP106",    "pascal/gp106/dev_ism.h",    "gp106_ism.cpp",   False]
    ,["GP107",    "pascal/gp108/dev_ism.h",    "gp107_ism.cpp",   False]
    ,["GP108",    "pascal/gp108/dev_ism.h",    "gp108_ism.cpp",   False]
    ,["GV100",    "volta/gv100/dev_ism.h",     "gv100_ism.cpp",   False]
    ,["SVNP01",   "lwswitch/svnp01/dev_ism.h", "svnp01_ism.cpp",  True]
    ,["TU102",    "turing/tu102/dev_ism.h",    "tu102_ism.cpp",   False]
    ,["TU104",    "turing/tu104/dev_ism.h",    "tu104_ism.cpp",   False]
    ,["TU106",    "turing/tu106/dev_ism.h",    "tu106_ism.cpp",   False]
    ,["TU116",    "turing/tu116/dev_ism.h",    "tu116_ism.cpp",   False]
    ,["TU117",    "turing/tu117/dev_ism.h",    "tu117_ism.cpp",   False]
    ,["GA100",    "ampere/ga100/dev_ism.h",    "ga100_ism.cpp",   False]
    ,["GA102",    "ampere/ga102/dev_ism.h",    "ga102_ism.cpp",   False]
    ,["GA103",    "ampere/ga103/dev_ism.h",    "ga103_ism.cpp",   False]
    ,["GA104",    "ampere/ga104/dev_ism.h",    "ga104_ism.cpp",   False]
    ,["GA106",    "ampere/ga106/dev_ism.h",    "ga106_ism.cpp",   False]
    ,["GA107",    "ampere/ga107/dev_ism.h",    "ga107_ism.cpp",   False]
    ,["GH100",    "hopper/gh100/dev_ism.h",    "gh100_ism.cpp",   False]
    ,["LR10",     "lwswitch/lr10/dev_ism.h",   "lr10_ism.cpp",    True]
    ,["GH202",    "hopper/gh202/dev_ism.h",    "gh202_ism.cpp",   False]
    ,["AD102",    "ada/ad102/dev_ism.h",       "ad102_ism.cpp",   False]
    ,["AD103",    "ada/ad103/dev_ism.h",       "ad103_ism.cpp",   False]
    ,["AD104",    "ada/ad104/dev_ism.h",       "ad104_ism.cpp",   False]
    ,["AD106",    "ada/ad106/dev_ism.h",       "ad106_ism.cpp",   False]
    ,["AD107",    "ada/ad107/dev_ism.h",       "ad107_ism.cpp",   False]
]

# skip chains defined with <item>_<gpu> as non-ISM chains
g_SkipChainDefs = [
    "JTAG2TMC_CHIPCTL_BITS_"
   ,"POWER_RESET_ENABLE_"
]

def print_chain (chain):
    for entry in chain :
        print( entry[0] + " " + entry[1])
    return

#-------------------------------------------------------------------------------
# Parse the textList for ISM chain definitions for the specific gpu.
# Below is an example of a single ISM chain that would be in the textList
# #define LW_ISM_CHAIN_GX0_0_GPC0_CLUSTER_DOWN_GM108_ID                                               0x2F
# #define LW_ISM_CHAIN_GX0_0_GPC0_CLUSTER_DOWN_GM108_WIDTH                                              71
# #define LW_ISM_CHAIN_GX0_0_GPC0_CLUSTER_DOWN_GM108_CHIPLET_SEL                                         2
# #define LW_ISM_CHAIN_GX0_0_GPC0_CLUSTER_DOWN_GM108_MINI_2CLK_GG0GP_GPCCLK_NOBG_XBARCLK_NOBG            1
# #define LW_ISM_CHAIN_GX0_0_GPC0_CLUSTER_DOWN_GM108_MINI_2CLK_GG0XBG_GPCCLK_NOBG_XBARCLK_NOBG          36
#-------------------------------------------------------------------------------
def parse_chains (dev, textList):
    global g_BadIsmChains
    global g_MaxIsmsInOneChain
    global g_Verbose
    # Note: we cant use a dictionary here because dictionaries are unorded.
    ismChains = []; # a list of lists of tuples
    idx = 0;
    #Search for strings like this:
    #define LW_ISM_CHAIN_GX0_0_GPC0_CLUSTER_DOWN_GM206_ID           0x84
    #define LW_ISM_CHAIN_GX0_0_GPC0_CLUSTER_DOWN_GM206_WIDTH        139
    #define LW_ISM_CHAIN_GX0_0_GPC0_CLUSTER_DOWN_GM206_CHIPLET_SEL  1
    #define LW_ISM_CHAIN_GX0_0_GPC0_CLUSTER_DOWN_GM206_SFE_CTRL     0x0010477c
    #define LW_ISM_CHAIN_GX0_0_GPC0_CLUSTER_DOWN_GM206_VST_CTRL     0x001044f8
    #define LW_ISM_CHAIN_GX0_0_GPC0_CLUSTER_DOWN_GM206_VST_DATA     0x001044fc
    # group1 = LW_ISM_CHAIN_GX0_0_GPC0_CLUSTER_DOWN_GM206_ID
    # group2 = 0x84
    exp = '([\w]*' + dev +'_ID\s)\s+(0x[0-9A-F]*)'
    reIsmId = re.compile(exp)
    while idx < len(textList):
        bSkipChain = False
        for skipChainPtrn in g_SkipChainDefs:
            reSkipChainId = re.compile('([\w]*' + skipChainPtrn + dev +'_ID\s)')
            reSkipChainMatch = reSkipChainId.search(textList[idx])
            if reSkipChainMatch != None:
                bSkipChain = True
                skipChainBase = str(reSkipChainMatch.group(1)).rpartition('_')
                if g_Verbose:
                    print("\nSkipping chain with base:" + skipChainBase[0])

                skipChain = re.compile('(' + skipChainBase[0]  + '[\w]+)\s+')
                while idx < len(textList):
                    reMatch = skipChain.search(textList[idx])
                    if reMatch == None:
                        break;
                    if g_Verbose:
                        print("\nSkipping : " + str(reMatch.group(1)))
                    idx = idx+1
                break
        if bSkipChain:
            continue

        reMatch = reIsmId.search(textList[idx])
        if reMatch != None:
            ismChainBase = str(reMatch.group(1)).rpartition('_')
            #reform the regex to include the ismChainBase ie.
            #ismChainBase[0] = "LW_ISM_CHAIN_GX0_0_GPC0_CLUSTER_DOWN_GM206"
            #ismChainBase[1] = "_"
            #ismChainBase[2] = "ID"
            #group(1) = "LW_ISM_CHAIN_GX0_0_GPC0_CLUSTER_DOWN_GM206_*"
            #group(2) = the value for the define, ie 0x84, 139, 1, etc.
            exp = '(' + ismChainBase[0] + '[\w]*' + ')' + '\s*([xX0-9A-Fa-f]*)'
            reChainId = re.compile(exp)
            ismChain = []; # a list of tuples
            if g_Verbose:
                print("\nNew chain with base:" + ismChainBase[0])
            while idx < len(textList):
                reMatch = reChainId.search(textList[idx])
                if reMatch == None:
                    break
                if len(ismChain) < 3:
                    ismChain.append((reMatch.group(1),reMatch.group(2)))
                    if g_Verbose:
                        print("adding " + reMatch.group(1) + "  " + reMatch.group(2))
                else:
                    if getModsIsmType(reMatch.group(1),0) != "Ism::LW_ISM_none":
                        if g_Verbose:
                            print("adding " + reMatch.group(1) + "  " + reMatch.group(2))
                        ismChain.append((reMatch.group(1),reMatch.group(2)))
                    else:
                        if g_Verbose:
                            print("skipping " + reMatch.group(1));
                idx = idx+1
            if len(ismChain) < 4:
                g_BadIsmChains = g_BadIsmChains + 1
                print("*** MISSING ISM OFFSETS! PLEASE FIX THIS BEFORE USING THIS dev_ism.h FILE! ***")
                print_chain(ismChain)
            else:
                ismChains.append(ismChain)
        else:
            idx = idx + 1
    return ismChains

#-------------------------------------------------------------------------------
# Return a string with the proper GpuIsm::IsmTypes for the ism define.
#-------------------------------------------------------------------------------
def getModsIsmType(ism, increment):
    global g_IsmPatternMatch
    for ptrn in g_IsmPatternMatch:
        reMatch = re.search(ptrn[0],ism)
        if reMatch != None:
            ptrn[2] = ptrn[2] + increment
            return "Ism::" + ptrn[1]

    print("WARNING.... Unable to determine the type of ISM for: #define " + ism)
    print("Your list will be incomplete!")
    return "Ism::LW_ISM_none"

#------------------------------------------------------------------------------
# Return a string with the hexidecimal value for the flags or null if flags=0
# ismType = one of the gpuISM types in g_IsmPatternMatch[1] prepended with "Ism::"
#------------------------------------------------------------------------------
def getModsIsmTypeFlags(ismType, ism):
    exp = 'Ism::LW_ISM_ctrl'
    reMatch = re.search("Ism::LW_ISM_ctrl",ismType)
    if reMatch != None:
        reMatch = re.search("CHAIN_SYS0|CHAIN_S0|CHAIN_SX0",ism)
        if reMatch != None:
            return "Ism::IsmFlags::PRIMARY, "
    return "Ism::IsmFlags::FLAGS_NONE, "


def getModsIsmChainTypeFlags(ism):
    global g_IsmChainTypeFlags
    connectChar = ""
    flags = ""
    for ptrn in g_IsmChainTypeFlags:
        reMatch = re.search(ptrn[0],ism)
        if reMatch != None:
            flags += connectChar + ptrn[1]
            connectChar = "|"
    if flags == "":
        flags = "Ism::NORMAL_CHAIN"
    return flags

#-------------------------------------------------------------------------------
def get_ism_stats(ismChains):
    global g_MaxIsmsInOneChain
    global g_Verbose
    # reset the global counters.
    g_MaxIsmsInOneChain = 0
    for ptrn in g_IsmPatternMatch:
        ptrn[2] = 0

    for chain in ismChains:
        exp = "SFE_CTRL"
			
        if g_Verbose:
            print("\nlen(chain) = " + str(len(chain)) );
            print(chain)
        reMatch = re.search(exp, chain[3][0])
        if reMatch != None:
            idx = 6
        else:
            idx = 3

        if g_MaxIsmsInOneChain < (len(chain)-idx):
            g_MaxIsmsInOneChain = (len(chain)-idx)

        for i in range(idx,len(chain)) :
            entry = chain[i]
            getModsIsmType(entry[0],1)

#-------------------------------------------------------------------------------
def print_ism_stats(dev, ismChains, outputFile):
    get_ism_stats(ismChains)
    global g_IsmPatternMatch
    global g_MaxIsmsInOneChain
    # the last chain is a sentinel, don't count it
    outputFile.write("    //ISM Chains:" + str(len(ismChains)) + "\n")
    outputFile.write("    //Max ISMs in a single chain:    " + str(g_MaxIsmsInOneChain) + "\n")
    for ptrn in g_IsmPatternMatch:
        outputFile.write("    //" + ptrn[1] +":" + str(ptrn[2]) + "\n")


#-------------------------------------------------------------------------------
# ismChains is a list of lists of tuples
# each list of tuples is the complete definition of 1 ISM chain
# ismChain[0] = (instruction id define, value)
# ismChain[1] = (chain length define, value)
# ismChain[2] = (chain chiplet select, value)
# ismChain[3..x] = (ISM x offset define, value)
#-------------------------------------------------------------------------------
def build_mods_chain_init_func(dev, ismChains,outputFile):
    chainCount = 0;
    outputFile.write("    //--------------------------------------------------------------------------\n")
    outputFile.write("    // " + dev + " ISM TABLE\n")
    outputFile.write("    //--------------------------------------------------------------------------\n")
    vectorName = dev + "SpeedoChains"
	
    if getIsSwitch(dev):
        outputFile.write("vector<LwSwitchIsm::IsmChain> " + vectorName +";\n")
    else:
        outputFile.write("vector<GpuIsm::IsmChain> " + vectorName +";\n")
	
    outputFile.write("void Initialize" + dev + "SpeedoChains()\n")
    outputFile.write("{\n")
    outputFile.write("    " + vectorName + ".clear();\n")
    outputFile.write("    " + vectorName + ".reserve(" + str(len(ismChains)) + ");\n")
    commaOrSpace = ' '
    for chain in ismChains:
        if getIsSwitch(dev):
            outputFile.write("    " + vectorName + ".push_back(LwSwitchIsm::IsmChain(\n")
        else:
            outputFile.write("    " + vectorName + ".push_back(GpuIsm::IsmChain(\n")
			
        entry = chain[0]
        outputFile.write("        " + entry[0] + ",  //InstrId=" + entry[1] + "//$\n")
        entry = chain[1]
        outputFile.write("        " + entry[0] + ",  //Size=" + entry[1] + "(in bits)//$\n")
        entry = chain[2]
        outputFile.write("        " + entry[0] + ",  //Chiplet=" + entry[1] + "//$\n")

        idx = 3 # default
        exp = "SFE_CTRL"
        reMatch = re.search(exp, chain[3][0])
        if reMatch != None:
            # we have a chain with new NMEAS parameters
            idx = 6; 
            entry = chain[3]
            outputFile.write("        " + entry[0] + ",  //Sfe=" + entry[1] + "\n")
            entry = chain[4]
            outputFile.write("        " + entry[0] + ",  //VstCtrl=" + entry[1] + "\n")
            entry = chain[5]
            outputFile.write("        " + entry[0] + ",  //VstData=" + entry[1] + "\n")
        else:
            outputFile.write("        " + "~0, //Sfe=undefined\n")
            outputFile.write("        " + "~0, //VstCtrl=undefined\n")
            outputFile.write("        " + "~0, //VstData=undefined\n")
                
        flags = getModsIsmChainTypeFlags(chain[0][0]);
        outputFile.write("        " + flags + "));  //Flags\n")
        outputFile.write("        // ISMs in the chain (type, flags, startBit)\n")
        commaOrSpace2 = ' '
        numISMs = len(chain)-idx;
        outputFile.write("        " + vectorName + "[" + str(chainCount) + "].ISMs.reserve(" + str(numISMs) + ");\n")
        for i in range(idx,len(chain)) :
            entry = chain[i]
            ismType = getModsIsmType(entry[0],0) + ","
            ismFlags = getModsIsmTypeFlags(ismType, entry[0])
            ismType = ("%-38s" % ismType)
            ismFlags = ("%-18s" % ismFlags)
            if getIsSwitch(dev):
                outputFile.write("        " + vectorName + "[" + str(chainCount) + "].ISMs.push_back(LwSwitchIsm::IsmRef(" + ismType + ismFlags + entry[0] + ")); //$= " + entry[1] + "\n")
            else:
                outputFile.write("        " + vectorName + "[" + str(chainCount) + "].ISMs.push_back(GpuIsm::IsmRef(" + ismType + ismFlags + entry[0] + ")); //$= " + entry[1] + "\n")
        chainCount = chainCount + 1
        outputFile.write("\n")

    outputFile.write("}\n")


#-------------------------------------------------------------------------------
def get_sw_file_path(filename):
    exp = "(\/sw\/[\w\/]*" + os.path.basename(filename) + ")"
    reMatch = re.search(exp,os.path.abspath(filename))
    if reMatch != None:
        return reMatch.group(1)
    return ""
#-------------------------------------------------------------------------------
def getIncludePath(dev):
    for entry in g_IsmFiles:
        if entry[0] == dev:
            return entry[1]
    return ""
#-------------------------------------------------------------------------------
def getIsSwitch(dev):
    for entry in g_IsmFiles:
        if entry[0] == dev:
            return entry[3]
    return False

#-------------------------------------------------------------------------------
# Process the dev_ism.h file for all of the ISM chains for a specific gpu.
# Create a GpuIsm::IsmChain [] for the ISMs and write them to the outputFilename.
#-------------------------------------------------------------------------------
def process_ism_file(dev, inputFilename, outputFilename):
    global g_BadIsmChains
    global g_MaxIsmsInOneChain
    print("\nProcessing: dev=" + dev + " inputFilename=" + inputFilename + " outputFilename=" + outputFilename)
    g_MaxIsmsInOneChain = 0
    g_BadIsmChains = 0;
    inputFile = open(inputFilename, 'r')
    outputFile = open(outputFilename,'w')
    textList = inputFile.readlines();
    now = datetime.datetime.now();
    outputFile.write("/*\n")
    outputFile.write(" * LWIDIA_COPYRIGHT_BEGIN\n")
    outputFile.write(" *\n")
    outputFile.write(" * Copyright %04d by LWPU Corporation.  All rights reserved.  All\n" % now.year)
    outputFile.write(" * information contained herein is proprietary and confidential to LWPU\n")
    outputFile.write(" * Corporation.  Any use, reproduction, or disclosure without the written\n")
    outputFile.write(" * permission of LWPU Corporation is prohibited.\n")
    outputFile.write(" *\n")
    outputFile.write(" * LWIDIA_COPYRIGHT_END\n")
    outputFile.write(" */\n")
    includePath = getIncludePath(dev);
    if includePath != "":
        outputFile.write("#include \"" + includePath + "\"\n");

    outputFile.write("    //--------------------------------------------------------------------------\n")
    outputFile.write("    // " + str(now) + "\n")
    outputFile.write("    // This table was auto generated by:" + get_sw_file_path(sys.argv[0]) + "\n")
    outputFile.write("    // Source file:" + get_sw_file_path(inputFilename) + "\n")
    outputFile.write("    // Do not modify contents\n")
    outputFile.write("    //--------------------------------------------------------------------------\n")

    # returns a list of lists of tuples
    ismChains = parse_chains(dev,textList)
    print_ism_stats(dev,ismChains,outputFile)
    build_mods_chain_init_func(dev,ismChains,outputFile)
    #build_mods_array(gpu, ismChains,outputFile)
    print("Finished: Bad ISM Chains:" + str(g_BadIsmChains) + " Good ISM Chains:" + str(len(ismChains)-1) + " Max ISMs per chain:" + str(g_MaxIsmsInOneChain))

    inputFile.close()
    outputFile.close()

#-------------------------------------------------------------------------------
# Print usage string
#-------------------------------------------------------------------------------
def usage ():
    print("Usage1: python parse_dev_ism.py -d <Gpu> -i <inputfilename> -o <outputfilename> [-verbose]")
    print("e.g.:  python parse_dev_ism.py -d GM107 -i ../../../../drivers/common/inc/hwref/maxwell/gm107/dev_ism.h -o gm107_ism.out")
    print("Usage2: python parse_dev_ism.py -d <LwSwitch> -i <inputfilename> -o <outputfilename> [-verbose]")
    print("e.g.:  python parse_dev_ism.py -d SVNP01 -i ../../../../drivers/common/inc/hwref/lwswitch/svnp01/dev_ism.h -o svnp01_ism.out")
    print("Usage3: python parse_dev_ism.py -b true -i <BaseInputDir> -o <BaseOutputDir>")
    print("e.g. python parse_dev_ism.py -b true -i ../../../../drivers/common/inc/hwref -o .")


#-------------------------------------------------------------------------------
# Main entry point.
#-------------------------------------------------------------------------------
def main ():
    global g_Verbose
    if os.name != "posix":
        print("parsing dev_ism.h files must be done on a Linux system")
        sys.exit(3)

    # Parse the options from the command line
    try:
        opts, args = getopt.getopt(sys.argv[1:], "d:i:o:b:v", ["dev=", "inputfilename=", "outputfilename=","batchMode=","verbose"])
    except getopt.GetoptError as err:
        print(str(err))
        usage()
        sys.exit(2)
    inputFilename = ""
    outputFilename = ""
    dev = ""
    runBatchMode = "false"
    for o, a in opts:
        if o in ("-d", "--dev"):
            dev = a
        elif o in ("-i", "--inputfilename"):
            inputFilename = a
        elif o in ("-o", "--outputfilename"):
            outputFilename = a
        elif o in ("-b", "--batchMode"):
            runBatchMode = a
        elif o in ("-v", "--verbose"):
            g_Verbose = True
        else:
            usage()
            sys.exit(2)

    if (dev == ""  or inputFilename == "" or outputFilename == "")  and runBatchMode == "false":
        usage()
        sys.exit(2)

    if runBatchMode == "true":
        #Use inputFilename as the input base directory
        #Use outputFilename as the output base directory
        #Fill in gpu for all known GPUs to date.
        LW_ISM_MAX_PER_chain = 0
        print("Running batch mode:")
        for entry in g_IsmFiles:
            process_ism_file(entry[0], inputFilename+"/"+entry[1], outputFilename+"/"+entry[2])
    else:
        process_ism_file(dev, inputFilename, outputFilename)

if __name__ == "__main__":
    main()


