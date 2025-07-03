/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "utils.h"
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <vector>
#include "core/include/massert.h"
#include "core/include/rc.h"
#include "core/include/utility.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/scopereg.h"
#include "mdiag/sysspec.h"

#include "fermi/gf100/dev_bus.h"
#include "fermi/gf100/dev_ram.h"
#include "core/include/fileholder.h"
#include "core/include/platform.h"
#include "core/include/refparse.h"
#include "mdiag/vmiopmods/vmiopmdiagelwmgr.h"
#include "mdiag/vmiopmods/vmiopmdiagelw.h"
#include "mdiag/utils/buffinfo.h"
#include "mdiag/utils/mdiagsurf.h"

string UtilStrTranslate(const string& str, char c1, char c2)
{
    // Replace all oclwrences
    string result = str;
    size_t pos = result.find(c1);
    while (pos != string::npos)
    {
        result[pos] = c2;
        pos = result.find(c1, pos+1);
    }

    // Replace duplicates, except leading //
    const char dup[] = { c2, c2, 0 };
    pos = result.find(dup, 1);
    while (pos != string::npos)
    {
        result.erase(pos, 1);
        pos = result.find(dup, pos);
    }

    return result;
}

int P4Action(const char *action, const char *filename)
{
    const char *absfilename;

    // colwert the filename into an absolute path filename
#if defined(_WIN32)
    char buf2[512];
    absfilename = _fullpath(buf2,filename,sizeof(buf2));
#else
    absfilename = filename;
#endif
    if (absfilename==NULL)
        return errno;

    char buf[512];
    sprintf(buf, "p4 %s %s", action, absfilename);

    return system(buf);
}

// clean_filename_slashes -- clean up an input filename string
// get rid of extra / and \ characters, next to other / and \ characters,
// except as first character in string (for \\)

// this puts it in a canonical form, so it can be used for wildcard matches, for example.
void clean_filename_slashes(char *out, const char *in)
{
    if (in == NULL) {
        out[0] = '\0';
        return;
    }
    int iout = 0;
    for (int iin = 0; in[iin] != '\0'; iin++) {
        if (iin >= 1) {
            MASSERT(iout >= 1);        // this should never happen, so its a safe assert in the code
            if (out[iout-1] == '/' || out[iout-1] == '\\') { // last char output was / or \.
                if (in[iin] == out[iout-1]) {// next character to copy is same as last output
                    if (iin > 1) { // dont continue if iin == 1, so we allow leading \\ or //
                        continue; // then dont copy this one, skip it
                    }
                }
            }
        }
        out[iout++] = in[iin]; // only increment output if there is actually a copy
    }
    out[iout++] = '\0'; // end the output string
}

void extract_sub_and_match_list(char *sub, char *match_list, const char *prefix)
{
    // prefix looks like L:\perforce\arch\traces(O:\traces,P:\)
    // so match should be O:\traces,P:\ and sub should be L:\perforce\arch\traces
    strcpy(sub, prefix);
    char *last = strchr(sub, '(');
    if (last == NULL) {
        sub[0] = '\0';
        match_list[0] = '\0';
        return;
    }
    *last = '\0';
    last = strchr(const_cast<char *>(prefix), '(');
    if (last == NULL) {
        sub[0] = '\0';
        match_list[0] = '\0';
        return;
    }
    strcpy(match_list, last+1); // skip the '(' characer
    last = strchr(match_list, ')');
    if (last == NULL) {
        sub[0] = '\0';
        match_list[0] = '\0';
        return;
    }
    if (!(last == match_list + strlen(match_list) - 1)) { // make sure it was in the right place
        sub[0] = '\0';
        match_list[0] = '\0';
        return;
    }
    if (last[1] != '\0') {
        sub[0] = '\0';
        match_list[0] = '\0';
        return;
    }
    last[0] = '\0'; // erase the ')' character
}

char *                          // returns start of next match string or pointer to '\0'
extract_next_match_string(char *match, char *match_list)
{
    // match_list looks like O:\traces,P:\.
    char *last = strchr(match_list, ',');
    if (last == NULL) {
        last = strchr(match_list, '\0');
    }
    MASSERT(last != NULL);         // this must be true or string had no END!
    int count = last - match_list;
    strncpy(match, match_list, count);
    match[count] = '\0';  // need to cap off the copy we just made, since it does not include the \0
    match_list += count;
    MASSERT(match_list[0] == ',' || match_list[0] == '\0');
    // this must be true ot previous search found wrong spot, so assert should be safe
    if (match_list[0] == ',') {
        match_list++;
    }
    return match_list;
}

//
// This is a function for debugging
RC PraminPhysRd
(
    UINT64 physAddress,
    Memory::Location location,
    UINT32 size,
    string * pString
)
{
    RC rc;

    if (!size)
    {
        return true;
    }

    std::vector<UINT08> dataReadback;
    CHECK_RC(MDiagUtils::PraminPhysRdRaw(physAddress, location, size, &dataReadback));

    string strData;
    UINT32 *pData = reinterpret_cast<UINT32*>(&(dataReadback[0]));
    for(size_t i = 0; i < dataReadback.size() / 4; ++ i)
    {
        if(i%8 == 0) strData += Utility::StrPrintf("\n");
        strData += Utility::StrPrintf("0x%08x ", pData[i]);
    }

    if (dataReadback.size() % 4 != 0)
    {
        size_t baseIndex = dataReadback.size() / 4 * 4;

        for (size_t i = 0; i < dataReadback.size() % 4; ++ i)
        {
            strData += Utility::StrPrintf("0x%02x", pData[baseIndex + i + 1]);
        }
    }

    if(pString)
        *pString = strData;
    else
        RawPrintf("%s", strData.c_str());

    return rc;
}

namespace MDiagUtils
{
    void SetMemoryTag::MemoryTag(GpuDevice* pGpuDevice, const char *ss)
    {
        if (Platform::GetSimulationMode() == Platform::Fmodel)
        {
            string str("MemoryTag");
            if (ss)
            {
                str += ss;
                Platform::EscapeWrite(str.c_str(), 0, 4, 0);
            }
            Platform::EscapeWrite("MemoryTag", 0, 4, (unsigned int)(uintptr_t)ss);
        }
        else if ((Platform::GetSimulationMode() == Platform::Amodel) && (ss != 0))
        {
            MASSERT(pGpuDevice != nullptr);
            pGpuDevice->EscapeWriteBuffer("MemoryTag", 0, strlen(ss), (const void *)ss);
            Printf(Tee::PriDebug, "Amodel escapewrites: %s\n", ss);
        }
    }

    RawMemory::~RawMemory()
    {
        Unmap();
    }
    
