# VFLwrve Test 
# This test fetches all the supported VF points for the requested clock domains
# and switches to them one by one.
# Note that we just specify master clock domains in this file and changing master
# clock domain frequency also make the corresponding change for slave clock domains
# based on the master-slave ratio or master-slave table relationship defined in
# VBIOS Clocks Programming Table.
# Note that we specify superset of pstates and test will automatically skip pstates
# which are not present for a particular chip/sku.
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

name=gpcP0
gpcclk.vflwrve = vbiosp0

name=gpcP1
gpcclk.vflwrve = vbiosp1

name=gpcP2
gpcclk.vflwrve = vbiosp2

name=gpcP5
gpcclk.vflwrve = vbiosp5

name=gpcP8
gpcclk.vflwrve = vbiosp8

name=dispP0
dispclk.vflwrve = vbiosp0

name=dispP1
dispclk.vflwrve = vbiosp1

name=dispP2
dispclk.vflwrve = vbiosp2

name=dispP5
dispclk.vflwrve = vbiosp5

name=dispP8
dispclk.vflwrve = vbiosp8

name=mclkP0
mclk.vflwrve = vbiosp0

name=mclkP1
mclk.vflwrve = vbiosp1

name=mclkP2
mclk.vflwrve = vbiosp2

name=mclkP5
mclk.vflwrve = vbiosp5

name=mclkP8
mclk.vflwrve = vbiosp8

name = vflwrveTest
test = vbiosp0:gpcP0, vbiosp1:gpcP1, vbiosp2:gpcP2, vbiosP5:gpcP5, vbiosp8:gpcP8
test = vbiosp0:dispP0, vbiosp1:dispP1, vbiosp2:dispP2, vbiosP5:dispP5, vbiosp8:dispP8
test = vbiosp0:mclkP0, vbiosp1:mclkP1, vbiosp2:mclkP2, vbiosP5:mclkP5, vbiosp8:mclkP8
rmapi = perf
order = random
end = 20
tolerance = 0.05, 0.05
disable = thermalslowdown
