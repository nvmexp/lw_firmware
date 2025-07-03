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

#include "gpu/utility/surf2d.h"
#include "mdiag/utils/CrcInfo.h"
#include "mdiag/utils/utils.h"
#include "core/include/utility.h"
#include "mdiag/utils/IGpuSurfaceMgr.h"
#include "mdiag/tests/test_state_report.h"
#include "mdiag/resource/lwgpu/gpucrccalc.h"

#include "fermi/gf100/dev_bus.h"
#include "fermi/gf100/dev_ram.h"
#include "mdiag/utils/raster_types.h"

#ifndef _WIN32
    #include <unistd.h> // access, stat
    #include <errno.h>
    #include <limits.h>
    #include <stdlib.h>
#else
    #include <windows.h>
    #include <io.h>
    #include <direct.h>
#endif

#include "GpuVerif.h"
#include "ProfileKey.h"
#include "VerifUtils.h"
#include "DmaUtils.h"
#include "DumpUtils.h"
#include "core/include/chiplibtracecapture.h"

#include "SurfBufInfo.h"

#include "mdiag/tests/gpu/trace_3d/tracechan.h"

#include <sstream>

namespace
{

    // Print and debug CRC computed buffer size value.
    //
    // We have to catch so many CRC failure bugs as quickly as possible.
    // Dereferencing the computed size and checking it should be the 1st
    //   step upon any failure.
    // Lwrrently log the message at debug level, and throw a warning
    //   if size is not a multiple of 4B.
    //
    // Some CRC check fuctions use GetSize(), while others may use GetArrayPitch()
    // or GetUntiledSize() to get how many bytes should be computed.
    void CRCLog(const char* pComputeFunc, // CRC function name
            const char* pSizeTypeStr, // GetSize(), GetArrayPitch() or GetUntiledSize()?
            const char* pBufName,
            const UINT64 size
            )
    {
        stringstream msg;

        if (pComputeFunc != 0)
        {
            msg << pComputeFunc << ": ";
        }
        if (pBufName != 0)
        {
            msg << "Compute CRC for (" << pBufName << "), ";
        }
        msg << "computing ";
        if (pSizeTypeStr != 0)
        {
            msg << pSizeTypeStr << " ";
        }
        msg << "0x" << hex << size << " bytes";
        // Debug level log example:
        // [638291] (#1) Debug: DmaBufferInfo2::CpuComputeCRC: Compute CRC for (ce0_src.bin), computing Wholly-UntiledSize 0x40401 bytes.
        DebugPrintf("%s.\n", msg.str().c_str());
        if (size & 0x3)
        {
            // Throw at warning level if size is not a multiple of 4B.
            // Warning level log example:
            // [638133] Warning: DmaBufferInfo2::CpuComputeCRC: Compute CRC for (ce0_src.bin), computing Wholly-UntiledSize 0x40401 bytes, which is not a multiple of 4-byte - this could be a reason if later CRC check failed.
            msg << ", which is not a multiple of 4-byte - this could be a reason if later CRC check failed";

            WarnPrintf("%s.\n", msg.str().c_str());
        }
    }

} // namespace

static uintptr_t castToUintptr(UINT08 *p)
{
    return reinterpret_cast<uintptr_t>(p);
}

static bool GzAllowed(GpuVerif* verif)
{
    return !verif->Params()->ParamPresent("-no_tga_gz");
}

void CRCINFO2::Save(const string& key, const string& prof, UINT32 value, UINT32 subdev)
{
    // this may need to be re-visised
    // saving CRCs from one gpu only, since the rest of the inrastructure
    // is not ready to deal with CRC per gpu
    if (subdev != 0)
        return;

    Key = key;
    Prof = prof;
    CRCValue = value;
    gpuSubdevice = subdev;
}

ICrcVerifier* SurfaceInfo::GetChecker() const
{
    const CheckDmaBuffer* pCheckInfo = GetCheckInfo();
    return pCheckInfo && pCheckInfo->Check == REF_CHECK ?
             m_refChecker : m_crcChecker;
}

SurfaceInfo::SurfaceInfo(MdiagSurf* surface, GpuVerif* gpuVerif)
    : m_surface(surface), m_profkey(""), m_surfbase(0), m_crc(0), m_GpuVerif(gpuVerif)
{
    m_params = m_GpuVerif->Params();
    m_dmaUtils = m_GpuVerif->GetDmaUtils();
    m_dumpUtils = m_GpuVerif->GetDumpUtils();
    m_surfaceReader = m_dmaUtils->GetSurfaceReader();
    m_surfaceDumper = m_dmaUtils->GetSurfaceDumper();

    m_crcChecker = static_cast<ICrcVerifier*>(
            m_GpuVerif->GetCrcChecker(CT_CRC));
    m_refChecker = static_cast<ICrcVerifier*>(
            m_GpuVerif->GetCrcChecker(CT_Reference));
}

bool SurfaceInfo::IsChunked() const
{
    return m_params->ParamPresent("-crc_chunk_size") > 0 &&
           m_params->ParamUnsigned("-crc_chunk_size") > 0;
}

void SurfaceInfo::DumpCRC(TestEnums::TEST_STATUS& status)
{
    InfoPrintf("%s save key string %s\n", m_surface->GetName().c_str(), m_profkey.c_str());

    if(m_profkey == "") // couldn't find profile
    {
        status = TestEnums::TEST_CRC_NON_EXISTANT;
    }
    else
    {
        ICrcProfile* profile = m_GpuVerif->Profile();
        CrcEnums::CRC_MODE crcMode = m_GpuVerif->CrcMode();

        if(!profile->CheckCrc("trace_dx5", m_profkey.c_str(), m_crc,
            (crcMode > CrcEnums::CRC_CHECK)))
        {
            status = TestEnums::TEST_FAILED_CRC;
        }
    }
}

RC SurfaceInfo::DumpRawMemory(MdiagSurf*surface, UINT64 offset, UINT32 size)
{
    RC rc;

    CHECK_RC(DumpRawSurfaceMemory(m_GpuVerif->GetLwRmPtr(), m_GpuVerif->GetSmcEngine(), surface, offset, size, GetSurfaceName(), true, m_GpuVerif->LWGpu(), m_GpuVerif->Channel()->GetCh(), m_params));

    return rc;
}

