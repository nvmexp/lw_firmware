/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "hbm_spec_devid.h"
#include "hbm_spec_defines.h"

#include "core/include/massert.h"
#include "core/include/utility.h"

#include <map>

#define EXTRACT_BITS(_r, _t, _f) (Utility::ExtractBitsFromU32Vector<_t>(_r, (1 ? _f), (0 ? _f)))

namespace
{
    constexpr UINT16 SamsungHbmModelKey(UINT08 modelPartNum, UINT08 stackHeight)
    {
        return static_cast<UINT16>(modelPartNum) << 8 | stackHeight;
    }
}

namespace {
    using namespace Memory::Hbm;
    using HbmModel = Memory::Hbm::Model;

    // Taken from Samsung HBM spec as per med team
    using VendorHbmModelMap = map<UINT16, HbmModel>;
    static const VendorHbmModelMap s_SamsungHbmModelMap =
    {
        { 0x01,                        HbmModel(SpecVersion::V2, Vendor::Samsung, Die::B, StackHeight::Hi4, Revision::A,  "A_4HI")   }, // HBM2 B-Die 4HI RevA
        { SamsungHbmModelKey(0x41, 4), HbmModel(SpecVersion::V2, Vendor::Samsung, Die::B, StackHeight::Hi4, Revision::B,  "B_4HI")   }, // HBM2 B-Die 4HI RevB
        { 0x51,                        HbmModel(SpecVersion::V2, Vendor::Samsung, Die::B, StackHeight::Hi4, Revision::C,  "C_4HI")   }, // HBM2 B-Die 4HI RevC
        { 0x11,                        HbmModel(SpecVersion::V2, Vendor::Samsung, Die::B, StackHeight::Hi4, Revision::D,  "D_4HI")   }, // HBM2 B-Die 4HI RevD or RevE
        { 0x21,                        HbmModel(SpecVersion::V2, Vendor::Samsung, Die::B, StackHeight::Hi4, Revision::F,  "F_4HI")   }, // HBM2 B-Die 4HI RevF
        { 0x02,                        HbmModel(SpecVersion::V2, Vendor::Samsung, Die::B, StackHeight::Hi8, Revision::A,  "A_8HI")   }, // HBM2 B-Die 8HI RevA
        { 0x52,                        HbmModel(SpecVersion::V2, Vendor::Samsung, Die::B, StackHeight::Hi8, Revision::C,  "C_8HI")   }, // HBM2 B-Die 8HI RevC
        { 0x12,                        HbmModel(SpecVersion::V2, Vendor::Samsung, Die::B, StackHeight::Hi8, Revision::D,  "D_8HI")   }, // HBM2 B-Die 8HI RevD or RevE
        { 0x22,                        HbmModel(SpecVersion::V2, Vendor::Samsung, Die::B, StackHeight::Hi8, Revision::F,  "F_8HI")   }, // HBM2 B-Die 8HI RevF
        { SamsungHbmModelKey(0x41, 8), HbmModel(SpecVersion::V2, Vendor::Samsung, Die::X, StackHeight::Hi8, Revision::A2, "XA2_8HI") }, // HBM2 X-Die 8HI RevA2
        { 0x40,                        HbmModel(SpecVersion::V2, Vendor::Samsung, Die::X, StackHeight::Hi4, Revision::A2, "XA2_4HI") }, // HBM2 X-Die 4HI RevA2
        { 0x43,                        HbmModel(SpecVersion::V2e, Vendor::Samsung, Die::Unknown, StackHeight::Hi8, Revision::Unknown, "?_16GB_8HI") },
        { 0x42,                        HbmModel(SpecVersion::V2e, Vendor::Samsung, Die::Unknown, StackHeight::Hi4, Revision::Unknown, "?_8GB_4HI") }
    };

