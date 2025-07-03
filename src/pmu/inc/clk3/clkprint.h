/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see     Confluence: RMCLOC/Clocks+3.0
 * @author  Antone Vogt-Varvak
 * @note    Each macro is guarded rather than the file as a whole.
 */

#ifndef CLK3_PRINT_H
#define CLK3_PRINT_H


/* ------------------------ Includes --------------------------------------- */

#if defined(UPROC_RISCV)
#include "lwriscv/print.h"
#endif


/* ------------------------ Macros ----------------------------------------- */

/*!
 * @brief       Are prints enabled for this build?
 *
 * @note        Because this feature consumes a fair amount of non-swappable
 *              DMEM (for the format strings), it should be enabled on an
 *              as-needed basis.
 *
 * @todo        Maxim Mints plans to tokenize the format strings.  Since that
 *              is done, this feature should be enabled on all supported builds.
 *
 * @details     When debugging an issue, enable this feature then start a
 *              virtual DVS build, pasting a link to the appropriate bug
 *              report, etc.
 *
 * @note        Printing can be enabled for supported builds by including
 *              -DCLK_PRINT_ENABLED=1 on the compiler command line (via make)
 *              or by editing this file and changing the 0 to 1 below.
 */
#if defined(UPROC_RISCV) && LWRISCV_DEBUG_PRINT_ENABLED
#ifndef CLK_PRINT_ENABLED
#define CLK_PRINT_ENABLED 0     /* 1=Enable 0=Disable */
#endif

#else // defined(UPROC_RISCV) && LWRISCV_DEBUG_PRINT_ENABLED
#ifdef  CLK_PRINT_ENABLED
#undef  CLK_PRINT_ENABLED
#endif
#define CLK_PRINT_ENABLED 0     /* Always disabled for non-RISCV */
#endif // defined(UPROC_RISCV) && LWRISCV_DEBUG_PRINT_ENABLED


/*!
 * @brief       Print if prints are enabled.
 */
// TODO: `printf` in uproc/lwriscv/inc/lwriscv/print.h should have this attribute:
//      __attribute__ ((format (printf, 1, 2)))
// Once that is done, `clkPrintfStaticChecker` becomes obsolete.
#if CLK_PRINT_ENABLED
inline void __attribute__ ((format (printf, 1, 2)))
            clkPrintfStaticChecker(const char *pFmt, ...) { }
#define CLK_PRINTF(pFmt, ...) do                            \
{                                                           \
    clkPrintfStaticChecker((pFmt), ##__VA_ARGS__);          \
    dbgPrintf(LEVEL_CRIT, (pFmt), ##__VA_ARGS__);           \
} while (LW_FALSE)
#else
#define CLK_PRINTF(pFmt, ...) ((void) 0)
#endif


/*!
 * @brief       Read and print register data.
 */
#if CLK_PRINT_ENABLED
#define CLK_PRINT_REG(PREFIX, ADDR) do                      \
{                                                           \
    if ((ADDR) != 0x00000000)                               \
    {                                                       \
        LwU32 data = CLK_REG_RD32(ADDR);                    \
        CLK_PRINTF("%s: addr=0x%08x data=0x%08x\n",         \
            (PREFIX), (ADDR), data);                        \
    }                                                       \
} while (LW_FALSE)
#else
#define CLK_PRINT_REG(PREFIX, ADDR) ((void) 0)
#endif

#endif // CLK3_PRINT_H