//-------------- SurfInfo2 --------------
SurfInfo2::SurfInfo2(MdiagSurf* surface, GpuVerif* gpuVerif, int idx)
    : SurfaceInfo(surface, gpuVerif),
      filename(""), imgname("test"),
      tgadumped(false), index(idx), rawCrc(false), rect(0, 0, 0, 0)
{
    MASSERT(index >= -1 && index <= MAX_RENDER_TARGETS);
}

string SurfInfo2::CreateInitialKey(const string& suffix)
{
    string key;

    if (index != -1)
    {
        key = Utility::StrPrintf("0%c", VerifUtils::GetCrcColorBufferName(index));
    }
    else
    {
        key = "0z";
    }

    key += suffix;

    return key;
}

bool SurfInfo2::Verify
(
    CrcStatus *crcStatus,
    const char *key,
    ITestStateReport *report,
    UINT32 gpuSubdevice
)
{
    return GetChecker()->Verify(key, m_crc, this, gpuSubdevice, report, crcStatus);
}

RC SurfInfo2::DumpImage(const string &tgaProfileKey, UINT32 subdev, UINT32 index)
{
    if(tgadumped || !IsValid())
        return OK;

    string tgaName = filename;
    VerifUtils::AddGpuTagToDumpName(&tgaName, subdev);

    RC rc = DumpImageToFile(tgaName, tgaProfileKey, subdev);

    if (OK == rc)
    {
        tgadumped = true;
        m_crcInfo.CRCDumpIMGFilename =
            Utility::StrPrintf("crcdumpIMGFilename_%c ", IsZBuffer() ?
                    'z' : VerifUtils::GetCrcColorBufferName(index)) + tgaName;

        if (IsZBuffer())
        {
            // Dump the zlwll sanity image if we have it
            const UINT32 *zlwll_image = m_GpuVerif->SurfaceManager()->GetZLwllSanityImage(RASTER_A8R8G8B8);
            if (zlwll_image)
            {
                string k_name(filename);
                string::size_type pos = k_name.find("_z.tga");
                k_name[pos + 1] = 'k';

                m_dmaUtils->GetBuf()->SaveBufTGA(k_name.c_str(), m_surface->GetWidth(),
                        m_surface->GetHeight(), (uintptr_t) zlwll_image,
                        m_surface->GetWidth() * 4, 4, CTRaster(RASTER_A8R8G8B8,
                                CreateColorMap()["A8R8G8B8"]), 0);
            }
        }
    }

    return rc;
}

RC SurfInfo2::DumpImageToFile(const string& filename, const string &tgaProfileKey,
    UINT32 subdev)
{
    return m_surfaceDumper->DumpSurface(
            filename.c_str(), m_surface, m_surfbase, rect,
            tgaProfileKey.c_str(), subdev, rawCrc);
}

bool SurfInfo2::GenProfileKey
(
    const char*    suffix,
    bool           search,
    UINT32         subdev,
    std::string&   profileKey,
    bool           ignoreLocalLead
)
{
    return ProfileKey::Generate(
            m_GpuVerif, suffix, NULL, index, search,
            subdev, profileKey, ignoreLocalLead);
}

string SurfInfo2::GetImageNameSuffix(const string& extraSuffix)
{
    return VerifUtils::GetImageFileSuffix(index);
}

bool SurfInfo2::ZLwllSanityCheck(UINT32 subdev, bool* zlwll_passed)
{
    if (IsValid())
    {
        // bug 379291, if RawImages enabled, skip sanity check
        if (m_params->ParamPresent("-RawImageMode") > 0 &&
            m_params->ParamUnsigned("-RawImageMode") == RAWON)
        {
            InfoPrintf("Z lwll sanity check skipped since 'RawImages' mode enabled\n");
            return true;
        }

        // check if lwlling enabled
        IGpuSurfaceMgr* surfMgr = m_GpuVerif->SurfaceManager();
        if(!surfMgr->IsZlwllEnabled() && !surfMgr->IsSlwllEnabled())
            return true;

        // pull the uncompressed z buffer image to do the zlwll sanity check
        if(m_GpuVerif->RawImageMode() != RAWOFF)
        {
            vector<UINT08> ztempbuf_noraw;
            surfMgr->DisableRawFBAccess();

            _RAWMODE saveRawMode = m_GpuVerif->RawImageMode();
            m_GpuVerif->RawImageMode() = RAWOFF;

            RC rc = m_surfaceReader->ReadSurface(&ztempbuf_noraw, m_surface, subdev, false, g_NullRectangle);
            if(rc != OK)
            {
                ErrPrintf("Could not read back normal Z buffer: %s\n", rc.Message());
                return false;
            }

            surfMgr->EnableRawFBAccess(saveRawMode);
            UINT32* bufaddr = (UINT32*)castToUintptr(&ztempbuf_noraw[0]);
            if(surfMgr->DoZLwllSanityCheck(bufaddr, subdev) > 0)
            {
                ErrPrintf("ZLwll sanity check failed!\n");
                *zlwll_passed = false;
            }

            m_GpuVerif->RawImageMode() = saveRawMode; // turn back on...
        }
        else
        {
            if(surfMgr->DoZLwllSanityCheck((UINT32*)m_surfbase, subdev) > 0)
            {
                ErrPrintf("ZLwll sanity check failed!\n");
                *zlwll_passed = false;
            }
        }
    }

    return true;
}

bool SurfInfo2::GpuComputeCRC(UINT32 subdev)
{
    bool retVal = true;

    if(IsValid())
    {
        if(!(retVal = DoGpuComputeCRC(subdev)))
        {
            ErrPrintf("Error reading surface during CRC check!\n",
                      m_surface->GetName().c_str());
        }
    }

    return retVal;
}

bool SurfInfo2::CompareCRC(const char* key, UINT32 subdev, ITestStateReport* report, CrcStatus& crcStatus)
{
    IGpuSurfaceMgr* surfMgr = m_GpuVerif->SurfaceManager();
    LW50Raster *pRaster = surfMgr->GetRaster(m_surface->GetColorFormat());
    if(pRaster)
    {
        pRaster->SetBase(castToUintptr(m_surfbase));
    }

    // Read stored crcs and compare with measured CRC, dump color img if needed
    string strKey(key);
    strKey += IsZBuffer() ? 'z' :
            VerifUtils::GetCrcColorBufferName(index);

    return GetChecker()->Verify(strKey.c_str(), m_crc, this,
                                subdev, report, &crcStatus);
}

