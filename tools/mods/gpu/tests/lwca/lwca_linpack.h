/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2016-2022 by LWPU Corporation. All rights reserved. All
* information contained herein is proprietary and confidential to LWPU
* Corporation. Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

#pragma once

#include "gpu/tests/lwca/linpack/linpack.h"
#include "gpu/tests/lwdastst.h"
#include "inc/bytestream.h"
#include "core/include/crc.h"
#include <regex>

// Shared Arch header objects
#include "linpack/gemm_shader_params.hpp"
#include "linpack/cta_swizzle.hpp"

extern SObject LwdaLinpack_Object;

// Utility functions for printing Matrix data
namespace MatrixData
{
    // Half-Float support
    struct FP16
    {
        UINT16 data;
    };
    struct BF16
    {
        UINT16 data;
    };
    struct TF32
    {
        UINT32 data;
    };

    // Use template specialization
    template<typename T> string FormatType(T data);

    template<>
    inline string FormatType(BF16 wrap)
    {
        return Utility::StrPrintf("%f", Utility::E8M7ToFloat32(wrap.data));
    }
    template<>
    inline string FormatType(TF32 wrap)
    {
        UINT32 adj = wrap.data & ~((1u << 13) - 1);
        return Utility::StrPrintf("%f", *reinterpret_cast<float*>(&adj));
    }
    template<>
    inline string FormatType(FP16 wrap)
    {
        return Utility::StrPrintf("%f", Utility::Float16ToFloat32(wrap.data));
    }
    template<>
    inline string FormatType(INT08 data)
    {
        return Utility::StrPrintf("%d", data);
    }
    template<>
    inline string FormatType(INT32 data)
    {
        return Utility::StrPrintf("%d", data);
    }
    template<>
    inline string FormatType(UINT08 data)
    {
        return Utility::StrPrintf("0x%.2x", data);
    }
    template<>
    inline string FormatType(UINT16 data)
    {
        return Utility::StrPrintf("0x%.4x", data);
    }
    template<>
    inline string FormatType(FLOAT32 data)
    {
        return Utility::StrPrintf("%f", data);
    }
    template<>
    inline string FormatType(FLOAT64 data)
    {
        return Utility::StrPrintf("%f", data);
    }
}

//-------------------------------------------------------------------
//! \brief LWCA-based GEMM stress test
//!

class LwdaLinpack : public LwdaStreamTest
{
public:
    // Type of test
    enum TestMode
    {
        MODE_NORMAL,
        MODE_SHMOO,
        OBSOLETE_MODE_CS, // Preserve ordering
        MODE_WALKING,
        MODE_PULSE
    };

    // Modes on how to target max gpcclk frequency
    //
    // Naming convention:
    // <Name of parameter>_<Method of targeting>
    enum class TargetingMode
    {
        NONE,
        FMAX_PULSE,
        FMAX_SM,
        NUM_MODES
    };

    LwdaLinpack();
    virtual ~LwdaLinpack() {}
    virtual bool IsSupported() override;
    virtual RC InitFromJs() override;
    virtual void PrintJsProperties(Tee::Priority pri) override;
    virtual void PropertyHelp(JSContext *pCtx, regex *pRegex) override;
    virtual RC Setup() override;
    virtual RC Run() override;
    virtual RC RunInner();
    virtual RC Cleanup() override;
    bool IsSupportedVf() override { return true; }

    // JS Property accessors
    SETGET_PROP(Alpha, double);
    SETGET_PROP(Beta, double);
    SETGET_PROP(Msize, UINT32);
    SETGET_PROP(Nsize, UINT32);
    SETGET_PROP(Ksize, UINT32);
    SETGET_PROP(Kstart, UINT32);
    SETGET_PROP(Kstep, UINT32);
    SETGET_PROP(UseLegacyKshmoo, bool);
    SETGET_PROP(MaxKsizeForPulse, UINT32);
    SETGET_PROP(MinPulseUs, double);
    SETGET_PROP(MaxPulseUs, double);
    SETGET_PROP(MaxLaunchDelayUs, double);
    SETGET_PROP(TestMode, UINT32);
    SETGET_PROP(MNKMode, UINT32);
    SETGET_PROP(NumWaves, UINT32);
    SETGET_PROP(NumFracs, UINT32);
    SETGET_PROP(MaxDataDump, UINT32);
    SETGET_PROP(RuntimeMs, UINT32);
    SETGET_PROP(NumNewMatrices, UINT32);
    SETGET_PROP(VerifyResults, bool);
    SETGET_PROP(NaiveInit, bool);
    SETGET_PROP(DumpMiscompares, bool);
    SETGET_PROP(DumpMatrixData, bool);
    SETGET_PROP(SmidFailureLimit, UINT64);
    SETGET_PROP(ReportFailingSmids, bool);
    SETGET_PROP(PrintPerf, bool);
    SETGET_PROP(RecordPerfMetric, bool);
    SETGET_PROP(GflopsLowerBound, double);
    SETGET_PROP(GflopsUpperBound, double);
    SETGET_PROP(KeepRunning, bool);
    SETGET_PROP(UseConstantData, bool);
    SETGET_PROP(UseConstantDataA, bool);
    SETGET_PROP(UseConstantDataB, bool);
    SETGET_PROP(CMatrixScale, UINT32);
    SETGET_PROP(CtaSwizzle, bool);
    SETGET_PROP(MtxDataConstant, float);
    SETGET_PROP(MtxADataConstant, float);
    SETGET_PROP(MtxBDataConstant, float);
    SETGET_PROP(MtxDataMean, float);
    SETGET_PROP(MtxDataSD, float);
    SETGET_PROP(NonZeroPct, float);
    SETGET_PROP(SynchronousMode, bool);
    SETGET_PROP(RandomPulse, bool);
    SETGET_PROP(ShmooPulse, bool);
    SETGET_PROP(CpuThreads, UINT32);
    SETGET_PROP(LaunchDelayUs, double);
    SETGET_PROP(CheckLoops, UINT32);
    SETGET_PROP(BatchDelayUs, double);
    SETGET_PROP(UseCrcToVerify, bool);
    SETGET_PROP(SkipAlphaBetaCheck, bool);
    SETGET_PROP(NumWalks, UINT32);
    SETGET_PROP(WalkScale, double);
    SETGET_PROP(PulseUs, double);
    SETGET_PROP(PulseStepUs, double);
    SETGET_PROP(BatchUs, double);
    SETGET_PROP(DutyPct, double);
    SETGET_PROP(TargetingMode, UINT32);
    SETGET_PROP(PllSampleCount, UINT32);
    SETGET_PROP(SuccessThreshholdCount, UINT32);
    SETGET_PROP(MaxNumSearchTries, UINT32);
    SETGET_PROP(DutyPctSearchStepSize, UINT32);
    SETGET_PROP(PulseUsSearchStepSize, UINT32);
    SETGET_PROP(SMCountSearchStepSize, UINT32);
    SETGET_PROP(SMSelectionMode, UINT32);
    SETGET_PROP(NumDisabledSMs, UINT32);
    SETGET_PROP(NumDisabledGPCs, UINT32);

protected:

