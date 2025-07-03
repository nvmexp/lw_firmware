/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_objlsf-mock.h
 * @brief   Data required for configuring mock pmu_objlsf interfaces.
 */

#ifndef PMU_OBJLSF_MOCK_H
#define PMU_OBJLSF_MOCK_H

/* ------------------------- System Includes -------------------------------- */
#include "ut_port_types.h"
#include "fff.h"
#include "flcnifcmn.h"
#include "pmu_objlsf.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
/*!
 * @brief   Initializes mocking for clk
 */
void lsfMockInit(void);

/* ------------------------- Mock Prototypes -------------------------------- */
/*!
 * @copydoc constructLsf_MOCK
 *
 * @note    Mock implementation of @ref constructLsf_MOCK
 */
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, constructLsf_MOCK);

#endif // PMU_OBJLSF_MOCK_H
