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
 * @copydoc vbios_frts_access_dma.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "data_id_reference_table_addendum.h"
#include "ucode_interface.h"

/* ------------------------ Application Includes ---------------------------- */
#include "vbios/vbios_frts_access_dma.h"
#include "vbios/vbios_bit.h"
#include "vbios/vbios_frts.h"
#include "vbios/vbios_frts_cert.h"
#include "perf/vbios/pstates_vbios_6x.h"

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   Given the needed size for a buffer, returns the actual size needed
 *          for the buffer to be able to DMA safely.
 *
 * @note    This macro actually bumps the size up to not only be safe but to be
 *          most efficient for DMA, i.e., possible to be used with start and end
 *          alignments of @ref DMA_MAX_READ_ALIGNMENT
 *
 * @param[in]   _bufferSize The nominal buffer size
 *
 * @return  The _bufferSize aligned to be safe for DMA
 */
#define VBIOS_TABLE_BUFFER_SIZE_ALIGNED(_bufferSize) \
    (DMA_MAX_READ_ALIGNMENT + LWUPROC_ALIGN_UP_CT((_bufferSize), DMA_MAX_READ_ALIGNMENT))

/*!
 * @brief   Defines a buffer (and a wrapper structure) to be used for holding
 *          table data.
 *
 * @param[in]   _dirtId     @ref LW_GFW_DIRT for the table
 * @param[in]   _maxSize    Maximum expected size of the table
 */
