/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file matshelp.cpp   Implements MATS helper routines.

#ifndef MATS_STANDALONE
#include "matshelp.h"
#include "core/include/types.h"
#include "core/include/script.h"
#include "core/include/utility.h"
#include "gpu/utility/gpuutils.h"
#include <cstdio>
#include <ctype.h>
#endif

using namespace std;

MatsHelper::MatsHelper()
{
    m_NumBoxes  = 0;
    m_MemoryAlignment = static_cast<UINT32>(1_MB); // must be a power of 2
}

MatsHelper::~MatsHelper()
{
    Reset();
}

RC MatsHelper::ParseTable(const JsArray &args)
{
    RC rc;
    JavaScriptPtr pJs;

    string value_str;
    UINT64 value_int;
    int index = 0;

    m_Patterns.clear();
    for (JsArray::const_iterator i = args.begin();
         i != args.end(); ++i, ++index)
    {
        if (!JSVAL_IS_STRING(*i))
        {
            // perform normal colwersion for non-string types
            CHECK_RC ( pJs->FromJsval(*i, &value_int) );
            m_Patterns.push_back(value_int);
            continue;
        }

        // if the table entry is a string, check from random and 64 bit types
        CHECK_RC (pJs->FromJsval(*i, &value_str) );
        value_str = StringTrim(value_str);

        if (StrToUINT64(value_str.c_str(), &value_int))
        {
            // parsed a valid integer
            m_Patterns.push_back(value_int);
        }
        else
        {
            // Could also try to interpret the string as a named pattern and
            // search m_PatternClass...
            Printf(Tee::PriHigh, "Unable to parse table index %d: %s\n",
                   index, value_str.c_str());
            return RC::BAD_PARAMETER;
        }
    }

    return OK;
}

RC MatsHelper::ParseSequence(const JsArray &args, CommandSequences *seq)
{
    MASSERT(seq != NULL);

    RC rc;
    JavaScriptPtr pJs;

    // verify we have a command string, direction, memory format, and size
    if (4 != args.size())
        return RC::BAD_PARAMETER;

    string pattern;
    UINT32 memoryDir, memoryFmt, memorySize;

    CHECK_RC( pJs->FromJsval(args[0], &pattern) );
    CHECK_RC( pJs->FromJsval(args[1], &memoryDir) );
    CHECK_RC( pJs->FromJsval(args[2], &memoryFmt) );
    CHECK_RC( pJs->FromJsval(args[3], &memorySize) );

    CommandSequence lwrrSeq;
    lwrrSeq.pattern = pattern;
    lwrrSeq.runflags = 0;

    // verify that the specified memory direction and size are valid
    switch (memoryDir)
    {
        case MEMDIR_DOWN:
            lwrrSeq.ascending = false;
            break;
        case MEMDIR_UP:
            lwrrSeq.ascending = true;
            break;
        default:
            Printf(Tee::PriHigh, "Invalid memory direction (%d) specified.\n",
                   memoryDir);
            return RC::BAD_PARAMETER;
    }

    // determine the memory format (linear/box), either can be normal or bounded
    switch (memoryFmt)
    {
        case MEMFMT_LINEAR_BOUNDED:
            lwrrSeq.bounded = true;
            lwrrSeq.mem_fmt = MEMFMT_LINEAR;
            break;
        case MEMFMT_LINEAR:
            lwrrSeq.bounded = false;
            lwrrSeq.mem_fmt = MEMFMT_LINEAR;
            break;
        case MEMFMT_BOX_BOUNDED:
            lwrrSeq.bounded = true;
            lwrrSeq.mem_fmt = MEMFMT_BOX;
            break;
        case MEMFMT_BOX:
            lwrrSeq.bounded = false;
            lwrrSeq.mem_fmt = MEMFMT_BOX;
            break;
        default:
            Printf(Tee::PriHigh, "Invalid memory format (%d) specified.\n",
                   memoryFmt);
            return RC::BAD_PARAMETER;
    }

    // only support 8/16/32/64 bit widths, store internally in terms of bytes
    OperationSize opSize;
    switch (memorySize)
    {
        case 8:  opSize = OPSIZE_8;  break;
        case 16: opSize = OPSIZE_16; break;
        case 32: opSize = OPSIZE_32; break;
        case 64: opSize = OPSIZE_64; break;
        default:
            Printf(Tee::PriHigh, "Invalid size (%d) specified.\n", memorySize);
            return RC::BAD_PARAMETER;
    }
    lwrrSeq.memsize = memorySize >> 3;

    const char *pattern_ptr = NextSymbol(pattern.c_str());
    while (*pattern_ptr != '\0')
    {
        MemoryCommand command = {};
        command.op_size = opSize;

        // determine if we are performing a memory read or write
        switch (toupper(*pattern_ptr))
        {
            case 'W':
                command.op_type = OPTYPE_WRITE;
                break;
            case 'R':
                command.op_type = OPTYPE_READ;
                break;
            default:
                Printf(Tee::PriHigh,
                       "Invalid operation type (%d) specified.\n",
                       *pattern_ptr);
                return RC::BAD_PARAMETER;
        }
        pattern_ptr = NextSymbol(++pattern_ptr);

        // enable the runflag bit for this command
        command.runflag = 1 << command.op_type;
        lwrrSeq.runflags |= command.runflag;

        // next character indicates mode, A/I for address, digit(s) for index
        char mode = toupper(*pattern_ptr);
        if (isdigit(mode))
        {
            UINT64 value;
            CHECK_RC( ParseTableIndex(&pattern_ptr, &value) );
            command.value = value;
            command.op_mode = OPMODE_IMMEDIATE;
            pattern_ptr = NextSymbol(pattern_ptr);
        }
        else
        {
            switch (mode)
            {
                case 'A': // address
                    command.op_mode = OPMODE_ADDRESS;
                    break;
                case 'I': // ilwerse address
                    command.op_mode = OPMODE_ADDRESS_ILW;
                    break;
                case 'S': // skip and increment
                    command.op_mode = OPMODE_SKIP;
                    break;
                default:
                    Printf(Tee::PriHigh, "Invalid mode (%d) specified.\n",
                           mode);
                    return RC::BAD_PARAMETER;
            }
            pattern_ptr = NextSymbol(++pattern_ptr);
        }

        // if this command is immediately followed by +, increment memory
        command.increment = false;
        if (*pattern_ptr == '+')
        {
            command.increment = true;
            pattern_ptr = NextSymbol(++pattern_ptr);
        }

        lwrrSeq.operations.push_back(command);

        // optionally split a single command string into one or more sequences
        if (*pattern_ptr == '/' && lwrrSeq.operations.size())
        {
            seq->push_back(lwrrSeq);
            pattern_ptr = NextSymbol(++pattern_ptr);

            lwrrSeq.operations.clear();
            lwrrSeq.runflags = 0;
        }
    }

    if (lwrrSeq.operations.size())
        seq->push_back(lwrrSeq);

    return OK;
}

