/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2011, 2014-2016, 2018-2019 by LWPU Corporation. All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// RC - return code.

#include "core/include/rc.h"
#include "core/include/globals.h"
#include "core/include/script.h"
#include "core/include/errormap.h"
#ifdef SEQ_FAIL
#include "core/include/xp.h"
#endif

void RC::Warn() const
{
    Printf(Tee::PriHigh, "Warning: possible lost RC %d.\n", m_rc);
}

const char *RC::Message() const
{
    return ErrorMap(*this).Message();
}

//------------------------------------------------------------------------------
// RC object and properties.
//
JS_CLASS(RC);

static SObject RC_Object
(
    "RC",
    RCClass,
    0,
    0,
    "Return code"
);

PROP_CONST_DESC
(
    RC,
    TEST_BASE,
    RC::TEST_BASE,
    "Multiplier for test number in combined phase:test:rc RCs."
);
PROP_CONST_DESC
(
    RC,
    PHASE_BASE,
    RC::PHASE_BASE,
    "Multiplier for phase number (i.e. pstate) in combined phase:test:rc RCs."
);

GLOBAL_JS_PROP_CONST
(
    OK,
    RC::OK,
    "Global JS constant for a 'pass'. Defined for backward-compatibility only."
    " Should not be used in new code. Use RC.OK instead."
);

#undef  DEFINE_RC
#define DEFINE_RC(errno, code, message) \
    PROP_CONST_DESC(RC, code, RC::code, message);
#define DEFINE_RETURN_CODES
#include "core/error/errors.h"

#ifdef SEQ_FAIL
#include <atomic>

namespace
{
    UINT64         s_SeqFailAt = 0U;
    atomic<UINT64> s_SeqPoint(0U);
}

namespace Utility
{
    bool IsSeqFailActive()
    {
        static bool initialized = false;
        if (!initialized)
        {
            const string elw = Xp::GetElw("SEQ_FAIL");
            if (!elw.empty())
            {
                const char* begin = elw.c_str();
                char* end = nullptr;
                const long value = strtol(begin, &end, 10);
                if (value > 0 && end && !*end)
                {
                    s_SeqFailAt = static_cast<UINT64>(value);
                }
                else
                {
                    // Note: this is ilwoked before we initialize Tee
                    printf("Invalid value in SEQ_FAIL, ignoring\n");
                }
            }
            initialized = true;
        }
        return s_SeqFailAt > 0;
    }

    bool SeqFail()
    {
        if (!IsSeqFailActive())
        {
            return false;
        }

        if (++s_SeqPoint != s_SeqFailAt)
        {
            return false;
        }

        return true;
    }
}
#endif
