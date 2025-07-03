/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_apmga100.c
 * @brief  Contains all APM routines specific to GA100.
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"
#include "sec2_bar0.h"
#include "sec2_csb.h"
#include "sec2_hs.h"
#include "mmu/mmucmn.h"
#include "dev_fb.h"
#include "dev_gc6_island.h"

#include "lwdevid.h"
#include "dev_master.h"
#include "dev_fb.h"
#include "dev_pri_ringstation_fbp.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_smcarb.h"
#include "dev_graphics_nobundle.h"
#include "dev_pri_ringmaster.h"
#include "dev_smcarb_addendum.h"
#include "hwproject.h"
#include "dev_pri_ringstation_sys_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_objapm.h"
#include "apm/sec2_apmdefines.h"

/* ------------------------- Private Functions ------------------------------- */
static FLCN_STATUS _apmDecodeTrapCheckIfInUseHs_GA100(LwU32)
        GCC_ATTRIB_SECTION("imem_initHs", "_apmDecodeTrapCheckIfInUseHs_GA100");
static FLCN_STATUS _apmDecodeTrapConfigHs_GA100(LwU32, LwU32, LwU32, LwBool)
        GCC_ATTRIB_SECTION("imem_initHs", "_apmDecodeTrapConfigHs_GA100");

/* ------------------------- Public Functions ------------------------------- */
/**
 * @brief Reads the starting address of the APM RTS SubWPr from the MMU register.
 * 
 * @param pOutRts[out]  The pointer to the output variable in DMEM.
 * 
 * @return FLCN_STATUS  FLCN_OK if successful, relevant error otherwise.
 */
FLCN_STATUS
apmGetRtsFbOffset_GA100
(
    LwU64 *pOutRts
)
{
    FLCN_STATUS flcnStatus   = FLCN_ERR_ILLEGAL_OPERATION;
    LwU32       apmRtsOffset = 0;

    if (pOutRts == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Read the MMU register
    CHECK_FLCN_STATUS(BAR0_REG_RD32_ERRCHK(
        LW_PFB_PRI_MMU_FALCON_SEC_CFGA(SEC2_SUB_WPR_ID_5_APM_RTS_WPR1), &apmRtsOffset));

    // Extract the 4KB frame from the register
    apmRtsOffset = DRF_VAL(_PFB, _PRI_MMU_FALCON_SEC_CFGA, _ADDR_LO, apmRtsOffset);

    // Shift to colwert the 4KB page to the address
    *pOutRts = apmRtsOffset;
    *pOutRts <<= SHIFT_4KB;

ErrorExit:

    return flcnStatus;
}

#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_APM))

/**
 * @brief Check if APM has been enabled by the Hypervisor using either OOB or LwFlash
 *        VBIOS snaps the APM enablement bit in a secure scratch register for Ucode & RM to consume.
 *        Bit 0 in LW_PGC6_AON_SELWRE_SCRATCH_GROUP_20 is used for CC enable/disable bit.
 */
FLCN_STATUS
apmCheckIfEnabledHs_GA100()
{
    FLCN_STATUS flcnStatus   = FLCN_ERR_HS_APM_NOT_ENABLED;
    LwU32       chipId       = 0;
    LwU32       devId        = 0;
    LwU32       val          = 0;
    LwU32       apmMode      = 0;

    // If this is a debug board, continue allowing APM to be enabled
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_SELWRE_SECENGINE_DEBUG_DIS, &val));
    if (val != LW_FUSE_OPT_SELWRE_SECENGINE_DEBUG_DIS_DATA_NO)
    {
        // Check for APM Mode
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PGC6_AON_SELWRE_SCRATCH_GROUP_20, &apmMode));

        if (!FLD_TEST_DRF(_SEC2, _APM_VBIOS_SCRATCH, _CCM, _ENABLE, apmMode))
        {
            flcnStatus = FLCN_ERR_HS_APM_NOT_ENABLED;
            goto ErrorExit;
        }

        // SKU Check
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_PCIE_DEVIDA, &devId));
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &chipId));
        devId  = DRF_VAL( _FUSE, _OPT_PCIE_DEVIDA, _DATA, devId);
        chipId = DRF_VAL( _PMC, _BOOT_42, _CHIP_ID, chipId);    

        // Adding ct_assert to alert incase if the DevId values are changed for the defines
        ct_assert(LW_PCI_DEVID_DEVICE_P1001_SKU230 == 0x20B5);

        if (!((chipId == LW_PMC_BOOT_42_CHIP_ID_GA100) && ( devId == LW_PCI_DEVID_DEVICE_P1001_SKU230)))
        {
            flcnStatus = FLCN_ERR_HS_APM_NOT_ENABLED;
            goto ErrorExit;
        }
    }

    flcnStatus = FLCN_OK;

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Check if a given decode trap is in use. Fail if in use
 */
