/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_apmmsr.c
 */

/* ------------------------ System Includes -------------------------------- */
#include "acr.h"
#include "acr_objsha.h"
#include "acrapm.h"

#ifdef ACR_BUILD_ONLY
#include "config/g_acrlib_private.h"
#include "config/g_acr_private.h"
#else
#include "g_acrlib_private.h"
#endif

#include "sec2mutexreservation.h"
#include "sec2shamutex.h"
#include "lwuproc.h"

/* ------------------------ External Definitions --------------------------- */
extern ACR_DMA_PROP  g_dmaProp;

/* ------------------------ Global Variables ------------------------------- */
APM_EXTENSION_BUFFER  g_msrExtBuffer              ATTR_ALIGNED(0x10)   ATTR_OVLY(".data");
APM_MSR               g_hashDstBuffer             ATTR_ALIGNED(0x10)   ATTR_OVLY(".data");
APM_MSR_GROUP         g_apmDmaGroup               ATTR_ALIGNED(0x10)   ATTR_OVLY(".data");
LwU32 g_offsetApmRts;


/* ------------------------ Static Functions ------------------------------- */

/**
 * @brief Reads from the root of trust for storage the MSR group containing MSR, MSRS,
 *        MSRCNT and MSRSCNT
 * 
 * @param msrIdx[in]    The index of the MSR group
 * @param pDest[out]    The pointer to the group structure in DMEM
 * @return ACR_STATUS   ACR_OK if succesful, relevant error otherwise
 */
static ACR_STATUS GCC_ATTRIB_ALWAYSINLINE()
_acrReadMsrGroup
(
    LwU32 msrIdx,
    PAPM_MSR_GROUP pDest
)
{
    // Read the MSR from FB to DMEM
    if (acrIssueDma_HAL(pAcr, pDest, LW_FALSE, APM_MSR_GRP_OFFSET(g_offsetApmRts, msrIdx), 
                        sizeof(APM_MSR_GROUP), ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END,
                        &g_dmaProp) != sizeof(APM_MSR_GROUP))
    {
        return ACR_ERROR_DMA_FAILURE;
    }

    return ACR_OK;
}

/**
 * @brief Writes the MSR group containing MSR, MSRS, MSRCNT and MSRSCNT to the
 *        root of trust
 * 
 * @param msrIdx[out]    The index of the MSR group
 * @param pSrc[in]       The pointer to the group structure in DMEM
 * @return ACR_STATUS    ACR_OK if succesful, relevant error otherwise
 */
static ACR_STATUS GCC_ATTRIB_ALWAYSINLINE()
_acrWriteMsrGroup
(
    LwU32 msrIdx,
    PAPM_MSR_GROUP pSrc
)
{
    // Read the MSR from FB from DMEM
    if (acrIssueDma_HAL(pAcr, pSrc, LW_FALSE, APM_MSR_GRP_OFFSET(g_offsetApmRts, msrIdx), 
                        sizeof(APM_MSR_GROUP), ACR_DMA_TO_FB, ACR_DMA_SYNC_AT_END,
                        &g_dmaProp) != sizeof(APM_MSR_GROUP))
    {
        return ACR_ERROR_DMA_FAILURE;
    }

    return ACR_OK;
}

/**
 * @brief Prepares the extension buffer using the new measurement and either
 *        the old MSR value, or zeros (depending on the indicator)
 *        For the values used read:
 *        https://confluence.lwpu.com/display/FalconSelwrityIPUserSpace/HW+RTS+IAS#HWRTSIAS-SHA2-384
 * 
 * @param pExtensionBuffer[out]  The buffer of size APM_EXTENSION_BUFFER_SIZE
 *                               used to store the data needed to form the
 *                               measurement
 * @param pMeasurement[in]       The measurement to add
 * @param pMsr[in]               The MSR being extended 
 * @param bFromZero[in]          The indicator whether to use MSR or zero-string
 *                               for extension
 */
