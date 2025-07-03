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
 * @file: booter_se_tu10x.c
 */

#ifndef DISABLE_SE_ACCESSES
//
// Includes
//
#include "booter.h"
#include "dev_se_seb.h"

#include "config/g_se_private.h"
#include "booter_objse.h"

static BOOTER_STATUS _seColwertPKAStatusToBooterStatus_TU10X(LwU32 value) ATTR_OVLY(OVL_NAME);
static BOOTER_STATUS _sePkaReadPkaOperandFromBankAddress_TU10X(SE_PKA_BANK bankId, LwU32 index, LwU32 *pData, LwU32 pkaKeySizeDW) ATTR_OVLY(OVL_NAME);
static BOOTER_STATUS _sePkaWritePkaOperandToBankAddress_TU10X(SE_PKA_BANK bankId, LwU32 index, LwU32 *pData, LwU32 pkaKeySizeDW) ATTR_OVLY(OVL_NAME);
static BOOTER_STATUS _sePkaSetRadix_TU10X(SE_KEY_SIZE numBits, PSE_PKA_REG pSePkaReg) ATTR_OVLY(OVL_NAME);

/*!
 * @brief Determines the radix mask to be used in  LW_SSE_SE_PKA_CONTROL register
 *
 * @param[in] numBits    Radix or the size of the operation (key length)
 * @param[in] pSePkaReg  Struct containing the number of words based on the radix
 *
 * @return status BOOTER_ERROR_ILWALID_ARGUMENT if pSePkaReg is a NULL pointer.
 *                BOOTER_ERROR_SE_ILWALID_SE_KEY_CONFIG if an invalid key passed.
 *                BOOTER_OK if no error, and  stuct is initilized that contains
 *                sizes and values needed to address operand memory.
 */
