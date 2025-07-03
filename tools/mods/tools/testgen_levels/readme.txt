To run a test gen level on a fmodel please follow these steps (replace paths and p4 settings with your own).
The levels are using testgen 3.0 syntax so they require lwgpu tree or newer.

* Sync these directories:

//hw/lwgpu/*
//hw/lwgpu/bin/...
//hw/lwgpu/diag/...
-//hw/lwgpu/diag/perf/...
//hw/lwgpu/etc/...
//hw/lwgpu/make/...
//hw/lwgpu/security/...
//hw/lwgpu/tools/trap_handlers/...
//hw/lwgpu/uproc/...

* Get current fmodel (can be done once in a while):

export P4PORT=p4hw:2001
export P4CLIENT=kamild-linuxg
cd /home/scratch.kamild_gpu/srcg1/hw/lwgpu/diag/testgen
/home/lw/utils/fetch/fetch-0.2/bin/get_fmodel -chip ga102 -release -latest -arch Linux_x86_64

* Run a level:

export P4PORT=p4hw:2001
export P4CLIENT=kamild-linuxg
cd /home/scratch.kamild_gpu/srcg1/hw/lwgpu/diag/testgen
tgen.pl -target FMODEL -chip ga102 -release -nosandbox -elwvar SC_COPYRIGHT_MESSAGE=DISABLE -modsRunspace /home/scratch.kamild_sw/regress/sim/chips_a/27873574.0 -processArgs /home/scratch.kamild_sw/src3/sw/dev/gpu_drv/chips_a/diag/mods/tools/testgen_levels/process_args -macroFile /home/scratch.kamild_sw/src3/sw/dev/gpu_drv/chips_a/diag/mods/tools/testgen_levels/default_macros -level /home/scratch.kamild_sw/src3/sw/dev/gpu_drv/chips_a/diag/mods/tools/testgen_levels/glrandom_full -maxFileSize 200000 -queue o_cpu_16G


* To test the command line without running mods use "-allButRun"
* For info on more tgen arguments run (e.g. "-only ...", "-queue ..."):

tgen.pl -help


*** AMODEL ***

* To get current amodel:
export P4PORT=p4hw:2001
export P4CLIENT=kamild-linuxg
cd /home/scratch.kamild_gpu/srcg1/hw/lwgpu/diag/testgen
../../bin/get_good_changelist
../../../../mysynchw //...@?????????
../../bin/get_amodel.pl -family ampere

* To run one frame (remove "-only" for more):
tgen.pl -target AMODEL -chip ga100 -release -modsRunspace /home/scratch.kamild_sw/src2/runspace/chipalinda -processArgs /home/scratch.kamild_sw/src2/sw/dev/gpu_drv/chips_a/diag/mods/tools/testgen_levels/process_args -level /home/scratch.kamild_sw/src2/sw/dev/gpu_drv/chips_a/diag/mods/tools/testgen_levels/glrandom_full -macroFile /home/scratch.kamild_sw/src2/sw/dev/gpu_drv/chips_a/diag/mods/tools/testgen_levels/default_macros -queue o_cpu_8G_24H -maxFileSize 200000 -only GlrA8R8G8B8_isolate_04
