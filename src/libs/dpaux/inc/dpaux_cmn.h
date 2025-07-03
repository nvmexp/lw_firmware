/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  dpaux_cmn.h
 * @brief Master header file defining types that may be used in the dpaux
 *        application. This file defines all dpaux library related macros
 *        for different falcons
 */

#ifndef DPAUX_CMN_H
#define DPAUX_CMN_H
/* --------------------------- System includes ----------------------------- */
#include "flcncmn.h"
#include "osptimer.h"
#ifdef HDCP_TEGRA_BUILD
#include "dev_ardpaux.h"
#endif
/* ------------------------ Application includes --------------------------- */
#include "lib_intfcdpaux.h"
#include "ctrl/ctrl0073/ctrl0073dp.h"
#include "displayport.h"
#include "config/g_dpaux_hal.h"
/* ------------------------- Macros and Defines ---------------------------- */

#ifdef DPU_RTOS
#define LW_PMGR_DP_AUXCTL_SEMA_REQUEST_DISPFLCN     LW_PMGR_DP_AUXCTL_SEMA_REQUEST_DPU
#define LW_PMGR_DP_AUXCTL_SEMA_GRANT_DISPFLCN       LW_PMGR_DP_AUXCTL_SEMA_GRANT_DPU
#elif  defined(GSP_RTOS) || defined(GSPLITE_RTOS)
#ifdef HDCP_TEGRA_BUILD 
#define LW_PMGR_DP_AUXCTL_SEMA_REQUEST_DISPFLCN     LW_PDPAUX_DP_AUXCTL_SEMA_REQUEST_RM
#define LW_PMGR_DP_AUXCTL_SEMA_GRANT_DISPFLCN       LW_PDPAUX_DP_AUXCTL_SEMA_GRANT_RM
#else
#define LW_PMGR_DP_AUXCTL_SEMA_REQUEST_DISPFLCN     LW_PMGR_DP_AUXCTL_SEMA_REQUEST_GSP
#define LW_PMGR_DP_AUXCTL_SEMA_GRANT_DISPFLCN       LW_PMGR_DP_AUXCTL_SEMA_GRANT_GSP
#endif
#else
#error "Add corresponding defintion for other uproc."
#endif

#endif // DPAUX_CMN_H
