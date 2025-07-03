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
 * @file: main.c
 */

//
// Includes
//
#include "rv_acr.h"
#include "acr_rv_sect234.h"
#include "external/pmuifcmn.h"

#include <liblwriscv/io.h>
#include <liblwriscv/shutdown.h>
#include <liblwriscv/dma.h>
#include <liblwriscv/fbif.h>
#include <liblwriscv/libc.h>

#ifdef IS_SSP_ENABLED
#include <liblwriscv/ssp.h>
#endif

#include <lwriscv/csr.h>
#include <lwriscv/status.h>
#include "dev_gsp_csb.h"
//
// Global variables
//
/*! g_desc is ACR DMEM descriptor containing data passed by LWGPU-RM Master */
RISCV_ACR_DESC   g_desc       ATTR_ALIGNED(ACR_DESC_ALIGN);
/*! g_dmaProp Global variable to store DMA properties for DMAing to/from SYSMEM & falcon's IMEM/DMEM */
ACR_DMA_PROP     g_dmaProp    ATTR_ALIGNED(LSF_LSB_HEADER_ALIGNMENT);

#define ALL_SOURCE_DISABLE_NO_READ_WRITE 0xF00
#define GSP_SOURCE_ENABLE_L3_READ_WRITE  0x3F88
#define ALL_SOURCE_ENABLE_L0_READ        0xFFFFFF0F

#define right_shift_16bits(v)    (v >> 16U)

//TODO: Remove once data section of actual acr is non zero.
volatile LwU32 global_var = 0xabcd1234;

extern void trapEntry(void);

/*!
 * @brief  Initialisation of core privilege level.
 *
 * Unlock IMEM/DMEM access and release priv lockdown. \n
 * Increase priv level of M mode ucode to PL3. \n
 */
static void acrSecInitCorePrivilege(void)
{
    LwU64 csrVal = 0ULL;

    // Set M mode PLM in MSPM to only L3 and increase priv level of ucode to L3 in MRSP
    csrVal = csr_read(LW_RISCV_CSR_MSPM);
    csr_write(LW_RISCV_CSR_MSPM, FLD_SET_DRF64(_RISCV, _CSR_MSPM, _MPLM, _LEVEL3 , csrVal));
    csrVal = csr_read(LW_RISCV_CSR_MRSP);
    csr_write(LW_RISCV_CSR_MRSP, FLD_SET_DRF64(_RISCV, _CSR_MRSP, _MRPL,_LEVEL3, csrVal) |
					FLD_SET_DRF64(_RISCV, _CSR_MRSP, _MRSEC, _SEC, csrVal));
}


