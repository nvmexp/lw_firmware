# Base Sine Test
# See https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Features#Sine_Test for details on Sine Test
# If this test fails, please file a bug and/or contact sw-gpu-rm-clocks-dev.
# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# Pull in chip specific test parameters
include sine.basic

# Trial Specification
# Sine test replaces the values of the frequency domains it applies to,
# for domains not changed by sine test, we will inherit from PState 0
name        = SineBasicTest
test        = initpstate:SineBasic
tolerance   = 0.15, 0.25
rmapi       = vfinject
order       = sequence
prune       = auto

