# Clock Test Profile for Universal Clock Test
# DISPCLK and HUBCLK for GA10x and later

# Trials named with "pll" can not be reached using the bypass path.
# Since the bypass path is an integer divider of 2700MHz, the prime factorization
# of any target frequency which contains more than 3^3 is not reachable from the
# bypass path.

# Trials named with "bypass" can not be reached using the PLL path,
# generally because they are below the thresholds, which defined in
# bug 2557340/48 as:
# DISPCLK: 900MHz
# HUBCLK:  675MHz
# Since the PLL is an integer multiplier of 27MHz, the prime factorization of
# any target frequency which contains less than 3^3 is not reachable from the PLL.
# Bug 2789595 disabled NDIV_FRAC which means the multiplier is an integer.
# See also bug 200579236/14.

# For trials named with "any", the target frequency can not be reached with either
# the bypass or PLL, so that the resulting frequency indicates which path was
# chosen.  For "rmapi = vfinject". this means that the path is determined
# by VBIOS > Clocks > Clocks Programming Table.  Comments at each such test
# decode the result.  The tolerance must be large for these tests to pass.


name            = disppll
dispclk.freq    = 950MHz, 1150MHz, 1026MHz, 1350MHz, 1080MHz, 1296MHz, 1188MHz, 945MHz, 1134MHz

name            = hubpll
hubclk.freq     = 783MHz, 810MHz, 756MHz

name            = dispany
dispclk.freq    = 899MHz, 777MHz, 1350MHz, 555MHz, 1188MHz, 648MHz, 810MHz, 1107MHz, 675MHz, 1296MHz, 1269MHz

name            = hubany
hubclk.freq     = 450MHz, 800MHz, 540MHz, 783MHz, 810MHz, 675MHz

name            = dispbypass
dispclk.freq    = 450MHz, 540MHz, 675MHz, 900MHz

name            = hubbypass
hubclk.freq     = 675MHz, 450MHz, 540MHz


name            = DispHubClkTest
test            = initPstate:disppll, initPstate:dispany, initPstate:dispbypass, initPstate:hubpll, initPstate:hubany, initPstate:hubbypass
tolerance       = 0.15, 0.15
rmapi           = pmu
order           = sequence
end             = 50
disable         = thermalslowdown
