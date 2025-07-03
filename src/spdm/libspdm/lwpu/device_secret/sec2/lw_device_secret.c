/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/**
    Copyright Notice:
    Copyright 2021 DMTF. All rights reserved.
    License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
**/

// Included libspdm copyright, as file is LW-authored uses on libspdm code.

/*!
 * @file   lw_device_secret.c
 * @brief  Stub implementations for unused libspdm cryptographic functionality.
 *         APM-TODO CONFCOMP-400: Move this code to HS.
 */

/* ------------------------- System Includes -------------------------------- */
#include "base.h"
#include "lwuproc.h"
#include "lwoslayer.h"
#include "lwrtos.h"
#include "lwos_utility.h"
#include "lwos_ovl.h"

/* ------------------------- Application Includes --------------------------- */
#include "lw_device_secret.h"
#include "lw_sec2_rtos.h"
#include "lw_crypt.h"
#include "spdm_common_lib.h"
#include "spdm_selwred_message_lib.h"
#include "spdm_selwred_message_lib_internal.h"
#include "spdm_device_secret_lib.h"
#include "lw_apm_rts.h"
#include "lw_apm_const.h"
#include "lw_apm_spdm_common.h"
#include "apm/sec2_apm.h"

/* ------------------------ Macros and Defines ----------------------------- */

// The constant should match the longest hash
#define SPDM_EXPORT_MASTER_SECRET_MAX_SIZE_IN_BYTES  (64)

// The label used for SEC2 DMA secret key derivation
#define BIN_STR_SEC2_LABEL                           "sec2"
#define BIN_STR_SEC2_CONCAT_BUFFER_SIZE              (128)

// Struct to keep track of the signing keys used.
typedef struct
{
  LwU32 akPrivate[ECC_P256_PRIVKEY_SIZE_IN_DWORDS];
  LwU32 akPublic[ECC_P256_PUBKEY_SIZE_IN_DWORDS];
} SPDM_KEY_CONTEXT_T;

/* ------------------------- Global Variables ------------------------------- */
//
// Attestation key, serves as device key presented to requester.
// APM-TODO CONFCOMP-400: Move this to HS eventually.
//
static SPDM_KEY_CONTEXT_T g_ak
    GCC_ATTRIB_SECTION("dmem_spdm", "g_ak");

//
// Key exchange key, used to sign the key exchange.
// APM-TODO CONFCOMP-400: Move this to HS eventually.
//
static SPDM_KEY_CONTEXT_T g_kx
    GCC_ATTRIB_SECTION("dmem_spdm", "g_kx");

/* ---------------------- Lw_device_secret.h Functions ---------------------- */
/**
  Generate AK key pair.

  @note Must be called before any libspdm processing is done, as AK is
  used in multiple scenarios when signing is required.

  @retval TRUE   key generation success.
  @retval FALSE  key generation failed.
**/
boolean
spdm_device_secret_initialize_ak(void)
{
    return ec_generate_ecc_key_pair(g_ak.akPublic, g_ak.akPrivate);
}

/**
  Generate key exchange key pair.

  @note Must be called before any libspdm processing is done, but after the
        generation of AK.
        APM-TODO PID2: Implement unique key generation for KX.

  @retval TRUE   key generation success.
  @retval FALSE  key generation failed.
**/
boolean
spdm_device_secret_initialize_kx(void)
{
    copy_mem(&g_kx, &g_ak, sizeof(g_kx));
    return LW_TRUE;
}

/**
  Get Public AK.

  @note This function returns the AK in big-endian format.

  @param public_key       Buffer to receive Public AK.
  @param public_key_size  Size in Bytes of buffer public_key points to.

  @retval TRUE   Key generation success.
  @retval FALSE  Output buffer for key too small.
  @retval FALSE  Key generation failed.
**/
boolean
spdm_device_secret_get_public_ak
(
    OUT    uint8 *public_key,
    IN OUT uintn *public_key_size
)
{
    if (public_key == NULL || public_key_size == NULL ||
        *public_key_size < ECC_P256_PUBKEY_SIZE_IN_BYTES)
    {
        return LW_FALSE;
    }

    // Copy pub AK to callers buffer.
    copy_mem(public_key, g_ak.akPublic, ECC_P256_PUBKEY_SIZE_IN_BYTES); 
    *public_key_size = ECC_P256_PUBKEY_SIZE_IN_BYTES;

    return LW_TRUE;
}

/**
   @brief Derive the APM secret key used by the secure copy DMA and export it
          to global storage in a separate overlay.
   
   @param context       Opaque (void) pointer to the context structure
   @param session_id    The current session ID

   @retval LW_TRUE      Operation successful
   @retval LW_FALSE     Operation failed
  
 */
