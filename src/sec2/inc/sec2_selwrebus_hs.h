/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SEC2_SELWREBUS_HS_H
#define SEC2_SELWREBUS_HS_H


/*!
 * @file sec2_selwrebus_hs.h 
 * This file holds the inline definitions of functions used for secure bus access
 */

/* ------------------------- System includes -------------------------------- */
#include "dev_sec_csb.h"
#include "dev_fbhub_sec_bus.h"

/* ------------------------- Application includes --------------------------- */
/* ------------------------- Types definitions ------------------------------ */
/* ------------------------- External definitions --------------------------- */
/* ------------------------- Static variables ------------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
static inline FLCN_STATUS _sec2HubSelwreBusSendRequest_GP10X(LwBool bRead, LwU32 addr, LwU32 val)
    GCC_ATTRIB_ALWAYSINLINE();
static inline FLCN_STATUS _sec2HubSelwreBusGetData_GP10X(LwU32 *pVal)
    GCC_ATTRIB_ALWAYSINLINE();
static inline FLCN_STATUS _sec2HubSelwreBusWriteRegister(LwU32 addr, LwU32 val)
    GCC_ATTRIB_ALWAYSINLINE();
static inline FLCN_STATUS _sec2HubSelwreBusReadRegister(LwU32 addr, LwU32 *pVal)
    GCC_ATTRIB_ALWAYSINLINE();
static inline FLCN_STATUS _sec2HubIsAcrEncryptionEnabled_GP10X(LwU32 region, LwBool *pBEncryptionEnable)
    GCC_ATTRIB_ALWAYSINLINE();

/*!
 * TODO (nkuo) - This is just a temporary implementation before secure bus
 *               access is ported to common library
 *
 * @brief Sends a read or write request to the Private Bus of FBHUB
 *
 * @param[in] bRead  Whether it is a read or a write request
 * @param[in] addr   Address of the request
 * @param[in] val    If it's a write request, the value is written
 *
 * @return FLCN_OK  if the request is sent successfully
 */
static inline FLCN_STATUS
_sec2HubSelwreBusSendRequest_GP10X
(
    LwBool bRead,
    LwU32  addr,
    LwU32  val
)
{
    LwU32 count;
    LwU32 data = DRF_DEF(_CSEC, _FALCON_DIO_DOC_CTRL, _EMPTY, _INIT) |
                 DRF_DEF(_CSEC, _FALCON_DIO_DOC_CTRL, _WR_FINISHED, _INIT) |
                 DRF_DEF(_CSEC, _FALCON_DIO_DOC_CTRL, _RD_FINISHED, _INIT);
    do
    {
        count = REG_RD32(CSB, LW_CSEC_FALCON_DIO_DOC_CTRL(0)) & data;
    }
    while (count != data); 

    if (!bRead) 
    {
        REG_WR32(CSB, LW_CSEC_FALCON_DIO_DOC_D1(0), val);
    }

    REG_WR32(CSB, LW_CSEC_FALCON_DIO_DOC_D0(0), (addr | (bRead ? 0x10000 : 0)));

    return FLCN_OK;
}

/*!
 * TODO (nkuo) - This is just a temporary implementation before secure bus
 *               access is ported to common library
 *
 * @brief Once a read request happens through sec2HubSelwreBusSendRequest,
 *        this function reads the data back
 * 
 * @param[out]  pVal   Pointer storing the data gotten from secure bus
 *
 * @return FLCN_OK  if the data is gotten successfully
 *         FLCN_ERROR otherwise
 */
static inline FLCN_STATUS
_sec2HubSelwreBusGetData_GP10X
(
    LwU32 *pVal
) 
{
    LwU32 error;
    LwU32 data;
    LwU32 val = 0;

    do
    {
        data = REG_RD32(CSB, LW_CSEC_FALCON_DIO_DIC_CTRL(0));
    }
    while (FLD_TEST_DRF_NUM(_CSEC, _FALCON_DIO_DIC_CTRL, _COUNT, 0x0, data));

    val = FLD_SET_DRF_NUM(_CSEC, _FALCON_DIO_DIC_CTRL, _POP, 0x1, val);
    REG_WR32(CSB, LW_CSEC_FALCON_DIO_DIC_CTRL(0), val);

    error = REG_RD32(CSB, LW_CSEC_FALCON_DIO_DOC_CTRL(0));

    // Check if there is a read error or a protocol error
    if (FLD_TEST_DRF_NUM(_CSEC, _FALCON_DIO_DOC_CTRL, _RD_ERROR, 0x1, error) ||
        FLD_TEST_DRF_NUM(_CSEC, _FALCON_DIO_DOC_CTRL, _PROTOCOL_ERROR, 0x1, error)) 
    {
        return FLCN_ERROR;
    }

    *pVal = REG_RD32(CSB, LW_CSEC_FALCON_DIO_DIC_D0(0));
    return FLCN_OK;
}

