# P-State Test

# If this test fails, please file a bug or contact sw-gpu-rm-clocks-dev.

# To make a chip-specific version of these partial p-states,
# create a file with the chip/family name in the middle.
# For example: pstateswitch.gm107.ctp or pstateswitch.gm10x.ctp or pstateswitch.maxwell.ctp
# See https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest



# Test common p-states
name=switch
test=vbiosp0, vbiosp8
test=vbiosp0, vbiosp8
test=vbiosp0, vbiosp8
test=vbiosp0, vbiosp8
rmapi=pstate


