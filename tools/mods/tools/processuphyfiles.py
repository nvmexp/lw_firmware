#!/usr/bin/elw python3
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import json
import argparse
import os
import re
import sys


FILE_HEADER = '''/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*!! GENERATED USING //sw/mods/scripts/processuphyfiles.py !!*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/

/* LWIDIA_COPYRIGHT_BEGIN                                                 */
/*                                                                        */
/* Copyright 2021 by LWPU Corporation.  All rights reserved.  All       */
/* information contained herein is proprietary and confidential to LWPU */
/* Corporation.  Any use, reproduction, or disclosure without the written */
/* permission of LWPU Corporation is prohibited.                        */
/*                                                                        */
/* LWIDIA_COPYRIGHT_END                                                   */

'''


MAKE_HEADER = '''#########################################################
# GENERATED USING //sw/mods/scripts/processuphyfiles.py #
#########################################################

#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

'''

######################################################################
# Dump a UPHY header file for MODS to consume
def DumpHeader(registerList, regType, outputHeaderFile):
    with open(outputHeaderFile, "w+") as f:
        f.write(FILE_HEADER)
        f.write("#pragma once\n\n")
        for reg in registerList:
            f.write('#define LW_{0}_{1:60s} {2}\n'.format(regType, reg.Name, reg.Address))
            for field in reg.Fields:
                f.write('#define LW_{0}_{1:60s} {2}:{3}\n'.format(regType,
                                                                  reg.Name + "_" + field.Name,
                                                                  field.EndBit,
                                                                  field.StartBit))

######################################################################
# Process a register dump definition file which is simply a file with
# one base-10 register address per line into a CPP vector
def ProcessRegListFile(outputFile, reglist):
    outputFile.write(' =\n    {\n')
    firstAddr = True
    lineAddrs = []
    for addr in reglist:
        lineAddrs += [str(addr)]
        if len(lineAddrs) == 8:
            isLastAddr = (addr == reglist[-1])
            outputFile.write('        ' + ', '.join(lineAddrs) + (',' if not isLastAddr else '') + '\n')
            lineAddrs = []
    if len(lineAddrs):
        outputFile.write('        ' + ', '.join(lineAddrs) + '\n')
    outputFile.write('    }')


######################################################################
def GetUphyVersionAndType(uphyDict):
    retData = {}
    if 'version_major' in uphyDict and 'version_minor' in uphyDict:
        retData['Version'] = str(uphyDict['version_major']) + str(uphyDict['version_minor'])
    elif 'uphy_version' in uphyDict:
        retData['Version'] = str(uphyDict['uphy_version']).replace('.', '')
    if 'name' in uphyDict:
        if uphyDict['name'] == 'LW_UPHY_CLM':
            retData['Block'] = 'CLN'
        elif uphyDict['name'] == 'LW_UPHY_DLM':
            retData['Block'] = 'DLN'
        elif uphyDict['name'] == 'LW_GPHY_C2C':
            retData['Block'] = 'C2C'
    return retData

######################################################################
def WriteLwLinkSpeedArray(speeds, bkvFile):
    bkvFile.write('            {\n')
    comma = ''
    for speed in speeds:
        if speed == 'ALL':
            bkvFile.write('                ' + comma + 'static_cast<UINT32>(LwLink::SystemLineRate::GBPS_UNKNOWN)\n')
        else:
            bkvFile.write('                ' + comma + 'static_cast<UINT32>(LwLink::SystemLineRate::' + speed + ')\n')
        comma = ','
    bkvFile.write('            },\n')

######################################################################
def WritePcieSpeedArray(speeds, bkvFile):
    bkvFile.write('            {\n')
    comma = ''
    for speed in speeds:
        if speed == 'ALL':
            bkvFile.write('                ' + comma + 'static_cast<UINT32>(Pci::PcieLinkSpeed::SpeedUnknown)\n')
        else:
            bkvFile.write('                ' + comma + 'static_cast<UINT32>(Pci::PcieLinkSpeed::' + speed + ')\n')
        comma = ','
    bkvFile.write('            },\n')

