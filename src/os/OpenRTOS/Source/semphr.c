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

#include "OpenRTOS.h"
#include "queue.h"
#include "semphr.h"

/*-----------------------------------------------------------
 * PUBLIC SEMAPHORE API dolwmented in semphr.h
 *----------------------------------------------------------*/

/*!
 * @copydoc xSemaphoreTake
 */
signed portBASE_TYPE xSemaphoreTake( xSemaphoreHandle xSemaphore, portTickType xBlockTime )
{
    return xQueueReceive( xSemaphore, NULL, semSEMAPHORE_QUEUE_ITEM_LENGTH, xBlockTime );
}

/*!
 * @copydoc xSemaphoreGive
 */
signed portBASE_TYPE xSemaphoreGive( xSemaphoreHandle xSemaphore )
{
    return xQueueSend( xSemaphore, NULL, semSEMAPHORE_QUEUE_ITEM_LENGTH, semGIVE_BLOCK_TIME );
}

/*!
 * @copydoc xSemaphoreGiveFromISR
 */
signed portBASE_TYPE xSemaphoreGiveFromISR( xSemaphoreHandle xSemaphore, signed portBASE_TYPE xTaskPreviouslyWoken )
{
    signed portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

    xQueueSendFromISR( xSemaphore, NULL, semSEMAPHORE_QUEUE_ITEM_LENGTH, &xHigherPriorityTaskWoken );

    return xHigherPriorityTaskWoken;
}