    void RawMemory::SetLocation(Memory::Location location)
    {
        if (!IsMapped())
            m_Location = location;
        else
            MASSERT(!"Cannot set location as it is already mapped.");
    }
    
    void RawMemory::SetSize(UINT64 size)
    {
        if (!IsMapped())
            m_Size = size;
        else
            MASSERT(!"Cannot set size as it is already mapped.");
    }
    
    void RawMemory::SetPhysAddress(PHYSADDR physAddress)
    {
        if (!IsMapped())
            m_PhysAddress = physAddress;
        else
            MASSERT(!"Cannot set physical address as it is already mapped.");
    }

    RC RawMemory::Map()
    {
        RC rc = OK;
        DebugPrintf("Mapping segment at physical addr=0x%llx\n", m_PhysAddress);

        if (m_Address != nullptr)
            return rc;

        if (m_Location == Memory::Fb)
        {
            LW0080_CTRL_FB_CREATE_FB_SEGMENT_PARAMS createFbSegParams = {};

            UINT64 pageSize = 0x1000;  // 4k pagesize specified in flags
            MASSERT((pageSize & (pageSize - 1)) == 0);
            UINT64 offsetMask = pageSize - 1;
            createFbSegParams.VidOffset       = m_PhysAddress & ~offsetMask;
            createFbSegParams.ValidLength     = ALIGN_UP(m_PhysAddress + m_Size, pageSize)
                - ALIGN_DOWN(m_PhysAddress, pageSize);
            createFbSegParams.Length          = createFbSegParams.ValidLength;
            createFbSegParams.AllocHintHandle = 0;
            createFbSegParams.hClient         = m_pLwRm->GetClientHandle();
            createFbSegParams.hDevice         = m_pLwRm->GetDeviceHandle(m_pGpuDev);
            createFbSegParams.Flags           =
                DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _FIXED_OFFSET, _NO) |
                DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _MAP_CPUVA, _YES) |
                DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _CONTIGUOUS, _TRUE) |
                DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _PAGE_SIZE, _4K);
            createFbSegParams.subDeviceIDMask = 1; // Just enable subdevice 0

            CHECK_RC(m_pLwRm->CreateFbSegment(m_pGpuDev, &createFbSegParams));

            m_MappedHandle = createFbSegParams.hMemory;
            m_Address = (UINT08 *)createFbSegParams.ppCpuAddress[0] +
                            (m_PhysAddress & offsetMask);
        }
        else
        {
            UINT64 pageSize = static_cast<UINT64>(Platform::GetPageSize());
            MASSERT((pageSize & (pageSize - 1)) == 0);
            UINT64 offsetMask = pageSize - 1;

            PHYSADDR physAddr = m_PhysAddress & ~offsetMask;
            UINT64 size = ALIGN_UP(physAddr + m_Size, pageSize)
                    - ALIGN_DOWN(physAddr, pageSize);

            void* pMem = 0;
            CHECK_RC(Platform::MapDeviceMemory(&pMem, physAddr,
                    static_cast<size_t>(size), Memory::UC, Memory::ReadWrite));
            m_Address = static_cast<UINT08*>(pMem) + (m_PhysAddress & offsetMask);
        }
        DebugPrintf("Segment mapped to virtual addr=0x%llx\n", m_Address);

        return rc;

    }

    RC RawMemory::Unmap()
    {
        RC rc = OK;

        if (nullptr == m_Address)
            return rc;

        if (m_Location == Memory::Fb)
        {
            MASSERT(0 != m_MappedHandle);

            LW0080_CTRL_FB_DESTROY_FB_SEGMENT_PARAMS destroyFbSegParams = {};
            destroyFbSegParams.hMemory = m_MappedHandle;
            CHECK_RC(m_pLwRm->DestroyFbSegment(m_pGpuDev, &destroyFbSegParams));

            m_MappedHandle = 0;
        }
        else
        {
            MASSERT((Memory::Coherent == m_Location) || (Memory::NonCoherent == m_Location));

            Platform::UnMapDeviceMemory(m_Address);
        }

        m_Address = nullptr;

        return rc;

    }

    //--------------------------------------------------------------------
    //! \brief Allocate the channel's error notifier, pushbuffer, and gpfifo.
    //!
    RC AllocateChannelBuffers
    (
        GpuDevice *pGpuDevice,
        MdiagSurf *pErrorNotifier,
        MdiagSurf *pPushBuffer,
        MdiagSurf *pGpFifo,
        const char *channelName,
        LwRm::Handle hVaSpace,
        LwRm* pLwRm
    )
    {
        RC rc;
        BuffInfo buffInfo;
    
        pErrorNotifier->SetArrayPitch(16);
        pErrorNotifier->SetColorFormat(ColorUtils::Y8);
        pErrorNotifier->SetForceSizeAlloc(true);

        if (pErrorNotifier->GetLocation() == Memory::Optimal)
        {
            if (Platform::GetSimulationMode() == Platform::Amodel)
            {
                pErrorNotifier->SetLocation(Memory::Fb);
            }
            else
            {
                pErrorNotifier->SetLocation(Memory::Coherent);
            }
        }
        pErrorNotifier->SetAddressModel(Memory::Segmentation);
        pErrorNotifier->SetKernelMapping(true);
        pErrorNotifier->SetGpuCacheMode(Surface2D::GpuCacheOff);
        pErrorNotifier->SetP2PGpuCacheMode(Surface2D::GpuCacheOff);
        CHECK_RC(pErrorNotifier->Alloc(pGpuDevice, pLwRm));
        CHECK_RC(pErrorNotifier->Map(0, pLwRm));
    
        pPushBuffer->SetArrayPitch(1024*1024);
        pPushBuffer->SetColorFormat(ColorUtils::Y8);
        pPushBuffer->SetForceSizeAlloc(true);
        if (pPushBuffer->GetLocation() == Memory::Optimal)
        {
            if (Platform::GetSimulationMode() == Platform::Amodel)
            {
                pPushBuffer->SetLocation(Memory::Fb);
            }
            else
            {
                pPushBuffer->SetLocation(Memory::NonCoherent);
            }
        }
        pPushBuffer->SetProtect(Memory::Readable);
        pPushBuffer->SetGpuCacheMode(Surface2D::GpuCacheOff);
        pPushBuffer->SetP2PGpuCacheMode(Surface2D::GpuCacheOff);
        pPushBuffer->SetGpuVASpace(hVaSpace);
        CHECK_RC(pPushBuffer->Alloc(pGpuDevice, pLwRm));
        CHECK_RC(pPushBuffer->Map(0, pLwRm));
    
        pGpFifo->SetArrayPitch(4096);
        pGpFifo->SetColorFormat(ColorUtils::Y8);
        pGpFifo->SetForceSizeAlloc(true);
        if (pGpFifo->GetLocation() == Memory::Optimal)
        {
            if (Platform::GetSimulationMode() == Platform::Amodel)
            {
                pGpFifo->SetLocation(Memory::Fb);
            }
            else
            {
                pGpFifo->SetLocation(Memory::NonCoherent);
            }
        }
        pGpFifo->SetProtect(Memory::Readable);
        pGpFifo->SetGpuCacheMode(Surface2D::GpuCacheOff);
        pGpFifo->SetP2PGpuCacheMode(Surface2D::GpuCacheOff);
        pGpFifo->SetGpuVASpace(hVaSpace);
        CHECK_RC(pGpFifo->Alloc(pGpuDevice, pLwRm));
        CHECK_RC(pGpFifo->Map(0, pLwRm));

        buffInfo.SetDmaBuff(string("PushBuff"), *pPushBuffer);
        buffInfo.SetDmaBuff(string("GpFifo"), *pGpFifo);
        buffInfo.SetDmaBuff(string("Err"), *pErrorNotifier);
        buffInfo.Print(channelName, pGpuDevice); 
        return OK;
    }

    UINT32 GetRmNotifierByEngineType(UINT32 engineType, GpuSubdevice *pGpuSubdevice, LwRm* pLwRm)
    {
        // Single instance engines to notifier map
        static const map<UINT32, UINT32> engNotifierMap = 
        {
            {LW2080_ENGINE_TYPE_VP,     LW2080_NOTIFIERS_PDEC},
            {LW2080_ENGINE_TYPE_SEC2,   LW2080_NOTIFIERS_SEC2},
            {LW2080_ENGINE_TYPE_OFA,    LW2080_NOTIFIERS_OFA}
        };

        pLwRm = pLwRm ? pLwRm : LwRmPtr().Get();

        UINT32 notifier = LW2080_NOTIFIERS_MAXCOUNT;

        // Check for all the multiple instances engines first
        if (IsGraphicsEngine(engineType))
        {
            notifier = LW2080_NOTIFIERS_GR(GetGrEngineOffset(engineType));
        }
        else if (IsLwDecEngine(engineType))
        {
            notifier = LW2080_NOTIFIERS_LWDEC(GetLwDecEngineOffset(engineType));
        }
        else if (IsLWJPGEngine(engineType))
        {
            notifier = LW2080_NOTIFIERS_LWJPEG(GetLwJpgEngineOffset(engineType));
        }
        else if (IsCopyEngine(engineType))
        {
            MASSERT(pGpuSubdevice);
            UINT08 capsArray[LW2080_CTRL_CE_CAPS_TBL_SIZE];
            memset(capsArray, 0, sizeof(capsArray));

            LW2080_CTRL_CE_GET_CAPS_PARAMS capsParams = {};
            capsParams.ceEngineType = engineType;
            capsParams.capsTblSize  = LW2080_CTRL_CE_CAPS_TBL_SIZE;
            capsParams.capsTbl      = (LwP64)capsArray;
            if (OK != pLwRm->ControlBySubdevice(pGpuSubdevice,
                                                    LW2080_CTRL_CMD_CE_GET_CAPS,
                                                    &capsParams, sizeof(capsParams)))
            {
                MASSERT(!"Calling LW2080_CTRL_CMD_CE_GET_CAPS failed");
                return LW2080_NOTIFIERS_MAXCOUNT;
            }

            if (LW2080_CTRL_CE_GET_CAP(capsArray, LW2080_CTRL_CE_CAPS_CE_GRCE))
            {
                notifier = LW2080_NOTIFIERS_GRAPHICS; // GRCE uses GR notifier
            }
            else
            {
                notifier = LW2080_NOTIFIERS_CE(GetCopyEngineOffset(engineType));
            }
        }
        else if (IsLwEncEngine(engineType))
        {
            notifier = LW2080_NOTIFIERS_LWENC(GetLwEncEngineOffset(engineType));
        }
        else // Single instance engines
        {
            auto engNotifier = engNotifierMap.find(engineType);
            if (engNotifier != engNotifierMap.end())
            {
                notifier = engNotifier->second;
            }
            else
            {
                MASSERT(!"Unknown engine type to colwert to notifiers");
            }
        }

        return notifier;
    }

    // Ilwoke RM to get Instance memory info for a given channel handle
    LW2080_CTRL_FIFO_MEM_INFO GetInstProperties(GpuSubdevice *pSubGpuDevice, UINT32 hChannel, LwRm* pLwRm)
    {
        LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_INFO_PARAMS params = {0};

        params.hChannel = hChannel;
        RC rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(pSubGpuDevice),
                               LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_INFO,
                               &params, sizeof(params));
        if (rc != OK)
        {
            // if this fails (and it can if there's no RM) just return
            ErrPrintf("RM failed to return Instance memory information for channel handle: 0x%x",
                      hChannel);
            MASSERT(!"No Instance memory found!");
        }

        DebugPrintf("Channel 0x%x instance memory aperture 0x%x"
                    " base 0x%llx size 0x%llx\n",
                    hChannel, params.chMemInfo.inst.aperture,
                    params.chMemInfo.inst.base, params.chMemInfo.inst.size);
        return params.chMemInfo.inst;
    }

    RC TagTrepFilePass()
    {
        RC rc;

        FileHolder file;
        DEFER
        {
            // The file should be closed explicitly because FileHolder doesn't end its lifecycle before call
            // the exit function and this will lead to no data in trep.txt.
            file.Close();
        };

        if(OK == file.Open("trep.txt", "a"))
        {
            // Test run tools need to make report according to trep.txt. 
            const char testPassStr[] = "test #0: TEST_SUCCEEDED (gold)\n"
                "summary = all tests passed\n"
                "expected results = 1\n";
            CHECK_RC(Utility::FWrite(testPassStr, sizeof(testPassStr), 1, file.GetFile()));
        }


        return rc;
    }

    bool IsCopyEngine(UINT32 engine)
    {
        return LW2080_ENGINE_TYPE_IS_COPY(engine);
    }

    bool IsLwDecEngine(UINT32 engine)
    {
        return LW2080_ENGINE_TYPE_IS_LWDEC(engine);
    }

    bool IsLwEncEngine(UINT32 engine)
    {
        return LW2080_ENGINE_TYPE_IS_LWENC(engine);
    }

    bool IsOFAEngine(UINT32 engine)
    {
        return (engine == LW2080_ENGINE_TYPE_OFA) ? true : false;
    }
    
    bool IsLWJPGEngine(UINT32 engine)
    {
        return LW2080_ENGINE_TYPE_IS_LWJPEG(engine);
    }

    bool IsSECEngine(UINT32 engine)
    {
        return (engine == LW2080_ENGINE_TYPE_SEC2) ? true : false;
    }

    bool IsSWEngine(UINT32 engine)
    {
        return (engine == LW2080_ENGINE_TYPE_SW);
    }

    bool IsGraphicsEngine(UINT32 engine)
    {
        return LW2080_ENGINE_TYPE_IS_GR(engine);
    }

    UINT32 GetGrEngineOffset(UINT32 engine)
    {
        return LW2080_ENGINE_TYPE_GR_IDX(engine);
    }

    UINT32 GetCopyEngineOffset(UINT32 engine)
    {
        return LW2080_ENGINE_TYPE_COPY_IDX(engine);
    }

    UINT32 GetLwDecEngineOffset(UINT32 engine)
    {
        if (MDiagUtils::IsLwDecEngine(engine))
        {
            return LW2080_ENGINE_TYPE_LWDEC_IDX(engine);
        }
        else
        {
            MASSERT(!"Invalid lwdec engine id\n");
            return 0;
        }
    }

    UINT32 GetLwEncEngineOffset(UINT32 engine)
    {
        if (MDiagUtils::IsLwEncEngine(engine))
        {
            return LW2080_ENGINE_TYPE_LWENC_IDX(engine);
        }
        else
        {
            MASSERT(!"Invalid lwenc engine id\n");
            return 0;
        }
    }

    UINT32 GetLwJpgEngineOffset(UINT32 engine)
    {
        return LW2080_ENGINE_TYPE_LWJPEG_IDX(engine);
    }

    string Eng2Str(UINT32 engine)
    {
        UINT32 offset = 0;
        string engStr;

        if (IsCopyEngine(engine))
        {
            offset = GetCopyEngineOffset(engine);
            engStr = "COPY";
        }
        else if (IsLwDecEngine(engine))
        {
            offset = GetLwDecEngineOffset(engine);
            engStr = "LWDEC";
        }
        else if (IsLWJPGEngine(engine))
        {
            offset = GetLwJpgEngineOffset(engine);
            engStr = "LWJPG";
        }
        else if (IsLwEncEngine(engine))
        {
            offset = GetLwEncEngineOffset(engine);
            engStr = "LWENC";
        }
        else if (IsGraphicsEngine(engine))
        {
            offset = GetGrEngineOffset(engine);
            engStr = "GR";
        }
        else
        {
            // For engines with no offset
            return GpuDevice::GetEngineName(engine);
        }

        return engStr + to_string(offset);
    }

    UINT32 GetEngineIdBase(vector<UINT32> classIds, GpuDevice* pGpuDevice, LwRm* pLwRm, bool isGrCE)
    {
        vector<UINT32> engineList;
        UINT32 engineIdBase = 0;

        // Get all the engines supported by classes
        if (pGpuDevice->GetEnginesFilteredByClass(classIds, &engineList, pLwRm) != OK)
            MASSERT(!"GetEngineIdBase: Failed to get engines from class list\n");

        // if the engine list is empty then there is some problem
        if (engineList.empty())
        {
            MASSERT(!"GetEngineIdBase: engine list is empty\n");
        }
        else if (any_of(begin(engineList), end(engineList), IsGraphicsEngine) ||
                isGrCE)
        {
            engineIdBase = LW2080_ENGINE_TYPE_GR0;
        }
        else if (IsCopyEngine(engineList[0]))
        {
            for (auto& engine : engineList)
            {
                if (!IsCopyEngine(engine))
                {
                    MASSERT(!"GetEngineIdBase: All engines are not COPY engines\n");
                    return LW2080_ENGINE_TYPE_NULL;
                }
            }
            engineIdBase = LW2080_ENGINE_TYPE_COPY0;
        }
        else if (IsLwDecEngine(engineList[0]))
        {
            for (auto& engine : engineList)
            {
                if (!IsLwDecEngine(engine))
                {
                    MASSERT(!"GetEngineIdBase: All engines are not LWDEC engines\n");
                    return LW2080_ENGINE_TYPE_NULL;
                }
            }
            engineIdBase = LW2080_ENGINE_TYPE_LWDEC0;
        }
        else if (IsLWJPGEngine(engineList[0]))
        {
            for (auto& engine : engineList)
            {
                if (!IsLWJPGEngine(engine))
                {
                    MASSERT(!"GetEngineIdBase: All engines are not LWJPG engines\n");
                    return LW2080_ENGINE_TYPE_NULL;
                }
            }
            engineIdBase = LW2080_ENGINE_TYPE_LWJPG;
        }
        else if (IsLwEncEngine(engineList[0]))
        {
            for (auto& engine : engineList)
            {
                if (!IsLwEncEngine(engine))
                {
                    MASSERT(!"GetEngineIdBase: All engines are not LWENC engines\n");
                    return LW2080_ENGINE_TYPE_NULL;
                }
            }
            engineIdBase = LW2080_ENGINE_TYPE_LWENC0;
        }
        else
        {
            // For non-multiple engines type just check that the engine size is 1
            MASSERT(engineList.size() == 1);
            engineIdBase = engineList[0];
        }
        return engineIdBase;
    }

    UINT32 GetGrEngineId(UINT32 engineOffset)
    {
        return LW2080_ENGINE_TYPE_GR(engineOffset);
    }

    UINT32 GetCopyEngineId(UINT32 engineOffset)
    {
        return LW2080_ENGINE_TYPE_COPY(engineOffset);
    }

    UINT32 GetLwDecEngineId(UINT32 engineOffset)
    {
        return LW2080_ENGINE_TYPE_LWDEC(engineOffset);
    }

    UINT32 GetLwEncEngineId(UINT32 engineOffset)
    {
        return LW2080_ENGINE_TYPE_LWENC(engineOffset);
    }

    UINT32 GetLwJpgEngineId(UINT32 engineOffset)
    {
        return LW2080_ENGINE_TYPE_LWJPEG(engineOffset);
    }

    RC VirtFunctionReset(UINT32 bus, UINT32 domain, UINT32 function, UINT32 device)
    {
        RC rc;
        VmiopMdiagElwManager * pVmiopMdiagElwMgr = VmiopMdiagElwManager::Instance();
        GpuSubdevice* pSubdev = pVmiopMdiagElwMgr->GetGpuSubdevInstance();
        UINT32 data = 0;
 
        Platform::FreeIRQs(domain, bus, device, function);

        pSubdev->Regs().SetField(&data, 
            MODS_EP_PCFG_VF_DEVICE_CONTROL_STATUS_INITIATE_FN_LVL_RST, 1);
        CHECK_RC(Platform::PciWrite32(
            domain, bus, device, function, 
            pSubdev->Regs().LookupAddress(MODS_EP_PCFG_VF_DEVICE_CONTROL_STATUS), 
            data));

        return rc;
    }

    void ReadFLRPendingBits(vector<UINT32> * data)
    {
        VmiopMdiagElwManager * pVmiopMdiagElwMgr = VmiopMdiagElwManager::Instance();
        GpuSubdevice* pSubdev = pVmiopMdiagElwMgr->GetGpuSubdevInstance();
        UINT32 flrIntrPendingSize = 
            pSubdev->Regs().LookupArraySize(
                MODS_XTL_EP_PRI_VF_FLR_INTR_PENDING, 1);
        data->resize(flrIntrPendingSize);
        for (UINT32 i = 0; i < data->size(); ++i)
        {
            (*data) [i] = pSubdev->Regs().Read32(
                MODS_XTL_EP_PRI_VF_FLR_INTR_PENDING, i);
        }
    }

    void UpdateFLRPendingBits(UINT32 GFID, vector<UINT32> * data)
    {
        UINT32 index = GFID / 32;
        UINT32 bit = GFID % 32;
        UINT32 value = 1 << bit;
        (*data)[index] = (*data)[index] | (~value);
    }

    void ClearFLRPendingBits(vector<UINT32> data)
    {
        VmiopMdiagElwManager * pVmiopMdiagElwMgr = VmiopMdiagElwManager::Instance();
        GpuSubdevice* pSubdev = pVmiopMdiagElwMgr->GetGpuSubdevInstance();
        for (UINT32 i = 0; i < data.size(); ++i)
        {
            pSubdev->Regs().Write32(
                MODS_XTL_EP_PRI_VF_FLR_INTR_PENDING, i, data[i]);
        }
    }

    RC PollSharedMemoryMessage(set<UINT32> &gfids, const string &message)
    {
        RC rc;

        MASSERT(gfids.size() > 0);
        VmiopMdiagElwManager * pVmiopMdiagElwMgr = VmiopMdiagElwManager::Instance();

        auto pollAllVfMessageDone = [&message, &rc, &pVmiopMdiagElwMgr, &gfids]()->bool
        {
            for (auto gfid_it = gfids.begin(); gfid_it != gfids.end();)
            {
                auto pVmiopMdiagElw = pVmiopMdiagElwMgr->GetVmiopMdiagElw(*gfid_it);
                // give up polling when the corresponding VF is already shutdown
                if (pVmiopMdiagElw == nullptr) 
                {
                    rc = RC::SOFTWARE_ERROR;
                    ErrPrintf("SRIOV: Can't find this VmiopMdiagElw whose GFID = %d\n",
                            *gfid_it);
                    MASSERT(0);
                    return true;
                }

                rc = pVmiopMdiagElw->CheckReceiveMessageReady();
                if (rc == RC::SOFTWARE_ERROR)
                {
                    MASSERT(0);
                    return true;
                }
                else if (rc != OK)
                {
                    rc.Clear();
                    gfid_it++; 
                    continue;
                }

                string data = "";
                rc = pVmiopMdiagElw->ReadProcMessage(data);
                if (rc != OK) // should never happen, give up polling
                {
                    ErrPrintf("SRIOV: Can't read the data correctly\n");
                    MASSERT(0);
                    return true;
                }

                if (message.compare(data) == 0)
                {
                    gfid_it = gfids.erase(gfid_it);
                } 
                else
                {
                    ErrPrintf("SRIOV: the data in shared memory \"%s\" cannot match the expected message \"%s\" \n",
                        data.c_str(), message.c_str());
                    MASSERT(0);
                    return true;
                }
            }

            return (gfids.size() == 0) ? true : false;
        };

        Tasker::PollHw(Tasker::NO_TIMEOUT, pollAllVfMessageDone, __FUNCTION__);
        return rc;
    }

    RC WaitFlrDone(UINT32 gfid)
    {
        RC rc;
        VmiopMdiagElwManager * pVmiopMdiagElwMgr = VmiopMdiagElwManager::Instance();
        DebugPrintf("SRIOV: WaitFlrDone Wait GFID %d.\n", gfid);
        
        Tasker::PollHw(Tasker::NO_TIMEOUT,[&gfid, &pVmiopMdiagElwMgr]()->bool
            {
                UINT32 flrIntrPendingData;
                UINT32 flrPendingData;
                UINT32 index = gfid / 32;
                GpuSubdevice* pSubdev = pVmiopMdiagElwMgr->GetGpuSubdevInstance();
                flrIntrPendingData = pSubdev->Regs().Read32(
                    MODS_XTL_EP_PRI_VF_FLR_INTR_PENDING, index);
                flrPendingData = pSubdev->Regs().Read32(
                    MODS_XTL_EP_PRI_VF_FLR_PENDING, index);
                    
                VmiopMdiagElw * pVmiopMdiagElw = pVmiopMdiagElwMgr->GetVmiopMdiagElw(gfid);

                for (UINT32 i = 0; i < 32; i++)
                {
                    UINT32 flrIntrPendingValue = flrIntrPendingData & 0x1;
                    flrIntrPendingData = flrIntrPendingData >> 1;
                    
                    UINT32 flrPendingValue = flrPendingData & 0x1;
                    flrPendingData = flrPendingData >> 1;
                    
                    auto gfidBit = 
                        (pSubdev->GetParentDevice()->GetFamily() >= GpuDevice::Hopper) ? 
                            (gfid % 32) : ((gfid - 1) % 32);

                    if (i == gfidBit && flrIntrPendingValue == 0 && flrPendingValue == 0)
                    {
                        if (!pVmiopMdiagElw || !pVmiopMdiagElw->IsRemoteProcessRunning())
                        {
                            DebugPrintf("SRIOV: WaitFlrDone, i %d, gfid %d, flrIntrPendingValue 0x%x,  flrPendingValue 0x%x.\n",
                                i, gfid, flrIntrPendingValue, flrPendingValue);
                            return true;
                        }
                    }
                }
                return false;
            },
                __FUNCTION__);
        CHECK_RC(pVmiopMdiagElwMgr->WaitPluginThreadDone(gfid));

        return rc;
    }

    RegHalDomain AdjustDomain(RegHalDomain domain, const string& regSpace)
    {
        const string lwLinkPrefix = "LW_LWLINK_IOCTRL_";
        bool isMultiCast = regSpace.find(lwLinkPrefix) != string::npos &&
                           regSpace.find("_MULTI") != string::npos;

        static map<RegHalDomain, RegHalDomain> multicastDomainMap =
        {
            {RegHalDomain::LWLIPT_LNK, RegHalDomain::LWLIPT_LNK_MULTI},
            {RegHalDomain::LWLDL, RegHalDomain::LWLDL_MULTI},
            {RegHalDomain::LWLTLC, RegHalDomain::LWLTLC_MULTI}
        };

        if (isMultiCast && multicastDomainMap.find(domain) != multicastDomainMap.end())
        {
            return multicastDomainMap[domain];
        }

        return domain;
    }

    RegHalDomain GetDomain(const string& regName, const string& regSpace)
    {
        // Default value of domain is RAW. If domain is not recognoized, RAW is returned
        RegHalDomain domain = RegHalDomain::RAW;

        static vector<pair<string, RegHalDomain>> str2Domain =
        {
            {"LW_RUNLIST", RegHalDomain::RUNLIST},
            {"LW_CHRAM", RegHalDomain::CHRAM},

            {"LW_LWLIPT_COMMON", RegHalDomain::LWLIPT},
            {"LW_LWLIPT", RegHalDomain::LWLIPT_LNK},
            {"LW_LWLDL", RegHalDomain::LWLDL},
            {"LW_LWLPHYCTL", RegHalDomain::LWLDL},
            {"LW_LWLTLC", RegHalDomain::LWLTLC},

            {"LW_IOCTRL", RegHalDomain::IOCTRL},
            {"LW_LWLPLL", RegHalDomain::LWLPLL},
            {"LW_MINION", RegHalDomain::MINION},
            {"LW_CMINION", RegHalDomain::MINION},
            {"LW_CPR", RegHalDomain::LWLCPR},
            {"LW_XTL_EP_PRI", RegHalDomain::XTL},
            {"LW_XPL", RegHalDomain::XPL},
            {"LW_LWLW", RegHalDomain::LWLW}
        };

        for (auto & item : str2Domain)
        {
            if (regName.find(item.first) != string::npos)
            {
                domain = regSpace == "" ? item.second : AdjustDomain(item.second, regSpace);
                break;
            }
        }

        return domain;
    }

    UINT32 GetTailNum(const string& str)
    {
        size_t lastIndex = str.find_last_not_of("0123456789");
        string result = str.substr(lastIndex + 1);
        return atoi(result.c_str());
    }

    UINT32 GetDomainIndex(const string& regSpace)
    {
        // Per runlist register
        const string engPrefix = "LW_ENGINE_";
        static const map<string, UINT32> engMap = {
            {"LW_ENGINE_GRAPHICS", LW2080_ENGINE_TYPE_GRAPHICS},
            {"LW_ENGINE_VP", LW2080_ENGINE_TYPE_VP},
            {"LW_ENGINE_ME", LW2080_ENGINE_TYPE_ME},
            {"LW_ENGINE_BSP", LW2080_ENGINE_TYPE_BSP},
            {"LW_ENGINE_MPEG", LW2080_ENGINE_TYPE_MPEG},
            {"LW_ENGINE_CIPHER", LW2080_ENGINE_TYPE_CIPHER},
            {"LW_ENGINE_TSEC", LW2080_ENGINE_TYPE_TSEC},
            {"LW_ENGINE_VIC", LW2080_ENGINE_TYPE_VIC},
            {"LW_ENGINE_MSENC", LW2080_ENGINE_TYPE_MSENC},
            {"LW_ENGINE_MP", LW2080_ENGINE_TYPE_MP},
            {"LW_ENGINE_SEC2", LW2080_ENGINE_TYPE_SEC2},
            {"LW_ENGINE_HOST", LW2080_ENGINE_TYPE_HOST},
            {"LW_ENGINE_DPU", LW2080_ENGINE_TYPE_DPU},
            {"LW_ENGINE_PMU", LW2080_ENGINE_TYPE_PMU},
            {"LW_ENGINE_FBFLCN", LW2080_ENGINE_TYPE_FBFLCN},
            {"LW_ENGINE_LWJPG", LW2080_ENGINE_TYPE_LWJPG},
            {"LW_ENGINE_OFA", LW2080_ENGINE_TYPE_OFA}
        };

        auto eng = engMap.find(regSpace);
        if (eng != engMap.end())
        {
            return eng->second;
        }

        if (regSpace.find(engPrefix+"GR") != string::npos)
        {
            return GetGrEngineId(GetTailNum(regSpace));
        }

        if (regSpace.find(engPrefix+"COPY") != string::npos)
        {
            return GetCopyEngineId(GetTailNum(regSpace));
        }

        if (regSpace.find(engPrefix+"LWENC") != string::npos)
        {
            return GetLwEncEngineId(GetTailNum(regSpace));
        }
       
        if (regSpace.find(engPrefix+"LWDEC") != string::npos)
        {
            return GetLwDecEngineId(GetTailNum(regSpace));
        }

        if (regSpace.find(engPrefix+"LWJPG") != string::npos)
        {
            int num = GetTailNum(regSpace);
            return GetLwJpgEngineId(num);
        }
        
        if (regSpace.find("LW_MINION") != string::npos)
        {
            return GetTailNum(regSpace);
        }

        // regSpace is made optional
        if (regSpace.empty())
        {
            return 0;
        }
        
        // LW_XTL_PRI resgistres
        const string lwXtlPrefix = "LW_XTL_PRI";
        if (regSpace.find(lwXtlPrefix) != string::npos)
        {
            return 0;
        }
        
        // LW_XPL resgistres
        const string lwXplPrefix = "LW_XPL";
        if (regSpace.find(lwXplPrefix) != string::npos)
        {
            return 0;        
        }
        
        // LWLINK register
        const string lwLinkPrefix = "LW_LWLINK_IOCTRL_";
        size_t start = regSpace.find(lwLinkPrefix);

        if (start == string::npos)
        {
            ErrPrintf("Invalid register space! regSpace should be LW_ENGINE_<type> or LW_MINION_<index> or LW_LWLINK_IOCTRL_<x>[_LINK_<y>|_MULTI]\n");
            MASSERT(0);
        }

        size_t end = start + lwLinkPrefix.size();
        size_t nextDelim = regSpace.find("_", end);
        string devNum = nextDelim == string::npos ? regSpace.substr(end) : regSpace.substr(end, nextDelim - end);
        UINT32 x = atoi(devNum.c_str());

        UINT32 domainIndex = x;

        // Has y
        if (regSpace.find("_LINK_") != string::npos)
        {
            UINT32 y = GetTailNum(regSpace);
            UINT32 linksPerIoctl = 0;
            GpuDevMgr* pDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
            GpuSubdevice* pSubdev = pDevMgr->GetFirstGpu();
            RegHal& regs = pSubdev->Regs();
            if (regs.IsSupported(MODS_LWL_LWLIPT_NUM_LINKS))
            {
                linksPerIoctl = regs.LookupAddress(MODS_LWL_LWLIPT_NUM_LINKS);
            }
            domainIndex = linksPerIoctl * x + y;
        }

        return domainIndex;
    }

    UINT32 GetDomainBase(const char* regName, const char* regSpace, GpuSubdevice* pSubdev, LwRm* pLwRm)
    {
        const RegHal &regs = pSubdev->Regs();
        
        if (Platform::IsPhysFunMode() || Platform::IsDefaultMode())
        {
            // adjust register offset for registers starting with LW_VIRTUAL_FUNCTION
            if (strstr(regName, "LW_VIRTUAL_FUNCTION"))
            {
                const RefManual *pRefManual = pSubdev->GetRefManual();
                if (!pRefManual)
                    MASSERT(!"ref manual is not found!");
                const RefManualRegisterGroup *pVirtualOffset =
                    pRefManual->FindGroup("LW_VIRTUAL_FUNCTION_FULL_PHYS_OFFSET");
                MASSERT(pVirtualOffset);
                return pVirtualOffset->GetRangeLo();
            }
        }
        
        if (regSpace == nullptr)
        {
            regSpace = "";
        }
        
        // If regspace is LW_PGRAPH_PRI_GPC*, this function callwlates and returns the offset 
        // to the register rather than returning the register base address. 
        
        const char *regSpaceGpcTpc = strstr (regSpace, s_LWPgraphGpcPrefix.c_str());
        if (regSpaceGpcTpc)
        {
            UINT32 gpcIndex = 0;
            UINT32 tpcIndex = 0;
            UINT32 gpcStride = regs.LookupAddress(MODS_GPC_PRI_STRIDE);
            UINT32 tpcStride = regs.LookupAddress(MODS_TPC_IN_GPC_STRIDE);
            
            const string gpcStr = "GPC";
            const string tpcStr = "TPC";
            const char *gpcSearchPtr = nullptr;
            gpcSearchPtr = strstr (regSpace, gpcStr.c_str());
            const char *tpcSearchPtr = nullptr;
            tpcSearchPtr = strstr (regSpace, tpcStr.c_str());
            
            if (gpcSearchPtr)
            {
                gpcIndex = std::atoi(gpcSearchPtr + gpcStr.length());
                if (gpcIndex >= pSubdev->GetMaxGpcCount())
                {
                    ErrPrintf("GPC index used:%d is greater than max number of GPCs %d for regSpace %s.\n", gpcIndex, pSubdev->GetMaxGpcCount(), regSpace);
                    MASSERT(0);
                }
                if (!isdigit(*(gpcSearchPtr + gpcStr.length())))
                {
                    ErrPrintf("GPC index is not mentioned in regSpace %s !\n", regSpace);
                    MASSERT(0);
                }
            }
            if (tpcSearchPtr)
            {
                tpcIndex = std::atoi(tpcSearchPtr + tpcStr.length());
                if (tpcIndex >= pSubdev->GetMaxTpcCount())
                {
                    ErrPrintf("TPC index used:%d is greater than max number of TPCs %d for regSpace %s.\n", tpcIndex, pSubdev->GetMaxTpcCount(), regSpace);
                    MASSERT(0);
                }
                if (!isdigit(*(tpcSearchPtr + tpcStr.length())))
                {
                    ErrPrintf("TPC index is not mentioned in regSpace %s !\n", regSpace);
                    MASSERT(0);
                }
            }

            return (gpcIndex * gpcStride + tpcIndex * tpcStride);
        }

        UINT32 domainIndex = GetDomainIndex(regSpace);
        RegHalDomain regDomain = GetDomain(regName, regSpace);
        UINT32 domainBaseAddressOffset = pSubdev->GetDomainBase(domainIndex, regDomain, pLwRm);
        
        DebugPrintf("Register information:\n"
                    "    regName: %s\n"
                    "    regSpace: %s\n"
                    "    regDomian: %s\n"
                    "    regDomainIndex: %d\n"
                    "    regBaseAddressOffset: 0x%x\n",
                    regName, regSpace, RegHalTable::ColwertToString(regDomain).c_str(), domainIndex, domainBaseAddressOffset);

        return (domainBaseAddressOffset);
    }
    string GetTestNameFromTraceFile(const string& traceFile)
    {
        string::size_type LastDiv = traceFile.find_last_of("/\\");
        string::size_type PrevDiv;
        string ret;

        if ((string::npos == LastDiv) ||
            (0 == LastDiv) ||
            (string::npos == (PrevDiv = traceFile.find_last_of("/\\", LastDiv - 1))) ||
            ((PrevDiv + 1) == LastDiv))
        {
            InfoPrintf("Unable to extract test name from input file, using default name\n");
        }
        else
        {
            ret = traceFile.substr(PrevDiv + 1, LastDiv - PrevDiv - 1);
        }

        return ret;
    }

    bool IsChipTu10x()
    {
        GpuDevMgr* pDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
        GpuDevice* pGpuDevice = pDevMgr->GetFirstGpuDevice();

        switch (pGpuDevice->DeviceId())
        {
#if LWCFG(GLOBAL_GPU_IMPL_TU102)
            case Gpu::TU102:
#endif
#if LWCFG(GLOBAL_GPU_IMPL_TU104)
            case Gpu::TU104:
#endif
#if LWCFG(GLOBAL_GPU_IMPL_TU106)
            case Gpu::TU106:
#endif
                return true;
            default:
                break;
        }
        return false;
    }

    bool IsChipTu11x()
    {
        GpuDevMgr* pDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
        GpuDevice* pGpuDevice = pDevMgr->GetFirstGpuDevice();

        switch (pGpuDevice->DeviceId())
        {
            case Gpu::TU116:
            case Gpu::TU117:
                return true;
            default:
                break;
        }
        return false;
    }

    RC ResetEngine(UINT32 hChannel, UINT32 engineId, LwRm * pLwRm, GpuDevice * pGpuDevice)
    {
        RC rc;

        MASSERT(pLwRm);
        
        LW0080_CTRL_FIFO_SET_CHANNEL_PROPERTIES_PARAMS Params = {};
        Params.hChannel = hChannel;
        Params.property =
            LW0080_CTRL_FIFO_SET_CHANNEL_PROPERTIES_RESETENGINECONTEXT_NOPREEMPT;
        Params.value = engineId;

        CHECK_RC(pLwRm->ControlByDevice(
                    pGpuDevice,
                    LW0080_CTRL_CMD_FIFO_SET_CHANNEL_PROPERTIES,
                    &Params, sizeof(Params)));

        return rc;
    }

    UINT32 GetPhysChannelId(UINT32 vChannelId, LwRm * pLwRm, UINT32 engineId)
    {
        if (Platform::IsVirtFunMode())
        {
            RC rc; 
            MASSERT(pLwRm);
            GpuDevMgr* pDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
            GpuSubdevice* pSubdev = pDevMgr->GetFirstGpu();

            LW2080_CTRL_CMD_FIFO_GET_SCHID_PARAMS params = {};
            params.vChid = vChannelId;
            params.engineType = engineId;
            rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(pSubdev),
                                LW2080_CTRL_CMD_FIFO_GET_SCHID,
                                &params, sizeof(params));

            MASSERT((OK == rc) && "Fail to get physical channel id.\n");

            return params.sChid;
        }
        else
        {
            WarnPrintf("GetPhysChannelId: In non vf mode, physical channel id and virtual channel id should be same.\n");
            return vChannelId;
        }
    }

    RC PraminPhysRdRaw
    (
        UINT64 physAddress,
        Memory::Location location,
        UINT64 size,
        vector<UINT08>* pOut
    )
    {
        RC rc;

        MASSERT(pOut != nullptr);
        MASSERT(size > 0);

        GpuDevice *gpudev = nullptr;

        GpuDevMgr* pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
        if (pGpuDevMgr && pGpuDevMgr->NumDevices() > 0)
            gpudev = pGpuDevMgr->GetFirstGpuDevice();

        MASSERT(gpudev);

        return PraminPhysRdRaw(gpudev->GetSubdevice(0), physAddress, location, size, pOut);
    }

    RC PraminPhysRdRaw
    (
        GpuSubdevice *pSubDev,
        UINT64 physAddress,
        Memory::Location location,
        UINT64 size,
        vector<UINT08>* pOut
    )
    {
        RC rc;

        MASSERT(pSubDev);
        MASSERT(pOut != nullptr);
        MASSERT(size > 0);

        DebugPrintf("%s, Reading PA 0x%llx size 0x%llx.\n",
            __FUNCTION__,
            physAddress,
            size);

        UINT64 readAddr = physAddress;
        UINT64 bytesRemaining = size;
        const UINT64 BAR0_WINDOW_SIZE = 0x100000;
        UINT64 startIndex = 0;

        std::vector<UINT08>& dataReadback = *pOut;
        dataReadback.resize(size);

        while (bytesRemaining > 0)
        {
            auto scopedBar0window = pSubDev->ProgramScopedPraminWindow(readAddr, location);

            UINT64 bar0base = readAddr & 0x000000000000ffffLL;
            UINT64 bytesToRead = min(bytesRemaining, BAR0_WINDOW_SIZE - bar0base);

            Platform::PhysRd(pSubDev->GetPhysLwBase() + DEVICE_BASE(LW_PRAMIN) + bar0base,
                &dataReadback[startIndex], static_cast<UINT32>(bytesToRead));

            bytesRemaining -= bytesToRead;
            readAddr += bytesToRead;
            startIndex += bytesToRead;
        }

        return rc;
    }

    string GetSupportedClassesString(LwRm* pLwRm, GpuDevice* pGpuDevice)
    {
        string classListStr;
        vector<UINT32> classList;
        pLwRm->GetSupportedClasses(&classList, pGpuDevice);
        for (auto classNum : classList)
        {
            classListStr += Utility::StrPrintf("0x%x ", classNum);
        }
        classListStr.erase(classListStr.find_last_not_of(" ") + 1);
        return classListStr;
    }

    UINT08 GetIRQType()
    {
        if (GpuPtr()->GetMsixInterrupts())
        {
            return MODS_XP_IRQ_TYPE_MSIX;
        }
        else if (GpuPtr()->GetMsiInterrupts())
        {
            return MODS_XP_IRQ_TYPE_MSI;
        }
        else
        {
            return MODS_XP_IRQ_TYPE_INT;
        }
    }

    // For Access Counters in SMC mode, all smcengines will have the same value of
    // MMU Engine Id Start.
    // Lwrrently LW2080_CTRL_CMD_GPU_GET_ENGINE_FAULT_INFO returns a different
    // value for each smcengine.
    // As a workaround, to support access counter in SMC mode, GR0's MMU Engine Id
    // Start is queried and stored in Policy Manager before SMC partitioning
    RC CacheMMUEngineIdStart(GpuSubdevice* pGpuSubdev)
    {
        RC rc;

        if (s_Subdev2MMUEngineIdStart.find(pGpuSubdev) != s_Subdev2MMUEngineIdStart.end())
        {
            DebugPrintf("MMU engine id has already been cached for this gpu subdevice.");
            return rc;
        }

        LwRm* pLwRm = LwRmPtr().Get();

        LW2080_CTRL_GPU_GET_ENGINE_FAULT_INFO_PARAMS params = {};
        params.engineType = LW2080_ENGINE_TYPE_GR0;

        CHECK_RC(pLwRm->ControlBySubdevice(pGpuSubdev,
                LW2080_CTRL_CMD_GPU_GET_ENGINE_FAULT_INFO,
                (void *)&params,
                sizeof(params)));

        s_Subdev2MMUEngineIdStart[pGpuSubdev] = params.mmuFaultId;

        return rc;
    }

    UINT32 GetCachedMMUEngineIdStart(GpuSubdevice* pGpuSubdev)
    {
        map<GpuSubdevice*, UINT32>::iterator cachedEntry = s_Subdev2MMUEngineIdStart.find(pGpuSubdev);
        if (cachedEntry == s_Subdev2MMUEngineIdStart.end())
        {
            MASSERT(!"Could not find cached MMUEngineId start");
        }

        return cachedEntry->second;
    }

    bool IsFaultIndexRegisteredInUtl(UINT32 index)
    {
        return s_RegisteredUtlFaultIndices.find(index) != s_RegisteredUtlFaultIndices.end();
    }

    void RegisterFaultIndexInUtl(UINT32 index)
    {
        s_RegisteredUtlFaultIndices.insert(index);
    }

    RC ZeroMem(UINT64 addr, UINT32 len)
    {
        RC rc = OK;
        vector<UINT08> data(Platform::GetPageSize());

        DebugPrintf("Start to zeroize sysmem range from 0x%x to 0x%x, size:0x%x!\n", addr, addr + len -1, len);

        while (len > 0)
        {
            UINT32 count = (len < data.size()) ? len : data.size();
            CHECK_RC(Platform::PhysWr(addr, &data[0], count));
            addr += count;
            len -= count;
        }

        DebugPrintf("Finish zeroizing sysmem range from 0x%x to 0x%x!\n", addr, addr + len -1);

        return rc;
    }

} // MDiagUtils

