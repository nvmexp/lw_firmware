/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/fuseutil.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/fuse/fuse.h"

#include <map>

#include "fsset.h"

//!
//! \brief  Main class for downbinning a chip to meet a SKU's requirements
//!
class DownbinImpl
{
    using FsUnit = Downbin::FsUnit;
    using FuseMaskType = Downbin::FuseMaskType;
    using Settings = Downbin::Settings;

public:
    explicit DownbinImpl(TestDevice *pDev);
    virtual ~DownbinImpl() = default;

    //!
    //! \brief  Set pointer back to the main fuse object
    //!
    void SetParent(Fuse *pFuse);

    //!
    //! \brief Apply downbinning to meet the given SKU requirements
    //!
    //! \param skuName  Target SKU name
    //!
    RC DownbinSku(const string &skuName);

    //!
    //! \brief  Print results of the MODS floorsweeping script
    //!
    RC DumpFsScriptInfo();

    //!
    //! \brief Get the global downbin related settings
    //!
    const Settings* GetSettings() const { return &m_DownbinSettings; }

    //!
    //! \brief Get FS result script file name
    //!
    string GetFsResultFile() const;
    //!
    //! \brief Set a FS result script file name to override the default
    //!
    void SetFsResultFile(const string& fileName) { m_FsResultFile = fileName; }

    RC SetDownbinInfo(Downbin::DownbinInfo downbinInfo)
    {
        m_DownbinInfo = downbinInfo;
        return RC::OK;
    }

    //!
    //! \brief Get FuseValues only related to floorsweeping
    //!
    RC GetOnlyFsFuseInfo(FuseValues* pRequest, const string& fsFusingOpt);

protected:
    //!
    //! \brief  Retrieve downbin settings for a given fsunit for the SKU
    //!
    //! \param skuName        Name of SKU being downbinned to
    //! \param fsUnit         Floorsweeping unit to get config for
    //! \param[out] pConfig   Config details
    //!
    RC GetUnitDownbinConfig
    (
        const string &skuName,
        FsUnit fsUnit,
        FuseUtil::DownbinConfig *pConfig
    );

    //!
    //! \brief  Add chips-specific floorsweeping rules to FS sets
    //!
    virtual RC AddChipFsRules();

    //!
    //! \brief  Apply chip / SKU rules post downbinning
    //!
    //! \param skuName        Name of SKU being downbinned to
    //!
    virtual RC ApplyPostDownbinRules(const string & skuName);

    //!
    //! \brief Add downbin pickers to GPC set
    //! \param skuName        Name of SKU being downbinned to
    //!
    virtual RC AddGpcDownbinPickers(const string &skuName);
    //!
    //! \brief Add downbin pickers to FBP set
    //!
    //! \param skuName        Name of SKU being downbinned to
    //!
    virtual RC AddFbpDownbinPickers(const string &skuName);

    //!
    //! \brief Add Misc DownbinPickers
    //!
    virtual RC AddMiscDownbinPickers();

    //!
    //! \brief Add highest priority pickers for protected bits
    //!
    //! \param skuName        Name of SKU being downbinned to
    //!
    virtual RC AddProtectedBitsGDPickers(const string& skuName);
    //!
    //! \brief Add highest priority pickers for protected bits
    //!
    //! \param pFsSet   Add the element pickers to this fsSet
    //! \param skuName  Name of SKU being downbinned to
    //!
    virtual RC AddProtectedBitsEDPickers
    (
        FsSet *pFsSet,
        const string &skuName
    );
    //!
    //! \brief Add group reduce pickers for protected bits
    //!
    //! \param pFsSet Add the group reduce pickers to this fsSet
    //! \param skuName        Name of SKU being downbinned to
    //!
    virtual RC AddProtectedBitsGRPickers
    (
        FsSet *pFsSet,
        const string & skuName
    );

