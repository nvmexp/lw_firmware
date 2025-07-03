/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2000-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/massert.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/tee.h"
#include "core/include/serial.h"
#include "core/include/rc.h"
#include "core/include/platform.h"
#include <stdio.h>

// TODOWJM: Figure this out
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"

namespace SerialDoor
{
   RC Uninitialize();
   RC Initialize(UINT32 Baud);

   void Callback(char Input);
   RC ParseCommand(const char *Command);

   const UINT32 PORT = 1;

   bool s_IsSerialDoorInitialized = false;

   Serial *s_pCom;
}

//------------------------------------------------------------------------------
// Javascript interface
//------------------------------------------------------------------------------
JS_CLASS(SerialDoor);

static SObject SerialDoor_Object
(
   "SerialDoor",
   SerialDoorClass,
   0,
   0,
   "SerialDoor SerialDoor"
);

C_(SerialDoor_Initialize);
STest SerialDoor_Initialize
(
   SerialDoor_Object,
   "Initialize",
   C_SerialDoor_Initialize,
   1,
   "Initialize SerialDoor Interface"
);

C_(SerialDoor_Uninitialize);
STest SerialDoor_Uninitialize
(
   SerialDoor_Object,
   "Uninitialize",
   C_SerialDoor_Uninitialize,
   0,
   "Uninitialize SerialDoor Interface"
);

//------------------------------------------------------------------------------
// Javascript methods
//------------------------------------------------------------------------------

C_(SerialDoor_Initialize)
{
   MASSERT(pContext     !=0);
   MASSERT(pArguments   !=0);
   MASSERT(pReturlwalue !=0);

   JavaScriptPtr pJavaScript;
   UINT32 Baud;
   *pReturlwalue = JSVAL_NULL;

   // Check the arguments.
   if( NumArguments != 1 )
   {
      JS_ReportError(pContext,
         "Usage: SerialDoor.Initialize(Baud)");
      return JS_FALSE;
   }

   if ( OK != pJavaScript->FromJsval( pArguments[0], &Baud ) )
   {
      JS_ReportError(pContext, "Error in SerialDoor.Initialize()");
      return JS_FALSE;
   }

   RETURN_RC( SerialDoor::Initialize(Baud) );
}

C_(SerialDoor_Uninitialize)
{
   MASSERT( pContext     != 0 );
   MASSERT( pArguments   != 0 );
   MASSERT( pReturlwalue != 0 );

   RETURN_RC( SerialDoor::Uninitialize() );
}

//------------------------------------------------------------------------------
// Generic SerialDoor Interface
//------------------------------------------------------------------------------

void SerialDoor::Callback(char Input)
{
   s_pCom->Put(SerialConst::CLIENT_SERDOOR, Input);

   string InString;
   if(Input == 13)
   {
      s_pCom->Put(SerialConst::CLIENT_SERDOOR, '\n');

      s_pCom->GetString(SerialConst::CLIENT_SERDOOR, &InString);

      ParseCommand(InString.c_str());
   }
}

RC SerialDoor::Initialize(UINT32 Baud)
{
   if(s_IsSerialDoorInitialized)
      SerialDoor::Uninitialize();

   s_pCom = GetSerialObj::GetCom(PORT);

   s_pCom->Initialize(SerialConst::CLIENT_SERDOOR, false);
   s_pCom->SetBaud(SerialConst::CLIENT_SERDOOR, Baud);
   s_pCom->SetAddCarriageReturn(true);

   string Hello("Serial backdoor active\n");

   s_pCom->PutString(SerialConst::CLIENT_SERDOOR, &Hello);

   s_pCom->SetReadCallbackFunction( Callback );

   s_IsSerialDoorInitialized=true;

   return OK;
}

RC SerialDoor::Uninitialize()
{
   if(s_IsSerialDoorInitialized)
      s_pCom->Uninitialize(SerialConst::CLIENT_SERDOOR);

   s_IsSerialDoorInitialized=false;
   return OK;
}

