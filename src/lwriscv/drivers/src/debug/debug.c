/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    debug.c
 * @brief   Debug message buffer driver
 *
 * Handles debug print buffer that can be read by lwwatch / RM.
 *
 * Provides kernel-level printf and task interface.
 */

/* ------------------------ System Includes -------------------------------- */
#include <limits.h>
#include <stdarg.h>

/* ------------------------ LW Includes ------------------------------------ */
#include <flcnretval.h>
#include <lwtypes.h>
#include <riscvifriscv.h>
#include <osptimer.h>

/* ------------------------ Register Includes ------------------------------ */
#include <riscv_csr.h>
#include <memops.h>

/* ------------------------ SafeRTOS Includes ------------------------------ */
#include <lwrtos.h>
#include <shared.h>
#include <sections.h>
#include <engine.h>

/* ------------------------ RISC-V system library  -------------------------- */
#include <lwriscv/print.h>
#include <shlib/syscall.h>
#include <shlib/string.h>

/* ------------------------ Module Includes -------------------------------- */
#include <drivers/drivers.h>


ct_assert(RM_RISCV_DEBUG_BUFFER_QUEUE <= ENGINE_REG(_QUEUE_TAIL__SIZE_1));

// MMINTS-TODO: 500 ms for now, re-evalute timeout to a more reasonable value later.
#define LW_RISCV_DEBUG_BUFFER_WAIT_TIMEOUT_MS (500U)
#define LW_RISCV_DEBUG_BUFFER_WAIT_TIMEOUT_US (LW_RISCV_DEBUG_BUFFER_WAIT_TIMEOUT_MS * 1000U)


// The upper-bound on the theoretically possible token length
#define LW_RISCV_RAW_MODE_PRINT_MAX_TOKENS_LIMIT    LW_U8_MAX

//------------------------------------------------------------------------------
// External symbols
//------------------------------------------------------------------------------

extern LwU8 _dmesg_buffer_start[];
extern LwU8 _dmesg_buffer_size[];
static volatile const LwUPtr debugBufferSize = (LwUPtr)_dmesg_buffer_size;

//------------------------------------------------------------------------------
// Types
//------------------------------------------------------------------------------

/*
 * MK TODO: it should be possible to seamlessly disable buffer.
 * https://jirasw.lwpu.com/browse/RMT10X-2314
 */
/*!
 * Structure containing information for shared debug ring buffer between
 * client and GSP firmware RM.
 */
typedef struct LW_DEBUG_BUFFER
{
    /* shared data */
    volatile LwU8 *pBufferData;

    LW_RISCV_DEBUG_BUFFER *pMetadata;
} LW_DEBUG_BUFFER;

//------------------------------------------------------------------------------
// Local variables
//------------------------------------------------------------------------------

//
// For now buffer is disable until it's initialized. Later we may permit
// disabling it on demand.
//
sysKERNEL_DATA static LwBool bBufferEnabled = LW_FALSE;

//
// Local copy of buffer metadata - we don't trust buffer shared with CPU.
//
sysKERNEL_DATA static LW_DEBUG_BUFFER debugBuffer;

//------------------------------------------------------------------------------
// Code
//------------------------------------------------------------------------------

sysKERNEL_CODE static LwBool
debugBufferIsFull(void)
{
    LwU32 bufferSize = DRF_VAL(_RISCV, _DEBUG_BUFFER_CONFIG, _SIZE, debugBuffer.pMetadata->bufferConfig);
    LwU32 wo;

    wo = debugBuffer.pMetadata->writeOffset + 1;
    wo %= bufferSize;

    // re-read register if we think we ran out of buffer space
    if (debugBuffer.pMetadata->readOffset == wo)
    {
        LwU32 lwrrReadOffset = csbRead(ENGINE_REG(_QUEUE_TAIL(RM_RISCV_DEBUG_BUFFER_QUEUE)));

        if (lwrrReadOffset == LW_RISCV_DEBUG_BUFFER_QUEUE_OFFS_MODE_SWITCH_ACK)
        {
            //
            // RM still hasn't sent out the first valid post-switch read offset!
            // Consider the debug buffer full.
            //
            return LW_TRUE;
        }

        debugBuffer.pMetadata->readOffset = lwrrReadOffset;

        debugBuffer.pMetadata->readOffset %= bufferSize;
    }

    return (debugBuffer.pMetadata->readOffset == wo);
}

sysKERNEL_CODE static LwBool
debugBufferIsEmpty(void *pArgs)
{
    (void)pArgs;

    // Always read the register to update information from RM
    debugBuffer.pMetadata->readOffset = csbRead(ENGINE_REG(_QUEUE_TAIL(RM_RISCV_DEBUG_BUFFER_QUEUE)));

    // RM has caught up to our writes once readOffset == writeOffset
    return (debugBuffer.pMetadata->readOffset == debugBuffer.pMetadata->writeOffset);
}

