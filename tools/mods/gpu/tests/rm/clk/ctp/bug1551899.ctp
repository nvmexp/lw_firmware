# Bug 1551899
# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Features#Sine_Test for details on Sine Test


# Will reach maximum of over 3GHz at iteration 16
name           = sine
gpc2clk.alpha  = 810MHz
gpc2clk.beta   = 2200MHz
gpc2clk.omega  = 0
gpc2clk.gamma  = 0.1
iterations = 20

name        	= trial
test        	= vbiosp0:sine
tolerance   	= 0.05, 0.01
rmapi       	= config
order       	= sequence
prune       	= auto
