/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrdev_ba.h
 * @copydoc pwrdev_ba.c
 */

#ifndef PWRDEV_BA_H
#define PWRDEV_BA_H

/* ------------------------ System Includes -------------------------------- */

/* ------------------------ Application Includes --------------------------- */
/* ------------------------ Types Definitions ------------------------------ */
/*!
 * This structure is a placeholder for data common to all BA power devices.
 */
typedef struct
{
    /*!
     * If set, this flag denotes that at least one BA device is present and its
     * common data were properly initialized (can be used at later init stages).
     */
    LwBool bInitializedAndUsed;
    /*!
     * utilsClk value in KHz passed down from RM for BA energy counter 
     * callwlation.
     */
    LwU32  utilsClkkHz;

    /*!
     * scaleACompensationFactor = numGPCs enabled / num GPCs with BA enabled
     *
     * scaleA is multiplied by scaleACompensationFactor as a SW WAR for
     * bug 200433679.
     */
#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_BA_WAR_200433679))
    LwUFXP20_12 scaleACompensationFactor;
#endif
} PWR_DEVICE_BA_INFO;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/*!
 * Helper macro to enable/disable BA units within monitored partitions,
 * controlling whether those units send weighted BA samples to the BA
 * aclwmulators/fifos in the PWR/THERM.  This effectively disables/eanbles the
 * entire BA HW feature, applying to all possible BA PWR_DEVICEs.
 *
 * @param[in] bEnable
 *     Boolean indicating whether to enable/disable the BA units.
 */
#define pwrDevBaEnable(bEnable)                                               \
    thermPwrDevBaEnable_HAL(&Therm, bEnable)

/* ------------------------ Function Prototypes ---------------------------- */

#endif // PWRDEV_BA_H
