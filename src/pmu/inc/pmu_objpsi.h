/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJPSI_H
#define PMU_OBJPSI_H

/*!
 * @file pmu_objpsi.h manages Power Saving Feature Coupled PSI
 *
 * "Power Saving Feature Coupled PSI"
 *
 * Phase State Indicator (PSI) is a feature of the voltage controller which
 * allows us to dynamically switch from multiphase to single phase mode to
 * improve regulator efficiency. Improvements in Regulator efficiency results
 * in power saving.
 *
 * Why do we need to Couple PSI with other power saving feature?
 * Single phase mode of regulator gives power benefits only if LWVDD current
 * is less than HW defined limit called Ioptimal_1phase OR Ioptimal_1phase
 * current. If LWVDD current is greater than Ioptimal_1phase then regulator
 * will consume more power in Single Phase mode. P8 doesn't ensures the LWVDD
 * current is less than Ioptimal_1phase on most of Notebook GPUs. Thus we
 * decided to couple PSI with other power saving feature to ensure that PSI
 * will get kicked-in only when LWVDD current is less than Ioptimal_1phase.
 *
 * This feature worked stably on Notebook thus we extended it for Desktop GPUs.
 *
 * Power Saving Feature Coupled PSI Infrastructure:
 * Idea is to facilitate any power feature to engage PSI. We have multiple
 * power saving features that wants to engage PSI. OBJPSI provides
 * infrastructure to engage/disengage PSI with any power feature. Current plan
 * is to couple PSI with GR-ELPG, MSCG, DI.
 *
 * OBJPSI provides following APIs:
 * - psiEngage(ctrlId, stepMask):
 *   To engage PSI with power feature identified by "ctrlId". Power feature
 *   can engage PSI only if PSI is not already engaged by another power
 *   feature.
 *
 * - psiDisengage(ctrlId, stepMask):
 *   To dis-engage PSI.  Power feature identified by "ctrlId" can disengage
 *   PSI only if it was engaged by "ctrlId"power feature.
 *
 *   NOTE : Enabling PSI means allowing power feature to engage PSI in its
 *   upcoming successful entry cycles.
 *
 * OBJPSI provides facility to partially execute PSI entry/exit sequence. This
 * help us-
 * 1) To handle special cases in MSCG-coupled-PSI.
 *    E.g. As per design of MSCG-coupled-PSI, PMU keeps GPIO mutex when GPU is
 *    in MSCG. That means, MSCG sequence will not release GPIO mutex after
 *    engaging PSI.
 *
* 2) To parallelize power feature's exit sequence with PSI exit sequence.
 *    PSI entry/exit sequence has some waits. E.g. GPIO flush timeout, Voltage
 *    settle time. Power feature can run their entry/exit sequence during these
 *    waits.
 *
 * SW RESTRICTION:
 *    We don't have use case where power feature needs to parallelize PSI entry
 *    sequence with its own entry sequence. Thus, psiEngage() API DOES NOT
 *    provides this facility power feature has to engage PSI in one stroke. It
 *    always safe to engage PSI after completing entry sequence of power feature.
 *
 *    psiDisngage() API provides the facility of parallelizing PSI exit
 *    sequence with power feature's exit sequence.
 *
 *
 * Steps in PSI entry/exit Sequence:
 * 1) ACQUIRE_MUTEX:
 *    Waits until PMU acquires GPIO mutex. Each power feature have its own
 *    timeout value for this step.
 *    E.g GR-ELPG don't have any strict restriction on entry/exit latency thus
 *    it uses 200ms as timeout where as MSCG uses 20us as timeout.
 *    PSI will not get engaged if timeout happened in PSI entry sequences.
 *    Timeout during PSI exit sequence is fatal error.
 *
 * 2) WRITE_GPIO:
 *    Writes to LW_PMGR_GPIO_6_OUTPUT_CNTL to engage/dis-engage PSI.
 *
 * 3) WAIT_FOR_GPIO_FLUSH:
 *    As per HW folks, GPIO flush operation completes immediately (Bug #990198
 *    comment #8). But SW found that GPIO takes "Non Zero" time to complete the
 *    flush  operation. Thus, SW must wait until GPIO gets flush. Max timeout
 *    for this operation is 50us.
 *    This step is optional while engaging PSI. But it's good to ensure that
 *    GPIO completely got flushed before proceeding further.
 *
 * 4) RELEASE_MUTEX:
 *    Release GPIO mutex. This step is optional in case power feature wants to
 *    hold the mutex.
 *
 * 5) SETTLE_TIME:
 *    After "engaging" PSI, this step adds WAIT to achieve some PSI residency
 *    before exiting power feature.
 *    After "dis-engaging" PSI, this step adds WAIT so that voltage regulator
 *    will get some time to settle down.
 *    Regulator Settle time can be parallelized with Power Features exit
 *    sequence, provided EXIT SEQUENCE IS NOT RAISING TOTAL CURRENT on LWVDD
 *    rail.
 *
 * 6) RELEASE_OWNERSHIP:
 *    This is DONT CARE step while engaging PSI but MUST HAVE step while
 *    dis-engaging PSI.
 *    "On Exit", this step notifies that power feature has successfully
 *    dis-engaged PSI and thus other power features can take the ownership of
 *    PSI.
 *
 * RM Based Initialization:
 * - Regulator settlement time varies with GPU. RM sends this value to PMU on
 *   init.
 * - RM decides Mutex Acquire timeout for each power feature.
 *
 * Additionally RM/VBIOS allows/disallows PSI for each PState.
 *
 * PSI Flavors:
 * 1) Static PSI
 *    - In Static PSI VBIOS Flags decides with which feature PSI is coupled.
 *    - Power feature will engage PSI in each cycles.
 * 2) Current Aware PSI
 *    - In high performance state (like P5), static PSI doesn't ensures LWVDD
 *      current requirements all use-cases. Thus, we introduced another flavour
 *      of PSI named as Current Aware PSI.
 *    - In Current Aware PSI, PMU predict LWVDD current for different Low Power
 *      States (Power state in which some Power Saving Feature is engaged) of
 *      GPU. This predicted current is called as Isleep current.
 *    - PMU will engage the PSI only if Isleep is less than Ioptimal_1phase.
 * 3) Auto PSI
 *    - Voltage Regulator is capable callwlating LWVDD current and engaging PSI.
 *      SW dont have to do current predition. SW is just enabling/disabling PSI.
 *
 * Wiki: https://wiki.lwpu.com/engwiki/index.php/Power_Feature_Coupled_PSI
 */

