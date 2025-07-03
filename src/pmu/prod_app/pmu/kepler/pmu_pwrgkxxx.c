/*
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pwrgkxxx.c
 * @brief  PMU Power State (GCx) Hal Functions for GKXXX
 *
 * Functions implement chip-specific power state (GCx) init and sequences.
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "pmusw.h"
#include "pmu_objdisp.h"
#include "vbios/vbiosscratch.h"
#include "dev_bus.h"
#include "dev_timer.h"
#include "dev_graphics_nobundle.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "pmu_objpg.h"
#include "pmu_oslayer.h"
#include "pmu_gc6.h"
#include "cmdmgmt/cmdmgmt.h"
#include "g_pmurmifrpc.h"

#include "config/g_pmu_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

#define DMA_MIN_READ_ALIGN_MASK   (DMA_MIN_READ_ALIGNMENT - 1)

/* ------------------------- Global Variables ------------------------------- */
PMU_GC6 PmuGc6;

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/*!
 * @brief Write GC6 checkpoint to the VBIOS scratch register
 *
 * @param[in]   checkpoint    the checkpoint to update in the scratch register
 */
void
pmuUpdateCheckPoint_GMXXX
(
    PMU_RM_CHECKPOINTS checkpoint
)
{
    LwU32 val;

    switch (checkpoint)
    {
        case CHECKPOINT_BOOTSTRAP_COMPLETED:
            val = LW_VBIOS_PRIV_SCRATCH_04_GC6_STATUS_BOOTSTRAP_COMPLETE;
            break;
        case CHECKPOINT_DISP_INIT_COMPLETED:
            val = LW_VBIOS_PRIV_SCRATCH_04_GC6_STATUS_DISP_INIT_COMPLETE;
            break;
        case CHECKPOINT_MODESET_METHODS_COMPLETED:
            val = LW_VBIOS_PRIV_SCRATCH_04_GC6_STATUS_MODESET_METHODS_COMPLETE;
            break;
        case CHECKPOINT_MODESET_COMPLETED:
            val = LW_VBIOS_PRIV_SCRATCH_04_GC6_STATUS_MODESET_COMPLETE;
            break;
        default:
            return;
    }

    REG_WR32(BAR0, LW_PBUS_VBIOS_SCRATCH(4),
             FLD_SET_DRF_NUM(_VBIOS, _PRIV_SCRATCH_04, _GC6_STATUS, val,
                             REG_RD32(BAR0, LW_PBUS_VBIOS_SCRATCH(4))));
}

