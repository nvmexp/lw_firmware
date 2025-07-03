/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <stdarg.h>

#include <lwrtos.h>
#include <lwriscv/print.h>
#include <shlib/syscall.h>

#if LWRISCV_PRINT_RAW_MODE

void printRawModeDispatch(LwU32 numTokens, const LwUPtr *tokens)
{
    if (lwrtosIS_KERNEL())
    {
        dbgDispatchRawDataKernel(numTokens, tokens);
    }
    else
    {
        syscall2(LW_APP_SYSCALL_DBG_DISPATCH_RAW_DATA, numTokens, (LwUPtr)tokens);
    }
}

void gccFakePrintf(const char *pFmt, ...)
{
    (void)pFmt;
}

#else // LWRISCV_PRINT_RAW_MODE

int putchar(int c)
{
    if (lwrtosIS_KERNEL())
    {
        dbgPutcharKernel(c);
    }
    else
    {
        dbgPutcharUser(c);
    }
    return 0;
}

int puts(const char *pStr)
{
    if (lwrtosIS_KERNEL())
    {
        dbgPutsKernel(pStr);
    }
    else
    {
        dbgPutsUser(pStr);
    }
    return 0;
}

int printf(const char *pFmt, ...)
{
    va_list ap;

    va_start(ap, pFmt);
    if (lwrtosIS_KERNEL())
    {
        dbgPrintfKernel(pFmt, ap);
    }
    else
    {
        dbgPrintfUser(pFmt, ap);
    }
    va_end(ap);
    return 0;
}

#endif // LWRISCV_PRINT_RAW_MODE
