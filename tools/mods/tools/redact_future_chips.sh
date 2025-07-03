#!/bin/bash

set -euo pipefail

source "$(dirname "$0")"/redact_list

# Set C locale to ensure proper handling of binary files
export LC_ALL=C

# Use pigz if it's available, it's a multithreaded version of gzip
if [[ -n $(which pigz 2>/dev/null) ]]; then
    GZ="--use-compress-program pigz"
else
    GZ="-z"
fi

if [[ $# -lt 1 ]]; then
    echo "Usage: $(basename "$0") <RANDOMS_FILE> [OPTIONS] [<FILE>...]"
    exit 1
fi

# Create random replacements for chip patterns
# Chip names are used in variables in JS and we need a consistent way to replace
# each chip name across multiple scripts so that we don't break them.
REPLACEMENTS=( )
generate_replacements()
{
    local RANDOM_DATA
    if [[ -f $1 ]]; then
        RANDOM_DATA="$(cat "$1")"
    else
        echo "Generating random replacements in $1"
        RANDOM_DATA=$(tr -dc '[:alnum:]' < /dev/urandom | dd bs=1 count=512 2> /dev/null || true)
        echo -n "$RANDOM_DATA" > "$1"
    fi

    local PATTERN
    local I=0
    for PATTERN in "${CHIP_REGEX[@]}"; do
        # Replacement consists of underscore followed by random characters, as many
        # as in the pattern.  The pattern should contain only single characters,
        # dot and [character lists].
        local REPLACEMENT="$(sed 's=\\[<>]==g ; s=\[[^[]*\]=x=g ; s=.=x=g' <<< "$PATTERN")"
        local LEN=$((${#REPLACEMENT} - 1))
        [[ $(($I + $LEN)) -le ${#RANDOM_DATA} ]] || I=1
        REPLACEMENTS+=( "_${RANDOM_DATA:$I:$LEN}" )
        I=$(( $I + $LEN ))
    done
}

RANDOM_DATA_FILE="$1"
shift

NO_DELETE=""
ONLY_CONTENTS=""
GEN_INTERNAL=""
GW_DIR=""
GW_FILE=""
GW_EN=""
SKIP_REGEX="^$"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --no-delete)      NO_DELETE=1               ;;
        --only-contents)  ONLY_CONTENTS=1           ;;
        --gen-internal)   GEN_INTERNAL="$2" ; shift ;;
        --guardword)      GW_DIR="$2"
                          GW_FILE="$3"
                          GW_EN="$4"
                          shift 3                   ;;
        --skip)           SKIP_REGEX+="\|^$2\$"
                          shift                     ;;
        *)                break                     ;;
    esac
    shift
done

generate_replacements "$RANDOM_DATA_FILE"

smart_tar()
{
    local OPT="$1"
    local FILE="$2"
    shift
    shift

    local FILETYPE="$(file "$FILE" 2>/dev/null || true)"
    local COMPR="$GZ"
    if [[ $OPT = "-x" && $FILETYPE =~ XZ.compressed ]]; then
        COMPR="-J"
    elif [[ $FILETYPE =~ tar.archive ]]; then
        COMPR=""
    fi

    tar "$OPT" $COMPR -f "$FILE" "$@"
}

grep_regex()
{
    grep -i "${CHIP_REGEX[@]/#/--regex=}" "$@"
}

sed_inplace()
{
    case "$(uname -s)" in
        Darwin)
            echo "% sed -i '' '$1' $2"
            sed -i '' "$1" "$2"
            ;;
        CYGWIN*|MINGW*|MSYS*)
            echo "% sed -i '$1' $2"
            local TMP="$2.sed"
            cp -f "$2" "$TMP"
            rm -f "$2"
            sed "$1" < "$TMP" > "$2"
            rm -f "$TMP"
            ;;
        *)
            echo "% sed -i '$1' $2"
            sed -i "$1" "$2"
            ;;
    esac
}

