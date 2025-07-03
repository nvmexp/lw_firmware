#!/usr/bin/python3
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
import json
import re
import os
import sys, getopt

NUM_OF_WRITES = 6

galit1 = {
    "max_fbpaSubpMask" : 0xFFFFFFFFFFFF,
    "LW_PFB_FBPA_0_TRAINING_DEBUG_CTRL": 0x009009C0, 
    "LW_PFB_FBPA_0_TRAINING_DEBUG_CTRL_SELECT": "7:0", 
    "LW_PFB_FBPA_0_TRAINING_DEBUG_DQ0": 0x00900938, 
    "LW_PFB_FBPA_0_TRAINING_DEBUG_DQ1": 0x00900940, 
    "LW_PFB_FBPA_0_TRAINING_DEBUG_DQ2": 0x00900948, 
    "LW_PFB_FBPA_0_TRAINING_DEBUG_DQ3": 0x00900950, 
    "LW_PFB_FBPA_0_TRAINING_DP": 0x00900918, 
    "LW_PFB_FBPA_0_TRAINING_DP_CNTD": 0x00900920, 
    "LW_PFB_FBPA_0_TRAINING_DP_CNTD_DBI": "3:0", 
    "LW_PFB_FBPA_0_TRAINING_DP_CNTD_EDC": "7:4", 
    "LW_PFB_FBPA_0_TRAINING_DP_CNTD_SEL": "8:8", 
    "LW_PFB_FBPA_0_TRAINING_DP_CNTD_SEL_CHAR_ENGINE": 0x00000001, 
    "LW_PFB_FBPA_0_TRAINING_PATTERN_PTR": 0x00900968, 
    "LW_PFB_FBPA_0_TRAINING_PATTERN_PTR_ACT_ADR": "31:31", 
    "LW_PFB_FBPA_0_TRAINING_PATTERN_PTR_ACT_ADR_ENABLED": 0x00000001, 
    "LW_PFB_FBPA_0_TRAINING_PATTERN_PTR_DP": "15:8", 
    "LW_PFB_FBPA_0_UC_CTL": 0x009001C0, 
    "LW_PFB_FBPA_0_UC_CTL_ENABLE": "0:0", 
    "LW_PFB_FBPA_0_UC_CTL_ENABLE_FALSE": 0x00000000, 
    "LW_PFB_FBPA_0_UC_CTL_ENABLE_TRUE": 0x00000001, 
    "LW_PFB_FBPA_0_UC_CTL_LOAD_ADDR": "26:20", 
    "LW_PFB_FBPA_0_UC_CTL_LOAD_ENABLE": "19:19", 
    "LW_PFB_FBPA_0_UC_CTL_LOAD_ENABLE_TRUE": 0x00000001, 
    "LW_PFB_FBPA_0_UC_CTL_START_ADDR": "7:1", 
    "LW_PFB_FBPA_0_UC_DATA": 0x009001C8, 
    "LW_PFB_FBPA_TRAINING_DEBUG_CTRL": 0x009A09C0, 
    "LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT": "7:0", 
    "LW_PFB_FBPA_TRAINING_DEBUG_DQ0": 0x009A0938, 
    "LW_PFB_FBPA_TRAINING_DEBUG_DQ1": 0x009A0940, 
    "LW_PFB_FBPA_TRAINING_DEBUG_DQ2": 0x009A0948, 
    "LW_PFB_FBPA_TRAINING_DEBUG_DQ3": 0x009A0950, 
    "LW_PFB_FBPA_TRAINING_DP": 0x009A0918, 
    "LW_PFB_FBPA_TRAINING_DP_CNTD": 0x009A0920,
    "LW_PFB_FBPA_TRAINING_DP_CNTD_DBI": "3:0", 
    "LW_PFB_FBPA_TRAINING_DP_CNTD_EDC": "7:4", 
    "LW_PFB_FBPA_TRAINING_DP_CNTD_SEL": "8:8", 
    "LW_PFB_FBPA_TRAINING_DP_CNTD_SEL_CHAR_ENGINE": 0x00000001, 
    "LW_PFB_FBPA_TRAINING_PATTERN_PTR": 0x009A0968, 
    "LW_PFB_FBPA_TRAINING_PATTERN_PTR_ACT_ADR": "31:31", 
    "LW_PFB_FBPA_TRAINING_PATTERN_PTR_ACT_ADR_ENABLED": 0x00000001, 
    "LW_PFB_FBPA_TRAINING_PATTERN_PTR_DP": "15:8", 
    "LW_PFB_FBPA_UC_CTL": 0x009A01C0, 
    "LW_PFB_FBPA_UC_CTL_ENABLE": "0:0", 
    "LW_PFB_FBPA_UC_CTL_ENABLE_FALSE": 0x00000000, 
    "LW_PFB_FBPA_UC_CTL_ENABLE_TRUE": 0x00000001, 
    "LW_PFB_FBPA_UC_CTL_LOAD_ADDR": "26:20", 
    "LW_PFB_FBPA_UC_CTL_LOAD_ENABLE": "19:19", 
    "LW_PFB_FBPA_UC_CTL_LOAD_ENABLE_TRUE": 0x00000001, 
    "LW_PFB_FBPA_UC_CTL_START_ADDR": "7:1", 
    "LW_PFB_FBPA_UC_DATA": 0x009A01C8,
    "LW_FBPA_PRI_STRIDE": 0x4000,
}

