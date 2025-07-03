/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lib_pwm.c
 * @brief   Library of PWM source control functions.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dmemovl.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib/lib_pwm.h"
#include "lwosreg.h"
#include "pmu_objpmgr.h"
#include "therm/objtherm.h"
#include "pmu_objpg.h"
#include "pmu_objdisp.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_CONFIGURE)
/*!
 * @copydoc s_pwmParamsGet_TRIGGER
 */
#define s_pwmParamsGet_CONFIGURE(pPwmSrcDesc, pPwmDutyCycle, pPwmPeriod, pBDone, bIlwert)     \
    s_pwmParamsGet_TRIGGER((pPwmSrcDesc), (pPwmDutyCycle), (pPwmPeriod), (pBDone), (bIlwert))

/*!
 * @copydoc s_pwmParamsSet_TRIGGER
 */
#define s_pwmParamsSet_CONFIGURE(pPwmSrcDesc, pwmDutyCycle, pwmPeriod, bTrigger, bIlwert)     \
    s_pwmParamsSet_TRIGGER((pPwmSrcDesc), (pwmDutyCycle), (pwmPeriod), (bTrigger), (bIlwert))
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_CONFIGURE)

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Static Function Prototypes  -------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * This mask denotes all of the PWM source descriptors constructed for PWM
 * sources of type RM_PMU_PMGR_PWM_SOURCE_<xyz>. We use this to prevent
 * multiple allocations for same PWM source assuring that PWM source can be
 * used only by a single client.
 */
UPROC_STATIC LwU32 pwmSourceDescriptorsConstructedMask = 0;

/*!
 * Make sure we don't exceed the number of PWM sources than the number of bits
 * in the allocated @ref pwmSourceDescriptorsConstructedMask.
 */
ct_assert(RM_PMU_PMGR_PWM_SOURCE__COUNT <= (8U * sizeof(pwmSourceDescriptorsConstructedMask)));

#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_DEFAULT)
static FLCN_STATUS  s_pwmParamsGet_DEFAULT(PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc, LwU32 *pPwmDutyCycle, LwU32 *pPwmPeriod, LwBool *pBDone, LwBool bIlwert)
    GCC_ATTRIB_SECTION("imem_libPwm", "s_pwmParamsGet_DEFAULT");
static FLCN_STATUS  s_pwmParamsSet_DEFAULT(PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc, LwU32 pwmDutyCycle, LwU32 pwmPeriod, LwBool bTrigger, LwBool bIlwert)
    GCC_ATTRIB_SECTION("imem_libPwm", "s_pwmParamsSet_DEFAULT");
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_DEFAULT)

#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_TRIGGER)
static FLCN_STATUS  s_pwmParamsGet_TRIGGER(PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc, LwU32 *pPwmDutyCycle, LwU32 *pPwmPeriod, LwBool *pBDone, LwBool bIlwert)
    GCC_ATTRIB_SECTION("imem_libPwm", "s_pwmParamsGet_TRIGGER");
static FLCN_STATUS  s_pwmParamsSet_TRIGGER(PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc, LwU32 pwmDutyCycle, LwU32 pwmPeriod, LwBool bTrigger, LwBool bIlwert)
    GCC_ATTRIB_SECTION("imem_libPwm", "s_pwmParamsSet_TRIGGER");
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_TRIGGER)

#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_CONFIGURE)
static FLCN_STATUS  s_pwmSourceConfigure_CONFIGURE(PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc, LwBool bEnable)
    GCC_ATTRIB_SECTION("imem_libPwm", "s_pwmSourceConfigure_CONFIGURE");
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_CONFIGURE)

#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_CLKSRC)
static FLCN_STATUS  s_pwmParamsGet_CLKSRC(PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc, LwU32 *pPwmDutyCycle, LwU32 *pPwmPeriod, LwBool *pBDone, LwBool bIlwert)
    GCC_ATTRIB_SECTION("imem_libPwm", "s_pwmParamsGet_CLKSRC");
static FLCN_STATUS  s_pwmParamsSet_CLKSRC(PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc, LwU32 pwmDutyCycle, LwU32 pwmPeriod, LwBool bTrigger, LwBool bIlwert)
    GCC_ATTRIB_SECTION("imem_libPwm", "s_pwmParamsSet_CLKSRC");
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_CLKSRC)

/* ------------------------- Compile-Time Checks ---------------------------- */
/*
 * Ensure that at least one type of PWM is supported on this profile.
 */
