/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _DEEPL1_GEN3_H_
#define _DEEPL1_GEN3_H_

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"

#define RMA_SEL_DATA32  0x6
#define RMA_SEL_DATA32  0x6
#define AR_IS_IO        1
#define AR_ISNT_IO      0
#ifdef FALSE
#undef FALSE
#endif
#ifdef TRUE
#undef TRUE
#endif
#define FALSE           0
#define TRUE            1

#define  DEVICE_ADDR_3_FERMI  0x4c //7'b1001_100
#define  DEVICE_ADDR_2_FERMI  0x4d //7'b1001_101
#define  DEVICE_ADDR_1_FERMI  0x4e //7'b1001_110
#define  DEVICE_ADDR_0_FERMI  0x4f //7'b1001_111
#define  READ_BYTE       0x1
#define  WRITE_BYTE      0x2
#define  READ_BYTE_CRC   0x3
#define  WRITE_BYTE_CRC  0x4
#define  READ_WORD       0x9
#define  WRITE_WORD      0xa
#define  READ_WORD_CRC   0xb
#define  WRITE_WORD_CRC  0xc
#define  PRE_ARP         0x1
#define  RST_ARP         0x2
#define  GET_UID         0x3
#define  SET_UID         0x4
#define  GET_UID_DIR     0x5
#define  RST_ARP_DIR     0x6

#define LW_XVE_PRIV_XV_1                                               0x00000488 /* RW-4R */

#define LW_XVE_PRIV_XV_1_CYA_XVE_MULTIFUNC                                  25:25 /* RWIVF */
#define LW_XVE_PRIV_XV_1_CYA_XVE_MULTIFUNC_INIT                        0x00000001 /* RWI-V */
#define LW_XVE_PRIV_XV_1_CYA_XVE_MULTIFUNC_DISABLE                     0x00000000 /* RW--V */
#define LW_XVE_PRIV_XV_1_CYA_XVE_MULTIFUNC_ENABLE                      0x00000001 /* RW--V */

#define LW_XVE_PEX_PLL                                        0x00000644 /* RW-4R */

#define LW_XVE_PEX_PLL_PWRDN_EN                                      0:0 /* RWIVF */
#define LW_XVE_PEX_PLL_PWRDN_EN_DISBLED                       0x00000000 /* RWI-V */
#define LW_XVE_PEX_PLL_PWRDN_EN_ENABLED                       0x00000001 /* RW--V */

#define LW_XVE_PEX_PLL_PWRDN_D3_EN                                   1:1 /* RWIVF */
#define LW_XVE_PEX_PLL_PWRDN_D3_EN_DISBLED                    0x00000000 /* RWI-V */
#define LW_XVE_PEX_PLL_PWRDN_D3_EN_ENABLED                    0x00000001 /* RW--V */

#define LW_XVE_PEX_PLL_PWRDN_NON_D0_EN                               2:2 /* RWIVF */
#define LW_XVE_PEX_PLL_PWRDN_NON_D0_EN_DISBLED                0x00000000 /* RWI-V */
#define LW_XVE_PEX_PLL_PWRDN_NON_D0_EN_ENABLED                0x00000001 /* RW--V */

#define LW_XVE_PEX_PLL_PWRDN_L1_ASPM_EN                              3:3 /* RWIVF */
#define LW_XVE_PEX_PLL_PWRDN_L1_ASPM_EN_DISBLED               0x00000000 /* RWI-V */
#define LW_XVE_PEX_PLL_PWRDN_L1_ASPM_EN_ENABLED               0x00000001 /* RW--V */

#define LW_XVE_PEX_PLL_HOSTCLK_IDLE_WAIT_EN                           4:4 /* RWIVF */
#define LW_XVE_PEX_PLL_HOSTCLK_IDLE_WAIT_EN_DISBLED            0x00000000 /* RWI-V */
#define LW_XVE_PEX_PLL_HOSTCLK_IDLE_WAIT_EN_ENABLED            0x00000001 /* RW--V */

