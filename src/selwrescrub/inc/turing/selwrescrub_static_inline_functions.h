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
 * @file: selwrescrub_static_inline_funcitons
 */

#ifndef _SELWRESCRUB_STATIC_INLINE_FUNCTIONS_H_
#define _SELWRESCRUB_STATIC_INLINE_FUNCTIONS_H_
//#include <lwuproc.h>
#include "dev_falcon_v4.h"
#include "dev_lwdec_csb.h"
#include "selwrescrubreg.h"
#include "selwrescrubutils.h"

/*
 * @brief  Reads CSB registers with limited Pri Err handling to support inlined functions with no falc_halt.
 *         Do not use in case of PTIMER/TRNG as this does not handle cases of 0xbadf which are valid cases for
 *         these registers.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwU32 _selwrescrubReadCsbInlineWithPriErrHandling(LwU32 addr)
{
    LwU32 value, csbErrStatValRd;

    value = falc_ldxb_i((LwU32*) (addr), 0);

    csbErrStatValRd = falc_ldxb_i ((LwU32*)(LW_CLWDEC_FALCON_CSBERRSTAT), 0);

    if (FLD_TEST_DRF(_PFALCON, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatValRd))
    {
        // If CSBERRSTAT is true, halt no matter what register was read.
        INFINITE_LOOP();
    }

    return value;
}

/*
 * @brief  Writes to CSB registers with limited Pri Err handling to support inlined functions with no falc_halt.
 *         Do not use in case of PTIMER/TRNG as this does not handle cases of 0xbadf which are valid cases for
 *         these registers.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void _selwrescrubWriteCsbInlineWithPriErrHandling(LwU32 addr, LwU32 data)
{
    LwU32 csbErrStatValWr;

    falc_stxb_i((LwU32*) (addr), 0, data);

    csbErrStatValWr = falc_ldxb_i ((LwU32*)(LW_CLWDEC_FALCON_CSBERRSTAT), 0);

    if (FLD_TEST_DRF(_PFALCON, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatValWr))
    {
        // If CSBERRSTAT is true, halt no matter what register was read.
        INFINITE_LOOP();
    }
}

/*!
* @brief: To mitigate the risk of a  NS restart from HS mode we follow #19 from
* https://wiki.lwpu.com/gpuhwmaxwell/index.php/MaxwellSW/Resman/Security#Guidelines_for_HS_binary.
* Please refer bug 2534981 for more details.
* We use direct falc instructions instead of lwstomary RD/WR macros since the later are not inlined
* and we require code to be inlined here since SSP is not setup.
*/
GCC_ATTRIB_ALWAYSINLINE()
static inline
void
selwrescrubMitigateNSRestartFromHS_GV100(void)
{
    LwU32 cpuctlAliasEn = 0;

    cpuctlAliasEn = FALC_CSB_REG_RD32_INLINE(LW_CLWDEC_FALCON_CPUCTL);
    cpuctlAliasEn = FLD_SET_DRF(_CLWDEC, _FALCON_CPUCTL, _ALIAS_EN, _FALSE, cpuctlAliasEn);
    FALC_CSB_REG_WR32_INLINE(LW_CLWDEC_FALCON_CPUCTL, cpuctlAliasEn);

    // We read-back the value of CPUCTL until ALIAS_EN  is set to FALSE.
    cpuctlAliasEn = FALC_CSB_REG_RD32_INLINE(LW_CLWDEC_FALCON_CPUCTL);
    while(!FLD_TEST_DRF(_CLWDEC, _FALCON_CPUCTL, _ALIAS_EN, _FALSE, cpuctlAliasEn))
    {
        cpuctlAliasEn = FALC_CSB_REG_RD32_INLINE(LW_CLWDEC_FALCON_CPUCTL);
    }

}

// Default Timeout Value
#define SELWRESCRUB_TIMEOUT_FOR_PIPE_RESET          (0x400)

