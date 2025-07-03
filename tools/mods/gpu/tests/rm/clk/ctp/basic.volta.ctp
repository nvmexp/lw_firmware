# BasicSanityTest
# Adapted from diag/mods/gpu/tests/rm/utility/clk/clk_gf100.cpp:493 (ClockUtilGF100::ClockSanityTest)
# If this test fails, please file a bug and/or contact sw-gpu-rm-clocks-dev.
# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# You can make chip/family/platform-specific versions of this test,
# but your probably don't want to.  You'd probably prefer making
# chip/family/platform-specific versions of basic.pstate.ctp.
# Examples: basic.pstate.gf119.ctp, basic.pstate.gf11x.ctp, basic.pstate.fermi.ctp
# Examples: basic.pstate.emu.ctp, basic.pstate.emu.maxwell.ctp, basic.pstate.rtl.gf20x.ctp
# The 'include' directive automatically loads chip/family/platform-specific profiles.



# Define BasicSanity.HalfFreqBypass
include basic.pstate

# Trial Specification
# Run on the bypass path at half-speed bypass of P0
# then run at full speed P0.
# For a sanity test, once in each is sufficient.
name        = BasicSanityTest
test        = vbiosp0:BasicSanity.HalfFreqBypass, vbiosp0, vbiosp0:BasicSanity.FullFreqPLL
tolerance   = 0.15, 0.25
rmapi       = clk
order       = sequence
prune       = auto