    struct KernelParameters
    {
        UINT32 blockDimX;
        UINT32 blockDimY;
        UINT32 rowsPerBlock;
        UINT32 colsPerBlock;
        UINT32 mSizeMultiple;
        UINT32 nSizeMultiple;
        UINT32 kSizeMultiple;

        elementDataType_t typeofInputElement;
        elementDataType_t typeofOutputElement;

        bool   isSparse;
        UINT32 metadataRatio;
        UINT32 sizeofMetadataElement;

        UINT32 shiftFastA;
        INT32  multiplierSlowA;
        INT32  offsetSlowA;
        UINT32 shiftFastB;
        INT32  multiplierSlowB;
        INT32  offsetSlowB;

        UINT32 sharedMemSize;
        UINT32 vectorLanes;
        bool   reLuAndBias;
    };

    struct GemmData
    {
        const char* moduleName;
        const char* genRandomFuncName;
        const char* genConstantFuncName;
        const char* checkFuncName;
        const char* naiveGemmFuncName;
        map<Lwca::Capability::Enum, const char*> linpackFuncNames;
        map<Lwca::Capability::Enum, KernelParameters> smParameters;
    };
    virtual void AddKernelFromArch(Lwca::Capability::Enum cap,
                                   const ShaderParams& params);

    //! \brief Callwlate the number of bytes required to represent the provided number of elements
    //!
    //! This function is necessary since 4-bit and 1-bit types must be packed into 8-bit values
    //!
    //! \param type     Type of element from gemm_shader_params.hpp
    //! \param numElems Number of elements of type 'type'
    //!
    UINT64 NumElemsToBytes(const elementDataType_t type, const UINT64 numElems) const
    {
        switch (type)
        {
            case R_1U:
                MASSERT(numElems % 8 == 0);
                return numElems / 8;
            case R_4U:
            case R_4I:
                MASSERT(numElems % 2 == 0);
                return numElems / 2;
            default:
                return numElems * static_cast<UINT64>(elementDataSize(type));
        }
    }

    //! \brief Callwlate the number of elements represented by the given number of bytes
    //!
    //! This function is necessary since 4-bit and 1-bit types must be packed into 8-bit values
    //!
    //! \param type     Type of element from gemm_shader_params.hpp
    //! \param numBytes Number of bytes used to store the elements
    //!
    UINT64 NumBytesToElems(const elementDataType_t type, const UINT64 numBytes) const
    {
        switch (type)
        {
            case R_1U:
                return numBytes * 8;
            case R_4U:
            case R_4I:
                return numBytes * 2;
            default:
                MASSERT(numBytes % static_cast<UINT64>(elementDataSize(type)) == 0);
                return numBytes / static_cast<UINT64>(elementDataSize(type));
        }
    }

    template<class T>
    RC AllocZeroMemoryBuffer()
    {
        RC rc;
        T zeroVal = 0;
        CHECK_RC(GetLwdaInstance().AllocDeviceMem(sizeof(T), &m_ZeroMemory));
        CHECK_RC(m_ZeroMemory.Set(&zeroVal, sizeof(T)));
        return rc;
    }

    RC AllocReferenceDataMemory()
    {
        RC rc;

        const UINT64 refDataSize = CalcMatrixCMaxBytes();
        if (m_TestMode == MODE_WALKING)
        {
            m_ReferenceDataSubOffset    = 0;
            m_ComparisonResultSubOffset = m_ReferenceDataSubOffset + refDataSize;
            m_MiscompareCountSubOffset  = m_ComparisonResultSubOffset + ERROR_LOG_SIZE_BYTES;
            MASSERT(m_ComparisonResultSubOffset % MATRIX_ALIGN == 0);
            MASSERT(m_MiscompareCountSubOffset % MATRIX_ALIGN == 0);
        }
        else
        {
            CHECK_RC(GetLwdaInstance().AllocDeviceMem(refDataSize, &m_ReferenceData));
            m_ReferenceDataSubOffset    = 0;
            CHECK_RC(GetLwdaInstance().AllocDeviceMem(ERROR_LOG_SIZE_BYTES, &m_ComparisonResult));
            m_ComparisonResultSubOffset = 0;
            CHECK_RC(GetLwdaInstance().AllocDeviceMem(sizeof(UINT64), &m_MiscompareCount));
            m_MiscompareCountSubOffset = 0;
            if (m_ReportFailingSmids)
            {
                CHECK_RC(AllocateSmidBuffers());
            }
        }
        CHECK_RC(GetLwdaInstance().AllocHostMem(ERROR_LOG_SIZE_BYTES, &m_HostComparisonResult));
        CHECK_RC(GetLwdaInstance().AllocHostMem(sizeof(UINT64), &m_HostMiscompareCount));

        return rc;
    }

