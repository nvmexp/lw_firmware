/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef HDCP22WIRED_SRM_H
#define HDCP22WIRED_SRM_H

// TODO: Change the file name to hdcp22wired_certrx_srm.h

/* ------------------------ Application includes ---------------------------- */
#include "hdcp22wired_cmn.h"

/* ------------------------ Type Definitions -------------------------------- */
// SRM header 1st WORD
#define LW_HDCP22_SRM_HEADER_BITS_ID                    31:28
#define LW_HDCP22_SRM_HEADER_BITS_HDCP2_IND             27:24
#define LW_HDCP22_SRM_HEADER_BITS_RESERVED              23:16
#define LW_HDCP22_SRM_HEADER_BITS_VERSION               15:0
#define LW_HDCP22_SRM_HEADER_BITS_HDCP2_IND_TRUE        0x1
#define LW_HDCP22_SRM_HEADER_BITS_HDCP2_IND_FALSE       0x0

// SRM header 2nd WORD
#define LW_HDCP22_SRM_1X_BITS_GENERATION                31:24
#define LW_HDCP22_SRM_1X_BITS_LENGTH                    23:0

// SRM version 1.
#define LW_HDCP22_SRM_1X_NEXT_GEN_LENGTH                16:0
#define LW_HDCP22_SRM_1X_BITS_NO_OF_DEVICES             7:0

// SRM minimum lengths
#define HDCP22_SRM_1X_FIRST_GEN_MIN_LEN                 0x2B
#define HDCP22_SRM_1X_NEXT_GEN_MIN_LEN                  0x2A
#define HDCP22_SRM_2X_FIRST_GEN_MIN_LEN                 0x187
#define HDCP22_SRM_2X_NEXT_GEN_MIN_LEN                  0x184

// SRM version 2, next Gen header fields for 1st word
#define LW_HDCP22_SRM_2X_NEXT_GEN_LENGTH                31:16
#define LW_HDCP22_SRM_2X_NEXT_GEN_RESERVED              15:10
#define LW_HDCP22_SRM_2X_NEXT_GEN_NO_OF_DEVICES         9:0

#define HDCP22_SRM_SIGNATURE_SIZE                       384
#define HDCP22_SRM_DSA_SIGNATURE_SIZE                   40
#define HDCP22_DMA_BUFFER_SIZE                          64

/* ------------------------ External Definitions ---------------------------- */
FLCN_STATUS hdcp22wiredSrmRead(PRM_FLCN_MEM_DESC pSrmDmaDesc, LwU8 *pSrmBuf, LwU32 *pTotalSrmSize)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredCertrxSrm", "hdcp22wiredSrmRead");
FLCN_STATUS hdcp22RevocationCheck(LwU8 *pKsvList, LwU32 noOfKsvs, PRM_FLCN_MEM_DESC pSrmDmaDesc)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredCertrxSrm", "hdcp22RevocationCheck");
FLCN_STATUS hdcp22HandleVerifyCertRx (HDCP22_SESSION *pSession, HDCP22_CERTIFICATE *pCertRx)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredCertrxSrm", "hdcp22HandleVerifyCertRx");
FLCN_STATUS hdcp22wiredSrmRevocationHandler(SELWRE_ACTION_ARG *pArg)
    GCC_ATTRIB_SECTION("imem_hdcp22SrmRevocation", "hdcp22wiredSrmRevocationHandler");

#endif //HDCP22WIRED_SRM_H
