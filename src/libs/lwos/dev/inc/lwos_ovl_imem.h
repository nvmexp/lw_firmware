/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWOS_OVL_IMEM_H
#define LWOS_OVL_IMEM_H

/*!
 * @file    lwos_ovl_imem.h
 * @brief   Macros handling IMEM overlays.
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#ifdef UPROC_RISCV
#include "config/g_sections_riscv.h"
#endif // UPROC_RISCV

/* ------------------------ External Definitions ---------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Define an index for an invalid IMEM overlay. For more information check
 * the documentation of @ref OVL_INDEX_IMEM().
 */
#define OVL_INDEX_ILWALID               0x00U

/*!
 * Retrieve a number of used overlays (on current build). Do not default to
 * OVL_INDEX_***() since DMEM one is adjusted if DMEM_VA is not supported.
 */
#if defined(UPROC_RISCV)
#define OVL_COUNT_IMEM()                UPROC_CODE_SECTION_COUNT
#else
#define OVL_COUNT_IMEM()                OS_LABEL(_overlay_id_imem__count)
#endif

/*!
 * Maps the overlay name into linker-generated overlay index. Overlay indexing
 * starts at 1 and last valid overlay's index have value of OVL_COUNT_****().
 * Invalid overlays (zero sized ones) are set to OVL_INDEX_ILWALID. Any changes
 * to this indexing has to be made in sync between the RTOS code and the perl
 * script generating linker's *.ld files.
 */
#if defined(UPROC_FALCON)
#define OVL_INDEX_IMEM(overlayName)     ((LwU8)OS_LABEL(_overlay_id_imem_##overlayName))
#elif defined(UPROC_RISCV)
#define OVL_INDEX_IMEM(overlayName)     ((LwU8)OS_LABEL(PTR16(_section_imem_##overlayName##_index)))
#else
#define OVL_INDEX_IMEM(overlayName)     ((LwU8)OVL_INDEX_ILWALID)
#endif


/*!
 * Checks if the IMEM overlay specified with overlay index i is resident
 */
#define OVL_IMEM_IS_RESIDENT(ovlIdx)                                           \
    LWOS_BM_GET(_imem_ovl_attr_resident, ovlIdx, 8U)

/*!
 * Attaches an IMEM/DMEM overlay (described by a linker-generated overlay index)
 * to a task (identifed by task handle).
 *
 * @param[in]   pTask       Pointer to task's TCB (aka Task Handle)
 * @param[in]   ovlIdxImem  Index of the IMEM overlay to attach
 *
 * @note !IMPORTANT! Be sure to read the full function documentation for @ref
 *       lwosTaskOverlayAttach before using this macro. Failure to abide by the
 *       rules set forth in the dolwmenation may result in unexpected behavior.
 *
 * @note After LS/HS code sharing is fully supported, use _HS version macro
 *       if the overlay is to be attached as HS, use the version without _HS
 *       otherwise.
 */
#if defined(UPROC_RISCV)
#define OSTASK_ATTACH_PINNED_IMEM_OVERLAY_TO_TASK(pTask, ovlIdxImem)           \
    lwosVarNOP(pTask, ovlIdxImem)
#else
#define OSTASK_ATTACH_PINNED_IMEM_OVERLAY_TO_TASK(pTask, ovlIdxImem)           \
    lwosTaskOverlayAttachAndFail(pTask, ovlIdxImem, LW_FALSE, LW_FALSE)
#endif

/*!
 * Allows a task to attach an IMEM/DMEM overlay (described by a linker-generated
 * overlay index) to itself (does not require a task handle).
 *
 * @param[in]   ovlIdxImem  Index of the overlay to attach
 *
 * @note !IMPORTANT! Be sure to read the full function documentation for @ref
 *       lwosTaskOverlayAttach before using this macro. Failure to abide by the
 *       rules set forth in the dolwmenation may result in unexpected behavior.
 *
 * @note After LS/HS code sharing is fully supported, use _HS version macro
 *       if the overlay is to be attached as HS, use the version without _HS
 *       otherwise.
 */