#define VBIOS_FRTS_ACCESS_DMA_TABLE_BUFFER_DEFINE(_dirtId, _maxSize) \
    static LwU8 _dirtId##InternalBuffer[VBIOS_TABLE_BUFFER_SIZE_ALIGNED(_maxSize)] \
        GCC_ATTRIB_ALIGN(DMA_MAX_READ_ALIGNMENT) \
        GCC_ATTRIB_SECTION("dmem_vbiosFrtsAccessDmaBuffers", #_dirtId"InternalBuffer"); \
    static VBIOS_FRTS_ACCESS_DMA_TABLE_BUFFER _dirtId##Buffer \
        GCC_ATTRIB_SECTION("dmem_initData", #_dirtId"Buffer") = \
    { \
        .dirtId = (_dirtId), \
        .pBuffer = &_dirtId##InternalBuffer[0], \
        .bufferSize = (_maxSize), \
    }

/*!
 * @brief   Generates an entry in an array of
 *          @ref VBIOS_FRTS_ACCESS_DMA_TABLE_BUFFER objects
 *
 * @param[in]   dirtId  @ref LW_GFW_DIRT for the table
 *
 * @return  Pointer to the @ref VBIOS_FRTS_ACCESS_DMA_TABLE_BUFFER for the table
 */
#define VBIOS_FRTS_ACCESS_DMA_TABLE_BUFFER_ENTRY(_dirtId) \
    (&_dirtId##Buffer)

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
/*!
 * @brief   Finds the @ref VBIOS_FRTS_ACCESS_DMA_TABLE_BUFFER for a given table
 *
 * @param[in]   dirtId      @ref LW_GFW_DIRT of the table
 * @param[out]  ppBuffer    Pointer to pointer into which to store buffer
 *                          pointer
 *
 * @return  @ref FLCN_OK    Success
 */
static FLCN_STATUS s_vbiosFrtsAccessDmaTableBufferFind(
    const VBIOS_FRTS_ACCESS_DMA *pAccess, LW_GFW_DIRT dirtId, const VBIOS_FRTS_ACCESS_DMA_TABLE_BUFFER **ppBuffer)
    GCC_ATTRIB_SECTION("imem_init", "s_vbiosFrtsAccessDmaTableBufferFind");

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/*!
 * Buffer for reading the array of VDPA entries
 *
 * @note    Placed here rather than in @ref VBIOS_FRTS_ACCESS_DMA to try to
 *          reduce waste inside structure due to alignment.
 */
static LwU8 VbiosFrtsAccessDmaVdpaEntries[
    LW_ALIGN_UP(
        VBIOS_FRTS_VDPA_ENTRIES_MAX * VBIOS_FRTS_VDPA_ENTRIES_SIZE,
        DMA_MAX_READ_ALIGNMENT)]
    GCC_ATTRIB_ALIGN(DMA_MAX_READ_ALIGNMENT)
    GCC_ATTRIB_SECTION("dmem_vbiosFrtsAccessDmaBuffers", "VbiosFrtsAccessDmaVdpaEntries");

#if PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_ACCESS_DMA_BIT_INTERNAL_USE_ONLY)
/*!
 * If enabled, buffer for access to the BIT_INTERNAL_USE_ONLY table
 */
VBIOS_FRTS_ACCESS_DMA_TABLE_BUFFER_DEFINE(
    LW_GFW_DIRT_BIT_INTERNAL_USE_ONLY,
    sizeof(VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2));
#endif // PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_ACCESS_DMA_BIT_INTERNAL_USE_ONLY)

#if PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_ACCESS_DMA_PERFORMANCE_TABLE_6X)
/*!
 * If enabled, buffer for access to the PERFORMANCE_TABLE_6X table
 */
VBIOS_FRTS_ACCESS_DMA_TABLE_BUFFER_DEFINE(
    LW_GFW_DIRT_PERFORMANCE_TABLE, PSTATES_VBIOS_6X_MAX_SIZE);
#endif // PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_ACCESS_DMA_PERFORMANCE_TABLE_6X)

/*!
 * Array of buffers for all supported tables.
 */
static const VBIOS_FRTS_ACCESS_DMA_TABLE_BUFFER *VbiosFrtsAccessDmaTableBuffers[]
    GCC_ATTRIB_SECTION("dmem_initData", "VbiosFrtsAccessDmaTableBuffers") =
{
#if PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_ACCESS_DMA_BIT_INTERNAL_USE_ONLY)
    VBIOS_FRTS_ACCESS_DMA_TABLE_BUFFER_ENTRY(LW_GFW_DIRT_BIT_INTERNAL_USE_ONLY),
#endif // PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_ACCESS_DMA_BIT_INTERNAL_USE_ONLY)
#if PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_ACCESS_DMA_PERFORMANCE_TABLE_6X)
    VBIOS_FRTS_ACCESS_DMA_TABLE_BUFFER_ENTRY(LW_GFW_DIRT_PERFORMANCE_TABLE),
#endif // PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_ACCESS_DMA_PERFORMANCE_TABLE_6X)
};

/* ------------------------ Compile-Time Asserts ---------------------------- */
//
// If we want to try to DMA a section most efficiently, we need to use max
// alignment, so make sure that it's safe to do so by asserting that it's the
// same as the FRTS section alignment.
//
ct_assert(VBIOS_FRTS_SUB_REGION_ALIGNMENT == DMA_MAX_READ_ALIGNMENT);

//
// Ensure that it's safe to read the metadata about the FW_IMAGE_DESCRIPTOR and
// the FW_IMAGE_DESCRIPTOR by themselves.
//
ct_assert(
    LW_IS_ALIGNED(
        LW_OFFSETOF(FW_IMAGE_DESCRIPTOR, size) +
            sizeof(((FW_IMAGE_DESCRIPTOR *)NULL)->size),
        DMA_MIN_READ_ALIGNMENT) &&
    LW_IS_ALIGNED(
        sizeof(FW_IMAGE_DESCRIPTOR),
        DMA_MIN_READ_ALIGNMENT));

/* ------------------------ Public Functions -------------------------------- */
FLCN_STATUS
vbiosFrtsAccessDmaConstruct
(
    VBIOS_FRTS_ACCESS_DMA *pAccess,
    const VBIOS_FRTS_CONFIG *pConfig,
    LwU32 dmaIdx
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pAccess != NULL) &&
        (pConfig != NULL) &&
        (dmaIdx <= RM_PMU_DMAIDX_END),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosFrtsAccessDmaConstruct_exit);

    pAccess->memDesc = (RM_FLCN_MEM_DESC)
    {
        .address =
        {
            .lo = (LwU32)pConfig->offset,
            .hi = (LwU32)(pConfig->offset >> 32U),
        },
        .params =
            FLD_SET_REF_NUM(
                LW_RM_FLCN_MEM_DESC_PARAMS_DMA_IDX,
                dmaIdx,
            REF_NUM(
                LW_RM_FLCN_MEM_DESC_PARAMS_SIZE,
                pConfig->size)),
    };

    pAccess->pVdpaEntries = VbiosFrtsAccessDmaVdpaEntries;
    pAccess->ppTableBuffers = VbiosFrtsAccessDmaTableBuffers;
    pAccess->numTableBuffers = LW_ARRAY_ELEMENTS(VbiosFrtsAccessDmaTableBuffers);

vbiosFrtsAccessDmaConstruct_exit:
    return status;
}

FLCN_STATUS
vbiosFrtsAccessDmaImageDescriptorMetaPrepare
(
    VBIOS_FRTS_ACCESS_DMA *pAccess
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pAccess != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosFrtsAccessDmaImageDescriptorMetaPrepare_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        dmaReadUnrestricted(
            &pAccess->imageDescriptor,
            &pAccess->memDesc,
            0U,
            LW_OFFSETOF(FW_IMAGE_DESCRIPTOR, size) +
                sizeof(pAccess->imageDescriptor.size)),
        vbiosFrtsAccessDmaImageDescriptorMetaPrepare_exit);

vbiosFrtsAccessDmaImageDescriptorMetaPrepare_exit:
    return status;
}

