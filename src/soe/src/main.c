/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file main.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "rmsoecmdif.h"
#include "lwostimer.h"

/* ------------------------- Application Includes --------------------------- */
#include "soesw.h"
#include "main.h"
#include "soe_cmdmgmt.h"
#include "soe_objsoe.h"
#include "soe_objic.h"
#include "soe_objlsf.h"
#include "soe_objtimer.h"
#include "soe_objrttimer.h"
#include "soe_objtherm.h"
#include "soe_objbif.h"
#include "soe_objsmbpbi.h"
#include "soe_objsaw.h"
#include "soe_objgin.h"
#include "soe_objintr.h"
#include "soe_objlwlink.h"
#include "soe_objpmgr.h"
#include "soe_obji2c.h"
#include "soe_objgpio.h"
#include "soe_objspi.h"
#include "soe_objifr.h"
#include "soe_objcore.h"
#include "soe_bar0.h"
#include "soe_device.h"
#include "scp_rand.h"
#include "scp_internals.h"
#include "config/g_hal_register.h"
#include "config/g_ic_private.h"
#include "config/g_therm_private.h"
#include "config/g_bif_private.h"
#include "config/g_core_private.h"
#include "config/g_saw_private.h"
#include "config/g_gin_private.h"
#include "config/g_intr_private.h"
#include "config/g_lwlink_private.h"
#include "config/g_pmgr_private.h"
#include "config/g_i2c_private.h"
#include "config/g_gpio_private.h"
#include "config/g_spi_private.h"
#include "config/g_ifr_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

#define SCP_REG_SIZE_IN_DWORDS      0x00000004
#define SCP_ADDR_ALIGNMENT_IN_BYTES 0x00000010

/* ------------------------- Global Variables ------------------------------- */
/*!
 * Handles for all tasks spawned by the SOE application.  Each is named using
 * a common-prefix for colwenient symbol look-up.
 */
LwrtosTaskHandle OsTaskCmdMgmt       = NULL;
LwrtosTaskHandle OsTaskTherm         = NULL;
LwrtosTaskHandle OsTaskBif           = NULL;
LwrtosTaskHandle OsTaskSmbpbi        = NULL;
LwrtosTaskHandle OsTaskSpi           = NULL;
LwrtosTaskHandle OsTaskCore          = NULL;
LwrtosTaskHandle OsTaskIfr           = NULL;

/*!
 * A dispatch queue for sending commands to the command management task
 */
LwrtosQueueHandle  SoeCmdMgmtCmdDispQueue;

#ifdef IS_SSP_ENABLED
/*!
 * Stack Canary for Stack Smash Protection
 */
void *__stack_chk_guard = (void *) 0;

/*!
 * Stack Canary from Caller
 */
void *canaryFromCaller = (void *) 0;

/*!
 * Scratch space for Canary callwlation
 */
static LwU32 prLsScratchPad[SCP_REG_SIZE_IN_DWORDS]
    GCC_ATTRIB_ALIGN(SCP_ADDR_ALIGNMENT_IN_BYTES);
#endif // IS_SSP_ENABLED

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
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

/* ------------------------- Implementation --------------------------------- */

/*!
 * We have two flags to enable HS ucode encryption (one is a compile time flag
 * used everywhere, and one is a flag sent to the linker script generator).
 * If the flag to generate the linker script symbols is set, then so should the
 * compile-time flag that enables all the code that uses the symbols, else it
 * results in resident data waste. Catch that case here to remind engineers that
 * either both must be set or both must be not set.
 * Setting just HS_UCODE_ENCRYPTION without HS_UCODE_ENCRYPTION_LINKER_SYMBOLS
 * is not going to work anyway. Compilation/link will fail if we don't generate
 * the symbols required for it.
 */
#if HS_UCODE_ENCRYPTION_LINKER_SYMBOLS
#ifndef HS_UCODE_ENCRYPTION
ct_assert(0);
#endif
#endif