    //!
    //! \brief Add basic group disable and group reduce pickers
    //!
    //! \param pfsset         Add basic group disable pickers to this fs set
    //! \param elementFsUnit  Add picker using this element from the fs set
    //!
    virtual RC AddMostDisabledGDPickers
    (
        FsSet *pFsSet,
        FsUnit elementFsUnit
    );
    //!
    //! \brief Add basic group disable and group reduce pickers
    //!
    //! \param pfsset   add basic group disable pickers to this fs set
    //! \param elementFsUnit  Add picker using this element from the fs set
    //!
    virtual RC AddMostAvailableGRPickers
    (
        FsSet *pFsSet,
        FsUnit elementFsUnit
    );

    //!
    //! \brief Add group disable pickers for GPC set
    //! \param skuName        Name of SKU being downbinned to
    //!
    virtual RC AddGpcSetGroupDisablePickers(const string &skuName);
    //!
    //! \brief Add group reduce pickers for GPC set
    //! \param skuName        Name of SKU being downbinned to
    //!
    virtual RC AddGpcSetGroupReducePickers(const string &skuName);
    //! \brief Add element disable pickers for GPC set
    //! \param skuName        Name of SKU being downbinned to
    //!
    virtual RC AddGpcSetElementDisablePickers(const string &skuName);

    //!
    //! \brief Add group disable pickers for FBP set
    //! \param skuName        Name of SKU being downbinned to
    //!
    virtual RC AddFbpSetGroupDisablePickers(const string &skuName);
    //! \brief Add group reduce pickers for FBP set
    //! \param skuName        Name of SKU being downbinned to
    //!
    virtual RC AddFbpSetGroupReducePickers(const string &skuName);
    //! \brief Add element disable pickers for FBP set
    //! \param skuName        Name of SKU being downbinned to
    //!
    virtual RC AddFbpSetElementDisablePickers(const string &skuName);
    //! \brief Add Element disable picker to select LTC in a FBP based on enabled L2SLICEs
    //!
    RC AddLtcL2SliceDependencyEDPicker();
    //!
    //! \brief Add Group Reduce picker to select FBP for disabling LTC based on L2SLICES
    //!
    RC AddLtcL2SliceDependencyGRPicker();

    //!
    //! \brief check if an fsUnit is protected i.e has as non empty must enable list
    //!
    //! \param fsUnit  fsUnit to be checked if it is protected
    //! \param skuName        Name of SKU being downbinned to
    //! \param pIsProtected   Set to true if element is protected
    //!
    RC CheckMustEnableList
    (
        FsUnit fsUnit,
        const string &skuName,
        bool *pIsProtected
    );

    // Map of all floorsweeping sets for the chip
    map<FsUnit, FsSet> m_FsSets;

    Downbin::DownbinInfo m_DownbinInfo;

private:
    //!
    //! \brief Setup downbinning
    //!
    RC Initialize();
    //!
    //! \brief Create floorsweeping sets for downbinning
    //!
    RC CreateFsSets();

    //!
    //! \brief Add min group count for given FS elements
    //!
    //! \param fsSet       FS set the element belongs to
    //! \param groupIdx    Group index in the FS set the element set belongs to
    //! \param fsUnit      Floorsweeping unit to add the minimum count for
    //! \param minCount    Minimum count per group
    //!
    void AddMinGroupCountRule
    (
        FsSet &fsSet,
        UINT32 groupIdx,
        FsUnit fsUnit,
        UINT32 minCount
    );
    //!
    //! \brief Add element dependency downbin rules for given FS elements
    //!
    //! \param pFsGroup    FS group the rule must be added to
    //! \param element     FS unit representing the element
    //! \param subElement  FS unit representing the sub-element
    //!
    void AddElementDependencyRule
    (
        FsGroup *pFsGroup,
        FsUnit element,
        FsUnit subElement
    );

    //!
    //! \brief Print current contents of all FS sets
    //!
    //! \param pri Print priority for these prints
    //!
    void PrintAllSets(Tee::Priority pri) const;

