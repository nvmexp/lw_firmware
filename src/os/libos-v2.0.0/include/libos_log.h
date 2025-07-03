/*
 * Copyright (c) 2018-2020 LWPU CORPORATION.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of LWPU CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROLWREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef LIBOS_LOGGER_H_
#define LIBOS_LOGGER_H_
#include "lwtypes.h"

/**
 *  @brief The log metadata structures and format strings are stripped
 *         These structures are emitted into the .logging section
 *         which is stripped from the image as the final build step.
 *
 */
typedef struct
{
    LW_DECLARE_ALIGNED(const char *filename, 8);
    LW_DECLARE_ALIGNED(const char *format, 8);
    LW_DECLARE_ALIGNED(LwU32 lineNumber, 8);
    LwU8 argumentCount; //! Count of arguments not including format string.
    LwU8 printLevel;
} libosLogMetadata;

/*!
 *  Count arguments
 *
 */
#define LIBOS_MACRO_GET_COUNT(...)                                                                           \
    LIBOS_MACRO_GET_19TH(__VA_ARGS__, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define LIBOS_MACRO_GET_19TH(                                                                                \
    _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, N, ...)                 \
    N

/*!
 *  Utility
 *
 */
#define LIBOS_MACRO_PASTE(fmt, b)             fmt##b
#define LIBOS_MACRO_PASTE_EVAL(fmt, b)        LIBOS_MACRO_PASTE(fmt, b)
#define LIBOS_MACRO_FIRST(format_string, ...) format_string

/*!
 *  Cast remaining log arguments to integers for storage
 *
 */
#define LIBOS_LOG_BUILD_ARG(a) (LwU64)(a),
#define LIBOS_LOG_BUILD_1(fmt)
#define LIBOS_LOG_BUILD_2(fmt, b)       LIBOS_LOG_BUILD_ARG(b)
#define LIBOS_LOG_BUILD_3(fmt, b, c)    LIBOS_LOG_BUILD_ARG(b) LIBOS_LOG_BUILD_ARG(c)
#define LIBOS_LOG_BUILD_4(fmt, b, c, d) LIBOS_LOG_BUILD_ARG(b) LIBOS_LOG_BUILD_ARG(c) LIBOS_LOG_BUILD_ARG(d)
#define LIBOS_LOG_BUILD_5(fmt, b, c, d, e)                                                                   \
    LIBOS_LOG_BUILD_ARG(b) LIBOS_LOG_BUILD_ARG(c) LIBOS_LOG_BUILD_ARG(d) LIBOS_LOG_BUILD_ARG(e)
#define LIBOS_LOG_BUILD_6(fmt, b, c, d, e, f)                                                                \
    LIBOS_LOG_BUILD_ARG(b)                                                                                   \
    LIBOS_LOG_BUILD_ARG(c) LIBOS_LOG_BUILD_ARG(d) LIBOS_LOG_BUILD_ARG(e) LIBOS_LOG_BUILD_ARG(f)
#define LIBOS_LOG_BUILD_7(fmt, b, c, d, e, f, g)                                                             \
    LIBOS_LOG_BUILD_ARG(b)                                                                                   \
    LIBOS_LOG_BUILD_ARG(c) LIBOS_LOG_BUILD_ARG(d) LIBOS_LOG_BUILD_ARG(e) LIBOS_LOG_BUILD_ARG(f)              \
        LIBOS_LOG_BUILD_ARG(g)
#define LIBOS_LOG_BUILD_8(fmt, b, c, d, e, f, g, h)                                                          \
    LIBOS_LOG_BUILD_ARG(b)                                                                                   \
    LIBOS_LOG_BUILD_ARG(c) LIBOS_LOG_BUILD_ARG(d) LIBOS_LOG_BUILD_ARG(e) LIBOS_LOG_BUILD_ARG(f)              \
        LIBOS_LOG_BUILD_ARG(g) LIBOS_LOG_BUILD_ARG(h)
#define LIBOS_LOG_BUILD_9(fmt, b, c, d, e, f, g, h, i)                                                       \
    LIBOS_LOG_BUILD_ARG(b)                                                                                   \
    LIBOS_LOG_BUILD_ARG(c) LIBOS_LOG_BUILD_ARG(d) LIBOS_LOG_BUILD_ARG(e) LIBOS_LOG_BUILD_ARG(f)              \
        LIBOS_LOG_BUILD_ARG(g) LIBOS_LOG_BUILD_ARG(h) LIBOS_LOG_BUILD_ARG(i)
#define LIBOS_LOG_BUILD_10(fmt, b, c, d, e, f, g, h, i, j)                                                   \
    LIBOS_LOG_BUILD_ARG(b)                                                                                   \
    LIBOS_LOG_BUILD_ARG(c) LIBOS_LOG_BUILD_ARG(d) LIBOS_LOG_BUILD_ARG(e) LIBOS_LOG_BUILD_ARG(f)              \
        LIBOS_LOG_BUILD_ARG(g) LIBOS_LOG_BUILD_ARG(h) LIBOS_LOG_BUILD_ARG(i) LIBOS_LOG_BUILD_ARG(j)
#define LIBOS_LOG_BUILD_11(fmt, b, c, d, e, f, g, h, i, j, k)                                                \
    LIBOS_LOG_BUILD_ARG(b)                                                                                   \
    LIBOS_LOG_BUILD_ARG(c) LIBOS_LOG_BUILD_ARG(d) LIBOS_LOG_BUILD_ARG(e) LIBOS_LOG_BUILD_ARG(f)              \
        LIBOS_LOG_BUILD_ARG(g) LIBOS_LOG_BUILD_ARG(h) LIBOS_LOG_BUILD_ARG(i) LIBOS_LOG_BUILD_ARG(j)          \
            LIBOS_LOG_BUILD_ARG(k)
#define LIBOS_LOG_BUILD_12(fmt, b, c, d, e, f, g, h, i, j, k, l)                                             \
    LIBOS_LOG_BUILD_ARG(b)                                                                                   \
    LIBOS_LOG_BUILD_ARG(c) LIBOS_LOG_BUILD_ARG(d) LIBOS_LOG_BUILD_ARG(e) LIBOS_LOG_BUILD_ARG(f)              \
        LIBOS_LOG_BUILD_ARG(g) LIBOS_LOG_BUILD_ARG(h) LIBOS_LOG_BUILD_ARG(i) LIBOS_LOG_BUILD_ARG(j)          \
            LIBOS_LOG_BUILD_ARG(k) LIBOS_LOG_BUILD_ARG(l)
#define LIBOS_LOG_BUILD_13(fmt, b, c, d, e, f, g, h, i, j, k, l, m)                                          \
    LIBOS_LOG_BUILD_ARG(b)                                                                                   \
    LIBOS_LOG_BUILD_ARG(c) LIBOS_LOG_BUILD_ARG(d) LIBOS_LOG_BUILD_ARG(e) LIBOS_LOG_BUILD_ARG(f)              \
        LIBOS_LOG_BUILD_ARG(g) LIBOS_LOG_BUILD_ARG(h) LIBOS_LOG_BUILD_ARG(i) LIBOS_LOG_BUILD_ARG(j)          \
            LIBOS_LOG_BUILD_ARG(k) LIBOS_LOG_BUILD_ARG(l) LIBOS_LOG_BUILD_ARG(m)
#define LIBOS_LOG_BUILD_14(fmt, b, c, d, e, f, g, h, i, j, k, l, m, n)                                       \
    LIBOS_LOG_BUILD_ARG(b)                                                                                   \
    LIBOS_LOG_BUILD_ARG(c) LIBOS_LOG_BUILD_ARG(d) LIBOS_LOG_BUILD_ARG(e) LIBOS_LOG_BUILD_ARG(f)              \
        LIBOS_LOG_BUILD_ARG(g) LIBOS_LOG_BUILD_ARG(h) LIBOS_LOG_BUILD_ARG(i) LIBOS_LOG_BUILD_ARG(j)          \
            LIBOS_LOG_BUILD_ARG(k) LIBOS_LOG_BUILD_ARG(l) LIBOS_LOG_BUILD_ARG(m) LIBOS_LOG_BUILD_ARG(n)
#define LIBOS_LOG_BUILD_15(fmt, b, c, d, e, f, g, h, i, j, k, l, m, n, o)                                    \
    LIBOS_LOG_BUILD_ARG(b)                                                                                   \
    LIBOS_LOG_BUILD_ARG(c) LIBOS_LOG_BUILD_ARG(d) LIBOS_LOG_BUILD_ARG(e) LIBOS_LOG_BUILD_ARG(f)              \
        LIBOS_LOG_BUILD_ARG(g) LIBOS_LOG_BUILD_ARG(h) LIBOS_LOG_BUILD_ARG(i) LIBOS_LOG_BUILD_ARG(j)          \
            LIBOS_LOG_BUILD_ARG(k) LIBOS_LOG_BUILD_ARG(l) LIBOS_LOG_BUILD_ARG(m) LIBOS_LOG_BUILD_ARG(n)      \
                LIBOS_LOG_BUILD_ARG(o)
#define LIBOS_LOG_BUILD_16(fmt, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p)                                 \
    LIBOS_LOG_BUILD_ARG(b)                                                                                   \
    LIBOS_LOG_BUILD_ARG(c) LIBOS_LOG_BUILD_ARG(d) LIBOS_LOG_BUILD_ARG(e) LIBOS_LOG_BUILD_ARG(f)              \
        LIBOS_LOG_BUILD_ARG(g) LIBOS_LOG_BUILD_ARG(h) LIBOS_LOG_BUILD_ARG(i) LIBOS_LOG_BUILD_ARG(j)          \
            LIBOS_LOG_BUILD_ARG(k) LIBOS_LOG_BUILD_ARG(l) LIBOS_LOG_BUILD_ARG(m) LIBOS_LOG_BUILD_ARG(n)      \
                LIBOS_LOG_BUILD_ARG(o) LIBOS_LOG_BUILD_ARG(p)
#define LIBOS_LOG_BUILD_17(fmt, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q)                              \
    LIBOS_LOG_BUILD_ARG(b)                                                                                   \
    LIBOS_LOG_BUILD_ARG(c) LIBOS_LOG_BUILD_ARG(d) LIBOS_LOG_BUILD_ARG(e) LIBOS_LOG_BUILD_ARG(f)              \
        LIBOS_LOG_BUILD_ARG(g) LIBOS_LOG_BUILD_ARG(h) LIBOS_LOG_BUILD_ARG(i) LIBOS_LOG_BUILD_ARG(j)          \
            LIBOS_LOG_BUILD_ARG(k) LIBOS_LOG_BUILD_ARG(l) LIBOS_LOG_BUILD_ARG(m) LIBOS_LOG_BUILD_ARG(n)      \
                LIBOS_LOG_BUILD_ARG(o) LIBOS_LOG_BUILD_ARG(p) LIBOS_LOG_BUILD_ARG(q)
#define LIBOS_LOG_BUILD_18(fmt, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r)                           \
    LIBOS_LOG_BUILD_ARG(b)                                                                                   \
    LIBOS_LOG_BUILD_ARG(c) LIBOS_LOG_BUILD_ARG(d) LIBOS_LOG_BUILD_ARG(e) LIBOS_LOG_BUILD_ARG(f)              \
        LIBOS_LOG_BUILD_ARG(g) LIBOS_LOG_BUILD_ARG(h) LIBOS_LOG_BUILD_ARG(i) LIBOS_LOG_BUILD_ARG(j)          \
            LIBOS_LOG_BUILD_ARG(k) LIBOS_LOG_BUILD_ARG(l) LIBOS_LOG_BUILD_ARG(m) LIBOS_LOG_BUILD_ARG(n)      \
                LIBOS_LOG_BUILD_ARG(o) LIBOS_LOG_BUILD_ARG(p) LIBOS_LOG_BUILD_ARG(q) LIBOS_LOG_BUILD_ARG(r)

#define LIBOS_LOG_BUILD_APPLY(F, ...) F(__VA_ARGS__)
#define APPLY_REMAINDER(...)                                                                                 \
    LIBOS_LOG_BUILD_APPLY(                                                                                   \
        LIBOS_MACRO_PASTE_EVAL(LIBOS_LOG_BUILD_, LIBOS_MACRO_GET_COUNT(__VA_ARGS__)), __VA_ARGS__)

#define LOG_LEVEL_INFO  0
#define LOG_LEVEL_ERROR 1

# define LIBOS_SECTION_LOGGING          __attribute__((section(".logging")))

#ifdef LIBOS_LOGGING_METADATA_SPLIT
/*!
 * When the METADATA_SPLIT feature is enabled, libos print data is split between 4 input sections (all of which
 * must be dropped from the final image). The default .logging is used for str literals and custom strings which
 * are directly referenced by pointer (for %s substitution); .logging_const is used for format strings and the
 * aux metadata; and .logging_metadata is used for metadata vars.
 *
 * The idea is to have a split where  metadata-only changes (i.e. changing what data gets printed, line number changes
 * in file with prints, file renames) will usually not affect the main image, and can be bundled together and
 * relied upon to trigger custom ucode build release behavior.
 * The only exception is renaming headers with prints inside static inline functions; since these don't overlap
 * with basenames, and we can't reliably get a nolwolatile version of a header file name here, such name changes
 * alone won't be able to trigger ucode releases.
 */
#    define LIBOS_SECTION_LOGGING_CONST    __attribute__((section(".logging_const")))
#    define LIBOS_SECTION_LOGGING_METADATA __attribute__((section(".logging_metadata")))
#    define LIBOS_LOGGING_AUX_METADATA_DUMP \
    static const LIBOS_SECTION_LOGGING_CONST int  libos_dummy_line[] LIBOS_ATTR_USED = {__LINE__};
#else // LIBOS_LOGGING_VOLATILE_METADATA_SPLIT
#    define LIBOS_SECTION_LOGGING_CONST  LIBOS_SECTION_LOGGING
#    define LIBOS_SECTION_LOGGING_METADATA  LIBOS_SECTION_LOGGING
#    define LIBOS_LOGGING_AUX_METADATA_DUMP
#endif // LIBOS_LOGGING_VOLATILE_METADATA_SPLIT

/*!
 *  Used for log variables which we want to dump; clients may want to pick these up to check for metadata changes
 */
#define LIBOS_ATTR_USED                __attribute__((used))

/*!
 *  Cast remaining log arguments to integers for storage
 */
#define LIBOS_LOG_INTERNAL(dispatcher, level, ...)                                                             \
    do                                                                                                         \
    {                                                                                                          \
        static const LIBOS_SECTION_LOGGING_CONST char libos_pvt_format[]   = {LIBOS_MACRO_FIRST(__VA_ARGS__)}; \
        static const LIBOS_SECTION_LOGGING_CONST char libos_pvt_file[]     = {__FILE__};                       \
        LIBOS_LOGGING_AUX_METADATA_DUMP;                                                                       \
        static const LIBOS_SECTION_LOGGING_METADATA libosLogMetadata libos_pvt_meta = {                        \
            .filename      = &libos_pvt_file[0],                                                               \
            .format        = &libos_pvt_format[0],                                                             \
            .lineNumber    = __LINE__,                                                                         \
            .argumentCount = LIBOS_MACRO_GET_COUNT(__VA_ARGS__) - 1,                                           \
            .printLevel    = level};                                                                           \
        const LwU64 tokens[] = {APPLY_REMAINDER(__VA_ARGS__)(LwU64) & libos_pvt_meta};                         \
        dispatcher(sizeof(tokens) / sizeof(*tokens), &tokens[0]);                                              \
    } while (0)

#endif
