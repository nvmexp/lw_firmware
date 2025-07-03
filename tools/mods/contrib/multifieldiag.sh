#!/bin/bash
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2016-2018 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

###############################################################################
# User-Configured Test Variables
###############################################################################

# Time in seconds between launching the conlwrrent tests
# This delay reduces the resource contention between the instances of mods
PAUSE_SEC=4

# Per-GPU timeout for each stage of testing
TIMEOUT_COMMON_SEC=18000
TIMEOUT_PARALLEL_SEC=27000

# Name for temporary folders from which to run the fieldiag, store logs, etc.
RUNSPACE_NAME="__fieldiag"

###############################################################################
# User-Configured Tests
###############################################################################

# Testing Order:
# ==============
# LwLink Test -> Parallel Tests


function acquire_access_token()
{
    # Exit with failure if any command fails
    local access_result=`./fieldiag acquire_access_token`
    local token_regex="Access Token : ([0-9]+)"

    if [[ $access_result =~ ${token_regex} ]]; then
        ACCESS_TOKEN=${BASH_REMATCH[1]}
        echo "./fieldiag release_access_token access_token=$ACCESS_TOKEN > /dev/null" > release_access.sh
        chmod +x release_access.sh
    else
        echo "Failed to acquire MODS access token"
        exit 1
    fi
}

function release_access_token() {
    if [[ -n "$ACCESS_TOKEN" ]]; then
        ./fieldiag release_access_token access_token=$ACCESS_TOKEN >> ps.txt
        if [[ $? && -f release_access.sh ]]; then
            rm -f release_access.sh
        fi
    fi
}

# 'LwLink Test' is run once with all GPUs being active on a single instance 
# Exiting with a failure will cause all GPUs to fail the script.
function common_test()
{
    # Exit with failure if any command fails
    set -eu -o pipefail
    local ngpus=$1
    shift

    ./fieldiag access_token=$ACCESS_TOKEN only_lwlink $@
}

# 'Parallel Test' is run on all GPUs at the same time, one instance for each gpu
function parallel_test()
{
    # Exit with failure if any command fails
    set -eu -o pipefail
    local i=$1
    local pciid=$2
    local visible=$3
    shift; shift; shift
    if [ ! -e "${RUNSPACE_NAME}$i" ]
    then
        mkdir ${RUNSPACE_NAME}$i
        for file in `ls | egrep -v '[.](sh|pdf|log|tgz.*)$'`
        do
            if [ ! -d $file ] # don't copy directories
            then
                cp $file ${RUNSPACE_NAME}$i
            fi
        done
    fi
    cd ${RUNSPACE_NAME}$i

    # show one run's output so the user has some idea of what's happening
    # might backfire if the visible GPU fails...
    if [ "$visible" -eq "1" ]
    then
        ./fieldiag access_token=$ACCESS_TOKEN pciid=$pciid skip_lwlink $@
    else
        ./fieldiag access_token=$ACCESS_TOKEN pciid=$pciid skip_lwlink $@ > /dev/null
    fi
}

###############################################################################
# Helper functions (do not modify)
###############################################################################

GPULIST=()
PIDLIST=()
NAMELIST=()
STATUSLIST=()
CHILDPIDS=()
DYNAMICPRINT=0

function save_child_pids()
{
    # Always insert new PIDS at the start of the array so that decendents get
    # cleaned up before antecedents
    CHILDPIDS=("${1}-${2}" ${CHILDPIDS[@]})
    local childpids=$(pgrep -P $2)
    for pid in $childpids; do        
        save_child_pids $1 $pid
    done
}

function reset_lists()
{
    for i in ${!GPULIST[@]}
    do
        NAMELIST[i]="GPU $i"
        PIDLIST[i]="-1"
        STATUSLIST[i]="PENDING"
    done
    CHILDPIDS=()
}

#
# Usage: cleanup <PID>
# Terminates the specified PID and all its child processes
#
function cleanup()
{
    if ps -p $1 > /dev/null
    then
        kill -SIGTERM $1     2> /dev/null
        sleep 5
        kill -SIGKILL $1     2> /dev/null
    fi
}

#
# Usage: cleanup_pid_tree <PID>
# Terminates the specified PID and all its child processes starting with the
# farthest descendant.  This operates on a captured list of PIDs so will work
# even if the children were detached from the original parent
#
function cleanup_pid_tree()
{
    for i in ${!CHILDPIDS[@]}
    do
        local childpid=${CHILDPIDS[$i]}
        if [[ ${childpid} =~ ^${1}- ]]
        then
            local chpid=$(echo ${childpid} | cut -f 2 -d '-')
            cleanup $chpid
            eval find /dev/mqueue -type f -name "\"*ppid = ${chpid}*\"" -print0 | xargs -0 -n1 rm -f
        fi
    done
}

