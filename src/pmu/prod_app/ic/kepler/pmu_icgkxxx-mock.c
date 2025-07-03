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
 * @file   pmu_icgkxxx-mock.c
 * @brief  Mock implementations for icgkxxx
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "ic/pmu_icgkxxx-mock.h"
#include "regmacros.h"
#include <pmu_bar0.h>
#include "dev_pwr_csb.h"
#include "dev_graphics_nobundle.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
void
icgkxxxMockInit(void)
{
    // For icHaltIntrEnable_GMXXX
    REG_WR32(CSB, LW_CMSDEC_FALCON_IRQSCLR, 0U);
    REG_WR32(CSB, LW_CMSDEC_FALCON_IRQMSET, 0U);

    // For icEccIsEnabled_TUXXX
    REG_WR32(FECS, LW_PGRAPH_PRI_FECS_FEATURE_READOUT, 0U);

    // For icEccIntrEnable_TUXXX
    REG_WR32(CSB, LW_CPWR_FALCON_IRQDEST, 0U);
}

/* ------------------------- Mocked Functions ------------------------------- */
/* ------------------------- Static Functions ------------------------------- */
