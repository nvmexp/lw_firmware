#!/bin/bash

# Any subsequent commands which fail will cause the shell script to exit
# immediately
set -e

function die()
{
    echo "$@"
    exit 1
}

function usage()
{
    die "Usage: `basename $0` <SPECFILE> <VERSION> <CHANGELIST> [runspace=RUNSPACE] [buildmods] [dvs] [cleanall]"
}

if (($# < 3)); then
    usage
fi
SPECFILE="$1"
export VERSION="$2"
export CHANGELIST="$3"
BUILDMODS=false
ISDVS=false
CLEANALL=false
INSTALLONLY=false
if (($# > 3)); then
    shift 3
    while [[ -n $1 ]]; do
        case $1 in
            buildmods)   BUILDMODS=true ;;
            dvs)         ISDVS=true ;;
            cleanall)    CLEANALL=true ;;
            installonly) INSTALLONLY=true ;;
            *)           usage ;;
        esac
        shift
    done
fi

if [ "$SYMBOLIZE" = "true" ]; then
    prefix="S"
else
    prefix=""
fi

SPECFILEPATH="gpu/js"

# Guess STEALTH_MODE value from the command line. If it fails to recognize a
# familiar value, assign a "general" value of 3141592654.
if [ -z "$STEALTH_MODE" ]; then # Allow override
    case "$SPECFILE" in
             629.spc) export STEALTH_MODE="629" ;;
        griddiag.spc) export STEALTH_MODE="grid" ;;
        gridfull.spc) export STEALTH_MODE="gridfull" ;;
    dgxfielddiag.spc) export STEALTH_MODE="dgxfielddiag" ;;
                   *) export STEALTH_MODE="3141592654" ;;
    esac
fi

# Prepare environment variables for make
[ "$BUILD_OS" ] || export BUILD_OS="linuxmfg"
[ "$BUILD_OS" = "linuxmfg" -o "$BUILD_OS" = "linux" ] && [ -z "$BUILD_ARCH" ] && export BUILD_ARCH="amd64"
[ "$BUILD_OS" = "linux" ] && [ -z "$MODS_KERNEL_RM_VER_CHK" ] && export MODS_KERNEL_RM_VER_CHK="true"

export BUILD_CFG=${BUILD_CFG:-release}
export INCLUDE_LWDA="true"
export INCLUDE_MDIAG="false"
export MODS_OUTPUT_DIR=${MODS_OUTPUT_DIR:-$PWD/artifacts}
export ENCRYPT_JS="true"
export INCLUDE_ENCRYPT_INTERNAL="true"
declare -r rel_o_dir=$BUILD_CFG/$BUILD_OS/$BUILD_ARCH
declare -r JOBS=${JOBS:-1}
export INCLUDE_BOARDS_DB="false"

# Need to include to support Cask-based Linpack tests for 629.spc
if [[ "$STEALTH_MODE" = "629" || "$STEALTH_MODE" = "dgxfielddiag" ]]; then
    if [[ "$BUILD_ARCH" = "amd64" ]]; then
        export INCLUDE_LWDART="true"
    fi
fi

EXT=""

callmake()
{
    echo make -r -j $JOBS "$@"
    nice make -r -j $JOBS "$@"
}

# Resolve any symbolic links in current path
cd -P .

# Make sure no files are opened
#( cd ../.. && [ `p4 opened ./... 2> /dev/null | wc -l` -eq 0 ] ) || die "Please revert any local changes"

# Sync client to specified changelist
#( cd ../.. && p4 sync ./...@$CHANGELIST )

# Allow other files in the package
if [ "$STEALTH_MODE" = "629" ]; then
    declare -a other_good_files=(
        lwca.bin default.bin gv100_f.jsone tu102_f.jsone tu102_fpf.jsone tu104_f.jsone tu104_fpf.jsone version_checker
        install_module.sh multifieldiag.sh driver.tgz
    )
