/*
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   se_segp10x.c
 * @brief  SE HAL Functions for GP10X
 *
 * SE HAL functions to implement chip-specific initialization, helper functions
 */
#include "lwuproc.h"
#include "setypes.h"
#include "config/se-config.h"

#include "se_objse.h"
#include "secbus_ls.h"
#include "secbus_hs.h"
#include "se_private.h"
#include "seapi.h"
#include "config/g_se_hal.h"
#include "dev_se_seb.h"
#if ((SECFG_CHIP_ENABLED(GP10X)) || (SECFG_CHIP_ENABLED(GV10X)))
#include "dev_se_addendum.h"
#endif

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
inline static SE_STATUS  _seColwertPKAStatusToSEStatus(LwU32 value)
    GCC_ATTRIB_ALWAYSINLINE();
inline static SE_STATUS _sePkaSetRadix_GP10X(SE_KEY_SIZE numBits, PSE_PKA_REG pPkaReg)
    GCC_ATTRIB_ALWAYSINLINE();
/* ------------------------- Implementation --------------------------------- */
/* ------------------------- Private Functions------------------------------- */
/*!
 * @brief Determines the radix mask to be used in  LW_SSE_SE_PKA_CONTROL register
 *
 * @param[in] numBits    Radix or the size of the operation (key length)
 * @param[in] pPkaReg    This structure is filled up based on numBits
 *
 * @return status SE_ERR_ILWALID_KEY_SIZE if an improper key length is passed.
 *                SE_OK for proper key sizes.  Also stuct is initilized that contains
 *                sizes and values needed to address operand memory.
 */
inline static SE_STATUS _sePkaSetRadix_GP10X
(
    SE_KEY_SIZE numBits,
    PSE_PKA_REG pPkaReg
)
{
    SE_STATUS status  = SE_OK;
    LwU32     minBits = SE_KEY_SIZE_256;

    // Check input data
    if (pPkaReg == NULL)
    {
        return SE_ERR_BAD_INPUT_DATA;
    }

    while (minBits < numBits)
    {
        minBits <<= 1;
    }
    pPkaReg->pkaKeySizeDW = minBits >> 5;

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
            pPkaReg->pkaRadixMask = DRF_DEF(_SSE, _SE_PKA_CONTROL, _CTRL_BASE_RADIX, _256) |
                                    DRF_VAL(_SSE, _SE_PKA_CONTROL, _CTRL_PARTIAL_RADIX, numBits >> 5);
            break;
        case SE_KEY_SIZE_256:
            pPkaReg->pkaRadixMask = DRF_DEF(_SSE, _SE_PKA_CONTROL, _CTRL_BASE_RADIX, _256) |
                                    DRF_VAL(_SSE, _SE_PKA_CONTROL, _CTRL_PARTIAL_RADIX, 0);
            break;
        case SE_KEY_SIZE_384:
            pPkaReg->pkaRadixMask = DRF_DEF(_SSE, _SE_PKA_CONTROL, _CTRL_BASE_RADIX, _512) |
                                    DRF_VAL(_SSE, _SE_PKA_CONTROL, _CTRL_PARTIAL_RADIX, numBits >> 5);
            break;
        case SE_KEY_SIZE_512:
            pPkaReg->pkaRadixMask = DRF_DEF(_SSE, _SE_PKA_CONTROL, _CTRL_BASE_RADIX, _512) |
                                    DRF_VAL(_SSE, _SE_PKA_CONTROL, _CTRL_PARTIAL_RADIX, 0);
            break;
        case SE_KEY_SIZE_768:
            pPkaReg->pkaRadixMask = DRF_DEF(_SSE, _SE_PKA_CONTROL, _CTRL_BASE_RADIX, _1024) |
                                    DRF_VAL(_SSE, _SE_PKA_CONTROL, _CTRL_PARTIAL_RADIX, numBits >> 5);
            break;
        case SE_KEY_SIZE_1024:
            pPkaReg->pkaRadixMask = DRF_DEF(_SSE, _SE_PKA_CONTROL, _CTRL_BASE_RADIX, _1024) |
                                    DRF_VAL(_SSE, _SE_PKA_CONTROL, _CTRL_PARTIAL_RADIX, 0);
            break;
        case SE_KEY_SIZE_1536:
            pPkaReg->pkaRadixMask = DRF_DEF(_SSE, _SE_PKA_CONTROL, _CTRL_BASE_RADIX, _2048) |
                                    DRF_VAL(_SSE, _SE_PKA_CONTROL, _CTRL_PARTIAL_RADIX, numBits >> 5);
            break;
        case SE_KEY_SIZE_2048:
            pPkaReg->pkaRadixMask = DRF_DEF(_SSE, _SE_PKA_CONTROL, _CTRL_BASE_RADIX, _2048) |
                                    DRF_VAL(_SSE, _SE_PKA_CONTROL, _CTRL_PARTIAL_RADIX, 0);
            break;
        case SE_KEY_SIZE_3072:
            pPkaReg->pkaRadixMask = DRF_DEF(_SSE, _SE_PKA_CONTROL, _CTRL_BASE_RADIX, _4096) |
                                    DRF_VAL(_SSE, _SE_PKA_CONTROL, _CTRL_PARTIAL_RADIX, numBits >> 5);
            break;
        case SE_KEY_SIZE_4096:
            pPkaReg->pkaRadixMask = DRF_DEF(_SSE, _SE_PKA_CONTROL, _CTRL_BASE_RADIX, _4096) |
                                    DRF_VAL(_SSE, _SE_PKA_CONTROL, _CTRL_PARTIAL_RADIX, 0);

            break;
        default:
            status = SE_ERR_ILWALID_KEY_SIZE;
            break;
    }
    return status;
}