bool SurfInfo2::SaveMeasuredCRC(const char* key, UINT32 subdev)
{
    char suffix = IsZBuffer() ? 'z' : VerifUtils::GetCrcColorBufferName(index);
    string strKey(key);
    strKey += suffix;

    string buffer;
    bool retVal = ProfileKey::Generate(
            m_GpuVerif, strKey.c_str(), /* pCheck */ 0,
            index, /* search */ 0,
            subdev, buffer);

    if(retVal)
    {
        string str_crc("crc_");
        str_crc += suffix;

        m_crcInfo.Save(str_crc, buffer.c_str(), m_crc, subdev);
    }

    return retVal;
}

bool SurfInfo2::UpdateCRC(const string& tgaProfileKey, UINT32 subdev, UINT32 index)
{
    RC rc = OK;
    SurfaceDumper* surfaceDumper = m_GpuVerif->GetDmaUtils()->GetSurfaceDumper();
    const ArgReader* params = m_GpuVerif->Params();
    CrcEnums::CRC_MODE crcMode = m_GpuVerif->CrcMode();

    if(access(imgname.c_str(), 0) == -1)
    {
        // file does not exist, simply write it out
        VerifUtils::optionalCreateDir(imgname.c_str(), params);
        rc = surfaceDumper->DumpSurface(imgname.c_str(), m_surface, m_surfbase,
            rect, tgaProfileKey.c_str(), subdev, rawCrc);

        if (rc != OK)
            return false;

        if (crcMode == CrcEnums::CRC_UPDATE)
            VerifUtils::p4Action("add", imgname.c_str());
    }
    else
    {
        // if file is not writeable and we are updating, try to p4 edit it
        if(access(imgname.c_str(), 2) == -1)
        {
            VerifUtils::optionalCreateDir(imgname.c_str(), params);
            if(crcMode == CrcEnums::CRC_UPDATE)
                VerifUtils::p4Action("edit", imgname.c_str());
        }

        rc = surfaceDumper->DumpSurface(imgname.c_str(), m_surface, m_surfbase,
            rect, tgaProfileKey.c_str(), subdev, rawCrc);

        if (rc != OK)
            return false;
    }

    if (OK == rc)
    {
        char comment[64];
        sprintf(comment, "crcdumpIMGFilename_%c ", IsZBuffer() ?
                'z' : VerifUtils::GetCrcColorBufferName(index));
        m_crcInfo.CRCDumpIMGFilename = comment + imgname;
        return true;
    }
    else
    {
        return false;
    }
}

void SurfInfo2::DumpSurface(UINT32 subdev, const string& tgaProfileKey)
{
    // The dump_images_never option overrides all other image dumping options.
    bool dump_images_never = m_params->ParamPresent("-dump_images") && m_params->ParamUnsigned("-dump_images") == 1;
    if (!dump_images_never)
    {
        if (!tgadumped)
        {
            string tgaName = filename;
            VerifUtils::AddGpuTagToDumpName(&tgaName, subdev);

            m_surfaceDumper->DumpSurface(tgaName.c_str(), m_surface, m_surfbase, rect,
                tgaProfileKey.c_str(), subdev, rawCrc);

            tgadumped = true;
        }
    }
}

void SurfInfo2::PrintCRC() const
{
    Printf(Tee::PriNormal, " %s=0x%08x", m_surface->GetName().c_str(), m_crc);
}

bool SurfInfo2::IsValid() const
{
    return m_surface && m_GpuVerif->SurfaceManager()->GetValid(m_surface);
}

string SurfInfo2::GetSurfaceName() const
{
    return m_surface->GetName();
}

bool SurfInfo2::CpuComputeCRC(UINT32 subdev)
{
    bool retVal = true;
    if(IsValid())
    {
        if (m_dmaUtils->UseDmaTransfer())
        {
            m_surfaceReader->GetDmaReader()->BindSurface(m_surface->GetCtxDmaHandleGpu());
        }
        if(!(retVal = DoCpuComputeCRC(subdev)))
        {
            ErrPrintf("Error reading surface during CRC check!\n", m_surface->GetName().c_str());
        }
    }

    return retVal;
}

bool SurfInfo2::DoCpuComputeCRC(UINT32 subdev)
{
    if (IsChunked())
    {
        if (m_params->ParamPresent("-dump_raw") > 0)
        {
            // Raw image dumping is incompatible with chunked crc checking
            ErrPrintf("-dump_raw is not compatible with chunked CRC checking, "
                "will skip chunked crc checking and do full-surface read\n");
        }
        else
        {
            // Bug 507849, for some huge m_surfaces, mods supports read and CRC
            // part by part in order to reduce memory consumption
            return DoCpuComputeCRCByChunk(subdev);
        }
    }

    RC rc;
    UINT32 width;
    UINT32 height;

    rc = m_surfaceReader->ReadSurface(&surfdata, m_surface, subdev, rawCrc, rect);
    if (rc != OK)
    {
        ErrPrintf("Could not read back %s buffer: %s\n",
            m_surface->GetName().c_str(), rc.Message());
        return false;
    }
    if (!rect.IsNull())
    {
        // -crc_rectX option is used
        width  = rect.width;
        height = rect.height;
    }
    else
    {
        width  = m_surface->GetWidth();
        height = m_surface->GetHeight();
    }

    // check if we're just in the backdoor archive read case.
    if (!(m_params->ParamPresent("-backdoor_archive") &&
        m_params->ParamUnsigned("-backdoor_archive") == 1))
    {
        UINT32 size = 0;
        const CheckDmaBuffer* pCheckInfo = GetCheckInfo();
        if (pCheckInfo)
        {
            size = pCheckInfo->GetUntiledSize();
        }
        // if size is not specified, callwlate it here
        if (size == 0)
        {
            size = width*m_surface->GetBytesPerPixel()*height;
            size = size*m_surface->GetDepth()*m_surface->GetArraySize();
            if (width == 0) size = m_surface->GetSize();
            CRCLog("SurfInfo2::DoCpuComputeCRC", "Size",
                    m_surface->GetName().c_str(), size);
        }
        else
        {
            CRCLog("SurfInfo2::DoCpuComputeCRC", "UntiledSize",
                    m_surface->GetName().c_str(), size);
        }

        if (m_GpuVerif->GetPreCrcValue() > 0)
        {
            m_crc = BufCRC32Append(castToUintptr(&surfdata[0]), size, m_GpuVerif->GetPreCrcValue());
            m_GpuVerif->SetPreCrcValue(0);
        }
        else
        {
            m_crc = BufCRC32(castToUintptr(&surfdata[0]), size);
        }
    }
    m_surfbase = &surfdata[0];

    if (m_params->ParamPresent("-dump_raw"))
    {
        std::string rawFilename(filename);
        DumpUtils* m_dumpUtils = m_GpuVerif->GetDumpUtils();
        m_dumpUtils->DumpRawImages(m_surfbase, surfdata.size(), subdev, rawFilename);

        tgadumped = true;
    }
    else
    {
        tgadumped = false;
    }

    return true;
}

