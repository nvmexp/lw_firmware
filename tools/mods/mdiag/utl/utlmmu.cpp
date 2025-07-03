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

#include "core/include/utility.h"
#include "utlmmu.h"
#include "utlutil.h"
#include "utlchannel.h"
#include "mdiag/sysspec.h"
#include "utlsurface.h"
#include "gpu/include/gpudev.h"
#include "mdiag/advschd/policymn.h"
#include "mdiag/utils/mmuutils.h"

void UtlMmu::Register(py::module module)
{
    auto mmu = module.def_submodule("Mmu");
    py::enum_<SUB_LEVEL_INDEX>(mmu, "SUB_LEVEL_INDEX")
        .value("BIG", SUB_LEVEL_INDEX::BIG)
        .value("SMALL", SUB_LEVEL_INDEX::SMALL);

    py::enum_<APERTURE>(mmu, "APERTURE")
        .value("FB", APERTURE::APERTURE_FB)
        .value("COHERENT", APERTURE::APERTURE_COHERENT)
        .value("NONCOHERENT", APERTURE::APERTURE_NONCOHERENT)
        .value("PEER", APERTURE::APERTURE_PEER)
        .value("INVALID", APERTURE::APERTURE_ILWALID);

    py::enum_<LEVEL>(mmu, "LEVEL")
        .value("PTE_4K", LEVEL::LEVEL_PTE_4K)
        .value("PTE_64K", LEVEL::LEVEL_PTE_64K)
        .value("PTE_2M", LEVEL::LEVEL_PTE_2M)
        .value("PDE0", LEVEL::LEVEL_PDE0)
        .value("PTE_512M", LEVEL::LEVEL_PTE_512M)
        .value("PDE1", LEVEL::LEVEL_PDE1)
        .value("PDE2", LEVEL::LEVEL_PDE2)
        .value("PDE3", LEVEL::LEVEL_PDE3)
        .value("PDE4", LEVEL::LEVEL_PDE4);
    
    py::enum_<FIELD>(mmu, "FIELD")
        .value("ReadOnly", FIELD::ReadOnly)
        .value("AtomicDisable", FIELD::AtomicDisable)
        .value("Volatile", FIELD::Volatile)
        .value("Privilege", FIELD::Privilege)
        .value("Valid", FIELD::Valid)
        .value("Acd", FIELD::Acd)
        .value("Sparse", FIELD::Sparse);
    
    py::enum_<PCF_SW> pcf(mmu, "PCF");
    UtlMmu::RegisterPcf(pcf);

    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("MmuEntry.SetAttributes", "readOnly", false, "this entry set readonly or not");
    kwargs->RegisterKwarg("MmuEntry.SetAttributes", "atomicDisable", false, "this entry set atomicDisable or not");
    kwargs->RegisterKwarg("MmuEntry.SetAttributes", "volatile", false, "this entry set volatile or not");
    kwargs->RegisterKwarg("MmuEntry.SetAttributes", "privileged", false, "this entry set privileged or not");
    kwargs->RegisterKwarg("MmuEntry.SetAttributes", "valid", false, "this entry set valid or not");
    kwargs->RegisterKwarg("MmuEntry.SetAttributes", "sparse", false, "this entry set sparse or not");
    kwargs->RegisterKwarg("MmuEntry.SetAttributes", "kind", false, "this entry set pte kind");
    kwargs->RegisterKwarg("MmuEntry.SetAttributes", "aperture", false, "this entry set aperture");
    kwargs->RegisterKwarg("MmuEntry.SetAttributes", "physAddress", false, "this entry set physical address");
    kwargs->RegisterKwarg("MmuEntry.SetAttributes", "pcf", false, "this entry set PTE PCF or PDE PCF and only valid after v3 MMU");
    kwargs->RegisterKwarg("MmuEntry.SetAttributes", "targetLevel", false, "this entry change to other page size(level)");
    kwargs->RegisterKwarg("MmuEntry.WriteMmu", "inbandChannel", false, "update the hw mmu via this channel in band way");
    kwargs->RegisterKwarg("MmuEntry.WriteMmu", "subchNum", false, "update the hw mmu via this sub channel number in band way");
    kwargs->RegisterKwarg("MmuEntry.FieldGetBool", "field", true, "get the bool from given field value");

    py::class_<UtlMmuEntry> entry(module, "MmuEntry");
    entry.def("GetData", &UtlMmuEntry::GetData,
            "get raw data");
    entry.def("SetAttributes", &UtlMmuEntry::SetAttributes,
        UTL_DOCSTRING("MmuEntry.SetAttributes", "Modify this PTE entry attributes"));
    entry.def("WriteMmu", &UtlMmuEntry::WriteMmuEntryPy,
        UTL_DOCSTRING("MmuEntry.WriteMmu", "Update the local PTE information into the HW via inband way or out of band way"));
    entry.def("GetAperture", &UtlMmuEntry::FieldGetAperturePy,
            "Get the aperture information in this entry");
    entry.def("GetPhysAddr", &UtlMmuEntry::FieldGetPhysAddr,
            "Get the physical address information in this entry");
    entry.def("GetBool", &UtlMmuEntry::FieldGetBoolPy,
        UTL_DOCSTRING("MmuEntry.FieldGetBool", "Get the bool information in this entry"));
    entry.def("GetPcf", &UtlMmuEntry::FieldGetPcfPy,
            "Get the PCF information in this entry");
    entry.def("GetKind", &UtlMmuEntry::FieldGetKind,
            "Get the PTE kind information in this entry");
}

void UtlMmu::RegisterPcf(py::enum_<PCF_SW> mmuPcf)
{
    for (const auto & pcf : s_PcfName)
    {
        mmuPcf.value(pcf.second.c_str(), pcf.first);
    }
}

