/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: booter_static_inline_functions.h
 */

#ifndef _BOOTER_STATIC_INLINE_FUNCTIONS_H_
#define _BOOTER_STATIC_INLINE_FUNCTIONS_H_

/*
 * @brief  Reads CSB registers with limited Pri Err handling to support inlined functions with no falc_halt.
 *         Do not use in case of PTIMER/TRNG as this does not handle cases of 0xbadf which are valid cases for
 *         these registers.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
LwU32
_booterReadCsbInlineWithPriErrHandling(LwU32 addr)
{
    LwU32 value, csbErrStatVal;

    value = falc_ldxb_i((LwU32*) (addr), 0);

#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    csbErrStatVal = falc_ldxb_i ((LwU32*)(LW_CSEC_FALCON_CSBERRSTAT), 0);
#elif defined(BOOTER_RELOAD)
    csbErrStatVal = falc_ldxb_i ((LwU32*)(LW_CLWDEC_FALCON_CSBERRSTAT), 0);
#else
    ct_assert(0);
#endif

    if (FLD_TEST_DRF(_PFALCON, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatVal))
    {
        // If CSBERRSTAT is true, halt no matter what register was read.
        // coverity[no_escape]
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
static inline
void
_booterWriteCsbInlineWithPriErrHandling(LwU32 addr, LwU32 data)
{
    LwU32 csbErrStatVal;

    falc_stxb_i((LwU32*) (addr), 0, data);

#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    csbErrStatVal = falc_ldxb_i ((LwU32*)(LW_CSEC_FALCON_CSBERRSTAT), 0);
#elif defined(BOOTER_RELOAD)
    csbErrStatVal = falc_ldxb_i ((LwU32*)(LW_CLWDEC_FALCON_CSBERRSTAT), 0);
#else
    ct_assert(0);
#endif

    if (FLD_TEST_DRF(_PFALCON, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatVal))
    {
        // If CSBERRSTAT is true, halt no matter what register was read.
        // coverity[no_escape]
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
booterMitigateNSRestartFromHS_TU10X(void)
{
    LwU32 cpuctlAliasEn = 0;

#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    cpuctlAliasEn = BOOTER_CSB_REG_RD32_INLINE(LW_CSEC_FALCON_CPUCTL);
    cpuctlAliasEn = FLD_SET_DRF(_CSEC, _FALCON_CPUCTL, _ALIAS_EN, _FALSE, cpuctlAliasEn);
    BOOTER_CSB_REG_WR32_INLINE(LW_CSEC_FALCON_CPUCTL, cpuctlAliasEn);
 
    // We read-back the value of CPUCTL until ALIAS_EN is set to FALSE. 
    cpuctlAliasEn = BOOTER_CSB_REG_RD32_INLINE(LW_CSEC_FALCON_CPUCTL);
    while(!FLD_TEST_DRF(_CSEC, _FALCON_CPUCTL, _ALIAS_EN, _FALSE, cpuctlAliasEn))
    {
        cpuctlAliasEn = BOOTER_CSB_REG_RD32_INLINE(LW_CSEC_FALCON_CPUCTL);
    }
#elif defined(BOOTER_RELOAD)
    cpuctlAliasEn = BOOTER_CSB_REG_RD32_INLINE(LW_CLWDEC_FALCON_CPUCTL);
    cpuctlAliasEn = FLD_SET_DRF(_CLWDEC, _FALCON_CPUCTL, _ALIAS_EN, _FALSE, cpuctlAliasEn);
    BOOTER_CSB_REG_WR32_INLINE(LW_CLWDEC_FALCON_CPUCTL, cpuctlAliasEn);
 
    // We read-back the value of CPUCTL until ALIAS_EN is set to FALSE. 
    cpuctlAliasEn = BOOTER_CSB_REG_RD32_INLINE(LW_CLWDEC_FALCON_CPUCTL);
    while(!FLD_TEST_DRF(_CLWDEC, _FALCON_CPUCTL, _ALIAS_EN, _FALSE, cpuctlAliasEn))
    {
        cpuctlAliasEn = BOOTER_CSB_REG_RD32_INLINE(LW_CLWDEC_FALCON_CPUCTL);
    }
#else
    ct_assert(0);
#endif

}

#ifndef BOOT_FROM_HS_BUILD
//
// Below functions to generate random number from SCP engine are copied from
// "//sw/main/bios/core90/core/pmu/scp_rand.c".
//

/*!
 * @brief   Sets up SCP TRNG and starts the random number generator.
 *          We use direct falc instructions instead of lwstomary RD/WR macros
 *          since the later are not inlined and we require code to be inlined 
 *          here since SSP is not setup.
 */
