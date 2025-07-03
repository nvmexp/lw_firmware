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
 * @file pmu_clknafll-mock.h
 * @brief Mock interface for pmu_clknafll public interface.
 */

#ifndef PMU_CLKNAFLL_MOCK_H
#define PMU_CLKNAFLL_MOCK_H

#include "pmusw.h"
#include "clk/pmu_clknafll.h"

/* ------------------------ Data Types -------------------------------------- */
typedef FLCN_STATUS (clkNafllFreqMHzQuantize_MOCK_CALLBACK)(LwU32 clkDomain, LwU16 *pFreqMHz, LwBool bFloor);
typedef FLCN_STATUS (clkNafllGetFreqMHzByIndex_MOCK_CALLBACK)(LwU32 clkDomain, LwU16 idx, LwU16 *pFreqMHz);
typedef FLCN_STATUS (clkNafllGetIndexByFreqMHz_MOCK_CALLBACK)(LwU32 clkDomain, LwU16 freqMHz, LwBool bFloor, LwU16 *pIdx);

/* ------------------------ Public Functions -------------------------------- */
void clkNafllFreqMHzQuantizeMockInit();
void clkNafllFreqMHzQuantizeMockAddEntry(LwU8 entry, FLCN_STATUS status);
void clkNafllFreqMHzQuantize_StubWithCallback(LwU8 entry, clkNafllFreqMHzQuantize_MOCK_CALLBACK *pCallback);

void clkNafllGetFreqMHzByIndexMockInit();
void clkNafllGetFreqMHzByIndexMockAddEntry(LwU8 entry, FLCN_STATUS status);
void clkNafllGetFreqMHzByIndex_StubWithCallback(LwU8 entry, clkNafllGetFreqMHzByIndex_MOCK_CALLBACK *pCallback);

void clkNafllGetIndexByFreqMHzMockInit();
void clkNafllGetIndexByFreqMHzMockAddEntry(LwU8 entry, FLCN_STATUS status);
void clkNafllGetIndexByFreqMHz_StubWithCallback(LwU8 entry, clkNafllGetIndexByFreqMHz_MOCK_CALLBACK *pCallback);

#endif // PMU_CLKNAFLL_MOCK_H