static void GCC_ATTRIB_ALWAYSINLINE()
_acrPrepareExtendMsr
(
    PAPM_EXTENSION_BUFFER pExtensionBuffer,
    LwU8 *pMeasurement,
    PAPM_MSR pMsr,
    LwBool bIsShadow,
    LwBool bFromZero
)
{
    LwU32 idx;
    pExtensionBuffer->reserved[0]   = 0x00;
    pExtensionBuffer->reserved[1]   = 0x00;
    pExtensionBuffer->falconID      = LSF_FALCON_ID_SEC2; // This needs to be replaced with the ID of the current falcon
    pExtensionBuffer->prevStateSrc  = bFromZero ? APM_PREVIOUS_MSR_STATE_ENUM_ZERO_STRING : APM_PREVIOUS_MSR_STATE_OLD_VALUE;

    for (idx = 0; idx < APM_HASH_SIZE_BYTE; idx++)
    {
        pExtensionBuffer->newMeasurement.bytes[idx] = pMeasurement[idx];
    }

    if (bFromZero && bIsShadow)
    {
        for (idx = 0; idx < APM_HASH_SIZE_BYTE; idx++)
        {
            pExtensionBuffer->prevState.bytes[idx] = 0x00;
        }
    }
    else
    {
        for (idx = 0; idx < APM_HASH_SIZE_BYTE; idx++)
        {
            pExtensionBuffer->prevState.bytes[idx] = pMsr->bytes[idx];
        }
    }
}

/**
 * @brief Computes the hash of the buffer prepared by apmPrepareExtendMsr and
 *        stores the result at the beginning of the same buffer in the
 *        big-endian byte ordering
 * 
 * @param pExtensionBuffer[in/out]    The buffer with the prepared data
 * @return ACR_STATUS                 ACR_OK if succesful, relevant error otherwise
 */
static ACR_STATUS GCC_ATTRIB_ALWAYSINLINE()
_acrExtendMeasurement
(
    PAPM_EXTENSION_BUFFER pExtensionBuffer,
    PAPM_MSR pExtensionDst
)
{
    SHA_CONTEXT shaCtx;
    SHA_TASK_CONFIG taskCfg;
    ACR_STATUS status, tmpStatus;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaAcquireMutex_HAL(pSha, ACR_APM_MUTEX_ID));

    shaCtx.algoId  = APM_HASH_ID;
    shaCtx.msgSize = APM_EXTENSION_BUFFER_SIZE;
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(shaOperationInit_HAL(pSha, &shaCtx));

    taskCfg.srcType = SHA_SRC_CONFIG_SRC_DMEM;
    taskCfg.defaultHashIv = LW_TRUE;
    taskCfg.size = APM_EXTENSION_BUFFER_SIZE;
    taskCfg.addr = (LwU64)((LwU32)pExtensionBuffer);
 
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(shaInsertTask_HAL(pSha, &shaCtx, &taskCfg));

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(shaReadHashResult_HAL(pSha, &shaCtx, pExtensionDst, LW_TRUE));

Cleanup:
    tmpStatus = shaReleaseMutex_HAL(pSha, ACR_APM_MUTEX_ID);

    status = (status == ACR_OK ? tmpStatus : status);

    return status;
}

/**
 * @brief Move the measurement from the buffer to the MSR
 * 
 * @param pSrc[in]      The buffer with the new measurement
 * @param pMsr[out]     The pointer to the destination MSR
 */
static void GCC_ATTRIB_ALWAYSINLINE()
_acrMoveMeasurementToMsr
(
    PAPM_MSR pSrc,
    PAPM_MSR pMsr
)
{
    acrlibMemcpy_HAL(pAcrlib, pMsr, pSrc, sizeof(APM_MSR));
}

/**
 * @brief Extends the MSR and MSRS in DMEM, and updates the counters
 * 
 * @param pMsrGroup[in/out]   The pointer to the MSR group in DMEM
 * @param pMeasurement[in]    The pointer to the new measurement
 * @param bFlushShadow[in]    Indicates if MSRS should be extended from a zero-string
 * @return ACR_STATUS         ACR_OK if succesful, relevant error otherwise
 */
static ACR_STATUS GCC_ATTRIB_ALWAYSINLINE()
_acrMsrExtendHelper
(
    PAPM_MSR_GROUP pMsrGroup,
    LwU8 *pMeasurement,
    LwBool bFlushShadow
)
{
    ACR_STATUS status = ACR_OK;

    _acrPrepareExtendMsr(&g_msrExtBuffer, pMeasurement, &pMsrGroup->msr, LW_FALSE, bFlushShadow);
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrExtendMeasurement(&g_msrExtBuffer, &g_hashDstBuffer));
    _acrMoveMeasurementToMsr(&g_hashDstBuffer, &pMsrGroup->msr);
    pMsrGroup->msrcnt++;

    _acrPrepareExtendMsr(&g_msrExtBuffer, pMeasurement, &pMsrGroup->msrs, LW_TRUE, bFlushShadow);
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrExtendMeasurement(&g_msrExtBuffer, &g_hashDstBuffer));
    _acrMoveMeasurementToMsr(&g_hashDstBuffer, &pMsrGroup->msrs);
    pMsrGroup->msrscnt = bFlushShadow ? 1 : pMsrGroup->msrscnt+1;

    return status;
}

