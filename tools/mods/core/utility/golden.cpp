/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2012, 2016-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file golden.cpp
//! \brief Implementation of class Goldelwalues.

#include "core/include/script.h"
#include "core/include/tee.h"
#include "core/include/jscript.h"
#include "core/include/golden.h"
#include "core/include/crc.h"
#include "core/include/gpu.h"
#if defined(INCLUDE_GPU)
#include "gpu/include/gpudev.h"
#endif
#include "core/include/lwrm.h"
#include "core/include/massert.h"
#include "core/include/imagefil.h"
#include "core/include/display.h"
#include "core/include/xp.h"
#include "core/include/simpleui.h"
#include "core/include/utility.h"
#include "core/include/errormap.h"
#include "core/include/version.h"
#include "lwos.h"
#include "Lwcm.h"
#include "core/include/tasker.h"
#include "core/include/platform.h"
#include "goldutil.h"
#include "core/include/extcrcdev.h"
#include <cstdio>
#include <cmath>
#include <ctype.h>
#include <cstring>

#include "lwmisc.h" // for drf stuff

//------------------------------------------------------------------------------
// Helper functions, using anonymous namespace instead of static.
//------------------------------------------------------------------------------
namespace {

    UINT32 SortAndCountUnique(UINT32 *Values, UINT32 NumElements)
    {
       MASSERT(Values);
       MASSERT(NumElements > 2);

       UINT32 UniqueCount = 1;

       // Bubble sort.  Don Knuth is rolling in his grave as I write this.
       // I intend to use only for arrays of ten elements, so it's just KISS.
       UINT32 j,k;
       for (j=0 ; j<NumElements ; j++)
       {
          for (k=j ; k<NumElements ; k++)
          {
             if (Values[j] > Values[k])
             {
                UINT32 tmp = Values[j];
                Values[j]=Values[k];
                Values[k]=tmp;
             }
          }
       }

       for (j = 1; j < NumElements; j++)
       {
          if (Values[j-1] != Values[j])
             UniqueCount++;
       }

       return UniqueCount;
    }

    UINT32 CountWrong(UINT32 *Values, UINT32 NumElements, UINT32 Expected)
    {
       UINT32 j;
       UINT32 Wrong = 0;

       for (j = 0; j < NumElements; j++)
       {
          if (Values[j] != Expected)
             Wrong++;
       }

       return Wrong;
    }
};

//------------------------------------------------------------------------------
// Lookup table for CheckVal() to allow the algorithm to be more generic.
//------------------------------------------------------------------------------
const Goldelwalues::CodeDesc Goldelwalues::s_Codex[] =
{
//   Code,      Name,      RqDisp,useRot,FmtSupportedPtr,
//      ErrHndlr, NumBins,
//      NumElems
    {CheckSums,"Checksums" ,false ,false,&Goldelwalues::NoFFmt
        ,&Goldelwalues::CksumErrHandler  ,&Goldelwalues::MultiBins
        ,&Goldelwalues::PixelElems}
    ,{Crc      ,"Crcs"     ,false ,false,&Goldelwalues::AllFmts
        ,&Goldelwalues::CrcErrHandler    ,&Goldelwalues::MultiBins
        ,&Goldelwalues::PixelElems}
    ,{DacCrc   ,"DacCrcs"  ,true  ,false,&Goldelwalues::AllFmts
        ,&Goldelwalues::DacCrcErrHandler ,&Goldelwalues::OneBin
        ,&Goldelwalues::DacCrcElems}
    ,{TmdsCrc  ,"TmdsCrcs" ,false ,false,&Goldelwalues::AllFmts
        ,&Goldelwalues::TmdsCrcErrHandler,&Goldelwalues::OneBin
        ,&Goldelwalues::TmdsCrcElems}
    ,{OldCrc   ,"OldCrcs"  ,false ,false,&Goldelwalues::AllFmts
        ,&Goldelwalues::CrcErrHandler    ,&Goldelwalues::MultiBins
        ,&Goldelwalues::PixelElems}
    ,{Extra    ,"Zpass"    ,false ,false,&Goldelwalues::AllFmts
        ,&Goldelwalues::ZpassErrHandler    ,&Goldelwalues::ExtraBins
        ,&Goldelwalues::ZpassElems}
    ,{ExtCrc   ,"ExtCrcs"  ,false ,false,&Goldelwalues::AllFmts
        ,&Goldelwalues::ExtCrcErrHandler   ,&Goldelwalues::ExtCrcBins
        ,&Goldelwalues::ExtCrcElems}
};
//------------------------------------------------------------------------------
// Helper functions to support s_Codex[] table lookups
//------------------------------------------------------------------------------
bool Goldelwalues::NoFFmt(ColorUtils::Format fmt)
{
    return !ColorUtils::IsFloat(fmt);
}

bool Goldelwalues::AllFmts(ColorUtils::Format fmt)
{
    return !ColorUtils::FormatIsOutOfRange(fmt);
}

int Goldelwalues::MultiBins(int surfIdx)
{
    return m_NumCodeBins;
}
int Goldelwalues::OneBin(int surfIdx)
{
    return 1;
}
int Goldelwalues::ExtraBins(int surfIdx)
{
    if (surfIdx == m_pSurfaces->NumSurfaces()-1)
        return static_cast<int>(m_Extra.size());
    else
        return 0;
}
int Goldelwalues::ExtCrcBins(int surfIdx)
{
    CrcGoldenInterface *pExtCrcDev = m_pSurfaces->CrcGoldenInt(surfIdx);
    if (nullptr != pExtCrcDev)
        return pExtCrcDev->NumCrcBins();
    else
        return 0;
}

// Return the number of elements for a give code & format.
int Goldelwalues::PixelElems(ColorUtils::Format fmt)
{
    return ColorUtils::PixelElements(fmt);
}
int Goldelwalues::DacCrcElems(ColorUtils::Format fmt /*not used*/)
{
    return NUM_HW_CRCS;
}
int Goldelwalues::TmdsCrcElems(ColorUtils::Format fmt/*not used*/)
{
    return NUM_TMDS_CRCS;
}
int Goldelwalues::ZpassElems(ColorUtils::Format fmt/*not used*/)
{
    return 1;
}
int Goldelwalues::ExtCrcElems(ColorUtils::Format fmt/*not used*/)
{
    return 1;
}

//------------------------------------------------------------------------------
// Error handler functions for CheckVal()
//------------------------------------------------------------------------------
bool Goldelwalues::CksumErrHandler(ErrHandlerArgs &Arg)
{
    const INT32 * pNewV = (const INT32 *)Arg.pNew;
    const INT32 * pOldV = (const INT32 *)Arg.pOld;
    const ColorUtils::Format fmt = m_pSurfaces->Format(Arg.surf);
    UINT32 diff = abs(*pNewV - *pOldV);
    const ColorUtils::Element e = ColorUtils::ElementAt(fmt, Arg.elem);
    if (static_cast<UINT32>(e) < static_cast<UINT32>(NumElem))
    {
        if (diff > m_AllowedBinDiff[e])
        {
            Arg.pMD->errBins[Arg.bin] = true;
        }
        Arg.pMD->badBins[Arg.bin] = true;
        Arg.pMD->sumDiffs[e] += diff;
    
        Printf(Tee::PriDebug,
                "Golden sum %s %d: 0x%x != measured 0x%x, (diff %d)\n",
                ColorUtils::ElementToString(e),
                Arg.bin,
                *Arg.pOld,
                *Arg.pNew,
                diff);
    
    }
    else
    {
        MASSERT(!"CksumErrHandler() e >= NumElem");
    }
    return false; // fail based on other criteria
}

bool Goldelwalues::CrcErrHandler(ErrHandlerArgs &Arg)
{
    const ColorUtils::Format fmt = m_pSurfaces->Format(Arg.surf);
    const ColorUtils::Element e = ColorUtils::ElementAt(fmt, Arg.elem);
    if (static_cast<UINT32>(e) < static_cast<UINT32>(NumElem))
    {
        Arg.pMD->badBins[Arg.bin] = true;
        Arg.pMD->errBins[Arg.bin] = true;
        if (Arg.code & Crc)
            Arg.pMD->sumDiffs[e] += 1;
    
        Printf(Tee::PriDebug, "Golden crc %s %d: 0x%x != measured 0x%x\n",
               ColorUtils::ElementToString(e),
               Arg.bin,
               *Arg.pOld,
               *Arg.pNew);
    }
    else
    {
        MASSERT(!"CksumErrHandler() e >= NumElem");
    }
    return false; // fail based on other criteria
}

bool Goldelwalues::TmdsCrcErrHandler(ErrHandlerArgs &Arg)
{
    Printf(Tee::PriDebug, "Golden TMDS crc %d: 0x%x != measured 0x%x\n",
            Arg.elem,*Arg.pOld,*Arg.pNew);
    return true; // fail unconditionally.
}

// Generic error handler, just prints fail message and returns recomendation to
// fail unconditionally.
bool Goldelwalues::GenErrHandler(ErrHandlerArgs &Arg)
{
    for (unsigned i=0; i < NUMELEMS(s_Codex); i++)
    {
        if (s_Codex[i].code == Arg.code)
        {
            Printf(Tee::PriDebug, "Golden %s failed.\n",s_Codex[i].szName);
            break;
        }
    }
    return true; // unconditional failure.
}
// Generic error handler, just updates the miscompare details, prints fail message and returns
// the recomendation to conditionally fail based on m_AllowedBadBinsOneCheck.
bool Goldelwalues::ZpassErrHandler(ErrHandlerArgs &Arg)
{
    const INT32 * pNewV = (const INT32 *)Arg.pNew;
    const INT32 * pOldV = (const INT32 *)Arg.pOld;
    Arg.pMD->extraDiff[Arg.bin].diff = abs(*pNewV - *pOldV);
    Arg.pMD->extraDiff[Arg.bin].expected = *pOldV;
    Arg.pMD->extraDiff[Arg.bin].actual = *pNewV;

    for (unsigned i=0; i < NUMELEMS(s_Codex); i++)
    {
        if (s_Codex[i].code == Arg.code)
        {
            Printf(Tee::PriDebug, "Golden %s failed.\n",s_Codex[i].szName);
            break;
        }
    }
    return false; // conditional failure.
}

// Generic error handler, just prints fail message and returns recomendation to
// fail unconditionally.
bool Goldelwalues::ExtCrcErrHandler(ErrHandlerArgs &Arg)
{
    CrcGoldenInterface *pExtCrcDev = m_pSurfaces->CrcGoldenInt(Arg.surf);
    if (nullptr != pExtCrcDev)
    {
        pExtCrcDev->ReportCrcMiscompare(Arg.bin, *Arg.pOld, *Arg.pNew);
    }
    else
    {
        Printf(Tee::PriDebug,
               "Golden External Device CRC %d: 0x%x != measured 0x%x\n",
                Arg.bin,*Arg.pOld,*Arg.pNew);
    }
    return true; // unconditional failure.
}

bool Goldelwalues::DacCrcErrHandler(ErrHandlerArgs &Arg)
{
    // Compare DAC CRC's.
    // Elements here are not surface elements, but display-hw CRCs.
    RC rc;
    const UINT32 retry = 10;
    const int    hwCrcElements = NUM_HW_CRCS;
    const char * hwCrcNames[hwCrcElements] = { "primary", "NA", "compositor" };
    UINT32 retryCrcs[hwCrcElements][retry];
    Display *pDisplay = GetGpuDevice()->GetDisplay();

    // Reset the pointers because we may not have miscompared on the 1st element.
    Arg.pOld -= Arg.elem;
    Arg.pNew -= Arg.elem;
    for (int i = 0; i<hwCrcElements; i++)
    {
        if (Arg.pOld[i] != Arg.pNew[i])
        {
            Printf(Tee::PriDebug, "Golden DAC %s cr: 0x%x != measured 0x%x\n",
                    hwCrcNames[i],Arg.pOld[i],Arg.pNew[i]);
        }
    }

    for (UINT32 r = 0; r<retry ; r++)
    {
        rc = pDisplay->GetCrcValues(
                m_pSurfaces->Display(Arg.surf),
                retryCrcs[0]+r,
                retryCrcs[1]+r,
                retryCrcs[2]+r);

        if (OK != rc)
            break;
    }

    if (OK == rc)
    {
        for (int i = 0; i < hwCrcElements; i++)
        {
            const UINT32 numWrong =CountWrong(retryCrcs[i], retry, Arg.pOld[i]);
            if (numWrong)
            {
                Printf(Tee::PriNormal,
                    "%s has %2d/%2d error(s) and %2d unique value(s)\n",
                    hwCrcNames[i],
                    numWrong,
                    retry,
                    SortAndCountUnique(retryCrcs[i], retry));
            }
        }
    }
    return true; // unconditional fail
}

//------------------------------------------------------------------------------
// Helper class MiscompareDetails.
//------------------------------------------------------------------------------
Goldelwalues::MiscompareDetails::MiscompareDetails (int numBins/*=0*/,int numExtra /*=0*/)
:   badBins(numBins, false)
   ,errBins(numBins, false)
   ,extraDiff(numExtra)
   ,badTpcMask(0)
{
    for (int i = 0; i < NumElem; i++)
        sumDiffs[i] = 0;
}

