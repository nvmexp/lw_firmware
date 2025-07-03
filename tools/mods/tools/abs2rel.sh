#!/bin/bash
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2015,2017 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
set -o igncr 2>/dev/null || true   # Ignore carriage returns on Cygwin

set -e

if [[ -z $1 ]]; then
    echo "Empty path passed as first argument to `basename $0`" >&2
    exit 1
fi

# Function to turn a local path into a full path, with trailing slash
fullpath()
{
    case $(uname -s) in
        CYGWIN*|MSYS*)
            # Windows
            cygpath -au "$1"
            ;;
        Darwin)
            # MacOSX
            local PREFIX="$1"
            local SUFFIX=""
            while ! [[ -d "$PREFIX" ]]; do
                SUFFIX="/$(basename "$PREFIX")$SUFFIX"
                PREFIX="$(dirname "$PREFIX")"
            done
            echo "$(cd "$PREFIX" && pwd)$SUFFIX"
            ;;
        *)
            # Linux
            readlink -m "$1"
            ;;
    esac | sed 's!/*$!/!' # Replace trailing slashes with one slash
}

# Get paths from command line
TGT_PATH=$(fullpath "$1")
WORK_DIR=$(fullpath "${2:-$(pwd)}")

# Early exit if both dirs are the same
if [[ $TGT_PATH = $WORK_DIR ]]; then
    echo "."
    exit
fi

# Remove the same directory at the beginning of both TGT_PATH and WORK_DIR
while [[ $TGT_PATH != $WORK_DIR ]]; do
    TGT_HEAD="${TGT_PATH%%/*}"
    WORK_HEAD="${WORK_DIR%%/*}"
    [[ $TGT_HEAD = $WORK_HEAD ]] || break
    TGT_PATH="${TGT_PATH#*/}"
    WORK_DIR="${WORK_DIR#*/}"
done

# Change WORK_DIR into a relative path ../../.. (etc.)
REV_WORK_DIR=$(sed -e 's/[^/][^/]*/../g' <<< "$WORK_DIR")

if [[ -z "$TGT_PATH" ]]; then
    echo "${REV_WORK_DIR%/}"
else
    echo "${REV_WORK_DIR}${TGT_PATH%/}"
fi
