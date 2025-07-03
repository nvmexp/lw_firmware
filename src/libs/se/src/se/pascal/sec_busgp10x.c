/*
 * Copyright 2014-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec_busgp10x.c
 * @brief  Secure Bus HAL functions for GP10X
 *
 * 
 */

#include "se_objse.h"
#include "secsb.h"
#include "se_private.h"
#include "config/g_se_hal.h"
#include "dev_se_seb.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
inline static SE_STATUS _seSelwreBusSendRequest_GP10X(LwBool bRead, LwU32 addr,LwU32 val)
    GCC_ATTRIB_ALWAYSINLINE();
inline static SE_STATUS _seSelwreBusGetData_GP10X(LwU32, LwU32 *)
    GCC_ATTRIB_ALWAYSINLINE();


/* ------------------------- Implementation --------------------------------- */

//
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO
// Secure Bus needs to move to separate library
// TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//

/*!
 * @brief Sends a read or write request to the Private Bus of SE
 *
 * @param[in] bRead Whether it is a read or a write request
 * @param[in] addr  Address
 * @param[in] val   If its a write request, the value is written
 * 
 * @return SE_OK if successful. Error code if unsuccessful.
 */
inline static SE_STATUS
_seSelwreBusSendRequest_GP10X
(
    LwBool bRead,
    LwU32 addr,
    LwU32 val
)
{
    SE_STATUS status      = SE_OK;
    LwU32     error       = 0;
    LwU32     doneInitVal = 0;
    LwU32     docCtrl     = 0;

    doneInitVal = CSB_DRF_DEF(_FALCON_DOC_CTRL, _EMPTY, _INIT) |
                  CSB_DRF_DEF(_FALCON_DOC_CTRL, _RD_FINISHED, _INIT) |
                  CSB_DRF_DEF(_FALCON_DOC_CTRL, _WR_FINISHED, _INIT);
    do
    {
        CHECK_SE_STATUS(CSB_REG_RD32_ERRCHK((LwUPtr)CSB_REG(_DOC_CTRL), &docCtrl));
        docCtrl = docCtrl & doneInitVal;
    }
    while (docCtrl != doneInitVal);

    // Clear any previous errors
    CHECK_SE_STATUS(CSB_REG_WR32_ERRCHK((LwUPtr)CSB_REG(_DOC_CTRL), CSB_DRF_DEF(_FALCON_DOC_CTRL, _WR_ERROR, _INIT) |
                                                                    CSB_DRF_DEF(_FALCON_DOC_CTRL, _RD_ERROR, _INIT) |
                                                                    CSB_DRF_DEF(_FALCON_DOC_CTRL, _PROTOCOL_ERROR, _INIT)));

    // Perform the read/write operation
    if (!bRead)
    {
        CHECK_SE_STATUS(CSB_REG_WR32_ERRCHK((LwUPtr)CSB_REG(_DOC_D1), val));
    }
    CHECK_SE_STATUS(CSB_REG_WR32_ERRCHK((LwUPtr)CSB_REG(_DOC_D0), (addr | (bRead ? 0x10000 : 0))));

    if(!bRead)
    {
        // Wait for write to be finished
        do
        {
            CHECK_SE_STATUS(CSB_REG_RD32_ERRCHK((LwUPtr)CSB_REG(_DOC_CTRL), &docCtrl));
            docCtrl = docCtrl & doneInitVal;
        }
        while (docCtrl != doneInitVal);

        CHECK_SE_STATUS(CSB_REG_RD32_ERRCHK((LwUPtr)CSB_REG(_DOC_CTRL), &error));
        if (CSB_FLD_TEST_DRF_NUM(_FALCON_DOC_CTRL, _WR_ERROR, 0X1, error) ||
            CSB_FLD_TEST_DRF_NUM(_FALCON_DOC_CTRL, _PROTOCOL_ERROR, 0X1, error))
        {
            return SE_ERR_SECBUS_WRITE_ERROR;
        }
    }

ErrorExit:
    return status;
}


