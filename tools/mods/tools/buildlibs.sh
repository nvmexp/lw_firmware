#!/bin/bash
#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#
#
#
# Recompile a bunch of mods libraries that we submit to Perforce.
#
# This script can run as-is on Linux and Mac.
# When running this script on Windows, set: SHELLOPTS=igncr

die()
{
    echo "$@"
    exit 1
}

# Run a command and print results to stdout.  Called from runcmd().
doruncmd()
{
    local TEE=""
    if [[ $1 = tee ]]; then
        TEE="$2"
        shift 2
    fi

    echo "----------------------------------------------------------------------"
    echo "% $@"
    echo

    if [[ -z $TEE ]]; then
        "$@" 2>&1
    else
        "$@" 2>&1 | tee "$TEE"
    fi

    RETVAL=$?
    if [ $RETVAL -ne 0 ]; then
        echo
        echo "Command failed with exit code $RETVAL"
    fi
    return $RETVAL
}

runcmd()
{
    local BUILDLOG
    BUILDLOG="$MODSDIR/build.log"

    local RETVAL

    if [ "$PRETEND" ]; then
        [[ $1 = tee ]] && shift 2
        echo "% $@"
    elif [[ $USE_STDOUT ]]; then
        doruncmd "$@"
    else
        doruncmd "$@" >> "$BUILDLOG"
    fi

    RETVAL=$?
    if [ $RETVAL -ne 0 ]; then
        [ "$DVS" ] || echo "Command failed, check build.log for errors"
        exit $RETVAL
    fi
}

banner()
{
    [[ $DVS ]] && echo "######################################################################"
    [[ $DVS ]] && echo "######################################################################"
    [[ $DVS ]] && echo -n "####            "
    echo "$@"
    [[ $DVS ]] && echo "######################################################################"
    [[ $DVS ]] && echo "######################################################################"
    [[ $DVS ]] && echo
}

buildlib()
{
    local LIBDIR
    LIBDIR="$1"
    shift

    local LIBNAME
    LIBNAME="$1"
    shift

    local LIBBASENAME
    LIBBASENAME="lib$LIBNAME"
    [[ $LIBNAME =~ \.lib$ ]] && LIBBASENAME="${LIBNAME%.lib}"

    if echo "$SKIPLIST" | grep -q "\<${LIBNAME}\>"; then
        echo "Skipping ${LIBNAME}"
        return
    fi

    local BUILD_CFGS
    if echo "$DEBUGLIST" | grep -q "\<${LIBNAME}\>"; then
        BUILD_CFGS="release debug"
    else
        BUILD_CFGS="release"
        [[ $SANITIZER ]] && BUILD_CFGS="debug"
    fi

    local BUILD_CFG
    for BUILD_CFG in $BUILD_CFGS; do
        local DESTSUBDIR
        if [[ "$BUILD_CFG" = "$BUILD_CFGS" ]]; then
            DESTSUBDIR=""
        else
            DESTSUBDIR="/$BUILD_CFG"
        fi

        banner "Building ${LIBNAME}${DESTSUBDIR}..."

        # Build library
        if [[ "$UNAME" = "MSYS" ]]; then
            local OLDPATH
            OLDPATH="$PATH"
            runcmd export PATH="$WIN_TOOLS\win32\gnumake\mingw32-4.3:$WIN_TOOLS\win32\MiscBuildTools"
            runcmd mingw32-make.exe -C "$LIBDIR" BUILD_CFG=$BUILD_CFG $BUILDARGS
            runcmd export PATH="$OLDPATH"
        else
            runcmd make -C "$LIBDIR" BUILD_CFG=$BUILD_CFG $BUILDARGS
        fi

        # Copy library to destination
        if [[ ! -d "$DESTDIR$DESTSUBDIR" ]]; then
            if [[ -f "$DESTDIR$DESTSUBDIR" ]]; then
                runcmd rm -f "$DESTDIR$DESTSUBDIR"
            fi
            runcmd mkdir -p "$DESTDIR$DESTSUBDIR"
        fi
        local EXT
        local ALTEXT
        local LIBFILE
        EXT="$LIBEXT"
        ALTEXT=so
        [[ $EXT = lib ]] && ALTEXT=dll
        LIBFILE="$BUILDOUTPUT/$BUILD_CFG/$ODIR/${LIBBASENAME}/${LIBBASENAME}"
        [[ ! -f "$LIBFILE.$LIBEXT" && -f "$LIBFILE.$ALTEXT" ]] && EXT=$ALTEXT
        runcmd cp "$LIBFILE.$EXT" "$DESTDIR$DESTSUBDIR"
    done
}

extract_arg()
{
    local VALUE="${BUILDARGS##*$1=}"
    echo "${VALUE%% *}"
}

