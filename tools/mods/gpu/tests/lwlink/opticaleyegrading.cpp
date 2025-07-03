/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file opticaleyegrading.cpp
 * @brief: checks that OSFP eye grading values are above the specified value
 *
 */

#include "core/include/errormap.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "core/include/mle_protobuf.h"
#include "gpu/tests/gputest.h"
#include "device/interface/lwlink/lwlcci.h"

class OpticalEyeGrading : public GpuTest
{
public:
    OpticalEyeGrading();
    ~OpticalEyeGrading();

    virtual RC   Setup() override;
    virtual RC   Run() override;
    virtual RC   Cleanup() override;
    virtual bool IsSupported() override;
    virtual void PrintJsProperties(Tee::Priority pri) override;

    SETGET_PROP(TxMaintFailThreshold, UINT08);
    SETGET_PROP(RxMaintFailThreshold, UINT08);
    SETGET_PROP(TxInitFailThreshold, UINT08);
    SETGET_PROP(RxInitFailThreshold, UINT08);
    SETGET_PROP(MaintTimeMs, UINT64);

private:
    UINT08  m_TxMaintFailThreshold     = 6;
    UINT08  m_RxMaintFailThreshold     = 6;
    UINT08  m_TxInitFailThreshold      = 6;
    UINT08  m_RxInitFailThreshold      = 6;
    UINT64  m_MaintTimeMs              = 4000;
    RC      CheckGradingValues
    (
        const vector<LwLinkCableController::GradingValues> &gradingValues,
        UINT32 lwrLink
    );
    RC      InnerLoop();
};
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(OpticalEyeGrading, GpuTest, "Optical Grading Threshold Test");

CLASS_PROP_READWRITE(OpticalEyeGrading, TxMaintFailThreshold, UINT08,
                    "UINT08: lower tx threshold for optical grading values");
CLASS_PROP_READWRITE(OpticalEyeGrading, RxMaintFailThreshold, UINT08,
                    "UINT08: lower rx threshold for optical grading values");
CLASS_PROP_READWRITE(OpticalEyeGrading, TxInitFailThreshold, UINT08,
                    "UINT08: lower tx threshold for optical grading values");
CLASS_PROP_READWRITE(OpticalEyeGrading, RxInitFailThreshold, UINT08,
                    "UINT08: lower rx threshold for optical grading values");
CLASS_PROP_READWRITE(OpticalEyeGrading, MaintTimeMs, FLOAT64,
                    "FLOAT64: length of delay in ms between checking maintenance grading values");

//-----------------------------------------------------------------------------

OpticalEyeGrading::OpticalEyeGrading()
{
    SetName("OpticalEyeGrading");
}

OpticalEyeGrading::~OpticalEyeGrading()
{
}

RC OpticalEyeGrading::Setup()
{
    RC rc = RC::OK;
    CHECK_RC(GpuTest::Setup());
    return rc;
}

RC OpticalEyeGrading::Run()
{
    RC rc = RC::OK;
    TestDevicePtr pDev = GetBoundTestDevice();
    if (pDev->SupportsInterface<LwLink>())
    {
        auto pLwLink = pDev->GetInterface<LwLink>();
        if (pLwLink->SupportsInterface<LwLinkCableController>())
        {
            const UINT32 numLoops = GetTestConfiguration()->Loops();
            for (UINT32 loop = 0; loop < numLoops; ++loop)
            {
                CHECK_RC(InnerLoop());
                // Only sleep in between loops
                if (loop < (numLoops - 1))
                {
                    Tasker::Sleep(m_MaintTimeMs);
                }
            }
        }
    }
    return rc;
}

RC OpticalEyeGrading::Cleanup()
{
    RC rc = RC::OK;
    CHECK_RC(GpuTest::Cleanup());
    return rc;
}

bool OpticalEyeGrading::IsSupported()
{
    TestDevicePtr pDev = GetBoundTestDevice();
    if (pDev->SupportsInterface<LwLink>())
    {
        auto pLwLink = pDev->GetInterface<LwLink>();
        if (pLwLink->SupportsInterface<LwLinkCableController>())
        {
            return pLwLink->GetInterface<LwLinkCableController>()->GetCount() > 0;
        }
    }
    return false;
}

void OpticalEyeGrading::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);
    Printf(pri, "\tTxMaintFailThreshold        : %i\n",   m_TxMaintFailThreshold);
    Printf(pri, "\tRxMaintFailThreshold        : %i\n",   m_RxMaintFailThreshold);
    Printf(pri, "\tTxInitFailThreshold         : %i\n",   m_TxInitFailThreshold);
    Printf(pri, "\tRxInitFailThreshold         : %i\n",   m_RxInitFailThreshold);
    Printf(pri, "\tMaintTimeMs          : %lli\n", m_MaintTimeMs);
}

