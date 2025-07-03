#!/bin/bash

# Environment variables required initially to build LinuxMfg
#export BUILD_OS=linuxmfg
#export VERSION=177.38
#export CHANGELIST=3651904
#export P4ROOT=~/p4

set -e

export BUILD_CFG="release"
export INCLUDE_LWDA="true"
export INCLUDE_ENCRYPT_INTERNAL="true"

print_elapsed_since()
{
    local START=$1
    local END=$(date +%s)
    local ELAPSED=$((END - START))
    local HR=$((ELAPSED / 3600))
    local MIN=$((ELAPSED % 3600 / 60))
    local SEC=$((ELAPSED % 60))
    echo "$2 : Elapsed(hr:min.sec): ($HR:$MIN.$SEC)"
}

elapsed()
{
    local START=$(date +%s)
    local TAG="$1"
    shift
    "$@"
    print_elapsed_since $START "$TAG"
}

# When printing strings containing illegal keywords during redaction,
# avoid printing the "error" keyword to avoid breaking the build in
# Jenkins just by having the word "error" printed.
export HIDE_ERRORS_IN_REDACTED_STRINGS="noerrors"

# Detect number of CPUs
NUMCPUS=$(grep -c ^processor /proc/cpuinfo)
[[ $NUMCPUS -lt 1 ]] && NUMCPUS=1
export JOBS=$NUMCPUS
[[ $NUMCPUS -gt 1 ]] && JOBS=$(($NUMCPUS+$NUMCPUS/2))
echo "# Detected $NUMCPUS CPU(s), will launch $JOBS job(s)"

cd -P "$(dirname "$0")"

# Disable implicit rules (-r)
MAKE_CMD="make -r -j $JOBS"

echo "# [$(date)] Building MODS"
SCRIPT_START_TIME=$(date +%s)
elapsed "Clean MODS Time" $MAKE_CMD clean_all
elapsed "Build MODS Time" $MAKE_CMD build_all

echo "# [$(date)] Building MODS Package"
elapsed "Package MODS Time" $MAKE_CMD zip jenkins

if [[ "$BUILD_OS" = "linuxmfg" ]]; then
    EXT="tgz"

    echo "# [$(date)] Building 629 Package"
    elapsed "629 Package Time" tools/modsbld_stealth_end_user.sh "629.spc" "629.$VERSION" "$CHANGELIST"

    echo "# [$(date)] Building dgxfielddiag Package"
    elapsed "dgxfielddiag Package Time" tools/modsbld_stealth_end_user.sh "dgxfielddiag.spc" "dgxfielddiag.$VERSION" "$CHANGELIST" installonly       
fi

if [[ "$INCLUDE_SYMBOLIZED" = "true" ]]; then
    echo "# [$(date)] Building MODS Symbols"
    export SYMBOLIZE="true"
    elapsed "Symbolized MODS Clean Time" $MAKE_CMD clean_all
    elapsed "Symbolized MODS Build Time" $MAKE_CMD build_all
    echo "# [$(date)] Building MODS Symbolized Package"
    elapsed "Symbolized MODS Package Time" $MAKE_CMD zip
    unset SYMBOLIZE
fi

echo "# [$(date)] MODS Build Complete"
print_elapsed_since $SCRIPT_START_TIME "Total MODS Build Time"
