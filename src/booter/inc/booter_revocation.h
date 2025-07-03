/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BOOTER_REVOCATION_H
#define BOOTER_REVOCATION_H

#include <lwtypes.h>
#include <lwmisc.h>

/*!
 * Utilities for GSP-RM revocation based on LS ucode version and
 * VBIOS secure scratch registers.
 *
 * See https://confluence.lwpu.com/display/GSP/GSP-RM+Revocation
 */

/*
 * LS ucode version
 *
 * GSP-RM LS ucode version is made up of two components. A revision number
 * and a branch number.
 *
 *   REV_NUM (6 bits):
 *     Number increased for every revocable GSP-RM version
 *   BRANCH_NUM (6 bits):
 *     Number encoding branch from which GSP-RM was built from (see below)
 */
#define GSPRM_LS_UCODE_VER_REV_NUM             5:0
#define GSPRM_LS_UCODE_VER_BRANCH_NUM          11:6

/*
 * An invalid revision number that is larger than all possible revision
 * numbers (meaning all revocation checks using this as the minimum will fail)
 */
#define GSPRM_REVOCATION_ILWALID_REV_NUM       ((LwU32) -1)

/*
 * Branches known to Booter as encoded in GSP-RM's
 * LS ucode version's BRANCH_NUM
 */
#define GSPRM_LS_UCODE_VER_BRANCH_CHIPS_A      0
#define GSPRM_LS_UCODE_VER_BRANCH_R515         1

/*
 * Secure scratch registers containing GSP-RM revocation information.
 *
 * One of the following registers:
 *   LW_PGC6_AON_SELWRE_SCRATCH_GROUP_19 
 *   LW_PGC6_AON_SELWRE_SCRATCH_GROUP_25(i) where i = 0 to 3
 *
 * Each register is made up of five components. One minimum revision number
 * for each of five branches.
 *
 *   MIN_REV_FOR_BRANCH(branchIdx) (6 bits):
 *     Minimum revision number for GSP-RM for a particular branch.
 *     GSP-RM with REV_NUM less than this number for the corresponding branch
 *     are considered to be revoked.
 */
#define GSPRM_REVOCATION_NUM_BRANCHES_PER_REG     5
#define GSPRM_REVOCATION_NUM_REV_BITS_PER_BRANCH  6

/*
 * Retrieve the minimum revision number encoded in a register given the value
 * read from the appropriate register and a branch number.
 *
 * The caller is responsible for reading the appropriate secure scratch
 * register (and providing the value read) for the given branch.
 */
static inline LwU32 gsprmRevocationGetMinRevNumFromRegVal(LwU32 regVal, LwU32 branchNum)
{
    LwU32 branchIdxInReg = branchNum % GSPRM_REVOCATION_NUM_BRANCHES_PER_REG;
    LwU32 shiftAmount = branchIdxInReg * GSPRM_REVOCATION_NUM_REV_BITS_PER_BRANCH;
    LwU32 mask;
    LwU32 minRevNum;

    // note: we only use lower 30 bits, assert that high bit is unused
    ct_assert(GSPRM_REVOCATION_NUM_BRANCHES_PER_REG * GSPRM_REVOCATION_NUM_REV_BITS_PER_BRANCH < 32);
    // ensure high bit is not set (as it would for e.g. 0xbadfxxxx)
    if ((regVal >> 31) != 0)
    {
        return GSPRM_REVOCATION_ILWALID_REV_NUM;
    }

    mask = ((1u << GSPRM_REVOCATION_NUM_REV_BITS_PER_BRANCH) - 1);
    minRevNum = (regVal >> shiftAmount) & mask;

    if (minRevNum == mask)
    {
        //
        // register encodes highest possible minimum revision number for branch,
        // consider entire branch revoked
        //
        return GSPRM_REVOCATION_ILWALID_REV_NUM;
    }

    return minRevNum;
}

/*
 * Retrieve the hard-coded minimum revision number for a given branch.
 */
static inline LwU32 gsprmRevocationGetHardcodedMinRevNumForBranch(LwU32 branchNum)
{
    switch (branchNum)
    {
        case GSPRM_LS_UCODE_VER_BRANCH_CHIPS_A:
            return 1;
        case GSPRM_LS_UCODE_VER_BRANCH_R515:
            return 1;
        default:
            return 1;
    }
}

#endif // BOOTER_REVOCATION_H
