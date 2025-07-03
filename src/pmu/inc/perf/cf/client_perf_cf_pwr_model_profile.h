/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    client_perf_cf_pwr_model_profile.h
 * @brief   Common Client PERF_CF Pwr Model Profile related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef CLIENT_PERF_CF_PWR_MODEL_PROFILE_H
#define CLIENT_PERF_CF_PWR_MODEL_PROFILE_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct CLIENT_PERF_CF_PWR_MODEL_PROFILE CLIENT_PERF_CF_PWR_MODEL_PROFILE, CLIENT_PERF_CF_PWR_MODEL_PROFILE_BASE;

typedef struct CLIENT_PERF_CF_PWR_MODEL_PROFILES CLIENT_PERF_CF_PWR_MODEL_PROFILES;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Colwenience macro to look-up CLIENT_PERF_CF_PWR_MODEL_PROFILES.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_CLIENT_PERF_CF_PWR_MODEL_PROFILE))
#define PERF_CF_GET_CLIENT_PWR_MODEL_PROFILES() \
    (&(PERF_CF_GET_OBJ()->clientPwrModelProfiles))
#else
#define PERF_CF_GET_CLIENT_PWR_MODEL_PROFILES() \
    ((CLIENT_PERF_CF_PWR_MODEL_PROFILES*)NULL)
#endif

/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_CLIENT_PERF_CF_PWR_MODEL_PROFILE))
#define BOARDOBJGRP_DATA_LOCATION_CLIENT_PERF_CF_PWR_MODEL_PROFILE \
    (&(PERF_CF_GET_CLIENT_PWR_MODEL_PROFILES()->super.super))
#else
#define BOARDOBJGRP_DATA_LOCATION_CLIENT_PERF_CF_PWR_MODEL_PROFILE \
    ((BOARDOBJGRP *)NULL)
#endif

/*!
 * @brief   List of ovl.descriptors required to construct Client PERF_CF pwr model profiles.
 */
#define OSTASK_OVL_DESC_DEFINE_CLIENT_PERF_CF_PWR_MODEL_PROFILE_CONSTRUCT  \
        OVL_INDEX_ILWALID

/*!
 * @brief   List of ovl. descriptors required when running the
 *          CLIENT_PWR_MODEL_PROFILE scale RPC.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_CLIENT_PERF_CF_PWR_MODEL_PROFILE_SCALE_RPC)
#define OSTASK_OVL_DESC_DEFINE_CLIENT_PERF_CF_PWR_MODEL_PROFILE_SCALE_RPC      \
        OSTASK_OVL_DESC_DEFINE_PERF_CF_PWR_MODEL_SCALE_RPC_NON_ODP_BASE        \
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clientPerfCfPwrModelProfileScaleRpc)
#else
#define OSTASK_OVL_DESC_DEFINE_CLIENT_PERF_CF_PWR_MODEL_PROFILE_SCALE_RPC      \
        OVL_INDEX_ILWALID
#endif

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Virtual BOARDOBJ child providing attributes common to all Client PERF_CF Pwr Model Profiles.
 */
struct CLIENT_PERF_CF_PWR_MODEL_PROFILE
{
    /*!
     * BOARDOBJ super-class.
     * Must be first element of the structure!
     */
    BOARDOBJ super;

    /*!
     * Unique named identifier for the particular client profile. To be used as
     * input to client-wrapped scaling operations.
     */
    LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_ID profileId;

    /*!
     * Semantic index of internal PERF_CF_PWR_MODEL used for scaling.
     * This semantic index is derrived from the boardobj type of the profile.
     */
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SEMANTIC_INDEX pwrModelSemanticIndex;
};

/*!
 * Collection of all Client PERF_CF Pwr Model Profiles and related information.
 */
struct CLIENT_PERF_CF_PWR_MODEL_PROFILES
{
    /*!
     * BOARDOBJGRP_E32 super class. This should always be the first member!
     */
    BOARDOBJGRP_E32 super;
};

/*!
 * @copydoc LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_SCALE_INPUT
 */
typedef struct
{
    /*!
     * @brief   Mask of CLIENT_CLK_DOMAIN indices to be selected from ::freqkHz as
     *          overriden input to CLIENT_PERF_CF_PWR_MODEL_PROFILE_SCALE.
     */
    BOARDOBJGRPMASK_E32 clientDomainsMask;

    /*!
     * @brief   Frequencies (indexed by CLIENT_CLK_DOMAIN indices) to use as input to
     *          the estimation.
     */
    LwU32 freqkHz[LW2080_CTRL_CLK_CLK_DOMAIN_CLIENT_MAX_DOMAINS];
} CLIENT_PERF_CF_PWR_MODEL_PROFILE_SCALE_INPUT;

