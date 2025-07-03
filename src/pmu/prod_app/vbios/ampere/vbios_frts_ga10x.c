/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @copydoc vbios_frts.h
 *
 * @brief   Additional chip-specific functionality for Firmware Runtime Security
 *          for GA10X and later.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "pmu_bar0.h"
#ifdef UPROC_RISCV
#include "riscv_csr.h"
#endif // UPROC_RISCV
#include "mmu/mmucmn.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"
#include "dev_fb.h"
#include "dev_fuse.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objvbios.h"
#include "vbios/vbios_frts.h"
#include "pmu_objfuse.h"
#include "pmu_objpmu.h"

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * The FRTS interface specifies the offsets and sizes of structures not in terms
 * of bytes but in counts of 4KiB blocks. This is a named constant provided for
 * shifting those values to absolute values.
 */
#define VBIOS_FRTS_MEMORY_BLOCK_BITS                                       (12U)

/*!
 * The @ref LW_PFB_PRI_MMU_FALCON_PMU_CFGA and
 * @ref LW_PFB_PRI_MMU_FALCON_PMU_CFGB registers specify their addresses not in
 * terms of bytes but in counts of 4KiB blocks. This is a named constant
 * provided for shifting those values to absolute values.
 */
#define VBIOS_FRTS_CFG_ADDR_BLOCK_BITS                                     (12U)

/* ------------------------ Function Prototypes ----------------------------- */
/*!
 * @brief   Returns whether the PMU is lwrrently running in LS or not
 *
 * @note    This could normally be determined by @ref OBJPMU::bLSMode, but we
 *          need to inspect this very early in init, before that data can be set
 *          up.
 *
 * @return  @ref LW_TRUE    PMU is in LS
 * @return  @ref LW_FALSE   Otherwise
 */
static LwBool s_vbiosFrtsPmuIsLs_GA10X(void)
    GCC_ATTRIB_SECTION("imem_resident", "s_vbiosFrtsPmuIsLs_GA10X");

/*!
 * @brief   Verifies whether the FRTS secure scratch registers are actually
 *          secure
 *
 * @return  @ref FLCN_OK                        Success
 * @return  @ref FLCN_ERR_PRIV_SEC_VIOLATION    Scratch registers are not secure
 *                                              when the PMU is
 */
static FLCN_STATUS s_vbiosFrtsSelwreScratchVerify_GA10X(void)
    GCC_ATTRIB_SECTION("imem_resident", "s_vbiosFrtsSelwreScratchVerify_GA10X");

/*!
 * @brief   Verifies whether the DMA hardware (FBIF) registers are at
 *          appropriate security levels
 *
 * @return  @ref FLCN_OK                        Success
 * @return  @ref FLCN_ERR_PRIV_SEC_VIOLATION    Registers are not secure when
 *                                              the PMU is
 */
static FLCN_STATUS s_vbiosFrtsDmaRegisterSelwrityVerify_GA10X(void)
    GCC_ATTRIB_SECTION("imem_init", "s_vbiosFrtsDmaRegisterSelwrityVerify_GA10X");

/*!
 * @brief   Indicates whether priv level write security is enabled for the MMU
 *          registers with which the PMU is concerned.
 *
 * @param[out]  pbEnabled   Whether the PLM security is enabled or not
 *
 * @return  @ref FLCN_OK    Always
 */
static FLCN_STATUS s_vbiosFrtsMmuPlmWriteSelwrityEnabled_GA10X(LwBool *pbEnabled)
    GCC_ATTRIB_SECTION("imem_init", "s_vbiosFrtsMmuPlmWriteSelwrityEnabled_GA10X");

/*!
 * @brief   Verifies whether the sub-WPR registers are at appropriate security
 *          levels
 *
 * @note    Should only be called when the PMU is expected to be in LS
 *
 * @return  @ref FLCN_OK                        Success
 * @return  @ref FLCN_ERR_PRIV_SEC_VIOLATION    Registers are not secure
 */
