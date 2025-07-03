/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#define DRM_BUILDING_DRMXMRFORMATPARSER_C

#include <oemcommon.h>
#include <oemcommonmem.h>
#include <drmbytemanip.h>
#include <drmconstants.h>
#include <drmxbparser.h>
#include <drmxmrformattypes.h>
#include <drmxmrformat_generated.h>
#include <drmlastinclude.h>

PRAGMA_STRICT_GS_PUSH_ON;

ENTER_PK_NAMESPACE_CODE;

#if defined (SEC_COMPILE)

#define XMRFORMAT_REMAP_XBINARY_ERROR_CODES(dr) DRM_DO {    \
    switch( (dr) )                                          \
    {                                                       \
        case DRM_E_XB_OBJECT_NOTFOUND:                      \
        case DRM_E_XB_ILWALID_OBJECT:                       \
        case DRM_E_XB_REQUIRED_OBJECT_MISSING:              \
        case DRM_E_XB_UNKNOWN_ELEMENT_TYPE:                 \
        case DRM_E_XB_ILWALID_VERSION:                      \
            (dr) = DRM_E_ILWALID_LICENSE;                   \
            break;                                          \
        default:                                            \
            break;                                          \
    }                                                       \
} DRM_WHILE_FALSE


#define VALIDATE_REQUIRED_OBJECT( pXmrLicense, IS_VALID_MACRO )             \
    ChkBOOL( IS_VALID_MACRO( pXmrLicense ), DRM_E_ILWALID_LICENSE )

/* All enabler containers must include one child which is the enabler type object, but no more than one of that type */
#define VALIDATE_ENABLERS( pXmrLicense, IS_VALID_MACRO, ObjPath, ContainerType, ObjectType ) DRM_DO {                   \
    if( IS_VALID_MACRO( pXmrLicense ) )                                                                                 \
    {                                                                                                                   \
        const DRM_XB_UNKNOWN_CONTAINER *pUnkContainer =                                                                 \
            &pXmrLicense->OuterContainer.ObjPath.UnknownContainer;                                                      \
        while( pUnkContainer != NULL )                                                                                  \
        {                                                                                                               \
            if( pUnkContainer->fValid )                                                                                 \
            {                                                                                                           \
                if( pUnkContainer->wType == (ContainerType) )                                                           \
                {                                                                                                       \
                    DRM_BOOL fHasEnabler = FALSE;                                                                       \
                    const DRM_XB_UNKNOWN_OBJECT *pUnkObject = pUnkContainer->pObject;                                   \
                    while( pUnkObject != NULL )                                                                         \
                    {                                                                                                   \
                        if( pUnkObject->fValid && ( pUnkObject->wType == (ObjectType) ) )                               \
                        {                                                                                               \
                            ChkBOOL( !fHasEnabler, DRM_E_ILWALID_LICENSE );                                             \
                            fHasEnabler = TRUE;                                                                         \
                        }                                                                                               \
                        pUnkObject = pUnkObject->pNext;                                                                 \
                    }                                                                                                   \
                    ChkBOOL( fHasEnabler, DRM_E_ILWALID_LICENSE );                                                      \
                }                                                                                                       \
            }                                                                                                           \
            pUnkContainer = pUnkContainer->pNext;                                                                       \
        }                                                                                                               \
    }                                                                                                                   \
} DRM_WHILE_FALSE


#define VALIDATE_RIGHTS_MASK( pXmrLicense, wValidMask )                                                                           \
    ChkBOOL(                                                                                                                      \
        ( (pXmrLicense)->OuterContainer.GlobalPolicyContainer.Rights.wValue == 0 )                                                \
     || ( (pXmrLicense)->OuterContainer.GlobalPolicyContainer.Rights.wValue                                                       \
     == ( (pXmrLicense)->OuterContainer.GlobalPolicyContainer.Rights.wValue & (DRM_WORD)(wValidMask) ) ), DRM_E_ILWALID_LICENSE )

