/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef __ga102_dev_sec_addendum_h__
#define __ga102_dev_sec_addendum_h__

//
// The detail description about each of the mutex can be found in 
// <branch>/drivers/resman/arch/lwalloc/common/inc/lw_mutex.h
//
#define LW_SEC2_MUTEX_DEFINES                              \
    LW_MUTEX_ID_SEC2_EMEM_ACCESS,                                      \
    LW_MUTEX_ID_PLAYREADY,                                 \
    LW_MUTEX_ID_VPR_SCRATCH,                               \
    LW_MUTEX_ID_ACR_ALLOW_ONLY_ONE_ACR_BINARY_AT_ANY_TIME, \
    LW_MUTEX_ID_BSI_WRITE,                                 \
    LW_MUTEX_ID_WPR_SCRATCH,                               \
    LW_MUTEX_ID_COLD_BOOT_GC6_UDE_COMPLETION,              \
    LW_MUTEX_ID_MMU_VPR_WPR_WRITE,                         \
    LW_MUTEX_ID_DISP_SELWRE_SCRATCH,                       \
    LW_MUTEX_ID_ILWALID,                                   \
    LW_MUTEX_ID_ILWALID,                                   \
    LW_MUTEX_ID_ILWALID,                                   \
    LW_MUTEX_ID_ILWALID,                                   \
    LW_MUTEX_ID_ILWALID,                                   \
    LW_MUTEX_ID_ILWALID,                                   \
    LW_MUTEX_ID_ILWALID,

#define LW_SEC2_EMEM_ACCESS_PORT_RM                                (0)
#define LW_SEC2_EMEM_ACCESS_PORT_LWWATCH                           (1)
#define UNUSED_EMEM_ACCESS_PORT_2                                  (2)
#define UNUSED_EMEM_ACCESS_PORT_3                                  (3)

#endif // __ga102_dev_sec_addendum_h__
