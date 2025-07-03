/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_sspt210.c
 */
#include "rng_scp.h"
#include "acr.h"
#include "dev_pwr_falcon_csb.h"
#include "dev_pwr_csb.h"
#include "falcon.h"
#include "acr_objacr.h"

#ifdef IS_SSP_ENABLED

/*!
 * @brief stack check handler in case SSP(Stack Smashing Protection) feature detects stack corruption.\n
 *        It writes "ACR_ERROR_SSP_STACK_CHECK_FAILED" error code to specify STACK corruption and then HALTs falcon exelwtion.
 */
void __stack_chk_fail(void)
{
    acrWriteStatusToFalconMailbox(ACR_ERROR_SSP_STACK_CHECK_FAILED);
    falc_halt();
}

/*!
 * @brief Disable RNG in SCP by writing below bits. This allows re-configuration of RNG.RNG is used for generating RANDOM number.\n
 *        -# SEQ_CLEAR to clear sequencer.\n
 *        -# PIPE_RESET to ilwalidate the pipe.\n
 *        -# RNG_FAKE_MODE to DISABLED to disable RNG fake mode for fmodels.\n
 *        -# RNG_EN to DISABLED to disable RNG(RANDOM NUMBER GENERATOR).\n
 *        -# SF_FETCH_AS_ZERO to DISABLED.\n
 *        -# SF_FETCH_BPASS to DISABLED.\n
 *        -# SF_PUSH_BYPASS to DISABLED.\n
 */
static void inline stopScpRng(void)
{
    LwU32 regVal;
    /* disable RNG */
    regVal = DRF_DEF(_CPWR_PMU, _SCP_CTL1, _SEQ_CLEAR,        _TASK)     | /* Using _IDLE as HW has not provided any enum to not trigger */
             DRF_DEF(_CPWR_PMU, _SCP_CTL1, _PIPE_RESET,       _TASK)     | /* Using _IDLE as HW has not provided any enum to not trigger */
             DRF_DEF(_CPWR_PMU, _SCP_CTL1, _RNG_FAKE_MODE,    _DISABLED) |
             DRF_DEF(_CPWR_PMU, _SCP_CTL1, _RNG_EN,           _DISABLED) |
             DRF_DEF(_CPWR_PMU, _SCP_CTL1, _SF_FETCH_AS_ZERO, _DISABLED) |
             DRF_DEF(_CPWR_PMU, _SCP_CTL1, _SF_FETCH_BYPASS,  _DISABLED) |
             DRF_DEF(_CPWR_PMU, _SCP_CTL1, _SF_PUSH_BYPASS,   _DISABLED);
    ACR_REG_WR32(CSB, LW_CPWR_PMU_SCP_CTL1, regVal);

    regVal = ACR_REG_RD32(CSB, LW_CPWR_PMU_SCP_CTL_CFG);
    regVal = FLD_SET_DRF(_CPWR_PMU, _SCP_CTL_CFG, _LOCKDOWN_SCP, _ENABLE, regVal);
    ACR_REG_WR32(CSB, LW_CPWR_PMU_SCP_CTL_CFG, regVal);
}

