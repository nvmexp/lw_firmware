/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acrucsega10X.c
 */

#ifndef DISABLE_SE_ACCESSES
//
// Includes
//
#include "acr.h"
#include "acr_static_inline_functions.h"
#include "dev_falcon_v4.h"
#include "dev_fbfalcon_pri.h"
#include "dev_se_seb.h"
#include "dev_sec_csb.h"
#include "dev_pwr_csb.h"

#ifdef ACR_BUILD_ONLY
#include "config/g_se_private.h"
#else
#include "g_se_private.h"
#endif

#include "acr_objacr.h"
#include "acr_objse.h"

static ACR_STATUS _seColwertPKAStatusToAcrStatus_GA10X(LwU32 value) ATTR_OVLY(OVL_NAME);
static ACR_STATUS _sePkaReadPkaOperandFromBankAddress_GA10X(SE_PKA_BANK bankId, LwU32 index, LwU32 *pData, LwU32 pkaKeySizeDW) ATTR_OVLY(OVL_NAME);
static ACR_STATUS _sePkaWritePkaOperandToBankAddress_GA10X(SE_PKA_BANK bankId, LwU32 index, LwU32 *pData, LwU32 pkaKeySizeDW) ATTR_OVLY(OVL_NAME);
static ACR_STATUS _sePkaSetRadix_GA10X(SE_KEY_SIZE numBits, PSE_PKA_REG pSePkaReg) ATTR_OVLY(OVL_NAME);

/*!
 * @brief Determines the radix mask to be used in  LW_SSE_SE_PKA_CONTROL register
 *
 * @param[in] numBits    Radix or the size of the operation (key length)
 * @param[in] pSePkaReg  Struct containing the number of words based on the radix
 *
 * @return status ACR_ERROR_ILWALID_ARGUMENT if pSePkaReg is a NULL pointer.
 *                ACR_ERROR_SE_ILWALID_SE_KEY_CONFIG if an invalid key passed.
 *                ACR_OK if no error, and  stuct is initilized that contains
 *                sizes and values needed to address operand memory.
 */