static FLCN_STATUS s_vbiosFrtsSubWprRegisterSelwrityVerify_GA10X(void)
    GCC_ATTRIB_SECTION("imem_init", "s_vbiosFrtsSubWprRegisterSelwrityVerify_GA10X");

/*!
 * @brief   Verifies whether the WPR registers are at appropriate security
 *          levels
 *
 * @note    Should only be called when the PMU is expected to be in LS
 *
 * @return  @ref FLCN_OK                        Success
 * @return  @ref FLCN_ERR_PRIV_SEC_VIOLATION    Registers are not secure
 */
static FLCN_STATUS s_vbiosFrtsWprRegisterSelwrityVerify_GA10X(void)
    GCC_ATTRIB_SECTION("imem_init", "s_vbiosFrtsWprRegisterSelwrityVerify_GA10X");

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief   Retrieves the configuration data for Firmware Runtime Security and
 *          colwerts it into a chip-agnostic format.
 *
 * @param[out]  pConfig Pointer to configuration data to initialize
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  Any pointer argument is NULL
 * @return  @ref FLCN_ERR_ILWALID_STATE     FRTS output indicates it has not
 *                                          been set up; FRTS version
 *                                          unrecognized; FRTS media type not
 *                                          recognized
 * @return  Others                          Errors propagated from callees.
 */
FLCN_STATUS
vbiosFrtsConfigRead_GA10X
(
    VBIOS_FRTS_CONFIG *pConfig
)
{
    FLCN_STATUS status;
    LwU32 outputFlags;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pConfig != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosFrtsConfigRead_GA10X_exit);

    //
    // Initialize the version to "INVALID" to indicate FRTS is not available
    // until we actually read the version
    //
    pConfig->version = VBIOS_FRTS_CONFIG_VERSION_ILWALID;

    // Verify the security of the secure scratch registers before reading them.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_vbiosFrtsSelwreScratchVerify_GA10X(),
        vbiosFrtsConfigRead_GA10X_exit);

    outputFlags = REG_RD32(FECS,
        LW_PGC6_AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03_2);

    //
    // First determine whether FRTS is available, and if not, bail out.
    //
    // Note that in the future, when FRTS is available everywhere, this should
    // be made an error.
    //
    if (!FLD_TEST_REF(
            LW_PGC6_AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03_2_CMD_SUCCESSFUL,
            _YES,
            outputFlags))
    {
        goto vbiosFrtsConfigRead_GA10X_exit;
    }

    {
        const LwU32 inputFlagsVersionAndMedia = REG_RD32(FECS,
            LW_PGC6_AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2);

        switch (REF_VAL(
                    LW_PGC6_AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2_VERSION,
                    inputFlagsVersionAndMedia))
        {
            case LW_PGC6_AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2_VERSION_VER_1:
                pConfig->version = VBIOS_FRTS_CONFIG_VERSION_1;
                break;
            default:
                status = FLCN_ERR_ILWALID_STATE;
                break;
        }

        PMU_ASSERT_OK_OR_GOTO(status,
            status,
            vbiosFrtsConfigRead_GA10X_exit);

        switch (REF_VAL(
                    LW_PGC6_AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2_MEDIA_TYPE,
                    inputFlagsVersionAndMedia))
        {
            case LW_PGC6_AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2_MEDIA_TYPE_FB:
                pConfig->mediaType = VBIOS_FRTS_CONFIG_MEDIA_TYPE_FB;
                break;
            case LW_PGC6_AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2_MEDIA_TYPE_SYSMEM:
                pConfig->mediaType = VBIOS_FRTS_CONFIG_MEDIA_TYPE_SYSMEM;
                break;
            default:
                status = FLCN_ERR_ILWALID_STATE;
                break;
        }

        PMU_ASSERT_OK_OR_GOTO(status,
            status,
            vbiosFrtsConfigRead_GA10X_exit);

        //
        // Not necessarily relevant if mediaType is not FB, but fine to set
        // regardless.
        //
        pConfig->wprId = REF_VAL(
            LW_PGC6_AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2_WPR_ID,
            inputFlagsVersionAndMedia);
    }

    //
    // Set the offset and the size, translating from the block size to the byte
    // size while doing so. Need to cast to an LwU64 while doing so in case
    // the offset/size are more than 4GiB.
    //
    pConfig->offset = ((LwU64)REG_FLD_RD_DRF(FECS,
        _PGC6, _AON_FRTS_INPUT_WPR_OFFSET_SELWRE_SCRATCH_GROUP_03_1, _WPR_OFFSET)) <<
            VBIOS_FRTS_MEMORY_BLOCK_BITS;
    pConfig->size = ((LwU64)REG_FLD_RD_DRF(FECS,
        _PGC6, _AON_FRTS_INPUT_WPR_SIZE_SELWRE_SCRATCH_GROUP_03_0, _WPR_SIZE)) <<
            VBIOS_FRTS_MEMORY_BLOCK_BITS;

