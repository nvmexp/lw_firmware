/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    vfe_equ_type.h
 * @brief   To define a new VFE index and related types and macros.
 *          The reasoning behind this file is to solve issues of
 *          cirlwlar header file dependence between VFE_EQU and
 *          VFE_VAR.
 *          Until this file, VFE_EQU depended on VFE_VAR header files
 *          for its own includes. However, this new requirement of
 *          >255 VFE entries means VFE_VAR also now will depend on
 *          VFE_EQU if the following are part of VFE_EQU header files.
 *          To avoid this cirlwlar dependency, we create a new file
 *          called VFE_EQU_TYPE which will be imported in both
 *          VFE_VAR and VFE_EQU header files.
 *
 * @note    Do not use these functions directly for VFE_EQU code
 *          or derived classes, instead use the VFE specific wrappers
 *          around these functions, or create VFE wrappers if need be.
 *          This is to facilitate the varying VFE index size
 *          (8-bit versus 16-bit) issue in PMU code.
 *
 */

#ifndef VFE_EQU_TYPE_H
#define VFE_EQU_TYPE_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * This feature dependent macro declares whether the VFE_EQU
 * index datatype is either 8-bit or 16-bit. The former is for
 * older chips (Ampere and before), and the latter is intended
 * for newer chips (Hopper and later).
 *
 * Apart from the index datatype itself, the invalid index value
 * for VFE_EQU is also changed, as are copy macros that copy
 * data between RM and PMU - since RM will always, regardless
 * of the underlying chip/PMU, use 16-bit VFE EQU index.
 * */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_EQU_16BIT_INDEX)
/*!
 * Datatype indicating the VFE index, required due to
 * chip-dependent VFE index and PMU DMEM size
 * */
typedef LwBoardObjIdx   LwVfeEquIdx;

/*!
 * Macro indicating the Invalid index of a VFE entry
 * */
#define PMU_PERF_VFE_EQU_INDEX_ILWALID LW2080_CTRL_PERF_VFE_EQU_INDEX_ILWALID

/*!
 * Macro to copy the VFE index from RM to PMU, specifically
 * from the RM-PMU shared structure to the PMU structure.
 *
 * This is required as the RM is planned to have 16-bit
 * indices for the VFE Index, while the PMU depending on chip
 * may have 8-bit or 16-bit index.
 *
 * @note    Eventually the macro for the non-feature case will be
 *          something like this:
 *          (_idxPmuStruct) = BOARDOBJ_GRP_IDX_TO_8BIT(_idxSharedStruct)
 * @note    The macro for the feature case will be unchanged.
 *
 * @param[in/out]   _idxPmuStruct       Index of the PMU structure, which
 *                                      may be either 8-bit or 16-bit.
 * @param[in]       _idxSharedStruct    Index of the RM structure, which
 *                                      will be 16-bit eventually.
 * */
#define VFE_EQU_IDX_COPY_RM_TO_PMU(_idxPmuStruct, _idxSharedStruct) \
    ((_idxPmuStruct) = (_idxSharedStruct))

/*!
 * Macro to copy the VFE index from PMU to RM, specifically
 * to the RM-PMU shared structure from the PMU structure.
 *
 * This is required as the RM is planned to have 16-bit
 * indices for the VFE Index, while the PMU depending on chip
 * may have 8-bit or 16-bit index.
 *
 * @note    Eventually the macro for the non-feature case will be
 *          something like this:
 *          (_idxSharedStruct) = BOARDOBJ_GRP_IDX_FROM_8BIT(_idxPmuStruct)
 * @note    The macro for the feature case will be unchanged.
 *
 * @param[in/out]   _idxSharedStruct    Index of the RM structure, which
 *                                      will be 16-bit eventually.
 * @param[in]       _idxPmuStruct       Index of the PMU structure, which
 *                                      may be either 8-bit or 16-bit.
 * */
#define VFE_EQU_IDX_COPY_PMU_TO_RM(_idxSharedStruct, _idxPmuStruct) \
    ((_idxSharedStruct) = (_idxPmuStruct))