Memory::Location UtlMmu::ColwertMmuApertureToMemoryLocation
(
    UtlMmu::APERTURE mmuAperture
)
{
    Memory::Location memoryLocation = Memory::Optimal;
    switch (mmuAperture)
    {
        case UtlMmu::APERTURE::APERTURE_FB:
            memoryLocation = Memory::Fb;
            break;
        case UtlMmu::APERTURE::APERTURE_COHERENT:
            memoryLocation = Memory::Coherent;
            break;
        case UtlMmu::APERTURE::APERTURE_NONCOHERENT:
            memoryLocation = Memory::NonCoherent;
            break;
        case UtlMmu::APERTURE::APERTURE_ILWALID:
            memoryLocation = Memory::Optimal;
            break;
        default:
            MASSERT(!"Invalid mmuAperture passed to ColwertMmuApertureToMemoryLocation");
    }

    return memoryLocation;
}

/******************************************************
 * UtlMmuSegment
 * 1. Internal object and don't be explore to the API
 * 2. Warp of the MmuLevelSegment
 * 3. Own some specified function which don't shared with MmuLevelSegment
 *
 * ****************************************************/
UtlMmuSegment::UtlMmuSegment
(
    UtlSurface * pSurface,
    MmuLevelSegment * pSegment,
    SUB_LEVEL_INDEX subLevelIdx
) :
    m_pSurface(pSurface),
    m_pSegment(pSegment),
    m_SubLevelIdx(subLevelIdx)
{
    MmuLevel * pMmuLevel = m_pSegment->GetMmuLevel();
    m_LevelIndex = static_cast<MmuLevelTree::LevelIndex>(pMmuLevel->GetLevelId());
}

void UtlMmuSegment::WriteMmuEntries
(
    UINT32 offset,
    UINT64 *size,
    Channel * pChannel,
    UINT32 subchNum
)
{
    m_pSegment->WriteMmuEntries(offset, size, pChannel, subchNum);
}

vector<UtlMmuEntry * > UtlMmuSegment::GetEntryLists
(
    UINT64 offset,
    UINT64 * size
)
{
    MmuLevelSegmentInfo segmentInfo;
    RC rc = GetMmuLevelSegmentInfo(&segmentInfo);
    UtlUtility::PyErrorRC(rc, "Get MmuLevel segment information error.");

    UINT64 realSize =
        min(segmentInfo.surfOffset + segmentInfo.surfSegmentSize - offset,
                *size);
    UINT64 levelPageSize = segmentInfo.levelPageSize;
    // entry start index in this segment
    UINT32 entryIndex = static_cast<UINT32>
        ((offset - segmentInfo.surfOffset) / levelPageSize);
    // entry number
    UINT64 pageCount = (realSize + levelPageSize - 1) / levelPageSize;
    // Update the real size
    *size = realSize;

    vector<UtlMmuEntry * > entryLists;

    for (UINT32 i = 0; i < pageCount; ++i)
    {
        UINT64 offset = (entryIndex + i) * segmentInfo.levelPageSize + segmentInfo.surfOffset;
        UtlMmuEntry * pEntry = GetEntry(offset);
        entryLists.push_back(pEntry);
    }

    // Mods side has this ownership
    return entryLists;
}

UtlMmuEntry * UtlMmuSegment::GetEntry
(
    UINT64 offset
)
{
    if (m_UtlEntryLists.find(offset) == m_UtlEntryLists.end())
    {
        m_UtlEntryLists[offset] = unique_ptr<UtlMmuEntry>(new UtlMmuEntry(offset, this));
    }

    return m_UtlEntryLists[offset].get();
}

/******************************************************
 * UtlMmuEntry
 * 1. unit object of MMU operation 
 *
 * ****************************************************/
UtlMmuEntry::UtlMmuEntry
(
    UINT64 offset,
    UtlMmuSegment * pSegment
) :
    m_Offset(offset),
    m_pSegment(pSegment)
{
    MASSERT(pSegment);
    MmuLevelSegmentInfo segmentInfo;
    pSegment->GetMmuLevelSegmentInfo(&segmentInfo);
    m_Size = segmentInfo.entrySize;

    InitEntry();
}

UtlMmuEntry::~UtlMmuEntry()
{
    vector<UINT08> data = GetData();

    EntryOps entryOps;

    if (m_OrignalMmuInfo.PCF != FieldGetPcf())
    {
        entryOps.MmuEntryOps |= EntryOps::MmuEntryOpTypes::MMUENTRY_OP_SET_PCF;
    }

    if (m_OrignalMmuInfo.valid != FieldGetBool(FIELD::Valid))
    {
        entryOps.MmuEntryOps |= EntryOps::MmuEntryOpTypes::MMUENTRY_OP_SET_VALID;
    }

    if (m_OrignalMmuInfo.physAddr != FieldGetPhysAddr())
    {
        entryOps.MmuEntryOps |= EntryOps::MmuEntryOpTypes::MMUENTRY_OP_SET_PA;
    }

    if (m_OrignalMmuInfo.aperture != FieldGetAperture())
    {
        entryOps.MmuEntryOps |= EntryOps::MmuEntryOpTypes::MMUENTRY_OP_SET_APERTURE;
    }

    if (entryOps.MmuEntryOps)
    {
        // Current entry is different layer as original
        // Ilwalidate current entry before restoring
        // orignal entry.
        MmuLevelTree * pMmuLevelTree = GetMmuLevelTree();
        if (pMmuLevelTree->IsChangedPageSize())
        {
            FieldSetBool(FIELD::Valid, false);
        }

        if (entryOps.MmuEntryOps & EntryOps::MmuEntryOpTypes::MMUENTRY_OP_SET_PA
                || entryOps.MmuEntryOps & EntryOps::MmuEntryOpTypes::MMUENTRY_OP_SET_APERTURE)
        {
            if (m_OrignalMmuInfo.pageSize != pMmuLevelTree->GetActivePageSize())
            {
                pMmuLevelTree->SetActivePageSize(m_OrignalMmuInfo.pageSize);
            }

            // For the page size change, PTE_2m which aperture need to be updated when updated to PDE0
            // since different aperture which value is different in PDE and PTE_2m
            FieldSetPhysAddr(m_OrignalMmuInfo.physAddr);
            FieldSetAperture(m_OrignalMmuInfo.aperture);
        }
        
        if (entryOps.MmuEntryOps & EntryOps::MmuEntryOpTypes::MMUENTRY_OP_SET_PCF)
        {
            FieldSetPcf(m_OrignalMmuInfo.PCF);
        }
        
        if (entryOps.MmuEntryOps & EntryOps::MmuEntryOpTypes::MMUENTRY_OP_SET_VALID)
        {
            FieldSetBool(FIELD::Valid, m_OrignalMmuInfo.valid);
        }

        if (pMmuLevelTree->IsChangedPageSize() && m_OrignalMmuInfo.valid == true)
        {
            // recovery it back
            FieldSetBool(FIELD::Valid, m_OrignalMmuInfo.valid);
        }

        WriteMmuEntry(nullptr);
    }
}

