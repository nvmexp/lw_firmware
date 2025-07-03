/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2014, 2020 by LWPU Corporation.  All rights reserved.  
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file
//! \brief Data classes for various CRC operations.

#ifndef INCLUDED_SURFBUFINFO_H
#define INCLUDED_SURFBUFINFO_H

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif

#ifndef INCLUDED_TYPES_H
#include "mdiag/utils/types.h"
#endif

#ifndef INCLUDED_SURF2D_H
#include "gpu/utility/surf2d.h"
#endif

#ifndef INCLUDED_STDIO_H
#include "stdio.h"
#endif

#include "mdiag/resource/lwgpu/surfasm.h"
#include "mdiag/resource/lwgpu/SurfaceReader.h"
#include "VerifUtils.h"

class GpuVerif;
class DmaUtils;
class DumpUtils;
class SurfaceReader;
class SurfaceDumper;
class CrcBaseChecker;
class ICrcVerifier;

struct CRCINFO2
{
    string Key;
    string Prof;
    string CRCDumpIMGFilename;
    string SelfGildInfo;
    UINT32 CRCValue;
    UINT32 gpuSubdevice;

    CRCINFO2() {CRCValue = 0; gpuSubdevice = 0;}
    void Save(const string& key, const string& prof, UINT32 value, UINT32 subdev);
};

enum SurfInfoType
{
    SIT_Surface_C,
    SIT_Surface_Z,
    SIT_AllocSurface,
    SIT_DmaBuffer,
    NUM_SurfInfoType
};

class SurfaceInfo
{
public:
    SurfaceInfo(MdiagSurf* surface, GpuVerif* gpuVerif);
    virtual ~SurfaceInfo() {}

    virtual void PrintCRC() const = 0;
    virtual bool GpuComputeCRC(UINT32 subdev) = 0;
    virtual bool CpuComputeCRC(UINT32 subdev) = 0;
    virtual bool SaveMeasuredCRC(const char* key, UINT32 subdev) = 0;
    virtual bool UpdateCRC(const string& tgaProfileKey, UINT32 subdev, UINT32 index) = 0;
    virtual bool CompareCRC(const char* key, UINT32 subdev, ITestStateReport* report, CrcStatus& crcStatus) = 0;

    virtual const CheckDmaBuffer* GetCheckInfo() const = 0;
    virtual SurfInfoType GetInfoType() const = 0;

    virtual void DumpCRC(TestEnums::TEST_STATUS& status);
    virtual RC DumpImage(const string &tgaProfileKey, UINT32 subdev, UINT32 index) = 0;

    virtual void BuildImgFilename() = 0;
    virtual bool BuildProfKeyImgName(const char* key, UINT32 subdev) = 0;

    virtual bool GenProfileKey(
            const char*    suffix,
            bool           search,
            UINT32         subdev,
            std::string&   profileKey,
            bool           ignoreLocalLead = true
    ) = 0;

    virtual string GetSurfaceName() const = 0;

    virtual bool MustMatchCrc() const = 0;
    virtual bool IsValid() const { return true; }
    virtual bool IsColorOrZ() const
    {
        return GetInfoType() == SIT_Surface_C ||
               GetInfoType() == SIT_Surface_Z;
    }

    // Getters
    MdiagSurf* GetSurface() { return m_surface; }
    CRCINFO2&  GetCRCInfo() { return m_crcInfo; }
    string&    GetProfKey() { return m_profkey; }
    UINT08*    GetSurfBase() const { return m_surfbase; }
    UINT32     GetCRC()      const { return m_crc;      }

protected:
    bool IsChunked() const;
    ICrcVerifier* GetChecker() const;

    RC DumpRawMemory(MdiagSurf* surface, UINT64 offset, UINT32 size);

    MdiagSurf*        m_surface;
    string            m_profkey;
    CRCINFO2          m_crcInfo;
    UINT08*           m_surfbase;
    UINT32            m_crc;

