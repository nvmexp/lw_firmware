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

    http://www.HighIntegritySystems.com
*/

#ifndef SAFERTOS_API_H
#define SAFERTOS_API_H


#include "SafeRTOS.h"
#include "queue.h"
#include "task.h"
#include "semaphore.h"
#include "timers.h"
#include "eventgroups.h"
#include "mutex.h"
#include "eventpoll.h"

/*-----------------------------------------------------------
 * LWPU CODE BELOW
 *----------------------------------------------------------*/

#ifndef SAFE_RTOS_BUILD
#error This header file must not be included outside SafeRTOS build.
#endif

#endif /* SAFERTOS_API_H */

