/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLIENT_CLK_VF_POINT_PROG_20_H
#define CLIENT_CLK_VF_POINT_PROG_20_H

/*!
 * @file client_clk_vf_point_prog_20.h
 * @brief @copydoc client_clk_vf_point_prog_20.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/client_clk_vf_point_prog.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Client Clock VF Point Prog 20 structure. 
 */
typedef struct
{
    /*!
     * CLIENT_CLK_VF_POINT_PROG super class.  Must always be the first element in the structure.
     */
    CLIENT_CLK_VF_POINT_PROG    super;
} CLIENT_CLK_VF_POINT_PROG_20;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */\
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clientClkVfPointGrpIfaceModel10ObjSet_PROG_20)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clientClkVfPointGrpIfaceModel10ObjSet_PROG_20");

BoardObjIfaceModel10GetStatus (clientClkVfPointIfaceModel10GetStatus_PROG_20)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clientClkVfPointIfaceModel10GetStatus_PROG_20");
/* ------------------------ Include Derived Types -------------------------- */
#include "clk/client_clk_vf_point_prog_20_freq.h"
#include "clk/client_clk_vf_point_prog_20_volt.h"

#endif // CLIENT_CLK_VF_POINT_PROG_20_H_
