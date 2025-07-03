/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef _FUB_INLINE_STATIC_FUNCS_TU10X_H_
#define _FUB_INLINE_STATIC_FUNCS_TU10X_H_

/*!
 * @file: fub_inline_static_funcs_tu10x.h
 */

/* ------------------------- System Includes -------------------------------- */
#include <falcon-intrinsics.h>
#include "config/g_fub_private.h"
#include "fubutils.h"
#include "fubovl.h"
#include "lwuproc.h"
#include "lwRmReg.h"
#include "fubreg.h"
#include "fubScpDefines.h"
#include "config/fub-config.h"

#include "dev_master.h"
#include "dev_lwdec_csb.h"
#include "dev_sec_csb.h"
#include "dev_therm.h"
#include "dev_therm_vmacro.h"
#include "dev_fuse.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_fb.h"
#include "dev_se_seb.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
// DIO channel requests use bit 17 of address to indicate a read req
#define FALCON_DIO_D0_READ_BIT          (0x10000)

#if FUBCFG_FEATURE_ENABLED(FUBCORE_GA10X) && FUB_ON_LWDEC
#ifndef LW_CLWDEC_BAR0_TMOUT_CYCLES__PROD
#define LW_CLWDEC_BAR0_TMOUT_CYCLES__PROD    (0x01312d00)
#endif
#endif // FUBCFG_FEATURE_ENABLED(FUBCORE_GA10X) && FUB_ON_LWDEC

#if FUBCFG_FEATURE_ENABLED(FUBCORE_GA10X) && FUB_ON_SEC2
#ifndef LW_CSEC_BAR0_TMOUT_CYCLES_PROD
#define LW_CSEC_BAR0_TMOUT_CYCLES_PROD       (0x01312d00)
#endif
#endif // FUBCFG_FEATURE_ENABLED(FUBCORE_GA10X) && FUB_ON_SEC2

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */


