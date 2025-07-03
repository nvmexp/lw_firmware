/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SEC2_PRIERR_H
#define SEC2_PRIERR_H

#include "dev_sec_csb.h"
#include "sec2_objsec2.h"
#include "dev_master.h"

//
// These are included for checking for exceptions for bad read
// return values for timers.
//
#include "dev_sec_pri.h"
#include "dev_gc6_island.h"
#include "dev_timer.h"

/*!
 * @file sec2_prierr.h 
 * This file holds the inline fucntions for Priv Error Handling (PEH).
 * The functions are inlined to reduce duplication between LS and HS mode.
 * The main read and write PEH functions are called by LS/HS HAL functions.
 * If the HAL needs to be forked for a particular chip,  then a new inline
 * function will need to be forked for that chip. This will be the case
 * while we are using inline functions to share code between LS/HS.
 * 
 * There is planned infrastructure to support shared LS/HS code.  When 
 * that infrastructure is complete, PEH support will use that instead of
 * inline functions.
 */

/*
 * The following four functions are the main inline error handling functions that 
 * are called by wrapper HAL calls.
 */
static inline FLCN_STATUS _sec2CsbErrorChkRegRd32_GP10X(LwU32  addr, LwU32  *pData)
    GCC_ATTRIB_ALWAYSINLINE();
static inline FLCN_STATUS _sec2CsbErrorChkRegWr32NonPosted_GP10X(LwU32  addr, LwU32 data)
    GCC_ATTRIB_ALWAYSINLINE();
static inline FLCN_STATUS _sec2Bar0ErrorChkRegRd32_GP10X(LwU32  addr, LwU32  *pData)
    GCC_ATTRIB_ALWAYSINLINE();
static inline FLCN_STATUS _sec2Bar0ErrorChkRegWr32NonPosted_GP10X(LwU32  addr, LwU32  data)
    GCC_ATTRIB_ALWAYSINLINE();

/*
 * The following functions are support functions for above and have no corresponding HAL 
 * calls.
 */
static inline void _sec2Bar0ClearCsrErrors_GP10X(void)
    GCC_ATTRIB_ALWAYSINLINE();


/*!
 * Reads the given CSB address. The read transaction is nonposted.
 * 
 * Checks the return read value for priv errors and returns a status.
 * It is up the the calling apps to decide how to handle a failing status.
 *
 * @param[in]  addr   The CSB address to read
 * @param[out] *pData The 32-bit value of the requested address.
 *                    This function allows this value to be NULL
 *                    as well.
 *
 * @return The status of the read operation.
 */
inline FLCN_STATUS
_sec2CsbErrorChkRegRd32_GP10X
(
    LwU32  addr,
    LwU32  *pData
)
{
    LwU32                 val;
    LwU32                 csbErrStatVal = 0;
    FLCN_STATUS           flcnStatus    = FLCN_ERR_CSB_PRIV_READ_ERROR;

    // Read and save the requested register and error register
    val = falc_ldxb_i ((LwU32*)(addr), 0);
    csbErrStatVal = falc_ldxb_i ((LwU32*)(LW_CSEC_FALCON_CSBERRSTAT), 0);

    // If FALCON_CSBERRSTAT_VALID == _FALSE, CSB read succeeded
    if (FLD_TEST_DRF(_PFALCON, _FALCON_CSBERRSTAT, _VALID, _FALSE, csbErrStatVal))
    {
        //
        // It is possible that sometimes we get bad data on CSB despite hw
        // thinking that read was a success. Apply a heuristic to trap such
        // cases.
        //
        if ((val & FLCN_CSB_PRIV_PRI_ERROR_MASK) == FLCN_CSB_PRIV_PRI_ERROR_CODE)
        {
            if (addr == LW_CSEC_FALCON_PTIMER1)
            {
                //
                // PTIMER1 can report a constant 0xbadfxxxx for a long time, so
                // just make an exception for it.
                //
                flcnStatus = FLCN_OK;
            }
            else if (addr == LW_CSEC_FALCON_PTIMER0)
            {
                //
                // PTIMER0 can report a 0xbadfxxxx value for up to 2^16 ns, but it
                // should keep changing its value every nanosecond. We just check
                // to make sure we can detect its value changing by reading it a
                // few times.
                //
                LwU32   val1 = 0;
                LwU32   i;

                for (i = 0; i < 3; i++)
                {
                    val1 = falc_ldx_i((LwU32 *)(addr), 0);
                    if (val1 != val)
                    {
                        val = val1;
                        flcnStatus = FLCN_OK;
                        break;
                    }
                }
            }
        }
        else
        {
            flcnStatus = FLCN_OK;
        }
    }

    //
    // Always update the output read data value, even in case of error.
    // The data is always invalid in case of error,  so it doesn't matter
    // what the value is.  The caller should detect the error case and
    // act appropriately.
    //
    if (pData != NULL)
    {
        *pData = val;
    }
    return flcnStatus;
}


