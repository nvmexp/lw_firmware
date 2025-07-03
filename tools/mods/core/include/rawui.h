/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef RAWUI_H__
#define RAWUI_H__

#include "types.h"
#include <string>

class RawUserInterface
{
   public:

      RawUserInterface();
      ~RawUserInterface();

      void Run();

   private:

      // Do not allow copying.
      RawUserInterface(const RawUserInterface &);
      RawUserInterface & operator=(const RawUserInterface &);
};

#endif // ! RAWUI_H__