/* ------------------------ Public Functions ------------------------------- */

/**
 * @brief Reads the MSR group defined by msrIdx from the RTS into memory pointed
 *        to by pDst. The accesses are serialized using SEC2_MUTEX_APM_RTS.
 * 
 * @param msrIdx[in]        The index of the MSR group
 * @param pDst[out]         The destination buffer in DMEM
 * @return ACR_STATUS       ACR_OK if succesful, relevant error otherwise
 */
ACR_STATUS
acrReadMsrGroup
(
    LwU32 msrIdx,
    PAPM_MSR_GROUP pDst
)
{
    ACR_STATUS status = ACR_OK;
    ACR_STATUS tmpStatus;
    LwU8 apmMsrMutexToken = 0;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrAcquireSelwreMutex_HAL(pAcr, SEC2_MUTEX_APM_RTS, &apmMsrMutexToken));

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(_acrReadMsrGroup(msrIdx, &g_apmDmaGroup));

    acrlibMemcpy_HAL(pAcrlib, pDst, &g_apmDmaGroup, sizeof(APM_MSR_GROUP));
Cleanup:
    tmpStatus = acrReleaseSelwreMutex_HAL(pAcr, SEC2_MUTEX_APM_RTS, apmMsrMutexToken);

    status = (status == ACR_OK ? tmpStatus : status);

    return status;
}

/**
 * @brief This function extends the measurement in the targeted MSR with the
 *        hash pointed to by the pNextMeasurement. The result is stored
 *        in MSR.
 *
 *        pNextMeasurement needs to point to memory of size
 *        APM_HASH_SIZE_BYTE.
 *
 *        The accesses are serialized using SEC2_MUTEX_APM_RTS.
 *
 * @param pMeasurement[in]    Pointer to the measurement in DMEM with
                                the measurement to extend (big-endian)
 * @return ACR_STATUS         ACR_OK if succesful, relevant error otherwise
 */
ACR_STATUS 
acrMsrExtend
(
    LwU32 msrIdx,
    LwU8 *pMeasurement,
    LwBool bFlushShadow
)
{
    ACR_STATUS status = ACR_OK;
    ACR_STATUS tmpStatus;
    LwU8 apmMsrMutexToken = 0;

    if (pMeasurement == NULL || msrIdx >= APM_MSR_NUM)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrAcquireSelwreMutex_HAL(pAcr, SEC2_MUTEX_APM_RTS, &apmMsrMutexToken));
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(_acrReadMsrGroup(msrIdx, &g_apmDmaGroup));
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(_acrMsrExtendHelper(&g_apmDmaGroup, pMeasurement, bFlushShadow));
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(_acrWriteMsrGroup(msrIdx, &g_apmDmaGroup));

Cleanup:
    tmpStatus = acrReleaseSelwreMutex_HAL(pAcr, SEC2_MUTEX_APM_RTS, apmMsrMutexToken);

    status = (status == ACR_OK ? tmpStatus : status);

    return status;
}

/**
 * @brief Resets all bytes in RTS to 0.
 *        The accesses to RTS are serialized using SEC2_MUTEX_APM_RTS.
 * 
 * @return ACR_STATUS      ACR_OK if succesful, relevant error otherwise
 */
ACR_STATUS 
acrMsrResetAll()
{
    ACR_STATUS status, tmpStatus;
    LwU32 msrIdx;
    LwU8 apmMsrMutexToken = 0;

    acrMemsetByte(&g_apmDmaGroup, sizeof(APM_MSR_GROUP), 0);

    //
    // We do a soft reset to ensure that the SHA engine is in consistent state
    // before extending MSRs during the computation of the static GPU state
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(shaEngineSoftReset_HAL(pSha, SHA_ENGINE_SW_RESET_TIMEOUT));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrAcquireSelwreMutex_HAL(pAcr, SEC2_MUTEX_APM_RTS, &apmMsrMutexToken));

    //
    // Set MSRs to the starting state
    //
    for (msrIdx = 0; msrIdx < APM_MSR_NUM; msrIdx++)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(_acrWriteMsrGroup(msrIdx, &g_apmDmaGroup));
    }

Cleanup:
    tmpStatus = acrReleaseSelwreMutex_HAL(pAcr, SEC2_MUTEX_APM_RTS, apmMsrMutexToken);

    status = (status == ACR_OK ? tmpStatus : status);

    return status;
}