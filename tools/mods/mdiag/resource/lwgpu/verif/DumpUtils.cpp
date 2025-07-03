/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "DumpUtils.h"

#include "mdiag/utils/CrcInfo.h"
#include "mdiag/utils/TestInfo.h"

#include "mdiag/utils/utils.h"
#include "core/include/utility.h"
#include "mdiag/sysspec.h"
#include "core/utility/errloggr.h"

#include "mdiag/resource/lwgpu/dmasurfr.h"
#include "mdiag/resource/lwgpu/crcchain.h"

#include "VerifUtils.h"
#include "ProfileKey.h"
#include "DmaUtils.h"
#include "GpuVerif.h"
#include "mdiag/smc/smcengine.h"
#include "mdiag/tests/gpu/trace_3d/tracechan.h"

#define foreach_all_surfaces(it) \
    for(SurfaceInfos::iterator it = m_verifier->AllSurfaces().begin(); \
                               it!= m_verifier->AllSurfaces().end(); ++it)

DumpUtils::DumpUtils(GpuVerif* verifier) : m_verifier(verifier), m_params(verifier->Params())
{
    m_DmaUtils = m_verifier->GetDmaUtils();
}

DumpUtils::~DumpUtils()
{
}

RC DumpUtils::DumpDmaBuffer(const char *filename, CheckDmaBuffer *pCheck, UINT08 *Data, UINT32 gpuSubdevice)
{
    // The dump_images_never option overrides all other image dumping options.
    if (m_params->ParamPresent("-dump_images") &&
        m_params->ParamUnsigned("-dump_images") == 1)
    {
        return OK;
    }

    const string& output_filename = m_verifier->OutFilename();
    size_t slash = output_filename.rfind(DIR_SEPARATOR_CHAR, output_filename.length());
    std::string fullFilename;
    if (slash == string::npos)
    {
        fullFilename = filename;
    }
    else
    {
        fullFilename = output_filename.substr(0, slash + 1);
        fullFilename += filename;
    }
    UINT32 chunkSize = 0;

    const ArgReader* params = m_verifier->Params();
    if (params->ParamPresent("-crc_chunk_size") > 0 &&
        params->ParamUnsigned("-crc_chunk_size") > 0)
    {
        chunkSize = params->ParamUnsigned("-crc_chunk_size");
    }

    SurfaceDumper* surfaceDumper = m_DmaUtils->GetSurfaceDumper();
    return surfaceDumper->DumpDmaBuf(fullFilename.c_str(), pCheck, Data,
        (params->ParamPresent("-no_tga_gz") == 0), gpuSubdevice, chunkSize);
}

RC DumpUtils::ReadAndDumpDmaBuffer(MdiagSurf *buffer, UINT32 size, string &name, UINT32 gpuSubdevice)
{
    // The dump_images_never option overrides all other image dumping options.
    if (m_params->ParamPresent("-dump_images") &&
        m_params->ParamUnsigned("-dump_images") == 1)
    {
        return OK;
    }

    RC rc = OK;

    if (m_DmaUtils->UseDmaTransfer() && !GetDmaReader() && !m_DmaUtils->SetupDmaBuffer())
    {
        MASSERT(!"Failed to setup DMA buffer for dumping\n");
        return RC::SOFTWARE_ERROR;
    }
    auto pDmaSurfReader = m_DmaUtils->UseDmaTransfer() ?
        SurfaceUtils::CreateDmaSurfaceReader(*buffer, GetDmaReader()) : nullptr;
    auto pMapSurfReader = SurfaceUtils::CreateMappingSurfaceReader(*buffer);
    SurfaceUtils::ISurfaceReader* pSurfReader = m_DmaUtils->UseDmaTransfer() ?
        static_cast<SurfaceUtils::ISurfaceReader*>(pDmaSurfReader.get()) :
        static_cast<SurfaceUtils::ISurfaceReader*>(pMapSurfReader.get());

    pSurfReader->SetTargetSubdevice(gpuSubdevice);

    vector<UINT08> tmpBuf(size);
    rc = pSurfReader->ReadBytes(0, &tmpBuf[0], size);
    if (OK != rc)
    {
        ErrPrintf("Failed to dump buffer %s\n", buffer->GetName().c_str());
        return rc;
    }
    InfoPrintf("Dumping %s with size %d\n", buffer->GetName().c_str(), size);

    FileIO* fileio = Sys::GetFileIO(name.c_str(), "wb");
    if(!fileio)
    {
        ErrPrintf("Could not open file %s for writing!\n", name.c_str());
        return RC::CANNOT_OPEN_FILE;
    }

    if (fileio->FileWrite(&tmpBuf[0], 1, size) != size)
    {
        ErrPrintf("Failed to write file %s\n", name.c_str());
        return RC::FILE_WRITE_ERROR;
    }
    fileio->FileClose();
    delete fileio;

    return OK;
}

