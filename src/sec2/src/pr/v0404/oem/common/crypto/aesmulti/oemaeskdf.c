/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#define DRM_BUILDING_OEMAESMULTI_C
#include <drmtypes.h>
#include <drmmathsafe.h>
#include <oembyteorder.h>
#include <oem.h>
#include <oemaesmulti.h>
#include <oembroker.h>
#include <drmprofile.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

/*********************************************************************************************
**
** Function:  Oem_Aes_AES128KDFCTR_r8_L128
**
** Synopsis:  Implements AES-128 CTR Key Derivation Function as described in
** "Recommendation for Key Derivation Using Pseudorandom Functions", NIST Special Publication 800-108,
** http://csrc.nist.gov/publications/nistpubs/800-108/sp800-108.pdf
**
** We only derive AES128 keys which are 128 bits long ((i.e. 'L'==128)
** Our psueo-random function (PRF) is Oem_Omac1_Sign that has output size of 128 bits (i.e. 'h'==128),
** So we always perform only one iteration since 'L' == 'h' and therefore Ceil(L/h) == 'n' == 1.
**
** We choose to use 8 bits (one byte) for the size of the binary representation of 'i'. (r == 8)
**
** Our 'Label' and 'Context' are input GUID's specified by the user.
** If 'Context' is not specified, a value of all zeroes will be used.
** L == 128 for output of AES-128 keys and is word size.
**
** Arguments: [f_pDeriverKey]       : AES key used for derivation
**                                   - If this API is called inside the TEE. Then OEM_AES_KEY_CONTEXT will
**                                     be pointing to an OEM_TEE_KEY_AES.
**                                     OEM can colwert it to a DRM_AES_KEY using
**                                     OEM_TEE_INTERNAL_COLWERT_OEM_TEE_KEY_AES128_TO_DRM_AES_KEY.
**                                   - If this API is called from the normal world, then OEM_AES_KEY_CONTEXT
**                                     will be pointing to a DRM_AES_KEY. A simple cast to a DRM_AES_KEY*
**                                     will be sufficient.
**            [f_pidLabel]          : Label value of 16 bytes
**            [f_pidContext]        : Context value of 16 bytes
**            [f_pbDerivedKey]      : Derived 16 bytes
**
** Returns:   DRM_SUCCESS
**                The specified signature matches the computed signature
**            DRM_E_ILWALIDARG
**              One of the pointers is NULL
**********************************************************************************************/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Aes_AES128KDFCTR_r8_L128(
    __in_ecount( 1 )                                const OEM_AES_KEY_CONTEXT   *f_pDeriverKey,
    __in_ecount( 1 )                                const DRM_ID                *f_pidLabel,
    __in_ecount_opt( 1 )                            const DRM_ID                *f_pidContext,
    __out_ecount( DRM_AES_KEYSIZE_128 )                   DRM_BYTE              *f_pbDerivedKey )
{
    DRM_RESULT     dr                    = DRM_SUCCESS;
    const DRM_BYTE cIteration            = 1;
    const DRM_BYTE cSeparator            = 0;
    const DRM_WORD cLenInBitsReturlwalue = 128;
    DRM_ID         idZeroes              = {{0}};
    DRM_BYTE       rgbKeyDerivationInput[
        sizeof(cIteration)
      + sizeof(*f_pidLabel)
      + sizeof(cSeparator)
      + sizeof(*f_pidContext)
      + sizeof(cLenInBitsReturlwalue)] = {0};

    ChkArg( f_pDeriverKey  != NULL );
    ChkArg( f_pidLabel     != NULL );
    ChkArg( f_pbDerivedKey != NULL );

    /* Populate the input data iteration */
    rgbKeyDerivationInput[0] = cIteration;

    /* Populate the input data label */
    OEM_SELWRE_MEMCPY(
        &rgbKeyDerivationInput[sizeof(cIteration)],
        f_pidLabel,
        sizeof(*f_pidLabel) );

    /* Populate the input data separator */
    rgbKeyDerivationInput[sizeof(cIteration) + sizeof(*f_pidLabel)] = cSeparator;

    /* Populate the input data context */
    OEM_SELWRE_MEMCPY(
        &rgbKeyDerivationInput[sizeof(cIteration) + sizeof(*f_pidLabel) + sizeof(cSeparator)],
        f_pidContext != NULL ? f_pidContext : &idZeroes,
        sizeof(*f_pidContext) );

    /* Populate the input data derived length */
    WORD_TO_NETWORKBYTES(
        rgbKeyDerivationInput,
        sizeof(cIteration) + sizeof(*f_pidLabel) + sizeof(cSeparator) + sizeof(*f_pidContext),
        cLenInBitsReturlwalue );

    /* Generate the derived key */
    ChkDR( Oem_Omac1_Sign(
        f_pDeriverKey,
        rgbKeyDerivationInput,
        0,
        sizeof(rgbKeyDerivationInput),
        f_pbDerivedKey ) );

ErrorExit:
    return dr;
}
#endif
EXIT_PK_NAMESPACE_CODE;

