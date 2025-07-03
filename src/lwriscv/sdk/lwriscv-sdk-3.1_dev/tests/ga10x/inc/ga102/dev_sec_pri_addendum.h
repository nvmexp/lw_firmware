
/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef __ga102_dev_sec_pri_addendum_h__
#define __ga102_dev_sec_pri_addendum_h__

// Defines the current state and error code of GPCCS bootstrapping
#define LW_PSEC_GPC_RG_GPCCS_BOOTSTRAP_INFO                        LW_PSEC_FALCON_COMMON_SCRATCH_GROUP_0(0)
#define LW_PSEC_GPC_RG_GPCCS_BOOTSTRAP_INFO_STATE                  3:0
#define LW_PSEC_GPC_RG_GPCCS_BOOTSTRAP_INFO_STATE_NOT_STARTED      0x0
#define LW_PSEC_GPC_RG_GPCCS_BOOTSTRAP_INFO_STATE_STARTED          0x1
#define LW_PSEC_GPC_RG_GPCCS_BOOTSTRAP_INFO_STATE_DONE             0x2
#define LW_PSEC_GPC_RG_GPCCS_BOOTSTRAP_INFO_ERR_CODE               11:4


#endif // __ga102_dev_sec_pri_addendum_h__

