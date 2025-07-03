/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/floorsweepimpl.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/xp.h"
#include "core/include/platform.h"
#include <algorithm> // for std::replace

//------------------------------------------------------------------------------
/*virtual*/ FloorsweepImpl::FloorsweepImpl( GpuSubdevice *pSubdev) :
    m_pSub(pSubdev),
    m_FloorsweepingAffected(false),
    m_AppliedFloorsweepingChanges(false)
{
    MASSERT(pSubdev);
    FS_ADD_NAMED_PROPERTY(FbEnableMask, UINT32, 0);
}

//------------------------------------------------------------------------------
// Should this go into the FloorsweepImpl as a common method?
// We could call it GetFsArgs() and retrieve the FermiFsArgs property. The
// "Fermi" prefix was an oversight when it was originally created, its now used
// on all Fermi plus GPUs to store the floorsweep arguments.
string FloorsweepImpl::GetFermiFsArgs()
{
    GpuSubdevice::NamedProperty *pProp;
    string FermiFsArgs;
    if ((OK != m_pSub->GetNamedProperty("FermiFsArgs", &pProp)) ||
        (OK != pProp->Get(&FermiFsArgs)))
    {
        Printf(Tee::PriDebug, "FermiFsArgs is not valid!!\n");
    }
    return FermiFsArgs;
}