    GpuVerif*        m_GpuVerif;
    const ArgReader*  m_params;
    DmaUtils*         m_dmaUtils;
    DumpUtils*        m_dumpUtils;
    SurfaceReader*    m_surfaceReader;
    SurfaceDumper*    m_surfaceDumper;

    ICrcVerifier*     m_crcChecker;
    ICrcVerifier*     m_refChecker;
};

class SurfInfo2 : public SurfaceInfo
{
    friend class GpuVerif; // initialize members
    friend class ProfileKey; // Keygen
    friend class DumpUtils;

public:
    SurfInfo2(MdiagSurf* surf, GpuVerif* gpuVerif, int idx = -1);

    // new interfaces
    virtual void PrintCRC() const;

    virtual bool GpuComputeCRC(UINT32 subdev);
    virtual bool CpuComputeCRC(UINT32 subdev);
    virtual bool SaveMeasuredCRC(const char* key, UINT32 subdev);
    virtual bool UpdateCRC(const string& tgaProfileKey, UINT32 subdev, UINT32 index);
    virtual bool CompareCRC(const char* key, UINT32 subdev, ITestStateReport* report, CrcStatus& crcStatus);

    virtual bool IsValid() const;
    virtual bool IsColorOrZ() const { return true; }
    virtual bool MustMatchCrc() const { return true; }

    virtual bool BuildProfKeyImgName(const char* key, UINT32 subdev);
    virtual void BuildImgFilename();

    virtual const CheckDmaBuffer* GetCheckInfo() const { return NULL; }
    virtual SurfInfoType GetInfoType() const
    {
        return IsZBuffer() ? SIT_Surface_Z : SIT_Surface_C;
    };

    virtual void DumpSurface(UINT32 subdev, const string& tgaProfileKey);
    bool ZLwllSanityCheck(UINT32 subdev, bool* zlwll_passed);

    // legacy interfaces
    virtual string CreateInitialKey(const string& suffix);

    virtual bool Verify(CrcStatus *crcStatus, const char *key,
        ITestStateReport *report, UINT32 gpuSubdevice);

    virtual RC DumpImage(const string &tgaProfileKey, UINT32 subdev, UINT32 index);
    virtual RC DumpImageToFile(const string& filename, const string &profileKey, UINT32 gpuSubdevice);

    virtual bool GenProfileKey(
        const char*    suffix,
        bool           search,
        UINT32         subdev,
        std::string&   profileKey,
        bool           ignoreLocalLead = true
    );

    virtual string GetImageNameSuffix(const string& extraSuffix);

    // glue getters
    const string& GetFilename() const { return filename; }
    const string& GetImageName() const { return imgname; }
    UINT32 ColorIndex() const { return index; }

    virtual string GetSurfaceName() const;
    SurfaceAssembler::Rectangle* GetRect() { return &rect; }

protected:
    bool IsZBuffer() const { return index == -1; }

    bool DoGpuComputeCRC(UINT32 subdev);
    bool DoCpuComputeCRC(UINT32 subdev);
    bool DoCpuComputeCRCByChunk(UINT32 subdev);
    bool DoCpuComputeCRCByChunkForPitch(UINT32 subdev);

protected:
    vector<UINT08>    surfdata;
    string            filename;
    string            imgname;
    bool              tgadumped;
    int               index;
    bool              rawCrc;
    SurfaceAssembler::Rectangle rect;
};

class AllocSurfaceInfo2 : public SurfInfo2
{
    friend class GpuVerif; // initialize members
    friend class ProfileKey; // Keygen
    friend class DumpUtils;

public:
    AllocSurfaceInfo2(MdiagSurf* surface, GpuVerif* gpuVerif);

