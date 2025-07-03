/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef DBGSINK_H__
#define DBGSINK_H__

#include "datasink.h"

/**
 * @class( DebuggerSink )
 *
 * Print Tee's stream to the debugger.
 */

class DebuggerSink : public DataSink
{
   public:

      DebuggerSink();

   protected:
      // DataSink override.
      void DoPrint(const char* str, size_t strLen, Tee::ScreenPrintState sps) override;
};

#endif // ! DBGSINK_H__
