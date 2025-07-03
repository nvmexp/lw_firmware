/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwdarndm.h"
#include "gpu/tests/lwca/random/lwrkernel.h"

namespace LwdaRandom
{
class DebugLaunchParam : public LaunchParam
{
public:
    DebugLaunchParam()
        : LaunchParam()
    {
        memset(&inputTile, 0, sizeof(inputTile));
    }
    virtual ~DebugLaunchParam() { }

    Tile inputTile;
};

//-----------------------------------------------------------------------------
// A very simple kernel that copies it's input tile to its output tile.
class DebugSubtest: public Subtest
{
public:
    DebugSubtest(LwdaRandomTest * pParent)
        : Subtest(pParent, "SmallDebugTest")
    {
    }
    virtual ~DebugSubtest() { }

    virtual LaunchParam * NewLaunchParam()
    {
        DebugLaunchParam * pLP = new DebugLaunchParam;
        pLP->pSubtest = this;
        return pLP;
    }

    virtual void FillLaunchParam(LaunchParam* pLPbase)
    {
        MASSERT(pLPbase->pSubtest == this);
        DebugLaunchParam * pLP = static_cast<DebugLaunchParam*>(pLPbase);

        // We "own" the same corresponding rect in all three surfaces, but
        // only the output surface has the right gpu address already.
        pLP->inputTile = pLP->outputTile;
        pLP->inputTile.elements = DevTileAddr(mxInputA, &pLP->inputTile);
    }

    virtual void FillSurfaces(LaunchParam* pLPbase)
    {
        MASSERT(pLPbase->pSubtest == this);
        DebugLaunchParam * pLP = static_cast<DebugLaunchParam*>(pLPbase);

        FillTileUint32(&pLP->inputTile, Pick(CRTST_DEBUG_INPUT),
                       Pick(CRTST_DEBUG_INPUT), pLP);
        FillTileUint32(&pLP->outputTile, Pick(CRTST_DEBUG_OUTPUT),
                       Pick(CRTST_DEBUG_OUTPUT), pLP);
    }

    virtual RC Launch(LaunchParam * pLPbase)
    {
        MASSERT(pLPbase->pSubtest == this);
        DebugLaunchParam * pLP = static_cast<DebugLaunchParam*>(pLPbase);

        SetLaunchDim(pLP);
        return GetFunction().Launch(GetStream(pLP->streamIdx),
                                    pLP->inputTile, pLP->outputTile, pLP->gFP);
    }

    virtual void Print(Tee::Priority pri, const LaunchParam * pLPbase)
    {
        MASSERT(pLPbase->pSubtest == this);
        const DebugLaunchParam * pLP = static_cast<const DebugLaunchParam*>(pLPbase);

        PrintTile(pri, " In", pLP->inputTile);
        PrintTile(pri, "Out", pLP->outputTile);
    }
};

Subtest * NewDebugSubtest(LwdaRandomTest * pTest)
{
    return new DebugSubtest(pTest);
}

} // namespace LwdaRandom
