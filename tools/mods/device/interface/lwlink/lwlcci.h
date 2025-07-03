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

#pragma once

#include "core/include/types.h"
#include "core/include/rc.h"
#include <string>

// LwLink interface encapsulating cable controller functionality (e.g. optical
// modules on Delta2)
class LwLinkCableController
{
public:
    virtual ~LwLinkCableController() { }

    // Module media type from CMIS Rev 4.0 Specification, table 8-12
    enum class CCModuleMediaType : UINT08
    {
        Undefined
       ,OpticalMmf
       ,OpticalSmf
       ,PassiveCopper
       ,ActiveCables
       ,BaseT
       ,Reserved
       ,Custom
       ,Unknown
    };

    // Module identifiers from the SFF-8024 Rev 4.8 Specification, table 4-1
    enum class CCModuleIdentifier : UINT08
    {
        Unknown
       ,GBIC
       ,Soldered
       ,SFP
       ,XBI
       ,XENPAK
       ,XFP
       ,XFF
       ,XFP_E
       ,XPAK
       ,X2
       ,DWDM_SFP
       ,QSFP
       ,QSFPPlus
       ,CXP
       ,SMMHD4X
       ,SMMHD8X
       ,QSFP28
       ,CXP2
       ,CDFP_1_2
       ,SMMHD4XFanout
       ,SMMHD8XFanout
       ,CDFP_3
       ,MicroQSFP
       ,QSFP_DD
       ,OSFP8X
       ,SFP_DD
       ,DFSP
       ,MiniLinkX4
       ,MiniLinkX8
       ,QSFPPlusWithCMIS
       ,Reserved
       ,VendorSpecific
    };

    enum class CCInitStatus : UINT08
    {
        NotFound
       ,Failure
       ,Success
       ,Unknown
    };

    enum class CCFirmwareStatus : UINT08
    {
        Present
       ,Running
       ,Empty
       ,NotPresent
       ,Unknown
    };

    enum class CCFirmwareImage : UINT08
    {
        Factory
       ,ImageA
       ,ImageB
    };

    struct CCFirmwareInfo
    {
        CCFirmwareStatus status       = CCFirmwareStatus::Unknown;
        bool           bIsBoot        = false;
        UINT08         majorVersion   = 0;
        UINT08         minorVersion   = 0;
        UINT16         build          = 0;
    };

    struct GradingValues
    {
        UINT32 linkId;
        UINT08 laneMask;
        vector<UINT08> rxInit;
        vector<UINT08> txInit;
        vector<UINT08> rxMaint;
        vector<UINT08> txMaint;
    };

    struct CarrierFruInfo
    {
        string carrierPortNum;
        string carrierBoardMfgDate;
        string carrierBoardMfgName;
        string carrierBoardProductName;
        string carrierBoardSerialNum;
        string carrierBoardHwRev;
        string carrierBoardPartNum;
        string carrierBoardFruRev;
    };

    enum class ErrorFlagType : UINT08
    {
        ModuleFirmwareFault
       ,DataPathFirmwareFault
       ,HighTempAlarm
       ,LowTempAlarm
       ,HighVoltageAlarm
       ,LowVoltageAlarm
       ,Aux1HighAlarm
       ,Aux1LowAlarm
       ,Aux2HighAlarm
       ,Aux2LowAlarm
       ,Aux3HighAlarm
       ,Aux3LowAlarm
       ,VendorHighAlarm
       ,VendorLowAlarm
       ,HighTempWarning
       ,LowTempWarning
       ,HighVoltageWarning
       ,LowVoltageWarning
       ,Aux1HighWarning
       ,Aux1LowWarning
       ,Aux2HighWarning
       ,Aux2LowWarning
       ,Aux3HighWarning
       ,Aux3LowWarning
       ,VendorHighWarning
       ,VendorLowWarning
       ,LatchedDataPathStateChange
       ,TxFault
       ,TxLossOfSignal
       ,TxCDRLossOfLock
       ,TxAdaptiveInputEqFault
       ,TxOutputPowerHighAlarm
       ,TxOutputPowerLowAlarm
       ,TxBiasHighAlarm
       ,TxBiasLowAlarm
       ,RxLatchedLossOfSignal
       ,RxLatchedCDRLossOfLock
       ,RxInputPowerHighAlarm
       ,RxInputPowerLowAlarm
       ,TxOutputPowerHighWarning
       ,TxOutputPowerLowWarning
       ,TxBiasHighWarning
       ,TxBiasLowWarning
       ,RxInputPowerHighWarning
       ,RxInputPowerLowWarning
       ,Unknown
    };