/* ------------------------- Public Functions-------------------------------- */
/*!
 * @brief Function to initialize SE for PKA operations
 *
 * @param[in] bitCount       Radix or the size of the operation
 * @param[in/out] pPkaReg    Struct containing the number of words based
 *                           on the radix
 * @return status
 */
SE_STATUS seInit_GP10X
(
    SE_KEY_SIZE bitCount,
    PSE_PKA_REG pPkaReg
)
{
    SE_STATUS status        = SE_OK;
    LwU32     sccJumpRandom = 0;

    //
    // Init is to be called before doing any SE operation, so there is no need to
    // read/modify/write these registers as they are being inited for subsequent operation.
    // Any writes after initial initialization will need to be read/modify/write.
    //
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChk(LW_SSE_SE_CTRL_CONTROL,
                                                   DRF_DEF(_SSE, _SE_CTRL_CONTROL, _AUTORESEED,    _ENABLE)  |
                                                   DRF_DEF(_SSE, _SE_CTRL_CONTROL, _LOAD_KEY,      _DISABLE) |
                                                   DRF_DEF(_SSE, _SE_CTRL_CONTROL, _SCRUB_PKA_MEM, _DISABLE) |
                                                   DRF_NUM(_SSE, _SE_CTRL_CONTROL, _KEYSLOT,       0)));

    //
    // Check if pPkaReg is not NULL.  Some operations like random
    // number generator don't need to set this up.
    //
    if (pPkaReg)
    {
        CHECK_SE_STATUS(sePkaSetRadix_HAL(&Se, bitCount, pPkaReg));
    }

    //
    // From IAS:
    // On emulation, TRNG couldn't work properly, so SW must set SCC Control Register.DPA_DISABLE = 1 to make PKA work properly.
    // https://p4viewer/get/p4hw:2001//hw/ar/doc/t18x/se/SE_PKA_IAS.doc
    //
    // Enable Side Channel Counter measures.
    //

    //
    // True Random Number Generator must be enabled to use SCC.  Enable TRNG and request a random number
    // to use with the Jump Probability Register.
    //
    CHECK_SE_STATUS(seTrueRandomGetNumber_HAL(&Se, &sccJumpRandom, sizeof(sccJumpRandom) / sizeof(LwU32)));
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChk(LW_SSE_SE_PKA_JUMP_PROBABILITY,
                                                   DRF_NUM(_SSE, _SE_PKA_JUMP_PROBABILITY, _JUMP_PROBABILITY,
                                                          (sccJumpRandom & LW_SSE_SE_PKA_JUMP_PROBABILITY_JUMP_PROBABILITY_MAX))));
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChk(LW_SSE_SE_CTRL_SCC_CONTROL,
                                                   DRF_DEF(_SSE, _SE_CTRL_SCC_CONTROL_DPA, _DISABLE, _UNSET)));