/*!
 * Accessor macro for obtaining a VFE index size if the source
 * is always 16-bit (such as RMCTRL or RM-PMU structures), and
 * the destination is 8-bit.
 *
 * @note    Once VFE is ported to 512/1024 entries, this function
 *          will remain unchanged for PMU/chips supporting
 *          >255 entries. For older PMU/chips unable to do so,
 *          we use the @ref BOARDOBJ_GRP_IDX_FROM_8BIT or
 *          similar to extract only the 8-bits from the
 *          16-bit value.
 *
 * @param[in]   vfeIdx  The Index from the LwBoardObjIdx (16-bit)
 *
 * @return      Either the 8-bit truncation or the 16-bit
 *              original, depending on the PMU feature.
 * */
#define VFE_GET_PMU_IDX_FROM_BOARDOBJIDX(vfeIdx) \
    ((vfeIdx))

#else // PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_EQU_16BIT_INDEX)

/*!
 * Datatype indicating the VFE index, required due to
 * chip-dependent VFE index and PMU DMEM size
 * */
typedef LwU8   LwVfeEquIdx;

/*!
 * Macro indicating the Invalid index of a VFE entry
 * */
#define PMU_PERF_VFE_EQU_INDEX_ILWALID LW2080_CTRL_PERF_VFE_EQU_INDEX_ILWALID_8BIT

/*!
 * Macro to copy the VFE index from RM to PMU, specifically
 * from the RM-PMU shared structure to the PMU structure.
 *
 * This is required as the RM is planned to have 16-bit
 * indices for the VFE Index, while the PMU depending on chip
 * may have 8-bit or 16-bit index.
 *
 * @note    Eventually the macro for the non-feature case will be
 *          something like this:
 *          (_idxPmuStruct) = BOARDOBJ_GRP_IDX_TO_8BIT(_idxSharedStruct)
 * @note    The macro for the feature case will be unchanged.
 *
 * @param[in/out]   _idxPmuStruct       Index of the PMU structure, which
 *                                      may be either 8-bit or 16-bit.
 * @param[in]       _idxSharedStruct    Index of the RM structure, which
 *                                      will be 16-bit eventually.
 * */
#define VFE_EQU_IDX_COPY_RM_TO_PMU(_idxPmuStruct, _idxSharedStruct) \
    ((_idxPmuStruct) = BOARDOBJ_GRP_IDX_TO_8BIT(_idxSharedStruct))

/*!
 * Macro to copy the VFE index from PMU to RM, specifically
 * to the RM-PMU shared structure from the PMU structure.
 *
 * This is required as the RM is planned to have 16-bit
 * indices for the VFE Index, while the PMU depending on chip
 * may have 8-bit or 16-bit index.
 *
 * @note    Eventually the macro for the non-feature case will be
 *          something like this:
 *          (_idxSharedStruct) = BOARDOBJ_GRP_IDX_FROM_8BIT(_idxPmuStruct)
 * @note    The macro for the feature case will be unchanged.
 *
 * @param[in/out]   _idxSharedStruct    Index of the RM structure, which
 *                                      will be 16-bit eventually.
 * @param[in]       _idxPmuStruct       Index of the PMU structure, which
 *                                      may be either 8-bit or 16-bit.
 * */
#define VFE_EQU_IDX_COPY_PMU_TO_RM(_idxSharedStruct, _idxPmuStruct) \
    ((_idxSharedStruct) = BOARDOBJ_GRP_IDX_FROM_8BIT(_idxPmuStruct))

/*!
 * Accessor macro for obtaining a VFE index size if the source
 * is always 16-bit (such as RMCTRL or RM-PMU structures), and
 * the destination is 8-bit.
 *
 * @note    Once VFE is ported to 512/1024 entries, this function
 *          will remain unchanged for PMU/chips supporting
 *          >255 entries. For older PMU/chips unable to do so,
 *          we use the @ref BOARDOBJ_GRP_IDX_FROM_8BIT or
 *          similar to extract only the 8-bits from the
 *          16-bit value.
 *
 * @param[in]   vfeIdx  The Index from the LwBoardObjIdx (16-bit)
 *
 * @return      Either the 8-bit truncation or the 16-bit
 *              original, depending on the PMU feature.
 * */
