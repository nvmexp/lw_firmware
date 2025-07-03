#!/bin/tcsh

source $HOME/.cshrc_lwstom
setelw P4ROOT /home/scratch.vyadav_fermi-arch/src/t1
setelw MODS_PATH $P4ROOT/sw/dev/gpu_drv/bugfix_main/diag/mods
setelw MODS_RUNSPACE $P4ROOT/hw/lwdiag/mods/bugfix_main
cd $P4ROOT/sw;
setelw P4CLIENT VYADAV-SSW
setelw P4USER vyadav 
#rm -rf $HOME/log-old;
#mv $HOME/log $HOME/log-old/ 
#mkdir -p /home/vyadav/log
qsub -eo /home/vyadav/log/qsub.log /home/vyadav/bin/updallfmods_bfm.pl
