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
 * @file   boot_selwre.c
 * @brief  HS Routines for SOE Bootloader
 */
/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "lwoslayer.h"
#include "dev_soe_csb.h"
#include "csb.h"
#include "scp_rand.h"
#include "scp_internals.h"
#include "scp_crypt.h"
#include "dev_fuse.h"
#include "dev_lws_master.h"
#include "dev_lws_master_addendum.h"
#include "boot_platform_interface.h"

/* ------------------------- Application Includes --------------------------- */
#include "boot.h"
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */

// VERSIONING:
// 1) HS uCode Version.  Incremented each time HS is signed.
//    Ensures that HS uCode is only used with a compatible LS image.
//    This could be removed if LS IMEM signature covered both LS+HS regions.
// 2) Overall uCode Version.  Hashed into LS IMEM and LS DMEM signatures
//    to ensure that only compatible versions are used together.
//    Incremented with each release of uCode.
//    Minor version only incremented for experimental releases.
// 3) Fuse Version for Security (not in DMEM). LWL_FUSE_UCODE_REV
//    UCODE REVOCATION uses this to prevent old less secure uCode versions
//    from running on newer silicon SKU with fuse rev bump.
//    This is only incremented if there is a security concern.
//
volatile LwU32 gRevUcodeHs
    GCC_ATTRIB_SECTION("data_hs", "gRevUcodeHs") = LWL_REV_UCODE_HS;
volatile LwU32 gRevUcodeOverall
    GCC_ATTRIB_SECTION("data_hs", "gRevUcodeOverall") = LWL_UCODE_VERSION;

//
// Global variables (better when alignment required compared to local variables).
// Keep these DMEM locations constant so we do not have to re-sign HS uCode.
//
extern LwU8  gSignatureLsDmem[SCP_SIG_SIZE_IN_BYTES];
extern LwU8  gSignatureLsImem[SCP_SIG_SIZE_IN_BYTES];

/* These variables will be patched from makefile based on SOE IMAGE's linker output */
LwU32 appDmemBase
    GCC_ATTRIB_SECTION("data_hs", "appDmemBase")
    GCC_ATTRIB_ALIGN(sizeof(LwU32));
LwU32 appDmemImageSize
    GCC_ATTRIB_SECTION("data_hs", "appDmemImageSize")
    GCC_ATTRIB_ALIGN(sizeof(LwU32));
LwU32 appImemLSBase
    GCC_ATTRIB_SECTION("data_hs", "appImemLSBase")
    GCC_ATTRIB_ALIGN(sizeof(LwU32));
LwU32 appImemLSSize
    GCC_ATTRIB_SECTION("data_hs", "appImemLSSize")
    GCC_ATTRIB_ALIGN(sizeof(LwU32));

// Variables from link script.
extern LwU32 _dataStackBottom[];
extern LwU32 _resident_code_start[];
extern LwU32 _resident_code_end[];
extern LwU32 _stackChkGuardEndLoc[];

// More HS variables used for LS signature verification.
LwU8 gHsDmHash[SCP_SIG_SIZE_IN_BYTES]
    GCC_ATTRIB_SECTION("data_hs", "gHsDmHash")
    GCC_ATTRIB_ALIGN(SCP_SIG_SIZE_IN_BYTES);
LwU8 gHsTempBuffer[SCP_SIG_SIZE_IN_BYTES]
    GCC_ATTRIB_SECTION("data_hs", "gHsTempBuffer")
    GCC_ATTRIB_ALIGN(SCP_SIG_SIZE_IN_BYTES);
LwU8 gHsTempSig[SCP_SIG_SIZE_IN_BYTES]
    GCC_ATTRIB_SECTION("data_hs", "gHsTempSig")
    GCC_ATTRIB_ALIGN(SCP_SIG_SIZE_IN_BYTES);
LwU32 prHsScratchPad[SCP_REG_SIZE_IN_DWORDS]
    GCC_ATTRIB_SECTION("data_hs", "prHsScratchPad")
    GCC_ATTRIB_ALIGN(SCP_ADDR_ALIGNMENT_IN_BYTES);

void *__stack_chk_guard
    GCC_ATTRIB_SECTION("stackchkguard", "__stack_chk_guard") = (void *) 0;

/* ------------------------- Static Variables ------------------------------- */

static void *canaryFromNSCaller
    GCC_ATTRIB_SECTION("data_hs", "canaryFromNSCaller") = (void *) 0;

/* ------------------------- Private Functions ------------------------------ */

static inline void InitializeStackCanary_HS(void)
    GCC_ATTRIB_SECTION("imem_hs", "InitializeStackCanary_HS")
    GCC_ATTRIB_ALWAYSINLINE();
static inline void saveStackCanary_HS(void)
    GCC_ATTRIB_SECTION("imem_hs", "saveStackCanary_HS")
    GCC_ATTRIB_ALWAYSINLINE();
static inline void restoreStackCanary_HS(void)
    GCC_ATTRIB_SECTION("imem_hs", "restoreStackCanary_HS")
    GCC_ATTRIB_ALWAYSINLINE();
static inline void  stopScpRng(void)
    GCC_ATTRIB_SECTION("imem_hs", "stopScpRng")
    GCC_ATTRIB_ALWAYSINLINE();
static inline void  startScpRng(void)
    GCC_ATTRIB_SECTION("imem_hs", "startScpRng")
    GCC_ATTRIB_ALWAYSINLINE();
static inline void  initScpRng(void)
    GCC_ATTRIB_SECTION("imem_hs", "initScpRng")
    GCC_ATTRIB_ALWAYSINLINE();
static inline void  waitForRngBufferToBeReady(void)
    GCC_ATTRIB_SECTION("imem_hs", "waitForRngBufferToBeReady")
    GCC_ATTRIB_ALWAYSINLINE();
static inline FLCN_STATUS get128BitRandNum(LwU32 pRandInOut[SCP_REG_SIZE_IN_DWORDS])
    GCC_ATTRIB_SECTION("imem_hs", "get128BitRandNum")
    GCC_ATTRIB_ALWAYSINLINE();
static inline LwBool isValidChip(void)
    GCC_ATTRIB_SECTION("imem_hs", "isValidChip")
    GCC_ATTRIB_ALWAYSINLINE();
static inline LwU32 getUcodeRevFuseVal(void)
    GCC_ATTRIB_SECTION("imem_hs", "getUcodeRevFuseVal")
    GCC_ATTRIB_ALWAYSINLINE();
static inline LwU32 getUcodeRevFpfVal(void)
    GCC_ATTRIB_SECTION("imem_hs", "getUcodeRevFpfVal")
    GCC_ATTRIB_ALWAYSINLINE();
static inline void clearSCPregisters(void)
    GCC_ATTRIB_SECTION("imem_hs", "clearSCPregisters")
    GCC_ATTRIB_ALWAYSINLINE();
static inline void clearFalconGprs(void)
    GCC_ATTRIB_SECTION("imem_hs", "clearFalconGprs")
    GCC_ATTRIB_ALWAYSINLINE();
static inline void raiseFalconResetPLM(void)
    GCC_ATTRIB_SECTION("imem_hs", "raiseFalconResetPLM")
    GCC_ATTRIB_ALWAYSINLINE();
static inline void lowerFalconResetPLM(void)
    GCC_ATTRIB_SECTION("imem_hs", "lowerFalconResetPLM")
    GCC_ATTRIB_ALWAYSINLINE();
static inline LwBool isDebugModeEnabled(void)
    GCC_ATTRIB_SECTION("imem_hs", "isDebugModeEnabled")
    GCC_ATTRIB_ALWAYSINLINE();
static inline void ensureReturnAddressNotInHS(void)
    GCC_ATTRIB_SECTION("imem_hs", "ensureReturnAddressNotInHS")
    GCC_ATTRIB_ALWAYSINLINE();