#define LW_XVE_PEX_PLL_LOCKDET_FIXED_DLY_EN                           5:5 /* RWIVF */
#define LW_XVE_PEX_PLL_LOCKDET_FIXED_DLY_EN_DISBLED            0x00000000 /* RWI-V */
#define LW_XVE_PEX_PLL_LOCKDET_FIXED_DLY_EN_ENABLED            0x00000001 /* RW--V */

#define LW_XVE_PEX_PLL_CLK_REQ_EN                                     6:6 /* RWIVF */
#define LW_XVE_PEX_PLL_CLK_REQ_EN_DISBLED                      0x00000000 /* RWI-V */
#define LW_XVE_PEX_PLL_CLK_REQ_EN_ENABLED                      0x00000001 /* RW--V */

#define LW_XVE_PEX_PLL_SWITCH_REFCLK_EN                               7:7 /* RWIVF */
#define LW_XVE_PEX_PLL_SWITCH_REFCLK_EN_DISBLED                0x00000000 /* RW--V */
#define LW_XVE_PEX_PLL_SWITCH_REFCLK_EN_ENABLED                0x00000001 /* RWI-V */

#define LW_XVE_PEX_PLL_PWRDN_SEQ_START_XCLK_DLY                      31:8 /* RWIVF */
#define LW_XVE_PEX_PLL_PWRDN_SEQ_START_XCLK_DLY_DEFAULT        0x00004000 /* RWI-V */

#define LW_XVE_PWR_MGMT_1                                   0x00000064 /* RW-4R */

#define LW_XVE_PWR_MGMT_1_PME_DATA                               31:24 /* C--VF */
#define LW_XVE_PWR_MGMT_1_PME_DATA_UNS                      0x00000000 /* C---V */
#define LW_XVE_PWR_MGMT_1_PME_BPCC                               23:23 /* C--VF */
#define LW_XVE_PWR_MGMT_1_PME_BPCC_UNS                      0x00000000 /* C---V */
#define LW_XVE_PWR_MGMT_1_PME_B2B3                               22:22 /* C--VF */
#define LW_XVE_PWR_MGMT_1_PME_B2B3_UNS                      0x00000000 /* C---V */
#define LW_XVE_PWR_MGMT_1_PME_STATUS                             15:15 /* RWIVF */
#define LW_XVE_PWR_MGMT_1_PME_STATUS_NOT_ACTIVE             0x00000000 /* R-I-V */
#define LW_XVE_PWR_MGMT_1_PME_STATUS_ACTIVE                 0x00000001 /* R---V */
#define LW_XVE_PWR_MGMT_1_PME_STATUS_CLEAR                  0x00000001 /* -W--C */
#define LW_XVE_PWR_MGMT_1_PME_DATA_SCALE                         14:13 /* C--VF */
#define LW_XVE_PWR_MGMT_1_PME_DATA_SCALE_UNS                0x00000000 /* C---V */
#define LW_XVE_PWR_MGMT_1_PME_DATA_SEL                            12:9 /* C--VF */
#define LW_XVE_PWR_MGMT_1_PME_DATA_SEL_UNS                  0x00000000 /* C---V */
#define LW_XVE_PWR_MGMT_1_PME                                      8:8 /* RWIVF */
#define LW_XVE_PWR_MGMT_1_PME_DISABLE                       0x00000000 /* RWI-V */
#define LW_XVE_PWR_MGMT_1_PME_ENABLE                        0x00000001 /* RW--V */
#define LW_XVE_PWR_MGMT_1_NO_SOFT_RESET                            3:3 /* C--VF */
#define LW_XVE_PWR_MGMT_1_NO_SOFT_RESET_ENABLE              0x00000001 /* C---V */
#define LW_XVE_PWR_MGMT_1_NO_SOFT_RESET_DISABLE             0x00000000 /* ----V */
#define LW_XVE_PWR_MGMT_1_PWR_STATE                                1:0 /* RWIVF */
#define LW_XVE_PWR_MGMT_1_PWR_STATE_D0                      0x00000000 /* RWI-V */
#define LW_XVE_PWR_MGMT_1_PWR_STATE_D1                      0x00000001 /* RW--V */
#define LW_XVE_PWR_MGMT_1_PWR_STATE_D2                      0x00000002 /* RW--V */
#define LW_XVE_PWR_MGMT_1_PWR_STATE_D3HOT                   0x00000003 /* RW--V */

