#!/bin/sh

export AS2_DPU_SKIP_SIGN_RELEASE=true
./dpu-release-dvs.sh "$@"
if [ "$?" -ne "0" ]; then
     exit 1
fi
