#!/bin/bash

set -euo pipefail

RANDOM_DATA="$1"
shift

ENCRYPT_TOOL="$1"
shift

REDACT_SCRIPT="$(dirname "$0")/redact_future_chips.sh"

EXTRA_OPTIONS=( )
OUTPUT=""
SOURCE=""
parse_args()
{
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --input)
                 ;;
            -o)  shift ; OUTPUT="$1" ;;
            -o*) OUTPUT="${1#-o}"
                 ;;
            -*)  EXTRA_OPTIONS+=( "$1" )
                 ;;
            *)   if [[ $SOURCE ]]; then
                     echo "Source specified twice: \"$SOURCE\" and \"$1\""
                     exit 1
                 fi
                 SOURCE="$1"
        esac
        shift
    done
}
parse_args "$@"

OUT_DIR="$OUTPUT"
[[ -d $OUT_DIR ]] || OUT_DIR=$(dirname "$OUTPUT")
SOURCE_COPY="$OUT_DIR/$(basename "$SOURCE")"
if [[ $SOURCE_COPY != $SOURCE ]]; then
    rm -f "$SOURCE_COPY"
    cp "$SOURCE" "$SOURCE_COPY"
fi  
# Don't delete any files, they will be deleted anyway after packaging
"$REDACT_SCRIPT" "$RANDOM_DATA" --only-contents "$SOURCE_COPY"
if [ ${#EXTRA_OPTIONS[@]} -eq 0 ]; then
    "$ENCRYPT_TOOL" --input "$SOURCE_COPY" -o "${OUTPUT}"
else
    "$ENCRYPT_TOOL" "${EXTRA_OPTIONS[@]}" --input "$SOURCE_COPY" -o "${OUTPUT}"
fi  
