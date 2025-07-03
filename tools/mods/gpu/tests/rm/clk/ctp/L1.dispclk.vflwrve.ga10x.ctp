# VFLwrve Test
# This test fetches all the supported VF points for the requested clock domains
# and switches to them one by one.
# Note that we just specify master clock domains in this file and changing master
# clock domain frequency also make the corresponding change for slave clock domains
# based on the master-slave ratio or master-slave table relationship defined in
# the VBIOS.
#
# If this test fails, please file a bug and/or contact sw-gpu-rm-clocks-team.
# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

name=disp
dispclk.vflwrve = initpstate

name        = vflwrveTest
test        = initpstate:disp
rmapi       = vfinject
order       = random
#end         = 20 - this is what differentiates L1 wrt L0
tolerance   = 0.05, 0.05
disable     = thermalslowdown
