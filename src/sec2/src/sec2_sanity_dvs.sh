#!/bin/sh

export AS2_SEC2_SKIP_SIGN_RELEASE=true
./sec2-release-dvs.sh
if [ "$?" -ne "0" ]; then
     exit 1
fi