/*!
 * @brief Once a read request happens through seSelwreBusSendRequest,
 *        this function reads the data back
 * @param[in] addr    address of a read request 
 * @param[in] pData   Data out for read request 
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 */
inline static SE_STATUS
_seSelwreBusGetData_GP10X
(
    LwU32 addr,
    LwU32 *pData
)
{
    SE_STATUS status = SE_OK;
    LwU32     count;
    LwU32     error = 0;
    LwU32     dicCtrl = 0;

    do 
    {
        CHECK_SE_STATUS(CSB_REG_RD32_ERRCHK((LwUPtr)CSB_REG(_DIC_CTRL), &dicCtrl));
        count = CSB_DRF_VAL(_FALCON_DIC_CTRL, _COUNT, dicCtrl);
    } 
    while (count == 0);
    
    CHECK_SE_STATUS(CSB_REG_WR32_ERRCHK((LwUPtr)CSB_REG(_DIC_CTRL), REF_NUM(CSB_FIELD(_DIC_CTRL_POP), 1)));
    CHECK_SE_STATUS(CSB_REG_RD32_ERRCHK((LwUPtr)CSB_REG(_DOC_CTRL), &error));
    if (CSB_FLD_TEST_DRF_NUM(_FALCON_DOC_CTRL, _RD_ERROR, 0X1, error) ||
        CSB_FLD_TEST_DRF_NUM(_FALCON_DOC_CTRL, _PROTOCOL_ERROR, 0X1, error))
    {
        status = SE_ERR_SECBUS_READ_ERROR;
        goto ErrorExit;
    }
    
    CHECK_SE_STATUS(CSB_REG_RD32_ERRCHK((LwUPtr)CSB_REG(_DIC_D0), pData));

ErrorExit:
    if (status == SE_ERR_CSB_PRIV_READ_0xBADF_VALUE_RECEIVED)
    {
#ifndef SELWRE_BUS_ONLY_SUPPORTED
        //
        // While reading SE_TRNG, the random number generator can return a badfxxxx value
        // which is actually a random number provided by TRNG unit. In case we are reading
        // TRNG registers, allow it to return 0xbadf values 
        // More details in Bug 200326572
        //
        LwU32     index  = 0;
        for (index = 0; index < LW_SSE_SE_TRNG_RAND__SIZE_1; index++)
        {
            if(addr == LW_SSE_SE_TRNG_RAND(index))
            {
                status = SE_OK;
            }
        }
#endif        
    }

    return status;
}

/*!
 * @brief Sends a read or write request to the Private Bus of SE
 *
 * @param[in] bRead Whether it is a read or a write request
 * @param[in] addr  Address
 * @param[in] val   If its a write request, the value is written
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 */
SE_STATUS seSelwreBusSendRequest_GP10X(LwBool bRead, LwU32 addr, LwU32 val)
{
    return _seSelwreBusSendRequest_GP10X(bRead, addr, val);
}

/*!
 * @brief Once a read request happens through seSelwreBusSendRequest,
 *        this function reads the data back
 *
 * @param[in] addr    address of a read request 
 * @param[in] pData   Data out for read request 
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 */
SE_STATUS seSelwreBusGetData_GP10X(LwU32 addr, LwU32 *pData) 
{
    return _seSelwreBusGetData_GP10X(addr, pData);
}

/*!
 * @brief Sends a read or write request to the Private Bus of SE
 *
 * @param[in] bRead Whether it is a read or a write request
 * @param[in] addr  Address
 * @param[in] val   If its a write request, the value is written
 * 
 * @return SE_OK if successful. Error code if unsuccessful.
 */
SE_STATUS  seSelwreBusSendRequestHs_GP10X(LwBool bRead, LwU32 addr, LwU32 val)
{
    return _seSelwreBusSendRequest_GP10X(bRead, addr, val); 
}

/*!
 * @brief Once a read request happens through seSelwreBusSendRequest,
 *        this function reads the data back
 *
 * @param[in] pData   Data out for read request 
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 */
SE_STATUS seSelwreBusGetDataHs_GP10X(LwU32 addr, LwU32 *pData) 
{
    return _seSelwreBusGetData_GP10X(addr, pData);
}
