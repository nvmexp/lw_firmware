/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    hdcpmc_data.c
 * @brief   Global HDCP task data.
 */

#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HDCPMC))
/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "hdcpmc/hdcpmc_data.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * Maintains the current states required by the HDCP task.
 */
HDCP_GLOBAL_DATA hdcpGlobalData
    GCC_ATTRIB_ALIGN(256)
    GCC_ATTRIB_SECTION("dmem_hdcpmc", "hdcpGlobalData");

/*!
 * All session details are stroed in the FB to save space in DMEM. Whenever a
 * method targets a particular session, the method handler loads the particular
 * session from the FB into this buffer.
 */
LwU8 hdcpSessionBuffer[HDCP_SB_SESSION_SIZE]
    GCC_ATTRIB_ALIGN(256)
    GCC_ATTRIB_SECTION("dmem_hdcpmc", "hdcpSessionBuffer");

/*!
 * Buffer used by the VERIFY_CERT_RX method to store KpubDcp.
 *
 * This buffer is stored in the DMEM overlay HDCP_MTHD_VERIFY_CERT_RX and needs
 * to be explicitly loaded before use.
 */
LwU8 hdcpKpubDcp[ALIGN_UP((HDCP_SIZE_DCP_KPUB + 4), HDCP_DMA_ALIGNMENT)]
    GCC_ATTRIB_ALIGN(256)
    GCC_ATTRIB_SECTION("dmem_hdcpmc", "hdcpKpubDcp");

/*!
 * This is a temporary buffer used by method handlers. Primarily used to pull
 * additional data from the FB to operate on. The buffer is shared to use by
 * VERIFYCERTRX and ENCRYPT method to reduce DMEM size.
 */
LwU8 hdcpTempBuffer[HDCP_SIZE_TEMP_BUFFER]
    GCC_ATTRIB_ALIGN(256)
    GCC_ATTRIB_SECTION("dmem_hdcpmc", "hdcpTempBuffer");

#if SEC2CFG_FEATURE_ENABLED(SEC2_HDCPMC_VERIF)
/*!
 * Test receivers' IDs.
 */
LwU8 hdcpTestRcvrIds[HDCP_TEST_RECEIVER_COUNT][HDCP_SIZE_RCVR_ID] =
{
    {0x74, 0x5b, 0xb8, 0xbd, 0x04},
    {0x8b, 0xa4, 0x47, 0x42, 0xfb},
};

/*!
 * rtx test values.
 */
LwU8 hdcpTestRtx[HDCP_TEST_RECEIVER_COUNT][HDCP_SIZE_RTX] =
{
    { 0x18, 0xfa, 0xe4, 0x20, 0x6a, 0xfb, 0x51, 0x49 },
    { 0xf9, 0xf1, 0x30, 0xa8, 0x2d, 0x5b, 0xe5, 0xc3 },
};

/*!
 * rn test values.
 */
LwU8 hdcpTestRn[HDCP_TEST_RECEIVER_COUNT][HDCP_SIZE_RN] =
{
    { 0x32, 0x75, 0x3e, 0xa8, 0x78, 0xa6, 0x38, 0x1c },
    { 0xa0, 0xfe, 0x9b, 0xb8, 0x20, 0x60, 0x58, 0xca },
};

/*!
 * km test values.
 */
LwU8 hdcpTestKm[HDCP_TEST_RECEIVER_COUNT][HDCP_SIZE_KM] =
{
    {
        0x68, 0xbc, 0xc5, 0x1b, 0xa9, 0xdb, 0x1b, 0xd0,
        0xfa, 0xf1, 0x5e, 0x9a, 0xd8, 0xa5, 0xaf, 0xb9
    },
    {
        0xca, 0x9f, 0x83, 0x95, 0x70, 0xd0, 0xd0, 0xf9,
        0xcf, 0xe4, 0xeb, 0x54, 0x7e, 0x09, 0xfa, 0x3b
    },
};

/*!
 * ks test values.
 */
LwU8 hdcpTestKs[HDCP_TEST_RECEIVER_COUNT][HDCP_SIZE_KS] =
{
    {
        0xf3, 0xdf, 0x1d, 0xd9, 0x57, 0x96, 0x12, 0x3f,
        0x98, 0x97, 0x89, 0xb4, 0x21, 0xe1, 0x2d, 0xe1
    },
    {
        0xf3, 0xdf, 0x1d, 0xd9, 0x57, 0x96, 0x12, 0x3f,
        0x98, 0x97, 0x89, 0xb4, 0x21, 0xe1, 0x2d, 0xe1
    },
};

/*!
 * riv test values.
 */
LwU8 hdcpTestRiv[HDCP_TEST_RECEIVER_COUNT][HDCP_SIZE_RIV] =
{
    { 0x40, 0x2b, 0x6b, 0x43, 0xc5, 0xe8, 0x86, 0xd8 },
    { 0x9a, 0x6d, 0x11, 0x00, 0xa9, 0xb7, 0x6f, 0x64 },
};
#endif // SEC2CFG_FEATURE_ENABLED(SEC2_HDCPMC_VERIF)
#endif