/*!
 * Writes a 32-bit value to the given CSB address. 
 *
 * After writing the registers, check for issues that may have oclwrred with
 * the write and return status.  It is up the the calling apps to decide how
 * to handle a failing status.
 *
 * According to LW Bug 292204 -  falcon function "stxb" is the non-posted
 * version for CSB writes.
 *
 * @param[in]  addr  The CSB address to write
 * @param[in]  data  The data to write to the address
 *
 * @return The status of the write operation.
 */
static inline FLCN_STATUS
_sec2CsbErrorChkRegWr32NonPosted_GP10X
(
    LwU32  addr,
    LwU32  data
)
{
    LwU32                 csbErrStatVal = 0;
    FLCN_STATUS           flcnStatus    = FLCN_OK;

    // Write requested register value and read error register
    falc_stxb_i((LwU32*)(addr), 0, data);
    csbErrStatVal = falc_ldxb_i((LwU32*)(LW_CSEC_FALCON_CSBERRSTAT), 0);

    if (FLD_TEST_DRF(_PFALCON, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatVal))
    {
        flcnStatus = FLCN_ERR_CSB_PRIV_WRITE_ERROR;
    }

    return flcnStatus;
}

/*!
 * Reads the given BAR0 address. The read transaction is nonposted.
 *
 * Checks the return read value for priv errors and returns a status.
 * It is up the the calling apps to decide how to handle a failing status.
 *
 * @param[in]   addr  The BAR0 address to read
 * @param[out] *pData The 32-bit value of the requested BAR0 address.
 *                    This function allows this value to be NULL as
 *                    well.
 *
 * @return The status of the read operation.
 */