######################################################################
def WriteBkvData(bkv_data, bkvFile):
    bkvFile.write('                {}f,\n'.format(bkv_data['protocol_version']))
    bkvFile.write('                { ')
    comma = ''
    for bkv_version in bkv_data['bkv_versions']:
        bkvFile.write('{}{}f'.format(comma, bkv_version))
        comma = ', '
    bkvFile.write(' },\n')
    comma = ' '
    bkvFile.write('                {\n')
    for register in bkv_data['registers']:
        bkvFile.write('                    {}{{ 0x{:04X}, 0x{:04X}, 0x{:04X}, {} }}\n'.format(comma,
                                                                                              register['addr'],
                                                                                              register['wdata'],
                                                                                              register['wmask'],
                                                                                              register['block']))
        comma = ','
    bkvFile.write('                }\n')


######################################################################
# Process the BKV file into a C++ file containing the necessary structures
def GenerateBkvHeaderFile(inputfile, outputfile):
    uphydata = []
    with open(inputfile, 'r') as uphyfile:
        try:
            # Parse the string into a python dictionary.
            uphydata = json.load(uphyfile)
        except Exception as ex:
            print('Error parsing ' + inputfile + ': ' + ex)
            return 1
    uphyinfo = GetUphyVersionAndType(uphydata)
    if 'Version' not in uphyinfo:
        print('UPHY version not found')
        return 1

    with open(outputfile, "w+") as bkvFile:
        bkvFile.write(FILE_HEADER)
        uphyName = 'UphyV' + uphyinfo['Version']

        missing_protocols = [ 'LWHS', 'PCIE' ]
        for protocol,protocol_data in uphydata['protocols'].items():
            varName = ''
            if protocol == 'LWHS':
                varName = 'g_LwLink' + uphyName + 'BkvData'
            elif protocol == 'PCIE':
                varName = 'g_Pcie' + uphyName + 'BkvData'
            else:
                print('Unknown UPHY protocol ' + protocol)
                return 1
            missing_protocols.remove(protocol)

            bkvFile.write('const BkvTable ' + varName + ' =\n')
            bkvFile.write('{\n')

            comma = ''
            for bkv_entry in protocol_data:
                bkvFile.write('    {}{{\n'.format(comma))
                bkvFile.write('        "' + bkv_entry['name'] + '",\n')

                if bkv_entry['type'] == 'DLM':
                    bkvFile.write('        UphyIf::RegBlock::DLN,\n')
                elif bkv_entry['type'] == 'CLM':
                    bkvFile.write('        UphyIf::RegBlock::CLN,\n')
                else:
                    print('Unknown UPHY type ' + bkv_entry['type'])
                    return 1

                if protocol == 'LWHS':
                    WriteLwLinkSpeedArray(bkv_entry['speeds'], bkvFile)
                elif protocol == 'PCIE':
                    WritePcieSpeedArray(bkv_entry['speeds'], bkvFile)

                bkvFile.write('        {\n')
                bkv_data_comma = ' '
                for bkv_data in bkv_entry['bkv_data']:
                    bkvFile.write('           {}{{\n'.format(bkv_data_comma))
                    WriteBkvData(bkv_data, bkvFile)
                    bkvFile.write('            }\n')
                    bkv_data_comma = ','
                bkvFile.write('        }\n')
                bkvFile.write('    }\n')
                comma = ','

            bkvFile.write('};\n')
        for protocol in missing_protocols:
            varName = ''
            if protocol == 'LWHS':
                varName = 'g_LwLink' + uphyName + 'BkvData'
            elif protocol == 'PCIE':
                varName = 'g_Pcie' + uphyName + 'BkvData'
            bkvFile.write('    const BkvTable ' + varName + ';\n')

