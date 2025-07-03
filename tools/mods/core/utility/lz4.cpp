/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/lz4.h"
#include "core/include/massert.h"
#include "core/include/utility.h"   // For DEFER

#include "lz4frame.h"               // The third-party library

namespace
{
    /*
    * \brief Decompress lz4frame-compressed data.
    *        This function does the actual decompression from pCompressed to
    *        pDecompressed.
    * \note  Not part of Lz4 class to avoid including lz4frame.h in the header
    * \param alreadyConsumed  How many bytes from pCompressed have already
    *                         been read (e.g. with header)
    */
    RC Decompress
    (
        const UINT08* pCompressed,
        const size_t size,
        size_t alreadyConsumed,
        Lz4fDecompressionContext context,
        vector<UINT08>* pDecompressed
    )
    {
        MASSERT(context);
        MASSERT(pCompressed);
        MASSERT(size > 0);
        MASSERT(pDecompressed);
        MASSERT(pDecompressed->size() > 0);

        // Because the lz4 library only has a C API, we have to resort to bare
        // pointers to iterate through our containers.
        UINT08 const* src = pCompressed + alreadyConsumed;
        UINT08      * dst = pDecompressed->data();
        UINT08 const* dstEnd = dst + pDecompressed->size();
        const size_t initialOutputSize = pDecompressed->size();

        const auto inputSize = size;
        while (alreadyConsumed < inputSize)
        {
            // Resize output if not large enough
            // We choose to grow it by its initial size, which was chosen to be at
            // least large enough for one frame.
            if (dst == dstEnd)
            {
                const auto oldSize = pDecompressed->size();
                pDecompressed->resize(oldSize + initialOutputSize);
                // Reset the pointers
                dst = pDecompressed->data() + oldSize;
                dstEnd = pDecompressed->data() + pDecompressed->size();
            }

            // Input size left - will be modified to return number of bytes
            // read during decompression
            size_t leftOrConsumed = inputSize - alreadyConsumed;
            MASSERT(leftOrConsumed == static_cast<size_t>(pCompressed + inputSize - src));
            // Memory size left - will be modified to return number of bytes
            // generated during decompression
            size_t sizeOrGenerated = dstEnd - dst;

            const auto status = LZ4F_decompress(
                context,
                dst,
                &sizeOrGenerated,
                src,
                &leftOrConsumed,
                nullptr    // LZ4F_decompressOptions_t
            );

            if (LZ4F_isError(status))
            {
                Printf(Tee::PriError,
                       "Lz4 decompression error: %s\n",
                       LZ4F_getErrorName(status));
                return RC::FILE_READ_ERROR;
            }

            alreadyConsumed += leftOrConsumed;
            src += leftOrConsumed;
            dst += sizeOrGenerated;

            // The "status" returned by LZ4F_decompress() can be:
            // 0 = finished decompression
            // some error value: will be recognized by LZ4F_isError()
            // anything else = a hint to the expected size of the next block
            //                (so that it is efficient to provide at least that
            //                 many bytes as input in the next iteration)
            // So here we check if decompression is finished before consuming
            // all of our input data.
            if ((status == 0) && (alreadyConsumed < inputSize))
            {
                Printf(Tee::PriWarn,
                       "Lz4 decompression finished before end of input!"
                       "(%zu bytes read from %zu). Please investigate.\n",
                    alreadyConsumed, inputSize);
                break;
            }
        }

        if (alreadyConsumed > inputSize)
        {
            Printf(Tee::PriError,
                   "Lz4 decompression read past end of input... This should "
                   "not happen. Please investigate.\n");
            return RC::SOFTWARE_ERROR;
        }

        // Free unused storage
        pDecompressed->resize(static_cast<size_t>(dst - pDecompressed->data()));
        pDecompressed->shrink_to_fit();

        return RC::OK;
    }

