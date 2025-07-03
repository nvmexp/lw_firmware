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
#include "elf.h"

typedef struct
{
    LwU64 address;
    LwU32 length;
    LwU32 arangeUnit;
    const LwU8 *lineUnitBuffer;
    LwU32 lineUnitSize;

} ARangeTupple;

typedef struct
{
    LwU8 *debugLineStart, *debugLineEnd;
    LwU8 *debugARangesStart, *debugARangesEnd;
    LwU8 *symtabStart, *symtabEnd;
    LwU8 *strtabStart, *strtabEnd;
    ARangeTupple *arangeTable;
    LwU32 nARangeEntries;
} libosDebugResolver;

void libosDebugResolverConstruct(libosDebugResolver *pThis, elf64_header *elf);
void libosDebugResolverDestroy(libosDebugResolver *pThis);

// @note Optimized for single lookup (no search structures are created)
LwBool libosDebugResolveSymbolToVA(libosDebugResolver *pThis, const char *symbol, LwU64 *address);
LwBool libosDebugResolveSymbolToName(
    libosDebugResolver *pThis, LwU64 symbolAddress, const char **name, LwU64 *offset);
LwBool libosDwarfResolveLine(
    libosDebugResolver *pThis, LwU64 address, const char **directory, const char **filename,
    LwU64 *outputLine, LwU64 *outputColumn, LwU64 *matchedAddress);
LwBool libosDebugGetSymbolRange(
    libosDebugResolver *pThis, LwU64 symbolAddress, LwU64 *symStart, LwU64 *symEnd);

#endif
