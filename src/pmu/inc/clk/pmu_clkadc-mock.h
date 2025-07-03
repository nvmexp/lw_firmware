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
 * @file    pmu_clkadc-mock.h
 * @brief   Data required for configuring mock pmu_clkadc interfaces.
 */

#ifndef PMU_CLKADC_MOCK_H
#define PMU_CLKADC_MOCK_H

/* ------------------------ Includes ---------------------------------------- */
#include "pmusw.h"

/* ------------------------ Function Prototypes ----------------------------- */
void clksAdcProgramMockInit(void);
void clksAdcProgramMockAddEntry(LwU8 entry, FLCN_STATUS status);

#endif // PMU_CLKADC_MOCK_H
