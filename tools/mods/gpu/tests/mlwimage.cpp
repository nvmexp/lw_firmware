/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021  by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/crc.h"
#include "core/include/jscript.h"
#include "core/include/tee.h"
#include "device/interface/i2c.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/stm32f051x.h"
#include "gpu/tests/gputest.h"

/**
 * @file   mlwimage.cpp
 * @brief  Implemetation of a MLWImage test.
 *         This test will verify if the valid mlw image
 *         is flashed on the boards that have mlw device present
 *  
 * MLW communication is based on lwflash code from here: 
 *  
 *     https://p4sw-swarm.lwpu.com/files/sw/rel/gfw_tools/r5/lwflash/core/IGmacAccessor.h
 *     https://p4sw-swarm.lwpu.com/files/sw/rel/gfw_tools/r5/lwflash/core/GmacAccessorImpl.h
 */

class MLWImage : public GpuTest
{
    public:
        MLWImage();
        virtual ~MLWImage() { }

        bool IsSupported() override;
        void PrintJsProperties(Tee::Priority pri) override;
        RC Setup() override;
        RC Run() override;

        SETGET_PROP(MLWModeSwitchDelayMs,  UINT32);
        SETGET_PROP(MLWAccessDelayMs,      UINT32);
        SETGET_PROP(MLWModeSwitchRetries,  UINT32);
        SETGET_PROP(MLWBootloaderVersion,  UINT16);
        SETGET_PROP(MLWFirmwareDeviceId,   UINT16);
        SETGET_PROP(MLWApplicatiolwersion, string);
        SETGET_PROP(MLWProject,            string);
        SETGET_PROP(MLWSku,                string);
        SETGET_PROP(MLWSkuBranch,          string);
        GET_PROP(MLWBootloaderCrc,         UINT32);
        SET_PROP_LWSTOM(MLWBootloaderCrc,  UINT32);

    private:
        enum class MLWMode
        {
            Bootloader,
            Application
        };

        static const UINT32 GMAC_BOOTLOADER_BASE_ADDRESS   = 0;
        static const UINT32 GMAC_BOOTLOADER_SIZE           = STM32_FLASH_SECTOR_SIZE;
        static const UINT32 GMAC_ROM_BASE_ADDRESS          = STM32_FLASH_SECTOR_SIZE;
        static const UINT32 GMAC_ROM_HEADER_OFFSET         = 0U;
        static const UINT32 GMAC_PARAGRAPH_SIZE            = SMBUS_MAX_TRANSFER_LENGTH;

#pragma pack(push)  /// Push current alignment to stack
#pragma pack(1)     /// Set alignment to 1 byte boundary        
        struct MLWRomImageHeader
        {
            char   signature[4];
            UINT16 headerVersion;
            UINT08 headerSize;
            UINT08 reserved1[1];
            char   projectString[4];
            char   skuString[4];
            char   skuBranchString[2];
            char   versionString[2];
            UINT16 pciVendorIdentifier;
            UINT16 pciDeviceIdentifier;
            UINT16 stm32DeviceIdentifier;
            UINT16 imageVersion;
            UINT32 imageSize;
            UINT32 imageChecksum;
            UINT32 extendedDirectoryAddr;
            UINT08 reserved2[23];
            UINT08 headerChecksum;
        };
#pragma pack(pop)
        
        bool IsMLWDevicePresent();

        RC GetMLWMode(MLWMode *pMode);
        RC GetBootloaderCrc(UINT32 *pCrc);
        RC ReadMLWHeader(MLWRomImageHeader *pHeaderData);
        void PrintMLWHeader(const MLWRomImageHeader & headerData, Tee::Priority pri);
        RC ReadMLWBlock(UINT32 offset, UINT32 size, vector<UINT08> * pData);

        // See DCB specification wiki
        enum MLWDeviceType
        {
            I2C_GMAC = 0x98
        };

        I2c::I2cDcbDevInfoEntry m_MlwDevEntry = {};

        UINT32 m_MLWModeSwitchDelayMs  = 200;
        UINT32 m_MLWAccessDelayMs      = 2;
        UINT32 m_MLWModeSwitchRetries  = 10;
        UINT16 m_MLWBootloaderVersion  = 0x100;
        UINT16 m_MLWFirmwareDeviceId   = 0xFFFF;
        string m_MLWApplicatiolwersion = "14";
        string m_MLWProject            = "X999";
        string m_MLWSku                = "9999";
        string m_MLWSkuBranch          = "01"; 
        UINT32 m_MLWBootloaderCrc      = 0U;
        bool   m_MLWBootloaderCrcSet   = false;
};