ErrorExit:
    return status;
}

/*!
 * @brief Function to initialize SE for PKA operations in HS Mode
 *
 * @param[in] bitCount       Radix or the size of the operation
 * @param[in/out] pPkaReg    Struct containing the number of words based
 *                           on the radix
 * @return status
 */
SE_STATUS seInitHs_GP10X
(
    SE_KEY_SIZE bitCount,
    PSE_PKA_REG pPkaReg
)
{
    SE_STATUS status        = SE_OK;
    LwU32     sccJumpRandom = 0;

    //
    // Init is to be called before doing any SE operation, so there is no need to
    // read/modify/write these registers as they are being inited for subsequent operation.
    // Any writes after initial initialization will need to be read/modify/write.
    //
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChkHs(LW_SSE_SE_CTRL_CONTROL,
                                                   DRF_DEF(_SSE, _SE_CTRL_CONTROL, _AUTORESEED,    _ENABLE)  |
                                                   DRF_DEF(_SSE, _SE_CTRL_CONTROL, _LOAD_KEY,      _DISABLE) |
                                                   DRF_DEF(_SSE, _SE_CTRL_CONTROL, _SCRUB_PKA_MEM, _DISABLE) |
                                                   DRF_NUM(_SSE, _SE_CTRL_CONTROL, _KEYSLOT,       0)));

    //
    // Check if pPkaReg is not NULL.  Some operations like random
    // number generator don't need to set this up.
    //
    if (pPkaReg)
    {
        CHECK_SE_STATUS(sePkaSetRadixHs_HAL(&Se, bitCount, pPkaReg));
    }

    //
    // From IAS:
    // On emulation, TRNG couldnt work properly, so SW must set SCC Control Register.DPA_DISABLE = 1 to make PKA work properly.
    // https://p4viewer/get/p4hw:2001//hw/ar/doc/t18x/se/SE_PKA_IAS.doc
    //
    // Enable Side Channel Counter measures.
    //

    //
    // True Random Number Generator must be enabled to use SCC.  Enable TRNG and request a random number
    // to use with the Jump Probability Register.
    //
    CHECK_SE_STATUS(seTrueRandomGetNumberHs_HAL(&Se, &sccJumpRandom, sizeof(sccJumpRandom) / sizeof(LwU32)));
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChkHs(LW_SSE_SE_PKA_JUMP_PROBABILITY,
                                                   DRF_NUM(_SSE, _SE_PKA_JUMP_PROBABILITY, _JUMP_PROBABILITY,
                                                          (sccJumpRandom & LW_SSE_SE_PKA_JUMP_PROBABILITY_JUMP_PROBABILITY_MAX))));
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChkHs(LW_SSE_SE_CTRL_SCC_CONTROL,
                                                   DRF_DEF(_SSE, _SE_CTRL_SCC_CONTROL_DPA, _DISABLE, _UNSET)));

ErrorExit:
    return status;
}

/*!
 * @brief Checks the opcode return register fields and return status
 *
 * @param[in] value       Value of LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON
 *
 * @return status returns whether PKA operation successful or not
 */
inline static SE_STATUS  _seColwertPKAStatusToSEStatus(LwU32 value)
{
    SE_STATUS status = SE_OK;

    switch (value)
    {
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_NORMAL:
            status = SE_OK;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_ILWALID_OPCODE:
            status = SE_ERR_PKA_OP_ILWALID_OPCODE;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_F_STACK_UNDERFLOW:
            status = SE_ERR_PKA_OP_F_STACK_UNDERFLOW;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_F_STACK_OVERFLOW:
            status = SE_ERR_PKA_OP_F_STACK_OVERFLOW;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_WATCHDOG:
            status = SE_ERR_PKA_OP_WATCHDOG;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_HOST_REQUEST:
            status = SE_ERR_PKA_OP_HOST_REQUEST;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_P_STACK_UNDERFLOW:
            status = SE_ERR_PKA_OP_P_STACK_UNDERFLOW;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_P_STACK_OVERFLOW:
            status = SE_ERR_PKA_OP_P_STACK_OVERFLOW;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_MEM_PORT_COLLISION:
            status = SE_ERR_PKA_OP_MEM_PORT_COLLISION;
            break;
        case LW_SSE_SE_PKA_RETURN_CODE_RC_STOP_REASON_OPERATION_SIZE_EXCEED_CFG:
            status = SE_ERR_PKA_OP_OPERATION_SIZE_EXCEED_CFG;
            break;
        default:
            status = SE_ERR_PKA_OP_UNKNOWN_ERR;
            break;
    }

    return status;
}

