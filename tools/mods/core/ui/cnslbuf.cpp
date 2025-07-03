/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! @file    cnslbuf.cpp
//! @brief   Implement ConsoleBuffer

#include "core/include/cnslbuf.h"
#include "core/include/console.h"
#include "core/include/tasker.h"
#include "core/include/massert.h"

#include <vector>
#include <string>
#include <deque>

//-----------------------------------------------------------------------------
//! \brief Constructor
//!
ConsoleBuffer::ConsoleBuffer()
:   m_Mutex(NULL),
    m_Width(200),
    m_ConsoleWidth(200),
    m_Depth(500),
    m_LwrState(Tee::SPS_NORMAL),
    m_LongestLine(0),
    m_CachePut(0),
    m_CacheGet(0)
{
    m_BufferData.clear();
    vector<CharData> v;
    v.resize(m_Width);
    m_BufferData.assign(m_Depth, v);
    m_LwrCol = 0;
    m_FirstRow = 0;
    m_LwrRow = 0;
}

//-----------------------------------------------------------------------------
//! \brief Destructor
//!
ConsoleBuffer::~ConsoleBuffer()
{
    if (m_Mutex != NULL)
    {
        Tasker::FreeMutex(m_Mutex);
    }
    m_BufferData.clear();
}

//-----------------------------------------------------------------------------
//! \brief Initialize the console buffer
//!
void ConsoleBuffer::Initialize()
{
    if (m_Mutex == NULL)
    {
        m_Mutex = Tasker::AllocMutex("ConsoleBuffer::m_Mutex",
                                     Tasker::mtxLast);
    }
}

//-----------------------------------------------------------------------------
//! \brief Resize the console buffer to the specified size
//!
//! \param width is the width to resize the buffer to
//! \param depth is the depth to resize the buffer to
//!
void ConsoleBuffer::Resize(UINT32 width, UINT32 depth)
{
    MASSERT(m_Mutex);
    if ((width != m_Width) || (depth != m_Depth))
    {
        Tasker::MutexHolder mh(m_Mutex);
        m_Width = width;
        m_ConsoleWidth = width;
        m_Depth = depth;
        Clear();
    }
}

//-----------------------------------------------------------------------------
//! \brief Add a string to the console buffer
//!
//! \param[in]  str is the string to add
//! \param[out] pStartModified is an offset from m_FirstRow to the first
//! modified line in the buffer belonging to the current string.
//!
//! \return The number of modified lines
//!
UINT32 ConsoleBuffer::AddString(const char* str, size_t strLen, UINT32 *pStartModified)
{
    MASSERT(str);
    MASSERT(pStartModified);
    MASSERT(m_Width && m_Depth);

    UINT32 ModifiedLines = 0;

    // If the mutex can be acquired (without blocking since this may be
    // called from an interrupt
    if (TryAcquireMutex())
    {
        *pStartModified = ValidLines() - 1;
        FlushCache(pStartModified);
        ProcessStringToLine(str, strLen, pStartModified);
        FlushCache(pStartModified);
        Tasker::ReleaseMutex(m_Mutex);
        ModifiedLines = ValidLines() - *pStartModified;
    }
    else
    {
        MASSERT(((m_CachePut + 1) % CACHE_BUFFER_SIZE) != m_CacheGet);
        m_CacheBuffer[m_CachePut++].assign(str, strLen);
    }
    return ModifiedLines;
}

//-----------------------------------------------------------------------------
//! \brief Get a pointer to the specified line in the buffer
//!
//! \param line is the line to retrieve
//!
//! \return Pointer to the modified line if it exists, NULL if it does not
const ConsoleBuffer::BufferLine *ConsoleBuffer::GetLine(UINT32 line)
{
    MASSERT(line < ValidLines());

    if (line >= ValidLines())
        return NULL;

    // TODO : Need to protect this with mutex?  If so, then print updates that
    // use this function need to never occur during interrupts...
    return &m_BufferData[(m_FirstRow + line) % m_Depth];
}

//-----------------------------------------------------------------------------
//! \brief Clear the contents of the console buffer
//!
void ConsoleBuffer::Clear()
{
    MASSERT(m_Mutex);
    Tasker::MutexHolder mh(m_Mutex);

    m_BufferData.clear();
    vector<CharData> v;
    v.resize(m_Width);
    m_BufferData.assign(m_Depth, v);
    m_LwrCol = 0;
    m_FirstRow = 0;
    m_LwrRow = 0;
    m_LongestLine = 0;
}

//-----------------------------------------------------------------------------
//! \brief Set the width in characters of the current console (used to wrap
//!        lines in the buffer
//!
//! \param width is the width of the current console
//!
void ConsoleBuffer::SetConsoleWidth(UINT32 width)
{
    MASSERT(width <= m_Width);
    MASSERT(m_Mutex);
    Tasker::MutexHolder mh(m_Mutex);
    m_ConsoleWidth = width;
}

//-----------------------------------------------------------------------------
//! \brief Set the write position within the console buffer
//!
//! \param row is the row to set the write position to
//! \param col is the column to set the write position to
//!
void ConsoleBuffer::SetWritePosition(UINT32 row, UINT32 col)
{
    MASSERT(m_Mutex);
    Tasker::MutexHolder mh(m_Mutex);

    m_LwrCol = col;
    m_LwrRow = (m_FirstRow + row) % m_Depth;

    // Replace NULLs in the desired row with blanks so that the data will show
    for (UINT32 col = 0; col < m_LwrCol; col++)
    {
        if (m_BufferData[m_LwrRow][col].c == 0)
        {
            m_BufferData[m_LwrRow][col].c = ' ';
            m_BufferData[m_LwrRow][col].state = Tee::SPS_NORMAL;
        }
    }
}