#define CHAR_ARRAY_TO_STRING(ca) (string(ca, ca + sizeof(ca) / sizeof(ca[0])))

JS_CLASS_INHERIT(MLWImage, GpuTest, "MLW Image validation test");
CLASS_PROP_READWRITE(MLWImage, MLWModeSwitchDelayMs,  UINT32,
                     "Wait time in milliseconds after mode switch operations (default = 200)");
CLASS_PROP_READWRITE(MLWImage, MLWAccessDelayMs,      UINT32,
                     "Wait time in milliseconds after general access operations (default = 2)");
CLASS_PROP_READWRITE(MLWImage, MLWModeSwitchRetries,  UINT32,
                     "Number of times to retry a mode switch request (default = 10)");
CLASS_PROP_READWRITE(MLWImage, MLWBootloaderVersion,  UINT16,
                     "Bootloader version (default=0x100)");
CLASS_PROP_READWRITE(MLWImage, MLWFirmwareDeviceId,  UINT16,
                     "Firmware device ID (default=0xFFFF)");
CLASS_PROP_READWRITE(MLWImage, MLWApplicatiolwersion, string,
                     "Application version (default=\"09\")");
CLASS_PROP_READWRITE(MLWImage, MLWProject, string,
                     "Project (default=\"X999\")");
CLASS_PROP_READWRITE(MLWImage, MLWSku, string,
                     "SKU (default=\"9999\")");
CLASS_PROP_READWRITE(MLWImage, MLWSkuBranch, string,
                     "SKU Branch (default=\"01\")");
CLASS_PROP_READWRITE(MLWImage, MLWBootloaderCrc, UINT32,
                     "Override the MLW bootloader CRC with a custom CRC check (default=0");

//----------------------------------------------------------------------------
//! \brief Constructor
//!
MLWImage::MLWImage()
{
    SetName("MLWImage");
}

//----------------------------------------------------------------------------
//! \brief Determine if the MLWImage test is supported
//!
//! \return true if MLWImage test is supported, false if not
//!
bool MLWImage::IsSupported()
{
    GpuSubdevice *pGpuSubdev = GetBoundGpuSubdevice();
    if (pGpuSubdev->IsSOC())
    {
        return false;
    }

    if (!pGpuSubdev->SupportsInterface<I2c>())
    {
        return false;
    }

    if (pGpuSubdev->IsEmOrSim())
    {
        JavaScriptPtr pJs;
        bool sanityMode = false;
        pJs->GetProperty(pJs->GetGlobalObject(), "g_SanityMode", &sanityMode);
        if (!sanityMode)
        {
            Printf(Tee::PriLow, "Skipping MLWImage Test because"
                    "it is not supported on emulation/simulation\n");
            return false;
        }
    }

    return IsMLWDevicePresent();
}

//---------------------------------------------------------------------
//! \brief Print properties of the JS object for the class
//!
void MLWImage::PrintJsProperties(Tee::Priority pri)
{
    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tMLWModeSwitchDelayMs:           %u\n", m_MLWModeSwitchDelayMs);
    Printf(pri, "\tMLWAccessDelayMs:               %u\n", m_MLWAccessDelayMs);
    Printf(pri, "\tMLWModeSwitchRetries:           %u\n", m_MLWModeSwitchRetries);
    Printf(pri, "\tMLWBootloaderVersion:           %u\n", m_MLWBootloaderVersion);
    Printf(pri, "\tMLWFirmwareDeviceId:            0x%04x\n", m_MLWFirmwareDeviceId);
    Printf(pri, "\tMLWApplicatiolwersion:          %s\n", m_MLWApplicatiolwersion.c_str());
    Printf(pri, "\tMLWProject:                     %s\n", m_MLWProject.c_str());
    Printf(pri, "\tMLWSku:                         %s\n", m_MLWSku.c_str());
    Printf(pri, "\tMLWSkuBranch:                   %s\n", m_MLWSkuBranch.c_str());
    if (m_MLWBootloaderCrcSet)
        Printf(pri, "\tMLWBootloaderCrc:               0x%08x\n", m_MLWBootloaderCrc);
    GpuTest::PrintJsProperties(pri);
}