static ACR_STATUS
_sePkaSetRadix_GA10X
(
    SE_KEY_SIZE numBits,
    PSE_PKA_REG pSePkaReg
)
{
    ACR_STATUS status  = ACR_OK;
    LwU32      minBits = SE_KEY_SIZE_256;

    // Check input data
    if (pSePkaReg == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    while (minBits < numBits)
    {
        minBits <<= 1;
    }
    pSePkaReg->pkaKeySizeDW = minBits >> 5;

    //
    // From manuals - lw_sse_se.ref
    // The BASE_RADIX bits are used to set the size of the base operand size.
    // The CTRL_PARTIAL_RADIX bits are used to select a partial field from within the base radix.The bits
    // operate as a kind of mask, allowing a sub - portion of the full radix to be performed.They operate by
    // enabling only that number of words of the operand to be processed.For example, for an instance with a
    // 32 - bit memory word, if the CTRL_BASE_RADIX bits are set to 2 to indicate a 256 - bit operand size, and the
    // CTRL_PARTIAL_RADIX bits are set to 5, then 5 words of the 8 word operand will be processed.
    //
    // TODO:  521 bit keys (some special casing)
    //
    switch (numBits)
    {
        case SE_KEY_SIZE_160:
        case SE_KEY_SIZE_192:
        case SE_KEY_SIZE_224:
            pSePkaReg->pkaRadixMask = DRF_DEF(_SSE, _SE_PKA_CONTROL, _CTRL_BASE_RADIX, _256) |
                                      DRF_VAL(_SSE, _SE_PKA_CONTROL, _CTRL_PARTIAL_RADIX, numBits >> 5);
            break;
        case SE_KEY_SIZE_256:
            pSePkaReg->pkaRadixMask = DRF_DEF(_SSE, _SE_PKA_CONTROL, _CTRL_BASE_RADIX, _256) |
                                      DRF_VAL(_SSE, _SE_PKA_CONTROL, _CTRL_PARTIAL_RADIX, 0);
            break;
        case SE_KEY_SIZE_384:
            pSePkaReg->pkaRadixMask = DRF_DEF(_SSE, _SE_PKA_CONTROL, _CTRL_BASE_RADIX, _512) |
                                      DRF_VAL(_SSE, _SE_PKA_CONTROL, _CTRL_PARTIAL_RADIX, numBits >> 5);
            break;
        case SE_KEY_SIZE_512:
            pSePkaReg->pkaRadixMask = DRF_DEF(_SSE, _SE_PKA_CONTROL, _CTRL_BASE_RADIX, _512) |
                                      DRF_VAL(_SSE, _SE_PKA_CONTROL, _CTRL_PARTIAL_RADIX, 0);
            break;
        case SE_KEY_SIZE_768:
            pSePkaReg->pkaRadixMask = DRF_DEF(_SSE, _SE_PKA_CONTROL, _CTRL_BASE_RADIX, _1024) |
                                      DRF_VAL(_SSE, _SE_PKA_CONTROL, _CTRL_PARTIAL_RADIX, numBits >> 5);
            break;
        case SE_KEY_SIZE_1024:
            pSePkaReg->pkaRadixMask = DRF_DEF(_SSE, _SE_PKA_CONTROL, _CTRL_BASE_RADIX, _1024) |
                                      DRF_VAL(_SSE, _SE_PKA_CONTROL, _CTRL_PARTIAL_RADIX, 0);
            break;
        case SE_KEY_SIZE_1536:
            pSePkaReg->pkaRadixMask = DRF_DEF(_SSE, _SE_PKA_CONTROL, _CTRL_BASE_RADIX, _2048) |
                                      DRF_VAL(_SSE, _SE_PKA_CONTROL, _CTRL_PARTIAL_RADIX, numBits >> 5);
            break;
        case SE_KEY_SIZE_2048:
            pSePkaReg->pkaRadixMask = DRF_DEF(_SSE, _SE_PKA_CONTROL, _CTRL_BASE_RADIX, _2048) |
                                      DRF_VAL(_SSE, _SE_PKA_CONTROL, _CTRL_PARTIAL_RADIX, 0);
            break;
        case SE_KEY_SIZE_3072:
            pSePkaReg->pkaRadixMask = DRF_DEF(_SSE, _SE_PKA_CONTROL, _CTRL_BASE_RADIX, _4096) |
                                      DRF_VAL(_SSE, _SE_PKA_CONTROL, _CTRL_PARTIAL_RADIX, numBits >> 5);
            break;
        case SE_KEY_SIZE_4096:
            pSePkaReg->pkaRadixMask = DRF_DEF(_SSE, _SE_PKA_CONTROL, _CTRL_BASE_RADIX, _4096) |
                                      DRF_VAL(_SSE, _SE_PKA_CONTROL, _CTRL_PARTIAL_RADIX, 0);

            break;
        default:
            status = ACR_ERROR_SE_ILWALID_KEY_CONFIG;
            break;
    }
    return status;
}

/*!
 * @brief Gets the bank address and writes data to the PKA operand memory (A, B, C or D registers)
 *
 * @param[in]  bankId        Which bank address to return?
 * @param[in]  index         Index of the register
 * @param[in] *pData         Pointer for the data to be written
 * @param[in]  pkaKeySizeDW  Number of words to write
 *
 * @return ACR_OK if successful otherwise ACR_ERROR_XXX code return.
 */
static ACR_STATUS
_sePkaWritePkaOperandToBankAddress_GA10X
(
    SE_PKA_BANK   bankId,
    LwU32         index,
    LwU32        *pData,
    LwU32         pkaKeySizeDW
)
{
    ACR_STATUS status;
    LwU32      bankAddress = 0, i;

    if (pData == NULL)
    {
        return  ACR_ERROR_ILWALID_ARGUMENT;
    }

#if defined(AHESASC) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
    ACR_SELWREBUS_TARGET busTarget = SEC2_SELWREBUS_TARGET_SE;
#else
    // Control is not expected to come here. If you hit this assert, please check build profile config
    ct_assert(0);
#endif


    CHECK_STATUS_OK_OR_GOTO_CLEANUP(seGetPkaOperandBankAddress_HAL(pSe, bankId, &bankAddress, index, pkaKeySizeDW));

    for (i = 0; i < pkaKeySizeDW; i++)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrSelwreBusWriteRegister_HAL(pAcr,
                                                                      busTarget,
                                                                      bankAddress + i * 4,
                                                                      pData[i]));
    }

