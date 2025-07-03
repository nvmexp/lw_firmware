/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _SPARSETEX_COPY_AMAP_H_
#define _SPARSETEX_COPY_AMAP_H_

#include "mdiag/utils/types.h"

#include "fb_common.h"
#include "gpu/include/gralloc.h"

class SparseTexCopyAmapTest : public FBTest {
public:
    SparseTexCopyAmapTest(ArgReader *params);
    virtual ~SparseTexCopyAmapTest();

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    virtual void Run(void);
    virtual void CleanUp(void);

private:
    int m_LocalStatus;
    UINT32 m_EnablePrint;

    UINT64 m_PhysAddrSrc;
    UINT64 m_PhysAddrSrcAlias;
    UINT64 m_PhysAddrDst;
    UINT64 m_PhysAddrDstAlias;

    DmaBuffer m_SrcBuffer;
    DmaBuffer m_DstBuffer;

    DmaBuffer m_SrcAliasBuffer;
    DmaBuffer m_DstAliasBuffer;

    UINT32 m_TargetBuf0;
    UINT32 m_TargetBuf1;

    UINT64 m_OffsetSrc;
    UINT64 m_OffsetSrcAlias;
    UINT64 m_OffsetDest;
    UINT64 m_OffsetDestAlias;

    UINT64 m_Size;

    UINT32 m_PageSizeBuf0;
    UINT32 m_PageSizeBuf1;

    string m_PteKindBuf0;
    string m_PteKindBuf1;

    UINT64 m_SizeCheck;

    UINT32 *m_RefData;
    UINT32 *m_RefData2;
    UINT32 *m_RefData3;

    UINT32 m_Width, m_Height;
    UINT32 m_SubChannel;

    DmaCopyAlloc m_DmaCopyAlloc;

    void InitBuffer();
    void SetSurfaceAperture(DmaBuffer &m_buffer, UINT32 m_target);
    RC   ReadWriteTestUsingBar1CePte();
    void FlushIlwalidateL2Cache();

    void FillPteMemParamsFlagsAperture(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS &params, const DmaBuffer &m_buffer);
    // Get cpu physical address according to memory aperture type.
    UINT64 GetPhysAddress(const DmaBuffer &m_buffer);
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(SparseTexCopyAmap, SparseTexCopyAmapTest,"Testing interface for kindless copy to support sparse texture. Basically full-chip test for this feature in address mapping.");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &SparseTexCopyAmap_testentry
#endif

#endif  // _SPARSETEX_COPY_AMAP_H_