void Goldelwalues::MiscompareDetails::Accumulate (const MiscompareDetails & that)
{
    const size_t numBins = that.badBins.size();
    if (badBins.size() != numBins)
    {
        badBins.resize(numBins, false);
        errBins.resize(numBins, false);
    }

    for (size_t i = 0; i < numBins; i++)
    {
        badBins[i] = badBins[i] || that.badBins[i];
        errBins[i] = errBins[i] || that.errBins[i];
    }
    for (int i = 0; i < NumElem; i++)
        sumDiffs[i] += that.sumDiffs[i];

    MASSERT(extraDiff.size() == that.extraDiff.size());
    for (UINT32 i = 0; i < extraDiff.size(); i++)
    {
        // keep the 1st miscompare details
        if (extraDiff[i].diff == 0)
        {
            extraDiff[i] = that.extraDiff[i];
        }
    }
    badTpcMask |= that.badTpcMask;
}

int Goldelwalues::MiscompareDetails::numBad () const
{
    int num = 0;
    for (vector<bool>::const_iterator i = badBins.begin();
        i != badBins.end(); ++i)
    {
        if (*i)
            num++;
    }
    return num;
}
int Goldelwalues::MiscompareDetails::numErr () const
{
    int num = 0;
    for (vector<bool>::const_iterator i = errBins.begin();
        i != errBins.end(); ++i)
    {
        if (*i)
            num++;
    }
    return num;
}

//------------------------------------------------------------------------------
// Iterate through the ExtraDiff bins and count the number of non-zero entries.
int Goldelwalues::MiscompareDetails::numExtraDiff () const
{
    int num = 0;
    for (vector<ExtraValues>::const_iterator i = extraDiff.begin();
        i != extraDiff.end(); ++i)
    {
        if (i->diff)
            num++;
    }
    return num;
}

//------------------------------------------------------------------------------
// JS Class, and the one object.
//------------------------------------------------------------------------------
JS_CLASS(Golden);

static SObject Golden_Object
(
    "Golden",
    GoldenClass,
    0,
    0,
    "Default controls for golden CRC checks (may be overridden per-test)."
);

//------------------------------------------------------------------------------
// Properties (for setting the shared default values)
//------------------------------------------------------------------------------
static SProperty Golden_Name
(
    Golden_Object,
    "Name",
    0,
    "Std",
    0,
    0,
    0,
    "Name used for Values hash keys."
);
static SProperty Golden_Action
(
    Golden_Object,
    "Action",
    0,
    Goldelwalues::Check,
    0,
    0,
    0,
    "Action to take at each loop: Check, Store, or Skip."
);
static SProperty Golden_SkipCount
(
    Golden_Object,
    "SkipCount",
    0,
    100,
    0,
    0,
    0,
    "Loops to skip between checks: if 0, act every loop."
);
static SProperty Golden_Codes
(
    Golden_Object,
    "Codes",
    0,
    Goldelwalues::Crc,
    0,
    0,
    0,
    "Mask of: Golden.Crc, .DacCrc, .TmdsCrc, .TvCrc, .CheckSums, .OldCrc and/or .Extra"
);
static SProperty Golden_ForceErrorMask
(
    Golden_Object,
    "ForceErrorMask",
    0,
    0,
    0,
    0,
    0,
    "XOR'd with first 4 bytes of color and Z buffers to force a miscompare."
);
static SProperty Golden_ForceErrorLoop
(
    Golden_Object,
    "ForceErrorLoop",
    0,
    0,
    0,
    0,
    0,
    "If ForceErrorMask is !0, apply on this loop."
);

static SProperty Golden_BufferFetchHint
(
    Golden_Object,
    "BufferFetchHint",
    0,
    Goldelwalues::opCpuDma,
    0,
    0,
    0,
    "Fetch surface using CPU/CPU DMA access."
);

static SProperty Golden_CalcAlgoHint
(
    Golden_Object,
    "CalcAlgoHint",
    0,
    Goldelwalues::CpuCalcAlgorithm,
    0,
    0,
    0,
    "Callwlate Goldens at CPU/GPU/Mimic GPU behavior on CPU."
);

static SProperty Golden_DumpTga
(
    Golden_Object,
    "DumpTga",
    0,
    Goldelwalues::Never,
    0,
    0,
    0,
    "Mask of when to write .TGA image file: Never, OnError, OnBadPixel, OnSkip, OnStore, OnCheck, or Always."
);
static SProperty Golden_DumpPng
(
    Golden_Object,
    "DumpPng",
    0,
    Goldelwalues::Never,
    0,
    0,
    0,
    "Mask of when to write .PNG image file: Never, OnError, OnBadPixel, OnSkip, OnStore, OnCheck, or Always."
);
static SProperty Golden_Interact
(
    Golden_Object,
    "Interact",
    0,
    Goldelwalues::Never,
    0,
    0,
    0,
    "Mask of when to go interactive during a test: Never, OnError, OnBadPixel, OnSkip, OnStore, OnCheck, or Always."
);
static SProperty Golden_CrcFileName
(
    Golden_Object,
    "CrcFileName",
    0,
    "",
    0,
    0,
    0,
    "Name of text .crc file from \"old\" (non-MODS) diags."
);
static SProperty Golden_TgaName
(
    Golden_Object,
    "TgaName",
    0,
    "",
    0,
    0,
    0,
    "Name to use for dumped .TGA files (if \"\", will auto-generate)."
);
static SProperty Golden_Print
(
    Golden_Object,
    "Print",
    0,
    Goldelwalues::Never,
    0,
    0,
    0,
    "Mask of when to print verbose test status: Never, OnError, OnBadPixel, OnSkip, OnStore, OnCheck, or Always."
);
static SProperty Golden_NumCodeBins
(
    Golden_Object,
    "NumCodeBins",
    0,
    1,
    0,
    0,
    0,
    "Number of pixel-interleaved codes to do."
);
static SProperty Golden_TestMode
(
    Golden_Object,
    "TestMode",
    0,
    Goldelwalues::Oqa,
    0,
    0,
    0,
    "Test mode: one of Golden.Oqa, or Golden.Mfg."
);

static SProperty Golden_TgaAlphaToRgb
(
    Golden_Object,
    "TgaAlphaToRgb",
    0,
    false,
    0,
    0,
    0,
    "When dumping .TGA files, copy alpha to r,g,b (and stencil to depth)."
);
static SProperty Golden_StopOnError
(
    Golden_Object,
    "StopOnError",
    0,
    true,
    0,
    0,
    0,
    "Stop the test on pixel error failure (else continue to end of test before returning error)."
);
static SProperty Golden_PrintCsv
(
    Golden_Object,
    "PrintCsv",
    0,
    false,
    0,
    0,
    0,
    "Print .csv format data on each check to import into spreadsheets."
);

static SProperty Golden_CheckDmaOnFail
(
    Golden_Object,
    "CheckDmaOnFail",
    0,
    false,
    0,
    0,
    0,
    "On golden miscompare, check that the DMA'd copy of the FB matches the FB."
);

static SProperty Golden_RetryDmaOnFail
(
    Golden_Object,
    "RetryDmaOnFail",
    0,
    false,
    0,
    0,
    0,
    "On golden miscompare, retry without dma copy, to hide DMA errors."
);

static SProperty Golden_SendTrigger
(
    Golden_Object,
    "SendTrigger",
    0,
    false,
    0,
    0,
    0,
    "Send a trigger on given loop or error so HW engineers can debug problems using a logic analyzer."
);

static SProperty Golden_TriggerLoop
(
    Golden_Object,
    "TriggerLoop",
    0,
    0,
    0,
    0,
    0,
    "Send a trigger on the specified loop."
);

static SProperty Golden_TriggerSubdevice
(
    Golden_Object,
    "TriggerSubdevice",
    0,
    0,
    0,
    0,
    0,
    "Send a trigger on the specified subdevice instance."
);

static SProperty Golden_CrcDelay
(
    Golden_Object,
    "CrcDelay",
    0,
    0,
    0,
    0,
    0,
    "" // Arbitrary delay in callwlating CRC (no description on purpose)
);

static SProperty Golden_SurfChkOnGpuBinSize
(
    Golden_Object,
    "SurfChkOnGpuBinSize",
    0,
    31,
    0,
    0,
    0,
    "Golden bin size used for surface check."
);
PROP_CONST_DESC(Golden, Crc,        Goldelwalues::Crc,
        "CONSTANT: do Cyclic Redundancy Check");
PROP_CONST_DESC(Golden, DacCrc,     Goldelwalues::DacCrc,
        "CONSTANT: do DAC CRC");
PROP_CONST_DESC(Golden, TmdsCrc,    Goldelwalues::TmdsCrc,
        "CONSTANT: do TMDS CRC");
PROP_CONST_DESC(Golden, CheckSums,  Goldelwalues::CheckSums,
        "CONSTANT: do a set of component-wise (A,R,G,B,S,Z) Check Sums");
PROP_CONST_DESC(Golden, OldCrc,     Goldelwalues::OldCrc,
        "CONSTANT: do old-style CRC (to match emulation golden values)");
PROP_CONST_DESC(Golden, Extra,      Goldelwalues::Extra,
        "CONSTANT: do extra test-specific check (ie. zpass pixel count for GLRandom)");
PROP_CONST_DESC(Golden, ExtCrc,     Goldelwalues::ExtCrc,
        "CONSTANT: use external device CRC");

PROP_CONST_DESC(Golden, Skip,       Goldelwalues::Skip,
        "CONSTANT: don't check or generate values");
PROP_CONST_DESC(Golden, Check,      Goldelwalues::Check,
        "CONSTANT: do check screen against golden values");
PROP_CONST_DESC(Golden, Store,       Goldelwalues::Store,
        "CONSTANT: do store screen values as new golden values");

PROP_CONST_DESC(Golden, Never,      Goldelwalues::Never,
        "CONSTANT: when to do something?  Never.");
PROP_CONST_DESC(Golden, Always,     Goldelwalues::Always,
        "CONSTANT: when to do something?  Every loop.");
PROP_CONST_DESC(Golden, OnError,    Goldelwalues::OnError,
        "CONSTANT: when to do something?  On Checks that fail and abort the test.");
PROP_CONST_DESC(Golden, OnBadPixel, Goldelwalues::OnBadPixel,
        "CONSTANT: when to do something?  On Checks that find a bad pixel.");
PROP_CONST_DESC(Golden, OnCheck,    Goldelwalues::OnCheck,
        "CONSTANT: when to do something?  On loops when we check CRC/CheckSum.");
PROP_CONST_DESC(Golden, OnStore,    Goldelwalues::OnStore,
        "CONSTANT: when to do something?  On loops when we store new CRC/CheckSum.");
PROP_CONST_DESC(Golden, OnSkip,     Goldelwalues::OnSkip,
        "CONSTANT: when to do something?  On loops we would have checked, but action is Skip.");

PROP_CONST_DESC(Golden, Mfg,        Goldelwalues::Mfg,
        "CONSTANT: TestMode: manufacturing.");
PROP_CONST_DESC(Golden, Oqa,        Goldelwalues::Oqa,
        "CONSTANT: TestMode: OEM/Outgoing Quality Assurance.");

PROP_CONST_DESC(Golden, opCpu,      Goldelwalues::opCpu,
        "CONSTANT: TestMode: Optimize at lowest level for FB readback");
PROP_CONST_DESC(Golden, opCpuDma,   Goldelwalues::opCpuDma,
        "CONSTANT: TestMode: Optimize high speed DMA for FB readback");

PROP_CONST_DESC(Golden, CpuCalcAlgorithm,           Goldelwalues::CpuCalcAlgorithm,
        "CONSTANT: TestMode: Compute CRC of entire buffer as a whole on CPU");
PROP_CONST_DESC(Golden, FragmentedGpuCalcAlgorithm, Goldelwalues::FragmentedGpuCalcAlgorithm,
        "CONSTANT: TestMode: Fragment the surface and compute CRC of fragments on GPU threads");
PROP_CONST_DESC(Golden, FragmentedCpuCalcAlgorithm, Goldelwalues::FragmentedCpuCalcAlgorithm,
        "CONSTANT: TestMode: Fragment the surface and compute CRC of fragments on CPU threads");

//------------------------------------------------------------------------------
// Methods and Tests
//------------------------------------------------------------------------------

C_(Golden_PrintValues);
static STest Golden_PrintValues
(
    Golden_Object,
    "PrintValues",
    C_Golden_PrintValues,
    2,
    "Print the golden values to screen and to log file."
);

C_(Golden_ReadFile);
static STest Golden_ReadFile
(
    Golden_Object,
    "ReadFile",
    C_Golden_ReadFile,
    1,
    "Read golden values from a binary file."
);

C_(Golden_UnmarkAllRecs);
static STest Golden_UnmarkAllRecs
(
    Golden_Object,
    "UnmarkAllRecs",
    C_Golden_UnmarkAllRecs,
    0,
    "Clear the mark on all in-memory golden records.",
    MODS_INTERNAL_ONLY
);

