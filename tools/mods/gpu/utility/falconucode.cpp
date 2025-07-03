/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "falconimpl.h"

#include "core/include/fileholder.h"
#include "core/include/platform.h"
#include "core/include/utility.h"

//-----------------------------------------------------------------------------
RC FalconUCode::ReadBinary(const string& filename, vector<UINT32>* uCodeBinary)
{
    RC rc;
    MASSERT(uCodeBinary);

    FileHolder file;
    long fileSize;

    CHECK_RC(file.Open(filename, "rb"));
    CHECK_RC(Utility::FileSize(file.GetFile(), &fileSize));

    uCodeBinary->resize((fileSize + 3) / sizeof(UINT32), 0);

    size_t length = fread(&uCodeBinary->front(), 1, fileSize, file.GetFile());

    if (length != (size_t)fileSize || length == 0)
    {
        Printf(Tee::PriError, "Failed to read fuse binary.\n");
        return RC::FILE_READ_ERROR;
    }
    return rc;
}

RC FalconUCode::ParseBinary
(
    const vector<UINT32>& uCodeBinary,
    vector<UINT32> *pImemBinary,
    vector<UINT32> *pDmemBinary,
    UCodeInfo *pUcodeInfo,
    UCodeDescType descType
)
{
    RC rc;

    switch (descType)
    {
        case UCodeDescType::LEGACY_V1:
            CHECK_RC(ParseBinaryLegacyV1(uCodeBinary, pImemBinary, pDmemBinary, pUcodeInfo));
            break;
        case UCodeDescType::LEGACY_V2:
            CHECK_RC(ParseBinaryLegacyV2(uCodeBinary, pImemBinary, pDmemBinary, pUcodeInfo));
            break;
        case UCodeDescType::GFW_Vx:
            CHECK_RC(ParseBinaryGfwVx(uCodeBinary, pImemBinary, pDmemBinary, pUcodeInfo));
            break;
        default:
            Printf(Tee::PriError, "Ucode desc Type %u not supported\n",
                                   static_cast<UINT32>(descType));
            return RC::UNSUPPORTED_FUNCTION;
    }

    return rc;
}

RC FalconUCode::ParseBinaryLegacyV1
(
    const vector<UINT32>& uCodeBinary,
    vector<UINT32> *pImemBinary,
    vector<UINT32> *pDmemBinary,
    UCodeInfo *pUcodeInfo
)
{
    MASSERT(pImemBinary);
    MASSERT(pDmemBinary);
    MASSERT(pUcodeInfo);

    enum HeaderOffsets
    {
        VERSION     = 0,
        HEADER_SIZE = 1,
        IMEM_SIZE   = 2,
        DMEM_SIZE   = 3,
        SEC_START   = 4,
        SEC_END     = 5
    };

    UINT32 version       = uCodeBinary[VERSION];
    UINT32 headerSize    = uCodeBinary[HEADER_SIZE];
    UINT32 imemSize      = uCodeBinary[IMEM_SIZE];
    UINT32 dmemSize      = uCodeBinary[DMEM_SIZE];
    pUcodeInfo->secStart = uCodeBinary[SEC_START];
    pUcodeInfo->secEnd   = uCodeBinary[SEC_END];

    // Brief sanity on the header to make sure it's got the expected data
    // Version comes from //sw/rel/gfw_ucode/r3/v1/bldsys/gelwbiosfalcexports.pl
    // (ported from //src/sw/main/apps/lwflash/pmu/tools/genpmuexports.pl)
    if (version != 1)
    {
        Printf(Tee::PriError, "Ucode header version %u doesn't match with ucode desc type\n",
                               version);
        return RC::CANNOT_PARSE_FILE;
    }
    // Since the binary can pick up a few bytes from encryption or from not
    // being a multiple of 4 bytes, we can't use an equality comparison.
    if (headerSize + imemSize + dmemSize > uCodeBinary.size() * 4)
    {
        Printf(Tee::PriError, "Malformed or unencrypted binary!\n");
        return RC::CANNOT_PARSE_FILE;
    }

    vector<UINT32>::const_iterator start     = uCodeBinary.begin();
    vector<UINT32>::const_iterator imemStart = start + headerSize / 4;
    vector<UINT32>::const_iterator dmemStart = imemStart + imemSize / 4;
    vector<UINT32>::const_iterator end       = dmemStart + dmemSize / 4;

    Printf(Tee::PriLow, "Sizes (Bytes): IMEM: 0x%x; DMEM: 0x%x\n", imemSize, dmemSize);

    pImemBinary->clear();
    pImemBinary->assign(imemStart, dmemStart);
    pDmemBinary->clear();
    pDmemBinary->assign(dmemStart, end);

    // Older fuse read/write binaries use secure start and secure end values
    // from the DMEM to mark the secure regions
    // Slip in the selwre_start/end values just after the keys,
    // which are already in the binary
    // Each key is 4 dwords long
    UINT32 keySize = 2 * 4;
    (*pDmemBinary)[keySize]     = pUcodeInfo->secStart;
    (*pDmemBinary)[keySize + 1] = pUcodeInfo->secEnd;

    return RC::OK;
}

