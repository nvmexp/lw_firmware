#_LWRM_COPYRIGHT_BEGIN_
#
#Copyright 2013-2017 by LWPU Corporation.  All rights reserved.  All
#information contained herein is proprietary and confidential to LWPU
#Corporation.  Any use, reproduction, or disclosure without the written
#permission of LWPU Corporation is prohibited.
#
#__LWRM_COPYRIGHT_END_
#
#@file:     coverity.sh
#@location: /uproc/utilities/coverity
#
#Created on: 15th November 2017
#This file is written in the support of "coverity.sh" having the instructions
#about how to use script file for coverity defect on local changes.

Quick start-
- Sync some P4 paths (see Setup step 5 below)
- Run "coverity.sh -h" to see options

Requirement- To commit defects from local changes on Developer Coverity server
             (txcovlind:8443) without using DVS.

Bug-ID- 200358190

Setup-
    1. SERVER- "txcovlind" (Developer Server)
    2. PROJECTS- ACR , SCRUBBER , SEC2
    3. STREAM- For adding any seperate stream for your local changes 
               we can ask "Sanket Rathod" or "Swapnil Maheshwari"
    4. SCRIPT- Please use "coverity.sh" script file for committing defects.
    5. Please sync below tools before running the coverity script : 
            a. //sw/tools/Coverity/ <Version of Coverity> /...
            b. //sw/tools/scripts/CoverityAutomation/SSLCert/ca-chain.crt

Instructions to use Script file:-
1. This script file is divided into 3 PHASES and need to change variables 
   according to our Local environment.

    a. PHASE-1 (Local Environment)
        COMMON- Provide your Perforce path.

    b. PHASE-2 (Coverity Setup)
        AGGRESSIVENESS_LEVEL- can be "high" or "medium" or "low"
                      For HS and LS microcodes "high" is preferred
                      It is used in the following command-
              "cov-analyze --all --aggressiveness-level $AGGRESSIVENESS_LEVEL "

        STREAM- Please make a seperate stream for you local project so that 
                can be easily bifurcated (by setting up filter in server).
                For Example-
                    "ACR_r384"
                    "ACR_chips_a"
                    "ACR_tegra_r384"
                    "ACR_tegra_chips_a"

        TARGET PROFILE- It is required to add comments to our defects,
                which we can see in "Description" in server.

                Please follow following Naming colwentions:-
                "PROJECT_BRANCH_CHIPNAME_PROFILE"

                For example- acr_r384_gp10x_sec2_load
                             acr_chipsa_gp10x_pmu_unload

        *CFG_PROFILE- Here, we need to provide profile name which will be build
                      specifically. 
                      If you want to build for all profiles, just comment it.

            For Example-
                ACRCFG_PROFILE="acr_sec2-gv100_load"

        BUILD PATH- Give the path of the project src, which we want to build
                    where we have makefile or buildscript.


    c. PHASE-3(Coverity Steps)
        Rest of the task will be taken care by script itself.
        Please expect snapshot-ID at last.

2. If everything is set properly, then it will commit defects in 5 steps 
(Config, Build, Analyze, HTML and COMMIT)
