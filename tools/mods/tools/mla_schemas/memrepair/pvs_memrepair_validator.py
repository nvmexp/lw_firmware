#!/usr/bin/elw python3

#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

#
# \brief PVS Memory Repair post-subtest validator.
#
# The PVS test pvs_mods_hbm runs a number of subtests. A pass/fail from MODS is
# not sufficient to determine if a subtest has passed. The actual values of the
# operations perform, the order of operations, and so on, are critical.
#
# This script is run after each subtest of pvs_mods_hbm. It then runs
# post-processing on the result of the MODS run to unlitmately determine the
# pass/fail status of pvs_mods_hbm.
#

import glob
import os
import subprocess
import sys

from collections import namedtuple

PVS_ERROR_CODE_PASS = 0 # Pass the test
PVS_ERROR_CODE_FAIL = 1 # Fail the test and show stdout prints
PVS_ERROR_CODE_INFO = 2 # Pass the test and show stdout prints

SCHEMA_ROOT_DIR = os.getcwd()

Config = namedtuple('Config', ['pvsMachine', 'pvsTest', 'pvsSubtest', 'mlaFilePath', 'mleFilePath'])

#
# \brief WAR issue with MLE data while running the MemRepair sequence.
#
# ref: https://jirasw.lwpu.com/browse/MODSAMPERE-445
#
def WAR_hbmvmemrepair(config: Config):
    if config.pvsSubtest == 'hbmvmemrepair':
        print('WAR(JIRA MODSAMPERE-445): hbmvmemrepair schema will not be checked')
        exit(PVS_ERROR_CODE_INFO)

#
# \brief Print the command line interface help message.
#
def PrintHelpText() -> None:
    print('Usage: {0} <pvs_machine> <pvs_test> <pvs_subtest> <mla_path> <mle_file>'.format(sys.argv[0]))
    print('Expects JSON schema file in "{0}" with name "*_<pvs_machine>_<pvs_subtest>*.json"'.format(SCHEMA_ROOT_DIR))

#
# \brief Create a configuration from the command line interface arguments.
# Performs CLI argument validation.
#
def CreateConfigFromCliArgs() -> Config:
    # Check number of CLI arguments
    EXPECTED_NUM_ARGS = 5
    if len(sys.argv) != EXPECTED_NUM_ARGS + 1:
        print('Error: Expected {0} CLI arguments, given {1}'.format(EXPECTED_NUM_ARGS, len(sys.argv) - 1))
        PrintHelpText()
        exit(PVS_ERROR_CODE_FAIL)

    validationError = False

    mlaFilePath = str(sys.argv[4])
    if not os.path.isfile(mlaFilePath):
        print('Error: Unknown file: ' + mlaFilePath)
        validationError = True

    mleFilePath = str(sys.argv[5])
    if not os.path.isfile(mleFilePath):
        print('Error: Unknown file: ' + mleFilePath)
        validationError = True

    if validationError:
        exit(PVS_ERROR_CODE_FAIL)

    config = Config(pvsMachine  = str(sys.argv[1]),
                    pvsTest     = str(sys.argv[2]),
                    pvsSubtest  = str(sys.argv[3]),
                    mlaFilePath = os.path.realpath(mlaFilePath),
                    mleFilePath = os.path.realpath(mleFilePath))

    # Check if supported PVS test
    if config.pvsTest != 'pvs_mods_hbm':
        print('Called with unsupported PVS test \"{0}\". \"pvs_mods_hbm\" is the only supported PVS test.'.format(config.pvsTest))
        exit(PVS_ERROR_CODE_FAIL)

    WAR_hbmvmemrepair(config)
        
    return config

#
# \brief Return the schema path. Terminates on error.
#
# \param pvsMachine PVS machine name.
# \param pvsSubtest PVS sub test name. The specific test being run.
#
def GetSchemaPath(pvsMachine: str, pvsSubtest: str) -> str:
    if os.path.exists(SCHEMA_ROOT_DIR):
        schemaPaths = glob.glob(os.path.join(SCHEMA_ROOT_DIR, '*_' + pvsMachine + '_' + pvsSubtest + '*.json'))

        numMatches = len(schemaPaths)
        if numMatches == 1:
            return schemaPaths[0]

        elif numMatches == 0:
            print('Error: No schema available for PVS subtest({0}), machine({1})'.format(pvsSubtest, pvsMachine))

        else:
            s = 'Error: Multiple schema file matches for PVS machine({0}), subtest({1}):\n'.format(pvsMachine, pvsSubtest)
            for match in schemaPaths:
                s += '\t' + match + '\n'
            print(s)

    else:
        print('Error: Unknown machine name: ' + pvsMachine)

    exit(PVS_ERROR_CODE_FAIL)

#
# \brief Main exelwtion point.
#
def Main():
    config = CreateConfigFromCliArgs()
    print('Running with configuration: ' + str(config))

    # Find JSON schema
    schemaPath = GetSchemaPath(config.pvsMachine, config.pvsSubtest)
    print('Using schema: ' + str(schemaPath))
    print() # line break

    # Run JSON schema validation on MLE file via MLA
    mlaRc = 0
    mleFilePath = config.mleFilePath
    mlaExecArgs = [config.mlaFilePath, '-schema', schemaPath, '-r', 'json2', mleFilePath]
    if sys.version_info >= (3, 5): # subprocess.run deprecates subprocess.call
        mlaRc = subprocess.run(mlaExecArgs).returncode
    else:
        mlaRc = subprocess.call(mlaExecArgs)

    if mlaRc != 0:
        # MLA failed
        debugFilePath = mleFilePath[:mleFilePath.rfind('.')] + '.debug'

        print() # line break
        print('MLA failed with error code: ' + str(mlaRc))
        print('Dumping MLA debug log: {0}'.format(debugFilePath))
        
        with open(debugFilePath, 'r') as f:
            print(f.read())
        exit(PVS_ERROR_CODE_FAIL)

    # Successfully passed the PVS test
    print('Passed schema validation')

if __name__ == '__main__':
    # Python version check
    if sys.version_info[0] < 3:
        print('Script requires Python3 or newer')

    Main()
    exit(PVS_ERROR_CODE_PASS)