    RC AllocCrcDataMemory()
    {
        RC rc;

        // m_CrcNumBlocks and m_CrcNumThreadsPerBlock cannot be zero
        MASSERT(m_CrcNumBlocks);
        MASSERT(m_CrcNumThreadsPerBlock);
        const UINT64 crcBufSize = CrcBufSize();

        if (m_TestMode == MODE_WALKING)
        {
            UINT64 subOffset = 0;

            m_RefCrcBufSubOffset = subOffset;
            subOffset += crcBufSize;

            m_DataCrcBufSubOffset = subOffset;
            subOffset += crcBufSize;

            m_CompareCrcSubOffset = subOffset;
            subOffset += crcBufSize;

            m_CrcTableBufSubOffset = subOffset;

            MASSERT(m_RefCrcBufSubOffset % MATRIX_ALIGN == 0);
            MASSERT(m_DataCrcBufSubOffset % MATRIX_ALIGN == 0);
            MASSERT(m_CompareCrcSubOffset % MATRIX_ALIGN == 0);
            MASSERT(m_CrcTableBufSubOffset % MATRIX_ALIGN == 0);
        }
        else
        {
            CHECK_RC(GetLwdaInstance().AllocDeviceMem(crcBufSize, &m_RefDataCrcBuf));
            CHECK_RC(GetLwdaInstance().AllocDeviceMem(crcBufSize, &m_DataCrcBuf));
            CHECK_RC(GetLwdaInstance().AllocDeviceMem(crcBufSize, &m_CompareCrcBuf));
            CHECK_RC(GetLwdaInstance().AllocDeviceMem(Crc::TableSizeBytes(), &m_DevCrcTableBuf));
        }
        CHECK_RC(GetLwdaInstance().AllocHostMem(crcBufSize, &m_HostComparisonResult));

        return rc;
    }

    RC AllocOtherMemory()
    {
        RC rc;
        if (UseReferenceData())
        {
            CHECK_RC(AllocReferenceDataMemory());
        }
        else
        {
            CHECK_RC(AllocCrcDataMemory());
            CHECK_RC(SetCrcTable());
        }

        return rc;
    }

    template<class dataType>
    RC DumpMatrixData(Lwca::DeviceMemory & deviceData, UINT64 deviceDataOffset, UINT64 bufferSize, char matrixLabel)
    {
        RC rc;
        const UINT64 maxBatchItems = 0x10000; // read & compare 64K chunks at a time
        const UINT64 numBatchesToCopy = bufferSize / maxBatchItems + (bufferSize % maxBatchItems ? 1 : 0);

        for (UINT64 batchNum = 0; batchNum < numBatchesToCopy; batchNum++)
        {
            // alloc host memory
            const UINT64 lwrrentBatchOffset = batchNum * maxBatchItems;
            const UINT64 lwrrentBatchItems = min(maxBatchItems, bufferSize - lwrrentBatchOffset);

            vector<dataType, Lwca::HostMemoryAllocator<dataType> > hostData(lwrrentBatchItems,dataType(),Lwca::HostMemoryAllocator<dataType>(GetLwdaInstancePtr(),GetBoundLwdaDevice()));

            //copy data from device in host buffers
            CHECK_RC(deviceData.Get(&hostData[0],deviceDataOffset + sizeof(dataType)*lwrrentBatchOffset, hostData.size()*sizeof(dataType)));
            CHECK_RC(GetLwdaInstance().Synchronize());

            //dump contents
            for (UINT64 i = 0; i < lwrrentBatchItems; i++)
            {
                if (lwrrentBatchOffset + i <= m_MaxDataDump)
                {
                    Printf(Tee::FileOnly, " Matrix: %c, Index %llu = ", matrixLabel, lwrrentBatchOffset + i);
                    Printf(Tee::FileOnly, "%s\n", MatrixData::FormatType<dataType>(hostData[i]).c_str());
                }
            }
        }

        return rc;
    }

    template<class UIntType>
    RC DumpMiscomparesTemplated()
    {
        MASSERT(UseReferenceData());

        RC rc;
        const UINT64 maxBatchItems = 0x10000; // read & compare 64K chunks at a time
        const UINT64 numBatchesToCopy = m_OutputBufferLen / maxBatchItems + (m_OutputBufferLen % maxBatchItems ? 1 : 0);
        // Track number of mismatched values dumped so far.
        UINT64 mismatchDataCount = 0;

        for(UINT64 batchNum=0; (batchNum < numBatchesToCopy) &&
                    (mismatchDataCount < m_MaxDataDump); batchNum++)
        {
            // Alloc host memory
            const UINT64 lwrrentBatchOffset = batchNum * maxBatchItems;
            const UINT64 lwrrentBatchItems = min(maxBatchItems, m_OutputBufferLen - lwrrentBatchOffset);

            vector<UIntType, Lwca::HostMemoryAllocator<UIntType> > refHostData(lwrrentBatchItems, UIntType(), Lwca::HostMemoryAllocator<UIntType>(GetLwdaInstancePtr(),GetBoundLwdaDevice()));

            vector<UIntType, Lwca::HostMemoryAllocator<UIntType> > compareHostData(lwrrentBatchItems,UIntType(),Lwca::HostMemoryAllocator<UIntType>(GetLwdaInstancePtr(),GetBoundLwdaDevice()));

            // Copy data from device in host buffers
            if (m_TestMode == MODE_WALKING)
            {
                CHECK_RC(m_Data.Get(&refHostData[0], CalcRefMatrixCOffset() + sizeof(UIntType)*lwrrentBatchOffset, refHostData.size()*sizeof(UIntType)));
                CHECK_RC(m_Data.Get(&compareHostData[0], CalcMatrixCOffset() + sizeof(UIntType)*lwrrentBatchOffset, compareHostData.size()*sizeof(UIntType)));
            }
            else
            {
                CHECK_RC(m_ReferenceData.Get(&refHostData[0], sizeof(UIntType)*lwrrentBatchOffset, refHostData.size()*sizeof(UIntType)));
                CHECK_RC(m_Data.Get(&compareHostData[0], sizeof(UIntType)*lwrrentBatchOffset, compareHostData.size()*sizeof(UIntType)));
            }
            CHECK_RC(GetLwdaInstance().Synchronize());

            // Compare and dump the range of values that miscompare
            for (UINT64 i = 0; (i < lwrrentBatchItems) && (mismatchDataCount < m_MaxDataDump); i++ )
            {
                // do an integer comaprison
                UINT64 src = static_cast<UIntType>(refHostData[i]);
                UINT64 dst = static_cast<UIntType>(compareHostData[i]);
                if(src != dst )
                {
                    Printf(Tee::PriNormal, "Miscompare at index %llu, values = 0x%llx and 0x%llx\n", lwrrentBatchOffset + i, src, dst);
                    mismatchDataCount++;
                }
            }

        }

        return rc;
    }

