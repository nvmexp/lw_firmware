/*
 * Copyright 2018-19 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   soe_prierrlr10.c
 * @brief  SOE Read/write BAR0/CSB priv read/write error checking for LR10
 *
 * SOE HAL functions implementing error checking on priv reads and writes using BAR0/CSB
 * Details in wiki below
 * https://wiki.lwpu.com/gpuhwmaxwell/index.php/Maxwell_PRIV#Error_Reporting
 */

/* ------------------------- System Includes -------------------------------- */
#include "soe_csb.h"
#include "dev_soe_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "soesw.h"
#include "main.h"
#include "soe_bar0.h"
#include "soe_objsoe.h"
#include "soe_prierr.h"
#include "flcntypes.h"

#include "config/g_soe_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/* ------------------------- Static Function Prototypes  -------------------- */

/*!
 * Reads the given CSB address. The read transaction is nonposted.
 * 
 * Checks the return read value for priv errors and returns a status.
 * It is up the the calling apps to decide how to handle a failing status.
 * 
 * This is the LS version of the function.
 *
 * @param[in]  addr   The CSB address to read
 * @param[out] *data  The 32-bit value of the requested address
 *
 * @return The status of the read operation.
 */
FLCN_STATUS
soeCsbErrChkRegRd32_LR10
(
    LwU32  addr,
    LwU32  *pData
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    lwrtosENTER_CRITICAL();
    {
        flcnStatus = _soeCsbErrorChkRegRd32_LR10(addr, pData);
    }
    lwrtosEXIT_CRITICAL();

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
 * version.
 *
 * This is the LS version of the function.
 *
 * @param[in]  addr  The CSB address to write
 * @param[in]  data  The data to write to the address
 *
 * @return The status of the write operation.
 */
FLCN_STATUS
soeCsbErrChkRegWr32NonPosted_LR10
(
    LwU32  addr,
    LwU32  data
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    lwrtosENTER_CRITICAL();
    {
        flcnStatus = _soeCsbErrorChkRegWr32NonPosted_LR10(addr, data);
    }
    lwrtosEXIT_CRITICAL();

    return flcnStatus;
}

/*!
 * Reads the given BAR0 address. The read transaction is nonposted.
 * 
 * Checks the return read value for priv errors and returns a status.
 * It is up the the calling apps to decide how to handle a failing status.
 *
 * This is the LS version of the function.
 *
 * @param[in]  addr  The BAR0 address to read
 * @param[out] *data  The 32-bit value of the requested BAR0 address
 *
 * @return The status of the read operation.
 */
FLCN_STATUS
soeBar0ErrChkRegRd32_LR10
(
    LwU32  addr,
    LwU32  *pData
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    lwrtosENTER_CRITICAL();
    {
        flcnStatus = _soeBar0ErrorChkRegRd32_LR10(addr, pData);
    }
    lwrtosEXIT_CRITICAL();

    return flcnStatus;
}

/*!
 * Writes a 32-bit value to the given BAR0 address.  This is the nonposted form
 * (will wait for transaction to complete) of bar0RegWr32Posted.  It is safe to
 * call it repeatedly and in any combination with other BAR0MASTER functions.
 *
 * This is the LS version of the function.
 *
 * @param[in]  addr  The BAR0 address to write
 * @param[in]  data  The data to write to the address
 *
 * @return The status of the write operation.
 */
FLCN_STATUS
soeBar0ErrChkRegWr32NonPosted_LR10
(
    LwU32  addr,
    LwU32  data
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    lwrtosENTER_CRITICAL();
    {
        flcnStatus = _soeBar0ErrorChkRegWr32NonPosted_LR10(addr, data);
    }
    lwrtosEXIT_CRITICAL();

    return flcnStatus;
}