/*!
 * @interface   CLIENT_PERF_CF_PWR_MODEL_PROFILE
 *
 * @brief       Scale metrics against a CLIENT_PERF_CF_PWR_MODEL_PROFILE
 *
 * @param[in]   pClientPwrModelProfile  Pointer to the CLIENT_PERF_CF_PWR_MODEL_PROFILE object.
 * @param[in]   pInput                  Pointer to scaling inputs.
 * @param[in]   pOutput                 Pointer to outputs of scaling operation.
 *
 * @return  FLCN_OK     CLIENT_PERF_CF_PWR_MODEL_PROFILE metric scaled successfully
 * @return  other       Propagates return values from various calls
 */
#define ClientPerfCfPwrModelProfileScale(fname) FLCN_STATUS (fname)(                        \
    CLIENT_PERF_CF_PWR_MODEL_PROFILE                               *pClientPwrModelProfile, \
    CLIENT_PERF_CF_PWR_MODEL_PROFILE_SCALE_INPUT                   *pInput,                 \
    LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_SCALE_OUTPUT *pOutput                 \
)

/*!
 * @interface   CLIENT_PERF_CF_PWR_MODEL_PROFILE
 *
 * @brief       Setup metrics to be used in perfCfPwrModelScale (internal pwr model scaling routine)
 *
 * @param[in]       pClientPwrModelProfile  Pointer to the CLIENT_PERF_CF_PWR_MODEL_PROFILE object.
 * @param[inout]    pObservedMetrics        Pointer to the internal observed metrics inputs that need populating.
 * @param[inout]    pBounds                 Pointer to the internal bounding parameters that need populating.
 *
 * @return  FLCN_OK     Inputs were setup successfully
 * @return  other       Propagates return values from various calls
 */
#define ClientPerfCfPwrModelProfileScaleInternalMetricsInit(fname) FLCN_STATUS (fname)( \
    CLIENT_PERF_CF_PWR_MODEL_PROFILE           *pClientPwrModelProfile,                 \
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS *pObservedMetrics,                       \
    PERF_CF_PWR_MODEL_SCALE_TYPE_PARAMS        *pTypeParams                             \
)

/*!
 * @interface   CLIENT_PERF_CF_PWR_MODEL_PROFILE
 *
 * @brief       Extract outputs from perfCfPwrModelScale estimated metrics
 *
 * @param[in]   pClientPwrModelProfile  Pointer to the CLIENT_PERF_CF_PWR_MODEL_PROFILE object.
 * @param[in]   pEstimatedMetrics       Pointer to the estimated metrics that will be extracted.
 * @param[out]  pOutputs                Pointer to the client outputs that need populating.
 *
 * @return  FLCN_OK     Outputs were extracted successfully
 * @return  other       Propagates return values from various calls
 */
#define ClientPerfCfPwrModelProfileScaleInternalMetricsExtract(fname) FLCN_STATUS (fname)(  \
    CLIENT_PERF_CF_PWR_MODEL_PROFILE                               *pClientPwrModelProfile, \
    LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_METRICS                     *pEstimatedMetrics,      \
    LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_SCALE_OUTPUT *pOutputs                \
)

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// BOARDOBJGRP interfaces.
BoardObjGrpIfaceModel10CmdHandler                         (clientPerfCfPwrModelProfileBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "clientPerfCfPwrModelProfileBoardObjGrpIfaceModel10Set");

// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet                             (clientPerfCfPwrModelProfileGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "clientPerfCfPwrModelProfileGrpIfaceModel10ObjSetImpl_SUPER");

// Client PerfCf PwrModel Profile interfaces.
ClientPerfCfPwrModelProfileScale                        (clientPerfCfPwrModelProfileScale)
    GCC_ATTRIB_SECTION("imem_perfCfModel",              "clientPerfCfPwrModelProfileScale");
ClientPerfCfPwrModelProfileScaleInternalMetricsInit      (clientPerfCfPwrModelProfileScaleInternalMetricsInit)
    GCC_ATTRIB_SECTION("imem_perfCfModel",              "clientPerfCfPwrModelProfileScaleInternalMetricsInit");
ClientPerfCfPwrModelProfileScaleInternalMetricsInit      (clientPerfCfPwrModelProfileScaleInternalMetricsInit_SUPER)
    GCC_ATTRIB_SECTION("imem_perfCfModel",              "clientPerfCfPwrModelProfileScaleInternalMetricsInit_SUPER");
ClientPerfCfPwrModelProfileScaleInternalMetricsExtract  (clientPerfCfPwrModelProfileScaleInternalMetricsExtract)
    GCC_ATTRIB_SECTION("imem_perfCfModel",              "clientPerfCfPwrModelProfileScaleInternalMetricsExtract");
