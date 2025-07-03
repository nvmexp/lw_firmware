/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_sect234.c
 */
//
// Includes
//
#include "acr_rv_sect234.h"
#include "external/rmlsfm.h"
#include "external/lwmisc.h"

#include <liblwriscv/io_pri.h>

#ifdef ACR_CHIP_PROFILE_T239
#include <liblwriscv/ptimer.h>
#endif

#include "dev_master.h"
#include "dev_fuse.h"
#include "dev_fb.h"
#include "dev_hshub_SW.h"
#include "dev_pwr_pri.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_pri_ringstation_sys_addendum.h"
#include "dev_graphics_nobundle.h"
#include "dev_pri_ringstation_gpc.h"
#include "dev_pri_ringstation_gpc_addendum.h"
#include "dev_ltc.h"
#include "dev_top.h"
#include "dev_smcarb.h"
#include "dev_pri_ringmaster.h"
#include "dev_bus.h"

/*!
 * This is temporary measure as current riscv header path for t239
 * is not supported; Once support is added for t239 header path remove
 * this define.
 */
#define LW_PMC_BOOT_42_CHIP_ID_GA10F      0x0000017E

/*!
 * SMC/MIG mode FECS/GPCCS instance register's are accessed using below defines,
 * These register address info is not part of ref-manual so need to fetch with
 * below define's/macro's.
 */
#define LW_SMC_PRIV_BASE    0x01000000
#define LW_SMC_PRIV_STRIDE  0x00200000
#define LW_GPC_PRI_STRIDE   0x00008000

#define LW_GET_PGRAPH_SMC_REG(legacyAddr, engine)   ((legacyAddr) + LW_SMC_PRIV_BASE + ((engine) * LW_SMC_PRIV_STRIDE) - DEVICE_BASE(LW_PGRAPH))
#define SMC_LEGACY_UNICAST_ADDR(addr, instance) (addr + (LW_GPC_PRI_STRIDE * instance))

#define FECS_BASE_ADDRESS(engine) (LW_GET_PGRAPH_SMC_REG(LW_PGRAPH_PRI_FECS_FALCON_IRQSSET, engine))
#define FECS_FALCON_SCTL(engine)  (FECS_BASE_ADDRESS(engine) + LW_PFALCON_FALCON_SCTL)

#define GPCCS_BASE_ADDRESS(engine) (SMC_LEGACY_UNICAST_ADDR(LW_PGRAPH_PRI_GPC0_GPCCS_FALCON_IRQSSET, engine))
#define GPCCS_FALCON_SCTL(engine)  (GPCCS_BASE_ADDRESS(engine) + LW_PFALCON_FALCON_SCTL)

#define FIELD_BITS(TARGET)  ((LW_PPRIV_SYS_PRI_SOURCE_ID_##TARGET << 1) + 1) : \
                            (LW_PPRIV_SYS_PRI_SOURCE_ID_##TARGET << 1)

#define LW_PPRIV_SYS_TARGET_MASK_PMU_ACCESS_CONTROL                            FIELD_BITS(PMU)
#define LW_PPRIV_SYS_TARGET_MASK_PMU_ACCESS_CONTROL_RW_ENABLED                      0x00000003
#define LW_PPRIV_SYS_TARGET_MASK_PMU_ACCESS_CONTROL_RW_L0                           0x00000002
#define LW_PPRIV_SYS_TARGET_MASK_PMU_ACCESS_CONTROL_BLOCK_WRITES_READ_L0            0x00000001
#define LW_PPRIV_SYS_TARGET_MASK_PMU_ACCESS_CONTROL_RW_DISABLED                     0x00000000

#define LW_PPRIV_SYS_TARGET_MASK_GSP_ACCESS_CONTROL                            FIELD_BITS(GSP)
#define LW_PPRIV_SYS_TARGET_MASK_GSP_ACCESS_CONTROL_RW_ENABLED                      0x00000003
#define LW_PPRIV_SYS_TARGET_MASK_GSP_ACCESS_CONTROL_RW_L0                           0x00000002
#define LW_PPRIV_SYS_TARGET_MASK_GSP_ACCESS_CONTROL_BLOCK_WRITES_READ_L0            0x00000001
#define LW_PPRIV_SYS_TARGET_MASK_GSP_ACCESS_CONTROL_RW_DISABLED                     0x00000000

/*! Compare with LW_FUSE_OPT_FUSE_UCODE_UDE_REV value to enable low performance configurations */
#define UDE_REV_FUSE_SET                                                            0X00000001

#define ACR_EMULATE_MODE_MASK                                                       0X000000FF
#define ACR_GPC0_1                                                                  0X00000002
#define ACR_GPC0_0                                                                  0X00000001

#define MIG_MODE                                                                    (1 << 8)

#define SOURCE_ENABLE_L3_READ_WRITE         0x3F88
#define ALL_SOURCE_ENABLE_L0_READ_WRITE     0xFFFFFFFF

#ifdef ACR_CHIP_PROFILE_T239
/*! Magic number to XOR with ptimer generated value,
* As ptimer generated value is always truly not random
*/
#define SSP_MAGIC_NUMBER 0x17CAEFF0D086FE91
#endif

/*!
 * @brief Verify CHIP ID is same as intended.\n
 *        This is a security feature so as to constrain a binary for a particular chip.
 *
 * @return ACR_OK If chip-id is as same as intended for your build.
 * @return ACR_ERROR_ILWALID_CHIP_ID If chip-id is not same as intended.
 */
ACR_STATUS
acrCheckIfBuildIsSupported_T234(void)
{
    LwU32 chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, priRead(LW_PMC_BOOT_42));

    if (chip == LW_PMC_BOOT_42_CHIP_ID_GA10B ||
            chip == LW_PMC_BOOT_42_CHIP_ID_GA10F)
    {
        return ACR_OK;
    }
    return  ACR_ERROR_ILWALID_CHIP_ID;
}

