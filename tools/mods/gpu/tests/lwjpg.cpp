/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/channel.h"
#include "gpu/js/fpk_comm.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gputest.h"
#include "cheetah/dev/lwjpg/lwjpg_api.h"
#include "core/include/platform.h"
#include "cheetah/include/tegragoldsurf.h"
#include "core/include/utility.h"
#include "class/cle26e.h"
#include "class/cle7d0.h"

// Maximum width/height of the images encoded/decoded by LwjpgTest.
// Can be overridden by changing the fancyPickers.
//
static const UINT32 DEFAULT_MAX_IMAGE_WIDTH = 4096;
static const UINT32 DEFAULT_MAX_IMAGE_HEIGHT = 4096;

// Maximum number of bits in JPG Huffman codes
//
static const UINT32 MAX_HUFFMAN_CODE_LENGTH = 16;

// Same as NUMELEMS(), except recognized by all our compilers as a
// compile-time constant.  (Some compilers, especially older ones,
// choke on things like on "int foo[5]; int bar[NUMELEMS(foo)];",
// claming that bar[] has variable size.)
//
#define CT_NUMELEMS(arr) (sizeof(arr) / sizeof((arr)[0]))

//--------------------------------------------------------------------
//! \brief Test for the LWJPG engine
//!
//! The LWJPG engine is used for encoding and decoding JPG images.

//! This test uses the engine to encode & decode random images, and
//! uses golden values to verify that the results are consistent.  On
//! each loop, this test uses a fancyPicker to decide whether to
//! encode or decode, what the image size should be, etc.
//!
//! === JPG overview ===
//!
//! Here's a brief summary of the JPEG baseline format, which should
//! hopefully explain most of the concepts and terminology used in
//! this test.  For the full spec, see document ITU-T.81 at
//! //hw/gpu_ip/doc/lwjpg/1.0/NewHirerMaterial/JPEG spec/itu-t81.pdf.
//!
//! (1) To encode a JPG image, first colwert the image to YUV.  The Y,
//!     U, and V planes do not need to be the same size, but they
//!     should be a simple ratio.  For example, in 4:2:0 subsampling,
//!     Y has twice the resolution of U and V, so the luma (Y) plane
//!     is twice as wide & tall as the chroma (U & V) planes.
//! (2) Normalize YUV to all be signed one-byte values.  Y is normally
//!     unsigned, so subtract an offset to make it signed.
//! (3) Divide each plane into 8x8 "blocks".
//! (4) Combine the blocks into "MLWs" (Minimum Coding Units) such
//!     that each MLW is the smallest rectangle of pixels that can be
//!     made from a whole number of blocks.  For example, in the 4:2:0
//!     subsampling mentioned in (1), each MLW is a 16x16 square of
//!     pixels consisting of 6 blocks: four Y blocks, one U block, and
//!     one V block.
//! (5) Pad the image to contain an even number of MLWs, by
//!     duplicating the rightmost column & bottom row in each plane.
//! (6) Use DCT (Discrete Cosine Transform) to colwert each 8x8 block
//!     into a 8x8 square of coefficients.  The first coefficient is
//!     "DC", the rest are "AC".
//! (7) Use "zig-zag ordering" to colwert the 8x8 coefficients into a
//!     64-element array.  Zig-zag ordering puts the most-important
//!     low-frequency coefficients at the start of the array, and the
//!     least-important high-frequency coefficients at the end of the
//!     array.
//! (8) Divide each element of the array by the corresponding element
//!     of a 64-element "quantization table", and round to the nearest
//!     integer.  Typically, luma uses a different quantization table
//!     than chroma.
//! (9) Discard AC components as needed for data compression.
//! (10) Compress the data, using Huffman encoding and a few other
//!     tricks.  There are typically 4 Huffman tables: one for DC
//!     luma, one for AC luma, one for DC chroma, and one for AC
//!     chroma.
//! (11) The final compressed image consists of MLWs, packed together
//!      in left-to-right top-to-bottom order.  Each MLW consists of
//!      blocks, compressed as described in (6)-(10) above, in
//!      left-to-right top-to-bottom Y-U-V order.
//! (12) The full JPEG file consists of a series of "segments"
//!     identified by "markers".  Each "marker" consists of a 0xff
//!     byte, followed by a one-byte value that identifies the
//!     segment.  The segments contain things such as the image
//!     resolution, the Huffman and quantization tables, and the
//!     compressed image described in (11).
//! (13) The compressed image described in (11) is "entropy encoded":
//!     the packed bits continue until we hit a marker.  If the data
//!     happens to contain a 0xff byte, we "byte stuff" a 0x00 byte
//!     after it to distinguish it from a marker.  The decoder ignores
//!     stuffed 0x00 bytes.  Most compressed images also insert a
//!     special "restart" marker every N MLWs, which the decoder also
//!     ignores.  (Exception: MJPG Type B doesn't use byte stuffing or
//!     restart markers.)
//!
//! All values in JPG are big-endian.  In entropy-coded segments, the
//! bits are packed starting at the MSB of the first byte in the
//! segment.  The LWJPG engine only operates on the entropy-coded data
//! described in (11), but it needs to deal with the embedded restart
//! markers and byte-stuffing described in (12)-(13).
//!
class LwjpgTest : public GpuTest
{
public:
    LwjpgTest();

    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual RC SetDefaultsForPicker(UINT32 idx);

    SETGET_PROP(KeepRunning, bool);

private:
    //! Class for writing ECS (Entropy Coded Segment) bitstream,
    //! with optional byte-stuffing.
    //!
    class EcsBitstream
    {
    public:
        EcsBitstream(void *pData, size_t sizeInBytes, bool useByteStuffing);
        RC WriteBits(UINT32 data, size_t numBits);
        RC WriteMarker(UINT08 marker);
        RC Flush();
        size_t GetBitPosition() const { return m_BitPosition; }
    private:
        UINT32 *const m_pData;         //!< Pointer to the start of bitstream
        const size_t m_SizeInBits;     //!< Maximum bits that can be written
        bool m_UseByteStuffing;        //!< true = stuff 0x00 after each 0xff
        size_t m_BitPosition;          //!< Num bits written so far
        union
        {
            UINT32 m_Buffer32;   //!< Output buffer when m_BitPosition%32 != 0
            UINT08 m_Buffer[4];  //!< Output buffer bytes, in big-endian order
        };
    };

    //! Struct for holding the dimensions of an MLW or image plane.
    //!
    struct Dimensions
    {
        Dimensions(UINT32 w, UINT32 h) :
            width(w), height(h), endOffset(width*height) {}
        UINT32 width;      //!< Width of MLW or plane
        UINT32 height;     //!< Height of MLW or plane
        UINT32 endOffset;  //!< Set by GetPlaneDimensions() to the aligned
                           //!< offset where each plane ends. Ignored for MLW.
    };

    vector<Dimensions> GetPlaneDimensions() const;
    Dimensions GetMlwBlockDimensions() const;
    void CreateRandomHuffmanTable(Random *pRandom,
                                  Lwjpg::HuffTabIndex tableType,
                                  vector<UINT08> *pTable);
    void CreateRandomQuantTable(Random *pRandom, UINT08 *pTable);
    RC WriteRandomEcsBlock(Random *pRandom, const UINT08 *pQuantTable,
                           const vector<huffman_symbol_s> &dcHuffman,
                           const vector<huffman_symbol_s> &acHuffman,
                           INT32 *pPrevDc, EcsBitstream *pBitstream);
    static Lwjpg::ChromaFormat ChromaMode2ChromaFormat(Lwjpg::ChromaMode mode);
    static void ColwertHuffmanTable(const UINT08 *pSrc, huffman_tab_s *pDst);
    static void ColwertHuffmanTable(const huffman_tab_s &src,
                                    huffman_symbol_s *pDst, size_t dstSize);
    static void ColwertHuffmanTable(const UINT08 *pSrc,
                                    huffman_symbol_s *pDst, size_t dstSize);
    template<typename T> static void CopyQuantTable(const UINT08 *pSrc, T *pDst)
        { copy(&pSrc[0], &pSrc[Lwjpg::BLOCK_SIZE], pDst); }
    RC PickLoopParams();
    RC RunEncodeTest();
    RC RunDecodeTest();
    void DumpBuffer(const string &title, const void *pAddr, size_t size);

    // Standard huffman tables & quantization tables
    //
    static const UINT08 s_StdDcLumaHuffman[];
    static const UINT08 s_StdDcChromaHuffman[];
    static const UINT08 s_StdAcLumaHuffman[];
    static const UINT08 s_StdAcChromaHuffman[];
    static const UINT08 s_StdLumaQuant[Lwjpg::BLOCK_SIZE];
    static const UINT08 s_StdChromaQuant[Lwjpg::BLOCK_SIZE];

    // Channels & surfaces
    //
    Channel * m_pCh;
    UINT32 m_MaxImageWidth;
    UINT32 m_MaxImageHeight;
    Surface2D m_SetupSurface;  //!< Setup struct to configure LWJPG operation
    Surface2D m_StatusSurface; //!< Status struct returned from LWJPG operation
    Surface2D m_SrcSurface;    //!< Image or bitstream sent to LWJPG
    Surface2D m_DstSurface;    //!< Image or bitstream written by LWJPG
    TegraGoldenSurfaces m_GoldenSurfaces; //!< Used to make CRC on m_DstSurface

    // Test args
    //
    bool m_KeepRunning = false;

    // Values chosen by fancypicker
    //
    enum TestType
    {
        TEST_TYPE_ENCODE,
        TEST_TYPE_DECODE,
        TEST_TYPE_MAX_VALUE = TEST_TYPE_DECODE
    };
    TestType m_TestType;               //!< Operation we're doing in this loop
    UINT32 m_ImageWidth;
    UINT32 m_ImageHeight;
    Lwjpg::PixelFormat m_PixelFormat;  //!< Tells whether image is YUV/RGB
    Lwjpg::ChromaMode m_ImgChromaMode; //!< Subsampling in decoded image
    Lwjpg::ChromaMode m_JpgChromaMode; //!< Subsampling in encoded bitstream
    Lwjpg::YuvMemoryMode m_YuvMemoryMode; //!< How image is stored in memory
    UINT32 m_RestartInterval;  //!< Interval of restart markers in bitstream
    bool m_UseByteStuffing;    //!< Enable byte stuffing (MJPG-A)
    bool m_UseRandomImages;    //!< If false, m_SrcSurface is all-zero image
    bool m_UseRandomTables;    //!< Use random or standard Huffman/quant tables

