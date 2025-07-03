/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "device/interface/lwlink/lwlcci.h"

namespace
{
    struct ErrFlagData
    {
        bool         bFatal    = false;
        const char * errString = nullptr;
    };
    map<LwLinkCableController::ErrorFlagType, ErrFlagData> s_ErrFlagData
    {
         { LwLinkCableController::ErrorFlagType::ModuleFirmwareFault,        { true,  "Module firmware fault"           } }
        ,{ LwLinkCableController::ErrorFlagType::DataPathFirmwareFault,      { true,  "Data path firmware fault"        } }
        ,{ LwLinkCableController::ErrorFlagType::HighTempAlarm,              { true,  "High temperature alarm"          } }
        ,{ LwLinkCableController::ErrorFlagType::LowTempAlarm,               { true,  "Low temperature alarm"           } }
        ,{ LwLinkCableController::ErrorFlagType::HighVoltageAlarm,           { true,  "High voltage alarm"              } }
        ,{ LwLinkCableController::ErrorFlagType::LowVoltageAlarm,            { true,  "Low voltage alarm"               } }
        ,{ LwLinkCableController::ErrorFlagType::Aux1HighAlarm,              { true,  "Aux 1 high alarm"                } }
        ,{ LwLinkCableController::ErrorFlagType::Aux1LowAlarm,               { true,  "Aux 1 low alarm"                 } }
        ,{ LwLinkCableController::ErrorFlagType::Aux1HighAlarm,              { true,  "Aux 2 high alarm"                } }
        ,{ LwLinkCableController::ErrorFlagType::Aux1LowAlarm,               { true,  "Aux 2 low alarm"                 } }
        ,{ LwLinkCableController::ErrorFlagType::Aux3HighAlarm,              { true,  "Aux 3 high alarm"                } }
        ,{ LwLinkCableController::ErrorFlagType::Aux3LowAlarm,               { true,  "Aux 3 low alarm"                 } }
        ,{ LwLinkCableController::ErrorFlagType::VendorHighAlarm,            { true,  "Vendor high alarm"               } }
        ,{ LwLinkCableController::ErrorFlagType::VendorLowAlarm,             { true,  "Vendor low alarm"                } }
        ,{ LwLinkCableController::ErrorFlagType::HighTempWarning,            { false, "High temperature warning"        } }
        ,{ LwLinkCableController::ErrorFlagType::LowTempWarning,             { false, "Low temperature warning"         } }
        ,{ LwLinkCableController::ErrorFlagType::HighVoltageWarning,         { false, "High voltage warning"            } }
        ,{ LwLinkCableController::ErrorFlagType::LowVoltageWarning,          { false, "Low voltage warning"             } }
        ,{ LwLinkCableController::ErrorFlagType::Aux1HighWarning,            { false, "Aux 1 high warning"              } }
        ,{ LwLinkCableController::ErrorFlagType::Aux1LowWarning,             { false, "Aux 1 low warning"               } }
        ,{ LwLinkCableController::ErrorFlagType::Aux1HighWarning,            { false, "Aux 2 high warning"              } }
        ,{ LwLinkCableController::ErrorFlagType::Aux1LowWarning,             { false, "Aux 2 low warning"               } }
        ,{ LwLinkCableController::ErrorFlagType::Aux3HighWarning,            { false, "Aux 3 high warning"              } }
        ,{ LwLinkCableController::ErrorFlagType::Aux3LowWarning,             { false, "Aux 3 low warning"               } }
        ,{ LwLinkCableController::ErrorFlagType::VendorHighWarning,          { false, "Vendor high warning"             } }
        ,{ LwLinkCableController::ErrorFlagType::VendorLowWarning,           { false, "Vendor low warning"              } }
        ,{ LwLinkCableController::ErrorFlagType::LatchedDataPathStateChange, { true,  "Latched data path state changed" } }
        ,{ LwLinkCableController::ErrorFlagType::TxFault,                    { true,  "TX fault"                        } }
        ,{ LwLinkCableController::ErrorFlagType::TxLossOfSignal,             { true,  "TX loss of signal"               } }
        ,{ LwLinkCableController::ErrorFlagType::TxCDRLossOfLock,            { true,  "TX CDR loss of lock"             } }
        ,{ LwLinkCableController::ErrorFlagType::TxAdaptiveInputEqFault,     { true,  "TX adaptive input EQ iault"      } }
        ,{ LwLinkCableController::ErrorFlagType::TxOutputPowerHighAlarm,     { true,  "TX output power high alarm"      } }
        ,{ LwLinkCableController::ErrorFlagType::TxOutputPowerLowAlarm,      { true,  "TX output power low alarm"       } }
        ,{ LwLinkCableController::ErrorFlagType::TxBiasHighAlarm,            { true,  "TX bias high alarm"              } }
        ,{ LwLinkCableController::ErrorFlagType::TxBiasLowAlarm,             { true,  "TX bias low alarm"               } }
        ,{ LwLinkCableController::ErrorFlagType::RxLatchedLossOfSignal,      { true,  "Latched RX loss of signal"       } }
        ,{ LwLinkCableController::ErrorFlagType::RxLatchedCDRLossOfLock,     { true,  "Latched RX CDR loss of lock"     } }
        ,{ LwLinkCableController::ErrorFlagType::RxInputPowerHighAlarm,      { true,  "RX input power high alarm"       } }
        ,{ LwLinkCableController::ErrorFlagType::RxInputPowerLowAlarm,       { true,  "RX intput power low alarm"       } }
        ,{ LwLinkCableController::ErrorFlagType::TxOutputPowerHighWarning,   { false, "TX output power high warning"    } }
        ,{ LwLinkCableController::ErrorFlagType::TxOutputPowerLowWarning,    { false, "TX output power low warning"     } }
        ,{ LwLinkCableController::ErrorFlagType::TxBiasHighWarning,          { false, "TX bias high warning"            } }
        ,{ LwLinkCableController::ErrorFlagType::TxBiasLowWarning,           { false, "TX bias low warning"             } }
        ,{ LwLinkCableController::ErrorFlagType::RxInputPowerHighWarning,    { false, "RX input power high warning"     } }
        ,{ LwLinkCableController::ErrorFlagType::RxInputPowerLowWarning,     { false, "RX input power low warning"      } }
    };
}