//-----------------------------------------------------------------------------
//! \brief Get the write position within the console buffer
//!
//! \param pRow is the returned write row if not null
//! \param pCol is the returned write column if not null
//!
void ConsoleBuffer::GetWritePosition(UINT32 *pRow, UINT32 *pCol)
{
    if (pRow)
        *pRow = m_LwrRow;
    if (pCol)
        *pCol = m_LwrCol;
}

//-----------------------------------------------------------------------------
//! \brief Set the attributes for writing all subsequent characters
//!
//! \param state is the state os the characters to write (controls color)
//!
void ConsoleBuffer::SetCharAttributes(Tee::ScreenPrintState state)
{
    MASSERT(m_Mutex);
    Tasker::MutexHolder mh(m_Mutex);
    m_LwrState = state;
}

//-----------------------------------------------------------------------------
//! \brief Get the number of valid lines in the buffer
//!
//! \return The number of valid lines in the buffer
//!
UINT32 ConsoleBuffer::ValidLines()  const
{
    return (m_FirstRow <= m_LwrRow) ? (m_LwrRow - m_FirstRow + 1) :
                                      (m_LwrRow + m_Depth - m_FirstRow + 1);
}

//-----------------------------------------------------------------------------
//! \brief Private function to process a string into a line in the buffer
//!
//! \param[in]     str is the string to process
//! \param[in,out] pStartModified is an offset from m_FirstRow to the first
//! modified line in the buffer belonging to the current string. Assumes
//! *pStartModified was set appropriately prior to this call.
//!
void ConsoleBuffer::ProcessStringToLine(const char* str, size_t strLen, UINT32 *pStartModified)
{
    MASSERT(str);
    MASSERT(pStartModified);

    CharData charData;
    UINT32   repeatSpacesCount = 0;

    const char* const end = str + strLen;

    while (str < end)
    {
        if (repeatSpacesCount)
        {
            charData.c = ' ';
            repeatSpacesCount--;
        }
        else
        {
            charData.c = *str;
        }
        charData.state = m_LwrState;

        // Replace tabs with a 4 spaces and \n with a 0 to mark the end of line
        if (charData.c == '\t')
        {
            charData.c = ' ';

            // Need 3 more spaces to replace the tab
            repeatSpacesCount = 3;
        }
        // Do not use 'else' here since the space character by the tab still
        // needs to be processed normally
        if (charData.c == '\r')
        {
            m_LwrCol = 0;
            str++;
            continue;
        }
        else if (charData.c == '\n')
        {
            charData.c = 0;
            if (m_LwrCol < m_ConsoleWidth)
            {
                m_BufferData[m_LwrRow][m_LwrCol] = charData;
            }
            NextLine(pStartModified);
            str++;
            continue;
        }
        else if (m_LwrCol == m_ConsoleWidth)
        {
            NextLine(pStartModified);
        }

        m_BufferData[m_LwrRow][m_LwrCol] = charData;
        if (m_LwrCol > m_LongestLine)
            m_LongestLine = m_LwrCol;
        m_LwrCol++;
        str++;
    }
}

//-----------------------------------------------------------------------------
//! \brief Private function to move to the next line in the buffer
//!
//! \param[in,out] pStartModified is an offset from m_FirstRow to the first
//! modified line in the buffer belonging to the current string. Assumes
//! *pStartModified was set appropriately prior to this call.
//!
void ConsoleBuffer::NextLine(UINT32 *pStartModified)
{
    MASSERT(pStartModified);

    UINT32 nextRow = ((m_LwrRow + 1) % m_Depth);
    if (nextRow == m_FirstRow)
    {
        m_FirstRow = ((m_FirstRow + 1) % m_Depth);
        // Clear out the row about to be written
        for (UINT32 i = 0; i < m_Width; i++)
        {
            m_BufferData[nextRow][i].c = 0;
            m_BufferData[nextRow][i].state = Tee::SPS_NORMAL;
        }

        // If a single string being tracked by pStateModified has enough
        // newlines to overflow the cirlwlar buffer, every subsequent new line
        // starts modifying from the beginning of the buffer (ie. zero offset
        // from m_FirstRow).
        const bool isBufFull = (*pStartModified == 0);
        if (!isBufFull)
        {
            (*pStartModified)--;
        }
    }
    m_LwrRow = nextRow;
    m_LwrCol = 0;
}

//-----------------------------------------------------------------------------
//! \brief Private function to flushed the cache lines to the real history
//!
//! \param[in,out] pStartModified is an offset from m_FirstRow to the first
//! modified line in the buffer belonging to the current string. Assumes
//! *pStartModified was set appropriately prior to this call.
//!
void ConsoleBuffer::FlushCache(UINT32 *pStartModified)
{
    MASSERT(pStartModified);

    UINT08 cachePut = m_CachePut;

    while (m_CacheGet != cachePut)
    {
        const string& cached = m_CacheBuffer[m_CacheGet++];
        ProcessStringToLine(cached.data(), cached.size(), pStartModified);
    }
}

//-----------------------------------------------------------------------------
//! \brief Private function to try to acquire a lock on the mutex for the
//!        buffer (DO NOT BLOCK).
//!
//! \return true if lock was obtained (or called before mutexes are available)
//!
bool ConsoleBuffer::TryAcquireMutex()
{
    if (!Tasker::IsInitialized() || (m_Mutex == NULL))
    {
        return true;
    }
    return Tasker::TryAcquireMutex(m_Mutex);
}
