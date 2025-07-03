/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2009,2014-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_MCPMACRO_H
#define INCLUDED_MCPMACRO_H

#include "mcpdrf.h"
#include "core/include/tee.h"

//!
//! \file mcpmacro.h
//! \brief General macros for MCP classes
//!
//! This file contains general macros for MCP code. In particular, it
//! contains macros to be used in Gen2 code. As the code tree is
//! cleaned up and ported to Gen2, portions of the file mcpmcr.h
//! should also be moved here. Anything that is deemed to be *really*
//! useful (and would be useful across functional group boundaries)
//! should probably be moved to script.h instead so that it is
//! accessible to the GPU/WMP team as well. Don't forget to
//! leave good comments!
//!

#ifdef XP_WIN
    // Logging functions
    #define FUNCNAME __FUNCTION__
    #define LOG_ENT()\
        Printf(Tee::PriDebug, "<Enter> %s::%s\n", CLASS_NAME, FUNCNAME)
    #define LOG_ENT_C(format, ...)\
        Printf(Tee::PriDebug, "<Enter> %s::%s: " format "\n", CLASS_NAME, FUNCNAME, ##__VA_ARGS__)
    #define LOG_EXT()\
        Printf(Tee::PriDebug, "<Exit> %s::%s\n", CLASS_NAME, FUNCNAME)
    #define LOG_EXT_C(format, ...)\
        Printf(Tee::PriDebug, "<Exit> %s::%s: " format "\n", CLASS_NAME, FUNCNAME, ##__VA_ARGS__)
    #define LOG_ERR(format, ...)\
        Printf(Tee::PriHigh, "ERROR: " format "\n", ##__VA_ARGS__)
    #define LOG_WRN(format, ...)\
        Printf(Tee::PriHigh, "WARNING: " format "\n", ##__VA_ARGS__)
    #define LOG_HGH(format, ...)\
        Printf(Tee::PriHigh, format "\n", ##__VA_ARGS__)

    #define LOG_INFO(format, ...)\
        Printf(Tee::PriLow, format "\n", ##__VA_ARGS__)
    #define LOG_INFO_ENTER()\
        Printf(Tee::PriLow, "<Enter> %s::%s\n", CLASSNAME, FUNCNAME)
    #define LOG_INFO_EXIT()\
        Printf(Tee::PriLow, "<Exit> %s::%s\n", CLASSNAME, FUNCNAME)
    #define LOG_INFO_ENTER_C(format, ...)\
        Printf(Tee::PriLow, "<Enter> %s::%s: " format "\n", CLASSNAME, FUNCNAME, ##__VA_ARGS__)
    #define LOG_INFO_EXIT_C(format, ...)\
        Printf(Tee::PriLow, "<Exit> %s::%s: " format "\n", CLASSNAME, FUNCNAME, ##__VA_ARGS__)
    #define LOG_DBG(format, ...)\
        Printf(Tee::PriDebug, format, ##__VA_ARGS__)
    /*
    #define LOG_ERROR(message)\
        Printf(Tee::PriHigh, "*** ERROR: %s::%s" message " ***\n", CLASSNAME, FUNCNAME)
    #define LOG_ERROR_C(format, ...)\
        Printf(Tee::PriHigh, "*** ERROR: %s::%s" format " ***\n", CLASSNAME, FUNCNAME, ##__VA_ARGS__)
    #define LOG_WARNING(message)\
        Printf(Tee::PriHigh, "WARNING: %s::%s" message "\n", CLASSNAME, FUNCNAME)
    #define LOG_WARNING_C(format, ...)\
        Printf(Tee::PriHigh, "WARNING: %s::%s" format "\n", CLASSNAME, FUNCNAME, ##__VA_ARGS__)
    */

    #define LOG_PASS_FAIL_C(rc, format, ...)                                       \
    {                                                                              \
        if(OK==rc)                                                                 \
        {                                                                          \
            Printf(Tee::PriHigh, "$$$ PASSED $$$");                                \
        }                                                                          \
        else                                                                       \
        {                                                                          \
            Printf(Tee::PriHigh, "*** FAILED ***");                                \
        }                                                                          \
        Printf(Tee::PriHigh,  " %s::%s(" format ")\n", CLASS_NAME, FUNCNAME, ##__VA_ARGS__);\
    }

#else
    // Logging functions
    #define FUNCNAME __FUNCTION__
    #define LOG_ENT()\
        Printf(Tee::PriDebug, "<Enter> %s::%s\n", CLASS_NAME, FUNCNAME)
    #define LOG_ENT_C(format, args...)\
        Printf(Tee::PriDebug, "<Enter> %s::%s: " format "\n", CLASS_NAME, FUNCNAME, ##args)
    #define LOG_EXT()\
        Printf(Tee::PriDebug, "<Exit> %s::%s\n", CLASS_NAME, FUNCNAME)
    #define LOG_EXT_C(format, args...)\
        Printf(Tee::PriDebug, "<Exit> %s::%s: " format "\n", CLASS_NAME, FUNCNAME, ##args)
    #define LOG_ERR(format, args...)\
        Printf(Tee::PriHigh, "ERROR: " format "\n", ##args)
    #define LOG_WRN(format, args...)\
        Printf(Tee::PriHigh, "WARNING: " format "\n", ##args)
    #define LOG_HGH(format, args...)\
        Printf(Tee::PriHigh, format "\n", ##args)

    #define LOG_INFO(format, args...)\
        Printf(Tee::PriLow, format "\n", ##args)
    #define LOG_INFO_ENTER()\
        Printf(Tee::PriLow, "<Enter> %s::%s\n", CLASSNAME, FUNCNAME)
    #define LOG_INFO_EXIT()\
        Printf(Tee::PriLow, "<Exit> %s::%s\n", CLASSNAME, FUNCNAME)
    #define LOG_INFO_ENTER_C(format, args...)\
        Printf(Tee::PriLow, "<Enter> %s::%s: " format "\n", CLASSNAME, FUNCNAME, ##args)
    #define LOG_INFO_EXIT_C(format, args...)\
        Printf(Tee::PriLow, "<Exit> %s::%s: " format "\n", CLASSNAME, FUNCNAME, ##args)
    #define LOG_DBG(format, args...)\
        Printf(Tee::PriDebug, format, ##args)
    /*
    #define LOG_ERROR(message)\
        Printf(Tee::PriHigh, "*** ERROR: %s::%s" message " ***\n", CLASSNAME, FUNCNAME)
    #define LOG_ERROR_C(format, args...)\
        Printf(Tee::PriHigh, "*** ERROR: %s::%s" format " ***\n", CLASSNAME, FUNCNAME, ##args)
    #define LOG_WARNING(message)\
        Printf(Tee::PriHigh, "WARNING: %s::%s" message "\n", CLASSNAME, FUNCNAME)
    #define LOG_WARNING_C(format, args...)\
        Printf(Tee::PriHigh, "WARNING: %s::%s" format "\n", CLASSNAME, FUNCNAME, ##args)
    */

    #define LOG_PASS_FAIL_C(rc, format, args...)                                \
    {                                                                              \
        if(OK==rc)                                                                 \
        {                                                                          \
            Printf(Tee::PriHigh, "$$$ PASSED $$$");                                \
        }                                                                          \
        else                                                                       \
        {                                                                          \
            Printf(Tee::PriHigh, "*** FAILED ***");                                \
        }                                                                          \
        Printf(Tee::PriHigh,  " %s::%s(" format ")\n", CLASS_NAME, FUNCNAME, ##args);\
    }
#endif

#define LOG_PASS_FAIL(rc)                                                      \
{                                                                              \
    if(OK==rc)                                                                 \
    {                                                                          \
        Printf(Tee::PriHigh, "$$$ PASSED $$$");                                \
    }                                                                          \
    else                                                                       \
    {                                                                          \
        Printf(Tee::PriHigh, "*** FAILED ***");                                \
    }                                                                          \
    Printf(Tee::PriHigh, " %s::%s()\n", CLASS_NAME, FUNCNAME);                 \
}

#define RECORD_FIRST_FAILURE(rc, f)                                           \
{                                                                             \
    if(rc==OK)                                                                \
    {                                                                         \
        rc = (f);                                                             \
    }                                                                         \
    else                                                                      \
    {                                                                         \
        (f);                                                                  \
    }                                                                         \
}

#define PRINT_PHYS                      "0x%08llx"
#define PRINT_FMT_PTR(ptr) ((UINT64)(uintptr_t)(ptr))

//------------------ Test related macros ---------------------------------------
#define MCPTEST_GEN2_JSCLASS_PROPERTY(JsClassSymbol, PropertySymbol, DefaultValue, CommentString)\
static SProperty JsClassSymbol##_##PropertySymbol        \
   (                                                     \
   JsClassSymbol##_Object,                               \
   #PropertySymbol,                                          \
   0,                                                        \
   DefaultValue,                                             \
   0,                                                        \
   0,                                                        \
   0,                                                        \
   CommentString                                             \
   )

#define MCP_GEN2_JSCLASS_TEST(JsClassSymbol, TestSymbol, NumArgs, CommentSring)\
C_(JsClassSymbol##_##TestSymbol);                       \
static STest JsClassSymbol##_##TestSymbol               \
   (                                                    \
   JsClassSymbol##_Object,                              \
   #TestSymbol,                                         \
   C_##JsClassSymbol##_##TestSymbol,                    \
   NumArgs,                                             \
   "(Test) " CommentSring                                \
   );

#define MCP_GEN2_JSCLASS_METHOD(JsClassSymbol, MethodSymbol, NumArgs, CommentSring)\
C_(JsClassSymbol##_##MethodSymbol);                       \
static SMethod JsClassSymbol##_##MethodSymbol             \
   (                                                      \
   JsClassSymbol##_Object,                                \
   #MethodSymbol,                                         \
   C_##JsClassSymbol##_##MethodSymbol,                    \
   NumArgs,                                               \
   "(Method) " CommentSring                                \
   );

// Macro to create basic property setter and getter functions
#define SETGET_MCPTEST_PROP(propName, resulttype) \
    resulttype Get##propName(){ return m_##propName; } \
    RC Set##propName(resulttype val){ m_##propName = val; return OK; }

#define SETGET_MCPTEST_PROP_VAR(propName, resultType, cVar)  \
    resultType Get##propName(){ return cVar; }     \
    RC Set##propName(resultType val){ cVar = val; return OK; }

#define SETGET_MCPTEST_PROP_FUNC(propName, resultType, setFunc, getFunc)  \
    resultType Get##propName(){ return getFunc(); }     \
    RC Set##propName(resultType val){ RC rc; CHECK_RC(setFunc(val)); return OK; }

#endif // INCLUDED_MCPMACRO_H