Cleanup:
    return status;
}

/*!
 * @brief Gets the bank address and reads data from PKA operand memory (A, B, C or D registers)
 *
 * @param[in] bankId        Which bank address to return?
 * @param[in] index         Index of the register
 * @param[in] *pData        Pointer to return read data
 * @param[in] pkaKeySizeDW  Number of words to write
 *
 * @return ACR_OK if successful otherwise ACR_ERROR_XXX code return.
 */
static ACR_STATUS
_sePkaReadPkaOperandFromBankAddress_GA10X
(
    SE_PKA_BANK   bankId,
    LwU32         index,
    LwU32        *pData,
    LwU32         pkaKeySizeDW
)
{
    ACR_STATUS status;
    LwU32      bankAddress = 0, i;

#if defined(AHESASC) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
    ACR_SELWREBUS_TARGET busTarget = SEC2_SELWREBUS_TARGET_SE;
#else
    // Control is not expected to come here. If you hit this assert, please check build profile config
    ct_assert(0);
#endif

    if (pData == NULL)
    {
        return  ACR_ERROR_ILWALID_ARGUMENT;
    }

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(seGetPkaOperandBankAddress_HAL(pSe, bankId, &bankAddress, index, pkaKeySizeDW));

    for (i = 0; i < pkaKeySizeDW; i++)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrSelwreBusReadRegister_HAL(pAcr,
                                                                     busTarget,
                                                                     bankAddress + i * 4,
                                                                     (LwU32 *)&pData[i]));
    }

Cleanup:
    return status;
}

/*!
 * @brief To colwert SE interrupt status to ACR_STATUS
 *
 * @param[in] value  The input interrupt status
 *
 * @return ACR_OK if no error detected; otherwise ACR_ERROR_PKA_XXX code return.
 */
static ACR_STATUS
_seColwertPKAStatusToAcrStatus_GA10X
(
    LwU32 value
)
{
    ACR_STATUS status;


    switch (value)
    {
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_NORMAL:
            status = ACR_OK;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_ILWALID_OPCODE:
            status = ACR_ERROR_SE_PKA_OP_ILWALID_OPCODE;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_F_STACK_UNDERFLOW:
            status = ACR_ERROR_SE_PKA_OP_F_STACK_UNDERFLOW;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_F_STACK_OVERFLOW:
            status = ACR_ERROR_SE_PKA_OP_F_STACK_OVERFLOW;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_WATCHDOG:
            status = ACR_ERROR_SE_PKA_OP_WATCHDOG;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_HOST_REQUEST:
            status = ACR_ERROR_SE_PKA_OP_HOST_REQUEST;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_P_STACK_UNDERFLOW:
            status = ACR_ERROR_SE_PKA_OP_P_STACK_UNDERFLOW;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_P_STACK_OVERFLOW:
            status = ACR_ERROR_SE_PKA_OP_P_STACK_OVERFLOW;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_MEM_PORT_COLLISION:
            status = ACR_ERROR_SE_PKA_OP_MEM_PORT_COLLISION;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_OPERATION_SIZE_EXCEED_CFG:
            status = ACR_ERROR_SE_PKA_OP_OPERATION_SIZE_EXCEED_CFG;
            break;
        default:
            status = ACR_ERROR_SE_PKA_OP_UNKNOWN_ERR;
            break;
    }

    return status;
}

/*!
 * @brief To acquire SE engine mutex
 *
 * @return ACR_OK if no error detected; otherwise ACR_ERROR_PKA_XXX code return.
 */
ACR_STATUS
seAcquireMutex_GA10X
(
    void
)
{
    LwU32 val;
    ACR_STATUS status;

    // Acquire SE global mutex
#if defined(AHESASC) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
    ACR_SELWREBUS_TARGET busTarget = SEC2_SELWREBUS_TARGET_SE;
#else
    // Control is not expected to come here. If you hit this assert, please check build profile config
    ct_assert(0);
#endif


    // Acquire SE global mutex
    status = acrSelwreBusReadRegister_HAL(pAcr, busTarget, LW_SSE_SE_CTRL_PKA_MUTEX, &val);
    if (val != LW_SSE_SE_CTRL_PKA_MUTEX_RC_SUCCESS || status != ACR_OK)
    {
        status = ACR_ERROR_SELWRE_BUS_REQUEST_FAILED;
    }

    return status;
}

