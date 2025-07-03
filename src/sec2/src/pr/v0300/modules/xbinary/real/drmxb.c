/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#define DRM_BUILDING_DRMXB_C
#include <drmcontextsizes.h>
#include <byteorder.h>
#include <drmxb.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;


/*********************************************************************
**
** Function: _XB_MapObjectTypeToEntryDescriptionIndex
**
** Synopsis: API that searches a format description table for a given object
**           type and returns the index in the table of that type.
**
** Arguments:
**
** [f_pFormat]         -- Pointer to a format description table
** [f_wType]           -- WORD object type that is being searched for
**
** Returns:               Integer index in the format table of the object type.
**
**********************************************************************/
#if defined(SEC_COMPILE)
DRM_WORD DRM_CALL _XB_MapObjectTypeToEntryDescriptionIndex(
    __in  const   DRM_XB_FORMAT_DESCRIPTION   *f_pFormat,
    __in          DRM_WORD                     f_wType )
{
    DRM_WORD    iEntry    = 0;

    for( iEntry = 0; iEntry < f_pFormat->cEntryDescriptions; iEntry++ )
    {
        if( f_pFormat->pEntryDescriptions[iEntry].wType == f_wType )
        {
            return iEntry;
        }
    }

    return 0;
}
#endif

/*********************************************************************
**
** Function: _XB_IsObjectTypeAChildOf
**
** Synopsis: API that searches a format description table for a given object
**           type of a given parent object type.  If the object type is found
**           and has the given parent type this function will return TRUE and
**           will assign the entry description of the search object (if the
**           parameter is not NULL).
**
** Arguments:
**
** [f_pFormat]         -- Pointer to a format description table
** [f_wType]           -- WORD object type that is being searched for
** [f_wParentType]     -- Parent type for the object being searched
** [f_ppEntry]         -- A reference to the entry description pointer for the
**                        searched object type.
**
** Returns:            TRUE  if the object type is found in the description table and
**                           its parent type matches the provided parent type.
**                     FALSE if the object type is NOT found in the description table
**                           or the the parent type does not match the provided parent
**                           type.
**
**********************************************************************/
#if defined(SEC_COMPILE)
DRM_BOOL DRM_CALL _XB_IsObjectTypeAChildOf(
    __in  const   DRM_XB_FORMAT_DESCRIPTION   *f_pFormat,
    __in          DRM_WORD                     f_wChildType,
    __in          DRM_WORD                     f_wParentType,
    __out_opt     DRM_XB_ENTRY_DESCRIPTION   **f_ppEntry )
{
    DRM_WORD    iEntry    = 0;

    InitOutputPtr( f_ppEntry, NULL );

    for( iEntry = 0; iEntry < f_pFormat->cEntryDescriptions; iEntry++ )
    {
        if( f_pFormat->pEntryDescriptions[iEntry].wType == f_wChildType )
        {
            if( f_pFormat->pEntryDescriptions[iEntry].wParent == f_wParentType )
            {
                if( f_ppEntry != NULL )
                {
                    *f_ppEntry = ( DRM_XB_ENTRY_DESCRIPTION * )&f_pFormat->pEntryDescriptions[iEntry];
                }
                return TRUE;
            }
        }
    }

    return FALSE;
}
#endif
/*********************************************************************
**
** Function: _XB_IsKnownObjectType
**
** Synopsis: API that searches a format description table for a given object
**           type and returns TRUE if the object type is in the table
**
** Arguments:
**
** [f_pFormat]         -- Pointer to a format description table
** [f_wType]           -- WORD object type that is being searched for
**
** Returns:            TRUE if the object type is found in the description table
**                     FALSE if the object type is NOT found in the description table
**
**********************************************************************/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_BOOL DRM_CALL _XB_IsKnownObjectType(
    __in  const   DRM_XB_FORMAT_DESCRIPTION   *f_pFormat,
    __in          DRM_WORD                     f_wType )
{
    DRM_WORD    iEntry    = 0;

    for( iEntry = 0; iEntry < f_pFormat->cEntryDescriptions; iEntry++ )
    {
        if( f_pFormat->pEntryDescriptions[iEntry].wType == f_wType )
        {
            return TRUE;
        }
    }

    return FALSE;
}
#endif
/*********************************************************************
**
** Function: _DRM_XB_AlignData
**
** Synopsis: API that aligns the buffer offset so that the data being parsed
**           is always aligned to the specified (f_eAlign) byte alignment.
**           The alignment is based on the offset from the start of the binary
**           message buffer and not the address of the object.
**
** Arguments:
**
** [f_eAlign   ]    -- The byte alignment enumeration value.
** [f_cbBuffer ]    -- The size of the data buffer in bytes.
** [f_piBuffer ]    -- The offset into the buffer for which to align to the
**                     nearest f_wAlign byte(s).
** [f_pcbPading]    -- Optional.  The number of padding bytes required to
**                     achieve the appropriate alignment.
**
** Returns:         DRM_SUCCESS if the alignment callwlation succeeded.
**                  DRM_E_BUFFER_BOUNDS_EXCEEDED if the alignment moves the
**                      index beyond the buffer size.
**                  DRM_E_ARITHMETIC_OVERFLOW if the alignment callwlation
**                      of the index overflowed or underflowed.
**
**********************************************************************/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_RESULT DRM_CALL _DRM_XB_AlignData(
    __in                            DRM_XB_ALIGNMENT    f_eAlign,
    __in                            DRM_DWORD           f_cbBuffer,
    __inout_ecount( 1 )             DRM_DWORD          *f_piBuffer,
    __out_opt                       DRM_DWORD          *f_pcbPadding )
{
    DRM_RESULT    dr      = DRM_SUCCESS;
    DRM_DWORD     ibData  = 0;
    DRM_DWORD     dwMask  = ( DRM_DWORD )f_eAlign - 1; /* This only works with power of 2, but we ensure this when checking the args */

    ChkArg( f_piBuffer != NULL );

    if( f_eAlign == XB_ALIGN_1_BYTE )
    {
        /* Nothing to do for 1 byte alignment */
        if( f_pcbPadding != NULL )
        {
            *f_pcbPadding = 0;
        }
        goto ErrorExit;
    }

    AssertChkArg( f_eAlign == XB_ALIGN_8_BYTE || f_eAlign == XB_ALIGN_16_BYTE );

    /*
    ** Round up to the nearest alignment value.
    ** alignedAddress = ( ( offset ) + ( alignment - 1 ) ) & ( ~( alignment - 1) );
    ** This works since the aligment values are ensured to be a power of 2.
    */
    ibData = *f_piBuffer;
    ChkDR( DRM_DWordAddSame( &ibData, dwMask ) );
    ibData = ibData & (~dwMask);

    ChkBOOL( ibData < f_cbBuffer, DRM_E_BUFFER_BOUNDS_EXCEEDED );

    if( f_pcbPadding != NULL )
    {
        *f_pcbPadding = ibData - *f_piBuffer; /* Underflow check not needed because ibData can only be larger than *f_piBuffer */
    }

    *f_piBuffer = ibData;

ErrorExit:
    return dr;
}
#endif
EXIT_PK_NAMESPACE_CODE;