    // new interfaces
    virtual void DumpSurface(UINT32 subdev, const string& tgaProfileKey);
    virtual bool SaveMeasuredCRC(const char* key, UINT32 subdev);
    virtual bool UpdateCRC(const string& tgaProfileKey, UINT32 subdev, UINT32 index);
    virtual bool CompareCRC(const char* key, UINT32 subdev, ITestStateReport* report, CrcStatus& crcStatus);
    virtual bool IsValid() const { return true; } // always valid

    virtual bool BuildProfKeyImgName(const char* key, UINT32 subdev);
    bool BuildProfKeyImgNameSuffix(const char* key, const string& suffix, UINT32 subdev);

    virtual void BuildImgFilename();
    void BuildImgFilenameSuffix(const string& suffix);

    virtual SurfInfoType GetInfoType() const { return SIT_AllocSurface; };

    // legacy interfaces
    virtual string CreateInitialKey(const string& suffix);

    virtual bool GenProfileKey(
        const char*    suffix,
        bool           search,
        UINT32         subdev,
        std::string&   profileKey,
        bool           ignoreLocalLead = true
    );

    virtual RC DumpImage(const string &tgaProfileKey, UINT32 subdev, UINT32 index);
    virtual RC DumpImageToFile(const string& filename, const string &profileKey, UINT32 gpuSubdevice);

    virtual string GetSurfaceName() const { return m_CheckInfo.Filename; }
    virtual bool MustMatchCrc() const;

    virtual string GetImageNameSuffix(const string& extraSuffix);

    const string& CrcName() const { return m_CheckInfo.Filename; }

protected:
    virtual const CheckDmaBuffer* GetCheckInfo() const { return &m_CheckInfo; }

    CheckDmaBuffer m_CheckInfo;
    bool m_UseTga;
};

class DmaBufferInfo2 : public SurfaceInfo
{
    friend class GpuVerif; // initialize members
    friend class ProfileKey; // Keygen
    friend class DumpUtils;

public:
    DmaBufferInfo2(MdiagSurf *surface, GpuVerif* gpuVerif);
    virtual ~DmaBufferInfo2();

    virtual void PrintCRC() const;
    virtual bool GpuComputeCRC(UINT32 subdev);
    virtual bool CpuComputeCRC(UINT32 subdev);
    virtual bool SaveMeasuredCRC(const char* key, UINT32 subdev);
    virtual bool UpdateCRC(const string& tgaProfileKey, UINT32 subdev, UINT32 index);
    virtual bool CompareCRC(const char* key, UINT32 subdev, ITestStateReport* report, CrcStatus& crcStatus);

    virtual RC DumpImage(const string &tgaProfileKey, UINT32 subdev, UINT32 index);

    virtual SurfInfoType GetInfoType() const { return SIT_DmaBuffer; };
    virtual string GetSurfaceName() const;
    virtual bool MustMatchCrc() const;

    virtual void BuildImgFilename();
    virtual bool BuildProfKeyImgName(const char* key, UINT32 subdev);

    virtual bool GenProfileKey(
        const char*    suffix,
        bool           search,
        UINT32         subdev,
        std::string&   profileKey,
        bool           ignoreLocalLead = true
    );

protected:
    virtual const CheckDmaBuffer* GetCheckInfo() const { return &DmaBuffers; }

private:
    string          imgNameDmaBuf;
    UINT08*         dmabuf_base;
    string          dmabuf_name;
    bool            dmabuf_dumped;
    CheckDmaBuffer  DmaBuffers;
};

typedef vector<SurfaceInfo*> SurfaceInfos;
typedef vector<SurfInfo2*> SurfVec2_t;
typedef vector<DmaBufferInfo2*> DmaBufVec2_t;
typedef vector<AllocSurfaceInfo2*> AllocSurfaceVec2_t;

typedef SurfVec2_t::const_iterator SurfIt2_t;
typedef DmaBufVec2_t::const_iterator DmaBufIt2_t;

#endif /* INCLUDED_SURFBUFINFO_H */