ct_assert(PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_DEFAULT) ||
          PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_TRIGGER) ||
          PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_CONFIGURE));

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Constructs a PWM source descriptor for requested PWM source
 *
 * @note    This function assumes that the overlay described by @ref ovlIdxDmem
 *          is already attached by the caller.
 *
 * @param[in]  ovlIdxDmem  DMEM Overlay index in which to construct the PWM
                           source descriptor
 * @param[in]  pwmSource   Requested PWM source as RM_PMU_PMGR_PWM_SOURCE_<xyz>
 *
 * @return  PWM_SOURCE_DESCRIPTOR   Pointer to PWM_SOURCE_DESCRIPTOR structure.
 * @return  NULL                    In case of invalid PWM source.
 */
PWM_SOURCE_DESCRIPTOR *
pwmSourceDescriptorConstruct
(
    LwU8                   ovlIdxDmem,
    RM_PMU_PMGR_PWM_SOURCE pwmSource
)
{
    if (RM_PMU_PMGR_PWM_SOURCE_IS_BIT_SET(pwmSource, MASK_THERM))
    {
        return thermPwmSourceDescriptorConstruct_HAL(&Therm, pwmSource, ovlIdxDmem);
    }
    else if (RM_PMU_PMGR_PWM_SOURCE_IS_BIT_SET(pwmSource, MASK_SCI_FAN) ||
             RM_PMU_PMGR_PWM_SOURCE_IS_BIT_SET(pwmSource, MASK_PMGR))
    {
        return pmgrPwmSourceDescriptorConstruct_HAL(&Pmgr, pwmSource, ovlIdxDmem);
    }
    else if (RM_PMU_PMGR_PWM_SOURCE_IS_BIT_SET(pwmSource, MASK_SCI_VID_PWM))
    {
        return pgIslandPwmSourceDescriptorConstruct_HAL(&Pg, pwmSource, ovlIdxDmem);
    }
    else if (RM_PMU_PMGR_PWM_SOURCE_IS_BIT_SET(pwmSource, MASK_DISP))
    {
        return dispPwmSourceDescriptorConstruct_HAL(&Disp, pwmSource, ovlIdxDmem);
    }
    else
    {
        // Nothing to do.
    }

    return NULL;
}

/*!
 * This function may be used to retrieve the PWM control parameters for a
 * given PWM source descriptor.
 *
 * @param[in]   pPwmSrcDesc     Pointer to descriptor of the PWM source driving the PWM
 * @param[out]  pPwmDutyCycle   Buffer in which to return duty cycle in raw units
 * @param[out]  pPwmPeriod      Buffer in which to return period in raw units
 * @param[out]  pBDone          Buffer in which to return Done status, TRUE/FALSE
 * @param[in]   bIlwert         Denotes if PWM source should be ilwerted
 *
 * @return  FLCN_OK                     PWM parameters successfully obtained.
 * @return  FLCN_ERR_NOT_SUPPORTED      PWM source descriptor type is not supported.
 * @return  FLCN_ERR_ILWALID_ARGUMENT   Provided pointer is NULL
 * @return  Other unexpected errors     Unexpected errors propagated from other functions.
 */
FLCN_STATUS
pwmParamsGet_IMPL
(
    PWM_SOURCE_DESCRIPTOR  *pPwmSrcDesc,
    LwU32                  *pPwmDutyCycle,
    LwU32                  *pPwmPeriod,
    LwBool                 *pBDone,
    LwBool                  bIlwert
)
{
    FLCN_STATUS status;

    if ((pPwmSrcDesc == NULL) ||
        (pPwmDutyCycle == NULL) ||
        (pPwmPeriod == NULL) ||
        (pBDone == NULL))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    switch (pPwmSrcDesc->type)
    {
#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_DEFAULT)
        case PWM_SOURCE_DESCRIPTOR_TYPE_DEFAULT:
        {
            status = s_pwmParamsGet_DEFAULT(pPwmSrcDesc, pPwmDutyCycle, pPwmPeriod,
                    pBDone, bIlwert);
            break;
        }
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_DEFAULT)
#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_TRIGGER)
        case PWM_SOURCE_DESCRIPTOR_TYPE_TRIGGER:
        {
            status = s_pwmParamsGet_TRIGGER(pPwmSrcDesc, pPwmDutyCycle, pPwmPeriod,
                    pBDone, bIlwert);
            break;
        }
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_TRIGGER)
#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_CONFIGURE)
        case PWM_SOURCE_DESCRIPTOR_TYPE_CONFIGURE:
        {
            status = s_pwmParamsGet_CONFIGURE(pPwmSrcDesc, pPwmDutyCycle, pPwmPeriod,
                    pBDone, bIlwert);
            break;
        }
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_CONFIGURE)
#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_CLKSRC)
        case PWM_SOURCE_DESCRIPTOR_TYPE_CLKSRC:
        {
            status = s_pwmParamsGet_CLKSRC(pPwmSrcDesc, pPwmDutyCycle, pPwmPeriod,
                    pBDone, bIlwert);
            break;
        }
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_CLKSRC)
        default:
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NOT_SUPPORTED;
            break;
        }
    }

    return status;
}

