/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/fuse/newfuse.h"

#include "ampere/ga100/dev_bus.h" // For IFF format
#include "core/include/fuseparser.h"
#include "core/include/mle_protobuf.h"
#include "core/include/types.h"
#include "core/include/xp.h"
#include "device/interface/lwlink.h"
#include "gpu/fuse/fuseutils.h"
#include "gpu/include/testdevice.h"
#include "gpu/utility/ga10xfalcon.h"
#include "gpu/utility/gm20xfalcon.h"

#if LWCFG(GLOBAL_ARCH_LWSWITCH)
#include "gpu/utility/lwlink/lwlinkdevif/lwl_lwswitch_dev.h"
#endif

using namespace FuseUtil;
//==============================================================================
NewFuse::NewFuse
(
    TestDevice *pDev,
    unique_ptr<SkuHandler>&& pSkuHandler,
    shared_ptr<FuseEncoder>&& pEncoder,
    map<FuseMacroType, shared_ptr<FuseHwAccessor>>&& hwAccessors,
    unique_ptr<DownbinImpl>&& pDownbinImpl
)
: m_pDev(pDev)
, m_pSkuHandler(move(pSkuHandler))
, m_pEncoder(move(pEncoder))
, m_HwAccessors(move(hwAccessors))
, m_pDownbin(move(pDownbinImpl))
{
    MASSERT(pDev);
    for (const auto& typeAccessorPair : m_HwAccessors)
    {
        typeAccessorPair.second->SetParent(this);
    }
    if (m_pDownbin)
    {
        m_pDownbin->SetParent(this);
    }
    if (FuseUtils::GetShowFusingPrompt())
    {
        m_pPromptHandler = std::move(CreateFusePromptHandler());
    }
}

unique_ptr<FusePromptBase> NewFuse::CreateFusePromptHandler()
{
    return std::move(make_unique<FusePromptBase>());
}

string NewFuse::GetPromptMessage()
{
    string msg = Utility::StrPrintf("Do you want to proceed "
                                   "with HW Fusing (y for yes/n for no)?\n");
    return msg;
}

//==============================================================================
// HW FUSING

//------------------------------------------------------------------------------
// Fuse chip to the given SKU (-fuse_sku)
RC NewFuse::FuseSku(const string& sku)
{
    RC rc;
    FuseValues request;
    if (m_pDownbin)
    {
        CHECK_RC(m_pDownbin->DownbinSku(sku));
    }
    if (m_HwAccessors[FuseMacroType::Fuse]->IsFusePrivSelwrityEnabled())
    {
        CHECK_RC(m_pSkuHandler->GetDesiredFuseValues(sku, false, false, &request));
        return RunFuseReworkBinary(sku, request);
    }
    else
    {
        CHECK_RC(m_pSkuHandler->AddDebugFuseOverrides(sku));
        CHECK_RC(m_pSkuHandler->GetDesiredFuseValues(sku, false, false, &request));
        return BlowFuses(request, m_NumValidRir);
    }
}

//------------------------------------------------------------------------------
// Fuse a set of fuses directly to given values (-blow_one_fuse)
RC NewFuse::BlowFuses(const map<string, UINT32>& fuseValues)
{
    FuseValues request;
    request.m_NamedValues = fuseValues;
    return BlowFuses(request, ALL_RIR_VALID);
}

//------------------------------------------------------------------------------
// Take a fuse request, encode the values into binary data, and then blow that
// data into the fuse macros
RC NewFuse::BlowFuses(const FuseValues& request, UINT32 numValidRir)
{
    RC rc;
    FuseBinaryMap fuseBinary;
    CHECK_RC(m_pEncoder->EncodeFuseValues(request, &fuseBinary));
    if (numValidRir != ALL_RIR_VALID)
    {
        CHECK_RC(IlwalidateRirRecords(&fuseBinary, numValidRir));
    }
    return BlowFuseRows(fuseBinary);
}

//------------------------------------------------------------------------------
// Blow fuses using array constructed with user defined specific fuseRow, Value pairs
RC NewFuse::BlowFuseRows(const vector<UINT32>& regsToBlow, const FuseMacroType& macroType)
{
    RC rc;
    FuseBinaryMap fuseBinary;
    fuseBinary[macroType] = regsToBlow;
    CHECK_RC(BlowFuseRows(fuseBinary));
    return rc;
}

//------------------------------------------------------------------------------
// Send binary data to the FuseHwAccessors to burn into the fuses
RC NewFuse::BlowFuseRows(const FuseBinaryMap& fuseBinary)
{
    RC rc;
    if (FuseUtils::GetShowFusingPrompt())
    {
        string promptMsg = GetPromptMessage();

        CHECK_RC(m_pPromptHandler->SetPromptMsg(promptMsg));
        rc = m_pPromptHandler->PromptUser();
        if (rc == RC::EXIT_OK)
        {
            // This code is reached when n is pressed for prompt
            // we skip blowing fuses and continue with default flow

            Printf(Tee::PriWarn, "No fuses were blown.\n");
            return RC::OK;
        }
        CHECK_RC(rc);
    }

    m_pEncoder->MarkDecodeDirty();
    for (const auto& macroBinaryPair : fuseBinary)
    {
        const auto& macroType = macroBinaryPair.first;
        const auto& binaryData = macroBinaryPair.second;
        if (m_HwAccessors.count(macroType) == 0)
        {
            Printf(FuseUtils::GetErrorPriority(),
                "Encoder returned data for a non-existent macro!\n");
            return RC::SOFTWARE_ERROR;
        }
        Printf(FuseUtils::GetPrintPriority(), "\nBurning %s macro:\n", fuseMacroString[macroType].c_str());
        CHECK_RC(m_HwAccessors[macroType]->BlowFuses(binaryData));
    }

    // Resense fuses so OPT registers read their new values
    for (const auto& macroHwPair : m_HwAccessors)
    {
        // Resense FUSE last since it will enable security
        if (macroHwPair.first == FuseMacroType::Fuse)
        {
            continue;
        }
        CHECK_RC(macroHwPair.second->LatchFusesToOptRegs());
    }
    CHECK_RC(m_HwAccessors[FuseMacroType::Fuse]->LatchFusesToOptRegs());
    MarkFuseReadDirty();
    return rc;
}

