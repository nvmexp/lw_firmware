/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJFB_H
#define PMU_OBJFB_H

#include "g_pmu_objfb.h"

#ifndef G_PMU_OBJFB_H
#define G_PMU_OBJFB_H

/*!
 * @file pmu_objfb.h
 */

/* ------------------------ System includes -------------------------------- */
#include "flcntypes.h"
#include "pmusw.h"
#include "pmu/pmuif_pg_gc6.h"

/* ------------------------ Forward Declarations -----------------------------*/
#define FB_FBPA_INDEX_ILWALID (0xFF)

/* ------------------------ Application includes --------------------------- */
#include "config/g_fb_hal.h"

/* ------------------------ Types definitions ------------------------------ */

//
// The Fermi manuals define broadcast registers at 0x0010Fxxx and corresponding
// per-partition registers at 0x0011pxxx where p is the partition number.
// Since the manuals don't have an indexed symbol for the per-partition regs,
// we have this macro to colwert a broadcast or per-partition address into
// the corresponding per-partition address.
//
#define FB_PER_FBPA(reg,part) \
            (((reg) & 0xfff00fff) | 0x00010000| ((part) << 12))

/* ------------------------ External definitions --------------------------- */
/*!
 * FB object Definition
 */
typedef struct
{
    LwU8    dummy;    // unused -- for compilation purposes only
} OBJFB;

extern OBJFB Fb;

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */

#endif // G_PMU_OBJFB_H
#endif // PMU_OBJFB_H
