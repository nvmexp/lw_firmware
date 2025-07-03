# VFLwrve Test 
# This test fetches all the supported VF points for the requested clock domains
# and switches to them one by one.
# Note that we just specify master clock domains in this file and changing master
# clock domain frequency also make the corresponding change for slave clock domains
# based on the master-slave ratio or master-slave table relationship defined in
# VBIOS Clocks Programming Table.
#
# You can make pstate/nonpstate specific version of this file and test will automatically
# pick the correct CTP file based on whether pstate is enabled or not.
# Example: L0.pstate.ctp, L0.nonpstate.ctp
#
# You can also make chip/family/platform-specific versions of this file and correct CTP file
# will be used based on chip and platform you are running the test.
# Example: L0.pstate.gv100.ctp, L0.pstate.emu.gv100.ctp, L0.nonpstate.fmod.volta.ctp
#  
# If this test fails, please file a bug and/or contact sw-gpu-rm-clocks-dev.
# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

name=gpc
gpcclk.vflwrve = initpstate

name=disp
dispclk.vflwrve = initpstate

name=mclk
mclk.vflwrve = initpstate

name = vflwrveTest
test = initpstate:gpc, initpstate:disp, initpstate:mclk
rmapi = vfinject
order = random
#end = 20
tolerance = 0.05, 0.05
disable = thermalslowdown