#ifdef ACR_CHIP_PROFILE_T239
static LwU64 acrGet64BitRandomNumber(void)
{
    LwU64 randomNumber  = ptimer_read();
    LwU64 lowerBits     = (randomNumber >> 4) & 0xFFFF;

    randomNumber ^= ((lowerBits<<48) | (lowerBits<<32) |
                     (lowerBits<<16) | lowerBits);

    return randomNumber;
}

LwU64 acrGetTimeBasedRandomCanary_T239(void)
{
    LwU64 result = acrGet64BitRandomNumber();

    //
    // Need to XOR with SSP_MAGIC_NUMBER because acrGet64BitRandomNumber
    // is not truly random
    //
    result ^= SSP_MAGIC_NUMBER;

    return result;
}
#endif

/*!
 * @brief Setup decode trap for CPU to access FBHUB registers.\n
 *        These registers can only be accessed by PL3 client by default.
 */
void acrSetupFbhubDecodeTrap_T234(void)
{
    priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP4_MATCH, DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP4_MATCH, _ADDR, LW_PFB_HSHUB_NUM_ACTIVE_LTCS(0)) |
                                                            DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP4_MATCH, _SOURCE_ID, PRI_SOURCE_ID_XVE));
    priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP4_MATCH_CFG, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP4_MATCH, _CFG_TRAP_APPLICATION_LEVEL0, _ENABLE));
    priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP4_MASK, DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP4_MASK, _SOURCE_ID, PRI_SOURCE_ID_XVE));
    priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP4_DATA1, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP4_DATA1, _V_FOR_ACTION_SET_PRIV_LEVEL, _VALUE_LEVEL_3));
    priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP4_ACTION, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP4_ACTION, _SET_PRIV_LEVEL, _ENABLE));

    priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP5_MATCH, DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP5_MATCH, _ADDR, LW_PFB_FBHUB_NUM_ACTIVE_LTCS) |
                                                            DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP5_MATCH, _SOURCE_ID, PRI_SOURCE_ID_XVE));
    priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP5_MATCH_CFG, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP5_MATCH, _CFG_TRAP_APPLICATION_LEVEL0, _ENABLE));
    priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP5_MASK, DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP5_MASK, _SOURCE_ID, PRI_SOURCE_ID_XVE));
    priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP5_DATA1, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP5_DATA1, _V_FOR_ACTION_SET_PRIV_LEVEL, _VALUE_LEVEL_3));
    priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP5_ACTION, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP5_ACTION, _SET_PRIV_LEVEL, _ENABLE));
}

/*!
 * @brief Setup decode trap for LS PMU to access GR falcon SCTL registers.\n
 *        These registers can only be accessed by PL3 client by default.
 */
