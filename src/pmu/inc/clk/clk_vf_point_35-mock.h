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
 * @file clk_vf_point_35-mock.h
 * @brief Mock interface for clk_vf_point_35 public interface.
 */

#ifndef CLK_VF_POINT_35_MOCK_H
#define CLK_VF_POINT_35_MOCK_H

#include "pmusw.h"
#include "boardobj/boardobjgrp.h"
#include "clk/clk_vf_point.h"

/* ------------------------ Data Types -------------------------------------- */
typedef FLCN_STATUS (clkVfPointConstruct_35_MOCK_CALLBACK)(BOARDOBJGRP *pBObjGrp, BOARDOBJ **ppBoardObj, LwLength size, RM_PMU_BOARDOBJ *BoardObjDesc);
typedef FLCN_STATUS (clkVfPointGetStatus_35_MOCK_CALLBACK)(BOARDOBJ *pBoardObj, RM_PMU_BOARDOBJ *pPayload);
typedef FLCN_STATUS (clkVfPointVoltageuVGet_35_MOCK_CALLBACK)(CLK_VF_POINT *pVfPoint, LwU8 voltageType, LwU32 *pVoltageuV);

/* ------------------------ Public Functions -------------------------------- */
void clkVfPointConstruct_35MockInit();
void clkVfPointConstruct_35MockAddEntry(LwU8 entry, FLCN_STATUS status, BOARDOBJ *pBoardObj);
void clkVfPointConstruct_35_StubWithCallback(LwU8 entry, clkVfPointConstruct_35_MOCK_CALLBACK *pCallback);

void clkVfPointGetStatus_35_MockInit();
void clkVfPointGetStatus_35_MockAddEntry(LwU8 entry, FLCN_STATUS status);
void clkVfPointGetStatus_35_StubWithCallback(LwU8 entry, clkVfPointGetStatus_35_MOCK_CALLBACK *pCallback);

void clkVfPointVoltageuVGet_35_MockInit();
void clkVfPointVoltageuVGet_35_MockAddEntry(LwU8 entry, FLCN_STATUS status);
void clkVfPointVoltageuVGet_35_StubWithCallback(LwU8 entry, clkVfPointVoltageuVGet_35_MOCK_CALLBACK *pCallback);

void clkVfPoint35CacheMockInit();
void clkVfPoint35CacheMockAddEntry(LwU8 entry, FLCN_STATUS status);

void clkVfPointLoadMockInit();
void clkVfPointLoadMockAddEntry(LwU8 entry, FLCN_STATUS status);

#endif // CLK_VF_POINT_35_MOCK_H