    // Other values set on a per-loop basis
    //
    vector<UINT08> m_RandomDcLumaHuffman;
    vector<UINT08> m_RandomDcChromaHuffman;
    vector<UINT08> m_RandomAcLumaHuffman;
    vector<UINT08> m_RandomAcChromaHuffman;
    UINT08 m_RandomLumaQuant[Lwjpg::BLOCK_SIZE];
    UINT08 m_RandomChromaQuant[Lwjpg::BLOCK_SIZE];
    UINT32 m_DstBytesWritten;   //!< Num bytes of m_DstSurface to take golden of
};

//--------------------------------------------------------------------
// Standard Huffman tables from ITU-T.81 pp 149-159.
//
// These tables are in Annex-C format.  See CreateRandomHuffmanTable()
// for a summary of Annex-C format.
//
/* static */ const UINT08 LwjpgTest::s_StdDcLumaHuffman[] =
{
    0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
};

/* static */ const UINT08 LwjpgTest::s_StdDcChromaHuffman[] =
{
    0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
};

/* static */ const UINT08 LwjpgTest::s_StdAcLumaHuffman[] =
{
    0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 125,
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31,
    0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32,
    0x81, 0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52,
    0xd1, 0xf0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
    0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45,
    0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57,
    0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83,
    0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94,
    0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
    0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
    0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
    0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8,
    0xd9, 0xda, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8,
    0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa
};

/* static */ const UINT08 LwjpgTest::s_StdAcChromaHuffman[] =
{
    0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 119,
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06,
    0x12, 0x41, 0x51, 0x07, 0x61, 0x71, 0x13, 0x22, 0x32, 0x81,
    0x08, 0x14, 0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33,
    0x52, 0xf0, 0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
    0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44,
    0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56,
    0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a,
    0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92,
    0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3,
    0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
    0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
    0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
    0xd7, 0xd8, 0xd9, 0xda, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
    0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa
};

//--------------------------------------------------------------------
// Standard quantization tables from ITU-T.81 p143, in zig-zag order
//
/* static */ const UINT08 LwjpgTest::s_StdLumaQuant[Lwjpg::BLOCK_SIZE] =
{
    16,
    11,  12,
    14,  12,  10,
    16,  14,  13,  14,
    18,  17,  16,  19,  24,
    40,  26,  24,  22,  22, 24,
    49,  35,  37,  29,  40, 58, 51,
    61,  60,  57,  51,  56, 55, 64, 72,
    92,  78,  64,  68,  87, 69, 55,
    56,  80,  109, 81,  87, 95,
    98,  103, 104, 103, 62,
    77,  113, 121, 112,
    100, 120, 92,
    101, 103,
    99
};

/* static */ const UINT08 LwjpgTest::s_StdChromaQuant[Lwjpg::BLOCK_SIZE] =
{
    17,
    18, 18,
    24, 21, 24,
    47, 26, 26, 47,
    99, 66, 56, 66, 99,
    99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99,
    99, 99, 99, 99,
    99, 99, 99,
    99, 99,
    99
};

//--------------------------------------------------------------------
//! \brief Constructor for LwjpgTest
//!
LwjpgTest::LwjpgTest() :
    m_pCh(nullptr),
    m_MaxImageWidth(0),
    m_MaxImageHeight(0),
    m_TestType(static_cast<TestType>(0)),
    m_ImageWidth(0),
    m_ImageHeight(0),
    m_PixelFormat(static_cast<Lwjpg::PixelFormat>(0)),
    m_ImgChromaMode(static_cast<Lwjpg::ChromaMode>(0)),
    m_JpgChromaMode(static_cast<Lwjpg::ChromaMode>(0)),
    m_YuvMemoryMode(static_cast<Lwjpg::YuvMemoryMode>(0)),
    m_RestartInterval(0),
    m_UseByteStuffing(false),
    m_UseRandomImages(false),
    m_UseRandomTables(false),
    m_DstBytesWritten(0)
{
    SetName("LwjpgTest");
    CreateFancyPickerArray(FPK_LWJPG_NUM_PICKERS);
}

//--------------------------------------------------------------------
//! \brief Overrides IsSupported method from base class
//!
/* virtual */ bool LwjpgTest::IsSupported()
{
    GpuDevice *pGpuDevice = GetBoundGpuDevice();
    GpuSubdevice *pGpuSubdevice = GetBoundGpuSubdevice();
    LwRm *pLwRm = GetBoundRmClient();

    // This test is not supported from T234 onwards,
    // so only make it supported for T194.
    const auto deviceId = pGpuSubdevice->DeviceId();
    if (deviceId != Gpu::T194)
    {
        return false;
    }

    return (GpuTest::IsSupported() &&
            pGpuSubdevice->IsSOC() &&
            pLwRm->IsClassSupported(LWE7D0_VIDEO_LWJPG, pGpuDevice));
}

//--------------------------------------------------------------------
//! \brief Overrides Setup method from base class
//!
/* virtual */ RC LwjpgTest::Setup()
{
    GpuTestConfiguration *pTestConfig = GetTestConfiguration();
    Goldelwalues *pGoldelwalues = GetGoldelwalues();
    LwRm::Handle hChannel;
    RC rc;

    CHECK_RC(GpuTest::Setup());

    // Allocate channel
    //
    CHECK_RC(pTestConfig->AllocateChannel(&m_pCh, &hChannel));
    CHECK_RC(m_pCh->SetClass(0, LWE26E_CMD_SETCL_CLASSID_LWJPG));

    // Get the maximum screen dimensions that the fancyPickers can
    // generate.  Round the dimensions up to a multiple of the LWJPG
    // alignment requirements, to simplify the math when we callwlate
    // the maximum surface size.
    //
    FancyPickerArray &fpk = *GetFancyPickerArray();
    if (OK != fpk[FPK_LWJPG_IMAGE_WIDTH].GetPickRange(nullptr,
                                                      &m_MaxImageWidth))
    {
        m_MaxImageWidth = DEFAULT_MAX_IMAGE_WIDTH;
    }
    if (OK != fpk[FPK_LWJPG_IMAGE_HEIGHT].GetPickRange(nullptr,
                                                       &m_MaxImageHeight))
    {
        m_MaxImageHeight = DEFAULT_MAX_IMAGE_HEIGHT;
    }
    m_MaxImageWidth  = ALIGN_UP(m_MaxImageWidth, Lwjpg::STRIDE_ALIGN);
    m_MaxImageHeight = ALIGN_UP(m_MaxImageHeight,
                                Lwjpg::SURFACE_ALIGN / Lwjpg::STRIDE_ALIGN);
    ct_assert((Lwjpg::SURFACE_ALIGN % Lwjpg::STRIDE_ALIGN) == 0);

    // Allocate surfaces.
    //
    // This test uses the same source, destination, setup, and status
    // surfaces for encoding or decoding.  It allocates each surface
    // to be the maximum size required.
    //
    // The maximum source/destination surface is callwlated as follows:
    // * Allocate a width of 4 times the maximum image width.  The
    //   rationale is that, if the surface holds JPG data, then each
    //   sample can take up to 4 bytes: up to 16 bits for the
    //   Huffman-encoded data, and up to 11 bits for the data word.
    //   This estimate does not include the restart markers and
    //   occasional byte-stuffing, but since it rounds up the data word
    //   from 11 bits to 16 bits, we should be OK.
    // * Allocate a height of 3 times the maximum image height, on the
    //   rationale that the image contains at most 3 components (YUV
    //   or RGB).
    // This test ignores the Surface2D width/height for the most part,
    // and just uses it as a raw buffer.  The width/height
    // callwlations above are just to make sure the buffer is big
    // enough.
    //
    m_SetupSurface.SetWidth(max(sizeof(lwjpg_enc_drv_pic_setup_s),
                                sizeof(lwjpg_dec_drv_pic_setup_s)));
    m_SetupSurface.SetHeight(1);
    m_StatusSurface.SetWidth(max(sizeof(lwjpg_enc_status),
                                 sizeof(lwjpg_dec_status)));
    m_StatusSurface.SetHeight(1);
    m_SrcSurface.SetWidth(4 * m_MaxImageWidth);
    m_SrcSurface.SetHeight(3 * m_MaxImageHeight);
    m_DstSurface.SetWidth(4 * m_MaxImageWidth);
    m_DstSurface.SetHeight(3 * m_MaxImageHeight);

    Surface2D *ppSurface[] =
        { &m_SetupSurface, &m_StatusSurface, &m_SrcSurface, &m_DstSurface };
    for (UINT32 ii = 0; ii < NUMELEMS(ppSurface); ++ii)
    {
        ppSurface[ii]->SetColorFormat(ColorUtils::Y8);
        ppSurface[ii]->SetVASpace(Surface2D::TegraVASpace);
        ppSurface[ii]->SetAlignment(Lwjpg::SURFACE_ALIGN);
        CHECK_RC(ppSurface[ii]->Alloc(GetBoundGpuDevice()));
        CHECK_RC(ppSurface[ii]->Map());
    }

    // Setup golden values
    //
    m_GoldenSurfaces.SetNumSurfaces(1);
    CHECK_RC(m_GoldenSurfaces.DescribeSurface(
                    0, m_DstSurface.GetWidth(), 1, m_DstSurface.GetWidth(),
                    m_DstSurface.GetColorFormat(), m_DstSurface.GetAddress(),
                    0, "Dst"));
    CHECK_RC(pGoldelwalues->SetSurfaces(&m_GoldenSurfaces));

    return rc;
}

//--------------------------------------------------------------------
//! \brief Overrides Setup method from base class
//!
/* virtual */ RC LwjpgTest::Run()
{
    const GpuTestConfiguration *pTestConfig = GetTestConfiguration();
    const UINT32 startLoop = pTestConfig->StartLoop();
    const UINT32 endLoop = startLoop + pTestConfig->Loops();
    const Tee::Priority pri = GetVerbosePrintPri();
    FancyPicker::FpContext *pFpCtx = GetFpContext();
    Goldelwalues *pGoldelwalues = GetGoldelwalues();
    RC rc;

    const bool isBackground = m_KeepRunning;

    // Main loop
    //
    pGoldelwalues->SetLoop(startLoop);
    for (UINT32 loopNum = startLoop;
         (isBackground && m_KeepRunning) || (loopNum < endLoop);
         ++loopNum)
    {
        if (loopNum >= endLoop)
        {
            pGoldelwalues->SetLoop(startLoop);
            loopNum = startLoop;
        }

        Printf(pri, "== Loop %d ==\n", loopNum);
        pFpCtx->LoopNum = loopNum;
        pFpCtx->Rand.SeedRandom(pTestConfig->Seed() + loopNum);

        // Use FancyPickers to get the loop parameters
        //
        CHECK_RC(PickLoopParams());

        // Run the test
        //
        {
            Tasker::DetachThread detach;

            switch (m_TestType)
            {
                case TEST_TYPE_ENCODE:
                    CHECK_RC(RunEncodeTest());
                    break;
                case TEST_TYPE_DECODE:
                    CHECK_RC(RunDecodeTest());
                    break;
                default:
                    MASSERT(!"Illegal TestType");
            }
        }

        // Run golden values on the output buffer
        //
        CHECK_RC(m_GoldenSurfaces.DescribeSurface(
                        0, m_DstBytesWritten, 1, m_DstBytesWritten,
                        m_DstSurface.GetColorFormat(),
                        m_DstSurface.GetAddress(),
                        0, "Dst"));
        CHECK_RC(pGoldelwalues->Run());
    }

    CHECK_RC(pGoldelwalues->ErrorRateTest(GetJSObject()));

    return rc;
}