static FLCN_STATUS
_apmDecodeTrapCheckIfInUseHs_GA100
(
    LwU32 trapId
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       trapAction = 0;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP_ACTION(trapId), 
                                &trapAction));
    if (!FLD_TEST_DRF(_PPRIV, _SYS_PRI_DECODE_TRAP1_ACTION, _ALL_ACTIONS, _DISABLED, trapAction))
    {
        flcnStatus = FLCN_ERR_HS_DECODE_TRAP_ALREADY_IN_USE;
        goto ErrorExit;
    }

ErrorExit:
    return flcnStatus;

}

/*!
 * @brief Helper function to configure decode traps with the following settings
 *        1. Set the PLM of decode trap to only L3 writable, readable by all levels
 *        2. Trap accesses with Level0/1/2. Trap L3 if bAllowL3 is false.
 *        3. Ignore reads in match config and use SINGLE source ID ctl (matches all sources)
 */
static FLCN_STATUS
_apmDecodeTrapConfigHs_GA100
(
    LwU32  trapId,
    LwU32  priAddr,
    LwU32  addrMask,
    LwBool bAllowL3
)
{
    FLCN_STATUS flcnStatus   = FLCN_OK;
    LwU32       data         = 0;
    LwU32       plm          = 0;

    //
    // Allow read from all levels and restrict write only to L3.
    // Read is allowed to aid debugging, can be removed later.
    //
    {
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP_PRIV_LEVEL_MASK(trapId), &plm));
        plm = FLD_SET_DRF(_PPRIV, _SYS_PRI_DECODE_TRAP_PRIV_LEVEL_MASK, _READ_PROTECTION,  _ALL_LEVELS_ENABLED, plm);
        plm = FLD_SET_DRF(_PPRIV, _SYS_PRI_DECODE_TRAP_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ONLY_LEVEL3_ENABLED, plm);
        plm = FLD_SET_DRF_NUM(_PPRIV, _SYS_PRI_DECODE_TRAP_PRIV_LEVEL_MASK, _SOURCE_ENABLE, 
                                                                     BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_SEC2), plm);
        plm = FLD_SET_DRF(_PPRIV, _SYS_PRI_DECODE_TRAP_PRIV_LEVEL_MASK, _SOURCE_WRITE_CONTROL, _BLOCKED, plm);
        plm = FLD_SET_DRF(_PPRIV, _SYS_PRI_DECODE_TRAP_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL, _LOWERED, plm);

        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP_PRIV_LEVEL_MASK(trapId), plm));
    }

    // Trap levels 0,1,2 from all sources
    {
        data = 0;
        data = FLD_SET_DRF(_PPRIV, _SYS_PRI_DECODE_TRAP_MATCH_CFG, _TRAP_APPLICATION_LEVEL0, _ENABLE, data);
        data = FLD_SET_DRF(_PPRIV, _SYS_PRI_DECODE_TRAP_MATCH_CFG, _TRAP_APPLICATION_LEVEL1, _ENABLE, data);
        data = FLD_SET_DRF(_PPRIV, _SYS_PRI_DECODE_TRAP_MATCH_CFG, _TRAP_APPLICATION_LEVEL2, _ENABLE, data);
        if (bAllowL3 == LW_FALSE)
            data = FLD_SET_DRF(_PPRIV, _SYS_PRI_DECODE_TRAP_MATCH_CFG, _TRAP_APPLICATION_LEVEL3, _ENABLE, data);

        data = FLD_SET_DRF(_PPRIV, _SYS_PRI_DECODE_TRAP_MATCH_CFG, _IGNORE_READ, _IGNORED, data);
        data = FLD_SET_DRF(_PPRIV, _SYS_PRI_DECODE_TRAP_MATCH_CFG, _SOURCE_ID_CTL, _SINGLE, data);

        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP_MATCH_CFG(trapId), data));
    }

    // Enable all sources
    {
        data = 0;
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP_MASK(trapId),
                          DRF_NUM(_PPRIV, _SYS_PRI_DECODE_TRAP_MASK, _ADDR, addrMask) |
                          DRF_NUM(_PPRIV, _SYS_PRI_DECODE_TRAP_MASK, _SOURCE_ID, 0x3F)));
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP_MASK(trapId), &data));
    }

    // Match the given PRI address
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP_MATCH(trapId),
                      DRF_NUM(_PPRIV, _SYS_PRI_DECODE_TRAP_MATCH, _ADDR, priAddr) |
                      DRF_NUM(_PPRIV, _SYS_PRI_DECODE_TRAP_MATCH, _SOURCE_ID, 0x0)));

    // Zero out DATA1 and DATA2. These are not used since we set PRI_DECODE_TRAP_ACTION._FORCE_ERROR_RETURN
    {
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP_DATA1(trapId),
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP0_DATA1, _V, _I)));
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP_DATA2(trapId),
                            DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP0_DATA2, _V, _I)));
    }

    // Set decode trap ACTION to FORCE_ERROR_RETURN
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_SYS_PRI_DECODE_TRAP_ACTION(trapId),
                      DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP_ACTION, _FORCE_ERROR_RETURN, _ENABLE)));

