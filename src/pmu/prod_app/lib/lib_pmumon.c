/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lib_pmumon.c
 * @brief  Code for the PMUMON library.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "main.h"
#include "rmpmusupersurfif.h"
#include "dbgprintf.h"

#include "pmu/pmuif_pmumon.h"
#include "pmu/ssurface.h"
#include "lwos_ustreamer.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib/lib_pmumon.h"

/* ------------------------- Private Defines -------------------------------- */
#define PMUMON_USTREAMER_LOCAL_BUFFER_SIZE                                4096U

/* ------------------------- Static Function Prototypes --------------------- */
static FLCN_STATUS s_pmumonQueuePublishEntry(PMUMON_QUEUE_DESCRIPTOR *pQueueDesc, void *pData)
    GCC_ATTRIB_SECTION("imem_libPmumon", "s_pmumonQueuePublishEntry");

static FLCN_STATUS s_pmumonQueuePublishHeader(PMUMON_QUEUE_DESCRIPTOR *pQueueDesc)
    GCC_ATTRIB_SECTION("imem_libPmumon", "s_pmumonQueuePublishHeader");

static void s_pmumonQueueUpdateDescriptorHeader(PMUMON_QUEUE_DESCRIPTOR *pQueueDesc)
    GCC_ATTRIB_SECTION("imem_libPmumon", "s_pmumonQueueUpdateDescriptorHeader");

/* ------------------------- Public Variables ------------------------------- */
LwU8 pmumonUstreamerQueueId = 0;

/* ------------------------- Private Variables ------------------------------ */
LwBool pmumonUstreamerReady = LW_FALSE;

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc pmumonQueueConstruct
 */
FLCN_STATUS
pmumonQueueConstruct
(
    PMUMON_QUEUE_DESCRIPTOR    *pQueueDesc,
    LwU32                       headerOffset,
    LwU32                       bufferOffset,
    LwU32                       entrySize,
    LwU32                       queueSize
)
{
    FLCN_STATUS status = FLCN_OK;

    if ((pQueueDesc == NULL) ||
        (entrySize == 0)     ||
        (queueSize == 0)     ||
        (queueSize == RM_PMU_PMUMON_ILWALID_HEAD_INDEX))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto pmumonQueueConstruct_exit;
    }

    pQueueDesc->bConstructed      = LW_TRUE;
    pQueueDesc->headerOffset      = headerOffset;
    pQueueDesc->bufferOffset      = bufferOffset;
    pQueueDesc->entrySize         = entrySize;
    pQueueDesc->queueSize         = queueSize;
    pQueueDesc->header.headIndex  = RM_PMU_PMUMON_ILWALID_HEAD_INDEX;
    pQueueDesc->header.sequenceId = 0;

    // Put queue into invalid state
    status = s_pmumonQueuePublishHeader(pQueueDesc);
    if (status != FLCN_OK)
    {
        goto pmumonQueueConstruct_exit;
    }

    // Restore local queue state in prep for first write.
    pQueueDesc->header.headIndex = 0;

pmumonQueueConstruct_exit:
    return status;
}

/*!
 * @copydoc pmumonQueueWrite
 */
FLCN_STATUS
pmumonQueueWrite
(
    PMUMON_QUEUE_DESCRIPTOR    *pQueueDesc,
    void                       *pData
)
{
    FLCN_STATUS status = FLCN_OK;

    if ((pQueueDesc == NULL) ||
        (pData == NULL))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto pmumonQueueWrite_exit;
    }

    if (!pQueueDesc->bConstructed)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto pmumonQueueWrite_exit;
    }

    status = s_pmumonQueuePublishEntry(pQueueDesc, pData);
    if (status != FLCN_OK)
    {
        goto pmumonQueueWrite_exit;
    }

    status = s_pmumonQueuePublishHeader(pQueueDesc);
    if (status != FLCN_OK)
    {
        goto pmumonQueueWrite_exit;
    }

    s_pmumonQueueUpdateDescriptorHeader(pQueueDesc);

pmumonQueueWrite_exit:
    return FLCN_OK;
}

/*!
 * @copydoc pmumonUstreamerInitHook
 */
