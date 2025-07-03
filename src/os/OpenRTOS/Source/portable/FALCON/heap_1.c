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

/*
 * The simplest possible implementation of pvPortMalloc().  Note that this
 * implementation does NOT allow allocated memory to be freed again.
 *
 * See heap_2.c and heap_3.c for alternative implementations, and the memory
 * management pages of http://www.OpenRTOS.org for more information.
 */
#include "portmacro.h"

/* Check if supported heap byte alignment walue was specified. */
#if ((portBYTE_ALIGNMENT != 4) && (portBYTE_ALIGNMENT != 2) && (portBYTE_ALIGNMENT != 1))
    #error "Invalid portBYTE_ALIGNMENT definition"
#endif

/*
 * The heap_1.c implementation of the dynamic memory allocator was reimplemented
 * to support LWPU's concept of DMEM overlays.  This file was kept to preserve
 * copyright notice and this message, @see lwosMallocAligned().
 */
