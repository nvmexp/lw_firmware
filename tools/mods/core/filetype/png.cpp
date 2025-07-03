/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2014,2016,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/massert.h"
#include "core/include/imagefil.h"
#include "core/include/utility.h"
#include "png.h"
#include "core/include/platform.h"
#include "core/include/fileholder.h"
#include <vector>

/*
 * PNG Reader Helper Functions.
 */

// Cleanup memory and file handles allocated when reading a PNG file.
void PNGReadCleanUp(png_structp& pngPtr,
                    png_infop& pngInfo,
                    png_infop& pngEnd)
{
    // cleanup png data structures
    if (pngPtr)
    {
        if (pngEnd)
            png_destroy_info_struct(pngPtr, &pngEnd);
        if (pngInfo)
            png_destroy_info_struct(pngPtr, &pngInfo);
        png_destroy_read_struct(&pngPtr, (png_infopp) NULL, (png_infopp) NULL);
    }
}

// Read the header information for a PNG file.
// Return number of header bytes read (0 on failure).
int PNGReadHeaderInfo(FileHolder &fp,
                      png_structp& pngPtr,
                      png_infop& pngInfo,
                      png_infop& pngEnd)
{
    // read header and check the file is actually a png
    unsigned char header[8] = {0,0,0,0,0,0,0,0};
    size_t x = fread(header, 1, 8, fp.GetFile());
    if (png_sig_cmp(header, 0, x))
    {
        PNGReadCleanUp(pngPtr, pngInfo, pngEnd);
        return 0;
    }

    // create read structure
    pngPtr = png_create_read_struct(
        PNG_LIBPNG_VER_STRING,
        png_voidp_NULL,
        png_error_ptr_NULL,
        png_error_ptr_NULL);
    if (!pngPtr)
        return 0;

    // create info structure
    pngInfo = png_create_info_struct(pngPtr);
    if (!pngInfo)
        return 0;

    // create end info structure
    pngEnd = png_create_info_struct(pngPtr);
    if (!pngEnd)
        return 0;

    // header successfully read
    return (int)x;
}

