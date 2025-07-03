/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_pmutu10x-mock.h
 * @brief   Mock declaratiosn for pmutu10x
 */

#ifndef PMU_PMUTU10X_MOCK_H
#define PMU_PMUTU10X_MOCK_H

/* ------------------------- System Includes -------------------------------- */
#include "ut_port_types.h"
#include "fff.h"
#include "flcnifcmn.h"
#include "pmu_objpmu.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
/*!
 * @brief   Initializes mocking for pmutu10x
 */
void pmutu10xMockInit(void);

/* ------------------------- Mock Prototypes -------------------------------- */
/*!
 * @copydoc pmuDmemVaInit_TU10X
 *
 * @note    Mock implementation of @ref pmuDmemVaInit_TU10X
 *
 * @todo    Suffix this with "_MOCK" when HAL mocking solved
 */
DECLARE_FAKE_VOID_FUNC(pmuPreInitEnablePreciseExceptions_TU10X);

#endif // PMU_PMUTU10X_MOCK_H
