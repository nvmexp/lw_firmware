/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/mods_profile.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "core/include/xp.h"

#include <atomic>

static constexpr int numProfileTypes = static_cast<int>(Utility::ProfileType::NUM_PROFILE_TYPES);

static std::atomic<UINT64> s_ProfilingData[numProfileTypes];

Utility::Profile::Profile(ProfileType what)
: m_What(what),
  m_StartTime(Xp::GetWallTimeNS())
{
    MASSERT(what < ProfileType::NUM_PROFILE_TYPES);
}

Utility::Profile::~Profile()
{
    const UINT64 lwrTime = Xp::GetWallTimeNS();
    const UINT64 duration = lwrTime - m_StartTime;
    s_ProfilingData[static_cast<int>(m_What)] += duration;
}

UINT64 Utility::Profile::GetTotalTimeNs(ProfileType what)
{
    MASSERT(what < ProfileType::NUM_PROFILE_TYPES);
    return s_ProfilingData[static_cast<int>(what)];
}

C_(Global_GetProfilingData);
static SMethod Global_GetProfilingData
(
    "GetProfilingData",
    C_Global_GetProfilingData,
    0,
    "Returns profiling data collected so far."
);

C_(Global_GetProfilingData)
{
    JavaScriptPtr pJs;
    RC rc;

    JsArray data;
    JsArray nameValuePair;
    for (int i = 0; i < numProfileTypes; i++)
    {
        const char* name = nullptr;
        const auto type = static_cast<Utility::ProfileType>(i);
        switch (type)
        {
            case Utility::ProfileType::CRC:
                name = "CRC";
                break;

            default:
                MASSERT(!"Missing implementation!");
                name = "Unknown";
                break;
        }

        JSString* pNameStr = JS_NewStringCopyZ(pContext, name);
        if (!pNameStr)
            C_CHECK_RC_THROW(RC::CANNOT_COLWERT_STRING_TO_JSVAL, "JS error");

        const double dval = static_cast<double>(s_ProfilingData[i])
                          / 1'000'000'000.0; // Colwert from ns to s
        jsval jsValue;
        C_CHECK_RC_THROW(pJs->ToJsval(dval, &jsValue), "JS error");

        nameValuePair.push_back(STRING_TO_JSVAL(pNameStr));
        nameValuePair.push_back(jsValue);

        jsval jsNameValuePair;
        C_CHECK_RC_THROW(pJs->ToJsval(&nameValuePair, &jsNameValuePair), "JS error");
        nameValuePair.clear();

        data.push_back(jsNameValuePair);
    }

    C_CHECK_RC_THROW(pJs->ToJsval(&data, pReturlwalue), "JS error");

    return JS_TRUE;
}