// Return the color format best matching the data stored in the PNG file.
// This is determined by examining the PNG file header.
ColorUtils::Format FormatOfPNGFile(const char *FileName)
{
    // pointers to dynamically allocated memory
    FileHolder fp;
    png_structp pngPtr = NULL;
    png_infop pngInfo = NULL;
    png_infop pngEnd = NULL;

    // open the file
    if (OK != fp.Open(FileName, "rb"))
        return ColorUtils::LWFMT_NONE;

    // read the header
    int header_bytes = PNGReadHeaderInfo(fp, pngPtr, pngInfo, pngEnd);
    if (header_bytes == 0)
    {
        PNGReadCleanUp(pngPtr, pngInfo, pngEnd);
        return ColorUtils::LWFMT_NONE;
    }

    // set failure point for png functions
    if (setjmp(pngPtr->jmpbuf))
    {
        PNGReadCleanUp(pngPtr, pngInfo, pngEnd);
        return ColorUtils::LWFMT_NONE;
    }

    // initialize and let libpng know we read the file header
    png_init_io(pngPtr, fp.GetFile());
    png_set_sig_bytes(pngPtr, header_bytes);

    // read png color information
    png_read_info(pngPtr, pngInfo);
    int color_type = png_get_color_type(pngPtr, pngInfo);
    int bit_depth = png_get_bit_depth(pngPtr, pngInfo);

    // determine the format from the color type and bit depth
    ColorUtils::Format fmt;
    if ((color_type == PNG_COLOR_TYPE_PALETTE) && (bit_depth == 1))
        fmt = ColorUtils::Y8;
    else if ((color_type == PNG_COLOR_TYPE_PALETTE) && (bit_depth == 2))
        fmt = ColorUtils::Y8;
    else if ((color_type == PNG_COLOR_TYPE_PALETTE) && (bit_depth == 4))
        fmt = ColorUtils::Y8;
    else if ((color_type == PNG_COLOR_TYPE_PALETTE) && (bit_depth == 8))
        fmt = ColorUtils::Y8;
    else if ((color_type == PNG_COLOR_TYPE_GRAY) && (bit_depth == 1))
        fmt = ColorUtils::Y8;
    else if ((color_type == PNG_COLOR_TYPE_GRAY) && (bit_depth == 2))
        fmt = ColorUtils::Y8;
    else if ((color_type == PNG_COLOR_TYPE_GRAY) && (bit_depth == 4))
        fmt = ColorUtils::Y8;
    else if ((color_type == PNG_COLOR_TYPE_GRAY) && (bit_depth == 8))
        fmt = ColorUtils::Y8;
    else if ((color_type == PNG_COLOR_TYPE_GRAY) && (bit_depth == 16))
        fmt = ColorUtils::Z16;
    else if ((color_type == PNG_COLOR_TYPE_GRAY_ALPHA) && (bit_depth == 8))
        fmt = ColorUtils::LWFMT_NONE;
    else if ((color_type == PNG_COLOR_TYPE_GRAY_ALPHA) && (bit_depth == 16))
        fmt = ColorUtils::LWFMT_NONE;
    else if ((color_type == PNG_COLOR_TYPE_RGB) && (bit_depth == 8))
        fmt = ColorUtils::B8_G8_R8;
    else if ((color_type == PNG_COLOR_TYPE_RGB) && (bit_depth == 16))
        fmt = ColorUtils::LWFMT_NONE;
    else if ((color_type == PNG_COLOR_TYPE_RGB_ALPHA) && (bit_depth == 8))
        fmt = ColorUtils::R8G8B8A8;
    else if ((color_type == PNG_COLOR_TYPE_RGB_ALPHA) && (bit_depth == 16))
        fmt = ColorUtils::RF16_GF16_BF16_AF16;
    else
        fmt = ColorUtils::LWFMT_NONE;

    // read png text comments
    png_textp textPtr;
    int num_comments = png_get_text(pngPtr, pngInfo, &textPtr, NULL);

    // determine the format from the text comments
    // (if they contain a format string)
    ColorUtils::Format fmt_text = ColorUtils::LWFMT_NONE;
    int comments_scanned = 0;
    while ((fmt_text == ColorUtils::LWFMT_NONE) && (comments_scanned < num_comments))
    {
        fmt_text = ColorUtils::StringToFormat(string(textPtr[comments_scanned].key));
        comments_scanned++;
    }

    // format specified in text comments takes precendence
    if (fmt_text != ColorUtils::LWFMT_NONE)
        fmt = fmt_text;

    // cleanup
    PNGReadCleanUp(pngPtr, pngInfo, pngEnd);
    return fmt;
}

