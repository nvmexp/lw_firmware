/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2018, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gm10x_fuse.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/scopereg.h"
#include "core/include/utility.h"
#include "core/include/crypto.h"
#include "fusesrc.h"
#include "core/include/fileholder.h"
#include "core/include/version.h"
#include "maxwell/gm107/dev_fuse.h"
#include "maxwell/gm107/dev_pwr_pri.h"
#include "maxwell/gm107/dev_master.h"
#include "maxwell/gm107/dev_bus.h"
#include "maxwell/gm107/dev_ext_devices.h"
#include "maxwell/gm107/dev_lw_xve.h"
#include "maxwell/gm107/dev_lw_xve1.h"
#include "maxwell/gm107/dev_lw_xve.h"
#include "maxwell/gm107/dev_lw_xp.h"
#include "maxwell/gm107/dev_trim.h"
#include "maxwell/gm107/dev_host.h"

//-----------------------------------------------------------------------------
GM10xFuse::GM10xFuse(Device* pSubdev)
:
 GpuFuse(pSubdev),
 m_PrivSecSwFuseValue(0)
{
    // these values may change for maxwell family
    m_FuseSpec.NumOfFuseReg  = 64;
    m_FuseSpec.NumKFuseWords = 192;
    m_FuseSpec.FuselessStart = 512; // start of ram repair in gm10x
    m_FuseSpec.FuselessEnd   = 2015;
    // these are delays in Ms; copied from GK208 for now
    m_FuseSpec.SleepTime[FuseSpec::SLEEP_AFTER_GPIO_HIGH] = 5;
    m_FuseSpec.SleepTime[FuseSpec::SLEEP_AFTER_FUSECTRL_WRITE] = 10;
    m_FuseSpec.SleepTime[FuseSpec::SLEEP_AFTER_GPIO_LOW] = 600;
    m_FuseSpec.SleepTime[FuseSpec::SLEEP_AFTER_FUSECTRL_READ] = 10;

    GpuSubdevice *pGpuSubdev = (GpuSubdevice*)GetDevice();
    switch (pGpuSubdev->DeviceId())
    {
        case Gpu::GM108:
            m_GPUSuffix = "GM108";
            break;

        case Gpu::GM107:
            m_GPUSuffix = "GM107";
            break;

        default:
            m_GPUSuffix = "UNKNOWN";
            break;
    }

    m_SetFieldSpaceName[IFF_SPACE(PMC_0)] = "LW_PMC";
    m_SetFieldSpaceAddr[IFF_SPACE(PMC_0)] = DRF_BASE(LW_PMC);

    m_SetFieldSpaceName[IFF_SPACE(PBUS_0)] = "LW_PBUS";
    m_SetFieldSpaceAddr[IFF_SPACE(PBUS_0)] = DRF_BASE(LW_PBUS);

    m_SetFieldSpaceName[IFF_SPACE(PEXTDEV_0)] = "LW_PEXTDEV";
    m_SetFieldSpaceAddr[IFF_SPACE(PEXTDEV_0)] = DRF_BASE(LW_PEXTDEV);

    m_SetFieldSpaceName[IFF_SPACE(PCFG_0)] = "LW_PCFG";
    m_SetFieldSpaceAddr[IFF_SPACE(PCFG_0)] = DRF_BASE(LW_PCFG);

    m_SetFieldSpaceName[IFF_SPACE(PCFG1_0)] = "LW_PCFG1";
    m_SetFieldSpaceAddr[IFF_SPACE(PCFG1_0)] = DRF_BASE(LW_PCFG1);

    // LW_PCFG2 does not exist in the .h, but does exist in the .ref
    m_SetFieldSpaceName[IFF_SPACE(PCFG2_0)] = "LW_PCFG2";
    m_SetFieldSpaceAddr[IFF_SPACE(PCFG2_0)] = 0x00083000;

    m_SetFieldSpaceName[IFF_SPACE(XP3G_0)] = "LW_XP3G_0";
    m_SetFieldSpaceAddr[IFF_SPACE(XP3G_0)] = DRF_BASE(LW_XP) + 0x0000;

    m_SetFieldSpaceName[IFF_SPACE(XP3G_1)] = "LW_XP3G_1";
    m_SetFieldSpaceAddr[IFF_SPACE(XP3G_1)] = DRF_BASE(LW_XP) + 0x1000;

    m_SetFieldSpaceName[IFF_SPACE(XP3G_2)] = "LW_XP3G_2";
    m_SetFieldSpaceAddr[IFF_SPACE(XP3G_2)] = DRF_BASE(LW_XP) + 0x2000;

    m_SetFieldSpaceName[IFF_SPACE(XP3G_3)] = "LW_XP3G_3";
    m_SetFieldSpaceAddr[IFF_SPACE(XP3G_3)] = DRF_BASE(LW_XP) + 0x3000;

    m_SetFieldSpaceName[IFF_SPACE(PTRIM_7)] = "LW_PTRIM_7";
    m_SetFieldSpaceAddr[IFF_SPACE(PTRIM_7)] = DRF_BASE(LW_PTRIM) + 0x7000;

    // LW_PVTRIM isn't 0x1000 aligned, thus HW masks out the last 12 bits.
    m_SetFieldSpaceName[IFF_SPACE(PVTRIM_0)] = "LW_PVTRIM";
    m_SetFieldSpaceAddr[IFF_SPACE(PVTRIM_0)] = DRF_BASE(LW_PVTRIM) & ~0xFFF;

    m_SetFieldSpaceName[IFF_SPACE(PHOST_0)] = "LW_PHOST";
    m_SetFieldSpaceAddr[IFF_SPACE(PHOST_0)] = DRF_BASE(LW_PHOST);
}