//--------------------------------------------------------------------
//! \brief Overrides Cleanup method from base class
//!
/* virtual */ RC LwjpgTest::Cleanup()
{
    Goldelwalues *pGoldelwalues = GetGoldelwalues();
    StickyRC firstRc;

    pGoldelwalues->ClearSurfaces();

    m_SetupSurface.Free();
    m_StatusSurface.Free();
    m_SrcSurface.Free();
    m_DstSurface.Free();

    if (m_pCh)
    {
        firstRc = GetTestConfiguration()->FreeChannel(m_pCh);
        m_pCh = nullptr;
    }

    firstRc = GpuTest::Cleanup();
    return firstRc;
}

//--------------------------------------------------------------------
//! \brief Constructor for EcsBitstream
//!
LwjpgTest::EcsBitstream::EcsBitstream
(
    void *pData,
    size_t sizeInBytes,
    bool useByteStuffing
) :
    m_pData(static_cast<UINT32*>(pData)),
    m_SizeInBits(sizeInBytes * 8),
    m_UseByteStuffing(useByteStuffing),
    m_BitPosition(0),
    m_Buffer32(0)
{
    MASSERT(pData);
}

//--------------------------------------------------------------------
//! \brief Append bits to the bitstream
//!
//! \param data The bits to write.  This method uses the
//!     least-significant numBits bits of data, and ignores the rest.
//! \param numBits The number bits to write.
//!
RC LwjpgTest::EcsBitstream::WriteBits(UINT32 data, size_t numBits)
{
    MASSERT(numBits <= 32);
    while (numBits > 0)
    {
        const size_t endOfByte = ALIGN_UP(m_BitPosition + 1, 8U);
        const size_t bitsToWrite = min(endOfByte - m_BitPosition, numBits);
        const size_t downShift = numBits - bitsToWrite;
        const size_t upShift = endOfByte - (m_BitPosition + bitsToWrite);
        const UINT32 mask = (1 << bitsToWrite) - 1;
        const size_t buffIdx = (m_BitPosition / 8) % 4;
        m_Buffer[buffIdx] |= ((data >> downShift) & mask) << upShift;
        m_BitPosition += bitsToWrite;
        numBits -= bitsToWrite;

        if (upShift == 0)
        {
            const bool stuffOneByte =
                (m_UseByteStuffing && m_Buffer[buffIdx] == 0xff);
            if (stuffOneByte)
                m_BitPosition += 8;
            if (buffIdx == 3 || (buffIdx == 2 && stuffOneByte))
            {
                MEM_WR32(&m_pData[m_BitPosition/32 - 1], m_Buffer32);
                m_Buffer32 = 0;
            }
        }
    }

    if (m_BitPosition > m_SizeInBits)
        return RC::DATA_TOO_LARGE;
    return OK;
}

