/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file pmudriver.cpp
//! \brief ucode fuzz driver for PMU
//!

#include "rtosucodefuzzdriver.h"
#include "gpu/tests/rmtest.h"
#include "gpu/tests/gputestc.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/channel.h"
#include "lwos.h"
#include "core/include/utility.h"
#include "core/utility/errloggr.h"
#include "class/cl85b6.h"  // GT212_SUBDEVICE_PMU
#include "ctrl/ctrl85b6.h" // GT212_SUBDEVICE_PMU CTRL
#include "gpu/perf/pmusub.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "rmpmucmdif.h"

class PmuPayloadDataTemplate : public DataTemplate
{
public:
    PmuPayloadDataTemplate(LwU8 unitId, LwU8 cmdId, LwU8 rpcFuncId, LwU32 rpcSize);
    LwU32 PayloadPresetByteCount() const;

    virtual FilledDataTemplate* Fill(const std::vector<LwU8>& data) const override;
    virtual LwU32               MaxInputSize() const override;

private:
    LwU32               m_RpcSize;

    RM_PMU_RPC_HEADER   m_PayloadMask;
    RM_PMU_RPC_HEADER   m_PayloadFill;
};

class PmuFuzzDriver: public RtosUcodeFuzzDriver
{
public:
    PmuFuzzDriver();
    virtual ~PmuFuzzDriver() = default;

    virtual LwU32        GetUcodeId() override { return LW2080_UCODE_FUZZER_PMU; }
    virtual const DataTemplate* GetDataTemplate() const override { return m_pDataTemplate.get(); }

    virtual string       IsTestSupported() override;
    virtual RC           Setup() override;
    virtual RC           Cleanup() override;
    virtual RC           PreRun(FilledDataTemplate* pData) override;

    SETGET_PROP(RpcFuncId, LwU32);
    SETGET_PROP(RpcSize, LwU32);
private:
    LwU32                    m_RpcFuncId;
    LwU32                    m_RpcSize;

    PMU                     *m_pPmu;
    LwU32                    m_pmuUcodeStateSaved;
    unique_ptr<DataTemplate> m_pDataTemplate;
};

PmuPayloadDataTemplate::PmuPayloadDataTemplate
(
    LwU8 unitId,
    LwU8 cmdId,
    LwU8 rpcFuncId,
    LwU32 rpcSize
)
{
    m_RpcSize = rpcSize;

    memset(&m_PayloadMask, 0, sizeof(m_PayloadMask));
    memset(&m_PayloadFill, 0, sizeof(m_PayloadFill));

    m_PayloadMask.flags          = 0xff;
    m_PayloadMask.size           = 0xff;
    m_PayloadMask.sc.checksum    = 0xff;
    m_PayloadMask.unitId         = 0xff;
    m_PayloadMask.function       = 0xff;
    m_PayloadMask.flcnStatus     = 0x00;
    m_PayloadMask.seqNumId       = 0xff;
    m_PayloadMask.sequenceNumber = 0xff;
    m_PayloadMask.rsvd           = 0xff;
    m_PayloadMask.time.execPmuns = 0xff;

    m_PayloadFill.flags          = 0x00;
    m_PayloadFill.unitId         = unitId;
    m_PayloadFill.function       = rpcFuncId;
}

LwU32 PmuPayloadDataTemplate::PayloadPresetByteCount() const
{
    return std::count((const LwU8*)&m_PayloadMask, (const LwU8*)(&m_PayloadMask + 1), 0xff);
}

LwU32 PmuPayloadDataTemplate::MaxInputSize() const
{
    return m_RpcSize - PayloadPresetByteCount();
}

FilledDataTemplate* PmuPayloadDataTemplate::Fill(const std::vector<LwU8>& data) const
{
    RM_PMU_RPC_HEADER *pPayload;
    auto              *pRet = new FilledRtosCmdTemplate();

    pRet->payload.resize(m_RpcSize, 0);

    pPayload  = (RM_PMU_RPC_HEADER *) pRet->payload.data();
    *pPayload = m_PayloadFill;
    FillMask(pRet->payload.begin(),
             data.begin(), data.end(),
             (const LwU8 *) &m_PayloadMask, (const LwU8 *) (&m_PayloadMask + 1));

    return pRet;
}

