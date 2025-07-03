/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pstates_self_init.h
 * @brief   Declarations, type definitions, and macros for the self-init
 *          portions of the PSTATES SW module.
 */

#ifndef PSTATE_SELF_INIT_H
#define PSTATE_SELF_INIT_H

/* ------------------------ Includes --------------------------------------- */
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_self_init.h"
#include "boardobj/boardobjgrp_src.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Forward Definitions ---------------------------- */
/* ------------------------ Datatypes -------------------------------------- */
/* ------------------------ Interface Definitions -------------------------- */
/* ------------------------ BOARDOBJGRP Interfaces ------------------------- */
BoardObjGrpBoardObjGrpSrcInit   (pstatesBoardObjGrpSrcInit)
    GCC_ATTRIB_SECTION("imem_libPstateBoardObjInit", "pstatesBoardObjGrpSrcInit");

/* ------------------------ BOARDOBJ Interfaces ---------------------------- */
/* ------------------------ PSTATE Interfaces ------------------------------ */
/* ------------------------ Include Derived Types -------------------------- */
/* ------------------------ End of File ------------------------------------ */

#endif // PSTATE_SELF_INIT_H