/* ------------------------ System includes -------------------------------- */
/* ------------------------ Application includes --------------------------- */
#include "config/g_psi_hal.h"

#include "pmu_objpmgr.h"

/* ------------------------ Macros & Defines ------------------------------- */

/*!
 * @brief Sub-steps in PSI engage/dis-engage sequence
 */
enum
{
    PSI_ENTRY_EXIT_STEP_CHECK_LWRRENT = 0x0,
    PSI_ENTRY_EXIT_STEP_ACQUIRE_MUTEX      ,
    PSI_ENTRY_EXIT_STEP_WRITE_GPIO         ,
    PSI_ENTRY_EXIT_STEP_WAIT_FOR_GPIO_FLUSH,
    PSI_ENTRY_EXIT_STEP_RELEASE_MUTEX      ,
    PSI_ENTRY_EXIT_STEP_SETTLE_TIME        ,
    PSI_ENTRY_EXIT_STEP_RELEASE_OWNERSHIP  ,
};

/*!
 * @brief SCI update steps for PSI
 *
 * *ENABLE  - Enables PSI for SCI. PSI will be asserted in powerup/down SCI seq
 * *INSTR   - Swithces to default SCI seq i.e engage PSI in powerup, disengage in powerdown
 * *DISABLE - Disable PSI for SCI. PSI will be de asserted in powerup/down SCI seq
 */
enum
{
    PSI_SCI_STEP_ENABLE,
    PSI_SCI_STEP_INSTR,
    PSI_SCI_STEP_DISABLE,
};

/*!
 * @brief Checks whether sub-step in PSI engage/dis-engage sequence is enabled
 * or not.
 */
