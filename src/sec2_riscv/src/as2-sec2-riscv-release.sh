#!/bin/sh
export PATH="$AS2_TREEROOT/sw/tools/unix/hosts/Linux-x86/unix-build/bin:$PATH"
# Flag to disable signing and release of profiles with prod-signed binaries

./sec2-release.sh
if [ "$?" -ne "0" ]; then
     exit 1
fi
p4 revert -a ../../../drivers/resman/kernel/inc/sec2_riscv/bin/g_sec2uc*
if [ "$?" -ne "0" ]; then
     exit 1
fi
