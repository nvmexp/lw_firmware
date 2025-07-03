/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007,2009 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_ACHANNEL_H
#define INCLUDED_ACHANNEL_H

#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif

//------------------------------------------------------------------------------
// Audio channel implementation.
//
class AudioChannel
{
   public:
      AudioChannel(UINT08 * pVPBase, UINT08 * pGPBase, UINT08 * pEPBase);
      ~AudioChannel();

      // Channel definitions
      static const UINT32 VP = 0;
      static const UINT32 GP = 1;
      static const UINT32 EP = 2;

      RC Write
      (
         UINT32 Subchannel,
         UINT32 Method,
         UINT32 Data
      );
      RC Write
      (
         UINT32         Subchannel,
         UINT32         Method,
         UINT32         Count,
         const UINT32 * Data
      );

      RC WaitFreeCount
      (
         UINT32 Words,
         FLOAT64 TimeoutMs
      );

   private:
      UINT08 * m_pVPBase;
      UINT08 * m_pGPBase;
      UINT08 * m_pEPBase;

      AudioChannel(const AudioChannel &);              // not implemented
      AudioChannel & operator=(const AudioChannel &);  // not implemented
};

#endif // ! INCLUDED_ACHANNEL_H

