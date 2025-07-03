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
#include <liblwriscv/print.h>
#include <liblwriscv/libc.h>
#include <liblwriscv/io.h>

static bool bPrintEnabled = false;

//
// Local copy of buffer metadata - we don't trust buffer shared with CPU.
//
static LWRISCV_DEBUG_BUFFER printBuffer;
static volatile uint8_t *printBufferData;

#if LWRISCV_CONFIG_DEBUG_PRINT_USES_QUEUES
static uint32_t queueHeadAddr;
static uint32_t queueTailAddr;
#endif

#if LWRISCV_CONFIG_DEBUG_PRINT_METADATA_IN_BUFFER
static volatile LWRISCV_DEBUG_BUFFER *printBufferTarget;
#endif

#if LWRISCV_CONFIG_DEBUG_PRINT_USES_SWGEN
static uint8_t printBufferInterrupt; // SWGEN 0 / 1
#endif

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


#if LWRISCV_CONFIG_DEBUG_PRINT_METADATA_IN_BUFFER
    printBuffer.readOffset = printBufferTarget->readOffset;
#endif

// Check queues (if enabled) *after* we read offset from buffer. That way
// both can be enabled, but queue takes precedence.
#if LWRISCV_CONFIG_DEBUG_PRINT_USES_QUEUES
    // re-read register if we think we run out of buffer space
    if (printBuffer.readOffset == wo)
    {
        printBuffer.readOffset = priRead(queueTailAddr);
    }
#endif

    return printBuffer.readOffset == wo;
}

static void debugLogsFlush(void)
{
    //
    // Ensure logs reach system memory, increment corresponding offset and
    // send notification (interrupt) to client RM.
    // MK TODO: maybe lightweight fence if there is no fbif?
    //
#if !LWRISCV_HAS_FBIF || LWRISCV_WAR_FBIF_IS_DEAD
    riscvLwfenceIO();
#else
    riscvFenceRW();
#endif

#if LWRISCV_PLATFORM_IS_CMOD
#if LWRISCV_CONFIG_DEBUG_PRINT_METADATA_IN_BUFFER
    printBuffer.readOffset = printBufferTarget->readOffset;
#endif

    // This is based on Cmod print code
    localWrite(LW_PRGNLCL_FALCON_DEBUGINFO, 0x15c0de02); //start code
    localWrite(LW_PRGNLCL_FALCON_DEBUGINFO, 0x00000000); //number
    while (printBuffer.readOffset != printBuffer.writeOffset)
    {
        localWrite(LW_PRGNLCL_FALCON_DEBUGINFO, (uint32_t)(printBufferData[printBuffer.readOffset]) );
        // Optimized modulo operation
        printBuffer.readOffset = printBuffer.readOffset + 1;
        if (printBuffer.readOffset >= printBuffer.bufferSize)
        {
            printBuffer.readOffset -= printBuffer.bufferSize;
        }
    }
    localWrite(LW_PRGNLCL_FALCON_DEBUGINFO, 0x0e0dc0de); //end code

#if LWRISCV_CONFIG_DEBUG_PRINT_METADATA_IN_BUFFER
    printBufferTarget->readOffset = printBuffer.readOffset;
#endif
#else //LWRISCV_PLATFORM_IS_CMOD
#if LWRISCV_CONFIG_DEBUG_PRINT_USES_QUEUES
    priWrite(queueHeadAddr,
             printBuffer.writeOffset);
#endif

#if LWRISCV_CONFIG_DEBUG_PRINT_METADATA_IN_BUFFER
    printBufferTarget->writeOffset = printBuffer.writeOffset;
#endif

#if LWRISCV_CONFIG_DEBUG_PRINT_USES_SWGEN
    if (printBufferInterrupt == 0)
        localWrite(LW_PRGNLCL_FALCON_IRQSSET, DRF_DEF(_PRGNLCL, _FALCON_IRQSSET, _SWGEN0, _SET));
    else
        localWrite(LW_PRGNLCL_FALCON_IRQSSET, DRF_DEF(_PRGNLCL, _FALCON_IRQSSET, _SWGEN1, _SET));
#endif
#endif //LWRISCV_PLATFORM_IS_CMOD
}

static void debugPutchar(int ch, void *pArg GCC_ATTR_UNUSED)
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

bool printInitEx(void *pBuffer, uint16_t bufferSize, uint32_t queueHeadPri, uint32_t queueTailPri, uint8_t swgenNo)
{
    if (pBuffer == NULL)
    {
        return false;
    }

    if (bufferSize & 0x7)
    {
        return false;
    }

    if (bufferSize < sizeof(printBuffer) + 16)
    {
        return false;
    }

    if ((uintptr_t)pBuffer & 0x7)
    {
        return false;
    }

#if LWRISCV_CONFIG_DEBUG_PRINT_USES_QUEUES
    queueHeadAddr = queueHeadPri;
    queueTailAddr = queueTailPri;
#else
    (void) queueHeadPri;
    (void) queueTailPri;
#endif

#if LWRISCV_CONFIG_DEBUG_PRINT_USES_SWGEN
    if (swgenNo > 1)
    {
        return false;
    }

    printBufferInterrupt = swgenNo;
#else
    (void) swgenNo;
#endif

    printBufferData = pBuffer;
    printBuffer.readOffset = 0;
    printBuffer.writeOffset = 0;
    printBuffer.bufferSize = (uint32_t)bufferSize - (uint32_t)sizeof(printBuffer);
    printBuffer.magic = LWRISCV_DEBUG_BUFFER_MAGIC;

    // Let last character be always null char for ease of displaying debug logs
    printBufferData[printBuffer.bufferSize] = '\0';

#if LWRISCV_CONFIG_DEBUG_PRINT_USES_QUEUES
    printBuffer.readOffset = priRead(queueTailAddr);
    printBuffer.writeOffset = priRead(queueHeadAddr);

    // Optimized modulo operation
    if (printBuffer.writeOffset >= printBuffer.bufferSize)
    {
        printBuffer.writeOffset -= printBuffer.bufferSize;
        priWrite(queueHeadAddr, printBuffer.writeOffset);
    }
#endif

#if LWRISCV_CONFIG_DEBUG_PRINT_METADATA_IN_BUFFER
    printBufferTarget = (volatile LWRISCV_DEBUG_BUFFER *)(((uint8_t*)pBuffer) + bufferSize - sizeof(LWRISCV_DEBUG_BUFFER));
    printBufferTarget->readOffset = printBuffer.readOffset;
    printBufferTarget->writeOffset = printBuffer.writeOffset;
    printBufferTarget->bufferSize = printBuffer.bufferSize;
    printBufferTarget->magic = printBuffer.magic;
#endif

    bPrintEnabled = true;

    return true;
}

bool printInit(void *pBuffer, uint16_t bufferSize,
               uint8_t queueNo GCC_ATTR_UNUSED, uint8_t swgenNo)
{
#if LWRISCV_CONFIG_DEBUG_PRINT_USES_QUEUES
    (void) pBuffer;
    (void) bufferSize;
    (void) swgenNo;
    return false; // We can't do queues with legacy print init, return error
#else
    return printInitEx(pBuffer, bufferSize, 0, 0, swgenNo);
#endif // LWRISCV_CONFIG_DEBUG_PRINT_USES_QUEUES
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
    if ((pStr != NULL) && bPrintEnabled)
    {
        for (; *pStr; pStr++)
        {
            debugPutchar(*pStr, 0);
        }
        debugLogsFlush();
    }
    return 0;
}

void putHex(unsigned count, unsigned long value)
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
