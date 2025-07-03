/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_DMA_H
#define PMU_DMA_H

/*!
 * @file    pmu_dma.h
 * @brief   Interfaces for handling content of the PMU's super-surface.
 */

/* ------------------------ System includes --------------------------------- */
#include "pmusw.h"
#include "main.h"
#include "lwostask.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Application includes ---------------------------- */
/* ------------------------ Defines ----------------------------------------- */
/*!
 * @brief   Helper for reading of an entire element of the super-surface
 * @copydoc ssurfaceRd
 *
 * @param[in]   _pBuf       Pointer to the destination DMEM buffer
 * @param[in]   _member     Super-surface element name of the source buffer
 */
#define SSURFACE_RD(_pBuf, _member)                         \
    ssurfaceRd((_pBuf),                                     \
               RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(_member), \
               RM_PMU_SUPER_SURFACE_MEMBER_SIZE(_member))

/*!
 * @brief   Helper for writing of an entire element of the super-surface
 * @copydoc ssurfaceWr
 *
 * @param[in]   _pBuf       Pointer to the source DMEM buffer
 * @param[in]   _element    Super-surface element name of the destination buffer
 */
#define SSURFACE_WR(_pBuf, _member)                         \
    ssurfaceWr((_pBuf),                                     \
               RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(_member), \
               RM_PMU_SUPER_SURFACE_MEMBER_SIZE(_member))

/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
/*!
 * @brief   Read a content from the PMU's super-surface
 *
 * Performs the DMA transfer from the super-surface (FB) into the PMU's DMEM
 *
 * @param[in]   pBuf        Pointer to the destination DMEM buffer
 * @param[in]   offset      Super-surface offset of the source buffer
 * @param[in]   numBytes    The number of bytes to transfer
 *
 * @return  FLCN_OK     Transfer was sucessfully completed
 * @return  <error>     Propagates error code from @ref dmaRead()
 */
FLCN_STATUS ssurfaceRd(void *pBuf, LwU32 offset, LwU32 numBytes)
    GCC_ATTRIB_SECTION("imem_resident", "ssurfaceRd");

/*!
 * @brief   Write a content into the PMU's super-surface
 *
 * Performs the DMA transfer from the PMU's DMEM into the super-surface (FB)
 *
 * @param[in]   pBuf        Pointer to the source DMEM buffer
 * @param[in]   offset      Super-surface offset of the destination buffer
 * @param[in]   numBytes    The number of bytes to transfer
 *
 * @return  FLCN_OK     Transfer was sucessfully completed
 * @return  <error>     Propagates error code from @ref dmaWrite()
 */
FLCN_STATUS ssurfaceWr(void *pBuf, LwU32 offset, LwU32 numBytes)
    GCC_ATTRIB_SECTION("imem_resident", "ssurfaceWr");

/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @brief   Returns the size of the super-surface
 *
 * @return  Size of the super-surface
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwU32
ssurfaceSize(void)
{
    return REF_VAL(
        LW_RM_FLCN_MEM_DESC_PARAMS_SIZE,
        PmuInitArgs.superSurface.params);
}

#endif // PMU_DMA_H
