#!/bin/tcsh
setelw PATH /home/utils/java/jdk1.6.0_17/bin:$PATH
source $HOME/.cshrc_lwstom
setelw P4ROOT $fmodUpdateData/p4/sw-rmtest-autoCheckin-fmodUpdate
setelw BUILD_ARCH amd64
setelw MODS_PATH ${P4ROOT}/sw/dev/gpu_drv/chips_a/diag/mods
setelw MODS_RUNSPACE $fmodUpdateData/modsRunspace/sw-rmtest-autoCheckin-fmodUpdate/chipsa_64
setelw P4CLIENT sw-rmtest-autoCheckin-fmodUpdate
setelw P4USER rmtest-autocheckin
setelw P4PASSWD berkeley_3D
#qsub -eo $HOME/fmodUpdateGlobalData/fmodUpdateScripts/qsub.log -q l_pri_interactive $fmodUpdateScripts/updAllFmodsChipsa.pl
#qsub -n 4 -R 'span[hosts=1]' -eo $HOME/fmodUpdateGlobalData/fmodUpdateScripts/qsub.log -q o_pri_interactive_cpu_4G $fmodUpdateScripts/updAllFmodsChipsa.pl
qsub -n 4 -R 'span[hosts=1]' -eo $HOME/fmodUpdateGlobalData/fmodUpdateScripts/qsub.log -q o_pri_interactive_cpu_4G $fmodUpdateScripts/updAllFmodsChipsa_sm2_64bit.pl
#qsub -n 4 -R 'span[hosts=1]' -eo $HOME/fmodUpdateGlobalData/fmodUpdateScripts/qsub.log -q o_pri_cpu_4G $fmodUpdateScripts/updAllFmodsChipsa.pl
