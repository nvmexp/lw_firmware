/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_UPSTREAM_H
#define PMU_UPSTREAM_H

#include "flcntypes.h"

#define HDCP_SOR_PANEL_TYPE_NONE          0x00
#define HDCP_SOR_PANEL_TYPE_INTERNAL      0x01
#define HDCP_SOR_PANEL_TYPE_EXTERNAL      0x02

/* Function Prototypes */
FLCN_STATUS hdcpReadSprime(LwU8 *, LwU8 *, LwU8 *);
FLCN_STATUS hdcpSprimeSanityCheck(LwU8 *, LwU8, LwU8);

#endif // PMU_UPSTREAM_H