void UtlMmuEntry::InitEntry()
{
    m_OrignalMmuInfo.valid = FieldGetBool(FIELD::Valid);
    m_OrignalMmuInfo.aperture = FieldGetAperture();
    m_OrignalMmuInfo.physAddr = FieldGetPhysAddr();
    m_OrignalMmuInfo.PCF = FieldGetPcf();
    
    MmuLevelTree * pMmuLevelTree = GetMmuLevelTree();
    m_OrignalMmuInfo.pageSize = pMmuLevelTree->GetActivePageSize();
}

void UtlMmuEntry::SetAttributes(py::kwargs kwargs)
{
    bool readOnly = false;
    bool atomicDisable = false;
    bool volatileBit = false;
    bool privileged = false;
    bool valid = false;
    UINT32 kind= ~0x0;
    APERTURE aperture = APERTURE::APERTURE_ILWALID;
    UINT64 physAddress = ~0x0;
    PCF_SW pcf = PCF_SW::PCF_MAX_COUNT;
    LEVEL targetLevel;
    bool isSet = false;
    map<MmuLevelSegment::PtePcfField, bool> pteFields;
    map<MmuLevelSegment::PdePcfField, bool> pdeFields;
    MmuLevelSegment * pSegment = m_pSegment->GetRawSegment();
    RC rc;

    PolicyManager * pPolicyMgr = PolicyManager::Instance();
    if (pPolicyMgr->HasUpdateSurfaceInMmu(m_pSegment->GetSurface()->GetName()))
    {
        UtlUtility::PyError("Updated MMU in the Policy Manager and UTL side. It will cause uncertain behavior.");
    }

    Utl::Instance()->SetUpdateSurfaceInMmu(m_pSegment->GetSurface()->GetName());

    PrintMmuInfo("Before change mmu:");

    if (UtlUtility::GetOptionalKwarg<bool>(&valid, kwargs, "valid", "MmuEntry.SetAttributes"))
    {
        // Reset PCF only if the valid is changed from current value.
        if (FieldGetBool(FIELD::Valid) != valid)
        {           
            Printf(Tee::PriNormal, "Resetting the PCF due to change of valid bit. \n");
            ResetPcf(valid);
            isSet = true;
        }
    }

    if (UtlUtility::GetOptionalKwarg<bool>(&readOnly, kwargs, "readOnly", "MmuEntry.SetAttributes"))
    {
        pteFields[MmuLevelSegment::PtePcfField::ReadOnly] = readOnly;
        isSet = true;
    }
    if (UtlUtility::GetOptionalKwarg<bool>(&atomicDisable, kwargs, "atomicDisable", "MmuEntry.SetAttributes"))
    {
        pteFields[MmuLevelSegment::PtePcfField::AtomicDisable] = atomicDisable;
        isSet = true;
    }
    if (UtlUtility::GetOptionalKwarg<bool>(&volatileBit, kwargs, "volatile", "MmuEntry.SetAttributes"))
    {
        pteFields[MmuLevelSegment::PtePcfField::Volatile] = volatileBit;
        pdeFields[MmuLevelSegment::PdePcfField::Volatile] = volatileBit;
        isSet = true;
    }
    if (UtlUtility::GetOptionalKwarg<bool>(&privileged, kwargs, "privileged", "MmuEntry.SetAttributes"))
    {
        pteFields[MmuLevelSegment::PtePcfField::Privilege] = privileged;
        isSet = true;
    }
    rc = pSegment->SetSegmentPcf(m_Offset, &m_Size, m_pSegment->GetSubLevelIndex(),
        pteFields, pdeFields, MmuLevelSegment::FetchMode::FetchCache);
    UtlUtility::PyErrorRC(rc, "Error setting segment PCF.");

    if (UtlUtility::GetOptionalKwarg<bool>(&valid, kwargs, "valid", "MmuEntry.SetAttributes"))
    {
        FieldSetBool(FIELD::Valid, valid);
        isSet = true;
    }
    if (UtlUtility::GetOptionalKwarg<UINT32>(&kind, kwargs, "kind", "MmuEntry.SetAttributes"))
    {
        UtlGil::Release gil;
        // PTE kind modification differ from other bit which need to call the RM ctrl call
        // No need to call the entry.WriteMmu to flush data
        FieldSetKind(kind);
        isSet = true;
    }
    if (UtlUtility::GetOptionalKwarg<APERTURE>(&aperture, kwargs, "aperture", "MmuEntry.SetAttributes"))
    {
        // PTE and PDE has different branch
        FieldSetAperture(aperture);
        isSet = true;
    }
    if (UtlUtility::GetOptionalKwarg<UINT64>(&physAddress, kwargs, "physAddress", "MmuEntry.SetAttributes"))
    {
        FieldSetPhysAddr(physAddress);
        isSet = true;
    }
    if (UtlUtility::GetOptionalKwarg<PCF_SW>(&pcf, kwargs, "pcf", "MmuEntry.SetAttributes"))
    {
        FieldSetPcf(pcf);
        isSet = true;
    }
    if (UtlUtility::GetOptionalKwarg<LEVEL>(&targetLevel, kwargs, "targetLevel", "MmuEntry.SetAttributes"))
    {
        FieldChangePageSize(targetLevel);
        isSet = true;
    }

    PrintMmuInfo("After change mmu:");

    if (isSet == false)
    {
        UtlUtility::PyError("invalid argument. Need at least one valid argument.");
    }

}

