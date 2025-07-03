# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# To make a chip-specific version of this test,
# create a file with the chip/family name in the middle.
# Examples: overclockconfig.gk110b.ctp, overclockconfig.gm10x.ctp, overclockconfig.maxwell.ctp

# If this test fails, please file a bug or contact sw-gpu-rm-clocks-dev.

# This software-only test uses the LW2080_CTRL_CLK_CONFIG_INFO RMAPI call to configures clock domains 
# without changing the hardware state.  This ensures that Resman can handle the overclocked frequencies 
# without requiring the hardware to tolerate the stress.

name=gpc2clk
gpc2clk.freq = 2000MHz, 2100MHz, 2200MHz, 2300MHz, 2400MHz, 2500MHz, 2600MHz, 2700MHz, 2800MHz, 2900MHz, 3000MHz, 3100MHz, 3200MHz, 3300MHz, 3400MHz, 3500MHz, 3600MHz, 3700MHz, 3800MHz, 3900MHz, 4000MHz


# Trial specification for overclock.config test
# Define one trial: based on VBIOS P0
# tolerance.measured is meanlingless here, since checking measured freq is skipped for clk_config_info rmapi.
name       = ConfigClockTest 
test       = vbiosp0:gpc2clk
rmapi      = config
order      = sequence
tolerance  = 0.005, 0.005