def GetRegAddress(baseregName, subpNum = 0, fbpaNum = 0):
    return galit1[baseregName] + galit1["LW_FBPA_PRI_STRIDE"]*fbpaNum + subpNum*0x4

def SetRegBitFields(regName, regbitfieldStringValueMap):
    regData = 0
    for regbitfieldStr, value in regbitfieldStringValueMap.items():
        if isinstance(value, str):
            value = galit1[regName + value]
        highLow = galit1[regName + regbitfieldStr]
        highLow = highLow.split(":")
        lowest = int(highLow[1])
        highest = int(highLow[0])
        # For [n:m] get a mask with (n - m + 1) 1's
        requiredOnes = (1 << (highest - lowest + 1)) - 1
        # save regdata and same will be used next time
        regData = regData | ((value & requiredOnes) << lowest);
    return regData

def PrintAteWriteToRegister(regAddress, regData, regName):
    regPrints.append("fbpa_j2u_write $prgmConfig $testConfig \"{}\" \"{}\" $pa $flush $read_ack; {}".format(hex(regAddress), hex(regData), regName))

def PrintAteReadFromRegister(regAddress, regName):
    regPrints.append("fbpa_j2u_read $prgmConfig $testConfig \"{}\" \"00000000\" $pa $flush \"FFFFFFFF\" {}".format(hex(regAddress), regName))