//==============================================================================
// SW FUSING

//------------------------------------------------------------------------------
// SW Fuse a chip to a SKU (-sw_fuse_sku)
RC NewFuse::SwFuseSku(const string& sku)
{
    RC rc;
    FuseValues request;
    if (m_pDownbin)
    {
        CHECK_RC(m_pDownbin->DownbinSku(sku));
    }
    CHECK_RC(m_pSkuHandler->AddDebugFuseOverrides(sku));
    CHECK_RC(m_pSkuHandler->GetDesiredFuseValues(sku, true, true, &request));
    return SetSwFuses(request);
}

//------------------------------------------------------------------------------
// SW Fuse a set of fuses directly to given values (-set_one_sw_fuse)
RC NewFuse::SetSwFuses(const map<string, UINT32>& fuseValues)
{
    FuseValues request;
    request.m_NamedValues = fuseValues;
    return SetSwFuses(request);
}

//------------------------------------------------------------------------------
// Take a fuse request, encode the values into SW fuse writes, and then execute
// those writes
RC NewFuse::SetSwFuses(const FuseValues& request)
{
    RC rc;
    map<FuseMacroType, map<UINT32, UINT32>> swFuseWrites;
    CHECK_RC(m_pEncoder->EncodeSwFuseValues(request, &swFuseWrites));
    if (m_pDev->HasFeature(Device::GPUSUB_SUPPORTS_IFF_SW_FUSING))
    {
        vector<UINT32> iffRows;
        CHECK_RC(m_pEncoder->EncodeSwIffValues(request, &iffRows));

        RegHal& regHal = m_pDev->Regs();
        UINT32 iffCrc = 0;
        if (regHal.IsSupported(MODS_FUSE_OPT_SELWRE_IFF_CRC_CHECK))
        {
            CHECK_RC(m_pEncoder->CallwlateIffCrc(iffRows, &iffCrc));
        }


        CHECK_RC(SetSwIffFuses(iffRows, iffCrc));
        CHECK_RC(WriteSwFuses(swFuseWrites));
        MarkFuseReadDirty();
        CHECK_RC(SwReset());
        VerifySwIffFusing(iffRows, iffCrc);
    }
    else
    {
        CHECK_RC(WriteSwFuses(swFuseWrites));
        MarkFuseReadDirty();
        CHECK_RC(SwReset());
    }
    return rc;
}

//------------------------------------------------------------------------------
// Send SW fuse writes to the FuseHwAccessors
RC NewFuse::WriteSwFuses(const SwFuseMap& swFuseWrites)
{
    RC rc;
    for (const auto& macroSwFusePair : swFuseWrites)
    {
        const auto& macroType = macroSwFusePair.first;
        const auto& swFuseWrite = macroSwFusePair.second;
        if (m_HwAccessors.count(macroType) == 0)
        {
            Printf(Tee::PriError, "Encoder returned data for a non-existent macro!\n");
            return RC::SOFTWARE_ERROR;
        }
        CHECK_RC(m_HwAccessors[macroType]->SetSwFuses(swFuseWrite));
    }
    return rc;
}

//------------------------------------------------------------------------------
// Send SW fuse writes to the FuseHwAccessors
RC NewFuse::SetSwIffFuses(const vector<UINT32>& iffRows, UINT32 iffCrc)
{
    if (!iffRows.empty())
    {
        return m_HwAccessors[FuseMacroType::Fuse]->SetSwIffFuses(iffRows, iffCrc);
    }
    return RC::OK;
}

RC NewFuse::VerifySwIffFusing(const vector<UINT32>& iffRows, UINT32 iffCrc)
{
    if (!iffRows.empty())
    {
        return m_HwAccessors[FuseMacroType::Fuse]->VerifySwIffFusing(iffRows,  iffCrc);
    }
    return RC::OK;
}

//==============================================================================
// FUSE REWORKS

