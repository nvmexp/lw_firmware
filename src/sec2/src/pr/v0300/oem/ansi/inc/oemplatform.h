/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef OEMPLATFORM_H
#define OEMPLATFORM_H

ENTER_PK_NAMESPACE;

/* OEM critical sectiion function. */
#if __MACINTOSH__
#if DRM_64BIT_TARGET
#define OEM_CRITICAL_SECTION_SIZE     8
#else
#define OEM_CRITICAL_SECTION_SIZE     4
#endif
#else
#define OEM_CRITICAL_SECTION_SIZE     24
#endif

//
// LWE (kwilson) Add mutex id - 8 bit value
//
typedef struct __tagOEM_CRITICAL_SECTION
{
#ifdef NONE
    DRM_BYTE rgb[OEM_CRITICAL_SECTION_SIZE];
#endif
    DRM_BYTE     mutexToken;
} OEM_CRITICAL_SECTION;

/**********************************************************************
** Function:    Oem_CritSec_Initialize
** Synopsis:    Initializes critical section.
** Arguments:   [f_pCS]--Pointer to OEM_CRITICAL_SECTION structure to be
**              initialized.
***********************************************************************/
DRM_API_VOID DRM_VOID DRM_CALL Oem_CritSec_Initialize(
    __inout OEM_CRITICAL_SECTION *f_pCS );

/**********************************************************************
** Function:    Oem_CritSec_Delete
** Synopsis:    Deletes existing critical section.
** Arguments:   [f_pCS]--Pointer to OEM_CRITICAL_SECTION structure to be
**              deleted.
***********************************************************************/
DRM_API_VOID DRM_VOID DRM_CALL Oem_CritSec_Delete(
    __inout OEM_CRITICAL_SECTION *f_pCS );

/**********************************************************************
** Function:    Oem_CritSec_Enter
** Synopsis:    Enters critical section.
** Arguments:   [f_pCS]--Pointer to OEM_CRITICAL_SECTION structure to be
**              entered.
***********************************************************************/
DRM_API_VOID DRM_VOID DRM_CALL Oem_CritSec_Enter(
    __inout OEM_CRITICAL_SECTION *f_pCS );

/**********************************************************************
** Function:    Oem_CritSec_Leave
** Synopsis:    Leaves critical section.
** Arguments:   [f_pCS]--Pointer to OEM_CRITICAL_SECTION structure to be
**              left.
***********************************************************************/
DRM_API_VOID DRM_VOID DRM_CALL Oem_CritSec_Leave(
    __inout OEM_CRITICAL_SECTION *f_pCS );

DRM_API_VOID DRM_VOID DRM_CALL Oem_Test_Mem_Alloc_Check_Leakscan( DRM_VOID );
DRM_API_VOID DRM_VOID DRM_CALL Oem_Test_Mem_Alloc_Clear_Leakscan( DRM_VOID );

EXIT_PK_NAMESPACE;

#endif  // OEMPLATFORM_H
