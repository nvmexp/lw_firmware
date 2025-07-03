Virtual fields provide a way to specify where a register field is located in a chip
independant manner.

This script is used to parse through Virtual Field tables and parse data from them
for each chip, grouping chips with same Virtual Field tables together and printing out
their table of register addresses.

Requirements:
1. Mapped to workspace //sw/main/bios/...
2. Mapped //sw/dev/gpu_drv/chips_a/...
3. At least Python 3.5 

How to use:
The script does not take any argument, so running would be like this:
./vfieldval_vfield_bios_retrieve.py