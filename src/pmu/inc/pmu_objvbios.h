/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_objvbios.h
 * @brief   Contains the structure used between all components of the VBIOS IED parser
 */

#ifndef PMU_OBJVBIOS_H
#define PMU_OBJVBIOS_H

/* ------------------------ System includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application includes ---------------------------- */
#include "vbios/vbios_dirt.h"
#include "vbios/vbios_frts.h"
#include "vbios/vbios_sku.h"

/* ------------------------ Types definitions ------------------------------- */
typedef struct
{
    RM_FLCN_MEM_DESC *pVbiosDesc;           // The memory descriptor for the VBIOOS copy in FB.
    LwU16             nOffsetLwrrent;       // Current instruction pointer in the IED script.
    LwU16             conditionTableOffset; // Offset of the condition table for the IED scripts.
    LwU8              nOpcode;              // Current opcode number.
    LwBool            bCompleted;           // Indicates if IED script or subroutine exelwtion is complete.
    LwBool            bCondition;           // Indicates the state of the IED script codition.
    LwU8              dpPortNum;            // AUX port number of the DP monitor.
    LwU8              orIndex;              // Output Resource index of the DP monitor.
    LwU8              linkIndex;            // DP link index. 
} VBIOS_IED_CONTEXT;

typedef enum
{
    IED_TABLE_COMPARISON_EQ,
    IED_TABLE_COMPARISON_GE
} IED_TABLE_COMPARISON;

/*!
 * Structure holding overarching VBIOS data.
 */
typedef struct
{
#if PMUCFG_FEATURE_ENABLED(PMU_VBIOS_DIRT)
    /*!
     * Structure containing mappings from @ref LW_GFW_DIRT IDs to VBIOS table
     * data.
     */
    VBIOS_DIRT dirt;
#endif // PMUCFG_FEATURE_ENABLED(PMU_VBIOS_DIRT)

#if PMUCFG_FEATURE_ENABLED(PMU_VBIOS_SKU)
    /*!
     * SKU identification information for this board.
     */
    VBIOS_SKU sku;
#endif

#if !PMUCFG_FEATURE_ENABLED(PMU_VBIOS_DIRT) && \
    !PMUCFG_FEATURE_ENABLED(PMU_VBIOS_SKU)
    /*!
     * Dummy field in case no features are enabled.
     */
    LwU8 dummy;
#endif
} OBJVBIOS;

/*!
 * Structure holding overarching VBIOS data that is only needed during pre-init.
 */
typedef struct
{
#if PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS)
    /*!
     * The configuration settings for Firmware Runtime Security.
     */
    VBIOS_FRTS frts;
#endif  // PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS)

#if !PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS)
    /*!
     * Dummy field in case no features are enabled.
     */
    LwU8 dummy;
#endif
} OBJVBIOS_PRE_INIT;

/* ------------------------ HAL Include ------------------------------------- */
#include "config/g_vbios_hal.h"

/* ------------------------ Global Variables -------------------------------- */
/*!
 * Global VBIOS object.
 */
extern OBJVBIOS Vbios;

/*!
 * Pre-init portions of the global VBIOS object.
 */
extern OBJVBIOS_PRE_INIT VbiosPreInit;

/* ------------------------ Public interfaces ------------------------------- */
FLCN_STATUS vbiosCheckValid(RM_FLCN_MEM_DESC *pVbiosDesc);

FLCN_STATUS vbiosIedExelwteScript(RM_FLCN_MEM_DESC *pVbiosDesc, LwU16 scriptOffset,
    LwU16 conditionTableOffset, LwU8 dpPortNum, LwU8 orIndex, LwU8 linkIndex); 

FLCN_STATUS vbiosIedExelwteScriptTable(RM_FLCN_MEM_DESC *pVbiosDesc,
    LwU16 scriptOffset, LwU16 conditionTableOffset, LwU8 dpPortNum,
    LwU8  orIndex, LwU8 linkIndex, LwU32 data, LwU32 dataSizeByte, 
    IED_TABLE_COMPARISON comparison);

