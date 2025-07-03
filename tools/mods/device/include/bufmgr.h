/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef INCLUDED_BUFMGR_H
#define INCLUDED_BUFMGR_H

#include "core/include/memfrag.h"

#define MODE_ALIGN         0x20
#define MODE_RND_FRAG      0x10
#define MODE_RND_FRAGSIZE  0x08
#define MODE_OFFSET        0x06
#define MODE_RND_GAP       0x01

class Controller;

typedef struct stAllocParam
{
    UINT32 fixNum;
    UINT32 minNum;
    UINT32 maxNum;
    UINT32 stepSize;
} AllocParam;

class BufferMgr
{
public:
typedef struct stMemBlock
{
    void*    pVA;
    UINT32   size;
} MemBlock;

typedef struct stAllocInfo
{
    UINT32   offset;
    UINT32   size;
}AllocInfo;

public:
    BufferMgr(Controller *pCtrl = nullptr);
    ~BufferMgr();

    /*
     *  protected functions in BufferMgr,
     *  implementing different algorithms in each step of fragment allocation
     *  should only be called from the public interface functions
     */

    //! \brief Set the attribute of the buffer in the memory
    //! \param [in] HwMaxFragments : maximum number of fragments limited by the hardware
    //! \param [in] HwMinDataWidth : minimum size of data unit limited by the hardware
    //! \param [in] MemAttr : types of memory requested {UC,WC,WT,WP,WB}
    RC InitBufferAttr(UINT32 HwMaxFragments,
                      UINT32 HwMinDataWidth,
                      UINT32 HwMaxFragSize,
                      UINT32 AddrBits,
                      Memory::Attrib MemAttr=Memory::UC,
                      UINT32  BoundaryMask = 0);

    //! \brief Initialize the buffer blocks in the memory
    //! \param [in] ByteSize : total bytes requested, will be round-up to multiples of MEM_BLOCK_SIZE
    //! \param [in] IsAligned : need to get aligned memory or not
    //! \param [in] Alignment : alignment of memory address for the fragments
    RC AllocMemBuffer(UINT32 ByteSize,
                    bool IsAligned,
                    UINT32 Alignment);

    //! \brief fill up the memory blocks with UINT08 pattern
    //! \param [in] pPattern : The pattern to fill in the buffer
    RC FillPattern(vector<UINT08> *pPattern);

    //! \brief fill up the memory blocks with UINT16 pattern
    //! \param [in] pPattern : The pattern to fill in the buffer
    RC FillPattern(vector<UINT16> *pPattern);

    //! \brief fill up the memory blocks with UINT32 pattern
    //! \param [in] pPattern : The pattern to fill in the buffer
    RC FillPattern(vector<UINT32> *pPattern);

    //! \brief Generate fragments for the buffers in PRD tables
    //! \param [out] pFragList : pointer to the memmory fragment list
    //! \param [in] Modes : bit vector specifying the different modes in allocating the memory fragments:
    //!                     bit 0-1: mode in offset 0:fixed, 1:
    //!                     random, 2: incremental
    //!                     bit 2: mode in size of fragment 0:
    //!                     average, 1: random
    //!                     bit 3: mode in the number of fragment
    //!                     0:fixed, 1: random
    //!                     bit 4: mode in aligned/non-aligned
    //!                     allocation 0: non-aligned, 1: aligned
    //! \param [in] Alignment : alignment of memory address for the fragments
    //! \param [in] TotalBytes : total number of bytes requested in the pFragList
    //! \param [in] DataWidth : the requested data unit size: 1: byte,2: word, 4: dword, 8: qword
    //! \param [in] pNumFragments : pointer to the structure AllocParam that specifies
    //!                             parameters to callwlate number of fragments
    //! \param [in] pFragSizes : pointer to the structure AllocParam that specifies
    //!                          parameters to callwlate the fragment sizes
    //! \param [in] pOffsets : pointer to the structure AllocParam that specifies parameters
    //!                        to callwlate the offsets
    //! \param [in] IsRealloc : whether to dynamically re-alloc buffer blocks according to the requested fraglist
    //! \param [in] IsShuffle : whether to shuffle the fragment list
    RC GenerateFragmentList(MemoryFragment::FRAGLIST *pFragList,
                            UINT08 Modes,
                            UINT32 Alignment,
                            UINT32 TotalBytes,
                            UINT32 DataWidth,
                            AllocParam* pNumFragments,
                            AllocParam* pFragSizes,
                            AllocParam* pOffsets,
                            bool IsRealloc=true,
                            bool IsShuffle=true);

