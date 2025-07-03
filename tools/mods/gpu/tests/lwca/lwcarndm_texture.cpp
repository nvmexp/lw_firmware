/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2012,2015,2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwdarndm.h"

namespace LwdaRandom
{

//-----------------------------------------------------------------------------
// Describe a kernel launch for the Texture kernels of LwdaRandom.
//
class TextureLaunchParam : public LaunchParam
{
public:
    TextureLaunchParam() : LaunchParam() { }
    virtual ~TextureLaunchParam() { }

    void Print(Tee::Priority pri) const;

    UINT32 numOps = 0;
    UINT32 opIdx  = 0;
};

void TextureLaunchParam::Print(Tee::Priority pri) const
{
    Printf(pri,
           "Kernel Params:\n"
           "NumOps:%d OpIdx:%d\n",
           numOps,
           opIdx);
    PrintTile(pri, "MatrixC", outputTile);
}

//-----------------------------------------------------------------------------
const char * TextureOpcodeToString(UINT32 Opcode)
{
    #define OPCODE_STRING(x)   case x: return #x
    switch (Opcode)
    {
        default: return "Unknown Opcode";
        OPCODE_STRING(CRTST_KRNL_TEX_OPCODE_tex1D);
        OPCODE_STRING(CRTST_KRNL_TEX_OPCODE_tex2D);
        OPCODE_STRING(CRTST_KRNL_TEX_OPCODE_tex3D);
        OPCODE_STRING(CRTST_KRNL_TEX_OPCODE_tex1DL);
        OPCODE_STRING(CRTST_KRNL_TEX_OPCODE_tex2DL);
        OPCODE_STRING(CRTST_KRNL_TEX_OPCODE_tld4);
        OPCODE_STRING(CRTST_KRNL_TEX_OPCODE_texclamphi);
        OPCODE_STRING(CRTST_KRNL_TEX_OPCODE_texclamplo);
        OPCODE_STRING(CRTST_KRNL_TEX_OPCODE_texwraphi);
        OPCODE_STRING(CRTST_KRNL_TEX_OPCODE_texwraplo);
    }
}

//-----------------------------------------------------------------------------
// Hooks the TextureTest kernel to the core of LwdaRandom.
//
class TextureSubtest: public Subtest
{
public:
    TextureSubtest
    (
        LwdaRandomTest       * pParent
       ,const Lwca::Instance * pInst
       ,Lwca::Device           dev
    )
        : Subtest(pParent, "TextureTest")
         ,m_OpVec(TexOpsSize,
                  SeqKernelOp(),
                  Lwca::HostMemoryAllocator<SeqKernelOp>(pInst, dev))
    {
    }
    virtual ~TextureSubtest() { }
    virtual RC Setup();
    virtual RC Cleanup();
    virtual RC RandomizeOncePerRestart(UINT32 Frame);
    virtual LaunchParam * NewLaunchParam();
    virtual void FillLaunchParam(LaunchParam* pLP);
    virtual void FillSurfaces(LaunchParam* pLP);
    virtual RC Launch(LaunchParam * pLP);
    virtual void Print(Tee::Priority pri, const LaunchParam * pLP);

private:
    //! Per-frame global array used in kernel.
    //! This maps to TexKernelOps in lwrkernel.lw.
    Lwca::Global m_TexKernelOps;