#entry is the patram entry. It is used during regwrite
def WritePatramEntry(fbpaSubpMask, entry, dqarray, eccarray, dbiarray):
    inputsNotFound = False
    if not dqarray:
        print("Error: dq array not found for patram entry {}".format(entry), file=sys.stderr)
        inputsNotFound = True
    if not eccarray:
        print("Error: ecc array not found for patram entry {}".format(entry), file=sys.stderr)
        inputsNotFound = True
    if not dbiarray:
        print("Error: dbi array not found for patram entry {}".format(entry), file=sys.stderr)
        inputsNotFound = True
    if (inputsNotFound):
        exit(1)

    subpBroadCastIndexes = []
    fbpaSubpMask = FillSubpIndexes(fbpaSubpMask, subpBroadCastIndexes)

    for subPos in subpBroadCastIndexes:
        for ii in range(8):
            regName = "LW_PFB_FBPA_0_TRAINING_PATTERN_PTR"
            regData = SetRegBitFields(regName, {"_DP" : 8 * entry + ii, "_ACT_ADR": "_ACT_ADR_ENABLED"})
            PrintAteWriteToRegister(GetRegAddress("LW_PFB_FBPA_TRAINING_PATTERN_PTR", subPos), regData, "# LW_PFB_FBPA_TRAINING_PATTERN_PTR({})".format(subPos))
            regName = "LW_PFB_FBPA_0_TRAINING_DP_CNTD"
            regData = SetRegBitFields(regName, {"_SEL" : "_SEL_CHAR_ENGINE", "_DBI": (dbiarray[0] >> (4 * ii)) & 0xF, "_EDC": (eccarray[0] >> (4 * ii)) & 0xF})
            PrintAteWriteToRegister(GetRegAddress("LW_PFB_FBPA_TRAINING_DP_CNTD", subPos), regData, "# LW_PFB_FBPA_TRAINING_DP_CNTD({})".format(subPos))
            regName = "LW_PFB_FBPA_0_TRAINING_DP"
            PrintAteWriteToRegister(GetRegAddress("LW_PFB_FBPA_TRAINING_DP", subPos), int(dqarray[ii]), "# LW_PFB_FBPA_TRAINING_DP({})".format(subPos))

    fbpaPos = 0
    while fbpaSubpMask:
        for subPos in range(2):
            if fbpaSubpMask & 1:
                for ii in range(8):
                    fbpahex = format(fbpaPos, 'X')
                    regName = "LW_PFB_FBPA_0_TRAINING_PATTERN_PTR"
                    regData = SetRegBitFields(regName, {"_DP" : 8 * entry + ii, "_ACT_ADR": "_ACT_ADR_ENABLED"})
                    PrintAteWriteToRegister(GetRegAddress(regName, subPos, fbpaPos), regData, "# LW_PFB_FBPA_{}_TRAINING_PATTERN_PTR({})".format(fbpaPos, subPos))
                    regName = "LW_PFB_FBPA_0_TRAINING_DP_CNTD"
                    regData = SetRegBitFields(regName, {"_SEL" : "_SEL_CHAR_ENGINE", "_DBI": (dbiarray[0] >> (4 * ii)) & 0xF, "_EDC": (eccarray[0] >> (4 * ii)) & 0xF})
                    PrintAteWriteToRegister(GetRegAddress(regName, subPos, fbpaPos), regData, "# LW_PFB_FBPA_{}_TRAINING_DP_CNTD({})".format(fbpaPos, subPos))
                    regName = "LW_PFB_FBPA_0_TRAINING_DP"
                    PrintAteWriteToRegister(GetRegAddress(regName, subPos, fbpaPos), int(dqarray[ii]), "# LW_PFB_FBPA_{}_TRAINING_DP({})".format(fbpaPos, subPos))
            fbpaSubpMask = fbpaSubpMask >> 1
        fbpaPos = fbpaPos + 1

def WriteInstructionWord(fbpaSubpMask, data, address = 0):
    inputsNotFound = False
    if not data:
        print("Error: data associated with instruction not found", file=sys.stderr)
        inputsNotFound = True
    if inputsNotFound:
        exit(1)

    subpBroadCastIndexes = []
    fbpaSubpMask = FillSubpIndexes(fbpaSubpMask, subpBroadCastIndexes)

    for subPos in subpBroadCastIndexes:
        for ii in range(NUM_OF_WRITES):
            regName = "LW_PFB_FBPA_0_UC_DATA"
            PrintAteWriteToRegister(GetRegAddress("LW_PFB_FBPA_UC_DATA", subPos), data[ii], "# LW_PFB_FBPA_UC_DATA({})".format(subPos))
        regName = "LW_PFB_FBPA_0_UC_CTL"
        regData = SetRegBitFields(regName, {"_LOAD_ENABLE": "_LOAD_ENABLE_TRUE", "_LOAD_ADDR" : address})
        PrintAteWriteToRegister(GetRegAddress("LW_PFB_FBPA_UC_CTL", subPos), regData, "# LW_PFB_FBPA_UC_CTL({})".format(subPos))

    fbpaPos = 0
    while fbpaSubpMask:
        for subPos in range(2):
            if fbpaSubpMask & 1:
                for ii in range(NUM_OF_WRITES):
                    regName = "LW_PFB_FBPA_0_UC_DATA"
                    PrintAteWriteToRegister(GetRegAddress(regName, subPos, fbpaPos), data[ii], "# LW_PFB_FBPA_{}_UC_DATA({})".format(fbpaPos, subPos))
                regName = "LW_PFB_FBPA_0_UC_CTL"
                regData = SetRegBitFields(regName, {"_LOAD_ENABLE": "_LOAD_ENABLE_TRUE", "_LOAD_ADDR" : address})
                PrintAteWriteToRegister(GetRegAddress(regName, subPos, fbpaPos), regData, "# LW_PFB_FBPA_{}_UC_CTL({})".format(fbpaPos, subPos))
            fbpaSubpMask >>= 1
        fbpaPos = fbpaPos + 1

