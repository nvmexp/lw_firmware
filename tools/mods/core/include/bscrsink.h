/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef BSCRSINK_H__
#define BSCRSINK_H__

#include "types.h"
#include "datasink.h"

/**
 * @class( BatchScreenSink )
 *
 * Print Tee's stream to the batch mode screen.
 */

class BatchScreenSink : public DataSink
{
   public:

      BatchScreenSink();

   protected:
      // DataSink override.
      void DoPrint(const char* str, size_t size, Tee::ScreenPrintState sps) override;
      bool DoPrintBinary(const UINT08* data, size_t size) override;

   private:

      INT32 m_LinesWritten;

};

#endif // ! BSCRSINK_H__
