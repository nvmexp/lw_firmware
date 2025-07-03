/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef IFS_STATUS_H
#define IFS_STATUS_H


#ifdef __cplusplus
extern "C" {
#endif

/* When building as part of resman use resman status values
 * to avoid having to colwert back and forth between the two.
 */
#if defined(LWRM)
#include "lwstatus.h"

typedef LW_STATUS FS_STATUS;

#define FS_OK                                  LW_OK

#define FS_ERROR                               LW_ERR_GENERIC
#define FS_ERR_NO_FREE_MEM                     LW_ERR_NO_MEMORY
#define FS_ERR_ILWALID_ARG                     LW_ERR_ILWALID_ARGUMENT
#define FS_ERR_ILWALID_STATE                   LW_ERR_ILWALID_STATE
#define FS_ERR_FILE_NOT_FOUND                  LW_ERR_OBJECT_NOT_FOUND
#define FS_ERR_NOT_SUPPORTED                   LW_ERR_NOT_SUPPORTED
#define FS_ERR_ILWALID_DATA                    LW_ERR_ILWALID_DATA
#define FS_ERR_INSUFFICIENT_RESOURCES          LW_ERR_INSUFFICIENT_RESOURCES


#else /* ! LWRM */

#include "lwtypes.h"

typedef LwU32 FS_STATUS;
#define FS_OK                                  0x00000000

#define FS_ERROR                               0xFFFFFFFF
#define FS_ERR_NO_FREE_MEM                     0x00000001
#define FS_ERR_ILWALID_ARG                     0x00000002
#define FS_ERR_ILWALID_STATE                   0x00000003
#define FS_ERR_FILE_NOT_FOUND                  0x00000004
#define FS_ERR_NOT_SUPPORTED                   0x00000005
#define FS_ERR_ILWALID_DATA                    0x00000006
#define FS_ERR_INSUFFICIENT_RESOURCES          0x0000001A

#endif /* LWRM */

#ifdef __cplusplus
};
#endif

#endif /* IFS_STATUS_H */
