/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef INTERFACE_H
#define INTERFACE_H

#include <lwmisc.h>
#include <lwtypes.h>
#include "fbflcn_defines.h"
#include "fbflcn_table.h"

#define FB_INIT_IFACE_APPVERSION_VALUE 0x0001


typedef struct
{
    LwU8    version;
    LwU8    headerSize;
    LwU8    entrySize;
    LwU8    entryCount;
} APPLICATION_INTERFACE_HEADER;


typedef struct
{
    LwU32   id;

// application ID define from  //sw/main/bios/core90/core/pmu/interface.h
#define APPLICATION_INTERFACE_ENTRY_ID_FB_INIT  0x0000000d

    LwU32   ptr;
} APPLICATION_INTERFACE_ENTRY;


// inti interface define from  //sw/main/bios/core90/core/pmu/interface.h
typedef struct
{
    LwU16   version;
    LwU16   structSize;
    LwU16   appVersion;
    LwU16   appFeatures;
} FBFLCN_INIT_INTERFACE;


#endif
