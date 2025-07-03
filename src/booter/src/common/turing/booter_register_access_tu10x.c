/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: booter_register_access_tu10x.c
 */
//
// Includes
//
#include "booter.h"

#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)

//
// TODO: Get this define added in GA100 manuals.
// Current value of this timeout is taken from http://lwbugs/2198976/285
//
#ifndef LW_CSEC_BAR0_TMOUT_CYCLES__PROD
#define LW_CSEC_BAR0_TMOUT_CYCLES__PROD    (0x01312d00)
#endif

/*!
 *  @brief: Set timeout value for BAR0 transactions
 */
void booterSetBar0Timeout_TU10X(void)
{
    BOOTER_REG_WR32(CSB, LW_CSEC_BAR0_TMOUT,
        DRF_DEF(_CSEC, _BAR0_TMOUT, _CYCLES, __PROD));
}

/*!
 * @brief Wait for BAR0 to become idle
 */
void
booterBar0WaitIdle_TU10X(void)
{
    LwBool bDone = LW_FALSE;

    // As this is an infinite loop, timeout should be considered on using bewlo loop elsewhere.
    do
    {
        switch (DRF_VAL(_CSEC, _BAR0_CSR, _STATUS,
                    BOOTER_REG_RD32(CSB, LW_CSEC_BAR0_CSR)))
        {
            case LW_CSEC_BAR0_CSR_STATUS_IDLE:
                bDone = LW_TRUE;
                break;

            case LW_CSEC_BAR0_CSR_STATUS_BUSY:
                break;

            // unexpected error conditions
            case LW_CSEC_BAR0_CSR_STATUS_ERR:
            case LW_CSEC_BAR0_CSR_STATUS_TMOUT:
            case LW_CSEC_BAR0_CSR_STATUS_DIS:
            default:
                FAIL_BOOTER_WITH_ERROR(BOOTER_ERROR_FLCN_REG_ACCESS);
        }
    }
    while (!bDone);

}

LwU32
booterBar0RegRead_TU10X
(
    LwU32 addr
)
{
    LwU32  val;

    booterBar0WaitIdle_HAL(pBooter);
    BOOTER_REG_WR32(CSB, LW_CSEC_BAR0_ADDR, addr);

    BOOTER_REG_WR32(CSB, LW_CSEC_BAR0_CSR,
        DRF_DEF(_CSEC, _BAR0_CSR, _CMD ,        _READ) |
        DRF_NUM(_CSEC, _BAR0_CSR, _BYTE_ENABLE, 0xF  ) |
        DRF_DEF(_CSEC, _BAR0_CSR, _TRIG,        _TRUE));

    (void)BOOTER_REG_RD32(CSB, LW_CSEC_BAR0_CSR);

    booterBar0WaitIdle_HAL(pBooter);
    val = BOOTER_REG_RD32(CSB, LW_CSEC_BAR0_DATA);
    return val;
}

void
booterBar0RegWrite_TU10X
(
    LwU32 addr,
    LwU32 data
)
{
    booterBar0WaitIdle_HAL(pBooter);
    BOOTER_REG_WR32(CSB, LW_CSEC_BAR0_ADDR, addr);
    BOOTER_REG_WR32(CSB, LW_CSEC_BAR0_DATA, data);

    BOOTER_REG_WR32(CSB, LW_CSEC_BAR0_CSR,
        DRF_DEF(_CSEC, _BAR0_CSR, _CMD ,        _WRITE) |
        DRF_NUM(_CSEC, _BAR0_CSR, _BYTE_ENABLE, 0xF   ) |
        DRF_DEF(_CSEC, _BAR0_CSR, _TRIG,        _TRUE ));

    (void)BOOTER_REG_RD32(CSB, LW_CSEC_BAR0_CSR);
    booterBar0WaitIdle_HAL(pBooter);

}

/*!
 * Wrapper function for blocking falcon read instruction to halt in case of Priv Error.
 */