static DRM_RESULT DRM_CALL _XMRFORMAT_ValidateLicenseObjects(
    __in    const DRM_XMRFORMAT                        *f_pXmrLicense )
{
    DRM_RESULT                         dr                             = DRM_SUCCESS;
    const DRM_XB_UNKNOWN_CONTAINER    *pUnknownContainer              = NULL;
    const DRM_XB_UNKNOWN_OBJECT       *pUnknownObject                 = NULL;
    DRM_GUID                           dguidFromObject                = DRM_EMPTY_DRM_GUID;
    DRM_BOOL                           fHasCopyEnabler                = FALSE;
    DRM_BOOL                           fHasDomainIlwalidCopyEnabler   = FALSE;
    /* These two GUIDs should be consistent with DRM_ACTION_COPY_TO_PC and DRM_ACTION_COPY_TO_DEVICE defined in drmconstants.h */
    DRM_GUID                           dguidActionCopyToPC            = { 0xCE480EDE, 0x516B, 0x40B3, { 0x90, 0xE1, 0xD6, 0xCF, 0xC4, 0x76, 0x30, 0xC5 } };
    DRM_GUID                           dguidActionCopyToDevice        = { 0x6848955D, 0x516B, 0x4EB0, { 0x90, 0xE8, 0x8F, 0x6D, 0x5A, 0x77, 0xB8, 0x5F } };

    ChkArg( f_pXmrLicense != NULL );

    /* Only XMR_VERSION_3 is supported */
    ChkBOOL( f_pXmrLicense->HeaderData.dwVersion == XMR_VERSION_3, DRM_E_ILWALID_LICENSE );

    /* RID (LID) is required: this also validates that the outer container is valid */
    VALIDATE_REQUIRED_OBJECT( f_pXmrLicense, XMRFORMAT_IS_RID_VALID );

    /* Validate the Global Policy Container Object */
    VALIDATE_REQUIRED_OBJECT( f_pXmrLicense, XMRFORMAT_IS_GLOBAL_POLICIES_VALID );
    VALIDATE_REQUIRED_OBJECT( f_pXmrLicense, XMRFORMAT_IS_SELWRITY_LEVEL_VALID );
    VALIDATE_REQUIRED_OBJECT( f_pXmrLicense, XMRFORMAT_IS_REVOCATION_INFORMATION_VERSION_VALID );

    /* Validate the Key Container Object */
    VALIDATE_REQUIRED_OBJECT( f_pXmrLicense, XMRFORMAT_IS_KEY_MATERIAL_VALID ); /* All licenses require key material */

    /* Validate the license doesn't have both uplink KID and uplinkX objects */
    ChkBOOL( !XMRFORMAT_IS_UPLINK_KID_VALID( f_pXmrLicense ) || !XMRFORMAT_IS_UPLINKX_VALID( f_pXmrLicense ), DRM_E_ILWALID_LICENSE );

    if( XMRFORMAT_IS_LEAF_LICENSE( f_pXmrLicense ) )
    {
        /* OptimizedContentKey2 is invalid on leaf licenses */
        ChkBOOL( !XMRFORMAT_IS_OPTIMIZED_CONTENT_KEY2_VALID( f_pXmrLicense ), DRM_E_ILWALID_LICENSE );
    }

    if( XMRFORMAT_IS_CONTENT_KEY_VALID( f_pXmrLicense ) )
    {
        /* Validate that the license has a valid encryption type */
        ChkBOOL(
            ( f_pXmrLicense->OuterContainer.KeyMaterialContainer.ContentKey.wSymmetricCipherType == XMR_SYMMETRIC_ENCRYPTION_TYPE_AES_128_CTR )
         || ( f_pXmrLicense->OuterContainer.KeyMaterialContainer.ContentKey.wSymmetricCipherType == XMR_SYMMETRIC_ENCRYPTION_TYPE_AES_128_CBC )
         || ( f_pXmrLicense->OuterContainer.KeyMaterialContainer.ContentKey.wSymmetricCipherType == XMR_SYMMETRIC_ENCRYPTION_TYPE_AES_128_ECB )
         || ( f_pXmrLicense->OuterContainer.KeyMaterialContainer.ContentKey.wSymmetricCipherType == XMR_SYMMETRIC_ENCRYPTION_TYPE_COCKTAIL    ), DRM_E_ILWALID_LICENSE );

        /* Validate that the content key is assigned */
        ChkBOOL( XBBA_TO_CB( f_pXmrLicense->OuterContainer.KeyMaterialContainer.ContentKey.xbbaEncryptedKey )  > 0   , DRM_E_ILWALID_LICENSE  );
        ChkBOOL( XBBA_TO_PB( f_pXmrLicense->OuterContainer.KeyMaterialContainer.ContentKey.xbbaEncryptedKey ) != NULL, DRM_E_ILWALID_LICENSE  );
    }

    if( XMRFORMAT_IS_DOMAIN_ID_VALID( f_pXmrLicense ) )
    {
        /*
        ** Make sure the license doesn't have an uplink KID or uplinkX, because leaf
        ** licenses cannot be domain-bound
        */
        ChkBOOL( !XMRFORMAT_IS_UPLINK_KID_VALID( f_pXmrLicense ), DRM_E_ILWALID_LICENSE );
        ChkBOOL( !XMRFORMAT_IS_UPLINKX_VALID( f_pXmrLicense ), DRM_E_ILWALID_LICENSE );
    }

    /* Validate the rights mask values */
    if( XMRFORMAT_IS_RIGHTS_VALID( f_pXmrLicense ) )
    {
        VALIDATE_RIGHTS_MASK(
            f_pXmrLicense,
            ( XMR_RIGHTS_CANNOT_PERSIST
            | XMR_RIGHTS_CANNOT_BIND_LICENSE
            | XMR_RIGHTS_TEMP_STORE_ONLY ) );
    }

    /* Validate if the license has a uplinkx then it has to have a valid UplinkKID pointer in the uplinkx object */
    if ( XMRFORMAT_IS_UPLINKX_VALID( f_pXmrLicense ) )
    {
        DRM_GUID guidEmpty   = DRM_EMPTY_DRM_GUID;

        ChkBOOL( !OEM_SELWRE_ARE_EQUAL( f_pXmrLicense->OuterContainer.KeyMaterialContainer.UplinkX.idUplinkKID.rgb, &guidEmpty, sizeof( DRM_GUID ) ), DRM_E_ILWALID_LICENSE );
    }

    /* Validate the Playback Policy Container Object */
    VALIDATE_ENABLERS( f_pXmrLicense, XMRFORMAT_IS_PLAY_VALID, PlaybackPolicyContainer, DRM_XMRFORMAT_ID_PLAY_ENABLER_CONTAINER, DRM_XMRFORMAT_ID_PLAY_ENABLER_OBJECT );

    /* Validate the Copy Policy Container Object */
    VALIDATE_ENABLERS( f_pXmrLicense, XMRFORMAT_IS_COPY_VALID, CopyPolicyContainer, DRM_XMRFORMAT_ID_COPY_ENABLER_CONTAINER, DRM_XMRFORMAT_ID_COPY_ENABLER_OBJECT );

    if( XMRFORMAT_IS_COPY_VALID( f_pXmrLicense ) )
    {
        /*
        ** If we have a copy container 2 object,
        ** we MUST have at least one copy enabler under it.
        ** This block verifies that fact.
        */
        pUnknownContainer = &f_pXmrLicense->OuterContainer.CopyPolicyContainer.UnknownContainer;
        while( ( pUnknownContainer != NULL ) && pUnknownContainer->fValid )
        {
            if( pUnknownContainer->wType == DRM_XMRFORMAT_ID_COPY_ENABLER_CONTAINER )
            {
                pUnknownObject = pUnknownContainer->pObject;
                while( ( pUnknownObject != NULL ) && pUnknownObject->fValid )
                {
                    if( pUnknownObject->wType == DRM_XMRFORMAT_ID_COPY_ENABLER_OBJECT )
                    {
                        /*
                        ** Check that the object contains a GUID
                        */
                        ChkBOOL( sizeof(DRM_GUID) == (pUnknownObject->cbData), DRM_E_ILWALID_LICENSE );

                        fHasCopyEnabler = TRUE;

                        DRM_BYT_CopyBytes(
                            &dguidFromObject,
                            0,
                            pUnknownObject->pbBuffer,
                            pUnknownObject->ibData,
                            sizeof(DRM_GUID) );
                        if( DRM_IDENTICAL_GUIDS( &dguidFromObject, &dguidActionCopyToPC )
                         || DRM_IDENTICAL_GUIDS( &dguidFromObject, &dguidActionCopyToDevice ) )
                        {
                            /*
                            ** Domain licenses can only have a copy container
                            ** if the only copy enabler(s) present are NEITHER
                            ** Copy To PC nor Copy To Device
                            */
                            fHasDomainIlwalidCopyEnabler = TRUE;
                        }
                    }
                    pUnknownObject = pUnknownObject->pNext;
                }
            }
            pUnknownContainer = pUnknownContainer->pNext;
        }

        ChkBOOL( fHasCopyEnabler, DRM_E_ILWALID_LICENSE );
    }

    if( XMRFORMAT_IS_DOMAIN_ID_VALID( f_pXmrLicense ) )
    {
        /*
        ** Make sure the license doesn't have one of the copy
        ** enablers which is invalid for domain-bound licenses
        */
        ChkBOOL( !fHasDomainIlwalidCopyEnabler, DRM_E_ILWALID_LICENSE );
    }

ErrorExit:
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_XMRFORMAT_Parse(
    __in                              DRM_DWORD                    f_cbXMR,
    __in_bcount( f_cbXMR )      const DRM_BYTE                    *f_pbXMR,
    __inout                           DRM_STACK_ALLOCATOR_CONTEXT *f_pStack,
    __out_opt                         DRM_DWORD                   *f_pcbParsed,
    __out                             DRM_XMRFORMAT               *f_pXmrLicense )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_pbXMR       != NULL );
    ChkArg( f_pXmrLicense != NULL );
    ChkArg( f_pStack      != NULL );

    OEM_SELWRE_MEMSET( f_pXmrLicense, 0, sizeof( *f_pXmrLicense ) );

    ChkDR( DRM_XB_UnpackBinary(
        f_pbXMR,
        f_cbXMR,
        f_pStack,
        s_DRM_XMRFORMAT_FormatDescription,
        DRM_NO_OF(s_DRM_XMRFORMAT_FormatDescription),
        NULL,
        f_pcbParsed,
        f_pXmrLicense ) );

    ChkDR( _XMRFORMAT_ValidateLicenseObjects( f_pXmrLicense ) );

ErrorExit:
    XMRFORMAT_REMAP_XBINARY_ERROR_CODES( dr );

    return dr;
}
#endif

#ifdef NONE
DRM_API DRM_RESULT DRM_CALL DRM_XMRFORMAT_RequiresSST(
    __in  const DRM_XMRFORMAT *f_pXmrLicense,
    __out       DRM_BOOL      *f_pfRequiresSST )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( NULL != f_pXmrLicense );
    ChkArg( NULL != f_pfRequiresSST );

    *f_pfRequiresSST = FALSE;

    if( XMRFORMAT_HAS_TIME_BASED_RESTRICTIONS( f_pXmrLicense )
     || XMRFORMAT_IS_COPYCOUNT_VALID( f_pXmrLicense )
     || XMRFORMAT_IS_METERING_VALID( f_pXmrLicense ) )
    {
        *f_pfRequiresSST = TRUE;
        goto ErrorExit;
    }

ErrorExit:
    return dr;
}
#endif

EXIT_PK_NAMESPACE_CODE;

PRAGMA_STRICT_GS_POP;

