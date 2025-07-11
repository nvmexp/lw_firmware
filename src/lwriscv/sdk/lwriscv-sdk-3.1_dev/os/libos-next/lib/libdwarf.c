/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifdef LWRM

#    include <stddef.h>
#    include <lwport/lwport.h>
#    define printf(fmt, ...) lw_printf(LEVEL_ERROR, fmt, ##__VA_ARGS__)

#else // LWRM

#    include <memory.h>
#    include <stdio.h>
#    include <stdlib.h>

#    define portStringCompare(str1, str2, l) strcmp(str1, str2)
#    define portMemAllocNonPaged(l)          malloc(l);
#    define portMemFree(p)                   free(p);

#endif // LWRM

#include "lwtypes.h"
#include "libdwarf.h"

typedef struct
{
    const LwU8 *buffer;
    LwU64 offset;
    LwU64 size;
} DwarfStream;

typedef struct DwarfARangeTuple
{
    LwU64 address;
    LwU32 length;
    LwU32 arangeUnit;
    const LwU8 *lineUnitBuffer;
    LwU32 lineUnitSize;
} DwarfARangeTuple;

void constructDwarfARangeTuple(DwarfARangeTuple * pThis)
{
    pThis->address = 0;
    pThis->length = 0;
    pThis->arangeUnit = 0;
    pThis->lineUnitBuffer = 0;
    pThis->lineUnitSize = 0;
}

static void libosDwarfBuildTables(LibosDebugResolver *pThis);
static void dwarfBuildARangeTable(LibosDebugResolver *pThis);
static void dwarfSetARangeTableLineUnit(LibosDebugResolver *pThis, DwarfStream unit, LwU64 address);

// http://www.dwarfstd.org/doc/dwarf-2.0.0.pdf

// Debug Line information related structures
// (for branch history and call stacks)
enum
{
    DW_LNS_extended_operation = 0,
    DW_LNS_copy               = 1,
    DW_LNS_advance_pc         = 2,
    DW_LNS_advance_line       = 3,
    DW_LNS_set_file           = 4,
    DW_LNS_set_column         = 5,
    DW_LNS_negate_stmt        = 6,
    DW_LNS_set_basic_block    = 7,
    DW_LNS_const_add_pc       = 8,
    DW_LNS_fixed_advance_pc   = 9,
    DW_LNS_set_prologue_end   = 10,
    DW_LNS_set_epilogue_begin = 11,
};

enum
{
    DW_LNE_end_sequence      = 1,
    DW_LNE_set_address       = 2,
    DW_LNE_define_file       = 3,
    DW_LNE_set_discriminator = 4,
};

/**
 *
 * @brief Creates a resolver object for a given ELF.
 *        No resources or memory are retained by this call.
 *
 * @param[in] image
 *   An elf containing .debug_line and or .symtab data
 * @param[in] pThis
 *   An uninitialized resolver object.
 */
LibosStatus LibosDebugResolverConstruct(LibosDebugResolver *pThis, LibosElfImage * image)
{
   
    LibosElf64SectionHeader * debugLine = LibosElfFindSectionByName(image, ".debug_line");
    if (!debugLine || LibosOk != LibosElfMapSection(image, debugLine, &pThis->debugLineStart, &pThis->debugLineEnd))
        pThis->debugLineStart = pThis->debugLineEnd = 0;

    LibosElf64SectionHeader * debugARanges = LibosElfFindSectionByName(image, ".debug_aranges");
    if (!debugARanges || LibosOk != LibosElfMapSection(image, debugARanges, &pThis->debugARangesStart, &pThis->debugARangesEnd))
        pThis->debugARangesStart = pThis->debugARangesEnd = 0;

    LibosElf64SectionHeader * debugSymTab = LibosElfFindSectionByName(image, ".symtab");
    if (!debugSymTab || LibosOk != LibosElfMapSection(image, debugSymTab, &pThis->symtabStart, &pThis->symtabEnd))
        pThis->symtabStart = pThis->symtabEnd = 0;

    LibosElf64SectionHeader * debugStrTab = LibosElfFindSectionByName(image, ".strtab");
    if (!debugStrTab || LibosOk != LibosElfMapSection(image, debugStrTab, &pThis->strtabStart, &pThis->strtabEnd))
        pThis->strtabStart = pThis->strtabEnd = 0;

    libosDwarfBuildTables(pThis);

    return LibosOk;
}

