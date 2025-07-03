/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_PROG_H
#define CLK_PROG_H

/*!
 * @file clk_prog.h
 * @brief @copydoc clk_prog.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct CLK_PROG CLK_PROG, CLK_PROG_BASE;

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Macro to locate CLK_PROGS BOARDOBJGRP.
 *
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_CLK_PROG                                    \
    (&Clk.progs.super.super)

/*!
 * Helper CLK PROG MACRO for @copydoc BOARDOBJ_TO_INTERFACE_CAST
 */
#define CLK_PROG_BOARDOBJ_TO_INTERFACE_CAST(_pProg, _type)                    \
    BOARDOBJ_TO_INTERFACE_CAST((_pProg), CLK, CLK_PROG, _type)

/*!
 * Helper CLK PROG MACRO for interface to interface colwersion.
 */
#define CLK_PROG_INTERFACE_TO_INTERFACE_CAST(_pProgIntSrc, _destIntType)      \
    CLK_PROG_BOARDOBJ_TO_INTERFACE_CAST(                                      \
        (INTERFACE_TO_BOARDOBJ_CAST(_pProgIntSrc)), _destIntType)

/*!
 * Accessor macro for CLK_PROGS::vfSecEntryCount
 *
 * @return @ref CLK_PROGS::vfSecEntryCount
 */
#define clkProgsVfSecEntryCountGet()                                          \
    (Clk.progs.vfSecEntryCount)

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Clock prog info entry.  Defines the how the RM will control a given clock
 * prog per the VBIOS specification.
 */
struct CLK_PROG
{
    /*!
     * BOARDOBJ_VTABLE super class. Must always be the first element in the structure.
     */
    BOARDOBJ_VTABLE super;
};

/*!
 * Clock prog info.  Specifies all clock progs that the RM will control per
 * the VBIOS specification.
 */
typedef struct
{
    /*!
     * BOARDOBJGRP_E255 super class.  Must always be the first element in the
     * structure.
     */
    BOARDOBJGRP_E255 super;

    /*!
     * Number of Secondary entries per _PRIMARY entry.
     */
    LwU8 secondaryEntryCount;
    /*!
     * Number of VF entries per each _PRIMARY entry.
     */
    LwU8 vfEntryCount;
    /*!
     * Number of secondary VF entries per each _PRIMARY entry.
     */
    LwU8 vfSecEntryCount;
} CLK_PROGS;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10CmdHandler  (clkProgBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkProgGrpSet");
BoardObjGrpIfaceModel10CmdHandler  (clkProgBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkProgBoardObjGrpIfaceModel10GetStatus");

// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkProgGrpIfaceModel10ObjSet_SUPER)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkProgGrpIfaceModel10ObjSet_SUPER");

/* ------------------------ Include Derived Types -------------------------- */
#include "clk/clk_prog_3x.h"

#endif // CLK_PROG_H
