# Sanity Test for PLL Databases
# See bug 1196510

# If this test fails, please file a bug and/or contact sw-gpu-rm-clocks-dev.

# Chip-specific versions use a file name with the chip/family name in the middle.
# For example: emu.pstateswitch.gm107.ctp or emu.pstateswitch.gm10x.ctp or emu.pstateswitch.maxwell.ctp
# See https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest


# Test all POR p-states
name=Sanity
test=vbiosp5, vbiosp0, vbiosp8
rmapi=pstate