FLCN_STATUS vbiosIedExelwteScriptSub(VBIOS_IED_CONTEXT *pCtx);

FLCN_STATUS vbiosIedFetch(VBIOS_IED_CONTEXT *pCtx, LwU8 *pData);

FLCN_STATUS vbiosIedFetchBlock(VBIOS_IED_CONTEXT *pCtx, void *pData, LwU32 maxUnpackedSize);

FLCN_STATUS vbiosIedCheckCondition(VBIOS_IED_CONTEXT *pCtx, LwU8 nCondition, LwBool *pResult);

/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @brief   Retrives @ref OBJVBIOS_PRE_INIT::frts, if enabled
 *
 * @param[in]   pVbios
 * @param[out]  ppFrts  Pointer into which to store pointer to FRTS structure
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  A pointer argument was @ref NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
vbiosFrtsGet
(
    OBJVBIOS_PRE_INIT *pVbios,
    VBIOS_FRTS **ppFrts
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pVbios != NULL) &&
        (ppFrts != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosFrtsFrtsGet_exit);

#if PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS)
    *ppFrts = &pVbios->frts;
#else // PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS)
    *ppFrts = NULL;
#endif // PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS)

vbiosFrtsFrtsGet_exit:
   return status;
}

/*!
 * @brief   Retrives @ref OBJVBIOS::dirt, if enabled
 *
 * @param[in]   pVbios
 * @param[out]  ppDirt  Pointer into which to store pointer to config
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  A pointer argument was @ref NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
vbiosDirtGet
(
    OBJVBIOS *pVbios,
    VBIOS_DIRT **ppDirt
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pVbios != NULL) &&
        (ppDirt != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosDirtGet_exit);

#if PMUCFG_FEATURE_ENABLED(PMU_VBIOS_DIRT)
    *ppDirt = &pVbios->dirt;
#else // PMUCFG_FEATURE_ENABLED(PMU_VBIOS_DIRT)
    *ppDirt = NULL;
#endif // PMUCFG_FEATURE_ENABLED(PMU_VBIOS_DIRT)

vbiosDirtGet_exit:
    return status;
}

/*!
 * @copydoc vbiosDirtGet
 *
 * @note    Supports providing a const @ref OBJVBIOS pointer and retreiving a
 *          const @ref VBIOS_DIRT pointer
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
vbiosDirtGetConst
(
    const OBJVBIOS *pVbios,
    const VBIOS_DIRT **ppDirt
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pVbios != NULL) &&
        (ppDirt != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosDirtGetConst_exit);

#if PMUCFG_FEATURE_ENABLED(PMU_VBIOS_DIRT)
    *ppDirt = &pVbios->dirt;
#else // PMUCFG_FEATURE_ENABLED(PMU_VBIOS_DIRT)
    *ppDirt = NULL;
#endif // PMUCFG_FEATURE_ENABLED(PMU_VBIOS_DIRT)

vbiosDirtGetConst_exit:
    return status;
}

/*!
 * @brief   Retrives @ref OBJVBIOS::sku, if enabled
 *
 * @param[in]   pVbios
 * @param[out]  ppDirt  Pointer into which to store pointer to SKU data
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  A pointer argument was @ref NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
vbiosSkuGet
(
    OBJVBIOS *pVbios,
    VBIOS_SKU **ppSku
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pVbios != NULL) &&
        (ppSku != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosSkuGet_exit);

#if PMUCFG_FEATURE_ENABLED(PMU_VBIOS_SKU)
    *ppSku = &pVbios->sku;
#else // PMUCFG_FEATURE_ENABLED(PMU_VBIOS_SKU)
    *ppSku = NULL;
#endif // PMUCFG_FEATURE_ENABLED(PMU_VBIOS_SKU)

vbiosSkuGet_exit:
    return status;
}

#endif  