//-----------------------------------------------------------------------------
/* virtual */ bool GM10xFuse::PollFusesIdle()
{
    /* Following register offsets changed in GK208
       #define LW_FUSE_FUSECTRL_STATE  19:16
        is now,
       #define LW_FUSE_FUSECTRL_STATE  20:16
    */

    // copied implementation from GT21x
    GpuSubdevice *pSubDev = (GpuSubdevice *)this->GetDevice();
    bool retval = FLD_TEST_DRF(_FUSE,
                               _FUSECTRL,
                               _STATE,
                               _IDLE,
                               pSubDev->RegRd32(LW_FUSE_FUSECTRL));
    return retval;
}
//------------------------------------------------------------------------------
// Wrapper around GF10xFuse::ReadInRecords to find the
// start of IFF before reading in the records.
//------------------------------------------------------------------------------
RC GM10xFuse::ReadInRecords()
{
    RC rc;
    vector<UINT32> fuseRegs;
    CHECK_RC(GetCachedFuseReg(&fuseRegs));

    for (UINT32 lwrrOffset = m_FuseSpec.FuselessStart;
         lwrrOffset < m_FuseSpec.FuselessEnd;
         lwrrOffset += REPLACE_RECORD_SIZE)
    {
        UINT32 tempOffset = lwrrOffset + CHAINID_SIZE;
        UINT32 type = GetFuseSnippet(fuseRegs,
                                     GetRecordTypeSize(),
                                     &tempOffset);

        if (type == INIT_FROM_FUSE)
        {
            // Found the start of IFF, and thus the end of fuseless fuses
            m_FuseSpec.FuselessEnd = lwrrOffset;
            break;
        }
    }
    return GpuFuse::ReadInRecords();
}

