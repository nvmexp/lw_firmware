/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BOARDOBJGRP_H
#define BOARDOBJGRP_H

#include "g_boardobjgrp.h"

#ifndef G_BOARDOBJGRP_H
#define G_BOARDOBJGRP_H

/*!
 * @file    boardobjgrp.h
 *
 * @brief   Provides PMU-specific definitions for BOARDOBJGRP infrastructure.
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
typedef struct BOARDOBJGRP                  BOARDOBJGRP;
typedef struct BOARDOBJGRP_VIRTUAL_TABLES   BOARDOBJGRP_VIRTUAL_TABLES;

/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobj.h"
#include "boardobj/boardobj_interface.h"
#include "boardobj/boardobj_virtual_table.h"
#include "boardobj/boardobjgrpmask.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   Helper macro to retrieve from group the object at the given index.
 *
 * @param[in]   _pBObjGrp   Board object group pointer.
 * @param[in]   _objIdx     Index of an object to retrieve.
 *
 * @return  pointer to BOARDOBJ object at the specified index in the group.
 */
#define BOARDOBJGRP_OBJ_GET_BY_GRP_PTR(_pBObjGrp, _objIdx)                     \
    ((_pBObjGrp)->ppObjects[(_objIdx)])

/*!
 * @brief   Retrieves a group pointer for requested object class.
 *
 * @pre     Each object class (BOARDOBJ child) is responsible to define macro
 *          BOARDOBJGRP_DATA_LOCATION_<class> defining location of the group.
 *
 * @param[in]   _class  Class name (<=> struct name) of requested object class.
 *
 * @return  Pointer to BOARDOBJGRP object at the base of the requested group
 *          singleton.
 */
