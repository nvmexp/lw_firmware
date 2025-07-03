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
 * @brief   Interfaces for accessing structures from Firmware Runtime Security
 */

#ifndef VBIOS_FRTS_ACCESS_H
#define VBIOS_FRTS_ACCESS_H

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "ucode_interface.h"

/* ------------------------ Application Includes ---------------------------- */
#include "vbios/vbios_frts_access_dma.h"
#include "vbios/vbios_frts_config.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
typedef struct
{
#if PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_ACCESS_DMA)
    /*!
     * DMA-specific access data
     */
    VBIOS_FRTS_ACCESS_DMA dma;
#else
    /*!
     * Only necessary for cases where this file is included from builds that
     * don't enable FRTS
     */
    LwU8 dummy;
#endif
} VBIOS_FRTS_ACCESS;

/* ------------------------ Compile-Time Asserts ---------------------------- */
//
// If FRTS is enabled, we need at least one way of accessing it, so assert that
// here.
// 
// Lwrrently, there is only access via DMA, but additional variants should be
// added here in the future.
//
ct_assert(
    !PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS) ||
    PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_ACCESS_DMA));

/* ------------------------ Public Functions -------------------------------- */
/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @brief   Retrieves @ref VBIOS_FRTS_ACCESS::dma, if enabled
 *
 * @param[in]   pAccess Pointer to structure from which to retrieve
 * @param[out]  pDma    Pointer to dma field, if enabled; NULL otherwise
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  Any pointer argument was NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
vbiosFrtsAccessDmaGet
(
    VBIOS_FRTS_ACCESS *pAccess,
    VBIOS_FRTS_ACCESS_DMA **ppDma
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pAccess != NULL) &&
        (ppDma != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosFrtsAccessDmaGet_exit);

#if PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_ACCESS_DMA)
    *ppDma = &pAccess->dma;
#else // PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_ACCESS_DMA)
    *ppDma = NULL;
#endif // PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_ACCESS_DMA)

vbiosFrtsAccessDmaGet_exit:
    return status;
}