// Return the storage format used for color data in the PNG file.
// This is determined by examining the PNG file header.
ColorUtils::PNGFormat PNGFormatOfPNGFile(const char *FileName)
{
    // pointers to dynamically allocated memory
    FileHolder fp;
    png_structp pngPtr = NULL;
    png_infop pngInfo = NULL;
    png_infop pngEnd = NULL;

    // open the file
    if (OK != fp.Open(FileName, "rb"))
        return ColorUtils::LWPNGFMT_NONE;

    // read the header
    int header_bytes = PNGReadHeaderInfo(fp, pngPtr, pngInfo, pngEnd);
    if (header_bytes == 0)
    {
        PNGReadCleanUp(pngPtr, pngInfo, pngEnd);
        return ColorUtils::LWPNGFMT_NONE;
    }

    // set failure point for png functions
    if (setjmp(pngPtr->jmpbuf))
    {
        PNGReadCleanUp(pngPtr, pngInfo, pngEnd);
        return ColorUtils::LWPNGFMT_NONE;
    }

    // initialize and let libpng know we read the file header
    png_init_io(pngPtr, fp.GetFile());
    png_set_sig_bytes(pngPtr, header_bytes);

    // read png color information
    png_read_info(pngPtr, pngInfo);
    int color_type = png_get_color_type(pngPtr, pngInfo);
    int bit_depth = png_get_bit_depth(pngPtr, pngInfo);

    // determine the format from the color type and bit depth
    ColorUtils::PNGFormat fmt;
    if ((color_type == PNG_COLOR_TYPE_PALETTE) && (bit_depth == 1))
        fmt = ColorUtils::C1;
    else if ((color_type == PNG_COLOR_TYPE_PALETTE) && (bit_depth == 2))
        fmt = ColorUtils::C2;
    else if ((color_type == PNG_COLOR_TYPE_PALETTE) && (bit_depth == 4))
        fmt = ColorUtils::C4;
    else if ((color_type == PNG_COLOR_TYPE_PALETTE) && (bit_depth == 8))
        fmt = ColorUtils::C8;
    else if ((color_type == PNG_COLOR_TYPE_GRAY) && (bit_depth == 1))
        fmt = ColorUtils::C1;
    else if ((color_type == PNG_COLOR_TYPE_GRAY) && (bit_depth == 2))
        fmt = ColorUtils::C2;
    else if ((color_type == PNG_COLOR_TYPE_GRAY) && (bit_depth == 4))
        fmt = ColorUtils::C4;
    else if ((color_type == PNG_COLOR_TYPE_GRAY) && (bit_depth == 8))
        fmt = ColorUtils::C8;
    else if ((color_type == PNG_COLOR_TYPE_GRAY) && (bit_depth == 16))
        fmt = ColorUtils::C16;
    else if ((color_type == PNG_COLOR_TYPE_GRAY_ALPHA) && (bit_depth == 8))
        fmt = ColorUtils::C8C8;
    else if ((color_type == PNG_COLOR_TYPE_GRAY_ALPHA) && (bit_depth == 16))
        fmt = ColorUtils::C16C16;
    else if ((color_type == PNG_COLOR_TYPE_RGB) && (bit_depth == 8))
        fmt = ColorUtils::C8C8C8;
    else if ((color_type == PNG_COLOR_TYPE_RGB) && (bit_depth == 16))
        fmt = ColorUtils::C16C16C16;
    else if ((color_type == PNG_COLOR_TYPE_RGB_ALPHA) && (bit_depth == 8))
        fmt = ColorUtils::C8C8C8C8;
    else if ((color_type == PNG_COLOR_TYPE_RGB_ALPHA) && (bit_depth == 16))
        fmt = ColorUtils::C16C16C16C16;
    else
        fmt = ColorUtils::LWPNGFMT_NONE;

    // read png text comments
    png_textp textPtr;
    int num_comments = png_get_text(pngPtr, pngInfo, &textPtr, NULL);

    // determine the format from the text comments
    // (if they contain a format string)
    ColorUtils::PNGFormat fmt_text = ColorUtils::LWPNGFMT_NONE;
    int comments_scanned = 0;
    while ((fmt_text == ColorUtils::LWPNGFMT_NONE) && (comments_scanned < num_comments))
    {
        fmt_text = ColorUtils::StringToPNGFormat(string(textPtr[comments_scanned].key));
        comments_scanned++;
    }
    if (fmt_text == ColorUtils::LWPNGFMT_NONE)
        fmt_text = fmt;

    // if the formats are compatible (same #bits/pixel) then
    // the one specified in the text comments takes precedence
    if (ColorUtils::PixelBytes(fmt) == ColorUtils::PixelBytes(fmt_text))
        fmt = fmt_text;

    // cleanup
    PNGReadCleanUp(pngPtr, pngInfo, pngEnd);
    return fmt;
}