//-----------------------------------------------------------------------------
/* static */ const char * LwLinkCableController::ErrorFlagToString(ErrorFlagType errFlagType)
{
    MASSERT(s_ErrFlagData.count(errFlagType));
    if (!s_ErrFlagData.count(errFlagType))
        return "Unknown";
    return s_ErrFlagData[errFlagType].errString;
}

//-----------------------------------------------------------------------------
/* static */ const char * LwLinkCableController::FirmwareStatusToString(CCFirmwareStatus firmwareStatus)
{
    switch (firmwareStatus)
    {
        case CCFirmwareStatus::Running:    return "Running";
        case CCFirmwareStatus::Empty:      return "Empty";
        case CCFirmwareStatus::Present:    return "Present";
        case CCFirmwareStatus::NotPresent: return "NotPresent";
        default:
            break;
    }
    return "Unknown";
}

//-----------------------------------------------------------------------------
/* static */ const char * LwLinkCableController::FirmwareImageToString(CCFirmwareImage firmwareImage)
{
    switch (firmwareImage)
    {
        case CCFirmwareImage::Factory: return "Factory";
        case CCFirmwareImage::ImageA:  return "ImageA";
        case CCFirmwareImage::ImageB:  return "ImageB";
        default:
            break;
    }
    return "Unknown";
}

//-----------------------------------------------------------------------------
/* static */ const char * LwLinkCableController::InitStatusToString(CCInitStatus initStatus)
{
    switch (initStatus)
    {
        case CCInitStatus::NotFound:   return "Not Found";
        case CCInitStatus::Failure:    return "Init Failed";
        case CCInitStatus::Success:    return "Initialized";
        default:
            break;
    }
    return "Unknown";
}

//-----------------------------------------------------------------------------
/* static */ bool LwLinkCableController::IsErrorFlagFatal(ErrorFlagType errFlagType)
{
    MASSERT(s_ErrFlagData.count(errFlagType));
    if (!s_ErrFlagData.count(errFlagType))
        return true;
    return s_ErrFlagData[errFlagType].bFatal;
}