//----------------------------------------------------------------------------
//! \brief Setup the MLWImage test
//!
//! \return OK if setup was successful, not OK otherwise
//!
RC MLWImage::Setup()
{
    RC rc;
    CHECK_RC(GpuTest::Setup());

    I2c::I2cDcbDevInfo i2cDcbDevInfo;
    CHECK_RC(GetBoundGpuSubdevice()->GetInterface<I2c>()->GetI2cDcbDevInfo(&i2cDcbDevInfo));

    for (UINT32 dev= 0; dev < i2cDcbDevInfo.size(); ++dev)
    {
        if (i2cDcbDevInfo[dev].Type == I2C_GMAC)
        {
            m_MlwDevEntry = i2cDcbDevInfo[dev];
            break;
        }
    }

    MLWMode mlwMode;
    CHECK_RC(GetMLWMode(&mlwMode));
    if (mlwMode != MLWMode::Application)
    {
        Printf(Tee::PriError, "%s : MLW not booting to application mode!\n", GetName().c_str());
        return RC::HW_STATUS_ERROR;
    }

    return rc;
}

//----------------------------------------------------------------------------
namespace
{
    struct BootloaderCrcs
    {
        UINT32 crc;
        UINT16 version;
    };

    const BootloaderCrcs s_BootloaderCrcTable[] =
    {
        { 0x18C19B9A, 0x100 }
       ,{ 0xDBB6F06F, 0x100 }
    };
    const size_t s_BootloaderCrcTableSize = sizeof(s_BootloaderCrcTable) / sizeof(BootloaderCrcs);
}

//----------------------------------------------------------------------------
//! \brief Run the MLWImage test
//!
//! \return OK if the test was successful, not OK otherwise
//!
RC MLWImage::Run()
{
    RC rc;
    StickyRC validationRc;
    UINT32 bootloaderCrc;

    CHECK_RC(GetBootloaderCrc(&bootloaderCrc));
    if (m_MLWBootloaderCrcSet)
    {
        if (m_MLWBootloaderCrc != bootloaderCrc)
        {
            Printf(Tee::PriError,
                   "MLW Bootloader CRC (%08X) does not match provided CRC (%08X)\n",
                   bootloaderCrc,
                   m_MLWBootloaderCrc);
            validationRc = RC::PARAMETER_MISMATCH;
        }
    }
    else
    {
        const auto bootloaderEntry =
            find_if(s_BootloaderCrcTable,
                    s_BootloaderCrcTable + s_BootloaderCrcTableSize,
                    [&](const BootloaderCrcs & b) -> bool { return b.crc == bootloaderCrc; });
        if (bootloaderEntry == s_BootloaderCrcTable + s_BootloaderCrcTableSize)
        {
            Printf(Tee::PriError, "\n%s : Unknown MLW bootloader CRC (%08X), known CRCs are:\n"
                                  "     Version         CRC\n"
                                  "------------------------\n", GetName().c_str(), bootloaderCrc);
            for (size_t ii = 0; ii < s_BootloaderCrcTableSize; ii++)
            {
                Printf(Tee::PriNormal, "        %04X    %08X\n",
                       s_BootloaderCrcTable[ii].version,
                       s_BootloaderCrcTable[ii].crc);
            }
            validationRc = RC::PARAMETER_MISMATCH;
        }
        else if (bootloaderEntry->version != m_MLWBootloaderVersion)
        {
            Printf(Tee::PriError,
                   "MLW Bootloader Version Mismatch: Expected = %04X, Actual = %04X\n",
                   m_MLWBootloaderVersion, bootloaderEntry->version);
            validationRc = RC::PARAMETER_MISMATCH;
        }
    }

    MLWRomImageHeader header;
    CHECK_RC(ReadMLWHeader(&header));
    if (header.pciDeviceIdentifier != m_MLWFirmwareDeviceId)
    {
        Printf(Tee::PriError,
               "MLW PCI Device ID Mismatch : Expected = %04X (GENERIC), Actual = %04X\n",
               m_MLWFirmwareDeviceId, header.pciDeviceIdentifier);
        validationRc = RC::PARAMETER_MISMATCH;
    }

    string tempString = CHAR_ARRAY_TO_STRING(header.versionString);
    if (tempString != m_MLWApplicatiolwersion)
    {
        Printf(Tee::PriError,
               "MLW Application Version Mismatch : Expected = %s, Actual = %s\n",
               m_MLWApplicatiolwersion.c_str(), tempString.c_str());
        validationRc = RC::PARAMETER_MISMATCH;
    }

    tempString = CHAR_ARRAY_TO_STRING(header.projectString);
    if (tempString != m_MLWProject)
    {
        Printf(Tee::PriError,
               "MLW Project Mismatch : Expected = %s, Actual = %s\n",
               m_MLWProject.c_str(), tempString.c_str());
        validationRc = RC::PARAMETER_MISMATCH;
    }

    tempString = CHAR_ARRAY_TO_STRING(header.skuString);
    if (tempString != m_MLWSku)
    {
        Printf(Tee::PriError,
               "MLW SKU Mismatch : Expected = %s, Actual = %s\n",
               m_MLWSku.c_str(), tempString.c_str());
        validationRc = RC::PARAMETER_MISMATCH;
    }

    tempString = CHAR_ARRAY_TO_STRING(header.skuBranchString);
    if (tempString != m_MLWSkuBranch)
    {
        Printf(Tee::PriError,
               "MLW SKU Branch Mismatch : Expected = %s, Actual = %s\n",
               m_MLWSkuBranch.c_str(), tempString.c_str());
        validationRc = RC::PARAMETER_MISMATCH;
    }
    PrintMLWHeader(header, (validationRc != RC::OK) ? Tee::PriError : GetVerbosePrintPri());
    return (rc == RC::OK) ? validationRc : rc;
}

