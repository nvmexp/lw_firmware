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
#include "libprintf.h"

int LIBOS_SECTION_TEXT_COLD KernelPutChar(struct LibosStream * s, char ch) 
{
    KernelMmioWrite(LW_PRGNLCL_FALCON_DEBUGINFO, ch);
    while (KernelMmioRead(LW_PRGNLCL_FALCON_DEBUGINFO))    
        ;
    return 0;
}

struct StreamKernelPutc
{
	struct LibosStream base;
};

__attribute__((used)) LIBOS_SECTION_TEXT_COLD void KernelPrintf(const char* format, ...)
{
    va_list argptr;
    va_start(argptr, format);
	
	struct StreamKernelPutc putcStream = { { KernelPutChar }};

#if !LIBOS_TINY
	LibosPrintfArgument nl_arg[LIBOS_PRINTF_ARGMAX];
	int nl_type[LIBOS_PRINTF_ARGMAX];

	for (LwU32 i = 0; i < LIBOS_PRINTF_ARGMAX; i++)
		nl_type[i] = 0;

    LibosValistPrintf(&putcStream.base, 0, format, &argptr, &nl_arg[0], &nl_type[0], LIBOS_PRINTF_ARGMAX);
#else
	LibosValistPrintf(&putcStream.base, format, &argptr);
#endif	
    va_end(argptr);
}