/*!
 * This function may be used to set the PWM control parameters for a given PWM
 * source descriptor.
 *
 * @param[in]   pPwmSrcDesc     Pointer to descriptor of the PWM source driving the PWM
 * @param[in]   pwmDutyCycle    Duty cycle to set, in raw units
 * @param[in]   pwmPeriod       Period to set, in raw units
 * @param[in]   bTrigger        Boolean to trigger the PWM settings into the HW
 * @param[in]   bIlwert         Denotes if PWM source should be ilwerted
 *
 * @return  FLCN_OK                     PWM parameters updated successfully.
 * @return  FLCN_ERR_NOT_SUPPORTED      PWM source descriptor type is not supported.
 * @return  FLCN_ERR_ILWALID_ARGUMENT   Provided pointer is NULL
 * @return  Other unexpected errors     Unexpected errors propagated from other functions.
 */
FLCN_STATUS
pwmParamsSet
(
    PWM_SOURCE_DESCRIPTOR  *pPwmSrcDesc,
    LwU32                   pwmDutyCycle,
    LwU32                   pwmPeriod,
    LwBool                  bTrigger,
    LwBool                  bIlwert
)
{
    FLCN_STATUS status;

    if (pPwmSrcDesc == NULL)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    switch (pPwmSrcDesc->type)
    {
#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_DEFAULT)
        case PWM_SOURCE_DESCRIPTOR_TYPE_DEFAULT:
        {
            status = s_pwmParamsSet_DEFAULT(pPwmSrcDesc, pwmDutyCycle, pwmPeriod,
                    bTrigger, bIlwert);
            break;
        }
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_DEFAULT)
#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_TRIGGER)
        case PWM_SOURCE_DESCRIPTOR_TYPE_TRIGGER:
        {
            status = s_pwmParamsSet_TRIGGER(pPwmSrcDesc, pwmDutyCycle, pwmPeriod,
                    bTrigger, bIlwert);
            break;
        }
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_TRIGGER)
#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_CONFIGURE)
        case PWM_SOURCE_DESCRIPTOR_TYPE_CONFIGURE:
        {
            status = s_pwmParamsSet_CONFIGURE(pPwmSrcDesc, pwmDutyCycle, pwmPeriod,
                    bTrigger, bIlwert);
            break;
        }
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_CONFIGURE)
#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_CLKSRC)
        case PWM_SOURCE_DESCRIPTOR_TYPE_CLKSRC:
        {
            status = s_pwmParamsSet_CLKSRC(pPwmSrcDesc, pwmDutyCycle, pwmPeriod,
                    bTrigger, bIlwert);
            break;
        }
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_CLKSRC)
        default:
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NOT_SUPPORTED;
            break;
        }
    }

    return status;
}

#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_ACT_GET)
/*!
 * Used to retrieve an actual period and duty cycle of the PWM signal that is
 * being detected by the given PWM source.
 *
 * @param[in]  pwmSource     Enum of the PWM source driving the PWM signal
 * @param[out] pPwmDutyCycle LwU32 * in which to return the duty cycle in raw
 *                           units
 * @param[out] pPwmPeriod    LwU32 * in which to return the period in raw units
 * @param[in]  bIlwert       Denotes if PWM source should be ilwerted
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT if at least one pointer argument is NULL or
 *                                  an invalid pwmSource specified (not
 *                                  supporting PWM detection).
 * @return FLCN_OK otherwise.
 */
FLCN_STATUS
pwmParamsActGet
(
    RM_PMU_PMGR_PWM_SOURCE  pwmSource,
    LwU32                  *pPwmDutyCycle,
    LwU32                  *pPwmPeriod,
    LwBool                  bIlwert
)
{
    FLCN_STATUS status = FLCN_ERR_ILWALID_ARGUMENT;

    if ((pPwmDutyCycle != NULL) &&
        (pPwmPeriod != NULL))
    {
        if (RM_PMU_PMGR_PWM_SOURCE_IS_BIT_SET(pwmSource, MASK_THERM))
        {
            status = thermPwmParamsActGet_HAL(&Therm, pwmSource, pPwmDutyCycle,
                        pPwmPeriod);
        }
        else if (RM_PMU_PMGR_PWM_SOURCE_IS_BIT_SET(pwmSource, MASK_SCI_FAN))
        {
            status = pmgrSciPwmParamsActGet_HAL(&Pmgr, pwmSource, pPwmDutyCycle,
                        pPwmPeriod);
        }
        else
        {
            // Error assigned above.
        }
    }

    if (status == FLCN_OK)
    {
        // Ilwert the duty cycle if required
        if (bIlwert)
        {
            *pPwmDutyCycle = *pPwmPeriod - *pPwmDutyCycle;
        }
    }

    return status;
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_ACT_GET)

