/*
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   secbus_hs.c
 * @brief  Non halified Heavy Secure secure bus functions
 *
 */

#include "se_objse.h"
#include "secbus_hs.h"
#include "csb.h"
#include "common_hslib.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static void _seSelwreBusHsEntry(void)
    GCC_ATTRIB_SECTION("imem_libSEHs", "start")
    GCC_ATTRIB_USED();
/* ------------------------- Implementation --------------------------------- */

/*!
 * @brief Entry function of SE Secure Bus library. This function merely returns.
 *        It is used to validate the overlay blocks corresponding to the secure
 *        overlay before jumping to arbitrary functions inside it. The linker
 *        script should make sure that it puts this section at the top of the
 *        overlay to ensure that we can jump to the overlay's 0th offset before
 *        exelwting code at arbitrary offsets inside the overlay.
 */
static void
_seSelwreBusHsEntry(void)
{
    // Validate that caller is HS, otherwise halt
    VALIDATE_THAT_CALLER_IS_HS();
    return;
}

/*!
 * @brief Write an SE register using the secure bus in HS mode
 *  
 * @param[in] addr  Address
 * @param[in] val   If its a write request, the value is written 
 *
 * @return SE_OK if request is successful
 */
SE_STATUS 
seSelwreBusWriteRegisterErrChkHs
(
    LwU32 addr,
    LwU32 val
)
{
    return seSelwreBusSendRequestHs_HAL(&Se, LW_FALSE, addr, val);
}

/*!
 * @brief Read an SE register using the secure bus in HS mode
 *  
 * @param[in]  addr  Address
 * @param[out] pData Data out for read request
 *
 * @return SE_OK if request is successful
 */
SE_STATUS
seSelwreBusReadRegisterErrChkHs
(
    LwU32 addr,
    LwU32 *pData
)
{
    SE_STATUS seStatus = SE_OK;

    if (pData == NULL)
    {
        return SE_ERR_ILWALID_ARGUMENT;
    }
    seStatus = seSelwreBusSendRequestHs_HAL(&Se, LW_TRUE, addr, 0);
    if (seStatus != SE_OK)
    {
        return seStatus;
    }

    return seSelwreBusGetDataHs_HAL(&Se, addr, pData);
}