#
# Terminates all processes in the same process group
# Used to kill all background processes spawned by the script
#
function terminate_all()
{
    trap '' SIGHUP SIGINT SIGTERM
    echo -e "\nExiting script..."

    for i in ${!PIDLIST[@]}
    do
        local pid=${PIDLIST[$i]}
        [ $pid -eq -1 ] && continue
        cleanup_pid_tree $pid
    done

    release_access_token

    kill -SIGTERM 0
    sleep 5
    kill -SIGKILL 0
    exit 1
}

#
# Usage: monitor <TIMEOUT_SEC>
#
# Checks all running subprocesses for completion. The processes are specified
# in the global variable PIDLIST. If the process has completed or timed out,
# update global variable STATUSLIST with the result.
#
# The contents of STATUSLIST are printed via pstatus at the beginning and
# whenever its contents are updated.
#
function monitor()
{
    local timeout=$1
    local starttime=`date +%s`

    pstatus
    while true
    do
        local running=0
        local update=0
        for i in ${!PIDLIST[@]}
        do
            # Skip over inactive GPUs
            local pid=${PIDLIST[$i]}
            [ $pid -eq -1 ] && continue

            # Check Timeout
            local waittime=`date +%s`
            if (($waittime - $starttime > $timeout))
            then
                PIDLIST[$i]=-1
                STATUSLIST[$i]="TIMEOUT"
                cleanup_pid_tree $pid
                update=1
                continue
            fi

            # Check Done
            if ! kill -0 $pid 2> /dev/null
            then
                wait $pid
                local rc=$?

                PIDLIST[$i]=-1
                if [ $rc -eq 0 ]
                then
                    STATUSLIST[$i]="PENDING"
                else
                    STATUSLIST[$i]="FAIL"
                fi
                update=1
                continue
            fi

            ((running++))

        done

        [ $update  -eq 1 ] && pstatus
        [ $running -eq 0 ] && break

        sleep 0.25
    done
}

# This function is called whenever the test status of a GPU changes
function pstatus()
{
    for i in ${!GPULIST[@]}
    do
        echo -e -n "${NAMELIST[$i]}: ${STATUSLIST[$i]}\t"
    done
    echo
}

die()
{
    echo "$@" >&2
    exit 1
}

###############################################################################
# START OF SCRIPT
###############################################################################

# Setup CTRL-C override for killing background test processes
trap terminate_all SIGHUP SIGINT SIGTERM

# release the access token on exit
trap release_access_token EXIT

if [ $(id -u) -ne 0 ]; then
    echo "Script must be run as root user! Exiting..."
    exit 1
fi