/*!
 * @brief   Allocate the PWM source descriptor structure for the requested PWM
 *          source of type @ref pPwmSrcDescData->type.
 *
 * @note    This function should only be called from within the HALs called in
 *          @ref pwmSourceDescriptorConstruct.
 *
 * @param[in]   ovlIdxDmem      DMEM overlay index to use to allocate the
 *                              PWM_SOURCE_DESCRIPTOR structure
 * @param[in]   pwmSource       Requested PWM source as RM_PMU_PMGR_PWM_SOURCE_<xyz>
 *
 * @param[in]   pPwmSrcDescData Argument containing PWM descriptor data to be copied
 *                              into the allocated descriptor as per @ref pPwmSrcDescData->type.
 *
 * @return  PWM_SOURCE_DESCRIPTOR   Pointer to the PWM_SOURCE_DESCRIPTOR structure.
 * @return  NULL                    In case a PWM_SOURCE_DESCRIPTOR structure
 *                                  is already allocated for @ref pwmSource or
 *                                  the @ref pwmSource is invalid.
 */
PWM_SOURCE_DESCRIPTOR *
pwmSourceDescriptorAllocate_IMPL
(
    LwU8                    ovlIdxDmem,
    RM_PMU_PMGR_PWM_SOURCE  pwmSource,
    PWM_SOURCE_DESCRIPTOR  *pPwmSrcDescData
)
{
    PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc = NULL;
    LwU32                  size;

    if (pPwmSrcDescData == NULL)
    {
        PMU_BREAKPOINT();
        goto pwmSourceDescriptorAllocate_exit;
    }

    // Allocate only once per valid PWM source.
    if (((LWBIT32(pwmSource) & pwmSourceDescriptorsConstructedMask) == 0U) &&
        (pwmSource < RM_PMU_PMGR_PWM_SOURCE__COUNT))
    {
        switch (pPwmSrcDescData->type)
        {
#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_DEFAULT)
            case PWM_SOURCE_DESCRIPTOR_TYPE_DEFAULT:
            {
                size = sizeof(PWM_SOURCE_DESCRIPTOR);
                break;
            }
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_DEFAULT)
#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_TRIGGER)
            case PWM_SOURCE_DESCRIPTOR_TYPE_TRIGGER:
            {
                size = sizeof(PWM_SOURCE_DESCRIPTOR_TRIGGER);
                break;
            }
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_TRIGGER)
#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_CONFIGURE)
            case PWM_SOURCE_DESCRIPTOR_TYPE_CONFIGURE:
            {
                size = sizeof(PWM_SOURCE_DESCRIPTOR_CONFIGURE);
                break;
            }
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_CONFIGURE)
#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_CLKSRC)
            case PWM_SOURCE_DESCRIPTOR_TYPE_CLKSRC:
            {
                size = sizeof(PWM_SOURCE_DESCRIPTOR_CLKSRC);
                break;
            }
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_CLKSRC)
            default:
            {
                size = LW_U32_MAX;
                PMU_BREAKPOINT();
                break;
            }
        }

        if (size == LW_U32_MAX)
        {
            goto pwmSourceDescriptorAllocate_exit;
        }

        pPwmSrcDesc = (PWM_SOURCE_DESCRIPTOR *)lwosCalloc(ovlIdxDmem, size);
        if (pPwmSrcDesc == NULL)
        {
            PMU_BREAKPOINT();
            goto pwmSourceDescriptorAllocate_exit;
        }

        // Set the bit corresponding to PWM source in the mask on successfull allocation.
        pwmSourceDescriptorsConstructedMask |= LWBIT32(pwmSource);

        (void)memcpy(pPwmSrcDesc, pPwmSrcDescData, size);
    }

pwmSourceDescriptorAllocate_exit:
    return pPwmSrcDesc;
}