/********************************** SCP RNG code ************************************/
//
// Usage: Call initScpRng one time from HS. Subsequently, call get128BitRandNum anytime you need a random number
// NOTE: If you exit HS, mode, initScpRng must be called on next entry into HS mode since HS should not trust LS/NS
//
//
// Stop Random Number generator. This is used when trying to re-configure SCP's RNG
//
static inline void
stopScpRng(void)
{
    /*
    LwU32 regVal;
    // disable RNG
    regVal = DRF_DEF(_CSOE, _SCP_CTL1, _SEQ_CLEAR,        _IDLE)     | // Using _IDLE as HW has not provided any enum to not trigger
             DRF_DEF(_CSOE, _SCP_CTL1, _PIPE_RESET,       _IDLE)     | // Using _IDLE as HW has not provided any enum to not trigger
             DRF_DEF(_CSOE, _SCP_CTL1, _RNG_FAKE_MODE,    _DISABLED) |
             DRF_DEF(_CSOE, _SCP_CTL1, _RNG_EN,           _DISABLED) |
             DRF_DEF(_CSOE, _SCP_CTL1, _SF_FETCH_AS_ZERO, _DISABLED) |
             DRF_DEF(_CSOE, _SCP_CTL1, _SF_FETCH_BYPASS,  _DISABLED) |
             DRF_DEF(_CSOE, _SCP_CTL1, _SF_PUSH_BYPASS,   _DISABLED);
    CSB_REG_WR32(LW_CSOE_SCP_CTL1, regVal);

    // dkumar: Is the below required? the lockdown gets auto applied after jumping into HS mode. Why is TSEC code doing this? Putting unnecessary writes is also risk (ROP)
    regVal = CSB_REG_RD32(LW_CSOE_SCP_CTL_CFG);
    regVal = FLD_SET_DRF(_CSOE, _SCP_CTL_CFG, _LOCKDOWN_SCP, _ENABLE, regVal);
    CSB_REG_WR32(LW_CSOE_SCP_CTL_CFG, regVal);
    */
    CSB_REG_WR32(LW_CSOE_SCP_CTL1, 0);
}

//
// Configure and start random number generator. The function assumes that RNG was stopped previously or that HW
// doesn't require doing so
//
static inline void
startScpRng(void)
{
    //
    // TODO_apoorvg: I took some of these hard coded constants from https://rmopengrok.lwpu.com/source/xref/chips_a/uproc/sec2/src/sec2/pascal/sec2_sec2gp10x.c#611
    // Also, need to confirm the commented missing defines from manuals
    // Configure RNG setting. Leave no field implicitly initialized to zero
    //
    CSB_REG_WR32(LW_CSOE_SCP_RNDCTL0, DRF_NUM(_CSOE, _SCP_RNDCTL0, _HOLDOFF_INIT_LOWER, 0x7fff)); //DRF_DEF(_CSOE, _SCP_RNDCTL0, _HOLDOFF_INIT_LOWER, __PROD)

    CSB_REG_WR32(LW_CSOE_SCP_RNDCTL1, DRF_DEF(_CSOE, _SCP_RNDCTL1, _HOLDOFF_INIT_UPPER, _ZERO) |
                                      DRF_NUM(_CSOE, _SCP_RNDCTL1, _HOLDOFF_INTRA_MASK, 0x03ff)); //DRF_DEF(_CSOE, _SCP_RNDCTL1, _HOLDOFF_INTRA_MASK, __PROD)

    CSB_REG_WR32(LW_CSOE_SCP_RNDCTL11, DRF_NUM(_CSOE, _SCP_RNDCTL11, _AUTOCAL_ENABLE, 0)|
                                       DRF_NUM(_CSOE, _SCP_RNDCTL11, _AUTOCAL_MASTERSLAVE, 0) | // _NO
                                       DRF_NUM(_CSOE, _SCP_RNDCTL11, _SYNCH_RAND_A, 0) | // _NO
                                       DRF_NUM(_CSOE, _SCP_RNDCTL11, _SYNCH_RAND_B, 0) | // _NO)
                                       DRF_DEF(_CSOE, _SCP_RNDCTL11, _RAND_SAMPLE_SELECT_A, _OSC) |
                                       DRF_DEF(_CSOE, _SCP_RNDCTL11, _RAND_SAMPLE_SELECT_B, _LFSR) |
                                       DRF_NUM(_CSOE, _SCP_RNDCTL11, _AUTOCAL_STATIC_TAP_A, 0x000f) | //__PROD
                                       DRF_NUM(_CSOE, _SCP_RNDCTL11, _AUTOCAL_STATIC_TAP_B, 0x000f) | //__PROD
                                       DRF_NUM(_CSOE, _SCP_RNDCTL11, _MIN_AUTO_TAP,              0) |
                                       DRF_NUM(_CSOE, _SCP_RNDCTL11, _AUTOCAL_HOLDOFF_DELAY,     0) | // dkumar: HW init is 1
                                       DRF_NUM(_CSOE, _SCP_RNDCTL11, _AUTOCAL_ASYNCH_HOLD_DELAY, 0) |
                                       DRF_DEF(_CSOE, _SCP_RNDCTL11, _AUTOCAL_SAFE_MODE, _DISABLE));

    // dkumar: Should we be force overriding other SCP RNG related regs under CSOE_SCP*?

    // Enable RNG
    CSB_REG_WR32(LW_CSOE_SCP_CTL1, DRF_DEF(_CSOE, _SCP_CTL1, _SEQ_CLEAR,        _IDLE)     | // Using _IDLE as HW has not provided any enum to not trigger
                                   DRF_DEF(_CSOE, _SCP_CTL1, _PIPE_RESET,       _IDLE)     | // Using _IDLE as HW has not provided any enum to not trigger
                                   DRF_DEF(_CSOE, _SCP_CTL1, _RNG_FAKE_MODE,    _DISABLED) |
                                   DRF_DEF(_CSOE, _SCP_CTL1, _RNG_EN,           _ENABLED)  |
                                   DRF_DEF(_CSOE, _SCP_CTL1, _SF_FETCH_AS_ZERO, _DISABLED) |
                                   DRF_DEF(_CSOE, _SCP_CTL1, _SF_FETCH_BYPASS,  _DISABLED) |
                                   DRF_DEF(_CSOE, _SCP_CTL1, _SF_PUSH_BYPASS,   _DISABLED));
}

// Initialize the SCP Random Number Generator
static inline void
initScpRng(void)
{
    stopScpRng();
    startScpRng();
}

//
// wait for random number buffer to be full
//
static inline void
waitForRngBufferToBeReady(void)
{
    LwU32 regVal;
    do 
    {
        regVal = CSB_REG_RD32(LW_CSOE_SCP_RNG_STAT0);
    } while (DRF_VAL(_CSOE, _SCP_RNG_STAT0, _RAW_RAND_READY, regVal) == LW_CSOE_SCP_RNG_STAT0_RAW_RAND_READY_PROCESSING);
}