/*!
 * Wrapper function for blocking falcon write instruction to return error in case of Priv Error.
 *
 * @param[in]  addr   The CSB address to read
 * @param[in]  Data   The 32-bit value to be written to address.
 *
 * @return The status of the write operation.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubWriteRegister_TU10X
(
    LwU32 addr,
    LwU32 data
)
{
    LwU32      csbErrStatVal = 0;
    FUB_STATUS fubStatus     = FUB_OK;

    falc_stxb_i((LwU32*)(addr), 0, data);
#ifdef FUB_ON_LWDEC
    csbErrStatVal = falc_ldxb_i((LwU32*)(LW_CLWDEC_FALCON_CSBERRSTAT), 0);
#elif FUB_ON_SEC2
    csbErrStatVal = falc_ldxb_i((LwU32*)(LW_CSEC_FALCON_CSBERRSTAT), 0);
#endif

    ct_assert(LW_CLWDEC_FALCON_CSBERRSTAT_VALID_TRUE == LW_CSEC_FALCON_CSBERRSTAT_VALID_TRUE);

    if (FLD_TEST_DRF(_CLWDEC, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatVal))
    {
        fubStatus = FUB_ERR_CSB_PRIV_WRITE_ERROR;
    }

    return fubStatus;
}

/*!
 * Wrapper function for blocking falcon read instruction to return error in case of Priv Error.
 *
 * @param[in]  addr   The CSB address to read
 * @param[out] *pData The 32-bit value of the requested address.
 *
 * @return The status of the read operation.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubReadRegister_TU10X
(
    LwU32 addr,
    LwU32 *pData
)
{
    LwU32         val           = 0;
    LwU32         csbErrStatVal = 0;
    FUB_STATUS    fubStatus     = FUB_ERR_CSB_PRIV_READ_ERROR;

    if (pData == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    val = falc_ldxb_i ((LwU32*)(addr), 0);
#ifdef FUB_ON_LWDEC
    csbErrStatVal = falc_ldxb_i ((LwU32*)(LW_CLWDEC_FALCON_CSBERRSTAT), 0);
#elif FUB_ON_SEC2
    csbErrStatVal = falc_ldxb_i ((LwU32*)(LW_CSEC_FALCON_CSBERRSTAT), 0);
#endif

    ct_assert(LW_CLWDEC_FALCON_CSBERRSTAT_VALID_TRUE == LW_CSEC_FALCON_CSBERRSTAT_VALID_TRUE);

    if (FLD_TEST_DRF(_CLWDEC, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatVal))
    {
        // If CSBERRSTAT is true, return no matter what register was read.
        CHECK_FUB_STATUS(FUB_ERR_CSB_PRIV_READ_ERROR);
    }
    else if ((val & CSB_INTERFACE_MASK_VALUE) == CSB_INTERFACE_BAD_VALUE)
    {
        //
        // For registers like TIMER and RNG, the caller will have to take care of this case where
        // BADFXXXX can be a valid value and retry reads
        //
        fubStatus = FUB_ERR_CSB_INTERFACE_BAD_VALUE;
    }
    else
    {
        fubStatus = FUB_OK;
    }

    //
    // Always update the output read data value, even in case of error.
    // The data is always invalid in case of error,  so it doesn't matter
    // what the value is.  The caller should detect the error case and
    // act appropriately.
    //
    *pData = val;
ErrorExit:
    return fubStatus;
}

/*!
 * @brief Write Status to mailbox.
 *
 * @param[in]  status The Error status to write to Mailbox
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
void
fubReportStatus_TU10X(LwU32 status)
{
    // can't report a failure in status reporting
#ifdef FUB_ON_LWDEC
    (void)FALC_REG_WR32(CSB, LW_CLWDEC_FALCON_MAILBOX0, status);
#elif FUB_ON_SEC2
    (void)FALC_REG_WR32(CSB, LW_CSEC_FALCON_MAILBOX0, status);
#endif
}

/*!
 * @brief Wait for BAR0 to become idle
 *
 * @return Return error if Register access failed.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubBar0WaitIdle_TU10X(void)
{
    LwBool     bDone      = LW_FALSE;
    LwU32      val        = 0;
    FUB_STATUS fubStatus  = FUB_OK;

    fubReportStatus_HAL(pFub, FUB_ERR_BIN_WAITING_FOR_BAR0_STATUS_TO_IDLE);
    do
    {
#ifdef FUB_ON_LWDEC
        CHECK_FUB_STATUS(FALC_REG_RD32_STALL(CSB, LW_CLWDEC_BAR0_CSR, &val));
#elif FUB_ON_SEC2
        CHECK_FUB_STATUS(FALC_REG_RD32_STALL(CSB, LW_CSEC_BAR0_CSR, &val));
#endif

        ct_assert(LW_CLWDEC_BAR0_CSR_STATUS_IDLE == LW_CSEC_BAR0_CSR_STATUS_IDLE);
        ct_assert(LW_CLWDEC_BAR0_CSR_STATUS_BUSY == LW_CSEC_BAR0_CSR_STATUS_BUSY);
        ct_assert(LW_CLWDEC_BAR0_CSR_STATUS_TMOUT == LW_CSEC_BAR0_CSR_STATUS_TMOUT);
        ct_assert(LW_CLWDEC_BAR0_CSR_STATUS_DIS == LW_CSEC_BAR0_CSR_STATUS_DIS);
        ct_assert(LW_CLWDEC_BAR0_CSR_STATUS_ERR == LW_CSEC_BAR0_CSR_STATUS_ERR);

#ifdef FUB_ON_LWDEC
        switch (DRF_VAL(_CLWDEC, _BAR0_CSR, _STATUS, val))
#elif FUB_ON_SEC2
        switch (DRF_VAL(_CSEC, _BAR0_CSR, _STATUS, val))
#endif
        {
            case LW_CLWDEC_BAR0_CSR_STATUS_IDLE:
                bDone = LW_TRUE;
                break;

            case LW_CLWDEC_BAR0_CSR_STATUS_BUSY:
                break;

            // unexpected error conditions
            case LW_CLWDEC_BAR0_CSR_STATUS_TMOUT:
            case LW_CLWDEC_BAR0_CSR_STATUS_DIS:
            case LW_CLWDEC_BAR0_CSR_STATUS_ERR:
            default:
                bDone      = LW_TRUE;
                fubStatus = FUB_ERR_WAIT_FOR_BAR0_IDLE_FAILED;
                break;
        }
    }
    while (!bDone);

    // FUB is out of loop, so clear error code
    fubReportStatus_HAL(pFub, FUB_OK);

ErrorExit:
    return fubStatus;
}

/*!
 * @brief Set timeout for BAR0 transactions
 *
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubSetBar0Timeout_TU10X(void)
{
    FUB_STATUS fubStatus = FUB_ERR_CSB_PRIV_READ_ERROR;

#ifdef FUB_ON_LWDEC
    CHECK_FUB_STATUS(FALC_REG_WR32_STALL(CSB, LW_CLWDEC_BAR0_TMOUT, DRF_DEF(_CLWDEC, _BAR0_TMOUT, _CYCLES, __PROD)));
#elif FUB_ON_SEC2
    CHECK_FUB_STATUS(FALC_REG_WR32_STALL(CSB, LW_CSEC_BAR0_TMOUT, DRF_DEF(_CSEC, _BAR0_TMOUT, _CYCLES, _PROD)));
#else 
    ct_assert(0);
#endif

ErrorExit:
    return fubStatus;
}


/*!
 * @brief Reads the given BAR0 address.
 *        WARNING: fubSetBar0Timeout must be called before calling this function.
 *
 * @param[in]   addr   The BAR0 address to read
 * @param[out]  *pData The 32-bit value of the requested BAR0 address.
 *
 * @return The status of the read operation.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubReadBar0_TU10X(LwU32 addr, LwU32 *pData)
{
    LwU32      val32     = 0;
    FUB_STATUS fubStatus = FUB_ERR_CSB_PRIV_READ_ERROR;

    if (pData == NULL)
    {
        return FUB_ERR_ILWALID_ARGUMENT;
    }

#ifdef FUB_ON_LWDEC
    CHECK_FUB_STATUS(fubBar0WaitIdle_HAL());
    CHECK_FUB_STATUS(FALC_REG_WR32_STALL(CSB, LW_CLWDEC_BAR0_ADDR, addr));

    CHECK_FUB_STATUS(FALC_REG_WR32_STALL(CSB, LW_CLWDEC_BAR0_CSR,
                        DRF_DEF(_CLWDEC, _BAR0_CSR, _CMD ,        _READ) |
                        DRF_NUM(_CLWDEC, _BAR0_CSR, _BYTE_ENABLE, 0xF  ) |
                        DRF_DEF(_CLWDEC, _BAR0_CSR, _TRIG,        _TRUE)));

    CHECK_FUB_STATUS(FALC_REG_RD32_STALL(CSB, LW_CLWDEC_BAR0_CSR, &val32));

    CHECK_FUB_STATUS(fubBar0WaitIdle_HAL());

    CHECK_FUB_STATUS(FALC_REG_RD32(CSB, LW_CLWDEC_BAR0_DATA, pData));
#elif FUB_ON_SEC2
    CHECK_FUB_STATUS(fubBar0WaitIdle_HAL());
    CHECK_FUB_STATUS(FALC_REG_WR32_STALL(CSB, LW_CSEC_BAR0_ADDR, addr));

    CHECK_FUB_STATUS(FALC_REG_WR32_STALL(CSB, LW_CSEC_BAR0_CSR,
                        DRF_DEF(_CSEC, _BAR0_CSR, _CMD ,        _READ) |
                        DRF_NUM(_CSEC, _BAR0_CSR, _BYTE_ENABLE, 0xF  ) |
                        DRF_DEF(_CSEC, _BAR0_CSR, _TRIG,        _TRUE)));

    CHECK_FUB_STATUS(FALC_REG_RD32_STALL(CSB, LW_CSEC_BAR0_CSR, &val32));

    CHECK_FUB_STATUS(fubBar0WaitIdle_HAL());

    CHECK_FUB_STATUS(FALC_REG_RD32(CSB, LW_CSEC_BAR0_DATA, pData));
#endif

ErrorExit:
    return fubStatus;
}

/*!
 * @brief Writes a 32-bit value to the given BAR0 address.
 *        WARNING: fubSetBar0Timeout must be called before calling this function.
 *
 * @param[in]  addr  The BAR0 address to write
 * @param[in]  data  The data to write to the address
 *
 * @return The status of the write operation.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubWriteBar0_TU10X(LwU32 addr, LwU32 data)
{
    LwU32      val32     = 0;
    FUB_STATUS fubStatus = FUB_ERR_CSB_PRIV_READ_ERROR;

#ifdef FUB_ON_LWDEC
    CHECK_FUB_STATUS(fubBar0WaitIdle_HAL());
    CHECK_FUB_STATUS(FALC_REG_WR32_STALL(CSB, LW_CLWDEC_BAR0_ADDR, addr));
    CHECK_FUB_STATUS(FALC_REG_WR32_STALL(CSB, LW_CLWDEC_BAR0_DATA, data));

    CHECK_FUB_STATUS(FALC_REG_WR32_STALL(CSB, LW_CLWDEC_BAR0_CSR,
                        DRF_DEF(_CLWDEC, _BAR0_CSR, _CMD ,        _WRITE) |
                        DRF_NUM(_CLWDEC, _BAR0_CSR, _BYTE_ENABLE, 0xF   ) |
                        DRF_DEF(_CLWDEC, _BAR0_CSR, _TRIG,        _TRUE )));

    CHECK_FUB_STATUS(FALC_REG_RD32_STALL(CSB, LW_CLWDEC_BAR0_CSR, &val32));
#elif FUB_ON_SEC2
    CHECK_FUB_STATUS(fubBar0WaitIdle_HAL());
    CHECK_FUB_STATUS(FALC_REG_WR32_STALL(CSB, LW_CSEC_BAR0_ADDR, addr));
    CHECK_FUB_STATUS(FALC_REG_WR32_STALL(CSB, LW_CSEC_BAR0_DATA, data));

    CHECK_FUB_STATUS(FALC_REG_WR32_STALL(CSB, LW_CSEC_BAR0_CSR,
                        DRF_DEF(_CSEC, _BAR0_CSR, _CMD ,        _WRITE) |
                        DRF_NUM(_CSEC, _BAR0_CSR, _BYTE_ENABLE, 0xF   ) |
                        DRF_DEF(_CSEC, _BAR0_CSR, _TRIG,        _TRUE )));

    CHECK_FUB_STATUS(FALC_REG_RD32_STALL(CSB, LW_CSEC_BAR0_CSR, &val32));
#endif

    CHECK_FUB_STATUS(fubBar0WaitIdle_HAL());

ErrorExit:
    return fubStatus;
}


/*
 * @brief: Check if UCODE is running on valid chip
 *
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubIsChipSupported_TU10X(void)
{
    FUB_STATUS  fubStatus = FUB_ERR_UNSUPPORTED_CHIP;
    LwU32       chipId    = 0;

    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_PMC_BOOT_42, &chipId));
    chipId = DRF_VAL( _PMC, _BOOT_42, _CHIP_ID, chipId);

    switch (chipId)
    {
        case LW_PMC_BOOT_42_CHIP_ID_TU102:
        case LW_PMC_BOOT_42_CHIP_ID_TU104:
        case LW_PMC_BOOT_42_CHIP_ID_TU106:
            fubStatus = FUB_OK;
        break;
        default:
            fubStatus = FUB_ERR_UNSUPPORTED_CHIP;
    }

ErrorExit:
    return fubStatus;
}


/*
 * @brief: Check if FUB is exelwting on the LWDEC falcon
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubIsValidFalconEngine_TU10X(void)
{
    FUB_STATUS fubStatus = FUB_ERR_UNEXPECTED_FALCON_ENGID;
    LwU32      engineId  = 0;

#ifdef FUB_ON_LWDEC
    CHECK_FUB_STATUS(FALC_REG_RD32(CSB, LW_CLWDEC_FALCON_ENGID, &engineId));
    if (FLD_TEST_DRF(_CLWDEC, _FALCON_ENGID, _FAMILYID, _LWDEC, engineId))
#elif FUB_ON_SEC2
    CHECK_FUB_STATUS(FALC_REG_RD32(CSB, LW_CSEC_FALCON_ENGID, &engineId));
    if (FLD_TEST_DRF(_CSEC, _FALCON_ENGID, _FAMILYID, _SEC, engineId))
#endif
    {
        fubStatus = FUB_OK;
    }
    else
    {
        CHECK_FUB_STATUS(FUB_ERR_UNEXPECTED_FALCON_ENGID);
    }
ErrorExit:
    return fubStatus;
}

/*!
 * @brief: To mitigate the risk of a  NS restart from HS mode we follow #19 from
 * https://wiki.lwpu.com/gpuhwmaxwell/index.php/MaxwellSW/Resman/Security#Guidelines_for_HS_binary.
 * Please refer bug 2534981 for detail info.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
void
fubMitigateNSRestartFromHS_TU10X(void)
{
    LwU32         cpuctlAliasEn = 0;
    FUB_STATUS    fubStatus     = FUB_OK;

#ifdef FUB_ON_LWDEC
    //
    // Return status is being ignored intentionally here, as we do not want to 
    // check status and return error and halt in caller before CPUCTL_ALIAS is set to FALSE
    // to avoid security threat
    //
    (void)FALC_REG_RD32(CSB, LW_CLWDEC_FALCON_CPUCTL, &cpuctlAliasEn);
    cpuctlAliasEn = FLD_SET_DRF(_CLWDEC, _FALCON_CPUCTL, _ALIAS_EN, _FALSE, cpuctlAliasEn);

    //
    // Return status is being ignored intentionally here, as we do not want to 
    // check status and return error and halt in caller before CPUCTL_ALIAS is set to FALSE
    // to avoid security threat
    //
    (void)FALC_REG_WR32(CSB, LW_CLWDEC_FALCON_CPUCTL, cpuctlAliasEn);

    // We read-back the value of CPUCTL until ALIAS_EN  is set to FALSE
    do
    {
        fubStatus = FALC_REG_RD32(CSB, LW_CLWDEC_FALCON_CPUCTL, &cpuctlAliasEn);
    } while((fubStatus != FUB_OK) || (!FLD_TEST_DRF(_CLWDEC, _FALCON_CPUCTL, _ALIAS_EN, _FALSE, cpuctlAliasEn)));
#elif FUB_ON_SEC2
    //
    // Return status is being ignored intentionally here, as we do not want to 
    // check status and return error and halt in caller before CPUCTL_ALIAS is set to FALSE
    // to avoid security threat
    //
    (void)FALC_REG_RD32(CSB, LW_CSEC_FALCON_CPUCTL, &cpuctlAliasEn);
    cpuctlAliasEn = FLD_SET_DRF(_CSEC, _FALCON_CPUCTL, _ALIAS_EN, _FALSE, cpuctlAliasEn);

    //
    // Return status is being ignored intentionally here, as we do not want to 
    // check status and return error and halt in caller before CPUCTL_ALIAS is set to FALSE
    // to avoid security threat
    //
    (void)FALC_REG_WR32(CSB, LW_CSEC_FALCON_CPUCTL, cpuctlAliasEn);

    // We read-back the value of CPUCTL until ALIAS_EN  is set to FALSE
    do
    {
        fubStatus = FALC_REG_RD32(CSB, LW_CSEC_FALCON_CPUCTL, &cpuctlAliasEn);
    } while((fubStatus != FUB_OK) || (!FLD_TEST_DRF(_CSEC, _FALCON_CPUCTL, _ALIAS_EN, _FALSE, cpuctlAliasEn)));
#endif
}

/*!
 * @brief  Check is LWVDD voltage level is acceptable for fuseblow
 *         This voltage is set externally by MODS to 840mV
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubCheckLwvdd_TU10X(void)
{
    FUB_STATUS  fubStatus  = FUB_OK;
    LwU32       period     = 0;
    LwU32       dutyCycle  = 0;
    // Voltage Regulator Config is different only on TURING AUTO SKU
#if defined(AUTO_FUB) && FUBCFG_FEATURE_ENABLED(FUBCORE_TU10X) 
    // 
    // These constants correspond to 800mV-880mV according to the board design
    // On Auto SKU 510,610 voltage regulator configuration is changed recently (Bug 200532759)
    // Callwlations are explained below :-
    // Formula Used - (refer  https://confluence.lwpu.com/display/BS/Voltage+Device+Table)
    //  TargetVoltage = BaseVoltage + DutyCycle * OffsetScale / RawPeriod ()
    // VBIOS used   - "core90->tu104->gddr6(auto_gddr6_pkc)->pg189-pkc->g189_0610_510" on VBOD
    // Variable Values –
    // 1) BaseVoltage = 300000 [Take from Voltage Device Table -> Voltage Base (uV)]
    // 2) OffsetScale = 700000 [Take from Voltage Device Table -> Voltage Scale (uV)]
    // 3) DutyCycle   = 84     [Take from Devinit Boot Scripts -> General Initialization -> LW_THERM_VID1_PWM(0) (MAIN) OR Mask]
    // 4) RawPeriod   = 112    [Take from Devinit Boot Scripts -> General Initialization -> LW_THERM_VID0_PWM(0) (MAIN) OR Mask]
    //
    const LwU32 REQ_PERIOD = 0x70U;
    const LwU32 REQ_DCMIN  = 0x50U;
    const LwU32 REQ_DCMAX  = 0x5LW;
#else //defined(AUTO_FUB) && FUBCFG_FEATURE_ENABLED(FUBCORE_TU10X)
    //
    // These constants correspond to 800mV-880mV according to the board design
    // for rest Turing boards. This is the required range for FPF.
    // Callwlation of duty cycle ported from _voltageCalcVid()
    // https://p4sw-swarm.lwpu.com/files/sw/main/bios/core90/core/pmu/voltage.c#227
    //
    const LwU32 REQ_PERIOD = 0xA0U;
    const LwU32 REQ_DCMIN  = 0x50U;
    const LwU32 REQ_DCMAX  = 0x5DU;
#endif // defined(AUTO_FUB) && FUBCFG_FEATURE_ENABLED(FUBCORE_TU10X)
    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_THERM_VID0_PWM(0), &period));
    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_THERM_VID1_PWM(0), &dutyCycle));

    period      = DRF_VAL(_THERM, _VID0_PWM, _PERIOD, period);
    dutyCycle   = DRF_VAL(_THERM, _VID1_PWM, _HI,     dutyCycle);

    if (period != REQ_PERIOD)
    {
        CHECK_FUB_STATUS(FUB_ERR_BAD_VOLTAGE);
    }
    if ((dutyCycle < REQ_DCMIN) || (dutyCycle > REQ_DCMAX))
    {
        CHECK_FUB_STATUS(FUB_ERR_BAD_VOLTAGE);
    }

ErrorExit:
    return fubStatus;
}

/* @brief Reads SCI SEC TIMER lower 32 bits (TIME_0). Upper 32 bits (TIME_1) are not required since
 *        in current use case wait time is less than 4.3 seconds which easily fits in TIME_0
 */ 