vbiosFrtsConfigRead_GA10X_exit:
    return status;
}

/*!
 * @brief   Verifies the memory aperture settings in the hardware register for
 *          the given DMA index are appropriate for accessing FRTS according to
 *          the config.
 *
 * @param[in]   pConfig Pointer to FRTS config to check
 * @param[in]   dmaIdx  Index of DMA aperture to check
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  Any pointer argument is NULL; dmaIdx
 *                                          is an invalid DMA index
 * @return  @ref FLCN_ERR_ILWALID_STATE     Hardware settings do not match
 * @return  Others                          Errors propagated from callees.
 */
FLCN_STATUS
vbiosFrtsConfigDmaIdxVerify_GA10X
(
    const VBIOS_FRTS_CONFIG *pConfig,
    LwU32 dmaIdx
)
{
    FLCN_STATUS status;
    LwU32 transcfg;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pConfig != NULL) &&
        (dmaIdx <= LW_CPWR_FBIF_TRANSCFG__SIZE_1),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosFrtsConfigDmaIdxVerify_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        s_vbiosFrtsDmaRegisterSelwrityVerify_GA10X(),
        vbiosFrtsConfigDmaIdxVerify_GA10X_exit);

    transcfg = REG_RD32(CSB, LW_CPWR_FBIF_TRANSCFG(dmaIdx));

    //
    // Ensure that the TARGET field in the register matches the mediaType of the
    // configuration
    //
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (FLD_TEST_REF(LW_CPWR_FBIF_TRANSCFG_TARGET, _LOCAL_FB, transcfg) &&
         (pConfig->mediaType == VBIOS_FRTS_CONFIG_MEDIA_TYPE_FB)) ||
        (FLD_TEST_REF(LW_CPWR_FBIF_TRANSCFG_TARGET, _COHERENT_SYSMEM, transcfg) &&
         (pConfig->mediaType == VBIOS_FRTS_CONFIG_MEDIA_TYPE_SYSMEM)),
        FLCN_ERR_ILWALID_STATE,
        vbiosFrtsConfigDmaIdxVerify_GA10X_exit);

    // If the target is FB, then ensure that the WPR settings match as well.
    if (FLD_TEST_REF(LW_CPWR_FBIF_TRANSCFG_TARGET, _LOCAL_FB, transcfg))
    {
        const LwU32 regioncfg = REG_RD32(CSB, LW_CPWR_FBIF_REGIONCFG);
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (DRF_IDX_VAL(_CPWR, _FBIF_REGIONCFG, _T, dmaIdx, regioncfg) ==
                pConfig->wprId),
            FLCN_ERR_ILWALID_STATE,
            vbiosFrtsConfigDmaIdxVerify_GA10X_exit);
    }

vbiosFrtsConfigDmaIdxVerify_GA10X_exit:
    return status;
}

