/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2009,2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#if DEBUG_TRACE_LEVEL >= 2

#include <string>
#include <execinfo.h>
#include <cxxabi.h>
#include "core/include/stackdbg.h"

#ifndef MIN
#define MIN(x,y) ((x) < (y)) ? (x) : (y)
#endif

using namespace std;

struct StackDebug::StackData
{
    char **levels;
    void *stack[DEBUG_TRACE_DEPTH];
    size_t stacksize;
    bool decoded;
};

StackDebug::Handle StackDebug::Create()
{
    Handle ret     = (Handle)malloc(sizeof(StackData));
    ret->decoded   = false;
    ret->levels    = NULL;
    ret->stacksize = backtrace(ret->stack, DEBUG_TRACE_DEPTH);

    // if trace level 2, just keep track of the addresses for now (faster)
    // if higher, get the symbols now (otherwise we miss unloaded dynamic libs)
#if DEBUG_TRACE_LEVEL >= 3
    ret->levels = backtrace_symbols(ret->stack, ret->stacksize);
    ret->decoded = true;
#endif

    return ret;
}

void StackDebug::Destroy(Handle data)
{
    if (data)
    {
        if (data->levels)
        {
            free(data->levels);
            data->levels = NULL;
        }
        free(data);
        data = NULL;
    }
}

size_t StackDebug::GetDepth(Handle data)
{
    return data ? data->stacksize : 0;
}

const string StackDebug::Decode(Handle data, size_t level)
{
    if (!data || level >= data->stacksize || level >= DEBUG_TRACE_DEPTH)
        return "";

    if (!data->decoded)
    {
        data->levels = backtrace_symbols(data->stack, data->stacksize);
        data->decoded = true;
    }

    const string &mangled = data->levels[level];
    size_t start = mangled.find('(', 0);
    size_t end   = mangled.find(')', start);
    size_t plus  = mangled.find('+', start);
    if (start == string::npos || end == string::npos)
        return mangled;

    size_t minpos = MIN(end, plus) - 1;
    string symbol = mangled.substr(start + 1, minpos - start);

    int status;
    char *unmangled = abi::__cxa_demangle(symbol.c_str(), 0, 0, &status);
    if (unmangled == NULL)
        return mangled;

    string final = mangled;
    final.replace(start + 1, minpos - start, unmangled);
    free(unmangled);

    return final;
}

#endif // DEBUG_TRACE_LEVEL >= 2