// Extract the current pixel into pixel_copy.
// Update the pixel pointer and bit count to point to the start of the next pixel.
void PNGReadNextPixel(UINT08 **pixel, int *bit_count, int bit_depth, UINT08 *pixel_copy)
{
    while (bit_depth > 0)
    {
        // grab rest of current byte
        UINT08 p_byte = ((*pixel)[0]) << (*bit_count);
        int bits_read = (8 - (*bit_count));
        // grab start of next byte (if needed)
        if (bit_depth > bits_read)
        {
            p_byte = p_byte | (((*pixel)[1]) >> bits_read);
            bits_read = 8;
        }
        // write the pixel
        *pixel_copy = **pixel;
        pixel_copy++;
        // adjust source pointer, bit_depth remaining
        if (bit_depth >= bits_read)
        {
            // increment location by a byte
            (*pixel)++;
            bit_depth -= bits_read;
        }
        else
        {
            // increment location by bit_depth
            (*bit_count) += bits_read;
            if (*bit_count >= 8)
            {
                (*pixel)++;
                (*bit_count) -= 8;
            }
            bit_depth = 0;
        }
    }
}

// Tile an image in memory.
void PNGMemoryTile(void *   Address,
                   UINT32   SrcWidth,
                   UINT32   SrcHeight,
                   UINT32   PixelBytes,
                   UINT32   Pitch,
                   UINT32   DstWidth,
                   UINT32   DstHeight)
{
    MASSERT(SrcWidth > 0);
    MASSERT(SrcHeight > 0);

    UINT32 BpSrcRow = PixelBytes * SrcWidth;     // Bytes Per Source Row
    UINT32 BpDstRow = PixelBytes * DstWidth;     // Bytes Per Destination Row

    UINT08 * pRowBegin   = static_cast<UINT08*>(Address);
    UINT08 * pRowEnd     = pRowBegin + BpDstRow;

    for ( UINT32 row = 0; row < DstHeight; ++row )
    {
        UINT08 * pSrc = static_cast<UINT08*>(Address) + Pitch * (row % SrcHeight);
        UINT08 * pDst = pRowBegin;

        if ( row < SrcHeight )
            pDst += BpSrcRow;

        while ( pDst < pRowEnd )
        {
            UINT32 BytesToCopy = (UINT32)(pRowEnd - pDst);
            if ( BytesToCopy > BpSrcRow )
                BytesToCopy = BpSrcRow;

            Platform::MemCopy(pDst, pSrc, BytesToCopy);
            pDst += BytesToCopy;
        }
        pRowBegin += Pitch;
        pRowEnd   += Pitch;
    }
}

/*
 * Read PNG files using the libpng library
 */