    UINT64 CalcMatrixCLen() const { return static_cast<UINT64>(m_Msize) * m_Nsize; }
    UINT64 CalcMatrixALen() const
    {
        UINT64 matrixLen = static_cast<UINT64>(m_Msize) * m_Ktrue;
        return m_KernParams.isSparse ? (matrixLen / 2ul) : matrixLen;
    }
    UINT64 CalcMatrixBLen() const { return static_cast<UINT64>(m_Ktrue) * m_Nsize; }
    UINT64 CalcMatrixAmLen() const
    {
        return m_KernParams.isSparse ?
               (CalcMatrixALen() / m_KernParams.metadataRatio) : 0;
    }

    UINT64 CalcMatrixCOffset() const { return m_CMatrixOffset; }
    UINT64 CalcMatrixAOffset() const { return m_ABMatrixOffset; }
    UINT64 CalcMatrixBOffset() const { return m_ABMatrixOffset + CalcMatrixAMaxBytes(); }
    UINT64 CalcMatrixAmOffset() const { return m_ABMatrixOffset + CalcMatrixAMaxBytes() + CalcMatrixBMaxBytes(); }

    UINT64 CalcMatrixCPtr() const { return m_Data.GetDevicePtr() + CalcMatrixCOffset(); }
    UINT64 CalcMatrixAPtr() const { return m_Data.GetDevicePtr() + CalcMatrixAOffset(); }
    UINT64 CalcMatrixBPtr() const { return m_Data.GetDevicePtr() + CalcMatrixBOffset(); }
    UINT64 CalcMatrixAmPtr() const
    {
        return m_KernParams.isSparse ? (m_Data.GetDevicePtr() + CalcMatrixAmOffset()) : 0;
    }

    UINT64 CalcRefMatrixCOffset() const { return m_OtherDataOffset + m_ReferenceDataSubOffset; }
    UINT64 CalcRefMatrixCPtr() const
    {
        const UINT64 devPtr = (m_TestMode == MODE_WALKING)
                              ? m_Data.GetDevicePtr()
                              : m_ReferenceData.GetDevicePtr();
        return devPtr + CalcRefMatrixCOffset();
    }

    UINT64 CalcComparisonResultPtr() const
    {
        const UINT64 devPtr = (m_TestMode == MODE_WALKING)
                              ? m_Data.GetDevicePtr()
                              : m_ComparisonResult.GetDevicePtr();
        return devPtr + m_OtherDataOffset + m_ComparisonResultSubOffset;
    }
    UINT64 CalcMicompareCountPtr() const
    {
        const UINT64 devPtr = (m_TestMode == MODE_WALKING)
                              ? m_Data.GetDevicePtr()
                              : m_MiscompareCount.GetDevicePtr();
        return devPtr + m_OtherDataOffset + m_MiscompareCountSubOffset;
    }

    UINT64 CalcCrcTableBufPtr() const
    {
        const UINT64 devPtr = (m_TestMode == MODE_WALKING)
                              ? m_Data.GetDevicePtr()
                              : m_DevCrcTableBuf.GetDevicePtr();
        return devPtr + m_OtherDataOffset + m_CrcTableBufSubOffset;
    }
    UINT64 CalcRefCrcBufPtr() const
    {
        const UINT64 devPtr = (m_TestMode == MODE_WALKING)
                              ? m_Data.GetDevicePtr()
                              : m_RefDataCrcBuf.GetDevicePtr();
        return devPtr + m_OtherDataOffset + m_RefCrcBufSubOffset;
    }
    UINT64 CalcDataCrcBufPtr() const
    {
        const UINT64 devPtr = (m_TestMode == MODE_WALKING)
                              ? m_Data.GetDevicePtr()
                              : m_DataCrcBuf.GetDevicePtr();
        return devPtr + m_OtherDataOffset + m_DataCrcBufSubOffset;
    }
    UINT64 CalcCompareCrcBufPtr() const
    {
        const UINT64 devPtr = (m_TestMode == MODE_WALKING)
                              ? m_Data.GetDevicePtr()
                              : m_CompareCrcBuf.GetDevicePtr();
        return devPtr + m_OtherDataOffset + m_CompareCrcSubOffset;
    }
    UINT64 CrcBufSize() const { return m_CrcNumBlocks * m_CrcNumThreadsPerBlock * sizeof(UINT32); }

    // Work per thread = ceil(buffer size/ Total threads)
    UINT64 CalcWorkPerThread(UINT64 bufLen) const { return (bufLen + m_NumThreads * m_NumBlocks-1)/(m_NumThreads * m_NumBlocks); }
    inline bool UseReferenceData() const { return (!m_UseCrcToVerify); }
    RC SetCrcTable();
    RC CallwlateCrc(UINT64 dataPtr, UINT64 crcBufPtr);
    RC CompareCrcs(UINT64 crcBuf1Ptr, UINT64 crcBuf2Ptr, bool* pResult);

    virtual RC Launch(const ByteStream& paramBuffer);

