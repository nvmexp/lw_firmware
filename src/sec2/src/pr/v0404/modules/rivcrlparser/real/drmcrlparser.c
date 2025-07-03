/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#define DRM_BUILDING_DRMBCRLPARSER_C

#include <drmrivcrlparser.h>
#include <oembroker.h>
#include <drmmathsafe.h>
#include <drmprofile.h>
#include <oembyteorder.h>
#include <drmbcertformatparser.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;
#if defined (SEC_COMPILE)
/*****************************************************************************
** Function:    DRM_BCrl_ParseUnsignedCrl
**
** Synopsis:    Parses the unsigned portion of Playready CRL from binary to data structure.
**
** Arguments:   [f_pbCrlData]  : A pointer to the raw binary CRL data
**              [f_cbCrlData]  : Number of bytes in the raw binary CRL data
**              [f_cbSignedMessageLength] : Total length of the signed portion of data
**              [f_poCrl]      : A pointer to the structure to hold the parsed CRL data
**
** Returns:     DRM_SUCCESS      - on success
**              DRM_E_ILWALIDARG - if the output parm or the CRL data is NULL
**
** Notes:       The parser does not make copies of the DRM_RevocationEntry
**              data, it just points to them in the f_pbCrlData buffer.  Thus
**              the caller cannot free the f_pbCrlData buffer and still have a
**              valid f_poCrl data structure.
**
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCrl_ParseUnsignedCrl(
    __in_bcount(f_cbCrlData) const DRM_BYTE       *f_pbCrlData,
    __in                     const DRM_DWORD       f_cbCrlData,
    __out                    DRM_DWORD            *f_pcbSignedMessageLength,
    __out                    DRM_BCRL             *f_poCrl )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT  dr                      = DRM_SUCCESS;
    DRM_DWORD   dwOffset                = 0;
    DRM_DWORD   cbRevocationEntries     = 0;

    ChkArg( f_poCrl != NULL );
    ChkArg( f_pbCrlData != NULL);
    ChkArg( f_pcbSignedMessageLength != NULL);

    /*
    ** Retrieving crl header
    */
    SELWRE_COPY_FROMBUFFER( &f_poCrl->Identifier,
                            f_pbCrlData,
                            dwOffset,
                            sizeof(f_poCrl->Identifier),
                            f_cbCrlData );

    NETWORKBYTES_FROMBUFFER_TO_DWORD( f_poCrl->dwVersion,
                                      f_pbCrlData,
                                      dwOffset,
                                      f_cbCrlData );

    NETWORKBYTES_FROMBUFFER_TO_DWORD( f_poCrl->cRevocationEntries,
                                      f_pbCrlData,
                                      dwOffset,
                                      f_cbCrlData );
    /*
    ** Retrieving revocation entries
    */
    if ( f_poCrl->cRevocationEntries > 0 )
    {
        ChkBOOL( dwOffset < f_cbCrlData, DRM_E_BUFFER_BOUNDS_EXCEEDED );
        f_poCrl->Entries = (DRM_RevocationEntry*) ( f_pbCrlData + dwOffset );
    }
    else
    {
        f_poCrl->Entries = NULL;
    }
    ChkDR( DRM_DWordMult( f_poCrl->cRevocationEntries, sizeof( DRM_RevocationEntry ), &cbRevocationEntries ) );
    ChkDR( DRM_DWordAddSame( &dwOffset, cbRevocationEntries ) );

    ChkBOOL( dwOffset < f_cbCrlData, DRM_E_BUFFER_BOUNDS_EXCEEDED );

    *f_pcbSignedMessageLength = dwOffset;

ErrorExit:
    return dr;
}