static void acrSecinitPLMs(void)
{
    localWrite(LW_PRGNLCL_FALCON_AMAP_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_BOOTVEC_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_CPUCTL_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_DBGCTL_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_DIODT_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_DIODTA_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_DMA_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_DMEM_PRIV_LEVEL_MASK, GSP_SOURCE_ENABLE_L3_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_ENGCTL_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_EXE_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_HSCTL_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_IMEM_PRIV_LEVEL_MASK, GSP_SOURCE_ENABLE_L3_READ_WRITE );
    localWrite(LW_PRGNLCL_FALCON_INTR_CTRL_PRIV_LEVEL_MASK(1), ALL_SOURCE_ENABLE_L0_READ);
    localWrite(LW_PRGNLCL_FALCON_MTHDCTX_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_PMB_DMEM_PRIV_LEVEL_MASK(0), ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_PMB_IMEM_PRIV_LEVEL_MASK(0), ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_SAFETY_CTRL_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_SCTL_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_SHA_RAL_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_TMR_PRIV_LEVEL_MASK,ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_TRACEBUF_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_WDTMR_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FBIF_CTL2_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FBIF_REGIONCFG_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK, ALL_SOURCE_ENABLE_L0_READ);
    localWrite(LW_PRGNLCL_RISCV_BCR_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_RISCV_CPUCTL_PRIV_LEVEL_MASK, ALL_SOURCE_ENABLE_L0_READ);
    localWrite(LW_PRGNLCL_RISCV_IRQ_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_RISCV_MSIP_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_RISCV_LWCONFIG_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_RISCV_TRACEBUF_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_CG2_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_CSBERR_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_EXCI_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_EXTERR_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_BRKPT_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_ICD_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_IDLESTATE_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_KFUSE_LOAD_CTL_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_KMEM_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_RSTAT0_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_SP_MIN_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_FALCON_SVEC_SPR_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
    localWrite(LW_PRGNLCL_RISCV_ERR_PRIV_LEVEL_MASK, ALL_SOURCE_ENABLE_L0_READ);
    localWrite(LW_PRGNLCL_RISCV_PICSCMASK_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
#ifndef ACR_CHIP_PROFILE_T239
    localWrite(LW_PRGNLCL_RISCV_MSPM_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
#endif
	localWrite(LW_PRGNLCL_FALCON_BR_PARAMS_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_ENG_STATE_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_RISCV_DBGCTL_PRIV_LEVEL_MASK, ALL_SOURCE_ENABLE_L0_READ);
	localWrite(LW_PRGNLCL_RISCV_BOOTVEC_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_PRIVSTATE_PRIV_LEVEL_MASK, ALL_SOURCE_DISABLE_NO_READ_WRITE);
	localWrite(LW_PRGNLCL_FALCON_IRQSCMASK_PRIV_LEVEL_MASK, ALL_SOURCE_ENABLE_L0_READ);
}

/*!
 * @brief  Triggers riscvpanic on SSP check fail.
 *
 * When SSP check fails sspCheckFailHook() trrigers this function. \n
 * The value of mailbox0 is updated to ACR_ERROR_SSP_STACK_CHECK_FAILED  \n
 * and riscvpanic is triggered to halt ACR exelwtion. \n
 */
void sspFailHandler(void)
{
    localWrite(LW_PRGNLCL_FALCON_MAILBOX0,
                ACR_ERROR_SSP_STACK_CHECK_FAILED);
    acrSecinitPLMs();
    riscvShutdown();
}

/*!
 * @brief  Checks for priv failures and memerr/iopmperr and update Mailboxes.
 *
 * When there's a riscv ecpection trap handler is triggered which call this function. \n
 * This function checks for priv errors or mem/iopmp interrupts are genrated  \n
 * and updates the error status accordingly \n
 * For any other exception default riscv exception status is set. \n
 */
void acrHandler(void)
{
	uint32_t reg_data;
	uint32_t status = ACR_ERROR_RISCV_EXCEPTION_FAILURE;

	reg_data = localRead(LW_PRGNLCL_RISCV_PRIV_ERR_STAT);
	if (FLD_TEST_DRF(_PRGNLCL, _RISCV_PRIV_ERR_STAT, _VALID, _TRUE, reg_data))
    {
        status = ACR_ERROR_PRI_FAILURE;
        goto end;
    }

	reg_data = localRead(LW_PRGNLCL_RISCV_HUB_ERR_STAT);
	if (FLD_TEST_DRF(_PRISCV, _RISCV_HUB_ERR_STAT, _VALID, _TRUE, reg_data))
    {
		status = ACR_ERROR_PRI_FAILURE;
		goto end;
    }

    reg_data = localRead(LW_PRGNLCL_RISCV_PRIV_ERR_INFO);

    if ((right_shift_16bits(reg_data)) == 0xbadf)
    {
        status = ACR_ERROR_PRI_FAILURE;
        goto end;
    }

	reg_data = localRead(LW_PRGNLCL_FALCON_IRQSTAT);
    if (FLD_TEST_DRF(_PRGNLCL, _FALCON_IRQSTAT, _MEMERR, _TRUE, reg_data) ||
			FLD_TEST_DRF(_PRGNLCL, _FALCON_IRQSTAT, _IOPMP, _TRUE, reg_data))
    {
        status = ACR_ERROR_PRI_FAILURE;
        goto end;
    }

end:
	localWrite(LW_PRGNLCL_FALCON_MAILBOX0, status);
	localWrite(LW_PRGNLCL_FALCON_MAILBOX1, global_var);

	acrSecinitPLMs();
	riscvShutdown();
}

/*!
 * @brief  Initialisation of trace buffers.
 *
 * Configure LW_PRGNLCL_RISCV_TRACECTL to turn on trace buffer. \n
 */
static void acrTraceInit(void)
{
    // Turn on tracebuffer
    localWrite(LW_PRGNLCL_RISCV_TRACECTL,
               DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _MODE, _STACK) |
               DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _MMODE_ENABLE, _TRUE) |
               DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _SMODE_ENABLE, _TRUE) |
               DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _UMODE_ENABLE, _TRUE));
}

/*!
 * @brief  Configure FBIF apertures for mem access.
 *         Call fbifConfigureAperture from liblwriscv to setup config registers.\n
 *
 * @param[in] regionID Region ID non-zero in case of WPR.
 * @param[in] isWpr    Flag to indicate whether setup is to be done for WPR access.
 *
 * @return ACR_ERROR_DMA_FAILURE if aperture configuration fails.
 * @return ACR_OK                if successful.
 */
