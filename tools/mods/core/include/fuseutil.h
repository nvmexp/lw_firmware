/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file FuseUtil provides types and enums used by MODS fuse code.
//!       Avoid putting XML or JSON specific stuff here.

#pragma once
#ifndef INCLUDED_FUSEUTIL_H
#define INCLUDED_FUSEUTIL_H

#include "types.h"
#include <list>
#include <string>
#include <vector>
#include <map>

class RC;

namespace FuseUtil
{
    enum
    {
        INFO_ATTRIBUTE,
        INFO_STR_VAL,
        INFO_VAL,
        INFO_VAL_LIST
    };

    enum PorBinType
    {
        PORBIN_SOC,
        PORBIN_CPU,
        PORBIN_GPU,
        PORBIN_CV,
        PORBIN_FSI
    };

    const UINT32 UNKNOWN_OFFSET = 0xFFFFFFFF;

    const UINT32 UNKNOWN_ECC_TYPE = 0xFFFFFFFF;
    const UINT32 UNKNOWN_RELOAD_TYPE = 0xFFFFFFFF;

    const string TYPE_PSEUDO = "pseudo";

    enum FuseAttribute
    {
        FLAG_READONLY = (1<<0),         // this fuse cannot be blown - just check
        FLAG_DONTCARE = (1<<1),         // this is a don't care
        FLAG_BITCOUNT = (1<<2),         // for BC:0x2 : see if num bits match  -eg number of TPCs
        FLAG_MATCHVAL = (1<<3),         // check if (fuseValu & Mask) is true
        FLAG_UNDOFUSE = (1<<4),
        FLAG_ATE      = (1<<5),         // fuse blowing in ATE - check for a range of values
        FLAG_ATE_NOT  = (1<<6),         // fuse blown in ATE - check for fuse ISn't this value
        FLAG_ATE_RANGE= (1<<7),         // fuse blown in ATE - check the range
                                        // Nothing for now
        FLAG_FUSELESS = (1<<9),         // 'fuseless' fuse type
        FLAG_PSEUDO   = (1<<10),        // 'pseudo' fuse type
        FLAG_TRACKING = (1<<11),        // master fuse tracking pseudo fuses
        FLAG_ALLTYPES = 0xFFFFFFFF
    };

    struct FuseLoc
    {
        UINT32 Number;
        UINT32 Lsb;
        UINT32 Msb;
        UINT32 ChainId;
        UINT32 ChainOffset;
        UINT32 DataShift;
        UINT32 ReadValue;

        bool operator==(const FuseLoc& rhs) const;
        bool operator!=(const FuseLoc& rhs) const;
    };

    struct OffsetInfo
    {
        UINT32 Offset = UNKNOWN_OFFSET;
        UINT32 Lsb = 0; // first bit to check
        UINT32 Msb = 0; // last bit to check

        bool operator==(const OffsetInfo& rhs) const;
        bool operator!=(const OffsetInfo& rhs) const;
    };

    // this stores the information for one particular fuse
    struct FuseDef
    {
        string Name;
        string Type;
       // isInFpfMacro will not be set for Ampere+ which supports merging of fuse and FPF macro
       // Though, it will still be set for previous chips with separate FPF macro
        bool isInFpfMacro = false;

        // kind of like Tee::PriLevel, but for fuse checking
        // [whether or not to check the fuse]
        UINT32 Visibility = 0;

        UINT32 NumBits = 0;
        OffsetInfo PrimaryOffset;
        list<FuseLoc> FuseNum;
        list<FuseLoc> RedundantFuse;

        // for pseduo fuse, it stores master fuse bit-fields it's mapped to
        // FuseDef, upper-bound, lower-bound
        vector<tuple<FuseDef*, UINT32, UINT32>> subFuses;

        // for master fuse, it stores all pseudo fuses mapped to it
        // pseudo fuse name, <upper-bound, lower-bound>
        map<string, pair<UINT32, UINT32>> PseudoFuseMap;

        UINT32 HammingECCType = UNKNOWN_ECC_TYPE;
        UINT32 Reload = UNKNOWN_RELOAD_TYPE;

        bool operator==(const FuseDef& rhs) const;
        bool operator!=(const FuseDef& rhs) const;
    };

    struct BRPatchRec
    {
        UINT32 Addr;
        UINT32 Value;
    };

    // Bootrom patch records for a particular revision are stored here
    struct BRRevisionInfo
    {
        string VerName;      // name of the BRPatch revision
        string VerNo;        // sequence num of the revision
        string PatchSection; // target_section of the revision
        string PatchVersion; // target_version of the revision
        list<BRPatchRec> BRPatches; // BootRom Patches for a given revision
    };

    struct PscPatchRec
    {
        UINT32 Addr;
        UINT32 Value;
    };

    // PscRom patch records for a particular revision are stored here
    struct PscRevisionInfo
    {
        string VerName;      // name of the PscRom Patch revision
        string VerNo;        // sequence num of the revision
        string PatchSection; // target_section of the revision
        string PatchVersion; // target_version of the revision
        list<PscPatchRec> PscPatches; // PscRom Patches for a given revision
    };