//------------------------------------------------------------------------------
// Run the fusing rework binary to burn the fuses
RC NewFuse::RunFuseReworkBinary(const string &sku, const FuseValues& request)
{
    RC rc;
    StickyRC stickyRc;

    GpuSubdevice *pSubdev = dynamic_cast<GpuSubdevice*>(m_pDev);
    if (!pSubdev)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    // Get the list of floorsweeping fuses to be supplied to the binary
    vector<ReworkFFRecord> fsFuses;
    for (const auto& nameValPair : request.m_NamedValues)
    {
        UINT32 lwrrVal;
        INT32  attribute;
        const string& name   = nameValPair.first;
        const UINT32& newVal = nameValPair.second;
        CHECK_RC(m_pEncoder->GetFuseValue(name, FuseFilter::ALL_FUSES, &lwrrVal));
        CHECK_RC(m_pSkuHandler->GetFuseAttribute(sku, name, &attribute));

        // Add only floorsweeping fuses which were marked as tracking (and have been overridden)
        if (m_pEncoder->IsFsFuse(name) && ((attribute & FuseUtil::FLAG_DONTCARE) != 0) &&
            newVal != lwrrVal)
        {
            CHECK_RC(m_pEncoder->AddReworkFuseInfo(name, newVal, fsFuses));
        }
    }

    unique_ptr<FalconImpl> pReworkFalcon;
    if (pSubdev->GetDeviceId() >= Device::GA102)
    {
        pReworkFalcon = make_unique<GA10xFalcon>(pSubdev, GA10xFalcon::FalconType::SEC);
    }
    else
    {
        pReworkFalcon = make_unique<GM20xFalcon>(pSubdev, GM20xFalcon::FalconType::SEC);
    }
    CHECK_RC(pReworkFalcon->Reset());

    // Load the EMEM with FS fuse information
    vector<UINT32> ememInfo;
    ememInfo.push_back(static_cast<UINT32>(fsFuses.size()));
    for (const ReworkFFRecord &ffRecord : fsFuses)
    {
        ememInfo.push_back(ffRecord.loc.ChainId);
        ememInfo.push_back(ffRecord.loc.Lsb);
        ememInfo.push_back(ffRecord.loc.Msb);
        ememInfo.push_back(ffRecord.val);
    }
    pReworkFalcon->WriteEMem(ememInfo, 0);

    // Pre ucode exelwtion setup
    RegHal &regs = pSubdev->Regs();
    // Put other falcons in reset
    CHECK_RC(pSubdev->HaltAndExitPmuPreOs());
    regs.Write32(MODS_PPWR_FALCON_ENGINE_RESET_TRUE);
    regs.Write32(MODS_PGSP_FALCON_ENGINE_RESET_TRUE);

    // Setup fusing
    CHECK_RC(m_HwAccessors[FuseMacroType::Fuse]->SetupFuseBlow());
    // Setup the progress mailbox register to an arbitrary value
    CHECK_RC(pReworkFalcon->WriteMailbox(0, 0xAB));
    // Start the exelwtion
    CHECK_RC(pReworkFalcon->LoadBinary(m_ReworkBinaryFilename,
                                       FalconUCode::UCodeDescType::GFW_Vx));
    CHECK_RC(pReworkFalcon->Start(false));

    stickyRc = pReworkFalcon->WaitForHalt(5000); // 5000ms (number is arbitrarily chosen)
    // Cleanup fusing setup
    stickyRc = m_HwAccessors[FuseMacroType::Fuse]->CleanupFuseBlow();

    // Mailbox 0 and 1 hold the progress status and error code respectively
    static constexpr UINT32 REWORK_PROGRESS_COMPLETE = 0x0000000F;
    UINT32 progress;
    UINT32 errCode;
    stickyRc = pReworkFalcon->ReadMailbox(0, &progress);
    stickyRc = pReworkFalcon->ReadMailbox(1, &errCode);
    if (errCode  != 0 ||
        progress != REWORK_PROGRESS_COMPLETE)
    {
        Printf(Tee::PriError, "Fuseblow failed. Progress Status = 0x%x, ErrorCode = 0x%x\n",
                               progress, errCode);
        stickyRc = RC::LWRM_FLCN_ERROR;
    }

    // Reset GPU to a clean state
    stickyRc = pSubdev->HotReset();
    m_pEncoder->MarkDecodeDirty();
    MarkFuseReadDirty();

    return stickyRc;
}

//------------------------------------------------------------------------------
// Get information required to colwert srcSku to destSku
RC NewFuse::GetSkuReworkInfo
(
    const string& srcSku,
    const string &destSku,
    const string &fileName
)
{
    RC rc;
    FuseValues srcFuses;
    FuseValues destFuses;

    m_pEncoder->IgnoreLwrrFuseVals(true);
    CHECK_RC(m_pSkuHandler->GetDesiredFuseValues(srcSku,  false, false, &srcFuses));
    CHECK_RC(m_pSkuHandler->AddDebugFuseOverrides(destSku));
    CHECK_RC(m_pSkuHandler->GetDesiredFuseValues(destSku, false, false, &destFuses));
    m_pEncoder->IgnoreLwrrFuseVals(false);

    ReworkRequest reworkVals;
    // Fetch the values of DEVIDs for source SKU
    const string devidAFuse = "OPT_PCIE_DEVIDA";
    const string devidBFuse = "OPT_PCIE_DEVIDB";
    if (srcFuses.m_NamedValues.find(devidAFuse) == srcFuses.m_NamedValues.end() ||
        srcFuses.m_NamedValues.find(devidBFuse) == srcFuses.m_NamedValues.end())
    {
        Printf(Tee::PriError, "Values for source DEVIDs not found\n");
        return RC::SOFTWARE_ERROR;
    }
    reworkVals.srcDevidA = srcFuses.m_NamedValues[devidAFuse];
    reworkVals.srcDevidB = srcFuses.m_NamedValues[devidBFuse];
    CHECK_RC(m_pEncoder->EncodeReworkFuseValues(srcFuses, destFuses, &reworkVals));
    CHECK_RC(PrintReworkInfoToFile(reworkVals, fileName));

    return rc;
}