LwU32
falc_ldxb_i_with_halt
(
    LwU32 addr
)
{
    LwU32   val           = 0;
    LwU32   csbErrStatVal = 0;

    val = falc_ldxb_i ((LwU32*)(addr), 0);

    csbErrStatVal = falc_ldxb_i ((LwU32*)(LW_CSEC_FALCON_CSBERRSTAT), 0);

    if (FLD_TEST_DRF(_PFALCON, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatVal))
    {
        // If CSBERRSTAT is true, halt no matter what register was read.
        FAIL_BOOTER_WITH_ERROR(BOOTER_ERROR_FLCN_REG_ACCESS);
    }
    else if ((val & CSB_INTERFACE_MASK_VALUE) == CSB_INTERFACE_BAD_VALUE)
    {

        if (booterIgnoreShaResultRegForBadValueCheck_HAL(pBooter, addr))
        {
            return val;
        }

        //
        // If we get here, we thought we read a bad value. However, there are
        // certain cases where the values may lead us to think we read something
        // bad, but in fact, they were just valid values that the register could
        // report. For example, the host ptimer PTIMER1 and PTIMER0 registers
        // can return 0xbadfxxxx value that we interpret to be "bad".
        //

        if (addr == LW_CSEC_FALCON_PTIMER1)
        {
            //
            // PTIMER1 can report a constant 0xbadfxxxx for a long time, so
            // just make an exception for it.
            //
            return val;
        }
        else if (addr == LW_CSEC_FALCON_PTIMER0)
        {
            //
            // PTIMER0 can report a 0xbadfxxxx value for up to 2^16 ns, but it
            // should keep changing its value every nanosecond. We just check
            // to make sure we can detect its value changing by reading it a
            // few times.
            //
            LwU32   val1      = 0;
            LwU32   i         = 0;

            for (i = 0; i < 3; i++)
            {
                val1 = falc_ldx_i ((LwU32 *)(addr), 0);
                if (val1 != val)
                {
                    // Timer0 is progressing, so return latest value, i.e. val1.
                    return val1;
                }
            }
        }

        //
        // While reading SE_TRNG, the random number generator can return a badfxxxx value
        // which is actually a random number provided by TRNG unit. Since the output of this
        // SE_TRNG registers comes in FALCON_DIC_D0, we need to skip PEH for this FALCON_DIC_D0
        // This in turn reduces our PEH coverage for all SE registers since all SE registers are read from
        // FALCON_DIC_D0 register.
        // Skipping 0xBADFxxxx check here on FALCON_DIC_D0 is fine, this check is being done in the
        // upper level parent function booterSelwreBusGetData_HAL and explicitly discounting only the
        // SE TRNG registers from 0xBADFxxxx check.
        // More details in Bug 200326572
        // Also, the ifdef on LW_CPWR_FALCON_DIC_D0 and LW_CSEC_FALCON_DIC_D0 is required since
        // these registers are not defined for GM20X (this file is also built for GM20X).
        // We should make this function a HAL and to fork separate and appropriate implementation for GM20X/GP10X/GV100+
        //
        if (addr == LW_CSEC_FALCON_DIC_D0)
        {
            return val;
        }

        FAIL_BOOTER_WITH_ERROR(BOOTER_ERROR_FLCN_REG_ACCESS);
    }

    return val;
}

/*!
 * Wrapper function for blocking falcon write instruction to halt in case of Priv Error.
 */
void
falc_stxb_i_with_halt
(
    LwU32 addr,
    LwU32 data
)
{
    LwU32 csbErrStatVal = 0;

    falc_stxb_i ((LwU32*)(addr), 0, data);

    csbErrStatVal = falc_ldxb_i ((LwU32*)(LW_CSEC_FALCON_CSBERRSTAT), 0);

    if (FLD_TEST_DRF(_PFALCON, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatVal))
    {
        FAIL_BOOTER_WITH_ERROR(BOOTER_ERROR_FLCN_REG_ACCESS);
    }
}

