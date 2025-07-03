#!/bin/sh
export PATH="$AS2_TREEROOT/sw/tools/unix/hosts/Linux-x86/unix-build/bin:$PATH"
./pmu-release.sh
if [ "$?" -ne "0" ]; then
    exit 1
fi
p4 revert -a ../../drivers/resman/kernel/inc/pmu/bin/g_c85b6_*
if [ "$?" -ne "0" ]; then
    exit 1
fi
p4 revert -a ../../drivers/resman/kernel/inc/pmu/bin/g_pmu.def
if [ "$?" -ne "0" ]; then
    exit 1
fi