FLCN_STATUS
vbiosFrtsAccessDmaImageDescriptorMetaGet
(
    const VBIOS_FRTS_ACCESS_DMA *pAccess,
    LwU32 *pIdentifier,
    LwU32 *pVersionField,
    LwU32 *pSize
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pAccess != NULL) &&
        (pIdentifier != NULL) &&
        (pVersionField != NULL) &&
        (pSize != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosFrtsAccessDmaImageDescriptorMetaGet_exit);

    *pIdentifier = pAccess->imageDescriptor.identifier;
    *pVersionField = pAccess->imageDescriptor.version;
    *pSize = pAccess->imageDescriptor.size;

vbiosFrtsAccessDmaImageDescriptorMetaGet_exit:
    return status;
}

FLCN_STATUS
vbiosFrtsAccessDmaImageDescriptorV1Prepare
(
    VBIOS_FRTS_ACCESS_DMA *pAccess
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pAccess != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosFrtsAccessDmaImageDescriptorV1Prepare_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        dmaReadUnrestricted(
            &pAccess->imageDescriptor,
            &pAccess->memDesc,
            0U,
            sizeof(pAccess->imageDescriptor)),
        vbiosFrtsAccessDmaImageDescriptorV1Prepare_exit);

vbiosFrtsAccessDmaImageDescriptorV1Prepare_exit:
    return status;
}

FLCN_STATUS
vbiosFrtsAccessDmaImageDescriptorV1Get
(
    const VBIOS_FRTS_ACCESS_DMA *pAccess,
    const FW_IMAGE_DESCRIPTOR **ppDescriptor
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pAccess != NULL) &&
        (ppDescriptor != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosFrtsAccessDmaImageDescriptorV1Get_exit);

    *ppDescriptor = &pAccess->imageDescriptor;

vbiosFrtsAccessDmaImageDescriptorV1Get_exit:
    return status;
}

FLCN_STATUS
vbiosFrtsAccessDmaVdpaEntriesPrepare
(
    VBIOS_FRTS_ACCESS_DMA *pAccess,
    const VBIOS_FRTS_METADATA *pMetadata
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pAccess != NULL) &&
        (pMetadata != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosFrtsAccessDmaVdpaEntriesGet_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        dmaReadUnrestricted(
            pAccess->pVdpaEntries,
            &pAccess->memDesc,
            pMetadata->vdpaEntryOffset,
            LW_ALIGN_UP(
                pMetadata->vdpaEntryCount * pMetadata->vdpaEntrySize,
                DMA_MAX_READ_ALIGNMENT)),
        vbiosFrtsAccessDmaVdpaEntriesGet_exit);

vbiosFrtsAccessDmaVdpaEntriesGet_exit:
    return status;
}

