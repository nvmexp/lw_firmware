/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef INFOROM_FS_H
#define INFOROM_FS_H

#include "lwtypes.h"
#include "inforom/status.h"
#include "soeififr.h"

#define IFS_FILE_NAME_SIZE     INFOROM_FS_FILE_NAME_SIZE

// Public-facing InfoROM File System interface.

typedef struct IFS IFS;

IFS* inforomFsConstruct(void *pFlashAccessor, LwU32 version,
                        LwU32 gcSectorThreshold);

FS_STATUS inforomFsLoad(IFS *pFs, LwBool bReadOnly);

FS_STATUS inforomFsWriteEnable(IFS *pFs);

FS_STATUS inforomFsRead(IFS *pFs, const char pName[], LwU32 offset, LwU32 size, LwU8 *pOutBuffer);
FS_STATUS inforomFsWrite(IFS *pFs, const char pName[], LwU32 offset, LwU32 size, LwU8 *pInBuffer);

FS_STATUS inforomFsGetFileSize(IFS *pFs, const char pName[], LwU32 *pSize);
FS_STATUS inforomFsGetFileType(IFS *pFs, const char pName[], LwU32 *pType);
FS_STATUS inforomFsGetFileList(IFS *pFs, char (*pNameList)[], LwU32 *pFileCount);

FS_STATUS inforomFsSetGCEnabled(IFS *pFs, LwBool bEnableGC);
FS_STATUS inforomFsIsGCNeeded(IFS *pFs, LwBool *pIsGcNeeded);
FS_STATUS inforomFsDoGC(IFS *pFs);

FS_STATUS inforomFsDestruct(IFS *pFs);

#endif // INFOROM_FS_H
