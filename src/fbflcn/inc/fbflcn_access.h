/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef FBFLCN_ACCESS_H
#define FBFLCN_ACCESS_H

/*!
 * @file   fbflcn_access.h
 * @brief  Implements access functions into a packed data structures where
 *         content fields might not be properly aligned to their size.
 *
 */

#include <lwmisc.h>
#include <lwtypes.h>
#include "lwuproc.h"
#include "config/fbfalcon-config.h"

//
// quick access macros to extrat content from packed data that can be referenced
// from a structure definition
//
#define TABLE_VAL(field) getData((LwU8*)(&field), (LwU8)sizeof(field))
#define TABLE_FORCE_VAL(field,data) forceData((LwU8*)(&field), (LwU8)sizeof(field), (data))
#define EXTRACT_TABLE(pSource,destination)  extractData ((LwU8*)(pSource),sizeof(destination),(LwU8*)(&destination));


//
// content access functions for packed and unaligned data
//
LwU32 getData(LwU8 *adr, LwU8 size)
    GCC_ATTRIB_SECTION("resident", "getData");

void forceData(LwU8 *adr, LwU8 size, LwU32 data)
    GCC_ATTRIB_SECTION("resident", "forceData");

void extractData(LwU8 *adr,LwU16 size, LwU8 *dest)
    GCC_ATTRIB_SECTION("resident", "extractData");



#endif //  FBFLCN_ACCESS_H
