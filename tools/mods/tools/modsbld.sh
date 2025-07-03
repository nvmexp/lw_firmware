#!/bin/bash

unset BUILD_OS
unset BUILD_CFG
unset BUILD_ARCH
unset BUILD_OS_SUBTYPE
unset INCLUDE_LWDA
unset INCLUDE_MDIAG
unset INCLUDE_OGL
unset INCLUDE_GPU
unset INCLUDE_RMTEST
unset INCLUDE_LWOGTEST
unset CHANGELIST
unset VERSION

PRETEND=""
[ "$JOBS" ] || JOBS=8

die()
{
    echo "$@"
    exit 1
}

build()
{
    BUILD="$1"
    ARCH=$(echo "$4"|cut -d "=" -f2)
    shift

    local MAKECMD
    MAKECMD="make -r -j $JOBS INCLUDE_LWDA=true CHANGELIST=$CHANGELIST VERSION=$VERSION"
    local QSUBCMD
    #QSUBCMD="qsub -m rel5  -q o_cpu_2G_15M --projectMode direct -P sw_inf_sw_mods -Is -n 12 time "
    [ -d "$VERSION/$BUILD" ] && die "Directory $VERSION/$BUILD is in the way!"
    $PRETEND mkdir "$VERSION/$BUILD" || exit $?

    if [[ "$BUILD" != "sim" && "$BUILD" != "sim_ppc64le" ]]; then
        $PRETEND export INCLUDE_ENCRYPT_INTERNAL=true
        $PRETEND rm -rf "$MODS_RUNSPACE"/* || exit $?
        $PRETEND $QSUBCMD $MAKECMD "$@" clean_all || exit $?
        $PRETEND $QSUBCMD $MAKECMD "$@" clobber_all || exit $?
        $PRETEND $QSUBCMD $MAKECMD "$@" build_all || exit $?
        $PRETEND $QSUBCMD $MAKECMD "$@" zip || exit $?
            
        $PRETEND mv $VERSION.* "$VERSION/$BUILD" || exit $?
    else
        $PRETEND unset INCLUDE_ENCRYPT_INTERNAL
        $PRETEND $QSUBCMD $MAKECMD "$@" clean_all || exit $?
        $PRETEND $QSUBCMD $MAKECMD "$@" clobber_all || exit $?
        $PRETEND $QSUBCMD $MAKECMD "$@" build_all || exit $?
        ( $PRETEND cd "$MODS_RUNSPACE" && $PRETEND tar czf "$VERSION.INTERNAL.tgz" * ) || exit $?
        $PRETEND mv "$MODS_RUNSPACE/$VERSION.INTERNAL.tgz" "$VERSION/$BUILD" || exit $?
    fi
}

if [ "$1" = "pretend" ]; then
    shift
    PRETEND="echo %"
fi

if [ $# -ne 2 ]; then
    echo "Usage: `basename $0` [pretend] CHANGELIST VERSION"
    echo
    exit 0
fi

CHANGELIST="$1"
VERSION="$2"

$PRETEND p4 sync ../../...@$CHANGELIST || exit $?
$PRETEND p4 sync //sw/mods/...@$CHANGELIST || exit $?

[ "$MODS_RUNSPACE" ] || die "Please set \$MODS_RUNSPACE! (Please use an absolute path)"
[[ ${MODS_RUNSPACE:0:1} = '/' ]] || die "\$MODS_RUNSPACE must be an absolute path!"
if [ ! -d "$MODS_RUNSPACE" ]; then
    if [ -e "$MODS_RUNSPACE" ]; then
        die "MODS_RUNSPACE $MODS_RUNSPACE is not a directory!"
    else
        mkdir "$MODS_RUNSPACE"
    fi
fi

[ -d "$VERSION" ] && die "Directory $VERSION is in the way!"
$PRETEND mkdir "$VERSION" || exit $?

build linux_x86_64     BUILD_OS=linuxmfg BUILD_CFG=release BUILD_ARCH=amd64
build sim              BUILD_OS=sim      BUILD_CFG=debug   BUILD_ARCH=amd64 INCLUDE_MDIAG=true
build linux_ppc64le    BUILD_OS=linuxmfg BUILD_CFG=release BUILD_ARCH=ppc64le
build sim_ppc64le      BUILD_OS=sim      BUILD_CFG=debug   BUILD_ARCH=ppc64le INCLUDE_MDIAG=true

