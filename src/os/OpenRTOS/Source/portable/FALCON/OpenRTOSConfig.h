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

#ifndef OPENRTOS_CONFIG_H
#define OPENRTOS_CONFIG_H

#include <falcon-intrinsics.h>

/*-----------------------------------------------------------
 * Application specific definitions.
 *
 * These definitions should be adjusted for your particular hardware and
 * application requirements.
 *
 * THESE PARAMETERS ARE DESCRIBED WITHIN THE 'CONFIGURATION' SECTION OF THE
 * OpenRTOS API DOCUMENTATION AVAILABLE ON THE OpenRTOS.org WEB SITE.
 *----------------------------------------------------------*/

#define configUSE_IDLE_HOOK                  (1)
#define configMAX_PRIORITIES                 ((unsigned portBASE_TYPE)8)
#define configMINIMAL_STACK_SIZE             ((unsigned portLONG ) 64)
#define configABORT(x)                       falc_halt()

/* Set the following definitions to 1 to include the API function, or zero
to exclude the API function. */

#define INCLUDE_vTaskPrioritySet             (1)
#define INCLUDE_uxTaskPriorityGet            (1)
#define INCLUDE_vTaskSuspend                 (1)
#define INCLUDE_vTaskDelayUntil              (1)
#define INCLUDE_vTaskDelay                   (1)

#endif  /* OPENRTOS_CONFIG_H */