elif [ "$STEALTH_MODE" = "grid" ]; then
    declare -a other_good_files=(
        eccl120.bin eccsm20.bin dpstrs30.bin lwlrf30.bin lwgrf30.bin crnd30.bin
        ncmats30.bin cstrsf30.bin lwtool30.bin
        default.bin gk104_f.xme
    )
elif [ "$STEALTH_MODE" = "gridfull" ]; then
    declare -a other_good_files=(
        eccl120.bin eccsm20.bin pstrs30.bin lwlrf30.bin lwgrf30.bin crnd30.bin
        ncmats30.bin cstrsf30.bin lwtool30.bin
        default.bin gk104_f.xme gk107_f.xme
    )
elif [ "$STEALTH_MODE" = "dgxfielddiag" ]; then
    declare -a other_good_files=(
        lwca.bin default.bin install_module.sh
        driver.tgz lwdec.bin lwlinktopofiles.bin gv100_f.jsone svnp01_f.jsone tu102_f.jsone
        gp104_f.jsone fuseread_gp10x.bin libspirv.so librtcore.so
    )
else
    other_good_files=()
fi
if [ "$BUILD_CFG" = "debug" ]; then
    other_good_files+=(
        libdrvexport.so libmodsgl.so libamap.so librm.so libdramhal.so
    )
fi
if [[ $ISDVS == true ]]; then
    other_good_files+=(decrypt)
fi

# Here is what join does. First it prints the first element of the array with
# echo $1. Then printf "%s" "${@/#/$d}" prints all other elements prepended by
# $d. "${@/#/$d}" means "change the beginning to $d", # is an anchor to the
# beginning.
function join { local d=$1; shift; echo -n "$1"; shift; printf "%s" "${@/#/$d}"; }
OTHER_GOOD_FILES=$(join '\|' ${other_good_files[@]})

# Create runspace
export MODS_RUNSPACE="$PWD/runspace"
rm -rf "$MODS_RUNSPACE" || die "Unable to remove old runspace $MODS_RUNSPACE"
mkdir "$MODS_RUNSPACE" || die "Unable to create runspace $MODS_RUNSPACE"

contrib_dir=$(callmake print_contrib_dir | grep '^Contrib:' | cut -d ':' -f 2)

SPECFILEPATH="gpu/js"
[ "$SPECFILE" = "dgxfielddiag.spc" ] && SPECFILEPATH="$contrib_dir/dgx" 

# Make sure the specified spec file exists
[ -f "$SPECFILEPATH/$SPECFILE" ] || die "Spec file $SPECFILE does not exist in $SPECFILEPATH"

cp $SPECFILEPATH/$SPECFILE $MODS_RUNSPACE/stealth_end_user.spc

# Build encryption tool and create boundjs.h
callmake submake.encrypt.all BUILD_OS=$BUILD_OS BUILD_CFG=release
ENDUSER=gpu/js/stealth_end_user.js
if [[ "$STEALTH_MODE" = "dgxfielddiag" ]]; then
    ENDUSER=gpu/js/dgx_field_diag.js
fi

callmake submake.encrypt.clean BUILD_OS=$BUILD_OS BUILD_CFG=release

# Build MODS
if [[ $ISDVS == false && $INSTALLONLY == false ]]; then
    if [[ $CLEANALL == false ]]; then
        callmake clean_official_build
        callmake clean
    else
        callmake clean_all
    fi
fi

cp "$ENDUSER" "$MODS_RUNSPACE/bound.js"
cat gpu/js/random2d.js >> "$MODS_RUNSPACE/bound.js"
if [[ $ISDVS == false && $INSTALLONLY == false ]]; then
    callmake BOUND_JS="$MODS_RUNSPACE/bound.js" build_all
else
    # In DVS or if installonly has been specified, we rely on prebuild components
    callmake BOUND_JS="$MODS_RUNSPACE/bound.js"
    callmake BOUND_JS="$MODS_RUNSPACE/bound.js" install
