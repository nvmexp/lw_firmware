/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_vf_point_35_volt-mock.h
 * @brief Mock interface for clk_vf_point_35_volt public interface.
 */

#ifndef CLK_VF_POINT_35_VOLT_MOCK_H
#define CLK_VF_POINT_35_VOLT_MOCK_H

#include "pmusw.h"

void clkVfPointLoad_35_VOLTMockInit();
void clkVfPointLoad_35_VOLTMockAddEntry(LwU8 entry, FLCN_STATUS status);

void clkVfPointVoltageuVGet_35_VOLTMockInit();
void clkVfPointVoltageuVGet_35_VOLTMockAddEntry(LwU8 entry, FLCN_STATUS status, LwU32 voltageuV);

#endif // CLK_VF_POINT_35_VOLT_MOCK_H
