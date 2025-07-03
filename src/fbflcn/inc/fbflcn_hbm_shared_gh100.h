/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "fbflcn_defines.h"

#ifndef FBFLCN_HBM_SHARED_GH100_H_
#define FBFLCN_HBM_SHARED_GH100_H_

// read broadcast_misc0/1 registers via other registers:
// LW_PFB_FBPA_FBIO_HBM_CHANNEL0_DWORD0_TX_DDLL
// LW_PFB_FBPA_FBIO_HBM_CHANNEL0_DWORD0_WL_DDLL
// LW_PFB_FBPA_FBIO_HBM_CHANNEL0_DWORD0_RX_DDLL
// LW_PFB_FBPA_FBIO_HBM_CFG2_CHANNEL0_DWORD0_BRICK
// LW_PFB_FBPA_FBIO_HBM_CFG1_CHANNEL0_DWORD0_PWRD2
//
void save_fbio_hbm_delay_broadcast_misc(LwU32* misc0, LwU32* misc1);

#endif /* FBFLCN_HBM_SHARED_GH100_H_ */
