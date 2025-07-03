#!/bin/bash
#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2007 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

# NOTE : In this file if a variable name is capitalized, it is a 
#        global variable to the file, lowercase variables are
#        local to their scope
MODS_DIR=$P4ROOT/sw/pvt/main_lw5x/diag/mods

# Array of GPUs to process, these directly map to gpu files by either 
# appending 'gpu.cpp' or 'gp.cpp' to the end.  Note that these must be
# in the array based on class heirarchy.  Base classes MUST be listed 
# before their derived classes
GPUS=( lw4x lw44 lw50 g82 g84 g86 g92 g94 g98 gt200 gt206 igt206 igt209 gt212 gt214 gt216 gt215 gt218 gt21c igt21a gf100 ) 
GPU_START_PROP=( 1 0 3 4 0 6 7 0 0 10 12 0 0 14 20 0 16 0 0 0 0 ) 
GPU_END_PROP=( 1 0 20 20 0 20 20 0 0 20 12 0 0 20 20 0 19 0 0 0 0 ) 

# Array / array index for storing the buglist
BUGLIST=( )
BUGIDX=0

# Map of whether the feature/bug is set/clear for the list of GPUs
HASMAP=( )

# Print the usage for the command
# Usage: PrintUsage <FileName>
PrintUsage() {
    local name=`basename $1`
    echo
    echo $name" [-d modsdir] [-b buglist] [-f featlist] [-h]"
    echo "     -d modsdir     : Specify the MODS directory to parse from"
    echo "     -b buglist     : List of bugs to check GPUs on"
    echo "     -f featlist    : List of features to check GPUs on"
    echo "     -h             : Print this message"
    echo
}

# Print the header for the feature table
# Usage: PrintHeader
PrintHeader() {
    local gpu

    echo "List of features and bugs from "$MODS_DIR
    echo

    # Columnize the GPUS, ensure each column is 6 chars wide
    for gpu in ${GPUS[@]}
    do
        case ${#gpu} in
            3) echo -n "   ";;
            4) echo -n "  ";;
            5) echo -n " ";;
            *) ;;
        esac
        echo -n $gpu"  "
    done
    echo ""
}