//-----------------------------------------------------------------------------
RC MLWImage::SetMLWBootloaderCrc(UINT32 crc)
{
    m_MLWBootloaderCrc    = crc;
    m_MLWBootloaderCrcSet = true;
    return RC::OK;
}

//----------------------------------------------------------------------------
//! \brief Check if the GMAC microcontroller is present on the board
//!
//! \return true if present, false if not
//!
bool MLWImage::IsMLWDevicePresent()
{
    RC rc;
    I2c::I2cDcbDevInfo i2cDcbDevInfo;
    rc = GetBoundGpuSubdevice()->GetInterface<I2c>()->GetI2cDcbDevInfo(&i2cDcbDevInfo);

    if (rc != OK)
    {
        Printf(Tee::PriLow, "Failed to retrieve i2c device info from DCB\n");
        return false;
    }

    for (UINT32 dev= 0; dev < i2cDcbDevInfo.size(); ++dev)
    {
        if (i2cDcbDevInfo[dev].Type == I2C_GMAC)
        {
            return true;
        }
    }

    Printf(Tee::PriLow,
            "No MLW device found on dev:%u subdev:%u\n",
            GetBoundGpuDevice()->GetDeviceInst(),
            GetBoundGpuSubdevice()->GetSubdeviceInst());

    return false;
}

//----------------------------------------------------------------------------
RC MLWImage::GetMLWMode(MLWMode *pMode)
{
    MASSERT(pMode);

    RC rc;
    I2c::Dev i2cDev =
        GetBoundGpuSubdevice()->GetInterface<I2c>()->I2cDev(m_MlwDevEntry.I2CLogicalPort,
                                                            m_MlwDevEntry.I2CAddress);
    vector<UINT08> data = { 0 };
    CHECK_RC(i2cDev.ReadBlk(LW_SMBUS_GET_MODE, 1, &data));
    Tasker::Sleep(m_MLWAccessDelayMs);

    if (FLD_TEST_DRF(_SMBUS, _DATA, _MODE, _BOOTLOADER, data[0]))
        *pMode = MLWMode::Bootloader;
    else
        *pMode = MLWMode::Application;
    return rc;
}

RC MLWImage::GetBootloaderCrc(UINT32 *pCrc)
{
    MASSERT(pCrc);

    RC rc;

    vector<UINT08> bootloaderData;
    CHECK_RC(ReadMLWBlock(GMAC_BOOTLOADER_BASE_ADDRESS,
                          GMAC_BOOTLOADER_SIZE,
                          &bootloaderData));

    *pCrc = Crc::Callwlate(&bootloaderData[0],
                           static_cast<UINT32>(bootloaderData.size()) / sizeof(UINT32),
                           1,
                           32,
                           GMAC_BOOTLOADER_SIZE);

    return rc;
}

//----------------------------------------------------------------------------
RC MLWImage::ReadMLWHeader(MLWRomImageHeader *pHeaderData)
{
    MASSERT(pHeaderData);

    RC rc;
    vector<UINT08> headerData;

    CHECK_RC(ReadMLWBlock(GMAC_ROM_BASE_ADDRESS + GMAC_ROM_HEADER_OFFSET,
                          sizeof(MLWRomImageHeader),
                          &headerData));

    // Copy the header data into the overlay structure
    UINT08 *pData = reinterpret_cast<UINT08 *>(pHeaderData);
    copy(headerData.begin(), headerData.end(), pData);

    return rc;
}

