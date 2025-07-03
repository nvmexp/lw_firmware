/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#include "core/include/jscript.h"
#include "core/include/script.h"
#include "core/include/platform.h"
#include "core/include/massert.h"

/**
 * @file   pio.cpp
 * @brief  Contains JavaScript interface for Port I/O.
 */

JS_CLASS(Pio);

//! Port I/O class.
static SObject Pio_Object
(
   "Pio",
   PioClass,
   0,
   0,
   "Port input and output."
);

//
// Methods and Tests
//

C_(Pio_Read08);
//! Read one byte from a port.
/**
 * @param arg1 Port to read from.
 * @return Byte read from the port.
 */
static SMethod Pio_Read08
(
   Pio_Object,
   "Read08",
   C_Pio_Read08,
   1,
   "Read byte from port."
);

C_(Pio_Read16);
//! Read two bytes from a port.
/**
 * @param arg1 Port to read from.
 * @return Bytes read from the port.
 */
static SMethod Pio_Read16
(
   Pio_Object,
   "Read16",
   C_Pio_Read16,
   1,
   "Read word from port."
);

C_(Pio_Read32);
//! Read four bytes from a port.
/**
 * @param arg1 Port to read from.
 * @return Bytes read from the port.
 */
static SMethod Pio_Read32
(
   Pio_Object,
   "Read32",
   C_Pio_Read32,
   1,
   "Read long from port."
);

C_(Pio_Write08);
//! Writes one byte of data to a port.
/**
 * @param arg1 Port to write to.
 * @param arg2 Data to write.
 */
static STest Pio_Write08
(
   Pio_Object,
   "Write08",
   C_Pio_Write08,
   2,
   "Write byte to port."
);

C_(Pio_Write16);
//! Writes two bytes of data to a port.
/**
 * @param arg1 Port to write to.
 * @param arg2 Data to write.
 */
static STest Pio_Write16
(
   Pio_Object,
   "Write16",
   C_Pio_Write16,
   2,
   "Write word to port."
);

C_(Pio_Write32);
//! Writes four bytes of data to a port.
/**
 * @param arg1 Port to write to.
 * @param arg2 Data to write.
 */
static STest Pio_Write32
(
   Pio_Object,
   "Write32",
   C_Pio_Write32,
   2,
   "Write long to port."
);

//
// Implementation
//

// SMethod
C_(Pio_Read08)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   *pReturlwalue = JSVAL_NULL;

   JavaScriptPtr pJavaScript;

   // Check the arguments.
   UINT32 Port = 0;
   if
   (
         (NumArguments != 1)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Port))
   )
   {
      JS_ReportError(pContext, "Usage: Pio.Read08(port)");
      return JS_FALSE;
   }

   UINT08 temp = 0;
   if
   (
         (OK != Platform::PioRead08(Port, &temp))
      || (OK != pJavaScript->ToJsval(temp, pReturlwalue))
   )
   {
      JS_ReportError(pContext, "Error oclwred in Pio.Read08()");
      *pReturlwalue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

// SMethod
C_(Pio_Read16)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   *pReturlwalue = JSVAL_NULL;

   JavaScriptPtr pJavaScript;

   // Check the arguments.
   UINT32 Port = 0;
   if
   (
         (NumArguments != 1)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Port))
   )
   {
      JS_ReportError(pContext, "Usage: Pio.Read16(port)");
      return JS_FALSE;
   }

   UINT16 temp = 0;
   if
   (
         (OK != Platform::PioRead16(Port, &temp))
      || (OK != pJavaScript->ToJsval(temp, pReturlwalue))
   )
   {
      JS_ReportError(pContext, "Error oclwrred in Pio.Read16()");
      *pReturlwalue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

// SMethod
C_(Pio_Read32)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   *pReturlwalue = JSVAL_NULL;

   JavaScriptPtr pJavaScript;

   // Check the arguments.
   UINT32 Port = 0;
   if
   (
         (NumArguments != 1)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Port))
   )
   {
      JS_ReportError(pContext, "Usage: Pio.Read32(port)");
      return JS_FALSE;
   }

   UINT32 temp = 0;
   if
   (
         (OK != Platform::PioRead32(Port, &temp))
      || (OK != pJavaScript->ToJsval(temp, pReturlwalue))
   )
   {
      JS_ReportError(pContext, "Error oclwrred in Pio.Read32()");
      *pReturlwalue = JSVAL_NULL;
      return JS_FALSE;
   }

   return JS_TRUE;
}

// STest
C_(Pio_Write08)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;

   // Check the arguments.
   UINT32 Port = 0;
   UINT32 Data = 0;
   if
   (
         (NumArguments != 2)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Port))
      || (OK != pJavaScript->FromJsval(pArguments[1], &Data))
   )
   {
      JS_ReportError(pContext, "Usage: Pio.Write08(port, data)");
      return JS_FALSE;
   }

   RETURN_RC(Platform::PioWrite08(Port, Data));
}

// STest
C_(Pio_Write16)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;

   // Check the arguments.
   UINT32 Port = 0;
   UINT32 Data = 0;
   if
   (
         (NumArguments != 2)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Port))
      || (OK != pJavaScript->FromJsval(pArguments[1], &Data))
   )
   {
      JS_ReportError(pContext, "Usage: Pio.Write16(port, data)");
      return JS_FALSE;
   }

   RETURN_RC(Platform::PioWrite16(Port, Data));
}

// STest
C_(Pio_Write32)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;

   // Check the arguments.
   UINT32 Port = 0;
   UINT32 Data = 0;
   if
   (
         (NumArguments != 2)
      || (OK != pJavaScript->FromJsval(pArguments[0], &Port))
      || (OK != pJavaScript->FromJsval(pArguments[1], &Data))
   )
   {
      JS_ReportError(pContext, "Usage: Pio.Write32(port, data)");
      return JS_FALSE;
   }

   RETURN_RC(Platform::PioWrite32(Port, Data));
}