/*****************************************************************************
** Function:    DRM_BCrl_ParseSignedCrl
**
** Synopsis:    Parses Playready CRL from binary to data structure up to its
**              signature. This function does NOT verify the CRL signature
**              itself - use DRM_BCrl_ParseCrl for that functionality
**
** Arguments:   [f_pbCrlData]  : A pointer to the raw binary CRL data
**              [f_cbCrlData]  : Number of bytes in the raw binary CRL data
**              [f_pcbSignedMessageLength]: Returns the length of data
**                                          that is signed.
**              [f_poCrl]      : A pointer to the structure to hold the parsed CRL data
**
** Returns:     DRM_SUCCESS      - on success
**              DRM_E_ILWALIDARG - if the output parm or the CRL data is NULL
**
** Notes:       The parser does not make copies of the DRM_RevocationEntry
**              data, it just points to them in the f_pbCrlData buffer.  Thus
**              the caller cannot free the f_pbCrlData buffer and still have a
**              valid f_poCrl data structure.
**
******************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCrl_ParseSignedCrl(
    __in_bcount(f_cbCrlData) const DRM_BYTE        *f_pbCrlData,
    __in                     const DRM_DWORD        f_cbCrlData,
    __out_ecount_opt(1)            DRM_DWORD       *f_pcbSignedMessageLength,
    __out                          DRM_BCRL_Signed *f_poCrl )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_DWORD dwOffset = 0;
    DRM_DWORD dwSignedMessageLength = 0;

    ChkArg( f_poCrl != NULL );
    ChkArg( f_pbCrlData != NULL);

    ChkDR( DRM_BCrl_ParseUnsignedCrl( f_pbCrlData,
                                      f_cbCrlData,
                                      &dwSignedMessageLength,
                                      &f_poCrl->Crl ) );

    dwOffset = dwSignedMessageLength;
    DRMASSERT( dwSignedMessageLength < f_cbCrlData );   /* Should have been ensured by DRM_BCrl_ParseUnsignedCrl */

    /*
    ** Retrieving signature
    */
    f_poCrl->Signature.bType = f_pbCrlData[dwOffset++];
    ChkArg( PLAYREADY_DRM_BCRL_SIGNATURE_TYPE == f_poCrl->Signature.bType );

    NETWORKBYTES_FROMBUFFER_TO_WORD( f_poCrl->Signature.cb,
                                     f_pbCrlData,
                                     dwOffset,
                                     f_cbCrlData );

    ChkArg( sizeof( SIGNATURE_P256 ) == f_poCrl->Signature.cb );

    SELWRE_COPY_FROMBUFFER( &f_poCrl->Signature.rgb,
                            f_pbCrlData,
                            dwOffset,
                            f_poCrl->Signature.cb,
                            f_cbCrlData );

    /*
    ** Retrieving certificate chain
    */
    ChkBOOL( dwOffset < f_cbCrlData, DRM_E_BUFFER_BOUNDS_EXCEEDED );
    f_poCrl->pbCertificateChain = (DRM_BYTE*)(f_pbCrlData + dwOffset);    /* Can't overflow: Verified by previous check */
    f_poCrl->cbCertificateChain = f_cbCrlData - dwOffset;   /* Can't underflow: Verified by previous check */

    if( f_pcbSignedMessageLength != NULL )
    {
        *f_pcbSignedMessageLength = dwSignedMessageLength;
    }

ErrorExit:
    return dr;
}
#endif