#define PSI_STEP_ENABLED(stepMask, step)                           \
                       (((stepMask) & BIT(PSI_ENTRY_EXIT_STEP_##step)) != 0)

#define PSI_SCI_STEP_ENABLED(stepMask, step)                       \
                       (((stepMask) & BIT(PSI_SCI_STEP_##step)) != 0)

/*!
 * @brief PSI engage/dis-engage step masks.
 */
#define PSI_ENTRY_EXIT_STEP_MASK_ALL                                         \
        (                                                                    \
            BIT(PSI_ENTRY_EXIT_STEP_CHECK_LWRRENT)                          |\
            BIT(PSI_ENTRY_EXIT_STEP_ACQUIRE_MUTEX)                          |\
            BIT(PSI_ENTRY_EXIT_STEP_WRITE_GPIO)                             |\
            BIT(PSI_ENTRY_EXIT_STEP_WAIT_FOR_GPIO_FLUSH)                    |\
            BIT(PSI_ENTRY_EXIT_STEP_RELEASE_MUTEX)                          |\
            BIT(PSI_ENTRY_EXIT_STEP_SETTLE_TIME)                            |\
            BIT(PSI_ENTRY_EXIT_STEP_RELEASE_OWNERSHIP)                       \
        )

/*!
 * PSI entry exit step mask for GR coupled PSI.
 */
#define GR_PSI_ENTRY_EXIT_STEPS_MASK            (PSI_ENTRY_EXIT_STEP_MASK_ALL)

/*!
 * @brief MSCG coupled PSI entry step mask
 *
 * If MSCG coupled PSI is engaged in MSCG entry sequence MSCG does not
 * release the mutex after completing PSI entry sequence
 */
#define MS_PSI_ENTRY_STEPS_MASK_CORE_SEQ                                     \
        (                                                                    \
            BIT(PSI_ENTRY_EXIT_STEP_CHECK_LWRRENT)                          |\
            BIT(PSI_ENTRY_EXIT_STEP_ACQUIRE_MUTEX)                          |\
            BIT(PSI_ENTRY_EXIT_STEP_WRITE_GPIO)                             |\
            BIT(PSI_ENTRY_EXIT_STEP_WAIT_FOR_GPIO_FLUSH)                    |\
            BIT(PSI_ENTRY_EXIT_STEP_SETTLE_TIME)                             \
        )

/*!
 * @brief MSCG Coupled PSI Exit Stage1
 *
 * Write GPIO register which will trigger PSI Exit. No need to acquire mutex
 * as PMU already owns GPIO mutex.
 */
#define MS_PSI_EXIT_STEPS_MASK_CORE_SEQ_STAGE_1                              \
        (                                                                    \
            BIT(PSI_ENTRY_EXIT_STEP_WRITE_GPIO)                              \
        )

/*!
 * @brief MSCG Coupled PSI Exit Stage2
 *
 * 2nd stage ensures that GPIO register has flushed successfully and releases
 * mutex. It does not wait for regulator settle time as MSCG has its own ucode
 * to ensure the settle time requirements.
 */
#define MS_PSI_EXIT_STEPS_MASK_CORE_SEQ_STAGE_2                              \
        (                                                                    \
            BIT(PSI_ENTRY_EXIT_STEP_WAIT_FOR_GPIO_FLUSH)                    |\
            BIT(PSI_ENTRY_EXIT_STEP_RELEASE_MUTEX)                          |\
            BIT(PSI_ENTRY_EXIT_STEP_RELEASE_OWNERSHIP)                       \
        )

/*!
 * @brief MSCG Coupled PSI Exit for Multirail
 *
 * For Multirail psi, we need to follow all the PSI exit steps.
 */
#define MS_PSI_MULTIRAIL_EXIT_STEPS                                          \
        (                                                                    \
            BIT(PSI_ENTRY_EXIT_STEP_WRITE_GPIO)                             |\
            BIT(PSI_ENTRY_EXIT_STEP_WAIT_FOR_GPIO_FLUSH)                    |\
            BIT(PSI_ENTRY_EXIT_STEP_SETTLE_TIME)                            |\
            BIT(PSI_ENTRY_EXIT_STEP_RELEASE_MUTEX)                          |\
            BIT(PSI_ENTRY_EXIT_STEP_RELEASE_OWNERSHIP)                       \
        )

/*!
 * @brief Mask to disengage and re-engage MSCG coupled PSI across DI cycles
 *
 * MSCG coupled PSI is disengaged in DI(GC5) entry sequence and re-engage in
 * DI exit. MSCG only needs to write GPIO for completing PSI exit sequence.
 */
#define MS_PSI_ENTRY_EXIT_STEPS_MASK_DI_SEQ                                  \
        (                                                                    \
            BIT(PSI_ENTRY_EXIT_STEP_WRITE_GPIO)                             |\
            BIT(PSI_ENTRY_EXIT_STEP_WAIT_FOR_GPIO_FLUSH)                    |\
            BIT(PSI_ENTRY_EXIT_STEP_SETTLE_TIME)                             \
        )

/*!
 * @brief Mask to engage and disengage DI coupled PSI with PMGR
 */
#define DI_PSI_ENTRY_EXIT_STEPS_MASK_WITH_PMGR  (PSI_ENTRY_EXIT_STEP_MASK_ALL)

/*!
 * @brief Mask to engage and disengage DI coupled PSI with SCI
 *
 * SCI will engage PSI. So PMU just need check the LWVD current and grab the
 * PSI ownership. Rest stuff will be done by SCI.
 */
#define DI_PSI_ENTRY_EXIT_STEPS_MASK_WITH_SCI                                \
        (                                                                    \
            BIT(PSI_ENTRY_EXIT_STEP_CHECK_LWRRENT)                          |\
            BIT(PSI_ENTRY_EXIT_STEP_RELEASE_OWNERSHIP)                       \
        )

/*!
* @brief PSI engage/dis-engage step masks for I2C Based PSI.
*/
#define PSI_I2C_ENTRY_EXIT_STEP_MASK_ALL                                    \
       (                                                                    \
           BIT(PSI_ENTRY_EXIT_STEP_CHECK_LWRRENT)                          |\
           BIT(PSI_ENTRY_EXIT_STEP_SETTLE_TIME)                            |\
           BIT(PSI_ENTRY_EXIT_STEP_RELEASE_OWNERSHIP)                       \
       )


// Invalid sleep current
#define PSI_SLEEP_LWRRENT_ILWALID_MA    (LW_U32_MAX)

/*!
 * @brief Macros to get pointer to PSI_CTRL.
 */
#define PSI_GET_CTRL(ctrlId)    (Psi.pPsiCtrl[ctrlId])

/*!
 * @brief Macros to get pointer to PSI_RAIL.
 */
#define PSI_GET_RAIL(railId)    (Psi.pPsiRail[railId])

/*!
 * @brief Macro to check the support for PSICTRL
 */
#define PSI_IS_CTRL_SUPPORTED(ctrlId)  ((Psi.ctrlSupportMask & BIT(ctrlId)) != 0)

/*!
 * @brief Macro to check the support for PSI_RAIL
 */
#define PSI_IS_RAIL_SUPPORTED(railId)  ((Psi.railSupportMask & BIT(railId)) != 0)


// Check for the PSI flavour the GPU supports
#define PSI_FLAVOUR_IS_ENABLED(flavour, railId)                              \
(PMUCFG_FEATURE_ENABLED(PMU_PSI_FLAVOUR_##flavour) &&                        \
 (PSI_GET_RAIL(railId)->flavor == RM_PMU_PSI_FLAVOR_##flavour))

/*!
 * @brief PSI engage/dis-engage APIs
 *
 * ELPG, MSCG and DI will use these APIs to engage/dis-engage PSI
 */
#define psiEngage(ctrlId, railMask, stepMask)                                \
           psiEngagePsi_HAL(&Psi, ctrlId, railMask, stepMask)

#define psiDisengage(ctrlId, railMask, stepMask)                             \
           psiDisengagePsi_HAL(&Psi, ctrlId, railMask, stepMask)

#define psiIsCtrlEngaged(ctrlId, railId)  (PSI_GET_RAIL(railId)->lwrrentOwnerId == ctrlId)

#define psiIsEngaged()   (PSI_GET_RAIL(RM_PMU_PSI_RAIL_ID_LWVDD)->lwrrentOwnerId != RM_PMU_PSI_CTRL_ID_ILWALID)

#define psiCtrlRailEnabledMaskGet(ctrlId)          \
            (PSI_GET_CTRL(ctrlId)->railEnabledMask)

// Get PSI GPIO pin number
#define psiGpioPinGet(railId)              (PSI_GET_RAIL(railId)->gpioPin)

// Get VR settle time
#define psiVRSettleTimeGet()               (PSI_GET_RAIL(RM_PMU_PSI_RAIL_ID_LWVDD)->settleTimeUs)

// Get Current Polling Period (Normal)
#define psiLwrrentPollingPeriodNormalGet() (Psi.lwrrPollPeriodNormalUs)

// Get Current Polling Period (Sleep)
#define psiLwrrentPollingPeriodSleepGet()  (Psi.lwrrPollPeriodSleepUs)

// Increase PSI engage count
#define psiCtrlEngageCountIncrease(ctrlId, railId)                           \
            (PSI_GET_CTRL(ctrlId)->pRailStat[railId]->engageCount++)

/*!
 * Mutex acquire timeout for power feature coupled PSI
 * Default value is taken from GPIO_PMU_MUTEX_ACQUIRE_TIMEOUT_DEFAULT_US
 */
#define PSI_MUTEX_ACQUIRE_TIMEOUT_DEFAULT_US                    (200000)

// Mutex acquire timeout for MSCG coupled PSI
#define PSI_MUTEX_ACQUIRE_TIMEOUT_MSCG_COUPLED_US               (20)

// Mutex acquire timeout for DI coupled PSI
#define PSI_MUTEX_ACQUIRE_TIMEOUT_DI_COUPLED_US                 (30)


// I2C Command Types used by I2C
#define PSI_I2C_CMD_RAIL_SELECT                                 0x0
#define PSI_I2C_CMD_RAIL_PHASE_UPDATE                           0x1
#define PSI_I2C_CMD__COUNT                                      0x2

/*!
 * MP2891 VR PSI Entry/Exit command values for I2C Transaction
 */

// Total number of command for PSI Entry or Exit
#define LPWR_PSI_I2C_DEVICE_MP2891_NUM_COMMANDS                 0x2

// Rail Select Control Register
#define LPWR_PSI_I2C_MP2891_RAIL_SELECT_CTRL_REG                0x00
// Rail Phase Select Control Register
#define LPWR_PSI_I2C_MP2891_RAIL_PHASE_SELECT_CTRL_REG          0x04

// Rail Select Values
#define LPWR_PSI_I2C_MP2891_RAIL_SELECT_LWVDD                   0x00
#define LPWR_PSI_I2C_MP2891_RAIL_SELECT_FBVDD                   0x01

//
// Command value to change LWVDD Phase
// 0x2309 - 6 Phase
// 0x0009 - Full Phase
//
#define LPWR_PSI_I2C_MP2891_LWVDD_PHASE_PSI                     0x2309
#define LPWR_PSI_I2C_MP2891_LWVDD_PHASE_FULL                    0x0009

//
// Command value to change LWVDD Phase
// 0x1103 - 1 Phase
// 0x0003 - Full Phase
//
#define LPWR_PSI_I2C_MP2891_FBVDD_PHASE_PSI                     0x1103
#define LPWR_PSI_I2C_MP2891_FBVDD_PHASE_FULL                    0x0003

/* ------------------------ Types definitions ------------------------------ */

/*!
 * @brief OBJPSICTRL
 *
 * This object stores PSI related parameters for each power feature.
 */
typedef struct
{
    //
    // Mutex Acquire Timeout
    //
    // Power features like MSCG have strict latency constraints. Default mutex
    // acquire timeout will not work for them as they can't spent much time in
    // just acquiring mutex.
    //
    LwU32                   mutexAcquireTimeoutUs;

    // Pointer to PSI Ctrl statistics
    RM_PMU_PSI_CTRL_STAT   *pRailStat[RM_PMU_PSI_RAIL_ID__COUNT];

    // PSI Ctrl rail enabledMask
    LwU32                   railEnabledMask;

    //
    // Minimum Pulse Width Low
    //
    // This is minimum PSI residency that we need to achieve for better power
    // numbers. "Zero" is valid value of minPulseWidthLowUs where SW is not
    // insisting any constraints on PSI residency.
    //
    // HW folks has given go ahead for "Zero" value. But its always good to
    // have non-zero values of minPulseWidthLowUs for better power saving and
    // reliable working of votage regulator.
    //
    // In design we found only two possible values for minPulseWidthLowUs.
    // - Zero: Can be achived by disabling PSI_ENTRY_EXIT_STEP_SETTLE_TIME step
    //         While engaging PSI.
    // - settleTimeUs : Otherwise
    //
    // Thus, we really dont need this variable.
    //
    // LwU8                 minPulseWidthLowUs;
    //
} PSI_CTRL;

/*!
 * @brief PSI RAIL I2C Command Structure
 *
 * Structure to hold the I2C Command
 */
typedef struct
{
    // Control Register
    LwU32 controlAddr;

    // Control Value
    LwU16 val;
} PSI_RAIL_I2C_COMMAND;

/*!
 * @brief PSI RAIL I2C Information
 *
 * Structure to maintain the I2C information for PSI rail
 */
typedef struct
{
    // I2C device Index
    LwU32                 i2cDevIdx;

    // Type of I2C Device
    LwU32                 i2cDevType;

    // PSI Engage Command
    PSI_RAIL_I2C_COMMAND  psiEngageCommand[PSI_I2C_CMD__COUNT];

    // PSI Disengage Command
    PSI_RAIL_I2C_COMMAND  psiDisengageCommand[PSI_I2C_CMD__COUNT];

} PSI_RAIL_I2C_INFO;

/*!
 * @brief OBJPSIRAIL
 *
 * OBJPSIRAIL helps us to keep the PSI rail specific parameters.
 */
typedef struct
{
    // Structure to manage the I2C related information for PSI Rail
    PSI_RAIL_I2C_INFO  *pI2cInfo;

    // Power feature ID that have lwrrently engaged PSI
    LwU8                lwrrentOwnerId;

    //
    // Regulator Settle Time
    //
    // The regulator needs settleTimeUs to complete the switch from single
    // phase mode to multi-phase mode before we can proceed further. This value
    // can be "Zero" in case power feature who is triggering PSI ensuring that
    // regulator will get sufficient time to complete the switch.
    //
    LwU8                settleTimeUs;

    // Gpio pin number for given PSI rail
    LwU8                gpioPin;

    //
    // Psi flavor
    // RM_PMU_PSI_FLAVOR_* defines PSI flavor
    //
    LwU8                flavor;

    //
    // voltRailIdx - This field is valid only for LWVDD_LOGIC|SRAM rails
    // which supports current aware PSI. For rest of the rails, this field will
    // be ilwlid.
    //
    LwU8                voltRailIdx;

    // Rail dependency mask
    LwU8                railDependencyMask;

    //
    // Optimal single phase current value for current aware PSI
    //
    // We will engage current aware PSI only if Isleep < iCrossmA
    // iSleep of power feature (eg: GCx,MSCG) will be callwlated before
    // engaging PSI
    //
    LwU16               iCrossmA;

    // Rail VR interface mode
    LwU8                vrInterfaceMode;
} PSI_RAIL;

/*!
 * @brief PSI Object Definition
 *
 * OBJPSI helps us to engage/disengage PSI with any power saving features.
 */
typedef struct
{
    // PSI Control that stores power feature specific information for PSI
    PSI_CTRL   *pPsiCtrl[RM_PMU_PSI_CTRL_ID__COUNT];

    // PSI Rail object
    PSI_RAIL   *pPsiRail[RM_PMU_PSI_RAIL_ID__COUNT];

    // Current Polling Period (Normal) for Current Aware PSI
    LwU32       lwrrPollPeriodNormalUs;

    // Current Polling Period (Sleep) for Current Aware PSI
    LwU32       lwrrPollPeriodSleepUs;

    // PSI Rail supportMask
    LwU32       railSupportMask;

    // PSI Ctrl Supported Mask
    LwU32       ctrlSupportMask;

    // Mutex Token for GPIO Mutex.
    LwU8        mutexTokenId;

    // Bool to suggest if callback for current callwlations has been scheduled.
    LwU8        bCallbackScheduled;

    // Variable to check if Pstate can engage the PSI or not
    LwBool      bPstateCoupledPsiDisallowed;
} OBJPSI;

/* ------------------------ External definitions --------------------------- */
extern OBJPSI Psi;

/* ------------------------ Function Prototypes ---------------------------- */
FLCN_STATUS psiPostInit(void)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "psiPostInit");

void        psiSleepLwrrentCalcMonoRail(PWR_EQUATION_LEAKAGE *pLeakage)
            GCC_ATTRIB_SECTION("imem_pmgr", "psiSleepLwrrentCalcMonoRail");

void        psiSleepLwrrentCalcMultiRail(void)
            GCC_ATTRIB_SECTION("imem_libPsiCallback", "psiSleepLwrrentCalcMultiRail");

// Callback related Routines in PSI.
void        psiCallbackTriggerMultiRail(LwBool bSchedule)
            GCC_ATTRIB_SECTION("imem_libPsiCallback", "psiCallbackTriggerMultiRail");

void        psiSchedulePmgrCallbackMultiRail(LwBool bSchedule)
                 GCC_ATTRIB_SECTION("imem_lpwrLoadin", "psiSchedulePmgrCallbackMultiRail");

LwU32       psiCtrlPstateRailMaskGet(LwU32, LwU8);

void        psiRailI2cCommandInit(LwU8 railId)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "psiRailI2cCommandInit");

FLCN_STATUS psiPstateDisallow(LwBool bDisallow)
            GCC_ATTRIB_SECTION("imem_lpwrLp", "psiPstateDisallow");
#endif // PMU_OBJPSI_H
