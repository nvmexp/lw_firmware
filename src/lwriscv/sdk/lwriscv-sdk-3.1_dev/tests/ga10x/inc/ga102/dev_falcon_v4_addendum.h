/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef __ga102_dev_falcon_v4_addendum_h__
#define __ga102_dev_falcon_v4_addendum_h__

// #define LW_PFALCON_FALCON_RSTAT3_CTSXW_STATE 14:12
#define LW_PFALCON_FALCON_RSTAT3_CTSXW_STATE_SM_IDLE          0 // Idle 
#define LW_PFALCON_FALCON_RSTAT3_CTSXW_STATE_SM_CHECK         1 // Check what to do
#define LW_PFALCON_FALCON_RSTAT3_CTSXW_STATE_SM_SAVE          2 // Tell cpu to save current context
#define LW_PFALCON_FALCON_RSTAT3_CTSXW_STATE_SM_SAVE_WAIT1    3 // Wait for cpu to idle
#define LW_PFALCON_FALCON_RSTAT3_CTSXW_STATE_SM_BLOCK_BIND    4 // Issue block bind for new context
#define LW_PFALCON_FALCON_RSTAT3_CTSXW_STATE_SM_REST          5 // Tell cpu to restore new context
#define LW_PFALCON_FALCON_RSTAT3_CTSXW_STATE_SM_REST_WAIT1    6 // Wait for cpu to idle
#define LW_PFALCON_FALCON_RSTAT3_CTSXW_STATE_SM_ACK           7 // Ack to host, await host to drop req

#define LW_PFALCON_FALCON_IMTAG_BLK_SELWRE                    26:26
#define LW_PFALCON_FALCON_IMTAG_BLK_SELWRE_NO                 0x00000000
#define LW_PFALCON_FALCON_IMTAG_BLK_SELWRE_YES                0x00000001

#endif