//! \brief PmuFuzzDriver constructor
//! \sa Setup
//-----------------------------------------------------------------------------
PmuFuzzDriver::PmuFuzzDriver()
{
    SetName("PmuFuzzDriver");
    m_pPmu = NULL;
}

//! \brief IsTestSupported: Looks for whether test can execute in current elw.
//-----------------------------------------------------------------------------
string PmuFuzzDriver::IsTestSupported()
{
    // Returns true only if the class is supported.
    if (IsClassSupported(GT212_SUBDEVICE_PMU))
    {
        return RUN_RMTEST_TRUE;
    }
    else
    {
        return "PmuFuzzDriver: GT212_SUBDEVICE_PMU class is not supported on current platform";
    }
}

//! \brief Setup: Allocate PMU instance and check RPC id
//!
//! Checking if the bootstrap rom image file is present, also obtaining
//! the instance of PMU class, through which all acces to PMU will be done.
//
//! \return RC::SOFTWARE_ERROR if the PMU bootstrap file is not found,
//!         RC::LWRM_ILWALID_ARGUMENT if the unit/rpc id is invalid
//-----------------------------------------------------------------------------
RC PmuFuzzDriver::Setup()
{
    RC     rc;

    CHECK_RC(RtosUcodeFuzzDriver::Setup());

    // Get PMU class instance
    rc = (GetBoundGpuSubdevice()->GetPmu(&m_pPmu));
    if (rc != RC::OK)
    {
        Printf(Tee::PriError, "PmuFuzzDriver: PMU not supported\n");
        CHECK_RC(rc);
    }

    //
    // get and save the current PMU ucode state.
    // We should restore the PMU ucode to this state
    // after our tests are done.
    //
    rc = m_pPmu->GetUcodeState(&m_pmuUcodeStateSaved);
    if (rc != RC::OK)
    {
        Printf(Tee::PriError, "PmuFuzzDriver: Failed to get PMU ucode state\n");
        CHECK_RC(rc);
    }

    m_pDataTemplate = make_unique<PmuPayloadDataTemplate>(GetUnitId(),
                                                          GetCmdId(),
                                                          GetRpcFuncId(),
                                                          GetRpcSize());

    return rc;
}

//! \brief PreRun: waits for pmu to be ready for a command
//------------------------------------------------------------------------------
RC PmuFuzzDriver::PreRun(FilledDataTemplate* pData)
{
    RC rc;

    rc = m_pPmu->GetUcodeState(&m_pmuUcodeStateSaved);
    if (rc != RC::OK)
    {
        Printf(Tee::PriError, "PmuFuzzDriver: Failed to get PMU ucode state\n");
        CHECK_RC(rc);
    }
    if (m_pmuUcodeStateSaved != LW85B6_CTRL_PMU_UCODE_STATE_READY)
    {
        Printf(Tee::PriError, "PmuFuzzDriver: Bug 460276: PMU is not bootstrapped.\n");
        rc = RC::LWRM_ERROR;
    }

    CHECK_RC(RtosUcodeFuzzDriver::PreRun(pData));
    return rc;
}

//! \brief Cleanup()
//!
//! As everything done in Run (lwrrently) this cleanup acts again like a
//! placeholder except few buffer free up we might need allocated in Setup
//! \sa Run()
//-----------------------------------------------------------------------------
RC PmuFuzzDriver::Cleanup()
{
    RC rc;

    m_pDataTemplate.reset();

    CHECK_RC(RtosUcodeFuzzDriver::Cleanup());
    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ PmuFuzzDriver
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(PmuFuzzDriver, RmTest, "Pmu Fuzzer Test");
DEFINE_RTOS_UCODE_FUZZ_PROPS(PmuFuzzDriver)

CLASS_PROP_READWRITE(PmuFuzzDriver, RpcFuncId, LwU32,
                     "PMU RPC function id to be fuzzed");

CLASS_PROP_READWRITE(PmuFuzzDriver, RpcSize, LwU32,
                     "Size of PMU RPC function payload to be fuzzed");