void LibosDebugResolverDestroy(LibosDebugResolver *pThis)
{
    if (pThis->arangeTable != NULL)
    {
        portMemFree(pThis->arangeTable);
        pThis->arangeTable = NULL;
    }
}

/**
 *
 * @brief Helper method to resolve symbol name to VA
 *
 * @param[in] pThis
 *   An initialized resolver object.
 * @param[in] symbolName
 *   Name of a symbol (code or data) for lookup.
 * @param[out] address
 *   The virtual address for the given symbol.\
 */
LwBool LibosDebugResolveSymbolToVA(LibosDebugResolver *pThis, const char *symbolName, LwU64 *address)
{
    LibosElf64Symbol *i = (LibosElf64Symbol *)pThis->symtabStart;

    LwU64 count = (pThis->symtabEnd - pThis->symtabStart) / sizeof(LibosElf64Symbol);
    while (count--)
    {
        if (i->name != 0)
        {
            if (!portStringCompare(i->name + (const char *)pThis->strtabStart, symbolName, 0x1000))
            {
                *address = i->value;
                return LW_TRUE;
            }
        }
        i++;
    }
    return LW_FALSE;
}

/**
 *
 * @brief Helper method to resolve symbol VA back to name
 * @note This will match on addresses within a sized symbol (!)
 *
 * @param[in] pThis
 *   An initialized resolver object.
 * @param[in] symbolAddress
 *   An address for which we want either
 *    (1) The symbol that exactly resolves to this address
 *    (2) The symbol that contains this address
 * @param[out] offset
 *   Offsets are returned if the match is within a symbol.
 * @param[out] name
 *   The symbol name containing or matching the search address
 */
LwBool LibosDebugResolveSymbolToName(
    LibosDebugResolver *pThis, LwU64 symbolAddress, const char **name, LwU64 *offset)
{
    LibosElf64Symbol *i = (LibosElf64Symbol *)pThis->symtabStart;
    LwU64 count  = (pThis->symtabEnd - pThis->symtabStart) / sizeof(LibosElf64Symbol);
    while (count--)
    {
        if (i->name && (symbolAddress == i->value || (symbolAddress >= i->value && symbolAddress < i->value + i->size)))
        {
            *name = i->name + (const char *)pThis->strtabStart;
            *offset = symbolAddress - i->value;

            // Return now if this was a non-empty label (e.g. we're contained within it)
            if (i->size)
                return LW_TRUE;
        }
        i++;
    }
    // We hit an empty label, good enough
    if (*name)
        return LW_TRUE;
    return LW_FALSE;
}

/**
 *
 * @brief Helper method to look up the symbol for a VA and return the VA range
 *        that symbol covers.
 * @note This will match on addresses within a sized symbol (!)
 *
 * @param[in] pThis
 *   An initialized resolver object.
 * @param[in] symbolAddress
 *   An address for which we want either
 *    (1) The symbol that exactly resolves to this address
 *    (2) The symbol that contains this address
 * @param[out] symStart
 *   First address covered by the symbol..
 * @param[out] symEnd
 *   One past the last address covered by the symbol.
 */
LwBool LibosDebugGetSymbolRange(
    LibosDebugResolver *pThis, LwU64 symbolAddress, LwU64 *symStart, LwU64 *symEnd)
{
    LibosElf64Symbol *i         = (LibosElf64Symbol *)pThis->symtabStart;
    LibosElf64Symbol *symtabEnd = (LibosElf64Symbol *)pThis->symtabEnd;

    for (; i < symtabEnd; i++)
    {
        if (symbolAddress == i->value ||
            ((symbolAddress >= i->value) && (symbolAddress < i->value + i->size)))
        {
            *symStart = i->value;
            *symEnd   = i->value + i->size;
            return LW_TRUE;
        }
    }
    return LW_FALSE;
}

static LwBool libosDwarfReadRaw(DwarfStream *stream, LwU8 *buffer, LwU64 size)
{
    LwU64 newOffset = stream->offset + size;
    if (newOffset > stream->size)
        return LW_FALSE;
    for (LwU32 i = 0; i < size; i++)
        buffer[i] = stream->buffer[stream->offset+i];
    stream->offset = newOffset;
    return LW_TRUE;
}

