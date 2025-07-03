/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    boardobjgrp-mock.h
 * @brief   Mock interface of BOARDOBJGRP public interfaces.
 */

#ifndef BOARDOBJGRP_MOCK_H
#define BOARDOBJGRP_MOCK_H

/* ------------------------ Includes ---------------------------------------- */
#include "pmusw.h"

/* ------------------------ Data Types -------------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
void boardObjGrpObjGetMockInit();
void boardObjGrpObjGetMockAddEntry(LwU8 entry, BOARDOBJ *pReturn);

void boardObjGrpCmdHandlerDispatchInit();
void boardObjGrpCmdHandlerDispatchAddEntry(LwU8 entry, FLCN_STATUS status);

#endif // BOARDOBJGRP_MOCK_H