//------------------------------------------------------------------------------
// Common method to handle ALL types of fuse records EXCEPT replace records
// (Maxwell style fuse records)
//------------------------------------------------------------------------------
/* virtual */ RC GM10xFuse::ProcessNonRRFuseRecord(
                                                 UINT32 RecordType,
                                                 UINT32 ChainId,
                                                 const vector<UINT32> &FuseRegs,
                                                 UINT32 *pLwrrOffset)
{
    RC rc;
    switch (RecordType)
    {
        case REPLACE_REC:
                Printf(Tee::PriNormal,
                       "Unable to process replace records\n");
                rc = RC::BAD_PARAMETER;
                break;
        case INIT_FROM_FUSE:
                // Ignore IFF fuses and but don't fail MODS
                // They are handled as column-based fuses in gt21x_fuse.cpp
                if (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
                {
                    Printf(Tee::PriNormal,
                           "IFF fuse detected @ row %d\n",
                           *pLwrrOffset / 32);
                }

                // If we hit an IFF fuse record that means no more fuseless
                // fuses can be encountered further
                // Seek to end of previous fuse record
                *pLwrrOffset -= (CHAINID_SIZE + GetRecordTypeSize());

                // Update the fuseless end marker
                m_FuseSpec.FuselessEnd = *pLwrrOffset;
                break;
        default:
                Printf(Tee::PriNormal,
                       "unknown record type %d\n", RecordType);
                rc = RC::SOFTWARE_ERROR;
                break;
    }
    return rc;
}

//------------------------------------------------------------------------------
// GM10x Fuse API to fetch encryption key from a file
//------------------------------------------------------------------------------
/* virtual */ RC GM10xFuse::GetFuseKeyFromFile(string fileName, size_t keyLength,
                                               string &key)
{
    RC rc;
    FileHolder file;
    long filesize = 0;
    CHECK_RC(file.Open(fileName, "rb"));
    CHECK_RC(Utility::FileSize(file.GetFile(), &filesize));

    // Copy the file into a buffer and verify the operation
    vector<UINT08> fileContents(filesize);
    size_t length = fread((void *)&fileContents[0], 1, filesize, file.GetFile());
    if (length != (size_t)filesize || length == 0)
    {
        Printf(Tee::PriError, "Failed to read key from %s.\n", fileName.c_str());
        return RC::FILE_READ_ERROR;
    }

    key = string(reinterpret_cast<const char *>(&fileContents[0]), keyLength);
    return RC::OK;
}

//------------------------------------------------------------------------------
// GM10x Fuse API to fetch data from PMU binary
//------------------------------------------------------------------------------
/* virtual */ RC GM10xFuse::ReadFromFuseBinary(const string &fuseChunk,
                                              vector<UINT08> fuseBinary,
                                              vector<UINT32> *data)
{
    char *fuseString = reinterpret_cast<char *>(&fuseBinary[0]);
    char *chunkString = strstr(fuseString, fuseChunk.c_str());
    if (chunkString == nullptr)
    {
        //try the chunk in lower case as well
        char *chunkString = strstr(fuseString, Utility::ToLowerCase(fuseChunk).c_str());
        if (chunkString == nullptr)
        {
            Printf(Tee::PriError, "ReadFromFuseBinary: Failed to find fuse chunk %s\n",
                    fuseChunk.c_str());
            return RC::SOFTWARE_ERROR;
        }
    }

    // chunkString = string of numbers inside fuseChunk { };
    const char *bracketsStart = strchr(chunkString, '{');
    const char *bracketsEnd = strchr(chunkString, '}');
    if (bracketsStart == nullptr || bracketsEnd == nullptr)
    {
        Printf(Tee::PriError, "ReadFromFuseBinary: Failed to read data in fuse chunk\n");
        return RC::SOFTWARE_ERROR;
    }

    // Create a string by removing braces from chunkString
    string KeyString(bracketsStart + 1, bracketsEnd - bracketsStart - 1);
    vector<string> KeyElements;
    Utility::Tokenizer(KeyString, ",", &KeyElements);
    data->clear();
    for (UINT32 i=0; i < KeyElements.size(); i++)
    {
        //push only non character keys
        if (KeyElements[i].find_first_of("0123456789") != string::npos)
        {
            data->push_back(static_cast<UINT32>(
                Utility::Strtoull(KeyElements[i].c_str(), 0, 0)));
        }
    }
    MASSERT(data->size() > 0);
    return RC::OK;
}

//------------------------------------------------------------------------------
// GM10x Fuse API to pass on the key to JS to be written to daughter card.
//------------------------------------------------------------------------------
/* virtual */ RC GM10xFuse::GetFuseKeyToInstall(string &installFuseKey)
{
    RC rc;
    string L2eKeyFileName = "installKey_" + m_GPUSuffix + ".bin";
    CHECK_RC(GetFuseKeyFromFile(L2eKeyFileName, FUSE_KEY_LENGTH, installFuseKey));
    Printf(Tee::PriDebug, "Installing key %s \n", installFuseKey.c_str());
    return RC::OK;
}

//------------------------------------------------------------------------------
// GM10x Fuse API to set the decryption fuseKey read from daughter card.
//------------------------------------------------------------------------------
/* virtual */ RC GM10xFuse::SetInstalledFuseKey(string fuseKey)
{
    RC rc;
    if(fuseKey.empty() || fuseKey.length() != FUSE_KEY_LENGTH)
    {
        Printf(Tee::PriNormal, "Error: Fusekeys of not right size.\n");
        return RC::SOFTWARE_ERROR;
    }
    string L1Key;
    string L1KeyFileName = "fuseKey_" + m_GPUSuffix + ".bin";
    CHECK_RC(GetFuseKeyFromFile(L1KeyFileName, FUSE_KEY_LENGTH, L1Key));

    Crypto::XTSAESKey xtsAesKey;
    memset(&xtsAesKey, 0 , sizeof(xtsAesKey));
    memcpy(reinterpret_cast<UINT08 *>(&(xtsAesKey.keys[0].dwords[0].dw)),
           reinterpret_cast<const char *>(L1Key.c_str()),
           FUSE_KEY_LENGTH);

    Printf(Tee::PriDebug, "Decrypting with key %s.\n",
        reinterpret_cast<char *>(&(xtsAesKey.keys[0].dwords[0].dw)));
    vector<UINT08> L2Key(FUSE_KEY_LENGTH);
    CHECK_RC(Crypto::XtsAesDecSect(
                xtsAesKey,
                0,
                FUSE_KEY_LENGTH,
                reinterpret_cast<const UINT08 *>(fuseKey.c_str()),
                reinterpret_cast< UINT08 *>(&L2Key[0])));

    m_L2Key = string(reinterpret_cast<const char *>(&L2Key[0]), FUSE_KEY_LENGTH);
    Printf(Tee::PriDebug, "Setting L2Key to %s \n", m_L2Key.c_str());

    return RC::OK;
}

//------------------------------------------------------------------------------
// GM10x Fuse API to fetch secure PMU binary for current GPU
// (including PMU code and data sections)
//------------------------------------------------------------------------------
/* virtual */ RC GM10xFuse::GetSelwrePmuUcode(bool isPMUBinaryEncrypted,
                                              vector<UINT32> *pUcode,
                                              vector<PMU::PmuUcodeInfo> *pData,
                                              vector<PMU::PmuUcodeInfo> *pCode)
{
    RC rc;
    MASSERT(pUcode);
    MASSERT(pData);
    MASSERT(pCode);

    // Standard indexes to data/code offsets defined in PMU ucode
    const UINT32 FUSE_UCODE_HEADER_OS_CODE_OFFSET_INDEX  = 0;
    const UINT32 FUSE_UCODE_HEADER_OS_CODE_SIZE_INDEX    = 1;
    const UINT32 FUSE_UCODE_HEADER_APP_CODE_OFFSET_INDEX = 5;
    const UINT32 FUSE_UCODE_HEADER_APP_CODE_SIZE_INDEX   = 6;
    const UINT32 FUSE_UCODE_HEADER_OS_DATA_OFFSET_INDEX  = 2;
    const UINT32 FUSE_UCODE_HEADER_OS_DATA_SIZE_INDEX    = 3;
    const UINT32 FUSE_UCODE_HEADER_APP_DATA_OFFSET_INDEX = 7;
    const UINT32 FUSE_UCODE_HEADER_APP_DATA_SIZE_INDEX   = 8;

    // the header could start with either fuse_ or pmu_
    string strUcodeHeader = "_ucode_header_" + m_GPUSuffix;
    string strUcodeData = "_ucode_data_" + m_GPUSuffix;
    vector<UINT32> ucodeHeader;
    FileHolder file;
    long filesize = 0;

    string pmuBinFile = "fusebinary_" + m_GPUSuffix + ".bin";
    if (!isPMUBinaryEncrypted)
    {
        pmuBinFile = "fusebinary_" + m_GPUSuffix + ".txt";
    }
    rc = file.Open(pmuBinFile, "rb");
    if (rc != RC::OK)
    {
        //try lower case
        pmuBinFile = Utility::ToLowerCase(pmuBinFile);
        CHECK_RC(file.Open(pmuBinFile, "rb"));
    }
    Printf(Tee::PriNormal, "Using keys from %s.\n", pmuBinFile.c_str());
    CHECK_RC(Utility::FileSize(file.GetFile(), &filesize));

    // Copy the file into a buffer and verify the operation
    vector<UINT08> fuseBinaryContents(filesize);
    size_t length = fread((void *)&fuseBinaryContents[0], 1, filesize,
                            file.GetFile());
    if (length != (size_t)filesize || length == 0)
    {
        Printf(Tee::PriError, "Failed to read fuse binary.\n");
        return RC::FILE_READ_ERROR;
    }

    // Decrypt the PMU binary if encrypted
    if (isPMUBinaryEncrypted)
    {
        Crypto::XTSAESKey xtsAesKey;
        memset(&xtsAesKey, 0 , sizeof(xtsAesKey));
        memcpy(reinterpret_cast<UINT08 *>(&(xtsAesKey.keys[0].dwords[0].dw)),
               reinterpret_cast<const char *>(m_L2Key.c_str()),
               FUSE_KEY_LENGTH);

        Printf(Tee::PriDebug, "Decrypting file %s with key %s.\n",
            pmuBinFile.c_str(),
            reinterpret_cast<char *>(&(xtsAesKey.keys[0].dwords[0].dw)));
        vector<UINT08> decryptedFuseBinary(filesize+1);
        CHECK_RC(Crypto::XtsAesDecSect(
                    xtsAesKey,
                    0,
                    filesize,
                    reinterpret_cast<const UINT08 *>(&fuseBinaryContents[0]),
                    reinterpret_cast<UINT08 *>(&decryptedFuseBinary[0])));

        decryptedFuseBinary[filesize] = '\0';
        Printf(Tee::PriDebug, "Decrypted fuse binary contents: \n");
        Printf(Tee::PriDebug, "%s\n",
                reinterpret_cast<char *>(&decryptedFuseBinary[0]));

        //replace encrypted binary with decrypted.
        fuseBinaryContents = decryptedFuseBinary;
    }

    CHECK_RC(ReadFromFuseBinary(strUcodeHeader, fuseBinaryContents,
                                    &ucodeHeader));
    CHECK_RC(ReadFromFuseBinary(strUcodeData, fuseBinaryContents, pUcode));

    UINT32 *pUcodeHeader = &ucodeHeader[0];
    PMU::PmuUcodeInfo OsData =
    {
        *(pUcodeHeader + FUSE_UCODE_HEADER_OS_DATA_OFFSET_INDEX),
        *(pUcodeHeader + FUSE_UCODE_HEADER_OS_DATA_SIZE_INDEX),
        false
    };

    PMU::PmuUcodeInfo AppData =
    {
        *(pUcodeHeader + FUSE_UCODE_HEADER_APP_DATA_OFFSET_INDEX),
        *(pUcodeHeader + FUSE_UCODE_HEADER_APP_DATA_SIZE_INDEX),
        false
    };

    PMU::PmuUcodeInfo OsCode =
    {
       *(pUcodeHeader + FUSE_UCODE_HEADER_OS_CODE_OFFSET_INDEX),
       *(pUcodeHeader + FUSE_UCODE_HEADER_OS_CODE_SIZE_INDEX),
       false
    };

    PMU::PmuUcodeInfo AppCode =
    {
        *(pUcodeHeader + FUSE_UCODE_HEADER_APP_CODE_OFFSET_INDEX),
        *(pUcodeHeader + FUSE_UCODE_HEADER_APP_CODE_SIZE_INDEX),
        true
    };

    pData->clear();
    pCode->clear();

    pData->push_back(OsData);
    pData->push_back(AppData);
    pCode->push_back(OsCode);
    pCode->push_back(AppCode);

    return rc;
}

// Secure Fuseblow support
RC GM10xFuse::EnableSelwreFuseblow(bool isPMUBinaryEncrypted)
{
    RC rc;
    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();
    PMU *pPmu = nullptr;

    vector<UINT32> SelwrePmuUcode;
    vector<PMU::PmuUcodeInfo> DataSections;
    vector<PMU::PmuUcodeInfo> CodeSections;

    CHECK_RC(pSubdev->GetPmu(&pPmu));
    CHECK_RC(GetSelwrePmuUcode(isPMUBinaryEncrypted,
                               &SelwrePmuUcode,
                               &DataSections,
                               &CodeSections));

    MASSERT(SelwrePmuUcode.size());

    // Reset the PMU without RM support
    CHECK_RC(pPmu->ResetPmuWithoutRm());

    // Load the secure PMU data
    CHECK_RC(pPmu->LoadPmuUcodeSection(0, // always use access port 0
                                       PMU::PMU_SECTION_TYPE_DATA,
                                       SelwrePmuUcode,
                                       DataSections));
    // Load the secure PMU code
    CHECK_RC(pPmu->LoadPmuUcodeSection(0, // always use access port 0
                                       PMU::PMU_SECTION_TYPE_CODE,
                                       SelwrePmuUcode,
                                       CodeSections));
    // Execute the PMU ucode
    CHECK_RC(pPmu->ExelwtePmuWithoutRm());

    // Check for secure PMU ucode status (mailbox register)
    // LW_PPWR_FALCON_MAILBOX0 [31:0]
    // PMU ucode status code (Bits [31:16])
    // PMU additional data (Bits [15:0])
    UINT32 PmuMailboxReg = pSubdev->RegRd32(LW_PPWR_FALCON_MAILBOX0);

    SelwrePmuStatusCode PmuStatusCode = static_cast<SelwrePmuStatusCode>(
                                            (PmuMailboxReg & 0xFFFF0000) >> 16);
    UINT32 PmuAddtnlData = PmuMailboxReg & 0xFFFF;

    switch (PmuStatusCode)
    {
        case SELWRE_PMU_STATUS_SUCCESS:
            break;
        case SELWRE_PMU_STATUS_UCODE_VERSION_MISMATCH:
            Printf(Tee::PriError,
                   "Ucode version mismatch !\n");
            return RC::CMD_STATUS_ERROR;
            break;
        case SELWRE_PMU_STATUS_UNSUPPORTED_GPU:
            Printf(Tee::PriError,
                   "Unsupported GPU ID, Expected 0x%08x\n", PmuAddtnlData);
            return RC::CMD_STATUS_ERROR;
            break;
        case SELWRE_PMU_STATUS_FAILURE:
            Printf(Tee::PriError,
                   "PMU ucode failed to lower privilege level\n");
            return RC::CMD_STATUS_ERROR;
            break;
        default:
            Printf(Tee::PriError,
                   "Unknown PMU status code = 0x%08x \n", PmuStatusCode);
            return RC::UNSUPPORTED_FUNCTION;
            break;
    }

    // One last check to confirm if the security privilege levels have been
    // lowered to proceed with secure fuseblow
    UINT32 FuseWdataPrivMask = pSubdev->RegRd32(LW_FUSE_WDATA_PRIV_LEVEL_MASK);

    if (!FLD_TEST_DRF(_FUSE,
                      _WDATA_PRIV_LEVEL_MASK,
                      _WRITE_PROTECTION,
                      _ALL_LEVELS_ENABLED,
                      FuseWdataPrivMask))
    {
        Printf(Tee::PriError, "Secure PMU could not be loaded !\n");
        return RC::PMU_DEVICE_FAIL;
    }

    return rc;
}

/* virtual */ bool GM10xFuse::IsFusePrivSelwrityEnabled()
{
    const UINT32 ENABLE_FUSE_PRIV_SEC = (1<<0);
    UINT32 FuseVal = 0;

    // Look in the OPT registers if SW fusing is enabled
    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();
    FuseVal = pSubdev->RegRd32(LW_FUSE_OPT_PRIV_SEC_EN);

    return ((FuseVal & ENABLE_FUSE_PRIV_SEC) == ENABLE_FUSE_PRIV_SEC);
}

//-----------------------------------------------------------------------------
// SW_OVERRIDE_FUSE is enabled by default and is a disable fuse
// Enabling SW fusing is not supported on Maxwell
RC GM10xFuse::EnableSwFusing(string Method,
                             UINT32 DurationNs,
                             UINT32 LwVddMv)
{
    Printf(Tee::PriNormal, "SW fusing can only be disabled.\n");
    return RC::UNSUPPORTED_FUNCTION;
}

// SW fusing support in Maxwell
/* virtual */ bool GM10xFuse::IsSwFusingEnabled()
{
    const FuseUtil::FuseDef *pFuseDef = GetSwFusingFuseDef();
    if (nullptr == pFuseDef)
    {
        Printf(Tee::PriNormal, "XML error. no SW_OVERRIDE_FUSE detected\n");
        return false;
    }

    CacheFuseReg();
    UINT32 FuseVal = ExtractFuseVal(pFuseDef, OldFuse::ALL_MATCH);

    // Bit 0 of SW_OVERRIDE_FUSE must be 0x0
    const UINT32 ENABLE_SW_FUSING = (0<<0);
    if (FuseVal == ENABLE_SW_FUSING)
    {
        return true;
    }
    return false;
}

/* virtual */ bool GM10xFuse::IsSwFusingAllowed()
{
    // On Maxwell SW_OVERRIDE_FUSE is no longer an undo fuse
    // It is just a disable fuse, so if bit 0 == 0x0 then
    // MODS can proceed with SW fusing
    return (IsSwFusingEnabled());
}

/* virtual */ RC GM10xFuse::GetIffRecords
(
    vector<FuseUtil::IFFRecord>* pRecords,
    bool keepDeadRecords
)
{
    RC rc;
    vector<UINT32> fuseBlock;
    CHECK_RC(GetCachedFuseReg(&fuseBlock));

    UINT32 iff = static_cast<UINT32>(fuseBlock.size());
    for (UINT32 i = m_FuseSpec.FuselessStart / REPLACE_RECORD_SIZE;
        i < m_FuseSpec.NumOfFuseReg; i++)
    {
        UINT32 row = fuseBlock[i];

        // Check the IFF bit to see if this row is an IFF record
        // Anything at or past an IFF record is IFF space
        if ((row & (1 << CHAINID_SIZE)) != 0)
        {
            iff = i;
            break;
        }
    }

    for (; iff < fuseBlock.size(); iff++)
    {
        pRecords->push_back(fuseBlock[iff]);
    }

    if (!keepDeadRecords)
    {
        for (iff = 0; iff < pRecords->size(); iff++)
        {
            UINT32 opCode = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _CMD, _OP, (*pRecords)[iff].data);
            switch (opCode)
            {
                case IFF_OPCODE(MODIFY_DW): iff += 2; break;
                case IFF_OPCODE(WRITE_DW): iff++; break;
                case IFF_OPCODE(SET_FIELD): break;

                case IFF_OPCODE(NULL):
                case IFF_OPCODE(INVALID):
                    pRecords->erase(pRecords->begin() + iff);
                    iff--;
                    break;

                default:
                    Printf(Tee::PriDebug, "Unknown IFF record: %#x\n", (*pRecords)[iff].data);
                    break;
            }
        }
    }
    return RC::OK;
}