    //! System copy of TexKernelOps.
    typedef vector<SeqKernelOp, Lwca::HostMemoryAllocator<SeqKernelOp> > OpVec;
    OpVec m_OpVec;

protected:
    void ReadKernelOps();
    const SeqKernelOp& GetTexKernelOp(UINT32 idx);
};

RC TextureSubtest::Setup()
{
    RC rc;
    CHECK_RC(Subtest::Setup());
    m_TexKernelOps = GetModule()->GetGlobal("TexKernelOps");
    return rc;
}

RC TextureSubtest::Cleanup()
{
    // Use erase because experience shows it is faster than clear
    m_OpVec.erase(m_OpVec.begin(), m_OpVec.end());
    return OK;
}

RC TextureSubtest::RandomizeOncePerRestart(UINT32 Frame)
{
    for (size_t i=0; i < m_OpVec.size(); i++)
    {
        SeqKernelOp* pOp = &m_OpVec[i];
        pOp->OpCode   = Pick(CRTST_KRNL_TEX_OPCODE);

        // Enforce requirements:
        //   OpCode should be legal.
        //   Operands should be legal.
        //   Operand1 and Operand2 should be different.

        if (pOp->OpCode < CRTST_KRNL_TEX_OPCODE_FIRST)
            pOp->OpCode = CRTST_KRNL_TEX_OPCODE_FIRST;
        if (pOp->OpCode > CRTST_KRNL_TEX_OPCODE_LAST)
            pOp->OpCode = CRTST_KRNL_TEX_OPCODE_LAST;
    }

    // Copy to constant device memory
    m_TexKernelOps.Set(&m_OpVec[0], 0/*stream*/,
                         TexOpsSize * sizeof(SeqKernelOp));
    return OK;
}

void TextureSubtest::ReadKernelOps()
{
    m_TexKernelOps.Get(&m_OpVec[0], 0/*stream*/,
                         TexOpsSize * sizeof(SeqKernelOp));
}

LaunchParam * TextureSubtest::NewLaunchParam()
{
    TextureLaunchParam * pLP = new TextureLaunchParam;
    pLP->pSubtest = this;
    return pLP;
}

void TextureSubtest::FillLaunchParam(LaunchParam * pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    TextureLaunchParam * pLP = static_cast<TextureLaunchParam*>(pLPbase);

    // Randomly choose how many ops to use during this launch.
    pLP->opIdx = Pick(CRTST_KRNL_TEX_OP_IDX) % TexOpsSize;
    pLP->numOps = min(Pick(CRTST_KRNL_TEX_NUM_OPS), TexOpsSize - pLP->opIdx);

}

void TextureSubtest::FillSurfaces(LaunchParam* pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    TextureLaunchParam * pLP = static_cast<TextureLaunchParam*>(pLPbase);

    FillTilesFloat32Exp(NULL, NULL, &pLP->outputTile, pLP);
}

RC TextureSubtest::Launch(LaunchParam * pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    TextureLaunchParam * pLP = static_cast<TextureLaunchParam*>(pLPbase);

    // For Opcode coverage stats
    coverageTracker.OpcodeAdd<OpVec>(CRTST_CTRL_KERNEL_Texture,
                                     pLP->numOps,
                                     pLP->opIdx,
                                     m_OpVec);

    SetLaunchDim(pLP);
    return GetFunction().Launch(GetStream(pLP->streamIdx),
           pLP->outputTile, pLP->numOps, pLP->opIdx, pLP->gFP);
}

void TextureSubtest::Print(Tee::Priority pri, const LaunchParam * pLPbase)
{
    MASSERT(pLPbase->pSubtest == this);
    const TextureLaunchParam * pLP = static_cast<const TextureLaunchParam*>(pLPbase);

    pLP->Print(pri);

    for (UINT32 i = 0; i < pLP->numOps; i++)
    {
        const int idx = pLP->opIdx + i;
        Printf(pri, "Index:%d OpCode:%s \n",
               idx,
               TextureOpcodeToString(m_OpVec[idx].OpCode));
    }
}

const SeqKernelOp& TextureSubtest::GetTexKernelOp(UINT32 idx)
{
    return m_OpVec[idx];
}

Subtest * NewTextureSubtest
(
    LwdaRandomTest       * pTest,
    const Lwca::Instance * pInst,
    Lwca::Device           dev
)
{
    return new TextureSubtest(pTest, pInst, dev);
}

} // namespace LwdaRandom
