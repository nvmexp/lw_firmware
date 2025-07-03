/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
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

// Included libSPDM copyright, as file is LW-authored uses on libSPDM code.

/*!
 * @file   lw_device_secret.c
 * @brief  Stub implementations for unused libSPDM cryptographic functionality.
 *
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "spdm_device_secret_lib.h"
#include "lw_crypt.h"
#include "lw_cc_rts.h"
#include "spdm_common_lib.h"
#include "spdm_selwred_message_lib.h"
#include "spdm_selwred_message_lib_internal.h"
#include "spdm_device_secret_lib.h"

/* ------------------------- Global Variables ------------------------------- */
// TODO: This is a hardcoded AK for usage in testing.
static EC_CONTEXT g_akContext
    GCC_ATTRIB_SECTION("dmem_spdm", "g_akContext") =
{
    // Private Key
    {
        0x5ba4f499, 0x457bca7d, 0xc27278ba, 0x0b4baae3, 0x9fe8d6a2, 0xae59b19e,
        0x4f90871f, 0x2ff05bfb, 0x087b435a, 0x0084b060, 0xae76cfec, 0x30ee8197,
    },
    // Public Key
    {
        0xa83e46a0, 0xe8e5ed8b, 0xbbaeca86, 0x2f3a6e66, 0x75b47a93, 0x230b364a,
        0x8907eb40, 0xef913ec5, 0xd948a198, 0x4cdd9f77, 0x9d229e56, 0x4334d056,
        0x0148850a, 0x43953406, 0xd2fde128, 0x10f7a230, 0x1ecafed0, 0x4fc671ee,
        0x87b6638f, 0x4ec27af1, 0xbc3c637e, 0xd39ca181, 0x69593b6a, 0x078a60f9,
    },
};

/* ------------------- Spdm_device_secret_lib.h Functions ------------------- */
/**
  Collect the device measurement.

  @param  measurement_specification  Indicates the measurement specification.
                                     It must align with measurement_specification (SPDM_MEASUREMENT_BLOCK_HEADER_SPECIFICATION_*)
  @param  measurement_hash_algo      Indicates the measurement hash algorithm.
                                     It must align with measurement_hash_algo (SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_*)
  @param  device_measurement_count   The count of the device measurement block.
  @param  device_measurement         A pointer to a destination buffer to store the concatenation of all device measurement blocks.
  @param  device_measurement_size    On input, indicates the size in bytes of the destination buffer.
                                     On output, indicates the size in bytes of all device measurement blocks in the buffer.

  @retval TRUE  the device measurement collection success and measurement is returned.
  @retval FALSE the device measurement collection fail.
**/
boolean spdm_measurement_collection
(
    IN uint8      measurement_specification,
    IN uint32     measurement_hash_algo,
    OUT uint8    *device_measurement_count,
    OUT void     *device_measurement,
    IN OUT uintn *device_measurement_size
)
{
    spdm_measurement_block_dmtf_t *measurementBlock;
    LwU32                          hashSize;
    LwU8                           index;
    LwU32                          totalSize;
    LwU32                          dmtfBlockSize;
    LwU32                          dmtfHeaderSize;
    LwU8                           msrType;

    if (device_measurement_count == NULL || device_measurement == NULL ||
        device_measurement_size == NULL)
    {
        return LW_FALSE;
    }

    if (measurement_specification != SPDM_MEASUREMENT_BLOCK_HEADER_SPECIFICATION_DMTF)
    {
        return LW_FALSE;
    }

    if (measurement_hash_algo != SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA_384)
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

    for (index = 0; index < MAX_SPDM_MEASUREMENT_BLOCK_COUNT; index++)
    {
        measurementBlock->Measurement_block_common_header.index = index + 1;
        measurementBlock->Measurement_block_common_header.measurement_specification =
            SPDM_MEASUREMENT_BLOCK_HEADER_SPECIFICATION_DMTF;

        if (cc_rts_get_msr_type(index, &msrType) != RETURN_SUCCESS)
        {
            return LW_FALSE;
        }

        measurementBlock->Measurement_block_dmtf_header.dmtf_spec_measurement_value_type = msrType;
        measurementBlock->Measurement_block_dmtf_header.dmtf_spec_measurement_value_size = (uint16)hashSize;

        measurementBlock->Measurement_block_common_header.measurement_size = (LwU16)(dmtfHeaderSize +
            measurementBlock->Measurement_block_dmtf_header.dmtf_spec_measurement_value_size);

        if (!cc_rts_read_msr(index, (void *)(measurementBlock + 1), LW_FALSE))
        {
            return LW_FALSE;
        }

        measurementBlock = (void *)((uint8 *)measurementBlock + dmtfBlockSize + hashSize);
    }

    return LW_TRUE;
}

/**
  Sign an SPDM message data.

  @param  base_asym_algo  Indicates the signing algorithm.
  @param  bash_hash_algo  Indicates the hash algorithm.
  @param  signing_key     Selects the key used to sign the message.
  @param  message         A pointer to a message to be signed (before hash).
  @param  message_size    The size in bytes of the message to be signed.
  @param  signature       A pointer to a destination buffer to store the signature.
  @param  sig_size        On input, indicates the size in bytes of the destination buffer to store the signature.
                          On output, indicates the size in bytes of the signature in the buffer.

  @retval TRUE  signing success.
  @retval FALSE signing fail.
**/
boolean spdm_responder_data_sign
(
    IN uint32                base_asym_algo,
    IN uint32                bash_hash_algo,
    IN spdm_asym_sign_key_t  signing_key,
    IN const uint8          *message,
    IN uintn                 message_size,
    OUT uint8               *signature,
    IN OUT uintn            *sig_size
)
{
    //
    // TODO: GSP-CC CONFCOMP-575: Temp implementation to unblock E2E testing.
    // Sign a message using generated fake AK. Use same key for both key
    // scenarios, as GSP doesn't support multi-certificates yet.
    // 
    if (message == NULL || signature == NULL || sig_size == NULL ||
        message_size == 0 || *sig_size == 0)
    {
        return FALSE;
    }

    if (base_asym_algo != SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P384 ||
        bash_hash_algo != SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_384 ||
        *sig_size < ECC_P384_POINT_SIZE_IN_BYTES)
    {
        return FALSE;
    }

    zero_mem(signature, *sig_size);

    return spdm_asym_sign(base_asym_algo, bash_hash_algo, (void *)&g_akContext,
                          message, message_size, signature, sig_size);
}

