/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <stdarg.h>
#include <limits.h>

#include <lwmisc.h>
#include <lwriscv/fence.h>
#include <lwriscv-intf/debug.h>

#include "io.h"
#include "common.h"

static bool bPrintEnabled = false;

//
// Local copy of buffer metadata - we don't trust buffer shared with CPU.
//
static LWRISCV_DEBUG_BUFFER printBuffer;
static volatile uint8_t *printBufferData;

static volatile LWRISCV_DEBUG_BUFFER *printBufferTarget;

static const char s_hex_table[16] = "0123456789ABCDEF";

//------------------------------------------------------------------------------
// Code
//------------------------------------------------------------------------------
static bool printBufferIsFull(void)
{
    uint32_t wo;

    // Optimized modulo operation
    wo = printBuffer.writeOffset + 1;
    if (wo >= printBuffer.bufferSize)
    {
        wo -= printBuffer.bufferSize;
    }

    printBuffer.readOffset = printBufferTarget->readOffset;

    return printBuffer.readOffset == wo;
}

static void debugLogsFlush(void)
{
    //
    // Ensure logs reach system memory, increment corresponding offset and
    // send notification (interrupt) to client RM.
    // MK TODO: maybe lightweight fence if there is no fbif?
    //
    riscvLwfenceIO();

    printBufferTarget->writeOffset = printBuffer.writeOffset;
}

static void debugPutchar(int ch, void *pArg)
{
    // If buffer is full, notify client RM and stall GSP
    if (printBufferIsFull())
    {
        debugLogsFlush();
        do
        {
        } while (printBufferIsFull());
    }

    // Replace newline with null char for ease of displaying debug logs
    if (ch == '\n')
    {
        ch = '\0';
    }

    printBufferData[printBuffer.writeOffset] = (uint8_t)ch;

    // Optimized modulo operation
    printBuffer.writeOffset = printBuffer.writeOffset + 1;
    if (printBuffer.writeOffset >= printBuffer.bufferSize)
    {
        printBuffer.writeOffset -= printBuffer.bufferSize;
    }
}

bool printInit(uint16_t bufferSize)
{
    uint32_t dmemSize = DRF_VAL(_PRGNLCL, _FALCON_HWCFG3, _DMEM_TOTAL_SIZE,
                                localRead(LW_PRGNLCL_FALCON_HWCFG3)) << 8;
    void * pBuffer;

    if (bufferSize & 0x7)
    {
        return false;
    }

    if (bufferSize < sizeof(printBuffer) + 16)
    {
        return false;
    }

    pBuffer = (void*)LW_RISCV_AMAP_DMEM_START + dmemSize - bufferSize;

    if ((uintptr_t)pBuffer & 0x7)
    {
        return false;
    }

    printBufferData = pBuffer;
    printBuffer.readOffset = 0;
    printBuffer.writeOffset = 0;
    printBuffer.bufferSize = (uint32_t)bufferSize - (uint32_t)sizeof(printBuffer);
    printBuffer.magic = LWRISCV_DEBUG_BUFFER_MAGIC;

    // Let last character be always null char for ease of displaying debug logs
    printBufferData[printBuffer.bufferSize] = '\0';

    printBufferTarget = (volatile LWRISCV_DEBUG_BUFFER *)(((uint8_t*)pBuffer) + bufferSize - sizeof(LWRISCV_DEBUG_BUFFER));
    printBufferTarget->readOffset = printBuffer.readOffset;
    printBufferTarget->writeOffset = printBuffer.writeOffset;
    printBufferTarget->bufferSize = printBuffer.bufferSize;
    printBufferTarget->magic = printBuffer.magic;

    bPrintEnabled = true;

    return true;
}

int putchar(int c)
{
    if (bPrintEnabled)
    {
        debugPutchar(c, 0);
        debugLogsFlush();
    }
    return 0;
}

int puts(const char *pStr)
{
    if (bPrintEnabled)
    {
        for (; *pStr; pStr++)
        {
            debugPutchar(*pStr, 0);
        }
        debugLogsFlush();
    }
    return 0;
}

void putHex(int count, unsigned long value)
{
    if (bPrintEnabled)
    {
        while (count)
        {
            count--;
            putchar(s_hex_table[(value >> (count * 4)) & 0xF]);
        }
        debugLogsFlush();
    }
}

int printf(const char *pFmt, ...)
{
    va_list ap;

    va_start(ap, pFmt);

    if (bPrintEnabled)
    {
        vprintfmt(debugPutchar, 0, pFmt, ap);

        debugLogsFlush();
    }
    va_end(ap);

    return 0;
}