buildlib_lwmake()
{
    local LIBDIR
    LIBDIR="$1"
    shift

    local LIBNAME
    LIBNAME="$1"
    shift

    local LIBBASENAME
    LIBBASENAME="lib$LIBNAME"
    [[ $LIBNAME =~ \.lib$ ]] && LIBBASENAME="${LIBNAME%.lib}"

    if echo "$SKIPLIST" | grep -q "\<${LIBNAME}\>"; then
        echo "Skipping ${LIBNAME}"
        return
    fi

    local BUILD_CFGS
    if echo "$DEBUGLIST" | grep -q "\<${LIBNAME}\>"; then
        BUILD_CFGS="release debug"
    else
        BUILD_CFGS="release"
        [[ $SANITIZER ]] && BUILD_CFGS="debug"
    fi

    local BUILD_OS=$(extract_arg BUILD_OS)
    if [[ $BUILD_OS = cheetah || $BUILD_OS = linuxmfg ]]; then
        local BUILD_OS_SUBTYPE=$(extract_arg BUILD_OS_SUBTYPE)
        case "$BUILD_OS_SUBTYPE" in
            qnx)      BUILD_OS=qnx ;;
        esac
    fi

    local OUTPUT_ROOT="$BUILDOUTPUT"
    if type -t cygpath >/dev/null 2>&1; then
        OUTPUT_ROOT="$(cygpath -m "$OUTPUT_ROOT")"
    fi

    local LWMAKE="$(cd -P . && pwd)/tools/lwmake"
    local LWMAKE_BUILDARGS=(
        "LW_MODS=$BUILD_OS"
        "$(extract_arg BUILD_ARCH)"
        "LW_OUTPUT_ROOT=$OUTPUT_ROOT"
        "-j$JOBS"
        )
    [[ "$SHOW_OUTPUT" = "true" ]] && LWMAKE_BUILDARGS+=("verbose")
    [[ $SANITIZER ]] && LWMAKE_BUILDARGS+=( "LW_SANITIZER=$SANITIZER" )

    local BUILD_CFG
    for BUILD_CFG in $BUILD_CFGS; do
        local DESTSUBDIR
        if [[ "$BUILD_CFG" = "$BUILD_CFGS" ]]; then
            DESTSUBDIR=""
        else
            DESTSUBDIR="/$BUILD_CFG"
        fi

        banner "Building ${LIBNAME}${DESTSUBDIR}..."

        # Build library
        runcmd cd "$LIBDIR"
        runcmd "$LWMAKE" $BUILD_CFG "${LWMAKE_BUILDARGS[@]}"
        runcmd cd -

        # Copy library to destination
        if [[ ! -d "$DESTDIR$DESTSUBDIR" ]]; then
            if [[ -f "$DESTDIR$DESTSUBDIR" ]]; then
                runcmd rm -f "$DESTDIR$DESTSUBDIR"
            fi
            runcmd mkdir -p "$DESTDIR$DESTSUBDIR"
        fi
        local EXT
        local ALTEXT
        local LIBFILE
        EXT="$LIBEXT"
        ALTEXT=so
        [[ $EXT = lib ]] && ALTEXT=dll
        local LWMAKE_OUT_DIR="${BUILD_OS}_$(extract_arg BUILD_ARCH)_$BUILD_CFG"
        local LIBSRCPATH="${LIBDIR/../diag}"
        LIBFILE="$BUILDOUTPUT/$LWMAKE_OUT_DIR/$LIBSRCPATH/${LIBBASENAME}"
        [[ ! -f "$LIBFILE.$LIBEXT" && -f "$LIBFILE.$ALTEXT" ]] && EXT=$ALTEXT
        runcmd cp "$LIBFILE.$EXT" "$DESTDIR$DESTSUBDIR"
    done
}

# We build mods.lib twice.  Once we build it normally with SERVER_MODE defaulting
# to false and here we build it with SERVER_MODE set to true.  Unlike in the
# default build, we change all the directory locations so that the builds
# do not clash with each other.  We rename the name of the resulting lib when
# copying it into its final place.
buildmodsdlllib()
{
    if echo "$SKIPLIST" | grep -q "\<mods.lib\>"; then
        echo "Skipping mods_dll.lib"
        return
    fi

    local SAVED_BUILDARGS
    local SAVED_BUILDOUTPUT
    local SAVED_DESTDIR
    local TARGET_DIR

    SAVED_BUILDARGS="$BUILDARGS"
    SAVED_BUILDOUTPUT="$BUILDOUTPUT"
    SAVED_DESTDIR="$DESTDIR"

    BUILDOUTPUT="${BUILDOUTPUT}/modsdll"
    BUILDARGS=$(echo "$BUILDARGS" | sed 's/MODS_OUTPUT_DIR=[^ ]*//')
    BUILDARGS="$BUILDARGS SERVER_MODE=true MODS_OUTPUT_DIR=$BUILDOUTPUT"
    DESTDIR="${BUILDOUTPUT}/lib"
    TARGET_DIR="$DESTDIR"

    buildlib win32/modslib mods.lib

    BUILDARGS="$SAVED_BUILDARGS"
    BUILDOUTPUT="$SAVED_BUILDOUTPUT"
    DESTDIR="$SAVED_DESTDIR"

    runcmd cp "$TARGET_DIR/mods.lib" "$DESTDIR/mods_dll.lib"
}

