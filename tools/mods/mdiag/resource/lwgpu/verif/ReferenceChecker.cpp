/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <string>
using std::string;

#include "gpu/utility/surf2d.h"
#include "mdiag/sysspec.h"
#include "mdiag/utils/profile.h"
#include "core/include/massert.h"
#include "core/include/platform.h"
#include "mdiag/utils/tracefil.h"
#include "mdiag/utils/crc.h"
#include "mdiag/utils/raster_types.h"
#include "mdiag/utils/CrcInfo.h"
#include "mdiag/tests/test_state_report.h"
#include "mdiag/tests/gpu/trace_3d/tracechan.h"

#include "mdiag/resource/lwgpu/surfasm.h"
#include "mdiag/resource/lwgpu/dmasurfr.h"
#include "mdiag/resource/lwgpu/sclnrdim.h"
#include "mdiag/resource/lwgpu/dmard.h"
#include "mdiag/resource/lwgpu/gpucrccalc.h"
#include "mdiag/resource/lwgpu/crcchain.h"
#include "mdiag/resource/lwgpu/SurfaceReader.h"
#include "mdiag/resource/lwgpu/gpucrccalc.h"

#include "GpuVerif.h"
#include "ProfileKey.h"
#include "VerifUtils.h"
#include "DumpUtils.h"
#include "ReferenceChecker.h"

void ReferenceChecker::Setup(GpuVerif* verifier)
{
    m_verifier = verifier;
    m_params = verifier->Params();
    m_PrintRefCompMismatchCnt = m_params->ParamUnsigned("-printrefmismatch", 100);
}

TestEnums::TEST_STATUS ReferenceChecker::Check(ICheckInfo* info)
{
    MASSERT(!"Not supported by Reference Checker");
    return TestEnums::TEST_FAILED_CRC;
}

