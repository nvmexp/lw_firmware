/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2010 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
// TCP socket for JS implementation
//------------------------------------------------------------------------------

#include "core/include/massert.h"
#include "core/include/rc.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/sockmods.h"
#include "core/include/tee.h"
#include "core/include/xp.h"
#include <string.h>

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ SocketJs object.
//------------------------------------------------------------------------------

static void SocketJs_finalize(JSContext * pContext, JSObject * pObject);
C_(SocketJs_constructor);
C_(SocketJs_Connect);
C_(SocketJs_ListenOn);
C_(SocketJs_Accept);
C_(SocketJs_Read);
C_(SocketJs_ReadData);
C_(SocketJs_Write);
C_(SocketJs_WriteData);
C_(SocketJs_Flush);
C_(SocketJs_Close);
C_(SocketJs_GetHostIPs);

static JSClass SocketJs_class =
{
   "Socket",
   JSCLASS_HAS_PRIVATE,       // Our "private" data is our SocketJs*.
   JS_PropertyStub,           // addProperty
   JS_PropertyStub,           // delProperty
   JS_PropertyStub,           // getProperty
   JS_PropertyStub,           // setProperty
   JS_EnumerateStub,          // enumerate
   JS_ResolveStub,            // resolve
   JS_ColwertStub,            // colwert
   SocketJs_finalize,         // finalize (free our C++ SocketJs object)
};

// This static C++ object's constructor will hook C_SocketJs_constructor into
// the JavaScript symbol table as a JS constructor function (aka JS "class").
static SObject SocketJs_SObject
(
   "Socket",
   SocketJs_class,
   0,
   0,
   "Socket Javascript interface",
   C_SocketJs_constructor
);

//! Connect socket to IP/Port
/**
 * @param IP, Port
 *
 * @return true or false
 */
static STest SocketJs_Connect
(
   SocketJs_SObject,
   "Connect",
   C_SocketJs_Connect,
   2,  // number of args
   "Connect to IP, port"
);

//! Listen for connection on Port
/**
 * @param Port
 *
 * @return true or false
 */
static STest SocketJs_ListenOn
(
   SocketJs_SObject,
   "ListenOn",
   C_SocketJs_ListenOn,
   1,  // number of args
   "Start listening for connection on port, includes bind"
);

//! Accept connection that has connected to listening port
/**
 * @param
 *
 * @return true or false
 */
static STest SocketJs_Accept
(
   SocketJs_SObject,
   "Accept",
   C_SocketJs_Accept,
   0,  // number of args
   "Accept a connection that has connected to listening port"
);

//! Write a string to a socket
/**
 * @param string
 *
 * @return true or false
 */
static STest SocketJs_Write
(
   SocketJs_SObject,
   "Write",
   C_SocketJs_Write,
   1,  // number of args
   "Write a string to an opened socket"
);

//! Write data to a socket
/**
 * @param data
 *
 * @return true or false
 */
static STest SocketJs_WriteData
(
   SocketJs_SObject,
   "WriteData",
   C_SocketJs_WriteData,
   1,  // number of args
   "Write an array of bytes to an opened socket"
);

//! Read data from connected socket
/**
 * @param data
 *
 * @return true or false
 */
static STest SocketJs_Read
(
   SocketJs_SObject,
   "Read",
   C_SocketJs_Read,
   1,  // number of args
   "Read a string from an opened socket"
);

//! Read data from connected socket
/**
 * @param data
 *
 * @return true or false
 */
static STest SocketJs_ReadData
(
   SocketJs_SObject,
   "ReadData",
   C_SocketJs_ReadData,
   1,  // number of args
   "Read an array of bytes from an opened socket"
);

//! Force flush of socket
/**
 * @param
 *
 * @return true or false
 */
static STest SocketJs_Flush
(
   SocketJs_SObject,
   "Flush",
   C_SocketJs_Flush,
   0,  // number of args
   "Force flush of data"
);