fi
callmake BOUND_JS="$MODS_RUNSPACE/bound.js" zip
rm -rf "$MODS_RUNSPACE"

# Repack MODS build leaving only necessary files
mkdir "$MODS_RUNSPACE"
tar xzf "${prefix}$VERSION.tgz" -C "$MODS_RUNSPACE"
# Build a decryptor tool to pack for DVS. DVS needs decrypted logs.
if [[ $ISDVS == true ]]; then
    BUILD_DECRYPT=true callmake --directory=../encrypt
    cp $MODS_OUTPUT_DIR/$rel_o_dir/decrypt/decrypt $MODS_RUNSPACE
fi

#Build version checker utility, which gets copied to runspace dir
(cd ../version_checker && callmake clean BUILD_ARCH=$BUILD_ARCH BUILD_OS=$BUILD_OS BUILD_CFG=release &&
 callmake all BUILD_ARCH=$BUILD_ARCH BUILD_OS=$BUILD_OS BUILD_CFG=release)

ls "$MODS_RUNSPACE" | grep -v "${OTHER_GOOD_FILES}\|mods${EXT}$" | while read FILE; do
    rm -f "$MODS_RUNSPACE/$FILE"
done

[ "$STEALTH_MODE" = "629" ] && mv "$MODS_RUNSPACE/mods$EXT" "$MODS_RUNSPACE/fieldiag$EXT"
[ "$STEALTH_MODE" = "grid" ] && mv "$MODS_RUNSPACE/mods$EXT" "$MODS_RUNSPACE/griddiag$EXT"
[ "$STEALTH_MODE" = "gridfull" ] && mv "$MODS_RUNSPACE/mods$EXT" "$MODS_RUNSPACE/griddiag_full$EXT"
[ "$STEALTH_MODE" = "dgxfielddiag" ] && mv "$MODS_RUNSPACE/mods$EXT" "$MODS_RUNSPACE/fieldiag$EXT"

if [ "$STEALTH_MODE" = "grid" ]; then
    cp tools/grid-diagnostic.sh "$MODS_RUNSPACE"
fi

( cd "$MODS_RUNSPACE" && tar czf "${prefix}$VERSION.tgz" * )
mv "$MODS_RUNSPACE/${prefix}$VERSION.tgz" .

# Speedo and bypass are useless with the 629 (cannot import them), remove them
# so they dont get copied by the build system
rm "$VERSION.bypass.bin"
rm "$VERSION.bypass.INTERNAL.bin"
# speedo.jse is not generated as part of aarch64 builds so remove file if present
[[ -f "$VERSION.speedo.jse" ]] && rm "$VERSION.speedo.jse"

# With the merged packages the unbound version of the fieldiag needs to be called
# something else otherwise the package name will conflict with the bound package
# name
MODSBUILT=false
if [[ $ISDVS == false && $BUILDMODS == true ]]; then
    export STEALTH_MODE=""
    callmake clean
    callmake VERSION="$VERSION.UNBOUND" build_all
    callmake VERSION="$VERSION.UNBOUND" zip
    MODSBUILT=true
fi

# Add spec file to engineering MODS
if [[ $MODSBUILT == true && $BUILD_OS == linuxmfg ]]; then
    gunzip < "${prefix}$VERSION.UNBOUND.tgz" > "${prefix}$VERSION.UNBOUND.tar"
    cp $SPECFILEPATH/$SPECFILE $MODS_RUNSPACE/stealth_end_user.spc
    ( cd "$MODS_RUNSPACE" && tar rf "../${prefix}$VERSION.UNBOUND.tar" stealth_end_user.spc )
    gzip < "${prefix}$VERSION.UNBOUND.tar" > "${prefix}$VERSION.UNBOUND.tgz"
    rm -f "${prefix}$VERSION.UNBOUND.tar"
fi

rm -rf "$MODS_RUNSPACE"