/*!
 * @brief    Triggers the PKA operation
 *
 * @param[in] pkaOp       The PKA operation to be done
 * @param[in] radixMask   The mask which is dependent on the size of the operation
 *
 * @return status returns whether writes were successful or not
 */
SE_STATUS seStartPkaOperationAndPoll_GP10X
(
    LwU32  pkaOp,
    LwU32  radixMask
)
{
    SE_STATUS status = SE_OK;
    LwU32     value;

    // Clear residual flags
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChk(LW_SSE_SE_PKA_FLAGS,
                                                   DRF_DEF(_SSE, _SE_PKA_FLAGS, _FLAG_ZERO,   _UNSET) |
                                                   DRF_DEF(_SSE, _SE_PKA_FLAGS, _FLAG_MEMBIT, _UNSET) |
                                                   DRF_DEF(_SSE, _SE_PKA_FLAGS, _FLAG_BORROW, _UNSET) |
                                                   DRF_DEF(_SSE, _SE_PKA_FLAGS, _FLAG_CARRY,  _UNSET) |
                                                   DRF_DEF(_SSE, _SE_PKA_FLAGS, _FLAG_F0,     _UNSET) |
                                                   DRF_DEF(_SSE, _SE_PKA_FLAGS, _FLAG_F1,     _UNSET) |
                                                   DRF_DEF(_SSE, _SE_PKA_FLAGS, _FLAG_F2,     _UNSET) |
                                                   DRF_DEF(_SSE, _SE_PKA_FLAGS, _FLAG_F3,     _UNSET)));

    // clear F-Stack:
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChk(LW_SSE_SE_PKA_FUNCTION_STACK_POINT,
                                                   DRF_DEF(_SSE, _SE_PKA_FUNCTION_STACK_POINT, _FSTACK_POINTER, _CLEAR)));

    // Write the program entry point
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChk(LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS,
                                                   DRF_NUM(_SSE, _SE_PKA_PROGRAM_ENTRY_ADDRESS, _PROG_ADDR, pkaOp)));

    // enable interrupts
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChk(LW_SSE_SE_PKA_INTERRUPT_ENABLE,
                                                   DRF_DEF(_SSE, _SE_PKA_INTERRUPT_ENABLE, _IE_IRQ_EN, _SET)));

    //
    // Start the PKA with assigned mode:
    //
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChk(LW_SSE_SE_PKA_CONTROL,
                                                   DRF_DEF(_SSE, _SE_PKA_CONTROL, _CTRL_GO, _ENABLE) |
                                                   radixMask));

    //
    // Poll the status register until PKA operation is complete. This is
    // indicated by STAT_IRQ bit in PKA_STATUS.  Once set, this bit will
    // remain set until cleared by writing to it.
    //
    do
    {
        CHECK_SE_STATUS(seSelwreBusReadRegisterErrChk(LW_SSE_SE_PKA_STATUS, &value));
    }
    while (FLD_TEST_DRF(_SSE, _SE_PKA_STATUS, _STAT_IRQ, _UNSET, value));

    // Clear the PKA op completed bit  (see above)
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChk(LW_SSE_SE_PKA_STATUS,
                                                   DRF_DEF(_SSE, _SE_PKA_STATUS, _STAT_IRQ, _SET)));

    // Colwert PKA status return code to SE_STATUS and return
    CHECK_SE_STATUS(seSelwreBusReadRegisterErrChk(LW_SSE_SE_PKA_RETURN_CODE, &value));
    value = DRF_VAL(_SSE, _SE_PKA_RETURN_CODE, _RC_STOP_REASON, value);
    status = _seColwertPKAStatusToSEStatus(value);

ErrorExit:
    return status;
}

