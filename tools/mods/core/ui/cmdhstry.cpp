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

#include "core/include/cmdhstry.h"
#include "core/include/massert.h"

CommandHistory::CommandHistory(
   INT32 HistorySize /* = 100 */ ) :
   m_HistorySize( HistorySize ),
   m_InsertAt( 0 ),
   m_History( HistorySize )
{
   MASSERT( 0 < m_HistorySize );
}

void CommandHistory::AddCommand(
   string Command )
{
   // Do not store, if the current command is the same as the previous command.
   if ( Command != GetCommand( 1 ) )
   {
      m_History[ m_InsertAt ] = Command;

      // Move m_InsertAt to point to the next location.
      // m_History is a cirlwlar buffer.
      m_InsertAt = ( m_InsertAt + 1 ) % m_HistorySize;
   }
}

string CommandHistory::GetCommand(
   INT32 CommandsBack ) const
{
   if ( ( CommandsBack < 1 ) || ( m_HistorySize < CommandsBack ) )
      return "";

   return m_History[ ( m_InsertAt + m_HistorySize - CommandsBack )
      % m_HistorySize ];
}

