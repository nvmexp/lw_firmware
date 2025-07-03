#!/bin/sh

export PATH="$AS2_TREEROOT/sw/tools/unix/hosts/Linux-x86/unix-build/bin:$PATH"
export LW_TOOLS="$AS2_TREEROOT/sw/tools"

# Flag to disable signing and release of profiles with prod-signed binaries
export AS2_ACR_SKIP_SIGN_RELEASE=true
sh ./build.sh 
if [ "$?" -ne "0" ]; then
     exit 1
fi