//------------------------------------------------------------------------------
// Store rework information to a file
RC NewFuse::PrintReworkInfoToFile(const ReworkRequest &reworkVals, const string &fileName)
{
    RC rc;

    string request;

    const size_t numFuseRows = reworkVals.staticFuseRows.size();
    const size_t numFFRecords = reworkVals.newFFRecords.size();
    const size_t numFsRecords = reworkVals.fsFFRecords.size();
    const size_t numFpfRows = reworkVals.fpfRows.size();
    const size_t numRirRows = reworkVals.rirRows.size();

    request += Utility::StrPrintf("#define SOURCE_DEVIDA 0x%x\n", reworkVals.srcDevidA);
    request += Utility::StrPrintf("#define SOURCE_DEVIDB 0x%x\n", reworkVals.srcDevidB);
    request += Utility::StrPrintf("#define NUM_FUSE_ROWS %lu\n", numFuseRows);
    request += Utility::StrPrintf("#define FUSELESS_START_ROW %u\n",
                                   reworkVals.fuselessRowStart);
    request += Utility::StrPrintf("#define FUSELESS_END_ROW %u\n",
                                   reworkVals.fuselessRowEnd);
    request += Utility::StrPrintf("#define NUM_FF_RECORDS %lu\n", numFFRecords);

    request += Utility::StrPrintf("#define NUM_FS_RECORDS %lu\n", numFsRecords);
    if (reworkVals.hasFpfMacro)
    {
        request += "#define PROGRAM_FPF_MACRO\n";
        request += Utility::StrPrintf("#define NUM_FPF_ROWS %lu\n", numFpfRows);
    }
    if (reworkVals.supportsRir && reworkVals.requiresRir)
    {
        request += "#define PROGRAM_RIR_MACRO\n";
        request += Utility::StrPrintf("#define NUM_RIR_ROWS %lu\n", numRirRows);
    }

    request += "\n#ifdef NEEDS_REWORK_INFO\n\n";

    request += "LW_STATIC LwU32 FUSE_DIFF[NUM_FUSE_ROWS];\n";
    request += "LW_STATIC ffInfo FF_INFO[NUM_FF_RECORDS];\n";
    request += "LW_STATIC ffInfo FS_FF_INFO[NUM_FS_RECORDS];\n";
    if (reworkVals.hasFpfMacro)
    {
        request += "LW_STATIC LwU32 FPF_DIFF[NUM_FPF_ROWS];\n";
    }
    if (reworkVals.supportsRir && reworkVals.requiresRir)
    {
        request += "LW_STATIC LwU32 RIR_ROWS[NUM_RIR_ROWS];\n";
    }
    request += "\nstatic void SetupReworkInfo(void)\n{\n";

    // Fuse macro
    request += "\t// Static fuse section\n";
    for (size_t i = 0; i < numFuseRows; i++)
    {
        request += Utility::StrPrintf("\tFUSE_DIFF[%lu] = 0x%x;\n",
                                       i, reworkVals.staticFuseRows[i]);
    }

    // Fuseless fuse records
    request += "\n\t// FF info\n";
    for (size_t i = 0; i < numFFRecords; i++)
    {
        const auto &loc = reworkVals.newFFRecords[i].loc;
        request += Utility::StrPrintf("\tFF_INFO[%lu].chainId = 0x%x;\n", i, loc.ChainId);
        request += Utility::StrPrintf("\tFF_INFO[%lu].lsb     = 0x%x;\n", i, loc.Lsb);
        request += Utility::StrPrintf("\tFF_INFO[%lu].msb     = 0x%x;\n", i, loc.Msb);
        request += Utility::StrPrintf("\tFF_INFO[%lu].val     = 0x%x;\n",
                                       i, reworkVals.newFFRecords[i].val);
    }

    // Floorsweeping fuseless fuse records
    request += "\n\t// FS FF info\n";
    for (size_t i = 0; i < numFsRecords; i++)
    {
        const auto &loc = reworkVals.fsFFRecords[i].loc;
        request += Utility::StrPrintf("\tFS_FF_INFO[%lu].chainId = 0x%x;\n", i, loc.ChainId);
        request += Utility::StrPrintf("\tFS_FF_INFO[%lu].lsb     = 0x%x;\n", i, loc.Lsb);
        request += Utility::StrPrintf("\tFS_FF_INFO[%lu].msb     = 0x%x;\n", i, loc.Msb);
        request += Utility::StrPrintf("\tFS_FF_INFO[%lu].val     = 0xFFFFFFFF;\n", i);
    }

    // FPF records
    if (reworkVals.hasFpfMacro)
    {
        request += "\n\t// FPF static section\n";
        for (size_t i = 0; i < numFpfRows; i++)
        {
            request += Utility::StrPrintf("\tFPF_DIFF[%lu] = 0x%x;\n",
                                           i, reworkVals.fpfRows[i]);
        }
    }

    // RIR records
    if (reworkVals.supportsRir && reworkVals.requiresRir)
    {
        request += "\n\t// RIR section\n";
        for (size_t i = 0; i < numRirRows; i++)
        {
            request += Utility::StrPrintf("\tRIR_ROWS[%lu] = 0x%x;\n",
                                           i, reworkVals.rirRows[i]);
        }
    }

    request += "}\n\n#endif // NEEDS_REWORK_INFO\n";

    Xp::InteractiveFile reworkFile;
    CHECK_RC(reworkFile.Open(fileName.c_str(), Xp::InteractiveFile::Create));
    CHECK_RC(reworkFile.Write(request));
    reworkFile.Close();
    Printf(Tee::PriNormal, "Wrote contents to file %s\n", fileName.c_str());

    return rc;
}

//==============================================================================
// Printing functions

