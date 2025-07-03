/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file clkbase.h
//! \brief Header file of base clk hal
//!

#include <vector>
#include <map>
#include <cmath>
#include <stdio.h>
#include <lwRmApi.h>
#include "core/include/tee.h"
#include "gpu/tests/rm/utility/rmhalmodule.h"
#include "class/cl5097.h"
#include "class/cl9097.h"
#include "core/include/platform.h"
#include "gpu/tests/rmtest.h"
#include "core/include/utility.h"
#include "gpu/utility/rmclkutil.h"
#include "gpu/perf/perfsub.h"
#include "ctrl/ctrl2080/ctrl2080clk.h"
#include "ctrl/ctrl2080/ctrl2080fb.h"
#include "ctrl/ctrl2080/ctrl2080perf.h"
#include "ctrl/ctrl208f/ctrl208fclk.h"
#include "core/include/threadsync.h"
#include "gpu/tests/rm/utility/rmtestsyncpoints.h"

//
// Please do not copy the below macros based on device Id  anywhere outside the clk utils
// It s use is not recommended and try to use HasFeature IsClassSupported wherever possible
//

#define IsFERMI(p)           false
#define IsFERMIorBetter(p)   true
#define IsKEPLER(p)          ((p >= Gpu::GK104) && (p <= Gpu::GK208S))
#define IsKEPLERorBetter(p)  true
#define IsMAXWELLorBetter(p) ((p >= Gpu::GM107))
#define IsTESLA(p)           false
#define IsGM20XorBetter(p)   ((p >= Gpu::GM200))
#define IsPASCALorBetter(p)  ((p >= Gpu::GP100))
#define IsGP10X(p)           ((p >= Gpu::GP102) && (p <= Gpu::GP108))
#define IsGP10XorBetter(p)   ((p >= Gpu::GP102))
#define IsGP107orBetter(p)   ((p >= Gpu::GP107))

class ClockUtil : public RMTHalModule
{
public:
    //
    // Common utility functions and members
    //
    static ClockUtil * CreateModule(LwU32 hClient, GpuSubdevice* pSubdev);
    virtual RC PrintClocks(const LwU32 clkDomains);
    virtual RC GetClocks(
        const LwU32 clkDomains,
        vector<LW2080_CTRL_CLK_INFO>& vNewClkInfo);
    virtual RC GetClocks(
        vector<LW2080_CTRL_CLK_INFO>& vNewClkInfo);
    virtual RC GetEffectiveClocks(
        vector<LW2080_CTRL_CLK_EXTENDED_INFO>& vExtendedClkInfo);
    virtual RC GetEffectiveClocks(
        const LwU32 clkDomains,
        vector<LW2080_CTRL_CLK_EXTENDED_INFO>& vExtendedClkInfo);
    virtual LwBool clkIsRootClock(LwU32 clkDomain);
    virtual LwBool clkIsDerivativeClock(LwU32 clkDomain);
    virtual LwBool clkIsOneSrc(LwU32 clkSrc);
    virtual RC ProgramAndVerifyClockFreq(
        const vector<LW2080_CTRL_CLK_INFO>& vSetClkInfo,
        vector<LW2080_CTRL_CLK_INFO>& vNewClkInfo);
    virtual RC VerifyClockFreq(
        const vector<LW2080_CTRL_CLK_INFO>& vTargetClkInfo,
        vector<LW2080_CTRL_CLK_INFO>& vActualClkInfo);
    virtual RC VerifyClockSrc(
        const vector<LW2080_CTRL_CLK_INFO>& vActualClkInfo,
        LwBool bIsTargetPathVCO);
    virtual RC VerifyClockSrc(
        const vector<LW2080_CTRL_CLK_INFO>& vActualClkInfo,
        LwU32 clkSrc);
    virtual LwU32 GetFreqTolerance100X(const LW2080_CTRL_CLK_INFO ClkInfo);
    virtual RC clkSelectTargetFreq(
        LwBool bSwitchToVCOPath,
        LwU32  clkDomain,
        LwU32  refFreqKHz,
        LwU32& targetFreqKHz);
    virtual LwU32 GetProgrammableDomains();
    virtual LwU32 GetAllDomains();
    RC clkGetPllInfo(LwU32 pll, LW2080_CTRL_CLK_PLL_INFO &clkPllInfo);

    LwU32 GetNumProgrammableDomains();
    LwU32 GetNumDomains();
    RC GetProgrammableClocks(vector<LW2080_CTRL_CLK_INFO>& vClkInfo);
    RC GetAllClocks(vector<LW2080_CTRL_CLK_INFO>& vClkInfo);
    RC GetClkSrcFreqKHz(LwU32 clkSrc, LwU32& clkSrcFreqKHz);