#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_CONFIGURE)
/*!
 * This function is used to configure a PWM source via given PWM
 * source descriptor.
 *
 * @param[in]   pPwmSrcDesc Pointer to descriptor of the PWM source driving the PWM
 * @param[in]   bEnable     Boolean denoting whether to enable/disable the PWM source
 *
 * @return  FLCN_ERR_NOT_SUPPORTED      PWM source descriptor type is not supported.
 * @return  FLCN_ERR_ILWALID_ARGUMENT   Provided pointer is NULL
 * @return  Other unexpected errors     Unexpected errors propagated from other functions.
 */
FLCN_STATUS
pwmSourceConfigure
(
    PWM_SOURCE_DESCRIPTOR  *pPwmSrcDesc,
    LwBool                  bEnable
)
{
    FLCN_STATUS status;

    if (pPwmSrcDesc == NULL)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    switch (pPwmSrcDesc->type)
    {
        case PWM_SOURCE_DESCRIPTOR_TYPE_CONFIGURE:
        {
            status =  s_pwmSourceConfigure_CONFIGURE(pPwmSrcDesc, bEnable);
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NOT_SUPPORTED;
            break;
        }
    }

    return status;
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_CONFIGURE)

/* ------------------------- Private Functions ------------------------------ */
#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_DEFAULT)
/*!
 * This function may be used to retrieve the PWM control parameters for the
 * given PWM source descriptor of type PWM_SOURCE_DESCRIPTOR_TYPE_DEFAULT.
 *
 * @note    When provided by valid pPwmSrcDesc this call will never fail.
 *          Also note that interface do not check input parameters for NULL.
 *
 * @param[in]   pPwmSrcDesc     Pointer to descriptor of the PWM source driving the PWM
 * @param[out]  pPwmDutyCycle   Buffer in which to return duty cycle in raw units
 * @param[out]  pPwmPeriod      Buffer in which to return period in raw units
 * @param[out]  pBDone          Buffer in which to return Done status, TRUE/FALSE
 * @param[in]   bIlwert         Denotes if PWM source should be ilwerted
 *
 * @return  FLCN_OK Always succeeds (for now)
 */
static FLCN_STATUS
s_pwmParamsGet_DEFAULT
(
    PWM_SOURCE_DESCRIPTOR  *pPwmSrcDesc,
    LwU32                  *pPwmDutyCycle,
    LwU32                  *pPwmPeriod,
    LwBool                 *pBDone,
    LwBool                  bIlwert
)
{
    LwU32 period    = osRegRd32(pPwmSrcDesc->bus, pPwmSrcDesc->addrPeriod);
    LwU32 dutycycle = osRegRd32(pPwmSrcDesc->bus, pPwmSrcDesc->addrDutycycle);

    *pPwmPeriod    = (period & pPwmSrcDesc->mask);
    *pPwmDutyCycle = (dutycycle & pPwmSrcDesc->mask);
    *pBDone        = ((dutycycle & LWBIT32(pPwmSrcDesc->doneIdx)) == 0U);

    // Ilwert the duty cycle if required.
    if (bIlwert)
    {
        *pPwmDutyCycle = *pPwmPeriod - *pPwmDutyCycle;
    }

    return FLCN_OK;
}

/*!
 * This function may be used to set the PWM control parameters for the
 * given PWM source descriptor of type PWM_SOURCE_DESCRIPTOR_TYPE_DEFAULT.
 *
 * @param[in]   pPwmSrcDesc     Pointer to descriptor of the PWM source driving the PWM
 * @param[in]   pwmDutyCycle    Duty cycle to set, in raw units
 * @param[in]   pwmPeriod       Period to set, in raw units
 * @param[in]   bTrigger        Boolean to trigger the PWM settings into the HW
 * @param[in]   bIlwert         Denotes if PWM source should be ilwerted
 *
 * @return  FLCN_OK Always succeeds (for now)
 *
 * @pre     This function assumes that PWM control has already been enabled
 *          on GPU and corresponding GPIO properly configured.
 */