// -----------------------------------------------------------------------------
//  read and callwlate CRC for a surface in chunks
// -----------------------------------------------------------------------------
bool SurfInfo2::DoCpuComputeCRCByChunk(UINT32 subdev)
{
    MASSERT(((m_surface->GetWidth() != 0) && (m_surface->GetHeight() != 0)) ||
            (m_surface->GetArrayPitch() != 0));

    WarnPrintf("-crc_chunk_size will disable image dump if crc is callwlated in the end of test."
               " Try to use CHECK_DYNAMICSURFACE to callwlate crc explicitly and then images can be dumped.");
    if (m_surface->GetWidth() == 0 || m_surface->GetHeight() == 0)
    {
        return DoCpuComputeCRCByChunkForPitch(subdev);
    }

    RC rc;
    UINT32 chunk_size = m_params->ParamUnsigned("-crc_chunk_size");

    InfoPrintf("Read and CRC surface in chunks with size: %d bytes\n", chunk_size);

    UINT32 width  = m_surface->GetWidth();
    UINT32 height = m_surface->GetHeight();
    UINT32 bpp    = m_surface->GetBytesPerPixel();
    UINT32 depth  = m_surface->GetDepth();
    UINT32 arraysize = m_surface->GetArraySize();
    UINT32 height_per_part = max(chunk_size/(width*bpp*depth*arraysize), (UINT32)1);
    UINT32 parts = height / height_per_part;
    UINT32 rmdr  = height % height_per_part;
    UINT32 size_per_part = width * depth * bpp * arraysize * height_per_part;
    UINT32 size_rmdr     = width * depth * bpp * arraysize * rmdr;
    UINT32 crcVal = 0;

    CRCLog("SurfInfo2::DoCpuComputeCRCByChunk", "NonPitch",
                m_surface->GetName().c_str(), parts * size_per_part + size_rmdr);

    for (UINT32 i = 0; i < parts; i++)
    {
        vector<UINT08> tmp(size_per_part);
        SurfaceAssembler::Rectangle rect(0, height_per_part * i, width, height_per_part);
        rc = m_surfaceReader->ReadSurface(&tmp, m_surface, subdev, rawCrc, rect);
        if (rc != OK)
        {
            ErrPrintf("Error in reading back buffer %s in part of height "
                "%u - %u: %s\n", m_surface->GetName().c_str(), height_per_part * i,
                height_per_part*(i+1) - 1, rc.Message());
            return false;
        }
        crcVal = BufCRC32Append(castToUintptr(&tmp[0]), size_per_part, crcVal);
    }
    if (rmdr != 0)
    {
        vector<UINT08> tmp(size_rmdr);
        SurfaceAssembler::Rectangle rect(0, height_per_part*parts, width, rmdr);
        rc = m_surfaceReader->ReadSurface(&tmp, m_surface, subdev, rawCrc, rect);
        if (rc != OK)
        {
            ErrPrintf("Error in reading back buffer %s in part of height "
                "%u - %u: %s\n", m_surface->GetName().c_str(), height_per_part*parts,
                height_per_part*parts + rmdr - 1, rc.Message());
            return false;
        }
        crcVal = BufCRC32Append(castToUintptr(&tmp[0]), size_rmdr, crcVal);
    }

    tgadumped = true; // FIXME: dumping is not well supported here, this
                      // line is to avoid image dumping crash afterwards
    m_crc = crcVal;

    return true;
}

// -----------------------------------------------------------------------------
//  read and callwlate CRC for a pitch surface in chunks
// -----------------------------------------------------------------------------
bool SurfInfo2::DoCpuComputeCRCByChunkForPitch(UINT32 subdev)
{
    MASSERT(m_surface->GetArrayPitch() != 0);

    RC rc;
    UINT32 chunkSize = m_params->ParamUnsigned("-crc_chunk_size");

    CRCLog("SurfInfo2::DoCpuComputeCRCByChunkForPitch", "ArrayPitch",
            m_surface->GetName().c_str(), m_surface->GetArrayPitch());

    InfoPrintf("Read and CRC pitch surface in chunks with size: %d bytes\n", chunkSize);

    UINT32 parts = m_surface->GetArrayPitch() / chunkSize;
    UINT32 rmdr = m_surface->GetArrayPitch() % chunkSize;
    UINT32 crcVal = 0;

    for (UINT32 i = 0; i < parts; ++i)
    {
        vector<UINT08> tmp(chunkSize);
        SurfaceAssembler::Rectangle rect(chunkSize * i, 0, chunkSize, 1);
        rc = m_surfaceReader->ReadSurface(&tmp, m_surface, subdev, rawCrc, rect);
        if (rc != OK)
        {
            ErrPrintf("Error in reading back buffer %s.\n",
                m_surface->GetName().c_str());
            return false;
        }
        crcVal = BufCRC32Append(castToUintptr(&tmp[0]), chunkSize, crcVal);
    }
    if (rmdr != 0)
    {
        vector<UINT08> tmp(rmdr);
        SurfaceAssembler::Rectangle rect(chunkSize * parts, 0, rmdr, 1);
        rc = m_surfaceReader->ReadSurface(&tmp, m_surface, subdev, rawCrc, rect);
        if (rc != OK)
        {
            ErrPrintf("Error in reading back buffer %s.\n",
                m_surface->GetName().c_str());
            return false;
        }
        crcVal = BufCRC32Append(castToUintptr(&tmp[0]), rmdr, crcVal);
    }

    tgadumped = true;
    m_crc = crcVal;

    return true;
}