    //!
    //! \brief Get fuse suffix for the given floorsweeping unit and mask type
    //!        (Eg. "_defective", "_enable")
    //!
    //! \param pFsSet       Pointer to the fs set the floorsweeping unit belongs to
    //! \param fsUnit       Floorsweeping unit to get suffix for
    //! \param type         Fuse Mask type
    //! \param[out] pPrefix The fuse prefix
    //! \param[out] pSuffix The fuse suffix
    //!
    RC GetFusePrefixSuffix
    (
        const FsSet &fsSet,
        FsUnit fsUnit,
        FuseMaskType type,
        string* pPrefix,
        string* pSuffix
    ) const;
    //!
    //! \brief Get fuse names for the given floorsweeping unit and mask type
    //!        The fuses must be per-group fuses
    //!
    //! \param pFsSet       Pointer to the fs set the floorsweeping unit belongs to
    //! \param fsUnit       Floorsweeping unit to get names for
    //! \param type         Fuse Mask type
    //! \param[out] pFuseNames Vector of fuse names to populate
    //!
    RC GetElementFuseNames
    (
        const FsSet &fsSet,
        FsUnit fsUnit,
        FuseMaskType type,
        vector<string> *pFuseNames
    ) const;
    //!
    //! \brief Get fuse names for the given floorsweeping unit and mask type
    //!        The fuses must be non per-group fuses
    //!
    //! \param pFsSet       Pointer to the fs set the floorsweeping unit belongs to
    //! \param fsUnit       Floorsweeping unit to get names for
    //! \param type         Fuse Mask type
    //! \param[out] pFuseName Fuse names for the given unit
    //!
    RC GetFuseName
    (
        const FsSet &fsSet,
        FsUnit fsUnit,
        FuseMaskType type,
        string *pFuseName
    ) const;

    // Store FS script results
    struct FsSection
    {
        string Name;       // Section Name
        string StartTime;  // Start time for section evaluation
        string EndTime;    // End time for section evaluation
        map<string, vector<UINT32>> FuseVals; // Map of fuse and values
    };
    //!
    //! \brief Parse out floorsweeping script results from the given file
    //!
    //! \param resultFile       Name of JSON file to parse
    //! \param[out] pFsSections Pointer to map to be filled with parsed results
    //!
    static RC ParseFsSections(const string &resultFile, map<string, FsSection> *pFsSections);
    //!
    //! \brief Add FS script section to MLE
    //!
    //! \param fsSection FS result object to add
    //!
    static RC AddFsSectionToMle(const FsSection& fsSection);
    //!
    //! \brief Get FS script sections
    //!
    //! \param bIsRequired       FS script is required to exist
    //! \param[out] pFsSections  Pointer to map to be filled with parsed results
    //! \param[out] pFoundScript If FS script was found or not
    //! \param[out] pExitCode    Exit code reported by FS script
    //!
    RC GetFsScriptInfo
    (
        bool bIsRequired,
        map<string, FsSection> *pFsSections,
        bool *pFoundScript,
        RC *pExitCode
    ) const;
    //!
    //! \brief Apply given fs section results to FS sets
    //!
    //! \param fsSections        Map of parsed FS script results
    //! \param bModifyDefective  Modify defective masks while importing info
    //!
    RC ApplyFsScriptInfo(const map<string, FsSection> &fsSections, bool bMarkDefective);

    //!
    //! \brief  Import defective mask information from the chip and apply them to
    //!         the floorsweeping sets
    //!
    //! \param bModifyDefective  Modify defective masks while importing info
    //!
    RC ImportGpuFsInfo(bool bModifyDefective);

    //!
    //! \brief  Import results from the MODS floorsweeping script and apply them to
    //!         the floorsweeping sets
    //!
    //! \param bModifyDefective  Modify defective masks while importing info
    //!
    RC ImportFsScriptInfo(bool bModifyDefective);

    //!
    //! \brief  Add basic floorsweeping rules to FS sets
    //!
    RC AddBasicFsRules();
    //!
    //! \brief  Add basic floorsweeping rules to GPC set
    //!
    RC AddBasicGpcSetRules();
    //!
    //! \brief  Add basic floorsweeping rules to FBP set
    //!
    RC AddBasicFbpSetRules();

    //!
    //! \brief  Add SKU-specific floorsweeping rules to FS sets
    //!
    //! \param skuName   Name of SKU whose settings must be applied
    //!
    RC AddSkuFsRules(const string &skuName);

