/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/tests/lwdamemtest.h"
#include "gpu/tests/lwca/mats/bitlane.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/utility.h"
#include "gpu/tests/nwfmats.h"
#include <math.h>
#include <memory>

class BitLaneTest : public LwdaMemTest
{
public:
    BitLaneTest();
    virtual ~BitLaneTest() { }
    virtual bool IsSupported();
    virtual void PrintJsProperties(Tee::Priority pri);
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    RC Clear();
    RC Fill(UINT32 pattern);
    RC SetPatternForAll(const UINT08* pattern, UINT32 size);
    RC SetPattern(char partitionLetter, UINT32 bit,
                  const UINT08* pattern, UINT32 size);
    RC Write(UINT64 beginOffs, UINT64 endOffs);
    RC Read(UINT64 beginOffs, UINT64 endOffs);
    RC Verify(UINT64 beginOffs, UINT64 endOffs);
    RC Synchronize();
    RC CheckErrors();

    // JS property accessors
    SETGET_PROP(MaxErrorsPerBlock,  UINT32);
    SETGET_PROP(NumBlocks,          UINT32);
    SETGET_PROP(NumThreads,         UINT32);
    SETGET_PROP(VerifyChannelSwizzle, bool);

private:
    UINT32 GetResultsSize() const;
    RC ExtractErrors(const char* results, bool* pCorrupted, UINT32 resultsSize);
    RC UpdatePattern();
    RC UpdateRange(UINT64 begin, UINT64 end);
    void SetupFunctionParam(FunctionParam* pParam, UINT64 beginOffs);
    RC AlignPhysAddresses(UINT64* beginOffs, UINT64* endOffs) const;
    UINT32 CalcBin(UINT64 addr);
    static UINT32 CalcGoodPatternSize(UINT32 numRevBursts);

    // JS properties
    UINT32 m_MaxErrorsPerBlock;
    UINT32 m_NumBlocks;
    UINT32 m_NumThreads;
    bool m_VerifyChannelSwizzle;

    Lwca::Module m_Module;
    Lwca::Function m_WriteFunction;
    Lwca::Function m_ReadFunction;
    Lwca::DeviceMemory m_ResultsMem;
    Lwca::DeviceMemory m_PatternsMem;
    Lwca::DeviceMemory m_ChunksMem;
    Lwca::DeviceMemory m_BlocksMem;
    Lwca::Event m_UpdatePatEv;
    Lwca::Event m_UpdateBlocksEv;

    typedef vector<UINT32, Lwca::HostMemoryAllocator<UINT32> > PatVec;
    shared_ptr<PatVec> m_PatternOnHost;

    typedef vector<UINT32, Lwca::HostMemoryAllocator<UINT32> > BlockVec;
    shared_ptr<BlockVec> m_BlocksOnHost;

    UINT32 m_ActualMaxErrorsPerBlock;
    UINT32 m_Iter;
    bool m_PatternChanged;
    const UINT64 m_VanillaBlockPhysAddr;
    UINT32 m_BlockSize;
    UINT32 m_ChunksPerBlock;
    UINT32 m_NumPatterns;
    UINT32 m_NumTestedBlocks;
    UINT64 m_LastBlockBeginOffs;
    UINT64 m_LastBlockEndOffs;
    UINT32 m_Bin0;
    UINT32 m_OnlyPartChan;
    vector<UINT08> m_PartitonLetters;
};

JS_CLASS_INHERIT(BitLaneTest, LwdaMemTest,
                 "LWCA MATS (Modified Algorithmic Test Sequence)");

CLASS_PROP_READWRITE(BitLaneTest, MaxErrorsPerBlock, UINT32,
        "Maximum number of errors reported per each LWCA block (0=default)");
CLASS_PROP_READWRITE(BitLaneTest, NumBlocks, UINT32,
                     "Number of kernel blocks to run");
CLASS_PROP_READWRITE(BitLaneTest, NumThreads, UINT32,
                     "Number of kernel threads to run");
CLASS_PROP_READWRITE(BitLaneTest, VerifyChannelSwizzle, bool,
                     "Enables verification of channel swizzling");

BitLaneTest::BitLaneTest() :
    m_MaxErrorsPerBlock(0),
    m_NumBlocks(0),
    m_NumThreads(0),
    m_VerifyChannelSwizzle(false),
    m_ActualMaxErrorsPerBlock(0),
    m_Iter(0),
    m_PatternChanged(false),
    m_VanillaBlockPhysAddr(0),
    m_BlockSize(0),
    m_ChunksPerBlock(0),
    m_NumPatterns(0),
    m_NumTestedBlocks(0),
    m_LastBlockBeginOffs(0),
    m_LastBlockEndOffs(0),
    m_Bin0(~0U),
    m_OnlyPartChan(0)
{
    SetName("BitLaneTest");
}

bool BitLaneTest::IsSupported()
{
    if (!LwdaMemTest::IsSupported())
        return false;

    const Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();
    if (cap >= Lwca::Capability::SM_60)
    {
        return false;
    }

    return true;
}

void BitLaneTest::PrintJsProperties(Tee::Priority pri)
{
    LwdaMemTest::PrintJsProperties(pri);

    Printf(pri, "BitLaneTest Js Properties:\n");
    Printf(pri, "\tMaxErrorsPerBlock:\t\t%u\n", m_MaxErrorsPerBlock);
    Printf(pri, "\tNumBlocks:\t\t\t%08x\n", m_NumBlocks);
    Printf(pri, "\tNumThreads:\t\t\t%08x\n", m_NumThreads);
    Printf(pri, "\tVerifyChannelSwizzle:\t\t%s\n",
           m_VerifyChannelSwizzle ? "true" : "false");
}