bool SurfInfo2::DoGpuComputeCRC(UINT32 subdev)
{
    RC rc = OK;

    UINT32 size = 0;
    const CheckDmaBuffer* pCheckInfo = GetCheckInfo();
    if (pCheckInfo)
    {
        size = pCheckInfo->GetUntiledSize();
    }
    // if size is not specified, callwlate it here
    if (size == 0)
    {
        UINT32 width  = m_surface->GetWidth();
        UINT32 height = m_surface->GetHeight();
        size = width*m_surface->GetBytesPerPixel()*height;
        size = size*m_surface->GetDepth()*m_surface->GetArraySize();
        if (size == 0)
        {
            // No width and height, so not a graphics surface
            size = static_cast<UINT32>(m_surface->GetSize());
        }
    }

    if (size & 0x3)
    {
        ErrPrintf("Could not CRC %s buffer: HW CRC supported only for "
                  "multiple-of-four sizes.\n", m_surface->GetName().c_str());
        return false;
    }

    UINT32 hSrcCtxDma = m_surface->GetCtxDmaHandle();
    UINT64 SrcOffset = m_surface->GetCtxDmaOffsetGpu() + m_surface->GetExtraAllocSize();

    bool doBlit = false;
    if (m_params->ParamPresent("-copy_surface"))
    {
        if (m_params->ParamUnsigned("-copy_surface") == 0 ||
            m_params->ParamUnsigned("-copy_surface") == 1)
        {
            doBlit = true;
        }
    }
    else if (m_surface->GetLayout() == Surface2D::BlockLinear)
    {
        // need to colwert to pitch
        doBlit = true;
    }
    else if (m_surface->GetPitch() != m_surface->GetWidth()*m_surface->GetBytesPerPixel())
    {
        // need to colwert to naive pitch
        doBlit = true;
    }
    else if (SrcOffset & 0xff)
    {
        // need to align for FECS API
        doBlit = true;
    }
    if (doBlit)
    {
        rc = m_surfaceReader->CopySurface(m_surface, subdev);

        if(rc != OK)
        {
            ErrPrintf("Could not read back %s buffer: %s\n",
                m_surface->GetName().c_str(), rc.Message());
            return false;
        }

        hSrcCtxDma = m_surfaceReader->GetCopyHandle();
        SrcOffset = m_surfaceReader->GetCopyBuffer();
    }

    GpuCrcCallwlator* gpuCalc = m_dmaUtils->GetGpuCrcCallwlator();
    rc = gpuCalc->ReadLine(hSrcCtxDma, SrcOffset, 0, 1, size, subdev, m_surface->GetSurf2D());

    if(rc != OK)
    {
        ErrPrintf("Could not read back %s buffer: %s\n",
            m_surface->GetName().c_str(), rc.Message());
        return false;
    }

    {
        ChiplibOpScope::Crc_Surf_Read_Info surfInfo =
        {
            m_surface->GetName(), ColorUtils::FormatToString(m_surface->GetColorFormat()),
            m_surface->GetPitch(), m_surface->GetWidth(), m_surface->GetHeight(),
            m_surface->GetBytesPerPixel(), m_surface->GetDepth(),
            m_surface->GetArraySize(), 0, 0, m_surface->GetWidth(), m_surface->GetHeight(), 0, 0
        };

        ChiplibOpScope newScope("gpucalc_crc_read", NON_IRQ,
            ChiplibOpScope::SCOPE_CRC_SURF_READ, &surfInfo);

        rc = m_GpuVerif->FlushGpuCache(subdev);

        if (rc != OK)
        {
            ErrPrintf("cannot flush gpu l2");
            return false;
        }

        DebugPrintf("GPU-callwlated CRC is at address 0x%x\n", gpuCalc->GetBuffer());
        Platform::VirtualRd((void*)gpuCalc->GetBuffer(), &m_crc, 4);
        tgadumped = false;
    }

    return true;
}

bool SurfInfo2::BuildProfKeyImgName(const char* key, UINT32 subdev)
{
    ICrcProfile* profile = m_GpuVerif->Profile();

    string strKey(key);
    strKey += IsZBuffer() ?
            'z' : VerifUtils::GetCrcColorBufferName(index);

    string lBuffer("");
    if (!ProfileKey::Generate(m_GpuVerif, strKey.c_str(), NULL, index, true,
                              subdev, lBuffer, false))
    {
        return false;
    }

    // save color profile key for crc check
    m_profkey = lBuffer;

    // remove key suffix
    lBuffer[lBuffer.length() - 1 - strKey.size()] = '\0';

    if (IsZBuffer())
    {
        string suffix("_z.tga");
        if (!m_GpuVerif->Params()->ParamPresent("-no_tga_gz"))
            suffix += ".gz";

        imgname = VerifUtils::create_imgname(
                        profile->GetImgDir().c_str(),
                        profile->GetTestName().c_str(),
                        lBuffer.c_str(),
                        suffix.c_str(), false);
    }
    else
    {
        imgname = VerifUtils::create_imgname(
                profile->GetImgDir().c_str(),
                profile->GetTestName().c_str(),
                lBuffer.c_str(),
                VerifUtils::GetImageFileSuffix(index).c_str(),
                false);
    }

    InfoPrintf("%s crcdumpIMGFilename=%s\n", GetSurfaceName().c_str(), imgname.c_str());

    struct stat statbuf;
    bool imgFileMissing = (stat(imgname.c_str(), &statbuf) < 0);

    if (imgFileMissing)
    {
        InfoPrintf("The above image file does not exist.\n");
        InfoPrintf("MODS is deducing the filepath based on the tracepath.\n");
        InfoPrintf("Please try changing the class root or may be look around to find the image file.\n");
    }

    CrcEnums::CRC_MODE crcMode = m_GpuVerif->CrcMode();
    if (crcMode == CrcEnums::CRC_REPORT)
    {
        if (imgFileMissing)
        {
            m_GpuVerif->WriteToReport("crcReport: image file %s is missing\n", imgname.c_str());
        }
        else
        {
            m_GpuVerif->WriteToReport("crcReport: HAVE image file %s\n", imgname.c_str());
        }
    }

    return true;
}

void SurfInfo2::BuildImgFilename()
{
    const string& outputFilename = m_GpuVerif->OutFilename();
    if (IsZBuffer())
    {
        filename = outputFilename;

        if (GzAllowed(m_GpuVerif))
        {
            filename += "_z.tga.gz";
        }
        else
        {
            filename += "_z.tga";
        }
    }
    else
    {
        filename = outputFilename;
        filename += VerifUtils::GetImageFileSuffix(index);
    }
}

AllocSurfaceInfo2::AllocSurfaceInfo2(MdiagSurf*surface, GpuVerif* gpuVerif)
    : SurfInfo2(surface, gpuVerif, -1), m_UseTga(false)
{
    if ((surface->GetWidth() > 0) && (surface->GetHeight() > 0))
    {
        m_UseTga = true;
    }
}

