#!/bin/bash
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2017 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

#
# Remove duplicate encrypted files from the MODS runspace.
# If both a encrypted and unencrypted version of a file are present in the
# MODS runspace this file will remove the duplicate version
#
set -euo pipefail

MODS_RUNSPACE=${1:-.}

remove_dup()
{
    echo "Removing ${1}..."
    rm -f "${1}"
}

echo "Removing duplicate encrypted files in ${MODS_RUNSPACE}..."

for FNAME in "${MODS_RUNSPACE}"/*; do
    BASEFILE=$(basename $FNAME)
    EXT="${BASEFILE##*.}"

    # File could have been removed on a previous iteration, skip it if so
    [[ ! -f "${FNAME}" ]] && continue

    ENCRYTPED_EXT=""
    UNENCRYTPED_EXT=""
    case "${EXT}" in
        js)    ENCRYTPED_EXT="jse"     ;;
        h)     ENCRYTPED_EXT="he"      ;;
        xml)   ENCRYTPED_EXT="xme"     ;;
        json)  ENCRYTPED_EXT="jsone"   ;;
        db)    ENCRYTPED_EXT="dbe"     ;;
        spc)   ENCRYTPED_EXT="spe"     ;;
        jse)   UNENCRYTPED_EXT="js"    ;;
        he)    UNENCRYTPED_EXT="h"     ;;
        xme)   UNENCRYTPED_EXT="xml"   ;;
        jsone) UNENCRYTPED_EXT="json"  ;;
        dbe)   UNENCRYTPED_EXT="db"    ;;
        spe)   UNENCRYTPED_EXT="spc"   ;;
    esac

    # No valid extension found, skip
    [[ -n "${ENCRYTPED_EXT}" || -n "${UNENCRYTPED_EXT}" ]] || continue

    # Set the opposing extension appropriately
    [[ -z "${ENCRYTPED_EXT}" ]] && ENCRYTPED_EXT=${EXT}
    [[ -z "${UNENCRYTPED_EXT}" ]] && UNENCRYTPED_EXT=${EXT}
    ENCRYPTED_FILE="${BASEFILE%.*}"."${ENCRYTPED_EXT}"
    UNENCRYPTED_FILE="${BASEFILE%.*}"."${UNENCRYTPED_EXT}"

    # If both encrypted and unencrypted files are present, remove them
    [[ ! -f "${MODS_RUNSPACE}"/${ENCRYPTED_FILE} || ! -f "${MODS_RUNSPACE}"/${UNENCRYPTED_FILE} ]] || remove_dup "${MODS_RUNSPACE}"/"${ENCRYPTED_FILE}"
done
