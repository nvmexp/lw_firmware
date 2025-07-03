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
 * @brief   Interfaces for interacting with the PSTATES portions of the VBIOS
 */

#ifndef PSTATES_VBIOS
#define PSTATES_VBIOS

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp_src_vbios.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ VBIOS Definitions ------------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Compile-Time Asserts ---------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * Initializes a @ref BOARDOBJGRP_SRC_VBIOS structure to access the Performance
 * Table
 *
 * @param[out]   pSrc    Pointer to source structure to initialize
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  A pointer argument was NULL.
 * @return  Others                          Errors propagated from callees.
 */
FLCN_STATUS pstatesVbiosBoardObjGrpSrcInit(BOARDOBJGRP_SRC_VBIOS *pSrc)
    GCC_ATTRIB_SECTION("imem_libPstateBoardObjInit", "pstatesVbiosBoardObjGrpSrcInit");

/* ------------------------ Include Derived Types --------------------------- */
#include "perf/vbios/pstates_vbios_6x.h"

#endif // PSTATES_VBIOS