void acrSetupSctlDecodeTrap_T234(void)
{
    if ((g_desc.mode & MIG_MODE) && (priRead(LW_PSMCARB_SYS_PIPE_INFO) == LW_PSMCARB_SYS_PIPE_INFO_MODE_SMC))
    {
    	//Decode Trap 6 controlling LW_PGRAPH_PRI_FECS_FALCON_SCTL-0x01009240 and LW_PGRAPH_PRI_FECS_FALCON_SCTL1-0x01209240 using *MASK_ADDR.
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP6_MATCH, DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP6_MATCH, _ADDR, FECS_FALCON_SCTL(0)) |
                                                        DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP6_MATCH, _SOURCE_ID, PRI_SOURCE_ID_PMU));
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP6_MATCH_CFG, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP6_MATCH, _CFG_TRAP_APPLICATION_LEVEL2, _ENABLE));
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP6_MASK,
        			DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP6_MASK, _SOURCE_ID, PRI_SOURCE_ID_PMU) |
					DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP6_MASK, _ADDR, 0x200000));
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP6_DATA1, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP6_DATA1, _V_FOR_ACTION_SET_PRIV_LEVEL, _VALUE_LEVEL_3));
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP6_ACTION, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP6_ACTION, _SET_PRIV_LEVEL, _ENABLE));

        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP7_MATCH, DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP7_MATCH, _ADDR, GPCCS_FALCON_SCTL(0)) |
                                                        DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP7_MATCH, _SOURCE_ID, PRI_SOURCE_ID_PMU));
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP7_MATCH_CFG, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP7_MATCH, _CFG_TRAP_APPLICATION_LEVEL2, _ENABLE));
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP7_MASK, DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP7_MASK, _SOURCE_ID, PRI_SOURCE_ID_PMU));
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP7_DATA1, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP7_DATA1, _V_FOR_ACTION_SET_PRIV_LEVEL, _VALUE_LEVEL_3));
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP7_ACTION, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP7_ACTION, _SET_PRIV_LEVEL, _ENABLE));

        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP8_MATCH, DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP8_MATCH, _ADDR, GPCCS_FALCON_SCTL(1)) |
                                                        DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP8_MATCH, _SOURCE_ID, PRI_SOURCE_ID_PMU));
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP8_MATCH_CFG, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP8_MATCH, _CFG_TRAP_APPLICATION_LEVEL2, _ENABLE));
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP8_MASK, DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP8_MASK, _SOURCE_ID, PRI_SOURCE_ID_PMU));
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP8_DATA1, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP8_DATA1, _V_FOR_ACTION_SET_PRIV_LEVEL, _VALUE_LEVEL_3));
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP8_ACTION, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP8_ACTION, _SET_PRIV_LEVEL, _ENABLE));
    }
    else
    {
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP6_MATCH, DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP6_MATCH, _ADDR, LW_PGRAPH_PRI_FECS_FALCON_SCTL) |
                                                                DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP6_MATCH, _SOURCE_ID, PRI_SOURCE_ID_PMU));
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP6_MATCH_CFG, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP6_MATCH, _CFG_TRAP_APPLICATION_LEVEL2, _ENABLE));
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP6_MASK, DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP6_MASK, _SOURCE_ID, PRI_SOURCE_ID_PMU));
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP6_DATA1, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP6_DATA1, _V_FOR_ACTION_SET_PRIV_LEVEL, _VALUE_LEVEL_3));
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP6_ACTION, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP6_ACTION, _SET_PRIV_LEVEL, _ENABLE));
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP7_MATCH, DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP7_MATCH, _ADDR, LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_SCTL) |
                                                                DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP7_MATCH, _SOURCE_ID, PRI_SOURCE_ID_PMU));
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP7_MATCH_CFG, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP7_MATCH, _CFG_TRAP_APPLICATION_LEVEL2, _ENABLE));
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP7_MASK, DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP7_MASK, _SOURCE_ID, PRI_SOURCE_ID_PMU));
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP7_DATA1, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP7_DATA1, _V_FOR_ACTION_SET_PRIV_LEVEL, _VALUE_LEVEL_3));
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP7_ACTION, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP7_ACTION, _SET_PRIV_LEVEL, _ENABLE));
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP8_MATCH, DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP8_MATCH, _ADDR, LW_PGRAPH_PRI_GPC0_GPCCS_FALCON_SCTL) |
                                                                DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP8_MATCH, _SOURCE_ID, PRI_SOURCE_ID_PMU));
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP8_MATCH_CFG, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP8_MATCH, _CFG_TRAP_APPLICATION_LEVEL2, _ENABLE));
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP8_MASK, DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP8_MASK, _SOURCE_ID, PRI_SOURCE_ID_PMU) |
                                                                DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP8_MASK, _ADDR, 0x8000));
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP8_DATA1, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP8_DATA1, _V_FOR_ACTION_SET_PRIV_LEVEL, _VALUE_LEVEL_3));
        priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP8_ACTION, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP8_ACTION, _SET_PRIV_LEVEL, _ENABLE));
    }
}