//------------------------------------------------------------------------------
RC FloorsweepImpl::PrepareArgDatabase(string FsArgs, ArgDatabase* fsArgDatabase)
{

    // -fermi_fs feature strings use ":" as a separator.  Replace all ":" with
    // spaces so that the argReader can parse the arg string
    //
    std::replace(FsArgs.begin(), FsArgs.end(), ':', ' ');

    // Need to break up the FsArgs string into a vector of strings
    // using a space as the tokenizer.  This could probably be made
    // into a utility function
    //
    vector<string> FsArgVector;
    string::size_type copyStart = FsArgs.find_first_not_of(' ');
    string::size_type copyEnd = FsArgs.find_first_of(' ', copyStart);
    while (copyStart != copyEnd)
    {
        if (copyEnd == string::npos)
        {
            FsArgVector.push_back(FsArgs.substr(copyStart));
            copyStart = copyEnd;
        }
        else
        {
            FsArgVector.push_back(FsArgs.substr(copyStart, copyEnd - copyStart));
            copyStart = FsArgs.find_first_not_of(' ', copyEnd);
            copyEnd = FsArgs.find_first_of(' ', copyStart);
        }
    }

    ArgPreProcessor fsArgPreProc;

    // Create the database
    //
    if (OK != fsArgPreProc.PreprocessArgs(&FsArgVector))
    {
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    if (OK != fsArgPreProc.AddArgsToDatabase(fsArgDatabase))
    {
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    return OK;
}

//-------------------------------------------------------------------------------
/* virtual */ RC FloorsweepImpl::GetFloorsweepingMasks
(
    FloorsweepingMasks* pFsMasks
) const
{
    MASSERT(pFsMasks != 0);
    pFsMasks->clear();

    pFsMasks->insert(pair<string, UINT32>("tpc_mask", TpcMask()));

    pFsMasks->insert(pair<string, UINT32>("sm_mask", SmMask()));

    pFsMasks->insert(pair<string, UINT32>("fb_mask", FbMaskImpl()));

    return OK;
}

//------------------------------------------------------------------------------
Tee::Priority FloorsweepImpl::GetFloorsweepingPrintPriority() const
{
    Tee::Priority pri = Tee::PriLow;

    if ((Xp::GetOperatingSystem() == Xp::OS_LINUXSIM) ||
        (Xp::GetOperatingSystem() == Xp::OS_WINSIM))
    {
        pri =Tee::PriNormal;
    }

    return pri;
}

//------------------------------------------------------------------------------
// floorsweeping contains register access protocols that cause non-RTL
// and non-HW pre-Pascal models severe indigestion on, don't attempt HW floorsweeping
// on the SW models
//
/*virtual*/ bool FloorsweepImpl::IsFloorsweepingAllowed() const
{
    Platform::SimulationMode simMode = Platform::GetSimulationMode();
    if ((simMode != Platform::Amodel) && (simMode != Platform::Fmodel))
    {
        return true;
    }
    return false;
}
//------------------------------------------------------------------------------
/*virtual*/ RC FloorsweepImpl::ApplyFloorsweepingChanges(bool InPreVBIOSSetup)
{
    Printf(Tee::PriError, "ApplyFloorsweepingChanges not implemented for this GPU!");
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/* virtual */ RC FloorsweepImpl::PreserveFloorsweepingSettings() const
{
    Printf(Tee::PriError, "PreserveFloorsweepingArguments not implemented for this GPU!");
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/* virtual */ RC FloorsweepImpl::PrintPreservedFloorsweeping() const
{
    Printf(Tee::PriLow, "PrintPreservedFloorsweeping is not implemented for this GPU\n");
    return RC::OK;
}

//------------------------------------------------------------------------------
/*virtual*/ RC   FloorsweepImpl::ReadFloorsweepingArguments()
{
    Printf(Tee::PriError, "ReadFloorsweepingArguments not implemented for this GPU!");
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/*virtual*/ RC   FloorsweepImpl::AdjustDependentFloorsweepingArguments()
{
    Printf(Tee::PriError, "AdjustDependentFloorsweepingArguments not implemented for this GPU!");
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/*virtual*/ RC   FloorsweepImpl::AdjustGpcFloorsweepingArguments()
{
    Printf(Tee::PriError, "AdjustGpcFloorsweepingArguments not implemented for this GPU!");
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/*virtual*/ RC   FloorsweepImpl::AdjustFbpFloorsweepingArguments()
{
    Printf(Tee::PriError, "AdjustFbpFloorsweepingArguments not implemented for this GPU!");
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/*virtual*/ RC   FloorsweepImpl::AdjustL2NocFloorsweepingArguments()
{
    Printf(Tee::PriError, "AdjustL2NocFloorsweepingArguments not implemented for this GPU!");
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/*virtual*/ RC   FloorsweepImpl::AddLwrFsMask()
{
    Printf(Tee::PriError, "AddLwrFsMask not implemented for this GPU!");
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/*virtual*/ void FloorsweepImpl::ApplyFloorsweepingSettings()
{
    MASSERT(!"ApplyFloorsweepingSettings not implemented for this GPU!");
}

//------------------------------------------------------------------------------
/*virtual*/ RC FloorsweepImpl::CheckFloorsweepingArguments()
{
    Printf(Tee::PriError, "CheckFloorsweepingArguments not implemented for this GPU!");
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/*virtual*/ void FloorsweepImpl::PrintFloorsweepingParams()
{
    MASSERT(!"PrintFloorsweepingParams not implemented for this GPU!");
}

//------------------------------------------------------------------------------
/*virtual*/ void FloorsweepImpl::RestoreFloorsweepingSettings()
{
    MASSERT(!"RestoreFloorsweepingSettings not implemented for this GPU!");
}

//------------------------------------------------------------------------------
/*virtual*/ void FloorsweepImpl::ResetFloorsweepingSettings()
{
    MASSERT(!"ResetFloorsweepingSettings not implemented for this GPU!");
}

//------------------------------------------------------------------------------
/*virtual*/ void FloorsweepImpl::SaveFloorsweepingSettings()
{
    MASSERT(!"SaveFloorsweepingSettings not implemented for this GPU!");
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 FloorsweepImpl::FscontrolMask() const
{
    MASSERT(!"FscontrolMask not valid for the GPU architecture\n");
    return 0;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 FloorsweepImpl::FbioMaskImpl() const
{
    MASSERT(!"FbioMask not valid for the GPU architecture\n");
    return 0;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 FloorsweepImpl::FbMaskImpl() const
{
    MASSERT(!"FbMask not valid for the GPU architecture\n");
    return 0;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 FloorsweepImpl::FbpMaskImpl() const
{
    MASSERT(!"FbpMask not valid for the GPU architecture\n");
    return 0;
}

//------------------------------------------------------------------------------
UINT32 FloorsweepImpl::SyspipeMask() const
{
    return 0;
}

UINT32 FloorsweepImpl::SyspipeMaskDisable() const
{
    return ~SyspipeMask() & ((1 << m_pSub->GetMaxSyspipeCount()) - 1);
}

//------------------------------------------------------------------------------
UINT32 FloorsweepImpl::OfaMask() const
{
    return 0;
}

UINT32 FloorsweepImpl::OfaMaskDisable() const
{
     return ~OfaMask() & ((1 << m_pSub->GetMaxOfaCount()) - 1);
}

//------------------------------------------------------------------------------
UINT32 FloorsweepImpl::LwjpgMask() const
{
    return 0;
}

UINT32 FloorsweepImpl::LwjpgMaskDisable() const
{
    return ~LwjpgMask() & ((1 << m_pSub->GetMaxLwjpgCount()) - 1);
}

//------------------------------------------------------------------------------
UINT32 FloorsweepImpl::LwLinkMask() const
{
    return 0;
}

//------------------------------------------------------------------------------
UINT32 FloorsweepImpl::PceMask() const
{
    return 0;
}

//------------------------------------------------------------------------------
UINT32 FloorsweepImpl::L2MaskImpl(UINT32 HwFbpNum) const
{
    return 0;
}

//------------------------------------------------------------------------------
UINT32 FloorsweepImpl::L2MaskImpl() const
{
    return L2MaskImpl(~0U);
}

//------------------------------------------------------------------------------
bool FloorsweepImpl::SupportsL2SliceMask(UINT32 hwFbpNum) const
{
    return false;
}

//------------------------------------------------------------------------------
UINT32 FloorsweepImpl::L2SliceMask(UINT32 HwFbpNum) const
{
    return 0;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 FloorsweepImpl::GpcMask() const
{
    MASSERT(!"GpcMask not valid for the GPU architecture\n");
    return 0;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 FloorsweepImpl::GfxGpcMask() const
{
    MASSERT(!"GpcMask not valid for the GPU architecture\n");
    return 0;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 FloorsweepImpl::LwdecMask() const
{
    MASSERT(!"LwdecMask not valid for the GPU architecture\n");
    return 0;
}

/* virtual */ UINT32 FloorsweepImpl::LwdecMaskDisable() const
{
    return ~LwdecMask() & ((1 << m_pSub->GetMaxLwdecCount()) - 1);
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 FloorsweepImpl::LwencMask() const
{
    MASSERT(!"LwencMask not valid for the GPU architecture\n");
    return 0;
}

/* virtual */ UINT32 FloorsweepImpl::LwencMaskDisable() const
{
    return ~LwencMask() & ((1 << m_pSub->GetMaxLwencCount()) - 1);
}

//------------------------------------------------------------------------------
/* virtual */ RC FloorsweepImpl::PrintMiscFuseRegs()
{
    Printf(Tee::PriNormal, "PrintMiscFuseRegs not valid for the GPU architecture\n");
    return RC::SOFTWARE_ERROR;
}

//------------------------------------------------------------------------------
/*virtual*/ UINT32 FloorsweepImpl::GetFbEnableMask() const
{
    GpuSubdevice::NamedProperty *pFbEnableMaskProp;
    UINT32         FbEnableMask = 0;
    if ((OK != m_pSub->GetNamedProperty("FbEnableMask", &pFbEnableMaskProp)) ||
        (OK != pFbEnableMaskProp->Get(&FbEnableMask)))
    {
        Printf(Tee::PriDebug, "FbEnableMask is not valid!!\n");
    }
    return FbEnableMask;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 FloorsweepImpl::GetGpcEnableMask() const
{
    MASSERT(!"GetGpcEnableMask not valid for the GPU architecture\n");
    return 0;
}

//------------------------------------------------------------------------------
/*virtual*/ UINT32 FloorsweepImpl::GetTpcEnableMask() const
{
    MASSERT(!"GetTpcEnableMask not valid for the GPU architecture\n");
    return 0;
}

//------------------------------------------------------------------------------
/*virtual*/ UINT32 FloorsweepImpl::GetSmEnableMask() const
{
    GpuSubdevice::NamedProperty *pSmEnableMaskProp;
    UINT32         SmEnableMask = 0;
    if ((OK != m_pSub->GetNamedProperty("SmEnableMask", &pSmEnableMaskProp)) ||
        (OK != pSmEnableMaskProp->Get(&SmEnableMask)))
    {
        Printf(Tee::PriDebug, "SmEnableMask is not valid!!\n");
    }
    return SmEnableMask;
}

//------------------------------------------------------------------------------
/*virtual*/ UINT32 FloorsweepImpl::GetMevpEnableMask() const
{
    GpuSubdevice::NamedProperty *pMevpEnableMaskProp;
    UINT32         MevpEnableMask = 0;
    if ((OK != m_pSub->GetNamedProperty("MevpEnableMask", &pMevpEnableMaskProp)) ||
        (OK != pMevpEnableMaskProp->Get(&MevpEnableMask)))
    {
        Printf(Tee::PriDebug, "MevpEnableMask is not valid!!\n");
    }
    return MevpEnableMask;
}

//------------------------------------------------------------------------------
/*virtual*/ void FloorsweepImpl::GetTpcEnableMaskOnGpc(vector<UINT32> *pValues) const
{
    MASSERT(!"GetTpcEnableMaskOnGpc not valid for the GPU architecture\n");
}

//------------------------------------------------------------------------------
/*virtual*/ void FloorsweepImpl::GetZlwllEnableMaskOnGpc(vector<UINT32> *pValues) const
{
    MASSERT(!"GetZlwllEnableMaskOnGpc not valid for the GPU architecture\n");
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 FloorsweepImpl::FbpaMaskImpl() const
{
    return 0;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 FloorsweepImpl::FbioshiftMaskImpl() const
{
    return 0;
}

//------------------------------------------------------------------------------
UINT32 FloorsweepImpl::TpcMask(UINT32 HwGpcNum) const
{
    return 0;
}

//------------------------------------------------------------------------------
UINT32 FloorsweepImpl::PesOnGpcStride() const
{
    // WAR - Bug 2064455: Some chips have 2 PES/GPC, but the PES disable fuse
    // has a 3 PES stride due to RTL reuse. This means we need to read 3 PES
    // for each GPC to align bits correctly.
    if (m_pSub->HasBug(2064455))
    {
        return 3;
    }
    return m_pSub->GetMaxPesCount();
}

//------------------------------------------------------------------------------
UINT32 FloorsweepImpl::PesMask(UINT32 HwGpcNum) const
{
    return 0;
}

//------------------------------------------------------------------------------
UINT32 FloorsweepImpl::GetHalfFbpaWidth() const
{
    return 0;
}

//------------------------------------------------------------------------------
UINT32 FloorsweepImpl::HalfFbpaMask() const
{
    return 0;
}

//------------------------------------------------------------------------------
bool FloorsweepImpl::SupportsRopMask(UINT32 hwGpcNum) const
{
    return false;
}

//------------------------------------------------------------------------------
UINT32 FloorsweepImpl::RopMask(UINT32 HwGpcNum) const
{
    return 0;
}

//------------------------------------------------------------------------------
UINT32 FloorsweepImpl::TpcMask() const
{
    return TpcMask(~0U);
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 FloorsweepImpl::ZlwllMask(UINT32 HwGpcNum) const
{
    return 0;
}

//------------------------------------------------------------------------------
UINT32 FloorsweepImpl::MevpMask() const
{
    return 0;
}

//------------------------------------------------------------------------------
UINT32 FloorsweepImpl::SmMask(UINT32 HwGpcNum, UINT32 HwTpcNum) const
{
    return 0;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 FloorsweepImpl::SpareMask() const
{
    return 0;
}

//------------------------------------------------------------------------------
UINT32 FloorsweepImpl::XpMask() const
{
    return 0;
}

//------------------------------------------------------------------------------
bool FloorsweepImpl::SupportsCpcMask(UINT32 hwGpcNum) const
{
    return false;
}

//------------------------------------------------------------------------------
UINT32 FloorsweepImpl::CpcMask(UINT32 HwGpcNum) const
{
    return 0;
}

//------------------------------------------------------------------------------
UINT32 FloorsweepImpl::LwLinkLinkMask() const
{
    if (m_pSub->Regs().IsSupported(MODS_SCAL_LITTER_NUM_LWLINK))
    {
        return (1U << m_pSub->Regs().LookupAddress(MODS_SCAL_LITTER_NUM_LWLINK)) - 1;
    }
    return 0;
}

/* virtual */ UINT32 FloorsweepImpl::LwLinkLinkMaskDisable() const
{
    return ~0U;
}

//------------------------------------------------------------------------------
UINT32 FloorsweepImpl::C2CMask() const
{
    return 0;
}

/* virtual */ UINT32 FloorsweepImpl::C2CMaskDisable() const
{
    return ~C2CMask();
}

//------------------------------------------------------------------------------
bool FloorsweepImpl::GetRestoreFloorsweeping() const
{
    GpuSubdevice::NamedProperty *pNoFsRestoreProp;
    bool           NoFsRestore = false;
    if ((OK != m_pSub->GetNamedProperty("NoFsRestore", &pNoFsRestoreProp)) ||
        (OK != pNoFsRestoreProp->Get(&NoFsRestore)))
    {
        Printf(Tee::PriDebug, "NoFsRestore is not valid!!\n");
    }
    return !NoFsRestore;
}

//------------------------------------------------------------------------------
bool FloorsweepImpl::GetResetFloorsweeping() const
{
    GpuSubdevice::NamedProperty *pFsResetProp;
    bool fsReset = false;
    if ((OK != m_pSub->GetNamedProperty("FsReset", &pFsResetProp)) ||
        (OK != pFsResetProp->Get(&fsReset)))
    {
        Printf(Tee::PriDebug, "FsReset is not valid!!\n");
    }
    return fsReset;
}

//------------------------------------------------------------------------------
bool FloorsweepImpl::GetAdjustFloorsweepingArgs() const
{
    GpuSubdevice::NamedProperty *pAdjustFsArgsProp;
    bool adjustFs = false;
    if ((OK != m_pSub->GetNamedProperty("AdjustFsArgs", &pAdjustFsArgsProp)) ||
        (OK != pAdjustFsArgsProp->Get(&adjustFs)))
    {
        Printf(Tee::PriDebug, "AdjustFsArgs is not valid!!\n");
    }
    return adjustFs;
}

//------------------------------------------------------------------------------
bool FloorsweepImpl::GetAddLwrFsMask() const
{
    GpuSubdevice::NamedProperty *pAddLwrFsMaskProp;
    bool addLwrFsMask = false;
    if ((RC::OK != m_pSub->GetNamedProperty("AddLwrFsMask", &pAddLwrFsMaskProp)) ||
        (RC::OK != pAddLwrFsMaskProp->Get(&addLwrFsMask)))
    {
        Printf(Tee::PriDebug, "AddLwrFsMask is not valid!!\n");
    }
    return addLwrFsMask;
}

//------------------------------------------------------------------------------
bool FloorsweepImpl::GetAdjustL2NocFloorsweepingArgs() const
{
    GpuSubdevice::NamedProperty *pAdjustFsL2NocProp;
    bool adjustFsL2Noc = false;
    if ((OK != m_pSub->GetNamedProperty("AdjustFsL2Noc", &pAdjustFsL2NocProp)) ||
        (OK != pAdjustFsL2NocProp->Get(&adjustFsL2Noc)))
    {
        Printf(Tee::PriDebug, "AdjustFsL2Noc is not valid!!\n");
    }
    return adjustFsL2Noc;
}

//------------------------------------------------------------------------------
void FloorsweepImpl::Reset()
{
    m_AppliedFloorsweepingChanges = false;
}

//------------------------------------------------------------------------------
/*virtual*/ bool FloorsweepImpl::GetFloorsweepingAffected() const
{
    return m_FloorsweepingAffected;
}

//------------------------------------------------------------------------------
void FloorsweepImpl::SetFloorsweepingAffected(bool affected)
{
    m_FloorsweepingAffected = affected;
}

//------------------------------------------------------------------------------
void FloorsweepImpl::SetFloorsweepingChangesApplied(bool bApplied)
{
    m_AppliedFloorsweepingChanges = bApplied;
}

//------------------------------------------------------------------------------
/*virtual*/ bool FloorsweepImpl::GetFloorsweepingChangesApplied() const
{
    return m_AppliedFloorsweepingChanges;
}

//------------------------------------------------------------------------------
/*virtual*/ bool FloorsweepImpl::IsFbBroken() const
{
    return m_pSub->IsFbBroken();
}

/*virtual*/ RC FloorsweepImpl::SwReset(bool bReinitPriRing)
{
    Printf(Tee::PriWarn, "SW Reset is unsupported on this Gpu\n");
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/*virtual*/RC FloorsweepImpl::FixFloorsweepingInitMasks(bool *pFsAffected)
{
    MASSERT(!"FixFloorsweepingInitMasks not implemented for this GPU!");
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/*virtual*/bool FloorsweepImpl::GetFixFloorsweepingInitMasks()
{
    GpuSubdevice::NamedProperty *pFixFsInitMasksProp;
    bool fixFsInitMasks = false;
    if ((m_pSub->GetNamedProperty("FixFsInitMasks", &pFixFsInitMasksProp) != RC::OK) ||
        (pFixFsInitMasksProp->Get(&fixFsInitMasks) != RC::OK))
    {
        Printf(Tee::PriDebug, "FixFsInitMasks is not valid!!\n");
    }
    return fixFsInitMasks;
}

/*  Sample JSON floorsweep generated (sweep.json)
    [{
        "name": "Final",
        "time_end": "2012-06-13 15:07:13.197055", 
        "result": {
            "OPT_FBP_DISABLE": "0x0",
            "OPT_TPC_DISABLE": "['0x0', '0x0', ..., '0x0']",
            "OPT_GPC_DISABLE": "0x0",
            "OPT_FBIO_DISABLE": "0x0",
            "OPT_ROP_L2_DISABLE": "['0x0', '0x0', ..., '0x0]",
            ...
            "Exit Code": 0
        },
        "time_begin": "2012-06-13 15:07:13.196775"
    }] 
*/
RC FloorsweepImpl::GenerateSweepJson() const
{
    RC rc = RC::OK;
      
    JsonItem jsiResult;
    PopulateGpcDisableMasks(&jsiResult);
    PopulateFbpDisableMasks(&jsiResult);
    PopulateMiscDisableMasks(&jsiResult);
    jsiResult.SetField("Exit Code", rc);

    string jsonStr;
    JsonItem jsi;
    jsi.SetLwrDateTimeField("time_begin");
    jsi.SetField("result", &jsiResult);
    jsi.SetLwrDateTimeField("time_end");
    jsi.Stringify(&jsonStr);

    string modsRunspacePath = Xp::GetElw("MODS_RUNSPACE");
    string sweepJsonFilePath = modsRunspacePath.empty() ? "sweep.json" 
        : modsRunspacePath + "/sweep.json";
    FileHolder fileHolder;
    CHECK_RC(fileHolder.Open(sweepJsonFilePath.c_str(), "w+"));
    FILE *fpSweepJson = fileHolder.GetFile();
    fprintf(fpSweepJson, "[{\n");
    fprintf(fpSweepJson, "\t\"name\" : \"Final\"\n");
    fprintf(fpSweepJson, "%s", jsonStr.c_str());
    fprintf(fpSweepJson, "}]\n");

    Printf(Tee::PriNormal, 
        "Generating and dumping the final command line floorsweeping options to %s.\n", 
        sweepJsonFilePath.c_str());
    return rc;
}