#elif defined(BOOTER_RELOAD)

// Needed for GA100
#ifndef LW_CLWDEC_BAR0_TMOUT_CYCLES__PROD
#define LW_CLWDEC_BAR0_TMOUT_CYCLES__PROD  (0x01312d00)
#endif

/*!
 *  @brief: Set timeout value for BAR0 transactions
 */
void booterSetBar0Timeout_TU10X(void)
{
    BOOTER_REG_WR32(CSB, LW_CLWDEC_BAR0_TMOUT,
        DRF_DEF(_CLWDEC, _BAR0_TMOUT, _CYCLES, __PROD));
}

/*!
 * @brief Wait for BAR0 to become idle
 */
void
booterBar0WaitIdle_TU10X(void)
{
    LwBool bDone = LW_FALSE;

    // As this is an infinite loop, timeout should be considered on using bewlo loop elsewhere.
    do
    {
        switch (DRF_VAL(_CLWDEC, _BAR0_CSR, _STATUS,
                    BOOTER_REG_RD32(CSB, LW_CLWDEC_BAR0_CSR)))
        {
            case LW_CLWDEC_BAR0_CSR_STATUS_IDLE:
                bDone = LW_TRUE;
                break;

            case LW_CLWDEC_BAR0_CSR_STATUS_BUSY:
                break;

            // unexpected error conditions
            case LW_CLWDEC_BAR0_CSR_STATUS_ERR:
            case LW_CLWDEC_BAR0_CSR_STATUS_TMOUT:
            case LW_CLWDEC_BAR0_CSR_STATUS_DIS:
            default:
                FAIL_BOOTER_WITH_ERROR(BOOTER_ERROR_FLCN_REG_ACCESS);
        }
    }
    while (!bDone);

}

LwU32
booterBar0RegRead_TU10X
(
    LwU32 addr
)
{
    LwU32  val;

    booterBar0WaitIdle_HAL(pBooter);
    BOOTER_REG_WR32(CSB, LW_CLWDEC_BAR0_ADDR, addr);

    BOOTER_REG_WR32(CSB, LW_CLWDEC_BAR0_CSR,
        DRF_DEF(_CLWDEC, _BAR0_CSR, _CMD ,        _READ) |
        DRF_NUM(_CLWDEC, _BAR0_CSR, _BYTE_ENABLE, 0xF  ) |
        DRF_DEF(_CLWDEC, _BAR0_CSR, _TRIG,        _TRUE));

    (void)BOOTER_REG_RD32(CSB, LW_CLWDEC_BAR0_CSR);

    booterBar0WaitIdle_HAL(pBooter);
    val = BOOTER_REG_RD32(CSB, LW_CLWDEC_BAR0_DATA);
    return val;
}

void
booterBar0RegWrite_TU10X
(
    LwU32 addr,
    LwU32 data
)
{
    booterBar0WaitIdle_HAL(pBooter);
    BOOTER_REG_WR32(CSB, LW_CLWDEC_BAR0_ADDR, addr);
    BOOTER_REG_WR32(CSB, LW_CLWDEC_BAR0_DATA, data);

    BOOTER_REG_WR32(CSB, LW_CLWDEC_BAR0_CSR,
        DRF_DEF(_CLWDEC, _BAR0_CSR, _CMD ,        _WRITE) |
        DRF_NUM(_CLWDEC, _BAR0_CSR, _BYTE_ENABLE, 0xF   ) |
        DRF_DEF(_CLWDEC, _BAR0_CSR, _TRIG,        _TRUE ));

    (void)BOOTER_REG_RD32(CSB, LW_CLWDEC_BAR0_CSR);
    booterBar0WaitIdle_HAL(pBooter);

}

/*!
 * Wrapper function for blocking falcon read instruction to halt in case of Priv Error.
 */
