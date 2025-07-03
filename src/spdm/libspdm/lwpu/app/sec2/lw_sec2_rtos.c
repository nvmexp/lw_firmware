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
 * @file   lw_sec2_rtos.c
 * @brief  Implementations for functions used to bridge SEC2-RTOS and SEC2
 *         libspdm implementation. For example, functions required for
 *         SEC2-RTOS to initialize libspdm on SEC2 module.
 */

/* ------------------------- System Includes ------------------------------- */
#include "base.h"
#include "lwrtos.h"

/* ------------------------- Application Includes -------------------------- */
#include "spdm_responder_lib_internal.h"
#include "lw_sec2_rtos.h"
#include "lw_utils.h"
#include "lw_apm_const.h"
#include "lw_apm_rts.h"
#include "lw_crypt.h"
#include "lw_device_secret.h"
#include "lw_measurements.h"
#include "lw_transport_layer.h"
#include "lib_intfcshahw.h"

/* ------------------------- Macros and Defines ---------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
  @brief  Initializes the SPDM task. This includes any sub-module init,
          such as crypt or transport layer init, as well as general
          libspdm init.

          Lwrrently, this function will also initialize capabilities
          and libspdm features that are specific to Ampere Protected Memory.
          An effort has been made to clearly identify these features, so that,
          if this function needs to be more generic in the future, the APM-related
          behavior can easily be separated into another function.

  @note  This function MUST be called before libspdm module can be used
         on SEC2 for any reason - such as processing messages, for example.
 
  @return  return_status representing result of task init.
 */
