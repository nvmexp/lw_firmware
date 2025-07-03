/* * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "pascal/gp100/dev_graphics_nobundle.h"
#include "rmt_alphabetacb.h"
extern bool isSemaReleaseSuccess;

void AlphaBetaCBTest::PascalGetBetaInitValue(GpuSubdevice *pSubdev)
{
    m_cbInitial = DRF_VAL( _PGRAPH, _PRI_DS_TGA_CONSTRAINTLOGIC_BETA, _CBSIZE,
                    pSubdev->RegRd32( LW_PGRAPH_PRI_DS_TGA_CONSTRAINTLOGIC_BETA )) / 4;
    Printf(Tee::PriHigh,"%s: m_cbInitial=0x%08x\n", __FUNCTION__, m_cbInitial);

}

void AlphaBetaCBTest::PascalGetAlphaInitValue(GpuSubdevice *pSubdev)
{
    m_alphaCbInitial = DRF_VAL( _PGRAPH, _PRI_DS_TGA_CONSTRAINTLOGIC_ALPHA, _CBSIZE,
                       pSubdev->RegRd32( LW_PGRAPH_PRI_DS_TGA_CONSTRAINTLOGIC_ALPHA )) / 4;
    Printf(Tee::PriHigh,"%s: m_alphaCbInitial=0x%08x\n", __FUNCTION__, m_alphaCbInitial);
}

//! \brief Check current Cirlwlar buffer register value
//!
//! \sa CheckPascalCBValue
//------------------------------------------------------------------------
RC AlphaBetaCBTest::CheckPascalCBValue(UINT32 ch, UINT32 expected)
{
    RC  rc;
    UINT32  actual;

    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();

    // Avoid race condition of reading register before method completion
    CHECK_RC(m_pCh[ch]->WaitIdle(m_TimeoutMs));

    // Read the current register value
    pSubdev->CtxRegRd32(m_hCh[ch], LW_PGRAPH_PRI_DS_TGA_CONSTRAINTLOGIC_BETA, &actual);

    Printf(Tee::PriHigh,"%s: actual=0x%08x expected=0x%08x\n", __FUNCTION__,
           (DRF_VAL( _PGRAPH, _PRI_DS_TGA_CONSTRAINTLOGIC_BETA, _CBSIZE, actual) / 4), expected);

    // Register setting should be equal to expected value
    KASSERT( ( (DRF_VAL( _PGRAPH, _PRI_DS_TGA_CONSTRAINTLOGIC_BETA, _CBSIZE, actual) / 4) == expected ),
             "AlphaBetaCBTest::CheckPascalCBValue: CB register value incorrect\n", rc );

    return rc;
}

//! \brief Check current Alpha Cirlwlar buffer register value
//!
//! \sa CheckPascalAlphaCBValue
//------------------------------------------------------------------------
RC AlphaBetaCBTest::CheckPascalAlphaCBValue(UINT32 ch, UINT32 expected)
{
    RC  rc;
    UINT32  actual;

    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();

    // Avoid race condition of reading register before method completion
    CHECK_RC(m_pCh[ch]->WaitIdle(m_TimeoutMs));

    // Read the current register value
    pSubdev->CtxRegRd32(m_hCh[ch], LW_PGRAPH_PRI_DS_TGA_CONSTRAINTLOGIC_ALPHA, &actual);

    Printf(Tee::PriHigh,"%s: actual=0x%08x expected=0x%08x\n", __FUNCTION__,
           (DRF_VAL( _PGRAPH, _PRI_DS_TGA_CONSTRAINTLOGIC_ALPHA, _CBSIZE, actual) / 4), expected);

    // Register setting should be equal to (expected value)
    KASSERT( ( (DRF_VAL( _PGRAPH, _PRI_DS_TGA_CONSTRAINTLOGIC_ALPHA, _CBSIZE, actual) / 4) == expected ),
             "AlphaBetaCBTest::CheckPascalAlphaCBValue: Alpha CB register value incorrect\n", rc );

    return rc;
}
