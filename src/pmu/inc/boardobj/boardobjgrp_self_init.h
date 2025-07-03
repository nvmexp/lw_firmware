/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BOARDOBJGRP_SELF_INIT_H
#define BOARDOBJGRP_SELF_INIT_H

/*!
 * @file    boardobjgrp_self_init.h
 *
 * @brief   Provides BOARDOBJGRP infrastructure for self-initialization of
 *          BOARDOBJGRPs
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_src.h"

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   Initializes a @ref BOARDOBJGRP_SRC_UNION to be used for parsing
 *
 * @param[out]  pSrc    Pointer to source structure to initialize
 *
 * @return @ref FLCN_OK Success
 * @return Others       Implementation specific error code.
 */
#define BoardObjGrpBoardObjGrpSrcInit(fname) FLCN_STATUS (fname)(BOARDOBJGRP_SRC_UNION *pSrc)

/* ------------------------ Datatypes --------------------------------------- */
/* ------------------------ Inline Functions--------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/* ------------------------ External Definitions ---------------------------- */
/* ------------------------ Include Derived Types --------------------------- */

#endif // BOARDOBJGRP_SELF_INIT_H
