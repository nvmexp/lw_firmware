/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2008 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// MODS simple (command line) user interface.

#ifndef INCLUDED_SIMPLEUI_H
#define INCLUDED_SIMPLEUI_H

#ifndef INCLUDED_TYPES_H
#include "types.h"
#endif
#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

class CommandHistory;
class Console;

class SimpleUserInterface
{
   public:

      SimpleUserInterface(bool StartInScriptMode);
      ~SimpleUserInterface();

      void Run(bool AllowYield = true);
      static string GetLine();

   private:

      static string GetInput(Console *pConsole, bool AllowYield, SimpleUserInterface *pUI);

      bool             m_IsInsertMode;
      bool             m_RecordUserInput;
      bool             m_IsScriptUserInterface;
      CommandHistory * m_pHistory;
      INT32            m_CommandsBack;

      // Do not allow copying.
      SimpleUserInterface(const SimpleUserInterface &);
      SimpleUserInterface & operator=(const SimpleUserInterface &);
};

#endif // !INCLUDED_SIMPLEUI_H
