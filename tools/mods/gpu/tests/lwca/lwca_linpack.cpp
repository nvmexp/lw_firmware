/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

#include "lwda_linpack.h"
#include "lwca.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "core/include/mgrmgr.h"
#include "core/include/jsonlog.h"
#include "core/include/utility.h"
#include "core/include/tasker.h"
#include "core/include/help.h"
#include "device/cpu/tests/cpustress/cpustress.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpumgr.h"
#include "gpu/perf/thermsub.h"
#include "gpu/tests/lwca/tools/gpusync.h"
#include "cheetah/include/tegrasocdeviceptr.h"
#include "cheetah/include/tegrasocdevice.h"
#include <numeric>             // for std::accumulate()
#include <float.h>

Lwca::HostMemory LwdaLinpack::s_GpuSync;
Lwca::HostMemory LwdaLinpack::s_TimeoutMatrix;
bool             LwdaLinpack::s_TimedOut = false;
atomic<UINT32>   LwdaLinpack::s_CpuStressState;

//-----------------------------------------------------------------------------
//! \brief Constructor
//!
LwdaLinpack::LwdaLinpack()
: m_GemmData()
, m_KernParams()
, m_Msize(0)
, m_Nsize(0)
, m_Ksize(0)
, m_Ktrue(0)
, m_Kstart(8)
, m_Kstep(8)
, m_UseLegacyKshmoo(true)
, m_MaxKsizeForPulse(0)
, m_MinPulseUs(0.0)
, m_MaxPulseUs(0.0)
, m_UsePulseUsRange(false)
, m_GridDimX(0)
, m_GridDimY(0)
, m_Alpha(std::numeric_limits<float>::infinity())
, m_Beta(std::numeric_limits<float>::infinity())
, m_DefaultAlpha(0.5)
, m_DefaultBeta(0.5)
, m_LaunchDelayUs(0)
, m_MaxLaunchDelayUs(1000)
, m_CheckLoops(0)
, m_BatchDelayUs(0)
, m_VerifyResults(true)
, m_NaiveInit(false)
, m_DumpMatrixData(false)
, m_ReportFailingSmids(false)
, m_UseCrcToVerify(false)
, m_SkipAlphaBetaCheck(false)
, m_NumWalks(1)
, m_WalkScale(1.0)
, m_CMatrixScale(1)
, m_CtaSwizzle(false)
, m_NumThreads(0)
, m_NumBlocks(0)
, m_MaxInputBufferLen(0)
, m_OutputBufferLen(0)
, m_ComputeCap(0, 0)
, m_MaxBlocksPerSM(0)
, m_TestMode(MODE_NORMAL)
, m_MNKMode(MNK_WAVE)
, m_NumWaves(0)
, m_NumFracs(0)
, m_MaxDataDump(_UINT32_MAX)
, m_RuntimeMs(0)
, m_NumNewMatrices(0)
, m_KeepRunning(false)
, m_DumpMiscompares(false)
, m_PrintPerf(false)
, m_RecordPerfMetric(false)
, m_GflopsLowerBound(0.0)
, m_GflopsUpperBound(0.0)
, m_UseConstantData(false)
, m_UseConstantDataA(false)
, m_UseConstantDataB(false)
, m_MtxDataConstant(0.0)
, m_MtxADataConstant(0.0)
, m_MtxBDataConstant(0.0)
, m_MtxDataMean(0.0)
, m_MtxDataSD(5.0)
, m_NonZeroPct(100.0)
, m_SynchronousMode(false)
, m_RandomPulse(false)
, m_ShmooPulse(false)
, m_OclwpySMs(false)
, m_SmidFailureLimit(1)
, m_SyncKernelCount(0)
, m_CrcNumThreadsPerBlock(0)
, m_CrcNumBlocks(0)
, m_TotalFreeMem(0)
, m_TotalMem(0)
, m_CMatrixOffset(0)
, m_ABMatrixOffset(0)
, m_OtherDataOffset(0)
, m_WalkingState(WalkingState::NORMAL)
, m_ReferenceDataSubOffset(0)
, m_ComparisonResultSubOffset(0)
, m_MiscompareCountSubOffset(0)
, m_CrcTableBufSubOffset(0)
, m_RefCrcBufSubOffset(0)
, m_DataCrcBufSubOffset(0)
, m_CompareCrcSubOffset(0)
, m_TargetingMode(static_cast<UINT32>(TargetingMode::NONE))
, m_TargetingFmax(false)
, m_CanAdjustPulseWidth(false)
, m_MaxTargetPllkHz(0)
, m_PllSampleCount(0)
, m_SuccessThreshholdCount(0)
, m_NumSearchTries(1)
, m_MaxNumSearchTries(1)
, m_DutyPctSearchStepSize(5)
, m_PulseUsSearchStepSize(1000)
, m_SMCountSearchStepSize(5)
, m_PulseAdjustmentStage(PulseAdjustmentStage::ADJUSTING_DUTY)
, m_DoneAdjustingSMCount(false)
, m_pDutyPctBinSearcher()
, m_pPulseUsBinSearcher()
, m_SMSelectionMode(static_cast<UINT32>(SMSelectionMode::RANDOM))
, m_NumDisabledSMs(0)
, m_NumDisabledGPCs(0)
, m_MemTmpRange(make_pair(std::numeric_limits<INT32>::max(),
        std::numeric_limits<INT32>::max()))
, m_GlobalMemTmpRange(make_pair(std::numeric_limits<INT32>::max(),
        std::numeric_limits<INT32>::max()))
, m_GpuTmpRange(make_pair(std::numeric_limits<INT32>::max(),
        std::numeric_limits<INT32>::max()))
, m_GlobalGpuTmpRange(make_pair(std::numeric_limits<INT32>::max(),
        std::numeric_limits<INT32>::max()))
{
}

void LwdaLinpack::AddKernelFromArch
(
    Lwca::Capability::Enum cap,
    const ShaderParams& params
)
{
    MASSERT(elementDataSize(params.typeA) == elementDataSize(params.typeB));
    m_GemmData.linpackFuncNames[cap] = params.name;
    m_GemmData.smParameters[cap] =
    {
        params.threadsPerCta,
        1,
        params.elementRowsPerCta,
        params.elementColsPerCta,
        params.elementRowsPerCta,
        params.elementColsPerCta,
        1,
        params.typeA,
        params.typeC,
        false,
        0,
        0,
        params.shiftFastA,
        params.multiplierSlowA,
        params.offsetSlowA,
        params.shiftFastB,
        params.multiplierSlowB,
        params.offsetSlowB,
        params.sharedMemSize,
        1,
        params.reLuAndBias
    };
}

RC LwdaLinpack::AllocMemory()
{
    RC rc;
    UINT64 bufferSize = 0;

    // The C matrix is first in the framebuffer
    m_CMatrixOffset  = 0;
    m_ABMatrixOffset = m_CMatrixOffset + CalcMatrixCMaxBytes();

    if (m_TestMode == MODE_WALKING)
    {
        size_t freeMem = 0;
        size_t totalMem = 0;
        CHECK_RC(GetLwdaInstance().GetMemInfo(GetBoundLwdaDevice(), &freeMem, &totalMem));

        // Bug 1975218
        bufferSize = ALIGN_DOWN(static_cast<UINT64>(freeMem * 0.95), MATRIX_ALIGN);
        m_TotalMem = totalMem;
        m_TotalFreeMem = bufferSize;
        VerbosePrintf("Total Memory on GPU:%12llu\n", m_TotalMem);
        VerbosePrintf("Free Memory To Test:%12llu\n", m_TotalFreeMem);

        m_OtherDataOffset = m_ABMatrixOffset +
                            CalcMatrixAMaxBytes() +
                            CalcMatrixBMaxBytes() +
                            CalcMatrixAmMaxBytes();

        MASSERT(m_CMatrixOffset % MATRIX_ALIGN == 0);
        MASSERT(m_ABMatrixOffset % MATRIX_ALIGN == 0);
        MASSERT(m_OtherDataOffset % MATRIX_ALIGN == 0);
    }
    else
    {
        bufferSize = NumElemsToBytes(m_KernParams.typeofInputElement, m_MaxInputBufferLen) +
                     CalcMatrixAmMaxBytes() +
                     NumElemsToBytes(m_KernParams.typeofOutputElement,  m_OutputBufferLen);

        m_OtherDataOffset = 0;
    }

    CHECK_RC(GetLwdaInstance().AllocDeviceMem(bufferSize, &m_Data));
    CHECK_RC(AllocOtherMemory());

    return rc;
}

RC LwdaLinpack::DumpMiscompares()
{
    if (!m_DumpMiscompares)
    {
        return RC::OK;
    }
    switch (NumElemsToBytes(m_KernParams.typeofOutputElement, 1))
    {
        case 1:
            return DumpMiscomparesTemplated<UINT08>();
        case 2:
            return DumpMiscomparesTemplated<UINT16>();
        case 4:
            return DumpMiscomparesTemplated<UINT32>();
        case 8:
            return DumpMiscomparesTemplated<UINT64>();
        default:
            MASSERT(!"Invalid output element size");
            return RC::SOFTWARE_ERROR;
    }
}