######################################################################
# Create the header file for bkv data access.  The structures defined here
# must stay in sync with the structures generated by GenerateBkvDataFile
def GenerateUphyBkvHeader(outputfile):
    with open(outputfile, "w+") as bkvHeader:
        bkvHeader.write(FILE_HEADER)
        bkvHeader.write('#pragma once\n')
        bkvHeader.write('\n')
        bkvHeader.write('#include "core/include/types.h"\n')
        bkvHeader.write('#include "device/interface/uphyif.h"\n')
        bkvHeader.write('\n')
        bkvHeader.write('#include <vector>\n')
        bkvHeader.write('#include <string>\n')
        bkvHeader.write('#include <set>\n')
        bkvHeader.write('\n')
        bkvHeader.write('namespace UphyBkvData\n')
        bkvHeader.write('{\n')
        bkvHeader.write('    struct BkvRegister\n')
        bkvHeader.write('    {\n')
        bkvHeader.write('        UINT16 addr;\n')
        bkvHeader.write('        UINT16 data;\n')
        bkvHeader.write('        UINT16 mask;\n')
        bkvHeader.write('        UINT16 block;\n')
        bkvHeader.write('    };\n')
        bkvHeader.write('    struct BkvSettings\n')
        bkvHeader.write('    {\n')
        bkvHeader.write('        FLOAT32             protocolVersion;\n')
        bkvHeader.write('        set<FLOAT32>        bkvVersions;\n')
        bkvHeader.write('        vector<BkvRegister> bkvRegisters;\n')
        bkvHeader.write('    };\n')
        bkvHeader.write('    struct BkvEntry\n')
        bkvHeader.write('    {\n')
        bkvHeader.write('        string              bkvName;\n')
        bkvHeader.write('        UphyIf::RegBlock    regBlock;\n')
        bkvHeader.write('        vector<UINT32>      speeds;\n')
        bkvHeader.write('        vector<BkvSettings> bkvEntries;\n')
        bkvHeader.write('    };\n')
        bkvHeader.write('    typedef vector<BkvEntry> BkvTable;\n')
        bkvHeader.write('\n')
        bkvHeader.write('    const BkvTable & GetBkvTable(UphyIf::Version version, UphyIf::Interface interface);\n')
        bkvHeader.write('}\n')