def FillSubpIndexes(tmpFbpaSubpMask, subpBroadCastIndexes = []):
    # GA100 has 24 FBPAs. Each Fbpa has two sub-partitions
    # subp 0 & subp 1 of FBPA 0 is bits [0:1], of FBPA 1 is bits [2:3] and so on
    # Check if broadcast needed to subp 0 in all FBPAs
    if (tmpFbpaSubpMask & 0x555555555555 == 0x555555555555):
        subpBroadCastIndexes.append(0)
        # Unset the bits corresponding to subp0, so that the registers corresponding
        # to subp0 are not accessed again
        tmpFbpaSubpMask = tmpFbpaSubpMask & ~(0x555555555555)
    # Check if broadcast needed to subp 1 in all FBPAs
    if (tmpFbpaSubpMask & 0xAAAAAAAAAAAA == 0xAAAAAAAAAAAA):
        subpBroadCastIndexes.append(1)
        # Unset the bits corresponding to subp1
        tmpFbpaSubpMask = tmpFbpaSubpMask & ~(0xAAAAAAAAAAAA)
    return tmpFbpaSubpMask

regPrints = []
def MainProgram():

    inputfile = ''
    outputfile = ''
    try:
        opts, args = getopt.getopt(sys.argv[1:], "hi:o:")
    except getopt.GetoptError:
        print ('Options: -i <inputfile> -o <outputfile>')
        sys.exit(2)

    for opt, arg in opts:
        if opt == '-h':
            print ('Options: -i <inputfile> -o <outputfile>')
            sys.exit()
        elif opt == '-i':
            inputfile = arg
        elif opt == '-o':
            outputfile = arg

    with (open(inputfile) if inputfile else sys.stdin) as f:
        data = json.load(f)

    global regPrints
    regPrints = []

    # GH100 (GHLIT1) the instruction is now 256 bits (vs 192 bits for GA10x),
    # as such it takes 8 writes to UC_DATA register (vs 6 writes for GA10x)
    global NUM_OF_WRITES
    if data["litter"] == "GHLIT1":
        NUM_OF_WRITES = 8
    elif data["litter"] == "GALIT1":
        NUM_OF_WRITES = 6
    else:
        print("Error: Unknown litter name")
        exit(1)

    for thread in data["threads"]:
        fbpaSubpMask = 0
        if "mask" in thread:
            fbpaSubpMask = thread["mask"]
        else:
            fbpaSubpMask = galit1["max_fbpaSubpMask"]

        origFbpaSubpMask = fbpaSubpMask

        if not thread.get("code"):
            print("Error: 'code' entry not found in json file", file=sys.stderr)
            exit(1)

        for idx, patramEntry in enumerate(thread["patram"]):
            for entry in patramEntry["source"]:
                regPrints.append("# {}".format(entry))
            WritePatramEntry(fbpaSubpMask, idx, patramEntry.get("dq", None), patramEntry.get("ecc", None), patramEntry.get("dbi", None))

        address = 0
        for codeEntry in thread["code"]:
            for entry in codeEntry["source"]:
                regPrints.append("# {}".format(entry))
            WriteInstructionWord(fbpaSubpMask, codeEntry["data"], address)
            address = address + 1

        subpBroadCastIndexes = []
        fbpaSubpMask = FillSubpIndexes(fbpaSubpMask, subpBroadCastIndexes)

        for subPos in subpBroadCastIndexes:
            regName = "LW_PFB_FBPA_0_UC_CTL"
            regData = SetRegBitFields(regName, {"_ENABLE": "_ENABLE_TRUE", "_START_ADDR" : 0})
            PrintAteWriteToRegister(GetRegAddress("LW_PFB_FBPA_UC_CTL", subPos), regData, "# LW_PFB_FBPA_UC_CTL({})".format(subPos))

        # Run Program
        fbpaPos = 0
        tmpFbpaSubpMask = fbpaSubpMask
        while tmpFbpaSubpMask:
            for subPos in range(2):
                if tmpFbpaSubpMask & 1:
                    regName = "LW_PFB_FBPA_0_UC_CTL"
                    regData = SetRegBitFields(regName, {"_ENABLE": "_ENABLE_TRUE", "_START_ADDR" : 0})
                    PrintAteWriteToRegister(GetRegAddress(regName, subPos, fbpaPos), regData, "# LW_PFB_FBPA_{}_UC_CTL({})".format(fbpaPos, subPos))
                tmpFbpaSubpMask >>= 1
            fbpaPos = fbpaPos + 1

        # Wait till the program completes (Assumed maxCycles)
        regPrints.append("WaitRti({}); # Wait {} clock cycles".format(thread["maxCycles"], thread["maxCycles"]))

        writeCtrlErrBroadcast = False
        # Check for errors
        errorList = [0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7F, 0x84, 0x85, 0x86, 0x87]
        fbpaPos = 0
        while origFbpaSubpMask:
            for subPos in range(2):
                if origFbpaSubpMask & 1:
                    for selectErrorIndex in errorList:
                        regName = "LW_PFB_FBPA_0_TRAINING_DEBUG_CTRL"
                        regData = SetRegBitFields(regName, {"_SELECT": selectErrorIndex})
                        PrintAteWriteToRegister(GetRegAddress("LW_PFB_FBPA_0_TRAINING_DEBUG_CTRL", subPos, fbpaPos), regData, "# LW_PFB_FBPA_{}_TRAINING_DEBUG_CTRL({})".format(fbpaPos, subPos))
                        PrintAteReadFromRegister(GetRegAddress("LW_PFB_FBPA_0_TRAINING_DEBUG_DQ0", subPos, fbpaPos), "# LW_PFB_FBPA_{}_TRAINING_DEBUG_DQ0({})".format(fbpaPos, subPos))
                        PrintAteReadFromRegister(GetRegAddress("LW_PFB_FBPA_0_TRAINING_DEBUG_DQ1", subPos, fbpaPos), "# LW_PFB_FBPA_{}_TRAINING_DEBUG_DQ1({})".format(fbpaPos, subPos))
                        PrintAteReadFromRegister(GetRegAddress("LW_PFB_FBPA_0_TRAINING_DEBUG_DQ2", subPos, fbpaPos), "# LW_PFB_FBPA_{}_TRAINING_DEBUG_DQ2({})".format(fbpaPos, subPos))
                        PrintAteReadFromRegister(GetRegAddress("LW_PFB_FBPA_0_TRAINING_DEBUG_DQ3", subPos, fbpaPos), "# LW_PFB_FBPA_{}_TRAINING_DEBUG_DQ3({})".format(fbpaPos, subPos))
                origFbpaSubpMask >>= 1
            fbpaPos = fbpaPos + 1;

    sys.stdout.close = lambda: None

    with (open(outputfile, "w") if outputfile else sys.stdout) as f:
        for string in regPrints:
            print(string, file=f)

MainProgram()
