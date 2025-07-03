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

//!
//! \file rtosucodefuzzdriver.cpp
//! \brief ucode fuzz driver base classes for RTOS ucodes
//!

#include "rtosucodefuzzdriver.h"

#include "rmflcncmdif.h"

class RtosFlcnCmdDataTemplate : public DataTemplate
{
public:
    RtosFlcnCmdDataTemplate(LwU8 unitId, LwU8 cmdId, LwU32 cmdSize);
    LwU32 PresetByteCount() const;

    virtual FilledDataTemplate* Fill(const std::vector<LwU8>& data) const override;
    // The total size must be less than half the queue capacity (4 byte aligned)
    // PMU/SEC2/GSP/DPU have queue size of 128, fbflcn is 64, but might depend on hw
    // The problem a single 64-length cmd in the queue will timeout depending on the size
    //  of the previous cmd.  Cmds as small as 0x08 were causing this issue.
    //
    //  Another command (with a smaller size) can come in inbetween.  If
    //  we restrict the length to 60 (0x3c) max, it should never fail submission
    virtual LwU32 MaxInputSize() const override { return m_CmdSize - PresetByteCount(); }
private:
    RM_FLCN_CMD_GEN m_Mask;
    RM_FLCN_CMD_GEN m_Fill;
    LwU32           m_CmdSize;
};

RtosFlcnCmdDataTemplate::RtosFlcnCmdDataTemplate(LwU8 unitId, LwU8 cmdId, LwU32 cmdSize)
{
    m_Mask.hdr.unitId    = 0xff;
    // TODO: Size is not being fuzzed
    m_Mask.hdr.size      = 0xff;
    m_Mask.hdr.ctrlFlags = 0x00;
    m_Mask.hdr.seqNumId  = 0x00;
    m_Mask.cmd           = 0xff; // Only mask LSB of cmd

    m_Fill.hdr.unitId    = unitId;
    m_Fill.hdr.size      = 0xff;
    m_Fill.hdr.ctrlFlags = 0x00;
    m_Fill.hdr.seqNumId  = 0x00;
    m_Fill.cmd           = cmdId;

    m_CmdSize = cmdSize;
}

LwU32 RtosFlcnCmdDataTemplate::PresetByteCount() const
{
    return std::count((const LwU8*)&m_Mask, (const LwU8*)(&m_Mask + 1), 0xff);
}

FilledDataTemplate* RtosFlcnCmdDataTemplate::Fill(const std::vector<LwU8>& data) const
{
    RM_FLCN_CMD_GEN *pCmd;
    auto            *pRet = new FilledRtosCmdTemplate();

    pRet->cmd.resize(std::max(sizeof(m_Fill), data.size() + PresetByteCount()), 0);

    pCmd  = (RM_FLCN_CMD_GEN*)pRet->cmd.data();
    *pCmd = m_Fill;
    FillMask(pRet->cmd.begin(),
             data.begin(), data.end(),
             (const LwU8*)&m_Mask, (const LwU8*)(&m_Mask + 1));

    // TODO: Size is not being fuzzed
    pCmd->hdr.size = pRet->cmd.size();

    return pRet;
}

RC RtosFlcnCmdFuzzDriver::Setup()
{
    RC rc;
    CHECK_RC(RtosUcodeFuzzDriver::Setup());

    m_pDataTemplate = make_unique<RtosFlcnCmdDataTemplate>(GetUnitId(), GetCmdId(), GetCmdSize());

    return rc;
}

RC RtosUcodeFuzzDriver::PreRun(FilledDataTemplate* pData)
{
    auto *pCmd = static_cast<FilledRtosCmdTemplate*>(pData);
    if (pCmd->payload.size() != 0)
    {
        Printf(Tee::PriNormal,
               "RtosUcodeFuzzDriver: Command Data: UnitId=%d; CmdId=%d; "
               "RPC Id=%d; CmdSize=%lu, PayloadSize=%lu\n",
               m_UnitId, m_CmdId,
               ((RM_PMU_RPC_HEADER *) pCmd->payload.data())->function,
               pCmd->cmd.size(), pCmd->payload.size());
    }
    else
    {
        Printf(Tee::PriNormal,
               "RtosUcodeFuzzDriver: Command Data: UnitId=%d; CmdId=%d; "
               "CmdSize=%lu\n",
               m_UnitId, m_CmdId, pCmd->cmd.size());
    }
    return RC::OK;
}

RC RtosUcodeFuzzDriver::SubmitFuzzedData(const FilledDataTemplate* pData)
{
    auto *pCmd = static_cast<const FilledRtosCmdTemplate*>(pData);
    return UcodeFuzzer().RtosCommand(GetUcodeId(),
                                     GetTimeoutMs(),
                                     pCmd->cmd,
                                     pCmd->payload);
}

RC RtosFlcnCmdFuzzDriver::Cleanup()
{
    RC rc;

    m_pDataTemplate.reset();

    ErrorLogger::TestCompleted();

    CHECK_RC(RtosUcodeFuzzDriver::Cleanup());
    return rc;
}

RC RtosUcodeFuzzDriver::CoverageEnable()
{
    RC     rc;

    rc = UcodeFuzzer().SanitizerCovSetControl(GetUcodeId(),
                                              0,  // reset used
                                              0,  // reset missed
                                              LW_TRUE);
    return rc;
}

RC RtosUcodeFuzzDriver::CoverageDisable()
{
    RC     rc;
    RC     rcTmp;
    LwU32  used;
    LwU32  missed;
    LwBool bEnabled;

    rc = UcodeFuzzer().SanitizerCovSetControl(GetUcodeId(),
                                              1,  // don't touch used
                                              1,  // don't touch missed
                                              LW_FALSE);

    rcTmp = UcodeFuzzer().SanitizerCovGetControl(GetUcodeId(),
                                                 used,
                                                 missed,
                                                 bEnabled);
    if (rcTmp == RC::OK)
    {
        if (m_LogVerbose)
        {
            Printf(Tee::PriNormal,
                "RtosUcodeFuzzDriver: SanitizerCoverage used %u elements, missed %u callbacks\n",
                used, missed);
        }
    }

    return rc;
}

RC RtosUcodeFuzzDriver::CoverageCollect(TestcaseResult& result)
{
    result.covType = UCODEFUZZ_MSG_COV_SANITIZER_COVERAGE;
    return UcodeFuzzer().SanitizerCovDataGet(GetUcodeId(),
                                             result.covData.sanitizerCoverage);
}

