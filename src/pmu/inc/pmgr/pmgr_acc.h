/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmgr_acc.h
 * @copydoc pmgr_acc.c
 */

#ifndef PMGR_ACC_H
#define PMGR_ACC_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
/* ------------------------- Macros ---------------------------------------- */
/* ------------------------- Types Definitions ----------------------------- */
/*!
 * Structure containing aclwmulator values from last read. Values are stored
 * in two units; the source units and the destination units. These units and
 * their scaling factors are to be specified by the clients.
 *
 * The reason for using differential readings to acheive this src->dst scaling
 * is to avoid underflow which will happen whenever the scaling yields less
 * than 64 bits.
 *
 * Examples:
 * Input/source - HW acc for power in code*cycles represented as FXP 59.34
 * Output/destination - SW acc for power in mW*us (nJ) which will be FXP 59.0
 *     after scaling
 *
 * Numerical analysis of scaling
 *     46.18 code*cycle * 13.16 mW*us/code/cycle =>  59.34 mW*us
 *     59.34 mW*us >> 34                         =>  59.0  mW*us
 *
 * Overflow/wrap-aroud in HW acc leading to underflow in SW acc
 *     When HW acc overflows/wraps-around, the difference between lwrr HW acc
 *     and last HW acc value will give the correct power value in ADC code but
 *     the corresponding difference between the lwrr SW acc and last SW acc
 *     will be incorrect (unexpectedly large) due to the SW acc going from
 *     ~2^59 nJ to few nJ. We solve this problem by taking differential read of
 *     the HW acc, scale to SI units and then add to the SW acc. Differentail
 *     reads is not expected to overflow 64 bits and hence no underflow.
 *
 * Similar issue is resolved when colwerting the 64.0 nJ value to 44.0 mJ value
 *     64.0 nJ * (1/1000000) = 44.0 mJ
 */
typedef struct
{
    /*!
     * Last aclwmulator reading in source units. Take differential readings
     * against this value and add it into @ref accDest after colwerting
     * from source to destination units.
     */
    LwU64    srcLast;
    /*!
     * Last aclwmulator reading in destination units. Accumulate into this
     * as differential readings of the @ref accSrc, after proper
     * colwersion from source units to destination units.
     */
    LwU64    dstLast;
    /*!
     * Scaling factor to colwert from source unit to destination unit.
     */
    LwUFXP32 scaleFactor;
    /*!
     * Number of bits to be right shifted in pmgrAclwpdate to remove all
     * fractional bits from the result.
     */
    LwU8     shiftBits;
} PMGR_ACC;

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/*!
 * Update the given PMGR_ACC.
 * @ref PMGR_ACC::srcLast is updated with current aclwmulator value (*pSrcLwrr)
 * @ref PMGR_ACC::dstLast, i.e. the destination aclwmulator is updated by adding
 * the difference between PMGR_ACC::srcLast and *pSrcLwrr after proper colwersion
 *
 * @param[in/out] pAcc         Pointer to PMGR_ACC
 * @param[in]     pSrcLwrr     Pointer to variable having current aclwmulator
 *     value in source units
 */
void pmgrAclwpdate(PMGR_ACC *pAcc, LWU64_TYPE *pSrcLwrr)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pmgrAclwpdate");

/* ------------------------ Inline Functions  -------------------------------- */
/*!
 * @brief   Accessor for PMGR_ACC::dstLast.
 *
 * @param[in]   pAcc    PMGR_ACC pointer.
 *
 * @return  @ref PMGR_ACC::dstLast pointer for the passed in object.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwU64 *
PMGR_ACC_DST_GET
(
    PMGR_ACC *pAcc
)
{
    return &(pAcc->dstLast);
}

/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/* ------------------------- Debug Macros ---------------------------------- */

#endif // PMGR_ACC_H