RC BitLaneTest::Setup()
{
    RC rc;
    CHECK_RC(LwdaMemTest::Setup());

    // Initialize iteration counter
    m_Iter = 0;

    // Set default maximum number of errors per block
    m_ActualMaxErrorsPerBlock = m_MaxErrorsPerBlock;
    if (m_ActualMaxErrorsPerBlock == 0)
    {
        m_ActualMaxErrorsPerBlock = 1023;
    }

    // Initialize module
    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("bitlan", &m_Module));

    // Get FB properties
    FrameBuffer* const pFB = GetBoundGpuSubdevice()->GetFB();
    const UINT32 numPartitions = pFB->GetFbioCount();
    const UINT32 numChannels = pFB->GetSubpartitions();

    // Set block size that was experimentally determined to be good enough
    m_BlockSize = 65536;

    // Allocate memory for results
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(GetResultsSize(), &m_ResultsMem));
    CHECK_RC(m_ResultsMem.Clear());

    // Allocate memory for patterns
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(
                 maxBurstLengthTimesPatterns * sizeof(UINT32) *
                 numPartitions * numChannels,
                 &m_PatternsMem));
    m_UpdatePatEv = GetLwdaInstance().CreateEvent();
    CHECK_RC(m_UpdatePatEv.InitCheck());

    // Allocate memory for chunk layout
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(maxChunks, &m_ChunksMem));
    CHECK_RC(m_ChunksMem.InitCheck());

    // Allocate memory for blocks
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(maxBlocks*sizeof(UINT16), &m_BlocksMem));
    m_UpdateBlocksEv = GetLwdaInstance().CreateEvent();
    CHECK_RC(m_UpdateBlocksEv.InitCheck());

    // Allocate memory to test
    CHECK_RC(AllocateEntireFramebuffer());

    // Decode partitions and channels for block 0
    const GpuUtility::MemoryChunkDesc& chunk0 = GetChunkDesc(0);
    vector<UINT08> chunks;
    UINT32 chunkSize = 0;
    CHECK_RC(GpuUtility::DecodeRangePartChan(
            GetBoundGpuSubdevice(),
            chunk0,
            m_VanillaBlockPhysAddr,
            m_VanillaBlockPhysAddr+m_BlockSize,
            &chunks,
            &chunkSize));
    if (chunks.size() > maxChunks)
    {
        Printf(Tee::PriError,
               "Unable to handle this GPU's partition swizzle -"
               " too many chunks\n");
        Printf(Tee::PriError,
               "%u chunks, max %u chunks supported, chunk size %uB,"
               " block size %uB\n",
                static_cast<UINT32>(chunks.size()),
                static_cast<UINT32>(maxChunks), chunkSize, m_BlockSize);
        return RC::SOFTWARE_ERROR;
    }

    // Upload the chunks layout to the GPU
    vector<UINT08, Lwca::HostMemoryAllocator<UINT08> >
        chunksHost(Lwca::HostMemoryAllocator<UINT08>(GetLwdaInstancePtr(),
                                                     GetBoundLwdaDevice()));
    chunksHost.assign(chunks.begin(), chunks.end());
    CHECK_RC(m_ChunksMem.Set(&chunksHost[0], chunksHost.size()));
    m_ChunksPerBlock = (UINT32)chunks.size();

    // Callwalte bin identifier of the original chunk
    m_Bin0 = CalcBin(m_VanillaBlockPhysAddr);

    // Prepare kernels
    if (m_NumBlocks == 0)
    {
        m_NumBlocks = numPartitions * numChannels;
    }
    else if ((m_NumBlocks != numPartitions*numChannels) && (m_NumBlocks != 1))
    {
        Printf(Tee::PriError, "Invalid number of blocks: only 1 or %u blocks "
                "are supported on this chip\n", numPartitions*numChannels);
        return RC::BAD_PARAMETER;
    }
    if (m_NumThreads == 0)
    {
        m_NumThreads = static_cast<UINT32>(
                m_BlockSize/(m_ChunksPerBlock * sizeof(UINT32)));
    }
    if (m_NumThreads > 1024)
    {
        m_NumThreads = 1024;
    }
    if (1 != Utility::CountBits(m_NumThreads))
    {
        Printf(Tee::PriError,
               "Invalid number of threads: %u - must be power of 2\n",
               m_NumThreads);
        return RC::BAD_PARAMETER;
    }
    m_ReadFunction = m_Module.GetFunction("Read", m_NumBlocks, m_NumThreads);
    CHECK_RC(m_ReadFunction.InitCheck());
    m_WriteFunction = m_Module.GetFunction("Write", m_NumBlocks, m_NumThreads);
    CHECK_RC(m_WriteFunction.InitCheck());

    // Compute partition letter map
    m_PartitonLetters.reserve((UINT08)'H');
    for (UINT08 ipart=0; ipart < numPartitions; ipart++)
    {
        const UINT08 letter = pFB->VirtualFbioToLetter(ipart);
        if (m_PartitonLetters.size() <= letter)
        {
            m_PartitonLetters.resize(letter+1, 0xFFU);
        }
        m_PartitonLetters[letter] = ipart;
    }

    // Make sure that the uploading of chunks layout was completed
    CHECK_RC(GetLwdaInstance().Synchronize());

    Tee::Priority pri = GetVerbosePrintPri();
    Printf(pri, "Setup complete:\n");
    Printf(pri, "    blocks        %u\n", m_NumBlocks);
    Printf(pri, "    threads       %u\n", m_NumThreads);
    Printf(pri, "    partitions    %u\n", numPartitions);
    Printf(pri, "    subpartitions %u\n", numChannels);
    Printf(pri, "    burst length  %u\n", (pFB->GetBurstSize() /
                                           pFB->GetAmapColumnSize()));
    Printf(pri, "    block size    %u\n", m_BlockSize);
    Printf(pri, "    chunks        %u\n", (UINT32)chunks.size());
    Printf(pri, "    chunk size    %u\n", chunkSize);

    return OK;
}

RC BitLaneTest::Cleanup()
{
    StickyRC rc;
    rc = FreeEntireFramebuffer();
    m_UpdateBlocksEv.Free();
    m_BlocksMem.Free();
    m_ChunksMem.Free();
    m_UpdatePatEv.Free();
    m_PatternsMem.Free();
    if (m_PatternOnHost)
        m_PatternOnHost->clear();
    m_ResultsMem.Free();
    m_Module.Unload();
    rc = LwdaMemTest::Cleanup();
    return rc;
}

RC BitLaneTest::Run()
{
    JavaScriptPtr pJs;
    return pJs->CallMethod(GetJSObject(), "RunLwstom");
}

