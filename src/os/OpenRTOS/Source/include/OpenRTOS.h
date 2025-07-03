/*
 * OpenRTOS - Copyright (C) Wittenstein High Integrity Systems.
 *
 * OpenRTOS is distributed exclusively by Wittenstein High Integrity Systems,
 * and is subject to the terms of the License granted to your organization,
 * including its warranties and limitations on distribution.  It cannot be
 * copied or reproduced in any way except as permitted by the License.
 *
 * Licenses are issued for each conlwrrent user working on a specified product
 * line.
 *
 * WITTENSTEIN high integrity systems is a trading name of WITTENSTEIN
 * aerospace & simulation ltd, Registered Office: Brown's Court, Long Ashton
 * Business Park, Yanley Lane, Long Ashton, Bristol, BS41 9LB, UK.
 * Tel: +44 (0) 1275 395 600, fax: +44 (0) 1275 393 630.
 * E-mail: info@HighIntegritySystems.com
 * Registered in England No. 3711047; VAT No. GB 729 1583 15
 *
 * http://www.HighIntegritySystems.com
 */

#ifndef OPEN_RTOS_BUILD
    #error This header file must not be included outside OpenRTOS build.
#endif

#ifndef INC_OPENRTOS_H
#define INC_OPENRTOS_H


/*
 * Include the generic headers required for the OpenRTOS port being used.
 */
#include <stddef.h>

/* Basic OpenRTOS definitions. */
#include "projdefs.h"

/* Application specific configuration options. */
#include "OpenRTOSConfig.h"

/* Definitions specific to the port being used. */
#include "portable.h"

/*
 * Check all the required application specific macros have been defined.
 * These macros are application specific and (as downloaded) are defined
 * within OpenRTOSConfig.h.
 */

#ifndef configUSE_IDLE_HOOK
    #error Missing definition:  configUSE_IDLE_HOOK should be defined in OpenRTOSConfig.h as either 1 or 0.  See the Configuration section of the OpenRTOS API documentation for details.
#endif

#ifndef INCLUDE_vTaskPrioritySet
    #error Missing definition:  INCLUDE_vTaskPrioritySet should be defined in OpenRTOSConfig.h as either 1 or 0.  See the Configuration section of the OpenRTOS API documentation for details.
#endif

#ifndef INCLUDE_uxTaskPriorityGet
    #error Missing definition:  INCLUDE_uxTaskPriorityGet should be defined in OpenRTOSConfig.h as either 1 or 0.  See the Configuration section of the OpenRTOS API documentation for details.
#endif

#ifndef INCLUDE_vTaskSuspend
    #error Missing definition:  INCLUDE_vTaskSuspend     should be defined in OpenRTOSConfig.h as either 1 or 0.  See the Configuration section of the OpenRTOS API documentation for details.
#endif

#ifndef INCLUDE_vTaskDelayUntil
    #error Missing definition:  INCLUDE_vTaskDelayUntil should be defined in OpenRTOSConfig.h as either 1 or 0.  See the Configuration section of the OpenRTOS API documentation for details.
#endif

#ifndef INCLUDE_vTaskDelay
    #error Missing definition:  INCLUDE_vTaskDelay should be defined in OpenRTOSConfig.h as either 1 or 0.  See the Configuration section of the OpenRTOS API documentation for details.
#endif

#ifndef configABORT
    #define configABORT(x)   printf(x); exit(-1)
#endif

#endif
