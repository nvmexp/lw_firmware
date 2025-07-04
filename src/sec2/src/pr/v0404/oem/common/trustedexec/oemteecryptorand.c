/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <oemteecrypto.h>
#include <oemcommon.h>
#include <drmlastinclude.h>

// LWE (nkuo): headers required when accessing LWPU's HW random number generator
#include "mem_hs.h"
#include "lwosselwreovly.h"

ENTER_PK_NAMESPACE_CODE;

#if defined(SEC_COMPILE)
/*
** OEM_MANDATORY:
** You MUST replace this function implementation with your own implementation that is specific
** to your PlayReady port if this function is still called by other OEM_TEE functions once your
** port is complete.
**
** You must provide a cryptographically-secure random number generator per the PlayReady
** Compliance and Robustness Rules.
**
** This function is used in all PlayReady scenarios.
**
** Synopsis:
**
** Operations Performed:
**
**  1. Generate a set of random bytes using a cryptographically-secure random number generator.
**
** Arguments:
**
** f_pContextAllowNULL:     (in/out) The TEE context returned from
**                                   OEM_TEE_BASE_AllocTEEContext.
**                                   This function may receive NULL.
**                                   This function may receive an
**                                   OEM_TEE_CONTEXT where
**                                   cbUnprotectedOEMData == 0 and
**                                   pbUnprotectedOEMData == NULL.
** f_pbData:                (out)    The randomly generated bytes.
** f_cbData:                (in)     The number of bytes to generate.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CRYPTO_RANDOM_GetBytes(
PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_PARAM_25004, "OEM_TEE_CONTEXT should never be const." )
    __inout_opt                                           OEM_TEE_CONTEXT              *f_pContextAllowNULL,
PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */
    __out_bcount( f_cbData )                              DRM_BYTE                     *f_pbData,
    __in                                                  DRM_DWORD                     f_cbData )
{
    return Oem_Random_GetBytes( NULL, f_pbData, f_cbData );
}

// LWE (nkuo) - This buffer is used by SCP engine so we can't put it in VA (i.e. can't be assigned to data overlay)
LwU8 g_randCtx[16]          GCC_ATTRIB_ALIGN(SCP_BUF_ALIGNMENT);
/**********************************************************************
** Function:    Oem_Random_GetRand128
**
** Synopsis:    Generate 128 bit random number using HW SCP
**              Copies the generated number in g_randCtx
**
** Returns:     void
**
***********************************************************************/
DRM_API DRM_VOID DRM_CALL Oem_Random_GetRand128(void)
{
    // load rand_ctx to SCP
    falc_scp_trap(TC_INFINITY);
    falc_trapped_dmwrite(falc_sethi_i((LwU32)g_randCtx, SCP_R5));

    // improve RNG quality by collecting entropy across
    // two conselwtive random numbers
    falc_scp_rand(SCP_R4);
    falc_scp_rand(SCP_R3);

    // trapped dmwait
    falc_dmwait();

    // mixing in previous whitened rand data
    falc_scp_xor(SCP_R5, SCP_R4);

    // use AES cipher to whiten random data
    falc_scp_key(SCP_R4);
    falc_scp_encrypt(SCP_R3, SCP_R3);

    // retrieve random data and update rand_ctx
    falc_trapped_dmread(falc_sethi_i((LwU32)g_randCtx, SCP_R3));
    falc_dmwait();

    // Reset trap to 0
    falc_scp_trap(0);
}

/*
** Synopsis:
**
** Generate the random number via LWPU's HW RNG in the size requested
** (Note that the RNG control registers should be configured and enabled before
** calling this function)
**
** Arguments:
**
** f_pOEMContext:    (in)  OEM specified context
** f_pbData:         (out) Buffer to hold the random bytes
** f_cbData:         (in)  Count of bytes to generate and fill in pbData
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
*/
DRM_API DRM_RESULT DRM_CALL Oem_Random_GetBytes(
    __in_opt                     DRM_VOID      *f_pOEMContext,
    __out_bcount(f_cbData)     DRM_BYTE      *f_pbData,
    __in                         DRM_DWORD      f_cbData)
{
    DRM_RESULT dr = DRM_SUCCESS;
    DRM_DWORD  cbBytesLeft = f_cbData;
    DRM_DWORD  cbBytesCopied = 0;
    DRM_DWORD  cbLwrrent = 0;


    ChkArg(f_pbData != NULL);
    ChkArg(f_cbData > 0);

    // Generate a sample rand number in g_randCtx
    Oem_Random_GetRand128();

    while (cbBytesLeft != 0)
    {
        // Generate next 16 bytes of random numbers in g_randCtx
        Oem_Random_GetRand128();

        // Copy the next lot of random numbers to output buffer
        cbLwrrent = DRM_MIN(cbBytesLeft, SCP_BUF_ALIGNMENT);
        OEM_SELWRE_MEMCPY(&f_pbData[cbBytesCopied], g_randCtx, cbLwrrent);
        cbBytesCopied += cbLwrrent;
        cbBytesLeft -= cbLwrrent;
    }

ErrorExit:
    return dr;
}

#endif

EXIT_PK_NAMESPACE_CODE;