RC MatsHelper::ParseSequenceList(const JsArray &args, CommandSequences *ops)
{
    RC rc;
    JavaScriptPtr pJs;

    MASSERT(ops != NULL);
    ops->clear();

    for (JsArray::const_iterator cmd = args.begin(); cmd != args.end(); ++cmd)
    {
        JsArray argumentList;
        CHECK_RC( pJs->FromJsval(*cmd, &argumentList) );
        CHECK_RC( ParseSequence(argumentList, ops) );
    }

    return OK;
}

// Divide the framebuffer into boxes for the DoBoxMats() test, such
// that no box crosses a megabyte boundary.
//
// This routine fills m_BoxHeight[] and m_BoxTlc[] with the height and
// top-left-corner address (relative to the start of the framebuffer)
// of each box.
//
// This routine also fills m_BoxIdx[] with a shuffled list of indices
// of boxes that will be tested.  Each m_BoxIdx[] entry is an index
// into m_BoxHeight[] & m_BoxTlc[].  The m_BoxIdx[] array is typically
// smaller than m_BoxHeight[] & m_BoxTlc for two reasons:
//
// (1) There may be some boxes in m_BoxHeight[] & m_BoxTlc[] that we
//     must not overwrite, because they cover part of the framebuffer
//     that was already allocated before Mats started.  (TestRanges
//     indicates the parts of the framebuffer we can test.)  m_BoxIdx[]
//     will not contain the index of any "forbidden" box.
// (2) We may opt for partial coverage of the framebuffer, in which
//     case m_BoxIdx[] will only contain a fraction of the testable boxes.
RC MatsHelper::InitBoxes
(
    const MemoryRanges &TestRanges,
    Random *pRandom,
    UINT32 pitch,
    FLOAT64 coverage
)
{
    UINT32 ResX = pitch / 4;

    MASSERT((m_MemoryAlignment % BoxWidth) == 0); // Because box must end on alignment bound
    MASSERT((ResX % BoxWidth) == 0);   // BoxesPerRow must be integer

    m_BoxIdx.clear();
    m_BoxTlc.clear();

    UINT32 NumBoxes = 0;
    const UINT32 NumCol = ResX / BoxWidth;

    for (MemoryRanges::const_iterator testRange = TestRanges.begin();
         testRange != TestRanges.end(); ++testRange)
    {
        UINT64 StartOffset = testRange->StartOffset;
        UINT64 EndOffset = testRange->EndOffset;

        // round to alignment
        const UINT64 roundMask = ~static_cast<UINT64>(m_MemoryAlignment - 1);
        StartOffset = (StartOffset + m_MemoryAlignment - 1) & roundMask;
        EndOffset = EndOffset & roundMask;

        UINT32 NumRow = UINT32((EndOffset - StartOffset) / (pitch * BoxHeight));

        Printf(Tee::PriLow, "NumRow = %u, NumCol = %u.\n", NumRow, NumCol);
        for (UINT64 Row = 0; Row < NumRow; Row++)
        {
            const UINT64 RowOffset = StartOffset
                + static_cast<UINT64>(Row) * BoxHeight * pitch;
            for (UINT32 Col = 0; Col < NumCol; Col++)
            {
                UINT64 Offset = RowOffset + Col * BoxWidth * sizeof(UINT32);
                MASSERT(EndOffset >= (Offset + ((ResX * (BoxHeight - 1))
                                             + BoxWidth) * sizeof(UINT32)));

                m_BoxIdx.push_back(NumBoxes);
                NumBoxes++;
                m_BoxTlc.push_back(Offset);
            }
        }
    }

    Printf(Tee::PriLow, "NumBox tested = %u\n", NumBoxes);
    m_NumBoxes = NumBoxes;

    // shuffle the array
    pRandom->Shuffle(m_NumBoxes, &m_BoxIdx[0]);

    // callwlate the percent of boxes to cover
    if (coverage < 100)
    {
        m_NumBoxes = UINT32((coverage * m_NumBoxes) / 100.0);
    }

    return OK;

}