/*!
 * @brief To release SE engine mutex
 *
 * @return ACR_OK if no error detected; otherwise ACR_ERROR_PKA_XXX code return.
 */
ACR_STATUS
seReleaseMutex_GA10X
(
    void
)
{
    ACR_STATUS status;
#if defined(AHESASC) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
    ACR_SELWREBUS_TARGET busTarget = SEC2_SELWREBUS_TARGET_SE;
#else
    // Control is not expected to come here. If you hit this assert, please check build profile config
    ct_assert(0);
#endif


    // Release SE global mutex
    status = acrSelwreBusWriteRegister_HAL(pAcr, busTarget,
                                           LW_SSE_SE_CTRL_PKA_MUTEX_RELEASE,
                                           LW_SSE_SE_CTRL_PKA_MUTEX_RELEASE_SE_LOCK_RELEASE);
    if (status != ACR_OK)
    {
        status = ACR_ERROR_SELWRE_BUS_REQUEST_FAILED;
    }

    return status;
}

/*!
 * @brief  Function to set RSA parameters
 *
 * @param[in]  pSePkaReg    Struct containing the number of words based on the radix.
 * @param[in]  paramsType   The RSA parameter type.
 * @param[out] dwBuffer     The buffer to save input data.
 *
 * @return ACR_OK if successful otherwise ACR_ERROR_XXX code return.
 */
ACR_STATUS
seRsaSetParams_GA10X
(
    SE_PKA_REG        *pSePkaReg,
    SE_RSA_PARAMS_TYPE paramsType,
    LwU32             *pDwBuffer
)
{
    ACR_STATUS  status;

    if (pDwBuffer == NULL || pSePkaReg == NULL)
    {
        return  ACR_ERROR_ILWALID_ARGUMENT;
    }

    if (paramsType == SE_RSA_PARAMS_TYPE_MODULUS ||
        paramsType == SE_RSA_PARAMS_TYPE_MODULUS_MONTGOMERY_EN)
    {
        // Store prime modulus in D(0)
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(_sePkaWritePkaOperandToBankAddress_GA10X(SE_PKA_BANK_D, 0, pDwBuffer, pSePkaReg->pkaKeySizeDW));


        if (paramsType == SE_RSA_PARAMS_TYPE_MODULUS_MONTGOMERY_EN)
        {
            // Calulate r ilwerse. r_ilw is output to C(0) as a result of HW operation
            CHECK_STATUS_OK_OR_GOTO_CLEANUP(seStartPkaOperationAndPoll_HAL(pSe,
                                                                           LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_CALC_R_ILW,
                                                                           pSePkaReg->pkaRadixMask,
                                                                           SE_ENGINE_OPERATION_TIMEOUT_DEFAULT));

            // Callwlate modulus'. modulus' is output to D(1) as a result of HW operation
            CHECK_STATUS_OK_OR_GOTO_CLEANUP(seStartPkaOperationAndPoll_HAL(pSe,
                                                                           LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_CALC_MP,
                                                                           pSePkaReg->pkaRadixMask,
                                                                           SE_ENGINE_OPERATION_TIMEOUT_DEFAULT));

            // Callwlate (r^2 mod m). Result is output to D(3) as a result of HW operation.
            CHECK_STATUS_OK_OR_GOTO_CLEANUP(seStartPkaOperationAndPoll_HAL(pSe,
                                                                           LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_CALC_R_SQR,
                                                                           pSePkaReg->pkaRadixMask,
                                                                           SE_ENGINE_OPERATION_TIMEOUT_DEFAULT));
        }
    }
    else if (paramsType == SE_RSA_PARAMS_TYPE_MONTGOMERY_MP)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(_sePkaWritePkaOperandToBankAddress_GA10X(SE_PKA_BANK_D, 1, pDwBuffer, pSePkaReg->pkaKeySizeDW));
    }
    else if (paramsType == SE_RSA_PARAMS_TYPE_MONTGOMERY_R_SQR)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(_sePkaWritePkaOperandToBankAddress_GA10X(SE_PKA_BANK_D, 3, pDwBuffer, pSePkaReg->pkaKeySizeDW));
    }
    else if (paramsType == SE_RSA_PARAMS_TYPE_EXPONENT)
    {
        // Store exponent in D(2)
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(_sePkaWritePkaOperandToBankAddress_GA10X(SE_PKA_BANK_D, 2, pDwBuffer, pSePkaReg->pkaKeySizeDW));
    }
    else if (paramsType == SE_RSA_PARAMS_TYPE_BASE)
    {
        // Store base in A(0)
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(_sePkaWritePkaOperandToBankAddress_GA10X(SE_PKA_BANK_A, 0, pDwBuffer, pSePkaReg->pkaKeySizeDW));
    }
    else
    {
        status = ACR_ERROR_ILWALID_ARGUMENT;
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(status);
    }