UINT32 BitLaneTest::CalcBin(UINT64 addr)
{
    const GpuUtility::MemoryChunkDesc& chunk0 = GetChunkDesc(0);
    FrameBuffer* const pFB = GetBoundGpuSubdevice()->GetFB();
    const UINT32 numPartitions = pFB->GetFbioCount();
    const UINT32 numChannels = pFB->GetSubpartitions();
    const UINT32 chunkSize = m_BlockSize / m_ChunksPerBlock;
    FbDecode decode;

    // Collect partition/channel at:
    //   addr, addr+chunkSize, addr+chunkSize*2, addr+chunkSize*4, ...
    // Partitions and channels at these addresses are folded into an integer.
    // Note that partition/channel stays the same within each chunk.
    UINT32 bin = 0;
    const int maxSamplePoints = static_cast<int>(floor(
            log(double(bin-1)) / log(double(numPartitions*numChannels))));
    for (int i=0; i < maxSamplePoints; i++)
    {
        const UINT32 offset = (i == 0) ? 0 : (1 << (i-1))*chunkSize;
        if (offset >= m_BlockSize)
        {
            break;
        }
        pFB->DecodeAddress(&decode, addr+offset,
                           chunk0.pteKind, chunk0.pageSizeKB);
        bin = (bin * numPartitions + decode.virtualFbio)
                   * numChannels + decode.subpartition;
    }
    return bin;
}

RC BitLaneTest::Clear()
{
    for (UINT32 i=0; i < NumChunks(); i++)
    {
        RC rc;
        CHECK_RC(GetLwdaChunk(i).Clear());
    }
    return OK;
}

RC BitLaneTest::Fill(UINT32 pattern)
{
    for (UINT32 i = 0; i < NumChunks(); i++)
    {
        RC rc;
        CHECK_RC(GetLwdaChunk(i).Fill(pattern));
    }
    return OK;
}

RC BitLaneTest::SetPatternForAll(const UINT08* pattern, UINT32 size)
{
    RC rc;
    vector<UINT08> tmpPattern;
    CHECK_RC(NewWfMatsTest::DecodeUserBitLanePatternForAll(
            GetBoundGpuSubdevice(),
            pattern,
            size,
            &tmpPattern));
    size = static_cast<UINT32>(tmpPattern.size());

    // Check pattern length against limit
    if (size > maxBurstLengthTimesPatterns)
    {
        Printf(Tee::PriError,
                "Pattern too long: %u, "
                "maximum supported pattern size on this device is %u\n",
                size,
                static_cast<UINT32>(maxBurstLengthTimesPatterns));
        return RC::BAD_PARAMETER;
    }

    // If a read or write is launched after this function with one block
    // it should apply to all patterns and channels
    m_OnlyPartChan = ~0U;

    // Get FB configuration
    FrameBuffer* const pFB = GetBoundGpuSubdevice()->GetFB();
    const UINT32 numPartitions = pFB->GetFbioCount();
    const UINT32 numChannels = pFB->GetSubpartitions();

    // Set host array ready for copying to LWCA device
    const UINT32 patSize = size * numPartitions * numChannels;
    m_PatternOnHost.reset(new PatVec(patSize,
                                     UINT32(),
                                     Lwca::HostMemoryAllocator<UINT32>(GetLwdaInstancePtr(),
                                                                       GetBoundLwdaDevice())));
    for (UINT32 i=0; i < patSize; i++)
    {
        (*m_PatternOnHost)[i] = (tmpPattern[i%size] ? ~0U : 0U);
    }
    m_PatternChanged = true;

    // Print the pattern
    Printf(Tee::PriDebug, "Set pattern for all partitions, channels:\n");
    for (UINT32 i=0; i < size; i++)
    {
        Printf(Tee::PriDebug, "%u", (UINT32)tmpPattern[i]);
    }
    Printf(Tee::PriDebug, "\n");
    return OK;
}

RC BitLaneTest::SetPattern
(
    char partitionLetter,
    UINT32 bit,
    const UINT08* pattern,
    UINT32 size
)
{
    // Get FB configuration
    FrameBuffer* const pFB = GetBoundGpuSubdevice()->GetFB();
    const UINT32 numPartitions = pFB->GetFbioCount();
    const UINT32 numChannels = pFB->GetSubpartitions();
    const UINT32 burstLength = pFB->GetBurstSize() / pFB->GetAmapColumnSize();

    // Create full pattern set if it's empty
    if (!m_PatternOnHost || m_PatternOnHost->empty())
    {
        // Fill with zeroes
        const UINT32 fullSize = size + burstLength - (size % burstLength);
        m_PatternOnHost.reset(new PatVec(Lwca::HostMemoryAllocator<UINT32>(GetLwdaInstancePtr(),
                                                                           GetBoundLwdaDevice())));
        m_PatternOnHost->resize(fullSize * numPartitions * numChannels, 0);
    }

    // Colwert user pattern to FB-friendly pattern
    RC rc;
    vector<UINT08> tmpPattern;
    CHECK_RC(NewWfMatsTest::DecodeUserBitLanePattern(
            GetBoundGpuSubdevice(),
            pattern,
            size,
            static_cast<UINT32>(m_PatternOnHost->size() / (numPartitions * numChannels)),
            &tmpPattern));
    size = static_cast<UINT32>(tmpPattern.size());

    // Colwert letter to partition index
    const UINT08 index = partitionLetter;
    if ((index >= m_PartitonLetters.size()) ||
            (m_PartitonLetters[index] == 0xFFU))
    {
        Printf(Tee::PriError,
                "Invalid partition letter %c\n", partitionLetter);
        return RC::BAD_PARAMETER;
    }
    if (bit >= numChannels*32)
    {
        Printf(Tee::PriError,
                "Invalid bit %u (there are only %u bits)\n",
                bit, numChannels*32);
        return RC::BAD_PARAMETER;
    }
    const UINT32 partition = m_PartitonLetters[index];
    const UINT32 channel = bit / 32;

    // Save partition/channel in case the user has chosen to run one block
    m_OnlyPartChan = partition * numChannels + channel;

    // Copy pattern to host array ready for copying to LWCA device
    const UINT32 base = (partition * numChannels + channel) * size;
    const UINT32 bitMask = 1 << (bit % 32);
    for (UINT32 i=0; i < size; i++)
    {
        (*m_PatternOnHost)[base+i] = ((*m_PatternOnHost)[base+i] & ~bitMask)
            | (tmpPattern[i] ? bitMask : 0);
    }
    m_PatternChanged = true;

    // Print the pattern
    Printf(Tee::PriDebug, "Set pattern for partition %u, bit %u:\n",
            partition, bit);
    for (UINT32 i=0; i < size; i++)
    {
        Printf(Tee::PriDebug, "%u", (UINT32)tmpPattern[i]);
    }
    Printf(Tee::PriDebug, "\n");
    return OK;
}

