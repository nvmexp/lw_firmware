/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

/*
** Module Name: oemcommonmem_lw.c
*/

//
// LWE (nkuo):
// gcc stack analysis is having issue in analyzing the function which is
// in .h file. Thus the real implementation of following functions were moved
// from oemcommonmem.h to oemcommonmem_lw.c
//DRMCRT_LocalMemset
//DRMCRT_LocalAreEqual
//DRMCRT_LocalMemcpy
//DRMCRT_ScrubMemory
//DRMCRT_LocalDWORDSetZero
//DRMCRT_LocalDWORDcpy
//

#include <oemcommonmem.h>

ENTER_PK_NAMESPACE_CODE;

/* OEM special implementation functions (oemimpl.c). */
#if defined(DRM_MSC_VER) || defined(__GNUC__)

DRM_ALWAYS_INLINE DRM_VOID * DRM_CALL DRMCRT_ScrubMemory(
    __out_bcount( f_cbCount ) DRM_VOID  *f_ptr,
    __in DRM_DWORD  f_cbCount  )
{
    /*
    ** Casting the pointer to volatile makes MS and GNU compilers act
    ** as if another thread can see and access the buffer. This
    ** prevents the compiler from reordering or optimizing away
    ** the writes.
    */

    volatile char *vptr = (volatile char *)f_ptr;

    while (f_cbCount)
    {
        *vptr = 0;
        vptr++;
        f_cbCount--;
    }

    return f_ptr;
}


DRM_ALWAYS_INLINE DRM_VOID * DRM_CALL DRMCRT_LocalMemcpy(
    __out_bcount_full( f_cbCount )  DRM_VOID  *f_pOut,
    __in_bcount( f_cbCount ) const  DRM_VOID  *f_pIn,
    __in                            DRM_DWORD  f_cbCount )
{
    /* w/o volatile some compilers can figure out this is an
       implementation of a memcpy */
    volatile const char * pIn = (const char *) f_pIn;
    char * pOut = (char *) f_pOut;
    while (f_cbCount--)
    {
        *pOut++ = *pIn++;
    }
    return f_pOut;
}

DRM_ALWAYS_INLINE DRM_VOID * DRM_CALL DRMCRT_LocalMemset(
    __out_bcount_full( f_cbCount )  DRM_VOID  *f_pOut,
    __in                            DRM_BYTE   f_cbValue,
    __in                            DRM_DWORD  f_cbCount )
{
    /* w/o volatile some compilers can figure out this is an
       implementation of a memset */
    volatile unsigned char * pOut = (unsigned char *) f_pOut;
    while (f_cbCount--)
    {
        *pOut++ = f_cbValue;
    }
    return f_pOut;
}

DRM_ALWAYS_INLINE DRM_VOID * DRM_CALL DRMCRT_LocalDWORDSetZero(
    __out_bcount_full( f_cdwCount ) DRM_DWORD  *f_pdwOut,
    __in                            DRM_DWORD   f_cdwCount )
{
    /* w/o volatile some compilers can figure out this is an
       implementation of a memset */
    volatile DRM_DWORD * pdwOut = f_pdwOut;
    while (f_cdwCount--)
    {
        *pdwOut++ = 0;
    }
    return f_pdwOut;
}

DRM_ALWAYS_INLINE DRM_BOOL DRM_CALL DRMCRT_LocalAreEqual(
    __in_bcount( f_cbCount ) const  DRM_VOID  *f_pLHS,
    __in_bcount( f_cbCount ) const  DRM_VOID  *f_pRHS,
    __in                            DRM_DWORD  f_cbCount )
{
    /*
    ** Without volatile, some compilers can figure out this is an
    ** implementation of a memcmp-similar function or can
    ** optimize out checks which we don't want.
    */
    volatile DRM_DWORD dwResult = 0;

#if TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS

    /* Optimize: When unaligned pointers are allowed, compare in DRM_DWORD-sized groups. */
    volatile const DRM_DWORD *pLHS = (const DRM_DWORD*) f_pLHS;
    volatile const DRM_DWORD *pRHS = (const DRM_DWORD*) f_pRHS;

    while( f_cbCount >= sizeof(DRM_DWORD) )
    {
        dwResult |= (*pLHS ^ *pRHS);
        pLHS++;
        pRHS++;
        f_cbCount -= sizeof(DRM_DWORD);
    }

#else   /* TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS */

    volatile const DRM_BYTE *pLHS = (const DRM_BYTE*) f_pLHS;
    volatile const DRM_BYTE *pRHS = (const DRM_BYTE*) f_pRHS;

#endif  /* TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS */

    while( f_cbCount-- )
    {
PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_READ_OVERRUN_6385, "pLHS and pRHS both have length of f_cbCount and are properly checked before usage." )
        volatile const DRM_BYTE bLHS = ((const DRM_BYTE*)pLHS)[f_cbCount];
        volatile const DRM_BYTE bRHS = ((const DRM_BYTE*)pRHS)[f_cbCount];
PREFAST_POP /* __WARNING_READ_OVERRUN_6385 */
        dwResult |= (DRM_DWORD)( bLHS ^ bRHS );
    }

    return ( dwResult == 0 );
}

#if DRM_SUPPORT_INLINEDWORDCPY

DRM_ALWAYS_INLINE DRM_VOID * DRM_CALL DRMCRT_LocalDWORDcpy(
    __out_ecount_full( f_cdwCount )  DRM_DWORD  *f_pdwOut,
    __in_ecount( f_cdwCount ) const  DRM_DWORD  *f_pdwIn,
    __in                             DRM_DWORD  f_cdwCount )
{
    /* w/o volatile some compilers can figure out this is an
       implementation of a memcpy */
    volatile const DRM_DWORD * pdwIn = (const DRM_DWORD *) f_pdwIn;
    DRM_DWORD * pdwOut = (DRM_DWORD *) f_pdwOut;
    while (f_cdwCount--)
    {
        *pdwOut++ = *pdwIn++;
    }
    return pdwOut;
}
#endif  /* DRM_SUPPORT_INLINEDWORDCPY */

#endif /* defined(DRM_MSC_VER) || defined(__GNUC__) */

EXIT_PK_NAMESPACE_CODE;