/*!
 * TODO (nkuo) - This is just a temporary implementation before secure bus
 *               access is ported to common library
 *
 * @brief Write an SFBHUB register using the secure bus
 *  
 * @param[in]  addr  Address of the register
 * @param[in]  val   Value to be written
 *
 * @return the status returned from _sec2HubSelwreBusSendRequest_GP10X
 */
static inline FLCN_STATUS
_sec2HubSelwreBusWriteRegister
(
    LwU32 addr,
    LwU32 val
)
{
    return _sec2HubSelwreBusSendRequest_GP10X(LW_FALSE, addr, val);
}

/*!
 * TODO (nkuo) - This is just a temporary implementation before secure bus
 *               access is ported to common library
 *
 * @brief Read an SFBHUB register using the secure bus
 *  
 * @param[in]   addr   Address of the register
 * @param[out]  pVal   Pointer storing the data read from the register
 *
 * @return FLCN_OK  if the data is read successfully
 */
static inline FLCN_STATUS
_sec2HubSelwreBusReadRegister
(
    LwU32  addr,
    LwU32 *pVal
)
{
    FLCN_STATUS status;
    
    status = _sec2HubSelwreBusSendRequest_GP10X(LW_TRUE, addr, 0);
    if (status != FLCN_OK)
    {
        return status;
    }
    return _sec2HubSelwreBusGetData_GP10X(pVal);
}

/*!
 * TODO (nkuo) - This is just a temporary implementation before secure bus
 *               access is ported to common library
 *
 * @brief check if HUB encryption is enabled on the specific region
 *  
 * @param[in]   region                Region to be checked
 * @param[out]  pBEncryptionEnabled   Pointer saving the encryption state
 *
 * @return FLCN_OK  if the encryption is enalbed on the region
 *         FLCN_ERR_ILWALID_ARGUMENT  if the input pointer is NULL
 *         Other error code can be returned from callee, and will be bubbled up directly.
 */
static inline FLCN_STATUS
_sec2HubIsAcrEncryptionEnabled_GP10X
(
    LwU32   region,
    LwBool *pBEncryptionEnabled
)
{
    FLCN_STATUS status;
    LwU32       data32;

    if (pBEncryptionEnabled == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    *pBEncryptionEnabled = LW_FALSE;
    status = _sec2HubSelwreBusReadRegister(LW_SFBHUB_ENCRYPTION_ACR_CFG, &data32);

    if (status != FLCN_OK)
    {
        return status;
    }
    else if (!FLD_IDX_TEST_DRF(_SFBHUB, _ENCRYPTION_ACR_CFG, _REG, region,
                               _ENCRYPT_EN, data32))
    {
        return FLCN_OK;
    }

    status = _sec2HubSelwreBusReadRegister(LW_SFBHUB_ENCRYPTION_ACR_VIDMEM_READ_VALID, &data32);

    if (status != FLCN_OK)
    {
        return status;
    }
    else if (!FLD_IDX_TEST_DRF(_SFBHUB, _ENCRYPTION_ACR_VIDMEM, _READ_VALID_REG,
                               region, _TRUE, data32))
    {
        return FLCN_OK;
    }

    status = _sec2HubSelwreBusReadRegister(LW_SFBHUB_ENCRYPTION_ACR_VIDMEM_WRITE_VALID, &data32);

    if (status != FLCN_OK)
    {
        return status;
    }
    else if (!FLD_IDX_TEST_DRF(_SFBHUB, _ENCRYPTION_ACR_VIDMEM, _WRITE_VALID_REG,
                               region, _TRUE, data32))
    {
        return FLCN_OK;
    }

    *pBEncryptionEnabled = LW_TRUE;
    return FLCN_OK;
}

#endif // SEC2_SELWREBUS_HS_H

