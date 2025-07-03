LW50 vbiosii for LW50 display verification are named according to following
rules.

1. All names start with lw50n.
2. Following lw50n, there are 3 hex digits to describe the DCB.


How is DCB described using 3 hex digits?
Each OR type (DAC/SOR/PIOR) is represented by two or three bits depending on
the number of ORs of that type. For example, DACs are represented using 3 bits
while SORs need only 2 bits. So, we have 7 bits as indicated below for LW50

Bit values:     x    x    x    x    x     x     x
Bit Locations:  7    6    5    4    3     2     1
OR:            DAC0 DAC1 DAC2 SOR0 SOR1 PIOR0 PIOR1

Since, each x is a binary digit, it can be only either 0 or 1. What 0 or 1
at a particular bit number mean depends on the the OR type. Lwrrently, the
following convention is being used

DAC:  TV   = 0, CRT  = 1
SOR:  LVDS = 0, TMDS = 1
PIOR: SDI  = 0, TMDS = 1


Thus, a DCB which looks like the following will be described by 011 10 01
or 321 in hex. So, the vbios name would be lw50n321.rom

Type    OR
TV      DAC0
CRT     DAC1
CRT     DAC2
TMDS    SOR0
LVDS    SOR1
SDI     PIOR0
TMDS    PIOR1

How to figure out the DCB from any given ROM name?
1. Colwert the 3 hex digits following lw50n in the ROM name into their
hex representations while using 3 bits for DAC, 2 for each of SOR and PIOR.
2. Use the Bit values and locations table described above to assign values
to each OR.
3. Use the convention about what 0, 1 within an OR group means to colwert
the binary number into an output device (CRT, TV, TMDS etc).

Example: The bit representation of DCB for lw50n712.rom is 111 01 10
which is equivalent to the following table

Type    OR
CRT     DAC0
CRT     DAC1
CRT     DAC2
LVDS    SOR0
TMDS    SOR1
TMDS    PIOR0
SDI     PIOR1