/*!
 * @brief    Triggers the PKA operation in HS Mode
 *
 * @param[in] pkaOp       The PKA operation to be done
 * @param[in] radixMask   The mask which is dependent on the size of the operation
 *
 * @return status returns whether writes were successful or not
 */
SE_STATUS seStartPkaOperationAndPollHs_GP10X
(
    LwU32  pkaOp,
    LwU32  radixMask
)
{
    SE_STATUS status = SE_OK;
    LwU32     value;

    // Clear residual flags
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChkHs(LW_SSE_SE_PKA_FLAGS,
                                                   DRF_DEF(_SSE, _SE_PKA_FLAGS, _FLAG_ZERO,   _UNSET) |
                                                   DRF_DEF(_SSE, _SE_PKA_FLAGS, _FLAG_MEMBIT, _UNSET) |
                                                   DRF_DEF(_SSE, _SE_PKA_FLAGS, _FLAG_BORROW, _UNSET) |
                                                   DRF_DEF(_SSE, _SE_PKA_FLAGS, _FLAG_CARRY,  _UNSET) |
                                                   DRF_DEF(_SSE, _SE_PKA_FLAGS, _FLAG_F0,     _UNSET) |
                                                   DRF_DEF(_SSE, _SE_PKA_FLAGS, _FLAG_F1,     _UNSET) |
                                                   DRF_DEF(_SSE, _SE_PKA_FLAGS, _FLAG_F2,     _UNSET) |
                                                   DRF_DEF(_SSE, _SE_PKA_FLAGS, _FLAG_F3,     _UNSET)));

    // clear F-Stack:
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChkHs(LW_SSE_SE_PKA_FUNCTION_STACK_POINT,
                                                   DRF_DEF(_SSE, _SE_PKA_FUNCTION_STACK_POINT, _FSTACK_POINTER, _CLEAR)));

    // Write the program entry point
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChkHs(LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS,
                                                   DRF_NUM(_SSE, _SE_PKA_PROGRAM_ENTRY_ADDRESS, _PROG_ADDR, pkaOp)));

    // enable interrupts
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChkHs(LW_SSE_SE_PKA_INTERRUPT_ENABLE,
                                                   DRF_DEF(_SSE, _SE_PKA_INTERRUPT_ENABLE, _IE_IRQ_EN, _SET)));

    //
    // Start the PKA with assigned mode:
    //
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChkHs(LW_SSE_SE_PKA_CONTROL,
                                                   DRF_DEF(_SSE, _SE_PKA_CONTROL, _CTRL_GO, _ENABLE) |
                                                   radixMask));

    //
    // Poll the status register until PKA operation is complete. This is
    // indicated by STAT_IRQ bit in PKA_STATUS.  Once set, this bit will
    // remain set until cleared by writing to it.
    //
    do
    {
        CHECK_SE_STATUS(seSelwreBusReadRegisterErrChkHs(LW_SSE_SE_PKA_STATUS, &value));
    }
    while (FLD_TEST_DRF(_SSE, _SE_PKA_STATUS, _STAT_IRQ, _UNSET, value));

    // Clear the PKA op completed bit  (see above)
    CHECK_SE_STATUS(seSelwreBusWriteRegisterErrChkHs(LW_SSE_SE_PKA_STATUS,
                                                   DRF_DEF(_SSE, _SE_PKA_STATUS, _STAT_IRQ, _SET)));

    // Colwert PKA status return code to SE_STATUS and return
    CHECK_SE_STATUS(seSelwreBusReadRegisterErrChkHs(LW_SSE_SE_PKA_RETURN_CODE, &value));
    value = DRF_VAL(_SSE, _SE_PKA_RETURN_CODE, _RC_STOP_REASON, value);
    status = _seColwertPKAStatusToSEStatus(value);

ErrorExit:
    return status;
}

/*!
 * @brief Determines the radix mask to be used in  LW_SSE_SE_PKA_CONTROL register
 *
 * @param[in] numBits    Radix or the size of the operation (key length)
 * @param[in] pPkaReg    This structure is filled up based on numBits
 *
 * @return status SE_ERR_ILWALID_KEY_SIZE if an improper key length is passed.
 *                SE_OK for proper key sizes.  Also stuct is initilized that contains
 *                sizes and values needed to address operand memory.
 */