    template<typename T>
    void FillParamBufferCommon(UINT64 A, UINT64 B, UINT64 C, T alpha, T beta, ByteStream& paramBuffer)
    {
        UINT32 tempData = 0;
        UINT64 tempData64 = 0;
        INT64 tempDataI64 = 0;

        paramBuffer.PushPadded(A);
        paramBuffer.PushPadded(B);
        paramBuffer.PushPadded(C);

        const UINT32 strideA = m_Msize * m_KernParams.vectorLanes;
        const UINT32 strideB = m_Nsize * m_KernParams.vectorLanes;
        const UINT32 strideC = m_Msize * m_KernParams.vectorLanes;

        tempData64 = static_cast<UINT64>(strideA) << m_KernParams.shiftFastA;
        paramBuffer.PushPadded(tempData64);
        tempData64 = static_cast<UINT64>(strideB) << m_KernParams.shiftFastB;
        paramBuffer.PushPadded(tempData64);

        tempDataI64 =
            static_cast<INT64>(strideA) * m_KernParams.multiplierSlowA + m_KernParams.offsetSlowA;
        paramBuffer.PushPadded(tempDataI64);
        tempDataI64 =
            static_cast<INT64>(strideB) * m_KernParams.multiplierSlowB + m_KernParams.offsetSlowB;
        paramBuffer.PushPadded(tempDataI64);

        tempData64 = 0;
        paramBuffer.PushPadded(tempData64); // matrixStrideA
        paramBuffer.PushPadded(tempData64); // matrixStrideB
        paramBuffer.PushPadded(tempData64); // matrixStrideC

        paramBuffer.PushPadded(strideA); // StrideA
        paramBuffer.PushPadded(strideB); // StrideB
        paramBuffer.PushPadded(strideC); // StrideC

        const UINT32 adjustedK = (m_Ktrue / m_KernParams.vectorLanes);
        paramBuffer.PushPadded(m_Msize);   // CountM
        paramBuffer.PushPadded(m_Nsize);   // CountN
        paramBuffer.PushPadded(adjustedK); // CountK

        UINT32 mode = 0x0;
        if (m_ReportFailingSmids)
        {
            mode |= 0x10000;
            paramBuffer.PushPadded(m_SmidMap.GetDevicePtr());
        }
        else
        {
            tempData64 = 0;
            paramBuffer.PushPadded(tempData64);
        }
        if (m_CtaSwizzle)
        {
            CtaSwizzle swizzle
            (
                CtaSwizzle::Pos3(m_GridDimX, m_GridDimY, 1),
                CtaSwizzle::Pos2
                (
                    static_cast<UINT32>(NumElemsToBytes(m_KernParams.typeofInputElement,
                                                        m_KernParams.rowsPerBlock)),
                    static_cast<UINT32>(NumElemsToBytes(m_KernParams.typeofInputElement,
                                                        m_KernParams.colsPerBlock))
                )
            );
            UINT32 maxBlocksPerWave = m_MaxBlocksPerSM * GetBoundLwdaDevice().GetShaderCount();
            UINT32 best = swizzle.chooseBestLog2GroupCols(maxBlocksPerWave);
            mode &= ~CTA_SWIZZLE_MASK;
            mode |= (best << CTA_SWIZZLE_START);
            mode |= CTA_SWIZZLE_SERPENTINE;
        }

        tempData = 0;
        paramBuffer.PushPadded(tempData);   // k chunk size
        paramBuffer.PushPadded(mode);       // mode

        // Alpha/beta ref addresses. Arch expects a value of 1 to specify these
        // addresses are unused
        tempData64 = 1;
        paramBuffer.PushPadded(tempData64); // Alpha ref
        paramBuffer.PushPadded(tempData64); // Beta ref

        // The offset for all kernels at this point is the same (0x1e8 on Volta)
        // From here on out the offset of each field varies by kernel
        paramBuffer.PushPadded(alpha); // alpha
        paramBuffer.PushPadded(beta); // beta

        tempData = 0;
        paramBuffer.PushPadded(tempData); // alphaBetaByRef

        if (m_FullyConnected)
        {
            // Set reLu parameters
            //
            // reLu kernels are of the form:
            // newC[i][j] = alpha[i] * max(reLu, ACC) + bias[i] + beta[i] * oldC[i][j]
            //
            // Argument buffer configuration derived from
            // //dev/compute_arch/src/fast_kernels/trunk/src/sass_generator/common/gemm.cpp
            constexpr float milwal = -std::numeric_limits<float>::infinity();
            constexpr float maxVal =  std::numeric_limits<float>::infinity();
            paramBuffer.PushPadded(milwal); // set ReLu to -inf (ignore)

            constexpr UINT32 zero32 = 0;
            constexpr UINT64 zero64 = 0;
            paramBuffer.PushPadded(zero32); // don't use a bias
            paramBuffer.PushPadded(maxVal); // upper bound (ignore)
            paramBuffer.PushPadded(zero64); // bias ptr if bias was being used
        }

        // Set Dptr = Cptr
        // (C = alpha*AB + beta*D)
        paramBuffer.PushPadded(C);

        if (m_UseRegDescriptor)
        {
            tempData = 0x10000000;
            paramBuffer.PushPadded(tempData); // UR register descriptor A
            paramBuffer.PushPadded(tempData); // UR register descriptor B
            tempData = 0;
            paramBuffer.PushPadded(tempData); // UR register descriptor C0
            paramBuffer.PushPadded(tempData); // UR register descriptor C1
        }
    }

    virtual void SetGridDimX()
    {
        MASSERT(m_Msize != 0);
        m_GridDimX = m_Msize / m_KernParams.rowsPerBlock;
    }

    virtual void SetGridDimY()
    {
        MASSERT(m_Nsize != 0);
        m_GridDimY = m_Nsize / m_KernParams.colsPerBlock;
    }

    virtual const char* GemmFunctionName();
    virtual RC InitAlphaBeta();
    virtual RC ValidateAlphaBeta();
    virtual RC InitializeMParameterNormal();
    virtual RC InitializeNParameterNormal();
    virtual RC InitializeKParameterNormal();
    virtual RC InitializeMNParameterWave();
    virtual RC InitGemmFunc();
    virtual RC ConfigGemmFuncDim();
    virtual RC LaunchGemmKernel(const ByteStream& paramBuffer);
    virtual RC GenerateCMatrices(double* estTimeMsForOneLoop);
    virtual RC RunGemm(UINT64 matrixAPtr, UINT64 matrixBPtr, UINT64 matrixCPtr, bool initialize) = 0;
    virtual RC AllocMemory();
    virtual RC AllocateSmidBuffers();
    virtual RC CopySmidMapToHost(Lwca::HostMemory& hostSmidMap);
    virtual RC DumpMiscompares();
    virtual RC DumpMatrices(bool isRefData);
    virtual RC WalkMatrices(UINT64* amtToTestC, UINT64* amtToTestAB);
    virtual UINT64 CalcOpCount(const UINT32 mSize,
                               const UINT32 nSize,
                               const UINT32 kSize,
                               const bool initialize) const;

    GemmData m_GemmData;
    KernelParameters m_KernParams;

    //! m_Msize, m_Nsize and m_Ksize make up the size of 3 matrices A,B and C
    //! buffer size for matrix A = m_Msize*m_Ksize
    //! buffer size for matrix B = m_Nsize*m_Ksize
    //! buffer size for matrix C = m_Msize*m_Nsize
    UINT32 m_Msize;
    UINT32 m_Nsize;
    UINT32 m_Ksize;
    UINT32 m_Ktrue;
    UINT32 m_Kstart;
    UINT32 m_Kstep;
    bool   m_UseLegacyKshmoo;
    UINT32 m_MaxKsizeForPulse;     //!< Max Ksize to set when pulsing, needed for buffer allocations
    double m_MinPulseUs;
    double m_MaxPulseUs;
    bool   m_UsePulseUsRange;
    vector<UINT32> m_Kset;