//------------------------------------------------------------------------------
// Print the decoded fuse values and IFF records
RC NewFuse::PrintFuses(bool optFusesOnly)
{
    RC rc;
    FuseValues fuseVals;
    FuseFilter filter = FuseFilter::ALL_FUSES;
    if (optFusesOnly)
    {
        filter |= FuseFilter::OPT_FUSES_ONLY;
    }
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        filter |= FuseFilter::LWSTOMER_FUSES_ONLY;
    }
    CHECK_RC(m_pEncoder->GetFuseValues(&fuseVals, filter));
    ErrorMap::ScopedDevInst setDevInst(m_pDev->DevInst());
    for (const auto& fuseValPair : fuseVals.m_NamedValues)
    {
        string fuseValstr = Utility::StrPrintf("%s : 0x%x",
            fuseValPair.first.c_str(), fuseValPair.second);

        Printf(Tee::PriNormal, "  Fuse %s\n", fuseValstr.c_str());
        // TODO Deduce Gpu Id dynamically. Lwrrently MLA supports only single gpu.
        // So Gpu ID is always zero.
        Mle::Print(Mle::Entry::tagged_str)
            .tag("Fuse.Val")
            .key(fuseValstr)
            .value("0");
    }

    return OK;
}

//------------------------------------------------------------------------------
// Print the raw fuse values from the macros (FUSE and FPF macros only)
RC NewFuse::PrintRawFuses()
{
    RC rc;
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    for (auto& macroAccPair : m_HwAccessors)
    {
        auto& macro = macroAccPair.first;
        auto& pAccessor = macroAccPair.second;
        // Only print the main macros
        if (macro != FuseMacroType::Fuse &&
            macro != FuseMacroType::Fpf)
        {
            continue;
        }
        Printf(FuseUtils::GetPrintPriority(), "Fuse rows for %s macro:\n", fuseMacroString[macro].c_str());

        const vector<UINT32>* pFuseRows = nullptr;
        CHECK_RC(pAccessor->GetFuseBlock(&pFuseRows));
        for (UINT32 i = 0; i < pFuseRows->size(); i++)
        {
            Printf(FuseUtils::GetPrintPriority(), "%4d: 0x%08x\n", i, (*pFuseRows)[i]);
        }
        Printf(FuseUtils::GetPrintPriority(), "\n");
    }
    return rc;
}

//------------------------------------------------------------------------------
// Decode the IFF records on the chip and print them in human-readable format
RC NewFuse::DecodeIff()
{
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        return RC::OK;
    }

    RC rc;
    FuseValues fuseVals;
    FuseFilter filter = FuseFilter::ALL_FUSES;
    CHECK_RC(m_pEncoder->GetFuseValues(&fuseVals, filter));
    CHECK_RC(PrintDecodedIffRecords(fuseVals.m_IffRecords));
    return rc;
}

//------------------------------------------------------------------------------
// Decode the IFF records for a SKU and print them in human-readable format
RC NewFuse::DecodeIff(const string& sku)
{
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        return RC::OK;
    }

    RC rc;
    vector<shared_ptr<IffRecord>> iffRecords;
    CHECK_RC(m_pSkuHandler->GetIffRecords(sku, &iffRecords));
    CHECK_RC(PrintDecodedIffRecords(iffRecords));
    return rc;
}

//------------------------------------------------------------------------------
// Decode the given fuse values and print IFF records in human-readable format
RC NewFuse::PrintDecodedIffRecords(const vector<shared_ptr<IffRecord>> &iffRecords)
{
    RC rc;
    bool isValid;
    UINT32 numCmds = 0;
    if (!iffRecords.empty())
    {
        Printf(Tee::PriNormal, "IFF commands:\n");
        for (const auto &record : iffRecords)
        {
            string decodedIff;
            isValid = true;
            CHECK_RC(m_pEncoder->DecodeIffRecord(record, decodedIff, &isValid));
            if (isValid)
            {
                Printf(Tee::PriNormal, " %2d - %s\n", numCmds++, decodedIff.c_str());
            }
        }
    }

    if (numCmds == 0)
    {
        Printf(Tee::PriNormal, "No valid IFF records found\n");
    }

    return rc;
}

//==============================================================================
// Forwarding functions

//------------------------------------------------------------------------------
// Compares the current fusing against all the SKUs for the current
// chip and returns a list of the SKUs which the chip matched.
// pMatchingSkus: a vector by which to return the matching SKU names
// filter: a FuseFilter enum value for selecting only certain fuses e.g. only
//         only reading customer visible fuses
RC NewFuse::FindMatchingSkus(vector<string>* pMatchingSkus, FuseFilter filter)
{
    return m_pSkuHandler->FindMatchingSkus(pMatchingSkus, filter);
}

//------------------------------------------------------------------------------
// Compares the current fusing to the given SKU
// sku: the name of the SKU to match
// pDoesMatch: pointer to a bool for the result of the query
RC NewFuse::DoesSkuMatch
(
    const string& sku,
    bool* pDoesMatch,
    FuseFilter filter,
    Tee::Priority pri
)
{
    return m_pSkuHandler->DoesSkuMatch(sku, pDoesMatch, filter, pri);
}

//------------------------------------------------------------------------------
RC NewFuse::SetDownbinInfo(Downbin::DownbinInfo downbinInfo)
{
    MASSERT(m_pDownbin);
    return m_pDownbin->SetDownbinInfo(downbinInfo);
}