/*!
 * @brief Set PBUS PLM TO L3 access only
 *
 * Refer Bug: 3374805
 */
void acrSetupPbusDebugAccess_T234(void)
{
    priWrite(LW_PBUS_DEBUG_PRIV_LEVEL_MASK, SOURCE_ENABLE_L3_READ_WRITE);
}

/*!
 * @brief Set LTCS Clock Gating PLM registers to Lo read/write
 *
 * Refer Bug: 3469873
 */
void acrLowerClockGatingPLM_T234(void)
{
    priWrite(LW_PLTCG_LTCS_CGATE_PRIV_LEVEL_MASK, ALL_SOURCE_ENABLE_L0_READ_WRITE);
    priWrite(LW_PLTCG_LTCS_LTSS_CGATE_PRIV_LEVEL_MASK, ALL_SOURCE_ENABLE_L0_READ_WRITE);
}

/*!
 * @brief Enable VAB even if VPR is enabled
 *
 * Refer Bug: 3374805
 */
void acrSetupVprProtectedAccess_T234(void)
{
    LwU32 regVal = 0ULL;
    regVal = priRead(LW_PFB_PRI_MMU_ACCESS_LOGGING_VPR);
    regVal = FLD_SET_DRF(_PFB_PRI, _MMU_ACCESS, _LOGGING_VPR_VIDMEM, _PROTECTED_DISABLED, regVal);
    priWrite(LW_PFB_PRI_MMU_ACCESS_LOGGING_VPR, regVal);
}

#ifndef ACR_CHIP_PROFILE_T239
/*!
 * @brief  Emulate mode on t23x
 *
 * Configure LW_PLTCG_LTCS_LTSS_TSTG_CFG_1,
 * LW_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT,
 * LW_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_1,
 * LW_PTOP_ROP_THROTTLE_LIMIT to set parameters
 * to enable emulate mode.
 * 
 * @param[in] gpcNum Number of GPC to set at half performance.
 */
void acrEmulateMode_T234(LwU32 gpcNum)
{
    LwU32 regVal = 0ULL;
    LwU32 i;

    gpcNum = gpcNum & ACR_EMULATE_MODE_MASK;

    /*
     * Check whether LW_FUSE_OPT_FUSE_UCODE_UDE_REV is set and
     * 1:0 Bit of mode var is checked for specific value to enable emulate mode.
     */
    if (priRead(LW_FUSE_OPT_FUSE_UCODE_UDE_REV) != UDE_REV_FUSE_SET)
    {
        return;
    }

    if ((gpcNum != ACR_GPC0_0) && (gpcNum != ACR_GPC0_1))
    {
        return;
    }

    regVal = priRead(LW_PLTCG_LTCS_LTSS_TSTG_CFG_1);

    regVal = FLD_SET_DRF(_PLTCG_LTCS, _LTSS_TSTG, _CFG_1_ACTIVE_SETS, _HALF, regVal);

    priWrite(LW_PLTCG_LTCS_LTSS_TSTG_CFG_1, regVal);

    regVal = priRead(LW_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT);

    regVal = FLD_SET_DRF(_FUSE_FEATURE, _OVERRIDE_SM, _SPEED_SELECT_IMLA0, _REDUCED_SPEED_1_2, regVal);
    regVal = FLD_SET_DRF(_FUSE_FEATURE, _OVERRIDE_SM, _SPEED_SELECT_IMLA0_OVERRIDE, _TRUE, regVal);

    regVal = FLD_SET_DRF(_FUSE_FEATURE, _OVERRIDE_SM, _SPEED_SELECT_FMLA16, _REDUCED_SPEED_1_2, regVal);
    regVal = FLD_SET_DRF(_FUSE_FEATURE, _OVERRIDE_SM, _SPEED_SELECT_FMLA16_OVERRIDE, _TRUE, regVal);

    regVal = FLD_SET_DRF(_FUSE_FEATURE, _OVERRIDE_SM, _SPEED_SELECT_FMLA32, _REDUCED_SPEED_1_2, regVal);
    regVal = FLD_SET_DRF(_FUSE_FEATURE, _OVERRIDE_SM, _SPEED_SELECT_FMLA32_OVERRIDE, _TRUE, regVal);

    regVal = FLD_SET_DRF(_FUSE_FEATURE, _OVERRIDE_SM, _SPEED_SELECT_IMLA1, _REDUCED_SPEED_1_2, regVal);
    regVal = FLD_SET_DRF(_FUSE_FEATURE, _OVERRIDE_SM, _SPEED_SELECT_IMLA1_OVERRIDE, _TRUE, regVal);

    regVal = FLD_SET_DRF(_FUSE_FEATURE, _OVERRIDE_SM, _SPEED_SELECT_IMLA2, _REDUCED_SPEED_1_2, regVal);
    regVal = FLD_SET_DRF(_FUSE_FEATURE, _OVERRIDE_SM, _SPEED_SELECT_IMLA2_OVERRIDE, _TRUE, regVal);

    regVal = FLD_SET_DRF(_FUSE_FEATURE, _OVERRIDE_SM, _SPEED_SELECT_IMLA3, _REDUCED_SPEED_1_2, regVal);
    regVal = FLD_SET_DRF(_FUSE_FEATURE, _OVERRIDE_SM, _SPEED_SELECT_IMLA3_OVERRIDE, _TRUE, regVal);

    priWrite(LW_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT, regVal);

    regVal = priRead(LW_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_1);

    regVal = FLD_SET_DRF(_FUSE_FEATURE, _OVERRIDE_SM, _SPEED_SELECT_1_IMLA4, _REDUCED_SPEED_1_2, regVal);
    regVal = FLD_SET_DRF(_FUSE_FEATURE, _OVERRIDE_SM, _SPEED_SELECT_1_IMLA4_OVERRIDE, _TRUE, regVal);

    priWrite(LW_FUSE_FEATURE_OVERRIDE_SM_SPEED_SELECT_1, regVal);

    for (i = 0; i < gpcNum; i++) {
        regVal = priRead(LW_PTOP_ROP_THROTTLE_LIMIT(i));
        regVal = FLD_SET_DRF(_PTOP_ROP, _THROTTLE_LIMIT, _PHYSICAL_GPC, _0_50000, regVal);
        priWrite(LW_PTOP_ROP_THROTTLE_LIMIT(i), regVal);
    }

}