//
// NOTE : Do not call this function before fubProtectRegisters_HAL since the PLM's for the timer
//        might not be protected.
//
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubReadSciSecTimer_TU10X(LwU32 *pTime)
{
    FUB_STATUS  fubStatus          = FUB_ERR_GENERIC;
    LwU32       time0              = 0;
    LwU32       oldValue           = 0;
    LwU32       lwrrValue          = 0;
    LwU32       count              = 0;

    // Read Value of TIME_0
    fubStatus = FALC_REG_RD32(BAR0, LW_PGC6_SCI_SEC_TIMER_TIME_0, &time0);
    // NSEC field stores the actual value of the timer 0
    oldValue = DRF_VAL(_PGC6, _SCI_SEC_TIMER_TIME_0, _NSEC, time0);

    //
    // Since 0xBADFXXXX can be a valid time value for TIME_0 for upto 2^16ns, but it keeps
    // changing its value thus we read for few times to detect this.
    //
    if (fubStatus == FUB_ERR_CSB_INTERFACE_BAD_VALUE)
    {         
        for(count =0; count<3; count++)
        {
            // Read Value of TIME_0 again
            fubStatus = FALC_REG_RD32(BAR0, LW_PGC6_SCI_SEC_TIMER_TIME_0, &time0);
            // NSEC field stores the actual value of the timer 0
            lwrrValue = DRF_VAL(_PGC6, _SCI_SEC_TIMER_TIME_0, _NSEC, time0);

            if (oldValue != lwrrValue)
            {
                oldValue = lwrrValue;
                fubStatus = FUB_OK;
                break; 
            }
            else
            {
               fubStatus = FUB_ERR_CSB_INTERFACE_BAD_VALUE;
            }
        }
    }

    (*pTime) = oldValue;
    CHECK_FUB_STATUS(fubStatus);

ErrorExit:
    return fubStatus;
}