//--------------------------------------------------------------------
//! \brief Append a marker to the bitstream
//!
//! Write enough 1-bits to take us to a byte boundary, stuffing a 0x00
//! byte if the last byte is 0xff, and then write the two-byte marker
//! 0xff 0x**, where 0x** is determined by the argument.
//!
//! The caller should only call this method if byte-stuffing is
//! enabled.  Otherwise, a marker is indistinguishable from data.
//!
RC LwjpgTest::EcsBitstream::WriteMarker(UINT08 marker)
{
    MASSERT(m_UseByteStuffing);
    RC rc;
    CHECK_RC(WriteBits(0xff, (8 - (m_BitPosition % 8)) % 8));
    Utility::TempValue<bool> tmpDisableByteStuffing(m_UseByteStuffing, false);
    CHECK_RC(WriteBits(0xff00 | (0x00ff & marker), 16));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Write any remaining partial data to the bitstream
//!
//! This method is used at the very end of the bitstream, to flush any
//! partial data in m_Buffer.  Append enough 1-bits to take us to a
//! byte boundary, stuffing a 0x00 if the last byte was 0xff, and
//! append to bitstream.
//!
RC LwjpgTest::EcsBitstream::Flush()
{
    RC rc;
    CHECK_RC(WriteBits(0xff, (8 - (m_BitPosition % 8)) % 8));
    if ((m_BitPosition % 32) != 0)
        MEM_WR32(&m_pData[m_BitPosition/32], m_Buffer32);
    return rc;
}

//--------------------------------------------------------------------
//! \brief Override the base-class SetDefaultsForPicker
//!
//! This method provides defaults for the fancyPickers.  Almost all of
//! them are random pickers that generate the full range of legal
//! values, with a few exceptions:
//! * FPK_LWJPG_RESTART_INTERAL returns a fairly arbitrary range,
//!     chosen to be less than the number of MLWs for most images.
//! * FPK_LWJPG_USE_RANDOM_IMAGES defaults to always true.  It should
//!     only be false for debugging.
//!
/* virtual */ RC LwjpgTest::SetDefaultsForPicker(UINT32 idx)
{
    FancyPicker *pPicker = &(*GetFancyPickerArray())[idx];
    RC rc;

    CHECK_RC(pPicker->ConfigRandom());
    switch (idx)
    {
        case FPK_LWJPG_TEST_TYPE:
            pPicker->AddRandRange(1, 0, TEST_TYPE_MAX_VALUE);
            break;
        case FPK_LWJPG_IMAGE_WIDTH:
            pPicker->AddRandRange(1, 1, DEFAULT_MAX_IMAGE_WIDTH);
            break;
        case FPK_LWJPG_IMAGE_HEIGHT:
            pPicker->AddRandRange(1, 1, DEFAULT_MAX_IMAGE_HEIGHT);
            break;
        case FPK_LWJPG_PIXEL_FORMAT:
            pPicker->AddRandItem(1, Lwjpg::PIXEL_FORMAT_YUV);
            pPicker->AddRandRange(1, 0, Lwjpg::PIXEL_FORMAT_MAX_VALUE);
            break;
        case FPK_LWJPG_IMG_CHROMA_MODE:
            pPicker->AddRandRange(1, 0, Lwjpg::CHROMA_MODE_MAX_VALUE);
            break;
        case FPK_LWJPG_JPG_CHROMA_MODE:
            pPicker->AddRandRange(1, 0, Lwjpg::CHROMA_MODE_MAX_VALUE);
            break;
        case FPK_LWJPG_YUV_MEMORY_MODE:
            pPicker->AddRandRange(1, 0, Lwjpg::YUV_MEMORY_MODE_MAX_VALUE);
            break;
        case FPK_LWJPG_RESTART_INTERVAL:
            pPicker->AddRandRange(1, 0, 100);
            break;
        case FPK_LWJPG_USE_BYTE_STUFFING:
            pPicker->AddRandItem(1, false);
            pPicker->AddRandItem(1, true);
            break;
        case FPK_LWJPG_USE_RANDOM_IMAGES:
            pPicker->AddRandItem(1, true);
            break;
        case FPK_LWJPG_USE_RANDOM_TABLES:
            pPicker->AddRandItem(1, false);
            pPicker->AddRandItem(1, true);
            break;
        default:
            MASSERT(!"Unknown picker");
            return RC::SOFTWARE_ERROR;
    }
    pPicker->CompileRandom();
    return rc;
}

//--------------------------------------------------------------------
//! \brief Return an array with the dimensions of each image plane
//!
//! Some formats store the image in a set of planes.  For example, the
//! semiplanar LW12 format stores the YUV components in two planes:
//! one MxN plane for the Y components, and one MxN/2 plane for the
//! interleaved U & V components.  Other formats, such as RGBA and
//! YUY2, store all all of the components in a single plane.
//!
//! This method returns the dimensions of each plane in an array, with
//! the following fields in each element:
//! * width: The width of the plane, in bytes.  In order to find the
//!     stride, the caller must round up to Lwjpg::STRIDE_ALIGN.
//! * height: The height of the plane, in rows.
//! * endOffset: Assuming that the planes are concatenated together in
//!     a single surfaces, endOffset contains the offset where this
//!     plane ends and the next begins.  It's basically the sum of
//!     width*height in all planes up to this one, rounded up for
//!     alignment requirements.
//!
//! When there are multiple planes, this method returns them in Y-U-V
//! order.  It is up to the caller to know how the components are
//! stored in interleaved planes.
//!
vector<LwjpgTest::Dimensions> LwjpgTest::GetPlaneDimensions() const
{
    vector<Dimensions> dims;

    // Callwlate width/height for nonplanar RGB & RGBA formats
    //
    switch (m_PixelFormat)
    {
        case Lwjpg::PIXEL_FORMAT_YUV:
            break;  // Callwlate dimensions in next section
        case Lwjpg::PIXEL_FORMAT_RGB:
        case Lwjpg::PIXEL_FORMAT_BGR:
            dims.push_back(Dimensions(3 * m_ImageWidth, m_ImageHeight));
            break;
        case Lwjpg::PIXEL_FORMAT_RGBA:
        case Lwjpg::PIXEL_FORMAT_BGRA:
        case Lwjpg::PIXEL_FORMAT_ABGR:
        case Lwjpg::PIXEL_FORMAT_ARGB:
            dims.push_back(Dimensions(4 * m_ImageWidth, m_ImageHeight));
            break;
    }

    // Callwlate width/height for nonplanar & semiplanar YUV formats
    //
    if (dims.empty() && m_PixelFormat == Lwjpg::PIXEL_FORMAT_YUV)
    {
        switch (m_YuvMemoryMode)
        {
            case Lwjpg::YUV_MEMORY_MODE_LW12:
            case Lwjpg::YUV_MEMORY_MODE_LW21:
                dims.push_back(Dimensions(m_ImageWidth, m_ImageHeight));
                dims.push_back(Dimensions(ALIGN_UP(m_ImageWidth, 2U),
                                          (m_ImageHeight + 1) / 2));
                break;
            case Lwjpg::YUV_MEMORY_MODE_YUY2:
                dims.push_back(Dimensions(2 * ALIGN_UP(m_ImageWidth, 2U),
                                          m_ImageHeight));
                break;
            case Lwjpg::YUV_MEMORY_MODE_PLANAR:
                break; // Callwlate dimensions in next section
        }
    }

    // Callwlate width/height for planar YUV formats
    //
    if (dims.empty() &&
        m_PixelFormat == Lwjpg::PIXEL_FORMAT_YUV &&
        m_YuvMemoryMode == Lwjpg::YUV_MEMORY_MODE_PLANAR)
    {
        switch (m_ImgChromaMode)
        {
            case Lwjpg::CHROMA_MODE_MONOCHROME:
                dims.push_back(Dimensions(m_ImageWidth, m_ImageHeight));
                break;
            case Lwjpg::CHROMA_MODE_YUV420:
                dims.push_back(Dimensions(m_ImageWidth, m_ImageHeight));
                dims.push_back(Dimensions((m_ImageWidth + 1) / 2,
                                          (m_ImageHeight + 1) / 2));
                dims.push_back(dims.back());
                break;
            case Lwjpg::CHROMA_MODE_YUV422H:
                dims.push_back(Dimensions(m_ImageWidth, m_ImageHeight));
                dims.push_back(Dimensions((m_ImageWidth + 1) / 2,
                                          m_ImageHeight));
                dims.push_back(dims.back());
                break;
            case Lwjpg::CHROMA_MODE_YUV422V:
                dims.push_back(Dimensions(m_ImageWidth, m_ImageHeight));
                dims.push_back(Dimensions(m_ImageWidth,
                                          (m_ImageHeight + 1) / 2));
                dims.push_back(dims.back());
                break;
            case Lwjpg::CHROMA_MODE_YUV444:
                dims.push_back(Dimensions(m_ImageWidth, m_ImageHeight));
                dims.push_back(dims.back());
                dims.push_back(dims.back());
                break;
        }
    }

    // Callwlate endOffset
    //
    for (UINT32 ii = 0; ii < dims.size(); ++ii)
    {
        UINT32 stride = ALIGN_UP(dims[ii].width, Lwjpg::STRIDE_ALIGN);
        dims[ii].endOffset = ALIGN_UP(stride * dims[ii].height,
                                      Lwjpg::SURFACE_ALIGN);
        if (ii > 0)
            dims[ii].endOffset += dims[ii-1].endOffset;
    }

    MASSERT(dims.size() > 0);
    MASSERT(dims.back().endOffset <= m_SrcSurface.GetSize());
    MASSERT(dims.back().endOffset <= m_DstSurface.GetSize());
    return dims;
}

//--------------------------------------------------------------------
//! \brief Return the dimensions of the MLW, in blocks
//!
//! Technically, this method returns the dimensions of the luma
//! component in each MLW.  In all lwrrently-supported formats, luma
//! is sampled one per pixel, and both chroma components are sampled
//! at an identical 1/N fraction of the luma sampling (except for
//! monochrome, which has no chroma).  So each MLW consists of a
//! rectangle of luma blocks, with dimensions given by this method,
//! followed by two chroma blocks (U and V) except for monochrome.
//!
LwjpgTest::Dimensions LwjpgTest::GetMlwBlockDimensions() const
{
    switch (m_JpgChromaMode)
    {
        case Lwjpg::CHROMA_MODE_MONOCHROME:
            return Dimensions(1, 1);
        case Lwjpg::CHROMA_MODE_YUV420:
            return Dimensions(2, 2);
        case Lwjpg::CHROMA_MODE_YUV422H:
            return Dimensions(2, 1);
        case Lwjpg::CHROMA_MODE_YUV422V:
            return Dimensions(1, 2);
        case Lwjpg::CHROMA_MODE_YUV444:
            return Dimensions(1, 1);
    }

    MASSERT(!"Unknown chromaMode");  // Should never get here
    return Dimensions(0, 0);
}

//--------------------------------------------------------------------
//! \brief Create a random DC or AC Huffman table
//!
//! Create a random Huffman table to "compress" the data.  (I put
//! "compress" in quotes because a randomly-generated table is
//! unlikely to be any good at compression.  But it should exercise
//! the LWJPG engine.)
//!
//! This function writes the table in the format described in Annex C
//! of ITU-T.81.  Annex-C format is compact but obscure:
//! * The table is stored as a byte array with 16+N elements, where N
//!   is the number of symbols that can be encoded.
//! * The first 16 bytes are the number of symbols that have a 1-bit
//!   code, the number of symbols that have a 2-bit code, and so on,
//!   up to the number of symbols that have a 16-bit code.  (Note:
//!   This file defines MAX_HUFFMAN_CODE_SIZE = 16, to avoid using
//!   "16" as a magic number.)
//! * The next N bytes are the symbols that can be encoded, from
//!   smallest code-size to largest.
//! * The codes are assigned starting at 0 for the first symbol,
//!   incrementing the code after each symbol, and appending 0-bits to
//!   the end of the code whenever the 16-element array says that the
//!   next symbol has a bigger code.  A code with all 1-bits is
//!   illegal.
//!
//! \param pRandom Random-number generator
//! \param tableType Tells whether to generate a DC or AC table
//! \param[out] pTable Holds the table on exit
//!
void LwjpgTest::CreateRandomHuffmanTable
(
    Random *pRandom,
    Lwjpg::HuffTabIndex tableType,
    vector<UINT08> *pTable
)
{
    // Use tableType to select the list of symbols that can be encoded
    // or decoded by this Huffman table
    //
    static const UINT08 dcSymbols[] =
    {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
    };
    static const UINT08 acSymbols[] =
    {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a,
        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a,
        0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a,
        0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a,
        0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a,
        0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a,
        0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a,
        0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a,
        0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a,
        0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa,
        0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba,
        0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca,
        0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
        0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
        0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa,
        0x00, 0xf0
    };
    const UINT08 *pSymbols = (tableType == Lwjpg::HUFFTAB_DC
                              ? &dcSymbols[0] : &acSymbols[0]);
    const UINT08 numSymbols = static_cast<UINT08>(tableType == Lwjpg::HUFFTAB_DC
                                                  ? NUMELEMS(dcSymbols)
                                                  : NUMELEMS(acSymbols));

    // Randomly generate the 16-element array that says how many codes
    // there are for each bit-length.  We start by assuming that all
    // symbols have a 1-bit code, and then randomly increment the
    // number of bits of symbols until the array is legal.
    //
    pTable->assign(MAX_HUFFMAN_CODE_LENGTH, 0);
    (*pTable)[0] = numSymbols;
    bool done = false;
    while (!done)
    {
        // Randomly select codes with length 15 or less, and increment
        // their lengths by 1.
        //
        for (size_t ii = MAX_HUFFMAN_CODE_LENGTH-1; ii > 0; --ii)
        {
            const UINT08 numToMove = pRandom->GetRandom(0, (*pTable)[ii - 1]);
            (*pTable)[ii - 1] -= numToMove;
            (*pTable)[ii] += numToMove;
        }

        // We're done if all codes generated by this array fit within
        // the correct number of bits, with no code consisting of all
        // 1-bits.
        //
        done = true;
        UINT32 nextCode = 0;
        for (size_t codeLength = 1;
             codeLength <= MAX_HUFFMAN_CODE_LENGTH; ++codeLength)
        {
            nextCode = (nextCode << 1) + (*pTable)[codeLength - 1];
            if (nextCode >= (1U << codeLength))
            {
                done = false;
                break;
            }
        }
    }

    // Create the Huffman table by taking the 16-element array
    // generated above, and concatenating the symbols in random order.
    //
    vector<UINT32> deck(numSymbols);
    for (UINT32 ii = 0; ii < numSymbols; ++ii)
        deck[ii] = ii;
    pRandom->Shuffle(numSymbols, &deck[0]);

    pTable->reserve(MAX_HUFFMAN_CODE_LENGTH + numSymbols);
    for (UINT32 ii = 0; ii < numSymbols; ++ii)
        pTable->push_back(pSymbols[deck[ii]]);
}

//--------------------------------------------------------------------
//! \brief Create a random quantization table
//!
//! Each element of the quantization table is an unsigned byte from
//! 1-255.  Some quick math shows that any value in this range should
//! work:
//! * The FDCT equation on ITU-T.81 p27 produces coefficients that can
//!   be up to 8 times as big as the input samples.
//! * The uncoded YUV samples are signed one-byte numbers.  So each is
//!   a 7-bit value, not counting the sign bit.
//! * So, in the worst case of a high coefficient divided by a low
//!   quantization value, the quantized coeffiecients can be up to
//!   10 bits (not counting sign).
//! * DC coefficients are stored differentially, so they can need 11
//!   bits.
//! * This perfectly matches the ranges in ITU-T.81 pp 88-91: it can
//!   encode quantized DC coefficients up to 11 bits and AC up to 10
//!   bits, not counting sign.  The spec doesn't say where they got
//!   those limits from, but they obviously did the math.
//!
//! The quantization values are chosen on a more-or-less logarithmic
//! scale, on the theory that if we chose values from 1-255 linearly,
//! we would usually choose larger numbers from 64-255 and the
//! quantized coefficients would almost all be only 2-3 bits long.  A
//! log scale exercises a wider range of coefficient lengths.
//!
//! \param pRandom Random-number generator
//! \param[out] pTable Holds the table on exit, in zig-zag order
//!
void LwjpgTest::CreateRandomQuantTable(Random *pRandom, UINT08 *pTable)
{
    for (UINT32 ii = 0; ii < Lwjpg::BLOCK_SIZE; ++ii)
    {
        float value = pRandom->GetRandomFloatExp(0, 7);
        if (value < 0)
            value = -value;
        value = min(max(value, 1.0f), 255.0f);
        pTable[ii] = static_cast<UINT08>(value);
    }
}

//--------------------------------------------------------------------
//! \brief Write a random ECS (Entropy-Coded Segment) block
//!
//! This method is used by RunDecodeTest() to generate a random JPG.
//! It generates an 8x8 block of Huffman-encoded quantized
//! coefficients, and appends the block to a bitstream.
//!
//! If m_UseRandomImages is set, then the coefficients are selected
//! randomly on a mostly logarithmic scale (to ensure a decent spread
//! of coefficient lengths).  If m_UseRandomImages is false (eg for
//! debugging), then the coefficients are all zero.
//!
//! \param pRandom Random-number generator
//! \param pQuantTable The quantization table
//! \param dcHuffman The Huffman table used to encode the DC coefficient
//! \param acHuffman The Huffman table used to encode the AC coefficients
//! \param[in,out] pPrevDc Holds the DC coefficient of the previous
//!     block on entry, and the DC coefficient of this block on exit.
//!     Required because DC coefficients are stored differentially.
//! \param[in,out] pBitstream The bitstream to append the ECS block to
//!
RC LwjpgTest::WriteRandomEcsBlock
(
    Random *pRandom,
    const UINT08 *pQuantTable,
    const vector<huffman_symbol_s> &dcHuffman,
    const vector<huffman_symbol_s> &acHuffman,
    INT32 *pPrevDc,
    EcsBitstream *pBitstream
)
{
    RC rc;
    MASSERT(11 < dcHuffman.size());
    MASSERT(0xfa < acHuffman.size());
    MASSERT(acHuffman[0xf0].length > 0);
    MASSERT(acHuffman[0x00].length > 0);

    // Randomly generate the 64 quantized coefficients
    //
    INT32 data[Lwjpg::BLOCK_SIZE];
    if (m_UseRandomImages)
    {
        for (size_t ii = 0; ii < Lwjpg::BLOCK_SIZE; ++ii)
        {
            float randomFloat = pRandom->GetRandomFloatExp(-1, 10);
            data[ii] = static_cast<INT32>(floor(randomFloat / pQuantTable[ii]
                                                + 0.5));
            data[ii] = min(max(data[ii], -0x3ff), 0x3ff);
        }
    }
    else
    {
        memset(data, 0, sizeof(data));
    }

    // Colwert the DC coefficient to a differential value
    //
    const INT32 dc = data[0];
    data[0] = dc - *pPrevDc;
    *pPrevDc = dc;

    // Encode the coefficients.
    //
    // The DC coefficient is stored in the form <N><C>, where N is a
    // Huffman-encoded number of bits, and C is the lowest N bits of
    // the coefficient.  Positive numbers are stored up to the leading
    // 1, negative numbers are stored in one's-complement up to the
    // leading 0.
    //
    // AC coefficients are stored in the form <Z><N><C>, where Z is
    // the number of conselwtive zeroes (0-15) before this
    // coefficient, and N & C have the same meaning as above.  <Z><N>
    // is stored as a Huffman-encoded byte, with the special values
    // 0xf0 & 0x00 meaning "16 conselwtive zeroes" and "all remaining
    // AC coefficients are zero" respectively.
    //
    UINT08 numZeroes = 0;  // Num conselwtive 0x0 AC coeffs to write
    for (size_t ii = 0; ii < Lwjpg::BLOCK_SIZE; ++ii)
    {
        if (data[ii] == 0 && ii != 0)
        {
            ++numZeroes;
        }
        else
        {
            const vector<huffman_symbol_s> &huffman =
                (ii == 0) ? dcHuffman : acHuffman;
            while (numZeroes >= 16)
            {
                CHECK_RC(pBitstream->WriteBits(huffman[0xf0].value,
                                               huffman[0xf0].length));
                numZeroes -= 16;
            }
            UINT08 numBits =
                Utility::BitScanReverse(static_cast<UINT32>(abs(data[ii]))) + 1;
            UINT08 code = (numZeroes << 4) | numBits;
            MASSERT(code < huffman.size());
            MASSERT(huffman[code].length > 0);
            CHECK_RC(pBitstream->WriteBits(huffman[code].value,
                                           huffman[code].length));
            CHECK_RC(pBitstream->WriteBits(
                            data[ii] >= 0 ? data[ii] : data[ii] - 1, numBits));
            numZeroes = 0;
        }
    }
    if (numZeroes > 0)
    {
        CHECK_RC(pBitstream->WriteBits(acHuffman[0x00].value,
                                       acHuffman[0x00].length));
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Colwert a ChromaMode to ChromaFormat
//!
//! For some reason, the LWJPG team used different constants in the
//! encoder & decoder for the same set of YUV subsample modes.  To
//! keep it simple, this test only uses the decoder's ChromaMode
//! constants, and colwerts to the encoder's ChromaFormat constants as
//! needed.
//!
/* static */ Lwjpg::ChromaFormat LwjpgTest::ChromaMode2ChromaFormat
(
    Lwjpg::ChromaMode mode
)
{
    switch (mode)
    {
        case Lwjpg::CHROMA_MODE_MONOCHROME: break;  // No valid colwersion
        case Lwjpg::CHROMA_MODE_YUV420:     return Lwjpg::CHROMA_FORMAT_420;
        case Lwjpg::CHROMA_MODE_YUV422H:    return Lwjpg::CHROMA_FORMAT_422H;
        case Lwjpg::CHROMA_MODE_YUV422V:    return Lwjpg::CHROMA_FORMAT_422V;
        case Lwjpg::CHROMA_MODE_YUV444:     return Lwjpg::CHROMA_FORMAT_444;
    }

    MASSERT(!"Illegal ChromaMode");
    return static_cast<Lwjpg::ChromaFormat>(0);
}

//---------------------------------------------------------------------
//! \brief Colwert a Huffman table from Annex-C to decoder format
//!
//! There are three different formats for Huffman tables used in this
//! test: the Annex-C format used by JPG (which is the easiest to
//! store and generate), the format used by the encoder, and the
//! format used by the decoder.  This is one of several routines to
//! colwert between the formats.
//!
//! See CreateRandomHuffmanTable() for a description of Annex-C format.
//!
//! \param pSrc The table in Annex-C format
//! \param[out] pDst The table in Annex-C format
//! \sa CreateRandomHuffmanTable()
//!
/* static */ void LwjpgTest::ColwertHuffmanTable
(
    const UINT08 *pSrc,
    huffman_tab_s *pDst
)
{
    ct_assert(MAX_HUFFMAN_CODE_LENGTH == CT_NUMELEMS(pDst->codeNum));
    ct_assert(MAX_HUFFMAN_CODE_LENGTH == CT_NUMELEMS(pDst->minCodeIdx));
    ct_assert(MAX_HUFFMAN_CODE_LENGTH == CT_NUMELEMS(pDst->minCode));

    memset(pDst, 0, sizeof(*pDst));

    // The first 16 elements of pSrc contain the number of codes of
    // each bit-length.  Write the fields of pDst that can be derived
    // from that.
    //
    UINT08 numSymbols = 0;
    UINT32 nextCode = 0;
    for (UINT08 ii = 0; ii < MAX_HUFFMAN_CODE_LENGTH; ++ii)
    {
        UINT08 numCodes = pSrc[ii];
        MASSERT(nextCode + numCodes < (2U << ii));

        pDst->codeNum[ii] = numCodes;
        pDst->minCodeIdx[ii] = numSymbols;
        pDst->minCode[ii] = nextCode;

        numSymbols += numCodes;
        nextCode = (nextCode + numCodes) << 1;
    }

    // Copy the symbols in the remaining elements of pSrc to pDst.
    //
    MASSERT(numSymbols <= NUMELEMS(pDst->symbol));
    for (UINT32 ii = 0; ii < numSymbols; ++ii)
        pDst->symbol[ii] = pSrc[MAX_HUFFMAN_CODE_LENGTH + ii];
}

//---------------------------------------------------------------------
//! \brief Colwert a Huffman table from decoder to encoder format
//!
//! There are three different formats for Huffman tables used in this
//! test: the Annex-C format used by JPG (which is the easiest to
//! store and generate), the format used by the encoder, and the
//! format used by the decoder.  This is one of several routines to
//! colwert between the formats.
//!
//! \param pSrc The table in decoder format
//! \param[out] pDst The table in encoder format
//! \param dstSize The number of elements in pDst
//!
/* static */ void LwjpgTest::ColwertHuffmanTable
(
    const huffman_tab_s &src,
    huffman_symbol_s *pDst,
    size_t dstSize
)
{
    memset(pDst, 0, sizeof(*pDst) * dstSize);
    for (UINT08 codeLength = 1;
         codeLength <= MAX_HUFFMAN_CODE_LENGTH; ++codeLength)
    {
        UINT08 numCodes = src.codeNum[codeLength - 1];
        UINT08 minCodeIdx = src.minCodeIdx[codeLength - 1];
        UINT16 minCode = src.minCode[codeLength - 1];
        for (UINT08 ii = 0; ii < numCodes; ++ii)
        {
            UINT08 symbol = src.symbol[minCodeIdx + ii];
            MASSERT(symbol < dstSize);
            pDst[symbol].length = codeLength;
            pDst[symbol].value = minCode + ii;
        }
    }
}

//---------------------------------------------------------------------
//! \brief Colwert a Huffman table from Annex-C to encoder format
//!
//! There are three different formats for Huffman tables used in this
//! test: the Annex-C format used by JPG (which is the easiest to
//! store and generate), the format used by the encoder, and the
//! format used by the decoder.  This is one of several routines to
//! colwert between the formats.
//!
//! See CreateRandomHuffmanTable() for a description of Annex-C format.
//!
//! \param pSrc The table in Annex-C format
//! \param[out] pDst The table in encoder format
//! \param dstSize The number of elements in pDst
//! \sa CreateRandomHuffmanTable()
//!
/* static */ void LwjpgTest::ColwertHuffmanTable
(
    const UINT08 *pSrc,
    huffman_symbol_s *pDst,
    size_t dstSize
)
{
    huffman_tab_s decodeTable;
    ColwertHuffmanTable(pSrc, &decodeTable);
    ColwertHuffmanTable(decodeTable, pDst, dstSize);
}

//---------------------------------------------------------------------
//! \brief Pick the parameters used by this loop of the test
//!
//! Use fancyPickers to get the parameters for this loop.  Then tweak
//! the values chosen by the pickers as needed, and initialize any
//! other values used in this loop.
//!
RC LwjpgTest::PickLoopParams()
{
    Random *pRandom = &GetFpContext()->Rand;
    const Tee::Priority pri = GetVerbosePrintPri();

    // Get values from fancyPickers
    //
    FancyPickerArray &fpk = *GetFancyPickerArray();
    m_TestType = static_cast<TestType>(fpk[FPK_LWJPG_TEST_TYPE].Pick());
    m_ImageWidth = fpk[FPK_LWJPG_IMAGE_WIDTH].Pick();
    m_ImageHeight = fpk[FPK_LWJPG_IMAGE_HEIGHT].Pick();
    m_PixelFormat = static_cast<Lwjpg::PixelFormat>(
            fpk[FPK_LWJPG_PIXEL_FORMAT].Pick());
    m_ImgChromaMode = static_cast<Lwjpg::ChromaMode>(
            fpk[FPK_LWJPG_IMG_CHROMA_MODE].Pick());
    m_JpgChromaMode = static_cast<Lwjpg::ChromaMode>(
            fpk[FPK_LWJPG_JPG_CHROMA_MODE].Pick());
    m_YuvMemoryMode = static_cast<Lwjpg::YuvMemoryMode>(
            fpk[FPK_LWJPG_YUV_MEMORY_MODE].Pick());
    m_RestartInterval = fpk[FPK_LWJPG_RESTART_INTERVAL].Pick();
    m_UseByteStuffing = (fpk[FPK_LWJPG_USE_BYTE_STUFFING].Pick() != 0);
    m_UseRandomImages = (fpk[FPK_LWJPG_USE_RANDOM_IMAGES].Pick() != 0);
    m_UseRandomTables = (fpk[FPK_LWJPG_USE_RANDOM_TABLES].Pick() != 0);

    // Check the picked values for range violations, which would
    // indicate a mis-configured fancypicker.
    //
    MASSERT(m_TestType <= TEST_TYPE_MAX_VALUE);
    MASSERT(m_ImageWidth >= 1);
    MASSERT(m_ImageWidth <= m_MaxImageWidth);
    MASSERT(m_ImageHeight >= 1);
    MASSERT(m_ImageHeight <= m_MaxImageHeight);
    MASSERT(m_PixelFormat <= Lwjpg::PIXEL_FORMAT_MAX_VALUE);
    MASSERT(m_ImgChromaMode <= Lwjpg::CHROMA_MODE_MAX_VALUE);
    MASSERT(m_JpgChromaMode <= Lwjpg::CHROMA_MODE_MAX_VALUE);
    MASSERT(m_YuvMemoryMode <= Lwjpg::YUV_MEMORY_MODE_MAX_VALUE);

    // Adjust the picked values to meet other constraints.
    //
    // To keep things from getting too complicated, we propagate
    // constraints in the following semi-arbitrary order:
    // m_TestType => m_JpgChromaMode => m_PixelFormat =>
    // m_ImgChromaMode => m_YuvMemoryMode => everything else.
    // Also, we apply LWJPG 1.0 constraints before applying generic
    // constraints.
    //
    if (m_TestType == TEST_TYPE_ENCODE)
    {
        m_JpgChromaMode = Lwjpg::CHROMA_MODE_YUV420;            // LWJPG 1.0
        m_PixelFormat = Lwjpg::PIXEL_FORMAT_YUV;                // LWJPG 1.0
        m_ImgChromaMode = Lwjpg::CHROMA_MODE_YUV420;            // LWJPG 1.0
    }

    if (m_TestType == TEST_TYPE_DECODE)
    {
        // LWJPG 1.0 decoder doesn't support RGB, BGR, or LW21
        //
        if (m_PixelFormat == Lwjpg::PIXEL_FORMAT_RGB)
            m_PixelFormat = Lwjpg::PIXEL_FORMAT_RGBA;
        if (m_PixelFormat == Lwjpg::PIXEL_FORMAT_BGR)
            m_PixelFormat = Lwjpg::PIXEL_FORMAT_ABGR;
        if (m_YuvMemoryMode == Lwjpg::YUV_MEMORY_MODE_LW21)
            m_YuvMemoryMode = Lwjpg::YUV_MEMORY_MODE_PLANAR;
    }

    if (m_TestType == TEST_TYPE_DECODE &&
        m_PixelFormat == Lwjpg::PIXEL_FORMAT_YUV &&
        m_ImgChromaMode != m_JpgChromaMode)
    {
        // LWJPG 1.0 supports decode-to-YUV colwersions from any of
        // {420, 422H, 422V, 444} to any of {420, 422H}.  All other
        // colwersions are unsupported.
        //
        bool supported = false;
        if (m_JpgChromaMode == Lwjpg::CHROMA_MODE_YUV420 ||
            m_JpgChromaMode == Lwjpg::CHROMA_MODE_YUV422H ||
            m_JpgChromaMode == Lwjpg::CHROMA_MODE_YUV422V ||
            m_JpgChromaMode == Lwjpg::CHROMA_MODE_YUV444)
        {
            if (m_ImgChromaMode == Lwjpg::CHROMA_MODE_YUV420 ||
                m_ImgChromaMode == Lwjpg::CHROMA_MODE_YUV422H)
            {
                supported = true;
            }
        }
        if (!supported)
            m_ImgChromaMode = m_JpgChromaMode;
    }

    if (m_PixelFormat == Lwjpg::PIXEL_FORMAT_YUV)
    {
        // Resolve incompibilities between chroma mode & memory mode.
        // LW12 & LW21 require YUV420, and YUY2 requires YUV422H.
        //
        bool compatible = false;
        switch (m_YuvMemoryMode)
        {
            case Lwjpg::YUV_MEMORY_MODE_LW12:
            case Lwjpg::YUV_MEMORY_MODE_LW21:
                compatible = (m_ImgChromaMode == Lwjpg::CHROMA_MODE_YUV420);
                break;
            case Lwjpg::YUV_MEMORY_MODE_YUY2:
                compatible = (m_ImgChromaMode == Lwjpg::CHROMA_MODE_YUV422H);
                break;
            case Lwjpg::YUV_MEMORY_MODE_PLANAR:
                compatible = true;
                break;
        }
        if (!compatible)
        {
            m_YuvMemoryMode = (m_ImgChromaMode == Lwjpg::CHROMA_MODE_YUV420 ?
                               Lwjpg::YUV_MEMORY_MODE_LW12 :
                               m_ImgChromaMode == Lwjpg::CHROMA_MODE_YUV422H ?
                               Lwjpg::YUV_MEMORY_MODE_YUY2 :
                               Lwjpg::YUV_MEMORY_MODE_PLANAR);
        }
    }
    else
    {
        // Discard the YUV settings for non-YUV pixel formats such as RGBA
        //
        m_ImgChromaMode = static_cast<Lwjpg::ChromaMode>(0);
        m_YuvMemoryMode = static_cast<Lwjpg::YuvMemoryMode>(0);
    }

    if (!m_UseByteStuffing)
    {
        m_RestartInterval = 0;
    }

    if (m_TestType == TEST_TYPE_ENCODE &&
        m_JpgChromaMode == Lwjpg::CHROMA_MODE_YUV420)
    {
        // Bug 1615038: 420 encoding only supports even width/height
        m_ImageWidth  = ALIGN_UP(m_ImageWidth, 2U);
        m_ImageHeight = ALIGN_UP(m_ImageHeight, 2U);
    }

    // Print the picked values
    //
    Printf(pri, "Picks:\n");
    Printf(pri, "    TestType        = %u\n", m_TestType);
    Printf(pri, "    ImageWidth      = %u\n", m_ImageWidth);
    Printf(pri, "    ImageHeight     = %u\n", m_ImageHeight);
    Printf(pri, "    PixelFormat     = %u\n", m_PixelFormat);
    Printf(pri, "    ImgChromaMode   = %u\n", m_ImgChromaMode);
    Printf(pri, "    JpgChromaMode   = %u\n", m_JpgChromaMode);
    Printf(pri, "    YuvMemoryMode   = %u\n", m_YuvMemoryMode);
    Printf(pri, "    RestartInterval = %u\n", m_RestartInterval);
    Printf(pri, "    UseByteStuffing = %u\n", m_UseByteStuffing);
    Printf(pri, "    UseRandomImages = %u\n", m_UseRandomImages);
    Printf(pri, "    UseRandomTables = %u\n", m_UseRandomTables);

    // Initialize other per-loop parameters
    //
    CreateRandomHuffmanTable(pRandom, Lwjpg::HUFFTAB_DC,
                             &m_RandomDcLumaHuffman);
    CreateRandomHuffmanTable(pRandom, Lwjpg::HUFFTAB_DC,
                             &m_RandomDcChromaHuffman);
    CreateRandomHuffmanTable(pRandom, Lwjpg::HUFFTAB_AC,
                             &m_RandomAcLumaHuffman);
    CreateRandomHuffmanTable(pRandom, Lwjpg::HUFFTAB_AC,
                             &m_RandomAcChromaHuffman);
    CreateRandomQuantTable(pRandom, m_RandomLumaQuant);
    CreateRandomQuantTable(pRandom, m_RandomChromaQuant);
    m_DstBytesWritten = 0;
    return OK;
}

//---------------------------------------------------------------------
//! \brief Use LWJPG to encode a random image
//!
//! This method implements one loop of the test when m_TestType is
//! TEST_TYPE_ENCODE.  After this method writes the encoded image into
//! m_DstSurface/m_DstBytesWritten, the caller will run golden values
//! on it.
//!
RC LwjpgTest::RunEncodeTest()
{
    const vector<Dimensions> planeDimensions = GetPlaneDimensions();
    const Dimensions mlwBlockDimensions = GetMlwBlockDimensions();
    const Dimensions mlwPixelDimensions(
            mlwBlockDimensions.width * Lwjpg::BLOCK_WIDTH,
            mlwBlockDimensions.height * Lwjpg::BLOCK_HEIGHT);
    const GpuTestConfiguration *pTestConfig = GetTestConfiguration();
    const Tee::Priority pri = GetVerbosePrintPri();
    RC rc;

    // Fill the encoder setup struct
    //
    lwjpg_enc_drv_pic_setup_s setup = {0};
    setup.bitstream_start_off = 0;
    setup.bitstream_buf_size = static_cast<unsigned>(m_DstSurface.GetSize());
    setup.luma_stride = ALIGN_UP(planeDimensions[0].width, Lwjpg::STRIDE_ALIGN);
    if (planeDimensions.size() > 1)
    {
        setup.chroma_stride = ALIGN_UP(planeDimensions[1].width,
                                       Lwjpg::STRIDE_ALIGN);
    }
    setup.inputType = m_PixelFormat;
    setup.chromaFormat = ChromaMode2ChromaFormat(m_ImgChromaMode);
    setup.tilingMode = Lwjpg::TILING_MODE_LINEAR;               // LWJPG 1.0
    setup.gobHeight = Lwjpg::GOB_HEIGHT_2;                      // LWJPG 1.0
    setup.yuvMemoryMode = m_YuvMemoryMode;
    setup.imageWidth = m_ImageWidth;
    setup.imageHeight = m_ImageHeight;
    setup.jpegWidth = ALIGN_UP(m_ImageWidth, mlwPixelDimensions.width);
    setup.jpegHeight = ALIGN_UP(m_ImageHeight, mlwPixelDimensions.height);
    setup.widthMlw = setup.jpegWidth / mlwPixelDimensions.width;
    setup.heightMlw = setup.jpegHeight / mlwPixelDimensions.height;
    setup.totalMlw = setup.widthMlw * setup.heightMlw;
    setup.restartInterval = m_RestartInterval;
    setup.rateControl = Lwjpg::RATE_CONTROL_DISABLE;            // TODO
    setup.rcTargetYBits = 0;                                    // TODO
    setup.rcTargetCBits = 0;                                    // TODO
    setup.preQuant = false;                                     // TODO
    setup.rcThreshIdx = 0;
    setup.rcThreshMag = 0;
    setup.isMjpgTypeB = !m_UseByteStuffing;
    ColwertHuffmanTable(m_UseRandomTables ? &m_RandomDcLumaHuffman[0]
                                          : &s_StdDcLumaHuffman[0],
                        setup.hfDcLuma, NUMELEMS(setup.hfDcLuma));
    ColwertHuffmanTable(m_UseRandomTables ? &m_RandomAcLumaHuffman[0]
                                          : &s_StdAcLumaHuffman[0],
                        setup.hfAcLuma, NUMELEMS(setup.hfAcLuma));
    ColwertHuffmanTable(m_UseRandomTables ? &m_RandomDcChromaHuffman[0]
                                          : &s_StdDcChromaHuffman[0],
                        setup.hfDcChroma, NUMELEMS(setup.hfDcChroma));
    ColwertHuffmanTable(m_UseRandomTables ? &m_RandomAcChromaHuffman[0]
                                          : &s_StdAcChromaHuffman[0],
                        setup.hfAcChroma, NUMELEMS(setup.hfAcChroma));
    CopyQuantTable(m_UseRandomTables ? m_RandomLumaQuant : s_StdLumaQuant,
                   setup.quantLumaFactor);
    CopyQuantTable(m_UseRandomTables ? m_RandomChromaQuant : s_StdChromaQuant,
                   setup.quantChromaFactor);

    // Fill surfaces
    //
    UINT32 imageSeed = GetFpContext()->Rand.GetRandom();
    UINT08 *pSrcSurface = static_cast<UINT08*>(m_SrcSurface.GetAddress());
    UINT32 endOffset = planeDimensions.back().endOffset;
    if (m_UseRandomImages)
    {
        // Fill image with random values.  Use temporary RNG, so we can
        // disable random images without changing other random picks.
        //
        Random tmpRandom;
        tmpRandom.SeedRandom(imageSeed);
        for (UINT32 ii = 0; ii < endOffset; ii += 4)
            MEM_WR32(&pSrcSurface[ii], tmpRandom.GetRandom());
    }
    else
    {
        for (UINT32 ii = 0; ii < endOffset; ii += 4)
            MEM_WR32(&pSrcSurface[ii], 0);
    }
    Platform::VirtualWr(m_SetupSurface.GetAddress(), &setup, sizeof(setup));
    m_StatusSurface.Fill(0);

    // Write methods to channel
    //
    CHECK_RC(m_pCh->Write(0, LWE7D0_SET_APPLICATION_ID,
                          LWE7D0_SET_APPLICATION_ID_ID_LWJPG_ENCODER));
    CHECK_RC(m_pCh->Write(0, LWE7D0_SET_CONTROL_PARAMS, 0));
    CHECK_RC(m_pCh->WriteWithSurface(
                    0, LWE7D0_SET_IN_DRV_PIC_SETUP, m_SetupSurface,
                    0, Lwjpg::SURFACE_SHIFT));
    CHECK_RC(m_pCh->WriteWithSurface(
                    0, LWE7D0_SET_OUT_STATUS, m_StatusSurface,
                    0, Lwjpg::SURFACE_SHIFT));
    CHECK_RC(m_pCh->WriteWithSurface(
                    0, LWE7D0_SET_BITSTREAM, m_DstSurface,
                    0, Lwjpg::SURFACE_SHIFT));
    CHECK_RC(m_pCh->WriteWithSurface(0, LWE7D0_SET_LWR_PIC, m_SrcSurface,
                                     0, Lwjpg::SURFACE_SHIFT));
    if (m_PixelFormat == Lwjpg::PIXEL_FORMAT_YUV &&
        m_ImgChromaMode != Lwjpg::CHROMA_MODE_MONOCHROME)
    {
        UINT32 chroma_u = (planeDimensions.size() > 1 ?
                           planeDimensions[0].endOffset : 0);
        UINT32 chroma_v = (planeDimensions.size() > 2 ?
                           planeDimensions[1].endOffset : chroma_u);
        CHECK_RC(m_pCh->WriteWithSurface(0, LWE7D0_SET_LWR_PIC_CHROMA_U,
                                         m_SrcSurface, chroma_u,
                                         Lwjpg::SURFACE_SHIFT));
        CHECK_RC(m_pCh->WriteWithSurface(0, LWE7D0_SET_LWR_PIC_CHROMA_V,
                                         m_SrcSurface, chroma_v,
                                         Lwjpg::SURFACE_SHIFT));
    }
    CHECK_RC(m_pCh->Write(0, LWE7D0_EXELWTE,
                          DRF_DEF(E7D0, _EXELWTE, _AWAKEN, _ENABLE)));
    CHECK_RC(m_pCh->Flush());
    CHECK_RC(m_pCh->WaitIdle(pTestConfig->TimeoutMs()));

    // Process the status returned from LWJPG
    //
    lwjpg_enc_status status;
    Platform::VirtualRd(m_StatusSurface.GetAddress(), &status, sizeof(status));
    Printf(pri, "Encoder done: cycle_count=%d bitstream_size=%d MLW=(%d,%d)\n",
           status.cycle_count, status.bitstream_size,
           status.mlw_x, status.mlw_y);
    if (status.error_status != LWE7D0_ERROR_NONE)
    {
        Printf(Tee::PriHigh, "LWJPG encoder failed with status 0x%x\n",
               status.error_status);
        return RC::ENCRYPT_DECRYPT_FAILED;
    }

    m_DstBytesWritten = status.bitstream_size;
    return OK;
}

//---------------------------------------------------------------------
//! \brief Use LWJPG to decode a random image
//!
//! This method implements one loop of the test when m_TestType is
//! TEST_TYPE_DECODE.  After this method writes the decoded image into
//! m_DstSurface/m_DstBytesWritten, the caller will run golden values
//! on it.
//!
RC LwjpgTest::RunDecodeTest()
{
    const vector<Dimensions> planeDimensions = GetPlaneDimensions();
    const Dimensions mlwBlockDimensions = GetMlwBlockDimensions();
    const Dimensions mlwPixelDimensions(
            mlwBlockDimensions.width * Lwjpg::BLOCK_WIDTH,
            mlwBlockDimensions.height * Lwjpg::BLOCK_HEIGHT);
    const GpuTestConfiguration *pTestConfig = GetTestConfiguration();
    const Tee::Priority pri = GetVerbosePrintPri();
    RC rc;

    // Fill the decoder setup struct
    //
    lwjpg_dec_drv_pic_setup_s setup = {{{{{0}}}}};

    ColwertHuffmanTable(&m_RandomDcLumaHuffman[0],
                        &setup.huffTab[Lwjpg::HUFFTAB_DC][0]);
    ColwertHuffmanTable(&m_RandomAcLumaHuffman[0],
                        &setup.huffTab[Lwjpg::HUFFTAB_AC][0]);
    ColwertHuffmanTable(s_StdDcLumaHuffman,
                        &setup.huffTab[Lwjpg::HUFFTAB_DC][1]);
    ColwertHuffmanTable(s_StdAcLumaHuffman,
                        &setup.huffTab[Lwjpg::HUFFTAB_AC][1]);
    ColwertHuffmanTable(&m_RandomDcChromaHuffman[0],
                        &setup.huffTab[Lwjpg::HUFFTAB_DC][2]);
    ColwertHuffmanTable(&m_RandomAcChromaHuffman[0],
                        &setup.huffTab[Lwjpg::HUFFTAB_AC][2]);
    ColwertHuffmanTable(s_StdDcChromaHuffman,
                        &setup.huffTab[Lwjpg::HUFFTAB_DC][3]);
    ColwertHuffmanTable(s_StdAcChromaHuffman,
                        &setup.huffTab[Lwjpg::HUFFTAB_AC][3]);

    setup.blkPar[0].hblock = mlwBlockDimensions.width;
    setup.blkPar[0].vblock = mlwBlockDimensions.height;
    setup.blkPar[0].quant = m_UseRandomTables ? 0 : 1;
    setup.blkPar[0].ac = m_UseRandomTables ? 0 : 1;
    setup.blkPar[0].dc = m_UseRandomTables ? 0 : 1;
    if (m_JpgChromaMode != Lwjpg::CHROMA_MODE_MONOCHROME)
    {
        setup.blkPar[1].hblock = 1;
        setup.blkPar[1].vblock = 1;
        setup.blkPar[1].quant = m_UseRandomTables ? 2 : 3;
        setup.blkPar[1].ac = m_UseRandomTables ? 2 : 3;
        setup.blkPar[1].dc = m_UseRandomTables ? 2 : 3;
        setup.blkPar[2].hblock = 1;
        setup.blkPar[2].vblock = 1;
        setup.blkPar[2].quant = m_UseRandomTables ? 2 : 3;
        setup.blkPar[2].ac = m_UseRandomTables ? 2 : 3;
        setup.blkPar[2].dc = m_UseRandomTables ? 2 : 3;
    }
    CopyQuantTable(m_RandomLumaQuant, setup.quant[0]);
    CopyQuantTable(s_StdLumaQuant, setup.quant[1]);
    CopyQuantTable(m_RandomChromaQuant, setup.quant[2]);
    CopyQuantTable(s_StdChromaQuant, setup.quant[3]);
    setup.restart_interval = m_RestartInterval;
    setup.frame_width = m_ImageWidth;
    setup.frame_height = m_ImageHeight;
    setup.mlw_width = (ALIGN_UP(m_ImageWidth, mlwPixelDimensions.width) /
                       mlwPixelDimensions.width);
    setup.mlw_height = (ALIGN_UP(m_ImageHeight, mlwPixelDimensions.height) /
                        mlwPixelDimensions.height);
    setup.comp = (m_JpgChromaMode == Lwjpg::CHROMA_MODE_MONOCHROME) ? 1 : 3;
    setup.bitstream_offset = 0;
    setup.bitstream_size = 0;                                  // Fill in later
    setup.stream_chroma_mode = m_JpgChromaMode;
    setup.output_chroma_mode = m_ImgChromaMode;
    setup.output_pixel_format = m_PixelFormat;
    setup.output_stride_luma = ALIGN_UP(planeDimensions[0].width,
                                        Lwjpg::STRIDE_ALIGN);
    if (planeDimensions.size() > 1)
    {
        setup.output_stride_chroma = ALIGN_UP(planeDimensions[1].width,
                                              Lwjpg::STRIDE_ALIGN);
    }
    setup.alpha_value = 0xff;
    setup.yuv2rgb_param[Lwjpg::YUV2RGB_K0] = 65536;          // See lwjpg_api.h
    setup.yuv2rgb_param[Lwjpg::YUV2RGB_K1] = 91881;
    setup.yuv2rgb_param[Lwjpg::YUV2RGB_K2] = -22553;
    setup.yuv2rgb_param[Lwjpg::YUV2RGB_K3] = -46802;
    setup.yuv2rgb_param[Lwjpg::YUV2RGB_K4] = 116130;
    setup.yuv2rgb_param[Lwjpg::YUV2RGB_C]  = 0;
    setup.tile_mode = Lwjpg::TILING_MODE_LINEAR;                // LWJPG 1.0
    setup.block_linear_height = 0;                              // LWJPG 1.0
    setup.memory_mode = m_YuvMemoryMode;
    setup.power2_downscale = Lwjpg::DOWNSCALE_NONE;             // TODO
    setup.motion_jpeg_type = (m_UseByteStuffing ? Lwjpg::MOTION_JPEG_TYPE_A
                                                : Lwjpg::MOTION_JPEG_TYPE_B);
    setup.start_mlw_x = 0;                                      // TODO
    setup.start_mlw_y = 0;                                      // TODO

    // Create random JPG image.  Use temporary RNG, so we can disable
    // random images without changing other random picks.
    //
    Random tmpRandom;
    tmpRandom.SeedRandom(GetFpContext()->Rand.GetRandom());
    UINT08 *pSrcSurface = static_cast<UINT08*>(m_SrcSurface.GetAddress());
    EcsBitstream bitstream(pSrcSurface,
                           static_cast<size_t>(m_SrcSurface.GetSize()),
                           m_UseByteStuffing);

    struct CompData
    {
        UINT32 numBlocks;
        UINT08 quant[Lwjpg::BLOCK_SIZE];
        vector<huffman_symbol_s> dcHuffman;
        vector<huffman_symbol_s> acHuffman;
        INT32 prevDc;
    };
    CompData compData[CT_NUMELEMS(setup.blkPar)];
    size_t numComps = setup.comp;
    for (size_t compIdx = 0; compIdx < numComps; ++compIdx)
    {
        CompData *pCompData = &compData[compIdx];
        const block_parameter_s &blkPar = setup.blkPar[compIdx];
        pCompData->numBlocks = blkPar.vblock * blkPar.hblock;
        CopyQuantTable(setup.quant[blkPar.quant], pCompData->quant);
        pCompData->dcHuffman.resize(DCVALUEITEM);
        ColwertHuffmanTable(setup.huffTab[Lwjpg::HUFFTAB_DC][blkPar.dc],
                            &pCompData->dcHuffman[0], DCVALUEITEM);
        pCompData->acHuffman.resize(ACVALUEITEM);
        ColwertHuffmanTable(setup.huffTab[Lwjpg::HUFFTAB_AC][blkPar.ac],
                            &pCompData->acHuffman[0], ACVALUEITEM);
        pCompData->prevDc = 0;
    }

    const UINT32 numMlws = setup.mlw_width * setup.mlw_height;
    for (UINT32 mlwIdx = 0; mlwIdx < numMlws; ++mlwIdx)
    {
        if (m_RestartInterval != 0 &&
            (mlwIdx % m_RestartInterval) == 0 &&
            mlwIdx > 0)
        {
            UINT08 rstNum = ((mlwIdx - 1) / m_RestartInterval) & 0x07;
            CHECK_RC(bitstream.WriteMarker(0xd0 | rstNum));
            for (size_t compIdx = 0; compIdx < numComps; ++compIdx)
                compData[compIdx].prevDc = 0;
        }
        for (size_t compIdx = 0; compIdx < numComps; ++compIdx)
        {
            CompData *pCompData = &compData[compIdx];
            for (UINT32 blockIdx = 0;
                 blockIdx < pCompData->numBlocks; ++blockIdx)
            {
                CHECK_RC(WriteRandomEcsBlock(
                                &tmpRandom, pCompData->quant,
                                pCompData->dcHuffman, pCompData->acHuffman,
                                &pCompData->prevDc, &bitstream));
            }
        }
    }
    CHECK_RC(bitstream.Flush());
    size_t bitstreamSize = bitstream.GetBitPosition() / 8;
    bool lastByteIsStuffed =
                (m_UseByteStuffing &&
                 bitstreamSize >= 2 &&
                 MEM_RD08(&pSrcSurface[bitstreamSize - 2]) == 0xff &&
                 MEM_RD08(&pSrcSurface[bitstreamSize - 1]) == 0x00);
    setup.bitstream_size = static_cast<int>(bitstreamSize);

    // Fill other surfaces
    //
    Platform::VirtualWr(m_SetupSurface.GetAddress(), &setup, sizeof(setup));
    UINT08 *pDstSurface = static_cast<UINT08*>(m_DstSurface.GetAddress());
    for (UINT32 ii = 0; ii < planeDimensions.back().endOffset; ii += 4)
        MEM_WR32(&pDstSurface[ii], 0);
    m_StatusSurface.Fill(0);

    // Write methods to channel
    //
    CHECK_RC(m_pCh->Write(0, LWE7D0_SET_APPLICATION_ID,
                          LWE7D0_SET_APPLICATION_ID_ID_LWJPG_DECODER));
    CHECK_RC(m_pCh->Write(0, LWE7D0_SET_CONTROL_PARAMS, 0));
    CHECK_RC(m_pCh->WriteWithSurface(
                    0, LWE7D0_SET_IN_DRV_PIC_SETUP, m_SetupSurface,
                    0, Lwjpg::SURFACE_SHIFT));
    CHECK_RC(m_pCh->WriteWithSurface(
                    0, LWE7D0_SET_OUT_STATUS, m_StatusSurface,
                    0, Lwjpg::SURFACE_SHIFT));
    CHECK_RC(m_pCh->WriteWithSurface(
                    0, LWE7D0_SET_BITSTREAM, m_SrcSurface,
                    0, Lwjpg::SURFACE_SHIFT));
    CHECK_RC(m_pCh->WriteWithSurface(0, LWE7D0_SET_LWR_PIC, m_DstSurface,
                                     0, Lwjpg::SURFACE_SHIFT));
    if (m_PixelFormat == Lwjpg::PIXEL_FORMAT_YUV &&
        m_ImgChromaMode != Lwjpg::CHROMA_MODE_MONOCHROME)
    {
        UINT32 chroma_u = (planeDimensions.size() > 1 ?
                           planeDimensions[0].endOffset : 0);
        UINT32 chroma_v = (planeDimensions.size() > 2 ?
                           planeDimensions[1].endOffset : chroma_u);
        CHECK_RC(m_pCh->WriteWithSurface(0, LWE7D0_SET_LWR_PIC_CHROMA_U,
                                         m_DstSurface, chroma_u,
                                         Lwjpg::SURFACE_SHIFT));
        CHECK_RC(m_pCh->WriteWithSurface(0, LWE7D0_SET_LWR_PIC_CHROMA_V,
                                         m_DstSurface, chroma_v,
                                         Lwjpg::SURFACE_SHIFT));
    }
    CHECK_RC(m_pCh->Write(0, LWE7D0_EXELWTE,
                          DRF_DEF(E7D0, _EXELWTE, _AWAKEN, _ENABLE)));
    CHECK_RC(m_pCh->Flush());
    CHECK_RC(m_pCh->WaitIdle(pTestConfig->TimeoutMs()));

    // Process the status returned from LWJPG.  Note that we require
    // the decoder to either read the exact number of bytes we wrote
    // to the JPG bitstream, or one fewer byte if the last byte was
    // just a byte-stuffed 0x00.
    //
    lwjpg_dec_status status;
    Platform::VirtualRd(m_StatusSurface.GetAddress(), &status, sizeof(status));
    Printf(pri, "Decoder done: cycle_count=%d bytes_offset=%d MLW=(%d,%d)\n",
           status.cycle_count, status.bytes_offset,
           status.mlw_x, status.mlw_y);
    if (status.error_status != LWE7D0_ERROR_NONE)
    {
        Printf(Tee::PriHigh, "LWJPG decoder failed with status 0x%x\n",
               status.error_status);
        return RC::ENCRYPT_DECRYPT_FAILED;
    }
    if (status.bytes_offset != bitstreamSize &&
        !(lastByteIsStuffed && status.bytes_offset == bitstreamSize - 1))
    {
        Printf(Tee::PriHigh,
               "LWJPG decoder read wrong number of bytes:"
               " expected = %u, actual = %u\n",
               static_cast<unsigned>(bitstreamSize), status.bytes_offset);
        return RC::MEMORY_SIZE_MISMATCH;
    }

    m_DstBytesWritten = planeDimensions.back().endOffset;
    return OK;
}

//---------------------------------------------------------------------
//! \brief Print the contents of a buffer.  Only used for debugging.
//!
void LwjpgTest::DumpBuffer
(
    const string &title,
    const void *pAddr,
    size_t size
)
{
    const Tee::Priority pri = GetVerbosePrintPri();
    const UINT08 *pData = static_cast<const UINT08*>(pAddr);

    if (!title.empty())
        Printf(pri, "%s:\n", title.c_str());
    for (size_t ii = 0; ii < size; ii += 16)
    {
        Printf(pri, "    %06x:", static_cast<unsigned>(ii));
        for (size_t jj = ii; jj < min(ii + 16, size); ++jj)
            Printf(pri, " %02x", 0x00ff & MEM_RD08(&pData[jj]));
        Printf(pri, "\n");
    }
}

//--------------------------------------------------------------------
// JavaScript interface
//
JS_CLASS_INHERIT(LwjpgTest, GpuTest, "LwjpgTest object");
CLASS_PROP_READWRITE(LwjpgTest, KeepRunning, bool,
                     "The test will keep running as long as this flag is set");
