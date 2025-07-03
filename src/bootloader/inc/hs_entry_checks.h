/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: hs_entry_checks.h
 */

#ifndef HS_ENTRY_CHECKS_H
#define HS_ENTRY_CHECKS_H

#include "lwctassert.h"

// SEC2
#if BOOT_FROM_HS_FALCON_SEC2
#include "dev_sec_csb.h"
#include "dev_sec_csb_addendum.h"
#define HS_FALC_REG(reg)    LW_CSEC_##reg
#define HS_FALC             _CSEC
#endif

// GSP
#if BOOT_FROM_HS_FALCON_GSP
#include "dev_gsp_csb.h"
#include "dev_gsp_csb_addendum.h"
#define HS_FALC_REG(reg)    LW_CGSP_##reg
#define HS_FALC             _CGSP
#endif

// LWDEC
#if BOOT_FROM_HS_FALCON_LWDEC
#include "dev_lwdec_csb.h"
#include "dev_lwdec_csb_addendum.h"
#define HS_FALC_REG(reg)    LW_CLWDEC_##reg
#define HS_FALC             _CLWDEC
#endif

// Infinite loop handler
#define INFINITE_LOOP()  while(LW_TRUE)

#define FLCN_DMEM_SIZE_IN_BLK(falc)     (LW##falc##_FLCN_DMEM_SIZE_IN_BLK)
#define FLCN_DMEM_BLK_ALIGN_BITS(falc)  (LW##falc##_FLCN_DMEM_BLK_ALIGN_BITS)

//
// Set the stack size for HS FMC.
// Lwrrently allocated stack size is 0x4000 starting from DMEM end address.
// DMEM end address is used from chip specific headers rather than relying on 
// __stack linker variable.
// This is because __stack is defined as "PROVIDE (__stack = LENGTH(DATA));"
// LENGTH(DATA) is defined in linker script itself which is common to all profiles, 
// not chip specific and it may not point to actual end of DMEM.
//
#define FALCON_DMEM_SIZE(falc)          (FLCN_DMEM_SIZE_IN_BLK(falc) << FLCN_DMEM_BLK_ALIGN_BITS(falc))
#define HS_STACK_SIZE                   (0x4000)
#define HS_STACK_BOTTOM_LIMIT(falc)     ((FALCON_DMEM_SIZE(falc)) - (HS_STACK_SIZE))

#define HS_TIMEOUT_FOR_PIPE_RESET   (0x400)

#define ATTR_OVLY(ov)               __attribute__ ((section(ov)))
#define ATTR_ALIGNED(align)         __attribute__ ((aligned(align)))

#define LW_FALC_SEC_SPR_DISABLE_EXCEPTIONS          19:19
#define LW_FALC_SEC_SPR_DISABLE_EXCEPTIONS_CLEAR    0x00000000
#define LW_FALC_SEC_SPR_DISABLE_EXCEPTIONS_SET      0x00000001

#define HS_CSB_REG_RD32_INLINE(addr)               hsReadCsbInlineWithPriErrHandling(HS_FALC_REG(addr))
#define HS_CSB_REG_WR32_INLINE(addr, data)         hsWriteCsbInlineWithPriErrHandling(HS_FALC_REG(addr), data)

#define HS_FLD_SET_DRF(d,r,f,c,v)       FLD_SET_DRF(d,r,f,c,v)
#define HS_FLD_TEST_DRF(d,r,f,c,v)      FLD_TEST_DRF(d,r,f,c,v)
#define HS_FLD_SET_DRF_NUM(d,r,f,n,v)   FLD_SET_DRF_NUM(d,r,f,n,v)
#define HS_DRF_VAL(d,r,f,v)             DRF_VAL(d,r,f,v)
#define HS_DRF_DEF(d,r,f,c)             DRF_DEF(d,r,f,c)

#ifndef STEADY_STATE_BUILD
static LwU32 _randNum16byte[4] GCC_ATTRIB_ALIGN(16) = { 0 };
// variable to store canary for SSP
void * __stack_chk_guard;
#endif // !STEADY_STATE_BUILD

// 
// Linker Script Variable to identify start of bss
// This needs to be defined in *.ld file of each binary
//
extern LwU32 _bss_start[]                  ATTR_OVLY(".data");

extern void hsExceptionHandlerSelwreHang(void);