/*!
 * Read Falcon PTimer. There is no assocaited PLM with this hence this is inselwred.
 * Use of this function is restricted till the point we can not access BAR0 in 
 * HS entry/init sequence.
 * Other places should use secure timer through fub (fubReadSciSecTimer).
 * @param[out] *pData The 32-bit value of the requested address.
 * @return The status of the read operation.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubReadFalconPTimer0_TU10X
(
    LwU32 *pData
)
{
#if (AUTO_FUB && FUB_ON_SEC2)
    LwU32         val           = 0;
    FUB_STATUS    fubStatus     = FUB_ERR_CSB_PRIV_READ_ERROR;

    if (pData == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }
    
    fubStatus = FALC_REG_RD32(CSB, LW_CSEC_FALCON_PTIMER0, &val);
    if (fubStatus == FUB_ERR_CSB_INTERFACE_BAD_VALUE)
    {
        //
        // If we get here, we thought we read a bad value. However, there are
        // certain cases where the values may lead us to think we read something
        // bad, but in fact, they were just valid values that the register could
        // report. For example, the host ptimer PTIMER1 and PTIMER0 registers
        // can return 0xbadfxxxx value that we interpret to be "bad".
        //
        LwU32 newVal      = 0;
        LwU8 i            = 0;
        for (i=0; i<3; i++)
        {
             fubStatus = FALC_REG_RD32(CSB, LW_CSEC_FALCON_PTIMER0, &newVal);
             if ((fubStatus == FUB_OK) || ((fubStatus == FUB_ERR_CSB_INTERFACE_BAD_VALUE) && (newVal != val)))
             {
                 val = newVal;
                 fubStatus = FUB_OK;
                 break;
             }
             else
             {
                 fubStatus = FUB_ERR_CSB_INTERFACE_BAD_VALUE;
             }
        }
    }
    //
    // Always update the output read data value, even in case of error.
    // The data is always invalid in case of error,  so it doesn't matter
    // what the value is.  The caller should detect the error case and
    // act appropriately.
    //
    *pData = val;
ErrorExit:
    return fubStatus;
#else
    return FUB_ERR_UNSUPPORTED_CHIP;
#endif // #if AUTO_FUB && FUB_ON_SEC2
}

/* !
 * @brief: Callwlate the elapsed time from the provided time.
 *         Uses Falcon PTIMER0 which is not protected by PLM
 *         Hence should not be called apart from HS init sequence.
 * @param[in]  pTime        : Base time
 * @param[out] pElapsedTime : elapsed time from base time.
 * @return     FUB_STATUS   : FUB_OK for success else error.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubFalconPTimer0GetElapsedTime_TU10X
(
    const LwU32 pTime, 
    LwU32 *pElapsedTime
)
{
#if (AUTO_FUB && FUB_ON_SEC2)
    FUB_STATUS fubStatus   = FUB_OK;
    LwU32 lwrrentTime      = 0;
    if (pElapsedTime == NULL)
    {
        return FUB_ERR_ILWALID_ARGUMENT;
    }
    
    CHECK_FUB_STATUS(fubReadFalconPTimer0_HAL(pFub, &lwrrentTime));
    *pElapsedTime = lwrrentTime - pTime;
    fubStatus = FUB_OK;
ErrorExit:
    return fubStatus;
#else
    return FUB_ERR_UNSUPPORTED_CHIP;
#endif // #ifdef AUTO_FUB && FUB_ON_SEC2
}

/* !
 * @brief: Check whether timeout has oclwred.
 *         Uses Falcon PTIMER0 which is not protected by PLM
 *         Hence should not be called apart from HS init sequence.
 * @param[in]  timeOut     : Time out value.
 * @param[in]  startTime   : Start of the timer.
 * @param[out] pTimeOutLeft: Time left out of the specified timeOut.
 * @return     FUB_STATUS   : FUB_OK for success else error.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubFalconPTimer0CheckTimeout_TU10X
(
    const LwU32 timeOut, 
    LwU32 startTime, 
    LwS32 *pTimeOutLeft
)
{
#if (AUTO_FUB && FUB_ON_SEC2)
    LwU32 elapsedTime    = 0;
    FUB_STATUS fubStatus = FUB_OK;

    fubStatus = fubFalconPTimer0GetElapsedTime_HAL(pFub, startTime, &elapsedTime);
    if (fubStatus != FUB_OK)
    {
        return fubStatus;
    }
    
    *pTimeOutLeft = timeOut - elapsedTime;
    return (elapsedTime >= timeOut) ? FUB_ERR_TIMEOUT : FUB_OK;
#else
    return FUB_ERR_UNSUPPORTED_CHIP;
#endif // #if AUTO_FUB && FUB_ON_SEC2
}

/*!
 * @brief  For voltage at VQPS pin to shift from 0 to 1.8V or vice versa does not
 *         happen immediately after setting control bit. VQPS pin does not have voltage
 *         monitor attached to check if voltage has reached intended level.
 *         So using PTIMER to wait for Voltage to shift levels.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubWaitForVoltageShift_TU10X(void)
{
    FUB_STATUS      fubStatus  = FUB_OK;
    LwU32           delayNs    = WAIT_FOR_LWVDD_RAIL_VOLTAGE_TO_RAMPUP;
    LwU32           lwrrTime   = 0;
    LwU32           startTime  = 0;

    // Read start time
    CHECK_FUB_STATUS(fubReadSciSecTimer_HAL(pFub, &startTime));

    // Write Status to indicate FUB is waiting for voltage to shift levels
    fubReportStatus_HAL(pFub, FUB_ERR_BIN_WAITING_FOR_VOLTAGE_TO_SHIFT_LEVELS);
    do
    {
        // Read current time until while condition is met
        CHECK_FUB_STATUS(fubReadSciSecTimer_HAL(pFub, &lwrrTime));
    }
    //
    // adding useless checks to LHS of && for Cert-C compliance
    // integer wraparound naturally works for this logic
    //
    while(((lwrrTime > startTime) || (lwrrTime <= startTime)) && ((lwrrTime - startTime) < delayNs));

    // FUB is out of loop, so clear error code
    fubReportStatus_HAL(pFub, FUB_OK);

ErrorExit:
    return fubStatus;
}

/*!
 * @brief Write applicability failure to mailbox1.
 *        Bit set for usecase found to not be applicable.
 *
 * @param[in]  usecase  the usecase to report
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
void
fubReportApplicabilityFailure_TU10X(LwU32 usecase)
{
    LwU32       mailbox1          = 0;
    // can't report a failure in status reporting
#ifdef FUB_ON_LWDEC
    (void)FALC_REG_RD32(CSB, LW_CLWDEC_FALCON_MAILBOX1, &mailbox1);
    mailbox1 |= usecase;
    (void)FALC_REG_WR32(CSB, LW_CLWDEC_FALCON_MAILBOX1, mailbox1);
#elif FUB_ON_SEC2
    (void)FALC_REG_RD32(CSB, LW_CSEC_FALCON_MAILBOX1, &mailbox1);
    mailbox1 |= usecase;
    (void)FALC_REG_WR32(CSB, LW_CSEC_FALCON_MAILBOX1, mailbox1);
#endif
}

/*
 * @brief: Check if FUB is exelwting when in GC6 exit path
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubInGc6Exit_TU10X(void)
{
    FUB_STATUS fubStatus          = FUB_ERR_IN_GC6_EXIT_PATH;
    LwU32      gc6InterruptStatus = 0;

    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_PGC6_SCI_SW_SEC_INTR_STATUS, &gc6InterruptStatus));
    if (FLD_TEST_DRF(_PGC6, _SCI_SW_SEC_INTR_STATUS, _GC6_EXIT, _PENDING, gc6InterruptStatus))
    {
        CHECK_FUB_STATUS(FUB_ERR_IN_GC6_EXIT_PATH);
    }
    else
    {
        fubStatus = FUB_OK;
    }
ErrorExit:
    return fubStatus;
}


#ifdef IS_SSP_ENABLED

static LwU32 _randNum16byte[4] GCC_ATTRIB_ALIGN(16) = { 0 };

//
// Below functions to generate random number from SCP engine are copied from
// "//sw/main/bios/core90/core/pmu/scp_rand.c".
//

/*!
 * @brief   Sets up SCP TRNG and starts the random number generator.
 */