C_(Golden_MarkRecs);
static STest Golden_MarkRecs
(
    Golden_Object,
    "MarkRecs",
    C_Golden_MarkRecs,
    2,
    "Mark records for a given test and dbIndex for writing later.",
    MODS_INTERNAL_ONLY
);

C_(Golden_MarkRecsWithName);
static STest Golden_MarkRecsWithName
(
    Golden_Object,
    "MarkRecsWithName",
    C_Golden_MarkRecsWithName,
    2,
    "Marks all records that contain the given name.",
    MODS_INTERNAL_ONLY
);

C_(Golden_WriteFile);
static STest Golden_WriteFile
(
    Golden_Object,
    "WriteFile",
    C_Golden_WriteFile,
    2,
    "Write the current golden values to binary file.",
    MODS_INTERNAL_ONLY
);

//------------------------------------------------------------------------------
// Goldelwalues
//------------------------------------------------------------------------------

Goldelwalues::Goldelwalues()
{
    Reset();
}

//------------------------------------------------------------------------------
Goldelwalues::~Goldelwalues()
{
    if (IsErrorRateTestCallMissing())
    {
        Printf(Tee::PriError,
            "   !!! Possible software error: DeferredRc = %d not read by test \"%s\" !!!\n",
            m_DeferredRc.Get(), m_TstName.c_str());
    }
    ClearSurfaces();
}

//------------------------------------------------------------------------------
void Goldelwalues::Reset()
{
    m_TstName             = "";
    m_PlatformName        = "";
    m_NameSuffix          = "";
    m_Action              = Check;
    m_SkipCount           = 100;
    m_Codes               = Crc;
    m_ForceErrorMask      = 0;
    m_ForceErrorLoop      = 0;
    m_BufferFetchHint     = opCpuDma;   // highest common level of optimization
    m_CalcAlgoHint        = CpuCalcAlgorithm;
    m_DumpTga             = Never;
    m_DumpPng             = Never;
    m_Print               = Never;
    m_Interact            = Never;
    m_UseCrcFile          = false;
    m_CrcFileName         = "";
    m_TgaName             = "";
    m_NumCodeBins         = 1;
    m_StopOnError         = true;
    m_TgaAlphaToRgb       = false;
    m_PrintCsv            = false;
    m_CheckDmaOnFail      = false;
    m_RetryDmaOnFail      = false;
    m_SendTrigger         = false;
    m_TriggerLoop         = 0;
    m_TriggerSubdevice    = 0;
    m_CrcDelay            = 0;
    m_CheckCRCsUnique     = false;
    m_SurfChkOnGpuBinSize = 31;

    m_YIlwerted  = false;

    m_SubdevMask     = 0xffffffff;
    m_Loop           = 0;
    m_DbIndex        = 0;
    m_DbHandle       = NULL;
    m_CodeBuf        = NULL;

    m_pPrintFunc     = 0;
    m_PrintFuncData  = 0;
    m_NumChecks      = 0;
    m_DeferredRc.Clear();
    m_DeferredRcWasRead = true;
    m_pIdleFunc      = NULL;
    m_pIdleCallbackData0 = NULL;
    m_pIdleCallbackData1 = NULL;
    m_Extra.clear();
    m_pSurfaces      = NULL;

    m_AclwmErrsBySurf.clear();
    m_NumSurfaceChecks = 0;

    // The "error tolerance" feature of glrandom is no longer used.
    // We retain the code in case we need it again someday.
    m_AllowedErrorBinsOneCheck = 0;
    m_AllowedBadBinsOneCheck   = 0;
    m_AllowedExtraDiffOneCheck = 0;
    m_AllowedBinDiff[ColorUtils::elRed]     = 0;
    m_AllowedBinDiff[ColorUtils::elGreen]   = 0;
    m_AllowedBinDiff[ColorUtils::elBlue]    = 0;
    m_AllowedBinDiff[ColorUtils::elAlpha]   = 0;
    m_AllowedBinDiff[ColorUtils::elDepth]   = 0;
    m_AllowedBinDiff[ColorUtils::elStencil] = 0;
    m_AllowedBinDiff[ColorUtils::elOther]   = 0;
}

//------------------------------------------------------------------------------
RC Goldelwalues::InitFromJs(SObject & tstSObj)
{
    JSObject *  testObj = tstSObj.GetJSObject();

    return InitFromJs (testObj);
}

