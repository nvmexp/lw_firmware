#!/bin/bash

set -euo pipefail

die()
{
    echo "$@"
    exit 1
}

find_exe()
{
    if [[ ! -x $1 ]]; then
        local EXE="$(which "$1" 2>/dev/null || true)"
        if [[ -n $EXE ]]; then
            echo "$EXE"
            return 0
        fi
    fi

    echo "$1"
}

[[ $# -ge 3 ]] || die "Error: separate_symbols.sh needs at least 3 arguments, but $# provided"

OBJCOPY="$(find_exe "$1")"
STRIP="$(find_exe "$2")"
shift 2

# Temporary workaround
[[ $STRIP =~ aarch64 ]] && ! [[ $OBJCOPY =~ aarch64 ]] && OBJCOPY=$(echo "$STRIP" | sed "s/strip/objcopy/")

[[ -x $OBJCOPY ]] || die "Error: Missing exelwtable: $OBJCOPY"
[[ -x $STRIP ]] || die "Error: Missing exelwtable: $STRIP"

separate_symbols_from_file()
{
    echo "Separating symbols from $1"
    local SRC="$1"
    local DST="$(basename "$1").debug"
    "$OBJCOPY" --only-keep-debug "$SRC" "$DST"
    "$STRIP" --strip-debug --strip-unneeded "$SRC"
    "$OBJCOPY" "--add-gnu-debuglink=$DST" "$SRC"
}

is_known_file()
{
    local LWR_NAME="$(basename "$1")"

    # libusb is pre-compiled and checked-in, don't modify it
    if [[ $LWR_NAME =~ libusb ]]; then
        return 1
    fi

    if [[ $LWR_NAME = mods || $LWR_NAME =~ .so$ ]]; then
        return 0
    fi

    return 1
}

# Separate symbols from each exelwtable file
SYMBOL_FILES=( )
for FILE in "${@}"; do
    if is_known_file "$FILE"; then
        separate_symbols_from_file "$FILE"

        # Only include symbol file if the exelwtable/so had symbols
        SYMFILE="${FILE##*/}.debug"
        if [[ $(stat -c '%s' "$SYMFILE") -ge 16384 ]]; then
            SYMBOL_FILES+=( $SYMFILE )
        fi
    fi
done

# Compress each symbol file with xz
COMPRESSED_SYMBOL_FILES=( )
for FILE in "${SYMBOL_FILES[@]}"; do
    echo "Compressing $FILE with xz"
    time xz -1 "$FILE"
    COMPRESSED_SYMBOL_FILES+=( "$FILE.xz" )
done

# Create package containing all symbol files
PACKAGE=symbols.tgz
echo "Packaging all symbol files into $PACKAGE"
time tar czf "$PACKAGE" "${COMPRESSED_SYMBOL_FILES[@]}"