######################################################################
def GenerateUphyBkvFile(uphydirectory, outputfile):
    bkvVersions = []
    bkvfiles = []
    if os.path.isdir(uphydirectory):
        bkvfiles = [f for f in os.listdir(uphydirectory) if os.path.isfile(os.path.join(uphydirectory, f)) and \
                                                            os.path.splitext(f)[1] == '.json']
        for bkvFile in bkvfiles:
            with open(os.path.join(uphydirectory, bkvFile), 'r') as bkvDataFile:
                try:
                    # Parse the string into a python array.
                    bkvdata = json.load(bkvDataFile)
                except Exception as ex:
                    print('Error parsing ' + inputfile + ': ' + ex)
                    return 1
                bkvVersions.append(GetUphyVersionAndType(bkvdata)['Version'])

    with open(outputfile, "w+") as bkvFile:
        bkvFile.write(FILE_HEADER)
        bkvFile.write('\n')
        bkvFile.write('#include "uphy_bkv_data.h"\n')
        bkvFile.write('#include "core/include/massert.h"\n')
        bkvFile.write('#include "core/include/pci.h"\n')
        bkvFile.write('#include "core/include/types.h"\n')
        bkvFile.write('#include "device/interface/lwlink.h"\n')
        bkvFile.write('\n')
        bkvFile.write('namespace UphyBkvData\n')
        bkvFile.write('{\n')
        bkvFile.write('    BkvTable emptyBkvTable;\n\n')
        for bkvfile in bkvfiles:
            bkvFile.write('    #include "{}.h"\n'.format(os.path.splitext(bkvfile)[0]))
        bkvFile.write('}\n')        
        bkvFile.write('#ifdef _WIN32\n')         
        bkvFile.write('#   pragma warning(push)\n')         
        bkvFile.write('#   pragma warning(disable : 4065)\n')         
        bkvFile.write('#endif\n')         
        bkvFile.write('const UphyBkvData::BkvTable & UphyBkvData::GetBkvTable(UphyIf::Version version, UphyIf::Interface interface)\n')
        bkvFile.write('{\n')
        bkvFile.write('    MASSERT((interface == UphyIf::Interface::Pcie) || (interface == UphyIf::Interface::LwLink));\n')
        bkvFile.write('\n')
        bkvFile.write('    switch (version)\n')
        bkvFile.write('    {\n')
        for bkvVersion in bkvVersions:
            bkvFile.write('        case UphyIf::Version::V{}:\n'.format(bkvVersion))
            bkvFile.write('            return (interface == UphyIf::Interface::Pcie) ? g_PcieUphyV{}BkvData :\n'.format(bkvVersion, bkvVersion))
            bkvFile.write('                                                            g_LwLinkUphyV{}BkvData;\n'.format(bkvVersion))
        bkvFile.write('        default:\n')
        bkvFile.write('            MASSERT(!"Unknown UPHY version");\n')
        bkvFile.write('            break;\n')
        bkvFile.write('    }\n')
        bkvFile.write('    return emptyBkvTable;\n')
        bkvFile.write('}\n')
        bkvFile.write('#ifdef _WIN32\n')         
        bkvFile.write('#   pragma warning(pop)\n')         
        bkvFile.write('#endif\n')         


######################################################################
# Process both a long and a short register definition file into a CPP
# file with vectors containing the addresses to dump
def GenerateRegListFile(inputfile, outputfile):
    uphydata = []
    with open(inputfile, 'r') as uphyfile:
        try:
            # Parse the string into a python array.
            uphydata = json.load(uphyfile)
        except Exception as ex:
            print('Error parsing ' + inputfile + ': ' + ex)
            return 1
    uphyinfo = GetUphyVersionAndType(uphydata)
    if 'Version' not in uphyinfo or 'Block' not in uphyinfo:
        print('UPHY version or block not found')
        return 1
    with open(outputfile, "w+") as reglistfile:
        reglistfile.write(FILE_HEADER)
        reglistfile.write("#include \"core/include/types.h\"\n")
        reglistfile.write("#include \"gpu/uphy/uphyregloglists.h\"\n")
        reglistfile.write("\n")
        reglistfile.write("namespace UphyRegLogLists\n")
        reglistfile.write("{\n")
        if uphyinfo['Block'] == 'C2C':
            dumpVarName = 'C2CGphyV' + uphyinfo['Version']
        else:
            dumpVarName = 'UphyV' + uphyinfo['Version'] + uphyinfo['Block']

        # Format for the short indicator is one of the following:
        #    'dumps' : 'short'
        #    'dumps' : [ ..., 'short', ... ]
        shortreglist = [ reg['addr'] for reg in uphydata['registers'] if 'dumps' in reg and
            ((type(reg['dumps']) is list and
              ('short' in reg['dumps'] or'rx_short' in reg['dumps'] or 'tx_short' in reg['dumps'])) or
             (type(reg['dumps']) is str and 
              (reg['dumps'] == 'short' or reg['dumps'] == 'rx_short' or reg['dumps'] == 'tx_short')))]

        shortreglist.sort()
        if len(shortreglist):
            reglistfile.write('    static const UINT16 s_' + dumpVarName + 'ShortDumpInitData[]')
            ProcessRegListFile(reglistfile, shortreglist)
            reglistfile.write(";\n")
            reglistfile.write('    const UINT16* const g_' + dumpVarName + 'ShortDump = s_' +
                           dumpVarName + 'ShortDumpInitData;\n')
            reglistfile.write('    const size_t g_' + dumpVarName + 'ShortDumpSize = sizeof(s_' +
                           dumpVarName + 'ShortDumpInitData) / sizeof(UINT16);\n')
        else:
            reglistfile.write('    const UINT16* const g_' + dumpVarName + 'ShortDump = nullptr;\n')
            reglistfile.write('    const size_t g_' + dumpVarName + 'ShortDumpSize = 0;\n')
        reglistfile.write("\n")
        reglistfile.write('    static const UINT16 s_' + dumpVarName + 'LongDumpInitData[]')
        longreglist = [ reg['addr'] for reg in uphydata['registers'] ]
        longreglist.sort()
        ProcessRegListFile(reglistfile, longreglist)
        reglistfile.write(";\n")
        reglistfile.write('    const UINT16* const g_' + dumpVarName + 'LongDump = s_' +
                       dumpVarName + 'LongDumpInitData;\n')
        reglistfile.write('    const size_t g_' + dumpVarName + 'LongDumpSize = sizeof(s_' +
                       dumpVarName + 'LongDumpInitData) / sizeof(UINT16);\n')
        reglistfile.write("}\n")
    return 0

