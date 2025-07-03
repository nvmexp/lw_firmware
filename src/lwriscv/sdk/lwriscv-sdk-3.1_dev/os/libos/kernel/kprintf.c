/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "kernel.h"
#include "libos_log.h"

int LIBOS_SECTION_TEXT_COLD KernelPutChar(char ch, void * f) 
{
    KernelMmioWrite(LW_PRGNLCL_FALCON_DEBUGINFO, ch);
    while (KernelMmioRead(LW_PRGNLCL_FALCON_DEBUGINFO))    
        ;
    return 0;
}

__attribute__((used)) LIBOS_SECTION_TEXT_COLD void KernelPrintf(const char* format, ...)
{
    va_list argptr;
    va_start(argptr, format);
#if !LIBOS_CONFIG_PRINTF_TINY
	union arg nl_arg[NL_ARGMAX];
	int nl_type[NL_ARGMAX];
    printf_core(KernelPutChar, 0, format, &argptr, &nl_arg, &nl_type);
#else
	printf_core(KernelPutChar, 0, format, &argptr);
#endif	
    va_end(argptr);
}