#define OSTASK_ATTACH_PINNED_IMEM_OVERLAY(ovlIdxImem)                          \
    OSTASK_ATTACH_PINNED_IMEM_OVERLAY_TO_TASK(NULL, ovlIdxImem)

/*!
 * Allows a task to conditionally attach an IMEM/DMEM overlay (described by a
 * linker-generated overlay index) to itself (does not require a task handle).
 * In case that the requested overlay is already attached this macro does not
 * fail and returns LW_FALSE instead.
 *
 * @param[in]   ovlIdxImem  Index of the overlay to attach
 *
 * @return  LW_TRUE     On successful attachment of requested overlay.
 * @return  LW_FALSE    When requested overlay was already attached.
 *
 * @note Intended use of this macro is in library code that should conditionally
 *       attach/detach required overlay (if not already attached).
 *
 * @note !IMPORTANT! Be sure to read the full function documentation for @ref
 *       lwosTaskOverlayAttach before using this macro. Failure to abide by the
 *       rules set forth in the dolwmenation may result in unexpected behavior.
 *
 * @note After LS/HS code sharing is fully supported, use _HS version macro
 *       if the overlay is to be attached as HS, use the version without _HS
 *       otherwise.
 */
#if defined(UPROC_RISCV)
#define OSTASK_ATTACH_PINNED_IMEM_OVERLAY_COND(ovlIdxImem)           (LW_FALSE)
#else
#define OSTASK_ATTACH_PINNED_IMEM_OVERLAY_COND(ovlIdxImem)                     \
    lwosTaskOverlayAttach(NULL, ovlIdxImem, LW_FALSE, LW_FALSE)
#endif

/*!
 * A wrapper for @ref OSTASK_ATTACH_IMEM_OVERLAY that allows a task to attach an
 * IMEM/DMEM overlay to itself and then load it (the attach process by itself
 * does not deal with the loading process). The attach process implemented by
 * yielding control of the processor and forcing a context-switch. Do not
 * attempt this process when such a yield is not possible (or highly
 * undesireable). When attaching multiple overlays in sequence, it is more
 * optimal to defer the loading process until the end. In some cases it may also
 * be safe to avoid any forceful loading process and rely on a natural
 * context-switch to occur and bring in all attached overlays.
 *
 * @copydoc OSTASK_ATTACH_IMEM_OVERLAY
 *
 * @note After LS/HS code sharing is fully supported, use _HS version macro
 *       if the overlay is to be attached as HS, use the version without _HS
 *       otherwise.
 */
#if defined(UPROC_RISCV)
#define OSTASK_ATTACH_AND_LOAD_PINNED_IMEM_OVERLAY(ovlIdxImem)                 \
    lwosVarNOP(ovlIdxImem)
#else
#define OSTASK_ATTACH_AND_LOAD_PINNED_IMEM_OVERLAY(ovlIdxImem)                 \
    do {                                                                       \
        OSTASK_ATTACH_PINNED_IMEM_OVERLAY(ovlIdxImem);                         \
        lwrtosRELOAD();                                                        \
    } while (LW_FALSE)
#endif

/*!
 * Detaches an IMEM/DMEM overlay (described by a linker-generated overlay index)
 * from a task (identified by task handle).
 *
 * @param[in]   pTask       Pointer to task's TCB (aka Task Handle)
 * @param[in]   ovlIdxImem  Index of the IMEM overlay to attach
 *
 * @note !IMPORTANT! Be sure to read the full function documentation for @ref
 *       lwosTaskOverlayDetach before using this macro. Failure to abide by the
 *       rules set forth in the dolwmenation may result in unexpected behavior.
 *
 * @note After LS/HS code sharing is fully supported, use _HS version macro
 *       if the overlay is to be detached as HS, use the version without _HS
 *       otherwise.
 */