RC ImageFile::ReadPng
(
    const char *        FileName,
    bool                DoTile,
    ColorUtils::Format  MemColorFormat,
    void *              MemAddress,
    UINT32              MemWidth,
    UINT32              MemHeight,
    UINT32              MemPitch
)
{
    RC rc = OK;

    // pointers to dynamically allocated memory
    FileHolder fp;
    png_structp pngPtr = NULL;
    png_infop pngInfo = NULL;
    png_infop pngEnd = NULL;

    // get the internal color format used by the png file
    ColorUtils::PNGFormat PngColorFormat = PNGFormatOfPNGFile(FileName);
    if (PngColorFormat == ColorUtils::LWPNGFMT_NONE)
        return RC::ILWALID_FILE_FORMAT;

    // open the file
    CHECK_RC(fp.Open(FileName, "rb"));

    // read the header
    int header_bytes = PNGReadHeaderInfo(fp, pngPtr, pngInfo, pngEnd);
    if (header_bytes == 0)
    {
        PNGReadCleanUp(pngPtr, pngInfo, pngEnd);
        return RC::ILWALID_FILE_FORMAT;
    }

    // set failure point for png functions
    if (setjmp(pngPtr->jmpbuf))
    {
        PNGReadCleanUp(pngPtr, pngInfo, pngEnd);
        return RC::ILWALID_FILE_FORMAT;
    }

    // initialize and let libpng know we read the file header
    png_init_io(pngPtr, fp.GetFile());
    png_set_sig_bytes(pngPtr, header_bytes);

    // read png file info
    png_read_info(pngPtr, pngInfo);
    UINT32 width  = png_get_image_width(pngPtr, pngInfo);
    UINT32 height = png_get_image_height(pngPtr, pngInfo);

    // get color format sizes
    const UINT32 MemPixelBytes = ColorUtils::PixelBytes(MemColorFormat);
    const UINT32 PngPixelBytes = ColorUtils::PixelBytes(PngColorFormat);
    const UINT32 PngPixelBits = ColorUtils::PixelBits(PngColorFormat);

    // allocate memory for the image, setup row pointers
    vector<UINT08> png_img_buf(width * height * PngPixelBytes);
    vector<png_bytep> row_pointers(height);
    for (UINT32 y = 0; y < height; y++)
        row_pointers[y] = (png_bytep) (&png_img_buf[0] + y * width * PngPixelBytes);

    // read the image into temporary memory
    png_read_image(pngPtr, &row_pointers[0]);

    // allocate temporary memory for pixel colwersion
    vector<UINT08> pixel_copy(PngPixelBytes);
    vector<UINT08> pixel_mem(MemPixelBytes);

    // clip the image if it is too large
    if (height > MemHeight)
        height = MemHeight;
    if (width > MemWidth)
        width = MemWidth;

    // copy from temporary memory into actual image memory
    // performing color colwersion as needed
    for (UINT32 row = 0; row < height; row++)
    {
        // initialize position to start of row
        UINT08 *pMem = row * MemPitch + (UINT08*)(MemAddress);
        UINT08 *pixel = row_pointers[row];
        int bit_count = 0;
        // copy the row
        for (UINT32 col = 0; col < width; col++)
        {
            // extract current pixel and update pointer to next pixel
            PNGReadNextPixel(&pixel, &bit_count, PngPixelBits, &pixel_copy[0]);

            // perform color colwersion
            ColorUtils::Colwert(
                (const char*)(&pixel_copy[0]),
                PngColorFormat,
                (char*)(&pixel_mem[0]),
                MemColorFormat);

            // store pixel in memory
            Platform::MemCopy(pMem + MemPixelBytes*col, &pixel_mem[0], MemPixelBytes);
        }
    }

    // finish the read
    png_read_end(pngPtr, pngEnd);
    PNGReadCleanUp(pngPtr, pngInfo, pngEnd);

    // tile the image if requested
    if (DoTile)
        PNGMemoryTile(MemAddress, width, height, MemPixelBytes, MemPitch, MemWidth, MemHeight);

    return OK;
}

/*
 * PNG Writer Helper Functions.
 */

// Cleanup memory and file handles allocated when writing a PNG file.
void PNGWriteCleanUp(png_structp& pngPtr,
                     png_infop& pngInfo)
{
    // cleanup png data structures
    if (pngPtr)
    {
        if (pngInfo)
            png_destroy_info_struct(pngPtr, &pngInfo);
        png_destroy_write_struct(&pngPtr, (png_infopp) NULL);
    }
}

// Return the per-pixel bit depth used for storage
// of colors of the given format in a png file.
int PNGFileBitDepthOfPNGFormat(ColorUtils::PNGFormat fmt)
{
    switch (fmt)
    {
        case ColorUtils::C1:
            return 1;
        case ColorUtils::C2:
            return 2;
        case ColorUtils::C4:
            return 4;
        case ColorUtils::C8:
        case ColorUtils::C8C8:
        case ColorUtils::C8C8C8:
        case ColorUtils::C8C8C8C8:
        case ColorUtils::C10C10C10C2:
            return 8;
        case ColorUtils::C16:
        case ColorUtils::C16C16:
        case ColorUtils::C16C16C16:
        case ColorUtils::C16C16C16C16:
            return 16;
        case ColorUtils::LWPNGFMT_NONE:
        default:
            MASSERT(0);
            return 0;
    }
}

// Return the per-pixel bit depth used for storage
// of colors of the given format in a png file.
int PNGFileBitDepthOfFormat(ColorUtils::Format fmt)
{
    return PNGFileBitDepthOfPNGFormat(ColorUtils::FormatToPNGFormat(fmt));
}

