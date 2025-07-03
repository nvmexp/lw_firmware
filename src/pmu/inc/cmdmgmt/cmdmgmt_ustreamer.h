/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CMDMGMT_USTREAMER_H
#define CMDMGMT_USTREAMER_H

/*!
 * @file cmdmgmt_ustreamer.h
 *
 * Provides the definitions and interfaces for all items related to the
 * uStreamer framework
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Includes --------------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */

/*!
 * @brief   Initialize uStreamer in PMU. Call this during PMU initialization.
 *
 * @return FLCN_OK                  Initialization completed successfully.
 * @return FLCN_ERR_NO_FREE_MEM     Failed to initialize due to malloc failure.
 */
FLCN_STATUS pmuUstreamerInitialize(void)
    GCC_ATTRIB_SECTION("imem_init", "pmuUstreamerInitialize");

#endif // CMDMGMT_USTREAMER_H