RC DumpUtils::DumpSurfOrBuf(MdiagSurf *dma, string &fname, UINT32 gpuSubdevice)
{
    MASSERT(dma);

    RC rc = OK;

    // add gpu tag
    if (gpuSubdevice > 0)
    {
        string gputag = Utility::StrPrintf("_GPU%d", gpuSubdevice);

        size_t dotpos = fname.rfind(".");
        string::npos == dotpos ?
            fname += gputag : fname.insert(dotpos, gputag);
    }

    // dump all two-d surfaces that has raster in to image
    if (dma->GetWidth() > 0 && dma->GetHeight() > 0
        && m_verifier->SurfaceManager()->GetRaster(dma->GetColorFormat()) != 0)
    {
        fname += ".tga";

        InfoPrintf("Dumping %s.\n", fname.c_str());

        // -no_tga_gz will be handled inside DumpActiveSurface
        if (!DumpActiveSurface(dma, fname.c_str(), gpuSubdevice))
        {
            return RC::FILE_UNKNOWN_ERROR;
        }
    }
    // dump all raw buffers
    else
    {
        InfoPrintf("Dumping %s.\n", fname.c_str());

        // in case of overwriting trace buffer file
        fname = "_" + fname;

        if (!m_verifier->Params()->ParamPresent("-no_tga_gz"))
        {
            fname += ".gz";
        }
        CHECK_RC(ReadAndDumpDmaBuffer(dma, (UINT32)(dma->GetAllocSize() & 0xffffffff), fname, gpuSubdevice));
    }

    return rc;
}

RC DumpUtils::DumpCRCInfos() const
{
    RC rc = OK;
    FILE* fp = 0;

    string fname = m_verifier->Params()->ParamStr("-o", "test");
    fname += ".inf";
    rc = Utility::OpenFile(fname.c_str(), &fp, "a");

    if (OK != rc)
        return rc;

    foreach_all_surfaces(surfIt)
    {
        CRCINFO2& info = (*surfIt)->GetCRCInfo();

        fprintf(fp, "%s %s 0x%08x  \n",
                info.Key.c_str(),
                info.Prof.c_str(),
                info.CRCValue);

        if(!info.CRCDumpIMGFilename.empty())
            fprintf(fp, "%s\n", info.CRCDumpIMGFilename.c_str());

        if(!info.SelfGildInfo.empty())
            fprintf(fp, "%s\n", info.SelfGildInfo.c_str());
    }
    fclose(fp);

    return rc;
}

void DumpUtils::DumpCRCsPostTest()
{
    // Bug 888134
    //
    // Remove the suffix added by ModifySameNames because
    // we want all CRCs to be dumped into a single file.
    // Keeping ModifySameNames as is because image dumping
    // still depends on it to generate diff. file names for
    // different channels.
    //
    string output_file(m_verifier->Params()->ParamStr("-o", "test"));
    output_file = UtilStrTranslate(output_file,
            DIR_SEPARATOR_CHAR2, DIR_SEPARATOR_CHAR);

    // check if we are given a _XX suffix
    string crcfile = TestStateReport::create_status_filename(
            ".crc", output_file.c_str());

    ICrcProfile* profile = m_verifier->Profile();
    profile->SetFileName(crcfile);
    profile->SetDump(true);
}