// Return the libpng constant representing the color type
// for storage of colors of the given format in a png file.
int PNGFileColorTypeOfPNGFormat(ColorUtils::PNGFormat fmt)
{
    switch (fmt)
    {
        case ColorUtils::C1:
        case ColorUtils::C2:
        case ColorUtils::C4:
        case ColorUtils::C8:
        case ColorUtils::C16:
            return PNG_COLOR_TYPE_GRAY;
        case ColorUtils::C8C8:
        case ColorUtils::C16C16:
            return PNG_COLOR_TYPE_GRAY_ALPHA;
        case ColorUtils::C8C8C8:
        case ColorUtils::C16C16C16:
            return PNG_COLOR_TYPE_RGB;
        case ColorUtils::C8C8C8C8:
        case ColorUtils::C10C10C10C2:
        case ColorUtils::C16C16C16C16:
            return PNG_COLOR_TYPE_RGB_ALPHA;
        case ColorUtils::LWPNGFMT_NONE:
        default:
            MASSERT(0);
            return 0;
    }
}

// Return the libpng constant representing the color type
// for storage of colors of the given format in a png file.
int PNGFileColorTypeOfFormat(ColorUtils::Format fmt)
{
    return PNGFileColorTypeOfPNGFormat(ColorUtils::FormatToPNGFormat(fmt));
}

// Store pixel_copy into the current pixel.
// Update the pixel pointer and bit count to point to the start of the next pixel.
void PNGWriteNextPixel(UINT08 **pixel, int *bit_count, int bit_depth, UINT08 *pixel_copy)
{
    while (bit_depth > 0)
    {
        // grab next bits to be written
        UINT08 read_mask = (bit_depth < 8) ? (0xFF << (8 - bit_depth)) : (0xFF);
        UINT08 p_byte = pixel_copy[0] & read_mask;
        // write up to end of current byte
        UINT08 write_mask = read_mask >> (*bit_count);
        **pixel = ((**pixel) & (~write_mask)) | ((p_byte >> (*bit_count)) & write_mask);
        // compute bits written
        int bits_written = (8 - (*bit_count));
        if (bit_depth < bits_written)
            bits_written = bit_depth;
        // update write position
        (*bit_count) += bits_written;
        bit_depth -= bits_written;
        if (*bit_count >= 8)
        {
            (*pixel)++;
            (*bit_count) -= 8;
        }
        // update read position
        // we don't keep track of bit position in read since we should
        // always consume either less than a full byte or an integer
        // multiple of bytes (with initial write position byte aligned)
        pixel_copy++;
    }
}

/*
 * Write PNG files using the libpng library
 */