#ifdef NONE
static DRM_GLOBAL_CONST DRM_DWORD s_rgdwKeyUsageSet[] PR_ATTR_DATA_OVLY(_s_rgdwKeyUsageSet) = { DRM_BCERT_KEYUSAGE_SIGN_CRL }; /* verify that this certificate has the Sign Crl key usage value */
static DRM_RESULT DRM_CALL _GetPublicKeyNormalWorld(
    __in_opt                          const DRMFILETIME            *f_pftLwrrentTime,
    __in                                    DRM_DWORD               f_cbCertificateChain,
    __in_bcount(f_cbCertificateChain) const DRM_BYTE               *f_pbCertificateChain,
    __in_ecount_opt(1)                const PUBKEY_P256            *f_pRootPubkey,
    __inout                                 PUBKEY_P256            *f_pPublicSignKey )
{
    DRM_RESULT                      dr                      = DRM_SUCCESS;
    DRM_BCERTFORMAT_PARSER_CONTEXT  oParserCtx;             /* OEM_SELWRE_ZERO_MEMORY */
    DRM_BCERTFORMAT_CHAIN           oChain                  = {0};
    DRM_BCERTFORMAT_CERT            oCert                   = {0};
    DRM_DWORD                       cbStack                 = DRM_BCERTFORMAT_PARSER_STACK_SIZE_MIN;
    DRM_BYTE                       *pbStack                 = NULL;
    DRM_STACK_ALLOCATOR_CONTEXT     oStack;                 /* Initialized by DRM_STK_Init */

    ChkVOID( OEM_SELWRE_ZERO_MEMORY( &oParserCtx, sizeof( oParserCtx ) ) );

    DRMASSERT( f_pPublicSignKey != NULL );

    OEM_SELWRE_ZERO_MEMORY( f_pPublicSignKey, sizeof( *f_pPublicSignKey ) );

    ChkDR( DRM_BCERTFORMAT_InitializeParserContext(
        TRUE,                           /* f_fCheckSignature                */
        FALSE,                          /* f_fDontFailOnMissingExtData      */
        DRM_NO_OF(s_rgdwKeyUsageSet),   /* f_cRequiredKeyUsages             */
        s_rgdwKeyUsageSet,              /* f_pdwRequiredKeyUsages           */
        0,                              /* f_cResults                       */
        NULL,                           /* f_pResults                       */
        &oParserCtx ) );

    if( f_pRootPubkey != NULL )
    {
        ChkDR( DRM_BCERTFORMAT_OverrideRootPublicKey(
            &oParserCtx,
            f_pRootPubkey ) );
    }

    /*
    ** We are attempting to minimize the amount of memory used to parse a certificate chain.
    ** We do this by allocating a relatively small amount of memory (DRM_BCERTFORMAT_PARSER_STACK_SIZE_MIN)
    ** and attempting to parse the chain.  This will fail with DRM_E_OUTOFMEMORY if it is
    ** not enough space.  If this happens, we double the amount of memory we allocated
    ** and try again.  We repeat this until the amount of memory we would allocate exceeds
    ** a value which should work for all valid certificates (DRM_BCERTFORMAT_PARSER_STACK_SIZE_MAX).
    ** If we exceed that maximum, we actually fail parsing with DRM_E_OUTOFMEMORY.
    */
    do
    {
        ChkBOOL( cbStack <= DRM_BCERTFORMAT_PARSER_STACK_SIZE_MAX, DRM_E_OUTOFMEMORY );
        ChkVOID( Oem_Broker_MemFree( (DRM_VOID**)&pbStack ) );
        ChkDR( Oem_Broker_MemAlloc( cbStack, (DRM_VOID**)&pbStack ) );
        ChkDR( DRM_STK_Init( &oStack, pbStack, cbStack, TRUE ) );

        cbStack <<= 1;

        dr = DRM_BCERTFORMAT_ParseCertificateChain(
            &oParserCtx,
            NULL,
            f_pftLwrrentTime,               /* f_pftLwrrentTime                 */
            DRM_BCERT_CERTTYPE_CRL_SIGNER,  /* f_dwExpectedLeafCertType         */
            &oStack,
            f_cbCertificateChain,
            f_pbCertificateChain,
            NULL,
            &oChain,
            &oCert );
    } while( dr == DRM_E_OUTOFMEMORY );

    ChkDR( dr );

    /* retrieving key from certificate chain */
    ChkDR( DRM_BCERTFORMAT_GetPublicKeyFromCert( &oCert, f_pPublicSignKey ) );

ErrorExit:
    ChkVOID( Oem_Broker_MemFree( (DRM_VOID**)&pbStack ) );
    return dr;
}
#endif