#define BOARDOBJGRP_GRP_GET(_class)                                            \
    (BOARDOBJGRP_DATA_LOCATION_##_class)

/*!
 * @brief   Retrieves an object pointer of requested class and index, or NULL
 *
 * @pre     Each object class (BOARDOBJ child) is responsible to define macro
 *          BOARDOBJGRP_DATA_LOCATION_<class> defining location of the group.
 *
 * @param[in]   _class  Class name (<=> struct name) of requested object class.
 * @param[in]   _objIdx Index of an object to retrieve.
 *
 * @return  pointer to (_class *) casted BOARDOBJ object at the specified index
 *          in the group.
 */
#define BOARDOBJGRP_OBJ_GET(_class, _objIdx)                                            \
    BOARDOBJ_UNSAFE_CAST(boardObjGrpObjGet(BOARDOBJGRP_GRP_GET(_class), (_objIdx)),     \
                          DUMMY_UNIT, _class, BASE)

/*!
 * @brief   Retrieves an object pointer of requested class and index, or NULL
 *
 * @pre     Each object class (BOARDOBJ child) is responsible to define macro
 *          BOARDOBJGRP_DATA_LOCATION_<class> defining location of the group.
 *
 * @param[in]   _class  Class name (<=> struct name) of requested object class.
 * @param[in]   _objIdx Index of an object to retrieve.
 *
 * @return  Pointer to (_class##_##_type *) down-casted BOARDOBJ object at the
 *          specified index in the group.
 */
#define BOARDOBJGRP_OBJ_GET_DYNAMIC_CAST(_unit, _class, _type,  _objIdx)                \
    BOARDOBJ_DYNAMIC_CAST(boardObjGrpObjGet(BOARDOBJGRP_GRP_GET(_class), (_objIdx)),    \
                          _unit, _class, _type)

/*!
 * @brief   Checks if an object index points to a valid object entry.
 *
 * @pre     Each object class (BOARDOBJ child) is responsible to define macro
 *          BOARDOBJGRP_DATA_LOCATION_<class> defining location of the group.
 *
 * @param[in]   _class  Class name (<=> struct name) of requested object class.
 * @param[in]   _objIdx Index of an object to validate.
 *
 * @return LW_TRUE  if object index is valid in the group.
 * @return LW_FALSE if object index is not valid in the group.
 */
#define BOARDOBJGRP_IS_VALID(_class,_objIdx)                                   \
    (boardObjGrpObjGet(BOARDOBJGRP_GRP_GET(_class), (_objIdx)) != NULL)

/*!
 * @brief   Board Object Group iterating macro.
 *
 * Used to traverse all Board Objects stored within @ref _class group in
 * increasing index order.
 *
 * If @ref _pMask is provided, only objects specified by the mask are traversed.
 *
 * @param[in]       _class  Class name (& name of base class) identifying target
 *                          object type.
 * @param[out]      _pObj   (_class##_##_type *) type variable (LValue) to hold
 *                          pointer of next object.
 * @param[in,out]   _index  LwBoardObjIdx type variable (LValue) to hold the
 *                          index of next object.
 * @param[in]       _pMask  BOARDOBJGRPMASK pointer if filtering required (or
 *                          NULL).
 */
#define BOARDOBJGRP_ITERATOR_BEGIN(_class, _pObj, _index, _pMask)              \
    BOARDOBJGRP_ITERATOR_PTR_BEGIN(_class, BOARDOBJGRP_GRP_GET(_class), _pObj, \
                                   _index, _pMask)                             \
    {                                                                          \
        CHECK_SCOPE_BEGIN(BOARDOBJGRP_ITERATOR);
#define BOARDOBJGRP_ITERATOR_END                                               \
        CHECK_SCOPE_END(BOARDOBJGRP_ITERATOR);                                 \
    }                                                                          \
    BOARDOBJGRP_ITERATOR_PTR_END

/*!
 * @brief   Wrapper for @ref BOARDOBJ_UNSAFE_CAST() that matches the _OR_GOTO
 *          BOARDOBJ casting interface.
 *
 * @note    __PRITVATE, Not to be used outside of this file.
 *
 * @param[in,out]   _pDst       Pointer to be assigned with pointer of type
 *                              (_class##_##_type *) at the same address of
 *                              _pBoardobj.
 * @param[in]       _pBoardobj  Pointer to BOARDOBJ to be casted.
 * @param[in]       _unit       The unit _pBoardobj belongs to. Unused in this macro.
 * @param[in]       _class      The class _pBoardobj belongs to.
 * @param[in]       _type       The leaf type of the class that _pBoardobj is.
 * @param[in,out]   _pStatus    A FLCN_STATUS* variable to be filled in.
 *                              For this macro, it must be NULL.
 *                              BOARDOBJ_UNSAFE_CAST_NO_STATUS if not required.
 * @param[in]       _label      Unused. Voiding to ensure that this isn't
 *                              actually a label being passed in.
 */
#define BOARDOBJ_UNSAFE_CAST_OR_GOTO__PRIVATE(_pDst, _pBoardobj, _unit, _class, _type, _pStatus, _label)    \
    do {                                                                                                    \
        (_pDst) = BOARDOBJ_UNSAFE_CAST(_pBoardobj, _unit, _class, _type);                                   \
        ct_assert((_pStatus) == NULL);                                                                      \
        (void)(_label);                                                                                     \
    } while (LW_FALSE)

/*!
 * @brief   Board Object Group iterating macro.
 *
 * Used to traverse all Board Objects stored within @ref _class Group in
 * increasing index order.
 *
 * If @ref _pMask is provided, only objects specified by the mask are traversed.
 *
 * @note    This _PTR_ version of the iterating macro is useful in the case
 *          where there are multiple instances of a BOARDOBJGRP class.
 *
 * @param[in]       _class  Class name (& name of base class) identifying target
 *                          object type.
 * @param[in]       _pGrp   BOARDOBJGRP pointer over which to iterate.
 * @param[out]      _pObj   (_class##_##_type *) type variable (LValue) to hold
 *                          pointer of next object.
 * @param[in,out]   _index  LwBoardObjIdx type variable (LValue) to hold the
 *                          index of next object.
 * @param[in]       _pMask  BOARDOBJGRPMASK pointer if filtering required (or
 *                          NULL).
 */
#define BOARDOBJGRP_ITERATOR_PTR_BEGIN(_class, _pGrp, _pObj, _index, _pMask)            \
    do {                                                                                \
        CHECK_SCOPE_BEGIN(BOARDOBJGRP_ITERATOR_PTR);                                    \
        LwU32 _variableIsNotALabel;                                                     \
        BOARDOBJGRP_ITERATOR_PTR__PRIVATE_BEGIN(DUMMY_UNIT, _class, BASE,               \
                                                _pGrp, _pObj, _index, _pMask,           \
                                                NULL, _variableIsNotALabel,             \
                                                BOARDOBJ_UNSAFE_CAST_OR_GOTO__PRIVATE)  \
        {
#define BOARDOBJGRP_ITERATOR_PTR_END                                                    \
        }                                                                               \
        BOARDOBJGRP_ITERATOR_PTR__PRIVATE_END;                                          \
        CHECK_SCOPE_END(BOARDOBJGRP_ITERATOR_PTR);                                      \
    } while (LW_FALSE)

/*!
 * @brief   Board Object Group iterating macro. Does a type-safe dynamic cast
 *          from the underlying BOARODBJ to the specified type.
 *
 * Used to traverse all Board Objects stored within @ref _class Group in
 * increasing index order.
 *
 * If @ref _pMask is provided, only objects specified by the mask are traversed.
 *
 * @param[in]       _unit       Unit that _pObj belongs to.
 * @param[in]       _class      Class name (& name of base class) identifying
 *                              target object type.
 * @param[in]       _type       Derived type of the class.
 * @param[out]      _pObj       (_class##_##_type *) type variable (LValue) to
 *                              hold pointer of next object.
 * @param[in,out]   _index      LwBoardObjIdx type variable (LValue) to hold the
 *                              index of next object.
 * @param[in]       _pMask      BOARDOBJGRPMASK pointer if filtering required
 *                              (or NULL).
 * @param[in]       _pStatus    A FLCN_STATUS* variable to be populated in the
 *                              case of a failed cast.
 * @param[in]       _label      Label to jump to in the case of a failed cast.
 */
#define BOARDOBJGRP_ITERATOR_DYNAMIC_CAST_BEGIN(_unit, _class, _type, _pObj, _index, _pMask, _pStatus, _label)  \
    BOARDOBJGRP_ITERATOR_PTR_DYNAMIC_CAST_BEGIN(_unit, _class, _type, BOARDOBJGRP_GRP_GET(_class), _pObj,       \
                                                _index, _pMask, _pStatus, _label)                               \
    {                                                                                                           \
        CHECK_SCOPE_BEGIN(BOARDOBJGRP_ITERATOR_DYNAMIC_CAST);
#define BOARDOBJGRP_ITERATOR_DYNAMIC_CAST_END                                                                   \
        CHECK_SCOPE_END(BOARDOBJGRP_ITERATOR_DYNAMIC_CAST);                                                     \
    }                                                                                                           \
    BOARDOBJGRP_ITERATOR_PTR_DYNAMIC_CAST_END

/*!
 * @brief   Board Object Group iterating macro. Does a type-safe dynamic cast
 *          from the underlying BOARODBJ to the specified type.
 *
 * Used to traverse all Board Objects stored within @ref _class Group in
 * increasing index order.
 *
 * If @ref _pMask is provided, only objects specified by the mask are traversed.
 *
 * @note    This _PTR_ version of the iterating macro is useful in the case
 *          where there are multiple instances of a BOARDOBJGRP class.
 *
 * @param[in]       _unit       Unit that _pObj belongs to.
 * @param[in]       _class      Class name (& name of base class) identifying
 *                              target object type.
 * @param[in]       _type       Derived type of the class.
 * @param[in]       _pGrp       BOARDOBJGRP pointer over which to iterate.
 * @param[out]      _pObj       (_class##_##_type *) type variable (LValue) to
 *                              hold pointer of next object.
 * @param[in,out]   _index      LwBoardObjIdx type variable (LValue) to hold the
 *                              index of next object.
 * @param[in]       _pMask      BOARDOBJGRPMASK pointer if filtering required
 *                              (or NULL).
 * @param[in]       _pStatus    A FLCN_STATUS* variable to be populated in the
 *                              case of a failed cast.
 * @param[in]       _label      Label to jump to in the case of a failed cast.
 */
#define BOARDOBJGRP_ITERATOR_PTR_DYNAMIC_CAST_BEGIN(_unit, _class, _type, _pGrp, _pObj, _index, _pMask, _pStatus, _label)   \
    BOARDOBJGRP_ITERATOR_PTR__PRIVATE_BEGIN(_unit, _class, _type, _pGrp, _pObj, _index, _pMask, _pStatus, _label,           \
                                            BOARDOBJ_DYNAMIC_CAST_OR_GOTO)                                                  \
    {                                                                                                                       \
        CHECK_SCOPE_BEGIN(BOARDOBJGRP_ITERATOR_PTR_DYNAMIC_CAST);
#define BOARDOBJGRP_ITERATOR_PTR_DYNAMIC_CAST_END                                                                           \
        CHECK_SCOPE_END(BOARDOBJGRP_ITERATOR_PTR_DYNAMIC_CAST);                                                             \
    }                                                                                                                       \
    BOARDOBJGRP_ITERATOR_PTR__PRIVATE_END

/*!
 * @brief   A private, generic implementation of @ref BOARDOBJGRP_ITERATOR_PTR_BEGIN.
 *
 * @note    Not to be called as is, only to be called from macros in this file.
 *
 * @note    lwosVarNOP(_pObj) to toss out RISC-V tool-chain warnings.
 *
 * @param[in]       _unit               Unit that _pObj belongs to.
 * @param[in]       _class              Class name (& name of base class)
 *                                      identifying target object type.
 * @param[in]       _type               Derived type of the class.
 * @param[in]       _pGrp               BOARDOBJGRP pointer over which to
 *                                      iterate.
 * @param[out]      _pObj               (_class##_##_type *) type variable
 *                                      (LValue) to hold pointer of next object.
 * @param[in,out]   _index              LwBoardObjIdx type variable (LValue) to
 *                                      hold the index of next object.
 * @param[in]       _pMask              BOARDOBJGRPMASK pointer if filtering
 *                                      required (or NULL).
 * @param[in]       _pStatus            A FLCN_STATUS* variable to be populated
 *                                      in the case of a failed cast.
 * @param[in]       _label              Label to jump to in the case of a failed
 *                                      cast.
 * @param[in]       _castOrGotoMacro    A macro that implements a _CAST_OR_GOTO
 *                                      interface.
 */
#define BOARDOBJGRP_ITERATOR_PTR__PRIVATE_BEGIN(_unit, _class, _type, _pGrp, _pObj, _index, _pMask, _pStatus, _label, _castOrGotoMacro) \
    do {                                                                                                                                \
        CHECK_SCOPE_BEGIN(BOARDOBJGRP_ITERATOR_PTR__PRIVATE);                                                                           \
        BOARDOBJGRPMASK *_pMyMask  = (_pMask);                                                                                          \
        LwBoardObjIdx    _objSlots = (_pGrp)->objSlots;                                                                                 \
        ct_assert(sizeof(_index) == sizeof(LwBoardObjIdx));                                                                             \
        for ((_index) = 0; (_index) < _objSlots; (_index)++)                                                                            \
        {                                                                                                                               \
            BOARDOBJ *_pMyObj = BOARDOBJGRP_OBJ_GET_BY_GRP_PTR((_pGrp), _index);                                                        \
            if (_pMyObj == NULL)                                                                                                        \
            {                                                                                                                           \
                continue;                                                                                                               \
            }                                                                                                                           \
            if ((_pMyMask != NULL) &&                                                                                                   \
                !boardObjGrpMaskBitGet_SUPER(_pMyMask, _index))                                                                         \
            {                                                                                                                           \
                continue;                                                                                                               \
            }                                                                                                                           \
            _castOrGotoMacro(_pObj, _pMyObj, _unit, _class, _type, _pStatus, _label);                                                   \
            lwosVarNOP(_pObj);
#define BOARDOBJGRP_ITERATOR_PTR__PRIVATE_END                                                                                           \
        }                                                                                                                               \
        CHECK_SCOPE_END(BOARDOBJGRP_ITERATOR_PTR__PRIVATE);                                                                             \
    } while (LW_FALSE)

/*!
 * @brief   Checks if a BOARDOBJGRP is empty.
 *
 * @pre     Each object class (BOARDOBJ child) is responsible to define macro
 *          BOARDOBJGRP_DATA_LOCATION_<class> defining location of the group.
 *
 * @param[in]   _class  Class name (<=> struct name) of requested object class.
 *
 * @return LW_TRUE  if group has zero objects.
 * @return LW_FALSE if group has more than zero objects.
 */
#define BOARDOBJGRP_IS_EMPTY(_class)                                           \
    ((BOARDOBJGRP_GRP_GET(_class))->objSlots == 0U)

/*!
 * @brief   Get maximum index of the BOARDOBJGRP.
 *
 * @pre     Each object class (BOARDOBJ child) is responsible to define macro
 *          BOARDOBJGRP_DATA_LOCATION_<class> defining location of the group.
 *
 * @note    This macro abuses integer wrap around in the case @ref
 *          BOARDOBJGRP::objSlots is zero in order to return @ref
 *          LW2080_CTRL_BOARDOBJ_IDX_ILWALID.
 *
 * @param[in]   _class  Class name (<=> struct name) of requested object class.
 *
 * @return  Maximum index of the BOARDOBJGRP.
 * @return  @ref LW2080_CTRL_BOARDOBJ_IDX_ILWALID if the group is empty.
 */
#define BOARDOBJGRP_MAX_IDX_GET(_class)                                        \
    ((BOARDOBJGRP_GRP_GET(_class))->objSlots - 1U)

/*!
 * @brief   Colwenience macro for getting a pointer to a BOARDOBJGRP's
 *          BOARDOBJGRP_VIRTUAL_TABLES structure regardless of feature
 *          enablement (i.e. on any build profile).
 *
 * @param[in]   _pBObjGrp   Pointer to BOARDOBJGRP.
 *
 * @return  Pointer to BOARDOBJGRP's BOARDOBJGRP_VIRTUAL_TABLES structure.
 * @return  NULL if PMU_BOARDOBJGRP_VIRTUAL_TABLES is not enabled.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_VIRTUAL_TABLES)
#define BOARDOBJGRP_VIRTUAL_TABLES_GET(_pBObjGrp)                              \
    (&(_pBObjGrp)->vtables)
#else
#define BOARDOBJGRP_VIRTUAL_TABLES_GET(_pBObjGrp)                              \
    ((BOARDOBJGRP_VIRTUAL_TABLES *)NULL)
#endif

/*!
 * @brief   Helper macro to get an object's vtable from its BOARDOBJGRP.
 *
 * @param[in]   _pBObjGrp   Pointer to BOARDOBJGRP that _pBoardobj belongs to.
 * @param[in]   _pBoardobj  Pointer to BOARDOBJ.
 *
 * @return  Pointer to BOARDOBJ's BOARDOBJ_VIRTUAL_TABLE.
 * @return  NULL if feature PMU_BOARDOBJGRP_VIRTUAL_TABLES is not enabled
 */
#if PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_VIRTUAL_TABLES)
#define BOARDOBJGRP_BOARDOBJ_VIRTUAL_TABLE_GET(_pBObjGrp, _pBoardobj)          \
    (BOARDOBJ_GET_TYPE(_pBoardobj) <                                           \
     BOARDOBJGRP_VIRTUAL_TABLES_GET(_pBObjGrp)->numObjectVtables) ?            \
        BOARDOBJGRP_VIRTUAL_TABLES_GET(_pBObjGrp)->                            \
            ppObjectVtables[BOARDOBJ_GET_TYPE(_pBoardobj)] :                   \
        ((BOARDOBJ_VIRTUAL_TABLE *)NULL)
#else
#define BOARDOBJGRP_BOARDOBJ_VIRTUAL_TABLE_GET(_pBObjGrp, _pBoardobj)          \
    ((BOARDOBJ_VIRTUAL_TABLE *)NULL)
#endif

/*!
 * @brief   Helper macro to obfuscate use of BOARDOBOJGRP constructor vtables
 *          arguments that are best to be optimized out of the build if
 *          possible.
 *
 * @details When PMU_BOARDOBJGRP_VIRTUAL_TABLES feature is disabled, references
 *          to the passed in _ppObjectVtables symbol will ideally not exist when
 *          the microcode binary is linked. This will prevent existing profiles
 *          from bloating in DMEM even though no new functionality is added.
 *
 * @note    This macro is only to be used in boardobjgrp_e<xyz>.h as argument
 *          wrapper within the BOARDOBJGRP_CONSTRUCT_E<xyz> macros.
 *
 * @param[in]   _ppObjectVtables    Pointer to BOARDOBJGRP's object vtables.
 *
 * @return  Comma separated args representing the virtual table array address
 *          and its size that change depending on feature enablement.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_VIRTUAL_TABLES)
#define BOARDOBJGRP_CONSTRUCT_VIRTUAL_TABLES_ARGS(_ppObjectVtables)            \
    ((_ppObjectVtables) == BoardObjGrpVirtualTablesNotImplemented) ?           \
        NULL : (_ppObjectVtables),                                             \
        LW_ARRAY_ELEMENTS(_ppObjectVtables)
#else
#define BOARDOBJGRP_CONSTRUCT_VIRTUAL_TABLES_ARGS(_ppObjectVtables)  \
    NULL, 0
#endif

/* ------------------------ Datatypes --------------------------------------- */
/*!
 * @brief   Structure wrapping the details of vtables stored within a
 *          BOARDOBJGRP.
 */
struct BOARDOBJGRP_VIRTUAL_TABLES
{
    /*!
     * @brief   Number of object vtables pointers allocated for
     *          BOARDOBJGRP::ppObjectVtables. Expected to be of size
     *          LW2080_CTRL_BOARDOBJ_TYPE(UNIT, CLASS, MAX).
     *
     * @note    That is, access beyond ::ppObjectVtables[::numObjectVtables - 1]
     *          is undefined.
     */
    LwLength                    numObjectVtables;

    /*!
     * @brief   Array of virtual tables for each object type within the group's
     *          class.
     *
     * @note    Has a size of BOARDOBJGRP_VIRTUAL_TABLES::numObjectVtables and
     *          is indexed by LW2080_CTRL_BOARDOBJ_TYPE(UNIT, CLASS, TYPE).
     */
    BOARDOBJ_VIRTUAL_TABLE    **ppObjectVtables;
};

/*!
 * @brief BOARDOBJGRP virtual base class in PMU microcode.
 *
 * Allows storage of up to 255 (@ref ::objSlots) @ref BOARDOBJ object pointers
 * of given @ref ::classId.
 *
 * @note    There should be NO object instances of this type.
 */
struct BOARDOBJGRP
{
    /*!
     * @brief   Board Object Group type as @ref
     *          LW2080_CTRL_BOARDOBJGRP_TYPE_ENUM
     */
    LW2080_CTRL_BOARDOBJGRP_TYPE    type;

    /*!
     * @brief   Class ID as @ref LW2080_CTRL_BOARDOBJGRP_CLASS_ID_ENUM of
     *          objects stored in @ref BOARDOBJGRP::ppObjects.
     */
    LW2080_CTRL_BOARDOBJGRP_CLASS_ID    classId;

    /*!
     * @brief   Index of the DMEM overlay storing group's data. This overlay
     *          will be allocated from for any internal calls to lwosCallocType.
     *
     * @note    Not used in the case of overlays being disabled.
      */
    LwU8        ovlIdxDmem;

    /*!
     * @brief   flag to indicate whether the group has been constructed or not
     */
    LwBool      bConstructed;

    /*!
     * @brief   Number of @ref BOARDOBJ pointers allocated in @ref
     *          BOARDOBJGRP::ppObjects.
     */
    LwBoardObjIdx objSlots;

    /*!
     * @brief   Dynamically allocated array of @ref BOARDOBJ pointers with
     *          length of @ref BOARDOBJGRP::objSlots. Valid entries are
     *          identified by a non-NULL object pointer.
     */
    BOARDOBJ  **ppObjects;

#if PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_VIRTUAL_TABLES)
    /*!
     * @brief   Virtual tables for all object types within the group's class.
     *          See @ref BOARDOBJGRP_VIRTUAL_TABLES.
     */
    BOARDOBJGRP_VIRTUAL_TABLES  vtables;
#endif
};

/* ------------------------- Inline Functions  ----------------------------- */
/*!
 * Helper function to retrieve the corresponding MASK size for a given
 * BOARDOBJGRP.
 *
 * @param[in]  pBoardObjGrp  BOARDOBJGRP pointer
 * @param[out] pMaskSize     Pointer in which to return mask size.
 *
 * @return FLCN_OK
 *     MASK size returned correctly.
 * @return Unexpected errors
 *     Mask size could not be determined or is not supported for this
 *     PMU build.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
boardObjGrpMaskSizeGet
(
    BOARDOBJGRP   *pBoardObjGrp,
    LwBoardObjIdx *pMaskSize
)
{
    FLCN_STATUS status = FLCN_ERR_ILWALID_STATE;

    if (pMaskSize == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto boardObjGrpMaskSizeGet_exit;
    }

    switch (pBoardObjGrp->type)
    {
        case LW2080_CTRL_BOARDOBJGRP_TYPE_E32:
        {
            *pMaskSize = LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS;
            status = FLCN_OK;
            break;
        }
        case LW2080_CTRL_BOARDOBJGRP_TYPE_E255:
        {
            *pMaskSize = LW2080_CTRL_BOARDOBJGRP_E255_MAX_OBJECTS;
            status = FLCN_OK;
            break;
        }
        case LW2080_CTRL_BOARDOBJGRP_TYPE_E512:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJ_GRP_E512))
            {
                *pMaskSize = LW2080_CTRL_BOARDOBJGRP_E512_MAX_OBJECTS;
                status = FLCN_OK;
            }
            break;
        }
        case LW2080_CTRL_BOARDOBJGRP_TYPE_E1024:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJ_GRP_E1024))
            {
                *pMaskSize = LW2080_CTRL_BOARDOBJGRP_E1024_MAX_OBJECTS;
                status = FLCN_OK;
            }
            break;
        }
        case LW2080_CTRL_BOARDOBJGRP_TYPE_E2048:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJ_GRP_E2048))
            {
                *pMaskSize = LW2080_CTRL_BOARDOBJGRP_E2048_MAX_OBJECTS;
                status = FLCN_OK;
            }
            break;
        }
    }

boardObjGrpMaskSizeGet_exit:
    return status;
}

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
mockable BOARDOBJ * boardObjGrpObjGet(BOARDOBJGRP *pBoardObjGrp, LwBoardObjIdx objIdx)
    GCC_ATTRIB_SECTION("imem_resident", "boardObjGrpObjGet");

/* ------------------------ External Definitions --------------------------- */
extern BOARDOBJ_VIRTUAL_TABLE *BoardObjGrpVirtualTablesNotImplemented[0];

/* ------------------------ Include Derived Types --------------------------- */
#include "boardobj/boardobjgrp_self_init.h"
#include "boardobj/boardobjgrp_e32.h"
#include "boardobj/boardobjgrp_e255.h"
#include "boardobj/boardobjgrp_e512.h"
#include "boardobj/boardobjgrp_e1024.h"
#include "boardobj/boardobjgrp_e2048.h"

#endif // G_BOARDOBJGRP_H
#endif // BOARDOBJGRP_H