/*!
 * @brief   Retrieves the expected offset and size for FRTS based on its sub-WPR
 *          configuration
 *
 * @param[in]   wprId   WPR ID being used for FRTS
 * @param[out]  pOffset Offset of FRTS within the memory region
 * @param[out]  pSize   Size of the FRTS region
 *
 * @return  @ref FLCN_OK                        Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT      A pointer argument was NULL;
 *                                              wprId does not correspond to a
 *                                              known WPR
 * @return  @ref FLCN_ERR_ILWALID_STATE         The FRTS WPR range is
 *                                              inconsistent
 * @return  Others                              Errors propagated from callees.
 */
FLCN_STATUS
vbiosFrtsSubWprRangeGet_GA10X
(
    LwU8 wprId,
    LwU64 *pOffset,
    LwU64 *pSize
)
{
    FLCN_STATUS status;
    LwU8 subWprIdx;
    LwU32 pmuCfgA;
    LwU32 pmuCfgB;
    LwU64 subWprStartAddr;
    LwU64 subWprExtentAddr;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pOffset != NULL) &&
        (pSize != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosFrtsSubWprRangeGet_GA10X_exit);

    // Verify the security of the hardware before reading any registers
    PMU_ASSERT_OK_OR_GOTO(status,
        s_vbiosFrtsSubWprRegisterSelwrityVerify_GA10X(),
        vbiosFrtsSubWprRangeGet_GA10X_exit);

    //
    // Use the wprId to get the hardware sub-WPR for the wprId. Note that we
    // ct_assert here to make sure the software-defined values are valid
    // hardware values.
    //
    ct_assert(
        (PMU_SUB_WPR_ID_1_FRTS_DATA_WPR1 < LW_PFB_PRI_MMU_FALCON_PMU_CFGA__SIZE_1) &&
        (PMU_SUB_WPR_ID_1_FRTS_DATA_WPR2 < LW_PFB_PRI_MMU_FALCON_PMU_CFGB__SIZE_1) &&
        (LW_PFB_PRI_MMU_FALCON_PMU_CFGA__SIZE_1 == LW_PFB_PRI_MMU_FALCON_PMU_CFGB__SIZE_1));
    status = FLCN_OK;
    switch (wprId)
    {
        case 1U:
            subWprIdx = PMU_SUB_WPR_ID_1_FRTS_DATA_WPR1;
            break;
        case 2U:
            subWprIdx = PMU_SUB_WPR_ID_1_FRTS_DATA_WPR2;
            break;
        default:
            status = FLCN_ERR_ILWALID_ARGUMENT;
            break;
    }
    PMU_ASSERT_OK_OR_GOTO(status,
        status,
        vbiosFrtsSubWprRangeGet_GA10X_exit);

    pmuCfgA = REG_RD32(BAR0, LW_PFB_PRI_MMU_FALCON_PMU_CFGA(subWprIdx));
    pmuCfgB = REG_RD32(BAR0, LW_PFB_PRI_MMU_FALCON_PMU_CFGB(subWprIdx));

    //
    // Get the subWprStartAddr and the subWprExtentAddr (i.e., one beyond the
    // last valid address). Both are aligned, so shift them appropriately.
    // Additionally, ADDR_HI is the last aligned block, so add 1 to it to get
    // the extent address.
    //
    subWprStartAddr = ((LwU64)REF_VAL(
        LW_PFB_PRI_MMU_FALCON_PMU_CFGA_ADDR_LO, pmuCfgA)) <<
            VBIOS_FRTS_CFG_ADDR_BLOCK_BITS;
    subWprExtentAddr = (((LwU64)REF_VAL(
        LW_PFB_PRI_MMU_FALCON_PMU_CFGB_ADDR_HI, pmuCfgB)) + 1U) <<
            VBIOS_FRTS_CFG_ADDR_BLOCK_BITS;

    // Ensure that the sub-region is coherent
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (subWprStartAddr <= subWprExtentAddr),
        FLCN_ERR_ILWALID_STATE,
        vbiosFrtsSubWprRangeGet_GA10X_exit);

    // Copy the start address and callwlate the size
    *pOffset = subWprStartAddr;
    *pSize = subWprExtentAddr - subWprStartAddr;