    //!
    //! \brief Add downbin pickers to FS sets
    //!
    //! \param fuseConfig  map of downbin config for various fsUnits as per current SKU
    //!
    RC AddDownbinPickers
    (
       const string &skuName
    );

    //!
    //! \brief  Add floorsweeping rules based on config for the given FS unit
    //!
    //! \param pFsSet     FS set the given unit belongs to
    //! \param fsUnit     Floorsweeping unit to apply settings for
    //! \param downbinConfig   Downbinconfig for given FS unit
    //!
    RC ApplyFsConfigRules
    (
        FsSet *pFsSet,
        FsUnit fsUnit,
        const FuseUtil::DownbinConfig &downbinConfig
    );
    //!
    //! \brief  Add floorsweeping rules related to SKU element count guarentees for the given
    //!         FS unit
    //!
    //! \param pFsSet     FS set the given unit belongs to
    //! \param fsUnit     Floorsweeping unit to apply settings for
    //! \param downbinConfig   Downbinconfig for given FS unit
    //!
    RC ApplyElementCountRules
    (
        FsSet *pFsSet,
        FsUnit fsUnit,
        const FuseUtil::DownbinConfig &downbinConfig
    );
    //!
    //! \brief  Add floorsweeping rules related to SKU must-enable list for the given FS unit
    //!
    //! \param pFsSet     FS set the given unit belongs to
    //! \param fsUnit     Floorsweeping unit to apply settings for
    //! \param downbinConfig   Floorsweeping config for given FS unit
    //!
    RC ApplyMustEnableListRules
    (
        FsSet *pFsSet,
        FsUnit fsUnit,
        const FuseUtil::DownbinConfig &downbinConfig
    );
    //!
    //! \brief  Add floorsweeping rules related to SKU must-disable list for the given FS unit
    //!
    //! \param pFsSet     FS set the given unit belongs to
    //! \param fsUnit     Floorsweeping unit to apply settings for
    //! \param downbinConfig   Floorsweeping config for given FS unit
    //!
    RC ApplyMustDisableListRules
    (
        FsSet *pFsSet,
        FsUnit fsUnit,
        const FuseUtil::DownbinConfig &downbinConfig
    );

    //!
    //! \brief Add graphic TPC SKU config rules for GPC set
    //!
    //! \param pGpcSet       GPC set to apply the rules to
    //! \param tpcConfig     Floorsweeping config for TPCs
    //!
    RC ApplyGraphicsTpcsFsConfigRules
    (
        FsSet *pGpcSet,
        const FuseUtil::DownbinConfig &tpcConfig
    );
    //!
    //! \brief  Add HBM site SKU config rules for FBP set
    //!
    //! \param pFbpSet       FBP set to apply the rules to
    //! \param sliceConfig   Floorsweeping config for L2 slice
    //!
    RC ApplyHbmSiteFsConfigRules
    (
        FsSet *pFbpSet,
        const FuseUtil::DownbinConfig &fbpConfig
    );
    //!
    //! \brief  Add LTC-L2 slice SKU config rules for FBP set
    //!
    //! \param pFbpSet       FBP set to apply the rules to
    //! \param sliceConfig   Floorsweeping config for L2 slice
    //!
    RC ApplyLtcSliceFsConfigRules
    (
        FsSet *pFbpSet,
        const FuseUtil::DownbinConfig &sliceConfig
    );
    //!
    //! \brief  Add vGpc Skyline Rule
    //!
    //! \param pGpcSet       GPC set to apply the rules to
    //! \param tpcConfig     Floorsweeping config for TPC
    //!
    RC ApplyVGpcSkylineRule
    (
        FsSet *pGpcSet,
        const FuseUtil::DownbinConfig &tpcConfig
    );
    //!
    //! \brief  Add floorsweeping rules related to misc SKU fields for the given FS set
    //!
    //! \param pFsSet          FS set to apply the rules to
    //! \param fsUnit          Fs unit the downbin config is for
    //! \param downbinConfig   Floorsweeping config for given FS unit
    //!
    RC ApplyMiscFsConfigRules
    (
        FsSet *pFsSet,
        FsUnit fsUnit,
        const FuseUtil::DownbinConfig &downbinConfig
    );

