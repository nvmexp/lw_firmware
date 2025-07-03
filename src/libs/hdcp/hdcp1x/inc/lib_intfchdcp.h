/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIB_INTFCHDCP_H
#define LIB_INTFCHDCP_H

/*!
 * @file lib_intfchdcp.h
 */

/* ------------------------ System Includes -------------------------------- */
#include "lwuproc.h"
#include "flcntypes.h"
#include <string.h>
/* ------------------------ Application Includes --------------------------- */
#include "rmflcncmdif.h"
#include "unit_dispatch.h"
#include "lib_hdcp.h"
#include "lib_intfchdcp_cmn.h"
/* ------------------------ Types Definitions ------------------------------ */
/*!
 * @brief Structure used to pass HDCP related events
 */
typedef union
{
    LwU8          eventType;
#if defined(GSPLITE_RTOS)
    DISP2UNIT_HDR   hdr;
#endif
    DISP2UNIT_CMD command;
} DISPATCH_HDCP;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
extern FLCN_STATUS libIntfcHdcpSecBusAccess(LwBool bIsReadReq, LwU32 addr, LwU32 dataIn, LwU32 *pDataOut);
#if UPROC_RISCV
extern FLCN_STATUS libIntfcHdcpSendResponse(RM_FLCN_QUEUE_HDR hdr, RM_FLCN_HDCP_MSG msg);
#endif

void hdcpProcessCmd(HDCP_CONTEXT *pContext, RM_FLCN_CMD *pCmd)
    GCC_ATTRIB_SECTION("imem_libHdcp", "hdcpProcessCmd");

#endif // LIB_INTFCHDCP_H
