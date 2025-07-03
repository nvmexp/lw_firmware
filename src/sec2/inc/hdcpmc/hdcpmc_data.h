/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HDCPMC))
#ifndef HDCPMC_DATA_H
#define HDCPMC_DATA_H

/*!
 * @file    hdcpmc_data.h
 * @brief   Export of all global variables used by the HDCP task.
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "hdcpmc/hdcpmc_constants.h"
#include "hdcpmc/hdcpmc_mem.h"
#include "hdcpmc/hdcpmc_types.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
extern HDCP_GLOBAL_DATA hdcpGlobalData;
extern LwU8 hdcpSessionBuffer[HDCP_SB_SESSION_SIZE];
extern LwU8 hdcpKpubDcp[ALIGN_UP((HDCP_SIZE_DCP_KPUB + 4), HDCP_DMA_ALIGNMENT)];
extern LwU8 hdcpTempBuffer[HDCP_SIZE_TEMP_BUFFER];

#if SEC2CFG_FEATURE_ENABLED(SEC2_HDCPMC_VERIF)
extern LwU8 hdcpTestRcvrIds[HDCP_TEST_RECEIVER_COUNT][HDCP_SIZE_RCVR_ID];
extern LwU8 hdcpTestRtx[HDCP_TEST_RECEIVER_COUNT][HDCP_SIZE_RTX];
extern LwU8 hdcpTestRn[HDCP_TEST_RECEIVER_COUNT][HDCP_SIZE_RN];
extern LwU8 hdcpTestKm[HDCP_TEST_RECEIVER_COUNT][HDCP_SIZE_KM];
extern LwU8 hdcpTestKs[HDCP_TEST_RECEIVER_COUNT][HDCP_SIZE_KS];
extern LwU8 hdcpTestRiv[HDCP_TEST_RECEIVER_COUNT][HDCP_SIZE_RIV];
#endif
/* ------------------------- Prototypes ------------------------------------- */

#endif // HDCPMC_DATA_H
#endif