buildzlib()
{
    local LIBNAME
    LIBNAME="z"

    if echo "$SKIPLIST" | grep -q "\<${LIBNAME}\>"; then
        echo "Skipping ${LIBNAME}"
        return
    fi

    local BUILD_CFGS
    if echo "$DEBUGLIST" | grep -q "\<${LIBNAME}\>"; then
        BUILD_CFGS="release debug"
    else
        BUILD_CFGS="release"
        [[ $SANITIZER ]] && BUILD_CFGS="debug"
    fi

    local BUILD_CFG
    for BUILD_CFG in $BUILD_CFGS; do
        local DESTSUBDIR
        if [[ "$BUILD_CFG" = "$BUILD_CFGS" ]]; then
            DESTSUBDIR=""
        else
            DESTSUBDIR="/$BUILD_CFG"
        fi

        banner "Building ${LIBNAME}${DESTSUBDIR}..."

        local COMPILER
        local TARGET
        local MYCC
        local MYCFLAGS
        local MYOBJA
        local MYPATHS
        local MAKECMD
        local MAKEFILE
        local NO_UNISTD
        local SRCLIBNAME
        SRCLIBNAME="lib$LIBNAME.$LIBEXT"
        case "$BUILDTYPE" in
            linux_aarch64)
                COMPILER="$TOOLS/linux/mods/gcc-7.2.0-cross-aarch64-on-x86_64/bin"
                TARGET="aarch64-unknown-linux-gnu"
                CROSS_PREFIX="$TARGET"
                MAKECMD="make -j $JOBS"
                ;;
            linux_amd64_gcc73)
                COMPILER="$TOOLS/linux/mods/gcc-7.3.0-x86_64/bin"
                TARGET="x86_64-pc-linux-gnu"
                MAKECMD="make -j $JOBS"
                MYCFLAGS="-ffunction-sections -fdata-sections -fPIC --sysroot $COMPILER/.."
                [[ $ENABLE_LTO ]] && MYCFLAGS+=" -flto"
                ;;
            linux_ppc64le)
                COMPILER="$TOOLS/linux/mods/gcc-9.1.0-cross-ppc64le-on-x86_64/bin"
                TARGET="powerpc64le-unknown-linux-gnu"
                MAKECMD="make -j $JOBS"
                ;;
            macosx_x86)
                COMPILER="/usr/bin"
                TARGET="i386-apple-Darwin"
                MYCFLAGS="-arch i386 -march=pentium4"
                MAKECMD="make -j $JOBS"
                ;;
            macosx_amd64)
                COMPILER="/usr/bin"
                TARGET="x86_64-apple-Darwin"
                MYCFLAGS="-arch x86_64"
                MAKECMD="make -j $JOBS"
                ;;
            qnx_aarch64)
                runcmd export QNX_HOST="$TOOLS/embedded/qnx/qnx710-ga1/host/linux/x86_64"
                runcmd export QNX_TARGET="$TOOLS/embedded/qnx/qnx710-ga1/target/qnx7"
                COMPILER="$TOOLS/embedded/qnx/qnx710-ga1/host/linux/x86_64/usr/bin"
                TARGET="aarch64-unknown-nto-qnx7.1.0"
                CROSS_PREFIX="$TARGET"
                MAKECMD="make -j $JOBS"
                MYCFLAGS="-I$QNX_TARGET/usr/include"
                ;;
            win32_amd64)
                COMPILER="$WIN_TOOLS/win32/msvc141u8/VC/bin/amd64"
                MYCFLAGS="-D_LARGEFILE64_SOURCE=1 -D_LFS64_LARGEFILE=1 -I$TOOLS/win32/msvc141u8/VC/include -I$TOOLS/ddk/wddmv2/official/18362/Include/10.0.18362.0/ucrt -nologo -O2 -Oy- #"
                case "$BUILD_CFG" in
                    release) MYCFLAGS="-MT  $MYCFLAGS";;
                    debug)   MYCFLAGS="-MTd $MYCFLAGS";;
                esac
                MYOBJA="inffast.obj"
                MAKECMD="$WIN_TOOLS/win32/msvc141u8/VC/bin/amd64/nmake"
                MAKEFILE=win32/Makefile.msc
                NO_UNISTD=1
                SRCLIBNAME=zlib.lib
                MYPATHS="$MYPATHS:$WIN_TOOLS/win32/msvc141u8/VC/bin/amd64"
                MYPATHS="$MYPATHS:$WIN_TOOLS/win32/msvc141u8/bin-support/amd64"
                MYPATHS="$MYPATHS:$WIN_TOOLS/win32/msvc141u8/VC/redist/amd64/Microsoft.VC141.CRT"
                ;;
            win32_x86)
                COMPILER="$WIN_TOOLS/win32/msvc141u8/VC/bin/amd64_x86"
                MYCFLAGS="-D_LARGEFILE64_SOURCE=1 -D_LFS64_LARGEFILE=1 -I$TOOLS/win32/msvc141u8/VC/include -I$TOOLS/ddk/wddmv2/official/18362/Include/10.0.18362.0/ucrt -nologo -O2 -Oy- #"
                case "$BUILD_CFG" in
                    release) MYCFLAGS="-MT  $MYCFLAGS";;
                    debug)   MYCFLAGS="-MTd $MYCFLAGS";;
                esac
                MYOBJA="inffast.obj"
                MAKECMD="$WIN_TOOLS/win32/msvc141u8/VC/bin/amd64/nmake"
                MAKEFILE=win32/Makefile.msc
                NO_UNISTD=1
                SRCLIBNAME=zlib.lib
                MYPATHS="$MYPATHS:$WIN_TOOLS/win32/msvc141u8/VC/bin/amd64"
                MYPATHS="$MYPATHS:$WIN_TOOLS/win32/msvc141u8/bin-support/amd64"
                MYPATHS="$MYPATHS:$WIN_TOOLS/win32/msvc141u8/VC/redist/amd64/Microsoft.VC141.CRT"
                ;;
            *)
                die "Unsupported build type - $BUILDTYPE"
                ;;
        esac

        [[ $SANITIZER && $SANITIZER != none ]] && MYCFLAGS="$MYCFLAGS -fsanitize=$SANITIZER"

        local OLDPATH
        OLDPATH="$PATH"

        local SRCLIBDIR
        SRCLIBDIR="zlib-1.2.11"

        # Make a copy of the source directory
        runcmd rm -rf "$BUILDOUTPUT/zlib"
        runcmd cp -a "$TOOLS/$SRCLIBDIR" "$BUILDOUTPUT/zlib"

        # Build the library
        runcmd cd "$BUILDOUTPUT/zlib"
        runcmd chmod u+w zconf.h.in zconf.h Makefile
        runcmd export PATH="$COMPILER$MYPATHS:$PATH"
        runcmd export CHOST="$TARGET"
        if [[ -n "$MAKEFILE" ]]; then
            cp "$MAKEFILE" Makefile
            runcmd sed -i -e "s!^CFLAGS *=!CFLAGS=$MYCFLAGS !" Makefile
            runcmd sed -i -e "s!^SFLAGS *=!SFLAGS=$MYCFLAGS !" Makefile
            runcmd sed -i -e "s!^OBJA *=!OBJA=$MYOBJA !" Makefile
        else
            [[ "$CROSS_PREFIX" ]] && export CROSS_PREFIX="${CROSS_PREFIX}-"
            local CC
            [[ "$MYCC" ]] && export CC="$MYCC"
            runcmd elw CFLAGS="$MYCFLAGS" sh ./configure
        fi
        if [[ "$NO_UNISTD" ]]; then
            runcmd sed -i -e 's/^ *# *include .unistd.h.*$/#define off64_t long long\n#define lseek64 _lseeki64/' zconf.h
        fi
        runcmd $MAKECMD "$SRCLIBNAME"
        runcmd export PATH="$OLDPATH"

        # Copy built library to destination
        runcmd cd "$MODSDIR"
        runcmd cp "$BUILDOUTPUT/zlib/$SRCLIBNAME" "$DESTDIR$DESTSUBDIR/lib$LIBNAME.$LIBEXT"
    done
}