/*!
 * @brief Setup decode trap for SMC/MIG registers to increase PLM.\n
 *        These registers should not be modifiable from level-0 if VPR is enabled.
 *        Bug https://lwbugswb.lwpu.com/LwBugs5/SWBug.aspx?bugid=200772158&cmtNo=92
 */
static void acrSetupSMCDecodeTrap_T234(void)
{
	// Refer Decode Trap 0 to 3 at https://confluence.lwpu.com/display/AMPCTXSWPRIHUB/Ampere+DECODE_TRAP+usage+by+chip#AmpereDECODE_TRAPusagebychip-GA10bakaiGPUinT234

	//Decode Trap 0 controlling LW_PSMCARB_SMC_PARTITION_GPC_MAP(0), LW_PSMCARB_SMC_PARTITION_GPC_MAP(1) and LW_PSMCARB_SYS_PIPE_INFO
	priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP0_MATCH,
				DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP0_MATCH, _ADDR, LW_PSMCARB_SMC_PARTITION_GPC_MAP(0)));
	priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP0_MATCH_CFG,
				DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP0_MATCH, _CFG_TRAP_APPLICATION_LEVEL0, _ENABLE) |
				DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP0_MATCH, _CFG_TRAP_APPLICATION_LEVEL1, _ENABLE) |
				DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP0_MATCH, _CFG_TRAP_APPLICATION_LEVEL2, _ENABLE) |
				DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP0_MATCH, _CFG_IGNORE_READ, _IGNORED));
	priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP0_MASK,
				DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP0_MASK, _SOURCE_ID, PRI_SOURCE_ID_ALL) |
				DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP0_MASK, _ADDR, 0x84));
	priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP0_ACTION,
				DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP0_ACTION, _FORCE_ERROR_RETURN, _ENABLE));

	//Decode Trap 1 controlling LW_PFB_PRI_MMU_HYPERVISOR_CTL
	priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP1_MATCH,
				DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP1_MATCH, _ADDR, LW_PFB_PRI_MMU_HYPERVISOR_CTL));
	priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP1_MATCH_CFG,
				DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP1_MATCH, _CFG_TRAP_APPLICATION_LEVEL0, _ENABLE) |
				DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP1_MATCH, _CFG_TRAP_APPLICATION_LEVEL1, _ENABLE) |
				DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP1_MATCH, _CFG_TRAP_APPLICATION_LEVEL2, _ENABLE) |
				DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP1_MATCH, _CFG_IGNORE_READ, _IGNORED));
	priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP1_MASK,
				DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP1_MASK, _SOURCE_ID, PRI_SOURCE_ID_ALL));
	priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP1_ACTION,
				DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP1_ACTION, _FORCE_ERROR_RETURN, _ENABLE));

	//Decode Trap 2 controlling LW_PPRIV_MASTER_GPC_RS_MAP(0) and LW_PPRIV_MASTER_GPC_RS_MAP(1)
	priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP2_MATCH,
				DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP2_MATCH, _ADDR, LW_PPRIV_MASTER_GPC_RS_MAP(0)));
	priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP2_MATCH_CFG,
				DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP2_MATCH, _CFG_TRAP_APPLICATION_LEVEL0, _ENABLE) |
				DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP2_MATCH, _CFG_TRAP_APPLICATION_LEVEL1, _ENABLE) |
				DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP2_MATCH, _CFG_TRAP_APPLICATION_LEVEL2, _ENABLE) |
				DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP2_MATCH, _CFG_IGNORE_READ, _IGNORED));
	priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP2_MASK,
				DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP2_MASK, _SOURCE_ID, PRI_SOURCE_ID_ALL) |
				DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP2_MASK, _ADDR, 0x4));
	priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP2_ACTION,
				DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP2_ACTION, _FORCE_ERROR_RETURN, _ENABLE));

	//Decode Trap 3 controlling LW_PFB_PRI_MMU_SMC_ENG_CFG_1(0) and LW_PFB_PRI_MMU_SMC_ENG_CFG_1(1)
	priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP3_MATCH,
				DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP3_MATCH, _ADDR, LW_PFB_PRI_MMU_SMC_ENG_CFG_1(0)));
	priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP3_MATCH_CFG,
				DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP3_MATCH, _CFG_TRAP_APPLICATION_LEVEL0, _ENABLE) |
				DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP3_MATCH, _CFG_TRAP_APPLICATION_LEVEL1, _ENABLE) |
				DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP3_MATCH, _CFG_TRAP_APPLICATION_LEVEL2, _ENABLE) |
				DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP3_MATCH, _CFG_IGNORE_READ, _IGNORED));
	priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP3_MASK,
				DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP3_MASK, _SOURCE_ID, PRI_SOURCE_ID_ALL) |
				DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP3_MASK, _ADDR, 0x4));
	priWrite(LW_PPRIV_SYS_PRI_DECODE_TRAP3_ACTION,
				DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP3_ACTION, _FORCE_ERROR_RETURN, _ENABLE));
}

