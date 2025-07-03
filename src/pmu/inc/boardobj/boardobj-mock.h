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
 * @file    boardobj-mock.h
 * @brief   Mock interface of BOARDOBJ public interfaces.
 */

#ifndef BOARDOBJ_MOCK_H
#define BOARDOBJ_MOCK_H

/* ------------------------ Includes ---------------------------------------- */
#include "pmusw.h"

/* ------------------------ Data Types -------------------------------------- */
typedef FLCN_STATUS (boardObjConstruct_MOCK_CALLBACK)(BOARDOBJGRP *pBObjGrp, BOARDOBJ **ppBoardObj, LwLength size, RM_PMU_BOARDOBJ *pBoardObjDesc);

/* ------------------------ Public Functions -------------------------------- */
void boardObjConstructMockInit();
void boardObjConstructMockAddEntry(LwU8 entry, FLCN_STATUS status);
void boardObjConstruct_StubWithCallback(LwU8 entry, boardObjConstruct_MOCK_CALLBACK *pCallback);

void boardObjQueryMockInit();
void boardObjQueryMockAddEntry(LwU8 entry, FLCN_STATUS status);

void boardObjDynamicCastMockInit();
void boardObjDynamicCastMockAddEntry(LwU8 entry, void* pReturn);

#endif // BOARDOBJ_MOCK_H
