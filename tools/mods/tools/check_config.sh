#!/bin/sh

# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2012-2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

QUIT="exit 1"

while [ $# -gt 0 ]; do
    if [ "$1" = "--all" ]; then
        QUIT="true"
    elif [ "$1" = "-a" ]; then
        QUIT="true"
    else
        echo "Unrecognized option - $1"
        exit 1
    fi
    shift
done

echo -n "Checking OS... "
if [ `uname` != Linux ]; then
    echo "Unsupported OS - `uname`. MODS will not work on this system."
    $QUIT
fi
echo "Linux - OK"

echo -n "Checking kernel version... "
MAJOR=`uname -r | cut -f 1 -d '.'`
MINOR=`uname -r | cut -f 2 -d '.'`
REV=`uname -r | cut -f 3 -d '.' | sed "s/[^0-9].*//"`
echo -n "$MAJOR.$MINOR.$REV - "
ERROR=""
if [ $MAJOR -eq 2 ]; then
    if [ "$MINOR" != "6" ]; then
        echo "ERROR - unsupported kernel version, recommended 2.6.29 or newer"
        $QUIT
    fi
    if [ $REV -lt 29 ]; then
        echo "ERROR - unsupported kernel version, recommended 2.6.29 or newer"
        $QUIT
    fi
    echo "OK"
else
    echo "OK"
fi

echo -n "Checking architecture... "
ARCH=`uname -m`
echo -n "$ARCH - "
if [ "$ARCH" != "x86_64" ] && [ "$ARCH" != "aarch64" ]; then
    echo "ERROR - unsupported architecture, should be x86_64 or aarch64"
    $QUIT
else
    echo "OK"
fi

echo -n "Checking libc version... "
LIBC="/lib/libc.so.6"
if [ ! -x "$LIBC" ]; then
    LIBC=`find /lib/ -name libc.so.6 | head -n 1`
fi
if [ ! -x "$LIBC" ] && [ "$ARCH" = "x86_64" ] && [ -d /lib64 ]; then
    LIBC=`find /lib64/ -name libc.so.6 | head -n 1`
fi
if [ -x "$LIBC" ]; then
    LIBCVER=`"$LIBC" | head -n 1 | sed "s/.*release version *// ; s/[^0-9.].*//"`
    MAJOR=`echo "$LIBCVER" | cut -f 1 -d '.'`
    MINOR=`echo "$LIBCVER" | cut -f 2 -d '.'`
    echo -n "$LIBCVER - "
    if [ $MAJOR -gt 2 ]; then
        echo "OK"
    elif [ $MAJOR -eq 2 ] && [ $MINOR -ge 8 ]; then
        echo "OK"
    else
        echo "ERROR - unsupported libc version, should be 2.8 or newer"
        $QUIT
    fi
else
    echo "WARNING - unable to find libc.so.6, skipping check"
fi

echo -n "Checking MODS kernel module... "
VERSION=`dmesg | grep "\<mods:" | grep version | sed "s/.*version *//" | tail -n 1`
LSMOD="/sbin/lsmod"
[ -x "$LSMOD" ] || LSMOD="/usr/sbin/lsmod"
[ -x "$LSMOD" ] || LSMOD="/bin/lsmod"
[ -x "$LSMOD" ] || LSMOD="/usr/bin/lsmod"
if [ "$VERSION" ]; then
    echo -n "$VERSION - "
    MAJOR=`echo "$VERSION" | cut -f 1 -d '.'`
    MINOR=`echo "$VERSION" | cut -f 2 -d '.'`
    if [ $MAJOR -lt 3 ]; then
        echo "ERROR - unsupported MODS kernel module version"
        $QUIT
    fi
    if [ $MAJOR -eq 3 -a $MINOR -lt 87 ]; then
        echo "ERROR - MODS kernel module too old, please run install_module.sh -i to upgrade"
        $QUIT
    else
        echo "OK"
    fi
elif [ ! -x "$LSMOD" ]; then
    echo "WARNING - lsmod tool is missing, unable to determine kernel module's presence"
elif "$LSMOD" | grep -q "\<mods\>"; then
    echo "WARNING - module loaded, but unable to determine its version"
else
    echo "ERROR - module not loaded, please run install_module.sh -i and make sure it's loaded on boot"
    $QUIT
fi

echo -n "Checking conflicting kernel modules... "
if [ "$LSMOD" ]; then
    MODULES=`"$LSMOD" | cut -f 1 -d ' ' | grep "\<lwpu\>\|\<nouveau\>\|\<lwgpu\>"`
    if [ "$MODULES" ]; then
        echo "$MODULES - ERROR - please unload conflicting kernel modules"
        $QUIT
    else
        echo "none - OK"
    fi
else
    echo "WARNING - lsmod tool is missing, unable to determine kernel module's presence"
fi

echo -n "Checking console blank ilwerval... "
CONSOLEBLANK="/sys/module/kernel/parameters/consoleblank"
if [ -f "$CONSOLEBLANK" ]; then
    INTERVAL=`cat "$CONSOLEBLANK"`
    echo -n "$INTERVAL - "
    if [ $INTERVAL -gt 0 ]; then
        echo -e "WARNING - please disable console blanking to avoid stability issues, run as root: # echo -ne '\\\033[9;0]\\\033[14;0]' > /dev/console"
    else
        echo "OK"
    fi
else
    echo "WARNING - $CONSOLEBLANK not found"
fi

echo -n "Looking for LWPU GPUs... "
LSPCI="/sbin/lspci"
[ -x "$LSPCI" ] || LSPCI="/usr/sbin/lspci"
[ -x "$LSPCI" ] || LSPCI="/bin/lspci"
[ -x "$LSPCI" ] || LSPCI="/usr/bin/lspci"
if [ -x "$LSPCI" ]; then
    ADAPTERS=`"$LSPCI" -n | grep -i "\<10de:[0-9a-f][0-9a-f][0-9a-f][0-9a-f]\>" | grep "\<0300:\|\<0302:" | cut -f 1 -d ' ' | tr -s '\n' ' '`
    if [ "$ADAPTERS" ]; then
        echo "$ADAPTERS - OK"
    else
        echo "ERROR - no LWPU GPUs found"
        $QUIT
    fi
else
    echo "WARNING - lspci tool not found"
fi