RC BitLaneTest::UpdatePattern()
{
    if (m_PatternChanged)
    {
        RC rc;
        CHECK_RC(m_UpdatePatEv.Synchronize());
        const UINT32 sizeInBytes = static_cast<UINT32>(
                m_PatternOnHost->size() * sizeof((*m_PatternOnHost)[0]));
        if (m_PatternsMem.GetSize() < sizeInBytes)
        {
            CHECK_RC(GetLwdaInstance().AllocDeviceMem(sizeInBytes, &m_PatternsMem));
        }
        CHECK_RC(m_PatternsMem.Set(&((*m_PatternOnHost)[0]), sizeInBytes));
        CHECK_RC(m_UpdatePatEv.Record());

        FrameBuffer* const pFB = GetBoundGpuSubdevice()->GetFB();
        const UINT32 numPartitions = pFB->GetFbioCount();
        const UINT32 numChannels = pFB->GetSubpartitions();
        m_NumPatterns = (static_cast<UINT32>(m_PatternOnHost->size()) /
                         (numPartitions * numChannels));

        m_PatternChanged = false;

        Printf(GetVerbosePrintPri(), "Sent patterns to the GPU\n");
    }
    return OK;
}

RC BitLaneTest::UpdateRange(UINT64 begin, UINT64 end)
{
    if ((begin == m_LastBlockBeginOffs) && (end == m_LastBlockEndOffs))
    {
        return OK;
    }

    const GpuUtility::MemoryChunkDesc& chunk0 = GetChunkDesc(0);

    RC rc;
    vector<UINT08> origChunks;
    if (m_VerifyChannelSwizzle)
    {
        UINT32 chunkSize = 0;
        CHECK_RC(GpuUtility::DecodeRangePartChan(
                GetBoundGpuSubdevice(),
                chunk0,
                m_VanillaBlockPhysAddr,
                m_VanillaBlockPhysAddr+m_BlockSize,
                &origChunks,
                &chunkSize));
    }

    CHECK_RC(m_UpdateBlocksEv.Synchronize());
    m_BlocksOnHost.reset(new BlockVec(Lwca::HostMemoryAllocator<UINT16>(GetLwdaInstancePtr(),
                                                                        GetBoundLwdaDevice())));

    UINT64 realEnd = end;
    for (UINT64 addr=begin; addr < end; addr += m_BlockSize)
    {
        const UINT32 bin = CalcBin(addr);
        if (bin == m_Bin0)
        {
            // Verify decode
            if (m_VerifyChannelSwizzle)
            {
                vector<UINT08> chunks;
                UINT32 chunkSize = 0;
                CHECK_RC(GpuUtility::DecodeRangePartChan(
                        GetBoundGpuSubdevice(),
                        chunk0,
                        addr,
                        addr+m_BlockSize,
                        &chunks,
                        &chunkSize));
                if (chunks != origChunks)
                {
                    Printf(Tee::PriError,
                           "Partition swizzle does not match assumptions!\n");
                    return RC::SOFTWARE_ERROR;
                }
            }

            // Save channel
            const UINT32 index =
                static_cast<UINT32>((addr - begin) / m_BlockSize);
            const UINT32 maxBytes = 0xFFFFU;
            if (index > maxBytes)
            {
                Printf(Tee::PriError,
                       "Specified address range too big!"
                       " Maximum %llu bytes supported.\n",
                       static_cast<UINT64>(maxBytes)*m_BlockSize);
                return RC::BAD_PARAMETER;
            }
            if (m_BlocksOnHost->size() == maxBlocks)
            {
                Printf(Tee::PriNormal,
                       "Warning: Too many blocks generated (%u) -"
                       " only %u blocks are supported.\n"
                       "Tested range shrunk from 0x%llx..0x%llx"
                       " to 0x%llx..0x%llx\n.",
                       (UINT32)m_BlocksOnHost->size(), maxBlocks,
                       begin, end, begin, addr);
                realEnd = addr;
                break;
            }
            m_BlocksOnHost->push_back(index);
        }
    }
    CHECK_RC(m_BlocksMem.Set(&((*m_BlocksOnHost)[0]),
                             m_BlocksOnHost->size()*sizeof(((*m_BlocksOnHost)[0]))));
    CHECK_RC(m_UpdateBlocksEv.Record());
    m_NumTestedBlocks = static_cast<UINT32>(m_BlocksOnHost->size());
    m_LastBlockBeginOffs = begin;
    m_LastBlockEndOffs = end;
    Printf(GetVerbosePrintPri(),
           "New tested physical address range: 0x%llx..0x%llx"
           " (%.1f MB), %u blocks\n",
           begin, realEnd, (realEnd - begin) / (1024.0 * 1024.0),
           m_NumTestedBlocks);
    return OK;
}

void BitLaneTest::SetupFunctionParam(FunctionParam* pParam, UINT64 beginOffs)
{
    FrameBuffer *pFB = GetBoundGpuSubdevice()->GetFB();
    pParam->mem            = beginOffs;
    pParam->blocks         = m_BlocksMem.GetDevicePtr();
    pParam->chunks         = m_ChunksMem.GetDevicePtr();
    pParam->patterns       = m_PatternsMem.GetDevicePtr();
    pParam->results        = m_ResultsMem.GetDevicePtr();
    pParam->numBlocks      = m_NumTestedBlocks;
    pParam->blockSize      = m_BlockSize;
    pParam->chunksPerBlock = m_ChunksPerBlock;
    pParam->dwordsPerChunk = static_cast<UINT32>(
                        m_BlockSize / (m_ChunksPerBlock * sizeof(UINT32)));
    pParam->burstLength    = pFB->GetBurstSize() / pFB->GetAmapColumnSize();
    pParam->numPatterns    = m_NumPatterns;
    pParam->resultsSize    = GetResultsSize();
    pParam->iteration      = m_Iter++;
    pParam->partChan       = m_OnlyPartChan;
    pParam->verif          = 0;
}