vbiosFrtsSubWprRangeGet_GA10X_exit:
    return status;
}

/*!
 * @brief   Verifies whether access to the FRTS via WPR can be considered secure
 *
 * @param[in]   wprId           WPR ID being used for FRTS
 * @param[in]   frtsWprOffset   Offset of FRTS within the memory region
 * @param[in]   frtsWprSize     Size of the FRTS region
 *
 * @note    Should only be called when the PMU is expected to be in LS
 *
 * @return  @ref FLCN_OK                        Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT      wprId does not correspond to a
 *                                              known WPR
 * @return  @ref FLCN_ERR_ILWALID_STATE         The WPR range is inconsistent
 * @return  Others                              Errors propagated from callees.
 */
FLCN_STATUS
vbiosFrtsWprAccessSelwreVerify_GA10X
(
    LwU8 wprId,
    LwU64 frtsWprOffset,
    LwU64 frtsWprSize
)
{
    FLCN_STATUS status;
    LwU64 wprStartAddr;
    LwU64 wprExtentAddr;

    // Verify the security of the hardware before reading any registers
    PMU_ASSERT_OK_OR_GOTO(status,
        s_vbiosFrtsWprRegisterSelwrityVerify_GA10X(),
        vbiosFrtsWprAccessSelwreVerify_GA10X_exit);

    //
    // Use the wprId to get the range of the WPR as a whole. Note that the end
    // address is the last valid aligned block, so we have to add 1 to get the
    // extent address.
    //
    status = FLCN_OK;
    switch (wprId)
    {
        case 1U:
            wprStartAddr = ((LwU64)REG_FLD_RD_DRF(BAR0,
                _PFB, _PRI_MMU_WPR1_ADDR_LO, _VAL)) <<
                    LW_PFB_PRI_MMU_WPR1_ADDR_LO_ALIGNMENT;
            wprExtentAddr = (((LwU64)REG_FLD_RD_DRF(BAR0,
                _PFB, _PRI_MMU_WPR1_ADDR_HI, _VAL)) + 1U) <<
                    LW_PFB_PRI_MMU_WPR1_ADDR_HI_ALIGNMENT;
            break;
        case 2U:
            wprStartAddr = ((LwU64)REG_FLD_RD_DRF(BAR0,
                _PFB, _PRI_MMU_WPR2_ADDR_LO, _VAL)) <<
                    LW_PFB_PRI_MMU_WPR2_ADDR_LO_ALIGNMENT;
            wprExtentAddr = (((LwU64)REG_FLD_RD_DRF(BAR0,
                _PFB, _PRI_MMU_WPR2_ADDR_HI, _VAL)) + 1U) <<
                    LW_PFB_PRI_MMU_WPR2_ADDR_HI_ALIGNMENT;
            break;
        default:
            status = FLCN_ERR_ILWALID_ARGUMENT;
            break;
    }
    PMU_ASSERT_OK_OR_GOTO(status,
        status,
        vbiosFrtsWprAccessSelwreVerify_GA10X_exit);

    // Ensure the WPR region is coherent and that the FRTS region is within it
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (wprStartAddr <= wprExtentAddr) &&
        (frtsWprOffset >= wprStartAddr) &&
        (frtsWprSize <= (wprExtentAddr - wprStartAddr)),
        FLCN_ERR_ILWALID_STATE,
        vbiosFrtsWprAccessSelwreVerify_GA10X_exit);

    //
    // As a fail-safe,  ensure that if the PMU makes a WPR request that does not
    // land in WPR that it will fail.
    //
    PMU_ASSERT_TRUE_OR_GOTO(status,
        REG_FLD_TEST_DRF_DEF(BAR0,
            _PFB, _PRI_MMU_WPR_ALLOW_READ, _MIS_MATCH2, _NO),
        FLCN_ERR_PRIV_SEC_VIOLATION,
        vbiosFrtsWprAccessSelwreVerify_GA10X_exit);