void UtlMmuEntry::ResetPcf(bool valid)
{
    map<MmuLevelSegment::PtePcfField, bool> pteFields;
    map<MmuLevelSegment::PdePcfField, bool> pdeFields;
    MmuLevelSegment * pSegment = m_pSegment->GetRawSegment();
    RC rc;

    pteFields[MmuLevelSegment::PtePcfField::ReadOnly]       = false;
    pteFields[MmuLevelSegment::PtePcfField::AtomicDisable]  = false;
    pteFields[MmuLevelSegment::PtePcfField::Volatile]       = false;
    pteFields[MmuLevelSegment::PtePcfField::Privilege]      = false;
    pteFields[MmuLevelSegment::PtePcfField::Lw4K]           = false;
    pteFields[MmuLevelSegment::PtePcfField::Sparse]         = false;
    pteFields[MmuLevelSegment::PtePcfField::NoMapping]      = false;
    pteFields[MmuLevelSegment::PtePcfField::Invalid]        = false;

    pdeFields[MmuLevelSegment::PdePcfField::Invalid]        = false;
    pdeFields[MmuLevelSegment::PdePcfField::Volatile]       = false;
    pdeFields[MmuLevelSegment::PdePcfField::Sparse]         = false;
    pdeFields[MmuLevelSegment::PdePcfField::AtsAllow]       = false;

    if (valid)
    {
        // When switching to "true", set the valid first before resetting the flags.
        // Reason: The old combination of flags may not be legal with valid=false.
        FieldSetBool(FIELD::Valid, valid);
        rc = pSegment->SetSegmentPcf(m_Offset, &m_Size, m_pSegment->GetSubLevelIndex(),
                pteFields, pdeFields, MmuLevelSegment::FetchMode::FetchCache);        
    }
    else
    {
        // When switching to "false", reset all the flags before setting the valid to
        // "false".
        pteFields[MmuLevelSegment::PtePcfField::Invalid]        = true;
        pdeFields[MmuLevelSegment::PdePcfField::Invalid]        = true;

        rc = pSegment->SetSegmentPcf(m_Offset, &m_Size, m_pSegment->GetSubLevelIndex(),
                pteFields, pdeFields, MmuLevelSegment::FetchMode::FetchCache);
        FieldSetBool(FIELD::Valid, valid);        
    }

    UtlUtility::PyErrorRC(rc, "Error setting segment PCF in ResetPtePcf");
}

// This function return the entry data
vector<UINT08> UtlMmuEntry::GetData()
{
    RC rc;

    vector<UINT08> entryData;
    MmuLevelSegment * pSegment = m_pSegment->GetRawSegment();
    rc = pSegment->GetMmuEntriesData(m_Offset, &m_Size, &entryData);
    if (rc != OK)
    {
        MASSERT(!"Can't get the entry data.");
    }

    return entryData;
}

// This function will return the raw entry data ptr
UINT08 * UtlMmuEntry::GetDataPtr()
{
    UINT64 pageCount = 0;
    MmuLevelSegment * pSegment = m_pSegment->GetRawSegment();
    return pSegment->GetEntryLocalData(m_Offset, m_Size,
                                       &m_Size, &pageCount);
}

// Flush the local MMU data to the hw
void UtlMmuEntry::WriteMmuEntry
(    
    UtlChannel * pInbandChannel,
    UINT32 subchNum
)
{
    Channel * pRawChannel = nullptr;
    if (pInbandChannel)
    {
        LWGpuChannel * pChannel = pInbandChannel->GetLwGpuChannel();
        if (pChannel)
        {
            pRawChannel = pChannel->GetModsChannel();
        }
    }
    
    m_pSegment->WriteMmuEntries(m_Offset, &m_Size, pRawChannel, subchNum);  
}

void UtlMmuEntry::WriteMmuEntryPy(py::kwargs kwargs)
{
    UtlChannel * pInbandChannel = nullptr;
    UINT32 subchNum = 0;
    UtlUtility::GetOptionalKwarg<UtlChannel *>(&pInbandChannel, kwargs, "inbandChannel", "MmuEntry.WriteMmu");
    
    UtlUtility::GetOptionalKwarg<UINT32>(&subchNum, kwargs, "subchNum", "MmuEntry.WriteMmu");

    UtlGil::Release gil; 
    
    WriteMmuEntry(pInbandChannel, subchNum);  
}

bool UtlMmuEntry::IsPteEntry()
{
    MmuLevelSegment * pSegment = m_pSegment->GetRawSegment();
    return pSegment->IsPteEntry(GetDataPtr());
}

PCF_SW UtlMmuEntry::PcfHwToPcfSw(UINT32 pcfHw)
{
    UINT32 pcfSw = 0;

    if (IsPteEntry())
    {
        bool valid = FieldGetBool(FIELD::Valid);
        MmuUtils::GmmuTranslatePtePcfFromHw(pcfHw, valid, &pcfSw);
        return static_cast<PCF_SW>(pcfSw);
    }
    else
    {
        GMMU_APERTURE aperture = static_cast<GMMU_APERTURE>(FieldGetAperture());
        MmuUtils::GmmuTranslatePdePcfFromHw(pcfHw, aperture, &pcfSw);
        return static_cast<PCF_SW>(pcfSw + static_cast<UINT32>(PCF_SW::PTE_COUNT));
    }
}

UINT32 UtlMmuEntry::PcfSwToPcfHw(PCF_SW pcfSw)
{
    UINT32 pcfHw;
    UINT32 pcfSwAsInt;
    if (IsPteEntry())
    {
        pcfSwAsInt = static_cast<UINT32>(pcfSw);
        MmuUtils::GmmuTranslatePtePcfFromSw(pcfSwAsInt, &pcfHw);
    }
    else
    {
        pcfSwAsInt = static_cast<UINT32>(pcfSw) - static_cast<UINT32>(PCF_SW::PTE_COUNT);
        MmuUtils::GmmuTranslatePdePcfFromSw(pcfSwAsInt, &pcfHw);
    }

    return pcfHw;
}