/*!
 * @brief Configure and start random number generator. The function assumes that RNG was stopped previously or that HW
 *        doesn't require doing so.\n
 *        Starting RNG in SCP requires programming bits:\n
 *        -# LW_CPWR_PMU_SCP_RNDCTL0 to program lower threshold that determines the delay (in number of lwclk cycles) of the production
 *           of the first random number.\n
 *        -# LW_CPWR_PMU_SCP_RNDCTL1 to program upper threshold to zero.\n
 *        -# LW_CPWR_PMU_SCP_RNDCTL11_AUTOCAL_ENABLE to enable dnamic calibration behaviour.\n
 *        -# LW_CPWR_PMU_SCP_RNDCTL11_AUTOCAL_MASTERSLAVE to cause AUTOCAL settings for ring A to be used to calibrate both
 *           ring A and ring B to identical ring lengths at all times.\n
 *        -# LW_CPWR_PMU_SCP_RNDCTL11_SNCH_RAND_A/B. These bits bypass the oscillator clocks and use lwclk instead.\n
 *        -# LW_CPWR_PMU_SCP_RNDCTL11_RAND_SAMPLE_SELECT_A/B. These two bit fields select the final random sample variable used by the random
 *           number generator. The choices are OSC, LFSR, and ZERO.\n
 *        -# LW_CPWR_PMU_SCP_RNDCTL11_AUTOCAL_STATIC_TAP_A/B to set the ring length of ring A.\n
 *        -# LW_CPWR_PMU_SCP_RNDCTL11_MIN_AUTO_TAP is a CYA in case any of the faster frequency taps cause unstable behavior in the
 *           autocal circuit, we can limit the autocal to choosing from a slower set of taps.\n
 *           The value of this field refers to tap number -- tap 0 programs the fastest oscillator frequency, and tap 15 programs the slowest.\n 
 *        -# LW_CPWR_PMU_SCP_RNDCTL11_AUTOCAL_HOLDOFF_DELAY is a CYA to cause the autocal circuit to hold off and ignore some number
 *           (0 to 15) of programmable sample periods after any update of the autocal tap value to program lower threshold that determines the delay in 
 *           number of lwclk cycles) of the production.\n
 *        -# LW_CPWR_PMU_SCP_RNDCTL11_AUTOCAL_ASNCH_HOLD_DELAY to program lwclk cycles that signals must be held asserted between calibration logic in the
 *           ring oscillator clock domain and the lwclk domain in order to be sampled correctly and to avoid pulse swallowing.\n
 *        -# LW_CPWR_PMU_SCP_RNDCTL11_AUTOCAL_SAFE_MODE to program autocal circuit in a static manner.This mode is available just in case the analysis
 *           for the dynamic autocal case was in error.\n
 *        -# LW_CPWR_PMU_SCP_CTL1_SEQ_CLEAR to clear the sequencer.\n
 *        -# LW_CPWR_PMU_SCP_CTL1_PIPE_RESET to reset the pipe.\n
 *        -# LW_CPWR_PMU_SCP_CTL1_RNG_FAKE_MODE to DISABLED to disable RNG fake mode for fmodels.\n
 *        -# LW_CPWR_PMU_SCP_CTL1_RNG_EN to DISABLED to disable RNG(RANDOM NUMBER GENERATOR).\n
 *        -# LW_CPWR_PMU_SCP_CTL1_SF_FETCH_AS_ZERO to DISABLED.\n
 *        -# LW_CPWR_PMU_SCP_CTL1_ SF_FETCH_BPASS to DISABLED.\n
 *        -# LW_CPWR_PMU_SCP_CTL1_SF_PUSH_BYPASS to DISABLED.\n
 *        
 */
static void inline startScpRng(void)
{
    // Configure RNG setting. Leave no field implicitly initialized to zero
    ACR_REG_WR32(CSB, LW_CPWR_PMU_SCP_RNDCTL0,  DRF_DEF(_CPWR_PMU, _SCP_RNDCTL0, _HOLDOFF_INIT_LOWER, _ACR_INIT));

    ACR_REG_WR32(CSB, LW_CPWR_PMU_SCP_RNDCTL1,  DRF_DEF(_CPWR_PMU, _SCP_RNDCTL1, _HOLDOFF_INIT_UPPER, _ZERO) |
                                       DRF_DEF(_CPWR_PMU, _SCP_RNDCTL1, _HOLDOFF_INTRA_MASK, _ACR_INIT));

    ACR_REG_WR32(CSB, LW_CPWR_PMU_SCP_RNDCTL11, DRF_DEF(_CPWR_PMU, _SCP_RNDCTL11, _AUTOCAL_ENABLE,            _INIT)   |
                                       DRF_DEF(_CPWR_PMU, _SCP_RNDCTL11, _AUTOCAL_MASTERSLAVE,       _INIT)   |
                                       DRF_DEF(_CPWR_PMU, _SCP_RNDCTL11, _SYNCH_RAND_A,              _INIT)   |
                                       DRF_DEF(_CPWR_PMU, _SCP_RNDCTL11, _SYNCH_RAND_B,              _INIT)   |
                                       DRF_DEF(_CPWR_PMU, _SCP_RNDCTL11, _RAND_SAMPLE_SELECT_A,      _OSC)  |
                                       DRF_DEF(_CPWR_PMU, _SCP_RNDCTL11, _RAND_SAMPLE_SELECT_B,      _LFSR) |
                                       DRF_DEF(_CPWR_PMU, _SCP_RNDCTL11, _AUTOCAL_STATIC_TAP_A,      _SEVEN)|
                                       DRF_DEF(_CPWR_PMU, _SCP_RNDCTL11, _AUTOCAL_STATIC_TAP_B,      _FIVE)|
                                       DRF_NUM(_CPWR_PMU, _SCP_RNDCTL11, _MIN_AUTO_TAP,              0)     |
                                       DRF_NUM(_CPWR_PMU, _SCP_RNDCTL11, _AUTOCAL_HOLDOFF_DELAY,     0)     | /* dkumar: HW init is 1 */
                                       DRF_NUM(_CPWR_PMU, _SCP_RNDCTL11, _AUTOCAL_ASYNCH_HOLD_DELAY, 0)     |
                                       DRF_DEF(_CPWR_PMU, _SCP_RNDCTL11, _AUTOCAL_SAFE_MODE,         _DISABLE));

    // dkumar: Should we be force overriding other SCP RNG related regs under CPWR_SCP*?

    // Enable RNG
    ACR_REG_WR32(CSB, LW_CPWR_PMU_SCP_CTL1,
                                       DRF_DEF(_CPWR_PMU, _SCP_CTL1, _SEQ_CLEAR,        _TASK)     | /* Using _IDLE as HW has not provided any enum to not trigger */
                                       DRF_DEF(_CPWR_PMU, _SCP_CTL1, _PIPE_RESET,       _TASK)     | /* Using _IDLE as HW has not provided any enum to not trigger */
                                       DRF_DEF(_CPWR_PMU, _SCP_CTL1, _RNG_FAKE_MODE,    _DISABLED) |
                                       DRF_DEF(_CPWR_PMU, _SCP_CTL1, _RNG_EN,           _ENABLED)  |
                                       DRF_DEF(_CPWR_PMU, _SCP_CTL1, _SF_FETCH_AS_ZERO, _DISABLED) |
                                       DRF_DEF(_CPWR_PMU, _SCP_CTL1, _SF_FETCH_BYPASS,  _DISABLED) |
                                       DRF_DEF(_CPWR_PMU, _SCP_CTL1, _SF_PUSH_BYPASS,   _DISABLED));

}