static LwBool libosDwarfExtractString(DwarfStream *stream, const char **string)
{
    LwU64 offsetEnd = stream->offset;
    while (1)
    {
        if (offsetEnd >= stream->size)
            return LW_FALSE;
        if (!stream->buffer[offsetEnd])
            break;
        offsetEnd++;
    }

    *string        = (const char *)stream->buffer + stream->offset;
    stream->offset = offsetEnd + 1;

    return LW_TRUE;
}

#define DWARF_READ(stream, ptr) libosDwarfReadRaw(stream, ptr, sizeof(*ptr))

static LwBool dwarfReadLeb128Generic(DwarfStream *stream, LwU64 *presult, LwBool sextension)
{
    LwU8 byte;
    LwU64 bitsRead = 0, result = 0;
    while (bitsRead < 64 && DWARF_READ(stream, &byte))
    {
        // Read 7 bits
        result |= (LwU64)(byte & 0x7f) << bitsRead;
        bitsRead += 7;

        // Was this the last byte?
        if (!(byte & 0x80))
        {
            // Sign extend based on the top bit we just read
            if (sextension && bitsRead < 64 && (byte & 0x40))
                result |= 0xFFFFFFFFFFFFFFFFULL << bitsRead;

            // Return result
            *presult = result;
            return LW_TRUE;
        }
    }
    return LW_FALSE;
}

static LwBool dwarfReadSleb128(DwarfStream *stream, LwS64 *result)
{
    return dwarfReadLeb128Generic(stream, (LwU64 *)result, LW_TRUE /* sign extend */);
}

static LwBool dwarfReadUleb128(DwarfStream *stream, LwU64 *result)
{
    return dwarfReadLeb128Generic(stream, result, LW_FALSE);
}

static LwBool dwarfReadFilename(DwarfStream *names, const char **directory, const char **filename, LwU64 file)
{
    *directory = "";
    *filename  = "";
    if (!file)
        return LW_FALSE;

    // Skip the directory names stream
    DwarfStream dirnames = *names;
    const char *name;
    do
    {
        if (!libosDwarfExtractString(names, &name))
            return (LW_FALSE);
    } while (*name);

    // Walk through the file entries
    LwU64 directoryEntryId = 0;
    LwU64 i;
    for (i = 0; i < file; i++)
    {
        LwU64 mtime, size;
        if (!libosDwarfExtractString(names, filename) || !dwarfReadUleb128(names, &directoryEntryId) ||
            !dwarfReadUleb128(names, &mtime) || !dwarfReadUleb128(names, &size))
            return LW_FALSE;
    }

    // Walk the directory table up until the required point
    for (i = 0; i < directoryEntryId; i++)
        if (!libosDwarfExtractString(&dirnames, directory))
            return LW_FALSE;

    return LW_TRUE;
}

/**
 *
 * @brief DWARF-2 Virtual machine interpreter for debug data
 *
 * @param[in] pThis
 *   An initialized resolver object.
 * @param[in/out] unit
 *   A dwarf stream object initialized to the start of a unit in the
 *   .debug_lines elf section.
 * @param[in] pc
 *   Virtual address of the code to resolve.
 * @param[out] directory
 *   The source file directory for this line of code. Requires a .debug_line section.
 * @param[out] filename
 *   The source file for this line of code. Requires a .debug_line section.
 * @param[out] outputLine
 *   The source line for address.  Requires a .debug_line section.
 * @param[out] outputColumn
 *   The source column for this address. Requires additional debug info -g3.
 * @param[in] matchedAddress
 *   Returns the virtual address to the start of the matched line/col.
  * @param[in] bBuildTable
 *   Set to true when building the aranges index table.
 */