return_status spdm_sec2_initialize
(
    void  *pContext,
    LwU8  *pPayload,
    LwU32  payloadSize,
    LwU64  rtsOffset
)
{
    return_status         spdmStatus          = RETURN_SUCCESS;
    LwU8                  ctExponent          = 0;
    LwU32                 capFlags            = 0;
    LwU32                 baseAsymAlgo        = 0;
    LwU32                 baseHashAlgo        = 0;
    LwU16                 aeadSuite           = 0;
    LwU16                 dheGroup            = 0;
    LwU16                 keySched            = 0;
    LwU16                 reqAsymAlg          = 0;
    LwU8                  measSpec            = 0;
    LwU32                 measHashAlgo        = 0;
    LwU8                  slotCount           = 0;
    LwU8                 *pAkCertChain        = NULL;
    LwU32                 akCertChainSize     = 0;
    LwU8                 *pKxCertChain     = NULL;
    LwU32                 kxCertChainSize  = 0;
    spdm_data_parameter_t parameter;

    if (pContext == NULL || pPayload == NULL || payloadSize == 0 ||
        payloadSize < MAX_SPDM_MESSAGE_BUFFER_SIZE)
    {
        return RETURN_ILWALID_PARAMETER;
    }

    //
    // Initialize all sub-modules first.
    // We will initialize core-libspdm afterwards.
    //

    // Perform any crypt init.
    if (!spdm_ec_context_mutex_initialize())
    {
        spdmStatus = RETURN_DEVICE_ERROR;
        goto ErrorExit;
    }

    //
    // Generate keys and certificates
    // KX is lwrrently a copy of AK, so we generate AK first and then copy it to
    // KX.
    //

    // Initialize attestation key.
    if (!spdm_device_secret_initialize_ak())
    {
        spdmStatus = RETURN_DEVICE_ERROR;
        goto ErrorExit;
    }

    // Generate AK Certificate
    if (!spdm_generate_ak_certificate(&pAkCertChain, &akCertChainSize))
    {
        spdmStatus = RETURN_DEVICE_ERROR;
        goto ErrorExit;
    }

    // Perform KEY_EXCHANGE key init.
    if (!spdm_device_secret_initialize_kx())
    {
        spdmStatus = RETURN_DEVICE_ERROR;
        goto ErrorExit;
    }

    // Generate KEY_EXCHANGE Certificate
    if (!spdm_generate_kx_certificate(&pKxCertChain, &kxCertChainSize,
                                           pAkCertChain, akCertChainSize))
    {
        spdmStatus = RETURN_DEVICE_ERROR;
        goto ErrorExit;
    }
    
    // Perform any measurements init.
    apm_rts_offset_initialize(rtsOffset);

    // Perform any transport layer init.
    if (!spdm_transport_layer_initialize(pPayload, payloadSize))
    {
        spdmStatus = RETURN_DEVICE_ERROR;
        goto ErrorExit;
    }

    //
    // After sub-module init complete, initialize core-libspdm code.
    // This ilwolves setting information about responder, such as
    // capabilities, as well as number and location of certificates chains.
    //

    //
    // Initialize all SPDM Requester attributes to APM values. In the future,
    // if we want these to be configurable, make these values parameters, with
    // certain values being optional, based on capabilities passed in.
    //
    ctExponent   = APM_SPDM_CT_EXPONENT;
    capFlags     = APM_SPDM_CAPABILITY_FLAGS;
    baseAsymAlgo = APM_SPDM_BASE_ASYM_ALGO;
    baseHashAlgo = APM_SPDM_BASE_HASH_ALGO;
    aeadSuite    = APM_SPDM_AEAD_SUITE;
    dheGroup     = APM_SPDM_DHE_GROUP;
    keySched     = APM_SPDM_KEY_SCHEDULE;
    reqAsymAlg   = APM_SPDM_BASE_ASYM_ALGO;
    measSpec     = APM_SPDM_MEASUREMENT_SPEC;
    measHashAlgo = APM_SPDM_MEASUREMENT_HASH_ALGO;
    slotCount    = APM_SPDM_CERT_SLOT_COUNT;

    // Initialize libspdm context struct
    spdm_init_context(pContext);

    // Set requester attributes.
    memset(&parameter, 0, sizeof(parameter));
    parameter.location = SPDM_DATA_LOCATION_LOCAL;

    CHECK_SPDM_STATUS(spdm_set_data(pContext, SPDM_DATA_CAPABILITY_CT_EXPONENT,
                                    &parameter, &ctExponent, sizeof(ctExponent)));

    CHECK_SPDM_STATUS(spdm_set_data(pContext, SPDM_DATA_CAPABILITY_FLAGS,
                                    &parameter, &capFlags, sizeof(capFlags)));

    CHECK_SPDM_STATUS(spdm_set_data(pContext, SPDM_DATA_BASE_ASYM_ALGO,
                                    &parameter, &baseAsymAlgo,
                                    sizeof(baseAsymAlgo)));

    CHECK_SPDM_STATUS(spdm_set_data(pContext, SPDM_DATA_BASE_HASH_ALGO,
                                    &parameter, &baseHashAlgo,
                                    sizeof(baseHashAlgo)));

    CHECK_SPDM_STATUS(spdm_set_data(pContext, SPDM_DATA_AEAD_CIPHER_SUITE,
                                    &parameter, &aeadSuite, sizeof(aeadSuite)));

    CHECK_SPDM_STATUS(spdm_set_data(pContext, SPDM_DATA_DHE_NAME_GROUP,
                                    &parameter, &dheGroup, sizeof(dheGroup)));

    CHECK_SPDM_STATUS(spdm_set_data(pContext, SPDM_DATA_KEY_SCHEDULE,
                                    &parameter, &keySched, sizeof(keySched)));

    CHECK_SPDM_STATUS(spdm_set_data(pContext, SPDM_DATA_REQ_BASE_ASYM_ALG,
                                    &parameter, &reqAsymAlg,
                                    sizeof(reqAsymAlg)));

    CHECK_SPDM_STATUS(spdm_set_data(pContext, SPDM_DATA_MEASUREMENT_SPEC,
                                    &parameter, &measSpec, sizeof(measSpec)));

    CHECK_SPDM_STATUS(spdm_set_data(pContext, SPDM_DATA_MEASUREMENT_HASH_ALGO,
                                    &parameter, &measHashAlgo,
                                    sizeof(measHashAlgo)));

    // Store certificate slot count before storing certificate.
    CHECK_SPDM_STATUS(spdm_set_data(pContext, SPDM_DATA_LOCAL_SLOT_COUNT,
                                    &parameter, &slotCount, sizeof(slotCount)));

    //
    // Specify certificate location, passing slot number as well.
    // Set up the key exchange certificate
    //
    parameter.additional_data[0] = APM_SPDM_CERT_SLOT_ID_KX_CERT;
    CHECK_SPDM_STATUS(spdm_set_data(pContext, SPDM_DATA_LOCAL_PUBLIC_CERT_CHAIN,
                                    &parameter, pKxCertChain, kxCertChainSize));


    // Set up the attestation certificate
    parameter.additional_data[0] = APM_SPDM_CERT_SLOT_ID_AK_CERT;
    CHECK_SPDM_STATUS(spdm_set_data(pContext, SPDM_DATA_LOCAL_PUBLIC_CERT_CHAIN,
                                    &parameter, pAkCertChain, akCertChainSize));
    // Ensure we clear after usage.
    parameter.additional_data[0] = 0;

    //
    // Initialize any APM-specific functionality.
    // APM-TODO: Implement and call a separate APM-initialization function.
    //

ErrorExit:

    return spdmStatus;
}