    static const VendorHbmModelMap s_SkHynixHbmModelMap =
    {
        { 0x00, HbmModel(SpecVersion::V2, Vendor::SkHynix, Die::Mask, StackHeight::Hi8, Revision::V4, "Mask4_8Hi") },
        // TODO(stewarts): upcoming SK Hynix model
        //{  <TBD>, HbmModel(SpecVersion::V2e, Vendor::SkHynix, Die::Unknown, StackHeight::Hi4, Revision::Unknown, "?_4Hi") },
        // TODO(aperiwal): Update die / revision once known
        { 0x0A, HbmModel(SpecVersion::V2e, Vendor::SkHynix, Die::Unknown, StackHeight::Hi8, Revision::Unknown, "?_16GB_8Hi") },
        { 0x4a, HbmModel(SpecVersion::V2e, Vendor::SkHynix, Die::Unknown, StackHeight::Hi4, Revision::Unknown, "?_8GB_4HI") },
        { 0x46, HbmModel(SpecVersion::V2e, Vendor::SkHynix, Die::Unknown, StackHeight::Hi4, Revision::Unknown, "?_8GB_4HI") }
    };

    //! SK Hynix used different manufacturer number for their old HBM.
    static const VendorHbmModelMap s_SkHynixOldHbmModelMap =
    {
        { 0xb, HbmModel(SpecVersion::V2, Vendor::SkHynix, Die::Unknown, StackHeight::Unknown, Revision::Unknown, "Unknown") }, // GP100 HBM
    };

    // TODO(stewarts): upcoming Micron models
    static const VendorHbmModelMap s_MicronHbmModelMap =
    {
        //{ <TBD>, HbmModel(SpecVersion::V2e, Vendor::Micron, Die::Unknown, StackHeight::Hi4, Revision::Unknown, "?_4Hi") },
        { 0, HbmModel(SpecVersion::V2e, Vendor::Micron, Die::Unknown, StackHeight::Hi8, Revision::Unknown, "?_16GB_8Hi") }
    };

    static constexpr const char* s_UnknownStr = "Unknown";
}

void HbmDeviceIdData::Initialize(const vector<UINT32> &regValues)
{
    MASSERT(regValues.size() == HBM_WIR_DEVICE_ID_NUM_DWORDS);
    MASSERT(m_ManufacturerId == Manufacturer::Unknown);

    m_Gen2Supported     = EXTRACT_BITS(regValues, UINT08, HBM_DEV_ID_BITS_GEN2_TEST) ? true : false;
    m_EccSupported      = EXTRACT_BITS(regValues, UINT08, HBM_DEV_ID_BITS_ECC) ? true : false;
    m_DensityPerChan    = EXTRACT_BITS(regValues, UINT08, HBM_DEV_ID_BITS_DENSITY);
    m_ManufacturerId    = static_cast<Manufacturer>(EXTRACT_BITS(regValues, UINT08, HBM_DEV_ID_BITS_MANUFACTURER_ID));
    m_ManufacturingLoc  = EXTRACT_BITS(regValues, UINT08, HBM_DEV_ID_BITS_MANUFACTURING_LOCATION);
    m_ManufacturingYear = EXTRACT_BITS(regValues, UINT08, HBM_DEV_ID_BITS_MANUFACTURING_YEAR);
    m_ManufacturingWeek = EXTRACT_BITS(regValues, UINT08, HBM_DEV_ID_BITS_MANUFACTURING_WEEK);
    m_SerialNumber      = EXTRACT_BITS(regValues, UINT64, HBM_DEV_ID_BITS_SERIAL_NUMBER);
    m_AddressingMode    = EXTRACT_BITS(regValues, UINT08, HBM_DEV_ID_BITS_ADDRESSING_MODE);
    m_ChannelsAvailable = EXTRACT_BITS(regValues, UINT08, HBM_DEV_ID_BITS_CHANNEL_AVAILABLE);
    m_StackHeight       = EXTRACT_BITS(regValues, UINT08, HBM_DEV_ID_BITS_HBM_STACK_HEIGHT);
    m_ModelPartNumber   = EXTRACT_BITS(regValues, UINT08, HBM_DEV_ID_BITS_MODEL_PART_NUMBER);

#ifdef USE_FAKE_HBM_FUSE_DATA
    InitFakeData();
#endif
}