RC DumpUtils::DumpRawImages(UINT08* buffer, UINT32 size, UINT32 subdev, const string& output_filename)
{
    string rawFilename(output_filename);
    size_t period = rawFilename.find('.', output_filename.length());
    string rawTrimmed;
    if (period == string::npos)
    {
        rawTrimmed += ".subdev";
        rawTrimmed += ('0' + subdev);
        rawTrimmed = rawFilename + ".raw";
    }
    else
    {
        rawTrimmed = rawFilename.substr(0, period);
        rawTrimmed += ".subdev";
        rawTrimmed += ('0' + subdev);
        rawTrimmed += ".raw";
    }

    FileIO* fh = Sys::GetFileIO(rawTrimmed.c_str(), "wb");
    if (!fh)
    {
        ErrPrintf("Could not open file %s\n", rawTrimmed.c_str());
        return RC::CANNOT_OPEN_FILE;
    }

    if (size != fh->FileWrite(buffer, sizeof(UINT08), size))
    {
        ErrPrintf("Could not write file %s\n", rawTrimmed.c_str());
        return RC::FILE_WRITE_ERROR;
    }
    fh->FileClose();
    Sys::ReleaseFileIO(fh);
    fh = NULL;

    return OK;
}

// ---------------------------------------------------------------------------------
//  dump and update crcs
// ---------------------------------------------------------------------------------
bool DumpUtils::DumpUpdateCrcs
(
    TestEnums::TEST_STATUS& status,
    ITestStateReport* report,
    const string& tgaProfileKey,
    UINT32        gpuSubdevice
)
{
    CrcEnums::CRC_MODE crcMode = m_verifier->CrcMode();

    InfoPrintf("-crcDump/-crlwpdate section\n");
    bool skip_images = 0;
    ICrcProfile* profile = m_verifier->Profile();
    ICrcProfile* gold_profile = m_verifier->GoldProfile();

    foreach_all_surfaces(surfIt)
    {
        (*surfIt)->DumpCRC(status); //TODO: error checking
    }

    ProfileKey::CleanUpProfile(m_verifier, gpuSubdevice);
    const char* crc_filename = profile->GetFileName().c_str();
    const char* gold_filename = gold_profile->GetFileName().c_str();
    profile->SetDump(true);

    const ArgReader* params = m_verifier->Params();
    VerifUtils::optionalCreateDir(crc_filename, params);
    if (crcMode == CrcEnums::CRC_UPDATE)
        VerifUtils::p4Action("edit", crc_filename);

    skip_images = !DumpUpdateIntrs(report);

    // if the profile is new, there was no error saving the profile to file,
    // and in Update mode then we need to do a p4 add for this new file
    // so it goes into perforce for us
    if(!profile->Loaded() && (skip_images == false)) {
        VerifUtils::optionalCreateDir(crc_filename, params);
        if(crcMode == CrcEnums::CRC_UPDATE)
            VerifUtils::p4Action("add", crc_filename);
    }

    // If we do not have an existing golden.crc file, lets create one!
    // NOTE: this code should really be part of the CrcProfile class
    if(!gold_profile->Loaded())
    {
        VerifUtils::optionalCreateDir(gold_filename, params);

        CrcChain& crcChain = m_verifier->CrcChain_Manager()->GetCrcChain();
        if(VerifUtils::CreateGoldCrc(gold_filename, crcChain.begin()->c_str()))
        {
            if(crcMode == CrcEnums::CRC_UPDATE)
                VerifUtils::p4Action("add",gold_filename);
        }
        else
        {
            ErrPrintf("Could not create gold crc file %s\n", gold_filename);
        }
    }

    bool ret = true;
    if(!skip_images)
    {
        int index = 0;
        foreach_all_surfaces(surfIt)
        {
            ret &= (*surfIt)->UpdateCRC(tgaProfileKey, gpuSubdevice, index);

            // only DmaBuffers need the index
            if ((*surfIt)->GetInfoType() == SIT_DmaBuffer)
                ++ index;
        }
    }

    return ret;
}

bool DumpUtils::DumpImages(const string& tgaProfileKey, const UINT32 gpuSubdevice)
{
    StickyRC src;

    // dump color surfaces, if not dumped yet
    int index = 0;
    foreach_all_surfaces(surfIt)
    {
        src = (*surfIt)->DumpImage(tgaProfileKey, gpuSubdevice, index);

        // only DmaBuffers need indice
        if ((*surfIt)->GetInfoType() == SIT_DmaBuffer)
            ++ index;
    }

    return OK == src;
}