LwU32
falc_ldxb_i_with_halt
(
    LwU32 addr
)
{
    LwU32   val           = 0;
    LwU32   csbErrStatVal = 0;

    val = falc_ldxb_i ((LwU32*)(addr), 0);

    csbErrStatVal = falc_ldxb_i ((LwU32*)(LW_CLWDEC_FALCON_CSBERRSTAT), 0);

    if (FLD_TEST_DRF(_PFALCON, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatVal))
    {
        // If CSBERRSTAT is true, halt no matter what register was read.
        FAIL_BOOTER_WITH_ERROR(BOOTER_ERROR_FLCN_REG_ACCESS);
    }
    else if ((val & CSB_INTERFACE_MASK_VALUE) == CSB_INTERFACE_BAD_VALUE)
    {

        if (booterIgnoreShaResultRegForBadValueCheck_HAL(pBooter, addr))
        {
            return val;
        }

        //
        // If we get here, we thought we read a bad value. However, there are
        // certain cases where the values may lead us to think we read something
        // bad, but in fact, they were just valid values that the register could
        // report. For example, the host ptimer PTIMER1 and PTIMER0 registers
        // can return 0xbadfxxxx value that we interpret to be "bad".
        //

        if (addr == LW_CLWDEC_FALCON_PTIMER1)
        {
            //
            // PTIMER1 can report a constant 0xbadfxxxx for a long time, so
            // just make an exception for it.
            //
            return val;
        }
        else if (addr == LW_CLWDEC_FALCON_PTIMER0)
        {
            //
            // PTIMER0 can report a 0xbadfxxxx value for up to 2^16 ns, but it
            // should keep changing its value every nanosecond. We just check
            // to make sure we can detect its value changing by reading it a
            // few times.
            //
            LwU32   val1      = 0;
            LwU32   i         = 0;

            for (i = 0; i < 3; i++)
            {
                val1 = falc_ldx_i ((LwU32 *)(addr), 0);
                if (val1 != val)
                {
                    // Timer0 is progressing, so return latest value, i.e. val1.
                    return val1;
                }
            }
        }

        //
        // While reading SE_TRNG, the random number generator can return a badfxxxx value
        // which is actually a random number provided by TRNG unit. Since the output of this
        // SE_TRNG registers comes in FALCON_DIC_D0, we need to skip PEH for this FALCON_DIC_D0
        // This in turn reduces our PEH coverage for all SE registers since all SE registers are read from
        // FALCON_DIC_D0 register.
        // Skipping 0xBADFxxxx check here on FALCON_DIC_D0 is fine, this check is being done in the
        // upper level parent function booterSelwreBusGetData_HAL and explicitly discounting only the
        // SE TRNG registers from 0xBADFxxxx check.
        // More details in Bug 200326572
        // Also, the ifdef on LW_CPWR_FALCON_DIC_D0 and LW_CLWDEC_FALCON_DIC_D0 is required since
        // these registers are not defined for GM20X (this file is also built for GM20X).
        // We should make this function a HAL and to fork separate and appropriate implementation for GM20X/GP10X/GV100+
        //
        if (addr == LW_CLWDEC_FALCON_DIC_D0)
        {
            return val;
        }

        FAIL_BOOTER_WITH_ERROR(BOOTER_ERROR_FLCN_REG_ACCESS);
    }

    return val;
}

/*!
 * Wrapper function for blocking falcon write instruction to halt in case of Priv Error.
 */
void
falc_stxb_i_with_halt
(
    LwU32 addr,
    LwU32 data
)
{
    LwU32 csbErrStatVal = 0;

    falc_stxb_i ((LwU32*)(addr), 0, data);

    csbErrStatVal = falc_ldxb_i ((LwU32*)(LW_CLWDEC_FALCON_CSBERRSTAT), 0);

    if (FLD_TEST_DRF(_PFALCON, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatVal))
    {
        FAIL_BOOTER_WITH_ERROR(BOOTER_ERROR_FLCN_REG_ACCESS);
    }
}
#endif