RC Goldelwalues::InitFromJs(JSObject * testObj)
{
    JavaScriptPtr pJs;
    RC          rc = OK;
    JSObject *  localGoldenObj = 0;
    UINT32      tmpI;
    string      tmpS;

    //
    // Put all member data to defaults.
    //
    Reset();

    if (OK != pJs->GetProperty(testObj, "name", &tmpS))
        tmpS = "";
    m_TstName = tmpS;

    //
    // Get the global Golden control properties.
    //
    CHECK_RC(pJs->GetProperty(Golden_Object, Golden_Name,               &m_PlatformName));
    CHECK_RC(pJs->GetProperty(Golden_Object, Golden_Action,             &tmpI));
    m_Action = (Action)tmpI;
    CHECK_RC(pJs->GetProperty(Golden_Object, Golden_SkipCount,          &m_SkipCount));
    CHECK_RC(pJs->GetProperty(Golden_Object, Golden_Codes,              &m_Codes));
    CHECK_RC(pJs->GetProperty(Golden_Object, Golden_ForceErrorMask,     &m_ForceErrorMask));
    CHECK_RC(pJs->GetProperty(Golden_Object, Golden_ForceErrorLoop,     &m_ForceErrorLoop));
    CHECK_RC(pJs->GetProperty(Golden_Object, Golden_BufferFetchHint,   &tmpI));
    m_BufferFetchHint = (Goldelwalues::BufferFetchHint)tmpI;
    CHECK_RC(pJs->GetProperty(Golden_Object, Golden_CalcAlgoHint, &tmpI));
    m_CalcAlgoHint = (Goldelwalues::CalcAlgorithm)tmpI;
    CHECK_RC(pJs->GetProperty(Golden_Object, Golden_DumpTga,            &m_DumpTga));
    CHECK_RC(pJs->GetProperty(Golden_Object, Golden_DumpPng,            &m_DumpPng));
    CHECK_RC(pJs->GetProperty(Golden_Object, Golden_CrcFileName,        &m_CrcFileName));
    CHECK_RC(pJs->GetProperty(Golden_Object, Golden_TgaName,            &m_TgaName));
    CHECK_RC(pJs->GetProperty(Golden_Object, Golden_Print,              &m_Print));
    CHECK_RC(pJs->GetProperty(Golden_Object, Golden_Interact,           &m_Interact));
    CHECK_RC(pJs->GetProperty(Golden_Object, Golden_NumCodeBins,        &m_NumCodeBins));
    CHECK_RC(pJs->GetProperty(Golden_Object, Golden_TgaAlphaToRgb,      &tmpI));
    m_TgaAlphaToRgb = (tmpI != 0);
    CHECK_RC(pJs->GetProperty(Golden_Object, Golden_StopOnError,        &tmpI));
    m_StopOnError = (tmpI != 0);
    CHECK_RC(pJs->GetProperty(Golden_Object, Golden_PrintCsv,           &tmpI));
    m_PrintCsv = (tmpI != 0);
    CHECK_RC(pJs->GetProperty(Golden_Object, Golden_CheckDmaOnFail,     &tmpI));
    m_CheckDmaOnFail = (tmpI != 0);
    CHECK_RC(pJs->GetProperty(Golden_Object, Golden_RetryDmaOnFail,     &tmpI));
    m_RetryDmaOnFail = (tmpI != 0);
    CHECK_RC(pJs->GetProperty(Golden_Object, Golden_SendTrigger,        &tmpI));
    m_SendTrigger = (tmpI != 0);
    CHECK_RC(pJs->GetProperty(Golden_Object, Golden_TriggerLoop,        &m_TriggerLoop));
    CHECK_RC(pJs->GetProperty(Golden_Object, Golden_TriggerSubdevice,   &m_TriggerSubdevice));
    CHECK_RC(pJs->GetProperty(Golden_Object, Golden_CrcDelay,           &m_CrcDelay));
    CHECK_RC(pJs->GetProperty(Golden_Object, Golden_SurfChkOnGpuBinSize,&m_SurfChkOnGpuBinSize));

    //
    // Get the per-test Golden property, if present.
    //
    if (OK != pJs->GetProperty(testObj, "Golden", &localGoldenObj))
        localGoldenObj = 0;

    if (localGoldenObj)
    {
        //
        // Get per-test control overrides, if any.
        //
        if (OK == pJs->GetProperty(localGoldenObj, "Name", &tmpS))
            m_PlatformName = tmpS;
        if (OK == pJs->GetProperty(localGoldenObj, "Action", &tmpI))
            m_Action = (Action)tmpI;
        if (OK == pJs->GetProperty(localGoldenObj, "SkipCount", &tmpI))
            m_SkipCount = tmpI;
        if (OK == pJs->GetProperty(localGoldenObj, "Codes", &tmpI))
            m_Codes = tmpI;
        if (OK == pJs->GetProperty(localGoldenObj, "ForceErrorMask", &tmpI))
            m_ForceErrorMask = tmpI;
        if (OK == pJs->GetProperty(localGoldenObj, "ForceErrorLoop", &tmpI))
            m_ForceErrorLoop = tmpI;
        if (OK == pJs->GetProperty(localGoldenObj, "BufferFetchHint", &tmpI))
            m_BufferFetchHint = (Goldelwalues::BufferFetchHint)tmpI;
        if (OK == pJs->GetProperty(localGoldenObj, "CalcAlgoHint", &tmpI))
            m_CalcAlgoHint = (Goldelwalues::CalcAlgorithm)tmpI;
        if (OK == pJs->GetProperty(localGoldenObj, "DumpTga", &tmpI))
            m_DumpTga = tmpI;
        if (OK == pJs->GetProperty(localGoldenObj, "DumpPng", &tmpI))
            m_DumpPng = tmpI;
        if (OK == pJs->GetProperty(localGoldenObj, "CrcFileName", &tmpS))
            m_CrcFileName = tmpS;
        if (OK == pJs->GetProperty(localGoldenObj, "TgaName", &tmpS))
            m_TgaName = tmpS;
        if (OK == pJs->GetProperty(localGoldenObj, "Print", &tmpI))
            m_Print = tmpI;
        if (OK == pJs->GetProperty(localGoldenObj, "Interact", &tmpI))
            m_Interact = tmpI;
        if (OK == pJs->GetProperty(localGoldenObj, "NumCodeBins", &tmpI))
            m_NumCodeBins = tmpI;
        if (OK == pJs->GetProperty(localGoldenObj, "TgaAlphaToRgb", &tmpI))
            m_TgaAlphaToRgb = (tmpI != 0);
        if (OK == pJs->GetProperty(localGoldenObj, "StopOnError", &tmpI))
            m_StopOnError = (tmpI != 0);
        if (OK == pJs->GetProperty(localGoldenObj, "PrintCsv", &tmpI))
            m_PrintCsv = (tmpI != 0);
        if (OK == pJs->GetProperty(localGoldenObj, "CheckDmaOnFail", &tmpI))
            m_CheckDmaOnFail = (tmpI != 0);
        if (OK == pJs->GetProperty(localGoldenObj, "RetryDmaOnFail", &tmpI))
            m_RetryDmaOnFail = (tmpI != 0);
        if (OK == pJs->GetProperty(localGoldenObj, "SendTrigger", &tmpI))
            m_SendTrigger = (tmpI != 0);
        if (OK == pJs->GetProperty(localGoldenObj, "TriggerLoop", &tmpI))
            m_TriggerLoop = tmpI;
        if (OK == pJs->GetProperty(localGoldenObj, "TriggerSubdevice", &tmpI))
            m_TriggerSubdevice = tmpI;
        if (OK == pJs->GetProperty(localGoldenObj, "CrcDelay", &tmpI))
            m_CrcDelay = tmpI;
        if (OK == pJs->GetProperty(localGoldenObj, "SurfChkOnGpuBinSize", &tmpI))
            m_SurfChkOnGpuBinSize = tmpI;
    }

    if (m_BufferFetchHint > opCpuDma)
    {
        Printf(Tee::PriHigh,
               "Golden.BufferFetchHint should be Golden.opCpu or .opCpuDma.\n");
        return RC::BAD_PARAMETER;
    }
    if (m_CalcAlgoHint > FragmentedGpuCalcAlgorithm)
    {
        Printf(Tee::PriHigh,
               "Golden.CalcAlgoHint should be Golden.CpuCalcAlgorithm, "
               ".FragmentedCpuCalcAlgorithm, or "
               ".FragmentedGpuCalcAlgorithm.\n");
        return RC::BAD_PARAMETER;
    }

    if (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
    {
        jsval       tmpJ;

        JSObject * globalGoldenObj = Golden_Object.GetJSObject();

        if (OK == pJs->GetProperty(globalGoldenObj,
                                   "AllowedBadBinsOneCheck", &tmpI))
        {
            m_AllowedBadBinsOneCheck = tmpI;
        }
        if (OK == pJs->GetProperty(globalGoldenObj,
                                   "AllowedErrorBinsOneCheck", &tmpI))
        {
            m_AllowedErrorBinsOneCheck = tmpI;
        }
        if (OK == pJs->GetPropertyJsval(globalGoldenObj,
                                        "AllowedBinDiff", &tmpJ))
        {
            JsArray jsa;
            rc = pJs->FromJsval(tmpJ, &jsa);
            if (OK == rc)
            {
                if (jsa.size() != NumElem)
                {
                    Printf(Tee::PriHigh,
                           "Golden.AllowedBinDiff should be an array of %d integers\n",
                           NumElem);
                    return RC::BAD_PARAMETER;
                }
                for (int i = 0; i < NumElem; i++)
                    CHECK_RC(pJs->FromJsval(jsa[i], &m_AllowedBinDiff[i]));
            }
        }
        if (OK == pJs->GetProperty(globalGoldenObj,
                                   "AllowedExtraDiffOneCheck", &tmpI))
        {
            m_AllowedExtraDiffOneCheck = tmpI;
        }

        if (localGoldenObj)
        {
            if (OK == pJs->GetProperty(localGoldenObj,
                                       "AllowedBadBinsOneCheck", &tmpI))
            {
                m_AllowedBadBinsOneCheck = tmpI;
            }
            if (OK == pJs->GetProperty(localGoldenObj,
                                       "AllowedErrorBinsOneCheck", &tmpI))
            {
                m_AllowedErrorBinsOneCheck = tmpI;
            }
            if (OK == pJs->GetPropertyJsval(localGoldenObj,
                                            "AllowedBinDiff", &tmpJ))
            {
                JsArray jsa;
                rc = pJs->FromJsval(tmpJ, &jsa);
                if (OK == rc)
                {
                    if (jsa.size() != NumElem)
                    {
                        Printf(Tee::PriHigh,
                               "Golden.AllowedBinDiff should be an array of %d integers\n",
                               NumElem);
                        return RC::BAD_PARAMETER;
                    }
                    for (int i = 0; i < NumElem; i++)
                        CHECK_RC(pJs->FromJsval(jsa[i], &m_AllowedBinDiff[i]));
                }
            }
            if (OK == pJs->GetProperty(localGoldenObj,
                                       "AllowedExtraDiffOneCheck", &tmpI))
            {
                m_AllowedExtraDiffOneCheck = tmpI;
            }
        }
    }

    //
    // Check that our control arguments make sense.
    //
    switch (m_Action)
    {
        case Check:
        case Store:
        case Skip:
            break;

        default:
            Printf(Tee::PriNormal,
                   "Action must be Golden.Check, .Store, or .Skip\n");
            return RC::BAD_PARAMETER;
    }
    if (m_Codes & ~(END_CODE-1))
    {
        Printf(Tee::PriNormal,
               "Code must be a mask of one or more of: Golden.Crc .DacCrc"
               " .TmdsCrc .TvCrc .CheckSums .OldCrc, or .Extra\n");
        return RC::BAD_PARAMETER;
    }
    if (m_CrcFileName.size())
    {
        m_UseCrcFile = true;

        if (m_Codes != OldCrc)
        {
            Printf(Tee::PriNormal,
                   "NOTE: forcing Code to OldCrc for use with .crc file.\n");
            m_Codes = OldCrc;
        }
        if (m_Action == Store)
        {
            Printf(Tee::PriNormal, "NOTE: Cannot store to old .crc files.");
        }
    }
    for (const auto &ii: initializer_list<pair<UINT32, const char*>>{
                                                    {m_DumpTga,  "DumpTga" },
                                                    {m_DumpPng,  "DumpPng" },
                                                    {m_Print,    "Print"   },
                                                    {m_Interact, "Interact"}})
    {
        const UINT32 val  = ii.first;
        const char * name = ii.second;

        if (val & ~(OnCheck|OnStore|OnSkip|Always|OnError|OnBadPixel))
        {
            Printf(Tee::PriHigh,
                   "Value of %s is invalid (%x)\n"
                   "  It must be a mask of any of: Golden.Never, .OnCheck,"
                   " .OnStore, .OnSkip, .Always, OnBadPixel, .OnError\n",
                   name, val);
            return RC::BAD_PARAMETER;
        }
    }

    if ((m_NumCodeBins == 0) || (m_NumCodeBins > m_MaxCodeBins))
    {
        Printf(Tee::PriHigh,
               "NumCodeBins must be 1 to %d,\n"
               " should be at least 5x AllowedBadBinsOneCheck,\n"
               " and should not be a near-factor of screen width\n",
               m_MaxCodeBins);
        return RC::BAD_PARAMETER;
    }

    return OK;
}

//------------------------------------------------------------------------------
void Goldelwalues::PrintJsProperties(Tee::Priority pri)
{
    // Report the controls we arrived at.
    const char * ActionStrings[] =
      {
          "Golden.Check"
         ,"Golden.Store"
         ,"Golden.Skip"
      };
    const char * FTstr[] =
      {
         "false",
         "true"
      };

    const char * BufferFetchHintStr[] =
    {
        "opCpu",
        "opCpuDma"
    };

    const char * CalcAlgorithmStr[] =
    {
        "CpuCalcAlgorithm",
        "FragmentedCpuCalcAlgorithm",
        "FragmentedGpuCalcAlgorithm"
    };

    Printf(pri, "Goldelwalues Js Properties:\n");
    Printf(pri, "\tPlatformName:\t\t\t%s\n",  m_PlatformName.c_str());
    Printf(pri, "\tNameSuffix:\t\t\t%s\n",    m_NameSuffix.c_str());
    Printf(pri, "\tAction:\t\t\t\t%s\n",      ActionStrings[ m_Action ]);
    Printf(pri, "\tSkipCount:\t\t\t%d\n",     m_SkipCount);
    Printf(pri, "\tCodes:\t\t\t\t");
    PrintCodesValue(m_Codes, pri);
    Printf(pri, "\n");

    Printf(pri, "\tNumCodeBins:\t\t\t%d\n",   m_NumCodeBins);
    if (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
    {
        Printf(pri, "\tAllowedBadBinsOneCheck:\t%d\n",
               m_AllowedBadBinsOneCheck);
        Printf(pri, "\tAllowedErrorBinsOneCheck:\t%d\n",
               m_AllowedErrorBinsOneCheck);
        if (m_Codes & CheckSums)
        {
            Printf(pri, "\tAllowedBinDiff:\t\t");
            for (int i = 0; i < NumElem; i++)
                Printf(pri, "%d,", m_AllowedBinDiff[i]);
            Printf(pri, " (R,G,B,A,D,S,O)\n");
        }
        Printf(pri, "\tAllowedExtraDiffOneCheck:\t%d\n",
               m_AllowedExtraDiffOneCheck);
    }

    Printf(pri, "\tStopOnError:\t\t\t%s\n",          FTstr[m_StopOnError]);
    Printf(pri, "\tBufferFetchHint:\t\t%s\n",        BufferFetchHintStr[m_BufferFetchHint]);
    Printf(pri, "\tCallwlationAlgorithm:\t\t%s\n",   CalcAlgorithmStr[m_CalcAlgoHint]);
    Printf(pri, "\tCheckDmaOnFail:\t\t\t%s\n",       FTstr[m_CheckDmaOnFail]);
    Printf(pri, "\tRetryDmaOnFail:\t\t\t%s\n",       FTstr[m_RetryDmaOnFail]);
    Printf(pri, "\tSendTrigger:\t\t\t%s\n",          FTstr[m_SendTrigger]);
    Printf(pri, "\tTriggerLoop:\t\t\t%d\n",          m_TriggerLoop);
    Printf(pri, "\tTriggerSubdevice:\t\t%d\n",       m_TriggerSubdevice);
    Printf(pri, "\tPrintCsv:\t\t\t%s\n",             FTstr[m_PrintCsv]);
    Printf(pri, "\tPrint:\t\t\t\t");                 PrintWhelwalue(m_Print, pri);
    Printf(pri, "\tInteract:\t\t\t");                PrintWhelwalue(m_Interact, pri);
    Printf(pri, "\tDumpTga:\t\t\t");                 PrintWhelwalue(m_DumpTga, pri);
    Printf(pri, "\tDumpPng:\t\t\t");                 PrintWhelwalue(m_DumpPng, pri);
    if (m_DumpTga || m_DumpPng)
    {
        Printf(pri, "\tTgaAlphaToRgb:\t\t\t%s\n",    FTstr[m_TgaAlphaToRgb]);
        Printf(pri, "\tTgaName:\t\t\t\"%s\"\n",      m_TgaName.c_str());
    }
    if (m_ForceErrorMask)
    {
        Printf(pri, "\tForceErrorMask:\t\t%x\n",      m_ForceErrorMask);
        Printf(pri, "\tForceErrorLoop:\t\t%d\n",      m_ForceErrorLoop);
    }
    if (m_UseCrcFile)
    {
        Printf(pri, "\tCrcFileName:\t\t\"%s\"\n",     m_CrcFileName.c_str());
    }
    if (m_CalcAlgoHint == FragmentedGpuCalcAlgorithm)
    {
        Printf(pri, "\tSurfChkOnGpuBinSize:\t\t%d\n",m_SurfChkOnGpuBinSize);
    }

    return;
}

//------------------------------------------------------------------------------
void Goldelwalues::PrintWhelwalue(UINT32 wval, Tee::Priority pri)
{
    if (wval == Never)
    {
        Printf(pri, "%x (Never)\n", wval);
    }
    else if (wval & Always)
    {
        Printf(pri, "%x (Always)\n", wval);
    }
    else
    {
        Printf(pri, "%x (",wval);
        if (wval & OnStore)
        {
            wval &= ~OnStore;
            Printf(pri, "OnStore%s", wval ? "|" : "");
        }
        if (wval & OnCheck)
        {
            wval &= ~OnCheck;
            Printf(pri, "OnCheck%s", wval ? "|" : "");
        }
        if (wval & OnSkip)
        {
            wval &= ~OnSkip;
            Printf(pri, "OnSkip%s", wval ? "|" : "");
        }
        if (wval & OnError)
        {
            wval &= ~OnError;
            Printf(pri, "OnError%s", wval ? "|" : "");
        }
        if (wval & OnBadPixel)
        {
            wval &= ~OnBadPixel;
            Printf(pri, "OnBadPixel%s", wval ? "|" : "");
        }
        if (wval)
        {
            Printf(pri, "UNKNOWN_FLAG");
        }
        Printf(pri, ")\n");
    }
}

//------------------------------------------------------------------------------
void Goldelwalues::PrintCodesValue(UINT32 codes, Tee::Priority pri)
{
    Printf(pri, "%d", codes);
    if (codes)
    {
        Printf(pri, "(");

        if (codes & Crc)
        {
            codes &= ~Crc;
            Printf(pri, "Crc%s", codes ? "|" : "");
        }
        if (codes & DacCrc)
        {
            codes &= ~DacCrc;
            Printf(pri, "DacCrc%s", codes ? "|" : "");
        }
        if (codes & TmdsCrc)
        {
            codes &= ~TmdsCrc;
            Printf(pri, "TmdsCrc%s", codes ? "|" : "");
        }
        if (codes & CheckSums)
        {
            codes &= ~CheckSums;
            Printf(pri, "CheckSums%s", codes ? "|" : "");
        }
        if (codes & OldCrc)
        {
            codes &= ~OldCrc;
            Printf(pri, "OldCrc%s", codes ? "|" : "");
        }
        if (codes & Extra)
        {
            codes &= ~Extra;
            Printf(pri, "Extra%s", codes ? "|" : "");
        }
        if (codes & ExtCrc)
        {
            codes &= ~ExtCrc;
            Printf(pri, "ExtCrc%s", codes ? "|" : "");
        }

        if (codes)
        {
            Printf(pri, "UNKNOWN_FLAG");
        }
        Printf(pri, ")");
    }
}

//------------------------------------------------------------------------------
// parse .crc file, store CRCs into database.
//
RC Goldelwalues::GetCrcsFromFile(const vector<string> & GoldenNames)
{
    RC     rc = OK;
    char   lineBuf[256];
    char   keyBuf[256];
    UINT32 val;
    UINT32 crcs[2] = { 0, 0 };
    FILE  *fp = 0;

    // Open the file.
    CHECK_RC(Utility::OpenFile(m_CrcFileName, &fp, "r"));

    for (vector<string>::const_iterator it = GoldenNames.begin();
         it != GoldenNames.end(); ++it)
    {
        // Reset the file pointer to the beginning of the file.
        fseek(fp, 0, SEEK_SET);

        const char * nameStr = it->c_str();
        size_t       nameLen = it->size();

        // Read each line.
        while (fgets(lineBuf, sizeof(lineBuf)-1, fp))
        {
            // Find matching lines.
            if (strncmp(lineBuf, nameStr, nameLen) == 0)
            {
                // Get exact key and value.
                if (2 != sscanf(lineBuf, "%s = 0x%x", keyBuf, &val))
                {
                    rc = RC::CANNOT_PARSE_FILE;
                    break;
                }

                if ('z' == tolower(keyBuf[ strlen(keyBuf)-1 ]))
                {
                    MASSERT(0 == crcs[0]);
                    crcs[0] = val;
                }
                else
                {
                    MASSERT(0 == crcs[1]);
                    crcs[1] = val;
                }

                // Store database record when we have seen both C and Z.
                if (crcs[0] && crcs[1])
                {
                    if (m_DbHandle != NULL)
                    {
                        rc = GoldenDb::PutRec(m_DbHandle, 0, crcs, false);
                    }
                    m_PlatformName = *it;
                    goto done;
                }
            }
        }
    }
done:

    // Close the file.
    fclose(fp);

    return rc;
}

static void SetSurfacesFailure()
{
    MASSERT(!"Golden::SetSurfaces failed");
}

//------------------------------------------------------------------------------
//! \brief Set up this Goldelwalues object with runtime surfaces info.
//!
//! GoldenSurfaces describes the array of surfaces to be checked, and hides
//! some platform-specific details of readback.
//!
//! Also during SetSurfaces, we use the surface data to callwlate a unique
//! database key and open the GoldenDb database.
//
RC Goldelwalues::SetSurfaces
(
  GoldenSurfaces * pgs
)
{
    RC rc;

    MASSERT(pgs);
    m_pSurfaces = pgs;
    m_SurfaceDataToDump.resize(m_pSurfaces->NumSurfaces());

    Utility::CleanupOnReturn<Goldelwalues> CleanupSurface(this, &Goldelwalues::ClearSurfaces);
    Utility::CleanupOnReturn<void> FailureMassert(&SetSurfacesFailure);

    m_Loop  = 0;
    m_DbIndex = 0;
    m_NameSuffix = "";

    // Per surface errors become invalid if a test resets its surfaces
    m_AclwmErrsBySurf.clear();

    // Include a description of the displays we will use with display CRCs
    // in the GoldenDB database key.  Since we prefer to test panels in
    // gpu-scaling mode, the CRCs will differ with different panel native modes.
    //
    // Since we share one GoldenDB record for all surfaces, we will loop over
    // all surfaces and accumulate suffixes.  The common case is just one surf.
    Display *pDisplay = GetGpuDevice()->GetDisplay();
    int surfIdx;
    for (surfIdx = 0; surfIdx < m_pSurfaces->NumSurfaces(); surfIdx++)
    {
        if (m_Codes & ExtCrc)
        {
            CrcGoldenInterface *pExtCrcDev = m_pSurfaces->CrcGoldenInt(surfIdx);
            if (nullptr != pExtCrcDev)
            {
                string CrcSignature;
                CHECK_RC(pExtCrcDev->GetCrcSignature(&CrcSignature));
                m_NameSuffix += CrcSignature;
            }
        }
        UINT32 disp = m_pSurfaces->Display(surfIdx);
        if (0 == disp)
            continue;

        // Crude WAR for what is arguably a gputest.js bug:
        //   disallow display-specific CRC types that aren't supported on all
        //   the tested displays.

        Display::DisplayType dispType = Display::NONE;
        UINT32 Displays = disp;
        while (Displays)
        {
            UINT32 SingleDisplay = Displays & ~(Displays-1);
            Displays ^= SingleDisplay;
            if (dispType == Display::NONE)
            {
                dispType = pDisplay->GetDisplayType(SingleDisplay);
            }
            else if (pDisplay->GetDisplayType(SingleDisplay) != dispType)
            {
                Printf(Tee::PriNormal,
                    "Error: Goldens for non uniform multiple displays are not supported.\n");
                CHECK_RC(RC::SOFTWARE_ERROR);
            }
        }
        if ((m_Codes & TmdsCrc) && (dispType != Display::DFP))
        {
            Printf(Tee::PriLow,
            "TMDS CRCs were selected, but display %06x does not support them.\n"
            "Turning on Dac CRCs instead\n", disp);
            m_Codes &= ~TmdsCrc;
            m_Codes |= DacCrc;
        }
        if (m_Codes & DacCrc)
        {
            string CrcSignature;
            UINT32 FirstDisplay = disp & ~(disp-1);
            CHECK_RC(pDisplay->GetCrcSignature(FirstDisplay,
                                               Display::DAC_CRC,
                                               &CrcSignature));
            UINT32 Displays  = disp ^ FirstDisplay;
            while (Displays)
            {
                UINT32 SecondaryDisplay = Displays & ~(Displays-1);
                Displays ^= SecondaryDisplay;
                string SecondaryCrcSignature;
                CHECK_RC(pDisplay->GetCrcSignature(SecondaryDisplay,
                                                   Display::DAC_CRC,
                                                   &SecondaryCrcSignature));
                if (CrcSignature != SecondaryCrcSignature)
                {
                    Printf(Tee::PriNormal,
                        "Error: Goldens for non uniform multiple displays are not supported.\n");
                    Printf(Tee::PriNormal,
                        "Display 0x%08x = %s.\n", FirstDisplay, CrcSignature.c_str());
                    Printf(Tee::PriNormal,
                        "Display 0x%08x = %s.\n", SecondaryDisplay, SecondaryCrcSignature.c_str());
                    CHECK_RC(RC::SOFTWARE_ERROR);
                }
            }
            m_NameSuffix += CrcSignature;
        }
        if (m_Codes & TmdsCrc)
        {
            string CrcSignature;
            CHECK_RC(pDisplay->GetCrcSignature(disp,
                                               Display::TMDS_CRC,
                                               &CrcSignature));
            m_NameSuffix += CrcSignature;
        }
    }

    CHECK_RC(OpenDb());

    CleanupSurface.Cancel();
    FailureMassert.Cancel();
    return rc;
}

//------------------------------------------------------------------------------
RC Goldelwalues::OpenDb()
{
    RC rc;
    string Name = m_PlatformName + m_NameSuffix;
    Printf(Tee::PriLow, "Golden Values name is %s\n", Name.c_str());

    // We now know everything we need to in order to callwlate our GoldenDB
    // key value.  Open up the database.

    UINT32 ValsPerRecord = CalcRecordOffsetsAndSize();
    if (m_CodeBuf)
        delete [] m_CodeBuf;

    m_CodeBuf = new UINT32[ ValsPerRecord ];

    CHECK_RC(GoldenDb::GetHandle(
            m_TstName.c_str(),
            Name.c_str(),
            m_Codes,
            ValsPerRecord,
            &m_DbHandle));

    if (m_UseCrcFile)
    {
        vector<string> GoldenNames;
        GoldenNames.push_back(Name);
        CHECK_RC(GetCrcsFromFile(GoldenNames));
    }
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Callwlate an offset within a GoldenDb data record.
//!
//! For each tested loop of a test, we have a GoldenDb record full of codes.
//! This is an array of CRCs, checksums, DAC crcs, etc. in a layout
//! that depends on runtime javascript configuration.
//!
//! This function isolates the callwlation of this data layout for
//! a given code and surface.
//!
//! Also, if called with (bufIndex == max) and (code == END_CODE), it instead
//! callwlates the size of the entire GoldenDb record.
//
UINT32 Goldelwalues::GetRecordOffset
(
    Code code,
    int bufIndex
)
{
    MASSERT(m_pSurfaces);
    MASSERT(m_pSurfaces->NumSurfaces() > bufIndex);

    UINT32 recOff = 0;
    int buf;
    for (buf = 0; buf <= bufIndex; buf++)
    {
        UINT32 formatElements = ColorUtils::PixelElements(m_pSurfaces->Format(buf));

        UINT32 c;
        for (c = CheckSums; c < END_CODE; c <<= 1)
        {
            if ((c == UINT32(code)) && (buf == bufIndex))
                return recOff;

            if (! (m_Codes & c))
                continue;

            switch (c)
            {
                case CheckSums:
                case Crc:
                    recOff += formatElements * m_NumCodeBins;
                    break;

                case DacCrc:
                    if (0 != m_pSurfaces->Display(buf))
                        recOff += 3;
                    break;

                case TmdsCrc:
                    if (0 != m_pSurfaces->Display(buf))
                        recOff += NUM_TMDS_CRCS;
                    break;

                case OldCrc:
                    recOff += 1;
                    break;

                case Extra:
                    if (buf == m_pSurfaces->NumSurfaces() - 1)
                        recOff += (UINT32)m_Extra.size();
                    break;

                case ExtCrc:
                    {
                        CrcGoldenInterface *pExtCrcDev =
                            m_pSurfaces->CrcGoldenInt(buf);
                        if (nullptr != pExtCrcDev)
                            recOff += pExtCrcDev->NumCrcBins();
                    }
                    break;

                default:
                    MASSERT(!"Missing Code enum in switch");
            }
        }
    }

    // If we got here, we should be doing the "callwlate full record size" thing.
    MASSERT(bufIndex == m_pSurfaces->NumSurfaces() - 1);
    MASSERT(code == END_CODE);
    return recOff;
}

//------------------------------------------------------------------------------
UINT32 Goldelwalues::CalcRecordOffsetsAndSize()
{
    return GetRecordOffset(END_CODE, m_pSurfaces->NumSurfaces() - 1);
}

//------------------------------------------------------------------------------
void Goldelwalues::ClearSurfaces()
{
    m_pSurfaces        = NULL;
    m_SurfaceDataToDump.clear();
    delete [] m_CodeBuf;
    m_CodeBuf          = NULL;
    m_DbHandle         = NULL;
}

//------------------------------------------------------------------------------
void Goldelwalues::SetLoop(UINT32 Loop)
{
    m_Loop = Loop;
    m_DbIndex = Loop;
}

//------------------------------------------------------------------------------
void Goldelwalues::SetLoopAndDbIndex
(
    UINT32 Loop,
    UINT32 DbIndex
)
{
    m_Loop = Loop;
    m_DbIndex = DbIndex;
}

//------------------------------------------------------------------------------
void Goldelwalues::SetPrintCallback(void (*pFunc)(void *), void * pData)
{
    m_pPrintFunc = pFunc;
    m_PrintFuncData = pData;
}

//------------------------------------------------------------------------------
void Goldelwalues::SetYIlwerted (bool Y0IsBottom)
{
    m_YIlwerted = Y0IsBottom;
}

//------------------------------------------------------------------------------
void Goldelwalues::SetAction(Action action)
{
    m_Action = action;
}

//------------------------------------------------------------------------------
void Goldelwalues::SetSkipCount(UINT32 skipCount)
{
    m_SkipCount = skipCount;
}

//------------------------------------------------------------------------------
bool Goldelwalues::GetRetryWithReducedOptimization(const RC &rc) const
{
    bool ret = false;
    if ((OK != rc) && (m_Codes & SurfaceCodes))
    {
        switch (m_CalcAlgoHint)
        {
            case CpuCalcAlgorithm:
                if (m_BufferFetchHint == opCpuDma && m_RetryDmaOnFail)
                {
                    ret = true;
                }
                break;

            case FragmentedGpuCalcAlgorithm:
                ret = true;
                break;

            default:
                break;
        }
    }
    return ret;
}

//------------------------------------------------------------------------------
RC Goldelwalues::Run()
{
    RC rc;
    if (NULL == m_pSurfaces)
    {
        MASSERT(!"SetSurfaces wasn't run before Goldelwalues::Run");
        return RC::SOFTWARE_ERROR;
    }

    rc = InnerRun();

    if (GetRetryWithReducedOptimization(rc))
    {
        // This error *might* have been due to faulty DMA readback or GPU SM.
        // Reduce the optimization level and retry
        if (m_pSurfaces->ReduceOptimization(true))
        {
            rc.Clear();
            rc = InnerRun();

            if (OK == rc)
            {
                Printf(Tee::PriHigh,
                    "Ignoring GPU/DMA readback error at loop %d.\n", m_Loop - 1);

                // To completely "undo" the earlier dma-induced/gpu induced error,
                // we should also now delete any dumped .png or .tga files, and
                // rewind aclwmulated pixel errors, etc.
                // Too much trouble for this nasty hack...
            }
            m_pSurfaces->ReduceOptimization(false);
        }
    }
    ++m_Loop;
    ++m_DbIndex;

    // GOLDEN_BAD_PIXEL is only for communication among Goldelwalues functions.
    // It indicates a "tolerated" error that we intend to ignore.
    // It should never be returned to outside the class.
    MASSERT(rc != RC::GOLDEN_BAD_PIXEL);

    return rc;
}

//------------------------------------------------------------------------------
RC Goldelwalues::InnerRun()
{
    RC rc;
    RC finalRc;

    const UINT32 * DbCodeBuf = 0;         // old CRC values from database

    if (m_SendTrigger && (m_TriggerLoop == m_Loop))
    {
        // Send a trigger for debugging on a logic analyzer.
        GoldenUtils::SendTrigger(m_TriggerSubdevice);
    }

    // Putting a yield here in this very commonly called location
    // helps keep other threads from starving.
    Tasker::Yield();

    // Figure out what we will be doing.

    UINT32 whenMask = Always;

    if ((m_SkipCount == 0) ||
         (m_Loop % m_SkipCount == m_SkipCount-1))
    {
        // This is an "action" loop.
        switch (m_Action)
        {
            case Skip:  whenMask |= OnSkip;  break;
            case Store: whenMask |= OnStore; break;
            case Check: whenMask |= OnCheck; break;

            default: MASSERT(!"Unrecognized Goldelwalues::Action enum."); break;
        }
    }

    if (! (whenMask & (OnCheck | OnStore | m_DumpTga | m_DumpPng)))
    {
        // Not checking, storing, or dumping images.  Nothing to do!
        return OK;
    }

    Utility::CleanupOnReturnArg<Goldelwalues, void, UINT32*>
        Failure(this, &Goldelwalues::InnerRunFailure, &whenMask);

    MiscompareDetails mdThisRun(m_NumCodeBins,(int)m_Extra.size());

    // Wait for the HW to be done rendering (test-specific callback).
    if (m_pIdleFunc)
    {
        CHECK_RC(m_pIdleFunc(m_pIdleCallbackData0, m_pIdleCallbackData1));
    }

    if (whenMask & OnCheck)
    {
        CHECK_RC(GoldenDb::GetRec(m_DbHandle, m_DbIndex, &DbCodeBuf));
        m_NumChecks++;
    }

    vector< vector<UINT08>* > pSurfaceDataToDump(m_SurfaceDataToDump.size());
    for (int surfIdx = 0; surfIdx < m_pSurfaces->NumSurfaces(); surfIdx++)
    {
        if ((m_DumpPng | m_DumpTga) != Never)
        {
            pSurfaceDataToDump[surfIdx] = &m_SurfaceDataToDump[surfIdx];
        }
        else
        {
            pSurfaceDataToDump[surfIdx] = NULL;
        }
    }

    int surfIdx;
    UINT32 subdevIdx;
    for (surfIdx = 0; surfIdx < m_pSurfaces->NumSurfaces(); surfIdx++)
    {
        for (subdevIdx = 0;
             subdevIdx < GetGpuDevice()->GetNumSubdevices();
             subdevIdx++)
        {
            if (! (m_SubdevMask & (1<<subdevIdx)))
                continue;

            if (whenMask & (OnCheck | OnStore))
            {
                // Read the surface, get the CRCs, checksums, etc.
                CHECK_RC(CheckSurface(surfIdx, subdevIdx, pSurfaceDataToDump[surfIdx]));
            }
            if (whenMask & OnCheck)
            {

                // Compare new CRC/other code values with golden
                // values now, rather than waiting for all surfaces to
                // be checked.  We want to catch the miscompare now,
                // while this surface is still cached, so we can
                // efficiently dump images or test for readback dma
                // errors.
                MiscompareDetails mdThisSurf(m_NumCodeBins,(int)m_Extra.size());

                RC checkRc = CheckVal(surfIdx, m_CodeBuf, DbCodeBuf, &mdThisSurf);

                if (checkRc && m_CheckDmaOnFail && (m_BufferFetchHint == opCpuDma))
                {
                    // Report readback (as opposed to rendering)
                    // errors, if any.
                    m_pSurfaces->CheckAndReportDmaErrors(subdevIdx);
                }

                if (checkRc == RC::GOLDEN_BAD_PIXEL)
                {
                    // Don't set finalRc -- Ignore this "tolerated" miscompare.
                    whenMask |= OnBadPixel;
                }
                else if (checkRc != OK)
                {
                    // Final RC is crc error from last surface checked.
                    finalRc = checkRc;
                    whenMask |= (OnError|OnBadPixel);
                }

                if (whenMask & OnBadPixel)
                {
                    if (0 == m_AclwmErrsBySurf.count(surfIdx))
                        m_AclwmErrsBySurf[surfIdx] = MiscompareDetails(m_NumCodeBins,(int)m_Extra.size());

                    m_AclwmErrsBySurf[surfIdx].Accumulate (mdThisSurf);
                    mdThisRun.Accumulate (mdThisSurf);
                }
                if (checkRc != OK)
                {
                    for (size_t i = 0; i < m_ErrCallbackFuncs.size(); i++)
                    {
                        m_ErrCallbackFuncs[i](
                                m_ErrCallbackData[i],
                                m_Loop,
                                checkRc,
                                surfIdx);
                    }
                }
            }

            // Shall we dump an image file for this surface?
            if (m_DumpTga & whenMask)
            {
                DumpImage(surfIdx, whenMask, false, subdevIdx);
            }
            if (m_DumpPng & whenMask)
            {
                DumpImage(surfIdx, whenMask, true, subdevIdx);
            }
        } // for each subdevice index
    } // for each surface

    if (whenMask & OnCheck)
    {
        if (m_PrintCsv)
        {
            // Print a comma-separated-variable report to the logfile for
            // import into a spreadsheet.  Useful for analysing error tolerances.
            PrintCsv (mdThisRun);
        }
    } // Action == Check

    if (whenMask & OnStore)
    {
        if (m_CheckCRCsUnique)
        {
            bool Equal = true;
            if (GoldenDb::CompareToLoop(m_DbHandle, m_DbIndex - 1, m_CodeBuf, &Equal)
                == OK)
            {
                if (Equal)
                    CHECK_RC(RC::CRC_VALUES_NOT_UNIQUE);
            }
        }
        // Store a new record in the golden database.
        CHECK_RC(GoldenDb::PutRec(m_DbHandle, m_DbIndex, m_CodeBuf, false));
    }

    Failure.Cancel();
    InnerRunCleanup(&whenMask);
    return finalRc;
}

//------------------------------------------------------------------------------
void Goldelwalues::InnerRunFailure(UINT32 *whenMask)
{
    *whenMask |= OnError;

    InnerRunCleanup(whenMask);
}

//------------------------------------------------------------------------------
void Goldelwalues::InnerRunCleanup(const UINT32 *whenMask)
{

    if ((m_Print & *whenMask) && m_pPrintFunc)
    {
        // Print callback.
        (*m_pPrintFunc)(m_PrintFuncData);
    }

    if (m_Interact & *whenMask)
    {
        // Spawn a temporary interactive prompt.
        Printf(Tee::ScreenOnly,
                "Golden.Interact: type Exit() to continue with test.\n");
        SimpleUserInterface * pInterface = new SimpleUserInterface(true);
        pInterface->Run();
        delete pInterface;
    }

    m_pSurfaces->Ilwalidate();
}

//------------------------------------------------------------------------------
RC Goldelwalues::CheckSurface
(
    int surfIdx,
    UINT32 subdevIdx,
    vector<UINT08> *surfDumpBuffer
)
{
    MASSERT(m_pSurfaces);
    RC rc = OK;

    // Arbitrary delay
    if (m_CrcDelay > 0)
    {
        Tasker::Sleep(m_CrcDelay);
    }

    // Debugging feature, optionally corrupt the surface to force errors.
    if (m_ForceErrorMask &&
       (m_ForceErrorLoop == m_Loop) &&
       (m_Action == Check))
    {
        UINT32 * p = (UINT32 *)m_pSurfaces->GetCachedAddress(surfIdx, m_BufferFetchHint, subdevIdx, surfDumpBuffer);
        UINT32 i;
        for (i = 0; i < m_NumCodeBins; i++)
        {
            UINT32 x = MEM_RD32(p + i);
            MEM_WR32(p + i, x^m_ForceErrorMask);
        }
    }

    for (UINT32 cd = CheckSums; cd < END_CODE; cd <<= 1)
    {
        if (m_Codes & cd)
        {
            UINT32 * cdbuf = m_CodeBuf + GetRecordOffset((Code)cd, surfIdx);

            if (cd == Extra)
            {
                if (surfIdx == m_pSurfaces->NumSurfaces()-1)
                {
                    for (UINT32 i=0; i < m_Extra.size(); i++)
                    {
                        *cdbuf = (*m_Extra[i].pFunc)(m_Extra[i].data);
                        cdbuf++;
                    }
                }
            }
            else
            {
                if ((cd & DisplayCodes) && (0 == m_pSurfaces->Display(surfIdx)))
                    continue;

                if ((cd == ExtCrc) && (nullptr == m_pSurfaces->CrcGoldenInt(surfIdx)))
                    continue;

                CHECK_RC(m_pSurfaces->FetchAndCallwlate(
                            surfIdx,
                            subdevIdx,
                            m_BufferFetchHint,
                            m_NumCodeBins,
                            (Code)cd,
                            cdbuf,
                            surfDumpBuffer));

                m_NumSurfaceChecks++;
            }
        }
    }

    return rc;
}

int Goldelwalues::GetNumSurfaceChecks() const
{
    return m_NumSurfaceChecks;
}

//------------------------------------------------------------------------------
void Goldelwalues::PrintCsv
(
    const MiscompareDetails & md
)
{
    if (m_NumChecks == 1)
    {
        // Print column headers:
        Printf(Tee::PriHigh,
               "\ntest,name,subdev,checks,dbindex,"
               "rc,err bins,bad bins,bad TPCs,");

        for (int i = 0; i < NumElem; i++)
        {
            Printf(Tee::PriHigh, "%s,", ColorUtils::ElementToString(i));
        }
        if (m_Codes & Extra)
        {
            for (UINT32 i = 0; i < m_Extra.size(); i++)
            {
                Printf(Tee::PriHigh,
                    "%s,%s_diff,", m_Extra[i].name.c_str(), m_Extra[i].name.c_str());
            }
        }
        Printf(Tee::PriHigh, "bin-number(s) with miscompare\n");
    }

    string Name = m_PlatformName + m_NameSuffix;
    const int badBinsThisCheck = md.numBad();

    Printf(Tee::PriHigh,
            "%s,%s,%d,%d,%d,%d,%d,%d,0x%x,",
            m_TstName.c_str(),
            Name.c_str(),
            0, // subdevice
            m_NumChecks,
            m_DbIndex,
            (INT32) m_DeferredRc,
            md.numErr(),
            badBinsThisCheck,
            md.badTpcMask);

    for (int i = 0; i < NumElem; i++)
        Printf(Tee::PriHigh, "%d,", md.sumDiffs[i]);

    if (m_Codes & Extra)
    {
        int offset = GetRecordOffset(Extra, 0);
        for (UINT32 i=0; i < m_Extra.size(); i++)
        {
            Printf(Tee::PriHigh, "%d,%d,",
                    m_CodeBuf[offset+i], md.extraDiff[i].diff);
        }
    }
    if (badBinsThisCheck)
    {
        UINT32 bin;
        for (bin = 0; bin < m_NumCodeBins; bin++)
        {
            if (md.badBins[bin])
                Printf(Tee::PriHigh, "%d,", bin);
        }
    }
    Printf(Tee::PriHigh, "\n");
}

//------------------------------------------------------------------------------
RC Goldelwalues::CheckVal
(
    int            bufIndex,   // color or Z
    const UINT32 * newVals,    // array of new CRC & checksums
    const UINT32 * oldVals,    // array of expected CRC & checksums
    MiscompareDetails * pmd
)
{
    bool    fail    = false;
    const ColorUtils::Format fmt = m_pSurfaces->Format(bufIndex);

    m_DeferredRcWasRead = false;

    ErrHandlerArgs arg = {
        pmd,
        (UINT32)bufIndex,
        (Goldelwalues::Code)0,
        newVals,
        oldVals,
        0, // bin
        0, // elem
    };

    // Run through the data check table and execute any algorithms that are
    // specified via m_Codes.
    for (unsigned ci = 0; ci < NUMELEMS(s_Codex); ci++)
    {
        arg.code = s_Codex[ci].code;
        const UINT32 elems = (this->*s_Codex[ci].pNumElems)(fmt);
        const UINT32 off  = GetRecordOffset (arg.code, bufIndex);

        if (m_Codes & s_Codex[ci].code)
        {
            // Some codes require a valid display object for this surface.
            if (s_Codex[ci].rqDisp && !m_pSurfaces->Display(bufIndex))
                continue;

            // External CRCs require an external CRC device.
            if ((s_Codex[ci].code == ExtCrc) &&
                (nullptr == m_pSurfaces->CrcGoldenInt(bufIndex)))
            {
                continue;
            }
        }

        // Apply rotation miscompare algorithm.
        // Compare internal TV encoder CRC's.
        // The CRCs are not synced to any event and can come in at any order,
        // so we need to check all combinations.
        // Imagine a matrix that is new[NUM_TV_CRCS] x old[NUM_TV_CRCS]. If we
        // compare the new from the old, then we need to find a single diagonal
        // line (including wrapping) that matches.
        // -   -       old[0] old[1] old[2] old[3] old[4] old[5] old[6] old[7]
        // -   -       18     45     37     467    26      2       88      76
        // new[0]  88  -      -      -      -      -       -       *       -
        // new[1]  76  -      -      -      -      -       -       -       *
        // new[2]  18  *      -      -      -      -       -       -       -
        // new[3]  45  -      *      -      -      -       -       -       -
        // new[4]  37  -      -      *      -      -       -       -       -
        // new[5]  467 -      -      -      *      -       -       -       -
        // new[6]  26  -      -      -      -      *       -       -       -
        // new[7]  2   -      -      -      -      -       *       -       -
        if ((m_Codes & s_Codex[ci].code) && s_Codex[ci].useRot)
        {
            UINT32 comp = 0;
            UINT32 rot = 0;
            bool crcsMatch = false;
            for (rot = 0; rot < elems; rot++)
            {
                for (comp = 0; comp < elems; comp++)
                {
                    if (newVals[off + ((comp + rot) % elems)] !=
                        oldVals[off + comp])
                    {
                        break;
                    }
                }

                // Did all components match?
                if (comp == elems)
                {
                    crcsMatch = true;
                    break;
                }
            }

            if (!crcsMatch)
            {
                arg.pNew = &newVals[off];
                arg.pOld = &oldVals[off];
                fail = (this->*s_Codex[ci].pErrHndlr)(arg);
            }
        }
        // Apply standard miscompare check algorithm.
        else if (m_Codes & s_Codex[ci].code)
        {
            MASSERT((this->*s_Codex[ci].pFormatSupported)(fmt));
            // Compare the data
            const UINT32 bins = (this->*s_Codex[ci].pNumBins)(bufIndex);

            arg.pOld = oldVals + off;
            arg.pNew = newVals + off;

            for (arg.bin = 0; arg.bin < bins && !fail; arg.bin++)
            {
                for (arg.elem = 0; arg.elem < elems && !fail; arg.elem++)
                {
                    if (*arg.pNew != *arg.pOld)
                    {
                        // call the error handler for this bin/value & to print
                        // any debug messages.
                        if ((this->*s_Codex[ci].pErrHndlr)(arg))
                            fail = true;
                    }
                    arg.pNew++;
                    arg.pOld++;
                }
            }
        }
    }

    // Count up the miscomparing and error bins:

    const UINT32 numBinsMismatch = pmd->numBad();
    const UINT32 numBinsError    = pmd->numErr();
    const UINT32 numExtraDiff    = pmd->numExtraDiff();
    // Now, determine our pass/fail status.

    if (numBinsMismatch || fail || numExtraDiff)
    {
        // Send a trigger for debugging on a logic analyzer.
        if (m_SendTrigger)
        {
            GoldenUtils::SendTrigger(m_TriggerSubdevice);
        }

        // Look for too many bad bins (i.e. too many incorrect pixels)

        if (numBinsMismatch > m_AllowedBadBinsOneCheck)
            fail = true;

        if (numBinsError > m_AllowedErrorBinsOneCheck)
            fail = true;

        if (m_NumCodeBins > 1)
        {
            Printf(Tee::PriDebug,
                    "Bins err,diff,total: %d,%d,%d  allowed err,diff: %d,%d  "
                    "(%d%% prob. multiple bad pixels in a bin).\n",
                    numBinsError,
                    numBinsMismatch,
                    m_NumCodeBins,
                    m_AllowedErrorBinsOneCheck,
                    m_AllowedBadBinsOneCheck,
                    (int)(100.0*numBinsMismatch/(double)m_NumCodeBins));
        }

        RC rc = OK;

        if (fail)
        {
            if (bufIndex == 0 && (m_pSurfaces->NumSurfaces() > 1))
            {
                rc = RC::GOLDEN_VALUE_MISCOMPARE_Z;
            }
            else
            {
                rc = RC::GOLDEN_VALUE_MISCOMPARE;
            }
        }
        else if (numExtraDiff > m_AllowedExtraDiffOneCheck)
        {
                rc = RC::GOLDEN_EXTRA_MISCOMPARE;
        }
        if (OK == m_DeferredRc)
            m_DeferredRc = rc;      // save first error

        if ((OK != rc) && m_StopOnError)
        {
            // FAIL, test will stop
            return rc;
        }

        // Error is within allowed limits, or StopOnError is not set:
        // report a pixel error to Run, which will do any necessary
        // dumping/printing/pausing and then return OK to the test.

        return RC::GOLDEN_BAD_PIXEL;

    } // end if (numBinsMismatch || fail || *extraDiff)

    return OK;
}

//------------------------------------------------------------------------------
// Call this at the end of your test, to check for overall error-rate
// failures (if not using multiple code bins, always passes).
// Also, sets the .JS properties that reveal error-rate details.
//
RC Goldelwalues::ErrorRateTest
(
    SObject & tstSObj
)
{
    // test SObject to JSObject
    JSObject * testObj = tstSObj.GetJSObject();
    return ErrorRateTest(testObj);
}

RC Goldelwalues::ErrorRateTest
(
    JSObject * testObj
)
{
    if (!m_CodeBuf)
        return RC::GOLDEN_VALUE_NOT_FOUND;     // SetBuffer() hasn't run!

    StickyRC firstRc;
    JavaScriptPtr pJs;

    // Get test.Golden, or create it if necessary.
    JSObject * goldenObj;
    if ((OK != pJs->GetProperty(testObj, "Golden", &goldenObj)) ||
        (!goldenObj))
    {
       firstRc = pJs->DefineProperty(testObj, "Golden", &goldenObj);
    }

    //
    // Set some readable properties to the test.Golden object, for JS to read.
    //
    if (goldenObj)
    {
        MiscompareDetails mdAllSurfs(m_NumCodeBins,(int)m_Extra.size());
        for (map<UINT32, MiscompareDetails>::const_iterator i =
                m_AclwmErrsBySurf.begin(); i != m_AclwmErrsBySurf.end(); ++i)
        {
            mdAllSurfs.Accumulate(i->second);
        }

        firstRc = pJs->SetProperty(goldenObj, "TotalBinMiscompares", mdAllSurfs.numBad());
        firstRc = pJs->SetProperty(goldenObj, "TotalBinErrors",      mdAllSurfs.numErr());
        firstRc = pJs->SetProperty(goldenObj, "ChecksCompleted",     m_NumChecks);
        firstRc = pJs->SetProperty(goldenObj, "BadTpcMask",          mdAllSurfs.badTpcMask);

        JsArray jsa;
        for (int i = 0; i < NumElem; i++)
        {
            jsval jsv;
            firstRc = pJs->ToJsval(mdAllSurfs.sumDiffs[i], &jsv);
            jsa.push_back(jsv);
        }
        jsval jsv;
        firstRc = pJs->ToJsval(&jsa, &jsv);
        firstRc = pJs->SetPropertyJsval(goldenObj, "TotalCheckSumDiffs", jsv);

        firstRc = pJs->SetProperty(goldenObj, "TotalExtraMiscompares",      mdAllSurfs.numExtraDiff());
        JsArray outerArray;
        for (UINT32 i = 0; i < mdAllSurfs.extraDiff.size(); i++)
        {
            JsArray innerArray;
            firstRc = pJs->ToJsval(m_Extra[i].name, &jsv);
            innerArray.push_back(jsv);

            firstRc = pJs->ToJsval(mdAllSurfs.extraDiff[i].diff, &jsv);
            innerArray.push_back(jsv);

            firstRc = pJs->ToJsval(mdAllSurfs.extraDiff[i].expected, &jsv);
            innerArray.push_back(jsv);

            firstRc = pJs->ToJsval(mdAllSurfs.extraDiff[i].actual, &jsv);
            innerArray.push_back(jsv);

            firstRc = pJs->ToJsval(&innerArray, &jsv);
            outerArray.push_back(jsv);
        }
        firstRc = pJs->ToJsval(&outerArray, &jsv);
        firstRc = pJs->SetPropertyJsval(goldenObj, "ExtraErrs", jsv);

        outerArray.clear();
        for (map<UINT32, MiscompareDetails>::const_iterator i =
                m_AclwmErrsBySurf.begin(); i != m_AclwmErrsBySurf.end(); ++i)
        {
            const UINT32 surfIdx = i->first;
            const MiscompareDetails & md = i->second;
            JsArray innerArray;

            firstRc = pJs->ToJsval(m_pSurfaces->Name(surfIdx), &jsv);
            innerArray.push_back(jsv);

            ColorUtils::Format fmt = m_pSurfaces->Format(surfIdx);
            firstRc = pJs->ToJsval(ColorUtils::FormatToString(fmt), &jsv);
            innerArray.push_back(jsv);

            firstRc = pJs->ToJsval(md.numBad(), &jsv);
            innerArray.push_back(jsv);

            for (int elem = 0; elem < NumElem; elem++)
            {
                firstRc = pJs->ToJsval(md.sumDiffs[elem], &jsv);
                innerArray.push_back(jsv);
            }

            firstRc = pJs->ToJsval(&innerArray, &jsv);
            outerArray.push_back(jsv);
        }
        firstRc = pJs->ToJsval(&outerArray, &jsv);
        firstRc = pJs->SetPropertyJsval(goldenObj, "SurfErrs", jsv);

    }

    if (!m_StopOnError)
    {
        m_DeferredRcWasRead = true;
        if (m_DeferredRc != OK)
        {
            return m_DeferredRc;          // A per-check failure was "saved up" earlier.
        }
    }

    return firstRc;
}

bool Goldelwalues::IsErrorRateTestCallMissing()
{
    return (!m_StopOnError && !m_DeferredRcWasRead);
}

//------------------------------------------------------------------------------
RC Goldelwalues::DumpImage
(
    int bufIndex,
    UINT32 whenMask,
    bool png,
    UINT32 subdevNum
)
{
    RC             rc = OK;
    string         fname;
    size_t i = 0;
    size_t maxLen = 14;
    size_t maxi;   // last index in filename for loop number

    maxi = maxLen-1;  // max index is max len-1

    string bufName = "";

    if (m_pSurfaces->NumSurfaces() > 1)
    {
        bufName = m_pSurfaces->Name(bufIndex);
        maxi -= bufName.size();
    }

    if (m_TgaName.size() == 0)
    {
        // Default filename prefix is tstnum and a "when" string.
        // For example, if test 10 failed, the name is "10Fa".

        ErrorMap em;
        INT32    tstNum = (em.Test() % 1000);

        fname.append("000");
        fname[0] = (tstNum / 100) + '0';
        fname[1] = ((tstNum/10) % 10) + '0';
        fname[2] = (tstNum % 10) + '0';

        const char * str;

        if      (whenMask & OnStore)
            str = "St";
        else if (whenMask & OnSkip)
            str = "Sk";
        else if (whenMask & (OnBadPixel|OnError))
            str = "Fa";
        else
            str = "Pa";

        fname.append(str);
    }
    else
    {
        // The user specified a filename.

        fname = m_TgaName;

        // Find length of path.
        size_t pathlen = 0;
        i = fname.find_last_of("/\\:");
        if (i != string::npos)
            pathlen = i + 1;

        // Discard extension.
        i = fname.find_last_of(".");
        if ((i != string::npos) && (i > pathlen))
            fname.resize(i);

        // Truncate to fit.
        if (fname.size() > (maxi+1))
            fname.resize(maxi+1, '0');
    }
    // Stick in an underscore "_" if we have room.
    if (maxi - (int)(fname.size()) > 4)
        fname.append("_");

    // i == leftmost index in string to use for loop number.
    i = fname.size();

    // Pre-fill rest of basename with 0s.
    fname.resize(maxi + 1, '0');

    // Use letters i through maxi for loop number, base 36.
    UINT32 loop = m_Loop;
    constexpr UINT32 base = 10; // Using base10 makes it easier to find your failing loop!

    const char digits[36 + 1] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    while (maxi >= i)
    {
        fname[maxi] = digits[loop % base];
        loop = loop / base;
        maxi--;
    }

    fname.append(bufName);

    if (png)
    {
        fname.append(".png");
    }
    else
    {
        fname.append(".tga");
    }

    Printf(Tee::PriNormal, "Dump surface %d to file %s\n",
           bufIndex, fname.c_str());

    UINT32 width = m_pSurfaces->Width(bufIndex);
    UINT32 height = m_pSurfaces->Height(bufIndex);
    UINT32 absPitch = abs(m_pSurfaces->Pitch(bufIndex));
    ColorUtils::Format format = m_pSurfaces->Format(bufIndex);

    if (ColorUtils::PixelBytes(format) > 4)
    {
        // We're dumping an image that is wider than the highest pixel
        // format supported by .TGA/.PNGs.  Dump as a raw image.
        width  = width * ColorUtils::PixelBytes(format) / 4;
        format = ColorUtils::A8R8G8B8;
    }

    switch (format)
    {
        case ColorUtils::VOID32: // 24-bit Z bloated up to 32bpp by glReadPixels
            format = ColorUtils::A8R8G8B8;
            break;

        case ColorUtils::Z16:
            format = ColorUtils::R5G6B5;
            break;

        default:
            break;
    }

    void *surfAddr = &m_SurfaceDataToDump[bufIndex][0];
    if (png)
    {
        rc = ImageFile::WritePng(fname.c_str(),
                                 format,
                                 surfAddr,
                                 width,
                                 height,
                                 absPitch,
                                 m_TgaAlphaToRgb,
                                 m_pSurfaces->Pitch(bufIndex) < 0);
    }
    else
    {
        rc = ImageFile::WriteTga(fname.c_str(),
                                 format,
                                 surfAddr,
                                 width,
                                 height,
                                 absPitch,
                                 m_TgaAlphaToRgb,
                                 m_pSurfaces->Pitch(bufIndex) < 0,
                                 true /*colwertToRgba*/);
    }
    return rc;
}

//------------------------------------------------------------------------------
Goldelwalues::Action Goldelwalues::GetAction() const
{
    return m_Action;
}

//------------------------------------------------------------------------------
UINT32 Goldelwalues::GetSkipCount() const
{
    return m_SkipCount;
}

//------------------------------------------------------------------------------
UINT32 Goldelwalues::GetCodes() const
{
    return m_Codes;
}

//------------------------------------------------------------------------------
string Goldelwalues::GetName() const
{
    return m_PlatformName;
}

//------------------------------------------------------------------------------
bool Goldelwalues::GetStopOnError() const
{
    return m_StopOnError;
}

UINT32 Goldelwalues::GetDumpPng() const
{
    return m_DumpPng;
}

//------------------------------------------------------------------------------
RC Goldelwalues::OverrideSuffix(string newSuffix)
{
    m_NameSuffix = newSuffix;
    return OpenDb();
}

//------------------------------------------------------------------------------
// static class function
//
UINT32 Goldelwalues::DummyExtraCodeFunc(void*)
{
    return 0;
}

//------------------------------------------------------------------------------
Goldelwalues::CalcAlgorithm Goldelwalues::GetCalcAlgorithmHint() const
{
    return m_CalcAlgoHint;
}
//------------------------------------------------------------------------------
UINT32 Goldelwalues::GetSurfChkOnGpuBinSize() const
{
    return m_SurfChkOnGpuBinSize;
}
//------------------------------------------------------------------------------
// Set this callback for golden to get the "Extra" code value.
// Used by GLRandom to store/check zpass pixel counts for example.
void Goldelwalues::AddExtraCodeCallback
(
    const char * name,
    UINT32 (*pFunc)(void*),
    void * data
)
{
    int i = (int)m_Extra.size();
    m_Extra.resize(i+1);
    m_Extra[i].name = name;
    m_Extra[i].pFunc = pFunc;
    m_Extra[i].data = data;

    if (!pFunc)
    {
      MASSERT(0);
      m_Extra[i].pFunc = &DummyExtraCodeFunc;
    }
}

//------------------------------------------------------------------------------
void Goldelwalues::SetIdleCallback
(
    RC (*pIdleCallback)(void *pCallbackObj, void * pCallbackData),
    void *pCallbackObj,
    void *pCallbackData
)
{
    m_pIdleFunc = pIdleCallback;
    m_pIdleCallbackData0 = pCallbackObj;
    m_pIdleCallbackData1 = pCallbackData;
}

GpuDevice *Goldelwalues::GetGpuDevice() const
{
    auto pSubdev = m_pTestDevice->GetInterface<GpuSubdevice>();
    if (pSubdev)
        return pSubdev->GetParentDevice();
    return nullptr;
}

//-----------------------------------------------------------------------------
void Goldelwalues::AddErrorCallback
(
    Goldelwalues::ErrCallbackFuncPtr pFunc,
    void * pData
)
{
    m_ErrCallbackFuncs.push_back(pFunc);
    m_ErrCallbackData.push_back(pData);
}

void Goldelwalues::SetSubdeviceMask(UINT32 mask)
{
    m_SubdevMask = mask;
}

void Goldelwalues::SetCheckCRCsUnique(bool enable)
{
    m_CheckCRCsUnique = enable;
}

//! \brief Check whether the golden value is present in the database
//!
//! \param LoopOrDbIndex : Loop to use to check goldens are present (will be
//!                        rounded up to next generated loop) or DbIndex for
//!                        goldens (which is used as is)
//! \param bDbIndex      : true if the previous parameter is a DbIndex
//! \param pbGoldenPresent : returned boolean indicating whether the golden
//!                          is present
//!
//! \return OK if successful, not OK otherwise
//------------------------------------------------------------------------------
RC Goldelwalues::IsGoldenPresent
(
    UINT32 LoopOrDbIndex,
    bool   bDbIndex,
    bool  *pbGoldenPresent
)
{
    MASSERT(pbGoldenPresent);

    *pbGoldenPresent = false;

    if (NULL == m_pSurfaces)
    {
        MASSERT(!"SetSurfaces wasn't run before Goldelwalues::IsGoldenPresent");
        return RC::SOFTWARE_ERROR;
    }

    // If not using a DbIndex, round the loop up to the next loop where goldens
    // would be present in the database
    if (!bDbIndex &&
        (m_SkipCount != 0) &&
        (LoopOrDbIndex % m_SkipCount != m_SkipCount-1))
    {
        LoopOrDbIndex += m_SkipCount - (LoopOrDbIndex % m_SkipCount) - 1;
    }

    const UINT32 * DbCodeBuf = 0;         // old CRC values from database
    RC rc;

    rc = GoldenDb::GetRec(m_DbHandle, LoopOrDbIndex, &DbCodeBuf);

    *pbGoldenPresent = (rc == OK);

    if (RC::GOLDEN_VALUE_NOT_FOUND == rc)
        rc.Clear();

    return rc;
}

//------------------------------------------------------------------------------
// Script implementation.
//------------------------------------------------------------------------------

// STest
C_(Golden_PrintValues)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    string TestName;
    INT32  Columns;
    if (   (NumArguments != 2)
        || (OK != pJavaScript->FromJsval(pArguments[0], &TestName))
        || (OK != pJavaScript->FromJsval(pArguments[1], &Columns)))
    {
        JS_ReportError(pContext,
                       "Usage: Golden.PrintValues(\"test name\", columns)");
        return JS_FALSE;
    }

    RETURN_RC(GoldenDb::PrintValues(TestName.c_str(), Columns));
}

// STest
C_(Golden_ReadFile)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    string Filename;
    UINT32 Pri       = Tee::PriLow;
    UINT32 DoCompare = false;
    if (   (NumArguments < 1)
        || (OK != pJavaScript->FromJsval(pArguments[0], &Filename))
        || ((NumArguments >= 2) &&
            (OK != pJavaScript->FromJsval(pArguments[1], &Pri)))
        || ((NumArguments >= 3) &&
            (OK != pJavaScript->FromJsval(pArguments[2], &DoCompare)))
        || (NumArguments > 3))
    {
        JS_ReportError(
            pContext,
            "Usage: Golden.ReadFile(\"filename\", OutPri, DoCompare)");
        return JS_FALSE;
    }

    RETURN_RC(GoldenDb::ReadFile(Filename.c_str(), Pri, (DoCompare != 0)));
}

// STest
C_(Golden_UnmarkAllRecs)
{
    GoldenDb::UnmarkAllRecs();
    RETURN_RC(OK);
}

// STest
C_(Golden_MarkRecs)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    string TestName;
    UINT32 DbIndex;
    if (   (NumArguments != 2)
        || (OK != pJavaScript->FromJsval(pArguments[0], &TestName))
        || (OK != pJavaScript->FromJsval(pArguments[1], &DbIndex)))
    {
        JS_ReportError(pContext,
                       "Usage: Golden.MarkRecs(\"testname\", dbIndex)");
        return JS_FALSE;
    }

    RETURN_RC(GoldenDb::MarkRecs(TestName.c_str(), DbIndex));
}

