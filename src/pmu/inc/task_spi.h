/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_spi.h
 * @brief  Common header for all SPI task files.
 *
 * @section References
 * @ref "../../../drivers/resman/arch/lwalloc/common/inc/rmpmucmdif.h"
 */

#ifndef _TASK_SPI_H
#define _TASK_SPI_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "unit_api.h"

/* ------------------------- Types Definitions ----------------------------- */
/*!
 * A union of all available commands to SPI.
 */
typedef union
{
    DISP2UNIT_HDR       hdr;
    DISP2UNIT_RM_RPC    rpc;
} DISPATCH_SPI;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */

#endif // _TASK_SPI_H
