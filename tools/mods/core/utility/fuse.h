/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2011, 2015-2019 by LWPU Corporation. All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDE_FUSE_H
#define INCLUDE_FUSE_H

#include "core/include/rc.h"
#include "core/include/fuseparser.h"
#include "core/include/fuseutil.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include <set>

class Device;
class FuseDataSet;

class OldFuse
{
public:
    enum Tolerance
    {
        ALL_MATCH = 0,
        REDUNDANT_MATCH = (1<<0),
        LWSTOMER_MATCH = (1<<1)
    };

    enum WhichFuses
    {
        BAD_FUSES  = (1<<0),
        GOOD_FUSES = (1<<1),
        ALL_FUSES  = (BAD_FUSES|GOOD_FUSES)
    };

    OldFuse(Device *pDevice);
    virtual ~OldFuse();

    OldFuse(const OldFuse&) = delete;
    OldFuse& operator=(const OldFuse&) = delete;

    //! \brief!  Go through the definitions and look for known chip skus
    //! that matches the current device
    RC FindSkuMatch(vector<string>* pFoundSkus, Tolerance Strictness);

    //! \brief! Only check whether the ATE fuses were blown correctly
    //! on the current device.
    RC CheckAteFuses(string ChipSku);

    RC CheckFuse(const string FuseName, const string Specs);

    struct FuseVals
    {
        string Name;
        UINT32 Attribute;
        string ExpectedStr;
        UINT32 ActualVal;
    };
    //! \brief given ChipSku, pull the related fuse names, fuse attributes
    // the expected value and the actual value.
    RC GetFusesInfo(string            ChipSku,
                    WhichFuses        WhatToGet,
                    Tolerance         Strictness,
                    UINT32            AttributeMask,
                    vector<FuseVals> *pRetInfo);

    //! \brief Prints fuse info on the current device
    // If given a chipsku, this function can print out what are the properly
    // burnt fuses, improperly burnt fuses, or ALL known fuse values
    // The third parameter determines how strict the fuse check is
    RC PrintFuseValues(string     ChipSku,
                       WhichFuses WhatToPrint,
                       Tolerance  Strictness,
                       UINT32     AttributeMask);

    //! \brief Given a chip SKU name, create an vector of registers that we
    // are suppose to have (for Raw fuse only)
    RC GetDesiredFuses(string ChipSku, vector<UINT32> *pRegsToBlow);

    enum OverrideFlags
    {
        AS_IS                   = 0,
        USE_SKU_FLOORSWEEP_DEF  = (1<<0), //
    };
    struct OverrideInfo
    {
        UINT32 Value;
        UINT32 Flag;    // OverrideFlags
    };

    RC FuseCompose(map<string, pair<UINT32, UINT32>> *pPseudoFuseMap,
                   map<string, UINT32> *pPseudoFuseValMap,
                   UINT32 *MasterFuseVal);

    RC FuseDecompose(map<string, pair<UINT32, UINT32>> *pPseudoFuseMap,
                     UINT32 MasterFuseVal,
                     map<string, UINT32> *pPseudoFuseValMap);

    RC FuseDecompose(map<string, pair<UINT32, UINT32>> *pPseudoFuseMap,
                     UINT32 MasterFuseVal,
                     string PseudoFuseName,
                     UINT32 *PseudoFuseVal);

    // ! Brief: mirror to UpdateRecord
    // ValToBlow:
    // pRegsToBlow:   Registers that are going to be set after setting the
    //              primary and redundant values
    RC UpdateColumnBasedFuse(const FuseUtil::FuseDef& fuseDef,
                             UINT32 ValToBlow,
                             vector<UINT32> *pRegsToBlow);

    // ! Brief: Given a fuse (from chip sku definition), this function sets the
    //          bits fuse registers that would be turned on
    // https://wiki.lwpu.com/gpunotebook/index.php/MODS/Fuse_XML
    // Info:          This tells us what the current fuse is supposed to be
    RC GetFuseValToBurn(const FuseUtil::FuseInfo &Info,
                        OverrideInfo *pOverride,
                        UINT32 *RegToBlow);

    Device* GetDevice(){return m_pDevice;};

    //! \brief: read info such as XML version
    RC GetMiscInfo(const FuseUtil::MiscInfo **pMiscInfo);