RC LwdaLinpack::DumpMatrices(bool isRefData)
{
    RC rc;
    if (!m_DumpMatrixData)
    {
        return RC::OK;
    }
    switch (m_KernParams.typeofInputElement)
    {
        case BF16_16F:
            CHECK_RC(DumpMatrixData<MatrixData::BF16>(
                m_Data, CalcMatrixAOffset(), CalcMatrixALen(), 'A'));
            CHECK_RC(DumpMatrixData<MatrixData::BF16>(
                m_Data, CalcMatrixBOffset(), CalcMatrixBLen(), 'B'));
            break;
        case R_16F:
            CHECK_RC(DumpMatrixData<MatrixData::FP16>(
                m_Data, CalcMatrixAOffset(), CalcMatrixALen(), 'A'));
            CHECK_RC(DumpMatrixData<MatrixData::FP16>(
                m_Data, CalcMatrixBOffset(), CalcMatrixBLen(), 'B'));
            break;
        case TF32_19F:
            CHECK_RC(DumpMatrixData<MatrixData::TF32>(m_Data, CalcMatrixAOffset(), CalcMatrixALen(), 'A'));
            CHECK_RC(DumpMatrixData<MatrixData::TF32>(m_Data, CalcMatrixBOffset(), CalcMatrixBLen(), 'B'));
            break;
        case R_32F:
            CHECK_RC(DumpMatrixData<float>(m_Data, CalcMatrixAOffset(), CalcMatrixALen(), 'A'));
            CHECK_RC(DumpMatrixData<float>(m_Data, CalcMatrixBOffset(), CalcMatrixBLen(), 'B'));
            break;
        case R_64F:
            CHECK_RC(DumpMatrixData<double>(m_Data, CalcMatrixAOffset(), CalcMatrixALen(), 'A'));
            CHECK_RC(DumpMatrixData<double>(m_Data, CalcMatrixBOffset(), CalcMatrixBLen(), 'B'));
            break;
        case R_1U:
        case R_4I:
            CHECK_RC(DumpMatrixData<UINT08>(
                m_Data,
                CalcMatrixAOffset(),
                NumElemsToBytes(m_KernParams.typeofInputElement, CalcMatrixALen()),
                'A'));
            CHECK_RC(DumpMatrixData<UINT08>(
                m_Data,
                CalcMatrixBOffset(),
                NumElemsToBytes(m_KernParams.typeofInputElement, CalcMatrixBLen()),
                'B'));
            break;
        case R_8I:
            CHECK_RC(DumpMatrixData<INT08>(m_Data, CalcMatrixAOffset(), CalcMatrixALen(), 'A'));
            CHECK_RC(DumpMatrixData<INT08>(m_Data, CalcMatrixBOffset(), CalcMatrixBLen(), 'A'));
            break;
        case R_32I:
            CHECK_RC(DumpMatrixData<INT32>(m_Data, CalcMatrixAOffset(), CalcMatrixALen(), 'A'));
            CHECK_RC(DumpMatrixData<INT32>(m_Data, CalcMatrixBOffset(), CalcMatrixBLen(), 'B'));
            break;
        default:
            MASSERT(!"Unsupported input data type");
            return RC::SOFTWARE_ERROR;
    }
    switch (m_KernParams.sizeofMetadataElement)
    {
        case 0:
            // no sparse data
            break;
        case sizeof(UINT16):
            CHECK_RC(DumpMatrixData<UINT16>(
                m_Data, CalcMatrixAmOffset(), CalcMatrixAmLen(), 'M'));
            break;
        default:
            MASSERT(!"Unsupported metadata data type");
            return RC::SOFTWARE_ERROR;
    }

    auto& cData = (isRefData && m_TestMode != TestMode::MODE_WALKING) ?
                  m_ReferenceData :
                  m_Data;
    const auto cOffset = (isRefData) ? CalcRefMatrixCOffset() : CalcMatrixCOffset();
    switch (m_KernParams.typeofOutputElement)
    {
        case BF16_16F:
            CHECK_RC(DumpMatrixData<MatrixData::BF16>(cData, cOffset, CalcMatrixCLen(), 'C'));
            break;
        case R_16F:
            CHECK_RC(DumpMatrixData<MatrixData::FP16>(cData, cOffset, CalcMatrixCLen(), 'C'));
            break;
        case TF32_19F:
            CHECK_RC(DumpMatrixData<MatrixData::TF32>(cData, cOffset, CalcMatrixCLen(), 'C'));
            break;
        case R_32F:
            CHECK_RC(DumpMatrixData<float>(cData, cOffset, CalcMatrixCLen(), 'C'));
            break;
        case R_64F:
            CHECK_RC(DumpMatrixData<double>(cData, cOffset, CalcMatrixCLen(), 'C'));
            break;
        case R_8I:
            CHECK_RC(DumpMatrixData<INT08>(cData, cOffset, CalcMatrixCLen(), 'C'));
            break;
        case R_32I:
            CHECK_RC(DumpMatrixData<INT32>(cData, cOffset, CalcMatrixCLen(), 'C'));
            break;
        default:
            MASSERT(!"Unsupport output data type");
            return RC::SOFTWARE_ERROR;
    }
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Check whether LwdaLinpack is supported
//!
bool LwdaLinpack::IsSupported()
{
    // Check if compute is supported at allocate
    if (!LwdaStreamTest::IsSupported())
    {
        return false;
    }
    Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();

    // This must be populated by the derived class during construction.
    MASSERT(!m_GemmData.smParameters.empty());

    return m_GemmData.smParameters.find(cap) != m_GemmData.smParameters.end();
}

//------------------------------------------------------------------------
//! \brief Stringify TestMode enum values
//!
const char* LwdaLinpack::GetTestModeStr(TestMode testMode)
{
    switch(testMode)
    {
        case MODE_NORMAL:
            return "Normal";

        case MODE_SHMOO:
            return "Shmoo";

        case MODE_WALKING:
            return "Walking Linpack";

        case MODE_PULSE:
            return "Pulse";

        default:
            MASSERT(!"Unhandled TestMode!");
            return "";
    }
}

//------------------------------------------------------------------------
//! \brief Stringify MNKMode enum values
//!
const char* LwdaLinpack::GetMNKModeStr(MNKMode mnkMode)
{
    switch (mnkMode)
    {
        case MNK_STRICT:
            return "Strict";

        case MNK_ALIGN:
            return "Align";

        case MNK_PERMIT:
            return "Permit";

        case MNK_WAVE:
            return "Wave";

        default:
            MASSERT(!"Unhandled MNKMode!");
    }

    return "";
}

//------------------------------------------------------------------------
//! \brief Stringify TargetingMode enum values
//!
const char* LwdaLinpack::GetTargetingModeStr(TargetingMode targetingMode)
{
    switch (targetingMode)
    {
        case TargetingMode::NONE:
            return "None";

        case TargetingMode::FMAX_PULSE:
            return "Pulse-based";

        case TargetingMode::FMAX_SM:
            return "SM-based";

        default:
            MASSERT(!"Unhandled TargetingMode!");
            return "";
    }
}

//! \brief Stringify SMSelectionMode enum values
//!
const char* LwdaLinpack::GetSMSelectionModeStr(SMSelectionMode selectionMode)
{
    switch (selectionMode)
    {
        case SMSelectionMode::RANDOM:
            return "Random";

        case SMSelectionMode::RANDOM_GPC:
            return "Random GPC";

        default:
            MASSERT(!"Unhandled SMSelectionMode!");
            return "";
    }
}

//-----------------------------------------------------------------------
//! \brief Validate and potentially scale MNK variables according to arch kernel requirements
//!
RC LwdaLinpack::ValidateMNKParameters(UINT32 smCount)
{

    StickyRC returnRc;
    switch (m_MNKMode)
    {
        case MNK_STRICT:
            // Check that the combination of M and N are optimal for performance
            if ((m_Msize * m_Nsize) % (m_KernParams.mSizeMultiple *
                                       m_KernParams.nSizeMultiple *
                                       smCount) != 0)
            {
                Printf(Tee::PriError, "M and N are not optimal for the # of SMs on this chip!\n");
                returnRc = RC::BAD_PARAMETER;
            }
            // fallthrough

        case MNK_PERMIT:
            // Check that M/N/K sizes are valid
            if (m_Msize % m_KernParams.mSizeMultiple != 0)
            {
                Printf(Tee::PriError, "M must be a multiple of %u for this kernel/arch!\n",
                       m_KernParams.mSizeMultiple);
                returnRc = RC::BAD_PARAMETER;
            }

            if (m_Nsize % m_KernParams.nSizeMultiple != 0)
            {
                Printf(Tee::PriError, "N must be a multiple of %u for this kernel/arch!\n",
                       m_KernParams.nSizeMultiple);
                returnRc = RC::BAD_PARAMETER;
            }

            for (UINT32 k : m_Kset)
            {
                if (k % m_KernParams.kSizeMultiple != 0)
                {
                    Printf(Tee::PriError, "K must be a multiple of %u for this kernel/arch!\n",
                           m_KernParams.kSizeMultiple);
                    returnRc = RC::BAD_PARAMETER;
                }
            }
            break;

        case MNK_ALIGN:
            // Round sizes to the least optimal size, rounding Msize first
            m_Msize = max(m_Msize, m_KernParams.mSizeMultiple * smCount);
            m_Msize = ALIGN_DOWN(m_Msize, m_KernParams.mSizeMultiple * smCount);

            m_Nsize = max(m_Nsize, m_KernParams.nSizeMultiple);
            m_Nsize = ALIGN_DOWN(m_Nsize, m_KernParams.nSizeMultiple);

            for (UINT32 i = 0; i < m_Kset.size(); i++)
            {
                m_Kset[i] = max(m_Kset[i], m_KernParams.kSizeMultiple);
                m_Kset[i] = ALIGN_DOWN(m_Kset[i], m_KernParams.kSizeMultiple);
            }
            break;

        case MNK_WAVE:
            // This mode does not take M/N values from user.
            for (UINT32 k : m_Kset)
            {
                if (k % m_KernParams.kSizeMultiple != 0)
                {
                    Printf(Tee::PriError, "K must be a multiple of %u for this kernel/arch!\n",
                           m_KernParams.kSizeMultiple);
                    returnRc = RC::BAD_PARAMETER;
                }
            }
            break;

        default:
            Printf(Tee::PriError, "Unknown MNKMode mode: %u!\n", m_MNKMode);
            returnRc = RC::BAD_PARAMETER;
    }

    // Set Ksize to max value due to matrix allocations which happen in SetupBaseParams
    for (UINT32 i = 0; i < m_Kset.size(); i++)
    {
        m_Ksize = max(m_Ksize,m_Kset[i]);
    }

    return returnRc;
}

//-----------------------------------------------------------------------------
//! \brief Callwlate how much memory is needed for the C matrix
//!
UINT64 LwdaLinpack::CalcMatrixCMaxBytes() const
{
    UINT64 matrixSize = 0;
    matrixSize = static_cast<UINT64>(m_Msize) * m_Nsize;

    return NumElemsToBytes(m_KernParams.typeofOutputElement, matrixSize * m_CMatrixScale);
}

//-----------------------------------------------------------------------------
//! \brief Callwlate how much memory is needed for the A matrix
//!
UINT64 LwdaLinpack::CalcMatrixAMaxBytes() const
{
    UINT64 matrixSize = 0;
    if (m_TestMode == MODE_PULSE)
    {
        matrixSize = static_cast<UINT64>(m_Msize) * m_MaxKsizeForPulse;
    }
    else
    {
        matrixSize = static_cast<UINT64>(m_Msize) * m_Ksize;
    }

    if (m_KernParams.isSparse)
    {
        matrixSize /= 2;
    }

    return NumElemsToBytes(m_KernParams.typeofInputElement, matrixSize);
}

//-----------------------------------------------------------------------------
//! \brief Callwlate how much memory is needed for the Am matrix
//!
UINT64 LwdaLinpack::CalcMatrixAmMaxBytes() const
{
    // No need for metadata for non-sparse kernels
    if (!m_KernParams.isSparse)
    {
        return 0;
    }

    // A-Metadata size is scaled from A matrix size.
    UINT64 matrixLen = NumBytesToElems(m_KernParams.typeofInputElement, CalcMatrixAMaxBytes());

    // Scale down by the ratio to get max element count for Am
    matrixLen /= m_KernParams.metadataRatio;

    return matrixLen * m_KernParams.sizeofMetadataElement;
}

//-----------------------------------------------------------------------------
//! \brief Callwlate how much memory is needed for the B matrix
//!
UINT64 LwdaLinpack::CalcMatrixBMaxBytes() const
{
    UINT64 matrixSize = 0;
    if (m_TestMode == MODE_PULSE)
    {
        matrixSize = static_cast<UINT64>(m_MaxKsizeForPulse) * m_Nsize;
    }
    else
    {
        matrixSize = static_cast<UINT64>(m_Ksize) * m_Nsize;
    }
    return NumElemsToBytes(m_KernParams.typeofInputElement, matrixSize);
}

//-----------------------------------------------------------------------------
//! \brief Callwlate how much memory is needed for the reference/CRC data
//!
UINT64 LwdaLinpack::CalcOtherMemBytes() const
{
    if (UseReferenceData())
    {
        return CalcMatrixCMaxBytes() + ERROR_LOG_SIZE_BYTES + sizeof(UINT64);
    }
    else
    {
        return 3 * CrcBufSize() + Crc::TableSizeBytes();
    }
}

//-----------------------------------------------------------------------------
//! \brief Callwlate the number of arithmetic operations given the kernel launch counts
//!
UINT64 LwdaLinpack::CalcOpCount(const UINT32 mSize,
                                const UINT32 nSize,
                                const UINT32 kSize,
                                const bool initialize) const
{
    const UINT64 opsPerMainLoop = static_cast<UINT64>(mSize) * nSize * (kSize * 2 - 1);
    const UINT64 opsPerEpilogue = static_cast<UINT64>(mSize) * nSize * 3;
    if (initialize)
    {
        // Init kernel launches only need the main GEMM loop
        return opsPerMainLoop;
    }
    else
    {
        // Main kernel launches have an eplogue to scale Alpha/Beta
        return opsPerMainLoop + opsPerEpilogue;
    }
}

//-----------------------------------------------------------------------------
//! \brief Callwlate how much memory to allocate for A,B,C matrices altogether
//!
RC LwdaLinpack::GetMemAllocInfo()
{
    // callwlate total buffer size required for 3 matrices
    // buffer size for matrix A = m_Msize*m_Ksize
    // buffer size for matrix B = m_Ksize*m_Nsize
    // buffer size for matrix C = m_Msize*m_Nsize
    // Note that this does not include A-metadata matrix
    m_MaxInputBufferLen = NumBytesToElems(m_KernParams.typeofInputElement,
                                          CalcMatrixAMaxBytes() + CalcMatrixBMaxBytes());
    m_OutputBufferLen = NumBytesToElems(m_KernParams.typeofOutputElement,
                                        CalcMatrixCMaxBytes());

    if (!m_MaxInputBufferLen || !m_OutputBufferLen || !m_Msize || !m_Nsize || !m_Ksize)
    {
        Printf(Tee::PriError, "No Buffer allocated, Set valid Matrix size!\n");
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

//---------------------------------------------------------------------
//! \brief Print properties of the JS object for the class
//!
void LwdaLinpack::PrintJsProperties(Tee::Priority pri)
{
    LwdaStreamTest::PrintJsProperties(pri);
    Printf(pri, "   CheckLoops          %u\n",
           GetTestConfiguration()->Loops() < m_CheckLoops ?
           GetTestConfiguration()->Loops() :
           m_CheckLoops);
    if (m_BatchDelayUs)
    {
        Printf(pri, "   BatchDelay(uSec):   %f\n", m_BatchDelayUs);
    }
    if (m_BatchUs)
    {
        Printf(pri, "   BatchUs:            %llu\n", static_cast<UINT64>(m_BatchUs));
    }
    Printf(pri, "   RuntimeMs:          %u\n", m_RuntimeMs);
    Printf(pri, "   KeepRunning:        %s\n", m_KeepRunning?"true":"false");
    Printf(pri, "   Test type:          %s\n", GetTestModeStr(static_cast<TestMode>(m_TestMode)));
    Printf(pri, "   MNK Align mode:     %s\n", GetMNKModeStr(static_cast<MNKMode>(m_MNKMode)));
    if (m_MNKMode == MNK_WAVE)
    {
        Printf(pri, "   NumWaves:           %d\n", m_NumWaves);
        Printf(pri, "   NumFracs:           %d\n", m_NumFracs);
    }
    Printf(pri, "   Max data dump:      %u\n", m_MaxDataDump);
    Printf(pri, "   NumNewMatrices:     %u\n", m_NumNewMatrices);
    if (m_TestMode == MODE_SHMOO)
    {
        Printf(pri, "   Kstart:             %u\n", m_Kstart);
        Printf(pri, "   Kstep:              %u\n", m_Kstep);
    }
    else if (m_TestMode == MODE_PULSE)
    {
        if (m_UsePulseUsRange)
        {
            Printf(pri, "   MinPulseUs: %f\n", m_MinPulseUs);
            Printf(pri, "   MaxPulseUs: %f\n", m_MaxPulseUs);
        }
        if (m_ShmooPulse)
        {
            Printf(pri, "   Kstart:             %u\n", m_Kstart);
            Printf(pri, "   PulseStepUs:        %llu\n", static_cast<UINT64>(m_PulseStepUs));
        }
        else if (m_RandomPulse)
        {
            Printf(pri, "   MaxLaunchDelayUs: %llu\n", static_cast<UINT64>(m_MaxLaunchDelayUs));
        }
        else
        {
            Printf(pri, "   PulseUs:            %llu\n", static_cast<UINT64>(m_PulseUs));
        }
        Printf(pri, "   Max Ksize for pulse: %u\n", m_MaxKsizeForPulse);
        Printf(pri, "   Workload duty cycle: %llu\n", static_cast<UINT64>(m_DutyPct));
    }

    Printf(pri, "   Dump Miscompares:   %s\n", m_DumpMiscompares?"true":"false");
    Printf(pri, "   Dump Matrices:      %s\n", m_DumpMatrixData?"true":"false");
    Printf(pri, "   GflopsLowerBound:   %f\n", m_GflopsLowerBound);
    Printf(pri, "   GflopsUpperBound:   %f\n", m_GflopsUpperBound);
    Printf(pri, "   Verify Results:     %s\n", m_VerifyResults?"true":"false");
    Printf(pri, "   Naive Init:         %s\n", m_NaiveInit?"true":"false");
    Printf(pri, "   Alpha:              %f\n", m_Alpha);
    Printf(pri, "   Beta:               %f\n", m_Beta);
    Printf(pri, "   Synchronous Mode:   %s\n", m_SynchronousMode?"true":"false");
    Printf(pri, "   CPU threads:        %u\n", m_CpuThreads);
    Printf(pri, "   LaunchDelay(uSec):  %f\n", m_LaunchDelayUs);
    Printf(pri, "   UseCrcToVerify:     %s\n", m_UseCrcToVerify ? "true": "false");
    Printf(pri, "   SkipAlphaBetaCheck: %s\n", m_SkipAlphaBetaCheck?"true":"false");
    Printf(pri, "   CMatrixScale:       %u\n", m_CMatrixScale);
    Printf(pri, "   CtaSwizzle:         %s\n", m_CtaSwizzle ? "true" : "false");
    if (m_TestMode == MODE_WALKING)
    {
        Printf(pri, "   Number of Walks :   %u\n", m_NumWalks);
        Printf(pri, "   Walk Step Scale:    %f\n", m_WalkScale);
    }
    Printf(pri, "   Mtx A fill data type: %s\n", m_UseConstantDataA ? "Constant" : "Random");
    if (m_UseConstantDataA)
    {
        Printf(pri, "   Mtx A fill constant:  %f\n", m_MtxADataConstant);
    }
    else
    {
        Printf(pri, "   Mtx A fill mean:      %f\n", m_MtxDataMean);
        Printf(pri, "   Mtx A fill std dev:   %f\n", m_MtxDataSD);
    }
    Printf(pri, "   Mtx B fill data type: %s\n", m_UseConstantDataB ? "Constant" : "Random");
    if (m_UseConstantDataB)
    {
        Printf(pri, "   Mtx B fill constant:  %f\n", m_MtxBDataConstant);
    }
    else
    {
        Printf(pri, "   Mtx B fill mean:      %f\n", m_MtxDataMean);
        Printf(pri, "   Mtx B fill std dev:   %f\n", m_MtxDataSD);
    }
    Printf(pri, "   Non-Zero Pct:         %f\n", m_NonZeroPct);
    Printf(pri, "   Targeting mode:       %s\n",
           GetTargetingModeStr(static_cast<TargetingMode>(m_TargetingMode)));
    if (m_TargetingFmax)
    {
        Printf(pri, "   Sample threshhold:    [%u / %u]\n", m_SuccessThreshholdCount, m_PllSampleCount);
        Printf(pri, "   Max number of search tries: %u\n", m_MaxNumSearchTries);
    }
    if (static_cast<TargetingMode>(m_TargetingMode) == TargetingMode::FMAX_PULSE)
    {
        Printf(pri, "   DutyPct step size:    %u\n", m_DutyPctSearchStepSize);
        Printf(pri, "   PulseUs step size:    %u\n", m_PulseUsSearchStepSize);
    }
    else if (static_cast<TargetingMode>(m_TargetingMode) == TargetingMode::FMAX_SM)
    {
        Printf(pri, "   SMCount step size:    %u\n", m_SMCountSearchStepSize);
    }
    if (m_OclwpySMs)
    {
        Printf(pri, "   SM selection mode:    %s\n",
               GetSMSelectionModeStr(static_cast<SMSelectionMode>(m_SMSelectionMode)));
        Printf(pri, "   Num disabled SMs:     %u\n", m_NumDisabledSMs);
        Printf(pri, "   Num disabled GPCs:    %u\n", m_NumDisabledGPCs);
    }
}

//------------------------------------------------------------------------------
//! \brief Return name of GEMM function
//!
const char* LwdaLinpack::GemmFunctionName()
{
    return m_GemmData.linpackFuncNames[m_ComputeCap];
}

//------------------------------------------------------------------------------
//! \brief Allocate buffers required for SMID dump
//! This map gets populated by the running Linpack kernel
//!
RC LwdaLinpack::AllocateSmidBuffers()
{
    RC rc;
    const UINT64 bufferSize = sizeof(UINT16) * m_GridDimX * m_GridDimY;
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(bufferSize, &m_SmidMap));
    CHECK_RC(GetLwdaInstance().AllocHostMem(bufferSize, &m_HostSmidMap));
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Copy from device to host smid buffer
RC LwdaLinpack::CopySmidMapToHost(Lwca::HostMemory& hostSmidMap)
{
    return m_SmidMap.Get(hostSmidMap.GetPtr(), hostSmidMap.GetSize());
}

//------------------------------------------------------------------------------
//! \brief Initialize the default Alpha/Beta values if Alpha and Beta were not specified
RC LwdaLinpack::InitAlphaBeta()
{
    if (std::isinf(m_Alpha))
    {
        m_Alpha = m_DefaultAlpha;
    }
    if (std::isinf(m_Beta))
    {
        m_Beta = m_DefaultBeta;
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
//! \brief Check that Alpha and Beta are valid (unless the check is overridden)
//!        If Alpha + Beta don't sum up to 1.0 test results will miscompare
//!
RC LwdaLinpack::ValidateAlphaBeta()
{
    if ((m_Alpha + m_Beta != 1.0))
    {
        Printf(Tee::PriError, "Alpha and Beta values should sum up to 1.0\n");
        return RC::BAD_PARAMETER;
    }
    return RC::OK;
}

//---------------------------------------------------------------------
//! \brief Initialize M,N,K variables if they haven't been set
//!
RC LwdaLinpack::InitializeMNKParameters()
{
    RC rc;

    // In Wave mode, the test initializes M/N differently.
    if (m_MNKMode == MNK_WAVE)
    {
        CHECK_RC(InitializeMNParameterWave());
    }
    else
    {
        CHECK_RC(InitializeMParameterNormal());
        CHECK_RC(InitializeNParameterNormal());
    }
    
    // Check for maximum C matrix size
    if (CalcMatrixCLen() > m_MaxMatrixNumElements)
    {
        Printf(Tee::PriError,
               "The selected kernel is lwrrently limited to Matrices with fewer than %llu elements.\n"
               "Consider decreasing Msize or Nsize\n",
               m_MaxMatrixNumElements);
        return RC::BAD_PARAMETER;
    }

    // Only initialize Ksize if Kset isn't in use
    if (m_Kset.empty())
    {
        CHECK_RC(InitializeKParameterNormal());
    }
    return rc;
}

RC LwdaLinpack::InitializeMParameterNormal()
{
    // By default the number of grid "rows" is the number of SMs
    if (!m_Msize)
    {
        m_Msize = GetBoundLwdaDevice().GetShaderCount() * m_KernParams.mSizeMultiple;
    }
    return OK;
}

RC LwdaLinpack::InitializeNParameterNormal()
{
    // By default the number of grid "columns" is the number of blocks that can fit on an SM
    if (!m_Nsize)
    {
        m_Nsize = m_MaxBlocksPerSM * m_KernParams.nSizeMultiple;
    }
    return OK;
}

RC LwdaLinpack::InitializeKParameterNormal()
{
    m_Kset.clear();

    // Some kernels lwrrently limit the size of the matrices
    using KTYPE = decltype(m_Ksize);
    const KTYPE maxKForMatrixA =
        static_cast<KTYPE>(std::min<UINT64>(m_MaxMatrixNumElements / m_Msize,
                                            std::numeric_limits<KTYPE>::max()));
    const KTYPE maxKForMatrixB =
        static_cast<KTYPE>(std::min<UINT64>(m_MaxMatrixNumElements / m_Nsize,
                                            std::numeric_limits<KTYPE>::max()));
    const KTYPE kernelKsizeLimit = ALIGN_DOWN(std::min(maxKForMatrixA, maxKForMatrixB),
                                              m_KernParams.kSizeMultiple);
    if (m_Ksize && m_Ksize > kernelKsizeLimit)
    {
        Printf(Tee::PriError,
               "The selected kernel is lwrrently limited to Matrices with fewer than %llu elements.\n"
               "Consider decreasing Ksize below %u.\n",
               m_MaxMatrixNumElements, kernelKsizeLimit);
        return RC::BAD_PARAMETER;
    }

    if (!m_Ksize)
    {
        // Choose default Ksize such that the default time spent while exelwting
        // a kernel is similar, for similar M,N

        // Using FFMA as the base for scaling
        double archEffInstr = GetBoundGpuSubdevice()->GetArchEfficiency(m_Instr);
        double archEffFfma  = GetBoundGpuSubdevice()->GetArchEfficiency(Instr::FFMA);
        double archEffFactor = 1.0f;
        if (archEffInstr != 0.0f && archEffFfma != 0.0f)
        {
            archEffFactor = archEffInstr / archEffFfma;
        }

        double issueRateFactor = 1.0f;
        if (!Platform::IsVirtFunMode())
        {
            issueRateFactor = GetBoundGpuSubdevice()->GetIssueRate(m_Instr);
        }

        const UINT32 baseKsize = 8192;
        // Setting a minimum so that the matrix doesn't fit into the LTC
        // (Actual number chosen is arbitary)
        const UINT32 minKsize  = 1024;
        // Setting a maximum so that we don't take up too much memory
        // For now, allow max memory oclwpation to be 4x of FFMA
        const UINT32 inputSizeFactor = static_cast<UINT32>
                                       (128 / NumElemsToBytes(m_KernParams.typeofInputElement, 8));
        const UINT32 maxTargetKsize = baseKsize * inputSizeFactor;

        double kScale = archEffFactor * issueRateFactor;
        const UINT32 scaledKsize = ALIGN_UP(static_cast<UINT32>(baseKsize * kScale),
                                            m_KernParams.kSizeMultiple);

        m_Ksize = std::min(std::min(std::max(minKsize, scaledKsize), maxTargetKsize), kernelKsizeLimit);
    }
    m_Kset.push_back(m_Ksize);

    if (!m_MaxKsizeForPulse)
    {
        m_MaxKsizeForPulse = m_Ksize;
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
// This mode sets MN values to saturate the SMs in a lwca wave.
// The hard requirements are that
// 1. M and N are multiples of mSizeMultiple and nSizeMultiple respectively
// 2. Saturate max available threads
// 3. Total threads scales with NumWaves, if set
// The soft requirement is that
// 1. N is at least s_MinNSize (1024)
//
RC LwdaLinpack::InitializeMNParameterWave()
{
    if (m_Nsize || m_Msize)
    {
        Printf(Tee::PriError, "MNK_Wave Mode: Msize/Nsize cannot be set when running with "
                               "Wave mode.\n");
        return RC::BAD_PARAMETER;
    }

    // Start with the defaults that match Normal mode. This is one wave.
    m_Msize = GetBoundLwdaDevice().GetShaderCount() * m_KernParams.mSizeMultiple;
    m_Nsize = m_MaxBlocksPerSM * m_KernParams.nSizeMultiple;

    UINT32 waveCount = m_NumWaves;
    const UINT32 waveSize = m_Msize * m_Nsize;
    const UINT32 fracSize = m_NumFracs * m_KernParams.mSizeMultiple;

    // Step 1: Apply requested wave count
    if (m_NumWaves != 0)
    {
        // Apply wave multiplier to N
        m_Nsize *= waveCount;
    }
    else
    {
        // Default starts at 1
        waveCount = 1;
    }

    // Step 2: Check if the scaling is possible.
    if (m_Nsize < s_MinNSize)
    {
        if (m_NumWaves == 0)
        {
            UINT32 nScale = (s_MinNSize + m_Nsize - 1) / m_Nsize;
            waveCount *= nScale;
            m_Nsize *= nScale;

            VerbosePrintf("MNK_Wave Mode: \t Setting wave count = %d with wave size = %d\n",
                            waveCount, waveSize);
        }
        else
        {
            Printf(Tee::PriWarn, "MNK_Wave Mode: M and N are not optimal for perf, "
                                 "consider updating NumWaves\n");
        }
    }

    // Print max possible fractional units. This can only be done after M and N are set
    VerbosePrintf("MNK_Wave Mode: \t This setting supports up to %d numFracs\n",
                    (m_Msize / waveCount / m_KernParams.mSizeMultiple));

    // Step 4: add fractional units after M and N values have been adjusted
    m_Msize += fracSize;

    // Print the final wave count and actual fractions
    UINT64 totalSize = static_cast<UINT64>(m_Msize) * m_Nsize;
    float fraction = (static_cast<float>(totalSize)) / waveSize;

    // Print a warning if we are not saturating SMs
    if (totalSize % waveSize != 0)
    {
        VerbosePrintf("MNK_Wave Mode: \t "
                        "WARNING - Fractional waves do not saturate all SMs on the GPU!\n"
                        "\t\t This is not optimal for perf and is for TESTING ONLY.\n");
    }

    VerbosePrintf("MNK_Wave Mode: \t Setting %.2f waves for this run\n", fraction);

    // Sanity check without fractional part
    totalSize = static_cast<UINT64>(m_Msize - fracSize) * m_Nsize;

    // check we are a multiple of a wave
    // check that we scaled the correct number of waves
    if (((totalSize % waveSize) != 0) ||
        ((totalSize) / waveSize) != waveCount)
    {
        MASSERT(!"Error in scaling waves!\n");
        return RC::SOFTWARE_ERROR;
    }

    // check that MN are multiples of row/col
    if ((m_Msize % m_KernParams.mSizeMultiple != 0) ||
        (m_Nsize % m_KernParams.nSizeMultiple != 0))
    {
        MASSERT(!"Error callwlating M/N!\n");
        return RC::SOFTWARE_ERROR;
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
//! \brief Initialize properties of the test from Javascript
//!
RC LwdaLinpack::InitFromJs()
{
    RC rc;
    CHECK_RC(LwdaStreamTest::InitFromJs());

    // Get compute cap and kernel parameters
    m_ComputeCap = GetBoundLwdaDevice().GetCapability();
    if (m_ComputeCap >= Lwca::Capability::SM_UNKNOWN)
    {
        return RC::SOFTWARE_ERROR;
    }
    m_KernParams = m_GemmData.smParameters[m_ComputeCap];

    JavaScriptPtr pJs;
    rc = pJs->GetProperty(GetJSObject(), "Kset", &m_Kset);
    if (rc != OK && rc != RC::ILWALID_OBJECT_PROPERTY)
    {
        return rc;
    }
    rc.Clear();

    vector<INT32> memTempRange;
    rc = pJs->GetProperty(GetJSObject(), "MemTmpRange", &memTempRange);
    if (rc != OK && rc != RC::ILWALID_OBJECT_PROPERTY)
    {
        return rc;
    }
    rc.Clear();
    if (memTempRange.size() != 0 && memTempRange.size() != 2)
    {
        Printf(Tee::PriError, "MemTmpRange parameter should contain only [min, max].\n");
        return RC::BAD_PARAMETER;
    }
    else if (memTempRange.size() == 2)
    {
        if (memTempRange[1] < memTempRange[0])
        {
            Printf(Tee::PriError, "MemTmpRange: Temperature Max should be "
                  "greater than Temperature Min.\n");
            return RC::BAD_PARAMETER;
        }
        m_MemTmpRange = make_pair(memTempRange[0], memTempRange[1]);
    }

    vector<INT32> GpuTempRange;
    rc = pJs->GetProperty(GetJSObject(), "GpuTmpRange", &GpuTempRange);
    if (rc != OK && rc != RC::ILWALID_OBJECT_PROPERTY)
    {
        return rc;
    }
    rc.Clear();
    if (GpuTempRange.size() != 0 && GpuTempRange.size() != 2)
    {
        Printf(Tee::PriError, "GpuTempRange parameter should contain only [min, max].\n");
        return RC::BAD_PARAMETER;
    }
    else if (GpuTempRange.size() == 2)
    {
        if (GpuTempRange[1] < GpuTempRange[0])
        {
            Printf(Tee::PriError, "GpuTempRange: Temperature Max should be "
                  "greater than Temperature Min.\n");
            return RC::BAD_PARAMETER;
        }
        m_GpuTmpRange = make_pair(GpuTempRange[0], GpuTempRange[1]);
    }

    if (m_Ksize && !m_Kset.empty())
    {
        Printf(Tee::PriError, "Ksize and Kset cannot both be set!\n");
        return RC::BAD_PARAMETER;
    }

    // NaiveInit uses the reference matrix
    if (m_NaiveInit && m_UseCrcToVerify)
    {
        Printf(Tee::PriError, "NaiveInit mode requires UseCrcToVerify=false\n");
        return RC::BAD_PARAMETER;
    }

    if (m_TestMode == MODE_PULSE)
    {
        if (m_RandomPulse && m_ShmooPulse)
        {
            Printf(Tee::PriError, "RandomPulse and ShmooPulse are mutually exclusive.\n");
            return RC::BAD_PARAMETER;
        }
        if (m_RandomPulse)
        {
            // We will randomize pulseUs and delayUs
            if (m_PulseUs || m_LaunchDelayUs)
            {
                Printf(Tee::PriError, "PulseUs and LaunchDelayUs cannot be set with RandomPulse.\n");
                return RC::BAD_PARAMETER;
            }
        }
        else if (m_ShmooPulse)
        {
            // When shmoo, we will vary PulseUs and CheckLoops
            if (m_PulseUs || m_BatchUs)
            {
                Printf(Tee::PriError, "PulseUs and BatchUs cannot be set with ShmooPulse.\n");
                return RC::BAD_PARAMETER;
            }
        }
        else
        {
            if (m_DutyPct > 100 || m_DutyPct < 0)
            {
                Printf(Tee::PriError, "DutyPct must be between 0 and 100.\n");
                return RC::BAD_PARAMETER;
            }
            else if (m_DutyPct && m_LaunchDelayUs)
            {
                Printf(Tee::PriError, "DutyPct and LaunchDelayUs are mutually exclusive.\n");
                return RC::BAD_PARAMETER;
            }
            else if (!m_PulseUs && !m_LaunchDelayUs && !m_DutyPct)
            {
                // default set the duty cycle to 40 if no pulse args provided
                m_DutyPct = 30;
            }
        }
    }
    else
    {
        if (m_PulseUs)
        {
            Printf(Tee::PriError, "PulseUs can only be used with TestMode = 4 (PULSE)\n");
            return RC::BAD_PARAMETER;
        }
        if (m_RandomPulse)
        {
            Printf(Tee::PriError, "RandomPulse can only be used with TestMode = 4 (PULSE)\n");
            return RC::BAD_PARAMETER;
        }
        if (m_ShmooPulse)
        {
            Printf(Tee::PriError, "ShmooPulse can only be used with TestMode = 4 (PULSE)\n");
            return RC::BAD_PARAMETER;
        }
    }

    // Verify that TestMode is valid
    switch (m_TestMode)
    {
        case MODE_NORMAL:
        case MODE_SHMOO:
        case MODE_WALKING:
        case MODE_PULSE:
            break;
        default:
            Printf(Tee::PriError, "Unknown/unsupported TestMode: %u!\n", m_TestMode);
            return RC::BAD_PARAMETER;
    }
    
    if (m_UseConstantData && (m_UseConstantDataA || m_UseConstantDataB))
    {
        Printf(Tee::PriError, "Cannot use argument UseConstantData with "
                              "UseConstantDataA/UseConstantDataB\n");
        return RC::BAD_PARAMETER;
    }

    // If user specified UseConstantData = true, but set MtxADataConstant
    // or MtxBDataConstant to some value, override them to be MtxDataConstant
    if (m_UseConstantData)
    {
        m_UseConstantDataA = true;
        m_UseConstantDataB = true;
        m_MtxADataConstant = m_MtxDataConstant;
        m_MtxBDataConstant = m_MtxDataConstant;
    }

    // Verify that TargetingMode is valid
    if (m_TargetingMode >= static_cast<UINT32>(TargetingMode::NUM_MODES))
    {
        Printf(Tee::PriError,
               "Unknown/unsupported targeting mode: %u!\n",
               m_TargetingMode);
        return RC::BAD_PARAMETER;
    }

    m_TargetingFmax =
        static_cast<TargetingMode>(m_TargetingMode) == TargetingMode::FMAX_PULSE ||
        static_cast<TargetingMode>(m_TargetingMode) == TargetingMode::FMAX_SM;

    if (m_TargetingFmax)
    {
        if (!m_PllSampleCount && m_SuccessThreshholdCount)
        {
            Printf(Tee::PriError, "Please specify PllSampleCount\n");
        }
        if (!m_PllSampleCount)
        {
            m_PllSampleCount = DEFAULT_PLL_SAMPLE_COUNT;
        }
        if (!m_SuccessThreshholdCount)
        {
            // Default threshhold count is 80% of PllSampleCount
            m_SuccessThreshholdCount = m_PllSampleCount * 4 / 5;
        }
        if (m_SuccessThreshholdCount > m_PllSampleCount)
        {
            Printf
            (
                Tee::PriError,
                "SuccessThreshholdCount <%u> cannot be greater than PllSampleCount <%u>\n",
                m_SuccessThreshholdCount,
                m_PllSampleCount
            );
            return RC::BAD_PARAMETER;
        }
    }

    if (static_cast<TargetingMode>(m_TargetingMode) == TargetingMode::FMAX_PULSE)
    {
        if (m_TestMode != MODE_PULSE)
        {
            Printf
            (
                Tee::PriError,
                "To enable pulse-based targeting mode, TestMode must be set "
                "to %u (Pulse Mode)\n",
                static_cast<UINT32>(MODE_PULSE)
            );
            return RC::BAD_PARAMETER;
        }
        if (m_LaunchDelayUs)
        {
            Printf
            (
                Tee::PriError,
                "Cannot specify LaunchDelayUs when enabling pulse-based "
                "targeting mode!\n"
            );
            return RC::BAD_PARAMETER;
        }
        if (m_DutyPctSearchStepSize == 0 || m_PulseUsSearchStepSize == 0)
        {
            Printf
            (
                Tee::PriError,
                "Search step size cannot be 0! "
                "DutyPctSearchStepSize: %u, PulseUsSearchStepSize: %u\n",
                m_DutyPctSearchStepSize,
                m_PulseUsSearchStepSize
            );
            return RC::BAD_PARAMETER;
        }
        // If PulseUs or Ksize is unspecified, we'll:
        //     - start PulseUs at 1000us
        //     - do binary search on DutyPct to see which value allows
        //       GEMM workload to run a max gpcclk frequency
        //     - do binary search on PulseUs between min (1000us) and max (7000us)
        if (!m_PulseUs)
        {
            m_CanAdjustPulseWidth = true;
            m_PulseUs = BIN_SEARCH_MIN_PULSE_US;
        }
    }
    else if (static_cast<TargetingMode>(m_TargetingMode) == TargetingMode::FMAX_SM)
    {
        const UINT32 smCount = GetBoundLwdaDevice().GetShaderCount();
        if (m_SMCountSearchStepSize > smCount)
        {
            Printf(Tee::PriError,
                   "SM count search step size (%u) cannot be greater "
                   "than number of SMs (%u)!\n",
                   m_SMCountSearchStepSize,
                   smCount);
            return RC::BAD_PARAMETER;
        }
        if (m_NumDisabledSMs || m_NumDisabledGPCs)
        {
            Printf(Tee::PriError,
                   "Cannot set arguments NumDisabledSMs or NumDisabledGPCs "
                   "when using Fmax SM-based targeting mode!\n");
            return RC::BAD_PARAMETER;
        }
    }

    if (m_MinPulseUs || m_MaxPulseUs)
    {
        if (!m_ShmooPulse && !m_RandomPulse)
        {
            Printf(Tee::PriError,
                   "MinPulseUs and MaxPulseUs can only be set "
                   "with ShmooPulse = true or RandomPulse = true!\n");
            return RC::BAD_PARAMETER;
        }
        if (m_MinPulseUs > m_MaxPulseUs)
        {
            Printf(Tee::PriError,
                   "MinPulseUs (%f) cannot be greater than MaxPulseUs(%f)!\n",
                   m_MinPulseUs,
                   m_MaxPulseUs);
            return RC::BAD_PARAMETER;
        }
        if (m_MinPulseUs < 0 || m_MaxPulseUs < 0)
        {
            Printf(Tee::PriError,
                   "MinPulseUs (%f) and MaxPulseUs (%f) cannot be negative!\n",
                   m_MinPulseUs,
                   m_MaxPulseUs);
            return RC::BAD_PARAMETER;
        }
        if (!m_PulseStepUs)
        {
            m_PulseStepUs = (m_MaxPulseUs - m_MinPulseUs) / 10;
        }
        m_UsePulseUsRange = true;
    }

    if (m_NumDisabledSMs && m_NumDisabledGPCs)
    {
        Printf(Tee::PriError, "Cannot use argument NumDisabledSMs with NumDisabledGPCs\n");
        return RC::BAD_PARAMETER;
    }
    if (m_NumDisabledSMs)
    {
        const UINT32 smCount = GetBoundLwdaDevice().GetShaderCount();
        if (m_NumDisabledSMs >= smCount)
        {
            Printf(Tee::PriError,
                   "Cannot disable %u SMs! Number of SMs on device: %u\n",
                   m_NumDisabledSMs,
                   smCount);
            return RC::BAD_PARAMETER;
        }

        m_SMSelectionMode = static_cast<UINT32>(SMSelectionMode::RANDOM);
    }
    else if (m_NumDisabledGPCs)
    {
        const UINT32 gpcCount = GetBoundGpuSubdevice()->GetGpcCount();
        if (m_NumDisabledGPCs >= gpcCount)
        {
            Printf(Tee::PriError,
                   "Cannot disable %u GPCs! Number of GPCs on device: %u\n",
                   m_NumDisabledGPCs,
                   gpcCount);
            return RC::BAD_PARAMETER;
        }

        m_SMSelectionMode = static_cast<UINT32>(SMSelectionMode::RANDOM_GPC);
    }

    // Cases when we launch oclwpancy kernels to restrict number of active SMs
    //  1) When either NumDisabledSMs or NumDisabledGPCs is explicitly set
    //  2) When we're using SM-based Fmax targeting mode
    m_OclwpySMs =
        m_NumDisabledSMs ||
        m_NumDisabledGPCs ||
        static_cast<TargetingMode>(m_TargetingMode) == TargetingMode::FMAX_SM;

    if (m_OclwpySMs)
    {
        if (m_ComputeCap < Lwca::Capability::SM_70)
        {
            Printf(Tee::PriError, "SM restriction not supported on pre-Volta kernels\n");
            return RC::BAD_PARAMETER;
        }

        // Verify that SMSelectionMode is valid
        if (m_SMSelectionMode >= static_cast<UINT32>(SMSelectionMode::NUM_MODES))
        {
            Printf(Tee::PriError,
                   "Unknown/unsupported SM selection mode: %u!\n",
                   m_SMSelectionMode);
            return RC::BAD_PARAMETER;
        }
    }

    // Initialize Alpha and Beta and verify that the Alpha and Beta parameters
    // are valid for the GEMM variant
    CHECK_RC(InitAlphaBeta());
    if (!m_SkipAlphaBetaCheck)
    {
        CHECK_RC(ValidateAlphaBeta());
    }

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Evaluates whether a CRC based matrix comparison is possible or not
//!
//! Lwrrently, CRC algorithm we use processes data in 64 bit units. Hence we need
//! to verify if the data satisfies alignment requirements.
RC LwdaLinpack::IsCrcBasedVerificationPossible()
{
    UINT64 outputBufferSize = NumElemsToBytes(m_KernParams.typeofOutputElement, m_OutputBufferLen);
    m_CrcNumThreadsPerBlock = GetBoundLwdaDevice().GetAttribute
                                         (LW_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK);
    m_CrcNumBlocks = GetBoundLwdaDevice().GetShaderCount();

    if ((m_CrcNumThreadsPerBlock == 0) || (m_CrcNumBlocks == 0))
    {
        Printf(Tee::PriWarn, "Unable to read Lwca device attributes \n");
        // Since we cannot decide whether CRC based verification is possible,
        // fall back to reference based validation
        m_UseCrcToVerify = false;
        return OK;
    }

    if (!(GetLwdaInstance().IsLwdaModuleSupported("lwsurf")))
    {
        Printf(Tee::PriWarn, "Unable to load lwsurf lwbin required for computing CRCs."
                             "Fall back to reference data based verification\n");
        m_UseCrcToVerify = false;
        return OK;
    }

    const UINT64 crcThreadsLaunched = static_cast<UINT64>(m_CrcNumThreadsPerBlock) * m_CrcNumBlocks;
    if (outputBufferSize % sizeof(UINT64))
    {
        Printf(Tee::PriWarn, " Memory size not aligned to run CRC algorithm."
                             "Fall back to reference data based verification\n");
        m_UseCrcToVerify = false;
        return OK;
    }

    // crcThreadsLaunched represents the number of threads launched to compute CRC.
    // Buffer is split across these threads and CRCs are computed in parallel. Memory
    // region size per shard must be a multiple of 64 bit
    UINT64 bufferSizePerThread = outputBufferSize / crcThreadsLaunched;
    if (bufferSizePerThread % sizeof(UINT64))
    {
        Printf(Tee::PriWarn, " Buffer Size per CRC thread not aligned to run CRC "
                      "algorithm. Fall back to reference data based verification\n");
        m_UseCrcToVerify = false;
        return OK;
    }

    // If there are any residual elements, those elements are considered while
    // computing the CRC by the threads (each thread picks one residual element
    // and uses it while computing the CRC). Need to check that this data is a
    // multiple of 64 bit
    UINT64 residualDataSize = outputBufferSize % crcThreadsLaunched;
    if (residualDataSize  % sizeof(UINT64))
    {
        Printf(Tee::PriWarn, "Residual data size not aligned to run CRC algorithm."
                             "Fall back to reference data based verification\n");
        m_UseCrcToVerify = false;
    }

    // Each CRC thread launched writes a 32 bit number to crc buffer. In order to optimize
    // CRC comparison, we compare CRC buffers generated from Reference matrix against data
    // matrix in FB. This compare kernel uses 64 bit units of data to perform comparison
    // Hence CRC buffer size should be a multiple of 64 bits which means, crcThreadsLaunched
    // must be a multiple of 2.
    if (crcThreadsLaunched % 2)
    {
        Printf(Tee::PriWarn, "CRC threads launched is not a multiple of 2."
                             "Fall back to reference data based verification\n");
        m_UseCrcToVerify = false;
    }

    return OK;
}

//---------------------------------------------------------------
//! \brief Load and configure the GEMM kernel
//!
RC LwdaLinpack::InitGemmFunc()
{
    RC rc;

    // Load the GEMM module
    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule(m_GemmData.moduleName, &m_ModuleGemm));

    // Locate the kernel
    const char* linpackFuncName = m_GemmData.linpackFuncNames[m_ComputeCap];
    MASSERT(linpackFuncName);
    if (linpackFuncName[0] == '\0')
    {
        MASSERT(!"No linpack function exists for this particular GEMM type for this Compute Capability");
        return RC::SOFTWARE_ERROR;
    }
    m_GemmFunc = m_ModuleGemm[linpackFuncName];
    CHECK_RC(m_GemmFunc.InitCheck());

    // Callwlate the number of blocks in one wave (depends upon the kernel being used)
    CHECK_RC(m_GemmFunc.GetMaxActiveBlocksPerSM(
                 &m_MaxBlocksPerSM,
                 m_KernParams.blockDimX * m_KernParams.blockDimY,
                 0));

    // Use dynamic shared memory if amount of shared mem exceeds static shared mem
    const UINT32 maxStaticShmem = static_cast<UINT32>(GetBoundLwdaDevice().GetAttribute(
                                      LW_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK));
    if (m_KernParams.sharedMemSize > maxStaticShmem)
    {
        const UINT32 dynamicShmem = m_KernParams.sharedMemSize - maxStaticShmem;

        // We need to set this attribute if our dynamic shared mem is over the max static
        CHECK_RC(m_GemmFunc.SetAttribute(LW_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES,
                                         dynamicShmem));
        CHECK_RC(m_GemmFunc.SetSharedSize(dynamicShmem));
    }

    return rc;
}

//---------------------------------------------------------------
//! \brief Configure the grid and CTA dimensions of the GEMM kernel
//!
RC LwdaLinpack::ConfigGemmFuncDim()
{
    m_GemmFunc.SetGridDim(m_GridDimX, m_GridDimY);
    m_GemmFunc.SetBlockDim(m_KernParams.blockDimX, m_KernParams.blockDimY);
    return RC::OK;
}

//---------------------------------------------------------------
//! \brief Launch the GEMM kernel
//!
RC LwdaLinpack::LaunchGemmKernel(const ByteStream& paramBuffer)
{
    return m_GemmFunc.LaunchWithBuffer(paramBuffer);
}

//---------------------------------------------------------------
//! \brief Setup the properties needed to run the test
//!
RC LwdaLinpack::Setup()
{
    RC rc;
    CHECK_RC(LwdaStreamTest::Setup());

    // Load linpack module
    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("linpack", &m_Module));

    // Load and configure the GEMM kernel
    // (This doesn't apply to LwdaLinpackCask)
    CHECK_RC(InitGemmFunc());

    // Initialize M/N/K parameters values if they haven't been overriden by user
    CHECK_RC(InitializeMNKParameters());

    // Scale M/N/K parameters if requested, otherwise check for correctness
    CHECK_RC(ValidateMNKParameters(GetBoundLwdaDevice().GetShaderCount()));

    // Set GEMM kernel grid dimensions based on Msize and Nsize
    SetGridDimX();
    SetGridDimY();

    // Configure GEMM kernel grid and CTA dimensions (this comes after MNK is set)
    // (This doesn't apply to LwdaLinpackCask)
    CHECK_RC(ConfigGemmFuncDim());

    // Callwlate how much memory to allocate for A, B, C matrices altogether
    CHECK_RC(GetMemAllocInfo());

    // For non-GEMM kernels (matrix fill and verification) use max threads and blocks
    m_NumThreads = GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK);
    m_NumBlocks = GetBoundLwdaDevice().GetShaderCount();

    if (m_UseCrcToVerify)
    {
        // Check if CRC based verification is possible. If not, reset it to
        // reference data based verification
        CHECK_RC(IsCrcBasedVerificationPossible());
    }

    // Allocate all LWCA Host and Device memory
    if (OK != (rc = AllocMemory()))
    {
        Printf(Tee::PriError,
               "LwdaLinpack memory allocation failed! Requested %llu MB.\n"
               "Consider reducing Ksize, Nsize, or Msize to reduce the size of the matrices\n",
               CalcTotalMemBytes() >> 20);
        return rc;
    }

    if (m_Ktrue == 0)
    {
        m_Ktrue = m_Ksize;
    }

    // Enforce synchronous mode when launching CPU stress
    if (m_CpuThreads)
    {
        m_SynchronousMode = true;
    }

    // Load gpusync module if we need to manage kernel launches
    if (m_SynchronousMode || m_LaunchDelayUs || m_BatchDelayUs || (m_TestMode == MODE_PULSE))
    {
        CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("gpusync", &m_ModuleGpuSync));
        m_SyncFunc = m_ModuleGpuSync.GetFunction(
            "PollAllLocks",
            1,
            1);
        m_DelayFunc = m_ModuleGpuSync.GetFunction(
            "LwdaDelay",
            1,
            1);
        m_WriteU32Func = m_ModuleGpuSync.GetFunction(
            "WriteU32",
            1,
            1);
        CHECK_RC(m_SyncFunc.InitCheck());
        CHECK_RC(m_DelayFunc.InitCheck());
        CHECK_RC(m_WriteU32Func.InitCheck());
    }

    // For synchronous mode, allocate common memory that can be accessed by all GPUs under test
    if (m_SynchronousMode)
    {
        GpuDevMgr * pMgr = (GpuDevMgr*)DevMgrMgr::d_GraphDevMgr;
        const UINT32 numGpus = pMgr->NumGpus();
        const UINT32 gpuIdx = GetBoundGpuSubdevice()->GetGpuInst();

        if (Tasker::GetGroupSize() != numGpus)
        {
            Printf(Tee::PriError, "Linpack SynchronousMode must be run with conlwrrent devices\n");
            return RC::ILWALID_ARGUMENT;
        }

        // Have the first GPU allocate and manage the host memory
        if (gpuIdx == 0)
        {
            // Ensure that no other LwdaLinpack test is running simultaneously
            if (s_GpuSync.GetPtr() || s_GpuSync.GetSize())
            {
                Printf(Tee::PriError, "Only one LwdaLinpack test can be driving synchronous mode, "
                                      "another one is already running!\n");
                return RC::RESOURCE_IN_USE;
            }

            // Reset timeout flag (we use this with RuntimeMs to synchronize ending the test)
            s_TimedOut = false;

            // Allocate host memory for synchronizing kernel launches
            // Note: For every device, allocate
            // * One UINT64 to store the value used between GPUs to synchronize them
            //   with each other.
            // * One UINT32 to store the value that will notify the CPU stress thread
            //   when GPUs are doing work and when they are idle.
            const size_t syncSize = (sizeof(UINT64) + sizeof(UINT32)) * MAX_SYNC_DEVICES;
            CHECK_RC(GetLwdaInstance().AllocHostMem(syncSize, &s_GpuSync));
            CHECK_RC(s_GpuSync.Clear());

            // Allocate matrix for reporting sync timeouts
            const size_t timeoutMatrixSize = sizeof(UINT32) * MAX_SYNC_DEVICES * MAX_SYNC_DEVICES;
            CHECK_RC(GetLwdaInstance().AllocHostMem(timeoutMatrixSize, &s_TimeoutMatrix));
            CHECK_RC(s_TimeoutMatrix.Clear());
        }
        // Sync all conlwrrent devices to prevent races on shared host memory allocation
        Tasker::WaitOnBarrier();
    }

    if (m_UseCrcToVerify)
    {
        CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("lwsurf", &m_ModuleLwSurf));

        m_ComputeCrcFunc = m_ModuleLwSurf["GetCRC64"];
        CHECK_RC(m_ComputeCrcFunc.InitCheck());
        m_ComputeCrcFunc.SetGridDim(m_CrcNumBlocks);
        m_ComputeCrcFunc.SetBlockDim(m_CrcNumThreadsPerBlock);

        m_CompareCrcFunc = m_ModuleLwSurf["CompareBuffers64"];
        CHECK_RC(m_CompareCrcFunc.InitCheck());
        m_CompareCrcFunc.SetGridDim(m_CrcNumBlocks);
        m_CompareCrcFunc.SetBlockDim(m_CrcNumThreadsPerBlock);
    }

    m_GenRndmDataFunc = m_Module[m_GemmData.genRandomFuncName];
    CHECK_RC(m_GenRndmDataFunc.InitCheck());
    m_GenRndmDataFunc.SetGridDim(m_NumBlocks);
    m_GenRndmDataFunc.SetBlockDim(m_NumThreads);

    m_GenConstantDataFunc = m_Module[m_GemmData.genConstantFuncName];
    CHECK_RC(m_GenConstantDataFunc.InitCheck());
    m_GenConstantDataFunc.SetGridDim(m_NumBlocks);
    m_GenConstantDataFunc.SetBlockDim(m_NumThreads);

    m_CompareOnDevFunc = m_Module[m_GemmData.checkFuncName];
    CHECK_RC(m_CompareOnDevFunc.InitCheck());
    m_CompareOnDevFunc.SetGridDim(m_NumBlocks);
    m_CompareOnDevFunc.SetBlockDim(m_NumThreads);

    // Generate metadata for sparse
    if (m_KernParams.isSparse)
    {
        m_GenMetadataFunc = m_Module["GenSparseMetadata"];
        CHECK_RC(m_GenMetadataFunc.InitCheck());
        m_GenMetadataFunc.SetGridDim(m_NumBlocks);
        m_GenMetadataFunc.SetBlockDim(m_NumThreads);
    }

    if (m_GemmData.naiveGemmFuncName[0] != '\0')
    {
        // Naive init isn't supported on all kernels
        m_NaiveGemmFunc = m_Module[m_GemmData.naiveGemmFuncName];
        CHECK_RC(m_NaiveGemmFunc.InitCheck());
        m_NaiveGemmFunc.SetGridDim(m_NumBlocks);
        m_NaiveGemmFunc.SetBlockDim(m_NumThreads);
    }
    else if (m_NaiveInit)
    {
        Printf(Tee::PriError, "NaiveInit is not supported for this GEMM type\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    // Allocate some events.
    for (UINT32 i = 0; i < EvNumEvents; i++)
    {
        m_Events[i] = GetLwdaInstance().CreateEvent();
        CHECK_RC(m_Events[i].InitCheck());
    }

    // Check that ReportFailingSmids is used properly and override CheckLoops
    if (m_ReportFailingSmids)
    {
        StickyRC stickyRc;

        // Override CheckLoops, since for XMMA kernels the SMIDs are not stable
        // between kernel launches
        m_CheckLoops = 1;
        Printf(Tee::PriWarn, "Forcing CheckLoops=1 as ReportFailingSmids is set\n");

        if (m_ComputeCap < Lwca::Capability::SM_50)
        {
            Printf(Tee::PriError, "ReportFailingSmids not supported on pre-Maxwell kernels\n");
            stickyRc = RC::BAD_PARAMETER;
        }
        if (m_TestMode == MODE_WALKING)
        {
            Printf(Tee::PriError, "ReportFailingSmids not supported in Walking mode\n");
            stickyRc = RC::BAD_PARAMETER;
        }
        if (m_UseCrcToVerify)
        {
            Printf(Tee::PriError, "ReportFailingSmids cannot be used with UseCrcToVerify\n");
            stickyRc = RC::BAD_PARAMETER;
        }
        if (m_NaiveInit)
        {
            Printf(Tee::PriError, "ReportFailingSmids cannot be used with NaiveInit\n");
            stickyRc = RC::BAD_PARAMETER;
        }
        CHECK_RC(stickyRc);
    }

    // General kernel launch information
    VerbosePrintf("    Function name       %s\n", GemmFunctionName());
    VerbosePrintf("    Buffer size:        %llu elements\n", m_MaxInputBufferLen + m_OutputBufferLen);
    VerbosePrintf("    Allocating on GPU:  %.3f MB\n", static_cast<double>(CalcTotalMemBytes()) /
                                                       static_cast<double>(1_MB));
    VerbosePrintf("    Num SM:             %u\n", GetBoundLwdaDevice().GetShaderCount());
    VerbosePrintf("    Block size          %u %u\n", m_KernParams.blockDimX, m_KernParams.blockDimY);
    VerbosePrintf("    Grid size           %u %u\n", m_GridDimX, m_GridDimY);
    VerbosePrintf("    Msize:              %u (x%u)\n", m_Msize, m_KernParams.mSizeMultiple);
    VerbosePrintf("    Nsize:              %u (x%u)\n", m_Nsize, m_KernParams.nSizeMultiple);
    VerbosePrintf("    Ksize:              %u (x%u)\n", m_Ksize, m_KernParams.kSizeMultiple);
    if (m_KernParams.isSparse)
    {
        VerbosePrintf("    Sparse Metadata:    %llu Bytes\n", CalcMatrixAmMaxBytes());
    }
    if (m_Kset.size() > 1)
    {
        VerbosePrintf("    Kset:               [%u", m_Kset[0]);
        for (UINT32 i = 1; i < m_Kset.size(); i++)
        {
            VerbosePrintf(", %u", m_Kset[i]);
        }
        VerbosePrintf("]\n");
    }

    if (m_MemTmpRange.first != std::numeric_limits<INT32>::max() &&
        m_MemTmpRange.second != std::numeric_limits<INT32>::max())
    {
        VerbosePrintf("    MemTmpRange:        [%d, %d]\n",
                m_MemTmpRange.first, m_MemTmpRange.second);

        // Store the Global memTmpRange
        CHECK_RC(GetBoundGpuSubdevice()->GetThermal()->GetOverTempRange(
                OverTempCounter::MEMORY_TEMP,
                &m_GlobalMemTmpRange.first,
                &m_GlobalMemTmpRange.second));

        VerbosePrintf("    Overriding Global MemTmpRange:        [%d, %d]\n",
                m_GlobalMemTmpRange.first, m_GlobalMemTmpRange.second);

        CHECK_RC(GetBoundGpuSubdevice()->GetThermal()->SetOverTempRange(
                OverTempCounter::MEMORY_TEMP,
                m_MemTmpRange.first,
                m_MemTmpRange.second));
    }

    if (m_GpuTmpRange.first != std::numeric_limits<INT32>::max() &&
        m_GpuTmpRange.second != std::numeric_limits<INT32>::max())
    {
        VerbosePrintf("    GpuTmpRange:        [%d, %d]\n",
                m_GpuTmpRange.first, m_GpuTmpRange.second);

        // Store the Global gpuTmpRange
        CHECK_RC(GetBoundGpuSubdevice()->GetThermal()->GetOverTempRange(
                OverTempCounter::INTERNAL_TEMP,
                &m_GlobalGpuTmpRange.first,
                &m_GlobalGpuTmpRange.second));

        VerbosePrintf("    Overriding Global GpuTmpRange:        [%d, %d]\n",
                m_GlobalGpuTmpRange.first, m_GlobalGpuTmpRange.second);

        CHECK_RC(GetBoundGpuSubdevice()->GetThermal()->SetOverTempRange(
                OverTempCounter::INTERNAL_TEMP,
                m_GpuTmpRange.first,
                m_GpuTmpRange.second));
    }

    // Useful prints for debugging memory allocations and virtual address of failures
    VerbosePrintf("    Matrix A:           0x%08llx\n", CalcMatrixAPtr());
    VerbosePrintf("    Matrix B:           0x%08llx\n", CalcMatrixBPtr());
    VerbosePrintf("    Matrix C:           0x%08llx\n", CalcMatrixCPtr());
    if (UseReferenceData())
    {
        VerbosePrintf("    Matrix RefC:        0x%08llx\n", CalcRefMatrixCPtr());
    }

    const UINT32 gpcCount = GetBoundGpuSubdevice()->GetGpcCount();
    const UINT32 smCount = GetBoundLwdaDevice().GetShaderCount();

    if (m_TargetingFmax)
    {
        // Query target Pll while GPU is in idle state
        // We'll aim for this value in our GEMM workloads
        CHECK_RC(QueryTargetPllkHz(&m_MaxTargetPllkHz));
        VerbosePrintf("    TargetPllkHz in idle state:  %llu\n", m_MaxTargetPllkHz);
    }

    if (static_cast<TargetingMode>(m_TargetingMode) == TargetingMode::FMAX_PULSE)
    {
        // Begin by adjusting DutyPct first
        m_PulseAdjustmentStage = PulseAdjustmentStage::ADJUSTING_DUTY;

        CHECK_RC(MakeSearcher(&m_pDutyPctBinSearcher,
                              5,
                              100,
                              m_DutyPctSearchStepSize));

        CHECK_RC(MakeSearcher(&m_pPulseUsBinSearcher,
                              BIN_SEARCH_MIN_PULSE_US,
                              BIN_SEARCH_MAX_PULSE_US,
                              m_PulseUsSearchStepSize));
	}
	else if (static_cast<TargetingMode>(m_TargetingMode) == TargetingMode::FMAX_SM)
	{
        CHECK_RC(MakeSearcher(&m_pSMCountBinSearcher,
                              ALIGN_UP(BIN_SEARCH_MIN_SM_COUNT, m_SMCountSearchStepSize),
                              ALIGN_DOWN(smCount, m_SMCountSearchStepSize),
                              m_SMCountSearchStepSize));
        m_NumDisabledSMs =
            static_cast<UINT32>(m_pSMCountBinSearcher->StartAndGetStartingIdx());
    }

    if (m_NumDisabledSMs)
    {
        // Initialize vector of elements in range (0, #(SMs) - 1)
        m_RangeSMIDs.resize(smCount);
        std::iota(m_RangeSMIDs.begin(), m_RangeSMIDs.end(), 0);
    }
    else if (m_NumDisabledGPCs)
    {
        // Contains mapping of HW GPC ID to SMIDs belonging to that particular GPC
        m_GPCToSMIDsMap.resize(gpcCount);

        // Initialize vector of elements in range (0, #(GPCs) - 1)
        m_RangeGPCs.resize(gpcCount);
        std::iota(m_RangeGPCs.begin(), m_RangeGPCs.end(), 0);

        // Map HW GPC ID to list of SMIDs belonging to that particular GPC
        for (UINT32 smid = 0; smid < smCount; ++smid)
        {
            UINT32 hwGpcNum = 0;
            UINT32 dummy = 0;
            CHECK_RC(GetBoundGpuSubdevice()->SmidToHwGpcHwTpc(smid, &hwGpcNum, &dummy));
            
            m_GPCToSMIDsMap[hwGpcNum].push_back(smid);
        }
    }

    if (m_OclwpySMs)
    {
        // Need different stream to launch occupy kernel conlwrrently with
        // GEMM kernels
        m_SMOclwpancyStream = GetLwdaInstance().CreateStream();
        CHECK_RC(m_SMOclwpancyStream.InitCheck());

        m_InitOclwpyBarrierFunc = m_Module.GetFunction("InitOclwpyBarrier", 1, 1);
        CHECK_RC(m_InitOclwpyBarrierFunc.InitCheck());
        m_OclwpyFunc = m_Module.GetFunction("OclwpySelectSMs", smCount, 1);
        CHECK_RC(m_OclwpyFunc.InitCheck());

        // Allocate max possible shared memory for each block to fully occupy
        // the SM for itself
        CHECK_RC(m_OclwpyFunc.SetSharedSizeMax());

        CHECK_RC(GetLwdaInstance().AllocHostMem(sizeof(UINT32), &m_HostAllBlocksScheduled));
        CHECK_RC(GetLwdaInstance().AllocHostMem(sizeof(UINT32) * smCount, &m_HostOclwpySMFlags));
        CHECK_RC(m_HostAllBlocksScheduled.Clear());
        CHECK_RC(m_HostOclwpySMFlags.Clear());
    }

    return rc;
}

//-------------------------------------------------------------------------------------
//! \brief Generate constant/random data in device memory
//!
RC LwdaLinpack::FillABMatrices(UINT32 salt)
{
    RC rc;

    // Types smaller than 1 byte in size are packed into 8-bit integers
    MASSERT(elementDataSize(m_KernParams.typeofInputElement) >= 1);
    const bool byteType = elementDataSize(m_KernParams.typeofInputElement) == 1;
    const elementDataType_t type = byteType ? R_8I : m_KernParams.typeofInputElement;
    const UINT64 numElemsA = NumBytesToElems(type, CalcMatrixAMaxBytes());
    const UINT64 numElemsB = NumBytesToElems(type, CalcMatrixBMaxBytes());

    // Use different seeds for matrices A and B so we get a different
    // sequence of data
    const UINT32 seedA = (2 * salt) + GetTestConfiguration()->Seed();
    const UINT32 seedB = (2 * salt + 1) + GetTestConfiguration()->Seed();

    if (m_UseConstantDataA)
    {
        CHECK_RC(m_GenConstantDataFunc.Launch(
                    CalcMatrixAPtr(),
                    numElemsA,
                    m_MtxADataConstant));
    }
    else
    {
        CHECK_RC(m_GenRndmDataFunc.Launch(
                    CalcMatrixAPtr(),
                    numElemsA,
                    seedA,
                    m_MtxDataMean,
                    m_MtxDataSD,
                    m_NonZeroPct));
    }

    if (m_UseConstantDataB)
    {
        CHECK_RC(m_GenConstantDataFunc.Launch(
                    CalcMatrixBPtr(),
                    numElemsB,
                    m_MtxBDataConstant));
    }
    else
    {
        CHECK_RC(m_GenRndmDataFunc.Launch(
                    CalcMatrixBPtr(),
                    numElemsB,
                    seedB,
                    m_MtxDataMean,
                    m_MtxDataSD,
                    m_NonZeroPct));
    }

    // if we need metadata
    if (m_KernParams.isSparse)
    {
        CHECK_RC(m_GenMetadataFunc.Launch(
                    CalcMatrixAmPtr(),
                    CalcMatrixAmMaxBytes() / m_KernParams.sizeofMetadataElement,
                    seedA));
    }

    CHECK_RC(GetLwdaInstance().Synchronize());
    return OK;
}

// Increase a matrix dimension by 'milwalue' and return true if increased
bool LwdaLinpack::IncreaseMtxDimension(UINT32 * value, UINT32 milwalue, UINT32 maxValue)
{
    UINT32 oldValue = *value;
    *value += milwalue;
    *value = MINMAX(milwalue, *value, maxValue);
    return oldValue < *value;
}

// Decrease a matrix dimension by 'milwalue' and return true if decreased
bool LwdaLinpack::DecreaseMtxDimension(UINT32 * value, UINT32 milwalue, UINT32 maxValue)
{
    UINT32 oldValue = *value;
    *value -= milwalue;
    *value = MINMAX(milwalue, *value, maxValue);
    return oldValue > *value;
}

//-------------------------------------------------------------------------------
//! \brief Compares the reference matrix against computed matrix. Requires both
//!        reference and computed matrices to be stored on FB
RC LwdaLinpack::VerifyUsingReferenceData()
{
    RC rc = OK;
    MASSERT(UseReferenceData());

    // Init the ComparisonResult Buffer to zeroes
    if (m_TestMode == MODE_WALKING)
    {
        vector<INT08> zeroBuf(ERROR_LOG_SIZE_BYTES, 0);
        CHECK_RC(m_Data.Set(zeroBuf.data(),
                            m_OtherDataOffset + m_ComparisonResultSubOffset,
                            zeroBuf.size()));
        CHECK_RC(m_Data.Set(zeroBuf.data(),
                            m_OtherDataOffset + m_MiscompareCountSubOffset,
                            sizeof(UINT64)));
    }
    else
    {
        CHECK_RC(m_ComparisonResult.Clear());
        CHECK_RC(m_MiscompareCount.Clear());
    }

    // Compare the results on GPU
    CHECK_RC(m_CompareOnDevFunc.Launch(CalcRefMatrixCPtr(), CalcMatrixCPtr(), CalcComparisonResultPtr(), CalcMicompareCountPtr(), m_OutputBufferLen));

    // Get the miscompare count first
    if (m_TestMode == MODE_WALKING)
    {
        CHECK_RC(m_Data.Get(m_HostMiscompareCount.GetPtr(),
                            m_OtherDataOffset + m_MiscompareCountSubOffset,
                            m_HostMiscompareCount.GetSize()));
    }
    else
    {
        CHECK_RC(m_MiscompareCount.Get(m_HostMiscompareCount.GetPtr(), m_HostMiscompareCount.GetSize()));
    }
    CHECK_RC(GetLwdaInstance().Synchronize());

    UINT64 numMiscompares = *static_cast<UINT64*>(m_HostMiscompareCount.GetPtr());
    if (numMiscompares > 0)
    {
        Printf(Tee::PriError, "LwdaLinpack found %llu miscompare(s)\n", numMiscompares);

        if (m_ReportFailingSmids)
        {
            // Copy 'm_ComparisonResult' to 'm_HostComparisonResult' to get the miscompares
            if (m_TestMode == MODE_WALKING)
            {
                CHECK_RC(m_Data.Get(m_HostComparisonResult.GetPtr(),
                                    m_OtherDataOffset + m_ComparisonResultSubOffset,
                                    m_HostComparisonResult.GetSize()));
            }
            else
            {
                CHECK_RC(m_ComparisonResult.Get(m_HostComparisonResult.GetPtr(),
                                                m_HostComparisonResult.GetSize()));
            }

            // Copy SMID map to host memory
            CHECK_RC(CopySmidMapToHost(m_HostSmidMap));
            CHECK_RC(GetLwdaInstance().Synchronize());

            LinpackError* pMiscompares = static_cast<LinpackError*>(m_HostComparisonResult.GetPtr());
            UINT16* pSmidMap = static_cast<UINT16*>(m_HostSmidMap.GetPtr());

            if (numMiscompares > ERROR_LOG_LEN)
            {
                Printf(Tee::PriWarn,
                       "%llu failing addresses, but error log only contains %u entries. "
                       "Some failing SMID/TPCs may not be reported.\n",
                       numMiscompares, ERROR_LOG_LEN);
            }
            for (UINT32 i = 0; i < numMiscompares && i < ERROR_LOG_LEN; i++)
            {
                UINT64 offset = pMiscompares[i].failureIdx;

                // Assumes NT-kernel
                UINT64 col = offset / m_Msize;
                UINT64 row = offset % m_Msize;

                UINT64 blkCol = col / m_KernParams.colsPerBlock;
                UINT64 blkRow = row / m_KernParams.rowsPerBlock;
                UINT16 smid = pSmidMap[blkRow + blkCol * m_GridDimX];
                m_SmidMiscompareCount[smid]++;
            }
        }

        CHECK_RC(DumpMiscompares());
        rc = RC::GOLDEN_VALUE_MISCOMPARE;
    }

    return rc;
}

//-------------------------------------------------------------------------------
//! \brief Verify the results of the test
//!
RC LwdaLinpack::Verify()
{
    RC rc = OK;
    if (!m_VerifyResults)
    {
        return OK;
    }

    if (UseReferenceData())
    {
        CHECK_RC(VerifyUsingReferenceData());
    }
    else
    {   // Compare CRC buffers
        bool crcsMatch = true;
        CHECK_RC(CompareCrcs(CalcRefCrcBufPtr(),
                             CalcDataCrcBufPtr(),
                             &crcsMatch));
        if (!crcsMatch)
        {
            rc = RC::GOLDEN_VALUE_MISCOMPARE;
        }
    }

    return rc;
}

RC LwdaLinpack::RunNaiveGemm(UINT64 A, UINT64 B, UINT64 C)
{
    return m_NaiveGemmFunc.Launch(A, B, C, m_Msize, m_Nsize, m_Ktrue);
}

RC LwdaLinpack::SynchronousKernelLaunch()
{
    RC rc;

    GpuDevMgr * pMgr = (GpuDevMgr*)DevMgrMgr::d_GraphDevMgr;
    const UINT32 numGpus = pMgr->NumGpus();
    const UINT32 gpuIdx = GetBoundGpuSubdevice()->GetGpuInst();

    LWdeviceptr devPtrSync = s_GpuSync.GetDevicePtr(&GetLwdaInstance(),
                                                    GetBoundLwdaDevice());
    if (devPtrSync == 0ULL)
    {
        Printf(Tee::PriError, "Failed getting Gpu Sync Device Pointer\n");
        return RC::LWDA_ERROR;
    }
    LWdeviceptr devPtrTimeout = s_TimeoutMatrix.GetDevicePtr(&GetLwdaInstance(),
                                                             GetBoundLwdaDevice());
    if (devPtrTimeout == 0ULL)
    {
        Printf(Tee::PriError, "Failed getting Timeout Matrix Device Pointer\n");
        return RC::LWDA_ERROR;
    }

    // Synchronize kernels
    // Ignore timeout loops for now by setting to max_uint
    CHECK_RC(m_SyncFunc.Launch(devPtrSync, ++m_SyncKernelCount, numGpus, gpuIdx,
                               devPtrTimeout, (std::numeric_limits<UINT32>::max)()));
    return rc;
}

RC LwdaLinpack::Launch(const ByteStream& paramBuffer)
{
    RC rc;

    LWdeviceptr gpuActivePtr = 0;
    if (m_CpuThreads)
    {
        gpuActivePtr = s_GpuSync.GetDevicePtr(&GetLwdaInstance(), GetBoundLwdaDevice());
        gpuActivePtr += sizeof(UINT64) * MAX_SYNC_DEVICES;
        gpuActivePtr += sizeof(UINT32) * GetBoundGpuSubdevice()->GetGpuInst();
    }

    // Optional delay before sync/launch
    if (m_LaunchDelayUs)
    {
        CHECK_RC(m_DelayFunc.Launch(static_cast<UINT64>(m_LaunchDelayUs * 1000.0)));
    }

    // Trigger memory fault. This is a nop unless -emu_mem_trigger arg is used
    GetBoundGpuSubdevice()->EmuMemTrigger();

    // Optional sync before launch
    if (m_SynchronousMode)
    {
        CHECK_RC(SynchronousKernelLaunch());
    }

    if (gpuActivePtr)
    {
        CHECK_RC(m_WriteU32Func.Launch(gpuActivePtr, 1u));
    }

    // Launch GEMM workload
    CHECK_RC(LaunchGemmKernel(paramBuffer));

    if (gpuActivePtr)
    {
        CHECK_RC(m_WriteU32Func.Launch(gpuActivePtr, 0u));
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Print matrix offsets
//!
void LwdaLinpack::PrintMatrixOffsets(Tee::Priority pri) const
{
    Printf(pri, "CMatrixOffset:      %12llu\n", m_CMatrixOffset);
    Printf(pri, "ABMatrixOffset:     %12llu\n", m_ABMatrixOffset);
    Printf(pri, "OtherDataOffset:    %12llu\n", m_OtherDataOffset);
    MASSERT(m_CMatrixOffset % MATRIX_ALIGN == 0);
    MASSERT(m_ABMatrixOffset % MATRIX_ALIGN == 0);
    MASSERT(m_OtherDataOffset % MATRIX_ALIGN == 0);
}

//-----------------------------------------------------------------------------
//! \brief Update the memory pointers for the matrices for Walking Linpack test
//!
RC LwdaLinpack::WalkMatrices(UINT64* amtToTestC, UINT64* amtToTestAB)
{
    RC rc;

    VerbosePrintf("\nWalking matrix pointers\n");
    // Update matrix pointers
    const UINT64 cSize = CalcMatrixCMaxBytes();
    const UINT64 abSize = CalcMatrixAMaxBytes() + CalcMatrixBMaxBytes() + CalcMatrixAmMaxBytes();
    const UINT64 otherSizePadded = ALIGN_UP(CalcOtherMemBytes(), MATRIX_ALIGN);
    const UINT64 lead  = min(min(m_CMatrixOffset, m_ABMatrixOffset), m_OtherDataOffset);
    const UINT64 trail = m_TotalFreeMem - max(max(m_CMatrixOffset + cSize,
                                                  m_ABMatrixOffset + abSize),
                                              m_OtherDataOffset + otherSizePadded);
    const UINT64 memToWalk = min(ALIGN_DOWN(static_cast<UINT64>(m_WalkScale * min(abSize, cSize)),
                                            MATRIX_ALIGN)
                                ,trail);
    MASSERT(lead % MATRIX_ALIGN == 0);
    MASSERT(trail % MATRIX_ALIGN == 0);

    // Useful prints for debugging and viewing test progress (only in -verbose)
    Printf(Tee::PriLow,
           "Lead:     %12llu\n"
           "cSize:    %12llu\n"
           "abSize:   %12llu\n"
           "otherSize:%12llu\n"
           "Trail:    %12llu\n"
           "MemToWalk:%12llu\n"
           "----------------------\n"
           , lead, cSize, abSize, otherSizePadded, trail, memToWalk);

    switch (m_WalkingState)
    {
    // No overlap has happened
    case WalkingState::NORMAL:
        MASSERT(m_CMatrixOffset == lead);
        MASSERT(m_CMatrixOffset + cSize  == m_ABMatrixOffset);
        MASSERT(m_ABMatrixOffset + abSize == m_OtherDataOffset);
        MASSERT(m_OtherDataOffset + otherSizePadded == m_TotalFreeMem - trail);

        // If there is no more room to walk move the "other" data block to the front
        if (trail == 0)
        {
            VerbosePrintf("Other Data Block Overlapped\n");
            m_WalkingState = WalkingState::OTHER_WRAPPED;
            m_OtherDataOffset = 0;
            *amtToTestC  = 0;
            *amtToTestAB = 0;
        }
        // Otherwise walk all data blocks equally
        else
        {
            m_CMatrixOffset   += memToWalk;
            m_ABMatrixOffset  += memToWalk;
            m_OtherDataOffset += memToWalk;
            *amtToTestC  = memToWalk;
            *amtToTestAB = memToWalk;
        }
        break;

    // The Other data block has overlapped
    case WalkingState::OTHER_WRAPPED:
        MASSERT(m_OtherDataOffset == lead);
        MASSERT(m_OtherDataOffset + otherSizePadded <= m_CMatrixOffset);
        MASSERT(m_CMatrixOffset + cSize == m_ABMatrixOffset);
        MASSERT(m_ABMatrixOffset + abSize == m_TotalFreeMem - trail);

        // If there is no more room to walk move the AB Matrix to the front
        if (trail == 0)
        {
            VerbosePrintf("AB Matrix Overlapped\n");
            m_WalkingState = WalkingState::AB_WRAPPED;
            m_ABMatrixOffset  = 0;
            m_OtherDataOffset = abSize;
            *amtToTestC  = 0;
            *amtToTestAB = abSize;
        }
        // Walk the Other data block a reduced amount if a normal walk
        // results in fragmenting the memory
        else if (lead + memToWalk > abSize)
        {
            MASSERT(abSize < otherSizePadded);
            VerbosePrintf("Reduced walk for Other data block\n");
            m_CMatrixOffset    += memToWalk;
            m_ABMatrixOffset   += memToWalk;
            m_OtherDataOffset = abSize;
            *amtToTestC  = memToWalk;
            *amtToTestAB = memToWalk;
        }
        // Otherwise walk all data blocks equally
        else
        {
            m_CMatrixOffset   += memToWalk;
            m_ABMatrixOffset  += memToWalk;
            m_OtherDataOffset += memToWalk;
            *amtToTestC  = memToWalk;
            *amtToTestAB = memToWalk;
        }
        break;

    // Both the AB Matrix and the Other data block have overlapped
    case WalkingState::AB_WRAPPED:
        MASSERT(m_ABMatrixOffset == lead);
        MASSERT(m_ABMatrixOffset + abSize == m_OtherDataOffset);
        MASSERT(m_OtherDataOffset + otherSizePadded <= m_CMatrixOffset);
        MASSERT(m_CMatrixOffset + cSize == m_TotalFreeMem - trail);

        if (trail == 0)
        {
            // Move the C Matrix to the front if there is room.
            // This returns us to WalkingState::NORMAL, indicating the competion of a walk.
            if (lead == cSize)
            {
                VerbosePrintf("C Matrix Overlapped\n");
                m_WalkingState = WalkingState::NORMAL;

                m_CMatrixOffset = 0;
                *amtToTestC  = cSize;
                *amtToTestAB = 0;
            }
            // If there isn't enough room for the C Matrix, scoot the AB Matrix and
            // the Other data block along to make room (and finish testing the memory)
            else if (lead < cSize)
            {
                // C must be larger than AB in order for there to not be enough room
                MASSERT(cSize > abSize);
                VerbosePrintf("Only walking AB Matrix and Other data block\n");
                const UINT64 scoot = min(cSize - m_ABMatrixOffset,
                                         ALIGN_DOWN(static_cast<UINT64>(m_WalkScale * abSize),
                                                    MATRIX_ALIGN));
                m_ABMatrixOffset  += scoot;
                m_OtherDataOffset += scoot;
                *amtToTestC  = 0;
                *amtToTestAB = scoot;
            }
            else
            {
                MASSERT(!"AB matrix offset in WalkingState::AB_WRAPPED is greater than Csize");
                return RC::SOFTWARE_ERROR;
            }
        }
        // Walk the AB Matrix and the Other data block a reduced amount if a normal walk
        // results in fragmenting the memory
        else if (lead + memToWalk > cSize)
        {
            MASSERT(abSize > cSize);
            VerbosePrintf("Reduced walk for AB Matrix and Other data block\n");
            m_CMatrixOffset += memToWalk;
            m_ABMatrixOffset = cSize;
            m_OtherDataOffset = cSize + abSize;
            *amtToTestC = memToWalk;
            *amtToTestAB = cSize - lead;
        }
        // Otherwise walk all data blocks equally
        else
        {
            m_CMatrixOffset   += memToWalk;
            m_ABMatrixOffset  += memToWalk;
            m_OtherDataOffset += memToWalk;
            *amtToTestC  = memToWalk;
            *amtToTestAB = memToWalk;
        }
        break;

    default:
        MASSERT(!"Unsupported WalkingState!");
    }

    // Reset CRC table if applicible
    if (m_UseCrcToVerify)
    {
        CHECK_RC(SetCrcTable());
    }

    VerbosePrintf("New mem to test C:  %12llu\n", *amtToTestC);
    VerbosePrintf("New mem to test AB: %12llu\n", *amtToTestAB);
    PrintMatrixOffsets(GetVerbosePrintPri());

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Run Linpack
//! If we are using Walking Linpack mode then run Linpack multiple times in
//! order to test the entire framebuffer
//!
RC LwdaLinpack::Run()
{
    RC rc;
    StickyRC returnRc;

    // seed the RNG for RandomPulse
    m_Random.SeedRandom(GetTestConfiguration()->Seed());

    if (m_TestMode == MODE_WALKING)
    {
        // Return UNSUPPORTED_FUNCTION if the sizes selected is too big
        const UINT64 cSize =
            CalcMatrixCMaxBytes();
        const UINT64 abSize =
            CalcMatrixAMaxBytes() + CalcMatrixBMaxBytes() + CalcMatrixAmMaxBytes();
        const UINT64 otherSizePadded = ALIGN_UP(CalcOtherMemBytes(), MATRIX_ALIGN);
        const UINT64 trail = m_TotalFreeMem - max(max(m_CMatrixOffset + cSize,
                                                      m_ABMatrixOffset + abSize),
                                                  m_OtherDataOffset + otherSizePadded);
        const UINT64 required = max(max(abSize, cSize), otherSizePadded);
        if (trail < required)
        {
            Printf(Tee::PriError, "Matrix dimensions are too large for Walking Linpack test!\n"
                                  "Require at least %llu unused bytes in buffer of size %llu.\n"
                                  "Only %llu bytes are unused\n",
                                  required, m_TotalFreeMem, trail);
            return RC::BAD_PARAMETER;
        }

        UINT64 memTestedC = 0;
        UINT64 memTestedAB = 0;
        UINT64 amtToTestC = cSize;
        UINT64 amtToTestAB = abSize;

        PrintMatrixOffsets(GetVerbosePrintPri());
        UINT64 step = 0;
        bool walkCompleted = false;
        while (!walkCompleted)
        {
            rc = RunInner();
            returnRc = rc;
            if (rc != OK)
            {
                rc.Clear();
                Printf(Tee::PriError, "Walking Linpack Test failed in step %llu\n", step);
                PrintMatrixOffsets(Tee::PriNormal);
                if (GetGoldelwalues()->GetStopOnError())
                {
                    return returnRc;
                }
            }
            memTestedC  += amtToTestC;
            memTestedAB += amtToTestAB;
            VerbosePrintf("Total mem tested C: %12llu\n", memTestedC);
            VerbosePrintf("Total mem tested AB:%12llu\n", memTestedAB);

            walkCompleted = memTestedC >= m_TotalFreeMem * m_NumWalks &&
                            memTestedAB >= m_TotalFreeMem * m_NumWalks;
            if (!walkCompleted)
            {
                CHECK_RC(WalkMatrices(&amtToTestC, &amtToTestAB));
                step++;
            }
        }

        // Report the percentage of memory that was covered by the matrices.
        double coverage =
            (static_cast<double>(min(memTestedC, memTestedAB)) / static_cast<double>(m_TotalMem)) * 100.0f;
        VerbosePrintf("Coverage was %f%%\n", coverage);
    }
    else
    {
        returnRc = RunInner();
    }
    return returnRc;
}

//-----------------------------------------------------------------------------
//! Check how many sampled gpcclk TargetPll values are >= maxTargetPll
//! If most sample values satisfy that condition, we say that the current
//! pulsing parameters (PulseUs and DutyPct) allow us to run GEMM workload at fmax
//!
bool LwdaLinpack::IsLwrrTargetPllAtMax()
{
    MASSERT(m_TargetPllkHzSamples.size());
    INT32 numPllSamplesAtMax =
        static_cast<INT32>(count_if(m_TargetPllkHzSamples.begin(),
                                    m_TargetPllkHzSamples.end(),
                                    [this](UINT64 targetPLL)
                                    {
                                        return (targetPLL >= m_MaxTargetPllkHz);
                                    }));
    VerbosePrintf("%d / %lu samples at max gpcclk target Pll\n",
                  numPllSamplesAtMax, m_TargetPllkHzSamples.size());
    return numPllSamplesAtMax >= static_cast<INT32>(m_SuccessThreshholdCount);
}

//-----------------------------------------------------------------------------
//! A call to this function can be thought of as a single step in a binary search
//! ex)
//!     - <Call #1> Begin binary search on DutyPct between 1, 100
//!         DutyPct = 50
//!     - Run a batch of kernels to sample gpcclk TargetPll
//!     - <Call #2> If sample suggests we're running at fmax
//!         DutyPct = 75
//!       Else
//!         DutyPct = 25
//!     - Run a batch of kernels to sample gpcclk TargetPll
//!     - ...
//!
void LwdaLinpack::AdjustDutyPct()
{
    if (!m_pDutyPctBinSearcher->HasSearchStarted())
    {
        m_DutyPct =
            static_cast<double>(m_pDutyPctBinSearcher->StartAndGetStartingIdx());
        return;
    }

    BinSearchResult searchResult = BinSearchResult::TARGET_ABSENT;
    bool maxTargetPllReached = IsLwrrTargetPllAtMax();
    m_DutyPct =
        static_cast<double>(m_pDutyPctBinSearcher->RunNextStep(maxTargetPllReached, &searchResult));

    switch (searchResult)
    {
        // We're done adjusting DutyPct. If we're allowed, (user did not specify PulseUs/Ksize)
        // start adjusting PulseUs; otherwise we're done adjusting pulse params.
        case BinSearchResult::TARGET_FOUND:
            m_PulseAdjustmentStage = m_CanAdjustPulseWidth ?
                         PulseAdjustmentStage::ADJUSTING_PULSE_WIDTH :
                         PulseAdjustmentStage::DONE;
            break;

        // If we couldn't find a DutyPct value for which to run at fmax, try
        // searching again, until m_NumSearchTries counter is > m_MaxNumSearchTries.
        case BinSearchResult::TARGET_ABSENT:
            ++m_NumSearchTries;
            if (m_NumSearchTries <= m_MaxNumSearchTries)
            {
                m_pDutyPctBinSearcher->Reset();
                m_DutyPct =
                    static_cast<double>(m_pDutyPctBinSearcher->StartAndGetStartingIdx());
            }
            else
            {
                m_PulseAdjustmentStage = PulseAdjustmentStage::DONE;
            }
            break;

        // Search is still ongoing.
        case BinSearchResult::STILL_SEARCHING:
            break;

        default:
            MASSERT(!"Invalid search result value");
    }
}

//-----------------------------------------------------------------------------
//! See AdjustDutyPct
//!
void LwdaLinpack::AdjustPulseUs()
{
    if (!m_pPulseUsBinSearcher->HasSearchStarted())
    {
        m_PulseUs = static_cast<double>(m_pPulseUsBinSearcher->StartAndGetStartingIdx());
        return;
    }

    BinSearchResult searchResult = BinSearchResult::TARGET_ABSENT;
    bool maxTargetPllReached = IsLwrrTargetPllAtMax();
    m_PulseUs =
        static_cast<double>(m_pPulseUsBinSearcher->RunNextStep(maxTargetPllReached, &searchResult));

    switch (searchResult)
    {
        // We're done adjusting PulseUs. This is the last stage of pulse
        // adjustment period, so signify that we're done.
        case BinSearchResult::TARGET_FOUND:
            m_PulseAdjustmentStage = PulseAdjustmentStage::DONE;
            break;

        // If we couldn't find a PulseUs value for which to run at fmax, revert
        // to the previous stage (ADJUSTING_DUTY) and try our search again from
        // there.
        case BinSearchResult::TARGET_ABSENT:
            ++m_NumSearchTries;
            if (m_NumSearchTries <= m_MaxNumSearchTries)
            {
                // Reset PulseUs to the default value we use for adjusting DutyPct
                m_PulseUs = BIN_SEARCH_MIN_PULSE_US;
                m_pDutyPctBinSearcher->Reset();
                m_PulseAdjustmentStage = PulseAdjustmentStage::ADJUSTING_DUTY;
            }
            else
            {
                m_PulseAdjustmentStage = PulseAdjustmentStage::DONE;
            }
            break;

        // Still searching.
        case BinSearchResult::STILL_SEARCHING:
            break;
        default:
            MASSERT(!"Invalid search result value");
    }
}

//-----------------------------------------------------------------------------
//! See AdjustDutyPct
//!
void LwdaLinpack::AdjustSMCount()
{
    if (!m_pSMCountBinSearcher->HasSearchStarted())
    {
        m_NumDisabledSMs = static_cast<UINT32>(m_pSMCountBinSearcher->StartAndGetStartingIdx());
        return;
    }

    BinSearchResult searchResult = BinSearchResult::TARGET_ABSENT;
    bool maxTargetPllReached = IsLwrrTargetPllAtMax();

    m_NumDisabledSMs =
        static_cast<UINT32>(m_pSMCountBinSearcher->RunNextStep(maxTargetPllReached, &searchResult));

    switch (searchResult)
    {
        // We found the number of active SMs to run at fmax.
        // Signify that we're done adjusting sm count
        case BinSearchResult::TARGET_FOUND:
            m_DoneAdjustingSMCount = true;
            break;

        // If we couldn't find number of active SMs at which to run at fmax, try
        // searching again, until m_NumSearchTries counter is > m_MaxNumSearchTries.
        case BinSearchResult::TARGET_ABSENT:
            ++m_NumSearchTries;
            if (m_NumSearchTries <= m_MaxNumSearchTries)
            {
                m_pSMCountBinSearcher->Reset();
            }
            else
            {
                m_DoneAdjustingSMCount = true;
            }
            break;

        // Still searching.
        case BinSearchResult::STILL_SEARCHING:
            break;

        default:
            MASSERT(!"Invalid search result value");
    }
}

RC LwdaLinpack::ConfigurePulse(UINT32* pKval, UINT32* pBatchSize)
{
    bool bCallwlatedK = false;
    double newKval = m_PrevK;
    double estbatch = m_PrevNumLoops;

    *pKval = m_PrevK;

    // If we're actively adjusting pulse width and we have
    // enough gpcclk target pll samples collected, adjust pulse width based
    // on our sample values.
    if (static_cast<TargetingMode>(m_TargetingMode) == TargetingMode::FMAX_PULSE &&
        m_PulseAdjustmentStage == PulseAdjustmentStage::ADJUSTING_PULSE_WIDTH &&
        m_TargetPllkHzSamples.size() >= m_PllSampleCount)
    {
        AdjustPulseUs();
        m_TargetPllkHzSamples.clear();
    }

    // Callwlate new K value if there is a target PulseUs
    if (m_PrevK && m_PulseUs)
    {
        MASSERT(m_PrevBatchMs);
        MASSERT(m_PrevNumLoops);
        double gemmUs = (m_PrevBatchMs * 1000.0 / m_PrevNumLoops) - m_LaunchDelayUs;
        MASSERT(gemmUs > 0);
        newKval = round((m_PulseUs / gemmUs) * m_PrevK);

        if (newKval < m_KernParams.kSizeMultiple)
        {
            Printf(Tee::PriError,
                   "Unable to set PulseUs %5.2f, try increasing this value\n", m_PulseUs);
            return RC::CANNOT_MEET_WAVEFORM;
        }

        newKval = ALIGN_UP(static_cast<UINT32>(newKval), m_KernParams.kSizeMultiple);
        if (newKval > m_MaxKsizeForPulse)
        {
            if (m_ShmooPulse)
            {
                if (m_UsePulseUsRange)
                {
                    // If we're using duration based shmoo and we cannot increase
                    // Ksize over MaxKsizeForPulse, throw an error and inform
                    // the user roughly what MaxKsizeForPulse value should be set
                    const UINT32 estimatedMaxKsizeNeeded =
                        static_cast<UINT32>(round((m_MaxPulseUs / gemmUs) * m_PrevK));
                    Printf(Tee::PriError,
                           "Unable to set PulseUs %5.2f, try increasing MaxKsizeForPulse! "
                           "(Estimate: %u)\n",
                           m_PulseUs,
                           estimatedMaxKsizeNeeded);
                    return RC::CANNOT_MEET_WAVEFORM;
                }
                else
                {
                    // Otherwise, reset all the values
                    *pKval = ALIGN_UP(static_cast<UINT32>(m_Kstart), m_KernParams.kSizeMultiple);
                    estbatch = m_CheckLoops;
                    m_BatchUs = 0;
                    m_PulseUs = 0;
                    bCallwlatedK = false;
                }
            }
            else
            {
                Printf(Tee::PriError,
                       "Unable to set PulseUs %5.2f, try increasing MaxKsizeForPulse\n", m_PulseUs);
                return RC::CANNOT_MEET_WAVEFORM;
            }
        }
        else
        {
            bCallwlatedK = true;
        }
    }

    // Callwlate new batch size if there is a target BatchUs
    if (m_PrevNumLoops && m_BatchUs)
    {
        MASSERT(m_PrevBatchMs);
        estbatch = m_BatchUs / ((m_PrevBatchMs * 1000.0) / m_PrevNumLoops);

        if (round(estbatch) < 1)
        {
            if (m_ShmooPulse)
            {
                // Just set to 1, the shmoo is on PulseUs
                estbatch = 1;
            }
            else
            {
                Printf(Tee::PriError,
                       "Unable to set BatchUs %5.2f, try increasing this value\n", m_BatchUs);
                return RC::CANNOT_MEET_WAVEFORM;
            }
        }
    }

    // If we're actively adjusting duty percentage, and we have
    // enough gpcclk target pll samples collected, adjust duty percentage based
    // on our sample values.
    if (static_cast<TargetingMode>(m_TargetingMode) == TargetingMode::FMAX_PULSE &&
        m_PulseAdjustmentStage == PulseAdjustmentStage::ADJUSTING_DUTY &&
        m_TargetPllkHzSamples.size() >= m_PllSampleCount)
    {
        AdjustDutyPct();
        m_TargetPllkHzSamples.clear();
    }

    // After m_NumSearchTries, we couldn't find the right PulseUs and/or
    // DutyPct values to run at fmax.
    //
    // If we're targeting fmax in deployment down the line, we should
    // warn the user, but continue to run the test with the best known parameters.
    if (m_NumSearchTries > m_MaxNumSearchTries)
    {
        Printf
        (
            Tee::PriError,
            "Failed to modulate pulse parameters to run at target gpcclk frequency: %llukHz\n"
            "       Try increasing MaxNumSearchTries (current %u)\n",
            m_MaxTargetPllkHz,
            m_MaxNumSearchTries
        );
        return RC::CANNOT_MEET_WAVEFORM;
    }

    // If a specific duty cycle is requested, update m_LaunchDelayUs accordingly before
    // it is used to callwlate the new batch size.
    if (m_DutyPct)
    {
        m_LaunchDelayUs = m_PulseUs / m_DutyPct * (100 - m_DutyPct);
    }

    // If Ksize has changed significantly, estimate batchsize using the target pulse
    // and delay instead, since the gemm runtime will change with the new K.
    if (bCallwlatedK && (abs(1 - (m_PrevK / newKval)) > 0.005))
    {
        if (m_BatchUs)
        {
            estbatch = m_BatchUs / (m_PulseUs + m_LaunchDelayUs);
        }
        // Only update to newKval if there was a significant change, otherwise
        // we have to keep recallwlating the ref data at the start of each batch
        *pKval = static_cast<UINT32>(newKval);
    }
    else if (m_RandomPulse)
    {
        if (m_UsePulseUsRange && m_PrevK && m_PrevNumLoops)
        {
            // If we're using duration based pulse range, and we're in random
            // pulse mode, use the previous gemm runtime to callwlate minimum
            // and maximum Ksize based on MinPulseUs and MaxPulseUs
            const double gemmUs =
                (m_PrevBatchMs * 1000.0 / m_PrevNumLoops) - m_LaunchDelayUs;
            MASSERT(gemmUs > 0);
            const double gemmUsPerK = gemmUs / m_PrevK;
            const UINT32 maxKsize =
                static_cast<UINT32>(round(m_MaxPulseUs / gemmUsPerK));
            const UINT32 minKsize =
                max(m_KernParams.kSizeMultiple, static_cast<UINT32>(round(m_MinPulseUs / gemmUsPerK)));
            if (maxKsize > m_MaxKsizeForPulse)
            {
                Printf(Tee::PriError,
                       "Unable to hit max PulseUs for shmoo %5.2f, try increasing MaxKsizeForPulse! "
                       "(Estimate: %u)\n",
                       m_PulseUs,
                       maxKsize);
                return RC::CANNOT_MEET_WAVEFORM;
            }

            *pKval = m_Random.GetRandom(minKsize, maxKsize);
            *pKval = (*pKval / m_KernParams.kSizeMultiple) * m_KernParams.kSizeMultiple;
        }
        else
        {
            // Otherwise, pick a random K value in [1, m_MaxKsizeForPulse]
            *pKval = m_KernParams.kSizeMultiple *
                     m_Random.GetRandom(1, m_MaxKsizeForPulse / m_KernParams.kSizeMultiple);
        }
        m_LaunchDelayUs = m_Random.GetRandom(0, static_cast<UINT32>(m_MaxLaunchDelayUs));
        estbatch = m_CheckLoops;
    }
    else if (m_ShmooPulse && !m_PrevK)
    {
        // If this is the first cycle, initialize to kstart and default batchsize
        *pKval = ALIGN_UP(static_cast<UINT32>(m_Kstart), m_KernParams.kSizeMultiple);
        estbatch = m_CheckLoops;
    }

    *pBatchSize = static_cast<UINT32>(round(fmax(1, estbatch)));

    VerbosePrintf("Configure pulse: K=%d batch=%d LaunchDelayUs=%f\n",
                  *pKval, *pBatchSize, m_LaunchDelayUs);

    return RC::OK;
}

//-----------------------------------------------------------------------------
//! Randomly select SMs to occupy
//!
RC LwdaLinpack::RandomlySelectSMs()
{
    UINT32 *pOclwpySMFlags = static_cast<UINT32*>(m_HostOclwpySMFlags.GetPtr());

    // To simulate randomly selecting SMs to occupy, we'll shuffle the vector
    // of elements in range (0, #SMs - 1), then select the first #(m_NumDisabledSMs)
    // elements which'll correspond to the ids of the SMs we want to occupy.
    m_Random.Shuffle(static_cast<UINT32>(m_RangeSMIDs.size()), m_RangeSMIDs.data());
    for (UINT32 i = 0; i < m_NumDisabledSMs; ++i)
    {
        const UINT32 smid = m_RangeSMIDs[i];
        pOclwpySMFlags[smid] = static_cast<UINT32>(true);
    }

    return RC::OK;
}

//-----------------------------------------------------------------------------
//! Randomly select GPCs to occupy
//!
RC LwdaLinpack::RandomlySelectGPCs()
{
    RC rc;
    
    UINT32 *pOclwpySMFlags = static_cast<UINT32*>(m_HostOclwpySMFlags.GetPtr());

    // To simulate randomly selecting GPCs to occupy, we'll shuffle the vector
    // of elements in range (0, #GPCs - 1), then select the first #(m_NumDisabledGPCs)
    // elements which'll correspond to GPCs that we want to "disable".
    m_Random.Shuffle(static_cast<UINT32>(m_RangeGPCs.size()), m_RangeGPCs.data());
    for (UINT32 i = 0; i < m_NumDisabledGPCs; ++i)
    {
        const UINT32 gpcId = m_RangeGPCs[i];

        // Indicate that all SMs belonging to this GPC should be oclwpied
        for (auto smid : m_GPCToSMIDsMap[gpcId])
        {
            pOclwpySMFlags[smid] = static_cast<UINT32>(true);
        }
    }

    return RC::OK;
}

//-----------------------------------------------------------------------------
//! Router function that calls the correct selector function
//! depending on SMSelectionMode
//!
RC LwdaLinpack::SelectSMsToOclwpy()
{
    RC rc;
    switch (static_cast<SMSelectionMode>(m_SMSelectionMode))
    {
        case SMSelectionMode::RANDOM:
            CHECK_RC(RandomlySelectSMs());
            break;

        case SMSelectionMode::RANDOM_GPC:
            CHECK_RC(RandomlySelectGPCs());
            break;

        default:
            MASSERT(!"Invalid SM oclwpation selection mode!\n");
            return RC::SOFTWARE_ERROR;
    }

    return RC::OK;
}

//-----------------------------------------------------------------------------
//! Launch occupy kernel and wait until SMs are actually oclwpied
//!
RC LwdaLinpack::OclwpySMs()
{
    RC rc;

    // There shouldn't be a case where all SMs are oclwpied, since kernels
    // from different streams will be waiting indefinitely for their blocks
    // to be scheduled
    MASSERT(m_NumDisabledSMs < GetBoundLwdaDevice().GetShaderCount() &&
            m_NumDisabledGPCs < GetBoundGpuSubdevice()->GetGpcCount());

    UINT32 *pAllBlocksScheduled =
        static_cast<UINT32*>(m_HostAllBlocksScheduled.GetPtr());
    Cpu::AtomicWrite(pAllBlocksScheduled, static_cast<UINT32>(false));

    // Launch oclwpancy kernel on different stream, so we can run GEMM kernels
    // conlwrrently and have their blocks scheduled on the unoclwpied SMs
    CHECK_RC(m_OclwpyFunc.Launch(
        m_SMOclwpancyStream,
        m_HostOclwpySMFlags.GetDevicePtr(GetBoundLwdaDevice()),
        m_HostAllBlocksScheduled.GetDevicePtr(GetBoundLwdaDevice())
    ));

    // Poll and wait until all the blocks have been scheduled and are running on
    // actual SMs. Since we allocate max possible shared memory per block,
    // no other LWCA block will be able be scheduled on those SMs
    return Tasker::PollHw(GetTestConfiguration()->TimeoutMs(),
                          [&]()->bool
                          {
                              // Atomically read to prevent compiler optimizing away memory load
                              return static_cast<bool>(Cpu::AtomicRead(pAllBlocksScheduled));
                          },
                          __FUNCTION__);
}

//-----------------------------------------------------------------------------
//! Stop oclwpying SMs
//!
RC LwdaLinpack::VacateSMs()
{
    RC rc;

    // Set all flags to 0 (false), which'll let the LWCA threads that have been
    // polling on this value exit
    CHECK_RC(m_HostOclwpySMFlags.Clear());

    const UINT32 syncTimeoutSec =
        static_cast<UINT32>(std::ceil(GetTestConfiguration()->TimeoutMs() / 1000));
    // Wait until the oclwpancy kernel exits
    CHECK_RC(m_SMOclwpancyStream.Synchronize(syncTimeoutSec));

    return RC::OK;
}

//-----------------------------------------------------------------------------
//! \brief Ilwoke Gemm
//! Setup data in matrices, allocate memory for reference matrices
//! Perform Gemm and verify the result, report errors and timing information
//!
RC LwdaLinpack::RunInner()
{
    RC rc;
    StickyRC returnRc;
    bool timed = m_RuntimeMs > 0;

    const UINT32 gpuIdx = GetBoundGpuSubdevice()->GetGpuInst();

    // Clocks aren't modeled on fmodel, so might as well skip the perf checks.
    // This also means we can skip pmu on fmodel.
    Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();
    bool bClocksSupported = (Platform::GetSimulationMode() != Platform::Fmodel &&
                             !Platform::IsVirtFunMode() && pPerf->IsPState30Supported());

    DEFER
    {
        // If exiting early in SynchronousMode, we want to allow the other GPUs to continue the test.
        //
        // We do this by setting the GpuSync variable to the max value, so that other GPUs will
        // always see the exiting GPU as being ready.
        //
        // This is also useful if RuntimeMs is being used, since GPUs may exit at different times.
        if (m_SynchronousMode)
        {
            static_cast<UINT64*>(s_GpuSync.GetPtr())[gpuIdx] = std::numeric_limits<UINT64>::max();
        }
    };

    if (m_ComputeCap == Lwca::Capability::SM_UNKNOWN)
    {
        Printf(Tee::PriError, "LwdaLinpack not initialized correctly!!\n");
        return RC::SOFTWARE_ERROR;
    }

    UINT64 maxIterations = 0;
    UINT32 totalLoops = GetTestConfiguration()->Loops();
    // If a target batchUs is set, we ignore TestConfiguration.loops
    if (m_BatchUs || totalLoops == 0)
    {
        totalLoops = std::numeric_limits<UINT32>::max();
    }
    // Make sure m_CheckLoops is valid (defaults to TestConfiguration.loops)
    if (m_CheckLoops == 0)
    {
        m_CheckLoops = totalLoops;
    }

    vector< pair<UINT32, UINT32> > steps;
    // Shmoo mode pre-fills the k size to shmoo a range
    if (m_TestMode == MODE_SHMOO)
    {
        UINT32 kstep = ALIGN_UP(m_Kstep, max(8u, m_KernParams.kSizeMultiple));
        UINT32 kstart = ALIGN_UP(m_Kstart, kstep);
        UINT32 nsteps = ((m_Ksize - kstart) / kstep) + 1; //shmoo from k=kstep to k=Ksize

        for (UINT32 i = 0; i < nsteps; i++)
        {
            UINT32 kval  = kstart + i * kstep;
            UINT32 loops = 0;
            if (m_UseLegacyKshmoo)
            {
                // y = mx + b -- distribute original loops across shmoo values according to Kval
                //
                // This is actually bugged since the integer divisions truncate the callwation.
                // As a result regardless of the current kval we run for the same number loops.
                // We keep the behavior anyways in case some teams were relying on this shmoo.
                //
                loops = totalLoops / nsteps - (totalLoops / nsteps / nsteps) * i;
                loops = max<UINT32>(1, loops);
            }
            else
            {
                // This Kshmoo uses an improved algorithm. The goal is to run each step
                // of the Kshmoo for approximately the same amount of time.
                //
                // Since kernel runtime is proportional to the current kval, the number
                // of loops should be ilwersely proportional to it.
                //
                // In order to run for approximate TestConfiguration.Loops loops
                // we scale the number of loops by 'Q', which we compute through integration:
                // totalLoops = Integral[kstart:m_Ksize] of (Q / k)
                //
                double Q =
                    static_cast<double>(totalLoops) /
                    (std::log(static_cast<double>(m_Ksize)) - std::log(static_cast<double>(kstart)));

                // Then we plug in the current kval to get the number of loops for the step
                double stepLoops = kstep * (Q / kval);
                loops = static_cast<UINT32>(max(1.0, stepLoops));
            }
            steps.push_back(pair<UINT32, UINT32>(kval, loops));
            maxIterations += loops;
        }
    }
    else
    {
        UINT32 loops =
            max<UINT32>(1, static_cast<UINT32>(totalLoops / m_Kset.size()));
        for (UINT32 i = 0; i < m_Kset.size(); i++)
        {
            steps.push_back( pair<UINT32,UINT32>(m_Kset[i], loops) );
            maxIterations += loops;
            Printf(Tee::PriDebug, "Kset[%u] will run for %u loops\n", m_Kset[i], i);
        }
    }

    UINT64 opCount = 0;
    UINT64 opCountDuringSampling = 0;
    bool running = true;
    bool doneSamplingPll = false;
    UINT32 scaleFactor = 0;
    UINT64 totalLoopsSoFar = 0;
    UINT64 totalChecksSoFar = 0;
    double timeMsSoFar = 0.0;

    // Initialize the A and B matrices
    CHECK_RC(FillABMatrices(0));

    // This clock counter is for callwlating efficiency, capture it before and after
    // running the test. When targeting fmax, capture clock counter after our
    // adjusment/sampling period to callwlate avg. GPC clock with our adjusted
    // pulse parameters.
    UINT64 clkBegin = 0;
    UINT64 clkEnd   = 0;
    UINT64 clkCntPostSampling = 0;
    if (bClocksSupported)
    {
        CHECK_RC(pPerf->MeasureAvgClkCount(Gpu::ClkGpc, &clkBegin));
    }

    // We need to remember if test was started with KeepRunning set to true in
    // order to know whether to end the test early once KeepRunning has been
    // switched to false by another test
    const bool bPerpetualRun = m_KeepRunning;
    const bool fixedNumLoops = !timed && !bPerpetualRun;

    // Setup progress information for linpack tests if we're not performing a
    // perpetual run. In the case of a perpetual run, we can't lwrrently report
    // on progress since we don't know when the test might end, as the end is
    // controlled by an external test.
    if (!bPerpetualRun)
    {
        if (timed)
        {
            PrintProgressInit(m_RuntimeMs + 1);
        }
        else
        {
            PrintProgressInit(maxIterations + 1);
        }
    }

    double estTimeMsForOneLoop = 0.0;
    UINT32 prevM = 0;
    UINT32 prevN = 0;
    UINT32 prevK = 0;

    if (m_CpuThreads && gpuIdx == 0)
    {
        CHECK_RC(StartCpuStress());
    }
    DEFER
    {
        StopCpuStress();
    };

    UINT32 newMatricesStepSize = 0;
    if (m_NumNewMatrices)
    {
        newMatricesStepSize =
            max(1u, static_cast<UINT32>(maxIterations / m_NumNewMatrices));
        VerbosePrintf("Generating new matrices every %u loops.\n", newMatricesStepSize);
    }

    // Initialize barrier residing in device global memory
    // This barrier will be used in the occupy kernel for device-synchronization
    if (m_OclwpySMs)
    {
        m_InitOclwpyBarrierFunc.Launch(m_SMOclwpancyStream,
                                       GetBoundLwdaDevice().GetShaderCount());
    }

    // Time the test in order to callwlate GFLOPS.
    // This lwrrently includes results checking, so for small Ksize
    // the GFLOPS value may be slightly skewed unless VerifyResults=false.
    CHECK_RC(m_Events[EvBeginGemm].Record());

    // Detach thread, otherwise threads may starve for LWCA work when run on
    // multi-GPU systems or with background tests.
    UINT64 stepIdx = 0;
    for (Tasker::DetachThread detach; running; stepIdx = (stepIdx + 1) % steps.size())
    {
        UINT32 kval = steps[stepIdx].first;
        const UINT32 loops = steps[stepIdx].second;
        UINT32 loopsRemaining = loops;

        // In the call to "ConfigurePulse" method, we'll adjust DutyPct or PulseUs
        // if we're still actively searching for the best pulse parameters to run at fmax
        if (m_BatchUs || (m_TestMode == MODE_PULSE))
        {
            // batch defaults to m_CheckLoops, override later if target batchUs is requested
            UINT32 batch = m_CheckLoops;

            // Only configure after at least one batch, or random/shmoo mode
            if ((m_PrevK && m_PrevNumLoops) || m_RandomPulse || m_ShmooPulse)
            {
                CHECK_RC(ConfigurePulse(&kval, &batch));
            }
            // Make sure we only run one batch, instead of all loops
            loopsRemaining = batch;
        }

        m_Ktrue = kval;

        // Adjust SM count if we're targeting fmax based on SM oclwpancy, and
        // we have enough PLL samples, and
        // we're still actively searching for the best active SM count
        const bool shouldAdjustSMCount =
            static_cast<TargetingMode>(m_TargetingMode) == TargetingMode::FMAX_SM &&
            m_TargetPllkHzSamples.size() >= m_PllSampleCount &&
            !m_DoneAdjustingSMCount;

        if (shouldAdjustSMCount)
        {
            AdjustSMCount();
            m_TargetPllkHzSamples.clear();

            // After m_NumSearchTries, we couldn't find the number of active
            // SMs to run at fmax
            if (m_NumSearchTries > m_MaxNumSearchTries)
            {
                Printf
                (
                    Tee::PriError,
                    "Failed to target gpcclk frequency: %llukHz\n"
                    "Try increasing MaxNumSearchTries (current %u)\n",
                    m_MaxTargetPllkHz,
                    m_MaxNumSearchTries
                );
                return RC::CANNOT_MEET_WAVEFORM;
            }
        }

        if (m_TargetingFmax)
        {
            const bool binSearchDone =
                m_PulseAdjustmentStage == PulseAdjustmentStage::DONE ||
                m_DoneAdjustingSMCount;

            // If we're done sampling gpcclk PLL, reset number of batch loops to
            // m_CheckLoops (default batch value)
            if (!doneSamplingPll && binSearchDone)
            {
                doneSamplingPll = true;
                loopsRemaining = m_CheckLoops;

                // Save launch counts after we're done sampling, so we can subtract
                // this value from the total launch count to get post-sampling launch counts.
                opCountDuringSampling = opCount;
            }

            // If we're not done sampling, override batch loops to some low
            // number to expedite this process
            if (!doneSamplingPll)
            {
                loopsRemaining = NUM_BATCH_LOOPS_PER_SAMPLE;
            }

            // Done sampling gpcclk PLL, get clock count
            if (doneSamplingPll && !clkCntPostSampling)
            {
                CHECK_RC(m_Events[EvEndSampling].Record());
                CHECK_RC(m_Events[EvEndSampling].Synchronize());
                CHECK_RC(pPerf->MeasureAvgClkCount(Gpu::ClkGpc, &clkCntPostSampling));
            }
        }

        // If first run or MNK values changed, regenerate the C matrices and print the MNK
        if (prevM != m_Msize || prevN != m_Nsize || prevK != m_Ktrue)
        {
            // Callwlate C matrix and get the time taken for a loop
            CHECK_RC(GenerateCMatrices(&estTimeMsForOneLoop));
            scaleFactor = 1;

            // Log the kernels taken to build C matrix
            opCount += CalcOpCount(m_Msize, m_Nsize, m_Ktrue, true);
            if (UseReferenceData())
            {
                opCount += CalcOpCount(m_Msize, m_Nsize, m_Ktrue, true);
            }

            VerbosePrintf("LwdaLinpack run: M=%u N=%u K=%u Loops=%u TimePerLoop=%.0fus\n",
                          m_Msize, m_Nsize, m_Ktrue, loopsRemaining, estTimeMsForOneLoop * 1000.0);

            // If this is the first run with a target batchUs, estimate the batch length
            if (!m_PrevNumLoops && m_BatchUs)
            {
                loopsRemaining = static_cast<UINT32>(m_BatchUs / (estTimeMsForOneLoop * 1000 + m_LaunchDelayUs));
            }
        }
        prevM = m_Msize;
        prevN = m_Nsize;
        prevK = m_Ktrue;

        while (loopsRemaining > 0 && running)
        {
            UINT32 numBatchLoops;
            // When pulsing or a batchUs target is set, ignore m_CheckLoops
            if (m_BatchUs || (m_TestMode == MODE_PULSE))
            {
                numBatchLoops = loopsRemaining;
            }
            else
            {
                numBatchLoops = min<UINT32>(m_CheckLoops, loopsRemaining);
            }

            if (m_NumNewMatrices)
            {
                const UINT32 stepsRemaining =
                    newMatricesStepSize - (totalLoopsSoFar % newMatricesStepSize);
                numBatchLoops = min<UINT32>(numBatchLoops, stepsRemaining);
            }

            if (m_OclwpySMs)
            {
                CHECK_RC(SelectSMsToOclwpy());
                CHECK_RC(OclwpySMs());
            }

            // If SM restriction is enabled, always vacate SMs on premature exits
            DEFERNAME(vacate)
            {
                if (m_OclwpySMs)
                {
                    VacateSMs();
                }
            };

            MASSERT(loopsRemaining >= numBatchLoops);

            CHECK_RC(m_Events[EvBeginBatch].Record());
            for (UINT32 batchLoop = 0; batchLoop < numBatchLoops && running; batchLoop++)
            {
                // Launch GEMM
                const UINT64 A = CalcMatrixAPtr();
                const UINT64 B = CalcMatrixBPtr();
                const UINT64 C = CalcMatrixCPtr();
                CHECK_RC(RunGemm(A, B, C, false));

                totalLoopsSoFar++;
                scaleFactor++;
                loopsRemaining--;
                opCount += CalcOpCount(m_Msize, m_Nsize, m_Ktrue, false);
                double timeMsSoFarAfterLoop =
                    timeMsSoFar + estTimeMsForOneLoop * (batchLoop + 1);

                // Check if a perpetual run has been ordered to stop
                if (bPerpetualRun && !m_KeepRunning)
                {
                    running = false;
                    break;
                }

                // Callwlate whether we may have surpassed the requested runtime
                // based on our estimate runtime of a single kernel run
                if (!bPerpetualRun && timed && IsWorkloadConstant() &&
                    TestRunTimeExceeded(timeMsSoFarAfterLoop))
                {
                    // We check the timers after breaking out to actually set running=false
                    break;
                }
            }

            // If we're still in the process of targeting fmax, sample
            // gpcclk target Pll value after each batch
            if (m_TargetingFmax && !doneSamplingPll)
            {
                UINT64 lwrrTargetPLL = 0;
                CHECK_RC(GetLwdaInstance().Synchronize());
                CHECK_RC(QueryTargetPllkHz(&lwrrTargetPLL));
                m_TargetPllkHzSamples.push_back(lwrrTargetPLL);
            }

            if (m_BatchUs || (m_TestMode == MODE_PULSE))
            {
                m_PrevK = m_Ktrue;
                m_PrevNumLoops = numBatchLoops;
            }

            // Check remaining runtime
            CHECK_RC(m_Events[EvEnd].Record());
            CHECK_RC(m_Events[EvEnd].Synchronize());
            timeMsSoFar = m_Events[EvEnd].TimeMsElapsedSinceF(m_Events[EvBeginGemm]);

            // Explicitly vacate SMs to check for RC
            if (m_OclwpySMs)
            {
                vacate.Cancel();
                CHECK_RC(VacateSMs());
            }

            if (!bPerpetualRun)
            {
                if (timed)
                {
                    if (TestRunTimeExceeded(timeMsSoFar))
                    {
                        running = false;
                    }
                    // It's possible that we'll overrun once, since RuntimeMs is
                    // a target for how long we want to run, but not a guarantee
                    EndLoopMLE(min(static_cast<UINT64>(timeMsSoFar), static_cast<UINT64>(m_RuntimeMs)));
                }
                else
                {
                    EndLoopMLE(totalLoopsSoFar);
                }
            }

            // Optional delay before result verification
            if (m_BatchDelayUs)
            {
                CHECK_RC(m_DelayFunc.Launch(static_cast<UINT64>(m_BatchDelayUs * 1000.0)));
            }

            // Check results
            if (m_VerifyResults)
            {
                RC verifyRc;

                if (UseReferenceData())
                {
                    // For IGEMM / IMMAgemm:
                    // Scale reference data by number of loops (including init),
                    // then reset it after checking so that it can be scaled again next time
                    CHECK_RC(ScaleReferenceData(scaleFactor));
                    verifyRc = Verify();

                    // If there was a miscompare, reset C with the reference data,
                    // so these miscompares do not affect the next batch
                    if (verifyRc == RC::GOLDEN_VALUE_MISCOMPARE)
                    {
                        const Lwca::DeviceMemory& devMem =
                            (m_TestMode == MODE_WALKING) ? m_Data : m_ReferenceData;
                        CHECK_RC(m_Data.Set(CalcMatrixCOffset(),
                                            devMem, CalcRefMatrixCOffset(),
                                            CalcMatrixCMaxBytes()));
                    }
                    CHECK_RC(ResetReferenceData());
                }
                else
                {
                    // Compute test CRC
                    CHECK_RC(CallwlateCrc(CalcMatrixCPtr(), CalcDataCrcBufPtr()));

                    // For IGEMM / IMMAgemm:
                    // Reset (regenerate) C Matrix and scale result
                    // Callwlate new reference CRC
                    CHECK_RC(ResetReferenceData());
                    CHECK_RC(ScaleReferenceData(scaleFactor));

                    // Verify
                    verifyRc = Verify();
                }
                returnRc = verifyRc;
                totalChecksSoFar++;

                // Print error and break early if failed
                if (verifyRc != RC::OK)
                {
                    UINT32 lwrrentLoop = loops - (loopsRemaining + 1);
                    // Re-attach threads since the JSON function isn't thread safe.
                    Tasker::AttachThread attach;
                    if (verifyRc == RC::GOLDEN_VALUE_MISCOMPARE)
                    {
                        Printf(Tee::PriError,
                               "LwdaLinpack miscompared with M=%u, N=%u, K=%u, Loop=%u\n",
                               m_Msize, m_Nsize, m_Ktrue, lwrrentLoop);
                    }
                    else
                    {
                        Printf(Tee::PriError,
                               "LwdaLinpack failure (non-miscompare) with "
                               "M=%u, N=%u, K=%u, Loop=%u\n",
                               m_Msize, m_Nsize, m_Ktrue, lwrrentLoop);
                    }
                    GetJsonExit()->AddFailLoop(lwrrentLoop);
                    if (timed)
                    {
                        UINT64 time = min(static_cast<UINT64>(timeMsSoFar),
                                          static_cast<UINT64>(m_RuntimeMs));
                        PrintErrorUpdate(time, returnRc);
                    }
                    else
                    {
                        PrintErrorUpdate(totalLoopsSoFar, returnRc);
                    }
                    if (GetGoldelwalues()->GetStopOnError())
                    {
                        running = false;
                        break;
                    }
                }
            }
            CHECK_RC(m_Events[EvEndCheck].Record());
            CHECK_RC(m_Events[EvEndCheck].Synchronize());
            Tasker::Yield();

            // Regenerate new matrices if needed
            if (m_NumNewMatrices && (totalLoopsSoFar % newMatricesStepSize == 0))
            {
                const UINT32 newMatrixNum =
                    (totalLoopsSoFar / newMatricesStepSize) % m_NumNewMatrices;
                VerbosePrintf("New Matrix Num: %d\n", newMatrixNum);
                CHECK_RC(FillABMatrices(newMatrixNum));
                CHECK_RC(GenerateCMatrices(&estTimeMsForOneLoop));
                scaleFactor = 1;

                opCount += CalcOpCount(m_Msize, m_Nsize, m_Ktrue, true);
                if (UseReferenceData())
                {
                    opCount += CalcOpCount(m_Msize, m_Nsize, m_Ktrue, true);
                }
            }


            if (running && (m_BatchUs || (m_TestMode == MODE_PULSE)))
            {
                // time taken for batch
                m_PrevBatchMs = m_Events[EvEnd].TimeMsElapsedSinceF(m_Events[EvBeginBatch]);
                // time taken for each gemm
                double gemmUs = (m_PrevBatchMs * 1000.0 / double(numBatchLoops)) - m_LaunchDelayUs;
                // time taken for check kernels
                const double checkMs = m_Events[EvEndCheck].TimeMsElapsedSinceF(m_Events[EvEnd]);
                VerbosePrintf("Pulse metrics: K %d | loops %d | batchUs %5.2f | gemmUs %5.2f | checkUs %5.2f\n",
                              m_PrevK, m_PrevNumLoops, m_PrevBatchMs * 1000.0, gemmUs, checkMs * 1000.0);

                if (m_TestMode == MODE_PULSE)
                {
                    if (m_ShmooPulse)
                    {
                        // Initialize PulseStepUs and BatchUs if this is the first loop
                        if (!m_PulseStepUs)
                        {
                            m_PulseStepUs = gemmUs;
                            VerbosePrintf("Setting PulseStepUs to %5.2f\n", m_PulseStepUs);
                        }
                        if (!m_BatchUs)
                        {
                            // Keep the batch lengths the same
                            m_BatchUs = m_PrevBatchMs * 1000;
                            VerbosePrintf("Setting BatchUs to %5.2f\n", m_BatchUs);
                        }
                        m_PulseUs += m_PulseStepUs;
                        if (m_UsePulseUsRange)
                        {
                            m_PulseUs = max(m_PulseUs, m_MinPulseUs);
                            m_PulseUs = m_PulseUs > m_MaxPulseUs ? m_MinPulseUs : m_PulseUs;
                        }
                    }
                    else if (!m_RandomPulse)
                    {
                        // Check if PulseUs was provided, if not set to a default
                        // based on the first run. The first runs are always longer, so
                        // target half the gemmUs to make sure we don't exceed MaxK.
                        if (!m_PulseUs)
                        {
                            m_PulseUs = gemmUs / 2;
                        }
                    }
                }
            }
        }
        MASSERT(!running || loopsRemaining == 0);

        // Exit if we aren't timed and reached the end of the run
        if (fixedNumLoops && (stepIdx + 1 == steps.size()))
        {
            running = false;
        }
    }

    // Didn't have enough time/loops available to sample adequately
    //
    // If we're targeting fmax in deployment down the line, we should
    // warn the user, but continue to run the test with the best known parameters.
    if (m_TargetingFmax && !doneSamplingPll)
    {
        Printf
        (
            Tee::PriError,
            "Could not finish sampling for targeting fmax. "
            "Try increasing %s\n",
            timed ? "RuntimeMs" : "TestConfiguration.Loops"
        );
        return RC::CANNOT_MEET_WAVEFORM;
    }

    // Record event indicating end of the test
    CHECK_RC(m_Events[EvEnd].Record());
    CHECK_RC(m_Events[EvEnd].Synchronize());
    if (bClocksSupported)
    {
        CHECK_RC(pPerf->MeasureAvgClkCount(Gpu::ClkGpc, &clkEnd));
    }
    StopCpuStress();

    // log time and perf values
    const double timeMs = m_Events[EvEnd].TimeMsElapsedSinceF(m_Events[EvBeginGemm]);
    if (timeMs > 0.0f)
    {
        const double gflops = static_cast<double>(1e-6 * opCount) / timeMs;

        // Check Lower and Upper perf bounds if specified
        if (m_GflopsLowerBound && gflops < m_GflopsLowerBound)
        {
            Printf(Tee::PriError, "Error: Perf (%.3f GFLOPS) < Lower Bound (%.3f GFLOPS)\n",
                   gflops, m_GflopsLowerBound);
            returnRc = RC::UNEXPECTED_TEST_PERFORMANCE;
        }
        if (m_GflopsUpperBound && gflops > m_GflopsUpperBound)
        {
            Printf(Tee::PriError, "Error: Perf (%.3f GFLOPS) > Upper Bound (%.3f GFLOPS)\n",
                   gflops, m_GflopsUpperBound);
            returnRc = RC::UNEXPECTED_TEST_PERFORMANCE;
        }

        // Print metrics on LWCA kernels launched
        const string gpuName = GetBoundGpuSubdevice()->GpuIdentStr();
        VerbosePrintf("GEMM  kernels launched: %llu\n"
                      "Check kernels launched: %llu\n",
                      totalLoopsSoFar, totalChecksSoFar);

        if (m_PrintPerf)
        {
            bool bPrintArchEff = bClocksSupported || Platform::UsesLwgpuDriver();

            if (Platform::UsesLwgpuDriver())
            {
                UINT64 gpcFreqHz = 0;
                const RC clockRc = CheetAh::SocPtr()->GetAvgGpcFreqHz(&gpcFreqHz);
                if (clockRc == RC::OK)
                {
                    clkBegin = 0;
                    clkEnd   = static_cast<UINT64>(gpcFreqHz * (timeMs / 1e3));
                }
                else
                {
                    bPrintArchEff = false;
                }
            }

            // Callwlate maximum possible GOPS
            //
            // Consider the issue-rate of the GEMM instruction (archEfficiency * issueRate),
            // the number of TPCs, and the measured number of clocks the test ran for.
            //
            // Then divide by the time the test ran for to get the rate (GOPS)
            //
            if (bPrintArchEff)
            {
                const double numClocks = static_cast<double>(clkEnd - clkBegin);
                const double archEfficiency = GetBoundGpuSubdevice()->GetArchEfficiency(m_Instr);
                const double issueRate = GetBoundGpuSubdevice()->GetIssueRate(m_Instr);
                const double archOpCount =
                    archEfficiency * issueRate * GetBoundLwdaDevice().GetShaderCount() * numClocks;
                const double archGops = (archOpCount / 1e9) / (timeMs / 1e3);

                Printf(Tee::PriDebug, "Callwlating arch efficiency on %s:\n", gpuName.c_str());
                Printf(Tee::PriDebug, "    numClocks       %.3f\n", numClocks);
                Printf(Tee::PriDebug, "    shaderCount     %u\n",   GetBoundLwdaDevice().GetShaderCount());
                Printf(Tee::PriDebug, "    Arch efficiency %.3f\n", archEfficiency);
                Printf(Tee::PriDebug, "    issueRate       %.3f\n", issueRate);
                Printf(Tee::PriDebug, "    archOpCount     %.3f\n", archOpCount);
                Printf(Tee::PriDebug, "    timeMs          %.3f\n", timeMs);
                Printf(Tee::PriDebug, "    archGops        %.3f\n", archGops);

                // Print efficiency of kernel
                if (archGops > 0)
                {
                    VerbosePrintf("Arch efficiency %.3f, issue rate 1/%.0f\n",
                                  archEfficiency, 1.0 / issueRate);
                    VerbosePrintf("Arch max possible GOPS = %.4f\n",
                                  archGops);
                    Printf(Tee::PriNormal,
                           "%s: %.1f%% percent of raw SM throughput\n",
                           gpuName.c_str(), gflops * 100.0 / archGops);
                }
                if (m_TargetingFmax)
                {
                    const FLOAT64 timeMsPostSampling =
                        m_Events[EvEnd].TimeMsElapsedSinceF(m_Events[EvEndSampling]);

                    // Print average gpcclk after our pulse optimization effort
                    const FLOAT64 numClockPostSampling =
                        static_cast<FLOAT64>(clkEnd - clkCntPostSampling);
                    Printf(Tee::PriNormal, "%s: (Post-Sampling) Avg gpcclk = %.2f MHz\n",
                           gpuName.c_str(), (numClockPostSampling / timeMsPostSampling) / 1e3);

                    // Print runtime and gflops count after our pulse optimization effort
                    const UINT64 opCountPostSampling = opCount - opCountDuringSampling;
                    const double gflopsPostSampling =
                        static_cast<double>(1e-6 * opCountPostSampling) / timeMsPostSampling;
                    Printf(Tee::PriNormal, "%s: (Post-Sampling) Time taken = %f ms, GFLOPS = %.3f\n",
                           gpuName.c_str(), timeMsPostSampling, gflopsPostSampling);
                }
                // Print average gpcclk (helps show how much power-capping is kicking in)
                if (numClocks > 0)
                {
                    Printf(Tee::PriNormal, "%s: Avg gpcclk = %.2f MHz\n",
                           gpuName.c_str(), (numClocks / timeMs) / 1e3);
                }
            }

            // Print total runtime and perf
            Printf(Tee::PriNormal, "%s: Time taken = %f ms, GFLOPS = %.3f\n",
                   gpuName.c_str(), timeMs, gflops);

            GetJsonExit()->SetField("FLOPS", gflops);
            GetJsonExit()->SetField("TotalTime", timeMs);
        }

        if (m_RecordPerfMetric)
        {
            // Monitor perf deviation in DVS
            CHECK_RC(SetPerformanceMetric(ENCJSENT("GFlops"), gflops));
        }
    }

    // Indicate to Progress tracker that we've reached the end of test
    if (!bPerpetualRun)
    {
        if (timed)
        {
            PrintProgressUpdate(m_RuntimeMs + 1);
        }
        else
        {
            PrintProgressUpdate(maxIterations + 1);
        }
    }

    // Report failing Smids that pass the threshold of errors
    if (m_ReportFailingSmids)
    {
        CHECK_RC(ReportFailingSmids(m_SmidMiscompareCount, m_SmidFailureLimit));
    }
    return returnRc;
}

RC LwdaLinpack::GenerateCMatrices(double *pEstTimeMsForOneLoop)
{
    MASSERT(pEstTimeMsForOneLoop);

    RC rc;

    const UINT64 A = CalcMatrixAPtr();
    const UINT64 B = CalcMatrixBPtr();
    const UINT64 C = CalcMatrixCPtr();
    const UINT64 refC = CalcRefMatrixCPtr();

    // Initialize reference matrix if in use
    //
    // This is the first kernel launch of LwdaLinpack, so in LwdaLinpackCask variants
    // it may take longer since the LWCA runtime lazily loads LWCA modules upon first use
    // (That's why we don't track how long this kernel takes to run)
    if (UseReferenceData())
    {
        if (m_NaiveInit)
        {
            CHECK_RC(RunNaiveGemm(A, B, refC));
        }
        else
        {
            CHECK_RC(RunGemm(A, B, refC, true));
        }
    }

    // Initialize result Matrix C = A*B, also track how much time it takes to do a loop
    CHECK_RC(m_Events[EvBeginTimeCalc].Record());
    CHECK_RC(RunGemm(A, B, C, true));

    // Measure expected time per loop only once
    CHECK_RC(m_Events[EvEnd].Record());
    CHECK_RC(m_Events[EvEnd].Synchronize());
    *pEstTimeMsForOneLoop =
        m_Events[EvEnd].TimeMsElapsedSinceF(m_Events[EvBeginTimeCalc]);

    // If we are using the CRC check, generate the reference CRC from the C matrix
    if (!UseReferenceData())
    {
        CHECK_RC(CallwlateCrc(C, CalcRefCrcBufPtr()));
    }

    // Dump matrices
    if (UseReferenceData())
    {
        CHECK_RC(DumpMatrices(true));
    }
    CHECK_RC(DumpMatrices(false));

    return rc;
}

//-------------------------------------------------------------------------------------------------
//! \brief Perform Cleanup
//!
RC LwdaLinpack::Cleanup()
{
    RC rc;
    if (m_SynchronousMode || m_LaunchDelayUs || m_CheckLoops || m_RandomPulse)
    {
        m_ModuleGpuSync.Unload();
    }

    if (m_SynchronousMode)
    {
        // Sync all conlwrrent devices to prevent races on freeing shared host memory
        Tasker::WaitOnBarrier();
        const UINT32 gpuIdx = GetBoundGpuSubdevice()->GetGpuInst();
        if (gpuIdx == 0)
        {
            s_GpuSync.Free();
            s_TimeoutMatrix.Free();
        }
    }

    // Reset M/N to 0 so it doesn't affect the next run.
    if (m_MNKMode == MNK_WAVE)
    {
        m_Msize = 0;
        m_Nsize = 0;
    }

    m_SMOclwpancyStream.Free();
    m_HostAllBlocksScheduled.Free();
    m_HostOclwpySMFlags.Free();

    // Free memory allocated to m_RangeSMIDs
    vector<UINT32>().swap(m_RangeSMIDs);

    m_ReferenceData.Free();
    m_ComparisonResult.Free();
    m_MiscompareCount.Free();

    m_SmidMap.Free();
    m_HostSmidMap.Free();

    m_RefDataCrcBuf.Free();
    m_DataCrcBuf.Free();
    m_DevCrcTableBuf.Free();
    m_CompareCrcBuf.Free();

    m_HostComparisonResult.Free();
    m_ZeroMemory.Free();
    m_Data.Free();

    m_HostMiscompareCount.Free();
    m_ModuleLwSurf.Unload();

    for (auto& event : m_Events)
    {
        event.Free();
    }
    m_Module.Unload();
    m_ModuleGemm.Unload();

    m_Kset.clear();

    CHECK_RC(LwdaStreamTest::Cleanup());

    if (m_MemTmpRange.first != std::numeric_limits<INT32>::max() &&
        m_MemTmpRange.second != std::numeric_limits<INT32>::max())
    {
        VerbosePrintf("Setting Back Global MemTmpRange: [%d, %d]\n", m_GlobalMemTmpRange.first,
                m_GlobalMemTmpRange.second);

        CHECK_RC(GetBoundGpuSubdevice()->GetThermal()->SetOverTempRange(
                OverTempCounter::MEMORY_TEMP, m_GlobalMemTmpRange.first, m_GlobalMemTmpRange.second));
    }
    m_MemTmpRange = make_pair(std::numeric_limits<INT32>::max(),
            std::numeric_limits<INT32>::max());
    m_GlobalMemTmpRange = make_pair(std::numeric_limits<INT32>::max(),
            std::numeric_limits<INT32>::max());

    if (m_GpuTmpRange.first != std::numeric_limits<INT32>::max() &&
        m_GpuTmpRange.second != std::numeric_limits<INT32>::max())
    {
        VerbosePrintf("Setting Back Global GpuTmpRange: [%d, %d]\n", m_GlobalGpuTmpRange.first,
                m_GlobalGpuTmpRange.second);

        CHECK_RC(GetBoundGpuSubdevice()->GetThermal()->SetOverTempRange(
                OverTempCounter::INTERNAL_TEMP, m_GlobalGpuTmpRange.first, m_GlobalGpuTmpRange.second));
    }
    m_GpuTmpRange = make_pair(std::numeric_limits<INT32>::max(),
            std::numeric_limits<INT32>::max());
    m_GlobalGpuTmpRange = make_pair(std::numeric_limits<INT32>::max(),
            std::numeric_limits<INT32>::max());

    if (m_pDutyPctBinSearcher)
    {
        m_pDutyPctBinSearcher.release();
    }
    if (m_pPulseUsBinSearcher)
    {
        m_pPulseUsBinSearcher.release();
    }
    if (m_pSMCountBinSearcher)
    {
        m_pSMCountBinSearcher.release();
    }
    vector<UINT64>().swap(m_TargetPllkHzSamples);

    return OK;
}

//-------------------------------------------------------------------------------------------------
//! \brief Scale reference data
//! A placeholder function that performs a no-op.
//! This function is overriden in IGEMM as it is only needed for that
//!
RC LwdaLinpack::ScaleReferenceData(UINT32 scale)
{
    return OK;
}

//-------------------------------------------------------------------------------------------------
//! \brief Reset reference data
//! A placeholder function that performs a no-op.
//! This function is overriden in IGEMM as it is only needed for that
//!
RC LwdaLinpack::ResetReferenceData()
{
    return OK;
}

bool LwdaLinpack::TestRunTimeExceeded(double timeSoFarMs)
{
    return timeSoFarMs >= static_cast<double>(m_RuntimeMs);
}

//-----------------------------------------------------------------------------
//! \brief Writes the CRC table used by CallwlateCrc to the GPU
//!
RC LwdaLinpack::SetCrcTable()
{
    RC rc;
    vector<UINT32> crcTable;
    Crc::GetCRCTable(&crcTable);

    if (m_TestMode == MODE_WALKING)
    {
        CHECK_RC(m_Data.Set(&crcTable[0],
                            m_OtherDataOffset + m_CrcTableBufSubOffset,
                            Crc::TableSizeBytes()));
    }
    else
    {
        CHECK_RC(m_DevCrcTableBuf.Set(&crcTable[0], Crc::TableSizeBytes()));
    }

    return rc;
}

//-------------------------------------------------------------------------------------------------
//! \brief Callwlates CRC and stores the results in crcBuf
//! This implementation spawns multiple threads on GPU and each thread computes CRC on a portion of
//! data. Computed CRCs are stored in FB and accessible using crcBuf handle
RC LwdaLinpack::CallwlateCrc
(
    UINT64 dataPtr,
    UINT64 crcBufPtr
)
{
    RC rc;
    UINT64 size = NumElemsToBytes(m_KernParams.typeofOutputElement, m_OutputBufferLen);

    MASSERT(m_UseCrcToVerify);
    if (size % sizeof(UINT64))
    {
        Printf(Tee::PriError, "Kernel only supports surfaces that are multiple of 8.\n");
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(m_ComputeCrcFunc.Launch(
                 dataPtr,
                 CalcCrcTableBufPtr(),
                 crcBufPtr,
                 size / sizeof(UINT64)));
    CHECK_RC(GetLwdaInstance().Synchronize());

    return rc;
}

//-------------------------------------------------------------------
//! \brief Compares CRCs
//! Compares CRCs generated from reference matrix and computed matrix
RC LwdaLinpack::CompareCrcs(UINT64 crcBuf1Ptr,
                            UINT64 crcBuf2Ptr,
                            bool* pResult)
{
    RC rc;
    MASSERT(pResult);
    MASSERT(m_UseCrcToVerify);

    const UINT64 size = CrcBufSize();

    if (size % sizeof(UINT64))
    {
        Printf(Tee::PriError, "Kernel only supports surfaces that are multiple of 8.\n");
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(m_CompareCrcFunc.Launch(
                 crcBuf1Ptr,
                 crcBuf2Ptr,
                 CalcCompareCrcBufPtr(),
                 size / sizeof(UINT64),
                 ~0ULL));

    CHECK_RC(GetLwdaInstance().Synchronize());

    if (m_TestMode == MODE_WALKING)
    {
        CHECK_RC(m_Data.Get(m_HostComparisonResult.GetPtr(),
                            m_OtherDataOffset + m_CompareCrcSubOffset,
                            m_HostComparisonResult.GetSize()));
    }
    else
    {
        CHECK_RC(m_CompareCrcBuf.Get(m_HostComparisonResult.GetPtr(),
                                     m_HostComparisonResult.GetSize()));
    }
    CHECK_RC(GetLwdaInstance().Synchronize());

    for (UINT32 i = 0; i < m_HostComparisonResult.GetSize() / sizeof(UINT32); i++)
    {
        if (static_cast<UINT32*>(m_HostComparisonResult.GetPtr())[i])
        {
            *pResult = false;
            Printf( Tee::PriError, "Mismatch detected at index %u\n", i);
            break;
        }
    }

    return rc;
}

//---------------------------------------------------------------------------------
//! \brief Debug utility function used to inject an error value at desired location
//!
RC LwdaLinpack::InjectError(Lwca::DeviceMemory &deviceMem,
                            UINT64 location,
                            UINT64 errorValue)
{
    RC rc;
    Lwca::Function injectErrorFunc;
    UINT64 size = NumElemsToBytes(m_KernParams.typeofOutputElement, m_OutputBufferLen);

    MASSERT(m_UseCrcToVerify);

    injectErrorFunc = m_ModuleLwSurf["InjectError"];
    CHECK_RC(injectErrorFunc.InitCheck());
    injectErrorFunc.SetGridDim(1);
    injectErrorFunc.SetBlockDim(GetBoundLwdaDevice().GetAttribute
                 (LW_DEVICE_ATTRIBUTE_WARP_SIZE));

    //The kernel writes a 64 bit value.
    CHECK_RC(injectErrorFunc.Launch(deviceMem.GetDevicePtr(),
                          size/sizeof(UINT64),
                          location,
                          errorValue));

    CHECK_RC(GetLwdaInstance().Synchronize());
    return rc;
}

Tasker::ThreadID LwdaLinpack::CreateCpuStressThread(UINT32 threadIdx)
{
    // Note on the use of atomic operations
    // ====================================
    //
    // We use relaxed ordering whenever we can.  This reduces the effect
    // on CPU caches by avoiding unnecessary flushes.  We can do this because
    // all the atomic variables are generally independent, unless otherwise
    // noted.
    //
    return Tasker::CreateThread("CpuStress", [threadIdx]
    {
        // GPU 0 writes a value to this location to tell us whether it is
        // running or not.  The value is 0 if the GPU is idle (e.g. waiting
        // for other GPUs or in delay function) and 1 if the GPU is active.
        const atomic<UINT32>& gpuSync = *reinterpret_cast<atomic<UINT32>*>(
                static_cast<UINT08*>(s_GpuSync.GetPtr()) + MAX_SYNC_DEVICES * sizeof(UINT64));

        Tasker::DetachThread detach;

        // Initialize CPU stress
        const auto cpuStress = MakeCpuStress();
        if (!cpuStress->IsSupported())
        {
            Printf(Tee::PriError, "CPU stress test not supported on this CPU\n");
            s_CpuStressState.store(CSUnsupported, memory_order_relaxed);
            return;
        }
        if (cpuStress->Setup() != RC::OK)
        {
            Printf(Tee::PriError, "CPU stress test setup failed\n");
            s_CpuStressState.store(CSFailed, memory_order_relaxed);
            return;
        }
        DEFER
        {
            cpuStress->Cleanup();
        };

        UINT64 numCpuLoops = 0;
        UINT64 numGpuLoops = 0;
        bool   gpuIsActive = false;

        // Loop until any CPU thread exits or Kill is requested
        while (s_CpuStressState.load(memory_order_relaxed) == CSGo)
        {
            // Wait for GPU to become active
            if (gpuSync.load(memory_order_relaxed) == 0)
            {
                gpuIsActive = false;
                Cpu::Pause();
                continue;
            }
            if (!gpuIsActive)
            {
                ++numGpuLoops;
                gpuIsActive = true;
            }

            if (cpuStress->Run() != RC::OK)
            {
                s_CpuStressState.store(CSFailed, memory_order_relaxed);
                break;
            }

            ++numCpuLoops;
        }

        Printf(Tee::PriLow,
               "CPU stress thread %u completed %llu loops, detected %llu loops on GPU 0\n",
               threadIdx, numCpuLoops, numGpuLoops);
    });
}

RC LwdaLinpack::StartCpuStress()
{
    MASSERT(m_CpuThreads && GetBoundGpuSubdevice()->GetGpuInst() == 0);
    MASSERT(m_CpuStressThreads.empty());

    s_CpuStressState = CSGo;

    VerbosePrintf("Launching %u threads with CPU stress\n", m_CpuThreads);

    for (UINT32 i = 0; i < m_CpuThreads; i++)
    {
        const auto threadId = CreateCpuStressThread(i);

        if (threadId == Tasker::NULL_THREAD)
        {
            StopCpuStress();
            Printf(Tee::PriError, "Failed to create threads\n");
            return RC::LWRM_OPERATING_SYSTEM;
        }

        m_CpuStressThreads.push_back(threadId);
    }
    return RC::OK;
}

RC LwdaLinpack::StopCpuStress()
{
    const auto state = s_CpuStressState.exchange(CSKill, memory_order_relaxed);

    if (m_CpuStressThreads.empty())
    {
        return RC::OK;
    }

    Printf(Tee::PriLow, "Stopping %u CPU stress threads\n",
           static_cast<unsigned>(m_CpuStressThreads.size()));

    while (!m_CpuStressThreads.empty())
    {
        const auto threadId = m_CpuStressThreads.back();
        m_CpuStressThreads.pop_back();

        Tasker::Join(threadId);
    }

    switch (state)
    {
        case CSUnsupported:
            return RC::UNSUPPORTED_HARDWARE_FEATURE;

        case CSFailed:
            return RC::HW_ERROR;

        default:
            break;
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
//! Begin BinarySearcher method implementations

bool LwdaLinpack::BinarySearcher::HasSearchStarted()
{
    return m_SearchStarted;
}

size_t LwdaLinpack::BinarySearcher::StartAndGetStartingIdx()
{
    MASSERT(!m_SearchStarted);

    // Callwlate the mid index. If the search range is even, get the lower index
    // of the two middle indices.
    m_Mid = ALIGN_DOWN((m_Low + m_High) / 2, m_StepSize);
    MASSERT(m_Mid >= m_Low && m_Mid <= m_High);

    m_SearchStarted = true;
    return m_Mid;
}

size_t LwdaLinpack::BinarySearcher::RunNextStep(bool searchRight, BinSearchResult *pResult)
{
    // If arr[m_Mid] is true, mark it as our right-most index at which the
    // the array element is true
    if (searchRight)
    {
        m_TrueElemExists = true;
        m_RightMostTrueIdx = m_Mid;
    }

    // Narrow our search range from the left if arr[m_Mid] is true, otherwise
    // narrow our search range from the right if arr[m_Mid] is false
    if (searchRight)
    {
        m_Low = m_Mid + m_StepSize;
    }
    else
    {
        m_High = m_Mid - m_StepSize;
    }

    // If low > high, we've completed our binary search and we can return
    // our right-most index at which the element is true. If no such element
    // exists, signify that there were no true elments
    if (m_Low > m_High)
    {
        if (m_TrueElemExists)
        {
            *pResult = BinSearchResult::TARGET_FOUND;
            MASSERT(m_RightMostTrueIdx <= m_High);
            return m_RightMostTrueIdx;
        }
        else
        {
            *pResult = BinSearchResult::TARGET_ABSENT;
            return m_Mid;
        }
    }

    m_Mid = ALIGN_DOWN((m_Low + m_High) / 2, m_StepSize);
    MASSERT(m_Mid >= m_Low && m_Mid <= m_High);

    *pResult = BinSearchResult::STILL_SEARCHING;
    return m_Mid;
}

// Resets the binary searcher range to its initial range and mark the searcher
// as unstarted
void LwdaLinpack::BinarySearcher::Reset()
{
    m_Low = m_InitialLow;
    m_High = m_InitialHigh;
    m_SearchStarted = false;
}

//! End BinarySearcher method implementations
//------------------------------------------------------------------------------

JS_CLASS_VIRTUAL_INHERIT(LwdaLinpack, LwdaStreamTest, "Run the Lwca DGEMM stress test.");

void LwdaLinpack::PropertyHelp(JSContext *pCtx, regex *pRegex)
{
    JSObject* pLwrJObj = JS_NewObject(pCtx,&LwdaLinpack_class,0,0);
    JS_SetPrivate(pCtx,pLwrJObj,this);
    Help::JsPropertyHelp(pLwrJObj,pCtx,pRegex,0);
    JS_SetPrivate(pCtx,pLwrJObj,nullptr);
    LwdaStreamTest::PropertyHelp(pCtx,pRegex);
}

CLASS_PROP_READWRITE(LwdaLinpack, Alpha, double, "Constant Alpha");
CLASS_PROP_READWRITE(LwdaLinpack, Beta, double, "Constant Beta");
CLASS_PROP_READWRITE(LwdaLinpack, Msize, UINT32, "Matrix dimension M");
CLASS_PROP_READWRITE(LwdaLinpack, Nsize, UINT32, "Matrix dimension N");
CLASS_PROP_READWRITE(LwdaLinpack, Ksize, UINT32, "Matrix dimension K");
CLASS_PROP_READWRITE(LwdaLinpack, Kstart, UINT32,
                         "Start of Kshmoo (for TestMode=MODE_SHMOO)");
CLASS_PROP_READWRITE(LwdaLinpack, Kstep, UINT32,
                         "Amount to increase K by in each step of Kshmoo (for TestMode=MODE_SHMOO)");
CLASS_PROP_READWRITE(LwdaLinpack, UseLegacyKshmoo, bool,
                         "Use the old algorithm for distributing loops among the Kshmoo steps (for TestMode=MODE_SHMOO)");
CLASS_PROP_READWRITE(LwdaLinpack, MaxKsizeForPulse, UINT32, "Max Matrix dimension K for pulsing");
CLASS_PROP_READWRITE(LwdaLinpack, MinPulseUs, double, "Minimum pulse duration in microseconds for shmoo");
CLASS_PROP_READWRITE(LwdaLinpack, MaxPulseUs, double, "Maximum pulse duration in microseconds for shmoo");
CLASS_PROP_READWRITE(LwdaLinpack, TestMode, UINT32, "Select test mode: 0=Normal, 1=Pulse, 2=CS (obsolete), or 3=Walking"); //$
CLASS_PROP_READWRITE(LwdaLinpack, MNKMode, UINT32, "Select MNK Alignment mode: 0=Error, 1=Align, 2=Permit, 3=Wave"); //$
CLASS_PROP_READWRITE(LwdaLinpack, NumWaves, UINT32,
                        "Number of waves per kernel launch in MNK Wave mode");
CLASS_PROP_READWRITE(LwdaLinpack, NumFracs, UINT32,
                        "Number of fractional units per kernel launch in MNK Wave mode");
CLASS_PROP_READWRITE(LwdaLinpack, MaxDataDump, UINT32, "Maximum amount of values to dump.");
CLASS_PROP_READWRITE(LwdaLinpack, RuntimeMs, UINT32,
                        "Maximum amount of time the test can run for");
CLASS_PROP_READWRITE(LwdaLinpack, NumNewMatrices, UINT32,
                        "Number of new matrices to generate while looping");
CLASS_PROP_READWRITE(LwdaLinpack, VerifyResults, bool, "Verify results or not");
CLASS_PROP_READWRITE(LwdaLinpack, NaiveInit, bool, "Initialize reference matrix using a simple (slow) version of GEMM");
CLASS_PROP_READWRITE(LwdaLinpack, DumpMiscompares, bool,
                        "Enable/Disable printing miscompares found in LwdaLinpack");
CLASS_PROP_READWRITE(LwdaLinpack, DumpMatrixData, bool,
                        "Enable/Disable printing matrix contents");
CLASS_PROP_READWRITE(LwdaLinpack, SmidFailureLimit, UINT64,
                        "Lower bound of miscompares at which to report an SM as failing");
CLASS_PROP_READWRITE(LwdaLinpack, ReportFailingSmids, bool,
                        "Report which SMID and HW TPC computes failing matrix values");
CLASS_PROP_READWRITE(LwdaLinpack, PrintPerf, bool, "Enable/Disable printing"
        " perf values in mods log file");
CLASS_PROP_READWRITE(LwdaLinpack, RecordPerfMetric, bool, "Enable/Disable recording the"
        " performance metric for GFLOPS");
CLASS_PROP_READWRITE(LwdaLinpack, GflopsLowerBound, double, "Lower bound for"
        " test GFLOPS performance, separate check from CheckPerf");
CLASS_PROP_READWRITE(LwdaLinpack, GflopsUpperBound, double, "Upper bound for"
        " test GFLOPS performance, separate check from CheckPerf");
CLASS_PROP_READWRITE(LwdaLinpack, KeepRunning, bool,
        "While this is true, Run will continue even beyond RuntimeMs.");
CLASS_PROP_READWRITE(LwdaLinpack, UseConstantData, bool, "Use constant data to fill matrices");
CLASS_PROP_READWRITE(LwdaLinpack, UseConstantDataA, bool, "Use constant data to fill matrix A");
CLASS_PROP_READWRITE(LwdaLinpack, UseConstantDataB, bool, "Use constant data to fill matrix B");
CLASS_PROP_READWRITE(LwdaLinpack, CMatrixScale, UINT32,
        "Scale the amount of memory allocated for the C Matrix, results in unused memory.");
CLASS_PROP_READWRITE(LwdaLinpack, CtaSwizzle, bool,
        "Enable/Disable kernel CTA ID swizzling (reduces L2 cache misses on runs with tall C matrices)");
CLASS_PROP_READWRITE(LwdaLinpack, MtxDataConstant, float, "constant data used in matrices.");
CLASS_PROP_READWRITE(LwdaLinpack, MtxADataConstant, float, "constant data used in matrix A.");
CLASS_PROP_READWRITE(LwdaLinpack, MtxBDataConstant, float, "constant data used in matrix B.");
CLASS_PROP_READWRITE(LwdaLinpack, MtxDataMean, float, "mean of random data used in matrices.");
CLASS_PROP_READWRITE(LwdaLinpack, MtxDataSD, float, "std dev of random data used in matrices.");
CLASS_PROP_READWRITE(LwdaLinpack, NonZeroPct, float,
                        "Percent from 0.0 to 100.0 of non-zero elements in random matrix data.");
CLASS_PROP_READWRITE(LwdaLinpack, SynchronousMode, bool,
                        "Enable/Disable synchronous launches of the kernel on multi-GPU systems.");
CLASS_PROP_READWRITE(LwdaLinpack, CpuThreads, UINT32,
                        "Number of CPU stress threads to launch.");
CLASS_PROP_READWRITE(LwdaLinpack, LaunchDelayUs, double,
                        "Time in microseconds to wait between kernel launches.");
CLASS_PROP_READWRITE(LwdaLinpack, MaxLaunchDelayUs, double,
                        "Max LaunchDelayUs picked in RandomPulse mode.");
CLASS_PROP_READWRITE(LwdaLinpack, CheckLoops, UINT32,
                        "Number of Kernels to launch as part of a batch before checking results.");
CLASS_PROP_READWRITE(LwdaLinpack, BatchDelayUs, double,
                        "This will define delay period, if any, between batches.");
CLASS_PROP_READWRITE(LwdaLinpack, UseCrcToVerify, bool,
                        "Use CRC computed instead of complete reference matrix for verification");
CLASS_PROP_READWRITE(LwdaLinpack, SkipAlphaBetaCheck, bool,
                        "Skip the check that validates the Alpha and Beta kernel parameters");
CLASS_PROP_READWRITE(LwdaLinpack, NumWalks, UINT32,
                        "Number of times to walk the framebuffer in Walking Linpack mode.");
CLASS_PROP_READWRITE(LwdaLinpack, WalkScale, double,
                        "Scaling factor of the amount walked per step in Walking Linpack mode.");
CLASS_PROP_READWRITE(LwdaLinpack, PulseUs, double,
                        "Pulse width in Pulse mode.");
CLASS_PROP_READWRITE(LwdaLinpack, PulseStepUs, double,
                        "Pulse step for ShmooPulse.");
CLASS_PROP_READWRITE(LwdaLinpack, BatchUs, double,
                        "Length of a batch of pulse/idles in Pulse mode");
CLASS_PROP_READWRITE(LwdaLinpack, DutyPct, double,
                        "Workload duty cycle in Pulse mode");
CLASS_PROP_READWRITE(LwdaLinpack, RandomPulse, bool,
                        "Randomize pulse frequencies in Pulse mode");
CLASS_PROP_READWRITE(LwdaLinpack, ShmooPulse, bool,
                        "Shmoo pulse frequencies in Pulse mode");
CLASS_PROP_READWRITE(LwdaLinpack, TargetingMode, UINT32,
                        "Select Targeting mode: 0=None, 1=Pulse-based");
CLASS_PROP_READWRITE(LwdaLinpack, PllSampleCount, UINT32,
                        "Number of times to query target Pll for gpcclk domain after each batch");
CLASS_PROP_READWRITE(LwdaLinpack, SuccessThreshholdCount, UINT32,
                        "Number of target Pll samples that must be equal to "
                        "target Pll of GPU at idle state to deem current pulse parameters good");
CLASS_PROP_READWRITE(LwdaLinpack, MaxNumSearchTries, UINT32,
                        "Max number of time to perform binary search on relevant parameters");
CLASS_PROP_READWRITE(LwdaLinpack, DutyPctSearchStepSize, UINT32,
                        "Step size when running binary search on DutyPct");
CLASS_PROP_READWRITE(LwdaLinpack, PulseUsSearchStepSize, UINT32,
                        "Step size when running binary search on PulseUs");
CLASS_PROP_READWRITE(LwdaLinpack, SMCountSearchStepSize, UINT32,
                        "Step size when running binary search on active SM count");
CLASS_PROP_READWRITE(LwdaLinpack, NumDisabledSMs, UINT32,
                        "Number of SMs to disable");
CLASS_PROP_READWRITE(LwdaLinpack, NumDisabledGPCs, UINT32,
                        "Number of GPCs to disable");
CLASS_PROP_READWRITE(LwdaLinpack, SMSelectionMode, UINT32,
                        "Method of selecting which SMs to restrict: 0=Random, 1=Randomly by GPC");
PROP_CONST(LwdaLinpack, MODE_NORMAL,  LwdaLinpack::MODE_NORMAL);
PROP_CONST(LwdaLinpack, MODE_SHMOO,   LwdaLinpack::MODE_SHMOO);
PROP_CONST(LwdaLinpack, MODE_WALKING, LwdaLinpack::MODE_WALKING);
PROP_CONST(LwdaLinpack, MODE_PULSE,   LwdaLinpack::MODE_PULSE);
PROP_CONST(LwdaLinpack, FMAX_PULSE,   static_cast<UINT32>(LwdaLinpack::TargetingMode::FMAX_PULSE));
PROP_CONST(LwdaLinpack, FMAX_SM,      static_cast<UINT32>(LwdaLinpack::TargetingMode::FMAX_SM));