//-----------------------------------------------------------------------------
/* static */ const char * LwLinkCableController::ModuleIdentifierToString(CCModuleIdentifier mi)
{
    switch (mi)
    {
        case CCModuleIdentifier::Unknown:          return "Unknown";
        case CCModuleIdentifier::GBIC:             return "GBIC";
        case CCModuleIdentifier::Soldered:         return "Soldered";
        case CCModuleIdentifier::SFP:              return "SFP/SFP+/SFP28";
        case CCModuleIdentifier::XBI:              return "300 pin XBI";
        case CCModuleIdentifier::XENPAK:           return "XENPAK";
        case CCModuleIdentifier::XFP:              return "XFP";
        case CCModuleIdentifier::XFF:              return "XFF";
        case CCModuleIdentifier::XFP_E:            return "XFP-E";
        case CCModuleIdentifier::XPAK:             return "XPAK";
        case CCModuleIdentifier::X2:               return "X2";
        case CCModuleIdentifier::DWDM_SFP:         return "DWDM-SFP/SFP+";
        case CCModuleIdentifier::QSFP:             return "QSFP";
        case CCModuleIdentifier::QSFPPlus:         return "QSFP+ with SFF-8639 or SFF-8436";
        case CCModuleIdentifier::CXP:              return "CXP";
        case CCModuleIdentifier::SMMHD4X:          return "Shielded Mini Multilane HD 4x";
        case CCModuleIdentifier::SMMHD8X:          return "Shielded Mini Multilane HD 8x";
        case CCModuleIdentifier::QSFP28:           return "QSFP28";
        case CCModuleIdentifier::CXP2:             return "CXP2";
        case CCModuleIdentifier::CDFP_1_2:         return "CDFP (Style 1/Style 2)";
        case CCModuleIdentifier::SMMHD4XFanout:    return "Shielded Mini Multilane HD 4x Fanout";
        case CCModuleIdentifier::SMMHD8XFanout:    return "Shielded Mini Multilane HD 8x Fanout";
        case CCModuleIdentifier::CDFP_3:           return "CDFP (Style 3)";
        case CCModuleIdentifier::MicroQSFP:        return "Micro QSFP";
        case CCModuleIdentifier::QSFP_DD:          return "QSFP-DD 8x Pluggable Tranceiver";
        case CCModuleIdentifier::OSFP8X:           return "OSFP 8X Pluggable Tranceiver";
        case CCModuleIdentifier::SFP_DD:           return "SFP_DD";
        case CCModuleIdentifier::DFSP:             return "DFSP";
        case CCModuleIdentifier::MiniLinkX4:       return "x4 Minilink/Olwlink";
        case CCModuleIdentifier::MiniLinkX8:       return "x8 Minilink";
        case CCModuleIdentifier::QSFPPlusWithCMIS: return "QSFP+ with CMIS";
        case CCModuleIdentifier::Reserved:         return "Reserved";
        case CCModuleIdentifier::VendorSpecific:   return "Vendor Specific";
        default:
            break;
    }
    return "Unknown";
}

//-----------------------------------------------------------------------------
/* static */ const char * LwLinkCableController::ModuleMediaTypeToString(CCModuleMediaType mmt)
{
    switch (mmt)
    {
        case CCModuleMediaType::Undefined:     return "Undefined";
        case CCModuleMediaType::OpticalMmf:    return "OpticalMmf";
        case CCModuleMediaType::OpticalSmf:    return "OpticalSmf";
        case CCModuleMediaType::PassiveCopper: return "PassiveCopper";
        case CCModuleMediaType::ActiveCables:  return "ActiveCables";
        case CCModuleMediaType::BaseT:         return "BaseT";
        case CCModuleMediaType::Reserved:      return "Reserved";
        case CCModuleMediaType::Custom:        return "Custom";
        default:
            break;
    }
    return "Unknown";
}

//-----------------------------------------------------------------------------
/* static */ const char * LwLinkCableController::ModuleStateToString(ModuleState state)
{
    switch (state)
    {
        case ModuleState::Reserved:         return "Reserved";
        case ModuleState::LowPwr:           return "Low Power";
        case ModuleState::PwrUp:            return "Power Up";
        case ModuleState::Ready:            return "Ready";
        case ModuleState::PwrDn:            return "Power Down";
        case ModuleState::Fault:            return "Fault";
        default:
            break;
    }
    return "Unknown";
}

//-----------------------------------------------------------------------------
/* static */ string LwLinkCableController::LinkGradingValuesToString
(
    UINT32 linkId, 
    const vector<LwLinkCableController::GradingValues>& gradingVals
)
{
    string topoStr;

    auto LogCCGrading = [&] (const string & valueStr, 
                             INT32 firstLane,
                             INT32 lastLane,
                             const vector<UINT08> & vals)
    {
        constexpr size_t columnWidth = 37;

        string headerStr = valueStr + 
            Utility::StrPrintf(" Lane(%d-%d)", firstLane, lastLane);
        if (headerStr.size() < columnWidth)
            headerStr += string(columnWidth - headerStr.size(), ' ');
        headerStr += ":";
        topoStr += headerStr;
        string comma;

        for (INT32 lane = firstLane; lane <= lastLane; lane++)
        {
            topoStr += Utility::StrPrintf("%s %u", comma.c_str(), vals[lane]);
            comma = ",";
        }
        topoStr += "\n";
    };


    for (auto const & lwrGrading : gradingVals)
    {
        if ((lwrGrading.linkId == linkId) && (lwrGrading.laneMask != 0))
        {
            const INT32 firstLane =
                Utility::BitScanForward(lwrGrading.laneMask);
            const INT32 lastLane =
                Utility::BitScanReverse(lwrGrading.laneMask);
            
            LogCCGrading("    Diag RX Init Grading", 
                         firstLane, 
                         lastLane, 
                         lwrGrading.rxInit);
            LogCCGrading("    Diag TX Init Grading", 
                         firstLane, 
                         lastLane, 
                         lwrGrading.txInit);
            LogCCGrading("    Diag RX Maint Grading", 
                         firstLane, 
                         lastLane, 
                         lwrGrading.rxMaint);
            LogCCGrading("    Diag TX Maint Grading", 
                         firstLane, 
                         lastLane, 
                         lwrGrading.txMaint);                           
        }
    }

    return topoStr;
}