static FLCN_STATUS
s_pwmParamsSet_DEFAULT
(
    PWM_SOURCE_DESCRIPTOR  *pPwmSrcDesc,
    LwU32                   pwmDutyCycle,
    LwU32                   pwmPeriod,
    LwBool                  bTrigger,
    LwBool                  bIlwert
)
{
    LwU32 period    = osRegRd32(pPwmSrcDesc->bus, pPwmSrcDesc->addrPeriod);
    LwU32 oldPeriod = period;
    LwU32 dutycycle = osRegRd32(pPwmSrcDesc->bus, pPwmSrcDesc->addrDutycycle);
    LwU32 mask      = pPwmSrcDesc->mask;

    // Ilwert the duty cycle if required.
    if (bIlwert)
    {
        pwmDutyCycle = pwmPeriod - pwmDutyCycle;
    }

    //
    // Configuring PWM values while previous update is pending may lead to PWM
    // corruption so cancel previous pending update before applying new settings
    //
    if (pPwmSrcDesc->bCancel)
    {
        LwU32 tmp = dutycycle & ~LWBIT32(pPwmSrcDesc->triggerIdx);

        if (tmp != dutycycle)
        {
            osRegWr32(pPwmSrcDesc->bus, pPwmSrcDesc->addrDutycycle, tmp);
        }
    }

    period    = (~mask & period) | (mask & pwmPeriod);
    dutycycle = (~mask & dutycycle) | (mask & pwmDutyCycle);

    if (bTrigger && pPwmSrcDesc->bTrigger)
    {
        dutycycle |= LWBIT32(pPwmSrcDesc->triggerIdx);
    }

    if (period != oldPeriod)
    {
        osRegWr32(pPwmSrcDesc->bus, pPwmSrcDesc->addrPeriod, period);
    }

    osRegWr32(pPwmSrcDesc->bus, pPwmSrcDesc->addrDutycycle, dutycycle);

    return FLCN_OK;
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_DEFAULT)

#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_TRIGGER)
/*!
 * This function may be used to retrieve the PWM control parameters for the
 * given PWM source descriptor of type PWM_SOURCE_DESCRIPTOR_TYPE_TRIGGER.
 *
 * @note    When provided by valid pPwmSrcDesc this call will never fail.
 *          Also note that interface do not check input parameters for NULL.
 *
 * @param[in]   pPwmSrcDesc     Pointer to descriptor of the PWM source driving the PWM
 * @param[out]  pPwmDutyCycle   Buffer in which to return duty cycle in raw units
 * @param[out]  pPwmPeriod      Buffer in which to return period in raw units
 * @param[out]  pBDone          Buffer in which to return Done status, TRUE/FALSE
 * @param[in]   bIlwert         Denotes if PWM source should be ilwerted
 *
 * @return  FLCN_OK Always succeeds (for now)
 */
static FLCN_STATUS
s_pwmParamsGet_TRIGGER
(
    PWM_SOURCE_DESCRIPTOR  *pPwmSrcDesc,
    LwU32                  *pPwmDutyCycle,
    LwU32                  *pPwmPeriod,
    LwBool                 *pBDone,
    LwBool                  bIlwert
)
{
    PWM_SOURCE_DESCRIPTOR_TRIGGER *pPwmSrcDescTrigger =
        (PWM_SOURCE_DESCRIPTOR_TRIGGER *)((void *)pPwmSrcDesc);

    LwU32 period    = osRegRd32(pPwmSrcDesc->bus, pPwmSrcDesc->addrPeriod);
    LwU32 dutycycle = osRegRd32(pPwmSrcDesc->bus, pPwmSrcDesc->addrDutycycle);
    LwU32 trigger   = osRegRd32(pPwmSrcDesc->bus, pPwmSrcDescTrigger->addrTrigger);

    *pPwmPeriod    = (period & pPwmSrcDesc->mask);
    *pPwmDutyCycle = (dutycycle & pPwmSrcDesc->mask);
    *pBDone        = ((trigger & LWBIT32(pPwmSrcDesc->doneIdx)) == 0U);

    // Ilwert the duty cycle if required.
    if (bIlwert)
    {
        *pPwmDutyCycle = *pPwmPeriod - *pPwmDutyCycle;
    }

    return FLCN_OK;
}

/*!
 * This function may be used to set the PWM control parameters for the
 * given PWM source descriptor of type PWM_SOURCE_DESCRIPTOR_TYPE_TRIGGER.
 *
 * @param[in]   pPwmSrcDesc     Pointer to descriptor of the PWM source driving the PWM
 * @param[in]   pwmDutyCycle    Duty cycle to set, in raw units
 * @param[in]   pwmPeriod       Period to set, in raw units
 * @param[in]   bTrigger        Boolean to trigger the PWM settings into the HW
 * @param[in]   bIlwert         Denotes if PWM source should be ilwerted
 *
 * @return  FLCN_OK Always succeeds (for now)
 *
 * @pre     This function assumes that PWM control has already been enabled
 *          on GPU and corresponding GPIO properly configured.
 */
