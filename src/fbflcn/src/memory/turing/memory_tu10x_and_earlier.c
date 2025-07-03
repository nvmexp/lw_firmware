/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <falcon-intrinsics.h>
#include "dev_pri_ringstation_fbp.h"
#include "dev_fbpa.h"
#include "dev_pri_ringstation_fbp.h"
#include "dev_pwr_pri.h"
#include "dev_disp.h"
#include "dev_fuse.h"

#include <falcon-intrinsics.h>

#include "memory.h"
#include "osptimer.h"
#include "config/g_memory_hal.h"

#include "priv.h"


void
memorySetTrainingControlPrbsMode_TU10X
(
    LwU8 prbsMode
)
{
    LwU32 fbpaTrainingPatram;
    fbpaTrainingPatram= REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_PATRAM);
    fbpaTrainingPatram = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_PATRAM, _PRBS_MODE,     prbsMode,       fbpaTrainingPatram);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_PATRAM,  fbpaTrainingPatram);
}

void
memorySetTrainingControlGddr5Commands_TU10X
(
    LwU8 gddr5Command
)
{
    LwU32 fbpaTrainingCtrl;
    fbpaTrainingCtrl= REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CTRL);
    fbpaTrainingCtrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_CTRL, _GDDR5_COMMANDS,     gddr5Command,       fbpaTrainingCtrl);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CTRL,  fbpaTrainingCtrl);
}


