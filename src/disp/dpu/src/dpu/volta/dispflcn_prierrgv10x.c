/*
 * Copyright 2017-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dispflcn_prierrgp10x.c
 * @brief  SEC2 Read/write BAR0/CSB priv read/write error checking for GV10X
 *
 * SEC2 HAL functions implementing error checking on priv reads and writes using BAR0/CSB
 * Details in wiki below
 * https://wiki.lwpu.com/gpuhwmaxwell/index.php/Maxwell_PRIV#Error_Reporting
 */

/* ------------------------- System Includes -------------------------------- */
#include "gsp_csb.h"
#include "dev_gsp_csb.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_bus.h"
#include "dev_timer.h"

/* ------------------------- Application Includes --------------------------- */
#include "dpusw.h"
#include "dpu_objdpu.h"
#include "gsp_bar0_hs.h"
#include "gsp_prierr.h"
#include "flcntypes.h"

#include "config/g_dpu_private.h"

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
 * @param[out] pData  The 32-bit value of the requested address
 *
 * @return The status of the read operation.
 */
FLCN_STATUS
dpuCsbErrChkRegRd32_GV10X
(
    LwUPtr  addr,
    LwU32  *pData
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    lwrtosENTER_CRITICAL();
    {
        flcnStatus = _dpuCsbErrorChkRegRd32_GV10X(addr, pData);
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
dpuCsbErrChkRegWr32NonPosted_GV10X
(
    LwUPtr addr,
    LwU32  data
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    lwrtosENTER_CRITICAL();
    {
        flcnStatus = _dpuCsbErrorChkRegWr32NonPosted_GV10X(addr, data);
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
 * @param[in]  addr   The BAR0 address to read
 * @param[out] pData  The 32-bit value of the requested BAR0 address
 *
 * @return The status of the read operation.
 */
FLCN_STATUS
dpuBar0ErrChkRegRd32_GV10X
(
    LwU32  addr,
    LwU32 *pData
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    lwrtosENTER_CRITICAL();
    {
        flcnStatus = _dpuBar0ErrorChkRegRd32_GV10X(addr, pData);
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
dpuBar0ErrChkRegWr32NonPosted_GV10X
(
    LwU32  addr,
    LwU32  data
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    lwrtosENTER_CRITICAL();
    {
        flcnStatus = _dpuBar0ErrorChkRegWr32NonPosted_GV10X(addr, data);
    }
    lwrtosEXIT_CRITICAL();

    return flcnStatus;
}

/*!
 * Reads the given CSB address. The read transaction is nonposted.
 * 
 * Checks the return read value for priv errors and returns a status.
 * It is up the the calling apps to decide how to handle a failing status.
 *
 * This is the HS version of the function.
 *
 * @param[in]  addr   The CSB address to read
 * @param[out] pData  The 32-bit value of the requested address
 *
 * @return The status of the read operation.
 */
FLCN_STATUS
dpuCsbErrChkRegRd32Hs_GV10X
(
    LwUPtr  addr,
    LwU32  *pData
)
{
    return _dpuCsbErrorChkRegRd32_GV10X(addr, pData);
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
 * This is the HS version of the function.
 *
 * @param[in]  addr  The CSB address to write
 * @param[in]  data  The data to write to the address
 *
 * @return The status of the write operation.
 */
FLCN_STATUS
dpuCsbErrChkRegWr32NonPostedHs_GV10X
(
    LwUPtr addr,
    LwU32  data
)
{
    return _dpuCsbErrorChkRegWr32NonPosted_GV10X(addr, data);
}

/*!
 * Reads the given BAR0 address. The read transaction is nonposted.
 *
 * Checks the return read value for priv errors and returns a status.
 * It is up the the calling apps to decide how to handle a failing status.
 *
 * This is the HS version of the function.
 *
 * @param[in]   addr  The BAR0 address to read
 * @param[out]  pData The 32-bit value of the requested BAR0 address 
 *
 * @return The status of the read operation.
 */
FLCN_STATUS
dpuBar0ErrChkRegRd32Hs_GV10X
(
    LwU32  addr,
    LwU32 *pData
)
{
    return _dpuBar0ErrorChkRegRd32_GV10X(addr, pData);
}

/*!
 * Writes a 32-bit value to the given BAR0 address.  This is the nonposted form
 * (will wait for transaction to complete) of bar0RegWr32Posted.  It is safe to
 * call it repeatedly and in any combination with other BAR0MASTER functions.
 *
 * This is the HS version of the function.
 *
 * @param[in]  addr  The BAR0 address to write
 * @param[in]  data  The data to write to the address
 *
 * @return The status of the write operation.
 */
FLCN_STATUS
dpuBar0ErrChkRegWr32NonPostedHs_GV10X
(
    LwU32  addr,
    LwU32  data
)
{
    return  _dpuBar0ErrorChkRegWr32NonPosted_GV10X(addr, data);
}
