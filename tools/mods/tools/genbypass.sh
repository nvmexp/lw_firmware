#!/bin/bash
function usage()
{
    echo "usage: $myName [-aAc] make_args" >&2
    echo "    -a: Same as qi $myName -j8 -k build_all" >&2
    echo "    -A: Same as qi $myName -j8 -k" >&2
    echo "    -c: Clean EXE_DIR before build" >&2
}

while getopts e:t:d: OPTCHAR "$@"; do
    case $OPTCHAR in
        d)  EXE_DIR=$OPTARG;;
        e)  EXE_NAME=$OPTARG;;
        t)  TOOLS_DIR=$OPTARG;;
        ?)  break;;
    esac
done

[[ $OPTIND > 1 ]] && shift $((OPTIND - 1))

EXE_DIR=${EXE_DIR:-${MODS_RUNSPACE:-.}}
EXE_NAME=${EXE_NAME:-mods}
TOOLS_DIR=${TOOLS_DIR:-`dirname $0`}
EXTRA_ARGS=$*

[ -z "$EXTRA_ARGS" ] && [ "$EXE_NAME" == "mods" ] && EXTRA_ARGS="-v"

# In case MODS runspace points to a debug version of MODS, include it in the
# library path so that MODS can find the shared libraries
export LD_LIBRARY_PATH="$EXE_DIR"${LD_LIBRARY_PATH:+:}$LD_LIBRARY_PATH

# Ensure that the necessary files exist
if [ ! -f "$EXE_DIR/$EXE_NAME" ]; then
    echo "$EXE_NAME exelwtable not found in "$EXE_DIR
    exit 1
fi

if [ ! -f "$TOOLS_DIR"/genfiles.py ]; then
    echo "bypass generation file not found in "$TOOLS_DIR
    exit 1
fi

# Ensure that MODS will give the correct information to generate bypass.bin and internal_bypass.bin
# (i.e. is not an official build)
VER_PRESENT=`"$EXE_DIR/$EXE_NAME" $EXTRA_ARGS | grep -c "Version:"`
CL_PRESENT=`"$EXE_DIR/$EXE_NAME" $EXTRA_ARGS | grep -c "Changelist:"`
if [ "$VER_PRESENT" != "1" ]; then
    echo "Version string not present in $EXE_NAME output"
    exit 1
fi
if [ "$CL_PRESENT" != "1" ]; then
    echo "Changelist string not present in $EXE_NAME output"
    exit 1
fi

# Extract the version and CL
EXE_VERSION=`"$EXE_DIR/$EXE_NAME" $EXTRA_ARGS | gawk -F ' ' '{ if ($1 == "Version:") { print $2 } }'`
EXE_CL=`"$EXE_DIR/$EXE_NAME" $EXTRA_ARGS | gawk -F ' ' '{ if ($1 == "Changelist:") { print $2 } }'`
[ "$EXE_CL" != "unspecified" ] || EXE_CL=0

echo "Generating bypass files with :"
echo "    Version     : " $EXE_VERSION
echo "    Changelist  : " $EXE_CL

# Generate bypass.bin and internal_bypass.bin
"$TOOLS_DIR"/genfiles.py -bbypass.bin g_Version,$EXE_VERSION g_Changelist,$EXE_CL


