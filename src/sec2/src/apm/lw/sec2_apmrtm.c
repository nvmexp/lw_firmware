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
 * @file   sec2_apmrtm.c
 * @brief  The functions needed to access Root of Storage SubWPr from SEC2 task.
 *         RTS still needs to be initialized properly by ACR, SPDM task initialized,
 *         and this code needs to share synchronization with SPDM RTS accesses.
 */

 /* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"
#include "mmu/mmucmn.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_objapm.h"
#include "lib_intfcshahw.h"
#include "apm_rts.h"
#include "lw_mutex.h"
#include "lib_mutex.h"

/* --------------------------- Macro Definitions ---------------------------- */
#define APM_DMA_BUFFER_ALIGNMENT 0x10

/* --------------------------- Global Variables ----------------------------- */
// The buffer used to DMA the data from RTS.
static APM_MSR_GROUP         g_dmaGroupBuffer
    GCC_ATTRIB_SECTION("dmem_apm", "g_dmaGroupBuff")
    GCC_ATTRIB_ALIGN(APM_DMA_BUFFER_ALIGNMENT);

static APM_EXTENSION_BUFFER  g_msrExtBuffer
    GCC_ATTRIB_SECTION("dmem_apm", "g_msrExtBuffer")
    GCC_ATTRIB_ALIGN(APM_DMA_BUFFER_ALIGNMENT);

static APM_MSR               g_hashDstBuffer
    GCC_ATTRIB_SECTION("dmem_apm", "g_hashDstBuffer")
    GCC_ATTRIB_ALIGN(APM_DMA_BUFFER_ALIGNMENT);


/* -------------------------- Function Prototypes --------------------------- */
static FLCN_STATUS
_apmExtendMeasurement(PAPM_EXTENSION_BUFFER, PAPM_MSR)
    GCC_ATTRIB_SECTION("imem_apm", "_apmExtendMeasurement");

static void
_apmPrepareExtendMsr(PAPM_EXTENSION_BUFFER, LwU8 *, PAPM_MSR, LwBool, LwBool)
    GCC_ATTRIB_SECTION("imem_apm", "_apmPrepareExtendMsr");

static FLCN_STATUS
_apmMsrExtendHelper(PAPM_MSR_GROUP, LwU8 *, LwBool)
    GCC_ATTRIB_SECTION("imem_apm", "_apmMsrExtendHelper");

/* --------------------------- Static functions ----------------------------- */

/**
  @brief Reads the MSR group defined by msrIdx from the RTS into memory
         pointed to by dst.

         APM-TODO CONFCOMP-465: Implement synchronization for the HS milestone

  @note Must call apm_rts_offset_initialize before using this function.

  @param[in]  msrIdx  The index of the MSR group.
  @param[out] dst      The destination buffer in DMEM.

  @retval TRUE   MSR group read successfully.
  @retval FALSE  MSR group read failed.
 */
static FLCN_STATUS
_apmReadMsrGroup
(
    LwU64           apmRtsOffset,
    LwU32           msrIdx
)
{
    FLCN_STATUS      flcnStatus = FLCN_OK;
    LwU64            offset     = 0;
    RM_FLCN_MEM_DESC memDesc;

    offset = APM_MSR_GRP_OFFSET(apmRtsOffset, msrIdx);

    RM_FLCN_U64_PACK(&(memDesc.address), &offset);
    memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                                     RM_SEC2_DMAIDX_UCODE, 0);
    memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE,
                                     sizeof(APM_MSR_GROUP), memDesc.params);

    CHECK_FLCN_STATUS(dmaReadUnrestricted((LwU8 *) &g_dmaGroupBuffer, &memDesc, 0,
                                          sizeof(APM_MSR_GROUP)));


ErrorExit:
    return flcnStatus;
}

/**
  @brief Reads the MSR group defined by msrIdx from the RTS into memory
         pointed to by dst.

         APM-TODO CONFCOMP-465: Implement synchronization for the HS milestone

  @note Must call apm_rts_offset_initialize before using this function.

  @param[in]  msrIdx  The index of the MSR group.
  @param[out] dst      The destination buffer in DMEM.

  @retval TRUE   MSR group read successfully.
  @retval FALSE  MSR group read failed.
 */
static FLCN_STATUS
_apmWriteMsrGroup
(
    LwU64           apmRtsOffset,
    LwU32           msrIdx
)
{
    FLCN_STATUS      flcnStatus = FLCN_OK;
    LwU64            offset     = 0;
    RM_FLCN_MEM_DESC memDesc;

    offset = APM_MSR_GRP_OFFSET(apmRtsOffset, msrIdx);

    RM_FLCN_U64_PACK(&(memDesc.address), &offset);
    memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                                     RM_SEC2_DMAIDX_UCODE, 0);
    memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE,
                                     sizeof(APM_MSR_GROUP), memDesc.params);

    CHECK_FLCN_STATUS(dmaWriteUnrestricted(&g_dmaGroupBuffer, &memDesc, 0,
                                          sizeof(APM_MSR_GROUP)));