    enum class ModuleState
    {
         Reserved
        ,LowPwr
        ,PwrUp
        ,Ready
        ,PwrDn
        ,Fault
        ,Unknown
    };

    static constexpr UINT08 CCI_ILWALID = 0xff;
    struct ErrorFlag
    {
        UINT08        cciId   = CCI_ILWALID;
        UINT08        lane    = CCI_ILWALID;
        ErrorFlagType errFlag = ErrorFlagType::Unknown;
    };
    typedef vector<ErrorFlag> CCErrorFlags;
    typedef vector<CarrierFruInfo> CarrierFruInfos;

    static constexpr UINT32 ILWALID_CCI_ID = ~0U;

    UINT64 GetBiosLinkMask() const
        { return DoGetCCBiosLinkMask(); }
    UINT32 GetCciIdFromLink(UINT32 linkId) const
        { return DoGetCciIdFromLink(linkId); }
    UINT32 GetCount() const
        { return DoGetCCCount(); }
    RC GetTempC(UINT32 cciId, FLOAT32 *pTemp) const
        { return DoGetCCTempC(cciId, pTemp); }
    RC GetErrorFlags(CCErrorFlags * pErrorFlags) const
        { return DoGetCCErrorFlags(pErrorFlags); }
    const CCFirmwareInfo & GetFirmwareInfo(UINT32 cciId, CCFirmwareImage image) const
        { return DoGetCCFirmwareInfo(cciId, image); }
    RC GetGradingValues(UINT32 cciId, vector<GradingValues> * pGradingValues) const
        { return DoGetCCGradingValues(cciId, pGradingValues); }
    UINT32 GetHostLaneCount(UINT32 cciId) const
        { return DoGetCCHostLaneCount(cciId); }
    string GetHwRevision(UINT32 cciId) const
        { return DoGetCCHwRevision(cciId); }
    RC GetCarrierFruEepromData(vector<CarrierFruInfo>* carrierInfo)
        { return DoGetCarrierFruEepromData(carrierInfo); }
    CCInitStatus GetInitStatus(UINT32 cciId) const
        { return DoGetCCInitStatus(cciId); }
    UINT64 GetLinkMask(UINT32 cciId) const
        { return DoGetCCLinkMask(cciId); }
    CCModuleIdentifier GetModuleIdentifier(UINT32 cciId) const
        { return DoGetCCModuleIdentifier(cciId); }
    UINT32 GetModuleLaneCount(UINT32 cciId) const
        { return DoGetCCModuleLaneCount(cciId); }
    CCModuleMediaType GetModuleMediaType(UINT32 cciId) const
        { return DoGetCCModuleMediaType(cciId); }
    RC GetCCModuleState(UINT32 cciId, ModuleState * pStateValue) const
        { return DoGetCCModuleState(cciId, pStateValue); }
    string GetPartNumber(UINT32 cciId) const
        { return DoGetCCPartNumber(cciId); }
    UINT32 GetPhysicalId(UINT32 cciId) const
        { return DoGetCCPhysicalId(cciId); }
    string GetSerialNumber(UINT32 cciId) const
        { return DoGetCCSerialNumber(cciId); }
    RC GetVoltage(UINT32 cciId, UINT16 * pVoltageMv) const
        { return DoGetCCVoltage(cciId, pVoltageMv); }
    const CarrierFruInfos & GetCarrierFruInfo() const
        { return DoGetCarrierFruInfo(); }
    static const char * ErrorFlagToString(ErrorFlagType errFlagType);
    static const char * FirmwareStatusToString(CCFirmwareStatus firmwareStatus);
    static const char * FirmwareImageToString(CCFirmwareImage firmwareImage);
    static const char * InitStatusToString(CCInitStatus initStatus);
    static bool IsErrorFlagFatal(ErrorFlagType errFlagType);
    static const char * ModuleIdentifierToString(CCModuleIdentifier mi);
    static const char * ModuleMediaTypeToString(CCModuleMediaType mmt);
    static const char * ModuleStateToString(ModuleState state);
    static string LinkGradingValuesToString(UINT32 linkId, 
        const vector<GradingValues>& gradingVals);

protected:
    static constexpr time_t CARRIER_BASE_START_EPOCH                = 820454400; //1/1/96 at 00:00
    static constexpr UINT08 CARRIER_MFG_DATE_BYTE_OFFSET            = 11;
    static constexpr UINT08 CARRIER_MFG_NAME_BYTE_OFFSET            = 14;
    static constexpr UINT08 CARRIER_BOARD_NAME_BYTE_OFFSET          = 21;
    static constexpr UINT08 CARRIER_BOARD_SERIAL_BYTE_OFFSET        = 31;
    static constexpr UINT08 CARRIER_BOARD_HWREV_PN_BYTE_OFFSET      = 45;
    static constexpr UINT08 CARRIER_BOARD_FRU_BYTE_OFFSET           = 68;
    static constexpr UINT08 CARRIER_BOARD_DATA_LENGTH_MASK          = 0x3f;

private:
    virtual UINT64 DoGetCCBiosLinkMask() const = 0;
    virtual UINT32 DoGetCCCount() const = 0;
    virtual UINT32 DoGetCciIdFromLink(UINT32 linkId) const = 0;
    virtual RC DoGetCCTempC(UINT32 cciId, FLOAT32 *pTemp) const = 0;
    virtual RC DoGetCCErrorFlags(CCErrorFlags * pErrorFlags) const = 0;
    virtual const CCFirmwareInfo & DoGetCCFirmwareInfo(UINT32 cciId, CCFirmwareImage image) const = 0;
    virtual RC DoGetCCGradingValues(UINT32 cciId, vector<GradingValues> * pGradingValues) const = 0;
    virtual UINT32 DoGetCCHostLaneCount(UINT32 cciId) const = 0;
    virtual string DoGetCCHwRevision(UINT32 cciId) const = 0;
    virtual RC DoGetCarrierFruEepromData(vector<CarrierFruInfo>* carrierInfo) = 0;
    virtual CCInitStatus DoGetCCInitStatus(UINT32 cciId) const = 0;
    virtual UINT64 DoGetCCLinkMask(UINT32 cciId) const = 0;
    virtual CCModuleIdentifier DoGetCCModuleIdentifier(UINT32 cciId) const = 0;
    virtual UINT32 DoGetCCModuleLaneCount(UINT32 cciId) const = 0;
    virtual CCModuleMediaType DoGetCCModuleMediaType(UINT32 cciId) const = 0;
    virtual RC DoGetCCModuleState(UINT32 cciId, ModuleState * pStateValue) const = 0;
    virtual string DoGetCCPartNumber(UINT32 cciId) const = 0;
    virtual UINT32 DoGetCCPhysicalId(UINT32 cciId) const = 0;
    virtual string DoGetCCSerialNumber(UINT32 cciId) const = 0;
    virtual RC DoGetCCVoltage(UINT32 cciId, UINT16 * pVoltageMv) const = 0;
    virtual const CarrierFruInfos & DoGetCarrierFruInfo() const = 0;
    virtual RC InitializeCableController() = 0;
};