//-----------------------------------------------------------------------------
// Private functions
RC OpticalEyeGrading::CheckGradingValues
(
        const vector<LwLinkCableController::GradingValues> &gradingValues
        ,UINT32 lwrCciId
)
{
    StickyRC rc = RC::OK;

    for (auto const & lwrGrading : gradingValues)
    {
        const INT32 firstLane = Utility::BitScanForward(lwrGrading.laneMask);
        const INT32 lastLane = Utility::BitScanReverse(lwrGrading.laneMask);
        TestDevicePtr pDev = GetBoundTestDevice();
        string rxMaintValues;
        string rxInitValues;
        string txMaintValues;
        string txInitValues;
        string comma = ", ";

        for (INT32 lane = firstLane; lane <= lastLane; ++lane)
        {
            // Only check for maintenance values, init values should be ignored
            if (lwrGrading.rxMaint[lane] < m_RxMaintFailThreshold)
            {
                rc = RC::OPTICAL_LANE_FAILURE;
                Printf(Tee::PriError,
                       "%s LwLink %i Optical Lane %d: CC RX Maint Grading value is below "
                       "threshold %i (found) < %i (min expected)\n",
                        pDev->GetName().c_str(),
                       lwrGrading.linkId,
                       lane,
                       lwrGrading.rxMaint[lane],
                       m_RxMaintFailThreshold);

                Mle::Print(Mle::Entry::lwlink_cc_grading_mismatch)
                    .cci_id(lwrCciId)
                    .lane(lane)
                    .grading_type(Mle::LwlinkCCGradingMismatch::GradingType::rx_maint)
                    .grading(lwrGrading.rxMaint[lane])
                    .threshold(m_RxMaintFailThreshold);
            }
            if (lwrGrading.txMaint[lane] < m_TxMaintFailThreshold)
            {
                rc = RC::OPTICAL_LANE_FAILURE;
                Printf(Tee::PriError,
                       "%s LwLink %i Optical Lane %d: CC TX Maint Grading value is below "
                       "threshold %i (found) < %i (min expected)\n",
                        pDev->GetName().c_str(),
                       lwrGrading.linkId,
                       lane,
                       lwrGrading.txMaint[lane],
                       m_TxMaintFailThreshold);

                Mle::Print(Mle::Entry::lwlink_cc_grading_mismatch)
                    .cci_id(lwrCciId)
                    .lane(lane)
                    .grading_type(Mle::LwlinkCCGradingMismatch::GradingType::tx_maint)
                    .grading(lwrGrading.txMaint[lane])
                    .threshold(m_TxMaintFailThreshold);
            }
            if (lwrGrading.rxInit[lane] < m_RxInitFailThreshold)
            {
                rc = RC::OPTICAL_LANE_FAILURE;
                Printf(Tee::PriError,
                       "%s LwLink %i Optical Lane %d: CC RX Init Grading value is below "
                       "threshold %i (found) < %i (min expected)\n",
                        pDev->GetName().c_str(),
                       lwrGrading.linkId,
                       lane,
                       lwrGrading.rxInit[lane],
                       m_RxInitFailThreshold);

                Mle::Print(Mle::Entry::lwlink_cc_grading_mismatch)
                    .cci_id(lwrCciId)
                    .lane(lane)
                    .grading_type(Mle::LwlinkCCGradingMismatch::GradingType::rx_init)
                    .grading(lwrGrading.rxInit[lane])
                    .threshold(m_RxInitFailThreshold);
            }
            if (lwrGrading.txInit[lane] < m_TxInitFailThreshold)
            {
                rc = RC::OPTICAL_LANE_FAILURE;
                Printf(Tee::PriError,
                       "%s LwLink %i Optical Lane %d: CC TX Init Grading value is below "
                       "threshold %i (found) < %i (min expected)\n",
                        pDev->GetName().c_str(),
                       lwrGrading.linkId,
                       lane,
                       lwrGrading.txInit[lane],
                       m_TxInitFailThreshold);

                Mle::Print(Mle::Entry::lwlink_cc_grading_mismatch)
                    .cci_id(lwrCciId)
                    .lane(lane)
                    .grading_type(Mle::LwlinkCCGradingMismatch::GradingType::tx_init)
                    .grading(lwrGrading.txInit[lane])
                    .threshold(m_TxInitFailThreshold);
            }

            if (lane == lastLane)
                comma = "";
            rxMaintValues += Utility::StrPrintf("%i%s",
                                           lwrGrading.rxMaint[lane], comma.c_str());
            txMaintValues += Utility::StrPrintf("%i%s",
                                           lwrGrading.txMaint[lane], comma.c_str());
            rxInitValues += Utility::StrPrintf("%i%s",
                                           lwrGrading.rxInit[lane], comma.c_str());
            txInitValues += Utility::StrPrintf("%i%s",
                                           lwrGrading.txInit[lane], comma.c_str());
        }
        Printf(Tee::PriNormal, "%s LwLink %i Optical Lanes %d-%d: CC RX Maint Grading values %s\n",
                pDev->GetName().c_str(), lwrGrading.linkId, firstLane, lastLane, rxMaintValues.c_str());
        Printf(Tee::PriNormal, "%s LwLink %i Optical Lanes %d-%d: CC TX Maint Grading values %s\n",
                pDev->GetName().c_str(), lwrGrading.linkId, firstLane, lastLane, txMaintValues.c_str());
        Printf(Tee::PriNormal, "%s LwLink %i Optical Lanes %d-%d: CC RX Init Grading values %s\n",
                pDev->GetName().c_str(), lwrGrading.linkId, firstLane, lastLane, rxInitValues.c_str());
        Printf(Tee::PriNormal, "%s LwLink %i Optical Lanes %d-%d: CC TX Init Grading values %s\n",
                pDev->GetName().c_str(), lwrGrading.linkId, firstLane, lastLane, txInitValues.c_str());
    }

    return rc;
}

RC OpticalEyeGrading:: InnerLoop()
{
    StickyRC rc = RC::OK;

    auto pLwlCC =
        GetBoundTestDevice()->GetInterface<LwLink>()->GetInterface<LwLinkCableController>();
    for (UINT32 lwrCciId = 0; lwrCciId < pLwlCC->GetCount(); ++lwrCciId)
    {
        vector<LwLinkCableController::GradingValues> gradingValues;
        RC getGradingValuesRC = pLwlCC->GetGradingValues(lwrCciId, &gradingValues);
        // If GetCCGradingValues fails, then fail the test immediately
        if (getGradingValuesRC != RC::OK)
            return getGradingValuesRC;
        else
            rc = CheckGradingValues(gradingValues, lwrCciId);
    }
    return rc;
}
