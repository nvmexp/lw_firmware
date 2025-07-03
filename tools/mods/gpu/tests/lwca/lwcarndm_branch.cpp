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
class BranchLaunchParam : public LaunchParam
{
public:
    BranchLaunchParam()
        : LaunchParam()
    {
        memset(&inputTile, 0, sizeof(inputTile));
    }
    virtual ~BranchLaunchParam() { }

    Tile inputTile;
};

//-----------------------------------------------------------------------------
// A kernel that tests branching operations.
class BranchSubtest: public Subtest
{
public:
    BranchSubtest(LwdaRandomTest * pParent)
        : Subtest(pParent, "DivergentBranching")
    {
    }
    virtual ~BranchSubtest() { }

    virtual LaunchParam * NewLaunchParam()
    {
        BranchLaunchParam * pLP = new BranchLaunchParam;
        pLP->pSubtest = this;
        return pLP;
    }

    virtual void FillLaunchParam(LaunchParam* pLPbase)
    {
        MASSERT(pLPbase->pSubtest == this);
        BranchLaunchParam * pLP = static_cast<BranchLaunchParam*>(pLPbase);

        // We "own" the same corresponding rect in all three surfaces, but
        // only the output surface has the right gpu address already.
        pLP->inputTile = pLP->outputTile;
        pLP->inputTile.elements = DevTileAddr(mxInputA, &pLP->inputTile);
    }

    virtual void FillSurfaces(LaunchParam* pLPbase)
    {
        MASSERT(pLPbase->pSubtest == this);
        BranchLaunchParam * pLP = static_cast<BranchLaunchParam*>(pLPbase);

        FillTileUint32(&pLP->inputTile, Pick(CRTST_BRANCH_INPUT_MIN),
                       Pick(CRTST_BRANCH_INPUT_MAX), pLP);
        FillTileUint32(&pLP->outputTile, Pick(CRTST_BRANCH_OUTPUT_MIN),
                       Pick(CRTST_BRANCH_OUTPUT_MAX), pLP);
    }

    virtual RC Launch(LaunchParam * pLPbase)
    {
        MASSERT(pLPbase->pSubtest == this);
        BranchLaunchParam * pLP = static_cast<BranchLaunchParam*>(pLPbase);

        SetLaunchDim(pLP);
        return GetFunction().Launch(GetStream(pLP->streamIdx),
                                    pLP->inputTile, pLP->outputTile, pLP->gFP);
    }

    virtual void Print(Tee::Priority pri, const LaunchParam * pLPbase)
    {
        MASSERT(pLPbase->pSubtest == this);
        const BranchLaunchParam * pLP = static_cast<const BranchLaunchParam*>(pLPbase);

        PrintTile(pri, " In", pLP->inputTile);
        PrintTile(pri, "Out", pLP->outputTile);
    }
};

Subtest * NewBranchSubtest(LwdaRandomTest * pTest)
{
    return new BranchSubtest(pTest);
}

} // namespace LwdaRandom