RC FalconUCode::ParseBinaryLegacyV2
(
    const vector<UINT32>& uCodeBinary,
    vector<UINT32> *pImemBinary,
    vector<UINT32> *pDmemBinary,
    UCodeInfo *pUcodeInfo
)
{
    MASSERT(pImemBinary);
    MASSERT(pDmemBinary);
    MASSERT(pUcodeInfo);

    enum HeaderOffsets
    {
        VERSION     = 0,
        HEADER_SIZE = 1,
        IMEM_SIZE   = 2,
        DMEM_SIZE   = 3,
        SEC_START   = 4,
        SEC_SIZE    = 6
    };

    UINT32 version       = uCodeBinary[VERSION];
    UINT32 headerSize    = uCodeBinary[HEADER_SIZE];
    UINT32 imemSize      = uCodeBinary[IMEM_SIZE];
    UINT32 dmemSize      = uCodeBinary[DMEM_SIZE];
    pUcodeInfo->secStart = uCodeBinary[SEC_START];
    pUcodeInfo->secEnd   = pUcodeInfo->secStart + uCodeBinary[SEC_SIZE] - 1;

    // Brief sanity on the header to make sure it's got the expected data
    // Version comes from //sw/rel/gfw_ucode/r3/v1/bldsys/gelwbiosfalcexports.pl
    // (ported from //src/sw/main/apps/lwflash/pmu/tools/genpmuexports.pl)
    if (version != 2)
    {
        Printf(Tee::PriError, "Ucode header version %u doesn't match with ucode desc type\n",
                               version);
        return RC::CANNOT_PARSE_FILE;
    }
    // Since the binary can pick up a few bytes from encryption or from not
    // being a multiple of 4 bytes, we can't use an equality comparison.
    if (headerSize + imemSize + dmemSize > uCodeBinary.size() * 4)
    {
        Printf(Tee::PriError, "Malformed or unencrypted binary!\n");
        return RC::CANNOT_PARSE_FILE;
    }

    vector<UINT32>::const_iterator start     = uCodeBinary.begin();
    vector<UINT32>::const_iterator imemStart = start + headerSize / 4;
    vector<UINT32>::const_iterator dmemStart = imemStart + imemSize / 4;
    vector<UINT32>::const_iterator end       = dmemStart + dmemSize / 4;

    Printf(Tee::PriLow, "Sizes (Bytes): IMEM: 0x%x; DMEM: 0x%x\n", imemSize, dmemSize);

    pImemBinary->clear();
    pImemBinary->assign(imemStart, dmemStart);
    pDmemBinary->clear();
    pDmemBinary->assign(dmemStart, end);

    return RC::OK;
}