bool AllocSurfaceInfo2::SaveMeasuredCRC(const char* key, UINT32 subdev)
{
    string buffer;
    bool retVal = false;
    if((retVal = ProfileKey::Generate(
            m_GpuVerif, key,
            &m_CheckInfo,
            0, 0, subdev, buffer)))
    {
        string str_crc("crc_");
        str_crc += m_CheckInfo.Filename;

        m_crcInfo.Save(str_crc, buffer.c_str(), m_crc, subdev);
    }

    return retVal;
}

bool AllocSurfaceInfo2::UpdateCRC(const string& tgaProfileKey, UINT32 subdev, UINT32 index)
{
    CrcEnums::CRC_MODE crcMode = m_GpuVerif->CrcMode();
    const ArgReader* params = m_GpuVerif->Params();

    if (access(imgname.c_str(), 0) == -1)
    {
        // file does not exist, simply write it out
        VerifUtils::optionalCreateDir(imgname.c_str(), params);

        if (OK != DumpImageToFile(imgname, tgaProfileKey, subdev))
        {
            return false;
        }

        if (crcMode == CrcEnums::CRC_UPDATE)
            VerifUtils::p4Action("add", imgname.c_str());
    }
    else
    {
        // if file is not writeable and we are updating, try to p4 edit it
        if(access(imgname.c_str(), 2) == -1)
        {
            VerifUtils::optionalCreateDir(imgname.c_str(), params);
            if(crcMode == CrcEnums::CRC_UPDATE)
                VerifUtils::p4Action("edit", imgname.c_str());
        }

        if (OK != DumpImageToFile(imgname, tgaProfileKey, subdev))
        {
            return false;
        }
    }

    return true;
}

RC AllocSurfaceInfo2::DumpImage(const string &tgaProfileKey, UINT32 subdev, UINT32 index)
{
    if(tgadumped || !IsValid())
        return OK;

    string tgaName = filename;
    VerifUtils::AddGpuTagToDumpName(&tgaName, subdev);

    RC rc = DumpImageToFile(tgaName, tgaProfileKey, subdev);

    if (OK == rc)
    {
        tgadumped = true;
        m_crcInfo.CRCDumpIMGFilename = "crcdumpIMGFilename " + filename;
    }

    return rc;
}

string AllocSurfaceInfo2::CreateInitialKey(const string& suffix)
{
    return string("0" + suffix);
}

RC AllocSurfaceInfo2::DumpImageToFile(const string& filename,
    const string &profileKey, UINT32 gpuSubdevice)
{
    if (m_UseTga)
    {
        return m_surfaceDumper->DumpSurface(
                filename.c_str(), m_surface, m_surfbase, rect,
                profileKey.c_str(), gpuSubdevice, rawCrc);
    }
    else
    {
        return m_dumpUtils->DumpDmaBuffer(
                filename.c_str(), &m_CheckInfo, m_surfbase, gpuSubdevice);
    }
}

bool AllocSurfaceInfo2::GenProfileKey
(
    const char*    suffix,
    bool           search,
    UINT32         subdev,
    std::string&   profileKey,
    bool           ignoreLocalLead
)
{
    return ProfileKey::Generate(
            m_GpuVerif, suffix, &m_CheckInfo, 0, search,
            subdev, profileKey, ignoreLocalLead);
}

bool AllocSurfaceInfo2::BuildProfKeyImgName(const char* key, UINT32 subdev)
{
    return BuildProfKeyImgNameSuffix(key, "", subdev);
}

bool AllocSurfaceInfo2::BuildProfKeyImgNameSuffix(const char* key,
        const string& suffix, UINT32 subdev)
{
    ICrcProfile* profile = m_GpuVerif->Profile();

    string lBuffer("");
    if (!GenProfileKey(key, true, subdev, lBuffer))
    {
        return false;
    }

    // save profile key for crc check
    m_profkey = lBuffer;

    // remove key suffix
    lBuffer[lBuffer.length()-1-strlen(key)] = '\0';

    string theSuffix = GetImageNameSuffix(suffix);

    imgname = VerifUtils::create_imgname(
            profile->GetImgDir().c_str(),
            profile->GetTestName().c_str(),
            lBuffer.c_str(),
            theSuffix.c_str(), true);

    return true;
}

string AllocSurfaceInfo2::GetImageNameSuffix(const string& extraSuffix)
{
    string suffix = "_";
    suffix += m_CheckInfo.Filename + extraSuffix;
    suffix += ".gz";

    return suffix;
}

void AllocSurfaceInfo2::BuildImgFilename()
{
    BuildImgFilenameSuffix("");
}

void AllocSurfaceInfo2::BuildImgFilenameSuffix(const string& suffix)
{
    const string& outputFilename = m_GpuVerif->OutFilename();

    if (m_UseTga)
    {
        filename = outputFilename;
        // filename += cframe;
        filename += "_";
        filename += m_CheckInfo.Filename + suffix;
        filename += ".tga";
    }
    else
    {
        filename = "_";
        filename += m_CheckInfo.Filename + suffix;
    }

    if (GzAllowed(m_GpuVerif))
    {
        filename += ".gz";
    }
}

bool AllocSurfaceInfo2::CompareCRC(const char* key, UINT32 subdev, ITestStateReport* report, CrcStatus& crcStatus)
{
    // Read stored crcs and compare with measured CRC, dump color img if needed
    return GetChecker()->Verify(key, m_crc, this, subdev, report, &crcStatus);
}

void AllocSurfaceInfo2::DumpSurface(UINT32 subdev, const string& tgaProfileKey)
{
    // The dump_images_never option overrides all other image dumping options.
    bool dump_images_never = m_params->ParamPresent("-dump_images") && m_params->ParamUnsigned("-dump_images") == 1;
    if (!dump_images_never)
    {
        if (!tgadumped)
        {
            if (m_UseTga)
            {
                m_surfaceDumper->DumpSurface(
                    filename.c_str(), m_surface, m_surfbase, rect, tgaProfileKey.c_str(), subdev, rawCrc);
            }
            else
            {
                m_dumpUtils->DumpDmaBuffer(filename.c_str(), &m_CheckInfo, m_surfbase, subdev);
            }

            tgadumped = true;
        }
    }
}

bool AllocSurfaceInfo2::MustMatchCrc() const
{
    return m_CheckInfo.CrcMatch;
}

DmaBufferInfo2::DmaBufferInfo2(MdiagSurf*surface, GpuVerif* gpuVerif)
    : SurfaceInfo(surface, gpuVerif)
{
}