//! Close socket
/**
 * @param
 *
 * @return true or false
 */
static STest SocketJs_Close
(
   SocketJs_SObject,
   "Close",
   C_SocketJs_Close,
   0,  // number of args
   "Close the socket"
);

//! Return local IP addresses
/**
 * @param address
 *
 * @return true or false
 */
static STest SocketJs_GetHostIPs
(
   SocketJs_SObject,
   "GetHostIPs",
   C_SocketJs_GetHostIPs,
   1,  // number of args
   "Find the local IP addresses"
);

//! Left for backwards compatibility
static STest SocketJs_GetHostIP
(
   SocketJs_SObject,
   "GetHostIP",
   C_SocketJs_GetHostIPs,
   1,  // number of args
   "Find the local IP addresses"
);

/* C imlementation */

//------------------------------------------------------------------------------
C_(SocketJs_constructor)
{
   SocketMods * sock = Xp::GetSocket();
   if (!sock)
   {
      Printf(Tee::PriHigh, "Failed to allocate Socket!\n");
   }
   return JS_SetPrivate(pContext, pObject, sock);
}

//------------------------------------------------------------------------------
static void SocketJs_finalize
(
   JSContext * pContext,
   JSObject * pObject
)
{
   SocketMods * sock = (SocketMods*)JS_GetPrivate(pContext, pObject);
   if (sock)
   {
      sock->Close();
      Xp::FreeSocket(sock);
      sock = 0;
   }
   JS_SetPrivate(pContext, pObject, sock);
}