# Parse Arguments
SCRIPTNAME="$0"
FIELDIAG_ARGS=""
PCIDEV_ARG=""
RUN_PARALLEL=1
RUN_LWLINK=1
LOG_FILENAME="../fieldiag-logs.tgz"
while [[ $# -gt 0 ]]
do
    arg="$1"
    case $arg in
    # No reason to use pciid or device with a script meant to run multiple
    # fieldiag instances. Make them use the exelwtable directly.
    pciid=*|device=*)
        baseArg=`echo $arg | sed "s/=.*//"`
        echo \"$baseArg\" cannot be used with $SCRIPTNAME
        echo To use \"$baseArg\", please call fieldiag directly
        echo Otherwise, use \"pci_devices\" to select a subset of GPUs to test
        exit 1
    ;;
    # LwLink tests get all pci devices, parallel tests get them individually
    # so parse this argument here to handle that correctly
    pci_devices=*)
        PCIDEV_ARG=$arg
        for pciid in `echo $arg | sed "s/pci_devices=//;s/,/ /g"`
        do
            GPULIST+=($pciid)
        done
    ;;
    BS_Test) # With BS_Test, LwLink tests shouldn't be run
        if [ $RUN_PARALLEL -eq 0 ]
        then
            echo "Cannot use BS_Test with only_lwlink"
            exit 1
        fi
        RUN_LWLINK=0
        FIELDIAG_ARGS=$FIELDIAG_ARGS" "$arg
    ;;
    skip_lwlink)
        if [ $RUN_PARALLEL -eq 0 ]
        then
            echo "Cannot use skip_lwlink with only_lwlink"
            exit 1
        fi
        RUN_LWLINK=0
    ;;
    only_lwlink)
        if [ $RUN_LWLINK -eq 0 ]
        then
            echo "Cannot use only_lwlink with skip_lwlink or BS_Test"
            exit 1
        fi
        RUN_PARALLEL=0
    ;;
    # log files need to be named differently, so honor
    # this argument by renaming the final package name
    logfilename=*)
        LOG_FILENAME=`echo $arg | sed "s/logfilename=//"`
        # Relative paths need to jump up a directory since the
        # tgz is made in a subdirectory of the working directory
        if [[ "$LOG_FILENAME" != /* ]]
        then
            LOG_FILENAME="../"$LOG_FILENAME
        fi
        # Force extension to .tgz
        if [ "${LOG_FILENAME##.*}" != ".tgz" ]
        then
            LOG_FILENAME=$LOG_FILENAME".tgz"
        fi
    ;;
    # Other arguments are just passed directly to fieldiag
    *)
        FIELDIAG_ARGS=$FIELDIAG_ARGS" "$arg
    ;;
    esac
    shift
done

# Check if the MODS kernel driver installed. If not, or if the version
# is not supported install it
RC=0
MODS_INSERTED="1"
MIN_SUPP_VER_MAJOR="3"
MIN_SUPP_VER_MINOR="82"
[[ -f install_module.sh ]] || die "install_module.sh not found"

# Check if MODS is inserted
`lsmod | awk '{print $1}' | grep ^mods$ > /dev/null` || MODS_INSERTED="0"

if [[ "$MODS_INSERTED" = "0" ]];then
    # Check if MODS is installed
    modprobe mods 2> /dev/null || ./install_module.sh -i || die "Error installing MODS kernel module"
fi

[[ -f "version_checker" ]] || die "version_checker utility not found"

./version_checker "$MIN_SUPP_VER_MAJOR" "$MIN_SUPP_VER_MINOR" || RC=$?

if [[ $RC -eq 1 ]];then
    ./install_module.sh -u || die "Error uninstalling MODS kernel module"
    ./install_module.sh -i || die "Error installing MODS kernel module"
elif [[ $RC -gt 1 ]];then
    die "Error checking MODS kernel version"
fi

if [[ "$LOAD_MULTI_INST_DRIVER" = "1" ]]; then
    # Reload MODS kernel driver in multiple-instance mode
    rmmod mods
    modprobe mods multi_instance=1 || die "Failed to load MODS driver. Exiting..."
fi

# If -s argument is not in use, detect all Lwpu GPUs by PCIe vendorid
[ ${#GPULIST[@]} -eq 0 ] && GPULIST+=(`lspci -mm -s .0 -D -d 10DE: | awk '{ print $1 }'`)

if [ ${#GPULIST[@]} -eq 0 ]
then
    echo "No Lwpu GPUs found"
    exit 1
else
    echo "GPU Devices Under Test: ${GPULIST[@]}"
fi

# Init Lists
reset_lists

# Clear log directory
if [ -e ${RUNSPACE_NAME}logs ]
then
    rm -f ${RUNSPACE_NAME}logs/*
else
    mkdir ${RUNSPACE_NAME}logs
fi

#
# Shared Tests (LwLink)
#

acquire_access_token

if [ $RUN_LWLINK -eq 1 ]
then
    common_test ${#GPULIST[@]} $PCIDEV_ARG $FIELDIAG_ARGS &
    for i in ${!GPULIST[@]}
    do
        if [ ${STATUSLIST[i]} != "PENDING" ]
        then
            continue
        fi
        STATUSLIST[i]="RUNNING"
        PIDLIST[i]=$!
    done
    sleep 2
    save_child_pids ${PIDLIST[0]} ${PIDLIST[0]}
    echo "Running LwLink Tests"
    monitor TIMEOUT_COMMON_SEC

    # Grab the log from the LwLink run
    for log in `ls *.log`
    do
        mv $log ${RUNSPACE_NAME}logs/lwlink_$(basename $log)
    done
fi

#
# Parallel Tests
#

# Make sure everything passed before trying to run parallel tests
for i in ${!GPULIST[@]}
do
    if [ ${STATUSLIST[i]} != "PENDING" ]
    then
        RUN_PARALLEL=0
    fi
done

if [ $RUN_PARALLEL -eq 1 ]
then
    echo "Running Parallel Tests"
    
    # Make the output from the first GPU visible
    # so the user has some idea of what's going on
    VISIBLE=1
    CHILDPIDS=()

    # Parallel Tests
    for i in ${!GPULIST[@]}
    do
        STATUSLIST[i]="RUNNING"
        pciid=${GPULIST[i]}
        parallel_test $i $pciid $VISIBLE $FIELDIAG_ARGS &
        VISIBLE=0

        PIDLIST[i]=$!
        sleep $PAUSE_SEC
        save_child_pids ${PIDLIST[i]} ${PIDLIST[i]}
    done
    monitor TIMEOUT_PARALLEL_SEC

    # Grab the logs from each gpu's run
    for i in ${!GPULIST[@]}
    do
        for log in `ls ${RUNSPACE_NAME}$i/*.log`
        do
            mv $log ${RUNSPACE_NAME}logs/gpu${i}_$(basename $log)
        done
    done
fi

# Zip up the log files
cd ${RUNSPACE_NAME}logs
if [ "$(ls)" ]
then
    tar -czf $LOG_FILENAME *
else
    echo "No log files found"
fi
cd ..

# If all GPUs passed all tests, exit with success
allpassing=0
for i in ${!GPULIST[@]}
do
    if [ ${STATUSLIST[i]} == "PENDING" ]
    then
        STATUSLIST[i]="PASS"
    else
        allpassing=1
    fi
done

echo "----------------------"
echo "Fieldiag Testing Completed"
pstatus


exit $allpassing
