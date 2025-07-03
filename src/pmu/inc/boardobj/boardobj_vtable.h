/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BOARDOBJ_VTABLE_H
#define BOARDOBJ_VTABLE_H

/*!
 * @file    boardobj_vtable.h
 *
 * @brief   Provides PMU-specific definitions for BOARDOBJ_VTABLE
 *          infrastructure.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobj_interface.h"
#include "boardobj/boardobj_virtual_table.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct BOARDOBJ_VTABLE          BOARDOBJ_VTABLE;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   Helper macro to get INTERFACE pointer from BOARDOBJ pointer and cast
 *          it to input type.
 *
 * @note This macro does not guarantee type safety.
 *
 * @pre     _pBoardObj must have a BOARDOBJ_VTABLE base class with zero offset
 *          within the object.
 *
 * @param[in]   _pBoardObj  pointer to BOARDOBJ base class.
 * @param[in]   _obj        2080 class type.
 * @param[in]   _class      object base class type.
 * @param[in]   _type       object derived interface type.
 */
#define BOARDOBJ_TO_INTERFACE_CAST(_pBoardObj, _obj, _class, _type)                     \
    ((_class##_##_type *)                                                               \
        (((BOARDOBJ_VTABLE *)(_pBoardObj))->pVirtualTable->getInterfaceFromBoardObj(    \
            ((BOARDOBJ *)(_pBoardObj)),                                                 \
            (LW2080_CTRL_##_obj##_##_class##_INTERFACE_TYPE_##_type))))

/*!
 * @brief   Helper macro for calling BOARDOBJ_VIRTUAL_TABLE::dynamicCast even if
 *          the supporting feature is not enabled.
 *
 * @param[in,out]   _pObject        Address to be set by the casting function.
 * @param[in]       _pBoardObjGrp   The BOARDOBJGRP of the BOARDOBJ being casted.
 * @param[in]       _pBoardObj      The BOARDOBJ being dynamically casted.
 * @param[in]       _requestedType  The requested type.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJ_DYNAMIC_CAST)
#define BOARDOBJ_VIRTUAL_TABLE_DYNAMIC_CAST(_pObject, _pBoardObjGrp, _pBoardObj, _requestedType)    \
    do                                                                                              \
    {                                                                                               \
        BOARDOBJ_VIRTUAL_TABLE *pBoardObjVtable =                                                   \
            BOARDOBJGRP_BOARDOBJ_VIRTUAL_TABLE_GET(pBoardObjGrp, pBoardObj);                        \
        if ((pBoardObjVtable != NULL) && (pBoardObjVtable->dynamicCast != NULL))                    \
        {                                                                                           \
            _pObject = pBoardObjVtable->dynamicCast(_pBoardObj, _requestedType);                    \
        }                                                                                           \
        else                                                                                        \
        {                                                                                           \
            PMU_BREAKPOINT();                                                                       \
        }                                                                                           \
    } while (LW_FALSE)
#else
#define BOARDOBJ_VIRTUAL_TABLE_DYNAMIC_CAST(_pObject, _pBoardObjGrp, _pBoardObj, _requestedType)    \
    do                                                                                              \
    {                                                                                               \
        PMU_BREAKPOINT();                                                                           \
    } while (LW_FALSE)
#endif

/* ------------------------ Interface Assignment Macros --------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief   BOARDOBJ_VTABLE virtual base class in PMU microcode.
 *
 * This is the recommended base class for classes that will implement multiple
 * inheritance using BOARDOBJ_INTERFACE-derived members.
 *
 * @note    There should be NO object instances of this type. Object instances
 *          should be classes derived from BOARDOBJ_VTABLE.
 */
struct BOARDOBJ_VTABLE
{
    /*!
     * @brief BOARDOBJ super class.
     *
     * @note Must always be the first element in the structure.
     */
    BOARDOBJ                super;

    /*!
     * @brief Pointer to a statically allocated virtual table.
     */
    BOARDOBJ_VIRTUAL_TABLE *pVirtualTable;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
BoardObjGrpIfaceModel10ObjSet   (boardObjVtableGrpIfaceModel10ObjSet);

/* ------------------------ Include Derived Types --------------------------- */

#endif // BOARDOBJ_VTABLE_H
