# Used to test UCTFileReader and UCTSolver
#  - to start, copy paste this file and rename it to UCTTest.ctp
#  - have UCT start with UCTTest.ctp, it will report errors
#  - correct errors one by one to test
name=defaults
dispclk.freq = 324MHz, 900MHz
gpc2clk.freq = 324MHz

name=sys2clk
sys2clk.freq =  121MHZ

name=ltc2clk
ltc2clk.freq = +20%, -55%, 25%

name=mixed1
sys2clk.freq = +20%, -55%, 25%:+FORCE_BYPASS:-FORCE_PLL

name=msdclk
msdclk.freq = +50Mhz, -60Mhz

name=orderTest
test=vbiosp0:defaults:sys2clk:msdclk, 
test = vbiosp0:sinespec2:sinespec3:sinespec1, vbiosp8:sinespec1:sinespec3
test = vbiosp8:defaults:sys2clk:msdclk
test = vbiosp0:sinespec2:sinespec3:sinespec1, vbiosp8:sinespec1:sinespec3
rmapi=clocks
prune = msdclk

name=test2
test=vbiosp0:sinespec1:sinespec3, vbiosp5:sinespec1
rmapi=pstate

name=test3
test=vbiosp1:defaults, vbiosp0:msdclk

name = mixedTest1
test = vbiosp0:sinespec1:mixed1
test = vbiosp0:mixed1:sinespec1

name = mixedTest2
test = vbiosp8:msdclk:sinespec1:mixed1:ltc2clk:defaults

name=sinespec1
sys2clk.flags = none
sys2clk.alpha = 1620MHz
sys2clk.beta = 5mhz
sys2clk.gamma = 0.2
sys2clk.omega = 0.7

msdclk.flags= -FORCE_PLL
msdclk.alpha = 1620MHz
msdclk.beta = 5mhz
msdclk.gamma = 0.2
msdclk.omega = 0.7

iterations = 3

name=sinespec2
xbar2clk.alpha = 1020MHz
xbar2clk.beta = 11mhz
xbar2clk.gamma = 0.8
xbar2clk.omega = 0.4
iterations = 4

name=sinespec3
gpc2clk.flags = +FORCE_BYPASS:-FORCE_PLL
gpc2clk.alpha = 9800MHz
gpc2clk.beta = 3mhz
gpc2clk.gamma = 0.47
gpc2clk.omega = 0.66

dispclk.alpha = 9800MHz
dispclk.beta = 3mhz
dispclk.gamma = 0.47
dispclk.omega = 0.66

iterations = 5
