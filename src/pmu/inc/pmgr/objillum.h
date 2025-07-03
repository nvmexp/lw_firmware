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
 * @file    objillum.h
 * @brief   Common ILLUM related defines (public interfaces).
 *
 * Detailed documentation is located at:
 * https://confluence.lwpu.com/display/BS/RGB+illumination+control+-+RID+69692
 */

#ifndef OBJILLUM_H
#define OBJILLUM_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ OpenRTOS Includes ------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "pmgr/illum_device.h"
#include "pmgr/illum_zone.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   Colwenience macros for the ILLUM overlays attachment.
 *
 * @note    Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM_LS after
 *          updates to this macro. Failure to do so might result in falcon halt.
 */
#define OSTASK_OVL_DESC_DEFINE_ILLUM_CONSTRUCT                                 \
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, perfVf)                          \
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libLeakage)                      \
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libFxpBasic)                     \
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libFxpExtended)                  \
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, pmgrLibConstruct)                \
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibIllumConstruct)

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Structure holding all Illumination related data.
 */
typedef struct
{
    /*!
     * Board Object Group of all ILLUM_DEVICE objects.
     */
    ILLUM_DEVICES   devices;

    /*!
     * Board Object Group of all ILLUM_ZONES objects.
     */
    ILLUM_ZONES     zones;
} OBJILLUM;

/* ------------------------ External Definitions ---------------------------- */
extern OBJILLUM Illum;

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */

#endif // OBJILLUM_H

