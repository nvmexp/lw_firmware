/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef GSP_PRIERR_H
#define GSP_PRIERR_H

#include "dev_gsp_csb.h"
#include "dpu_objdpu.h"
#include "dev_master.h"

//
// These are included for checking for exceptions for bad read
// return values for timers, i2c and dpaux.
//
#include "dev_gsp.h"
#include "dev_gc6_island.h"
#include "dev_timer.h"
#include "dev_pmgr.h"

/*!
 * @file gsp_prierr.h 
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
static inline FLCN_STATUS _dpuCsbErrorChkRegRd32_GV10X(LwUPtr addr, LwU32 *pData)
    GCC_ATTRIB_ALWAYSINLINE();
static inline FLCN_STATUS _dpuCsbErrorChkRegWr32NonPosted_GV10X(LwUPtr addr, LwU32 data)
    GCC_ATTRIB_ALWAYSINLINE();
static inline FLCN_STATUS _dpuBar0ErrorChkRegRd32_GV10X(LwU32 addr, LwU32 *pData)
    GCC_ATTRIB_ALWAYSINLINE();
static inline FLCN_STATUS _dpuBar0ErrorChkRegWr32NonPosted_GV10X(LwU32 addr, LwU32 data)
    GCC_ATTRIB_ALWAYSINLINE();

/*
 * The following functions are support functions for above and have no corresponding HAL 
 * calls.
 */
static inline void _dpuBar0ClearCsrErrors_GV10X(void)
    GCC_ATTRIB_ALWAYSINLINE();

//
// Adding this assert to remind we should update code in bar0 read error handling as well 
// if there is a change in number of i2c ports/ dp aux port. 
//
ct_assert(LW_PMGR_I2C_DATA__SIZE_1 == 10);
ct_assert(LW_PMGR_DP_AUXDATA_WRITE_W0__SIZE_1 == 7);
ct_assert(LW_PMGR_DP_AUXDATA_WRITE_W1__SIZE_1 == 7);
ct_assert(LW_PMGR_DP_AUXDATA_WRITE_W2__SIZE_1 == 7);
ct_assert(LW_PMGR_DP_AUXDATA_WRITE_W3__SIZE_1 == 7);


static LwU32 privErrExceptionAddrs[] =
{
    LW_PGSP_FALCON_PTIMER1,
    LW_PMGR_I2C_DATA(0),
    LW_PMGR_I2C_DATA(1),
    LW_PMGR_I2C_DATA(2),
    LW_PMGR_I2C_DATA(3),
    LW_PMGR_I2C_DATA(4),
    LW_PMGR_I2C_DATA(5),
    LW_PMGR_I2C_DATA(6),
    LW_PMGR_I2C_DATA(7),
    LW_PMGR_I2C_DATA(8),
    LW_PMGR_I2C_DATA(9),
    LW_PMGR_DP_AUXDATA_READ_W0(0),
    LW_PMGR_DP_AUXDATA_READ_W0(1),
    LW_PMGR_DP_AUXDATA_READ_W0(2),
    LW_PMGR_DP_AUXDATA_READ_W0(3),
    LW_PMGR_DP_AUXDATA_READ_W0(4),
    LW_PMGR_DP_AUXDATA_READ_W0(5),
    LW_PMGR_DP_AUXDATA_READ_W0(6),
    LW_PMGR_DP_AUXDATA_READ_W1(0),
    LW_PMGR_DP_AUXDATA_READ_W1(1),
    LW_PMGR_DP_AUXDATA_READ_W1(2),
    LW_PMGR_DP_AUXDATA_READ_W1(3),
    LW_PMGR_DP_AUXDATA_READ_W1(4),
    LW_PMGR_DP_AUXDATA_READ_W1(5),
    LW_PMGR_DP_AUXDATA_READ_W1(6),
    LW_PMGR_DP_AUXDATA_READ_W2(0),
    LW_PMGR_DP_AUXDATA_READ_W2(1),
    LW_PMGR_DP_AUXDATA_READ_W2(2),
    LW_PMGR_DP_AUXDATA_READ_W2(3),
    LW_PMGR_DP_AUXDATA_READ_W2(4),
    LW_PMGR_DP_AUXDATA_READ_W2(5),
    LW_PMGR_DP_AUXDATA_READ_W2(6),
    LW_PMGR_DP_AUXDATA_READ_W3(0),
    LW_PMGR_DP_AUXDATA_READ_W3(1),
    LW_PMGR_DP_AUXDATA_READ_W3(2),
    LW_PMGR_DP_AUXDATA_READ_W3(3),
    LW_PMGR_DP_AUXDATA_READ_W3(4),
    LW_PMGR_DP_AUXDATA_READ_W3(5),
    LW_PMGR_DP_AUXDATA_READ_W3(6),
};

