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

//! @file    cnslbuf.h
//! @brief   Declare ConsoleBuffer -- Console Buffer Class.

#ifndef INCLUDED_CNSLBUF_H
#define INCLUDED_CNSLBUF_H

#ifndef INCLUDED_CONSOLE_H
#include "console.h"
#endif

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif

#ifndef INCLUDED_STL_DEQUE
#include <deque>
#define INCLUDED_STL_DEQUE
#endif

//!
//! @class(ConsoleBuffer)  Console Buffer.
//!
//! The ConsoleBuffer class maintains the history for the console and
//! (potentially) can allow transfer of data when consoles are changed
//! as well as implementing scrolling on consoles that normally do not
//! support scrolling
//!
class ConsoleBuffer
{
public:
    ConsoleBuffer();
    ~ConsoleBuffer();

    //! This structure defines the data for each character on a
    //! line
    class CharData
    {
        public:
            explicit CharData() : c(0), state(Tee::SPS_NORMAL) { }
            explicit CharData(char aC, Tee::ScreenPrintState aState) :
                c(aC), state(aState) { }

            char                  c;     // Character data
            Tee::ScreenPrintState state; // State data for the character

    };

    //! Type for representing a line in the buffer
    typedef vector<CharData> BufferLine;

    void Initialize();
    void Resize(UINT32 width, UINT32 depth);
    UINT32 AddString(const char* str, size_t strLen, UINT32 *pStartModified);
    const BufferLine *GetLine(UINT32 line);
    void Clear();
    void SetConsoleWidth(UINT32 width);
    void SetWritePosition(UINT32 row, UINT32 col);
    void GetWritePosition(UINT32 *pRow, UINT32 *pCol);
    void SetCharAttributes(Tee::ScreenPrintState state);

    UINT32 ValidLines()  const;
    UINT32 LongestLine() const { return m_LongestLine; }
    UINT32 Width()       const { return m_Width; }
    UINT32 Depth()       const { return m_Depth; }

private:
    void ProcessStringToLine(const char* str, size_t strLen, UINT32 *pStartModified);
    void NextLine(UINT32 *pStartModified);
    void FlushCache(UINT32 *pStartModified);
    bool TryAcquireMutex();

    vector< BufferLine > m_BufferData;       //!< Console buffer history
    void       *m_Mutex;    //!< Mutex for adding to the console buffer
    UINT32      m_Width;    //!< Width in characters of the buffer
    UINT32      m_ConsoleWidth; //!< Width in characters of the console
    UINT32      m_Depth;    //!< Depth in lines of the buffer
    UINT32      m_LwrCol;   //!< Current character insertion column
    UINT32      m_FirstRow; //!< First valid row
    UINT32      m_LwrRow;   //!< Current character insertion row
    Tee::ScreenPrintState m_LwrState;   //!< Current state for characters
    UINT32      m_LongestLine; //!< Length of the longest line in the buffer

    //! Enumeration for the cache buffer size
    enum { CACHE_BUFFER_SIZE = 8 };
    UINT08      m_CachePut;     //!< Put pointer for the cache buffer
    UINT08      m_CacheGet;     //!< Get pointer for the cache buffer

    //! Buffer for caching string additions that occur while a string is
    //! being added to the real history.  The buffer is flushed on the next
    //! addition to the real history
    string      m_CacheBuffer[CACHE_BUFFER_SIZE + 1];
};
#endif //INCLUDED_CNSLBUF_H