    //! \brief:  Set the fuses that we want to override (on top of the
    //! marketing requirement for each SKU)
    void SetOverrides(const map<string, OverrideInfo> &Overrides);

    void SetUseUndoFuse(bool UseUndoFuse) {m_UseUndoFuse = UseUndoFuse;};
    bool GetUseUndoFuse(){return m_UseUndoFuse;};

    virtual void PrintRecords();

    //! \brief: given a ChipSku
    RC RegenerateRecords(string ChipSku);
    virtual RC DumpFuseRecToRegister(vector<UINT32> *pFuseReg) = 0;
    virtual RC ReadInRecords() = 0;

    //! \brief: Given a ChipSku, try to apply the sku's IFF patches
    virtual RC ApplyIffPatches(string chipSku, vector<UINT32>* pRegsToBlow) = 0;
    virtual RC GetIffRecords(vector<FuseUtil::IFFRecord>* pRecords, bool keepDeadRecords)
        { return OK; }

    //! \brief: wrapper to get the fuse value when given name
    UINT32 GetFuseValGivenName(const string FuseName, Tolerance Strictness);

    UINT32 GetOptFuseValGivenName(const string FuseName);

    //! \brief Check whether a fuse is burnt correctly. 
    //! Also return the current Value (and if the fuse is supported)
    bool IsFuseRight(const FuseUtil::FuseInfo &Spec,
                     UINT32 *pGotValue,
                     Tolerance Strictness,
                     bool *pIsSupported);
    bool IsFuseRight(const FuseUtil::FuseInfo &Spec,
                     UINT32 *pGotValue,
                     Tolerance Strictness);

    //! \brief Check whether a fuse is correctly latched to its OPT register.
    //! Only checks the attached register's value, does not check the raw fuses.
    //! Also returns the current value of the fuse (and if the fuse is supported)
    bool IsOptFuseRight(const FuseUtil::FuseInfo &Spec,
                        UINT32 *pGotValue,
                        Tolerance Strictness,
                        bool *pIsSupported);
    bool IsOptFuseRight(const FuseUtil::FuseInfo &Spec,
                        UINT32 *pGotValue,
                        Tolerance Strictness);

    //! \brief Check if a pseudo fuse is set correctly. Also return the current Value
    bool IsPseudoFuseRight(const FuseUtil::FuseInfo &Spec,
                           UINT32 *pGotValue,
                           Tolerance Strictness);

    //! \brief: add to a list of fuses that will be skipped when IsFuseRight
    // is called
    void AppendIgnoreList(string FuseName);
    void ClearIgnoreList();

    // Add the fuse to the list of fuses which may use RIR overrides
    void RequestRirOnFuse(const string& fuseName)
        { m_UseRir.insert(Utility::ToUpperCase(fuseName)); }
    
    // Add the fuse to the list of fuses which may need RIR to be disabled
    void RequestDisableRirOnFuse(const string& fuseName)
        { m_DisableRir.insert(Utility::ToUpperCase(fuseName)); }

    //! \brief Pure virtual function. Report whether the platform supports a given fuse.
    virtual bool IsFuseSupported(const FuseUtil::FuseInfo &Spec, Tolerance Strictness);

    // good for unit tests:
    void ClearSimulatedFuses();
    void SetSimulatedFuses(const vector<UINT32> &FuseRegs);
    RC   GetSimulatedFuses(vector<UINT32> *pFuseRegs);
    bool AreSimFusesUsed(){return (m_SimFuseRegs.size() > 0);};
    void SetPrintPriority(Tee::Priority Pri) { m_PrintPri = Pri; }
    static void SetCheckInternalFuses(bool bCheck) { s_CheckInternalFuses = bCheck; }

protected:
    bool           m_FuseReadDirty = false;
    vector<UINT32> m_FuseReg;   // cached fuse registers

    set<string>    m_UseRir;          // Fuses for which RIR overrides have been requested
    set<string>    m_DisableRir;      // Fuses for which RIR disables have been requested
    set<string>    m_NeedRir;         // Fuses for which RIR has been requested and is needed
    set<string>    m_PossibleUndoRir; // Fuses for which RIR disable has been requested
                                      // and may require disabling