    UINT32 m_GridDimX;              //!< Grid Dim along X
    UINT32 m_GridDimY;              //!< Grid Dim along Y

    double m_Alpha;                 //!< Result (A*B) matrix scaling factor used in GEMM
    double m_Beta;                  //!< C input matrix scaling factor used in GEMM
    double m_DefaultAlpha;          //!< Default m_Alpha value
    double m_DefaultBeta;           //!< Default m_Beta value
    double m_LaunchDelayUs;         //!< Delay in microseconds between kernel launches
    double m_MaxLaunchDelayUs;      //!< Max launch delay used for random pulsing

    UINT32 m_CheckLoops;            //!< Num kernel launches before checking results (size of batch)
    double m_BatchDelayUs;          //!< Wait in-between batches of kernel launches

    bool   m_VerifyResults;         //!< Do/Do not verify the results
    bool   m_NaiveInit;             //!< Do/Do not use a simpler GEMM kernel for init
    bool   m_DumpMatrixData;        //!< To dump matrix contents or not
    bool   m_ReportFailingSmids;    //!< Report SM/TPC which computed the miscompare

    Lwca::Module m_Module;
    Lwca::Module m_ModuleGemm;
    Lwca::Module m_ModuleLwSurf;    //!< Module used to compute CRCs for Matrix C
    Lwca::Module m_ModuleGpuSync;   //!< Module used to synchronize kernels or delay launch

    Lwca::Function m_GemmFunc;
    Lwca::Function m_ComputeCrcFunc;
    Lwca::Function m_CompareCrcFunc;
    Lwca::Function m_SyncFunc;
    Lwca::Function m_DelayFunc;
    Lwca::Function m_WriteU32Func;
    Lwca::Function m_InitOclwpyBarrierFunc;
    Lwca::Function m_OclwpyFunc;

    Lwca::DeviceMemory m_Data;
    Lwca::DeviceMemory m_ReferenceData;
    Lwca::DeviceMemory m_ZeroMemory;
    Lwca::DeviceMemory m_DevCrcTableBuf; //!< CRC table is passed in this buffer
    Lwca::DeviceMemory m_RefDataCrcBuf;  //!< CRCs generated from Reference Data
    Lwca::DeviceMemory m_DataCrcBuf;     //!< CRCs generated from Data
    Lwca::DeviceMemory m_CompareCrcBuf;

    Lwca::DeviceMemory m_SmidMap;
    Lwca::HostMemory   m_HostSmidMap;

    bool   m_UseCrcToVerify;
    bool   m_SkipAlphaBetaCheck;
    UINT32 m_NumWalks;
    double m_WalkScale;
    UINT32 m_CMatrixScale;
    bool   m_CtaSwizzle;

    UINT32 m_NumThreads;
    UINT32 m_NumBlocks;
    UINT64 m_MaxInputBufferLen;
    UINT64 m_OutputBufferLen;
    Lwca::Capability m_ComputeCap;
    UINT32 m_MaxBlocksPerSM;

    UINT32 m_TestMode;

    // Type of instruction used by the kernel to do the work
    using Instr = GpuSubdevice::Instr;
    Instr m_Instr = Instr::UNKNOWN;

    // Whether the kernel is fully connected (support reLU and bias)
    bool m_FullyConnected   = false;

    // Whether a register descriptor must be passed in as kernel argument
    // (Only on Ampere)
    bool m_UseRegDescriptor = false;

    // Max size of input/output matrix in number of elements
    UINT64 m_MaxMatrixNumElements = std::numeric_limits<UINT64>::max();

    // Min N size to optimize for GEMM stress
    static constexpr UINT32 s_MinNSize = 1024;
private:

    enum EventIdxs
    {
        EvBeginGemm,
        EvBeginTimeCalc,
        EvBeginBatch,
        EvEnd,
        EvEndCheck,
        EvEndSampling,
        EvNumEvents
    };

    // Walking Linpack state machine
    enum class WalkingState
    {
        NORMAL,
        OTHER_WRAPPED,
        AB_WRAPPED
    };

    // Modes on how to handle unaligned MNK values
    enum MNKMode
    {
        MNK_STRICT,
        MNK_ALIGN,
        MNK_PERMIT,
        MNK_WAVE
    };

	// Mode on how to select SMs to occupy
    enum class SMSelectionMode : UINT32
    {
        RANDOM,
        RANDOM_GPC,
        NUM_MODES
    };

    // Search state of the binary searcher
    enum class BinSearchResult
    {
        TARGET_ABSENT,
        TARGET_FOUND,
        STILL_SEARCHING
    };

    // Pulse parameter adjustment stage progression
    enum class PulseAdjustmentStage
    {
        ADJUSTING_PULSE_WIDTH,
        ADJUSTING_DUTY,
        DONE
    };

    // Required alignment of matrices in LWCA
    static constexpr UINT64 MATRIX_ALIGN = 16ULL;

    inline bool IsWorkloadConstant () const
    {
        // M/N/K change when running pulsed versions
        return (m_TestMode != MODE_SHMOO);
    }

    static const char* GetTestModeStr(TestMode testMode);
    static const char* GetMNKModeStr(MNKMode mnkMode);
    static const char* GetTargetingModeStr(TargetingMode targetingMode);
    static const char* GetSMSelectionModeStr(SMSelectionMode selectionMode);
    static bool IncreaseMtxDimension(UINT32 * value, UINT32 milwalue, UINT32 maxValue);
    static bool DecreaseMtxDimension(UINT32 * value, UINT32 milwalue, UINT32 maxValue);

