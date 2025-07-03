/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BOARDOBJ_H
#define BOARDOBJ_H

#include "g_boardobj.h"

#ifndef G_BOARDOBJ_H
#define G_BOARDOBJ_H

/*!
 * @file    boardobj.h
 *
 * @brief   Provides PMU-specific definitions for BOARDOBJ infrastructure.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu/pmuifcmn.h"
#include "pmu/pmuifboardobj.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct BOARDOBJ BOARDOBJ, BOARDOBJ_BASE;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   Accessor for BOARDOBJ::type.
 *
 * @pre     pObj must have a BOARDOBJ base class with zero offset within the
 *          object.
 *
 * @param[in]   pObj    Pointer to object extending BOARDOBJ.
 *
 * @return      BOARDOBJ::type for the passed in object.
 */
#define BOARDOBJ_GET_TYPE(pObj)     (((BOARDOBJ *)(pObj))->type)

/*!
 * Accessor macro for BOARDOBJ::classId.
 *
 * @note This macro is used to hide implementation details of the
 * PMU_BOARDOBJ_CLASS_ID feature.
 *
 * @param[in] _pBoardObj  BOARDOBJ pointer.
 *
 * @return @ref BOARDOBJ::classId if PMU_BOARDOBJ_CLASS_ID feature is enabled
 * @return LW_U8_MAX otherwise
 */
#if PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJ_CLASS_ID)
#define BOARDOBJ_GET_CLASS_ID(_pBoardObj) ((_pBoardObj)->classId)
#else
#define BOARDOBJ_GET_CLASS_ID(_pBoardObj) LW2080_CTRL_BOARODBJGRP_CLASS_ID_ILWALID
#endif