    RC GenerateFixedFragList(MemoryFragment::FRAGLIST *pFragList,
                             vector<AllocInfo> *pAllocList,
                             UINT32 TotalBytes,
                             UINT32 DataWidth,
                             UINT32 Alignment,
                             bool IsRealloc = true,
                             bool IsShuffle = true);

    //! \brief Callwlate the number of fragments
    //! \param [out] pAllocList :  pointer to the allocation list
    //! \param [in] TotalBytes : the total size of requested data
    //! \param [in] DataWidth : data width
    //! \param [in] MinFragments : the minium  value
    //! \param [in] MaxFragments : the maxium value
    RC CallwlateNumFragments(vector<AllocInfo>* pAllocList,
                             UINT32 TotalBytes,
                             UINT32 DataWidth,
                             UINT32 MinFragments,
                             UINT32 MaxFragments);

    //! \brief Callwlates the fragment sizes
    //! \param [out] pAllocList : pointer to the allocation list
    //! \param [in] IsRndMode : random mode or not
    //! \param [in] TotalBytes : the total size of requested data
    //! \param [in] DataWidth : the requested data unit size: 1: byte, 2: word, 4: dword, 8: qword
    RC CallwlateFragSizes(vector<AllocInfo> *pAllocList,
                          UINT32 TotalBytes,
                          UINT32 DataWidth,
                          bool   IsRndMode,
                          UINT32 MinSize,
                          UINT32 MaxSize);

    //! \brief Callwlate the offsets in aligned mode
    //! \param [in,out] pAllocList : pointer to the allocation list
    //! \param [in] DataWidth : the requested data unit size: 1:byte, 2: word, 4: dword, 8: qword
    RC CallwlateOffsets(vector<AllocInfo>* pAllocList,
                        UINT32 DataWidth,
                        UINT32 MinOffset,
                        UINT32 MaxOffset,
                        UINT32 StepSize  = 0);

    //! \brief Callwlate the gaps in non-aligned mode
    //! \param [in,out] pAllocList : pointer to the allocation list
    //! \param [in] IsRndBap : allocation modes
    //! \param [in] ExtraBytes : Extra bytes allocated in the gaps
    //! \param [in] DataWidth : the requested data unit size: 1: byte, 2: word, 4: dword, 8: qword
    RC  CallwlateGaps(vector<AllocInfo> *pAllocList,
                      bool   IsRndMode,
                      UINT32 TotalBytes,
                      UINT32 DataWidth,
                      bool IsRealloc);

    void PrintAllocList(vector<AllocInfo>* pAllocList);

    UINT32 GetMaxFragSize();
    void   SetMaxFragSize(UINT32 MaxFragSize);
    UINT32 GetBoundaryMask();
    UINT32 GetChunkSize();

private:
    //! \var the memory blocks allocated
    //! Assumes the memory in each block is continuous
    //! The memory in different blocks are not necessarily continuous
    vector<MemBlock> m_vMemBlocks;

    //! \var number of address bits used to allocate memory
    UINT32 m_AddrBits;

    //! \var the type of memory space allocated
    Memory::Attrib m_MemAttr;

    //! \var the maximum number of fragments limited by the device
    UINT32 m_HwMaxFragments;

