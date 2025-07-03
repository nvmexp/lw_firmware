/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007,2014,2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
// Mcp macros
//------------------------------------------------------------------------------
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

#ifndef INCLUDED_MCPMCR_H
#define INCLUDED_MCPMCR_H

// define mcp classes

/******************** Mcp Base Class Only *************************************/

#define MCP_JS_OBJECT(JsClassSymbol, CommentString) \
JS_CLASS(JsClassSymbol);             \
SObject g_##JsClassSymbol##_Object   \
   (                                 \
   "Mcp"#JsClassSymbol,              \
   JsClassSymbol##Class,             \
   0,                                \
   0,                                \
   CommentString                     \
   );

// define mcp classes constant
#define MCP_JSCLASS_CONSTANT(JsClassSymbol, PropertySymbol, ValueSymbol, CommentString)\
static SProperty JsClassSymbol##_##PropertySymbol            \
   (                                                         \
   g_##JsClassSymbol##_Object,                               \
   #PropertySymbol,                                          \
   0,                                                        \
   ValueSymbol,                                              \
   0,                                                        \
   0,                                                        \
   JSPROP_READONLY,                                          \
   "CONSTANT: " CommentString                                \
   );

// define mcp classes method
#define MCP_JSCLASS_TEST(JsClassSymbol, TestSymbol, NumArgs, CommentSring)\
C_(JsClassSymbol##_##TestSymbol);                       \
static STest JsClassSymbol##_##TestSymbol               \
   (                                                    \
   g_##JsClassSymbol##_Object,                          \
   #TestSymbol,                                         \
   C_##JsClassSymbol##_##TestSymbol,                    \
   NumArgs,                                             \
   "(Test) " CommentSring                               \
   );

// define mcp classes test
#define MCP_JSCLASS_METHOD(JsClassSymbol, MethodSymbol, NumArgs, CommentSring)\
C_(JsClassSymbol##_##MethodSymbol);                       \
static SMethod JsClassSymbol##_##MethodSymbol             \
   (                                                      \
   g_##JsClassSymbol##_Object,                            \
   #MethodSymbol,                                         \
   C_##JsClassSymbol##_##MethodSymbol,                    \
   NumArgs,                                               \
   "(Method) " CommentSring                               \
   );

// define mcp classes test withought any argunment
#define MCP_JSCLASS_TEST_NOARG(JsClassSymbol, TestSymbol, CommentString)\
MCP_JSCLASS_TEST(JsClassSymbol, TestSymbol, 0, CommentString)           \
C_(JsClassSymbol##_##TestSymbol)                              \
{                                                             \
   MASSERT(pContext     != 0);                                \
   MASSERT(pArguments   != 0);                                \
   MASSERT(pReturlwalue != 0);                                \
   RETURN_RC( JsClassSymbol##Ptr()->TestSymbol() );           \
}

// define mcp classes test withought any argunment
#define MCP_JSCLASS_METHOD_NOARG(JsClassSymbol, MethodSymbol, CommentString) \
MCP_JSCLASS_METHOD(JsClassSymbol, MethodSymbol, 0, CommentString)            \
C_(JsClassSymbol##_##MethodSymbol)                            \
{                                                             \
   MASSERT(pContext     != 0);                                \
   MASSERT(pArguments   != 0);                                \
   MASSERT(pReturlwalue != 0);                                \
   JsClassSymbol##Ptr()->MethodSymbol();                      \
   return JS_TRUE;                                            \
}

#define MCP_JSCLASS_FREE(JsClassSymbol)          \
MCP_JSCLASS_METHOD(JsClassSymbol, Free, 0, "Free Class Pointer")\
C_(JsClassSymbol##_Free)                         \
{                                                \
   MASSERT(pContext     != 0);                   \
   MASSERT(pArguments   != 0);                   \
   MASSERT(pReturlwalue != 0);                   \
                                                 \
   JsClassSymbol##Ptr::Free();                   \
   return JS_TRUE;                               \
}

// define mcp classes register Rd Wr functions
#define MCP_JSCLASS_REGRD(JsClassSymbol)                                       \
MCP_JSCLASS_METHOD(JsClassSymbol, RegRd, 1, "Read Register From Device")       \
C_(JsClassSymbol##_RegRd)                                                      \
{                                                                              \
   MASSERT(pContext     != 0);                                                 \
   MASSERT(pArguments   != 0);                                                 \
   MASSERT(pReturlwalue != 0);                                                 \
                                                                               \
   if (NumArguments != 1)                                                      \
   {                                                                           \
      JS_ReportError(pContext, "Usage: "#JsClassSymbol".RegRd(Reg)");          \
      return JS_FALSE;                                                         \
   }                                                                           \
   JavaScriptPtr pJavaScript;                                                  \
   UINT32 Reg;                                                                 \
   UINT32 Data;                                                                \
                                                                               \
   if (OK != pJavaScript->FromJsval(pArguments[0], &Reg))                      \
   {                                                                           \
      JS_ReportError(pContext, "ioLoc bad value.");                            \
      return JS_FALSE;                                                         \
   }                                                                           \
                                                                               \
   Data = JsClassSymbol##Ptr()->Get(Reg);                                      \
                                                                               \
   if (OK != pJavaScript->ToJsval(Data, pReturlwalue))                         \
   {                                                                           \
      JS_ReportError(pContext, "Error oclwrred in Ata.RdReg()");               \
      *pReturlwalue = JSVAL_NULL;                                              \
      return JS_FALSE;                                                         \
   }                                                                           \
   Printf(Tee::PriNormal, "\tReg 0x%x => 0x%0x\n", Reg, Data);              \
   return JS_TRUE;                                                             \
}

#define MCP_JSCLASS_REGWR(JsClassSymbol)                                       \
MCP_JSCLASS_TEST(JsClassSymbol, RegWr, 2, "Write Register To Device")          \
C_(JsClassSymbol##_RegWr)                                                      \
{                                                                              \
   MASSERT(pContext     != 0);                                                 \
   MASSERT(pArguments   != 0);                                                 \
   MASSERT(pReturlwalue != 0);                                                 \
                                                                               \
   if (NumArguments != 2)                                                      \
   {                                                                           \
      JS_ReportError(pContext, "Usage: "#JsClassSymbol".RegWr(RegCode, Value)");\
      return JS_FALSE;                                                         \
   }                                                                           \
                                                                               \
   JavaScriptPtr pJavaScript;                                                  \
   UINT32 Reg, Data;                                                           \
                                                                               \
   if (OK != pJavaScript->FromJsval(pArguments[0], &Reg))                      \
   {                                                                           \
      JS_ReportError(pContext, "RegName bad Value.");                          \
      return JS_FALSE;                                                         \
   }                                                                           \
   if (OK != pJavaScript->FromJsval(pArguments[1], &Data))                     \
   {                                                                           \
      JS_ReportError(pContext, "RegValue bad value.");                         \
      return JS_FALSE;                                                         \
   }                                                                           \
   Printf(Tee::PriNormal, "\tReg 0x%x <= 0x%0x\n", Reg, Data);              \
   RETURN_RC (JsClassSymbol##Ptr()->Set(Reg, Data));                           \
}

/******************** Mcp Test Class Only *************************************/

#define MCPTEST_JS_OBJECT(JsClassSymbol, CommentString) \
JS_CLASS(JsClassSymbol##Test);        \
static SObject g_##JsClassSymbol##Test_Object  \
   (                                    \
   #JsClassSymbol"Test",                \
   JsClassSymbol##TestClass,            \
   0,                                   \
   0,                                   \
   CommentString                        \
   );

// define mcp classes constant
#define MCPTEST_JSCLASS_CONSTANT(JsClassSymbol, PropertySymbol, ValueSymbol, CommentString)\
static SProperty JsClassSymbol##Test_##PropertySymbol        \
   (                                                         \
   g_##JsClassSymbol##Test_Object,                           \
   #PropertySymbol,                                          \
   0,                                                        \
   ValueSymbol,                                              \
   0,                                                        \
   0,                                                        \
   JSPROP_READONLY,                                          \
   "CONSTANT: " CommentString                                \
   );

// Used for Test classes
#define MCPTEST_JSCLASS_PROPERTY(JsClassSymbol, PropertySymbol, CommentString)\
static SProperty JsClassSymbol##Test_##PropertySymbol        \
   (                                                         \
   g_##JsClassSymbol##Test_Object,                           \
   #PropertySymbol,                                          \
   0,                                                        \
   JsClassSymbol##Test::d_##PropertySymbol,                  \
   0,                                                        \
   0,                                                        \
   0,                                                        \
   CommentString                                             \
   );

// define mcp classes method
#define MCPTEST_JSCLASS_TEST(JsClassSymbol, TestSymbol, NumArgs, CommentSring)\
C_(JsClassSymbol##Test_##TestSymbol);                   \
static STest JsClassSymbol##Test_##TestSymbol           \
   (                                                    \
   g_##JsClassSymbol##Test_Object,                      \
   #TestSymbol,                                         \
   C_##JsClassSymbol##Test_##TestSymbol,                \
   NumArgs,                                             \
   "(Test) " CommentSring                               \
   );

// define mcp classes method
#define MCPTEST_JSCLASS_METHOD(JsClassSymbol, MethodSymbol, NumArgs, CommentSring)\
C_(JsClassSymbol##Test_##MethodSymbol);                 \
static SMethod JsClassSymbol##Test_##MethodSymbol       \
   (                                                    \
   g_##JsClassSymbol##Test_Object,                      \
   #MethodSymbol,                                       \
   C_##JsClassSymbol##Test_##MethodSymbol,              \
   NumArgs,                                             \
   "(Method) " CommentSring                             \
   );

// define mcp classes test withought any argunment
#define MCPTEST_JSCLASS_TEST_NOARG(JsClassSymbol, TestSymbol, CommentString)\
MCPTEST_JSCLASS_TEST(JsClassSymbol, TestSymbol, 0, CommentString) \
C_(JsClassSymbol##Test_##TestSymbol)                          \
{                                                             \
   MASSERT(pContext     != 0);                                \
   MASSERT(pArguments   != 0);                                \
   MASSERT(pReturlwalue != 0);                                \
   RETURN_RC( JsClassSymbol##Test::TestSymbol() );            \
}

// define mcp classes test withought any argunment
#define MCPTEST_JSCLASS_METHOD_NOARG(JsClassSymbol, MethodSymbol, CommentString)\
MCPTEST_JSCLASS_METHOD(JsClassSymbol, MethodSymbol, 0, CommentString) \
C_(JsClassSymbol##Test_##MethodSymbol)                        \
{                                                             \
   MASSERT(pContext     != 0);                                \
   MASSERT(pArguments   != 0);                                \
   MASSERT(pReturlwalue != 0);                                \
   JsClassSymbol##Test::MethodSymbol();                       \
   return JS_TRUE;                                            \
}

#endif // end of mcp macros