//------------------------------------------------------------------------------
// Compares the current SW fusing to the given SKU
// sku: the name of the SKU to match
// returns a RC::SW_FUSING_ERROR if the SKU doesn't match.
RC NewFuse::CheckSwSku(string skuName, FuseFilter filter)
{
    RC rc;

    bool doesMatch = false;
    FuseFilter matchFilter = filter | FuseFilter::OPT_FUSES_ONLY;
    CHECK_RC(m_pSkuHandler->DoesSkuMatch(skuName, &doesMatch,
                                         matchFilter, FuseUtils::GetPrintPriority()));

    if (doesMatch)
    {
        return RC::OK;
    }
    return RC::SW_FUSING_ERROR;
}

//------------------------------------------------------------------------------
// Compares the current fusing for ATE fuses to the given SKU
// sku: the name of the SKU to match
// returns a RC::FUSE_VALUE_OUT_OF_RANGE if the ATE fuses don't match.
RC NewFuse::CheckAteFuses(string skuName)
{
    return m_pSkuHandler->CheckAteFuses(skuName);
}

//------------------------------------------------------------------------------
// Set an override for a given fuse
// fuseName: the name of the fuse given in Fuse.JSON (case insensitive)
// fuseOverride: the new value the fuse should have and a flag to indicate if
//               the override is treated as a starting point and the SKU handler
//               is still supposed to determine a value based on the SKU
RC NewFuse::SetOverride(const string& fuseName, FuseOverride fuseOverride)
{
    return m_pSkuHandler->OverrideFuseVal(fuseName, fuseOverride);
}

//------------------------------------------------------------------------------
// Set multiple fuse overrides
// \see: NewFuse::SetOverride
RC NewFuse::SetOverrides(const map<string, FuseOverride>& overrides)
{
    RC rc;
    for (const auto& nameValPair : overrides)
    {
        CHECK_RC(m_pSkuHandler->OverrideFuseVal(nameValPair.first, nameValPair.second));
    }
    return rc;
}

//------------------------------------------------------------------------------
// Returns the value of a given fuse
RC NewFuse::GetFuseValue(const string& fuseName, FuseFilter filter, UINT32* pVal)
{
    return m_pEncoder->GetFuseValue(fuseName, filter, pVal);
}

//------------------------------------------------------------------------------
// Compares the chip's current fusing to a given sku and prints out those fuses
// which mismatch and why.
RC NewFuse::PrintMismatchingFuses(const string& chipSku, FuseFilter filter)
{
    return m_pSkuHandler->PrintMismatchingFuses(chipSku, filter);
}

//------------------------------------------------------------------------------
// Compares the chip's current fusing to a given sku and returns a list of
// those fuses which mismatch and why.
RC NewFuse::GetMismatchingFuses
(
    const string& chipSku,
    FuseFilter filter,
    FuseDiff* pRetInfo
)
{
    return m_pSkuHandler->GetMismatchingFuses(chipSku, filter, pRetInfo);
}

//------------------------------------------------------------------------------
// Marks all fuse reads and decodes dirty so they'll have to be read fresh
// instead of using cached values. (used during fusing and the FuseSenseTest)
void NewFuse::MarkFuseReadDirty()
{
    for (const auto& macroAccessorPair : m_HwAccessors)
    {
        macroAccessorPair.second->MarkFuseReadDirty();
    }

    // TODO: this should happen automatically with FuseHwAccess::MarkFuseReadDirty
    m_pEncoder->MarkDecodeDirty();
}

//------------------------------------------------------------------------------
// Disables waiting for the fuse hardware to report idle before continuing with
// certain operations (only used on fmodel to speed things up)
void NewFuse::DisableWaitForIdle()
{
    for (const auto& macroAccessorPair : m_HwAccessors)
    {
        macroAccessorPair.second->DisableWaitForIdle();
    }
}

//------------------------------------------------------------------------------
// Tells the SKU handler to use SLT Test Values if provided when
// burning or comparing SKUs
void NewFuse::UseTestValues()
{
    m_pSkuHandler->UseTestValues();
}

//------------------------------------------------------------------------------
// Tells the SKU handler to set if reconfig fuses should be checked
// when comparing SKUs
void NewFuse::CheckReconfigFuses(bool check)
{
    m_pSkuHandler->CheckReconfigFuses(check);
}

//------------------------------------------------------------------------------
// Tells if reconfig fuses should be checked when comparing SKUs
bool NewFuse::IsReconfigFusesCheckEnabled()
{
    return m_pSkuHandler->IsReconfigFusesCheckEnabled();
}

//------------------------------------------------------------------------------
// Gets whether or not we can use the undo bit of an undo fuse to change the
// fuse value from 1 to 0
bool NewFuse::GetUseUndoFuse()
{
    return m_pEncoder->GetUseUndoFuse();
}

//------------------------------------------------------------------------------
// Sets whether or not we can use the undo bit of an undo fuse to change the
// fuse value from 1 to 0
void NewFuse::SetUseUndoFuse(bool useUndo)
{
    m_pEncoder->SetUseUndoFuse(useUndo);
}

//------------------------------------------------------------------------------
// Sets whether RIR can be used to repair bits from 1->0 for a fuse, if needed
RC NewFuse::RequestRirOnFuse(const string& fuseName)
{
    return m_pEncoder->SetUseRirOnFuse(fuseName);
}

//------------------------------------------------------------------------------
// Sets whether RIR records for this fuse can be disabled, if needed
RC NewFuse::RequestDisableRirOnFuse(const string& fuseName)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// Adds a fuse to a list of floorsweeping fusenames
void NewFuse::AppendFsFuseList(const string& fsFuse)
{
    m_pEncoder->AppendFsFuseList(fsFuse);
}