######################################################################
def FilterJsonFiles(uphydirectory):
    json_files = []
    if os.path.isdir(uphydirectory):
        for f in os.listdir(uphydirectory):
            if os.path.isfile(os.path.join(uphydirectory, f)):
                _,uphyext = os.path.splitext(f)
                if uphyext == '.json':
                        json_files.append(f)
    return json_files

######################################################################
def CreateCppRule(outputFile, inputFile, genExt):
    uphyname,_ = os.path.splitext(inputFile)
    outputFile.write('$(gen_cpp_dir)/' + os.path.basename(uphyname) + genExt +
                     ': $(UPHY_FILES_DIR_UNIX)/' + uphyname +
                     '.json $(MODS_DIR_UNIX)/tools/processuphyfiles.py ' +
                     '$(gen_cpp_dir)/dummy.txt $(gen_cpp_dir)/uphy_makesrc.inc\n')
    outputFile.write('\t@$(ECHO) Generating $(notdir $@)\n')
    outputFile.write('\t$(Q)$(PYTHON3) $(MODS_DIR_UNIX)/tools/processuphyfiles.py --inputfile $< $@\n')
    outputFile.write('\n')

######################################################################
# Add the cpp files to a makesrc.inc file that can be included
def GenerateMakesrc(uphydirectory, outputMakesrcFile):
    # The UPHY directory is actually optional.  For instance almost all users of sim MODS do not
    # need the uphy information.  The only consequence of not having it is that UPHY logs may not
    # be generated unless a custom register list is provided on the command line or the MFG MODS
    # BKV test may not be run.
    #
    # uphy_bkv_data.h, uphy_bkv_data.cpp, uphy_list.h, and uphy_makesrc.inc are still necessary
    # in order to continue to compile MODS
    uphyfiles = FilterJsonFiles(uphydirectory)
    bkv_files = FilterJsonFiles(os.path.join(uphydirectory, 'bkv'))
    c2c_files = FilterJsonFiles(os.path.join(uphydirectory, 'c2c'))

    with open(outputMakesrcFile, "w+") as makesrcFile:
        makesrcFile.write(MAKE_HEADER)
        for uphyfile in uphyfiles + c2c_files:
            uphyname,_ = os.path.splitext(uphyfile)
            makesrcFile.write('gen_uphy_files += ' + uphyname + '.cpp\n')
        makesrcFile.write('\n')
        for uphyfile in uphyfiles:
            CreateCppRule(makesrcFile, uphyfile, '.cpp')
        for c2c_file in c2c_files:
            CreateCppRule(makesrcFile, os.path.join('c2c', c2c_file), '.cpp')
        for bkv_file in bkv_files:
            CreateCppRule(makesrcFile, os.path.join('bkv', bkv_file), '.h')
        makesrcFile.write('$(gen_cpp_dir)/uphy_list.h: $(MODS_DIR_UNIX)/tools/processuphyfiles.py $(gen_cpp_dir)/dummy.txt\n')
        makesrcFile.write('\t@$(ECHO) Creating uphy_list.h\n')
        makesrcFile.write('\t$(Q)$(PYTHON3) $(MODS_DIR_UNIX)/tools/processuphyfiles.py --uphydirectory $(UPHY_FILES_DIR) $@\n')
        makesrcFile.write('\n')
        makesrcFile.write('$(gen_cpp_dir)/uphy_bkv_data.h: $(MODS_DIR_UNIX)/tools/processuphyfiles.py $(gen_cpp_dir)/dummy.txt\n')
        makesrcFile.write('\t@$(ECHO) Creating uphy_bkv_data.h\n')
        makesrcFile.write('\t$(Q)$(PYTHON3) $(MODS_DIR_UNIX)/tools/processuphyfiles.py --uphydirectory $(UPHY_FILES_DIR)/bkv $@\n')
        makesrcFile.write('\n')
        makesrcFile.write('$(gen_cpp_dir)/uphy_bkv_data.cpp: $(MODS_DIR_UNIX)/tools/processuphyfiles.py $(gen_cpp_dir)/dummy.txt\n')
        makesrcFile.write('\t@$(ECHO) Creating uphy_bkv_data.cpp\n')
        makesrcFile.write('\t$(Q)$(PYTHON3) $(MODS_DIR_UNIX)/tools/processuphyfiles.py --uphydirectory $(UPHY_FILES_DIR)/bkv $@\n')
        makesrcFile.write('\n')
        makesrcFile.write('.PHONY: uphy\n')
        makesrcFile.write('uphy: $(gen_cpp_dir)/uphy_makesrc.inc $(gen_cpp_dir)/uphy_list.h\n')
        makesrcFile.write('uphy: $(gen_cpp_dir)/uphy_bkv_data.h $(gen_cpp_dir)/uphy_bkv_data.cpp\n')
        makesrcFile.write('\n')
        for bkv_file in bkv_files:
            bkv_name,_ = os.path.splitext(bkv_file)
            makesrcFile.write('gen_h_files += $(gen_cpp_dir)/' + bkv_name + '.h\n')
        makesrcFile.write('gen_h_files += $(gen_cpp_dir)/uphy_bkv_data.h\n')
        makesrcFile.write('gen_h_files += $(gen_cpp_dir)/uphy_list.h\n')
        makesrcFile.write('gen_cpp_files += $(gen_cpp_dir)/uphy_bkv_data.cpp\n')
        makesrcFile.write('\n')
        makesrcFile.write('cpp_files += $(gen_cpp_dir)/uphy_bkv_data.cpp\n')
        if len(uphyfiles):
            makesrcFile.write('gen_cpp_files   += $(addprefix $(gen_cpp_dir)/,$(gen_uphy_files))\n')
            makesrcFile.write('cpp_files       += $(addprefix $(gen_cpp_dir)/,$(gen_uphy_files))\n')
        makesrcFile.write('\n')
        for uphyfile in uphyfiles:
            makesrcFile.write('internal_release_files += $(UPHY_FILES_DIR)/' + uphyfile + '\n')
    return 0


