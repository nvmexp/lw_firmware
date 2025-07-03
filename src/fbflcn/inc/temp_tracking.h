/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef TEMP_TRACKING_H
#define TEMP_TRACKING_H

#include "fbflcn_defines.h"

// FIXME: as this is a mode enumerator it might be better to turn it into an enum
/*
#define FBFLCN_TEMP_MODE_HIGH_SETTINGS 2
#define FBFLCN_TEMP_MODE_LOW_SETTINGS 1
#define FBFLCN_TEMP_MODE_AT_BOOT  FBFLCN_TEMP_MODE_LOW_SETTINGS
*/
typedef enum FBFLCN_TEMP_MODE_ENUM {
    FBFLCN_TEMP_MODE_LOW_SETTINGS = 1,
    FBFLCN_TEMP_MODE_HIGH_SETTINGS,
    FBFLCN_TEMP_MODE_AT_BOOT = FBFLCN_TEMP_MODE_LOW_SETTINGS
} FBFLCN_TEMP_MODE_TYPE;

#define FBFLCN_TEMP_RECORD_STORAGE 10
#define SFXP16_FRACTIONAL_BITS 5
#define STARTUP_ILWALID_MS  300
#define FBFLCN_TEMP_DRAM_MASK 0xFF

extern LwU8 gTempTrackingMode;

LwU32 initTempTracking(void);
void temperature_read_event(void);
LwS16 read_temperature(void);
LwU32 startTempTracking(void);
LwU32 stopTempTracking(void);
void getTimingValuesForRefi(LwU32 *config4, LwU32 *timing21);

#endif // TEMP_TRACKING_H
