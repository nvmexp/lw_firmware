/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/bitflags.h"
#include "core/include/types.h"
#include "core/include/rc.h"
#include "core/utility/fuse.h"
#include "gpu/fuse/fuseutils.h"
#include "gpu/fuse/iffrecord.h"

#include <map>
#include <vector>

enum class FuseMacroType
{
    Fuse,
    Fpf,
    Rir
};

extern map<FuseMacroType, string> fuseMacroString;

class FuseValues
{
public:
    map<string, UINT32> m_NamedValues;
    bool m_HasIff = false;
    vector<shared_ptr<IffRecord>> m_IffRecords;
    set<string> m_NeedsRir;

    bool IsEmpty() 
        { return m_NamedValues.empty() && m_IffRecords.empty(); }
    void Clear() 
        { m_NamedValues.clear(); m_IffRecords.clear(); m_NeedsRir.clear(); m_HasIff = false; }
};

struct FuseOverride
{
    // the new value the fuse should have
    UINT32 value;
    // true if this override is treated as a starting point and the SKU
    // handler is still supposed to determine a value based on the SKU
    bool useSkuDef;
};

enum class FuseFilter : UINT08
{
    NONE = 0,
    ALL_FUSES = 0,
    OPT_FUSES_ONLY = (1 << 0),
    LWSTOMER_FUSES_ONLY = (1 << 1),
    SKIP_FS_FUSES = (1 << 2)
};

BITFLAGS_ENUM(FuseFilter);

OldFuse::Tolerance FuseFilterToTolerance(FuseFilter filter);
FuseFilter ToleranceToFuseFilter(OldFuse::Tolerance tolerance);

enum WhichFuses
{
    BAD_FUSES  = (1<<0),
    GOOD_FUSES = (1<<1),
    ALL_FUSES  = (BAD_FUSES|GOOD_FUSES)
};

enum CheckPriority
{
    LEVEL_ALWAYS    = 0,
    LEVEL_LWSTOMER  = 1,
    LEVEL_ILWISIBLE = 2,
};

struct FuseVals
{
    string name;
    string expectedStr;
    UINT32 actualVal;

    FuseVals() = default;

    FuseVals(OldFuse::FuseVals vals)
        : name(vals.Name)
        , expectedStr(vals.ExpectedStr)
        , actualVal(vals.ActualVal)
    { }
};

class FuseDiff
{
public:
    vector<FuseVals> namedFuseMismatches;
    bool doesFsMismatch = false;
    bool doesIffMismatch = false;

    bool Matches()
    {
        return namedFuseMismatches.empty() && !doesFsMismatch && !doesIffMismatch;
    }
};

struct ReworkFFRecord
{
    FuseUtil::FuseLoc loc;
    UINT32 val;

    ReworkFFRecord(const FuseUtil::FuseLoc& loc, UINT32 val)
        : loc(loc)
        , val(val)
    { }
};

struct ReworkRequest
{
    // Src SKU DEVIDs
    UINT32 srcDevidA = 0;
    UINT32 srcDevidB = 0;

    // Fuse macro related info
    UINT32 fuselessRowStart = 0;
    UINT32 fuselessRowEnd   = 0;
    vector<UINT32>         staticFuseRows;
    vector<ReworkFFRecord> newFFRecords;
    vector<ReworkFFRecord> fsFFRecords;

    // FPF macro related info
    bool hasFpfMacro = false;
    vector<UINT32> fpfRows;

    // RIR info
    bool supportsRir = false;
    bool requiresRir = false;
    vector<UINT32> rirRows;
};

struct FuseSpec
{
    enum SleepState
    {
        SLEEP_AFTER_GPIO_HIGH,
        SLEEP_AFTER_FUSECTRL_WRITE,
        SLEEP_AFTER_GPIO_LOW,
        SLEEP_AFTER_FUSECTRL_READ,
        NUM_OF_SLEEPSTATES
    };

    UINT32 FuseClockPeriodNs;
    UINT32 NumKFuseWords;
    // Unions are used here because GpuFuse has PascalCase but
    // LwLinkFuse has camelCase, and this change is already too
    // big to warrant changing it in all the sub-classes now.
    union
    {
        UINT32 numOfFuseReg;
        UINT32 NumOfFuseReg;
    };
    union
    {
        UINT32 fuselessStart;   // for Fermi and up
        UINT32 FuselessStart;   // for Fermi and up
    };
    union
    {
        UINT32 fuselessEnd;     // for Fermi and up
        UINT32 FuselessEnd;     // for Fermi and up
    };
    union
    {
        UINT32 sleepTime[NUM_OF_SLEEPSTATES];
        UINT32 SleepTime[NUM_OF_SLEEPSTATES];
    };

    FuseSpec() :
        FuseClockPeriodNs(0),
        NumKFuseWords(0),
        numOfFuseReg(0),
        fuselessStart(0),
        fuselessEnd(0)
    {
        memset(SleepTime, 0, sizeof(SleepTime));
    }
};