// Read stored crcs and compare with measured crc. Return true if caller should dump image
bool ReferenceChecker::Verify
(
    const char *key, // Key used to find stored crcs
    UINT32 measuredCrc, // Measured crc
    SurfaceInfo* surfInfo,
    UINT32 gpuSubdevice,
    ITestStateReport *report, // File to touch in case of error
    CrcStatus *crcStatus // Result of comparison
)
{
    const CheckDmaBuffer* pCheck = surfInfo->GetCheckInfo();

    MASSERT(pCheck);

    m_verifier->IncrementCheckCounter();

    UINT32 crc = 0;
    vector<UINT08> refVec;
    RC rc = ReadCRC(pCheck, &crc, gpuSubdevice, &refVec);

    if (OK != rc)
    {
        ErrPrintf("Can not read reference buffer: %s\n", rc.Message());
        *crcStatus = CrcEnums::FAIL;
        return true;
    }

    InfoPrintf("%s: REFERENCE_CHECK %s: "
        "referece_crc = 0x%08x vs. computed_crc = 0x%08x\n",
        __FUNCTION__, pCheck->Filename.c_str(), crc, measuredCrc);

    if (crc == measuredCrc)
    {
        if (pCheck->CrcMatch)
        {
            InfoPrintf("CRC OK: %s CRC passed GOLD\n", pCheck->Filename.c_str());
            *crcStatus = CrcEnums::GOLD;
            return false;
        }
        else
        {
            ErrPrintf("CRC of %s matches, but mismatch is expected\n", pCheck->Filename.c_str());
            *crcStatus = CrcEnums::FAIL;
            return true;
        }
    }
    else
    {
        if (pCheck->CrcMatch)
        {
            ErrPrintf("CRC WRONG!! %s CRC failed, computed 0x%08x != 0x%08x\n",
                    pCheck->Filename.c_str(), measuredCrc, crc);
            *crcStatus = CrcEnums::FAIL;

            UINT08* pBufferBase = surfInfo->GetSurfBase();
            if (m_PrintRefCompMismatchCnt > 0)
            {
                InfoPrintf("REF_CHECK failed. Print the mismatch data.\n");
                if (0 == pBufferBase &&
                    (m_params->ParamUnsigned("-crc_chunk_size", 0) > 0 ||
                     m_params->ParamUnsigned("-dma_check") == 6))
                {
                    // If chunked CRC/REF checking is used to save runtime memory, mods
                    // doesn't hold the flat memory for dma buffer, so mods can't
                    // provide detail info
                    // For -crccheck_gpu, crc is callwlated by gpu and the surface
                    // to be checked will not be dumped to pBufferBase until surface
                    // dump phase. No plan to support detailed mismatch message here.
                    InfoPrintf("Unable to print the detail mismatch data "
                               "probably because chunked Reference checking "
                               "(typically for large surfaces) is enabled\n");
                    return true;
                }
                UINT32 linecn = m_PrintRefCompMismatchCnt;
                UINT64 file_size = pCheck->GetUntiledSize();
                UINT32 i = 0;

                MdiagSurf* surf = surfInfo->GetSurface();
                const UINT32* pBufferBase = reinterpret_cast<const UINT32 *>(surfInfo->GetSurfBase());
                const UINT32* pRefBufBase = reinterpret_cast<const UINT32 *>(&refVec[0]);

                MASSERT(surf != 0);
                UINT64 dmaBufGpuVA = surf->GetCtxDmaOffsetGpu() + surf->GetExtraAllocSize() + pCheck->Offset;

                while (linecn && (file_size/4))
                {
                    if (pBufferBase[i] != pRefBufBase[i])
                    {
                        if (pCheck->pBlockLinear)
                        {
                            UINT32 offset = pCheck->pBlockLinear->Address(i * 4);
                            UINT32 offsetInGob = pCheck->pBlockLinear->OffsetInGob(i * 4);
                            InfoPrintf("CrcMatch compare: expect match"
                                " <pitch offset> = 0x%x,"
                                " <blocklinear va> = 0x%08x_%08x,"
                                " <blocklinear offset> = 0x%x,"
                                " <offset in gob> = 0x%x,"
                                " <expected dword> = 0x%08x,"
                                " <actual dword> = 0x%08x\n",
                                i * 4,
                                (UINT32)((dmaBufGpuVA + offset) >> 32),
                                (UINT32)(dmaBufGpuVA + offset),
                                offset,
                                offsetInGob,
                                pRefBufBase[i],
                                pBufferBase[i]);
                        }
                        else
                        {
                            InfoPrintf("CrcMatch compare: expect match"
                                "<va> = 0x%08x_%08x, <offset> = 0x%x, <expected dword> = 0x%08x, <actual dword> = 0x%08x\n",
                                (UINT32)((dmaBufGpuVA + i * 4) >> 32), (UINT32)(dmaBufGpuVA + i * 4),
                                i * 4,
                                pRefBufBase[i],
                                pBufferBase[i]);
                        }

                        linecn --;
                    }

                    file_size -= 4;
                    i ++;
                }

                if (linecn && file_size)
                {
                    const UINT08* pBuf = reinterpret_cast<const UINT08*>(pBufferBase + i);
                    const UINT08* pRef = reinterpret_cast<const UINT08*>(pRefBufBase + i);
                    UINT32 buf = 0;
                    UINT32 ref = 0;
                    switch(file_size)
                    {
                    case 1:
                        buf = *pBuf;
                        ref = *pRef;
                        break;
                    case 2:
                        buf = *pBuf + (*(pBuf+1) << 8);
                        ref = *pRef + (*(pRef+1) << 8);
                        break;
                    case 3:
                        buf = *pBuf + (*(pBuf+1) << 8) + (*(pBuf+2) << 16);
                        ref = *pRef + (*(pRef+1) << 8) + (*(pRef+2) << 16);
                        break;
                    default:
                        MASSERT(!"Ilwalide file_size.");
                    }

                    if (buf != ref)
                    {
                        if (pCheck->pBlockLinear)
                        {
                            UINT32 offset = pCheck->pBlockLinear->Address(i * 4);
                            UINT32 offsetInGob = pCheck->pBlockLinear->OffsetInGob(i * 4);
                            InfoPrintf("CrcMatch compare: expect match"
                                " <pitch offset> = 0x%x,"
                                " <blocklinear va> = 0x%08x_%08x,"
                                " <blocklinear offset> = 0x%x,"
                                " <offset in gob> = 0x%x,"
                                " <expected dword> = 0x%08x,"
                                " <actual dword> = 0x%08x\n",
                                i * 4,
                                (UINT32)((dmaBufGpuVA + offset) >> 32),
                                (UINT32)(dmaBufGpuVA + offset),
                                offset,
                                offsetInGob,
                                pRefBufBase[i],
                                pBufferBase[i]);
                        }
                        else
                        {
                            InfoPrintf("CrcMatch compare: expect match"
                                "<va> = 0x%08x_%08x, <offset> = 0x%x, <expected dword> = 0x%08x, <actual dword> = 0x%08x\n",
                                (UINT32)((dmaBufGpuVA + i * 4) >> 32), (UINT32)(dmaBufGpuVA + i * 4),
                                i * 4,
                                ref,
                                buf);
                        }
                    }
                }
            }

            return true;
        }
        else
        {
            InfoPrintf("CRC OK: %s CRC passed GOLD\n", pCheck->Filename.c_str());
            *crcStatus = CrcEnums::GOLD;
            return false;
        }
    }
}