// STest
C_(Golden_MarkRecsWithName)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    string Name;
    if (   (NumArguments != 1)
        || (OK != pJavaScript->FromJsval(pArguments[0], &Name)))
    {
        JS_ReportError(pContext, "Usage: Golden.MarkRecsWithName(\"Name\")");
        return JS_FALSE;
    }

    RETURN_RC(GoldenDb::MarkRecsWithName(Name.c_str()));
}

// STest
C_(Golden_WriteFile)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    string Filename;
    bool   MarkedOnly = false;
    UINT32 StatusPri = (UINT32)Tee::PriHigh;
    if (   (NumArguments < 1)
        || (OK != pJavaScript->FromJsval(pArguments[0], &Filename))
        || (   (NumArguments >= 2)
            && (OK != pJavaScript->FromJsval(pArguments[1], &MarkedOnly)))
        || (   (NumArguments >= 3)
            && (OK != pJavaScript->FromJsval(pArguments[2], &StatusPri)))
        || (NumArguments > 3))
    {
        JS_ReportError(
                pContext,
                "Usage: Golden.WriteFile(\"filename\", markedOnly, statusPri)");
        return JS_FALSE;
    }

    RETURN_RC(GoldenDb::WriteFile(Filename.c_str(), MarkedOnly, StatusPri));
}