FLCN_STATUS pmumonUstreamerInitHook(void)
{
    FLCN_STATUS     status = FLCN_OK;
    void           *pmumonUstreamerLocalBuffer = NULL;
    LwU32           superSurfaceBufferOffset;
    LwU32           superSurfaceBufferSize;

    // Allocate Buffer
    pmumonUstreamerLocalBuffer = lwosMallocAligned
    (
        UPROC_SECTION_REF(dmem_ustreamerData),
        PMUMON_USTREAMER_LOCAL_BUFFER_SIZE,
        sizeof(LwU32)
    );
    if (pmumonUstreamerLocalBuffer == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        dbgPrintf(LEVEL_CRIT, "pmumonUstreamerLocalBuffer failed to allocate.\n");
        goto pmumonUstreamerInitHook_exit;
    }

    superSurfaceBufferOffset =
            RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga10xAndLater.ustreamer.pmumonUstreamerBuffer.data);
    superSurfaceBufferSize =
            RM_PMU_SUPER_SURFACE_MEMBER_SIZE(ga10xAndLater.ustreamer.pmumonUstreamerBuffer.data);

    status = lwosUstreamerQueueConstruct
    (
        pmumonUstreamerLocalBuffer,
        PMUMON_USTREAMER_LOCAL_BUFFER_SIZE,
        &PmuInitArgs.superSurface,
        superSurfaceBufferOffset,
        superSurfaceBufferSize,
        &pmumonUstreamerQueueId,
        LW2080_CTRL_FLCN_USTREAMER_FEATURE_PMUMON,
        LW_FALSE
    );

pmumonUstreamerInitHook_exit:
    return status;
}

/* ------------------------- Static Functions ------------------------------- */
/*!
 * @brief      DMA out a new entry to the fb cirlwlar queue at index
 *             pQueueDesc->header.headIndex.
 *
 * @param[in]  pQueueDesc   Descriptor for queue being written to
 * @param[in]  pData        The data to be written out
 *
 * @return     FLCN_OK if successful, otherwise whatever status is bubbled up
 *             from dmaWrite()
 */
static FLCN_STATUS
s_pmumonQueuePublishEntry
(
    PMUMON_QUEUE_DESCRIPTOR    *pQueueDesc,
    void                       *pData
)
{
    FLCN_STATUS status;

    // Write out the entry to the head of the queue
    status = ssurfaceWr(pData,
                        pQueueDesc->bufferOffset +
                        (pQueueDesc->entrySize * pQueueDesc->header.headIndex),
                        pQueueDesc->entrySize);
    if (status != FLCN_OK)
    {
        goto s_pmumonQueuePublishEntry_exit;
    }

s_pmumonQueuePublishEntry_exit:
    return status;
}

/*!
 * @brief      DMA out updated queue header to the fb cirlwlar queue.
 *
 * @param[in]  pQueueDesc   Descriptor for queue being written to
 *
 * @return     FLCN_OK if successful, otherwise whatever status is bubbled up
 *             from dmaWrite()
 */
static FLCN_STATUS
s_pmumonQueuePublishHeader
(
    PMUMON_QUEUE_DESCRIPTOR *pQueueDesc
)
{
    FLCN_STATUS status;

    // Write out the head index to the queue's header
    status = ssurfaceWr(&pQueueDesc->header,
                        pQueueDesc->headerOffset,
                        sizeof(pQueueDesc->header));
    if (status != FLCN_OK)
    {
        goto s_pmumonQueuePublishHeader_exit;
    }

s_pmumonQueuePublishHeader_exit:
    return status;
}

/*!
 * @brief      Modify the head index within the queue descriptor.
 *
 * @param[in]  pQueueDesc  Descriptor for queue being modified
 */
static void
s_pmumonQueueUpdateDescriptorHeader
(
    PMUMON_QUEUE_DESCRIPTOR *pQueueDesc
)
{
    // Wrap-around the head index based on size of queue
    pQueueDesc->header.headIndex =
        (pQueueDesc->header.headIndex + 1) % pQueueDesc->queueSize;

    // Change the sequence ID
    pQueueDesc->header.sequenceId++;
}