/********************************** SCP RNG code ************************************/
#ifdef IS_SSP_ENABLED

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

    val32 = BAR0_REG_RD32(LW_PSMC_BOOT_2);
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
InitializeStackCanary_LS(void)
{
    LwBool bFound = LW_FALSE;
    LwU32 i;

    initScpRng();

    // Set the value of stack canary to a random value to ensure adversary cannot craft an attack by looking at the
    // assembly to determine the canary value.
    do
    {
        if (get128BitRandNum(prLsScratchPad) == FLCN_OK)
        {
            // Make sure value of random number is not all 0s or all 1s as these
            // might be values an adversary may think of trying
            for (i = 0; i < SCP_REG_SIZE_IN_DWORDS; i++)
            {
                if (prLsScratchPad[i] != 0 && prLsScratchPad[i] != 0xffffffff)
                {
                    __stack_chk_guard = (void *)(prLsScratchPad[i]);
                    bFound = LW_TRUE;
                    break;
                }
            }
        }
        else
        {
            //Fatal error if 128 bit random number generation failed
            REG_WR32_STALL(CSB, SOE_STATUS_SCRATCH_REGISTER, SOE_ERROR_LS_STACK_CHECK_SETUP_FAILED);
            clearFalconGprs();
            OS_HALT();
        }
    } while (!bFound);

    //Scrub the local variable as a good security practice
    prLsScratchPad[0] = 0;
    prLsScratchPad[1] = 0;
    prLsScratchPad[2] = 0;
    prLsScratchPad[3] = 0;
}


static inline void
saveStackCanary_LS(void)
{
    canaryFromCaller = __stack_chk_guard;
}

static inline void
restoreStackCanary_LS(void)
{
    __stack_chk_guard = canaryFromCaller;
}

//
//stack_chk_fail cannot be inlined, since compiler calls this function after
//all the inlining optimization is already done.
//
void  __stack_chk_fail(void)
    GCC_ATTRIB_NOINLINE();

//Disable GCC attribute to disable stack protection on this function
__attribute__((no_stack_protect))
void __stack_chk_fail()
{
    // Update the error to SOE MAILBOX register
    REG_WR32_STALL(CSB, SOE_STATUS_SCRATCH_REGISTER, SOE_ERROR_LS_STACK_CHECK_FAILED);
    // HALT to prevent any further code exelwtion.
    SOE_HALT();
}
#endif // IS_SSP_ENABLED

/*!
 * Main entry-point for the RTOS application.
 *
 * @param[in]  argc    Number of command-line arguments (always 1)
 * @param[in]  ppArgv  Pointer to the command-line argument structure
 *
 * @return zero upon success; non-zero negative number upon failure
 */
int
main
(
    int    argc,
    char **ppArgv
)
{
    // TODO: Ilwalidate bootloader later
    /*

    LwU32   blk;
    LwU32   imemBlks = 0;

    // Read the total number of IMEM blocks
    imemBlks = REG_RD32(CSB, LW_CSOE_FALCON_HWCFG);
    imemBlks = DRF_VAL(_CSOE_FALCON, _HWCFG, _IMEM_SIZE, imemBlks);

    // Ilwalidate bootloader. It may not be at the default block
    falc_imtag(&blk, SOE_LOADER_OFFSET);
    if (!IMTAG_IS_MISS(blk))
    {
        LwU32 tmp;
        blk = IMTAG_IDX_GET(blk);

        // Bootloader might take more than a block, so ilwalidate those as well.
        for (tmp = blk; tmp < imemBlks; tmp++)
        {
            falc_imilw(tmp);
        }
    }
    */
#ifdef IS_SSP_ENABLED
    saveStackCanary_LS();
    InitializeStackCanary_LS();
#endif // IS_SSP_ENABLED

    InitSoeApp();

#ifdef IS_SSP_ENABLED
    restoreStackCanary_LS();
#endif // IS_SSP_ENABLED

    return 0;
}

/*!
 * Initializes the SOE RTOS application.  This function is responsible for
 * creating all tasks, queues, mutexes, and semaphores.
 */
