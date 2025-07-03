/*
    Copyright (C)2006 onward WITTENSTEIN aerospace & simulation limited. All rights reserved.

    This file is part of the SafeRTOS product, see projdefs.h for version number
    information.

    SafeRTOS is distributed exclusively by WITTENSTEIN high integrity systems,
    and is subject to the terms of the License granted to your organization,
    including its warranties and limitations on use and distribution. It cannot be
    copied or reproduced in any way except as permitted by the License.

    Licenses authorize use by processor, compiler, business unit, and product.

    WITTENSTEIN high integrity systems is a trading name of WITTENSTEIN
    aerospace & simulation ltd, Registered Office: Brown's Court, Long Ashton
    Business Park, Yanley Lane, Long Ashton, Bristol, BS41 9LB, UK.
    Tel: +44 (0) 1275 395 600, fax: +44 (0) 1275 393 630.
    E-mail: info@HighIntegritySystems.com

    www.HighIntegritySystems.com
*/


#ifndef RUNTIMESTATS_H
#define RUNTIMESTATS_H

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/

/* The following macros are for use by application tasks. */

typedef struct xRunTimeStatistics
{
    portUInt32Type ulRunTimeCounter1;
    portUInt32Type ulRunTimeCounter2;
    portUInt32Type ulLastRunTimeCounter1;
    portUInt32Type ulLastRunTimeCounter2;
    portUInt32Type ulPeriodBaseTime1;
    portUInt32Type ulPeriodBaseTime2;
} xRTS;

typedef struct xPercentRecord
{
    portUInt32Type ulLwrrent;
    portUInt32Type ulMax;
} xPERCENT;

typedef struct xPercentages
{
    xPERCENT xOverall;
    xPERCENT xPeriod;
} xPERCENTAGES;

/*-----------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xCallwlateCPUUsage( portTaskHandleType xHandle, xPERCENTAGES *const pxPercentages );

/*-----------------------------------------------------------------------------
 * Functions for internal use only.  Not to be called directly from a host
 * application or task.
 *---------------------------------------------------------------------------*/
KERNEL_INIT_FUNCTION void vInitialiseRunTimeStatistics( void );
KERNEL_FUNCTION void vUpdateRunTimeStatistics( void );
KERNEL_CREATE_FUNCTION void vInitialiseTaskRunTimeStatistics( portTaskHandleType xHandle );

/*---------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* RUNTIMESTATS_H */