/*!
 * Mutator macro for BOARDOBJ::classId.
 *
 * @note This macro is used to hide implementation details of the
 * PMU_BOARDOBJ_CLASS_ID feature.
 *
 * @param[in] _pBoardObj  BOARDOBJ pointer.
 * @param[in] _classId
 *     LW2080_CTRL_BOARDOBJGRP_CLASS_ID value to set in BOARDOBJ.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJ_CLASS_ID)
#define BOARDOBJ_SET_CLASS_ID(_pBoardObj, _classId)       \
    do { (_pBoardObj)->classId = (_classId); } while (LW_FALSE)
#else
#define BOARDOBJ_SET_CLASS_ID(_pBoardObj, _classId) lwosNOP()
#endif

/*!
 * @brief   Safely cast from one BOARDOBJ type to another within the same class
 *          by first checking the run-time type information (RTTI) embedded in
 *          each BOARDBJ.
 *
 * @note    In order to cast to a base class, such as PSTATE, one must pass in
 *          BASE as the _type and have their structure type defined
 *          appropriately to this name; i.e. typedef PSTATE PSTATE_BASE;.
 *
 * @note    Explicitly NOT using BOARDOBJ_UNSAFE_CAST(...) as a replacement for
 *          (_class##_##_type *) when
 *          PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJ_DYNAMIC_CAST) so that this macro
 *          can be marked as waived for MISRA-C 2012 Rule 11.3 violations.
 *
 * @param[in]       _pBoardobj  The BOARDOBJ pointer from which we are dynamic
 *                              casting.
 * @param[in]       _unit       The unit the BOARDOBJGRP class belongs to.
 * @param[in]       _class      The class of the BOARDOBJGRP the BOARDOBJ
 *                              belongs.
 * @param[in]       _type       The destination BOARDOBJ::type to which we are
 *                              casting.
 *
 * @return  Pointer of type _typeCast if all type checks pass.
 * @return  NULL if any type checks fail.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJ_DYNAMIC_CAST)
#define BOARDOBJ_DYNAMIC_CAST(_pBoardobj, _unit, _class, _type)                     \
    (_class##_##_type *)boardObjDynamicCast(BOARDOBJGRP_GRP_GET(_class),            \
                                   (_pBoardobj),                                    \
                                   LW2080_CTRL_BOARDOBJGRP_CLASS_ID(_unit, _class), \
                                   LW2080_CTRL_BOARDOBJ_TYPE(_unit, _class, _type))
#else
#define BOARDOBJ_DYNAMIC_CAST(_pBoardobj, _unit, _class, _type)                     \
    BOARDOBJ_UNSAFE_CAST(_pBoardobj, _unit, _class, _type)
#endif

/*!
 * @brief   Safely cast from one BOARDOBJ type to another within the same class.
 *          In case of failure, the specified label will be jumped to (goto).
 *
 * @note    In order to cast to a base class, such as PSTATE, one must pass in
 *          BASE as the _type and have their structure type-defined
 *          appropriately to this name; i.e. typedef PSTATE PSTATE_BASE;.
 *
 * @param[in, out]  _pDst       Destination pointer to be set by dynamic cast.
 *                              Will be NULL if the cast fails.
 * @param[in]       _pBoardobj  The BOARDOBJ pointer from which we are dynamic
 *                              casting.
 * @param[in]       _unit       The unit the BOARDOBJGRP class belongs to.
 * @param[in]       _class      The class of the BOARDOBJGRP the BOARDOBJ
 *                              belongs.
 * @param[in]       _type       The destination BOARDOBJ::type to which we are
 *                              casting.
 * @param[in,out]   _status     FLCN_STATUS variable to be set by the underlying
 *                              function call.
 * @param[in]       _label      Label to jump to (goto) in case of failure.
 *
 * @return  In the case the cast fails, FLCN_ERR_ILWALID_CAST will be set to
 *          _status and the provided _label will be jumped to.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJ_DYNAMIC_CAST)
#define BOARDOBJ_DYNAMIC_CAST_OR_GOTO(_pDst, _pBoardobj, _unit, _class, _type, _pStatus, _label)    \
    do {                                                                                            \
        (_pDst) = BOARDOBJ_DYNAMIC_CAST(_pBoardobj, _unit, _class, _type);                          \
        if ((_pDst) == NULL)                                                                        \
        {                                                                                           \
            *(_pStatus) = FLCN_ERR_ILWALID_CAST;                                                    \
            goto _label;                                                                            \
        }                                                                                           \
        else                                                                                        \
        {                                                                                           \
            *(_pStatus) = FLCN_OK;                                                                  \
        }                                                                                           \
    } while (LW_FALSE)
#else
#define BOARDOBJ_DYNAMIC_CAST_OR_GOTO(_pDst, _pBoardobj, _unit, _class, _type, _pStatus, _label)    \
    do {                                                                                            \
        (_pDst) = BOARDOBJ_DYNAMIC_CAST(_pBoardobj, _unit, _class, _type);                          \
        if ((_pStatus) != NULL)                                                                     \
        {                                                                                           \
            *(_pStatus) = FLCN_OK;                                                                  \
        }                                                                                           \
        if (LW_FALSE)                                                                               \
        {                                                                                           \
            goto _label;                                                                            \
        }                                                                                           \
    } while (LW_FALSE)
#endif

/*!
 * @brief   Safely cast from one BOARDOBJ type to another within the same class.
 *          In case of failure, the calling function will return with _status.
 *
 * @note    In order to cast to a base class, such as PSTATE, one must pass in
 *          BASE as the _type and have their structure type-defined
 *          appropriately to this name; i.e. typedef PSTATE PSTATE_BASE;.
 *
 * @param[in, out]  _pDst       Destination pointer to be set by dynamic cast.
 *                              Will be NULL if the cast fails.
 * @param[in]       _pBoardobj  The BOARDOBJ pointer from which we are dynamic
 *                              casting.
 * @param[in]       _unit       The unit the BOARDOBJGRP class belongs to.
 * @param[in]       _class      The class of the BOARDOBJGRP the BOARDOBJ
 *                              belongs.
 * @param[in]       _type       The destination BOARDOBJ::type to which we are
 *                              casting.
 * @param[in]       _typeCast   The type to be casted to.
 *
 * @return  In the case the cast fails, FLCN_ERR_ILWALID_CAST will be returned
 *          by the calling function.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJ_DYNAMIC_CAST)
#define BOARDOBJ_DYNAMIC_CAST_OR_RETURN(_pDst, _pBoardobj, _unit, _class, _type)    \
    do {                                                                            \
        (_pDst) = BOARDOBJ_DYNAMIC_CAST(_pBoardobj, _unit, _class, _type);          \
        if ((_pDst) == NULL)                                                        \
        {                                                                           \
            return FLCN_ERR_ILWALID_CAST;                                           \
        }                                                                           \
    } while (LW_FALSE)
#else
#define BOARDOBJ_DYNAMIC_CAST_OR_RETURN(_pDst, _pBoardobj, _unit, _class, _type)    \
    (_pDst) = BOARDOBJ_DYNAMIC_CAST(_pBoardobj, _unit, _class, _type)
#endif

/*!
 * @brief   Formalized way of casting BOARDOBJ pointers to derived types.
 *
 * @note    This cast is NOT type safe. Simply does a C-Style cast on a BOADOBJ
 *          pointer.
 *
 * @param[in]  _pBoardobj  Pointer to BOARDOBJ to be casted.
 * @param[in]  _unit       The unit _pBoardobj belongs to. Unused in this macro.
 * @param[in]  _class      The class _pBoardobj belongs to.
 * @param[in]  _type       The leaf type of the class that _pBoardobj is.
 *
 * @return  Down-casted pointer of type (_class##_##_type *) at the same address
 *          of _pBoardobj.
 */
