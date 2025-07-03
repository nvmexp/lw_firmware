/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SEC2_BAR0_HS_H
#define SEC2_BAR0_HS_H

#include "dev_sec_csb.h"
#include "lwoslayer.h"
#include "sec2_objsec2.h"

/*!
 * @file sec2_bar0_hs.h
 * This file holds the inline definitions of BAR0 register read/write functions.
 * This header should only be included by heavy secure microcode (besides being
 * included by the HAL implementations that implement register read/writes). The
 * reason for inlining is detailed below.
 */

/*!
 * HS version of GPU BAR0/Priv Access Macros {BAR0_*_HS}
 */
#define BAR0_REG_RD32_HS(addr)                     sec2Bar0RegRd32Hs_HAL(&Sec2, addr)
#define BAR0_REG_WR32_HS(addr, data)               sec2Bar0RegWr32NonPostedHs_HAL(&Sec2, addr, data)

//
// Define HS BAR0 priv read and write macros with error handling.
// These are in libCommonHs.
//
#define BAR0_REG_RD32_HS_ERRCHK(addr, pReadVal)    sec2Bar0ErrChkRegRd32Hs_HAL(&Sec2, addr, pReadVal)
#define BAR0_REG_WR32_HS_ERRCHK(addr, data)        sec2Bar0ErrChkRegWr32NonPostedHs_HAL(&Sec2, addr, data)

//
// Note: The below inline interfaces are intentionally exposed so that they can
// be inlined in heavy secure code which cannot jump to light secure code while
// it is exelwting without ilwalidating the HS blocks. This code should ideally
// not be required once we have EMEM apertures in GP10X. We should be able to
// write assembly instructions inline to be able to handle register reads and
// writes. Please do not use the inline functions outside of heavy secure code
// to avoid code size bloat. Instead, rely on the macros to do the right thing.
//
static inline FLCN_STATUS _sec2Bar0WaitIdle_GM20X(LwBool bEnablePrivErrChk)
    GCC_ATTRIB_ALWAYSINLINE();

/*!
 * Wait for the BAR0MASTER to report a non-busy (idle) status.  Note that
 * 'idle' is defined as any non-busy. It includes the disabled and timeout
 * status codes.
 * Note that the actual functionality is implemented inside an inline function
 * to facilitate exposing the inline function to heavy secure code which cannot
 * jump to light secure code without ilwalidating the HS blocks. Once we get
 * EMEM aperture on GP10X, this code should disappear, so the code size bloat
 * because of inlining is only temporary for testing.
 */
static inline FLCN_STATUS
_sec2Bar0WaitIdle_GM20X(LwBool bEnablePrivErrChk)
{
    LwBool bDone = LW_FALSE;
    FLCN_STATUS flcnStatus = FLCN_OK;

    while (!bDone)
    {
        switch (DRF_VAL(_CSEC, _BAR0_CSR, _STATUS,
                    REG_RD32(CSB, LW_CSEC_BAR0_CSR)))
        {
            case LW_CSEC_BAR0_CSR_STATUS_IDLE:
                bDone = LW_TRUE;
                break;

            case LW_CSEC_BAR0_CSR_STATUS_BUSY:
                break;

            // unexpected error conditions
            case LW_CSEC_BAR0_CSR_STATUS_TMOUT:
            case LW_CSEC_BAR0_CSR_STATUS_DIS:
            case LW_CSEC_BAR0_CSR_STATUS_ERR:
            default:
            {
                //
                // If we are doing priv error handling (PEH), then record the error on WFI.
                // When we are not doing PEH, preserve the existing behavior of FALC_HALT.
                // Eventually all code will do PEH, and the HALT  will be removed as well as the
                // bEnablePrivErrChk param.
                //
                if (bEnablePrivErrChk)
                {
                    flcnStatus = FLCN_ERR_WAIT_FOR_BAR0_IDLE_FAILED;
                    bDone = LW_TRUE;
                    break;
                }
                else
                {
                    OS_HALT();
                }
            }
        }
    }

    return flcnStatus;
}

static inline void _sec2Bar0RegWr32NonPosted_GM20X(LwU32 addr, LwU32 data)
    GCC_ATTRIB_ALWAYSINLINE();

/*!
 * Writes a 32-bit value to the given BAR0 address.  This is the nonposted form
 * (will wait for transaction to complete) of bar0RegWr32Posted.  It is safe to
 * call it repeatedly and in any combination with other BAR0MASTER functions.
 *
 * @param[in]  addr  The BAR0 address to write
 * @param[in]  data  The data to write to the address
 */
static inline void
_sec2Bar0RegWr32NonPosted_GM20X
(
    LwU32  addr,
    LwU32  data
)
{
    // If we are not doing PEH,  we don't need to check the return value.
    (void)_sec2Bar0WaitIdle_GM20X(LW_FALSE);

    REG_WR32(CSB, LW_CSEC_BAR0_ADDR, addr);
    REG_WR32(CSB, LW_CSEC_BAR0_DATA, data);

    REG_WR32(CSB, LW_CSEC_BAR0_CSR,
        DRF_DEF(_CSEC, _BAR0_CSR, _CMD ,        _WRITE) |
        DRF_NUM(_CSEC, _BAR0_CSR, _BYTE_ENABLE, 0xF   ) |
        DRF_DEF(_CSEC, _BAR0_CSR, _TRIG,        _TRUE ));

    (void)REG_RD32(CSB, LW_CSEC_BAR0_CSR);

    // If we are not doing PEH,  we don't need to check the return value.
    (void)_sec2Bar0WaitIdle_GM20X(LW_FALSE);
}

static inline LwU32 _sec2Bar0RegRd32_GM20X(LwU32 addr)
    GCC_ATTRIB_ALWAYSINLINE();

/*!
 * Reads the given BAR0 address. The read transaction is nonposted.
 *
 * @param[in]  addr        The BAR0 address to write
 *
 * @return The 32-bit value of the requested BAR0 address
 */
static inline LwU32
_sec2Bar0RegRd32_GM20X
(
    LwU32  addr
)
{
    LwU32  val;

    // If we are not doing PEH,  we don't need to check the return value.
    (void)_sec2Bar0WaitIdle_GM20X(LW_FALSE);
    REG_WR32(CSB, LW_CSEC_BAR0_ADDR, addr);

    REG_WR32_STALL(CSB, LW_CSEC_BAR0_CSR,
        DRF_DEF(_CSEC, _BAR0_CSR, _CMD ,        _READ) |
        DRF_NUM(_CSEC, _BAR0_CSR, _BYTE_ENABLE, 0xF  ) |
        DRF_DEF(_CSEC, _BAR0_CSR, _TRIG,        _TRUE));

    (void)REG_RD32(CSB, LW_CSEC_BAR0_CSR);

    // If we are not doing PEH,  we don't need to check the return value.
    (void)_sec2Bar0WaitIdle_GM20X(LW_FALSE);
    val = REG_RD32(CSB, LW_CSEC_BAR0_DATA);
    return val;
}

#endif // SEC2_BAR0_HS_H