static ACR_STATUS acrSetupAperture(LwU32 regionID, LwBool isWpr)
{
    FBIF_APERTURE_CFG acrApertCfg = {0};

    if(isWpr)
    {
        acrApertCfg.apertureIdx = RM_PMU_DMAIDX_UCODE;
    }
    else
    {
        acrApertCfg.apertureIdx = RM_PMU_DMAIDX_PHYS_VID;
    }
    acrApertCfg.target = FBIF_TRANSCFG_TARGET_COHERENT_SYSTEM;
    acrApertCfg.bTargetVa = LW_FALSE;
    acrApertCfg.l2cWr = FBIF_TRANSCFG_L2C_L2_EVICT_NORMAL;
    acrApertCfg.l2cRd = FBIF_TRANSCFG_L2C_L2_EVICT_NORMAL;
    acrApertCfg.fbifTranscfWachk0Enable = LW_FALSE;
    acrApertCfg.fbifTranscfWachk1Enable = LW_FALSE;
    acrApertCfg.fbifTranscfRachk0Enable = LW_FALSE;
    acrApertCfg.fbifTranscfRachk1Enable = LW_FALSE;
    acrApertCfg.regionid = (LwU8)regionID;
 
    if (fbifConfigureAperture(&acrApertCfg, 1) != LWRV_OK)
    {
        return ACR_ERROR_DMA_FAILURE;
    }

    return ACR_OK;
}

/*!
 * @brief  Read FB physical addr of acr interface struct.
 *
 * Read LW_PRGNLCL_FALCON_MAILBOX0/1 to get lo/hi 32 bits repectively
 * of acr interface struct stored in non-wpr region of FB. \n
 */
static LwU64 acrGetDescAddr(void)
{
    LwU32 addr_lo = 0;
    LwU32 addr_hi = 0;

    addr_lo = localRead(LW_PRGNLCL_FALCON_MAILBOX0);
    addr_hi = localRead(LW_PRGNLCL_FALCON_MAILBOX1);

    return ((LwU64)(addr_hi) << 32 | (LwU64)(addr_lo));
}

/*!
 * @brief  Set External Interrrupts.
 * Set IRQ register to detect MEMERR on pri access
 */
void acrIrqSet(void)
{
	LwU32 irqDest = 0U;
    LwU32 irqMset = 0U;

	csr_clear(LW_RISCV_CSR_MCFG, DRF_SHIFTMASK64(LW_RISCV_CSR_MCFG_MPOSTIO));
    irqDest = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDEST, _MEMERR, _RISCV, irqDest);
	irqDest = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDEST, _IOPMP, _RISCV, irqDest);
    irqMset = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQMSET, _MEMERR, _SET, irqMset);
	irqMset = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQMSET, _IOPMP, _SET, irqMset);
    localWrite(LW_PRGNLCL_RISCV_IRQDEST, irqDest);
    localWrite(LW_PRGNLCL_RISCV_IRQMSET, irqMset);
    localWrite(LW_PRGNLCL_RISCV_IRQMCLR, ~irqMset);
    localWrite(LW_PRGNLCL_RISCV_IRQDELEG, 0);
    localWrite(LW_PRGNLCL_RISCV_PRIV_ERR_STAT, 0);

	/*
     * Enable external interrupt
     */
	csr_set(LW_RISCV_CSR_XIE, DRF_NUM64(_RISCV, _CSR_XIE, _XEIE, 1));
}

/*!
 * @brief  ACR ucode Entry. First starts with code exelwtion in Priv Level 0.
 *
 * Call acrSecInitCorePrivilege() to configure security level.
 * Call acrTraceInit() to configure trace buffer.
 * Add Default error code i.e. "ACR_ERROR_BIN_STARTED_BUT_NOT_FINISHED"
 */