//--------------------------------------------------------------------------------------------------
// Golden Self Check Implementation
//--------------------------------------------------------------------------------------------------
GoldenSelfCheck::GoldenSelfCheck() :
    m_LogMode(Skip),
    m_LogState(lsUnknown),
    m_pGValues(nullptr)
{}

//--------------------------------------------------------------------------------------------------
//Initialize the GoldenSelfCheck object
void GoldenSelfCheck::Init(const char * szTestName, Goldelwalues * pGVals)
{
    m_TestName = szTestName;
    m_pGValues = pGVals;
}

//--------------------------------------------------------------------------------------------------
// Set the logging mode to Skip,Check, or Store
void GoldenSelfCheck::SetLogMode(UINT32 logMode)
{
    m_LogMode = static_cast<eLogMode>(logMode);
}
//--------------------------------------------------------------------------------------------------
// Begin a new self check loggin session.
void GoldenSelfCheck::LogBegin(size_t numItems)
{
    MASSERT(m_LogState == lsUnknown || m_LogState == lsFinish);
    MASSERT(m_pGValues);
    m_LogState = lsBegin;

    m_LogItemCrcs.resize(numItems);

    size_t i;
    for (i = 0; i < numItems; i++)
        m_LogItemCrcs[i] = ~(UINT32)0;
}