static LwBool dwarfParseUnitLines(
    LibosDebugResolver *pThis, DwarfStream unit, LwU64 pc, const char **directory, const char **filename,
    LwU64 *outputLine, LwU64 *outputColumn, LwU64 *matchedAddress, LwBool bBuildTable)
{
    LwU16 version;
    LwU32 prologueLength;

    DwarfStream saveUnit = unit;

    if (!DWARF_READ(&unit, &version) || version > 2 || !DWARF_READ(&unit, &prologueLength))
    {
        return LW_FALSE;
    }

    LwU64 headerOffset = unit.offset;
    LwU8 minimumInstructionLength, defaultIsStmt, lineRange, opcodeBase;
    LwS8 line_base;
    if (!DWARF_READ(&unit, &minimumInstructionLength) || !DWARF_READ(&unit, &defaultIsStmt) ||
        !DWARF_READ(&unit, &line_base) || !DWARF_READ(&unit, &lineRange) || !DWARF_READ(&unit, &opcodeBase))
    {
        return LW_FALSE;
    }

    // Skip over the opcode lengths
    LwU64 i;
    for (i = 1; i < opcodeBase; i++)
    {
        LwU64 dummy;
        if (!dwarfReadUleb128(&unit, &dummy))
        {
            return LW_FALSE;
        }
    }

    // Names section starts here inside after the prologue
    DwarfStream names = unit;

    // Skip prologue
    unit.offset = headerOffset;
    if (unit.size - unit.offset < prologueLength)
    {
        //printf("Bailing 3...\n");
        return LW_FALSE;
    }
    unit.offset += prologueLength;

    // Initial state of virtuall machine
    LwU64 previousAddress = 0, previousLine = 0, previousColumn = 0, previousFile = 0;
    LwU64 address = 0, file = 1, line = 1, column = 0;
    LwU8 isStmt = defaultIsStmt;
    //LwBool basicBlock = LW_FALSE
    LwBool postEmitResetState = LW_FALSE;
    //LwBool prologueEnd = LW_FALSE;
    //LwBool epilogueBegin = LW_FALSE;
    LwBool postEmitResetStateIsStmt = LW_FALSE;
    //LwBool endSequence = LW_FALSE;

    // Run the line number information program for this unit
    LwU8 opcode;
    while (LW_TRUE)
    {
        //LwU64 offset = unit.offset;
        if (!DWARF_READ(&unit, &opcode))
            break;
        LwBool emit_row = LW_FALSE;
        //LwBool reset_basic_block = LW_FALSE;

        // 6.2.5.1 Special Opcodes
        if (opcode >= opcodeBase)
        {
            LwU8 normalizedOpcode = opcode - opcodeBase;
            address += (normalizedOpcode / lineRange) * minimumInstructionLength;
            line += line_base + normalizedOpcode % lineRange;
            //reset_basic_block = LW_TRUE;
            emit_row = LW_TRUE;
            //printf("  [0x%08llx]  Special opcode\n", offset);
        }
        // 6.2.5.3 Extended Opcodes
        else if (opcode == DW_LNS_extended_operation)
        {
            LwU64 extBaseOffset = unit.offset, extraSize;
            DwarfStream extra;

            if (!dwarfReadUleb128(&unit, &extraSize))
                return LW_FALSE;

            if (unit.size - unit.offset < extraSize)
                return LW_FALSE;
            extra.buffer = unit.buffer + extBaseOffset;
            extra.offset = unit.offset - extBaseOffset;
            extra.size   = extraSize + unit.offset - extBaseOffset;
            unit.offset += extraSize;

            if (!DWARF_READ(&extra, &opcode))
                return LW_FALSE;

            switch (opcode)
            {
            case DW_LNE_end_sequence:
                emit_row                 = LW_TRUE;
                postEmitResetStateIsStmt = isStmt;
                postEmitResetState       = LW_TRUE;
                //printf("  [0x%08llx]  Extended opcode 1: End of Sequence\n", offset);
                break;

            case DW_LNE_set_address:
                switch (extra.size - extra.offset)
                {
                case 8:
                    if (!DWARF_READ(&extra, &address))
                        return LW_FALSE;
                    break;

                case 4: {
                    LwU32 address32;
                    if (!DWARF_READ(&extra, &address32))
                        return LW_FALSE;
                    address = address32;
                    break;
                }

                default:
                    //printf("unexpected address length: %zu", extra.size - extra.offset);
                    return LW_FALSE;
                }
                //printf("  [0x%08llx]  Extended opcode 2: set Address to 0x%llx\n", offset, address);
                break;

            case DW_LNE_define_file: {
                const char *fname = "";
                libosDwarfExtractString(&unit, &fname);
                //printf("  [0x%08x]  Define file: %s\n", fname, offset);
                LwU64 dir, time, size;
                dwarfReadUleb128(&unit, &dir);
                dwarfReadUleb128(&unit, &time);
                dwarfReadUleb128(&unit, &size);
            }
            break;

            case DW_LNE_set_discriminator: // For profilers, how many code paths pass through this insn
            {
                LwU64 discriminator;
                dwarfReadUleb128(&extra, &discriminator);
                //printf("  [0x%08llx]  Extended opcode 4: set Discriminator to   %lld\n", offset, discriminator);
                break;
            }

            default:
                //printf("  [0x%08llx]  unknown extended opcode: %d\n", offset, opcode);
                return LW_FALSE;
            }
        }
        else
        {
            /* "Standard" opcodes. */
            switch (opcode)
            {
            case DW_LNS_copy:
                emit_row = LW_TRUE;
                //reset_basic_block = LW_TRUE;
                //printf("  [0x%08llx]  Copy\n", offset);
                break;

            case DW_LNS_advance_pc: {
                LwU64 delta;
                if (!dwarfReadUleb128(&unit, &delta))
                    return LW_FALSE;
                address += delta * minimumInstructionLength;
                //printf("  [0x%08llx]  Advance PC by %lld to 0x%llx\n", offset, delta, address);
                break;
            }

            case DW_LNS_fixed_advance_pc: {
                LwU16 delta = 0;
                DWARF_READ(&unit, &delta);
                address += delta * minimumInstructionLength;
                //printf("  [0x%08llx]  Advance PC by fixed size amount %d to 0x%llx\n",offset , delta, address);
                break;
            }

            case DW_LNS_advance_line: {
                LwS64 delta;
                if (!dwarfReadSleb128(&unit, &delta))
                    return LW_FALSE;
                line += delta;
                //printf("  [0x%08llx]  Advance Line by %lld to %lld\n", offset, delta, line);
                break;
            }

            case DW_LNS_set_file:
                if (!dwarfReadUleb128(&unit, &file))
                    return LW_FALSE;
                //printf("  [0x%08llx]  Set File Name to entry %lld in the File Name Table\n", offset, file);
                break;

            case DW_LNS_set_column:
                if (!dwarfReadUleb128(&unit, &column))
                    return LW_FALSE;
                //printf("  [0x%08llx]  Set column to %lld\n", offset, column);
                break;

            case DW_LNS_negate_stmt:
                isStmt = !isStmt;
                //printf("  [0x%08llx]  Set isStmt to %d\n", offset, isStmt);
                break;

            case DW_LNS_set_basic_block:
                //basicBlock = LW_TRUE;
                //printf("  [0x%08llx]  Set basic block\n", offset);
                break;

            case DW_LNS_const_add_pc: {
                LwU64 delta = ((255 - opcodeBase) / lineRange) * minimumInstructionLength;
                address += delta;
                //printf("  [0x%08llx]  Add pc by %lld to %08llx\n", offset, delta, address);
                break;
            }

            case DW_LNS_set_prologue_end:
                //printf("  [0x%08llx]  Set prologue end\n", offset);
                //prologueEnd = LW_TRUE;
                break;

            case DW_LNS_set_epilogue_begin:
                //printf("  [0x%08llx]  Set epilogie begin\n", offset);
                //epilogueBegin = LW_TRUE;
                break;

            default:
                //printf("  [0x%08llx]  unknown standard opcode: %d\n", offset, opcode);
                return LW_FALSE;
            }
        }

        if (emit_row)
        {
            if (bBuildTable)
            {
                dwarfSetARangeTableLineUnit(pThis, saveUnit, address);
                return LW_FALSE;
            }

            if ((previousAddress && (pc >= previousAddress && pc < address)))
            {
                dwarfReadFilename(&names, directory, filename, previousFile);
                *outputLine     = previousLine;
                *outputColumn   = previousColumn;
                *matchedAddress = pc;
                return LW_TRUE;
            }

            previousAddress = address;
            previousFile    = file;
            previousLine    = line;
            previousColumn  = column;
        }

        if (postEmitResetState)
        {
            address = 0;
            file    = 1;
            line    = 1;
            column  = 0;
            isStmt  = postEmitResetStateIsStmt;
            //basicBlock = LW_FALSE;
            postEmitResetState = LW_FALSE;
            //endSequence = LW_FALSE;
            //prologueEnd = LW_FALSE;
            //epilogueBegin = LW_FALSE;
        }

        //if (reset_basic_block)
        //    basicBlock = LW_FALSE;
    }
    return LW_FALSE;
}