buildpng()
{
    local LIBNAME
    LIBNAME="png"

    if echo "$SKIPLIST" | grep -q "\<${LIBNAME}\>"; then
        echo "Skipping ${LIBNAME}"
        return
    fi

    local BUILD_CFGS
    if echo "$DEBUGLIST" | grep -q "\<${LIBNAME}\>"; then
        BUILD_CFGS="release debug"
    else
        BUILD_CFGS="release"
        [[ $SANITIZER ]] && BUILD_CFGS="debug"
    fi

    local BUILD_CFG
    for BUILD_CFG in $BUILD_CFGS; do
        local DESTSUBDIR
        if [[ "$BUILD_CFG" = "$BUILD_CFGS" ]]; then
            DESTSUBDIR=""
        else
            DESTSUBDIR="/$BUILD_CFG"
        fi

        banner "Building ${LIBNAME}${DESTSUBDIR}..."

        local COMPILER
        local TARGET
        local MYCC
        local MYRANLIB
        local MYCFLAGS
        local MYPATHS
        local MAKECMD
        local MAKEFILE
        case "$BUILDTYPE" in
            linux_aarch64)
                COMPILER="$TOOLS/linux/mods/gcc-7.2.0-cross-aarch64-on-x86_64/bin"
                TARGET="aarch64-unknown-linux-gnu"
                MYCC="$TARGET-gcc"
                MYRANLIB="$TARGET-ranlib"
                MAKECMD="make -j $JOBS"
                MAKEFILE="scripts/makefile.linux"
                ;;
            linux_amd64_gcc73)
                COMPILER="$TOOLS/linux/mods/gcc-7.3.0-x86_64/bin"
                TARGET="x86_64-pc-linux-gnu"
                MYCC="$TARGET-gcc"
                MYRANLIB="$COMPILER/../x86_64-pc-linux-gnu/bin/ranlib"
                MAKECMD="make -j $JOBS"
                MAKEFILE="scripts/makefile.linux"
                MYCFLAGS="-ffunction-sections -fdata-sections -fPIC --sysroot $COMPILER/.."
                [[ $ENABLE_LTO ]] && MYCFLAGS+=" -flto"
                ;;
            linux_ppc64le)
                COMPILER="$TOOLS/linux/mods/gcc-9.1.0-cross-ppc64le-on-x86_64/bin"
                TARGET="powerpc64le-unknown-linux-gnu"
                MYCC="$TARGET-gcc"
                MYRANLIB="$TARGET-ranlib"
                MAKECMD="make -j $JOBS"
                MAKEFILE="scripts/makefile.linux"
                ;;
            macosx_x86)
                MYCC="/usr/bin/gcc"
                MYRANLIB="/usr/bin/ranlib"
                MYCFLAGS="-arch i386 -march=pentium4"
                MAKECMD="make -j $JOBS"
                MAKEFILE="scripts/makefile.darwin"
                ;;
            macosx_amd64)
                MYCC="/usr/bin/gcc"
                MYRANLIB="/usr/bin/ranlib"
                MYCFLAGS="-arch x86_64"
                MAKECMD="make -j $JOBS"
                MAKEFILE="scripts/makefile.darwin"
                ;;
            qnx_aarch64)
                runcmd export QNX_HOST="$TOOLS/embedded/qnx/qnx710-ga1/host/linux/x86_64"
                runcmd export QNX_TARGET="$TOOLS/embedded/qnx/qnx710-ga1/target/qnx7"
                COMPILER="$TOOLS/embedded/qnx/qnx710-ga1/host/linux/x86_64/usr/bin"
                TARGET="aarch64-unknown-nto-qnx7.1.0"
                MYCC="$TARGET-gcc"
                MYRANLIB="$TARGET-ranlib"
                MAKECMD="make -j $JOBS"
                MAKEFILE="scripts/makefile.linux"
                ;;
            win32_amd64)
                COMPILER="$WIN_TOOLS/win32/msvc141u8/VC/bin/amd64"
                MYCC="cl"
                MYCFLAGS="-I$TOOLS/win32/msvc141u8/VC/include -I$TOOLS/ddk/wddmv2/official/18362/Include/10.0.18362.0/ucrt -nologo -DPNG_NO_MMX_CODE -O2 -W3 -I../zlib #"
                case "$BUILD_CFG" in
                    release) MYCFLAGS="-MT  $MYCFLAGS";;
                    debug)   MYCFLAGS="-MTd $MYCFLAGS";;
                esac
                MAKECMD="$WIN_TOOLS/win32/msvc141u8/VC/bin/amd64/nmake"
                MAKEFILE="scripts/makefile.vcwin32"
                MYPATHS="$MYPATHS:$WIN_TOOLS/win32/msvc141u8/VC/bin/amd64"
                MYPATHS="$MYPATHS:$WIN_TOOLS/win32/msvc141u8/bin-support/amd64"
                MYPATHS="$MYPATHS:$WIN_TOOLS/win32/msvc141u8/VC/redist/amd64/Microsoft.VC141.CRT"
                ;;
            win32_x86)
                COMPILER="$WIN_TOOLS/win32/msvc141u8/VC/bin/amd64_x86"
                MYCC="cl"
                MYCFLAGS="-I$TOOLS/win32/msvc141u8/VC/include -I$TOOLS/ddk/wddmv2/official/18362/Include/10.0.18362.0/ucrt -nologo -DPNG_NO_MMX_CODE -O2 -W3 -I../zlib #"
                case "$BUILD_CFG" in
                    release) MYCFLAGS="-MT  $MYCFLAGS";;
                    debug)   MYCFLAGS="-MTd $MYCFLAGS";;
                esac
                MAKECMD="$WIN_TOOLS/win32/msvc141u8/VC/bin/amd64/nmake"
                MAKEFILE="scripts/makefile.vcwin32"
                MYPATHS="$MYPATHS:$WIN_TOOLS/win32/msvc141u8/VC/bin/amd64"
                MYPATHS="$MYPATHS:$WIN_TOOLS/win32/msvc141u8/bin-support/amd64"
                MYPATHS="$MYPATHS:$WIN_TOOLS/win32/msvc141u8/VC/redist/x86/Microsoft.VC141.CRT"
                ;;
            *)
                die "Unsupported build type - $BUILDTYPE"
                ;;
        esac

        [[ $SANITIZER && $SANITIZER != none ]] && MYCFLAGS="$MYCFLAGS -fsanitize=$SANITIZER"

        local OLDPATH
        OLDPATH="$PATH"

        local SRCLIBDIR
        SRCLIBDIR="lpng1251"

        local ZLIBDIR
        ZLIBDIR="zlib-1.2.11"

        # Make a copy of the source directory
        runcmd rm -rf "$BUILDOUTPUT/$SRCLIBDIR"
        runcmd cp -a "$TOOLS/$SRCLIBDIR" "$BUILDOUTPUT/$SRCLIBDIR"

        # Make a copy of zlib
        runcmd rm -rf "$BUILDOUTPUT/zlib"
        runcmd cp -a "$TOOLS/$ZLIBDIR" "$BUILDOUTPUT/zlib"

        # Build the library
        runcmd cd "$BUILDOUTPUT/$SRCLIBDIR"
        runcmd export PATH="$COMPILER$MYPATHS:$PATH"
        runcmd sed -i -e "s!^CC=.*!CC=\"$MYCC\"!" "$MAKEFILE"
        runcmd sed -i -e "s!^RANLIB *=.*!RANLIB=\"$MYRANLIB\"!" "$MAKEFILE"
        runcmd sed -i -e "s!^CFLAGS *=!CFLAGS=$MYCFLAGS !" "$MAKEFILE"
        runcmd $MAKECMD -f "$MAKEFILE" "lib${LIBNAME}.$LIBEXT"
        runcmd export PATH="$OLDPATH"

        # Copy built library to destination
        runcmd cd "$MODSDIR"
        runcmd cp "$BUILDOUTPUT/$SRCLIBDIR/lib${LIBNAME}.$LIBEXT" "$DESTDIR$DESTSUBDIR"
    done
}