/* virtual */ RC GM10xFuse::DecodeIff()
{
    // Only supported in non-official builds
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    RC rc;
    vector<FuseUtil::IFFRecord> iffRecords;

    CHECK_RC(GetIffRecords(&iffRecords, true));
    CHECK_RC(DecodeIffRecords(iffRecords));

    return rc;
}

RC GM10xFuse::DecodeSkuIff(string skuName)
{
    // Only supported in non-official builds
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    const FuseUtil::SkuConfig* skuConfig = GetSkuSpecFromName(skuName);

    if (!skuConfig)
    {
        Printf(Tee::PriError, "Cannot find sku %s\n", skuName.c_str());
        return RC::BAD_PARAMETER;
    }

    vector<FuseUtil::IFFRecord> iffRecords;
    if (skuConfig->iffPatches.empty())
    {
        // Legacy IFF from FuseXML
        for (const auto& fuseInfo : skuConfig->FuseList)
        {
            string fuseName = Utility::ToLowerCase(fuseInfo.pFuseDef->Name);
            if (fuseName.compare(0, 4, "iff_") == 0)
            {
                UINT64 index = Utility::Strtoull(fuseName.substr(4).c_str(), nullptr, 10);
                if (index >= iffRecords.size())
                {
                    iffRecords.resize(index + 1);
                }
                iffRecords[index] = fuseInfo.Value;
            }
        }
        // Reverse the order (IFF_0 is the last IFF record)
        for (UINT32 i = 0; i < iffRecords.size() / 2; i++)
        {
            auto temp = iffRecords[i];
            iffRecords[i] = iffRecords[iffRecords.size() - i - 1];
            iffRecords[iffRecords.size() - i - 1] = temp;
        }
    }
    else
    {
        list<FuseUtil::IFFRecord> records;

        // Combine the patches
        list<FuseUtil::IFFInfo>::const_iterator listItr;
        for (auto iffPatch : skuConfig->iffPatches)
        {
            records.insert(records.end(),
                iffPatch.rows.begin(), iffPatch.rows.end());
        }

        // insert in reverse order since iff patches are specified bottom up
        for (auto rIter = records.rbegin();
             rIter != records.rend();
             rIter++)
        {
            iffRecords.push_back(*rIter);
        }
    }

    return DecodeIffRecords(iffRecords);
}