    //!
    //! \brief Apply fuse overrides which can be picked up while fusing for the given mask type
    //!
    //! \param maskType  Fuse mask type
    //!
    RC ApplyFuseOverrides(FuseMaskType maskType);

    //!
    //! \brief Extract information in FsSets and output FuseValues
    //!
    //! \param maskType      Fuse Mask Type: DEFECTIVE/DISABLE/RECONFIG
    //! \param[out] pRequest Pointer to the FuseValues output
    //! \param fsFusingOpt   fsFusing option from user's commandline: gpc/fb/all
    //!
    RC GetFuseValuesFromFsSets(FuseMaskType maskType, FuseValues *pRequest, const string& fsFusingOpt);

    //!
    //! \brief  Verify that the final downbin config is functionally valid and
    //!         matches the given SKU
    //!
    //! \param skuName  Target SKU name
    //!
    RC VerifyDownbinConfig(const string& skuname);

    //!
    //! \brief  Check if the floorsweeping config is valid
    //!
    RC IsFsValid(FsLib::FsMode fsMode);

    //!
    //! \brief  Get GPC Mask, for FsLib to take
    //!
    RC GetFsLibGpcMask(FsLib::GpcMasks *pGpcMasks) const;

    //!
    //! \brief  Get FBP Mask, for FsLib to take
    //!
    RC GetFsLibFbpMask(FsLib::FbpMasks *pFbpMasks) const;

    //!
    //! \brief Extract information in FsSets and output fuseMap (map<string, UINT32>)
    //!
    //! \param[out] pFuseMap  map<fuseName, value>
    //! \param[in] maskType Fuse Mask Type: DEFECTIVE/DISABLE/RECONFIG
    //! \param[in] desiredFsUnit Desired FS Unit
    //!
    RC GetFuseMapFromFsSets
    (
        map<string, UINT32>* fuseMap,
        FuseMaskType maskType,
        FsUnit desiredFsUnit
    ) const;

    //!
    //! \brief Run Actual Downbin
    //!
    //! \param[in] pFsSet  The FsSet that needs Downbin
    //! \param[in] fuseConfig fuseConfig map containing the FsUnit string and the DownbinConfig
    //!
    RC DownbinSet
    (
        FsSet* pFsSet,
        const map<string, FuseUtil::DownbinConfig>& fuseConfig
    );

    //!
    //! \brief downbin Syspipe Set
    //! \param syspipeConfig DownbinConfig for Syspipe Set
    virtual RC DownbinSyspipeSet(const FuseUtil::DownbinConfig &syspipeConfig);

    // Pointer back to the main fuse object so it can perform
    // common fuse actions
    Fuse *m_pFuse = nullptr;

    // If the downbinning details have been set up
    bool m_bIsInitialized = false;

    // Pointer to the test device object
    TestDevice *m_pDev = nullptr;

    // Downbinning implementation related settings
    Settings m_DownbinSettings;

    // Stores FS result JSON file name
    string m_FsResultFile;

    // List of SKUs for the chip and associated details
    const FuseUtil::SkuList* m_pSkuList = nullptr;

    // Tags for the FS result JSON file
    static constexpr const char* JSON_SECTION_GPU_INFO = "CreateGpuInfo";
    static constexpr const char* JSON_SECTION_FINAL = "final";
    static constexpr const char* JSON_VALUE_NAME = "name";
    static constexpr const char* JSON_VALUE_TIME_START = "time_begin";
    static constexpr const char* JSON_VALUE_TIME_END = "time_end";
    static constexpr const char* JSON_VALUE_EXIT_CODE = "exit code";
    static constexpr const char* JSON_SECTION_RESULT = "result";

    // map to indicate if we need a groupReduce or element picker for protected elements
    map<FsUnit, bool> m_MustEnElemPickerMap;
    // map to indicate if we need a groupReduce or element picker for protected elements
    map<FsUnit, vector<UINT32>> m_MustEnElemMap;

    // Has provision for populating board level floorsweeping fuses
    bool m_HasBoardFsFuses = false;
};