/*!
 * @brief Check VPR and SMC/MIG enablement as both should not be enabled same time.\n
 *        ACR boot fails with ACR_ERROR_ILWALID_OPERATION if both are enabled.
 *        If not then boot continues with PLM set to LEVEL-3 for SMC/MIG config register using decode trap.
 */
ACR_STATUS
acrVprSmcCheck_T234(void)
{
	LwU32  start   = 0;
	LwU32  end     = 0;
	LwU32  vprSize = 0;

	// Read start address
	start = (LwU64)priRead(LW_PFB_PRI_MMU_VPR_ADDR_LO);
	start = DRF_VAL(_PFB, _PRI_MMU_VPR_ADDR_LO, _VAL, start);

	// Read end address
	end = (LwU64)priRead(LW_PFB_PRI_MMU_VPR_ADDR_HI);
	end = DRF_VAL(_PFB, _PRI_MMU_VPR_ADDR_HI, _VAL, end);

	vprSize = end - start;

	// check for VPR enablement using vpr size,
	if (vprSize != 0U)
	{
		// Setup decode trap for SMC/MIG registers to increase PLM
		acrSetupSMCDecodeTrap_T234();

		/*
		 * check for SMC enablement, if enabled then ACR should halt as VPR and SMC
		 * are not allowed to operate same time. Bug 3172419 - comment 142
		 */
		if ((priRead(LW_PSMCARB_SYS_PIPE_INFO) != LW_PSMCARB_SYS_PIPE_INFO_MODE_LEGACY) ||
			(priRead(LW_PPRIV_MASTER_GPC_RS_MAP(0)) != LW_PPRIV_MASTER_GPC_RS_MAP_SMC_ENABLE_FALSE) ||
			(priRead(LW_PPRIV_MASTER_GPC_RS_MAP(1)) != LW_PPRIV_MASTER_GPC_RS_MAP_SMC_ENABLE_FALSE) ||
			(priRead(LW_PFB_PRI_MMU_SMC_ENG_CFG_1(0)) != LW_PFB_PRI_MMU_SMC_ENG_CFG_1_GPC_MASK_INIT) ||
			(priRead(LW_PFB_PRI_MMU_SMC_ENG_CFG_1(1)) != LW_PFB_PRI_MMU_SMC_ENG_CFG_1_GPC_MASK_INIT))
		{
			LwU32 tmp = 0;

			priWrite(LW_PSMCARB_SYS_PIPE_INFO, LW_PSMCARB_SYS_PIPE_INFO_MODE_LEGACY);
			priWrite(LW_PPRIV_MASTER_GPC_RS_MAP(0), LW_PPRIV_MASTER_GPC_RS_MAP_SMC_ENABLE_FALSE);
			priWrite(LW_PPRIV_MASTER_GPC_RS_MAP(1), LW_PPRIV_MASTER_GPC_RS_MAP_SMC_ENABLE_FALSE);
			priWrite(LW_PFB_PRI_MMU_SMC_ENG_CFG_1(0), LW_PFB_PRI_MMU_SMC_ENG_CFG_1_GPC_MASK_INIT);
			priWrite(LW_PFB_PRI_MMU_SMC_ENG_CFG_1(1), LW_PFB_PRI_MMU_SMC_ENG_CFG_1_GPC_MASK_INIT);
			tmp = priRead(LW_PFB_PRI_MMU_HYPERVISOR_CTL);
			tmp = FLD_SET_DRF(_PFB_PRI, _MMU_HYPERVISOR_CTL, _USE_SMC_VEID_TABLES, _DISABLE, 0x0);
			priWrite(LW_PFB_PRI_MMU_HYPERVISOR_CTL, tmp);
			priWrite(LW_PSMCARB_SMC_PARTITION_GPC_MAP(0), 0x0);
			priWrite(LW_PSMCARB_SMC_PARTITION_GPC_MAP(1), 0x0);

			return ACR_ERROR_ILWALID_OPERATION;
		}
	}

	return ACR_OK;
}
#endif

