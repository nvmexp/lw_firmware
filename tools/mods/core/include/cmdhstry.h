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

#ifndef CMDHSTRY_H__
#define CMDHSTRY_H__

#include "types.h"
#include <vector>
#include <string>

/**
 * @class( CommandHistory )
 *
 * Command history.
 */

class CommandHistory
{
   public:

      CommandHistory( INT32 HistorySize = 100 );

      void AddCommand( string Command );
      string GetCommand( INT32 CommandsBack ) const;

   private:

      const INT32      m_HistorySize;
      INT32            m_InsertAt;
      vector< string > m_History;

};

#endif // ! CMDHSTRY_H__