RC BitLaneTest::AlignPhysAddresses(UINT64* beginOffs, UINT64* endOffs) const
{
    const UINT64 beginRem = *beginOffs % m_BlockSize;
    if (beginRem > 0)
    {
        *beginOffs += m_BlockSize - beginRem;
        Printf(Tee::PriNormal,
               "Warning: Aligning StartPhys on block boundary -"
               " new StartPhys is 0x%llx\n",
                *beginOffs);
    }
    const UINT64 endRem = *endOffs % m_BlockSize;
    if (endRem > 0)
    {
        *endOffs -= endRem;
        Printf(Tee::PriNormal,
               "Warning: Aligning EndPhys on block boundary -"
               " new EndPhys is 0x%llx\n",
                *endOffs);
    }
    if (*beginOffs >= *endOffs)
    {
        Printf(Tee::PriError, "Invalid range 0x%llx..0x%llx\n",
                *beginOffs, *endOffs);
        return RC::BAD_PARAMETER;
    }
    return OK;
}

RC BitLaneTest::Write(UINT64 beginOffs, UINT64 endOffs)
{
    RC rc;
    CHECK_RC(AlignPhysAddresses(&beginOffs, &endOffs));
    CHECK_RC(UpdatePattern());
    CHECK_RC(UpdateRange(beginOffs, endOffs));
    CHECK_RC(PhysicalToVirtual(&beginOffs, &endOffs));
    FunctionParam param;
    SetupFunctionParam(&param, beginOffs);
    m_WriteFunction.SetGridDim(m_NumBlocks);
    CHECK_RC(m_WriteFunction.Launch(param));
    Printf(GetVerbosePrintPri(), "Write patterns\n");
    return OK;
}

RC BitLaneTest::Read(UINT64 beginOffs, UINT64 endOffs)
{
    RC rc;
    CHECK_RC(AlignPhysAddresses(&beginOffs, &endOffs));
    CHECK_RC(UpdatePattern());
    CHECK_RC(UpdateRange(beginOffs, endOffs));
    CHECK_RC(PhysicalToVirtual(&beginOffs, &endOffs));
    FunctionParam param;
    SetupFunctionParam(&param, beginOffs);
    m_ReadFunction.SetGridDim(m_NumBlocks);
    CHECK_RC(m_ReadFunction.Launch(param));
    Printf(GetVerbosePrintPri(), "Read patterns\n");
    return OK;
}

UINT32 BitLaneTest::CalcGoodPatternSize(UINT32 numRevBursts)
{
    if (numRevBursts <= 2)
    {
        return 16;
    }
    return (15 / numRevBursts + 1) * numRevBursts;
}

