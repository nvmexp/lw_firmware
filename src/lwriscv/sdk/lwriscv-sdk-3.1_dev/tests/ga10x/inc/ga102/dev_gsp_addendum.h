/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef __ga102_dev_gsp_addendum_h__
#define __ga102_dev_gsp_addendum_h__

#define LW_PGSP_FBIF_REGIONCFG_T(i)                             (3+(4*(i))) : (0+(4*(i)))

#define LW_PGSP_SCRATCH(i)                                      (0x110814+(i)*4) /* RW-4A */
#define LW_PGSP_SCRATCH__SIZE_1                                 4               /*       */
#define LW_PGSP_SCRATCH_VAL                                     31:0            /* RWIVF */
#define LW_PGSP_SCRATCH_VAL_INIT                                0x00000000      /* RWI-V */

//
// HS overlays (non-lib) write this register before calling HS libs
// HS libs use this to validate that the caller is HS
//
#define LW_PGSP_SCRATCH0_HS_LIB_CALLER                                7:0
#define LW_PGSP_SCRATCH0_HS_LIB_CALLER_NOT_SETUP                      0x00000000
#define LW_PGSP_SCRATCH0_HS_LIB_CALLER_NOT_HS                         0x00000055
#define LW_PGSP_SCRATCH0_HS_LIB_CALLER_HS                             0x000000AA

//
// The detail description about each of the mutex can be found in
// <branch>/drivers/resman/arch/lwalloc/common/inc/lw_mutex.h
//
#define LW_GSP_MUTEX_DEFINES     \
    LW_MUTEX_ID_ILWALID,         \
    LW_MUTEX_ID_ILWALID,         \
    LW_MUTEX_ID_ILWALID,         \
    LW_MUTEX_ID_ILWALID,         \
    LW_MUTEX_ID_ILWALID,         \
    LW_MUTEX_ID_I2C,             \
    LW_MUTEX_ID_ILWALID,         \
    LW_MUTEX_ID_ILWALID,         \
    LW_MUTEX_ID_ILWALID,         \
    LW_MUTEX_ID_ILWALID,         \
    LW_MUTEX_ID_ILWALID,         \
    LW_MUTEX_ID_ILWALID,         \
    LW_MUTEX_ID_ILWALID,         \
    LW_MUTEX_ID_ILWALID,         \
    LW_MUTEX_ID_ILWALID,         \
    LW_MUTEX_ID_ILWALID,

#endif // __ga102_dev_gsp_addendum_h__