buildelf()
{
    local LIBNAME
    LIBNAME="elf"

    if echo "$SKIPLIST" | grep -q "\<${LIBNAME}\>"; then
        echo "Skipping ${LIBNAME}"
        return
    fi

    banner "Building ${LIBNAME}..."

    if ! type -t msgfmt >/dev/null 2>&1; then
        echo "Error: Missing msgfmt utility, install gettext package on your system!"
        exit 1
    fi

    local COMPILER
    local TARGET
    local MYCC
    local MYAR
    local MYRANLIB
    local MYCFLAGS
    local MYLDFLAGS
    local MAKEFILE
    case "$BUILDTYPE" in
        linux_amd64_gcc73)
            COMPILER="$TOOLS/linux/mods/gcc-7.3.0-x86_64/bin"
            TARGET="x86_64-pc-linux-gnu"
            MYCC="$COMPILER/$TARGET-gcc"
            MYAR="$COMPILER/$TARGET-gcc-ar"
            MYRANLIB="/usr/bin/ranlib"
            MAKEFILE="scripts/makefile.linux"
            MYCFLAGS="-fPIC --sysroot $COMPILER/.."
            MYLDFLAGS="$MYCFLAGS"
            ;;
        linux_ppc64le)
            COMPILER="$TOOLS/linux/mods/gcc-9.1.0-cross-ppc64le-on-x86_64/bin"
            TARGET="powerpc64le-unknown-linux-gnu"
            MYCC="$COMPILER/$TARGET-gcc"
            MYAR="$COMPILER/$TARGET-ar"
            MYRANLIB="$COMPILER/$TARGET-ranlib"
            MAKEFILE="scripts/makefile.linux"
            ;;
        *)
            die "Unsupported build type - $BUILDTYPE"
            ;;
    esac

    local OLDPATH
    OLDPATH="$PATH"

    local SRCLIBDIR
    SRCLIBDIR="libelf-0.8.13"

    # Make a copy of the source directory
    runcmd rm -rf "$BUILDOUTPUT/$SRCLIBDIR"
    runcmd cp -a "$TOOLS/$SRCLIBDIR" "$BUILDOUTPUT/$SRCLIBDIR"

    # Build the library
    runcmd cd "$BUILDOUTPUT/$SRCLIBDIR"
    runcmd export PATH="$COMPILER:$PATH"
    runcmd export LIBRARYPATH="$COMPILER:$LIBRARYPATH"
    runcmd export CHOST="$TARGET"
    runcmd export CC="$MYCC"
    runcmd export CFLAGS="$MYCFLAGS"
    runcmd export CPPFLAGS="$MYCFLAGS"
    runcmd export LDFLAGS="$MYLDFLAGS"
    runcmd export AR="$MYAR"
    runcmd export RANLIB="$MYRANLIB"
    runcmd export mr_cv_target_elf="yes"
    runcmd sh ./configure
    if [[ -n $MYLDFLAGS ]]; then
        runcmd sed -i -e "/^LINK_SHLIB/s/\$/ \$(LDFLAGS)/" lib/Makefile
    fi
    runcmd make -j "$JOBS" -f Makefile
    runcmd export PATH="$OLDPATH"

    # Copy built library to destination
    runcmd cd "$MODSDIR"
    runcmd cp "$BUILDOUTPUT/$SRCLIBDIR/lib/lib${LIBNAME}.so.0" "$DESTDIR"
    runcmd rm -f "$DESTDIR/lib${LIBNAME}.so"
    runcmd ln -s "lib${LIBNAME}.so.0" "$DESTDIR/lib${LIBNAME}.so"
}

