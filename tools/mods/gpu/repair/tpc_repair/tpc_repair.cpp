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

#include "tpc_repair.h"
#include "class/cl90e7.h"

TpcRepair::TpcRepair(GpuSubdevice *pSubdev)
    : m_pSub(pSubdev)
{
    MASSERT(pSubdev);
}

RC TpcRepair::IsSupported() const
{
    if (!(m_pSub->Regs().IsSupported(MODS_FUSE_DISABLE_EXTEND_OPT_TPC_GPC, 0) &&
          m_pSub->Regs().IsSupported(MODS_FUSE_OPT_TPC_GPCx_RECONFIG, 0)))
    {
        Printf(Tee::PriError, "TPC repair not supported on this device\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    return RC::OK;
}

bool TpcRepair::IsTpcRepairReg(UINT32 address) const
{
    RegHal &regs = m_pSub->Regs();
    UINT32 numDisableExtendRegs = regs.LookupAddress(MODS_FUSE_DISABLE_EXTEND_OPT_TPC_GPC__SIZE_1);
    for (UINT32 gpc = 0; gpc < numDisableExtendRegs; gpc++)
    {
        if (address == regs.LookupAddress(MODS_FUSE_DISABLE_EXTEND_OPT_TPC_GPC, gpc))
        {
            return true;
        }
    }
    return false;
}

RC TpcRepair::ReadLwrrInforomParams
(
    LW90E7_CTRL_RPR_GET_INFO_PARAMS *pInforomParams
) const
{
    return LwRmPtr()->ControlSubdeviceChild(m_pSub,
                                            GF100_SUBDEVICE_INFOROM,
                                            LW90E7_CTRL_CMD_RPR_GET_INFO,
                                            pInforomParams,
                                            sizeof(*pInforomParams));
}

RC TpcRepair::WriteInforomParams
(
    LW90E7_CTRL_RPR_WRITE_OBJECT_PARAMS *pInforomParams
) const
{
    return LwRmPtr()->ControlSubdeviceChild(m_pSub,
                                            GF100_SUBDEVICE_INFOROM,
                                            LW90E7_CTRL_CMD_RPR_WRITE_OBJECT,
                                            pInforomParams,
                                            sizeof(*pInforomParams));
}

RC TpcRepair::CopyLwrrInforomParams
(
    LW90E7_CTRL_RPR_WRITE_OBJECT_PARAMS *pInforomParams,
    bool includeTpcRepairData
) const
{
    RC rc;

    LW90E7_CTRL_RPR_GET_INFO_PARAMS lwrrInforomParams = { 0 };
    CHECK_RC(ReadLwrrInforomParams(&lwrrInforomParams));

    // On Ampere, the RPR object is shared between the FS preservation table
    // and TPC repair
    UINT32& newEntryCount = pInforomParams->entryCount;
    for (UINT32 i = 0; i < lwrrInforomParams.entryCount; i++)
    {
        auto& entry = lwrrInforomParams.repairData[i];
        if (includeTpcRepairData ||
            (!includeTpcRepairData && !IsTpcRepairReg(entry.address)))
        {
            pInforomParams->repairData[newEntryCount].address = entry.address;
            pInforomParams->repairData[newEntryCount].data    = entry.data;
            newEntryCount++;
        }
    }

    return rc;
}

RC TpcRepair::ParsePopulateInforomArgs
(
    string cmdInput,
    vector<TpcRepairData> *pRepairData
) const
{
    RC rc;

    if (cmdInput.empty())
    {
        Printf(Tee::PriError, "No repair data provided\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    // Input is in the format "<gpcN1>:<tpc_mask>,<gpcN2>:<tpc_mask>..."
    vector<string> repairGpcVec;
    if (Utility::Tokenizer(cmdInput, ",", &repairGpcVec) != RC::OK)
    {
        Printf(Tee::PriError, "Invalid repair data input format - %s\n", cmdInput.c_str());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    UINT32 gpc               = 0;
    UINT32 disableExtendMask = 0;
    UINT32 maxGpcCount       = m_pSub->GetMaxGpcCount();
    for (const auto& perGpcData : repairGpcVec)
    {
        vector<string> perGpcRepairVec;
        if (Utility::Tokenizer(perGpcData, ":", &perGpcRepairVec) != RC::OK ||
            perGpcRepairVec.size() != 2)
        {
            Printf(Tee::PriError, "Invalid repair data input %s\n", perGpcData.c_str());
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        CHECK_RC(Utility::StringToUINT32(perGpcRepairVec[0].c_str(), &gpc, 0));
        CHECK_RC(Utility::StringToUINT32(perGpcRepairVec[1].c_str(), &disableExtendMask, 0));
        if (gpc >= maxGpcCount)
        {
            Printf(Tee::PriError, "Invalid GPC number %u\n", gpc);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        pRepairData->emplace_back(gpc, disableExtendMask);
    }

    return rc;
}

RC TpcRepair::PopulateInforomEntries
(
    string repairInput,
    bool bAppend
) const
{
    RC rc;

    CHECK_RC(IsSupported());

    UINT32 maxRprEntries;
    CHECK_RC(m_pSub->GetInfoRomRprMaxDataCount(&maxRprEntries));

    // Get TPC repair data
    vector<TpcRepairData> repairData;
    CHECK_RC(ParsePopulateInforomArgs(repairInput, &repairData));

    LW90E7_CTRL_RPR_WRITE_OBJECT_PARAMS ifrParams = { 0 };
    CHECK_RC(CopyLwrrInforomParams(&ifrParams, bAppend));

    RegHal &regs  = m_pSub->Regs();
    UINT32& entry = ifrParams.entryCount;
    for (const auto& data : repairData)
    {
        ifrParams.repairData[entry].address = regs.LookupAddress(MODS_FUSE_DISABLE_EXTEND_OPT_TPC_GPC,
                                                                 data.first);
        ifrParams.repairData[entry].data    = data.second;
        entry++;

        if (entry > maxRprEntries)
        {
            Printf(Tee::PriError, "Too many entries for RPR object! Have: %u; Max %u\n",
                                   entry, maxRprEntries);
            return RC::SOFTWARE_ERROR;
        }
    }

    CHECK_RC(WriteInforomParams(&ifrParams));
    return rc;
}

RC TpcRepair::ClearInforomEntries() const
{
    RC rc;

    CHECK_RC(IsSupported());

    LW90E7_CTRL_RPR_WRITE_OBJECT_PARAMS ifrParams = { 0 };
    CHECK_RC(CopyLwrrInforomParams(&ifrParams, false));
    CHECK_RC(WriteInforomParams(&ifrParams));

    return rc;
}

RC TpcRepair::PrintInforomEntries() const
{
    RC rc;

    CHECK_RC(IsSupported());

    LW90E7_CTRL_RPR_GET_INFO_PARAMS ifrParams = { 0 };
    CHECK_RC(ReadLwrrInforomParams(&ifrParams));

    UINT32 numRcds = 0;
    Printf(Tee::PriNormal, "TPC Repair IFR Records:\n");
    for (UINT32 i = 0; i < ifrParams.entryCount; i++)
    {
        auto& entry = ifrParams.repairData[i];
        // On Ampere, the RPR object is shared between the FS preservation table
        // and TPC repair
        if (IsTpcRepairReg(entry.address))
        {
            Printf(Tee::PriNormal, "[0x%08x] = 0x%08x\n", entry.address, entry.data);
            numRcds++;
        }
    }
    if (numRcds == 0)
    {
        Printf(Tee::PriNormal, "No TPC repair entries found\n");
        return rc;
    }

    return rc;
}