int main(void)
{
    /*
     * Increase secure level of current peregrine core to L3
     * as soon as FMC starts.
     */
    acrSecInitCorePrivilege();

    ACR_STATUS status          = ACR_ERROR_BIN_STARTED_BUT_NOT_FINISHED;
    LWRV_STATUS     dmaRet          = LWRV_OK;
    LwU64      acrDmemDescAddr = 0ULL;
    LwU32      wprIndex        = 0U;
    RM_FLCN_ACR_REGION_PROP *pRegionProp;
	LwU32 regVal = 0U;

	/*
	* Set interrupt handling
	*/
    acrIrqSet();
    csr_write(LW_RISCV_CSR_MTVEC, (LwU64)&trapEntry);

	regVal = localRead(LW_PRGNLCL_SCP_CTL_CFG);
	regVal = FLD_SET_DRF(_PRGNLCL, _SCP_CTL_CFG, _LOCKDOWN_SCP, _ENABLE, regVal);
	localWrite(LW_PRGNLCL_SCP_CTL_CFG, regVal);

#ifdef IS_SSP_ENABLED
#ifdef ACR_CHIP_PROFILE_T239
    LwU64 val = acrGetTimeBasedRandomCanary_HAL();
    sspSetCanary(val);
#else
    /*
     * Generates SSP for all stack below this function and sets canary value,
     * uses SCP for random number generation for canary.
     */
    if (sspGenerateAndSetCanaryWithInit() != LWRV_OK)
    {
        status = ACR_ERROR_SCP_RNG_FAILED;
        goto end;
    }
#endif
#endif

    g_desc = (RISCV_ACR_DESC){0};

    acrTraceInit();

    /*
     * Verify if this binary should be allowed to run 
     * on riscv supported chip.
     */
    if ((status = acrCheckIfBuildIsSupported_HAL()) != ACR_OK) {
        goto end;
    }

    /*
     * Setup apertures to access non-wpr region of sysmem for
     * fetching interface struct.
     */
    if ((status = acrSetupAperture(0U, LW_FALSE)) != ACR_OK) {
        goto end;
    }

    /*
     * Fetch the phy addr in sys mem storing acr interface struct.
     * And then fetch the interface struct using DMA.
     */
    acrDmemDescAddr = acrGetDescAddr();
    if (!LW_IS_ALIGNED(acrDmemDescAddr, ACR_DESC_ALIGN))
    {
        status = ACR_ERROR_ILWALID_ARGUMENT;
        goto end;
    }
    dmaRet = dmaPa((LwU64)&g_desc, acrDmemDescAddr, sizeof(RISCV_ACR_DESC),
                                        RM_PMU_DMAIDX_PHYS_VID, LW_TRUE);
    if (dmaRet != LWRV_OK)
    {
        status = ACR_ERROR_DMA_FAILURE;
        goto end;
    }

#ifndef ACR_CHIP_PROFILE_T239
    if ((status = acrVprSmcCheck_HAL()) != ACR_OK) {
        goto end;
    }
#endif

    /*
     * Get the falcon debug status
     */
    localWrite(LW_PRGNLCL_FALCON_MAILBOX0, ACR_ERROR_BIN_STARTED_BUT_NOT_FINISHED);
    g_bIsDebug = acrIsDebugModeEnabled_HAL();
    /*
     * Find available WPR region and range.
     * Then setup mem aperture to access the WPR region.
     */
    if ((status = acrFindWprRegions_HAL(&wprIndex)) != ACR_OK)
    {
        goto end;
    }
    if ((status = acrSetupAperture(g_desc.wprRegionID, LW_TRUE)) != ACR_OK) {
        goto end;
    }

    pRegionProp         = &(g_desc.regions.regionProps[wprIndex]);
    if ((status = acrProgramFalconSubWpr_HAL(LSF_FALCON_ID_GSP_RISCV, 0U,
                (pRegionProp->startAddress), (pRegionProp->endAddress),
                ACR_SUB_WPR_MMU_RMASK_L3,
                ACR_SUB_WPR_MMU_WMASK_L3)) != ACR_OK)
    {
        goto end;
    }
    if ((status = acrProgramFalconSubWpr_HAL(LSF_FALCON_ID_PMU_RISCV, 0U,
                (pRegionProp->startAddress), (pRegionProp->endAddress),
                ACR_SUB_WPR_MMU_RMASK_L2_AND_L3,
                ACR_SUB_WPR_MMU_WMASK_L2_AND_L3)) != ACR_OK)
    {
        goto end;
    }

    /*
     * Populate global variable g_dmaProp with DMA properties of WPR region.
     */
    if ((status = acrPopulateDMAParameters_HAL(wprIndex)) != ACR_OK)
    {
        goto end;
    }

    if ((status = acrCopyUcodesToWpr_T234()) != ACR_OK)
    {
        goto end;
    }

    if ((status = acrBootstrapUcode()) != ACR_OK)
    {
        goto end;
    }

    acrSetupFbhubDecodeTrap_HAL();

#ifndef ACR_CHIP_PROFILE_T239
    acrEmulateMode_HAL(g_desc.mode);
#endif

    acrSetupVprProtectedAccess_HAL();

	acrSetupPbusDebugAccess_HAL();

    acrLowerClockGatingPLM_HAL();

end:
    localWrite(LW_PRGNLCL_FALCON_MAILBOX0, status);
    //TODO: Remove once data section of actual acr is non zero.
    localWrite(LW_PRGNLCL_FALCON_MAILBOX1, global_var);

    acrSecinitPLMs();
    riscvShutdown();
    return 0;
}