static BOOTER_STATUS
_sePkaSetRadix_TU10X
(
    SE_KEY_SIZE numBits,
    PSE_PKA_REG pSePkaReg
)
{
    BOOTER_STATUS status  = BOOTER_OK;
    LwU32      minBits = SE_KEY_SIZE_256;

    // Check input data
    if (pSePkaReg == NULL)
    {
        return BOOTER_ERROR_ILWALID_ARGUMENT;
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
            status = BOOTER_ERROR_SE_ILWALID_KEY_CONFIG;
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
 * @return BOOTER_OK if successful otherwise BOOTER_ERROR_XXX code return.
 */
static BOOTER_STATUS
_sePkaWritePkaOperandToBankAddress_TU10X
(
    SE_PKA_BANK   bankId,
    LwU32         index,
    LwU32        *pData,
    LwU32         pkaKeySizeDW
)
{
    BOOTER_STATUS status;
    LwU32      bankAddress = 0, i;

    // Setup bus target based on which falcon we're running on.
#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    BOOTER_SELWREBUS_TARGET busTarget = SEC2_SELWREBUS_TARGET_SE;
#elif defined(BOOTER_RELOAD)
    BOOTER_SELWREBUS_TARGET busTarget = LWDEC_SELWREBUS_TARGET_SE;
#else
    ct_assert(0);
#endif

    if (pData == NULL)
    {
        return  BOOTER_ERROR_ILWALID_ARGUMENT;
    }

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(seGetPkaOperandBankAddress_HAL(pSe, bankId, &bankAddress, index, pkaKeySizeDW));

    for (i = 0; i < pkaKeySizeDW; i++)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(booterSelwreBusWriteRegister_HAL(pBooter,
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
 * @return BOOTER_OK if successful otherwise BOOTER_ERROR_XXX code return.
 */
static BOOTER_STATUS
_sePkaReadPkaOperandFromBankAddress_TU10X
(
    SE_PKA_BANK   bankId,
    LwU32         index,
    LwU32        *pData,
    LwU32         pkaKeySizeDW
)
{
    BOOTER_STATUS status;
    LwU32      bankAddress = 0, i;

    // Setup bus target based on which falcon we're running on.
#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    BOOTER_SELWREBUS_TARGET busTarget = SEC2_SELWREBUS_TARGET_SE;
#elif defined(BOOTER_RELOAD)
    BOOTER_SELWREBUS_TARGET busTarget = LWDEC_SELWREBUS_TARGET_SE;
#else
    ct_assert(0);
#endif

    if (pData == NULL)
    {
        return  BOOTER_ERROR_ILWALID_ARGUMENT;
    }

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(seGetPkaOperandBankAddress_HAL(pSe, bankId, &bankAddress, index, pkaKeySizeDW));

    for (i = 0; i < pkaKeySizeDW; i++)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(booterSelwreBusReadRegister_HAL(pBooter,
                                                                     busTarget,
                                                                     bankAddress + i * 4,
                                                                     (LwU32 *)&pData[i]));
    }

Cleanup:
    return status;
}

/*!
 * @brief To colwert SE interrupt status to BOOTER_STATUS
 *
 * @param[in] value  The input interrupt status
 *
 * @return BOOTER_OK if no error detected; otherwise BOOTER_ERROR_PKA_XXX code return.
 */
static BOOTER_STATUS
_seColwertPKAStatusToBooterStatus_TU10X
(
    LwU32 value
)
{
    BOOTER_STATUS status;


    switch (value)
    {
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_NORMAL:
            status = BOOTER_OK;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_ILWALID_OPCODE:
            status = BOOTER_ERROR_SE_PKA_OP_ILWALID_OPCODE;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_F_STACK_UNDERFLOW:
            status = BOOTER_ERROR_SE_PKA_OP_F_STACK_UNDERFLOW;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_F_STACK_OVERFLOW:
            status = BOOTER_ERROR_SE_PKA_OP_F_STACK_OVERFLOW;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_WATCHDOG:
            status = BOOTER_ERROR_SE_PKA_OP_WATCHDOG;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_HOST_REQUEST:
            status = BOOTER_ERROR_SE_PKA_OP_HOST_REQUEST;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_P_STACK_UNDERFLOW:
            status = BOOTER_ERROR_SE_PKA_OP_P_STACK_UNDERFLOW;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_P_STACK_OVERFLOW:
            status = BOOTER_ERROR_SE_PKA_OP_P_STACK_OVERFLOW;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_MEM_PORT_COLLISION:
            status = BOOTER_ERROR_SE_PKA_OP_MEM_PORT_COLLISION;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_OPERATION_SIZE_EXCEED_CFG:
            status = BOOTER_ERROR_SE_PKA_OP_OPERATION_SIZE_EXCEED_CFG;
            break;
        default:
            status = BOOTER_ERROR_SE_PKA_OP_UNKNOWN_ERR;
            break;
    }

    return status;
}

/*!
 * @brief To acquire SE engine mutex
 *
 * @return BOOTER_OK if no error detected; otherwise BOOTER_ERROR_PKA_XXX code return.
 */
BOOTER_STATUS
seAcquireMutex_TU10X
(
    void
)
{
    LwU32 val;
    BOOTER_STATUS status;

    // Setup bus target based on which falcon we're running on.
#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    BOOTER_SELWREBUS_TARGET busTarget = SEC2_SELWREBUS_TARGET_SE;
#elif defined(BOOTER_RELOAD)
    BOOTER_SELWREBUS_TARGET busTarget = LWDEC_SELWREBUS_TARGET_SE;
#else
    ct_assert(0);
#endif

    // Acquire SE global mutex
    status = booterSelwreBusReadRegister_HAL(pBooter, busTarget, LW_SSE_SE_CTRL_PKA_MUTEX, &val);
    if (val != LW_SSE_SE_CTRL_PKA_MUTEX_RC_SUCCESS || status != BOOTER_OK)
    {
        status = BOOTER_ERROR_SELWRE_BUS_REQUEST_FAILED;
    }

    return status;
}

/*!
 * @brief To release SE engine mutex
 *
 * @return BOOTER_OK if no error detected; otherwise BOOTER_ERROR_PKA_XXX code return.
 */
BOOTER_STATUS
seReleaseMutex_TU10X
(
    void
)
{
    BOOTER_STATUS status;

    // Setup bus target based on which falcon we're running on.
#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    BOOTER_SELWREBUS_TARGET busTarget = SEC2_SELWREBUS_TARGET_SE;
#elif defined(BOOTER_RELOAD)
    BOOTER_SELWREBUS_TARGET busTarget = LWDEC_SELWREBUS_TARGET_SE;
#else
    ct_assert(0);
#endif

    // Release SE global mutex
    status = booterSelwreBusWriteRegister_HAL(pBooter, busTarget,
                                           LW_SSE_SE_CTRL_PKA_MUTEX_RELEASE,
                                           LW_SSE_SE_CTRL_PKA_MUTEX_RELEASE_SE_LOCK_RELEASE);
    if (status != BOOTER_OK)
    {
        status = BOOTER_ERROR_SELWRE_BUS_REQUEST_FAILED;
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
 * @return BOOTER_OK if successful otherwise BOOTER_ERROR_XXX code return.
 */
BOOTER_STATUS
seRsaSetParams_TU10X
(
    SE_PKA_REG        *pSePkaReg,
    SE_RSA_PARAMS_TYPE paramsType,
    LwU32             *pDwBuffer
)
{
    BOOTER_STATUS  status;

    if (pDwBuffer == NULL || pSePkaReg == NULL)
    {
        return  BOOTER_ERROR_ILWALID_ARGUMENT;
    }

    if (paramsType == SE_RSA_PARAMS_TYPE_MODULUS ||
        paramsType == SE_RSA_PARAMS_TYPE_MODULUS_MONTGOMERY_EN)
    {
        // Store prime modulus in D(0)
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(_sePkaWritePkaOperandToBankAddress_TU10X(SE_PKA_BANK_D, 0, pDwBuffer, pSePkaReg->pkaKeySizeDW));


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
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(_sePkaWritePkaOperandToBankAddress_TU10X(SE_PKA_BANK_D, 1, pDwBuffer, pSePkaReg->pkaKeySizeDW));
    }
    else if (paramsType == SE_RSA_PARAMS_TYPE_MONTGOMERY_R_SQR)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(_sePkaWritePkaOperandToBankAddress_TU10X(SE_PKA_BANK_D, 3, pDwBuffer, pSePkaReg->pkaKeySizeDW));
    }
    else if (paramsType == SE_RSA_PARAMS_TYPE_EXPONENT)
    {
        // Store exponent in D(2)
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(_sePkaWritePkaOperandToBankAddress_TU10X(SE_PKA_BANK_D, 2, pDwBuffer, pSePkaReg->pkaKeySizeDW));
    }
    else if (paramsType == SE_RSA_PARAMS_TYPE_BASE)
    {
        // Store base in A(0)
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(_sePkaWritePkaOperandToBankAddress_TU10X(SE_PKA_BANK_A, 0, pDwBuffer, pSePkaReg->pkaKeySizeDW));
    }
    else
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(BOOTER_ERROR_ILWALID_ARGUMENT);
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
 * @return  BOOTER_OK if successful otherwise BOOTER_ERROR_XXX code return.
 */
BOOTER_STATUS
seRsaGetParams_TU10X
(
    SE_PKA_REG *pSePkaReg,
    SE_RSA_PARAMS_TYPE paramsType,
    LwU32  dwBuffer[]
)
{
    BOOTER_STATUS  status;

    if (dwBuffer == NULL || pSePkaReg == NULL)
    {
        return  BOOTER_ERROR_ILWALID_ARGUMENT;
    }

    if (paramsType == SE_RSA_PARAMS_TYPE_MODULUS)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(_sePkaReadPkaOperandFromBankAddress_TU10X(SE_PKA_BANK_D, 0, dwBuffer, pSePkaReg->pkaKeySizeDW));
    }
    else if (paramsType == SE_RSA_PARAMS_TYPE_MONTGOMERY_MP)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(_sePkaReadPkaOperandFromBankAddress_TU10X(SE_PKA_BANK_D, 1, dwBuffer, pSePkaReg->pkaKeySizeDW));
    }
    else if (paramsType == SE_RSA_PARAMS_TYPE_MONTGOMERY_R_SQR)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(_sePkaReadPkaOperandFromBankAddress_TU10X(SE_PKA_BANK_D, 3, dwBuffer, pSePkaReg->pkaKeySizeDW));
    }
    else if (paramsType == SE_RSA_PARAMS_TYPE_EXPONENT)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(_sePkaReadPkaOperandFromBankAddress_TU10X(SE_PKA_BANK_D, 2, dwBuffer, pSePkaReg->pkaKeySizeDW));
    }
    else if (paramsType == SE_RSA_PARAMS_TYPE_BASE ||
             paramsType == SE_RSA_PARAMS_TYPE_RSA_RESULT)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(_sePkaReadPkaOperandFromBankAddress_TU10X(SE_PKA_BANK_A, 0, dwBuffer, pSePkaReg->pkaKeySizeDW));
    }
   else
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(BOOTER_ERROR_ILWALID_ARGUMENT);
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
 * @return  BOOTER_OK if successful otherwise BOOTER_ERROR_XXX code return.
 */
BOOTER_STATUS
seStartPkaOperationAndPoll_TU10X
(
    LwU32  pkaOp,
    LwU32  radixMask,
    LwU32  timeOut
)
{
    BOOTER_STATUS status;
    LwU32      value;
    LwU32      lwrrTime;
    LwU32      startTime;

    // Setup bus target based on which falcon we're running on.
#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    BOOTER_SELWREBUS_TARGET busTarget = SEC2_SELWREBUS_TARGET_SE;
#elif defined(BOOTER_RELOAD)
    BOOTER_SELWREBUS_TARGET busTarget = LWDEC_SELWREBUS_TARGET_SE;
#else
    ct_assert(0);
#endif

    // clear L index register to run FF1 for EC Point Multiplication:
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(booterSelwreBusWriteRegister_HAL(pBooter,
                                                                  busTarget,
                                                                  LW_SSE_SE_PKA_INDEX_L,
                                                                  DRF_DEF(_SSE, _SE_PKA_INDEX_L, _INDEX_L, _CLEAR)));
    // Clear residual flags
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(booterSelwreBusWriteRegister_HAL(pBooter,
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
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(booterSelwreBusWriteRegister_HAL(pBooter,
                                                                  busTarget,
                                                                  LW_SSE_SE_PKA_FUNCTION_STACK_POINT,
                                                                  DRF_DEF(_SSE, _SE_PKA_FUNCTION_STACK_POINT, _FSTACK_POINTER, _CLEAR)));

    // Write the program entry point
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(booterSelwreBusWriteRegister_HAL(pBooter,
                                                                     busTarget,
                                                                     LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS,
                                                                     DRF_NUM(_SSE, _SE_PKA_PROGRAM_ENTRY_ADDRESS, _PROG_ADDR, pkaOp)));

    // enable interrupts
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(booterSelwreBusWriteRegister_HAL(pBooter,
                                                                  busTarget,
                                                                  LW_SSE_SE_PKA_INTERRUPT_ENABLE,
                                                                  DRF_DEF(_SSE, _SE_PKA_INTERRUPT_ENABLE, _IE_IRQ_EN, _SET)));
    // Clear IRQ status
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(booterSelwreBusWriteRegister_HAL(pBooter,
                                                                  busTarget,
                                                                  LW_SSE_SE_PKA_STATUS,
                                                                  DRF_DEF(_SSE, _SE_PKA_STATUS, _STAT_IRQ, _SET)));

    //
    // Start the PKA with assigned mode:
    //
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(booterSelwreBusWriteRegister_HAL(pBooter,
                                                                  busTarget,
                                                                  LW_SSE_SE_PKA_CONTROL,
                                                                  DRF_DEF(_SSE, _SE_PKA_CONTROL, _CTRL_GO, _ENABLE) |
                                                                  radixMask));

    startTime = BOOTER_REG_RD32(CSB, LW_CSEC_FALCON_PTIMER0);
    //
    // Poll the status register until PKA operation is complete. This is
    // indicated by STAT_IRQ bit in PKA_STATUS.  Once set, this bit will
    // remain set until cleared by writing to it.
    //
    do
    {
        lwrrTime = BOOTER_REG_RD32(CSB, LW_CSEC_FALCON_PTIMER0);

        if ((lwrrTime - startTime) > timeOut)
        {
            return BOOTER_ERROR_SE_PKA_OP_OPERATION_TIMEOUT;
        }

        CHECK_STATUS_OK_OR_GOTO_CLEANUP(booterSelwreBusReadRegister_HAL(pBooter, busTarget, LW_SSE_SE_PKA_STATUS, &value));
    }
    while (FLD_TEST_DRF(_SSE, _SE_PKA_STATUS, _STAT_IRQ, _UNSET, value));

    // Clear the PKA op completed bit  (see above)
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(booterSelwreBusWriteRegister_HAL(pBooter,
                                                                  busTarget,
                                                                  LW_SSE_SE_PKA_STATUS,
                                                                  DRF_DEF(_SSE, _SE_PKA_STATUS, _STAT_IRQ, _SET)));


    // Colwert PKA status return code to SE_STATUS and return
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(booterSelwreBusReadRegister_HAL(pBooter, busTarget, LW_SSE_SE_PKA_RETURN_CODE, &value));
    value = DRF_VAL(_SSE, _SE_PKA_RETURN_CODE, _RC_STOP_REASON, value);
    status = _seColwertPKAStatusToBooterStatus_TU10X(value);

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
 * @return BOOTER_OK if successful otherwise BOOTER_ERROR_XXX code return.
 */
BOOTER_STATUS
seGetPkaOperandBankAddress_TU10X
(
    SE_PKA_BANK bankId,
    LwU32 *pBankAddr,
    LwU32 idx,
    LwU32 pkaKeySizeDW
)
{
    BOOTER_STATUS status;
    LwU32     bankOffset;
    LwU64     tmp64;

    if (pBankAddr == NULL)
    {
        return BOOTER_ERROR_ILWALID_ARGUMENT;
    }

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(seGetSpecificPkaOperandBankOffset_HAL(pSe, bankId, &bankOffset));
    tmp64 = booterMultipleUnsigned32(idx <<2, pkaKeySizeDW);
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
 * @return     BOOTER_OK if successful otherwise BOOTER_ERROR_XXX code return.
 */
BOOTER_STATUS
seGetSpecificPkaOperandBankOffset_TU10X
(
    SE_PKA_BANK  bankId,
    LwU32 *pBankOffset
)
{
    LwU32 baseAddress;
    BOOTER_STATUS status = BOOTER_OK;

    if (pBankOffset == NULL)
    {
        return BOOTER_ERROR_ILWALID_ARGUMENT;
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
            status = BOOTER_ERROR_ILWALID_ARGUMENT;
            break;
    }

    return status;
}

/*!
 * @brief Function to get base Pka operand bank base address
 *
 * @return LwU32 The bank base address to return.
 */
LwU32 seGetPkaOperandBankBaseAddress_TU10X(void)
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
 * @return     BOOTER_OK if successful otherwise BOOTER_ERROR_XXX code return.
 */
BOOTER_STATUS
seSetRsaOperationKeys_TU10X
(
    SE_PKA_REG *pSePkaReg,
    LwU32      *pModulus,
    LwU32      *pExp,
    LwU32      *pMp,
    LwU32      *pRSqr
)
{
    BOOTER_STATUS status;

    if (pSePkaReg == NULL ||
        pModulus  == NULL ||
        pExp      == NULL)
    {
        return BOOTER_ERROR_ILWALID_ARGUMENT;
    }

    if ((pMp == NULL && pRSqr != NULL) ||
        (pMp != NULL && pRSqr == NULL))
    {
        return BOOTER_ERROR_SE_ILWALID_KEY_CONFIG;
    }
    else if (pMp != NULL && pRSqr != NULL)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(seRsaSetParams_TU10X(pSePkaReg, SE_RSA_PARAMS_TYPE_MODULUS, pModulus));
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(seRsaSetParams_TU10X(pSePkaReg, SE_RSA_PARAMS_TYPE_MONTGOMERY_MP, pMp));
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(seRsaSetParams_TU10X(pSePkaReg, SE_RSA_PARAMS_TYPE_EXPONENT, pExp));
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(seRsaSetParams_TU10X(pSePkaReg, SE_RSA_PARAMS_TYPE_MONTGOMERY_R_SQR, pRSqr));
    }
    else  // i.e. (pMp == NULL && pRSqr == NULL)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(seRsaSetParams_TU10X(pSePkaReg, SE_RSA_PARAMS_TYPE_MODULUS_MONTGOMERY_EN, pModulus));
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(seRsaSetParams_TU10X(pSePkaReg, SE_RSA_PARAMS_TYPE_EXPONENT, pExp));
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
 * @return        BOOTER_OK if successful otherwise BOOTER_ERROR_XXX code return.
 */
BOOTER_STATUS
seContextIni_TU10X
(
    SE_PKA_REG *pSePkaReg,
    SE_KEY_SIZE keySize
)
{
    BOOTER_STATUS status;

    // Setup bus target based on which falcon we're running on.
#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    BOOTER_SELWREBUS_TARGET busTarget = SEC2_SELWREBUS_TARGET_SE;
#elif defined(BOOTER_RELOAD)
    BOOTER_SELWREBUS_TARGET busTarget = LWDEC_SELWREBUS_TARGET_SE;
#else
    ct_assert(0);
#endif

    if (pSePkaReg == NULL)
    {
        return BOOTER_ERROR_ILWALID_ARGUMENT;
    }

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(_sePkaSetRadix_TU10X(keySize, pSePkaReg));

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(booterSelwreBusWriteRegister_HAL(pBooter,
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
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(booterSelwreBusWriteRegister_HAL(pBooter,
                                                                  busTarget,
                                                                  LW_SSE_SE_CTRL_SCC_CONTROL,
                                                                  DRF_DEF(_SSE, _SE_CTRL_SCC_CONTROL_DPA, _DISABLE, _SET)));

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(booterSelwreBusWriteRegister_HAL(pBooter,
                                                                  busTarget,
                                                                  LW_SSE_SE_PKA_JUMP_PROBABILITY,
                                                                  DRF_DEF(_SSE, _SE_PKA_JUMP_PROBABILITY, _JUMP_PROBABILITY, _CLEAR)));
Cleanup:
    return status;
}

//TODO(suppal): Update this per chip
BOOTER_STATUS
seInitializationRsaKeyProd_TU10X
(
    LwU32 *pModulusProd,
    LwU32 *pExponentProd,
    LwU32 *pMpProd,
    LwU32 *pRsqrProd
)
{
    if (pModulusProd == NULL || pExponentProd == NULL ||
        pMpProd      == NULL || pRsqrProd     == NULL)
    {
        return BOOTER_ERROR_ILWALID_ARGUMENT;
    }

    pModulusProd[0]   = 0x97d2077d; pModulusProd[1]   = 0xcb1bdd87; pModulusProd[2]   = 0xea017216; pModulusProd[3]   = 0x2a58daa6;
    pModulusProd[4]   = 0xb41fbb9c; pModulusProd[5]   = 0xd1815254; pModulusProd[6]   = 0x884a9782; pModulusProd[7]   = 0xe1a6e4ad;
    pModulusProd[8]   = 0x8d131aff; pModulusProd[9]   = 0xee466176; pModulusProd[10]  = 0xbfa3b809; pModulusProd[11]  = 0x2952c580;
    pModulusProd[12]  = 0x1db2dd1e; pModulusProd[13]  = 0xfcc90b6a; pModulusProd[14]  = 0xf5cb0b37; pModulusProd[15]  = 0x45333dad;
    pModulusProd[16]  = 0x06f73c16; pModulusProd[17]  = 0x2acf9e56; pModulusProd[18]  = 0x8d3a3a5c; pModulusProd[19]  = 0xda89fe4d;
    pModulusProd[20]  = 0x309f0093; pModulusProd[21]  = 0x00b4969c; pModulusProd[22]  = 0x72c7e0c9; pModulusProd[23]  = 0x6f0a91e0;
    pModulusProd[24]  = 0xf6666fe6; pModulusProd[25]  = 0x4f03e2f7; pModulusProd[26]  = 0x9fbf9999; pModulusProd[27]  = 0x8640ae4e;
    pModulusProd[28]  = 0xf19da7e4; pModulusProd[29]  = 0x2016632e; pModulusProd[30]  = 0xaf28b3f3; pModulusProd[31]  = 0xe5635aae;
    pModulusProd[32]  = 0xa12ce1db; pModulusProd[33]  = 0xef2054d3; pModulusProd[34]  = 0x50abe007; pModulusProd[35]  = 0xc6a4447c;
    pModulusProd[36]  = 0x5daa4f1c; pModulusProd[37]  = 0xa1d23b2c; pModulusProd[38]  = 0x7aebafa2; pModulusProd[39]  = 0x4172ded3;
    pModulusProd[40]  = 0x0a2fe0b7; pModulusProd[41]  = 0x9c630bf0; pModulusProd[42]  = 0x06200523; pModulusProd[43]  = 0x27379c83;
    pModulusProd[44]  = 0x857bab7c; pModulusProd[45]  = 0x6278ea3d; pModulusProd[46]  = 0xf72531ed; pModulusProd[47]  = 0xd91c7ad3;
    pModulusProd[48]  = 0x151655c6; pModulusProd[49]  = 0x06c0578d; pModulusProd[50]  = 0x621769cf; pModulusProd[51]  = 0xd43b2ae9;
    pModulusProd[52]  = 0xa8618aa1; pModulusProd[53]  = 0x04db96b7; pModulusProd[54]  = 0x491b55a4; pModulusProd[55]  = 0x0f06f12a;
    pModulusProd[56]  = 0xfb622f9c; pModulusProd[57]  = 0x4abbc16e; pModulusProd[58]  = 0x6e946c1f; pModulusProd[59]  = 0x8856a346;
    pModulusProd[60]  = 0xeb238023; pModulusProd[61]  = 0xace79ca9; pModulusProd[62]  = 0x56eb7878; pModulusProd[63]  = 0x885938cc;
    pModulusProd[64]  = 0x92a437db; pModulusProd[65]  = 0xb195706d; pModulusProd[66]  = 0x8547eb0f; pModulusProd[67]  = 0xb584df78;
    pModulusProd[68]  = 0x972b572c; pModulusProd[69]  = 0xc49abf79; pModulusProd[70]  = 0x80e3a025; pModulusProd[71]  = 0x355d4307;
    pModulusProd[72]  = 0x3532328b; pModulusProd[73]  = 0xd345fd12; pModulusProd[74]  = 0x86054da2; pModulusProd[75]  = 0x49ca5313;
    pModulusProd[76]  = 0x14550c80; pModulusProd[77]  = 0xc541f5cc; pModulusProd[78]  = 0x3d6b606d; pModulusProd[79]  = 0x966e8468;
    pModulusProd[80]  = 0x639b0b6b; pModulusProd[81]  = 0xde5b07e7; pModulusProd[82]  = 0xbbc231ad; pModulusProd[83]  = 0xf3981414;
    pModulusProd[84]  = 0xc77e6979; pModulusProd[85]  = 0x027800f7; pModulusProd[86]  = 0x2d7fd7a8; pModulusProd[87]  = 0x49b8ec2e;
    pModulusProd[88]  = 0xec466c05; pModulusProd[89]  = 0x82e5acbe; pModulusProd[90]  = 0x15c61888; pModulusProd[91]  = 0xabe6c6d5;
    pModulusProd[92]  = 0x6f98bdd5; pModulusProd[93]  = 0xd3935670; pModulusProd[94]  = 0x9e6158ce; pModulusProd[95]  = 0x9da1c2f7;
    pModulusProd[96]  = 0x00000000; pModulusProd[97]  = 0x00000000; pModulusProd[98]  = 0x00000000; pModulusProd[99]  = 0x00000000;
    pModulusProd[100] = 0x00000000; pModulusProd[101] = 0x00000000; pModulusProd[102] = 0x00000000; pModulusProd[103] = 0x00000000;
    pModulusProd[104] = 0x00000000; pModulusProd[105] = 0x00000000; pModulusProd[106] = 0x00000000; pModulusProd[107] = 0x00000000;
    pModulusProd[108] = 0x00000000; pModulusProd[109] = 0x00000000; pModulusProd[110] = 0x00000000; pModulusProd[111] = 0x00000000;
    pModulusProd[112] = 0x00000000; pModulusProd[113] = 0x00000000; pModulusProd[114] = 0x00000000; pModulusProd[115] = 0x00000000;
    pModulusProd[116] = 0x00000000; pModulusProd[117] = 0x00000000; pModulusProd[118] = 0x00000000; pModulusProd[119] = 0x00000000;
    pModulusProd[120] = 0x00000000; pModulusProd[121] = 0x00000000; pModulusProd[122] = 0x00000000; pModulusProd[123] = 0x00000000;
    pModulusProd[124] = 0x00000000; pModulusProd[125] = 0x00000000; pModulusProd[126] = 0x00000000; pModulusProd[127] = 0x00000000;

    pExponentProd[0]   = 0x00010001; pExponentProd[1]   = 0x00000000; pExponentProd[2]   = 0x00000000; pExponentProd[3]   = 0x00000000;
    pExponentProd[4]   = 0x00000000; pExponentProd[5]   = 0x00000000; pExponentProd[6]   = 0x00000000; pExponentProd[7]   = 0x00000000;
    pExponentProd[8]   = 0x00000000; pExponentProd[9]   = 0x00000000; pExponentProd[10]  = 0x00000000; pExponentProd[11]  = 0x00000000;
    pExponentProd[12]  = 0x00000000; pExponentProd[13]  = 0x00000000; pExponentProd[14]  = 0x00000000; pExponentProd[15]  = 0x00000000;
    pExponentProd[16]  = 0x00000000; pExponentProd[17]  = 0x00000000; pExponentProd[18]  = 0x00000000; pExponentProd[19]  = 0x00000000;
    pExponentProd[20]  = 0x00000000; pExponentProd[21]  = 0x00000000; pExponentProd[22]  = 0x00000000; pExponentProd[23]  = 0x00000000;
    pExponentProd[24]  = 0x00000000; pExponentProd[25]  = 0x00000000; pExponentProd[26]  = 0x00000000; pExponentProd[27]  = 0x00000000;
    pExponentProd[28]  = 0x00000000; pExponentProd[29]  = 0x00000000; pExponentProd[30]  = 0x00000000; pExponentProd[31]  = 0x00000000;
    pExponentProd[32]  = 0x00000000; pExponentProd[33]  = 0x00000000; pExponentProd[34]  = 0x00000000; pExponentProd[35]  = 0x00000000;
    pExponentProd[36]  = 0x00000000; pExponentProd[37]  = 0x00000000; pExponentProd[38]  = 0x00000000; pExponentProd[39]  = 0x00000000;
    pExponentProd[40]  = 0x00000000; pExponentProd[41]  = 0x00000000; pExponentProd[42]  = 0x00000000; pExponentProd[43]  = 0x00000000;
    pExponentProd[44]  = 0x00000000; pExponentProd[45]  = 0x00000000; pExponentProd[46]  = 0x00000000; pExponentProd[47]  = 0x00000000;
    pExponentProd[48]  = 0x00000000; pExponentProd[49]  = 0x00000000; pExponentProd[50]  = 0x00000000; pExponentProd[51]  = 0x00000000;
    pExponentProd[52]  = 0x00000000; pExponentProd[53]  = 0x00000000; pExponentProd[54]  = 0x00000000; pExponentProd[55]  = 0x00000000;
    pExponentProd[56]  = 0x00000000; pExponentProd[57]  = 0x00000000; pExponentProd[58]  = 0x00000000; pExponentProd[59]  = 0x00000000;
    pExponentProd[60]  = 0x00000000; pExponentProd[61]  = 0x00000000; pExponentProd[62]  = 0x00000000; pExponentProd[63]  = 0x00000000;
    pExponentProd[64]  = 0x00000000; pExponentProd[65]  = 0x00000000; pExponentProd[66]  = 0x00000000; pExponentProd[67]  = 0x00000000;
    pExponentProd[68]  = 0x00000000; pExponentProd[69]  = 0x00000000; pExponentProd[70]  = 0x00000000; pExponentProd[71]  = 0x00000000;
    pExponentProd[72]  = 0x00000000; pExponentProd[73]  = 0x00000000; pExponentProd[74]  = 0x00000000; pExponentProd[75]  = 0x00000000;
    pExponentProd[76]  = 0x00000000; pExponentProd[77]  = 0x00000000; pExponentProd[78]  = 0x00000000; pExponentProd[79]  = 0x00000000;
    pExponentProd[80]  = 0x00000000; pExponentProd[81]  = 0x00000000; pExponentProd[82]  = 0x00000000; pExponentProd[83]  = 0x00000000;
    pExponentProd[84]  = 0x00000000; pExponentProd[85]  = 0x00000000; pExponentProd[86]  = 0x00000000; pExponentProd[87]  = 0x00000000;
    pExponentProd[88]  = 0x00000000; pExponentProd[89]  = 0x00000000; pExponentProd[90]  = 0x00000000; pExponentProd[91]  = 0x00000000;
    pExponentProd[92]  = 0x00000000; pExponentProd[93]  = 0x00000000; pExponentProd[94]  = 0x00000000; pExponentProd[95]  = 0x00000000;
    pExponentProd[96]  = 0x00000000; pExponentProd[97]  = 0x00000000; pExponentProd[98]  = 0x00000000; pExponentProd[99]  = 0x00000000;
    pExponentProd[100] = 0x00000000; pExponentProd[101] = 0x00000000; pExponentProd[102] = 0x00000000; pExponentProd[103] = 0x00000000;
    pExponentProd[104] = 0x00000000; pExponentProd[105] = 0x00000000; pExponentProd[106] = 0x00000000; pExponentProd[107] = 0x00000000;
    pExponentProd[108] = 0x00000000; pExponentProd[109] = 0x00000000; pExponentProd[110] = 0x00000000; pExponentProd[111] = 0x00000000;
    pExponentProd[112] = 0x00000000; pExponentProd[113] = 0x00000000; pExponentProd[114] = 0x00000000; pExponentProd[115] = 0x00000000;
    pExponentProd[116] = 0x00000000; pExponentProd[117] = 0x00000000; pExponentProd[118] = 0x00000000; pExponentProd[119] = 0x00000000;
    pExponentProd[120] = 0x00000000; pExponentProd[121] = 0x00000000; pExponentProd[122] = 0x00000000; pExponentProd[123] = 0x00000000;
    pExponentProd[124] = 0x00000000; pExponentProd[125] = 0x00000000; pExponentProd[126] = 0x00000000; pExponentProd[127] = 0x00000000;

    pMpProd[0]   = 0xf3a4162b; pMpProd[1]   = 0x944e7aa9; pMpProd[2]   = 0xae263ede; pMpProd[3]   = 0xd55869cc;
    pMpProd[4]   = 0xf38f66f4; pMpProd[5]   = 0xd3777ddc; pMpProd[6]   = 0x49b48ebd; pMpProd[7]   = 0x7b99cd0a;
    pMpProd[8]   = 0x36705fd2; pMpProd[9]   = 0x9b5e992f; pMpProd[10]  = 0x9a0b857c; pMpProd[11]  = 0x5772c509;
    pMpProd[12]  = 0x34ba11cb; pMpProd[13]  = 0x9418dc76; pMpProd[14]  = 0xb5008d95; pMpProd[15]  = 0xdfc86d08;
    pMpProd[16]  = 0xd090f817; pMpProd[17]  = 0xbc2261d5; pMpProd[18]  = 0xfcc17a2e; pMpProd[19]  = 0xd88da8b1;
    pMpProd[20]  = 0xad67aec8; pMpProd[21]  = 0x64acfd49; pMpProd[22]  = 0x13d8ad51; pMpProd[23]  = 0x3aa0833a;
    pMpProd[24]  = 0x844e9aed; pMpProd[25]  = 0xe2f547e9; pMpProd[26]  = 0x337f0f19; pMpProd[27]  = 0xe6bd10e8;
    pMpProd[28]  = 0xfd6c4164; pMpProd[29]  = 0xa601c175; pMpProd[30]  = 0x9f37e85d; pMpProd[31]  = 0x9a5bc3ef;
    pMpProd[32]  = 0x547c5eac; pMpProd[33]  = 0x356376d9; pMpProd[34]  = 0xfca36d79; pMpProd[35]  = 0x625f89d4;
    pMpProd[36]  = 0x4b426b59; pMpProd[37]  = 0xb15a68f9; pMpProd[38]  = 0x567c8bf3; pMpProd[39]  = 0x6f5b07d1;
    pMpProd[40]  = 0x6f7d8d3e; pMpProd[41]  = 0xb2080463; pMpProd[42]  = 0x7c739fde; pMpProd[43]  = 0x6e39a65b;
    pMpProd[44]  = 0x37eb85ad; pMpProd[45]  = 0xa90ca1bc; pMpProd[46]  = 0x03fd0992; pMpProd[47]  = 0x2e1d34c8;
    pMpProd[48]  = 0x4445a550; pMpProd[49]  = 0x3bbc9f25; pMpProd[50]  = 0xdfd0bb6e; pMpProd[51]  = 0xc8d7982a;
    pMpProd[52]  = 0x9abb639c; pMpProd[53]  = 0xd52c153d; pMpProd[54]  = 0x3eb5289e; pMpProd[55]  = 0x0408c285;
    pMpProd[56]  = 0x2f1d43e7; pMpProd[57]  = 0xb17be13a; pMpProd[58]  = 0x5207f2e0; pMpProd[59]  = 0x04e597ec;
    pMpProd[60]  = 0x9f05b0cf; pMpProd[61]  = 0x437cd38d; pMpProd[62]  = 0x9f88952d; pMpProd[63]  = 0x255ea16f;
    pMpProd[64]  = 0xfad42f13; pMpProd[65]  = 0x503ba423; pMpProd[66]  = 0x4b85f82b; pMpProd[67]  = 0x734bc52c;
    pMpProd[68]  = 0x4fc7208e; pMpProd[69]  = 0x56237ddb; pMpProd[70]  = 0x24718671; pMpProd[71]  = 0x4d30dd53;
    pMpProd[72]  = 0x87ca112a; pMpProd[73]  = 0x0d0ae5ff; pMpProd[74]  = 0xecd1c969; pMpProd[75]  = 0x1b9accbe;
    pMpProd[76]  = 0x1a323ee0; pMpProd[77]  = 0x351e9668; pMpProd[78]  = 0x059c46bb; pMpProd[79]  = 0x0610fcf3;
    pMpProd[80]  = 0xe838f18b; pMpProd[81]  = 0xe7ace78c; pMpProd[82]  = 0xa29b3a6c; pMpProd[83]  = 0xbe1f6373;
    pMpProd[84]  = 0x3e2631c4; pMpProd[85]  = 0xc2b3383a; pMpProd[86]  = 0x6b0c93bd; pMpProd[87]  = 0x1e4f7312;
    pMpProd[88]  = 0x7ffc3a64; pMpProd[89]  = 0x0eb89669; pMpProd[90]  = 0xfb0a3604; pMpProd[91]  = 0xbcf51658;
    pMpProd[92]  = 0x6f696b20; pMpProd[93]  = 0x1eef5872; pMpProd[94]  = 0x050a1603; pMpProd[95]  = 0x8dec1876;
    pMpProd[96]  = 0x00000000; pMpProd[97]  = 0x00000000; pMpProd[98]  = 0x00000000; pMpProd[99]  = 0x00000000;
    pMpProd[100] = 0x00000000; pMpProd[101] = 0x00000000; pMpProd[102] = 0x00000000; pMpProd[103] = 0x00000000;
    pMpProd[104] = 0x00000000; pMpProd[105] = 0x00000000; pMpProd[106] = 0x00000000; pMpProd[107] = 0x00000000;
    pMpProd[108] = 0x00000000; pMpProd[109] = 0x00000000; pMpProd[110] = 0x00000000; pMpProd[111] = 0x00000000;
    pMpProd[112] = 0x00000000; pMpProd[113] = 0x00000000; pMpProd[114] = 0x00000000; pMpProd[115] = 0x00000000;
    pMpProd[116] = 0x00000000; pMpProd[117] = 0x00000000; pMpProd[118] = 0x00000000; pMpProd[119] = 0x00000000;
    pMpProd[120] = 0x00000000; pMpProd[121] = 0x00000000; pMpProd[122] = 0x00000000; pMpProd[123] = 0x00000000;
    pMpProd[124] = 0x00000000; pMpProd[125] = 0x00000000; pMpProd[126] = 0x00000000; pMpProd[127] = 0x00000000;

    pRsqrProd[0]   = 0x224f8a58; pRsqrProd[1]   = 0x7fd35e69; pRsqrProd[2]   = 0xd5428432;  pRsqrProd[3]  = 0x3933bd96;
    pRsqrProd[4]   = 0x0e8f82cf; pRsqrProd[5]   = 0x0c60c807; pRsqrProd[6]   = 0xd40f7235;  pRsqrProd[7]  = 0xcb8dba01;
    pRsqrProd[8]   = 0x80e177fc; pRsqrProd[9]   = 0xfb4d1497; pRsqrProd[10]  = 0x5e32401d; pRsqrProd[11]  = 0xf37a8ec1;
    pRsqrProd[12]  = 0x2909af14; pRsqrProd[13]  = 0xe59d12bf; pRsqrProd[14]  = 0x73e00c2f; pRsqrProd[15]  = 0x42eec21b;
    pRsqrProd[16]  = 0xb200df01; pRsqrProd[17]  = 0xb1843678; pRsqrProd[18]  = 0x345e638a; pRsqrProd[19]  = 0x5c52d9f1;
    pRsqrProd[20]  = 0xddce30c9; pRsqrProd[21]  = 0xeedb7ba3; pRsqrProd[22]  = 0xbb012224; pRsqrProd[23]  = 0xbfda4fc8;
    pRsqrProd[24]  = 0x2e344848; pRsqrProd[25]  = 0xeb5975e3; pRsqrProd[26]  = 0x612b3234; pRsqrProd[27]  = 0x79989124;
    pRsqrProd[28]  = 0xc2c5eafb; pRsqrProd[29]  = 0xd0354953; pRsqrProd[30]  = 0xfaec9d0d; pRsqrProd[31]  = 0x59f0a7f6;
    pRsqrProd[32]  = 0x7662bcc2; pRsqrProd[33]  = 0x7b9e91a6; pRsqrProd[34]  = 0x0ab4c31e; pRsqrProd[35]  = 0xda31da4a;
    pRsqrProd[36]  = 0x1108ce7c; pRsqrProd[37]  = 0x0b91dd1d; pRsqrProd[38]  = 0x556efe91; pRsqrProd[39]  = 0x33e48326;
    pRsqrProd[40]  = 0x49292556; pRsqrProd[41]  = 0xd402ea6a; pRsqrProd[42]  = 0x9507d63c; pRsqrProd[43]  = 0x15cc5032;
    pRsqrProd[44]  = 0x6df5a84d; pRsqrProd[45]  = 0xdddb2a85; pRsqrProd[46]  = 0x1b3e3714; pRsqrProd[47]  = 0x73618845;
    pRsqrProd[48]  = 0xeab7226f; pRsqrProd[49]  = 0x879732de; pRsqrProd[50]  = 0xcfe96975; pRsqrProd[51]  = 0x238898af;
    pRsqrProd[52]  = 0x5da73178; pRsqrProd[53]  = 0x590d49fa; pRsqrProd[54]  = 0xd86caf30; pRsqrProd[55]  = 0x68d4665c;
    pRsqrProd[56]  = 0xf78b4c62; pRsqrProd[57]  = 0xa23bc48e; pRsqrProd[58]  = 0x27963d8a; pRsqrProd[59]  = 0x665e4d64;
    pRsqrProd[60]  = 0x6f24efec; pRsqrProd[61]  = 0xbbfe2bb1; pRsqrProd[62]  = 0x45e7ddc8; pRsqrProd[63]  = 0x3e15b000;
    pRsqrProd[64]  = 0xfdde2554; pRsqrProd[65]  = 0x07214438; pRsqrProd[66]  = 0xc5a26daa; pRsqrProd[67]  = 0x93e5e828;
    pRsqrProd[68]  = 0xec0e8801; pRsqrProd[69]  = 0x8392b22c; pRsqrProd[70]  = 0xfc95516e; pRsqrProd[71]  = 0xf7836245;
    pRsqrProd[72]  = 0xaf722c1e; pRsqrProd[73]  = 0x3f894d6f; pRsqrProd[74]  = 0x1794d543; pRsqrProd[75]  = 0x9dddd415;
    pRsqrProd[76]  = 0x3dbc5449; pRsqrProd[77]  = 0xa81ee21c; pRsqrProd[78]  = 0x24e7a33a; pRsqrProd[79]  = 0xea34e845;
    pRsqrProd[80]  = 0x5cb9c616; pRsqrProd[81]  = 0x41e67128; pRsqrProd[82]  = 0x8bde36ca; pRsqrProd[83]  = 0x0617f59a;
    pRsqrProd[84]  = 0xbf99b654; pRsqrProd[85]  = 0x4d111429; pRsqrProd[86]  = 0x44b414ed; pRsqrProd[87]  = 0x2d71d3ec;
    pRsqrProd[88]  = 0xac452c61; pRsqrProd[89]  = 0xac1bf8fb; pRsqrProd[90]  = 0x7e5ce60b; pRsqrProd[91]  = 0x3890e9cf;
    pRsqrProd[92]  = 0xfe90d686; pRsqrProd[93]  = 0x7c7fd6ae; pRsqrProd[94]  = 0xe2bab52e; pRsqrProd[95]  = 0x9022bcab;
    pRsqrProd[96]  = 0x00000000; pRsqrProd[97]  = 0x00000000; pRsqrProd[98]  = 0x00000000; pRsqrProd[99]  = 0x00000000;
    pRsqrProd[100] = 0x00000000; pRsqrProd[101] = 0x00000000; pRsqrProd[102] = 0x00000000; pRsqrProd[103] = 0x00000000;
    pRsqrProd[104] = 0x00000000; pRsqrProd[105] = 0x00000000; pRsqrProd[106] = 0x00000000; pRsqrProd[107] = 0x00000000;
    pRsqrProd[108] = 0x00000000; pRsqrProd[109] = 0x00000000; pRsqrProd[110] = 0x00000000; pRsqrProd[111] = 0x00000000;
    pRsqrProd[112] = 0x00000000; pRsqrProd[113] = 0x00000000; pRsqrProd[114] = 0x00000000; pRsqrProd[115] = 0x00000000;
    pRsqrProd[116] = 0x00000000; pRsqrProd[117] = 0x00000000; pRsqrProd[118] = 0x00000000; pRsqrProd[119] = 0x00000000;
    pRsqrProd[120] = 0x00000000; pRsqrProd[121] = 0x00000000; pRsqrProd[122] = 0x00000000; pRsqrProd[123] = 0x00000000;
    pRsqrProd[124] = 0x00000000; pRsqrProd[125] = 0x00000000; pRsqrProd[126] = 0x00000000; pRsqrProd[127] = 0x00000000;

    return BOOTER_OK;
}

BOOTER_STATUS
seInitializationRsaKeyDbg_TU10X
(
    LwU32 *pModulusDbg,
    LwU32 *pExponentDbg,
    LwU32 *pMpDbg,
    LwU32 *pRsqrDbg
)
{
    if (pModulusDbg == NULL || pExponentDbg == NULL ||
        pMpDbg      == NULL || pRsqrDbg     == NULL)
    {
        return BOOTER_ERROR_ILWALID_ARGUMENT;
    }

    pModulusDbg[0]   = 0x6404288d; pModulusDbg[1]   = 0xc3efcb20; pModulusDbg[2]   = 0xd0aae02c; pModulusDbg[3]  = 0x06adc570;
    pModulusDbg[4]   = 0xb23c4ce6; pModulusDbg[5]   = 0x46ed46ab; pModulusDbg[6]   = 0x4392a48c; pModulusDbg[7]  = 0x9b3aa00d;
    pModulusDbg[8]   = 0xb1102d00; pModulusDbg[9]   = 0xf4d95994; pModulusDbg[10]  = 0x5a5ff012; pModulusDbg[11] = 0x2d5a61e7;
    pModulusDbg[12]  = 0xfeab8251; pModulusDbg[13]  = 0x51dda5a3; pModulusDbg[14]  = 0xc475d928; pModulusDbg[15] = 0xf1036983;
    pModulusDbg[16]  = 0xd9207b7b; pModulusDbg[17]  = 0xc3aed23b; pModulusDbg[18]  = 0x6333437d; pModulusDbg[19] = 0x27c6c5f6;
    pModulusDbg[20]  = 0xb140f5ad; pModulusDbg[21]  = 0x02741a48; pModulusDbg[22]  = 0x5327dc45; pModulusDbg[23] = 0x61b927d2;
    pModulusDbg[24]  = 0xb050f67f; pModulusDbg[25]  = 0x4e67771b; pModulusDbg[26]  = 0x404c2842; pModulusDbg[27] = 0x6e40169d;
    pModulusDbg[28]  = 0xdb57e5c5; pModulusDbg[29]  = 0x4afb15a1; pModulusDbg[30]  = 0x3dc2ba0c; pModulusDbg[31] = 0x6e0d7696;
    pModulusDbg[32]  = 0x39e98c31; pModulusDbg[33]  = 0xd0e70fc9; pModulusDbg[34]  = 0x3d7c6406; pModulusDbg[35] = 0x9315dd98;
    pModulusDbg[36]  = 0x6d7a9422; pModulusDbg[37]  = 0xf3b1259b; pModulusDbg[38]  = 0x56911699; pModulusDbg[39] = 0xf3b34a4a;
    pModulusDbg[40]  = 0xe3145b36; pModulusDbg[41]  = 0x1c2f0b42; pModulusDbg[42]  = 0xc74cbbf8; pModulusDbg[43] = 0xb750c689;
    pModulusDbg[44]  = 0x6ca212ed; pModulusDbg[45]  = 0x3110a1ce; pModulusDbg[46]  = 0x75a9bcbf; pModulusDbg[47] = 0x19ee7f65;
    pModulusDbg[48]  = 0x420cb3fe; pModulusDbg[49]  = 0x0771cda1; pModulusDbg[50]  = 0x267fdb1f; pModulusDbg[51] = 0x693ffa8e;
    pModulusDbg[52]  = 0x6baf8baf; pModulusDbg[53]  = 0x4300a914; pModulusDbg[54]  = 0x97478df6; pModulusDbg[55] = 0x78f8655b;
    pModulusDbg[56]  = 0x95ebc445; pModulusDbg[57]  = 0x621fe604; pModulusDbg[58]  = 0x6099c478; pModulusDbg[59] = 0xc17fc515;
    pModulusDbg[60]  = 0x33875702; pModulusDbg[61]  = 0x66b4e135; pModulusDbg[62]  = 0xc2234dcf; pModulusDbg[63] = 0x5416f007;
    pModulusDbg[64]  = 0x06e1ec20; pModulusDbg[65]  = 0xc27f41e5; pModulusDbg[66]  = 0x15eaa226; pModulusDbg[67] = 0x94da66c8;
    pModulusDbg[68]  = 0xb3ed717c; pModulusDbg[69]  = 0x05f82c96; pModulusDbg[70]  = 0x1f649983; pModulusDbg[71] = 0x1b161a04;
    pModulusDbg[72]  = 0x4653a11f; pModulusDbg[73]  = 0x08c44fa9; pModulusDbg[74]  = 0xff5c792f; pModulusDbg[75] = 0x630a22e3;
    pModulusDbg[76]  = 0xbebc1e1d; pModulusDbg[77]  = 0x999fc596; pModulusDbg[78]  = 0xef16b029; pModulusDbg[79] = 0x9dbff0fc;
    pModulusDbg[80]  = 0x1b7eb531; pModulusDbg[81]  = 0xd441f918; pModulusDbg[82]  = 0xeae40439; pModulusDbg[83] = 0x88b5f9e9;
    pModulusDbg[84]  = 0x6f4d308a; pModulusDbg[85]  = 0x35449529; pModulusDbg[86]  = 0xc8337ce3; pModulusDbg[87] = 0x508eae45;
    pModulusDbg[88]  = 0xe059f539; pModulusDbg[89]  = 0x3ddf3dca; pModulusDbg[90]  = 0x598f19a7; pModulusDbg[91] = 0x2f12abe4;
    pModulusDbg[92]  = 0x6af4164f; pModulusDbg[93]  = 0x25a7f417; pModulusDbg[94]  = 0xac4a5570; pModulusDbg[95] = 0xc6f1a2c0;
    pModulusDbg[96]  = 0x00000000; pModulusDbg[97]  = 0x00000000; pModulusDbg[98]  = 0x00000000; pModulusDbg[99] = 0x00000000;
    pModulusDbg[100] = 0x00000000; pModulusDbg[101] = 0x00000000; pModulusDbg[102] = 0x00000000; pModulusDbg[103] = 0x00000000;
    pModulusDbg[104] = 0x00000000; pModulusDbg[105] = 0x00000000; pModulusDbg[106] = 0x00000000; pModulusDbg[107] = 0x00000000;
    pModulusDbg[108] = 0x00000000; pModulusDbg[109] = 0x00000000; pModulusDbg[110] = 0x00000000; pModulusDbg[111] = 0x00000000;
    pModulusDbg[112] = 0x00000000; pModulusDbg[113] = 0x00000000; pModulusDbg[114] = 0x00000000; pModulusDbg[115] = 0x00000000;
    pModulusDbg[116] = 0x00000000; pModulusDbg[117] = 0x00000000; pModulusDbg[118] = 0x00000000; pModulusDbg[119] = 0x00000000;
    pModulusDbg[120] = 0x00000000; pModulusDbg[121] = 0x00000000; pModulusDbg[122] = 0x00000000; pModulusDbg[123] = 0x00000000;
    pModulusDbg[124] = 0x00000000; pModulusDbg[125] = 0x00000000; pModulusDbg[126] = 0x00000000; pModulusDbg[127] = 0x00000000;

    pExponentDbg[0]   = 0x00010001; pExponentDbg[1]   = 0x00000000; pExponentDbg[2]   = 0x00000000; pExponentDbg[3]  = 0x00000000;
    pExponentDbg[4]   = 0x00000000; pExponentDbg[5]   = 0x00000000; pExponentDbg[6]   = 0x00000000; pExponentDbg[7]  = 0x00000000;
    pExponentDbg[8]   = 0x00000000; pExponentDbg[9]   = 0x00000000; pExponentDbg[10]  = 0x00000000; pExponentDbg[11]  = 0x00000000;
    pExponentDbg[12]  = 0x00000000; pExponentDbg[13]  = 0x00000000; pExponentDbg[14]  = 0x00000000; pExponentDbg[15]  = 0x00000000;
    pExponentDbg[16]  = 0x00000000; pExponentDbg[17]  = 0x00000000; pExponentDbg[18]  = 0x00000000; pExponentDbg[19]  = 0x00000000;
    pExponentDbg[20]  = 0x00000000; pExponentDbg[21]  = 0x00000000; pExponentDbg[22]  = 0x00000000; pExponentDbg[23]  = 0x00000000;
    pExponentDbg[24]  = 0x00000000; pExponentDbg[25]  = 0x00000000; pExponentDbg[26]  = 0x00000000; pExponentDbg[27]  = 0x00000000;
    pExponentDbg[28]  = 0x00000000; pExponentDbg[29]  = 0x00000000; pExponentDbg[30]  = 0x00000000; pExponentDbg[31]  = 0x00000000;
    pExponentDbg[32]  = 0x00000000; pExponentDbg[33]  = 0x00000000; pExponentDbg[34]  = 0x00000000; pExponentDbg[35]  = 0x00000000;
    pExponentDbg[36]  = 0x00000000; pExponentDbg[37]  = 0x00000000; pExponentDbg[38]  = 0x00000000; pExponentDbg[39]  = 0x00000000;
    pExponentDbg[40]  = 0x00000000; pExponentDbg[41]  = 0x00000000; pExponentDbg[42]  = 0x00000000; pExponentDbg[43]  = 0x00000000;
    pExponentDbg[44]  = 0x00000000; pExponentDbg[45]  = 0x00000000; pExponentDbg[46]  = 0x00000000; pExponentDbg[47]  = 0x00000000;
    pExponentDbg[48]  = 0x00000000; pExponentDbg[49]  = 0x00000000; pExponentDbg[50]  = 0x00000000; pExponentDbg[51]  = 0x00000000;
    pExponentDbg[52]  = 0x00000000; pExponentDbg[53]  = 0x00000000; pExponentDbg[54]  = 0x00000000; pExponentDbg[55]  = 0x00000000;
    pExponentDbg[56]  = 0x00000000; pExponentDbg[57]  = 0x00000000; pExponentDbg[58]  = 0x00000000; pExponentDbg[59]  = 0x00000000;
    pExponentDbg[60]  = 0x00000000; pExponentDbg[61]  = 0x00000000; pExponentDbg[62]  = 0x00000000; pExponentDbg[63]  = 0x00000000;
    pExponentDbg[64]  = 0x00000000; pExponentDbg[65]  = 0x00000000; pExponentDbg[66]  = 0x00000000; pExponentDbg[67]  = 0x00000000;
    pExponentDbg[68]  = 0x00000000; pExponentDbg[69]  = 0x00000000; pExponentDbg[70]  = 0x00000000; pExponentDbg[71]  = 0x00000000;
    pExponentDbg[72]  = 0x00000000; pExponentDbg[73]  = 0x00000000; pExponentDbg[74]  = 0x00000000; pExponentDbg[75]  = 0x00000000;
    pExponentDbg[76]  = 0x00000000; pExponentDbg[77]  = 0x00000000; pExponentDbg[78]  = 0x00000000; pExponentDbg[79]  = 0x00000000;
    pExponentDbg[80]  = 0x00000000; pExponentDbg[81]  = 0x00000000; pExponentDbg[82]  = 0x00000000; pExponentDbg[83]  = 0x00000000;
    pExponentDbg[84]  = 0x00000000; pExponentDbg[85]  = 0x00000000; pExponentDbg[86]  = 0x00000000; pExponentDbg[87]  = 0x00000000;
    pExponentDbg[88]  = 0x00000000; pExponentDbg[89]  = 0x00000000; pExponentDbg[90]  = 0x00000000; pExponentDbg[91]  = 0x00000000;
    pExponentDbg[92]  = 0x00000000; pExponentDbg[93]  = 0x00000000; pExponentDbg[94]  = 0x00000000; pExponentDbg[95]  = 0x00000000;
    pExponentDbg[96]  = 0x00000000; pExponentDbg[97]  = 0x00000000; pExponentDbg[98]  = 0x00000000; pExponentDbg[99]  = 0x00000000;
    pExponentDbg[100] = 0x00000000; pExponentDbg[101] = 0x00000000; pExponentDbg[102] = 0x00000000; pExponentDbg[103] = 0x00000000;
    pExponentDbg[104] = 0x00000000; pExponentDbg[105] = 0x00000000; pExponentDbg[106] = 0x00000000; pExponentDbg[107] = 0x00000000;
    pExponentDbg[108] = 0x00000000; pExponentDbg[109] = 0x00000000; pExponentDbg[110] = 0x00000000; pExponentDbg[111] = 0x00000000;
    pExponentDbg[112] = 0x00000000; pExponentDbg[113] = 0x00000000; pExponentDbg[114] = 0x00000000; pExponentDbg[115] = 0x00000000;
    pExponentDbg[116] = 0x00000000; pExponentDbg[117] = 0x00000000; pExponentDbg[118] = 0x00000000; pExponentDbg[119] = 0x00000000;
    pExponentDbg[120] = 0x00000000; pExponentDbg[121] = 0x00000000; pExponentDbg[122] = 0x00000000; pExponentDbg[123] = 0x00000000;
    pExponentDbg[124] = 0x00000000; pExponentDbg[125] = 0x00000000; pExponentDbg[126] = 0x00000000; pExponentDbg[127] = 0x00000000;

    pMpDbg[0]   = 0x9e0225bb; pMpDbg[1]   = 0xb73499df; pMpDbg[2]   = 0x6ff8a557; pMpDbg[3]   = 0xa008ebbe;
    pMpDbg[4]   = 0xfad37abe; pMpDbg[5]   = 0x553e5a40; pMpDbg[6]   = 0xd640088a; pMpDbg[7]   = 0x59ff459a;
    pMpDbg[8]   = 0xe1ec5eef; pMpDbg[9]   = 0x5860a6e7; pMpDbg[10]  = 0xb0bbfcc8; pMpDbg[11]  = 0x7f90a7f7;
    pMpDbg[12]  = 0x5a06dba3; pMpDbg[13]  = 0xc1ccae30; pMpDbg[14]  = 0x89d22079; pMpDbg[15]  = 0x61cf9d7a;
    pMpDbg[16]  = 0xfbb160f5; pMpDbg[17]  = 0xfb58f508; pMpDbg[18]  = 0x27e6dc64; pMpDbg[19]  = 0xa930f925;
    pMpDbg[20]  = 0x4fc1aaa2; pMpDbg[21]  = 0xe28d7f71; pMpDbg[22]  = 0xc778fe4a; pMpDbg[23]  = 0x1709668b;
    pMpDbg[24]  = 0x2d549f84; pMpDbg[25]  = 0x17828048; pMpDbg[26]  = 0xdcf03ec8; pMpDbg[27]  = 0x6f048e42;
    pMpDbg[28]  = 0xe582c272; pMpDbg[29]  = 0x24815bfd; pMpDbg[30]  = 0x4595a593; pMpDbg[31]  = 0xb0c362a6;
    pMpDbg[32]  = 0x9ac89758; pMpDbg[33]  = 0xacb00404; pMpDbg[34]  = 0x4de639ce; pMpDbg[35]  = 0x065946c9;
    pMpDbg[36]  = 0x73ce13ee; pMpDbg[37]  = 0x4a7c37e5; pMpDbg[38]  = 0x240b3952; pMpDbg[39]  = 0x3dbcbf59;
    pMpDbg[40]  = 0xd8d6c32c; pMpDbg[41]  = 0xad72e736; pMpDbg[42]  = 0x38d53615; pMpDbg[43]  = 0xc25ce7c0;
    pMpDbg[44]  = 0x6affd3a9; pMpDbg[45]  = 0x00776194; pMpDbg[46]  = 0x39bace8e; pMpDbg[47]  = 0x8ae30378;
    pMpDbg[48]  = 0xfd4b4155; pMpDbg[49]  = 0x59fa40df; pMpDbg[50]  = 0x7106134d; pMpDbg[51]  = 0xd0290f0f;
    pMpDbg[52]  = 0x1e77e75f; pMpDbg[53]  = 0xb5918ad4; pMpDbg[54]  = 0x0e2e8578; pMpDbg[55]  = 0x48c96e79;
    pMpDbg[56]  = 0x645a9703; pMpDbg[57]  = 0xa4ad05bf; pMpDbg[58]  = 0xb4e1091d; pMpDbg[59]  = 0x7fcdc0c9;
    pMpDbg[60]  = 0x5a2fe7ea; pMpDbg[61]  = 0x60770ad0; pMpDbg[62]  = 0x13162c8c; pMpDbg[63]  = 0xea602c8f;
    pMpDbg[64]  = 0x1e842a04; pMpDbg[65]  = 0x09bed660; pMpDbg[66]  = 0xfde85fa1; pMpDbg[67]  = 0xbf4fabcc;
    pMpDbg[68]  = 0x7f659cac; pMpDbg[69]  = 0x7dc7801e; pMpDbg[70]  = 0x70c40690; pMpDbg[71]  = 0x4884cbb7;
    pMpDbg[72]  = 0x21b03271; pMpDbg[73]  = 0xd8b069ab; pMpDbg[74]  = 0xee1c840b; pMpDbg[75]  = 0x25769431;
    pMpDbg[76]  = 0x998d2994; pMpDbg[77]  = 0x3f5933f3; pMpDbg[78]  = 0x111ac7df; pMpDbg[79]  = 0x3d4aca2c;
    pMpDbg[80]  = 0x6226059a; pMpDbg[81]  = 0x407914d5; pMpDbg[82]  = 0x4114fd36; pMpDbg[83]  = 0x931183ae;
    pMpDbg[84]  = 0x9f04cdcd; pMpDbg[85]  = 0x1936377e; pMpDbg[86]  = 0x572d44f4; pMpDbg[87]  = 0x9764d4d1;
    pMpDbg[88]  = 0xec729b5b; pMpDbg[89]  = 0xbb903cce; pMpDbg[90]  = 0xac72ad36; pMpDbg[91]  = 0x016cc881;
    pMpDbg[92]  = 0x306aadba; pMpDbg[93]  = 0xc58fb04a; pMpDbg[94]  = 0x9b8898f1; pMpDbg[95]  = 0x46a12ba5;
    pMpDbg[96]  = 0x00000000; pMpDbg[97]  = 0x00000000; pMpDbg[98]  = 0x00000000; pMpDbg[99]  = 0x00000000;
    pMpDbg[100] = 0x00000000; pMpDbg[101] = 0x00000000; pMpDbg[102] = 0x00000000; pMpDbg[103] = 0x00000000;
    pMpDbg[104] = 0x00000000; pMpDbg[105] = 0x00000000; pMpDbg[106] = 0x00000000; pMpDbg[107] = 0x00000000;
    pMpDbg[108] = 0x00000000; pMpDbg[109] = 0x00000000; pMpDbg[110] = 0x00000000; pMpDbg[111] = 0x00000000;
    pMpDbg[112] = 0x00000000; pMpDbg[113] = 0x00000000; pMpDbg[114] = 0x00000000; pMpDbg[115] = 0x00000000;
    pMpDbg[116] = 0x00000000; pMpDbg[117] = 0x00000000; pMpDbg[118] = 0x00000000; pMpDbg[119] = 0x00000000;
    pMpDbg[120] = 0x00000000; pMpDbg[121] = 0x00000000; pMpDbg[122] = 0x00000000; pMpDbg[123] = 0x00000000;
    pMpDbg[124] = 0x00000000; pMpDbg[125] = 0x00000000; pMpDbg[126] = 0x00000000; pMpDbg[127] = 0x00000000;

    pRsqrDbg[0]   = 0xc6508f68; pRsqrDbg[1]   = 0x987ae9a7; pRsqrDbg[2]   = 0x7eb51ee0; pRsqrDbg[3]   = 0x16e46d51;
    pRsqrDbg[4]   = 0xecea09e9; pRsqrDbg[5]   = 0x59895cba; pRsqrDbg[6]   = 0xeb92760d; pRsqrDbg[7]   = 0xab0ff839;
    pRsqrDbg[8]   = 0xff7b078e; pRsqrDbg[9]   = 0x3cca27de; pRsqrDbg[10]  = 0x59c694be; pRsqrDbg[11]  = 0xe38e795d;
    pRsqrDbg[12]  = 0x6c0a6ce2; pRsqrDbg[13]  = 0xa571a526; pRsqrDbg[14]  = 0xf97f68bc; pRsqrDbg[15]  = 0x64c6ef87;
    pRsqrDbg[16]  = 0x15eef093; pRsqrDbg[17]  = 0x3ec7525c; pRsqrDbg[18]  = 0xfc6b2a91; pRsqrDbg[19]  = 0x48a61d96;
    pRsqrDbg[20]  = 0x88dd7e9c; pRsqrDbg[21]  = 0xa6cdf464; pRsqrDbg[22]  = 0x2401e83e; pRsqrDbg[23]  = 0xb2690d86;
    pRsqrDbg[24]  = 0x96cd6ebb; pRsqrDbg[25]  = 0xb99b3821; pRsqrDbg[26]  = 0xeebc6d55; pRsqrDbg[27]  = 0xd0e2418c;
    pRsqrDbg[28]  = 0x8b5fbfe7; pRsqrDbg[29]  = 0x2a78cab9; pRsqrDbg[30]  = 0x71a4a1a0; pRsqrDbg[31]  = 0x367c2b3d;
    pRsqrDbg[32]  = 0xd5fe5b3e; pRsqrDbg[33]  = 0x134db196; pRsqrDbg[34]  = 0x5045948c; pRsqrDbg[35]  = 0x0328ca7d;
    pRsqrDbg[36]  = 0x254ec2e6; pRsqrDbg[37]  = 0x2277f32b; pRsqrDbg[38]  = 0xa76a0e9a; pRsqrDbg[39]  = 0x79b8c7c0;
    pRsqrDbg[40]  = 0xdf60dde0; pRsqrDbg[41]  = 0x6e3c80c1; pRsqrDbg[42]  = 0xf729b334; pRsqrDbg[43]  = 0x2f80a655;
    pRsqrDbg[44]  = 0x9401d3f4; pRsqrDbg[45]  = 0x4aa6a86a; pRsqrDbg[46]  = 0x1f694f50; pRsqrDbg[47]  = 0x126426d8;
    pRsqrDbg[48]  = 0xf9f8ebb8; pRsqrDbg[49]  = 0xa26de21f; pRsqrDbg[50]  = 0x46277fc7; pRsqrDbg[51]  = 0xbc805050;
    pRsqrDbg[52]  = 0xf07d0a06; pRsqrDbg[53]  = 0x33b25fcf; pRsqrDbg[54]  = 0x4ca79081; pRsqrDbg[55]  = 0x7474438e;
    pRsqrDbg[56]  = 0xa8d92aae; pRsqrDbg[57]  = 0xca962471; pRsqrDbg[58]  = 0x6251a267; pRsqrDbg[59]  = 0x11a9b153;
    pRsqrDbg[60]  = 0x920011ce; pRsqrDbg[61]  = 0xfc9fca68; pRsqrDbg[62]  = 0xa60dbe22; pRsqrDbg[63]  = 0x378f0e2d;
    pRsqrDbg[64]  = 0x50828ddb; pRsqrDbg[65]  = 0x1664b74c; pRsqrDbg[66]  = 0x5fbea6c8; pRsqrDbg[67]  = 0x51b166bf;
    pRsqrDbg[68]  = 0x1aa7a429; pRsqrDbg[69]  = 0x2e43c1d7; pRsqrDbg[70]  = 0xb17a4ca7; pRsqrDbg[71]  = 0xbe8ccf40;
    pRsqrDbg[72]  = 0x1c23c38f; pRsqrDbg[73]  = 0x1c23a492; pRsqrDbg[74]  = 0xd3cf1d42; pRsqrDbg[75]  = 0x6471c1d5;
    pRsqrDbg[76]  = 0xf8092276; pRsqrDbg[77]  = 0xa2a1246c; pRsqrDbg[78]  = 0x1f379705; pRsqrDbg[79]  = 0x25f8f305;
    pRsqrDbg[80]  = 0xeea8c2a7; pRsqrDbg[81]  = 0xfbd51fc1; pRsqrDbg[82]  = 0x89a497d9; pRsqrDbg[83]  = 0xc4d6d331;
    pRsqrDbg[84]  = 0x5e138984; pRsqrDbg[85]  = 0xf668372e; pRsqrDbg[86]  = 0x444d7e74; pRsqrDbg[87]  = 0x3ddeef60;
    pRsqrDbg[88]  = 0x30866f94; pRsqrDbg[89]  = 0xef650561; pRsqrDbg[90]  = 0xa55909c9; pRsqrDbg[91]  = 0x60e83d95;
    pRsqrDbg[92]  = 0x2c94ec95; pRsqrDbg[93]  = 0x90ca829e; pRsqrDbg[94]  = 0x64bb6c8b; pRsqrDbg[95]  = 0x828a5d9e;
    pRsqrDbg[96]  = 0x00000000; pRsqrDbg[97]  = 0x00000000; pRsqrDbg[98]  = 0x00000000; pRsqrDbg[99]  = 0x00000000;
    pRsqrDbg[100] = 0x00000000; pRsqrDbg[101] = 0x00000000; pRsqrDbg[102] = 0x00000000; pRsqrDbg[103] = 0x00000000;
    pRsqrDbg[104] = 0x00000000; pRsqrDbg[105] = 0x00000000; pRsqrDbg[106] = 0x00000000; pRsqrDbg[107] = 0x00000000;
    pRsqrDbg[108] = 0x00000000; pRsqrDbg[109] = 0x00000000; pRsqrDbg[110] = 0x00000000; pRsqrDbg[111] = 0x00000000;
    pRsqrDbg[112] = 0x00000000; pRsqrDbg[113] = 0x00000000; pRsqrDbg[114] = 0x00000000; pRsqrDbg[115] = 0x00000000;
    pRsqrDbg[116] = 0x00000000; pRsqrDbg[117] = 0x00000000; pRsqrDbg[118] = 0x00000000; pRsqrDbg[119] = 0x00000000;
    pRsqrDbg[120] = 0x00000000; pRsqrDbg[121] = 0x00000000; pRsqrDbg[122] = 0x00000000; pRsqrDbg[123] = 0x00000000;
    pRsqrDbg[124] = 0x00000000; pRsqrDbg[125] = 0x00000000; pRsqrDbg[126] = 0x00000000; pRsqrDbg[127] = 0x00000000;

    return BOOTER_OK;
}
#endif