    struct IFFRecord
    {
        UINT32 data = 0x0;
        bool dontCare = false;

        IFFRecord() = default;
        IFFRecord(UINT32 data) : data(data) {}

        bool operator==(const IFFRecord& other) const
        {
            return dontCare || other.dontCare || data == other.data;
        }
    };

    // IFF patches are stored here
    struct IFFInfo
    {
        string name;     // name of the BRPatch revision
        string num;       // sequence num of the revision
        list<IFFRecord> rows; // BootRom Patches for a given revision
    };

    // Depending on the fuse FuseAttribute, different portions
    // of FuseInfo could be used
    struct FuseInfo
    {
        string        StrValue;     // fuse value in string as retrieved from fuse file
        UINT32        Value = 0;    // fuse value
        struct Range
        {
            UINT32 Min = 0;
            UINT32 Max = 0;
        };
        vector<Range>  ValRanges;     // for ATE_RangeHex
        vector<UINT32> Vals;          // list of values for the specified attribute
        const FuseDef  *pFuseDef;     // pointer to definition of a Fuse (type/location/name, etc)
        INT32          Attribute = 0; // this is a flag to determine how a fuse is treated

        // Additional options
        INT32 maxBurnPerGroup = -1;         // Maximum disabled TPC/L2 allowed per active GPC/FPB
        bool hasTestVal = false;            // Flag to SW Fuse a different value in SLT testing
        UINT32 testVal = 0;                 // Alternative value to use for SLT testing
        vector<UINT32> protectedBits;       // Bits which should not be disabled
    };

    // Different groups of chip options are protected by different Hamming
    // codewords. Maintaining the boundaries of the groups in this struct
    struct EccInfo
    {
        UINT32 EccType;
        UINT32 FirstWord;
        UINT32 LastWord;
        UINT32 FirstBit;
        UINT32 LastBit;
    };

    struct AlphaLine
    {
        AlphaLine() = default;

        FLOAT64 bVal = 0;
        FLOAT64 cVal = 0;
    };

    struct PorBin
    {
        bool hasAlpha = false;
        AlphaLine alphaLine;
        FLOAT64 speedoMin = 0;
        FLOAT64 speedoMax = 0;
        FLOAT64 iddqMax = 0;
        FLOAT64 iddqMin = 0;
        INT32 minSram = 0;
        INT32 maxSram = 0;
        FLOAT64 kappaVal = 0;
        UINT32 subBinId = 0;
        FLOAT64 hvtSpeedoMin = 0;
        FLOAT64 hvtSpeedoMax = 0;
        FLOAT64 hvtSvtRatioMin = 0;
        FLOAT64 hvtSvtRatioMax = 0;
    };

    const UINT32 UNKNOWN_ENABLE_COUNT = 0xFFFFFFFF;
    // TODO remove FsConfig and replace FsConfig with DownbinConfig everywhere
    struct FsConfig
    {
        UINT32 enableCount = 0;
        UINT32 enableCountPerGroup = 0;
        vector<UINT32> mustEnableList;
        vector<UINT32> mustDisableList;
        UINT32 minEnablePerGroup = 0;
        bool hasReconfig = false;
        UINT32 reconfigMinCount = 0;
        UINT32 reconfigMaxCount = 0;
        UINT32 reconfigLwrCount = 0;
        UINT32 repairMinCount = 0;
        UINT32 repairMaxCount = 0;
        UINT32 minGraphicsTpcs = 0;
        bool fullSitesOnly = false;
        UINT32 maxUnitUgpuImbalance = ~0U;
        vector<UINT32> vGpcSkylineList;
    };

    struct SkuConfig
    {
        UINT32 skuId = 0;           // chip SKU id, unique to the fuse file
        string SkuName;             // chip SKU's name
        string checksum;            // checksum across all the bootrom revisions
        list<FuseInfo>FuseList;
        map<string, FsConfig> fsSettings;
        list<BRRevisionInfo>BRRevs;  // bootrom revisions are stored here
        list<PscRevisionInfo>PscRomRevs;  // PscRom revisions are stored here
        list<IFFInfo> iffPatches;
        map<PorBinType, list<PorBin>> porBinsMap; // map of porBinType to subbins
    };

    struct P4Info
    {
        UINT32 P4Revision;
        string P4Str;
        string GeneratedTime;
        string ProjectName;
        UINT32 Version;
        string Validity;
    };

    struct FuseMacroInfo
    {
        UINT32 FuseRows;
        UINT32 FuseCols;
        UINT32 FuseRecordStart;
    };

