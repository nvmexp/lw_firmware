/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __OEMCOMMONMEM_H__
#define __OEMCOMMONMEM_H__

#include <drmtypes.h>

ENTER_PK_NAMESPACE;

#undef OEM_SELWRE_ZERO_MEMORY

// TEE CHANGE (nkuo):
// gcc stack analysis is having issue in analyzing the function which is
// in .h file. Thus the real implementation of following functions were moved
// from oemcommonmem.h to oemcommonmem_lw.c
//DRMCRT_LocalMemset
//DRMCRT_LocalAreEqual
//DRMCRT_LocalMemcpy
//DRMCRT_ScrubMemory
//DRMCRT_LocalDWORDSetZero
//DRMCRT_LocalDWORDcpy

/* OEM special implementation functions (oemimpl.c). */
#if defined(DRM_MSC_VER) || defined(DRM_GNUC_MAJOR)

DRM_ALWAYS_INLINE DRM_VOID * DRM_CALL DRMCRT_ScrubMemory(
    __out_bcount( f_cbCount ) DRM_VOID  *f_ptr,
    __in DRM_DWORD  f_cbCount  );

#define OEM_SELWRE_ZERO_MEMORY DRMCRT_ScrubMemory

#endif /* defined(DRM_MSC_VER) || defined(DRM_GNUC_MAJOR) */

/*
** OEM_MANDATORY:
** For all devices, this macro MUST be implemented by the OEM.
*/
#ifndef OEM_SELWRE_ZERO_MEMORY
#error "Please provide implementation for OEM_SELWRE_ZERO_MEMORY macro.\
 OEM_SELWRE_ZERO_MEMORY is called to scrub memory on critical pieces of data before freeing buffers or exiting local scope.\
 Using of memset function on these buffers is not sufficient, since compiler\
 may optimize out zeroing of buffers that are not used afterwards.\
 Please verify the implementation will not be optimized out by your compiler.\
 Consult the documentation for your compiler to see how this can be done. \
 The Microsoft supplied example is only guaranteed to work on Microsoft compilers. "

#endif /* ifndef OEM_SELWRE_ZERO_MEMORY */

#undef OEM_SELWRE_MEMCPY
#undef OEM_SELWRE_MEMCPY_IDX
#undef OEM_SELWRE_MEMSET
#undef OEM_SELWRE_ARE_EQUAL
#undef OEM_SELWRE_DWORDCPY
#undef OEM_SELWRE_DIGITTCPY

//#if defined(DRM_MSC_VER) || defined(DRM_GNUC_MAJOR)

DRM_ALWAYS_INLINE DRM_VOID * DRM_CALL DRMCRT_LocalMemcpy(
    __out_bcount_full( f_cbCount )  DRM_VOID  *f_pOut,
    __in_bcount( f_cbCount ) const  DRM_VOID  *f_pIn,
    __in                            DRM_DWORD  f_cbCount );

DRM_ALWAYS_INLINE DRM_VOID * DRM_CALL DRMCRT_LocalMemset(
    __out_bcount_full( f_cbCount )  DRM_VOID  *f_pOut,
    __in                            DRM_BYTE   f_cbValue,
    __in                            DRM_DWORD  f_cbCount );

DRM_ALWAYS_INLINE DRM_VOID * DRM_CALL DRMCRT_LocalDWORDSetZero(
    __out_bcount_full( f_cdwCount ) DRM_DWORD  *f_pdwOut,
    __in                            DRM_DWORD   f_cdwCount );

DRM_ALWAYS_INLINE DRM_BOOL DRM_CALL DRMCRT_LocalAreEqual(
    __in_bcount( f_cbCount ) const  DRM_VOID  *f_pLHS,
    __in_bcount( f_cbCount ) const  DRM_VOID  *f_pRHS,
    __in                            DRM_DWORD  f_cbCount );

#if DRM_SUPPORT_INLINEDWORDCPY

DRM_ALWAYS_INLINE DRM_VOID * DRM_CALL DRMCRT_LocalDWORDcpy(
    __out_ecount_full( f_cdwCount )  DRM_DWORD  *f_pdwOut,
    __in_ecount( f_cdwCount ) const  DRM_DWORD  *f_pdwIn,
    __in                             DRM_DWORD  f_cdwCount );

#define OEM_SELWRE_DWORDCPY(out, in, ecount) DRMCRT_LocalDWORDcpy((DRM_DWORD*)(out), (const DRM_DWORD*)(in), (ecount))

#else   /* DRM_SUPPORT_INLINEDWORDCPY */
#define OEM_SELWRE_DWORDCPY(out, in, ecount) OEM_SELWRE_MEMCPY((out), (in), (ecount)*sizeof(DRM_DWORD))
#endif  /* DRM_SUPPORT_INLINEDWORDCPY */

#if DRM_DWORDS_PER_DIGIT == 1
#define OEM_SELWRE_DIGITTCPY OEM_SELWRE_DWORDCPY
#define OEM_SELWRE_DIGITZEROMEMORY DRMCRT_LocalDWORDSetZero
#endif  /* DRM_DWORDS_PER_DIGIT == 1 */

#define OEM_SELWRE_MEMCPY DRMCRT_LocalMemcpy

#define OEM_SELWRE_MEMCPY_IDX(dest, dest_offset, source, source_offset, count) DRM_DO {             \
    DRM_BYTE *pbDest   = (DRM_BYTE*)( dest );                                                       \
    DRM_BYTE *pbSource = (DRM_BYTE*)( source );                                                     \
    ChkArg( pbDest != NULL && pbSource != NULL );                                                   \
    ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)pbDest,   ( dest_offset ),   (DRM_SIZE_T*)&pbDest ) );      \
    ChkDR( DRM_DWordPtrAdd( (DRM_SIZE_T)pbSource, ( source_offset ), (DRM_SIZE_T*)&pbSource ) );    \
    __analysis_assume( pbDest == (dest)+(dest_offset) && pbSource == (source)+(source_offset) );    \
    OEM_SELWRE_MEMCPY( (char*)pbDest, (const char*)pbSource, (count) );                             \
} DRM_WHILE_FALSE

#define OEM_SELWRE_MEMSET DRMCRT_LocalMemset

#define OEM_SELWRE_ARE_EQUAL DRMCRT_LocalAreEqual

//#endif /* defined(DRM_MSC_VER) || defined(DRM_GNUC_MAJOR) */

/*
** OEM_MANDATORY:
** For all devices, these macros MUST be implemented by the OEM.
*/
#if !defined(OEM_SELWRE_MEMCPY) || !defined(OEM_SELWRE_MEMCPY_IDX) || !defined(OEM_SELWRE_DWORDCPY) || !defined(OEM_SELWRE_DIGITTCPY) || !defined(OEM_SELWRE_DIGITZEROMEMORY)

#error "Please provide implementation for OEM_SELWRE_* macros.\
 OEM_SELWRE_* is used for copying or setting bytes where calls to CRT implementation of \
 mem* functions cannot be used."

#endif /* !defined(OEM_SELWRE_MEMCPY) || !defined(OEM_SELWRE_MEMCPY_IDX) || !defined(OEM_SELWRE_DWORDCPY) || !defined(OEM_SELWRE_DIGITTCPY) || !defined(OEM_SELWRE_DIGITZEROMEMORY) */

EXIT_PK_NAMESPACE;

#endif /* __OEMCOMMONMEM_H__ */