GCC_ATTRIB_SECTION("imem_booter", "_scpStartTrng")
GCC_ATTRIB_ALWAYSINLINE()
static inline
void _scpStartTrng(void)
{
    LwU32      val = 0;

    //
    // Set LW_CSEC_SCP_RNDCTL0_HOLDOFF_INIT_LOWER.
    // This value determines the delay (in number of lwclk cycles) of the production
    // of the first random number. that positive transitions of CTL1.RNG_EN or
    // RNDCTL3.TRIG_LFSR_RELOAD_* cause this delay to be enforced.)
    // LW_CSEC_SCP_RNDCTL0 has only 1 field(LW_CSEC_SCP_RNDCTL0_HOLDOFF_INIT_LOWER) which is
    // 32 bits long.
    //
 
#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)

    // HOLDOFF_INIT_LOWER
    val = BOOTER_CSB_REG_RD32_INLINE(LW_CSEC_SCP_RNDCTL0);
    val = FLD_SET_DRF_NUM( _CSEC, _SCP_RNDCTL0, _HOLDOFF_INIT_LOWER, SCP_HOLDOFF_INIT_LOWER_VAL, val); 
    BOOTER_CSB_REG_WR32_INLINE(LW_CSEC_SCP_RNDCTL0, val);

    // HOLDOFF_INTRA_MASK
    val = BOOTER_CSB_REG_RD32_INLINE(LW_CSEC_SCP_RNDCTL1);
    val = FLD_SET_DRF_NUM(_CSEC, _SCP_RNDCTL1, _HOLDOFF_INTRA_MASK, SCP_HOLDOFF_INTRA_MASK, val);
    BOOTER_CSB_REG_WR32_INLINE(LW_CSEC_SCP_RNDCTL1, val);

    // Set the ring length of ring A/B
    val = BOOTER_CSB_REG_RD32_INLINE(LW_CSEC_SCP_RNDCTL11);
    val = FLD_SET_DRF_NUM(_CSEC, _SCP_RNDCTL11, _AUTOCAL_STATIC_TAP_A, SCP_AUTOCAL_STATIC_TAPA, val);
    val = FLD_SET_DRF_NUM(_CSEC, _SCP_RNDCTL11, _AUTOCAL_STATIC_TAP_B, SCP_AUTOCAL_STATIC_TAPB, val);
    BOOTER_CSB_REG_WR32_INLINE(LW_CSEC_SCP_RNDCTL11, val);

    // Enable SCP TRNG
    val = BOOTER_CSB_REG_RD32_INLINE(LW_CSEC_SCP_CTL1);
    val = FLD_SET_DRF(_CSEC, _SCP_CTL1_RNG, _EN, _ENABLED, val);
    BOOTER_CSB_REG_WR32_INLINE(LW_CSEC_SCP_CTL1, val);

#elif defined(BOOTER_RELOAD)
    val = BOOTER_CSB_REG_RD32_INLINE(LW_CLWDEC_SCP_RNDCTL0);
    val = FLD_SET_DRF_NUM( _CLWDEC, _SCP_RNDCTL0, _HOLDOFF_INIT_LOWER, SCP_HOLDOFF_INIT_LOWER_VAL, val);
    BOOTER_CSB_REG_WR32_INLINE(LW_CLWDEC_SCP_RNDCTL0, val);

    // HOLDOFF_INTRA_MASK
    val = BOOTER_CSB_REG_RD32_INLINE(LW_CLWDEC_SCP_RNDCTL1);
    val = FLD_SET_DRF_NUM(_CLWDEC, _SCP_RNDCTL1, _HOLDOFF_INTRA_MASK, SCP_HOLDOFF_INTRA_MASK, val);
    BOOTER_CSB_REG_WR32_INLINE(LW_CLWDEC_SCP_RNDCTL1, val);

    // Set the ring length of ring A/B
    val = BOOTER_CSB_REG_RD32_INLINE(LW_CLWDEC_SCP_RNDCTL11);
    val = FLD_SET_DRF_NUM(_CLWDEC, _SCP_RNDCTL11, _AUTOCAL_STATIC_TAP_A, SCP_AUTOCAL_STATIC_TAPA, val);
    val = FLD_SET_DRF_NUM(_CLWDEC, _SCP_RNDCTL11, _AUTOCAL_STATIC_TAP_B, SCP_AUTOCAL_STATIC_TAPB, val);
    BOOTER_CSB_REG_WR32_INLINE(LW_CLWDEC_SCP_RNDCTL11, val);

    // Enable SCP TRNG
    val = BOOTER_CSB_REG_RD32_INLINE(LW_CLWDEC_SCP_CTL1);
    val = FLD_SET_DRF(_CLWDEC, _SCP_CTL1_RNG, _EN, _ENABLED, val);
    BOOTER_CSB_REG_WR32_INLINE(LW_CLWDEC_SCP_CTL1, val);
#else
    ct_assert(0);
#endif

}

