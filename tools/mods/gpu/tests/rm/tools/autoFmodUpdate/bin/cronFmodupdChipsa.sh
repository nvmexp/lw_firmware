#!/bin/tcsh
setelw PATH /home/utils/gdb-7.9.1_gcc-4.2.2/bin:/home/utils/java/jdk1.6.0_17/bin:$PATH
source $HOME/.cshrc_lwstom
setelw P4ROOT $fmodUpdateData/p4/sw-rmtest-autoCheckin-fmodUpdate
setelw MODS_PATH ${P4ROOT}/sw/dev/gpu_drv/chips_a/diag/mods
setelw MODS_RUNSPACE $fmodUpdateData/modsRunspace/sw-rmtest-autoCheckin-fmodUpdate/chipsa
setelw P4CLIENT sw-rmtest-autoCheckin-fmodUpdate
setelw P4USER rmtest-autocheckin


#qsub -n 4 -R 'span[hosts=1]' -eo $HOME/fmodUpdateGlobalData/fmodUpdateScripts/qsub.log -q o_pri_interactive_cpu_4G $fmodUpdateScripts/updAllFmodsChipsa.pl

if ( $#argv == 0 ) then
    qsub -n 4 -R 'span[hosts=1]' -eo $HOME/fmodUpdateGlobalData/fmodUpdateScripts/qsub.log --projectMode direct -P sw_inf_sw_rm -q o_cpu_32G $MODS_PATH/gpu/tests/rm/tools/autoFmodUpdate/bin/updallfmods_chipsa_sm2_verif.pl all
  else
    qsub -n 4 -R 'span[hosts=1]' -eo $HOME/fmodUpdateGlobalData/fmodUpdateScripts/qsub.log --projectMode direct -P sw_inf_sw_rm -q o_cpu_32G $MODS_PATH/gpu/tests/rm/tools/autoFmodUpdate/bin/updallfmods_chipsa_sm2_verif.pl $argv[1]
endif


