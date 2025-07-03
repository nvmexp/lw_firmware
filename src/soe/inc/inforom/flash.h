/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef IFS_FLASH_H
#define IFS_FLASH_H

#include "inforom_fs.h"

/* Debug structure to record history of flash accesses from inforom */
#define FLASH_ACCESS_BUF_SIZE   16
#define FLASH_ACCESS_ARRAY_SIZE 100
typedef enum
{
    FLASHACCESSTYPE_READ,
    FLASHACCESSTYPE_WRITE,
    FLASHACCESSTYPE_ERASE,
} FLASHACCESSTYPE;

struct INFOROM_FLASH_ACCESS
{
    char                    name[IFS_FILE_NAME_SIZE];
    FLASHACCESSTYPE         type;
    LwU32                   time_start;
    LwU32                   time_end;
    FS_STATUS               status;
    LwU32                   offset;
    LwU32                   size;
    LwU8                    *buf;
};

typedef struct INFOROM_FLASH_ACCESS INFOROM_FLASH_ACCESS;


// Debug tracing functions
void ifsFlashTracingStart(IFS *pFs,
                          FLASHACCESSTYPE type,
                          const char pName[IFS_FILE_NAME_SIZE],
                          LwU32 offset,
                          void *buf,
                          LwU32 size,
                          INFOROM_FLASH_ACCESS **ppFlashAccess);

void ifsFlashTracingEnd(IFS *pFs,
                        LwU8 *buf,
                        LwU32 size,
                        FS_STATUS flashStatus,
                        INFOROM_FLASH_ACCESS *pFlashAccess);

void ifsFlashAccessDestruct(IFS *pFs);

#endif // IFS_FLASH_H
