/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DMVA_TESTS_H
#define DMVA_TESTS_H

#include <lwtypes.h>
#include "dmvaattrs.h"
#include "dmvacommon.h"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define TEST_OVLY(testId) ATTR_OVLY(".tc" STR(testId))

#define MULTIHIT_MAX_MAPPED_BLKS 5

typedef DMVA_RC (*TestFunc)(void);

typedef struct PACKED
{
    TestFunc testFunc;
    LwU32 codeStart;
    LwU32 codeEnd;
} TestOverlay;

LwU32 getTestId(void);
void loadTestOverlay(LwU32 testId);

void initSubtestIndicator(void);
void markSubtestStart(void);

#define TESTDEF(id, name)      \
    extern LwU32 _tc##id##s[]; \
    extern LwU32 _tc##id##e[]; \
    static TEST_OVLY(id) DMVA_RC name(void)

#define TESTSTRUCT(id, name) ((TestOverlay){name, ((LwU32)_tc##id##s), ((LwU32)_tc##id##e)})

#endif
