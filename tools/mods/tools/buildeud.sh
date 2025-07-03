#!/bin/bash

# If BUILD_CFG is defined before running this script and is equal debug, then
# the build will build all libraries in debug mode and will stop before
# packaging.

# Since JS1.7 will also be a debug build, you will have to disable
# asserts in JS_GetPrivate and JS_SetPrivate before compile the JS source code
# due to improper use of the private field in JS objects by MODS.

# Any subsequent commands which fail will cause the shell script to exit
# immediately
set -euo pipefail

declare -r SCRIPTNAME="$(basename "$0")"

function usage()
{
    echo "Usage: $SCRIPTNAME <VERSION> <CHANGELIST> [dvs]"
}

if (($# < 2 || $# > 3)); then
    echo "$SCRIPTNAME requires two or three arguments" >&2
    usage >&2
    exit 1
fi

if [[ !$2 =~ '^[0-9]+$' ]]; then
    echo "The second argument ($2) has to be a number" >&2
    usage >&2
    exit 1
fi

if [[ (($# == 3)) && $3 != dvs ]]; then
    echo "If present the third argument has to be dvs" >&2
    usage >&2
    exit 1
fi

export VERSION=$1
export CHANGELIST=$2
export ISDVS=false
if [[ $# -eq 3 && $3 = dvs ]]; then
    ISDVS=true
    export DVS_SW_CHANGELIST=$2
fi

if [[ "${SYMBOLIZE:-}" == "true" ]]; then
    PREFIX="S"
else
    PREFIX=""
fi

# If you change the following name, update "Release Linux AMD64 EUD Mods"
# configuration files in DVS
declare -r EUD_MODS_PACKAGE_NAME=${PREFIX}P${VERSION}.tgz

# If you change the following name, update "Release Linux AMD64 EUD Mods" and
# "Release Linux AMD64 EUD Package" configuration files in DVS
declare -r LWVS_PACKAGE_NAME=${PREFIX}${VERSION}.tgz

# For Buildmeister, it sets SHARED_MODS_FILES_DIR to access fuse files,
# we cannot use P4ROOT there. Declare it locally, so the make files use their
# own logic for SHARED_MODS_FILES_DIR.
declare -r SHARED_MODS_FILES_DIR=${SHARED_MODS_FILES_DIR:-$P4ROOT/sw/mods}

export BUILD_OS=linux

if [[ -z ${BUILD_ARCH+x} ]]; then
    TARGET_ARCH=amd64
else
    TARGET_ARCH=$BUILD_ARCH
fi
export BUILD_ARCH=amd64

export BUILD_CFG=${BUILD_CFG:-release}
export BOUND_JS=true

export STEALTH_MODE=617
export SHUFFLE_OPCODES=true

export MODS_KERNEL_RM_VER_CHK=true
export INCLUDE_BOARDS_DB=false
export INCLUDE_LWDA=true
export INCLUDE_MDIAG=false
export MODS_OUTPUT_DIR=${MODS_OUTPUT_DIR:-$PWD/artifacts}
export MODS_RUNSPACE=${MODS_RUNSPACE:-$PWD/runspace}

declare -r OTHER_GOOD_FILES="lwca.bin\|default.bin"
declare -r rel_o_dir=$BUILD_CFG/$BUILD_OS/$BUILD_ARCH
declare -r TARGET_REL_O_DIR=$BUILD_CFG/$BUILD_OS/$TARGET_ARCH
declare -r rel_o_dir_release=release/$BUILD_OS/$BUILD_ARCH
declare -r MODS_O_DIR="$MODS_OUTPUT_DIR/$TARGET_REL_O_DIR"/mods
declare -r MODS_O_DIR_GEN="$MODS_OUTPUT_DIR/$TARGET_REL_O_DIR"/mods_gen
declare -r LW_CERT="$SHARED_MODS_FILES_DIR/shared_files/HQLWCA121-CA.crt"
declare -r JOBS=${JOBS:-$(grep -c ^processor /proc/cpuinfo)}

if [[ -d $MODS_RUNSPACE ]]; then
    rm -rf "$MODS_RUNSPACE"/*
else
    rm -rf "$MODS_RUNSPACE"
    mkdir "$MODS_RUNSPACE"
fi

function callmake()
{
    echo make -r -j$JOBS "$@"
    nice make -r -j$JOBS "$@"
}

# Resolve any symbolic links in current path
cd -P .

echo "Building encrypted JavaScript engine"
BUILD_ARCH=$TARGET_ARCH callmake -C ../js1.7
callmake -C ../js1.7 jsc
BUILD_ARCH=$TARGET_ARCH callmake reghal

CONTRIB_DIR=$(callmake print_contrib_dir | grep '^Contrib:' | cut -d ':' -f 2)

echo "Compiling eud.js"
JSC_OPTS="-DINCLUDE_LWDA=true -DINCLUDE_GPU=true"
if [[ $TARGET_ARCH != ppc64le ]]; then
    JSC_OPTS="$JSC_OPTS -DINCLUDE_OGL=true"
fi
rm -f "$MODS_O_DIR"/bound.{bin,js}
cp gpu/js/eud.js "$MODS_O_DIR"/bound.js
chmod 644 "$MODS_O_DIR"/bound.js
cat gpu/js/random2d.js >> "$MODS_O_DIR"/bound.js
"$MODS_OUTPUT_DIR/$rel_o_dir"/jsc/jsc -b -I"$MODS_O_DIR" -Icore/js -Igpu/js -I"$CONTRIB_DIR" --input="$MODS_O_DIR"/bound.js --output="$MODS_O_DIR"/bound.bin $JSC_OPTS
export BOUND_JS="$MODS_O_DIR"/bound.bin
export INCLUDE_BYTECODE=true

if [[ ! -d "$MODS_O_DIR_GEN" ]]; then
    rm -rf "$MODS_O_DIR_GEN"
    mkdir -p "$MODS_O_DIR_GEN"
fi

echo "Building EUD"
if [[ $ISDVS = false ]]; then
    BUILD_ARCH=$TARGET_ARCH callmake build_all
else
    # In DVS we rely on prebuild components
    BUILD_ARCH=$TARGET_ARCH callmake
    BUILD_ARCH=$TARGET_ARCH callmake install
fi

if [[ $BUILD_CFG == release ]]; then
    ls "$MODS_RUNSPACE" | grep -v "${OTHER_GOOD_FILES}\|mods$" | while read FILE; do
        rm -f "$MODS_RUNSPACE"/$FILE
    done

    cp docs/enduserdiag.txt "$MODS_RUNSPACE"/README.txt
    cp tools/lwpu-diagnostic.sh "$MODS_RUNSPACE"

    COMPR=gzip
    type pigz >/dev/null 2>&1 && COMPR=pigz

    # We are in the process of removing string encryption and improving security
    # in MODS via other means.  Temporarily stubbing out the mods exelwtable in
    # release builds until we fix it.
    mv "$MODS_RUNSPACE"/mods ./mods.orig
    echo -e "#!/bin/bash\n\necho 'Not supported'" > "$MODS_RUNSPACE"/mods
    chmod 755 "$MODS_RUNSPACE"/mods

    tar --use-compress-program $COMPR -cf "$EUD_MODS_PACKAGE_NAME" -C "$MODS_RUNSPACE" $(ls "$MODS_RUNSPACE")

    EUD_TGZ="$PWD/$EUD_MODS_PACKAGE_NAME" callmake -C tools/eud_packaging clean
    EUD_TGZ="$PWD/$EUD_MODS_PACKAGE_NAME" callmake -C tools/eud_packaging install

    tar --use-compress-program $COMPR -cf "$LWVS_PACKAGE_NAME" -C "$MODS_RUNSPACE" liblwvs-diagnostic.so.1

    if [[ $ISDVS = true ]]; then
        mv mods.orig "$MODS_RUNSPACE"/mods # TODO remove this line
        rm -f "$MODS_RUNSPACE"/liblwvs-diagnostic.so.1
        # supply log file decryptor to use in PVS
        callmake submake.decrypt.all
        cp -f "$MODS_OUTPUT_DIR/$rel_o_dir"/decrypt/decrypt "$LW_CERT" "$MODS_RUNSPACE"
        tar --use-compress-program $COMPR -cf "$EUD_MODS_PACKAGE_NAME" -C "$MODS_RUNSPACE" $(ls "$MODS_RUNSPACE")
    fi
else
    callmake submake.decrypt.all
    cp -f "$MODS_OUTPUT_DIR/$rel_o_dir"/decrypt/decrypt "$LW_CERT" "$MODS_RUNSPACE"
fi