//------------------------------------------------------------------------------
// Adds a fuse to the list of fuses which should be ignored for SKU matching,
// decoding, etc. These are un-ignored by calling ClearIgnoreList
void NewFuse::AppendIgnoreList(string fuseToIgnore)
{
    m_pEncoder->TempIgnoreFuse(fuseToIgnore);
}

//------------------------------------------------------------------------------
// Stops ignoring fuses specified via AppendIgnoreList
void NewFuse::ClearIgnoreList()
{
    m_pEncoder->ClearTempIgnoreList();
}

//------------------------------------------------------------------------------
// Ignore mismatches between HW fuses and OPT register values
void NewFuse::SetIgnoreRawOptMismatch(bool ignore)
{
    m_pEncoder->SetIgnoreRawOptMismatch(ignore);
}

//------------------------------------------------------------------------------
// Checks whether or not SW fusing is still allowed on this chip
bool NewFuse::IsSwFusingAllowed()
{
    bool rtn = false;
    for (const auto& macroAccessorPair : m_HwAccessors)
    {
        rtn = rtn || macroAccessorPair.second->IsSwFusingAllowed();
    }
    return rtn;
}

//------------------------------------------------------------------------------
// Triggers a SW Reset to re-read the OPT fuses
RC NewFuse::SwReset()
{
    RC rc;
    for (const auto& macroAccessorPair : m_HwAccessors)
    {
        CHECK_RC(macroAccessorPair.second->SwReset());
    }
    return rc;
}

//------------------------------------------------------------------------------
// Checks whether or not priv security has been enabled for the fuse registers
bool NewFuse::IsFusePrivSelwrityEnabled()
{
    bool rtn = false;
    for (const auto& macroAccessorPair : m_HwAccessors)
    {
        rtn = rtn || macroAccessorPair.second->IsFusePrivSelwrityEnabled();
    }
    return rtn;
}

//------------------------------------------------------------------------------
// Returns the name of the base fuse file for this chip (e.g. gv100_f.json)
string NewFuse::GetFuseFilename()
{
    return FuseUtils::GetFuseFilename(m_pDev->GetDeviceId());
}

//------------------------------------------------------------------------------
RC NewFuse::GetCachedFuseReg(vector<UINT32>* pFuseCache)
{
    MASSERT(pFuseCache);
    RC rc;
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    for (auto& macroAccPair : m_HwAccessors)
    {
        auto& macro = macroAccPair.first;
        auto& pAccessor = macroAccPair.second;
        // Only cache the required macro
        if (macro != FuseMacroType::Fuse)
        {
            continue;
        }
        const vector<UINT32>* pFuseRows = nullptr;
        CHECK_RC(pAccessor->GetFuseBlock(&pFuseRows));
        *pFuseCache = *pFuseRows;
    }
    return rc;
}

//--------------------------------------------------------------------------
// Set core voltage on device
RC NewFuse::SetCoreVoltage(UINT32 vddMv)
{
    MASSERT(m_HwAccessors[FuseMacroType::Fuse]);
    return m_HwAccessors[FuseMacroType::Fuse]->SetFusingVoltage(vddMv);
}

//--------------------------------------------------------------------------
// Set fuse blow duration
RC NewFuse::SetFuseBlowTime(UINT32 blowTimeNs)
{
    if (blowTimeNs == 0)
    {
        // Leave at default value
        Printf(FuseUtils::GetPrintPriority(),
               "BlowTimeNs requested is 0, leaving fuse blow duration at default value\n");
        return RC::OK;
    }

    MASSERT(m_HwAccessors[FuseMacroType::Fuse]);
    const UINT32 fuseClockPeriodNs = m_HwAccessors[FuseMacroType::Fuse]->GetFuseClockPeriodNs();

    if (fuseClockPeriodNs == 0)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    auto& regs = m_pDev->Regs();
    UINT32 fuseTime = regs.Read32(MODS_FUSE_FUSETIME_PGM2);

    Printf(FuseUtils::GetPrintPriority(), "LW_FUSE_FUSETIME_PGM2 was 0x%x\n", fuseTime);

    UINT32 ticks = blowTimeNs / fuseClockPeriodNs;
    regs.SetField(&fuseTime, MODS_FUSE_FUSETIME_PGM2_TWIDTH_PGM, ticks);
    regs.Write32(MODS_FUSE_FUSETIME_PGM2, fuseTime);

    Printf(FuseUtils::GetPrintPriority(), "LW_FUSE_FUSETIME_PGM2 set to 0x%x\n", fuseTime);

    return RC::OK;
}

//--------------------------------------------------------------------------
// Set RIR record ilwalidation while blowing fuses
void NewFuse::SetNumValidRir(UINT32 numValidRecords)
{
    m_NumValidRir = numValidRecords;
}

//--------------------------------------------------------------------------
// Set value the poison fuse bit should be overriden to by RIR
void NewFuse::SetOverridePoisonRir(UINT32 overrideVal)
{
    m_OverridePoisonRir = overrideVal;
}

