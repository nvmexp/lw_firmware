/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   debug.c
 * @brief  Basic debugging features.
 */
#include <lwmisc.h>
#include <riscv_csr.h>

#include <engine.h>
#include "bootloader.h"

#include "debug.h"
#include "dmem_addrs.h"
#include "state.h"

#include <memops.h>

#if LWRISCV_DEBUG_PRINT_ENABLED
static const char s_hex_table[16] = "0123456789ABCDEF";

static void debugBufferClear(void)
{
    LW_DBG_STATE *dbg = GET_DBG_STATE();
    LwU32         i;

    for (i = 0; i < DRF_VAL(_RISCV, _DEBUG_BUFFER_CONFIG, _SIZE, dbg->metadata->bufferConfig); i++)
    {
        dbg->data[i] = 0;
    }
}

static LwBool debugBufferIsFull(void)
{
    LW_DBG_STATE *dbg = GET_DBG_STATE();
    LwU32 wo;
    LwU32 bufferSize = DRF_VAL(_RISCV, _DEBUG_BUFFER_CONFIG, _SIZE, dbg->metadata->bufferConfig);

    // Optimized modulo operation
    wo = dbg->metadata->writeOffset + 1;
    if (wo >= bufferSize)
    {
        wo -= bufferSize;
    }

    // re-read register if we think we run out of buffer space
    if (dbg->metadata->readOffset == wo)
    {
        dbg->metadata->readOffset = csbReadPA(ENGINE_REG(_QUEUE_TAIL(RM_RISCV_DEBUG_BUFFER_QUEUE)));
    }

    return (dbg->metadata->readOffset == wo);
}

void debugLogsFlush(void)
{
    LW_DBG_STATE *dbg = GET_DBG_STATE();

    //
    // Ensure logs writes complete, increment corresponding offset and
    // send notification (interrupt) to RM.
    //
    SYS_FLUSH_MEM();
    csbWritePA(ENGINE_REG(_QUEUE_HEAD(RM_RISCV_DEBUG_BUFFER_QUEUE)),
              dbg->metadata->writeOffset);

    // Fire interrupt
    intioWrite(LW_PRGNLCL_FALCON_IRQSSET,
              DRF_DEF(_PRGNLCL, _FALCON_IRQSSET, _SWGEN1, _SET));
}

/*!
 * @brief Puts a character to the debug TTY.
 *
 * @param[in] ch    ASCII character to print.
 */
int putchar(int ch)
{
    LW_DBG_STATE *dbg = GET_DBG_STATE();
    LwU32 bufferSize = DRF_VAL(_RISCV, _DEBUG_BUFFER_CONFIG, _SIZE, dbg->metadata->bufferConfig);

    if (!dbg->enabled)
    {
        return 0;
    }

    if (debugBufferIsFull())
    {
        debugLogsFlush();
        do
        {
        } while (debugBufferIsFull());
    }

    if (ch == '\n')
    {
        ch = '\0';
    }

    dbg->data[dbg->metadata->writeOffset] = (LwU8)ch;

    // Optimized modulo operation
    dbg->metadata->writeOffset++;
    if (dbg->metadata->writeOffset >= bufferSize)
    {
        dbg->metadata->writeOffset -= bufferSize;
    }
    return 0;
}

/*!
 * @brief Puts a string to the debug TTY.
 *
 * @param[in] str   String to print.
 */
int puts(const char *str)
{
    LW_DBG_STATE *dbg = GET_DBG_STATE();

    if (!dbg->enabled)
    {
        return 0;
    }

    while (*str)
    {
        putchar(*(str++));
    }
    debugLogsFlush();
    return 0;
}

/*!
 * @brief Puts a number in hexadecimal to the debug TTY.
 *
 * @param[in] count Number of hex digits to print.
 * @param[in] value Value to print.
 */
void putHex(int count, unsigned long value)
{
    LW_DBG_STATE *dbg = GET_DBG_STATE();

    if (!dbg->enabled)
    {
        return;
    }

    while (count)
    {
        count--;
        putchar(s_hex_table[(value >> (count * 4)) & 0xF]);
    }
    debugLogsFlush();
}

/*!
 * @brief Disables debug system.
 */
void dbgDisable(void)
{
    LW_DBG_STATE *state = GET_DBG_STATE();

    // Just in case there is anything left.
    debugLogsFlush();

    state->enabled = LW_FALSE;
}

/*!
 * @brief Initializes debug system, resetting pointers.
 */
