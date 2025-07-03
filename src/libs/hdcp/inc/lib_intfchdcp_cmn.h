/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIB_INTFCHDCP_CMN_H
#define LIB_INTFCHDCP_CMN_H

/*!
 * @file lib_intfchdcp_cmn.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "lwuproc.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
#define ILWALID_OVERLAY_INDEX                       (0xFFFFFFFF)

typedef enum
{
    // HDCP1.x
    HDCP_AUTH = 0,
    HDCP_BIGINT,
    HDCP_SHA1,
    HDCP_BUS,
    
    // HDCP22
    HDCP22WIRED_REPEATER,
    HDCP22WIRED_TIMER,
    HDCP22WIRED_DPAUX,
    HDCP22WIRED_I2C,
    HDCP22WIRED_LIBI2CSW,
    HDCP22WIRED_CERTRX_SRM,
    HDCP22WIRED_RSA_SIGNATURE,
    HDCP22WIRED_DSA_SIGNATURE,
    HDCP22WIRED_AES,
    HDCP22WIRED_AKE,
    HDCP22WIRED_AKEH,
    HDCP22WIRED_LC,
    HDCP22WIRED_SKE,
    HDCP22WIRED_LIB_SE,
    HDCP22WIRED_LIB_SHA,
    HDCP22WIRED_OVL_SELWRE_ACTION,
    HDCP22WIRED_OVL_START_SESSION,
    HDCP22WIRED_OVL_VALIDATE_CERTRX_SRM,
    HDCP22WIRED_OVL_GENERATE_KMKD,
    HDCP22WIRED_OVL_HPRIME_VALIDATION,
    HDCP22WIRED_OVL_LPRIME_VALIDATION,
    HDCP22WIRED_OVL_GENERATE_SESSIONKEY,
    HDCP22WIRED_OVL_CONTROL_ENCRYPTION,
    HDCP22WIRED_OVL_VPRIME_VALIDATION,
    HDCP22WIRED_OVL_MPRIME_VALIDATION,
    HDCP22WIRED_OVL_END_SESSION,
    HDCP_OVERLAYID_MAX,
} HDCP_OVERLAY_ID;

#ifdef GSPLITE_RTOS
extern FLCN_STATUS dioReadReg(LwU32 address, LwU32* pData);
extern FLCN_STATUS dioWriteReg(LwU32 address, LwU32 data);

// Overlay load/unload is disabled in hdcp gsplite code
#define hdcpAttachAndLoadOverlay(overlayId)
#define hdcpDetachOverlay(overlayId)
#else
extern void libIntfcHdcpAttachAndLoadOverlay(HDCP_OVERLAY_ID overlayId);
extern void libIntfcHdcpDetachOverlay(HDCP_OVERLAY_ID overlayId);

#define hdcpAttachAndLoadOverlay(overlayId)         libIntfcHdcpAttachAndLoadOverlay(overlayId)
#define hdcpDetachOverlay(overlayId)                libIntfcHdcpDetachOverlay(overlayId)
#endif

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
#define hdcpDmaRead(a,b,c,d)                        dmaRead(a,b,c,d)
#define hdcpDmaWrite(a,b,c,d)                       dmaWrite(a,b,c,d)

/* ------------------------ Public Functions ------------------------------- */

#endif // LIB_INTFCHDCP_CMN_H
