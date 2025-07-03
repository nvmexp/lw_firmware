#pragma once

#include "lwtypes.h"
#include "inforom/status.h"

FS_STATUS     fsAllocMem(void **, LwU32);
FS_STATUS     fsAllocZeroMem(void **pAddress, LwU32 size);
void          fsFreeMem(void *);
LwU8*         fsMemCopy(void *, const void *, LwU32);
void*         fsMemSet(void *, LwU8, LwU32);
int           fsMemCmp(const void *, const void *, LwU32);
FS_STATUS     fsGetLwrrentTime(LwU32 *, LwU32 *);