//------------------------------------------------------------------------------
// Update the crc value for 'item' using data supplied.
UINT32 GoldenSelfCheck::LogData(int item, const void * data, size_t dataSize)
{
    MASSERT(m_LogState == lsBegin || m_LogState == lsLog);
    if (static_cast<size_t>(item) >= m_LogItemCrcs.size())
    {
        MASSERT(!"GoldenSelfCheck: Invalid item index");
        return 0;
    }

    m_LogState = lsLog;

    if (m_LogMode != Skip)
    {
        UINT32 tmp = m_LogItemCrcs[item];

        const unsigned char * p = (const unsigned char *)data;
        while (dataSize--)
        {
            tmp = (tmp >> 3) | ((tmp & 7) << 28);
            tmp = tmp ^ *p++;
        }
        m_LogItemCrcs[item] = tmp;
    }

    return m_LogItemCrcs[item];
}

//------------------------------------------------------------------------------
// Finish out this self-check logging session and either store the crc values to
// the goldenDB or check the crc values against the goldenDB.
RC GoldenSelfCheck::LogFinish(const char * label, UINT32 dbIndex)
{
    MASSERT(m_LogState == lsBegin || m_LogState == lsLog);
    m_LogState = lsFinish;

    RC rc = OK;

    if ((m_LogMode == Skip) || (m_LogItemCrcs.size() == 0))
        return OK;

    // Get a handle (database index) into the golden db.
    string testName(m_TestName);
    testName.append(label);
    string recName = m_pGValues->GetName();

    GoldenDb::RecHandle dbHandle;
    if (OK != (rc = GoldenDb::GetHandle(testName.c_str(),
                                        recName.c_str(),
                                        0,
                                        (UINT32)m_LogItemCrcs.size(),
                                        &dbHandle)))
    {
        m_LogMode = Skip;
        return rc;
    }

    if (m_LogMode == Store)
    {
        // Store the logged data to the golden db.

        if (OK != (rc = GoldenDb::PutRec(dbHandle, dbIndex,
                                         &(m_LogItemCrcs[0]), false)))
        {
            m_LogMode = Skip;
            return rc;
        }
    }
    else
    {
        // Check the logged data against the "correct" values from the golden db.

        MASSERT(m_LogMode == Check);
        const UINT32 * pStoredData;

        if (OK != (rc = GoldenDb::GetRec(dbHandle, dbIndex, &pStoredData)))
        {
            m_LogMode = Skip;
            return rc;
        }
        size_t i;
        for (i = 0; i < m_LogItemCrcs.size(); i++)
        {
            if (pStoredData[i] != m_LogItemCrcs[i])
            {
                Printf(Tee::PriHigh,
                   "!!! dbIndex:%d %s item:%d is inconsistent! (expected 0x%x, got 0x%x)\n",
                   dbIndex,
                   label,
                   (unsigned int)i,
                   pStoredData[i],
                   m_LogItemCrcs[i]);
                if (OK == rc)
                {
                    rc = RC::SOFTWARE_ERROR;
                }
            }
        }
        return rc;
    }
    return OK;
}
