/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef __LINES_H__
#define __LINES_H__
#include "libelf.h"
#ifdef __cplusplus
extern "C" {
#endif

#include "libos_status.h"

typedef struct
{
    LwU8 *debugLineStart, *debugLineEnd;
    LwU8 *debugARangesStart, *debugARangesEnd;
    LwU8 *symtabStart, *symtabEnd;
    LwU8 *strtabStart, *strtabEnd;
    struct DwarfARangeTuple *arangeTable;
    LwU32 nARangeEntries;
} LibosDebugResolver;

LibosStatus LibosDebugResolverConstruct(LibosDebugResolver *pThis, LibosElfImage * image);
void LibosDebugResolverDestroy(LibosDebugResolver *pThis);

// @note Optimized for single lookup (no search structures are created)
LwBool LibosDebugResolveSymbolToVA(LibosDebugResolver *pThis, const char *symbol, LwU64 *address);
LwBool LibosDebugResolveSymbolToName(LibosDebugResolver *pThis, LwU64 symbolAddress, const char **name, LwU64 *offset);
LwBool LibosDwarfResolveLine(LibosDebugResolver *pThis, LwU64 address, const char **directory, const char **filename,
    LwU64 *outputLine, LwU64 *outputColumn, LwU64 *matchedAddress);
LwBool LibosDebugGetSymbolRange(LibosDebugResolver *pThis, LwU64 symbolAddress, LwU64 *symStart, LwU64 *symEnd);

#ifdef __cplusplus
}
#endif
#endif