/*!
 * @copydoc vbiosFrtsAccessDmaGet
 *
 * @note    Works with a pointer to const and returns a pointer to const
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
vbiosFrtsAccessDmaGetConst
(
    const VBIOS_FRTS_ACCESS *pAccess,
    const VBIOS_FRTS_ACCESS_DMA **ppDma
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pAccess != NULL) &&
        (ppDma != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosFrtsAccessDmaGetConst_exit);

#if PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_ACCESS_DMA)
    *ppDma = &pAccess->dma;
#else // PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_ACCESS_DMA)
    *ppDma = NULL;
#endif // PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_ACCESS_DMA)

vbiosFrtsAccessDmaGetConst_exit:
    return status;
}

/*!
 * @brief   Initializes structures needed to access the FRTS data.
 *
 * @param[out]  pAccess Pointer to @ref VBIOS_FRTS_ACCESS structure to construct
 * @param[in]   pConfig Configuration parameters for FRTS
 * @param[in]   dmaIdx  DMA index expected to be used to access FRTS
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  pAccess pointer is NULL
 * @return  @ref FLCN_ERR_ILWALID_STATE     Internal state is misconfigured.
 * @return  Others                          Errors propagated from callees.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
vbiosFrtsAccessConstruct
(
    VBIOS_FRTS_ACCESS *pAccess,
    const VBIOS_FRTS_CONFIG *pConfig,
    LwU32 dmaIdx
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pAccess != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosFrtsAccessConstruct_exit);

    if (PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_ACCESS_DMA))
    {
        VBIOS_FRTS_ACCESS_DMA *pDma;
        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosFrtsAccessDmaGet(pAccess, &pDma),
            vbiosFrtsAccessConstruct_exit);
        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosFrtsAccessDmaConstruct(pDma, pConfig, dmaIdx),
            vbiosFrtsAccessConstruct_exit);
    }
    else
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            FLCN_ERR_ILWALID_STATE,
            vbiosFrtsAccessConstruct_exit);
    }

vbiosFrtsAccessConstruct_exit:
    return status;
}

/*!
 * @brief   Prepares the metadata about the @ref FW_IMAGE_DESCRIPTOR itself for
 *          being accessed.
 *
 * @note    Must be called before @ref vbiosFrtsAccessImageDescriptorGet is ever
 *          called.
 *
 * @param[in,out]   pAccess Pointer to @ref VBIOS_FRTS_ACCESS structure
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  pAccess pointer is NULL
 * @return  @ref FLCN_ERR_ILWALID_STATE     Internal state is misconfigured.
 * @return  Others                          Errors propagated from callees.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
vbiosFrtsAccessImageDescriptorMetaPrepare
(
    VBIOS_FRTS_ACCESS *pAccess
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pAccess != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosFrtsAccessDmaImageDescriptorMetaPrepare_exit);

    if (PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_ACCESS_DMA))
    {
        VBIOS_FRTS_ACCESS_DMA *pDma;
        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosFrtsAccessDmaGet(pAccess, &pDma),
            vbiosFrtsAccessDmaImageDescriptorMetaPrepare_exit);
        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosFrtsAccessDmaImageDescriptorMetaPrepare(pDma),
            vbiosFrtsAccessDmaImageDescriptorMetaPrepare_exit);
    }
    else
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            FLCN_ERR_ILWALID_STATE,
            vbiosFrtsAccessDmaImageDescriptorMetaPrepare_exit);
    }

vbiosFrtsAccessDmaImageDescriptorMetaPrepare_exit:
    return status;
}

/*!
 * @brief   Retrieves the metadata about the @ref FW_IMAGE_DESCRIPTOR itself
 *
 * @param[in]   pAccess Pointer to @ref VBIOS_FRTS_ACCESS structure
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  pAccess pointer is NULL
 * @return  @ref FLCN_ERR_ILWALID_STATE     Internal state is misconfigured.
 * @return  Others                          Errors propagated from callees.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
vbiosFrtsAccessImageDescriptorMetaGet
(
    const VBIOS_FRTS_ACCESS *pAccess,
    LwU32 *pIdentifier,
    LwU32 *pVersionField,
    LwU32 *pSize
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pAccess != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosFrtsAccessImageDescriptorMetaGet_exit);

    if (PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_ACCESS_DMA))
    {
        const VBIOS_FRTS_ACCESS_DMA *pDma;
        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosFrtsAccessDmaGetConst(pAccess, &pDma),
            vbiosFrtsAccessImageDescriptorMetaGet_exit);
        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosFrtsAccessDmaImageDescriptorMetaGet(
                pDma, pIdentifier, pVersionField, pSize),
            vbiosFrtsAccessImageDescriptorMetaGet_exit);
    }
    else
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            FLCN_ERR_ILWALID_STATE,
            vbiosFrtsAccessImageDescriptorMetaGet_exit);
    }

vbiosFrtsAccessImageDescriptorMetaGet_exit:
    return status;
}

/*!
 * @brief   Prepares the the @ref FW_IMAGE_DESCRIPTOR for being accessed
 *
 * @note    Must be called before @ref vbiosFrtsAccessImageDescriptorGet is ever
 *          called.
 *
 * @param[in,out]   pAccess Pointer to @ref VBIOS_FRTS_ACCESS structure
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  pAccess pointer is NULL
 * @return  @ref FLCN_ERR_ILWALID_STATE     Internal state is misconfigured.
 * @return  Others                          Errors propagated from callees.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
vbiosFrtsAccessImageDescriptorV1Prepare
(
    VBIOS_FRTS_ACCESS *pAccess
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pAccess != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosFrtsAccessImageDescriptorV1Prepare_exit);

    if PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_ACCESS_DMA)
    {
        VBIOS_FRTS_ACCESS_DMA *pDma;
        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosFrtsAccessDmaGet(pAccess, &pDma),
            vbiosFrtsAccessImageDescriptorV1Prepare_exit);
        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosFrtsAccessDmaImageDescriptorV1Prepare(pDma),
            vbiosFrtsAccessImageDescriptorV1Prepare_exit);
    }
    else
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            FLCN_ERR_ILWALID_STATE,
            vbiosFrtsAccessImageDescriptorV1Prepare_exit);
    }

vbiosFrtsAccessImageDescriptorV1Prepare_exit:
    return status;
}

/*!
 * @brief   Retrieves the @ref FW_IMAGE_DESCRIPTOR containing overall FRTS
 *          metadata
 *
 * @param[in]   pAccess         Pointer to @ref VBIOS_FRTS_ACCESS structure
 * @param[out]  ppDescriptor    Pointer into which to store pointer to
 *                              descriptor
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  pAccess pointer is NULL
 * @return  @ref FLCN_ERR_ILWALID_STATE     Internal state is misconfigured.
 * @return  Others                          Errors propagated from callees.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
vbiosFrtsAccessImageDescriptorV1Get
(
    const VBIOS_FRTS_ACCESS *pAccess,
    const FW_IMAGE_DESCRIPTOR **ppDescriptor
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pAccess != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosFrtsAccessImageDescriptorV1Get_exit);

    if (PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_ACCESS_DMA))
    {
        const VBIOS_FRTS_ACCESS_DMA *pDma;
        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosFrtsAccessDmaGetConst(pAccess, &pDma),
            vbiosFrtsAccessImageDescriptorV1Get_exit);
        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosFrtsAccessDmaImageDescriptorV1Get(pDma, ppDescriptor),
            vbiosFrtsAccessImageDescriptorV1Get_exit);
    }
    else
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            FLCN_ERR_ILWALID_STATE,
            vbiosFrtsAccessImageDescriptorV1Get_exit);
    }

vbiosFrtsAccessImageDescriptorV1Get_exit:
    return status;
}

/*!
 * @brief   Prepares the the VDPA entries for being accessed
 *
 * @note    Must be called before @ref vbiosFrtsAccessVdpaEntriesGet is ever
 *          called.
 *
 * @param[in,out]   pAccess     Pointer to @ref VBIOS_FRTS_ACCESS structure
 * @param[in]       pMetadata   Pointer to structure containing FRTS metadata
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  pAccess pointer is NULL
 * @return  @ref FLCN_ERR_ILWALID_STATE     Internal state is misconfigured.
 * @return  Others                          Errors propagated from callees.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
vbiosFrtsAccessVdpaEntriesPrepare
(
    VBIOS_FRTS_ACCESS *pAccess,
    const VBIOS_FRTS_METADATA *pMetadata
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pAccess != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosFrtsAccessVdpaEntriesPrepare_exit);

    if (PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_ACCESS_DMA))
    {
        VBIOS_FRTS_ACCESS_DMA *pDma;
        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosFrtsAccessDmaGet(pAccess, &pDma),
            vbiosFrtsAccessVdpaEntriesPrepare_exit);
        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosFrtsAccessDmaVdpaEntriesPrepare(pDma, pMetadata),
            vbiosFrtsAccessVdpaEntriesPrepare_exit);
    }
    else
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            FLCN_ERR_ILWALID_STATE,
            vbiosFrtsAccessVdpaEntriesPrepare_exit);
    }

vbiosFrtsAccessVdpaEntriesPrepare_exit:
    return status;
}

/*!
 * @brief   Retrieves the array of VDPA entries from within FRTS
 *
 * @param[in]   pAccess         Pointer to @ref VBIOS_FRTS_ACCESS structure
 * @param[out]  ppVdpaEntries   Pointer into which to store pointer to entries
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  pAccess pointer is NULL
 * @return  @ref FLCN_ERR_ILWALID_STATE     Internal state is misconfigured.
 * @return  Others                          Errors propagated from callees.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
vbiosFrtsAccessVdpaEntriesGet
(
    const VBIOS_FRTS_ACCESS *pAccess,
    const void **ppVdpaEntries
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pAccess != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosFrtsAccessVdpaEntriesGet_exit);

    if (PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_ACCESS_DMA))
    {
        const VBIOS_FRTS_ACCESS_DMA *pDma;
        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosFrtsAccessDmaGetConst(pAccess, &pDma),
            vbiosFrtsAccessVdpaEntriesGet_exit);
        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosFrtsAccessDmaVdpaEntriesGet(pDma, ppVdpaEntries),
            vbiosFrtsAccessVdpaEntriesGet_exit);
    }
    else
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            FLCN_ERR_ILWALID_STATE,
            vbiosFrtsAccessVdpaEntriesGet_exit);
    }

vbiosFrtsAccessVdpaEntriesGet_exit:
    return status;
}

/*!
 * @brief   Retrieves a pointer to a table from the FRTS region
 *
 * @note    May have side-effects. Generally expected to be called only one per
 *          table.
 *
 * @param[in]   pAccess Pointer to @ref VBIOS_FRTS_ACCESS structure
 * @param[in]   dirtId  DIRT for the table
 * @param[in]   offset  Offset of the table within FRTS
 * @param[in]   size    Size of the table
 * @param[out]  ppTable Pointer into which to store pointer to table
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  pAccess pointer is NULL
 * @return  @ref FLCN_ERR_ILWALID_STATE     Internal state is misconfigured.
 * @return  Others                          Errors propagated from callees.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
vbiosFrtsAccessTablePrepareAndGet
(
    VBIOS_FRTS_ACCESS *pAccess,
    const VBIOS_FRTS_METADATA *pMetadata,
    LW_GFW_DIRT dirtId,
    LwLength offset,
    LwLength size,
    const void **ppTable
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pAccess != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosFrtsAccessTablePrepareAndGet_exit);

    if (PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_ACCESS_DMA))
    {
        VBIOS_FRTS_ACCESS_DMA *pDma;
        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosFrtsAccessDmaGet(pAccess, &pDma),
            vbiosFrtsAccessTablePrepareAndGet_exit);
        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosFrtsAccessDmaTablePrepareAndGet(
                pDma, pMetadata, dirtId, offset, size, ppTable),
            vbiosFrtsAccessTablePrepareAndGet_exit);
    }
    else
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            FLCN_ERR_ILWALID_STATE,
            vbiosFrtsAccessTablePrepareAndGet_exit);
    }

vbiosFrtsAccessTablePrepareAndGet_exit:
    return status;
}

#endif // VBIOS_FRTS_ACCESS_H
