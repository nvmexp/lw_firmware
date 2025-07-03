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
class MatrixInitLaunchParam : public LaunchParam
{
public:
    MatrixInitLaunchParam()
        : LaunchParam() { }
    virtual ~MatrixInitLaunchParam() { }
};

//-----------------------------------------------------------------------------
// A kernel that tests GPU surface inits.
class MatrixInitSubtest: public Subtest
{
public:
    MatrixInitSubtest(LwdaRandomTest * pParent)
        : Subtest(pParent, "MatrixInitTest")
    {
        m_FillType = GetRand()->GetRandom() % ft_numTypes;

        // These sizes should be the size of the indexed elements for
        // detemining grid width and picking output tiles.
        // ft_uint16 and ft_uint08 should use the size of UINT32
        // since the GPU fills in 32 bits at a time (as required by the
        // SeqInt Subtest)
        m_FillTypeSize[ft_uint32] = sizeof(UINT32);
        m_FillTypeSize[ft_uint16] = sizeof(UINT32);
        m_FillTypeSize[ft_uint08] = sizeof(UINT32);
        m_FillTypeSize[ft_float32] = sizeof(FLOAT32);
        m_FillTypeSize[ft_float64] = sizeof(FLOAT64);
    }
    virtual ~MatrixInitSubtest() { }

    virtual UINT32 ElementSize() const
    {
        return m_FillTypeSize[m_FillType];
    }

    virtual LaunchParam * NewLaunchParam()
    {
        MatrixInitLaunchParam * pLP = new MatrixInitLaunchParam;
        pLP->pSubtest = this;
        return pLP;
    }

    virtual void FillLaunchParam(LaunchParam* pLPbase)
    {
        MASSERT(pLPbase->pSubtest == this);
        MatrixInitLaunchParam * pLP = static_cast<MatrixInitLaunchParam*>(pLPbase);

        pLP->checkMat = true;
    }

    virtual void FillSurfaces(LaunchParam* pLPbase)
    {
        MASSERT(pLPbase->pSubtest == this);
        MatrixInitLaunchParam * pLP = static_cast<MatrixInitLaunchParam*>(pLPbase);

        switch(m_FillType)
        {
            default:
            {
                FillTileUint32(&pLP->outputTile, Pick(CRTST_MAT_INIT_INT_MIN),
                            Pick(CRTST_MAT_INIT_INT_MAX), pLP);
                break;
            }
            case ft_float32:
            {
                if (GetRand()->GetRandom() % 2)
                    FillTilesFloat32Exp(NULL, NULL, &pLP->outputTile, pLP);
                else
                    FillTileFloat32Range(&pLP->outputTile, FPick(CRTST_MAT_INIT_FLOAT_MIN),
                            FPick(CRTST_MAT_INIT_FLOAT_MAX), 0, pLP);
                break;
            }
            case ft_float64:
            {
                FillTilesFloat64Exp(NULL, NULL, &pLP->outputTile, pLP);
                break;
            }
            case ft_uint16:
            {
                FillTileUint16(&pLP->outputTile, Pick(CRTST_MAT_INIT_INT_MIN),
                            Pick(CRTST_MAT_INIT_INT_MAX), pLP);
                break;
            }
            case ft_uint08:
            {
                FillTileUint08(&pLP->outputTile, Pick(CRTST_MAT_INIT_INT_MIN),
                            Pick(CRTST_MAT_INIT_INT_MAX), pLP);
                break;
            }
        }
    }

    virtual RC Launch(LaunchParam * pLPbase)
    {
        MASSERT(pLPbase->pSubtest == this);
        MatrixInitLaunchParam * pLP = static_cast<MatrixInitLaunchParam*>(pLPbase);

        SetLaunchDim(pLP);
        return GetFunction().Launch(GetStream(pLP->streamIdx), pLP->outputTile, pLP->gFP);
    }

    virtual void Print(Tee::Priority pri, const LaunchParam * pLPbase)
    {
        MASSERT(pLPbase->pSubtest == this);
        const MatrixInitLaunchParam * pLP = static_cast<const MatrixInitLaunchParam*>(pLPbase);

        Printf(pri, "MatrixInit fill type: %d\n", m_FillType);
        PrintTile(pri, "Out", pLP->outputTile);
    }

private:
    UINT32 m_FillType;
    UINT32 m_FillTypeSize[ft_numTypes];
};

Subtest * NewMatrixInitSubtest(LwdaRandomTest * pTest)
{
    return new MatrixInitSubtest(pTest);
}

} // namespace LwdaRandom