RC BitLaneTest::Verify(UINT64 beginOffs, UINT64 endOffs)
{
    RC rc;
    CHECK_RC(AlignPhysAddresses(&beginOffs, &endOffs));

    FrameBuffer* const pFB = GetBoundGpuSubdevice()->GetFB();
    const UINT32 numPartitions = pFB->GetFbioCount();
    const UINT32 numChannels = pFB->GetSubpartitions();
    const UINT32 burstLength = pFB->GetBurstSize() / pFB->GetAmapColumnSize();
    const UINT32 numReversedBursts = pFB->GetNumReversedBursts();

    const UINT32 testPatternSize = CalcGoodPatternSize(numReversedBursts);
    {
        vector<UINT08> pattern(testPatternSize*burstLength);
        for (UINT32 ipat=0; ipat < testPatternSize*burstLength; ipat++)
        {
            pattern[ipat] = ipat & 1;
        }
        CHECK_RC(SetPatternForAll(&pattern[0], testPatternSize*burstLength));
    }

    vector<UINT32> baseValues(numPartitions*numChannels*burstLength, ~0U);
    UINT32 ival = 0;
    for (UINT32 ipart=0; ipart < numPartitions; ipart++)
    {
        const char partLetter = pFB->VirtualFbioToLetter(ipart);
        for (UINT32 ichan=0; ichan < numChannels; ichan++)
        {
            vector<UINT32> pattern32(testPatternSize*burstLength);
            const UINT32 chanAdd = ichan ? 32 : 0;
            for (UINT32 ibo=0; ibo < burstLength; ibo++)
            {
                const UINT32 masterIndex =
                    ((ipart * numChannels) + ichan) * burstLength + ibo;
                baseValues[masterIndex] = ival;
                for (UINT32 i=0; i < testPatternSize; i++)
                {
                    pattern32[i*burstLength+ibo] = ival++;
                }
            }

            // Set pattern for individual bits
            vector<UINT08> bitpattern(testPatternSize*burstLength);
            for (UINT32 ibit=0; ibit < 32; ibit++)
            {
                const UINT32 bitMask = 1 << ibit;
                for (UINT32 i=0; i < testPatternSize*burstLength; i++)
                {
                    bitpattern[i] = (pattern32[i] & bitMask) ? 1 : 0;
                }
                CHECK_RC(NewWfMatsTest::ApplyBurstReversal(&bitpattern, pFB));
                CHECK_RC(SetPattern(partLetter, chanAdd+ibit,
                            &bitpattern[0], testPatternSize*burstLength));
            }
        }
    }

    CHECK_RC(m_PatternsMem.Fill(0xDEADCAFE));
    CHECK_RC(UpdatePattern());

    CHECK_RC(UpdateRange(beginOffs, endOffs));
    CHECK_RC(PhysicalToVirtual(&beginOffs, &endOffs));

    const UINT32 chunkId = FindChunkByVirtualAddress(beginOffs);
    MASSERT(chunkId != ~0U);
    Lwca::ClientMemory& lwdaChunk = GetLwdaChunk(chunkId);
    lwdaChunk.Fill(0xDEADBEEF);

    FunctionParam param;
    SetupFunctionParam(&param, beginOffs);
    param.partChan = ~0U; // all partitions and channels are tested
    param.verif = 1;

    CHECK_RC(m_WriteFunction.Launch(param));
    CHECK_RC(m_ReadFunction.Launch(param));

    CHECK_RC(CheckErrors());
    if (GetNumReportedErrors() != 0)
    {
        Printf(Tee::PriError,
               "The test algorithm is broken on this chip -"
               " read and write don't match.\n");
        Printf(Tee::PriError, "%u errors reported\n", GetNumReportedErrors());
        return RC::SOFTWARE_ERROR;
    }

    // Piece meal copy of LWCA surface to CPU-accessible sysmem
    const UINT32 maxCopySize = 16*1024*1024;
    vector<UINT32, Lwca::HostMemoryAllocator<UINT32> >
        fbTmp(Lwca::HostMemoryAllocator<UINT32>(GetLwdaInstancePtr(),
                                                GetBoundLwdaDevice()));
    fbTmp.reserve(maxCopySize/sizeof(UINT32));
    const size_t size = size_t(endOffs - beginOffs);
    vector<UINT32> fb;
    fb.reserve(size/sizeof(UINT32));
    for (UINT64 copyOffs=0; copyOffs < size; copyOffs += maxCopySize)
    {
        const UINT64 copySize = min<UINT64>(size-copyOffs, maxCopySize);
        fbTmp.resize(size_t(copySize/sizeof(UINT32)));
        CHECK_RC(lwdaChunk.Get(&fbTmp[0],
                               static_cast<size_t>(beginOffs -
                                                   lwdaChunk.GetDevicePtr() +
                                                   copyOffs),
                               static_cast<size_t>(copySize)));
        CHECK_RC(GetLwdaInstance().Synchronize());
        fb.insert(fb.end(), fbTmp.begin(), fbTmp.end());
    }

    {
        const UINT32 base = ((*m_BlocksOnHost)[0]) * m_BlockSize / sizeof(UINT32);
        const UINT32 numDwords = 128;
#ifdef _DEBUG
        const INT32 pri = GetVerbosePrintPri();
#else
        constexpr auto pri = Tee::PriDebug;
#endif
        Printf(pri, "Printing first %u dwords of block 0\n", numDwords);
        for (UINT32 i=0; i < numDwords; i++)
        {
            if ((i % 8) == 0)
                Printf(pri, "%08x: ", (UINT32)((base+i)*sizeof(UINT32)));
            Printf(pri, "%08x ", fb[base+i]);
            if ((i == numDwords-1) || ((i % 8) == 7))
                Printf(pri, "\n");
        }
    }

    vector<UINT32> deltaValues(baseValues.size(), 0);
    const GpuUtility::MemoryChunkDesc& chunk = GetChunkDesc(chunkId);
    const UINT64 virt = lwdaChunk.GetDevicePtr();
    const UINT64 physBase = beginOffs - virt + VirtualToPhysical(virt);
    for (UINT32 offs=0; offs < fb.size(); offs++)
    {
        const UINT64 physAddr = physBase + offs*4;
        FbDecode decode;
        CHECK_RC(pFB->DecodeAddress(&decode, physAddr,
                                    chunk.pteKind, chunk.pageSizeKB));
        const UINT32 blockId = (decode.virtualFbio * numChannels +
                                decode.subpartition);
        const UINT32 binId = CalcBin(physAddr);

        if (((physAddr % m_BlockSize) == 0) && (binId != m_Bin0))
        {
            for (UINT32 j = 0;
                 (j < m_BlockSize/sizeof(UINT32)) && (offs < fb.size());
                 j++, offs++)
            {
                if (fb[offs] != 0xDEADBEEF)
                {
                    Printf(Tee::PriError,
                           "The kernel is writing to an"
                           " unexpected memory region at offset 0x%x\n",
                           (UINT32)(offs*sizeof(UINT32)));
                    const UINT32 lwrBlockId = fb[offs] >> 26;
                    const UINT32 lwrThreadId = (fb[offs] >> 16) & 0x3FF;
                    Printf(Tee::PriError, "Suspected block %u, thread %u\n",
                            lwrBlockId, lwrThreadId);
                    return RC::SOFTWARE_ERROR;
                }
            }
            offs--;
            continue;
        }

        if (fb[offs] == 0xDEADBEEF)
        {
            Printf(Tee::PriError,
                   "The kernel is not writing to"
                   " all expected memory regions (offset 0x%x)\n",
                    (UINT32)(offs*sizeof(UINT32)));
            return RC::SOFTWARE_ERROR;
        }

        // Assuming columns are 4 bytes (questionable),
        // "burstOrder" is the column offset within a burst
        const UINT32 burstOrder =
            (decode.beat * pFB->GetBeatSize() + decode.beatOffset) /
            pFB->GetAmapColumnSize();

        const UINT32 index =
            ((m_NumBlocks == 1) ? 0 : blockId) * burstLength + burstOrder;
        const UINT32 lwrValue = fb[offs] & 0xFFFF;
        const UINT32 lwrBlockId = fb[offs] >> 26;
        const UINT32 lwrThreadId = (fb[offs] >> 16) & 0x3FF;
        const UINT32 expectedBlockId = (m_NumBlocks == 1) ? 0x3F : blockId;
        if (lwrBlockId != expectedBlockId)
        {
            Printf(Tee::PriError,
                   "Block id mismatch -"
                   " actual %u, expected %u, value 0x%08x, offset 0x%x\n",
                    lwrBlockId, expectedBlockId, fb[offs],
                    static_cast<UINT32>(offs * sizeof(UINT32)));
            return RC::SOFTWARE_ERROR;
        }
        if ((lwrThreadId % burstLength) != burstOrder)
        {
            Printf(Tee::PriError, "Burst order mismatch\n");
            return RC::SOFTWARE_ERROR;
        }
        if (lwrValue != baseValues[index]+deltaValues[index])
        {
            Printf(Tee::PriError,
                   "Pattern write mismatch -"
                   " actual 0x%x, expected 0x%x, offset 0x%x\n",
                    lwrValue, baseValues[index]+deltaValues[index],
                    (UINT32)(offs*sizeof(UINT32)));
            return RC::SOFTWARE_ERROR;
        }
        deltaValues[index] = (deltaValues[index] + 1) % testPatternSize;
    }
    Printf(Tee::PriNormal, "Self-verification successful!\n");

    return OK;
}

RC BitLaneTest::Synchronize()
{
    return GetLwdaInstance().Synchronize();
}

RC BitLaneTest::CheckErrors()
{
    RC rc;
    vector<char, Lwca::HostMemoryAllocator<char> >
        results(GetResultsSize(),
                char(),
                Lwca::HostMemoryAllocator<char>(GetLwdaInstancePtr(),
                                                GetBoundLwdaDevice()));
    CHECK_RC(m_ResultsMem.Get(&results[0], results.size()));
    CHECK_RC(GetLwdaInstance().Synchronize());
    CHECK_RC(m_ResultsMem.Clear());
    bool corrupted = false;
    CHECK_RC(ExtractErrors(&results[0], &corrupted, (UINT32)results.size()));
    return corrupted ? RC::BAD_MEMORY : OK;
}

