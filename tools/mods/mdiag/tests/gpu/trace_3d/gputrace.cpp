/*
   rt
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// Please keep the documentation at the following page in sync with the code:
// http://engwiki/index.php/MODS/GPU_Verification/trace_3d/File_Format

// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "trace_3d.h"
#include "tracemod.h"
#include "peer_mod.h"
#include "tracerel.h"
#include "core/include/simclk.h"
#include "traceop.h"
#include "core/include/platform.h"
#include "lwmisc.h"
#include "gpu/utility/blocklin.h"
#include "core/include/utility.h"
#include "selfgild.h"           // class SelfgildState
#include "mdiag/utils/utils.h"
#include "mdiag/tests/gpu/lwgpu_subchannel.h"
#include "mdiag/utils/tex.h"
#include "core/include/massert.h"
#include "lwos.h"
#include "mdiag/tests/gpu/lwgpu_ch_helper.h"
#include "bufferdumper.h"
#include "tracetsg.h"
#include "mdiag/utils/lwgpu_classes.h"
#include "core/include/gpu.h"
#include "mdiag/utils/XMLlogging.h"
#include "mdiag/utl/utl.h"

#include "class/cl9097tex.h"
#include "class/cl90f1.h" //
#include "class/cla0c0.h" // KEPLER_COMPUTE_A
#include "class/cla097.h" // KEPLER_A
#include "class/clb097.h" // MAXWELL_A

#include "ctrl/ctrl0041.h"
#include "ctrl/ctrl208f.h"
#include "core/include/lwrm.h"

#include <sstream>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif
#include <fcntl.h>
#include <errno.h>
#include <utility>

// SLI support.
#include "slisurf.h"
#include "mdiag/resource/lwgpu/dmaloader.h"

#include "mdiag/resource/lwgpu/crcchain.h"
#include "teegroups.h"

// Minimum and maximum trace versions lwrrently supported by MODS
#define MIN_TRACE_VERSION 0
#define MAX_TRACE_VERSION 5

#define MSGID(Mod) T3D_MSGID(Mod)

// subctx support
#include "mdiag/t3dresourcecontainer.h"
#include "mdiag/subctx.h"
#include "mdiag/vaspace.h"
#include "parsefile.h"
#include "pe_configurefile.h"
#include "watermarkimpl.h"

// shared surface support
#include "mdiag/utils/sharedsurfacecontroller.h"

namespace
{
    class CEChooser
    {
        public:
            CEChooser(GpuTrace *pGpuTrace, const ArgReader *params, TraceChannel *pTraceChannel,
                    TraceSubChannel *pTraceSubChannel, UINT32 ceType, GpuSubdevice *subdev)
                : m_pGpuTrace(pGpuTrace), m_pParams(params), m_pTraceChannel(pTraceChannel),
                m_pTraceSubChannel(pTraceSubChannel), m_CeType(ceType), m_pSubdev(subdev)
        {
            if (m_NextCEInstance.count(m_pGpuTrace) == 0)
            {
                m_NextCEInstance[m_pGpuTrace] = m_pParams->ParamUnsigned("-set_first_ce_instance", 0);
            }
        }

        RC Execute()
        {
            RC rc;

            CHECK_RC(m_pTraceChannel->GetGpuResource()->GetSupportedCeNum(  m_pSubdev, 
                                                                            &m_SupportedCE, 
                                                                            m_pTraceChannel->GetLwRmPtr()));
            CHECK_RC(LoadGrCopyEngineType());
            PrintCEOverview();

            CHECK_RC(OverrideCeTypeByCommands());
            CHECK_RC(ValidateCeType());

            return OK;
        }

        UINT32 GetCeType() const
        {
            return m_CeType;
        }

        bool IsGrCE() const
        {
            if (m_CeType < CEObjCreateParam::CE_INSTANCE_MAX)
            {
                const UINT32 ceEngineType = CEObjCreateParam::GetEngineTypeByCeType(m_CeType);
                for (auto & grCeGroupItr : m_GrCeGroup)
                {
                    if (ceEngineType == grCeGroupItr.second)
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        RC GetGrCERunQueueID(UINT32* runQueue) const
        {
            RC rc;

            if (m_CeType < CEObjCreateParam::CE_INSTANCE_MAX)
            {
                // finalCeType >= CE0 && finalCeType < CE_INSTANCE_MAX
                const UINT32 ceEngineType = CEObjCreateParam::GetEngineTypeByCeType(m_CeType);
                MASSERT(!m_GrCeGroup.empty() && "No available GRCE?!");
                for (auto & grCeGroupItr : m_GrCeGroup)
                {
                    if (ceEngineType == grCeGroupItr.second)
                    {
                        *runQueue = grCeGroupItr.first;
                        return OK;
                    }
                }
            }

            return RC::SOFTWARE_ERROR;
        }

        void PrintInfo() const
        {
            const string tsgInfo = m_pTraceChannel->GetTraceTsg() ?
                "tsg " + m_pTraceChannel->GetTraceTsg()->GetName() : "";
            InfoPrintf("Using CE instance %s on subchannel %s of channel %s %s\n",
                       CEObjCreateParam::GetCeName(m_CeType), m_pTraceSubChannel->GetName().c_str(),
                       m_pTraceChannel->GetName().c_str(), tsgInfo.c_str());
        }

    private:
        RC LoadGrCopyEngineType()
        {
            RC rc;
            LWGpuResource* pGpuResource = m_pTraceChannel->GetGpuResource();
            UINT32 runqueueCount = pGpuResource->GetGpuSubdevice()->GetNumGraphicsRunqueues();
            UINT32 grCopyType = LW2080_ENGINE_TYPE_NULL;
            for (UINT32 runqueue = 0; runqueue < runqueueCount; ++runqueue)
            {
                CHECK_RC(pGpuResource->GetGrCopyEngineType(&grCopyType, runqueue,  m_pTraceChannel->GetLwRmPtr()));
                if (grCopyType != LW2080_ENGINE_TYPE_NULL)
                    m_GrCeGroup.push_back(make_pair(runqueue, grCopyType));
            }

            return OK;
        }

        RC OverrideCeTypeByCommands()
        {
            RC rc;

            const bool bSetFirstCeInst = m_pParams->ParamPresent("-set_first_ce_instance") > 0;
            const UINT32 numSetSubchCEArgs = m_pParams->ParamPresent("-set_subch_ce");

            //
            // priority:
            // CE type from hdr <  cmd -set_first_ce_instance < cmd -set_subch_ce
            //

            // 1. command option -set_first_ce_instance
            // bug 693467: -set_first_ce_instance stay the same, and only
            //             cycle between the legacy copy engines.
            // bug 1176015: grcopy needs to be ilwolved in the automatic
            //             ce assignment with legacy copy engines.
            bool bIlwolvGrCe =
                m_pParams->ParamPresent("-allow_grcopy_in_ce_assignment") > 0;
            if (bIlwolvGrCe && !bSetFirstCeInst)
            {
                // Error out according to the request in bug 1176015
                ErrPrintf("%s: -allow_grcopy_in_ce_assignment must cowork with "
                          "-set_first_ce_instance. Please check cmd line!\n", __FUNCTION__);
                return RC::ILWALID_ARGUMENT;
            }
            else if(!bIlwolvGrCe && bSetFirstCeInst &&
                    (m_CeType == CEObjCreateParam::GRAPHIC_CE))
            {
                WarnPrintf("%s: Argument(-set_first_ce_instance %d) is ignored because"
                           " test.hdr has specifed GrCopy; -set_first_ce_instance without "
                           "-allow_grcopy_in_ce_assignment does not involve GrCopy, so cmdline"
                           " can overwrite test.hdr specification\n",
                           __FUNCTION__, m_NextCEInstance[m_pGpuTrace]);
            }
            else if (bSetFirstCeInst)
            {
                vector<UINT32> ceCandidates;

                for (UINT32 i = 0; i < m_SupportedCE.size(); i ++)
                {
                    UINT32 ceEngineType = CEObjCreateParam::GetEngineTypeByCeType(m_SupportedCE[i]);
                    if(!bIlwolvGrCe)
                    {
                        if (find_if(m_GrCeGroup.begin(), m_GrCeGroup.end(), 
                                [ceEngineType](pair<UINT32, UINT32> runqueueGrCe)
                                    {return ceEngineType == runqueueGrCe.second;}) 
                            != m_GrCeGroup.end())
                            continue;
                    }
                    ceCandidates.push_back(m_SupportedCE[i]);
                }

                // Start from the last used instance number
                // Then update for next run
                if (ceCandidates.size() > 0)
                {
                    UINT32 idx = m_NextCEInstance[m_pGpuTrace] % ceCandidates.size();
                    m_CeType = ceCandidates[idx];
                    m_NextCEInstance[m_pGpuTrace] = (idx + 1) % ceCandidates.size();
                }
                else
                {
                    WarnPrintf("%s: No available CE. Argument(-set_first_ce_instance %d) is ignored\n",
                               __FUNCTION__, m_NextCEInstance[m_pGpuTrace]);
                }
            }

            // 2. command option -set_subch_ce
            // -set_subch_ce can specify GRCOPY engine or CEx
            for (UINT32 i = 0; i < numSetSubchCEArgs; ++i)
            {
                string ch_name    = m_pParams->ParamNStr("-set_subch_ce", i, 0);
                string subch_name = m_pParams->ParamNStr("-set_subch_ce", i, 1);
                string ce_name  = m_pParams->ParamNStr("-set_subch_ce", i, 2);

                // set ce_type if channel and subchannel matches
                if (subch_name == m_pTraceSubChannel->GetName() &&
                    ch_name    == m_pTraceChannel->GetName())
                {
                    // colwert ce_name to ceType
                    if (ce_name == "GRCOPY")
                    {
                        m_CeType = CEObjCreateParam::GRAPHIC_CE;
                    }
                    else if (1 == sscanf(ce_name.c_str(), "CE%d", &m_CeType))
                    {
                        DebugPrintf(MSGID(TraceParser), "CE number specifed by -set_subch_ce is %d.\n",
                                    m_CeType);
                    }
                    else
                    {
                        char * p;
                        m_CeType = strtoul(ce_name.c_str(), &p, 0);
                        if (p == ce_name.c_str())
                        {
                            MASSERT(!"Format of option -set_subch_ce is invalid!\n");
                        }
                    }
                }
            }

            return OK;
        }

        RC ValidateCeType()
        {
            RC rc;

            //
            // Sanity Check
            //
            if (m_CeType < CEObjCreateParam::CE_INSTANCE_MAX)
            {
                // CE instance sanity check. Replace it with a supported one if failed
                bool isValidCEType = false;
                for (UINT32 i = 0; i < m_SupportedCE.size(); i++)
                {
                    if (m_SupportedCE[i] == m_CeType)
                    {
                        isValidCEType = true;
                        break;
                    }
                }

                if (!isValidCEType)
                {
                    ErrPrintf("%s: %s is not a supported CE. \n",
                              __FUNCTION__, CEObjCreateParam::GetCeName(m_CeType));
                    MASSERT(0);
                }
            }
            else if (m_CeType < CEObjCreateParam::END_CE &&
                    m_CeType > CEObjCreateParam::CE_INSTANCE_MAX)
            {
                // Only check GRCE if there are multiple runqueue in the GPU
                // other parts will be checked on creating subchannel object.
                if (m_pSubdev->GetNumGraphicsRunqueues() > 1)
                {
                    if ((m_CeType == CEObjCreateParam::GRAPHIC_CE) && (m_pTraceChannel->GetPbdma() != 0))
                    {
                        if (m_pParams->ParamPresent("-ignore_tsgs") > 0)
                        {
                            ErrPrintf("%s: -ignore_tsgs can't be specified if you want to allocate GRCOPY1!\n ",
                                      __FUNCTION__);
                            return RC::ILWALID_ARGUMENT;
                        }
                        if (m_pTraceChannel->GetTraceTsg() == 0)
                        {
                            ErrPrintf("%s: A TSG must be required if you want to allocate GRCOPY1!\n ",
                                      __FUNCTION__);
                            return RC::ILWALID_ARGUMENT;
                        }
                    }
                }
            }
            else
            {
                // Invalid ce type
                ErrPrintf("%s: Unkown CE type %d!\n", __FUNCTION__, m_CeType);
                return RC::ILWALID_ARGUMENT;
            }

            return OK;
        }

        void PrintCEOverview () const
        {
            stringstream allCE;
            for (vector<UINT32>::const_iterator iter = m_SupportedCE.begin();
                 iter != m_SupportedCE.end(); iter++)
            {
                allCE << CEObjCreateParam::GetCeName(*iter) << " ";
            }
            InfoPrintf("Supported CE number is %d\n", (UINT32)m_SupportedCE.size());
            InfoPrintf("Supported CE: %s\n", allCE.str().c_str());

            stringstream grCE;
            for (auto & grCeGroupItr : m_GrCeGroup)
            {
                grCE << CEObjCreateParam::GetCeName(CEObjCreateParam::GetCeTypeByEngine(grCeGroupItr.second)) << " ";
            }
            InfoPrintf("Supported GRCE: %s\n", grCE.str().c_str());
        }

        CEChooser(const CEChooser&);
        CEChooser& operator=(const CEChooser&);

        GpuTrace *m_pGpuTrace;
        const ArgReader *m_pParams;
        TraceChannel *m_pTraceChannel;
        TraceSubChannel *m_pTraceSubChannel;
        UINT32 m_CeType;
        GpuSubdevice *m_pSubdev;
        vector<UINT32> m_SupportedCE;
        // List of runqueue:GrCe pairs
        vector<pair<UINT32, UINT32>> m_GrCeGroup;

        static map<GpuTrace*, UINT32> m_NextCEInstance;
    };

    string NewTsgName()
    {
        static int index = 0;
        stringstream ss;
        ss << "default_" << index++;
        return ss.str();
    }

    map<GpuTrace*, UINT32> CEChooser::m_NextCEInstance;
}

GpuTrace::FileTypeData GpuTrace::s_FileTypeData[FT_NUM_FILE_TYPE] =
{
//    ArgName         Description                         DefaultAlignment
    { "vtx",          "Vertex Buffer",                      4, "vtx", },
    { "idx",          "Index Buffer",                       4, "idx", },
    { "tex",          "Texture Buffer",                   256, "tex", },
    { "pushbuf",      "DMA Buffer",                         4, "pb",  },
    { "pgm",          "Shader Program",                   128, "pgm", },
    { "const",        "Constant Buffer",                   64, "cst", },
    { "header",       "Texture Header Pool",               32, "hdr", },
    { "sampler",      "Texture Sampler Pool",              32, "sam", },
    { "thread_mem",   "Shader Thread Memory",        128*1024, "thM", },
    { "thread_stack", "Shader Thread Stack",              256, "thS", },
    { "sem",          "Semaphore",                         16, "sem", },
    { "sem_16",       "Semaphore (16B aligned)",           16, "s16", },
    { "notifier",     "Notifier",                           4, "not", },
    { "stream",       "Streaming Output",                   4, "str", },
    { "selfgild",     "Selfgild",                           4, "sgd", },
    { "vp2_0",        "VP2/BSP DmaCtx 0",                 256, "vp0", },
    { "vp2_1",        "VP2/BSP DmaCtx 1",                 256, "vp1", },
    { "vp2_2",        "VP2/BSP DmaCtx 2",                 256, "vp2", },
    { "vp2_3",        "VP2/BSP DmaCtx 3",                 256, "vp3", },
    { "vp2_4",        "VP2/BSP DmaCtx 4",                 256, "vp4", },
    { "vp2_5",        "VP2/BSP DmaCtx 5",                 256, "vp5", },
    { "vp2_6",        "VP2/BSP DmaCtx 6",                 256, "vp6", },
    { "vp2_7",        "VP2/BSP DmaCtx 7",                 256, "vp7", },
    { "vp2_8",        "VP2/BSP DmaCtx 8",                 256, "vp8", },
    { "vp2_9",        "VP2/BSP DmaCtx 9",                 256, "vp9", },
    { "vp2_10",       "VP2/BSP DmaCtx 10",                256, "vpA", },
    { "vp2_14",       "VP2/BSP DmaCtx 14 (Data Segment)", 256, "vpE", },
    { "cipher_a",     "Cipher Src",                        64, "cpA", },
    { "cipher_b",     "Cipher Dst",                        64, "cpB", },
    { "cipher_c",     "Cipher Extra",                      64, "cpC", },
    { "gmem_A",       "Global memory region A",             4, "gmA", },
    { "gmem_B",       "Global memory region B",             4, "gmB", },
    { "gmem_C",       "Global memory region C",             4, "gmC", },
    { "gmem_D",       "Global memory region D",             4, "gmD", },
    { "gmem_E",       "Global memory region E",             4, "gmE", },
    { "gmem_F",       "Global memory region F",             4, "gmF", },
    { "gmem_G",       "Global memory region G",             4, "gmG", },
    { "gmem_H",       "Global memory region H",             4, "gmH", },
    { "gmem_I",       "Global memory region I",             4, "gmI", },
    { "gmem_J",       "Global memory region J",             4, "gmJ", },
    { "gmem_K",       "Global memory region K",             4, "gmK", },
    { "gmem_L",       "Global memory region L",             4, "gmL", },
    { "gmem_M",       "Global memory region M",             4, "gmM", },
    { "gmem_N",       "Global memory region N",             4, "gmN", },
    { "gmem_O",       "Global memory region O",             4, "gmO", },
    { "gmem_P",       "Global memory region P",             4, "gmP", },
    { "LodStat",      "Texture LOD statistics",             4, "LoD", },
    { "zlwll",        "Zlwll RAM buffer",                   4, "Zlw", },
    { "pmu_0",        "PMU Aperture 0",                     4, "pm0", },
    { "pmu_1",        "PMU Aperture 1",                     4, "pm1", },
    { "pmu_2",        "PMU Aperture 2",                     4, "pm2", },
    { "pmu_3",        "PMU Aperture 3",                     4, "pm3", },
    { "pmu_4",        "PMU Aperture 4",                     4, "pm4", },
    { "pmu_5",        "PMU Aperture 5",                     4, "pm5", },
    { "pmu_6",        "PMU Aperture 6",                     4, "pm6", },
    { "pmu_7",        "PMU Aperture 7",                     4, "pm7", },
    { "surf",         "ALLOC_SURFACE buffer",               4, "srf", }
};

bool GpuTrace::ReadTrace()
{
    ModuleIter it;
    for (it = m_Modules.begin(); it != m_Modules.end(); ++it)
    {
        if (!(*it)->IsDynamic())
        {
            RC rc = (*it)->LoadFromFile(m_TraceFileMgr);
            if (rc != OK)
                return false;
        }
    }

    return true;
}

RC GpuTrace::DoVP2TilingLayout()
{
    RC rc = OK;

    ModuleIter it;
    for (it = m_Modules.begin(); it != m_Modules.end(); ++it)
    {
        CHECK_RC((*it)->DoVP2TilingLayout());
    }

    return OK;
}

///////////////////////////////////////

GpuTrace::GpuTrace(const ArgReader *reader, Trace3DTest *test)
    : m_Test(test)
{
    DebugPrintf(MSGID(TraceParser), "GpuTrace::GpuTrace(params=0x%08x)\n", reader);

    m_HasVersion = false;
    m_Version = 0;

    m_DynamicTextures = false;
    m_HasFileUpdates = false;
    m_HasDynamicModules = false;
    m_HasRefCheckModules = false;

    params = reader;
    m_Dumper = 0;
    m_Width = 0;
    m_Height = 0;
    m_TimeoutMs = 1000;
    m_SingleMethod = params->ParamPresent("-single_method") > 0;
    m_DumpImageEveryBegin = false;
    m_DumpImageEveryEnd = false;
    m_LogInfoEveryEnd = false;
    m_DumpZlwllEveryEnd = false;
    m_ZlwllRamPresent = false;
    m_ContainExelwtionDependency = false;
    m_DumpBufferBeginMapsIter = m_DumpBufferBeginMaps.begin();
    m_DumpBufferEndMapsIter = m_DumpBufferEndMaps.begin();

    m_HasClearInit = false;
    m_OpDisabled = false;
    m_LastGpEntryOp = 0;
    m_ExtraVirtualAllocations = 0;

    m_TraceFileMgr = m_Test->GetTraceFileMgr();
    m_LwEncOffset = params->ParamUnsigned("-alloc_lwenc_rr", 0);
    m_LwDecOffset = params->ParamUnsigned("-alloc_lwdec_rr", 0);
    m_LwJpgOffset = params->ParamUnsigned("-alloc_lwjpg_rr", 0);
    ProcessEventDumpArgs();

    m_IgnoreTsg = params->ParamPresent("-ignore_tsgs") > 0;
    m_DmaCheckCeRequested = false;
    m_SkipBufferDownloads = false;
    m_numPmTriggers = 0;
    m_numSyncPmTriggers = 0;
    m_HaveSyncEventOp = false;
}

GpuTrace::~GpuTrace()
{
    // A GpuTrace object can be used by Trace3DTest to process several
    // different traces in a row (e.g., with -serial).  Most items should be
    // deleted in the function ClearHeader as that is called after Trace3DTest
    // is finished with a particular trace.
    ClearHeader();
    ClearDmaLoaders();

    delete params;
}

void GpuTrace::ProcessEventDumpArgs()
{
    UINT32 dump_event_count = params->ParamPresent("-dump_buffer_on_event");
    for (UINT32 i = 0; i < dump_event_count; i++)
    {
        string buf_name = params->ParamNStr("-dump_buffer_on_event", i, 0);
        string event    = params->ParamNStr("-dump_buffer_on_event", i, 1);
        // We don't check buffer name here since TraceModules are not defined yet.
        // If we put all the processing after hdr parsing, that would be too late.
        vector<string> &buf_vec = m_BuffersDumpOnEvent[event];
        vector<string>::iterator iter = buf_vec.begin();
        while (iter != buf_vec.end())
        {
            if (*iter == buf_name)
            {
                break;   // Already exists
            }
            iter++;
        }
        if (iter == buf_vec.end())
        {
            m_BuffersDumpOnEvent[event].push_back(buf_name);
        }
    }
}

void GpuTrace::ClearHeader()
{
    ModuleIter modIt;

    for (modIt = m_Modules.begin(); modIt != m_Modules.end(); ++modIt)
    {
        delete *(modIt);
    }
    m_Modules.clear();
    m_GenericModules.clear();
    m_PeerModules.clear();

    TraceOps::iterator opIt;
    for (opIt = m_TraceOps.begin(); opIt != m_TraceOps.end(); ++opIt)
    {
        delete opIt->second;
    }
    m_TraceOps.clear();
    m_TraceOpLineNos.clear();

    m_HasFileUpdates = false;
    m_HasDynamicModules = false;
    m_HasRefCheckModules = false;

    for (SelfgildStates::iterator i = m_Selfgilds.begin();
         i != m_Selfgilds.end();
         ++i)
    {
        delete *i;
    }

    m_Selfgilds.clear();

    for (ZLwllStorage::iterator zLwllIter = m_ZLwllStorage.begin();
         zLwllIter != m_ZLwllStorage.end();
         ++zLwllIter)
    {
        zLwllIter->second->Free();
        delete zLwllIter->second;
    }

    m_ZLwllStorage.clear();

    if (m_Dumper)
    {
        delete m_Dumper;
        m_Dumper = 0;
    }
}

RC GpuTrace::MatchFileType(const char *name, GpuTrace::TraceFileType *pftype)
{
    if (!strcmp(name, "VERTEX") && (m_Version < 2))
        *pftype = GpuTrace::FT_VERTEX_BUFFER;
    else if (!strcmp(name, "VertexBuffer"))
        *pftype = GpuTrace::FT_VERTEX_BUFFER;
    else if (!strcmp(name, "VertexBitBucket"))
        *pftype = GpuTrace::FT_VERTEX_BUFFER;
    else if (!strcmp(name, "IndexBuffer"))
        *pftype = GpuTrace::FT_INDEX_BUFFER;
    else if (!strcmp(name, "DMA") && (m_Version < 2))
        *pftype = GpuTrace::FT_PUSHBUFFER;
    else if (!strcmp(name, "Pushbuffer"))
        *pftype = GpuTrace::FT_PUSHBUFFER;
    else if (!strcmp(name, "TEXTURE") && (m_Version < 2))
        *pftype = GpuTrace::FT_TEXTURE;
    else if (!strcmp(name, "Texture"))
        *pftype = GpuTrace::FT_TEXTURE;
    else if (!strcmp(name, "RenderBuffer")) // bug# 310981
        *pftype = GpuTrace::FT_TEXTURE;
    else if (!strcmp(name, "VertexProgram"))
        *pftype = GpuTrace::FT_SHADER_PROGRAM;
    else if (!strcmp(name, "VertexCbfProgram")) // bug# 376892
        *pftype = GpuTrace::FT_SHADER_PROGRAM;
    else if (!strcmp(name, "PixelProgram"))
        *pftype = GpuTrace::FT_SHADER_PROGRAM;
    else if (!strcmp(name, "GeometryProgram"))
        *pftype = GpuTrace::FT_SHADER_PROGRAM;
    else if (!strcmp(name, "FragmentProgram"))
        *pftype = GpuTrace::FT_SHADER_PROGRAM;
    else if (!strcmp(name, "ComputeProgram"))
        *pftype = GpuTrace::FT_SHADER_PROGRAM;
    else if (!strcmp(name, "TessControlProgram")) // bug# 310981
        *pftype = GpuTrace::FT_SHADER_PROGRAM;
    else if (!strcmp(name, "TessEvaluationProgram")) // bug# 310981
        *pftype = GpuTrace::FT_SHADER_PROGRAM;
    else if (!strcmp(name, "TextureHeaderPool"))
        *pftype = GpuTrace::FT_TEXTURE_HEADER;
    else if (!strcmp(name, "TextureSamplerPool"))
        *pftype = GpuTrace::FT_TEXTURE_SAMPLER;
    else if (!strcmp(name, "ConstantBufferTable"))
        *pftype = GpuTrace::FT_CONSTANT_BUFFER;
    else if (!strcmp(name, "ConstantBuffer"))
        *pftype = GpuTrace::FT_CONSTANT_BUFFER;
    else if (!strcmp(name, "ShaderThreadMemory"))
        *pftype = GpuTrace::FT_SHADER_THREAD_MEMORY;
    else if (!strcmp(name, "ShaderThreadStack"))
        *pftype = GpuTrace::FT_SHADER_THREAD_STACK;
    else if (!strcmp(name, "Semaphore"))
        *pftype = GpuTrace::FT_SEMAPHORE;
    else if (!strcmp(name, "Notifier"))
        *pftype = GpuTrace::FT_NOTIFIER;
    else if (!strcmp(name, "StreamOutput"))
        *pftype = GpuTrace::FT_STREAM_OUTPUT;
    else if (!strcmp(name, "Selfgild"))
        *pftype = GpuTrace::FT_SELFGILD;
    else if (!strcmp(name, "VP2_0"))
        *pftype = GpuTrace::FT_VP2_0;
    else if (!strcmp(name, "VP2_1"))
        *pftype = GpuTrace::FT_VP2_1;
    else if (!strcmp(name, "VP2_1_VCODE"))
        *pftype = GpuTrace::FT_VP2_1;
    else if (!strcmp(name, "VP2_2"))
        *pftype = GpuTrace::FT_VP2_2;
    else if (!strcmp(name, "VP2_3"))
        *pftype = GpuTrace::FT_VP2_3;
    else if (!strcmp(name, "VP2_4"))
        *pftype = GpuTrace::FT_VP2_4;
    else if (!strcmp(name, "VP2_5"))
        *pftype = GpuTrace::FT_VP2_5;
    else if (!strcmp(name, "VP2_6"))
        *pftype = GpuTrace::FT_VP2_6;
    else if (!strcmp(name, "VP2_7"))
        *pftype = GpuTrace::FT_VP2_7;
    else if (!strcmp(name, "VP2_8"))
        *pftype = GpuTrace::FT_VP2_8;
    else if (!strcmp(name, "VP2_9"))
        *pftype = GpuTrace::FT_VP2_9;
    else if (!strcmp(name, "VP2_9_SCODE"))
        *pftype = GpuTrace::FT_VP2_9;
    else if (!strcmp(name, "VP2_10"))
        *pftype = GpuTrace::FT_VP2_10;
    else if (!strcmp(name, "VP2_14"))
        *pftype = GpuTrace::FT_VP2_14;
    else if (!strcmp(name, "VP2_14_DATASEG"))
        *pftype = GpuTrace::FT_VP2_14;
    else if (!strcmp(name, "CipherA"))
        *pftype = GpuTrace::FT_CIPHER_A;
    else if (!strcmp(name, "CipherB"))
        *pftype = GpuTrace::FT_CIPHER_B;
    else if (!strcmp(name, "CipherC"))
        *pftype = GpuTrace::FT_CIPHER_C;
    else if (!strcmp(name, "GMEM_A"))
        *pftype = GpuTrace::FT_GMEM_A;
    else if (!strcmp(name, "GMEM_B"))
        *pftype = GpuTrace::FT_GMEM_B;
    else if (!strcmp(name, "GMEM_C"))
        *pftype = GpuTrace::FT_GMEM_C;
    else if (!strcmp(name, "GMEM_D"))
        *pftype = GpuTrace::FT_GMEM_D;
    else if (!strcmp(name, "GMEM_E"))
        *pftype = GpuTrace::FT_GMEM_E;
    else if (!strcmp(name, "GMEM_F"))
        *pftype = GpuTrace::FT_GMEM_F;
    else if (!strcmp(name, "GMEM_G"))
        *pftype = GpuTrace::FT_GMEM_G;
    else if (!strcmp(name, "GMEM_H"))
        *pftype = GpuTrace::FT_GMEM_H;
    else if (!strcmp(name, "GMEM_I"))
        *pftype = GpuTrace::FT_GMEM_I;
    else if (!strcmp(name, "GMEM_J"))
        *pftype = GpuTrace::FT_GMEM_J;
    else if (!strcmp(name, "GMEM_K"))
        *pftype = GpuTrace::FT_GMEM_K;
    else if (!strcmp(name, "GMEM_L"))
        *pftype = GpuTrace::FT_GMEM_L;
    else if (!strcmp(name, "GMEM_M"))
        *pftype = GpuTrace::FT_GMEM_M;
    else if (!strcmp(name, "GMEM_N"))
        *pftype = GpuTrace::FT_GMEM_N;
    else if (!strcmp(name, "GMEM_O"))
        *pftype = GpuTrace::FT_GMEM_O;
    else if (!strcmp(name, "GMEM_P"))
        *pftype = GpuTrace::FT_GMEM_P;
    else if (!strcmp(name, "LodStat"))
        *pftype = GpuTrace::FT_LOD_STAT;
    else if (!strcmp(name, "ZlwllRamBuffer"))
        *pftype = GpuTrace::FT_ZLWLL_RAM;
    else if (!strcmp(name, "PMU_0"))
        *pftype = GpuTrace::FT_PMU_0;
    else if (!strcmp(name, "PMU_1"))
        *pftype = GpuTrace::FT_PMU_1;
    else if (!strcmp(name, "PMU_2"))
        *pftype = GpuTrace::FT_PMU_2;
    else if (!strcmp(name, "PMU_3"))
        *pftype = GpuTrace::FT_PMU_3;
    else if (!strcmp(name, "PMU_4"))
        *pftype = GpuTrace::FT_PMU_4;
    else if (!strcmp(name, "PMU_5"))
        *pftype = GpuTrace::FT_PMU_5;
    else if (!strcmp(name, "PMU_6"))
        *pftype = GpuTrace::FT_PMU_6;
    else if (!strcmp(name, "PMU_7"))
        *pftype = GpuTrace::FT_PMU_7;
    else
        return RC::ILWALID_FILE_FORMAT;

    return OK;
}

//---------------------------------------------------------------------------
// read a trace header, remember all the interesting data
RC GpuTrace::ReadHeader(const char *filename, UINT32 timeoutMs)
{
    RC rc = OK;

    m_TimeoutMs = timeoutMs;
    m_pParseFile.reset(new ParseFile(m_Test, params));

    // Read the file into a big char buffer.
    CHECK_RC(m_pParseFile->ReadHeader(filename, timeoutMs));

    swap(m_TestHdrTokens, m_pParseFile->GetTokens());

    // Parse the token-list.
    rc = ParseTestHdr(filename);

    if (params->ParamPresent("-enable_sync_event"))
    {
        // Not support sync event when traceop dependency is enabled.
        // Refer to 
        // https://confluence.lwpu.com/display/ArchMods/Execute+trace3D+commands+greedily+based+on+dependency-Implementation
        // for traceop dependency.
        MASSERT(!m_ContainExelwtionDependency);
        CHECK_RC(AddInternalSyncEvent());
    }

    // end of trace, update maximum GP Entries for all TraceChannel
    UpdateAllTraceChannelMaxGpEntries();

    return rc;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseTestHdr (const char * filename)
{
    StickyRC rc = DoParseTestHdr();

    if (OK != rc)
    {
        ErrPrintf("\"%s\"\n", rc.Message());
        ErrPrintf("Error at at line %d of %s\n", m_LineNumber, filename);
        return rc;
    }

    CHECK_RC(ConfigureWatermark());

    if (OK == rc)
    {
        TraceChannel * pTraceChannel = GetLwrrentChannel(CPU_MANAGED);
        if (pTraceChannel == 0)
        {
            if (params->ParamPresent("-allow_nocpuchannel_trace") == 0 &&
                params->ParamPresent("-no_trace") == 0)
            {
                ErrPrintf("%s: you must declare a cpu managed CHANNEL!\n", filename);
                rc = RC::ILWALID_FILE_FORMAT;
            }
        }
        else
        {
            if ( !pTraceChannel->SubChSanityCheck(filename) )
            {
                rc = RC::ILWALID_FILE_FORMAT;
            }
            if (pTraceChannel->ModBegin() == pTraceChannel->ModEnd())
            {
                DebugPrintf(MSGID(TraceParser), "%s: This is a trace without METHODS!\n", filename);
            }
            if (ZlwllRamSanityCheck() != OK)
            {
                ErrPrintf("%s: ZlwllRamBuffer file cannot be loaded for current chip!\n",
                          filename);
                rc = RC::ILWALID_FILE_FORMAT;
            }
            vector<TraceChannel*> compute_ch;
            GetComputeChannels(compute_ch, CPU_MANAGED);
            if (!compute_ch.empty() &&
                compute_ch[0]->GetComputeSubChannel() &&
                compute_ch[0]->GetComputeSubChannel()->GetClass() >= KEPLER_COMPUTE_A)
            {
                CHECK_RC(CreateTrapHandlers(compute_ch[0]->GetComputeSubChannel()->GetClass()));
            }
            else
            {
                TraceChannel* grch = GetGrChannel();
                if (grch &&
                    grch->GetGrSubChannel() &&
                    grch->GetGrSubChannel()->GetClass() >= KEPLER_A)
                {
                    CHECK_RC(CreateTrapHandlers(grch->GetGrSubChannel()->GetClass()));
                }
            }
        }
    }
    if (OK == rc)
        rc = SetupTextureParams();

    if (OK == rc)
        rc = SetupVP2Params();

    if (OK == rc)
        rc = ProcessSphArgs();

    if ((OK == rc) && (m_Version < 3))
    {
        // Method-send ops are implicit in v2 and earler, add them now
        // in between the various other ops.
        InsertMethodSendOps();
    }

    // Hack for bug 818386.
    // Create a dummy graphic/compute channel if no one created,
    // so that the FECS ucode could get loaded.
    if (params->ParamPresent("-dummy_ch_for_offload") ||
        params->ParamPresent("-dummy_gr_channel"))
    {
        vector<TraceChannel *> computeChannels;
        GetComputeChannels(computeChannels);
        if (GetGrChannel() || computeChannels.size() > 0)
        {
            InfoPrintf("Graphic/compute channel already exists, no need to create a dummy one. "
                       "-dummy_ch_for_offload/-dummy_gr_channel discarded.\n");
        }
        else
        {
            const char *chName = 0;
            if (params->ParamPresent("-dummy_ch_for_offload"))
            {
                chName = params->ParamNStr("-dummy_ch_for_offload", 0);
            }
            else
            {
                chName = params->ParamNStr("-dummy_gr_channel", 0);
            }
            MASSERT(chName);
            shared_ptr<SubCtx> pTemp;
            CHECK_RC(AddChannel(chName, "", CPU_MANAGED, GetVAS("default"), pTemp));
            TraceChannel *channel = GetLwrrentChannel();
            TraceSubChannel *subChannel = channel->AddSubChannel("default", params);
            if (!subChannel)
            {
                return RC::SOFTWARE_ERROR;
            }

            UINT32 classId = 0;
            LwRm* pLwRm = m_Test->GetLwRmPtr();

            vector<UINT32> GrComputeClasses;
            GrComputeClasses.insert(GrComputeClasses.end(),
                EngineClasses::GetClassVec("Gr").begin(), EngineClasses::GetClassVec("Gr").end());
            GrComputeClasses.insert(GrComputeClasses.end(), 
                EngineClasses::GetClassVec("Compute").begin(), EngineClasses::GetClassVec("Compute").end());
            sort(GrComputeClasses.begin(), GrComputeClasses.end(), 
                    [](UINT32 first, UINT32 second) { return first > second; });

            CHECK_RC_MSG(pLwRm->GetFirstSupportedClass(
                GrComputeClasses.size(),
                GrComputeClasses.data(),
                &classId,
                m_Test->GetBoundGpuDevice()),
                "No supported graphic/compute class found.");

            subChannel->SetClass(classId);
            GetComputeChannels(computeChannels);
            MASSERT(GetGrChannel() || computeChannels.size() > 0);
        }
    }

    return rc;
}

RC GpuTrace::DoParseTestHdr()
{
    UINT32 DbgSize = params->ParamUnsigned("-debug_module_size", 32);

    m_LineNumber = 1; // First line is line "1".

    RC rc = OK;
    CHECK_RC(CreateCommandLineResource());

    // Parse each line of the tokenized test.hdr.
    while (m_TestHdrTokens.size())
    {
        const char * tok = m_TestHdrTokens.front();
        m_TestHdrTokens.pop_front();

        if (ParseFile::s_NewLine == tok)
        {
            // Empty line, do nothing.
            // Note we are comparing char * values rather than doing a strcmp,
            // which is correct in this special case, due to how Tokenize()
            // built the m_TestHdrTokens.stack.
            m_LineNumber++;
            continue;
        }

        // If the first character of the first token on the line is
        // an @ symbol, this token represents a predicate that determines
        // if the rest of the trace header line is used or discarded.
        if ('@' == tok[0])
        {
            bool ilwertLogic = false;
            ++tok;

            // An exclamation point after the @ symbol indicates the
            // predicate check is ilwerted.
            if ('!' == tok[0])
            {
                ilwertLogic = true;
                ++tok;
            }

            // If the predicate specified by the current token matches
            // a user-defined predicate, this line will be parsed,
            // unless the logic is ilwerted by an exclamation point,
            // in which case the line is ignored if the predicate matches.
            bool predicateExists = TestPredicate(tok);
            bool skipLine = ilwertLogic ? predicateExists : !predicateExists;

            if (skipLine)
            {
                // Skip the rest of the line.
                while ((m_TestHdrTokens.size() > 0) &&
                    (ParseFile::s_NewLine != m_TestHdrTokens.front()))
                {
                    m_TestHdrTokens.pop_front();
                }
                continue;
            }
            else
            {
                // Skip the predicate token and continue.
                tok = m_TestHdrTokens.front();
                m_TestHdrTokens.pop_front();

                // If the predicate was on an empty line, increment
                // the line number and start over.
                if (ParseFile::s_NewLine == tok)
                {
                    ++m_LineNumber;
                    continue;
                }
            }
        }
        // If the first character of the first token on the line is
        // an $ symbol, this token represents an alias of this traceop
        else if ('$' == tok[0])
        {
            ++tok;
            if (m_UserDefTraceOpIdMap.count(tok) > 0)
            {
                ErrPrintf("Duplicated user defined traceop name %s.\n", tok);
                return RC::ILWALID_FILE_FORMAT;
            }
            m_UserDefTraceOpIdMap.insert(pair<string, UINT32>(tok, m_LineNumber));

            tok = m_TestHdrTokens.front();
            m_TestHdrTokens.pop_front();
        }

        if (0 == strcmp("VERSION", tok))
        {
            // First statement must be VERSION <uint32>, or we assume version 0.
            rc = ParseVERSION();
        }
        else
        {
            if (!m_HasVersion)
            {
                m_HasVersion = true;
                m_Version = 0;
                InfoPrintf("First statement is not VERSION, assuming version 0.\n");
            }

            if      (0 == strcmp("CFG_COMPARE", tok))           rc = ParseCFG_COMPARE();
            else if (0 == strcmp("CFG_WRITE", tok))             rc = ParseCFG_WRITE();
            else if (0 == strcmp("CHANNEL", tok))               rc = ParseCHANNEL(false);
            else if (0 == strcmp("SUBCHANNEL", tok))            rc = ParseSUBCHANNEL(false);
            else if (0 == strcmp("ALLOC_CHANNEL", tok))         rc = ParseCHANNEL(true);
            else if (0 == strcmp("ALLOC_SUBCHANNEL", tok))      rc = ParseSUBCHANNEL(true);
            else if (0 == strcmp("FREE_CHANNEL", tok))          rc = ParseFREE_CHANNEL();
            else if (0 == strcmp("FREE_SUBCHANNEL",tok))        rc = ParseFREE_SUBCHANNEL();
            else if (0 == strcmp("CLASS", tok))                 rc = ParseCLASS();
            else if (0 == strcmp("CLEAR", tok))                 rc = ParseCLEAR();
            else if (0 == strcmp("CLEAR_FLOAT", tok))           rc = ParseCLEAR_FLOAT_INT(tok);
            else if (0 == strcmp("CLEAR_INT", tok))             rc = ParseCLEAR_FLOAT_INT(tok);
            else if (0 == strcmp("CLEAR_INIT", tok))            rc = ParseCLEAR_INIT();
            else if (0 == strcmp("ESCAPE_COMPARE", tok))        rc = ParseESCAPE_COMPARE();
            else if (0 == strcmp("ESCAPE_WRITE", tok))          rc = ParseESCAPE_WRITE();
            else if (0 == strcmp("ESCAPE_WRITE_FILE", tok))     rc = ParseESCAPE_WRITE_FILE();
            else if (0 == strcmp("FILE", tok))                  rc = ParseFILE(DbgSize, GpuTrace::FROM_FS_FILE);
            else if (0 == strcmp("ALLOC_SURFACE", tok))         rc = ParseALLOC_SURFACE();
            else if (0 == strcmp("SURFACE_VIEW", tok))          rc = ParseSURFACE_VIEW();
            else if (0 == strcmp("ALLOC_VIRTUAL", tok))         rc = ParseALLOC_VIRTUAL();
            else if (0 == strcmp("ALLOC_PHYSICAL", tok))        rc = ParseALLOC_PHYSICAL();
            else if (0 == strcmp("MAP", tok))                   rc = ParseMAP();
            else if (0 == strcmp("FREE_SURFACE", tok))          rc = ParseFREE_SURFACE();
            else if (0 == strcmp("FREE_VIRTUAL", tok))          rc = ParseFREE_SURFACE();
            else if (0 == strcmp("FREE_PHYSICAL", tok))         rc = ParseFREE_SURFACE();
            else if (0 == strcmp("UNMAP", tok))                 rc = ParseFREE_SURFACE();
            else if (0 == strcmp("FLUSH", tok))                 rc = ParseFLUSH();
            else if (0 == strcmp("GPENTRY", tok))               rc = ParseGPENTRY();
            else if (0 == strcmp("GPENTRY_CONTINUE", tok))      rc = ParseGPENTRY_CONTINUE();
            else if (0 == strcmp("INLINE_FILE", tok))           rc = ParseFILE(DbgSize, GpuTrace::FROM_INLINE_FILE);
            else if (0 == strcmp("ZEROS", tok))                 rc = ParseFILE(DbgSize, GpuTrace::FROM_CONSTANT);
            else if (0 == strcmp("METHODS", tok))               rc = ParseMETHODS();
            else if (0 == strcmp("PRI_COMPARE", tok))           rc = ParsePRI_COMPARE();
            else if (0 == strcmp("PRI_WRITE", tok))             rc = ParsePRI_WRITE();
            else if (0 == strcmp("RELOC", tok))                 rc = ParseRELOC();
            else if (0 == strcmp("RELOC40", tok))               rc = ParseRELOC40();
            else if (0 == strcmp("RELOC64", tok))               rc = ParseRELOC64();
            else if (0 == strcmp("RELOC_ACTIVE_REGION", tok))   rc = ParseRELOC_ACTIVE_REGION();
            else if (0 == strcmp("RELOC_BRANCH", tok))          rc = ParseRELOC_BRANCH();
            else if (0 == strcmp("RELOC_CONST32", tok))         rc = ParseRELOC_CONST32();
            else if (0 == strcmp("RELOC_CTXDMA_HANDLE", tok))   rc = ParseRELOC_CTXDMA_HANDLE();
            else if (0 == strcmp("RELOC_D", tok))               rc = ParseRELOC_D();
            else if (0 == strcmp("RELOC_FREEZE", tok))          rc = ParseRELOC_FREEZE();
            else if (0 == strcmp("RELOC_MS32", tok))            rc = ParseRELOC_MS32();
            else if (0 == strcmp("RELOC_RESET_METHOD", tok))    rc = ParseRELOC_RESET_METHOD();
            else if (0 == strcmp("RELOC_SIZE", tok))            rc = ParseRELOC_SIZE();
            else if (0 == strcmp("RELOC_SIZE_LONG", tok))       rc = ParseRELOC_SIZE_LONG();
            else if (0 == strcmp("RELOC_TYPE", tok))            rc = ParseRELOC_TYPE();
            else if (0 == strcmp("RELOC_VP2_PITCH", tok))       rc = ParseRELOC_VP2_PITCH();
            else if (0 == strcmp("RELOC_VP2_TILEMODE", tok))    rc = ParseRELOC_VP2_TILEMODE();
            else if (0 == strcmp("RELOC_SURFACE", tok))         rc = ParseRELOC_SURFACE();
            else if (0 == strcmp("RELOC_SURFACE_PROPERTY",tok)) rc = ParseRELOC_SURFACE_PROPERTY();
            else if (0 == strcmp("RELOC_LONG", tok))            rc = ParseRELOC_LONG();
            else if (0 == strcmp("RELOC_CLEAR", tok))           rc = ParseRELOC_CLEAR();
            else if (0 == strcmp("RELOC_ZLWLL", tok))           rc = ParseRELOC_ZLWLL();
            else if (0 == strcmp("RELOC_SCALE", tok))           rc = ParseRELOC_SCALE();
            else if (0 == strcmp("RUNLIST", tok))               rc = ParseRUNLIST();
            else if (0 == strcmp("SIZE", tok))                  rc = ParseSIZE();
            else if (0 == strcmp("TIMESLICE", tok))             rc = ParseTIMESLICE();
            else if (0 == strcmp("TRUSTED_CTX", tok))           rc = ParseTRUSTED_CTX();
            else if (0 == strcmp("UPDATE_FILE", tok))           rc = ParseUPDATE_FILE();
            else if (0 == strcmp("WAIT_FOR_IDLE", tok))         rc = ParseWAIT_FOR_IDLE();
            else if (0 == strcmp("WAIT_FOR_VALUE32", tok))      rc = ParseWAIT_FOR_VALUE32();
            else if (0 == strcmp("SEC_KEY", tok))               rc = ParseSEC_KEY();
            else if (0 == strcmp("WAIT_PMU_CMD", tok))          rc = ParseWAIT_PMU_CMD();
            else if (0 == strcmp("SYSMEM_CACHE_CTRL", tok))     rc = ParseSYSMEM_CACHE_CTRL();
            else if (0 == strcmp("FLUSH_CACHE_CTRL", tok))      rc = ParseFLUSH_CACHE_CTRL();
            else if (0 == strcmp("ENABLE_CHANNEL", tok))        rc = ParseENABLE_CHANNEL();
            else if (0 == strcmp("DISABLE_CHANNEL", tok))       rc = ParseDISABLE_CHANNEL();
            else if (0 == strcmp("DISABLE_OP", tok))            m_OpDisabled = true;
            else if (0 == strcmp("ENABLE_OP", tok))             m_OpDisabled = false;
            else if (0 == strcmp("PMTRIGGER_EVENT", tok))       rc = ParsePMTriggerEvent();
            else if (0 == strcmp("PMTRIGGER_SYNC_EVENT", tok))  rc = ParsePMTriggerSyncEvent();
            else if (0 == strcmp("EVENT", tok))                 rc = ParseEvent();
            else if (0 == strcmp("SYNC_EVENT", tok))            rc = ParseSyncEvent();
            else if (0 == strcmp("TIMESLICEGROUP", tok))        rc = ParseTIMESLICEGROUP();
            else if (0 == strcmp("CHECK_DYNAMICSURFACE", tok))  rc = ParseCHECK_DYNAMICSURFACE();
            else if (0 == strcmp("TRACE_OPTIONS", tok))         rc = ParseTRACE_OPTIONS();
            else if (0 == strcmp("PROGRAM_ZBC_COLOR", tok))     rc = ParsePROGRAM_ZBC_COLOR();
            else if (0 == strcmp("PROGRAM_ZBC_DEPTH", tok))     rc = ParsePROGRAM_ZBC_DEPTH();
            else if (0 == strcmp("PROGRAM_ZBC_STENCIL", tok))   rc = ParsePROGRAM_ZBC_STENCIL();
            else if (0 == strcmp("ADDRESS_SPACE", tok))         rc = ParseADDRESS_SPACE();
            else if (0 == strcmp("SUBCONTEXT", tok))            rc = ParseSUBCONTEXT();
            else if (0 == strcmp("DEPENDENCY", tok))            rc = ParseDEPENDENCY();
            else if (0 == strcmp("UTL_SCRIPT", tok))            rc = ParseUTL_SCRIPT();
            else if (0 == strcmp("UTL_RUN", tok))               rc = ParseUTL_RUN();
            else if (0 == strcmp("UTL_RUN_BEGIN", tok))         rc = ParseUTL_RUN_BEGIN();
            else
            {
                ErrPrintf("Unexpected token \"%s\" at beginning of line.\n", tok);
                rc = RC::ILWALID_FILE_FORMAT;
            }
        }
        if (OK != rc)
            break;

        // Every statement should be followed by a newline.
        tok = m_TestHdrTokens.front();
        m_TestHdrTokens.pop_front();
        while (tok != ParseFile::s_NewLine)
        {
            WarnPrintf("Unexpected token \"%s\" at the end of line %d.\n",
                       tok, m_LineNumber);
            tok = m_TestHdrTokens.front();
            m_TestHdrTokens.pop_front();

            if (m_Version > 2)
            {
                // Let's have all our users fix their traces when they update
                // to newer syntax versions, at least.
                rc = RC::ILWALID_FILE_FORMAT;
            }
            if (OK != rc)
                break;
        }
        m_LineNumber++;

    }   // end of while (m_TestHdrTokens.size());

    return rc;
}

//---------------------------------------------------------------------------
// Determine if the given token matches a predicate as defined by
// a user-specified command-line argument.
//
bool GpuTrace::TestPredicate(const char *tok)
{
    UINT32 argCount = params->ParamPresent("-define_predicate");
    UINT32 i;

    for (i = 0; i < argCount; ++i)
    {
        if (strcmp(tok,
            params->ParamNStr("-define_predicate", i, 0)) == 0)
        {
            return true;
        }
    }

    return false;
}

//---------------------------------------------------------------------------
// For v0, v1, and v2 traces, operations are ordered by offset in the
// single test.dma pushbuffer-data module.
// When we have finished parsing the test.hdr, we go back and insert the
// SendMethods ops in between the various WaitForIdle, UpdateFile, etc.
// operations.
// In the simplest case, there were no such operations, and we insert one
// SendMethods op for the whole test.dma file.
//
void GpuTrace::InsertMethodSendOps()
{
    TraceChannel * pTraceChannel = GetLwrrentChannel(CPU_MANAGED);
    if (!pTraceChannel || pTraceChannel->ModBegin() == pTraceChannel->ModEnd())
    {
        // nothing to be inserted for trace without channel or test.dma
        return;
    }

    TraceModule * pTraceModule = (*pTraceChannel->ModBegin());

    UINT32 lastOffsetSent = 0;

    for(TraceOps::iterator opIter = m_TraceOps.begin();
            opIter != m_TraceOps.end();
                    ++opIter)
    {
        UINT32 newOffset = (opIter->first) >> OP_SEQUENCE_SHIFT;
        if (newOffset > lastOffsetSent)
        {
            TraceOpSendMethods * pOp = new TraceOpSendMethods(
                    pTraceModule,
                    lastOffsetSent,
                    newOffset - lastOffsetSent);

            InsertOp(lastOffsetSent, pOp, OP_SEQUENCE_METHODS);
            lastOffsetSent = newOffset;
        }
    }
    if (lastOffsetSent < pTraceModule->GetSize())
    {
        TraceOpSendMethods * pOp = new TraceOpSendMethods(
                pTraceModule,
                lastOffsetSent,
                pTraceModule->GetSize() - lastOffsetSent);

        InsertOp(lastOffsetSent, pOp, OP_SEQUENCE_METHODS);
    }
}

//---------------------------------------------------------------------------
RC GpuTrace::RetriveZblwINT32Field(const char * tok, const char * fieldName, UINT32 * fieldVal)
{
    if (0 != strcmp(tok, (fieldName)))
    {
        return RC::ILWALID_FILE_FORMAT;
    }
    else
    {
        if (OK != ParseUINT32(fieldVal))
        {
            return RC::ILWALID_FILE_FORMAT;
        }
    }
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::RetriveZbcStringField(const char * tok, const char * fieldName, string * fieldVal)
{
    if (0 != strcmp(tok, (fieldName)))
    {
        return RC::ILWALID_FILE_FORMAT;
    }
    else
    {
        if (OK != ParseString(fieldVal))
        {
            return RC::ILWALID_FILE_FORMAT;
        }
    }
    return OK;
}
//---------------------------------------------------------------------------
void GpuTrace::InsertOp(UINT32 seqNum, TraceOp* pOp, UINT32 opOffset)
{
    if (m_Version < 3)
    {
        // Trace syntax versions 0, 1, and 2 ignore the line# in test.hdr.
        // Everything is done in a fixed sequence, except that the commands
        // WAIT_FOR_IDLE, UPDATE_FILE, and GPENTRY are done at particular
        // offsets in the pushbuffer.
        //
        // Frequently more than one of these is done at the same offset,
        // in which case they must be done in the order:
        //   WAIT_FOR_IDLE
        //   UPDATE_FILE
        //   GPENTRY
        seqNum = (seqNum << OP_SEQUENCE_SHIFT) + opOffset;
    }
    // (else)
    // Trace syntax version 3 did not allow WAIT_FOR_IDLE, UPDATE_FILE, or
    // GPENTRY commands at all, so there's no issue there.
    //
    // Trace syntax versions 4 and later are ordered by line-number in
    // the test.hdr file, i.e. explicit sequencing aka concept-of-time.

    if (m_OpDisabled)
    {
        return;
    }

    pOp->InitTraceOpDependency(this);

    if (Utl::HasInstance())
    {
        switch (pOp->GetType())
        {
        case TraceOpType::AllocSurface:
        case TraceOpType::AllocVirtual:
        case TraceOpType::AllocPhysical:
        case TraceOpType::Map:
        case TraceOpType::SurfaceView:
        case TraceOpType::FreeSurface:
        case TraceOpType::UpdateFile:
        case TraceOpType::CheckDynamicSurface:
        case TraceOpType::Reloc64:
            TriggerTraceOpEvent(pOp, TraceOpTriggerPoint::Parse);
            break;
        default:
            break;
        }
    }
    m_TraceOps.insert(pair<UINT32, TraceOp*>(seqNum, pOp));
    m_TraceOpLineNos.insert(pair<const TraceOp*, UINT32>(pOp, seqNum));
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseUINT32(UINT32 * pUINT32)
{
    // Someday, we could allow arithmetic expressions here.
    const char * tok = m_TestHdrTokens.front();
    char * p;
    errno = 0;
    *pUINT32 = strtoul(tok, &p, 0);
    // Either 'did not move forward' or 'did not hit the end' will be treated as failure.
    if ((p == tok) ||
        (p && *p != 0) ||
        (ERANGE == errno)) // Catching out of range
    {
        // First character wasn't recognized by strtoul as part of an integer.
        return RC::ILWALID_FILE_FORMAT;
    }

    // Only consume the token if we got a valid integer.
    m_TestHdrTokens.pop_front();
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseUINT64(UINT64 * pUINT64)
{
    // Someday, we could allow arithmetic expressions here.
    const char * tok = m_TestHdrTokens.front();
    char * p;
    errno = 0;
    *pUINT64 = Utility::Strtoull(tok, &p, 0);
    // Either 'did not move forward' or 'did not hit the end' will be treated as failure.
    if ((p == tok) ||
        (p && *p != 0) ||
        (ERANGE == errno)) // Catching out of range
    {
        // First character wasn't recognized by Strtoull as part of an integer.
        return RC::ILWALID_FILE_FORMAT;
    }

    // Only consume the token if we got a valid integer.
    m_TestHdrTokens.pop_front();
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseFileName(string * pstr, const char* cmd)
{
    if (ParseFile::s_NewLine == m_TestHdrTokens.front())
        return RC::ILWALID_FILE_FORMAT;

    *pstr = m_TestHdrTokens.front();
    size_t dummy;
    if (OK != m_TraceFileMgr->Find(*pstr, &dummy))
    {
        // File not found.
        ErrPrintf("%s: couldn't open \"%s\"\n", cmd, pstr->c_str());
        return RC::ILWALID_FILE_FORMAT;
    }

    // Only consume the token if we got a valid file name.
    m_TestHdrTokens.pop_front();
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseString(string * pstr)
{
    if (ParseFile::s_NewLine == m_TestHdrTokens.front())
        return RC::ILWALID_FILE_FORMAT;

    *pstr = m_TestHdrTokens.front();
    m_TestHdrTokens.pop_front();
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseGeometry(vector<UINT32>* geom)
{
    RC rc = OK;
    string geom_str;
    CHECK_RC(ParseString(&geom_str));

    UINT32 dim[3];
    for (int i = sscanf(geom_str.c_str(), "%dx%dx%d", dim, dim+1, dim+2); i != 0; --i)
        geom->insert(geom->begin(), dim[i-1]);

    if (!geom->size())
        return RC::ILWALID_FILE_FORMAT;

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseOffsetLong(pair<UINT32, UINT32>* offset)
{
    RC rc = OK;
    string offset_str;
    CHECK_RC(ParseString(&offset_str));

    UINT32 low, hi;

    if (2 != sscanf(offset_str.c_str(), "%x:%x", &low, &hi))
        return RC::ILWALID_FILE_FORMAT;

    offset->first = low;
    offset->second = hi;

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseFileType(TraceFileType * pTraceFileType)
{
    if (OK != MatchFileType(m_TestHdrTokens.front(), pTraceFileType))
        return RC::ILWALID_FILE_FORMAT;

    m_TestHdrTokens.pop_front();
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseSurfType(SurfaceType * pSurfaceType)
{
    *pSurfaceType = FindSurface(m_TestHdrTokens.front());

    if (*pSurfaceType == SURFACE_TYPE_UNKNOWN)
        return RC::ILWALID_FILE_FORMAT;

    // If -prog_zt_as_ct0 is set, the Z-buffer is used
    // instead of the first color render target.
    else if ((*pSurfaceType == SURFACE_TYPE_COLORA) &&
        (params->ParamPresent("-prog_zt_as_ct0") > 0))
    {
        *pSurfaceType = SURFACE_TYPE_Z;
    }

    m_TestHdrTokens.pop_front();
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseModule(TraceModule ** ppTraceModule)
{
    *ppTraceModule = ModFind(m_TestHdrTokens.front());

    if (NULL == *ppTraceModule)
        return RC::ILWALID_FILE_FORMAT;

    m_TestHdrTokens.pop_front();
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseFileOrSurfaceOrFileType(string * pstr)
{
    SurfaceType st;

    st = FindSurface(m_TestHdrTokens.front());
    if (st == SURFACE_TYPE_UNKNOWN)
    {
        TraceFileType ft;
        if (OK != MatchFileType(m_TestHdrTokens.front(), &ft))
        {
            TraceModule * ptm;
            ptm = ModFind(m_TestHdrTokens.front());
            if (NULL == ptm)
            {
                // Failure
                return RC::ILWALID_FILE_FORMAT;
            }
        }
    }

    // If -prog_zt_as_ct0 is set, the Z-buffer is used
    // instead of the first color render target.
    if ((st == SURFACE_TYPE_COLORA) &&
        (params->ParamPresent("-prog_zt_as_ct0") > 0))
    {
        *pstr = "Z";
    }
    else
    {
        *pstr = string(m_TestHdrTokens.front());
    }

    m_TestHdrTokens.pop_front();
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseChannel(TraceChannel ** ppTraceChannel, ChannelManagedMode mode)
{
    *ppTraceChannel = GetChannel(m_TestHdrTokens.front(), mode);
    if (0 == *ppTraceChannel)
        return RC::ILWALID_FILE_FORMAT;

    m_TestHdrTokens.pop_front();
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseSubChannel(TraceChannel ** ppTraceChannel,
    TraceSubChannel ** ppTraceSubChannel)
{
    *ppTraceChannel = GetLwrrentChannel();
    if (0 == *ppTraceChannel)
    {
        return RC::ILWALID_FILE_FORMAT;
    }
    *ppTraceSubChannel = GetSubChannel(*ppTraceChannel, m_TestHdrTokens.front());
    if ( !(*ppTraceSubChannel) )
    {
        // To support new ACE format and also be compatible with old traces,
        // we'll search all channels if the last channel doesn't have such
        // subchan, which needs ACE traces always use unique subchan names
        // See bug 547246
        *ppTraceChannel = GetChannelHasSubch(m_TestHdrTokens.front());
        if (0 == *ppTraceChannel ||
            0 == (*ppTraceSubChannel = GetSubChannel(*ppTraceChannel, m_TestHdrTokens.front())))
        {
            return RC::ILWALID_FILE_FORMAT;
        }
    }

    m_TestHdrTokens.pop_front();
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseBoolFunc(int * pBoolFunc)
{
    const char * tok = m_TestHdrTokens.front();
    if ((0 == strcmp(tok, "==")) ||
        (0 == strcmp(tok, "=" )))
    {
        *pBoolFunc = TraceOp::BfEqual;
    }
    else if ((0 == strcmp(tok, "!=")) ||
             (0 == strcmp(tok, "<>")))
    {
        *pBoolFunc = TraceOp::BfNotEqual;
    }
    else if (0 == strcmp(tok, ">"))
    {
        *pBoolFunc = TraceOp::BfGreater;
    }
    else if (0 == strcmp(tok, "<"))
    {
        *pBoolFunc = TraceOp::BfLess;
    }
    else if (0 == strcmp(tok, ">="))
    {
        *pBoolFunc = TraceOp::BfGreaterEqual;
    }
    else if (0 == strcmp(tok, "<="))
    {
        *pBoolFunc = TraceOp::BfLessEqual;
    }
    else
    {
        return RC::ILWALID_FILE_FORMAT;
    }

    m_TestHdrTokens.pop_front();
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseRelocPeer(string surfname, UINT32 *pPeer, UINT32* pPeerID)
{
    UINT32 peer = Gpu::MaxNumSubdevices;
    UINT32 peerID = USE_DEFAULT_RM_MAPPING;

    *pPeer = Gpu::MaxNumSubdevices;
    *pPeerID = peerID;

    if ((m_TestHdrTokens.front() != ParseFile::s_NewLine) &&
        (0 == strcmp(m_TestHdrTokens.front(), "PEER")))
    {
        TraceModule *pMod = ModFind(surfname.c_str());

        m_TestHdrTokens.pop_front();
        if (OK != ParseUINT32(&peer))
        {
            return RC::ILWALID_FILE_FORMAT;
        }
        if ((m_TestHdrTokens.front() != ParseFile::s_NewLine) &&
            (0 == strcmp(m_TestHdrTokens.front(), "PEERID")))
        {
            m_TestHdrTokens.pop_front();
            if (OK != ParseUINT32(&peerID))
            {
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        if (pMod)
        {
            pMod->SetPeerID(peerID);
            if (!pMod->IsPeer() && !pMod->GetLoopback())
            {
                WarnPrintf("PEER lwrrently only supported on peer or loopback modules, ignoring\n");
                return OK;
            }
        }
        else
        {
            SurfaceType st = FindSurface(surfname);

            if ((st == SURFACE_TYPE_UNKNOWN) ||
                (m_SurfaceTypeMap.count(st) == 0))
            {
                return RC::ILWALID_FILE_FORMAT;
            }

            if (!m_SurfaceTypeMap[st].bMapLoopback)
            {
                WarnPrintf("PEER lwrrently only supported on loopback surfaces, ignoring\n");
                return OK;
            }

            m_SurfaceTypeMap[st].peerIDs.push_back(peerID);
        }
        *pPeer = peer;
        *pPeerID = peerID;
    }

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseRelocPeer(SurfaceType st, UINT32 *pPeer)
{
    UINT32 peer = Gpu::MaxNumSubdevices;

    *pPeer = Gpu::MaxNumSubdevices;

    if ((m_TestHdrTokens.front() != ParseFile::s_NewLine) &&
        (0 == strcmp(m_TestHdrTokens.front(), "PEER")))
    {
        m_TestHdrTokens.pop_front();
        if ((st == SURFACE_TYPE_UNKNOWN) ||
            (m_SurfaceTypeMap.count(st) == 0) ||
            (OK != ParseUINT32(&peer)))
        {
            return RC::ILWALID_FILE_FORMAT;
        }

        if (!m_SurfaceTypeMap[st].bMapLoopback)
        {
            WarnPrintf("PEER only supported on loopback surfaces, ignoring\n");
        }
        else
        {
            *pPeer = peer;
        }
    }
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::RequireVersion(UINT32 milwer, const char * cmd)
{
    if (m_Version < milwer)
    {
        ErrPrintf("%s requires trace version %d or better, this trace is version %d.\n",
                  cmd, milwer, m_Version);
        return RC::ILWALID_FILE_FORMAT;
    }
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::RequireNotVersion(UINT32 badver, const char * cmd)
{
    if (m_Version == badver)
    {
        ErrPrintf("%s illegal in version %d.\n", cmd, badver);
        return RC::ILWALID_FILE_FORMAT;
    }
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::RequireModLast(const char * cmd)
{
    // Relocation commands operate on the previous surface or pushbuffer
    // (declared with a FILE command).  If we haven't seen one yet, error out.
    if (!ModLast())
    {
        ErrPrintf("Trace: %s found before a FILE command!\n", cmd);
        return RC::ILWALID_FILE_FORMAT;
    }
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::RequireModLocal(TraceModule *pMod, const char * cmd)
{
    // Many commands modify a surface/pushbuffer, this is not supported for
    // non local modules
    if (pMod->IsPeer())
    {
        ErrPrintf("Trace: %s not supported on peer surface %s!\n", cmd, pMod->GetName().c_str());
        return RC::ILWALID_FILE_FORMAT;
    }
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseVERSION()
{
    RC rc;

    if (m_HasVersion)
    {
        ErrPrintf("Cannot specify VERSION multiple times in a single trace,\n");
        ErrPrintf("or specify it after the first line.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    CHECK_RC(ParseUINT32(&m_Version));
    m_HasVersion = true;

#if MIN_TRACE_VERSION == 0
    if (m_Version > MAX_TRACE_VERSION)
#else
    if ((m_Version < MIN_TRACE_VERSION) ||
        (m_Version > MAX_TRACE_VERSION))
#endif
    {
        ErrPrintf("VERSION: trace is of version %d, which is not supported.\n");
        ErrPrintf("MODS lwrrently supports trace versions [%d,%d].\n",
                  MIN_TRACE_VERSION, MAX_TRACE_VERSION);
        return RC::ILWALID_FILE_FORMAT;
    }
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseCLASS()
{
    RC rc;
    TraceChannel * pTraceChannel = 0;
    TraceSubChannel* pTraceSubChannel = 0;
    shared_ptr<SubCtx> pSubCtx;

    if (m_Version >= 3)
    {
        // Version 3 takes a channel/subchannel name.

        if ( (OK != ParseSubChannel(&pTraceChannel, &pTraceSubChannel )) &&
            (OK != ParseChannel(&pTraceChannel)) )
        {
            ErrPrintf("Channel/SubChannel name expected.\n");
            return RC::ILWALID_FILE_FORMAT;
        }
    }
    else
    {
        if (params->ParamPresent("-tsg_name"))
        {
            string tsgName = params->ParamStr("-tsg_name");
            TraceTsg * pTraceTsg = GetTraceTsg(tsgName);
            if (0 == pTraceTsg)
            {
                AddTraceTsg(tsgName, GetVAS("default"), true);
            }
            pSubCtx = GetSubCtx("");
            // If pSubCtx != Null, the subctx should come from the command line since the
            // SUBCONTEXT is valid after VERSION 3. And the vaspace and tsg are the same reason.
            // So vaspace/tsg should come from the command line too.
            rc = AddChannel("default", tsgName, CPU_MANAGED, GetVAS("default"), pSubCtx);
        }
        else
        {
            // Version 2 and earlier implicitly have one channel named "default".
            shared_ptr<SubCtx> pTemp;
            rc = AddChannel("default", "", CPU_MANAGED, GetVAS("default"), pTemp);
        }

        if (OK != rc)
        {
            ErrPrintf("Parse CLASS command failed.\n");
            return rc;
        }
        pTraceChannel = GetLwrrentChannel();
    }

    if ( pTraceSubChannel && pTraceSubChannel->GetClass() )
    {
        ErrPrintf("SubChannel \"%s\" already has a CLASS.\n",
                  pTraceSubChannel->GetName().c_str());
        return RC::ILWALID_FILE_FORMAT;
    }

    if ( !pTraceSubChannel )
    {
        // No subchannel defined, create a default subchannel
        pTraceSubChannel = pTraceChannel->AddSubChannel("default", params);
        if ( !pTraceSubChannel )
        {
            return RC::SOFTWARE_ERROR;
        }
    }

    UINT32 forceClass = params->ParamUnsigned("-force_class", 0);

    if ((forceClass > 0) && pTraceChannel->HasMultipleSubChs())
    {
        ErrPrintf("-force_class cannot be used since channel %s has multiple subchannels\n",
                  pTraceChannel->GetName().c_str());

        ErrPrintf("Use -force_subch_class to change the class for specific subchannel.\n");

        return RC::SOFTWARE_ERROR;
    }

    UINT32 argNum = params->ParamPresent("-force_subch_class");

    for (UINT32 i = 0; i < argNum; ++i)
    {
        if (params->ParamNUnsigned("-force_subch_class", i, 0) ==
            pTraceSubChannel->GetSubChNum())
        {
            forceClass = params->ParamNUnsigned("-force_subch_class", i, 1);
            break;
        }
    }

    // Parse a list of 1 or more class names, choose the highest-numbered
    // class supported by this GPU.

    UINT32 finalClass = 0;
    LwRm* pLwRm = m_Test->GetLwRmPtr();

    while (m_TestHdrTokens.front() != ParseFile::s_NewLine)
    {
        UINT32 tempForceClass = forceClass;
        UINT32 tempClass = m_Test->ClassStr2Class(m_TestHdrTokens.front());
        m_TestHdrTokens.pop_front();

        argNum = params->ParamPresent("-force_class_to_class");

        for (UINT32 i = 0; i < argNum; ++i)
        {
            if (params->ParamNUnsigned("-force_class_to_class", i, 0) ==
                tempClass)
            {
                tempForceClass = params->ParamNUnsigned("-force_class_to_class", i, 1);
                if (pLwRm->IsClassSupported(tempForceClass, m_Test->GetBoundGpuDevice()))
                   break;
            }
        }

        if (tempForceClass > 0)
        {
            if (!pLwRm->IsClassSupported(tempForceClass,
                m_Test->GetBoundGpuDevice()))
            {
                ErrPrintf("Class 0x%x forced via command-line argument but is not supported by GPU.\n", 
                    tempForceClass);
                ErrPrintf("List of supported classes: %s\n", 
                    MDiagUtils::GetSupportedClassesString(
                        pLwRm, m_Test->GetBoundGpuDevice()).c_str());

                return RC::SOFTWARE_ERROR;
            }

            if (tempForceClass > finalClass)
            {
                finalClass = tempForceClass;
            }
        }
        else if ((tempClass > finalClass) &&
            pLwRm->IsClassSupported(tempClass, m_Test->GetBoundGpuDevice()))
        {
            finalClass = tempClass;
        }
    }

    if (finalClass == 0)
    {
        ErrPrintf("GPU does not support any class specified in the trace(%s/%s)\n",
            pTraceChannel->GetName().c_str(), pTraceSubChannel->GetName().c_str());
        ErrPrintf("List of supported classes: %s\n", 
            MDiagUtils::GetSupportedClassesString(
                pLwRm, m_Test->GetBoundGpuDevice()).c_str());

        return RC::ILWALID_FILE_FORMAT;
    }

    pTraceSubChannel->SetClass(finalClass);
    CHECK_RC(SetClassParam(finalClass, pTraceChannel, pTraceSubChannel));

    pSubCtx = pTraceChannel->GetSubCtx();
    if (pSubCtx)
    {
        if (pTraceSubChannel->GrSubChannel())
        {
            CHECK_RC(pSubCtx->SetGrSubContext());
        }
        if (pTraceSubChannel->I2memSubChannel())
        {
            CHECK_RC(pSubCtx->SetI2memSubContext());
        }
    }

    if (Platform::GetSimulationMode() == Platform::Amodel)
        m_Test->GetBoundGpuDevice()->SetGobHeight(8);

    if (m_Test->GetTextureHeaderFormat() == TextureHeader::HeaderFormat::Unknown)
    {
        if (finalClass >= LWGpuClasses::GPU_CLASS_HOPPER)
        {
            m_Test->SetTextureHeaderFormat(TextureHeader::HeaderFormat::Hopper);
        }
        else if (finalClass >= LWGpuClasses::GPU_CLASS_MAXWELL)
        {
            m_Test->SetTextureHeaderFormat(TextureHeader::HeaderFormat::Maxwell);
        }
        else
        {
            m_Test->SetTextureHeaderFormat(TextureHeader::HeaderFormat::Kepler);
        }
    }

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseESCAPE_WRITE_FILE()
{
    RC rc;
    if (ParseFile::s_NewLine == m_TestHdrTokens.front())
    {
        ErrPrintf("ESCAPE_WRITE_FILE: key expected\n");
        return RC::ILWALID_FILE_FORMAT;
    }
    string key = m_TestHdrTokens.front();
    m_TestHdrTokens.pop_front();

    if (ParseFile::s_NewLine == m_TestHdrTokens.front())
    {
        ErrPrintf("ESCAPE_WRITE_FILE: filename expected\n");
        return RC::ILWALID_FILE_FORMAT;
    }
    string fname = m_TestHdrTokens.front();
    m_TestHdrTokens.pop_front();

    TraceFileMgr::Handle h;
    rc = m_TraceFileMgr->Open(fname, &h);
    if (OK != rc)
    {
        // Reduced to a warning because of strange flakiness where
        // certain files intermittently cannot be found in the .tgz.
        // This is only a debugging feature, so this can't hurt
        // anything.
        WarnPrintf("Couldn't locate file '%s' in trace\n", fname.c_str());
        return OK; //Nop
    }

    // Read it in as a null-terminated string
    size_t Size = m_TraceFileMgr->Size(h);
    char *Data = new char[Size+1];
    m_TraceFileMgr->Read(Data, h);
    Data[Size] = 0;

    //
    // Escape write it into the simulator
    //
    // subdevMask is optional
    // write all gpudev if no subdevmask
    GpuDevice *pGpuDev = m_Test->GetBoundGpuDevice();
    UINT32 subDevNum = pGpuDev->GetNumSubdevices();
    UINT32 subdevMask;
    if (OK != ParseUINT32(&subdevMask))
    {
        subdevMask = (1 << subDevNum) - 1;
    }
    MASSERT(subDevNum < sizeof(subdevMask) * 8);

    if (m_Version < 4)
    {
        if (Platform::GetSimulationMode() == Platform::Amodel)
        {
            for (UINT32 subdev = 0; subdev < subDevNum; subdev++)
            {
                if (subdevMask & (1 << subdev))
                {
                    GpuSubdevice *pSubdev = pGpuDev->GetSubdevice(subdev);
                    if (pSubdev->EscapeWriteBuffer(key.c_str(),
                                                   0, Size, (void*)Data))
                    {
                        // This is only a debugging feature
                        // Igoring the failure returned will not hurt anything.
                        WarnPrintf("%s: Ignore failure returned by EscapeWriteBuffer(\"%s\").\n",
                                   __FUNCTION__, key.c_str());
                    }
                }
            }
        }
    }
    else
    {
        TraceOpEscapeWriteBuffer *
            pOp = new TraceOpEscapeWriteBuffer(pGpuDev,
                                               subdevMask,
                                               key,
                                               0,
                                               Size,
                                               Data);
        InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);
    }

    m_TraceFileMgr->Close(h);
    delete[] Data;

    return rc;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseESCAPE_WRITE()
{
    RC rc;
    UINT32 subdevMask;
    string path;
    UINT32 index;
    UINT32 size;
    UINT32 data;

    CHECK_RC(RequireVersion(4, "ESCAPE_WRITE"));

    if (Platform::GetSimulationMode() == Platform::Hardware)
    {
        ErrPrintf("ESCAPE_WRITE not supported on hardware.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    if ((OK != ParseUINT32(&subdevMask)) ||
        (OK != ParseString(&path)) ||
        (OK != ParseUINT32(&index)) ||
        (OK != ParseUINT32(&size)) ||
        (OK != ParseUINT32(&data)))
    {
        ErrPrintf("ESCAPE_WRITE needs <subdev mask> <path> <index> <size> <data> args.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    TraceOpEscapeWrite * pOp = new TraceOpEscapeWrite(m_Test->GetBoundGpuDevice(),
                                                      subdevMask,
                                                      path,
                                                      index,
                                                      size,
                                                      data);
    InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseESCAPE_COMPARE()
{
    RC rc;
    UINT32 subdevMask;
    string path;
    UINT32 index;
    UINT32 size;
    int    compare;
    UINT32 desiredValue;
    UINT32 timeoutMs;

    CHECK_RC(RequireVersion(4, "ESCAPE_COMPARE"));

    if (Platform::GetSimulationMode() == Platform::Hardware)
    {
        ErrPrintf("ESCAPE_COMPARE not supported on hardware.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    if ((OK != ParseUINT32(&subdevMask)) ||
        (OK != ParseString(&path)) ||
        (OK != ParseUINT32(&index)) ||
        (OK != ParseUINT32(&size)) ||
        (OK != ParseBoolFunc(&compare)) ||
        (OK != ParseUINT32(&desiredValue)))
    {
        ErrPrintf("ESCAPE_COMPARE needs <subdev mask> <path> <index> <size> <compare_func> <desired value> args.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    if (ParseFile::s_NewLine != m_TestHdrTokens.front())
    {
        if (OK != ParseUINT32(&timeoutMs))
        {
            ErrPrintf("ESCAPE_COMPARE: timeout expected\n");
            return RC::ILWALID_FILE_FORMAT;
        }
    }
    else
    {
        timeoutMs = TraceOp::TIMEOUT_IMMEDIATE;
    }

    TraceOpEscapeCompare * pOp = new TraceOpEscapeCompare(m_Test->GetBoundGpuDevice(),
                                                          subdevMask,
                                                          path,
                                                          index,
                                                          size,
                                                          (TraceOp::BoolFunc)compare,
                                                          desiredValue,
                                                          timeoutMs);
    InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);

    return OK;
}

RC GpuTrace::ParseReg(UINT32       * pSubdevMask,
                      string       * pReg,
                      string       * pField,
                      UINT32       * pMask,
                      const char   * feature)
{
    if (OK != ParseUINT32(pSubdevMask))

    {
        ErrPrintf("%s: subdevice mask expected\n", feature);
        return RC::ILWALID_FILE_FORMAT;
    }

    if (OK != ParseString(pReg))
    {
        ErrPrintf("%s: register name expected\n", feature);
        return RC::ILWALID_FILE_FORMAT;
    }

    if ((OK != ParseUINT32(pMask)) &&
        (OK != ParseString(pField)))
    {
        ErrPrintf("%s: mask, type, or field name expected\n", feature);
        return RC::ILWALID_FILE_FORMAT;
    }

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParsePROGRAM_ZBC_COLOR()
{
    RC rc;
    UINT32   colorFB[4];
    UINT32   colorDS[4];
    string   colorFormat;

    CHECK_RC(RequireVersion(5, "PROGRAM_ZBC_COLOR"));

    // Parse the rest of the optional args in any order, up to the newline.
    while (ParseFile::s_NewLine != m_TestHdrTokens.front())
    {
        const char * tok = m_TestHdrTokens.front();
        m_TestHdrTokens.pop_front();

        if (OK == RetriveZbcStringField(tok, "DS_COLOR_FMT",     &colorFormat))
        {
            continue;
        }
        else if (OK == RetriveZblwINT32Field(tok, "DS_COLOR_R",  &colorDS[0]))
        {
            continue;
        }
        else if (OK == RetriveZblwINT32Field(tok, "DS_COLOR_G",  &colorDS[1]))
        {
            continue;
        }
        else if (OK == RetriveZblwINT32Field(tok, "DS_COLOR_B",  &colorDS[2]))
        {
            continue;
        }
        else if (OK == RetriveZblwINT32Field(tok, "DS_COLOR_A",  &colorDS[3]))
        {
            continue;
        }
        else if (OK == RetriveZblwINT32Field(tok, "LTC_COLOR_0", &colorFB[0]))
        {
            continue;
        }
        else if (OK == RetriveZblwINT32Field(tok, "LTC_COLOR_1", &colorFB[1]))
        {
            continue;
        }
        else if (OK == RetriveZblwINT32Field(tok, "LTC_COLOR_2", &colorFB[2]))
        {
            continue;
        }
        else if (OK == RetriveZblwINT32Field(tok, "LTC_COLOR_3", &colorFB[3]))
        {
            continue;
        }
        else
        {
            ErrPrintf("Unrecognized tok %s when parsing PROGRAM_ZBC_COLOR\n", tok);
            return RC::ILWALID_FILE_FORMAT;
        }

    } // end while tok != ParseFile::s_NewLine

    // Initial ZBC clear format map.
    if (m_ZbcClearFmtMap.size()== 0)
    {
        InitZbcClearFmtMap();
    }

    // Find color format UINT32 value in ZBC color clear format map
    auto colorMapIter = m_ZbcClearFmtMap.find(colorFormat);
    if (colorMapIter == m_ZbcClearFmtMap.end())
    {
        ErrPrintf("Unsupported color format '%s'\n", colorFormat.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }
    UINT32 colorFormatUINT32 = colorMapIter->second;

    TraceOpProgramZBCColor *pOpZBCColor = new TraceOpProgramZBCColor(m_Test,
        colorFormatUINT32,
        colorFormat, // Color format string
        colorFB,
        colorDS,
        false // Set bSkipL2Table to false
        );
    InsertOp(m_LineNumber, pOpZBCColor, OP_SEQUENCE_OTHER);

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParsePROGRAM_ZBC_DEPTH()
{
    RC rc;

    UINT32   depth = 0;
    string   depthFormat;

    CHECK_RC(RequireVersion(5, "PROGRAM_ZBC_DEPTH"));

    // Parse the rest of the optional args in any order, up to the newline.
    while (ParseFile::s_NewLine != m_TestHdrTokens.front())
    {
        const char * tok = m_TestHdrTokens.front();
        m_TestHdrTokens.pop_front();

        if (OK == RetriveZbcStringField(tok, "DS_Z_FMT",  &depthFormat))
        {
            continue;
        }
        else if (OK == RetriveZblwINT32Field(tok, "DS_Z", &depth))
        {
            continue;
        }
        else
        {
            ErrPrintf("Unrecognized tok %s when parsing PROGRAM_ZBC_DEPTH\n", tok);
            return RC::ILWALID_FILE_FORMAT;
        }
    } // End while tok != ParseFile::s_NewLine

    // Initial ZBC depth clear format map.
    if (m_ZbcClearFmtMap.size()== 0)
    {
        InitZbcClearFmtMap();
    }

    // Find depth format UINT32 value in ZBC depth clear format map
    auto depthMapIter = m_ZbcClearFmtMap.find(depthFormat);
    if (depthMapIter == m_ZbcClearFmtMap.end())
    {
        ErrPrintf("Unsupported depth format '%s'\n", depthFormat.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }
    UINT32 depthFormatUINT32 = depthMapIter->second;

    TraceOpProgramZBCDepth *pOpZBCDepth = new TraceOpProgramZBCDepth(m_Test,
        depthFormatUINT32,
        depthFormat,// Depth format string
        depth,
        false // Set bSkipL2Table to false
        );
    InsertOp(m_LineNumber, pOpZBCDepth, OP_SEQUENCE_OTHER);

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParsePROGRAM_ZBC_STENCIL()
{
    RC rc;
    string stencilFormat;
    UINT32 stencil = 0;

    CHECK_RC(RequireVersion(5, "PROGRAM_ZBC_STENCIL"));

    while (ParseFile::s_NewLine != m_TestHdrTokens.front())
    {
        const char * tok = m_TestHdrTokens.front();
        m_TestHdrTokens.pop_front();

        if (OK == RetriveZbcStringField(tok, "DS_S_FMT",  &stencilFormat))
        {
            continue;
        }
        else if (OK == RetriveZblwINT32Field(tok, "DS_S", &stencil))
        {
            continue;
        }
        else
        {
            ErrPrintf("Unrecognized tok %s when parsing PROGRAM_ZBC_STENCIL\n", tok);
            return RC::ILWALID_FILE_FORMAT;
        }
    }

    if (m_ZbcClearFmtMap.size()== 0)
    {
        InitZbcClearFmtMap();
    }

    auto stencilMapIter = m_ZbcClearFmtMap.find(stencilFormat);
    if (stencilMapIter == m_ZbcClearFmtMap.end())
    {
        ErrPrintf("Unsupported stencil format '%s'\n", stencilFormat.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }
    UINT32 stencilFormatUINT32 = stencilMapIter->second;

    TraceOpProgramZBCStencil *pOpZBCStencil = new TraceOpProgramZBCStencil(m_Test,
        stencilFormatUINT32,
        stencilFormat,
        stencil,
        false
        );
    InsertOp(m_LineNumber, pOpZBCStencil, OP_SEQUENCE_OTHER);

    return rc;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParsePRI_WRITE()
{
    RC rc;
    UINT32 subdevMask;
    string reg;
    string field;
    UINT32 mask;
    UINT32 data;
    TraceModule * module;
    TraceOpPriWrite * pOp;

    CHECK_RC(RequireVersion(4, "PRI_WRITE"));
    CHECK_RC(ParseReg(&subdevMask, &reg, &field, &mask, "PRI_WRITE"));

    if ((field == "OFFSET_HI") ||
        (field == "OFFSET_LO") ||
        (field == "PMU_OFFSET_HI") ||
        (field == "PMU_OFFSET_LO") ||
        (field == "BUF_OFFSET") ||
        (field == "SIZE") ||
        (field == "WIDTH") ||
        (field == "HEIGHT"))
    {
        if (OK != ParseModule(&module))
        {
            ErrPrintf("PRI_WRITE: filename expected\n");
            return RC::ILWALID_FILE_FORMAT;
        }
        if (((field == "PMU_OFFSET_HI") || (field == "PMU_OFFSET_LO")) &&
            (module->IsPeer() || !module->IsPMUSupported()))
        {
            ErrPrintf("PRI_WRITE: PMU_OFFSET_HI and PMU_OFFSET_LO only supported on non-peer PMU_x modules\n");
            return RC::UNSUPPORTED_FUNCTION;
        }
        pOp = new TraceOpPriWrite(m_Test, subdevMask, reg, field, module);
    }
    else
    {
        if (OK != ParseUINT32(&data))
        {
            ErrPrintf("PRI_WRITE: data expected\n");
            return RC::ILWALID_FILE_FORMAT;
        }
        if (!field.empty())
        {
            pOp = new TraceOpPriWrite(m_Test, subdevMask, reg, field, data);
        }
        else
        {
            pOp = new TraceOpPriWrite(m_Test, subdevMask, reg, mask, data);
        }
    }

    CHECK_RC(pOp->SetupRegs());

    InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParsePRI_COMPARE()
{
    RC rc;
    UINT32 subdevMask;
    string reg;
    string field;
    UINT32 mask;
    int compare;
    UINT32 desiredValue;
    UINT32 timeoutMs = 0;

    CHECK_RC(RequireVersion(4, "PRI_COMPARE"));
    CHECK_RC(ParseReg(&subdevMask, &reg, &field, &mask, "PRI_COMPARE"));

    if (OK != ParseBoolFunc(&compare))
    {
        ErrPrintf("PRI_COMPARE: boolean function expected\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    if (OK != ParseUINT32(&desiredValue))
    {
        ErrPrintf("PRI_COMPARE: desired value expected\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    if (ParseFile::s_NewLine != m_TestHdrTokens.front())
    {
        if (OK != ParseUINT32(&timeoutMs))
        {
            ErrPrintf("PRI_COMPARE: timeout expected\n");
            return RC::ILWALID_FILE_FORMAT;
        }
    }
    else
    {
        timeoutMs = TraceOp::TIMEOUT_IMMEDIATE;
    }

    if (params->ParamPresent("-pri_compare_timeout"))
    {
        timeoutMs = params->ParamUnsigned("-pri_compare_timeout");
    }

    TraceOpPriCompare * pOp;
    if (!field.empty())
    {
        pOp = new TraceOpPriCompare(m_Test,
                                    subdevMask,
                                    reg,
                                    field,
                                    (TraceOp::BoolFunc)compare,
                                    desiredValue,
                                    timeoutMs);
    }
    else
    {
        pOp = new TraceOpPriCompare(m_Test,
                                    subdevMask,
                                    reg,
                                    mask,
                                    (TraceOp::BoolFunc)compare,
                                    desiredValue,
                                    timeoutMs);
    }
    CHECK_RC(pOp->SetupRegs());
    InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseCFG_WRITE()
{
    RC rc;
    UINT32 subdevMask;
    string reg;
    string field;
    UINT32 mask;
    UINT32 data;
    TraceOpCfgWrite * pOp;

    CHECK_RC(RequireVersion(4, "CFG_WRITE"));
    CHECK_RC(ParseReg(&subdevMask, &reg, &field, &mask, "CFG_WRITE"));

    if (OK != ParseUINT32(&data))
    {
        ErrPrintf("CFG_WRITE: data expected\n");
        return RC::ILWALID_FILE_FORMAT;
    }
    if (!field.empty())
    {
        pOp = new TraceOpCfgWrite(m_Test->GetBoundGpuDevice(),
                                  subdevMask, reg, field, data);
    }
    else
    {
        pOp = new TraceOpCfgWrite(m_Test->GetBoundGpuDevice(),
                                  subdevMask, reg, mask, data);
    }

    CHECK_RC(pOp->SetupRegs());

    InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseCFG_COMPARE()
{

    RC rc;
    UINT32 subdevMask;
    string reg;
    string field;
    UINT32 mask;
    int compare;
    UINT32 desiredValue;
    UINT32 timeoutMs = 0;

    CHECK_RC(RequireVersion(4, "CFG_COMPARE"));
    CHECK_RC(ParseReg(&subdevMask, &reg, &field, &mask, "CFG_COMPARE"));

    if (OK != ParseBoolFunc(&compare))
    {
        ErrPrintf("CFG_COMPARE: boolean function expected\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    if (OK != ParseUINT32(&desiredValue))
    {
        ErrPrintf("CFG_COMPARE: desired value expected\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    if (ParseFile::s_NewLine != m_TestHdrTokens.front())
    {
        if (OK != ParseUINT32(&timeoutMs))
        {
            ErrPrintf("CFG_COMPARE: timeout expected\n");
            return RC::ILWALID_FILE_FORMAT;
        }
    }
    else
    {
        timeoutMs = TraceOp::TIMEOUT_IMMEDIATE;
    }

    TraceOpCfgCompare * pOp;
    if (!field.empty())
    {
        pOp = new TraceOpCfgCompare(m_Test->GetBoundGpuDevice(),
                                    subdevMask,
                                    reg,
                                    field,
                                    (TraceOp::BoolFunc)compare,
                                    desiredValue,
                                    timeoutMs);
    }
    else
    {
        pOp = new TraceOpCfgCompare(m_Test->GetBoundGpuDevice(),
                                    subdevMask,
                                    reg,
                                    mask,
                                    (TraceOp::BoolFunc)compare,
                                    desiredValue,
                                    timeoutMs);
    }
    CHECK_RC(pOp->SetupRegs());
    InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseFILE(UINT32 DebugModuleSize, GpuTrace::TraceSource Source)
{
    RC rc = OK;

    // The FILE statement can either be used to declare a surface or to load
    // pushbuffer data.
    //
    // A surface declaration looks like this:
    //   FILE <name> <type> [tx idx] [swap] [crc] [protection] [location] [VP2 format] [encryption]
    //
    // A pushbuffer data load looks like this:
    //   FILE <name> Pushbuffer [swap]
    //
    // Also, in version 4, INLINE_FILE may be used instead, followed by
    // an arbitrary number of lines containing 32-bit integers, followed by
    // a line consisting of INLINE_FILE_END.

    if (Source == GpuTrace::FROM_INLINE_FILE)
    {
        CHECK_RC( RequireVersion(4, "INLINE_FILE") );
    }

    // First arg: <filename>

    TraceModule * pOldMod;
    if (OK == ParseModule(&pOldMod))
    {
        ErrPrintf("FILE: filename \"%s\" already used!\n", pOldMod->GetName().c_str());
        return RC::ILWALID_FILE_FORMAT;
    }

    string fname;
    if (OK != ParseString(&fname))
    {
        ErrPrintf("FILE: <filename> expected\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    // Second arg: <filetype>
    TraceFileType ftype;
    if (OK != ParseFileType(&ftype))
    {
        ErrPrintf("FILE: filetype \"%s\" invalid\n", m_TestHdrTokens.front());
        return RC::ILWALID_FILE_FORMAT;
    }

    // Backwards compatibility: ignore "FB n" in version 0.
    if ((m_Version == 0) && (0 == strcmp("FB", m_TestHdrTokens.front())))
    {
        m_TestHdrTokens.pop_front();
        if (ParseFile::s_NewLine != m_TestHdrTokens.front())
        m_TestHdrTokens.pop_front();
    }

    // Third arg: [optional texture index]
    UINT32 txidx;
    bool bValidTxIdx = false;
    if (OK == ParseUINT32(&txidx))
        bValidTxIdx = true;

    // Fourth arg: [optional PEER specification]
    // If specified, all arguments other than those specifying the peer test
    // name and peer file name are ignored (they are retrieved from the peer
    // file
    if (!strcmp(m_TestHdrTokens.front(), "PEER"))
    {
        string peerTestName;
        string peerFileName;

        // Peering is not supported for pushbuffers or texture headers
        if ((ftype == FT_PUSHBUFFER) || (ftype == FT_TEXTURE_HEADER))
        {
            ErrPrintf("FILE: Invalid file type (%d) for PEER mapping\n",
                      ftype);
            return RC::ILWALID_FILE_FORMAT;
        }

        m_TestHdrTokens.pop_front();

        if ((OK != ParseString(&peerTestName)) ||
            (OK != ParseString(&peerFileName)) ||
            (m_TestHdrTokens.front() != ParseFile::s_NewLine))
        {
            ErrPrintf("FILE: PEER <peer test> <peer file> only expected\n");
            return RC::ILWALID_FILE_FORMAT;
        }

        // Allocate the new module.
        Trace3DTest *peerTest = m_Test->FindTest(peerTestName);
        if (peerTest == NULL)
        {
            ErrPrintf("FILE: Unable to find peer test \"%s\"\n",
                      peerTestName.c_str());
            return RC::ILWALID_FILE_FORMAT;
        }

        // Notify the peer test which finename needs to be peer'd.  Note that
        // the peer test may not have yet been setup so it is not possible to
        // determine if the peer test alwtally contains that file yet
        peerTest->AddPeerMappedSurface(m_Test->GetBoundGpuDevice(), peerFileName);

        PeerTraceModule * newMod = new PeerTraceModule(m_Test, fname,
                                                       ftype, peerTest,
                                                       peerFileName);
        m_Modules.push_back((TraceModule *)newMod);
        m_PeerModules.push_back((TraceModule *)newMod);

        newMod->SetDebugPrintSize(DebugModuleSize);
        if (bValidTxIdx)
            newMod->AddTextureIndex(txidx);

        // Ignore any other flags on the FILE line.  The remainder of the
        // commands are not relevant to peer modules.
        while (ParseFile::s_NewLine != m_TestHdrTokens.front())
        {
            m_TestHdrTokens.pop_front();
        }
        return OK;
    }

    size_t fsize;
    if (Source == GpuTrace::FROM_INLINE_FILE || Source == GpuTrace::FROM_CONSTANT)
    {
        fsize = 0;
    }
    else
    {
        if (OK != (rc = m_TraceFileMgr->Find(fname, &fsize)))
        {
            ErrPrintf("FILE: couldn't open \"%s\"\n", fname.c_str());
            return rc;
        }
    }

    // Allocate the new module.
    GenericTraceModule * newMod = new GenericTraceModule(m_Test, fname, ftype, fsize);
    m_Modules.push_back((TraceModule *)newMod);
    m_GenericModules.push_back((TraceModule *)newMod);

    if (bValidTxIdx)
        newMod->AddTextureIndex(txidx);

    bool compressTexImage = (params->ParamPresent("-compress_tex_image") > 0);
    bool compressTexDepth = (params->ParamPresent("-compress_tex_depth") > 0);

    newMod->SetImageRTTCompressed(compressTexImage);
    newMod->SetDepthRTTCompressed(compressTexDepth);

    newMod->SetDebugPrintSize(DebugModuleSize);

    // Keep a few flags around to warn users of surprising interactions
    // between features.
    bool hasSwapFile = false;
    bool hasSwapSize = false;
    bool hasVp2Bigendian = false;
    bool isChecked = false;
    string swapFileName;
    bool hasSizePerWarp = false;
    bool hasSizePerTpc = false;
    UINT32 sizePerWarp = 0;
    UINT32 sizePerTpc = 0;
    string vasName, subctxName, tsgName;
    TraceModule::ModCheckInfo checkInfo;

    // Parse the rest of the optional args in any order, up to the newline.
    while (ParseFile::s_NewLine != m_TestHdrTokens.front())
    {
        const char * tok = m_TestHdrTokens.front();
        m_TestHdrTokens.pop_front();

        if (0 == strcmp(tok, "SWAP_SIZE"))
        {
            UINT32 swapSz;
            if (OK != ParseUINT32(&swapSz))
            {
                ErrPrintf("SWAP_SIZE needs <int> arg, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
            newMod->SetSwapInfo(swapSz);
            hasSwapSize = true;
        }
        else if (0 == strcmp(tok, "SWAP_FILE"))
        {
            if (OK != ParseFileName(&swapFileName, "FILE"))
            {
                ErrPrintf("SWAP_FILE needs <fname> arg, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
            // Don't apply the replacement filename until the end of this function.
            hasSwapFile = true;
        }
        else if (ftype == FT_PUSHBUFFER)
        {
            if (0 == strcmp(tok, "ALLOC_SURFACE"))
            {
                newMod->SetAllocPushbufSurf(true);
            }
            else if (0 == strcmp(tok, "VPR"))
            {
                string vpr;
                if (OK != ParseString(&vpr))
                {
                    ErrPrintf("VPR expects ON or OFF; got \"%s\".\n", m_TestHdrTokens.front());
                    return RC::ILWALID_FILE_FORMAT;
                }
                else if (vpr == "ON")
                {
                    newMod->SetVpr(true);
                }
                else if (vpr == "OFF")
                {
                    newMod->SetVpr(false);
                }
                else
                {
                    ErrPrintf("VPR expects ON or OFF; got \"%s\".\n", m_TestHdrTokens.front());
                    return RC::ILWALID_FILE_FORMAT;
                }
            }
            else if (0 == strcmp(tok, "SUBCONTEXT"))
            {
                if (OK != ParseString(&subctxName))
                {
                    ErrPrintf("SUBCONTEXT expects <subcontext Name> in FILE.\n");
                    return RC::ILWALID_FILE_FORMAT;
                }

            }
            else if (0 == strcmp(tok, "ADDRESS_SPACE"))
            {
                if(OK != ParseString(&vasName))
                {
                    ErrPrintf("ADDRESS_SPACE need a name.\n");
                    return RC::ILWALID_FILE_FORMAT;
                }
            }
            else
            {
                // The rest of the optional args are only allowed for non-pushbuffer surfaces.
                ErrPrintf("Filetype Pushbuffer does not support this option: \"%s\".\n", tok);
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        // All optional args from here down apply only to non-Pushbuffer FILEs.
        else if (0 == strcmp(tok, "CRC_CHECK"))
        {
            if (OK != ParseString(&checkInfo.CheckFilename))
            {
                ErrPrintf("CRC_CHECK needs <key> arg.\n");
                return RC::ILWALID_FILE_FORMAT;
            }
            if (isChecked)
            {
                ErrPrintf("Trace: only one of CRC_CHECK, CRC_CHECK_RANGE or REFERENCE_CHECK may be used per module\n");
                return RC::ILWALID_FILE_FORMAT;
            }
            isChecked = true;

            checkInfo.CheckMethod = CRC_CHECK;
        }
        else if (0 == strcmp(tok, "CRC_CHECK_RANGE"))
        {
            if ((OK != ParseString(&checkInfo.CheckFilename)) ||
                (OK != ParseUINT64(&checkInfo.Offset)) ||
                (OK != ParseUINT64(&checkInfo.Size)))
            {
                ErrPrintf("CRC_CHECK_RANGE needs <key> <offset> <size> args.\n");
                return RC::ILWALID_FILE_FORMAT;
            }
            if (isChecked)
            {
                ErrPrintf("Trace: only one of  CRC_CHECK, CRC_CHECK_RANGE or REFERENCE_CHECK may be used per module\n");
                return RC::ILWALID_FILE_FORMAT;
            }
            isChecked = true;
            checkInfo.CheckMethod = CRC_CHECK;
        }
        else if (0 == strcmp(tok, "REFERENCE_CHECK"))
        {
            if (OK != ParseString(&checkInfo.CheckFilename))
            {
                ErrPrintf("REFERENCE_CHECK needs <key> arg.\n");
                return RC::ILWALID_FILE_FORMAT;
            }
            if (isChecked)
            {
                ErrPrintf("Trace: only one of CRC_CHECK, CRC_CHECK_RANGE or REFERENCE_CHECK may be used per module\n");
                return RC::ILWALID_FILE_FORMAT;
            }
            isChecked = true;

            checkInfo.CheckMethod = REF_CHECK;

            m_HasRefCheckModules = true;
        }
        else if (0 == strcmp(tok, "CRC_CHECK_MUST_MISMATCH"))
        {
            newMod->SetCrcCheckMatch(false);
        }
        else if (0 == strcmp(tok, "READ_WRITE"))
        {
            CHECK_RC(RequireVersion(1, tok));
            newMod->SetProtect(Memory::ReadWrite);
        }
        else if (0 == strcmp(tok, "WRITE_ONLY"))
        {
            CHECK_RC(RequireVersion(1, tok));
            newMod->SetProtect(Memory::Writeable);
        }
        else if (0 == strcmp(tok, "COHERENT_SYSTEM_MEMORY"))
        {
            CHECK_RC(RequireVersion(2, tok));
            newMod->SetLocation(_DMA_TARGET_COHERENT);
        }
        else if (0 == strcmp(tok, "NONCOHERENT_SYSTEM_MEMORY"))
        {
            CHECK_RC(RequireVersion(2, tok));
            newMod->SetLocation(_DMA_TARGET_NONCOHERENT);
        }
        else if (0 == strncmp(tok, "VP2_", strlen("VP2_")))
        {
            const char * format = tok;
            CHECK_RC(RequireVersion(2, format));
            if ( !(newMod->IsVP2TilingSupported()) )
            {
                ErrPrintf("%s can be applied only to VP2_0, ..., VP2_9\n", format);
                return RC::ILWALID_FILE_FORMAT;
            }

            UINT32 width;
            UINT32 height;
            UINT32 bpp;
            string endian;

            if ((OK != ParseUINT32(&width)) ||
                (OK != ParseUINT32(&height)) ||
                (OK != ParseUINT32(&bpp)) ||
                (OK != ParseString(&endian)))
            {
                ErrPrintf("%s requires <w> <h> <bpp> <endian> args.\n", format);
                return RC::ILWALID_FILE_FORMAT;
            }
            if (0 == strcmp(endian.c_str(), "BE"))
                hasVp2Bigendian = true;

            UINT32 pitch = 0;
            if (newMod->IsVP2TilingPitchParameterNeeded(format))
            {
                if (OK != ParseUINT32(&pitch))
                {
                    ErrPrintf("%s requires <w> <h> <bpp> <endian> <pitch> args.\n", format);
                    return RC::ILWALID_FILE_FORMAT;
                }
            }
            else
            {
                // Assume pitch is equal to width*bpp
                pitch = width*bpp;
            }

            UINT32 blockRows = 0; // 0 means default block_rows
            if (0 == strcmp(m_TestHdrTokens.front(), "BLOCK_ROWS"))
            {
                m_TestHdrTokens.pop_front();
                if (OK != ParseUINT32(&blockRows))
                {
                    ErrPrintf("%s requires <w> <h> <bpp> <endian> [pitch] [BLOCK_ROWS x]. x = BLOCK_HEIGHT x GOB_HEIGHT. \n",
                              format);
                    return RC::ILWALID_FILE_FORMAT;
                }

                UINT32 gobHeight = m_Test->GetBoundGpuDevice()->GobHeight();
                UINT32 blockHeight = blockRows / gobHeight;
                if ((blockRows % gobHeight != 0) ||
                    ((blockHeight & (blockHeight - 1)) != 0))
                {
                    ErrPrintf("[BLOCK_ROWS x]: x should be power2 of GobHeight(%d).\n",
                              m_Test->GetBoundGpuDevice()->GobHeight());
                    return RC::ILWALID_FILE_FORMAT;
                }
            }

            CHECK_RC(newMod->SetVP2TilingModeFromHeader(
                    format, width, height, bpp, endian.c_str(), pitch, blockRows));
        }
        else if (0 == strcmp(tok, "SIZE"))
        {
            vector<UINT32> geom;
            if (OK != ParseGeometry(&geom))
            {
                ErrPrintf("SIZE needs W[xH[xD]] arg.\n");
                return RC::ILWALID_FILE_FORMAT;
            }
            CHECK_RC(newMod->SetGeometry(geom));
        }
        else if (0 == strcmp(tok, "TYPE_OVERRIDE"))
        {
            UINT32 type;
            if (OK != ParseUINT32(&type))
            {
                ErrPrintf("TYPE_OVERRIDE needs <int> arg, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
            newMod->SetTypeOverride(type);
        }
        else if (0 == strcmp(tok, "ATTR_OVERRIDE"))
        {
            UINT32 attr;
            if (OK != ParseUINT32(&attr))
            {
                ErrPrintf("ATTR_OVERRIDE needs <int> arg, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
            newMod->SetAttrOverride(attr);
        }
        else if (0 == strcmp(tok, "ATTR2_OVERRIDE"))
        {
            UINT32 attr2;
            if (OK != ParseUINT32(&attr2))
            {
                ErrPrintf("ATTR2_OVERRIDE needs <int> arg, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
            newMod->SetAttr2Override(attr2);
        }
        else if (0 == strcmp(tok, "RANGE_CHECK") || (0 == strcmp(tok, "ADDRESS_RANGE")))
        {
            UINT64 low, high;
            if (OK != ParseUINT64(&low) || OK != ParseUINT64(&high))
            {
                ErrPrintf("RANGE_CHECK needs <low> <high>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
            newMod->SetAddressRange(low, high);
        }
        else if (0 == strcmp(tok, "SIZE_PER_WARP"))
        {
            if (OK != ParseUINT32(&sizePerWarp))
            {
                ErrPrintf("SIZE_PER_WARP expects a value; got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
            hasSizePerWarp = true;
        }
        else if (0 == strcmp(tok, "SIZE_PER_SM"))
        {
            if (OK != ParseUINT32(&sizePerTpc))
            {
                ErrPrintf("SIZE_PER_SM expects a value; got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
            hasSizePerTpc = true;
        }
        else if (0 == strcmp(tok, "MORE_TEXTURE_INDEX"))
        {
            UINT32 index;
            if (OK != ParseUINT32(&index))
            {
                ErrPrintf("MORE_TEXTURE_INDEX expects a value; got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
            newMod->AddTextureIndex(index);
        }
        else if (0 == strcmp(tok, "RAW"))
        {
            newMod->SetBufferRaw();
        }
        else if (0 == strcmp(tok, "MAP_TO_BACKINGSTORE"))
        {
            if (params->ParamPresent("-disable_map_to_backingstore") == 0)
                newMod->SetMapToBackingStore();
        }
        else if (0 == strcmp(tok, "GPU_CACHE"))
        {
            string cache;
            if (OK != ParseString(&cache))
            {
                ErrPrintf("GPU_CACHE expects ON or OFF; got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
            else if (cache == "ON")
            {
                newMod->SetGpuCacheMode(Surface2D::GpuCacheOn);
            }
            else if (cache == "OFF")
            {
                newMod->SetGpuCacheMode(Surface2D::GpuCacheOff);
            }
            else
            {
                ErrPrintf("GPU_CACHE expects ON or OFF; got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "P2P_GPU_CACHE"))
        {
            string cache;
            if (OK != ParseString(&cache))
            {
                ErrPrintf("P2P_GPU_CACHE expects ON or OFF; got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
            else if (cache == "ON")
            {
                newMod->SetP2PGpuCacheMode(Surface2D::GpuCacheOn);
            }
            else if (cache == "OFF")
            {
                newMod->SetP2PGpuCacheMode(Surface2D::GpuCacheOff);
            }
            else
            {
                ErrPrintf("P2P_GPU_CACHE expects ON or OFF; got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "LOOPBACK"))
        {
            if (params->ParamPresent("-sli_p2ploopback") > 0)
            {
                ErrPrintf("LOOPBACK not compatible with \"-sli_p2ploopback\" command line option\n");
                return RC::ILWALID_FILE_FORMAT;
            }
            if (m_Test->GetBoundGpuDevice()->GetNumSubdevices() > 1)
            {
                WarnPrintf("LOOPBACK not supported on SLI devices, ignoring\n");
            }
            else
            {
                newMod->SetLoopback(true);
            }
        }
        else if (0 == strcmp(tok, "VPR"))
        {
            string vpr;
            if (OK != ParseString(&vpr))
            {
                ErrPrintf("VPR expects ON or OFF; got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
            else if (vpr == "ON")
            {
                newMod->SetVpr(true);
            }
            else if (vpr == "OFF")
            {
                newMod->SetVpr(false);
            }
            else
            {
                ErrPrintf("VPR expects ON or OFF; got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "SUBCONTEXT"))
        {
            if (OK != ParseString(&subctxName))
            {
                ErrPrintf("SUBCONTEXT expects <subcontext Name> in FILE.\n");
                return RC::ILWALID_FILE_FORMAT;
            }

        }
        else if (0 == strcmp(tok, "ADDRESS_SPACE"))
        {
            if(OK != ParseString(&vasName))
            {
                ErrPrintf("ADDRESS_SPACE need a name.\n");
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else
        {
            ErrPrintf("Unsupported FILE option \"%s\".\n", tok);
            return RC::ILWALID_FILE_FORMAT;
        }
    } // end while tok != ParseFile::s_NewLine

    shared_ptr<SubCtx> pSubCtx;
    pSubCtx = GetSubCtx(subctxName);
    LwRm::Handle hVASpace;
    const string &name = vasName.empty() ? "default" : vasName;
    if (pSubCtx.get())
    {
        CHECK_RC(SanityCheckTSG(pSubCtx, &tsgName));

        if (tsgName.empty())
        {
            ErrPrintf("FILE expects <tsg name>.\n");
            return RC::ILWALID_FILE_FORMAT;
        }
        if (!pSubCtx->GetVaSpace() || !vasName.empty())
        {
            CHECK_RC(pSubCtx->SetVaSpace(GetVAS(name)));
        }

        hVASpace = pSubCtx->GetVaSpace()->GetHandle();
    }
    else
    {
        hVASpace = GetVASHandle(name);
    }

    newMod->SetVASpaceHandle(hVASpace);

    if (isChecked)
    {
        int argnum = params->ParamPresent("-skip_crc_check");
        for (int i=0; i<argnum; ++i)
        {
            const char* buf_name = params->ParamNStr("-skip_crc_check", i, 0);

            if (0 == strcmp(fname.c_str(), buf_name))
            {
                InfoPrintf("'-skip_crc_check %s' specified skipping crc check\n", buf_name);
                isChecked = false;
            }
        }
    }
        
    if (isChecked)
    {
        checkInfo.pTraceCh = GetLastChannelByVASpace(hVASpace);
        newMod->SetCheckInfo(checkInfo);
    }

    if (ftype == FT_SHADER_THREAD_MEMORY &&
        params->ParamPresent("-set_lmem_size") > 0)
    {
        // Bug 490377, override lmem size
        UINT32 size = params->ParamUnsigned("-set_lmem_size", 0);
        CHECK_RC(newMod->SetSizeByArg(size));
    }
    else if (hasSizePerWarp || hasSizePerTpc)
    {
        if (params->ParamPresent("-scale_lmem_size") &&
            params->ParamUnsigned("-scale_lmem_size") == SCALE_LMEM_SIZE_NONE)
        {
            if (newMod->GetSize() > 0)
            {
                InfoPrintf("FILE \"%s\" use trace file size instead of scaled size because of -scale_lmem_size_none\n",
                           fname.c_str());
            }
            else
            {
                newMod->SetSizeByArg(sizePerWarp);
            }
            InfoPrintf("local memory configure bundle size is 0x%x, actually MODS/RM allocated size is 0x%x, "
                       "because of -scale_lmem_size_none\n",
                       hasSizePerWarp? sizePerWarp:sizePerTpc, newMod->GetSize());
        }
        else
        {
            // bug 335369, callwlate chunk size per tpc or per warp
            if (params->ParamPresent("-scale_lmem_size") &&
                params->ParamUnsigned("-scale_lmem_size") == SCALE_LMEM_SIZE_PER_TPC)
            {
                if (hasSizePerTpc)
                    CHECK_RC(newMod->SetSizePerTPC(sizePerTpc, pSubCtx));
                else
                {
                    ErrPrintf("Scale LMEM by TPC number, but SIZE_PER_SM is not in FILE command\n");
                    return RC::ILWALID_FILE_FORMAT;
                }
            }
            else
            {
                // by default, callwlate chunk size per warp
                if (hasSizePerWarp)
                    CHECK_RC(newMod->SetSizePerWarp(sizePerWarp, pSubCtx));
                else
                {
                    ErrPrintf("Scale LMEM by WARP number, but SIZE_PER_WARP is not in FILE command\n");
                    return RC::ILWALID_FILE_FORMAT;
                }
            }
        }
    }
    else if (ftype == FT_SHADER_THREAD_MEMORY)
    {
        if ((newMod->GetSize() > 0) &&
            params->ParamPresent("-scale_lmem_size") &&
            params->ParamUnsigned("-scale_lmem_size") == SCALE_LMEM_SIZE_NONE)
        {
            InfoPrintf("FILE \"%s\" use trace file size 0x%x instead of scaled size"
                       " because of -scale_lmem_size_none\n", fname.c_str(), newMod->GetSize());
        }
        else
        {
            // Scale up the size by the number of TPCs
            CHECK_RC(newMod->SetSizePerTPC(newMod->GetSize(), pSubCtx));
        }
    }

    if (hasVp2Bigendian)
    {
        InfoPrintf("FILE \"%s\" will will be byteswapped.\n", fname.c_str());

        if (hasSwapFile)
        {
            InfoPrintf("Using BE file \"%s\" in place of LE file \"%s\".\n",
                       swapFileName.c_str(), fname.c_str());
            newMod->SetFilename(swapFileName);

            if (hasSwapSize)
            {
                WarnPrintf("FILE \"%s\" uses both SWAP_FILE and SWAP_SIZE, both will be applied!\n",
                           fname.c_str());
            }
        }
        if (!(hasSwapSize || hasSwapFile))
        {
            ErrPrintf("FILE \"%s\" must specify one of SWAP_FILE or SWAP_SIZE!\n",
                      fname.c_str());
            return RC::ILWALID_FILE_FORMAT;
        }
        if (isChecked && !hasSwapSize)
        {
            // We have to be able to un-byte-swap on read for CRC checking!
            ErrPrintf("FILE \"%s\" must specify SWAP_SIZE with CRC_CHECK or CRC_CHECK_RANGE!\n",
                      fname.c_str());
            return RC::ILWALID_FILE_FORMAT;
        }
    }

    if ((m_Version < 3) && (ftype == FT_PUSHBUFFER))
    {
        // Trace version 2 and earlier have implied commands:
        //   CHANNEL default
        //   METHODS default test.dma
        // Since we've found a pushbuffer file, add the implied METHODS.

        TraceChannel * pTraceChannel = GetLwrrentChannel(CPU_MANAGED);

        // The default channel is supposed to be set up in ParseCLASS.
        if (0 == pTraceChannel)
        {
            ErrPrintf("Pushbuffer FILE seen before CLASS.\n");
            return RC::ILWALID_FILE_FORMAT;
        }

        CHECK_RC(newMod->SetTraceChannel(pTraceChannel));
    }

    if ( ftype == FT_ZLWLL_RAM )
    {
        TraceChannel * pTraceChannel = GetLastChannelByVASpace(newMod->GetVASpaceHandle());

        if (0 == pTraceChannel)
        {
            ErrPrintf("FILE ZlwllRamBuffer seen before CLASS.\n");
            return RC::ILWALID_FILE_FORMAT;
        }

        CHECK_RC(newMod->SetTraceChannel(pTraceChannel, false));
    }

    if (Source == GpuTrace::FROM_INLINE_FILE)
    {
        CHECK_RC( ParseInlineFileData(newMod) );
    }
    else if (Source == GpuTrace::FROM_CONSTANT)
    {
        CachedSurface* pCachedSurf = newMod->GetCachedSurface();
        if (pCachedSurf)
        {
            pCachedSurf->SetIsConstContBuffer();

            if (!pCachedSurf->IsInLazyLoadStatus())
            {
                // Load constant data 0 to cached surface
                CHECK_RC(pCachedSurf->SetConstantData());
            }
        }
        else
        {
            MASSERT(!"GpuTrace::ParseFILE: GetCachedSurface() == NULL");
        }
    }

    return OK;
} // end GpuTrace::ParseFILE

//---------------------------------------------------------------------------
RC GpuTrace::ParseALLOC_SURFACE()
{
    RC rc = OK;

    string name;
    if (OK != ParseString(&name))
    {
        ErrPrintf("ALLOC_SURFACE: <name> expected\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    // Make sure that the buffer name wasn't already used.
    if (ModFind(name.c_str()) != 0)
    {
        ErrPrintf("More than one trace command with name %s.\n", name.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }

    PropertyUsageMap propertyUsed;
    MemoryCommandProperties properties;

    properties.tagList.push_back(name);

    CHECK_RC(ParseMemoryCommandProperties(&propertyUsed, &properties,
        MC_ALLOC_SURFACE));

    SurfaceType surfaceType = FindSurface(name);

    MdiagSurf surface; 

    CHECK_RC(InitSurfaceFromProperties(name, &surface, surfaceType, properties,
        &propertyUsed));

    m_Test->SetAllocSurfaceCommandPresent(true);

    size_t fileSize;
    CHECK_RC(GetMemoryCommandFileSize(properties, &propertyUsed, &fileSize));

    SurfaceTraceModule * module = nullptr;
    if (properties.isShared)
    {
        module = new SharedTraceModule(m_Test, name,
                properties.file, surface, fileSize, surfaceType);
    }
    else
    {
        module = new SurfaceTraceModule(m_Test, name,
                properties.file, surface, fileSize, surfaceType);
    }

    module->SetTraceOpType(TraceOpType::AllocSurface);
    CHECK_RC(SetMemoryModuleProperties(module, &surface, surfaceType,
        properties, &propertyUsed));

    return rc;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseSURFACE_VIEW()
{
    RC rc = OK;

    string name;
    if (OK != ParseString(&name))
    {
        ErrPrintf("SURFACE_VIEW: <name> expected\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    // Make sure that the buffer name wasn't already used.
    if (ModFind(name.c_str()) != 0)
    {
        ErrPrintf("More than one trace command with name %s.\n", name.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }

    PropertyUsageMap propertyUsed;
    MemoryCommandProperties properties;

    properties.tagList.push_back(name);

    CHECK_RC(ParseMemoryCommandProperties(&propertyUsed, &properties,
        MC_SURFACE_VIEW));

    SurfaceType surfaceType = FindSurface(name);

    MdiagSurf surface;

    CHECK_RC(InitSurfaceFromProperties(name, &surface, surfaceType, properties,
        &propertyUsed));

    m_Test->SetAllocSurfaceCommandPresent(true);

    size_t fileSize;
    CHECK_RC(GetMemoryCommandFileSize(properties, &propertyUsed, &fileSize));

    if (propertyUsed["SKED_REFLECTED"] && properties.skedReflected)
    {
        surface.SetSkedReflected(true);
    }

    if (!propertyUsed["PARENT_SURFACE"])
    {
        ErrPrintf("SURFACE_VIEW must specify a PARENT_SURFACE\n");
        return RC::ILWALID_FILE_FORMAT;
    }
    else if (!propertyUsed["ADDRESS_OFFSET"])
    {
        ErrPrintf("SURFACE_VIEW must specify an ADDRESS_OFFSET\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    surface.SetIsSurfaceView();

    ViewTraceModule *module = new ViewTraceModule(m_Test, name,
        properties.file, surface, fileSize, properties.parentSurface,
        surfaceType);

    module->SetTraceOpType(TraceOpType::SurfaceView);
    CHECK_RC(SetMemoryModuleProperties(module, &surface, surfaceType,
        properties, &propertyUsed));

    if (!m_DmaCheckCeRequested)
    {
        MdiagSurf *parentSurf = module->GetParentModule()->GetParameterSurface();
        m_DmaCheckCeRequested = parentSurf && parentSurf->IsVirtualOnly();
    }

    return rc;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseALLOC_VIRTUAL()
{
    RC rc = OK;

    string name;
    if (OK != ParseString(&name))
    {
        ErrPrintf("ALLOC_VIRTUAL: <name> expected\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    // Make sure that the buffer name wasn't already used.
    if (ModFind(name.c_str()) != 0)
    {
        ErrPrintf("More than one trace command with name %s.\n", name.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }

    PropertyUsageMap propertyUsed;
    MemoryCommandProperties properties;

    properties.tagList.push_back(name);

    CHECK_RC(ParseMemoryCommandProperties(&propertyUsed, &properties,
        MC_ALLOC_VIRTUAL));

    SurfaceType surfaceType = FindSurface(name);

    MdiagSurf surface;

    surface.SetSpecialization(Surface2D::VirtualOnly);

    CHECK_RC(InitSurfaceFromProperties(name, &surface, surfaceType, properties,
        &propertyUsed));

    if ((Platform::GetSimulationMode() == Platform::Amodel) &&
        (surface.GetVirtAlignment() == 0))
    {
        // the physical allocation implicitly sets 4k alignment, so amodel needs it on virtual too
        surface.SetVirtAlignment(0x1000);
    }

    m_Test->SetAllocSurfaceCommandPresent(true);

    size_t fileSize;
    CHECK_RC(GetMemoryCommandFileSize(properties, &propertyUsed, &fileSize));

    SurfaceTraceModule *module = new SurfaceTraceModule(m_Test, name,
        properties.file, surface, fileSize, surfaceType);
    module->SetTraceOpType(TraceOpType::AllocVirtual);

    CHECK_RC(SetMemoryModuleProperties(module, &surface, surfaceType,
        properties, &propertyUsed));

    return rc;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseALLOC_PHYSICAL()
{
    RC rc = OK;

    string name;
    if (OK != ParseString(&name))
    {
        ErrPrintf("ALLOC_PHYSICAL: <name> expected\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    // Make sure that the buffer name wasn't already used.
    if (ModFind(name.c_str()) != 0)
    {
        ErrPrintf("More than one trace command with name %s.\n", name.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }

    PropertyUsageMap propertyUsed;
    MemoryCommandProperties properties;

    properties.tagList.push_back(name);

    CHECK_RC(ParseMemoryCommandProperties(&propertyUsed, &properties,
        MC_ALLOC_PHYSICAL));

    SurfaceType surfaceType = FindSurface(name);

    MdiagSurf surface;

    surface.SetSpecialization(Surface2D::PhysicalOnly);

    CHECK_RC(InitSurfaceFromProperties(name, &surface, surfaceType, properties,
        &propertyUsed));

    m_Test->SetAllocSurfaceCommandPresent(true);

    size_t fileSize;
    CHECK_RC(GetMemoryCommandFileSize(properties, &propertyUsed, &fileSize));

    SurfaceTraceModule * module = nullptr;
    if (properties.isShared)
    {
        module = new SharedTraceModule(m_Test, name,
                properties.file, surface, fileSize, surfaceType);
    }
    else
    {
        module = new SurfaceTraceModule(m_Test, name,
                properties.file, surface, fileSize, surfaceType);
    }
    module->SetTraceOpType(TraceOpType::AllocPhysical);

    CHECK_RC(SetMemoryModuleProperties(module, &surface, surfaceType,
        properties, &propertyUsed));

    return rc;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseMAP()
{
    RC rc = OK;

    string name;
    if (OK != ParseString(&name))
    {
        ErrPrintf("MAP: <name> expected\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    // Make sure that the buffer name wasn't already used.
    if (ModFind(name.c_str()) != 0)
    {
        ErrPrintf("More than one trace command with name %s.\n", name.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }

    PropertyUsageMap propertyUsed;
    MemoryCommandProperties properties;

    properties.tagList.push_back(name);

    CHECK_RC(ParseMemoryCommandProperties(&propertyUsed, &properties,
        MC_MAP));

    if (!propertyUsed["VIRTUAL_ALLOC"])
    {
        ErrPrintf("MAP requires VIRTUAL_ALLOC property.\n");
        return RC::ILWALID_FILE_FORMAT;
    }
    else if (!propertyUsed["PHYSICAL_ALLOC"])
    {
        ErrPrintf("MAP requires PHYSICAL_ALLOC property.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    UINT32 offsetProperties = 0;
    offsetProperties += (propertyUsed["VIRTUAL_OFFSET"]) ? 1 : 0;
    offsetProperties += (propertyUsed["TEXTURE_SUBIMAGE"]) ? 1 : 0;
    offsetProperties += (propertyUsed["TEXTURE_MIP_TAIL"]) ? 1 : 0;

    if (offsetProperties > 1)
    {
        ErrPrintf("Only one of the following can be specified for a MAP "
                  "command:  VIRTUAL_OFFSET, TEXTURE_SUBIMAGE, or TEXTURE_MIP_TAIL.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    TraceModule *virtModule = ModFind(properties.virtualAllocName.c_str());

    if (virtModule == 0)
    {
        ErrPrintf("No ALLOC_VIRTUAL command with name \'%s\' exists.\n",
                  properties.virtualAllocName.c_str());

        return RC::ILWALID_FILE_FORMAT;
    }

    MdiagSurf *virtualSurface = virtModule->GetParameterSurface();

    SurfaceType surfaceType = FindSurface(name);
    MdiagSurf mapSurface;

    mapSurface.SetSpecialization(Surface2D::MapOnly);

    CHECK_RC(InitSurfaceFromProperties(name, &mapSurface, SURFACE_TYPE_UNKNOWN,
        properties, &propertyUsed));

    // Color format can't be set for a MAP so it would normally default to
    // A8R8G8B8.  However, we need the bytes-per-pixel to be 1, otherwise
    // reading the surface won't work correctly.
    mapSurface.SetColorFormat(ColorUtils::Y8);

    // Grab various properties from the virtual surface.
    mapSurface.SetLayout(virtualSurface->GetLayout());
    mapSurface.SetLogBlockWidth(virtualSurface->GetLogBlockWidth());
    mapSurface.SetLogBlockHeight(virtualSurface->GetLogBlockHeight());
    mapSurface.SetLogBlockDepth(virtualSurface->GetLogBlockDepth());
    mapSurface.SetPageSize(virtualSurface->GetPageSize());

    if (propertyUsed["TEXTURE_SUBIMAGE"])
    {
        if (mapSurface.GetLayout() != Surface2D::BlockLinear)
        {
            ErrPrintf("a MAP with TEXTURE_SUBIMAGE must reference an ALLOC_VIRTUAL command with LAYOUT BLOCKLINEAR.\n");
            return RC::ILWALID_FILE_FORMAT;
        }

        // Set properties needed to callwlate the correct size of the subimage.
        mapSurface.SetIsSparse(true);
        mapSurface.SetType(LWOS32_TYPE_TEXTURE);
        mapSurface.SetWidth(properties.textureSubimageWidth);
        mapSurface.SetHeight(properties.textureSubimageHeight);
        mapSurface.SetDepth(properties.textureSubimageDepth);
        mapSurface.SetColorFormat(virtualSurface->GetColorFormat());
        mapSurface.SetTileWidthInGobs(virtualSurface->GetTileWidthInGobs());
    }
    else if (propertyUsed["TEXTURE_MIP_TAIL"])
    {
        if (mapSurface.GetLayout() != Surface2D::BlockLinear)
        {
            ErrPrintf("a MAP with TEXTURE_MIP_TAIL must reference an ALLOC_VIRTUAL command with LAYOUT BLOCKLINEAR.\n");
            return RC::ILWALID_FILE_FORMAT;
        }

        UINT32 levelWidth = max(virtualSurface->GetWidth() >>
            properties.textureMipTailStartingLevel, UINT32(1));
        UINT32 levelHeight = max(virtualSurface->GetHeight() >>
            properties.textureMipTailStartingLevel, UINT32(1));
        UINT32 levelDepth = max(virtualSurface->GetDepth() >>
            properties.textureMipTailStartingLevel, UINT32(1));

        // Set properties needed to callwlate the correct size of the mip tail.
        mapSurface.SetIsSparse(true);
        mapSurface.SetType(LWOS32_TYPE_TEXTURE);
        mapSurface.SetWidth(levelWidth);
        mapSurface.SetHeight(levelHeight);
        mapSurface.SetDepth(levelDepth);
        mapSurface.SetMipLevels(virtualSurface->GetMipLevels() -
            properties.textureMipTailStartingLevel);
        mapSurface.SetColorFormat(virtualSurface->GetColorFormat());
        mapSurface.SetTileWidthInGobs(virtualSurface->GetTileWidthInGobs());
    }

    size_t fileSize;
    CHECK_RC(GetMemoryCommandFileSize(properties, &propertyUsed, &fileSize));

    TraceModule *physModule;

    // If the user specified CREATE_PHYSICAL_ALLOC, then the no physical
    // allocation will already exist so one must be created.  Many of the
    // properties of this physical allocation must be copied from the virtual
    // allocation, but some of the properties need to come from the map
    // surface (generally ones related to size callwlation).
    if (propertyUsed["CREATE_PHYSICAL_ALLOC"])
    {
        MdiagSurf physSurface;
        physSurface.SetCreatedFromAllocSurface();
        physSurface.SetLocation(Memory::Fb);
        physSurface.SetSpecialization(Surface2D::PhysicalOnly);
        physSurface.SetColorFormat(virtualSurface->GetColorFormat());
        physSurface.SetAAMode(virtualSurface->GetAAMode());
        physSurface.SetLayout(virtualSurface->GetLayout());
        physSurface.SetCompressed(virtualSurface->GetCompressed());
        physSurface.SetCompressedFlag(virtualSurface->GetCompressedFlag());
        physSurface.SetType(virtualSurface->GetType());
        physSurface.SetZLwllFlag(virtualSurface->GetZLwllFlag());
        physSurface.SetProtect(mapSurface.GetProtect());
        physSurface.SetShaderProtect(mapSurface.GetShaderProtect());
        physSurface.SetPriv(virtualSurface->GetPriv());
        physSurface.SetGpuCacheMode(virtualSurface->GetGpuCacheMode());
        physSurface.SetP2PGpuCacheMode(virtualSurface->GetP2PGpuCacheMode());
        physSurface.SetZbcMode(virtualSurface->GetZbcMode());
        physSurface.SetVideoProtected(virtualSurface->GetVideoProtected());
        physSurface.SetWidth(mapSurface.GetWidth());
        physSurface.SetHeight(mapSurface.GetHeight());
        physSurface.SetDepth(mapSurface.GetDepth());
        physSurface.SetPitch(mapSurface.GetPitch());
        physSurface.SetLogBlockWidth(virtualSurface->GetLogBlockWidth());
        physSurface.SetLogBlockHeight(virtualSurface->GetLogBlockHeight());
        physSurface.SetLogBlockDepth(virtualSurface->GetLogBlockDepth());
        physSurface.SetMipLevels(mapSurface.GetMipLevels());
        physSurface.SetBorder(mapSurface.GetBorder());
        physSurface.SetPageSize(virtualSurface->GetPageSize());
        physSurface.SetTileWidthInGobs(mapSurface.GetTileWidthInGobs());
        physSurface.SetIsSparse(mapSurface.GetIsSparse());

        CHECK_RC(ProcessGlobalArguments(&physSurface, SURFACE_TYPE_UNKNOWN));

        CHECK_RC(GetAllocSurfaceParameters(&physSurface,
            properties.physicalAllocName));

        physModule = new SurfaceTraceModule(m_Test,
            properties.physicalAllocName, "", physSurface, fileSize,
            SURFACE_TYPE_UNKNOWN);

        m_Modules.push_back(physModule);
        m_GenericModules.push_back(physModule);
    }
    else
    {
        physModule = ModFind(properties.physicalAllocName.c_str());
        if (physModule == 0)
        {
            ErrPrintf("No ALLOC_PHYSICAL command with name \'%s\' exists.\n",
                      properties.physicalAllocName.c_str());

            return RC::ILWALID_FILE_FORMAT;
        }

        mapSurface.SetLocation(
            physModule->GetParameterSurface()->GetLocation());
    }

    m_Test->SetAllocSurfaceCommandPresent(true);

    MapTraceModule* module;

    if (propertyUsed["TEXTURE_SUBIMAGE"])
    {
        module = new MapTextureSubimageTraceModule(
            m_Test,
            name,
            properties.file,
            mapSurface,
            fileSize,
            surfaceType,
            properties.virtualAllocName,
            properties.physicalAllocName,
            properties.virtualOffset,
            properties.physicalOffset,
            properties.textureSubimageXOffset,
            properties.textureSubimageYOffset,
            properties.textureSubimageZOffset,
            properties.textureSubimageMipLevel,
            properties.textureSubimageDimension,
            properties.textureSubimageArrayIndex);
    }
    else if (propertyUsed["TEXTURE_MIP_TAIL"])
    {
        module = new MapTextureMipTailTraceModule(
            m_Test,
            name,
            properties.file,
            mapSurface,
            fileSize,
            surfaceType,
            properties.virtualAllocName,
            properties.physicalAllocName,
            properties.virtualOffset,
            properties.physicalOffset,
            properties.textureMipTailStartingLevel,
            properties.textureMipTailDimension,
            properties.textureMipTailArrayIndex);
    }
    else
    {
        module = new MapTraceModule(
            m_Test,
            name,
            properties.file,
            mapSurface,
            fileSize,
            surfaceType,
            properties.virtualAllocName,
            properties.physicalAllocName,
            properties.virtualOffset,
            properties.physicalOffset);
    }

    module->SetTraceOpType(TraceOpType::Map);
    module->SetVASpaceHandle(virtualSurface->GetGpuVASpace());
    CHECK_RC(module->SetTraceChannel(
        virtModule->GetTraceChannel(), surfaceType == SURFACE_TYPE_PUSHBUFFER));
    CHECK_RC(SetMemoryModuleProperties(module, &mapSurface, surfaceType,
        properties, &propertyUsed));

    return rc;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseMemoryCommandProperties(PropertyUsageMap *propertyUsed,
    MemoryCommandProperties *properties, UINT32 commandFlag)
{
    RC rc = OK;
    string name;

    if (m_MemoryCommandPropertyMap.size() == 0)
    {
        InitMemoryCommandPropertyMap();
    }

    MemoryCommandPropertyMap::iterator mapIter;

    // Initialize the propertyUsed map with all properties set
    // to false.  When properties are parsed below, the corresponding
    // propertyUsed map entry will be set to true.
    for (mapIter = m_MemoryCommandPropertyMap.begin();
         mapIter != m_MemoryCommandPropertyMap.end();
         ++mapIter)
    {
        (*propertyUsed)[(*mapIter).first] = false;
    }

    while (ParseFile::s_NewLine != m_TestHdrTokens.front())
    {
        const char * tok = m_TestHdrTokens.front();
        m_TestHdrTokens.pop_front();

        MemoryCommandPropertyMap::const_iterator mapIter =
            m_MemoryCommandPropertyMap.find(tok);

        // Check the memory command property map to make sure
        // the property is valid for the given command.
        if ((mapIter == m_MemoryCommandPropertyMap.end()) ||
            (((*mapIter).second & commandFlag) == 0))
        {
            ErrPrintf("Invalid property '%s'\n", tok);
            return RC::ILWALID_FILE_FORMAT;
        }

        // Unlike the other properties, the TAG option can be specified
        // multiple times, so a list of the names must be kept.
        // (Technically the other properties can be specified multiple times,
        // but only the last one is used.)
        if (0 == strcmp(tok, "TAG"))
        {
            if (OK != ParseString(&name))
            {
                ErrPrintf("Error parsing '%s'\n", tok);
                return RC::ILWALID_FILE_FORMAT;
            }

            properties->tagList.push_back(name);
        }
        else if (0 == strcmp(tok, "FILE"))
        {
            properties->hdrStrProps["FILE"] = m_TestHdrTokens.front();
            if (OK != ParseString(&properties->file))
            {
                ErrPrintf("FILE requires <filename>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "FORMAT"))
        {
            properties->hdrStrProps["FORMAT"] = m_TestHdrTokens.front();
            if (OK != ParseString(&properties->format))
            {
                ErrPrintf("FORMAT requires <string>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "SAMPLE_MODE"))
        {
            properties->hdrStrProps["SAMPLE_MODE"] = m_TestHdrTokens.front();
            if (OK != ParseString(&properties->sampleMode))
            {
                ErrPrintf("SAMPLE_MODE requires <string>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "LAYOUT"))
        {
            properties->hdrStrProps["LAYOUT"] = m_TestHdrTokens.front();
            if (OK != ParseString(&properties->layout))
            {
                ErrPrintf("LAYOUT requires <string>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "COMPRESSION"))
        {
            if (OK != ParseString(&properties->compression))
            {
                ErrPrintf("COMPRESSION requires <string>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "ZLWLL"))
        {
            if (OK != ParseString(&properties->zlwll))
            {
                ErrPrintf("ZLWLL requires <string>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "APERTURE"))
        {
            if (OK != ParseString(&properties->aperture))
            {
                ErrPrintf("APERTURE requires <string>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "TYPE"))
        {
            if (OK != ParseString(&properties->type))
            {
                ErrPrintf("TYPE requires <string>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "ACCESS"))
        {
            if (OK != ParseString(&properties->access))
            {
                ErrPrintf("ACCESS requires <string>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "SHADER_ACCESS"))
        {
            if (OK != ParseString(&properties->shaderAccess))
            {
                ErrPrintf("SHADER_ACCESS requires <string>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "PRIVILEGED"))
        {
            if (OK != ParseString(&properties->privileged))
            {
                ErrPrintf("PRIVILEGED requires <string>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "CRC_CHECK"))
        {
            properties->hdrStrProps["CRC_CHECK"] = m_TestHdrTokens.front();
            if (OK != ParseString(&properties->crcCheck))
            {
                ErrPrintf("CRC_CHECK requires <name>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
            ++properties->crcCheckCount;
        }
        else if (0 == strcmp(tok, "CRC_CHECK_NO_MATCH"))
        {
            if (OK != ParseString(&properties->crcCheck))
            {
                ErrPrintf("CRC_CHECK_NO_MATCH requires <name>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
            ++properties->crcCheckCount;
            properties->ilwertCrcCheck = true;
        }
        else if (0 == strcmp(tok, "REFERENCE_CHECK"))
        {
            properties->hdrStrProps["REFERENCE_CHECK"] = m_TestHdrTokens.front();
            if (OK != ParseString(&properties->referenceCheck))
            {
                ErrPrintf("REFERENCE_CHECK requires <filename>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
            ++properties->crcCheckCount;
        }
        else if (0 == strcmp(tok, "REFERENCE_CHECK_NO_MATCH"))
        {
            if (OK != ParseString(&properties->referenceCheck))
            {
                ErrPrintf("REFERENCE_CHECK_NO_MATCH requires <filename>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
            ++properties->crcCheckCount;
            properties->ilwertCrcCheck = true;
        }
        else if (0 == strcmp(tok, "GPU_CACHE"))
        {
            if (OK != ParseString(&properties->gpuCache))
            {
                ErrPrintf("GPU_CACHE requires <string>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "P2P_GPU_CACHE"))
        {
            if (OK != ParseString(&properties->p2pGpuCache))
            {
                ErrPrintf("P2P_GPU_CACHE requires <string>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "ZBC"))
        {
            if (OK != ParseString(&properties->zbc))
            {
                ErrPrintf("ZBC requires <string>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "CRC_RECT"))
        {
            if (OK != ParseString(&properties->crcRect))
            {
                ErrPrintf("CRC_RECT requires <dimension string>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "PARENT_SURFACE"))
        {
            properties->hdrStrProps["PARENT_SURFACE"] = m_TestHdrTokens.front();
            if (OK != ParseString(&properties->parentSurface))
            {
                ErrPrintf("PARENT_SURFACE requires <surface name>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }

            const char* surface_name = (properties->parentSurface).c_str();
            if (ModFind(surface_name) == 0)
            {
                ErrPrintf("PARENT_SURFACE requires an existing surface name, got invalid name \"%s\".\n", surface_name);
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "CHANNEL"))
        {
            if (OK != ParseString(&properties->traceChannel))
            {
                ErrPrintf("CHANNEL requires <channel name>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "WIDTH"))
        {
            properties->hdrStrProps["WIDTH"] = m_TestHdrTokens.front();
            if (OK != ParseUINT32(&properties->width))
            {
                ErrPrintf("WIDTH requires <unsigned integer>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "HEIGHT"))
        {
            properties->hdrStrProps["HEIGHT"] = m_TestHdrTokens.front();
            if (OK != ParseUINT32(&properties->height))
            {
                ErrPrintf("HEIGHT requires <unsigned integer>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "DEPTH"))
        {
            properties->hdrStrProps["DEPTH"] = m_TestHdrTokens.front();
            if (OK != ParseUINT32(&properties->depth))
            {
                ErrPrintf("DEPTH requires <unsigned integer>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "PITCH"))
        {
            properties->hdrStrProps["PITCH"] = m_TestHdrTokens.front();
            if (OK != ParseUINT32(&properties->pitch))
            {
                ErrPrintf("PITCH requires <unsigned integer>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "BLOCK_WIDTH"))
        {
            properties->hdrStrProps["BLOCK_WIDTH"] = m_TestHdrTokens.front();
            if (OK != ParseUINT32(&properties->blockWidth))
            {
                ErrPrintf("BLOCK_WIDTH requires <unsigned integer>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "BLOCK_HEIGHT"))
        {
            properties->hdrStrProps["BLOCK_HEIGHT"] = m_TestHdrTokens.front();
            if (OK != ParseUINT32(&properties->blockHeight))
            {
                ErrPrintf("BLOCK_HEIGHT requires <unsigned integer>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "BLOCK_DEPTH"))
        {
            properties->hdrStrProps["BLOCK_DEPTH"] = m_TestHdrTokens.front();
            if (OK != ParseUINT32(&properties->blockDepth))
            {
                ErrPrintf("BLOCK_DEPTH requires <unsigned integer>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "ARRAY_SIZE"))
        {
            properties->hdrStrProps["ARRAY_SIZE"] = m_TestHdrTokens.front();
            if (OK != ParseUINT32(&properties->arraySize))
            {
                ErrPrintf("ARRAY_SIZE requires <unsigned integer>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "SWAP_SIZE"))
        {
            if (OK != ParseUINT32(&properties->swapSize))
            {
                ErrPrintf("SWAP_SIZE requires <unsigned integer>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "PHYS_ALIGNMENT"))
        {
            properties->hdrStrProps["PHYS_ALIGNMENT"] = m_TestHdrTokens.front();
            if (OK != ParseUINT64(&properties->physAlignment))
            {
                ErrPrintf("PHYS_ALIGNMENT requires <unsigned integer>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "VIRT_ALIGNMENT"))
        {
            properties->hdrStrProps["VIRT_ALIGNMENT"] = m_TestHdrTokens.front();
            if (OK != ParseUINT64(&properties->virtAlignment))
            {
                ErrPrintf("VIRT_ALIGNMENT requires <unsigned integer>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "SIZE"))
        {
            properties->hdrStrProps["SIZE"] = m_TestHdrTokens.front();
            if (OK != ParseUINT64(&properties->size))
            {
                ErrPrintf("SIZE requires <unsigned integer>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "ATTR_OVERRIDE"))
        {
            properties->hdrStrProps["ATTR_OVERRIDE"] = m_TestHdrTokens.front();
            if (OK != ParseUINT32(&properties->attrOverride))
            {
                ErrPrintf("ATTR_OVERRIDE requires <unsigned integer>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "ATTR2_OVERRIDE"))
        {
            properties->hdrStrProps["ATTR2_OVERRIDE"] = m_TestHdrTokens.front();
            if (OK != ParseUINT32(&properties->attr2Override))
            {
                ErrPrintf("ATTR2_OVERRIDE requires <unsigned integer>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "TYPE_OVERRIDE"))
        {
            if (OK != ParseUINT32(&properties->typeOverride))
            {
                ErrPrintf("TYPE_OVERRIDE requires <unsigned integer>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "LOOPBACK"))
        {
            properties->loopback = true;
        }
        else if (0 == strcmp(tok, "RAW"))
        {
            properties->raw = true;
        }
        else if (0 == strcmp(tok, "RAW_CRC"))
        {
            properties->rawCrc = true;
        }
        else if (0 == strcmp(tok, "SKED_REFLECTED"))
        {
            properties->skedReflected = true;
        }
        else if (0 == strcmp(tok, "CONTIG"))
        {
            properties->contig = true;
        }
        else if (0 == strcmp(tok, "VIRT_ADDRESS"))
        {
            properties->hdrStrProps["VIRT_ADDRESS"] = m_TestHdrTokens.front();
            rc = ParseUINT64(&properties->virtAddress);
            if ((rc != OK) || (properties->virtAddress == 0))
            {
                const char *ilwalidVA = (rc != OK) ? m_TestHdrTokens.front() : "0";
                ErrPrintf("VIRT_ADDRESS requires a valid address, got \"%s\".\n", ilwalidVA);
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "PHYS_ADDRESS"))
        {
            properties->hdrStrProps["PHYS_ADDRESS"] = m_TestHdrTokens.front();
            if (OK != ParseUINT64(&properties->physAddress))
            {
                ErrPrintf("PHYS_ADDRESS requires an address, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "VIRT_ADDRESS_RANGE"))
        {
            MASSERT(m_TestHdrTokens.size() > 1);
            auto it = m_TestHdrTokens.begin();
            string prop = *it++;
            prop += " ";
            prop += *it;
            properties->hdrStrProps["VIRT_ADDRESS_RANGE"] = prop;
            if ((OK != ParseUINT64(&properties->virtAddressMin)) ||
                (OK != ParseUINT64(&properties->virtAddressMax)))
            {
                ErrPrintf("VIRT_ADDRESS_RANGE needs <low> <high>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
            else if (properties->virtAddressMin > properties->virtAddressMax)
            {
                ErrPrintf("Invalid VIRT_ADDRESS_RANGE: 0x%llx - 0x%llx.\n",
                    properties->virtAddressMin, properties->virtAddressMax);
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "PHYS_ADDRESS_RANGE"))
        {
            MASSERT(m_TestHdrTokens.size() > 1);
            auto it = m_TestHdrTokens.begin();
            string prop = *it++;
            prop += " ";
            prop += *it;
            properties->hdrStrProps["PHYS_ADDRESS_RANGE"] = prop;
            if ((OK != ParseUINT64(&properties->physAddressMin)) ||
                (OK != ParseUINT64(&properties->physAddressMax)))
            {
                ErrPrintf("PHYS_ADDRESS_RANGE needs <low> <high>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
            else if (properties->physAddressMin > properties->physAddressMax)
            {
                ErrPrintf("Invalid PHYS_ADDRESS_RANGE: 0x%llx - 0x%llx.\n",
                          properties->physAddressMin, properties->physAddressMax);
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "ADDRESS_OFFSET"))
        {
            properties->hdrStrProps["ADDRESS_OFFSET"] = m_TestHdrTokens.front();
            if (OK != ParseUINT64(&properties->addressOffset))
            {
                ErrPrintf("ADDRESS_OFFSET requires <offset>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "HOST_REFLECTED"))
        {
            if (OK != ParseString(&properties->hostReflectedChName))
            {
                ErrPrintf("HOST_REFLECTED requires GPU managed channel name.\n");
                return RC::ILWALID_FILE_FORMAT;
            }
            properties->hostReflected = true;
        }
        else if (0 == strcmp(tok, "VIRTUAL_ALLOC"))
        {
            properties->hdrStrProps["VIRTUAL_ALLOC"] = m_TestHdrTokens.front();
            if (OK != ParseString(&properties->virtualAllocName))
            {
                ErrPrintf("VIRTUAL_ALLOC requires <name>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "PHYSICAL_ALLOC"))
        {
            properties->hdrStrProps["PHYSICAL_ALLOC"] = m_TestHdrTokens.front();
            if (OK != ParseString(&properties->physicalAllocName))
            {
                ErrPrintf("PHYSICAL_ALLOC requires <name>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "VIRTUAL_OFFSET"))
        {
            properties->hdrStrProps["VIRTUAL_OFFSET"] = m_TestHdrTokens.front();
            if (OK != ParseUINT64(&properties->virtualOffset))
            {
                ErrPrintf("VIRTUAL_OFFSET requires <offset>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "PHYSICAL_OFFSET"))
        {
            properties->hdrStrProps["PHYSICAL_OFFSET"] = m_TestHdrTokens.front();
            if (OK != ParseUINT64(&properties->physicalOffset))
            {
                ErrPrintf("PHYSICAL_OFFSET requires <offset>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "MIPLEVEL_SIZE"))
        {
            if (OK != ParseUINT32(&properties->mipLevels))
            {
                ErrPrintf("MIPLEVEL_SIZE requires <miplevel_size>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "BORDER"))
        {
            properties->hdrStrProps["BORDER"] = m_TestHdrTokens.front();
            if (OK != ParseUINT32(&properties->border))
            {
                ErrPrintf("BORDER requires <border>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "LWBEMAP"))
        {
            properties->lwbemap = true;
        }
        else if (0 == strcmp(tok, "PAGE_SIZE"))
        {
            properties->hdrStrProps["PAGE_SIZE"] = m_TestHdrTokens.front();
            if (OK != ParseString(&properties->pageSize))
            {
                ErrPrintf("PAGE_SIZE requires <SMALL|BIG|HUGE>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "MAP_MODE"))
        {
            if (OK != ParseString(&properties->mapMode))
            {
                ErrPrintf("MAP_MODE requires <DIRECT|REFLECTED>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0== strcmp(tok, "CLASS"))
        {
            properties->hdrStrProps["CLASS"] = m_TestHdrTokens.front();
            properties->classNum = m_Test->ClassStr2Class(m_TestHdrTokens.front());

            if (properties->classNum == 0)
            {
                ErrPrintf("Class name(%s) following CLASS token is invalid.\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }

            m_TestHdrTokens.pop_front();
        }
        else if (0== strcmp(tok, "FILL8"))
        {
            properties->hdrStrProps["FILL8"] = m_TestHdrTokens.front();
            UINT32 value;
            if (OK != ParseUINT32(&value))
            {
                ErrPrintf("Fill value(%s) following FILL8 token is invalid.\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
            // check if the value has more than 8 bits
            if (value >> 8)
            {
                ErrPrintf("Fill value(%s) following FILL8 has more than 8 bits.\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
            properties->fill8 = UINT08(value);
        }
        else if (0== strcmp(tok, "FILL32"))
        {
            properties->hdrStrProps["FILL32"] = m_TestHdrTokens.front();
            if (OK != ParseUINT32(&properties->fill32))
            {
                ErrPrintf("Fill value(%s) following FILL32 token is invalid.\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "VPR"))
        {
            if (OK != ParseString(&properties->vpr))
            {
                ErrPrintf("VPR requires <string>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "GPU_SMMU"))
        {
            if (OK != ParseString(&properties->gpuSmmu))
            {
                ErrPrintf("GPU_SMMU requires <string>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "SPARSE"))
        {
            properties->sparse = true;
        }
        else if (0 == strcmp(tok, "MAP_TO_BACKINGSTORE"))
        {
            if (params->ParamPresent("-disable_map_to_backingstore") == 0)
                properties->mapToBackingStore = true;
        }
        else if (0 == strcmp(tok, "TILE_WIDTH_IN_GOBS"))
        {
            if (OK != ParseUINT32(&properties->tileWidthInGobs))
            {
                ErrPrintf("TILE_WIDTH_IN_GOBS requires <unsigned integer>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "TEXTURE_SUBIMAGE"))
        {
            if ((OK != ParseUINT32(&properties->textureSubimageWidth)) ||
                (OK != ParseUINT32(&properties->textureSubimageHeight)) ||
                (OK != ParseUINT32(&properties->textureSubimageDepth)) ||
                (OK != ParseUINT32(&properties->textureSubimageXOffset)) ||
                (OK != ParseUINT32(&properties->textureSubimageYOffset)) ||
                (OK != ParseUINT32(&properties->textureSubimageZOffset)) ||
                (OK != ParseUINT32(&properties->textureSubimageMipLevel)) ||
                (OK != ParseUINT32(&properties->textureSubimageDimension)) ||
                (OK != ParseUINT32(&properties->textureSubimageArrayIndex)))
            {
                ErrPrintf("Unexpected TEXTURE_SUBIMAGE value \"%s\" - "
                          "expected <width> <height> <depth> <x offset> <y offset> "
                          "<z offset> <level> <lwbemap face> <array index>\n",
                          m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "TEXTURE_MIP_TAIL"))
        {
            if ((OK != ParseUINT32(
                &properties->textureMipTailStartingLevel)) ||
                (OK != ParseUINT32(&properties->textureMipTailDimension)) ||
                (OK != ParseUINT32(&properties->textureMipTailArrayIndex)))
            {
                ErrPrintf("Unexpected TEXTURE_MIP_TAIL value \"%s\" - expected "
                          "<starting level> <lwbemap face> <array index>\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "ATS_PAGE_SIZE"))
        {
            if (OK != ParseUINT32(&properties->atsPageSize))
            {
                ErrPrintf("ATS_PAGE_SIZE requires <unsigned integer>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if ((0 == strcmp(tok, "CREATE_PHYSICAL_ALLOC")) ||
                 (0 == strcmp(tok, "SCALE_BY_SM_COUNT")) ||
                 (0 == strcmp(tok, "ATS_MAP")) ||
                 (0 == strcmp(tok, "NO_GMMU_MAP")))
        {
            // These properties are valid but don't require any special
            // processing yet, so nothing to do here.
        }
        else if (0 == strcmp(tok, "SM_COUNT"))
        {
            if (OK != ParseUINT32(&properties->smCount))
            {
                ErrPrintf("SM_COUNT requires <unsigned integer>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "ADDRESS_SPACE"))
        {
            if (OK != ParseString(&properties->vasName))
            {
                ErrPrintf("ADDRESS_SPACE need a <vaspace name>.\n");
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "HTEX"))
        {
            properties->htex = true;
        }
        else if (0 == strcmp(tok, "SHARED"))
        {
            properties->isShared = true;
        }
        else if (0 == strcmp(tok, "PTE_KIND"))
        {
            if (OK != ParseUINT32(&properties->pteKind))
            {
                ErrPrintf("PTE_KIND requires <unsigned integer>, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "INHERIT_PTE_KIND"))
        {
            if (OK != ParseString(&properties->inheritPteKind))
            {
                ErrPrintf("INHERIT_PTE_KIND requires PHYSICAL|VIRTUAL, got \"%s\".\n", m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "TEXTURE_HEADER"))
        {
            properties->isTextureHeader = true;
        }
        else
        {
            ErrPrintf("Unknown property: %s\n", tok);

            if (propertyUsed->find(tok) != propertyUsed->end())
            {
                ErrPrintf("property %s should have been valid.\n", tok);
            }

            return RC::ILWALID_FILE_FORMAT;
        }

        MASSERT(propertyUsed->find(tok) != propertyUsed->end());

        (*propertyUsed)[tok] = true;
    }

    // Process property command-line arguments.  This must happen before
    // property error checking.
    CHECK_RC(ProcessPropertyCommandLineArguments(propertyUsed, properties));

    // Now do some error checking on the properties.

    // Note that loopback SHOULD NOT use surface.SetLoopback().  The
    // purpose of loopback is to cause the surface to be mapped in
    // BOTH normal and loopback mode.  Using surface.SetLoopback()
    // would caused the surface to be mapped in loopback mode ONLY
    if ((*propertyUsed)["LOOPBACK"] && properties->loopback)
    {
        if (properties->aperture != "VIDEO")
        {
            ErrPrintf("LOOPBACK only supported with VIDEO apertures\n");
            return RC::ILWALID_FILE_FORMAT;
        }
        if (params->ParamPresent("-sli_p2ploopback") > 0)
        {
            ErrPrintf("LOOPBACK not compatible with \"-sli_p2ploopback\" command line option\n");
            return RC::ILWALID_FILE_FORMAT;
        }
        if (m_Test->GetBoundGpuDevice()->GetNumSubdevices() > 1)
        {
            ErrPrintf("LOOPBACK not supported on SLI devices.\n");
            return RC::ILWALID_FILE_FORMAT;
        }
    }

    if ((*propertyUsed)["RAW"] && properties->raw && !(*propertyUsed)["FILE"])
    {
        ErrPrintf("Property RAW must be used with the FILE property.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    if (!(*propertyUsed)["FILE"] &&
        !(*propertyUsed)["SIZE"] &&
        !(*propertyUsed)["WIDTH"] &&
        !(*propertyUsed)["HEIGHT"] &&
        !(*propertyUsed)["PHYSICAL_ALLOC"])
    {
        ErrPrintf("Memory command can't determine size.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    if ((*propertyUsed)["SIZE"])
    {
        if ((*propertyUsed)["WIDTH"])
        {
            ErrPrintf("Property SIZE may not be used with the WIDTH property.\n");
            return RC::ILWALID_FILE_FORMAT;
        }
        else if ((*propertyUsed)["HEIGHT"])
        {
            ErrPrintf("Property SIZE may not be used with the HEIGHT property.\n");
            return RC::ILWALID_FILE_FORMAT;
        }
    }
    else if ((*propertyUsed)["WIDTH"] && !(*propertyUsed)["HEIGHT"])
    {
        ErrPrintf("Property WIDTH requires property HEIGHT.\n");
        return RC::ILWALID_FILE_FORMAT;
    }
    else if ((*propertyUsed)["HEIGHT"] && !(*propertyUsed)["WIDTH"])
    {
        ErrPrintf("Property HEIGHT requires property WIDTH.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    if ((*propertyUsed)["FILE"] && (*propertyUsed)["FILL8"])
    {
        ErrPrintf("Property FILE and FILL8 cannot both exist.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    if ((*propertyUsed)["FILE"] && (*propertyUsed)["FILL32"])
    {
        ErrPrintf("Property FILE and FILL32 cannot both exist.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    if ((*propertyUsed)["FILL8"] && (*propertyUsed)["FILL32"])
    {
        ErrPrintf("Property FILL8 and FILL32 cannot both exist.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    if ((*propertyUsed)["SCALE_BY_SM_COUNT"] && !(*propertyUsed)["SIZE"])
    {
        ErrPrintf("Property SCALE_BY_SM_COUNT must be used with property SIZE.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    return rc;
}

//---------------------------------------------------------------------------
// Process any command-line arguments that can override a property.
//
RC GpuTrace::ProcessPropertyCommandLineArguments(
    PropertyUsageMap *propertyUsed, MemoryCommandProperties *properties)
{
    RC rc = OK;
    UINT32 i;
    UINT32 paramCount;
    vector<string>::const_iterator tagIter;

    for (tagIter = properties->tagList.begin();
         tagIter != properties->tagList.end();
         ++tagIter)
    {
        paramCount = params->ParamPresent("-as_size");
        for (i = 0; i < paramCount; ++i)
        {
            if (params->ParamNStr("-as_size", i, 0) == *tagIter)
            {
                properties->size = params->ParamNUnsigned("-as_size", i, 1);
                (*propertyUsed)["SIZE"] = true;
            }
        }

        paramCount = params->ParamPresent("-as_access");
        for (i = 0; i < paramCount; ++i)
        {
            if (params->ParamNStr("-as_access", i, 0) == *tagIter)
            {
                properties->access = params->ParamNStr("-as_access", i, 1);
                (*propertyUsed)["ACCESS"] = true;
            }
        }

        paramCount = params->ParamPresent("-as_shader_access");
        for (i = 0; i < paramCount; ++i)
        {
            if (params->ParamNStr("-as_shader_access", i, 0) == *tagIter)
            {
                properties->shaderAccess = params->ParamNStr("-as_shader_access", i, 1);
                (*propertyUsed)["SHADER_ACCESS"] = true;
            }
        }

        paramCount = params->ParamPresent("-as_sm_count");
        for (i = 0; i < paramCount; ++i)
        {
            if (params->ParamNStr("-as_sm_count", i, 0) == *tagIter)
            {
                properties->smCount = params->ParamNUnsigned("-as_sm_count", i, 1);
                (*propertyUsed)["SM_COUNT"] = true;
            }
        }
    }

    return rc;
}

//---------------------------------------------------------------------------
RC GpuTrace::InitSurfaceFromProperties(const string &name, MdiagSurf *surface,
    SurfaceType surfaceType, const MemoryCommandProperties &properties,
    PropertyUsageMap *propertyUsed)
{
    RC rc = OK;

    surface->SetName(name);
    surface->SetCreatedFromAllocSurface();
    surface->SetLocation(Memory::Fb);

    ColorUtils::Format format = ColorUtils::StringToFormat(properties.format);

    if (format == ColorUtils::LWFMT_NONE)
    {
        ErrPrintf("FORMAT %s is not a valid color format.\n", properties.format.c_str());

        return RC::ILWALID_FILE_FORMAT;
    }

    surface->SetColorFormat(format);

    CHECK_RC(SetSurfaceAAMode(surface, properties.sampleMode));
    CHECK_RC(SetSurfaceLayout(surface, properties.layout));
    CHECK_RC(SetSurfaceCompression(surface, properties.compression));
    CHECK_RC(SetSurfaceAperture(surface, properties.aperture));

    if ((*propertyUsed)["TYPE"])
    {
        CHECK_RC(SetSurfaceType(surface, properties.type));
    }
    else if (surfaceType == SURFACE_TYPE_Z)
    {
        surface->SetType(LWOS32_TYPE_DEPTH);
    }
    else if (surfaceType == SURFACE_TYPE_STENCIL)
    {
        surface->SetType(LWOS32_TYPE_STENCIL);
    }
    else
    {
        surface->SetType(LWOS32_TYPE_IMAGE);
    }

    // Moving SetSurfaceZlwll after SetType so MODS can enable Zlwll
    // for all depth buffers if option -zlwll/-slwll present.
    CHECK_RC(SetSurfaceZlwll(surface, properties.zlwll));

    if ((EngineClasses::IsGpuFamilyClassOrLater(
            TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_KEPLER) ||
         params->ParamPresent("-buf_default_readonly") > 0) &&
        !IsSurfaceTypeColorZ3D(surfaceType) &&
        !(*propertyUsed)["ACCESS"])
    {
        // For many ACE traces gilded on Amodel, they might trigger some access
        // violation issues on fmodel, the reason is some ACE traces don't set
        // buffer access attribute and MODS takes the default value as "RW"
        // which used to be "RO" for "FILE" command. In order to align the
        // attribute, here we set non-Ct/Zt buffers to be RO by default.
        // By now, many Fermi ACE/compute traces are using ALLOC_SURFACE, it's
        // almost impossible to redump the affected traces for this MODS change,
        // so we just apply this for Kepler or later. For debug use, we also
        // introduce the option -buf_default_readonly.
        // See detail at bug 631639.
        surface->SetProtect(Memory::Readable);
    }
    else
    {
        CHECK_RC(SetSurfaceProtect(surface, properties.access));
    }

    // Set the default ATS permission values to match the GMMU permissions.
    // This can be overrided later by ATS-specific options.
    surface->SetAtsReadPermission((surface->GetProtect() & Memory::Readable) != 0);
    surface->SetAtsWritePermission((surface->GetProtect() & Memory::Writeable) != 0);

    if ((*propertyUsed)["SHADER_ACCESS"])
    {
        CHECK_RC(SetSurfaceShaderProtect(surface, properties.shaderAccess));
    }

    CHECK_RC(SetSurfacePrivileged(surface, properties.privileged));

    if ((*propertyUsed)["GPU_CACHE"])
    {
        CHECK_RC(SetSurfaceGpuCache(surface, properties.gpuCache));
    }

    if ((*propertyUsed)["P2P_GPU_CACHE"])
    {
        CHECK_RC(SetSurfaceP2PGpuCache(surface, properties.p2pGpuCache));
    }

    if ((*propertyUsed)["ZBC"])
    {
        CHECK_RC(SetSurfaceZbc(surface, properties.zbc));
    }

    if ((*propertyUsed)["VPR"])
    {
        CHECK_RC(SetSurfaceVpr(surface, properties.vpr));
    }

    surface->SetWidth(properties.width);
    surface->SetHeight(properties.height);
    surface->SetDepth(properties.depth);
    surface->SetPitch(properties.pitch);
    surface->SetArraySize(properties.arraySize);
    surface->SetLogBlockWidth(log2(properties.blockWidth));
    surface->SetLogBlockHeight(log2(properties.blockHeight));
    surface->SetLogBlockDepth(log2(properties.blockDepth));

    surface->SetAlignment(properties.physAlignment);
    surface->SetVirtAlignment(properties.virtAlignment);
    surface->SetIsHtex(properties.htex);

    // ATTR_OVERRIDE and ATTR2_OVERRIDE are used to specify the attibutes
    // used by RM to allocate the surface.  This should override the settings
    // of properties like LAYOUT and SAMPLE_MODE.
    if ((*propertyUsed)["ATTR_OVERRIDE"])
    {
        // Store the attributes on the surface and set various
        // data members to match the attributes (e.g., compression).
        //
        // This function must be called after the ALLOC_SURFACE properties
        // are set on the surface but before the command-line arguments
        // are processed.
        surface->ConfigFromAttr(properties.attrOverride);
    }

    if ((*propertyUsed)["ATTR2_OVERRIDE"])
    {
        surface->ConfigFromAttr2(properties.attr2Override);
    }

    if ((*propertyUsed)["TYPE_OVERRIDE"])
    {
        surface->SetType(properties.typeOverride);
    }

    if ((*propertyUsed)["VIRT_ADDRESS"] &&
        (*propertyUsed)["VIRT_ADDRESS_RANGE"])
    {
        ErrPrintf("VIRT_ADDRESS and VIRT_ADDRESS_RANGE may not both be used for the same surface\n");
        return RC::ILWALID_FILE_FORMAT;
    }
    else if ((*propertyUsed)["VIRT_ADDRESS"])
    {
        surface->SetFixedVirtAddr(properties.virtAddress);
    }
    else if ((*propertyUsed)["VIRT_ADDRESS_RANGE"])
    {
        surface->SetVirtAddrRange(properties.virtAddressMin,
            properties.virtAddressMax);
    }

    if ((*propertyUsed)["PHYS_ADDRESS"] &&
        (*propertyUsed)["PHYS_ADDRESS_RANGE"])
    {
        ErrPrintf("PHYS_ADDRESS and PHYS_ADDRESS_RANGE may not both be used for the same surface\n");
        return RC::ILWALID_FILE_FORMAT;
    }
    else if (params->ParamPresent("-ignore_trace_phys_address") > 0)
    {
        // Ignore the PHYS_ADDRESS and PHYS_ADDRESS_RANGE properties.
    }
    else if ((*propertyUsed)["PHYS_ADDRESS"])
    {
        surface->SetFixedPhysAddr(properties.physAddress);
    }
    else if ((*propertyUsed)["PHYS_ADDRESS_RANGE"])
    {
        surface->SetPhysAddrRange(properties.physAddressMin,
            properties.physAddressMax);
    }

    if ((*propertyUsed)["CONTIG"])
    {
        surface->SetPhysContig(true);
    }

    if ((*propertyUsed)["HOST_REFLECTED"] && properties.hostReflected)
    {
        if (0 == GetChannel(properties.hostReflectedChName, GPU_MANAGED))
        {
            ErrPrintf("HOST_REFLECTED need a GPU managed channel\n");
            return RC::ILWALID_FILE_FORMAT;
        }

        surface->SetHostReflectedSurf();
        surface->SetGpuManagedChName(properties.hostReflectedChName);
    }

    if ((*propertyUsed)["SKED_REFLECTED"] && properties.skedReflected)
    {
        surface->SetSkedReflected(true);
    }

    if ((*propertyUsed)["MIPLEVEL_SIZE"])
    {
        surface->SetMipLevels(properties.mipLevels);
    }

    if ((*propertyUsed)["BORDER"])
    {
        surface->SetBorder(properties.border);
    }

    if ((*propertyUsed)["LWBEMAP"])
    {
        surface->SetDimensions(6);
    }
    else
    {
        surface->SetDimensions(1);
    }

    if ((*propertyUsed)["PAGE_SIZE"])
    {
        CHECK_RC(SetSurfacePageSize(surface, properties.pageSize));
    }

    if ((*propertyUsed)["MAP_MODE"])
    {
        CHECK_RC(SetSurfaceMapMode(surface, properties.mapMode));
    }

    if ((*propertyUsed)["SPARSE"])
    {
        surface->SetIsSparse(properties.sparse);
    }

    if ((*propertyUsed)["MAP_TO_BACKINGSTORE"])
    {
        if (properties.mapToBackingStore)
        {
            LwRm* pLwRm = m_Test->GetLwRmPtr();
            LW0080_CTRL_FB_GET_COMPBIT_STORE_INFO_PARAMS param = {0};

            CHECK_RC(pLwRm->Control(
                         pLwRm->GetDeviceHandle(m_Test->GetBoundGpuDevice()),
                         LW0080_CTRL_CMD_FB_GET_COMPBIT_STORE_INFO,
                         &param, sizeof(param)));
            if (param.AddressSpace != LW0080_CTRL_CMD_FB_GET_COMPBIT_STORE_INFO_ADDRESS_SPACE_FBMEM)
            {
                MASSERT(!"MODS only support backing store mappings in FB!");
            }
            surface->SetFixedPhysAddr(param.Address);

            surface->SetLocation(Memory::Fb);
        }
    }

    if ((*propertyUsed)["TILE_WIDTH_IN_GOBS"])
    {
        surface->SetTileWidthInGobs(properties.tileWidthInGobs);
    }

    if ((*propertyUsed)["ATS_MAP"])
    {
        surface->SetAtsMapped();
    }

    if ((*propertyUsed)["NO_GMMU_MAP"])
    {
        surface->SetNoGmmuMap();
    }

    if ((*propertyUsed)["ATS_PAGE_SIZE"])
    {
        surface->SetAtsPageSize(properties.atsPageSize);
    }

    if (surface->HasVirtual())
    {
        LwRm::Handle hVASpace;
        if ((*propertyUsed)["ADDRESS_SPACE"])
        {
            hVASpace = GetVASHandle(properties.vasName);
        }
        else
        {
            hVASpace = GetVASHandle("default");
        }
        surface->SetGpuVASpace(hVASpace);
    }

    if ((*propertyUsed)["PTE_KIND"])
    {
        surface->SetPteKind(properties.pteKind);
    }

    if ((*propertyUsed)["INHERIT_PTE_KIND"])
    {
        CHECK_RC(SetSurfaceInheritPteKind(surface, properties.inheritPteKind));
    }

    CHECK_RC(ProcessGlobalArguments(surface, surfaceType));

    vector<string>::const_iterator tagIter;

    for (tagIter = properties.tagList.begin();
         tagIter != properties.tagList.end();
         ++tagIter)
    {
        CHECK_RC(GetAllocSurfaceParameters(surface, (*tagIter)));
    }

    // Expand the dimension by AA scale, Ct & Zt are expanded in surfacemgr
    if (!IsSurfaceTypeColorZ3D(surfaceType))
    {
        surface->SetWidth(surface->GetWidth() * surface->GetAAWidthScale());
        surface->SetHeight(surface->GetHeight() * surface->GetAAHeightScale());
    }

    return rc;
}

//---------------------------------------------------------------------------
void GpuTrace::InitZbcClearFmtMap()
{
    m_ZbcClearFmtMap["INVALID"]             = INVALID; //color and depth has the same value for invalid, both of them are 0x0
    m_ZbcClearFmtMap["FP32"]                = FP32;    //used for depth format
    m_ZbcClearFmtMap["ZERO"]                = ZERO;
    m_ZbcClearFmtMap["UNORM_ONE"]           = UNORM_ONE;
    m_ZbcClearFmtMap["RF32_GF32_BF32_AF32"] = RF32_GF32_BF32_AF32;
    m_ZbcClearFmtMap["R16_G16_B16_A16"]     = R16_G16_B16_A16;
    m_ZbcClearFmtMap["RN16_GN16_BN16_AN16"] = RN16_GN16_BN16_AN16;
    m_ZbcClearFmtMap["RS16_GS16_BS16_AS16"] = RS16_GS16_BS16_AS16;
    m_ZbcClearFmtMap["RU16_GU16_BU16_AU16"] = RU16_GU16_BU16_AU16;
    m_ZbcClearFmtMap["RF16_GF16_BF16_AF16"] = RF16_GF16_BF16_AF16;
    m_ZbcClearFmtMap["A8R8G8B8"]            = A8R8G8B8;
    m_ZbcClearFmtMap["A8RL8GL8BL8"]         = A8RL8GL8BL8;
    m_ZbcClearFmtMap["A2B10G10R10"]         = A2B10G10R10;
    m_ZbcClearFmtMap["AU2BU10GU10RU10"]     = AU2BU10GU10RU10;
    m_ZbcClearFmtMap["A8B8G8R8"]            = A8B8G8R8;
    m_ZbcClearFmtMap["A8BL8GL8RL8"]         = A8BL8GL8RL8;
    m_ZbcClearFmtMap["AN8BN8GN8RN8"]        = AN8BN8GN8RN8;
    m_ZbcClearFmtMap["AS8BS8GS8RS8"]        = AS8BS8GS8RS8;
    m_ZbcClearFmtMap["AU8BU8GU8RU8"]        = AU8BU8GU8RU8;
    m_ZbcClearFmtMap["A2R10G10B10"]         = A2R10G10B10;
    m_ZbcClearFmtMap["BF10GF11RF11"]        = BF10GF11RF11;
    m_ZbcClearFmtMap["U8"]                  = U8;
}
//---------------------------------------------------------------------------
void GpuTrace::InitMemoryCommandPropertyMap()
{
    m_MemoryCommandPropertyMap["SHARED"] =
        MC_ALLOC_SURFACE |
        MC_ALLOC_PHYSICAL;

    m_MemoryCommandPropertyMap["FILE"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW |
        MC_ALLOC_VIRTUAL |
        MC_MAP;

    m_MemoryCommandPropertyMap["FORMAT"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW |
        MC_ALLOC_VIRTUAL |
        MC_ALLOC_PHYSICAL;

    m_MemoryCommandPropertyMap["SAMPLE_MODE"] =
        MC_ALLOC_SURFACE |
        MC_ALLOC_VIRTUAL |
        MC_SURFACE_VIEW;

    m_MemoryCommandPropertyMap["LAYOUT"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW |
        MC_ALLOC_VIRTUAL |
        MC_ALLOC_PHYSICAL;

    m_MemoryCommandPropertyMap["COMPRESSION"] = MC_ALLOC_SURFACE;
    m_MemoryCommandPropertyMap["ZLWLL"] = MC_ALLOC_SURFACE;

    m_MemoryCommandPropertyMap["APERTURE"] =
        MC_ALLOC_SURFACE |
        MC_ALLOC_PHYSICAL;

    m_MemoryCommandPropertyMap["TYPE"] = MC_ALLOC_SURFACE |
        MC_ALLOC_VIRTUAL |
        MC_ALLOC_PHYSICAL;

    m_MemoryCommandPropertyMap["ACCESS"] = MC_ALLOC_SURFACE | MC_MAP;
    m_MemoryCommandPropertyMap["SHADER_ACCESS"] = MC_ALLOC_SURFACE;
    m_MemoryCommandPropertyMap["PRIVILEGED"] = MC_ALLOC_SURFACE;

    m_MemoryCommandPropertyMap["CRC_CHECK"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW |
        MC_MAP;

    m_MemoryCommandPropertyMap["CRC_CHECK_NO_MATCH"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW |
        MC_MAP;

    m_MemoryCommandPropertyMap["CHANNEL"] = MC_ALLOC_SURFACE;

    m_MemoryCommandPropertyMap["REFERENCE_CHECK"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW  |
        MC_MAP;

    m_MemoryCommandPropertyMap["REFERENCE_CHECK_NO_MATCH"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW;

    m_MemoryCommandPropertyMap["GPU_CACHE"] = MC_ALLOC_SURFACE;
    m_MemoryCommandPropertyMap["P2P_GPU_CACHE"] = MC_ALLOC_SURFACE;
    m_MemoryCommandPropertyMap["ZBC"] = MC_ALLOC_SURFACE;

    m_MemoryCommandPropertyMap["CRC_RECT"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW;

    m_MemoryCommandPropertyMap["PARENT_SURFACE"] = MC_SURFACE_VIEW;

    m_MemoryCommandPropertyMap["WIDTH"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW |
        MC_ALLOC_VIRTUAL;

    m_MemoryCommandPropertyMap["HEIGHT"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW |
        MC_ALLOC_VIRTUAL;

    m_MemoryCommandPropertyMap["DEPTH"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW |
        MC_ALLOC_VIRTUAL;

    m_MemoryCommandPropertyMap["PITCH"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW |
        MC_ALLOC_PHYSICAL |
        MC_ALLOC_VIRTUAL;

    m_MemoryCommandPropertyMap["BLOCK_WIDTH"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW |
        MC_ALLOC_VIRTUAL;

    m_MemoryCommandPropertyMap["BLOCK_HEIGHT"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW |
        MC_ALLOC_VIRTUAL;

    m_MemoryCommandPropertyMap["BLOCK_DEPTH"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW |
        MC_ALLOC_VIRTUAL;

    m_MemoryCommandPropertyMap["ARRAY_SIZE"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW |
        MC_ALLOC_VIRTUAL;

    m_MemoryCommandPropertyMap["MIPLEVEL_SIZE"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW  |
        MC_ALLOC_VIRTUAL;

    m_MemoryCommandPropertyMap["BORDER"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW  |
        MC_ALLOC_VIRTUAL;

    m_MemoryCommandPropertyMap["LWBEMAP"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW  |
        MC_ALLOC_VIRTUAL;

    m_MemoryCommandPropertyMap["SWAP_SIZE"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW |
        MC_ALLOC_VIRTUAL |
        MC_MAP;

    m_MemoryCommandPropertyMap["PHYS_ALIGNMENT"] =
        MC_ALLOC_SURFACE |
        MC_ALLOC_PHYSICAL;

    m_MemoryCommandPropertyMap["VIRT_ALIGNMENT"] =
        MC_ALLOC_SURFACE |
        MC_ALLOC_VIRTUAL;

    m_MemoryCommandPropertyMap["SIZE"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW |
        MC_ALLOC_VIRTUAL |
        MC_ALLOC_PHYSICAL |
        MC_MAP;

    m_MemoryCommandPropertyMap["ATTR_OVERRIDE"] =
        MC_ALLOC_SURFACE |
        MC_ALLOC_VIRTUAL |
        MC_ALLOC_PHYSICAL;

    m_MemoryCommandPropertyMap["ATTR2_OVERRIDE"] =
        MC_ALLOC_SURFACE |
        MC_ALLOC_VIRTUAL |
        MC_ALLOC_PHYSICAL;

    m_MemoryCommandPropertyMap["TYPE_OVERRIDE"] =
        MC_ALLOC_SURFACE |
        MC_ALLOC_VIRTUAL |
        MC_ALLOC_PHYSICAL;

    m_MemoryCommandPropertyMap["LOOPBACK"] = MC_ALLOC_SURFACE;

    m_MemoryCommandPropertyMap["RAW"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW;

    m_MemoryCommandPropertyMap["RAW_CRC"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW;

    m_MemoryCommandPropertyMap["SKED_REFLECTED"] =
        MC_ALLOC_SURFACE |
        MC_ALLOC_VIRTUAL;

    m_MemoryCommandPropertyMap["VIRT_ADDRESS"] =
        MC_ALLOC_SURFACE |
        MC_ALLOC_VIRTUAL;

    m_MemoryCommandPropertyMap["PHYS_ADDRESS"] =
        MC_ALLOC_SURFACE |
        MC_ALLOC_PHYSICAL;

    m_MemoryCommandPropertyMap["VIRT_ADDRESS_RANGE"] =
        MC_ALLOC_SURFACE |
        MC_ALLOC_VIRTUAL;

    m_MemoryCommandPropertyMap["PHYS_ADDRESS_RANGE"] =
        MC_ALLOC_SURFACE |
        MC_ALLOC_PHYSICAL;

    m_MemoryCommandPropertyMap["ADDRESS_OFFSET"] = MC_SURFACE_VIEW;

    m_MemoryCommandPropertyMap["TAG"] = MC_ALL_COMMANDS;

    m_MemoryCommandPropertyMap["HOST_REFLECTED"] =
        MC_ALLOC_SURFACE |
        MC_ALLOC_VIRTUAL;

    m_MemoryCommandPropertyMap["VIRTUAL_ALLOC"] = MC_MAP;
    m_MemoryCommandPropertyMap["PHYSICAL_ALLOC"] = MC_MAP;
    m_MemoryCommandPropertyMap["VIRTUAL_OFFSET"] = MC_MAP;
    m_MemoryCommandPropertyMap["PHYSICAL_OFFSET"] = MC_MAP;

    m_MemoryCommandPropertyMap["PAGE_SIZE"] =
        MC_ALLOC_SURFACE |
        MC_ALLOC_VIRTUAL |
        MC_ALLOC_PHYSICAL;

    m_MemoryCommandPropertyMap["MAP_MODE"] = MC_ALLOC_SURFACE;

    m_MemoryCommandPropertyMap["CLASS"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW |
        MC_MAP;

    m_MemoryCommandPropertyMap["FILL8"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW |
        MC_ALLOC_PHYSICAL |
        MC_MAP;
    m_MemoryCommandPropertyMap["FILL32"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW |
        MC_ALLOC_PHYSICAL |
        MC_MAP;

    m_MemoryCommandPropertyMap["VPR"] =
        MC_ALLOC_SURFACE |
        MC_ALLOC_PHYSICAL;

    m_MemoryCommandPropertyMap["GPU_SMMU"] =
        MC_ALLOC_SURFACE;

    m_MemoryCommandPropertyMap["SPARSE"] =
        MC_ALLOC_VIRTUAL;

    m_MemoryCommandPropertyMap["MAP_TO_BACKINGSTORE"] =
        MC_ALLOC_SURFACE;

    m_MemoryCommandPropertyMap["TILE_WIDTH_IN_GOBS"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW |
        MC_ALLOC_VIRTUAL;

    m_MemoryCommandPropertyMap["TEXTURE_SUBIMAGE"] = MC_MAP;
    m_MemoryCommandPropertyMap["TEXTURE_MIP_TAIL"] = MC_MAP;
    m_MemoryCommandPropertyMap["CREATE_PHYSICAL_ALLOC"] = MC_MAP;

    m_MemoryCommandPropertyMap["SCALE_BY_SM_COUNT"] =
        MC_ALLOC_SURFACE |
        MC_ALLOC_VIRTUAL;

    m_MemoryCommandPropertyMap["SM_COUNT"] =
        MC_ALLOC_SURFACE |
        MC_ALLOC_VIRTUAL;

    m_MemoryCommandPropertyMap["ATS_MAP"] =
        MC_ALLOC_SURFACE |
        MC_MAP;

    m_MemoryCommandPropertyMap["NO_GMMU_MAP"] =
        MC_ALLOC_SURFACE |
        MC_MAP;

    m_MemoryCommandPropertyMap["ATS_PAGE_SIZE"] =
        MC_ALLOC_SURFACE |
        MC_MAP;

    m_MemoryCommandPropertyMap["ADDRESS_SPACE"] =
        MC_ALLOC_SURFACE |
        MC_ALLOC_VIRTUAL;

    m_MemoryCommandPropertyMap["CONTIG"] =
        MC_ALLOC_SURFACE |
        MC_ALLOC_PHYSICAL;

    m_MemoryCommandPropertyMap["HTEX"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW |
        MC_ALLOC_VIRTUAL;

    m_MemoryCommandPropertyMap["PTE_KIND"] =
        MC_ALLOC_SURFACE |
        MC_ALLOC_VIRTUAL |
        MC_ALLOC_PHYSICAL;

    m_MemoryCommandPropertyMap["INHERIT_PTE_KIND"] =
        MC_MAP;

    m_MemoryCommandPropertyMap["TEXTURE_HEADER"] =
        MC_ALLOC_SURFACE |
        MC_SURFACE_VIEW |
        MC_MAP;
}

RC GpuTrace::SetSurfaceAAMode(MdiagSurf *surface, const string &mode)
{
    if (mode == "1X1")
    {
        surface->SetAAMode(Surface2D::AA_1);
    }
    else if (mode == "2X1")
    {
        surface->SetAAMode(Surface2D::AA_2);
    }
    else if (mode == "2X1_D3D")
    {
        surface->SetAAMode(Surface2D::AA_2);
        surface->SetD3DSwizzled(true);
    }
    else if (mode == "2X2")
    {
        surface->SetAAMode(Surface2D::AA_4_Rotated);
    }
    else if (mode == "4X2")
    {
        surface->SetAAMode(Surface2D::AA_8);
    }
    else if (mode == "4X4")
    {
        surface->SetAAMode(Surface2D::AA_16);
    }
    else if (mode == "4X2_D3D")
    {
        surface->SetAAMode(Surface2D::AA_8);
        surface->SetD3DSwizzled(true);
    }
    else if (mode == "2X2_VC_4")
    {
        surface->SetAAMode(Surface2D::AA_4_Virtual_8);
    }
    else if (mode == "2X2_VC_12")
    {
        surface->SetAAMode(Surface2D::AA_4_Virtual_16);
    }
    else if (mode == "4X2_VC_8")
    {
        surface->SetAAMode(Surface2D::AA_8_Virtual_16);
    }
    else if (mode == "4X2_VC_24")
    {
        surface->SetAAMode(Surface2D::AA_8_Virtual_32);
    }
    else
    {
        ErrPrintf("Unrecognized SAMPLE_MODE %s for ALLOC_SURFACE command.\n", mode.c_str());

        return RC::ILWALID_FILE_FORMAT;
    }

    return OK;
}

RC GpuTrace::SetSurfaceLayout(MdiagSurf *surface, const string &layout)
{
    if (layout == "BLOCKLINEAR")
    {
        surface->SetLayout(Surface2D::BlockLinear);
    }
    else if (layout == "PITCH")
    {
        surface->SetLayout(Surface2D::Pitch);
    }
    else if (layout == "SWIZZLED")
    {
        MASSERT(!"SWIZZLED surface layout should no longer be used anywhere");
        surface->SetLayout(Surface2D::Swizzled);
    }
    else
    {
        ErrPrintf("Unrecognized LAYOUT %s for ALLOC_SURFACE command.\n", layout.c_str());

        return RC::ILWALID_FILE_FORMAT;
    }

    return OK;
}

RC GpuTrace::SetSurfaceCompression(MdiagSurf *surface, const string &compression)
{
    if (compression == "REQUIRED")
    {
        surface->SetCompressed(true);
        surface->SetCompressedFlag(LWOS32_ATTR_COMPR_REQUIRED);
    }
    else if (compression == "ANY")
    {
        surface->SetCompressed(true);
        surface->SetCompressedFlag(LWOS32_ATTR_COMPR_ANY);
    }
    else if (compression == "NONE")
    {
        surface->SetCompressed(false);
        surface->SetCompressedFlag(LWOS32_ATTR_COMPR_NONE);
    }
    else
    {
        ErrPrintf("Unrecognized COMPRESSION %s for ALLOC_SURFACE command.\n", compression.c_str());

        return RC::ILWALID_FILE_FORMAT;
    }

    return OK;
}

RC GpuTrace::SetSurfaceZlwll(MdiagSurf *surface, const string &zlwll)
{
    // Zlwll is not supported on Amodel.
    if (Platform::GetSimulationMode() != Platform::Amodel)
    {
        if (zlwll == "REQUIRED")
        {
            surface->SetZLwllFlag(LWOS32_ATTR_ZLWLL_REQUIRED);
        }
        else if (zlwll == "ANY")
        {
            surface->SetZLwllFlag(LWOS32_ATTR_ZLWLL_ANY);
        }
        else if (zlwll == "SHARED")
        {
            ErrPrintf("%s ZLWLL is no longer supported for ALLOC_SURFACE command.\n", zlwll.c_str());

            return RC::ILWALID_FILE_FORMAT;
        }
        else if (zlwll == "NONE")
        {
            surface->SetZLwllFlag(LWOS32_ATTR_ZLWLL_NONE);
        }
        else
        {
            ErrPrintf("Unrecognized ZLWLL %s for ALLOC_SURFACE command.\n", zlwll.c_str());

            return RC::ILWALID_FILE_FORMAT;
        }
    }
    else
    {
        surface->SetZLwllFlag(LWOS32_ATTR_ZLWLL_NONE);
    }

    return OK;
}

RC GpuTrace::SetSurfaceAperture(MdiagSurf *surface, const string &aperture)
{
    if (aperture == "VIDEO")
    {
        surface->SetLocation(Memory::Fb);
    }
    else if (aperture == "COHERENT_SYSMEM")
    {
        surface->SetLocation(Memory::Coherent);
    }
    else if (aperture == "NONCOHERENT_SYSMEM")
    {
        surface->SetLocation(Memory::NonCoherent);
    }
    else if (aperture == "P2P")
    {
        surface->SetLocation(Memory::Fb);
        surface->SetNeedsPeerMapping(true);

        if (params->ParamPresent("-sli_p2ploopback"))
        {
            surface->SetLoopBack(true);
        }
    }
    else
    {
        ErrPrintf("Unrecognized APERTURE %s for ALLOC_SURFACE command.\n", aperture.c_str());

        return RC::ILWALID_FILE_FORMAT;
    }

    return OK;
}

RC GpuTrace::SetSurfaceGpuCache(MdiagSurf *surface, const string &value)
{
    if (value == "ON")
    {
        surface->SetGpuCacheMode(Surface2D::GpuCacheOn);
    }
    else if (value == "OFF")
    {
        surface->SetGpuCacheMode(Surface2D::GpuCacheOff);
    }
    else
    {
        ErrPrintf("Unrecognized GPU_CACHE value %s for ALLOC_SURFACE command.\n", value.c_str());

        return RC::ILWALID_FILE_FORMAT;
    }

    return OK;
}

RC GpuTrace::SetSurfaceP2PGpuCache(MdiagSurf *surface, const string &value)
{
    if (value == "ON")
    {
        surface->SetP2PGpuCacheMode(Surface2D::GpuCacheOn);
    }
    else if (value == "OFF")
    {
        surface->SetP2PGpuCacheMode(Surface2D::GpuCacheOff);
    }
    else
    {
        ErrPrintf("Unrecognized P2P_GPU_CACHE value %s for ALLOC_SURFACE command.\n", value.c_str());

        return RC::ILWALID_FILE_FORMAT;
    }

    return OK;
}

RC GpuTrace::SetSurfaceZbc(MdiagSurf *surface, const string &value)
{
    if (value == "ON")
    {
        surface->SetZbcMode(Surface2D::ZbcOn);
    }
    else if (value == "OFF")
    {
        surface->SetZbcMode(Surface2D::ZbcOff);
    }
    else
    {
        ErrPrintf("Unrecognized ZBC value %s for ALLOC_SURFACE command.\n", value.c_str());

        return RC::ILWALID_FILE_FORMAT;
    }

    return OK;
}

RC GpuTrace::SetSurfaceType(MdiagSurf *surface, const string &type)
{
    if (type == "IMAGE")
    {
        surface->SetType(LWOS32_TYPE_IMAGE);
    }
    else if (type == "DEPTH")
    {
        surface->SetType(LWOS32_TYPE_DEPTH);
    }
    else if (type == "TEXTURE")
    {
        surface->SetType(LWOS32_TYPE_TEXTURE);
    }
    else if (type == "VIDEO")
    {
        surface->SetType(LWOS32_TYPE_VIDEO);
    }
    else if (EngineClasses::IsGpuFamilyClassOrLater(
                  TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_BLACKWELL) && 
            (type == "STENCIL"))
    {
        surface->SetType(LWOS32_TYPE_STENCIL);
    }
    else
    {
        ErrPrintf("Unrecognized TYPE %s for ALLOC_SURFACE command.\n", type.c_str());

        return RC::ILWALID_FILE_FORMAT;
    }

    return OK;
}

RC GpuTrace::SetSurfaceProtect(MdiagSurf *surface, const string &protect)
{
    if (protect == "READ_ONLY")
    {
        surface->SetProtect(Memory::Readable);
    }
    else if (protect == "WRITE_ONLY")
    {
        surface->SetProtect(Memory::Writeable);
    }
    else if (protect == "READ_WRITE")
    {
        surface->SetProtect(Memory::ReadWrite);
    }
    else
    {
        ErrPrintf("Unrecognized ACCESS %s for ALLOC_SURFACE command.\n", protect.c_str());

        return RC::ILWALID_FILE_FORMAT;
    }

    return OK;
}

RC GpuTrace::SetSurfaceShaderProtect(MdiagSurf *surface, const string &shaderProtect)
{
    if (shaderProtect == "READ_ONLY")
    {
        surface->SetShaderProtect(Memory::Readable);
    }
    else if (shaderProtect == "WRITE_ONLY")
    {
        surface->SetShaderProtect(Memory::Writeable);
    }
    else if (shaderProtect == "READ_WRITE")
    {
        surface->SetShaderProtect(Memory::ReadWrite);
    }
    else
    {
        ErrPrintf("Unrecognized SHADER_ACCESS %s for ALLOC_SURFACE command.\n", shaderProtect.c_str());

        return RC::ILWALID_FILE_FORMAT;
    }

    return OK;
}

RC GpuTrace::SetSurfacePrivileged(MdiagSurf *surface,
    const string &privileged)
{
    if (privileged == "YES")
    {
        surface->SetPriv(true);
    }
    else if (privileged == "NO")
    {
        surface->SetPriv(false);
    }
    else
    {
        ErrPrintf("Unrecognized PRIVILEGED %s for ALLOC_SURFACE command.\n", privileged.c_str());

        return RC::ILWALID_FILE_FORMAT;
    }

    return OK;
}

RC GpuTrace::SetSurfacePageSize(MdiagSurf *surface, const string &pageSize)
{
    RC rc;
    UINT32 pageSizeNum = 0;
    CHECK_RC(GetPageSizeFromStr(pageSize, &pageSizeNum));
    surface->SetPageSize(pageSizeNum);

    return rc;
}

RC GpuTrace::SetSurfacePhysPageSize(MdiagSurf *surface, const string &pageSize)
{
    RC rc;
    UINT32 pageSizeNum = 0;
    CHECK_RC(GetPageSizeFromStr(pageSize, &pageSizeNum));
    surface->SetPhysAddrPageSize(pageSizeNum);

    return rc;
}

RC GpuTrace::GetPageSizeFromStr(const string &pageSizeStr, UINT32 *pageSizeNum) const
{
    RC rc;
    const GpuDevice *pGpuDev = m_Test->GetBoundGpuDevice();
    UINT64 pagesize = 0;

    if (pageSizeStr == "HUGE" || pageSizeStr == "2MB")
    {
        pagesize = GpuDevice::PAGESIZE_HUGE;
    }
    else if (pageSizeStr == "BIG" || pageSizeStr == "64KB" || pageSizeStr == "128KB")
    {
        pagesize = pGpuDev->GetBigPageSize();
    }
    else if (pageSizeStr == "SMALL" || pageSizeStr == "4KB")
    {
        pagesize = GpuDevice::PAGESIZE_SMALL;
    }
    else if (pageSizeStr == "512MB")
    {
        if (Platform::GetSimulationMode() != Platform::Amodel &&
            !pGpuDev->SupportsPageSize(GpuDevice::PAGESIZE_512MB))
        {
            ErrPrintf("Unsupported PAGE_SIZE %s for ALLOC_SURFACE command.\n",
                      pageSizeStr.c_str());

            return RC::ILWALID_FILE_FORMAT;
        }
        pagesize = GpuDevice::PAGESIZE_512MB;
    }
    else
    {
        ErrPrintf("Unrecognized PAGE_SIZE %s for ALLOC_SURFACE command.\n", pageSizeStr.c_str());

        return RC::ILWALID_FILE_FORMAT;
    }

    // MdiagSurf page size is stored in K-bytes, so divide by 1024.
    pagesize >>= 10;

    if (pageSizeNum)
        *pageSizeNum = UNSIGNED_CAST(UINT32, pagesize);

    return rc;
}

RC GpuTrace::SetSurfaceMapMode(MdiagSurf *surface, const string &mapMode)
{
    if (mapMode == "REFLECTED")
    {
        surface->SetMemoryMappingMode(Surface2D::MapReflected);
    }
    else if (mapMode == "DIRECT")
    {
        surface->SetMemoryMappingMode(Surface2D::MapDirect);
    }
    else
    {
        ErrPrintf("Unrecognized MAP_MODE %s for ALLOC_SURFACE command.\n", mapMode.c_str());

        return RC::ILWALID_FILE_FORMAT;
    }

    return OK;
}

RC GpuTrace::SetSurfaceFbSpeed(MdiagSurf *surface, const string &fbSpeed)
{
    if (fbSpeed == "FAST")
    {
        surface->SetFbSpeed(Surface2D::FbSpeedFast);
    }
    else if (fbSpeed == "SLOW")
    {
        surface->SetFbSpeed(Surface2D::FbSpeedSlow);
    }
    else if (fbSpeed == "DEFAULT")
    {
        // Allow RM to choose the FB speed.
    }
    else
    {
        ErrPrintf("Unrecognized FB_SPEED %s for ALLOC_SURFACE command.\n", fbSpeed.c_str());

        return RC::ILWALID_FILE_FORMAT;
    }

    return OK;
}

RC GpuTrace::SetSurfaceUpr(MdiagSurf *surface, const string &value)
{
    if (value == "ON")
    {
        surface->SetAllocInUpr(true);
    }
    else if (value == "OFF")
    {
        surface->SetAllocInUpr(false);
    }
    else
    {
        ErrPrintf("Unrecognized UPR value %s. ON or OFF is expected for -as_upr <name> <ON/OFF>.\n", value.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }

    return OK;
}

RC GpuTrace::SetSurfaceVpr(MdiagSurf *surface, const string &value)
{
    if (value == "ON")
    {
        surface->SetVideoProtected(true);
    }
    else if (value == "OFF")
    {
        surface->SetVideoProtected(false);
    }
    else
    {
        ErrPrintf("Unrecognized VPR value %s for ALLOC_SURFACE command.\n", value.c_str());

        return RC::ILWALID_FILE_FORMAT;
    }

    return OK;
}

RC GpuTrace::SetSurfaceAtsReadPermission(MdiagSurf *surface, const string &value)
{
    if (value == "TRUE")
    {
        surface->SetAtsReadPermission(true);
    }
    else if (value == "FALSE")
    {
        surface->SetAtsReadPermission(false);
    }
    else
    {
        ErrPrintf("Unrecognized ATS_READ_PERMISSION value %s for ALLOC_SURFACE command.\n", value.c_str());

        return RC::ILWALID_FILE_FORMAT;
    }

    return OK;
}

RC GpuTrace::SetSurfaceAtsWritePermission(MdiagSurf *surface, const string &value)
{
    if (value == "TRUE")
    {
        surface->SetAtsWritePermission(true);
    }
    else if (value == "FALSE")
    {
        surface->SetAtsWritePermission(false);
    }
    else
    {
        ErrPrintf("Unrecognized ATS_WRITE_PERMISSION value %s for ALLOC_SURFACE command.\n", value.c_str());

        return RC::ILWALID_FILE_FORMAT;
    }

    return OK;
}

RC GpuTrace::SetSurfaceAcr(MdiagSurf *surface, const string &value, UINT32 index)
{
    bool setting = false;

    if (value == "ON")
    {
        setting = true;
    }
    else if (value == "OFF")
    {
        setting = false;
    }
    else
    {
        ErrPrintf("Unrecognized ACR value %s.  Valid options are ON or OFF.\n", value.c_str());

        return RC::ILWALID_FILE_FORMAT;
    }

    switch (index)
    {
        case 1:
            surface->SetAcrRegion1(setting);
            break;
        case 2:
            surface->SetAcrRegion2(setting);
            break;
        default:
            ErrPrintf("Illegal ACR index %u\n", index);
            return RC::SOFTWARE_ERROR;
    }

    return OK;
}

RC GpuTrace::SetSurfaceFlaImportMem(MdiagSurf *surface, const string &name)
{
    RC rc;

    const auto flaExportSurface = LWGpuResource::GetFlaExportSurface(name);
    MASSERT(flaExportSurface != nullptr);

    CHECK_RC(surface->SetFlaImportMem(flaExportSurface->GetCtxDmaOffsetGpu(),
            flaExportSurface->GetSize(), flaExportSurface));

    if (flaExportSurface->GetGpuDev()->GetDeviceInst() ==
        m_Test->GetBoundGpuDevice()->GetDeviceInst())
    {
        surface->SetLoopBack(true);
    }
    else
    {
        surface->SetLoopBack(false);
    }

    return rc;
}

RC GpuTrace::SetSurfaceInheritPteKind(MdiagSurf *surface, const string &kind)
{
    if (kind == "VIRTUAL")
    {
        surface->SetInheritPteKind(Surface2D::InheritVirtual);
    }
    else if (kind == "PHYSICAL")
    {
        surface->SetInheritPteKind(Surface2D::InheritPhysical);
    }
    else
    {
        ErrPrintf("Unrecognized Inherit Pte Kind %s for INHERIT_PTE_KIND command.\n", kind.c_str());

        return RC::ILWALID_FILE_FORMAT;
    }
    
    return OK;
}
//---------------------------------------------------------------------------
RC GpuTrace::ProcessGlobalArguments(MdiagSurf *surface, SurfaceType surfaceType)
{
    RC rc = OK;

    if (params->ParamPresent("-page_size") > 0)
    {
        surface->SetPageSize(params->ParamUnsigned("-page_size",0));
    }

    if (params->ParamPresent("-gpu_cache_mode"))
    {
        if (params->ParamUnsigned("-gpu_cache_mode") > 0)
        {
            surface->SetGpuCacheMode(Surface2D::GpuCacheOn);
        }
        else
        {
            surface->SetGpuCacheMode(Surface2D::GpuCacheOff);
        }
    }

    if (params->ParamPresent("-zbc_mode"))
    {
        if (params->ParamUnsigned("-zbc_mode") > 0)
        {
            surface->SetZbcMode(Surface2D::ZbcOn);
        }
        else
        {
            surface->SetZbcMode(Surface2D::ZbcOff);
        }
    }

    // Alloc global -compress argument to override the trace setting.
    if (params->ParamPresent("-compress") > 0)
    {
        Compression compressMode =
            (Compression) params->ParamUnsigned("-compress_mode",
                COMPR_REQUIRED);

        switch (compressMode)
        {
            case COMPR_REQUIRED:
                surface->SetCompressed(true);
                surface->SetCompressedFlag(LWOS32_ATTR_COMPR_REQUIRED);
                break;

            case COMPR_ANY:
                surface->SetCompressed(true);
                surface->SetCompressedFlag(LWOS32_ATTR_COMPR_ANY);
                break;

            case COMPR_NONE:
                surface->SetCompressed(false);
                surface->SetCompressedFlag(LWOS32_ATTR_COMPR_NONE);
                break;

            default:
                ErrPrintf("Unrecognized value for command-line argument -compress_mode.\n");
                return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    // Zlwll is not supported on Amodel.
    if (Platform::GetSimulationMode() != Platform::Amodel)
    {
        // Option -zlwll/-slwll will override the value set in the trace
        if (IsSurfaceTypeColorZ3D(surfaceType) &&
            (surface->GetType() == LWOS32_TYPE_DEPTH) &&
            (params->ParamPresent("-zlwll") || params->ParamPresent("-slwll")))
        {
            switch((ZLwllEnum)params->ParamUnsigned("-zlwll_mode", ZLWLL_REQUIRED))
            {
                case ZLWLL_REQUIRED:
                    surface->SetZLwllFlag(LWOS32_ATTR_ZLWLL_REQUIRED);
                    break;
                case ZLWLL_ANY:
                    surface->SetZLwllFlag(LWOS32_ATTR_ZLWLL_ANY);
                    break;
                case ZLWLL_NONE:
                    surface->SetZLwllFlag(LWOS32_ATTR_ZLWLL_NONE);
                    break;
                case ZLWLL_CTXSW_SEPERATE_BUFFER:
                case ZLWLL_CTXSW_NOCTXSW:
                    // These modes are handled elsewhere.
                    break;
                default:
                    ErrPrintf("Unrecognized value for command-line argument -zlwll_mode.\n");
                    return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
        }
    }
    if (params->ParamPresent("-loc")  > 0)
    {
        _DMA_TARGET target = (_DMA_TARGET)params->ParamUnsigned("-loc", _DMA_TARGET_VIDEO);
        if (target == _DMA_TARGET_VIDEO)
        {
            surface->SetLocation(Memory::Fb);
        }
        else if (target == _DMA_TARGET_COHERENT)
        {
            surface->SetLocation(Memory::Coherent);
        }
        else if (target == _DMA_TARGET_NONCOHERENT)
        {
            surface->SetLocation(Memory::NonCoherent);
        }
    }

    if (params->ParamPresent("-va_reverse") > 0)
    {
        surface->SetVaReverse(true);
    }

    if (params->ParamPresent("-pa_reverse") > 0)
    {
        surface->SetPaReverse(true);
    }

    if (params->ParamPresent("-inherit_pte_kind") > 0)
    {
        SetSurfaceInheritPteKind(surface, params->ParamStr("-inherit_pte_kind"));
    }

    return rc;
}

//---------------------------------------------------------------------------
RC GpuTrace::GetAllocSurfaceParameters(MdiagSurf *surface, const string &name)
{
    RC rc = OK;
    UINT32 i;

    // Some of the older command-line arguments that are processed
    // for buffers created with the FILE command are also processed
    // for ALLOC_SURFACE buffers with special TAGs.
    ProcessArgumentAliases(surface, name);

    for (i = 0; i < params->ParamPresent("-as_width"); ++i)
    {
        if (params->ParamNStr("-as_width", i, 0) == name)
        {
            surface->SetWidth(params->ParamNUnsigned("-as_width", i, 1));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_height"); ++i)
    {
        if (params->ParamNStr("-as_height", i, 0) == name)
        {
            surface->SetHeight(params->ParamNUnsigned("-as_height", i, 1));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_depth"); ++i)
    {
        if (params->ParamNStr("-as_depth", i, 0) == name)
        {
            surface->SetDepth(params->ParamNUnsigned("-as_depth", i, 1));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_pitch"); ++i)
    {
        if (params->ParamNStr("-as_pitch", i, 0) == name)
        {
            surface->SetPitch(params->ParamNUnsigned("-as_pitch", i, 1));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_block_height"); ++i)
    {
        if (params->ParamNStr("-as_block_height", i, 0) == name)
        {
            surface->SetLogBlockHeight(params->ParamNUnsigned("-as_block_height", i, 1));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_block_width"); ++i)
    {
        if (params->ParamNStr("-as_block_width", i, 0) == name)
        {
            surface->SetLogBlockWidth(params->ParamNUnsigned("-as_block_width", i, 1));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_block_depth"); ++i)
    {
        if (params->ParamNStr("-as_block_depth", i, 0) == name)
        {
            surface->SetLogBlockDepth(params->ParamNUnsigned("-as_block_depth", i, 1));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_array_size"); ++i)
    {
        if (params->ParamNStr("-as_array_size", i, 0) == name)
        {
            surface->SetArraySize(params->ParamNUnsigned("-as_array_size", i, 1));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_phys_align"); ++i)
    {
        if (params->ParamNStr("-as_phys_align", i, 0) == name)
        {
            surface->SetAlignment(params->ParamNUnsigned("-as_phys_align", i, 1));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_virt_align"); ++i)
    {
        if (params->ParamNStr("-as_virt_align", i, 0) == name)
        {
            surface->SetVirtAlignment(params->ParamNUnsigned("-as_virt_align", i, 1));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_gpu_cache"); ++i)
    {
        if (params->ParamNStr("-as_gpu_cache", i, 0) == name)
        {
            CHECK_RC(SetSurfaceGpuCache(surface, params->ParamNStr("-as_gpu_cache", i, 1)));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_p2p_gpu_cache"); ++i)
    {
        if (params->ParamNStr("-as_p2p_gpu_cache", i, 0) == name)
        {
            CHECK_RC(SetSurfaceP2PGpuCache(surface, params->ParamNStr("-as_p2p_gpu_cache", i, 1)));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_zbc"); ++i)
    {
        if (params->ParamNStr("-as_zbc", i, 0) == name)
        {
            CHECK_RC(SetSurfaceZbc(surface, params->ParamNStr("-as_zbc", i, 1)));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_format"); ++i)
    {
        if (params->ParamNStr("-as_format", i, 0) == name)
        {
            ColorUtils::Format format = ColorUtils::StringToFormat(
                params->ParamNStr("-as_format", i, 1));

            if (format == ColorUtils::LWFMT_NONE)
            {
                ErrPrintf("-as_format value %s is not a valid color format.\n",
                          params->ParamNStr("-as_format", i, 1));

                return RC::ILWALID_FILE_FORMAT;
            }

            surface->SetColorFormat(format);
        }
    }

    for (i = 0; i < params->ParamPresent("-as_sample_mode"); ++i)
    {
        if (params->ParamNStr("-as_sample_mode", i, 0) == name)
        {
            CHECK_RC(SetSurfaceAAMode(surface, params->ParamNStr("-as_sample_mode", i, 1)));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_compression"); ++i)
    {
        if (params->ParamNStr("-as_compression", i, 0) == name)
        {
            CHECK_RC(SetSurfaceCompression(surface, params->ParamNStr("-as_compression", i, 1)));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_zlwll"); ++i)
    {
        if (params->ParamNStr("-as_zlwll", i, 0) == name)
        {
            CHECK_RC(SetSurfaceZlwll(surface, params->ParamNStr("-as_zlwll", i, 1)));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_aperture"); ++i)
    {
        if (params->ParamNStr("-as_aperture", i, 0) == name)
        {
            CHECK_RC(SetSurfaceAperture(surface, params->ParamNStr("-as_aperture", i, 1)));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_pte_kind"); ++i)
    {
        if (params->ParamNStr("-as_pte_kind", i, 0) == name)
        {
            const char *pteKindStr = params->ParamNStr("-as_pte_kind", i, 1);
            UINT32 pteKind;

            GpuSubdevice *subDev = m_Test->GetBoundGpuDevice()->GetSubdevice(0);

            if (!subDev->GetPteKindFromName(pteKindStr, &pteKind))
            {
                ErrPrintf("Invalid -as_pte_kind value: %s\n", pteKindStr);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }

            surface->SetPteKind(pteKind);
        }
    }

    for (i = 0; i < params->ParamPresent("-as_virt_address_range"); ++i)
    {
        if (params->ParamNStr("-as_virt_address_range", i, 0) == name)
        {
            UINT64 addressMin =
                params->ParamNUnsigned64("-as_virt_address_range", i, 1);
            UINT64 addressMax =
                params->ParamNUnsigned64("-as_virt_address_range", i, 2);

            if (addressMin > addressMax)
            {
                ErrPrintf("Invalid -as_virt_address_range: 0x%llx - 0x%llx.\n", addressMin, addressMax);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
            else
            {
                surface->SetVirtAddrRange(addressMin, addressMax);
            }
        }
    }

    for (i = 0; i < params->ParamPresent("-as_virt_address"); ++i)
    {
        if (params->ParamNStr("-as_virt_address", i, 0) == name)
        {
            if (0 == params->ParamNUnsigned64("-as_virt_address", i, 1))
            {
                ErrPrintf("0 is invalid for argument -as_virt_address.\n");
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
            surface->SetFixedVirtAddr(
                params->ParamNUnsigned64("-as_virt_address", i, 1));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_phys_address_range"); ++i)
    {
        if (params->ParamNStr("-as_phys_address_range", i, 0) == name)
        {
            UINT64 addressMin =
                params->ParamNUnsigned64("-as_phys_address_range", i, 1);
            UINT64 addressMax =
                params->ParamNUnsigned64("-as_phys_address_range", i, 2);

            if (addressMin > addressMax)
            {
                ErrPrintf("Invalid -as_phys_address_range: 0x%llx - 0x%llx.\n", addressMin, addressMax);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
            else
            {
                surface->SetPhysAddrRange(addressMin, addressMax);
            }
        }
    }

    for (i = 0; i < params->ParamPresent("-as_phys_address"); ++i)
    {
        if (params->ParamNStr("-as_phys_address", i, 0) == name)
        {
            surface->SetFixedPhysAddr(
                params->ParamNUnsigned64("-as_phys_address", i, 1));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_page_size"); ++i)
    {
        if (params->ParamNStr("-as_page_size", i, 0) == name)
        {
            CHECK_RC(SetSurfacePageSize(surface, params->ParamNStr("-as_page_size", i, 1)));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_phys_page_size"); ++i)
    {
        if (params->ParamNStr("-as_phys_page_size", i, 0) == name)
        {
            CHECK_RC(SetSurfacePhysPageSize(surface, params->ParamNStr("-as_phys_page_size", i, 1)));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_map_mode"); ++i)
    {
        if (params->ParamNStr("-as_map_mode", i, 0) == name)
        {
            CHECK_RC(SetSurfaceMapMode(surface, params->ParamNStr("-as_map_mode", i, 1)));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_privileged"); ++i)
    {
        if (params->ParamNStr("-as_privileged", i, 0) == name)
        {
            CHECK_RC(SetSurfacePrivileged(surface, params->ParamNStr("-as_privileged", i, 1)));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_fb_speed"); ++i)
    {
        if (params->ParamNStr("-as_fb_speed", i, 0) == name)
        {
            CHECK_RC(SetSurfaceFbSpeed(surface, params->ParamNStr("-as_fb_speed", i, 1)));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_contig"); ++i)
    {
        if (params->ParamNStr("-as_contig", i, 0) == name)
        {
            surface->SetPhysContig(true);
        }
    }

    for (i = 0; i < params->ParamPresent("-as_vpr"); ++i)
    {
        if (params->ParamNStr("-as_vpr", i, 0) == name)
        {
            CHECK_RC(SetSurfaceVpr(surface, params->ParamNStr("-as_vpr", i, 1)));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_upr"); ++i)
    {
        if (params->ParamNStr("-as_upr", i, 0) == name)
        {
            CHECK_RC(SetSurfaceUpr(surface, params->ParamNStr("-as_upr", i, 1)));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_start_offset"); ++i)
    {
        if (params->ParamNStr("-as_start_offset", i, 0) == name)
        {
            surface->SetExtraAllocSize(
                params->ParamNUnsigned64("-as_start_offset", i, 1));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_start_adjust"); ++i)
    {
        if (params->ParamNStr("-as_start_adjust", i, 0) == name)
        {
            surface->SetHiddenAllocSize(
                params->ParamNUnsigned64("-as_start_adjust", i, 1));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_ats_map"); ++i)
    {
        if (params->ParamNStr("-as_ats_map", i, 0) == name)
        {
            surface->SetAtsMapped();
        }
    }

    for (i = 0; i < params->ParamPresent("-as_no_gmmu_map"); ++i)
    {
        if (params->ParamNStr("-as_no_gmmu_map", i, 0) == name)
        {
            surface->SetNoGmmuMap();
        }
    }

    for (i = 0; i < params->ParamPresent("-as_ats_page_size"); ++i)
    {
        if (params->ParamNStr("-as_ats_page_size", i, 0) == name)
        {
            surface->SetAtsPageSize(
                params->ParamNUnsigned("-as_ats_page_size", i, 1));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_ats_read_permission"); ++i)
    {
        if (params->ParamNStr("-as_ats_read_permission", i, 0) == name)
        {
            CHECK_RC(SetSurfaceAtsReadPermission(surface,
                params->ParamNStr("-as_ats_read_permission", i, 1)));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_ats_write_permission"); ++i)
    {
        if (params->ParamNStr("-as_ats_write_permission", i, 0) == name)
        {
            CHECK_RC(SetSurfaceAtsWritePermission(surface,
                params->ParamNStr("-as_ats_write_permission", i, 1)));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_acr1"); ++i)
    {
        if (params->ParamNStr("-as_acr1", i, 0) == name)
        {
            CHECK_RC(SetSurfaceAcr(surface, params->ParamNStr("-as_acr1", i, 1),
                1));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_acr2"); ++i)
    {
        if (params->ParamNStr("-as_acr2", i, 0) == name)
        {
            CHECK_RC(SetSurfaceAcr(surface, params->ParamNStr("-as_acr2", i, 1),
                2));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_map_as_peer"); ++i)
    {
        if (params->ParamNStr("-as_map_as_peer", i, 0) == name)
        {
            if (surface->GetLocation() == Memory::Fb)
            {
                surface->SetNeedsPeerMapping(true);

                if (params->ParamPresent("-sli_p2ploopback"))
                {
                    surface->SetLoopBack(true);
                }
            }
            else
            {
                ErrPrintf("-as_map_as_peer is set for surface %s but the surface isn't in video memory.\n", name.c_str());
                return RC::ILWALID_ARGUMENT;
            }
        }
    }

    // No RM support for FLA on Amodel
    if (Platform::GetSimulationMode() != Platform::Amodel)
    {
        for (i = 0; i < params->ParamPresent("-as_fla_map"); ++i)
        {
            if (params->ParamNStr("-as_fla_map", i, 0) == name)
            {
                if (!(surface->GetLocation() == Memory::Fb &&
                        surface->GetNeedsPeerMapping()))
                {
                    ErrPrintf("-as_fla_map is set for surface %s but the surface isn't in peer memory.\n", name.c_str());
                    return RC::ILWALID_ARGUMENT;
                }

                CHECK_RC(SetSurfaceFlaImportMem(surface, params->ParamNStr("-as_fla_map", i, 1)));
            }
        }
    }

    // This argument needs to be processed after any argument that might set
    // peer, fla, or loopback on this surface.
    for (i = 0; i < params->ParamPresent("-as_peer_gpu"); ++i)
    {
        if (params->ParamNStr("-as_peer_gpu", i, 0) == name)
        {
            if (surface->GetLoopBack())
            {
                ErrPrintf("-as_peer_gpu is set for surface %s but loopback mode is already specified.\n",
                          name.c_str());
                return RC::ILWALID_ARGUMENT;
            }

            if (surface->HasFlaImportMem())
            {
                ErrPrintf("-as_peer_gpu is set for surface %s but the surface is in FLA memory. "
                          "The GPU of FLA memory is determined by -fla_export.\n", name.c_str());
                return RC::ILWALID_ARGUMENT;
            }

            if (!(surface->GetLocation() == Memory::Fb &&
                    surface->GetNeedsPeerMapping()))
            {
                ErrPrintf("-as_peer_gpu is set for surface %s but the surface isn't in peer memory.\n", name.c_str());
                return RC::ILWALID_ARGUMENT;
            }

            auto gpuInst = params->ParamNUnsigned("-as_peer_gpu", i, 1);
            auto* pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
            Device* pDevice = nullptr;
            CHECK_RC(pGpuDevMgr->GetDevice(gpuInst, &pDevice));
            auto* pGpuDevice = static_cast<GpuDevice*>(pDevice);

            if (pGpuDevice == nullptr)
            {
                ErrPrintf("-as_peer_gpu is set for surface %s but gpu device %d is not found.\n", name.c_str(), gpuInst);
                return RC::ILWALID_ARGUMENT;
            }

            if (pGpuDevice->GetDeviceInst() ==
                m_Test->GetBoundGpuDevice()->GetDeviceInst())
            {
                surface->SetLoopBack(true);
            }
            else
            {
                surface->SetLoopBack(false);
                surface->SetSurfaceAsRemote(pGpuDevice);
            }
        }
    }

    // This argument needs to be processed after any argument that might
    // call SetLoopBack() on this surface.
    for (i = 0; i < params->ParamPresent("-as_loopback_peer_id"); ++i)
    {
        if (params->ParamNStr("-as_loopback_peer_id", i, 0) == name)
        {
            if (surface->GetLoopBack())
            {
                surface->SetLoopBackPeerId(
                    params->ParamNUnsigned("-as_loopback_peer_id", i, 1));
            }
            else
            {
                ErrPrintf("-as_loopback_peer_id set for surface %s but the surface isn't in loopback mode.\n", name.c_str());
                return RC::ILWALID_ARGUMENT;
            }
        }
    }

    for (i = 0; i < params->ParamPresent("-as_layout"); ++i)
    {
        if (params->ParamNStr("-as_layout", i, 0) == name)
        {
            CHECK_RC(SetSurfaceLayout(surface, params->ParamNStr("-as_layout", i, 1)));
        }
    }

    for (i = 0; i < params->ParamPresent("-as_egm_alloc"); ++i)
    {
        if (params->ParamNStr("-as_egm_alloc", i, 0) == name)
        {
            if (Platform::IsSelfHosted())
            {   
                if (!surface->IsOnSysmem())
                {
                    ErrPrintf("%s is not under the sysmem for SHH. "
                            "Please add -as_aperture to specify it\n", name.c_str());
                    return RC::SOFTWARE_ERROR;
                }
            } 
            else
            {
                surface->SetLocation(Memory::Fb);
            }
            surface->SetEgmAttr();
        }
    }

    return rc;
}

//---------------------------------------------------------------------------
// For Kepler, compute traces have colwerted from using FILE commands to
// ALLOC_SURFACE commands for some of the buffers in the trace header.
// However, the command-line arguments for those buffers have not yet been
// changed to the -as_* equivalents.  This function is a temporary hack
// to process the old arguments based on specific TAGs in an ALLOC_SURFACE
// declaration.
//
RC GpuTrace::ProcessArgumentAliases(MdiagSurf *surface, const string &name)
{
    RC rc = OK;

    // Create the TAG to buffer type map if it has not already been created.
    if (m_TagToBufferTypeMap.size() == 0)
    {
        InitTagToBufferTypeMap();
    }

    TagToBufferTypeMap::iterator iter;

    iter = m_TagToBufferTypeMap.find(name);

    // If the given name matches one of the map items,
    // get the buffer type and process the command-line arguments
    // for that buffer type.
    if (iter != m_TagToBufferTypeMap.end())
    {
        const char *bufferType = (*iter).second.c_str();
        string pteKindName;
        bool needsPeerMapping = false;

        ParseDmaBufferArgs(*surface, params, bufferType, &pteKindName,
            &needsPeerMapping);

        if (needsPeerMapping)
        {
            surface->SetNeedsPeerMapping(true);

            if (params->ParamPresent("-sli_p2ploopback"))
            {
                surface->SetLoopBack(true);
            }
        }

        SetPteKind(*surface, pteKindName, m_Test->GetBoundGpuDevice());
    }

    return rc;
}

//---------------------------------------------------------------------------
// Create a map of TAGs to buffer types.  This is used for processing old
// command-line arguments on ALLOC_SURFACE buffers with special TAGs.
//
void GpuTrace::InitTagToBufferTypeMap()
{
    m_TagToBufferTypeMap["CONSTANT"] = "const";
    m_TagToBufferTypeMap["SEMAPHORE"] = "sem";
    m_TagToBufferTypeMap["VERTEX_BUFFER"] = "vtx";
    m_TagToBufferTypeMap["INDEX_BUFFER"] = "idx";
    m_TagToBufferTypeMap["TEXTURE"] = "tex";
    m_TagToBufferTypeMap["PROGRAM"] = "pgm";
    m_TagToBufferTypeMap["TEXTURE_HEADER"] = "header";
    m_TagToBufferTypeMap["TEXTURE_SAMPLER"] = "sampler";
    m_TagToBufferTypeMap["NOTIFIER"] = "notifier";
}

RC GpuTrace::GetMemoryCommandFileSize(
    const MemoryCommandProperties &properties, PropertyUsageMap *propertyUsed,
    size_t *fileSize)
{
    RC rc = OK;
    const string &fileName = properties.file;

    if ((*propertyUsed)["SIZE"])
    {
        *fileSize = properties.size;
    }
    else if ((*propertyUsed)["FILE"])
    {
        if (OK != (rc = m_TraceFileMgr->Find(fileName, fileSize)))
        {
            ErrPrintf("FILE: couldn't open \"%s\"\n", fileName.c_str());
            return rc;
        }
    }
    else if ((*propertyUsed)["PHYSICAL_ALLOC"] && !(*propertyUsed)["CREATE_PHYSICAL_ALLOC"])
    {
        TraceModule *physModule = ModFind(
            properties.physicalAllocName.c_str());

        if (physModule == 0)
        {
            ErrPrintf("No ALLOC_PHYSICAL command with name \'%s\' exists.\n",
                      properties.physicalAllocName.c_str());

            return RC::ILWALID_FILE_FORMAT;
        }

        *fileSize = physModule->GetSize();
    }
    else
    {
        *fileSize = 0;
    }

    return rc;
}

RC GpuTrace::SetMemoryModuleProperties(SurfaceTraceModule *module,
    MdiagSurf *surface, SurfaceType surfaceType,
    const MemoryCommandProperties &properties, PropertyUsageMap *propertyUsed)
{
    RC rc = OK;

    module->SetSwapInfo(properties.swapSize);

    if ((*propertyUsed)["RAW"] && properties.raw)
    {
        module->SetBufferRaw();
    }

    if ((*propertyUsed)["CRC_RECT"])
    {
        CrcRect rect;
        UINT32 count = sscanf(properties.crcRect.c_str(),
            "%d:%d:%d:%d", &rect.x, &rect.y, &rect.width, &rect.height);
        if (count != 4)
        {
            ErrPrintf("Wrong format for CRC_RECT, expected: x:y:w:h\n");
            return RC::ILWALID_FILE_FORMAT;
        }
        MASSERT(rect.width > 0 && rect.height > 0);

        module->SetCrcRect(rect);
    }

    // ALLOC_SURFACE buffers created with the names COLOR0...COLOR7 or Z
    // are special and are handled by primarily by the surface manager.
    // A trace module will still be created for these buffers to take
    // advantage of trace module functionality like loading from a file,
    // but the surface manager is considered their owner.
    if (IsSurfaceTypeColorZ3D(surfaceType))
    {
        // Earlier code will give an error if two surfaces with the same
        // name are used.
        MASSERT(m_SurfaceTypeMap.find(surfaceType) == m_SurfaceTypeMap.end());

        m_SurfaceTypeMap[surfaceType].surface = *surface;

        module->SetIsColorOrZ();

        if ((*propertyUsed)["LOOPBACK"])
        {
            m_SurfaceTypeMap[surfaceType].bMapLoopback = properties.loopback;
        }

        if ((*propertyUsed)["SIZE"])
        {
            ErrPrintf("The ALLOC_SURFACE SIZE property may not be used with %s surfaces.\n",
                GetTypeName(surfaceType));

            return RC::ILWALID_FILE_FORMAT;
        }
    }

    bool rawCrc = (*propertyUsed)["RAW_CRC"] && properties.rawCrc;

    bool skipCRCCheck = false;
    if (properties.crcCheckCount)
    {
        for (vector<string>::const_iterator tagIter = properties.tagList.begin();
            tagIter != properties.tagList.end();
            ++tagIter)
        {
            for (UINT32 i = 0; i < params->ParamPresent("-skip_crc_check"); ++i)
            {
                if (params->ParamNStr("-skip_crc_check", i, 0) == *tagIter)
                {
                    InfoPrintf("'-skip_crc_check %s' specified skipping crc check for surface\n",
                               tagIter->c_str());
                    skipCRCCheck = true;
                }
            }
        }
    }

    if (!skipCRCCheck)
    {
        if (properties.crcCheckCount > 1)
        {
            ErrPrintf("Only one of CRC_CHECK, REFERENCE_CHECK, CRC_CHECK_NO_MATCH "
                      "and REFERENCE_CHECK_NO_MATCH can be used with a single ALLOC_SURFACE.\n");
    
            return RC::ILWALID_FILE_FORMAT;
        }
        else if ((*propertyUsed)["CRC_CHECK"] ||
            (*propertyUsed)["CRC_CHECK_NO_MATCH"])
        {
            TraceModule::ModCheckInfo checkInfo;
            checkInfo.CheckFilename = properties.crcCheck;
            checkInfo.Size = properties.size;
            checkInfo.pTraceCh = module->GetTraceChannel();
            checkInfo.CheckMethod = CRC_CHECK;
            checkInfo.isRawCRC = rawCrc;
            module->SetCheckInfo(checkInfo);
        }
        else if ((*propertyUsed)["REFERENCE_CHECK"] ||
            (*propertyUsed)["REFERENCE_CHECK_NO_MATCH"])
        {
            TraceModule::ModCheckInfo checkInfo;
            checkInfo.CheckFilename = properties.referenceCheck;
            checkInfo.Size = properties.size;
            checkInfo.pTraceCh = module->GetTraceChannel();
            checkInfo.CheckMethod = REF_CHECK;
            checkInfo.isRawCRC = rawCrc;
            module->SetCheckInfo(checkInfo);
    
            m_HasRefCheckModules = true;
        }
    
        if (properties.ilwertCrcCheck)
        {
            module->SetCrcCheckMatch(false);
        }
    }

    if ((*propertyUsed)["LOOPBACK"] && properties.loopback)
    {
        module->SetLoopback(properties.loopback);
    }

    // The attribute and type override values have already been set
    // on the surface, but they should still be set on the trace module
    // so that a peer trace module can access them.

    if ((*propertyUsed)["ATTR_OVERRIDE"])
    {
        module->SetAttrOverride(properties.attrOverride);
    }

    if ((*propertyUsed)["ATTR2_OVERRIDE"])
    {
        module->SetAttr2Override(properties.attr2Override);
    }

    if ((*propertyUsed)["TYPE_OVERRIDE"])
    {
        module->SetTypeOverride(properties.typeOverride);
    }

    if ((*propertyUsed)["ADDRESS_OFFSET"])
    {
        module->SetOffsetWithinDmaBuf(properties.addressOffset);
    }

    if ((*propertyUsed)["CLASS"])
    {
        module->SetClass(properties.classNum);
    }

    // The dynamic version of ALLOC_SURFACE only exists in traces
    // with version 5 or above.
    if ((m_Version >= 5) &&
        (params->ParamPresent("-no_dynamic_allocation") == 0))
    {
        m_HasDynamicModules = true;

        module->SetIsDynamic();

        TraceOpAllocSurface *traceOp;

        traceOp = new TraceOpAllocSurface(module, m_Test, this,
            m_TraceFileMgr);

        if (Utl::HasInstance())
        {
            traceOp->SetHdrStrProperties(properties.hdrStrProps);
        }
        InsertOp(m_LineNumber, traceOp, OP_SEQUENCE_METHODS);
    }

    if ((*propertyUsed)["FILL8"])
    {
        UINT32 val = UINT32(properties.fill8) | UINT32(properties.fill8) << 8 | UINT32(properties.fill8) << 16 | UINT32(properties.fill8) << 24;
        module->SetFillValue(val);
    }

    if ((*propertyUsed)["FILL32"])
    {
        module->SetFillValue(properties.fill32);
    }

    if ((*propertyUsed)["ACCESS"])
    {
        string protect = properties.access;
        if (protect == "READ_ONLY")
        {
            module->SetProtect(Memory::Readable);
        }
        else if (protect == "WRITE_ONLY")
        {
            module->SetProtect(Memory::Writeable);
        }
        else if (protect == "READ_WRITE")
        {
            module->SetProtect(Memory::ReadWrite);
        }
        else
        {
            ErrPrintf("Unrecognized ACCESS %s.\n", protect.c_str());
            return RC::ILWALID_FILE_FORMAT;
        }
    }

    if ((*propertyUsed)["MAP_TO_BACKINGSTORE"])
    {
        if (properties.mapToBackingStore)
            module->SetMapToBackingStore();
    }

    if ((*propertyUsed)["SCALE_BY_SM_COUNT"])
    {
        module->ScaleByTPCCount(properties.smCount);
    }

    if ((*propertyUsed)["TEXTURE_HEADER"])
    {
        module->SetIsTextureHeader(true);
    }

    if (surface->HasVirtual())
    {
        LwRm::Handle hVASpace;
        if ((*propertyUsed)["ADDRESS_SPACE"])
        {
            hVASpace = GetVASHandle(properties.vasName);
        }
        else if (module->GetParentModule())
        {
            // surface_view inherit parent's VAS
            hVASpace = module->GetParentModule()->GetVASpaceHandle();
        }
        else
        {
            hVASpace = GetVASHandle("default");
        }
        module->SetVASpaceHandle(hVASpace);

        TraceChannel* trCh = 0;
        if ((*propertyUsed)["CHANNEL"])
        {
            trCh = GetChannel(properties.traceChannel, CPU_MANAGED);
            if (trCh == 0)
            {
                ErrPrintf("Invalid channel_name %s\n", properties.traceChannel.c_str());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else
        {
            // The channel associated to surface is not specified,
            // use following rule to find one:
            // 1. use its parent module's channel
            // 2. use non-gpu managed channel
            if (module->GetParentModule())
            {
                trCh = module->GetParentModule()->GetTraceChannel();
            }
            else
            {
                // use channel declared in test.hdr most recently
                trCh = GetLastChannelByVASpace(module->GetVASpaceHandle());
                if (trCh == NULL)
                {
                    WarnPrintf("cannot find a channel with same VA for module %s\n", module->GetName().c_str());
                }
                else if(trCh->IsGpuManagedChannel())
                {
                    // use non-gpu managed channel in same tsg
                    TraceTsg* trTsg = trCh->GetTraceTsg();
                    trCh = trTsg->GetCpuMngTraceChannel();
                }
            }
        }

        CHECK_RC(module->SetTraceChannel(trCh, surfaceType == SURFACE_TYPE_PUSHBUFFER));
    }

    vector<string>::const_iterator tagIter;

    for (tagIter = properties.tagList.begin();
         tagIter != properties.tagList.end();
         ++tagIter)
    {
        module->AddTag(*tagIter);

        for (UINT32 i = 0; i < params->ParamPresent("-as_skip_init"); ++i)
        {
            if (params->ParamNStr("-as_skip_init", i, 0) == *tagIter)
            {
                module->SetSkipInit(true);
            }
        }
    }

    m_Modules.push_back(module);
    m_GenericModules.push_back(module);

    return rc;
}

RC GpuTrace::ParseFREE_SURFACE()
{
    RC rc = OK;

    CHECK_RC(RequireVersion(4, "FREE_SURFACE"));

    TraceModule *module;

    if (OK == ParseModule(&module))
    {
        TraceModule::ModCheckInfo checkInfo;
        if (!module->GetCheckInfo(checkInfo))
        {
            ErrPrintf("%s: No CRC checkInfo attached to the surface %s.\n",
                      __FUNCTION__, module->GetName().c_str());
            return RC::SOFTWARE_ERROR;
        }
        bool dumpBeforeFree = (params->ParamPresent("-dump_all_buffer_after_test") && // option -dump_all_buffer_after_test is specified
                               (module->GetProtect() & Memory::Writeable) && // writable
                               (NO_CHECK == checkInfo.CheckMethod)); // has no check method
        TraceOp *traceOp = new TraceOpFreeSurface(module, m_Test, dumpBeforeFree);
        InsertOp(m_LineNumber, traceOp, OP_SEQUENCE_METHODS);
    }
    else
    {
        ErrPrintf("FREE_SURFACE used on unrecognized FILE or ALLOC_SURFACE \"%s\".\n",
                  m_TestHdrTokens.front());

        return RC::ILWALID_FILE_FORMAT;
    }

    return rc;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseInlineFileData(GenericTraceModule * pTraceModule)
{
    // We expect a newline at the end of the INLINE_FILE <args> statement.

    const char * tok = m_TestHdrTokens.front();
    if (ParseFile::s_NewLine != tok)
    {
        ErrPrintf("Unexpected token \"%s\" at the end of line %d.\n",
                  tok, m_LineNumber);
        return RC::ILWALID_FILE_FORMAT;
    }
    m_LineNumber++;
    m_TestHdrTokens.pop_front();

    // Now, we expect to see 1+ UINT32 values over 1+ lines, follwed by
    // INLINE_FILE_END on a line by itself.

    vector<UINT32> data;

    while (m_TestHdrTokens.size())
    {
        UINT32 i;
        if (OK == ParseUINT32(&i))
        {
            data.push_back(i);
            continue;
        }

        tok = m_TestHdrTokens.front();
        m_TestHdrTokens.pop_front();

        if (ParseFile::s_NewLine == tok)
        {
            m_LineNumber++;
        }
        else if (0 == strcmp(tok, "INLINE_FILE_END"))
        {
            break;
        }
        else
        {
            ErrPrintf("Unexpected token \"%s\" in INLINE_FILE data at line %d.\n",
                      tok, m_LineNumber);
            return RC::ILWALID_FILE_FORMAT;
        }
    }
    size_t datasize = data.size() * sizeof(UINT32);

    DebugPrintf(MSGID(TraceParser), "Read inline data for \"%s\", size = 0x%x\n",
            pTraceModule->GetName().c_str(), datasize);

    pTraceModule->StoreInlineData(&data[0], datasize);
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseCLEAR()
{
    const char * tok = m_TestHdrTokens.front();
    if (ParseFile::s_NewLine == tok)
    {
        ErrPrintf("CLEAR requires <type> argument.\n");
        return RC::ILWALID_FILE_FORMAT;
    }
    m_TestHdrTokens.pop_front();

    if (0 == strcmp(tok, "RGBA"))
    {
        if (m_ColorClear.IsSet())
        {
            ErrPrintf("Only one CLEAR or CLEAR_FLOAT for color is allowed.\n");
            return RC::ILWALID_FILE_FORMAT;
        }
        m_ColorClear.Set();

        UINT32 rgba;
        if (OK != ParseUINT32(&rgba))
        {
            ErrPrintf("CLEAR RGBA requires a <uint32> arg.\n");
            return RC::ILWALID_FILE_FORMAT;
        }

        UINT32 color[4];
        color[0] =  rgba & 0xff;             //red
        color[1] = (rgba & 0xff00) >> 8;     //green
        color[2] = (rgba & 0xff0000) >> 16;  //blue
        color[3] = (rgba & 0xff000000) >> 24;//alpha

        m_ColorClear = RGBAFloat(color);
    }
    else if (0 == strcmp(tok, "ZS"))
    {
        if (m_ZetaClear.IsSet() || m_StencilClear.IsSet())
        {
            ErrPrintf("Only one CLEAR or CLEAR_FLOAT for Z or S is allowed.\n");
            return RC::ILWALID_FILE_FORMAT;
        }
        m_ZetaClear.Set();
        m_StencilClear.Set();

        UINT32 zs;
        if (OK != ParseUINT32(&zs))
        {
            ErrPrintf("CLEAR ZS requires a <uint32> arg.\n");
            return RC::ILWALID_FILE_FORMAT;
        }
        m_StencilClear = Stencil(zs & 0xff);
        m_ZetaClear = ZFloat(zs >> 8);
    }
    else if (0 == strcmp(tok, "STENCIL"))
    {
        if (m_StencilClear.IsSet())
        {
            ErrPrintf("Only one CLEAR or CLEAR_FLOAT for S is allowed.\n");
            return RC::ILWALID_FILE_FORMAT;
        }
        m_StencilClear.Set();

        UINT32 s;
        if (OK != ParseUINT32(&s))
        {
            ErrPrintf("CLEAR STENCIL requires a <uint32> arg.\n");
            return RC::ILWALID_FILE_FORMAT;
        }
        m_StencilClear = Stencil(s & 0xff);
    }
    else
    {
        ErrPrintf("CLEAR \"%s\" invalid clear type.\n", tok);
        return RC::ILWALID_FILE_FORMAT;
    }
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseCLEAR_INIT()
{
    TraceModule * module;
    if (OK != ParseModule(&module))
    {
        ErrPrintf("Unrecognized buffer %s\n", m_TestHdrTokens.front());
        return RC::ILWALID_FILE_FORMAT;
    }

    if (module->GetFileType() != FT_PUSHBUFFER)
    {
        ErrPrintf("Error in CLEAR_INIT: %s is expected to be a push buffer\n",
                  module->GetName().c_str());
        return RC::ILWALID_FILE_FORMAT;
    }

    UINT32 offset;
    if (OK != ParseUINT32(&offset))
    {
        ErrPrintf("CLEAR_INIT requires <filename> <offset>\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    // check if vcaa options enabled on amodel, gpu_clear must be kept, bug 373583
    bool amod_vcaa_flag = false;
    if (params->ParamPresent("-aamode") &&
        Platform::GetSimulationMode() == Platform::Amodel)
    {
        UINT32 argVal = params->ParamUnsigned("-aamode");
        if (argVal >= 7 && argVal <=10)
            amod_vcaa_flag = true;
    }

    if (m_Version >= 5)
    {
        WarnPrintf("CLEAR_INIT commands are ignored for trace version 5 or higher.\n");
    }
    else if (!amod_vcaa_flag)
    {
        module->AddClearInitMethod(offset);
    }

    // set clear_init flag for later trace_3d use
    SetClearInit(true);

    return OK;
}

//---------------------------------------------------------------------------
// CLEAR_FLOAT and CLEAR_INT are the same thing!
//
RC GpuTrace::ParseCLEAR_FLOAT_INT(const char * cmd)
{
    const char * tok = m_TestHdrTokens.front();
    if (ParseFile::s_NewLine == tok)
    {
        ErrPrintf("%s requires <type> argument.\n", cmd);
        return RC::ILWALID_FILE_FORMAT;
    }
    m_TestHdrTokens.pop_front();

    if (0 == strcmp(tok, "RGBA"))
    {
        if (m_ColorClear.IsSet())
        {
            ErrPrintf("Only one CLEAR or CLEAR_FLOAT for color is allowed.\n");
            return RC::ILWALID_FILE_FORMAT;
        }
        m_ColorClear.Set();

        UINT32 rgba[4];
        if ((OK != ParseUINT32(&rgba[0])) ||
            (OK != ParseUINT32(&rgba[1])) ||
            (OK != ParseUINT32(&rgba[2])) ||
            (OK != ParseUINT32(&rgba[3])))
        {
            ErrPrintf("%s RGBA requires four <uint32> args.\n", cmd);
            return RC::ILWALID_FILE_FORMAT;
        }

        m_ColorClear = RGBAFloat(rgba);
    }
    else if (0 == strcmp(tok, "Z"))
    {
        if (m_ZetaClear.IsSet())
        {
            ErrPrintf("Only one CLEAR or CLEAR_FLOAT for Z is allowed.\n");
            return RC::ILWALID_FILE_FORMAT;
        }
        m_ZetaClear.Set();

        UINT32 z;
        if (OK != ParseUINT32(&z))
        {
            ErrPrintf("%s Z requires a <uint32> arg.\n", cmd);
            return RC::ILWALID_FILE_FORMAT;
        }
        m_ZetaClear = ZFloat(z);
    }
    else
    {
        ErrPrintf("%s \"%s\" invalid clear type.\n", cmd, tok);
        return RC::ILWALID_FILE_FORMAT;
    }
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseRELOC()
{
    // Format of RELOC command is:
    // RELOC <offset> <file_name>|<file_type>|COLOR0|COLOR1...COLOR{MAX_RENDER_TARGETS-1}|Z|CLIPID
    //  if last argument is <file_name>, then a module last mentioned by FILE command
    //     will be patched with offset of <file_name>
    //  if last argument is <file_type>, then a module last mentioned by FILE command
    //     will be patched with offset of the area that contains files of the type
    //  if last argument is COLOR0|COLOR1...COLOR{MAX_RENDER_TARGETS-1}|Z, then a module
    //     last mentioned by FILE command will be patched with offset of that buffer

    RC rc;
    CHECK_RC(RequireModLast("RELOC"));
    CHECK_RC(RequireModLocal(ModLast(), "RELOC"));

    UINT32 offset;
    string surfname;
    UINT32 peerNum = Gpu::MaxNumSubdevices;
    UINT32 peerID = USE_DEFAULT_RM_MAPPING;
    if ((OK != ParseUINT32(&offset)) ||
        (OK != ParseFileOrSurfaceOrFileType(&surfname)) ||
        (OK != ParseRelocPeer(surfname, &peerNum, &peerID)))
    {
        ErrPrintf("RELOC requires <offset> <FILE/surface/filetype> [PEER <peer num> [PEERID <id>]] arguments.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    TraceModule *module = ModFind(surfname.c_str());
    if (module && m_Test->GetSSM()->IsSliSystem(module->GetGpuDev()))
    {
        // Build up a relocation dependency relationship for SLI
        module->AddRelocTarget(ModLast());
    }

    return ModLast()->RelocAdd(new TraceRelocationOffset(offset,
                                                         this,
                                                         surfname,
                                                         ModLast()->GetSubdevMaskDataMap(),
                                                         peerNum,
                                                         peerID));
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseRELOC40()
{
    //Format of RELOC40 command is:
    //RELOC40 <offset> <file_name>|<file_type>|COLOR0|COLOR1...COLOR{MAX_RENDER_TARGETS-1}|Z
    // if last argument is <file_name>, then a module last mentioned by FILE
    //  command will be patched with offset of <file_name>
    // if last argument is <file_type>, then a module last mentioned by FILE
    //  command will be patched with offset of the area that contains files
    //  of the tipe
    // if last argument is COLOR0|COLOR1...COLOR{MAX_RENDER_TARGETS-1}|Z,
    //  then a module last mentioned by FILE command will be patched with
    //  offset of that buffer

    RC rc;
    CHECK_RC(RequireModLast("RELOC40"));
    CHECK_RC(RequireModLocal(ModLast(), "RELOC40"));

    UINT32 offset;
    if (OK != ParseUINT32(&offset))
    {
        ErrPrintf("RELOC40 requires <offset> argument.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    UINT32 peerNum = Gpu::MaxNumSubdevices;
    UINT32 peerID = USE_DEFAULT_RM_MAPPING;

    SurfaceType surf;
    TraceModule * module;
    if (OK == ParseSurfType(&surf))
    {
        CHECK_RC(ParseRelocPeer(surf, &peerNum));
        CHECK_RC(ModLast()->RelocAdd(new TraceRelocationSurface(
                                                 offset, surf, 0, true,
                                                 ModLast()->GetSubdevMaskDataMap(),
                                                 peerNum, 0xffffffffffffffffull,
                                                 0xffffffffffffffffull)));
    }
    else if (OK == ParseModule(&module))
    {
        CHECK_RC(ParseRelocPeer(module->GetName(), &peerNum, &peerID));
        CHECK_RC(ModLast()->RelocAdd(new TraceRelocation40BitsSwapped(
                                               offset, module,
                                               ModLast()->GetSubdevMaskDataMap(),
                                               _DMA_TARGET_P2P ==
                                               (_DMA_TARGET)params->ParamUnsigned("-loc_tex"),
                                               peerNum)));
    }
    else
    {
        ErrPrintf("RELOC40 0x%x \"%s\" unrecognized FILE/surface.\n", offset, m_TestHdrTokens.front());
        return RC::ILWALID_FILE_FORMAT;
    }
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseRELOC_D()
{
    //relocateion for a dynamically generated texture
    //format is:
    //   RELOC_D idx [COLOR0|COLOR1...COLOR{MAX_RENDER_TARGETS-1}|Z|VCAA] [NO_OFFSET] [NO_SURF_ATTR]
    //
    //   idx - index of DynamicTexture  header in a header file
    //   [COLOR0|COLOR1...|Z|VCAA] is surface used to generate DynamicTexture

    RC rc;
    CHECK_RC(RequireModLast("RELOC_D"));
    CHECK_RC(RequireModLocal(ModLast(), "RELOC_D"));

    UINT32 offset;
    if (OK != ParseUINT32(&offset))
    {
        ErrPrintf("RELOC_D requires an <index of dynamic texture> argument.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    string s;
    if (OK != ParseString(&s))
    {
        ErrPrintf("RELOC_D requires a surface argument (COLORn|Z|VCAA|TEX).\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    SurfaceType surf;
    TextureMode Mode = TEXTURE_MODE_NORMAL;

    if (0 == strcmp(s.c_str(), "VCAA"))
    {
        surf = SURFACE_TYPE_Z;
        Mode = TEXTURE_MODE_VCAA;
    }
    else if (0 == strcmp(s.c_str(), "STENCIL"))
    {
        surf = SURFACE_TYPE_Z;
        Mode = TEXTURE_MODE_STENCIL;
    }
    else
    {
        surf = FindSurface(s.c_str());
        if (surf == SURFACE_TYPE_UNKNOWN)
        {
            TraceModule *mod = ModFind(s.c_str());
            if (mod && mod->GetFileType() == FT_ALLOC_SURFACE)
            {
                // Bind the texture to index
                mod->AddTextureIndex(offset);
            }
            else
            {
                ErrPrintf("Trace: invalid surface '%s' in RELOC_D command\n", s.c_str());
                ErrPrintf("       Allowed values are: any surface name, or VCAA\n");
                return RC::ILWALID_FILE_FORMAT;
            }
        }
    }

    bool no_offset = false;
    bool no_surf_attr = false;
    UINT32 peer = Gpu::MaxNumSubdevices;
    string tex_header;
    bool bOptTarget = false;

    while (m_TestHdrTokens.front() != ParseFile::s_NewLine)
    {
        const char *tok = m_TestHdrTokens.front();
        m_TestHdrTokens.pop_front();

        if (0 == strcmp(tok, "PEER"))
        {
            if ((m_SurfaceTypeMap.count(surf) == 0) ||
                (OK != ParseUINT32(&peer)))
            {
                return RC::ILWALID_FILE_FORMAT;
            }

            if (!m_SurfaceTypeMap[surf].bMapLoopback)
            {
                WarnPrintf("PEER only supported on loopback surfaces, ignoring!\n");
                peer = Gpu::MaxNumSubdevices;
            }
        }
        else if (0 == strcmp(tok, "NO_OFFSET"))
        {
            // When hacking the texture-header-pool data to set up a dynamic
            // texture as the Color0/etc surface, leave the offset field in
            // the existing texture-header entry alone.
            no_offset = true;
        }
        else if (0 == strcmp(tok, "NO_SURF_ATTR"))
        {
            // Do not use the surface attribute
            no_surf_attr = true;
        }
        else if (0 == strcmp(tok, "TARGET"))
        {
            if (OK != ParseString(&tex_header))
            {
                ErrPrintf("RELOC_D: Option TARGET needs a texture header file name\n");
                return RC::ILWALID_FILE_FORMAT;
            }
            bOptTarget = true;
        }
        else
        {
            return RC::ILWALID_FILE_FORMAT;
        }
    }

    UINT32 l1_promote = 0;
    if (params->ParamPresent("-l1_promotion"))
    {
        l1_promote = params->ParamUnsigned("-l1_promotion");
    }
    TraceModule *target = ModLast();
    if (bOptTarget)
    {
        target = ModFind(tex_header.c_str());
        if (0 == target)
        {
            ErrPrintf("RELOC_D: Target texture header pool file %s not found\n", tex_header.c_str());
            return RC::ILWALID_FILE_FORMAT;
        }
        else if (target->GetFileType() != FT_TEXTURE_HEADER)
        {
            ErrPrintf("RELOC_D: TARGET file %s is not a texture header\n", tex_header.c_str());
            return RC::ILWALID_FILE_FORMAT;
        }
    }

    bool bCenterSpoof = params->ParamPresent("-texture_center_spoof") > 0;
    if (m_Version < 5)
    {
        CHECK_RC(target->RelocAdd(new TraceRelocationDynTex(s, Mode, offset,
                                                       no_offset,
                                                       no_surf_attr,
                                                       target->GetSubdevMaskDataMap(),
                                                       peer,
                                                       bCenterSpoof,
                                                       m_Test->GetTextureHeaderFormat(),
                                                       l1_promote == 4))); //l1_promotion_optimal
    }
    else
    {
        // For dynamic textures allocated by ALLOC_SURFACE, the relocation
        // must also be dynamic, this is only for V5 or higher version
        TraceOp *pOp = new TraceOpDynTex(target, s, Mode, offset, no_offset, no_surf_attr,
            peer, bCenterSpoof, m_Test->GetTextureHeaderFormat(), l1_promote == 4);
        InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);
    }
    m_DynamicTextures = true;
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseRELOC_SIZE()
{
    // RELOC_SIZE command patches corresponding trace file with the size of
    // the specified file (unlike RELOC command, which patches with offset)
    // Update@07/02/2008 for bug440976, add mask_out & mask_in support
    // Update@06/17/2010 for bug660689, add TARGET optional argument,
    // change the syntax of the command,
    // reconstruct the code for further expansion of adding new optional arguments
    // Update@07/13/2010 for bug642960, allow the arguments after <offset> order-insensitive
    RC rc;
    CHECK_RC(RequireModLast("RELOC_SIZE"));
    CHECK_RC(RequireModLocal(ModLast(), "RELOC_SIZE"));

    // Allow the arguments after <offset> order-insensitive
    string syntax("RELOC_SIZE <offset> [mask_out [mask_in]] <filename> [TARGET <target>]");

    // Parse offset
    UINT32 offset;
    if (OK != ParseUINT32(&offset))
    {
        ErrPrintf("%s\n", syntax.c_str());
        ErrPrintf("Argument offset is required.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    // Parse mask_out and mask_in
    UINT64 mask_out = 0xffffffffffffffffull, mask_in = 0xffffffffffffffffull;
    UINT64 mask_temp = 0xffffffffffffffffull;
    bool mask_found = false;

    string filename;
    bool filename_found = false;

    TraceModule *target = ModLast();

    string temp;
    while (ParseFile::s_NewLine != m_TestHdrTokens.front())
    {
        // If it is number
        if (!mask_found && ParseUINT64(&mask_temp) == OK)
        {
            mask_found = true;
            mask_out = mask_temp;
            if (ParseUINT64(&mask_temp) == OK)
            {
                mask_in = mask_temp;
            }
        }
        // If it is filename
        else if (!filename_found && ParseFileOrSurfaceOrFileType(&filename) == OK)
        {
            filename_found = true;
        }
        // If it is optional argument
        else
        {
            // To handle errors
            if (ParseString(&temp) != OK)
            {
                ErrPrintf("%s\n", syntax.c_str());
                ErrPrintf("Unknown error oclwrred when parsing command RELOC_SIZE.");
                return RC::ILWALID_FILE_FORMAT;
            }
            // If it is TARGET
            else if (temp == "TARGET")
            {
                // Continue to read the value of target
                if (ParseString(&temp) != OK)
                {
                    ErrPrintf("%s\n", syntax.c_str());
                    ErrPrintf("Argument target is required.\n");
                    return RC::ILWALID_FILE_FORMAT;
                }
                target = ModFind(temp.c_str());
                if (0 == target)
                {
                    ErrPrintf("%s\n", syntax.c_str());
                    ErrPrintf("Target %s not found\n", temp.c_str());
                    return RC::ILWALID_FILE_FORMAT;
                }
            }
            // Unknown argument
            else
            {
                ErrPrintf("%s\n", syntax.c_str());
                ErrPrintf("Unknown argument: %s.\n", temp.c_str());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
    }

    if (!filename_found)
    {
        ErrPrintf("%s\n", syntax.c_str());
        ErrPrintf("Argument filename is required.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    CHECK_RC(target->RelocAdd(new TraceRelocationSize(mask_out, mask_in,
            offset, filename, true, target->GetSubdevMaskDataMap())));
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseRELOC_SIZE_LONG()
{
    // RELOC_SIZE_LONG command patches corresponding trace file with the size of
    // the specified file (unlike RELOC command, which patches with offset)
    // RELOC_SIZE_LONG <offset_low>:<offset_hi> <filename>

    RC rc;
    CHECK_RC(RequireModLast("RELOC_SIZE_LONG"));
    CHECK_RC(RequireModLocal(ModLast(), "RELOC_SIZE_LONG"));

    pair<UINT32, UINT32> offset;
    if (OK != ParseOffsetLong(&offset))
    {
        ErrPrintf("RELOC_SIZE_LONG requires <offset_low>:<offset_hi> argument.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    TraceModule * module;
    if (OK == ParseModule(&module))
    {
        CHECK_RC(ModLast()->RelocAdd(new TraceRelocationSizeLong(offset.first,
                                                        offset.second,
                                                        module,
                                                        ModLast()->GetSubdevMaskDataMap())));
    }
    else
    {
        ErrPrintf("RELOC_SIZE_LONG 0x%x:%x \"%s\" unrecognized FILE.\n",
                  offset.first, offset.second, m_TestHdrTokens.front());
        return RC::ILWALID_FILE_FORMAT;
    }
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseRELOC_LONG()
{
    // RELOC_LONG is similar to to RELOC40, but can be used with non-incremental methods
    // Format:
    // RELOC_LONG <offset_low>:<offset_hi> <filename>

    RC rc;
    CHECK_RC(RequireModLast("RELOC_LONG"));
    CHECK_RC(RequireModLocal(ModLast(), "RELOC_LONG"));

    pair<UINT32, UINT32> offset;
    if (OK != ParseOffsetLong(&offset))
    {
        ErrPrintf("RELOC_LONG requires <offset_low>:<offset_hi> argument.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    SurfaceType surf;
    TraceModule * module;
    UINT32 peerNum = Gpu::MaxNumSubdevices;
    UINT32 peerID = USE_DEFAULT_RM_MAPPING;
    if (OK == ParseSurfType(&surf))
    {
        CHECK_RC(ParseRelocPeer(surf, &peerNum));
        CHECK_RC(ModLast()->RelocAdd(new TraceRelocationLong(offset.first,
                                                    offset.second, surf,
                                                    ModLast()->GetSubdevMaskDataMap(),
                                                    peerNum)));
    }
    else if (OK == ParseModule(&module))
    {
        CHECK_RC(ParseRelocPeer(module->GetName(), &peerNum, &peerID));
        CHECK_RC(ModLast()->RelocAdd(new TraceRelocationLong(offset.first,
                                                    offset.second, module,
                                                    ModLast()->GetSubdevMaskDataMap(),
                                                    peerNum)));
    }
    else
    {
        ErrPrintf("RELOC_LONG 0x%x:0x%x \"%s\" unrecognized FILE/surface.\n",
                  offset.first, offset.second, m_TestHdrTokens.front());
        return RC::ILWALID_FILE_FORMAT;
    }

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseRELOC_CLEAR()
{
    // Format of RELOC_CLEAR command is:
    // RELOC <offset>

    RC rc;
    CHECK_RC(RequireModLast("RELOC_CLEAR"));
    CHECK_RC(RequireModLocal(ModLast(), "RELOC_CLEAR"));

    UINT32 offset;
    if (OK != ParseUINT32(&offset))
    {
        ErrPrintf("RELOC_CLEAR requires <offset>\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    if (params->ParamPresent("-gpu_clear") == 0 && params->ParamPresent("-fastclear") == 0)
    {
        CHECK_RC(ModLast()->RelocAdd(new TraceRelocationClear(offset,
                                                     ModLast()->GetSubdevMaskDataMap())));
    }

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseRELOC_ZLWLL()
{
    RC rc;
    CHECK_RC(RequireModLast("RELOC_ZLWLL"));
    CHECK_RC(RequireModLocal(ModLast(), "RELOC_ZLWLL"));

    UINT32 offset_a, offset_b, offset_c, offset_d, index;
    string surfname;
    UINT32 peerNum = Gpu::MaxNumSubdevices;
    UINT32 peerID = USE_DEFAULT_RM_MAPPING;

    if (OK != ParseUINT32(&offset_a) ||
        OK != ParseUINT32(&offset_b) ||
        OK != ParseUINT32(&offset_c) ||
        OK != ParseUINT32(&offset_d) ||
        OK != ParseFileOrSurfaceOrFileType(&surfname) ||
        OK != ParseUINT32(&index) ||
        OK != ParseRelocPeer(surfname, &peerNum, &peerID))
    {
        ErrPrintf("RELOC_ZLWLL requires offset_a offset_b offset_c offset_d surface_name index [PEER <peer num>]\n");
        return RC::ILWALID_FILE_FORMAT;
    }
    MdiagSurf* zlwll_storage = GetZLwllStorage(index, ModLast()->GetVASpaceHandle());
    MASSERT(zlwll_storage);

    return ModLast()->RelocAdd(new TraceRelocationZLwll(offset_a, offset_b,
                                                        offset_c,
                                                        offset_d,
                                                        zlwll_storage,
                                                        surfname,
                                                        ModLast()->GetSubdevMaskDataMap(),
                                                        peerNum));
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseRELOC_ACTIVE_REGION()
{
    // Format:
    // RELOC_ACTIVE_REGION offset file_name|Z
    //
    // this command patches corresponding trace file (likely push buffer)
    // with ActiveRegionId of the specified file (i.e. texture)
    //

    RC rc;
    CHECK_RC(RequireModLast("RELOC_ACTIVE_REGION"));
    CHECK_RC(RequireModLocal(ModLast(), "RELOC_ACTIVE_REGION"));

    UINT32 offset;
    if (OK != ParseUINT32(&offset))
    {
        ErrPrintf("RELOC_ACTIVE_REGION requires <offset> argument.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    string s;
    if (OK != ParseString(&s))
    {
        ErrPrintf("RELOC_ACTIVE_REGION requires FILE name (or \"Z\") argument.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    if (params->ParamPresent("-zlwll") > 0 || params->ParamPresent("-slwll") > 0)
        m_LwlledBuffers.insert(s);

    INT32 forcedRegionReloc = -1;
    if ( ((ZLwllEnum)params->ParamUnsigned("-zlwll_mode", ZLWLL_REQUIRED) == ZLWLL_CTXSW_SEPERATE_BUFFER)
       && (params->ParamPresent("-zlwll_allocation") > 0 || params->ParamPresent("-zlwll_subregions") > 0 ))
    {
         forcedRegionReloc = 0;
    }

    return ModLast()->RelocAdd(new TraceRelocationActiveRegion(offset,
                                                               s.c_str(),
                                                               this, ModLast()->GetSubdevMaskDataMap(), forcedRegionReloc));
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseRELOC_TYPE()
{
    // RELOC_TYPE command patches corresponding trace file
    // with the offset within a given type

    RC rc;
    CHECK_RC(RequireModLast("RELOC_TYPE"));
    CHECK_RC(RequireModLocal(ModLast(), "RELOC_TYPE"));

    UINT32 offset;
    if (OK != ParseUINT32(&offset))
    {
        ErrPrintf("RELOC_TYPE requires <offset> argument.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    TraceModule * module;
    if (OK == ParseModule(&module))
    {
        CHECK_RC(ModLast()->RelocAdd(new TraceRelocationType(offset, module,
                                                    ModLast()->GetSubdevMaskDataMap())));
    }
    else
    {
        ErrPrintf("RELOC_TYPE 0x%x \"%s\" unrecognized FILE.\n", offset, m_TestHdrTokens.front());
        return RC::ILWALID_FILE_FORMAT;
    }
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseRELOC_BRANCH()
{
    // RELOC_BRANCH patches branches of shader program
    // format is:
    //    RELOC_BRANCH <offset> <mask_out> [mask_in] <SWAP|NO_SWAP>
    //      <trace_file> [TARGET file]
    //
    //    <offset> is 64-bits word to be patched
    //    <mask_out> specifies mask
    //    <mask_in> optional argument
    //    <TARGET> optinal argument, specify the file to be patched
    // after 64-bit words is extracted, mask is applied
    // and offset of <trace_file> within corresponding file
    // is added. result is stored back (taking mask into account)

    RC rc;
    UINT32 offset = 0;
    UINT64 mask_out = 0xffffffffffffffffull;
    UINT64 mask_in  = 0xffffffffffffffffull;
    bool bSwap = true;
    string swap;
    CHECK_RC(RequireModLast("RELOC_BRANCH"));
    CHECK_RC(RequireModLocal(ModLast(), "RELOC_BRANCH"));
    TraceModule * module = 0;
    TraceModule * target = ModLast();

    const string syntax("RELOC_BRANCH <offset> <mask_out> [mask_in] "
        "<SWAP|NO_SWAP> <trace_file> [TARGET trace_file]");

    if ((OK != ParseUINT32(&offset)) ||
        (OK != ParseUINT64(&mask_out)))
    {
        ErrPrintf("Expected format: %s\n", syntax.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }

    if (OK != ParseModule(&module))
    {
        // New format, need to parse optional arguments
        // mask_in is optional
        if (OK != ParseUINT64(&mask_in))
        {
            mask_in = 0xffffffffffffffffull;
        }

        if (OK != ParseString(&swap) ||
            (swap != "SWAP" && swap != "NO_SWAP") ||
            OK != ParseModule(&module))
        {
            ErrPrintf("Expected format: %s\n", syntax.c_str());
            return RC::ILWALID_FILE_FORMAT;
        }
        bSwap = (swap == "SWAP" ? true : false);
    }

    while (ParseFile::s_NewLine != m_TestHdrTokens.front())
    {
        const char *tok = m_TestHdrTokens.front();
        m_TestHdrTokens.pop_front();
        if (0 == strcmp(tok, "TARGET"))
        {
            if (OK != ParseModule(&target))
            {
                ErrPrintf("Expected format: %s\n", syntax.c_str());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
    }

    return target->RelocAdd(new TraceRelocationBranch(mask_out, mask_in,
        offset, bSwap, module, true, target->GetSubdevMaskDataMap()));
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseRELOC64()
{
    // RELOC64 adds a surface offset to a field of up to 64 bits spread
    // across two 32-bit words in the target pushbuffer or surface.
    // It is equivalent to RELOC_BRANCH, but adds the SWAP/NO_SWAP tag
    // to control bigendian swapping.
    //
    // format is:
    //    RELOC64 <offset> <mask_out> [mask_in] <SWAP|NO_SWAP> <trace_file>
    //    [UPDATE <pb_offset>] [PEER <peer_num>] [TARGET|FILE <name>] 
    //    [SIGN_EXTEND <extend_mask> <extend_mode>]
    //
    //    <offset> is 64-bits word to be patched
    //    <mask>   specifies mask
    // after 64-bit words is extracted, mask is applied
    // and offset of <trace_file> within corresponding file
    // is added. result is stored back (taking mask into account)

    // Every time we update syntax, we should update it
    const string syntax("RELOC64 <offset> <mask_out> [mask_in] <SWAP|NO_SWAP> <trace_file>"
          " [UPDATE <pb_offset>] [PEER <peer_num>] [TARGET|FILE <name>] "
          "[SIGN_EXTEND <extend_mask> <extend_mode>]");
    RC rc;
    CHECK_RC(RequireModLast("RELOC64"));
    TraceModule *target = ModLast();

    UINT32 offset;
    UINT64 mask_out;
    string swap;

    if (OK != ParseUINT32(&offset) ||
        OK != ParseUINT64(&mask_out) ||
        OK != ParseString(&swap))
    {
        ErrPrintf("%s\n", syntax.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }

    bool bSwap = true;
    UINT64 mask_in = 0xffffffffffffffffull;

    if (swap != "SWAP" && swap != "NO_SWAP")
    {
        char *p;
        mask_in = Utility::Strtoull(swap.c_str(), &p, 0);
        if (p == swap.c_str())
        {
            ErrPrintf("%s\n", syntax.c_str());
            ErrPrintf("Unexpected token: %s\n", swap.c_str());
            return RC::ILWALID_FILE_FORMAT;
        }

        if (OK != ParseString(&swap))
        {
            ErrPrintf("%s\n", syntax.c_str());
            return RC::ILWALID_FILE_FORMAT;
        }
    }

    if (swap == "SWAP")
        bSwap = true;
    else if (swap == "NO_SWAP")
        bSwap = false;
    else
    {
        ErrPrintf("%s\n", syntax.c_str());
        ErrPrintf("Unexpected token: %s\n", swap.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }

    string surfname;
    if (OK != ParseFileOrSurfaceOrFileType(&surfname))
    {
        ErrPrintf("File %s not found\n", m_TestHdrTokens.front());
        return RC::ILWALID_FILE_FORMAT;
    }

    // Begin to parse optional arguments
    UINT32 peerNum = Gpu::MaxNumSubdevices;
    UINT32 peerID = USE_DEFAULT_RM_MAPPING;
    UINT32 updateNum = 0;
    string filename;
    bool useTraceOp = false;
    bool bOptTarget = false;
    UINT64 signExtendMask = 0;
    string signExtendMode;

    while (ParseFile::s_NewLine != m_TestHdrTokens.front())
    {
        const char *tok = m_TestHdrTokens.front();

        if (0 == strcmp(tok, "PEER"))
        {
            rc = ParseRelocPeer(surfname, &peerNum, &peerID);
            if (OK != rc)
            {
                return rc;
            }
        }
        else if ((0==strcmp(tok, "TARGET")) || (0==strcmp(tok, "FILE")))
        {
            m_TestHdrTokens.pop_front();
            if (OK != ParseString(&filename))
            {
                return RC::ILWALID_FILE_FORMAT;
            }
            bOptTarget = true;
        }
        else if (0 == strcmp(tok, "UPDATE"))
        {
            m_TestHdrTokens.pop_front();
            CHECK_RC(RequireNotVersion(3, "RELOC64"));
            if (m_Version < 3)
            {
                if (OK != ParseUINT32(&updateNum))
                {
                    return RC::ILWALID_FILE_FORMAT;
                }
            }
            else
            {
                updateNum = m_LineNumber;
            }
            useTraceOp = true;
        }
        else if (0 == strcmp(tok, "SIGN_EXTEND"))
        {
            m_TestHdrTokens.pop_front();
            if (OK != ParseUINT64(&signExtendMask)||
                OK != ParseString(&signExtendMode))
            {
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else
        {
            m_TestHdrTokens.pop_front();
            ErrPrintf("%s\n", syntax.c_str());
            ErrPrintf("Unexpected token: %s\n", tok);
            return RC::ILWALID_FILE_FORMAT;
        }
    }
    if (bOptTarget)
    {
        target = ModFind(filename.c_str());
        if (0 == target)
        {
            ErrPrintf("%s\n", syntax.c_str());
            ErrPrintf("Module %s not found\n", filename.c_str());
            return RC::ILWALID_FILE_FORMAT;
        }
    }

    TraceModule *pmodule = ModFind(surfname.c_str());

    if (target->IsDynamic() || (pmodule && pmodule->IsDynamic()))
    {
        updateNum = m_LineNumber;
        useTraceOp = true;
    }
    if (pmodule != 0 && m_Test->GetSSM()->IsSliSystem(pmodule->GetGpuDev()))
    {
        // Build up a relocation dependency relationship for SLI
        pmodule->AddRelocTarget(target);
    }

    if (useTraceOp)
    {
        UINT32 subdevNum = m_Test->GetBoundGpuDevice()->GetNumSubdevices();
        TraceOp* pOp = new TraceOpUpdateReloc64(surfname, target,
                                                mask_out, mask_in, offset, bSwap, true,
                                                peerNum, subdevNum, signExtendMask,
                                                signExtendMode);
        InsertOp(updateNum, pOp, OP_SEQUENCE_OTHER);
    }
    else
    {
        bool peer = pmodule &&
                    pmodule->GetFileType() == FT_TEXTURE &&
                    _DMA_TARGET_P2P == (_DMA_TARGET)params->ParamUnsigned("-loc_tex");
        CHECK_RC(target->RelocAdd(new TraceRelocation64(mask_out,
                                                        mask_in,
                                                        offset,
                                                        bSwap,
                                                        surfname,
                                                        peer,
                                                        true,
                                                        pmodule,
                                                        target->GetSubdevMaskDataMap(),
                                                        peerNum,
                                                        peerID,
                                                        signExtendMask,
                                                        signExtendMode)));
    }

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseRELOC_RESET_METHOD()
{
    RC rc;
    CHECK_RC(RequireModLast("RELOC_RESET_METHOD"));
    CHECK_RC(RequireModLocal(ModLast(), "RELOC_RESET_METHOD"));

    UINT32 offset;
    if (OK != ParseUINT32(&offset))
    {
        ErrPrintf("RELOC_RESET_METHOD requires an <offset> argument.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    TraceModule *module = ModLast();
    MASSERT(!module->HasRelocResetMethod(offset));
    return module->RelocResetMethodAdd(offset);
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseRELOC_FREEZE()
{
    RC rc;
    CHECK_RC(RequireModLast("RELOC_FREEZE"));
    CHECK_RC(RequireModLocal(ModLast(), "RELOC_FREEZE"));

    UINT32 offset;
    if (OK != ParseUINT32(&offset))
    {
        ErrPrintf("RELOC_FREEZE requires an <offset> argument.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    return ModLast()->AddFreezeReloc(offset);
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseRELOC_MS32()
{
    RC rc;
    CHECK_RC(RequireModLast("RELOC_MS32"));
    CHECK_RC(RequireModLocal(ModLast(), "RELOC_MS32"));

    UINT32 offset;
    TraceModule * module;
    UINT32 peerNum = Gpu::MaxNumSubdevices;
    UINT32 peerID = USE_DEFAULT_RM_MAPPING;

    if ((OK != ParseUINT32(&offset)) ||
        (OK != ParseModule(&module)) ||
        (OK != ParseRelocPeer(module->GetName(), &peerNum, &peerID)))
    {
        ErrPrintf("RELOC_MS32 requires <offset> <FILE name> [PEER <peer num>] arguments.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    return ModLast()->RelocAdd(new TraceRelocationFileMs32(offset, module,
                                                        ModLast()->GetSubdevMaskDataMap(),
                                                        peerNum));
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseRELOC_CONST32()
{
    RC rc;
    CHECK_RC(RequireModLast("RELOC_CONST32"));
    CHECK_RC(RequireModLocal(ModLast(), "RELOC_CONST32"));
    CHECK_RC(RequireNotVersion(3, "RELOC_CONST32"));

    UINT32 offset;
    UINT32 value;

    if ((OK != ParseUINT32(&offset)) ||
        (OK != ParseUINT32(&value)))
    {
        ErrPrintf("RELOC_CONST32 requires <offset> <value> arguments.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    if (ParseFile::s_NewLine == m_TestHdrTokens.front())
    {
        CHECK_RC(ModLast()->RelocAdd(new TraceRelocationConst32(offset, value,
                                                       ModLast()->GetSubdevMaskDataMap())));
    }
    else
    {
        string        update;
        UINT32        tupdate;
        if (m_Version < 3)
        {
            if ((OK != ParseString(&update)) ||
                (OK != ParseUINT32(&tupdate)))
            {
                ErrPrintf("UPDATE requires <pbuf_offset> args\n");
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else
        {
            // v4+
            if (OK != ParseString(&update))
            {
                ErrPrintf("unknown arg in RELOC64\n");
                return RC::ILWALID_FILE_FORMAT;
            }
            tupdate = m_LineNumber;
        }
        if (update != "UPDATE")
        {
            ErrPrintf("unknown arg in RELOC_CONST32: %s\n", update.c_str());
            return RC::ILWALID_FILE_FORMAT;
        }
        TraceOp* pOp = new TraceOpUpdateRelocConst32( ModLast(),
                                                      value, offset);
        InsertOp(tupdate, pOp, OP_SEQUENCE_GPENTRY);
    }

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseRELOC_CTXDMA_HANDLE()
{
    RC rc;
    CHECK_RC(RequireModLast("RELOC_CTXDMA_HANDLE"));
    CHECK_RC(RequireModLocal(ModLast(), "RELOC_CTXDMA_HANDLE"));

    UINT32 offset;
    string surfname;

    if ((OK != ParseUINT32(&offset)) ||
        (OK != ParseFileOrSurfaceOrFileType(&surfname)))
    {
        ErrPrintf("RELOC_CTXDMA_HANDLE requires <offset> <surf name> arguments.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    return ModLast()->RelocAdd(new TraceRelocationCtxDMAHandle(offset,
                                                               surfname,
                                                        ModLast()->GetSubdevMaskDataMap()));
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseRELOC_VP2_PITCH()
{
    // RELOC_VP2_PITCH patches pitch value in pushbuffer
    // format is:
    //    RELOC_VP2_PITCH <offset> <filename> <start_bit_position> <number_of_bits>
    //
    //    <offset>   The offset of the parameter method data within the pushbuffer.
    //               This parameter method data carries pitch.
    //    <filename> The filename specifying the surface.
    //
    //    <start_bit_position> start_bit_position 0 based of the pitch field within
    //                         the parameter method data carrying the pitch.
    //    <number_of_bits>     number_of_bits of the pitch field within
    //                         the parameter method data carrying pitch.
    RC rc;
    CHECK_RC(RequireModLast("RELOC_VP2_PITCH"));
    CHECK_RC(RequireModLocal(ModLast(), "RELOC_VP2_PITCH"));

    UINT32       offset;
    TraceModule *module;
    UINT32       start_bit_position;
    UINT32       number_of_bits;

    if ((OK != ParseUINT32(&offset))             ||
        (OK != ParseModule(&module))             ||
        (OK != ParseUINT32(&start_bit_position)) ||
        (OK != ParseUINT32(&number_of_bits)))
    {
        ErrPrintf("RELOC_VP2_PITCH requires <offset> <filename> <start_bit_position> <number_of_bits> arguments.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    CHECK_RC(ModLast()->RelocAdd(new TraceRelocatiolwP2Pitch(offset, module,
                                                    start_bit_position,
                                                    number_of_bits,
                                                    ModLast()->GetSubdevMaskDataMap())));

    module->SetVP2PitchRelocPresent();

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseRELOC_VP2_TILEMODE()
{
    // RELOC_VP2_TILEMODE patches tile mode value in pushbuffer
    // format is:
    //    RELOC_VP2_TILEMODE <offset> <filename> <start_bit_position> <number_of_bits>
    //
    //    <offset>   The offset of the parameter method data within the pushbuffer.
    //               This parameter method data carries tile mode.
    //    <filename> The filename specifying the surface.
    //
    //    <start_bit_position> start_bit_position 0 based of the tile mode field within
    //                         the parameter method data carrying the tile mode.
    //    <number_of_bits>     number_of_bits of the tile mode field within
    //                         the parameter method data carrying tile mode.
    RC rc;
    CHECK_RC(RequireModLast("RELOC_VP2_TILEMODE"));
    CHECK_RC(RequireModLocal(ModLast(), "RELOC_VP2_TILEMODE"));

    UINT32       offset;
    TraceModule *module;
    UINT32       start_bit_position;
    UINT32       number_of_bits;

    if ((OK != ParseUINT32(&offset))             ||
        (OK != ParseModule(&module))             ||
        (OK != ParseUINT32(&start_bit_position)) ||
        (OK != ParseUINT32(&number_of_bits)))
    {
        ErrPrintf("RELOC_VP2_TILEMODE requires <offset> <filename> <start_bit_position> <number_of_bits> arguments.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    CHECK_RC(ModLast()->RelocAdd(new TraceRelocatiolwP2TileMode(offset, module,
                                                       start_bit_position,
                                                       number_of_bits,
                                                       ModLast()->GetSubdevMaskDataMap())));

    module->SetVP2TileModeRelocPresent();

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseRELOC_SURFACE()
{
    // RELOC_SURFACE command patches corresponding trace file
    // with the offset of a texture pixel given given by tex_id, array_index, lwbemap_face, miplevel, x, y, z [MASK_OUT mask] [MASK_IN mask]
    //
    // format is:
    //    RELOC_SURFACE <offset> <surfacename>  array_index lwbemap_face miplevel x y z
    //
    //    <offset>   The offset in the file to be patched
    //    <surfacename> Either a texture index in texture header pool or a surface name
    //    <MASK_OUT/MASK_IN> 64-bit mask, optional
    RC rc;
    CHECK_RC(RequireModLast("RELOC_SURFACE"));
    CHECK_RC(RequireModLocal(ModLast(), "RELOC_SURFACE"));
    TraceModule *target = ModLast();

    UINT32 offset;
    if (OK != ParseUINT32(&offset))
    {
        ErrPrintf("RELOC_SURFACE requires <offset> argument.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    UINT32 tex_index;
    string surf_name;
    bool use_tex_idx = false;

    if (OK == ParseUINT32(&tex_index))
    {
        use_tex_idx = true;
    }
    else if (OK == ParseString(&surf_name))
    {
        // file can be Color0-7/Z or any other surfaces allocated by ALLOC_SURF
        use_tex_idx = false;
    }
    else
    {
        ErrPrintf("RELOC_SURFACE 0x%x \"%s\" unrecognized surface/texture index.\n",
                  offset, m_TestHdrTokens.front());
        return RC::ILWALID_FILE_FORMAT;
    }

    UINT32 array_index, lwbemap_face, miplevel, x, y, z;
    if (OK != ParseUINT32(&array_index)  ||
        OK != ParseUINT32(&lwbemap_face) ||
        OK != ParseUINT32(&miplevel)     ||
        OK != ParseUINT32(&x)            ||
        OK != ParseUINT32(&y)            ||
        OK != ParseUINT32(&z))
    {
        ErrPrintf("RELOC_SURFACE requires <offset> <TEX_INDEX/surface_name> <array_index> "
                  "<lwbemap_face> <miplevel> <x> <y> <z> [PEER <peer num>]\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    // Parse optional arguments
    UINT64 mask_out = 0xffffffffffffffffull;
    UINT64 mask_in  = 0xffffffffffffffffull;
    bool swap = false;
    while (ParseFile::s_NewLine != m_TestHdrTokens.front())
    {
        const char *tok = m_TestHdrTokens.front();
        m_TestHdrTokens.pop_front();

        if (0 == strcmp(tok, "MASK_OUT"))
        {
            if (OK != ParseUINT64(&mask_out))
            {
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "MASK_IN"))
        {
            if (OK != ParseUINT64(&mask_in))
            {
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "SWAP"))
        {
            swap = true;
        }
        else if (0 == strcmp(tok, "NO_SWAP"))
        {
            swap = false;
        }
        else if (0 == strcmp(tok, "TARGET"))
        {
            string filename;
            if (OK != ParseString(&filename))
            {
                return RC::ILWALID_FILE_FORMAT;
            }
            target = ModFind(filename.c_str());
            if (0 == target)
            {
                ErrPrintf("RELOC_SURFACE: TARGET %s not found\n", filename.c_str());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else
        {
            ErrPrintf("RELOC_SURFACE: Unexpected token: %s\n", tok);
            return RC::ILWALID_FILE_FORMAT;
        }
    }

    UINT32 peerNum = Gpu::MaxNumSubdevices;
    UINT32 peerID = USE_DEFAULT_RM_MAPPING;
    if (use_tex_idx)
    {
        TraceModule * module = ModFindTexIndex(tex_index);
        if (!module)
        {
            ErrPrintf("RELOC_SURFACE: can not find texture with index 0x%x\n", tex_index);
            return RC::ILWALID_FILE_FORMAT;
        }

        CHECK_RC(ParseRelocPeer(module->GetName(), &peerNum, &peerID));
        if (m_Version < 5)
        {
            CHECK_RC(target->RelocAdd(new TraceRelocationFile(offset, module,
                              mask_out, mask_in, swap,
                              array_index,
                              lwbemap_face,
                              miplevel,
                              x, y, z,
                              ModLast()->GetSubdevMaskDataMap(),
                              peerNum)));
        }
        else
        {
            // V5 or higher version support dynamic allocation
            TraceOp *pOp = new TraceOpFile(target, offset, module, array_index,
                lwbemap_face, miplevel, x, y, z, peerNum, mask_out, mask_in, swap);
            InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);
        }
    }
    else
    {
        CHECK_RC(ParseRelocPeer(surf_name, &peerNum, &peerID));
        if (m_Version < 5)
        {
            CHECK_RC(target->RelocAdd(new TraceRelocationSurface(offset, surf_name,
                                                       array_index,
                                                       lwbemap_face,
                                                       miplevel,
                                                       x, y, z,
                                                       swap,
                                                       ModLast()->GetSubdevMaskDataMap(),
                                                       peerNum,
                                                       m_Test->GetBoundGpuDevice(),
                                                       mask_out, mask_in)));
        }
        else
        {
            // V5 or higher version support dynamic allocation
            TraceOp *pOp = new TraceOpSurface(target, offset, surf_name,
                array_index, lwbemap_face, miplevel, x, y, z, peerNum, m_Test->GetBoundGpuDevice(),
                mask_out, mask_in, swap);
            InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);
        }
    }

    return OK;
}

RC GpuTrace::ParseRELOC_SURFACE_PROPERTY()
{
    RC rc;
    CHECK_RC(RequireModLast("RELOC_SURFACE_PROPERTY"));
    CHECK_RC(RequireModLocal(ModLast(), "RELOC_SURFACE_PROPERTY"));

    UINT32 offset;
    UINT64 mask_out = 0xffffffffffffffffull, mask_in = 0xffffffffffffffffull;
    string property;
    string name;
    SurfaceType surf_type;
    string write_mode;
    bool is_add = false;
    string target_name;
    bool swap = true;

    TraceModule *target = ModLast();

    if (ParseUINT32(&offset)   != OK ||
        ParseUINT64(&mask_out) != OK ||
        ParseUINT64(&mask_in)  != OK ||
        ParseString(&property) != OK)
    {
        ErrPrintf("Syntax: RELOC_SURFACE_PROPERTY offset mask_out mask _in <property> <name> [write_mode]\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    string oldName = m_TestHdrTokens.front();

    if (ParseFileOrSurfaceOrFileType(&name) != OK)
    {
        ErrPrintf("Syntax: RELOC_SURFACE_PROPERTY offset mask_out mask _in <property> <name> [write_mode]\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    if (property != "PIXEL_WIDTH" &&
        property != "PIXEL_HEIGHT" &&
        property != "SAMPLE_WIDTH" &&
        property != "SAMPLE_HEIGHT" &&
        property != "DEPTH" &&
        property != "ARRAY_PITCH" &&
        property != "BLOCK_WIDTH" &&
        property != "BLOCK_HEIGHT" &&
        property != "BLOCK_DEPTH" &&
        property != "ARRAY_SIZE" &&
        property != "FORMAT" &&
        property != "LAYOUT" &&
        property != "AA_MODE" &&
        property != "MEMORY" &&
        property != "AA_ENABLED" &&
        property != "PHYS_ADDRESS" &&
        property != "SAMPLE_POSITION0" &&
        property != "SAMPLE_POSITION1" &&
        property != "SAMPLE_POSITION2" &&
        property != "SAMPLE_POSITION3" &&
        property != "SAMPLE_POSITION_FLOAT0" &&
        property != "SAMPLE_POSITION_FLOAT1" &&
        property != "SAMPLE_POSITION_FLOAT2" &&
        property != "SAMPLE_POSITION_FLOAT3" &&
        property != "SAMPLE_POSITION_FLOAT4" &&
        property != "SAMPLE_POSITION_FLOAT5" &&
        property != "SAMPLE_POSITION_FLOAT6" &&
        property != "SAMPLE_POSITION_FLOAT7" &&
        property != "SAMPLE_POSITION_FLOAT8" &&
        property != "SAMPLE_POSITION_FLOAT9" &&
        property != "SAMPLE_POSITION_FLOAT10" &&
        property != "SAMPLE_POSITION_FLOAT11" &&
        property != "SAMPLE_POSITION_FLOAT12" &&
        property != "SAMPLE_POSITION_FLOAT13" &&
        property != "SAMPLE_POSITION_FLOAT14" &&
        property != "SAMPLE_POSITION_FLOAT15")
    {
        ErrPrintf("RELOC_SURFACE_PROPERTY: Undefined property %s\n", property.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }

    // If -prog_zt_as_ct0 is set, the Z-buffer is used
    // instead of the first color render target.
    // However, the color format used must come from the
    // the color surface rather than the Z-buffer.
    if ((params->ParamPresent("-prog_zt_as_ct0") > 0) &&
        (oldName == "COLOR0") &&
        (property == "FORMAT"))
    {
        name = "COLOR0";
    }

    string temp;
    while (ParseFile::s_NewLine != m_TestHdrTokens.front())
    {
        if (ParseString(&temp) == OK)
        {
            if (temp == "REPLACE")
            {
                is_add = false;
            }
            else if (temp == "ADD")
            {
                is_add = true;
            }
            else if (temp == "TARGET")
            {
                if (OK != ParseString(&target_name))
                {
                    ErrPrintf("RELOC_SURFACE_PROPERTY: target expected.\n");
                    return RC::ILWALID_FILE_FORMAT;
                }
                target = ModFind(target_name.c_str());
                if (0 == target)
                {
                    ErrPrintf("RELOC_SURFACE_PROPERTY: Undefined target %s.\n", target_name.c_str());
                    return RC::ILWALID_FILE_FORMAT;
                }
            }
            else if (temp == "SWAP")
            {
                swap = true;
            }
            else if (temp == "NO_SWAP")
            {
                swap = false;
            }
            else
            {
                ErrPrintf("Unexpected token: %s\n", temp.c_str());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else
        {
            ErrPrintf("Syntax: RELOC_SURFACE_PROPERTY offset mask_out mask _in <property> <name> [write_mode]\n");
            return RC::ILWALID_FILE_FORMAT;
        }
    }

    surf_type = FindSurface(name);

    TraceModule *sourceModule = ModFind(name.c_str());

    if (target->IsDynamic() ||
        ((sourceModule != 0) && sourceModule->IsDynamic()))
    {
        // Reloc dynamic allocated surfaces
        TraceOp *traceOp = new TraceOpRelocSurfaceProperty(offset, mask_out, mask_in, is_add,
            property, name, surf_type, target, swap, (params->ParamPresent("-prog_zt_as_ct0") > 0),
            target->GetSubdevMaskDataMap());
        InsertOp(m_LineNumber, traceOp, OP_SEQUENCE_METHODS);
    }
    else
    {
        CHECK_RC(target->RelocAdd(new TraceRelocationSurfaceProperty(mask_out, mask_in,
            offset, property, name, surf_type, 0, is_add, swap, (params->ParamPresent("-prog_zt_as_ct0") > 0),
            target->GetSubdevMaskDataMap())));
    }

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseSIZE()
{
    UINT32 x;
    UINT32 y;

    if ((OK != ParseUINT32(&x)) ||
        (OK != ParseUINT32(&y)))
    {
        ErrPrintf("SIZE requires <x> <y> arguments.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    m_Width = x;
    m_Height = y;

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseGPENTRY()
{
    RC rc;
    CHECK_RC(RequireModLast("GPENTRY"));
    CHECK_RC(RequireNotVersion(3, "GPENTRY"));

    bool sync = false;
    UINT32 pbufOffset = m_LineNumber;
    TraceModule * module;
    UINT64 fileOffsetInBytes;
    UINT32 sizeInBytes;
    TraceChannel * pTraceChannel;
    TraceModule  * pSizeModule = NULL;

    if (m_Version < 3)
    {
        if ((OK != ParseUINT32(&pbufOffset)) ||
            (OK != ParseModule(&module)) ||
            (OK != ParseUINT64(&fileOffsetInBytes)))
        {
            ErrPrintf("GPENTRY requires <pbuf_offset> <FILE name> <FILE offset> <nbytes> [SYNC|REPEAT <num>] args.\n");
            return RC::ILWALID_FILE_FORMAT;
        }

        pTraceChannel = GetLwrrentChannel(CPU_MANAGED);
    }
    else
    {
        // version 4+
        if ((OK != ParseChannel(&pTraceChannel, CPU_MANAGED)) ||
            (OK != ParseModule(&module)) ||
            (OK != ParseUINT64(&fileOffsetInBytes)))
        {
            ErrPrintf("GPENTRY requires <CHANNEL name> <FILE name> <FILE offset> <nbytes> [SYNC|REPEAT <num>] args.\n");
            return RC::ILWALID_FILE_FORMAT;
        }
    }
    // Until GPENTRY is modified to specify which remote subdevice to
    // pull the file offset from, disallow peer modules for GPENTRY
    CHECK_RC(RequireModLocal(module, "GPENTRY"));

    if ((OK != ParseUINT32(&sizeInBytes)) &&
        (OK != ParseModule(&pSizeModule)))
    {
        ErrPrintf("GPENTRY : <nbytes> must either be a number or a module name.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    const char * tok = m_TestHdrTokens.front();
    UINT32       repeat_num   = 1; // By default only repeat once

    // m_TestHdrTokens.pop_front();
    while (ParseFile::s_NewLine != tok)
    {
        if (0 == strcmp(tok, "SYNC"))
        {
            m_TestHdrTokens.pop_front();
            sync = true;
        }
        else if (0 == strcmp(tok, "REPEAT"))
        {
            m_TestHdrTokens.pop_front();
            if (OK != ParseUINT32(&repeat_num))
            {
                ErrPrintf("GPENTRY: REPEAT argument needs a number.\n");
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else
        {
            ErrPrintf("Unrecognized token: %s\n", tok);
            return RC::ILWALID_FILE_FORMAT;
        }
        tok = m_TestHdrTokens.front();
    }

    if (pSizeModule != 0)
    {
        sizeInBytes = pSizeModule->GetSize();
    }

    TraceOpSendGpEntry * pOp;
    pOp = new TraceOpSendGpEntry(pTraceChannel,
                   module,
                   fileOffsetInBytes,
                   sizeInBytes,
                   sync,
                   repeat_num);

    InsertOp(pbufOffset, pOp, OP_SEQUENCE_GPENTRY);
    m_LastGpEntryOp = pOp;

    IncrementTraceChannelGpEntries(pTraceChannel);

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseGPENTRY_CONTINUE()
{
    RC rc;
    CHECK_RC(RequireVersion(5, "GPENTRY_CONTINUE"));
    if (m_LastGpEntryOp == 0)
    {
        ErrPrintf("GPENTRY_CONTINUE requires to follow a valid GPENTRY.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    UINT32 size = 0;
    if (OK != ParseUINT32(&size))
    {
        ErrPrintf("GPENTRY_CONTINUE : <nbytes> must be a number.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    // In case last inserted TraceOp is TraceOpSendGpEntry,
    // consolidate trace command GPENTRY_CONTINUE with last GPENTRY
    TraceOps::reverse_iterator lastOp = m_TraceOps.rbegin();
    if ((lastOp != m_TraceOps.rend()) &&
        (m_LastGpEntryOp == lastOp->second))
    {
        CHECK_RC(m_LastGpEntryOp->ConsolidateGpEntry(size));
    }
    else
    {
        // create a new TraceOpSendGpEntry obj based on last GPENTRY
        TraceOpSendGpEntry * pOp;
        pOp = m_LastGpEntryOp->CreateNewContGpEntryOp(size);
        if (pOp == 0)
        {
            ErrPrintf("GPENTRY_CONTINUE: can't create a new TraceOpSendGpEntry obj.\n");
            return RC::ILWALID_FILE_FORMAT;
        }

        InsertOp(m_LineNumber, pOp, OP_SEQUENCE_GPENTRY);
        m_LastGpEntryOp = pOp;
    }

    TraceChannel * pTraceChannel = m_LastGpEntryOp->GetTraceChannel();
    IncrementTraceChannelGpEntries(pTraceChannel);

    return rc;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseWAIT_FOR_IDLE()
{
    RC rc;
    CHECK_RC(RequireNotVersion(3, "WAIT_FOR_IDLE"));

    if (m_Version < 3)
    {
        CHECK_RC(RequireModLast("WAIT_FOR_IDLE"));
        UINT32 pbufOffset;
        if (OK != ParseUINT32(&pbufOffset))
        {
            ErrPrintf("WAIT_FOR_IDLE requires <pbuf_offset> arg.\n");
            return RC::ILWALID_FILE_FORMAT;
        }
        TraceOpWaitIdle * pOp = new TraceOpWaitIdle();
        CHECK_RC(pOp->AddWFITraceChannel(GetLwrrentChannel(CPU_MANAGED)));
        UpdateTraceChannelMaxGpEntries(GetLwrrentChannel(CPU_MANAGED));
        InsertOp(pbufOffset, pOp, OP_SEQUENCE_WAIT_FOR_IDLE);
    }
    else
    {
        // Version 4
        TraceOpWaitIdle * pOp = new TraceOpWaitIdle();

        // Required
        TraceChannel * pTraceChannel;
        if (OK != ParseChannel(&pTraceChannel, CPU_MANAGED))
        {
            ErrPrintf("WAIT_FOR_IDLE requires <channelname> arg.\n");
            ErrPrintf("Syntax: WAIT_FOR_IDLE <chname1> [chname2] [chname3] ...\n");
            return RC::ILWALID_FILE_FORMAT;
        }
        CHECK_RC(pOp->AddWFITraceChannel(pTraceChannel));
        UpdateTraceChannelMaxGpEntries(pTraceChannel);

        // Optional
        while (ParseFile::s_NewLine != m_TestHdrTokens.front())
        {
            if (OK != ParseChannel(&pTraceChannel, CPU_MANAGED))
            {
                ErrPrintf("Invalid channel name \"%s\" in WAIT_FOR_IDLE command\n",
                          m_TestHdrTokens.front());
                return RC::ILWALID_FILE_FORMAT;
            }

            CHECK_RC(pOp->AddWFITraceChannel(pTraceChannel));
            UpdateTraceChannelMaxGpEntries(pTraceChannel);
        }

        InsertOp(m_LineNumber, pOp, OP_SEQUENCE_WAIT_FOR_IDLE);
    }

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseUPDATE_FILE()
{
    RC rc;
    CHECK_RC(RequireModLast("UPDATE_FILE"));
    CHECK_RC(RequireNotVersion(3, "UPDATE_FILE"));

    const bool flushCache =
        (params->ParamPresent("-flush_before_update_file") > 0);

    if (m_Version < 3)
    {
        UINT32        pbufOffset;
        TraceModule * pTraceModule;
        UINT32        moduleOffset;
        string        filename;

        if ((OK != ParseUINT32(&pbufOffset)) ||
            (OK != ParseModule(&pTraceModule)) ||
            (OK != ParseUINT32(&moduleOffset)) ||
            (OK != ParseFileName(&filename, "UPDATE_FILE")))
        {
            ErrPrintf("UPDATE_FILE requires <pbuf_offset> <dest name> <dest offset> <update_filename> args.\n");
            return RC::ILWALID_FILE_FORMAT;
        }

        CHECK_RC(RequireModLocal(pTraceModule, "UPDATE_FILE"));

        TraceOpUpdateFile * pOp = new TraceOpUpdateFile(
                pTraceModule,
                moduleOffset,
                m_TraceFileMgr,
                filename,
                flushCache,
                m_Test->GetBoundGpuDevice(),
                m_TimeoutMs);

        InsertOp(pbufOffset, pOp, OP_SEQUENCE_UPDATE_FILE);
    }
    else
    {
        // v4+
        TraceModule * pTraceModule;
        UINT32        moduleOffset;
        string        filename;

        if ((OK != ParseModule(&pTraceModule)) ||
            (OK != ParseUINT32(&moduleOffset)) ||
            (OK != ParseFileName(&filename, "UPDATE_FILE")))
        {
            ErrPrintf("UPDATE_FILE requires <dest name> <dest offset> <update_filename> args.\n");
            return RC::ILWALID_FILE_FORMAT;
        }

        CHECK_RC(RequireModLocal(pTraceModule, "UPDATE_FILE"));

        TraceOpUpdateFile * pOp = new TraceOpUpdateFile(
                pTraceModule,
                moduleOffset,
                m_TraceFileMgr,
                filename,
                flushCache,
                m_Test->GetBoundGpuDevice(),
                m_TimeoutMs);

        InsertOp(m_LineNumber, pOp, OP_SEQUENCE_UPDATE_FILE);
    }
    m_HasFileUpdates = true;
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseMETHODS()
{
    RC rc;
    CHECK_RC(RequireVersion(3, "METHODS"));

    TraceChannel * pTraceChannel;
    TraceModule * module;

    if ((OK != ParseChannel(&pTraceChannel, CPU_MANAGED)) ||
        (OK != ParseModule(&module)))
    {
        ErrPrintf("METHODS requires <channel name> <FILE name> <opt_start_offset> <opt_num_bytes> args.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    CHECK_RC(RequireModLocal(module, "METHODS"));

    if (pTraceChannel->GetVASpaceHandle() != module->GetVASpaceHandle())
    {
        ErrPrintf("push buffer is not in the same vaspace of channel, please check test.hdr!\n");
        ErrPrintf("Module %s, VAS %d\n", module->GetName().c_str(), module->GetVASpaceHandle());
        ErrPrintf("Channel %s, VAS %d\n", pTraceChannel->GetName().c_str(), pTraceChannel->GetVASpaceHandle());
        return  RC::ILWALID_FILE_FORMAT;
    }

    if (OK != (rc = module->SetTraceChannel(pTraceChannel)))
    {
        ErrPrintf("A FILE may be used with only one CHANNEL.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    UINT32 segStart = 0;
    UINT32 segSize = module->GetSize();

    UINT32 tmp;
    if (OK == ParseUINT32(&tmp))
    {
        segStart = tmp;
        segSize -= segStart;

        if (OK == ParseUINT32(&tmp) && tmp <= segSize)
            segSize = tmp;
        else
        {
            ErrPrintf("METHODS <channel> <FILE> <start> <size> -- size is invalid!\n");
            return RC::ILWALID_FILE_FORMAT;
        }

    }

    TraceOpSendMethods * pOp = new TraceOpSendMethods(
            module,
            segStart,
            segSize);

    if (! pOp->SegmentIsValid())
    {
        ErrPrintf("METHODS <channel> <FILE> <start> <size> -- start or size is invalid!\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    InsertOp(m_LineNumber, pOp, OP_SEQUENCE_METHODS);

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseCHANNEL(bool isAllocCommand)
{
    RC rc;
    CHECK_RC(RequireVersion(3, "CHANNEL"));

    TraceChannel * pTraceChannel;
    string ch_name;

    if (OK == ParseChannel(&pTraceChannel))
    {
        ErrPrintf("CHANNEL \"%s\" is already declared.\n", pTraceChannel->GetName().c_str());
        return RC::ILWALID_FILE_FORMAT;
    }
    if (OK != ParseString(&ch_name))
    {
        ErrPrintf("CHANNEL requires a <name> arg.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    // <TSG name> <GPU_MANAGED> are optional
    string tok, tsgName, scgTypeName, subctxName, vasName;
    bool isGpuManagedCh = false;
    bool isPreemptive = false;

    bool gpFifoEntriesPresent = false;
    bool pushbufferSizePresent = false;
    UINT32 gpFifoEntries, pbdma = 0;
    UINT64 pushbufferSize;

    while (OK == ParseString(&tok))
    {
        if (tok == "GPU_MANAGED")
        {
            // Gpu managed channel
            isGpuManagedCh = true;
        }
        else if (tok == "PREEMPTIVE")
        {
            isPreemptive = true;
        }
        else if (tok == "SHARE_CONTEXT")
        {
            ErrPrintf("CHANNEL SHARE_CONTEXT is not supported any more.\n");
            return RC::ILWALID_FILE_FORMAT;
        }
        else if (tok == "GPFIFO_ENTRIES")
        {
            if (OK != ParseUINT32(&gpFifoEntries))
            {
                ErrPrintf("CHANNEL GPFIFO_ENTRIES requires a <size> arg.\n");
                return RC::ILWALID_FILE_FORMAT;
            }
            gpFifoEntriesPresent = true;
        }
        else if (tok == "PUSHBUFFER_SIZE")
        {
            if (OK != ParseUINT64(&pushbufferSize))
            {
                ErrPrintf("CHANNEL PUSHBUFFER_SIZE requires a <size> arg.\n");
                return RC::ILWALID_FILE_FORMAT;
            }
            pushbufferSizePresent = true;
        }
        else if (tok == "SCG_TYPE")
        {
            if (OK != ParseString(&scgTypeName) ||
                ((scgTypeName != "GRAPHICS_COMPUTE0") &&
                 (scgTypeName != "COMPUTE1")))
            {
                ErrPrintf("CHANNEL SCG_TYPE(%s) is ilwalide.\n", scgTypeName.c_str());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (tok == "PBDMA")
        {
            if (OK != ParseUINT32(&pbdma))
            {
                ErrPrintf("CHANNEL PBDMA requires a <index> arg.\n");
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (tok == "SUBCONTEXT")
        {
            if (OK != ParseString(&subctxName))
            {
                ErrPrintf("CHANNEL SUBCONTEXT requires a <subctx name> arg.\n");
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (tok == "ADDRESS_SPACE")
        {
            if(OK != ParseString(&vasName))
            {
                ErrPrintf("CHANNEL ADDRESS_SPACE requires a <vas name> arg.\n");
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else
        {
            if (!tsgName.empty())
            {
                ErrPrintf("CHANNEL can't support more than one <tsg name>.\n");
                return RC::ILWALID_FILE_FORMAT;
            }
            tsgName = tok;
        }
    }

    shared_ptr<SubCtx> pSubCtx;
    pSubCtx = GetSubCtx(subctxName);
    tsgName = params->ParamStr("-tsg_name", tsgName.c_str());
    CHECK_RC(SanityCheckTSG(pSubCtx, &tsgName));

    const string &name = vasName.empty() ? "default" : vasName;
    shared_ptr<VaSpace> pVaSpace;
    if (pSubCtx.get())
    {
        if (tsgName.empty())
        {
            ErrPrintf("need a tsg for channel %s subcontext %s\n",
                      ch_name.c_str(), (pSubCtx->GetName()).c_str());
            return RC::ILWALID_FILE_FORMAT;
        }

        if (!pSubCtx->GetVaSpace() || !vasName.empty())
        {
            CHECK_RC(pSubCtx->SetVaSpace(GetVAS(name)));
        }

        pVaSpace = pSubCtx->GetVaSpace();
    }
    else
    {
        pVaSpace = GetVAS(name);
    }

    ChannelManagedMode mode = isGpuManagedCh ? GPU_MANAGED : CPU_MANAGED;

    CHECK_RC(AddChannel(ch_name, tsgName, mode, pVaSpace, pSubCtx));

    TraceChannel *ch = GetChannel(ch_name);
    MASSERT(ch);

    if (isPreemptive)
    {
        ch->SetPreemptive(true);
    }

    if (gpFifoEntriesPresent)
    {
        ch->SetGpFifoEntries(gpFifoEntries);
    }

    if (pushbufferSizePresent)
    {
        ch->SetPushbufferSize(pushbufferSize);
    }

    LWGpuChannel::SCGType scgType;
    if (scgTypeName == "COMPUTE1")
    {
        scgType = LWGpuChannel::COMPUTE1;
    }
    else
    {
        scgType = LWGpuChannel::GRAPHICS_COMPUTE0;
    }
    scgType = static_cast<LWGpuChannel::SCGType>
        (params->ParamUnsigned("-scg_type", scgType));
    CHECK_RC(ch->SetSCGType(scgType));

    if (params->ParamPresent("-pbdma"))
    {
        pbdma = params->ParamUnsigned("-pbdma", pbdma);
    }
    else
    {
        for(UINT32 i = 0; i < params->ParamPresent("-channel_pbdma"); ++i)
        {
            if(params->ParamNStr("-channel_pbdma", i, 0) == ch_name)
            {
                pbdma = params->ParamNUnsigned("-channel_pbdma", i, 1);
            }
        }
    }

    MASSERT(pbdma < 2);
    ch->SetPbdma(pbdma);
    if (isAllocCommand)
    {
        CHECK_RC(RequireVersion(5, "ALLOC_CHANNEL"));

        ch->SetIsDynamic();

        TraceOpAllocChannel* pOp;
        pOp= new TraceOpAllocChannel(m_Test, ch);
        InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);
    }
    return rc;
}

RC GpuTrace::SetCeType(TraceChannel *pTraceChannel,
                       TraceSubChannel *pTraceSubChannel,
                       UINT32 ceType,
                       GpuSubdevice *subdev)
{
    RC rc;

    // Two steps here.
    // 1. Select correct ceType from trace_3d arguments considering priority
    std::unique_ptr<CEChooser> ceChooser(
        new CEChooser(this, params, pTraceChannel, pTraceSubChannel, ceType, subdev));

    CHECK_RC(ceChooser->Execute());

    pTraceSubChannel->SetCeType(ceChooser->GetCeType());

    // 2. Add a default TSG and set PBDMA if necessary
    // for a a-sync CE, nothing to do
    // for a GRCE, if channel has only one subchannel and the chip can support multiple pbdma
    // MODS will auto add TSG if no TSG is found and set pbdma
    UINT32 runQueue = 0;
    if (ceChooser->IsGrCE())
    {
        CHECK_RC(ceChooser->GetGrCERunQueueID(&runQueue));
        if (!pTraceChannel->HasMultipleSubChs() &&
            (subdev->GetNumGraphicsRunqueues() > 1))
        {
            if (pTraceChannel->GetTraceTsg() == 0)
            {
                InfoPrintf("Add channel %s into a default TSG\n",
                            pTraceChannel->GetName().c_str());

                string tsgName(NewTsgName());
                AddTraceTsg(tsgName, pTraceChannel->GetVASpace(), true);
                TraceTsg * pTraceTsg = GetTraceTsg(tsgName);
                if(0 == pTraceTsg)
                {
                    ErrPrintf("%s: Can't find the TSG specified by name: %s.\n",
                              __FUNCTION__, tsgName.c_str());
                    return RC::SOFTWARE_ERROR;
                }
                CHECK_RC(pTraceChannel->SetTraceTsg(pTraceTsg));
                CHECK_RC(pTraceTsg->AddTraceChannel(pTraceChannel));
            }
            if (pTraceChannel->GetPbdma() != runQueue)
            {
                InfoPrintf("Pbdma is changed from %u to %u in channel %s\n",
                            pTraceChannel->GetPbdma(), runQueue, pTraceChannel->GetName().c_str());
                pTraceChannel->SetPbdma(runQueue);
            }
        }
        else
        {
            if (pTraceChannel->HasMultipleSubChs())
            {
                InfoPrintf("Channel %s has multiple subchannels, "
                           "so MODS will not add a new TSG and auto bind it to pbdma\n",
                           pTraceChannel->GetName().c_str());
            }
            if (subdev->GetNumGraphicsRunqueues() == 1)
            {
                InfoPrintf("Chip only support one PBDMA "
                           "so MODS will not a new default TSG and auto bind it to pbdma\n");
            }

        }
    }

    ceChooser->PrintInfo();

    return OK;
}

RC GpuTrace::GetEnginesList
(
    TraceSubChannel *pTraceSubChannel,
    vector<UINT32>* engineList
)
{
    RC rc;

    vector<UINT32> classIds;
    GpuDevice *pGpuDev = m_Test->GetBoundGpuDevice();
    classIds.push_back(pTraceSubChannel->GetClass());
    CHECK_RC(pGpuDev->GetEnginesFilteredByClass(classIds, engineList, m_Test->GetLwRmPtr()));

    if (engineList->empty())
    {
        ErrPrintf("%s: No supported engine found. \n", __FUNCTION__);
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    return rc;
}

RC GpuTrace::SetLwDecOffset
(
    TraceSubChannel *pTraceSubChannel
)
{
    RC rc;
    vector<UINT32> engineList;

    CHECK_RC(GetEnginesList(pTraceSubChannel, &engineList));
    InfoPrintf("Supported LwDec count is %d\n", (UINT32)engineList.size());

    if (engineList.empty())
    {
        ErrPrintf("engineList for LwDec is empty \n");
        return RC::ILWALID_FILE_FORMAT;
    }

    /* using engineList[0] as default */
    UINT32 finalLwDecOffset = MDiagUtils::GetLwDecEngineOffset(engineList[0]);
    if (params->ParamPresent("-alloc_lwdec_rr") > 0)
    {
        UINT32 idx = (m_LwDecOffset) % engineList.size();
        finalLwDecOffset = MDiagUtils::GetLwDecEngineOffset(engineList[idx]);
        m_LwDecOffset = (idx + 1) % engineList.size();
    }
    pTraceSubChannel->SetLwDecOffset(finalLwDecOffset);

    return rc;
}

RC GpuTrace::SetLwEncOffset
(
    TraceSubChannel *pTraceSubChannel
)
{
    RC rc;
    vector<UINT32> engineList;

    CHECK_RC(GetEnginesList(pTraceSubChannel, &engineList));
    InfoPrintf("Supported LwEnc count is %d\n", (UINT32)engineList.size());

    if (engineList.empty())
    {
        ErrPrintf("engineList for LwEnc is empty \n");
        return RC::ILWALID_FILE_FORMAT;
    }

    /* using engineList[0] as default */
    UINT32 finalLwEncOffset = MDiagUtils::GetLwEncEngineOffset(engineList[0]);
    if (params->ParamPresent("-alloc_lwenc_rr") > 0)
    {
        UINT32 idx = (m_LwEncOffset) % engineList.size();
        finalLwEncOffset = MDiagUtils::GetLwEncEngineOffset(engineList[idx]);
        m_LwEncOffset = (idx + 1) % engineList.size();
    }
    pTraceSubChannel->SetLwEncOffset(finalLwEncOffset);

    return rc;
}

RC GpuTrace::SetLwJpgOffset
(
    TraceSubChannel *pTraceSubChannel
)
{
    RC rc;
    vector<UINT32> engineList;

    CHECK_RC(GetEnginesList(pTraceSubChannel, &engineList));
    InfoPrintf("Supported LwJpg count is %d\n", (UINT32)engineList.size());

    if (engineList.empty())
    {
        ErrPrintf("engineList for LwJpg is empty \n");
        return RC::ILWALID_FILE_FORMAT;
    }

    /* using engineList[0] as default */
    UINT32 finalLwJpgOffset = MDiagUtils::GetLwJpgEngineOffset(engineList[0]);
    if (params->ParamPresent("-alloc_lwjpg_rr") > 0)
    {
        UINT32 idx = (m_LwJpgOffset) % engineList.size();
        finalLwJpgOffset = MDiagUtils::GetLwJpgEngineOffset(engineList[idx]);
        m_LwJpgOffset = (idx + 1) % engineList.size();
    }
    pTraceSubChannel->SetLwJpgOffset(finalLwJpgOffset);

    return rc;
}

RC GpuTrace::ParseSUBCHANNEL(bool isAllocCommand)
{
    RC rc;
    CHECK_RC(RequireVersion(3, "SUBCHANNEL"));

    TraceChannel* pTraceChannel;
    string ch_name;
    string subch_name;
    UINT32 val;
    UINT32 ceType = CEObjCreateParam::UNSPECIFIED_CE;

    if (OK != ParseString( &subch_name ))
    {
        ErrPrintf("SUBCHANNEL requires <subch_name channel_name subch_number> args.\n");
        return RC::ILWALID_FILE_FORMAT;
    }
    if (OK != ParseChannel(&pTraceChannel))
    {
        ErrPrintf("CHANNEL \"%s\" has not been declared yet.\n", m_TestHdrTokens.front());
        return RC::ILWALID_FILE_FORMAT;
    }
    if (GetSubChannel(pTraceChannel, subch_name))
    {
        ErrPrintf("SUBCHANNEL \"%s\" is already decleared for channel %s.\n",
                  subch_name.c_str(), ch_name.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }
    TraceSubChannel* pTraceSubChannel = pTraceChannel->AddSubChannel(subch_name, params);

    if ( OK != ParseUINT32( &val ) )
    {
        ErrPrintf("SUBCHANNEL requires subchannel number.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    //
    // Parse optional argument: [USE_TRACE_SUBCHNUM] [GRCOPY] [CEx] [CE_number]
    //
    bool bUseTraceSubchNum = params->ParamPresent("-use_trace_subchnum") > 0;
    while (ParseFile::s_NewLine != m_TestHdrTokens.front())
    {
        const char *tok = m_TestHdrTokens.front();
        if (0 == strcmp(tok, "USE_TRACE_SUBCHNUM"))
        {
            // consume the token
            m_TestHdrTokens.pop_front();
            bUseTraceSubchNum = true;
        }
        else if (0 == strcmp(tok, "GRCOPY"))
        {
            ceType = CEObjCreateParam::GRAPHIC_CE;
            m_TestHdrTokens.pop_front();
        }
        else if (1 == sscanf(tok, "CE%d", &ceType))
        {
            m_TestHdrTokens.pop_front();
        }
        else if (OK == ParseUINT32(&ceType))
        {
        }
        else
        {
            ErrPrintf("SUBCHANNEL can't identify the argument %s.\n", tok);
            return RC::ILWALID_FILE_FORMAT;
        }
    }

    if (ceType != CEObjCreateParam::UNSPECIFIED_CE)
    {
        // save ce type specified by trace to TraceSubChannel
        // ParseCLASS() will set final ce type if it's ce class
        pTraceSubChannel->SetCeType(ceType);
    }

    pTraceSubChannel->SetTraceSubChNum(val);
    pTraceSubChannel->SetUseTraceSubchNum(bUseTraceSubchNum);

    if (isAllocCommand)
    {
        CHECK_RC(RequireVersion(5, "ALLOC_SUBCHANNEL"));
        if (!pTraceChannel->IsDynamic())
        {
            ErrPrintf("ALLOC_SUBCHANNEL does not support to create subchannel \"%s\" on non-dynamic channel \"%s\"\n",
                      subch_name.c_str(), pTraceChannel->GetName().c_str());
            return RC::ILWALID_FILE_FORMAT;
        }

        pTraceSubChannel->SetIsDynamic();

        TraceOpAllocSubChannel* pOp;
        pOp= new TraceOpAllocSubChannel(m_Test, pTraceChannel, pTraceSubChannel);
        InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);
    }

    return rc;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseFREE_CHANNEL()
{
    RC rc;
    CHECK_RC(RequireVersion(5, "FREE_CHANNEL"));

    // Parse name
    TraceChannel* pTraceChannel;
    if (OK != ParseChannel(&pTraceChannel))
    {
        ErrPrintf("CHANNEL \"%s\" is not declared.\n", m_TestHdrTokens.front());
        return RC::ILWALID_FILE_FORMAT;
    }

    if (!pTraceChannel->IsDynamic())
    {
        ErrPrintf("FREE_CHANNEL does not support non-dynamic channel \"%s\".\n",
                  pTraceChannel->GetName().c_str());
        return RC::ILWALID_FILE_FORMAT;
    }

    // Insert trace op to list
    TraceOpFreeChannel* pOp;
    pOp= new TraceOpFreeChannel(m_Test, pTraceChannel);
    InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);

    pTraceChannel->ClearIsValidInParsing();

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseFREE_SUBCHANNEL()
{
    RC rc;
    CHECK_RC(RequireVersion(5, "FREE_SUBCHANNEL"));

    // Parse name
    string subChannelName;
    TraceChannel* pTraceChannel;
    if (OK != ParseString(&subChannelName))
    {
        ErrPrintf("FREE_SUBCHANNEL requires <subch_name channel_name> args.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    if (OK != ParseChannel(&pTraceChannel))
    {
        ErrPrintf("Channel \"%s\" has not been declared yet.\n", m_TestHdrTokens.front());
        return RC::ILWALID_FILE_FORMAT;
    }

    TraceSubChannel* pTraceSubChannel = GetSubChannel(pTraceChannel, subChannelName);
    if (pTraceSubChannel == 0)
    {
        ErrPrintf("SUBCHANNEL \"%s\" is not found for channel %s.\n",
                  subChannelName.c_str(), pTraceChannel->GetName().c_str());
        return RC::ILWALID_FILE_FORMAT;
    }

    if (!pTraceSubChannel->IsDynamic())
    {
        ErrPrintf("FREE_SUBCHANNEL does not support non-dynamic subchannel \"%s\".\n", subChannelName.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }

    // Insert trace op to list
    TraceOpFreeSubChannel* pOp;
    pOp= new TraceOpFreeSubChannel(m_Test, pTraceChannel, pTraceSubChannel);
    InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);

    pTraceSubChannel->ClearIsValidInParsing();

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseTIMESLICE()
{
    RC rc;
    CHECK_RC(RequireVersion(3, "TIMESLICE"));

    TraceChannel * pTraceChannel;
    UINT32 val;

    if ((OK != ParseChannel(&pTraceChannel, CPU_MANAGED)) ||
        (OK != ParseUINT32(&val)))
    {
        ErrPrintf("TIMESLICE requires <channel name> <uint32> args.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    pTraceChannel->SetTimeSlice(val);

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseTRUSTED_CTX()
{
    TraceChannel * pTraceChannel = 0;
    TraceSubChannel* pTraceSubCh = 0;

    if (m_Version >= 3)
    {
        // Version 3 takes a channel name.
        if ( (OK != ParseSubChannel(&pTraceChannel, &pTraceSubCh)) &&
            (OK != ParseChannel(&pTraceChannel, CPU_MANAGED)))
        {
            ErrPrintf("TRUSTED_CTX requires <channel/subchannel name> arg.\n");
            return RC::ILWALID_FILE_FORMAT;
        }
        else if ( !pTraceSubCh )
        {
            pTraceSubCh = GetSubChannel(pTraceChannel, "");
        }
    }
    else
    {
        pTraceChannel = GetLwrrentChannel(CPU_MANAGED);
        if (pTraceChannel == NULL)
        {
            ErrPrintf("TRUSTED_CTX seen before CLASS.\n");
            return RC::ILWALID_FILE_FORMAT;
        }
        pTraceSubCh = GetSubChannel(pTraceChannel, "");
        MASSERT( pTraceSubCh );
    }

    pTraceSubCh->SetTrustedContext();

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseRUNLIST()
{
    RC rc;
    CHECK_RC(RequireVersion(3, "RUNLIST"));

    list<TraceChannel *> run_list;

    while (ParseFile::s_NewLine != m_TestHdrTokens.front())
    {
        TraceChannel * pTraceChannel;
        if (OK != ParseChannel(&pTraceChannel, CPU_MANAGED))
        {
            ErrPrintf("RUNLIST \"%s\" is not a valid channel name.\n", m_TestHdrTokens.front());
            return RC::ILWALID_FILE_FORMAT;
        }
        run_list.push_back(pTraceChannel);
    }
    if (0 == run_list.size())
    {
        ErrPrintf("RUNLIST requires 1 or more <channel name> args.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    //TODO
    WarnPrintf("RUNLIST is not yet implemented by trace_3d, see bug 204426.\n");
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseFLUSH()
{
    RC rc;
    CHECK_RC(RequireVersion(4, "FLUSH"));

    TraceChannel * pTraceChannel;

    if (OK != ParseChannel(&pTraceChannel, CPU_MANAGED))
    {
        ErrPrintf("FLUSH requires a <channel name> arg.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    TraceOpFlush * pOp = new TraceOpFlush(
            pTraceChannel);
    InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);

    UpdateTraceChannelMaxGpEntries(pTraceChannel);

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseWAIT_FOR_VALUE32()
{
    RC rc;
    CHECK_RC(RequireVersion(4, "WAIT_FOR_VALUE32"));

    TraceModule * pTraceModule;
    UINT32        moduleOffset;
    int           boolFunc;
    UINT32        refValue;

    if ((OK != ParseModule(&pTraceModule)) ||
        (OK != ParseUINT32(&moduleOffset)) ||
        (OK != ParseBoolFunc(&boolFunc)) ||
        (OK != ParseUINT32(&refValue)))
    {
        ErrPrintf("WAIT_FOR_VALUE32 requires <FILE name> <offset> <bool-func> <ref-value> args.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    CHECK_RC(RequireModLocal(pTraceModule, "WAIT_FOR_VALUE32"));

    TraceOpWaitValue32 * pOp = new TraceOpWaitValue32(
            pTraceModule,
            moduleOffset,
            (TraceOp::BoolFunc) boolFunc,
            refValue,
            m_TimeoutMs);
    InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseSEC_KEY()
{
    RC rc;
    CHECK_RC(RequireModLast("SEC_KEY"));
    CHECK_RC(RequireVersion(4, "SEC_KEY"));

    TraceModule * pTraceModule;
    UINT32        moduleOffset;
    string        sessionOrData;

    if ((OK != ParseModule(&pTraceModule)) ||
        (OK != ParseUINT32(&moduleOffset)) ||
        (OK != ParseString(&sessionOrData)) ||
        (0 != strcmp(sessionOrData.c_str(), "SESSION") &&
         0 != strcmp(sessionOrData.c_str(), "CONTENT")) )
        {
            ErrPrintf("SEC_KEY requires <dest name> <dest offset> <SESSION or CONTENT> args. CONTENT mode requires <content_key> arg\n");
            return RC::ILWALID_FILE_FORMAT;
        }

    CHECK_RC(RequireModLocal(pTraceModule, "SEC_KEY"));

    bool isContent = (0 == strcmp(sessionOrData.c_str(), "CONTENT")) ;
    string contentKey ;

    if ( isContent && (OK != ParseString(&contentKey)) ) {
        ErrPrintf("SEC_KEY : Missing Content Key in CONTENT mode.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    if (params->ParamPresent("-require_session_key"))
    {
        TraceOpSec * pOp = new TraceOpSec(
            pTraceModule,
            moduleOffset,
            isContent,
            contentKey.c_str(),
            0) ;
        InsertOp(m_LineNumber, pOp, 0);
    } else {
        InfoPrintf("SEC_KEY: IGNORING SEC_KEY TraceOps in test.hdr . Rerun with -require_session_key to ENABLE.\n") ;
    }
    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseSYSMEM_CACHE_CTRL()
{
    RC rc;
    UINT32 pb_offset;
    string opStr;
    UINT32 cmdId             = 0;
    bool   bAddToPmuTraceOps = false;
    UINT32 opPowerState      = LW208F_CTRL_FB_CTRL_GPU_CACHE_POWER_STATE_DEFAULT;
    UINT32 opWriteMode       = LW208F_CTRL_FB_CTRL_GPU_CACHE_WRITE_MODE_DEFAULT;
    UINT32 opRCMState        = LW208F_CTRL_FB_CTRL_GPU_CACHE_RCM_STATE_DEFAULT;
    UINT32 opVGAMode         = LW208F_CTRL_FB_CTRL_GPU_CACHE_VGA_MODE_DEFAULT;

    CHECK_RC(RequireNotVersion(3, "SYSMEM_CACHE_CTRL"));

    if (m_Version < 3)
    {
        if (OK != ParseUINT32(&pb_offset))
        {
            ErrPrintf("Syntax: SYSMEM_CACHE_CTRL pb_offset ctrl_flag for V%d trace\n", m_Version);
            return RC::ILWALID_FILE_FORMAT;
        }
    }
    if (OK != ParseString(&opStr))
    {
        ErrPrintf("Syntax: SYSMEM_CACHE_CTRL ctrl_flag for V4 trace\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    if (opStr == "ENABLE")
        opPowerState = LW208F_CTRL_FB_CTRL_GPU_CACHE_POWER_STATE_ENABLED;
    else if (opStr == "DISABLE")
        opPowerState = LW208F_CTRL_FB_CTRL_GPU_CACHE_POWER_STATE_DISABLED;
    else if (opStr == "WB")
        opWriteMode = LW208F_CTRL_FB_CTRL_GPU_CACHE_WRITE_MODE_WRITEBACK;
    else if (opStr == "WT")
        opWriteMode = LW208F_CTRL_FB_CTRL_GPU_CACHE_WRITE_MODE_WRITETHROUGH;
    else if (opStr == "RCM_FULL")
        opRCMState = LW208F_CTRL_FB_CTRL_GPU_CACHE_RCM_STATE_FULL;
    else if (opStr == "RCM_REDUCED")
        opRCMState = LW208F_CTRL_FB_CTRL_GPU_CACHE_RCM_STATE_REDUCED;
    else if (opStr == "RCM_ZERO_CACHE")
        opRCMState = LW208F_CTRL_FB_CTRL_GPU_CACHE_RCM_STATE_ZERO_CACHE;
    else if (opStr == "VGA_ENABLE")
        opVGAMode = LW208F_CTRL_FB_CTRL_GPU_CACHE_VGA_MODE_ENABLED;
    else if (opStr == "VGA_DISABLE")
        opVGAMode = LW208F_CTRL_FB_CTRL_GPU_CACHE_VGA_MODE_DISABLED;
    else
    {
        ErrPrintf("SYSMEM_CACHE_CTRL: Invalid control op\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    if (ParseFile::s_NewLine != m_TestHdrTokens.front())
    {
        CHECK_RC(ParsePmuCmdId("SYSMEM_CACHE_CTRL", &cmdId));
        bAddToPmuTraceOps = true;
    }

    TraceOpSysMemCacheCtrl *pOp = new TraceOpSysMemCacheCtrl(opWriteMode,
        opPowerState, opRCMState, opVGAMode,
        m_Test->GetBoundGpuDevice()->GetSubdevice(0), cmdId, m_Test->GetLwRmPtr());

    if (bAddToPmuTraceOps)
        m_PmuCmdTraceOps.push_back(pOp);

    if (m_Version < 3)
        InsertOp(pb_offset, pOp, OP_SEQUENCE_OTHER);
    else
        InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseFLUSH_CACHE_CTRL()
{
    StickyRC rc = OK;
    UINT32 pb_offset = 0;
    string surf;
    string writeback;
    string ilwalidate;
    string mode;
    UINT32 writeback_val = 0;
    UINT32 ilwalidate_val = 0;
    UINT32 mode_val = 0;

    CHECK_RC(RequireNotVersion(3, "FLUSH_CACHE_CTRL"));
    if (m_Version < 3)
    {
        if (ParseUINT32(&pb_offset))
        {
            ErrPrintf("Syntax: FLUSH_CACHE_CTRL pb_offset surf_name write_back ilwalidate mode for V%d trace\n", m_Version);
            return RC::ILWALID_FILE_FORMAT;
        }
    }

    if (ParseString(&surf)       != OK ||
        ParseString(&writeback)  != OK ||
        ParseString(&ilwalidate) != OK ||
        ParseString(&mode)       != OK)
    {
        ErrPrintf("Syntax: FLUSH_CACHE_CTRL surf_name write_back ilwalidate mode\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    if (writeback == "NO")
        writeback_val = LW2080_CTRL_FB_FLUSH_GPU_CACHE_FLAGS_WRITE_BACK_NO;
    else if (writeback == "YES")
        writeback_val = LW2080_CTRL_FB_FLUSH_GPU_CACHE_FLAGS_WRITE_BACK_YES;
    else
        rc = RC::ILWALID_FILE_FORMAT;

    if (ilwalidate == "NO")
        ilwalidate_val = LW2080_CTRL_FB_FLUSH_GPU_CACHE_FLAGS_ILWALIDATE_NO;
    else if (ilwalidate == "YES")
        ilwalidate_val = LW2080_CTRL_FB_FLUSH_GPU_CACHE_FLAGS_ILWALIDATE_YES;
    else
        rc = RC::ILWALID_FILE_FORMAT;

    if (mode == "ADDRESS_ARRAY")
        mode_val = LW2080_CTRL_FB_FLUSH_GPU_CACHE_FLAGS_FLUSH_MODE_ADDRESS_ARRAY;
    else if (mode == "FULL_CACHE")
        mode_val = LW2080_CTRL_FB_FLUSH_GPU_CACHE_FLAGS_FLUSH_MODE_FULL_CACHE;
    else
        rc = RC::ILWALID_FILE_FORMAT;

    if (rc != OK)
    {
        ErrPrintf("FLUSH_CACHE_CTRL: Invalid arguments\n");
        return rc;
    }

    TraceOp *pOp = new TraceOpFlushCacheCtrl(surf, writeback_val, ilwalidate_val,
        mode_val, m_Test, m_Test->GetBoundGpuDevice()->GetSubdevice(0));
    if (m_Version < 3)
        InsertOp(pb_offset, pOp, OP_SEQUENCE_OTHER);
    else
        InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseENABLE_CHANNEL()
{
    RC rc;
    CHECK_RC(RequireVersion(4, "ENABLE_CHANNEL"));

    TraceChannel * pTraceChannel;

    if (OK != ParseChannel(&pTraceChannel, CPU_MANAGED))
    {
        ErrPrintf("ENABLE_CHANNEL requires a <channel name> arg.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    TraceOpSetChannel * pOp = new TraceOpSetChannel(pTraceChannel, true);
    InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseDISABLE_CHANNEL()
{
    RC rc;
    CHECK_RC(RequireVersion(4, "DISABLE_CHANNEL"));

    TraceChannel * pTraceChannel;

    if (OK != ParseChannel(&pTraceChannel, CPU_MANAGED))
    {
        ErrPrintf("DISABLE_CHANNEL requires a <channel name> arg.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    TraceOpSetChannel * pOp = new TraceOpSetChannel(pTraceChannel, false);
    InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseWAIT_PMU_CMD()
{
    UINT32        pbufOffset;
    UINT32        cmdId;
    UINT32        timeoutMs = TraceOp::TIMEOUT_NONE;
    RC            rc;

    CHECK_RC(RequireNotVersion(3, "WAIT_PMU_CMD"));

    if (m_Version < 3)
    {
        if (OK != ParseUINT32(&pbufOffset))
        {
            ErrPrintf("WAIT_PMU_CMD : <pbuf_offset> <cmd_id> [timeout]\n");
            return RC::ILWALID_FILE_FORMAT;
        }
    }
    if (OK != ParseUINT32(&cmdId))
    {
        ErrPrintf("WAIT_PMU_CMD : <cmd_id> expected\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    TraceOpPmu *pPmuOp = FindPmuOp(cmdId);
    if (!pPmuOp)
    {
        ErrPrintf("WAIT_PMU_CMD: Unknown PMU Command ID %d\n", cmdId);
        return RC::ILWALID_FILE_FORMAT;
    }

    if ((ParseFile::s_NewLine != m_TestHdrTokens.front()) &&
        (OK != ParseUINT32(&timeoutMs)))
    {
        ErrPrintf("WAIT_PMU_CMD: timeout expected\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    TraceOpWaitPmuCmd *pOp = new TraceOpWaitPmuCmd(cmdId, timeoutMs, m_Test->GetLwRmPtr());
    pPmuOp->SetWaitOp(pOp);

    if (m_Version < 3)
        InsertOp(pbufOffset, pOp, OP_SEQUENCE_OTHER);
    else
        InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);

    return rc;
}

RC GpuTrace::ParsePmuCmdId(const char *cmd, UINT32 *pCmdId)
{
    string pmuCmdIdStr;

    if ((OK != ParseString(&pmuCmdIdStr)) ||
        (pmuCmdIdStr != "PMU_CMD_ID") ||
        (OK != ParseUINT32(pCmdId)))
    {
        ErrPrintf("%s : optional [PMU_CMD_ID <cmdId>] expected\n", cmd);
        return RC::ILWALID_FILE_FORMAT;
    }

    if (FindPmuOp(*pCmdId))
    {
        ErrPrintf("%s : PMU_CMD_ID %d already exists.\n", cmd, *pCmdId);
        return RC::ILWALID_FILE_FORMAT;
    }

    return OK;
}

RC GpuTrace::ParsePMTriggerEvent()
{
    const char *syntax = "Syntax: PMTRIGGER_EVENT <ON|OFF> [name] [CHANNEL channel_name].\n";
    string value;
    string name;
    TraceChannel *channel = 0;

    if (OK != ParseString(&value))
    {
        ErrPrintf(syntax);
        return RC::ILWALID_FILE_FORMAT;
    }

    string temp;
    while (ParseFile::s_NewLine != m_TestHdrTokens.front())
    {
        MASSERT(ParseString(&temp) == OK);
        if (temp == "CHANNEL")
        {
            if (ParseChannel(&channel, CPU_MANAGED) != OK)
            {
                ErrPrintf(syntax);
                ErrPrintf("Missing or invalid channel_name.\n");
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else // name
        {
            name = temp;
        }
    }

    if (value == "ON")
    {
        ++m_numPmTriggers;
        if (name.empty())
        {
            name = Utility::StrPrintf("PMTRIGGER_EVENT_%u", m_numPmTriggers);
        }
    }

    if (channel == 0)
    {
        channel = GetLwrrentChannel(CPU_MANAGED);
        if (channel == 0)
        {
            ErrPrintf("PMTRIGGER_EVENT used before CHANNEL declaration.\n");
            return RC::ILWALID_FILE_FORMAT;
        }
    }

    // This trace op is used to tell TDBG and other trace plugins about
    // event commands in the trace header.  The other event-related trace ops
    // aren't necessarily added to the list as they may depend on
    // command-line arguments to be activated.
    TraceOp *pMarkerOp = new TraceOpEventMarker(m_Test, name,
                                                "PMTRIGGER_EVENT_" + value,
                                                channel, false);
    InsertOp(m_LineNumber, pMarkerOp, OP_SEQUENCE_OTHER);

    TraceOpEvent *pOp;
    if (value == "ON")
    {
        pOp = new TraceOpEventPMStart(m_Test, channel, name);
    }
    else if (value == "OFF")
    {
        pOp = new TraceOpEventPMStop(m_Test, channel, name);
    }
    else
    {
        ErrPrintf(syntax);
        ErrPrintf("Invalid PMTRIGGER_EVENT option: %s.\n", value.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }
    InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);

    pMarkerOp = new TraceOpEventMarker(m_Test, name,
                                       "PMTRIGGER_EVENT_" + value,
                                       channel, true);
    InsertOp(m_LineNumber, pMarkerOp, OP_SEQUENCE_OTHER);

    return OK;
}

RC GpuTrace::ParsePMTriggerSyncEvent()
{
    const char *syntax = "Syntax: PMTRIGGER_SYNC_EVENT <ON|OFF> [name] "
        "[CHANNEL channel_name] [SUBCHANNEL subchannel_name].\n";
    string value;
    string name;
    string subchName;
    TraceChannel *channel = NULL;
    TraceSubChannel *subchannel = NULL;

    if (params->ParamPresent("-pm_file"))
    {
        ErrPrintf("PMTRIGGER_SYNC_EVENT only work with -pm_sb_file on FModel/RTL/Silicon\n");
        return RC::ILWALID_ARGUMENT;
    }

    if (OK != ParseString(&value))
    {
        ErrPrintf(syntax);
        return RC::ILWALID_FILE_FORMAT;
    }

    string temp;
    while (ParseFile::s_NewLine != m_TestHdrTokens.front())
    {
        MASSERT(ParseString(&temp) == OK);
        if (temp == "CHANNEL")
        {
            if (ParseChannel(&channel, CPU_MANAGED) != OK)
            {
                ErrPrintf(syntax);
                ErrPrintf("Missing or invalid channel_name.\n");
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (temp == "SUBCHANNEL")
        {
            if (OK != ParseString(&subchName))
            {
                ErrPrintf("subchannel_name is missing in PMTRIGGER_SYNC_EVENT\n");
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else // name
        {
            name = temp;
        }
    }

    if (value == "ON")
    {
        ++m_numSyncPmTriggers;
        if (name.empty())
        {
            name = Utility::StrPrintf("PMTRIGGER_SYNC_EVENT_%u", m_numSyncPmTriggers);
        }
    }

    if (channel == 0)
    {
        channel = GetLwrrentChannel(CPU_MANAGED);
        if (channel == 0)
        {
            ErrPrintf("PMTRIGGER_SYNC_EVENT used before CHANNEL declaration.\n");
            return RC::ILWALID_FILE_FORMAT;
        }
    }

    if (subchName.empty())
    {
        ErrPrintf(syntax);
        ErrPrintf("PMTRIGGER_SYNC_EVENT doesn't have subchannel name.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    subchannel = GetSubChannel(channel, subchName);
    if (subchannel == 0)
    {
        ErrPrintf(syntax);
        ErrPrintf("PMTRIGGER_SYNC_EVENT doesn't have subchannel.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    // This trace op is used to tell TDBG and other trace plugins about
    // event commands in the trace header.  The other event-related trace ops
    // aren't necessarily added to the list as they may depend on
    // command-line arguments to be activated.
    TraceOp *pMarkerOp = new TraceOpEventMarker(m_Test, name,
                                                "PMTRIGGER_SYNC_EVENT_" + value,
                                                channel, false);
    InsertOp(m_LineNumber, pMarkerOp, OP_SEQUENCE_OTHER);

    TraceOpEvent *pOp;
    if (value == "ON")
    {
        pOp = new TraceOpEventSyncPMStart(m_Test, channel, subchannel, name);
    }
    else if (value == "OFF")
    {
        pOp = new TraceOpEventSyncPMStop(m_Test, channel, subchannel, name);
    }
    else
    {
        ErrPrintf(syntax);
        ErrPrintf("Invalid PMTRIGGER_SYNC_EVENT option: %s.\n", value.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }
    InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);

    pMarkerOp = new TraceOpEventMarker(m_Test, name,
                                       "PMTRIGGER_SYNC_EVENT_" + value,
                                       channel, true);
    InsertOp(m_LineNumber, pMarkerOp, OP_SEQUENCE_OTHER);

    return OK;
}

RC GpuTrace::ParseEvent()
{
    string name;
    if (OK != ParseString(&name))
    {
        ErrPrintf("EVENT requires a name.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    // This trace op is used to tell TDBG and other trace plugins about
    // event commands in the trace header.  The other event-related trace ops
    // aren't necessarily added to the list as they may depend on
    // command-line arguments to be activated.
    TraceOpEventMarker *pMarkerOp = new TraceOpEventMarker(m_Test, name, "EVENT",
                                                           nullptr, false);
    InsertOp(m_LineNumber, pMarkerOp, OP_SEQUENCE_OTHER);

    // Hook trace operations which should be triggered on the EVENT
    // Check for image/buffer dumping
    if (!m_BuffersDumpOnEvent.empty())
    {
        DumpBufferMapOnEvent_t::iterator map_iter = m_BuffersDumpOnEvent.find(name);
        if (map_iter != m_BuffersDumpOnEvent.end())
        {
            vector<string>::iterator iter, end = map_iter->second.end();
            for (iter = map_iter->second.begin(); iter != end; ++iter)
            {
                // Dump buffers, we don't check the module name since trace can
                // declare events prior to modules.
                TraceOp *pOp = new TraceOpEventDump(m_Test,
                    GetLwrrentChannel(CPU_MANAGED), this, name, *iter);
                pMarkerOp->AddTraceOperations(pOp);
            }
        }
    }

    pMarkerOp = new TraceOpEventMarker(m_Test, name, "EVENT", nullptr, true);
    InsertOp(m_LineNumber, pMarkerOp, OP_SEQUENCE_OTHER);

    return OK;
}

RC GpuTrace::ParseSyncEvent()
{
    const char *syntax = "Syntax: SYNC_EVENT <ON|OFF> [name] \n";
    string value;
    string name;

    if (OK != ParseString(&value))
    {
        ErrPrintf(syntax);
        return RC::ILWALID_FILE_FORMAT;
    }

    if (ParseFile::s_NewLine != m_TestHdrTokens.front())
    {
        MASSERT(ParseString(&name) == OK);
    }

    if (name.empty())
    {
        name = Utility::StrPrintf("SYNC_EVENT_%s", value.c_str());
    }

    if (!params->ParamPresent("-enable_sync_event"))
    {
        InfoPrintf("Ignore SYNC_EVENT because -enable_sync_event is not specified.\n");
    }
    else
    {
        TraceOpEvent *pOp;
        if (value == "ON")
        {
            pOp = new TraceOpEventSyncStart(m_Test, name);
        }
        else if (value == "OFF")
        {
            pOp = new TraceOpEventSyncStop(m_Test, name);
        }
        else
        {
            ErrPrintf(syntax);
            ErrPrintf("Invalid SYNC_EVENT option: %s.\n", value.c_str());
            return RC::ILWALID_FILE_FORMAT;
        }

        InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);
        m_HaveSyncEventOp = true;
    }

    return OK;
}

RC GpuTrace::ParseTIMESLICEGROUP()
{
    RC rc;
    CHECK_RC(RequireVersion(3, "TIMESLICEGROUP"));

    if (params->ParamPresent("-tsg_name"))
    {
        InfoPrintf("TIMESLICEGROUP token is ignored because of option -tsg_name.\n");
        // consume tsgname following TIMESLICEGROUP
        m_TestHdrTokens.pop_front();
        return OK;
    }

    string strTSGname;
    if (OK != ParseString(&strTSGname))
    {
        ErrPrintf( "TIMESLICEGROUP requires a name.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    if (0 == GetTraceTsg(strTSGname))
    {
        AddTraceTsg(strTSGname, GetVAS("default"), false);
    }

    return rc;
}

RC GpuTrace::ParseCHECK_DYNAMICSURFACE()
{
    RC rc;
    // This trace command is only valid for dynamicly allocated surface.
    CHECK_RC(RequireVersion(5, "CHECK_DYNAMICSURFACE"));

    string strModName;
    if (OK != ParseString(&strModName))
    {
        ErrPrintf( "CHECK_DYNAMICSURFACE <modName> [crcsurfix] [WAIT_FOR_IDLE channel_name] requires module name.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    string sufixName;
    vector<TraceChannel*> wfiChannels;
    // Parse the rest of the optional args in any order
    while (ParseFile::s_NewLine != m_TestHdrTokens.front())
    {
        const char * tok = m_TestHdrTokens.front();
        m_TestHdrTokens.pop_front();

        if (0 == strcmp(tok, "WAIT_FOR_IDLE"))
        {
            TraceChannel * pTraceChannel;
            if (OK != ParseChannel(&pTraceChannel, CPU_MANAGED))
            {
                ErrPrintf("[WAIT_FOR_IDLE channel_name]: Channel name expected.\n");
                return RC::ILWALID_FILE_FORMAT;
            }
            wfiChannels.push_back(pTraceChannel);
        }
        else
        {
            // Same surface can be checked multiple times.
            // [crcsurfix] is optinal which appends to crcname
            // to avoid name conflict.
            sufixName = tok;
        }
    }

    TraceModules::iterator it = m_GenericModules.begin();
    for (; it != m_GenericModules.end(); it++)
    {
        if ((*it)->GetName() == strModName)
        {
            break;
        }
    }

    if (it != m_GenericModules.end())
    {
        TraceModule::ModCheckInfo checkInfo;
        if (((*it)->GetCheckInfo(checkInfo) &&
                (checkInfo.CheckMethod != NO_CHECK)) ||
            (*it)->IsColorOrZ())
        {
            TraceOp *pOp = new TraceOpCheckDynSurf(*it, m_Test, sufixName, wfiChannels);
            InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);
        }
        else
        {
            // Temporary trace_3d option -forbid_ill_dyn_check to open the new mode,
            // that is to return error immediately if the surface has no check method.
            // This mode will be default after all ACE traces have got fixed,
            // and then we'll remove this option.
            if (params->ParamPresent("-forbid_ill_dyn_check"))
            {
                ErrPrintf("CHECK_DYNAMICSURFACE <%s>: no CRC_CHECK or REF_CHECK info. "
                          "-forbid_ill_dyn_check specified. Return error.\n",
                          strModName.c_str());
                return RC::ILWALID_FILE_FORMAT;
            }
            // Remain the old mode as default.
            else
            {
                InfoPrintf("CHECK_DYNAMICSURFACE <%s>: no CRC_CHECK or REF_CHECK info. "
                           "-forbid_ill_dyn_check not specified. Ignore the command.\n",
                           strModName.c_str());
                return OK;
            }
        }
    }
    else
    {
        ErrPrintf("CHECK_DYNAMICSURFACE <%s>: Can't find the module specified.\n",
                  strModName.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }

    return OK;
}

static bool ParseWfiMethod(const string& strMethod, WfiMethod* method)
{
    MASSERT(method != NULL);
    if (strMethod == "POLL")
    {
        *method = WFI_POLL;
    }
    else if (strMethod == "NOTIFY")
    {
        *method = WFI_NOTIFY;
    }
    else if (strMethod == "INTR")
    {
        *method = WFI_INTR;
    }
    else if (strMethod == "SLEEP")
    {
        *method = WFI_SLEEP;
    }
    else if (strMethod == "HOST")
    {
        *method = WFI_HOST;
    }
    else
    {
        return false;
    }

    return true;
}

RC GpuTrace::ParseTRACE_OPTIONS()
{
    while (ParseFile::s_NewLine != m_TestHdrTokens.front())
    {
        const char* tok = m_TestHdrTokens.front();
        m_TestHdrTokens.pop_front();

        if (0 == strcmp(tok, "WFI_METHOD"))
        {
            string strMethod;
            if (OK != ParseString(&strMethod))
            {
                ErrPrintf("Error parsing '%s'\n", tok);
                return RC::ILWALID_FILE_FORMAT;
            }

            // Command line setting overrides this trace command
            // and will be picked up in Trace3DTest::SetupStage0
            if (params->ParamPresent("-wfi_method"))
                continue;

            WfiMethod method;
            if (!ParseWfiMethod(strMethod, &method))
            {
                ErrPrintf("Invalid WFI method: %s\n", tok);
                return RC::ILWALID_FILE_FORMAT;
            }

            m_Test->SetWfiMethod(method);
            DebugPrintf(MSGID(TraceParser),
                        "Set -wfi_method to %s by trace.\n",
                        strMethod.c_str());
        }
        else if (0 == strcmp(tok, "NO_PB_MASSAGE"))
        {
            m_Test->SetNoPbMassage(true);
            DebugPrintf(MSGID(TraceParser), "Enable -no_pb_massage by trace.\n");
        }
        else if (0 == strcmp(tok, "DISABLE_AUTOFLUSH"))
        {
            m_Test->SetNoAutoFlush(true);
            DebugPrintf(MSGID(TraceParser), "Enable -no_autoflush by trace.\n");
        }
        else
        {
            WarnPrintf("%s is not a recognized TRACE_OPTION and will be ignored."
                "  Please remove %s from the TRACE_OPTION command.\n",
                tok, tok);
        }
    }

    return OK;
}

//---------------------------------------------------------------------------
RC GpuTrace::ParseRELOC_SCALE()
{
    // RELOC_SCALE takes a value from a field of up to 64 bits spread
    // across two 32-bit words in the target pushbuffer or surface,
    // multiplies that value by a specified scaling factor, and then replaces
    // that value in the target pushbuffer or surface.

    const string syntax("RELOC_SCALE <offset> <mask_out> <SWAP|NO_SWAP> "
        "[TARGET <reloc file>] [SCALE_BY_SM_COUNT] [SCALE_BY_WARPS_PER_SM] "
        "[OFFSET_INCREMENT <delta>]");
    RC rc;
    CHECK_RC(RequireModLast("RELOC_SCALE"));
    TraceModule *target = ModLast();

    UINT32 offset;
    UINT64 maskOut;

    if (OK != ParseUINT32(&offset) ||
        OK != ParseUINT64(&maskOut))
    {
        ErrPrintf("%s\n", syntax.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }

    bool swapWords = false;
    string targetName = "";
    bool scaleByTPCCount = false;
    bool scaleByComputeWarpsPerTPC = false;
    bool scaleByGraphicsWarpsPerTPC = false;
    UINT32 offsetIncrement = 4;

    while (ParseFile::s_NewLine != m_TestHdrTokens.front())
    {
        const char *tok = m_TestHdrTokens.front();
        m_TestHdrTokens.pop_front();

        if (0 == strcmp(tok, "SWAP"))
        {
            swapWords = true;
        }
        else if (0 == strcmp(tok, "NO_SWAP"))
        {
            swapWords = false;
        }
        else if (0 == strcmp(tok, "TARGET"))
        {
            if (OK != ParseString(&targetName))
            {
                ErrPrintf("RELOC_SCALE property TARGET requires buffer name\n");
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp(tok, "SCALE_BY_SM_COUNT"))
        {
            scaleByTPCCount = true;
        }
        else if ((0 == strcmp(tok, "SCALE_BY_WARPS_PER_SM")) ||
            (0 == strcmp(tok, "SCALE_BY_COMPUTE_WARPS_PER_SM")))
        {
            scaleByComputeWarpsPerTPC = true;
        }
        else if (0 == strcmp(tok, "SCALE_BY_GRAPHICS_WARPS_PER_SM"))
        {
            scaleByGraphicsWarpsPerTPC = true;
        }
        else if (0 == strcmp(tok, "OFFSET_INCREMENT"))
        {
            if (OK != ParseUINT32(&offsetIncrement))
            {
                ErrPrintf("%s\n", syntax.c_str());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else
        {
            ErrPrintf("%s\n", syntax.c_str());
            ErrPrintf("Unexpected token: %s\n", tok);
            return RC::ILWALID_FILE_FORMAT;
        }
    }

    if (!scaleByTPCCount &&
        !scaleByComputeWarpsPerTPC &&
        !scaleByGraphicsWarpsPerTPC)
    {
        ErrPrintf("The RELOC_SCALE command must include at least one of the following properties: "
                  "SCALE_BY_SM_COUNT, SCALE_BY_COMPUTE_WARPS_PER_SM, SCALE_BY_GRAPHICS_WARPS_PER_SM\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    if (targetName.length() > 0)
    {
        target = ModFind(targetName.c_str());
        if (0 == target)
        {
            ErrPrintf("%s\n", syntax.c_str());
            ErrPrintf("TARGET %s does not exist\n", targetName.c_str());
            return RC::ILWALID_FILE_FORMAT;
        }
    }

    CHECK_RC(target->RelocAdd(new TraceRelocationScale(
        maskOut,
        offset,
        offset + offsetIncrement,
        target,
        swapWords,
        scaleByTPCCount,
        scaleByComputeWarpsPerTPC,
        scaleByGraphicsWarpsPerTPC,
        target->GetSubdevMaskDataMap())));

    return OK;
}

// GpuTrace::SetupTextureParams() reads texture header state and based on that
// sets up texture parametrs for all texture modules
RC GpuTrace::SetupTextureParams()
{
    RC rc;
    ModuleIter modIter;
    vector<TextureHeaderState> texture_headers;
    UINT32* header = 0;

    // Step 1. Reading texture header state
    bool headerProcessed = false;
    for (modIter = m_Modules.begin();
            modIter != m_Modules.end() && !headerProcessed;
            ++modIter)
    {
        if ((*modIter)->IsTextureHeader())
        {
            // Load from file.
            (*modIter)->LoadFromFile(m_TraceFileMgr);
            // Read the texture state for all the subdevices.
            set<UINT32*> headers;
            (*modIter)->GetSurfaces(&headers);
            DebugPrintf(MSGID(TraceParser), "GpuTrace::%s: Get pointers for %s\n",
                    __FUNCTION__, (*modIter)->GetName().c_str());
            MASSERT(!headers.empty());
            for (set<UINT32*>::iterator i = headers.begin();
                 i != headers.end(); ++i)
            {
                header = *i;
                DebugPrintf(MSGID(TraceParser), "header= %x\n", header);
                if (!header)
                {
                    ErrPrintf("Can not read texture header state\n");
                    return RC::ILWALID_FILE_FORMAT;
                }

                CHECK_RC(SetTextureHeaderPromotionField(header, (*modIter)->GetSize()));
                CHECK_RC(SetTextureLayoutToPitch(header, (*modIter)->GetSize()));

                for (UINT32* tx = header;
                        tx < header + (*modIter)->GetSize()/sizeof(UINT32);
                        tx += sizeof(TextureHeaderState)/sizeof(UINT32))
                {
                    SetTextureHeaderState(tx);
                }

                // Only one header is allowed.
                headerProcessed = true;
            }
        }
    }

    // Step 2. Setting up texture parameters
    for (modIter = m_Modules.begin(); modIter != m_Modules.end(); ++modIter)
    {
        if ((*modIter)->GetFileType() == FT_TEXTURE ||
            ((*modIter)->GetFileType() == FT_ALLOC_SURFACE &&
             (*modIter)->GetTextureIndexNum() > 0))
        {
            const TextureHeaderState* texture_state = GetTextureHeaderState((*modIter)->GetTextureIndex());
            if (!texture_state)
            {
                if ((*modIter)->GetFileType() == FT_TEXTURE)
                {
                    ErrPrintf("Can't find texture header for %s\n", (*modIter)->GetName().c_str());
                    return RC::ILWALID_FILE_FORMAT;
                }
                return OK;
            }

            if (!(*modIter)->SetTextureParams(texture_state, params))
            {
                ErrPrintf("Header for texture %s seems wrong\n", (*modIter)->GetName().c_str());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
    }
    return OK;
}

UINT64 GpuTrace::GetUsageByFileType(TraceFileType type, bool excl_specified, LwRm::Handle hVASpace) const
{
    UINT64 Total = 0;
    UINT64 Align = GetFileTypeAlign(type);
    ModuleIter modIter;
    for (modIter = m_GenericModules.begin();
         modIter != m_GenericModules.end(); ++modIter)
    {
        if (((*modIter)->GetFileType() == type) &&
            ((*modIter)->GetLocation() == _DMA_TARGET_VIDEO) &&
            ((*modIter)->GetVASpaceHandle() == hVASpace))
        {
            // if excl_specified is set to true, do not include tracemodules
            // which will be allocated separately to specific addresses
            if (excl_specified && (*modIter)->AllocIndividualBuffer())
            {
                continue;
            }

            Total = (Total + Align-1) & ~(Align-1);
            if ((*modIter)->GetTxParams())
                Total += (*modIter)->GetTxParams()->Size();
            else
                Total += (*modIter)->GetSize();
        }
    }

    // HACK:  RM may allocate buffers with a lower alignment value
    // then is required by the buffer type because it doesn't know
    // what the buffer type is.  When mods divides the buffer
    // into suballocations, it will align each offset by the alignment
    // necessary for the buffer type.  RM may have allocated an
    // an address which isn't aligned for the given buffer and this
    // will mean that there isn't enough memory to do the suballocations.
    // (See bug 518698).  The proper way to fix this would be to tell RM
    // the necessary alignment for a given buffer type, but this won't
    // necessarily work for amodel.  The alternative is to always add extra
    // space in the big buffer so that the suballocations will not
    // run out of room if the offset needs to be aligned to a larger value.
    if (Total > 0)
    {
        Total += Align;
    }

    return Total;
}

Memory::Protect GpuTrace::GetProtectByFileType(TraceFileType type) const
{
    // Protections are computed by or'ing together the protections for all the
    // individual surfaces that comprise this memory arena.  If there aren't
    // any, we default to read-only.
    Memory::Protect Protect = (Memory::Protect)0;
    ModuleIter modIter;
    for (modIter = m_GenericModules.begin();
         modIter != m_GenericModules.end(); ++modIter)
    {
        if ((*modIter)->GetFileType() == type)
            Protect = (Memory::Protect)(Protect | (*modIter)->GetProtect());
    }
    if (!Protect)
        Protect = Memory::Readable;
    return Protect;
}

UINT32 GpuTrace::GetFileTypeAlign(TraceFileType type) const
{
    MASSERT( type < FT_NUM_FILE_TYPE );

    // Conditional return here to avoid
    // error: array subscript is above array bounds [-Werror=array-bounds]

    if (type >= FT_NUM_FILE_TYPE)
    {
        ErrPrintf("Unrecognized TraceFileType %d\n", type);
        return 0;
    }

    // Get default alignment.

    UINT32 align = s_FileTypeData[type].DefaultAlignment;

    // GPUs older than Turing do not support 64-bit alignment for constant
    // buffers, so use the old default.

    if (type == FT_CONSTANT_BUFFER &&
        m_Test->GetBoundGpuDevice()->DeviceId() < Gpu::TU102)
    {
        align = 256;
    }

    // Get global command-line override.

    if (params->ParamPresent("-suballoc_min_align"))
    {
        UINT32 minAlign = params->ParamUnsigned("-suballoc_min_align");
        if (minAlign > align)
            align = minAlign;
    }

    // Get per-filetype command-line override.

    char argname[256];
    sprintf(argname, "-suballoc_align_%s", s_FileTypeData[type].ArgName);

    if (params->ParamDefined(argname) && params->ParamPresent(argname))
    {
        align = params->ParamUnsigned(argname);
    }

    return align;
}

// load files, mapping them into memory - returns 1 on success, 0 on error
RC GpuTrace::AllocateSurfaces(UINT32 width, UINT32 height)
{
    RC rc;
    // Surfaces are not allocated up to this point, since some info from
    // pushbuffer is required to do that (Does the trace have streaming
    // output?).  Moving pushbuffer to the beginning of the list of modules,
    // so it was loaded first.  Surfaces are allocated in Load(), when it's
    // called for pushbuffer.  That makes sure that other surfaces are loaded
    // after they're allocated.  The problem here is the original (mdiag's)
    // design does not take into account the fact that some info about the way
    // trace should be loaded might be inside the trace. It assumed all the
    // "loading" info is in .hdr file.

    // The above change of logic seems to be the simplest for now.
    MoveModuleToBegin(GpuTrace::FT_PUSHBUFFER);

    ModuleIter moduleIt;
    for (moduleIt = m_Modules.begin(); moduleIt != m_Modules.end(); moduleIt++)
    {
        if (!(*moduleIt)->IsDynamic())
        {
            CHECK_RC((*moduleIt)->Allocate());
        }
    }

    //Relocations are done after surfaces are allocated (see DoRelocations())

    return rc;
}

RC GpuTrace::DoRelocations(GpuDevice *pGpuDev)
{
    RC rc;

    ModuleIter end = m_Modules.end();
    for (ModuleIter moduleIt = m_Modules.begin(); moduleIt != end; moduleIt++)
    {
        if ((*moduleIt)->IsColorOrZ())
        {
            continue;
        }

        for (UINT32 subdev = 0; subdev < pGpuDev->GetNumSubdevices(); ++subdev)
        {
            DebugPrintf(MSGID(TraceParser), "DoRelocations for Subdevice %d\n", subdev);
            CHECK_RC(DoRelocations2SubDev(*moduleIt, subdev));
        }

        if ((*moduleIt)->GetFileType() == GpuTrace::FT_PUSHBUFFER)
        {
            CHECK_RC((*moduleIt)->RemoveFermiMethods(0));
        }

        // Third stage of massaging:
        // Patch all of the offsets that need to be patched in the push buffer
        //    with the data stored in m_SubdevMaskDataMap,which is a
        //    GpuTrace::SubdevMaskDataMap.
        CHECK_RC(AddSubdevMaskDataMap(*moduleIt));

        if ((*moduleIt)->GetFileType() == GpuTrace::FT_PUSHBUFFER)
        {
            CHECK_RC((*moduleIt)->AllocPushbufSurf());
        }
    }

    return rc;
}

RC GpuTrace::DoRelocations2SubDev(TraceModule *pMod, UINT32 subdev)
{
    for (auto relocIt = pMod->RelocBegin(); relocIt != pMod->RelocEnd(); ++relocIt)
    {
        // at 'offset' in this module, add the start of 'target'
        (*relocIt)->PrintHeader(pMod, "before relocation: ", subdev);
        RC rc = (*relocIt)->DoOffset(pMod, subdev);
        if (OK != rc)
        {
            ErrPrintf("Can not relocate %s\n", pMod->GetName().c_str());
            return rc;
        }
        (*relocIt)->PrintHeader(pMod, "after relocation: ",
                subdev);
    }

    if (pMod->GetFileType() == GpuTrace::FT_PUSHBUFFER)
    {
        // Second stage of pushbuffer massaging
        pMod->MassagePushBuffer2();
        pMod->DoSkipGeometry();
        pMod->DoSkipComputeGrid();

        // Do a bit of extra massaging when needed. We need to be able to
        // add some methods to the pushbuffer and above massaging
        // doesn't allow for that.  Since above code is used by lots of
        // chips, we'll add this on a case by case basis. Its ugly, but
        // conceptually easy
        // only do it when its activated
        if (params->ParamPresent("-add_methods"))
        {
            InfoPrintf("Adding/Subtracting methods to/from push buffer\n");
            if (! pMod->MassageAndAddPushBuffer(this, params))
                return RC::SOFTWARE_ERROR;
        }

        if (!subdev) pMod->PrintPushBuffer("GoodPB", false);
    }

    return OK;
}

RC GpuTrace::AddSubdevMaskDataMap(TraceModule *pMod)
{
    RC rc = OK;
    if (pMod->GetFileType() == GpuTrace::FT_PUSHBUFFER)
    {
        if (!pMod->MassageAndInsertSubdeviceMasks())
        {
            return RC::SOFTWARE_ERROR;
        }
    }

    return rc;
}

RC GpuTrace::LoadSurfaces()
{
    RC rc;
    ModuleIter moduleIt;

    for (moduleIt = m_Modules.begin(); moduleIt != m_Modules.end(); moduleIt++)
    {
        // Don't download dynamically allocated modules to the chip yet.
        // They will be downloaded when they are allocated.
        if (!(*moduleIt)->IsDynamic())
        {
            CHECK_RC((*moduleIt)->Download());
        }
    }

    return OK;
}

void GpuTrace::MoveModuleToBegin(GpuTrace::TraceFileType ft)
{
    // Move all modules of type "ft" to the beginning of the list.
    list<TraceModule*>::iterator moduleIt;

    moduleIt = m_Modules.begin();

    // Skip over all modules at the beginning that are of type "ft", so we
    // don't move them unnecessarily.
    for (/**/; moduleIt != m_Modules.end(); ++moduleIt)
    {
        if ((*moduleIt)->GetFileType() != ft)
            break;
    }
    list<TraceModule*>::iterator insertIt = moduleIt;

    // Find all remaining modules of type "ft" and move them to before the
    // insert point (i.e. before the first non-"ft" module).
    while (moduleIt != m_Modules.end())
    {
        if ((*moduleIt)->GetFileType() == ft)
        {
            insertIt = m_Modules.insert(insertIt, *moduleIt);
            // Point to the next position so that the modules retain their
            // ordering from the trace file (insert inserts before the
            // insert point and returns an iterator to the inserted element
            // which will effectively reverse the order of all files in
            // the list)
            insertIt++;
            moduleIt = m_Modules.erase(moduleIt);
        }
        else
        {
            ++moduleIt;
        }
    }
}

SurfaceType GpuTrace::FindSurface(const string& surf) const
{
    //input is COLOR0|COLOR1|...|COLOR{MAX_RENDER_TARGETS-1}|Z|STENCIL
    //output is number of render color target, -1 for z and < -1 is for error

    if (surf.substr(0, 5) == "COLOR")
    {
        string::size_type p = surf.find_first_not_of("COLOR");
        if (p == string::npos)
            return SURFACE_TYPE_UNKNOWN; //index is missing

        int i;
        if (1 != sscanf(surf.substr(p).c_str(), "%d", &i))
            return SURFACE_TYPE_UNKNOWN; //not a number

        if (i < 0 || i >= MAX_RENDER_TARGETS)
            return SURFACE_TYPE_UNKNOWN; //number is too big

        return ColorSurfaceTypes[i];
    }
    else if (surf == "Z")
        return SURFACE_TYPE_Z;
    else if (surf == "CLIPID")
        return SURFACE_TYPE_CLIPID;
    else if (surf == "VERTEX_BUFFER")
        return SURFACE_TYPE_VERTEX_BUFFER;
    else if (surf == "TEXTURE")
        return SURFACE_TYPE_TEXTURE;
    else if (surf == "ZLWLL")
        return SURFACE_TYPE_ZLWLL;
    else if (surf == "INDEX_BUFFER")
        return SURFACE_TYPE_INDEX_BUFFER;
    else if (surf == "SEMAPHORE")
        return SURFACE_TYPE_SEMAPHORE;
    else if (surf == "NOTIFIER")
        return SURFACE_TYPE_NOTIFIER;
    else if (EngineClasses::IsGpuFamilyClassOrLater(
                  TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_BLACKWELL) && 
            (surf == "STENCIL"))
        return SURFACE_TYPE_STENCIL;
    else
        return SURFACE_TYPE_UNKNOWN; //unknown surface type
}

bool GpuTrace::BufferLwlled(const string& filename) const
{
    return Platform::GetSimulationMode()  != Platform::Amodel &&
           m_LwlledBuffers.find(filename) != m_LwlledBuffers.end();
}

// Get the beginning of the modules list, the default only gets the beginning
// of the list of generic modules (without peer modules), but by specifying
// different parameters, the beginning of only peer modules or of both
// peer and generic modules may be retrieved
ModuleIter GpuTrace::ModBegin(bool bIncludeGeneric /* = true */,
                              bool bIncludePeer    /* = false */) const
{
    MASSERT(bIncludeGeneric || bIncludePeer);

    if (bIncludeGeneric && bIncludePeer)
        return m_Modules.begin();

    if (bIncludePeer)
        return m_PeerModules.begin();

    return m_GenericModules.begin();
}

// Get the end of the modules list, the default only gets the end
// of the list of generic modules (without peer modules), but by specifying
// different parameters, the end of only peer modules or of both
// peer and generic modules may be retrieved
ModuleIter GpuTrace::ModEnd(bool bIncludeGeneric /* = true */,
                            bool bIncludePeer    /* = false */) const
{
    MASSERT(bIncludeGeneric || bIncludePeer);

    if (bIncludeGeneric && bIncludePeer)
        return m_Modules.end();

    if (bIncludePeer)
        return m_PeerModules.end();

    return m_GenericModules.end();
}

TraceModule * GpuTrace::ModLast() const
{
    return m_Modules.back();
}

TraceModule * GpuTrace::ModFind(const char *filename) const
{
    ModuleIter moduleIt;
    for (moduleIt = m_Modules.begin(); moduleIt != m_Modules.end(); ++moduleIt)
    {
        if (0 == strcmp((*moduleIt)->GetName().c_str(), filename))
            return *moduleIt;
    }
    return NULL;
}

//! \brief Find trace module by surface type
//!
//! \param surfaceType A surface type (e.g., color0, z, etc.)
TraceModule *GpuTrace::FindModuleBySurfaceType(SurfaceType surfaceType) const
{
    ModuleIter iter;

    for (iter = m_Modules.begin(); iter != m_Modules.end(); ++iter)
    {
        if (surfaceType == (*iter)->GetSurfaceType())
        {
            return *iter;
        }
    }

    return NULL;
}

TraceModule * GpuTrace::ModFindTexIndex(UINT32 index) const
{
    ModuleIter moduleIt;
    for (moduleIt = m_Modules.begin(); moduleIt != m_Modules.end(); ++moduleIt)
    {
        if (GpuTrace::FT_TEXTURE == (*moduleIt)->GetFileType() ||
            GpuTrace::FT_ALLOC_SURFACE == (*moduleIt)->GetFileType())
        {
            UINT32 texture_num = (*moduleIt)->GetTextureIndexNum();
            for (UINT32 i=0; i<texture_num; ++i)
            {
                if (index == (*moduleIt)->GetTextureIndex(i))
                    return *moduleIt;
            }
        }
    }
    return NULL;
}

const TextureHeaderState* GpuTrace::GetTextureHeaderState (UINT32 index) const
{
    if (index >= m_TextureHeaders.size())
        return 0;

    return &m_TextureHeaders[index];
}

void GpuTrace::SetTextureHeaderState (UINT32* begin)
{
    TextureHeaderState state(begin);
    m_TextureHeaders.push_back(state);
}

// @@@ extract the header-parsing code to an object, it is used 5 places
//     inside TraceModule, no need to track across gpentries for now
//     -- put this off for now, not needed in initial checkin!
//
// Assumptions:
// Pushbuffer modules will be 100% sent in 1 to N chunks, in order.
// A chunk always starts at a message header.
// A chunk ends at the end of an entire message.
//  - except when a gpfifo entry follows the chunk, in which case the chunk
//    ends after the message header.
// (@@@ can we enforce this?)
//
// Trace rewriting:
// Iterate over each message, skipping partial messages that are just before
// a GpEntry.
//   a) each method,data pair is sent through MassageTeslaMethod (or equivalent)
//      - data may be changed
//      - Trace3DState is updated
//      - before surface allocations, discover some surfaces used here
//   b) Apply relocs (modify method data for runtime data)
//     - after surface allocations
//   c) Apply RELOC_RESET_VALUE relocs (why not in step b?)
//   d) -inject_method (add meth,data pair every N methods)
//      increases size of data, ilwalidates old offsets in TraceOpSendMethods
//   e) -alias_startGeometry -endGeometry
//      change some SetBeginEnds to END
//   f) debug_init (gdb hackery to skip some initial methods and colwert other
//      to single-method sends (during playback)
//   g) for some PM modes and -single_method and debug_init
//      colwert multi-method sends to single-method sends (during playback)

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// This is the actual playback.
RC GpuTrace::RunTraceOps()
{
    RC rc;

    UINT32 jumpTarget = 0;
    T3dPlugin::EventAction action = T3dPlugin::NoAction;
    bool jumping = false;
    UINT32 traceOpRemainNum = m_TraceOps.size();
    vector<TraceOp*> pendingTraceOps;

    // Initialize every traceop status every time enter this function
    // in case that call this function many times during one test.
    InitTraceOpsStatus();

    do
    {
        UINT32 opNum = 0;
        UINT32 traceOpDoneEveryLoop = 0;
        pendingTraceOps.clear();
        for (TraceOps::iterator iOp = m_TraceOps.begin(); iOp != m_TraceOps.end(); ++iOp, ++opNum)
        {
            if (iOp->second->GetTraceOpStatus() == TraceOp::TRACEOP_STATUS_DONE ||
                (jumping && (opNum != jumpTarget)) || !(iOp->second->CheckTraceOpDependency()))
                continue;
            jumping = false;

            if (!iOp->second->NeedSkip() &&
                (Tee::WillPrint(Tee::PriDebug) ||
                // Bug #792744: We have two ways to print TraceOps by "Module":
                //  1) check for the module status here; or
                //  2) stick GetTeeModuleTestCode() to each TraceOp's Print method
                // We chose option #1 because it needs only one local change below.
                Tee::IsModuleEnabled(MSGID(TraceOp))))
            {
                if (m_Version < 3)
                    InfoPrintf(MSGID(TraceOp), "TraceOp[@pbuf 0x%x] ", (iOp->first >> OP_SEQUENCE_SHIFT));
                else
                    InfoPrintf(MSGID(TraceOp), "TraceOp[line %d] ", iOp->first);

                iOp->second->Print();
            }

            TriggerTraceOpEvent(iOp->second, TraceOpTriggerPoint::Before);

            iOp->second->SetTraceOpExecNum(opNum);
            if (!iOp->second->NeedSkip() &&
                m_Test->TraceOp_traceEvent(Trace3DTest::TraceEventType::BeforeTraceOp, opNum, &action, &jumpTarget))
            {
                if(jumpTarget <= opNum)
                {
                    ForEachTraceOps(jumpTarget, opNum, [](TraceOp * op){op->SetTraceOpStatus(TraceOp::TRACEOP_STATUS_DEFAULT);});
                }
                else if (jumpTarget > m_TraceOps.size())
                {
                    MASSERT(!"Jumping to non-existent traceOp");
                }
                else
                {
                    // Only set traces before jumpTarget as DONE, so the jumpTarget can run 
                    ForEachTraceOps(opNum, jumpTarget-1, [](TraceOp * op){op->SetTraceOpStatus(TraceOp::TRACEOP_STATUS_DONE);});
                    traceOpDoneEveryLoop += jumpTarget- opNum;
                }
                jumping = true;
                break;
            }
          
            if (iOp->second->NeedSkip())
            {
                InfoPrintf("Skip TraceOp %d, type %d name %s.\n",
                           iOp->second->GetTraceOpId(), iOp->second->GetType(),
                           TraceOp::GetOpName(iOp->second->GetType()));
            }
            else
            // Free the event just after the run() call, so we can get more accurate simclks
            { 
                string name = Utility::StrPrintf("hdr_line:%d", iOp->first);
                SimClk::EventWrapper event(SimClk::Event::T3D_TRACE_PROFILE, name);
                rc = iOp->second->Run();
            }

            if (OK != rc)
            {
                if (m_Version < 3)
                    ErrPrintf("TraceOp[@pbuf 0x%x] ", (iOp->first >> OP_SEQUENCE_SHIFT));
                else
                    ErrPrintf("TraceOp[line %d] ", iOp->first);

                iOp->second->Print();
                return rc;
            }
            else
            {
                if (iOp->second->GetTraceOpStatus() == TraceOp::TRACEOP_STATUS_PENDING)
                {
                    pendingTraceOps.push_back(iOp->second);
                    DebugPrintf("NON_BLOCKING EXELWTION: Traceop %d failed in first polling, ready to enter pending list.\n", iOp->second->GetTraceOpId());
                }
                else
                {
                    iOp->second->SetTraceOpStatus(TraceOp::TRACEOP_STATUS_DONE);
                    traceOpDoneEveryLoop +=1;
                }
            }

            // Trigger trace event AfterTraceOp until traceop completes.
            if (iOp->second->GetTraceOpStatus() != TraceOp::TRACEOP_STATUS_PENDING)
            {
                TriggerTraceOpEvent(iOp->second, TraceOpTriggerPoint::After);

                if (!iOp->second->NeedSkip() &&
                    m_Test->TraceOp_traceEvent(Trace3DTest::TraceEventType::AfterTraceOp, iOp->second->GetTraceOpExecOpNum(), &action, &jumpTarget))
                {
                    if(jumpTarget <= opNum)
                    {
                        ForEachTraceOps(jumpTarget, opNum, [](TraceOp * op){op->SetTraceOpStatus(TraceOp::TRACEOP_STATUS_DEFAULT);});
                    }
                    else if (jumpTarget > m_TraceOps.size())
                    {
                        MASSERT(!"Jumping to non-existent traceOp");
                    }
                    else
                    {
                        // Only set traces before jumpTarget as DONE, so the jumpTarget can run 
                        ForEachTraceOps(opNum, jumpTarget-1, [](TraceOp * op){op->SetTraceOpStatus(TraceOp::TRACEOP_STATUS_DONE);});
                        traceOpDoneEveryLoop += jumpTarget - opNum;
                    }
                    jumping = true;
                    break;
                }
            }
        }

        UINT32 doneCount = 0;
        while (pendingTraceOps.size() > 0 && doneCount == 0)
        {
            for (auto it = pendingTraceOps.begin(); it != pendingTraceOps.end(); ++it)
            {
                CHECK_RC((*it)->Run());
                if ((*it)->GetTraceOpStatus() == TraceOp::TRACEOP_STATUS_DEFAULT)
                {
                    doneCount++;
                    traceOpDoneEveryLoop++;
                    (*it)->SetTraceOpStatus(TraceOp::TRACEOP_STATUS_DONE);

                    // Not handle skip/jump request from TDBG here,
                    // which definitely would break SCG.
                    TriggerTraceOpEvent((*it), TraceOpTriggerPoint::After);
                    m_Test->TraceOp_traceEvent(Trace3DTest::TraceEventType::AfterTraceOp, (*it)->GetTraceOpExecOpNum(), nullptr, nullptr);

                    if (Tee::WillPrint(Tee::PriDebug) ||
                        Tee::IsModuleEnabled(MSGID(TraceOp)))
                    {
                        if (m_Version < 3)
                            InfoPrintf(MSGID(TraceOp), "TraceOp[@pbuf 0x%x] ", ((*it)->GetTraceOpId() >> OP_SEQUENCE_SHIFT));
                        else
                            InfoPrintf(MSGID(TraceOp), "TraceOp[line %d] ", (*it)->GetTraceOpId());
                        (*it)->Print();
                    }
                    DebugPrintf("NON_BLOCKING EXELWTION: Traceop %d succeed in final polling, getting out of pending list.\n", (*it)->GetTraceOpId());
                    break;
                }
            }

            if (doneCount == 0)
            {
                Tasker::Yield();
            }
        }

        traceOpRemainNum -= traceOpDoneEveryLoop;

        // For the Fast ACE case:
        // Handle jumping to the the end of trace (after last traceOp)
        // all trace between current trace and jump target has been marked done
        if (traceOpRemainNum == 0 && jumping)
            break;

    // need to check traceOpRemainNum
    } while (jumping || traceOpRemainNum > 0);

    return rc;
}

void GpuTrace::TriggerTraceOpEvent(TraceOp* op, const TraceOpTriggerPoint point)
{
    op->SetTriggerPoint(point);
    if (Utl::HasInstance())
    {
        Utl::Instance()->TriggerTraceOpEvent(m_Test, op);
    }
}

//---------------------------------------------------------------------------
// Checks for VP2 command line arguments and exelwtes them,
// checks if the VP2 Relocs are present if needed
RC GpuTrace::SetupVP2Params()
{
    RC rc = OK;

    ModuleIter modIter;

    // Peer setup needs to be delayed until all tests are setup so that
    // the peer information can be retrieved from the peer module
    for (modIter = m_GenericModules.begin();
         modIter != m_GenericModules.end(); ++modIter)
    {
        if (!(*modIter)->IsVP2TilingSupported())
            continue;

        CHECK_RC((*modIter)->ExelwteVP2TilingMode());

        if (!(*modIter)->AreRequiredVP2TileModeRelocsPresent())
        {
            ErrPrintf("At least one RELOC_VP2_TILEMODE is needed for %s\n", (*modIter)->GetName().c_str());
            return RC::SOFTWARE_ERROR;
        }

        /* Disable requirement for RELOC_VP2_PITCH presence for now,
           as tests are being prepared in a way that the reloc is not needed
        if (!(*modIter)->AreRequiredVP2PitchRelocsNeeded())
        {
            ErrPrintf("At least one RELOC_VP2_PITCH is needed for %s\n",
                (*modIter)->GetName().c_str());
            return RC::SOFTWARE_ERROR;
        }
        */
    }

    return rc;
}

RC GpuTrace::SetTextureHeaderPromotionField( UINT32* header, UINT32 size )
{
    RC rc;
    unsigned  l1_promote;

    GpuSubdevice *pSubdev = m_Test->GetGpuResource()->GetGpuSubdevice();
    MASSERT(pSubdev);

    if (params->ParamPresent("-l1_promotion"))
    {
        l1_promote = params->ParamUnsigned("-l1_promotion");
        bool bl1_promotion_optimal = l1_promote == 4;

        if (bl1_promotion_optimal)
        {
            ModuleIter modIter = m_Modules.begin();
            ModuleIter modIter_end = m_Modules.end();
            for( ; modIter != modIter_end; ++modIter )
            {
                if ((*modIter)->GetFileType() != FT_TEXTURE ) //texture module has correct pool header
                    continue;

                for(UINT32 i=0; i<(*modIter)->GetTextureIndexNum(); ++i)
                {
                    UINT32 offset = sizeof(TextureHeaderState) *
                        (*modIter)->GetTextureIndex(i) / sizeof(UINT32);

                    unique_ptr<TextureHeaderState> pTexHeaderState(new TextureHeaderState(header + offset));
                    unique_ptr<TextureHeader> pTexHeader(TextureHeader::CreateTextureHeader(pTexHeaderState.get(),
                        pSubdev, m_Test->GetTextureHeaderFormat()));

                    l1_promote = pTexHeader->GetOptimalSectorPromotion();
                    CHECK_RC(TextureHeader::ModifySectorPromotion(header + offset, (TextureHeader::L1_PROMOTION) l1_promote, m_Test->GetTextureHeaderFormat()));

                    InfoPrintf("SetTextureHeaderPromotionField: -l1_promotion_optimal=%d TextureHeaderPool offset=0x%x\n",l1_promote, offset);
                }
            }
        }
        else
        {
            for (UINT32 i = 0; i < size; i+= sizeof(TextureHeaderState))
            {
                UINT32 offset = i / sizeof(UINT32);

                CHECK_RC(TextureHeader::ModifySectorPromotion(header + offset, (TextureHeader::L1_PROMOTION) l1_promote, m_Test->GetTextureHeaderFormat()));
            }
        }
        return OK;
    }

    unsigned   l1_promote_vid, l1_promote_coh, l1_promote_ncoh;

    l1_promote_vid = params->ParamUnsigned("-l1_promotion_vid", 4);
    l1_promote_coh = params->ParamUnsigned("-l1_promotion_coh", 4);
    l1_promote_ncoh = params->ParamUnsigned("-l1_promotion_ncoh", 4);

    if ((l1_promote_vid == 4) && (l1_promote_coh == 4) && (l1_promote_ncoh == 4))
    {
        return OK;
    }

    // We have to iterate through each texture buffer and compare its location
    ModuleIter modIter = m_Modules.begin();
    ModuleIter modIter_end = m_Modules.end();
    for( ; modIter != modIter_end; ++modIter )
    {
        if ((*modIter)->GetFileType() != FT_TEXTURE )
            continue;

        unsigned l1_promote_all = 4;
        _DMA_TARGET loc = (*modIter)->GetLocation();

        if ((l1_promote_vid < 4) && (loc == _DMA_TARGET_VIDEO))
            l1_promote_all = l1_promote_vid;
        else if ((l1_promote_coh < 4) && (loc == _DMA_TARGET_COHERENT))
            l1_promote_all = l1_promote_coh;
        else if ((l1_promote_ncoh < 4) && (loc == _DMA_TARGET_NONCOHERENT))
            l1_promote_all = l1_promote_ncoh;

        if ( l1_promote_all < 4 ) // texture header need patch
        {
            for(UINT32 i=0; i<(*modIter)->GetTextureIndexNum(); ++i)
            {
                UINT32 offset = sizeof(TextureHeaderState) *
                    (*modIter)->GetTextureIndex(i) / sizeof(UINT32);

                CHECK_RC(TextureHeader::ModifySectorPromotion(header + offset, (TextureHeader::L1_PROMOTION) l1_promote_all, m_Test->GetTextureHeaderFormat()));
            }
        }
    }

    return OK;
}

RC GpuTrace::SetTextureLayoutToPitch(UINT32 *data, UINT32 size)
{
    if (params->ParamPresent("-pitch_tex"))
    {
        if (m_Test->GetTextureHeaderFormat() != TextureHeader::HeaderFormat::Kepler)
        {
            ErrPrintf("The -pitch_tex argument only works with texture header formats prior to Maxwell.\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        for (UINT32 i = 0; i < size; i+= sizeof(TextureHeaderState))
            *(data + i/sizeof(UINT32) + 2) = FLD_SET_DRF(
                    9097, _TEXHEAD2, _MEMORY_LAYOUT, _PITCH,
                    *(data + i/sizeof(UINT32) + 2));
    }

    return OK;
}

RC GpuTrace::SetClassParam(UINT32 classNum,
    TraceChannel *pTraceChannel,
    TraceSubChannel *pTraceSubChannel)
{
    RC rc;

    if (EngineClasses::IsClassType("Ce", classNum))
    {
        CHECK_RC(SetCeType(pTraceChannel, pTraceSubChannel,
            pTraceSubChannel->GetCeType(),
            m_Test->GetBoundGpuDevice()->GetSubdevice(0)));
    }
    else if (EngineClasses::IsClassType("Lwenc", classNum))
    {
        CHECK_RC(SetLwEncOffset(pTraceSubChannel));
    }
    else if (EngineClasses::IsClassType("Lwdec", classNum))
    {
        CHECK_RC(SetLwDecOffset(pTraceSubChannel));
    }
    else if (EngineClasses::IsClassType("Lwjpg", classNum))
    {
        CHECK_RC(SetLwJpgOffset(pTraceSubChannel));
    }

    return OK;
}

RC GpuTrace::ZlwllRamSanityCheck() const
{
    return OK;
}

RC GpuTrace::ProcessBufDumpArgs( )
{
    m_LogInfoEveryEnd = params->ParamPresent("-log_info_every_end") > 0;
    m_DumpImageEveryEnd = params->ParamPresent("-dump_image_every_end") > 0;
    m_DumpZlwllEveryEnd = params->ParamPresent("-dump_zlwll_every_end") > 0;
    m_DumpImageEveryBegin = params->ParamPresent("-dump_image_every_begin") > 0;
//     m_DumpZlwllEveryBegin = params->ParamPresent("-dump_zlwll_every_begin") > 0;

    int argnum = params->ParamPresent("-dump_image_nth_begin");
    for (int i=0; i<argnum; ++i)
    {
        m_DumpImageEveryBeginList.push(params->ParamNUnsigned("-dump_image_nth_begin",i,0));
    }

    argnum = params->ParamPresent("-dump_image_nth_end");
    for (int i=0; i<argnum; ++i)
    {
        m_DumpImageEveryEndList.push(params->ParamNUnsigned("-dump_image_nth_end",i,0));
    }

    argnum = params->ParamPresent("-dump_zlwll_nth_end");
    for (int i=0; i<argnum; ++i)
    {
        m_DumpZlwllList.push(params->ParamNUnsigned("-dump_zlwll_nth_end",i,0));
    }

    argnum = params->ParamPresent("-dump_buffer_every_begin");
    for (int i=0; i<argnum; ++i)
    {
        const char* buf_name = params->ParamNStr("-dump_buffer_every_begin", i, 0);
        TraceModule* tmodule = ModFind( buf_name );
        if ( !tmodule )
        {
            ErrPrintf("Could not find module %s for option -dump_buffer_every_begin\n", buf_name);
            return RC::ILWALID_FILE_FORMAT;
        }

        // Buffer dumpers use the cached surfaces of trace modules directly
        // which is not supported in peer modules
        if (tmodule->IsPeer())
        {
            ErrPrintf("Cannot dump peer module %s\n", buf_name);
            return RC::ILWALID_FILE_FORMAT;
        }

        m_DumpBufferList.push_back(tmodule);
    }

    argnum = params->ParamPresent("-dump_buffer_every_end");
    for (int i=0; i<argnum; ++i)
    {
        const char* buf_name = params->ParamNStr("-dump_buffer_every_end", i, 0);
        TraceModule* tmodule = ModFind(buf_name);
        if (!tmodule)
        {
            ErrPrintf("Could not find module %s for option -dump_buffer_every_end\n", buf_name);
            return RC::ILWALID_FILE_FORMAT;
        }

        // Buffer dumpers use the cached surfaces of trace modules directly
        // which is not supported in peer modules
        if (tmodule->IsPeer())
        {
            ErrPrintf("Cannot dump peer module %s\n", buf_name);
            return RC::ILWALID_FILE_FORMAT;
        }

        m_DumpBufferEndList.push_back(tmodule);
    }

    argnum = params->ParamPresent("-dump_buffer_nth_begin");
    for (int i=0; i<argnum; ++i)
    {
        const char* buf_name = params->ParamNStr("-dump_buffer_nth_begin", i, 0);
        TraceModule* tmodule = ModFind( buf_name );
        if ( !tmodule )
        {
            ErrPrintf("Could not find module %s for option -dump_buffer_nth_begin\n", buf_name);
            return RC::ILWALID_FILE_FORMAT;
        }

        // Buffer dumpers use the cached surfaces of trace modules directly
        // which is not supported in peer modules
        if (tmodule->IsPeer())
        {
            ErrPrintf("Cannot dump peer module %s\n", buf_name);
            return RC::ILWALID_FILE_FORMAT;
        }

        m_DumpBufferBeginMaps.insert(pair<UINT32, TraceModule*>(
                params->ParamNUnsigned("-dump_buffer_nth_begin", i, 1), tmodule));
    }

    argnum = params->ParamPresent("-dump_buffer_nth_end");
    for (int i=0; i<argnum; ++i)
    {
        const char* buf_name = params->ParamNStr("-dump_buffer_nth_end", i, 0);
        TraceModule* tmodule = ModFind( buf_name );
        if ( !tmodule )
        {
            ErrPrintf("Could not find module %s for option -dump_buffer_nth_end\n", buf_name);
            return RC::ILWALID_FILE_FORMAT;
        }

        // Buffer dumpers use the cached surfaces of trace modules directly
        // which is not supported in peer modules
        if (tmodule->IsPeer())
        {
            ErrPrintf("Cannot dump peer module %s\n", buf_name);
            return RC::ILWALID_FILE_FORMAT;
        }

        m_DumpBufferEndMaps.insert(pair<UINT32, TraceModule*>(
                params->ParamNUnsigned("-dump_buffer_nth_end", i, 1), tmodule));
    }

    argnum = params->ParamPresent("-skip_crc_check");
    for (int i=0; i<argnum; ++i)
    {
        const char* buf_name = params->ParamNStr("-skip_crc_check", i, 0);
        TraceModule* tmodule = ModFind(buf_name);
        if (!tmodule)
        {
            ErrPrintf("Could not find module %s for option -skip_crc_check\n", buf_name);
            return RC::ILWALID_FILE_FORMAT;
        }
    }

    m_DumpBufferBeginMapsIter = m_DumpBufferBeginMaps.begin();
    m_DumpBufferEndMapsIter = m_DumpBufferEndMaps.begin();

    return OK;
}

bool GpuTrace::DumpSurfaceRequested() const
{
    return (m_DumpImageEveryEnd || !m_DumpImageEveryEndList.empty() ||
        m_DumpImageEveryBegin || !m_DumpImageEveryBeginList.empty()) ||
        params->ParamPresent("-dump_image_range_begin") > 0   ||
        params->ParamPresent("-dump_image_range_end") > 0     ||
        params->ParamPresent("-dump_image_every_n_begin") > 0 ||
        params->ParamPresent("-dump_image_every_n_end") > 0 ||
        params->ParamPresent("-dump_image_every_method") > 0;
}

bool GpuTrace::DumpBufRequested() const
{
    return( m_DumpZlwllEveryEnd ||
            !m_DumpZlwllList.empty() ||
            !m_DumpBufferList.empty() ||
            !m_DumpBufferEndList.empty() ||
            !m_DumpBufferBeginMaps.empty() ||
            !m_DumpBufferEndMaps.empty());
}

RC GpuTrace::CreateBufferDumper()
{
    // MdiagSurf dumping arguments need to processed after all modules allocated
    RC rc = ProcessBufDumpArgs();

    if (DumpBufRequested() || DumpSurfaceRequested())
        m_Dumper = new BufferDumperFermi(m_Test, params);

    return rc;
}

void GpuTrace::AddTraceModule( TraceModule * pTraceModule )
{
    m_Modules.push_back( pTraceModule );
    if (pTraceModule->IsPeer())
    {
        m_PeerModules.push_back(pTraceModule);
    }
    else
    {
        m_GenericModules.push_back(pTraceModule);
    }
}

// Store per-file sph setting from option to map
RC GpuTrace::ProcessSphArgs()
{
    // Keep this updated when new args are added to trace_3d.cpp
    vector<string> SphArgs;
    SphArgs.push_back("-sph_MrtEnable_one");
    SphArgs.push_back("-sph_KillsPixels_one");
    SphArgs.push_back("-sph_DoesGlobalStore_one");
    SphArgs.push_back("-sph_SassVersion_one");
    SphArgs.push_back("-sph_DoesLoadOrStore_one");
    SphArgs.push_back("-sph_DoesFp64_one");
    SphArgs.push_back("-sph_StreamOutMask_one");
    SphArgs.push_back("-sph_ShaderLocalMemoryLowSize_one");
    SphArgs.push_back("-sph_PerPatchAttributeCount_one");
    SphArgs.push_back("-sph_ShaderLocalMemoryHighSize_one");
    SphArgs.push_back("-sph_ThreadsPerInputPrimitive_one");
    SphArgs.push_back("-sph_ShaderLocalMemoryCrsSize_one");
    SphArgs.push_back("-sph_OutputTopology_one");
    SphArgs.push_back("-sph_MaxOutputVertexCount_one");
    SphArgs.push_back("-sph_StoreReqStart_one");
    SphArgs.push_back("-sph_StoreReqEnd_one");

    vector<string>::iterator end = SphArgs.end();
    for (vector<string>::iterator iter = SphArgs.begin(); iter != end; ++iter)
    {
        string argname = *iter;
        UINT32 count   = params->ParamPresent(argname.c_str());
        for (UINT32 i = 0; i < count; ++i)
        {
            string filename = params->ParamNStr(argname.c_str(), i, 0);
            UINT32 value    = params->ParamNUnsigned(argname.c_str(), i, 1);

            TraceModule *mod = ModFind(filename.c_str());
            if (!mod || mod->GetFileType() != FT_SHADER_PROGRAM)
            {
                ErrPrintf("SPH option '%s': Invalid shader file %s\n", argname.c_str(), filename.c_str());
                return RC::ILWALID_FILE_FORMAT;
            }
            Attr2ValMap& setting = m_SphInfo[filename];
            setting[argname] = value;
        }
    }

    return OK;
}

// release all cached surfaces
void GpuTrace::ReleaseAllCachedSurfs()
{
    ModuleIter modIt;
    for (modIt = m_GenericModules.begin(); modIt != m_GenericModules.end(); ++modIt)
    {
        DebugPrintf(MSGID(TraceParser), "Release cached surfaces of module %s for all subdevs.\n",
            (*modIt)->GetName().c_str());

        CachedSurface* cachedSurf = (*modIt)->GetCachedSurface();
        if (NULL != cachedSurf)
        {
            for (UINT32 subdev = 0;
                 subdev < cachedSurf->GetSubdeviceNumber();
                 ++subdev)
            {
                // It doesn't matter if cached surface has been released
                (*modIt)->ReleaseCachedSurface(subdev);
            }
        }
    }
}

DmaLoader* GpuTrace::GetDmaLoader(TraceChannel *channel)
{
    if (m_DmaLoaders[channel] == 0)
    {
        DmaLoader *loader = DmaLoader::CreateDmaLoader(params, channel);
        if (loader == 0)
        {
            ErrPrintf("Fail to create a dma loader!\n");
            return 0;
        }
        m_DmaLoaders[channel] = loader;
        return loader;
    }
    return m_DmaLoaders[channel];
}

// Enable any non-dynamic Color or Z surfaces with the SurfaceManager.
RC GpuTrace::EnableColorSurfaces(IGpuSurfaceMgr *surfaceManager)
{
    RC rc = OK;
    ModuleIter moduleIter;

    for (moduleIter = m_Modules.begin();
         moduleIter != m_Modules.end();
         ++moduleIter)
    {
        TraceModule *module = *moduleIter;

        if (module->IsColorOrZ() && !module->IsDynamic())
        {
            MdiagSurf *surface = surfaceManager->EnableSurface(
                module->GetSurfaceType(), module->GetParameterSurface(),
                module->GetLoopback(), module->GetPeerIDs());

            module->SaveTrace3DSurface(surface);
        }
    }

    return rc;
}

GpuTrace::MemoryCommandProperties::MemoryCommandProperties()
{
    file = "";
    format = "A8R8G8B8";
    sampleMode = "1X1";
    layout = "BLOCKLINEAR";
    compression = "NONE";
    zlwll = "NONE";
    aperture = "VIDEO";
    type = "IMAGE";
    access = "READ_WRITE";
    privileged = "NO";
    crcCheck = "";
    referenceCheck = "";
    ilwertCrcCheck = false;
    crcCheckCount = 0;
    gpuCache = "";
    p2pGpuCache = "";
    zbc = "";
    crcRect = "0:0:0:0";
    parentSurface = "";
    traceChannel = "";
    width = 0;
    height = 0;
    depth = 1;
    pitch = 0;
    blockWidth = 1;
    blockHeight = 16;
    blockDepth = 1;
    arraySize = 1;
    swapSize = 0;
    physAlignment = 0;
    virtAlignment = 0;
    size = 0;
    attrOverride = 0;
    attr2Override = 0;
    typeOverride = 0;
    loopback = false;
    raw = false;
    rawCrc = false;
    skedReflected = false;
    contig = false;
    virtAddress = 0;
    physAddress = 0;
    virtAddressMin = 0;
    virtAddressMax = 0;
    physAddressMin = 0;
    physAddressMax = 0;
    addressOffset = 0;
    virtualOffset = 0;
    physicalOffset = 0;
    mipLevels = 1;
    border = 0;
    lwbemap = false;
    sparse = false;
    classNum = CrcChainManager::UNSPECIFIED_CLASSNUM;
    mapToBackingStore = false;
    tileWidthInGobs = 0;
    htex = false;
    isShared = false;
    smCount = 0;
    pteKind = 0;
    inheritPteKind = "";
}

RC GpuTrace::UpdateTrapHandler()
{
    if (m_TrapHandlerModules.empty())
    {
        return OK;
    }

    // Get the trap handler file name
    string name;
    if (params->ParamPresent("-trap_handler"))
    {
        name = params->ParamStr("-trap_handler");
    }
    else
    {
        name = Gpu::DeviceIdToInternalString(m_Test->GetBoundGpuDevice()->GetSubdevice(0)->DeviceId());
        name += "_trap_handler.bin";
    }
    name = LwDiagUtils::StripDirectory(name.c_str());

    TraceModule *mod = ModFind(name.c_str());
    if (mod && mod->RelocEnd() != mod->RelocBegin())
    {
        // Trap Handler is from a buffer that's declared in header file, and it was RELOCed
        // Copy file content from cached surface which has done RELOCs
        InfoPrintf("Updating TrapHandler with content from %s\n", mod->GetName().c_str());
        GenericTraceModule* pTrapHandler = GetTrapHandlerModule(mod->GetVASpaceHandle());
        pTrapHandler->StoreInlineData(mod->Get032Addr(0, 0), pTrapHandler->GetSize());
    }
    else
    {
        DebugPrintf(MSGID(TraceParser), "Updating TrapHandler - skipped since buffer was not RELOCed\n");
    }

    return OK;
}

RC GpuTrace::CreateTrapHandlers(UINT32 hwClass)
{
    RC rc;

    if (m_TrapHandlerModules.empty())
    {
        FILE* pf = 0;
        TraceFileMgr::Handle h = 0;
        long size = 0;
        vector<UINT08> data;
        RC traceFileRC = OK;
        if (params->ParamPresent("-trap_handler"))
        {
            // try looking in the trace for the handler.
            string fullname = params->ParamStr("-trap_handler");
            if (params->ParamPresent("-file_search_order"))
            {
                if (params->ParamUnsigned("-file_search_order") == 0)
                {
                    traceFileRC = m_TraceFileMgr->Open(fullname, &h);
                    if (OK != traceFileRC)
                    {
                        Utility::OpenFile(fullname, &pf, "rb");
                    }
                }
                else
                {
                    Utility::OpenFile(fullname, &pf, "rb");
                    if (!pf)
                    {
                        traceFileRC = m_TraceFileMgr->Open(fullname, &h);
                    }
                }
            }
            else
            {
                // prefer to look in archive
                traceFileRC = m_TraceFileMgr->Open(fullname, &h);
                if (OK != traceFileRC)
                {
                    Utility::OpenFile(fullname, &pf, "rb");
                }
            }
        }
        else
        {
            if (hwClass < MAXWELL_A)
            {
                Gpu::LwDeviceId Id;
                Id = m_Test->GetBoundGpuDevice()->GetSubdevice(0)->DeviceId();
                string filename(Gpu::DeviceIdToInternalString(Id));
                filename += "_trap_handler.bin";
                CHECK_RC(Utility::OpenFile(filename.c_str(), &pf, "rb"));

                DebugPrintf(MSGID(TraceParser), "Loading TrapHandler from file %s\n", filename.c_str());
            }
            else  //don't load default traphandler for chips after maxwell
            {
                DebugPrintf(MSGID(TraceParser), "Skip loading default TrapHandler for chips after maxwell\n");
                return OK;
            }
        }
        if (pf)
        {
            Utility::FileSize(pf, &size);
            data.resize(size);
            if (!fread(&data[0], size, 1, pf))
            {
                fclose(pf);
                return RC::FILE_EXIST;
            }
            fclose(pf);
        }
        else if (OK == traceFileRC)
        {
            size = m_TraceFileMgr->Size(h);
            data.resize(size);
            m_TraceFileMgr->Read(&data[0], h);
        }
        else
        {
            return RC::FILE_EXIST;
        }

        LWGpuResource *pGpu = m_Test->GetGpuResource();
        LWGpuResource::VaSpaceManager *pVaSpacemgr = pGpu->GetVaSpaceManager(m_Test->GetLwRmPtr());
        if (params->ParamPresent("-address_space_name"))
        {
            CreateTrapHandlerForVAS(GetVAS("default"), &data[0], size);
        }
        else
        {
            for( LWGpuResource::VaSpaceManager::iterator it = pVaSpacemgr->begin();
                    it != pVaSpacemgr->end(); ++it)
            {
                if (it->first == m_Test->GetTestId())
                {
                    CreateTrapHandlerForVAS(it->second, &data[0], size);
                }
            }
        }
    }
    return OK;
}

void GpuTrace::CreateTrapHandlerForVAS(shared_ptr<VaSpace> pVa, UINT08* data, UINT32 size)
{
    // don't append vas name for default vas trap handler
    // for back compatible with legacy (plugin) code which use original name directly.
    string name = "TrapHandler";
    if (pVa != GetVAS("default"))
        name += "_" + pVa->GetName();
    GenericTraceModule* pMod = new GenericTraceModule(m_Test,
            name, GpuTrace::FT_SHADER_PROGRAM, size);
    LwRm::Handle hVASpace = pVa->GetHandle();
    pMod->SetVASpaceHandle(hVASpace);
    m_Modules.push_back((TraceModule *)pMod);
    m_GenericModules.push_back((TraceModule *)pMod);
    pMod->StoreInlineData(&data[0], size);
    m_TrapHandlerModules.push_back(pMod);
}

GenericTraceModule* GpuTrace::GetTrapHandlerModule(LwRm::Handle hVASpace)
{
    for(unsigned int i = 0; i < m_TrapHandlerModules.size(); ++i)
     {
         if(m_TrapHandlerModules[i]->GetVASpaceHandle() == hVASpace)
             return m_TrapHandlerModules[i];
     }
     return 0;
}

// Add virtual allocations as specified by the -add_virtual_alloc
// command-line argument.
//
RC GpuTrace::AddVirtualAllocations()
{
    UINT32 i;
    UINT32 bufferCount = params->ParamPresent("-add_virtual_alloc");

    for (i = 0; i < bufferCount; ++i)
    {
        string name;

        // Update a static member to avoid collisions with multiple
        // extra virtual allocations, but also check to make sure there
        // aren't any name collisions with the trace buffers.
        // (Exceedingly unlikely, but you never know.)
        do
        {
            ++m_ExtraVirtualAllocations;

            name = Utility::StrPrintf("__add_virtual_alloc_%u",
                m_ExtraVirtualAllocations);

        } while (ModFind(name.c_str()) != 0);

        MdiagSurf surface;
        UINT64 addressMin = params->ParamNUnsigned64("-add_virtual_alloc",i,0);
        UINT64 addressMax = params->ParamNUnsigned64("-add_virtual_alloc",i,1);

        if (addressMin >= addressMax)
        {
            ErrPrintf("Invalid -add_virtual_alloc address range 0x%llx - 0x%llx.\n",
                      addressMin, addressMax);

            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        UINT64 bufferSize = addressMax - addressMin;

        surface.SetArrayPitch(bufferSize);
        surface.SetColorFormat(ColorUtils::Y8);
        surface.SetFixedVirtAddr(addressMin);
        surface.SetSpecialization(Surface2D::VirtualOnly);

        SurfaceTraceModule *module = new SurfaceTraceModule(m_Test, name,
            "", surface, 0, SURFACE_TYPE_UNKNOWN);

        m_Modules.push_back(module);
        m_GenericModules.push_back(module);
    }

    return OK;
}

RC GpuTrace::AddChannel
(
    const string &chName,
    const string &tsgName,
    ChannelManagedMode mode,
    const shared_ptr<VaSpace> &pVaSpace,
    const shared_ptr<SubCtx> &pSubCtx
)
{
    RC rc;

    if (0 != GetChannel(chName, mode))
    {
        // duplicate channel name
        ErrPrintf("%s: failed because of dumplicated channel name.\n",
                  __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    m_TraceChannelObjects.emplace_back(chName, m_Test->GetBuffInfo(), m_Test);

    TraceChannel *pTraceCh = &m_TraceChannelObjects.back();

    if (!tsgName.empty() && !m_IgnoreTsg)
    {
        TraceTsg * pTraceTsg = GetTraceTsg(tsgName);
        if (0 == pTraceTsg)
        {
            ErrPrintf("%s: Can't find the TSG specified by name %s.\n",
                      __FUNCTION__, tsgName.c_str());
            return RC::SOFTWARE_ERROR;
        }

        bool isGpuManagedCh = GpuTrace::GPU_MANAGED == mode;
        CHECK_RC(pTraceCh->SetTraceTsg(pTraceTsg));
        CHECK_RC(pTraceCh->SetGpuManagedChannel(isGpuManagedCh));
        CHECK_RC(pTraceTsg->AddTraceChannel(pTraceCh));
    }
    pTraceCh->SetVASpace(pVaSpace);
    if (!params->ParamPresent("-ignore_subcontexts"))
    {
        pTraceCh->SetSubCtx(pSubCtx);
    }
    return rc;
}

TraceChannel * GpuTrace::GetChannel
(
    const string &chname,
    ChannelManagedMode mode
)
{
    //
    // To reuse the name of freed channel(dumplicated name) in parseing
    // stage, validation in parse stage besides name and mode need
    // to be checked.
    TraceChannelObjects::iterator it;
    for (it = m_TraceChannelObjects.begin();
         it != m_TraceChannelObjects.end();
         it ++)
    {
        if (it->IsValidInParsing() &&
            0 == it->GetName().compare(chname) &&
            it->MatchesChannelManagedMode(mode))
        {
            return &(*it);
        }
    }

    return 0;
}

TraceChannel* GpuTrace::GetLwrrentChannel(ChannelManagedMode mode)
{
    TraceChannelObjects::reverse_iterator rIt;
    for (rIt = m_TraceChannelObjects.rbegin();
         rIt != m_TraceChannelObjects.rend();
         rIt ++)
    {
        if (rIt->MatchesChannelManagedMode(mode))
        {
            return &(*rIt);
        }
    }

    return 0;
}

TraceChannel* GpuTrace::GetGrChannel(ChannelManagedMode mode)
{
    TraceChannelObjects::iterator it;
    for (it = m_TraceChannelObjects.begin();
         it != m_TraceChannelObjects.end();
         it ++)
    {
        if (it->GetGrSubChannel() &&
            it->MatchesChannelManagedMode(mode))
        {
            return &(*it);
        }
    }

    return 0;
}

TraceChannel* GpuTrace::GetChannelHasSubch
(
    const string &name,
    ChannelManagedMode mode
)
{
    TraceChannelObjects::iterator it;
    for (it = m_TraceChannelObjects.begin();
         it != m_TraceChannelObjects.end();
         it ++)
    {
        if (GetSubChannel(&(*it), name) &&
            it->MatchesChannelManagedMode(mode))
        {
            return &(*it);
        }
    }

    return 0;
}

void GpuTrace::GetComputeChannels
(
    vector<TraceChannel*>& compute_channels,
    ChannelManagedMode mode
)
{
    compute_channels.clear();
    TraceChannelObjects::iterator it;
    for (it = m_TraceChannelObjects.begin();
         it != m_TraceChannelObjects.end();
         ++it)
    {
        if (it->GetComputeSubChannel() &&
            it->MatchesChannelManagedMode(mode))
        {
            compute_channels.push_back(&(*it));
        }
    }
}

TraceSubChannel* GpuTrace::GetSubChannel
(
    TraceChannel* pTraceChannel,
    const string& subChannelName
)
{
    //
    // This function is different from TraceChannel::GetSubChannel().
    // To reuse the name of freed subchannel(dumplicated name) in
    // parseing stage, validation in parse stage besides name and mode
    // need to be checked.
    if (pTraceChannel)
    {
        TraceSubChannelList::iterator it = pTraceChannel->SubChBegin();
        for (; it != pTraceChannel->SubChEnd(); it++)
        {
            if (0 == (*it)->GetName().compare(subChannelName) &&
                (*it)->IsValidInParsing())
            {
                return *it;
            }
        }
    }

    return 0;
}

TraceTsg* GpuTrace::GetTraceTsg(const string &tsgName)
{
    TraceTsgObjects::iterator it;
    for (it = m_TraceTsgObjects.begin(); it != m_TraceTsgObjects.end(); ++it)
    {
        if (it->GetName() == tsgName)
            return &(*it);
    }
    return 0;
}

RC GpuTrace::AddTraceTsg
(
    const string &tsgName
    ,const shared_ptr<VaSpace> &pVaSpace
    ,bool isShared
)
{
    if (0 != GetTraceTsg(tsgName))
    {
        return RC::SOFTWARE_ERROR;
    }

    m_TraceTsgObjects.push_back(TraceTsg(tsgName, m_Test, pVaSpace, isShared));

    return OK;
}

RC GpuTrace::ExelwteT3dCommands(const char* commands)
{
    RC rc;

    // Real parsing begins
    // ?? whethet it will call after readheader
    m_pParseFile.reset(new ParseFile(m_Test, params));

    CHECK_RC(m_pParseFile->ReadCommandLine(commands));

    swap(m_TestHdrTokens, m_pParseFile->GetTokens());

    rc = DoParseTestHdr();

    // Cleanup
    m_TestHdrTokens.clear();

    return rc;
}

void GpuTrace::ClearDmaLoaders()
{
    std::map<TraceChannel*, DmaLoader*>::iterator it = m_DmaLoaders.begin();
    while (it != m_DmaLoaders.end())
    {
        delete it->second;
        ++it;
    }
    m_DmaLoaders.clear();
}

LwRm::Handle GpuTrace::GetVASHandle(const string &name)
{
    shared_ptr<VaSpace> pVaSpace = GetVAS(name);
    return pVaSpace->GetHandle();
}

RC GpuTrace::ParseADDRESS_SPACE()
{
    RC rc;
    string name;
    const char * const commandStr = "ADDRESS_SPACE: <name> [ATS] expected.\n";

    CHECK_RC(RequireVersion(4, "ADDRESS_SPACE"));

    if (OK != ParseString(&name))
    {
        ErrPrintf(commandStr);
        return RC::ILWALID_FILE_FORMAT;
    }

    string property;
    bool atsEnabled = false;
    while (ParseFile::s_NewLine != m_TestHdrTokens.front())
    {
        if (ParseString(&property) == OK)
        {
            if (property == "ATS")
            {
                atsEnabled = true;
            }
            else
            {
                ErrPrintf("Unexpected token in ADDRESS_SPACE command: %s\n", property.c_str());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else
        {
            ErrPrintf(commandStr);
            return RC::ILWALID_FILE_FORMAT;
        }
    }

    LWGpuResource * pGpu = m_Test->GetGpuResource();
    LWGpuResource::VaSpaceManager * pVaSpaceMgr = pGpu->GetVaSpaceManager(m_Test->GetLwRmPtr());
    if (params->ParamPresent("-address_space_name"))
    {
        string aliasName = params->ParamStr("-address_space_name");
        pVaSpaceMgr->CreateResourceObjectAlias(m_Test->GetTestId(), name,
                                               LWGpuResource::TEST_ID_GLOBAL, aliasName);
    }
    else
    {
        shared_ptr<VaSpace> pVaSpace = pVaSpaceMgr->CreateResourceObject(m_Test->GetTestId(), name);

        if (atsEnabled)
        {
            pVaSpace->SetAtsEnabled();
        }

        m_Test->CreateChunkMemory(pVaSpace->GetHandle());
    }
    return rc;
}

RC GpuTrace::ParseSUBCONTEXT()
{
    RC rc;
    string tok, tsgName, vasName, subctxName;
    UINT32 veid;
    bool isSetVeid = false;

    CHECK_RC(RequireVersion(4, "SUBCONTEXT"));

    if (OK != ParseString(&subctxName))
    {
        ErrPrintf("SUBCONTEXT need a <subctx name> arg.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    // 1. create the subcontext object
    LWGpuResource * pGpu = m_Test->GetGpuResource();
    LWGpuResource::SubCtxManager * pSubCtxMgr = pGpu->GetSubCtxManager(m_Test->GetLwRmPtr());
    shared_ptr<SubCtx> pSubCtx;
    UINT32 testId = m_Test->GetTestId();

    if (params->ParamPresent("-subcontext_name"))
    {
        string refName = params->ParamStr("-subcontext_name");
        pSubCtx = pSubCtxMgr->CreateResourceObjectAlias(testId, subctxName,
                LWGpuResource::TEST_ID_GLOBAL, refName);
    }
    else
    {
        if(params->ParamPresent("-extern_trace_subcontext"))
            testId = LWGpuResource::TEST_ID_GLOBAL;
        pSubCtx = pSubCtxMgr->CreateResourceObject(testId, subctxName);
    }

    // 2. parse the token
    while(OK == ParseString(&tok))
    {
        if (tok == "TSG")
        {
            if (OK != ParseString(&tsgName))
            {
                ErrPrintf("SUBCONTEXT need a <tsg name> arg.\n");
                return RC::ILWALID_FILE_FORMAT;
            }

        }
        else if (tok == "ADDRESS_SPACE")
        {
            if (OK != ParseString(&vasName))
            {
                ErrPrintf("SUBCONTEXT need a <vas name> arg.\n");
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (tok == "VEID")
        {
            isSetVeid = true;
            if (OK != ParseUINT32(&veid))
            {
                ErrPrintf("SUBCONTEXT need a <veid> arg.\n");
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else
        {
            ErrPrintf("SUBCONTEXT can't this keyword %s.\n", tok.c_str());
            return RC::ILWALID_FILE_FORMAT;
        }
    }

    // 3. get vaspace and check for error from hdr
    shared_ptr<VaSpace> pVaSpace = GetVAS(vasName);

    // 4. initialize the subctx object
    if (params->ParamPresent("-subcontext_veid") || isSetVeid)
    {
        veid = params->ParamUnsigned("-subcontext_veid", veid);
        CHECK_RC(pSubCtx->SetSpecifiedVeid(veid));
    }

    CHECK_RC(SanityCheckTSG(pSubCtx, &tsgName));
    if (tsgName.empty())
    {
        ErrPrintf("SUBCONTEXT need a <tsg name> arg.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    CHECK_RC(pSubCtx->SetVaSpace(pVaSpace));

    return rc;
}

RC GpuTrace::ParseDEPENDENCY()
{
    const char *syntax = "Syntax: DEPENDENCY <name> DEPEND_ON <name> [ <name> <name>... ]\n";

    RC rc;
    string traceOpName, dependentTraceOpName;
    string temp;

    CHECK_RC(RequireVersion(4, "DEPENDENCY"));

    //mark as dependent trace
    m_ContainExelwtionDependency = true;

    if (OK != ParseString(&traceOpName))
    {
        goto error;
    }

    if (OK != ParseString(&temp))
    {
        goto error;
    }

    if (temp != "DEPEND_ON")
    {
        goto error;
    }

    while (ParseFile::s_NewLine != m_TestHdrTokens.front())
    {
        if (OK != ParseString(&dependentTraceOpName))
        {
            goto error;
        }
        rc = AddTraceOpDependency(traceOpName, dependentTraceOpName);
        
        if (OK != rc)
        {
            break;
        }
    }

    return rc;

    error:
    ErrPrintf(syntax);
    return RC::ILWALID_FILE_FORMAT;

}

//----------------------------------------------------------
// import utl script 
RC GpuTrace::ParseUTL_SCRIPT()
{
    RC rc;
    string scriptPath;
    string tok;
    string scopeName;
    
    // Support UTL_SCRIPT <script name> arguments... optional arguments...
    // Do the py::global copy and py::eval_file
    if (OK != ParseString(&scriptPath))
    {
        ErrPrintf("UTL_SCRIPT requires a <name> arg.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    // Optional argument support
    string lwrrentScriptArgs;
    while (OK == ParseString(&tok))
    {
        if (0 == strcmp("SCOPE_NAME", tok.c_str()))
        {
            if (OK != ParseString(&scopeName))
            {
                ErrPrintf("Error parsing '%s' in UTL_SCRIPT.\n", tok.c_str());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp("SCRIPT_ARGS", tok.c_str()))
        {
            if (OK != ParseString(&lwrrentScriptArgs))
            {
                ErrPrintf("Error parsing '%s' in UTL_SCRIPT.\n", tok.c_str());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else
        {
            ErrPrintf("UnSupported token '%s' in UTL_SCRIPT.\n", tok.c_str());
            return RC::ILWALID_FILE_FORMAT;
        }
    }

    TraceOpUtlRunScript * pOp = new TraceOpUtlRunScript(m_Test,
            scriptPath, lwrrentScriptArgs, scopeName);
    
    InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);

    return rc;
}

//----------------------------------------------------------
// Insert the script/func into traceOps 
RC GpuTrace::ParseUTL_RUN()
{
    string expression;
    string tok;
    RC rc;
    string scopeName;

    // Support UTL_RUN <module::func/func>(arguments... ) optional argument...
    while (OK == ParseString(&tok))
    {
        // support the optional argument
        if (0 == strcmp("SCOPE_NAME", tok.c_str()))
        {
            if (OK != ParseString(&scopeName))
            {
                ErrPrintf("Error parsing '%s' in UTL_RUN.\n", tok.c_str());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (0 == strcmp("EXPRESSION", tok.c_str()))
        {
            if (OK != ParseString(&expression))
            {
                ErrPrintf("Error parsing '%s' in UTL_RUN.\n", tok.c_str());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else
        {
            ErrPrintf("UnSupported token '%s' in UTL_RUN.\n", tok.c_str());
            return RC::ILWALID_FILE_FORMAT;
        }
    }
   
    if (expression.empty())
    {
        ErrPrintf("UTL_RUN requires a <module::func>(arguments... ) arg.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    TraceOpUtlRunFunc * pOp = new TraceOpUtlRunFunc(m_Test, expression, scopeName);
    InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);
    
    return rc;
}

// Support UTL_RUN inline format
RC GpuTrace::ParseUTL_RUN_BEGIN()
{
    RC rc;
    string expression;
    string tok;
    string scopeName;
    
    while (OK == ParseString(&tok))
    {
        // pass optional argument
        if (0 == strcmp("SCOPE_NAME", tok.c_str()))
        {
            if (OK != ParseString(&scopeName))
            {
                ErrPrintf("Error parsing '%s' in UTL_RUN_BEGIN.\n", tok.c_str());
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else
        {
            ErrPrintf("Unsupported tok '%s' in UTL_RUN_BEGIN.\n", tok.c_str());
            return RC::ILWALID_FILE_FORMAT;
        }
    }

    // Support UTL inline
    // UTL_RUN_BEGIN optional argument...
    // ...
    // UTL_RUN_END
    if (!m_pParseFile.get()->GetInlineBuf().empty())
    {
        swap(m_InlineHdrTokens, m_pParseFile.get()->GetInlineBuf());
    }
    while (strcmp(m_InlineHdrTokens.front(), "UTL_RUN_END") != 0)
    {
        tok = m_InlineHdrTokens.front();
        m_InlineHdrTokens.pop_front();
        expression += tok;
    }
    
    // pop the UTL_RUN_END and "\n"
    m_InlineHdrTokens.pop_front();
    m_InlineHdrTokens.pop_front();

    TraceOpUtlRunInline * pOp = new TraceOpUtlRunInline(m_Test, expression, scopeName);
    InsertOp(m_LineNumber, pOp, OP_SEQUENCE_OTHER);
    return rc;
}

//----------------------------------------------------------
// input: subCtxName: the subctx name from the hdr
// output: return the valid subctx pointer
shared_ptr<SubCtx> GpuTrace::GetSubCtx(const string& subCtxName)
{
    LWGpuResource * pGpu = m_Test->GetGpuResource();
    LWGpuResource::SubCtxManager * pSubCtxMgr = pGpu->GetSubCtxManager(m_Test->GetLwRmPtr());
    shared_ptr<SubCtx> pSubCtx;

    if (subCtxName.empty() && !params->ParamPresent("-subcontext_name"))
    {
        return pSubCtx;
    }

    if (!subCtxName.empty())
    {
        // avoid the following situation
        // 'CHANNEL ch1 SUBCONTEXT subctx1' while subctx1 have not been declared before
        if(params->ParamPresent("-extern_trace_subcontext"))
            pSubCtx = pSubCtxMgr->GetResourceObject(LWGpuResource::TEST_ID_GLOBAL, subCtxName);
        else
            pSubCtx = pSubCtxMgr->GetResourceObject(m_Test->GetTestId(), subCtxName);
    }
    else
    {
        string name = params->ParamStr("-subcontext_name");
        pSubCtx = pSubCtxMgr->GetResourceObject(LWGpuResource::TEST_ID_GLOBAL, name);
    }
    if (!pSubCtx)
    {
        ErrPrintf("Please check your hdr file. The subctx name %s doesn't exist!\n", subCtxName.c_str());
        MASSERT(0);
    }
    return pSubCtx;
}

//----------------------------------------------------------
// input: subCtxName: the subctx name from the hdr
// output: return the valid subctx pointer
shared_ptr<VaSpace> GpuTrace::GetVAS(const string& vasName)
{
    LWGpuResource * pGpu = m_Test->GetGpuResource();
    LWGpuResource::VaSpaceManager * pVaSpaceMgr = pGpu->GetVaSpaceManager(m_Test->GetLwRmPtr());
    shared_ptr<VaSpace> pVaSpace;
    if (!vasName.empty() && (vasName != "default"))
    {
        // avoid the following situation which not define the vas before
        // //ADDRESS_SPACE vas1
        // SUBCONTEXT subctx1 ADDRESS_SPACE vas1
        pVaSpace = pVaSpaceMgr->GetResourceObject(m_Test->GetTestId(), vasName);
    }
    else
    {
        pVaSpace = pVaSpaceMgr->GetResourceObject(m_Test->GetTestId(), "default");
    }

    if (!pVaSpace)
    {
        ErrPrintf("Please check your hdr file. The vaspace name %s doesn't exist!\n", vasName.c_str());
        MASSERT(0);
    }

    return pVaSpace;
}

RC GpuTrace::SanityCheckTSG(shared_ptr<SubCtx> pSubCtx, string * pTsgName)
{
    RC rc;

    string tsgName = params->ParamStr("-tsg_name", pTsgName->c_str());

    if (!tsgName.empty())
    {
        TraceTsg * pTraceTsg = GetTraceTsg(tsgName);
        // 1. For the TSG comes from the command line, so the trace tsg has not been created
        // 2. Fot the TSG comes from the SUBCONTEXT in hdr or the CHANNEL, the trace tsg should be created before
        if (0 == pTraceTsg)
        {
            if (params->ParamPresent("-tsg_name"))
            {
                if (pSubCtx)
                {
                    // if the tsg has subctx, the hvaspace in tsg is 0
                    AddTraceTsg(tsgName, shared_ptr<VaSpace>(), true);
                }
                else
                {
                    AddTraceTsg(tsgName, GetVAS("default"), true);
                }
            }
            else
            {
                ErrPrintf("invalid TSG name \"%s\".\n", tsgName.c_str());
                return RC::ILWALID_FILE_FORMAT;
            }
        }

        if(pSubCtx.get())
        {
            TraceTsg * pTraceTsg = GetTraceTsg(tsgName);
            // For share subctx and share tsg, new traceTsg Object will
            // be inserted the partition mode in case it lost some info.
            // ToDo: This logical will have potential problem that last
            //       TraceTsg information will be lost. Not sure whether
            //       other information will be need via pSubCtx()->GetTraceTsg().
            if(pSubCtx->GetTraceTsg() != NULL &&
                pSubCtx->GetTraceTsg()->GetName() == tsgName &&
                pSubCtx->GetTraceTsg()->IsSharedTsg())
            {
                PEConfigureFile::PARTITION_MODE partitionMode =
                    pSubCtx->GetTraceTsg()->GetPartitionMode();
                pTraceTsg->SetPartitionMode(partitionMode);
            }
            CHECK_RC(pSubCtx->SetTraceTsg(pTraceTsg));
            if(params->ParamPresent("-partition_table") &&
                !pSubCtx->IsSetTpcMask())
                CHECK_RC(ConfigureSubCtxPETable(pSubCtx));
        }
    }
    else if (pSubCtx && pSubCtx->GetTraceTsg())
    {
        tsgName = pSubCtx->GetTraceTsg()->GetName();
    }

    *pTsgName = tsgName;
    return rc;
}

RC GpuTrace::CreateCommandLineResource()
{
    RC rc;
    LWGpuResource * pGpu = m_Test->GetGpuResource();
    shared_ptr<VaSpace> pVaSpace;
    if (params->ParamPresent("-address_space_name"))
    {
        string vasName = params->ParamStr("-address_space_name");
        LWGpuResource::VaSpaceManager * pVaSpaceMgr = pGpu->GetVaSpaceManager(m_Test->GetLwRmPtr());
        pVaSpaceMgr->CreateResourceObject(LWGpuResource::TEST_ID_GLOBAL, vasName);
        // add default vaspace
        pVaSpace = pVaSpaceMgr->CreateResourceObjectAlias(m_Test->GetTestId(), "default",
                                       LWGpuResource::TEST_ID_GLOBAL, vasName);
    }
    else
    {
        // add default vaspace
        LWGpuResource::VaSpaceManager * pVaSpaceMgr = pGpu->GetVaSpaceManager(m_Test->GetLwRmPtr());
        pVaSpace = pVaSpaceMgr->CreateResourceObject(m_Test->GetTestId(), "default");
    }
    m_Test->CreateChunkMemory(pVaSpace->GetHandle());

    if (params->ParamPresent("-subcontext_name"))
    {
        string subctxName = params->ParamStr("-subcontext_name");
        LWGpuResource::SubCtxManager * pSubCtxMgr = pGpu->GetSubCtxManager(m_Test->GetLwRmPtr());
        shared_ptr<SubCtx> pSubCtx = pSubCtxMgr->CreateResourceObject(LWGpuResource::TEST_ID_GLOBAL, subctxName);
        if(params->ParamPresent("-subcontext_veid"))
        {
            UINT32 veid = params->ParamUnsigned("-subcontext_veid");
            CHECK_RC(pSubCtx->SetSpecifiedVeid(veid));
        }
    }

    return rc;
}

TraceChannel* GpuTrace::GetLastChannelByVASpace(LwRm::Handle hVASpace)
{
    TraceChannelObjects::reverse_iterator rIt;
    for (rIt = m_TraceChannelObjects.rbegin();
            rIt != m_TraceChannelObjects.rend();
            rIt ++)
    {
        if (rIt->GetVASpaceHandle() == hVASpace)
        {
            return &(*rIt);
        }
    }

    return 0;
}

RC GpuTrace::ConfigureSubCtxPETable(shared_ptr<SubCtx> pSubCtx)
{
    RC rc;
    UINT32 maxTpcCount;
    UINT32 maxSingletonTpcGpcCount;
    UINT32 maxPluralTpcGpcCount;
    vector<UINT32> tpcMasks;
    PEConfigureFile::PARTITION_MODE partitionMode;

    PEConfigureFile * pConfigure = m_Test->GetPEConfigureFile();

    CHECK_RC(pConfigure->GetTpcInfoPerSubCtx(pSubCtx, &tpcMasks, &maxTpcCount, 
        &maxSingletonTpcGpcCount, &maxPluralTpcGpcCount, &partitionMode));

    // since the PE configure file is per test.
    // NOTE: Need set the partition mode first since when configure tpc mask should know which mode it is
    TraceTsg * pTraceTsg = pSubCtx->GetTraceTsg();
    pTraceTsg->SetPartitionMode(partitionMode);
    CHECK_RC(pSubCtx->SetTpcMask(tpcMasks));
    pSubCtx->SetMaxTpcCount(maxTpcCount);
    pSubCtx->SetMaxSingletonTpcGpcCount(maxSingletonTpcGpcCount);
    pSubCtx->SetMaxPluralTpcGpcCount(maxPluralTpcGpcCount);

    return rc;
}

void GpuTrace::IncrementTraceChannelGpEntries(TraceChannel* pTraceChannel)
{
    if (m_NumGpEntries.find(pTraceChannel) != m_NumGpEntries.end())
        m_NumGpEntries[pTraceChannel]++;
    else
        m_NumGpEntries[pTraceChannel] = 1;
}

void GpuTrace::UpdateTraceChannelMaxGpEntries(TraceChannel* pTraceChannel)
{
    if (m_NumGpEntries.find(pTraceChannel) != m_NumGpEntries.end())
    {
        if( m_NumGpEntries[pTraceChannel] > pTraceChannel->GetMaxCountedGpFifoEntries())
        {
            pTraceChannel->SetMaxCountedGpFifoEntries(m_NumGpEntries[pTraceChannel]);
        }
        m_NumGpEntries[pTraceChannel] = 0;
    }
}

void GpuTrace::UpdateAllTraceChannelMaxGpEntries()
{
    std::map<TraceChannel*, UINT32 >::iterator it;

    for (it = m_NumGpEntries.begin(); it != m_NumGpEntries.end(); it++)
    {
        UpdateTraceChannelMaxGpEntries(it->first);
    }
}

RC GpuTrace::ConfigureWatermark()
{
    RC rc;

    LWGpuResource * pGpu = m_Test->GetGpuResource();
    LWGpuResource::SubCtxManager * pSubCtxMgr = pGpu->GetSubCtxManager(m_Test->GetLwRmPtr());

    if(params->ParamPresent("-subctx_cwdslot_watermark") ||
        params->ParamPresent("-subctx_veid_cwdslot_watermark"))
    {
        m_pWatermarkImpl.reset(new WaterMarkImpl(params, m_Test));
        m_pWatermarkImpl->ParseWaterMark();

        if(params->ParamPresent("-subcontext_name"))
        {
            string name = params->ParamStr("-subcontext_name");
            shared_ptr<SubCtx> pSubCtx = pSubCtxMgr->GetResourceObject(LWGpuResource::TEST_ID_GLOBAL, name);
            // for the test which has the global subctx, the subctxId should only have one
            CHECK_RC(SetWatermark(pSubCtx));
        }
        else
        {
            for(Trace3DResourceContainer<SubCtx>::iterator it = pSubCtxMgr->begin();
                    it != pSubCtxMgr->end(); ++it)
            {
                if(it->second.get() && it->first == m_Test->GetTestId())
                {
                    CHECK_RC(SetWatermark(it->second));
                }
            }
        }
    }

    return rc;
}

RC GpuTrace::SetWatermark(shared_ptr<SubCtx> pSubCtx)
{
    RC rc;

    if(!pSubCtx.get())
    {
        ErrPrintf("%s: Can't found invalid subctx.\n", __FUNCTION__);
        return RC::ILWALID_FILE_FORMAT;
    }

    UINT32 watermark;
    CHECK_RC(GetWatermarkImpl()->GetWaterMark(pSubCtx, &watermark));
    pSubCtx->SetWatermark(watermark);

    return rc;
}

RC GpuTrace::AddTraceOpDependency(const string & traceOpName, const string & dependentTraceOpName)
{
    RC rc;

    UINT32 traceOpId, dependentTraceOpId;

    traceOpId = GetUserDefTraceOpId(traceOpName);
    if (traceOpId == 0)
    {
        ErrPrintf("%s: unknown trace name %s.\n",__FUNCTION__, traceOpName.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }

    dependentTraceOpId = GetUserDefTraceOpId(dependentTraceOpName);
    if (dependentTraceOpId == 0)
    {
        ErrPrintf("%s: unknown trace name %s.\n",__FUNCTION__, dependentTraceOpName.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }

    TraceOp* pTraceOp = GetTraceOp(traceOpId);
    if (pTraceOp != nullptr)
    {
        pTraceOp->AddTraceOpDependency(dependentTraceOpId);
    }

    return rc;
}

void GpuTrace::InitTraceOpsStatus()
{
    for (TraceOps::iterator iOp = m_TraceOps.begin(); iOp != m_TraceOps.end(); ++iOp)
    {
        iOp->second->SetTraceOpStatus(TraceOp::TRACEOP_STATUS_DEFAULT);
    }
}

void GpuTrace::ForEachTraceOps(UINT32 beginTraceOpIdx, UINT32 endTraceOpIdx, std::function<void(TraceOp *)> func)
{
    UINT32 counter = 0;
    for (TraceOps::iterator iOp = m_TraceOps.begin(); iOp != m_TraceOps.end(); ++iOp)
    {
        if(counter >= beginTraceOpIdx && counter <= endTraceOpIdx)
        {
            func(iOp->second);
        }
        else if(counter > endTraceOpIdx)
        {
            break;
        }
        counter++;
    }
}

TraceOp* GpuTrace::GetTraceOp(UINT32 traceOpId)
{
    MASSERT(m_Version >= 3);

    // Warning: m_TraceOps is a multimap. Some traceOps share same line number as traceOpId.
    // This function always return the first traceop with the target traceOpId.
    TraceOps::const_iterator it = m_TraceOps.find(traceOpId);
    return it == m_TraceOps.end() ? nullptr : it->second;
}

RC GpuTrace::AddInternalSyncEvent()
{
    TraceOp* syncOnOp = nullptr;
    TraceOp* syncOffOp = nullptr;

    if (m_HaveSyncEventOp)
    {
        return OK;
    }

    TraceOps::iterator lastMethodOp;
    for (auto iOp = m_TraceOps.begin(); iOp != m_TraceOps.end(); ++iOp)
    {
        if (iOp->second->HasMethods())
        {
            if (!syncOnOp)
            {
                // In multimap, the order of the key-value pairs whose keys compare equivalent is the order of insertion
                auto methodOp = *iOp;
                m_TraceOps.erase(iOp);

                syncOnOp = new TraceOpEventSyncStart(m_Test, "internal_sync_on");
                syncOnOp->InitTraceOpDependency(this);
                m_TraceOps.insert({methodOp.first, syncOnOp});
                iOp = m_TraceOps.insert(methodOp);
                m_TraceOpLineNos.insert({syncOnOp, methodOp.first});
            }
            lastMethodOp = iOp;
        }

        if (syncOnOp && iOp->second->WillPolling())
        {
            lastMethodOp = --iOp;
            break;
        }
    }

    if (syncOnOp)
    {
        syncOffOp = new TraceOpEventSyncStop(m_Test, "internal_sync_off");
        syncOffOp->InitTraceOpDependency(this);
        m_TraceOps.insert({lastMethodOp->first, syncOffOp});
        m_TraceOpLineNos.insert({syncOffOp, lastMethodOp->first});
    }
    else
    {
        InfoPrintf(MSGID(TraceParser), "%s: This is a trace without methods!\n", __FUNCTION__);
    }
    return OK;
}

