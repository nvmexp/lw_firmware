/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "vktexture.h"
#include "core/include/crc.h"
#include "core/include/log.h"
#include "core/include/memoryif.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"

#ifndef VULKAN_STANDALONE
#include "vulkan/shared_sources/lwbc7/FastCompressBC7.h"

class VulkanTextureArchive
{
public:
    VulkanTextureArchive() = default;
    VulkanTextureArchive(const VulkanTextureArchive&) = delete;
    VulkanTextureArchive& operator=(const VulkanTextureArchive&) = delete;
    ~VulkanTextureArchive();

    VkResult GetCompressedTexture
    (
        VkFormat inputFormat,
        VkFormat outputFormat,
        UINT32 height,
        UINT32 width,
        UINT64 inputPitch,
        UINT64 outputPitch,
        const UINT08 *inputTexture,
        vector<UINT08> *outputTexture
    );

private:
    Tasker::Mutex m_Mutex = Tasker::CreateMutex("TextureArchiveMutex", Tasker::mtxUnchecked);
    TarArchive m_TarArchive;
    const char *m_TarArchiveFilename = "vktextures.bin";
    const char *m_TarArchiveFilenameNew = "vktextures_new.bin";
    bool m_TarArchiveInitialized = false;
    bool m_WriteNewArchive = false;

    bool InitializeTarArchive();
};

static VulkanTextureArchive s_VulkanTextureArchive;

VulkanTextureArchive::~VulkanTextureArchive()
{
    if (m_WriteNewArchive)
    {
        m_TarArchive.WriteToFile(m_TarArchiveFilenameNew);
    }
}

