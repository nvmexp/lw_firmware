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
 * @file  hdcp22wired_cmn.h
 * @brief Master header file defining types that may be used in the hdcp22wired
 *        application. This file defines all dpaux library related macros
 *        for different falcons
 */

#ifndef HDCP22WIRED_CMN_H
#define HDCP22WIRED_CMN_H
/* ------------------------ System includes --------------------------------- */
#include "flcncmn.h"

/* ------------------------ Application includes ---------------------------- */
#include "lib_intfchdcp22wired.h"
#include "displayport.h"
#include "hdcp22wired_selwreaction.h"
#include "hdcp22wired_objhdcp22wired.h"

/* ------------------------ Macros and Defines ------------------------------ */
#if defined(DPU_RTOS) || defined(GSPLITE_RTOS)
#include "rmdpucmdif.h"
#define RM_FLCN_UNIT_HDCP22WIRED   RM_DPU_UNIT_HDCP22WIRED
#elif defined(SEC2_RTOS)
#include "rmsec2cmdif.h"
#define RM_FLCN_UNIT_HDCP22WIRED   RM_SEC2_UNIT_HDCP22WIRED
#endif

#define HDCP22_STATUS_SOR_MAX      8
//Compile time Debug Flag To be removed once DP Errata v3 Compliance passes.
#define HDCP22_DP_ERRATA_V3        1

#endif // HDCP22WIRED_CMN_H