string UtlMmuEntry::FieldGetPcfPy()
{
    auto pcfNameIt = s_PcfName.find(FieldGetPcf());
    if (pcfNameIt == s_PcfName.end())
    {
        MASSERT(!"Can't find the corresponding pcf value.");
    }
    return pcfNameIt->second;
}

PCF_SW UtlMmuEntry::FieldGetPcf()
{
    MmuLevelSegment * pSegment = m_pSegment->GetRawSegment();
    MmuLevel * pMmuLevel = pSegment->GetMmuLevel();
    MmuLevel::MmuSubLevelIndex subLevelIdx = m_pSegment->GetSubLevelIndex();

    if (IsPteEntry())
    {
        const GMMU_FMT_PTE* pFmtPte = pMmuLevel->GetGmmuFmt()->pPte;
        return PcfHwToPcfSw(lwFieldGet32(&pFmtPte->fldPtePcf, 
                    GetDataPtr()));
    }
    else
    {
        MmuLevelSegmentInfo segInfo;
        pSegment->GetMmuLevelSegmentInfo(&segInfo);
        const GMMU_FMT* pFmt = pMmuLevel->GetGmmuFmt();
        const GMMU_FMT_PDE* pFmtPde = gmmuFmtGetPde(pFmt,
                segInfo.pFmtLevel,
                subLevelIdx);
        return PcfHwToPcfSw(lwFieldGet32(&pFmtPde->fldPdePcf, 
                GetDataPtr()));
    }
}

void UtlMmuEntry::FieldSetPcf
(
    PCF_SW pcf
)
{
    MmuLevelSegment * pSegment = m_pSegment->GetRawSegment();
    MmuLevel * pMmuLevel = pSegment->GetMmuLevel();
    MmuLevel::MmuSubLevelIndex subLevelIdx = m_pSegment->GetSubLevelIndex();

    if (IsPteEntry())
    {
        const GMMU_FMT_PTE* pFmtPte = pMmuLevel->GetGmmuFmt()->pPte;
        lwFieldSet32(&pFmtPte->fldPtePcf, PcfSwToPcfHw(pcf), 
                GetDataPtr());
    }
    else
    {
        // PDE branch
        MmuLevelSegmentInfo segInfo;
        pSegment->GetMmuLevelSegmentInfo(&segInfo);
        const GMMU_FMT* pFmt = pMmuLevel->GetGmmuFmt();
        const GMMU_FMT_PDE* pFmtPde = gmmuFmtGetPde(pFmt,
                segInfo.pFmtLevel,
                subLevelIdx);
        lwFieldSet32(&pFmtPde->fldPdePcf, PcfSwToPcfHw(pcf), 
                GetDataPtr());
    }
}

void UtlMmuEntry::FieldSetPhysAddr
(
    UINT64 physAddr
)
{
    const GMMU_FIELD_ADDRESS *pAddrFld;
    MmuLevelSegment * pSegment = m_pSegment->GetRawSegment();
    MmuLevel * pMmuLevel = pSegment->GetMmuLevel();

    if (IsPteEntry())
    {
        const GMMU_FMT_PTE* pFmtPte = pMmuLevel->GetGmmuFmt()->pPte;
        GMMU_APERTURE aperture = gmmuFieldGetAperture(&pFmtPte->fldAperture, GetDataPtr());

        pAddrFld = gmmuFmtPtePhysAddrFld(pFmtPte, aperture);
    }
    else
    {
        MmuLevelSegmentInfo segmentInfo;
        m_pSegment->GetMmuLevelSegmentInfo(&segmentInfo);
        const GMMU_FMT_PDE* pPde = gmmuFmtGetPde(pMmuLevel->GetGmmuFmt(), 
                segmentInfo.pFmtLevel, m_pSegment->GetSubLevelIndex());
        GMMU_APERTURE aperture = gmmuFieldGetAperture(&pPde->fldAperture, GetDataPtr());

        pAddrFld = gmmuFmtPdePhysAddrFld(pPde, aperture);
    }
        
    if (pAddrFld)
    {
        gmmuFieldSetAddress(pAddrFld, physAddr, GetDataPtr());
    }

}

UINT32 UtlMmuEntry::FieldGetKind()
{
    UtlSurface * pUtlSurface = m_pSegment->GetSurface();
    MdiagSurf * pSurf = pUtlSurface->GetRawPtr();
    GpuDevice * pGpuDevice = pSurf->GetGpuDev();
    LwRm* pLwRm = pSurf->GetLwRmPtr();
    // Refer PmMemRange::GetGpuAddr(NULL, NULL)
    UINT64 gpuAddr = pSurf->GetCtxDmaOffsetGpu() + m_Offset;

    MmuLevelTreeManager * pMmuLevelMgr = MmuLevelTreeManager::Instance();
    LW0080_CTRL_DMA_SET_PTE_INFO_PARAMS setParam = {};
    pMmuLevelMgr->SetPteInfoStruct(pLwRm, gpuAddr, pGpuDevice, &setParam);

    for (UINT32 i = 0; i < LW0080_CTRL_DMA_SET_PTE_INFO_PTE_BLOCKS; ++i)
    {
        if (setParam.pteBlocks[i].pageSize != 0 &&
                FLD_TEST_DRF(0080_CTRL_DMA_PTE_INFO_PARAMS, _FLAGS, _VALID,
                    _TRUE, setParam.pteBlocks[i].pteFlags))
        {
            return setParam.pteBlocks[i].kind;
        }
    }

    MASSERT(!"Can't find corresponding kind.");
    return ~0x0;
}

