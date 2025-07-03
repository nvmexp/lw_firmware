/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    syslibPrint.c
 * @brief   Debug print code for user tasks.
 */

/* ------------------------ System Includes -------------------------------- */
#include <stdarg.h>

/* ------------------------ LW Includes ------------------------------------ */
#include <lwtypes.h>
#include <lwriscv/print.h>

/* ------------------------ SafeRTOS Includes ------------------------------ */
#include <sections.h>
#include <shared.h>

/* ------------------------ Drivers Includes ------------------------------- */
#include <shlib/syscall.h>
#include <shlib/string.h>

#define PUTCHAR_BUF_SIZE 32

typedef struct
{
    unsigned ofs;
    unsigned totalChars;
    char chars[PUTCHAR_BUF_SIZE];
} PUTCHAR_BUF;

sysSHARED_CODE static void
putchar_buf_flush(PUTCHAR_BUF *pBuf)
{
    if (pBuf->ofs > 0)
    {
        pBuf->chars[pBuf->ofs] = 0;
        syscall1(LW_APP_SYSCALL_PUTS, (LwU64)pBuf->chars);
        pBuf->ofs = 0;
    }
}

sysSHARED_CODE static void
putchar_buffered(int ch, void *pArg)
{
    PUTCHAR_BUF *pBuf = (PUTCHAR_BUF*)pArg;

    pBuf->chars[pBuf->ofs] = ch;
    pBuf->ofs++;
    pBuf->totalChars++;
    if (ch == '\n' || (pBuf->ofs + 1) >= PUTCHAR_BUF_SIZE)
    {
        putchar_buf_flush(pBuf);
    }
}

sysSHARED_CODE void dbgPutcharUser(int c)
{
    char st[]={c, 0};
    syscall1(LW_APP_SYSCALL_PUTS, (LwU64)&st);
}

sysSHARED_CODE void dbgPutsUser(const char *pStr)
{
    PUTCHAR_BUF buf;
    buf.ofs = 0;
    buf.totalChars = 0;

    for (; *pStr; pStr++)
    {
        putchar_buffered(*pStr, &buf);
    }
    putchar_buffered('\n', &buf);
}

sysSHARED_CODE void dbgPrintfUser(const char *pFmt, va_list ap)
{
    PUTCHAR_BUF buf;

    buf.ofs = 0;
    buf.totalChars = 0;

    vprintfmt(putchar_buffered, &buf, pFmt, ap);
    putchar_buf_flush(&buf);
}