vbiosFrtsWprAccessSelwreVerify_GA10X_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
static LwBool
s_vbiosFrtsPmuIsLs_GA10X(void)
{
#ifdef UPROC_RISCV
    return FLD_TEST_REF(
        LW_RISCV_CSR_SRSP_SRPL, _LEVEL2, csr_read(LW_RISCV_CSR_SRSP));
#else // UPROC_RISCV
    return FLD_TEST_REF(
        LW_CPWR_FALCON_SCTL_LSMODE, _TRUE, REG_RD32(CSB, LW_CPWR_FALCON_SCTL));
#endif // UPROC_RISCV
}

static FLCN_STATUS
s_vbiosFrtsSelwreScratchVerify_GA10X(void)
{
    FLCN_STATUS status;
    const LwU32 scratchGroup03Plm = REG_RD32(FECS,
        LW_PGC6_AON_SELWRE_SCRATCH_GROUP_03_PRIV_LEVEL_MASK);

    //
    // Verify either that we're not running in LS or that the secure scratch
    // register can only be written be L3 agents.
    //
    PMU_ASSERT_TRUE_OR_GOTO(status,
        !s_vbiosFrtsPmuIsLs_GA10X() ||
        (FLD_TEST_REF(
            LW_PGC6_AON_SELWRE_SCRATCH_GROUP_03_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL0, _DISABLE,
            scratchGroup03Plm) &&
         FLD_TEST_REF(
            LW_PGC6_AON_SELWRE_SCRATCH_GROUP_03_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL1, _DISABLE,
            scratchGroup03Plm) &&
         FLD_TEST_REF(
            LW_PGC6_AON_SELWRE_SCRATCH_GROUP_03_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL2, _DISABLE,
            scratchGroup03Plm)),
        FLCN_ERR_PRIV_SEC_VIOLATION,
        s_vbiosFrtsSelwreScratchVerify_GA10X_exit);

s_vbiosFrtsSelwreScratchVerify_GA10X_exit:
    return status;
}

static FLCN_STATUS
s_vbiosFrtsDmaRegisterSelwrityVerify_GA10X(void)
{
    FLCN_STATUS status;
    const LwU32 regioncfgPlm = REG_RD32(CSB,
        LW_CPWR_FBIF_REGIONCFG_PRIV_LEVEL_MASK);

    //
    // Verify that all the registers with which we're concerned use the same PLM
    // register
    //
    ct_assert((LW_CPWR_FBIF_REGIONCFG_PRIV_LEVEL_MASK ==
                    LW_CPWR_FBIF_TRANSCFG__PRIV_LEVEL_MASK) &&
              (LW_CPWR_FBIF_REGIONCFG_PRIV_LEVEL_MASK ==
                    LW_CPWR_FBIF_REGIONCFG__PRIV_LEVEL_MASK));

    //
    // If the PMU is in LS, ensure that the FBIF registers cannot be redirected
    // somewhere else by an inselwre agent.
    //
    PMU_ASSERT_TRUE_OR_GOTO(status,
        !Pmu.bLSMode ||
        (FLD_TEST_REF(
            LW_CPWR_FBIF_REGIONCFG_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL0, _DISABLE,
            regioncfgPlm) &&
         FLD_TEST_REF(
            LW_CPWR_FBIF_REGIONCFG_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL1, _DISABLE,
            regioncfgPlm)),
        FLCN_ERR_PRIV_SEC_VIOLATION,
        s_vbiosFrtsDmaRegisterSelwrityVerify_GA10X_exit);

s_vbiosFrtsDmaRegisterSelwrityVerify_GA10X_exit:
    return status;
}

static FLCN_STATUS
s_vbiosFrtsMmuPlmWriteSelwrityEnabled_GA10X
(
    LwBool *pbEnabled
)
{
    //
    // The values of the MMU PLMs are controlled by a fuse, so check the fuse to
    // determine whether PLM security is enabled or not.
    //
    *pbEnabled = FLD_TEST_REF(
        LW_FUSE_OPT_SELWRE_MMU_PLM_WR_SELWRE_DATA, _ENABLE,
        fuseRead(LW_FUSE_OPT_SELWRE_MMU_PLM_WR_SELWRE));

    return FLCN_OK;
}

