/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWOS_DMA_LOCK_H
#define LWOS_DMA_LOCK_H

/* ------------------------- System Includes -------------------------------- */
#include "flcnifcmn.h"
#include "lwuproc.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
#ifdef DMA_SUSPENSION

LwBool lwosDmaSuspend(LwBool bIgnoreLock);
void   lwosDmaResume(void);
LwBool lwosDmaIsSuspended(void);
void   lwosDmaExitSuspension(void);
LwBool lwosDmaSuspensionNoticeISR(void);
void   vApplicationDmaSuspensionNoticeISR(void)
    GCC_ATTRIB_SECTION("imem_resident", "vApplicationDmaSuspensionNoticeISR");

#else // DMA_SUSPENSION

#define lwosDmaSuspend(bIgnoreLock)             LW_TRUE
#define lwosDmaResume()                         lwosNOP()
#define lwosDmaIsSuspended()                    LW_FALSE
#define lwosDmaExitSuspension()                 lwosNOP()
#define lwosDmaSuspensionNoticeISR()            LW_FALSE
#define vApplicationDmaSuspensionNoticeISR()    lwosNOP()

#endif // DMA_SUSPENSION

#define lwosDmaLockAcquire(...) MOCKABLE(lwosDmaLockAcquire)(__VA_ARGS__)
/*!
 * @brief Lock DMA to avoid DMA suspensions
 *
 * OS and DMA APIs calls this function to make sure that OS will own DMA until
 * further notice.
 *
 * Other notes:
 * This function should be called only by OS or other OS API. Tasks should not
 * directly call this API.
 *
 * @sa lwosDmaLockRelease
 */
void   lwosDmaLockAcquire   (void);

#define lwosDmaLockRelease(...) MOCKABLE(lwosDmaLockRelease)(__VA_ARGS__)
/*!
 * @brief Release DMA lock
 *
 * DMA APIs calls this function to release lock. Tasks can suspend DMA after
 * releasing lock.
 *
 * Other notes:
 * This function should be called only by OS or other OS API. Tasks should not
 * directly call this API.
 *
 * @sa lwosDmaLockAcquire
 */
void   lwosDmaLockRelease   (void);

#endif // LWOS_DMA_LOCK_H