RC FalconUCode::ParseBinaryGfwVx
(
    const vector<UINT32>& uCodeBinary,
    vector<UINT32> *pImemBinary,
    vector<UINT32> *pDmemBinary,
    UCodeInfo *pUcodeInfo
)
{
    RC rc;
    UCodeHeaderInfo header;
    ParseBinaryGfwHeader(uCodeBinary, &header);
    switch (header.version)
    {
        case 2:
            CHECK_RC(ParseBinaryGfwV2(uCodeBinary, header, pImemBinary, pDmemBinary, pUcodeInfo));
            break;
        case 3:
            CHECK_RC(ParseBinaryGfwV3(uCodeBinary, header, pImemBinary, pDmemBinary, pUcodeInfo));
            break;
        default:
            Printf(Tee::PriError, "Ucode header version %u not supported\n",
                                   static_cast<UINT32>(header.version));
            return RC::UNSUPPORTED_FUNCTION;
    }
    return rc;
}

void FalconUCode::ParseBinaryGfwHeader
(
    const vector<UINT32>& uCodeBinary,
    UCodeHeaderInfo *pHeader
)
{
    // See https://wiki.lwpu.com/engwiki/index.php/VBIOS/Data_Structures/Falcon_Ucode/Falcon_Ucode_Descriptor/Spec_V2
    // Header format :
    // 0:7   - Flags with [0:0] version indicator. If 0x0, parse structure as descriptor V1
    //                                             If 0x1, version given by next 8 bits
    // 8:15  - Descriptor version
    // 16:31 - Structure size
    UINT32 header     = uCodeBinary[0];
    pHeader->flags    = header & 0xFF;
    if (pHeader->flags & 0x1)
    {
        pHeader->version = (header >> 8) & 0xFF;
        pHeader->descSize = (header >> 16) & 0xFFFF;
    }
    else
    {
        pHeader->version = 1;
    }
}

RC FalconUCode::ParseBinaryGfwV2
(
    const vector<UINT32>& uCodeBinary,
    const UCodeHeaderInfo &headerInfo,
    vector<UINT32> *pImemBinary,
    vector<UINT32> *pDmemBinary,
    UCodeInfo *pUcodeInfo
)
{
    MASSERT(pImemBinary);
    MASSERT(pDmemBinary);
    MASSERT(pUcodeInfo);

    // See https://wiki.lwpu.com/engwiki/index.php/VBIOS/Data_Structures/Falcon_Ucode/Falcon_Ucode_Descriptor/Spec_V2
    enum DescOffsets
    {
        HEADER             = 0,
        STORED_SIZE        = 1,
        UNCOMPRESSED_SIZE  = 2,
        VIRTUAL_ENTRY      = 3,
        INTERFACE_OFFSET   = 4,
        IMEM_PHY_BASE      = 5,
        IMEM_SIZE          = 6,
        IMEM_VIRT_BASE     = 7,
        IMEM_SEC_START     = 8,
        IMEM_SEC_SIZE      = 9,
        DMEM_OFFSET        = 10,
        DMEM_PHY_BASE      = 11,
        DMEM_SIZE          = 12,
        ALT_IMEM_SIZE      = 13,
        ALT_DMEM_SIZE      = 14
    };

    UINT32 imemSize     = uCodeBinary[IMEM_SIZE];
    UINT32 dmemSize     = uCodeBinary[DMEM_SIZE];
    UINT32 dmemOffset   = uCodeBinary[DMEM_OFFSET];

    if (headerInfo.version != 2)
    {
        Printf(Tee::PriError, "Ucode header version %u doesn't match with ucode desc type\n",
                               headerInfo.version);
        return RC::CANNOT_PARSE_FILE;
    }
    if (uCodeBinary[STORED_SIZE] != uCodeBinary[UNCOMPRESSED_SIZE])
    {
        Printf(Tee::PriError, "Parsing compressed binaries not supported lwrrently\n");
        return RC::CANNOT_PARSE_FILE;
    }
    if (headerInfo.descSize + imemSize + dmemSize > uCodeBinary.size() * 4)
    {
        Printf(Tee::PriError, "Malformed or unencrypted binary!\n");
        return RC::CANNOT_PARSE_FILE;
    }

    vector<UINT32>::const_iterator start     = uCodeBinary.begin();
    vector<UINT32>::const_iterator imemStart = start + headerInfo.descSize / 4;
    vector<UINT32>::const_iterator dmemStart = imemStart + dmemOffset / 4;
    vector<UINT32>::const_iterator end       = dmemStart  + dmemSize / 4;

    pUcodeInfo->intrDataOffsetBytes = uCodeBinary[INTERFACE_OFFSET];
    pUcodeInfo->imemBase            = uCodeBinary[IMEM_PHY_BASE];
    pUcodeInfo->dmemBase            = uCodeBinary[DMEM_PHY_BASE];
    pUcodeInfo->secStart            = uCodeBinary[IMEM_SEC_START] + pUcodeInfo->imemBase;
    pUcodeInfo->secEnd              = pUcodeInfo->secStart + uCodeBinary[IMEM_SEC_SIZE] - 1;

    Printf(Tee::PriLow, "Sizes (Bytes): IMEM: 0x%x; DMEM: 0x%x\n", imemSize, dmemSize);

    pImemBinary->clear();
    pImemBinary->assign(imemStart, dmemStart);
    pDmemBinary->clear();
    pDmemBinary->assign(dmemStart, end);

    return RC::OK;
}

