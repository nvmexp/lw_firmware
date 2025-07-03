/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWOS_OVL_DESC_H
#define LWOS_OVL_DESC_H

/*!
 * @file    lwos_ovl_desc.h
 * @brief   Overlay descriptor macros and defines.
 */

/* ------------------------ System Includes --------------------------------- */
#include "lwuproc.h"

/* ------------------------ External Definitions ---------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief   Definition of an overlay descriptor type.
 *
 * Memory overlay indexes are unsigned 8-bit values with overlapping ranges for
 * IMEM and DMEM making it impossible to distinguish them at runtime. An overlay
 * descriptor complements overlay index with information necesary to distinguish
 * IMEM and DMEM overlays (@see LW_OSTASK_OVL_DESC_TYPE). It also allows caller
 * to specify a desired overlay action (@see LW_OSTASK_OVL_DESC_ACTION) allowing
 * batch processing (@see osTaskOverlaysProcessDesc).
 *
 * @note    Reserved bits can be re-used for future enhancements.
 */
typedef LwU16 OSTASK_OVL_DESC;

/*!
 * @brief   Defines bit-fields withing @ref OSTASK_OVL_DESC overlay descriptor.
 *
 * _TYPE    Defines overlay type (IMEM vs. DMEM).
 * _ACTION  Defines required overlay action (attach vs. detach).
 * _MODE    Defines HS or LS overlay.
 */
#define LW_OSTASK_OVL_DESC_INDEX                                             7:0
#define LW_OSTASK_OVL_DESC_TYPE                                              8:8
#define LW_OSTASK_OVL_DESC_TYPE_IMEM                                 0x00000000U
#define LW_OSTASK_OVL_DESC_TYPE_DMEM                                 0x00000001U
#define LW_OSTASK_OVL_DESC_ACTION                                            9:9
#define LW_OSTASK_OVL_DESC_ACTION_ATTACH                             0x00000000U
#define LW_OSTASK_OVL_DESC_ACTION_DETACH                             0x00000001U
#define LW_OSTASK_OVL_DESC_MODE                                            10:10
#define LW_OSTASK_OVL_DESC_MODE_LS                                   0x00000000U
#define LW_OSTASK_OVL_DESC_MODE_HS                                   0x00000001U
#define LW_OSTASK_OVL_DESC_RSVD                                            15:11

/*!
 * @brief   Helper macros generating an overlay descriptor using its index/name.
 *
 * @note    It is preffered to use @ref OSTASK_OVL_DESC_BUILD macro since it
 *          will assure that the index is indeed a valid @ref _type overlay.
 *
 * @note    ***_VAL macros define the list values while macros with the trailing
 *          commas are required to build lists supporting ODP overlay exclusion.
 *
 * @param[in]   _type               Type of the target overlay (_IMEM or _DMEM).
 * @param[in]   _action             Required action (_ATTACH or _DETACH).
 * @param[in]   _mode               Mode of the target overlay (_LS or _HS).
 * @param[in]   _ovlIdx/_ovlname    Index/Name of the target overlay.
 *
 * @return  Well formed overlay descriptor.
 */
#define OSTASK_OVL_DESC_BUILD_IDX_VAL(_type, _action, _mode, _ovlIdx)          \
    ((OSTASK_OVL_DESC)(DRF_NUM(_OSTASK, _OVL_DESC, _INDEX, (_ovlIdx))  |       \
                       DRF_DEF(_OSTASK, _OVL_DESC, _TYPE,   _type)     |       \
                       DRF_DEF(_OSTASK, _OVL_DESC, _ACTION, _action)   |       \
                       DRF_DEF(_OSTASK, _OVL_DESC, _MODE,   _mode)))

#define OSTASK_OVL_DESC_ILWALID



#if !defined(UPROC_RISCV)

#define OSTASK_OVL_DESC_BUILD_IDX_VAL_IMEM(_action, _mode, _ovlIdx)            \
    OSTASK_OVL_DESC_BUILD_IDX_VAL(_IMEM, _action, _mode, _ovlIdx),

#else

#define OSTASK_OVL_DESC_BUILD_IDX_VAL_IMEM(_action, _mode, _ovlIdx)            \
    OSTASK_OVL_DESC_ILWALID

#endif


#if defined(DMEM_VA_SUPPORTED) && !defined(UPROC_RISCV)

#define OSTASK_OVL_DESC_BUILD_IDX_VAL_DMEM(_action, _mode, _ovlIdx)            \
    OSTASK_OVL_DESC_BUILD_IDX_VAL(_DMEM, _action, _mode, _ovlIdx),

#else // DMEM_VA_SUPPORTED