    //! \var the size of data element used by the device.
    //       valid sizes: 1: byte, 2: word, 4: dword, 8: qword
    UINT32 m_HwMinDataWidth;

    //! \var mainly for ide use. Ide memory region can't straddle 64K
    UINT32 m_BoundaryMask;

    //! \var the maximum fragment size limited by the HW
    UINT32 m_MaxFragSize;

    //! \var pointer to the controller to custom memory allocation setting
    Controller *m_pCtrl;

    //! \brief Get current total size of allocated memory
    UINT32 TotalMemSize();

    //! \brief clear the fragment list and release the allocated memory
    RC ClearBuffer();

    //! \brief free the extra memory blocks
    RC FreeExtraMem(UINT32 UsedBlocks);

    //! \brief grow memory blocks
    RC GrowMemBlock(size_t Size, UINT32 Align);

    //! \brief assigning aligned memory blocks to fragment list
    //! \param [in,out] pFragList : pointer to the fragment list
    //! \param [in] pAllocList : pointer to the allocation list
    //! \param [in] Alignment : alignment of memory address for the fragments
    //! \param [in] DataWidth : the requested data unit size: 1: byte, 2: word, 4: dword, 8: qword
    //! \param [in] IsRealloc : whether to dynamically re-allocbuffer blocks according to the requested fraglist
    RC AllocMemory(MemoryFragment::FRAGLIST *pFragList,
                   vector<AllocInfo>* pAllocList,
                   UINT32 Alignment,
                   UINT32 DataWidth,
                   bool IsRealloc);
//--------------------------------------------------------------------------------
// Helper functions to allocate the memory to fragments
//--------------------------------------------------------------------------------

    RC GenerateRandomFragSizes(vector<AllocInfo>* pAllocList,
                               UINT32 TotalBytes,
                               UINT32 DataWidth,
                               UINT32 MinFragSize,
                               UINT32 MaxFragSize);

    //! \brief Allocate average number of byte size to each fragment
    //! \param [in,out] pAllocList : pointer to the allocation list
    //! \param [in] TotalBytes : the total bytes requested in all fragments
    //! \param [in] DataWidth : the requested data unit size: 1: byte, 2: word, 4: dword, 8: qword
    RC GenerateAverageFragSizes(vector<AllocInfo>* pAllocList,
                                UINT32 TotalBytes,
                                UINT32 DataWidth);

    //! \brief Generate random sizes of gaps between fragments in non-aligned allocation
    //! \param [in,out] pAllocList : pointer to the allocation list
    //! \param [in] TotalExtraBytes : total number of extra bytes in non-aligned memory fragments
    //! \param [in] DataWidth : the requested data unit size: 1: byte, 2: word, 4: dword, 8: qword
    //! \param [in] MinGapSize : minimum gap size in bytes
    //! \param [in] MaxGapSize : maximum gap size in bytes
    RC GenerateRandomGaps(vector<AllocInfo>* pAllocList,
                          UINT32 TotalExtraBytes,
                          UINT32 DataWidth,
                          UINT32 MinGapSize,
                          UINT32 MaxGapSize);

    //! \brief Generate average size of gaps between fragments in non-aligned allocation
    //! \param [in] pAllocList : pointer to the allocation list
    //! \param [in] TotalExtraBytes : total number of extra bytes in non-aligned memory fragments
    //! \param [in] DataWidth : the requested data unit size: 1: byte, 2: word, 4: dword, 8: qword
    RC GenerateAverageGaps(vector<AllocInfo>* pAllocList,
                           UINT32 TotalExtraBytes,
                           UINT32 DataWidth);

    RC CheckDataWidth(UINT32  DataWidth);
    RC CheckSize(UINT32 TotalBytes, UINT32 DataWidth);
    RC CheckVector(vector<AllocInfo>* pAllocList, bool IsCheckSize = true);
    RC CheckMinMax(UINT32 Min, UINT32 Max, bool IsCheckZero = true);
};
#endif