//------------------------------------------------------------------------------
// Handle RIR record ilwalidation in the fuse binary map
RC NewFuse::IlwalidateRirRecords(FuseBinaryMap* pFuseBinary, UINT32 numValidRir)
{
    RC rc;

    UINT32 numRows;
    if (m_HwAccessors.count(FuseMacroType::Rir) == 0)
    {
        Printf(FuseUtils::GetErrorPriority(), "RIR macro not available for ilwalidation\n");
        return RC::BAD_PARAMETER;
    }
    CHECK_RC(m_HwAccessors[FuseMacroType::Rir]->GetNumFuseRows(&numRows));

    const UINT32 numRecordsPerRow = 2;
    const UINT32 maxRecords       = numRows * numRecordsPerRow;
    if (numValidRir > maxRecords)
    {
        Printf(FuseUtils::GetErrorPriority(),
               "Number of enabled RIR records requested (%u) invalid, Max available = %u\n",
                numValidRir, maxRecords);
        return RC::BAD_PARAMETER;
    }

    vector<UINT32> rows(numRows, 0);
    const UINT32 ilwalidRecord      = 0xFFFF;
    const UINT32 numRirToIlwalidate = maxRecords - numValidRir;
    for (UINT32 i = 0; i < numRirToIlwalidate; i++)
    {
        UINT32 shift = (i % 2) ? 16 : 0;
        rows[i / numRecordsPerRow] |= (ilwalidRecord << shift);
    }

    // Set poison RIR if applicable
    if (m_OverridePoisonRir != ~0U)
    {
        // This condition should only be true for GA100
        // (The record format below is based on this assumption)
        if (!m_pDev->HasFeature(Device::GPUSUB_ALLOWS_POISON_RIR))
        {
            Printf(FuseUtils::GetErrorPriority(),
                   "Poison override RIR not supported on this chip\n");
            return RC::BAD_PARAMETER;
        }

        // Value can be either 1 or 0
        if (m_OverridePoisonRir > 1)
        {
            Printf(FuseUtils::GetErrorPriority(),
                   "Invalid value (%u) provided for poison override\n",
                    m_OverridePoisonRir);
            return RC::BAD_PARAMETER;
        }

        // On GA100, opt_global_poison_dis is at 18->9:8
        // RIR record format for TSMC 7nm :
        //   15-2: Addr[13-0] = 584 (= 18*32 + 8)
        //   1:    Data       = m_OverridePoisonRir
        //   0:    Enable     = 1
        const UINT32 poisonRecord = 0x0921 | (m_OverridePoisonRir << 1);
        rows[numRows - 1]         = (rows[numRows - 1] & 0xFFFF0000) | poisonRecord;
    }

    (*pFuseBinary)[FuseMacroType::Rir] = rows;
    return rc;
}

// Pull information from the individual FS fuses and from the
// Floorsweeping_options section (if present) and merge it into
// a single format for use by the down-bin script.
// This function has been copied from FuseUtil::GetFsInfo which
// is for legacy support
// This has some modifications for filtering fuses wrt gpu and
// removing pseudo_tpc_disable.
RC NewFuse::GetFsInfo
(
    const string &fileName,
    const string &chipSku,
    map<string, DownbinConfig>* pConfigs
)
{
    return m_pSkuHandler->GetFsInfo(fileName, chipSku, pConfigs);
}

void NewFuse::SetNumSpareFFRows(const UINT32 numSpareFFRows)
{
    m_pEncoder->SetNumSpareFFRows(numSpareFFRows);
}

RC NewFuse::SetReworkBinaryFilename(const string& filename)
{
    m_ReworkBinaryFilename = filename;
    return RC::OK;
}

RC NewFuse::DumpFsInfo(const string &chipSku)
{
    return m_pSkuHandler->DumpFsInfo(chipSku);
}

RC NewFuse::FuseOnlyFs(bool isSwFusing, const string& fsFusingOpt)
{
    RC rc;
    FuseValues request;
    if (m_pDownbin)
    {
        CHECK_RC(m_pDownbin->GetOnlyFsFuseInfo(&request, fsFusingOpt));
    }
    else
    {
        Printf(Tee::PriError, "DownbinImpl Object not exist\n");
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(m_pEncoder->RemoveUnsupportedFuses(&request));

    if (!isSwFusing)
    {
        CHECK_RC(BlowFuses(request, ALL_RIR_VALID));
    }
    else
    {
        CHECK_RC(SetSwFuses(request));
    }
    return rc;
}

RC NewFuse::GetFuseConfig(const string &chipSku, map<string, FuseUtil::DownbinConfig>* pFuseConfig)
{
    return m_pSkuHandler->GetFuseConfig(chipSku, pFuseConfig);
}

/* virtual */ RC NewFuse::GetFusesInfo(
    bool optFusesOnly, map<string,
    UINT32>& fuseNameValPairs
)
{
    RC rc;

    FuseValues fuseNamedVals;
    FuseFilter filter = FuseFilter::ALL_FUSES;
    if (optFusesOnly)
    {
        filter |= FuseFilter::OPT_FUSES_ONLY;
    }
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        filter |= FuseFilter::LWSTOMER_FUSES_ONLY;
    }
    CHECK_RC(m_pEncoder->GetFuseValues(&fuseNamedVals, filter));
    fuseNameValPairs = fuseNamedVals.m_NamedValues;

    return rc;
}
//--------------------------------------------------------------------------
RC NewFuse::DumpFsScriptInfo()
{
    MASSERT(m_pDownbin);
    RC rc;
    if (m_pDownbin)
    {
        CHECK_RC(m_pDownbin->DumpFsScriptInfo());
    }
    else
    {
        Printf(Tee::PriError, "DownbinImpl Object not exist\n");
        return RC::SOFTWARE_ERROR;
    }
    return rc;
}
