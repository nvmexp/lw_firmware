/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2001-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
// Cirlwlar sink.
//------------------------------------------------------------------------------
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

#ifndef CIRCSINK_H__
#define CIRCSINK_H__

#include "datasink.h"
#include "circbuf.h"
#include "rc.h"

#include <string>

/**
 * @class( CirlwlarSink )
 *
 * Print Tee's stream to a cirlwlar buffer
 */

class CirlwlarSink : public DataSink
{
   public:

      CirlwlarSink() = default;
      ~CirlwlarSink();

      RC Initialize();
      void SetCircBufferSize(UINT32 size);
      bool IsInitialized();
      RC Uninitialize();
      RC Dump(INT32 Priority);
      RC Erase();

      const char* GetFilterString() const { return m_Filter.c_str(); }
      void SetFilterString(const char* filter) { m_Filter = filter; }

      void SetExtraMessages(bool value);

      UINT32 GetCharsInBuffer();
      UINT32 GetRolloverCount();

   protected:
      // DataSink override.
      void DoPrint(const char* str, size_t strLen, Tee::ScreenPrintState sps) override;

   private:

      static constexpr UINT32 CIRLWLAR_SIZE = static_cast<UINT32>(16_MB);

      CirlwlarBuffer m_CircBuffer;
      string m_Filter;
      bool   m_IsEnabled      = false;
      bool   m_ExtraMessages  = true;
      UINT32 m_CircBufferSize = CIRLWLAR_SIZE;
};

#endif // ! CIRCSINK_H__
