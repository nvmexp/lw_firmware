/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <oemtee.h>
#include <oemteeinternal.h>
#include <drmlastinclude.h>

#include "sec2_objpr.h"

ENTER_PK_NAMESPACE_CODE;
#if defined (SEC_COMPILE)
/* Default Version: 1.0.0.0 */
#define OEM_TEE_VERSION_MAJOR   1
#define OEM_TEE_VERSION_MINOR   0
#define OEM_TEE_VERSION_RELEASE 0
#define OEM_TEE_VERSION_BUILD   0

/*
** OEM_MANDATORY:
** You MUST replace this function implementation with your own implementation that is
** specific to your PlayReady port.
** You MUST return correct versioning information for your TEE implementation.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function returns the version of your TEE firmware.
**
** Operations Performed:
**
**  1. Return version and property information regarding the TEE.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pdwVersion:         (out)    Version as four byte integer value.
**                                The first  byte is the major   version.
**                                The second byte is the minor   version.
**                                The third  byte is the release version.
**                                The forth  byte is the build   version.
**                                You MUST set this parameter as appropriate for your
**                                implementation.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_GetVersion(
    __inout                                           OEM_TEE_CONTEXT          *f_pContext,
    __out                                             DRM_DWORD                *f_pdwVersion )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_pdwVersion != NULL );

    //
    // LWE (kwilson) For our implementation, these values will only be used for informational
    // purposes. Per MS, for ports that use local provisioning the implementation of this
    // function is entirely up to the OEM. Recommendations are, however, the values always
    // increase and the bytes of the DWORD follow the guidelines in the function header
    // comments. The TEE version should be tied to the actual firmware image and not any
    // particular hardware.  Hardware versions should be identified by different model numbers.
    //
    // Per recommendations, we will use the default values for first release.
    //
    ( (DRM_BYTE*)f_pdwVersion )[ 0 ] = OEM_TEE_VERSION_MAJOR;
    ( (DRM_BYTE*)f_pdwVersion )[ 1 ] = OEM_TEE_VERSION_MINOR;
    ( (DRM_BYTE*)f_pdwVersion )[ 2 ] = OEM_TEE_VERSION_RELEASE;
    ( (DRM_BYTE*)f_pdwVersion )[ 3 ] = OEM_TEE_VERSION_BUILD;

ErrorExit:
    return dr;
}

/*
** OEM_MANDATORY:
** You MUST replace this function implementation with your own implementation that is specific to
** your PlayReady port.
** You MUST return correct versioning and property information for your TEE implementation.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** This function returns version and property information regarding the TEE.
**
** Operations Performed:
**
**  1. Return version and property information regarding the TEE.
**
** Arguments:
**
** f_pContext:               (in/out) The TEE context returned from
**                                    OEM_TEE_BASE_AllocTEEContext.
** f_pchManufacturerName:    (out)    Number of characters in manufacturer name string
**                                    This MUST include the string's null termination character.
** f_ppwszManufacturerName:  (out)    String representing the manufacturer name for this TEE.
**                                    This string MUST be null-terminated.
**                                    This string MUST be allocated with OEM_TEE_BASE_SelwreMemAlloc
**                                    with a size equal to ( *f_pchManufacturerName ) * sizeof( DRM_WCHAR ).
**                                    This will be freed by the caller using OEM_TEE_BASE_SelwreMemFree.
** f_pchModelName:           (out)    Number of characters in model name string.
**                                    This MUST include the string's null termination character.
** f_ppwszModelName:         (out)    String representing the model name for this TEE.
**                                    This string MUST be null-terminated.
**                                    This string MUST be allocated with OEM_TEE_BASE_SelwreMemAlloc
**                                    with a size equal to ( *f_pchModelName ) * sizeof( DRM_WCHAR ).
**                                    This will be freed by the caller using OEM_TEE_BASE_SelwreMemFree.
** f_pchModelNumber:         (out)    Number of characters in model number string.
**                                    This MUST include the string's null termination character.
** f_ppwszModelNumber:       (out)    String representing the model number for this TEE.
**                                    This string MUST be null-terminated.
**                                    This string MUST be allocated with OEM_TEE_BASE_SelwreMemAlloc
**                                    with a size equal to ( *f_pchModelNumber ) * sizeof( DRM_WCHAR ).
**                                    This will be freed by the caller using OEM_TEE_BASE_SelwreMemFree.
** f_pdwFunctionMap:         (out)    Set of DRM_DWORD pairs which map from each value
**                                    in the DRM_METHOD_ID_DRM_TEE enum to an OEM-specific
**                                    DRM_DWORD.  The DRM_METHOD_ID_DRM_TEE_BASE_AllocTEEContext
**                                    ID MUST map to the value 0 and is not included in the Funcion Map.
**
**                                    If the high-order bit in this value is set to 0,
**                                    then OEM_TEE_PROXY_MethodIlwoke will be called with XBinary
**                                    serialized data which is expected to be marshalled as-is across
**                                    the secure boundary to DRM_TEE_STUB_HandleMethodRequest.
**
**                                    If the high-order bit in the OEM-specific value is set to 1
**                                    (i.e. OEM-specific value & DRM_TEE_PROXY_BIT_USE_STRUCT_PARAMS != 0),
**                                    then OEM_TEE_PROXY_MethodIlwoke will be called with two populated
**                                    structured serialization buffers.  The request buffer
**                                    will contain all of the input function parameters and the response
**                                    will contain empty parameters to be filled by the callee.
**
**                                    The DRM_TEE_* implementation inside the PlayReady PK requires
**                                    XBinary-serialized parameters and does not support receiving
**                                    structured serialized data, so the high-order bit should only be
**                                    set when you intend to implement structured serialization in
**                                    OEM_TEE_PROXY_MethodIlwoke (or in the case of interoperating with
**                                    the PlayReady Windows runtime and intercepting calls made between
**                                    the PlayReady PC runtime and the firmware).  Only method IDs for
**                                    which the DRM_METHOD_ID_DRM_TEE_SUPPORTS_STRUCTURE_SERIALIZATION
**                                    macro returns TRUE may have their high-order bit set to 1.
**                                    This macro MUST NOT be modified.
**
**                                    If the 3rd bit in the OEM-specific value is set to 1
**                                    (i.e. OEM-specific value & DRM_TEE_PROXY_BIT_API_UNSUPPORTED != 0),
**                                    then the OEM is declaring this API as unsupported.  Code running
**                                    outside the TEE will never attempt to ilwoke an unsupported API.
**
**                                    The remaining 6 bits of the high-order byte are reserved
**                                    by Microsoft and MUST be set to 0.
**
**                                    The mapped OEM-specific DRM_DWORD is also passed to
**                                    OEM_TEE_PROXY_MethodIlwoke.
**
** f_pcbProperties:          (out)    Number of values from the DRM_TEE_PROPERTY enum.
** f_ppbProperties:          (out)    Values from the DRM_TEE_PROPERTY enum.
**                                    This should be allocated with OEM_TEE_BASE_SelwreMemAlloc
**                                    with a number of bytes equal to DRM_TEE_PROPERTY_ARRAY_SIZE.
**                                    Then, this should then be zero-initialized.
**                                    Then, any properties from the DRM_TEE_PROPERTY should
**                                    be set by calling DRM_TEE_PROPERTY_SET_VALUE.
**                                    This will be freed by the caller using OEM_TEE_BASE_SelwreMemFree.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_BASE_GetVersionInformation(
    __inout                                           OEM_TEE_CONTEXT          *f_pContext,
    __out                                             DRM_DWORD                *f_pchManufacturerName,
    __deref_out_ecount( *f_pchManufacturerName )      DRM_WCHAR               **f_ppwszManufacturerName,
    __out                                             DRM_DWORD                *f_pchModelName,
    __deref_out_ecount( *f_pchModelName )             DRM_WCHAR               **f_ppwszModelName,
    __out                                             DRM_DWORD                *f_pchModelNumber,
    __deref_out_ecount( *f_pchModelNumber )           DRM_WCHAR               **f_ppwszModelNumber,
    __out_ecount( DRM_TEE_METHOD_FUNCTION_MAP_COUNT ) DRM_DWORD                *f_pdwFunctionMap,
    __out                                             DRM_DWORD                *f_pcbProperties,
    __deref_out_bcount( *f_pcbProperties )            DRM_BYTE                **f_ppbProperties )
{
    DRM_RESULT  dr              = DRM_SUCCESS;
    DRM_DWORD   idx             = 0;
    DRM_DWORD   cbProperties    = DRM_TEE_PROPERTY_ARRAY_SIZE;
    DRM_BYTE   *pbProperties    = NULL;

    DRM_DWORD   cchManufacturerName    = 0;
    DRM_WCHAR  *pwszManufacturerNameLW = NULL;
    DRM_WCHAR  *pwszManufacturerName   = NULL;
    DRM_DWORD   cchModelName           = 0;
    DRM_WCHAR  *pwszModelNameLW        = NULL;
    DRM_WCHAR  *pwszModelName          = NULL;
    DRM_DWORD   cchModelNumber         = 0;
    DRM_WCHAR  *pwszModelNumberLW      = NULL;
    DRM_WCHAR  *pwszModelNumber        = NULL;

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );

    DRMASSERT( f_pchManufacturerName    != NULL );
    DRMASSERT( f_ppwszManufacturerName  != NULL );
    DRMASSERT( f_pchModelName           != NULL );
    DRMASSERT( f_ppwszModelName         != NULL );
    DRMASSERT( f_pchModelNumber         != NULL );
    DRMASSERT( f_ppwszModelNumber       != NULL );
    DRMASSERT( f_pdwFunctionMap         != NULL );
    DRMASSERT( f_pcbProperties          != NULL );
    DRMASSERT( f_ppbProperties          != NULL );

    // Get Lwpu's TEE version information
    prGetTeeVersionInfo_HAL(&PrHal, &pwszManufacturerNameLW, &cchManufacturerName, &pwszModelNameLW, &cchModelName, &pwszModelNumberLW, &cchModelNumber);

    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( f_pContext, ( cchManufacturerName * sizeof( DRM_WCHAR ) ), (DRM_VOID**)&pwszManufacturerName ) );
    ChkVOID( OEM_TEE_MEMCPY( pwszManufacturerName, pwszManufacturerNameLW, ( cchManufacturerName * sizeof( DRM_WCHAR ) ) ) );

    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( f_pContext, ( cchModelName * sizeof( DRM_WCHAR ) ), (DRM_VOID**)&pwszModelName ) );
    ChkVOID( OEM_TEE_MEMCPY( pwszModelName, pwszModelNameLW, ( cchModelName * sizeof( DRM_WCHAR ) ) ) );

    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( f_pContext, ( cchModelNumber * sizeof( DRM_WCHAR ) ), (DRM_VOID**)&pwszModelNumber ) );
    ChkVOID( OEM_TEE_MEMCPY( pwszModelNumber, pwszModelNumberLW, ( cchModelNumber * sizeof( DRM_WCHAR ) ) ) );

    ChkDR( OEM_TEE_BASE_SelwreMemAlloc( f_pContext, cbProperties, (DRM_VOID**)&pbProperties ) );
    ChkVOID( OEM_TEE_ZERO_MEMORY( pbProperties, cbProperties ) );  /* Note: This is REQUIRED.  All unset bits MUST be zero. */

    //
    // LWE (kwilson) Lwpu does not support a clock inside the TEE
    // LWE (nkuo) - set the properties that LW supports
    //
    DRM_TEE_PROPERTY_SET_VALUE( cbProperties, pbProperties, DRM_TEE_PROPERTY_SUPPORTS_HEVC_HW_DECODING );
    DRM_TEE_PROPERTY_SET_VALUE( cbProperties, pbProperties, DRM_TEE_PROPERTY_SUPPORTS_PRE_PROCESS_ENCRYPTED_DATA );
    DRM_TEE_PROPERTY_SET_VALUE( cbProperties, pbProperties, DRM_TEE_PROPERTY_REQUIRES_SAMPLE_PROTECTION );
    DRM_TEE_PROPERTY_SET_VALUE( cbProperties, pbProperties, DRM_TEE_PROPERTY_SUPPORTS_SELWRE_HDCP_TYPE_1 );

    // 
    // LWE (kwilson) - We don't want the DEBUG TRACING property turned on because of DMEM usage
    // when enabled.  Make sure this never gets enabled.
    //