static inline FLCN_STATUS
_sec2Bar0ErrorChkRegRd32_GP10X
(
    LwU32  addr,
    LwU32  *pData
)
{
    FLCN_STATUS           flcnStatus = FLCN_OK;
    LwU32                 val        = FLCN_BAR0_PRIV_PRI_RETURN_VAL;   // Init a return value for case failure 
                                                                        // oclwrs before getting a value back
    //
    // For PEH, we need to check the return value of WFI, so rather than
    // call inline the inline function, _sec2Bar0RegRd32_GM20X, to execute
    // the read, the read code is duplicated in PEH functions so that we
    // can check the WFI return value at each invocation and act appropriately
    // without complicating the existing inline functions.
    //

    // WFI and check WFI return status
    flcnStatus = _sec2Bar0WaitIdle_GM20X(LW_TRUE);
    if (flcnStatus != FLCN_OK)
    {
        flcnStatus = FLCN_ERR_BAR0_PRIV_READ_ERROR;
        goto ErrorExit;
    }

    // Set up the read
    CHECK_FLCN_STATUS(_sec2CsbErrorChkRegWr32NonPosted_GP10X(LW_CSEC_BAR0_ADDR, addr));
    CHECK_FLCN_STATUS(_sec2CsbErrorChkRegWr32NonPosted_GP10X(LW_CSEC_BAR0_CSR,
            DRF_DEF(_CSEC, _BAR0_CSR, _CMD ,        _READ) |
            DRF_NUM(_CSEC, _BAR0_CSR, _BYTE_ENABLE, 0xF  ) |
            DRF_DEF(_CSEC, _BAR0_CSR, _TRIG,        _TRUE)));
    CHECK_FLCN_STATUS(_sec2CsbErrorChkRegRd32_GP10X(LW_CSEC_BAR0_CSR, NULL));

    // WFI and check WFI return status
    flcnStatus = _sec2Bar0WaitIdle_GM20X(LW_TRUE);
    if (flcnStatus != FLCN_OK)
    {
        flcnStatus = FLCN_ERR_BAR0_PRIV_READ_ERROR;
        goto ErrorExit;
    }

    //
    // Bad reads should be caught by checking BAR0_CSR_STATUS in _sec2Bar0WaitIdle_GM20X,
    // but check the read return value in case the failure is missed by HW.
    // Exceptions are added for timer registers.
    //
    CHECK_FLCN_STATUS(_sec2CsbErrorChkRegRd32_GP10X(LW_CSEC_BAR0_DATA, &val));

    // 
    // Initialize fncnStatus to security safe default value of READ_ERROR to ensure
    // that in case of bugs below we fall on the safer side by returning an error.
    //
    flcnStatus = FLCN_ERR_BAR0_PRIV_READ_ERROR;
    if ((val & FLCN_BAR0_PRIV_PRI_ERROR_MASK) == FLCN_BAR0_PRIV_PRI_ERROR_CODE)
    {
        if (addr == LW_PSEC_FALCON_PTIMER1)
        {
            //
            // PTIMER1 can report a constant 0xbadfxxxx for a long time, so
            // just make an exception for it. LW_PGC6_SCI_PTIMER_TIME_1 and
            // LW_PTIMER_TIME_1 will always contain zero in the 3 most 
            // significant bits, so it will never contain "bad".
            //
            flcnStatus = FLCN_OK;
        }
        else if ((addr == LW_PSEC_FALCON_PTIMER0)    ||
                 (addr == LW_PGC6_SCI_PTIMER_TIME_0) ||
                 (addr == LW_PTIMER_TIME_0))
        {
            LwU32   val1 = 0;
            LwU32   i;

            //
            // TIME(R)0 can report a 0xbadfxxxx value for up to 2^16 ns, but it
            // should keep changing its value every nanosecond. We just check
            // to make sure we can detect its value changing by reading it a
            // few times.
            //
            // Use reads without error checking.  We have previously set the 
            // flcnStaus to READ_ERROR, if we encounter an issue.
            //
            for (i = 0; i < 3; i++)
            {
                val1 = _sec2Bar0RegRd32_GM20X(addr);
                if (val1 != val)
                {
                    val = val1;
                    flcnStatus = FLCN_OK;
                    break;
                }
            }
        }
    }
    else
    {
        flcnStatus = FLCN_OK;
    }

ErrorExit:
    //
    // Always update the output read data value, even in case of error.
    // The data is always invalid in case of error,  so it doesn't matter
    // what the value is.  The caller should detect the error case and
    // act appropriately.
    //
    if (pData != NULL)
    {
        *pData = val;
    }

    // Initiate a BAR0_CSR CSB read access to clear errors
    if (flcnStatus != FLCN_OK)
    {
        _sec2Bar0ClearCsrErrors_GP10X();
    }
    return flcnStatus;
}

/*!
 * Writes a 32-bit value to the given BAR0 address.  This is the nonposted form
 * (will wait for transaction to complete) of bar0RegWr32Posted.  It is safe to
 * call it repeatedly and in any combination with other BAR0MASTER functions.
 *
 * Note that the actual functionality is implemented inside an inline function
 * to facilitate exposing the inline function to heavy secure code which cannot
 * jump to light secure code without ilwalidating the HS blocks.
 *
 * It is up the the calling code to decide how to handle a failing status.
 *
 * @param[in]  addr  The BAR0 address to write
 * @param[in]  data  The data to write to the address
 *
 * @return The status of the write operation.
 */