/* virtual */ RC GM10xFuse::DecodeIffRecords(const vector<FuseUtil::IFFRecord>& iffRecords)
{
    // Decode information found in dev_bus.ref (most up-to-date) and
    // //hw/doc/gpu/maxwell/Ilwestigation_phase/Host/Maxwell_Host_IFF_uArch.doc
    // (a bit easier to read (MS Word formatting instead of ASCII),
    // but not the authoritative reference)

    Printf(Tee::PriNormal, "IFF Commands:\n");

    UINT32 commandNum = 0;
    for (UINT32 row = 0; row < iffRecords.size(); row++)
    {
        UINT32 record = iffRecords[row].data;
        if (record == 0 || record == 0xffffffff)
        {
            Printf(Tee::PriDebug, "Dead row. Continuing...\n");
            continue;
        }
        UINT32 opCode = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _CMD, _OP, record);
        switch (opCode)
        {
            case IFF_OPCODE(MODIFY_DW):
            {
                if (row > iffRecords.size() - 3)
                {
                    Printf(Tee::PriError,
                        "Not enough rows left for record type %x!\n", opCode);
                    return RC::FUSE_VALUE_OUT_OF_RANGE;
                }
                UINT32 addr = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _CMD_MODIFY_DW, _ADDR, record);
                addr <<= IFF_RECORD(CMD_MODIFY_DW_ADDR_BYTESHIFT);
                auto andMask = iffRecords[++row];
                auto orMask = iffRecords[++row];

                string decode = Utility::StrPrintf("%2d - Modify: [0x%06x] =", commandNum++, addr);

                if (andMask.dontCare || orMask.dontCare)
                {
                    Printf(Tee::PriNormal, "%s <ATE value>\n", decode.c_str());
                }
                else
                {
                    Printf(Tee::PriNormal, "%s ([0x%06x] & 0x%08x) | 0x%08x\n",
                           decode.c_str(), addr, andMask.data, orMask.data);
                }
            }
            break;
            case IFF_OPCODE(WRITE_DW):
            {
                if (row > iffRecords.size() - 2)
                {
                    Printf(Tee::PriError,
                        "Not enough rows left for record type %x!\n", opCode);
                    return RC::FUSE_VALUE_OUT_OF_RANGE;
                }

                UINT32 addr = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _CMD_WRITE_DW, _ADDR, record);
                addr <<= IFF_RECORD(CMD_WRITE_DW_ADDR_BYTESHIFT);
                auto record = iffRecords[++row];

                string decode = Utility::StrPrintf("%2d - Write:  [0x%06x] =", commandNum++, addr);

                if (record.dontCare)
                {
                    Printf(Tee::PriNormal, "%s <ATE value>\n", decode.c_str());
                }
                else
                {
                    Printf(Tee::PriNormal, "%s 0x%08x\n", decode.c_str(), record.data);
                }
            }
            break;
            case IFF_OPCODE(SET_FIELD):
            {
                UINT32 space = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _CMD_SET_FIELD, _SPACE, record);

                if (m_SetFieldSpaceAddr.find(space) == m_SetFieldSpaceAddr.end())
                {
                    Printf(Tee::PriError, "Invalid IFF Space value: %u\n", space);
                    return RC::FUSE_VALUE_OUT_OF_RANGE;
                }

                UINT32 addr = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _CMD_SET_FIELD, _ADDR, record);
                addr <<= IFF_RECORD(CMD_SET_FIELD_ADDR_BYTESHIFT);
                UINT32 bitIndex = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _CMD_SET_FIELD, _BITINDEX, record);
                UINT32 fieldSpec = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _CMD_SET_FIELD, _FIELDSPEC, record);

                // Most significant set bit of the field spec determines the size
                // Ex: 0b0100110 - the MSB set is bit 5, thus the data size is 5 bits (data = 0b00110)
                UINT32 fieldWidth = 6;
                for (; fieldWidth > 0; fieldWidth--)
                {
                    if (fieldSpec & (1 << fieldWidth))
                    {
                        break;
                    }
                }

                UINT32 orMask = (fieldSpec & ~(1 << fieldWidth)) << bitIndex;
                UINT32 andMask = ~(((1 << fieldWidth) - 1) << bitIndex);

                string spaceName = m_SetFieldSpaceName[space];
                addr |= m_SetFieldSpaceAddr[space];

                Printf(Tee::PriNormal,
                    "%2d - Set Field: (%s) [0x%06x] = ([0x%06x] & 0x%08x) | 0x%08x\n",
                    commandNum++, spaceName.c_str(), addr, addr, andMask, orMask);

            }
            break;
            case IFF_OPCODE(NULL):
            case IFF_OPCODE(INVALID):
                Printf(Tee::PriDebug, "Null/Ilwalidated IFF record\n");
                break;
            default:
                Printf(Tee::PriError, "Unknown IFF type: 0x%02x!\n", opCode);
                return RC::FUSE_VALUE_OUT_OF_RANGE;
        }
    }
    Printf(Tee::PriNormal, "\n");
    return RC::OK;
}

