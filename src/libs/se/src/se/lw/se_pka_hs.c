/*
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   se_pka_hs.c
 * @brief  Non halified library functions for writing/reading operand memory.
 *
 */

#include "se_objse.h"
#include "secbus_ls.h"
#include "se_private.h"
#include "secbus_hs.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */

/*!
 * @brief Writes to the PKA operand memory (A, B, C or D registers) in HS Mode
 *
 * @param[in] regAddr         PKA Operand space register address
 * @param[in] *pData          Pointer for the data to be written
 * @param[in] pkaKeySizeDW    Number of dwords to write
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 */
SE_STATUS sePkaWriteRegisterHs
(
    LwU32 regAddr,
    LwU32 *pData,
    LwU32 pkaKeySizeDW
)
{
    SE_STATUS status = SE_OK;
    LwU32     i      = 0;

    for (i = 0; i < pkaKeySizeDW; i++)
    {
        CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChkHs(regAddr + i * 4, *(pData + i)));
    }

ErrorExit:
    return status;
}

/*!
 * @brief Copies the output to be returned to the client in HS Mode
 *
 * @param[in] output          Client space register address
 * @param[in] regAddr         PKA Operand space register address
 * @param[in] pkaKeySizeDW    Number of words to read and return to client
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 */
SE_STATUS sePkaReadRegisterHs
(
    LwU32 output[],
    LwU32 regAddr,
    LwU32 pkaKeySizeDW
)
{
    SE_STATUS status = SE_OK;
    LwU32     val    = 0;
    LwU32     i      = 0;

    for (i = 0; i < pkaKeySizeDW; i++)
    {
        CHECK_SE_STATUS(seSelwreBusReadRegisterErrChkHs((regAddr + i * 4), &val));
        output[i] = val;
    }

ErrorExit:
    return status;
}

/*!
 * @brief Gets the bank address and writes to the PKA operand memory (A, B, C or D registers) in HS Mode
 *
 * @param[in] bankId        Which bank address to return?
 * @param[in] index         Index of the register
 * @param[in] *pData        Pointer for the data to be written
 * @param[in] pkaKeySizeDW  Number of words to write
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 */
SE_STATUS sePkaWritePkaOperandToBankAddressHs
(
    LwU32 bankId,
    LwU32 index,
    LwU32 *pData,
    LwU32 pkaKeySizeDW
)
{
    SE_STATUS status      = SE_OK;
    LwU32     bankAddress = 0;

    CHECK_SE_STATUS(seGetPkaOperandBankAddressHs_HAL(&Se, bankId, &bankAddress, index, pkaKeySizeDW));
    CHECK_SE_STATUS(sePkaWriteRegisterHs(bankAddress, pData, pkaKeySizeDW));

ErrorExit:
    return status;
}

/*!
 * @brief Gets the bank address and reads from PKA operand memory (A, B, C or D registers) in HS Mode
 *
 * @param[in] bankId        Which bank address to return?
 * @param[in] index         Index of the register
 * @param[in] *pOutput      Pointer to return read data
 * @param[in] pkaKeySizeDW  Number of words to write
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 */
SE_STATUS sePkaReadPkaOperandFromBankAddressHs
(
    SE_PKA_BANK bankId,
    LwU32       index,
    LwU32       *pOutput,
    LwU32       pkaKeySizeDW
)
{
    SE_STATUS status      = SE_OK;
    LwU32     bankAddress = 0;

    CHECK_SE_STATUS(seGetPkaOperandBankAddressHs_HAL(&Se, bankId, &bankAddress, index, pkaKeySizeDW));
    CHECK_SE_STATUS(sePkaReadRegisterHs(pOutput, bankAddress, pkaKeySizeDW));

ErrorExit:
    return status;
}