/*
 *@brief: Reset SCP's both instuction sequencers and pipe to put SCP in a more deterministic state
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
void
selwrescrubResetScpLoopTracesAndPipe_TU10X(void)
{

    LwU32 scpCtl1 = 0;
    LwU32 timer   = 0;


    scpCtl1 = FALC_CSB_REG_RD32_INLINE(LW_CLWDEC_SCP_CTL1);

    // Initialize CTL1 register and trigger a pipe reset and Seq Clear 
    scpCtl1 = FLD_SET_DRF(_CLWDEC, _SCP_CTL1, _PIPE_RESET, _TASK, scpCtl1);
    scpCtl1 = FLD_SET_DRF(_CLWDEC, _SCP_CTL1, _SEQ_CLEAR, _TASK, scpCtl1);

    FALC_CSB_REG_WR32_INLINE(LW_CLWDEC_SCP_CTL1, scpCtl1);

    // Poll PIPE_RESET until it clears
    while (FLD_TEST_DRF(_CLWDEC, _SCP_CTL1, _PIPE_RESET, _TASK,
                        FALC_CSB_REG_RD32_INLINE(LW_CLWDEC_SCP_CTL1)))
    {
        timer++;
        if (timer >= SELWRESCRUB_TIMEOUT_FOR_PIPE_RESET)
        {
           // In case PIPE_RESET does not clear before SELWRESCRUB_DEFAULT_TIMEOUT_NS
           INFINITE_LOOP();
        }

    }
}

static LwU32 _randNum16byte[4] GCC_ATTRIB_ALIGN(16) = { 0 };

//
// Below functions to generate random number from SCP engine are copied from
// "//sw/main/bios/core90/core/pmu/scp_rand.c".
//

/*!
 * @brief   Sets up SCP TRNG and starts the random number generator.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
SELWRESCRUB_RC selwrescrubScpStartTrng(void)
{
    LwU32          val;
    SELWRESCRUB_RC status = SELWRESCRUB_RC_OK;

    //
    // Set LW_CLWDEC_SCP_RNDCTL0_HOLDOFF_INIT_LOWER.
    // This value determines the delay (in number of lwclk cycles) of the production
    // of the first random number. that positive transitions of CTL1.RNG_EN or
    // RNDCTL3.TRIG_LFSR_RELOAD_* cause this delay to be enforced.)
    // LW_CLWDEC_SCP_RNDCTL0 has only 1 field(LW_CLWDEC_SCP_RNDCTL0_HOLDOFF_INIT_LOWER) which is
    // 32 bits long.
    //
    val = FALC_CSB_REG_RD32_INLINE( LW_CLWDEC_SCP_RNDCTL0 );
    val = FLD_SET_DRF_NUM( _CLWDEC, _SCP_RNDCTL0, _HOLDOFF_INIT_LOWER, SCP_HOLDOFF_INIT_LOWER_VAL, val );
    FALC_CSB_REG_WR32_INLINE( LW_CLWDEC_SCP_RNDCTL0, val );

    val = FALC_CSB_REG_RD32_INLINE( LW_CLWDEC_SCP_RNDCTL1);
    val = FLD_SET_DRF_NUM( _CLWDEC, _SCP_RNDCTL1, _HOLDOFF_INTRA_MASK, SCP_HOLDOFF_INTRA_MASK, val );
    FALC_CSB_REG_WR32_INLINE( LW_CLWDEC_SCP_RNDCTL1, val );

    // Set the ring length of ring A/B
    val = FALC_CSB_REG_RD32_INLINE( LW_CLWDEC_SCP_RNDCTL11 );
    val = FLD_SET_DRF_NUM( _CLWDEC, _SCP_RNDCTL11, _AUTOCAL_STATIC_TAP_A, SCP_AUTOCAL_STATIC_TAPA, val );
    val = FLD_SET_DRF_NUM( _CLWDEC, _SCP_RNDCTL11, _AUTOCAL_STATIC_TAP_B, SCP_AUTOCAL_STATIC_TAPB, val );
    FALC_CSB_REG_WR32_INLINE( LW_CLWDEC_SCP_RNDCTL11, val );

    // Enable SCP TRNG
    val = FALC_CSB_REG_RD32_INLINE( LW_CLWDEC_SCP_CTL1 );
    val = FLD_SET_DRF( _CLWDEC, _SCP_CTL1_RNG, _EN, _ENABLED, val );
    FALC_CSB_REG_WR32_INLINE( LW_CLWDEC_SCP_CTL1, val );

    return status;
}

/*!
 * @brief   Stops SCP TRNG
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
SELWRESCRUB_RC selwrescrubScpStopTrng(void)
{
    LwU32          val    = 0;
    SELWRESCRUB_RC status = SELWRESCRUB_RC_OK;

    val = FALC_CSB_REG_RD32_INLINE( LW_CLWDEC_SCP_CTL1 );
    val = FLD_SET_DRF( _CLWDEC, _SCP_CTL1_RNG, _EN, _DISABLED, val );
    FALC_CSB_REG_WR32_INLINE( LW_CLWDEC_SCP_CTL1, val );

    return status;
}

/*!
 * @brief   Gets 128 bit random number from SCP.
 *
 * @param[in,out] pRandomNum     Pointer to random number.
 * @return        SELWRESCRUB_RC
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
SELWRESCRUB_RC selwrescrubScpGetRandomNumber_TU10X(LwU32 *pRand32)
{
    SELWRESCRUB_RC status = SELWRESCRUB_RC_OK;
    LwU32          i      = 0;

    if (pRand32 == NULL)
    {
        CHECK_STATUS_AND_GOTO_EXIT_IF_NOT_OK(SELWRESCRUB_RC_ILWALID_ARGUMENT);
    }

    CHECK_STATUS_AND_GOTO_EXIT_IF_NOT_OK(selwrescrubScpStartTrng());

    // load _randNum16byte to SCP
    falc_scp_trap( TC_INFINITY );
    falc_trapped_dmwrite( falc_sethi_i( (LwU32)_randNum16byte, SCP_R5) );

    //
    // improve RNG quality by collecting entropy across
    // two conselwtive random numbers
    //
    falc_scp_rand( SCP_R4 );
    falc_scp_rand( SCP_R3 );

    // trapped dmwait
    falc_dmwait();
    // mixing in previous whitened rand data
    falc_scp_xor( SCP_R5, SCP_R4 );

    // use AES cipher to whiten random data
    falc_scp_key( SCP_R4 );
    falc_scp_encrypt( SCP_R3, SCP_R3 );

    // retrieve random data and update DMEM buffer.
    falc_trapped_dmread( falc_sethi_i( (LwU32)_randNum16byte, SCP_R3 ) );
    falc_dmwait();

    // Reset trap to 0
    falc_scp_trap(0);
    // Make sure value of non-zero number is returned
    for (i = 0; i < 4; i++)
    {
        if (_randNum16byte[i] != 0)
        {
            *pRand32 = (LwU32)_randNum16byte[i];
            break;
        }
    }

    // Scrub TRNG generated
    falc_scp_xor(SCP_R3, SCP_R3);
    falc_scp_xor(SCP_R4, SCP_R4);
    _randNum16byte[0] = 0;
    _randNum16byte[1] = 0;
    _randNum16byte[2] = 0;
    _randNum16byte[3] = 0;

    CHECK_STATUS_AND_GOTO_EXIT_IF_NOT_OK(selwrescrubScpStopTrng());

Exit:
    return status;
}

#endif
