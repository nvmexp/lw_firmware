/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef FBFLCN_IEEE1500_GV100_H
#define FBFLCN_IEEE1500_GV100_H

#include "fbflcn_defines.h"

// FIXME: as this is a mode enumerator it might be better to turn it into an enum
#define FBFLCN_TEMP_MODE_HIGH_SETTINGS 2
#define FBFLCN_TEMP_MODE_LOW_SETTINGS 1

#define FBFLCN_TEMP_MODE_AT_BOOT FBFLCN_TEMP_MODE_LOW_SETTINGS
#define FBFLCN_TEMP_RECORD_STORAGE 10

extern LwU8 gTempTrackingMode;

LwU32 initTempTracking(void);
LwU32 temperature_read_event(void);
LwU32 fbflcn_ieee1500_gv100_read_temperature(void);
LwU32 startTempTracking(void);
LwU32 stopTempTracking(void);

#endif // FBFLCN_IEEE1500_GV100_H
