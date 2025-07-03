# Used to test UCTFileReader and UCTSolver
#  - to start, copy paste this file and rename it to UCTTest.ctp
#  - have UCT start with UCTTest.ctp, it will report errors
#  - correct errors one by one to test
name=defaults
dispclk.freq = 162MHz, 324MHz, 900MHz
gpc2clk.freq = 162MHz, 324MHz, 900MHz

name=sys2clk
sys2clk.freq = 162, 324Hz, 750khz, 1GHZ

name=legclk
legclk.freq = +20%, -55%, 25%

name=msdclk
msdclk.freq = +50Mhz, -60Mhz, +20khz, -10khzm, +25, -14, -1gHz

name=test
test=vbiosp0:defaults:sys2clk:sinespec3:msdclk, vbiosp8:defaults:sys2clk:legclk:msdclk
test = vbiosp0:sinespec1:inespec2:sinespec3, vbiosp8:sinespec2:sys2clk
rmapi=clocks

name=test2
test=vbiosp0:sinespec1:sinespec3:defaults, vbiosp5:sinespec1
rmapi=pstate

name=test3
test=vbiosp1:defaults:legclk, vbiosp0:msdclk,legclk


name=sinespec1
sys2clk.flags = none
sys2clk.alpha = 163Hz
sys2clk.beta = 1khz
sys2clk.gamma = 0.2
sys2clk.omega = 1.1

dispclk.alpha = 100MHz

msdclk.flags= -FORCE_PLL
msdclk.alpha = 1620MHz
msdclk.beta = 5mhz
msdclk.gamma = 0.2
msdclk.omega = 0.7

iterations = 3


name=sinespec2
xbar2clk.alpha = +160MHz
xbar2clk.beta = +90,
xbar2clk.gamma = -2
xbar2clk.omega = -1


name=sinespec3
gpc2clk.flags = +FORCE_BYPASS:-FORCE_PLL
gpc2clk.alpha = 50MHz, 20MHz
gpc2clk.beta = 2000000, 10MHz
gpc2clk.gamma = +1
gpc2clk.omega = -0.7

sys2clk.alpha = 980MHz
dispclk.beta = -3mhz
dispclk.gamma = 0.47khz
dispclk.omega = 0.66mHz

iterations = 5