sysKERNEL_CODE static LwBool
debugBufferIsNotFull(void *pArgs)
{
    (void)pArgs;
    return !debugBufferIsFull();
}

sysKERNEL_CODE static void
debugLogsFlush(LwU32 writeOffset)
{
    //
    // Ensure logs writes complete, increment corresponding offset and
    // send notification (interrupt) to RM.
    //
    lwFenceAll();

    csbWrite(ENGINE_REG(_QUEUE_HEAD(RM_RISCV_DEBUG_BUFFER_QUEUE)),
        writeOffset);

    irqFireSwGen(SYS_INTR_SWGEN1);
}

sysKERNEL_CODE static void
debugPutByteKernel(LwU8 byte)
{
    if (bBufferEnabled)
    {
        LwU32 bufferSize = DRF_VAL(_RISCV, _DEBUG_BUFFER_CONFIG, _SIZE, debugBuffer.pMetadata->bufferConfig);

        // If buffer is full, notify RM and stall ucode
        if (debugBufferIsFull())
        {
            debugLogsFlush(debugBuffer.pMetadata->writeOffset);

            // Wait until debug buffer is no longer full
            if (!OS_PTIMER_SPIN_WAIT_US_COND(debugBufferIsNotFull,
                    NULL, LW_RISCV_DEBUG_BUFFER_WAIT_TIMEOUT_US))
            {
                //
                // MMINTS-TODO: consider reducing the timeout, and then
                // just disable the debug buffer here until the next print req.
                //
                appHalt();
            }
        }

        debugBuffer.pBufferData[debugBuffer.pMetadata->writeOffset] = byte;

        debugBuffer.pMetadata->writeOffset++;
        debugBuffer.pMetadata->writeOffset %= bufferSize;
    }
}

sysKERNEL_CODE static void
debugPutDwordKernel(LwUPtr dword)
{
    if (LWRISCV_PRINT_RAW_MODE && bBufferEnabled)
    {
        LwU8 *dwordBytes = (LwU8*)&dword;
        LwU8 numBytes = sizeof(LwUPtr) / sizeof(LwU8);

        for (LwU32 i = 0; i < numBytes; i++)
        {
            debugPutByteKernel(dwordBytes[i]);
        }
    }
}

sysKERNEL_CODE static void
debugPutchar(int ch, void *pArg)
{
    (void)pArg;

    // Replace newline with null char for ease of displaying debug logs
    if (ch == '\n')
    {
        ch = '\0';
    }

    debugPutByteKernel(ch);
}

/*!
 * This function is to be called by code that wants to wait until the debug buffer
 * queue is empty.
 * Does a busy wait. Calls appHalt on timeout (we don't want to allow custom
 * error handling here because if we error out, prints are probably in a bad state,
 * and code using this function might want to do something like a crash dump on error,
 * so we just crash without a crash dump and don't return an error code).
 */
sysKERNEL_CODE void
debugBufferWaitForEmpty(void)
{
    // Wait until debug buffer is empty
    if (!OS_PTIMER_SPIN_WAIT_US_COND(debugBufferIsEmpty,
            NULL, LW_RISCV_DEBUG_BUFFER_WAIT_TIMEOUT_US))
    {
        //
        // MMINTS-TODO: consider reducing the timeout, and then
        // just disable the debug buffer here until the next print req.
        //
        appHalt();
    }
}

sysKERNEL_CODE void
dbgPutcharKernel(int ch)
{
    lwrtosTaskCriticalEnter();
    debugPutchar(ch, 0);
    lwrtosTaskCriticalExit();

    debugLogsFlush(debugBuffer.pMetadata->writeOffset);
}

sysKERNEL_CODE void
dbgPutsKernel(const char *pStr)
{
    lwrtosTaskCriticalEnter();
    for (; *pStr; pStr++)
    {
        debugPutchar(*pStr, 0);
    }
    lwrtosTaskCriticalExit();

    debugLogsFlush(debugBuffer.pMetadata->writeOffset);
}

sysKERNEL_CODE void
dbgDispatchRawDataKernel(LwU32 numTokens, const LwUPtr *tokens)
{
    if (bBufferEnabled)
    {
        if (numTokens > LW_RISCV_RAW_MODE_PRINT_MAX_TOKENS_LIMIT)
        {
            appHalt();
        }

        // This way we know where each print ends and the next one begins!
        debugPutByteKernel(numTokens);

        // Last token is metadata token
        for (LwU32 i = 0; i < numTokens; i++)
        {
            debugPutDwordKernel(tokens[i]);
        }

        debugLogsFlush(debugBuffer.pMetadata->writeOffset);
    }
}