    RmtClockUtil m_ClkUtil;

    // Functions that are a complete test in themselves
    virtual RC ClockSanityTest();
    virtual RC ClockPathTest();
    virtual RC ClockNDivSlowdownTest();
    virtual ~ClockUtil();
protected:
    ClockUtil(LwU32 hClient, LwU32 hDev, LwU32 hSubDev);

protected:
    LwU32 m_ProgClkDomains;
    LwU32 m_AllClkDomains;

};

class ClockUtilGF100 : public ClockUtil
{
public:
    ClockUtilGF100(LwU32 hClient, LwU32 hDev, LwU32 hSubDev);
    virtual ~ClockUtilGF100();
public:
    RC PrintClocks(const LwU32 clkDomains);
    LwU32 GetProgrammableDomains();
    RC ProgramAndVerifyClockFreq(
        const vector<LW2080_CTRL_CLK_INFO>& vSetClkInfo,
        vector<LW2080_CTRL_CLK_INFO>& vNewClkInfo);
    RC VerifyClockFreq(
        const vector<LW2080_CTRL_CLK_INFO>& vTargetClkInfo,
        vector<LW2080_CTRL_CLK_INFO>& vActualClkInfo);
    RC clkSelectTargetFreq(
        LwBool bSwitchToVCOPath,
        LwU32  clkDomain,
        LwU32  refFreqKHz,
        LwU32& targetFreqKHz);
    LwBool clkIsRootClock(LwU32 clkDomain);
    LwBool clkIsDerivativeClock(LwU32 clkDomain);

    RC ClockPathTest();
    RC ClockSanityTest();

protected:
    virtual LwU32 GetOneSrcFreqKHz();
    virtual LwU32 GetXtalFreqKHz();
    virtual LwU32 GetRamType();
    virtual RC TestRootClocks(vector<LW2080_CTRL_CLK_INFO>& vClkInfo);
    virtual RC TestDerivativeClocks(vector<LW2080_CTRL_CLK_INFO>& vClkInfo);
    virtual RC TestCoreClocks(vector<LW2080_CTRL_CLK_INFO>& vClkInfo);
    LwU32 m_oneSrcFreqKHz;
    LwU32 m_xtalFreqKHz;
    LW2080_CTRL_FB_INFO m_RamType;

private:
    map < LwU32, LW208F_CTRL_CLK_GET_OPERATIONAL_FREQ_LIMITS_PARAMS > mClkFreqLUT;
};

class ClockUtilGF117 : public ClockUtilGF100
{
public:
    ClockUtilGF117(LwU32 hClient, LwU32 hDev, LwU32 hSubDev);
    virtual ~ClockUtilGF117() {};

    LwU32 GetProgrammableDomains();
};

class ClockUtilGK104 : public ClockUtilGF117
{
public:
    ClockUtilGK104(LwU32 hClient, LwU32 hDev, LwU32 hSubDev);
    virtual ~ClockUtilGK104() {};

public :
    LwU32  GetProgrammableDomains();
    LwBool clkIsRootClock(LwU32 clkDomain);
    LwBool clkIsDerivativeClock(LwU32 clkDomain);
    RC     ClockNDivSlowdownTest();

private:
    RC clkSetPllSpelwlatively(LwU32 pll, LwU32 targetFreqKHz, LwBool bAnyMdiv, LW2080_CTRL_CLK_PLL_INFO &clkPllInfo);
    RC clkForceNDivSlowdown(LwU32 clkDomain, LwBool bForceSlowdown);
    RC clkPreventNDivRampUp(LwU32 clkDomain, LwBool bPreventRampUp);
    RC clkVerifySlowdownFreq(LwU32 clkDomain);
    RC clkProgramAndVerifySlowdownFreq(LW2080_CTRL_CLK_INFO &clkInfo, LwBool bForceVCO, LwBool bForceBypass, LwBool bAnyMdiv);
};

class ClockUtilGK20A : public ClockUtilGK104
{
public:
    ClockUtilGK20A(LwU32 hClient, LwU32 hDev, LwU32 hSubDev);
    virtual ~ClockUtilGK20A() {};
    RC ClockSanityTest();

protected:
    virtual LwU32 GetOneSrcFreqKHz();

public :
    LwBool clkIsRootClock(LwU32 clkDomain);
    LwBool clkIsDerivativeClock(LwU32 clkDomain);

private:
    LwU32 clkGetCoreClkPllSource(LwU32 clkDomain);
};

