#!/bin/sh
export PATH="$AS2_TREEROOT/sw/tools/unix/hosts/Linux-x86/unix-build/bin:$PATH"
./fbflcn-release.sh
if [ "$?" -ne "0" ]; then
     exit 1
fi
p4 revert -a ../../../drivers/resman/kernel/inc/fbflcn/bin/g_fbfalconuc*
if [ "$?" -ne "0" ]; then
     exit 1
fi
