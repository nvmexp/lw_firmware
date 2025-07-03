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

#pragma once

#include "datasink.h"
#include "sockmods.h"
#include <vector>

/**
 * @class( RemoteDataSink )
 *
 * A Tee stream consumer, that writes to a remote console.
 */

class RemoteDataSink : public DataSink
{
   private:

      enum {
         BUFFERLENGTH = 512
      };

      SocketMods * m_pSocket;
      char     m_Buffer[BUFFERLENGTH];
      int      m_BufferLen;

   public:
      RemoteDataSink(SocketMods * socket);
      ~RemoteDataSink();

   protected:
      // DataSink override.
      void DoPrint(const char* str, size_t strLen, Tee::ScreenPrintState sps) override;
};
