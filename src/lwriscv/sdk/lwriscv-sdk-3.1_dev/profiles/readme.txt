Provided variables:
LIBLWRISCV_CONFIGS - config directories of liblwriscv
DIR_SDK_PROFILE - directory where profiles are stored (this directory)


Format:
ucode - chip - engine (ucode- part is important, rest is less)

Extensions:
.txt - information, human readable

.liblwriscv.mk

.*.mk - various includes (should not have extensions from above)

Triggers:
.liblwriscv.mk triggers build of liblwriscv
.libccc.mk triggers build of libccc