#if defined(SEC_COMPILE)
static DRM_RESULT DRM_CALL _GetPublicKeySelwreWorld(
    __inout                                 DRM_VOID               *f_pOemTeeContext,
    __in_opt                          const DRMFILETIME            *f_pftLwrrentTime,
    __in                                    DRM_DWORD               f_cbCertificateChain,
    __in_bcount(f_cbCertificateChain) const DRM_BYTE               *f_pbCertificateChain,
    __inout                                 PUBKEY_P256            *f_pPublicSignKey )
{
    DRM_RESULT                         dr         = DRM_SUCCESS;
    DRM_DWORD                          cbStack    = DRM_BCERTFORMAT_PARSER_STACK_SIZE_MIN;
    DRM_BYTE                          *pbStack    = NULL;
    DRM_STACK_ALLOCATOR_CONTEXT        oStack;    /* Initialized by DRM_STK_Init */
    DRM_BCERTFORMAT_PARSER_CHAIN_DATA *pChainData = NULL;
    OEM_TEE_CONTEXT                   *pOemTeeCtx = DRM_REINTERPRET_CAST( OEM_TEE_CONTEXT, f_pOemTeeContext );

    DRMASSERT( f_pOemTeeContext != NULL );
    DRMASSERT( f_pPublicSignKey != NULL );

    ChkDR( Oem_Broker_MemAlloc( sizeof(*pChainData), ( DRM_VOID ** )&pChainData ) );

    /*
    ** We are attempting to minimize the amount of memory used to parse a certificate chain.
    ** We do this by allocating a relatively small amount of memory (DRM_BCERTFORMAT_PARSER_STACK_SIZE_MIN)
    ** and attempting to parse the chain.  This will fail with DRM_E_OUTOFMEMORY if it is
    ** not enough space.  If this happens, we double the amount of memory we allocated
    ** and try again.  We repeat this until the amount of memory we would allocate exceeds
    ** a value which should work for all valid certificates (DRM_BCERTFORMAT_PARSER_STACK_SIZE_MAX).
    ** If we exceed that maximum, we actually fail parsing with DRM_E_OUTOFMEMORY.
    */
    do
    {
        ChkBOOL( cbStack <= DRM_BCERTFORMAT_PARSER_STACK_SIZE_MAX, DRM_E_OUTOFMEMORY );
        ChkVOID( Oem_Broker_MemFree( (DRM_VOID**)&pbStack ) );
        ChkDR( Oem_Broker_MemAlloc( cbStack, (DRM_VOID**)&pbStack ) );
        ChkDR( DRM_STK_Init( &oStack, pbStack, cbStack, TRUE ) );

        cbStack <<= 1;

        dr = DRM_BCERTFORMAT_ParseCertificateChainData(
            pOemTeeCtx,
            f_pftLwrrentTime,
            DRM_BCERT_CERTTYPE_CRL_SIGNER,
            &oStack,
            f_cbCertificateChain,
            f_pbCertificateChain,
            pChainData,
            FALSE );

    } while( dr == DRM_E_OUTOFMEMORY );

    ChkDR( dr );

    ChkBOOL( DRM_BCERTFORMAT_PARSER_CHAIN_DATA_IS_KEY_VALID( pChainData->dwValidKeyMask, DRM_BCERTFORMAT_CHAIN_DATA_KEY__SIGN_CRL ), DRM_E_BCERT_ILWALID_KEY_USAGE );
    ChkVOID( OEM_SELWRE_MEMCPY( f_pPublicSignKey, &pChainData->rgoLeafPubKeys[DRM_BCERTFORMAT_CHAIN_DATA_KEY__SIGN_CRL], sizeof( *f_pPublicSignKey ) ) );

ErrorExit:
    ChkVOID( Oem_Broker_MemFree( ( DRM_VOID ** )&pbStack ) );
    ChkVOID( Oem_Broker_MemFree( ( DRM_VOID ** )&pChainData ) );
    return dr;
}

