/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "hulkloader.h"
#include "gpu/utility/falconucode.h"
#include "gpu/utility/falconimpl.h"

void LogResult(UINT32 progress, UINT32 errorCode, bool ignoreErrors) noexcept;
vector<UINT32> ArrayToVector(unsigned char const* pArray, size_t pArraySize) noexcept;
bool WasLoadSuccessful(UINT32 progress, UINT32 errorCode) noexcept;

RC HulkLoader::LoadHulkLicence
(
    const vector<UINT32>& hulkLicence,
    bool ignoreErrors
)
{
    RC rc;
    CHECK_RC(DoSetup());

    unsigned char const* hulkFalconByteArray;
    size_t hulkFalconByteArraySize;
    GetHulkFalconBinaryArray(&hulkFalconByteArray, &hulkFalconByteArraySize);
    vector<UINT32> hulkFalconImemBinary;
    vector<UINT32> hulkFalconDmemBinary;
    FalconUCode::UCodeInfo hulkFalconUcodeInfo;
    // Extract the IMEM, DMEM sections and associated metadata
    CHECK_RC(FalconUCode::ParseBinary(ArrayToVector(hulkFalconByteArray, hulkFalconByteArraySize),
        &hulkFalconImemBinary,
        &hulkFalconDmemBinary,
        &hulkFalconUcodeInfo,
        FalconUCode::UCodeDescType::GFW_Vx));

    // Get ucode interface info from the DMEM Mapper
    size_t hulkLicenceSize = hulkLicence.size();
    FalconUCode::DmemMapper* pHulkFalconDmemMapper;
    CHECK_RC(FalconUCode::GetDmemMapper(hulkFalconUcodeInfo,
        &hulkFalconDmemBinary,
        &pHulkFalconDmemMapper));
    if (hulkLicenceSize > pHulkFalconDmemMapper->imgDataBufSize)
    {
        Printf(Tee::PriNormal, "Hulk size %lu greater than data buffer size %u\n",
            hulkLicenceSize, pHulkFalconDmemMapper->imgDataBufSize);
        return RC::SOFTWARE_ERROR;
    }

    // See https://rmopengrok.lwpu.com/source/xref/sw/rel/gfw_ucode/r3/v1/src/inc/ucode_interface.h
    static constexpr UINT32 HULK_CMD = 0x12;
    pHulkFalconDmemMapper->initCmd = HULK_CMD;

    // Load the program, HULK certificate and kick off the binary
    unique_ptr<FalconImpl> falcon = GetFalcon();
    CHECK_RC(falcon->ShutdownUCode());
    CHECK_RC(falcon->Reset());
    CHECK_RC(falcon->LoadProgram(hulkFalconImemBinary, hulkFalconDmemBinary, hulkFalconUcodeInfo));
    CHECK_RC(falcon->WriteDMem(hulkLicence, pHulkFalconDmemMapper->imgDataBufOffset));
    CHECK_RC(falcon->Start(true));

    UINT32 progress;
    UINT32 errorCode;
    GetLoadStatus(&progress, &errorCode);
    LogResult(progress, errorCode, ignoreErrors);
    if (!WasLoadSuccessful(progress,errorCode) && !ignoreErrors)
    {
        return RC::SOFTWARE_ERROR;
    }
    CHECK_RC(falcon->Reset());
    return rc;
}

void LogResult(UINT32 progress, UINT32 errorCode, bool ignoreErrors) noexcept
{
    if (WasLoadSuccessful(progress, errorCode))
    {
        Printf(Tee::PriLow, "HULK processed successfully\n");
    }
    else if (ignoreErrors)
    {
        Printf(Tee::PriWarn, "HULK processing failed. Ignoring\n");
    }
    else
    {
        Printf(Tee::PriError, "HULK processing failed. Progress = 0x%x, Error Code = 0x%x\n",
            progress, errorCode);
    }
}

vector<UINT32> ArrayToVector(unsigned char const* pArray, size_t pArraySize) noexcept
{
    vector<UINT32> result;
    UINT32 const* pStart = reinterpret_cast<UINT32 const*>(pArray);
    size_t binarySize = pArraySize / sizeof(UINT32);
    result.assign(pStart, pStart + binarySize);
    return result;

}
bool WasLoadSuccessful(UINT32 progress, UINT32 errorCode) noexcept
{
    //LW_UCODE_POST_CODE_HULK_PROG_COMPLETE is copied from here:
    //https://rmopengrok.lwpu.com/source/xref/sw/main/bios/common/cert20/ucode_postcodes.h#546
    static constexpr UINT32 LW_UCODE_POST_CODE_HULK_PROG_COMPLETE = 0x5;
    return errorCode == 0 && progress == LW_UCODE_POST_CODE_HULK_PROG_COMPLETE;
}