/**
 *
 * @brief Resolve an address to source file and line location for DWARF-2
 *
 * @param[in] pThis
 *   An initialized resolver object.
 * @param[in] address
 *   Virtual address of the code to resolve.
 * @param[out] filename
 *   The source file for this line of code. Requires a .debug_line section.
 * @param[out] outputLine
 *   The source line for address.  Requires a .debug_line section.
 * @param[out] outputColumn
 *   The source column for this address. Requires additional debug info -g3.
 * @param[in] matchedAddress
 *   Returns the virtual address to the start of the matched line/col.
 */
LwBool LibosDwarfResolveLine(
    LibosDebugResolver *pThis, LwU64 address, const char **directory, const char **filename,
    LwU64 *outputLine, LwU64 *outputColumn, LwU64 *matchedAddress)
{
    DwarfARangeTuple *pFoundARange = NULL;
    DwarfStream unit;
    LwU32 i;

    // Find entry in aranges table
    for (i = 0; i < pThis->nARangeEntries; i++)
    {
        if ((address >= pThis->arangeTable[i].address) &&
            (address < pThis->arangeTable[i].address + pThis->arangeTable[i].length))
        {
            pFoundARange = &pThis->arangeTable[i];

            if (pFoundARange->lineUnitBuffer != NULL)
            {
                // Found cached line into in ARange table.
                unit.buffer = pFoundARange->lineUnitBuffer;
                unit.offset = 0;
                unit.size   = pFoundARange->lineUnitSize;

                if (dwarfParseUnitLines(
                        pThis, unit, address, directory, filename, outputLine, outputColumn, matchedAddress,
                        LW_FALSE))
                {
                    return LW_TRUE;
                }
            }

            break;
        }
    }

    return LW_FALSE;
}