boolean
spdm_device_secret_derive_apm_sk
(
    IN void   *context,
    IN uint32  session_id
)
{
    // The SPDM data needed to fetch the export master secret
	spdm_selwred_message_context_t *pSessionContext = NULL;

    // The buffers used to store the export master secret
    LwU8  exportMasterSecret[SPDM_EXPORT_MASTER_SECRET_MAX_SIZE_IN_BYTES];
    LwU32 exportMasterSecretSize = SPDM_EXPORT_MASTER_SECRET_MAX_SIZE_IN_BYTES;

    // The binary string used for key derivation using HKDF-Expand
    LwU8 binStrSec2[BIN_STR_SEC2_CONCAT_BUFFER_SIZE];
    LwU32 binStrSec2Size = sizeof(binStrSec2);

    return_status status = RETURN_SUCCESS;
    boolean       retVal = LW_TRUE;

    LwU8 tempSecretKeyBuffer[APM_KEY_SIZE_IN_BYTES];

    if (context == NULL)
    {
        return LW_FALSE;
    }

    // Fetch the export master secret and check if the returned pointers are valid
    pSessionContext =
        spdm_get_selwred_message_context_via_session_id(context, session_id);

    if (pSessionContext == NULL)
    {
        return LW_FALSE;
    }

    if (spdm_selwred_message_export_master_secret(pSessionContext,
                                                  exportMasterSecret,
                                                  &exportMasterSecretSize)
                                                  != RETURN_SUCCESS)
    {
        return LW_FALSE;
    }

    //
    // Build the binary string for HKDF using the predefined label
    //
	status = spdm_bin_concat(BIN_STR_SEC2_LABEL, sizeof(BIN_STR_SEC2_LABEL) - 1,
				             NULL, APM_KEY_SIZE_IN_BYTES, 0, binStrSec2,
                             &binStrSec2Size);

	if (status != RETURN_SUCCESS)
    {
        return LW_FALSE;
    }

    //
    // Derive the key as HKDF-Expand(export_master_secret, binStrSec2, <key size>)
    // Key size needs to match the value used for the derivation of binStrSec2
    //
    retVal = spdm_hkdf_expand(pSessionContext->bash_hash_algo, 
                              exportMasterSecret, exportMasterSecretSize,
                              binStrSec2, binStrSec2Size, tempSecretKeyBuffer,
                              APM_KEY_SIZE_IN_BYTES);

    if (retVal != LW_TRUE)
    {
        return LW_FALSE;
    }

    //
    // Unload data overlay to create space for shared APM-SPDM overlay that
    // is used during key write. Ensure we reload overlay before exiting.
    //
    OSTASK_DETACH_DMEM_OVERLAY(OVL_INDEX_DMEM(spdm));
    if (apm_spdm_shared_write_data_key(tempSecretKeyBuffer,
                                       APM_SPDM_COMMON_MUTEX_TIMEOUT) != FLCN_OK)
    {
        retVal = LW_FALSE;
    }
    OSTASK_ATTACH_AND_LOAD_DMEM_OVERLAY(OVL_INDEX_DMEM(spdm));


    // Wipe the sensitive data from the stack
    zero_mem(tempSecretKeyBuffer, sizeof(tempSecretKeyBuffer));
    zero_mem(exportMasterSecret,  sizeof(exportMasterSecret));
  
    return retVal;
}

/* ------------------- Spdm_device_secret_lib.h Functions ------------------- */

/**
  Fill the measurement with the dummy value

  @param dst  The destination buffer                        
  @param idx  The index of the MSR being generated

  @retval LW_TRUE   Dummy measurement generation succeeded.
  @retval LW_FALSE  Dummy measurement generation failed.
**/
static boolean
spdm_generate_dummy_measurement
(
    OUT uint8* dst,
    IN  uint8  idx
)
{
    uint32 i = 0;
    dst[i++] = 0xf9;
    dst[i++] = 0x6d;
    dst[i++] = 0xcd;
    dst[i++] = 0x14;
    dst[i++] = 0x27;
    dst[i++] = 0x3d;
    dst[i++] = 0x3e;
    dst[i++] = 0xb0;
    dst[i++] = 0x67;
    dst[i++] = 0xb1;
    dst[i++] = 0x88;
    dst[i++] = 0xa7;
    dst[i++] = 0x53;
    dst[i++] = 0x6c;
    dst[i++] = 0x7c;
    dst[i++] = 0xe8;
    dst[i++] = 0xcd;
    dst[i++] = 0xe5;
    dst[i++] = 0x13;
    dst[i++] = 0x74;
    dst[i++] = 0x52;
    dst[i++] = 0x5d;
    dst[i++] = 0x0d;
    dst[i++] = 0x28;
    dst[i++] = 0x22;
    dst[i++] = 0xaa;
    dst[i++] = 0x8d;
    dst[i++] = 0x7a;
    dst[i++] = 0x53;
    dst[i++] = 0x42;
    dst[i++] = 0x48;
    dst[i++] =  idx;

    if (i != APM_HASH_SIZE_BYTE)
    {
        return LW_FALSE;
    }

    return LW_TRUE;
}