RC SerialDoor::ParseCommand(const char *ConstCommand)
{
   long unsigned Arg1, Arg2, Arg3;

   char OutStr[50];

   char *Command  = new char[strlen(ConstCommand)+1];

   const char *src = ConstCommand;
   char *dst = Command;

   do
   {
      if( *src == 8 )
      {
         src++;
         if(dst != Command)
            dst--;
      }
      else
      {
         *dst++ = *src++;
      }
   }
   while(*src);

   char *CmdToken = new char[strlen(Command)+1];

   CmdToken[0] = '\0';

   sscanf(Command, " %s", CmdToken );

   const char *Params = Command + strlen(CmdToken);

   GpuDevMgr    *pDevMgr = ((GpuDevMgr*)DevMgrMgr::d_GraphDevMgr);
   GpuSubdevice *pSubdev;

   //------------------------------------------------------------------------
   // Read memory 32.
   //------------------------------------------------------------------------
   if ( !strcmp( CmdToken, "mr"  ))
   {
      sscanf(Params, " %lx", &Arg1 );
      sprintf(OutStr, "%#010lx:  %#010lx\n", Arg1, (long unsigned)Platform::PhysRd32(Arg1));
   }
   //------------------------------------------------------------------------
   // Write memory 32.
   //------------------------------------------------------------------------
   else if ( !strcmp( CmdToken, "mw"  ))
   {
      sscanf(Params, " %lx %lu", &Arg1, &Arg2 );
      Platform::PhysWr32( Arg1, Arg2 );
      sprintf(OutStr, "%#010lx:  %#010lx\n", Arg1, (long unsigned)Platform::PhysRd32(Arg1));
   }
   //------------------------------------------------------------------------
   // Read framebuffer 32.
   //------------------------------------------------------------------------
   else if ( !strcmp( CmdToken, "fr"  ))
   {
      sscanf(Params, " %lx %lu", &Arg1, &Arg2 );
      pSubdev = pDevMgr->GetSubdeviceByGpuInst(Arg2);
      MASSERT(pSubdev);
      sprintf(OutStr, "%#010lx:  %#010lx\n", Arg1, (long unsigned)(pSubdev->FbRd32( Arg1 )) );
   }
   //------------------------------------------------------------------------
   // Write framebuffer 32.
   //------------------------------------------------------------------------
   else if ( !strcmp( CmdToken, "fw"  ))
   {
      sscanf(Params, " %lx %lu %lu", &Arg1, &Arg2, &Arg3 );
      pSubdev = pDevMgr->GetSubdeviceByGpuInst(Arg3);
      MASSERT(pSubdev);
      pSubdev->FbWr32( Arg1, Arg2 );
      sprintf(OutStr, "%#010lx:  %#010lx\n", Arg1, (long unsigned)(pSubdev->FbRd32( Arg1 )) );
   }
   //------------------------------------------------------------------------
   // Read register 32.
   //------------------------------------------------------------------------
   else if ( !strcmp( CmdToken, "r32" ) || !strcmp( CmdToken, " r32" ) || !strcmp( CmdToken, "r" ) || !strcmp( CmdToken, "rr" ) )
   {
      sscanf(Params, " %lx %lu", &Arg1, &Arg2 );
      pSubdev = pDevMgr->GetSubdeviceByGpuInst(Arg2);
      MASSERT(pSubdev);

      UINT32 i,j;

      for( i = 0 ; i < 256 ; i += 16 )
      {
         sprintf( OutStr, "%08lx: ", Arg1 + i );
         s_pCom->PutString( SerialConst::CLIENT_SERDOOR, OutStr, (UINT32)strlen(OutStr) );

         for( j = 0 ; j < 16 ; j += 4 )
         {
            sprintf(OutStr, " %08lx", (long unsigned)(pSubdev->RegRd32( Arg1 + i + j)) );
            s_pCom->PutString( SerialConst::CLIENT_SERDOOR, OutStr, (UINT32)strlen(OutStr) );
         }

         sprintf( OutStr, "\n" );
         s_pCom->PutString( SerialConst::CLIENT_SERDOOR, OutStr, (UINT32)strlen(OutStr) );
      }

      OutStr[0] = '\0'; // don't use this method to print
   }
   //------------------------------------------------------------------------
   // Write register 32.
   //------------------------------------------------------------------------
   else if ( !strcmp( CmdToken, "w32" ) || !strcmp( CmdToken, "rw32" ) || !strcmp( CmdToken, "w" ) || !strcmp( CmdToken, "rw" ) )
   {
      sscanf(Params, " %lx %lu %lu", &Arg1, &Arg2, &Arg3 );
      pSubdev = pDevMgr->GetSubdeviceByGpuInst(Arg3);
      MASSERT(pSubdev);
      pSubdev->RegWr32( Arg1, Arg2 );
      sprintf(OutStr, "%#010lx:  %#010lx\n", Arg1, (long unsigned)(pSubdev->RegRd32( Arg1 )) );
   }
   //------------------------------------------------------------------------
   // Read register 16.
   //------------------------------------------------------------------------
   else if ( !strcmp( CmdToken, "r16" ) || !strcmp( CmdToken, "rr16" ) )
   {
      sscanf(Params, " %lx %lu", &Arg1, &Arg2 );
      pSubdev = pDevMgr->GetSubdeviceByGpuInst(Arg2);
      MASSERT(pSubdev);
      sprintf(OutStr, "%#010lx:  %#06x\n", Arg1, (int unsigned)(pSubdev->RegRd16( Arg1 )) );
   }
   //------------------------------------------------------------------------
   // Write register 16.
   //------------------------------------------------------------------------
   else if ( !strcmp( CmdToken, "w16" ) || !strcmp( CmdToken, "rw16" ) )
   {
      sscanf(Params, " %lx %lu %lu", &Arg1, &Arg2, &Arg3 );
      pSubdev = pDevMgr->GetSubdeviceByGpuInst(Arg3);
      MASSERT(pSubdev);
      pSubdev->RegWr16( Arg1, UINT16(Arg2) );
      sprintf(OutStr, "%#010lx:  %#06x\n", Arg1, (int unsigned)(pSubdev->RegRd16( Arg1 )) );
   }
   //------------------------------------------------------------------------
   // Read register 08.
   //------------------------------------------------------------------------
   else if ( !strcmp( CmdToken, "r08" ) || !strcmp( CmdToken, "rr08" ) )
   {
      sscanf(Params, " %lx %lu", &Arg1, &Arg2 );
      pSubdev = pDevMgr->GetSubdeviceByGpuInst(Arg2);
      MASSERT(pSubdev);
      sprintf(OutStr, "%#010lx:  %#04x\n", Arg1, (int unsigned)(pSubdev->RegRd08( Arg1 )) );
   }
   //------------------------------------------------------------------------
   // Write register 08.
   //------------------------------------------------------------------------
   else if ( !strcmp( CmdToken, "w08" ) || !strcmp( CmdToken, "rw08" ) )
   {
      sscanf(Params, " %lx %lu %lu", &Arg1, &Arg2, &Arg3 );
      pSubdev = pDevMgr->GetSubdeviceByGpuInst(Arg3);
      MASSERT(pSubdev);
      pSubdev->RegWr08( Arg1, UINT08(Arg2) );
      sprintf(OutStr, "%#010lx:  %#04x\n", Arg1, (int unsigned)(pSubdev->RegRd08( Arg1 )) );
   }
   //------------------------------------------------------------------------
   // Invalid command.
   //------------------------------------------------------------------------
   else
   {
      s_pCom->PutString( SerialConst::CLIENT_SERDOOR,
         "Legal commands:\n"
         "   mr            addr              physical memory read  32\n"
         "   mw            addr data         physical memory write 32\n"
         "   fr            addr subdev       framebuffer read  32\n"
         "   fw            addr data subdev  framebuffer write 32\n"
         "   r32|rr32|r|rr addr subdev       register read  32\n"
         "   w32|rw32|w|rw addr data subdev  register write 32\n"
         "   r16|rr16      addr subdev       register read  16\n"
         "   w16|rw16      addr data subdev  register write 16\n"
         "   r08|rr08      addr subdev       register read  08\n"
         "   w08|rw08      addr data subdev  register write 08\n");

      OutStr[0] = '\0'; // so that the default print of OutStr prints null
   }

   s_pCom->PutString( SerialConst::CLIENT_SERDOOR, OutStr, (UINT32)strlen(OutStr));

   delete [] CmdToken;
   delete [] Command;

   return OK;
}