UINT32 BitLaneTest::GetResultsSize() const
{
    const UINT32 blockSize = sizeof(RangeErrors)
        + (m_ActualMaxErrorsPerBlock-1)*sizeof(BadValue);
    return blockSize;
}

RC BitLaneTest::ExtractErrors
(
    const char* results,
    bool* pCorrupted, // Set true if mem errors oclwred, else retain old value
    UINT32 resultsSize
)
{
    UINT32 numCorruptions = 0;
    const UINT32 maxCorruptions = 10;
    RC rc;

    const RangeErrors* const errors =
        reinterpret_cast<const RangeErrors*>(results);

    // Validate errors
    if ((errors->numReportedErrors > m_ActualMaxErrorsPerBlock)
         || (errors->numReportedErrors > errors->numEncounteredErrors))
    {
        numCorruptions++;
        if (numCorruptions <= maxCorruptions)
        {
            Printf(Tee::PriNormal,
                    "Error results are corrupted (%u reported errors?!)\n",
                    errors->numReportedErrors);
        }
        *pCorrupted = true;
        return OK;
    }
    if (errors->numReportedErrors >
        std::min<UINT64>(m_ActualMaxErrorsPerBlock,
                         errors->numEncounteredErrors))
    {
        numCorruptions++;
        if (numCorruptions <= maxCorruptions)
        {
            Printf(Tee::PriNormal,
                    "Error results are corrupted (%u reported errors with"
                    " %u max, but %llu total errors?!)\n",
                    errors->numReportedErrors, m_ActualMaxErrorsPerBlock,
                    errors->numEncounteredErrors);
        }
        *pCorrupted = true;
        return OK;
    }
    bool badValueCorrupted = false;
    for (UINT32 ierr=0; ierr < errors->numReportedErrors; ierr++)
    {
        const BadValue& badValue = errors->reportedValues[ierr];
        if (badValue.actual == badValue.expected)
        {
            numCorruptions++;
            if (numCorruptions <= maxCorruptions)
            {
                Printf(Tee::PriNormal,
                        "Error results are corrupted (actual value"
                        " 0x%08x the same as expected?!)\n",
                        badValue.actual);
            }
            badValueCorrupted = true;
            break;
        }
    }
    if (badValueCorrupted)
    {
        *pCorrupted = true;
        return OK;
    }

    FrameBuffer* const pFB = GetBoundGpuSubdevice()->GetFB();
    const UINT32 numChannels = pFB->GetSubpartitions();

    // Assume pseudoChannel 0
    const UINT32 pseudoChannel = 0;

    // Assume rank 0
    const UINT32 rank = 0;

    // Assume beat 0
    const UINT32 beatMask = 1;

    // Assume 32-bit bus
    const UINT32 beatOffset = 0;

    // Report unlogged errors
    if (errors->numEncounteredErrors - errors->numReportedErrors)
    {
        if (m_NumBlocks > 1)
        {
            UINT64 numBadBits = 0;
            for (UINT32 iblk=0; iblk < m_NumBlocks; iblk++)
            {
                const UINT32 partition = iblk / numChannels;
                const UINT32 lane = iblk % numChannels;
                if (errors->numReadErrors[iblk])
                {
                    CHECK_RC(GetMemError(0).LogFailingBits(
                                    partition,
                                    lane,
                                    pseudoChannel,
                                    rank,
                                    beatMask,
                                    beatOffset,
                                    0,
                                    errors->numReadErrors[iblk],
                                    0,
                                    MemError::IoType::READ));
                }
                if (errors->numWriteErrors[iblk])
                {
                    CHECK_RC(GetMemError(0).LogFailingBits(
                                    partition,
                                    lane,
                                    pseudoChannel,
                                    rank,
                                    beatMask,
                                    beatOffset,
                                    0,
                                    errors->numWriteErrors[iblk],
                                    0,
                                    MemError::IoType::WRITE));
                }
                for (int ibit=0; ibit < 32; ibit++)
                {
                    if (errors->numBitReadErrors[iblk][ibit])
                    {
                        CHECK_RC(GetMemError(0).LogFailingBits(
                                        partition,
                                        lane,
                                        pseudoChannel,
                                        rank,
                                        beatMask,
                                        beatOffset,
                                        ibit,
                                        0,
                                        errors->numBitReadErrors[iblk][ibit],
                                        MemError::IoType::READ));
                        numBadBits += errors->numBitReadErrors[iblk][ibit];
                    }
                    if (errors->numBitWriteErrors[iblk][ibit])
                    {
                        CHECK_RC(GetMemError(0).LogFailingBits(
                                        partition,
                                        lane,
                                        pseudoChannel,
                                        rank,
                                        beatMask,
                                        beatOffset,
                                        ibit,
                                        0,
                                        errors->numBitWriteErrors[iblk][ibit],
                                        MemError::IoType::WRITE));
                        numBadBits += errors->numBitWriteErrors[iblk][ibit];
                    }
                }
            }
            if (numBadBits == 0)
            {
                numCorruptions++;
                if (numCorruptions <= maxCorruptions)
                {
                    Printf(Tee::PriNormal,
                            "Error results are corrupted"
                            " (0 read or write errors?!)\n");
                }
                *pCorrupted = true;
                return OK;
            }
        }
    }

    // Report logged errors
    for (UINT32 ierr=0; ierr < errors->numReportedErrors; ierr++)
    {
        const BadValue& badValue = errors->reportedValues[ierr];

        const UINT64 fbOffset = VirtualToPhysical(badValue.address);;
        if (fbOffset == ~static_cast<UINT64>(0))
        {
            numCorruptions++;
            if (numCorruptions < maxCorruptions)
            {
                Printf(Tee::PriNormal, "Couldn't find a memory allocation "
                    "matching error detected at virtual address 0x%llx\n",
                    badValue.address);
            }
            continue;
        }

        LogError(fbOffset, badValue.actual, badValue.expected,
                badValue.reread1, badValue.reread2, badValue.iteration,
                GetChunkDesc(FindChunkByVirtualAddress(badValue.address)));
    }

    if (numCorruptions > 0)
    {
        *pCorrupted = true;
    }
    return OK;
}