void dbgInit(void)
{
    LW_DBG_STATE *state = GET_DBG_STATE();

    _Static_assert(DMESG_BUFFER_SIZE > sizeof(LW_RISCV_DEBUG_BUFFER),
        "DMESG buffer is too small to fit metadata!");

    LwU32 bufferSize = DMESG_BUFFER_SIZE - sizeof(LW_RISCV_DEBUG_BUFFER);
    LwU32 irqMset    = intioRead(LW_PRGNLCL_RISCV_IRQMASK);
    LwU32 irqDest    = intioRead(LW_PRGNLCL_RISCV_IRQDEST);
    LwU32 irqType    = intioRead(LW_PRGNLCL_RISCV_IRQTYPE);
    LwU32 irqMode    = intioRead(LW_PRGNLCL_FALCON_IRQMODE);

    //
    // Here we have the task of setting up the debug buffer in a way agnostic to whether or not
    // we had any prior prints from the FMC/springboard (which could be still pending,
    // i.e. lwrrently getting dequeued by RM).
    //

    //
    // Initialize state. This is either done the first time (safe), or writing over the same data
    // (prev prints from FMC/springboard happened, also safe), or writing over different data
    // (programmer error, e.g. bad print cfg in FMC/springboard); the last case is, sadly, ignored:
    // we can't come up with good sanities here given that we don't know the state of DMEM at init
    // on some platforms, so we can't handily tell apart garbage DMEM from a bad print cfg inherited
    // from FMC/springboard.
    //
    state->data = (LwU8*)(LwUPtr)(DMESG_BUFFER_BASE);
    state->metadata = (LW_RISCV_DEBUG_BUFFER*)(state->data + bufferSize);
    state->metadata->bufferConfig = DRF_NUM(_RISCV, _DEBUG_BUFFER_CONFIG, _SIZE, bufferSize) |
                                    DRF_DEF(_RISCV, _DEBUG_BUFFER_CONFIG, _MODE, _TEXT); // BL only supports text prints
    state->metadata->magic = LW_RISCV_DEBUG_BUFFER_MAGIC;

    if ((csbReadPA(ENGINE_REG(_QUEUE_TAIL(RM_RISCV_DEBUG_BUFFER_QUEUE))) == 0U) &&
        (csbReadPA(ENGINE_REG(_QUEUE_HEAD(RM_RISCV_DEBUG_BUFFER_QUEUE))) == 0U))
    {
        //
        // This is either the first startup (the queue registers are 0 out of reset),
        // or, possibly, we have done prints in the FMC/springboard and completed them
        // successfully, i.e. read offset caught up with write offset, arriving at r==w==0
        // (unlikely but possible, depending on what we print in the FMC/springboard).
        // In either case we can safely reset the debug buffer here.
        //

        state->metadata->readOffset = 0;
        state->metadata->writeOffset = 0;

        // Since we're starting with a clean slate, it's safe to empty the debug buffer!
        debugBufferClear();
    }

    // Enable _SWGEN1
    irqMset = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQMSET, _SWGEN1, _SET, irqMset);

    // route _SWGEN1 to engine
    irqDest = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDEST, _SWGEN1, _HOST, irqDest);

    // route as nonstall
    irqType = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQTYPE, _SWGEN1, _HOST_NORMAL, irqType);

    // make sure _SWGEN1 is edge-triggered
    irqMode = FLD_SET_DRF(_PRGNLCL, _FALCON_IRQMODE, _LVL_SWGEN1, _FALSE, irqMode);

    intioWrite(LW_PRGNLCL_RISCV_IRQDEST, irqDest);
    intioWrite(LW_PRGNLCL_RISCV_IRQTYPE, irqType);
    intioWrite(LW_PRGNLCL_FALCON_IRQMODE, irqMode);

    intioWrite(LW_PRGNLCL_RISCV_IRQMSET, irqMset);
    intioWrite(LW_PRGNLCL_RISCV_IRQMCLR, ~irqMset);

    state->enabled = LW_TRUE;

    dbgPuts(LEVEL_INFO, "!Initialized bootloader debug buffer... \nphys / size: ");
    dbgPutHex(LEVEL_INFO, 16, (LwU64)state->data);
    dbgPuts(LEVEL_INFO, " / ");
    dbgPutHex(LEVEL_INFO, 16, bufferSize);
    dbgPuts(LEVEL_INFO, "\n");
}
#endif // if LWRISCV_DEBUG_PRINT_ENABLED
