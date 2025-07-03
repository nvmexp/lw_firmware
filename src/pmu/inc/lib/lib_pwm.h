/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lib_pwm.h
 * @copydoc lib_pwm.c
 */

#ifndef LIB_PWM_H
#define LIB_PWM_H

#include "g_lib_pwm.h"

#ifndef G_LIB_PWM_H
#define G_LIB_PWM_H
/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Types Definitions ------------------------------ */
/*!
 * PWM source descriptor is a placeholder for all necessary information required
 * to perform get() / set() operations on given PWM source. Clients are expected
 * to cache this information at init time.
 *
 * @note    All assumptions listed below are enforced by compile-time checks.
 */
typedef struct
{
    /*!
     * PWM_SOURCE_DESCRIPTOR_TYPE_<xyz> enum specifying the descriptor type.
     */
    LwU8    type;

    /*!
     * Trigger bit index to trigger a PWM update. This indexes in the corresponding
     * trigger register determined by PWM_SOURCE_DESCRIPTOR_TYPE_<xyz>.
     *
     * @note    Assumption was made that this bit has to be set to trigger update.
     */
    LwU8    triggerIdx;

    /*!
     * Done bit index denoting a PWM update was completed. This indexes in the
     * corresponding trigger register determined by PWM_SOURCE_DESCRIPTOR_TYPE_<xyz>.
     *
     * @note    Assumption was made that PWM update is complete when this bit is cleared.
     */
    LwU8    doneIdx;

    /*!
     * REG_BUS_<xyz> specifying bus that must be used to access @ref addrPeriod and
     * @ref addrDutycycle.
     */
    LwU8    bus;

    /*!
     * Address of the register holding PWM period (see @ref bus).
     */
    LwU32   addrPeriod;

    /*!
     * Address of the register holding PWM duty cycle (see @ref bus).
     */
    LwU32   addrDutycycle;

    /*!
     * Bit-mask used to fetch/apply data to @ref addrPeriod and @ref addrDutycycle.
     *
     * @note    Assumption was made that both registers hold data in fields at
     *          same bit-positions so single mask is sufficient.
     */
    LwU32   mask;

    /*!
     * Some PWM sources require cancellation of previous set request if pending.
     */
    LwBool  bCancel;

    /*!
     * Some PWM sources don't need to set the trigger for PWM change.
     */
    LwBool  bTrigger;
} PWM_SOURCE_DESCRIPTOR;

/*!
 * PWM source descriptor holding additional address of the trigger register
 * required to update PWM change.
 */
typedef struct
{
    /*!
     * PWM_SOURCE_DESCRIPTOR super-class. This should always be the first member!
     */
    PWM_SOURCE_DESCRIPTOR   super;

    /*!
     * Address of the register holding trigger bits to update PWM change.
     * @ref super::triggerIdx and @ref super::doneIdx indexes denote the
     * respective functionality this field can perform.
     */
    LwU32                   addrTrigger;
} PWM_SOURCE_DESCRIPTOR_TRIGGER;

/*!
 * PWM source descriptor holding additional configuration data.
 */
typedef struct
{
    /*!
     * PWM_SOURCE_DESCRIPTOR_TRIGGER super-class.
     * This should always be the first member!
     */
    PWM_SOURCE_DESCRIPTOR_TRIGGER   super;

    /*!
     * Enable bit index to enable/disable the PWM source.
     *
     * @note    Assumption was made that this bit has to be set to enable/disable the
     *          PWM source and indexes in @ref super::super::addrPeriod.
     */
    LwU8                            enableIdx;

    /*!
     * Boolean indicating whether this PWM source can be enabled/disabled.
     */
    LwBool                          bEnable;
} PWM_SOURCE_DESCRIPTOR_CONFIGURE;

/*!
 * PWM source descriptor holding additional address of the clock source
 * required to update PWM change.
 */
typedef struct
{
    /*!
     * PWM_SOURCE_DESCRIPTOR super-class. This should always be the first member!
     */
    PWM_SOURCE_DESCRIPTOR   super;

    /*!
     * CLKSEL value to select CLK source of the PWM source.
     */
    LwU32                   clkSel;

    /*!
     * CLKSEL base index to select CLK source of the PWM source.
     */
    LwU32                   clkSelBaseIdx;
} PWM_SOURCE_DESCRIPTOR_CLKSRC;

/*!
 *  PWM_SOURCE_DESCRIPTOR_TYPE enum used in @ref PWM_SOURCE_DESCRIPTOR.
 */
#define PWM_SOURCE_DESCRIPTOR_TYPE_ILWALID    0x00000000
#define PWM_SOURCE_DESCRIPTOR_TYPE_DEFAULT    0x00000001
#define PWM_SOURCE_DESCRIPTOR_TYPE_TRIGGER    0x00000002
#define PWM_SOURCE_DESCRIPTOR_TYPE_CONFIGURE  0x00000003
#define PWM_SOURCE_DESCRIPTOR_TYPE_CLKSRC     0x00000004

/* ------------------------- Macros ----------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
mockable PWM_SOURCE_DESCRIPTOR* pwmSourceDescriptorAllocate(LwU8 ovlIdxDmem, RM_PMU_PMGR_PWM_SOURCE pwmSource, PWM_SOURCE_DESCRIPTOR *pPwmSrcDescData)
    GCC_ATTRIB_SECTION("imem_libPwmConstruct", "pwmSourceDescriptorAllocate");

PWM_SOURCE_DESCRIPTOR *pwmSourceDescriptorConstruct(LwU8 ovlIdxDmem, RM_PMU_PMGR_PWM_SOURCE pwmSource)
    GCC_ATTRIB_SECTION("imem_libPwmConstruct", "pwmSourceDescriptorConstruct");

FLCN_STATUS            pwmSourceConfigure(PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc, LwBool bEnable)
    GCC_ATTRIB_SECTION("imem_libPwm", "pwmSourceConfigure");

mockable FLCN_STATUS   pwmParamsGet(PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc, LwU32 *pPwmDutyCycle, LwU32 *pPwmPeriod, LwBool *pBDone, LwBool bIlwert)
    GCC_ATTRIB_SECTION("imem_libPwm", "pwmParamsGet");

FLCN_STATUS            pwmParamsSet(PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc, LwU32 pwmDutyCycle, LwU32 pwmPeriod, LwBool bTrigger, LwBool bIlwert)
    GCC_ATTRIB_SECTION("imem_libPwm", "pwmParamsSet");

FLCN_STATUS            pwmParamsActGet(RM_PMU_PMGR_PWM_SOURCE pwmSource, LwU32 *pPwmDutyCycle, LwU32 *pPwmPeriod, LwBool bIlwert)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommon", "pwmParamsActGet");

#endif // G_LIB_PWM_H
#endif // LIB_PWM_H