buildpython()
{
    local LIBNAME
    LIBNAME="python"

    if echo "$SKIPLIST" | grep -q "\<${LIBNAME}\>"; then
        echo "Skipping ${LIBNAME}"
        return
    fi

    local SRCLIBDIR
    SRCLIBDIR="$TOOLS/libpython-3.6.3"

    local DESTLIBDIR
    DESTLIBDIR="$BUILDOUTPUT/libpython-3.6.3"

    # Make a copy of the source directory
    runcmd rm -rf "$DESTLIBDIR"
    runcmd cp -a "$SRCLIBDIR" "$DESTLIBDIR"

    buildlib "$DESTLIBDIR" "$LIBNAME"
}

buildrm()
{
    local SAVED_BUILDARGS
    SAVED_BUILDARGS="$BUILDARGS"
    BUILDARGS="$(echo $BUILDARGS | sed 's/win32/winsim/')"

    local SAVED_ODIR
    SAVED_ODIR="$ODIR"
    ODIR="$(echo $ODIR | sed 's/win32/winsim/')"

    local SAVED_DESTDIR
    SAVED_DESTDIR="$DESTDIR"
    DESTDIR="$(echo $DESTDIR | sed 's/win32/winsim/')"

    buildlib ../../drivers/resman/arch/lwalloc/mods/winsim/librm rm

    BUILDARGS="$SAVED_BUILDARGS"
    ODIR="$SAVED_ODIR"
    DESTDIR="$SAVED_DESTDIR"
}