RC ImageFile::WritePng
(
    const char *        FileName,
    ColorUtils::Format  MemColorFormat,
    void *              MemAddress,
    UINT32              MemWidth,
    UINT32              MemHeight,
    UINT32              MemPitch,
    bool                AlphaToRgb,         // copy alpha to r,g,b
    bool                YIlwerted           // true if Y0 is bottom
)
{
    RC rc = OK;

    // pointers to dynamically allocated memory
    FileHolder fp;
    png_structp pngPtr = NULL;
    png_infop pngInfo = NULL;

    // open the file
    CHECK_RC(fp.Open(FileName, "wb"));

    // create write structure
    pngPtr = png_create_write_struct(
        PNG_LIBPNG_VER_STRING,
        png_voidp_NULL,
        png_error_ptr_NULL,
        png_error_ptr_NULL);
    if (!pngPtr)
    {
        PNGWriteCleanUp(pngPtr, pngInfo);
        return RC::ILWALID_FILE_FORMAT;
    }

    // create info structure
    pngInfo = png_create_info_struct(pngPtr);
    if (!pngInfo)
    {
        PNGWriteCleanUp(pngPtr, pngInfo);
        return RC::ILWALID_FILE_FORMAT;
    }

    // set failure point for png functions
    if (setjmp(pngPtr->jmpbuf))
    {
        PNGWriteCleanUp(pngPtr, pngInfo);
        return RC::ILWALID_FILE_FORMAT;
    }

    // get the internal color format for the png file
    const ColorUtils::PNGFormat PngColorFormat = ColorUtils::FormatToPNGFormat(MemColorFormat);

    // get color format sizes
    const UINT32 MemPixelBytes = ColorUtils::PixelBytes(MemColorFormat);
    const UINT32 PngPixelBytes = ColorUtils::PixelBytes(PngColorFormat);
    const UINT32 PngPixelBits = ColorUtils::PixelBits(PngColorFormat);

    // initialize
    png_init_io(pngPtr, fp.GetFile());

    // set highest compression setting
    png_set_compression_level(pngPtr, Z_BEST_COMPRESSION);

    // determine bit depth and color type
    int bit_depth  = PNGFileBitDepthOfFormat(MemColorFormat);
    int color_type = PNGFileColorTypeOfFormat(MemColorFormat);

    // set image size, color format, and storage format
    png_set_IHDR(
        pngPtr,
        pngInfo,
        MemWidth,
        MemHeight,
        bit_depth,
        color_type,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT);

    // create the file comments
    // store the original color format and png storage format in text keys
    string fmt_str;
    string png_fmt_str;
    png_text apngText[] =
    {
        { PNG_TEXT_COMPRESSION_NONE, NULL, NULL, 0 },
        { PNG_TEXT_COMPRESSION_NONE, NULL, NULL, 0 },
        { PNG_TEXT_COMPRESSION_NONE, NULL, NULL, 0 },    // will hold Format
        { PNG_TEXT_COMPRESSION_NONE, NULL, NULL, 0 }     // will hold PNGFormat
    };
    // write filename to comments
    apngText[0].key = const_cast<char*>("Title");
    apngText[0].text = const_cast<char*>(FileName);
    apngText[0].text_length = strlen(apngText[0].text);
    // write creation software
    apngText[1].key = const_cast<char*>("Software");
    apngText[1].text = const_cast<char*>("MODS");
    apngText[1].text_length = strlen(apngText[1].text);
    // write original color format
    fmt_str = ColorUtils::FormatToString(MemColorFormat);
    apngText[2].key = const_cast<char*>(fmt_str.c_str());
    // write storage color format
    png_fmt_str = ColorUtils::PNGFormatToString(PngColorFormat);
    apngText[3].key = const_cast<char*>(png_fmt_str.c_str());

    // set text
    png_set_text(pngPtr, pngInfo, apngText, sizeof(apngText) / sizeof(*apngText));

    // write info
    png_write_info(pngPtr, pngInfo);

    // allocate temporary memory for pixel colwersion
    vector<UINT08> pixel_mem(MemPixelBytes);
    vector<UINT08> pixel_copy(PngPixelBytes);

    // write image to png file
    vector<UINT08> img_row_buffer(MemWidth * PngPixelBytes);
    for (UINT32 row = 0; row < MemHeight; row++)
    {
        // initialize position to start of row
        UINT08 *pMem;
        if (YIlwerted)
            pMem = (MemHeight - 1 - row) * MemPitch + (UINT08*)MemAddress;
        else
            pMem = row * MemPitch + (UINT08*)(MemAddress);
        UINT08 *pixel = &img_row_buffer[0];
        int bit_count = 0;
        for (UINT32 col = 0; col < MemWidth; col++)
        {
            // grab pixel from memory
            Platform::MemCopy(&pixel_mem[0], pMem + MemPixelBytes*col, MemPixelBytes);

            // copy alpha channel into r,g,b channels if requested
            if (AlphaToRgb)
                ColorUtils::CopyAlphaToRgb((char*)(&pixel_mem[0]), MemColorFormat);

            // perform color colwersion
            ColorUtils::Colwert(
                (const char*)(&pixel_mem[0]),
                MemColorFormat,
                (char*)(&pixel_copy[0]),
                PngColorFormat);

            // store pixel_copy into png row buffer given by pixel
            // update pixel and bit_count to point to the next buffer position
            PNGWriteNextPixel(&pixel, &bit_count, PngPixelBits, &pixel_copy[0]);
        }
        // write the row
        png_write_row(pngPtr, (png_bytep)(&img_row_buffer[0]));
    }

    // finish the write
    png_write_end(pngPtr, pngInfo);
    PNGWriteCleanUp(pngPtr, pngInfo);
    return OK;
}
