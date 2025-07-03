#!/bin/bash
export PATH="$AS2_TREEROOT/sw/tools/unix/hosts/Linux-x86/unix-build/bin:$PATH"
# Flag to disable signing and release of profiles with prod-signed binaries
export AS2_SOE_SKIP_SIGN_RELEASE=true
# Building SOE profiles individually due to bug 200701730.
# This is temporary, will be reverted once the bug is fixed.
export SOECFG_PROFILE=soe-lr10
./soe-release.sh
./soe-release.sh prod
if [ "$?" -ne "0" ]; then
     exit 1
fi
export SOECFG_PROFILE=soe-ls10
./soe-release.sh
if [ "$?" -ne "0" ]; then
     exit 1
fi
p4 revert -a ../../../drivers/lwswitch/kernel/inc/soe/bin/g_soeuc*
if [ "$?" -ne "0" ]; then
     exit 1
fi