/**
 *
 * @brief Create a table indexing the units in .debug_line elf section
 *
 *
 * @param[in/out] pThis
 *   An initialized resolver object.
 */
static void libosDwarfBuildTables(LibosDebugResolver *pThis)
{
    pThis->arangeTable    = NULL;
    pThis->nARangeEntries = 0;

    // Run through the .debug_aranges elf section to get a count of consolidated ranges.
    dwarfBuildARangeTable(pThis);

    LwU32 tableSize = (pThis->nARangeEntries + 1) * sizeof(DwarfARangeTuple);

    // Allocate the table.
    pThis->arangeTable = portMemAllocNonPaged(tableSize);
    for (LwU32 i = 0; i < pThis->nARangeEntries + 1; i++)
        constructDwarfARangeTuple(&pThis->arangeTable[i]);

    // Run through the .debug_aranges elf section again to populate the table.
    dwarfBuildARangeTable(pThis);

    DwarfStream debugLines = {pThis->debugLineStart, 0, pThis->debugLineEnd - pThis->debugLineStart};
    DwarfStream unit;
    LwU32 lwrrentUnit = 1;
    LwU32 unitSize;

    // Run through the .debug_line elf section to match units to the arange table.
    for (lwrrentUnit = 1;; lwrrentUnit++)
    {
        if (!DWARF_READ(&debugLines, &unitSize) || unitSize >= 0xfffffff0)
        {
            break;
        }

        if (debugLines.size - debugLines.offset < unitSize)
        {
            break;
        }

        unit.buffer = debugLines.buffer + debugLines.offset;
        unit.offset = 0;
        unit.size   = unitSize;

        dwarfParseUnitLines(pThis, unit, 0, NULL, NULL, NULL, NULL, NULL, LW_TRUE);
        debugLines.offset += unitSize;
    }
}

/**
 *
 * @brief Walk the .debug_aranges elf section, consolidate adjacent ranges,
 *        and create a table with an entry for each range.
 *
 * @param[in/out] pThis
 *   An initialized resolver object.
 */
