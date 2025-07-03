/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SOE_BAR0_HS_INLINES_H
#define SOE_BAR0_HS_INLINES_H

/*!
 * @file soe_bar0_has_inlines.h
 * 
 * @brief   These functions are implemented as inline
 * to leverage source code reuse while ensuring
 * that HS images will get their own copy of the functions,
 * i.e. SOE will not call into the LS image to execute them.
 */

#include "lwuproc.h"
#include "lwoslayer.h"
#include "dev_soe_csb.h"
#include "csb.h"
#include "dev_fuse.h"
#include "dev_lws_master_addendum.h"


static inline void _soeBar0InitTmout_LR10(void)
    #if HS_INLINES
    GCC_ATTRIB_SECTION("imem_hs", "_soeBar0InitTmout_LR10")
    GCC_ATTRIB_USED()
    #endif
    GCC_ATTRIB_ALWAYSINLINE();

static inline LwU32 _soeBar0RegRd32_LR10(LwU32 addr)
    #if HS_INLINES
    GCC_ATTRIB_SECTION("imem_hs", "_soeBar0RegRd32_LR10")
    GCC_ATTRIB_USED()
    #endif
    GCC_ATTRIB_ALWAYSINLINE();

static inline void _soeBar0RegWr32Stall_LR10(LwU32 addr, LwU32 value)
    #if HS_INLINES
    GCC_ATTRIB_SECTION("imem_hs", "_soeBar0RegWr32Stall_LR10")
    GCC_ATTRIB_USED()
    #endif
    GCC_ATTRIB_ALWAYSINLINE();

static inline FLCN_STATUS _soeBar0WaitIdle_LR10(LwBool bEnablePrivErrChk)
    #if HS_INLINES
    GCC_ATTRIB_SECTION("imem_hs", "_soeBar0WaitIdle_LR10")
    GCC_ATTRIB_USED()
    #endif
    GCC_ATTRIB_ALWAYSINLINE();


static inline void
_soeBar0InitTmout_LR10(void)
{
    //
    // Initialize the timeout value within which the host has to ack a read or
    // write transaction.
    //
    // TODO (nkuo): Due to bug 1758045, we need to use a super large timeout to
    // suppress some of the timeout failures.  Will need to restore this value
    // once the issue is resolved.
    // (Ray Ngai found that 0x4000 was not sufficient with TOT, so 0xFFFFFF
    // is picked up for now)
    //
    REG_WR32(CSB, LW_CSOE_BAR0_TMOUT,
             DRF_NUM(_CSOE, _BAR0_TMOUT, _CYCLES, 0xFFFFFF));
}

/*!
 * Wait for the BAR0MASTER to report a non-busy (idle) status.  Note that
 * 'idle' is defined as any non-busy. It includes the disabled and timeout
 * status codes.
 */
static inline FLCN_STATUS
_soeBar0WaitIdle_LR10(LwBool bEnablePrivErrChk)
{
    LwBool bDone = LW_FALSE;
    FLCN_STATUS flcnStatus = FLCN_OK;

    while (!bDone)
    {
        switch (DRF_VAL(_CSOE, _BAR0_CSR, _STATUS,
                    REG_RD32(CSB, LW_CSOE_BAR0_CSR)))
        {
            case LW_CSOE_BAR0_CSR_STATUS_IDLE:
                bDone = LW_TRUE;
                break;

            case LW_CSOE_BAR0_CSR_STATUS_BUSY:
                break;

            // unexpected error conditions
            case LW_CSOE_BAR0_CSR_STATUS_TMOUT:
            case LW_CSOE_BAR0_CSR_STATUS_DIS:
            case LW_CSOE_BAR0_CSR_STATUS_ERR:
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

/*!
 * Reads the given BAR0 address. The read transaction is nonposted.
 *
 * @param[in]  addr        The BAR0 address to write
 *
 * @return The 32-bit value of the requested BAR0 address
 */
static inline LwU32
_soeBar0RegRd32_LR10
(
    LwU32  addr
)
{
    LwU32  val;

    // If we are not doing PEH,  we don't need to check the return value.
    (void)_soeBar0WaitIdle_LR10(LW_FALSE);
    REG_WR32(CSB, LW_CSOE_BAR0_ADDR, addr);

    REG_WR32_STALL(CSB, LW_CSOE_BAR0_CSR,
        DRF_DEF(_CSOE, _BAR0_CSR, _CMD ,        _READ) |
        DRF_NUM(_CSOE, _BAR0_CSR, _BYTE_ENABLE, 0xF  ) |
        DRF_DEF(_CSOE, _BAR0_CSR, _TRIG,        _TRUE));

    (void)REG_RD32(CSB, LW_CSOE_BAR0_CSR);

    // If we are not doing PEH,  we don't need to check the return value.
    (void)_soeBar0WaitIdle_LR10(LW_FALSE);
    val = REG_RD32(CSB, LW_CSOE_BAR0_DATA);
    return val;
}

/*!
 * Writes the given BAR0 address with the value provided.
 * The write transaction is nonposted.
 *
 * @param[in]  addr        The BAR0 address to write
 * @param[in]  value       The 32-bit value to write
 *
 */
static inline void
_soeBar0RegWr32Stall_LR10
(
    LwU32 addr,
    LwU32 value
)
{
    // If we are not doing PEH,  we don't need to check the return value.
    (void)_soeBar0WaitIdle_LR10(LW_FALSE);
    REG_WR32(CSB, LW_CSOE_BAR0_ADDR, addr);
    REG_WR32(CSB, LW_CSOE_BAR0_DATA, value);

    REG_WR32_STALL(CSB, LW_CSOE_BAR0_CSR,
        DRF_DEF(_CSOE, _BAR0_CSR, _CMD ,        _WRITE) |
        DRF_NUM(_CSOE, _BAR0_CSR, _BYTE_ENABLE, 0xF  ) |
        DRF_DEF(_CSOE, _BAR0_CSR, _TRIG,        _TRUE));

    (void)REG_RD32(CSB, LW_CSOE_BAR0_CSR);

    // If we are not doing PEH,  we don't need to check the return value.
    (void)_soeBar0WaitIdle_LR10(LW_FALSE);
}

#endif // SOE_BAR0_HS_INLINES_H