ErrorExit:
    return flcnStatus;

}

/**
 * @brief Enable protections necessary to block interference to APM exelwtion state
 *        Following changes are done:
 *        1. Set SELWRE_TOP to 0, disables memory partitioning
 *        2. Disable & block SMC_MODE in SMCARB to disable PRI rerouting
 *        3. Initialize & block RS_MAP to have static GPC assignment
 *        4. Set LW_PFB_PRI_MMU_HYPERVISOR_CTL_USE_SMC_VEID_TABLES=0 and block access to it
 *        5. Initialize and block LW_PFB_PRI_MMU_SMC_ENG_CFG_0/1
 *        6. Enable fault reporting in VPR region
 */
FLCN_STATUS
apmEnableProtectionsHs_GA100(void)
{
    FLCN_STATUS flcnStatus   = FLCN_OK;
    LwU32       plm          = 0;
    LwU32       data         = 0;
    LwU32       i            = 0;

    //
    // Decode traps 22,23,24,29 are alloted for APM, check to make sure those are unused
    // These decode traps are expected to programmed only when APM is active
    //
    {
        CHECK_FLCN_STATUS(_apmDecodeTrapCheckIfInUseHs_GA100(LW_SEC2_APM_DECODE_TRAP_SMC_MODE));
        CHECK_FLCN_STATUS(_apmDecodeTrapCheckIfInUseHs_GA100(LW_SEC2_APM_DECODE_TRAP_RS_MAP));
        CHECK_FLCN_STATUS(_apmDecodeTrapCheckIfInUseHs_GA100(LW_SEC2_APM_DECODE_TRAP_HV_CTL));
        CHECK_FLCN_STATUS(_apmDecodeTrapCheckIfInUseHs_GA100(LW_SEC2_APM_DECODE_TRAP_ENC_CFG));
    }
    
    // Set SELWRE_TOP to 0
    {
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_SELWRE_TOP_PRIV_LEVEL_MASK, &plm));
        plm = FLD_SET_DRF(_PFB, _PRI_MMU_SELWRE_TOP_PRIV_LEVEL_MASK, _READ_PROTECTION,  _ALL_LEVELS_ENABLED, plm);
        plm = FLD_SET_DRF(_PFB, _PRI_MMU_SELWRE_TOP_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ONLY_LEVEL3_ENABLED, plm);
        plm = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_SELWRE_TOP_PRIV_LEVEL_MASK, _SOURCE_ENABLE, 
                                                                  BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_SEC2), plm);

        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_PRI_MMU_SELWRE_TOP_PRIV_LEVEL_MASK, plm));

        // Setting secure top to 0
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_PRI_MMU_MEM_PARTITION_SELWRE_TOP, 0x0));

        // Take away write protection completely
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_SELWRE_TOP_PRIV_LEVEL_MASK, &plm));
        plm = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_SELWRE_TOP_PRIV_LEVEL_MASK, _WRITE_PROTECTION, 0x0, plm); // Disable all levels
        plm = FLD_SET_DRF(_PFB, _PRI_MMU_SELWRE_TOP_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL, _LOWERED, plm);
        plm = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_SELWRE_TOP_PRIV_LEVEL_MASK, _SOURCE_ENABLE, 0x0, plm);

        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_PRI_MMU_SELWRE_TOP_PRIV_LEVEL_MASK, plm));
    }

    //
    // Disable and block SMC MODE in SMCARB
    //
    {
        CHECK_FLCN_STATUS(_apmDecodeTrapConfigHs_GA100( LW_SEC2_APM_DECODE_TRAP_SMC_MODE, LW_PSMCARB_SYS_PIPE_INFO, 0x0, LW_TRUE));

        // Return error if SMC is already enabled
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PSMCARB_SYS_PIPE_INFO, &data));
        if (FLD_TEST_DRF( _PSMCARB, _SYS_PIPE_INFO, _MODE, _SMC, data))
        {
            flcnStatus = FLCN_ERR_HS_APM_SMC_ENABLED;
            goto ErrorExit;
        }
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PSMCARB_SYS_PIPE_INFO, 
                       DRF_DEF(_PSMCARB, _SYS_PIPE_INFO, _MODE, _LEGACY)));
        
        CHECK_FLCN_STATUS(_apmDecodeTrapConfigHs_GA100( LW_SEC2_APM_DECODE_TRAP_SMC_MODE, LW_PSMCARB_SYS_PIPE_INFO, 0x0, LW_FALSE));
    }

    // Disable and configure RS_MAP
    {
        CHECK_FLCN_STATUS(_apmDecodeTrapConfigHs_GA100( LW_SEC2_APM_DECODE_TRAP_RS_MAP, LW_PPRIV_MASTER_GPC_RS_MAP(0), 0x1F, LW_TRUE));
        for (i = 0; i< LW_PPRIV_MASTER_GPC_RS_MAP__SIZE_1; i++)
        {
            CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PPRIV_MASTER_GPC_RS_MAP(i), 
                              DRF_DEF(_PPRIV, _MASTER_GPC_RS_MAP, _SMC_VALID, _FALSE) |
                              DRF_DEF(_PPRIV, _MASTER_GPC_RS_MAP, _SMC_ENGINE_ID, _INIT) |
                              DRF_DEF(_PPRIV, _MASTER_GPC_RS_MAP, _SMC_ENGINE_LOCAL_CLUSTER_ID, _INIT)));
        }
        CHECK_FLCN_STATUS(_apmDecodeTrapConfigHs_GA100( LW_SEC2_APM_DECODE_TRAP_RS_MAP, LW_PPRIV_MASTER_GPC_RS_MAP(0), 0x1F, LW_FALSE));
    }

    //
    // Disable and block HYPERVISOR_CTL MODE in SMCARB
    // Use Decode trap 23 for blocking this
    //
    {
        CHECK_FLCN_STATUS(_apmDecodeTrapConfigHs_GA100( LW_SEC2_APM_DECODE_TRAP_HV_CTL, LW_PFB_PRI_MMU_HYPERVISOR_CTL, 0x0, LW_TRUE));

        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_PRI_MMU_HYPERVISOR_CTL, 
                       DRF_DEF(_PFB, _PRI_MMU_HYPERVISOR_CTL, _SRIOV, _DISABLED) | 
                       DRF_DEF(_PFB, _PRI_MMU_HYPERVISOR_CTL, _USE_SMC_VEID_TABLES, _DISABLE)));

        CHECK_FLCN_STATUS(_apmDecodeTrapConfigHs_GA100( LW_SEC2_APM_DECODE_TRAP_HV_CTL, LW_PFB_PRI_MMU_HYPERVISOR_CTL, 0x0, LW_FALSE));
    }


    // Configure and disable ENGINE_CFG0/1
    {
        CHECK_FLCN_STATUS(_apmDecodeTrapConfigHs_GA100( LW_SEC2_APM_DECODE_TRAP_ENC_CFG, LW_PFB_PRI_MMU_SMC_ENG_CFG_0(0), 0x3F, LW_TRUE));
        for (i = 0; i< LW_PFB_PRI_MMU_SMC_ENG_CFG_0__SIZE_1; i++)
        {
            CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_PRI_MMU_SMC_ENG_CFG_0(i), 
                              DRF_DEF(_PFB, _PRI_MMU_SMC_ENG_CFG_0, _REMOTE_SWIZID, _INIT) |
                              DRF_DEF(_PFB, _PRI_MMU_SMC_ENG_CFG_0, _MMU_ENG_VEID_OFFSET, _INIT) |
                              DRF_DEF(_PFB, _PRI_MMU_SMC_ENG_CFG_0, _VEID_MAX, _INIT)));

            CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_PRI_MMU_SMC_ENG_CFG_1(i), 
                              DRF_DEF(_PFB, _PRI_MMU_SMC_ENG_CFG_1, _GPC_MASK, _INIT) |
                              DRF_DEF(_PFB, _PRI_MMU_SMC_ENG_CFG_1, _GPC_MASK_HW, _INIT)));
        }
        CHECK_FLCN_STATUS(_apmDecodeTrapConfigHs_GA100( LW_SEC2_APM_DECODE_TRAP_ENC_CFG, LW_PFB_PRI_MMU_SMC_ENG_CFG_0(0), 0x3F, LW_FALSE));
    }

    // Enable fault reporting in VPR region
    {
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_MODE, &data));
        data = FLD_SET_DRF(_PFB, _PRI_MMU_VPR_MODE, _VPR_FAULT_REPORT, _ENABLED, data);
        CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_PFB_PRI_MMU_VPR_MODE, data));
    }