RC ReferenceChecker::ReadCRC(const CheckDmaBuffer *pCheck, UINT32* crc, UINT32 gpuSubdevice, vector<UINT08>* pRefVec) const
{
    MASSERT(pCheck);

    RC rc = OK;

    string buffer(m_verifier->Profile()->GetFileName());
    string::size_type dir = buffer.find_last_of("/\\");
    if (dir == string::npos)
        buffer = pCheck->Filename;
    else
        buffer.replace(dir+1, buffer.size() - dir-1, pCheck->Filename);

    TraceFileMgr *traceFileMgr = m_verifier->GetTraceFileMgr();
    TraceFileMgr::Handle handle = 0;
    CHECK_RC(traceFileMgr->Open(buffer, &handle));

    UINT64 file_size = pCheck->GetUntiledSize();

    vector<UINT08> tempVec(0);
    // We'll operate a vector object rather than pointer.
    vector<UINT08> &refVec = (pRefVec == NULL ? tempVec : *pRefVec);
    refVec.resize(file_size);

    // Padding the last 3 bytes with 0 in case the size is rounded.
    if (file_size >= 3)
    {
        refVec[file_size-3] = refVec[file_size-2] = refVec[file_size-1] = 0;
    }

    if (traceFileMgr->Read(&refVec[0], handle) != OK)
    {
        ErrPrintf("Can't read from file %s\n", buffer.c_str());
        traceFileMgr->Close(handle);
        return RC::FILE_READ_ERROR;
    }

    traceFileMgr->Close(handle);

    DebugPrintf("Computing crc for reference buffer: %s\n", buffer.c_str());

    if (pCheck->pBlockLinear)
    {
        UINT08* bl_buffer = new UINT08[pCheck->pBlockLinear->Size()];
        if (!bl_buffer)
        {
            return RC::CANNOT_ALLOCATE_MEMORY;
        }
        fill(bl_buffer, bl_buffer+pCheck->pBlockLinear->Size(), 0);

        for (UINT32 j = 0; j < pCheck->Height; ++j)
            for (UINT32 i = 0; i < pCheck->Width; ++i)
                bl_buffer[pCheck->pBlockLinear->Address(j*pCheck->Width+i)] = refVec[j*pCheck->Width+i];

        UINT32 kind = pCheck->pDmaBuf->GetPteKind();
        PteKindMask pteMask = VerifUtils::GetPtekindMask(kind, gpuSubdevice);
        for (UINT32 i = 0, *buff_32=(UINT32*)bl_buffer; i < pCheck->pBlockLinear->Size()/sizeof(UINT32); i+= 2)
        {
            UINT32 mask1 = VerifUtils::GetOffsetMask(i*sizeof(UINT32), &pteMask);
            UINT32 mask2 = VerifUtils::GetOffsetMask((i+1)*sizeof(UINT32), &pteMask);
            *(buff_32 + i) &= mask1;
            *(buff_32 + i+1) &= mask2;
        }
        for (UINT32 j = 0; j < pCheck->Height; ++j)
            for (UINT32 i = 0; i < pCheck->Width; ++i)
                refVec[j*pCheck->Width+i] = bl_buffer[pCheck->pBlockLinear->Address(j*pCheck->Width+i)];

        delete[] bl_buffer;
    }

    *crc = BufCrcDim32( reinterpret_cast<uintptr_t>(&refVec[0]), 0, UINT32(file_size), 1, 1);

    return OK;
}