//
// Generate a random number using SCP RNG.
// Assumption #1: SCP RNG is already configured correctly
// Assumption #2: pRandInOut is 16 byte aligned
//
static inline FLCN_STATUS
get128BitRandNum
(
    LwU32 pRandInOut[SCP_REG_SIZE_IN_DWORDS]
)
{
    LwU32 val32; 

    if (((LwU32)pRandInOut & (SCP_ADDR_ALIGNMENT_IN_BYTES - 1)) != 0)
    {
        return FLCN_ERROR;
    }

    val32 = soeBar0RegRd32_PlatformHS(LW_PSMC_BOOT_2);
    if (FLD_TEST_DRF(_PSMC, _BOOT_2, _FMODEL, _YES, val32) && 
        isDebugModeEnabled())
    {
        LwU32 val;
        for (val = 0; val < SCP_REG_SIZE_IN_DWORDS; val++)
        {
            pRandInOut[val] = 0xbeefaaa0 | val;
        }

        return FLCN_OK;
    }

    waitForRngBufferToBeReady(); // wait for random number buffer to be full before consuming

    falc_scp_trap(TC_INFINITY);

    //
    // Load input (context) into SCP_R5.
    // dkumar: What is the purpose of this? Use this as input to rand generation function? Don't see anyone initializing this.
    //
    falc_trapped_dmwrite(falc_sethi_i((LwU32)pRandInOut, SCP_R5));
    falc_dmwait();

    // Generate two conselwtive random numbers which we will use to create the final random number
    falc_scp_rand(SCP_R4);
    // dkumar: Should there be a waitForRngBufferToBeReady() here before next falc_scp_rand?
    falc_scp_rand(SCP_R3);

    // mix previous whitened rand data. SCP_R4 = SCP_R4 ^ SCP_R5. SCP_R5 is the initial context
    falc_scp_xor(SCP_R5, SCP_R4);

    // Use AES encryption to whiten random data. SCP_R3 = AES-128-ECB(SCP_R4 /* key */, SCP_R3 /* data */)
    falc_scp_key(SCP_R4);
    falc_scp_encrypt(SCP_R3, SCP_R3);

    // Copy the result of encryption above back into the desired output location
    falc_trapped_dmread(falc_sethi_i((LwU32)pRandInOut, SCP_R3));
    falc_dmwait();

    falc_scp_trap(0);

    return FLCN_OK;
}

static inline void
InitializeStackCanary_HS(void)
{
    LwBool bFound = LW_FALSE;
    LwU32 i;

    initScpRng();

    //
    // Set the value of stack canary to a random value to ensure adversary cannot craft an attack by looking at the
    // assembly to determine the canary value.
    //
    do
    {
        if (get128BitRandNum(prHsScratchPad) == FLCN_OK) 
        {
            //
            // Make sure value of random number is not all 0s or all 1s as these
            // might be values an adversary may think of trying
            //
            for (i = 0; i < SCP_REG_SIZE_IN_DWORDS; i++)
            {
                if (prHsScratchPad[i] != 0 && prHsScratchPad[i] != 0xffffffff)
                {
                    __stack_chk_guard = (void *)(prHsScratchPad[i]);
                    bFound = LW_TRUE;
                    break;
                }
            }
        }
        else 
        {
            // Fatal error if 128 bit random number generation failed
            soeWriteStatusToFalconMailbox(SOE_ERROR_HS_RAND_GEN_FAILED);
            clearFalconGprs();
            OS_HALT();
        }
    } while (!bFound);

    // Scrub the local variable as a good security practice
    prHsScratchPad[0] = 0;
    prHsScratchPad[1] = 0;
    prHsScratchPad[2] = 0;
    prHsScratchPad[3] = 0;
}

static inline void
saveStackCanary_HS(void)
{
    canaryFromNSCaller = __stack_chk_guard;
}

static inline void
restoreStackCanary_HS(void)
{
    __stack_chk_guard = canaryFromNSCaller;
}

/*!
 * @brief clear all the falcon General purpose registers
 */
static inline void clearFalconGprs(void)
{
    //  clear all Falcon GPRs.
    asm volatile ( "clr.w a0;");
    asm volatile ( "clr.w a1;");
    asm volatile ( "clr.w a2;");
    asm volatile ( "clr.w a3;");
    asm volatile ( "clr.w a4;");
    asm volatile ( "clr.w a5;");
    asm volatile ( "clr.w a6;");
    asm volatile ( "clr.w a7;");
    asm volatile ( "clr.w a8;");
    asm volatile ( "clr.w a9;");
    asm volatile ( "clr.w a10;");
    asm volatile ( "clr.w a11;");
    asm volatile ( "clr.w a12;");
    asm volatile ( "clr.w a13;");
    asm volatile ( "clr.w a14;");
    asm volatile ( "clr.w a15;");
}

/*!
 * @brief clear all the SCP registers
 */