Cleanup:

    return status;
}

/*!
 * @brief    Function to get RSA parameters
 *
 * @param[in]  pSePkaReg    Struct containing the number of words based on the radix.
 * @param[in]  paramsType   The RSA parameter type.
 * @param[out] dwBuffer     The buffer to save return data.
 *
 * @return  ACR_OK if successful otherwise ACR_ERROR_XXX code return.
 */
ACR_STATUS
seRsaGetParams_GA10X
(
    SE_PKA_REG *pSePkaReg,
    SE_RSA_PARAMS_TYPE paramsType,
    LwU32  dwBuffer[]
)
{
    ACR_STATUS  status;

    if (dwBuffer == NULL || pSePkaReg == NULL)
    {
        return  ACR_ERROR_ILWALID_ARGUMENT;
    }

    if (paramsType == SE_RSA_PARAMS_TYPE_MODULUS)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(_sePkaReadPkaOperandFromBankAddress_GA10X(SE_PKA_BANK_D, 0, dwBuffer, pSePkaReg->pkaKeySizeDW));
    }
    else if (paramsType == SE_RSA_PARAMS_TYPE_MONTGOMERY_MP)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(_sePkaReadPkaOperandFromBankAddress_GA10X(SE_PKA_BANK_D, 1, dwBuffer, pSePkaReg->pkaKeySizeDW));
    }
    else if (paramsType == SE_RSA_PARAMS_TYPE_MONTGOMERY_R_SQR)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(_sePkaReadPkaOperandFromBankAddress_GA10X(SE_PKA_BANK_D, 3, dwBuffer, pSePkaReg->pkaKeySizeDW));
    }
    else if (paramsType == SE_RSA_PARAMS_TYPE_EXPONENT)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(_sePkaReadPkaOperandFromBankAddress_GA10X(SE_PKA_BANK_D, 2, dwBuffer, pSePkaReg->pkaKeySizeDW));
    }
    else if (paramsType == SE_RSA_PARAMS_TYPE_BASE ||
             paramsType == SE_RSA_PARAMS_TYPE_RSA_RESULT)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(_sePkaReadPkaOperandFromBankAddress_GA10X(SE_PKA_BANK_A, 0, dwBuffer, pSePkaReg->pkaKeySizeDW));
    }
   else
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(ACR_ERROR_ILWALID_ARGUMENT);
    }

Cleanup:

    return status;
}

/*!
 * @brief    Triggers the PKA operation
 *
 * @param[in] pkaOp       The PKA operation to be done
 * @param[in] radixMask   The mask which is dependent on the size of the operation
 *
 * @return  ACR_OK if successful otherwise ACR_ERROR_XXX code return.
 */