void UtlMmuEntry::FieldSetKind(UINT32 kind)
{
    UtlSurface * pUtlSurface = m_pSegment->GetSurface();
    MdiagSurf * pSurf = pUtlSurface->GetRawPtr();
    GpuDevice * pGpuDevice = pSurf->GetGpuDev();
    LwRm* pLwRm = pSurf->GetLwRmPtr();
    // Refer PmMemRange::GetGpuAddr(NULL, NULL)
    //
    UINT64 gpuAddr = pSurf->GetCtxDmaOffsetGpu() + m_Offset;

    MmuLevelTreeManager * pMmuLevelMgr = MmuLevelTreeManager::Instance();
    LW0080_CTRL_DMA_SET_PTE_INFO_PARAMS setParam = {};
    pMmuLevelMgr->SetPteInfoStruct(pLwRm, gpuAddr, pGpuDevice, &setParam);
    
    for (UINT32 i = 0; i < LW0080_CTRL_DMA_SET_PTE_INFO_PTE_BLOCKS; ++i)
    {
        if (setParam.pteBlocks[i].pageSize != 0 &&
                FLD_TEST_DRF(0080_CTRL_DMA_PTE_INFO_PARAMS, _FLAGS, _VALID,
                    _TRUE, setParam.pteBlocks[i].pteFlags))
        {
            setParam.pteBlocks[i].kind = kind;
        }
    }

    RC rc = pLwRm->ControlByDevice(pGpuDevice,
                LW0080_CTRL_CMD_DMA_SET_PTE_INFO,
                &setParam, sizeof(setParam));
    if (rc != OK)
    {
        MASSERT(!"Can't set the Pte kind correctly.");
    }
}

UINT64 UtlMmuEntry::FieldGetPhysAddr()
{
    MmuLevelSegment * pSegment = m_pSegment->GetRawSegment();
    MmuLevel * pMmuLevel = pSegment->GetMmuLevel();
    const GMMU_FIELD_ADDRESS *pAddrFld;

    if (IsPteEntry())
    {
        const GMMU_FMT_PTE* pFmtPte = pMmuLevel->GetGmmuFmt()->pPte;
        GMMU_APERTURE aperture = gmmuFieldGetAperture(&pFmtPte->fldAperture, GetDataPtr());

        pAddrFld = gmmuFmtPtePhysAddrFld(pFmtPte, aperture);
    }
    else
    {
        MmuLevelSegmentInfo segmentInfo;
        m_pSegment->GetMmuLevelSegmentInfo(&segmentInfo);
        const GMMU_FMT_PDE* pPde = gmmuFmtGetPde(pMmuLevel->GetGmmuFmt(), 
                segmentInfo.pFmtLevel, m_pSegment->GetSubLevelIndex());
        GMMU_APERTURE aperture = gmmuFieldGetAperture(&pPde->fldAperture, GetDataPtr());

        pAddrFld = gmmuFmtPdePhysAddrFld(pPde, aperture);

    }
    
    if (pAddrFld)
    {
        return gmmuFieldGetAddress(pAddrFld, GetDataPtr());
    }

    MASSERT(!"Can't find corresponding physical address.");
    return ~0x0;
}

void UtlMmuEntry::FieldSetAperture
(
    APERTURE aperture
)
{
    MmuLevelSegment * pSegment = m_pSegment->GetRawSegment();
    MmuLevel * pMmuLevel = pSegment->GetMmuLevel();
    const GMMU_FIELD_APERTURE *pFmtAperture;
    if (IsPteEntry())
    {
        const GMMU_FMT_PTE* pFmtPte = pMmuLevel->GetGmmuFmt()->pPte;
        pFmtAperture = &pFmtPte->fldAperture;
    }
    else
    {
        MmuLevel * pMmuLevel = pSegment->GetMmuLevel();
        MmuLevelSegmentInfo segmentInfo;
        m_pSegment->GetMmuLevelSegmentInfo(&segmentInfo);
            const GMMU_FMT_PDE* pPde = gmmuFmtGetPde(pMmuLevel->GetGmmuFmt(), 
                segmentInfo.pFmtLevel, m_pSegment->GetSubLevelIndex());
        pFmtAperture = &pPde->fldAperture;

    }
    gmmuFieldSetAperture(pFmtAperture, static_cast<GMMU_APERTURE>(aperture),
            GetDataPtr());
}

string UtlMmuEntry::FieldGetAperturePy()
{
    return s_ApertureName.at(FieldGetAperture()); 
}

APERTURE UtlMmuEntry::FieldGetAperture()
{
    GMMU_APERTURE aperture;
    MmuLevelSegment * pSegment = m_pSegment->GetRawSegment();
    MmuLevel * pMmuLevel = pSegment->GetMmuLevel();

    if (IsPteEntry())
    {
        const GMMU_FMT_PTE* pFmtPte = pMmuLevel->GetGmmuFmt()->pPte;
        aperture = gmmuFieldGetAperture(&pFmtPte->fldAperture, GetDataPtr());

    }
    else
    {
        MmuLevel * pMmuLevel = pSegment->GetMmuLevel();
        MmuLevelSegmentInfo segmentInfo;
        m_pSegment->GetMmuLevelSegmentInfo(&segmentInfo);
            const GMMU_FMT_PDE* pPde = gmmuFmtGetPde(pMmuLevel->GetGmmuFmt(), 
                segmentInfo.pFmtLevel, m_pSegment->GetSubLevelIndex());
        aperture = gmmuFieldGetAperture(&pPde->fldAperture, GetDataPtr());
    }

    return static_cast<APERTURE>(aperture);
}

