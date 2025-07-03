/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWOS_OVL_DMEM_H
#define LWOS_OVL_DMEM_H

/*!
 * @file    lwos_ovl_dmem.h
 * @brief   Macros handling DMEM overlays.
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------- Application Includes --------------------------- */

#ifdef UPROC_RISCV
#include "config/g_sections_riscv.h"
#endif // UPROC_RISCV

/* ------------------------ External Definitions ---------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Define an index for a default DMEM overlay. For more information check
 * the documentation of @ref OVL_INDEX_DMEM().
 */
#define OVL_INDEX_OS_HEAP               0x00U

/*!
 * @copydoc OVL_COUNT_IMEM
 */
#if defined(UPROC_RISCV)
#define OVL_COUNT_DMEM()                UPROC_DATA_SECTION_COUNT
#else
#define OVL_COUNT_DMEM()                OS_LABEL(_overlay_id_dmem__count)
#endif

/*!
 * Check validity of DMEM overlay index, it should be less than count of
 * DMEM overlays used (on current build)
 */
#define LWOS_DMEM_OVL_VALID(ovlIdx)     ((ovlIdx) < OVL_COUNT_DMEM())

/*!
 * @copydoc OVL_INDEX_IMEM
 */
#if defined(UPROC_FALCON) && defined(DMEM_VA_SUPPORTED)
#define OVL_INDEX_DMEM(overlayName)     ((LwU8)OS_LABEL(_overlay_id_dmem_##overlayName))
#elif defined(UPROC_RISCV)
#define OVL_INDEX_DMEM(overlayName)     ((LwU8)OS_LABEL(PTR16(_section_dmem_##overlayName##_index)))
#else
#define OVL_INDEX_DMEM(overlayName)     ((LwU8)OVL_INDEX_OS_HEAP)
#endif

/*!
 * BASE PINNED DMEM overlay handling macros.
 */

#if defined(DMEM_VA_SUPPORTED) && !defined(UPROC_RISCV)

#define OSTASK_ATTACH_PINNED_DMEM_OVERLAY_TO_TASK(pTask, ovlIdxDmem)           \
    lwosTaskOverlayAttachAndFail(pTask, ovlIdxDmem, LW_TRUE, LW_FALSE)
#define OSTASK_DETACH_PINNED_DMEM_OVERLAY_FROM_TASK(pTask, ovlIdxDmem)         \
    lwosTaskOverlayDetachAndFail(pTask, ovlIdxDmem, LW_TRUE, LW_FALSE)

#define OSTASK_ATTACH_AND_LOAD_PINNED_DMEM_OVERLAY(ovlIdxDmem)                 \
    do {                                                                       \
        OSTASK_ATTACH_PINNED_DMEM_OVERLAY(ovlIdxDmem);                         \
        lwrtosRELOAD();                                                        \
    } while (LW_FALSE)

#else

#define OSTASK_ATTACH_PINNED_DMEM_OVERLAY_TO_TASK(pTask, ovlIdxDmem)           \
    lwosVarNOP(pTask, ovlIdxDmem)
#define OSTASK_DETACH_PINNED_DMEM_OVERLAY_FROM_TASK(pTask, ovlIdxDmem)         \
    lwosVarNOP(pTask, ovlIdxDmem)

#define OSTASK_ATTACH_AND_LOAD_PINNED_DMEM_OVERLAY(ovlIdxDmem)                 \
    lwosVarNOP(ovlIdxDmem)

#endif

/*!
 * DERIVED PINNED DMEM overlay handling macros.
 */

#define OSTASK_ATTACH_PINNED_DMEM_OVERLAY(ovlIdxDmem)                          \
    OSTASK_ATTACH_PINNED_DMEM_OVERLAY_TO_TASK(NULL, ovlIdxDmem)
#define OSTASK_DETACH_PINNED_DMEM_OVERLAY(ovlIdxDmem)                          \
    OSTASK_DETACH_PINNED_DMEM_OVERLAY_FROM_TASK(NULL, ovlIdxDmem)

/*!
 * BASE DMEM overlay handling macros.
 */

#if defined(ON_DEMAND_PAGING_BLK) || defined(UPROC_RISCV)

#define OSTASK_ATTACH_DMEM_OVERLAY_TO_TASK(pTask, ovlIdxImem)                  \
    lwosVarNOP(pTask, ovlIdxImem)
#define OSTASK_DETACH_DMEM_OVERLAY_FROM_TASK(pTask, ovlIdxImem)                \
    lwosVarNOP(pTask, ovlIdxImem)

#define OSTASK_ATTACH_AND_LOAD_DMEM_OVERLAY(ovlIdxDmem)                        \
    lwosVarNOP(ovlIdxDmem)

#else // ON_DEMAND_PAGING_BLK

#define OSTASK_ATTACH_DMEM_OVERLAY_TO_TASK(pTask, ovlIdxDmem)                  \
    OSTASK_ATTACH_PINNED_DMEM_OVERLAY_TO_TASK(pTask, ovlIdxDmem)
#define OSTASK_DETACH_DMEM_OVERLAY_FROM_TASK(pTask, ovlIdxDmem)                \
    OSTASK_DETACH_PINNED_DMEM_OVERLAY_FROM_TASK(pTask, ovlIdxDmem)

#define OSTASK_ATTACH_AND_LOAD_DMEM_OVERLAY(ovlIdxDmem)                        \
    OSTASK_ATTACH_AND_LOAD_PINNED_DMEM_OVERLAY(ovlIdxDmem)

#endif // ON_DEMAND_PAGING_BLK

/*!
 * DERIVED DMEM overlay handling macros.
 */

#define OSTASK_ATTACH_DMEM_OVERLAY(ovlIdxDmem)                                 \
    OSTASK_ATTACH_DMEM_OVERLAY_TO_TASK(NULL, ovlIdxDmem)
#define OSTASK_DETACH_DMEM_OVERLAY(ovlIdxDmem)                                 \
    OSTASK_DETACH_DMEM_OVERLAY_FROM_TASK(NULL, ovlIdxDmem)


/*!
 * Falcon code/data memory space is not flat. They are different address spaces,
 * both starting with address 0. Hence, we use the upper nibble to satisfy the
 * GNU linker script requirements of a flat address space. Data symbols defined
 * in the linker script have a location in the image that has the upper nibble
 * set to 1. This macro colwerts from that location to the DMEM location by
 * setting the upper nibble to 0.
 */
#define LWOS_DATA_SYM_IMAGE_LOC_TO_DMEM_LOC(x)  ((x) & ~0x10000000U)

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
#ifdef ON_DEMAND_PAGING_BLK
LwS32   lwosOvlDmemLoadPinnedOverlayList(LwU8 *pOvlList, LwU8 ovlCnt);
#endif // ON_DEMAND_PAGING_BLK

#if defined(DMEM_VA_SUPPORTED) && !defined(UPROC_RISCV)
LwS32   lwosOvlDmemLoadList(LwU8 *pOvlList, LwU8 ovlCnt);
#else
#define lwosOvlDmemLoadList(pOvlList, ovlCnt)   (0)
#endif // DMEM_VA_SUPPORTED

#endif // LWOS_OVL_DMEM_H
