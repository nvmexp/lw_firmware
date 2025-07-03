#!/bin/sh
#
# Copyright 2020 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# Working Directory Assumption: //sw/dev/gpu_drv/chips_hw/uproc/
#

echo "===== Update uCode BINRES for AS2 Promotion =====";

UCODE_IDX=0
UCODES=(sec2 disp/dpu)
UCODE_BINRES_SRC=(
	sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/sec2/bin
    sw/dev/gpu_drv/chips_a/drivers/resman/kernel/inc/dpu/bin
)
UCODE_BINRES_DST=(
	sw/dev/gpu_drv/chips_hw/drivers/resman/kernel/inc/sec2/bin
    sw/dev/gpu_drv/chips_hw/drivers/resman/kernel/inc/dpu/bin
)

PROMOTION_FILE=ucode_promotion_flag
UCODE_RELEASE=ucode_auto_promotion_files # profiles set to be released
PERL=/usr/bin/perl

RELEASE_SCRIPT=$AS2_TREEROOT/sw/dev/gpu_drv/chips_hw/uproc/build/scripts/release-ucode-profiles.pl

CL_VALUE=""
SYNC_RESULT=""
FIND_RESULT=""
UCODE_PATH=""

## Check to see if running on AS2 or not
if [ -z "$AS2_TREEROOT" ];
then
    echo "Unable to determine if running on AS2, exiting...";
    exit;
fi

#
# Checks if this build is oclwring during a promotion
# if it is, pull the changelist number and add it to the
# promotion flag file.
#
CL_VALUE=$(/home/utils/perl5/perlbrew/perls/5.24.2-009/bin/perl \
               build/scripts/get-cl.pl                          \
               --tag sw_to_hw_mods_promotion_try);

# If this for any reason pulling the CL fails, do not continue
if [ "$CL_VALUE" = "-1" ];
then
    echo "Failed to retrieve ucode promotion changelist";
    exit;
fi

# Check if promotion file is in root ucode directory
if [ ! -f $PROMOTION_FILE ];
then
	 echo "[$ucode] No promotion detected";
	 exit;
fi

#
# Iterates through ucodes and promotes files based on
# compilation output.
#
for ucode in ${UCODES[@]}
do
	# Ucode variables
	ucode_binres_src=${UCODE_BINRES_SRC[${UCODE_IDX}]}
	ucode_binres_dst=${UCODE_BINRES_DST[${UCODE_IDX}]}
	UCODE_IDX=$(( ${UCODE_IDX} + 1 ))

	echo "[$ucode] Source binres: $ucode_binres_src"
	echo "[$ucode] Destination binres: $ucode_binres_dst"
    echo "[$ucode] Updating ucode destination binres";

    # Check for the ucode release files
    FIND_RESULT=$(find $ucode/ -type f \( -iname "$UCODE_RELEASE" \))
    UCODE_PATH=$(pwd)/$(dirname "$FIND_RESULT")

    # Check if promotion release script exists
    if [ "$FIND_RESULT" = "" ];
    then
        echo "[$ucode] No ucode update file found";
        continue;
    else
        echo "[$ucode] Ucode update file found";
		echo "[$ucode] Path set to: $UCODE_PATH";
    fi

    # Synchronize the binary files
    echo "[$ucode] Syncing chips_a binres with promotion CL ($CL_VALUE) ...";
    SYNC_RESULT=$(p4 -s sync -f //$ucode_binres_src/...@$CL_VALUE | grep "error");

    # Check if the synchronization was successful
    if [ ! "$SYNC_RESULT" = "" ];
    then
        echo "[$ucode] Failed to sync binaries with promotion CL";
        exit;
    else
        echo "[$ucode] Binaries synchronized successfully";
    fi

    # Run the Release Script
    # Must also enter and exit the directories so references are correct
    echo "[$ucode] Run the release script";

    $PERL $RELEASE_SCRIPT --profiles $UCODE_PATH/$UCODE_RELEASE \
                          --source-path $AS2_TREEROOT/$ucode_binres_src \
                          --dest-path $AS2_TREEROOT/$ucode_binres_dst \
                          --release-others \
                          --verbose;

    echo "[$ucode] Removing temporary promotion files";
    rm $UCODE_PATH/$UCODE_RELEASE;

    # Check for Success
    if [ "$?" -ne "0" ];
    then
        echo "[$ucode] Failed to correctly update ucodes";
        exit;
    fi

    echo "[$ucode] Completed ucode promotion";
done