#define LW_XVE_PEX_PLL5                                                0x00000654 /* RW-4R */

#define LW_XVE_PEX_PLL5_HOST2XV_HOST_IDLE_WAKE_EN                             0:0 /* RWIVF */
#define LW_XVE_PEX_PLL5_HOST2XV_HOST_IDLE_WAKE_EN_DISABLED             0x00000000 /* RWI-V */
#define LW_XVE_PEX_PLL5_HOST2XV_HOST_IDLE_WAKE_EN_ENABLED              0x00000001 /* RW--V */

#define LW_XVE_PEX_PLL5_HOST2XV_FORCE_CLK_ON_WAKE_EN                          1:1 /* RWIVF */
#define LW_XVE_PEX_PLL5_HOST2XV_FORCE_CLK_ON_WAKE_EN_ENABLED           0x00000001 /* RWI-V */
#define LW_XVE_PEX_PLL5_HOST2XV_FORCE_CLK_ON_WAKE_EN_DISABLED          0x00000000 /* RW--V */

#define LW_XVE_PEX_PLL5_UPSTREAM_REQ_WAKE_EN                                  2:2 /* RWIVF */
#define LW_XVE_PEX_PLL5_UPSTREAM_REQ_WAKE_EN_ENABLED                   0x00000001 /* RWI-V */
#define LW_XVE_PEX_PLL5_UPSTREAM_REQ_WAKE_EN_DISABLED                  0x00000000 /* RW--V */

#define LW_XVE_PEX_PLL5_XP_PAD_IDLE_WAKE_EN                                   3:3 /* RWIVF */
#define LW_XVE_PEX_PLL5_XP_PAD_IDLE_WAKE_EN_ENABLED                    0x00000001 /* RWI-V */
#define LW_XVE_PEX_PLL5_XP_PAD_IDLE_WAKE_EN_DISABLED                   0x00000000 /* RW--V */

#define LW_XVE_PEX_PLL5_XP2XV_XP_IDLE_WAKE_EN                                 4:4 /* RWIVF */
#define LW_XVE_PEX_PLL5_XP2XV_XP_IDLE_WAKE_EN_ENABLED                  0x00000001 /* RWI-V */
#define LW_XVE_PEX_PLL5_XP2XV_XP_IDLE_WAKE_EN_DISABLED                 0x00000000 /* RW--V */

#define LW_XVE_PEX_PLL5_XV_COMMON_IDLE_WAKE_EN                                5:5 /* RWIVF */
#define LW_XVE_PEX_PLL5_XV_COMMON_IDLE_WAKE_EN_ENABLED                 0x00000001 /* RWI-V */
#define LW_XVE_PEX_PLL5_XV_COMMON_IDLE_WAKE_EN_DISABLED                0x00000000 /* RW--V */

#define LW_XVE_PEX_PLL5_CONFIG0_UPDATE_WAKE_EN                                6:6 /* RWIVF */
#define LW_XVE_PEX_PLL5_CONFIG0_UPDATE_WAKE_EN_ENABLED                 0x00000001 /* RWI-V */
#define LW_XVE_PEX_PLL5_CONFIG0_UPDATE_WAKE_EN_DISABLED                0x00000000 /* RW--V */