SE_STATUS sePkaSetRadix_GP10X
(
    SE_KEY_SIZE numBits,
    PSE_PKA_REG pPkaReg
)
{
    return _sePkaSetRadix_GP10X(numBits, pPkaReg);
}

/*!
 * @brief Determines the radix mask to be used in  LW_SSE_SE_PKA_CONTROL register
 *
 * @param[in] numBits    Radix or the size of the operation (key length)
 * @param[in] pPkaReg    This structure is filled up based on numBits
 *
 * @return status SE_ERR_ILWALID_KEY_SIZE if an improper key length is passed.
 *                SE_OK for proper key sizes.  Also stuct is initilized that contains
 *                sizes and values needed to address operand memory.
 */
SE_STATUS sePkaSetRadixHs_GP10X
(
    SE_KEY_SIZE numBits,
    PSE_PKA_REG pPkaReg
)
{
    return _sePkaSetRadix_GP10X(numBits, pPkaReg);
}

//
// From PKA User Guide
// The operand memories are dynamically divided into a series of logical operand registers based on the size
// (radix) of the operation being performed. The naming convention for these registers is that of the memory
// block([A - D]) followed by a digit([0 - 7]).The digit indicates the distance away from the base address
// of that memory block in units of integral power of 2 radix bits([256, 512, 1024, 2048, 4096]).
// For example register C6 for a 512 bit radix with base offset 0xc00  would start at 0xD80.
//
//  C1  = 0xc00 + (1 * 4 * 16(# 32 bit words in 512) = 0xc00 + 0x040 (64) = 0xc40
//  C2  = 0xc00 + (2 * 4 * 16(# 32 bit words in 512) = 0xc00 + 0x080(128) = 0xc80
//  C3  = 0xc00 + (3 * 4 * 16(# 32 bit words in 512) = 0xc00 + 0x0c0(192) = 0xcc0
//  C4  = 0xc00 + (4 * 4 * 16(# 32 bit words in 512) = 0xc00 + 0x100(256) = 0xd00
//  C5  = 0xc00 + (5 * 4 * 16(# 32 bit words in 512) = 0xc00 + 0x140(320) = 0xd40
//  C6  = 0xc00 + (6 * 4 * 16(# 32 bit words in 512) = 0xc00 + 0x180(384) = 0xd80
//  C7  = 0xc00 + (7 * 4 * 16(# 32 bit words in 512) = 0xc00 + 0x1c0(448) = 0xdc0
//
// If an operation is set for a radix size that is not an integral power of 2 (ie: not 256, 512, 1024,
// 2048, etc.), then these are termed partial radix sizes. For these partial radix operations, the logical register
// address map will use the next integral power of 2 higher. For example, a 384 bit partial radix operation
// will use the 512 bit register map and a 224 bit operation will use the 256 bit map. For these partial radix
// parameters, the value will be loaded justified to the lower address of the defined register area. For example,
// a 160 bit partial radix operand loaded into C3 will be written to + 0xC60 to + 0xC70, inclusive. Any data
// written to + 0x0C74 through + 0x0C7C and + 0x1078 through + 0x107C be ignored by the PKA arithmetic
// engine unless specifically stated.
//

/*!
 * @brief Determines the starting address of the register in the
 *        PKA operand memory based on the size of the operation and
 *        the bank ID.
 *
 * @param[in]  bankId        Which bank address to return?
 * @param[out] pBankAddress  Bank address to return
 * @param[in]  idx           Index of the register
 * @param[in]  pPkaReg       Struct containing the number of words based
 *                           on the radix
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 */
SE_STATUS seGetPkaOperandBankAddress_GP10X
(
    SE_PKA_BANK bankId,
    LwU32 *pBankAddr,
    LwU32 idx,
    LwU32 pkaKeySizeDW
)
{
    SE_STATUS status = SE_OK;
    LwU32     bankOffset;

    CHECK_SE_STATUS(seGetSpecificPkaOperandBankAddressOffset_HAL(&Se, bankId, &bankOffset));
    *pBankAddr = bankOffset + (LwU32)(idx * 4) * pkaKeySizeDW;

ErrorExit:
    return status;
}