// Clear all SCP registers
GCC_ATTRIB_ALWAYSINLINE()
static inline
void
hsClearSCPRegisters(void)
{
    // First load secret index 0, then Clear all SCP registers
    falc_scp_secret(0, SCP_R0);
    falc_scp_xor(SCP_R0, SCP_R0);

    falc_scp_secret(0, SCP_R1);
    falc_scp_xor(SCP_R1, SCP_R1);

    falc_scp_secret(0, SCP_R2);
    falc_scp_xor(SCP_R2, SCP_R2);

    falc_scp_secret(0, SCP_R3);
    falc_scp_xor(SCP_R3, SCP_R3);

    falc_scp_secret(0, SCP_R4);
    falc_scp_xor(SCP_R4, SCP_R4);

    falc_scp_secret(0, SCP_R5);
    falc_scp_xor(SCP_R5, SCP_R5);

    falc_scp_secret(0, SCP_R6);
    falc_scp_xor(SCP_R6, SCP_R6);

    falc_scp_secret(0, SCP_R7);
    falc_scp_xor(SCP_R7, SCP_R7);    
}

// Clear all falcon GPRs
GCC_ATTRIB_ALWAYSINLINE()
static inline
void
hsClearFalconGprs(void)
{
    // Clearing all falcon GPRs
    asm volatile
    (
        "clr.w a0;"
        "clr.w a1;"
        "clr.w a2;"
        "clr.w a3;"
        "clr.w a4;"
        "clr.w a5;"
        "clr.w a6;"
        "clr.w a7;"
        "clr.w a8;"
        "clr.w a9;"
        "clr.w a10;"
        "clr.w a11;"
        "clr.w a12;"
        "clr.w a13;"
        "clr.w a14;"
        "clr.w a15;"
    );
}