static FLCN_STATUS
s_pwmParamsSet_TRIGGER
(
    PWM_SOURCE_DESCRIPTOR  *pPwmSrcDesc,
    LwU32                   pwmDutyCycle,
    LwU32                   pwmPeriod,
    LwBool                  bTrigger,
    LwBool                  bIlwert
)
{
    PWM_SOURCE_DESCRIPTOR_TRIGGER *pPwmSrcDescTrigger =
        (PWM_SOURCE_DESCRIPTOR_TRIGGER *)((void *)pPwmSrcDesc);

    LwU32 period    = osRegRd32(pPwmSrcDesc->bus, pPwmSrcDesc->addrPeriod);
    LwU32 oldPeriod = period;
    LwU32 dutycycle = osRegRd32(pPwmSrcDesc->bus, pPwmSrcDesc->addrDutycycle);
    LwU32 trigger   = osRegRd32(pPwmSrcDesc->bus, pPwmSrcDescTrigger->addrTrigger);
    LwU32 mask      = pPwmSrcDesc->mask;

    // Ilwert the duty cycle if required.
    if (bIlwert)
    {
        pwmDutyCycle = pwmPeriod - pwmDutyCycle;
    }

    //
    // Configuring PWM values while previous update is pending may lead to PWM
    // corruption so cancel previous pending update before applying new settings
    //
    if (pPwmSrcDesc->bCancel)
    {
        LwU32 tmp = trigger & ~LWBIT32(pPwmSrcDesc->triggerIdx);

        if (tmp != trigger)
        {
            osRegWr32(pPwmSrcDesc->bus, pPwmSrcDescTrigger->addrTrigger, tmp);
        }
    }

    period    = (~mask & period) | (mask & pwmPeriod);
    dutycycle = (~mask & dutycycle) | (mask & pwmDutyCycle);

    if (bTrigger && pPwmSrcDesc->bTrigger)
    {
        trigger |= LWBIT32(pPwmSrcDesc->triggerIdx);
    }

    if (period != oldPeriod)
    {
        osRegWr32(pPwmSrcDesc->bus, pPwmSrcDesc->addrPeriod, period);
    }

    osRegWr32(pPwmSrcDesc->bus, pPwmSrcDesc->addrDutycycle, dutycycle);
    osRegWr32(pPwmSrcDesc->bus, pPwmSrcDescTrigger->addrTrigger, trigger);

    return FLCN_OK;
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_TRIGGER)

#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_CONFIGURE)
/*!
 * This function is used to configure a PWM source via given PWM
 * source descriptor of type PWM_SOURCE_DESCRIPTOR_TYPE_CONFIGURE.
 *
 * @note    This function does not check input parameters for NULL.
 *
 * @param[in]   pPwmSrcDesc Pointer to descriptor of the PWM source driving the PWM
 * @param[in]   bEnable     Boolean denoting whether to enable/disable the PWM source
 *
 * @return  FLCN_OK                 In case PWM source was configured successfully
 * @return  FLCN_ERR_NOT_SUPPORTED  In case configuration of PWM source is not
 *                                  supported.
 */
static FLCN_STATUS
s_pwmSourceConfigure_CONFIGURE
(
    PWM_SOURCE_DESCRIPTOR  *pPwmSrcDesc,
    LwBool                  bEnable
)
{
    FLCN_STATUS status               = FLCN_OK;
    PWM_SOURCE_DESCRIPTOR_CONFIGURE
               *pPwmSrcDescConfigure = (PWM_SOURCE_DESCRIPTOR_CONFIGURE *)((void *)pPwmSrcDesc);
    LwU8        enableIdx            = pPwmSrcDescConfigure->enableIdx;
    LwU32       reg32;
    LwU32       oldReg32;

    // This PWM source cannot be enabled/disabled.
    if (!pPwmSrcDescConfigure->bEnable)
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        goto s_pwmSourceConfigure_CONFIGURE_exit;
    }


    reg32    = osRegRd32(pPwmSrcDesc->bus, pPwmSrcDesc->addrPeriod);
    oldReg32 = reg32;
    reg32    = bEnable ? (reg32 | LWBIT32(enableIdx)) : (reg32 & ~LWBIT32(enableIdx));

    if (reg32 != oldReg32)
    {
        osRegWr32(pPwmSrcDesc->bus, pPwmSrcDesc->addrPeriod, reg32);
    }

s_pwmSourceConfigure_CONFIGURE_exit:
    return status;
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_CONFIGURE)

#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_CLKSRC)
/*!
 * This function may be used to retrieve the PWM control parameters for the
 * given PWM source descriptor of type PWM_SOURCE_DESCRIPTOR_TYPE_CLKSRC.
 *
 * @note    When provided by valid pPwmSrcDesc this call will never fail.
 *          Also note that interface do not check input parameters for NULL.
 *
 * @param[in]   pPwmSrcDesc     Pointer to descriptor of the PWM source driving the PWM
 * @param[out]  pPwmDutyCycle   Buffer in which to return duty cycle in raw units
 * @param[out]  pPwmPeriod      Buffer in which to return period in raw units
 * @param[out]  pBDone          Buffer in which to return Done status, TRUE/FALSE
 * @param[in]   bIlwert         Denotes if PWM source should be ilwerted
 *
 * @return  FLCN_OK Always succeeds (for now)
 */