#define VFE_GET_PMU_IDX_FROM_BOARDOBJIDX(vfeIdx) \
    (BOARDOBJ_GRP_IDX_TO_8BIT(vfeIdx))

#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_EQU_16BIT_INDEX)

/*!
 * @brief   Chip-dependent data-type for the index into the VF equation
 *          array
 *          This is meant for the expansion of VF equations from
 *          previously supported Max of 255, to at least 512, which
 *          require >8 bits for index (preferably 16).
 *
 * @note    We do not have VFE-specific functions/macros for
 *          mask ilwert, and, or etc. This is because the current
 *          calls to those functions/macros in the PMU client
 *          code are size-agnostic. OTOH, the functions for init
 *          and import mask are size-dependent, and the calls used
 *          by VFE clients have the _E255, which may change in
 *          future chips.
 *
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_EQU_BOARDOBJGRP_1024)

/*!
 * Data type indicating a mask object, where each bit
 * represents a VF equation.
 *
 * For chips using >255, this will be either 512 or 1024 bits wide,
 * i.e. BOARDOBJGRPMASK_E512 or BOARDOBJGRPMASK_E1024. For older
 * chips this will be BOARDOBJGRPMASK_E255.
 *
 * @note    As before, this will be the same until the VFE itself
 *          moves to 16-bit indices.
 */
typedef BOARDOBJGRPMASK_E1024 VFE_EQU_BOARDOBJGRPMASK;

/*!
 * Data type indicating a BOARDOBJGRP containing VFE entries.
 *
 * For chips using >255, this will be 1024 bits wide,
 * i.e. BOARDOBJGRP_E1024. For older
 * chips this will be BOARDOBJGRP_E255.
 *
 * @note    As before, this will be the same until the VFE itself
 *          moves to 16-bit indices.
 */
typedef BOARDOBJGRP_E1024 VFE_EQU_BOARDOBJGRP;

/*!
 * Wrapper to call the initialization function of the
 * VFE equation mask
 *
 * This will be mapped to the 512/1024 version in the
 * chips that use >255 VFE entries.
 *
 * @note    We have this since the init function used explicitly calls
 *          the _E255 class method for this initialization. Since
 *          depending on PMU, the class can change, we have this macro.
 *
 * @param[in/out]   _pMask  Pointer to the mask to be initialized.
 * */
#define vfeEquBoardObjMaskInit(_pMask) \
    boardObjGrpMaskInit_E1024((_pMask))

/*!
 * Wrapper to call the mask import function for VFE masks,
 * given that the PMU mask may be either 255 or more.
 *
 * @note    Once VFE is ported to 512/1024 entries, this function
 *          will be changed to copy only the first 255 entries, only
 *          in the event of the PMU/chip not supporting >255 entries.
 *
 * @note    We have this macro since there may be cases where RM
 *          supports 1024/512 VFE entries, but PMU (older chips) only
 *          supports 255 entries, and therefore only a partial copy
 *          needs to be done.
 *          Moreover, the calls used in VFE and clients explicitly
 *          use the _E255 class method, which may be different
 *          based on PMU class.
 *
 * @param[in]       _pExtMask   Pointer to mask to copy from, usually
 *                              from RM_PMU structure.
 * @param[in/out]   _pMask      Pointer to mask to copy into, in PMU.
 * */
#define vfeEquBoardObjGrpMaskImport(_pMask, _pExtMask) \
    boardObjGrpMaskImport_E1024((_pMask), (_pExtMask))

/*!
 * @copydoc BoardObjGrpIfaceModel10Set
 *
 * Wrapper around @ref BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA
 *
 * This is used due to the stringification requirements, and that
 * older PMU initializes 255 and the newer PMU initializes 1024.
 * */
