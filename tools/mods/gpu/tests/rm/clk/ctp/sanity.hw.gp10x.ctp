# Sanity Tests
# This Clock Test Profile is run by default from the Universal Clock Test
# Use the -ctp command line option to run a different test.

# If this test fails, please file a bug or contact sw-gpu-rm-clocks-dev.
# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# By default, the include directive loads a generic version of the tests.
# Examples: basic.ctp, overclock.ctp

# You can make chip/family/platform-specific versions of this test,
# but your probably don't want to.  You'd probably prefer making
# chip/family/platform-specific versions of basic.pstate.ctp or
# overclock.pstate.ctp, which are included from basic.ctp and overclock.ctp.

# Examples: basic.pstate.gf119.ctp, basic.pstate.gf11x.ctp, basic.pstate.fermi.ctp
# Examples: overclock.pstate.emu.ctp, overclock.pstate.emu.maxwell.ctp, overclock.pstate.rtl.gf20x.ctp

include basic