DmaBufferInfo2::~DmaBufferInfo2()
{
    if (m_surfbase)
    {
        delete [] m_surfbase;
    }
}

bool DmaBufferInfo2::GpuComputeCRC(UINT32 subdev)
{
    if (DmaBuffers.GetBufferSize() & 0x3)
    {
        ErrPrintf("Could not CRC %s buffer: HW CRC supported only for "
                  "multiple-of-four sizes.\n", DmaBuffers.Filename.c_str());
        return false;
    }

    MdiagSurf *pDmaBuf = DmaBuffers.pDmaBuf;

    // Offset from base of context DMA
    UINT64 SrcOffset = pDmaBuf->GetCtxDmaOffsetGpu() +
                       pDmaBuf->GetExtraAllocSize() +
                       DmaBuffers.Offset;

    UINT32 hSrcCtxDma = pDmaBuf->GetCtxDmaHandle();

    if ((SrcOffset & 0xff) != 0) // doBlit
    {
        RC rc = m_surfaceReader->CopySurface(pDmaBuf, subdev, DmaBuffers.Offset);

        if(rc != OK)
        {
            ErrPrintf("Could not read back buffer with VA: 0x%x\n",
                SrcOffset);
            return false;
        }
        hSrcCtxDma = m_surfaceReader->GetCopyHandle();
        SrcOffset = m_surfaceReader->GetCopyBuffer();
    }

    GpuCrcCallwlator* gpuCalc = m_dmaUtils->GetGpuCrcCallwlator();
    RC rc = gpuCalc->ReadLine(
                hSrcCtxDma, SrcOffset, 0,
                1, DmaBuffers.GetBufferSize(), subdev, pDmaBuf->GetSurf2D());

    if (rc != OK)
    {
        ErrPrintf("Could not read back %s: %s\n",
            DmaBuffers.Filename.c_str(), rc.Message());
        return false;
    }

    Platform::VirtualRd((void*)gpuCalc->GetBuffer(), &m_crc, 4);
    return true;
}

bool DmaBufferInfo2::CompareCRC(const char* key, UINT32 subdev, ITestStateReport* report, CrcStatus& crcStatus)
{
    string strKey(key);

    // verify dma CRC, dump dma buffer if needed
    if(GetChecker()->Verify(strKey.c_str(), m_crc, this, subdev, report, &crcStatus))
    {
        RC rc = m_dumpUtils->DumpDmaBuffer(dmabuf_name.c_str(),
                              &DmaBuffers, m_surfbase, subdev);
        if(rc != OK)
            ErrPrintf("Error writing file: %s\n", rc.Message());

        dmabuf_dumped = true;
        return true;
    }
    else
    {
        dmabuf_dumped = false;
    }

    return false;
}

bool DmaBufferInfo2::CpuComputeCRC(UINT32 subdev)
{
    UINT08* pDMABuffer;

    if (m_dmaUtils->UseDmaTransfer())
    {
        m_surfaceReader->GetDmaReader()->BindSurface(DmaBuffers.pDmaBuf->GetCtxDmaHandleGpu());
    }

    // Surface information to dump chiplib trace; No rect here
    const MdiagSurf* pSurface = DmaBuffers.pDmaBuf;
    ChiplibOpScope::Crc_Surf_Read_Info surfInfo =
    {
        pSurface->GetName(), ColorUtils::FormatToString(pSurface->GetColorFormat()),
        pSurface->GetPitch(), pSurface->GetWidth(), pSurface->GetHeight(),
        pSurface->GetBytesPerPixel(), pSurface->GetDepth(),
        pSurface->GetArraySize(), 0, 0, pSurface->GetWidth(), pSurface->GetHeight(), 0, 0
    };
    ChiplibOpScope newScope("crc_surf_read", NON_IRQ,
        ChiplibOpScope::SCOPE_CRC_SURF_READ, &surfInfo);

    bool bNeedRestoreChiplibTraceDump = false;

    // Disable the chiplib trace dump so that the normal BAR1 reads aren't dumped.
    if ((m_params->ParamPresent("-backdoor_crc")) && ChiplibTraceDumper::GetPtr()->IsChiplibTraceDumpEnabled())
    {
        ChiplibTraceDumper::GetPtr()->DisableChiplibTraceDump();
        bNeedRestoreChiplibTraceDump = true;
    }

    RC rc = OK;
    if (IsChunked())
    {
        UINT32 chunkSize = m_params->ParamUnsigned("-crc_chunk_size");
        UINT32 fullSize  = DmaBuffers.GetBufferSize();
        UINT32 crcVal = 0;

        CRCLog("DmaBufferInfo2::CpuComputeCRC", "Chunked-BufferSize",
                DmaBuffers.Filename.c_str(), DmaBuffers.GetBufferSize());

        if (m_params->ParamPresent("-dump_raw") > 0)
        {
            WarnPrintf("-crc_chunk_size & -dump_raw are used both, "
                       "disable chunked crc checking mode");
            chunkSize = fullSize;
        }
        else
        {
            InfoPrintf("Chunked CRC checking mode enabled, chunk size %d, reading %s...\n",
                chunkSize, DmaBuffers.Filename.c_str());
        }

        UINT32 saveOffset = DmaBuffers.Offset;
        while (fullSize > 0)
        {
            UINT32 readSize = min(fullSize, chunkSize);
            rc = m_surfaceReader->ReadDmaBuffer(&pDMABuffer, &DmaBuffers, subdev, readSize);
            if (rc != OK)
            {
                ErrPrintf("Could not read back %s: %s\n",
                    DmaBuffers.Filename.c_str(), rc.Message());
                //delete[] pDMABuffer; // This line has been called in ReadDmaBuffer()
                                       // but I still leave it here as a reminder
                return false;
            }
            DmaBuffers.Offset += readSize;
            crcVal = BufCRC32Append(castToUintptr(pDMABuffer), readSize, crcVal);
            if (readSize == DmaBuffers.GetBufferSize())
            {
                // In case of the whole buffer is actually read back once,
                // we still keep the memory for dumping use
                m_surfbase = pDMABuffer;
            }
            else
            {
                delete[] pDMABuffer;
                pDMABuffer = 0;
            }
            fullSize -= readSize;
        }
        DmaBuffers.Offset = saveOffset;
        m_crc = crcVal;
    }
    else
    {
        rc = m_surfaceReader->ReadDmaBuffer(&pDMABuffer, &DmaBuffers, subdev, 0);
        if (rc != OK)
        {
            ErrPrintf("Could not read back %s: %s\n",
                DmaBuffers.Filename.c_str(), rc.Message());
            return false;
        }

        CRCLog("DmaBufferInfo2::CpuComputeCRC", "Wholly-UntiledSize",
                DmaBuffers.Filename.c_str(), DmaBuffers.GetUntiledSize());

        m_surfbase = pDMABuffer;
        m_crc = BufCRC32(castToUintptr(pDMABuffer), DmaBuffers.GetUntiledSize());

        if (m_params->ParamPresent("-dump_raw"))
        {
            std::string rawFilename(m_GpuVerif->OutFilename());
            rawFilename += dmabuf_name;
            UINT32 size = DmaBuffers.GetBufferSize();

            m_dumpUtils->DumpRawImages(pDMABuffer, size, subdev, rawFilename);

            dmabuf_dumped = true;
        }
    }

    if (bNeedRestoreChiplibTraceDump)
    {
        ChiplibTraceDumper::GetPtr()->EnableChiplibTraceDump();
    }

    if (m_params->ParamPresent("-backdoor_crc"))
    {
        if (OK != DumpRawMemory(DmaBuffers.pDmaBuf, DmaBuffers.Offset, DmaBuffers.GetBufferSize()))
        {
            return false;
        }
    }

    return true;
}