VkResult VulkanTextureArchive::GetCompressedTexture
(
    VkFormat inputFormat,
    VkFormat outputFormat,
    UINT32 height,
    UINT32 width,
    UINT64 inputPitch,
    UINT64 outputPitch,
    const UINT08 *inputTexture,
    vector<UINT08> *outputTexture
)
{
    if ((inputFormat != VK_FORMAT_B8G8R8A8_UNORM) &&
        (inputFormat != VK_FORMAT_R8G8B8A8_UNORM))
    {
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }
    const UINT32 inputBPP = 4;
    if (outputFormat != VK_FORMAT_BC7_UNORM_BLOCK)
    {
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    const UINT32 numTilesH = (width + 3) / 4;
    if (numTilesH * 16 > outputPitch)
    {
        return VK_ERROR_UNKNOWN;
    }
    const UINT32 numTilesV = (height + 3) / 4;

    const UINT32 inputCRC = Crc32c::Callwlate(inputTexture, inputBPP * width, height, inputPitch);
    const string textureFilename = Utility::StrPrintf("bc7_%d_%d_%08x.bin",
        width, height, inputCRC);

    Tasker::MutexHolder mutexHolder(m_Mutex);
    if (!InitializeTarArchive())
    {
        return VK_ERROR_UNKNOWN;
    }
    TarFile *tarFile = m_TarArchive.Find(textureFilename);
    mutexHolder.Release();

    if (tarFile == nullptr)
    {
        unique_ptr<TarFile> newTarFile = make_unique<TarFile_BCImp>(numTilesV * numTilesH * 16);

        newTarFile->SetFileName(textureFilename);

        Printf(Tee::PriWarn, "Compressing texture %s ...\n", textureFilename.c_str());
        bool compressionAllowed = true;
        JavaScriptPtr pJs;
        bool skipCompressionCheck = false;
        pJs->GetProperty(pJs->GetGlobalObject(), "g_SkipCompressionCheck", &skipCompressionCheck);
        if (!skipCompressionCheck)
        {
            string pvsMachineName = "null";
            pJs->GetProperty(pJs->GetGlobalObject(), "g_PvsMachineName", &pvsMachineName);
            if (pvsMachineName != "null")
            {
                compressionAllowed = false;
                Printf(Tee::PriError,
                    "Texture %s compression is not allowed in DVS, regenarate vktextures.bin locally\n",
                    textureFilename.c_str());
                RC rc = RC::CANNOT_PARSE_FILE;
                ErrorMap error(rc);
                Log::SetFirstError(error);
            }
        }

        float colors[64];
        char outputBlock[16];
        for (UINT32 tileY = 0; tileY < numTilesV; tileY++)
        {
            for (UINT32 tileX = 0; tileX < numTilesH; tileX++)
            {
                const UINT64 inputOffset = tileY * 4 * inputPitch +
                    tileX * 4 * inputBPP;

                const UINT08 *input = &inputTexture[inputOffset];

                const UINT32 widthLeft = width - tileX * 4;
                const UINT32 blockSizeX = (widthLeft >= 4) ? 4 : widthLeft;

                const UINT32 heightLeft = height - tileY * 4;
                const UINT32 blockSizeY = (heightLeft >= 4) ? 4 : heightLeft;

                float *lwrColor = colors;
                for (UINT32 y = 0; y < blockSizeY; y++)
                {
                    for (UINT32 x = 0; x < blockSizeX; x++)
                    {
                        switch (inputFormat)
                        {
                            case VK_FORMAT_R8G8B8A8_UNORM:
                                lwrColor[0] = *input++ / 255.0f;
                                lwrColor[1] = *input++ / 255.0f;
                                lwrColor[2] = *input++ / 255.0f;
                                lwrColor[3] = *input++ / 255.0f;
                                break;
                            case VK_FORMAT_B8G8R8A8_UNORM:
                                lwrColor[2] = *input++ / 255.0f;
                                lwrColor[1] = *input++ / 255.0f;
                                lwrColor[0] = *input++ / 255.0f;
                                lwrColor[3] = *input++ / 255.0f;
                                break;
                            default:
                                return VK_ERROR_FORMAT_NOT_SUPPORTED;
                        }

                        lwrColor += 4;
                    }
                    input += inputPitch - blockSizeX * inputBPP;
                }
                lwtt_internal::CompressBC7Block(true, colors, outputBlock, blockSizeX, blockSizeY);

                newTarFile->Write(outputBlock, sizeof(outputBlock));
            }
        }

        mutexHolder.Acquire(m_Mutex.get());
        m_TarArchive.AddToArchive(move(newTarFile));
        if (compressionAllowed)
        {
            // The "if" is needed to prevent intermittent failures in PVS where
            // gputest--1 would FAIL but compress the textures and runs gputest--2 and 3
            // would PASS.
            m_WriteNewArchive = true;
        }
        tarFile = m_TarArchive.Find(textureFilename);
        mutexHolder.Release();

        MASSERT(tarFile);
    }

    if (tarFile->GetSize() != (numTilesV * numTilesH * 16))
    {
        return VK_ERROR_UNKNOWN;
    }

    outputTexture->resize(numTilesV * outputPitch);
    tarFile->Seek(0);
    char *output = reinterpret_cast<char*>(outputTexture->data());
    for (UINT32 tileY = 0; tileY < numTilesV; tileY++)
    {
        tarFile->Read(output, 16 * numTilesH);
        output += outputPitch;
    }

    return VK_SUCCESS;
}

bool VulkanTextureArchive::InitializeTarArchive()
{
    MASSERT(Tasker::CheckMutexOwner(m_Mutex.get()));

    if (m_TarArchiveInitialized)
        return true;

    string archiveLocation = Utility::DefaultFindFile(m_TarArchiveFilenameNew, true);
    if (Xp::DoesFileExist(archiveLocation))
    {
        Printf(Tee::PriWarn, "Loading compressed textures from %s ...\n", archiveLocation.c_str());
    }
    else
    {
        archiveLocation = Utility::DefaultFindFile(m_TarArchiveFilename, true);
        if (!Xp::DoesFileExist(archiveLocation))
        {
            Printf(Tee::PriWarn, "File %s not found\n", archiveLocation.c_str());
            m_TarArchiveInitialized = true;
            return true;
        }
    }

    m_TarArchiveInitialized = m_TarArchive.ReadFromFile(archiveLocation);
    if (!m_TarArchiveInitialized)
    {
        Printf(Tee::PriError,
            "Unable to load %s. Delete the file to regenerate it\n",
            archiveLocation.c_str());
    }
    return m_TarArchiveInitialized;
}
#endif

using namespace VkMods;

/*static*/ bool VulkanTexture::m_OverrideTextureMapping = false;

//--------------------------------------------------------------------
VulkanTexture::VulkanTexture(VulkanDevice* pVulkanDev) :
    m_pVulkanDev(pVulkanDev)
   ,m_TexImage(pVulkanDev)
{}

//--------------------------------------------------------------------
VulkanTexture::~VulkanTexture()
{
    Cleanup();
}

//--------------------------------------------------------------------
bool VulkanTexture::IsSamplingSupported() const
{
    return m_TexImage.IsSamplingSupported();
}

//--------------------------------------------------------------------
VkResult VulkanTexture::AllocateImage
(
    VkImageUsageFlags usageFlags,
    VkImageCreateFlags createFlags,
    VkMemoryPropertyFlags memoryFlag,
    VkImageLayout layout,
    VkImageTiling tiling,
    const char * imageName
)
{
    VkResult res = VK_SUCCESS;
    const auto biggestDim = m_Width > m_Height ? m_Width : m_Height;
    // If the largest dimension is a power of 2, we log2(dim)+1 levels
    // (e.g dim=8 -> 4 levels). If it is not a power of 2, we need to round
    // down (e.g dim=10 -> 4 levels (10, 5, 2, 1))
    UINT08 numMipMapLevels = m_UseMipMap ?
                        1 + static_cast<UINT08>(floor(log2(biggestDim)))
                        : 1;

    if (m_Format == VK_FORMAT_BC7_UNORM_BLOCK)
    {
        // MipMap subresource can't be smaller than the compressed tile (4x4):
        while (numMipMapLevels > 1)
        {
            if ( (m_Width / (1 << numMipMapLevels)) < 4)
            {
                numMipMapLevels--;
                continue;
            }
            if ( (m_Height / (1 << numMipMapLevels)) < 4)
            {
                numMipMapLevels--;
                continue;
            }
            break;
        }
    }

    MASSERT((m_Width  != 0) &&
            (m_Height != 0) &&
            (m_Depth  != 0) &&
            (m_FormatPixelSizeBytes != 0) &&
            "Image dimensions must be set before memory can be allocated");

    m_TexImage.SetFormat(m_Format);
    m_TexImage.SetImageProperties(m_Width,
                                  m_Height,
                                  numMipMapLevels,
                                  VulkanImage::COLOR);

    CHECK_VK_RESULT(m_TexImage.CreateImage(
        createFlags,
        VK_IMAGE_USAGE_SAMPLED_BIT | usageFlags,
        layout,
        tiling,
        VK_SAMPLE_COUNT_1_BIT,
        imageName));

    CHECK_VK_RESULT(m_TexImage.AllocateAndBindMemory(memoryFlag));
    CHECK_VK_RESULT(m_TexImage.CreateImageView());
    return res;
}

//--------------------------------------------------------------------
VkResult CreatePlainColorImages(vector<VulkanImage>* pImages,
                          VulkanDevice* pVulkanDev,
                          const UINT32 width,
                          const UINT32 height)
{
    MASSERT(pImages);
    vector<VulkanImage>& images = *pImages;
    VkResult res = VK_SUCCESS;

    // These colours are patterns from the PatternClass
    static const vector<string> colours =
    {
        "SolidRed"
        ,"SolidGreen"
        ,"SolidBlue"
        ,"SolidWhite"
        ,"SolidCyan"
        ,"SolidMagenta"
        ,"SolidYellow"
    };
    static const auto format = VK_FORMAT_B8G8R8A8_UNORM;
    static const auto formatPixelSize = 4;
    static const auto depth = 32;

    images.clear();
    images.reserve(colours.size());
    for (size_t i = 0; i < colours.size(); i++)
    {
        images.emplace_back(pVulkanDev);
    }

    vector<UINT08> data(width * height * formatPixelSize);
    PatternClass rPattern;

    for (size_t i = 0; i < colours.size(); ++i)
    {
        const auto& colour = colours[i];
        auto& image = images[i];

        image.SetFormat(format);
        image.SetImageProperties(width,
            height,
            1,
            VulkanImage::COLOR);

        CHECK_VK_RESULT(image.CreateImage(
            VkImageCreateFlags(0),
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VK_IMAGE_LAYOUT_PREINITIALIZED,
            VK_IMAGE_TILING_LINEAR,
            VK_SAMPLE_COUNT_1_BIT,
            "MipMapTestImage"));

        // also VK_MEMORY_PROPERTY_HOST_CACHED_BIT?
        CHECK_VK_RESULT(image.AllocateAndBindMemory(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
        CHECK_VK_RESULT(image.CreateImageView());

        // Fill in the image
        rPattern.FillMemoryWithPattern(data.data()
            , formatPixelSize * width             // pitch in bytes
            , height                              // lines
            , formatPixelSize * width             //WidthInBytes
            , depth                               //Depth (B8G8R8A8)
            , colour                              //PatternName
            , nullptr);                           //RepeatInterval *
        CHECK_VK_RESULT(image.SetData(data.data(), data.size(), 0));
    }

    return res;
}

//--------------------------------------------------------------------
VkResult VulkanTexture::CopyTexture_TestMipMap
(
    VulkanTexture *pSrcTexture // only used for dimensions
    , VulkanCmdBuffer *pCmdBuffer
)
{
    VkResult res = VK_SUCCESS;
    // static init of coloured images
    static vector<VulkanImage> plainColorImages;
    if (plainColorImages.empty())
    {
        CHECK_VK_RESULT(CreatePlainColorImages(&plainColorImages,
            m_pVulkanDev,
            pSrcTexture->GetWidth(),
            pSrcTexture->GetHeight()));
    }
    else if ((pSrcTexture->GetHeight() > plainColorImages.back().GetHeight()) ||
        (pSrcTexture->GetWidth() > plainColorImages.back().GetWidth()))
    {
        CHECK_VK_RESULT(CreatePlainColorImages(&plainColorImages,
            m_pVulkanDev,
            std::max(pSrcTexture->GetHeight(), plainColorImages.back().GetHeight()),
            std::max(pSrcTexture->GetWidth(), plainColorImages.back().GetWidth())));
    }

    const auto& numMipLevels = m_TexImage.GetMipLevels();
    for (size_t mipLevel = 0; mipLevel < numMipLevels; ++mipLevel)
    {
        VulkanImage* imageToUse =
            &plainColorImages[std::min(mipLevel, plainColorImages.size()-1)];

        CHECK_VK_RESULT(m_TexImage.CopyFrom(imageToUse,
            pCmdBuffer,
           static_cast<UINT08> (mipLevel)));
    }
    return res;
}

//--------------------------------------------------------------------
VkResult VulkanTexture::CopyTexture
(
    VulkanTexture *pSrcTexture
    , VulkanCmdBuffer *pCmdBuffer
)
{
    // If we are in a mipmap testing mode, we override every incoming texture
    // with plain-coloured buffers, with a different colour on each mip level
    if (m_OverrideTextureMapping)
    {
        return CopyTexture_TestMipMap(pSrcTexture, pCmdBuffer);
    }

    VkResult res = VK_SUCCESS;
    const auto& numMipLevels = m_TexImage.GetMipLevels();
    for (UINT08 mipLevel = 0; mipLevel < numMipLevels; ++mipLevel)
    {
        CHECK_VK_RESULT(m_TexImage.CopyFrom(&pSrcTexture->m_TexImage,
            pCmdBuffer,
            mipLevel));
    }
    return res;
}

//--------------------------------------------------------------------
void VulkanTexture::SetTestMipMapMode(const bool overrideTextureMapping)
{
    m_OverrideTextureMapping = overrideTextureMapping;
}

//--------------------------------------------------------------------
UINT64 VulkanTexture::GetSizeBytes() const
{
    return (static_cast<UINT64>(m_Width) * m_Height *
            m_Depth * m_FormatPixelSizeBytes);
}

//--------------------------------------------------------------------
UINT64 VulkanTexture::GetPitch() const
{
    return m_TexImage.GetPitch();
}

//--------------------------------------------------------------------
VkResult VulkanTexture::SetDataRaw
(
    void *data,
    VkDeviceSize size,
    VkDeviceSize offset
)
{
    return m_TexImage.SetData(data, size, offset);
}

//--------------------------------------------------------------------
void VulkanTexture::SetUseMipMap(const bool useMipMap)
{
    m_UseMipMap = useMipMap;
}

//--------------------------------------------------------------------
void VulkanTexture::SetTextureDimensions
(
    const UINT32 width,
    const UINT32 height,
    const UINT32 depth
)
{
    m_Width = width;
    m_Height = height;
    m_Depth = depth;
}

//--------------------------------------------------------------------
VkResult VulkanTexture::GetData
(
    void *data,
    VkDeviceSize size,
    VkDeviceSize offset
)
{
    return m_TexImage.GetData(data, size, offset);
}

//--------------------------------------------------------------------
VkImage VulkanTexture::GetVkImage() const
{
    return m_TexImage.GetImage();
}

//--------------------------------------------------------------------
VulkanImage & VulkanTexture::GetTexImage()
{
    return m_TexImage;
}

VkResult VulkanTexture::SetData(VkFormat dataFormat)
{
#ifndef VULKAN_STANDALONE
    if (m_Format == VK_FORMAT_BC7_UNORM_BLOCK)
    {
        vector<UINT08> compressedData;
        const UINT64 compressedDataPitch = GetPitch();

        VkResult res = VK_SUCCESS;
        CHECK_VK_RESULT(s_VulkanTextureArchive.GetCompressedTexture
        (
            dataFormat,
            m_Format,
            m_Height,
            m_Width,
            4 * m_Width,
            compressedDataPitch,
            m_Data.data(),
            &compressedData
        ));

        return SetDataRaw(compressedData.data(), compressedData.size(), 0);
    }
#endif
    return SetDataRaw(m_Data.data(), m_Data.size(), 0);
}

//--------------------------------------------------------------------
VkResult VulkanTexture::FillRandom(UINT32 seed)
{
    const UINT64 dataPitch = (m_Format == VK_FORMAT_BC7_UNORM_BLOCK) ?
        (m_FormatPixelSizeBytes * m_Width) :
        GetPitch();
    const UINT64 sizeBytes = m_Height * dataPitch;
    m_Data.resize(sizeBytes);
    Memory::FillRandom(m_Data.data(), seed, sizeBytes);
    return SetData(VK_FORMAT_R8G8B8A8_UNORM);
}

//--------------------------------------------------------------------
VkResult VulkanTexture::FillTexture()
{
    const UINT64 dataPitch = (m_Format == VK_FORMAT_BC7_UNORM_BLOCK) ?
        (m_FormatPixelSizeBytes * m_Width) :
        GetPitch();
    m_Geometry.GetTextureData(m_Data, m_Width, m_Height, m_Depth, dataPitch);
    MASSERT(m_Data.size());
    return SetData(VK_FORMAT_R8G8B8A8_UNORM);
}

//------------------------------------------------------------------------------------------------
VkResult VulkanTexture::FillTexture(PatternClass & rPattern, INT32 patIdx)
{
    const UINT64 dataPitch = (m_Format == VK_FORMAT_BC7_UNORM_BLOCK) ?
        (m_FormatPixelSizeBytes * m_Width) :
        GetPitch();
    if (m_Data.size() != (m_Height * dataPitch))
    {
        m_Data.resize(m_Height * dataPitch);
    }
    if (patIdx < 0)
    {
        rPattern.FillMemoryIncrementing(m_Data.data()
                                         ,UNSIGNED_CAST(UINT32, dataPitch)   // pitch in bytes
                                         ,m_Height                           // lines
                                         ,m_FormatPixelSizeBytes * m_Width   // width in bytes
                                         ,32); // display depth is typically 32bits.
    }
    else
    {
        rPattern.FillMemoryWithPattern(m_Data.data()
                                       ,UNSIGNED_CAST(UINT32, dataPitch)    // pitch in bytes
                                       ,m_Height                            // lines
                                       ,m_FormatPixelSizeBytes * m_Width    //WidthInBytes
                                       ,32                                  //Depth
                                       ,patIdx                              //PatternNum
                                       ,nullptr);                           //RepeatInterval *

    }
    return SetData(VK_FORMAT_B8G8R8A8_UNORM);
}

//------------------------------------------------------------------------------------------------
// Print the texture info.
VkResult VulkanTexture::PrintTexture()
{

    Printf(Tee::PriNormal, GetTeeModuleCode(), "Texture Info:\n");
    Printf(Tee::PriNormal, GetTeeModuleCode(), "width:%d, height:%d depth:%d format:%d\n",
           m_Width, m_Height, m_Depth, m_Format);

    // We need to check to make sure that this texture image is HOST_VISIBILE
    GetData(m_Data.data(), m_Data.size(), 0);
    UINT32 *pData = reinterpret_cast<UINT32*>(m_Data.data());

    for (unsigned h = 0; h < m_Height; h++)
    {
        for (unsigned w = 0; w < m_Width; w++)
        {
            Printf(Tee::PriNormal, GetTeeModuleCode(), "0x%08x ", *pData++);
        }
        Printf(Tee::PriNormal, GetTeeModuleCode(), "\n");
    }
    Printf(Tee::PriNormal, GetTeeModuleCode(), "\n");
    return VK_SUCCESS;
}

//--------------------------------------------------------------------
VkDescriptorImageInfo VulkanTexture::GetDescriptorImageInfo() const
{
    return
    {
        m_VkSampler               // sampler
       ,m_TexImage.GetImageView() // imageView
       ,VK_IMAGE_LAYOUT_GENERAL   // imageLayout
    };
}

//--------------------------------------------------------------------
void VulkanTexture::Cleanup()
{
    if (!m_pVulkanDev)
    {
        MASSERT(m_VkSampler == VK_NULL_HANDLE);
        return;
    }

    m_VkSampler = VK_NULL_HANDLE;

    m_TexImage.Cleanup();
}

void VulkanTexture::Reset(VulkanDevice* pVulkanDev)
{
    Cleanup();
    m_pVulkanDev = pVulkanDev;
    m_TexImage.Reset(pVulkanDev);
}
