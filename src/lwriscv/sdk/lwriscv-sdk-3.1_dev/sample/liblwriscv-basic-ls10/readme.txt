This is sample app using liblwriscv.
It is to be run on baremtal ls10 fsp/minion/soe.

To build, set LW_RESMAN to path to checked out //sw/resman (we need manuals) and use build.sh script.
Or look at it and execute makes manually.

This will build proper liblwriscv profile and then app.

Load it with lwwatch:
lw> rv brb basic.text.encrypt.bin basic.data.encrypt.bin basic.manifest.encrypt.bin.out.bin

Then after it boots, print message buffer:
lw> rv dmesg
Build Mar 22 2020 08:51:02 on XXX RISC-V. Args: dmesg
Found debug buffer at 0xf000, size 0xff0, w 43, r 0, magic f007ba11
DMESG buffer 
--------
Hello world. You are running on engine without PRI, this binary was built on xxx
------