/* virtual */ RC GM10xFuse::ClearPrivSelwritySwFuse()
{
    RC rc;

    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();
    UINT32 PrivSecSwFuseValue = pSubdev->RegRd32(LW_FUSE_OPT_PRIV_SEC_EN);
    pSubdev->RegWr32(LW_FUSE_OPT_PRIV_SEC_EN,
                     FLD_SET_DRF(_FUSE,
                                 _OPT_PRIV_SEC_EN,
                                 _DATA,
                                 _NO,
                                 PrivSecSwFuseValue));
    return rc;
}

/* virtual */ RC GM10xFuse::CachePrivSelwritySwFuse()
{
    RC rc;

    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();
    m_PrivSecSwFuseValue = pSubdev->RegRd32(LW_FUSE_OPT_PRIV_SEC_EN);
    return rc;
}

/* virtual */ RC GM10xFuse::RestorePrivSelwritySwFuse()
{
    RC rc;

    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();
    pSubdev->RegWr32(LW_FUSE_OPT_PRIV_SEC_EN, m_PrivSecSwFuseValue);
    return rc;
}

//-----------------------------------------------------------------------------
// After burning fuse, optionally verify with "margined" read instead of
// normal read.  This is intended to predict a fuse failure sooner.
RC GM10xFuse::BlowFuseRows
(
    const vector<UINT32> &NewBitsArray,
    const vector<UINT32> &RegsToBlow,
    FuseSource*           pFuseSrc,
    FLOAT64               TimeoutMs
)
{
    RC rc;
    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();
    // make fuse visible
    ScopedRegister FuseCtrl(pSubdev, LW_FUSE_FUSECTRL);

    pSubdev->RegWr32(LW_FUSE_FUSECTRL,
                     FLD_SET_DRF(_FUSE,
                                 _FUSECTRL,
                                 _CMD,
                                 _SENSE_CTRL,
                                 FuseCtrl));

    CHECK_RC(POLLWRAP_HW(PollFusesIdleWrapper, this, TimeoutMs));

    SetFuseVisibility(true);
    bool FuseSrcHigh = false;

    for (UINT32 i = 0; i < m_FuseSpec.NumOfFuseReg; i++)
    {
        if (Utility::CountBits(NewBitsArray[i]) == 0)
        {
            // don't bother attempting to blow this row if there's no new bits
            continue;
        }

        // wait until state is idle
        CHECK_RC(POLLWRAP_HW(PollFusesIdleWrapper, this, TimeoutMs));

        // set the Row of the fuse
        UINT32 FuseAddr = pSubdev->RegRd32(LW_FUSE_FUSEADDR);
        FuseAddr = FLD_SET_DRF_NUM(_FUSE, _FUSEADDR, _VLDFLD, i, FuseAddr);
        pSubdev->RegWr32(LW_FUSE_FUSEADDR, FuseAddr);

        // write the new fuse bits in
        pSubdev->RegWr32(LW_FUSE_FUSEWDATA, NewBitsArray[i]);

        if (!FuseSrcHigh)
        {
            // Flick FUSE_SRC to high
            CHECK_RC(pFuseSrc->Toggle(FuseSource::TO_HIGH));
            Tasker::Sleep(m_FuseSpec.SleepTime[FuseSpec::SLEEP_AFTER_GPIO_HIGH]);
            FuseSrcHigh = true;
        }

        // issue the Write command
        pSubdev->RegWr32(LW_FUSE_FUSECTRL,
                         FLD_SET_DRF(_FUSE, _FUSECTRL, _CMD, _WRITE, FuseCtrl));
        CHECK_RC(POLLWRAP_HW(PollFusesIdleWrapper, this, TimeoutMs));

        UINT32 FuseReg;
        GetFuseWord(i, TimeoutMs, &FuseReg);

        // see of fuse row is what we expected
        if (FuseReg != RegsToBlow[i])
        {
            Printf(Tee::PriNormal, "Misblow @ row:%d. Got 0x%x, expected 0x%x\n",
                   i, FuseReg, RegsToBlow[i]);
            return RC::VECTORDATA_VALUE_MISMATCH;
        }
    }
    return rc;
}
//-----------------------------------------------------------------------------
RC GM10xFuse::GetFuseWord(UINT32 FuseRow, FLOAT64 TimeoutMs, UINT32 *pRetData)
{
    MASSERT(pRetData);
    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();
    ScopedRegister FuseCtrl(pSubdev, LW_FUSE_FUSECTRL);

    if (IsMarginReadEnabled())
    {
        // start the 'margin read' HW
        pSubdev->RegWr32(LW_FUSE_FUSECTRL,
                         FLD_SET_DRF(_FUSE,_FUSECTRL,
                                     _MARGIN_READ,
                                     _ENABLE,
                                     FuseCtrl));
    }

    RC rc = GpuFuse::GetFuseWord(FuseRow, TimeoutMs, pRetData);

    if (IsMarginReadEnabled())
    {
        // disable the 'margin read' HW
        pSubdev->RegWr32(LW_FUSE_FUSECTRL,
                         FLD_SET_DRF(_FUSE,
                                     _FUSECTRL,
                                     _MARGIN_READ,
                                     _DISABLE,
                                     FuseCtrl));
    }
    return rc;
}
//-----------------------------------------------------------------------------
const FuseUtil::FuseDef* GM10xFuse::GetSwFusingFuseDef()
{
    return GetFuseDefFromName("SW_OVERRIDE_FUSE");
}
//-----------------------------------------------------------------------------
RC GM10xFuse::SetSwFuse(const FuseUtil::FuseDef* pDef, UINT32 Value)
{
    MASSERT(pDef);
    // write to SW register
    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();
    SetFuseVisibility(true);
    UINT32 SwOverride = pSubdev->RegRd32(LW_FUSE_EN_SW_OVERRIDE);
    pSubdev->RegWr32(LW_FUSE_EN_SW_OVERRIDE,
                     FLD_SET_DRF(_FUSE,
                                 _EN_SW_OVERRIDE,
                                 _VAL,
                                 _ENABLE,
                                 SwOverride));

    FuseUtil::OffsetInfo FuseOffsetInfo = pDef->PrimaryOffset;
    if (FuseOffsetInfo.Offset == FuseUtil::UNKNOWN_OFFSET)
    {
        Printf(Tee::PriNormal, "%s has no OPT fuse register. Bypassing\n",
               pDef->Name.c_str());
        return RC::OK;
    }

    if (pDef->Type == "dis")
    {
        // for disable fuse types, a 1 in raw fuse = 1 in OPT register.
        // Fuse XML fuse value definition refers to RAW fuse values
        UINT32 Mask = (1 << pDef->NumBits) - 1;
        Value = (~Value) & Mask;
    }

    // UNTIL they fix the XML in bug 836801
    // This hack assumes that all the priv fuse registers would start at LSB = 0
    // This is true for only GF119 and GF117...
    const UINT32 Msb = pDef->NumBits - 1;
    const UINT32 Lsb = 0;
    Printf(Tee::PriNormal, "%28s @ 0x%08x [%d:%d] to 0x%08x\n",
           pDef->Name.c_str(),
           FuseOffsetInfo.Offset,
           Msb,
           Lsb,
           Value);

    // this is kind of like DRF_MASK and DRF_VAL
    // Mask is the bits for the size of the fuse - eg. 4 bit fuse would be 0xF.
    // Fuse value may not always start at bit 0 of the fuse register, so need
    // to shift it by Lsb (of the fuse)
    UINT32 Mask =  0xFFFFFFFF >> (31 - Msb + Lsb);
    UINT32 ValToWrite = (Value & Mask) << Lsb;

    // write to SW register
    pSubdev->RegWr32(FuseOffsetInfo.Offset, ValToWrite);
    return RC::OK;
}