#define LW_XVE_PEX_PLL5_CONFIG1_UPDATE_WAKE_EN                                7:7 /* RWIVF */
#define LW_XVE_PEX_PLL5_CONFIG1_UPDATE_WAKE_EN_ENABLED                 0x00000001 /* RWI-V */
#define LW_XVE_PEX_PLL5_CONFIG1_UPDATE_WAKE_EN_DISABLED                0x00000000 /* RW--V */

#define LW_XVE_PRIV_XV_BLKCG2                                                0x00000658 /* RW-4R */

#define LW_XVE_PRIV_XV_BLKCG2_HOST2XV_HOST_IDLE_WAKE_EN                             0:0 /* RWIVF */
#define LW_XVE_PRIV_XV_BLKCG2_HOST2XV_HOST_IDLE_WAKE_EN_DISABLED             0x00000000 /* RWI-V */
#define LW_XVE_PRIV_XV_BLKCG2_HOST2XV_HOST_IDLE_WAKE_EN_ENABLED              0x00000001 /* RW--V */

#define LW_XVE_PRIV_XV_BLKCG2_HOST2XV_FORCE_CLK_ON_WAKE_EN                          1:1 /* RWIVF */
#define LW_XVE_PRIV_XV_BLKCG2_HOST2XV_FORCE_CLK_ON_WAKE_EN_ENABLED           0x00000001 /* RWI-V */
#define LW_XVE_PRIV_XV_BLKCG2_HOST2XV_FORCE_CLK_ON_WAKE_EN_DISABLED          0x00000000 /* RW--V */

#define LW_XVE_PRIV_XV_BLKCG2_UPSTREAM_REQ_WAKE_EN                                  2:2 /* RWIVF */
#define LW_XVE_PRIV_XV_BLKCG2_UPSTREAM_REQ_WAKE_EN_ENABLED                   0x00000001 /* RWI-V */
#define LW_XVE_PRIV_XV_BLKCG2_UPSTREAM_REQ_WAKE_EN_DISABLED                  0x00000000 /* RW--V */

#define LW_XVE_PRIV_XV_BLKCG2_XP_PAD_IDLE_WAKE_EN                                   3:3 /* RWIVF */
#define LW_XVE_PRIV_XV_BLKCG2_XP_PAD_IDLE_WAKE_EN_ENABLED                    0x00000001 /* RWI-V */
#define LW_XVE_PRIV_XV_BLKCG2_XP_PAD_IDLE_WAKE_EN_DISABLED                   0x00000000 /* RW--V */

#define LW_XVE_PRIV_XV_BLKCG2_CONFIG0_UPDATE_WAKE_EN                               4:4 /* RWIVF */
#define LW_XVE_PRIV_XV_BLKCG2_CONFIG0_UPDATE_WAKE_EN_ENABLED                0x00000001 /* RWI-V */
#define LW_XVE_PRIV_XV_BLKCG2_CONFIG0_UPDATE_WAKE_EN_DISABLED               0x00000000 /* RW--V */

#define LW_XVE_PRIV_XV_BLKCG2_CONFIG1_UPDATE_WAKE_EN                               5:5 /* RWIVF */
#define LW_XVE_PRIV_XV_BLKCG2_CONFIG1_UPDATE_WAKE_EN_ENABLED                0x00000001 /* RWI-V */
#define LW_XVE_PRIV_XV_BLKCG2_CONFIG1_UPDATE_WAKE_EN_DISABLED               0x00000000 /* RW--V */

class deepL1_gen3 : public LWGpuSingleChannelTest {
public:
    deepL1_gen3(ArgReader *params);

    virtual ~deepL1_gen3(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    // a little extra cleanup to be done
    virtual void CleanUp(void);

    virtual void DelayNs(UINT32);

    virtual int Checkstate(UINT32);

protected:

private:
    string m_paramsString;
    UINT32 arch;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(deepL1_gen3, deepL1_gen3, "GPU Deep L1 & CLKREQ# Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &deepL1_gen3_testentry
#endif

#endif  // _DEEPL1_GEN3_H_
