#! /bin/sh

die() {
    echo "$@"
    exit 1
}

mycompress() {
    case "$BUILD_OS" in
    linuxmfg)
        ( cd "$MODS_RUNSPACE" && tar czf "$VERSION.tgz" * ) || exit $?
        ;;
    *)
        die "Unsupported BUILD_OS $BUILD_OS"
        ;;
    esac
}

# Make sure the user passed a changelist
[ $# -eq 5 ] || die "Usage: `basename $0` <VERSION> <CHANGELIST> <BUILD_OS> <JOBS> <ENDUSER.JS>"

# Prepare environment variables for make
export BUILD_OS="$3"
ARCH=x86
if [ "$BUILD_OS" = "linuxmfg" ]; then
    export BUILD_ARCH=amd64
    ARCH=amd64
fi
export BUILD_CFG="release"
export VERSION="$1"
export CHANGELIST="$2"
export MODS_OUTPUT_DIR="$PWD/artifacts"
JOBS="-j $4"
ENDUSER="$5"
[ "$ENDUSER" = "lwdauser.js" ] && export INCLUDE_LWDA="true"

# Resolve any symbolic links in current path
cd -P .

# Make sure no files are opened
( cd ../.. && [ `p4 opened ./... 2> /dev/null | wc -l` -eq 0 ] ) || die "Please revert any local changes"

# Sync client to specified changelist
( cd ../.. && p4 sync ./...@$CHANGELIST ) || exit $?

# Make sure we have the enduser script
[ -f "gpu/js/$ENDUSER" ] || die "File gpu/js/$ENDUSER not found"

# Create runspace
export MODS_RUNSPACE="$PWD/runspace"
rm -rf "$MODS_RUNSPACE" || exit $?
mkdir "$MODS_RUNSPACE" || exit $?

# Clean all
nice make -r -s clean_all || exit $?
nice make -r -s clobber_all || exit $?
rm -rf "$BUILD_OS"/boundjs.h

# Build MODS
nice make -r -s $JOBS build_all COMPRESS_REL=false || exit $?

# Build encrypt tool and create boundjs.h
nice make -r -s submake.encrypt.all BUILD_OS=linuxmfg BUILD_ARCH=x86 || exit $?
cp gpu/js/$ENDUSER "$MODS_RUNSPACE"/ || exit $?
( cd "$MODS_RUNSPACE" && ../artifacts/release/linux/x86/encrypt/encrypt -b $ENDUSER boundjs.h ) || exit $?

# Build official MODS
make -r -s clean_official_build || exit $?
nice make -r -s clean || exit $?
cp "$MODS_RUNSPACE"/boundjs.h "$BUILD_OS"/ || exit $?
nice make -r -s $JOBS BOUND_JS=gpu/js/$ENDUSER build_all || exit $?
rm -rf "$MODS_RUNSPACE" || exit $?
nice make -r -s $JOBS BOUND_JS=gpu/js/$ENDUSER zip || exit $?

# Repack and leave only necessary files
case "$BUILD_OS" in
linuxmfg)
    tar xzf "$VERSION.tgz" -C "$MODS_RUNSPACE" || exit $?
    ;;
*)
    die "Unsupported BUILD_OS $BUILD_OS"
    ;;
esac
ls "$MODS_RUNSPACE" | grep -v "mods$\|mods\.exe\|install_module.sh\|driver.tgz\|default.bin" | while read FILE; do
    rm -f "$MODS_RUNSPACE/$FILE"
done || exit $?
if [ "$ENDUSER" = "lwdauser.js" ]; then
    cp docs/lwdauser.txt "$MODS_RUNSPACE/README"
    cp gpu/tests/traces/{lrf,grf,dgemm}.bin "$MODS_RUNSPACE/"
fi
mycompress
[ "$BUILD_OS" = "linuxmfg" ] && FILE="$VERSION.tgz"
mv "$MODS_RUNSPACE/$FILE" . || exit $?
rm -rf "$MODS_RUNSPACE"

# Inform the user that the build has completed
echo
echo "$FILE completed!"
