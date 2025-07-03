/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BOARDOBJ_VIRTUAL_TABLE_H
#define BOARDOBJ_VIRTUAL_TABLE_H

/*!
 * @file    boardobj_virtual_table.h
 *
 * @brief   Provides PMU-specific definitions for BOARDOBJ_VIRTUAL_TABLE
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobj_interface.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct BOARDOBJ_VIRTUAL_TABLE   BOARDOBJ_VIRTUAL_TABLE;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Interface Assignment Macros --------------------- */
/*!
 * @brief   Colwenience macro for setting
 *          BOARDOBJ_VIRTUAL_TABLE::getInterfaceFromBoardObj.
 *
 * @param[in]   func    The function to be assigned to the
 *                      getInterfaceFromBoardObj interface.
 *
 * @return  An assignment statement.
 */
#define BOARDOBJ_VIRTUAL_TABLE_ASSIGN_GET_INTERFACE_FROM_BOARDOBJ(func)        \
    .getInterfaceFromBoardObj = func,

/*!
 * @brief   Colwenience macro for setting BOARDOBJ_VIRTUAL_TABLE::dynamicCast
 *          regardless of the supporting profile.
 *
 * @param[in]   func    The function to be assigned to the dynamicCast
 *                      interface.
 *
 * @return  A assignment statement if the functionality is enabled
 * @return  Otherwise, emit nothing.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJ_DYNAMIC_CAST)
#define BOARDOBJ_VIRTUAL_TABLE_ASSIGN_DYNAMIC_CAST(func)                       \
    .dynamicCast = func,
#else
#define BOARDOBJ_VIRTUAL_TABLE_ASSIGN_DYNAMIC_CAST(func)
#endif

/* ------------------------ Function Prototypes ----------------------------- */
/*!
 * @brief   Helper interface to get BOARDOBJ_INTERFACE pointer from BOARDOBJ
 *          pointer.
 *
 * @param[in]   pBoardObj       pointer to the BOARDOBJ.
 * @param[in]   interfaceType   INTERFACE::type for expected interface.
 *
 * @return  pointer to BOARDOBJ_INTERFACE on success
 * @return  NULL on error
 */
#define BoardObjVirtualTableGetInterfaceFromBoardObj(fname) BOARDOBJ_INTERFACE * (fname)(BOARDOBJ *pBoardObj, LwU8 interfaceType)

/*!
 * @brief   Check to see if a given BOAROBJ can be safely casted to a desired
 *          type.
 *
 * @note    Calling of this interface should be protected with a feature check
 *          of PMU_BOARDOBJ_DYNAMIC_CAST.
 *
 * @param[in]       pBoardObj       Object to be casted.
 * @param[in]       requestedType   BoardObj type within the class's hierarchy.
 *
 * @return  Pointer to be casted if pBoardObj can be casted to the requested
 *          type of the given class.
 * @return  NULL if the object cannot be casted to the requested class+type.
 */
#define BoardObjVirtualTableDynamicCast(fname) void* (fname)(BOARDOBJ *pBoardObj, LwU8 requestedType)

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief   Virtual BOARDOBJ_VIRTUAL_TABLE structure in PMU microcode.
 *
 * @note    This is meant to be used as a per class static struct and
 *          referenced by either a BOARDOBJGRP or BOARDOBJ_VTABLE
 *          struct.
 */
struct BOARDOBJ_VIRTUAL_TABLE
{
    /*!
     * @brief   Virtual interface for an object's BOARDOBJ_INTERFACE lookup
     *          logic.
     */
    BoardObjVirtualTableGetInterfaceFromBoardObj    (*getInterfaceFromBoardObj);

#if PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJ_DYNAMIC_CAST)
    /*!
     * @brief   Virtual interface for an object's dynamic casting logic.
     */
    BoardObjVirtualTableDynamicCast                 (*dynamicCast);
#endif
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/* ------------------------ Include Derived Types --------------------------- */

#endif // BOARDOBJ_VIRTUAL_TABLE_H
