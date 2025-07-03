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
 * @brief   Interfaces for DMAing structures from Firmware Runtime Security into
 *          memory.
 */

#ifndef VBIOS_FRTS_ACCESS_DMA_H
#define VBIOS_FRTS_ACCESS_DMA_H

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "data_id_reference_table_addendum.h"
#include "ucode_interface.h"

/* ------------------------ Application Includes ---------------------------- */
#include "vbios/vbios_frts_config.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Structure for holding a table buffer and some metadata
 */
typedef struct
{
    /*!
     * The DIRT ID for the table.
     */
    LW_GFW_DIRT dirtId;

    /*!
     * The buffer to be used for the table
     */
    LwU8 *const pBuffer;

    /*!
     * The "nominal" size of @ref VBIOS_FRTS_ACCESS_DMA_TABLE_BUFFER::pBuffer
     *
     * @note    See @ref VBIOS_TABLE_BUFFER_SIZE_ALIGNED regarding "nominal"
     *          size
     */
    const LwLength bufferSize;
} VBIOS_FRTS_ACCESS_DMA_TABLE_BUFFER;

/*!
 * Structure containing data for reading from FRTS into DTCM at init time.
 */
typedef struct
{
    /*!
     * Memory descriptor for DMAing the FRTS data.
     */
    RM_FLCN_MEM_DESC memDesc;

    /*!
     * Buffer into which to DMA the global metadata for FRTS.
     */
    FW_IMAGE_DESCRIPTOR imageDescriptor
        GCC_ATTRIB_ALIGN(DMA_MIN_READ_ALIGNMENT);

    /*!
     * Pointer to buffer for reading the array of VDPA entries
     */
    LwU8 *pVdpaEntries;

    /*!
     * Array of pointers to buffer structures for individual tables
     */
    const VBIOS_FRTS_ACCESS_DMA_TABLE_BUFFER **ppTableBuffers;

    /*!
     * Number of @ref VBIOS_FRTS_ACCESS_DMA::ppTableBuffers
     */
    LwLength numTableBuffers;
} VBIOS_FRTS_ACCESS_DMA;

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc vbiosFrtsAccessConstruct
 *
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  Any pointer argument is NULL; dmaIdx
 *                                          is an invalid DMA index
 * @return  @ref FLCN_ERR_ILWALID_STATE     FRTS was not in the expected format.
 * @return  Others                          Errors propagated from callees.
 */
FLCN_STATUS vbiosFrtsAccessDmaConstruct(
    VBIOS_FRTS_ACCESS_DMA *pAccess, const VBIOS_FRTS_CONFIG *pConfig, LwU32 dmaIdx)
    GCC_ATTRIB_SECTION("imem_init", "vbiosFrtsAccessDmaConstruct");

/*!
 * @copydoc vbiosFrtsAccessImageDescriptorMetaPrepare
 *
 * @note    DMAs the descriptor data to a DTCM buffer.
 */
FLCN_STATUS vbiosFrtsAccessDmaImageDescriptorMetaPrepare(VBIOS_FRTS_ACCESS_DMA *pAccess)
    GCC_ATTRIB_SECTION("imem_init", "vbiosFrtsAccessDmaImageDescriptorMetaPrepare");

/*!
 * @copydoc vbiosFrtsAccessImageDescriptorMetaGet
 *
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  Any pointer argument is NULL
 */
FLCN_STATUS vbiosFrtsAccessDmaImageDescriptorMetaGet(
    const VBIOS_FRTS_ACCESS_DMA *pAccess, LwU32 *pIdentifier, LwU32 *pVersionField, LwU32 *pSize)
    GCC_ATTRIB_SECTION("imem_init", "vbiosFrtsAccessDmaImageDescriptorMetaGet");

/*!
 * @copydoc vbiosFrtsAccessImageDescriptorV1Prepare
 *
 * @note    DMAs the descriptor data to a DTCM buffer.
 */
FLCN_STATUS vbiosFrtsAccessDmaImageDescriptorV1Prepare(VBIOS_FRTS_ACCESS_DMA *pAccess)
    GCC_ATTRIB_SECTION("imem_init", "vbiosFrtsAccessDmaImageDescriptorV1Prepare");

/*!
 * @copydoc vbiosFrtsAccessImageDescriptorV1Get
 *
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  Any pointer argument is NULL
 */
FLCN_STATUS vbiosFrtsAccessDmaImageDescriptorV1Get(
    const VBIOS_FRTS_ACCESS_DMA *pAccess, const FW_IMAGE_DESCRIPTOR **ppDescriptor)
    GCC_ATTRIB_SECTION("imem_init", "vbiosFrtsAccessDmaImageDescriptorV1Get");

/*!
 * @copydoc vbiosFrtsAccessVdpaEntriesPrepare
 *
 * @note    DMAs the VDPA entries to a DTCM buffer.
 */
FLCN_STATUS vbiosFrtsAccessDmaVdpaEntriesPrepare(
    VBIOS_FRTS_ACCESS_DMA *pAccess, const VBIOS_FRTS_METADATA *pMetadata)
    GCC_ATTRIB_SECTION("imem_init", "vbiosFrtsAccessDmaVdpaEntriesPrepare");

/*!
 * @copydoc vbiosFrtsAccessVdpaEntriesGet
 *
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  Any pointer argument is NULL
 */
FLCN_STATUS vbiosFrtsAccessDmaVdpaEntriesGet(
    const VBIOS_FRTS_ACCESS_DMA *pAccess, const void **ppVdpaEntries)
    GCC_ATTRIB_SECTION("imem_init", "vbiosFrtsAccessDmaVdpaEntriesGet");

/*!
 * @copydoc vbiosFrtsAccessTablePrepareAndGet
 *
 * @note    Has the side-effect of DMAing the table to a DTCM buffer.
 *
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  Any pointer argument is NULL; dirtId
 *                                          is invalid; offset and size specify
 *                                          an invalid region for the table
 */
FLCN_STATUS vbiosFrtsAccessDmaTablePrepareAndGet(
    VBIOS_FRTS_ACCESS_DMA *pAccess, const VBIOS_FRTS_METADATA *pMetadata, LW_GFW_DIRT dirtId, LwLength offset, LwLength size, const void **ppTable)
    GCC_ATTRIB_SECTION("imem_init", "vbiosFrtsAccessDmaTablePrepareAndGet");

#endif // VBIOS_FRTS_ACCESS_DMA_H
