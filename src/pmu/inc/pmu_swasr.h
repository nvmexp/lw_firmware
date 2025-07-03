/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_SWASR_H
#define PMU_SWASR_H

/*!
 * @file pmu_swasr.h
 */

/* ------------------------ System includes -------------------------------- */
/* ------------------------ Application includes --------------------------- */
#include "config/g_ms_hal.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */

/*!
 * @brief Timeout values used SW-ASR entry/exit sequence.
 */
#define SWASR_FB_STOP_ENGAGE_TIMEOUT_NS         ( 10 * 1000)
#define SWASR_FB_STOP_RELEASE_TIMEOUT_NS        ( 10 * 1000)
#define SWASR_PRIV_FENCE_TIMEOUT_NS             ( 10 * 1000)

//
// AUTO Calibration should get triggered immediately but keeping a conservative
// timeout
//
#define SWASR_AUTO_CALIB_TRIGGER_TIMEOUT_NS     ( 10 * 1000)
//
// Worst case timeout for AUTO Calibration completion can be ~60-70uSec
// As per Mahipal), but in GM108 after profiling we found it to be 52uSec.
// The AUTO calibration cycle has been triggered early in the sequence & should
// never induce delay of more than 50uSec in MSCG exit as it has already 
// consumed around 10uSec when we check the AUTO_CALIBRATION_DONE. Also this 
// has been done only for GM108 which is displayless so it should not have any
// impact on MSCG watermarks.
//
#define SWASR_AUTO_CALIB_TIMEOUT_NS             ( 70 * 1000)

//
// We have to perform WCK and WR Training while exiting SW-ASR in P5 (In
// general when MCLK > 405MHz). In worst case, WCK training takes 40us and
// WR training takes 80us.
//
#define SWASR_FB_TRAINING_TIMEOUT_NS            (120 * 1000)

#endif // PMU_SWASR_H
