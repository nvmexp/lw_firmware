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

#pragma once

#include "rc.h"

/**
 * @class( CirlwlarBuffer )
 *
 * Storage for cirlwlar sink and assert info sink.
 */

class CirlwlarBuffer
{
   public:

      CirlwlarBuffer();
      ~CirlwlarBuffer();

      RC Initialize(UINT32 BufferSize);
      bool IsInitialized();
      RC Uninitialize();
      RC Dump(INT32 Priority);
      RC Erase();

      UINT32 GetCharsInBuffer();
      UINT32 GetRolloverCount();

      // DataSink override.
      void Print(const char* str, size_t strLen);

   private:

      char  *m_pTopOfBuf;
      bool   m_IsEnabled;
      volatile UINT32 m_PutOffset;  // can be touched by an ISR
      UINT32 m_BufferSize;
      UINT32 m_RolloverCount;
};
