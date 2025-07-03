/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2018 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <string>
#include <vector>

#if defined(INCLUDE_GPU)
#include <algorithm>

#include <boost/lambda/lambda.hpp>

#include "gpu/utility/blacklist.h"
#include "gpu/utility/ecccount.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/jscript.h"
#include "core/include/mgrmgr.h"
#include "core/tests/modstest.h"
#endif

#include "core/include/massert.h"
#include "core/include/restarter.h"

#include "core/include/fieldiag.h"

#if defined(INCLUDE_GPU)
static const size_t maximumBlacklistedPages = 64;
static const size_t maxRestartCount = 1;
#endif

namespace
{
    void PostWithoutRestart(ProcCtrl::Performer *performer, INT32 rc)
    {
        if (OK == rc)
        {
            performer->PostSuccess(rc);
        }
        else
        {
            performer->PostFailure(rc);
        }
    }

    class WithoutRestartPoster
    {
    public:
        WithoutRestartPoster(ProcCtrl::Performer *performer, INT32 rc)
          : m_Performer(performer)
          , m_Rc(rc)
          , m_DontNeedAnymore(false)
        {}

        ~WithoutRestartPoster()
        {
            if (!m_DontNeedAnymore)
            {
                PostWithoutRestart(m_Performer, m_Rc);
            }
        }

        void Clear()
        {
            m_DontNeedAnymore = true;
        }

    private:
        ProcCtrl::Performer *m_Performer;
        INT32                m_Rc;
        bool                 m_DontNeedAnymore;
    };

#if defined(INCLUDE_GPU)
    vector<INT32> CollectAllErrorCodes()
    {
        vector<INT32> res;
        JavaScriptPtr js;
        JsArray failedTests;

        if (OK != js->GetProperty(js->GetGlobalObject(), "g_FailedTests", &failedTests))
            return res;

        JsArray::iterator it;
        for (it = failedTests.begin(); failedTests.end() != it; ++it)
        {
            JSObject *obj = nullptr;
            if (OK != js->FromJsval(*it, &obj))
                return res;
            INT32 rc;
            if (OK != js->GetProperty(obj, "ErrorCode", &rc))
                return res;
            res.push_back(rc % RC::TEST_BASE);
        }

        return res;
    }

    bool ThereWereOnlyMemoryErrors()
    {
        namespace bl = boost::lambda;

        vector<INT32> rcs = CollectAllErrorCodes();
        return 0 == std::count_if(
            rcs.begin(),
            rcs.end(),
            bl::_1 != RC::BAD_MEMORY &&
            bl::_1 != RC::ECC_CORRECTABLE_ERROR &&
            bl::_1 != RC::SM_ECC_CORRECTABLE_ERROR &&
            bl::_1 != RC::L2_ECC_CORRECTABLE_ERROR &&
            bl::_1 != RC::FB_ECC_CORRECTABLE_ERROR &&
            bl::_1 != RC::ECC_UNCORRECTABLE_ERROR &&
            bl::_1 != RC::SM_ECC_UNCORRECTABLE_ERROR &&
            bl::_1 != RC::L2_ECC_UNCORRECTABLE_ERROR &&
            bl::_1 != RC::FB_ECC_UNCORRECTABLE_ERROR
        );
    }
#endif
}

void FieldiagRestartCheck(ProcCtrl::Performer *performer, INT32 rc)
{
    MASSERT(performer);

    WithoutRestartPoster withoutRestartPoster(performer, rc);

    // See 8 from http://lwbugs/1703848/32
    if (OK == rc)
    {
        return;
    }

#if defined(INCLUDE_GPU)
    // See 1 from http://lwbugs/1703848/32
    size_t restarts;
    performer->GetStartsCount(&restarts);
    if (restarts >= maxRestartCount)
        return;

    // See 2 from http://lwbugs/1703848/32
    if (0 == ModsTest::GetNumDoneRun())
        return;

    GpuDevMgr *dm = static_cast<GpuDevMgr *>(DevMgrMgr::d_GraphDevMgr);
    if (nullptr != dm)
    {
        size_t blPages = 0;
        size_t notFbErrors = 0;
        size_t fbUncorrErrors = 0;
        for (GpuSubdevice *sd = dm->GetFirstGpu(); nullptr != sd; sd = dm->GetNextGpu(sd))
        {
            Blacklist bl;
            bl.LoadIfSupported(sd);
            // See 4 from http://lwbugs/1703848/32
            if (maximumBlacklistedPages <= bl.GetSize())
            {
                return;
            }
            bl.Subtract(sd->GetBlacklistOnEntry());
            blPages += bl.GetSize();

            vector<UINT64> eccCounts;
            EccErrCounter *ec = sd->GetEccErrCounter();
            ec->GetUnexpectedErrors(&eccCounts);
            for (size_t i = 0; i < eccCounts.size(); ++i)
            {
                Ecc::Unit eclwnit;
                Ecc::ErrorType errType;
                MASSERT(i <= _UINT32_MAX); // Overflow check
                if (OK != ec->UnitErrFromErrCountIndex(static_cast<UINT32>(i), &eclwnit, &errType))
                {
                    MASSERT(!"Failure to colwert index to valid ecc unit\n");
                }

                if (eclwnit != Ecc::Unit::DRAM)
                {
                    notFbErrors += eccCounts[i];
                }
                else
                {
                    const UINT32 fbUncorrIndex
                        = ec->UnitErrCountIndex(Ecc::Unit::DRAM,
                                                Ecc::ErrorType::UNCORR);
                    fbUncorrErrors += eccCounts[fbUncorrIndex];
                }
            }
        }

        // See 6 from http://lwbugs/1703848/32
        if (0 == blPages)
        {
            return;
        }

        // See 7 from http://lwbugs/1703848/32
        if (0 != notFbErrors)
        {
            return;
        }

        // See 9 from http://lwbugs/1703848/32
        if (0 == fbUncorrErrors && !ThereWereOnlyMemoryErrors())
        {
            return;
        }
    }
    else
    {
        return;
    }

    withoutRestartPoster.Clear();
    Printf(Tee::ScreenOnly,
        "Memory error detected.\n"
        "Retiring the affected memory page and continuing the diagnostic.\n");
    performer->PostNeedRestart(rc);
#endif
}