ACR_STATUS
seStartPkaOperationAndPoll_GA10X
(
    LwU32  pkaOp,
    LwU32  radixMask,
    LwU32  timeOut
)
{
    ACR_STATUS status;
    LwU32      value;
    LwU32      lwrrTime;
    LwU32      startTime;

#if defined(AHESASC) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
    ACR_SELWREBUS_TARGET busTarget = SEC2_SELWREBUS_TARGET_SE;
#else
    // Control is not expected to come here. If you hit this assert, please check build profile config
    ct_assert(0);
#endif

    // clear L index register to run FF1 for EC Point Multiplication:
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrSelwreBusWriteRegister_HAL(pAcr,
                                                                  busTarget,
                                                                  LW_SSE_SE_PKA_INDEX_L,
                                                                  DRF_DEF(_SSE, _SE_PKA_INDEX_L, _INDEX_L, _CLEAR)));
    // Clear residual flags
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrSelwreBusWriteRegister_HAL(pAcr,
                                                                  busTarget,
                                                                  LW_SSE_SE_PKA_FLAGS,
                                                                  DRF_DEF(_SSE, _SE_PKA_FLAGS, _FLAG_ZERO,   _UNSET) |
                                                                  DRF_DEF(_SSE, _SE_PKA_FLAGS, _FLAG_MEMBIT, _UNSET) |
                                                                  DRF_DEF(_SSE, _SE_PKA_FLAGS, _FLAG_BORROW, _UNSET) |
                                                                  DRF_DEF(_SSE, _SE_PKA_FLAGS, _FLAG_CARRY,  _UNSET) |
                                                                  DRF_DEF(_SSE, _SE_PKA_FLAGS, _FLAG_F0,     _UNSET) |
                                                                  DRF_DEF(_SSE, _SE_PKA_FLAGS, _FLAG_F1,     _UNSET) |
                                                                  DRF_DEF(_SSE, _SE_PKA_FLAGS, _FLAG_F2,     _UNSET) |
                                                                  DRF_DEF(_SSE, _SE_PKA_FLAGS, _FLAG_F3,     _UNSET)));
    // clear F-Stack:
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrSelwreBusWriteRegister_HAL(pAcr,
                                                                  busTarget,
                                                                  LW_SSE_SE_PKA_FUNCTION_STACK_POINT,
                                                                  DRF_DEF(_SSE, _SE_PKA_FUNCTION_STACK_POINT, _FSTACK_POINTER, _CLEAR)));

    // Write the program entry point
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrSelwreBusWriteRegister_HAL(pAcr,
                                                                     busTarget,
                                                                     LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS,
                                                                     DRF_NUM(_SSE, _SE_PKA_PROGRAM_ENTRY_ADDRESS, _PROG_ADDR, pkaOp)));

    // enable interrupts
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrSelwreBusWriteRegister_HAL(pAcr,
                                                                  busTarget,
                                                                  LW_SSE_SE_PKA_INTERRUPT_ENABLE,
                                                                  DRF_DEF(_SSE, _SE_PKA_INTERRUPT_ENABLE, _IE_IRQ_EN, _SET)));
    // Clear IRQ status
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrSelwreBusWriteRegister_HAL(pAcr,
                                                                  busTarget,
                                                                  LW_SSE_SE_PKA_STATUS,
                                                                  DRF_DEF(_SSE, _SE_PKA_STATUS, _STAT_IRQ, _SET)));

    //
    // Start the PKA with assigned mode:
    //
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrSelwreBusWriteRegister_HAL(pAcr,
                                                                  busTarget,
                                                                  LW_SSE_SE_PKA_CONTROL,
                                                                  DRF_DEF(_SSE, _SE_PKA_CONTROL, _CTRL_GO, _ENABLE) |
                                                                  radixMask));

#if defined(AHESASC) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
    startTime = ACR_REG_RD32(CSB, LW_CSEC_FALCON_PTIMER0);
#else
    // Control is not expected to come here. If you hit this assert, please check build profile config
    ct_assert(0);
#endif
    //
    // Poll the status register until PKA operation is complete. This is
    // indicated by STAT_IRQ bit in PKA_STATUS.  Once set, this bit will
    // remain set until cleared by writing to it.
    //
    do
    {
#if defined(AHESASC) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
        lwrrTime = ACR_REG_RD32(CSB, LW_CSEC_FALCON_PTIMER0);
#else
        // Control is not expected to come here. If you hit this assert, please check build profile config
        ct_assert(0);
#endif

        if ((lwrrTime - startTime) > timeOut)
        {
            return ACR_ERROR_SE_PKA_OP_OPERATION_TIMEOUT;
        }

        CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrSelwreBusReadRegister_HAL(pAcr, busTarget, LW_SSE_SE_PKA_STATUS, &value));
    }
    while (FLD_TEST_DRF(_SSE, _SE_PKA_STATUS, _STAT_IRQ, _UNSET, value));

    // Clear the PKA op completed bit  (see above)
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrSelwreBusWriteRegister_HAL(pAcr,
                                                                  busTarget,
                                                                  LW_SSE_SE_PKA_STATUS,
                                                                  DRF_DEF(_SSE, _SE_PKA_STATUS, _STAT_IRQ, _SET)));


    // Colwert PKA status return code to SE_STATUS and return
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrSelwreBusReadRegister_HAL(pAcr, busTarget, LW_SSE_SE_PKA_RETURN_CODE, &value));
    value = DRF_VAL(_SSE, _SE_PKA_RETURN_CODE, _RC_STOP_REASON, value);
    status = _seColwertPKAStatusToAcrStatus_GA10X(value);

