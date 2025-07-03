/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_CLK_PLL_INFO_H
#define PMU_CLK_PLL_INFO_H

/*!
 * @file pmu_clkpll.h
 */
/* ------------------------ System Includes -------------------------------- */
#include "main.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PLL_DEVICE PLL_DEVICE, PLL_DEVICE_BASE;

/* ------------------------ Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Macro to locate CLK_FREQ_CONTROLLERS BOARDOBJGRP.
 *
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_PLL_DEVICE                                   \
    (&Clk.pllDevices.super.super)

/*!
 * Structure describing state of a PLL
 */
struct PLL_DEVICE
{
    /*!
     * Board Object super class
     */
    BOARDOBJ super;
    /*!
     * PLL ID @refLW2080_CTRL_CLK_SOURCE_<xyz>
     */
    LwU8 id;
    /*!
     * Minimum Reference Clock Frequency in MHz for this PLL Device
     */
    LwU16 MinRef;
    /*!
     * Maximum Reference Clock Frequency in MHz for this PLL Device
     */
    LwU16 MaxRef;
    /*!
     * Minimum VCO Frequency in MHz for this PLL Device
     */
    LwU16 MilwCO;
    /*!
     * Maximum VCO Frequency in MHz for this PLL Device
     */
    LwU16 MaxVCO;
    /*!
     * Minimum Update Rate in MHz for this PLL Device
     */
    LwU16 MinUpdate;
    /*!
     * Maximum Update Rate in MHz for this PLL Device
     */
    LwU16 MaxUpdate;
    /*!
     * Minimum Reference Clock Divider(M) Coefficient for this PLL Device
     */
    LwU8 MinM;
    /*!
     * Maximum Reference Clock Divider(M) Coefficient for this PLL Device
     */
    LwU8 MaxM;
    /*!
     * Minimum VCO Feedback Divider(N) Coefficient for this PLL Device
     */
    LwU8 MinN;
    /*!
     * Maximum VCO Feedback Divider(N) Coefficient for this PLL Device
     */
    LwU8 MaxN;
    /*!
     * Minimum Linear Post Divider(PL) Coefficient for this PLL Device
     */
    LwU8 MinPl;
    /*!
     * Maximum Linear Post Divider(PL) Coefficient for this PLL Device
     */
    LwU8 MaxPl;
};

/*!
 * Structure describing PLL in GPU
 */
typedef struct
{
    /*!
     * BOARDOBJGEP_E32 super class. Must always be the first element in the
     * structure
     */
    BOARDOBJGRP_E32 super;
} PLL_DEVICES;

/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10CmdHandler  (clkPllDevBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkPllDevBoardObjGrpIfaceModel10Set");
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkPllDevGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkPllDevGrpIfaceModel10ObjSetImpl_SUPER");

#endif //PMU_CLK_PLL_INFO_H