#ifdef NONE
#if DRM_DBG
    DRM_TEE_PROPERTY_SET_VALUE( cbProperties, pbProperties, DRM_TEE_PROPERTY_SUPPORTS_DEBUG_TRACING );
#endif /* DRM_DBG */
#endif

    /*
    ** Note: DRM_TEE_BASE_AllocTEEContext is NOT included in the count.
    ** Its function ID is ALWAYS 0 and it MUST use XBinary.
    */
    DRMCASSERT( DRM_METHOD_ID_DRM_TEE_Count == 37 );
    DRMCASSERT( DRM_TEE_METHOD_FUNCTION_MAP_COUNT == 72 );
    DRMCASSERT( DRM_METHOD_ID_DRM_TEE_BASE_AllocTEEContext == 0 );

    for( idx = 0; idx < DRM_METHOD_ID_DRM_TEE_Count - DRM_METHOD_ID_START_OF_OEM_FUNCTION_MAPPED_IDS; idx++ )
    {
        /* The default behavior is that the OEM function map value is equal to the PK function map value. */
        f_pdwFunctionMap[ idx * 2 ]   = idx + DRM_METHOD_ID_START_OF_OEM_FUNCTION_MAPPED_IDS;
        f_pdwFunctionMap[ idx * 2 + 1 ] = idx + DRM_METHOD_ID_START_OF_OEM_FUNCTION_MAPPED_IDS;
        
        // 
        // LWE (kwilson) - we need this code defined for structured serialization,
        // (removing #defines from around this code)
        //

        /* This if block would make DRM_TEE_H264_PreProcessEncryptedData use structured serialization. */
        if( f_pdwFunctionMap[ idx * 2 ] == DRM_METHOD_ID_DRM_TEE_H264_PreProcessEncryptedData )
        {
            f_pdwFunctionMap[ idx * 2 + 1 ] |= DRM_TEE_PROXY_BIT_USE_STRUCT_PARAMS;
        }

         /* This if block would make DRM_TEE_AES128CTR_DecryptAudioContentMultiple use structured serialization. */
        if( f_pdwFunctionMap[ idx * 2 ] == DRM_METHOD_ID_DRM_TEE_AES128CTR_DecryptAudioContentMultiple )
        {
            f_pdwFunctionMap[ idx * 2 + 1 ] |= DRM_TEE_PROXY_BIT_USE_STRUCT_PARAMS;
        }

         /* This if block would make DRM_TEE_AES128CTR_DecryptContent use structured serialization. */
        if( f_pdwFunctionMap[ idx * 2 ] == DRM_METHOD_ID_DRM_TEE_AES128CTR_DecryptContent)
        {
            f_pdwFunctionMap[ idx * 2 + 1 ] |= DRM_TEE_PROXY_BIT_USE_STRUCT_PARAMS;
        }
    }

    *f_pchManufacturerName   = cchManufacturerName;
    *f_ppwszManufacturerName = pwszManufacturerName;
    pwszManufacturerName = NULL;

    *f_pchModelName   = cchModelName;
    *f_ppwszModelName = pwszModelName;
    pwszModelName = NULL;

    *f_pchModelNumber   = cchModelNumber;
    *f_ppwszModelNumber = pwszModelNumber;
    pwszModelNumber = NULL;

    *f_pcbProperties = cbProperties;
    *f_ppbProperties = pbProperties;
    pbProperties = NULL;

ErrorExit:
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( f_pContext, (DRM_VOID**)&pwszManufacturerName ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( f_pContext, (DRM_VOID**)&pwszModelName ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( f_pContext, (DRM_VOID**)&pwszModelNumber ) );
    ChkVOID( OEM_TEE_BASE_SelwreMemFree( f_pContext, (DRM_VOID**)&pbProperties ) );
    return dr;
}
#endif
EXIT_PK_NAMESPACE_CODE;