//----------------------------------------------------------------------------
RC MLWImage::ReadMLWBlock(UINT32 offset, UINT32 size, vector<UINT08> * pData)
{
    MASSERT(pData);

    RC rc;
    UINT32 lwrOffset = offset % STM32_FLASH_PAGE_SIZE;
    UINT32 lwrPage   = offset / STM32_FLASH_PAGE_SIZE;
    UINT32 remainingSize = size;

    I2c::Dev i2cDev =
        GetBoundGpuSubdevice()->GetInterface<I2c>()->I2cDev(m_MlwDevEntry.I2CLogicalPort,
                                                            m_MlwDevEntry.I2CAddress);

    pData->clear();

    // When accessing the MLW in Application mode, it is required to access by
    // page and paragraph.  Handle this generically here so that if a header ever is
    // not page or paragraph aligned it can still be retrieved
    vector<UINT08> paragraphData(GMAC_PARAGRAPH_SIZE, 0);
    for (; remainingSize != 0; ++lwrPage)
    {
        // 1. Setup the current page to access in the MLW
        vector<UINT08> pageData =
        {
            static_cast<UINT08>(lwrPage & 0xFF),
            static_cast<UINT08>((lwrPage >> 8) & 0xFF)
        };
        CHECK_RC(i2cDev.WriteBlk(LW_SMBUS_SET_READ_PAGE, 2, pageData));
        Tasker::Sleep(m_MLWAccessDelayMs);

        // 2. Read all data from the header in the current page
        UINT32 remainingSizeInPage = min(remainingSize, STM32_FLASH_PAGE_SIZE - lwrOffset);
        while (remainingSizeInPage)
        {
            // 3. Read the paragraph in the page that contains the current
            // offset being read
            const UINT32 paragraph = lwrOffset / GMAC_PARAGRAPH_SIZE;
            CHECK_RC(i2cDev.ReadBlk(LW_SMBUS_READ_PAGE(paragraph),
                                    GMAC_PARAGRAPH_SIZE,
                                    &paragraphData));
            Tasker::Sleep(m_MLWAccessDelayMs);

            // 4. Callwlate how much valid data is in the paragraph and where in
            // the paragraph to start copying
            const UINT32 paragraphDataSize =
                min(GMAC_PARAGRAPH_SIZE - (lwrOffset % GMAC_PARAGRAPH_SIZE), remainingSize);
            auto paragraphDataStart = paragraphData.begin() + (lwrOffset % GMAC_PARAGRAPH_SIZE);

            // 5. Copy the data into the final data array
            pData->insert(pData->end(),
                          paragraphDataStart,
                          paragraphDataStart + paragraphDataSize);

            // 6. Update variable tracking header retrieval progress
            remainingSize       -= paragraphDataSize;
            remainingSizeInPage -= paragraphDataSize;
            lwrOffset            = (lwrOffset + paragraphDataSize) % STM32_FLASH_PAGE_SIZE;
            if (remainingSize > 0) 
                fill(paragraphData.begin(), paragraphData.end(), 0);
        }
    }

    return rc;
}
//----------------------------------------------------------------------------
void MLWImage::PrintMLWHeader(const MLWRomImageHeader & headerData, Tee::Priority pri)
{
    Printf(pri, "MLW Header:\n"
                "  Signature           : %s\n"
                "  Header Version      : %04X\n"
                "  Header Size         : 0x%02X (%u) bytes\n"
                "  Project             : %s\n"
                "  SKU                 : %s\n"
                "  SKU Branch          : %s\n"
                "  APP Version         : %s\n"
                "  PCI Vendor ID       : %04X\n"
                "  PCI Device ID       : %04X\n"
                "  STM32 Device ID     : %04X\n"
                "  Image Version       : %04X\n"
                "  Image Size          : 0x%X (%u) bytes\n"
                "  Image Checksum      : %X\n"
                "  Header Checksum     : %X\n"
                "  MFED Address        : %s\n",
                CHAR_ARRAY_TO_STRING(headerData.signature).c_str(),
                headerData.headerVersion,
                headerData.headerSize, headerData.headerSize,
                CHAR_ARRAY_TO_STRING(headerData.projectString).c_str(),
                CHAR_ARRAY_TO_STRING(headerData.skuString).c_str(),
                CHAR_ARRAY_TO_STRING(headerData.skuBranchString).c_str(),
                CHAR_ARRAY_TO_STRING(headerData.versionString).c_str(),
                headerData.pciVendorIdentifier,
                headerData.pciDeviceIdentifier,
                headerData.stm32DeviceIdentifier,
                headerData.imageVersion,
                headerData.imageSize, headerData.imageSize,
                headerData.imageChecksum,
                headerData.headerChecksum,
                headerData.extendedDirectoryAddr == 0 ?
                    "Unsupported" :
                    Utility::StrPrintf("%X", headerData.extendedDirectoryAddr).c_str());
}
