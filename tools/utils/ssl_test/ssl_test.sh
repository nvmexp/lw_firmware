#!/bin/bash

#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
# 

set -e

UNAME=$(uname -s)
WINDOWS=
[[ ${UNAME:0:6} = CYGWIN || ${UNAME:0:5} = MINGW ]] && WINDOWS=1



if [[ $# -gt 0 ]]; then
    SSL_TEST="$1"
    if [[ ! -x $SSL_TEST ]]; then
        echo "Unable to execute $SSL_TEST"
        exit 1
    fi
else
    SSL_TEST_NAME=ssl_test
    [[ $WINDOWS ]] && SSL_TEST_NAME=ssl_test.exe
    [[ $MODS_OUTPUT_DIR ]] || MODS_OUTPUT_DIR="$PWD/../../mods/artifacts"
    SSL_TEST=$(find "$MODS_OUTPUT_DIR" -maxdepth 6 -type f -name "$SSL_TEST_NAME" | head -n 1)
    if [[ -z $SSL_TEST || ! -x $SSL_TEST ]]; then
        echo "Please provide full path to ssl_test exelwtable as the first arg"
        exit 1
    fi
    echo "Using auto-discovered $SSL_TEST"
fi

echo "Testing network connection..."

SSL_TEST_NAME=$(basename $SSL_TEST)
SSL_DIR=$(dirname $SSL_TEST)
CA_FILE=HQLWCA121-CA.crt
cd $SSL_DIR
if [[ ! -f $CA_FILE ]]; then
    echo "$CA_FILE not found in $SSL_DIR"
    exit 1
fi

./$SSL_TEST_NAME
