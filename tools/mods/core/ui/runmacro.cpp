/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------

#include "core/include/jscript.h"
#include "core/include/script.h"
#include "runmacro.h"
#include "core/include/tee.h"
#include <vector>

/**
 *------------------------------------------------------------------------------
 * @function(RunMacro)
 *
 * Run a macro.
 *------------------------------------------------------------------------------
 */

void RunMacro
(
   string Input
)
{
   // Split the input between spaces.

   vector< string >  Tokens;
   string::size_type Start = 0;
   string::size_type End   = 0;

   while ((Start = Input.find_first_not_of(' ', Start)) != string::npos)
   {
      End = Input.find(' ', Start);

      if (End != string::npos)
         Tokens.push_back(Input.substr(Start, End - Start));
      else
         Tokens.push_back(Input.substr(Start));

      Start = End;
   }

   if (Tokens.size() == 0)
      return;

   // Check if the Command exists.

   string Command = Tokens[0];
   if (gUIMacros.find(Command) == gUIMacros.end())
   {
      Printf(Tee::ScreenOnly , "Invalid command: %s\n", Command.c_str());
      return;
   }

   string Format = gUIMacros[Command];

   // Parse the arguments.

   JavaScriptPtr pJavaScript;
   JsArray       Arguments;
   jsval         Value;

   for (vector<string>::size_type i = 1; i < Tokens.size(); ++i)
   {
      // Check if format exists for this argument.
      if (i > Format.size())
      {
         Printf(Tee::ScreenOnly, "Format not specified for argument.\n");
         return;
      }

      switch (Format[i-1])
      {
         // signed integer
         case 'c':
         case 'C':
         case 'd':
         case 'i':
         {
            INT32 Arg = atoi(Tokens[i].c_str());
            if (OK != pJavaScript->ToJsval(Arg, &Value))
            {
               Printf(Tee::ScreenOnly, "Failed to colwert argument.\n");
               return;
            }

            Arguments.push_back(Value);

            break;
         }

         // unsigned integer
         case 'o':
         case 'u':
         case 'x':
         case 'X':
         {
            INT32 Base = 10;
            if (Format[i-1] == 'o')
               Base = 8;
            else if ((Format[i-1] == 'x') || (Format[i-1] == 'X'))
               Base = 16;

            char * pEnd;
            UINT32 Arg = strtoul(Tokens[i].c_str(), &pEnd, Base);
            if (OK != pJavaScript->ToJsval(Arg, &Value))
            {
               Printf(Tee::ScreenOnly, "Failed to colwert argument.\n");
               return;
            }

            Arguments.push_back(Value);

            break;
         }

         // double
         case 'e':
         case 'E':
         case 'f':
         case 'g':
         case 'G':
         {
            FLOAT64 Arg = atof(Tokens[i].c_str());
            if (OK != pJavaScript->ToJsval(Arg, &Value))
            {
               Printf(Tee::ScreenOnly, "Failed to colwert argument.\n" );
               return;
            }

            Arguments.push_back(Value);

            break;
         }

         // C-string
         case 's':
         case 'S':
         {
            if (OK != pJavaScript->ToJsval(Tokens[i], &Value))
            {
               Printf(Tee::ScreenOnly, "Failed to colwert argument.\n");
               return;
            }

            Arguments.push_back(Value);

            break;
         }

         default :
         {
            Printf(Tee::ScreenOnly, "Invalid format specifier.\n");
            return;
         }
      }
   } // for each argument

   // Execute the method.
   pJavaScript->CallMethod(pJavaScript->GetGlobalObject(), Command, Arguments);
}