GCC_ATTRIB_SECTION("imem_fub", "s_scpStartTrng")
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS s_scpStartTrng(void)
{
    LwU32 val;
    FUB_STATUS fubStatus = FUB_ERR_GENERIC;

    //
    // Set LW_CLWDEC_SCP_RNDCTL0_HOLDOFF_INIT_LOWER.
    // This value determines the delay (in number of lwclk cycles) of the production
    // of the first random number. that positive transitions of CTL1.RNG_EN or
    // RNDCTL3.TRIG_LFSR_RELOAD_* cause this delay to be enforced.)
    // LW_CLWDEC_SCP_RNDCTL0 has only 1 field(LW_CLWDEC_SCP_RNDCTL0_HOLDOFF_INIT_LOWER) which is
    // 32 bits long.
    //
#ifdef FUB_ON_LWDEC
    CHECK_FUB_STATUS(FALC_REG_RD32( CSB, LW_CLWDEC_SCP_RNDCTL0, &val ));
    val = FLD_SET_DRF_NUM( _CLWDEC, _SCP_RNDCTL0, _HOLDOFF_INIT_LOWER, SCP_HOLDOFF_INIT_LOWER_VAL, val );
    CHECK_FUB_STATUS(FALC_REG_WR32( CSB, LW_CLWDEC_SCP_RNDCTL0, val));

    CHECK_FUB_STATUS(FALC_REG_RD32( CSB, LW_CLWDEC_SCP_RNDCTL1, &val ));
    val = FLD_SET_DRF_NUM( _CLWDEC, _SCP_RNDCTL1, _HOLDOFF_INTRA_MASK, SCP_HOLDOFF_INTRA_MASK, val );
    CHECK_FUB_STATUS(FALC_REG_WR32( CSB, LW_CLWDEC_SCP_RNDCTL1, val ));

    // Set the ring length of ring A/B
    CHECK_FUB_STATUS(FALC_REG_RD32( CSB, LW_CLWDEC_SCP_RNDCTL11, &val ));
    val = FLD_SET_DRF_NUM( _CLWDEC, _SCP_RNDCTL11, _AUTOCAL_STATIC_TAP_A, SCP_AUTOCAL_STATIC_TAPA, val );
    val = FLD_SET_DRF_NUM( _CLWDEC, _SCP_RNDCTL11, _AUTOCAL_STATIC_TAP_B, SCP_AUTOCAL_STATIC_TAPB, val );
    CHECK_FUB_STATUS(FALC_REG_WR32( CSB, LW_CLWDEC_SCP_RNDCTL11, val ));

    // Enable SCP TRNG
    CHECK_FUB_STATUS(FALC_REG_RD32( CSB, LW_CLWDEC_SCP_CTL1, &val ));
    val = FLD_SET_DRF( _CLWDEC, _SCP_CTL1_RNG, _EN, _ENABLED, val );
    CHECK_FUB_STATUS(FALC_REG_WR32( CSB, LW_CLWDEC_SCP_CTL1, val ));
#elif FUB_ON_SEC2
    CHECK_FUB_STATUS(FALC_REG_RD32( CSB, LW_CSEC_SCP_RNDCTL0, &val ));
    val = FLD_SET_DRF_NUM( _CSEC, _SCP_RNDCTL0, _HOLDOFF_INIT_LOWER, SCP_HOLDOFF_INIT_LOWER_VAL, val );
    CHECK_FUB_STATUS(FALC_REG_WR32( CSB, LW_CSEC_SCP_RNDCTL0, val));

    CHECK_FUB_STATUS(FALC_REG_RD32( CSB, LW_CSEC_SCP_RNDCTL1, &val ));
    val = FLD_SET_DRF_NUM( _CSEC, _SCP_RNDCTL1, _HOLDOFF_INTRA_MASK, SCP_HOLDOFF_INTRA_MASK, val );
    CHECK_FUB_STATUS(FALC_REG_WR32( CSB, LW_CSEC_SCP_RNDCTL1, val ));

    // Set the ring length of ring A/B
    CHECK_FUB_STATUS(FALC_REG_RD32( CSB, LW_CSEC_SCP_RNDCTL11, &val ));
    val = FLD_SET_DRF_NUM( _CSEC, _SCP_RNDCTL11, _AUTOCAL_STATIC_TAP_A, SCP_AUTOCAL_STATIC_TAPA, val );
    val = FLD_SET_DRF_NUM( _CSEC, _SCP_RNDCTL11, _AUTOCAL_STATIC_TAP_B, SCP_AUTOCAL_STATIC_TAPB, val );
    CHECK_FUB_STATUS(FALC_REG_WR32( CSB, LW_CSEC_SCP_RNDCTL11, val ));

    // Enable SCP TRNG
    CHECK_FUB_STATUS(FALC_REG_RD32( CSB, LW_CSEC_SCP_CTL1, &val ));
    val = FLD_SET_DRF( _CSEC, _SCP_CTL1_RNG, _EN, _ENABLED, val );
    CHECK_FUB_STATUS(FALC_REG_WR32( CSB, LW_CSEC_SCP_CTL1, val ));
#endif

ErrorExit:
    return fubStatus;
}