sysKERNEL_CODE FLCN_STATUS
debugInit(void)
{
    // We store metadata at the end of buffer, subtract that from total size
    LwU64 bufferSize = debugBufferSize - sizeof(LW_RISCV_DEBUG_BUFFER);

    //
    // sanity check - limiting buffer size to UINT_MAX,
    // it would be much smaller than that.
    //
    if (bufferSize >= UINT_MAX)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    debugBuffer.pBufferData = _dmesg_buffer_start;
    debugBuffer.pMetadata   = (LW_RISCV_DEBUG_BUFFER*)(debugBuffer.pBufferData + bufferSize);
    debugBuffer.pMetadata->bufferConfig = DRF_NUM(_RISCV, _DEBUG_BUFFER_CONFIG, _SIZE, bufferSize) |
                                          DRF_DEF(_RISCV, _DEBUG_BUFFER_CONFIG, _MODE, _TEXT);
    debugBuffer.pMetadata->magic = LW_RISCV_DEBUG_BUFFER_MAGIC;

    if ((debugBuffer.pMetadata->readOffset == 0) && (debugBuffer.pMetadata->writeOffset == 0))
    {
        // If buffer is empty at this point, make sure registers reflect that
        csbWrite(ENGINE_REG(_QUEUE_TAIL(RM_RISCV_DEBUG_BUFFER_QUEUE)), 0);
        csbWrite(ENGINE_REG(_QUEUE_HEAD(RM_RISCV_DEBUG_BUFFER_QUEUE)), 0);
    }
    else
    {
        debugBuffer.pMetadata->readOffset = csbRead(ENGINE_REG(_QUEUE_TAIL(RM_RISCV_DEBUG_BUFFER_QUEUE)));
        debugBuffer.pMetadata->writeOffset = csbRead(ENGINE_REG(_QUEUE_HEAD(RM_RISCV_DEBUG_BUFFER_QUEUE)));
    }

    debugBuffer.pMetadata->readOffset %= bufferSize;

    if (debugBuffer.pMetadata->writeOffset >= bufferSize)
    {
        debugBuffer.pMetadata->writeOffset %= bufferSize;
        csbWrite(ENGINE_REG(_QUEUE_HEAD(RM_RISCV_DEBUG_BUFFER_QUEUE)), debugBuffer.pMetadata->writeOffset);
    }

    bBufferEnabled = LW_TRUE;

    if (LWRISCV_PRINT_RAW_MODE)
    {
        //
        // In raw mode, we want to make sure that all the prints from the bootloader
        // (which only supports text mode) have been flushed out.
        //
        // Just in case the bootloader forgot a terminating newline, add one here to
        // make sure RM is able to print and flush out the remaining text from the bootloader.
        //
        debugPutchar('\n', 0);

        //
        // Spin until RM has processed all the remaining text from the bootloader.
        // Since this is early pre-init, don't worry about timeouts.
        //
        do
        {
            debugLogsFlush(debugBuffer.pMetadata->writeOffset);
        } while (!debugBufferIsEmpty(NULL));

        // Set the mode identifier correctly
        debugBuffer.pMetadata->bufferConfig = FLD_SET_DRF(_RISCV, _DEBUG_BUFFER_CONFIG, _MODE, _RAW,
                                                          debugBuffer.pMetadata->bufferConfig);

        //
        // Now we need to fire off a switch-mode notify!
        // Since this is early pre-init, don't worry about timeouts.
        //

        //
        // Do flush only once to avoid edge-cases where RM can think we want
        // to do back-to-back mode switches!
        //
        debugLogsFlush(LW_RISCV_DEBUG_BUFFER_QUEUE_OFFS_MODE_SWITCH_REQ);
        do
        {
            // Wait for RM to respond with an ACK for this!
        } while (csbRead(ENGINE_REG(_QUEUE_TAIL(RM_RISCV_DEBUG_BUFFER_QUEUE))) !=
                 LW_RISCV_DEBUG_BUFFER_QUEUE_OFFS_MODE_SWITCH_ACK);
    }

    dbgPrintf(LEVEL_DEBUG, "Debug init done\n");

    return FLCN_OK;
}

//
// This code is used to print messages by SafeRTOS and Drivers.
//
sysKERNEL_CODE void
dbgPrintfKernel(const char *pcFmt, va_list ap)
{
    lwrtosTaskCriticalEnter();
    vprintfmt(debugPutchar, 0, pcFmt, ap);
    lwrtosTaskCriticalExit();

    debugLogsFlush(debugBuffer.pMetadata->writeOffset);
}