buildgtest()
{
    local LIBNAME
    LIBNAME="gtest"

    if echo "$SKIPLIST" | grep -q "\<${LIBNAME}\>"; then
        echo "Skipping ${LIBNAME}"
        return
    fi

    banner "Building ${LIBNAME}..."

    if ! type -t cmake >/dev/null 2>&1; then
        echo "Error: Missing cmake utility, install cmake package on your system!"
        exit 1
    fi

    local COMPILER
    local TARGET
    local MYCC
    local MYAR
    local MYCFLAGS
    local MYLDFLAGS
    case "$BUILDTYPE" in
        linux_amd64_gcc73)
            COMPILER="$TOOLS/linux/mods/gcc-7.3.0-x86_64/bin"
            TARGET="x86_64-pc-linux-gnu"
            MYCC="$COMPILER/$TARGET-gcc"
            MYAR="$COMPILER/$TARGET-gcc-ar"
            MYCFLAGS="-ffunction-sections -fdata-sections --sysroot $COMPILER/.. -std=c++14"
            [[ $ENABLE_LTO ]] && MYCFLAGS+=" -flto"
            MYLDFLAGS="$MYCFLAGS"
            [[ $ENABLE_LTO ]] && MYLDFLAGS+=" -Wl,--gc-sections -Wl,--as-needed"
            ;;
        *)
            die "Unsupported build type - $BUILDTYPE"
            ;;
    esac

    local OLDPATH
    OLDPATH="$PATH"

    # Temporary artifacts folder
    local LIBBUILDDIR
    LIBBUILDDIR="$BUILDOUTPUT/googletest/$BUILDTYPE"

    # Build the library
    runcmd mkdir -p "$LIBBUILDDIR"
    runcmd cd "$LIBBUILDDIR"
    runcmd export PATH="$COMPILER:$PATH"
    runcmd export LIBRARYPATH="$COMPILER:$LIBRARYPATH"
    runcmd export CHOST="$TARGET"
    runcmd export CC="$MYCC"
    runcmd export CFLAGS="$MYCFLAGS"
    runcmd export CPPFLAGS="$MYCFLAGS"
    runcmd export CXXFLAGS="$MYCFLAGS"
    runcmd export LDFLAGS="$MYLDFLAGS"
    runcmd export AR="$MYAR"
    runcmd cmake "$TOOLS/mods/googletest"
    runcmd make -j "$JOBS" -f Makefile

    runcmd export PATH="$OLDPATH"

    # Copy built library to destination
    runcmd cd "$MODSDIR"
    runcmd cp "$LIBBUILDDIR/googlemock/libgmock.a" "$DESTDIR"
    runcmd cp "$LIBBUILDDIR/googlemock/libgmock_main.a" "$DESTDIR"
    runcmd cp "$LIBBUILDDIR/googlemock/gtest/libgtest.a" "$DESTDIR"
    runcmd cp "$LIBBUILDDIR/googlemock/gtest/libgtest_main.a" "$DESTDIR"
}

echohelp()
{
    echo "Usage:"
    echo "$0 [OPTIONS] BUILD"
    echo
    echo "BUILD types:"
    echo "  linux_aarch64       BUILD_OS=linuxmfg BUILD_ARCH=aarch64"
    echo "  linux_amd64_gcc73   BUILD_OS=linuxmfg BUILD_ARCH=amd64 BUILD_OS_SUBTYPE=gcc73"
    echo "  linux_ppc64le       BUILD_OS=linuxmfg BUILD_ARCH=ppc64le"
    echo "  macosx_x86          BUILD_OS=macosx   BUILD_ARCH=x86"
    echo "  macosx_amd64        BUILD_OS=macosx   BUILD_ARCH=amd64"
    echo "  qnx_aarch64         BUILD_OS=cheetah    BUILD_ARCH=aarch64 BUILD_OS_SUBTYPE=qnx"
    echo "  win32_amd64         BUILD_OS=win32    BUILD_ARCH=amd64"
    echo "  win32_x86           BUILD_OS=win32    BUILD_ARCH=x86"
    echo
    echo "OPTIONS:"
    echo "  -pretend    Print commands but don't run them"
    echo "  -j N        Specifies number of make jobs (default is 1)"
    echo "  -s S        Build debug with sanitizer S (disabled by default)"
    echo "  -s none     Build debug without sanitizers (disabled by default)"
    echo "  -dvs        For DVS: print to stdout instead of logfile"
    echo "  -skip       Skip building a specific library."
    echo "  -verbose    Print to stdout with SHOW_OUTPUT=true instead of logfile"
    echo "  -lto        Build with LTO"
    echo "  -h/--help   Print help message"
    echo
}

# Parse command-line flags
JOBS=1
SANITIZER=""
SKIPLIST=""
DVS=""
USE_STDOUT=""
ENABLE_LTO=""
while [ $# -gt 0 ]; do
    case "$1" in
        -skip)
            SKIPLIST+="$2 "
            shift
            ;;
        -pretend)
            PRETEND=1
            ;;
        -j)
            JOBS="$2"
            shift
            ;;
        -s)
            SANITIZER="$2"
            shift
            ;;
        -dvs)
            DVS=1
            USE_STDOUT=1
            ;;
        -lto)
            export ENABLE_LTO=true
            ;;
        -h|--help|help)
            echohelp
            exit
            ;;
        -verbose)
            USE_STDOUT=1
            export SHOW_OUTPUT="true"
            ;;
        *)
            break
            ;;
    esac
    shift
done

UNAME=$(uname -s)
case "$UNAME" in
    MSYS*) UNAME="MSYS" ;;
esac

# Find //sw/tools directory
if [ "$BUILD_TOOLS_DIR" ]; then
    TOOLS="$BUILD_TOOLS_DIR"
elif [ "$COMMON_WS_DIR" -a -d "${COMMON_WS_DIR}/sw/tools" ]; then
    TOOLS="${COMMON_WS_DIR}/sw/tools"
elif [ -d "${P4ROOT}/sw/tools" ]; then
    TOOLS="${P4ROOT}/sw/tools"
elif [ -d "../../../../../tools" ]; then
    TOOLS="../../../../../tools"
else
    echo "Unable to find //sw/tools!" >&2
    exit 1