/*!
 * @brief   Stops SCP TRNG
 *          We use direct falc instructions instead of lwstomary RD/WR macros
 *          since the later are not inlined and we require code to be inlined 
 *          here since SSP is not setup.
 */
GCC_ATTRIB_SECTION("imem_booter", "_scpStopTrng")
GCC_ATTRIB_ALWAYSINLINE()
static inline
void _scpStopTrng(void)
{
    LwU32      val       = 0;

#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    val = BOOTER_CSB_REG_RD32_INLINE(LW_CSEC_SCP_CTL1);
    val = FLD_SET_DRF(_CSEC, _SCP_CTL1_RNG, _EN, _DISABLED, val);
    BOOTER_CSB_REG_WR32_INLINE(LW_CSEC_SCP_CTL1, val);
#elif defined(BOOTER_RELOAD)
    val = BOOTER_CSB_REG_RD32_INLINE(LW_CLWDEC_SCP_CTL1);
    val = FLD_SET_DRF(_CLWDEC, _SCP_CTL1_RNG, _EN, _DISABLED, val);
    BOOTER_CSB_REG_WR32_INLINE(LW_CLWDEC_SCP_CTL1, val);
#else
    ct_assert(0);
#endif

}

static LwU32 _randNum16byte[4] GCC_ATTRIB_ALIGN(16) = { 0 };

