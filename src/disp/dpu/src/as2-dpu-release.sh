#!/bin/sh
export PATH="$AS2_TREEROOT/sw/tools/unix/hosts/Linux-x86/unix-build/bin:$PATH" 
# Flag to disable signing and release of profiles with prod-signed binaries
export AS2_DPU_SKIP_SIGN_RELEASE=true
./dpu-release.sh
if [ "$?" -ne "0" ]; then
     exit 1
fi

p4 revert -a ../../../../drivers/resman/kernel/inc/dpu/bin/g_dpuuc*
if [ "$?" -ne "0" ]; then
     exit 1
fi