    RC FillABMatrices(UINT32 salt);
    RC Verify();
    RC VerifyUsingReferenceData();
    RC InitializeMNKParameters();
    RC RunNaiveGemm(UINT64 matrixAPtr, UINT64 matrixBPtr, UINT64 matrixCPtr);
    RC SynchronousKernelLaunch();
    RC ValidateMNKParameters(UINT32 smCount);
    UINT64 CalcMatrixCMaxBytes() const;
    UINT64 CalcMatrixAMaxBytes() const;
    UINT64 CalcMatrixAmMaxBytes() const;
    UINT64 CalcMatrixBMaxBytes() const;
    UINT64 CalcOtherMemBytes() const;
    UINT64 CalcTotalMemBytes() const
    {
        return CalcMatrixCMaxBytes()  +
               CalcMatrixAMaxBytes()  +
               CalcMatrixBMaxBytes()  +
               CalcMatrixAmMaxBytes() +
               CalcOtherMemBytes();
    }
    RC GetMemAllocInfo();
    void PrintMatrixOffsets(Tee::Priority pri) const;

    RC StartCpuStress();
    Tasker::ThreadID CreateCpuStressThread(UINT32 threadIdx);
    RC StopCpuStress();

    RC IsCrcBasedVerificationPossible();
    RC InjectError(Lwca::DeviceMemory &deviceMem, UINT64 location, UINT64 errValue);

    virtual RC ScaleReferenceData(UINT32 scale);
    virtual RC ResetReferenceData();

    bool TestRunTimeExceeded(double timeSoFarMs);

    bool IsLwrrTargetPllAtMax();
    void AdjustPulseUs();
    void AdjustDutyPct();
    void AdjustSMCount();
    inline RC QueryTargetPllkHz(UINT64 *pTargetPLL)
    {
        RC rc = GetBoundGpuSubdevice()->GetClock(Gpu::ClkGpc, NULL, pTargetPLL, NULL, NULL);
        *pTargetPLL /= 1000; // Colwert Hz to kHz to null out small degree of differences
        return rc;
    }

    RC ConfigurePulse(UINT32* pKval, UINT32* batchSize);

    RC OclwpySMs();
    RC VacateSMs();
    RC SelectSMsToOclwpy();
    RC RandomlySelectSMs();
    RC RandomlySelectGPCs();

    UINT32 m_MNKMode;
    UINT32 m_NumWaves;
    UINT32 m_NumFracs;
    UINT32 m_MaxDataDump;

    UINT32 m_RuntimeMs;                 //!< Maximum time the test can run
    UINT32 m_NumNewMatrices;            //!< Number of new matrices to generate while looping
    bool   m_KeepRunning;               //!< Keep running until told to stop
    bool   m_DumpMiscompares;           //!< To dump miscompares or not
    bool   m_PrintPerf;                 //!< To print perf values in mods log file or not
    bool   m_RecordPerfMetric;          //!< To record GFLOPS performance metric
    double m_GflopsLowerBound;          //!< Check perf against user-provided lower bound
    double m_GflopsUpperBound;          //!< Check perf against user-provided upper bound
    bool   m_UseConstantData;           //!< To use constant data to fill in matrices or not
    bool   m_UseConstantDataA;          //!< To use constant data to fill in matrix A or not
    bool   m_UseConstantDataB;          //!< To use constant data to fill in matrix B or not
    float  m_MtxDataConstant;           //!< The constant data used to fill matrices
    float  m_MtxADataConstant;          //!< The constant data used to fill matrix A
    float  m_MtxBDataConstant;          //!< The constant data used to fill matrix B
    float  m_MtxDataMean;               //!< Mean of random data used to fill matrices
    float  m_MtxDataSD;                 //!< SD of random data used to fill matrices
    float  m_NonZeroPct;                //!< Percent from 0.0 to 100.0 of non-zero matrix elements
    bool   m_SynchronousMode;           //!< To use synchronous mode or not
    UINT32 m_CpuThreads = 0U;           //!< Number of CPU stress threads to launch
    bool   m_RandomPulse;               //!< To use random frequencies in Pulse mode
    bool   m_ShmooPulse;                //!< To use random frequencies in Pulse mode
    Random m_Random;                    //!< Random generator for MODE_PULSE and SM-based fmax
    bool   m_OclwpySMs;                 //!< Indicates if we should launch a kernel to idle 1 or more SMs

    vector<Tasker::ThreadID> m_CpuStressThreads; //!< Spawned CPU stress threads
    static atomic<UINT32>    s_CpuStressState;   //!< Manages termination of CPU stress threads
    static atomic<UINT32>    s_CpuBarrier;       //!< Used for syncing CPU stress threads
    static atomic<UINT64>    s_CpuLoopIdx;       //!< Used for syncing CPU stress with GPU threads

    // Constants for s_CpuStressState
    static constexpr UINT32 CSGo          = 0;
    static constexpr UINT32 CSKill        = 1;
    static constexpr UINT32 CSUnsupported = 2;
    static constexpr UINT32 CSFailed      = 3;

    UINT64 m_SmidFailureLimit;
    map<UINT16, UINT64> m_SmidMiscompareCount;

    Lwca::Stream m_SMOclwpancyStream;

    Lwca::Function m_GenRndmDataFunc;
    Lwca::Function m_GenConstantDataFunc;
    Lwca::Function m_CompareOnDevFunc;
    Lwca::Function m_NaiveGemmFunc;
    Lwca::Function m_GenMetadataFunc;

    Lwca::DeviceMemory m_ComparisonResult;
    Lwca::DeviceMemory m_MiscompareCount;
    Lwca::HostMemory   m_HostComparisonResult;
    Lwca::HostMemory   m_HostMiscompareCount;
    Lwca::HostMemory   m_HostAllBlocksScheduled;
    Lwca::HostMemory   m_HostOclwpySMFlags;

    UINT64 m_SyncKernelCount;

    Lwca::Event m_Events[EvNumEvents];

    UINT32 m_CrcNumThreadsPerBlock;
    UINT32 m_CrcNumBlocks;

    UINT64 m_TotalFreeMem;       //!< Amount of available memory allocated for Walking Linpack test
    UINT64 m_TotalMem;           //!< Total amount of memory available in the framebuffer
    UINT64 m_CMatrixOffset;      //!< Offset of the read/write (C) matrix in Walking Mode
    UINT64 m_ABMatrixOffset;     //!< Offset of the read (AB) matrix in Walking Mode
    UINT64 m_OtherDataOffset;    //!< Offset of the other (Reference, CRC) data in Walking Mode
    WalkingState m_WalkingState; //!< State machine variable for Walking Linpack mode