void HbmDeviceIdData::InitFakeData()
{
    m_Gen2Supported     = true;
    m_EccSupported      = true;
    m_DensityPerChan    = static_cast<UINT08>(Hbm2ChannelMemoryDensity::_8Gb);
    m_ManufacturerId    = Manufacturer::Samsung;
    m_ManufacturingLoc  = 0x9;
    m_ManufacturingYear = 0x5;
    m_ManufacturingWeek = 0x20;
    m_SerialNumber      = 0x2008a1632;
    m_AddressingMode    = HBM_DEV_ID_ADDR_MODE_PSEUDO;
    m_ChannelsAvailable = 0xff;
    m_StackHeight       = HBM_DEV_ID_STACK_HEIGHT_2_OR_4;
    m_ModelPartNumber   = 0x12;
}

bool HbmDeviceIdData::IsInitialized() const
{
    // If every field is NULL, then device ID was not initialized!
    return m_Gen2Supported ||
           m_EccSupported ||
           m_DensityPerChan ||
           m_ManufacturerId != Manufacturer::Unknown ||
           m_ManufacturingLoc ||
           m_ManufacturingYear ||
           m_ManufacturingWeek ||
           m_SerialNumber ||
           m_AddressingMode ||
           m_ChannelsAvailable ||
           m_StackHeight ||
           m_ModelPartNumber;
}

UINT08 HbmDeviceIdData::GetStackHeight() const
{
    switch (m_StackHeight)
    {
        // As per spec, non zero bit  indicates stack Height is 8, else stack height is 2/4.
        // We are going to assume '0' means height as 4 as spec is not clear when stack
        // height is 2.
        case HBM_DEV_ID_STACK_HEIGHT_2_OR_4:
            return 4;

        case HBM_DEV_ID_STACK_HEIGHT_8:
            return 8;

        default:
            // Avoid assertion for now until Bug 1943288 is resolved
            //MASSERT(!"Unknown HBM Stack Height!");
            break;
    };

    return 0;
}

string HbmDeviceIdData::GetVendorName() const
{
    return ColwertManufacturerIdToVendorName(m_ManufacturerId);
}

/* static */ string HbmDeviceIdData::ColwertManufacturerIdToVendorName(Manufacturer manufacturerId)
{
    switch (manufacturerId)
    {
        case Manufacturer::Samsung:
            return "Samsung";

        case Manufacturer::SkHynix:
            return "SK Hynix";

        case Manufacturer::Micron:
            return "Micron";

        default:
            break;
    };

    return s_UnknownStr;
}

string HbmDeviceIdData::GetChannelsAvail() const
{
    string chansAvailStr;

    for (UINT08 i = 0; i < 8; i++)
    {
        if ((m_ChannelsAvailable >> i) & 0x1)
        {
            chansAvailStr.push_back('a' + i);
        }
    }

    return chansAvailStr;
}

string HbmDeviceIdData::GetAddressMode() const
{
    switch (m_AddressingMode)
    {
        case HBM_DEV_ID_ADDR_MODE_PSEUDO:
            return "Only Pseudo Channel Mode Supported";

        case HBM_DEV_ID_ADDR_MODE_LEGACY:
            return "Only Legacy Mode Supported";

        default:
            // Avoid assertion for now until Bug 1943288 is resolved
            //MASSERT(!"Unknown HBM Address Mode!");
            break;
    };

    return "";
}

UINT16 HbmDeviceIdData::GetDensityPerChanGb() const
{
    Memory::Hbm::Model hbmModel;
    if (GetHbmModel(&hbmModel) == RC::OK)
    {
        switch (hbmModel.spec)
        {
            case Memory::Hbm::SpecVersion::V2:
                return GetHbm2DensityPerChanGb();

            case Memory::Hbm::SpecVersion::V2e:
                return GetHbm2eDensityPerChanGb();

            default:
                MASSERT(!"Unknown HBM spec version");
                break;
        }
    }

    return 0;
}