bool DumpUtils::DumpActiveSurface(MdiagSurf* surface, const char* fname , UINT32 gpuSubdevice)
{
    MASSERT( surface );

    if (m_verifier->RawImageMode() != RAWOFF)
        WarnPrintf("dumpActiveSurface: no raw image supported\n");

    // Generate a key;
    string tgaProfileKey("");
    ProfileKey::Generate(m_verifier, NULL, NULL, 0, 0,
            gpuSubdevice, tgaProfileKey);

    char filename[512];
    strncpy( filename, fname, 508 );
    if( !m_verifier->Params()->ParamPresent("-no_tga_gz") )
    {
        strcat( filename, ".gz" );
    }

    m_DmaUtils->GetSurfaceDumper()->DumpSurface(
        filename, surface, 0, g_NullRectangle, tgaProfileKey.c_str(), gpuSubdevice, false);
    return true;
}

bool DumpUtils::DumpUpdateIntrs(ITestStateReport* report)
{
    bool bRet = true;
    CrcEnums::CRC_MODE crcMode = m_verifier->CrcMode();
    const ArgReader* params = m_verifier->Params();

    UINT32 grIdx = ErrorLogger::ILWALID_GRIDX;
    ErrorLogger::CheckGrIdxMode checkGrIdx = ErrorLogger::None;
    if (m_verifier->LWGpu()->IsSMCEnabled())
    {
        if (params->ParamPresent("-compare_all_intr"))
        {
            checkGrIdx = ErrorLogger::None;
        }
        else if (params->ParamPresent("-compare_untagged_intr"))
        {
            checkGrIdx = ErrorLogger::IdxAndUntaggedMatch;
        }
        else
        {
            checkGrIdx = ErrorLogger::IdxMatch;
        }
    }
    // Send grIdx only if Smc Mode and channel is compute
    if (m_verifier->GetSmcEngine() && 
        m_verifier->Channel() && m_verifier->Channel()->GetComputeSubChannel())
    {
        grIdx = m_verifier->GetSmcEngine()->GetSysPipe();
    }
    
    if(ErrorLogger::HasErrors(checkGrIdx, grIdx))
    {
        const string ifile = GetIntrFileName();
        const char *crcIntrFile = ifile.c_str();

        FILE *intrFile = fopen(crcIntrFile, "r");
        bool didP4Action = false;

        // If the file exists, we'll have to p4Edit it.
        if(intrFile)
        {
            fclose(intrFile);
            if(crcMode == CrcEnums::CRC_UPDATE)
            {
                VerifUtils::p4Action("edit", crcIntrFile);
                didP4Action = true;
            }
        }

        if(OK == ErrorLogger::WriteFile(crcIntrFile, checkGrIdx, grIdx))
        {
            // If we're supposed to update the expected error log file and
            // we haven't already done a p4 edit, the file must be new
            if((crcMode == CrcEnums::CRC_UPDATE) && !didP4Action)
                VerifUtils::p4Action("add", crcIntrFile);
        }
        else
        {
            ErrPrintf("Could not save HW engine errors log to %s\n", crcIntrFile);
            ErrPrintf("Not dumping images either \n") ;
            report->error("Could not save crcs!\n");
            bRet = false;
        }
    }

    return bRet;
}

DmaReader* DumpUtils::GetDmaReader()
{
    MASSERT(m_DmaUtils);
    return m_DmaUtils->GetSurfaceReader()->GetDmaReader();
}

const string DumpUtils::GetIntrFileName() const
{
    ICrcProfile* profile = m_verifier->Profile();
    string IntrName(profile->GetFileName());

    if( m_verifier->Params()->ParamPresent("-interrupt_file") )
    {
        // Use file name specified in the command line
        string f_name = m_verifier->Params()->ParamStr("-interrupt_file");
        string::size_type dir_sep = IntrName.rfind( DIR_SEPARATOR_CHAR );
        if( dir_sep == string::npos )
        {
            return f_name;
        }
        else
        {
            IntrName.erase(dir_sep+1, string::npos);
            IntrName += f_name;
        }
    }
    else
    {
        // replace .crc with .int
        IntrName.replace( IntrName.length()-3, 3, "int");
    }

    return IntrName;
}

