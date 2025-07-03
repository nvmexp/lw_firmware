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

//------------------------------------------------------------------------------
// AssertInfo sink.
//------------------------------------------------------------------------------
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

#ifndef ASSERTINFOSINK_H__
#define ASSERTINFOSINK_H__

#include "datasink.h"
#include "circbuf.h"
#include "rc.h"

/**
 * @class( AssertInfoSink )
 *
 * Print Tee's stream to a cirlwlar buffer, for printing
 * information during an assertion failure
 */

class AssertInfoSink : public DataSink
{
   public:
      AssertInfoSink();
      virtual ~AssertInfoSink();

      AssertInfoSink(const AssertInfoSink&)            = delete;
      AssertInfoSink& operator=(const AssertInfoSink&) = delete;

      RC Initialize();
      bool IsInitialized();
      RC Uninitialize();
      RC Dump(INT32 Priority);
      RC Erase();

      void Print(const char* str, size_t strLen, const Mle::Context* pMleContext);

   private:
      void DoPrint(const char * str, size_t strLen, Tee::ScreenPrintState sps) override;

      struct Impl;
      unique_ptr<Impl> m_pImpl;
};

#endif // ! ASSERTINFOSINK_H__