#if defined(UPROC_RISCV)
#define OSTASK_DETACH_PINNED_IMEM_OVERLAY_FROM_TASK(pTask, ovlIdxImem)         \
    lwosVarNOP(pTask, ovlIdxImem)
#else
#define OSTASK_DETACH_PINNED_IMEM_OVERLAY_FROM_TASK(pTask, ovlIdxImem)         \
    lwosTaskOverlayDetachAndFail(pTask, ovlIdxImem, LW_FALSE, LW_FALSE)
#endif

/*!
 * A wrapper for @ref OSTASK_DETACH_IMEM_OVERLAY_FROM_TASK that allows a task to
 * detach an IMEM/DMEM overlay from itself (does not require a task handle).
 *
 * @note After LS/HS code sharing is fully supported, use _HS version macro
 *       if the overlay is to be detached as HS, use the version without _HS
 *       otherwise.
 */
#define OSTASK_DETACH_PINNED_IMEM_OVERLAY(ovlIdxImem)                          \
    OSTASK_DETACH_PINNED_IMEM_OVERLAY_FROM_TASK(NULL, ovlIdxImem)

/*!
 * Allows a task to conditionally detach an IMEM/DMEM overlay (described by a
 * linker-generated overlay index) to itself (does not require a task handle).
 * In case that the requested overlay is not yet attached this macro does not
 * fail and returns LW_FALSE instead.
 *
 * @param[in]   ovlIdxImem  Index of the overlay to detach
 *
 * @return  LW_TRUE     On successful detachment of requested overlay.
 * @return  LW_FALSE    When requested overlay was not yet attached.
 *
 * @note !IMPORTANT! Be sure to read the full function documentation for @ref
 *       lwosTaskOverlayDetach before using this macro. Failure to abide by the
 *       rules set forth in the dolwmenation may result in unexpected behavior.
 *
 * @note After LS/HS code sharing is fully supported, use _HS version macro
 *       if the overlay is to be detached as HS, use the version without _HS
 *       otherwise.
 */
#if defined(UPROC_RISCV)
#define OSTASK_DETACH_PINNED_IMEM_OVERLAY_COND(ovlIdxImem)           (LW_FALSE)
#else
#define OSTASK_DETACH_PINNED_IMEM_OVERLAY_COND(ovlIdxImem)                     \
    lwosTaskOverlayDetach(NULL, ovlIdxImem, LW_FALSE, LW_FALSE)
#endif

/*!
 * @brief   Loads multiple IMEM/DMEM overlays into physical IMEM.
 *
 * @param[in]   pOvlListImem
 *       List of IMEM/DMEM overlays to be loaded as light secure into the physical IMEM/DMEM.
 * @param[in]   ovlCntImem
 *       Size of the IMEM/DMEM overlay list (including invalid entries).
 */

#if defined(UPROC_RISCV)
#define OSTASK_LOAD_PINNED_IMEM_OVERLAYS_LS(pOvlListImemLS, ovlCntImemLS)      \
    lwosVarNOP(pOvlListImemLS, ovlCntImemLS)

#else
#ifdef ON_DEMAND_PAGING_BLK
#define OSTASK_LOAD_PINNED_IMEM_OVERLAYS_LS(pOvlListImem, ovlCntImem)          \
    lwosOvlImemLoadPinnedOverlayList(pOvlListImem, ovlCntImem)
#else // ON_DEMAND_PAGING_BLK
#define OSTASK_LOAD_PINNED_IMEM_OVERLAYS_LS(pOvlListImem, ovlCntImem)          \
    do {                                                                       \
        if (lwosOvlImemLoadList(pOvlListImem, ovlCntImem, LW_FALSE) < 0)       \
        {                                                                      \
            lwuc_halt();                                                       \
        }                                                                      \
    } while (LW_FALSE)
#endif // ON_DEMAND_PAGING_BLK
#endif

/*!
 * @brief IMEM/DMEM Overlay Macros
 *
 * These function the same as their pinned counterparts when on-demand
 * paging is not enabled. When on-demand paging is enabled they become NOPs.
 *
 * Most code will use these macros for overlay management, unless there is a
 * specific reason they should be pinned for ODP.
 */