FLCN_STATUS
vbiosFrtsAccessDmaVdpaEntriesGet
(
    const VBIOS_FRTS_ACCESS_DMA *pAccess,
    const void **ppVdpaEntries
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pAccess != NULL) &&
        (ppVdpaEntries != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosFrtsAccessDmaVdpaEntriesGet_exit);

    *ppVdpaEntries = pAccess->pVdpaEntries;

vbiosFrtsAccessDmaVdpaEntriesGet_exit:
    return status;
}

FLCN_STATUS
vbiosFrtsAccessDmaTablePrepareAndGet
(
    VBIOS_FRTS_ACCESS_DMA *pAccess,
    const VBIOS_FRTS_METADATA *pMetadata,
    LW_GFW_DIRT dirtId,
    LwLength offset,
    LwLength size,
    const void **ppTable
)
{
    FLCN_STATUS status;
    const VBIOS_FRTS_ACCESS_DMA_TABLE_BUFFER *pBuffer;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pAccess != NULL) &&
        (dirtId <= LW_GFW_DIRT__COUNT) &&
        ((offset + size) >= offset) &&
        (ppTable != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosFrtsAccessDmaTablePrepareAndGet_exit);

    // Start off by setting the table pointer to NULL, in case we exit early.
    *ppTable = NULL;

    PMU_ASSERT_OK_OR_GOTO(status,
        s_vbiosFrtsAccessDmaTableBufferFind(pAccess, dirtId, &pBuffer),
        vbiosFrtsAccessDmaTablePrepareAndGet_exit);

    // If there is no buffer for this table, then exit early
    if (pBuffer == NULL)
    {
        goto vbiosFrtsAccessDmaTablePrepareAndGet_exit;
    }

    // Make sure that the table buffer is large enough to handle the table.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pBuffer->bufferSize >= size),
        FLCN_ERR_ILWALID_STATE,
        vbiosFrtsAccessDmaTablePrepareAndGet_exit);

    //
    // DMA the table in. We want to do this in an aligned way to be efficient,
    // so:
    //  1.) Get the actual offset of the table in FRTS by adding the offset to
    //      the base image offset
    //  2.) Align that down to the max read alignment.
    //  3.) Get the total amount of data to DMA. This is:
    //      3.1.) The number of bytes before the table begins
    //      3.2.) The number of bytes actually in the table
    //      3.3.) Any trailing bytes to get up to the alignment
    //
    {
        const LwLength tableOffsetInFrts =
            pMetadata->imageOffset + offset;
        const LwLength tableOffsetAligned =
            LWUPROC_ALIGN_DOWN(tableOffsetInFrts, DMA_MAX_READ_ALIGNMENT);
        const LwLength prePaddingBytes =
            tableOffsetInFrts - tableOffsetAligned;
        const LwLength dmaSizeAligned =
            LWUPROC_ALIGN_UP(
                prePaddingBytes + size,
                DMA_MAX_READ_ALIGNMENT);

        PMU_ASSERT_OK_OR_GOTO(status,
            dmaReadUnrestricted(
                pBuffer->pBuffer,
                &pAccess->memDesc,
                tableOffsetAligned,
                dmaSizeAligned),
            vbiosFrtsAccessDmaTablePrepareAndGet_exit);

        // Set the pointer to the table to the internal buffer.
        *ppTable = (void *)(((LwU8 *)pBuffer->pBuffer) + prePaddingBytes);
    }

vbiosFrtsAccessDmaTablePrepareAndGet_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
static FLCN_STATUS
s_vbiosFrtsAccessDmaTableBufferFind
(
    const VBIOS_FRTS_ACCESS_DMA *pAccess,
    LW_GFW_DIRT dirtId,
    const VBIOS_FRTS_ACCESS_DMA_TABLE_BUFFER **ppBuffer
)
{
    LwLength i;

    *ppBuffer = NULL;
    for (i = 0U; i < pAccess->numTableBuffers; i++)
    {
        if (pAccess->ppTableBuffers[i]->dirtId == dirtId)
        {
            *ppBuffer = pAccess->ppTableBuffers[i];
            break;
        }
    }

    return FLCN_OK;
}