void DmaBufferInfo2::PrintCRC() const
{
    Printf(Tee::PriNormal, " %s=0x%08x", DmaBuffers.Filename.c_str(), m_crc);
}

bool DmaBufferInfo2::SaveMeasuredCRC(const char* key, UINT32 subdev)
{
    string strKey(key);

    string buffer;
    bool retVal = false;
    if((retVal = ProfileKey::Generate(
            m_GpuVerif, strKey.c_str(),
            &DmaBuffers,
            0, 0, subdev, buffer)))
    {
        string str_crc("crc_");
        str_crc += DmaBuffers.Filename;

        m_crcInfo.Save(str_crc, buffer.c_str(), m_crc, subdev);
    }

    return retVal;
}

bool DmaBufferInfo2::UpdateCRC(const string& tgaProfileKey, UINT32 subdev, UINT32 index)
{
    RC rc = OK;
    DumpUtils* dumpUtils = m_GpuVerif->GetDumpUtils();
    CrcEnums::CRC_MODE crcMode = m_GpuVerif->CrcMode();
    const ArgReader* params = m_GpuVerif->Params();

    if (access(imgNameDmaBuf.c_str(), 0) == -1)
    {
        // file does not exist, simply write it out
        VerifUtils::optionalCreateDir(imgNameDmaBuf.c_str(), params);
        rc = dumpUtils->DumpDmaBuffer(imgNameDmaBuf.c_str(), &DmaBuffers,
                m_surfbase, subdev);
        if (rc != OK)
            ErrPrintf("Error writing file: %s\n", rc.Message());
        if (crcMode == CrcEnums::CRC_UPDATE)
            VerifUtils::p4Action("add", imgNameDmaBuf.c_str());
    }
    else
    {
        // if file is not writeable and we are updating, try to p4 edit it
        if (access(imgNameDmaBuf.c_str(), 2) == -1)
        {
            VerifUtils::optionalCreateDir(imgNameDmaBuf.c_str(), params);
            if (crcMode == CrcEnums::CRC_UPDATE)
                VerifUtils::p4Action("edit", imgNameDmaBuf.c_str());
        }

        rc = dumpUtils->DumpDmaBuffer(imgNameDmaBuf.c_str(), &DmaBuffers,
                m_surfbase, subdev);
        if (rc != OK)
            ErrPrintf("Error writing file: %s\n", rc.Message());
    }

    if (OK == rc)
    {
        char comment[64];
        sprintf(comment, "crcdumpIMGFilename_dma[%d] ", index);
        m_crcInfo.CRCDumpIMGFilename = comment + imgNameDmaBuf;
        return true;
    }

    return false;
}

RC DmaBufferInfo2::DumpImage(const string &tgaProfileKey, UINT32 subdev, UINT32 index)
{
    if(dmabuf_dumped || !IsValid())
        return OK;

    RC rc = m_dumpUtils->DumpDmaBuffer(dmabuf_name.c_str(),
            &DmaBuffers, m_surfbase, subdev);

    if(rc != OK)
        ErrPrintf("Error writing file: %s\n", rc.Message());
    else
        dmabuf_dumped = true;

    if (OK == rc)
    {
        m_crcInfo.CRCDumpIMGFilename =
            Utility::StrPrintf("crcdumpIMGFilename_dma[%d] ", index)
            + dmabuf_name;
    }

    return rc;
}

bool DmaBufferInfo2::GenProfileKey
(
    const char*    suffix,
    bool           search,
    UINT32         subdev,
    std::string&   profileKey,
    bool           ignoreLocalLead
)
{
    return ProfileKey::Generate(
            m_GpuVerif, suffix, &DmaBuffers, 0, search,
            subdev, profileKey, ignoreLocalLead);
}

bool DmaBufferInfo2::BuildProfKeyImgName(const char* key, UINT32 subdev)
{
    string profileKeyIMG("");
    if (m_GpuVerif->GetGpuKey().empty())
    {
        profileKeyIMG = *m_GpuVerif->CrcChain_Manager()->GetCrcChain().begin();
    }
    else
    {
        profileKeyIMG = m_GpuVerif->GetGpuKey();
    }

    string suffix = "_";
    suffix += DmaBuffers.Filename.c_str();
    suffix += ".gz";

    string buffer("");

    ICrcProfile* profile = m_GpuVerif->Profile();
    if (!GenProfileKey(key, true, subdev, buffer, false))
    {
        return false;
    }

    m_profkey = buffer;

    imgNameDmaBuf = VerifUtils::create_imgname(
            profile->GetImgDir().c_str(),
            profile->GetTestName().c_str(),
            profileKeyIMG.c_str(),
            suffix.c_str(), true);

    return true;
}

void DmaBufferInfo2::BuildImgFilename()
{
    dmabuf_name += "_";
    dmabuf_name += DmaBuffers.Filename;

    if (GzAllowed(m_GpuVerif))
        dmabuf_name += ".gz";
}

string DmaBufferInfo2::GetSurfaceName() const
{
    return DmaBuffers.Filename;
}

bool DmaBufferInfo2::MustMatchCrc() const
{
    return DmaBuffers.CrcMatch;
}