######################################################################
# Add a uphy version to the supported version list
def GenerateUphyList(uphydirectory, outputListFile):
    # The UPHY directory is actually optional.  For instance almost alll users of sim MODS do not
    # need the uphy information.  The only consequence of not having it is that UPHY logs may not
    # be generated unless a custom register list is provided on the command line.
    # uphy_list.h an uphy_makesrc.inc are still necessary in order to continue to compile MODS
    uphyfiles = []
    if os.path.isdir(uphydirectory):
        uphyfiles = [f for f in os.listdir(uphydirectory) if os.path.isfile(os.path.join(uphydirectory, f))]
    c2c_directory = os.path.join(uphydirectory, 'c2c')
    if os.path.isdir(c2c_directory):
        uphyfiles += [os.path.join('c2c', f) for f in os.listdir(c2c_directory) if os.path.isfile(os.path.join(c2c_directory, f))]
    with open(outputListFile, 'w+') as outputFile:
        outputFile.write(FILE_HEADER)
        for uphyfilename in uphyfiles:
            _,uphyext = os.path.splitext(uphyfilename)
            if uphyext == '.json':
                uphydata = []
                fulluphyfilename = os.path.join(uphydirectory, uphyfilename)
                with open(fulluphyfilename, 'r') as uphyfile:
                    try:
                        # Parse the string into a python array.
                        uphydata = json.load(uphyfile)
                    except Exception as ex:
                        print('Error parsing ' + fulluphyfilename + ': ' + ex)
                        return 1
                uphyinfo = GetUphyVersionAndType(uphydata)
                if 'Version' not in uphyinfo or 'Block' not in uphyinfo:
                    print('UPHY version or block not found')
                    return 1
                if uphyinfo['Block'] == 'C2C':
                    outputFile.write('DEFINE_C2C_GPHY(V' + uphyinfo['Version'] + ', UphyIf::Version::V' + uphyinfo['Version'] + ')\n')
                else:
                    outputFile.write('DEFINE_UPHY(V' + uphyinfo['Version'] + ', ' +
                                     uphyinfo['Block'] + ', UphyIf::Version::V' +
                                     uphyinfo['Version'] + ')\n')
    return 0

