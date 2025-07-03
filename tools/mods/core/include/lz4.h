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
#pragma once

// Wrapper class for dealing with lz4-compressed data in memory.
// It has a set of static functionality for single-shot decompression (most
// efficient) but also some functions for an instance to decompress in stages.
// It lwrrently does not have any functionality to compress as there has
// not yet been a need for it

#include "core/include/rc.h"
#include "types.h"
#include <vector>

struct LZ4F_dctx_s;
typedef struct LZ4F_dctx_s LZ4F_dctx;   /* incomplete type */
typedef LZ4F_dctx* Lz4fDecompressionContext;

class Lz4
{
public:
    static const size_t MAX_HEADER_SIZE;

    // -- Functions for streaming

    /*
     * \brief Free context.
    */
    ~Lz4();

    /*
     * \brief Decompress the first chunk of lz4frame-compressed data.
     *        This must be called before ContinueDecompress() is called, and
     *        must be called only once.
     *        For a new file, a new instance must be created.
     *        Overloads for input as vector or raw pointer with size - can be
     *        used interchangeably.
    */
    RC StartDecompress
    (
        const UINT08* pCompressed,
        const size_t size,
        std::vector<UINT08>* pDecompressed
    );
    RC StartDecompress
    (
        const std::vector<UINT08>& compressed,
        std::vector<UINT08>* pDecompressed
    );

    /*
     * \brief Decompress another chunk of lz4frame-compressed data.
     *        This chunk must follow the previous one given to this object.
     *        Overloads for input as vector or raw pointer with size - can be
     *        used interchangeably.
    */
    RC ContinueDecompress
    (
        const UINT08* pCompressed,
        const size_t size,
        std::vector<UINT08>* pDecompressed
    );
    RC ContinueDecompress
    (
        const std::vector<UINT08>& compressed,
        std::vector<UINT08>* pDecompressed
    );

    /*
     * \brief Decompress lz4frame-compressed data.
     *        Overloads for input as vector or raw pointer with size - can be
     *        used interchangeably.
     *        Note: Lz4frame is the default of cli lz4 utilities, but is not
     *        the default lz4 compression from the library.
    */
    RC StreamDecompress
    (
        const UINT08* pCompressed,
        const size_t size,
        std::vector<UINT08>* pDecompressed
    );
    RC StreamDecompress
    (
        const std::vector<UINT08>& compressed,
        std::vector<UINT08>* pDecompressed
    );

    // -- Static functions for one-shot decompression

    /*
     * \brief Read lz4 header to extract the content size.
     *        Overloads for input as vector or raw pointer with size.
    */
    static RC GetDecompressedSize
    (
        const std::vector<UINT08>& compressed,
        size_t* pDecompressedSize
    );
    static RC GetDecompressedSize
    (
        const UINT08* pCompressed,
        const size_t size,
        size_t* pDecompressedSize
    );

    /*
     * \brief Decompress lz4frame-compressed data.
     *        Overloads for input as vector or raw pointer with size.
     *        Note: Lz4frame is the default of cli lz4 utilities, but is not
     *        the default lz4 compression from the library.
     */
    static RC Decompress
    (
        const std::vector<UINT08>& compressed,
        std::vector<UINT08>* pDecompressed
    );
    static RC Decompress
    (
        const UINT08* pCompressed,
        const size_t size,
        std::vector<UINT08>* pDecompressed
    );

private:

    /*
     * \brief Handle decompression context and decompress
     */
    static RC Decompress
    (
        const UINT08* pCompressed,
        const size_t size,
        std::vector<UINT08>* pDecompressed,
        size_t* pDecompressedSize
    );

    Lz4fDecompressionContext m_Context;
    size_t m_BlockSize = 0;
    bool m_Initialized = false;
};