/*
 * @brief  Reads CSB registers with limited Pri Err handling to support inlined functions with no falc_halt.
 *         Do not use in case of PTIMER/TRNG as this does not handle cases of 0xbadf which are valid cases for
 *         these registers.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
LwU32
hsReadCsbInlineWithPriErrHandling(LwU32 addr)
{
    LwU32 value, csbErrStatVal;

    value = falc_ldxb_i((LwU32*) (addr), 0);

    csbErrStatVal = falc_ldxb_i ((LwU32*)(HS_FALC_REG(FALCON_CSBERRSTAT)), 0);

    if (HS_FLD_TEST_DRF(HS_FALC, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatVal))
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
static inline
void
hsWriteCsbInlineWithPriErrHandling(LwU32 addr, LwU32 data)
{
    LwU32 csbErrStatVal;

    falc_stxb_i((LwU32*) (addr), 0, data);

    csbErrStatVal = falc_ldxb_i ((LwU32*)(HS_FALC_REG(FALCON_CSBERRSTAT)), 0);

    if (HS_FLD_TEST_DRF(HS_FALC, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatVal))
    {
        // If CSBERRSTAT is true, halt no matter what register was read.
        INFINITE_LOOP();
    }
}



/*!
 * Check if CSBERRSTAT_ENABLE bit is TRUE, else set CSBERRSTAT_ENABLE to TRUE
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
void hsValidateCsberrstat(void)
{
    LwU32 csbErrStatVal;
    csbErrStatVal = HS_CSB_REG_RD32_INLINE(FALCON_CSBERRSTAT);
    csbErrStatVal = HS_FLD_SET_DRF(HS_FALC, _FALCON_CSBERRSTAT, _ENABLE, _TRUE, csbErrStatVal);
    HS_CSB_REG_WR32_INLINE(FALCON_CSBERRSTAT, csbErrStatVal);
}


/*!
 * Function to scrub unused DMEM from startAddr to endAddr
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
void
hsScrubDmem(LwU32 startAddr, LwU32 endAddr)
{
    LwU32 *pDmem = NULL;

    // Scrub DMEM from startAddr to endAddr 
    for (pDmem = (LwU32*)startAddr;
         pDmem < (LwU32*)endAddr;
         pDmem++)
    {
        *pDmem = 0x0;
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
hsMitigateNSRestartFromHS(void)
{
    LwU32 cpuctlAliasEn = 0;

    cpuctlAliasEn = HS_CSB_REG_RD32_INLINE(FALCON_CPUCTL);
    cpuctlAliasEn = HS_FLD_SET_DRF(HS_FALC, _FALCON_CPUCTL, _ALIAS_EN, _FALSE, cpuctlAliasEn);
    HS_CSB_REG_WR32_INLINE(FALCON_CPUCTL, cpuctlAliasEn);
 
    // We read-back the value of CPUCTL until ALIAS_EN is set to FALSE. 
    cpuctlAliasEn = HS_CSB_REG_RD32_INLINE(FALCON_CPUCTL);
    while(!HS_FLD_TEST_DRF(HS_FALC, _FALCON_CPUCTL, _ALIAS_EN, _FALSE, cpuctlAliasEn))
    {
        cpuctlAliasEn = HS_CSB_REG_RD32_INLINE(FALCON_CPUCTL);
    }
}

/*!
 * Exception Installer which will install a minimum exception handler which just HALT's.
 * This subroutine sets up the Exception vector and other required registers for each exceptions. This will be built when we want to build secure ucode.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
void hsExceptionInstallerSelwreHang(void)
{
    LwU32 secSpr = 0;

    HS_CSB_REG_WR32_INLINE( FALCON_IBRKPT1, 0x00000000 );
    HS_CSB_REG_WR32_INLINE( FALCON_IBRKPT2, 0x00000000 );
    HS_CSB_REG_WR32_INLINE( FALCON_IBRKPT3, 0x00000000 );
    HS_CSB_REG_WR32_INLINE( FALCON_IBRKPT4, 0x00000000 );
    HS_CSB_REG_WR32_INLINE( FALCON_IBRKPT5, 0x00000000 );

    // Clear IE0 bit in CSW.
    falc_sclrb( 16 );

    // Clear IE1 bit in CSW.
    falc_sclrb( 17 );

    // Clear IE2 bit in CSW.
    falc_sclrb( 18 );

    // Clear Exception bit in CSW.
    falc_sclrb( 24 );

    falc_wspr( EV, hsExceptionHandlerSelwreHang );

    falc_rspr( secSpr, SEC );
    // Clear DISABLE_EXCEPTIONS
    secSpr = FLD_SET_DRF( _FALC, _SEC_SPR, _DISABLE_EXCEPTIONS, _CLEAR, secSpr );
    falc_wspr( SEC, secSpr );
}

/*!
 * @brief Stack Underflow Exception Installer for enabling the stack underflow exception in secure segment by
 * setting up the stackcfg register with stackbottom value.
 * @param None.
 * @return None.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline 
void 
hsStackUnderflowInstaller(void)
{
    LwU32 stackBottomLimit = HS_STACK_BOTTOM_LIMIT(HS_FALC);
    LwU32 var;
    //
    // STACKCFG.BOTTOM is word address, if the SP is equal or less than (STACKCFG.BOTTOM << 2), 
    // then an exception will be generated. Therefore pushing stackBottomLimit >> 2.
    //
    stackBottomLimit = stackBottomLimit >> 2;
    var = HS_CSB_REG_RD32_INLINE(FALCON_STACKCFG);
    var = HS_FLD_SET_DRF_NUM( HS_FALC, _FALCON_STACKCFG, _BOTTOM, stackBottomLimit, var);
    var = HS_FLD_SET_DRF( HS_FALC, _FALCON_STACKCFG, _SPEXC, _ENABLE, var );
    HS_CSB_REG_WR32_INLINE(FALCON_STACKCFG, var);
}

/*!
 * @brief Reset stack bottom limit to DMEM base (0).
 * Steady state ucodes have their own stack management scheme and might
 * allocate stack below the bottom limit set for HS FMC, in which case they will fail.
 * So this function is called during HS FMC exit. 
 * RTOS can implement ucode specific stack underflow detection.
 * 
 * @param None.
 * @return None.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline 
void 
hsStackBottomReset(void)
{
    LwU32 reg = 0;
    //
    // STACKCFG.BOTTOM is word address, if the SP is equal or less than (STACKCFG.BOTTOM << 2), 
    // then an exception will be generated. Therefore pushing stackBottomLimit >> 2.
    //
    reg = HS_CSB_REG_RD32_INLINE(FALCON_STACKCFG);
    reg = HS_FLD_SET_DRF_NUM( HS_FALC, _FALCON_STACKCFG, _BOTTOM, 0, reg);
    HS_CSB_REG_WR32_INLINE(FALCON_STACKCFG, reg);
}

/*
 *@brief: Reset SCP's 2 instruction sequencers and pipe to put SCP in a more deterministic state
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
void
hsResetScpLoopTracesAndPipe(void)
{
    LwU32 scpCtl1 = 0;
    LwU32 timer = 0;

    scpCtl1 = HS_CSB_REG_RD32_INLINE(SCP_CTL1);

    // Initialize CTL1 register and trigger a pipe and both scp loop traces reset 
    scpCtl1 = HS_FLD_SET_DRF(HS_FALC, _SCP_CTL1, _PIPE_RESET, _TASK, scpCtl1);
    scpCtl1 = HS_FLD_SET_DRF(HS_FALC, _SCP_CTL1, _SEQ_CLEAR, _TASK, scpCtl1);

    HS_CSB_REG_WR32_INLINE(SCP_CTL1, scpCtl1);

    // Poll PIPE_RESET until it clears
    while (HS_FLD_TEST_DRF(HS_FALC, _SCP_CTL1, _PIPE_RESET, _TASK,
                           HS_CSB_REG_RD32_INLINE(SCP_CTL1)))
    {
        timer++;
        if (timer >= HS_TIMEOUT_FOR_PIPE_RESET)
        {
           // In case PIPE_RESET does not clear before HS_DEFAULT_TIMEOUT_NS
           INFINITE_LOOP();
        }
    }
}


#ifndef STEADY_STATE_BUILD
/*!
 * @brief   Sets up SCP TRNG and starts the random number generator.
 *          We use direct falc instructions instead of lwstomary RD/WR macros
 *          since the later are not inlined and we require code to be inlined 
 *          here since SSP is not setup.
 */
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

    // HOLDOFF_INIT_LOWER
    val = HS_CSB_REG_RD32_INLINE(SCP_RNDCTL0);
    val = HS_FLD_SET_DRF_NUM( HS_FALC, _SCP_RNDCTL0, _HOLDOFF_INIT_LOWER, SCP_HOLDOFF_INIT_LOWER_VAL, val); 
    HS_CSB_REG_WR32_INLINE(SCP_RNDCTL0, val);

    // HOLDOFF_INTRA_MASK
    val = HS_CSB_REG_RD32_INLINE(SCP_RNDCTL1);
    val = HS_FLD_SET_DRF_NUM(HS_FALC, _SCP_RNDCTL1, _HOLDOFF_INTRA_MASK, SCP_HOLDOFF_INTRA_MASK, val);
    HS_CSB_REG_WR32_INLINE(SCP_RNDCTL1, val);

    // Set the ring length of ring A/B
    val = HS_CSB_REG_RD32_INLINE(SCP_RNDCTL11);
    val = HS_FLD_SET_DRF_NUM(HS_FALC, _SCP_RNDCTL11, _AUTOCAL_STATIC_TAP_A, SCP_AUTOCAL_STATIC_TAPA, val);
    val = HS_FLD_SET_DRF_NUM(HS_FALC, _SCP_RNDCTL11, _AUTOCAL_STATIC_TAP_B, SCP_AUTOCAL_STATIC_TAPB, val);
    HS_CSB_REG_WR32_INLINE(SCP_RNDCTL11, val);

    // Enable SCP TRNG
    val = HS_CSB_REG_RD32_INLINE(SCP_CTL1);
    val = HS_FLD_SET_DRF(HS_FALC, _SCP_CTL1_RNG, _EN, _ENABLED, val);
    HS_CSB_REG_WR32_INLINE(SCP_CTL1, val);

}