Cleanup:
    return status;
}

/*!
 * @brief Determines the starting address of the register in the
 *        PKA operand memory based on the size of the operation and
 *        the bank ID.
 *
 * @param[in]  bankId        Which bank address to return?
 * @param[out] pBankAddr     Bank address to return
 * @param[in]  idx           Index of the register
 * @param[in]  pkaKeySizeDW  The number of words based on the radix
 *
 * @return ACR_OK if successful otherwise ACR_ERROR_XXX code return.
 */
ACR_STATUS
seGetPkaOperandBankAddress_GA10X
(
    SE_PKA_BANK bankId,
    LwU32 *pBankAddr,
    LwU32 idx,
    LwU32 pkaKeySizeDW
)
{
    ACR_STATUS status;
    LwU32     bankOffset;
    LwU64     tmp64;

    if (pBankAddr == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(seGetSpecificPkaOperandBankOffset_HAL(pSe, bankId, &bankOffset));
    tmp64 = acrMultipleUnsigned32(idx <<2, pkaKeySizeDW);
    tmp64 = tmp64 + (LwU64)bankOffset;
    *pBankAddr = (LwU32)tmp64;

Cleanup:
    return status;
}

/*!
 * @brief Returns specific bank offset based on input
 *
 * @param[in]  bankId       Designated bank Id
 * @param[out] pBankOffset  The pointer to save return bank offset
 *
 * @return     ACR_OK if successful otherwise ACR_ERROR_XXX code return.
 */
ACR_STATUS
seGetSpecificPkaOperandBankOffset_GA10X
(
    SE_PKA_BANK  bankId,
    LwU32 *pBankOffset
)
{
    LwU32 baseAddress;
    ACR_STATUS status = ACR_OK;

    if (pBankOffset == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    baseAddress = seGetPkaOperandBankBaseAddress_HAL(pSe);

    switch (bankId)
    {
        case SE_PKA_BANK_A:
            *pBankOffset = baseAddress + LW_SSE_SE_PKA_BANK_START_A_OFFSET;
            break;
        case SE_PKA_BANK_B:
            *pBankOffset = baseAddress + LW_SSE_SE_PKA_BANK_START_B_OFFSET;
            break;
        case SE_PKA_BANK_C:
            *pBankOffset = baseAddress + LW_SSE_SE_PKA_BANK_START_C_OFFSET;
            break;
        case SE_PKA_BANK_D:
            *pBankOffset = baseAddress + LW_SSE_SE_PKA_BANK_START_D_OFFSET;
            break;
        default:
            status = ACR_ERROR_ILWALID_ARGUMENT;
            break;
    }

    return status;
}

/*!
 * @brief Function to get base Pka operand bank base address
 *
 * @return LwU32 The bank base address to return.
 */
LwU32 seGetPkaOperandBankBaseAddress_GA10X(void)
{
    return LW_SSE_SE_PKA_ADDR_OFFSET;
}

/*!
 * @brief Returns specific bank offset based on input
 *
 * @param[in]  pSePkaReg    Struct containing the number of words based on the radix
 * @param[in]  pModulus     The pointer of modulaus buffer address
 * @param[in]  pExp         The pointer of exponent buffer address
 * @param[in]  pMp          The pointer of pre-computed montgomery MP buffer address (option)
 * @param[in]  pRSqr        The pointer of pre-computed montgomery R_SQR buffer address(option)
 *
 * @return     ACR_OK if successful otherwise ACR_ERROR_XXX code return.
 */
ACR_STATUS
seSetRsaOperationKeys_GA10X
(
    SE_PKA_REG *pSePkaReg,
    LwU32      *pModulus,
    LwU32      *pExp,
    LwU32      *pMp,
    LwU32      *pRSqr
)
{
    ACR_STATUS status;

    if (pSePkaReg == NULL ||
        pModulus  == NULL ||
        pExp      == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    if ((pMp == NULL && pRSqr != NULL) ||
        (pMp != NULL && pRSqr == NULL))
    {
        return ACR_ERROR_SE_ILWALID_KEY_CONFIG;
    }
    else if (pMp != NULL && pRSqr != NULL)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(seRsaSetParams_GA10X(pSePkaReg, SE_RSA_PARAMS_TYPE_MODULUS, pModulus));
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(seRsaSetParams_GA10X(pSePkaReg, SE_RSA_PARAMS_TYPE_MONTGOMERY_MP, pMp));
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(seRsaSetParams_GA10X(pSePkaReg, SE_RSA_PARAMS_TYPE_EXPONENT, pExp));
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(seRsaSetParams_GA10X(pSePkaReg, SE_RSA_PARAMS_TYPE_MONTGOMERY_R_SQR, pRSqr));
    }
    else if (pMp == NULL && pRSqr == NULL)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(seRsaSetParams_GA10X(pSePkaReg, SE_RSA_PARAMS_TYPE_MODULUS_MONTGOMERY_EN, pModulus));
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(seRsaSetParams_GA10X(pSePkaReg, SE_RSA_PARAMS_TYPE_EXPONENT, pExp));
    }
    else
    {
        status = ACR_ERROR_SE_ILWALID_KEY_CONFIG;
    }

Cleanup:
    return status;

}

/*!
 * @brief Function to initialize SE for PKA operations
 *
 * @param[in]     keySize    Radix or the size of the operation.
 * @param[in/out] pSePkaReg  Struct containing the number of words based on the radix.
 *
 * @return        ACR_OK if successful otherwise ACR_ERROR_XXX code return.
 */
ACR_STATUS
seContextIni_GA10X
(
    SE_PKA_REG *pSePkaReg,
    SE_KEY_SIZE keySize
)
{
    ACR_STATUS status;

#if defined(AHESASC) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
    ACR_SELWREBUS_TARGET busTarget = SEC2_SELWREBUS_TARGET_SE;
#else
    // Control is not expected to come here. If you hit this assert, please check build profile config
    ct_assert(0);
#endif

    if (pSePkaReg == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(_sePkaSetRadix_GA10X(keySize, pSePkaReg));

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrSelwreBusWriteRegister_HAL(pAcr,
                                                                  busTarget,
                                                                  LW_SSE_SE_CTRL_CONTROL,
                                                                  DRF_DEF(_SSE, _SE_CTRL_CONTROL, _AUTORESEED,    _ENABLE)  |
                                                                  DRF_DEF(_SSE, _SE_CTRL_CONTROL, _LOAD_KEY,      _DISABLE) |
                                                                  DRF_DEF(_SSE, _SE_CTRL_CONTROL, _SCRUB_PKA_MEM, _DISABLE) |
                                                                  DRF_NUM(_SSE, _SE_CTRL_CONTROL, _KEYSLOT,       0)));

    //
    // From IAS:
    // On emulation, TRNG couldn't work properly, so SW must set SCC Control Register.DPA_DISABLE = 1 to make PKA work properly.
    // https://p4viewer/get/p4hw:2001//hw/ar/doc/t18x/se/SE_PKA_IAS.doc
    //

    // disable SCC
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrSelwreBusWriteRegister_HAL(pAcr,
                                                                  busTarget,
                                                                  LW_SSE_SE_CTRL_SCC_CONTROL,
                                                                  DRF_DEF(_SSE, _SE_CTRL_SCC_CONTROL_DPA, _DISABLE, _SET)));

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrSelwreBusWriteRegister_HAL(pAcr,
                                                                  busTarget,
                                                                  LW_SSE_SE_PKA_JUMP_PROBABILITY,
                                                                  DRF_DEF(_SSE, _SE_PKA_JUMP_PROBABILITY, _JUMP_PROBABILITY, _CLEAR)));
Cleanup:
    return status;
}
#endif
