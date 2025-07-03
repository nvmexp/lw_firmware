/*
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   secbus.c
 * @brief  Non halified secure bus functions
 *
 */

#include "se_objse.h"
#include "secbus_ls.h"
#include "flcnretval.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
#if GSP_RTOS
extern FLCN_STATUS hdcp22wiredSelwreRegAccessFromLs(LwBool bIsReadReq, LwU32 addr, LwU32 dataIn, LwU32 *pDataOut);
#endif

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#if GSP_RTOS
// TODO (Bug 200471270)- LS boot for GSP is not ready yet, so need to access secure bus via HS mode
#define hdcp22wiredReadRegister_hs(addr, pDataOut)   hdcp22wiredSelwreRegAccessFromLs(LW_TRUE, addr, 0, pDataOut)
#define hdcp22wiredWriteRegister_hs(addr, dataIn)    hdcp22wiredSelwreRegAccessFromLs(LW_FALSE, addr, dataIn, NULL)
#endif

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */

//
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO
// Secure Bus needs to move to separate library
// TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//

/*!
 * @brief Write an SE register using the secure bus
 *  
 * @param[in] addr  Address
 * @param[in] val   If its a write request, the value is written 
 *
 * @return SE_OK if request is successful
 */
SE_STATUS 
seSelwreBusWriteRegisterErrChk
(
    LwU32 addr,
    LwU32 val
)
{
#if GSP_RTOS
    // TODO (Bug 200471270) - LS boot for GSP is not ready yet, so need to access secure bus via HS mode
    if (hdcp22wiredWriteRegister_hs(addr, val) != FLCN_OK)
    {
        return SE_ERR_SECBUS_WRITE_ERROR;
    }
    else
    {
        return SE_OK;
    }
#else
    return seSelwreBusSendRequest_HAL(&Se, LW_FALSE, addr, val);
#endif
}

/*!
 * @brief Read an SE register using the secure bus
 *  
 * @param[in]  addr  Address
 * @param[out] pData Data out for read request
 *
 * @return SE_OK if request is successful
 */
SE_STATUS
seSelwreBusReadRegisterErrChk
(
    LwU32 addr,
    LwU32 *pData
)
{
#if GSP_RTOS
    // TODO (Bug 200471270) - LS boot for GSP is not ready yet, so need to access secure bus via HS mode
    if (hdcp22wiredReadRegister_hs(addr, pData) != FLCN_OK)
    {
        return SE_ERR_SECBUS_READ_ERROR;
    }
    else
    {
        return SE_OK;
    }
#else
    SE_STATUS seStatus = SE_OK;

    if (pData == NULL)
    {
        return SE_ERR_ILWALID_ARGUMENT;
    }
    seStatus = seSelwreBusSendRequest_HAL(&Se, LW_TRUE, addr, 0);
    if(seStatus != SE_OK)
    {
        return seStatus;
    }

    return seSelwreBusGetData_HAL(&Se, addr, pData);
#endif
}

