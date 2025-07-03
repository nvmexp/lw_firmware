/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "inforom/map.h"
#include "inforom/misc.h"
#include "oslib.h"

FS_STATUS ifsMapCreate(LwU32 numEntries, IFS_MAP **pMapOut)
{
    IFS_MAP *pMap;
    FS_STATUS status;
    LwU32 i;

    FS_ASSERT_AND_RETURN(numEntries != 0, FS_ERR_ILWALID_ARG);
    FS_ASSERT_AND_RETURN(numEntries < IFS_MAP_ENTRY_RESERVED, FS_ERR_ILWALID_ARG);

    status = fsAllocMem((void **)&pMap, sizeof(IFS_MAP) + (numEntries * sizeof(LwU32)));
    if (status != FS_OK)
        return status;

    pMap->numEntries = numEntries;

    for (i = 0; i < numEntries; i++)
    {
        pMap->entry[i] = IFS_MAP_ENTRY_DISCARD;
    }

    *pMapOut = pMap;
    return FS_OK;
}

FS_STATUS ifsMapGet(IFS_MAP *pMap, LwU32 addr, LwU32 *pValue)
{
    FS_ASSERT_AND_RETURN(addr < pMap->numEntries, FS_ERR_ILWALID_ARG);
    *pValue = pMap->entry[addr];
    return FS_OK;
}

FS_STATUS ifsMapSet(IFS_MAP *pMap, LwU32 addr, LwU32 value)
{
    FS_ASSERT_AND_RETURN(addr < pMap->numEntries, FS_ERR_ILWALID_ARG);
    pMap->entry[addr] = value;
    return FS_OK;
}

void ifsMapDestroy(IFS_MAP *pMap)
{
    if (pMap != NULL)
        fsFreeMem(pMap);
}

LwU32 ifsMapIterStart(IFS_MAP_ITERATOR *pIter, IFS_MAP *pMap, LwU32 firstAddr)
{
    if (firstAddr >= pMap->numEntries)
        return IFS_MAP_ENTRY_ILWALID;

    pIter->pMap = pMap;
    pIter->firstAddr = firstAddr;
    pIter->lwrAddr = firstAddr;
    return firstAddr;
}

LwU32 ifsMapIterNext(IFS_MAP_ITERATOR *pIter, LwU32 stride)
{
    pIter->lwrAddr += stride;
    if (pIter->lwrAddr >= pIter->pMap->numEntries)
        pIter->lwrAddr -= pIter->pMap->numEntries;
    if (pIter->lwrAddr == pIter->firstAddr)
        return IFS_MAP_ENTRY_ILWALID;
    return pIter->lwrAddr;
}