#define VFE_EQU_BOARDOBJGRP_CONSTRUCT_AUTO_DMA(_class, _pBuffer, _hdrFunc, _entryFunc, _ssElement, _ovlIdxDmem, _ppObjectVtables) \
    BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E1024, _class, _pBuffer, _hdrFunc, _entryFunc, _ssElement, _ovlIdxDmem, _ppObjectVtables)

#else // PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_EQU_BOARDOBJGRP_1024)

/*!
 * Data type indicating a mask object, where each bit
 * represents a VF equation.
 *
 * For chips using >255, this will be either 512 or 1024 bits wide,
 * i.e. BOARDOBJGRPMASK_E512 or BOARDOBJGRPMASK_E1024. For older
 * chips this will be BOARDOBJGRPMASK_E255.
 *
 * @note    As before, this will be the same until the VFE itself
 *          moves to 16-bit indices.
 */
typedef BOARDOBJGRPMASK_E255 VFE_EQU_BOARDOBJGRPMASK;

/*!
 * Data type indicating a BOARDOBJGRP containing VFE entries.
 *
 * For chips using >255, this will be 1024 bits wide,
 * i.e. BOARDOBJGRP_E1024. For older
 * chips this will be BOARDOBJGRP_E255.
 *
 * @note    As before, this will be the same until the VFE itself
 *          moves to 16-bit indices.
 */
typedef BOARDOBJGRP_E255 VFE_EQU_BOARDOBJGRP;

/*!
 * Wrapper to call the initialization function of the
 * VFE equation mask
 *
 * This will be mapped to the 512/1024 version in the
 * chips that use >255 VFE entries.
 *
 * @note    We have this since the init function used explicitly calls
 *          the _E255 class method for this initialization. Since
 *          depending on PMU, the class can change, we have this macro.
 *
 * @param[in/out]   _pMask  Pointer to the mask to be initialized.
 * */
#define vfeEquBoardObjMaskInit(_pMask) \
    boardObjGrpMaskInit_E255((_pMask))

/*!
 * Wrapper to call the mask import function for VFE masks,
 * given that the PMU mask may be either 255 or more.
 *
 * @note    Once VFE is ported to 512/1024 entries, this function
 *          will be changed to copy only the first 255 entries, only
 *          in the event of the PMU/chip not supporting >255 entries.
 *
 * @note    We have this macro since there may be cases where RM
 *          supports 1024/512 VFE entries, but PMU (older chips) only
 *          supports 255 entries, and therefore only a partial copy
 *          needs to be done.
 *          Moreover, the calls used in VFE and clients explicitly
 *          use the _E255 class method, which may be different
 *          based on PMU class.
 *
 * @param[in]       _pExtMask   Pointer to mask to copy from, usually
 *                              from RM_PMU structure.
 * @param[in/out]   _pMask      Pointer to mask to copy into, in PMU.
 * */
#define vfeEquBoardObjGrpMaskImport(_pMask, _pExtMask) \
    boardObjGrpMaskImport_E255((_pMask), ((LW2080_CTRL_BOARDOBJGRP_MASK_E255 *)(_pExtMask)))

/*!
 * @copydoc BoardObjGrpIfaceModel10Set
 *
 * Wrapper around @ref BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA
 *
 * This is used due to the stringification requirements, and that
 * older PMU initializes 255 and the newer PMU initializes 1024.
 * */
#define VFE_EQU_BOARDOBJGRP_CONSTRUCT_AUTO_DMA(_class, _pBuffer, _hdrFunc, _entryFunc, _ssElement, _ovlIdxDmem, _ppObjectVtables) \
    BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E255, _class, _pBuffer, _hdrFunc, _entryFunc, _ssElement, _ovlIdxDmem, _ppObjectVtables)

#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_EQU_BOARDOBJGRP_1024)

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/* ------------------------ Include Derived Types --------------------------- */

#endif // VFE_EQU_TYPE_H