/*!
 * Reads the given CSB address. The read transaction is nonposted.
 * 
 * Checks the return read value for priv errors and returns a status.
 * It is up the the calling apps to decide how to handle a failing status.
 *
 * @param[in]  addr   The CSB address to read
 * @param[out] pData  The 32-bit value of the requested address.
 *                    This function allows this value to be NULL
 *                    as well.
 *
 * @return The status of the read operation.
 */
inline FLCN_STATUS
_dpuCsbErrorChkRegRd32_GV10X
(
    LwUPtr  addr,
    LwU32  *pData
)
{
    LwU32       val;
    LwU32       csbErrStatVal = 0;
    FLCN_STATUS flcnStatus    = FLCN_ERR_CSB_PRIV_READ_ERROR;

    // Read and save the requested register and error register
    val = lwuc_ldxb_i ((LwU32*)(addr), 0);
    csbErrStatVal = lwuc_ldxb_i ((LwU32*)(LW_CGSP_FALCON_CSBERRSTAT), 0);

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
            if (addr == LW_CGSP_FALCON_PTIMER1)
            {
                //
                // PTIMER1 can report a constant 0xbadfxxxx for a long time, so
                // just make an exception for it.
                //
                flcnStatus = FLCN_OK;
            }
            else if (addr == LW_CGSP_FALCON_PTIMER0)
            {
                //
                // PTIMER0 can report a 0xbadfxxxx value for up to 2^16 ns, but it
                // should keep changing its value every nanosecond. We just check
                // to make sure we can detect its value changing by reading it a
                // few times.
                //
                LwU32 val1 = 0;
                LwU32 i;

                for (i = 0; i < 3; i++)
                {
                    val1 = lwuc_ldx_i((LwU32 *)(addr), 0);
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
_dpuCsbErrorChkRegWr32NonPosted_GV10X
(
    LwUPtr  addr,
    LwU32   data
)
{
    LwU32       csbErrStatVal = 0;
    FLCN_STATUS flcnStatus    = FLCN_OK;

    // Write requested register value and read error register
    lwuc_stxb_i((LwU32*)(addr), 0, data);
    csbErrStatVal = lwuc_ldxb_i((LwU32*)(LW_CGSP_FALCON_CSBERRSTAT), 0);

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
 * @param[out]  pData The 32-bit value of the requested BAR0 address.
 *                    This function allows this value to be NULL as
 *                    well.
 *
 * @return The status of the read operation.
 */
static inline FLCN_STATUS
_dpuBar0ErrorChkRegRd32_GV10X
(
    LwU32  addr,
    LwU32  *pData
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       val        = FLCN_BAR0_PRIV_PRI_RETURN_VAL;   // Init a return value for case failure 
                                                              // oclwrs before getting a value back
    //
    // For PEH, we need to check the return value of WFI, so rather than
    // call inline the inline function, _dpuBar0RegRd32_GV10X, to execute
    // the read, the read code is duplicated in PEH functions so that we
    // can check the WFI return value at each invocation and act appropriately
    // without complicating the existing inline functions.
    //

    // WFI and check WFI return status
    flcnStatus = _dpuBar0WaitIdle_GV10X(LW_TRUE);
    if (flcnStatus != FLCN_OK)
    {
        flcnStatus = FLCN_ERR_BAR0_PRIV_READ_ERROR;
        goto ErrorExit;
    }

    // Set up the read
    CHECK_FLCN_STATUS(_dpuCsbErrorChkRegWr32NonPosted_GV10X(LW_CGSP_BAR0_ADDR, addr));
    CHECK_FLCN_STATUS(_dpuCsbErrorChkRegWr32NonPosted_GV10X(LW_CGSP_BAR0_CSR,
            DRF_DEF(_CGSP, _BAR0_CSR, _CMD ,        _READ) |
            DRF_NUM(_CGSP, _BAR0_CSR, _BYTE_ENABLE, 0xF  ) |
            DRF_DEF(_CGSP, _BAR0_CSR, _TRIG,        _TRUE)));
    CHECK_FLCN_STATUS(_dpuCsbErrorChkRegRd32_GV10X(LW_CGSP_BAR0_CSR, NULL));

    // WFI and check WFI return status
    flcnStatus = _dpuBar0WaitIdle_GV10X(LW_TRUE);
    if (flcnStatus != FLCN_OK)
    {
        flcnStatus = FLCN_ERR_BAR0_PRIV_READ_ERROR;
        goto ErrorExit;
    }

    //
    // Bad reads should be caught by checking BAR0_CSR_STATUS in _dpuBar0WaitIdle_GV10X,
    // but check the read return value in case the failure is missed by HW.
    // Exceptions are added for timer registers.
    //
    CHECK_FLCN_STATUS(_dpuCsbErrorChkRegRd32_GV10X(LW_CGSP_BAR0_DATA, &val));

    // 
    // Initialize fncnStatus to security safe default value of READ_ERROR to ensure
    // that in case of bugs below we fall on the safer side by returning an error.
    //
    flcnStatus = FLCN_ERR_BAR0_PRIV_READ_ERROR;
    if ((val & FLCN_BAR0_PRIV_PRI_ERROR_MASK) == FLCN_BAR0_PRIV_PRI_ERROR_CODE)
    {
        LwU32 i;

        if ((addr == LW_PGSP_FALCON_PTIMER0)    ||
            (addr == LW_PGC6_SCI_PTIMER_TIME_0) ||
            (addr == LW_PTIMER_TIME_0))
        {
            LwU32   val1 = 0;

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
                val1 = _dpuBar0RegRd32_GV10X(addr);
                if (val1 != val)
                {
                    val = val1;
                    flcnStatus = FLCN_OK;
                    break;
                }
            }
        }
        else 
        {
            //
            // PTIMER1 can report a constant 0xbadfxxxx for a long time, so
            // just make an exception for it. LW_PGC6_SCI_PTIMER_TIME_1 and
            // LW_PTIMER_TIME_1 will always contain zero in the 3 most 
            // significant bits, so it will never contain "bad".
            //

            //
            // It was seen during HDCP22 authentication and certifcate read that particular monitor
            // returned data in certificate had 0xbadxxxxx which was treated as error here
            // though it was a valid data. Bug 200614494, 200628936
            // hence adding exception for i2c data read and dp aux data read 
            // TODO: We should remove complete any exception we are checking in this part of the code
            // and rely on BAR0_CSR_STATUS for error handling. Bug 200643025
            //
            for (i=0; i < sizeof(privErrExceptionAddrs)/sizeof(privErrExceptionAddrs[0]); i++)
            {
                if (addr == privErrExceptionAddrs[i])
                {
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
        _dpuBar0ClearCsrErrors_GV10X();
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
_dpuBar0ErrorChkRegWr32NonPosted_GV10X
(
    LwU32  addr,
    LwU32  data
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    // 
    // For PEH, we need to check the return value of WFI, so rather than
    // call the inline function, _dpuBar0RegWr32NonPosted_GV10X, to execute
    // the write, the write code is duplicated in PEH functionsvso that we
    // can check the WFI return value at each invocation and act
    // appropriately without complicating the existing inline function.
    //

    // WFI and check WFI return status
    flcnStatus = _dpuBar0WaitIdle_GV10X(LW_TRUE);
    if (flcnStatus != FLCN_OK)
    {
        flcnStatus = FLCN_ERR_BAR0_PRIV_WRITE_ERROR;
        goto ErrorExit;
    }

    // Do the write
    CHECK_FLCN_STATUS(_dpuCsbErrorChkRegWr32NonPosted_GV10X(LW_CGSP_BAR0_ADDR, addr));
    CHECK_FLCN_STATUS(_dpuCsbErrorChkRegWr32NonPosted_GV10X(LW_CGSP_BAR0_DATA, data));
    CHECK_FLCN_STATUS(_dpuCsbErrorChkRegWr32NonPosted_GV10X(LW_CGSP_BAR0_CSR,
            DRF_DEF(_CGSP, _BAR0_CSR, _CMD ,        _WRITE) |
            DRF_NUM(_CGSP, _BAR0_CSR, _BYTE_ENABLE, 0xF   ) |
            DRF_DEF(_CGSP, _BAR0_CSR, _TRIG,        _TRUE )));
    CHECK_FLCN_STATUS(_dpuCsbErrorChkRegRd32_GV10X(LW_CGSP_BAR0_CSR, NULL));

    // WFI and check WFI return status
    flcnStatus = _dpuBar0WaitIdle_GV10X(LW_TRUE);
    if (flcnStatus != FLCN_OK)
    {
        flcnStatus = FLCN_ERR_BAR0_PRIV_WRITE_ERROR;
    }

ErrorExit:

    // Initiate a BAR0_CSR CSB read access to clear errors
    if (flcnStatus != FLCN_OK)
    {
        _dpuBar0ClearCsrErrors_GV10X();
    }
    return flcnStatus;
}

/*!
 * Clears pending errors on BAR0_CSR by initiating a CSR read
 *
 * Once we have an error of LW_CGSP_BAR0_CSR_STATUS_TMOUT or
 * case LW_CGSP_BAR0_CSR_STATUS_DIS (or others),  the status will
 * remain until we have a successful CSR read.  Unless the error
 * status is cleared,  all subsequent calls to  dpuBar0WaitIdle
 * will fail.
 *
 * See https://sc-refmanuals.lwpu.com/~gpu_refmanuals/gp104/LW_CGSP.ref#125
 * STATUS :3 (R)
 * Determines the Status of the previous Command. When status is ERR or TOUT, 
 * the register is reset to IDLE on read access.
 */
static inline void
_dpuBar0ClearCsrErrors_GV10X(void)
{
    // Initiate a BAR0_CSR CSB read access to clear errors
    REG_WR32(CSB, LW_CGSP_BAR0_ADDR, LW_PMC_ENABLE);

    REG_WR32_STALL(CSB, LW_CGSP_BAR0_CSR,
        DRF_DEF(_CGSP, _BAR0_CSR, _CMD ,        _READ) |
        DRF_NUM(_CGSP, _BAR0_CSR, _BYTE_ENABLE, 0xF  ) |
        DRF_DEF(_CGSP, _BAR0_CSR, _TRIG,        _TRUE));

    (void)REG_RD32(CSB, LW_CGSP_BAR0_CSR);

    // If we are not doing PEH,  we don't need to check the return value.
    (void)_dpuBar0WaitIdle_GV10X(LW_FALSE);
    (void)REG_RD32(CSB, LW_CGSP_BAR0_DATA);
}

#endif // GSP_PRIERR_H