static void dwarfBuildARangeTable(LibosDebugResolver *pThis)
{
    DwarfStream debugARanges = {
        pThis->debugARangesStart, 0, pThis->debugARangesEnd - pThis->debugARangesStart};
    DwarfStream unit;
    LwU32 nUnit          = 0;
    LwU32 nARangeEntries = 0;

    for (nUnit = 1;; nUnit++)
    {
        LwU32 unit_size       = 0xffffffff;
        LwU32 debugInfoOffset = 0xffffffff;
        LwU16 version         = 0xffff;
        LwU8 addressSize      = 0xff;
        LwU8 selectorSize     = 0xff;

        if (!DWARF_READ(&debugARanges, &unit_size) || unit_size >= 0xfffffff0)
        {
            break;
        }

        if (debugARanges.size - debugARanges.offset < unit_size)
        {
            break;
        }

        unit.buffer = debugARanges.buffer + debugARanges.offset - sizeof unit_size;
        unit.offset = sizeof unit_size;
        unit.size   = unit_size + sizeof unit_size;
        debugARanges.offset += unit_size;

        if (!DWARF_READ(&unit, &version) || version != 2 || !DWARF_READ(&unit, &debugInfoOffset) ||
            !DWARF_READ(&unit, &addressSize) || !DWARF_READ(&unit, &selectorSize) || addressSize != 8 ||
            selectorSize != 0)
        {
            break;
        }

        // Pad to natural alignment
        unit.offset = (unit.offset + 15) & ~15;

        LwU32 nUnitTupples;
        LwU64 combAddress = 0;
        LwU64 combLength  = 0;

        for (nUnitTupples = 0;; nUnitTupples++)
        {
            LwU64 address;
            LwU64 length;

            if (!DWARF_READ(&unit, &address) || !DWARF_READ(&unit, &length))
            {
                address = 0;
                length  = 0;
            }

            if (address == combAddress + combLength)
            {
                combLength += length;
            }
            else
            {
                if (combAddress != 0 && combLength != 0)
                {
                    if (pThis->arangeTable != NULL && nARangeEntries < pThis->nARangeEntries)
                    {
                        // Table has been allocated -- fill it in.
                        DwarfARangeTuple *pEntry = &pThis->arangeTable[nARangeEntries];
                        pEntry->address      = combAddress;
                        pEntry->length       = combLength;
                        pEntry->arangeUnit   = nUnit;
                    }
                    nARangeEntries++;
                }

                combAddress = address;
                combLength  = length;
            }

            if (address == 0 && length == 0)
                break;
        }
    }

    pThis->nARangeEntries = nARangeEntries;
}

/**
 *
 * @brief Set a .debug_line reference in the table of consolidated aranges.
 *
 * @param[in] pThis
 *   An initialized resolver object.
 * @param[in/out] unit
 *   A dwarf stream object initialized to the start of a unit in the
 *   .debug_lines elf section.
 * @param[in] address
 *   Any virtual address contained in teh above .debug_lines unit.
 */
static void dwarfSetARangeTableLineUnit(LibosDebugResolver *pThis, DwarfStream unit, LwU64 address)
{
    LwU32 foundUnit = 0;
    LwU32 i;

    // Find entry in aranges table
    for (i = 0; i < pThis->nARangeEntries; i++)
    {
        if ((address >= pThis->arangeTable[i].address) &&
            (address < pThis->arangeTable[i].address + pThis->arangeTable[i].length))
        {
            foundUnit = pThis->arangeTable[i].arangeUnit;
            break;
        }
    }

    if (foundUnit == 0)
    {
        return;
    }

    //
    // Walk backwards to first table entry with the same areange unit.
    // Just in case -- I think i should always point to the first unit entry.
    //
    while ((i > 0) && (foundUnit == pThis->arangeTable[i - 1].arangeUnit))
    {
        i--;
    }

    // Walk forwards setting the line unit info for all entries with the same arange unit.
    for (; (foundUnit == pThis->arangeTable[i].arangeUnit); i++)
    {
        pThis->arangeTable[i].lineUnitBuffer = unit.buffer;
        pThis->arangeTable[i].lineUnitSize   = unit.size;
    }
}
