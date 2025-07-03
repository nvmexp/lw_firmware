/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LWRISCV_LIBC_VPRINTFMT_H
#define LWRISCV_LIBC_VPRINTFMT_H

#include <stdarg.h>

int vprintfmt(void (*putch)(int, void*), void *putdat,
              const char* pFormat, va_list ap);

#endif // LWRISCV_LIBC_VPRINTFMT_H