void
InitSoeApp(void)
{
    FLCN_STATUS status;

    //
    // Disable all Falcon interrupt sources.  This must be done in case
    // interrupts (on IV0 or IV1) are enabled between now and when the
    // scheduler is enabled.  This is avoid scenarios where the OS timer tick
    // fires on IV1 before the scheduler is started or a GPIO interrupt fires
    // on IV0 before the IV0 interrupt handler is installed.
    //
    REG_WR32_STALL(CSB, LW_CSOE_FALCON_IRQMCLR, 0xFFFFFFFF);

    //
    // Unmask the halt interrupt to host early to detect an init failure before
    // icPreInit_HAL is exelwted
    //
    icHaltIntrUnmask_HAL();

    // Initialize the LWOS specific data.
    lwosInit();

    //
    // Enable all HAL modules.  Calling this function prepares each HAL module's
    // interface-setup functions. This must be done prior to calling the
    // constructor for each module.
    //
    REGISTER_ALL_HALS();

    if (!soeVerifyChip_HAL())
    {
        SOE_HALT();
        return;
    }

    // Update the status to SOE MAILBOX register
    REG_WR32_STALL(CSB, SOE_STATUS_SCRATCH_REGISTER, SOE_RTOS_STARTED);

    //
    // Now construct all objects and HAL modules. This will setup all of the
    // HAL function pointers. Note that not all engines are enabled by default
    // so checks must be made for some engines to determine if construction is
    // appropriate.
    //

    // SOE engine is always enabled

    constructSoe();

    soeEnableEmemAperture_HAL();

    //
    // Then finish our preinit sequence before other engines are constructed or
    // initialized.
    //
    soePreInit_HAL();

    if (SOECFG_ENGINE_ENABLED(IC))
    {
        constructIc();
    }

    if (SOECFG_ENGINE_ENABLED(THERM))
    {
        constructTherm();
    }

    if (SOECFG_ENGINE_ENABLED(BIF))
    {
        constructBif();
    }

    if (SOECFG_ENGINE_ENABLED(LSF))
    {
        constructLsf();
    }

    if (SOECFG_ENGINE_ENABLED(SAW))
    {
        constructSaw();
    }

    if (SOECFG_ENGINE_ENABLED(GIN))
    {
        constructGin();
    }

    if (SOECFG_ENGINE_ENABLED(INTR))
    {
        constructIntr();
    }

    if (SOECFG_ENGINE_ENABLED(LWLINK))
    {
        constructLwlink();
    }

    if (SOECFG_ENGINE_ENABLED(PMGR))
    {
        constructPmgr();
    }

    if (SOECFG_ENGINE_ENABLED(I2C))
    {
        constructI2c();
    }

    if (SOECFG_ENGINE_ENABLED(GPIO))
    {
        constructGpio();
    }

    if (SOECFG_ENGINE_ENABLED(SPI))
    {
        constructSpi();
    }

    if (SOECFG_ENGINE_ENABLED(CORE))
    {
        constructCore();
    }

    if (SOECFG_ENGINE_ENABLED(BIOS))
    {
        constructBios();
    }

    if (SOECFG_ENGINE_ENABLED(SMBPBI))
    {
        constructSmbpbi();
    }

    if (SOECFG_ENGINE_ENABLED(TIMER))
    {
        constructTimer();
    }

    if (SOECFG_ENGINE_ENABLED(IFR))
    {
        constructIfr();
    }

    //
    // initialize falc-debug to write print data to corresponding register
    // falc_debug tool to decode print logs from the soe falcon spew is
    // at //sw/dev/gpu_drv/chips_a/uproc/soe/tools/flcndebug/falc_debug
    //
    if (SOECFG_FEATURE_ENABLED(DBG_PRINTF_FALCDEBUG))
    {
        soeFalcDebugInit_HAL();
    }

    DBG_PRINTF(("InitSoeApp\n", 0, 0, 0, 0));

    //
    // Create command mgmt/dispatcher task.  This task is NOT optional.  It
    // must be alive to forward commands to all tasks that are running.
    //
    OSTASK_CREATE(status, SOE, CMDMGMT);
    if (FLCN_OK != status)
    {
        goto InitSoeApp_exit;
    }

    status = lwrtosQueueCreateOvlRes(&SoeCmdMgmtCmdDispQueue, 4,
                                     sizeof(DISPATCH_CMDMGMT));
    if (FLCN_OK != status)
    {
        goto InitSoeApp_exit;
    }

    //Create Thermal Task
    if (SOECFG_ENGINE_ENABLED(THERM))
    {
        OSTASK_CREATE(status, SOE, THERM);
        status = lwrtosQueueCreateOvlRes(&Disp2QThermThd, 4, sizeof(DISP2UNIT_CMD));
        if (FLCN_OK != status)
        {
            goto InitSoeApp_exit;
        }
    }

    //Create Bif Task
    if (SOECFG_ENGINE_ENABLED(BIF))
    {
        OSTASK_CREATE(status, SOE, BIF);
        status = lwrtosQueueCreateOvlRes(&Disp2QBifThd, 4, sizeof(DISP2UNIT_CMD));
        if (FLCN_OK != status)
        {
            goto InitSoeApp_exit;
        }
    }

    //Create SMBPBI Task
    if (SOECFG_ENGINE_ENABLED(SMBPBI))
    {
        OSTASK_CREATE(status, SOE, SMBPBI);
        status = lwrtosQueueCreateOvlRes(&Disp2QSmbpbiThd, 2, sizeof(DISP2UNIT_CMD));
        if (FLCN_OK != status)
        {
            goto InitSoeApp_exit;
        }
    }

    if (SOECFG_ENGINE_ENABLED(SPI))
    {
        OSTASK_CREATE(status, SOE, SPI);
        status = lwrtosQueueCreateOvlRes(&Disp2QSpiThd, 4, sizeof(DISP2UNIT_CMD));
        if (FLCN_OK != status)
        {
            goto InitSoeApp_exit;
        }
    }

    if (SOECFG_ENGINE_ENABLED(CORE))
    {
        OSTASK_CREATE(status, SOE, CORE);
        status = lwrtosQueueCreateOvlRes(&Disp2QCoreThd, 4, sizeof(DISP2UNIT_CMD));
        if (FLCN_OK != status)
        {
            goto InitSoeApp_exit;
        }
    }

    if (SOECFG_ENGINE_ENABLED(IFR))
    {
        OSTASK_CREATE(status, SOE, IFR);
        status = lwrtosQueueCreateOvlRes(&Disp2QIfrThd, 4, sizeof(DISP2UNIT_CMD));
        if (FLCN_OK != status)
        {
            goto InitSoeApp_exit;
        }
    }

    osCmdmgmtPreInit();

    // Set the mask used to identify the timer interrupt for OS ticks
    lwrtosFlcnSetTickIntrMask(icOsTickIntrMaskGet_HAL());

    //
    // Interrupts may have been enabled above (safe since interrupts have
    // been masked). Be sure to disable interrupts before unmasking the
    // interrupt sources. This will NOT have a matching lwrtosEXIT_CRITICAL
    // call. Interrupts will be enabled when the scheduler starts and
    // restores the context of the first task it picks to run. Until the
    // scheduler starts, only lwrtosENTER_CRITICAL or lwrtosEXIT_CRITICAL may be
    // used; otherwise, interrupts will be enabled again.
    //
    lwrtosENTER_CRITICAL();

    if (SOECFG_ENGINE_ENABLED(INTR))
    {
        intrPreInit_HAL();
    }

    if (SOECFG_ENGINE_ENABLED(THERM))
    {
        thermPreInit_HAL();
    }

    if (SOECFG_ENGINE_ENABLED(IC))
    {
        icPreInit_HAL();
    }

    if (SOECFG_ENGINE_ENABLED(LSF))
    {
        lsfPreInit();
    }

    if (SOECFG_ENGINE_ENABLED(TIMER))
    {
        timerPreInit();
    }

    if (SOECFG_ENGINE_ENABLED(RTTIMER))
    {
        rttimerPreInit();
    }

    if (SOECFG_ENGINE_ENABLED(LWLINK))
    {
        lwlinkPreInit_HAL();
    }

    if (SOECFG_ENGINE_ENABLED(PMGR))
    {
        pmgrPreInit_HAL();
    }

    if (SOECFG_ENGINE_ENABLED(I2C))
    {
        i2cPreInit_HAL();
    }

    if (SOECFG_ENGINE_ENABLED(GPIO))
    {
        gpioPreInit_HAL();
    }

    if (SOECFG_ENGINE_ENABLED(SPI))
    {
        spiPreInit_HAL();
    }

    if (SOECFG_ENGINE_ENABLED(IFR))
    {
        ifrPreInit_HAL();
    }

    if (SOECFG_ENGINE_ENABLED(SMBPBI))
    {
        smbpbiPreInit();
    }

    if (SOECFG_ENGINE_ENABLED(CORE))
    {
        corePreInit();
    }

    if (SOECFG_ENGINE_ENABLED(BIOS))
    {
        biosPreInit_HAL();
    }

    if (SOECFG_ENGINE_ENABLED(BIF))
    {
        bifInitMarginingIntr_HAL();
    }
  
    // start the scheduler's timer
    soeStartOsTicksTimer();

InitSoeApp_exit:

    if (FLCN_OK == status)
    {
        // Start the scheduler.
        status = lwrtosTaskSchedulerStart();
    }

    if (FLCN_OK != status)
    {
        //
        // TODO: Find a way to communicate to the RM (even behind the message
        // queue's back) that the falcon has failed to initialize itself and
        // start the task scheduler.
        //
        SOE_HALT();
    }
}
