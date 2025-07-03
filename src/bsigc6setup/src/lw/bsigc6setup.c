/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: bsigc6setup.c
 */

//
// Includes
//
#include <falcon-intrinsics.h>
#include "dev_pwr_csb.h"
#include "dev_pwr_pri.h"
#include "dev_lw_xp.h"
#include "dev_bus.h"
#include "lwtypes.h"
#include "lwmisc.h"
#include "dev_gc6_island.h"
#include "osptimer.h"

// Global variable to hold Loader config
#define ATTR_OVLY(ov)               __attribute__ ((section(ov)))
#define ATTR_ALIGNED(align)         __attribute__ ((aligned(align)))

#define USE_EMEM_APERTURE(addr)          ((addr) | BIT(28))

#define BAR0_REG_RD32(addr)                                                \
        (*(const volatile LwU32 *)USE_EMEM_APERTURE(addr))

    #define BAR0_REG_WR32(addr, data)                                      \
        do                                                                 \
        {                                                                  \
            *(volatile LwU32 *)USE_EMEM_APERTURE(addr) = (data);           \
        }                                                                  \
        while (LW_FALSE)

#define PEXFASTWAKE_TIMEOUT 0x13880 //50us

int main(int argc, char **argv)
{

    LwU32 data32;
    LwU32 dataTimer;
    //FLCN_TIMESTAMP timeStart;

    // Enable direct BAR0 access, set timeout to ~10ms
    falc_stxb_i((LwU32*)(LW_CPWR_FALCON_DMEMAPERT), 0,
        DRF_NUM(_CPWR_FALCON, _DMEMAPERT, _ENABLE,    1) |
        DRF_NUM(_CPWR_FALCON, _DMEMAPERT, _TIME_UNIT, 7) |
        DRF_NUM(_CPWR_FALCON, _DMEMAPERT, _TIME_OUT,  166));

    //Configure iLA
    //BAR0_REG_WR32(0x0008DA00, 0x0007FE7F);
    //BAR0_REG_WR32(0x0008DA04, 0x00000000);
    //BAR0_REG_WR32(0x0008DA20, 0x00701001);
    //BAR0_REG_WR32(0x0008DA24, 0x00000000);
    //BAR0_REG_WR32(0x0008DB30, 0x00000000);
    //BAR0_REG_WR32(0x0008DA28, 0x00100000);
    //BAR0_REG_WR32(0x0008DA2C, 0xFFEFFFFF);
    //BAR0_REG_WR32(0x0008DA34, 0xFFFFFFFF);
    //BAR0_REG_WR32(0x0008DA3C, 0xFFFFFFFF);
    //BAR0_REG_WR32(0x0008DA44, 0xFFFFFFFF);
    //BAR0_REG_WR32(0x0008DA4C, 0xFFFFFFFF);
    //BAR0_REG_WR32(0x0008DB94, 0xFFFFFFFF);
    //BAR0_REG_WR32(0x0008DB9C, 0xFFFFFFFF);
    //BAR0_REG_WR32(0x0008DA54, 0xFFFFFFFF);
    //BAR0_REG_WR32(0x0008DB38, 0xFFFFFFFF);
    //BAR0_REG_WR32(0x0008DB40, 0xFFFFFFFF);
    //BAR0_REG_WR32(0x0008DB48, 0xFFFFFFFF);
    //BAR0_REG_WR32(0x0008DB50, 0xFFFFFFFF);
    //BAR0_REG_WR32(0x0008DB58, 0xFFFFFFFF);
    //BAR0_REG_WR32(0x0008DB60, 0xFFFFFFFF);

    // Start iLA
    //BAR0_REG_WR32(0x0008DA00, 0x0007FEBF);
    //data32 = BAR0_REG_RD32(LW_XP_LA_CR_1);
    //data32 = FLD_SET_DRF(_XP, _LA_CR_1, _START_LOGGING, _PENDING, data32);
    //data32 = FLD_SET_DRF(_XP, _LA_CR_1, _STOP_LOGGING, _DONE, data32);
    //data32 = FLD_SET_DRF(_XP, _LA_CR_1, _ABSOLUTE_TIME_COUNT_RESET, _DONE, data32);
    //BAR0_REG_WR32(LW_XP_LA_CR_1, data32);

    //Extend LTSSM_HOLDOFF timeout
    BAR0_REG_WR32(LW_PBUS_HOLD_LTSSM, 0xe1);
    BAR0_REG_WR32(LW_XP_PEX_PAD_CTL_SEL, 0x1);

    // Waking up PLL
    data32 = BAR0_REG_RD32(LW_XP_PEX_PLL_CTL1);
    data32 = FLD_SET_DRF(_XP, _PEX_PLL_CTL1, _PLL_RSTN, _DISABLE, data32);
    data32 = FLD_SET_DRF(_XP, _PEX_PLL_CTL1, _PLL_PWR_OVRD, _ENABLE, data32);
    BAR0_REG_WR32(LW_XP_PEX_PLL_CTL1, data32);
    data32 = BAR0_REG_RD32(LW_XP_PEX_PLL_CTL1);
    data32 = FLD_SET_DRF(_XP, _PEX_PLL_CTL1, _PLL_IDDQ, _DISABLE, data32);
    BAR0_REG_WR32(LW_XP_PEX_PLL_CTL1, data32);
    data32 = BAR0_REG_RD32(LW_XP_PEX_PLL_CTL1);
    data32 = FLD_SET_DRF(_XP, _PEX_PLL_CTL1, _PLL_SLEEP, _DISABLE, data32);
    BAR0_REG_WR32(LW_XP_PEX_PLL_CTL1, data32);

    //AUX_TX_IDDQ enable
    BAR0_REG_WR32(LW_XP_PEX_PAD_CTL_4, 0x210000);
    BAR0_REG_WR32(LW_XP_PEX_PAD_CTL_1, 0x24000330);
    BAR0_REG_WR32(LW_XP_PEX_PAD_CTL_1, 0x24000300);

    //Term enable
    BAR0_REG_WR32(LW_XP_PEX_PAD_CTL_2, 0x104001);

    //BYPASS enable
    BAR0_REG_WR32(LW_XP_PEX_PAD_CTL_3, 0x81000);

    // Few us delay
    data32 = BAR0_REG_RD32(LW_PPWR_FALCON_PTIMER0);
    do
    {
        //dataTimer = BAR0_REG_RD32(LW_PPWR_FALCON_PTIMER0);
        if ((BAR0_REG_RD32(LW_PPWR_FALCON_PTIMER0) - data32) > PEXFASTWAKE_TIMEOUT)
        {
            break;
        }
    }
    while (LW_TRUE);
    //BAR0_REG_WR32(LW_PGC6_BSI_SCRATCH(0), dataTimer-data32);

    //BYPASS disable
    BAR0_REG_WR32(LW_XP_PEX_PAD_CTL_3, 0x0);

    //Reverting the pex_fast_wake
    BAR0_REG_WR32(LW_XP_PEX_PAD_CTL_3, 0x00000);
    BAR0_REG_WR32(LW_XP_PEX_PAD_CTL_1, 0x24000330);
    BAR0_REG_WR32(LW_XP_PEX_PAD_CTL_1, 0x00000330);
    BAR0_REG_WR32(LW_XP_PEX_PAD_CTL_4, 0x010000);
    BAR0_REG_WR32(LW_XP_PEX_PAD_CTL_2, 0x104001);
    BAR0_REG_WR32(LW_XP_PEX_PAD_CTL_SEL, 0xffff);

    // Sleep PLL
    data32 = BAR0_REG_RD32(LW_XP_PEX_PLL_CTL1);
    data32 = FLD_SET_DRF(_XP, _PEX_PLL_CTL1, _PLL_SLEEP, _ENABLE, data32);
    BAR0_REG_WR32(LW_XP_PEX_PLL_CTL1, data32);
    data32 = BAR0_REG_RD32(LW_XP_PEX_PLL_CTL1);
    data32 = FLD_SET_DRF(_XP, _PEX_PLL_CTL1, _PLL_IDDQ, _ENABLE, data32);
    BAR0_REG_WR32(LW_XP_PEX_PLL_CTL1, data32);
    data32 = BAR0_REG_RD32(LW_XP_PEX_PLL_CTL1);
    data32 = FLD_SET_DRF(_XP, _PEX_PLL_CTL1, _PLL_RSTN, _ENABLE, data32);
    data32 = FLD_SET_DRF(_XP, _PEX_PLL_CTL1, _PLL_PWR_OVRD, _DISABLE, data32);
    BAR0_REG_WR32(LW_XP_PEX_PLL_CTL1, data32);

    //Get current timer value
    //osPTimerTimeNsLwrrentGet(&timeStart);

    dataTimer = BAR0_REG_RD32(LW_PPWR_FALCON_PTIMER0);
    do
    {
        data32 = BAR0_REG_RD32(LW_PGC6_BSI_DEBUG2);
        if (FLD_TEST_DRF_NUM(_PGC6_BSI, _DEBUG2, _RXSTATIDLE, 0x1, data32))
        {
            break;
        }
        if ((BAR0_REG_RD32(LW_PPWR_FALCON_PTIMER0) - dataTimer) > PEXFASTWAKE_TIMEOUT)
        {
            break;
        }
    }
    while (LW_TRUE);

//    while (osPTimerTimeNsElapsedGet(&timeStart) <
//                PEXFASTWAKE_TIMEOUT);


    //Release LTSSM hold
    //BAR0_REG_WR32(LW_PBUS_HOLD_LTSSM, 0x30);

    //Indicating bsigc6setup ucode exelwted
    //BAR0_REG_WR32(LW_PGC6_BSI_SCRATCH(0), 0xd01e);

    // Write RDY4TRANS
    BAR0_REG_WR32(LW_PPWR_PMU_PMU2BSI_RDY4TRANS,  LW_PPWR_PMU_PMU2BSI_RDY4TRANS_STATUS_DIS);

    return 0;
}