void MatsHelper::Reset()
{
    // Free the memory allocated to m_BoxIdx
    std::vector<UINT32> dummy1;
    m_BoxIdx.swap(dummy1);

    // Free the memory allocated to m_BoxTlc
    std::vector<UINT64> dummy2;
    m_BoxTlc.swap(dummy2);

    // Free the memory allocated to m_Patterns
    std::vector<UINT64> dummy3;
    m_Patterns.swap(dummy3);
}

// Processes PatternTable, verifying that each entry is a valid 8-64 bit pattern
RC MatsHelper::ParseTableIndex(const char **pattern_ptr, UINT64 *value)
{
    MASSERT(pattern_ptr != NULL);
    MASSERT(value != NULL);

    // extract the pattern table index and make sure it's something valid
    const char *str = *pattern_ptr;
    unsigned long index = 0;

    while ((*str != '\0') && isdigit(*str))
    {
        index = (index * 10) + (unsigned int)(*str - '0');
        ++str;
    }

    if (index >= m_Patterns.size())
    {
        Printf(Tee::PriHigh,"Pattern index exceeds maximum value of %lu.\n",
               (unsigned long)(m_Patterns.size() - 1));
        return RC::BAD_PARAMETER;
    }

    *value = m_Patterns[index];
    *pattern_ptr = str;
    return OK;
}

const char *MatsHelper::NextSymbol(const char *str)
{
    MASSERT(str != NULL);
    while(isspace(*str)) { ++str; }
    return str;
}

// Remove any leading or trailing whitespace from the input line
string MatsHelper::StringTrim(const string &str)
{
    string ret = str;
    ret.erase(ret.find_last_not_of(' ') + 1);
    ret.erase(0, ret.find_first_not_of(' '));
    return ret;
}

// Get the integer value of a hexadecimal character
bool MatsHelper::IsHexChar(char c, unsigned int *val)
{
    MASSERT(val != NULL);
    if ((c >= '0') && (c <= '9'))
    {
        *val = (unsigned int)(c - '0');
        return true;
    }
    else if ((c >= 'a') && (c <= 'f'))
    {
        *val = (unsigned int)(c - 'a') + 10;
        return true;
    }
    else if ((c >= 'A') && (c <= 'F'))
    {
        *val = (unsigned int)(c - 'A') + 10;
        return true;
    }
    return false;
}

// Colwert string to 64 bit unsigned integer
bool MatsHelper::StrToUINT64(const char *str, UINT64 *val)
{
    MASSERT(val != NULL);
    *val = 0L;

    /* skip any leading whitespace */
    while(isspace(*str)) { ++str; }

    if ((str[0] == '0') && (str[1] == 'x'))
    {
        unsigned int digit = 0;

        /* parse hex */
        str += 2;
        while (IsHexChar(*str, &digit))
        {
            *val = (*val << 4) | digit;
            ++str;
        }
        return true;
    }
    else if (isdigit(*str))
    {
        /* parse decimal */
        while (isdigit(*str))
        {
            *val = (*val * 10) + (UINT64)(*str - '0');
            ++str;
        }
        return true;
    }
    return false;
}

