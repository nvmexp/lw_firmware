# Emulation MClkSwitchTest Settings

# Silicon-specific Pseudo-P-State Definitions
# Silicon doesn't handle the same range as emulation, etc.,
# so we use a limited range of frequencies on the PLL path.
# The BYPASS path are okay with the wider range.
# See bug 1321545.





# Range between POR and POR-10%
# This partial p-state should be used with vbiosP0 or vbiosP1.
name=MClkSwitchTestPll
mclk.freq = +0%, -10%, +0%, -10%, +0%


# Bypass frequencies
# This partial p-state should be used with vbiosP8 or similar.
name=MClkSwitchTestBypass
mclk.freq = 270MHz, 324MHz, 405MHz, 540MHz, 810MHz


# MCLK at 90% of POR
# This partial p-state should be used with vbiosP0 or vbiosP1.
name=MClk90Percent
mclk.freq = 90%:+FORCE_PLL:-FORCE_BYPASS

# MCLK at 80% of POR
# This partial p-state should be used with vbiosP0 or vbiosP1.
name=MClk80Percent
mclk.freq = 80%:+FORCE_PLL:-FORCE_BYPASS

# MCLK at 810MHz
# This partial p-state should be used with vbiosP5 or similar.
name=MClk810MHz
mclk.freq = 810MHz:+FORCE_BYPASS:-FORCE_PLL

# MCLK at 405MHz
# This partial p-state should be used with vbiosP8 or similar.
name=MClk405MHz
mclk.freq = 405MHz:+FORCE_BYPASS:-FORCE_PLL

# MCLK at 324MHz
# This partial p-state should be used with vbiosP8 or similar.
name=MClk324MHz
mclk.freq = 324MHz:+FORCE_BYPASS:-FORCE_PLL

# MCLK at 270MHz
# This partial p-state should be used with vbiosP8 or similar.
name=MClk270MHz
mclk.freq = 270MHz:+FORCE_BYPASS:-FORCE_PLL


