/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BOARDOBJ_INTERFACE_H
#define BOARDOBJ_INTERFACE_H

/*!
 * @file    boardobj_interface.h
 *
 * @brief   Provides PMU-specific definitions for BOARDOBJ_INTERFACE
 *          infrastructure.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobj.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct INTERFACE_VIRTUAL_TABLE INTERFACE_VIRTUAL_TABLE;

typedef struct BOARDOBJ_INTERFACE BOARDOBJ_INTERFACE;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   Helper macro to get BOARDOBJ pointer from BOARDOBJ_INTERFACE
 *          pointer.
 *
 * @pre     pObj must have a BOARDOBJ_INTERFACE base class with zero offset
 *          within the object.
 *
 * @note    This macro does not guarantee type safety.
 *
 * @param[in]   pObj    Pointer to object extending BOARDOBJ_INTERFACE.
 *
 *
 * @return  pointer to BOARDOBJ on success
 * @return  NULL on error
 */
#define INTERFACE_TO_BOARDOBJ_CAST(_pInterface)                                \
    boardObjInterfaceGetBoardObjFromInterface((BOARDOBJ_INTERFACE *)(_pInterface))

/* ------------------------ Function Prototypes ----------------------------- */
/*!
 * @brief   Constructor prototype for BOARDOBJ_INTERFACE
 *
 * @param[in]   pBObjGrp        pointer to the BOARDOBJGRP of this interface.
 * @param[in]   pInterface      pointer to the BOARDOBJ_INTERFACE that needs to
 *                              be constructed.
 * @param[in]   pInterfaceDesc  pointer to this INTERFACE's description passed
 *                              from child class.
 * @param[in]   pVirtualTable   pointer to this BOARDOBJ_INTERFACE's virtual
 *                              table.
 *
 * @return FLCN_OK  This BOARDOBJ_INTERFACE is constructed successfully.
 * @return OTHERS   Implementation specific error oclwrs.
 */
#define BoardObjInterfaceConstruct(fname) FLCN_STATUS (fname)(BOARDOBJGRP *pBObjGrp, BOARDOBJ_INTERFACE *pInterface, RM_PMU_BOARDOBJ_INTERFACE *pInterfaceDesc, INTERFACE_VIRTUAL_TABLE *pVirtualTable)

/*!
 * @brief   Helper interface to get BOARDOBJ pointer from BOARDOBJ_INTERFACE
 *          pointer.
 *
 * @param[in]   pInterface  pointer to the BOARDOBJ_INTERFACE
 *
 * @return  pointer to BOARDOBJ on success
 * @return  NULL on error
 */
#define BoardObjInterfaceGetBoardObjFromInterface(fname) BOARDOBJ * (fname)(BOARDOBJ_INTERFACE *pInterface)

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief   Virtual VIRTUAL_TABLE structure in PMU microcode.
 *
 * @note    Instances of this object should be per class-type as a statically
 *          allocated structure.
 */
struct INTERFACE_VIRTUAL_TABLE
{
    /*!
     * @brief   Offset of the BOARDOBJ_INTERFACE node from its base BOARDOBJ
     *          pointer.
     *
     * This will be set to zero for all BOARDOBJ_INTERFACE nodes that follow the
     * linear class hierarchy and are direct children of BOARDOBJ.
     * This will be non-zero offset of the BOARDOBJ_INTERFACE node from the base
     * BOARDOBJ pointer for all BOARDOBJ_INTERFACEs that do not follow linear
     * class hierarchy.
     */
    LwS16   offset;
};

/*!
 * @brief   BOARDOBJ_INTERFACE virtual base class in PMU microcode.
 *
 * @note    There should be NO object instances of this type. Classes
 *          representing an interface, and used as members in classes derived
 *          from BOARDOBJ_VTABLE, should extend this class.
 */
struct BOARDOBJ_INTERFACE
{
    /*!
     * @brief   Pointer to a statically allocated INTERFACE_VIRTUAL_TABLE
     *          structure.
     */
    INTERFACE_VIRTUAL_TABLE  *pVirtualTable;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
BoardObjInterfaceConstruct                  (boardObjInterfaceConstruct);
BoardObjInterfaceGetBoardObjFromInterface   (boardObjInterfaceGetBoardObjFromInterface)
    GCC_ATTRIB_SECTION("imem_resident", "boardObjInterfaceGetBoardObjFromInterface");

/* ------------------------ Include Derived Types --------------------------- */

#endif // BOARDOBJ_INTERFACE_H