static FLCN_STATUS
s_vbiosFrtsSubWprRegisterSelwrityVerify_GA10X(void)
{
    FLCN_STATUS status;
    LwBool bWriteSelwrityEnabled;

    //
    // Verify that all the registers with which we're concerned use the same PLM
    // register
    //
    ct_assert(
        (LW_PFB_PRI_MMU_PMU_PRIV_LEVEL_MASK ==
            LW_PFB_PRI_MMU_FALCON_PMU_CFGA__PRIV_LEVEL_MASK) &&
        (LW_PFB_PRI_MMU_PMU_PRIV_LEVEL_MASK ==
            LW_PFB_PRI_MMU_FALCON_PMU_CFGB__PRIV_LEVEL_MASK));

    PMU_ASSERT_OK_OR_GOTO(status,
        s_vbiosFrtsMmuPlmWriteSelwrityEnabled_GA10X(&bWriteSelwrityEnabled),
        s_vbiosFrtsSubWprRegisterSelwrityVerify_GA10X_exit);

    PMU_ASSERT_TRUE_OR_GOTO(status,
        !bWriteSelwrityEnabled ||
        REG_FLD_TEST_DRF_DEF(BAR0,
            _PFB, _PRI_MMU_PMU_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ONLY_LEVEL3_ENABLED),
        FLCN_ERR_PRIV_SEC_VIOLATION,
        s_vbiosFrtsSubWprRegisterSelwrityVerify_GA10X_exit);

s_vbiosFrtsSubWprRegisterSelwrityVerify_GA10X_exit:
    return status;
}

static FLCN_STATUS
s_vbiosFrtsWprRegisterSelwrityVerify_GA10X(void)
{
    FLCN_STATUS status;
    LwBool bWriteSelwrityEnabled;

    //
    // Verify that all the registers with which we're concerned use the same PLM
    // register
    //
    ct_assert(
        (LW_PFB_PRI_MMU_WPR1_ADDR_LO__PRIV_LEVEL_MASK ==
            LW_PFB_PRI_MMU_WPR_CFG_PRIV_LEVEL_MASK) &&
        (LW_PFB_PRI_MMU_WPR1_ADDR_HI__PRIV_LEVEL_MASK ==
            LW_PFB_PRI_MMU_WPR_CFG_PRIV_LEVEL_MASK) &&
        (LW_PFB_PRI_MMU_WPR2_ADDR_LO__PRIV_LEVEL_MASK ==
            LW_PFB_PRI_MMU_WPR_CFG_PRIV_LEVEL_MASK) &&
        (LW_PFB_PRI_MMU_WPR2_ADDR_HI__PRIV_LEVEL_MASK ==
            LW_PFB_PRI_MMU_WPR_CFG_PRIV_LEVEL_MASK) &&
        (LW_PFB_PRI_MMU_WPR_ALLOW_READ__PRIV_LEVEL_MASK ==
            LW_PFB_PRI_MMU_WPR_CFG_PRIV_LEVEL_MASK));

    PMU_ASSERT_OK_OR_GOTO(status,
        s_vbiosFrtsMmuPlmWriteSelwrityEnabled_GA10X(&bWriteSelwrityEnabled),
        s_vbiosFrtsWprRegisterSelwrityVerify_GA10X_exit);

    PMU_ASSERT_TRUE_OR_GOTO(status,
        !bWriteSelwrityEnabled ||
        REG_FLD_TEST_DRF_DEF(BAR0,
            _PFB, _PRI_MMU_WPR_CFG_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ONLY_LEVEL3_ENABLED),
        FLCN_ERR_PRIV_SEC_VIOLATION,
        s_vbiosFrtsWprRegisterSelwrityVerify_GA10X_exit);

s_vbiosFrtsWprRegisterSelwrityVerify_GA10X_exit:
    return status;
}