    /*
    * \brief Prepare the context before starting to decompress
    * \note  Not part of Lz4 class to avoid including lz4frame.h in the header
    * \param[out] pFrameInfo  lz4-specific info block, with content size, etc
    * \param[out] pContext    lz4 context to be used for decompression
    * \param[out] pConsumed   number of chars consumed to read header
    */
    RC StartDecompress
    (
        const UINT08* pCompressed,
        const size_t size,
        LZ4F_frameInfo_t* pFrameInfo,
        Lz4fDecompressionContext* pContext,
        size_t* pConsumed
    )
    {
        MASSERT(pCompressed);
        MASSERT(size > 0);
        MASSERT(pFrameInfo);
        MASSERT(pContext);
        MASSERT(pConsumed);

        // Create a context for the decompression
        {
            const auto& status =
                LZ4F_createDecompressionContext(pContext, 100);
            if (LZ4F_isError(status))
            {
                Printf(Tee::PriError, "LZ4F_dctx creation error: %s\n",
                       LZ4F_getErrorName(status));
                return RC::CANNOT_ALLOCATE_MEMORY;
            }
        }


        // The size of the input must be at least the size of the header,
        // which is at most LZ4F_HEADER_SIZE_MAX (= 19 at version 1.8.2) bytes
        if (size < LZ4F_HEADER_SIZE_MAX)
        {
            Printf(Tee::PriWarn,
                   "Input lz4 data is smaller (%zu bytes) than the expected header"
                   " size (%d bytes). This is unusual and may lead to failure.\n",
                   size, LZ4F_HEADER_SIZE_MAX);
        }

        // Get expected size of decompressed data
        // give the data size and get back the number of chars consumed
        size_t sizeOrConsumed = size;
        const auto status = LZ4F_getFrameInfo(*pContext,
            pFrameInfo,
            reinterpret_cast<void const*>(pCompressed),
            &sizeOrConsumed);
        *pConsumed = sizeOrConsumed;

        if (LZ4F_isError(status))
        {
            Printf(Tee::PriError,
                   "Failed to get header information from lz4-compressed data."
                   " LZ4F_getFrameInfo error: %s\n", LZ4F_getErrorName(status));
            return RC::FILE_READ_ERROR;
        }

        return RC::OK;
    }

    /*
    * \brief Colwert the lz4-specific enum found in the header into a number.
    *        The block size represents how the data is split. For highest
    *        efficiency, decompression should be done in chunks of at least
    *        the block size.
    * \note  Not part of Lz4 class to avoid including lz4frame.h in the header
    */
    inline size_t GetBlockSize(const LZ4F_frameInfo_t& info)
    {
        switch (info.blockSizeID)
        {
        case LZ4F_default:
        case LZ4F_max64KB:  return 1 << 16;
        case LZ4F_max256KB: return 1 << 18;
        case LZ4F_max1MB:   return 1 << 20;
        case LZ4F_max4MB:   return 1 << 22;
        default:
            Printf(Tee::PriError,
                   "Could not extract block size from lz4 header. The "
                   "value read (%d) is unknown in versions <=v1.8.1\n",
                   info.blockSizeID);
            return 0;
        }
    }
}  // namespace

const size_t Lz4::MAX_HEADER_SIZE = LZ4F_HEADER_SIZE_MAX;

using std::vector;

/*static*/ RC Lz4::Decompress
(
    const vector<UINT08>& compressed,
    vector<UINT08>* pDecompressed
)
{
    return Decompress(compressed.data(), compressed.size(), pDecompressed, nullptr);
}

/*static*/ RC Lz4::Decompress
(
    const UINT08* pCompressed,
    const size_t size,
    vector<UINT08>* pDecompressed
)
{
    return Decompress(pCompressed, size, pDecompressed, nullptr);
}

/*static*/ RC Lz4::GetDecompressedSize
(
    const vector<UINT08>& compressed,
    size_t* pDecompressedSize
)
{
    vector<UINT08> dummy;
    return Decompress(compressed.data(), compressed.size(), &dummy, pDecompressedSize);
}

/*static*/ RC Lz4::GetDecompressedSize
(
    const UINT08* pCompressed,
    const size_t size,
    size_t* pDecompressedSize
)
{
    vector<UINT08> dummy;
    return Decompress(pCompressed, size, &dummy, pDecompressedSize);
}

