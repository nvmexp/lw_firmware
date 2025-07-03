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
 * @file    perf_daemon-mock.h
 * @brief   Mock declaratiosn for perfDaemon
 */

#ifndef PERF_DAEMON_MOCK_H
#define PERF_DAEMON_MOCK_H

/* ------------------------- System Includes -------------------------------- */
#include "ut_port_types.h"
#include "fff.h"
#include "flcnifcmn.h"
#include "perf_daemon.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
/*!
 * @brief   Initializes mocking for perfDaemon
 */
void perfDaemonMockInit(void);

/* ------------------------- Mock Prototypes -------------------------------- */
/*!
 * @copydoc constructPerfDaemon
 *
 * @note    Mock implementation of @ref constructPerfDaemon
 */
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, constructPerfDaemon_MOCK);

#endif // PERF_DAEMON_MOCK_H