static inline void clearSCPregisters(void)
{
    // Clear SCP registers
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

/*!
 * @brief Disable write access for Falcon Reset
 */
GCC_ATTRIB_STACK_PROTECT()
static inline void
raiseFalconResetPLM(void)
{
    LwU32 val = 0;
    // Protect falcon reset such that only level 2 can write to LW_CSOE_FALCON_ENGINE_RESET
    val = FLD_SET_DRF(_CSOE, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ALL_LEVELS_DISABLED, val);
    val = FLD_SET_DRF(_CSOE, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_VIOLATION,  _REPORT_ERROR, val);
    val = FLD_SET_DRF(_CSOE, _FALCON_RESET_PRIV_LEVEL_MASK, _READ_PROTECTION,  _ALL_LEVELS_ENABLED, val);
    val = FLD_SET_DRF(_CSOE, _FALCON_RESET_PRIV_LEVEL_MASK, _READ_VIOLATION,   _REPORT_ERROR, val);
    REG_WR32_STALL(CSB, LW_CSOE_FALCON_RESET_PRIV_LEVEL_MASK, val);
}

/*!
 * @brief Allow LS access for Falcon Reset
 */
GCC_ATTRIB_STACK_PROTECT()
static inline void
lowerFalconResetPLM(void)
{
    LwU32 val = 0;
    // Protect falcon reset such that only level 2 can write to LW_CSOE_FALCON_ENGINE_RESET
    val = REG_RD32(CSB, LW_CSOE_FALCON_RESET_PRIV_LEVEL_MASK);
    val = FLD_SET_DRF(_CSOE, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _LEVEL2_ENABLED, val);
    val = FLD_SET_DRF(_CSOE, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_VIOLATION,  _REPORT_ERROR, val);
    REG_WR32_STALL(CSB, LW_CSOE_FALCON_RESET_PRIV_LEVEL_MASK, val);
}

/*! 
 * @brief Checks if Debug mode is enabled or not. 
 * 
 * @return LW_TRUE  : if Debug mode is enabled
 *         LW_FALSE : if Debug mode is disabled
 */
static inline 
LwBool 
isDebugModeEnabled(void)
{
    LwU32  ctlVal;

    ctlVal = CSB_REG_RD32(LW_CSOE_SCP_CTL_STAT);
    
    return FLD_TEST_DRF(_CSOE_SCP, _CTL_STAT, _DEBUG_MODE, _DISABLED, ctlVal) ? 
              LW_FALSE : LW_TRUE;
}

static inline void
ensureReturnAddressNotInHS(void)
{
    LwU32 imtagVal = 0;
    LwU32 val = 0; 
    LwU32 *returnAddress = __builtin_return_address(0);

    if (!LW_IS_ALIGNED((LwU32)returnAddress, 4))
    {
        // coverity[no_escape]
        while(1);
    }

    falc_imtag( &imtagVal, (LwU32)returnAddress );
    if (FLD_TEST_DRF(_FLCN, _IMTAG, _BLK_VALID,   _NO, imtagVal)    ||
        FLD_TEST_DRF(_FLCN, _IMTAG, _BLK_PENDING, _YES, imtagVal)   ||
        FLD_TEST_DRF(_FLCN, _IMTAG, _BLK_SELWRE, _YES, imtagVal)    ||
        FLD_TEST_DRF(_FLCN, _IMTAG, _BLK_MULTI_HIT, _YES, imtagVal) ||
        FLD_TEST_DRF(_FLCN, _IMTAG, _BLK_MISS, _YES, imtagVal))
    {
        // coverity[no_escape]
        while(1);
    }

    // 
    // ******************* NO CSB ACCESS BEFORE THIS POINT OTHER THAN LW_CSOE_FALCON_HWCFG/HWCFG0******************
    // HS Entry Step #8: CSBERRSTAT Check.
    // 
    val = REG_RD32(CSB, LW_CSOE_FALCON_CSBERRSTAT);
    if (FLD_TEST_REF(LW_CSOE_FALCON_CSBERRSTAT_ENABLE, _FALSE, val))
    {
        // coverity[no_escape]
        while(1);
    }
    
    // Set CMEMBASE = 0 
    val = REG_RD32(CSB, LW_CSOE_FALCON_CMEMBASE);
    val = FLD_SET_DRF_NUM(_CSOE_FALCON, _CMEMBASE, _VAL, LW_CSOE_FALCON_CMEMBASE_VAL_INIT, val);
    REG_WR32_STALL(CSB, LW_CSOE_FALCON_CMEMBASE, val);

    //
    // Step 9: NS_Race_To_HS Mitigation.
    // Step 9a: Program IMEMC to read instruction at returnAddress and returnAddress + 1.
    //
    val = 0;
    val = FLD_SET_DRF_NUM(_CSOE_FALCON, _IMEMC, _BLK, imtagVal, val );
    val = FLD_SET_DRF_NUM(_CSOE_FALCON, _IMEMC, _OFFS, REF_VAL(LW_CSOE_FALCON_IMEMC_OFFS, (LwU32)returnAddress), val);
    val = FLD_SET_DRF(_CSOE_FALCON, _IMEMC, _AINCR, _TRUE, val);
    REG_WR32_STALL(CSB, LW_CSOE_FALCON_IMEMC(0), val);

    // Check for CSB error.
    val = REG_RD32( CSB, LW_CSOE_FALCON_CSBERRSTAT );
    if( FLD_TEST_REF( LW_CSOE_FALCON_CSBERRSTAT_VALID, _TRUE, val ) )
    {
        // coverity[no_escape]
        while(1);
    }

    // Step 9b: Read instruction at returnAddress and returnAddress + 1
    LwU32 retInst1 = REG_RD32( CSB, LW_CSOE_FALCON_IMEMD(0) );

    // Check for CSB error.
    val = REG_RD32( CSB, LW_CSOE_FALCON_CSBERRSTAT );
    if( FLD_TEST_REF( LW_CSOE_FALCON_CSBERRSTAT_VALID, _TRUE, val ) )
    {
        // coverity[no_escape]
        while(1);
    }

    LwU32 retInst2 = REG_RD32( CSB, LW_CSOE_FALCON_IMEMD(0) );

    // Check for CSB error.
    val = REG_RD32( CSB, LW_CSOE_FALCON_CSBERRSTAT );
    if( FLD_TEST_REF( LW_CSOE_FALCON_CSBERRSTAT_VALID, _TRUE, val ) )
    {
        // coverity[no_escape]
        while(1);
    }

#define LW_FLCN_JMP_INSTR_OPCODE     7:0
#define LW_FLCN_JMP_INSTR_OPCODE_JMP 0x3E
#define LW_FLCN_JMP_INSTR_OFFSET     31:8

    LwU32 nsDummyJmp1Addr = ((LwU32)returnAddress) + 4;
    LwU32 nsDummyJmp2Addr = ((LwU32)returnAddress) + 8;
    //
    // Step 9c: Check that instruction at returnAddress should be "jmp returnAddress+4" and instruction at
    //           returnAddress + 4 should be "jmp returnAddress+8".
    //
    if( !FLD_TEST_REF( LW_FLCN_JMP_INSTR_OPCODE, _JMP, retInst1 ) ||
        ( REF_VAL( LW_FLCN_JMP_INSTR_OFFSET, retInst1 ) != nsDummyJmp1Addr ) ||
        !FLD_TEST_REF( LW_FLCN_JMP_INSTR_OPCODE, _JMP, retInst2 ) ||
        ( REF_VAL( LW_FLCN_JMP_INSTR_OFFSET, retInst2 ) != nsDummyJmp2Addr ) )
    {
        // coverity[no_escape]
        while(1);
    }

    // Step 9d: Verify that the jmp to returnAddress + 4 does not end up in HS.
    falc_imtag( &imtagVal, nsDummyJmp1Addr );
    if (FLD_TEST_DRF(_FLCN, _IMTAG, _BLK_VALID,   _NO, imtagVal)    ||
        FLD_TEST_DRF(_FLCN, _IMTAG, _BLK_PENDING, _YES, imtagVal)   ||
        FLD_TEST_DRF(_FLCN, _IMTAG, _BLK_SELWRE, _YES, imtagVal)    ||
        FLD_TEST_DRF(_FLCN, _IMTAG, _BLK_MULTI_HIT, _YES, imtagVal) ||
        FLD_TEST_DRF(_FLCN, _IMTAG, _BLK_MISS, _YES, imtagVal))
    {
        // coverity[no_escape]
        while(1);
    }

    // Step 9e: Verify that the jmp to returnAddress + 8 does not end up in HS.
    falc_imtag( &imtagVal, nsDummyJmp2Addr );
    if (FLD_TEST_DRF(_FLCN, _IMTAG, _BLK_VALID,   _NO, imtagVal)    ||
        FLD_TEST_DRF(_FLCN, _IMTAG, _BLK_PENDING, _YES, imtagVal)   ||
        FLD_TEST_DRF(_FLCN, _IMTAG, _BLK_SELWRE, _YES, imtagVal)    ||
        FLD_TEST_DRF(_FLCN, _IMTAG, _BLK_MULTI_HIT, _YES, imtagVal) ||
        FLD_TEST_DRF(_FLCN, _IMTAG, _BLK_MISS, _YES, imtagVal))
    {
        // coverity[no_escape]
        while(1);
    }
}

void hsEntry(void)
    GCC_ATTRIB_SECTION("imem_hs", "entry")
    GCC_ATTRIB_NOINLINE()
    GCC_ATTRIB_USED();
void  hsMain(void)
    GCC_ATTRIB_SECTION("imem_hs", "hsMain")
    GCC_ATTRIB_NOINLINE();
void  hsUcodeRevocation(void)
    GCC_ATTRIB_SECTION("imem_hs", "hsUcodeRevocation")
    GCC_ATTRIB_NOINLINE();
void  hsFatalError(SOE_STATUS)
    GCC_ATTRIB_SECTION("imem_hs", "hsFatalError")
    GCC_ATTRIB_NOINLINE();
void* hsMemCopy(void *, const void *, LwU32)
    GCC_ATTRIB_SECTION("imem_hs", "hsMemCopy")
    GCC_ATTRIB_NOINLINE();
void  hsMemClear(void *, LwU32)
    GCC_ATTRIB_SECTION("imem_hs", "hsMemClear")
    GCC_ATTRIB_NOINLINE();
LwU32 hsMemCompare(void *, void *, LwU32)
    GCC_ATTRIB_SECTION("imem_hs", "hsMemCompare")
    GCC_ATTRIB_NOINLINE();
void  hsVerifyLsSignature(LwU8 *, LwU8 *, LwU32, LwU32)
    GCC_ATTRIB_SECTION("imem_hs", "hsVerifyLsSignature")
    GCC_ATTRIB_NOINLINE();
void  hsCallwlateHash(LwU8 *, LwU8 *, LwU32, LwU32)
    GCC_ATTRIB_SECTION("imem_hs", "hsCallwlateHash")
    GCC_ATTRIB_NOINLINE();
void  hsCallwlateSignature(LwU8 *, LwU8 *)
    GCC_ATTRIB_SECTION("imem_hs", "hsCallwlateSignature")
    GCC_ATTRIB_NOINLINE();
void  hsValidateImemBlocks(void)
    GCC_ATTRIB_SECTION("imem_hs", "hsValidateImemBlocks")
    GCC_ATTRIB_NOINLINE();
void hsSetupPriv(void)
    GCC_ATTRIB_SECTION("imem_hs", "hsSetupPriv")
    GCC_ATTRIB_NOINLINE();

//stack_chk_fail cannot be inlined, since compiler calls this function after
//all the inlining optimization is already done.
void  __stack_chk_fail(void)
    GCC_ATTRIB_SECTION("imem_hs", "__stack_chk_fail")
    GCC_ATTRIB_NOINLINE();

//Disable GCC attribute to disable stack protection on this function
__attribute__((no_stack_protect))
void __stack_chk_fail()
{
  soeWriteStatusToFalconMailbox(SOE_ERROR_HS_STACK_CHECK_FAILED);
  asm volatile("halt;");
}

#define _selwre_call(func, suffix) \
{\
    asm volatile ("jmp _selwre_call_" #suffix  ";\n" ".align 4\n" "_selwre_call_" #suffix ":\n" "lcall `(24@lo(_" #func "));\n" ::: "memory"); \
}

#define _SELWRE_CALL(func, suffix)  _selwre_call(func, suffix)
#define SELWRE_CALL(func)   _SELWRE_CALL(func, __COUNTER__)

/*!
 * This is the HS Entry function. This will be called indirectly using __imemHSBase.
 */
GCC_ATTRIB_NO_STACK_PROTECT()
void
hsEntry(void)
{
    LwU32 val = 0;

    soeWriteStatusToFalconMailbox(SOE_BOOT_BINARY_STARTED_IN_HS);

    // Save Regs
    asm volatile("pushm a15;");

    // HS Entry Step #2
    falc_scp_forget_sig();

    // HS Entry Step #3
    clearSCPregisters();
    
    // Step 7. Verify return address is not in HS.
    ensureReturnAddressNotInHS();

    // HS Entry Step #4
    clearFalconGprs();

    // Step 10: NS_Restart_From_HS mitigation
    val = CSB_REG_RD32(LW_CSOE_FALCON_CPUCTL_ALIAS);
    CSB_REG_WR32(LW_CSOE_FALCON_CPUCTL_ALIAS, FLD_SET_DRF(_CSOE_FALCON, _CPUCTL_ALIAS, _EN, _FALSE, val));

    // Init BAR0 so we can read the from Bar0
    soeBar0InitTmout_PlatformHS();

   // Step 11: Minimal Exception handler. 
    hsInstallExceptionHandler();

    //
    // HS Entry Step #13
    // Setup SSP.
    //
    saveStackCanary_HS();  // In case NS also wants to use canary for its own protection

    InitializeStackCanary_HS();

    // HS Entry Step #14: Raise Falcon RESET PLM to level 3.
    raiseFalconResetPLM();

    hsMain();

    //
    // HS code is mostly done now. Before we call into LS, we need to restore the stack canary 
    // that was setup by NS code, so that we don't leak the stack canary setup by HS. 
    //
    restoreStackCanary_HS();

    //
    // HS Exit Step #4
    // Clear all General Purpose Regs to eliminate chance of
    // exposing any secure information.
    //
    clearFalconGprs();

    // Restore Regs
    asm volatile("popm a15 0x0;");

    //
    // Permit LS to do Falcon Resets. This is needed for HS to give permission for CPU
    // to do the reset, one LS is ready to be unloaded.
    //
    lowerFalconResetPLM();
    
    // Enter LS
    soeWriteStatusToFalconMailbox(SOE_BOOT_BINARY_LS_ENTRY);
    
    //
    // entryFn for the LS app is always 0x0. Let's hardcode it instead of
    // patching it in to DMEM since patching it in to DMEM won't be covered
    // by the HS signature.
    //
    asm volatile("call 0x0;");

    soeWriteStatusToFalconMailbox(SOE_ERROR_UNREACHABLE_CODE);
    //Shouldn't reach here
    OS_HALT();
}

GCC_ATTRIB_STACK_PROTECT()
void
hsMain()
{
    LwU32 val32 = 0;
    LwU32 saveSP = 0;
    LwU8  *ptmp8;
    SOE_STATUS status = SOE_OK;

    // Primary purpose of this Heavy Secure uCode is to verify the LS signatures
    // for DMEM and IMEM.  The DMEM and IMEM are already loaded.

    //
    // Protect against stack overflow.
    // Configure Falcon to trigger exception if stack grows
    // within 256 bytes of the end of our used memory.
    // This will not trigger exception until falc_wspr(SEC,0) is called
    val32 = 0;
    val32 = FLD_SET_DRF_NUM(_CSOE_FALCON, _STACKCFG, _BOTTOM, (((LwU32)_dataStackBottom))/4, val32);
    val32 = FLD_SET_DRF(_CSOE_FALCON, _STACKCFG, _SPEXC, _ENABLE, val32);
    CSB_REG_WR32(LW_CSOE_FALCON_STACKCFG, val32);

    // Save Stack Pointer, used later for Stack Scrub
    falc_rspr(saveSP, SP);

    // Re-enable Exceptions (HW auto-disables on security exception).
    falc_wspr(SEC, 0);

    // HS Entry Step #19
    if(!isValidChip())
    {
        hsFatalError(SOE_ERROR_HS_FATAL_FALCON_EXCEPTION_HS);
    }

    // HS Entry Step #20
    // uCode Revocation - prevent old uCode from running on newer silicon
    // (or newer security fuses).
    hsUcodeRevocation();

    // Confirm the variables are 16 Byte aligned.
    status = SOE_OK;
    if (!VAL_IS_ALIGNED((LwU32)gHsDmHash, SCP_SIG_SIZE_IN_BYTES)) {
      status = SOE_ERROR_HS_FATAL_ALIGN_FAIL1;
    }
    if (!VAL_IS_ALIGNED((LwU32)gHsTempBuffer, SCP_SIG_SIZE_IN_BYTES)) {
      status = SOE_ERROR_HS_FATAL_ALIGN_FAIL2;
    }
    if (!VAL_IS_ALIGNED((LwU32)gSignatureLsDmem, SCP_SIG_SIZE_IN_BYTES)) {
      status = SOE_ERROR_HS_FATAL_ALIGN_FAIL3;
    }
    if (!VAL_IS_ALIGNED((LwU32)gSignatureLsImem, SCP_SIG_SIZE_IN_BYTES)) {
      status = SOE_ERROR_HS_FATAL_ALIGN_FAIL4;
    }
    if (status != SOE_OK) {
      hsFatalError(status);
    }

    //
    // Configure Priv Level Masks
    //
    hsSetupPriv();

    // Do not rely on any DMEM contents prior to here.

    //
    // Verify LS Signatures for IMEM and DMEM
    //
    hsVerifyLsSignature(gSignatureLsDmem, (LwU8 *)appDmemBase,   (LwU32)appDmemImageSize,    MEMTYPE_DMEM);
    hsVerifyLsSignature(gSignatureLsImem, (LwU8 *)appImemLSBase, (LwU32)appImemLSSize,  MEMTYPE_IMEM);

    soeWriteStatusToFalconMailbox(SOE_BOOT_BINARY_LS_VERIFIED);

    //
    // TODO_Apoorvg: Update the LSMODE_LEVEL to 1 in follow up after confirming from Terrence
    // Set LSMODE
    //
    // Need to set just AUTH_EN before writing LSMODE.
    //
    val32 = CSB_REG_RD32(LW_CSOE_FALCON_SCTL);
    val32 = FLD_SET_DRF(_CSOE_FALCON, _SCTL, _AUTH_EN, _TRUE, val32);
    CSB_REG_WR32(LW_CSOE_FALCON_SCTL, val32);
    val32 = FLD_SET_DRF(_CSOE_FALCON, _SCTL, _LSMODE, _TRUE, val32);
    val32 = FLD_SET_DRF_NUM(_CSOE_FALCON, _SCTL, _LSMODE_LEVEL, 2, val32);
    CSB_REG_WR32(LW_CSOE_FALCON_SCTL, val32);

    // HS Exit Step #1
    falc_scp_forget_sig();

    clearSCPregisters();

    //
    // Ilwalidate unused IMEM
    //
    // Prevent malicious use of of the usused/unsigned IMEM space.
    //
    hsValidateImemBlocks();

    //
    //
    // Scrub DMEM
    //
    // Clear all bss DMEM, all unused DMEM upto __stackChkGuardEndLoc, plus STACK up to saveSP.
    // We need to preserve the random __stack_chk_guard for use in LS routines later.
    // We do not want to call a function here since we are clearing the stack.
    //
    val32 = saveSP - (LwU32)_stackChkGuardEndLoc;
    ptmp8 = (LwU8 *)_stackChkGuardEndLoc;
    while (val32--) {
      *ptmp8 = 0;
      ptmp8++;
    }
    // Roll Stack Pointer back to SP that we started at.
    falc_wspr(SP, saveSP);

    //
    // Configure Priv Level Masks
    //
    hsSetupPriv();

    //
    // Disable PRIV LOCKDOWN
    //
    // SCP controls PRIV access for FALCON/CSB also).
    val32 = CSB_REG_RD32(LW_CSOE_SCP_CTL_CFG);
    val32 = FLD_SET_DRF(_CSOE_SCP, _CTL_CFG, _LOCKDOWN, _DISABLE, val32);
    CSB_REG_WR32(LW_CSOE_SCP_CTL_CFG, val32);

    // SOE IMAGE's stack pointer could be different. It will set its own STACK_BOTTOM
    val32 = 0;
    val32 = FLD_SET_DRF(_CSOE_FALCON, _STACKCFG, _SPEXC, _DISABLE, val32);
    CSB_REG_WR32(LW_CSOE_FALCON_STACKCFG, val32);

    //
    // Ready to exit to LS uCode
    //
}

/*!
 * @brief   returns the GFW ucode revlock val from non fpf fuse
 * @param   none
 * @return  returns the non fpf fuse revlock val
 *
 */
static inline
LwU32
getUcodeRevFuseVal(void)
{
    LwU32 revLockFuseVal = 0;

    revLockFuseVal = soeBar0RegRd32_PlatformHS(LW_FUSE_OPT_FUSE_UCODE_SEC2_REV);

    revLockFuseVal = DRF_VAL(_FUSE_OPT_FUSE, _UCODE_SEC2_REV, _DATA, revLockFuseVal);

    return revLockFuseVal;
}

/*!
 * @brief   returns the GFW ucode revlock val from fpf block
 * @param   none
 * @return  returns the fpf revlock val
 *
 */
static inline
LwU32
getUcodeRevFpfVal(void)
{
    LwU32 revLockValFpf = 0;

    //
    // LW_FPF* defines have moved to LW_FUSE* defines in Ampere.
    // Bug 200435123
    //
    revLockValFpf = soeBar0RegRd32_PlatformHS(LW_FUSE_OPT_FPF_UCODE_SEC2_REV);
    revLockValFpf = DRF_VAL(_FUSE_OPT_FPF, _UCODE_SEC2_REV, _DATA, revLockValFpf);

    //
    // FPF rev bits are one-bit encoded so we need to get the highest set bit
    // getHighestSetBit returns bitpos starting with index 0 so increment by 1
    // if FPF rev fuse is non-zero.
    //
    if (revLockValFpf != 0)
    {
        HIGHESTBITIDX_32(revLockValFpf); // Macro modifies revLockValFpf
        revLockValFpf += 1;
    }

    return revLockValFpf;
}


/**
 * @brief Checks if code is running on the right hardware.
 * 
 * @return LwBool True if hardware is correct, false otherwise.
 */
static inline
LwBool
isValidChip(void)
{
  LwU32 falconFamily, falconInstId;

  //
  // Makes sure this is an LR10, Laguna Seca, etc.
  // This function's implementation varies by platform.
  // See the platform/ directory.
  //
  if(soeVerifyChipId_PlatformHS() == LW_FALSE)
  {
    return LW_FALSE;
  }

  // Make sure running on an SOE
  falconFamily = CSB_REG_RD32( LW_CSOE_FALCON_ENGID );
  falconFamily = DRF_VAL(_CSOE, _FALCON_ENGID, _FAMILYID, falconFamily);
  if (LW_CSOE_FALCON_ENGID_FAMILYID_SOE != falconFamily)
  {
    return LW_FALSE;
  }

  // Make sure this is SOE instance 0.
  falconInstId = CSB_REG_RD32( LW_CSOE_FALCON_ENGID );
  falconInstId = DRF_VAL(_CSOE, _FALCON_ENGID, _INSTID, falconInstId);
  if (0 != falconInstId)
  {
    return LW_FALSE;
  }

  // No errors.
  return  LW_TRUE;
}

GCC_ATTRIB_STACK_PROTECT()
void
hsUcodeRevocation()
{
    //
    // 1. Fuse version
    // Checks hardware fuses against fuse version in HS IMEM.
    // If LS image changes and requires a new fuse version, then:
    //    - Bump the required fuse version in the HS code
    //      (SOE_HS_BOOT_VER in drivers/common/inc/swref/lwswitch/lr10/lwFalconSigParams.mk)
    //    - Bump the HS SW version
    //    - Update the version of HS required by LS to the newer HS version
    //    - Release new LS and new HS together
    //
    // Note: For HS revocation we need to check 3 fuse registers. 
    // a) LW_FUSE_OPT_FUSE_UCODE_SEC2_REV - Not Field Programmable. 
    // b) LW_FUSE_OPT_FPF_UCODE_SEC2_REV - Field Programmable fuse. 
    // c) LW_FUSE_OPT_FPF_SEC2_UCODE1_VERSION - Field Programmable, Checked by PKC BootRom. 
    //      If this check fails then HS binary will be rejected, so not adding this check in HS code. 
    // 

    LwU32 fuseVersionSW = soeGetRevocationUcodeVersionSW_PlatformHS();
    LwU32 fuseVersionHW = getUcodeRevFuseVal();
    LwU32 fpfVersionHW  = getUcodeRevFpfVal();

    if ((fuseVersionSW < fpfVersionHW) ||
        (fuseVersionSW < fuseVersionHW))
    {
        // Check b) above failed
        hsFatalError(SOE_ERROR_HS_FATAL_FALCON_EXCEPTION_HS);
    }
  
    // 2. If DEBUG_MODE disabled and PRIV_SEC_EN disabled, HS Fatal error.
    if (!isDebugModeEnabled())
    {
        if (FLD_TEST_DRF( _FUSE, _OPT_PRIV_SEC_EN, _DATA, _NO, soeBar0RegRd32_PlatformHS(LW_FUSE_OPT_PRIV_SEC_EN)))
        {
            hsFatalError(SOE_ERROR_HS_FATAL_FALCON_EXCEPTION_HS);
        }
    }

    //
    // SECURITY CHECK: confirm HS IMEM and LS DMEM are compatible.
    // This prevents adversary from running old LS with new HS, or vice versa.
    //
    // Check compiled version of HS ucode (HS IMEM) vs. what is stored in DMEM.
    // HS uCode may not be re-signed for every ucode released.
    // HS uCode Version needs to be incremented everytime we have new HS sig.
    // Version compatibility between LS DMEM/IMEM is part of their signatures.
    //
    if (gRevUcodeHs != LWL_REV_UCODE_HS) 
    {
        hsFatalError(SOE_ERROR_HS_FATAL_REV_HS_MISMATCH);
    }
}

GCC_ATTRIB_STACK_PROTECT()
void
hsFatalError
(
    SOE_STATUS status
)
{
  soeWriteStatusToFalconMailbox(status);
  
  //
  // Clear all General Purpose Regs to eliminate chance of
  // exposing any secure information.
  //
  clearFalconGprs();
  OS_HALT();
}

GCC_ATTRIB_STACK_PROTECT()
void
hsVerifyLsSignature
(
    LwU8 *pSig,
    LwU8 *pMemStart,
    LwU32 memSize,
    LwU32 memType
)
{
  SOE_STATUS status = SOE_OK;

  // Callwlate the hash.
  hsMemClear(gHsDmHash, SCP_SIG_SIZE_IN_BYTES);
  hsCallwlateHash(gHsDmHash, pMemStart, memSize, memType);

  //
  // UCODE VERSION appended to both DMEM and IMEM image to ensure we
  // are using compatible images from the same UCODE VERSION.
  // This is the release version and is NOT the same as the FUSE UCODE VERSION.
  // This version is incremented EVERY release.
  // Note: the version is in DMEM thus MEMTYPE_DMEM.
  //
  hsMemClear(gHsTempBuffer, SCP_SIG_SIZE_IN_BYTES);
  ((LwU32*)gHsTempBuffer)[0] = gRevUcodeOverall;
  hsCallwlateHash(gHsDmHash, gHsTempBuffer, SCP_SIG_SIZE_IN_BYTES, MEMTYPE_DMEM);

  // Callwlate LS signature and compare to expected.
  hsCallwlateSignature(gHsTempBuffer, gHsDmHash);
  if (hsMemCompare(gHsDmHash, pSig, SCP_SIG_SIZE_IN_BYTES)) {
    status = (IS_IMEM(memType)) ? SOE_ERROR_HS_FATAL_SIGFAIL_IMEM : SOE_ERROR_HS_FATAL_SIGFAIL_DMEM;
    hsFatalError(status);
  }
}

GCC_ATTRIB_STACK_PROTECT()
void
hsCallwlateHash
(
    LwU8 *pHash,
    LwU8 *pData,
    LwU32 size,
    LwU32 memtype
)
{
   LwU32  doneSize = 0;
 
   while (doneSize < size) {
       // H_i = H_{i-1} XOR encrypt(key = m_i, pMessage = H_{i-1})
       if (IS_IMEM(memtype)) {
         falc_scp_trap_imem(TC_INFINITY);
       } else {
         falc_scp_trap(TC_INFINITY);
       }
       falc_trapped_dmwrite(falc_sethi_i((LwU32)pData, SCP_R2));
       falc_dmwait();

       // If IMEM, switch back to using DMEM for callwlating the hash.
       if (IS_IMEM(memtype)) {
         falc_scp_trap(TC_INFINITY);
       }

       falc_trapped_dmwrite(falc_sethi_i((LwU32)pHash, SCP_R3));
       falc_dmwait();
       falc_scp_key(SCP_R2);
       falc_scp_encrypt(SCP_R3, SCP_R4);
       falc_scp_xor(SCP_R4, SCP_R3);
       falc_trapped_dmread(falc_sethi_i((LwU32)pHash, SCP_R3));
       falc_dmwait();
       falc_scp_trap(0);
 
       doneSize += SCP_SIG_SIZE_IN_BYTES;
       pData    += SCP_SIG_SIZE_IN_BYTES;
   }
}

GCC_ATTRIB_STACK_PROTECT()
void
hsCallwlateSignature
(
    LwU8 *pSaltBuf,
    LwU8 *pDmHash
)
{
    LwU32 val32;

    //
    // derived key = AES-ECB((g_kdfSalt XOR falconID), key)
    // Signature = AES-ECB(dmhash, derived key)
    //

    // pSaltBuf and pDmHash inputs must be 16B aligned

    // Write the pre-determined Salt.
    pSaltBuf[0]  = 0xB6;
    pSaltBuf[1]  = 0xC2;
    pSaltBuf[2]  = 0x31;
    pSaltBuf[3]  = 0xE9;
    pSaltBuf[4]  = 0x03;
    pSaltBuf[5]  = 0xB2;
    pSaltBuf[6]  = 0x77;
    pSaltBuf[7]  = 0xD7;
    pSaltBuf[8]  = 0x0E;
    pSaltBuf[9]  = 0x32;
    pSaltBuf[10] = 0xA0;
    pSaltBuf[11] = 0x69;
    pSaltBuf[12] = 0x8F;
    pSaltBuf[13] = 0x4E;
    pSaltBuf[14] = 0x80;
    pSaltBuf[15] = 0x62;

    // XOR FALCONID with the first DWORD of SALT
    ((LwU32*)pSaltBuf)[0] ^= LS_FALCON_ID;

    falc_scp_trap(TC_INFINITY);
    // Load XORed salt
    falc_trapped_dmwrite(falc_sethi_i((LwU32)(pSaltBuf), SCP_R3));
    falc_dmwait();
    // Load the keys accordingly
    val32 = CSB_REG_RD32(LW_CSOE_SCP_CTL_STAT);
    if (FLD_TEST_DRF(_CSOE_SCP, _CTL_STAT, _DEBUG_MODE, _DISABLED, val32)) 
    {
        falc_scp_secret(LS_VERIF_KEY_INDEX_PROD, SCP_R2);
    } else 
    {
        falc_scp_secret(LS_VERIF_KEY_INDEX_DEBUG, SCP_R2);
    }
    falc_scp_key(SCP_R2);
    // Derived key in R4
    falc_scp_encrypt(SCP_R3, SCP_R4);
    // Load the hash
    falc_trapped_dmwrite(falc_sethi_i((LwU32)pDmHash, SCP_R3));
    falc_dmwait();
    falc_scp_key(SCP_R4);
    // Signature in R2
    falc_scp_encrypt(SCP_R3, SCP_R2);
    // Read back the signature into DMEM.
    falc_trapped_dmread(falc_sethi_i((LwU32)pDmHash, SCP_R2));
    falc_dmwait();
    falc_scp_trap(0);
    
    clearSCPregisters();
}

GCC_ATTRIB_STACK_PROTECT()
void
hsValidateImemBlocks(void)
{

  LwU32 startImage       = (LWL_FALCON_PC_TRIM((LwU32)appImemLSBase));
  LwU32 endImage         = (LWL_FALCON_PC_TRIM((startImage + (LwU32)appImemLSSize)));

  LwU32 tmp          = 0;
  LwU32 imemBlks     = 0;
  LwU32 blockInfo    = 0;
  LwU32 lwrrAddr     = 0;

  //
  // Loop through all IMEM blocks and ilwalidate those that doesnt fall within
  // our Image or boot binaries range
  //
  // Read the total number of IMEM blocks
  //
  tmp = CSB_REG_RD32(LW_CSOE_FALCON_HWCFG);
  imemBlks = DRF_VAL(_CSOE_FALCON, _HWCFG, _IMEM_SIZE, tmp);

  for (tmp = 0; tmp < imemBlks; tmp++)
  {
      falc_imblk(&blockInfo, tmp);

      // Valid and non-secure
      if (!(IMBLK_IS_ILWALID(blockInfo)) && !(IMBLK_IS_SELWRE(blockInfo)))
      {
          lwrrAddr = IMBLK_TAG_ADDR(blockInfo);

          // Ilwalidate the block if it is not a part of SOE Image
          if ((lwrrAddr < startImage || lwrrAddr >= endImage))
          {
              falc_imilw(tmp);
          }
      }
  }
}

GCC_ATTRIB_STACK_PROTECT()
void*
hsMemCopy
(
    void *pDst,
    const void *pSrc,
    LwU32 sizeInBytes
)
{
  LwU8       *pDstC = (LwU8 *) pDst;
  const LwU8 *pSrcC = (LwU8 *) pSrc;
  while (sizeInBytes--) {
    *pDstC++ = *pSrcC++;
  }
  return pDst;
}

GCC_ATTRIB_STACK_PROTECT()
void
hsMemClear
(
    void *pStart,
    LwU32 sizeInBytes
)
{
  LwU8 *pLoc = (LwU8 *) pStart;
  while (sizeInBytes--) {
    *pLoc = 0;
    pLoc++;
  }
}

GCC_ATTRIB_STACK_PROTECT()
LwU32
hsMemCompare
(
    void *pA,
    void *pB,
    LwU32 sizeInBytes
)
{
  LwU8 *pLocA = (LwU8 *) pA;
  LwU8 *pLocB = (LwU8 *) pB;
  LwU32 retVal = 0;

  while (sizeInBytes--) {
    retVal |= (*pLocA ^*pLocB);
    pLocA++;
    pLocB++;
  }
  return(retVal);
}

GCC_ATTRIB_STACK_PROTECT()
void
hsSetupPriv()
{
  LwU32 plmRdNoneWrNone;
  LwU32 plmRdL2WrL2;
  LwU32 plmRdAllWrL2;
  LwU32 plmRdAllWrNone;

  // TODO_apoorvg: Figure out the other priv level masks for SOE and add them here

  //
  // Note: This code builds mask using IMEM_PRIV_LEVEL_MASK reg definition.
  //       This obviously assumes all MASKs follow a standard as they do today.
  //       This was chosen as alternative to having lots of repeated code for each MASK.
  //
  // Note: ENABLE means ACCESS ENABLED at PL.
  //
  plmRdNoneWrNone = 0;
  plmRdNoneWrNone = FLD_SET_DRF(_CSOE_FALCON, _IMEM_PRIV_LEVEL_MASK, _READ_PROTECTION,  _ALL_LEVELS_DISABLED, plmRdNoneWrNone);
  plmRdNoneWrNone = FLD_SET_DRF(_CSOE_FALCON, _IMEM_PRIV_LEVEL_MASK, _READ_VIOLATION,   _REPORT_ERROR,        plmRdNoneWrNone);
  plmRdNoneWrNone = FLD_SET_DRF(_CSOE_FALCON, _IMEM_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ALL_LEVELS_DISABLED, plmRdNoneWrNone);
  plmRdNoneWrNone = FLD_SET_DRF(_CSOE_FALCON, _IMEM_PRIV_LEVEL_MASK, _WRITE_VIOLATION,  _REPORT_ERROR,        plmRdNoneWrNone);

  plmRdL2WrL2 = 0;
  plmRdL2WrL2 = FLD_SET_DRF(_CSOE_FALCON, _IMEM_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL0,  _DISABLE,      plmRdL2WrL2);
  plmRdL2WrL2 = FLD_SET_DRF(_CSOE_FALCON, _IMEM_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL1,  _DISABLE,       plmRdL2WrL2);
  plmRdL2WrL2 = FLD_SET_DRF(_CSOE_FALCON, _IMEM_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL2,  _ENABLE,      plmRdL2WrL2);
  plmRdL2WrL2 = FLD_SET_DRF(_CSOE_FALCON, _IMEM_PRIV_LEVEL_MASK, _READ_VIOLATION,          _REPORT_ERROR, plmRdL2WrL2);
  plmRdL2WrL2 = FLD_SET_DRF(_CSOE_FALCON, _IMEM_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE,      plmRdL2WrL2);
  plmRdL2WrL2 = FLD_SET_DRF(_CSOE_FALCON, _IMEM_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE,       plmRdL2WrL2);
  plmRdL2WrL2 = FLD_SET_DRF(_CSOE_FALCON, _IMEM_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _ENABLE,      plmRdL2WrL2);
  plmRdL2WrL2 = FLD_SET_DRF(_CSOE_FALCON, _IMEM_PRIV_LEVEL_MASK, _WRITE_VIOLATION,         _REPORT_ERROR, plmRdL2WrL2);

  plmRdAllWrNone = 0;
  plmRdAllWrNone = FLD_SET_DRF(_CSOE_FALCON, _IMEM_PRIV_LEVEL_MASK, _READ_PROTECTION,  _ALL_LEVELS_ENABLED,  plmRdAllWrNone);
  plmRdAllWrNone = FLD_SET_DRF(_CSOE_FALCON, _IMEM_PRIV_LEVEL_MASK, _READ_VIOLATION,   _REPORT_ERROR,        plmRdAllWrNone);
  plmRdAllWrNone = FLD_SET_DRF(_CSOE_FALCON, _IMEM_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ALL_LEVELS_DISABLED, plmRdAllWrNone);
  plmRdAllWrNone = FLD_SET_DRF(_CSOE_FALCON, _IMEM_PRIV_LEVEL_MASK, _WRITE_VIOLATION,  _REPORT_ERROR,        plmRdAllWrNone);

  plmRdAllWrL2 = 0;
  plmRdAllWrL2 = FLD_SET_DRF(_CSOE_FALCON, _IMEM_PRIV_LEVEL_MASK, _READ_PROTECTION,         _ALL_LEVELS_ENABLED,  plmRdAllWrL2);
  plmRdAllWrL2 = FLD_SET_DRF(_CSOE_FALCON, _IMEM_PRIV_LEVEL_MASK, _READ_VIOLATION,          _REPORT_ERROR,        plmRdAllWrL2);
  plmRdAllWrL2 = FLD_SET_DRF(_CSOE_FALCON, _IMEM_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE,             plmRdAllWrL2);
  plmRdAllWrL2 = FLD_SET_DRF(_CSOE_FALCON, _IMEM_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE,             plmRdAllWrL2);
  plmRdAllWrL2 = FLD_SET_DRF(_CSOE_FALCON, _IMEM_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _ENABLE,              plmRdAllWrL2);
  plmRdAllWrL2 = FLD_SET_DRF(_CSOE_FALCON, _IMEM_PRIV_LEVEL_MASK, _WRITE_VIOLATION,         _REPORT_ERROR,        plmRdAllWrL2);

  // Masks that will not allow any read or write (only PL3 can read or write).
  CSB_REG_WR32(LW_CSOE_FALCON_DMEM_PRIV_LEVEL_MASK, plmRdNoneWrNone);

  // Masks that will allow read write only by Level2 code (SOE).
  CSB_REG_WR32(LW_CSOE_FALCON_WDTMR_PRIV_LEVEL_MASK, plmRdL2WrL2);

  // Masks that will allow write only by Level2 code (SOE), but read by any.
  CSB_REG_WR32(LW_CSOE_FALCON_IRQTMR_PRIV_LEVEL_MASK, plmRdAllWrL2);
  CSB_REG_WR32(LW_CSOE_FALCON_EXE_PRIV_LEVEL_MASK, plmRdAllWrL2);

  //
  // Masks that will allow read by any level, but no writes allowed.
  // We want IMEM read access to help with debug (no secrets in IMEM).
  //
  CSB_REG_WR32(LW_CSOE_FALCON_IMEM_PRIV_LEVEL_MASK, plmRdAllWrNone);
  CSB_REG_WR32(LW_CSOE_FALCON_CPUCTL_PRIV_LEVEL_MASK, plmRdAllWrNone);
  CSB_REG_WR32(LW_CSOE_FALCON_MTHDCTX_PRIV_LEVEL_MASK, plmRdAllWrNone);
  CSB_REG_WR32(LW_CSOE_FALCON_HSCTL_PRIV_LEVEL_MASK, plmRdAllWrNone);
  CSB_REG_WR32(LW_CSOE_FALCON_SCTL_PRIV_LEVEL_MASK, plmRdAllWrNone);
}