#if defined(ON_DEMAND_PAGING_BLK) || defined(UPROC_RISCV)

#define OSTASK_LOAD_IMEM_OVERLAYS_LS(pOvlListImemLS, ovlCntImemLS)             \
    lwosVarNOP(pOvlListImemLS, ovlCntImemLS)

#define OSTASK_ATTACH_IMEM_OVERLAY_TO_TASK(pTask, ovlIdxImem)                  \
    lwosVarNOP(pTask, ovlIdxImem)
#define OSTASK_DETACH_IMEM_OVERLAY_FROM_TASK(pTask, ovlIdxImem)                \
    lwosVarNOP(pTask, ovlIdxImem)

#define OSTASK_ATTACH_IMEM_OVERLAY_COND(ovlIdxImem)                   (LW_FALSE)
#define OSTASK_DETACH_IMEM_OVERLAY_COND(ovlIdxImem)                   (LW_FALSE)

#define OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(ovlIdxImem)   lwosVarNOP(ovlIdxImem)

#else // ON_DEMAND_PAGING_BLK

#define OSTASK_LOAD_IMEM_OVERLAYS_LS(pOvlListImemLS, ovlCntImemLS)             \
    OSTASK_LOAD_PINNED_IMEM_OVERLAYS_LS(pOvlListImemLS, ovlCntImemLS)

#define OSTASK_ATTACH_IMEM_OVERLAY_TO_TASK(pTask, ovlIdxImem)                  \
    OSTASK_ATTACH_PINNED_IMEM_OVERLAY_TO_TASK(pTask, ovlIdxImem)
#define OSTASK_DETACH_IMEM_OVERLAY_FROM_TASK(pTask, ovlIdxImem)                \
    OSTASK_DETACH_PINNED_IMEM_OVERLAY_FROM_TASK(pTask, ovlIdxImem)

#define OSTASK_ATTACH_IMEM_OVERLAY_COND(ovlIdxImem)                            \
    OSTASK_ATTACH_PINNED_IMEM_OVERLAY_COND(ovlIdxImem)
#define OSTASK_DETACH_IMEM_OVERLAY_COND(ovlIdxImem)                            \
    OSTASK_DETACH_PINNED_IMEM_OVERLAY_COND(ovlIdxImem)

#define OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(ovlIdxImem)                        \
    OSTASK_ATTACH_AND_LOAD_PINNED_IMEM_OVERLAY(ovlIdxImem)

#endif // ON_DEMAND_PAGING_BLK

#define OSTASK_ATTACH_IMEM_OVERLAY(ovlIdxImem)                                 \
    OSTASK_ATTACH_IMEM_OVERLAY_TO_TASK(NULL, ovlIdxImem)
#define OSTASK_DETACH_IMEM_OVERLAY(ovlIdxImem)                                 \
    OSTASK_DETACH_IMEM_OVERLAY_FROM_TASK(NULL, ovlIdxImem)

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
#ifdef ON_DEMAND_PAGING_BLK
LwS32   lwosOvlImemLoadPinnedOverlayList(LwU8 *pOvlList, LwU8 ovlCnt);
#endif // ON_DEMAND_PAGING_BLK

#if defined(UPROC_RISCV)
#define lwosOvlImemLoadList(pOvlList, ovlTotalCntImem, bHS)          (0)
#else
LwS32   lwosOvlImemLoadList(LwU8 *pOvlList, LwU8 ovlTotalCntImem, LwBool bHS);
#endif

#ifdef ON_DEMAND_PAGING_OVL_IMEM
void    lwosOvlImemOnDemandPaging(LwU32 exPc)
    GCC_ATTRIB_SECTION("imem_resident", "lwosOvlImemOnDemandPaging");
#endif // ON_DEMAND_PAGING_OVL_IMEM

#endif // LWOS_OVL_IMEM_H
