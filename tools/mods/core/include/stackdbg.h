/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2009 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_STACKDBG_H
#define INCLUDED_STACKDBG_H

#if DEBUG_TRACE_LEVEL >= 2

#ifndef DEBUG_TRACE_DEPTH
#define DEBUG_TRACE_DEPTH 8
#endif

#include <string>

namespace StackDebug
{

struct StackData;
typedef StackData *Handle;

Handle Create();
void Destroy(Handle stack);
size_t GetDepth(Handle stack);
const std::string Decode(Handle stack, size_t level);

} // namespace StackDebug

#endif // DEBUG_TRACE_LEVEL >= 2
#endif // INCLUDED_STACKDBG_H