    enum CheckPriority
    {
        LEVEL_ALWAYS    = 0,
        LEVEL_LWSTOMER  = 1,
        LEVEL_ILWISIBLE = 2,
    };
    CheckPriority GetCheckLevel() const {return m_CheckLevel;};

    //! \brief Returns true if raw fuses aren't accessible, and functions checking
    //! fuse values should be restricted to the OPT fuses.
    virtual bool CanOnlyUseOptFuses() { return false; }

    //! \brief Method to cache the fuse registers on devices that can use raw
    // fuse access.
    virtual RC CacheFuseReg();

    //! \brief Get the fuse value given a definition (fuse name)
    virtual UINT32 ExtractFuseVal(const FuseUtil::FuseDef* pDef,
                                  Tolerance Strictness) = 0;

    //! \brief Get the fuse value given a definition by OPT_Fuse reads
    virtual UINT32 ExtractOptFuseVal(const FuseUtil::FuseDef* pDef) = 0;

    virtual bool HasOptFuse(const FuseUtil::FuseDef* pDef);
    virtual bool DoesOptAndRawFuseMatch(const FuseUtil::FuseDef* pDef,
                                        UINT32 RawFuseVal,
                                        UINT32 OptFuseVal) = 0;

    const FuseUtil::FuseDef* GetFuseDefFromName(string FuseName);
    const FuseUtil::SkuConfig* GetSkuSpecFromName(string Sku);

    // Add RIR records to "unfuse" bits in fuses which
    // requested RIR and had a 1->0 transition
    virtual RC AddRIRRecords(const FuseUtil::FuseDef& fuseDef, UINT32 valToBlow)
        { return RC::UNSUPPORTED_FUNCTION; }

    // Disable existing RIR records
    virtual RC DisableRIRRecords(const FuseUtil::FuseDef& fuseDef, UINT32 valToBlow)
        { return RC::UNSUPPORTED_FUNCTION; }

    OverrideInfo* GetOverrideFromName(string FuseName);
    Tee::Priority GetPrintPriority() { return m_PrintPri; }
    void* GetMutex() { return m_Mutex; }
private:
    bool IsFuseValueRight(const FuseUtil::FuseInfo &spec, UINT32 fuseVal);

    UINT32 GetCachedFuseSize() { return (UINT32)m_FuseReg.size(); };

    virtual RC ParseXmlInt(const FuseUtil::FuseDefMap **ppFuseDefMap,
                           const FuseUtil::SkuList    **ppSkuList,
                           const FuseUtil::MiscInfo   **ppMiscInfo,
                           const FuseDataSet          **pFuseDataSet) = 0;

    //! Set partial fuse values (for fuse blowing)
    virtual RC SetPartialFuseVal(const list<FuseUtil::FuseLoc> &FuseLocList,
                                 UINT32 DesiredValue,
                                 vector<UINT32> *pRegsToBlow) = 0;

    //! \brief! Wrapper to call FuseXml::ParseFile.
    //!  MCP/GPU have can have own way
    //! of naming XML file name based on device ID
    RC ParseXml(const FuseUtil::FuseDefMap **ppFuseDefMap,
                const FuseUtil::SkuList    **ppSkuList,
                const FuseUtil::MiscInfo   **ppMiscInfo,
                const FuseDataSet          **pFuseDataSet);

    RC HandleColumnBasedFuses(string ChipSku, vector<UINT32> *pRegsToBlow);

    virtual RC UpdateRecord(const FuseUtil::FuseDef &Def, UINT32 Value) = 0;

    Device                         *m_pDevice;     // device handle
    bool                            m_UseUndoFuse;
    bool                            m_Parsed;      // XML already parsed?
    CheckPriority                   m_CheckLevel;  // whether we want to check the fuse
    const FuseUtil::FuseDefMap     *m_pFuseDefMap;
    const FuseUtil::SkuList        *m_pSkuList;
    const FuseUtil::MiscInfo       *m_pMiscInfo;
    const FuseDataSet              *m_pFuseDataSet;
    map<string, OverrideInfo>       m_Overrides;
    set<string>                     m_IgnoredFuseNames;
    vector<UINT32>                  m_SimFuseRegs;
    Tee::Priority                   m_PrintPri;
    void                           *m_Mutex;
    static bool                     s_CheckInternalFuses;
};

#endif