RC FalconUCode::ParseBinaryGfwV3
(
    const vector<UINT32>& uCodeBinary,
    const UCodeHeaderInfo &headerInfo,
    vector<UINT32> *pImemBinary,
    vector<UINT32> *pDmemBinary,
    UCodeInfo *pUcodeInfo
)
{
    MASSERT(pImemBinary);
    MASSERT(pDmemBinary);
    MASSERT(pUcodeInfo);

    // See https://wiki.lwpu.com/engwiki/index.php/VBIOS/Data_Structures/Falcon_Ucode/Falcon_Ucode_Descriptor/Spec_V3
    enum DescOffsets
    {
        HEADER             = 0,
        STORED_SIZE        = 1,
        PKC_OFFSET         = 2,
        INTERFACE_OFFSET   = 3,
        IMEM_PHY_BASE      = 4,
        IMEM_SIZE          = 5,
        IMEM_VIRT_BASE     = 6,
        DMEM_PHY_BASE      = 7,
        DMEM_SIZE          = 8,
        ID_INFO            = 9,
        SIG_VERSIONS       = 10,
        SIGNATURES         = 11
    };
    const UINT32 sigSizeBytes = 384;

    UINT32 imemSize     = uCodeBinary[IMEM_SIZE];
    UINT32 dmemSize     = uCodeBinary[DMEM_SIZE];

    if (headerInfo.version != 3)
    {
        Printf(Tee::PriError, "Ucode header version %u doesn't match with ucode desc type\n",
                               headerInfo.version);
        return RC::CANNOT_PARSE_FILE;
    }
    if (headerInfo.descSize + imemSize + dmemSize > uCodeBinary.size() * 4)
    {
        Printf(Tee::PriError, "Malformed or unencrypted binary!\n");
        return RC::CANNOT_PARSE_FILE;
    }

    vector<UINT32>::const_iterator start     = uCodeBinary.begin();
    vector<UINT32>::const_iterator imemStart = start + headerInfo.descSize / 4;
    vector<UINT32>::const_iterator dmemStart = imemStart + imemSize / 4;
    vector<UINT32>::const_iterator dmemEnd   = dmemStart + dmemSize / 4;

    pUcodeInfo->dmemBase            = uCodeBinary[DMEM_PHY_BASE];
    pUcodeInfo->intrDataOffsetBytes = uCodeBinary[INTERFACE_OFFSET];
    pUcodeInfo->pkcDataOffsetBytes  = uCodeBinary[PKC_OFFSET];
    // All IMEM blocks are secure
    pUcodeInfo->imemBase        = uCodeBinary[IMEM_PHY_BASE];
    pUcodeInfo->secStart        = pUcodeInfo->imemBase;
    pUcodeInfo->secEnd          = pUcodeInfo->secStart + imemSize - 1;

    Printf(Tee::PriLow, "Sizes (Bytes): IMEM: 0x%x; DMEM: 0x%x\n", imemSize, dmemSize);

    pImemBinary->clear();
    pImemBinary->assign(imemStart, dmemStart);
    pDmemBinary->clear();
    pDmemBinary->assign(dmemStart, dmemEnd);

    // All GFW V3 ucodes will be encrypted
    pUcodeInfo->bIsEncrypted = true;
    pUcodeInfo->engIdMask           = uCodeBinary[ID_INFO] & 0xFFFF;
    pUcodeInfo->ucodeId             = (uCodeBinary[ID_INFO] >> 16) & 0xFF;
    pUcodeInfo->sigCount            = (uCodeBinary[ID_INFO] >> 24) & 0xFF;
    pUcodeInfo->sigVersions         = uCodeBinary[SIG_VERSIONS] & 0xFFFF;
    pUcodeInfo->sigSizeBytes        = sigSizeBytes;
    pUcodeInfo->sigImageOffsetBytes = SIGNATURES * 4;

    return RC::OK;
}

