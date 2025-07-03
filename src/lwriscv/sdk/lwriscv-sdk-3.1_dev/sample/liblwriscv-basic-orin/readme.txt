This is sample app using liblwriscv.
It is to be run on baremtal ga10x gsp.

To build, set LW_RESMAN to path to checked out //sw/resman (we need manuals) and use build.sh script.
Or look at it and execute makes manually.

This will build proper liblwriscv profile and then app.

Load it with lwwatch:
lw> rv brb basic.text.encrypt.bin basic.data.encrypt.bin basic.manifest.encrypt.bin.out.bin

Then after it boots, print message buffer:
lw> rv dmesg
Build Mar 22 2020 08:51:02 on GSP RISC-V. Args: dmesg
Found debug buffer at 0xf000, size 0xff0, w 43, r 0, magic f007ba11
_riscvReadTcm_TU10X: doing unaligned tail reads (3 bytes).
DMESG buffer 
--------
Hello cruel world on Mar 31 2020 21:08:31.
------