static FLCN_STATUS
s_pwmParamsGet_CLKSRC
(
    PWM_SOURCE_DESCRIPTOR  *pPwmSrcDesc,
    LwU32                  *pPwmDutyCycle,
    LwU32                  *pPwmPeriod,
    LwBool                 *pBDone,
    LwBool                  bIlwert
)
{
    LwU32 period    = osRegRd32(pPwmSrcDesc->bus, pPwmSrcDesc->addrPeriod);
    LwU32 dutycycle = osRegRd32(pPwmSrcDesc->bus, pPwmSrcDesc->addrDutycycle);

    *pPwmPeriod    = (period & pPwmSrcDesc->mask);
    *pPwmDutyCycle = (dutycycle & pPwmSrcDesc->mask);
    *pBDone        = ((dutycycle & LWBIT32(pPwmSrcDesc->doneIdx)) == 0U);

    // Ilwert the duty cycle if required.
    if (bIlwert)
    {
        *pPwmDutyCycle = *pPwmPeriod - *pPwmDutyCycle;
    }

    return FLCN_OK;
}

/*!
 * This function may be used to set the PWM control parameters for the
 * given PWM source descriptor of type PWM_SOURCE_DESCRIPTOR_TYPE_CLKSRC.
 *
 * @param[in]   pPwmSrcDesc     Pointer to descriptor of the PWM source driving the PWM
 * @param[in]   pwmDutyCycle    Duty cycle to set, in raw units
 * @param[in]   pwmPeriod       Period to set, in raw units
 * @param[in]   bTrigger        Boolean to trigger the PWM settings into the HW
 * @param[in]   bIlwert         Denotes if PWM source should be ilwerted
 *
 * @return  FLCN_OK Always succeeds (for now)
 *
 * @pre     This function assumes that PWM control has already been enabled
 *          on GPU and corresponding GPIO properly configured.
 */
static FLCN_STATUS
s_pwmParamsSet_CLKSRC
(
    PWM_SOURCE_DESCRIPTOR  *pPwmSrcDesc,
    LwU32                   pwmDutyCycle,
    LwU32                   pwmPeriod,
    LwBool                  bTrigger,
    LwBool                  bIlwert
)
{
    PWM_SOURCE_DESCRIPTOR_CLKSRC *pPwmSrcDescClkSrc =
        (PWM_SOURCE_DESCRIPTOR_CLKSRC *)((void *)pPwmSrcDesc);

    LwU32 period         = osRegRd32(pPwmSrcDesc->bus, pPwmSrcDesc->addrPeriod);
    LwU32 dutycycle      = osRegRd32(pPwmSrcDesc->bus, pPwmSrcDesc->addrDutycycle);
    LwU32 mask           = pPwmSrcDesc->mask;
    LwU32 clkSelBaseIdx  = pPwmSrcDescClkSrc->clkSelBaseIdx;
    LwU32 clkSelValue    = pPwmSrcDescClkSrc->clkSel; 
    LwU32 clkSelTmp      = 0;

    // Ilwert the duty cycle if required.
    if (bIlwert)
    {
        pwmDutyCycle = pwmPeriod - pwmDutyCycle;
    }

    //
    // Configuring PWM values while previous update is pending may lead to PWM
    // corruption so cancel previous pending update before applying new settings
    //
    if (pPwmSrcDesc->bCancel)
    {
        LwU32 tmp = dutycycle & ~LWBIT32(pPwmSrcDesc->triggerIdx);

        if (tmp != dutycycle)
        {
            osRegWr32(pPwmSrcDesc->bus, pPwmSrcDesc->addrDutycycle, tmp);
        }
    }

    period    = (~mask & period) | (mask & pwmPeriod);
    dutycycle = (~mask & dutycycle) | (mask & pwmDutyCycle);
    clkSelTmp = (clkSelValue << clkSelBaseIdx);
    dutycycle |= clkSelTmp;

    if (bTrigger && pPwmSrcDesc->bTrigger)
    {
        dutycycle |= LWBIT32(pPwmSrcDesc->triggerIdx);
    }

    osRegWr32(pPwmSrcDesc->bus, pPwmSrcDesc->addrPeriod, period);
    osRegWr32(pPwmSrcDesc->bus, pPwmSrcDesc->addrDutycycle, dutycycle);

    return FLCN_OK;
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PWM_CLKSRC)