#define OSTASK_OVL_DESC_BUILD_IDX_VAL_DMEM(_action, _mode, _ovlIdx)            \
    OSTASK_OVL_DESC_ILWALID

#endif // DMEM_VA_SUPPORTED



#define OSTASK_PINNED_OVL_DESC_BUILD_IDX(_type, _action, _mode, _ovlIdx)       \
    OSTASK_OVL_DESC_BUILD_IDX_VAL##_type(_action, _mode, _ovlIdx)

#define OSTASK_PINNED_OVL_DESC_BUILD(_type, _action, _mode, _ovlName)          \
    OSTASK_PINNED_OVL_DESC_BUILD_IDX(_type, _action, _mode, OVL_INDEX##_type(_ovlName))



#if defined(ON_DEMAND_PAGING_BLK)

#define OSTASK_OVL_DESC_BUILD_IDX(_type, _action, _mode, _ovlIdx)              \
    OSTASK_OVL_DESC_ILWALID

#else // ON_DEMAND_PAGING_BLK

#define OSTASK_OVL_DESC_BUILD_IDX(_type, _action, _mode, _ovlIdx)              \
    OSTASK_OVL_DESC_BUILD_IDX_VAL##_type(_action, _mode, _ovlIdx)

#endif // ON_DEMAND_PAGING_BLK

#define OSTASK_OVL_DESC_BUILD(_type, _action, _ovlName)                        \
    OSTASK_OVL_DESC_BUILD_IDX(_type, _action, _LS, OVL_INDEX##_type(_ovlName))

#define OSTASK_OVL_DESC_BUILD_HS(_type, _action, _ovlName)                     \
    OSTASK_OVL_DESC_BUILD_IDX(_type, _action, _HS, OVL_INDEX##_type(_ovlName))

/*!
 * @brief   Defines an overlay descriptor that results in no action.
 *
 * Example use-case:
 *  {
 *      OSTASK_OVl_DESC list[] = {
 *          OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, name0)
 *          OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, name1)
 *          OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, name2)
 *      };
 *
 *      // Exclude overlay "name1"
 *  #ifndef ON_DEMAND_PAGING_BLK
 *      if (bRuntimeCondition)
 *      {
 *          list[1] = OSTASK_OVL_DESC_ILWALID_VAL;
 *      }
 *  #endif
 *
 *      OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER();
 *      {
 *          callSomething();
 *      }
 *      OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT();
 *  }
 */
#define OSTASK_OVL_DESC_ILWALID_VAL                                            \
    OSTASK_OVL_DESC_BUILD_IDX_VAL(_IMEM, _ATTACH, _LS, OVL_INDEX_ILWALID)

/*!
 * @brief   Helper macros simplifying handling of overlay attachment/detachment.
 *
 * Macros iterate over provided overlay descriptor list and perform requested
 * action (attachment / detachment). Overlay attachment and detachment is
 * conditional (only attached if not allready attached; only detach if already
 * attached).
 *
 * @param[in]   _list   Overlay descriptor list variable.
 *
 * @note    To support conditional functionality *ENTER() macro modifies list
 *          of overlay descriptors so this one cannot be statically allocated
 *          and has to be build each time code exelwtes.
 * @note    *EXIT() macro processes descriptors in ilwerse order assuring that
 *          it will never fail due to lack of overlay slots.
 */
#if !defined(UPROC_RISCV)

#define OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(_list)                               \
    do {                                                                            \
        if (LW_ARRAY_ELEMENTS(_list) > 0U)                                          \
        {                                                                           \
            lwosTaskOverlayDescListExec(_list, LW_ARRAY_ELEMENTS(_list), LW_FALSE); \
        }                                                                           \
    } while (LW_FALSE)

#define OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(_list)                                \
    do {                                                                            \
        if (LW_ARRAY_ELEMENTS(_list) > 0U)                                          \
        {                                                                           \
            lwosTaskOverlayDescListExec(_list, LW_ARRAY_ELEMENTS(_list), LW_TRUE);  \
        }                                                                           \
    } while (LW_FALSE)

#else

//
// These two macros had to be forked to avoid Coverity/MISRA problem:
// "This less-than-zero comparison of an unsigned value is never true. 0U > 0U".
//
#define OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(_list)                          \
    lwosVarNOP(_list)
#define OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(_list)                           \
    lwosVarNOP(_list)

#endif

/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
#define lwosTaskOverlayDescListExec MOCKABLE(lwosTaskOverlayDescListExec)
void    lwosTaskOverlayDescListExec(OSTASK_OVL_DESC *pOvlDescList, LwU32 ovlDescCnt, LwBool bReverse);

#endif // LWOS_OVL_DESC_H