    // Counters for a floorsweeping configuration
    struct FloorsweepingUnits
    {
        bool populated        = false;
        UINT32 gpcCount       = 0;
        UINT32 tpcCount       = 0;
        UINT32 tpcsPerGpc     = 0;
        UINT32 ropCount       = 0;
        UINT32 ropsPerGpc     = 0;
        UINT32 fbpCount       = 0;
        UINT32 fbpaCount      = 0;
        UINT32 l2Count        = 0;
        UINT32 l2sPerFbp      = 0;
        UINT32 l2SliceCount   = 0;
        UINT32 l2SlicesPerLtc = 0;
    };

    // Map of HammingEccType to EccInfo
    typedef map<UINT32, EccInfo> HammingEccList;

    struct MiscInfo
    {
        UINT32  FuselessStart;
        UINT32  FuselessEnd;
        P4Info  P4Data;
        map<string,string> McpInfo;
        HammingEccList EccList;
        FuseMacroInfo MacroInfo;
        FuseMacroInfo fpfMacroInfo;
        FloorsweepingUnits perfectCounts;
        vector<AlphaLine> alphaLines;
        map<string, vector<string>> tagMap;   // HW tags. Used for MATHS Link
    };

    struct DownbinConfig
    {
        // Flag for whether or not to use disable count or match values
        bool isBurnCount = true;

        // Disable count for this unit (-1 means "Don't Care")
        INT32 disableCount = -1;
        UINT32 enableCount = UNKNOWN_ENABLE_COUNT;

        // For GPC/FBP/FBPA/L2, this is a list of valid values
        // For TPC, these are the per-GPC masks
        vector<UINT32> matchValues;

        // List of bits which must be enabled/disabled
        vector<UINT32> mustEnableList;
        vector<UINT32> mustDisableList;

        // Exact enable count per group (-1 means "Don't care")
        INT32 enableCountPerGroup = -1;

        // For maximum disabled units allowed in an enabled group
        // Similar to maxBurnPerGroup, but for elements that don't have a perfect count
        // specified in the fuse file
        UINT32 minEnablePerGroup = 0;

        // For TPC, the maximum disabled units allowed in an enabled GPC
        INT32 maxBurnPerGroup = -1;

        // Mask for TPC bits which must be enabled if the GPC is enabled
        UINT32 groupIlwalidIfBurned = 0;

        // For TPC, whether or not _reconfig fuses should be blown
        // and the final TPC disable count after reconfig
        bool hasReconfig = false;
        UINT32 reconfigCount = 0;

        // For Min / max TPC repair counts
        UINT32 repairMinCount = 0;
        UINT32 repairMaxCount = 0;

        // Min number of graphics TPCs
        // (Hopper +)
        UINT32 minGraphicsTpcs = 0;

        // For HBM chips, whether half sites are allowed or not
        // (Hopper +)
        bool fullSitesOnly = false;

        // Max allowed difference in the number of enabled this unit between UGPUs
        // (Hopper +)
        UINT32 maxUnitUgpuImbalance = ~0U;
        // For Hopper+, vgpc skyline list is a sorted list of TPC counts per GPC
        vector<UINT32> vGpcSkylineList;
    };

    // Map of fuse names to fuse definitions
    typedef map<string, FuseDef> FuseDefMap;

    // Map of SKU names to SkuConfigs
    typedef map<string, SkuConfig> SkuList;

    RC SearchFuseInfo(string ChipSku,
                      string FuseName,
                      const map<string, SkuConfig> &SkuList,
                      FuseInfo* pInfo);

    bool FindFuseInfo
    (
        string fuseName,
        const SkuConfig& skuCfg,
        const FuseInfo** ppInfo
    );

    // Given a XML fuse string [from marketing section], populate pFuseInfo
    RC ParseAttributeAndValue(string StrVal, FuseUtil::FuseInfo *pFuseInfo);
    void ParseValHex(FuseUtil::FuseInfo *pFuseInfo, string FuseValInStr);
    void ParseBurnCount(FuseUtil::FuseInfo *FuseInfo, string FuseValInStr);
    void ParseRangeHex(FuseUtil::FuseInfo *pFuseInfo, string FuseValInStr);

    RC GetP4Info(const string& filename, const FuseUtil::MiscInfo **pInfo);
    RC PrintFuseSpec(const string &FileName);
    RC PrintSkuSpec(const string &FileName);
    RC GetSkuNames(const string &FileName, vector<string> *pSkuNames);
    RC GetFuseInfo(const string       &FileName,
                   const string       &ChipSku,
                   const string       &FuseName,
                   FuseUtil::FuseInfo *pInfo);
    RC GetFsInfo
    (
        const string &FileName,
        const string &ChipSku,
        map<string, DownbinConfig>* pConfigs
    );

    struct FuselessFuseInfo
    {
        UINT32 numFuseRows = 0;
        UINT32 fuselessStartRow = 0;
        UINT32 fuselessEndRow = 0;
    };

    RC GetFuseMacroInfo
    (
       const string &FileName,
       FuseUtil::FuselessFuseInfo *pFuselessFuseInfo,
       const FuseUtil::FuseDefMap **pFuseDefs,
       bool  hasFuseMacroMerged
    );

}

#endif
