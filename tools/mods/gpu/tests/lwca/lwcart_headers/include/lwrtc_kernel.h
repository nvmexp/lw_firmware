//HARDCODE FOR LWRTC.
#define __LWDACC__ 1
#define __LWDALWVM__ 1


#if !defined(__cplusplus)
#error "unexpected!"
#endif

// CSJ FIXME: document these predefined types and macros. 
// Also check if need to add remaining from cstddef header.

#if defined(__LP64__)
typedef unsigned long size_t;
typedef long int ptrdiff_t;
#else
typedef unsigned long long size_t;
typedef long long int ptrdiff_t;
#endif

typedef long clock_t;

#ifndef NULL
#define NULL 0
#endif 
// end predefined types and macros

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */
__attribute__((device)) int __lwvm_reflect(const char*);
#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#include "lwda_runtime.h"