/*!
 * @brief Initialize the SCP Random Number Generator\n
 *        Stops SCP RNG and then starts it back. \n
 *        \note
 *        While exiting HS mode, initScpRng must be called on next entry into HS mode since HS should not trust LS/NS.
 */
void  acrInitScpRng_T210(void)
{
    stopScpRng();
    startScpRng();
}

/*!
 * @brief Generates random number using SCP Random Number Generator\n
 *        Generates random number.\n
 *        encrypts it and store back in the input param pointer.
 * @param[out] pRandInOut Ptr to random number to be returned.
 * @return     ACR_OK If random number if successfully returned inside *pRandInOut.
 *        
 */
ACR_STATUS acrGetRandNum_T210(LwU32 *pRandInOut)
{
    if (((LwU32)pRandInOut & (SCP_ADDR_ALIGNMENT_IN_BYTES - (LwU32)1)) != (LwU32)0)
    {
        return ACR_ERROR_SSP_STACK_CHECK_FAILED;
    }
    //waitForRngBufferToBeReady(); // wait for random number buffer to be full before consuming

    falc_scp_trap(TC_INFINITY);

    // Load input (context) into SCP_R5.
    // dkumar: What is the purpose of this? Use this as input to rand generation function? Don't see anyone initializing this.
    falc_trapped_dmwrite(falc_sethi_i((LwU32)pRandInOut, SCP_R5));
    falc_dmwait();

    // Generate two conselwtive random numbers which we will use to create the final random number
    falc_scp_rand(SCP_R4);
    // dkumar: Should there be a waitForRngBufferToBeReady() here before next falc_scp_rand?
    falc_scp_rand(SCP_R3);

    // mix previous whitened rand data. SCP_R4 = SCP_R4 ^ SCP_R5. SCP_R5 is the initial context
    falc_scp_xor(SCP_R5, SCP_R4);

    // Use AES encryption to whiten random data. SCP_R3 = AES-128-ECB(SCP_R4(key), SCP_R3(data))
    falc_scp_key(SCP_R4);
    falc_scp_encrypt(SCP_R3, SCP_R3);

    // Copy the result of encryption above back into the desired output location
    falc_trapped_dmread(falc_sethi_i((LwU32)pRandInOut, SCP_R3));
    falc_dmwait();

    // Scrub all SCP registers used in current function since random number is expected to be secret
    falc_scp_xor(SCP_R3, SCP_R3);
    falc_scp_xor(SCP_R4, SCP_R4);
    falc_scp_xor(SCP_R5, SCP_R5);

    falc_scp_trap(0);

    return ACR_OK;
}
#endif
/*!
 *  @brief Security Feature to prevent NS restart from HS.
 *         It disables CPUCTL ALIAS_EN register as soon as it enters HS code so that nobody can restart the falcon from NS once it halts in HS.
 *         If not done, adversary can modify bootvec to desired location and execute selective part in HS.
 *
 */
void
acrMitigateNsRestartFromHs_T210(void)
{
	LwU32 cpuctlAliasEn;

	cpuctlAliasEn = ACR_REG_RD32(CSB, LW_CPWR_FALCON_CPUCTL);
	cpuctlAliasEn = FLD_SET_DRF(_CPWR, _FALCON_CPUCTL, _ALIAS_EN, _FALSE, cpuctlAliasEn);
	ACR_REG_WR32(CSB, LW_CPWR_FALCON_CPUCTL, cpuctlAliasEn);
	// We read-back the value of CPUCTL until ALIAS is set to FALSE.
	cpuctlAliasEn = ACR_REG_RD32(CSB, LW_CPWR_FALCON_CPUCTL);

	while (!FLD_TEST_DRF(_CPWR, _FALCON_CPUCTL, _ALIAS_EN, _FALSE, cpuctlAliasEn))
	{
		cpuctlAliasEn = ACR_REG_RD32(CSB, LW_CPWR_FALCON_CPUCTL);
	}

}