#define BOARDOBJ_UNSAFE_CAST(_pBoardobj, _unit, _class, _type)                 \
    (_class##_##_type *)(_pBoardobj)

/* ------------------------ Function Prototypes ----------------------------- */
/*!
 * @brief   Check to see if a given BOAROBJ can be safely casted to a desired
 *          type.
 *
 * @note    Calling of this interface should be protected with a feature check
 *          of PMU_BOARDOBJ_DYNAMIC_CAST.
 *
 * @param[in]       pBoardobjGrp    Group that the object belongs to.
 * @param[in]       pBoardObj       Object to be casted.
 * @param[in]       requestedClass  BoardObjGrp class that this object should
 *                                  belong to.
 * @param[in]       requestedType   BoardObj type within the class's hierarchy.
 *
 * @return  Pointer to be casted if pBoardObj can be casted to the requested
 *          type of the given class.
 * @return  NULL if the object cannot be casted to the requested class+type.
 */
#define BoardObjDynamicCast(fname) void* (fname)(BOARDOBJGRP *pBoardObjGrp, BOARDOBJ *pBoardObj, LW2080_CTRL_BOARDOBJGRP_CLASS_ID requestedClass, LwU8 requestedType)

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief   BOARDOBJ virtual base class in PMU microcode.
 *
 * @note    There should be NO object instances of this type. Object instances
 *          should be classes derived from BOARDOBJ.
 */
struct BOARDOBJ
{
    /*!
     * @brief Index of the BOARDOBJ within the BOARDOBJGRP. At
     * construct time set to @ref LW2080_CTRL_BOARDOBJ_IDX_ILWALID
     * until object is inserted into BOARDOBJGRP.
     *
     * @note Must be first in structure to allow tight packing of
     * following LwU8 parameters, avoiding regressing DMEM footprint
     * on older, DMEM-constrained profiles.
     */
    LwBoardObjIdx  grpIdx;
#if PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJ_CLASS_ID)
    /*!
     * @brief   Object's Class ID as @ref LW2080_CTRL_BOARDOBJGRP_CLASS_ID_ENUM.
     */
    LW2080_CTRL_BOARDOBJGRP_CLASS_ID    classId;
#endif
    /*!
     * @brief   BOARDOBJ type.
     *
     * This should be a unique value within the class that the BOARDOBJ belongs.
     */
    LwU8    type;
};


/* ------------------------ Inline Functions  -------------------------------- */
/*!
 * Helper function which to colwert a LwBoardObjIdx to LwU8, handling
 * any necessary truncation above @ref
 * LW2080_CTRL_BOARDOBJ_IDX_ILWALID_8BIT.
 *
 * @param[in]  grpIdx   LwBoardObjIdx value to to colwert to 8bit.
 *
 * @return grpIdx as 8-bit value, ceilinged to
 * LW2080_CTRL_BOARDOBJ_IDX_ILWALID_8BIT.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwU8
BOARDOBJ_GRP_IDX_TO_8BIT
(
    LwBoardObjIdx grpIdx
)
{
    //
    // Handle cases where grpIdx is larger than 8bits and cast to
    // LW2080_CTRL_BOARDOBJ_IDX_ILWALID_8BIT, as any value above 8-bits
    // is equally invalid as an 8-bit value.
    //
    if (grpIdx > LW2080_CTRL_BOARDOBJ_IDX_ILWALID_8BIT)
    {
        //
        // LW2080_CTRL_BOARDOBJ_IDX_ILWALID is the only expected
        // value, all others are not expected in the context of an
        // 8-bit group (i.e. _E32 or _E255).  So, throw a breakpoint
        // if we encounter those unexpected values.
        //
        if (grpIdx != LW2080_CTRL_BOARDOBJ_IDX_ILWALID)
        {
            PMU_BREAKPOINT();
        }
        return LW2080_CTRL_BOARDOBJ_IDX_ILWALID_8BIT;
    }

    return (LwU8)grpIdx;
}

/*!
 * Helper function which to provide a LwBoardObjIdx index from an LwU8
 * index input, handling any necessary colwersion
 * LW2080_CTRL_BOARDOBJ_IDX_ILWALID_8BIT ->
 * LW2080_CTRL_BOARDOBJ_IDX_ILWALID.
 *
 * @param[in]  grpIdx   8bit index value to to colwert to LwBoardObjIdx.
 *
 * @return grpIdx as an LwBoardObjIdx value, including necessary
 * colwersion LW2080_CTRL_BOARDOBJ_IDX_ILWALID_8BIT ->
 * LW2080_CTRL_BOARDOBJ_IDX_ILWALID.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwBoardObjIdx
BOARDOBJ_GRP_IDX_FROM_8BIT
(
    LwU8 grpIdx
)
{
    return (grpIdx == LW2080_CTRL_BOARDOBJ_IDX_ILWALID_8BIT) ?
        LW2080_CTRL_BOARDOBJ_IDX_ILWALID :
        (LwBoardObjIdx)grpIdx;
}

/*!
 * @brief   Accessor for BOARDOBJ::grpIdx.
 *
 * @param[in]   pBoardObj    BOARDOBJ pointer.
 *
 * @return  @ref BOARDOBJ::grpIdx for the passed in object.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwBoardObjIdx
BOARDOBJ_GET_GRP_IDX
(
    BOARDOBJ *pBoardObj
)
{
    return pBoardObj->grpIdx;
}

/*!
 * @brief   Accessor for @ref BOARDOBJ::grpIdx, but returning an 8-bit value.
 *
 * If @ref BOARDOBJ::grpIdx is > @ref
 * LW2080_CTRL_BOARDOBJ_IDX_ILWALID_8BIT, this funcion returns @ref
 * LW2080_CTRL_BOARDOBJ_IDX_ILWALID_8BIT.  This is safe assumption, as
 * both values are be equally invalid in the context of an 8-bit
 * value.  This case is never expected to happen, so providing a
 * PMU_BREAKPOINT() to identify those cases to be fixed.
 *
 * @note This accessor is provided for usage with legacy appcode cases
 * in which the BOARDOBJ index is always stored as an 8-bit variable
 * (i.e. these appcodes are implemetning @ref BOARDOBJGRP_E32 or @ref
 * BOARDOBJGRP_E255).
 *
 *
 * @param[in]   pBoardObj    BOARDOBJ pointer.
 *
 * @return @ref BOARDOBJ::grpIdx for the passed in object, as an 8-bit
 * type.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwU8
BOARDOBJ_GET_GRP_IDX_8BIT
(
    BOARDOBJ *pBoardObj
)
{
    return BOARDOBJ_GRP_IDX_TO_8BIT(BOARDOBJ_GET_GRP_IDX(pBoardObj));
}

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
mockable BoardObjDynamicCast (boardObjDynamicCast);

/* ------------------------ Include Derived Types --------------------------- */
#include "boardobj/boardobj_vtable.h"

#endif // G_BOARDOBJ_H
#endif // BOARDOBJ_H