/**********************************************************************
**
** Function:    DRM_BCrl_VerifySignature
**
** Synopsis:    Verifies the PlayReady revinfov2 or binary crl signature.
**
** Arguments:
**              [f_pOemTeeContextAllowNULL]   - Optional OEM TEE context.
**              [f_pBigContext]               - Pointer to crypto context.
**              [f_pbSignedBytes]             - Specifies the bytes that are signed
**              [f_cbSignedBytes]             - Specifies the size of the signed bytes
**              [f_pbSignature]               - Specifies the signature
**              [f_cbSignature]               - Specifies the size signature
**              [f_pbCertificateChain]        - Specifies the certificate chain used to find signing key
**              [f_cbCertificateChain]        - Specifies the size certificate chain used to find signing key
**              [f_pRootPubkey]               - A pointer to public key to verify root certificate.
**
** Returns:     DRM_SUCCESS if the signature is valid
**              DRM_E_ILWALIDARG if the data in the passed arguments is invalid
**              passes return codes from other failures up to caller
**
**********************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_BCrl_VerifySignature(
    __inout_opt                             DRM_VOID               *f_pOemTeeContextAllowNULL,
    __inout_opt                             DRM_CRYPTO_CONTEXT     *f_pCryptoContext,
    __in_bcount(f_cbSignedBytes)      const DRM_BYTE               *f_pbSignedBytes,
    __in                                    DRM_DWORD               f_cbSignedBytes,
    __in_bcount(f_cbSignature)        const DRM_BYTE               *f_pbSignature,
    __in                                    DRM_DWORD               f_cbSignature,
    __in_bcount(f_cbCertificateChain) const DRM_BYTE               *f_pbCertificateChain,
    __in                                    DRM_DWORD               f_cbCertificateChain,
    __in_ecount_opt(1)                const PUBKEY_P256            *f_pRootPubkey )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT  dr       = DRM_SUCCESS;
    PUBKEY_P256 pub_key; /* OEM_SELWRE_ZERO_MEMORY */
    DRMFILETIME ftTime   = {0};

    ChkVOID( OEM_SELWRE_ZERO_MEMORY( &pub_key, sizeof( pub_key ) ) );

    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_REVOCATION, PERF_FUNC_DRM_BCrl_VerifySignature );

    /* verifying certificate chain if it's trusted */
    ChkDRAllowENOTIMPL( Oem_Broker_Clock_GetSystemTimeAsFileTime( f_pOemTeeContextAllowNULL, &ftTime ) );
    // LWE (dkumar): _GetPublicKeyNormalWorld is not expected to be used in TEE. Lwrrently, _GetPublicKeyNormalWorld
    // has a large stack consumption. Compiling it out to avoid interference with stack usage analysis.
#ifdef NONE
    if( f_pOemTeeContextAllowNULL == NULL )
    {
        ChkDR( _GetPublicKeyNormalWorld(
            &ftTime,
            f_cbCertificateChain,
            f_pbCertificateChain,
            f_pRootPubkey,
            &pub_key ) );
    }
    else
    {
#endif
        /* We should not override the root public key in the TEE */
        DRMASSERT( f_pRootPubkey == NULL );
        ChkDR( _GetPublicKeySelwreWorld(
            f_pOemTeeContextAllowNULL,
            &ftTime,
            f_cbCertificateChain,
            f_pbCertificateChain,
            &pub_key ) );
#ifdef NONE
    }
#endif

    /* verifying signature */
    ChkArg(sizeof(SIGNATURE_P256) == f_cbSignature);
    {
        SIGNATURE_P256 oSignature;

        OEM_SELWRE_MEMCPY( &oSignature, f_pbSignature, sizeof(oSignature) );
        ChkDR( Oem_Broker_ECDSA_P256_Verify(
            f_pOemTeeContextAllowNULL,
            ( struct bigctx_t * )f_pCryptoContext,
            f_pbSignedBytes,
            f_cbSignedBytes,
            &pub_key,
            &oSignature ) );
    }

ErrorExit:
    DRM_PROFILING_LEAVE_SCOPE;
    return dr;
}
#endif

EXIT_PK_NAMESPACE_CODE;