UINT16 HbmDeviceIdData::GetHbm2DensityPerChanGb() const
{
    switch (static_cast<Hbm2ChannelMemoryDensity>(m_DensityPerChan))
    {
        case Hbm2ChannelMemoryDensity::_1Gb:
            return 1;

        case Hbm2ChannelMemoryDensity::_2Gb:
            return 2;

        case Hbm2ChannelMemoryDensity::_4Gb:
            return 4;

        case Hbm2ChannelMemoryDensity::_8Gb:
            return 8;

        case Hbm2ChannelMemoryDensity::_16Gb:
            return 16;

        case Hbm2ChannelMemoryDensity::_32Gb:
            return 32;

        default:
            // Avoid assertion for now until Bug 1943288 is resolved
            //MASSERT(!"Unknown HBM channel memory density!");
            break;
    };

    return 0;
}

UINT16 HbmDeviceIdData::GetHbm2eDensityPerChanGb() const
{
    switch (static_cast<Hbm2eChannelMemoryDensity>(m_DensityPerChan))
    {
        case Hbm2eChannelMemoryDensity::_1Gb:
            return 1;

        case Hbm2eChannelMemoryDensity::_2Gb:
            return 2;

        case Hbm2eChannelMemoryDensity::_4Gb:
            return 4;

        case Hbm2eChannelMemoryDensity::_6Gb:
            return 6;

        case Hbm2eChannelMemoryDensity::_8Gb_8Hi:
        case Hbm2eChannelMemoryDensity::_8Gb:
            return 8;

        case Hbm2eChannelMemoryDensity::_12Gb_8Hi:
        case Hbm2eChannelMemoryDensity::_8Gb_12Hi:
            return 12;

        case Hbm2eChannelMemoryDensity::_16Gb_8Hi:
            return 16;

        case Hbm2eChannelMemoryDensity::_12Gb_12Hi:
            return 18;

        case Hbm2eChannelMemoryDensity::_16Gb_12Hi:
            return 24;

        default:
            // Avoid assertion for now until Bug 1943288 is resolved
            //MASSERT(!"Unknown HBM channel memory density!");
            return 0;
    };
}

UINT16 HbmDeviceIdData::GetManufacturingYear() const
{
    return m_ManufacturingYear + 2011;
}

//!
//! \brief Get the model of HBM the HBM device represents.
//!
//! \param[out] pHbmModel HBM model.
//!
RC HbmDeviceIdData::GetHbmModel(Memory::Hbm::Model* pHbmModel) const
{
    MASSERT(pHbmModel);
    return HbmDeviceIdData::GetHbmModel(GetVendorId(), GetModelPartNumber(),
                                        GetStackHeight(), pHbmModel);
}

RC HbmDeviceIdData::GetHbmModel
(
    Manufacturer manufacturerId,
    UINT08 modelPartNum,
    UINT08 stackHeight,
    Memory::Hbm::Model* pHbmModel
)
{
    MASSERT(pHbmModel);
    UINT16 key = modelPartNum;
    const VendorHbmModelMap* pVendorHbmModelMap = nullptr;

    switch (manufacturerId)
    {
        case Manufacturer::Samsung:
            key = (key == 0x41) ? SamsungHbmModelKey(modelPartNum, stackHeight) : key;
            pVendorHbmModelMap = &s_SamsungHbmModelMap;
            break;

        case Manufacturer::SkHynix:
            pVendorHbmModelMap = &s_SkHynixHbmModelMap;
            break;

        case Manufacturer::SkHynixOld:
            pVendorHbmModelMap = &s_SkHynixOldHbmModelMap;
            break;

        case Manufacturer::Micron:
            pVendorHbmModelMap = &s_MicronHbmModelMap;
            break;

        default:
            Printf(Tee::PriError, "Unknown HBM vendor: %u\n", static_cast<UINT32>(manufacturerId));
            return RC::UNSUPPORTED_DEVICE;
    }

    MASSERT(pVendorHbmModelMap);
    const auto iter = pVendorHbmModelMap->find(key);
    if (iter != pVendorHbmModelMap->end())
    {
        *pHbmModel = iter->second;
    }
    else
    {
        Printf(Tee::PriError, "Unknown HBM model: 0x%x\n", key);
        return RC::UNSUPPORTED_DEVICE;
    }

    return RC::OK;
}