/*!
 * @brief Determines the starting address of the register in the
 *        PKA operand memory based on the size of the operation and
 *        the bank ID in HS Mode.
 *
 * @param[in]  bankId        Which bank address to return?
 * @param[out] pBankAddress  Bank address to return
 * @param[in]  idx           Index of the register
 * @param[in]  pPkaReg       Struct containing the number of words based
 *                           on the radix
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 */
SE_STATUS seGetPkaOperandBankAddressHs_GP10X
(
    SE_PKA_BANK bankId,
    LwU32 *pBankAddr,
    LwU32 idx,
    LwU32 pkaKeySizeDW
)
{
    SE_STATUS status = SE_OK;
    LwU32     bankOffset;

    CHECK_SE_STATUS(seGetSpecificPkaOperandBankAddressOffsetHs_HAL(&Se, bankId, &bankOffset));
    *pBankAddr = bankOffset + (LwU32)seMulu32Hs((idx << 2), pkaKeySizeDW);

ErrorExit:
    return status;
}


/*!
 * @brief Returns the main bank address offset for operand memory
 *
 */
LwU32 seGetPkaOperandBankAddressOffset_GP10X(void)
{
    return LW_SSE_SE_PKA_ADDR_OFFSET;
}

/*!
 * @brief Returns the main bank address offset for operand memory in HS Mode
 *
 */
LwU32 seGetPkaOperandBankAddressOffsetHs_GP10X(void)
{
    return LW_SSE_SE_PKA_ADDR_OFFSET;
}

/*!
 * @brief Returns specific bank address offset based on input
 *
 * @param[in]  bankId       Which bank address to return?
 * @param[out] pBankOffset  Bank offset to return
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 */
SE_STATUS seGetSpecificPkaOperandBankAddressOffset_GP10X
(
    SE_PKA_BANK bankId,
    LwU32      *pBankOffset
)
{
    SE_STATUS status = SE_OK;
    switch (bankId)
    {
        case SE_PKA_BANK_A:
            *pBankOffset = seGetPkaOperandBankAddressOffset_HAL() + LW_SSE_SE_PKA_BANK_START_A_OFFSET;
            break;
        case SE_PKA_BANK_B:
            *pBankOffset = seGetPkaOperandBankAddressOffset_HAL() + LW_SSE_SE_PKA_BANK_START_B_OFFSET;
            break;
        case SE_PKA_BANK_C:
            *pBankOffset = seGetPkaOperandBankAddressOffset_HAL() + LW_SSE_SE_PKA_BANK_START_C_OFFSET;
            break;
        case SE_PKA_BANK_D:
            *pBankOffset = seGetPkaOperandBankAddressOffset_HAL() + LW_SSE_SE_PKA_BANK_START_D_OFFSET;
            break;
        default:
            status = SE_ERR_ILWALID_ARGUMENT;
            break;
    }

    return status;
}

/*!
 * @brief Returns specific bank address offset based on input in HS Mode
 *
 * @param[in]  bankId       Which bank address to return?
 * @param[out] pBankOffset  Bank offset to return
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 */
SE_STATUS seGetSpecificPkaOperandBankAddressOffsetHs_GP10X
(
    SE_PKA_BANK bankId,
    LwU32      *pBankOffset
)
{
    SE_STATUS status = SE_OK;
    switch (bankId)
    {
        case SE_PKA_BANK_A:
            *pBankOffset = seGetPkaOperandBankAddressOffsetHs_HAL() + LW_SSE_SE_PKA_BANK_START_A_OFFSET;
            break;
        case SE_PKA_BANK_B:
            *pBankOffset = seGetPkaOperandBankAddressOffsetHs_HAL() + LW_SSE_SE_PKA_BANK_START_B_OFFSET;
            break;
        case SE_PKA_BANK_C:
            *pBankOffset = seGetPkaOperandBankAddressOffsetHs_HAL() + LW_SSE_SE_PKA_BANK_START_C_OFFSET;
            break;
        case SE_PKA_BANK_D:
            *pBankOffset = seGetPkaOperandBankAddressOffsetHs_HAL() + LW_SSE_SE_PKA_BANK_START_D_OFFSET;
            break;
        default:
            status = SE_ERR_ILWALID_ARGUMENT;
            break;
    }

    return status;
}

