/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef PARTRPC_H
#define PARTRPC_H
#include <lwtypes.h>
#include <lwstatus.h>
#include <lwmisc.h>

#define PARTITION_ID_RTOS           0
#define PARTITION_ID_WORKLAUNCH     1

#define WORKLAUNCH_METHOD_ARRAY_SIZE (0x40)
#define WORKLAUNCH_METHOD_ARRAY_IDX_ADDR (0x0)
#define WORKLAUNCH_METHOD_ARRAY_IDX_DATA (0x1)

// TODO-ssapre: enables code for staging/debugging
#define STAGING_TEMPORARY_CODE (1)

#ifdef STAGING_TEMPORARY_CODE
// unused method defines
#define LWCBA2_ENABLE_DBG_PRINT (0x00000424)
#define LWCBA2_ENABLE_CPR_DST (0x00000428)
#define LWCBA2_DISABLE_DECRYPT (0x0000042C)
#define LWCBA2_DISABLE_AUTH_TAG (0x00000430)
#endif

#define GCM_IV_SIZE_BYTES (12) // 96 bit IV

#define AUTH_TAG_BUF_SIZE_BYTES (16)
typedef struct
{
    LwU32  methods[WORKLAUNCH_METHOD_ARRAY_SIZE][2];
    LwU32  methodCount;
    LwU64  validMask;
    LwU32  iv[GCM_IV_SIZE_BYTES/4];
    LwBool bDbgPrint;
} SOE_CTX_WORKLAUNCH;

typedef struct
{
    SOE_CTX_WORKLAUNCH worklaunchParams;
} WL_REQUEST;

typedef struct
{
    LwU32 errorCode;
    LwU32 iv[GCM_IV_SIZE_BYTES/4];
} WL_REPLY;

WL_REQUEST *rpcGetRequest(void);
WL_REPLY *rpcGetReply(void);

#endif // PARTRPC_H