    UINT64 m_ReferenceDataSubOffset;
    UINT64 m_ComparisonResultSubOffset;
    UINT64 m_MiscompareCountSubOffset;

    UINT64 m_CrcTableBufSubOffset;
    UINT64 m_RefCrcBufSubOffset;
    UINT64 m_DataCrcBufSubOffset;
    UINT64 m_CompareCrcSubOffset;

    // Binary search helper where elements look like
    // arr = [true, true, true, false, false false, false]
    //                     ^
    //
    // The searcher tells the client which indices to callwlate the
    // boolean value of, and the client passes in the callwlated boolean value
    //
    // Target is the index of the last true element
    // Initialize the searcher with "low" and "high" index
    // Each level of search is performed by calling the method RunNextStep
    // RunNextStep input  = The boolean value of arr[idx]
    // RunNextStep output = Next idx to check the boolean value for && search result
    class BinarySearcher
    {
    public:
        BinarySearcher(size_t low, size_t high, size_t stepSize)
        : m_InitialLow(low)
        , m_InitialHigh(high)
        , m_Low(low)
        , m_Mid(0)
        , m_High(high)
        , m_StepSize(stepSize)
        , m_SearchStarted(false)
        , m_TrueElemExists(false)
        , m_RightMostTrueIdx(0)
        {}
        bool   HasSearchStarted();
        size_t StartAndGetStartingIdx();
        size_t RunNextStep(bool, BinSearchResult *);
        void   Reset();

    private:
        size_t m_InitialLow;        // Initial low index
        size_t m_InitialHigh;       // Initial high
        size_t m_Low;               // Lowest index of our search range
        size_t m_Mid;               // Middle index of our search range
        size_t m_High;              // Highest index of our search range
        size_t m_StepSize;          // Interval between one index value and the next
        bool   m_SearchStarted;     // Has search started
        bool   m_TrueElemExists;    // Have we encountered a true elem in our array
        size_t m_RightMostTrueIdx;  // Right-most index at which the elem is true
    };

    // Helper function that instantiates a BinarySearcher and checks for
    // valid constructor parameters
    inline RC MakeSearcher(unique_ptr<BinarySearcher> *ppSearcher, size_t low, size_t high, size_t stepSize)
    {
        if (low > high)
        {
            Printf(Tee::PriError, "Invalid parameter to BinarySearcher. "
                                  "low (%lu) > high (%lu)\n", low, high);
            return RC::SOFTWARE_ERROR;
        }

        if (stepSize == 0)
        {
            Printf(Tee::PriError, "Step size must be a non-zero value.\n");
            return RC::SOFTWARE_ERROR;
        }

        if ((low % stepSize != 0) || (high % stepSize != 0))
        {
            Printf(Tee::PriError, "Low (%lu) and High (%lu) parameters must be "
                                  "evenly divisibly by step size (%lu)\n",
                                  low, high, stepSize);
            return RC::SOFTWARE_ERROR;
        }

        *ppSearcher = make_unique<BinarySearcher>(low, high, stepSize);

        return RC::OK;
    }

    UINT32 m_TargetingMode;                 //!< Mode that indicates which GPU param we're targeting and how
    bool   m_TargetingFmax;                 //!< True if we're targeting fmax
    bool   m_CanAdjustPulseWidth;           //!< If PulseUs or Ksize is unspecified, adjust it ourselves
    UINT64 m_MaxTargetPllkHz;               //!< Target Pll of gpcclk in idle state
    UINT32 m_PllSampleCount;                //!< Number of times to query target Pll after each batch
    UINT32 m_SuccessThreshholdCount;        //!< (# sample PllkHz values) >= (SuccessThreshholdCount)
    UINT32 m_NumSearchTries;                //!< Number of times we couldn't find params to run at fmax
    UINT32 m_MaxNumSearchTries;             //!< Max number of retries
    UINT32 m_DutyPctSearchStepSize;         //!< Step size when running binary search on DutyPct
    UINT32 m_PulseUsSearchStepSize;         //!< Step size when running binary search on PulseUs
    UINT32 m_SMCountSearchStepSize;         //!< Step size when running binary search on SM count
    vector<UINT64> m_TargetPllkHzSamples;   //!< Holds samples of target PllkHz
    PulseAdjustmentStage m_PulseAdjustmentStage; //!< Current pulse adjustment stage
    bool   m_DoneAdjustingSMCount;

    static constexpr UINT32  DEFAULT_PLL_SAMPLE_COUNT     = 3;
    static constexpr UINT32  NUM_BATCH_LOOPS_PER_SAMPLE   = 5;
    static constexpr size_t  BIN_SEARCH_MIN_PULSE_US      = 1000;
    static constexpr size_t  BIN_SEARCH_MAX_PULSE_US      = 7000;

    // Minimum number of SMs that should stay active (for perf reasons)
    static constexpr size_t  BIN_SEARCH_MIN_SM_COUNT      = 5;


    // Binary search helper to find the best parameters for targeting some GPU param
    unique_ptr<BinarySearcher> m_pDutyPctBinSearcher;
    unique_ptr<BinarySearcher> m_pPulseUsBinSearcher;
    unique_ptr<BinarySearcher> m_pSMCountBinSearcher;

    UINT32 m_PrevK   = 0;
    double m_PrevBatchMs = 0;
    UINT32 m_PrevNumLoops = 0;
    double m_PulseUs = 0;
    double m_PulseStepUs = 0;
    double m_BatchUs = 0;
    double m_DutyPct = 0;

    UINT32 m_SMSelectionMode;
    UINT32 m_NumDisabledSMs;
    UINT32 m_NumDisabledGPCs;
    vector<UINT32> m_RangeSMIDs;
    vector<UINT32> m_RangeGPCs;
    vector<vector<UINT32>> m_GPCToSMIDsMap;

    pair<INT32, INT32> m_MemTmpRange;
    pair<INT32, INT32> m_GlobalMemTmpRange;

    pair<INT32, INT32> m_GpuTmpRange;
    pair<INT32, INT32> m_GlobalGpuTmpRange;

    static Lwca::HostMemory s_GpuSync;
    static Lwca::HostMemory s_TimeoutMatrix;
    static bool             s_TimedOut;
};