######################################################################
def main(argv):
    """ Main program """
    # Parse the command-line options
    #
    argparser = argparse.ArgumentParser(formatter_class= argparse.ArgumentDefaultsHelpFormatter,
                    description='''processuphyfiles.py is used to process files from the UPHY
                                   hardware tree to be consumed by MODS and MLA for UPHY data
                                   analysis.''')

    argparser.add_argument('--uphydirectory', type=str, required=False,
                           help='Directory where the UPHY files reside')
    argparser.add_argument('--inputfile', type=str, required=False,
                           help='Input filename when generating individual UPHY output files')
    argparser.add_argument('outputfile', type=str,
                           help='File to output')
    args = argparser.parse_args()

    if args.uphydirectory and not os.path.isdir(args.uphydirectory):
        print('UPHY directory ' + args.uphydirectory +
              ' does not exist, data will be generated')

    if args.inputfile and not os.path.isfile(args.inputfile):
        print('Input file ' + args.inputfile + ' does not exist')
        return 1

    fname,fext = os.path.splitext(args.outputfile)

    if fext == '.cpp':
        if os.path.basename(args.outputfile) == 'uphy_bkv_data.cpp':
            return GenerateUphyBkvFile(args.uphydirectory, args.outputfile)
        if not os.path.isfile(args.inputfile):
            print('Input file missing for log list generation')
            return 1
        GenerateRegListFile(args.inputfile, args.outputfile)
    else:
        if os.path.basename(args.outputfile) == 'uphy_makesrc.inc':
            return GenerateMakesrc(args.uphydirectory, args.outputfile)
        elif os.path.basename(args.outputfile) == 'uphy_list.h':
            return GenerateUphyList(args.uphydirectory, args.outputfile)
        elif os.path.basename(args.outputfile) == 'uphy_bkv_data.h':
            return GenerateUphyBkvHeader(args.outputfile)
        elif args.inputfile and os.path.splitext(os.path.splitext(args.inputfile)[0])[1] == '.bkv':
            return GenerateBkvHeaderFile(args.inputfile, args.outputfile)
        else:
            print('Do not know how to generate ' + args.outputfile)
            return 1

######################################################################
if __name__ == "__main__":
    sys.exit(main(sys.argv))