JS_STEST_LWSTOM
(
    BitLaneTest,
    Clear,
    0,
    "Clears all allocated memory chunks with 0."
)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    BitLaneTest* const pTest = (BitLaneTest*)JS_GetPrivate(pContext, pObject);

    if (NumArguments != 0)
    {
        JS_ReportError(pContext,
               "Usage: BitLaneTest.Clear()\n");
        return JS_FALSE;
    }

    RETURN_RC(pTest->Clear());
}

JS_STEST_LWSTOM
(
    BitLaneTest,
    Fill,
    0,
    "Fills all allocated memory chunks with specified dword."
)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    BitLaneTest* const pTest = (BitLaneTest*)JS_GetPrivate(pContext, pObject);

    JavaScriptPtr pJavaScript;
    UINT32 value;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &value)))
    {
        JS_ReportError(pContext,
               "Usage: BitLaneTest.Fill(Value32)\n");
        return JS_FALSE;
    }
    RETURN_RC(pTest->Fill(value));
}

JS_STEST_LWSTOM
(
    BitLaneTest,
    SetPatternForAll,
    0,
    "Sets the specified pattern for all partitions, channels and burst orders."
)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    BitLaneTest* const pTest = (BitLaneTest*)JS_GetPrivate(pContext, pObject);

    JavaScriptPtr pJavaScript;
    JsArray jsPattern;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &jsPattern)))
    {
        JS_ReportError(pContext,
               "Usage: BitLaneTest.SetPatternForAll(PatternArray)\n");
        return JS_FALSE;
    }
    vector<UINT08> pattern;
    pattern.reserve(jsPattern.size());
    for (size_t i=0; i < jsPattern.size(); i++)
    {
        UINT16 value;
        if ((OK != pJavaScript->FromJsval(jsPattern[i], &value)) || (value > 1))
        {
            JS_ReportError(pContext,
                   "Invalid pattern value\n");
            return JS_FALSE;
        }
        pattern.push_back((UINT08)value);
    }

    RETURN_RC(pTest->SetPatternForAll(&pattern[0], (UINT32)pattern.size()));
}

JS_STEST_LWSTOM
(
    BitLaneTest,
    SetPattern,
    0,
    "Sets the specified pattern for the specified partition and bit."
)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    BitLaneTest* const pTest = (BitLaneTest*)JS_GetPrivate(pContext, pObject);

    JavaScriptPtr pJavaScript;
    JsArray jsPattern;
    string partition;
    UINT32 bit;
    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &jsPattern)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &partition)) ||
        (partition.length() != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &bit)))
    {
        JS_ReportError(pContext,
               "Usage: BitLaneTest.SetPattern(PatternArray, Partition, Bit)\n");
        return JS_FALSE;
    }
    vector<UINT08> pattern;
    pattern.reserve(jsPattern.size());
    for (size_t i=0; i < jsPattern.size(); i++)
    {
        UINT16 value;
        if ((OK != pJavaScript->FromJsval(jsPattern[i], &value)) || (value > 1))
        {
            JS_ReportError(pContext,
                   "Invalid pattern value\n");
            return JS_FALSE;
        }
        pattern.push_back((UINT08)value);
    }

    RETURN_RC(pTest->SetPattern(partition[0], bit, &pattern[0],
                                static_cast<UINT32>(pattern.size())));
}

JS_STEST_LWSTOM
(
    BitLaneTest,
    Write,
    0,
    "Writes the lwrrently selected pattern"
    " to the specified range of physical addresses."
)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    BitLaneTest* const pTest = (BitLaneTest*)JS_GetPrivate(pContext, pObject);

    JavaScriptPtr pJavaScript;
    UINT64 begin, end;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &begin)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &end)))
    {
        JS_ReportError(pContext,
               "Usage: BitLaneTest.Write(StartPhys, EndPhys)\n");
        return JS_FALSE;
    }

    RETURN_RC(pTest->Write(begin, end));
}

JS_STEST_LWSTOM
(
    BitLaneTest,
    Read,
    0,
    "Reads the specified range of physical addresses"
    " and compares it against the lwrrently selected pattern."
)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    BitLaneTest* const pTest = (BitLaneTest*)JS_GetPrivate(pContext, pObject);

    JavaScriptPtr pJavaScript;
    UINT64 begin, end;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &begin)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &end)))
    {
        JS_ReportError(pContext,
               "Usage: BitLaneTest.Read(StartPhys, EndPhys)\n");
        return JS_FALSE;
    }

    RETURN_RC(pTest->Read(begin, end));
}

JS_STEST_LWSTOM
(
    BitLaneTest,
    Verify,
    0,
    "Verifies the algorithm on the specified range of physical addresses."
)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    BitLaneTest* const pTest = (BitLaneTest*)JS_GetPrivate(pContext, pObject);

    JavaScriptPtr pJavaScript;
    UINT64 begin, end;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &begin)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &end)))
    {
        JS_ReportError(pContext,
               "Usage: BitLaneTest.Verify(StartPhys, EndPhys)\n");
        return JS_FALSE;
    }

    RETURN_RC(pTest->Verify(begin, end));
}

JS_STEST_LWSTOM
(
    BitLaneTest,
    Synchronize,
    0,
    "Waits until all operations are complete."
)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    BitLaneTest* const pTest = (BitLaneTest*)JS_GetPrivate(pContext, pObject);

    if (NumArguments != 0)
    {
        JS_ReportError(pContext,
               "Usage: BitLaneTest.Synchronize()\n");
        return JS_FALSE;
    }

    RETURN_RC(pTest->Synchronize());
}

JS_STEST_LWSTOM
(
    BitLaneTest,
    CheckErrors,
    0,
    "Downloads errors from the GPU."
)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    BitLaneTest* const pTest = (BitLaneTest*)JS_GetPrivate(pContext, pObject);

    if (NumArguments != 0)
    {
        JS_ReportError(pContext,
               "Usage: BitLaneTest.CheckErrors()\n");
        return JS_FALSE;
    }

    RETURN_RC(pTest->CheckErrors());
}

JS_STEST_LWSTOM
(
    BitLaneTest,
    ReportErrors,
    0,
    "Reports any memory errors that oclwrred."
)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    BitLaneTest* const pTest = (BitLaneTest*)JS_GetPrivate(pContext, pObject);

    if (NumArguments != 0)
    {
        JS_ReportError(pContext,
               "Usage: BitLaneTest.ReportErrors()\n");
        return JS_FALSE;
    }

    RETURN_RC(pTest->ReportErrors());
}
