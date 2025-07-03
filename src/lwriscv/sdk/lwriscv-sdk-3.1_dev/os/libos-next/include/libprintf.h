/*
 * Copyright (c) 2018-2020 LWPU CORPORATION.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of LWPU CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROLWREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef LIBOS_PRINTF_H_
#define LIBOS_PRINTF_H_
#include "lwtypes.h"
#include <stdarg.h>
#include "libos-config.h"


#ifdef __cplusplus
extern "C"
{
#endif

#define LIBOS_PRINTF_ARGMAX 32

typedef union
{
	LwU64 i;   
#if LIBOS_CONFIG_FLOAT
	long double f;
#endif    
	void *p;
} LibosPrintfArgument;

// Inherit from this object
struct LibosStream
{
	int (*putc)(struct LibosStream * stream, char c);	
};

// Printf writing characters through putc()
__attribute__((format(printf, 1, 2)))
void LibosPrintf(const char * args, ...);

// Performs a printf from a va-list
#if (defined(LIBOS_HOST) && LIBOS_HOST) || !LIBOS_TINY
int LibosValistPrintf(struct LibosStream * stream, int (*printPointer)(struct LibosStream * stream, void * p), const char *fmt, va_list *ap, LibosPrintfArgument *nl_arg, int *nl_type, int nl_count);
#else
int LibosValistPrintf(struct LibosStream * stream, const char *fmt, va_list *ap);
#endif

/*!
 *	@brief Performs a printf() operation an an array of tokens stored in a log.
 *
 * 	%s  -> Argument string must be a const literal to be printed. 
 *	%-p -> Prints the address in hex followed by the symbol and/or line number
 *		   corresponding to the address.  Used for printing call stacks
 *		   and names of global variables.
 *  %p  -> Prints address in hex.
 * 		   Setting LIBOS_PRINTF_RESOLVE_POINTERS causes this to also
 * 		   print the symbol and line number information.
 *  %n  -> Has no effect.
 * 
 *  Returns a value less than 0 if an error oclwrred.
 */
#if (defined(LIBOS_HOST) && LIBOS_HOST) || !LIBOS_TINY
int LibosTokenPrintf(struct LibosStream * stream, int (*printPointer)(struct LibosStream * stream, void * p), const char *fmt, LibosPrintfArgument * arguments, LwU32 argumentCount);
#endif

#ifdef __cplusplus
}
#endif
#endif