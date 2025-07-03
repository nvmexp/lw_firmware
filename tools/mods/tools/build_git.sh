#!/bin/bash

set -e

die() {
    echo "$@"
    exit 1
}

export PKG_OUT_DIR="${PKG_OUT_DIR:-${LW_OUTDIR:+$LW_OUTDIR/lwpu/mods}}"
[[ $TOP         ]] || die "TOP is not set!"
[[ $TEGRA_TOP   ]] || die "TEGRA_TOP is not set!"
[[ $P4ROOT      ]] || die "P4ROOT is not set!"
[[ $PKG_OUT_DIR ]] || die "PKG_OUT_DIR is not set!"
[[ $CHANGELIST  ]] || die "CHANGELIST is not set!"
[[ $VERSION     ]] || die "VERSION is not set!"

unset BUILD_OS_SUBTYPE
unset BUILD_ARCH
unset BUILD_CFG
unset INCLUDE_GPU
unset INCLUDE_MDIAG
unset INCLUDE_MDIAG_REFS
unset INCLUDE_LWOGTEST
unset INCLUDE_LWWATCH
unset INCLUDE_REFS
unset INCLUDE_RMTEST
unset INCLUDE_LWDA
unset INCLUDE_LWDART
unset INCLUDE_OGL
unset INCLUDE_VULKAN
unset SHOW_OUTPUT
unset TARGET_LWSTOMER
DO_CLEAN=0
ZIP_TARGET=zip
ONLY_MAKE=0
EXTRA_ARGS=

while [[ $# -gt 0 ]]; do
    case "$1" in
        clean)    DO_CLEAN=1              ;;
        verbose)  SHOW_OUTPUT=true        ;;
        linux)    BUILD_OS_SUBTYPE=linux  ;;
        nogpu)    INCLUDE_GPU=false       ;;
        softfp)   BUILD_ARCH=armv7        ;;
        aarch64)  BUILD_ARCH=aarch64      ;;
        release)  BUILD_CFG=release       ;;
        dev)      BUILD_CFG=sanity        ;;
        debug|modsdebug)
                  BUILD_CFG=debug ; EXTRA_ARGS="CAN_USE_GOLD=false" ;;
        internal|external|official)       ;;
        # TODO Rename option to better represent RM tests are included
        pvs)      ZIP_TARGET=zip_pvs ; INCLUDE_RMTEST=true ;;
        nosync)   ;; # Ignore, used only by build_mods.sh
        make)     ONLY_MAKE=1 # Useful to speed up debugging MODS changes
                  shift
                  break
                  ;;
        *)        die "Unrecognized arg - $1" ;;
    esac
    shift
done

export ENABLE_P4WHERE_CHECK=false
export BUILD_OS=cheetah
export BUILD_OS_SUBTYPE=${BUILD_OS_SUBTYPE:-linux}
export BUILD_ARCH=${BUILD_ARCH:-aarch64}
export BUILD_CFG=${BUILD_CFG:-release}
export INCLUDE_GPU=${INCLUDE_GPU:-true}
export SHOW_OUTPUT=${SHOW_OUTPUT:-false}
[[ $INCLUDE_GPU = true ]] && export INCLUDE_RMTEST=${INCLUDE_RMTEST:-false}
[[ $TARGET_LWSTOMER ]] && export TARGET_LWSTOMER
export MODS_RUNSPACE="$PKG_OUT_DIR/runspace"
export MODS_OUTPUT_DIR="$PKG_OUT_DIR/obj"
export BUILD_TOOLS_DIR="$P4ROOT/sw/tools"
export SHARED_FILES_DIR="$P4ROOT/sw/mods/shared_files"
export MODS_RMTEST_DIR="$P4ROOT/sw/mods/rmtest"
export SHARED_MODS_FILES_DIR="$P4ROOT/sw/mods"
export P4ROOT="$TEGRA_TOP/gpu" # Fake P4ROOT to avoid pulling wrong stuff
export MODS_DIR="$TEGRA_TOP/gpu/diag/mods"
export DRIVERS_DIR="$TEGRA_TOP/gpu/drv/drivers"
export SDK_DIR="$TEGRA_TOP/gpu/drv/sdk"
export CHIP_XML_DIR="$TEGRA_TOP/gpu/chipfuse/chipfuse"
export LINUX_DRIVER_SRC_PATH="$TOP/kernel/lwpu/drivers/misc/mods"
export LINUX_DRIVER_INC_PATH="$TOP/kernel/lwpu/include/uapi/misc"
export GLSLANG_PARENT_DIR="$TEGRA_TOP/gpu/apps-graphics/gpu/drivers/opengl/lwogtest/thirdparty/shaderc/third_party"
export INCLUDE_ALL_FUSE_FILES=false
export CHANGELIST="$CHANGELIST"
export VERSION="${VERSION////-}" # Replace all slashes with dashes
export IS_BRANCH_MODULAR=${IS_BRANCH_MODULAR:-false}
SCRIPTS_DIR="$TEGRA_TOP/tinylinux/target_scripts"
export MODS_ANDROID_SCRIPT="$TEGRA_TOP/tinylinux/host_scripts/mods_android"
export PVSTEST_MS="$SCRIPTS_DIR/pvstest_ms.js"
export TEGRAPVS_MS="$SCRIPTS_DIR/tegrapvs_ms.js"
export TEGRAFSI_SCRIPT_MS="$SCRIPTS_DIR/tegra_fsi.sh"
export TEGRAIST_SCRIPT_MS="$SCRIPTS_DIR/tegra_ist.sh"
export TESTFRM_SCRIPT_MS="$SCRIPTS_DIR/tegra_testfrm_ms.sh"
export TESTFRM_CFG_MS="$SCRIPTS_DIR/tegra_boards_ms.sh"
JOBS=$(grep -c processor /proc/cpuinfo)

rm -f "$PKG_OUT_DIR/$VERSION.tgz"
rm -f "$PKG_OUT_DIR/$VERSION.INTERNAL.tgz"
rm -f "$PKG_OUT_DIR/$VERSION.bypass.*bin"

cd "$MODS_DIR"

echo "Detected $JOBS processors, using that many jobs"

if [[ $ONLY_MAKE = 1 ]]; then
    exec make -j $JOBS -k $EXTRA_ARGS "$@"
fi

[[ $DO_CLEAN = 0 ]] || make -j $JOBS clean_all
make -j $JOBS $EXTRA_ARGS build_all
make -j $JOBS $EXTRA_ARGS $ZIP_TARGET
