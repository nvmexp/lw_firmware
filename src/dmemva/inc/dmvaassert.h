/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DMVA_ASSERT_H
#define DMVA_ASSERT_H

#include "dmvaattrs.h"
#include "dmvaselwrity.h"

// compile time assert
#define CASSERT_CAT_I(a, b) a##b
#define CASSERT_CAT(a, b) CASSERT_CAT_I(a, b)
#define CASSERT(predicate) \
    typedef char CASSERT_CAT(assertion_failed_, __LINE__)[(predicate) ? 1 : -1]

// ucode assert
HS_SHARED NOINLINE void uAssertImpl(LwBool predicate, LwU32 line);
#define UASSERT(predicate) uAssertImpl(predicate, __LINE__)

#endif