/**
  Collect the device measurement.

  @param measurement_specification  Indicates the measurement specification.
                                    It must align with measurement_specification (SPDM_MEASUREMENT_BLOCK_HEADER_SPECIFICATION_*)
  @param measurement_hash_algo      Indicates the measurement hash algorithm.
                                    It must align with measurement_hash_algo (SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_*)
  @param device_measurement_count   The count of the device measurement block.
  @param device_measurement         A pointer to a destination buffer to store the concatenation of all device measurement blocks.
  @param device_measurement_size    On input, indicates the size in bytes of the destination buffer.
                                    On output, indicates the size in bytes of all device measurement blocks in the buffer.

  @retval TRUE   The device measurement collection success and measurement is returned.
  @retval FALSE  The device measurement collection fail.
**/
boolean
spdm_measurement_collection
(
    IN     uint8   measurement_specification,
    IN     uint32  measurement_hash_algo,
    OUT    uint8  *device_measurement_count,
    OUT    void   *device_measurement,
    IN OUT uintn  *device_measurement_size
)
{
    spdm_measurement_block_dmtf_t *measurementBlock;
    LwU32                          hashSize;
    LwU8                           index;
    LwU32                          totalSize;
    LwU32                          dmtfBlockSize;
    LwU32                          dmtfHeaderSize;
    LwU8                           msrType;

    //
    // APM-TODO PID2: Prod-sign AHESASC and uncomment the section below
    //
    //LwBool                         bDynamicStateStatus;

    if (device_measurement_count == NULL || device_measurement == NULL ||
        device_measurement_size == NULL)
    {
        return LW_FALSE;
    }

    if (measurement_specification != SPDM_MEASUREMENT_BLOCK_HEADER_SPECIFICATION_DMTF)
    {
        return LW_FALSE;
    }

    if (measurement_hash_algo != SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA_256)
    {
        return LW_FALSE;
    }

    dmtfBlockSize = sizeof(spdm_measurement_block_dmtf_t);
    hashSize      = spdm_get_measurement_hash_size(measurement_hash_algo);
    if (hashSize == 0)
    {
        return LW_FALSE;
    }

    *device_measurement_count = MAX_SPDM_MEASUREMENT_BLOCK_COUNT;
    totalSize                 = (*device_measurement_count) * (dmtfBlockSize + hashSize);

    if (*device_measurement_size < totalSize)
    {
        return LW_FALSE;
    }
    *device_measurement_size = totalSize;
    measurementBlock         = device_measurement;
    dmtfHeaderSize           = sizeof(spdm_measurement_block_dmtf_header_t);

    //
    // Call the hook from the falcon to capture the dynamic measurements.
    // The hook relies on the overlay .dmem_apm.
    //
    // APM-TODO PID2: Prod-sign AHESASC and uncomment the section below
    //
    //OSTASK_ATTACH_AND_LOAD_DMEM_OVERLAY(OVL_INDEX_DMEM(apm));

    //bDynamicStateStatus = apmCaptureDynamicState(apm_get_rts_offset());

    //OSTASK_DETACH_DMEM_OVERLAY(OVL_INDEX_DMEM(apm));

    //if (bDynamicStateStatus != LW_TRUE)
    //{
    //    return LW_FALSE;
    //}

    for (index = 0; index < MAX_SPDM_MEASUREMENT_BLOCK_COUNT; index++)
    {
        measurementBlock->Measurement_block_common_header.index = index + 1;
        measurementBlock->Measurement_block_common_header.measurement_specification =
            SPDM_MEASUREMENT_BLOCK_HEADER_SPECIFICATION_DMTF;
        
        if (apm_rts_get_msr_type(index, &msrType) != RETURN_SUCCESS)
        {
            return LW_FALSE;
        }

        measurementBlock->Measurement_block_dmtf_header.dmtf_spec_measurement_value_type = msrType;
        measurementBlock->Measurement_block_dmtf_header.dmtf_spec_measurement_value_size = (uint16)hashSize;

        measurementBlock->Measurement_block_common_header.measurement_size = (LwU16)(dmtfHeaderSize +
            measurementBlock->Measurement_block_dmtf_header.dmtf_spec_measurement_value_size);

        // APM-TODO PID2: Prod-sign AHESASC and uncomment the section below
        //
        // Reading the shadow MSR here will return the correct state for dynamic
        // measurement MSRs. For static measurement MSRs the result is also
        // correct because MSR and MSRS (shadow) store the same value.
        //
        //if (!apm_rts_read_msr(index, (PAPM_MSR)(measurementBlock + 1), LW_TRUE))
        //{
        //    return LW_FALSE;
        //}

        // APM-TODO PID2: Remove dummy measurements
        if (!spdm_generate_dummy_measurement((uint8 *)(measurementBlock + 1), index+1))
        {
            return LW_FALSE;
        }
        measurementBlock = (void *)((uint8 *)measurementBlock + dmtfBlockSize + hashSize);
    }

    return LW_TRUE;
}