// V2 supported bit read and V3 support which is not in PCF
#define GET_FIELD_BOOL(fmtPte, field, data)                           \
    do{                                                               \
         if (lwFieldIsValid32(&(fmtPte)->fld##field.desc))            \
             return lwFieldGetBool(&(fmtPte)->fld##field, &data[0]) ; \
    }while(0);

bool UtlMmuEntry::FieldGetBoolPy(py::kwargs kwargs)
{
    FIELD field = UtlUtility::GetRequiredKwarg<FIELD>(kwargs, "field", "MmuEntry.FieldGetBool");

    return FieldGetBool(field);
}

bool UtlMmuEntry::FieldGetBool(FIELD field)
{
    MmuLevelSegment * pSegment = m_pSegment->GetRawSegment();

    if (IsPteEntry())
    {
        MmuLevel * pMmuLevel = pSegment->GetMmuLevel();
        const GMMU_FMT_PTE* pFmtPte = pMmuLevel->GetGmmuFmt()->pPte;
        switch(field)
        {
            case FIELD::ReadOnly:
                return pSegment->GetFieldBool(MmuLevelSegment::PtePcfField::ReadOnly, GetDataPtr());
                break;
            case FIELD::AtomicDisable:
                return pSegment->GetFieldBool(MmuLevelSegment::PtePcfField::AtomicDisable, GetDataPtr());
                break;
            case FIELD::Volatile:
                return pSegment->GetFieldBool(MmuLevelSegment::PtePcfField::Volatile, GetDataPtr());
                break;
            case FIELD::Privilege:
                return pSegment->GetFieldBool(MmuLevelSegment::PtePcfField::Privilege, GetDataPtr());
                break;
            case FIELD::Sparse:
                // Sprase bit in v3 works on this way
                // Sparse bit in v2 is same as volatile which don't be supported on the UTL
                return pSegment->GetFieldBool(MmuLevelSegment::PtePcfField::Sparse, GetDataPtr());
                break;
            case FIELD::Valid:
                GET_FIELD_BOOL(pFmtPte, Valid, GetData());
                break;
            case FIELD::Acd:
                // ToDo: Not supported yet. 
                //GET_FIELD_BOOL(pFmtPte, AtomicDisable, GetData());
                break;
            case FIELD::AtsAllow:
            default:
                UtlUtility::PyError("Not support format.");
        }
    }
    else
    {
        // PDE branch
        switch(field)
        {
            case FIELD::Volatile:
                return pSegment->GetFieldBool(MmuLevelSegment::PdePcfField::Volatile, GetDataPtr(), m_pSegment->GetSubLevelIndex());
                break;
            case FIELD::AtsAllow:
                return pSegment->GetFieldBool(MmuLevelSegment::PdePcfField::AtsAllow, GetDataPtr(), m_pSegment->GetSubLevelIndex());
                break;
            case FIELD::Sparse:
                return pSegment->GetFieldBool(MmuLevelSegment::PdePcfField::Sparse, GetDataPtr(), m_pSegment->GetSubLevelIndex());
                break;
            case FIELD::Valid:
                // For the PDE, deduce the valid from the aperture. If is none zero aperture, the valid is true
                return (FieldGetAperture() == APERTURE::APERTURE_ILWALID) ? false : true;
            case FIELD::ReadOnly:
            case FIELD::AtomicDisable:
            case FIELD::Privilege:
            case FIELD::Acd:
            default:
                UtlUtility::PyError("Not support format.");
        }
    }

    return false;
}

void UtlMmuEntry::FieldSetBool
(
    FIELD field,
    bool flag
)
{
    MmuLevelSegment * pSegment = m_pSegment->GetRawSegment();

    switch(field)
    {
        case FIELD::ReadOnly:
            pSegment->SetSegmentAccessRO(m_Offset, &m_Size, m_pSegment->GetSubLevelIndex(), flag);
            break;
        case FIELD::AtomicDisable:
            pSegment->SetSegmentAtomicDisable(m_Offset, &m_Size, m_pSegment->GetSubLevelIndex(), flag);
            break;
        case FIELD::Volatile:
            pSegment->SetSegmentVolatile(m_Offset, &m_Size, m_pSegment->GetSubLevelIndex(), flag);
            break;
        case FIELD::Privilege:
            pSegment->SetSegmentPriv(m_Offset, &m_Size, m_pSegment->GetSubLevelIndex(), flag);
            break;
        case FIELD::Valid:
            pSegment->SetSegmentValid(m_Offset, &m_Size, m_pSegment->GetSubLevelIndex(), flag);
            break;
        case FIELD::Sparse:
            pSegment->SetSegmentSparse(m_Offset, &m_Size, m_pSegment->GetSubLevelIndex(), flag);
            break;
        case FIELD::Acd:
            // ToDo: RM has not supported this yet
            break;
        default:
            UtlUtility::PyError("Not support format.");
    }
}

void UtlMmuEntry::ChangePageEntryType
(
    MmuLevel::MmuSubLevelIndex subLevelIdx,
    bool bToPte
)
{
    MmuLevelSegment * pSegment = m_pSegment->GetRawSegment();
    pSegment->ChangePageEntryType(m_Offset, &m_Size, subLevelIdx, bToPte);
}


void UtlMmuEntry::ConnectToLowerLevel
(
    MmuLevel::MmuSubLevelIndex subLevelIdx
)
{
    MmuLevelSegment * pSegment = m_pSegment->GetRawSegment();
    pSegment->ConnectToLowerLevel(m_Offset, &m_Size, subLevelIdx);
}


void UtlMmuEntry::FieldChangePageSize
(
    LEVEL targetLevel
)
{
    UtlSurface * pSurf = m_pSegment->GetSurface();
    UtlMmu::SUB_LEVEL_INDEX subLevelIdx = SUB_LEVEL_INDEX::DEFAULT; 
    if (targetLevel == LEVEL::LEVEL_PTE_2M)
    {
        if (m_OrignalMmuInfo.pageSize == GpuDevice::PAGESIZE_64KB)
        {
            subLevelIdx = SUB_LEVEL_INDEX::BIG;
        }
        else if (m_OrignalMmuInfo.pageSize == GpuDevice::PAGESIZE_4KB)
        {
            subLevelIdx = SUB_LEVEL_INDEX::SMALL;
        }
    }
    vector<UtlMmuEntry *> targetEntryLists = pSurf->GetEntryLists(targetLevel,
            subLevelIdx, m_Offset, m_Size);
    
    // 1. Connect/disconnect last level
    //
    bool deactiveSrcLevel = false;
    MmuLevelSegmentInfo segmentInfo;
    m_pSegment->GetMmuLevelSegmentInfo(&segmentInfo);
    if (targetLevel == LEVEL::LEVEL_PTE_2M)
    {
        //
        // 4k/64k -> 2m: 5 levels -> 4 levels
        deactiveSrcLevel = true; // deactive 4k/64k
        
        for (auto entry : targetEntryLists)
        {
            entry->PrintMmuInfo(Utility::StrPrintf("Before change targetLevel = %d mmu:", targetLevel));
            entry->ChangePageEntryType(MmuLevel::GMMU_SUB_LEVEL_PAGE_TABLE_DEFAULT, 
                                       true);
            entry->PrintMmuInfo(Utility::StrPrintf("After change targetLevel = %d mmu's entry type:", targetLevel));
        }

    }
    else if (segmentInfo.levelPageSize == GpuDevice::PAGESIZE_2MB)
    {
        //
        // 2m -> 4k/64k: 4 levels -> 5 levels
        // Connect both 4k and 64k PTE
        for (UINT32 type = static_cast<UINT32>(SUB_LEVEL_INDEX::BIG); 
                type <= static_cast<UINT32>(SUB_LEVEL_INDEX::SMALL); ++type)
        {
            vector<UtlMmuEntry *> pte2mEntryLists = pSurf->GetEntryLists(LEVEL_PTE_2M,
                                                           static_cast<SUB_LEVEL_INDEX>(type), 
                                                           m_Offset, m_Size);

            for (const auto &entry : pte2mEntryLists)
            {
                MmuLevel::MmuSubLevelIndex subLevelIdx = 
                        static_cast<MmuLevel::MmuSubLevelIndex>(type);
                entry->ChangePageEntryType(subLevelIdx, false);
                entry->ConnectToLowerLevel(subLevelIdx);               
                entry->PrintMmuInfo("After change to PTE_2M mmu:");
            }
        }
           
    }
    else
    {
        // 4k <-> 64k:  5 levels unchanged
        deactiveSrcLevel = true; // deactive opposite page size
    }

    // 2. Copy paste the MmuInfo from current entry to target entry
    //
    
    for (const auto &entry : targetEntryLists)
    {
        entry->PrintMmuInfo(Utility::StrPrintf("Before change targetLevel = %d mmu:", targetLevel));
        // 2.1 copy phys address from current active pte
        //
        entry->FieldSetPhysAddr(FieldGetPhysAddr());
        entry->FieldSetAperture(FieldGetAperture());
        // 2.2 copy src attrs to dst
        //
        entry->FieldSetBool(FIELD::Valid, FieldGetBool(FIELD::Valid));
        entry->FieldSetPcf(FieldGetPcf());
        
        entry->PrintMmuInfo(Utility::StrPrintf("After change targetLevel = %d mmu:", targetLevel));
    }

    // 3. deactive orignal src level if it's mapped
    if (deactiveSrcLevel && FieldGetBool(FIELD::Valid))
    {
        FieldSetBool(FIELD::Valid, false);
    }

    // 4. update the active page size
    SetActivePageSize(targetLevel);
}

void UtlMmuEntry::SetActivePageSize(LEVEL level)
{
    MmuLevelTree * pMmuLevelTree = GetMmuLevelTree();
    MmuLevelSegment * pSegment = m_pSegment->GetRawSegment();
    MmuLevel * pMmuLevel = pSegment->GetMmuLevel();

    UINT64 pageSize = ~0x0;
    switch(level)
    {
        case LEVEL::LEVEL_PTE_512M:
            pageSize = GpuDevice::PAGESIZE_512MB;
            break;
        case LEVEL::LEVEL_PTE_2M:
            pageSize = GpuDevice::PAGESIZE_2MB;
            break;
        case LEVEL::LEVEL_PTE_64K:
            pageSize = pMmuLevel->GetGpuDevice()->GetBigPageSize();
            break;
        case LEVEL::LEVEL_PTE_4K:
            pageSize = GpuDevice::PAGESIZE_4KB;
            break;
        case LEVEL::LEVEL_PDE2:
        case LEVEL::LEVEL_PDE3:
        case LEVEL::LEVEL_PDE4:
        default:
            MASSERT(!"should NOT hit here");
    }
    pMmuLevelTree->SetActivePageSize(pageSize);
    pMmuLevelTree->ChangePageSize();
}

MmuLevelTree * UtlMmuEntry::GetMmuLevelTree()
{
    MmuLevelSegment * pSegment = m_pSegment->GetRawSegment();
    MmuLevel * pMmuLevel = pSegment->GetMmuLevel();
    return pMmuLevel->GetMmuLevelTree();
}

// Print entry MMU information
//
void UtlMmuEntry::PrintMmuInfo(const string & message)
{
    vector<vector<string>> mmuData(2);

    // Add the raw data
    mmuData[0].push_back("Raw Data");
    size_t sizeInDword = GetData().size() / sizeof(UINT32);
    UINT32 *pData = (UINT32*)(GetDataPtr());
    string rawData;
    // normal PTE/PDE is 2dw
    // PDE0 is 4dw
    for (size_t i = 0; i < sizeInDword; ++i)
    {
        rawData += Utility::StrPrintf("0x%08x ", pData[i]);
    }
    mmuData[1].push_back(rawData);

    // Add the bool value
    for (auto name : s_FieldName)
    {
        if ((IsPteEntry() && std::get<2>(name) == PteOrPde::PDEOnly) ||
                (!IsPteEntry() && std::get<2>(name) == PteOrPde::PTEOnly))
        {
            continue;
        }
        
        mmuData[0].push_back(std::get<1>(name));
        mmuData[1].push_back(FieldGetBool(std::get<0>(name)) ? "TRUE" : "FALSE");
    }

    // Add aperture
    mmuData[0].push_back("Aperture");
    mmuData[1].push_back(s_ApertureName.at(FieldGetAperture()));

    // Add physical address
    mmuData[0].push_back("PhysAddr");
    mmuData[1].push_back(Utility::StrPrintf("0x%llx", FieldGetPhysAddr()));

    // Add PCF
    mmuData[0].push_back("PCF");
    mmuData[1].push_back(FieldGetPcfPy());

    Utl::Instance()->TriggerTablePrintEvent(std::move(const_cast<string &>(message)), std::move(mmuData), true);
}
