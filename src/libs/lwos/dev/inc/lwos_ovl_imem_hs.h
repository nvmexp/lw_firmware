/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWOS_OVL_IMEM_HS_H
#define LWOS_OVL_IMEM_HS_H

/*!
 * @file    lwos_ovl_imem_hs.h
 * @brief   Macros handling HS IMEM overlays.
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ External Definitions ---------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Retrieves the number of used IMEM HS overlays (on current build) in the
 * format we use for overlay indices.
 */
#define OVL_COUNT_IMEM_HS()             OS_LABEL(_num_imem_hs_overlays)

/*!
 * @copydoc OSTASK_ATTACH_PINNED_IMEM_OVERLAY_TO_TASK
 */
#define OSTASK_ATTACH_IMEM_OVERLAY_TO_TASK_HS(pTask, ovlIdxImem)               \
    lwosTaskOverlayAttachAndFail(pTask, ovlIdxImem, LW_FALSE, LW_TRUE)

/*!
 * @copydoc OSTASK_ATTACH_PINNED_IMEM_OVERLAY
 */
#define OSTASK_ATTACH_IMEM_OVERLAY_HS(ovlIdxImem)                              \
    OSTASK_ATTACH_IMEM_OVERLAY_TO_TASK_HS(NULL, ovlIdxImem)

/*!
 * @copydoc OSTASK_ATTACH_PINNED_IMEM_OVERLAY_COND
 */
#define OSTASK_ATTACH_IMEM_OVERLAY_COND_HS(ovlIdxImem)                         \
    lwosTaskOverlayAttach(NULL, ovlIdxImem, LW_FALSE, LW_TRUE)

/*!
 * @copydoc OSTASK_ATTACH_AND_LOAD_PINNED_IMEM_OVERLAY
 */
#define OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(ovlIdxImem)                     \
    do {                                                                       \
        OSTASK_ATTACH_IMEM_OVERLAY_HS(ovlIdxImem);                             \
        lwrtosRELOAD();                                                        \
    } while (LW_FALSE)

/*!
 * @copydoc OSTASK_DETACH_PINNED_IMEM_OVERLAY_FROM_TASK
 */
#define OSTASK_DETACH_IMEM_OVERLAY_FROM_TASK_HS(pTask, ovlIdxImem)             \
    lwosTaskOverlayDetachAndFail(pTask, ovlIdxImem, LW_FALSE, LW_TRUE)

/*!
 * @copydoc OSTASK_DETACH_PINNED_IMEM_OVERLAY
 */
#define OSTASK_DETACH_IMEM_OVERLAY_HS(ovlIdxImem)                              \
    OSTASK_DETACH_IMEM_OVERLAY_FROM_TASK_HS(NULL, ovlIdxImem)

/*!
 * @copydoc OSTASK_DETACH_PINNED_IMEM_OVERLAY_COND
 */
#define OSTASK_DETACH_IMEM_OVERLAY_COND_HS(ovlIdxImem)                         \
    lwosTaskOverlayDetach(NULL, ovlIdxImem, LW_FALSE, LW_TRUE)

/*!
 * @copydoc OSTASK_LOAD_PINNED_IMEM_OVERLAYS_LS
 */
#define OSTASK_LOAD_IMEM_OVERLAYS_HS(pOvlListImem, ovlCntImem)                 \
    do {                                                                       \
        if (lwosOvlImemLoadList(pOvlListImem, ovlCntImem, LW_TRUE) < 0U)       \
        {                                                                      \
            lwuc_halt();                                                       \
        }                                                                      \
    } while(LW_FALSE)

/*!
 * @note    Subsequent defines does not logically belong to this file.
 *          They are used only within SEC2 ucode and in order to simplify
 *          shared defines we are moving them here until SEC2 is cleaned-up.
 *          TODO: AI for this owns Sahil Sapre.
 */
#define OSTASK_DETACH_DMEM_OVERLAY_COND(ovlIdxDmem)                            \
    lwosTaskOverlayDetach(NULL, ovlIdxDmem, LW_TRUE, LW_FALSE)

#define OSTASK_LOAD_DMEM_OVERLAYS(pOvlListDmem, ovlCntDmem)                    \
    do {                                                                       \
        if (lwosOvlDmemLoadList(pOvlListDmem, ovlCntDmem) < 0U)                \
        {                                                                      \
            lwuc_halt();                                                       \
        }                                                                      \
    } while(LW_FALSE)

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */

#endif // LWOS_OVL_IMEM_HS_H