RC FalconUCode::GetApplicationTableEntries
(
    const UCodeInfo &ucodeInfo,
    vector<UINT32> *pDmemBinary,
    vector<AppTableEntry> *pEntries
)
{
    MASSERT(pDmemBinary);
    MASSERT(pEntries);

    UINT32 interfaceOffset  = ucodeInfo.intrDataOffsetBytes - ucodeInfo.dmemBase;
    UINT08 *pDmem           = reinterpret_cast<UINT08*>(pDmemBinary->data());
    AppTableHeader *pHeader = reinterpret_cast<AppTableHeader*>(pDmem + interfaceOffset);

    if (pHeader->version != 0x1)
    {
        Printf(Tee::PriError, "Application Table Entry version %u != 1\n", pHeader->version);
        return RC::SOFTWARE_ERROR;
    }

    if (pHeader->entryCount != 0)
    {
        pEntries->clear();
        pEntries->resize(pHeader->entryCount);
        memcpy(pEntries->data(),
               pDmem + interfaceOffset + pHeader->headerSize,
               static_cast<size_t>(pHeader->entryCount * pHeader->entrySize));
    }
    else
    {
        Printf(Tee::PriLow, "No Application Table Entries found\n");
    }

    return RC::OK;
}

RC FalconUCode::GetInterfaceOffset
(
    const UCodeInfo &ucodeInfo,
    vector<UINT32> *pDmemBinary,
    InterfaceId interfaceId,
    UINT32 *pInterfaceOffset
)
{
    RC rc;
    MASSERT(pDmemBinary);
    MASSERT(pInterfaceOffset);

    vector<AppTableEntry> appEntries;
    CHECK_RC(GetApplicationTableEntries(ucodeInfo,
                                        pDmemBinary,
                                        &appEntries));
    if (appEntries.empty())
    {
        Printf(Tee::PriError, "No application table entries found\n");
        return RC::SOFTWARE_ERROR;
    }

    *pInterfaceOffset = ~0U;
    for (const auto &appEntry : appEntries)
    {
        if (appEntry.interfaceId == interfaceId)
        {
            *pInterfaceOffset = appEntry.interfacePtr - ucodeInfo.dmemBase;
            break;
        }
    }
    if (*pInterfaceOffset == ~0U)
    {
        Printf(Tee::PriError, "Interface offset not found\n");
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC FalconUCode::GetDmemMapper
(
    const UCodeInfo &ucodeInfo,
    vector<UINT32> *pDmemBinary,
    DmemMapper **ppDmemMapper
)
{
    RC rc;
    MASSERT(pDmemBinary);
    MASSERT(ppDmemMapper);

    UINT32 dmemMapperOffset;
    CHECK_RC(GetInterfaceOffset(ucodeInfo,
                                pDmemBinary,
                                InterfaceId::DMEM_MAPPER,
                                &dmemMapperOffset));

    UINT08 *pDmem = reinterpret_cast<UINT08*>(pDmemBinary->data());
    *ppDmemMapper = reinterpret_cast<DmemMapper*>(pDmem + dmemMapperOffset);
    if ((*ppDmemMapper)->version != 3)
    {
        Printf(Tee::PriError, "DMEM Mapper version %u != 3\n", (*ppDmemMapper)->version);
        return RC::SOFTWARE_ERROR;
    }

    return RC::OK;
}

RC FalconUCode::GetMultiTgtInterface
(
    const UCodeInfo &ucodeInfo,
    vector<UINT32> *pDmemBinary,
    MultiTgtInterface **ppMultiTgtInterface
)
{
    RC rc;
    MASSERT(pDmemBinary);
    MASSERT(ppMultiTgtInterface);

    UINT32 multiTgtOffset;
    CHECK_RC(GetInterfaceOffset(ucodeInfo,
                                pDmemBinary,
                                InterfaceId::MULTIPLE_TARGET,
                                &multiTgtOffset));

    UINT08 *pDmem = reinterpret_cast<UINT08*>(pDmemBinary->data());
    *ppMultiTgtInterface = reinterpret_cast<MultiTgtInterface*>(pDmem + multiTgtOffset);
    if ((*ppMultiTgtInterface)->version != 1)
    {
        Printf(Tee::PriError, "Mutiple-Target Interface version %u != 3\n",
                               (*ppMultiTgtInterface)->version);
        return RC::SOFTWARE_ERROR;
    }

    return RC::OK;
}

RC FalconUCode::AddSignature
(
    const vector<UINT32>& uCodeBinary,
    const UCodeInfo &ucodeInfo,
    UINT32 ucodeVersion,
    vector<UINT32> *pDmemBinary
)
{
    MASSERT(pDmemBinary);

    if (ucodeInfo.sigSizeBytes == 0)
    {
        return RC::OK;
    }

    // See https://wiki.lwpu.com/engwiki/index.php/VBIOS/Data_Structures/Falcon_Ucode/Falcon_Ucode_Descriptor/Spec_V3#Code_Sample
    // - Signature versions =
    //   Positions of set bits in this field indicate which ucode versions correspond to the RSA3K signatures
    // - Signature 0 =
    //   Signature for falcons with FALCON_UCODE_VERSION = the position of the first set bit in sig version
    // - Signature N =
    //   Signature for falcons with FALCON_UCODE_VERSION = the position of the (n+1)th set bit in sig version
    // Eg. If sig version = 0b00010100
    //     This means that for ucodeVersion = 2, signature idx = 0
    //                     for ucodeVersion = 4, signature idx = 1

    // Check if we have a signature for the current ucode's fused version
    // If the bit is not set in signature versions => we don't have the signature for it
    if (((1 << ucodeVersion) & ucodeInfo.sigVersions) == 0)
    {
        Printf(Tee::PriError, "Ucode version has been revoked\n");
        return RC::SOFTWARE_ERROR;
    }

    // Find the signature idx to be used
    UINT32 sigVersion = ucodeInfo.sigVersions;
    UINT32 sigIdx = UINT32_MAX;
    UINT32 setBitIdx = 0;
    while (sigVersion != 0)
    {
        if (Utility::BitScanForward(sigVersion) == static_cast<INT32>(ucodeVersion))
        {
            sigIdx = setBitIdx;
            break;
        }
        sigVersion &= (sigVersion - 1);
        setBitIdx++;
    }
    if (sigIdx >= ucodeInfo.sigCount)
    {
        Printf(Tee::PriError, "Signature index exceeds max possible index\n");
        return RC::SOFTWARE_ERROR;
    }

    // Load the signature corresponding to the matching ucode version
    UINT08 *pSigAddr = reinterpret_cast<UINT08*>(pDmemBinary->data()) +
                       ucodeInfo.pkcDataOffsetBytes;
    UINT08 *pSig     = reinterpret_cast<UINT08*>(const_cast<UINT32*>(uCodeBinary.data())) +
                       ucodeInfo.sigImageOffsetBytes +
                       (sigIdx * ucodeInfo.sigSizeBytes);
    Platform::MemCopy(pSigAddr, pSig, ucodeInfo.sigSizeBytes);

    return RC::OK;
}