static inline FLCN_STATUS
_sec2Bar0ErrorChkRegWr32NonPosted_GP10X
(
    LwU32  addr,
    LwU32  data
)
{
    FLCN_STATUS           flcnStatus = FLCN_OK;

    // 
    // For PEH, we need to check the return value of WFI, so rather than
    // call the inline function, _sec2Bar0RegWr32NonPosted_GM20X, to execute
    // the write, the write code is duplicated in PEH functionsvso that we
    // can check the WFI return value at each invocation and act
    // appropriately without complicating the existing inline function.
    //

    // WFI and check WFI return status
    flcnStatus = _sec2Bar0WaitIdle_GM20X(LW_TRUE);
    if (flcnStatus != FLCN_OK)
    {
        flcnStatus = FLCN_ERR_BAR0_PRIV_WRITE_ERROR;
        goto ErrorExit;
    }

    // Do the write
    CHECK_FLCN_STATUS(_sec2CsbErrorChkRegWr32NonPosted_GP10X(LW_CSEC_BAR0_ADDR, addr));
    CHECK_FLCN_STATUS(_sec2CsbErrorChkRegWr32NonPosted_GP10X(LW_CSEC_BAR0_DATA, data));
    CHECK_FLCN_STATUS(_sec2CsbErrorChkRegWr32NonPosted_GP10X(LW_CSEC_BAR0_CSR,
            DRF_DEF(_CSEC, _BAR0_CSR, _CMD ,        _WRITE) |
            DRF_NUM(_CSEC, _BAR0_CSR, _BYTE_ENABLE, 0xF   ) |
            DRF_DEF(_CSEC, _BAR0_CSR, _TRIG,        _TRUE )));
    CHECK_FLCN_STATUS(_sec2CsbErrorChkRegRd32_GP10X(LW_CSEC_BAR0_CSR, NULL));

    // WFI and check WFI return status
    flcnStatus = _sec2Bar0WaitIdle_GM20X(LW_TRUE);
    if (flcnStatus != FLCN_OK)
    {
        flcnStatus = FLCN_ERR_BAR0_PRIV_WRITE_ERROR;
    }

ErrorExit:

    // Initiate a BAR0_CSR CSB read access to clear errors
    if (flcnStatus != FLCN_OK)
    {
        _sec2Bar0ClearCsrErrors_GP10X();
    }
    return flcnStatus;
}

/*!
 * Clears pending errors on BAR0_CSR by initiating a CSR read
 *
 * Once we have an error of LW_CSEC_BAR0_CSR_STATUS_TMOUT or
 * case LW_CSEC_BAR0_CSR_STATUS_DIS (or others),  the status will
 * remain until we have a successful CSR read.  Unless the error
 * status is cleared,  all subsequent calls to  sec2Bar0WaitIdle
 * will fail.
 *
 * See https://sc-refmanuals.lwpu.com/~gpu_refmanuals/gp104/LW_CSEC.ref#125
 * STATUS :3 (R)
 * Determines the Status of the previous Command. When status is ERR or TOUT, 
 * the register is reset to IDLE on read access.
 */
static inline void
_sec2Bar0ClearCsrErrors_GP10X(void)
{
    LwU32 data = 0;

    // Initiate a BAR0_CSR CSB read access to clear errors
    REG_WR32(CSB, LW_CSEC_BAR0_ADDR, LW_PMC_ENABLE);

    REG_WR32_STALL(CSB, LW_CSEC_BAR0_CSR,
        DRF_DEF(_CSEC, _BAR0_CSR, _CMD ,        _READ) |
        DRF_NUM(_CSEC, _BAR0_CSR, _BYTE_ENABLE, 0xF  ) |
        DRF_DEF(_CSEC, _BAR0_CSR, _TRIG,        _TRUE));

    (void)REG_RD32(CSB, LW_CSEC_BAR0_CSR);

    // If we are not doing PEH,  we don't need to check the return value.
    (void)_sec2Bar0WaitIdle_GM20X(LW_FALSE);
    data = REG_RD32(CSB, LW_CSEC_BAR0_DATA);
}

#endif // SEC2_PRIERR_H
