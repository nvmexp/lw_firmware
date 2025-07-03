/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2012,2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file matshelp.h   Shared MATS types and routines.

#ifndef __MATS_HELP_H_INCLUDED
#define __MATS_HELP_H_INCLUDED

enum OperationType
{
    OPTYPE_READ  = 0,
    OPTYPE_WRITE,
    NUM_OPTYPE
};

enum OperationSize
{
    OPSIZE_8 = 0,
    OPSIZE_16,
    OPSIZE_32,
    OPSIZE_64,
    NUM_OPSIZE
};

enum OperationMode
{
    OPMODE_IMMEDIATE = 0,      // numeric value indexes into PatternTable
    OPMODE_ADDRESS,            // write address as data
    OPMODE_ADDRESS_ILW,        // write ilwerse of address as data
    OPMODE_SKIP,               // do not perform any operation
    NUM_OPMODE
};

enum MemoryFormat
{
    MEMFMT_LINEAR = 0,         // operate on a linear region of memory
    MEMFMT_LINEAR_BOUNDED,     // operate on a linear subregion of memory
    MEMFMT_BOX,                // operate on rectangular regions of memory
    MEMFMT_BOX_BOUNDED,        // operate on rectangular subregions of memory
    NUM_MEMFMT
};

enum MemoryDirection
{
    MEMDIR_UP = 0,             // ascending
    MEMDIR_DOWN,               // descending
    NUM_MEMDIR
};

// don't pull in any of the host-specific classes when compiling for LWCA
#ifndef __LWDACC__

#ifndef MATS_STANDALONE
#include <string>
#include <vector>
#include "core/include/types.h"
#include "core/include/jscript.h"
#include "random.h"
#endif

struct MemoryRange
{
    UINT64 StartOffset;
    UINT64 EndOffset;
    UINT64 GetSize() const { return EndOffset - StartOffset; }
};
typedef std::vector<MemoryRange> MemoryRanges;

struct PointerParams
{
    UINT64 Offset;             // framebuffer offset
    volatile UINT08 *Virtual;  // virtual memory address
    UINT64 Xpos;               // box x position
    UINT64 Ypos;               // box y position
};

class MatsTest;
struct MemoryCommand;
typedef RC (MatsTest::*MemoryRoutinePtr)(const MemoryCommand &);

struct MemoryCommand
{
    OperationType op_type;     // type of operation (read/write)
    OperationMode op_mode;     // mode of operation (immediate/addr/ilw/skip)
    OperationSize op_size;     // size of operation (8/16/32/64)
    UINT64 value;              // value to write (if immediate)
    UINT32 runflag;
    MemoryRoutinePtr run;
    PointerParams *ptr;        // Used in Run* to point to the read/write offset
    bool increment;            // increment pointer after operation
};
typedef std::vector<MemoryCommand> MemoryCommands;

struct CommandSequence
{
    MemoryCommands operations; // sequence of reads and writes
    MemoryFormat mem_fmt;      // format of memory (linear/box)
    UINT32 memsize;            // size of all contained memory operations
    UINT32 runflags;           // bits to indicate running commands
    std::string pattern;       // original operation string
    bool ascending;            // low to high memory, or high to low memory
    bool bounded;              // limit test by UpperBound
};
typedef std::vector<CommandSequence> CommandSequences;

class MatsHelper
{
public: // methods
    MatsHelper();
    ~MatsHelper();

    RC ParseTable(const JsArray &args);
    RC ParseSequence(const JsArray &args, CommandSequences *ops);
    RC ParseSequenceList(const JsArray &args, CommandSequences *ops);
    RC InitBoxes(const MemoryRanges &TestRanges, Random *pRandom,
                 UINT32 pitch, FLOAT64 coverage);
    void Reset();

    UINT64 GetPattern(UINT32 index) const { return m_Patterns[index]; }
    UINT64 GetPatternCount()        const { return m_Patterns.size(); }
    UINT32 GetBox(int index)        const { return m_BoxIdx[index]; }
    UINT64 GetBoxTlc(int index)     const { return m_BoxTlc[index]; }
    UINT32 GetBoxCount()            const { return m_NumBoxes; }
    void SetMemoryAlignment(UINT32 align) { m_MemoryAlignment = align; }

private: // methods
    RC ParseTableIndex(const char **pattern_ptr, UINT64 *value);
    const char *NextSymbol(const char *str);
    std::string StringTrim(const std::string &str);
    bool IsHexChar(char c, unsigned int *val);
    bool StrToUINT64(const char *str, UINT64 *val);

public: // fields
    enum
    {
        // Modifiable parameters for box-based test.  Must be powers of 2
        BoxWidth = 16,
        BoxHeight = 16,
        VertBoxPerMB = 16,
    };

private: // fields
    UINT32 m_NumBoxes;
    std::vector<UINT64> m_Patterns;
    std::vector<UINT32> m_BoxIdx;
    std::vector<UINT64> m_BoxTlc;
    UINT32 m_MemoryAlignment;
};

#endif // __LWDACC__
#endif // __MATS_HELP_H_INCLUDED

