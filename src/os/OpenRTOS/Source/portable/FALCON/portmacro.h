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

#ifndef PORTMACRO_H
#define PORTMACRO_H

/* ------------------------ Data types for Coldfire ----------------------- */
#define portCHAR        char
#define portFLOAT       float
#define portDOUBLE      double
#define portLONG        long
#define portSHORT       short
#define portSTACK_TYPE  unsigned int
#define portBASE_TYPE   int

typedef unsigned int portTickType;
#define portMAX_DELAY ( portTickType ) 0xffffffff

/* ------------------------ Architecture specifics ------------------------ */
#define portBYTE_ALIGNMENT              4

/* ------------------------ OpenRTOS macros for port ---------------------- */
extern void lwrtosYield(void);

#define portENTER_CRITICAL()                                                 \
    vPortEnterCritical()

#define portEXIT_CRITICAL()                                                  \
    vPortExitCritical()

#define portYIELD()   lwrtosYield()

/*
 * Call into the LWOS layer to request that it load the task.
 * This request is made just before the scheduler hands control
 * to any task it has selected. If the port does not require
 * SW-assisted loading (ie. all HW- managed) it may chose to
 * simply ignore this request.
 *
 * @param pxTask The handle of the task to load. If NULL, the current task (ie.
 * the task which is in the RUNNING state) is selected by default.
 *
 * @return pxLoadTimeTicks The time in OS ticks it took to load the task.
 *
 * @return A strictly-negative integer if the overlay could not be loaded. In this
 * case, the scheduler must select a new task to run.
 *
 * @return Zero if the overlay was previously loaded (100%).
 *
 * @return A strictly-postive integer if the task was successfully loaded.
 */
int xPortLoadTask( void * xTask, unsigned int *pxLoadTimeTicks );

/* ------------------------ Function prototypes --------------------------- */
void      vPortEnterCritical    (void) __attribute__((__noinline__));
void      vPortExitCritical     (void) __attribute__((__noinline__));

/* ------------------------ Compiler specifics ---------------------------- */

#define portTASK_FUNCTION( vFunction, pvParameters )                         \
    void vFunction( void *pvParameters )
#endif

