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

#include "queue.h"

#ifndef SEMAPHORE_H
#define SEMAPHORE_H

typedef xQueueHandle xSemaphoreHandle;

#define semBINARY_SEMAPHORE_QUEUE_LENGTH    ( ( unsigned portCHAR ) 1 )
#define semSEMAPHORE_QUEUE_ITEM_LENGTH      ( ( unsigned portCHAR ) 0 )
#define semGIVE_BLOCK_TIME                  ( ( portTickType ) 0 )


/**
 * <i>Macros</i> that implement a semaphore by using the
 * existing queue mechanism. The queue length is 1 as this is a
 * binary semaphore.  The data size is 0 as we don't want to
 * actually store any data - we just want to know if the queue
 * is empty or full.
 *
 * @param xSemaphore Handle to the created semaphore.  Should be of type xSemaphoreHandle.
 * @param ucOvlIdx Index of the target DMEM overlay.
 *
 * \defgroup vSemaphoreCreateBinary vSemaphoreCreateBinary
 * \ingroup Semaphores
 */

#define vSemaphoreCreateBinary_Taken( xSemaphore )                             \
    {                                                                          \
        xSemaphore = xQueueCreate( semBINARY_SEMAPHORE_QUEUE_LENGTH,           \
                                   semSEMAPHORE_QUEUE_ITEM_LENGTH );           \
    }

#define vSemaphoreCreateBinary( xSemaphore )                                   \
    {                                                                          \
        vSemaphoreCreateBinary_Taken( xSemaphore );                            \
        if( xSemaphore != NULL )                                               \
        {                                                                      \
            xSemaphoreGive( xSemaphore );                                      \
        }                                                                      \
    }

#define vSemaphoreCreateBinaryInOvl_Taken( xSemaphore, ucOvlIdx )              \
    {                                                                          \
        xSemaphore = xQueueCreateInOvl( semBINARY_SEMAPHORE_QUEUE_LENGTH,      \
                                        semSEMAPHORE_QUEUE_ITEM_LENGTH,        \
                                        ucOvlIdx );                            \
    }

#define vSemaphoreCreateBinaryInOvl( xSemaphore, ucOvlIdx )                    \
    {                                                                          \
        vSemaphoreCreateBinaryInOvl_Taken( xSemaphore, ucOvlIdx );             \
        if( xSemaphore != NULL )                                               \
        {                                                                      \
            xSemaphoreGive( xSemaphore );                                      \
        }                                                                      \
    }

/**
 * <i>Macro</i> to obtain a semaphore.  The semaphore must of been created using
 * vSemaphoreCreateBinary ().
 *
 * @param xSemaphore A handle to the semaphore being obtained.  This is the
 * handle returned by vSemaphoreCreateBinary ();
 *
 * @param xBlockTime The time in ticks to wait for the semaphore to become
 * available.  The macro portTICK_RATE_MS can be used to colwert this to a
 * real time.  A block time of zero can be used to poll the semaphore.
 *
 * @return pdTRUE if the semaphore was obtained.  pdFALSE if xBlockTime
 * expired without the semaphore becoming available.
 *
 * \defgroup xSemaphoreTake xSemaphoreTake
 * \ingroup Semaphores
 */
signed portBASE_TYPE xSemaphoreTake( xSemaphoreHandle xSemaphore, portTickType xBlockTime );

/**
 * <i>Macro</i> to release a semaphore.  The semaphore must of been created using
 * vSemaphoreCreateBinary (), and obtained using sSemaphoreTake ().
 *
 * This must not be used from an ISR.  See xSemaphoreGiveFromISR () for
 * an alternative which can be used from an ISR.
 *
 * @param xSemaphore A handle to the semaphore being released.  This is the
 * handle returned by vSemaphoreCreateBinary ();
 *
 * @return pdTRUE if the semaphore was released.  pdFALSE if an error oclwrred.
 * Semaphores are implemented using queues.  An error can occur if there is
 * no space on the queue to post a message - indicating that the
 * semaphore was not first obtained correctly.
 *
 * \defgroup xSemaphoreGive xSemaphoreGive
 * \ingroup Semaphores
 */
signed portBASE_TYPE xSemaphoreGive( xSemaphoreHandle xSemaphore );

/**
 * <i>Macro</i> to  release a semaphore.  The semaphore must of been created using
 * vSemaphoreCreateBinary (), and obtained using xSemaphoreTake ().
 *
 * This macro can be used from an ISR.
 *
 * @param xSemaphore A handle to the semaphore being released.  This is the
 * handle returned by vSemaphoreCreateBinary ();
 *
 * @param sTaskPreviouslyWoken This is included so an ISR can make multiple calls
 * to xSemaphoreGiveFromISR () from a single interrupt.  The first call
 * should always pass in pdFALSE.  Subsequent calls should pass in
 * the value returned from the previous call.  See the file serial .c in the
 * PC port for a good example of using xSemaphoreGiveFromISR ().
 *
 * @return pdTRUE if a task was woken by releasing the semaphore.  This is
 * used by the ISR to determine if a context switch may be required following
 * the ISR.
 *
 * \defgroup xSemaphoreGiveFromISR xSemaphoreGiveFromISR
 * \ingroup Semaphores
 */
signed portBASE_TYPE xSemaphoreGiveFromISR( xSemaphoreHandle xSemaphore, signed portBASE_TYPE xTaskPreviouslyWoken );

#endif

