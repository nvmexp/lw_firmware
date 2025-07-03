#!/bin/tcsh
#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#
#
# Multiengine Elpg test
#
# This test uses the policy file "" to power up/down the engines as required
# elpg_deterministic.cfg file triggers trace fermi_a/ace/func.dx10_dx10sdk_basichlsl
# (similar to ace_short level)
#
perl sim.pl -chip gf119 -chipLib gflit1_fmodel -fmod -testname func.dx10_dx10sdk_basichlsl -modsOpt -gpubios 0 gf119_sim_gddr5.rom mdiag.js -chipargs -num_tpcs=1 -elpg_mask 0x3 -disable_elpg_on_init -elpg_idle_thresh gr 1000 -elpg_idle_thresh vid 1000 -elpg_ppu_thresh gr 1000 -elpg_ppu_thresh vid 1000 -chsw -rmmsg mcProcess -pmu_bootstrap_mode 0 -policy_file $MODS_PATH/gpu/tests/rm/policymanager/policyfiles/policy_multiengine.pcy -policy_dumpgild -idle_channels_retries 10000 -chipargs -chipPOR=gf119 -assert_upon_bad_pri_config -outputfilename func.dx10_dx10sdk_basichlsl -f $MODS_PATH/gpu/tests/rm/policymanager/config/elpg_deterministic.cfg -trepfile trep.txt
