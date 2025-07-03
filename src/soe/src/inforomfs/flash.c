#include "inforom/flash.h"
#include "inforom/fs.h"
#include "inforom/flash.h"
#include "flash_access.h"
#include "oslib.h"

void ifsFlashTracingStart
(
    IFS *pFs,
    FLASHACCESSTYPE type,
    const char pName[IFS_FILE_NAME_SIZE],
    LwU32 offset,
    void *buf,
    LwU32 size,
    INFOROM_FLASH_ACCESS **ppFlashAccess
)
{
#ifdef FS_OPTION_FLASH_TRACING
    LwU32 sec, usec;
    INFOROM_FLASH_ACCESS *pFlashAccess;
    LwU32 *pIdx;
    LwU32 bufSize = (size < FLASH_ACCESS_BUF_SIZE) ? size : FLASH_ACCESS_BUF_SIZE;
    FS_STATUS status;

    if (!pFs->bEnableFlashTracing)
        return;

    if (!pFs->flashAccesses.firstEntries[FLASH_ACCESS_ARRAY_SIZE - 1].time_start)
    {
        pIdx = &pFs->flashAccesses.firstEntryIdx;
        pFlashAccess = pFs->flashAccesses.firstEntries;
    }
    else
    {
        pIdx = &pFs->flashAccesses.lastEntryIdx;
        pFlashAccess = pFs->flashAccesses.lastEntries;
    }

    pFlashAccess = &pFlashAccess[*pIdx];
    *pIdx = (*pIdx + 1) % FLASH_ACCESS_ARRAY_SIZE;

    // Overriding previous data
    if (pFlashAccess->time_start)
    {
        if (pFlashAccess->buf)
        {
            fsFreeMem(pFlashAccess->buf);
        }
        // Zero out so we don't have stale data
        fsMemSet(pFlashAccess, 0, sizeof(INFOROM_FLASH_ACCESS));
    }

    fsGetLwrrentTime(&sec, &usec);
    fsMemCopy(pFlashAccess->name, pName, IFS_FILE_NAME_SIZE);
    pFlashAccess->type = type;
    pFlashAccess->offset = offset;
    pFlashAccess->size = size;
    pFlashAccess->time_start = sec;
    if (buf)
    {
        status = fsAllocMem((void **)&pFlashAccess->buf, bufSize);
        if (status != FS_OK || !pFlashAccess->buf)
            return;

        fsMemCopy(pFlashAccess->buf, buf, bufSize);
    }

    *ppFlashAccess = pFlashAccess;
#endif // FS_OPTION_FLASH_TRACING
}

void ifsFlashTracingEnd
(
    IFS *pFs,
    LwU8 *buf,
    LwU32 size,
    FS_STATUS flashStatus,
    INFOROM_FLASH_ACCESS *pFlashAccess
)
{
#ifdef FS_OPTION_FLASH_TRACING
    LwU32 sec, usec;
    LwU32 bufSize = (size < FLASH_ACCESS_BUF_SIZE) ? size : FLASH_ACCESS_BUF_SIZE;
    FS_STATUS status;

    if (!pFs->bEnableFlashTracing)
        return;

    if (!pFlashAccess)
        return;

    if (buf)
    {
        FS_ASSERT(pFlashAccess->buf == NULL);
        status = fsAllocMem((void **)&pFlashAccess->buf, bufSize);
        if (status != FS_OK || !pFlashAccess->buf)
            return;

        fsMemCopy(pFlashAccess->buf, buf, bufSize);
    }

    fsGetLwrrentTime(&sec, &usec);
    pFlashAccess->time_end = sec;
    pFlashAccess->status = flashStatus;
#endif // FS_OPTION_FLASH_TRACING
}

void ifsFlashAccessDestruct(IFS *pFs)
{
#ifdef FS_OPTION_FLASH_TRACING
    int i;

    for (i = 0; i < FLASH_ACCESS_ARRAY_SIZE; i++)
    {
        if (pFs->flashAccesses.firstEntries[i].buf)
            fsFreeMem(pFs->flashAccesses.firstEntries[i].buf);

        if (pFs->flashAccesses.lastEntries[i].buf)
            fsFreeMem(pFs->flashAccesses.lastEntries[i].buf);
    }
#endif
}