/**
  Fill the context with the signing key passed as an argument.

  @note Only supports keys defined in spdm_asym_sign_key_t. All keys must be
        generated before calling this function.

  @param signing_key  Selects the key used to sign the message.
  @param pContext     The destination where the keys are written

  @retval TRUE   Key fetch successfull.
  @retval FALSE  Key fetch failed.
**/
static boolean
spdm_fill_context_asym_signing_key
(
    IN     spdm_asym_sign_key_t     signing_key,
    OUT    EC_CONTEXT              *pContext
)
{
    if (pContext == NULL)
    {
        return LW_FALSE;
    }

    switch (signing_key)
    {
        case SPDM_ASYM_SIGN_KEY_KX:
            // Load context with key exchange key.
            copy_mem(pContext->ecPrivate, g_kx.akPrivate, sizeof(g_kx.akPrivate));
            copy_mem(pContext->ecPublic,  g_kx.akPublic,  sizeof(g_kx.akPublic));
            break;
            
        case SPDM_ASYM_SIGN_KEY_AK:
            // Load context with attestation key.
            copy_mem(pContext->ecPrivate, g_ak.akPrivate, sizeof(g_ak.akPrivate));
            copy_mem(pContext->ecPublic,  g_ak.akPublic,  sizeof(g_ak.akPublic));
            break;

        default:
            return LW_FALSE;
    }

    pContext->bInitialized = LW_TRUE;
    return LW_TRUE;
}

/**
  Sign an SPDM message data. Should not be used until the keys are initialized.
  Signature is output in big-endian format, as that is likely how Requester
  and any verifying components expect it.

  @note Only supports keys defined in spdm_asym_sign_key_t.

  AK must be initialized previously via call to spdm_device_secret_generate_ak.

  @param base_asym_algo  Indicates the signing algorithm.
  @param bash_hash_algo  Indicates the hash algorithm.
  @param signing_key     Selects the key used to sign the message.
  @param message         A pointer to a message to be signed (before hash).
  @param message_size    The size in bytes of the message to be signed.
  @param signature       A pointer to a destination buffer to store the signature.
  @param sig_size        On input, indicates the size in bytes of the destination buffer to store the signature.
                         On output, indicates the size in bytes of the signature in the buffer.

  @retval TRUE   signing success.
  @retval FALSE  signing fail.
**/
boolean
spdm_responder_data_sign
(
    IN     uint32                   base_asym_algo,
    IN     uint32                   bash_hash_algo,
    IN     spdm_asym_sign_key_t     signing_key,
    IN     const uint8             *message,
    IN     uintn                    message_size,
    OUT    uint8                   *signature,
    IN OUT uintn                   *sig_size
)
{
    EC_CONTEXT *pContext = NULL;
    LwBool      bSuccess = LW_FALSE;

    if (message == NULL || signature == NULL || sig_size == NULL)
    {
        return LW_FALSE;
    }

    if (base_asym_algo != SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P256 ||
        bash_hash_algo != SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_256             ||
        message_size == 0 || *sig_size < ECC_P256_POINT_SIZE_IN_BYTES)
    {
        return LW_FALSE;
    }

    // After success of ec_new_by_nid, we need to be sure to free context.
    pContext = ec_new_by_nid(CRYPTO_NID_SECP256R1);
    if (pContext == NULL)
    {
        return LW_FALSE;
    }

    if (!spdm_fill_context_asym_signing_key(signing_key, pContext))
    {
        return LW_FALSE;
    }

    // sig_size should be updated by spdm_asym_sign.
    bSuccess = spdm_asym_sign(base_asym_algo, bash_hash_algo, pContext,
                              message, message_size, signature, sig_size);
    if (!bSuccess)
    {
        goto ErrorExit;
    }

ErrorExit:
    // This also scrubs attestation key from context.
    ec_free(pContext);

    return bSuccess;
}