/*!
 * @brief   Gets 32 bit random number from SCP.
 *
 * @param[in,out] pRandomNum  Pointer to random number.
 * @return        BOOTER_STATUS
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
BOOTER_STATUS booterScpGetRandomNumber_TU10X(LwU32 *pRand32)
{
    BOOTER_STATUS status = BOOTER_OK;
    LwU32      i      = 0;

    if (pRand32 == NULL)
    {
       return BOOTER_ERROR_ILWALID_ARGUMENT;      
    }

    _scpStartTrng();
    // load _randNum16byte to SCP
    falc_scp_trap(TC_INFINITY);
    falc_trapped_dmwrite(falc_sethi_i((LwU32)_randNum16byte, SCP_R5));

    //
    // improve RNG quality by collecting entropy across
    // two conselwtive random numbers
    //
    falc_scp_rand(SCP_R4);
    falc_scp_rand(SCP_R3);

    // trapped dmwait
    falc_dmwait();
    // mixing in previous whitened rand data
    falc_scp_xor(SCP_R5, SCP_R4);

    // use AES cipher to whiten random data
    falc_scp_key(SCP_R4);
    falc_scp_encrypt(SCP_R3, SCP_R3);

    // retrieve random data and update DMEM buffer.
    falc_trapped_dmread(falc_sethi_i((LwU32)_randNum16byte, SCP_R3));
    falc_dmwait();

    // Reset trap to 0
    falc_scp_trap(0);

    //
    // _randNum16byte has a 128 bit random number generated but we are interested 
    // in only a non-zero 32 bit random num
    //
 
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

     _scpStopTrng();

    return status; 
}

#endif //BOOT_FROM_HS_BUILD

/*
 *@brief: Reset SCP's 2 instruction sequencers and pipe to put SCP in a more deterministic state
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
void
booterResetScpLoopTracesAndPipe(void)
{
    LwU32 scpCtl1 = 0;
    LwU32 timer = 0;

#if defined(BOOTER_LOAD) || defined(BOOTER_UNLOAD)
    scpCtl1 = BOOTER_CSB_REG_RD32_INLINE(LW_CSEC_SCP_CTL1);

    // Initialize CTL1 register and trigger a pipe and both scp loop traces reset 
    scpCtl1 = FLD_SET_DRF(_CSEC, _SCP_CTL1, _PIPE_RESET, _TASK, scpCtl1);
    scpCtl1 = FLD_SET_DRF(_CSEC, _SCP_CTL1, _SEQ_CLEAR, _TASK, scpCtl1);

    BOOTER_CSB_REG_WR32_INLINE(LW_CSEC_SCP_CTL1, scpCtl1);

    // Poll PIPE_RESET until it clears
    while (FLD_TEST_DRF(_CSEC, _SCP_CTL1, _PIPE_RESET, _TASK,
                        BOOTER_CSB_REG_RD32_INLINE(LW_CSEC_SCP_CTL1)))
#elif defined(BOOTER_RELOAD)
    scpCtl1 = BOOTER_CSB_REG_RD32_INLINE(LW_CLWDEC_SCP_CTL1);

    // Initialize CTL1 register and trigger a pipe and both scp loop traces reset 
    scpCtl1 = FLD_SET_DRF(_CLWDEC, _SCP_CTL1, _PIPE_RESET, _TASK, scpCtl1);
    scpCtl1 = FLD_SET_DRF(_CLWDEC, _SCP_CTL1, _SEQ_CLEAR, _TASK, scpCtl1);

    BOOTER_CSB_REG_WR32_INLINE(LW_CLWDEC_SCP_CTL1, scpCtl1);

    // Poll PIPE_RESET until it clears
    while (FLD_TEST_DRF(_CLWDEC, _SCP_CTL1, _PIPE_RESET, _TASK,
                        BOOTER_CSB_REG_RD32_INLINE(LW_CLWDEC_SCP_CTL1)))
#endif
    {
        timer++;
        if (timer >= BOOTER_TIMEOUT_FOR_PIPE_RESET)
        {
           // In case PIPE_RESET does not clear before BOOTER_DEFAULT_TIMEOUT_NS
           // coverity[no_escape]
           INFINITE_LOOP();
        }
    }
}


/* 
 *  @brief Function to callwlate LwU32 multiple operation.
 *
 *  @param[in]  a, b  The multiplicand and multiplier
 *
 *  @return LwU64 The product to return.
 */
GCC_ATTRIB_ALWAYSINLINE() 
static inline
LwU64 booterMultipleUnsigned32(LwU32 a, LwU32 b)
{
    LwU16 a1, a2;
    LwU16 b1, b2;
    LwU32 a2b2, a1b2, a2b1, a1b1;
    LwU64 result;

    // 
    // Falcon has a 16-bit multiplication instruction.
    // Break down the 32-bit multiplication into 16-bit operations.
    // 
    a1 = a >> 16;
    a2 = a & 0xffff;

    b1 = b >> 16;
    b2 = b & 0xffff;

    a2b2 = a2 * b2;
    a1b2 = a1 * b2;
    a2b1 = a2 * b1;
    a1b1 = a1 * b1;

    result = (LwU64)a2b2 + ((LwU64)a1b2 << 16) +
            ((LwU64)a2b1 << 16) + ((LwU64)a1b1 << 32);
    return result;
}

#endif //_BOOTER_STATIC_INLINE_FUNCTIONS_H_