# Print the map of S/C for the feature or bug
# Usage: PrintMap <feature name/bug number>
PrintMap () {
    for idx in $(seq 0 $((${#GPUS[@]} - 1)))
    do
        echo -n "     "${HASMAP[$idx]}"  "
    done
    echo "    "$1
}


# Print list of bugs and features that are assigned at runtime
# Usage: PrintRuntimeItems
PrintRuntimeItems () {
    local gpufile
    local bug
    local feature

    gpufile=$MODS_DIR/gpu/gpusbdev.cpp

    # Search gpusbdev.cpp for SetHasBug and print the bug numbers
    echo "Bugs determined at runtime in gpusbdev.cpp SetupRmBugs() : "
    for bug in `grep " SetHasBug(.*);" $gpufile | cut -d '(' -f 2 | cut -d ')' -f 1`
    do
        echo "      Bug "$bug
    done
    echo

    # Search gpusbdev.cpp for SetHasFeature and print the bug numbers
    echo "Features determined at runtime in gpusbdev.cpp SetupFeatures() : "
    for feature in `grep " SetHasFeature(.*);" $gpufile | cut -d '(' -f 2 | cut -d ')' -f 1`
    do
        echo "      "$feature
    done
}

# Print the footer for the feature/bug table
# Usage: PrintFooter
PrintFooter () {
    echo
    echo "NOTE 1: lw4x applies to LW41, LW42, and LW43.  There may be DeviceId"
    echo "        qualifications to bug and feature setup which this script does not"
    echo "        indicate, to find specific GPU versions where a bug or feature is"
    echo "        present look in lwriegpu.cpp SetupBugs() and SetupFeatures()"
    echo
    echo "NOTE 2: lw44 applies to LW44 through G78.  There may be DeviceId qualifications"
    echo "        to bug and feature setup which this script does not check, to find"
    echo "        specific GPU versions where a bug or feature is present look in"
    echo "        lw44gpu.cpp SetupBugs() and SetupFeatures()"
}

# Get the gpu filename for the specified GPU
# Usage: GetGPUFile <GPU>
GetGPUFile () {
    local filename
    local gpu=$1
    if [ "$gpu" == "igt206" ]
    then
        filename=$MODS_DIR/gpu/${gpu}gp.cpp
    elif [ "$gpu" == "igt209" ]
    then
        filename=$MODS_DIR/gpu/${gpu}gp.cpp
    elif [ "$gpu" == "igt21a" ]
    then
        filename=$MODS_DIR/gpu/${gpu}gp.cpp
    else
        filename=$MODS_DIR/gpu/${gpu}gpu.cpp
    fi

    retval=$filename
}

# Since features are inherited, this function propogates a feature set
# or clear to all its subclasses
# Usage: PropogateFeature <GPU> <S or C>
PropogateFeature () {
    local start_copy
    local end_copy

    start_copy=${GPU_START_PROP[$1]}
    end_copy=${GPU_END_PROP[$1]}

    # If progagating using a loop, propogate the S/C flag
    if [ "$start_copy" != "0" ]
    then
        for ((idx=start_copy; idx <= end_copy; idx++))
        do
            HASMAP[$idx]=$2
        done
    fi
}

# Set or clear whether a GPU has the specified feature/bug
# Usage SetClearHas <Bug or Feature> <Bug # or Feature Name> <GPU filename> <GPU>
SetClearHas ()
{
    local sethas
    local clearhas
    local gpufile

    # Get the GPU filename
    GetGPUFile $3
    gpufile=$retval

    # Get set or clear flags. sethas be 1 if bug/feature is
    # set or 0 otherwise, clearhhas be 1 if bug/feature is
    # cleared or 0 otherwise
    sethas=`grep -m 1 -c "SetHas$1($2" $gpufile`
    clearhas=`grep -m 1 -c "ClearHas$1($2" $gpufile`

    # If set/clear in the specified GPU file, set the appropriate flag 
    # for the current GPU and if not processing bugs, propogate the  
    # feature to all derived GPUs
    if [ "$sethas" == "1" ]
    then
        HASMAP[$idx]=S
        if [ "$1" != "Bug" ]
        then
            PropogateFeature $idx S
        fi
    else
        if [ "$clearhas" == "1" ]
        then
            HASMAP[$idx]=C
            if [ "$1" != "Bug" ]
            then
                PropogateFeature $idx C
            fi
        fi
    fi
}

# Setup the map of which GPUS have the current bug/feature
# Usage SetupMap <Bug or Feature> <Bug # or Feature Name>
SetupMap () {
    local gpu
    local gpufile

    # Clear the map to start
    HASMAP=( C C C C C C C C C C C C C C C C C C C C C )

    # For every GPU in the list process the bug/feature
    for idx in $(seq 0 $((${#GPUS[@]} - 1)))
    do
        SetClearHas $1 $2 ${GPUS[$idx]}
    done
}

# Return 1 if the buglist already has the bug #, 0 otherwise
# Usage BugListHas <Bug #>
BugListHas () {
    for lwrbug in ${BUGLIST[@]}
    do
        if [ "$lwrbug" == "$1" ]
        then
            return 1
        fi
    done

    return 0
}

# Setup the list of bugs to search for
# Usage SetupBugList
SetupBugList () {
    local gpu
    local gpufile
    local bug

    # For each GPU
    for idx in $(seq 0 $((${#GPUS[@]} - 1)))
    do
        # Get the filename
        gpu=${GPUS[$idx]}
        GetGPUFile $gpu
        gpufile=$retval

        # For each instance of SetHasBug in the file, parse out the bug number
        # and if the number is not already in the buglist, add it
        for bug in `grep "SetHasBug(.*);" $gpufile | cut -d '(' -f 2 | cut -d ')' -f 1`
        do
            BugListHas $bug
            if [ $? -eq 0 ]
            then
                BUGLIST[$BUGIDX]=$bug
                (( BUGIDX++ ))
            fi
        done
    done
}

# Process all bugs/features
DOALL=true

while getopts f:b:d:h OPTCHAR "$@"; do
    case $OPTCHAR in
        f)  # The feature list has been specified, don't process everything
            FEAT_ARG=$OPTARG
            DOALL=false
            DOFEAT=true
            ;;
        b)  # The bug list has been specified, don't process everything
            BUG_ARG=$OPTARG
            DOALL=false
            DOBUG=true
            ;;
        d)  # The MODS directory has been specified, validate that it is a good one
            MODS_DIR=$OPTARG
            if [ ! -d $MODS_DIR/gpu ]
            then
                echo "Directory "$MODS_DIR"/gpu does not exist"
                PrintUsage $0
                exit -1
            fi
            ;;
        h)  PrintUsage $0
            exit 0
            ;;
        ?)  break;;
    esac
done
[[ $OPTIND > 1 ]] && shift $((OPTIND - 1))

# Print the header for the bug/feature list
PrintHeader

# If doing everything
if $DOALL
then
    # Setup the list of bugs to process
    SetupBugList

    # For every feature in the feature list file, Create and print the
    # map of where that feature is set/cleared
    for FEATURE in `grep -E 'FEATURE\(.*,' $MODS_DIR/core/include/featlist.h | cut -d '(' -f 2 | cut -d ',' -f 1`
    do
        SetupMap Feature $FEATURE
        PrintMap $FEATURE
    done
    echo

    # For every bug in the bug list, Create and print the
    # map of where that bug is set/cleared
    for BUG in ${BUGLIST[@]}
    do
        SetupMap Bug $BUG
        PrintMap "Bug "$BUG
    done

    # Print those items determined at runtime
    PrintRuntimeItems
else
    # If the feature argument was given, process the provided features
    if $DOFEAT
    then
        for FEATURE in $FEAT_ARG
        do
            SetupMap Feature $FEATURE
            PrintMap $FEATURE
        done
    fi

    # If the bug argument was given, process the provided bugs
    if $DOBUG
    then
        for BUG in $BUG_ARG
        do
            SetupMap Bug $BUG
            PrintMap "Bug "$BUG
        done
    fi
fi
echo

# Print the footer
PrintFooter
