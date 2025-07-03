#!/bin/bash

set -euo pipefail

run_mods_and_fail()
{
    local EXIT_CODE=0
    local TOOL="$1"
    local MODS_DIR="$2"
    local SEQ_FAIL="$3"
    local SEQ_DIR="$4"
    shift 4

    export SEQ_FAIL

    mkdir "$SEQ_DIR"
    cd "$SEQ_DIR"

    if [[ $TOOL ]]; then
        $TOOL "$MODS_DIR"/mods "$@" || EXIT_CODE=$?
    else
        nice "$MODS_DIR"/mods "$@" > output 2>&1 || EXIT_CODE=$?
    fi

    echo "Sequence point $SEQ_FAIL exit code $EXIT_CODE"

    if [[ $EXIT_CODE != 0 && $EXIT_CODE != 1 ]]; then
        cat output
        echo "MODS failed with invalid exit code at sequence point $SEQ_FAIL" >&2
        exit 1
    fi

    cd ..
    rm -rf "$SEQ_DIR"
}

# TODO Add a way to auto-detect that we've run through all sequence points in MODS
MAX_SEQ_FAIL=1000000

MODS_DIR="$(cd -P "$(dirname "$0")" && pwd)"
[[ -z $(ls -d seq_fail* 2>/dev/null || true) ]] || rm -rf seq_fail*

if [[ $1 = -gdb ]]; then
    SEQ_FAIL="$2"
    shift 2
    run_mods_and_fail "gdb --args" "$MODS_DIR" "$SEQ_FAIL" seq_fail "$@"
    exit 0
fi

START_SEQ_POINT=1
JOBS=1
case "$(uname -s)" in
    Linux)                JOBS="$(grep -c ^processor /proc/cpuinfo)" ;;
    CYGWIN*|MINGW*|MSYS*) JOBS="$NUMBER_OF_PROCESSORS" ;;
esac

NOT_ARG=0
while [[ $NOT_ARG = 0 && $# -gt 0 ]]; do
    case "$1" in
        -start) START_SEQ_POINT="$2" ; shift 2 ;;
        -jobs)  JOBS="$2" ; shift 2 ;;
        *)      NOT_ARG=1 ;;
    esac
done

echo "Using $JOBS jobs"

PIDS=( )
for I in $(seq 0 $(($JOBS - 1))); do
    PIDS[$I]=1
done

I=0
for SEQ_FAIL in $(seq "$START_SEQ_POINT" $MAX_SEQ_FAIL); do
    PID=${PIDS[$I]}
    [[ $PID = 1 ]] || wait $PID
    run_mods_and_fail "" "$MODS_DIR" "$SEQ_FAIL" "seq_fail_${I}" "$@" &
    PIDS[$I]=$!
    I=$(($I + 1))
    [[ $I -lt $JOBS ]] || I=0
done
