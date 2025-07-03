/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

/*!
 * @file   soe_objifr.c
 * @brief  Container-object for SOE IFR routines. Contains generic non-HAL
 *         IFR routines plus logic required to hook-up chip-specific
 *         IFR HAL-routines.
 */

/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"

/* ------------------------- Application Includes --------------------------- */
#include "soe_objhal.h"
#include "soe_objsoe.h"
#include "soe_objifr.h"
#include "soe_objspi.h"

#include "config/g_ifr_hal.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
IFR_HAL_IFACES IfrHal;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */

void
constructIfr(void)
{
    IfaceSetup->ifrHalIfacesSetupFn(&IfrHal);
}

FLCN_STATUS
initInforomFs(void)
{
    FLCN_STATUS ret = FLCN_OK;
    LwU32 gcSectorThreshold, fsStatus;
    LwU32 fsVersion = 0;

    if (!spiRomIsInitialized())
        return FLCN_ERROR;

    // spiRomReadLocal will automatically add the inforom offset,
    // so the offset needed here is offset of the version in the superblock
    ret = spiRomReadLocal(&spiRom, LW_OFFSETOF(IFS_SUPERBLOCK, version),
                            sizeof(LwU32), (LwU8*)&fsVersion);
    if (ret != FLCN_OK)
    {
        return ret;
    }

    ret = ifrGetGcThreshold_HAL(pIfr, &gcSectorThreshold);
    if (ret != FLCN_OK)
    {
        return ret;
    }

    g_pFs = inforomFsConstruct(NULL, fsVersion, gcSectorThreshold);
    if (g_pFs == NULL)
    {
        return FLCN_ERROR;
    }

    fsStatus = inforomFsLoad(g_pFs, LW_FALSE);
    if (fsStatus != FS_OK)
    {
        inforomFsDestruct(g_pFs);
        return FLCN_ERROR;
    }

    return FLCN_OK;
}

// Local implementations of inforomFS platform functions

FS_STATUS
fsAllocMem
(
    void **pAddress,
    LwU32 size
)
{
    *pAddress = lwosMalloc(0, size);

    if (*pAddress == NULL)
        return FS_ERR_NO_FREE_MEM;

    return FS_OK;
}

FS_STATUS
fsAllocZeroMem
(
    void **pAddress,
    LwU32 size
)
{
    *pAddress = lwosCalloc(0, size);

    if (*pAddress == NULL)
        return FS_ERR_NO_FREE_MEM;

    return FS_OK;
}


void
fsFreeMem
(
    void *pAddress
)
{
    // SOE does not implement free()
    return;
}
LwU8*
fsMemCopy
(
    void *pDst,
    const void *pSrc,
    LwU32 count
)
{
    return memcpy(pDst, pSrc, count);
}

void *
fsMemSet
(
    void *pDst,
    LwU8 val,
    LwU32 count
)
{
    return memset(pDst, val, count);
}

int
fsMemCmp
(
    const void *s1,
    const void *s2,
    LwU32 size
)
{
    return memcmp(s1, s2, size);
}

FS_STATUS
readFlash
(
    void  *pFlashContext,
    LwU32 offset,
    LwU32 size,
    LwU8  *pOutBuffer
)
{
    if (spiRomReadLocal(&spiRom, offset, size, pOutBuffer) != FLCN_OK)
    {
        return FS_ERROR;
    }
    return FS_OK;
}

FS_STATUS
writeFlash
(
    void  *pFlashContext,
    LwU32 offset,
    LwU32 size,
    LwU8  *pInBuffer
)
{
        if (spiRomWriteLocal(&spiRom, offset, size, pInBuffer) != FLCN_OK)
        {
            return FS_ERROR;
        }

        return FS_OK;
}

FS_STATUS
eraseFlash
(
    void  *pFlashContext,
    LwU32 offset,
    LwU32 size
)
{
    if (spiRomErase(&spiRom.super, offset, size) != FLCN_OK)
    {
        return FS_ERROR;
    }

    return FS_OK;
}

