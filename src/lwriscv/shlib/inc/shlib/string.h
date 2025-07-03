/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef SHLIB_STRING_H
#define SHLIB_STRING_H

#include <stdarg.h>
#include <stddef.h>

#include <lwriscv/print.h>

#ifdef LWRISCV_SDK
#include <libc/string.h>
#include <libc/vprintfmt.h>
#else /* LWRISCV_SDK */
int vprintfmt(void (*putch)(int, void*), void *putdat, const char *fmt, va_list ap);
size_t strnlen(const char *s, size_t n);
#endif /* LWRISCV_SDK */

//
// Could have just used SECTION() from SafeRTOS here, but that would make SafeRTOS
// a dependency of the string header, which is undesirable.
// MMINTS-TODO: unify?
//
# define LWRISCV_TO_STR2(x) #x
# define LWRISCV_TO_STR(x)  LWRISCV_TO_STR2(x)
# define LWRISCV_SECTION(x) __attribute__((section( x "." __FILE__ LWRISCV_TO_STR(__LINE__))))


# define SEC_STRLIT(_section, _strLit)                                           \
    ({                                                                           \
        static const LWRISCV_SECTION(_section) char pvtStrVal[] = { ""_strLit }; \
        pvtStrVal;                                                               \
    })

// Default string literal wrapper, sections will go into shared RODATA
# if LWRISCV_PRINT_RAW_MODE
#  define DEF_STRLIT(_strLit)  SEC_STRLIT(".logging", _strLit)
#  define DEF_KSTRLIT(_strLit) SEC_STRLIT(".logging", _strLit)
#  define STRSEC               LWRISCV_SECTION(".logging")
#  define KSTRSEC              LWRISCV_SECTION(".logging")
# else // LWRISCV_PRINT_RAW_MODE
#  define DEF_STRLIT(_strLit)  SEC_STRLIT(".shared_rodata", _strLit)
#  define DEF_KSTRLIT(_strLit) SEC_STRLIT(".kernel_rodata", _strLit)
#  define STRSEC               LWRISCV_SECTION(".shared_rodata")
#  define KSTRSEC              LWRISCV_SECTION(".kernel_rodata")
# endif // LWRISCV_PRINT_RAW_MODE

#endif // SHLIB_STRING_H
