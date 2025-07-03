/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clknafll_lut_reg_map_20.c
 * @brief  Mapping of NAFLL registers.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_trim.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "pmu_objperf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Function Prototypes --------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*
 * @brief Mapping between the NAFLL ID and the various LUT registers for that NAFLL
 */
const CLK_NAFLL_ADDRESS_MAP ClkNafllRegMap_HAL[]
    GCC_ATTRIB_SECTION("dmem_perf", "ClkNafllRegMap_HAL") =
{
    {
        LW2080_CTRL_CLK_NAFLL_ID_SYS,
        {
            LW_PTRIM_SYS_NAFLL_SYSLUT_WRITE_ADDR,
            LW_PTRIM_SYS_NAFLL_SYSLUT_WRITE_DATA,
            LW_PTRIM_SYS_NAFLL_SYSLUT_CFG,
            LW_PTRIM_SYS_NAFLL_SYSLUT_SW_FREQ_REQ,
            LW_PTRIM_SYS_NAFLL_SYSNAFLL_COEFF,
        }
    },
    {
        LW2080_CTRL_CLK_NAFLL_ID_XBAR,
        {
            LW_PTRIM_SYS_NAFLL_XBARLUT_WRITE_ADDR,
            LW_PTRIM_SYS_NAFLL_XBARLUT_WRITE_DATA,
            LW_PTRIM_SYS_NAFLL_XBARLUT_CFG,
            LW_PTRIM_SYS_NAFLL_XBARLUT_SW_FREQ_REQ,
            LW_PTRIM_SYS_NAFLL_XBARNAFLL_COEFF,
        }
    },
    {
        LW2080_CTRL_CLK_NAFLL_ID_GPC0,
        {
            LW_PTRIM_GPC_GPCLUT_WRITE_ADDR(0),
            LW_PTRIM_GPC_GPCLUT_WRITE_DATA(0),
            LW_PTRIM_GPC_GPCLUT_CFG(0),
            LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ(0),
            LW_PTRIM_GPC_GPCNAFLL_COEFF(0),
        }
    },
    {
        LW2080_CTRL_CLK_NAFLL_ID_GPC1,
        {
            LW_PTRIM_GPC_GPCLUT_WRITE_ADDR(1),
            LW_PTRIM_GPC_GPCLUT_WRITE_DATA(1),
            LW_PTRIM_GPC_GPCLUT_CFG(1),
            LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ(1),
            LW_PTRIM_GPC_GPCNAFLL_COEFF(1),
        }
    },
    {
        LW2080_CTRL_CLK_NAFLL_ID_GPC2,
        {
            LW_PTRIM_GPC_GPCLUT_WRITE_ADDR(2),
            LW_PTRIM_GPC_GPCLUT_WRITE_DATA(2),
            LW_PTRIM_GPC_GPCLUT_CFG(2),
            LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ(2),
            LW_PTRIM_GPC_GPCNAFLL_COEFF(2),
        }
    },
    {
        LW2080_CTRL_CLK_NAFLL_ID_GPC3,
        {
            LW_PTRIM_GPC_GPCLUT_WRITE_ADDR(3),
            LW_PTRIM_GPC_GPCLUT_WRITE_DATA(3),
            LW_PTRIM_GPC_GPCLUT_CFG(3),
            LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ(3),
            LW_PTRIM_GPC_GPCNAFLL_COEFF(3),
        }
    },
    {
        LW2080_CTRL_CLK_NAFLL_ID_GPC4,
        {
            LW_PTRIM_GPC_GPCLUT_WRITE_ADDR(4),
            LW_PTRIM_GPC_GPCLUT_WRITE_DATA(4),
            LW_PTRIM_GPC_GPCLUT_CFG(4),
            LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ(4),
            LW_PTRIM_GPC_GPCNAFLL_COEFF(4),
        }
    },
    {
        LW2080_CTRL_CLK_NAFLL_ID_GPC5,
        {
            LW_PTRIM_GPC_GPCLUT_WRITE_ADDR(5),
            LW_PTRIM_GPC_GPCLUT_WRITE_DATA(5),
            LW_PTRIM_GPC_GPCLUT_CFG(5),
            LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ(5),
            LW_PTRIM_GPC_GPCNAFLL_COEFF(5),
        }
    },
    {
        LW2080_CTRL_CLK_NAFLL_ID_GPCS,
        {
            LW_PTRIM_GPC_BCAST_GPCLUT_WRITE_ADDR,
            LW_PTRIM_GPC_BCAST_GPCLUT_WRITE_DATA,
            LW_PTRIM_GPC_BCAST_GPCLUT_CFG,
            LW_PTRIM_GPC_BCAST_GPCLUT_SW_FREQ_REQ,
            LW_PTRIM_GPC_BCAST_GPCNAFLL_COEFF,
        }
    },
    {
        LW2080_CTRL_CLK_NAFLL_ID_LWD,
        {
            LW_PTRIM_SYS_NAFLL_LWDLUT_WRITE_ADDR,
            LW_PTRIM_SYS_NAFLL_LWDLUT_WRITE_DATA,
            LW_PTRIM_SYS_NAFLL_LWDLUT_CFG,
            LW_PTRIM_SYS_NAFLL_LWDLUT_SW_FREQ_REQ,
            LW_PTRIM_SYS_NAFLL_LWDNAFLL_COEFF,
        }
    },
    {
        LW2080_CTRL_CLK_NAFLL_ID_HOST,
        {
            LW_PTRIM_SYS_NAFLL_HOSTLUT_WRITE_ADDR,
            LW_PTRIM_SYS_NAFLL_HOSTLUT_WRITE_DATA,
            LW_PTRIM_SYS_NAFLL_HOSTLUT_CFG,
            LW_PTRIM_SYS_NAFLL_HOSTLUT_SW_FREQ_REQ,
            LW_PTRIM_SYS_NAFLL_HOSTNAFLL_COEFF,
        }
    },
    // MUST be the last to find the end of array. 
    {
        LW2080_CTRL_CLK_NAFLL_ID_MAX,
        {
            CLK_NAFLL_REG_UNDEFINED,
            CLK_NAFLL_REG_UNDEFINED,
            CLK_NAFLL_REG_UNDEFINED,
            CLK_NAFLL_REG_UNDEFINED,
            CLK_NAFLL_REG_UNDEFINED,
        }
    },
};

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
