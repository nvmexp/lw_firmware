#!/bin/sh
#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

# Usage: Launch with or without command-line parameters to specify incremental
# or clean/clobber build.
# Examples: ./pmu-release-parallel.sh to fire an incremental build
#           ./pmu-release-parallel.sh clobber to clobber and build

lwmake pmu build release -j16 PMUCFG_PROFILE= PMUCFG_PROFILES= SERIALIZE_PROFILES="false" "$@"

if [ $? -eq 0 ]
then
  echo "*                                                  "
  echo "***************************************************"
  echo "*                                                 *"
  echo "*  Success: All PMU builds finished successfully! *"
  echo "*                                                 *"
  echo "*   !!!  REMEMBER to include ALL generated files  *"
  echo "*   with your changelist IF AND ONLY IF using p4  *"
  echo "*   submit for submission;                        *"
  echo "*   DO NOT include generated files with your      *"
  echo "*   changelist for AS2 or vDVS submissions  !!!   *"
  echo "*                                                 *"
  echo "***************************************************"
  echo "*                                                  "
else
  echo "*                                                  "
  echo "***************************************************"
  echo "*                                                 *"
  echo "*  Error:  At least one PMU build has FAILED !!!  *"
  echo "*                                                 *"
  echo "***************************************************"
  echo "*                                                  "
fi
