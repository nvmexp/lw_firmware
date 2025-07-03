# P-State Test

# If this test fails, please file a bug or contact sw-gpu-rm-clocks-dev.

# To make a chip-specific version of these partial p-states,
# create a file with the chip/family name in the middle.
# For example: pstateswitch.gm107.ctp or pstateswitch.gm10x.ctp or pstateswitch.maxwell.ctp
# See https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# GV100 has only pstate P0 and pstate p5 and p8 have been removed

# Test all POR p-states
name=switch
test=vbiosp0
rmapi=pstate
tolerance = 0.05, 0.05

