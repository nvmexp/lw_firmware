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
#ifndef HDCPMC_SESSION_H
#define HDCPMC_SESSION_H

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * Specifies whether a feature specific to the HDCP session is to be turned off.
 * A feature should only be turned off for testing purposes.
 */
#define HDCP_SESSION_FEATURE_OFF                                              0

/*!
 * Specifies whether a feature specific to the HDCP session is to be turned on.
 * A feature should only be turned off for testing purposes.
 */
#define HDCP_SESSION_FEATURE_ON                                               1

/*!
 * Specifies whether or not to encrypt the session data when writing to the FB.
 * This is to get around the page-fault caused when encrypting the session data.
 * With Gobi's guidance, we are storing the session in the FB in the clear.
 * FOR TESTING PURPOSES ONLY!!!
 *
 * HDCP_SESSION_FEATURE_OFF - Session data will be stored in the cleared.
 * HDCP_SESSION_FEATURE_ON  - Session data will be stored encrypted
 */
#define HDCP_SESSION_FEATURE_ENCRYPT                    HDCP_SESSION_FEATURE_ON

/*!
 * Specifies whether or not a signature will be generated for the session data.
 * When using MODS on fmodel, the test will time out due while callwlating
 * the SHA-256 hash. FOR TESTING PURPOSES ONLY!!!
 *
 * HDCP_SESSION_FEATURE_OFF - Session signature will not be callwlated.
 * HDCP_SESSION_FEATURE_ON  - Session signature will be callwlated.
 */
#define HDCP_SESSION_FEATURE_SIGNATURE                  HDCP_SESSION_FEATURE_ON

#define HDCP_GET_SESSION_INDEX(sessId)                        ((sessId) & 0xff)

/* ------------------------- Prototypes ------------------------------------- */
LwBool hdcpSessionFindFree(LwU32 *pSessionIdx)
    GCC_ATTRIB_SECTION("imem_hdcpmc", "hdcpSessionFindFree");
FLCN_STATUS hdcpSessionSetStatus(LwU32 sessionId, LwBool bSet)
    GCC_ATTRIB_SECTION("imem_hdcpmc", "hdcpSessionSetStatus");
LwBool hdcpSessionIsActive(LwU32 sessionId, LwU32  *pActRecInd)
    GCC_ATTRIB_SECTION("imem_hdcpmc", "hdcpSessionIsActive");
FLCN_STATUS hdcpSessionDeactivate(HDCP_SESSION *pSession)
    GCC_ATTRIB_SECTION("imem_hdcpmc", "hdcpSessionDeactivate");
void hdcpSessionReset(HDCP_SESSION *pSession)
    GCC_ATTRIB_SECTION("imem_hdcpmc", "hdcpSessionReset");
FLCN_STATUS hdcpSessionReadById(HDCP_SESSION **pSession, LwU32 sessionId)
    GCC_ATTRIB_SECTION("imem_hdcpmc", "hdcpSessionReadById");
FLCN_STATUS hdcpSessionWrite(HDCP_SESSION *pSession)
    GCC_ATTRIB_SECTION("imem_hdcpmc", "hdcpSessionWrite");
FLCN_STATUS hdcpSessionCryptActive(LwU32 actResInd, LwBool bIsEncrypt)
    GCC_ATTRIB_SECTION("imem_hdcpmc", "hdcpSessionCryptActive");

#endif // HDCPMC_SESSION_H
#endif