/*static*/ RC Lz4::Decompress
(
    const UINT08* pCompressed,
    const size_t size,
    vector<UINT08>* pDecompressed,
    size_t* pDecompressedSize
)
{
    MASSERT(pCompressed);
    MASSERT(size > 0);
    MASSERT(pDecompressed);

    Lz4fDecompressionContext decompressContext;
    LZ4F_frameInfo_t frameInfo;
    size_t consumed = 0;
    RC rc = ::StartDecompress(pCompressed, size, &frameInfo,
                              &decompressContext, &consumed);
    DEFER() { LZ4F_freeDecompressionContext(decompressContext); };

    if (rc != RC::OK)
    {
        return rc;
    }
    if (pDecompressedSize)
    {
        *pDecompressedSize = frameInfo.contentSize;
        return RC::OK;
    }

    // Determine initial capacity of destination container.  If the full
    // content size information is present, use it. Otherwise, use the
    // block size so that we can decompress one block at a time.
    const auto dstCapacity = std::max<size_t>(GetBlockSize(frameInfo),
                                              frameInfo.contentSize);
    if (dstCapacity == 0)
    {
        return RC::FILE_READ_ERROR;
    }
    pDecompressed->resize(dstCapacity);

    CHECK_RC(::Decompress(pCompressed, size, consumed, decompressContext, pDecompressed));

    return RC::OK;
}

// -- Non-static methods for streaming

Lz4::~Lz4()
{
    if (m_Initialized)
    {
        LZ4F_freeDecompressionContext(m_Context);
    }
}

RC Lz4::StartDecompress
(
    const UINT08* pCompressed,
    const size_t size,
    vector<UINT08>* pDecompressed
)
{
    MASSERT(pCompressed);
    MASSERT(size > 0);
    MASSERT(pDecompressed);

    if (m_Initialized)
    {
        return RC::OK;
    }

    size_t consumed = 0;
    LZ4F_frameInfo_t frameInfo;
    RC rc = ::StartDecompress(pCompressed, size, &frameInfo, &m_Context, &consumed);
    if (rc != RC::OK)
    {
        LZ4F_freeDecompressionContext(m_Context);
        return rc;
    }

    // Make sure the destination container can hold at least one frame
    m_BlockSize = UNSIGNED_CAST(size_t, GetBlockSize(frameInfo));
    if (m_BlockSize == 0)
    {
        LZ4F_freeDecompressionContext(m_Context);
        return RC::FILE_READ_ERROR;
    }
    if (m_BlockSize > pDecompressed->size())
    {
        pDecompressed->resize(m_BlockSize);
    }

    m_Initialized = true;
    return ::Decompress(pCompressed,
                        size,
                        consumed,
                        m_Context,
                        pDecompressed);
}

// Overload
RC Lz4::StartDecompress
(
    const vector<UINT08>& compressed,
    vector<UINT08>* pDecompressed
)
{
    return StartDecompress(compressed.data(),
                           compressed.size(),
                           pDecompressed);
}

RC Lz4::ContinueDecompress
(
    const UINT08* pCompressed,
    const size_t size,
    vector<UINT08>* pDecompressed
)
{
    if (!m_Initialized)
    {
        Printf(Tee::PriError,
              "Lz4::StartDecompress() must be called before Lz4::ContinueDecompress()");
        return RC::SOFTWARE_ERROR;
    }

    if (m_BlockSize > pDecompressed->size())
    {
        pDecompressed->resize(m_BlockSize);
    }
    return ::Decompress(pCompressed,
                        size,
                        0,
                        m_Context,
                        pDecompressed);
}

// Overload
RC Lz4::ContinueDecompress
(
    const vector<UINT08>& compressed,
    vector<UINT08>* pDecompressed
)
{
    return ::Decompress(compressed.data(),
                        compressed.size(),
                        0,
                        m_Context,
                        pDecompressed);
}

RC Lz4::StreamDecompress
(
    const UINT08* pCompressed,
    const size_t size,
    vector<UINT08>* pDecompressed
)
{
    return m_Initialized ?
        ContinueDecompress(pCompressed, size, pDecompressed) :
        StartDecompress(pCompressed, size, pDecompressed);
}

RC Lz4::StreamDecompress
(
    const vector<UINT08>& compressed,
    vector<UINT08>* pDecompressed
)
{
    return m_Initialized ?
        ContinueDecompress(compressed, pDecompressed) :
        StartDecompress(compressed, pDecompressed);
}







