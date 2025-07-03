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

#include "core/include/circbuf.h"
#include "core/include/tee.h"
#include "core/include/massert.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include <string.h>

CirlwlarBuffer::CirlwlarBuffer() :
   m_pTopOfBuf(0),
   m_IsEnabled(false),
   m_PutOffset(0),
   m_BufferSize(0),
   m_RolloverCount(0)
{}

CirlwlarBuffer::~CirlwlarBuffer()
{
   Uninitialize();
}

RC CirlwlarBuffer::Uninitialize()
{
   m_IsEnabled = 0;
   if(m_pTopOfBuf) delete [] m_pTopOfBuf;
   m_pTopOfBuf = 0;
   return OK;
}

RC CirlwlarBuffer::Erase()
{
   // no need to deallocate and reallocate!
   m_PutOffset     = 0;
   m_RolloverCount = 0;
   return OK;
}

void CirlwlarBuffer::Print
(
    const char* str,
    size_t      strLen
)
{
    MASSERT(str != nullptr);

    if (m_pTopOfBuf == nullptr || !m_IsEnabled)
        return;

    //
    // Copy the data from the str parameter into the right place in our
    // cirlwlar buffer.
    //
    // Note: this can be called from an ISR.  This code is designed to not
    // scribble outside out cirlwlar buffer when called reentrantly, but
    // the messages in the buffer might get overwritten.
    //
    while (strLen)
    {
        // Local snapshot of the volatile m_PutOffset.
        UINT32 LocalOffset   = m_PutOffset;
        UINT32 BytesToEnd    = m_BufferSize - LocalOffset;

        if (BytesToEnd > strLen)
        {
            memcpy(m_pTopOfBuf + LocalOffset, str, strLen);
            m_PutOffset = LocalOffset + static_cast<UINT32>(strLen);
            strLen = 0;
        }
        else // (BytesToEnd <= strLen)
        {
            memcpy(m_pTopOfBuf + LocalOffset, str, BytesToEnd);
            m_PutOffset = 0;
            strLen -= BytesToEnd;
            str += BytesToEnd;
            m_RolloverCount ++;
        }
    }
}

RC CirlwlarBuffer::Initialize(UINT32 BufferSize)
{
   m_BufferSize = BufferSize;
   m_pTopOfBuf = new char[m_BufferSize];

   if(0 == m_pTopOfBuf)
      return RC::CANNOT_ALLOCATE_MEMORY;

   m_PutOffset     = 0;
   m_RolloverCount = 0;
   m_IsEnabled     = true;

   return OK;
}

bool CirlwlarBuffer::IsInitialized()
{
   return (true == m_IsEnabled);
}

RC CirlwlarBuffer::Dump(INT32 DumpPri)
{
   if( (0     == m_pTopOfBuf ) ||
       (false == m_IsEnabled ) )
   {
      Printf(Tee::PriNormal,"Cirlwlar buffer disabled\n");
      return RC::SOFTWARE_ERROR;
   }

   UINT32 CharsToPrint = GetCharsInBuffer();
   UINT32 StartIdx     = m_RolloverCount ? m_PutOffset : 0;

   const UINT32 BUFSIZE = 100;

   char LocalBuf[BUFSIZE];

   UINT32 i;
   UINT32 BufIteration = 0;

   Tee::SinkPriorities SinkPri(static_cast<Tee::Priority>(DumpPri));

   // this is flag to keep the cirlwlar buffer from capturing data.  We don't
   // want the dump itself to end up being fed back into the buffer!
   m_IsEnabled = false;

   // break the buffer into chucks BUFSIZE long.
   while(CharsToPrint >= BUFSIZE)
   {
      for(i=0 ; i<BUFSIZE ; i++)
      {
         UINT32 idx = (StartIdx + BUFSIZE * BufIteration + i) % m_BufferSize;
         LocalBuf[i] = m_pTopOfBuf[ idx ];
      }
      BufIteration++;
      CharsToPrint -= BUFSIZE;
      Tee::Write(SinkPri, LocalBuf, BUFSIZE);
   }

   // print out the last part of the buffer that is a fraction of BUFSIZE in
   // length
   for(i=0 ; i<CharsToPrint ; i++)
   {
      UINT32 idx = (StartIdx + BUFSIZE * BufIteration + i) % m_BufferSize;
      LocalBuf[i] = m_pTopOfBuf[ idx ];
   }

   Tee::Write(SinkPri, LocalBuf, CharsToPrint);

   // re-enable cirlwlar buffer capture.  See comment above.
   m_IsEnabled = true;
   return OK;
}

UINT32 CirlwlarBuffer::GetCharsInBuffer()
{
   // if the buffer has rolled over at least once, the number of characters
   // in it must equal the buffer size.
   return (m_RolloverCount) ? m_BufferSize : m_PutOffset;
}

UINT32 CirlwlarBuffer::GetRolloverCount()
{
   return m_RolloverCount;
}

