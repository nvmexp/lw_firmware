#!/bin/bash

#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
# 

set -e

finish()
{
    rm -f output
}

trap finish EXIT

UNAME=$(uname -s)
case "$UNAME" in
    CYGWIN*|MINGW*|MSYS*) UNAME="Windows" ;;
esac

NOCR=""
[[ $UNAME = Windows ]] && NOCR=--strip-trailing-cr

if [[ $# -gt 0 ]]; then
    PREPROC="$1"
    if [[ ! -x $PREPROC ]]; then
        echo "Unable to execute $PREPROC"
        exit 1
    fi
else
    [[ $MODS_OUTPUT_DIR ]] || MODS_OUTPUT_DIR="$PWD/../../mods/artifacts"
    PREPROC_NAME=preproc_test
    [[ $UNAME = Windows ]] && PREPROC_NAME=preproc_test.exe
    PREPROC=$(find "$MODS_OUTPUT_DIR" -maxdepth 6 -type f -name "$PREPROC_NAME" | head -n 1)
    if [[ -z $PREPROC || ! -x $PREPROC ]]; then
        echo "Please provide full path to preproc_test exelwtable as the first arg"
        exit 1
    fi
    echo "Using auto-discovered $PREPROC"
fi

echo "Testing..."
cd tests
for FILE in `ls *.expected`; do
    echo "${FILE/.expected/.h}"
    $PREPROC "${FILE/.expected/.h}" -I tests | grep -v "^Successfully opened " > output
    if ! diff $NOCR -q "$FILE" output; then
        echo "=========="
        echo "output:"
        cat output
        echo "=========="
        rm output
        exit 1
    fi
    rm output
done
