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
 * @file   pmu_pmugp100-mock.c
 * @brief  Mock implementations for pmuGP100
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "pmu/pmu_pmugp100-mock.h"

#include "dev_pwr_csb.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
void
pmuGP100MockInit(void)
{
    pmuMockInit();
    REG_WR32(CSB, LW_CPWR_FBIF_CTL, 0U);
}

/* ------------------------- Mocked Functions ------------------------------- */
/* ------------------------- Static Functions ------------------------------- */
