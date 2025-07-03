/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

#pragma once

#include "lwda_linpack.h"
#include "cask/cask.h"
#include "cask/cask_internal.h"
#include "cask/hardware_information.h"

extern SObject LwdaLinpackCask_Object;

class LwdaLinpackCask : public LwdaLinpack
{
    public:
        LwdaLinpackCask();
        ~LwdaLinpackCask() override {}
        bool IsSupported() override;
        virtual void TestHelp(JSContext *pCtx, regex *pRegex) override;

        SETGET_PROP(DefaultKernelName, string);
        SETGET_PROP(KernelName, string);
        SETGET_PROP(ClusterX, UINT32);
        SETGET_PROP(ClusterY, UINT32);
        SETGET_PROP(ClusterZ, UINT32);
        SETGET_PROP(PrintKernelList, bool);
        SETGET_PROP(PrintPackagedKernels, bool);
    protected:
        RC Setup() override;
        RC Cleanup() override;
        virtual RC InitCaskGemm(cask::Gemm* pGemm, bool initialize);
        struct KernelDesc
        {
            Instr instr;
            const char* naiveGemmFuncName;
        };
        using KernelMap = map<const string, const LwdaLinpackCask::KernelDesc>;
        virtual const KernelMap& GetKernelMap() const;
    private:
        static const vector<pair<const char*, const char*>> s_AliasMap;
        static const vector<set<Instr>> s_VariantTypes;
        static const map<Lwca::Capability::Enum, vector<const char*>> s_FamilyMap;
        static const map<Lwca::Capability::Enum, vector<const char*>> s_SupportedMap;
        static const KernelMap s_KernelMap;

        static Tasker::Mutex s_pCaskInitMutex;
        static bool s_ManifestInit;
        static cask::Manifest* s_pManifest;

        const char* GemmFunctionName() override;
        RC AllocateSmidBuffers() override;
        RC CopySmidMapToHost(Lwca::HostMemory& hostSmidMap) override;
        RC ValidateAlphaBeta() override;
        RC ScaleReferenceData(UINT32 scale);
        RC ResetReferenceData();

        RC InitGemmFunc() override;
        RC ConfigGemmFuncDim() override;
        RC LaunchGemmKernel(const ByteStream& paramBuffer) override;
        RC GenerateCMatrices(double *pEstTimeMsForOneLoop) override;
        RC RunGemm(UINT64 A, UINT64 B, UINT64 C, bool initialize) override;

        RC HandleCaskError(const cask::Error& caskError, const string& errorMsg, void* pHostBuf);
        #define CHECK_CASK_RC(caskError, errorMsg, pHostBuf) \
                CHECK_RC(HandleCaskError(caskError, errorMsg, pHostBuf))

        elementDataType_t GetLegacyEnumType(cask::ScalarType type);
        cask::GemmShader* FindCaskShader(string* pShaderName);
        bool KernelInstrMatchesDefault(const string& kernelName, const Tee::Priority pri);
        bool KernelInWhitelist(const string& kernelName, const Tee::Priority pri);
        RC SetCaskShader();
        RC SetupCaskParams();
        RC SetupCaskKernel();
        RC CheckHardwareInfo(const cask::HardwareInformation& hwInfo);
        RC InitCaskBuffers
        (
            cask::Gemm& caskGemm,
            cask::RunInfo* pRunInfo,
            Lwca::HostMemory* pHostBuffer,
            Lwca::DeviceMemory* pDeviceBuffer,
            Lwca::DeviceMemory* pDeviceWorkspace
        );

        cask::GemmShader* m_pCaskShader = nullptr;

        cask::Gemm m_CaskInitGemm = {};
        cask::RunInfo m_CaskInitRunInfo = {};
        Lwca::HostMemory m_CaskInitHostBuffer;
        Lwca::DeviceMemory m_CaskInitDeviceBuffer;
        Lwca::DeviceMemory m_CaskInitDeviceWorkspace;

        cask::Gemm m_CaskGemm = {};
        cask::RunInfo m_CaskRunInfo = {};
        Lwca::HostMemory m_CaskHostBuffer;
        Lwca::DeviceMemory m_CaskDeviceBuffer;
        Lwca::DeviceMemory m_CaskDeviceWorkspace;

        // Kernel Data Types
        cask::ScalarType m_TypeA = cask::ScalarType::FP32;
        cask::ScalarType m_TypeB = cask::ScalarType::FP32;
        cask::ScalarType m_TypeC = cask::ScalarType::FP32;

        // Kernel Shape
        cask::md::MatrixLayoutType m_LayoutA = cask::md::MatrixLayoutType::N;
        cask::md::MatrixLayoutType m_LayoutB = cask::md::MatrixLayoutType::N;
        cask::md::MatrixLayoutType m_LayoutC = cask::md::MatrixLayoutType::N;

        // Cask doesn't use the same param buffer as the other LwdaLipnack tests
        struct RunParams
        {
            bool init;
            UINT64 A;
            UINT64 Am;
            UINT64 B;
            UINT64 C;
        };
        RunParams m_TmpRunParams = {};

        // For INT32-output kernels
        Lwca::Function m_ScaleMatrixFunc;

        // JS properties
        string m_DefaultKernelName;
        string m_KernelName;
        UINT32 m_ClusterX = 0;
        UINT32 m_ClusterY = 0;
        UINT32 m_ClusterZ = 0;
        bool m_PrintKernelList = false;
        bool m_PrintPackagedKernels = false;
};