/*!
 * @brief   Stops SCP TRNG
 *          We use direct falc instructions instead of lwstomary RD/WR macros
 *          since the later are not inlined and we require code to be inlined 
 *          here since SSP is not setup.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
void _scpStopTrng(void)
{
    LwU32      val       = 0;

    val = HS_CSB_REG_RD32_INLINE(SCP_CTL1);
    val = HS_FLD_SET_DRF(HS_FALC, _SCP_CTL1_RNG, _EN, _DISABLED, val);
    HS_CSB_REG_WR32_INLINE(SCP_CTL1, val);
}



/*!
 * @brief   Gets 32 bit random number from SCP.
 *
 * @param[in,out] pRandomNum  Pointer to random number.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
void hsScpGetRandomNumber(LwU32 *pRand32)
{
    LwU32      i      = 0;

    // 
    // TODO: If this function is used outside of boot from HS init, need to add NULL Check
    // and create HS_INIT status
    // Lwrrently only caller is HS init function. This function can be merged into caller.
    //

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

}

//
// Initialize canary for HS using SCP TRNG
//
GCC_ATTRIB_ALWAYSINLINE()
static inline void hsInitializeStackCanaryHSUsingSCP(void)
{
    LwU32      rand32 = 0;
 
    //
    // Set the value of stack canary to a random value to ensure adversary
    // can not craft an attack by looking at the assembly to determine the canary value
    //
    hsScpGetRandomNumber(&rand32); 

    __stack_chk_guard = (void *)rand32;
}
#endif // !STEADY_STATE_BUILD

// LWDEC does not have Reset PLM
#ifndef BOOT_FROM_HS_FALCON_LWDEC
/*
 * Save/Lock and Restore falcon reset when binary is running
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void hsSelfLockFalconReset(LwBool bLock)
{
    LwU32 reg = 0;

    reg = HS_CSB_REG_RD32_INLINE(FALCON_RESET_PRIV_LEVEL_MASK);

    if (bLock)
    {
        // 
        // Lock falcon reset for level 0, 1, 2
        // Standalone HS binaries need raise the PLM to PL3 whereas steady state binaries need to
        // raise it PL2 so that they can lower the PLM during unload. This is done later in HS FMC before exit 
        // to steady state ucode.
        //
        reg = HS_FLD_SET_DRF( HS_FALC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, reg);
        reg = HS_FLD_SET_DRF( HS_FALC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, reg);
        reg = HS_FLD_SET_DRF( HS_FALC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _DISABLE, reg);
    }
    else
    {
        // Unlock to Level 0 writable 
        reg = HS_FLD_SET_DRF( HS_FALC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _ENABLE, reg);
        reg = HS_FLD_SET_DRF( HS_FALC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _ENABLE, reg);
        reg = HS_FLD_SET_DRF( HS_FALC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _ENABLE, reg);
    }

    HS_CSB_REG_WR32_INLINE(FALCON_RESET_PRIV_LEVEL_MASK, reg);
}
#endif // !BOOT_FROM_HS_FALCON_LWDEC

GCC_ATTRIB_ALWAYSINLINE()
static inline
void
hsEntryChecks(void)
{
    // 
    // STEP 1: Set SP to end of DMEM
    // Not using HWCFG provided value here since CSB access is not allowed at this point.
    // Pre-GA10X, if SP is set to end of DMEM, it rolls over to 0.
    // This leads to exception being raised in when we enable stack underflow exception
    // in hsStackUnderflowInstaller, since SP (0) is lower than stack bottom.
    // So we set SP to value (DMEM_end -4) for pre-GA10X.
    // This roll over does not happen in GA10X and we can set SP to end of DMEM value.
    //
    falc_wspr(SP, FALCON_DMEM_SIZE(HS_FALC));

    // STEP 2: Clear SCP signature
    falc_scp_forget_sig();

    // STEP 3: Clear SCP GPRs
    hsClearSCPRegisters();

    // STEP 4: Clear Falcon GPRs
    hsClearFalconGprs();

    // STEP 5: Enable CSBERRSTAT
    hsValidateCsberrstat();

    // LWDEC does not have CMEMBASE
#ifndef BOOT_FROM_HS_FALCON_LWDEC
    // STEP 6: Write CMEMBASE = 0, to make sure ldd/std instructions are not routed to CSB.
    HS_CSB_REG_WR32_INLINE(FALCON_CMEMBASE, 0);
#endif // BOOT_FROM_HS_FALCON_LWDEC

    // STEP 7: Scrub bss and unused DMEM
#ifndef STEADY_STATE_BUILD
    hsScrubDmem((LwU32)_bss_start, FALCON_DMEM_SIZE(HS_FALC));
#endif // !STEADY_STATE_BUILD

    // STEP 8: Restart from HS mitigation
    hsMitigateNSRestartFromHS();

    // STEP 9: Install minimum exception handler
    hsExceptionInstallerSelwreHang();

    // STEP 10: Set the stack pointer to the MIN addr so if stack reaches that address then falcon can throw an exception.
    hsStackUnderflowInstaller();

    // STEP 11: Reset both SCP instruction sequencers and Pipe Reset to put SCP in a deterministic state
    hsResetScpLoopTracesAndPipe();

#ifndef STEADY_STATE_BUILD
    // STEP 12: Canary setup for SSP
    hsInitializeStackCanaryHSUsingSCP();
#endif // !STEADY_STATE_BUILD

    // LWDEC does not have Reset PLM
#ifndef BOOT_FROM_HS_FALCON_LWDEC
    // STEP 13: Lock falcon Reset
    hsSelfLockFalconReset(LW_TRUE);
#endif // BOOT_FROM_HS_FALCON_LWDEC

    //
    // STEP 14: Set BAR0 timeout value
    // WARNING: This function must be called before any BAR0 access.
    //
    HS_CSB_REG_WR32_INLINE(BAR0_TMOUT, HS_DRF_DEF(HS_FALC, _BAR0_TMOUT, _CYCLES, __PROD));

    return;
}

#endif // HS_ENTRY_CHECKS_H