/*!
 * @brief Lock target falcon register during LS loading process in ACR ucode.\n
 *        This is a security feature so as to constrain access to only L3 ucode during LS loading and reduce options for attack.
 *
 * @param[in] pFlcnCfg     Falcon config
 * @param[in] setLock      TRUE if reg space is to be locked.
 *
 * @return ACR_OK If opt_selwre_target_mask_wr_selwre fuse is set to L3 and register programming was successful.
 * @return ACR_ERROR_ILWALID_CHIP_ID If opt_selwre_target_mask_wr_selwre is not set to L3.
 */
ACR_STATUS
acrlibLockFalconRegSpace_T234
(
    LwU32 sourceId,
    PACR_FLCN_CONFIG pTargetFlcnCfg,
    LwU32 *pTargetMaskPlmOldValue,
    LwU32 *pTargetMaskOldValue,
    LwBool setLock
)
{

    ACR_STATUS  status                  = ACR_OK;
    LwU32       targetMaskValData       = 0;
    LwU32       targetMaskPlm           = 0;
    LwU32       targetMaskIndexData     = 0;
    LwU32       sourceMask              = 0;
    LwU32       targetMaskRegister      = LW_PPRIV_SYS_TARGET_MASK;
    LwU32       targetMaskPlmRegister   = LW_PPRIV_SYS_TARGET_MASK_PRIV_LEVEL_MASK;
    LwU32       targetMaskIndexRegister = LW_PPRIV_SYS_TARGET_MASK_INDEX;

    if ((pTargetFlcnCfg == LW_NULL) || (pTargetMaskPlmOldValue == LW_NULL) || (pTargetMaskOldValue == LW_NULL))
    {
        status = ACR_ERROR_ILWALID_ARGUMENT;
        goto Cleanup;
    }

    if (sourceId == pTargetFlcnCfg->falconId)
    {
        status = ACR_ERROR_PRI_SOURCE_NOT_ALLOWED;
        return status;
    }

    // All falcons except GPC can be locked through PPRIV_SYS, GPC is locked through PPRIV_GPC
    if (pTargetFlcnCfg->falconId == LSF_FALCON_ID_GPCCS)
    {
        targetMaskRegister      = LW_PPRIV_GPC_GPCS_TARGET_MASK;
        targetMaskPlmRegister   = LW_PPRIV_GPC_GPCS_TARGET_MASK_PRIV_LEVEL_MASK;
        targetMaskIndexRegister = LW_PPRIV_GPC_GPCS_TARGET_MASK_INDEX;
    }

    // Lock falcon
    if (setLock == LW_TRUE)
    {

        // Step 1: Read, store and program TARGET_MASK PLM register.
        targetMaskPlm = priRead(targetMaskPlmRegister);

        // Save plm value to be restored during unlock
        *pTargetMaskPlmOldValue = targetMaskPlm;

        switch (sourceId)
        {
            case LSF_FALCON_ID_PMU_RISCV:
                sourceMask = LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_PMU;
                break;
            case LSF_FALCON_ID_GSP_RISCV:
                sourceMask = LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_GSP;
                break;
            default:
                status = ACR_ERROR_PRI_SOURCE_NOT_ALLOWED;
                goto Cleanup;
        }

        // Verify if PLM of TARGET_MASK does not allow programming from PMU
        if (!(DRF_VAL(_PPRIV, _SYS_TARGET_MASK_PRIV_LEVEL_MASK, _SOURCE_ENABLE, targetMaskPlm) & BIT(sourceMask)))
        {
            status = ACR_ERROR_PRI_SOURCE_NOT_ALLOWED;
            goto Cleanup;
        }

        //
        // Program PLM of TARGET_MASK registers to be accessible only from PMU.
        // And Block accesses from other sources
        //
        targetMaskPlm = FLD_SET_DRF_NUM( _PPRIV, _SYS_TARGET_MASK_PRIV_LEVEL_MASK, _SOURCE_ENABLE, BIT(sourceMask), targetMaskPlm);
        targetMaskPlm = FLD_SET_DRF( _PPRIV, _SYS_TARGET_MASK_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL, _BLOCK, targetMaskPlm);
        targetMaskPlm = FLD_SET_DRF( _PPRIV, _SYS_TARGET_MASK_PRIV_LEVEL_MASK, _SOURCE_WRITE_CONTROL, _BLOCK, targetMaskPlm);
        priWrite(targetMaskPlmRegister, targetMaskPlm);

        // Step 2: Read and program TARGET_MASK_INDEX register.
        targetMaskIndexData = priRead(targetMaskIndexRegister);

        targetMaskIndexData = FLD_SET_DRF_NUM( _PPRIV_SYS, _TARGET_MASK_INDEX, _INDEX, pTargetFlcnCfg->targetMaskIndex, targetMaskIndexData);
        priWrite(targetMaskIndexRegister, targetMaskIndexData);

        // Step 3: Read, store and program TARGET_MASK register.
        targetMaskValData  = priRead(targetMaskRegister);
        // Save Target Mask value to be restored.
        *pTargetMaskOldValue = targetMaskValData;

        // Give only read permission to other PRI sources and Read/Write permission to PMU.
        // TODO: Increase the protection by disabling read permission for other sources. Lwrrently it is not added due to
        // fmodel bug: https://lwbugs/200730932 [T234][VDK][GA10B]:
        // using LW_PPRIV_SYS_TARGET_MASK register to isolate FECS falcon causes TARGET_MASK_VIOLATION when accessing PMU falcon from CPU.
        targetMaskValData = FLD_SET_DRF(_PPRIV, _SYS_TARGET_MASK, _VAL, _W_DISABLED_R_ENABLED_PL0, targetMaskValData);

        switch (sourceId)
        {
            case LSF_FALCON_ID_PMU_RISCV:
                targetMaskValData = FLD_SET_DRF(_PPRIV, _SYS_TARGET_MASK, _PMU_ACCESS_CONTROL, _RW_ENABLED, targetMaskValData);
                break;
            case LSF_FALCON_ID_GSP_RISCV:
                targetMaskValData = FLD_SET_DRF(_PPRIV, _SYS_TARGET_MASK, _GSP_ACCESS_CONTROL, _RW_ENABLED, targetMaskValData);
                break;
            default:
                status = ACR_ERROR_PRI_SOURCE_NOT_ALLOWED;
                goto Cleanup;
        }

        priWrite(targetMaskRegister, targetMaskValData);
    }
    else // Release lock
    {
        // Reprogram TARGET_MASK_INDEX register.
        targetMaskIndexData = priRead(targetMaskIndexRegister);

        targetMaskIndexData = FLD_SET_DRF_NUM( _PPRIV_SYS, _TARGET_MASK_INDEX, _INDEX, pTargetFlcnCfg->targetMaskIndex, targetMaskIndexData);
        priWrite(targetMaskIndexRegister, targetMaskIndexData);

        // Restore TARGET_MASK register to previous value.
        targetMaskValData = *pTargetMaskOldValue;
        priWrite(targetMaskRegister, targetMaskValData);

        //
        // Restore TARGET_MASK_PLM to previous value to
        // enable all the sources to access TARGET_MASK registers.
        //
        targetMaskPlm = *pTargetMaskPlmOldValue;
        priWrite(targetMaskPlmRegister, targetMaskPlm);
    }

Cleanup:
    return status;
}