/* static */ string HbmDeviceIdData::ColwertModelPartNumberToRevisionStr
(
    Manufacturer manufacturerId
    ,UINT08 modelPartNum
    ,UINT08 stackHeight
)
{
    Memory::Hbm::Model hbmModel;
    if (GetHbmModel(manufacturerId, modelPartNum, stackHeight, &hbmModel) == RC::OK)
    {
        return hbmModel.name;
    }

    return s_UnknownStr;
}

RC HbmDeviceIdData::DumpDevIdData() const
{
    if (!IsInitialized())
    {
        Printf(Tee::PriError, "Cannot dump HBM dev data, site not initialized!\n");
        return RC::WAS_NOT_INITIALIZED;
    }

    Printf(Tee::PriNormal, "  Serial #           : 0x%09llx\n", GetSerialNumber());
    Printf(Tee::PriNormal, "  Model Part #       : 0x%02hhx\n", GetModelPartNumber());
    Printf(Tee::PriNormal, "  Vendor Name        : %s\n", GetVendorName().c_str());
    Printf(Tee::PriNormal, "  Stack Height       : %d\n", GetStackHeight());
    Printf(Tee::PriNormal, "  Channels Avail     : %s\n", GetChannelsAvail().c_str());
    Printf(Tee::PriNormal, "  Address Mode       : %s\n", GetAddressMode().c_str());
    UINT16 densityPerChan = GetDensityPerChanGb();
    if (densityPerChan)
    {
        Printf(Tee::PriNormal, "  Density/chan       : %u Gb\n", densityPerChan);
    }
    Printf(Tee::PriNormal, "  ECC Supported      : %s\n", GetEccSupported() ? "yes" : "no");
    Printf(Tee::PriNormal, "  Gen2 Supported     : %s\n", GetGen2Supported() ? "yes" : "no");
    Printf(Tee::PriNormal, "  Manufacturing Year : %u\n", GetManufacturingYear());
    Printf(Tee::PriNormal, "  Manufacturing Week : %u\n", GetManufacturingWeek());
    Printf(Tee::PriNormal, "  Manufacturing Loc  : 0x%hhx\n", GetManufacturingLoc());

    return OK;
}

// Revision string that contains combined information about Version and Stack Height
string HbmDeviceIdData::GetRevisionStr() const
{
    // Getting the Rev ID is based on Vendor Specific Model Part Number
    return ColwertModelPartNumberToRevisionStr(GetVendorId(), GetModelPartNumber(),
                                               GetStackHeight());
}

bool HbmDeviceIdData::GetPseudoChannelSupport() const
{
    return m_AddressingMode == HBM_DEV_ID_ADDR_MODE_PSEUDO;
}

bool HbmDeviceIdData::IsDataValid() const
{
    switch (GetVendorId())
    {
        case Manufacturer::Samsung:
        case Manufacturer::SkHynix:
        case Manufacturer::SkHynixOld:
        case Manufacturer::Micron:
            // Supported Vendor
            break;
        default:
            Printf(Tee::PriLow, "Read unsupported or unknown vendor from HBM\n");
            // Unsupported Vendor
            return false;
    }

    if (GetDensityPerChanGb() == 0)
    {
        Printf(Tee::PriLow, "Read invalid density from HBM\n");
        return false;
    }

    if (GetAddressMode().empty())
    {
        Printf(Tee::PriLow, "Read invalid addressing mode from HBM\n");
        return false;
    }

    if (GetStackHeight() == 0)
    {
        Printf(Tee::PriLow, "Read invalid stack height from HBM\n");
        return false;
    }

    return true;
}

//!
//! \brief True if the given HBM device data is same model of HBM.
//!
//! The criteria for being the same model of HBM is: vendor, model part number,
//! and channel density. The revision and stack height are derived from the
//! model part number and checked for redundancy.
//!
//! \param o Other HBM devid data to check against.
//!
bool HbmDeviceIdData::IsSameHbmModel(const HbmDeviceIdData& o) const
{
    return GetVendorId() == o.GetVendorId()
        && GetRevisionStr() == o.GetRevisionStr()
        && GetDensityPerChanGb() == o.GetDensityPerChanGb()
        && GetStackHeight() == o.GetStackHeight();
}