ClientPerfCfPwrModelProfileScaleInternalMetricsExtract  (clientPerfCfPwrModelProfileScaleInternalMetricsExtract_SUPER)
    GCC_ATTRIB_SECTION("imem_perfCfModel",              "clientPerfCfPwrModelProfileScaleInternalMetricsExtract_SUPER");

/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @brief      Initialize a CLIENT_PERF_CF_PWR_MODEL_PROFILE_SCALE_INPUT.
 *
 * @param      pInput  The scaling input
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
clientPerfCfPwrModelProfileScaleInputInit
(
    CLIENT_PERF_CF_PWR_MODEL_PROFILE_SCALE_INPUT *pInput
)
{
    boardObjGrpMaskInit_E32(&pInput->clientDomainsMask);
    memset(pInput->freqkHz, 0 , sizeof(pInput->freqkHz));
}

/*!
 * @brief      Import data from an SDK structure into native PMU
 *             CLIENT_PERF_CF_PWR_MODEL_PROFILE_SCALE_INPUT structure.
 *
 * @param      pCtrlInput  The control metrics input
 * @param      pInput      The metrics input
 *
 * @return     The flcn status.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
clientPerfCfPwrModelProfileScaleInputImport
(
    LW2080_CTRL_PERF_CLIENT_PERF_CF_PWR_MODEL_PROFILE_SCALE_INPUT  *pCtrlInput,
    CLIENT_PERF_CF_PWR_MODEL_PROFILE_SCALE_INPUT                   *pInput
)
{
    FLCN_STATUS status;
    LwBoardObjIdx i;

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskImport_E32(&pInput->clientDomainsMask,
                                  &pCtrlInput->clientDomainsMask),
        clientPerfCfPwrModelProfileScaleInputImport_exit);

    BOARDOBJGRPMASK_FOR_EACH_BEGIN(&pInput->clientDomainsMask, i)
    {
        pInput->freqkHz[i] = pCtrlInput->freqkHz[i];
    }
    BOARDOBJGRPMASK_FOR_EACH_END;

clientPerfCfPwrModelProfileScaleInputImport_exit:
    return status;
}

/*!
 * @brief      Colwert client scale inputs to internal scale metrics input
 *
 * @param      pClientInput    The client scale input
 * @param[out] pInternalInput  The internal scale metrics input
 *
 * @return     The flcn status.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
clientPerfCfPwrModelProfileScaleInputColwertToInternal
(
    CLIENT_PERF_CF_PWR_MODEL_PROFILE_SCALE_INPUT   *pClientInput,
    PERF_CF_PWR_MODEL_SCALE_METRICS_INPUT          *pInternalInput
)
{
    FLCN_STATUS status;
    LwBoardObjIdx i;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pClientInput   != NULL) &&
         (pInternalInput != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        clientPerfCfPwrModelProfileScaleInputColwertToInternal_exit);

    BOARDOBJGRPMASK_FOR_EACH_BEGIN(&pClientInput->clientDomainsMask, i)
    {
        LwU8 internalIndex;

        PMU_ASSERT_OK_OR_GOTO(status,
            clkDomainClientIndexToInternalIndex(
                CLK_CLK_DOMAINS_GET(), i, &internalIndex),
            clientPerfCfPwrModelProfileScaleInputColwertToInternal_exit);

        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pClientInput->freqkHz[i] != 0),
            FLCN_ERR_ILWALID_ARGUMENT,
            clientPerfCfPwrModelProfileScaleInputColwertToInternal_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
            perfCfPwrModelScaleMeticsInputSetFreqkHz(
                pInternalInput, internalIndex, pClientInput->freqkHz[i]),
            clientPerfCfPwrModelProfileScaleInputColwertToInternal_exit);
    }
    BOARDOBJGRPMASK_FOR_EACH_END;

    // Omit non-programmable domains
    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskAnd(
            &pInternalInput->domainsMask,
            &pInternalInput->domainsMask,
            &(CLK_CLK_DOMAINS_GET()->progDomainsMask)),
        clientPerfCfPwrModelProfileScaleInputColwertToInternal_exit);

    // Ensure the mask is not empty
    PMU_ASSERT_TRUE_OR_GOTO(status,
        boardObjGrpMaskBitSetCount(&pInternalInput->domainsMask) != 0U,
        FLCN_ERR_NOT_SUPPORTED,
        clientPerfCfPwrModelProfileScaleInputColwertToInternal_exit);

clientPerfCfPwrModelProfileScaleInputColwertToInternal_exit:
    return status;
}

/* ------------------------ Include Derived Types --------------------------- */
#include "perf/cf/client_perf_cf_pwr_model_profile_tgp_1x.h"

#endif // CLIENT_PERF_CF_PWR_MODEL_PROFILE_H