ErrorExit:
    return flcnStatus;
}


/*!
 * @brief Update the scratch register with appropriate status, to be consumsed by
 *        rest of APM code.
 */
FLCN_STATUS
apmUpdateScratchWithStatus_GA100
(
    LwU32 statusCode
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    CHECK_FLCN_STATUS(CSB_REG_WR32_HS_ERRCHK(
        LW_CSEC_FALCON_COMMON_SCRATCH_GROUP_0(LW_SEC2_APM_SCRATCH_INDEX), statusCode));

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Verify and update the scratch register PLM
 */
FLCN_STATUS
apmUpdateScratchGroup0PlmHs_GA100()
{
    FLCN_STATUS flcnStatus   = FLCN_OK;
    LwU32       privMask     = 0;
    LwU32       data         = 0;

    CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(
        LW_CSEC_FALCON_COMMON_SCRATCH_GROUP_0_PRIV_LEVEL_MASK, &privMask));

    // Make sure PLM is in the setting we expect
    if (!FLD_TEST_DRF(_CSEC, _FALCON_COMMON_SCRATCH_GROUP_0_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ALL_LEVELS_DISABLED, privMask))
    {
        flcnStatus = FLCN_ERR_HS_APM_SCRATCH_PLM_ILWALID;
        goto ErrorExit;
    }
        
    CHECK_FLCN_STATUS(CSB_REG_RD32_HS_ERRCHK(
        LW_CSEC_FALCON_COMMON_SCRATCH_GROUP_0(LW_SEC2_APM_SCRATCH_INDEX), &data));

    if (data != LW_CSEC_FALCON_COMMON_SCRATCH_GROUP_0_VAL_INIT)
    {
        flcnStatus = FLCN_ERR_HS_APM_SCRATCH_INIT_ILWALID;
        goto ErrorExit;
    }

    privMask = FLD_SET_DRF(_CSEC, _FALCON_COMMON_SCRATCH_GROUP_0_PRIV_LEVEL_MASK,
                           _READ_PROTECTION_LEVEL0, _ENABLE, privMask);
    privMask = FLD_SET_DRF(_CSEC, _FALCON_COMMON_SCRATCH_GROUP_0_PRIV_LEVEL_MASK,
                           _READ_PROTECTION_LEVEL1, _ENABLE, privMask);
    privMask = FLD_SET_DRF(_CSEC, _FALCON_COMMON_SCRATCH_GROUP_0_PRIV_LEVEL_MASK,
                           _READ_PROTECTION_LEVEL2, _ENABLE, privMask);
                           
    privMask = FLD_SET_DRF(_CSEC, _FALCON_COMMON_SCRATCH_GROUP_0_PRIV_LEVEL_MASK,
                           _WRITE_PROTECTION_LEVEL0, _DISABLE, privMask);
    privMask = FLD_SET_DRF(_CSEC, _FALCON_COMMON_SCRATCH_GROUP_0_PRIV_LEVEL_MASK,
                           _WRITE_PROTECTION_LEVEL1, _DISABLE, privMask);
    privMask = FLD_SET_DRF(_CSEC, _FALCON_COMMON_SCRATCH_GROUP_0_PRIV_LEVEL_MASK,
                           _WRITE_PROTECTION_LEVEL2, _DISABLE, privMask);
                           
    CHECK_FLCN_STATUS(CSB_REG_WR32_HS_ERRCHK(
        LW_CSEC_FALCON_COMMON_SCRATCH_GROUP_0_PRIV_LEVEL_MASK, privMask));

ErrorExit:
    return flcnStatus;
}

#endif