/*!
 * @brief   Stops SCP TRNG
 */
GCC_ATTRIB_SECTION("imem_fub", "s_scpStopTrng")
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS s_scpStopTrng(void)
{
    LwU32      val       = 0;
    FUB_STATUS fubStatus = FUB_ERR_GENERIC;

#ifdef FUB_ON_LWDEC
    CHECK_FUB_STATUS(FALC_REG_RD32( CSB, LW_CLWDEC_SCP_CTL1, &val ));
    val = FLD_SET_DRF( _CLWDEC, _SCP_CTL1_RNG, _EN, _DISABLED, val );
    CHECK_FUB_STATUS(FALC_REG_WR32( CSB, LW_CLWDEC_SCP_CTL1, val ));
#elif FUB_ON_SEC2
    CHECK_FUB_STATUS(FALC_REG_RD32( CSB, LW_CSEC_SCP_CTL1, &val ));
    val = FLD_SET_DRF( _CSEC, _SCP_CTL1_RNG, _EN, _DISABLED, val );
    CHECK_FUB_STATUS(FALC_REG_WR32( CSB, LW_CSEC_SCP_CTL1, val ));
#endif

ErrorExit:
    return fubStatus;
}

/*!
 * @brief   Gets 128 bit random number from SCP.
 *
 * @param[in,out] pRandomNum  Pointer to random number.
 * @return        FUB_STATUS
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS fubScpGetRandomNumber_TU10X(LwU32 *pRand32)
{
    FUB_STATUS fubStatus = FUB_ERR_GENERIC;
    LwU32      i         = 0;

    if (pRand32 == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    CHECK_FUB_STATUS(s_scpStartTrng());

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

    CHECK_FUB_STATUS(s_scpStopTrng());

ErrorExit:
    return fubStatus;
}
#endif    // IS_SSP_ENABLED


/*
 * @brief: Isolate whole GPU to SEC2 using Decode Traps. 
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubIsolateGpuToSec2UsingDecodeTraps_TU10X(FUB_DECODE_TRAP_OPERATION_TO_PERFORM operationToPerform)
{
#if FUB_ON_SEC2
    // 
    // For allocation refer
    // https://confluence.lwpu.com/display/TURCTXSWPRIV/Turing+DECODE_TRAP+usage+by+chip
    //
    const LwU32 trapId    = 18;
    FUB_STATUS  fubStatus = FUB_ERR_GENERIC;

    // 
    // Clear all Decode Trap configuration registers. 
    // Even in case of setting up trap, they shpuld be cleared first
    //
    CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP_ACTION(trapId), 0));
    CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP_MASK(trapId), 0));
    CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP_MATCH(trapId), 0));
    CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP_MATCH_CFG(trapId), 0));
    CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP_DATA1(trapId), 0));
    CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP_DATA2(trapId), 0));

#if (FUBCFG_FEATURE_ENABLED(FUBCORE_TU10X) || FUBCFG_FEATURE_ENABLED(FUBCORE_TU116))
    if (operationToPerform == ENABLE_DECODE_TRAP)
    {
        //
        // Ignore trap on read.
        // Trap all 3 priv levels
        // SUBID_CTL is COMPARED. This means that the trap will match
        // all subids where MATCH_SUBID <= subid <= MASK_SUBID
        // ILWERT_SUBID_MATCH is 1 (ilwerted) since we want to match
        // everyone other than SEC2's range
        // COMPARE CTL is MASKED
        //
        const LwU32 trapMatchCfg = DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP_MATCH_CFG, _IGNORE_READ,        _IGNORED)   |
                                   DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP_MATCH_CFG, _IGNORE_WRITE,       _MATCHED)   |
                                   DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP_MATCH_CFG, _IGNORE_WRITE_ACK,   _MATCHED)   |
                                   DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP_MATCH_CFG, _IGNORE_WORD_ACCESS, _MATCHED)   |
                                   DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP_MATCH_CFG, _IGNORE_BYTE_ACCESS, _MATCHED)   |
                                   DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP_MATCH_CFG, _ILWERT_ADDR_MATCH,  _NORMAL)    |
                                   DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP_MATCH_CFG, _ILWERT_SUBID_MATCH, _ILWERTED)  |
                                   DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP_MATCH_CFG, _TRAP_APPLICATION,   _ALL_LEVELS_ENABLED)   |
                                   DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP_MATCH_CFG, _SUBID_CTL,          _COMPARED)    |
                                   DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP_MATCH_CFG, _COMPARE_CTL,        _MASKED);
        CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP_MATCH_CFG(trapId), trapMatchCfg));

        // 
        // Match ADDR is 0. This is base of bar0 offset to be matched
        // Match SUBID is 0x14 (start of SEC2's SUBID). This is start
        // of SUBID range to be matched
        //
        const LwU32 trapMatch = DRF_NUM(_PPRIV, _SYS_PRI_DECODE_TRAP_MATCH, _ADDR,  0x0) |
                                DRF_NUM(_PPRIV, _SYS_PRI_DECODE_TRAP_MATCH, _SUBID, 0x14);
        CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP_MATCH(trapId), trapMatch));

        //
        // Match all registers (Mask ADDR = 0x3FF_FFFF).
        // SUBID mask is 0x14. This is end of SUBID range to be matched.
        // Since SEC2 has only on SUBID (0x14) this is same as MATCH_SUBID
        // So, MATCH_SUBID (start) is same as MASK_SUBID (end)
        //
        const LwU32 trapMask = DRF_NUM(_PPRIV, _SYS_PRI_DECODE_TRAP_MASK, _ADDR, 0x3FFFFFF) |
                               DRF_NUM(_PPRIV, _SYS_PRI_DECODE_TRAP_MASK, _SUBID, 0x14);
        CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP_MASK(trapId), trapMask));

        // TRAP_ACTION_FORCE_ERROR_RETURN = _ENABLE
        const LwU32 trapAction = DRF_DEF(_PPRIV, _SYS_PRI_DECODE_TRAP_ACTION, _FORCE_ERROR_RETURN, _ENABLE);
        CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP_ACTION(trapId), trapAction));
    }
#else
    return FUB_ERR_NOT_SUPPORTED;
#endif
ErrorExit:
    return fubStatus;
#elif defined(FUB_ON_LWDEC)
    return FUB_ERR_NOT_SUPPORTED;
#else
    // Control is not expected to come here. If you hit this assert, please check build profile config
    ct_assert(0);
#endif
}

/*!
 * @brief FUB routine to check if LOCAL_MEM_RANGE is setup or not
 *        It is expected that UDE will program LOCAL_MEMORY_RANGE GP10X onwards. 
 *        This function helps ensure that it is not possible to run FUB without running UDE
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubCheckIfLocalMemoryRangeIsSet_TU10X(void)
{
    FUB_STATUS fubStatus = FUB_ERR_GENERIC;
    LwU32      reg       = 0;

    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE, &reg));

    if (reg == 0)
    {
        fubStatus = FUB_ERR_LOCAL_MEM_RANGE_IS_NOT_SET;
    }

ErrorExit:
    return fubStatus;
}


/*
 * Save/Lock and Restore falcon reset when binary is running
 * TODO: Vinayak : Use below enum instead of bool, and rename function to SaveLockRestore
 * enum
 * {
 *  lockFalconReset_SaveAndLock,
 *  lockFalconReset_Restore = 4,
 * } LOCK_FALCON_RESET;
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubLockFalconReset_TU10X(LwBool bLock)
{
#if defined(FUB_ON_SEC2)
    LwU32      reg        = 0;
    LwU32      scratchReg = 0;
    FUB_STATUS fubStatus  = FUB_ERR_GENERIC;

    CHECK_FUB_STATUS(FALC_REG_RD32(CSB, LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK, &reg));
    if (bLock)
    {
        // Lock falcon reset for level 0, 1, 2
        scratchReg = FLD_SET_DRF_NUM( _CSEC, _SCRATCH0, _VAL, reg, scratchReg);
        CHECK_FUB_STATUS(FALC_REG_WR32(CSB, LW_CSEC_SCRATCH0, scratchReg));
        reg = FLD_SET_DRF( _CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _LEVEL0_DISABLE, reg);
        reg = FLD_SET_DRF( _CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _LEVEL1_DISABLE, reg);
        reg = FLD_SET_DRF( _CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _LEVEL2_DISABLE, reg);
    }
    else
    {
        // Restore original mask
        CHECK_FUB_STATUS(FALC_REG_RD32(CSB, LW_CSEC_SCRATCH0, &scratchReg));
        reg = FLD_SET_DRF_NUM ( _CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0,
                       DRF_VAL( _CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, scratchReg), reg);
        reg = FLD_SET_DRF_NUM ( _CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1,
                       DRF_VAL( _CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, scratchReg), reg);
        reg = FLD_SET_DRF_NUM ( _CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2,
                       DRF_VAL( _CSEC, _FALCON_RESET_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, scratchReg), reg);
    }
    CHECK_FUB_STATUS(FALC_REG_WR32(CSB, LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK, reg));
ErrorExit:
    return fubStatus;
#elif defined(FUB_ON_LWDEC)
    return FUB_ERR_NOT_SUPPORTED;
#else
    // Control is not expected to come here. If you hit this assert, please check build profile config
    ct_assert(0);
#endif
}

/*
 * fubSelwreBusSendRequest: Send request for SE bus access. 
 *                          This func only access Secure SE bus and not SE HUB.
 * @param[in]     bRead :Determines whether the SE bus access is for read or write.
 * @param[in]     addr  :Address for which secure read/write transaction would take place.
 * @param[in/out] val   :Depending upon bRead val is used either to copy data or store the read data from secure bus.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubSelwreBusSendRequest_TU10X
(
    LwBool bRead,
    LwU32 addr,
    LwU32 val
)
{
    FUB_STATUS  fubStatus    = FUB_OK;
    LwU32       regData;
    LwU32       dioReadyVal;
    //
    // Send out the read/write request onto the DIO
    // 1. Wait for channel to become empty
    //
    dioReadyVal =   DRF_DEF(_CSEC, _FALCON_DOC_CTRL, _EMPTY,       _INIT) |
                    DRF_DEF(_CSEC, _FALCON_DOC_CTRL, _WR_FINISHED, _INIT) |
                    DRF_DEF(_CSEC, _FALCON_DOC_CTRL, _RD_FINISHED, _INIT);

    do
    {
        CHECK_FUB_STATUS(FALC_REG_RD32(CSB, LW_CSEC_FALCON_DOC_CTRL, &regData));
        regData = regData & dioReadyVal;
    }
    //
    // TODO for rbhenwal to handle secure bus timeout condition.
    // There is already Bug 1732094 for the similar implementation in ACR  assigned to jamesx
    //
    while (regData != dioReadyVal);

    //
    // 2. If it is a write request the push value onto D1,
    //    otherwise set the read bit in the address
    //
    if (bRead)
    {
        addr |= FALCON_DIO_D0_READ_BIT;
    }
    else
    {
        CHECK_FUB_STATUS(FALC_REG_WR32(CSB, LW_CSEC_FALCON_DOC_D1, val));
    }

    // 3. Issue request
    CHECK_FUB_STATUS(FALC_REG_WR32(CSB, LW_CSEC_FALCON_DOC_D0, addr));
ErrorExit:
    return fubStatus;
}

/*!
 * @brief Once a read request happens through fubSelwreBusSendRequest,
 *        this function reads the data back.
 * @param[in]  addr: Address from which data needs to be fetch.
 * @param[out] pVal: Data fetched is store here to return data back to caller.
 *
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubSelwreBusGetData_TU10X
(
    LwU32                 addr,
    LwU32                *pVal
)
{
    LwU32       error;
    LwU32       data;
    LwU32       pop1Dword     = 0;
    LwU32       index;
    FUB_STATUS  fubStatus     = FUB_OK;
    
    if (NULL == pVal)
    {
       return FUB_ERR_ILWALID_ARGUMENT;
    }
    
    pop1Dword   = FLD_SET_DRF_NUM(_CSEC, _FALCON_DIO_DIC_CTRL, _POP, 0x1, pop1Dword);

    //
    // Read data from DIO
    // 1. Wait until data is available
    //
    do
    {
        CHECK_FUB_STATUS(FALC_REG_RD32(CSB, LW_CSEC_FALCON_DIC_CTRL, &data));
    }
    //
    // TODO for rbhenwal to handle secure bus timeout condition.
    // There is already Bug 1732094 for the similar implementation in ACR  assigned to jamesx
    //
    while (FLD_TEST_DRF_NUM(_CSEC, _FALCON_DIC_CTRL, _COUNT, 0x0, data));

    // 2. Pop read data
    CHECK_FUB_STATUS(FALC_REG_WR32(CSB, LW_CSEC_FALCON_DIC_CTRL, pop1Dword));

    // 2.5 Check read error from the DOC control
    CHECK_FUB_STATUS(FALC_REG_RD32(CSB, LW_CSEC_FALCON_DOC_CTRL, &error));

    //Check if there is a read error or a protocol error
    if (FLD_TEST_DRF_NUM(_CSEC, _FALCON_DIO_DOC_CTRL, _RD_ERROR, 0x1, error) ||
        FLD_TEST_DRF_NUM(_CSEC, _FALCON_DIO_DOC_CTRL, _PROTOCOL_ERROR, 0x1, error))
    {
        fubStatus = FUB_ERR_SELWRE_BUS_REQUEST_FAILED;
        goto ErrorExit;
    }

    // 3. Get the data itself
    CHECK_FUB_STATUS(FALC_REG_RD32(CSB, LW_CSEC_FALCON_DIC_D0, pVal));
     
    //
    // We have skipped PEH for SE registers in falc_ldxb_i_with_halt
    // So doing some PEH here, and allowing badfxxxx reads(valid) for TRNG registers
    // More details in Bug 200326572, and in comments of function falc_ldxb_i_with_halt
    // 
    if ((*pVal & CSB_INTERFACE_MASK_VALUE) == CSB_INTERFACE_BAD_VALUE)
    {
        for (index = 0; index < LW_SSE_SE_TRNG_RAND__SIZE_1; index++)
        {
            if(addr == LW_SSE_SE_TRNG_RAND(index))
            {
               fubStatus = FUB_OK;
               goto ErrorExit;
            }
        }
        fubStatus = FUB_ERR_PRI_FAILURE_FOR_SE_REG_ACCESS_OVER_SE_BUS;
    }
ErrorExit:
     return fubStatus;
}

/*!
 * @brief Write a register using the secure bus
 *
 * @param[in] addr  Address
 * @param[in] val   If its a write request, the value is written
 *
 * @return    FUB_STATUS whether the write was successful or not
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubSelwreBusWriteRegister_TU10X
(
    LwU32 addr,
    LwU32 val
)
{
    return fubSelwreBusSendRequest_HAL(pFub, LW_FALSE, addr, val);
}

/*!
 * @brief Read a register using the secure bus
 *
 * @param[in] addr   Address
 * @param[in] *pVal  Pointer for value to be read
 *
 * @return FUB_STATUS whether the read was successful or not
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS 
fubSelwreBusReadRegister_TU10X
(
    LwU32 addr,
    LwU32 *pVal
)
{
    FUB_STATUS fubStatus = FUB_OK;
    fubStatus = fubSelwreBusSendRequest_HAL(pFub, LW_TRUE, addr, 0);
    if (fubStatus == FUB_OK)
    {
        fubStatus = fubSelwreBusGetData_HAL(pFub, addr, pVal);
    }
    return fubStatus;
}

/*!
 * @brief Check if the VHR is empty or not
 * @return FUB_STATUS Returns FUB_OK if VHR is empty else error is returned.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubCheckIfVhrEmpty_TU104
(
    void
)
{
#if (AUTO_FUB && FUB_ON_SEC2)
    // Setup bus target.
    LwU32      baseAddr        = LW_CSEC_FALCON_VHRCFG_BASE_VAL_SEC;
    LwU32      dwCount         = (VHR_HEADER_SIZE_IN_BYTES + (VHR_ENTRY_SIZE_IN_BYTES *  LW_CSEC_FALCON_VHRCFG_ENTNUM_VAL_SEC)) >> 2;
    FUB_STATUS fubStatus       = FUB_OK;
    FUB_STATUS statusTmp       = FUB_OK;
    LwBool     bAcquireSeMutex = LW_FALSE;
    LwU32      startTime       = 0;
    LwU32      val;
    LwU32      i, offset;
    LwS32      timeoutLeft;
    //
    // Check default timeout register to verify it has not been tampered with
    // before starting mutex acquire sequence
    //
    CHECK_FUB_STATUS(fubSelwreBusReadRegister_HAL(pFub, LW_SSE_SE_CTRL_SE_MUTEX_TMOUT_DFTVAL, &val));
    
    if (val != LW_SSE_SE_CTRL_SE_MUTEX_TMOUT_DFTVAL_TMOUT_MAX)
    {
        fubStatus = FUB_ERR_SE_BUS_DEFAULT_TIMEOUT_ILWALID;
        goto ErrorExit;
    }

    // Initialize timer with current time
    CHECK_FUB_STATUS(fubReadFalconPTimer0_HAL(pFub, &startTime));
    
    // Acquire the mutex. Timeout if acquire takes too long
    do
    {
        // Obtain SE PKA mutex
        CHECK_FUB_STATUS(fubSelwreBusReadRegister_HAL(pFub, LW_SSE_SE_CTRL_PKA_MUTEX, &val));

        if (FUB_OK != (fubStatus = fubFalconPTimer0CheckTimeout_HAL(pFub, (LwU32)SE_PKA_MUTEX_TIMEOUT_VAL_USEC,
                                                startTime, &timeoutLeft)))
        {
            break;
        }
    } while (!FLD_TEST_DRF(_SSE, _SE_CTRL, _PKA_MUTEX_RC_VAL, _SUCCESS, val));

    // Check to see if mutex was acquired in case timeout oclwrred
    if (!FLD_TEST_DRF(_SSE, _SE_CTRL, _PKA_MUTEX_RC_VAL, _SUCCESS, val))
    {
        fubStatus = FUB_ERR_SE_MUTEX_ACQUIRE_ERROR;
        goto ErrorExit;
    }
    else
    {
        bAcquireSeMutex = LW_TRUE;
    }

    // set PKA mutex timeout action to be release mutex & reset PKA core
    CHECK_FUB_STATUS(fubSelwreBusWriteRegister_HAL(pFub, LW_SSE_SE_CTRL_PKA_MUTEX_TMOUT_ACTION,
                                                              DRF_DEF(_SSE, _SE_CTRL_PKA_MUTEX_TMOUT_ACTION, _INTERRUPT,     _ENABLE) |
                                                              DRF_DEF(_SSE, _SE_CTRL_PKA_MUTEX_TMOUT_ACTION, _RELEASE_MUTEX, _ENABLE) |
                                                              DRF_DEF(_SSE, _SE_CTRL_PKA_MUTEX_TMOUT_ACTION, _RESET_PKA,     _ENABLE)));
    // Loop through VHR to check if empty
    for (i = 0, offset = 0; i < dwCount; i++, offset += VHR_CHECK_OFFSET_IN_BYTES)
    {
        CHECK_FUB_STATUS(fubSelwreBusReadRegister_HAL(pFub, baseAddr + offset, &val));
        if (val != 0)
        {
            fubStatus = FUB_ERR_VHR_NOT_EMPTY;
            break;
        }
    }

ErrorExit:
    // Release SE PKA mutex if acquire was successful
    if (bAcquireSeMutex)
    { 
        statusTmp = fubSelwreBusWriteRegister_HAL(pFub,
                                                  LW_SSE_SE_CTRL_PKA_MUTEX_RELEASE,
                                                  LW_SSE_SE_CTRL_PKA_MUTEX_RELEASE_SE_LOCK_RELEASE);
        if (statusTmp != FUB_OK)
        {
            fubStatus = (fubStatus == FUB_OK ? statusTmp : fubStatus);
        }
    }
   return fubStatus;
#else
    return FUB_ERR_UNSUPPORTED_CHIP;
#endif // #ifdef AUTO_FUB
}

#endif // _FUB_INLINE_STATIC_FUNCS_TU10X_H_