C_(SocketJs_Connect)
{
   MASSERT(pContext     != 0);
   MASSERT(pObject      != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;
   *pReturlwalue = JSVAL_NULL;
   SocketMods * pSocket = (SocketMods*)JS_GetPrivate(pContext, pObject);

   // Make sure the user passed us exactly 0 arguments.
   if(NumArguments !=2)
   {
      JS_ReportError(pContext, "Usage: Socket.Connect(\"x.x.x.x\", Port)");
      return JS_FALSE;
   }

   if (!pSocket)
   {
      JS_ReportError(pContext, "Socket() not allocated!");
      return JS_FALSE;
   }

   string HostName;
   // Read IP
   if (OK != pJavaScript->FromJsval(pArguments[0], &HostName))
   {
      JS_ReportError(pContext, "Error reading IP");
      return JS_FALSE;
   }

   UINT32 Port;
   // Read IP
   if (OK != pJavaScript->FromJsval(pArguments[1], &Port))
   {
      JS_ReportError(pContext, "Error reading Port");
      return JS_FALSE;
   }

   UINT32 Ip;
   Ip = Socket::ParseIp(HostName);

   if (Ip == 0)
   {
      Printf(Tee::PriNormal,
             "IP %s translated to 0.0.0.0! Usage: Connect(\"127.0.0.1\", 22)\n",
             HostName.c_str());
   }

   RC rc = pSocket->Connect(Ip, Port);
   if (rc != OK)
   {
      JS_ReportWarning(pContext, "Error in Socket.Connect(), can't connect");
      RETURN_RC(rc);
   }

   Printf(Tee::PriDebug, "Socket Connected\n");
   RETURN_RC(rc);
}

C_(SocketJs_ListenOn)
{
   MASSERT(pContext     != 0);
   MASSERT(pObject      != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;
   *pReturlwalue = JSVAL_NULL;
   SocketMods * pSocket = (SocketMods*)JS_GetPrivate(pContext, pObject);

   // Make sure the user passed us exactly 1 arguments.
   if(NumArguments !=1)
   {
      JS_ReportError(pContext, "Usage: Socket.ListenOn(Port)");
      return JS_FALSE;
   }

   if (!pSocket)
   {
      JS_ReportError(pContext, "Socket() not allocated!");
      return JS_FALSE;
   }

   UINT32 Port;
   // Read IP
   if (OK != pJavaScript->FromJsval(pArguments[0], &Port))
   {
      JS_ReportError(pContext, "Error reading Port");
      return JS_FALSE;
   }

   RC rc = pSocket->ListenOn(Port);
   if (rc != OK)
   {
      JS_ReportWarning(pContext, "Error in Socket.ListenOn()");
      RETURN_RC(rc);
   }

   Printf(Tee::PriDebug, "Socket Open\n");
   RETURN_RC(rc);
}

C_(SocketJs_Accept)
{
   MASSERT(pContext     != 0);
   MASSERT(pObject      != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   //JavaScriptPtr pJavaScript;
   *pReturlwalue = JSVAL_NULL;
   SocketMods * pSocket = (SocketMods*)JS_GetPrivate(pContext, pObject);

   // Make sure the user passed us exactly 1 arguments.
   if(NumArguments !=0)
   {
      JS_ReportError(pContext, "Usage: Socket.Accept()");
      return JS_FALSE;
   }

   if (!pSocket)
   {
      JS_ReportError(pContext, "Socket() not allocated!");
      return JS_FALSE;
   }

   RC rc = pSocket->Accept();
   if (rc != OK)
   {
      JS_ReportWarning(pContext, "Error in Socket.Accept()");
      RETURN_RC(rc);
   }

   Printf(Tee::PriDebug, "Socket Connection Accepted\n");
   RETURN_RC(rc);
}

C_(SocketJs_Write)
{
   MASSERT(pContext     != 0);
   MASSERT(pObject      != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;
   *pReturlwalue = JSVAL_NULL;
   SocketMods * pSocket = (SocketMods*)JS_GetPrivate(pContext, pObject);

   // Make sure the user passed us exactly 1 arguments.
   if(NumArguments != 1)
   {
      JS_ReportError(pContext, "Usage: Socket.Write(String)");
      return JS_FALSE;
   }

   if (!pSocket)
   {
      JS_ReportError(pContext, "Socket() not allocated!");
      return JS_FALSE;
   }

   string StrBuf;
   char * Buffer = 0;
   UINT32   Size = 0;

   // if the user passed in an array for the first argument
   if (OK != (pJavaScript->FromJsval(pArguments[0], &StrBuf)))
      RETURN_RC(RC::BAD_PARAMETER);

   Size = (UINT32)StrBuf.size();

   Printf(Tee::PriDebug, "Parsing a string of size %d\n", Size);
   if(Size == 0)
   {
      Printf(Tee::PriNormal, "Can't parse a string of size zero\n");
      return JS_FALSE;
   }

   Buffer = new char[Size];

   strncpy( Buffer, StrBuf.c_str(), Size);

   // print out the list of parameters
   Printf(Tee::PriDebug, "List of %d parameters parsed:\n", Size);
   for(UINT32 i = 0; i < Size; i++)
      Printf(Tee::PriDebug,"%c", Buffer[i]);

   Printf(Tee::PriDebug, "\n");

   RC rc = pSocket->Write(Buffer, Size);

   delete[] Buffer;

   RETURN_RC(rc);
}

C_(SocketJs_WriteData)
{
   MASSERT(pContext     != 0);
   MASSERT(pObject      != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;
   *pReturlwalue = JSVAL_NULL;
   SocketMods * pSocket = (SocketMods*)JS_GetPrivate(pContext, pObject);

   // Make sure the user passed us exactly 1 arguments.
   if(NumArguments == 0)
   {
      JS_ReportError(pContext, "Usage: Socket.WriteData(Array) or WriteData(a,b,c)");
      return JS_FALSE;
   }

   if (!pSocket)
   {
      JS_ReportError(pContext, "Socket() not allocated!");
      return JS_FALSE;
   }

   JsArray  DataArray;
   char * Buffer = 0;
   UINT32   Size = 0;

   // if the user passed in an array for the first argument
   if (OK == (pJavaScript->FromJsval(pArguments[0], &DataArray)))
   {
      Size = (UINT32)DataArray.size();

      Printf(Tee::PriDebug, "Parsing an array of size %d\n", Size);
      if(Size == 0)
      {
         Printf(Tee::PriNormal, "Can't parse an array of size zero\n");
         return JS_FALSE;
      }

      Buffer = new char[Size];

      for(UINT32 i = 0; i < Size; i++)
      {
         UINT32 Val;
         if(OK != pJavaScript->FromJsval( DataArray[i], &Val ))
         {
            delete[] Buffer;

            Printf(Tee::PriNormal, "Can't parse element %d of the array.\n", i);
            return JS_FALSE;
         }
         Buffer[i] = Val;
      }
   }
   else
      // if the user passed in a list of ints e.g. (1,2,3,4,5)
   {
      Buffer = new char[NumArguments];
      for(UINT32 i = 0; i < NumArguments; i++)
      {
         UINT32 Val;
         if ( OK != pJavaScript->FromJsval(pArguments[i], &Val))
         {
            delete[] Buffer;

            JS_ReportError(pContext, "Can't colwert parameters from JavaScript");
            return JS_FALSE;
         }
         Buffer[i] = Val;
      }
      Size = NumArguments;
   }

   // print out the list of parameters
   Printf(Tee::PriDebug, "List of %d parameters parsed:\n", Size);
   for(UINT32 i = 0; i < Size; i++)
      Printf(Tee::PriDebug,"%02x ", Buffer[i]);

   Printf(Tee::PriDebug, "\n");

   RC rc = pSocket->Write(Buffer, Size);

   delete[] Buffer;

   RETURN_RC(rc);
}

C_(SocketJs_Read)
{
   MASSERT(pContext     != 0);
   MASSERT(pObject      != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JSObject * pReturlwals;
   JavaScriptPtr pJavaScript;
   *pReturlwalue = JSVAL_NULL;
   SocketMods * pSocket = (SocketMods*)JS_GetPrivate(pContext, pObject);

   // Make sure the user passed us exactly 1 arguments.
   if (
       (NumArguments != 1)
       || (OK != pJavaScript->FromJsval(pArguments[0], &pReturlwals))
       )
   {
      JS_ReportError(pContext, "Usage: Socket.Read(String)");
      return JS_FALSE;
   }

   if (!pSocket)
   {
      JS_ReportError(pContext, "Socket() not allocated!");
      return JS_FALSE;
   }

   UINT32   BufSize   = 1024;
   char   * Buffer    = new char[BufSize + 1];
   string   StrBuf;
   UINT32   Size      = 0;
   RC       rc;

   rc = pSocket->Read(Buffer, BufSize, &Size);
   if (rc != OK)
   {
      delete[] Buffer;
      RETURN_RC(rc);
   }
   Buffer[Size] = 0;
   StrBuf.append(Buffer);
   delete[] Buffer;

   size_t c;
   while((size_t)-1 != (c = StrBuf.find('\r')))
   {
      StrBuf.erase(c, 1);
   }

   C_CHECK_RC(pJavaScript->SetProperty(pReturlwals, "length", 0));
   RETURN_RC(pJavaScript->SetElement(pReturlwals, 0, StrBuf));
}

C_(SocketJs_ReadData)
{
   MASSERT(pContext     != 0);
   MASSERT(pObject      != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   RC rc;
   JSObject * pReturlwals;
   JavaScriptPtr pJavaScript;
   *pReturlwalue = JSVAL_NULL;
   SocketMods * pSocket = (SocketMods*)JS_GetPrivate(pContext, pObject);

   // Make sure the user passed us exactly 1 arguments.
   if (
       (NumArguments != 1)
       || (OK != pJavaScript->FromJsval(pArguments[0], &pReturlwals))
       )
   {
      JS_ReportError(pContext, "Usage: Socket.ReadData(Data)");
      return JS_FALSE;
   }

   if (!pSocket)
   {
      JS_ReportError(pContext, "Socket() not allocated!");
      return JS_FALSE;
   }

   C_CHECK_RC(pJavaScript->SetProperty(pReturlwals, "length", 0));

   UINT32   BufSize   = 1024;
   char   * Buffer    = new char[BufSize];
   UINT32   Size      = 0;

   rc = pSocket->Read(Buffer, BufSize, &Size);
   if (rc != OK)
   {
      delete[] Buffer;
      RETURN_RC(rc);
   }

   for(UINT32 i = 0; i < Size; i++)
   {
      UINT32 Val = static_cast<unsigned char>(Buffer[i]);
      C_CHECK_RC(pJavaScript->SetElement(pReturlwals, i, Val));
   }

   delete[] Buffer;

   RETURN_RC(rc);
}

C_(SocketJs_Flush)
{
   MASSERT(pContext     != 0);
   MASSERT(pObject      != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   //JavaScriptPtr pJavaScript;
   *pReturlwalue = JSVAL_NULL;
   SocketMods * pSocket = (SocketMods*)JS_GetPrivate(pContext, pObject);

   // Make sure the user passed us exactly 1 arguments.
   if(NumArguments !=0)
   {
      JS_ReportError(pContext, "Usage: Socket.Flush()");
      return JS_FALSE;
   }

   if (!pSocket)
   {
      JS_ReportError(pContext, "Socket() not allocated!");
      return JS_FALSE;
   }

   RC rc = pSocket->Flush();
   if (rc != OK)
   {
      JS_ReportWarning(pContext, "Error in Socket.Flush()");
      RETURN_RC(rc);
   }

   Printf(Tee::PriDebug, "Socket Flushed\n");
   RETURN_RC(rc);
}

C_(SocketJs_Close)
{
   MASSERT(pContext     != 0);
   MASSERT(pObject      != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   //JavaScriptPtr pJavaScript;
   *pReturlwalue = JSVAL_NULL;
   SocketMods * pSocket = (SocketMods*)JS_GetPrivate(pContext, pObject);

   // Make sure the user passed us exactly 1 arguments.
   if(NumArguments !=0)
   {
      JS_ReportError(pContext, "Usage: Socket.Close()");
      return JS_FALSE;
   }

   if (!pSocket)
   {
      JS_ReportError(pContext, "Socket() not allocated!");
      return JS_FALSE;
   }

   RC rc = pSocket->Close();
   if (rc != OK)
   {
      JS_ReportWarning(pContext, "Error in Socket.Close()");
      RETURN_RC(rc);
   }

   Printf(Tee::PriLow, "Socket Closed\n");
   RETURN_RC(rc);
}

C_(SocketJs_GetHostIPs)
{
   MASSERT(pContext     != 0);
   MASSERT(pObject      != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   RC rc;
   JSObject * pReturlwals;
   JavaScriptPtr pJavaScript;
   *pReturlwalue = JSVAL_NULL;
   SocketMods * pSocket = (SocketMods*)JS_GetPrivate(pContext, pObject);

   // Make sure the user passed us exactly 1 arguments.
   if (
       (NumArguments != 1)
       || (OK != pJavaScript->FromJsval(pArguments[0], &pReturlwals))
       )
   {
      JS_ReportError(pContext, "Usage: Socket.SocketJs_GetHostIPs(Ip)");
      return JS_FALSE;
   }

   if (!pSocket)
   {
      JS_ReportError(pContext, "Socket() not allocated!");
      return JS_FALSE;
   }

   vector<UINT32> ips;
   C_CHECK_RC(pSocket->GetHostIPs(&ips));

   C_CHECK_RC(pJavaScript->SetProperty(pReturlwals, "length", 0));
   for (unsigned i=0; i < ips.size(); i++)
   {
      string literalIp = Socket::IpToString(ips[i]);
      Printf(Tee::PriLow, "Host IP is %s (0x%x)\n", literalIp.c_str(), ips[i]);
      C_CHECK_RC(pJavaScript->SetElement(pReturlwals, i,
                  literalIp));
   }
   RETURN_RC(rc);
}