fi
TOOLS="$(cd -P "$TOOLS" && pwd)"
WIN_TOOLS="$TOOLS"
if type -t cygpath >/dev/null 2>&1; then
    TOOLS="$(cygpath -m "$TOOLS")"
    WIN_TOOLS="$(cygpath "$TOOLS")"
fi

# Remove old build log
[ -f build.log ] && rm build.log

MODSDIR=$PWD

# Check whether we're in the right directory
[ -d ../mods/tools -a -f tools/buildlibs.sh ] || die "Please run this script in <BRANCH>/diag/mods"

# Display help if there were the wrong number of arguments
if [ $# -ne 1 ]; then
    echohelp
    exit
fi

# Set up variables
BUILDTYPE="$1"
shift
case "$BUILDTYPE" in
    linux_aarch64)
        BUILDARGS="BUILD_OS=linuxmfg BUILD_ARCH=aarch64"
        ODIR="linuxmfg/aarch64"
        DESTDIR="linux/aarch64"
        LIBEXT="a"
        SKIPLIST+="elf gtest mods.lib rm"
        ;;
    linux_amd64_gcc73)
        BUILDARGS="BUILD_OS=linuxmfg BUILD_ARCH=amd64 BUILD_OS_SUBTYPE=gcc73"
        ODIR="linuxmfg/amd64"
        DESTDIR="linux/amd64_gcc73"
        LIBEXT="a"
        SKIPLIST+="mods.lib rm"
        ;;
    linux_ppc64le)
        BUILDARGS="BUILD_OS=linuxmfg BUILD_ARCH=ppc64le"
        ODIR="linuxmfg/ppc64le"
        DESTDIR="linux/ppc64le"
        LIBEXT="a"
        SKIPLIST+="gtest mods.lib rm"
        ;;
    macosx_x86)
        BUILDARGS="BUILD_OS=macosx BUILD_ARCH=x86"
        ODIR="macosx/x86"
        DESTDIR="macosx/x86"
        LIBEXT="a"
        SKIPLIST+="elf gtest maths-access mods.lib rm usb-1.0 python"
        ;;
    macosx_amd64)
        BUILDARGS="BUILD_OS=macosx BUILD_ARCH=amd64"
        ODIR="macosx/amd64"
        DESTDIR="macosx/amd64"
        LIBEXT="a"
        SKIPLIST+="elf gtest maths-access mods.lib rm usb-1.0 python"
        ;;
    qnx_aarch64)
        BUILDARGS="BUILD_OS=cheetah BUILD_ARCH=aarch64 BUILD_OS_SUBTYPE=qnx"
        ODIR="cheetah/aarch64"
        DESTDIR="qnx/aarch64"
        LIBEXT="a"
        SKIPLIST+="elf gtest maths-access mods.lib rm usb-1.0 python glslang yaml"
        ;;
    win32_amd64)
        BUILDARGS="BUILD_OS=win32 BUILD_ARCH=amd64"
        ODIR="win32/amd64"
        DESTDIR="win32/amd64_msvc141u8"
        LIBEXT="lib"
        DEBUGLIST="boost cryptopp encryption glslang png spirv-tools xml z"
        SKIPLIST+="elf gtest maths-access usb-1.0"
        ;;
    win32_x86)
        BUILDARGS="BUILD_OS=win32 BUILD_ARCH=x86"
        ODIR="win32/x86"
        DESTDIR="win32/x86_msvc141u8"
        LIBEXT="lib"
        DEBUGLIST="boost cryptopp encryption glslang png spirv-tools xml z"
        SKIPLIST+="elf gtest maths-access usb-1.0"
        ;;
    *)
        die "Unrecognized build type - $BUILDTYPE"
        ;;
esac

BUILDOUTPUT="${MODSDIR}/buildlibs"
BUILDARGS="$BUILDARGS MODS_OUTPUT_DIR=$BUILDOUTPUT"
unset MODS_OUTPUT_DIR

[[ $SANITIZER && $SANITIZER != none ]] && BUILDARGS="$BUILDARGS SANITIZER=$SANITIZER"
[[ $SANITIZER = thread ]] && DESTDIR="${DESTDIR}_tsan" ODIR="${ODIR}_tsan"

# Handle the -j flag
[[ "$JOBS" -ne 1 ]] && BUILDARGS="$BUILDARGS -j $JOBS"
echo "*** Using $JOBS jobs ***"

# Use a different output directory in DVS, don't overwrite Perforce files
[[ $DVS ]] && DESTDIR="$BUILDOUTPUT/output/$DESTDIR"
runcmd rm -rf "$BUILDOUTPUT"/{release,debug}

# Create dummy "MODS runspace" needed by makedefs.inc
runcmd mkdir -p "$BUILDOUTPUT/runspace"
runcmd export MODS_RUNSPACE="$BUILDOUTPUT/runspace"

# Build libraries
runcmd mkdir -p "$DESTDIR"
buildlib_lwmake ../boost boost
buildlib ../cryptopp cryptopp
buildlib_lwmake ../encryption encryption
buildlib ../libusb usb-1.0
buildlib ../lz4 lz4
buildlib ../maths-access maths-access
buildlib_lwmake ../spirv-tools spirv-tools
buildlib ../xml xml
buildlib ../yaml yaml
buildlib win32/modslib mods.lib
buildelf
buildgtest
buildmodsdlllib
buildpng
buildpython
buildrm
buildzlib

echo "Success!"