ErrorExit:
    return flcnStatus;
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
static void
_apmPrepareExtendMsr
(
    PAPM_EXTENSION_BUFFER pExtensionBuffer,
    LwU8                 *pMeasurement,
    PAPM_MSR              pMsr,
    LwBool                bIsShadow,
    LwBool                bFromZero
)
{
    LwU32 idx;
    pExtensionBuffer->reserved[0]   = 0x00;
    pExtensionBuffer->reserved[1]   = 0x00;

    //
    // This needs to be replaced with the ID of the current falcon
    //
    pExtensionBuffer->falconID      = LSF_FALCON_ID_SEC2; 
    pExtensionBuffer->prevStateSrc  = bFromZero ? APM_PREVIOUS_MSR_STATE_ENUM_ZERO_STRING :
                                                  APM_PREVIOUS_MSR_STATE_OLD_VALUE;

    for (idx = 0; idx < APM_HASH_SIZE_BYTE; idx++)
    {
        pExtensionBuffer->newMeasurement.bytes[idx] = pMeasurement[idx];
    }

    
    for (idx = 0; idx < APM_HASH_SIZE_BYTE; idx++)
    {
        if (bFromZero && bIsShadow)
        {
            pExtensionBuffer->prevState.bytes[idx] = 0x00;
        }
        else
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
static FLCN_STATUS
_apmExtendMeasurement
(
    PAPM_EXTENSION_BUFFER pExtensionBuffer,
    PAPM_MSR              pExtensionDst
)
{
    SHA_CONTEXT shaCtx;
    FLCN_STATUS flcnStatus, tmpStatus;
    LwU32       shaStatus;

    flcnStatus = shahwAcquireMutex(SHA_MUTEX_ID_APM);
    if (flcnStatus != FLCN_OK)
    {
        return flcnStatus;
    }

    shaCtx.algoId  = APM_HASH_ID;
    shaCtx.msgSize = APM_EXTENSION_BUFFER_SIZE;
    shaCtx.mutexId = SHA_MUTEX_ID_APM;
    CHECK_FLCN_STATUS(shahwOpInit(&shaCtx));
    
    CHECK_FLCN_STATUS(shahwOpUpdate(&shaCtx,
                                    APM_EXTENSION_BUFFER_SIZE,
                                    LW_TRUE, 
                                    (LwU64)((LwU32)pExtensionBuffer),
                                    SHA_SRC_TYPE_DMEM,
                                    0,
                                    &shaStatus,
                                    LW_TRUE));

    CHECK_FLCN_STATUS(shahwOpFinal(&shaCtx, (LwU32 *) pExtensionDst, LW_TRUE, &shaStatus));

ErrorExit:
    tmpStatus = shahwReleaseMutex(SHA_MUTEX_ID_APM);
    flcnStatus = (flcnStatus == FLCN_OK ? tmpStatus : flcnStatus);
    return flcnStatus;
}

/**
 * @brief Extends the MSR and MSRS in DMEM, and updates the counters
 * 
 * @param pMsrGroup[in/out]   The pointer to the MSR group in DMEM
 * @param pMeasurement[in]    The pointer to the new measurement
 * @param bFlushShadow[in]    Indicates if MSRS should be extended from a zero-string
 * @return FLCN_STATUS        ACR_OK if succesful, relevant error otherwise
 */
static FLCN_STATUS
_apmMsrExtendHelper
(
    PAPM_MSR_GROUP pMsrGroup,
    LwU8          *pMeasurement,
    LwBool         bFlushShadow
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    _apmPrepareExtendMsr(&g_msrExtBuffer, pMeasurement, &pMsrGroup->msr, LW_FALSE, bFlushShadow);
    CHECK_FLCN_STATUS(_apmExtendMeasurement(&g_msrExtBuffer, &g_hashDstBuffer));
    memcpy(&pMsrGroup->msr, &g_hashDstBuffer, sizeof(APM_MSR));
    pMsrGroup->msrcnt++;

    _apmPrepareExtendMsr(&g_msrExtBuffer, pMeasurement, &pMsrGroup->msrs, LW_TRUE, bFlushShadow);
    CHECK_FLCN_STATUS(_apmExtendMeasurement(&g_msrExtBuffer, &g_hashDstBuffer));
    memcpy(&pMsrGroup->msrs, &g_hashDstBuffer, sizeof(APM_MSR));
    pMsrGroup->msrscnt = bFlushShadow ? 1 : pMsrGroup->msrscnt+1;

ErrorExit:
    return flcnStatus;
}

/* ---------------------------- Public Functions ---------------------------- */

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
 *                            the measurement to extend (big-endian)
 * @return FLCN_STATUS        FLCN_OK if succesful, relevant error otherwise
 *
 * @note  APM-TODO CONFCOMP-669: Add synchronization
 */
FLCN_STATUS
apmMsrExtend
(
    LwU64  apmRtsOffset,
    LwU32  msrIdx,
    LwU8  *pMeasurement,
    LwBool bFlushShadow
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    if (pMeasurement == NULL || msrIdx >= APM_MSR_NUM)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    CHECK_FLCN_STATUS(_apmReadMsrGroup(apmRtsOffset, msrIdx));
    CHECK_FLCN_STATUS(_apmMsrExtendHelper(&g_dmaGroupBuffer, pMeasurement, bFlushShadow));
    CHECK_FLCN_STATUS(_apmWriteMsrGroup(apmRtsOffset, msrIdx));

ErrorExit:
    return flcnStatus;
}

/**
 * @brief Captures and measures the dynamic state and stores the measurement in RTS.
 * 
 * @param apmRtsOffset The offset of RTS SubWPr in FB
 *
 * @return LwBool LW_TRUE if successful, LW_FALSE otherwise.
 */
LwBool
apmCaptureDynamicState
(
    LwU64  apmRtsOffset
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    CHECK_FLCN_STATUS(apmCaptureVprMmuState_HAL(&ApmHal, apmRtsOffset));
    CHECK_FLCN_STATUS(apmCaptureDecodeTrapState_HAL(&ApmHal, apmRtsOffset));

ErrorExit:
    return (flcnStatus == FLCN_OK);
}