indent()
{
    local HIDE_ERRORS="s/^/    / ; s/[Ee][Rr][Rr][Oo][Rr]/exxor/g"
    if [[ ${HIDE_ERRORS_IN_REDACTED_STRINGS:-} = "noerrors" ]]; then
        sed "$HIDE_ERRORS"
    elif [[ ${DVS_SW_CHANGELIST:-} ]]; then
        if [[ $# -gt 0 && $1 = guardword ]]; then
            if [[ $GW_EN = false ]]; then
                HIDE_ERRORS=";$HIDE_ERRORS"
            else
                HIDE_ERRORS=""
            fi
            sed "s/^/GUARDWORD:    /$HIDE_ERRORS"
        else
            sed "s/^/REDACT:    /"
        fi
    else
        sed "s/^/    /"
    fi
}

check_file_contents()
{
    local FILE="$1"

    if [[ -d $FILE ]]; then
        check_directory_contents "$FILE" || return $?
        return 0
    fi

    local BASENAME="$(basename "$FILE")"
    if grep_regex <<< "$BASENAME" | indent; then
        if [[ $NO_DELETE = 1 ]]; then
            local TRANSLATE_FILE="$(mktemp)"
            TEMPS+=( "$TRANSLATE_FILE" )
            
            echo "$BASENAME" > "$TRANSLATE_FILE"
            check_file_contents "$TRANSLATE_FILE"
            local NEW_NAME="$(dirname "$FILE")/$(cat "$TRANSLATE_FILE")"

            rm -f "$TRANSLATE_FILE"
            unset 'TEMPS[${#TEMPS[@]}-1]'

            echo "Renaming $FILE to $NEW_NAME"
            mv "$FILE" "$NEW_NAME"
            FILE="$NEW_NAME"
        elif [[ $ONLY_CONTENTS != 1 ]]; then
            echo "Deleting file $FILE, because its filename contains illegal strings"
            rm -f "$FILE"
            return 1
        fi
    fi

    local FILETYPE="$(file "$FILE" 2>/dev/null || true)"
    if [[ $FILETYPE =~ (XZ|gzip).compressed || $FILETYPE =~ tar.archive ]]; then
        check_archive_contents "$FILE" tar || return $?
        return 0
    fi
    if [[ $FILETYPE =~ Zip.archive ]]; then
        check_archive_contents "$FILE" zip || return $?
        return 0
    fi

    if [[ $FILE =~ \.jse$|\.spe$|\.he$|\.jsone$|\.xmle$|\.dbe$ ]]; then
        echo "Skipping file $FILE because it is encrypted"
        return 0
    fi

    if [[ $FILE =~ INTERNAL_names.db$ ]]; then
        echo "Skipping $FILE"
        return 0
    fi

    echo "Checking file contents: $FILE"

    # Quick test with grep, replace all non-printable characters with EOL
    # to improve readability of the log - all illegal strings will be printed
    if ! tr -c '[:print:]' '\n' < "$FILE" | grep_regex | indent; then
        run_guardword "$FILE"
        return 0
    fi

    echo "Redacting $FILE"

    local FULL_PATTERN=""
    local I
    for I in $(seq 0 $((${#CHIP_REGEX[@]} - 1))); do
        local PATTERN="${CHIP_REGEX[$I]}"
        local REPLACEMENT="${REPLACEMENTS[$I]}"
        [[ -z $FULL_PATTERN ]] || FULL_PATTERN="$FULL_PATTERN ; "
        FULL_PATTERN="${FULL_PATTERN}s=$PATTERN=$REPLACEMENT=g"
    done
    sed_inplace "$FULL_PATTERN" "$FILE"

    if tr -c '[:print:]' '\n' < "$FILE" | grep_regex | indent; then
        echo "WARNING: We may have missed some oclwrences of illegal strings due to case sensitivity!"
    fi

    run_guardword "$FILE"

    return 1
}

GW_IGNORE=( )  # ignore list for grep
GW_LIST_CI=( ) # case insensitive regexes for grep
GW_LIST_CS=( ) # case sensitive regexes for grep

prepare_guardword()
{
    if grep -q "^[^#]*#include" "$1"; then
        echo "Guardword file $1 contains unsupported #include keyword" >&2
        exit 1
    fi

    local IGNORE_STR
    local GUARDWORD_STR
    local ORIG_IFS="$IFS"
    IFS='
'
    IGNORE_STR=( $(grep "^ *IGNORE" "$1" | sed 's/#.*//') )
    GUARDWORD_STR=( $(grep "^ *GUARDWORD" "$1" | sed 's/#.*//') )
    IFS="$ORIG_IFS"

    local LINE
    local I=0
    if [[ ${#IGNORE_STR[*]} -gt 0 ]]; then
        while [[ $I -lt ${#IGNORE_STR[@]} ]]; do
            LINE="${IGNORE_STR[$I]}"
            I=$(($I + 1))
            local REGEX=$(sed 's/^[^{]*{ *"// ; s/" *}.*//' <<< "$LINE")
            GW_IGNORE+=( "$REGEX" )
        done
    fi

    if [[ ${#GUARDWORD_STR[*]} -gt 0 ]]; then
        for LINE in "${GUARDWORD_STR[@]}"; do
            local REGEX=$(sed 's/^[^{]*{ *"// ; s/"[^"]*}.*//' <<< "$LINE")

            if grep -q "\".*caseSensitive.*}" <<< "$LINE"; then
                GW_LIST_CS+=( "$REGEX" )
            else
                GW_LIST_CI+=( "$REGEX" )
            fi
        done
    fi
}

run_guardword()
{
    [[ $GW_DIR && $GW_FILE ]] || return 0

    echo "Running guardword on $1"

    if [[ ${#GW_IGNORE[*]} -eq 0 && ${#GW_LIST_CI[*]} -eq 0  && ${#GW_LIST_CS[*]} -eq 0 ]]; then
        prepare_guardword "$GW_DIR/baseGuardwords.txt"
        prepare_guardword "$GW_FILE"
        if [[ -f ${GW_FILE%/*}/gw_ignore.txt ]]; then
            prepare_guardword "${GW_FILE%/*}/gw_ignore.txt"
        fi
    fi

    local FAILED=0

    if [[ ${#GW_LIST_CI[@]} -gt 0 ]]; then
        if tr -c '[:print:]' '\n' < "$1" | grep -v "${GW_IGNORE[@]/#/--regex=}" | grep -i "${GW_LIST_CI[@]/#/--regex=}" | indent guardword; then
            FAILED=1
        fi
    fi
    if [[ ${#GW_LIST_CS[@]} -gt 0 ]]; then
        if tr -c '[:print:]' '\n' < "$1" | grep -v "${GW_IGNORE[@]/#/--regex=}" | grep "${GW_LIST_CS[@]/#/--regex=}" | indent guardword; then
            FAILED=1
        fi
    fi
    if [[ $FAILED != 0 ]]; then
        echo "******************************************************************************"
        if [[ $GW_EN = false ]]; then
            echo "GUARDWORD FAILED ON $1"
            echo "******************************************************************************"
        else
            echo "Error: GUARDWORD FAILED ON $1"
            echo "******************************************************************************"
            exit 1
        fi
    fi
}

check_archive_contents()
{
    local ARCHIVE="$1"
    local FILETYPE="$2"

    if [[ ${ARCHIVE##*/} = driver.tgz ]]; then
        echo "Skipping archive: $ARCHIVE"
        return 0
    fi

    echo "Checking archive contents: $ARCHIVE"

    local DIR="$(mktemp -d)"
    TEMPS+=( "$DIR" )

    if [[ $FILETYPE = tar ]]; then
        smart_tar -x "$ARCHIVE" -C "$DIR"
    elif [[ $FILETYPE = zip ]]; then
        unzip "$ARCHIVE" -d "$DIR"
    else
        echo "Unhandled archive type $FILETYPE: $ARCHIVE" >&2
        exit 1
    fi

    local CHANGED=0

    check_directory_contents "$DIR" || CHANGED=1

    if [[ $CHANGED -ne 0 ]]; then
        echo "Updating archive $ARCHIVE"
        if [[ $FILETYPE = tar ]]; then
            smart_tar -c "$ARCHIVE" -C "$DIR" $(ls "$DIR")
        elif [[ $FILETYPE = zip ]]; then
            local ABS_ARCHIVE_PATH="$ARCHIVE"
            [[ $ABS_ARCHIVE_PATH =~ ^/ ]] || ABS_ARCHIVE_PATH="$(pwd)/$ARCHIVE"
            rm -f "$ARCHIVE"
            ( cd "$DIR" && zip -9 -r "$ABS_ARCHIVE_PATH" * )
        else
            echo "Unhandled archive type $FILETYPE: $ARCHIVE" >&2
            exit 1
        fi
    else
        echo "Archive $ARCHIVE OK"
    fi

    rm -rf "$DIR"
    unset 'TEMPS[${#TEMPS[@]}-1]'

    return $CHANGED
}

check_directory_contents()
{
    local DIR="$1"

    local CHANGED=0
    local FILE

    # Note: mods and mats exelwtables were already redacted after linking and
    # probably compressed with upx, don't try to redact them again.
    for FILE in $(ls "$DIR" | grep -v "$SKIP_REGEX"); do
        check_file_contents "$DIR/$FILE" || CHANGED=1
    done

    return $CHANGED
}

TEMPS=( )

remove_tmpdir()
{
    [[ ${#TEMPS[@]} -gt 0 ]] || return 0
    local DIRNAME
    for DIRNAME in "${TEMPS[@]}"; do
        rm -rf "$DIRNAME"
    done
}

# Generate internal (non-releaseable) map
gen_internal()
{
    rm -f "$GEN_INTERNAL"
    touch "$GEN_INTERNAL"
    if [[ ${#DEOBFUSCATE[*]} -eq 0 ]]; then
        # It is required that a non-empty deobfuscation file be present so create
        # dummy deobfuscation that will never be hit
        echo "JensensFavoriteColorIsBlue" >> "$GEN_INTERNAL"
        echo "DwightsFavoriteColorIsRed" >> "$GEN_INTERNAL"
        return 0
    fi
    local PATTERN
    local I
    for PATTERN in "${DEOBFUSCATE[@]}"; do
        I=0
        for I in $(seq 0 $((${#CHIP_REGEX[@]} - 1))); do
            if [[ $PATTERN = "${CHIP_REGEX[$I]}" ]]; then
                local REPLACEMENT="${REPLACEMENTS[$I]}"
                echo "$REPLACEMENT" >> "$GEN_INTERNAL"
                echo "$PATTERN" >> "$GEN_INTERNAL"
                break
            fi
        done
    done
}

[[ -z $GEN_INTERNAL ]] || gen_internal

trap remove_tmpdir EXIT

while [[ $# -gt 0 ]]; do
    check_file_contents "$1" || true
    shift
done
