/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/channel.h"
#include "gpu/include/dmawrap.h"
#include "core/include/fileholder.h"
#include "core/include/cmdline.h"
#include "gpu/include/gpudev.h"
#include "gputest.h"
#include "gpu/include/gralloc.h"
#include "lwos.h"
#include "core/include/platform.h"
#include "gpu/utility/surf2d.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "core/include/lwrm.h"
#include "gpu/utility/surffill.h"
#include "class/cle26e.h" // LWE26E_CMD_SETCL_CLASSID_VIDEO_ENCODE_LWENC
#include "class/clc0b7.h" // LWC0B7_VIDEO_ENCODER
#include "class/cld0b7.h" // LWD0B7_VIDEO_ENCODER
#include "class/clc1b7.h" // LWC1B7_VIDEO_ENCODER
#include "class/clc2b7.h" // LWC2B7_VIDEO_ENCODER
#include "class/clc3b7.h" // LWC3B7_VIDEO_ENCODER
#include "class/clc4b7.h" // LWC4B7_VIDEO_ENCODER
#include "class/clc5b7.h" // LWC5B7_VIDEO_ENCODER
#include "class/clb4b7.h" // LWB4B7_VIDEO_ENCODER
#include "class/clc7b7.h" // LWC7B7_VIDEO_ENCODER
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"
#include "gpu/tests/lwencoders/h264syntax.h"
#include "gpu/tests/lwencoders/h265syntax.h"
#include "core/include/platform.h"

#include <fstream>
#include <random>
#include <boost/range/algorithm/fill.hpp>
#include <boost/range/algorithm/generate.hpp>
#include <boost/range/size.hpp>

// we need these defines just to avoid compiler errors in lwenc_drv.h
#define LW_MSENC_2_0 1
#define LW_MSENC_3_0 1
#define LW_LWENC_1_0 1
#define LW_LWENC_5_0 1
#define LW_LWENC_6_0 1
#define LW_LWENC_6_2 1
#define LW_LWENC_6_4 1
#define LW_LWENC_6_6 1
#define LW_LWENC_7_0 1
#define LW_LWENC_7_2 1

#include "lwenc_drv.h"

// +---------+-------------+---------------+------------+
// | Chip ID | HW class ID | LWENC version | Magic num  |
// +---------+-------------+---------------+------------+
// | GM107   |     C0B7    |      4.0      | 0xc0b70006 |
// | GM108   |     N/A     |      N/A      |    N/A     |
// | GM204   |     D0B7    |      5.0      | 0xd0b70006 |
// | GM200   |     D0B7    |      5.0      | 0xd0b70006 |
// | GM206   |     D0B7    |      5.0      | 0xd0b70006 |
// | GP100   |     C1B7    |      6.0      | 0xc1b70006 |
// | GP10x   |     C2B7    |      6.2      | 0xc2b70006 |
// | GV100   |     C3B7    |      6.4      | 0xc3b70006 |
// | TU10X   |     C4B7    |      7.2      | 0xc4b70006 |
// | T194    |     C5B7    |      7.2      | 0xc5b70006 |
// | TU117   |     B4B7    |      6.6      | 0xb4b70006 |
// | GA10X   |     C7B7    |      7.3      | 0xc7b70006 |
// +---------+-------------+---------------+------------+

#define LW_LWENC_DRV_MAGIC_VALUE_5_0  0xd0b70006
#define LW_LWENC_DRV_MAGIC_VALUE_1_0  0xc0b70006
#define LW_LWENC_DRV_MAGIC_VALUE_6_0  0xc1b70006
#define LW_LWENC_DRV_MAGIC_VALUE_6_2  0xc2b70006
#define LW_LWENC_DRV_MAGIC_VALUE_6_4  0xc3b70006
#define LW_LWENC_DRV_MAGIC_VALUE_6_6  0xb4b70006
#define LW_LWENC_DRV_MAGIC_VALUE_7_0  0xc5b70006
#define LW_LWENC_DRV_MAGIC_VALUE_7_2  0xc4b70006
#define LW_LWENC_DRV_MAGIC_VALUE_7_3  0xc7b70006
#define C0B7_DEFAULT_STREAM_SKIP      ((1 << H265_GENERIC  ) | \
                                       (1 << H265_10BIT_444) | \
                                       (1 << H265_4K_40F   ) | \
                                       (1 << VP9_GENERIC   ) | \
                                       (1 << VP8_GENERIC   ) | \
                                       (1 << H265_MULTIREF_BFRAME) | \
                                       (1 << OPTICAL_FLOW) | \
                                       (1 << STEREO) | \
                                       (1 << H264_B_FRAMES) | \
                                       (1 << H265_B_FRAMES_4K))
#define D0B7_DEFAULT_STREAM_SKIP      ((1 << H265_10BIT_444) | \
                                       (1 << H265_4K_40F   ) | \
                                       (1 << VP9_GENERIC   ) | \
                                       (1 << H265_MULTIREF_BFRAME) | \
                                       (1 << OPTICAL_FLOW) | \
                                       (1 << STEREO) | \
                                       (1 << H264_B_FRAMES) | \
                                       (1 << H265_B_FRAMES_4K))
#define C1B7_DEFAULT_STREAM_SKIP_DGPU ((1 << VP9_GENERIC   ) | \
                                       (1 << VP8_GENERIC   ) | \
                                       (1 << H265_MULTIREF_BFRAME) | \
                                       (1 << OPTICAL_FLOW) | \
                                       (1 << STEREO) | \
                                       (1 << H264_B_FRAMES) | \
                                       (1 << H265_B_FRAMES_4K))
#define C1B7_DEFAULT_STREAM_SKIP_SOC   (1 << H265_MULTIREF_BFRAME | \
                                       (1 << OPTICAL_FLOW) | \
                                       (1 << STEREO) | \
                                       (1 << H264_B_FRAMES) | \
                                       (1 << H265_B_FRAMES_4K))
#define C2B7_DEFAULT_STREAM_SKIP      ((1 << VP9_GENERIC   ) | \
                                       (1 << VP8_GENERIC   ) | \
                                       (1 << H265_MULTIREF_BFRAME) | \
                                       (1 << OPTICAL_FLOW) | \
                                       (1 << STEREO) | \
                                       (1 << H264_B_FRAMES) | \
                                       (1 << H265_B_FRAMES_4K))
#define C3B7_DEFAULT_STREAM_SKIP      ((1 << VP9_GENERIC   ) | \
                                       (1 << VP8_GENERIC   ) | \
                                       (1 << H265_MULTIREF_BFRAME) | \
                                       (1 << OPTICAL_FLOW) | \
                                       (1 << STEREO) | \
                                       (1 << H264_B_FRAMES) | \
                                       (1 << H265_B_FRAMES_4K))
#define C4B7_DEFAULT_STREAM_SKIP      ((1 << VP9_GENERIC   ) | \
                                       (1 << VP8_GENERIC   ) | \
                                       (1 << H265_4K_40F))
#define B4B7_DEFAULT_STREAM_SKIP      ((1 << VP9_GENERIC   ) | \
                                       (1 << VP8_GENERIC   ) | \
                                       (1 << H265_MULTIREF_BFRAME) | \
                                       (1 << OPTICAL_FLOW) | \
                                       (1 << STEREO) | \
                                       (1 << H264_B_FRAMES) | \
                                       (1 << H265_B_FRAMES_4K))
#define C7B7_DEFAULT_STREAM_SKIP      ((1 << VP9_GENERIC   ) | \
                                       (1 << VP8_GENERIC   ) | \
                                       (1 << H265_MULTIREF_BFRAME) | \
                                       (1 << OPTICAL_FLOW) | \
                                       (1 << STEREO) | \
                                       (1 << H264_B_FRAMES) | \
                                       (1 << H265_B_FRAMES_4K))

//TODO: Should work on the other streams
#define C5B7_DEFAULT_STREAM_SKIP      ((1 << H265_4K_40F   ) | \
                                       (1 << VP8_GENERIC   ) | \
                                       (1 << H265_B_FRAMES_4K))

using Utility::AlignUp;
using Utility::AlignDown;
using Utility::StrPrintf;
using Utility::CountBits;

// GCC 4.9.2 does not declare std::max and std::min as constexpr in violation
// of C++14 standard, even if compiled with -std=c++14
template <typename T>
constexpr
inline const T&
ConstExprMax(const T& a, const T& b)
{
    return (a < b) ? b : a;
}

static constexpr UINT32 MAX_NUM_FRAMES = 60;
static constexpr UINT32 MAX_REF_FRAMES = 6;
static constexpr UINT32 MAX_WIDTH      = 3840;
static constexpr UINT32 MAX_HEIGHT     = 2176;
static constexpr UINT32 MAX_REQ_SIZE   = 192; // From LWENC 6.0 IAS section 2.7

#define ENCODE_WIDTH(width)   AlignUp<64>(width)
#define ENCODE_HEIGHT(height) AlignUp<64>(height)
#define MB_WIDTH(width)       (ENCODE_WIDTH(width) / 16)
#define MB_HEIGHT(height)     (ENCODE_HEIGHT(height) / 16)
#define COUNTER_BUF_SIZE      AlignUp<256>(3316U * 4U)
#define PROB_BUF_SIZE         AlignUp<256>(234U * 16U)
#define CEA_HINTS_BUF_SIZE    AlignUp<64>(3136U)
#define COLOC_BUF_SIZE(width, height) \
    AlignUp<256>(MB_WIDTH(width) * MB_HEIGHT(height) * 64)
#define COMBINED_LINE_BUF_SIZE(width, height) \
    AlignUp<256>(MB_WIDTH(width) * MB_HEIGHT(height) * 20)
#define FILTER_LINE_BUF_SIZE(width, height) \
    AlignUp<256>(MB_WIDTH(width) * MB_HEIGHT(height) * 64)
#define FILTER_COL_LINE_BUF_SIZE(width, height) \
    AlignUp<256>(MB_WIDTH(width) * MB_HEIGHT(height) * 64)
#define HIST_BUF_SIZE(width, height) \
    AlignUp<256>(MB_WIDTH(width) * (MB_HEIGHT(height) + 1) * MAX_REQ_SIZE)
#define PARTITION_BUF_SIZE(width, height) \
    AlignUp<256>(MB_WIDTH(width) * MB_HEIGHT(height) * 336)
#define TEMPORAL_BUF_SIZE(width, height) \
    AlignUp<256>(MB_WIDTH(width) * (MB_HEIGHT(height) + 1) * 16)

#define UNUSED -2  // unsupported methods for some codecs

struct StreamDescription
{
    const char *name;
    UINT32 numFrames;
    INT32 outPictureIndex[MAX_NUM_FRAMES]; // index to m_YUVRefPic array
};

enum StreamIndexes
{
    H264_BASELINE_CAVLC  = 0,
    H264_HP_CABAC        = 1,
    H264_MVC_HP_CABAC    = 2,
    H264_HD_HP_CABAC_MS  = 3,
    H264_B_FRAMES        = 4,
    H265_GENERIC         = 5,
    H265_10BIT_444       = 6,
    VP9_GENERIC          = 7,
    VP8_GENERIC          = 8,
    H265_4K_40F          = 9,
    H265_MULTIREF_BFRAME = 10,
    OPTICAL_FLOW         = 11,
    STEREO               = 12,
    H265_B_FRAMES_4K     = 13,
    NUM_STREAMS
};

JS_CLASS(LwencStrm);
static SObject LwencStrm_Object
(
    "LwencStrm",
    LwencStrmClass,
    0,
    0,
    "LWENC test stream constants"
);
PROP_CONST(LwencStrm, H264_BASELINE_CAVLC,  H264_BASELINE_CAVLC);
PROP_CONST(LwencStrm, H264_HP_CABAC,        H264_HP_CABAC);
PROP_CONST(LwencStrm, H264_MVC_HP_CABAC,    H264_MVC_HP_CABAC);
PROP_CONST(LwencStrm, H264_HD_HP_CABAC_MS,  H264_HD_HP_CABAC_MS);
PROP_CONST(LwencStrm, H264_B_FRAMES,        H264_B_FRAMES);
PROP_CONST(LwencStrm, H265_GENERIC,         H265_GENERIC);
PROP_CONST(LwencStrm, H265_10BIT_444,       H265_10BIT_444);
PROP_CONST(LwencStrm, VP9_GENERIC,          VP9_GENERIC);
PROP_CONST(LwencStrm, VP8_GENERIC,          VP8_GENERIC);
PROP_CONST(LwencStrm, H265_4K_40F,          H265_4K_40F);
PROP_CONST(LwencStrm, H265_MULTIREF_BFRAME, H265_MULTIREF_BFRAME);
PROP_CONST(LwencStrm, OPTICAL_FLOW,         OPTICAL_FLOW);
PROP_CONST(LwencStrm, STEREO,               STEREO);
PROP_CONST(LwencStrm, H265_B_FRAMES_4K,     H265_B_FRAMES_4K);
PROP_CONST(LwencStrm, NUM_STREAMS,          NUM_STREAMS);


//  Streams (short movies) picked up by the video team to exercise
//  various compression schemes and provide sufficient engine coverage.
static const StreamDescription streamDescriptions[NUM_STREAMS] =
{
    // Name                                      NumF  OutPicIdx
    { "H264 Baseline CAVLC",                        4, {0, 1, 0, 1} },
    { "H264 High profile CABAC",                    8, {0, 0, 1, 1, 2, 2, 2, 2} },
    { "H264 MVC High profile CABAC",                8, {0, 1, 2, 3, 4, 4, 4, 4} },
    { "H264 HD High profile CABAC multiple slices", 3, {0, 1, 2} },
    { "H264 B frames",                              7, {0, 1, 2, 3, 4, 5, 0} },
    { "H265 Generic",                               5, {0, 1, 0, 1, 0} },
    { "H265 10 Bit 4:4:4",                          5, {0, 1, 0, 1, 0} },
    { "VP9 Generic",                                5, {0, 1, 0, 1, 0} },
    { "VP8 Generic",                                9, {0, 1, 0, 1, 0, 1, 0, 1, 0} },
    { "H265 4K 40 frames",                          40, {0, 1, 0, 1, 0, 1, 0, 1, 0,
                                                         1, 0, 1, 0, 1, 0, 1, 0, 1,
                                                         0, 1, 0, 1, 0, 1, 0, 1, 0,
                                                         1, 0, 1, 0, 1, 0, 1, 0, 1,
                                                         0, 1, 0, 1, 0, 1, 0, 1, 0,
                                                         1, 0, 1, 0, 1, 0, 1, 0, 1,
                                                         0, 1, 0, 1, 0, 1} },
    { "H265 MultiRef Bframe",                       7, {0, 1, 2, 3, 4, 3, 3} }, //Frame 5 and 6, uses frames 0, 1 and 4. Hence, OutPicIdx[5] could be 2 or 3
    { "Optical flow",                               3, {UNUSED, UNUSED, UNUSED} },
    { "Stereo",                                     2, {UNUSED, UNUSED} },
    { "H.265 4K B frames",                         50, {0, 1, 2, 3, 4, 2, 3, 2, 3, 3, 3, 0, 0, 0, 1,
                                                        1, 1, 4, 4, 4, 2, 2, 2, 3, 3, 3, 0, 0, 0, 1,
                                                        1, 1, 4, 4, 4, 2, 2, 2, 3, 3, 3, 0, 0, 0, 1,
                                                        1, 1, 4, 4, 4} },
};

static const UINT32 s_ApplicationId[NUM_STREAMS] =
{
    LWC1B7_SET_APPLICATION_ID_ID_LWENC_H264,    // H264_BASELINE_CAVLC
    LWC1B7_SET_APPLICATION_ID_ID_LWENC_H264,    // H264_HP_CABAC
    LWC1B7_SET_APPLICATION_ID_ID_LWENC_H264,    // H264_MVC_HP_CABAC
    LWC1B7_SET_APPLICATION_ID_ID_LWENC_H264,    // H264_HD_HP_CABAC_MS
    LWC1B7_SET_APPLICATION_ID_ID_LWENC_H264,    // H264_B_FRAMES
    LWC1B7_SET_APPLICATION_ID_ID_LWENC_H265,    // H265_GENERIC
    LWC1B7_SET_APPLICATION_ID_ID_LWENC_H265,    // H265_10BIT_444
    LWC1B7_SET_APPLICATION_ID_ID_LWENC_VP9,     // VP9_GENERIC
    LWC1B7_SET_APPLICATION_ID_ID_LWENC_VP8,     // VP8_GENERIC
    LWC1B7_SET_APPLICATION_ID_ID_LWENC_H265,    // H265_4K_40F
    LWC1B7_SET_APPLICATION_ID_ID_LWENC_H265,    // H265_MULTIREF_B
    LWC5B7_SET_APPLICATION_ID_ID_LWENC_OFS,     // OPTICAL_FLOW
    LWC5B7_SET_APPLICATION_ID_ID_LWENC_OFS,     // STEREO
    LWC4B7_SET_APPLICATION_ID_ID_LWENC_H265     // H265_B_FRAMES_4K
};  


// Indexes to reference picture surfaces used during current frame:
//    * "-1" means reference picture not used,
//    * value >= 0 is an index to m_YUVRefPic array
//
// For each frame there is a list or two of reference frames that are going to
// be used to compress the current frame. The engine knows what surfaces are in
// these lists via a two step process. First, there are two lists of numbers
// in the control structures. For example, for H.264 they are
// `lwenc_h264_pic_control_s::l0` and `lwenc_h264_pic_control_s::l1`. Then, when
// the engine takes a number from one of these lists, it determines the address
// of the surface from a push buffer command SET_IN_REF_PIC*_{LUMA|CHROMA}.
// This array defines where SET_IN_REF_PIC*_{LUMA|CHROMA} will set the address:
// to one of the surfaces in m_YUVRefPic.
// In pseudo-code, inside the engine:
//     N = lwenc_h265_pic_control_s::l0[i]
//     use address set by SET_IN_REF_PIC[N] for reference picture i
// `refPictureIndexes` is an additional re-mapping. In pseudo-code, inside the test:
//     SET_IN_REF_PIC[N] is set to the address of m_YUVRefPic[refPictureIndexes[N]].
// Therefore the engine will use for reference picture i this surface:
//     m_YUVRefPic[refPictureIndexes[lwenc_h265_pic_control_s::l0[i]]]
static const INT32 refPictureIndexes[NUM_STREAMS][MAX_NUM_FRAMES][MAX_REF_FRAMES] =
{
    // H264_BASELINE_CAVLC:
    {
        { -1, -1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        { -1, -1,  0, -1, -1, -1}
    },
    // H264_HP_CABAC:
    {
        { -1, -1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        {  0,  1, -1, -1, -1, -1},
        {  0,  1, -1, -1, -1, -1},
        {  0,  1, -1, -1, -1, -1},
        {  0,  1, -1, -1, -1, -1},
        {  0,  1, -1, -1, -1, -1}
    },
    // H264_MVC_HP_CABAC:
    {
        { -1, -1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        {  2,  1,  2, -1, -1, -1}, // Using "2" twice is not a typo, but normal
                                   //   according to the video team
        {  0, -1,  2, -1, -1, -1},
        { -1,  1, -1,  3, -1, -1},
        {  0, -1,  2, -1, -1, -1},
        { -1,  1, -1,  3, -1, -1}
    },
    // H264_HD_HP_CABAC_MS:
    {
        { -1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1},
        {  0,  1, -1, -1, -1}
    },
    // H264_B_FRAMES
    {                             // l0,       l1,       DPB and       out ref picture index:
    //    0   1   2   3   4   5                           0 1 2 3 4 5 <- indices to m_RefPic
        {-1, -1, -1, -1, -1, -1}, // [       ] [       ] [           ] 0
        { 0, -1, -1, -1, -1, -1}, // [0      ] [       ] [0          ] 1
        { 0,  1, -1, -1, -1, -1}, // [0 1    ] [1      ] [0 1        ] 2
        { 0,  1,  2, -1, -1, -1}, // [2 0 1  ] [1 2 0  ] [0 1 2      ] 3
        { 0,  1,  2,  3, -1, -1}, // [1 3 2 0] [       ] [0 1 2 3    ] 4
        { 0,  1,  2,  3,  4, -1}, // [1 3 2 0] [4 1 3 2] [0 1 2 3 4  ] 5
        {-1,  1,  2,  3,  4,  5}  // [5 1 3 2] [4 5 1 3] [0 1 2 3 4 5] 0 zero is not used as a
                                  //                                     reference, we can place the
                                  //                                     reconstructed picture to
                                  //                                     m_RefPic[0]
    },
    //H265_GENERIC:
    {
        { -1, -1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        {  1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        {  1,  1, -1, -1, -1, -1}
    },
    //H265_10BIT_444:
    {
        { -1, -1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        {  1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        {  1,  1, -1, -1, -1, -1}
    },
    // VP9_GENERIC:
    {
        { -1, -1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1}
    },
    // VP8_GENERIC:
    {
        { -1, -1, -1, -1, -1, -1},
        {  0,  1,  2, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        {  0,  1,  2, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        {  0,  1,  2, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        {  0,  1,  2, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1}
    },
    // H265_4K_40F:
    {
        { -1, -1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1, -1}
    },
    //H265_MultiRefBFrame
    {
        {-1, -1, -1, -1, -1, -1},
        { 0, -1, -1, -1, -1, -1},
        { 0,  1, -1, -1, -1, -1},
        { 0,  1, -1, -1, -1, -1},
        { 0,  1, -1, -1, -1, -1},
        { 0,  1,  4, -1, -1, -1},
        { 0,  1,  4, -1, -1, -1}
    },
    //Optical_Flow
    {
        {-1, -1, -1, -1, -1, -1},
        { 0, -1, -1, -1, -1, -1},
        { 0, -1, -1, -1, -1, -1}
    },
    //Stereo
    {
        {-1, -1, -1, -1, -1, -1},
        { 0, -1, -1, -1, -1, -1}
    },
    // H265_B_FRAMES_4K
    {   // use just sequential numbers, lwenc_h265_pic_control_s::l0 and l1 are already set to the
        // correct values
        {-1, -1, -1, -1, -1, -1}, { 0, -1, -1, -1, -1, -1}, { 0, 1, -1, -1, -1, -1},
        { 0,  1,  2, -1, -1, -1}, { 0,  1,  2,  3, -1, -1}, { 0, 1,  2,  3,  4, -1},
        { 0,  1,  2,  3,  4, -1}, { 0,  1,  2,  3,  4, -1}, { 0, 1,  2,  3,  4, -1},
        { 0,  1,  2,  3,  4, -1}, { 0,  1,  2,  3,  4, -1}, { 0, 1,  2,  3,  4, -1},
        { 0,  1,  2,  3,  4, -1}, { 0,  1,  2,  3,  4, -1}, { 0, 1,  2,  3,  4, -1},
        { 0,  1,  2,  3,  4, -1}, { 0,  1,  2,  3,  4, -1}, { 0, 1,  2,  3,  4, -1},
        { 0,  1,  2,  3,  4, -1}, { 0,  1,  2,  3,  4, -1}, { 0, 1,  2,  3,  4, -1},
        { 0,  1,  2,  3,  4, -1}, { 0,  1,  2,  3,  4, -1}, { 0, 1,  2,  3,  4, -1},
        { 0,  1,  2,  3,  4, -1}, { 0,  1,  2,  3,  4, -1}, { 0, 1,  2,  3,  4, -1},
        { 0,  1,  2,  3,  4, -1}, { 0,  1,  2,  3,  4, -1}, { 0, 1,  2,  3,  4, -1},
        { 0,  1,  2,  3,  4, -1}, { 0,  1,  2,  3,  4, -1}, { 0, 1,  2,  3,  4, -1},
        { 0,  1,  2,  3,  4, -1}, { 0,  1,  2,  3,  4, -1}, { 0, 1,  2,  3,  4, -1},
        { 0,  1,  2,  3,  4, -1}, { 0,  1,  2,  3,  4, -1}, { 0, 1,  2,  3,  4, -1},
        { 0,  1,  2,  3,  4, -1}, { 0,  1,  2,  3,  4, -1}, { 0, 1,  2,  3,  4, -1},
        { 0,  1,  2,  3,  4, -1}, { 0,  1,  2,  3,  4, -1}, { 0, 1,  2,  3,  4, -1},
        { 0,  1,  2,  3,  4, -1}, { 0,  1,  2,  3,  4, -1}, { 0, 1,  2,  3,  4, -1},
        { 0,  1,  2,  3,  4, -1}, { 0,  1,  2,  3,  4, -1}
    }
};

/// not used for VP8
// Indexes of reference picture surfaces sent to SET_IN_MO_COMP_PIC method
static const INT32 s_MoCompIndexes[NUM_STREAMS][MAX_NUM_FRAMES] =
{
    {-1, -1, -1, -1},                 // H264_BASELINE_CAVLC
    {-1, -1, -1, -1, -1, -1, -1, -1}, // H264_HP_CABAC
    {-1, -1, -1, -1, -1, -1, -1, -1}, // H264_MVC_HP_CABAC
    {-1, -1, -1},                     // H264_HD_HP_CABAC_MS
    {-1, -1, -1, -1, -1, -1, -1},     // H264_B_FRAMES
    {-1, -1, -1, -1, -1},             // H265_GENERIC
    {-1, -1, -1, -1, -1},             // H265_10BIT_444
    { 2,  2,  2,  2,  2},             // VP9_GENERIC
    {-1, -1, -1, -1, -1, -1, -1, -1, -1},   // VP8_GENERIC (unsupported)
    {-1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1},            // H265_4K_40F
    {-1, -1, -1, -1, -1, -1, -1},        // H265_MULTIREF_BFRAME
    {-1, -1, -1},                        // OPTICAL_FLOW
    {-1, -1},                            // STEREO
    {-1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1},                // H265_B_FRAMES_4K
};

/// not used for VP8
// Indexes to coloc surfaces used during current frame:
// { input, output}
//  "-1" means surface not used,
//  value >= 0 is an index to m_ColocData array
static const INT32 colocInOutIndexes[NUM_STREAMS][MAX_NUM_FRAMES][2] =
{
    { { -1,  0}, {  0,  1}, {  1,  0}, {  0,  1} }, // H264_BASELINE_CAVLC
    { { -1,  0}, {  0,  1}, {  0,  0}, {  1,  1},
      {  0, -1}, {  1, -1}, {  0, -1}, {  1, -1} }, // H264_HP_CABAC
    { { -1,  0}, {  0,  1}, {  0,  0}, {  1,  1},
      {  0, -1}, {  1, -1}, {  0, -1}, {  1, -1} }, // H264_MVC_HP_CABAC
    { { -1,  0}, {  0,  1}, {  1, -1}, {  1, -1} }, // H264_HD_HP_CABAC_MS
    { {UNUSED, 0},  { 0, -1},  { 0, -1},  {0, -1},  // H264_B_FRAMES
      { 0, -1},  { 0, -1},  { 0, -1} },
    { { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1},
      { -1, -1} },                                  // H265_GENERIC
    { { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1},
      { -1, -1} },                                  // H265_10BIT_444
    { { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1},
      { -1, -1} },                                  // VP9_GENERIC
    {
      {UNUSED, UNUSED}, {UNUSED, UNUSED},
      {UNUSED, UNUSED}, {UNUSED, UNUSED},
      {UNUSED, UNUSED}, {UNUSED, UNUSED},
      {UNUSED, UNUSED}, {UNUSED, UNUSED},
      {UNUSED, UNUSED}                              // VP8_GENERIC unsupported
    },
    { { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1},
      { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1},
      { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1},
      { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1},
      { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1},
      { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1},
      { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1},
      { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1},
      { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1},
      { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1}, { -1, -1},
      { -1, -1}, { -1, -1} },                       // H265_4K_40F
      { {-1, -1}, { 0, -1}, { 0, -1}, {0, -1},      // H265_Multiref_BFrame
        { 0, -1}, { 0, -1}, { 0, -1} },
      { {-1, -1}, { 0,  1}, { 1,  0} },             // OPTICAL_FLOW
      { {-1, -1}, { 0,  1} },                       // STEREO
      { {  0, -1}, {  0, -1}, {  0, -1}, {  0, -1}, {  0, -1}, {  0, -1},
      {  0, -1}, {  0, -1}, {  0, -1}, {  0, -1}, {  0, -1}, {  0, -1},
      {  0, -1}, {  0, -1}, {  0, -1}, {  0, -1}, {  0, -1}, {  0, -1},
      {  0, -1}, {  0, -1}, {  0, -1}, {  0, -1}, {  0, -1}, {  0, -1},
      {  0, -1}, {  0, -1}, {  0, -1}, {  0, -1}, {  0, -1}, {  0, -1},
      {  0, -1}, {  0, -1}, {  0, -1}, {  0, -1}, {  0, -1}, {  0, -1},
      {  0, -1}, {  0, -1}, {  0, -1}, {  0, -1}, {  0, -1}, {  0, -1},
      {  0, -1}, {  0, -1}, {  0, -1}, {  0, -1}, {  0, -1}, {  0, -1},
      {  0, -1}, {  0, -1} },                                          // H265_B_FRAMES_4K
};

// Indexes to me surfaces used during current frame:
// { input, output}
//  "-1" means surface not used,
//  value >= 0 is an index to m_MepredData array
static const INT32 meInOutIndexes[NUM_STREAMS][MAX_NUM_FRAMES][2] =
{
    { { -1,  0}, {  0,  1}, {  1,  0}, {  0,  1} }, // H264_BASELINE_CAVLC
    { { -1,  0}, {  0,  1}, {  1,  0}, {  0,  1},
      {  1,  0}, {  0,  1}, {  1,  0}, {  0,  1} }, // H264_HP_CABAC
    { { -1, -1}, {  0,  0}, {  0,  1}, {  0,  2},
      {  1,  0}, {  2,  1}, {  0,  2}, {  1,  0} }, // H264_MVC_HP_CABAC
    { { -1,  0}, {  0,  1}, {  1,  0}, {  0,  1} }, // H264_HD_HP_CABAC_MS
    { { UNUSED, UNUSED}, {  0,  1}, {  1,  0}, {  0,  1},   // H264_B_FRAMES
      {  1, 0},  {  0, 1},  {  1, 0} },
    { { -1,  0}, {  0,  1}, {  1,  0}, {  0,  1},
      { 1 , 0} },                                   // H265_GENERIC
    { { -1,  0}, {  0,  1}, {  1,  0}, {  0,  1},
      { 1 , 0} },                                   // H265_10BIT_444
    { { -1,  0}, {  0,  1}, {  1,  0}, {  0,  1},
      { 1 , 0} },                                   // VP9_GENERIC
    { { 0,  1 }, {  1,  0}, {  0,  1}, {  1,  0},
      { 0,  1},  {  1,  0}, {  0,  1}, {  1,  0},
      { 0,  1} },                                   // VP8_GENERIC
    { { -1,  0}, {  0,  1}, {  1,  0}, {  0,  1},
      { 1 , 0}, {  0,  1}, {  1,  0}, {  0,  1}, {  1,  0}, {  0,  1},
      { 1 , 0}, {  0,  1}, {  1,  0}, {  0,  1}, {  1,  0}, {  0,  1},
      { 1 , 0}, {  0,  1}, {  1,  0}, {  0,  1}, {  1,  0}, {  0,  1},
      { 1 , 0}, {  0,  1}, {  1,  0}, {  0,  1}, {  1,  0}, {  0,  1},
      { 1 , 0}, {  0,  1}, {  1,  0}, {  0,  1},
      { 1 , 0}, {  0,  1}, {  1,  0}, {  0,  1}, {  1,  0}, {  0,  1},
      { 1 , 0}, {  0,  1}, {  1,  0}, {  0,  1}, {  1,  0}, {  0,  1},
      { 1 , 0}, {  0,  1}, {  1,  0}, {  0,  1}, {  1,  0}, {  0,  1},
      { 1 , 0}, {  0,  1}, {  1,  0}, {  0,  1}, {  1,  0}, {  0,  1},
      { 1 , 0}, {  0,  1}, { 1 , 0}, {  0,  1} },   // H265_4K_40F
    { { -1,  -1}, {  0,  1}, {  1,  0}, {  0,  1},  // H265_Multiref_Bframe
      { 1 , 0},  { 0 , 1}, { 1 , 0} },
    { {-1, -1}, { 0,  1}, { 1, 0} },                // OPTICAL_FLOW
    { {-1, -1}, { 0,  1} },                         // STEREO
    { { -1 , -1}, {  0,  1}, {  1,  0}, {  0,  1}, {  1,  0}, {  0,  1},
      { 1 , 0}, {  0,  1}, {  1,  0}, {  0,  1}, {  1,  0}, {  0,  1},
      { 1 , 0}, {  0,  1}, {  1,  0}, {  0,  1}, {  1,  0}, {  0,  1},
      { 1 , 0}, {  0,  1}, {  1,  0}, {  0,  1}, {  1,  0}, {  0,  1},
      { 1 , 0}, {  0,  1}, {  1,  0}, {  0,  1}, {  1,  0}, {  0,  1},
      { 1 , 0}, {  0,  1}, {  1,  0}, {  0,  1}, {  1,  0}, {  0,  1},
      { 1 , 0}, {  0,  1}, {  1,  0}, {  0,  1}, {  1,  0}, {  0,  1},
      { 1 , 0}, {  0,  1}, {  1,  0}, {  0,  1}, {  1,  0}, {  0,  1},
      { 1 , 0}, {  0,  1} },                         // H265_B_FRAMES_4K
};

/// unused for VP8
// Indexes of surfaces sent to LWC1B7_SET_IN_LWRRENT_TEMPORAL_BUF
// and LWC1B7_SET_IN_REF_TEMPORAL_BUF methods
static const INT32 s_TemporalLwrRefIndexes[NUM_STREAMS][MAX_NUM_FRAMES][2] =
{
    {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},           // H264_BASELINE_CAVLC
    {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},           // H264_HP_CABAC
    {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},           // H264_MVC_HP_CABAC
    {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}},           // H264_HD_HP_CABAC_MS
    {{UNUSED, UNUSED}, {-1, -1}, {-1, -1}, {-1, -1},    // H264_B_FRAMES
     {-1, -1}, {-1, -1}, {-1, -1}},
    {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}}, // H265_GENERIC
    {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}}, // H265_10BIT_444
    {{ 0,  1}, { 1,  0}, { 0,  1}, { 1,  0}, { 0,  1}}, // VP9_GENERIC
    {
     {UNUSED, UNUSED}, {UNUSED, UNUSED},
     {UNUSED, UNUSED}, {UNUSED, UNUSED},
     {UNUSED, UNUSED}, {UNUSED, UNUSED},
     {UNUSED, UNUSED}, {UNUSED, UNUSED},
     {UNUSED, UNUSED}                                   // VP8_GENERIC unsupported
    },
    {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1} },          // H265_4K_40F
     {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},           // H265_MULTIREF_BFRAME
      {-1, -1}, {-1, -1}, {-1, -1}},
     {{-1, -1}, { -1, -1}, { -1, -1}},                  // OPTICAL_FLOW
     {{-1, -1}, { -1,  -1}},                            // STEREO
    {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
     {-1, -1}, {-1, -1}},                              // H265_B_FRAMES_4K
};

// outBitstreamBufferSize in bits, aligned to 256 bytes
static const UINT32 outBitstreamBufferSize[NUM_STREAMS] =
{
    AlignUp<256 * 8>(7088u),     // H264_BASELINE_CAVLC
    AlignUp<256 * 8>(3960u),     // H264_HP_CABAC
    AlignUp<256 * 8>(6640u),     // H264_MVC_HP_CABAC
    AlignUp<256 * 8>(66608u),    // H264_HD_HP_CABAC_MS
    AlignUp<256 * 8>(5480u),     // H264_B_FRAMES
    AlignUp<256 * 8>(27928u),    // H265_GENERIC
    AlignUp<256 * 8>(56304u),    // H265_10BIT_444
    AlignUp<256 * 8>(52696u),    // VP9_GENERIC
    AlignUp<256 * 8>(4448u),     // VP8_GENERIC
    AlignUp<256 * 8>(0x80000u),  // H265_4K_40F
    AlignUp<256 * 8>(6176u),     // H265_MULTIREF_BFRAME
    AlignUp<256 * 8>(77824u),    // OPTICAL_FLOW
    AlignUp<256 * 8>(45000u),    // STEREO
    AlignUp<256 * 8>(50922520u)  // H265_B_FRAMES_4K
};

constexpr size_t maxNumForcedSlices = 10;

constexpr UINT32 sliceControlOffset = AlignUp<256>(static_cast<UINT32>(ConstExprMax
(
    ConstExprMax
    (
        sizeof(lwenc_h264_drv_pic_setup_s),
        sizeof(lwenc_h265_drv_pic_setup_s)
    ),
    sizeof(lwenc_vp9_drv_pic_setup_s)
)));

constexpr UINT32 mdControlOffset = AlignUp<256>(static_cast<UINT32>
(
    sliceControlOffset +
    maxNumForcedSlices * ConstExprMax
    (
        sizeof(lwenc_h264_slice_control_s),
        sizeof(lwenc_h265_slice_control_s)
    )
));

constexpr UINT32 quantControlOffset = AlignUp<256>(static_cast<UINT32>
(
    mdControlOffset +
    ConstExprMax
    (
        sizeof(lwenc_h264_md_control_s),
        sizeof(lwenc_h265_md_control_s)
    )
));

constexpr UINT32 meControlOffset = AlignUp<256>(static_cast<UINT32>
(
    quantControlOffset +
    ConstExprMax
    (
        sizeof(lwenc_h264_quant_control_s),
        sizeof(lwenc_h265_quant_control_s)
    )
));

constexpr UINT32 h264H265VP9controlStructsSize = AlignUp<256>(static_cast<UINT32>
(
    meControlOffset + sizeof(lwenc_h264_me_control_s)
));

static const UINT32 allControlStructsSizes[NUM_STREAMS] =
{
    h264H265VP9controlStructsSize,      // H264_BASELINE_CAVLC
    h264H265VP9controlStructsSize,      // H264_HP_CABAC
    h264H265VP9controlStructsSize,      // H264_MVC_HP_CABAC
    h264H265VP9controlStructsSize,      // H264_HD_HP_CABAC_MS
    h264H265VP9controlStructsSize,      // H264_B_FRAMES
    h264H265VP9controlStructsSize,      // H265_GENERIC
    h264H265VP9controlStructsSize,      // H265_10BIT_444
    h264H265VP9controlStructsSize,      // VP9_GENERIC
    sizeof(lwenc_vp8_drv_pic_setup_s),  // VP8_GENERIC
    54784,                              // H265_4K_40F
    h264H265VP9controlStructsSize,      // H265_MULTIREF_BFRAME
    h264H265VP9controlStructsSize,      // OPTICAL_FLOW
    h264H265VP9controlStructsSize,      // STEREO
    h264H265VP9controlStructsSize,      // H265_B_FRAMES_4K
};

const UINT32 maxControlStructsSize = 54784;

H264::NALU spsNalus[NUM_STREAMS];
H264::NALU ppsNalus[NUM_STREAMS];
// for MVC stream
H264::NALU subsetSpsNalu;
H264::NALU subsetPpsNalu;

H265::NALU h265VpsNalus[NUM_STREAMS];
H265::NALU h265SpsNalus[NUM_STREAMS];
H265::NALU h265PpsNalus[NUM_STREAMS];

//! A helper class to serialize PPS
class SeqSetSrcImpl : public H264::SeqParamSetSrc
{
public:
    SeqSetSrcImpl(const H264::SeqParameterSet &_sps)
        : sps(_sps)
    {}

private:
    const H264::SeqParameterSet& DoGetSPS(size_t id) const
    {
        return sps;
    }

    const H264::SeqParameterSet &sps;
};

static void SaveSurface
(
    Surface2D *surface,
    const char *surfaceName,
    UINT32 streamIndex,
    UINT32 frameIdx,
    UINT32 engNum
)
{
    if (surface->GetSize() == 0)
        return;

    char fileName[32];
    UINT08 buffer[4096];
    sprintf(fileName, "S%dF%dE%d%s.bin", streamIndex, frameIdx, engNum, surfaceName);
    FileHolder file(fileName, "wb");
    if (file.GetFile() == 0)
    {
        Printf(Tee::PriError, "Error: Unable to open file %s for write!\n",
            fileName);
        return;
    }
    Surface2D::MappingSaver oldMapping(*surface);
    if (!surface->IsMapped() && OK != surface->Map())
    {
        Printf(Tee::PriError,
            "Error: Unable to dump SurfaceName surface - unable to map the surface!\n");
        return;
    }
    UINT32 size = (UINT32)surface->GetSize();
    for (UINT32 offset = 0; offset < size; offset += sizeof(buffer))
    {
        UINT32 transferSize = (size - offset) < sizeof(buffer) ?
            (size - offset) : sizeof(buffer);
        Platform::VirtualRd((UINT08*)surface->GetAddress() + offset,
            buffer, transferSize);
        if (transferSize != fwrite(buffer, 1, transferSize, file.GetFile()))
        {
            Printf(Tee::PriError, "Error: Unable to save the surface!\n");
            return;
        }
    }
}

// TODO: this class is the point of refactoring. There should a behavioral
// interface defined and described for the LWENC engine. All data members of
// course have to be made private. An implementation for every HW class has to
// be created.
class LWENCEngineData
{
public:
    typedef vector<Surface2D> RefPics;

    virtual ~LWENCEngineData()
    {}

    virtual RC   InitRefPics(GpuSubdevice *subdev,  Memory::Location processingLocation,
                             UINT32 alignment) = 0;
    virtual void FreeRefPics() = 0;
    virtual void SaveRefSurfaces(
        UINT32 streamIndex,
        UINT32 frameIdx,
        UINT32 engNum,
        const DmaWrapper* dmaWrap,
        const GpuSubdevice* subdev,
        const GpuTestConfiguration* testConfig) = 0;
    virtual RC   AddOutSurfToPushBuffer(INT32 surfIdx) = 0;
    virtual RC   AddRefPicToPushBuffer(INT32 inSurfaceIdx, UINT32 refPicIdx) = 0;
    virtual RC CopyInputPicToRefPic(
        Surface2D* pinputPicLuma,
        Surface2D* pinputPicChroma,
        UINT32 refPicIdx,
        UINT32 gpuDisplayInst,
        const GpuTestConfiguration* testConfig,
        DmaWrapper* dmaWrap) = 0;
    virtual RC  AddLastRefPicToPushBuffer(INT32 surfIdx) = 0;
    virtual RC  AddMoCompSurfToPushBuffer(INT32 surfIdx) = 0;

    virtual LWENCEngineData* Clone() const = 0;

    virtual RC ClearSurfaces(GpuSubdevice *pSubdev, SurfaceUtils::FillMethod fillMethod);

    RC AllocateOutputStreamBuffer(
        UINT32 streamIndex,
        UINT32 alignment,
        Memory::Location processingLocation,
        GpuDevice * gpuDevice);

    LwRm::Handle m_hCh;
    Channel     *m_pCh;
    Surface2D    m_Semaphore;
    Surface2D    m_EncodeStatus;
    Surface2D    m_IoRcProcess;

    Surface2D    m_IoHistory;
    Surface2D    m_ColocData[2];
    Surface2D    m_MepredData[3];
    Surface2D    m_TemporalBuf[2];
    Surface2D    m_CounterBuf;
    Surface2D    m_ProbBuf;
    Surface2D    m_CombinedLineBuf;
    Surface2D    m_FilterLineBuf;
    Surface2D    m_FilterColLineBuf;
    Surface2D    m_PartitionBuf;
    Surface2D    m_PicSetup;
    Surface2D    m_UcodeState;      // added for VP8 support
    Surface2D    m_VP8EncStatIo;    // added for VP8 support
    Surface2D    m_CEAHintsData;
    Surface2D    m_TaskStatus;
    RefPics      m_YUVRefPic;
    Surface2D    m_OutBitstream[NUM_STREAMS];
    Surface2D    m_OutBitstreamRes[NUM_STREAMS]; 

    Channel::MappingType m_MappingType = Channel::MapDefault;
    Surface2D::VASpace   m_VASpace     = Surface2D::DefaultVASpace;
};

RC LWENCEngineData::ClearSurfaces(GpuSubdevice *pSubdev, SurfaceUtils::FillMethod fillMethod)
{
    RC rc;
    CHECK_RC(SurfaceUtils::FillSurface(&m_EncodeStatus,    0, m_EncodeStatus.GetBitsPerPixel(),    pSubdev, fillMethod));
    CHECK_RC(SurfaceUtils::FillSurface(&m_IoRcProcess,     0, m_IoRcProcess.GetBitsPerPixel(),     pSubdev, fillMethod));
    CHECK_RC(SurfaceUtils::FillSurface(&m_IoHistory,       0, m_IoHistory.GetBitsPerPixel(),       pSubdev, fillMethod));
    CHECK_RC(SurfaceUtils::FillSurface(&m_UcodeState,      0, m_UcodeState.GetBitsPerPixel(),      pSubdev, fillMethod));
    CHECK_RC(SurfaceUtils::FillSurface(&m_VP8EncStatIo,    0, m_VP8EncStatIo.GetBitsPerPixel(),    pSubdev, fillMethod));
    CHECK_RC(SurfaceUtils::FillSurface(&m_CEAHintsData,    0, m_CEAHintsData.GetBitsPerPixel(),
                                       pSubdev,            fillMethod));
    for (auto& colocData : m_ColocData)
    {
        CHECK_RC(SurfaceUtils::FillSurface(&colocData, 0, colocData.GetBitsPerPixel(), pSubdev, fillMethod));
    }
    for (auto& mepredData: m_MepredData)
    {
        CHECK_RC(SurfaceUtils::FillSurface(&mepredData, 0, mepredData.GetBitsPerPixel(), pSubdev, fillMethod));
    }
    for (auto& temporalBuf : m_TemporalBuf)
    {
        CHECK_RC(SurfaceUtils::FillSurface(&temporalBuf, 0, temporalBuf.GetBitsPerPixel(), pSubdev, fillMethod));
    }
    for (UINT32 i = 0; i < NUM_STREAMS; i++)
    {
        CHECK_RC(SurfaceUtils::FillSurface(
            &m_OutBitstreamRes[i],
            0,
            m_OutBitstreamRes[i].GetBitsPerPixel(),
            pSubdev,
            fillMethod
            ));
        CHECK_RC(SurfaceUtils::FillSurface(
            &m_OutBitstream[i],
            0,
            m_OutBitstream[i].GetBitsPerPixel(),
            pSubdev,
            fillMethod
            ));
    }
    
    CHECK_RC(SurfaceUtils::FillSurface(&m_CounterBuf,       0, m_CounterBuf.GetBitsPerPixel(),       pSubdev, fillMethod));
    CHECK_RC(SurfaceUtils::FillSurface(&m_ProbBuf,          0, m_ProbBuf.GetBitsPerPixel(),          pSubdev, fillMethod));
    CHECK_RC(SurfaceUtils::FillSurface(&m_CombinedLineBuf,  0, m_CombinedLineBuf.GetBitsPerPixel(),  pSubdev, fillMethod));
    CHECK_RC(SurfaceUtils::FillSurface(&m_FilterLineBuf,    0, m_FilterLineBuf.GetBitsPerPixel(),    pSubdev, fillMethod));
    CHECK_RC(SurfaceUtils::FillSurface(&m_FilterColLineBuf, 0, m_FilterColLineBuf.GetBitsPerPixel(), pSubdev, fillMethod));
    CHECK_RC(SurfaceUtils::FillSurface(&m_PartitionBuf,     0, m_PartitionBuf.GetBitsPerPixel(),     pSubdev, fillMethod));
    CHECK_RC(SurfaceUtils::FillSurface(&m_PicSetup,         0, m_PicSetup.GetBitsPerPixel(),         pSubdev, fillMethod));
    return rc;
}

inline
LWENCEngineData* new_clone(const LWENCEngineData& a)
{
    return a.Clone();
}

class ClC0B7EngineData : public LWENCEngineData
{
public:
    RC InitRefPics(GpuSubdevice *subdev, Memory::Location processingLocation,
                   UINT32 alignment) override
    {
        RC rc;

        m_YUVRefPic.resize(MAX_REF_FRAMES);

        RefPics::iterator it;
        for (it = m_YUVRefPic.begin(); m_YUVRefPic.end() != it; ++it)
        {
            // This is the size that H264_HD_HP_CABAC_MS stream needs.
            // All other streams will use a smaller part of this allocation.
            it->SetWidth(MAX_WIDTH);
            it->SetHeight(MAX_HEIGHT * 3 / 2);  // Luma up to line MAX_HEIGHT-1
                                                // Chroma at lines >= MAX_HEIGHT
            it->SetColorFormat(ColorUtils::Y8);
            it->SetAlignment(alignment);
            it->SetType(LWOS32_TYPE_VIDEO);
            it->SetLocation(processingLocation);
            it->SetVASpace(m_VASpace);
            CHECK_RC(it->Alloc(subdev->GetParentDevice()));
            CHECK_RC(it->BindGpuChannel(m_hCh));
        }

        return rc;
    }

    RC ClearSurfaces(GpuSubdevice *pSubdev, SurfaceUtils::FillMethod fillMethod) override
    {
        RC rc;
        CHECK_RC(LWENCEngineData::ClearSurfaces(pSubdev, fillMethod));
        for (auto& surf : m_YUVRefPic)
        {
            CHECK_RC(SurfaceUtils::FillSurface(&surf, 0, surf.GetBitsPerPixel(), pSubdev, fillMethod));
        }
        return rc;
    }

    void FreeRefPics() override
    {
        RefPics::iterator it;
        for (it = m_YUVRefPic.begin(); m_YUVRefPic.end() != it; ++it)
        {
            it->Free();
        }
    }

    void SaveRefSurfaces(
        UINT32 streamIndex,
        UINT32 frameIdx,
        UINT32 engNum,
        const DmaWrapper* dmaWrap,
        const GpuSubdevice* subdev,
        const GpuTestConfiguration* testConfig) override
    {
        for (size_t i = 0; NUMELEMS(refPictureIndexes[streamIndex][frameIdx]) > i; ++i)
        {
            INT32 index = refPictureIndexes[streamIndex][frameIdx][i];
            if (-1 != index)
            {
                MASSERT(m_YUVRefPic.size() > static_cast<size_t>(index));
                string surfName = Utility::StrPrintf("YUVRefPic%d", index);
                SaveSurface(&m_YUVRefPic[index], surfName.c_str(), streamIndex, frameIdx, engNum);
            }
        }
    }

    RC AddOutSurfToPushBuffer(INT32 surfIdx) override
    {
        RC rc;

        if (surfIdx != UNUSED)
        {
            CHECK_RC(m_pCh->WriteWithSurface(0, LWC0B7_SET_OUT_REF_PIC, m_YUVRefPic[surfIdx],
                                         0, 8, m_MappingType));
        }
        
        return rc;
    }

    RC AddRefPicToPushBuffer(INT32 inSurfaceIdx, UINT32 refPicIdx) override
    {
        RC rc;

        if (inSurfaceIdx >= 0)
        {
            CHECK_RC(m_pCh->WriteWithSurface(0, LWC0B7_SET_IN_REF_PIC0 + refPicIdx * 4,
                                             m_YUVRefPic[inSurfaceIdx], 0, 8, m_MappingType));
        }

        return rc;
    }

    RC AddLastRefPicToPushBuffer(INT32 surfIdx) override
    {
        RC rc;

        if (surfIdx >= 0)
        {
            CHECK_RC(m_pCh->WriteWithSurface(0, LWC0B7_SET_IN_REF_PIC_LAST,
                                             m_YUVRefPic[surfIdx], 0, 8, m_MappingType));
        }

        return rc;
    }

    RC AddMoCompSurfToPushBuffer(INT32 surfIdx) override
    {
        if (surfIdx != -1)
        {
            MASSERT(!"SET_IN_MO_COMP_PIC not defined for this class");
            return RC::SOFTWARE_ERROR;
        }
        return OK;
    }

    LWENCEngineData* Clone() const override
    {
        return new ClC0B7EngineData(*this);
    }
    
    //TODO: Make pinputPicLuma as a const pointer
    RC CopyInputPicToRefPic
    (
        Surface2D* pinputPicLuma,
        Surface2D* pinputPicChroma,
        UINT32 refPicIdx,
        UINT32 gpuDisplayInst,
        const GpuTestConfiguration* testConfig,
        DmaWrapper* dmaWrap
    ) override
    {
        return OK;
    }
};

class ClD0B7EngineData : public LWENCEngineData
{
public:
    RC InitRefPics(GpuSubdevice *subdev, Memory::Location processingLocation,
                   UINT32 alignment) override
    {
        RC rc;

        m_YUVRefPic.resize(MAX_REF_FRAMES);

        for (UINT32 refPicIdx = 0; refPicIdx < m_YUVRefPic.size(); ++refPicIdx)
        {
            // This is the size that H264_HD_HP_CABAC_MS stream needs.
            // All other streams will use a smaller part of this allocation.
            // Surface is luma up to line MAX_WIDTH-1, and chroma below that
            m_YUVRefPic[refPicIdx].SetWidth(MAX_WIDTH);
            m_YUVRefPic[refPicIdx].SetHeight(MAX_HEIGHT * 3 / 2);
            m_YUVRefPic[refPicIdx].SetColorFormat(ColorUtils::Y8);
            m_YUVRefPic[refPicIdx].SetAlignment(alignment);
            m_YUVRefPic[refPicIdx].SetType(LWOS32_TYPE_VIDEO);
            m_YUVRefPic[refPicIdx].SetLocation(processingLocation);
            m_YUVRefPic[refPicIdx].SetVASpace(m_VASpace);
            CHECK_RC(m_YUVRefPic[refPicIdx].Alloc(subdev->GetParentDevice()));
            CHECK_RC(m_YUVRefPic[refPicIdx].BindGpuChannel(m_hCh));
        }

        return rc;
    }

    RC ClearSurfaces(GpuSubdevice *pSubdev, SurfaceUtils::FillMethod fillMethod) override
    {
        RC rc;
        CHECK_RC(LWENCEngineData::ClearSurfaces(pSubdev, fillMethod));
        for (auto& surf : m_YUVRefPic)
        {
            CHECK_RC(SurfaceUtils::FillSurface(&surf, 0, surf.GetBitsPerPixel(), pSubdev, fillMethod));
        }
        return rc;
    }

    void FreeRefPics() override
    {
        RefPics::iterator it;
        for (it = m_YUVRefPic.begin(); m_YUVRefPic.end() != it; ++it)
        {
            it->Free();
        }
    }

    void SaveRefSurfaces(
        UINT32 streamIndex,
        UINT32 frameIdx,
        UINT32 engNum,
        const DmaWrapper* dmaWrap,
        const GpuSubdevice* subdev,
        const GpuTestConfiguration* testConfig) override
    {
        for (size_t i = 0; NUMELEMS(refPictureIndexes[streamIndex][frameIdx]) > i; ++i)
        {
            INT32 index = refPictureIndexes[streamIndex][frameIdx][i];
            if (-1 != index)
            {
                MASSERT(m_YUVRefPic.size() > static_cast<size_t>(index));
                string surfName = Utility::StrPrintf("YUVRefPic%d", index);
                SaveSurface(&m_YUVRefPic[index], surfName.c_str(), streamIndex, frameIdx, engNum);
            }
        }
    }

    RC AddOutSurfToPushBuffer(INT32 surfIdx) override
    {
        RC rc;

        if (surfIdx != UNUSED)
        {
            CHECK_RC(m_pCh->WriteWithSurface(0, LWD0B7_SET_OUT_REF_PIC, m_YUVRefPic[surfIdx],
                                         0, 8, m_MappingType));
        }

        return rc;
    }

    RC AddRefPicToPushBuffer(INT32 inSurfaceIdx, UINT32 refPicIdx) override
    {
        RC rc;

        if (inSurfaceIdx >= 0)
        {
            CHECK_RC(m_pCh->WriteWithSurface(0, LWD0B7_SET_IN_REF_PIC0 + refPicIdx * 4,
                                             m_YUVRefPic[inSurfaceIdx], 0, 8, m_MappingType));
        }

        return rc;
    }

    RC AddLastRefPicToPushBuffer(INT32 surfIdx) override
    {
        RC rc;

        if (surfIdx >= 0)
        {
            CHECK_RC(m_pCh->WriteWithSurface(0, LWD0B7_SET_IN_REF_PIC_LAST,
                                             m_YUVRefPic[surfIdx], 0, 8, m_MappingType));
        }

        return rc;
    }

    RC AddMoCompSurfToPushBuffer(INT32 surfIdx) override
    {
        if (surfIdx != -1)
        {
            MASSERT(!"SET_IN_MO_COMP_PIC not defined for this class");
            return RC::SOFTWARE_ERROR;
        }
        return OK;
    }

    LWENCEngineData* Clone() const override
    {
        return new ClD0B7EngineData(*this);
    }

    RC CopyInputPicToRefPic
    (
        Surface2D* pinputPicLuma,
        Surface2D* pinputPicChroma,
        UINT32 refPicIdx,
        UINT32 gpuDisplayInst,
        const GpuTestConfiguration* testConfig,
        DmaWrapper* dmaWrap
    ) override
    {
        return OK;
    }
};

class ClC1B7EngineData : public LWENCEngineData
{
public:
    RC InitRefPics(GpuSubdevice* subdev, Memory::Location processingLocation,
                   UINT32 alignment) override
    {
        RC rc;

        m_YRefPic.resize(MAX_REF_FRAMES);
        m_UVRefPic.resize(MAX_REF_FRAMES);

        for (UINT32 refPicIdx = 0; refPicIdx < m_YRefPic.size(); ++refPicIdx)
        {
            // This is the size that H264_HD_HP_CABAC_MS stream needs.
            // All other streams will use a smaller part of this allocation.
            m_YRefPic[refPicIdx].SetWidth(MAX_WIDTH);
            m_YRefPic[refPicIdx].SetHeight(MAX_HEIGHT);
            m_YRefPic[refPicIdx].SetColorFormat(ColorUtils::Y8);
            m_YRefPic[refPicIdx].SetAlignment(alignment);
            m_YRefPic[refPicIdx].SetType(LWOS32_TYPE_VIDEO);
            m_YRefPic[refPicIdx].SetLocation(processingLocation);
            m_YRefPic[refPicIdx].SetLayout(Surface2D::BlockLinear);
            m_YRefPic[refPicIdx].SetVASpace(m_VASpace);
            CHECK_RC(m_YRefPic[refPicIdx].Alloc(subdev->GetParentDevice()));
            CHECK_RC(m_YRefPic[refPicIdx].BindGpuChannel(m_hCh));
        }

        for (UINT32 refPicIdx = 0; refPicIdx < m_UVRefPic.size(); ++refPicIdx)
        {
            // This is the size that H264_HD_HP_CABAC_MS stream needs.
            // All other streams will use a smaller part of this allocation.
            m_UVRefPic[refPicIdx].SetWidth(MAX_WIDTH);
            m_UVRefPic[refPicIdx].SetHeight(MAX_HEIGHT / 2);
            m_UVRefPic[refPicIdx].SetColorFormat(ColorUtils::Y8);
            m_UVRefPic[refPicIdx].SetAlignment(alignment);
            m_UVRefPic[refPicIdx].SetType(LWOS32_TYPE_VIDEO);
            m_UVRefPic[refPicIdx].SetLocation(processingLocation);
            m_UVRefPic[refPicIdx].SetLayout(Surface2D::BlockLinear);
            m_UVRefPic[refPicIdx].SetVASpace(m_VASpace);
            CHECK_RC(m_UVRefPic[refPicIdx].Alloc(subdev->GetParentDevice()));
            CHECK_RC(m_UVRefPic[refPicIdx].BindGpuChannel(m_hCh));
        }

        return rc;
    }

    RC ClearSurfaces(GpuSubdevice *pSubdev, SurfaceUtils::FillMethod fillMethod) override
    {
        RC rc;
        CHECK_RC(LWENCEngineData::ClearSurfaces(pSubdev, fillMethod));
        for (auto& surf : m_YRefPic)
        {
            CHECK_RC(SurfaceUtils::FillSurface(&surf, 0, surf.GetBitsPerPixel(), pSubdev, fillMethod));
        }
        for (auto& surf : m_UVRefPic)
        {
            CHECK_RC(SurfaceUtils::FillSurface(&surf, 0, surf.GetBitsPerPixel(), pSubdev, fillMethod));
        }
        return rc;
    }

    void FreeRefPics() override
    {
        RefPics::iterator it;
        for (it = m_YRefPic.begin(); m_YRefPic.end() != it; ++it)
        {
            it->Free();
        }

        for (it = m_UVRefPic.begin(); m_UVRefPic.end() != it; ++it)
        {
            it->Free();
        }
    }

    RC SurfaceCreateAndDmaCopy(Surface2D& srcSurf, Surface2D &dstSurf, const GpuSubdevice* subdev,
                               const GpuTestConfiguration* testConfig, DmaWrapper dmaWrap)
    {
        RC rc;

        dstSurf.SetWidth(srcSurf.GetWidth());
        dstSurf.SetHeight(srcSurf.GetHeight());
        dstSurf.SetLayout(Surface2D::Pitch);
        dstSurf.SetColorFormat(srcSurf.GetColorFormat());
        dstSurf.SetLocation(Memory::Coherent);
        dstSurf.SetVASpace(m_VASpace);
        CHECK_RC(dstSurf.Alloc(subdev->GetParentDevice()));
        CHECK_RC(dstSurf.BindGpuChannel(m_hCh));

        CHECK_RC(dmaWrap.SetSurfaces(&srcSurf, &dstSurf));
        CHECK_RC(dmaWrap.Copy(
            0,
            0,
            srcSurf.GetPitch(),
            srcSurf.GetHeight(),
            0,
            0,
            testConfig->TimeoutMs(),
            subdev->GetSubdeviceInst()
        ));

        return rc;
    }

    void SaveRefSurfaces(
        UINT32 streamIndex,
        UINT32 frameIdx,
        UINT32 engNum,
        const DmaWrapper* dmaWrap,
        const GpuSubdevice* subdev,
        const GpuTestConfiguration* testConfig) override
    {
        for (size_t i = 0; NUMELEMS(refPictureIndexes[streamIndex][frameIdx]) > i; ++i)
        {
            INT32 index = refPictureIndexes[streamIndex][frameIdx][i];
            if (-1 != index)
            {
                if (subdev->HasFeature(Device::GPUSUB_SUPPORTS_CPU_FB_MAPPINGS_THROUGH_LWLINK))
                {
                    MASSERT(m_YRefPic.size() > static_cast<size_t>(index));
                    string surfName = Utility::StrPrintf("YRefPic%d", index);
                    Surface2D yRefPicPitch;
                    SurfaceCreateAndDmaCopy(m_YRefPic[index], yRefPicPitch, subdev, testConfig, *dmaWrap);
                    SaveSurface(&yRefPicPitch, surfName.c_str(), streamIndex, frameIdx, engNum);

                    MASSERT(m_UVRefPic.size() > static_cast<size_t>(index));
                    surfName = Utility::StrPrintf("UVRefPic%d", index);
                    Surface2D uvRefPicPitch;
                    SurfaceCreateAndDmaCopy(m_UVRefPic[index], uvRefPicPitch, subdev, testConfig, *dmaWrap);
                    SaveSurface(&uvRefPicPitch, surfName.c_str(), streamIndex, frameIdx, engNum);
                }
                else
                {
                    MASSERT(m_YRefPic.size() > static_cast<size_t>(index));
                    string surfName = Utility::StrPrintf("YRefPic%d", index);
                    SaveSurface(&m_YRefPic[index], surfName.c_str(), streamIndex, frameIdx, engNum);

                    MASSERT(m_UVRefPic.size() > static_cast<size_t>(index));
                    surfName = Utility::StrPrintf("UVRefPic%d", index);
                    SaveSurface(&m_UVRefPic[index], surfName.c_str(), streamIndex, frameIdx, engNum);
                }
            }
        }
    }

    RC AddOutSurfToPushBuffer(INT32 surfIdx) override
    {
        RC rc;
        
        if (surfIdx != UNUSED)
        {
            CHECK_RC(m_pCh->WriteWithSurface(0, LWC1B7_SET_OUT_REF_PIC_LUMA, m_YRefPic[surfIdx],
                                         0, 8, m_MappingType));
            CHECK_RC(m_pCh->WriteWithSurface(0, LWC1B7_SET_OUT_REF_PIC_CHROMA, m_UVRefPic[surfIdx],
                                         0, 8, m_MappingType));
        }

        return rc;
    }

    RC AddRefPicToPushBuffer(INT32 inSurfaceIdx, UINT32 refPicIdx) override
    {
        RC rc;

        if (inSurfaceIdx >= 0)
        {
            CHECK_RC(m_pCh->WriteWithSurface(0, LWC1B7_SET_IN_REF_PIC0_LUMA + refPicIdx * 4,
                                             m_YRefPic[inSurfaceIdx], 0, 8, m_MappingType));
            CHECK_RC(m_pCh->WriteWithSurface(0, LWC1B7_SET_IN_REF_PIC0_CHROMA + refPicIdx * 4,
                                             m_UVRefPic[inSurfaceIdx], 0, 8, m_MappingType));
        }

        return rc;
    }

    RC AddLastRefPicToPushBuffer(INT32 surfIdx) override
    {
        RC rc;

        if (surfIdx >= 0)
        {
            CHECK_RC(m_pCh->WriteWithSurface(0, LWC1B7_SET_IN_REF_PIC_LAST_LUMA,
                                             m_YRefPic[surfIdx], 0, 8, m_MappingType));
            CHECK_RC(m_pCh->WriteWithSurface(0,
                                             LWC1B7_SET_IN_REF_PIC_LAST_CHROMA,
                                             m_UVRefPic[surfIdx], 0, 8, m_MappingType));
        }

        return rc;
    }

    RC AddMoCompSurfToPushBuffer(INT32 surfIdx) override
    {
        RC rc;

        if (surfIdx >= 0)
        {
            CHECK_RC(m_pCh->WriteWithSurface(0, LWC1B7_SET_IN_MO_COMP_PIC,
                                             m_YRefPic[surfIdx], 0, 8, m_MappingType));
            CHECK_RC(m_pCh->WriteWithSurface(0,
                                             LWC1B7_SET_IN_MO_COMP_PIC_CHROMA,
                                             m_UVRefPic[surfIdx], 0, 8, m_MappingType));
        }

        return rc;
    }

    LWENCEngineData* Clone() const override
    {
        return new ClC1B7EngineData(*this);
    }

    RC CopyInputPicToRefPic
    (
       Surface2D* pinputPicLuma,
       Surface2D* pinputPicChroma,
       UINT32 refPicIdx,
       UINT32 gpuDisplayInst,
       const GpuTestConfiguration* testConfig,
       DmaWrapper* dmaWrap
    ) override
    {
        RC rc;

        CHECK_RC(dmaWrap->SetSurfaces(pinputPicLuma, &m_YRefPic[refPicIdx]));
        CHECK_RC(dmaWrap->Copy(
            0,
            0,
            pinputPicLuma->GetPitch(),
            pinputPicLuma->GetHeight(),
            0,
            0,
            testConfig->TimeoutMs(),
            gpuDisplayInst
        ));

        CHECK_RC(dmaWrap->SetSurfaces(pinputPicChroma, &m_UVRefPic[refPicIdx]));
        CHECK_RC(dmaWrap->Copy(
            0,
            0,
            pinputPicChroma->GetPitch(),
            pinputPicChroma->GetHeight(),
            0,
            0,
            testConfig->TimeoutMs(),
            gpuDisplayInst
        ));

        return rc;
    }

private:
    RefPics      m_YRefPic;
    RefPics      m_UVRefPic;
};

class LWENCTest: public GpuTest
{
public:
    LWENCTest();

    SETGET_PROP(KeepRunning, bool);
    SETGET_PROP(SaveSurfaces, bool);
    SETGET_PROP(SaveStreams, bool);
    SETGET_PROP(AllocateInSysmem, bool);
    SETGET_PROP(MaxFrames, UINT32);
    SETGET_PROP(StreamSkipMask, UINT32);
    SETGET_PROP(EngineSkipMask, UINT32);
    SETGET_PROP(StreamYUVInputFromFilesMask, UINT32);
    SETGET_PROP(YUVInputPath, string);

private:
    static constexpr UINT32      s_4KWhiteNoiseWidth = 3840;
    static constexpr UINT32      s_4KWhiteNoiseHeight = 2176;
    static constexpr size_t      s_4KWhiteNoiseNFrames = 8;
    static tuple<
        Surface2D // Y
      , Surface2D // UV
      >                          s_4KWhiteNoise[s_4KWhiteNoiseNFrames];
    static constexpr UINT32      s_StreamsUsing4kNoise = (1 << H265_B_FRAMES_4K);

    static void GenWhiteNoise
    (
        UINT08 *Yaddr,
        UINT32 Ypitch,
        UINT08 *UVaddr,
        UINT32 UVpitch,
        UINT32 width,
        UINT32 height,
        UINT32 randSeed
    );
    static void GenWhiteNoiseThreadFunc(void *);

    RC Setup() override;
    RC Run() override;
    RC Cleanup() override;
    void PrintJsProperties(Tee::Priority pri) override;

    bool IsSupported() override;

    enum
    {
        s_ENC = 0,
    };

    GpuTestConfiguration *m_pTestConfig;
    GpuGoldenSurfaces    *m_pGGSurfs = NULL;
    Goldelwalues         *m_pGolden;

    bool         m_KeepRunning = false;
    bool         m_SaveSurfaces = false;
    bool         m_SaveStreams = false;
    bool         m_AllocateInSysmem = false;
    UINT32       m_MaxFrames = ~0U;
    UINT32       m_StreamSkipMask = 0;
    UINT32       m_EngineSkipMask = 0;
    UINT32       m_StreamYUVInputFromFilesMask = 0;

    UINT32       m_LwrStreamIdx = 0;
    UINT32       m_LwrFrameIdx = 0;

    EncoderAlloc m_LWENCAlloc;

    string       m_YUVInputPath = ".";

    Channel::MappingType m_MappingType    = Channel::MapDefault;
    Surface2D::VASpace   m_VASpace        = Surface2D::DefaultVASpace;

    Surface2D    m_YInput;
    Surface2D    m_UVInput;

    Surface2D    m_YInputPitchLinear;
    Surface2D    m_UVInputPitchLinear;

    vector<unique_ptr<LWENCEngineData>> m_PerEngData;

    DmaWrapper   m_dmaWrap;

    SurfaceUtils::FillMethod m_FillMethod = SurfaceUtils::FillMethod::Any;

    vector<UINT32> channelClasses;
    UINT32       m_EngineCount = 0; // 1 for golden values generation and the total number
                                    // of unmasked engines otherwise

    bool         m_AtleastOneFrameTested = false;

    void SaveSurfaces(UINT32 streamIndex, UINT32 frameIdx, UINT32 engNum);
    RC FillYUVInput(UINT32 streamIndex, UINT32 frameIdx);
    RC Fill4KNoise(UINT32 streamIndex, UINT32 frameIdx);
    RC RunFrame(UINT32 streamIndex, UINT32 frameIdx,
                UINT32 engNum, UINT32 className);
    RC SaveStreams(UINT32 streamIndex, UINT32 frameIdx, UINT32 engNum);
    static void PrintCallback(void* test);
    void Print(INT32 pri);

    //Get Engine Count and allocate required number of channels and surfaces
    RC InitializeEngines();
    RC StartEngine(UINT32 streamIdx, UINT32 frameIdx, UINT32 engNum);
    RC WaitOnEngine(UINT32 streamIndex, UINT32 frameIdx, UINT32 engNum);
    RC GetEngineCount(UINT32* videoEncoderCount);
    UINT32 GetSurfaceAlignment();
};

tuple<Surface2D, Surface2D> LWENCTest::s_4KWhiteNoise[LWENCTest::s_4KWhiteNoiseNFrames];

struct PollSemaphore_Args
{
    UINT32 *semaphore;
    UINT32  expectedValue;
};

static bool GpuPollSemaVal(void * args)
{
    PollSemaphore_Args * pArgs = static_cast<PollSemaphore_Args*>(args);

    return (MEM_RD32(pArgs->semaphore) == pArgs->expectedValue);
}

LWENCTest::LWENCTest()
{
    SetName("LWENCTest");
    m_pTestConfig = GetTestConfiguration();
    m_pGolden = GetGoldelwalues();

    m_LWENCAlloc.SetOldestSupportedClass(LWC0B7_VIDEO_ENCODER);
    m_LWENCAlloc.SetNewestSupportedClass(LWC5B7_VIDEO_ENCODER);
}

bool LWENCTest::IsSupported()
{
    return m_LWENCAlloc.IsSupported(GetBoundGpuDevice());
}

static void WritePicSetupSurface
(
    UINT32 frameIdx,
    Surface2D *picSetupSurface,
    const lwenc_h264_drv_pic_setup_s *picSetup,
    const lwenc_h264_slice_control_s *sliceControl,
    size_t                            numSlices,
    const lwenc_h264_md_control_s    *mdControl,
    const lwenc_h264_quant_control_s *qControl,
    const lwenc_h264_me_control_s    *meControl,
    UINT32 localControlStructsSize = h264H265VP9controlStructsSize
)
{
    UINT08 *baseAddress = static_cast<UINT08*> (picSetupSurface->GetAddress());
    baseAddress += frameIdx * localControlStructsSize;

    Platform::VirtualWr(baseAddress, picSetup,
        sizeof(lwenc_h264_drv_pic_setup_s));

    unsigned int sliceCtrlOffset;
    sliceCtrlOffset = picSetup->pic_control.slice_control_offset;
    for (size_t i = 0; numSlices > i; ++i)
    {
        Platform::VirtualWr(
            baseAddress + sliceCtrlOffset,
            sliceControl,
            sizeof(lwenc_h264_slice_control_s));
        sliceCtrlOffset += sizeof(lwenc_h264_slice_control_s);
    }
    Platform::VirtualWr(baseAddress +
        picSetup->pic_control.md_control_offset, mdControl,
        sizeof(lwenc_h264_md_control_s));

    Platform::VirtualWr(baseAddress +
        picSetup->pic_control.q_control_offset, qControl,
        sizeof(lwenc_h264_quant_control_s));

    Platform::VirtualWr(baseAddress +
        picSetup->pic_control.me_control_offset, meControl,
        sizeof(lwenc_h264_me_control_s));
}

static void WritePicSetupSurface
(
    UINT32 frameIdx,
    Surface2D *picSetupSurface,
    const lwenc_h265_drv_pic_setup_s *picSetup,
    const lwenc_h265_slice_control_s *sliceControl,
    size_t                            numSlices,
    const lwenc_h265_md_control_s    *mdControl,
    const lwenc_h265_quant_control_s *qControl,
    const lwenc_h264_me_control_s    *meControl,
    UINT32 localControlStructsSize = h264H265VP9controlStructsSize
)
{
    UINT08 *baseAddress = static_cast<UINT08*> (picSetupSurface->GetAddress());
    baseAddress += frameIdx * localControlStructsSize;

    Platform::VirtualWr(baseAddress, picSetup,
        sizeof(lwenc_h265_drv_pic_setup_s));

    unsigned int sliceCtrlOffset;
    sliceCtrlOffset = picSetup->pic_control.slice_control_offset;
    for (size_t i = 0; numSlices > i; ++i)
    {
        Platform::VirtualWr(
            baseAddress + sliceCtrlOffset,
            sliceControl,
            sizeof(lwenc_h265_slice_control_s));
        sliceCtrlOffset += sizeof(lwenc_h265_slice_control_s);
    }
    Platform::VirtualWr(baseAddress +
        picSetup->pic_control.md_control_offset, mdControl,
        sizeof(lwenc_h265_md_control_s));

    Platform::VirtualWr(baseAddress +
        picSetup->pic_control.q_control_offset, qControl,
        sizeof(lwenc_h265_quant_control_s));

    Platform::VirtualWr(baseAddress +
        picSetup->pic_control.me_control_offset, meControl,
        sizeof(lwenc_h264_me_control_s));
}

static void WritePicSetupSurface
(
    UINT32 frameIdx,
    Surface2D *picSetupSurface,
    const lwenc_vp9_drv_pic_setup_s  *picSetup,
    const lwenc_h265_slice_control_s *sliceControl,
    size_t                            numSlices,
    const lwenc_h265_md_control_s    *mdControl,
    const lwenc_h265_quant_control_s *qControl,
    const lwenc_h264_me_control_s    *meControl,
    UINT32 localControlStructsSize = h264H265VP9controlStructsSize
)
{
    UINT08 *baseAddress = static_cast<UINT08*> (picSetupSurface->GetAddress());
    baseAddress += frameIdx * localControlStructsSize;

    Platform::VirtualWr(baseAddress, picSetup,
        sizeof(lwenc_vp9_drv_pic_setup_s));

    unsigned int sliceCtrlOffset;
    sliceCtrlOffset = picSetup->h265_pic_control.slice_control_offset;
    for (size_t i = 0; numSlices > i; ++i)
    {
        Platform::VirtualWr(
            baseAddress + sliceCtrlOffset,
            sliceControl,
            sizeof(lwenc_h265_slice_control_s));
        sliceCtrlOffset += sizeof(lwenc_h265_slice_control_s);
    }
    Platform::VirtualWr(baseAddress +
        picSetup->h265_pic_control.md_control_offset, mdControl,
        sizeof(lwenc_h265_md_control_s));

    Platform::VirtualWr(baseAddress +
        picSetup->h265_pic_control.q_control_offset, qControl,
        sizeof(lwenc_h265_quant_control_s));

    Platform::VirtualWr(baseAddress +
        picSetup->h265_pic_control.me_control_offset, meControl,
        sizeof(lwenc_h264_me_control_s));
}

inline static void WritePicSetupSurface
(
    UINT32 frameIdx,
    Surface2D* picSetupSurface,
    const lwenc_vp8_drv_pic_setup_s* picSetup,
    UINT32 localControlStructsSize = allControlStructsSizes[VP8_GENERIC]
)
{
    // The difference of VP8 control structures from
    // the h264/h265/vp9's is that VP8 does not use offsets
    // to point to other control structs such as me_control
    // lwenc_vp8_drv_pic_setup_s contains all the control
    // structs needed.
    UINT08* baseAddress = static_cast<UINT08*> (picSetupSurface->GetAddress());
    baseAddress += frameIdx * localControlStructsSize;
    Platform::VirtualWr(baseAddress, picSetup, sizeof(lwenc_vp8_drv_pic_setup_s));
}

void SetMagicValue(UINT32 className, unsigned int *magic)
{
    MASSERT (nullptr != magic);
    *magic = 0;
    switch (className)
    {
        case LWC0B7_VIDEO_ENCODER :
            *magic = LW_LWENC_DRV_MAGIC_VALUE_1_0;
            break;
        case LWD0B7_VIDEO_ENCODER :
            *magic = LW_LWENC_DRV_MAGIC_VALUE_5_0;
            break;
        case LWC1B7_VIDEO_ENCODER:
            *magic = LW_LWENC_DRV_MAGIC_VALUE_6_0;
            break;
        case LWC2B7_VIDEO_ENCODER:
            *magic = LW_LWENC_DRV_MAGIC_VALUE_6_2;
            break;
        case LWC3B7_VIDEO_ENCODER:
            *magic = LW_LWENC_DRV_MAGIC_VALUE_6_4;
            break;
        case LWC4B7_VIDEO_ENCODER:
            *magic = LW_LWENC_DRV_MAGIC_VALUE_7_2;
            break;
        case LWC5B7_VIDEO_ENCODER:
            *magic = LW_LWENC_DRV_MAGIC_VALUE_7_0;
            break;
        case LWB4B7_VIDEO_ENCODER:
            *magic = LW_LWENC_DRV_MAGIC_VALUE_6_6;
            break;
        case LWC7B7_VIDEO_ENCODER:
            *magic = LW_LWENC_DRV_MAGIC_VALUE_7_3;
            break;
        default:
            MASSERT(!"Magic value not defined for this class\n");
    }
}

void SetDefaultStreamSkipMask
(
    GpuSubdevice *pGpuSubdevice,
    UINT32 className,
    UINT32 *mask
)
{
    MASSERT (nullptr != mask);
    switch (className)
    {
        case LWC0B7_VIDEO_ENCODER :
            *mask = *mask | C0B7_DEFAULT_STREAM_SKIP;
            break;
        case LWD0B7_VIDEO_ENCODER :
            *mask = *mask | D0B7_DEFAULT_STREAM_SKIP;
            break;
        case LWC1B7_VIDEO_ENCODER:
            if (pGpuSubdevice->IsSOC())
                *mask = *mask | C1B7_DEFAULT_STREAM_SKIP_SOC;
            else
                *mask = *mask | C1B7_DEFAULT_STREAM_SKIP_DGPU;
            break;
        case LWC2B7_VIDEO_ENCODER:
            *mask = *mask | C2B7_DEFAULT_STREAM_SKIP;
            break;
        case LWC3B7_VIDEO_ENCODER:
            *mask = *mask | C3B7_DEFAULT_STREAM_SKIP;
            break;
        case LWC4B7_VIDEO_ENCODER:
            *mask = *mask | C4B7_DEFAULT_STREAM_SKIP;
            break;
        case LWC5B7_VIDEO_ENCODER:
            *mask = *mask | C5B7_DEFAULT_STREAM_SKIP;
            break;
        case LWB4B7_VIDEO_ENCODER:
            *mask = *mask | B4B7_DEFAULT_STREAM_SKIP;
            break;
        case LWC7B7_VIDEO_ENCODER:
            *mask = *mask | C7B7_DEFAULT_STREAM_SKIP;
            break;
        default:
            MASSERT(!" default stream skipmask not defined for this class\n");
    }
}

// Used by PrepareSurfaceCfg()
enum SurfaceFmt
{
    SURFFMT_TILED,  // Tiled 16x16
    SURFFMT_BL1,    // Block linear, height = 1
    SURFFMT_BL2     // Block linear, height = 2
};
enum SurfaceType
{
    SURFTYPE_INPUT, // Luma & chroma are separate surfaces
    SURFTYPE_REFPIC // Luma & chroma are in same surface pre-Pascal
};
enum SurfaceColor
{
    SURFCOLOR_YUV420, // 4:2:0; chroma has half resolution of luma
    SURFCOLOR_YUV444  // 4:4:4; chroma has same resolution as luma
};

// Fill most of the fields of an h264_surface_cfg struct, assuming a
// packed semiplanar YUV surface.  Assume the caller already cleared
// the struct.
static RC PrepareSurfaceCfg
(
    lwenc_h264_surface_cfg_s *pSurfaceCfg,
    UINT32 className,
    unsigned short frameWidth,
    unsigned short frameHeight,
    unsigned short frameDepth,
    SurfaceType surfType,
    SurfaceFmt surfFmt,
    SurfaceColor surfColor,
    bool interlaced
)
{
    // For surfaces that contain multiple images, these vars keep
    // track of the offset in the surface in units of 256
    const unsigned short byteDepth    = AlignUp<8>(frameDepth) / 8;
    const unsigned short lumaPitch    = frameWidth * byteDepth;
    const unsigned short lumaHeight   = frameHeight;    //**? alignment needed?
    const unsigned short chromaPitch  =
        frameWidth * byteDepth * (surfColor == SURFCOLOR_YUV420 ? 1 : 2);
    const unsigned short chromaHeight =
        frameHeight / (surfColor == SURFCOLOR_YUV420 ? 2 : 1);
    const unsigned lumaSize256    = (lumaPitch * lumaHeight + 255) / 256;
    const unsigned chromaSize256  = (chromaPitch * chromaHeight + 255) / 256;
    unsigned lwrOffset256 = 0;

    pSurfaceCfg->frame_width_minus1    = frameWidth - 1;
    pSurfaceCfg->frame_height_minus1   = frameHeight - 1;
    pSurfaceCfg->sfc_pitch             = lumaPitch / byteDepth;
    pSurfaceCfg->sfc_pitch_chroma      = chromaPitch / byteDepth;
    pSurfaceCfg->luma_top_frm_offset   = lwrOffset256;
    lwrOffset256 += lumaSize256;
    if (interlaced)
    {
        pSurfaceCfg->luma_bot_offset   = lwrOffset256;
        lwrOffset256 += lumaSize256;
    }
    if (surfType == SURFTYPE_INPUT || (className != LWC0B7_VIDEO_ENCODER &&
                                       className != LWD0B7_VIDEO_ENCODER))
    {
        // Reset offset to zero if chroma is in separate surface
        lwrOffset256 = 0;
    }
    pSurfaceCfg->chroma_top_frm_offset = lwrOffset256;
    lwrOffset256 += chromaSize256;
    if (interlaced)
    {
        pSurfaceCfg->chroma_bot_offset = lwrOffset256;
        lwrOffset256 += chromaSize256;
    }
    switch (surfFmt)
    {
        case SURFFMT_TILED:
            pSurfaceCfg->tiled_16x16 = 1;
            break;
        case SURFFMT_BL1:
            pSurfaceCfg->block_height = 1;
            break;
        case SURFFMT_BL2:
            pSurfaceCfg->block_height = 2;
            break;
        default:
            MASSERT(!"Unknown surface format");
            Printf(Tee::PriError, "Unknown surface format\n");
            return RC::SOFTWARE_ERROR;
    }

    pSurfaceCfg->memory_mode = (surfColor == SURFCOLOR_YUV420 ? 0 : 3);

    // Remaining fields get set by caller:
    // sfc_trans_mode
    // lw21_enable
    // input_bl_mode
    return OK;
}

// Fill most of the fields of an h264_surface_cfg struct, assuming a
// packed semiplanar YUV surface.  Assume the caller already cleared
// the struct.
static RC PrepareSurfaceCfg
(
    lwenc_h265_surface_cfg_s *pSurfaceCfg,
    UINT32 className,
    unsigned short frameWidth,
    unsigned short frameHeight,
    unsigned short frameDepth,
    SurfaceType surfType,
    SurfaceFmt surfFmt,
    SurfaceColor surfColor,
    bool interlaced
)
{
    // lwenc_h265_surface_cfg_s happens to be bitwise identical with
    // lwenc_h264_surface_cfg_s, so we can save some duplication here
    // by calling the 264 routine.
    return PrepareSurfaceCfg(
            reinterpret_cast<lwenc_h264_surface_cfg_s*>(pSurfaceCfg),
            className, frameWidth, frameHeight, frameDepth,
            surfType, surfFmt, surfColor, interlaced);
}

static RC PrepareControlStructuresH264BaselineCAVLC(Surface2D *picSetupSurface, UINT32 className, UINT32 outBitstreamBufferSize)
{
    RC rc;
    lwenc_h264_drv_pic_setup_s picSetup;
    memset(&picSetup, 0, sizeof(picSetup));

    SetMagicValue(className, &picSetup.magic);

    CHECK_RC(PrepareSurfaceCfg(&picSetup.refpic_cfg, className, 176, 144, 8,
                               SURFTYPE_REFPIC, SURFFMT_TILED,
                               SURFCOLOR_YUV420, false));
    CHECK_RC(PrepareSurfaceCfg(&picSetup.input_cfg, className, 176, 144, 8,
                               SURFTYPE_INPUT, SURFFMT_BL2,
                               SURFCOLOR_YUV420, false));
    CHECK_RC(PrepareSurfaceCfg(&picSetup.outputpic_cfg, className, 176, 144, 8,
                               SURFTYPE_REFPIC, SURFFMT_TILED,
                               SURFCOLOR_YUV420, false));

    picSetup.sps_data.profile_idc                        = 66;
    picSetup.sps_data.level_idc                          = 30;
    picSetup.sps_data.chroma_format_idc                  = 1;
    picSetup.sps_data.pic_order_cnt_type                 = 2;
    picSetup.sps_data.log2_max_frame_num_minus4          = 8 - 4;
    picSetup.sps_data.log2_max_pic_order_cnt_lsb_minus4  = 8 - 4;
    picSetup.sps_data.frame_mbs_only                     = 1;

    picSetup.pps_data.deblocking_filter_control_present_flag = 1;

    picSetup.rate_control.hrd_type     = 2;
    picSetup.rate_control.QP[0]        = 28;
    picSetup.rate_control.QP[1]        = 31;
    picSetup.rate_control.QP[2]        = 25;
    picSetup.rate_control.minQP[0]     = 1;
    picSetup.rate_control.minQP[1]     = 1;
    picSetup.rate_control.minQP[2]     = 1;
    picSetup.rate_control.maxQP[0]     = 51;
    picSetup.rate_control.maxQP[1]     = 51;
    picSetup.rate_control.maxQP[2]     = 51;
    picSetup.rate_control.maxQPD       = 6;
    picSetup.rate_control.baseQPD      = 3;
    picSetup.rate_control.rhopbi[0]    = 256;
    picSetup.rate_control.rhopbi[1]    = 256;
    picSetup.rate_control.rhopbi[2]    = 256;
    picSetup.rate_control.framerate    = 7680;
    picSetup.rate_control.buffersize   = 1024000;
    picSetup.rate_control.nal_cpb_size = 1024000;
    picSetup.rate_control.nal_bitrate  = 512000;
    picSetup.rate_control.gop_length   = 10;
    picSetup.rate_control.rc_class     = 1;

    for (UINT32 idx = 0; idx <= 7; idx++)
    {
        picSetup.pic_control.l0[idx] = -2;
        picSetup.pic_control.l1[idx] = -2;
    }
    picSetup.pic_control.slice_control_offset = sliceControlOffset;
    picSetup.pic_control.md_control_offset    = mdControlOffset;
    picSetup.pic_control.q_control_offset     = quantControlOffset;
    picSetup.pic_control.me_control_offset    = meControlOffset;
    picSetup.pic_control.hist_buf_size        = 14080;
    picSetup.pic_control.bitstream_buf_size   = outBitstreamBufferSize;
    picSetup.pic_control.pic_type             = 3;
    picSetup.pic_control.ref_pic_flag         = 1;
    picSetup.pic_control.ipcm_rewind_enable   = 1;

    lwenc_h264_slice_control_s sliceControl;
    memset(&sliceControl, 0, sizeof(sliceControl));

    sliceControl.num_mb = 99;
    sliceControl.qp_avr = 25;

    lwenc_h264_md_control_s mdControl;
    memset(&mdControl, 0, sizeof(mdControl));

    mdControl.intra_luma4x4_mode_enable       = 511;
    mdControl.intra_luma16x16_mode_enable     = 15;
    mdControl.intra_chroma_mode_enable        = 15;
    mdControl.l0_part_16x16_enable            = 1;
    mdControl.l0_part_16x8_enable             = 1;
    mdControl.l0_part_8x16_enable             = 1;
    mdControl.l0_part_8x8_enable              = 1;
    mdControl.l1_part_16x16_enable            = 1;
    mdControl.l1_part_16x8_enable             = 1;
    mdControl.l1_part_8x16_enable             = 1;
    mdControl.l1_part_8x8_enable              = 1;
    mdControl.bi_part_16x16_enable            = 1;
    mdControl.bi_part_16x8_enable             = 1;
    mdControl.bi_part_8x16_enable             = 1;
    mdControl.bi_part_8x8_enable              = 1;
    mdControl.intra_nxn_bias_multiplier       = 24;
    mdControl.intra_most_prob_bias_multiplier = 4;
    mdControl.mv_cost_enable                  = 1;
    mdControl.ip_search_mode                  = 7;

    lwenc_h264_quant_control_s qControl;
    memset(&qControl, 0, sizeof(qControl));

    qControl.qpp_run_vector_4x4    = 1387;
    qControl.qpp_run_vector_8x8[0] = 43775;
    qControl.qpp_run_vector_8x8[1] = 65450;
    qControl.qpp_run_vector_8x8[2] = 15;
    qControl.qpp_luma8x8_cost      = 15;
    qControl.qpp_luma16x16_cost    = 15;
    qControl.qpp_chroma_cost       = 15;
    qControl.quant_intra_sat_flag  = 1;
    qControl.quant_inter_sat_flag  = 1;
    qControl.quant_intra_sat_limit = 2063;
    qControl.quant_inter_sat_limit = 2063;
    qControl.dz_4x4_YI[0]          = 1023;
    qControl.dz_4x4_YI[1]          = 878;
    qControl.dz_4x4_YI[2]          = 820;
    qControl.dz_4x4_YI[3]          = 682;
    qControl.dz_4x4_YI[4]          = 878;
    qControl.dz_4x4_YI[5]          = 820;
    qControl.dz_4x4_YI[6]          = 682;
    qControl.dz_4x4_YI[7]          = 512;
    qControl.dz_4x4_YI[8]          = 820;
    qControl.dz_4x4_YI[9]          = 682;
    qControl.dz_4x4_YI[10]         = 512;
    qControl.dz_4x4_YI[11]         = 410;
    qControl.dz_4x4_YI[12]         = 682;
    qControl.dz_4x4_YI[13]         = 512;
    qControl.dz_4x4_YI[14]         = 410;
    qControl.dz_4x4_YI[15]         = 410;
    qControl.dz_4x4_YP[0]          = 682;
    qControl.dz_4x4_YP[1]          = 586;
    qControl.dz_4x4_YP[2]          = 546;
    qControl.dz_4x4_YP[3]          = 456;
    qControl.dz_4x4_YP[4]          = 586;
    qControl.dz_4x4_YP[5]          = 546;
    qControl.dz_4x4_YP[6]          = 456;
    qControl.dz_4x4_YP[7]          = 342;
    qControl.dz_4x4_YP[8]          = 546;
    qControl.dz_4x4_YP[9]          = 456;
    qControl.dz_4x4_YP[10]         = 342;
    qControl.dz_4x4_YP[11]         = 292;
    qControl.dz_4x4_YP[12]         = 456;
    qControl.dz_4x4_YP[13]         = 342;
    qControl.dz_4x4_YP[14]         = 292;
    qControl.dz_4x4_YP[15]         = 274;
    qControl.dz_4x4_CI             = 682;
    qControl.dz_4x4_CP             = 342;
    for (size_t i = 0; NUMELEMS(qControl.dz_8x8_YI) > i; ++i)
    {
        qControl.dz_8x8_YI[i] = 682;
    }
    for (size_t i = 0; NUMELEMS(qControl.dz_8x8_YP) > i; ++i)
    {
        qControl.dz_8x8_YP[i] = 342;
    }

    lwenc_h264_me_control_s meControl;
    memset(&meControl, 0, sizeof(meControl));

    meControl.me_predictor_mode           = 1;
    meControl.refinement_mode             = 1;
    meControl.lambda_mode                 = 1;
    meControl.const_lambda                = 4;
    meControl.refine_on_search_enable     = 1;
    meControl.limit_mv.mv_limit_enable    = 1;
    meControl.limit_mv.left_mvx_int       = -2048;
    meControl.limit_mv.top_mvy_int        = -256;
    meControl.limit_mv.right_mvx_frac     = 3;
    meControl.limit_mv.right_mvx_int      = 2047;
    meControl.limit_mv.bottom_mvy_frac    = 3;
    meControl.limit_mv.bottom_mvy_int     = 255;
    meControl.predsrc.self_spatial_search = 1;
    meControl.predsrc.self_spatial_refine = 1;
    meControl.predsrc.self_spatial_enable = 1;
    meControl.predsrc.const_mv_search     = 1;
    meControl.predsrc.const_mv_refine     = 1;
    meControl.predsrc.const_mv_enable     = 1;
    meControl.fps_mvcost                  = 1;
    meControl.sps_mvcost                  = 1;
    meControl.sps_cost_func               = 1;
    meControl.vc1_mc_rnd                  = 1;
    meControl.shape0.bitmask[0]           = 0x7e7e7e7e;
    meControl.shape0.bitmask[1]           = 0x7e7e7e7e;
    meControl.mbc_mb_size                 = 110;

    if (className == LWC5B7_VIDEO_ENCODER || className == LWC4B7_VIDEO_ENCODER)
    {
        meControl.spatial_hint_pattern = 0xf;
        meControl.temporal_hint_pattern = 0x3f;
        meControl.Lw16partDecisionMadeByFPP = 0;
#ifdef MISRA_5_2
        meControl.fbm_op_winner_num_p_frame = 1;
        meControl.fbm_op_winner_num_b_frame_l0 = 1;
        meControl.fbm_op_winner_num_b_frame_l1 = 1;
#else
        meControl.fbm_output_winner_num_p_frame = 1;
        meControl.fbm_output_winner_num_b_frame_l0 = 1;
        meControl.fbm_output_winner_num_b_frame_l1 = 1;
#endif
        meControl.fbm_select_best_lw32_parttype_num = 3;
        meControl.sps_evaluate_merge_cand_num = 5;
        meControl.fps_quad_thresh_hold = 0;
        meControl.external_hint_order = 1;
        meControl.coloc_hint_order = 1;
        meControl.ct_threshold = 0;
        meControl.hint_type0 = 0;
        meControl.hint_type1 = 1;
        meControl.hint_type2 = 2;
        meControl.hint_type3 = 3;
        meControl.hint_type4 = 4;

        mdControl.l0_part_8x4_enable = 1;
        mdControl.l0_part_4x8_enable = 1;
        mdControl.l0_part_4x4_enable = 1;
        mdControl.intra_ssd_cnt_4x4 = 1;
        mdControl.intra_ssd_cnt_8x8 = 1;
        mdControl.intra_ssd_cnt_16x16 = 1;
        mdControl.skip_evaluate_enable = 0;
        mdControl.rdo_level = 1;
        mdControl.tu_search_num = 2;

        picSetup.pic_control.chroma_skip_threshold_4x4 = 8;
    }

    WritePicSetupSurface(0, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);

    picSetup.pic_control.l0[0] = 0;

    for (UINT32 idx = 0; idx <= 7; idx++)
    {
        picSetup.pic_control.temp_dist_l0[idx] = -1;
        picSetup.pic_control.temp_dist_l1[idx] = -1;
    }
    picSetup.pic_control.temp_dist_l0[0] = 1;

    for (UINT32 refdix1 = 0; refdix1 <= 7; refdix1++)
    {
        for (UINT32 refdix0 = 0; refdix0 <= 7; refdix0++)
        {
            picSetup.pic_control.dist_scale_factor[refdix1][refdix0] =
                (refdix0 == 0) ? 256 : -1;
        }
    }
    picSetup.pic_control.diff_pic_order_cnt_zero[0] = 0xfefefefe;
    picSetup.pic_control.diff_pic_order_cnt_zero[1] = 0xfefefefe;

    picSetup.pic_control.frame_num = 1;
    picSetup.pic_control.pic_order_cnt_lsb = 2;
    picSetup.pic_control.pic_type = 0;

    sliceControl.qp_avr = 28;

    meControl.vc1_mc_rnd = 0;

    WritePicSetupSurface(1, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);

    picSetup.pic_control.l0[0] = 2;
    picSetup.pic_control.frame_num = 2;
    picSetup.pic_control.pic_order_cnt_lsb = 4;
    meControl.vc1_mc_rnd = 1;

    WritePicSetupSurface(2, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);

    picSetup.pic_control.l0[0] = 4;
    picSetup.pic_control.frame_num = 3;
    picSetup.pic_control.pic_order_cnt_lsb = 6;
    meControl.vc1_mc_rnd = 0;

    WritePicSetupSurface(3, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);

    H264::SeqParameterSet sps;
    sps.profile_idc                       = picSetup.sps_data.profile_idc;
    sps.constraint_set1_flag              = true;
    sps.level_idc                         = picSetup.sps_data.level_idc;
    sps.chroma_format_idc                 = picSetup.sps_data.chroma_format_idc;
    sps.pic_order_cnt_type                = picSetup.sps_data.pic_order_cnt_type;
    sps.log2_max_frame_num_minus4         = picSetup.sps_data.log2_max_frame_num_minus4;
    sps.log2_max_pic_order_cnt_lsb_minus4 = picSetup.sps_data.log2_max_pic_order_cnt_lsb_minus4;
    sps.pic_width_in_mbs_minus1           = (picSetup.input_cfg.frame_width_minus1 + 1) / 16 - 1;
    sps.pic_height_in_map_units_minus1    = (picSetup.input_cfg.frame_height_minus1 + 1) / 16 - 1;
    sps.frame_mbs_only_flag               = picSetup.sps_data.frame_mbs_only;
    sps.max_num_ref_frames                = 1;
    sps.direct_8x8_inference_flag         = true;
    sps.UnsetEmpty();
    spsNalus[H264_BASELINE_CAVLC].CreateDataFromSPS(sps);

    SeqSetSrcImpl spsSrc(sps);
    H264::PicParameterSet pps(&spsSrc);
    pps.deblocking_filter_control_present_flag = picSetup.pps_data.deblocking_filter_control_present_flag;
    pps.UnsetEmpty();
    ppsNalus[H264_BASELINE_CAVLC].CreateDataFromPPS(pps);

    return OK;
}

static RC PrepareControlStructuresH264HighprofileCABAC
(
    Surface2D *picSetupSurface,
    Surface2D *ioRcProcess,
    UINT32 className,
    UINT32 outBitstreamBufferSize
)
{
    RC rc;
    lwenc_h264_drv_pic_setup_s picSetup;
    memset(&picSetup, 0, sizeof(picSetup));

    SetMagicValue(className, &picSetup.magic);

    CHECK_RC(PrepareSurfaceCfg(&picSetup.refpic_cfg, className, 176, 80, 8,
                               SURFTYPE_REFPIC, SURFFMT_TILED,
                               SURFCOLOR_YUV420, true));
    CHECK_RC(PrepareSurfaceCfg(&picSetup.input_cfg, className, 176, 80, 8,
                               SURFTYPE_INPUT, SURFFMT_BL2,
                               SURFCOLOR_YUV420, false));
    CHECK_RC(PrepareSurfaceCfg(&picSetup.outputpic_cfg, className, 176, 80, 8,
                               SURFTYPE_REFPIC, SURFFMT_TILED,
                               SURFCOLOR_YUV420, true));

    picSetup.sps_data.profile_idc                        = 100;
    picSetup.sps_data.level_idc                          = 41;
    picSetup.sps_data.chroma_format_idc                  = 1;
    picSetup.sps_data.log2_max_frame_num_minus4          = 8 - 4;
    picSetup.sps_data.log2_max_pic_order_cnt_lsb_minus4  = 8 - 4;

    picSetup.pps_data.entropy_coding_mode_flag               = 1;
    picSetup.pps_data.num_ref_idx_l0_active_minus1           = 4 - 1;
    picSetup.pps_data.deblocking_filter_control_present_flag = 1;
    picSetup.pps_data.transform_8x8_mode_flag                = 1;

    picSetup.rate_control.hrd_type     = 2;
    picSetup.rate_control.QP[0]        = 28;
    picSetup.rate_control.QP[1]        = 31;
    picSetup.rate_control.QP[2]        = 25;
    picSetup.rate_control.minQP[0]     = 1;
    picSetup.rate_control.minQP[1]     = 1;
    picSetup.rate_control.minQP[2]     = 1;
    picSetup.rate_control.maxQP[0]     = 51;
    picSetup.rate_control.maxQP[1]     = 51;
    picSetup.rate_control.maxQP[2]     = 51;
    picSetup.rate_control.maxQPD       = 6;
    picSetup.rate_control.baseQPD      = 3;
    picSetup.rate_control.rhopbi[0]    = 286;
    picSetup.rate_control.rhopbi[1]    = 317;
    picSetup.rate_control.rhopbi[2]    = 256;
    picSetup.rate_control.framerate    = 7680;
    picSetup.rate_control.buffersize   = 512000;
    picSetup.rate_control.nal_cpb_size = 512000;
    picSetup.rate_control.nal_bitrate  = 256000;
    picSetup.rate_control.gop_length   = 10;
    picSetup.rate_control.Np           = 2;
    picSetup.rate_control.Bmin         = 930;
    picSetup.rate_control.R            = 77;
    picSetup.rate_control.rc_class     = 1;

    for (UINT32 idx = 0; idx <= 7; idx++)
    {
        picSetup.pic_control.l0[idx] = -2;
        picSetup.pic_control.l1[idx] = -2;
    }
    picSetup.pic_control.max_slice_size       = 500;
    picSetup.pic_control.slice_control_offset = sliceControlOffset;
    picSetup.pic_control.me_control_offset    = meControlOffset;
    picSetup.pic_control.md_control_offset    = mdControlOffset;
    picSetup.pic_control.q_control_offset     = quantControlOffset;
    picSetup.pic_control.hist_buf_size        = 8448;
    picSetup.pic_control.bitstream_buf_size   = outBitstreamBufferSize;
    picSetup.pic_control.pic_struct           = 1;
    picSetup.pic_control.pic_type             = 3;
    picSetup.pic_control.ref_pic_flag         = 1;
    picSetup.pic_control.slice_mode           = 1;
    picSetup.pic_control.ipcm_rewind_enable   = 1;

    lwenc_h264_slice_control_s sliceControl;
    memset(&sliceControl, 0, sizeof(sliceControl));

    sliceControl.num_mb = 55;
    sliceControl.qp_avr = 25;

    lwenc_h264_md_control_s mdControl;
    memset(&mdControl, 0, sizeof(mdControl));

    mdControl.intra_luma4x4_mode_enable       = 511;
    mdControl.intra_luma8x8_mode_enable       = 511;
    mdControl.intra_luma16x16_mode_enable     = 15;
    mdControl.intra_chroma_mode_enable        = 15;
    mdControl.l0_part_16x16_enable            = 1;
    mdControl.l0_part_16x8_enable             = 1;
    mdControl.l0_part_8x16_enable             = 1;
    mdControl.l0_part_8x8_enable              = 1;
    mdControl.l1_part_16x16_enable            = 1;
    mdControl.l1_part_16x8_enable             = 1;
    mdControl.l1_part_8x16_enable             = 1;
    mdControl.l1_part_8x8_enable              = 1;
    mdControl.bi_part_16x16_enable            = 1;
    mdControl.bi_part_16x8_enable             = 1;
    mdControl.bi_part_8x16_enable             = 1;
    mdControl.bi_part_8x8_enable              = 1;
    mdControl.bdirect_mode                    = 2;
    mdControl.intra_nxn_bias_multiplier       = 24;
    mdControl.intra_most_prob_bias_multiplier = 4;
    mdControl.mv_cost_enable                  = 1;
    mdControl.ip_search_mode                  = 7;

    lwenc_h264_quant_control_s qControl;
    memset(&qControl, 0, sizeof(qControl));

    qControl.qpp_run_vector_4x4    = 1387;
    qControl.qpp_run_vector_8x8[0] = 43775;
    qControl.qpp_run_vector_8x8[1] = 65450;
    qControl.qpp_run_vector_8x8[2] = 15;
    qControl.qpp_luma8x8_cost      = 15;
    qControl.qpp_luma16x16_cost    = 15;
    qControl.qpp_chroma_cost       = 15;
    qControl.dz_4x4_YI[ 0]         = 1023;
    qControl.dz_4x4_YI[ 1]         = 878;
    qControl.dz_4x4_YI[ 2]         = 820;
    qControl.dz_4x4_YI[ 3]         = 682;
    qControl.dz_4x4_YI[ 4]         = 878;
    qControl.dz_4x4_YI[ 5]         = 820;
    qControl.dz_4x4_YI[ 6]         = 682;
    qControl.dz_4x4_YI[ 7]         = 512;
    qControl.dz_4x4_YI[ 8]         = 820;
    qControl.dz_4x4_YI[ 9]         = 682;
    qControl.dz_4x4_YI[10]         = 512;
    qControl.dz_4x4_YI[11]         = 410;
    qControl.dz_4x4_YI[12]         = 682;
    qControl.dz_4x4_YI[13]         = 512;
    qControl.dz_4x4_YI[14]         = 410;
    qControl.dz_4x4_YI[15]         = 410;
    qControl.dz_4x4_YP[ 0]         = 682;
    qControl.dz_4x4_YP[ 1]         = 586;
    qControl.dz_4x4_YP[ 2]         = 546;
    qControl.dz_4x4_YP[ 3]         = 456;
    qControl.dz_4x4_YP[ 4]         = 586;
    qControl.dz_4x4_YP[ 5]         = 546;
    qControl.dz_4x4_YP[ 6]         = 456;
    qControl.dz_4x4_YP[ 7]         = 342;
    qControl.dz_4x4_YP[ 8]         = 546;
    qControl.dz_4x4_YP[ 9]         = 456;
    qControl.dz_4x4_YP[10]         = 342;
    qControl.dz_4x4_YP[11]         = 292;
    qControl.dz_4x4_YP[12]         = 456;
    qControl.dz_4x4_YP[13]         = 342;
    qControl.dz_4x4_YP[14]         = 292;
    qControl.dz_4x4_YP[15]         = 274;
    qControl.dz_4x4_CI             = 682;
    qControl.dz_4x4_CP             = 342;
    for (size_t i = 0; NUMELEMS(qControl.dz_8x8_YI) > i; ++i)
    {
        qControl.dz_8x8_YI[i] = 682;
    }
    for (size_t i = 0; NUMELEMS(qControl.dz_8x8_YP) > i; ++i)
    {
        qControl.dz_8x8_YP[i] = 342;
    }

    lwenc_h264_me_control_s meControl;
    memset(&meControl, 0, sizeof(meControl));

    meControl.refinement_mode                = 1;
    meControl.refine_on_search_enable        = 1;
    meControl.limit_mv.mv_limit_enable       = 1;
    meControl.limit_mv.left_mvx_int          = -2048;
    meControl.limit_mv.top_mvy_int           = -512;
    meControl.limit_mv.right_mvx_frac        = 3;
    meControl.limit_mv.right_mvx_int         = 2047;
    meControl.limit_mv.bottom_mvy_frac       = 3;
    meControl.limit_mv.bottom_mvy_int        = 511;
    meControl.predsrc.self_temporal_stamp_l0 = 1;
    meControl.predsrc.self_temporal_stamp_l1 = 1;
    meControl.predsrc.self_temporal_search   = 1;
    meControl.predsrc.self_temporal_refine   = 1;
    meControl.predsrc.self_temporal_enable   = 1;
    meControl.predsrc.coloc_stamp_l0         = 2;
    meControl.predsrc.coloc_stamp_l1         = 2;
    meControl.predsrc.coloc_search           = 1;
    meControl.predsrc.coloc_refine           = 1;
    meControl.predsrc.coloc_enable           = 1;
    meControl.predsrc.self_spatial_search    = 1;
    meControl.predsrc.self_spatial_refine    = 1;
    meControl.predsrc.self_spatial_enable    = 1;
    meControl.predsrc.const_mv_stamp_l0      = 1;
    meControl.predsrc.const_mv_stamp_l1      = 1;
    meControl.predsrc.const_mv_search        = 1;
    meControl.predsrc.const_mv_refine        = 1;
    meControl.predsrc.const_mv_enable        = 1;
    meControl.shape1.bitmask[0]              = 0x7f3e1c08;
    meControl.shape1.bitmask[1]              = 0x081c3e3e;
    meControl.shape2.bitmask[0]              = 0xff7e3c18;
    meControl.shape2.bitmask[1]              = 0x183c7eff;
    meControl.fps_mvcost                     = 1;
    meControl.sps_mvcost                     = 1;
    meControl.sps_cost_func                  = 1;
    meControl.vc1_mc_rnd                     = 1;
    meControl.mbc_mb_size                    = 110;

    if (className == LWC5B7_VIDEO_ENCODER || className == LWC4B7_VIDEO_ENCODER)
    {
        meControl.spatial_hint_pattern = 0xf;
        meControl.temporal_hint_pattern = 0x3f;
        meControl.Lw16partDecisionMadeByFPP = 0;
#ifdef MISRA_5_2
        meControl.fbm_op_winner_num_p_frame = 1;
        meControl.fbm_op_winner_num_b_frame_l0 = 1;
        meControl.fbm_op_winner_num_b_frame_l1 = 1;
#else
        meControl.fbm_output_winner_num_p_frame = 1;
        meControl.fbm_output_winner_num_b_frame_l0 = 1;
        meControl.fbm_output_winner_num_b_frame_l1 = 1;
#endif
        meControl.fbm_select_best_lw32_parttype_num = 3;
        meControl.sps_evaluate_merge_cand_num = 5;
        meControl.fps_quad_thresh_hold = 0;
        meControl.external_hint_order = 1;
        meControl.coloc_hint_order = 1;
        meControl.ct_threshold = 0;
        meControl.hint_type0 = 0;
        meControl.hint_type1 = 1;
        meControl.hint_type2 = 2;
        meControl.hint_type3 = 3;
        meControl.hint_type4 = 4;

        mdControl.l0_part_8x4_enable = 1;
        mdControl.l0_part_4x8_enable = 1;
        mdControl.l0_part_4x4_enable = 1;
        mdControl.intra_ssd_cnt_4x4 = 1;
        mdControl.intra_ssd_cnt_8x8 = 1;
        mdControl.intra_ssd_cnt_16x16 = 1;
        mdControl.skip_evaluate_enable = 0;
        mdControl.rdo_level = 1;
        mdControl.tu_search_num = 2;

        picSetup.pic_control.chroma_skip_threshold_4x4 = 8;
    }

    WritePicSetupSurface(0, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);

    picSetup.pic_control.l0[0] = 0;

    for (size_t i = 1; NUMELEMS(picSetup.pic_control.temp_dist_l0) > i; ++i)
    {
        picSetup.pic_control.temp_dist_l0[i] = -1;
    }
    for (size_t i = 0; NUMELEMS(picSetup.pic_control.temp_dist_l1) > i; ++i)
    {
        picSetup.pic_control.temp_dist_l1[i] = -1;
    }

    for (UINT32 refdix1 = 0; refdix1 <= 7; refdix1++)
    {
        for (UINT32 refdix0 = 0; refdix0 <= 7; refdix0++)
        {
            picSetup.pic_control.dist_scale_factor[refdix1][refdix0] =
                (refdix0 == 0) ? 256 : -1;
        }
    }
    picSetup.pic_control.diff_pic_order_cnt_zero[0] = 0xfefefefe;
    picSetup.pic_control.diff_pic_order_cnt_zero[1] = 0xfefefefe;

    picSetup.pic_control.pic_order_cnt_lsb = 1;
    picSetup.pic_control.pic_struct = 2; // 2 = bot/second field
    picSetup.pic_control.pic_type = 0;

    sliceControl.qp_avr = 28;
    sliceControl.num_ref_idx_active_override_flag = 1;

    meControl.vc1_mc_rnd = 0;

    WritePicSetupSurface(1, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);

    picSetup.pic_control.l0[1] = 1;
    picSetup.pic_control.temp_dist_l0[0] = 4;
    picSetup.pic_control.temp_dist_l0[1] = 3;
    for (UINT32 refdix1 = 0; refdix1 <= 7; refdix1++)
    {
        picSetup.pic_control.dist_scale_factor[refdix1][1] = 256;
    }
    picSetup.pic_control.diff_pic_order_cnt_zero[0] = 0xfcfcfcfc;
    picSetup.pic_control.diff_pic_order_cnt_zero[1] = 0xfcfcfcfc;

    picSetup.pic_control.frame_num = 1;
    picSetup.pic_control.pic_order_cnt_lsb = 8;
    picSetup.pic_control.pic_struct = 1;

    sliceControl.num_ref_idx_l0_active_minus1 = 1;

    meControl.vc1_mc_rnd = 1;

    WritePicSetupSurface(2, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);

    picSetup.pic_control.l0[0] = 1;
    picSetup.pic_control.l0[1] = 2;
    picSetup.pic_control.l0[2] = 0;
    picSetup.pic_control.temp_dist_l0[1] = 0;
    picSetup.pic_control.temp_dist_l0[2] = 4;
    for (UINT32 refdix1 = 0; refdix1 <= 7; refdix1++)
    {
        picSetup.pic_control.dist_scale_factor[refdix1][2] = 256;
    }
    picSetup.pic_control.diff_pic_order_cnt_zero[0] = 0xf8f8f8f8;
    picSetup.pic_control.diff_pic_order_cnt_zero[1] = 0xf8f8f8f8;

    picSetup.pic_control.pic_order_cnt_lsb = 9;
    picSetup.pic_control.pic_struct = 2;

    sliceControl.num_ref_idx_l0_active_minus1 = 2;

    meControl.vc1_mc_rnd = 0;

    WritePicSetupSurface(3, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);

    picSetup.pic_control.l0[0] = 0;
    picSetup.pic_control.l0[1] = 1;
    picSetup.pic_control.l0[2] = 2;
    picSetup.pic_control.l0[3] = 3;
    picSetup.pic_control.l1[0] = 2;
    picSetup.pic_control.temp_dist_l0[0] = 1;
    picSetup.pic_control.temp_dist_l0[2] = -3;
    picSetup.pic_control.temp_dist_l0[3] = -4;
    picSetup.pic_control.temp_dist_l1[0] = -3;
    for (UINT32 refdix1 = 0; refdix1 <= 7; refdix1++)
    {
        for (UINT32 refdix0 = 0; refdix0 <= 7; refdix0++)
        {
            picSetup.pic_control.dist_scale_factor[refdix1][refdix0] = -1;
        }
    }
    picSetup.pic_control.dist_scale_factor[0][0] = 64;
    picSetup.pic_control.dist_scale_factor[0][1] = 37;
    picSetup.pic_control.dist_scale_factor[0][2] = -1;
    picSetup.pic_control.dist_scale_factor[0][3] = 1023;

    picSetup.pic_control.diff_pic_order_cnt_zero[0] = 0xf0f0f004;
    picSetup.pic_control.diff_pic_order_cnt_zero[1] = 0xf0f0f0f0;

    picSetup.pic_control.frame_num = 2;
    picSetup.pic_control.pic_order_cnt_lsb = 2;
    picSetup.pic_control.pic_struct   = 1;
    picSetup.pic_control.pic_type     = 1;
    picSetup.pic_control.ref_pic_flag = 0;

    sliceControl.qp_avr = 31;
    sliceControl.num_ref_idx_active_override_flag = 0;
    sliceControl.num_ref_idx_l0_active_minus1 = 3;

    WritePicSetupSurface(4, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);

    picSetup.pic_control.l0[0] = 1;
    picSetup.pic_control.l0[1] = 0;
    picSetup.pic_control.l0[2] = 3;
    picSetup.pic_control.l0[3] = 2;
    picSetup.pic_control.l1[0] = 3;
    picSetup.pic_control.temp_dist_l0[1] = 1;
    picSetup.pic_control.temp_dist_l0[3] = -3;
    picSetup.pic_control.dist_scale_factor[0][1] = 85;
    picSetup.pic_control.dist_scale_factor[0][3] = -1024;

    picSetup.pic_control.pic_order_cnt_lsb = 3;
    picSetup.pic_control.pic_struct   = 2;

    WritePicSetupSurface(5, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);

    picSetup.pic_control.l0[0] = 0;
    picSetup.pic_control.l0[1] = 1;
    picSetup.pic_control.l0[2] = 2;
    picSetup.pic_control.l0[3] = 3;
    picSetup.pic_control.l1[0] = 2;
    picSetup.pic_control.temp_dist_l0[0] = 2;
    picSetup.pic_control.temp_dist_l0[2] = -2;
    picSetup.pic_control.temp_dist_l1[0] = -2;
    picSetup.pic_control.dist_scale_factor[0][0] = 128;
    picSetup.pic_control.dist_scale_factor[0][1] = 110;
    picSetup.pic_control.dist_scale_factor[0][3] = 1023;

    picSetup.pic_control.pic_order_cnt_lsb = 4;
    picSetup.pic_control.pic_struct = 1;

    WritePicSetupSurface(6, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);

    picSetup.pic_control.l0[0] = 1;
    picSetup.pic_control.l0[1] = 0;
    picSetup.pic_control.l0[2] = 3;
    picSetup.pic_control.l0[3] = 2;
    picSetup.pic_control.l1[0] = 3;
    picSetup.pic_control.temp_dist_l0[1] = 2;
    picSetup.pic_control.temp_dist_l0[3] = -2;
    picSetup.pic_control.dist_scale_factor[0][1] = 142;
    picSetup.pic_control.dist_scale_factor[0][3] = -768;

    picSetup.pic_control.pic_order_cnt_lsb = 5;
    picSetup.pic_control.pic_struct = 2;

    WritePicSetupSurface(7, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);

    lwenc_persistent_state_s rcProcess;
    memset(&rcProcess, 0, sizeof(rcProcess));

    rcProcess.nal_cpb_fullness = 1641856;
    rcProcess.Wpbi[0]          = 128;
    rcProcess.Wpbi[1]          = 121;
    rcProcess.Wpbi[2]          = 380;
    rcProcess.Whist[0][0]      = 84;
    rcProcess.Whist[0][1]      = 66;
    rcProcess.Whist[0][2]      = 171;
    rcProcess.Whist[1][0]      = 28;
    rcProcess.Whist[1][1]      = 34;
    rcProcess.Whist[1][2]      = 26;
    rcProcess.Whist[2][0]      = 380;
    rcProcess.Whist[2][1]      = 121;
    rcProcess.Whist[2][2]      = 121;
    rcProcess.np               = 8;
    rcProcess.nb               = 88;
    rcProcess.Rmean            = 500759;
    rcProcess.average_mvx      = 3;

    Platform::VirtualWr(ioRcProcess->GetAddress(), &rcProcess, sizeof(rcProcess));

    H264::SeqParameterSet sps;
    sps.profile_idc                       = picSetup.sps_data.profile_idc;
    sps.level_idc                         = picSetup.sps_data.level_idc;
    sps.chroma_format_idc                 = picSetup.sps_data.chroma_format_idc;
    sps.pic_order_cnt_type                = picSetup.sps_data.pic_order_cnt_type;
    sps.log2_max_frame_num_minus4         = picSetup.sps_data.log2_max_frame_num_minus4;
    sps.log2_max_pic_order_cnt_lsb_minus4 = picSetup.sps_data.log2_max_pic_order_cnt_lsb_minus4;
    sps.pic_width_in_mbs_minus1           = (picSetup.input_cfg.frame_width_minus1 + 1) / 16 - 1;
    sps.pic_height_in_map_units_minus1    = (picSetup.input_cfg.frame_height_minus1 + 1) / 16 - 1;
    sps.frame_mbs_only_flag               = picSetup.sps_data.frame_mbs_only;
    sps.max_num_ref_frames                = MAX_REF_FRAMES;
    sps.direct_8x8_inference_flag         = true;
    sps.frame_cropping_flag               = true;
    sps.frame_crop_bottom_offset          = 4;
    sps.UnsetEmpty();
    spsNalus[H264_HP_CABAC].CreateDataFromSPS(sps);

    SeqSetSrcImpl spsSrc(sps);
    H264::PicParameterSet pps(&spsSrc);
    pps.entropy_coding_mode_flag               = picSetup.pps_data.entropy_coding_mode_flag;
    pps.num_ref_idx_l0_default_active_minus1   = picSetup.pps_data.num_ref_idx_l0_active_minus1;
    pps.deblocking_filter_control_present_flag = picSetup.pps_data.deblocking_filter_control_present_flag;
    pps.transform_8x8_mode_flag                = picSetup.pps_data.transform_8x8_mode_flag;
    pps.UnsetEmpty();
    ppsNalus[H264_HP_CABAC].CreateDataFromPPS(pps);

    return OK;
}

static RC PrepareControlStructuresH264MVCHighprofileCABAC
(
    Surface2D *picSetupSurface,
    UINT32 className,
    UINT32 outBitstreamBufferSize
)
{
    RC rc;
    lwenc_h264_drv_pic_setup_s picSetup;
    memset(&picSetup, 0, sizeof(picSetup));

    SetMagicValue(className, &picSetup.magic);

    CHECK_RC(PrepareSurfaceCfg(&picSetup.refpic_cfg, className, 176, 144, 8,
                               SURFTYPE_REFPIC, SURFFMT_TILED,
                               SURFCOLOR_YUV420, false));
    CHECK_RC(PrepareSurfaceCfg(&picSetup.input_cfg, className, 176, 144, 8,
                               SURFTYPE_INPUT, SURFFMT_BL2,
                               SURFCOLOR_YUV420, false));
    CHECK_RC(PrepareSurfaceCfg(&picSetup.outputpic_cfg, className, 176, 144, 8,
                               SURFTYPE_REFPIC, SURFFMT_TILED,
                               SURFCOLOR_YUV420, false));

    picSetup.sps_data.profile_idc                        = 128;
    picSetup.sps_data.level_idc                          = 41;
    picSetup.sps_data.chroma_format_idc                  = 1;
    picSetup.sps_data.log2_max_frame_num_minus4          = 8 - 4;
    picSetup.sps_data.log2_max_pic_order_cnt_lsb_minus4  = 8 - 4;
    picSetup.sps_data.frame_mbs_only                     = 1;
    picSetup.sps_data.stereo_mvc_enable                  = 1;

    picSetup.pps_data.entropy_coding_mode_flag               = 1;
    picSetup.pps_data.num_ref_idx_l0_active_minus1           = 4 - 1;
    picSetup.pps_data.deblocking_filter_control_present_flag = 1;
    picSetup.pps_data.transform_8x8_mode_flag                = 1;

    picSetup.rate_control.hrd_type     = 2;
    picSetup.rate_control.QP[0]        = 28;
    picSetup.rate_control.QP[1]        = 31;
    picSetup.rate_control.QP[2]        = 25;
    picSetup.rate_control.minQP[0]     = 1;
    picSetup.rate_control.minQP[1]     = 1;
    picSetup.rate_control.minQP[2]     = 1;
    picSetup.rate_control.maxQP[0]     = 51;
    picSetup.rate_control.maxQP[1]     = 51;
    picSetup.rate_control.maxQP[2]     = 51;
    picSetup.rate_control.maxQPD       = 6;
    picSetup.rate_control.baseQPD      = 3;
    picSetup.rate_control.rhopbi[0]    = 256;
    picSetup.rate_control.rhopbi[1]    = 256;
    picSetup.rate_control.rhopbi[2]    = 256;
    picSetup.rate_control.framerate    = 7680;
    picSetup.rate_control.buffersize   = 2048000;
    picSetup.rate_control.nal_cpb_size = 2048000;
    picSetup.rate_control.nal_bitrate  = 1024000;
    picSetup.rate_control.gop_length   = 10;
    picSetup.rate_control.rc_class     = 1;

    for (UINT32 idx = 0; idx <= 7; idx++)
    {
        picSetup.pic_control.l0[idx] = -2;
        picSetup.pic_control.l1[idx] = -2;
    }
    picSetup.pic_control.slice_control_offset   = sliceControlOffset;
    picSetup.pic_control.me_control_offset      = meControlOffset;
    picSetup.pic_control.md_control_offset      = mdControlOffset;
    picSetup.pic_control.q_control_offset       = quantControlOffset;
    picSetup.pic_control.hist_buf_size          = 14080;
    picSetup.pic_control.bitstream_buf_size     = outBitstreamBufferSize;
    picSetup.pic_control.pic_type               = 3;
    picSetup.pic_control.ref_pic_flag           = 1;
    picSetup.pic_control.base_view              = 1;
    picSetup.pic_control.anchor_pic_flag        = 1;
    picSetup.pic_control.inter_view_flag        = 1;
    picSetup.pic_control.lwr_interview_ref_pic  = -2;
    picSetup.pic_control.prev_interview_ref_pic = -2;

    lwenc_h264_slice_control_s sliceControl;
    memset(&sliceControl, 0, sizeof(sliceControl));

    sliceControl.num_mb = 99;
    sliceControl.qp_avr = 25;

    lwenc_h264_md_control_s mdControl;
    memset(&mdControl, 0, sizeof(mdControl));

    mdControl.intra_luma4x4_mode_enable       = 511;
    mdControl.intra_luma8x8_mode_enable       = 511;
    mdControl.intra_luma16x16_mode_enable     = 15;
    mdControl.intra_chroma_mode_enable        = 15;
    mdControl.l0_part_16x16_enable            = 1;
    mdControl.l0_part_16x8_enable             = 1;
    mdControl.l0_part_8x16_enable             = 1;
    mdControl.l0_part_8x8_enable              = 1;
    mdControl.l1_part_16x16_enable            = 1;
    mdControl.l1_part_16x8_enable             = 1;
    mdControl.l1_part_8x16_enable             = 1;
    mdControl.l1_part_8x8_enable              = 1;
    mdControl.bi_part_16x16_enable            = 1;
    mdControl.bi_part_16x8_enable             = 1;
    mdControl.bi_part_8x16_enable             = 1;
    mdControl.bi_part_8x8_enable              = 1;
    mdControl.bdirect_mode                    = 1;
    mdControl.bskip_enable                    = 1;
    mdControl.pskip_enable                    = 1;
    mdControl.intra_nxn_bias_multiplier       = 24;
    mdControl.intra_most_prob_bias_multiplier = 4;
    mdControl.mv_cost_enable                  = 1;
    mdControl.ip_search_mode                  = 7;

    lwenc_h264_quant_control_s qControl;
    memset(&qControl, 0, sizeof(qControl));

    qControl.qpp_run_vector_4x4    = 1387;
    qControl.qpp_run_vector_8x8[0] = 43775;
    qControl.qpp_run_vector_8x8[1] = 65450;
    qControl.qpp_run_vector_8x8[2] = 15;
    qControl.qpp_luma8x8_cost      = 15;
    qControl.qpp_luma16x16_cost    = 15;
    qControl.qpp_chroma_cost       = 15;
    qControl.dz_4x4_YI[ 0]         = 1023;
    qControl.dz_4x4_YI[ 1]         = 878;
    qControl.dz_4x4_YI[ 2]         = 820;
    qControl.dz_4x4_YI[ 3]         = 682;
    qControl.dz_4x4_YI[ 4]         = 878;
    qControl.dz_4x4_YI[ 5]         = 820;
    qControl.dz_4x4_YI[ 6]         = 682;
    qControl.dz_4x4_YI[ 7]         = 512;
    qControl.dz_4x4_YI[ 8]         = 820;
    qControl.dz_4x4_YI[ 9]         = 682;
    qControl.dz_4x4_YI[10]         = 512;
    qControl.dz_4x4_YI[11]         = 410;
    qControl.dz_4x4_YI[12]         = 682;
    qControl.dz_4x4_YI[13]         = 512;
    qControl.dz_4x4_YI[14]         = 410;
    qControl.dz_4x4_YI[15]         = 410;
    qControl.dz_4x4_YP[ 0]         = 682;
    qControl.dz_4x4_YP[ 1]         = 586;
    qControl.dz_4x4_YP[ 2]         = 546;
    qControl.dz_4x4_YP[ 3]         = 456;
    qControl.dz_4x4_YP[ 4]         = 586;
    qControl.dz_4x4_YP[ 5]         = 546;
    qControl.dz_4x4_YP[ 6]         = 456;
    qControl.dz_4x4_YP[ 7]         = 342;
    qControl.dz_4x4_YP[ 8]         = 546;
    qControl.dz_4x4_YP[ 9]         = 456;
    qControl.dz_4x4_YP[10]         = 342;
    qControl.dz_4x4_YP[11]         = 292;
    qControl.dz_4x4_YP[12]         = 456;
    qControl.dz_4x4_YP[13]         = 342;
    qControl.dz_4x4_YP[14]         = 292;
    qControl.dz_4x4_YP[15]         = 274;
    qControl.dz_4x4_CI             = 682;
    qControl.dz_4x4_CP             = 342;

    for (UINT32 idx = 0; idx <= 0xf; idx++)
    {
        qControl.dz_8x8_YI[idx] = 682;
        qControl.dz_8x8_YP[idx] = 342;
    }

    lwenc_h264_me_control_s meControl;
    memset(&meControl, 0, sizeof(meControl));

    meControl.refinement_mode               = 1;
    meControl.refine_on_search_enable       = 1;
    meControl.fps_mvcost                    = 1;
    meControl.sps_mvcost                    = 1;
    meControl.sps_cost_func                 = 1;
    meControl.vc1_mc_rnd                    = 1;
    meControl.limit_mv.mv_limit_enable      = 1;
    meControl.limit_mv.left_mvx_int         = -2048;
    meControl.limit_mv.top_mvy_int          = -512;
    meControl.limit_mv.right_mvx_frac       = 3;
    meControl.limit_mv.right_mvx_int        = 2047;
    meControl.limit_mv.bottom_mvy_frac      = 3;
    meControl.limit_mv.bottom_mvy_int       = 511;
    meControl.predsrc.self_temporal_search  = 1;
    meControl.predsrc.self_temporal_refine  = 1;
    meControl.predsrc.self_temporal_enable  = 1;
    meControl.predsrc.coloc_stamp_l0        = 2;
    meControl.predsrc.coloc_stamp_l1        = 2;
    meControl.predsrc.coloc_search          = 1;
    meControl.predsrc.coloc_refine          = 1;
    meControl.predsrc.coloc_enable          = 1;
    meControl.predsrc.self_spatial_stamp_l0 = 1;
    meControl.predsrc.self_spatial_stamp_l1 = 1;
    meControl.predsrc.self_spatial_search   = 1;
    meControl.predsrc.self_spatial_refine   = 1;
    meControl.predsrc.self_spatial_enable   = 1;
    meControl.predsrc.const_mv_search       = 1;
    meControl.predsrc.const_mv_refine       = 1;
    meControl.predsrc.const_mv_enable       = 1;
    meControl.shape0.bitmask[0]             = 0x7e7e7e7e;
    meControl.shape0.bitmask[1]             = 0x7e7e7e7e;
    meControl.shape1.bitmask[0]             = 0x7f3e1c08;
    meControl.shape1.bitmask[1]             = 0x081c3e3e;
    meControl.shape2.bitmask[0]             = 0xff7e3c18;
    meControl.shape2.bitmask[1]             = 0x183c7eff;
    meControl.mbc_mb_size                   = 110;

    if (className == LWC5B7_VIDEO_ENCODER || className == LWC4B7_VIDEO_ENCODER)
    {
        meControl.spatial_hint_pattern = 0xf;
        meControl.temporal_hint_pattern = 0x3f;
        meControl.Lw16partDecisionMadeByFPP = 0;
#ifdef MISRA_5_2
        meControl.fbm_op_winner_num_p_frame = 1;
        meControl.fbm_op_winner_num_b_frame_l0 = 1;
        meControl.fbm_op_winner_num_b_frame_l1 = 1;
#else
        meControl.fbm_output_winner_num_p_frame = 1;
        meControl.fbm_output_winner_num_b_frame_l0 = 1;
        meControl.fbm_output_winner_num_b_frame_l1 = 1;
#endif
        meControl.fbm_select_best_lw32_parttype_num = 3;
        meControl.sps_evaluate_merge_cand_num = 5;
        meControl.fps_quad_thresh_hold = 0;
        meControl.external_hint_order = 1;
        meControl.coloc_hint_order = 1;
        meControl.ct_threshold = 0;
        meControl.hint_type0 = 0;
        meControl.hint_type1 = 1;
        meControl.hint_type2 = 2;
        meControl.hint_type3 = 3;
        meControl.hint_type4 = 4;

        mdControl.l0_part_8x4_enable = 1;
        mdControl.l0_part_4x8_enable = 1;
        mdControl.l0_part_4x4_enable = 1;
        mdControl.intra_ssd_cnt_4x4 = 1;
        mdControl.intra_ssd_cnt_8x8 = 1;
        mdControl.intra_ssd_cnt_16x16 = 1;
        mdControl.skip_evaluate_enable = 0;
        mdControl.rdo_level = 1;
        mdControl.tu_search_num = 2;

        picSetup.pic_control.chroma_skip_threshold_4x4 = 8;
    }

    WritePicSetupSurface(0, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);

    picSetup.pps_data.pic_param_set_id = 1;

    picSetup.pic_control.l0[0] = 0;

    for (UINT32 idx = 0; idx <= 7; idx++)
    {
        picSetup.pic_control.temp_dist_l0[idx] = -1;
        picSetup.pic_control.temp_dist_l1[idx] = -1;
    }
    picSetup.pic_control.temp_dist_l0[0] = 0;

    for (UINT32 refdix1 = 0; refdix1 <= 7; refdix1++)
    {
        for (UINT32 refdix0 = 0; refdix0 <= 7; refdix0++)
        {
            picSetup.pic_control.dist_scale_factor[refdix1][refdix0] = -1;
        }
    }
    picSetup.pic_control.diff_pic_order_cnt_zero[0] = 0xfefefefe;
    picSetup.pic_control.diff_pic_order_cnt_zero[1] = 0xfefefefe;

    picSetup.pic_control.pic_type = 0;
    picSetup.pic_control.base_view = 0,
    picSetup.pic_control.view_id = 1,
    picSetup.pic_control.inter_view_flag = 0;

    sliceControl.qp_avr = 28;
    sliceControl.num_ref_idx_active_override_flag = 1;

    meControl.vc1_mc_rnd = 0;

    WritePicSetupSurface(1, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);

    picSetup.pps_data.pic_param_set_id = 0;

    picSetup.pic_control.temp_dist_l0[0] = 3;
    for (UINT32 refdix1 = 0; refdix1 <= 7; refdix1++)
    {
        picSetup.pic_control.dist_scale_factor[refdix1][0] = 256;
    }

    picSetup.pic_control.frame_num = 1;
    picSetup.pic_control.pic_order_cnt_lsb = 6;
    picSetup.pic_control.base_view = 1,
    picSetup.pic_control.view_id = 0,
    picSetup.pic_control.anchor_pic_flag = 0,
    picSetup.pic_control.inter_view_flag = 1;

    meControl.vc1_mc_rnd = 1;

    WritePicSetupSurface(2, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);

    picSetup.pps_data.pic_param_set_id = 1;

    picSetup.pic_control.l0[0] = 2;
    picSetup.pic_control.l0[1] = 4;
    picSetup.pic_control.temp_dist_l0[1] = 0;

    picSetup.pic_control.diff_pic_order_cnt_zero[0] = 0xfcfcfcfc;
    picSetup.pic_control.diff_pic_order_cnt_zero[1] = 0xfcfcfcfc;

    picSetup.pic_control.base_view = 0,
    picSetup.pic_control.view_id = 1,
    picSetup.pic_control.inter_view_flag = 0;
    picSetup.pic_control.lwr_interview_ref_pic = 4;
    picSetup.pic_control.prev_interview_ref_pic = 0;

    sliceControl.num_ref_idx_l0_active_minus1 = 1;

    WritePicSetupSurface(3, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);

    picSetup.pps_data.pic_param_set_id = 0;

    picSetup.pic_control.l0[0] = 0;
    picSetup.pic_control.l1[0] = 4;
    picSetup.pic_control.temp_dist_l0[0] = 1;
    picSetup.pic_control.temp_dist_l0[1] = -2;
    picSetup.pic_control.temp_dist_l1[0] = -2;
    for (UINT32 refdix1 = 0; refdix1 <= 7; refdix1++)
    {
        for (UINT32 refdix0 = 0; refdix0 <= 7; refdix0++)
        {
            picSetup.pic_control.dist_scale_factor[refdix1][refdix0] = -1;
        }
    }
    picSetup.pic_control.dist_scale_factor[0][0] = 85;

    picSetup.pic_control.diff_pic_order_cnt_zero[0] = 0xfcfcfc02;
    picSetup.pic_control.diff_pic_order_cnt_zero[1] = 0xfcfcfcfc;

    picSetup.pic_control.frame_num = 2;
    picSetup.pic_control.pic_order_cnt_lsb = 2;
    picSetup.pic_control.pic_type = 1;
    picSetup.pic_control.ref_pic_flag = 0,
    picSetup.pic_control.base_view = 1,
    picSetup.pic_control.view_id = 0,
    picSetup.pic_control.lwr_interview_ref_pic = -2;
    picSetup.pic_control.prev_interview_ref_pic = -2;

    sliceControl.qp_avr = 31;

    WritePicSetupSurface(4, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);

    picSetup.pps_data.pic_param_set_id = 1;

    picSetup.pic_control.l0[0] = 2;
    picSetup.pic_control.l0[1] = 6;
    picSetup.pic_control.l1[0] = 6;

    picSetup.pic_control.base_view = 0,
    picSetup.pic_control.view_id = 1,

    WritePicSetupSurface(5, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);

    picSetup.pps_data.pic_param_set_id = 0;

    picSetup.pic_control.l0[0] = 0;
    picSetup.pic_control.l0[1] = 4;
    picSetup.pic_control.l1[0] = 4;
    picSetup.pic_control.temp_dist_l0[0] = 2;
    picSetup.pic_control.temp_dist_l0[1] = -1;
    picSetup.pic_control.temp_dist_l1[0] = -1;
    picSetup.pic_control.dist_scale_factor[0][0] = 171;

    picSetup.pic_control.pic_order_cnt_lsb = 4;
    picSetup.pic_control.base_view = 1,
    picSetup.pic_control.view_id = 0,

    WritePicSetupSurface(6, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);

    picSetup.pps_data.pic_param_set_id = 1;

    picSetup.pic_control.l0[0] = 2;
    picSetup.pic_control.l0[1] = 6;
    picSetup.pic_control.l1[0] = 6;

    picSetup.pic_control.base_view = 0,
    picSetup.pic_control.view_id = 1,

    WritePicSetupSurface(7, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);

    H264::SeqParameterSet sps0;
    sps0.profile_idc                       = H264::FREXT_HP;
    sps0.level_idc                         = picSetup.sps_data.level_idc;
    sps0.seq_parameter_set_id              = 0;
    sps0.chroma_format_idc                 = picSetup.sps_data.chroma_format_idc;
    sps0.pic_order_cnt_type                = picSetup.sps_data.pic_order_cnt_type;
    sps0.log2_max_frame_num_minus4         = picSetup.sps_data.log2_max_frame_num_minus4;
    sps0.log2_max_pic_order_cnt_lsb_minus4 = picSetup.sps_data.log2_max_pic_order_cnt_lsb_minus4;
    sps0.pic_width_in_mbs_minus1           = (picSetup.input_cfg.frame_width_minus1 + 1) / 16 - 1;
    sps0.pic_height_in_map_units_minus1    = (picSetup.input_cfg.frame_height_minus1 + 1) / 16 - 1;
    sps0.frame_mbs_only_flag               = picSetup.sps_data.frame_mbs_only;
    sps0.max_num_ref_frames                = MAX_REF_FRAMES;
    sps0.direct_8x8_inference_flag         = true;
    if (!sps0.frame_mbs_only_flag)
    {
        // See H.264 7.4.2.1.1
        sps0.direct_8x8_inference_flag = true;
    }
    sps0.UnsetEmpty();
    spsNalus[H264_MVC_HP_CABAC].CreateDataFromSPS(sps0);

    SeqSetSrcImpl spsSrc0(sps0);
    H264::PicParameterSet pps0(&spsSrc0);
    pps0.seq_parameter_set_id                   = 0;
    pps0.entropy_coding_mode_flag               = picSetup.pps_data.entropy_coding_mode_flag;
    pps0.num_ref_idx_l0_default_active_minus1   = picSetup.pps_data.num_ref_idx_l0_active_minus1;
    pps0.deblocking_filter_control_present_flag = picSetup.pps_data.deblocking_filter_control_present_flag;
    pps0.transform_8x8_mode_flag                = picSetup.pps_data.transform_8x8_mode_flag;
    pps0.UnsetEmpty();
    ppsNalus[H264_MVC_HP_CABAC].CreateDataFromPPS(pps0);

    H264::SubsetSeqParameterSet subsetSps;
    H264::SeqParameterSet &sps1 = subsetSps.seq_parameter_set_data;
    sps1.profile_idc                       = H264::STEREO_HIGH;
    sps1.level_idc                         = picSetup.sps_data.level_idc;
    sps1.seq_parameter_set_id              = 1;
    sps1.chroma_format_idc                 = picSetup.sps_data.chroma_format_idc;
    sps1.pic_order_cnt_type                = picSetup.sps_data.pic_order_cnt_type;
    sps1.log2_max_frame_num_minus4         = picSetup.sps_data.log2_max_frame_num_minus4;
    sps1.log2_max_pic_order_cnt_lsb_minus4 = picSetup.sps_data.log2_max_pic_order_cnt_lsb_minus4;
    sps1.pic_width_in_mbs_minus1           = (picSetup.input_cfg.frame_width_minus1 + 1) / 16 - 1;
    sps1.pic_height_in_map_units_minus1    = (picSetup.input_cfg.frame_height_minus1 + 1) / 16 - 1;
    sps1.frame_mbs_only_flag               = picSetup.sps_data.frame_mbs_only;
    sps1.max_num_ref_frames                = MAX_REF_FRAMES;
    sps1.direct_8x8_inference_flag         = true;
    H264::SeqParameterSetMvcExtension &spsMvcExt = subsetSps.seq_parameter_set_mvc_extension;
    spsMvcExt.view_id.resize(2);
    spsMvcExt.view_id[0] = 0;
    spsMvcExt.view_id[1] = 1;
    spsMvcExt.anchor_refs.resize(2);
    spsMvcExt.anchor_refs[1].anchor_ref_l0.resize(1);
    spsMvcExt.anchor_refs[1].anchor_ref_l0[0] = 0;
    spsMvcExt.non_anchor_refs.resize(2);
    spsMvcExt.non_anchor_refs[1].non_anchor_ref_l0.resize(1);
    spsMvcExt.non_anchor_refs[1].non_anchor_ref_l0[0] = 0;
    spsMvcExt.level_values_signalled.resize(1);
    spsMvcExt.level_values_signalled[0].level_idc = 0;
    spsMvcExt.level_values_signalled[0].applicable_op.resize(1);
    spsMvcExt.level_values_signalled[0].applicable_op[0].applicable_op_temporal_id = 0;
    spsMvcExt.level_values_signalled[0].applicable_op[0].applicable_op_target_view_id.resize(1);
    spsMvcExt.level_values_signalled[0].applicable_op[0].applicable_op_target_view_id[0] = 0;
    spsMvcExt.level_values_signalled[0].applicable_op[0].applicable_op_num_views_minus1 = 0;
    subsetSpsNalu.CreateFromSubsetSPS(subsetSps);

    SeqSetSrcImpl spsSrc1(sps1);
    H264::PicParameterSet pps1(&spsSrc1);
    pps1.seq_parameter_set_id                   = 1;
    pps1.pic_parameter_set_id                   = 1;
    pps1.entropy_coding_mode_flag               = picSetup.pps_data.entropy_coding_mode_flag;
    pps1.num_ref_idx_l0_default_active_minus1   = picSetup.pps_data.num_ref_idx_l0_active_minus1;
    pps1.deblocking_filter_control_present_flag = picSetup.pps_data.deblocking_filter_control_present_flag;
    pps1.transform_8x8_mode_flag                = picSetup.pps_data.transform_8x8_mode_flag;
    pps1.UnsetEmpty();
    subsetPpsNalu.CreateDataFromPPS(pps1);

    return OK;
}

static RC PrepareControlStructuresH264HDHighprofileCABAC
(
    Surface2D *picSetupSurface,
    Surface2D *ioRcProcess,
    UINT32 className,
    UINT32 outBitstreamBufferSize
)
{
    RC rc;
    lwenc_h264_drv_pic_setup_s picSetup;
    memset(&picSetup, 0, sizeof(picSetup));

    const UINT32 width  = 1920;
    const UINT32 height = 1024;

    static_assert(width <= MAX_WIDTH, "");
    static_assert(height <= MAX_HEIGHT, "");

    SetMagicValue(className, &picSetup.magic);

    CHECK_RC(PrepareSurfaceCfg(&picSetup.refpic_cfg, className, width, height, 8,
                               SURFTYPE_REFPIC, SURFFMT_TILED,
                               SURFCOLOR_YUV420, false));
    CHECK_RC(PrepareSurfaceCfg(&picSetup.input_cfg, className, width, height, 8,
                               SURFTYPE_INPUT, SURFFMT_BL2,
                               SURFCOLOR_YUV420, false));
    CHECK_RC(PrepareSurfaceCfg(&picSetup.outputpic_cfg, className,
                               width, height, 8,
                               SURFTYPE_REFPIC, SURFFMT_TILED,
                               SURFCOLOR_YUV420, false));

    picSetup.sps_data.profile_idc                        = 100;
    picSetup.sps_data.level_idc                          = 41;
    picSetup.sps_data.chroma_format_idc                  = 1;
    picSetup.sps_data.log2_max_frame_num_minus4          = 8 - 4;
    picSetup.sps_data.log2_max_pic_order_cnt_lsb_minus4  = 8 - 4;
    picSetup.sps_data.frame_mbs_only                     = 1;

    picSetup.pps_data.entropy_coding_mode_flag               = 1;
    picSetup.pps_data.num_ref_idx_l0_active_minus1           = 3 - 1;
    picSetup.pps_data.deblocking_filter_control_present_flag = 1;
    picSetup.pps_data.transform_8x8_mode_flag                = 1;
    picSetup.rate_control.hrd_type     = 2;
    picSetup.rate_control.QP[0]        = 43;
    picSetup.rate_control.QP[1]        = 46;
    picSetup.rate_control.QP[2]        = 40;
    picSetup.rate_control.minQP[0]     = 1;
    picSetup.rate_control.minQP[1]     = 1;
    picSetup.rate_control.minQP[2]     = 1;
    picSetup.rate_control.maxQP[0]     = 51;
    picSetup.rate_control.maxQP[1]     = 51;
    picSetup.rate_control.maxQP[2]     = 51;
    picSetup.rate_control.maxQPD       = 6;
    picSetup.rate_control.baseQPD      = 3;
    picSetup.rate_control.rhopbi[0]    = 275;
    picSetup.rate_control.rhopbi[1]    = 294;
    picSetup.rate_control.rhopbi[2]    = 256;
    picSetup.rate_control.framerate    = 7424;
    picSetup.rate_control.buffersize   = 4096000;
    picSetup.rate_control.nal_cpb_size = 4096000;
    picSetup.rate_control.nal_bitrate  = 2048000;
    picSetup.rate_control.gop_length   = 15;
    picSetup.rate_control.Np           = 4;
    picSetup.rate_control.Bmin         = 100;
    picSetup.rate_control.Ravg         = 8;
    picSetup.rate_control.R            = 8;
    picSetup.rate_control.rc_class     = 1;

    for (UINT32 idx = 0; idx <= 7; idx++)
    {
        picSetup.pic_control.l0[idx] = -2;
        picSetup.pic_control.l1[idx] = -2;
    }
    picSetup.pic_control.num_forced_slices_minus1 = 9;
    picSetup.pic_control.slice_control_offset     = sliceControlOffset;
    picSetup.pic_control.me_control_offset        = meControlOffset;
    picSetup.pic_control.md_control_offset        = mdControlOffset;
    picSetup.pic_control.q_control_offset         = quantControlOffset;
    picSetup.pic_control.hist_buf_size            = HIST_BUF_SIZE(width, height);
    picSetup.pic_control.bitstream_buf_size       = outBitstreamBufferSize;
    picSetup.pic_control.pic_type                 = 3;
    picSetup.pic_control.ref_pic_flag             = 1;

    size_t numSlices = picSetup.pic_control.num_forced_slices_minus1 + 1;
    MASSERT(numSlices <= maxNumForcedSlices);
    vector<lwenc_h264_slice_control_s> sliceControl(numSlices);
    for (size_t i = 0; sliceControl.size() > i; ++i)
    {
        memset(&sliceControl[i], 0, sizeof(lwenc_h264_slice_control_s));
    }

    sliceControl[0].num_mb = 720;
    sliceControl[1].num_mb = 840;
    sliceControl[2].num_mb = 840;
    sliceControl[3].num_mb = 840;
    sliceControl[4].num_mb = 840;
    sliceControl[5].num_mb = 720;
    sliceControl[6].num_mb = 840;
    sliceControl[7].num_mb = 840;
    sliceControl[8].num_mb = 840;
    sliceControl[9].num_mb = 840;

    lwenc_h264_md_control_s mdControl;
    memset(&mdControl, 0, sizeof(mdControl));

    mdControl.intra_luma4x4_mode_enable       = 511;
    mdControl.intra_luma8x8_mode_enable       = 511;
    mdControl.intra_luma16x16_mode_enable     = 15;
    mdControl.intra_chroma_mode_enable        = 15;
    mdControl.l0_part_16x16_enable            = 1;
    mdControl.l0_part_16x8_enable             = 1;
    mdControl.l0_part_8x16_enable             = 1;
    mdControl.l0_part_8x8_enable              = 1;
    mdControl.l1_part_16x16_enable            = 1;
    mdControl.l1_part_16x8_enable             = 1;
    mdControl.l1_part_8x16_enable             = 1;
    mdControl.l1_part_8x8_enable              = 1;
    mdControl.bi_part_16x16_enable            = 1;
    mdControl.bi_part_16x8_enable             = 1;
    mdControl.bi_part_8x16_enable             = 1;
    mdControl.bi_part_8x8_enable              = 1;
    mdControl.bdirect_mode                    = 2;
    mdControl.bskip_enable                    = 1;
    mdControl.pskip_enable                    = 1;
    mdControl.intra_nxn_bias_multiplier       = 24;
    mdControl.intra_most_prob_bias_multiplier = 4;
    mdControl.mv_cost_enable                  = 1;
    mdControl.early_intra_mode_control        = 1;
    mdControl.early_intra_mode_type_16x16dc   = 1;
    mdControl.early_intra_mode_type_16x16h    = 1;
    mdControl.early_intra_mode_type_16x16v    = 1;
    mdControl.ip_search_mode                  = 7;

    lwenc_h264_quant_control_s qControl;
    memset(&qControl, 0, sizeof(qControl));

    qControl.qpp_run_vector_4x4    = 1387;
    qControl.qpp_run_vector_8x8[0] = 43775;
    qControl.qpp_run_vector_8x8[1] = 65450;
    qControl.qpp_run_vector_8x8[2] = 15;
    qControl.qpp_luma8x8_cost      = 15;
    qControl.qpp_luma16x16_cost    = 15;
    qControl.qpp_chroma_cost       = 15;
    qControl.dz_4x4_YI[ 0]         = 1023;
    qControl.dz_4x4_YI[ 1]         = 878;
    qControl.dz_4x4_YI[ 2]         = 820;
    qControl.dz_4x4_YI[ 3]         = 682;
    qControl.dz_4x4_YI[ 4]         = 878;
    qControl.dz_4x4_YI[ 5]         = 820;
    qControl.dz_4x4_YI[ 6]         = 682;
    qControl.dz_4x4_YI[ 7]         = 512;
    qControl.dz_4x4_YI[ 8]         = 820;
    qControl.dz_4x4_YI[ 9]         = 682;
    qControl.dz_4x4_YI[10]         = 512;
    qControl.dz_4x4_YI[11]         = 410;
    qControl.dz_4x4_YI[12]         = 682;
    qControl.dz_4x4_YI[13]         = 512;
    qControl.dz_4x4_YI[14]         = 410;
    qControl.dz_4x4_YI[15]         = 410;
    qControl.dz_4x4_YP[ 0]         = 682;
    qControl.dz_4x4_YP[ 1]         = 586;
    qControl.dz_4x4_YP[ 2]         = 546;
    qControl.dz_4x4_YP[ 3]         = 456;
    qControl.dz_4x4_YP[ 4]         = 586;
    qControl.dz_4x4_YP[ 5]         = 546;
    qControl.dz_4x4_YP[ 6]         = 456;
    qControl.dz_4x4_YP[ 7]         = 342;
    qControl.dz_4x4_YP[ 8]         = 546;
    qControl.dz_4x4_YP[ 9]         = 456;
    qControl.dz_4x4_YP[10]         = 342;
    qControl.dz_4x4_YP[11]         = 292;
    qControl.dz_4x4_YP[12]         = 456;
    qControl.dz_4x4_YP[13]         = 342;
    qControl.dz_4x4_YP[14]         = 292;
    qControl.dz_4x4_YP[15]         = 274;
    qControl.dz_4x4_CI             = 682;
    qControl.dz_4x4_CP             = 342;
    for (UINT32 idx = 0; idx <= 0xf; idx++)
    {
        qControl.dz_8x8_YI[idx] = 682;
        qControl.dz_8x8_YP[idx] = 342;
    }

    lwenc_h264_me_control_s meControl;
    memset(&meControl, 0, sizeof(meControl));

    meControl.refinement_mode             = 1;
    meControl.refine_on_search_enable     = 1;
    meControl.fps_mvcost                  = 1;
    meControl.sps_mvcost                  = 1;
    meControl.sps_cost_func               = 1;
    meControl.vc1_mc_rnd                  = 1;
    meControl.limit_mv.mv_limit_enable    = 1;
    meControl.limit_mv.left_mvx_int       = -2048;
    meControl.limit_mv.top_mvy_int        = -512;
    meControl.limit_mv.right_mvx_frac     = 3;
    meControl.limit_mv.right_mvx_int      = 2047;
    meControl.limit_mv.bottom_mvy_frac    = 3;
    meControl.limit_mv.bottom_mvy_int     = 511;
    meControl.predsrc.coloc_stamp_l0      = 1;
    meControl.predsrc.coloc_stamp_l1      = 1;
    meControl.predsrc.coloc_search        = 1;
    meControl.predsrc.coloc_refine        = 1;
    meControl.predsrc.coloc_enable        = 1;
    meControl.predsrc.self_spatial_search = 1;
    meControl.predsrc.self_spatial_refine = 1;
    meControl.predsrc.self_spatial_enable = 1;
    meControl.predsrc.const_mv_stamp_l0   = 2;
    meControl.predsrc.const_mv_stamp_l1   = 2;
    meControl.predsrc.const_mv_search     = 1;
    meControl.predsrc.const_mv_refine     = 1;
    meControl.predsrc.const_mv_enable     = 1;
    meControl.shape0.bitmask[0]           = 0x3c3c0000;
    meControl.shape0.bitmask[1]           = 0x00003c3c;
    meControl.shape1.bitmask[0]           = 0x3e1c0800;
    meControl.shape1.bitmask[1]           = 0x0000081c;
    meControl.shape2.bitmask[0]           = 0x3c3c1800;
    meControl.shape2.bitmask[1]           = 0x0000183c;
    meControl.mbc_mb_size                 = 110;

    if (className == LWC5B7_VIDEO_ENCODER || className == LWC4B7_VIDEO_ENCODER)
    {
        meControl.spatial_hint_pattern = 0xf;
        meControl.temporal_hint_pattern = 0x3f;
        meControl.Lw16partDecisionMadeByFPP = 0;
#ifdef MISRA_5_2
        meControl.fbm_op_winner_num_p_frame = 1;
        meControl.fbm_op_winner_num_b_frame_l0 = 1;
        meControl.fbm_op_winner_num_b_frame_l1 = 1;
#else
        meControl.fbm_output_winner_num_p_frame = 1;
        meControl.fbm_output_winner_num_b_frame_l0 = 1;
        meControl.fbm_output_winner_num_b_frame_l1 = 1;
#endif
        meControl.fbm_select_best_lw32_parttype_num = 3;
        meControl.sps_evaluate_merge_cand_num = 5;
        meControl.fps_quad_thresh_hold = 0;
        meControl.external_hint_order = 1;
        meControl.coloc_hint_order = 1;
        meControl.ct_threshold = 0;
        meControl.hint_type0 = 0;
        meControl.hint_type1 = 1;
        meControl.hint_type2 = 2;
        meControl.hint_type3 = 3;
        meControl.hint_type4 = 4;

        mdControl.l0_part_8x4_enable = 1;
        mdControl.l0_part_4x8_enable = 1;
        mdControl.l0_part_4x4_enable = 1;
        mdControl.intra_ssd_cnt_4x4 = 1;
        mdControl.intra_ssd_cnt_8x8 = 1;
        mdControl.intra_ssd_cnt_16x16 = 1;
        mdControl.skip_evaluate_enable = 0;
        mdControl.rdo_level = 1;
        mdControl.tu_search_num = 2;

        picSetup.pic_control.chroma_skip_threshold_4x4 = 8;
    }

    WritePicSetupSurface(0, picSetupSurface, &picSetup, &sliceControl[0],
        sliceControl.size(), &mdControl, &qControl, &meControl);

    picSetup.pic_control.l0[0] = 0;

    for (UINT32 idx = 0; idx <= 7; idx++)
    {
        picSetup.pic_control.temp_dist_l0[idx] = -1;
        picSetup.pic_control.temp_dist_l1[idx] = -1;
    }
    picSetup.pic_control.temp_dist_l0[0] = 3;

    for (UINT32 refdix1 = 0; refdix1 <= 7; refdix1++)
    {
        for (UINT32 refdix0 = 0; refdix0 <= 7; refdix0++)
        {
            picSetup.pic_control.dist_scale_factor[refdix1][refdix0] =
                (refdix0 == 0) ? 256 : -1;
        }
    }
    picSetup.pic_control.diff_pic_order_cnt_zero[0] = 0xfefefefe;
    picSetup.pic_control.diff_pic_order_cnt_zero[1] = 0xfefefefe;

    picSetup.pic_control.frame_num = 1;
    picSetup.pic_control.pic_order_cnt_lsb = 6;
    picSetup.pic_control.pic_type = 0;

    for (size_t i = 0; sliceControl.size() > i; ++i)
    {
        sliceControl[i].num_ref_idx_active_override_flag = 1;
    }

    meControl.vc1_mc_rnd = 0;

    WritePicSetupSurface(1, picSetupSurface, &picSetup, &sliceControl[0],
        sliceControl.size(), &mdControl, &qControl, &meControl);

    picSetup.pic_control.l0[1] = 2;
    picSetup.pic_control.l1[0] = 2;
    picSetup.pic_control.temp_dist_l0[0] = 1;
    picSetup.pic_control.temp_dist_l0[1] = -2;
    picSetup.pic_control.temp_dist_l1[0] = -2;
    for (UINT32 refdix1 = 1; refdix1 <= 7; refdix1++)
    {
        picSetup.pic_control.dist_scale_factor[refdix1][0] = -1;
    }
    picSetup.pic_control.dist_scale_factor[0][0] = 85;

    picSetup.pic_control.diff_pic_order_cnt_zero[0] = 0xfcfcfc02;
    picSetup.pic_control.diff_pic_order_cnt_zero[1] = 0xfcfcfcfc;

    picSetup.pic_control.frame_num = 2;
    picSetup.pic_control.pic_order_cnt_lsb = 2;
    picSetup.pic_control.pic_type = 1;
    picSetup.pic_control.ref_pic_flag = 0;

    for (size_t i = 0; sliceControl.size() > i; ++i)
    {
        sliceControl[i].num_ref_idx_l0_active_minus1 = 1;
    }

    WritePicSetupSurface(2, picSetupSurface, &picSetup, &sliceControl[0],
        sliceControl.size(), &mdControl, &qControl, &meControl);

    lwenc_persistent_state_s rcProcess;
    memset(&rcProcess, 0, sizeof(rcProcess));

    rcProcess.nal_cpb_fullness = 13206656;
    rcProcess.Wpbi[0]          = 23;
    rcProcess.Wpbi[1]          = 24;
    rcProcess.Wpbi[2]          = 28;
    rcProcess.Whist[0][0]      = 12;
    rcProcess.Whist[0][1]      = 23;
    rcProcess.Whist[0][2]      = 23;
    rcProcess.Whist[1][0]      = 24;
    rcProcess.Whist[1][1]      = 24;
    rcProcess.Whist[1][2]      = 24;
    rcProcess.Whist[2][0]      = 28;
    rcProcess.Whist[2][1]      = 21;
    rcProcess.Whist[2][2]      = 21;
    rcProcess.np               = 48;
    rcProcess.nb               = 160;
    rcProcess.Rmean            = 1309364;
    rcProcess.Bavg             = 1738;
    rcProcess.average_mvx      = 266;
    rcProcess.average_mvy      = -211;
    Platform::VirtualWr(ioRcProcess->GetAddress(), &rcProcess, sizeof(rcProcess));

    H264::SeqParameterSet sps;
    sps.profile_idc                       = picSetup.sps_data.profile_idc;
    sps.level_idc                         = picSetup.sps_data.level_idc;
    sps.chroma_format_idc                 = picSetup.sps_data.chroma_format_idc;
    sps.pic_order_cnt_type                = picSetup.sps_data.pic_order_cnt_type;
    sps.log2_max_frame_num_minus4         = picSetup.sps_data.log2_max_frame_num_minus4;
    sps.log2_max_pic_order_cnt_lsb_minus4 = picSetup.sps_data.log2_max_pic_order_cnt_lsb_minus4;
    sps.pic_width_in_mbs_minus1           = (picSetup.input_cfg.frame_width_minus1 + 1) / 16 - 1;
    sps.pic_height_in_map_units_minus1    = (picSetup.input_cfg.frame_height_minus1 + 1) / 16 - 1;
    sps.frame_mbs_only_flag               = picSetup.sps_data.frame_mbs_only;
    sps.max_num_ref_frames                = MAX_REF_FRAMES;
    sps.direct_8x8_inference_flag = true;
    sps.UnsetEmpty();
    spsNalus[H264_HD_HP_CABAC_MS].CreateDataFromSPS(sps);

    SeqSetSrcImpl spsSrc(sps);
    H264::PicParameterSet pps(&spsSrc);
    pps.entropy_coding_mode_flag               = picSetup.pps_data.entropy_coding_mode_flag;
    pps.num_ref_idx_l0_default_active_minus1   = picSetup.pps_data.num_ref_idx_l0_active_minus1;
    pps.deblocking_filter_control_present_flag = picSetup.pps_data.deblocking_filter_control_present_flag;
    pps.transform_8x8_mode_flag                = picSetup.pps_data.transform_8x8_mode_flag;
    pps.UnsetEmpty();
    ppsNalus[H264_HD_HP_CABAC_MS].CreateDataFromPPS(pps);

    return OK;
}

static RC PrepareControlStructuresH264BFrames
(
    Surface2D *picSetupSurface,
    UINT32 className,
    UINT32 outBitstreamBufferSize
)
{
    RC rc;
    lwenc_h264_drv_pic_setup_s picSetup;
    memset(&picSetup, 0, sizeof(picSetup));

    const UINT32 width  = 176;
    const UINT32 height = 144;

    static_assert(width <= MAX_WIDTH, "");
    static_assert(height <= MAX_HEIGHT, "");

    SetMagicValue(className, &picSetup.magic);

    CHECK_RC(PrepareSurfaceCfg(&picSetup.refpic_cfg, className, width, height, 8,
                               SURFTYPE_REFPIC, SURFFMT_TILED,
                               SURFCOLOR_YUV420, false));
    CHECK_RC(PrepareSurfaceCfg(&picSetup.input_cfg, className, width, height, 8,
                               SURFTYPE_INPUT, SURFFMT_BL2,
                               SURFCOLOR_YUV420, false));
    CHECK_RC(PrepareSurfaceCfg(&picSetup.outputpic_cfg, className,
                               width, height, 8,
                               SURFTYPE_REFPIC, SURFFMT_TILED,
                               SURFCOLOR_YUV420, false));

    picSetup.sps_data.profile_idc                       = 100;
    picSetup.sps_data.level_idc                         = 51;
    picSetup.sps_data.chroma_format_idc                 = 1;
    picSetup.sps_data.log2_max_frame_num_minus4         = 4;
    picSetup.sps_data.log2_max_pic_order_cnt_lsb_minus4 = 4;
    picSetup.sps_data.frame_mbs_only                    = 1;

    picSetup.pps_data.entropy_coding_mode_flag               = 1;
    picSetup.pps_data.num_ref_idx_l0_active_minus1           = 3;
    picSetup.pps_data.num_ref_idx_l1_active_minus1           = 3;
    picSetup.pps_data.deblocking_filter_control_present_flag = 1;
    picSetup.pps_data.transform_8x8_mode_flag                = 1;

    picSetup.rate_control.hrd_type     = 2;
    picSetup.rate_control.maxQPD       = 6;
    picSetup.rate_control.baseQPD      = 3;
    picSetup.rate_control.rhopbi[0]    = 256;
    picSetup.rate_control.rhopbi[1]    = 256;
    picSetup.rate_control.rhopbi[2]    = 256;
    picSetup.rate_control.framerate    = 7680;
    picSetup.rate_control.buffersize   = 300000000;
    picSetup.rate_control.nal_cpb_size = 300000000;
    picSetup.rate_control.nal_bitrate  = 300000000;
    picSetup.rate_control.gop_length   = 0xffffffff;
    picSetup.rate_control.iSizeRatioY  = 1;

    for (UINT32 idx = 0; idx <= 7; idx++)
    {
        picSetup.pic_control.l0[idx] = -2;
        picSetup.pic_control.l1[idx] = -2;
    }
    picSetup.pic_control.slice_control_offset    = sliceControlOffset;
    picSetup.pic_control.me_control_offset       = meControlOffset;
    picSetup.pic_control.md_control_offset       = mdControlOffset;
    picSetup.pic_control.q_control_offset        = quantControlOffset;
    picSetup.pic_control.hist_buf_size           = 21120;
    picSetup.pic_control.bitstream_buf_size      = outBitstreamBufferSize;
    picSetup.pic_control.pic_type                = 3;
    picSetup.pic_control.ref_pic_flag            = 1;
    picSetup.pic_control.cabac_zero_word_enable  = 1;
    picSetup.pic_control.slice_stat_offset       = 0xffffffff;
    picSetup.pic_control.mpec_stat_offset        = 0xffffffff;
    picSetup.pic_control.wp_control_offset       = 1536;
    picSetup.pic_control.b_as_ref                = 1;
    picSetup.pic_control.intra_sel_4x4_threshold = 2000;

    lwenc_h264_slice_control_s sliceControl;
    memset(&sliceControl, 0, sizeof(sliceControl));

    sliceControl.num_mb = 99;
    sliceControl.qp_avr = 25;

    lwenc_h264_me_control_s meControl;
    memset(&meControl, 0, sizeof(meControl));

    meControl.refinement_mode                   = 1;
    meControl.lambda_mode                       = 1;
    meControl.refine_on_search_enable           = 1;
    meControl.fps_mvcost                        = 1;
    meControl.sps_mvcost                        = 1;
    meControl.sps_cost_func                     = 1;
    meControl.vc1_mc_rnd                        = 1;
    meControl.limit_mv.mv_limit_enable          = 1;
    meControl.limit_mv.left_mvx_int             = -2048;
    meControl.limit_mv.top_mvy_int              = -512;
    meControl.limit_mv.right_mvx_frac           = 3;
    meControl.limit_mv.right_mvx_int            = 2047;
    meControl.limit_mv.bottom_mvy_frac          = 3;
    meControl.limit_mv.bottom_mvy_int           = 511;
    meControl.predsrc.self_temporal_explicit    = 1;
    meControl.predsrc.self_temporal_search      = 1;
    meControl.predsrc.self_temporal_refine      = 1;
    meControl.predsrc.self_temporal_enable      = 1;
    meControl.predsrc.coloc_search              = 1;
    meControl.predsrc.coloc_enable              = 1;
    meControl.predsrc.self_spatial_stamp_l0     = 1;
    meControl.predsrc.self_spatial_explicit     = 1;
    meControl.predsrc.self_spatial_search       = 1;
    meControl.predsrc.self_spatial_refine       = 1;
    meControl.predsrc.self_spatial_enable       = 1;
    meControl.predsrc.external_search           = 0;
    meControl.predsrc.external_refine           = 0;
    meControl.predsrc.external_enable           = 0;
    meControl.predsrc.const_mv_stamp_l0         = 3;
    meControl.predsrc.const_mv_explicit         = 1;
    meControl.predsrc.const_mv_search           = 1;
    meControl.predsrc.const_mv_enable           = 1;
    meControl.shape0.bitmask[0]                 = 0xff0c0c00;
    meControl.shape0.bitmask[1]                 = 0x00000c0c;
    meControl.shape0.hor_adjust                 = 1;
    meControl.shape0.ver_adjust                 = 1;
    meControl.shape1.bitmask[0]                 = 0xff0c0c00;
    meControl.shape1.bitmask[1]                 = 0x00000c0c;
    meControl.shape1.hor_adjust                 = 1;
    meControl.shape1.ver_adjust                 = 1;
    meControl.shape2.bitmask[0]                 = 0xff0c0c00;
    meControl.shape2.bitmask[1]                 = 0x00000c0c;
    meControl.shape2.hor_adjust                 = 1;
    meControl.shape2.ver_adjust                 = 1;
    meControl.shape3.bitmask[0]                 = 0xff1e0c0c;
    meControl.shape3.bitmask[1]                 = 0x000c0c1e;
    meControl.shape3.hor_adjust                 = 1;
    meControl.shape3.ver_adjust                 = 1;
    meControl.mbc_mb_size                       = 320;
    meControl.spatial_hint_pattern              = 15;
    meControl.temporal_hint_pattern             = 63;
    meControl.Lw16partDecisionMadeByFPP         = 1;
#ifdef MISRA_5_2
    meControl.fbm_op_winner_num_p_frame         = 1;
    meControl.fbm_op_winner_num_b_frame_l0      = 1;
    meControl.fbm_op_winner_num_b_frame_l1      = 1;
#else
    meControl.fbm_output_winner_num_p_frame     = 1;
    meControl.fbm_output_winner_num_b_frame_l0  = 1;
    meControl.fbm_output_winner_num_b_frame_l1  = 1;
#endif
    meControl.fbm_select_best_lw32_parttype_num = 3;
    meControl.sps_evaluate_merge_cand_num       = 5;
    meControl.external_hint_order               = 1;
    meControl.hint_type0                        = 3;
    meControl.hint_type1                        = 2;
    meControl.hint_type3                        = 1;
    meControl.hint_type4                        = 4;


    lwenc_h264_md_control_s mdControl;
    memset(&mdControl, 0, sizeof(mdControl));

    mdControl.intra_luma4x4_mode_enable       = 511;
    mdControl.intra_luma8x8_mode_enable       = 511;
    mdControl.intra_luma16x16_mode_enable     = 15;
    mdControl.intra_chroma_mode_enable        = 15;
    mdControl.l0_part_16x16_enable            = 1;
    mdControl.l0_part_16x8_enable             = 1;
    mdControl.l0_part_8x16_enable             = 1;
    mdControl.l0_part_8x8_enable              = 1;
    mdControl.l0_part_8x4_enable              = 1;
    mdControl.l0_part_4x8_enable              = 1;
    mdControl.l0_part_4x4_enable              = 1;
    mdControl.l1_part_16x16_enable            = 1;
    mdControl.l1_part_16x8_enable             = 1;
    mdControl.l1_part_8x16_enable             = 1;
    mdControl.l1_part_8x8_enable              = 1;
    mdControl.bi_part_16x16_enable            = 1;
    mdControl.bi_part_16x8_enable             = 1;
    mdControl.bi_part_8x16_enable             = 1;
    mdControl.bi_part_8x8_enable              = 1;
    mdControl.bskip_enable                    = 1;
    mdControl.pskip_enable                    = 1;
    mdControl.intra_nxn_bias_multiplier       = 24;
    mdControl.intra_most_prob_bias_multiplier = 4;
    mdControl.mv_cost_enable                  = 1;
    mdControl.early_intra_mode_type_16x16dc   = 1;
    mdControl.early_intra_mode_type_16x16h    = 1;
    mdControl.early_intra_mode_type_16x16v    = 1;
    mdControl.ip_search_mode                  = 7;
    mdControl.intra_ssd_cnt_4x4               = 1;
    mdControl.intra_ssd_cnt_8x8               = 1;
    mdControl.intra_ssd_cnt_16x16             = 1;
    mdControl.skip_evaluate_enable            = 1;
    mdControl.rdo_level                       = 1;
    mdControl.tu_search_num                   = 2;

    lwenc_h264_quant_control_s qControl;
    memset(&qControl, 0, sizeof(qControl));

    qControl.qpp_run_vector_4x4    = 1387;
    qControl.qpp_run_vector_8x8[0] = 43775;
    qControl.qpp_run_vector_8x8[1] = 65450;
    qControl.qpp_run_vector_8x8[2] = 15;
    qControl.qpp_luma8x8_cost      = 15;
    qControl.qpp_luma16x16_cost    = 15;
    qControl.qpp_chroma_cost       = 15;
    qControl.dz_4x4_YI[0]          = 1023;
    qControl.dz_4x4_YI[1]          = 878;
    qControl.dz_4x4_YI[2]          = 820;
    qControl.dz_4x4_YI[3]          = 682;
    qControl.dz_4x4_YI[4]          = 878;
    qControl.dz_4x4_YI[5]          = 820;
    qControl.dz_4x4_YI[6]          = 682;
    qControl.dz_4x4_YI[7]          = 512;
    qControl.dz_4x4_YI[8]          = 820;
    qControl.dz_4x4_YI[9]          = 682;
    qControl.dz_4x4_YI[10]         = 512;
    qControl.dz_4x4_YI[11]         = 410;
    qControl.dz_4x4_YI[12]         = 682;
    qControl.dz_4x4_YI[13]         = 512;
    qControl.dz_4x4_YI[14]         = 410;
    qControl.dz_4x4_YI[15]         = 410;
    qControl.dz_4x4_YP[0]          = 682;
    qControl.dz_4x4_YP[1]          = 586;
    qControl.dz_4x4_YP[2]          = 546;
    qControl.dz_4x4_YP[3]          = 456;
    qControl.dz_4x4_YP[4]          = 586;
    qControl.dz_4x4_YP[5]          = 546;
    qControl.dz_4x4_YP[6]          = 456;
    qControl.dz_4x4_YP[7]          = 342;
    qControl.dz_4x4_YP[8]          = 546;
    qControl.dz_4x4_YP[9]          = 456;
    qControl.dz_4x4_YP[10]         = 342;
    qControl.dz_4x4_YP[11]         = 292;
    qControl.dz_4x4_YP[12]         = 456;
    qControl.dz_4x4_YP[13]         = 342;
    qControl.dz_4x4_YP[14]         = 292;
    qControl.dz_4x4_YP[15]         = 274;
    qControl.dz_4x4_CI             = 682;
    qControl.dz_4x4_CP             = 342;
    for (size_t i = 0; NUMELEMS(qControl.dz_8x8_YI) > i; ++i)
    {
        qControl.dz_8x8_YI[i] = 682;
    }
    for (size_t i = 0; NUMELEMS(qControl.dz_8x8_YP) > i; ++i)
    {
        qControl.dz_8x8_YP[i] = 342;
    }

    WritePicSetupSurface(0, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);

    picSetup.pic_control.l0[0] = 0;
    picSetup.pic_control.temp_dist_l0[0] = 3;
    for (size_t i = 1; NUMELEMS(picSetup.pic_control.temp_dist_l0) > i; ++i)
    {
        picSetup.pic_control.temp_dist_l0[i] = -1;
    }
    for (size_t i = 0; NUMELEMS(picSetup.pic_control.temp_dist_l1) > i; ++i)
    {
        picSetup.pic_control.temp_dist_l1[i] = -1;
    }

    for (UINT32 refdix1 = 0; refdix1 <= 7; refdix1++)
    {
        for (UINT32 refdix0 = 0; refdix0 <= 7; refdix0++)
        {
            picSetup.pic_control.dist_scale_factor[refdix1][refdix0] =
                (refdix0 == 0) ? 256 : -1;
        }
    }
    picSetup.pic_control.diff_pic_order_cnt_zero[0] = 0xfefefefe;
    picSetup.pic_control.diff_pic_order_cnt_zero[1] = 0xfefefefe;

    picSetup.pic_control.frame_num         = 1;
    picSetup.pic_control.pic_order_cnt_lsb = 6;
    picSetup.pic_control.pic_type          = 0;

    sliceControl.qp_avr                           = 28;
    sliceControl.num_ref_idx_active_override_flag = 1;

    meControl.vc1_mc_rnd = 0;

    WritePicSetupSurface(1, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);

    picSetup.pic_control.l0[1]                      = 2;
    picSetup.pic_control.l1[0]                      = 2;
    picSetup.pic_control.l1[1]                      = 0;
    picSetup.pic_control.temp_dist_l0[0]            = 1;
    picSetup.pic_control.temp_dist_l0[1]            = -2;
    picSetup.pic_control.temp_dist_l1[0]            = -2;
    picSetup.pic_control.temp_dist_l1[1]            = 1;
    picSetup.pic_control.dist_scale_factor[0][0]    = 85;
    picSetup.pic_control.dist_scale_factor[1][0]    = -1;
    picSetup.pic_control.dist_scale_factor[1][1]    = 171;
    picSetup.pic_control.dist_scale_factor[2][0]    = -1;
    picSetup.pic_control.dist_scale_factor[3][0]    = -1;
    picSetup.pic_control.dist_scale_factor[4][0]    = -1;
    picSetup.pic_control.dist_scale_factor[5][0]    = -1;
    picSetup.pic_control.dist_scale_factor[6][0]    = -1;
    picSetup.pic_control.dist_scale_factor[7][0]    = -1;
    picSetup.pic_control.diff_pic_order_cnt_zero[0] = 0xfcfc0102;
    picSetup.pic_control.diff_pic_order_cnt_zero[1] = 0xfcfcfcfc;
    picSetup.pic_control.frame_num                  = 2;
    picSetup.pic_control.pic_order_cnt_lsb          = 2;
    picSetup.pic_control.ref_pic_flag               = 0;
    picSetup.pic_control.pic_type                   = 1;
    sliceControl.qp_avr                             = 31;
    sliceControl.num_ref_idx_l0_active_minus1       = 1;
    sliceControl.num_ref_idx_l1_active_minus1       = 1;

    WritePicSetupSurface(2, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);

    picSetup.pic_control.l0[1]                      = 0;
    picSetup.pic_control.l0[0]                      = 4;
    picSetup.pic_control.l0[2]                      = 2;
    picSetup.pic_control.l1[2]                      = 0;
    picSetup.pic_control.l1[1]                      = 4;
    picSetup.pic_control.temp_dist_l0[1]            = 2;
    picSetup.pic_control.temp_dist_l1[0]            = -1;
    picSetup.pic_control.temp_dist_l1[2]            = 2;
    picSetup.pic_control.dist_scale_factor[0][0]    = 128;
    picSetup.pic_control.dist_scale_factor[0][1]    = 171;
    picSetup.pic_control.dist_scale_factor[1][1]    = 512;
    picSetup.pic_control.dist_scale_factor[1][2]    = 128;
    picSetup.pic_control.dist_scale_factor[2][0]    = -256;
    picSetup.pic_control.dist_scale_factor[2][2]    = 85;
    picSetup.pic_control.diff_pic_order_cnt_zero[0] = 0xf8020104;
    picSetup.pic_control.diff_pic_order_cnt_zero[1] = 0xf8f8f8f8;
    picSetup.pic_control.frame_num                  = 3;
    picSetup.pic_control.pic_order_cnt_lsb          = 4;
    sliceControl.num_ref_idx_l0_active_minus1       = 2;
    sliceControl.num_ref_idx_l1_active_minus1       = 2;

    WritePicSetupSurface(3, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);

    picSetup.pic_control.l0[0]           = 2;
    picSetup.pic_control.l0[1]           = 6;
    picSetup.pic_control.l0[2]           = 4;
    picSetup.pic_control.l0[3]           = 0;
    picSetup.pic_control.l1[0]           = -2;
    picSetup.pic_control.l1[1]           = -2;
    picSetup.pic_control.l1[2]           = -2;
    picSetup.pic_control.temp_dist_l0[0] = 3;
    picSetup.pic_control.temp_dist_l0[1] = 4;
    picSetup.pic_control.temp_dist_l0[2] = 5;
    picSetup.pic_control.temp_dist_l0[3] = 6;
    picSetup.pic_control.temp_dist_l1[1] = -1;
    picSetup.pic_control.temp_dist_l1[2] = -1;
    for (UINT32 refdix1 = 0; 8 > refdix1; ++refdix1)
    {
        for (UINT32 refdix0 = 0; 4 > refdix0; ++refdix0)
        {
            picSetup.pic_control.dist_scale_factor[refdix1][refdix0] = 256;
        }
    }
    picSetup.pic_control.diff_pic_order_cnt_zero[0] = 0xf0f0f0f0;
    picSetup.pic_control.diff_pic_order_cnt_zero[1] = 0xf0f0f0f0;
    picSetup.pic_control.frame_num                  = 4;
    picSetup.pic_control.pic_order_cnt_lsb          = 12;
    picSetup.pic_control.pic_type                   = 0;
    picSetup.pic_control.ref_pic_flag               = 1;

    sliceControl.qp_avr                                                    = 28;
    sliceControl.num_ref_idx_active_override_flag                          = 0;
    sliceControl.num_ref_idx_l0_active_minus1                              = 3;
    sliceControl.num_ref_idx_l1_active_minus1                              = 0;
    sliceControl.ref_pic_cmd_cfg.reorder_l0_cmd_count                      = 4;
    sliceControl.ref_pic_list_reorder_cmd[0][0].abs_diff_pic_num_minus1    = 2;
    sliceControl.ref_pic_list_reorder_cmd[0][1].reordering_of_pic_nums_idc = 1;
    sliceControl.ref_pic_list_reorder_cmd[0][1].abs_diff_pic_num_minus1    = 1;
    sliceControl.ref_pic_list_reorder_cmd[0][3].reordering_of_pic_nums_idc = 3;

    meControl.vc1_mc_rnd = 1;

    WritePicSetupSurface(4, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);

    picSetup.pic_control.l1[0]                   = 8;
    picSetup.pic_control.l1[1]                   = 2;
    picSetup.pic_control.l1[2]                   = 6;
    picSetup.pic_control.l1[3]                   = 4;
    picSetup.pic_control.temp_dist_l0[0]         = 1;
    picSetup.pic_control.temp_dist_l0[1]         = 2;
    picSetup.pic_control.temp_dist_l0[2]         = 3;
    picSetup.pic_control.temp_dist_l0[3]         = 4;
    picSetup.pic_control.temp_dist_l1[0]         = -2;
    picSetup.pic_control.temp_dist_l1[1]         = 1;
    picSetup.pic_control.temp_dist_l1[2]         = 2;
    picSetup.pic_control.temp_dist_l1[3]         = 3;
    picSetup.pic_control.dist_scale_factor[0][0] = 85;
    picSetup.pic_control.dist_scale_factor[0][1] = 128;
    picSetup.pic_control.dist_scale_factor[0][2] = 154;
    picSetup.pic_control.dist_scale_factor[0][3] = 171;
    picSetup.pic_control.dist_scale_factor[1][0] = -1;
    picSetup.pic_control.dist_scale_factor[1][1] = 512;
    picSetup.pic_control.dist_scale_factor[1][2] = 384;
    picSetup.pic_control.dist_scale_factor[1][3] = 341;
    picSetup.pic_control.dist_scale_factor[2][0] = -256;
    picSetup.pic_control.dist_scale_factor[2][1] = -1;
    picSetup.pic_control.dist_scale_factor[2][2] = 768;
    picSetup.pic_control.dist_scale_factor[2][3] = 512;
    picSetup.pic_control.dist_scale_factor[3][0] = -128;
    picSetup.pic_control.dist_scale_factor[3][1] = -512;
    picSetup.pic_control.dist_scale_factor[3][2] = -1;
    picSetup.pic_control.dist_scale_factor[3][3] = 1023;
    for (UINT32 refdix1 = 4; 8 > refdix1; ++refdix1)
    {
        for (UINT32 refdix0 = 0; 4 > refdix0; ++refdix0)
        {
            picSetup.pic_control.dist_scale_factor[refdix1][refdix0] = -1;
        }
    }
    picSetup.pic_control.diff_pic_order_cnt_zero[0] = 0x04020100;
    picSetup.pic_control.frame_num                  = 5;
    picSetup.pic_control.pic_order_cnt_lsb          = 8;
    picSetup.pic_control.ref_pic_flag               = 0;
    picSetup.pic_control.pic_type                   = 1;

    sliceControl.qp_avr                               = 31;
    sliceControl.ref_pic_cmd_cfg.reorder_l0_cmd_count = 0;
    sliceControl.num_ref_idx_l1_active_minus1         = 3;

    WritePicSetupSurface(5, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);

    picSetup.pic_control.l0[0]                   = 10;
    picSetup.pic_control.l0[1]                   = 2;
    picSetup.pic_control.l0[2]                   = 6;
    picSetup.pic_control.l0[3]                   = 4;
    picSetup.pic_control.l1[1]                   = 10;
    picSetup.pic_control.l1[2]                   = 2;
    picSetup.pic_control.l1[3]                   = 6;
    picSetup.pic_control.temp_dist_l1[0]         = -1;
    picSetup.pic_control.dist_scale_factor[0][0] = 128;
    picSetup.pic_control.dist_scale_factor[0][1] = 171;
    picSetup.pic_control.dist_scale_factor[0][2] = 192;
    picSetup.pic_control.dist_scale_factor[0][3] = 205;
    picSetup.pic_control.frame_num               = 6;
    picSetup.pic_control.pic_order_cnt_lsb       = 10;

    WritePicSetupSurface(6, picSetupSurface, &picSetup, &sliceControl, 1,
        &mdControl, &qControl, &meControl);


    lwenc_persistent_state_s rcProcess;
    memset(&rcProcess, 0, sizeof(rcProcess));

    rcProcess.nal_cpb_fullness = 1641856;
    rcProcess.Wpbi[0]          = 128;
    rcProcess.Wpbi[1]          = 121;
    rcProcess.Wpbi[2]          = 380;
    rcProcess.Whist[0][0]      = 84;
    rcProcess.Whist[0][1]      = 66;
    rcProcess.Whist[0][2]      = 171;
    rcProcess.Whist[1][0]      = 28;
    rcProcess.Whist[1][1]      = 34;
    rcProcess.Whist[1][2]      = 26;
    rcProcess.Whist[2][0]      = 380;
    rcProcess.Whist[2][1]      = 121;
    rcProcess.Whist[2][2]      = 121;
    rcProcess.np               = 8;
    rcProcess.nb               = 88;
    rcProcess.Rmean            = 500759;
    rcProcess.average_mvx      = 3;

    H264::SeqParameterSet sps;
    sps.profile_idc                       = picSetup.sps_data.profile_idc;
    sps.level_idc                         = picSetup.sps_data.level_idc;
    sps.chroma_format_idc                 = picSetup.sps_data.chroma_format_idc;
    sps.pic_order_cnt_type                = picSetup.sps_data.pic_order_cnt_type;
    sps.log2_max_frame_num_minus4         = picSetup.sps_data.log2_max_frame_num_minus4;
    sps.log2_max_pic_order_cnt_lsb_minus4 = picSetup.sps_data.log2_max_pic_order_cnt_lsb_minus4;
    sps.pic_width_in_mbs_minus1           = (picSetup.input_cfg.frame_width_minus1 + 1) / 16 - 1;
    sps.pic_height_in_map_units_minus1    = (picSetup.input_cfg.frame_height_minus1 + 1) / 16 - 1;
    sps.frame_mbs_only_flag               = picSetup.sps_data.frame_mbs_only;
    sps.max_num_ref_frames                = MAX_REF_FRAMES;
    sps.direct_8x8_inference_flag         = true;
    sps.frame_cropping_flag               = true;
    sps.frame_crop_bottom_offset          = 4;
    sps.UnsetEmpty();
    spsNalus[H264_B_FRAMES].CreateDataFromSPS(sps);

    SeqSetSrcImpl spsSrc(sps);
    H264::PicParameterSet pps(&spsSrc);
    pps.entropy_coding_mode_flag               = picSetup.pps_data.entropy_coding_mode_flag;
    pps.num_ref_idx_l0_default_active_minus1   = picSetup.pps_data.num_ref_idx_l0_active_minus1;
    pps.deblocking_filter_control_present_flag = picSetup.pps_data.deblocking_filter_control_present_flag;
    pps.transform_8x8_mode_flag                = picSetup.pps_data.transform_8x8_mode_flag;
    pps.UnsetEmpty();
    ppsNalus[H264_B_FRAMES].CreateDataFromPPS(pps);

    return OK;
}

static RC PrepareControlStructuresH265Generic
(
    Surface2D *picSetupSurface,
    UINT32 className,
    UINT32 outBitstreamBufferSize
)
{
    RC rc;
    lwenc_h265_drv_pic_setup_s picSetup;
    memset(&picSetup, 0, sizeof(picSetup));

    const UINT32 width  = 192;
    const UINT32 height = 160;

    SetMagicValue(className, &picSetup.magic);

    CHECK_RC(PrepareSurfaceCfg(&picSetup.refpic_cfg, className, 192, 160, 8,
                               SURFTYPE_REFPIC, SURFFMT_TILED,
                               SURFCOLOR_YUV420, false));
    CHECK_RC(PrepareSurfaceCfg(&picSetup.input_cfg, className, 192, 160, 8,
                               SURFTYPE_INPUT, SURFFMT_BL2,
                               SURFCOLOR_YUV420, false));
    picSetup.input_cfg.block_height  = 2;
    picSetup.input_cfg.input_bl_mode = 1;
    CHECK_RC(PrepareSurfaceCfg(&picSetup.outputpic_cfg, className, 192, 160, 8,
                               SURFTYPE_REFPIC, SURFFMT_TILED,
                               SURFCOLOR_YUV420, false));

    picSetup.sps_data.chroma_format_idc                 = 1;
    picSetup.sps_data.log2_max_pic_order_cnt_lsb_minus4 = 4;
    picSetup.sps_data.min_lw_size                       = 1;
    picSetup.sps_data.max_lw_size                       = 2;
    picSetup.sps_data.max_tu_size                       = 3;
    picSetup.sps_data.max_transform_depth_inter         = 3;
    picSetup.sps_data.max_transform_depth_intra         = 3;
    picSetup.sps_data.amp_enabled_flag                  = 1;
    picSetup.sps_data.num_short_term_ref_pic_sets       = 1;
    picSetup.pps_data.transform_skip_enabled_flag                = 1;
    picSetup.pps_data.pps_loop_filter_across_slices_enabled_flag = 1;
    picSetup.pps_data.cabac_init_present_flag                    = 1;

    for (UINT32 idx = 0; idx < 8; idx++)
    {
        picSetup.pic_control.l0[idx] = -2;
        picSetup.pic_control.l1[idx] = -2;
    }

    picSetup.pic_control.slice_control_offset                    = sliceControlOffset;
    picSetup.pic_control.me_control_offset                       = meControlOffset;
    picSetup.pic_control.md_control_offset                       = mdControlOffset;
    picSetup.pic_control.q_control_offset                        = quantControlOffset;
    picSetup.pic_control.hist_buf_size                           = HIST_BUF_SIZE(width, height);
    picSetup.pic_control.bitstream_buf_size                      = outBitstreamBufferSize;
    picSetup.pic_control.pic_type                                = 3;
    picSetup.pic_control.ref_pic_flag                            = 1;
    picSetup.pic_control.codec                                   = 5;
    picSetup.pic_control.ceil_log2_pic_size_in_ctbs              = 5;
    picSetup.pic_control.slice_stat_offset                       = 64;
    picSetup.pic_control.mpec_stat_offset                        = 0xffffffff;
    picSetup.pic_control.aq_stat_offset                          = 0xffffffff;
    picSetup.pic_control.nal_unit_type                           = 19;
    picSetup.pic_control.nuh_temporal_id_plus1                   = 1;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 1;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 1;

    lwenc_h265_md_control_s md_control;
    memset(&md_control, 0, sizeof(md_control));
    md_control.h265_intra_luma4x4_mode_enable       = 0xffffffff;
    md_control.h265_intra_luma8x8_mode_enable       = 0xffffffff;
    md_control.h265_intra_luma16x16_mode_enable     = 0xffffffff;
    md_control.h265_intra_luma32x32_mode_enable     = 0xffffffff;
    md_control.h265_intra_luma_mode_enable_leftbits = 1073741823;
    md_control.lw16x16_l0_part_2Nx2N_enable = 1;
    md_control.lw16x16_l0_part_2NxN_enable  = 1;
    md_control.lw16x16_l0_part_Nx2N_enable  = 1;
    md_control.lw16x16_l0_part_NxN_enable   = 1;
    md_control.lw32x32_l0_part_2Nx2N_enable = 1;
    md_control.lw32x32_l0_part_2NxN_enable  = 1;
    md_control.lw32x32_l0_part_Nx2N_enable  = 1;
    md_control.lw32x32_l0_part_2NxnU_enable = 1;
    md_control.lw32x32_l0_part_2NxnD_enable = 1;
    md_control.lw32x32_l0_part_nLx2N_enable = 1;
    md_control.lw32x32_l0_part_nRx2N_enable = 1;
    md_control.intra_mode_8x8_enable         = 1;
    md_control.intra_mode_16x16_enable       = 1;
    md_control.intra_mode_32x32_enable       = 1;
    md_control.h265_intra_chroma_mode_enable = 31;
    md_control.tusize_4x4_enable             = 1;
    md_control.tusize_8x8_enable             = 1;
    md_control.tusize_16x16_enable           = 1;
    md_control.tusize_32x32_enable           = 1;
    md_control.pskip_enable                  = 1;
    md_control.mv_cost_enable                = 1;
    md_control.ip_search_mode                = 7;
    md_control.intra_ssd_cnt_4x4             = 1;
    md_control.intra_ssd_cnt_8x8             = 1;
    md_control.intra_ssd_cnt_16x16           = 1;
    md_control.intra_ssd_cnt_32x32           = 1;

    lwenc_h265_quant_control_s qControl;
    memset(&qControl, 0, sizeof(qControl));
    qControl.qpp_run_vector_4x4    = 1387;
    qControl.qpp_run_vector_8x8[0] = 43775;
    qControl.qpp_run_vector_8x8[1] = 65450;
    qControl.qpp_run_vector_8x8[2] = 15;
    qControl.qpp_luma8x8_cost      = 15;
    qControl.qpp_luma16x16_cost    = 15;
    qControl.qpp_chroma_cost       = 15;

    lwenc_h265_slice_control_s sliceControl;
    memset(&sliceControl, 0, sizeof(sliceControl));
    sliceControl.num_ctus                                     = 30;
    sliceControl.qp_avr                                       = 30;
    sliceControl.slice_type                                   = 2;
    sliceControl.slice_loop_filter_across_slices_enabled_flag = 1;
    sliceControl.qp_slice_max                                 = 51;

    lwenc_h264_me_control_s meControl;
    memset(&meControl, 0, sizeof(meControl));
    meControl.refinement_mode              = 1;
    meControl.refine_on_search_enable      = 1;
    meControl.fps_mvcost                   = 1;
    meControl.sps_mvcost                   = 1;
    meControl.sps_cost_func                = 1;
    meControl.predsrc.self_temporal_search = 1;
    meControl.predsrc.self_temporal_enable = 1;
    meControl.predsrc.self_spatial_search  = 1;
    meControl.predsrc.self_spatial_enable  = 1;
    meControl.predsrc.const_mv_stamp_l0    = 3;
    meControl.predsrc.const_mv_search      = 1;
    meControl.predsrc.const_mv_enable      = 1;
    meControl.shape0.bitmask[0]            = 2132543488;
    meControl.shape0.bitmask[1]            = 2076;
    meControl.shape0.hor_adjust            = 1;
    meControl.shape0.ver_adjust            = 1;
    meControl.shape1.bitmask[0]            = 2132543488;
    meControl.shape1.bitmask[1]            = 2076;
    meControl.shape1.hor_adjust            = 1;
    meControl.shape1.ver_adjust            = 1;
    meControl.shape2.bitmask[0]            = 2132543488;
    meControl.shape2.bitmask[1]            = 2076;
    meControl.shape2.hor_adjust            = 1;
    meControl.shape2.ver_adjust            = 1;
    meControl.shape3.bitmask[0]            = 2132548745;
    meControl.shape3.bitmask[1]            = 0x81081c1c;
    meControl.shape3.hor_adjust            = 1;
    meControl.shape3.ver_adjust            = 1;
    meControl.mbc_mb_size                  = 140;

    if (className == LWC5B7_VIDEO_ENCODER || className == LWC4B7_VIDEO_ENCODER)
    {
        meControl.hint_type0                        = 1;
        meControl.hint_type1                        = 2;
        meControl.hint_type2                        = 0;
        meControl.hint_type3                        = 3;
        meControl.hint_type4                        = 4;
        picSetup.pic_control.slice_stat_offset      = 64;
    }

    WritePicSetupSurface(0, picSetupSurface, &picSetup, &sliceControl, 1,
            &md_control, &qControl, &meControl);

    picSetup.pic_control.l0[0]           = 0;

    for (UINT32 idx = 0; idx < 8; idx++)
    {
        picSetup.pic_control.temp_dist_l0[idx] = -1;
        picSetup.pic_control.temp_dist_l1[idx] = -1;

    }
    picSetup.pic_control.temp_dist_l0[0] = 1;

    for (UINT32 idx1 = 0; idx1 < 8; idx1++)
    {
        for (UINT32 idx2 = 0; idx2 < 8; idx2 ++)
        {
            if (idx2 == 0)
            {
                picSetup.pic_control.dist_scale_factor[idx1][idx2] = 256;
            }
            else
            {
                picSetup.pic_control.dist_scale_factor[idx1][idx2] = -1;
            }
        }
    }

    picSetup.pic_control.diff_pic_order_cnt_zero[0]      = 0xfefefefe;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]      = 0xfefefefe;
    picSetup.pic_control.pic_order_cnt_lsb               = 1;
    picSetup.pic_control.pic_type                        = 0;
    picSetup.pic_control.short_term_ref_pic_set_sps_flag = 1;
    picSetup.pic_control.nal_unit_type                   = 1;
    sliceControl.slice_type                              = 0;

    WritePicSetupSurface(1, picSetupSurface, &picSetup, &sliceControl, 1,
            &md_control, &qControl, &meControl);

    picSetup.pic_control.l0[0]             = 2;
    picSetup.pic_control.pic_order_cnt_lsb = 2;
    WritePicSetupSurface(2, picSetupSurface, &picSetup, &sliceControl, 1,
            &md_control, &qControl, &meControl);

    picSetup.pic_control.l0[0]             = 0;
    picSetup.pic_control.pic_order_cnt_lsb = 3;
    WritePicSetupSurface(3, picSetupSurface, &picSetup, &sliceControl, 1,
            &md_control, &qControl, &meControl);

    picSetup.pic_control.l0[0]             = 2;
    picSetup.pic_control.pic_order_cnt_lsb = 4;
    WritePicSetupSurface(4, picSetupSurface, &picSetup, &sliceControl, 1,
            &md_control, &qControl, &meControl);

    return OK;
}

static RC PrepareControlStructuresH265TenBit444
(
    Surface2D *picSetupSurface,
    UINT32 className
)
{
    RC rc;
    lwenc_h265_drv_pic_setup_s picSetup;
    memset(&picSetup, 0, sizeof(picSetup));

    SetMagicValue(className, &picSetup.magic);

    CHECK_RC(PrepareSurfaceCfg(&picSetup.refpic_cfg, className, 192, 160, 10,
                               SURFTYPE_REFPIC, SURFFMT_TILED,
                               SURFCOLOR_YUV444, false));
    CHECK_RC(PrepareSurfaceCfg(&picSetup.input_cfg, className, 192, 160, 10,
                               SURFTYPE_INPUT, SURFFMT_BL2,
                               SURFCOLOR_YUV444, false));
    picSetup.input_cfg.block_height  = 2;
    picSetup.input_cfg.input_bl_mode = 1;
    CHECK_RC(PrepareSurfaceCfg(&picSetup.outputpic_cfg, className, 192, 160, 10,
                               SURFTYPE_REFPIC, SURFFMT_TILED,
                               SURFCOLOR_YUV444, false));

    picSetup.sps_data.chroma_format_idc                 = 3; // yuv4:4:4
    picSetup.sps_data.separate_colour_plane_flag        = 1; // For 4:4:4
    picSetup.sps_data.log2_max_pic_order_cnt_lsb_minus4 = 4;
    picSetup.sps_data.min_lw_size                       = 1;
    picSetup.sps_data.max_lw_size                       = 2;
    picSetup.sps_data.max_tu_size                       = 3;
    picSetup.sps_data.max_transform_depth_inter         = 3;
    picSetup.sps_data.max_transform_depth_intra         = 3;
    picSetup.sps_data.amp_enabled_flag                  = 1;
    picSetup.sps_data.num_short_term_ref_pic_sets       = 1;
    picSetup.sps_data.bit_depth_minus_8                 = 2;
    picSetup.pps_data.transform_skip_enabled_flag                = 1;
    picSetup.pps_data.pps_loop_filter_across_slices_enabled_flag = 1;
    picSetup.pps_data.cabac_init_present_flag                    = 1;

    for (UINT32 idx = 0; idx < 8; idx++)
    {
        picSetup.pic_control.l0[idx] = -2;
        picSetup.pic_control.l1[idx] = -2;
    }

    picSetup.pic_control.slice_control_offset                    = 1024;
    picSetup.pic_control.me_control_offset                       = 1792;
    picSetup.pic_control.md_control_offset                       = 1280;
    picSetup.pic_control.q_control_offset                        = 1536;
    picSetup.pic_control.hist_buf_size                           = 16896;
    picSetup.pic_control.bitstream_buf_size                      = 307200;
    picSetup.pic_control.pic_type                                = 3;
    picSetup.pic_control.ref_pic_flag                            = 1;
    picSetup.pic_control.codec                                   = 5;
    picSetup.pic_control.ceil_log2_pic_size_in_ctbs              = 5;
    picSetup.pic_control.slice_stat_offset                       = 256;
    picSetup.pic_control.mpec_stat_offset                        = 0xffffffff;
    picSetup.pic_control.aq_stat_offset                          = 0xffffffff;
    picSetup.pic_control.nal_unit_type                           = 19;
    picSetup.pic_control.nuh_temporal_id_plus1                   = 1;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 1;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 1;

    lwenc_h265_md_control_s md_control;
    memset(&md_control, 0, sizeof(md_control));
    md_control.h265_intra_luma4x4_mode_enable       = 0xffffffff;
    md_control.h265_intra_luma8x8_mode_enable       = 0xffffffff;
    md_control.h265_intra_luma16x16_mode_enable     = 0xffffffff;
    md_control.h265_intra_luma32x32_mode_enable     = 0xffffffff;
    md_control.h265_intra_luma_mode_enable_leftbits = 1073741823;
    md_control.lw16x16_l0_part_2Nx2N_enable = 1;
    md_control.lw16x16_l0_part_2NxN_enable  = 1;
    md_control.lw16x16_l0_part_Nx2N_enable  = 1;
    md_control.lw16x16_l0_part_NxN_enable   = 1;
    md_control.lw32x32_l0_part_2Nx2N_enable = 1;
    md_control.lw32x32_l0_part_2NxN_enable  = 1;
    md_control.lw32x32_l0_part_Nx2N_enable  = 1;
    md_control.lw32x32_l0_part_2NxnU_enable = 1;
    md_control.lw32x32_l0_part_2NxnD_enable = 1;
    md_control.lw32x32_l0_part_nLx2N_enable = 1;
    md_control.lw32x32_l0_part_nRx2N_enable = 1;
    md_control.intra_mode_8x8_enable         = 1;
    md_control.intra_mode_16x16_enable       = 1;
    md_control.intra_mode_32x32_enable       = 1;
    md_control.h265_intra_chroma_mode_enable = 31;
    md_control.tusize_4x4_enable             = 1;
    md_control.tusize_8x8_enable             = 1;
    md_control.tusize_16x16_enable           = 1;
    md_control.tusize_32x32_enable           = 1;
    md_control.pskip_enable                  = 1;
    md_control.mv_cost_enable                = 1;
    md_control.ip_search_mode                = 7;
    md_control.intra_ssd_cnt_4x4             = 1;
    md_control.intra_ssd_cnt_8x8             = 1;
    md_control.intra_ssd_cnt_16x16           = 1;
    md_control.intra_ssd_cnt_32x32           = 1;

    lwenc_h265_quant_control_s qControl;
    memset(&qControl, 0, sizeof(qControl));
    qControl.qpp_run_vector_4x4    = 1387;
    qControl.qpp_run_vector_8x8[0] = 43775;
    qControl.qpp_run_vector_8x8[1] = 65450;
    qControl.qpp_run_vector_8x8[2] = 15;
    qControl.qpp_luma8x8_cost      = 15;
    qControl.qpp_luma16x16_cost    = 15;
    qControl.qpp_chroma_cost       = 15;

    lwenc_h265_slice_control_s sliceControl;
    memset(&sliceControl, 0, sizeof(sliceControl));
    sliceControl.num_ctus                                     = 30;
    sliceControl.qp_avr                                       = 30;
    sliceControl.slice_type                                   = 2;
    sliceControl.slice_loop_filter_across_slices_enabled_flag = 1;
    sliceControl.qp_slice_max                                 = 51;

    lwenc_h264_me_control_s meControl;
    memset(&meControl, 0, sizeof(meControl));
    meControl.refinement_mode              = 1;
    meControl.refine_on_search_enable      = 1;
    meControl.fps_mvcost                   = 1;
    meControl.sps_mvcost                   = 1;
    meControl.sps_cost_func                = 1;
    meControl.predsrc.self_temporal_search = 1;
    meControl.predsrc.self_temporal_enable = 1;
    meControl.predsrc.self_spatial_search  = 1;
    meControl.predsrc.self_spatial_enable  = 1;
    meControl.predsrc.const_mv_stamp_l0    = 3;
    meControl.predsrc.const_mv_search      = 1;
    meControl.predsrc.const_mv_enable      = 1;
    meControl.shape0.bitmask[0]            = 2132543488;
    meControl.shape0.bitmask[1]            = 2076;
    meControl.shape0.hor_adjust            = 1;
    meControl.shape0.ver_adjust            = 1;
    meControl.shape1.bitmask[0]            = 2132543488;
    meControl.shape1.bitmask[1]            = 2076;
    meControl.shape1.hor_adjust            = 1;
    meControl.shape1.ver_adjust            = 1;
    meControl.shape2.bitmask[0]            = 2132543488;
    meControl.shape2.bitmask[1]            = 2076;
    meControl.shape2.hor_adjust            = 1;
    meControl.shape2.ver_adjust            = 1;
    meControl.shape3.bitmask[0]            = 2132548745;
    meControl.shape3.bitmask[1]            = 0x81081c1c;
    meControl.shape3.hor_adjust            = 1;
    meControl.shape3.ver_adjust            = 1;
    meControl.mbc_mb_size                  = 140;

    if (className == LWC5B7_VIDEO_ENCODER || className == LWC4B7_VIDEO_ENCODER)
    {
        meControl.hint_type0                        = 1;
        meControl.hint_type1                        = 2;
        meControl.hint_type2                        = 0;
        meControl.hint_type3                        = 3;
        meControl.hint_type4                        = 4;
        picSetup.pic_control.slice_stat_offset      = 64;
    }

    WritePicSetupSurface(0, picSetupSurface, &picSetup, &sliceControl, 1,
            &md_control, &qControl, &meControl);

    picSetup.pic_control.l0[0]           = 0;

    for (UINT32 idx = 0; idx < 8; idx++)
    {
        picSetup.pic_control.temp_dist_l0[idx] = -1;
        picSetup.pic_control.temp_dist_l1[idx] = -1;

    }
    picSetup.pic_control.temp_dist_l0[0] = 1;

    for (UINT32 idx1 = 0; idx1 < 8; idx1++)
    {
        for (UINT32 idx2 = 0; idx2 < 8; idx2 ++)
        {
            if (idx2 == 0)
            {
                picSetup.pic_control.dist_scale_factor[idx1][idx2] = 256;
            }
            else
            {
                picSetup.pic_control.dist_scale_factor[idx1][idx2] = -1;
            }
        }
    }

    picSetup.pic_control.diff_pic_order_cnt_zero[0]      = 0xfefefefe;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]      = 0xfefefefe;
    picSetup.pic_control.pic_order_cnt_lsb               = 1;
    picSetup.pic_control.pic_type                        = 0;
    picSetup.pic_control.short_term_ref_pic_set_sps_flag = 1;
    picSetup.pic_control.nal_unit_type                   = 1;
    sliceControl.slice_type                              = 0;

    WritePicSetupSurface(1, picSetupSurface, &picSetup, &sliceControl, 1,
            &md_control, &qControl, &meControl);

    picSetup.pic_control.l0[0]             = 2;
    picSetup.pic_control.pic_order_cnt_lsb = 2;
    WritePicSetupSurface(2, picSetupSurface, &picSetup, &sliceControl, 1,
            &md_control, &qControl, &meControl);

    picSetup.pic_control.l0[0]             = 0;
    picSetup.pic_control.pic_order_cnt_lsb = 3;
    WritePicSetupSurface(3, picSetupSurface, &picSetup, &sliceControl, 1,
            &md_control, &qControl, &meControl);

    picSetup.pic_control.l0[0]             = 2;
    picSetup.pic_control.pic_order_cnt_lsb = 4;
    WritePicSetupSurface(4, picSetupSurface, &picSetup, &sliceControl, 1,
            &md_control, &qControl, &meControl);

    return OK;
}

static RC PrepareControlStructuresVP9Generic
(
    Surface2D *picSetupSurface,
    UINT32 className
)
{
    RC rc;
    const UINT32 width  = 192;
    const UINT32 height = 160;
    const UINT32 depth  = 8;
    const UINT32 encodeWidth  = ENCODE_WIDTH(width);
    const UINT32 encodeHeight = ENCODE_HEIGHT(height);
    UINT32 structOffset = static_cast<UINT32>(
        AlignUp<256>(sizeof(lwenc_vp9_drv_pic_setup_s)));
    lwenc_vp9_drv_pic_setup_s picSetup;
    memset(&picSetup, 0, sizeof(picSetup));

    SetMagicValue(className, &picSetup.magic);

    CHECK_RC(PrepareSurfaceCfg(&picSetup.refpic_cfg, className,
                               encodeWidth, encodeHeight, depth,
                               SURFTYPE_REFPIC, SURFFMT_TILED,
                               SURFCOLOR_YUV420, false));
    CHECK_RC(PrepareSurfaceCfg(&picSetup.input_cfg, className,
                               encodeWidth, encodeHeight, depth,
                               SURFTYPE_INPUT, SURFFMT_BL2,
                               SURFCOLOR_YUV420, false));
    picSetup.input_cfg.block_height  = 2;
    picSetup.input_cfg.input_bl_mode = 1;
    CHECK_RC(PrepareSurfaceCfg(&picSetup.outputpic_cfg, className,
                               encodeWidth, encodeHeight, depth,
                               SURFTYPE_REFPIC, SURFFMT_TILED,
                               SURFCOLOR_YUV420, false));

    picSetup.sps_data.chroma_format_idc                 = 1;
    picSetup.sps_data.log2_max_pic_order_cnt_lsb_minus4 = 4;
    picSetup.sps_data.min_lw_size                       = 2;
    picSetup.sps_data.max_lw_size                       = 2;
    picSetup.sps_data.min_tu_size                       = 1;
    picSetup.sps_data.max_tu_size                       = 3;
    picSetup.sps_data.max_transform_depth_inter         = 1;
    picSetup.sps_data.max_transform_depth_intra         = 1;
    picSetup.sps_data.num_short_term_ref_pic_sets       = 1;

    picSetup.pps_data.pps_loop_filter_across_slices_enabled_flag = 1;
    picSetup.pps_data.cabac_init_present_flag                    = 1;

    for (UINT32 idx = 0; idx < 8; idx++)
    {
        picSetup.h265_pic_control.temp_dist_l0[idx] = -1;
        picSetup.h265_pic_control.temp_dist_l1[idx] = -1;
        for (UINT32 jj = 0;
             jj < NUMELEMS(picSetup.h265_pic_control.dist_scale_factor[idx]);
             ++jj)
        {
            picSetup.h265_pic_control.dist_scale_factor[idx][jj] = -1;
        }
    }

    picSetup.h265_pic_control.diff_pic_order_cnt_zero[0]          = -1;
    picSetup.h265_pic_control.diff_pic_order_cnt_zero[1]          = -1;
    picSetup.h265_pic_control.slice_control_offset              = structOffset;
    structOffset += static_cast<UINT32>(
        AlignUp<256>(sizeof(lwenc_h265_slice_control_s)));
    picSetup.h265_pic_control.me_control_offset                 = structOffset;
    structOffset += static_cast<UINT32>(
        AlignUp<256>(sizeof(lwenc_h264_me_control_s)));
    picSetup.h265_pic_control.md_control_offset                 = structOffset;
    structOffset += static_cast<UINT32>(
        AlignUp<256>(sizeof(lwenc_h265_md_control_s)));
    picSetup.h265_pic_control.q_control_offset                  = structOffset;
    structOffset += static_cast<UINT32>(
        AlignUp<256>(sizeof(lwenc_h265_quant_control_s)));
    picSetup.h265_pic_control.hist_buf_size                     = 29952;
    picSetup.h265_pic_control.bitstream_buf_size                = 307200;
    picSetup.h265_pic_control.pic_type                          = 3;
    picSetup.h265_pic_control.ref_pic_flag                      = 1;
    picSetup.h265_pic_control.codec                             = 6;
    picSetup.h265_pic_control.ceil_log2_pic_size_in_ctbs        = 6;
    picSetup.h265_pic_control.slice_stat_offset                 = 0xffffffff;
    picSetup.h265_pic_control.mpec_stat_offset                  = 0xffffffff;
    picSetup.h265_pic_control.aq_stat_offset                    = 256;
    picSetup.h265_pic_control.sao_luma_mode                     = 31;
    picSetup.h265_pic_control.sao_chroma_mode                   = 31;
    picSetup.h265_pic_control.nal_unit_type                     = 19;
    picSetup.h265_pic_control.nuh_temporal_id_plus1             = 1;
    picSetup.h265_pic_control.short_term_rps.num_negative_pics  = 1;
    picSetup.h265_pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 1;
    picSetup.h265_pic_control.mode_decision_only_flag           = 1;

    picSetup.vp9_pic_control.pic_width          = width;
    picSetup.vp9_pic_control.pic_height         = height;
    picSetup.vp9_pic_control.refframe           = 1;
    picSetup.vp9_pic_control.filter_cfg.disable = 1;
    for (UINT32 ii = 0;
         ii < NUMELEMS(picSetup.vp9_pic_control.quant_cfg.qindex_seg); ++ii)
    {
        picSetup.vp9_pic_control.quant_cfg.qindex_seg[ii] = 32;
    }
    for (UINT32 ii = 0;
         ii < NUMELEMS(picSetup.vp9_pic_control.quant_cfg.round_seg); ++ii)
    {
        picSetup.vp9_pic_control.quant_cfg.round_seg[ii].round_chroma_ac   = 2;
        picSetup.vp9_pic_control.quant_cfg.round_seg[ii].round_chroma_dc   = 3;
        picSetup.vp9_pic_control.quant_cfg.round_seg[ii].round_luma_ac     = 3;
        picSetup.vp9_pic_control.quant_cfg.round_seg[ii].round_luma_dc     = 4;
        picSetup.vp9_pic_control.quant_cfg.round_seg[ii].round_delta_inter = 7;
    }
    picSetup.vp9_pic_control.smalle_cfg.temporal_mv_enable  = 0;
    picSetup.vp9_pic_control.smalle_cfg.transform_mode      = 4;
    picSetup.vp9_pic_control.smalle_cfg.mocomp_filter_type  = 1;

    picSetup.vp9_pic_control.bitstream_buf_size =
        picSetup.h265_pic_control.bitstream_buf_size;
    picSetup.vp9_pic_control.combined_linebuf_size =
        COMBINED_LINE_BUF_SIZE(width, height);
    picSetup.vp9_pic_control.filterline_linebuf_size =
        FILTER_LINE_BUF_SIZE(width, height);
    picSetup.vp9_pic_control.filtercol_linebuf_size  =
        FILTER_COL_LINE_BUF_SIZE(width, height);

    lwenc_h265_md_control_s md_control;
    memset(&md_control, 0, sizeof(md_control));
    md_control.h265_intra_luma4x4_mode_enable       = 0xffffffff;
    md_control.h265_intra_luma8x8_mode_enable       = 0xffffffff;
    md_control.h265_intra_luma16x16_mode_enable     = 0xffffffff;
    md_control.h265_intra_luma32x32_mode_enable     = 0xffffffff;
    md_control.h265_intra_luma_mode_enable_leftbits = 0x3fffffff;
    md_control.lw32x32_l0_part_2Nx2N_enable         = 1;
    md_control.intra_mode_16x16_enable              = 1;
    md_control.intra_mode_32x32_enable              = 1;
    md_control.h265_intra_chroma_mode_enable        = 0x1f;
    md_control.tusize_16x16_enable                  = 1;
    md_control.tusize_32x32_enable                  = 1;
    md_control.pskip_enable                         = 1;
    md_control.mv_cost_enable                       = 1;
    md_control.ip_search_mode                       = 7;

    lwenc_h265_quant_control_s qControl;
    memset(&qControl, 0, sizeof(qControl));
    qControl.qpp_run_vector_4x4    = 1387;
    qControl.qpp_run_vector_8x8[0] = 43775;
    qControl.qpp_run_vector_8x8[1] = 65450;
    qControl.qpp_run_vector_8x8[2] = 15;
    qControl.qpp_luma8x8_cost      = 15;
    qControl.qpp_luma16x16_cost    = 15;
    qControl.qpp_chroma_cost       = 15;

    lwenc_h265_slice_control_s sliceControl;
    memset(&sliceControl, 0, sizeof(sliceControl));
    sliceControl.num_ctus                   = encodeWidth/32 * encodeHeight/32;
    sliceControl.qp_avr                                       = 25;
    sliceControl.slice_type                                   = 2;
    sliceControl.slice_deblocking_filter_disabled_flag        = 1;
    sliceControl.slice_loop_filter_across_slices_enabled_flag = 1;
    sliceControl.qp_slice_max                                 = 51;

    lwenc_h264_me_control_s meControl;
    memset(&meControl, 0, sizeof(meControl));
    meControl.lambda_mode                  = 1;
    meControl.fps_mvcost                   = 1;
    meControl.sps_mvcost                   = 1;
    meControl.sps_cost_func                = 1;
    meControl.predsrc.self_temporal_search = 1;
    meControl.predsrc.self_temporal_enable = 1;
    meControl.predsrc.const_mv_stamp_l0    = 4;
    meControl.predsrc.const_mv_stamp_l1    = 4;
    meControl.predsrc.const_mv_search      = 1;
    meControl.predsrc.const_mv_enable      = 1;
    meControl.shape0.bitmask[0]            = 2132543488;
    meControl.shape0.bitmask[1]            = 2076;
    meControl.shape0.hor_adjust            = 1;
    meControl.shape0.ver_adjust            = 1;
    meControl.shape1.bitmask[0]            = 2132543488;
    meControl.shape1.bitmask[1]            = 2076;
    meControl.shape1.hor_adjust            = 1;
    meControl.shape1.ver_adjust            = 1;
    meControl.shape2.bitmask[0]            = 2132543488;
    meControl.shape2.bitmask[1]            = 2076;
    meControl.shape2.hor_adjust            = 1;
    meControl.shape2.ver_adjust            = 1;
    meControl.shape3.bitmask[0]            = 2132548745;
    meControl.shape3.bitmask[1]            = 0x81081c1c;
    meControl.shape3.hor_adjust            = 1;
    meControl.shape3.ver_adjust            = 1;
    meControl.mbc_mb_size                  = 110;

    if (className == LWC5B7_VIDEO_ENCODER || className == LWC4B7_VIDEO_ENCODER)
    {
        picSetup.h265_pic_control.chroma_skip_threshold_4x4 = 8;
        md_control.rdo_level                       = 1;
        md_control.skip_evaluate_enable_lw8        = 0;
        md_control.skip_evaluate_enable_lw16       = 0;
        md_control.skip_evaluate_enable_lw32       = 0;
        md_control.skip_evaluate_enable_lw64       = 0;
        md_control.tu_search_num_lw16              = 2;
        md_control.tu_search_num_lw32              = 1;
        md_control.intra_ssd_cnt_4x4               = 1;
        md_control.intra_ssd_cnt_8x8               = 1;
        md_control.intra_ssd_cnt_16x16             = 1;
        md_control.intra_ssd_cnt_32x32             = 1;
        md_control.bias_skip                       = 0;
        md_control.bias_intra_32x32                = 0;
        md_control.bias_intra_16x16                = 0;
        md_control.bias_intra_8x8                  = 0;
        md_control.bias_intra_4x4                  = 0;

        meControl.fps_quad_thresh_hold            = 0;
        meControl.predsrc.stamp_refidx1_stamp     = 0;
        meControl.predsrc.stamp_refidx2_stamp     = 0;
        meControl.predsrc.stamp_refidx3_stamp     = 0;
        meControl.predsrc.stamp_refidx4_stamp     = 0;
        meControl.predsrc.stamp_refidx5_stamp     = 0;
        meControl.predsrc.stamp_refidx6_stamp     = 0;
        meControl.predsrc.stamp_refidx7_stamp     = 0;
        meControl.sps_evaluate_merge_cand_num     = 5;
        meControl.spatial_hint_pattern            = 0xf;
        meControl.temporal_hint_pattern           = 0x3f;
        meControl.Lw16partDecisionMadeByFPP       = 0;
#ifdef MISRA_5_2
        meControl.fbm_op_winner_num_p_frame       = 1;
        meControl.fbm_op_winner_num_b_frame_l0    = 1;
        meControl.fbm_op_winner_num_b_frame_l1    = 1;
#else
        meControl.fbm_output_winner_num_p_frame   = 1;
        meControl.fbm_output_winner_num_b_frame_l0= 1;
        meControl.fbm_output_winner_num_b_frame_l1= 1;
#endif

        meControl.fbm_select_best_lw32_parttype_num=3;
        meControl.external_hint_order             = 1;
        meControl.coloc_hint_order                = 1;
        meControl.ct_threshold                    = 0;
        meControl.hint_type0                      = 0;
        meControl.hint_type1                      = 1;
        meControl.hint_type2                      = 2;
        meControl.hint_type3                      = 3;
        meControl.hint_type4                      = 4;
        meControl.pyramidal_hint_order            = 0;
        meControl.left_hint_delay_N               = 0;
    }

    WritePicSetupSurface(0, picSetupSurface, &picSetup, &sliceControl, 1,
            &md_control, &qControl, &meControl);

    picSetup.h265_pic_control.temp_dist_l0[0]                 = 1;
    picSetup.h265_pic_control.dist_scale_factor[0][0]         = 256;
    picSetup.h265_pic_control.pic_order_cnt_lsb               = 1;
    picSetup.h265_pic_control.pic_type                        = 0;
    picSetup.h265_pic_control.short_term_ref_pic_set_sps_flag = 1;
    picSetup.h265_pic_control.nal_unit_type                   = 1;
    picSetup.vp9_pic_control.type                             = 1;
    sliceControl.qp_avr                                       = 28;
    sliceControl.slice_type                                   = 0;
    WritePicSetupSurface(1, picSetupSurface, &picSetup, &sliceControl, 1,
            &md_control, &qControl, &meControl);

    picSetup.h265_pic_control.temp_dist_l0[0]                 = -1;
    picSetup.h265_pic_control.dist_scale_factor[0][0]         = -1;
    picSetup.h265_pic_control.pic_order_cnt_lsb               = 2;
    picSetup.h265_pic_control.pic_type                        = 3;
    picSetup.h265_pic_control.short_term_ref_pic_set_sps_flag = 0;
    picSetup.h265_pic_control.nal_unit_type                   = 19;
    picSetup.vp9_pic_control.type                             = 0;
    sliceControl.qp_avr                                       = 25;
    sliceControl.slice_type                                   = 2;
    WritePicSetupSurface(2, picSetupSurface, &picSetup, &sliceControl, 1,
            &md_control, &qControl, &meControl);

    picSetup.h265_pic_control.temp_dist_l0[0]                 = 1;
    picSetup.h265_pic_control.dist_scale_factor[0][0]         = 256;
    picSetup.h265_pic_control.pic_order_cnt_lsb               = 3;
    picSetup.h265_pic_control.pic_type                        = 0;
    picSetup.h265_pic_control.short_term_ref_pic_set_sps_flag = 1;
    picSetup.h265_pic_control.nal_unit_type                   = 1;
    picSetup.vp9_pic_control.type                             = 1;
    sliceControl.qp_avr                                       = 28;
    sliceControl.slice_type                                   = 0;
    WritePicSetupSurface(3, picSetupSurface, &picSetup, &sliceControl, 1,
            &md_control, &qControl, &meControl);

    picSetup.h265_pic_control.temp_dist_l0[0]                 = -1;
    picSetup.h265_pic_control.dist_scale_factor[0][0]         = -1;
    picSetup.h265_pic_control.pic_order_cnt_lsb               = 4;
    picSetup.h265_pic_control.pic_type                        = 3;
    picSetup.h265_pic_control.short_term_ref_pic_set_sps_flag = 0;
    picSetup.h265_pic_control.nal_unit_type                   = 19;
    picSetup.vp9_pic_control.type                             = 0;
    sliceControl.qp_avr                                       = 25;
    sliceControl.slice_type                                   = 2;
    WritePicSetupSurface(4, picSetupSurface, &picSetup, &sliceControl, 1,
            &md_control, &qControl, &meControl);

    MASSERT(structOffset <= maxControlStructsSize);
    return OK;
}

/// added for vp8
static RC PrepareControlStructuresVP8Generic(Surface2D* picSetupSurface, UINT32 className)
{
    RC rc;
    lwenc_vp8_drv_pic_setup_s picSetup;
    memset(&picSetup, 0, sizeof(picSetup));

    // **? SetMagicValue function uses the different magic value from
    // the magic value in the decoded traces
//    picSetup.magic = 0xc1b70006;
    SetMagicValue(className, &picSetup.magic);

    //**** Frame 0

    // width, height from DRVPIC_0000X.bin, depth assumed to be 8 bits based
    // on existing control structures (h264, h265)
    // Surface Format based on block_height = 16
    // assuming block_height is in bits therefore it's 2 -> SURFFMT_BL2

    /* just filling the new values with given traces without using the
    PrepareSurfaceCfg function */
    picSetup.refpic_cfg = { 175, 143, 12, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 108, 0 };
    picSetup.input_cfg = { 175, 143, 12, 12, 0, 0, 2, 0, 0, 1, 0, 0, 0, 0, 0 };
    picSetup.outputpic_cfg = { 175, 143, 12, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 108, 0 };

    // sequence_data
    picSetup.sequence_data.horiz_scale = 0;
    picSetup.sequence_data.vert_scale = 0;

    // picture data (0-valued fields are automatically ignored)
    picSetup.picture_data.frame_type                = 0;
    picSetup.picture_data.show_frame                = 1;
    picSetup.picture_data.filter_level              = 63;
    picSetup.picture_data.refresh_entropy_probs     = 1;
    picSetup.picture_data.refresh_last_frame        = 1;
    picSetup.picture_data.refresh_golden_frame      = 1;
    picSetup.picture_data.refresh_alt_ref_frame     = 1;
    picSetup.picture_data.base_qindex               = 63;
    picSetup.picture_data.segmentation_enabled      = 1;
    picSetup.picture_data.update_mb_segmentation_map = 1;

    // pic control
    picSetup.pic_control.hist_buf_size              = 14080;
    picSetup.pic_control.bitstream_buf_size         = 38144;
    picSetup.pic_control.bitstream_residual_buf_size = 190208;

    // me control
    picSetup.me_control.refinement_mode             = 1;
    picSetup.me_control.refine_on_search_enable     = 1;
    picSetup.me_control.fps_mvcost                  = 1;
    picSetup.me_control.sps_mvcost                  = 1;
    picSetup.me_control.sps_cost_func               = 1;
    picSetup.me_control.sps_filter                  = 2;
    picSetup.me_control.mc_filter                   = 2;
    picSetup.me_control.mbc_mb_size                 = 140;
    picSetup.me_control.tempmv_scale                = 0;

    //  - limit_mv
    picSetup.me_control.limit_mv.mv_limit_enable    = 1;
    picSetup.me_control.limit_mv.left_mvx_int       = -128;
    picSetup.me_control.limit_mv.top_mvy_int        = -128;
    picSetup.me_control.limit_mv.right_mvx_frac     = 3;
    picSetup.me_control.limit_mv.right_mvx_int      = 127;
    picSetup.me_control.limit_mv.bottom_mvy_frac    = 3;
    picSetup.me_control.limit_mv.bottom_mvy_int     = 127;

    //  - predsrc
    picSetup.me_control.predsrc.const_mv_search     = 1;
    picSetup.me_control.predsrc.const_mv_refine     = 1;
    picSetup.me_control.predsrc.const_mv_enable     = 1;

    //  - shape0
    picSetup.me_control.shape0.bitmask[0]           = 134217728;

    // md control
    picSetup.md_control.early_intra_mode_type_16x16dc   = 1;
    picSetup.md_control.early_intra_mode_type_16x16h    = 1;
    picSetup.md_control.early_intra_mode_type_16x16v    = 1;
    picSetup.md_control.nearest_mv_enable               = 1;
    picSetup.md_control.near_mv_enable                  = 1;
    picSetup.md_control.zero_mv_enable                  = 1;
    picSetup.md_control.new_mv_enable                   = 1;
    picSetup.md_control.above4x4_mv_enable              = 1;
    picSetup.md_control.left4x4_mv_enable               = 1;
    picSetup.md_control.zero4x4_mv_enable               = 1;
    picSetup.md_control.new4x4_mv_enable                = 1;
    picSetup.md_control.l0_part_16x16_enable            = 1;
    picSetup.md_control.l0_part_16x8_enable             = 1;
    picSetup.md_control.l0_part_8x16_enable             = 1;
    picSetup.md_control.l0_part_8x8_enable              = 1;
    picSetup.md_control.pskip_enable                    = 1;
    picSetup.md_control.explicit_eval_nearestmv         = 1;
    picSetup.md_control.explicit_eval_nearmv            = 1;
    picSetup.md_control.explicit_eval_left_topmv        = 1;
    picSetup.md_control.explicit_eval_left_bottommv     = 1;
    picSetup.md_control.explicit_eval_top_leftmv        = 1;
    picSetup.md_control.explicit_eval_top_rightmv       = 1;
    picSetup.md_control.explicit_eval_zeromv            = 1;
    picSetup.md_control.refframe_pic                    = 1;
    picSetup.md_control.intra_luma4x4_mode_enable       = 511;
    picSetup.md_control.intra_luma16x16_mode_enable     = 15;
    picSetup.md_control.intra_chroma_mode_enable        = 15;
    picSetup.md_control.intra_tm_mode_enable            = 1;
    picSetup.md_control.ip_search_mode                  = 7;
    picSetup.md_control.mv_cost_enable                  = 1;
    picSetup.md_control.lambda_multiplier_intra[0]      = 19;
    picSetup.md_control.lambda_multiplier_intra[1]      = 19;
    picSetup.md_control.lambda_multiplier_intra[2]      = 19;
    picSetup.md_control.lambda_multiplier_intra[3]      = 19;
    picSetup.md_control.mdpLambdaCoefMd                 = 38;
    picSetup.md_control.mdpLambdaCoefMv                 = 38;
    picSetup.md_control.bias_inter_8x8                  = -100;
    picSetup.md_control.bias_zeromv                     = 700;
    picSetup.md_control.bias_zero4x4mv                  = 400;

    // rate control
    /* all rate_control fields are 0, therefore skipped */

    // quant control
    picSetup.quant_control.qpp_run_vector_4x4           = 1387;
    picSetup.quant_control.qpp_luma8x8_cost             = 15;
    picSetup.quant_control.qpp_luma16x16_cost           = 15;
    picSetup.quant_control.qpp_chroma_cost              = 15;
    picSetup.quant_control.dz_4x4_YI[0]                 = 682;
    picSetup.quant_control.dz_4x4_YI[1]                 = 682;
    picSetup.quant_control.dz_4x4_YI[2]                 = 682;
    picSetup.quant_control.dz_4x4_YI[3]                 = 682;
    picSetup.quant_control.dz_4x4_YI[4]                 = 682;
    picSetup.quant_control.dz_4x4_YI[5]                 = 682;
    picSetup.quant_control.dz_4x4_YI[6]                 = 682;
    picSetup.quant_control.dz_4x4_YI[7]                 = 682;
    picSetup.quant_control.dz_4x4_YI[8]                 = 682;
    picSetup.quant_control.dz_4x4_YI[9]                 = 682;
    picSetup.quant_control.dz_4x4_YI[10]                = 682;
    picSetup.quant_control.dz_4x4_YI[11]                = 682;
    picSetup.quant_control.dz_4x4_YI[12]                = 682;
    picSetup.quant_control.dz_4x4_YI[13]                = 682;
    picSetup.quant_control.dz_4x4_YI[14]                = 682;
    picSetup.quant_control.dz_4x4_YI[15]                = 682;
    picSetup.quant_control.dz_4x4_YP[0]                 = 341;
    picSetup.quant_control.dz_4x4_YP[1]                 = 341;
    picSetup.quant_control.dz_4x4_YP[2]                 = 341;
    picSetup.quant_control.dz_4x4_YP[3]                 = 341;
    picSetup.quant_control.dz_4x4_YP[4]                 = 341;
    picSetup.quant_control.dz_4x4_YP[5]                 = 341;
    picSetup.quant_control.dz_4x4_YP[6]                 = 341;
    picSetup.quant_control.dz_4x4_YP[7]                 = 341;
    picSetup.quant_control.dz_4x4_YP[8]                 = 341;
    picSetup.quant_control.dz_4x4_YP[9]                 = 341;
    picSetup.quant_control.dz_4x4_YP[10]                = 341;
    picSetup.quant_control.dz_4x4_YP[11]                = 341;
    picSetup.quant_control.dz_4x4_YP[12]                = 341;
    picSetup.quant_control.dz_4x4_YP[13]                = 341;
    picSetup.quant_control.dz_4x4_YP[14]                = 341;
    picSetup.quant_control.dz_4x4_YP[15]                = 341;
    picSetup.quant_control.dz_4x4_CI                    = 682;
    picSetup.quant_control.dz_4x4_CP                    = 341;

    WritePicSetupSurface(0, picSetupSurface, &picSetup);

    // It seems the bins for frame 1 - 8 are all the same
    // therefore nothing needs to be changed.
    // *** Frame 1
    picSetup.picture_data.frame_type                    = 1;
    picSetup.picture_data.refresh_golden_frame          = 0;
    picSetup.picture_data.refresh_alt_ref_frame         = 0;
    picSetup.md_control.early_intra_mode_control        = 1;

    WritePicSetupSurface(1, picSetupSurface, &picSetup);

    // *** Frame 2

    WritePicSetupSurface(2, picSetupSurface, &picSetup);

    // *** Frame 3

    WritePicSetupSurface(3, picSetupSurface, &picSetup);

    // *** Frame 4

    WritePicSetupSurface(4, picSetupSurface, &picSetup);

    // *** Frame 5

    WritePicSetupSurface(5, picSetupSurface, &picSetup);

    // *** Frame 6

    WritePicSetupSurface(6, picSetupSurface, &picSetup);

    // *** Frame 7

    WritePicSetupSurface(7, picSetupSurface, &picSetup);

    // *** Frame 8

    WritePicSetupSurface(8, picSetupSurface, &picSetup);

    return rc;
}

static RC PrepareControlStructuresH265FourK40F
(
    Surface2D *picSetupSurface,
    UINT32 className
)
{
    lwenc_h265_drv_pic_setup_s picSetup;
    memset(&picSetup, 0, sizeof(picSetup));

    // all width and width are AlignUp<32>();
    constexpr UINT32 width = 3840;
    constexpr UINT32 height = 2176;

    static_assert(width <= MAX_WIDTH, "");
    static_assert(height <= MAX_HEIGHT, "");

    SetMagicValue(className, &picSetup.magic);

    picSetup.refpic_cfg = {
        width - 1,
        height - 1,
        width,
        width,
        0, 0, 0, 0, 0, 0,
        2,
        0, 0, 0, 0, 0
    };
    picSetup.input_cfg = picSetup.refpic_cfg;
    picSetup.outputpic_cfg = picSetup.refpic_cfg;

    picSetup.sps_data.chroma_format_idc                 = 1;
    picSetup.sps_data.log2_max_pic_order_cnt_lsb_minus4 = 4;
    picSetup.sps_data.min_lw_size                       = 1;
    picSetup.sps_data.max_lw_size                       = 2;
    picSetup.sps_data.max_tu_size                       = 3;
    picSetup.sps_data.max_transform_depth_inter         = 3;
    picSetup.sps_data.max_transform_depth_intra         = 0;
    picSetup.sps_data.amp_enabled_flag                  = 1;
    picSetup.sps_data.sample_adaptive_offset_enabled_flag = 1;
    picSetup.sps_data.num_short_term_ref_pic_sets       = 1;
    picSetup.pps_data.transform_skip_enabled_flag                = 1;
    picSetup.pps_data.pps_loop_filter_across_slices_enabled_flag = 1;
    picSetup.pps_data.lw_qp_delta_enabled_flag                   = 1;
    picSetup.pps_data.cabac_init_present_flag                    = 1;

    for (UINT32 idx = 0; idx < 8; idx++)
    {
        picSetup.pic_control.l0[idx] = -2;
        picSetup.pic_control.l1[idx] = -2;
    }

    picSetup.pic_control.pic_output_flag                         = 1;
    picSetup.pic_control.slice_control_offset                    = 21760;
    picSetup.pic_control.me_control_offset                       = 1024;
    picSetup.pic_control.md_control_offset                       = 13312;
    picSetup.pic_control.q_control_offset                        = 21504;
    picSetup.pic_control.hist_buf_size                           = HIST_BUF_SIZE(width, height);
    picSetup.pic_control.bitstream_buf_size                      = 20893696;
    picSetup.pic_control.bitstream_start_pos                     = 88;
    picSetup.pic_control.pic_type                                = 3;
    picSetup.pic_control.ref_pic_flag                            = 1;
    picSetup.pic_control.codec                                   = 5;
    picSetup.pic_control.ceil_log2_pic_size_in_ctbs              = 13;
    picSetup.pic_control.sao_luma_mode                           = 31;
    picSetup.pic_control.sao_chroma_mode                         = 31;
    picSetup.pic_control.slice_stat_offset                       = 256;
    picSetup.pic_control.mpec_stat_offset                        = 256;
    picSetup.pic_control.aq_stat_offset                          = 256;
    picSetup.pic_control.nal_unit_type                           = 19;
    picSetup.pic_control.nuh_temporal_id_plus1                   = 1;
    picSetup.pic_control.qpfifo                                  = 1;
    picSetup.pic_control.rc_pass2_offset                         = 69888;
    picSetup.pic_control.wp_control_offset                       = 25856;
    picSetup.pic_control.strips_in_frame                         = 1;
    picSetup.pic_control.act_stat_offset                         = 35072;

    picSetup.rate_control.QP[0]         = 28;
    picSetup.rate_control.QP[1]         = 31;
    picSetup.rate_control.QP[2]         = 25;
    picSetup.rate_control.maxQP[0]      = 51;
    picSetup.rate_control.maxQP[1]      = 51;
    picSetup.rate_control.maxQP[2]      = 51;
    picSetup.rate_control.gop_length    = 0xffffffff;

    lwenc_h265_md_control_s md_control;
    memset(&md_control, 0, sizeof(md_control));
    md_control.h265_intra_luma4x4_mode_enable       = 0xffffffff;
    md_control.h265_intra_luma8x8_mode_enable       = 0xffffffff;
    md_control.h265_intra_luma16x16_mode_enable     = 0xffffffff;
    md_control.h265_intra_luma32x32_mode_enable     = 0xffffffff;
    md_control.h265_intra_luma_mode_enable_leftbits = 4095;
    md_control.lw16x16_l0_part_2Nx2N_enable = 1;
    md_control.lw16x16_l0_part_2NxN_enable  = 1;
    md_control.lw16x16_l0_part_Nx2N_enable  = 1;
    md_control.lw16x16_l0_part_NxN_enable   = 1;
    md_control.lw32x32_l0_part_2Nx2N_enable = 1;
    md_control.lw32x32_l0_part_2NxN_enable  = 1;
    md_control.lw32x32_l0_part_Nx2N_enable  = 1;
    md_control.lw32x32_l0_part_2NxnU_enable = 1;
    md_control.lw32x32_l0_part_2NxnD_enable = 1;
    md_control.lw32x32_l0_part_nLx2N_enable = 1;
    md_control.lw32x32_l0_part_nRx2N_enable = 1;
    md_control.intra_mode_4x4_enable         = 1;
    md_control.intra_mode_8x8_enable         = 1;
    md_control.intra_mode_16x16_enable       = 1;
    md_control.intra_mode_32x32_enable       = 1;
    md_control.h265_intra_chroma_mode_enable = 31;
    md_control.inter_penalty_factor_for_ip1  = 15;
    md_control.tusize_4x4_enable             = 1;
    md_control.tusize_8x8_enable             = 1;
    md_control.tusize_16x16_enable           = 1;
    md_control.tusize_32x32_enable           = 1;
    md_control.pskip_enable                  = 1;
    md_control.tempmv_wt_spread_threshold    = 65535;
    md_control.tempmv_wt_distort_threshold   = 65535;
    md_control.mv_cost_enable                = 1;
    md_control.ip_search_mode                = 7;
    md_control.early_termination_ip1         = 1;

    lwenc_h265_quant_control_s qControl;
    memset(&qControl, 0, sizeof(qControl));
    qControl.qpp_run_vector_4x4    = 1387;
    qControl.qpp_run_vector_8x8[0] = 43775;
    qControl.qpp_run_vector_8x8[1] = 65450;
    qControl.qpp_run_vector_8x8[2] = 15;
    qControl.qpp_luma8x8_cost      = 15;
    qControl.qpp_luma16x16_cost    = 15;
    qControl.qpp_chroma_cost       = 15;

    lwenc_h265_slice_control_s sliceControl;
    memset(&sliceControl, 0, sizeof(sliceControl));
    sliceControl.num_ctus                                     = 8160;
    sliceControl.qp_avr                                       = 25;
    sliceControl.slice_type                                   = 2;
    sliceControl.slice_loop_filter_across_slices_enabled_flag = 1;
    sliceControl.slice_sao_luma_flag                          = 1;
    sliceControl.slice_sao_chroma_flag                        = 1;

    lwenc_h264_me_control_s meControl;
    memset(&meControl, 0, sizeof(meControl));
    meControl.refinement_mode              = 1;
    meControl.refine_on_search_enable      = 1;
    meControl.fps_mvcost                   = 1;
    meControl.sps_mvcost                   = 1;
    meControl.sps_cost_func                = 1;
    meControl.limit_mv.mv_limit_enable     = 1;
    meControl.limit_mv.left_mvx_int        = -1024;
    meControl.limit_mv.top_mvy_int         = -256;
    meControl.limit_mv.right_mvx_frac      = 3;
    meControl.limit_mv.right_mvx_int       = 1023;
    meControl.limit_mv.bottom_mvy_frac     = 3;
    meControl.limit_mv.bottom_mvy_int      = 255;
    meControl.predsrc.self_temporal_search = 1;
    meControl.predsrc.self_temporal_enable = 1;
    meControl.predsrc.self_spatial_search  = 1;
    meControl.predsrc.self_spatial_enable  = 1;
    meControl.predsrc.const_mv_stamp_l0    = 1;
    meControl.predsrc.const_mv_stamp_l1    = 1;
    meControl.predsrc.const_mv_search      = 1;
    meControl.predsrc.const_mv_enable      = 1;
    meControl.shape0.bitmask[0]            = 2132560136;
    meControl.shape0.bitmask[1]            = 543004;
    meControl.shape1.bitmask[0]            = 2134771785;
    meControl.shape1.bitmask[1]            = 4786238;

    WritePicSetupSurface(0, picSetupSurface, &picSetup, &sliceControl, 1,
            &md_control, &qControl, &meControl, allControlStructsSizes[H265_4K_40F]);

    picSetup.pic_control.l0[0]           = 0;

    for (UINT32 idx = 0; idx < 8; idx++)
    {
        picSetup.pic_control.temp_dist_l0[idx] = -1;
        picSetup.pic_control.temp_dist_l1[idx] = -1;

    }
    picSetup.pic_control.temp_dist_l0[0] = 1;

    for (UINT32 idx1 = 0; idx1 < 8; idx1++)
    {
        for (UINT32 idx2 = 0; idx2 < 8; idx2 ++)
        {
            if (idx2 == 0)
            {
                picSetup.pic_control.dist_scale_factor[idx1][idx2] = 256;
            }
            else
            {
                picSetup.pic_control.dist_scale_factor[idx1][idx2] = -1;
            }
        }
    }

    picSetup.pic_control.diff_pic_order_cnt_zero[0]      = 0xfefefefe;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]      = 0xfefefefe;
    picSetup.pic_control.pic_order_cnt_lsb               = 1;
    picSetup.pic_control.bitstream_start_pos             = 0;
    picSetup.pic_control.pic_type                        = 0;
    picSetup.pic_control.short_term_ref_pic_set_sps_flag = 1;
    picSetup.pic_control.nal_unit_type                   = 1;
    picSetup.pic_control.ref_pic_list_modification.NumPocTotalLwrr = 1;

    sliceControl.qp_avr                                  = 28;
    sliceControl.slice_type                              = 0;

    WritePicSetupSurface(1, picSetupSurface, &picSetup, &sliceControl, 1,
            &md_control, &qControl, &meControl, allControlStructsSizes[H265_4K_40F]);

    constexpr int totalFrames = 40;

    static_assert(totalFrames <= MAX_NUM_FRAMES, "");

    for (int i = 2; i < totalFrames; i++)
    {
        picSetup.pic_control.l0[0] = i % 2 == 0 ? 2 : 0;
        picSetup.pic_control.pic_order_cnt_lsb = i;
        WritePicSetupSurface(i, picSetupSurface, &picSetup, &sliceControl, 1,
            &md_control, &qControl, &meControl, allControlStructsSizes[H265_4K_40F]);
    }

    return OK;
}

static RC PrepareControlStructuresH265MultiRefBFrame(Surface2D *picSetupSurface, UINT32 className, UINT32 outBitstreamBufferSize)
{
    RC rc;
    lwenc_h265_drv_pic_setup_s picSetup;
    memset(&picSetup, 0, sizeof(picSetup));

    const UINT32 width  = 192;
    const UINT32 height = 160;

    SetMagicValue(className, &picSetup.magic);

    CHECK_RC(PrepareSurfaceCfg(&picSetup.refpic_cfg, className, 192, 160, 8,
                               SURFTYPE_REFPIC, SURFFMT_TILED,
                               SURFCOLOR_YUV420, false));
    picSetup.refpic_cfg.block_height = 2;
    picSetup.refpic_cfg.tiled_16x16  = 0;
    CHECK_RC(PrepareSurfaceCfg(&picSetup.input_cfg, className, 192, 160, 8,
                               SURFTYPE_INPUT, SURFFMT_BL2,
                               SURFCOLOR_YUV420, false));
    picSetup.input_cfg.block_height  = 2;
    picSetup.input_cfg.input_bl_mode = 1;
    CHECK_RC(PrepareSurfaceCfg(&picSetup.outputpic_cfg, className, 192, 160, 8,
                               SURFTYPE_REFPIC, SURFFMT_TILED,
                               SURFCOLOR_YUV420, false));
    picSetup.outputpic_cfg.block_height = 2;
    picSetup.outputpic_cfg.tiled_16x16  = 0;
    picSetup.sps_data.chroma_format_idc                 = 1;
    picSetup.sps_data.log2_max_pic_order_cnt_lsb_minus4 = 4;
    picSetup.sps_data.max_lw_size                       = 2;

    picSetup.sps_data.max_tu_size                       = 3;
    picSetup.sps_data.max_transform_depth_inter         = 3;
    picSetup.sps_data.max_transform_depth_intra         = 3;
    picSetup.sps_data.amp_enabled_flag                  = 1;
    picSetup.sps_data.num_short_term_ref_pic_sets       = 1;
    picSetup.sps_data.sample_adaptive_offset_enabled_flag = 1;

    picSetup.pps_data.transform_skip_enabled_flag                = 1;
    picSetup.pps_data.pps_loop_filter_across_slices_enabled_flag = 1;
    picSetup.pps_data.cabac_init_present_flag                    = 1;

    picSetup.rate_control.QP[0] = 28;
    picSetup.rate_control.QP[1] = 31;
    picSetup.rate_control.QP[2] = 25;

    picSetup.rate_control.maxQP[0] = 51;
    picSetup.rate_control.maxQP[1] = 51;
    picSetup.rate_control.maxQP[2] = 51;
    picSetup.rate_control.aqMode = 1;
    picSetup.rate_control.dump_aq_stats = 1;

    for (UINT32 idx = 0; idx < 8; idx++)
    {
        picSetup.pic_control.l0[idx] = -2;
        picSetup.pic_control.l1[idx] = -2;
    }

    picSetup.pic_control.slice_control_offset                    = sliceControlOffset;
    picSetup.pic_control.me_control_offset                       = meControlOffset;
    picSetup.pic_control.md_control_offset                       = mdControlOffset;
    picSetup.pic_control.q_control_offset                        = quantControlOffset;
    picSetup.pic_control.hist_buf_size                           = HIST_BUF_SIZE(width, height);
    picSetup.pic_control.bitstream_buf_size                      = outBitstreamBufferSize;
    picSetup.pic_control.pic_type                                = 3;
    picSetup.pic_control.ref_pic_flag                            = 1;
    picSetup.pic_control.codec                                   = 5;
    picSetup.pic_control.ceil_log2_pic_size_in_ctbs              = 5;
    picSetup.pic_control.sao_luma_mode                           = 31;
    picSetup.pic_control.sao_chroma_mode                         = 31;
    picSetup.pic_control.mpec_threshold                          = 1;
    picSetup.pic_control.slice_stat_offset                       = 64;
    picSetup.pic_control.mpec_stat_offset                        = 512;
    picSetup.pic_control.aq_stat_offset                          = 4352;
    picSetup.pic_control.nal_unit_type                           = 19;
    picSetup.pic_control.nuh_temporal_id_plus1                   = 1;
    picSetup.pic_control.wp_control_offset                       = 2048;
    picSetup.pic_control.chroma_skip_threshold_4x4               = 8;

    lwenc_h265_md_control_s md_control;
    memset(&md_control, 0, sizeof(md_control));
    md_control.h265_intra_luma4x4_mode_enable = 4294967295;
    md_control.h265_intra_luma8x8_mode_enable = 4294967295;
    md_control.h265_intra_luma16x16_mode_enable = 4294967295;
    md_control.h265_intra_luma32x32_mode_enable = 4294967295;
    md_control.h265_intra_luma_mode_enable_leftbits = 1073741823;
    md_control.lw16x16_l0_part_2Nx2N_enable = 1;
    md_control.lw16x16_l0_part_2NxN_enable = 1;
    md_control.lw16x16_l0_part_Nx2N_enable = 1;
    md_control.lw16x16_l0_part_NxN_enable = 1;
    md_control.lw32x32_l0_part_2Nx2N_enable = 1;
    md_control.lw32x32_l0_part_2NxN_enable = 1;
    md_control.lw32x32_l0_part_Nx2N_enable = 1;
    md_control.lw32x32_l0_part_2NxnU_enable = 1;
    md_control.lw32x32_l0_part_2NxnD_enable = 1;
    md_control.lw32x32_l0_part_nLx2N_enable = 1;
    md_control.lw32x32_l0_part_nRx2N_enable = 1;
    md_control.intra_mode_4x4_enable = 1;
    md_control.intra_mode_8x8_enable = 1;
    md_control.intra_mode_16x16_enable = 1;
    md_control.intra_mode_32x32_enable = 1;
    md_control.h265_intra_chroma_mode_enable = 31;
    md_control.tusize_4x4_enable = 1;
    md_control.tusize_8x8_enable = 1;
    md_control.tusize_16x16_enable = 1;
    md_control.tusize_32x32_enable = 1;
    md_control.pskip_enable = 1;
    md_control.mv_cost_enable = 1;
    md_control.ip_search_mode = 7;
    md_control.tu_search_num_lw16 = 2;
    md_control.tu_search_num_lw32 = 1;
    md_control.skip_evaluate_enable_lw8 = 0;
    md_control.skip_evaluate_enable_lw16 = 0;
    md_control.skip_evaluate_enable_lw32 = 0;
    md_control.skip_evaluate_enable_lw64 = 0;
    md_control.intra_ssd_cnt_4x4 = 2;
    md_control.intra_ssd_cnt_8x8 = 2;
    md_control.intra_ssd_cnt_16x16 = 2;
    md_control.intra_ssd_cnt_32x32 = 2;
    md_control.rdo_level = 1;

    lwenc_h265_quant_control_s qControl;
    memset(&qControl, 0, sizeof(qControl));
    qControl.qpp_run_vector_4x4 = 1387;
    qControl.qpp_run_vector_8x8[0] = 43775;
    qControl.qpp_run_vector_8x8[1] = 65450;
    qControl.qpp_run_vector_8x8[2] = 15;

    qControl.qpp_luma8x8_cost = 15;
    qControl.qpp_luma16x16_cost = 15;
    qControl.qpp_chroma_cost = 15;

    lwenc_h265_slice_control_s sliceControl;
    memset(&sliceControl, 0, sizeof(sliceControl));
    sliceControl.num_ctus = 30;
    sliceControl.qp_avr = 25;
    sliceControl.slice_type = 2;
    sliceControl.slice_loop_filter_across_slices_enabled_flag = 1;
    sliceControl.qp_slice_max = 51;
    sliceControl.slice_sao_luma_flag = 1;
    sliceControl.slice_sao_chroma_flag = 1;

    lwenc_h264_me_control_s meControl;
    memset(&meControl, 0, sizeof(meControl));
    meControl.refinement_mode              = 1;
    meControl.refine_on_search_enable      = 1;
    meControl.fps_mvcost                   = 1;
    meControl.sps_mvcost                   = 1;
    meControl.sps_cost_func                = 1;
    meControl.predsrc.self_temporal_search = 1;
    meControl.predsrc.self_temporal_enable = 1;
    meControl.predsrc.self_spatial_search  = 1;
    meControl.predsrc.self_spatial_enable  = 1;
    meControl.predsrc.const_mv_stamp_l0    = 3;
    meControl.predsrc.const_mv_search      = 1;
    meControl.predsrc.const_mv_enable      = 1;
    meControl.shape0.bitmask[0]            = 4278979584;
    meControl.shape0.bitmask[1]            = 3084;
    meControl.shape0.hor_adjust            = 1;
    meControl.shape0.ver_adjust            = 1;
    meControl.shape1.bitmask[0]            = 4278979584;
    meControl.shape1.bitmask[1]            = 3084;
    meControl.shape1.hor_adjust            = 1;
    meControl.shape1.ver_adjust            = 1;
    meControl.shape2.bitmask[0]            = 4278979584;
    meControl.shape2.bitmask[1]            = 3084;
    meControl.shape2.hor_adjust            = 1;
    meControl.shape2.ver_adjust            = 1;
    meControl.shape3.bitmask[0]            = 4280159244;
    meControl.shape3.bitmask[1]            = 789534;
    meControl.shape3.hor_adjust            = 1;
    meControl.shape3.ver_adjust            = 1;
    meControl.mbc_mb_size                  = 320;
    meControl.partDecisionMadeByFPP = 1;
    meControl.spatial_hint_pattern = 15;
    meControl.temporal_hint_pattern = 63;
    meControl.Lw16partDecisionMadeByFPP = 1;
#ifdef MISRA_5_2
    meControl.fbm_op_winner_num_p_frame = 4;
    meControl.fbm_op_winner_num_b_frame_l0 = 1;
    meControl.fbm_op_winner_num_b_frame_l1 = 1;
#else
    meControl.fbm_output_winner_num_p_frame = 4;
    meControl.fbm_output_winner_num_b_frame_l0 = 1;
    meControl.fbm_output_winner_num_b_frame_l1 = 1;
#endif
    meControl.fbm_select_best_lw32_parttype_num = 3;
    meControl.sps_evaluate_merge_cand_num = 5;
    meControl.coloc_hint_order = 1;
    meControl.hint_type0 = 2;
    meControl.hint_type1 = 4;
    meControl.hint_type2 = 3;
    meControl.hint_type3 = 1;

    WritePicSetupSurface(0, picSetupSurface, &picSetup, &sliceControl, 1,
            &md_control, &qControl, &meControl, allControlStructsSizes[H265_MULTIREF_BFRAME]);

    for (UINT32 idx = 0; idx < 8; idx++)
    {
        picSetup.pic_control.l0[idx] = 0;
        picSetup.pic_control.l1[idx] = -2;
        picSetup.pic_control.temp_dist_l0[idx] = 3;
        picSetup.pic_control.temp_dist_l1[idx] = -1;
    }

    picSetup.pic_control.dist_scale_factor[0][0] = 256;
    for (UINT32 idx = 1; idx < 8; idx++)
    {
        for (UINT32 idx1 = 0; idx1 < 8; idx1++)
        {
            picSetup.pic_control.dist_scale_factor[idx][idx1] = -1;
        }
    }

    picSetup.pic_control.pic_order_cnt_lsb = 3;
    picSetup.pic_control.pic_type = 0;
    picSetup.pic_control.nal_unit_type = 1;

    picSetup.pic_control.short_term_rps.num_negative_pics = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0] = 2;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 1;
    picSetup.pic_control.ref_pic_list_modification.NumPocTotalLwrr = 1;

    for (UINT32 idx = 0; idx < 8; idx++)
        picSetup.pic_control.poc_entry_l1[idx] = -1;

    picSetup.pic_control.same_poc[0] = 1;

    WritePicSetupSurface(1, picSetupSurface, &picSetup, &sliceControl, 1,
            &md_control, &qControl, &meControl, allControlStructsSizes[H265_MULTIREF_BFRAME]);

    picSetup.pic_control.l0[1] = 2;
    picSetup.pic_control.l0[3] = 2;
    picSetup.pic_control.l0[4] = -2;
    picSetup.pic_control.l0[5] = -2;
    picSetup.pic_control.l0[6] = -2;
    picSetup.pic_control.l0[7] = -2;

    picSetup.pic_control.l1[0] = 2;
    picSetup.pic_control.l1[1] = 0;
    picSetup.pic_control.l1[2] = 2;
    picSetup.pic_control.l1[3] = 0;

    picSetup.pic_control.temp_dist_l0[0] = 1;
    picSetup.pic_control.temp_dist_l0[1] = -2;
    picSetup.pic_control.temp_dist_l0[2] = 1;
    picSetup.pic_control.temp_dist_l0[3] = -2;

    for (UINT32 idx = 4; idx < 8; idx++)
        picSetup.pic_control.temp_dist_l0[idx] = -1;

    picSetup.pic_control.temp_dist_l1[0] = -2;
    picSetup.pic_control.temp_dist_l1[1] = 1;
    picSetup.pic_control.temp_dist_l1[2] = -2;
    picSetup.pic_control.temp_dist_l1[3] = 1;

    picSetup.pic_control.dist_scale_factor[0][1] = -512;

    picSetup.pic_control.dist_scale_factor[1][0] = -128;
    picSetup.pic_control.dist_scale_factor[1][1] =  256;

    for (UINT32 idx = 2; idx < 8; idx++)
        picSetup.pic_control.dist_scale_factor[1][idx] = 0;

    picSetup.pic_control.diff_pic_order_cnt_zero[0] = 84542730;
    picSetup.pic_control.diff_pic_order_cnt_zero[1] = 4042322160;

    picSetup.pic_control.pic_order_cnt_lsb = 1;
    picSetup.pic_control.pic_type = 1;
    picSetup.pic_control.ref_pic_flag = 0;

    picSetup.pic_control.short_term_rps.num_positive_pics = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0] = 0;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0] = 1;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 1;

    picSetup.pic_control.ref_pic_list_modification.NumPocTotalLwrr = 2;

    picSetup.pic_control.poc_entry_l0[1] = 1;
    picSetup.pic_control.poc_entry_l0[3] = 1;
    for (UINT32 idx = 4; idx < 8; idx++)
        picSetup.pic_control.poc_entry_l0[idx] = -1;

    picSetup.pic_control.poc_entry_l1[0] = 1;
    picSetup.pic_control.poc_entry_l1[1] = 0;
    picSetup.pic_control.poc_entry_l1[2] = 1;
    picSetup.pic_control.poc_entry_l1[3] = 0;

    picSetup.pic_control.same_poc[0] = 513;
    sliceControl.qp_avr = 31;
    sliceControl.slice_type = 1;
    sliceControl.num_ref_idx_active_override_flag = 0;
    sliceControl.num_ref_idx_l0_active_minus1 = 3;
    sliceControl.num_ref_idx_l1_active_minus1 = 3;

    WritePicSetupSurface(2, picSetupSurface, &picSetup, &sliceControl, 1,
            &md_control, &qControl, &meControl, allControlStructsSizes[H265_MULTIREF_BFRAME]);

    picSetup.pic_control.temp_dist_l0[0] =  2;
    picSetup.pic_control.temp_dist_l0[1] = -1;
    picSetup.pic_control.temp_dist_l0[2] =  2;
    picSetup.pic_control.temp_dist_l0[3] = -1;

    picSetup.pic_control.temp_dist_l1[0] = -1;
    picSetup.pic_control.temp_dist_l1[1] =  2;
    picSetup.pic_control.temp_dist_l1[2] = -1;
    picSetup.pic_control.temp_dist_l1[3] =  2;

    picSetup.pic_control.dist_scale_factor[0][1] = -128;
    picSetup.pic_control.dist_scale_factor[1][0] = -512;
    picSetup.pic_control.pic_order_cnt_lsb = 2;

    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0] = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0] = 0;

    WritePicSetupSurface(3, picSetupSurface, &picSetup, &sliceControl, 1,
            &md_control, &qControl, &meControl, allControlStructsSizes[H265_MULTIREF_BFRAME]);

    picSetup.pic_control.l0[0] = 2;
    picSetup.pic_control.l0[1] = 0;
    picSetup.pic_control.l0[2] = 2;
    picSetup.pic_control.l0[3] = 0;
    picSetup.pic_control.l0[4] = 2;
    picSetup.pic_control.l0[5] = 0;
    picSetup.pic_control.l0[6] = 2;
    picSetup.pic_control.l0[7] = 0;

    picSetup.pic_control.l1[0] = -2;
    picSetup.pic_control.l1[1] = -2;
    picSetup.pic_control.l1[2] = -2;
    picSetup.pic_control.l1[3] = -2;

    picSetup.pic_control.temp_dist_l0[0] =  3;
    picSetup.pic_control.temp_dist_l0[1] =  6;
    picSetup.pic_control.temp_dist_l0[2] =  3;
    picSetup.pic_control.temp_dist_l0[3] =  6;
    picSetup.pic_control.temp_dist_l0[4] =  3;
    picSetup.pic_control.temp_dist_l0[5] =  6;
    picSetup.pic_control.temp_dist_l0[6] =  3;
    picSetup.pic_control.temp_dist_l0[7] =  6;

    picSetup.pic_control.temp_dist_l1[1] = -1;
    picSetup.pic_control.temp_dist_l1[3] = -1;

    picSetup.pic_control.pic_order_cnt_lsb = 6;

    picSetup.pic_control.pic_type = 0;
    picSetup.pic_control.ref_pic_flag = 1;

    picSetup.pic_control.short_term_rps.num_negative_pics = 2;
    picSetup.pic_control.short_term_rps.num_positive_pics = 0;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0] = 2;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[1] = 2;

    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 3;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 0;

    picSetup.pic_control.poc_entry_l0[0] = 1;
    picSetup.pic_control.poc_entry_l0[1] = 0;
    picSetup.pic_control.poc_entry_l0[2] = 1;
    picSetup.pic_control.poc_entry_l0[3] = 0;
    picSetup.pic_control.poc_entry_l0[4] = 1;
    picSetup.pic_control.poc_entry_l0[5] = 0;
    picSetup.pic_control.poc_entry_l0[6] = 1;
    picSetup.pic_control.poc_entry_l0[7] = 0;

    picSetup.pic_control.poc_entry_l1[0] = -1;
    picSetup.pic_control.poc_entry_l1[1] = -1;
    picSetup.pic_control.poc_entry_l1[2] = -1;
    picSetup.pic_control.poc_entry_l1[3] = -1;

    sliceControl.qp_avr = 28;
    sliceControl.slice_type = 0;
    sliceControl.num_ref_idx_active_override_flag = 1;
    sliceControl.num_ref_idx_l0_active_minus1 = 7;
    sliceControl.num_ref_idx_l1_active_minus1 = 0;
    picSetup.pic_control.dist_scale_factor[0][1] =  128;
    picSetup.pic_control.dist_scale_factor[1][0] =  512;

    picSetup.pic_control.diff_pic_order_cnt_zero[0] = 0;
    picSetup.pic_control.diff_pic_order_cnt_zero[1] = 0;

    memset(&(picSetup.pic_control.short_term_rps.delta_poc_s1_minus1), 0, sizeof(picSetup.pic_control.short_term_rps.delta_poc_s1_minus1));

    WritePicSetupSurface(4, picSetupSurface, &picSetup, &sliceControl, 1,
            &md_control, &qControl, &meControl, allControlStructsSizes[H265_MULTIREF_BFRAME]);

    picSetup.pic_control.l0[2] =  8;
    picSetup.pic_control.l0[4] = -2;
    picSetup.pic_control.l0[5] = -2;
    picSetup.pic_control.l0[6] = -2;
    picSetup.pic_control.l0[7] = -2;

    picSetup.pic_control.l1[0] = 8;
    picSetup.pic_control.l1[1] = 2;
    picSetup.pic_control.l1[2] = 0;
    picSetup.pic_control.l1[3] = 8;

    picSetup.pic_control.temp_dist_l0[0] =   1;
    picSetup.pic_control.temp_dist_l0[1] =   4;
    picSetup.pic_control.temp_dist_l0[2] =  -2;
    picSetup.pic_control.temp_dist_l0[3] =   1;
    picSetup.pic_control.temp_dist_l0[4] =  -1;
    picSetup.pic_control.temp_dist_l0[5] =  -1;
    picSetup.pic_control.temp_dist_l0[6] =  -1;
    picSetup.pic_control.temp_dist_l0[7] =  -1;

    picSetup.pic_control.temp_dist_l1[0] =  -2;
    picSetup.pic_control.temp_dist_l1[1] =   1;
    picSetup.pic_control.temp_dist_l1[2] =   4;
    picSetup.pic_control.temp_dist_l1[3] =  -2;

    picSetup.pic_control.dist_scale_factor[0][1] =   64;
    picSetup.pic_control.dist_scale_factor[0][2] = -128;
    picSetup.pic_control.dist_scale_factor[1][0] = 1024;
    picSetup.pic_control.dist_scale_factor[1][2] = -512;
    picSetup.pic_control.dist_scale_factor[2][0] = -512;
    picSetup.pic_control.dist_scale_factor[2][1] = -128;
    picSetup.pic_control.dist_scale_factor[2][2] =  256;

    for (UINT32 idx=3; idx<8; idx++)
        picSetup.pic_control.dist_scale_factor[2][idx] = 0;

    picSetup.pic_control.pic_order_cnt_lsb = 4;
    picSetup.pic_control.pic_type = 1;
    picSetup.pic_control.ref_pic_flag = 0;

    picSetup.pic_control.short_term_rps.num_positive_pics = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0] = 0;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0] = 1;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 1;

    picSetup.pic_control.ref_pic_list_modification.NumPocTotalLwrr = 3;

    picSetup.pic_control.poc_entry_l0[2] = 2;
    for (UINT32 idx = 4; idx < 8; idx++)
        picSetup.pic_control.poc_entry_l0[idx] = -1;

    picSetup.pic_control.poc_entry_l1[0] = 2;
    picSetup.pic_control.poc_entry_l1[1] = 1;
    picSetup.pic_control.poc_entry_l1[2] = 0;
    picSetup.pic_control.poc_entry_l1[3] = 2;

    picSetup.pic_control.same_poc[0] = 262657;

    sliceControl.qp_avr = 31;
    sliceControl.slice_type = 1;
    sliceControl.num_ref_idx_active_override_flag = 0;
    sliceControl.num_ref_idx_l0_active_minus1 = 3;
    sliceControl.num_ref_idx_l1_active_minus1 = 3;

    WritePicSetupSurface(5, picSetupSurface, &picSetup, &sliceControl, 1,
            &md_control, &qControl, &meControl, allControlStructsSizes[H265_MULTIREF_BFRAME]);

    picSetup.pic_control.temp_dist_l0[0] =   2;
    picSetup.pic_control.temp_dist_l0[1] =   5;
    picSetup.pic_control.temp_dist_l0[2] =  -1;
    picSetup.pic_control.temp_dist_l0[3] =   2;

    picSetup.pic_control.temp_dist_l1[0] =  -1;
    picSetup.pic_control.temp_dist_l1[1] =   2;
    picSetup.pic_control.temp_dist_l1[2] =   5;
    picSetup.pic_control.temp_dist_l1[3] =  -1;

    picSetup.pic_control.dist_scale_factor[0][1] =   102;
    picSetup.pic_control.dist_scale_factor[0][2] =   -51;
    picSetup.pic_control.dist_scale_factor[1][0] =   640;
    picSetup.pic_control.dist_scale_factor[1][2] =  -128;
    picSetup.pic_control.dist_scale_factor[2][0] = -1280;
    picSetup.pic_control.dist_scale_factor[2][1] =  -512;

    picSetup.pic_control.pic_order_cnt_lsb = 5;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0] = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0] = 0;

    WritePicSetupSurface(6, picSetupSurface, &picSetup, &sliceControl, 1,
            &md_control, &qControl, &meControl, allControlStructsSizes[H265_MULTIREF_BFRAME]);

    return OK;
}


static RC PrepareControlStructuresOpticalFlow
(
    Surface2D *picSetupSurface,
    UINT32 className,
    UINT32,
    UINT32 numFrames,
    UINT32 outBitstreamBufferSize
)
{
    lwenc_h264_drv_pic_setup_s picSetup;
    memset(&picSetup, 0, sizeof(picSetup));

    const UINT32 width  = 320;
    const UINT32 height = 96;

    SetMagicValue(className, &picSetup.magic);

    picSetup.refpic_cfg.frame_width_minus1  = 319;
    picSetup.refpic_cfg.frame_height_minus1 = 95;
    picSetup.refpic_cfg.sfc_pitch           = 320;
    picSetup.refpic_cfg.block_height        = 2;

    picSetup.input_cfg.frame_width_minus1  = 319;
    picSetup.input_cfg.frame_height_minus1 = 95;
    picSetup.input_cfg.sfc_pitch           = 320;
    picSetup.input_cfg.block_height        = 2;
    picSetup.input_cfg.input_bl_mode       = 1;

    picSetup.outputpic_cfg.frame_width_minus1  = 319;
    picSetup.outputpic_cfg.frame_height_minus1 = 95;
    picSetup.outputpic_cfg.sfc_pitch           = 320;
    picSetup.outputpic_cfg.block_height        = 2;

    picSetup.sps_data.profile_idc                            = 100;
    picSetup.sps_data.level_idc                              = 51;
    picSetup.sps_data.chroma_format_idc                      = 1;
    picSetup.sps_data.pic_order_cnt_type                     = 2;
    picSetup.sps_data.log2_max_frame_num_minus4              = 4;
    picSetup.sps_data.log2_max_pic_order_cnt_lsb_minus4      = 4;
    picSetup.sps_data.frame_mbs_only                         = 1;
    picSetup.pps_data.deblocking_filter_control_present_flag = 1;

    boost::fill(picSetup.pic_control.l0, -2);
    picSetup.pic_control.l0[0] = 0;
    boost::fill(picSetup.pic_control.l1, -2);
    boost::fill(picSetup.pic_control.temp_dist_l0, -1);
    picSetup.pic_control.temp_dist_l0[0] = 1;
    boost::fill(picSetup.pic_control.temp_dist_l1, -1);

    for (size_t i = 0; boost::size(picSetup.pic_control.dist_scale_factor) > i; ++i)
    {
        picSetup.pic_control.dist_scale_factor[i][0] = 256;
        for (size_t j = 1; boost::size(picSetup.pic_control.dist_scale_factor[i]) > j; ++j)
        {
            picSetup.pic_control.dist_scale_factor[i][j] = -1;
        }
    }

    picSetup.pic_control.pic_order_cnt_lsb = 2;

    picSetup.pic_control.slice_control_offset = sliceControlOffset;
    picSetup.pic_control.me_control_offset    = meControlOffset;
    picSetup.pic_control.md_control_offset    = mdControlOffset;
    picSetup.pic_control.q_control_offset     = quantControlOffset;
    picSetup.pic_control.hist_buf_size        = HIST_BUF_SIZE(width, height);

    picSetup.pic_control.ref_pic_flag           = 1;
    picSetup.pic_control.cabac_zero_word_enable = 1;
    picSetup.pic_control.ofs_mode               = 1;
    picSetup.pic_control.slice_encoding_row_num = 6;

    lwenc_h264_slice_control_s sliceControl;
    memset(&sliceControl, 0, sizeof(sliceControl));

    sliceControl.num_mb = 120;
    sliceControl.qp_avr = 25;

    lwenc_h264_me_control_s meControl;
    memset(&meControl, 0, sizeof(meControl));

    meControl.refinement_mode                   = 1;
    meControl.lambda_mode                       = 1;
    meControl.refine_on_search_enable           = 1;
    meControl.me_only_mode                      = 1;
    meControl.fps_mvcost                        = 1;
    meControl.sps_filter                        = 1;
    meControl.mv_only_enable                    = 1;
    meControl.limit_mv.mv_limit_enable          = 1;
    meControl.limit_mv.left_mvx_int             = -2048;
    meControl.limit_mv.top_mvy_int              = -512;
    meControl.limit_mv.right_mvx_frac           = 3;
    meControl.limit_mv.right_mvx_int            = 2047;
    meControl.limit_mv.bottom_mvy_frac          = 3;
    meControl.limit_mv.bottom_mvy_int           = 511;
    meControl.predsrc.self_temporal_search      = 1;
    meControl.predsrc.self_temporal_refine      = 1;
    meControl.predsrc.self_spatial_search       = 1;
    meControl.predsrc.self_spatial_enable       = 1;
    meControl.predsrc.external_refine           = 1;
    meControl.predsrc.const_mv_search           = 1;
    meControl.predsrc.const_mv_enable           = 1;
    meControl.shape0.bitmask[0]                 = 0xff180000;
    meControl.shape0.bitmask[1]                 = 0x000081ff;
    meControl.mbc_mb_size                       = 320;
    meControl.spatial_hint_pattern              = 15;
    meControl.temporal_hint_pattern             = 63;
#ifdef MISRA_5_2
    meControl.fbm_op_winner_num_p_frame         = 1;
    meControl.fbm_op_winner_num_b_frame_l0      = 1;
    meControl.fbm_op_winner_num_b_frame_l1      = 1;
#else
    meControl.fbm_output_winner_num_p_frame     = 1;
    meControl.fbm_output_winner_num_b_frame_l0  = 1;
    meControl.fbm_output_winner_num_b_frame_l1  = 1;
#endif
    meControl.fbm_select_best_lw32_parttype_num = 3;
    meControl.sps_evaluate_merge_cand_num       = 5;
    meControl.external_hint_order               = 1;
    meControl.coloc_hint_order                  = 1;
    meControl.hint_type1                        = 1;
    meControl.hint_type2                        = 2;
    meControl.hint_type3                        = 3;
    meControl.hint_type4                        = 4;
    meControl.left_hint_delay_N                 = 6;

    lwenc_h264_md_control_s mdControl;
    memset(&mdControl, 0, sizeof(mdControl));

    mdControl.intra_luma4x4_mode_enable       = 511;
    mdControl.intra_luma16x16_mode_enable     = 15;
    mdControl.intra_chroma_mode_enable        = 15;
    mdControl.l0_part_16x16_enable            = 1;
    mdControl.l0_part_16x8_enable             = 1;
    mdControl.l0_part_8x16_enable             = 1;
    mdControl.l0_part_8x8_enable              = 1;
    mdControl.l0_part_8x4_enable              = 1;
    mdControl.l0_part_4x8_enable              = 1;
    mdControl.l0_part_4x4_enable              = 1;
    mdControl.l1_part_16x16_enable            = 1;
    mdControl.l1_part_16x8_enable             = 1;
    mdControl.l1_part_8x16_enable             = 1;
    mdControl.l1_part_8x8_enable              = 1;
    mdControl.bi_part_16x16_enable            = 1;
    mdControl.bi_part_16x8_enable             = 1;
    mdControl.bi_part_8x16_enable             = 1;
    mdControl.bi_part_8x8_enable              = 1;
    mdControl.intra_nxn_bias_multiplier       = 24;
    mdControl.intra_most_prob_bias_multiplier = 4;
    mdControl.tempmv_wt_spread_threshold      = 31;
    mdControl.tempmv_wt_distort_threshold     = 1;
    mdControl.mv_cost_enable                  = 1;
    mdControl.ip_search_mode                  = 7;
    mdControl.intra_ssd_cnt_4x4               = 1;
    mdControl.intra_ssd_cnt_8x8               = 1;
    mdControl.intra_ssd_cnt_16x16             = 1;
    mdControl.tu_search_num                   = 2;

    lwenc_h264_quant_control_s qControl;
    memset(&qControl, 0, sizeof(qControl));

    //In Optical Flow, two frames(current frame and previous frame as reference) are needed for
    //encoding by the LWENC engine. Hence, the frameIdx begins from 1. Original Frame 0
    //is given as a reference for frame 1
    WritePicSetupSurface(1, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.frame_num         = 1;
    meControl.predsrc.self_temporal_enable = 1;

    WritePicSetupSurface(2, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    return OK;
}

static RC PrepareControlStructuresStereo
(
    Surface2D *picSetupSurface,
    UINT32 className,
    UINT32,
    UINT32 numFrames,
    UINT32 outBitstreamBufferSize
)
{
    lwenc_h264_drv_pic_setup_s picSetup;
    memset(&picSetup, 0, sizeof(picSetup));

    const UINT32 width  = 320;
    const UINT32 height = 96;

    SetMagicValue(className, &picSetup.magic);

    picSetup.refpic_cfg.frame_width_minus1  = 319;
    picSetup.refpic_cfg.frame_height_minus1 = 95;
    picSetup.refpic_cfg.sfc_pitch           = 320;
    picSetup.refpic_cfg.block_height        = 2;

    picSetup.input_cfg.frame_width_minus1  = 319;
    picSetup.input_cfg.frame_height_minus1 = 95;
    picSetup.input_cfg.sfc_pitch           = 320;
    picSetup.input_cfg.block_height        = 2;
    picSetup.input_cfg.input_bl_mode       = 1;

    picSetup.outputpic_cfg.frame_width_minus1  = 319;
    picSetup.outputpic_cfg.frame_height_minus1 = 95;
    picSetup.outputpic_cfg.sfc_pitch           = 192;
    picSetup.outputpic_cfg.block_height        = 2;

    picSetup.sps_data.profile_idc                            = 100;
    picSetup.sps_data.level_idc                              = 51;
    picSetup.sps_data.chroma_format_idc                      = 1;
    picSetup.sps_data.pic_order_cnt_type                     = 2;
    picSetup.sps_data.log2_max_frame_num_minus4              = 4;
    picSetup.sps_data.log2_max_pic_order_cnt_lsb_minus4      = 4;
    picSetup.sps_data.frame_mbs_only                         = 1;
    picSetup.pps_data.deblocking_filter_control_present_flag = 1;

    boost::fill(picSetup.pic_control.l0, -2);
    picSetup.pic_control.l0[0] = 0;
    boost::fill(picSetup.pic_control.l1, -2);
    boost::fill(picSetup.pic_control.temp_dist_l0, -1);
    picSetup.pic_control.temp_dist_l0[0] = 1;
    boost::fill(picSetup.pic_control.temp_dist_l1, -1);

    for (size_t i = 0; boost::size(picSetup.pic_control.dist_scale_factor) > i; ++i)
    {
        picSetup.pic_control.dist_scale_factor[i][0] = 256;
        for (size_t j = 1; boost::size(picSetup.pic_control.dist_scale_factor[i]) > j; ++j)
        {
            picSetup.pic_control.dist_scale_factor[i][j] = -1;
        }
    }

    picSetup.pic_control.pic_order_cnt_lsb = 2;

    picSetup.pic_control.slice_control_offset = sliceControlOffset;
    picSetup.pic_control.me_control_offset    = meControlOffset;
    picSetup.pic_control.md_control_offset    = mdControlOffset;
    picSetup.pic_control.q_control_offset     = quantControlOffset;
    picSetup.pic_control.hist_buf_size        = HIST_BUF_SIZE(width, height);

    picSetup.pic_control.ref_pic_flag           = 1;
    picSetup.pic_control.cabac_zero_word_enable = 1;
    picSetup.pic_control.ofs_mode               = 2;
    picSetup.pic_control.slice_encoding_row_num = 6;

    lwenc_h264_slice_control_s sliceControl;
    memset(&sliceControl, 0, sizeof(sliceControl));

    sliceControl.num_mb = 120;
    sliceControl.qp_avr = 20;

    lwenc_h264_me_control_s meControl;
    memset(&meControl, 0, sizeof(meControl));

    meControl.refinement_mode                   = 1;
    meControl.lambda_mode                       = 1;
    meControl.refine_on_search_enable           = 1;
    meControl.me_only_mode                      = 1;
    meControl.fps_mvcost                        = 1;
    meControl.sps_mvcost                        = 1;
    meControl.sps_filter                        = 1;
    meControl.mv_only_enable                    = 1;
    meControl.limit_mv.mv_limit_enable          = 1;
    meControl.limit_mv.left_mvx_int             = -256;
    meControl.limit_mv.top_mvy_int              = -1;
    meControl.limit_mv.bottom_mvy_int           = 1;
    meControl.predsrc.self_spatial_search       = 1;
    meControl.predsrc.self_spatial_enable       = 1;
    meControl.predsrc.external_search           = 1;
    meControl.predsrc.external_refine           = 1;
    meControl.predsrc.const_mv_search           = 1;
    meControl.predsrc.const_mv_enable           = 1;
    meControl.shape0.bitmask[0]                 = 0x0f000000;
    meControl.shape0.bitmask[1]                 = 0x0000000f;
    meControl.mbc_mb_size                       = 320;
    meControl.spatial_hint_pattern              = 15;
    meControl.temporal_hint_pattern             = 63;
#ifdef MISRA_5_2
    meControl.fbm_op_winner_num_p_frame         = 1;
    meControl.fbm_op_winner_num_b_frame_l0      = 1;
    meControl.fbm_op_winner_num_b_frame_l1      = 1;
#else
    meControl.fbm_output_winner_num_p_frame     = 1;
    meControl.fbm_output_winner_num_b_frame_l0  = 1;
    meControl.fbm_output_winner_num_b_frame_l1  = 1;
#endif
    meControl.fbm_select_best_lw32_parttype_num = 3;
    meControl.sps_evaluate_merge_cand_num       = 5;
    meControl.external_hint_order               = 1;
    meControl.coloc_hint_order                  = 1;
    meControl.hint_type1                        = 1;
    meControl.hint_type2                        = 2;
    meControl.hint_type3                        = 3;
    meControl.hint_type4                        = 4;
    meControl.left_hint_delay_N                 = 6;

    lwenc_h264_md_control_s mdControl;
    memset(&mdControl, 0, sizeof(mdControl));

    mdControl.intra_luma4x4_mode_enable       = 511;
    mdControl.intra_luma16x16_mode_enable     = 15;
    mdControl.intra_chroma_mode_enable        = 15;
    mdControl.l0_part_16x16_enable            = 1;
    mdControl.l0_part_16x8_enable             = 1;
    mdControl.l0_part_8x16_enable             = 1;
    mdControl.l0_part_8x8_enable              = 1;
    mdControl.l0_part_8x4_enable              = 1;
    mdControl.l0_part_4x8_enable              = 1;
    mdControl.l0_part_4x4_enable              = 1;
    mdControl.l1_part_16x16_enable            = 1;
    mdControl.l1_part_16x8_enable             = 1;
    mdControl.l1_part_8x16_enable             = 1;
    mdControl.l1_part_8x8_enable              = 1;
    mdControl.bi_part_16x16_enable            = 1;
    mdControl.bi_part_16x8_enable             = 1;
    mdControl.bi_part_8x16_enable             = 1;
    mdControl.bi_part_8x8_enable              = 1;
    mdControl.intra_nxn_bias_multiplier       = 24;
    mdControl.intra_most_prob_bias_multiplier = 4;
    mdControl.mv_cost_enable                  = 1;
    mdControl.ip_search_mode                  = 7;
    mdControl.intra_ssd_cnt_4x4               = 1;
    mdControl.intra_ssd_cnt_8x8               = 1;
    mdControl.intra_ssd_cnt_16x16             = 1;
    mdControl.tu_search_num                   = 2;

    lwenc_h264_quant_control_s qControl;
    memset(&qControl, 0, sizeof(qControl));

    //In Stereo, two new frames are needed for
    //encoding by the LWENC engine. Hence, the frameIdx begins from 1. New Frame 0
    //is given as a reference for frame 1
    WritePicSetupSurface(1, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    return OK;
}

static RC PrepareControlStructuresH265Frames4K
(
    Surface2D *picSetupSurface,
    UINT32 className,
    UINT32 outBitstreamBufferSize
)
{
    // The trace was generated with the following command line:
    /*
    lwenc_cmod -csv -codec 5 -o test.h265 -dump_lwenc_top -i in_to_tree_2160p50.yuv -size 3840 2160
    -startf 0 -endf 49 -frame_to_dump 20 -if_dump_debug 1 -trace -trace-basename test -dump_stats 0
    -slice_stat_on -mpec_stat_on -mpec_threshold 1 -aq_enable 1 -dump_aq_stats 1 -optfile
    quality/config/argSets/H265LWENC/h265_firststream.opt -transform_skip_enable
    -fpp_part_decision_enable 1 -max_lw_size 2 -min_lw_size 0 -max_tu_size 3 -min_tu_size 0
    -max_tu_depth_intra 3 -intra_tusize_decision 0 -intra_mode_4x4_enable -intra_mode_8x8_enable
    -intra_mode_16x16_enable -intra_mode_32x32_enable -disable_deblocking_filter_idc 0
    -max_tu_depth_inter 3 -refinement_mode 1 -lambda_mode 0 -refine_on_search_enable 1
    -lw32x32_l0_part_2Nx2N_enable -lw32x32_l0_part_2NxN_enable -lw32x32_l0_part_Nx2N_enable
    -no_lw32x32_l0_part_NxN_enable -lw32x32_l0_part_2NxnU_enable -lw32x32_l0_part_2NxnD_enable
    -lw32x32_l0_part_nLx2N_enable -lw32x32_l0_part_nRx2N_enable -lw16x16_l0_part_2nx2n_enable
    -lw16x16_l0_part_2nxn_enable -lw16x16_l0_part_nx2n_enable -lw16x16_l0_part_nxn_enable
    -tusize_4x4_enable 1 -tusize_8x8_enable 1 -tusize_16x16_enable 1 -tusize_32x32_enable 1
    -self_temporal_enable 1 -self_temporal_search 1 -self_spatial_enable 1 -self_spatial_search 1
    -bias_intra_over_inter 0 -sao_enable -rdo_level 1 -fbm_output_winner_num_p_frame 4
    -skip_evaluate_enable_flag 1 1 1 1 -intra_ssd_cnt 2 2 2 2 -hint_type 2 4 3 1 0
    -external_hint_order 0 -ref_in_both_lists 1 -rdo_level 1 -use_new_dpb 1 -max_dpb_size 5
    -num_ref_l0 4 -num_ref_l1 4 -numb 2 -fpp_part_decision_enable 1 -lw16_fpp_part_decision_enable 1
    -b_as_ref 0
    */
    // then lwenlwtil was used to correct l0 and l1 from the generated video file

    RC rc;
    lwenc_h265_drv_pic_setup_s picSetup;
    memset(&picSetup, 0, sizeof(picSetup));

    const UINT32 width  = 3840;
    const UINT32 height = 2176;

    SetMagicValue(className, &picSetup.magic);

    CHECK_RC(PrepareSurfaceCfg(&picSetup.refpic_cfg, className, width, height, 8,
                               SURFTYPE_REFPIC, SURFFMT_TILED,
                               SURFCOLOR_YUV420, false));
    picSetup.refpic_cfg.block_height = 2;
    picSetup.refpic_cfg.tiled_16x16  = 0;
    CHECK_RC(PrepareSurfaceCfg(&picSetup.input_cfg, className, width, height, 8,
                               SURFTYPE_INPUT, SURFFMT_BL2,
                               SURFCOLOR_YUV420, false));
    picSetup.input_cfg.block_height  = 2;
    picSetup.input_cfg.input_bl_mode = 1;
    CHECK_RC(PrepareSurfaceCfg(&picSetup.outputpic_cfg, className, width, height, 8,
                               SURFTYPE_REFPIC, SURFFMT_TILED,
                               SURFCOLOR_YUV420, false));
    picSetup.outputpic_cfg.block_height = 2;
    picSetup.outputpic_cfg.tiled_16x16  = 0;

    picSetup.sps_data.chroma_format_idc                          = 1;
    picSetup.sps_data.log2_max_pic_order_cnt_lsb_minus4          = 4;
    picSetup.sps_data.max_lw_size                                = 2;
    picSetup.sps_data.max_tu_size                                = 3;
    picSetup.sps_data.max_transform_depth_inter                  = 3;
    picSetup.sps_data.max_transform_depth_intra                  = 3;
    picSetup.sps_data.amp_enabled_flag                           = 1;
    picSetup.sps_data.num_short_term_ref_pic_sets                = 1;
    picSetup.sps_data.sample_adaptive_offset_enabled_flag        = 1;
    picSetup.pps_data.transform_skip_enabled_flag                = 1;
    picSetup.pps_data.pps_loop_filter_across_slices_enabled_flag = 1;
    picSetup.pps_data.cabac_init_present_flag                    = 1;
    picSetup.rate_control.QP[0]                                  = 28;
    picSetup.rate_control.QP[1]                                  = 31;
    picSetup.rate_control.QP[2]                                  = 25;
    picSetup.rate_control.maxQP[0]                               = 51;
    picSetup.rate_control.maxQP[1]                               = 51;
    picSetup.rate_control.maxQP[2]                               = 51;
    picSetup.rate_control.aqMode                                 = 1;
    picSetup.rate_control.dump_aq_stats                          = 1;
    boost::fill(picSetup.pic_control.l0, -2);
    boost::fill(picSetup.pic_control.l1, -2);
    picSetup.pic_control.slice_control_offset                    = sliceControlOffset;
    picSetup.pic_control.me_control_offset                       = meControlOffset;
    picSetup.pic_control.md_control_offset                       = mdControlOffset;
    picSetup.pic_control.q_control_offset                        = quantControlOffset;
    picSetup.pic_control.hist_buf_size                           = 6312960;
    picSetup.pic_control.bitstream_buf_size                      = outBitstreamBufferSize;
    picSetup.pic_control.pic_type                                = 3;
    picSetup.pic_control.ref_pic_flag                            = 1;
    picSetup.pic_control.codec                                   = 5;
    picSetup.pic_control.ceil_log2_pic_size_in_ctbs              = 13;
    picSetup.pic_control.sao_luma_mode                           = 31;
    picSetup.pic_control.sao_chroma_mode                         = 31;
    picSetup.pic_control.mpec_threshold                          = 1;
    picSetup.pic_control.slice_stat_offset                       = 0x00000100;
    picSetup.pic_control.mpec_stat_offset                        = 0x00000200;
    picSetup.pic_control.aq_stat_offset                          = 1044992;
    picSetup.pic_control.nal_unit_type                           = 19;
    picSetup.pic_control.nuh_temporal_id_plus1                   = 1;
    picSetup.pic_control.wp_control_offset                       = 2048;
    picSetup.pic_control.chroma_skip_threshold_4x4               = 8;
    boost::fill(picSetup.pic_control.poc_entry_l0, -1);
    boost::fill(picSetup.pic_control.poc_entry_l1, -1);

    lwenc_h265_slice_control_s sliceControl;
    memset(&sliceControl, 0, sizeof(sliceControl));

    sliceControl.num_ctus                                        = 8160;
    sliceControl.qp_avr                                          = 25;
    sliceControl.slice_type                                      = 2;
    sliceControl.slice_loop_filter_across_slices_enabled_flag    = 1;
    sliceControl.qp_slice_max                                    = 51;
    sliceControl.slice_sao_luma_flag                             = 1;
    sliceControl.slice_sao_chroma_flag                           = 1;

    lwenc_h264_me_control_s meControl;
    memset(&meControl, 0, sizeof(meControl));

    meControl.refinement_mode                                    = 1;
    meControl.refine_on_search_enable                            = 1;
    meControl.fps_mvcost                                         = 1;
    meControl.sps_mvcost                                         = 1;
    meControl.sps_cost_func                                      = 1;
    meControl.predsrc.self_temporal_search                       = 1;
    meControl.predsrc.self_temporal_enable                       = 1;
    meControl.predsrc.self_spatial_search                        = 1;
    meControl.predsrc.self_spatial_enable                        = 1;
    meControl.predsrc.const_mv_stamp_l0                          = 3;
    meControl.predsrc.const_mv_search                            = 1;
    meControl.predsrc.const_mv_enable                            = 1;
    meControl.shape0.bitmask[0]                                  = 0xff0c0c00;
    meControl.shape0.bitmask[1]                                  = 0x00000c0c;
    meControl.shape0.hor_adjust                                  = 1;
    meControl.shape0.ver_adjust                                  = 1;
    meControl.shape1.bitmask[0]                                  = 0xff0c0c00;
    meControl.shape1.bitmask[1]                                  = 0x00000c0c;
    meControl.shape1.hor_adjust                                  = 1;
    meControl.shape1.ver_adjust                                  = 1;
    meControl.shape2.bitmask[0]                                  = 0xff0c0c00;
    meControl.shape2.bitmask[1]                                  = 0x00000c0c;
    meControl.shape2.hor_adjust                                  = 1;
    meControl.shape2.ver_adjust                                  = 1;
    meControl.shape3.bitmask[0]                                  = 0xff1e0c0c;
    meControl.shape3.bitmask[1]                                  = 0x000c0c1e;
    meControl.shape3.hor_adjust                                  = 1;
    meControl.shape3.ver_adjust                                  = 1;
    meControl.mbc_mb_size                                        = 320;
    meControl.partDecisionMadeByFPP                              = 1;
    meControl.spatial_hint_pattern                               = 15;
    meControl.temporal_hint_pattern                              = 63;
    meControl.Lw16partDecisionMadeByFPP                          = 1;
#ifdef MISRA_5_2
    meControl.fbm_op_winner_num_p_frame                          = 4;
    meControl.fbm_op_winner_num_b_frame_l0                       = 1;
    meControl.fbm_op_winner_num_b_frame_l1                       = 1;
#else
    meControl.fbm_output_winner_num_p_frame                      = 4;
    meControl.fbm_output_winner_num_b_frame_l0                   = 1;
    meControl.fbm_output_winner_num_b_frame_l1                   = 1;
#endif
    meControl.fbm_select_best_lw32_parttype_num                  = 3;
    meControl.sps_evaluate_merge_cand_num                        = 5;
    meControl.coloc_hint_order                                   = 1;
    meControl.hint_type0                                         = 2;
    meControl.hint_type1                                         = 4;
    meControl.hint_type2                                         = 3;
    meControl.hint_type3                                         = 1;

    lwenc_h265_md_control_s mdControl;
    memset(&mdControl, 0, sizeof(mdControl));

    mdControl.h265_intra_luma4x4_mode_enable                     = 0xFFFFFFFF;
    mdControl.h265_intra_luma8x8_mode_enable                     = 0xFFFFFFFF;
    mdControl.h265_intra_luma16x16_mode_enable                   = 0xFFFFFFFF;
    mdControl.h265_intra_luma32x32_mode_enable                   = 0xFFFFFFFF;
    mdControl.h265_intra_luma_mode_enable_leftbits               = 0x3FFFFFFF;
    mdControl.lw16x16_l0_part_2Nx2N_enable                       = 1;
    mdControl.lw16x16_l0_part_2NxN_enable                        = 1;
    mdControl.lw16x16_l0_part_Nx2N_enable                        = 1;
    mdControl.lw16x16_l0_part_NxN_enable                         = 1;
    mdControl.lw32x32_l0_part_2Nx2N_enable                       = 1;
    mdControl.lw32x32_l0_part_2NxN_enable                        = 1;
    mdControl.lw32x32_l0_part_Nx2N_enable                        = 1;
    mdControl.lw32x32_l0_part_2NxnU_enable                       = 1;
    mdControl.lw32x32_l0_part_2NxnD_enable                       = 1;
    mdControl.lw32x32_l0_part_nLx2N_enable                       = 1;
    mdControl.lw32x32_l0_part_nRx2N_enable                       = 1;
    mdControl.intra_mode_4x4_enable                              = 1;
    mdControl.intra_mode_8x8_enable                              = 1;
    mdControl.intra_mode_16x16_enable                            = 1;
    mdControl.intra_mode_32x32_enable                            = 1;
    mdControl.h265_intra_chroma_mode_enable                      = 31;
    mdControl.tusize_4x4_enable                                  = 1;
    mdControl.tusize_8x8_enable                                  = 1;
    mdControl.tusize_16x16_enable                                = 1;
    mdControl.tusize_32x32_enable                                = 1;
    mdControl.pskip_enable                                       = 1;
    mdControl.mv_cost_enable                                     = 1;
    mdControl.ip_search_mode                                     = 7;
    mdControl.tu_search_num_lw16                                 = 2;
    mdControl.tu_search_num_lw32                                 = 1;
    mdControl.skip_evaluate_enable_lw8                           = 1;
    mdControl.skip_evaluate_enable_lw16                          = 1;
    mdControl.skip_evaluate_enable_lw32                          = 1;
    mdControl.skip_evaluate_enable_lw64                          = 1;
    mdControl.intra_ssd_cnt_4x4                                  = 2;
    mdControl.intra_ssd_cnt_8x8                                  = 2;
    mdControl.intra_ssd_cnt_16x16                                = 2;
    mdControl.intra_ssd_cnt_32x32                                = 2;
    mdControl.rdo_level                                          = 1;

    lwenc_h265_quant_control_s qControl;
    memset(&qControl, 0, sizeof(qControl));
    qControl.qpp_run_vector_4x4                                  = 1387;
    qControl.qpp_run_vector_8x8[0]                               = 43775;
    qControl.qpp_run_vector_8x8[1]                               = 65450;
    qControl.qpp_run_vector_8x8[2]                               = 15;
    qControl.qpp_luma8x8_cost                                    = 15;
    qControl.qpp_luma16x16_cost                                  = 15;
    qControl.qpp_chroma_cost                                     = 15;

    WritePicSetupSurface(0, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    boost::fill(picSetup.pic_control.l0, 0);
    boost::fill(picSetup.pic_control.temp_dist_l0, 3);
    boost::fill(picSetup.pic_control.temp_dist_l1, -1);
    for (auto &e : picSetup.pic_control.dist_scale_factor) boost::fill(e, -1);
    picSetup.pic_control.dist_scale_factor[0][0] = 256;

    picSetup.pic_control.pic_order_cnt_lsb                         = 3;
    picSetup.pic_control.pic_type                                  = 0;
    picSetup.pic_control.nal_unit_type                             = 1;
    picSetup.pic_control.short_term_rps.num_negative_pics          = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]     = 2;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag   = 1;
    picSetup.pic_control.ref_pic_list_modification.NumPocTotalLwrr = 1;
    boost::fill(picSetup.pic_control.poc_entry_l0, 0);
    picSetup.pic_control.same_poc[0]                               = 1;
    sliceControl.qp_avr                                            = 28;
    sliceControl.slice_type                                        = 0;
    sliceControl.num_ref_idx_active_override_flag                  = 1;
    sliceControl.num_ref_idx_l0_active_minus1                      = 7;

    WritePicSetupSurface(1, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.l0[1]                                     = 2;
    picSetup.pic_control.l0[3]                                     = 2;
    picSetup.pic_control.l0[4]                                     = -2;
    picSetup.pic_control.l0[5]                                     = -2;
    picSetup.pic_control.l0[6]                                     = -2;
    picSetup.pic_control.l0[7]                                     = -2;
    picSetup.pic_control.l1[0]                                     = 2;
    picSetup.pic_control.l1[1]                                     = 0;
    picSetup.pic_control.l1[2]                                     = 2;
    picSetup.pic_control.l1[3]                                     = 0;
    picSetup.pic_control.temp_dist_l0[0]                           = 1;
    picSetup.pic_control.temp_dist_l0[1]                           = -2;
    picSetup.pic_control.temp_dist_l0[2]                           = 1;
    picSetup.pic_control.temp_dist_l0[3]                           = -2;
    picSetup.pic_control.temp_dist_l0[4]                           = -1;
    picSetup.pic_control.temp_dist_l0[5]                           = -1;
    picSetup.pic_control.temp_dist_l0[6]                           = -1;
    picSetup.pic_control.temp_dist_l0[7]                           = -1;
    picSetup.pic_control.temp_dist_l1[0]                           = -2;
    picSetup.pic_control.temp_dist_l1[1]                           = 1;
    picSetup.pic_control.temp_dist_l1[2]                           = -2;
    picSetup.pic_control.temp_dist_l1[3]                           = 1;
    picSetup.pic_control.dist_scale_factor[0][1]                   = -512;
    picSetup.pic_control.dist_scale_factor[1][0]                   = -128;
    picSetup.pic_control.dist_scale_factor[1][1]                   = 256;
    picSetup.pic_control.dist_scale_factor[1][2]                   = 0;
    picSetup.pic_control.dist_scale_factor[1][3]                   = 0;
    picSetup.pic_control.dist_scale_factor[1][4]                   = 0;
    picSetup.pic_control.dist_scale_factor[1][5]                   = 0;
    picSetup.pic_control.dist_scale_factor[1][6]                   = 0;
    picSetup.pic_control.dist_scale_factor[1][7]                   = 0;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]                = 0x050a050a;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]                = 0xf0f0f0f0;
    picSetup.pic_control.pic_order_cnt_lsb                         = 1;
    picSetup.pic_control.ref_pic_flag                              = 0;
    picSetup.pic_control.pic_type                                  = 1;
    picSetup.pic_control.nal_unit_type                             = 0;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]     = 0;
    picSetup.pic_control.short_term_rps.num_positive_pics          = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0]     = 1;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag   = 1;
    picSetup.pic_control.ref_pic_list_modification.NumPocTotalLwrr = 2;
    picSetup.pic_control.poc_entry_l0[1]                           = 1;
    picSetup.pic_control.poc_entry_l0[3]                           = 1;
    picSetup.pic_control.poc_entry_l0[4]                           = -1;
    picSetup.pic_control.poc_entry_l0[5]                           = -1;
    picSetup.pic_control.poc_entry_l0[6]                           = -1;
    picSetup.pic_control.poc_entry_l0[7]                           = -1;
    picSetup.pic_control.poc_entry_l1[0]                           = 1;
    picSetup.pic_control.poc_entry_l1[1]                           = 0;
    picSetup.pic_control.poc_entry_l1[2]                           = 1;
    picSetup.pic_control.poc_entry_l1[3]                           = 0;
    picSetup.pic_control.same_poc[0]                               = 513;
    sliceControl.qp_avr                                            = 31;
    sliceControl.slice_type                                        = 1;
    sliceControl.num_ref_idx_active_override_flag                  = 0;
    sliceControl.num_ref_idx_l0_active_minus1                      = 3;
    sliceControl.num_ref_idx_l1_active_minus1                      = 3;

    WritePicSetupSurface(2, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);    

    picSetup.pic_control.temp_dist_l0[0]                       = 2;
    picSetup.pic_control.temp_dist_l0[1]                       = -1;
    picSetup.pic_control.temp_dist_l0[2]                       = 2;
    picSetup.pic_control.temp_dist_l0[3]                       = -1;
    picSetup.pic_control.temp_dist_l1[0]                       = -1;
    picSetup.pic_control.temp_dist_l1[1]                       = 2;
    picSetup.pic_control.temp_dist_l1[2]                       = -1;
    picSetup.pic_control.temp_dist_l1[3]                       = 2;
    picSetup.pic_control.dist_scale_factor[0][1]               = -128;
    picSetup.pic_control.dist_scale_factor[1][0]               = -512;
    picSetup.pic_control.pic_order_cnt_lsb                     = 2;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0] = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0] = 0;

    WritePicSetupSurface(3, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.l0[1]                                   = 0;
    picSetup.pic_control.l0[3]                                   = 0;
    picSetup.pic_control.l0[0]                                   = 2;
    picSetup.pic_control.l0[2]                                   = 2;
    picSetup.pic_control.l0[4]                                   = 2;
    picSetup.pic_control.l0[5]                                   = 0;
    picSetup.pic_control.l0[6]                                   = 2;
    picSetup.pic_control.l0[7]                                   = 0;
    picSetup.pic_control.l1[0]                                   = -2;
    picSetup.pic_control.l1[1]                                   = -2;
    picSetup.pic_control.l1[2]                                   = -2;
    picSetup.pic_control.l1[3]                                   = -2;
    picSetup.pic_control.temp_dist_l0[0]                         = 3;
    picSetup.pic_control.temp_dist_l0[1]                         = 6;
    picSetup.pic_control.temp_dist_l0[2]                         = 3;
    picSetup.pic_control.temp_dist_l0[3]                         = 6;
    picSetup.pic_control.temp_dist_l0[4]                         = 3;
    picSetup.pic_control.temp_dist_l0[5]                         = 6;
    picSetup.pic_control.temp_dist_l0[6]                         = 3;
    picSetup.pic_control.temp_dist_l0[7]                         = 6;
    picSetup.pic_control.temp_dist_l1[1]                         = -1;
    picSetup.pic_control.temp_dist_l1[3]                         = -1;
    picSetup.pic_control.dist_scale_factor[0][1]                 = 128;
    picSetup.pic_control.dist_scale_factor[1][0]                 = 512;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 6;
    picSetup.pic_control.pic_type                                = 0;
    picSetup.pic_control.ref_pic_flag                            = 1;
    picSetup.pic_control.nal_unit_type                           = 1;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 2;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 0;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 2;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[1]   = 2;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 3;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 0;
    picSetup.pic_control.poc_entry_l0[1]                         = 0;
    picSetup.pic_control.poc_entry_l0[3]                         = 0;
    picSetup.pic_control.poc_entry_l0[0]                         = 1;
    picSetup.pic_control.poc_entry_l0[2]                         = 1;
    picSetup.pic_control.poc_entry_l0[4]                         = 1;
    picSetup.pic_control.poc_entry_l0[5]                         = 0;
    picSetup.pic_control.poc_entry_l0[6]                         = 1;
    picSetup.pic_control.poc_entry_l0[7]                         = 0;
    picSetup.pic_control.poc_entry_l1[0]                         = -1;
    picSetup.pic_control.poc_entry_l1[1]                         = -1;
    picSetup.pic_control.poc_entry_l1[2]                         = -1;
    picSetup.pic_control.poc_entry_l1[3]                         = -1;
    sliceControl.qp_avr                                          = 28;
    sliceControl.slice_type                                      = 0;
    sliceControl.num_ref_idx_active_override_flag                = 1;
    sliceControl.num_ref_idx_l0_active_minus1                    = 7;
    sliceControl.num_ref_idx_l1_active_minus1                    = 0;

    WritePicSetupSurface(4, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.l0[2]                                     = 8;
    picSetup.pic_control.l0[3]                                     = 2;
    picSetup.pic_control.l0[4]                                     = -2;
    picSetup.pic_control.l0[5]                                     = -2;
    picSetup.pic_control.l0[6]                                     = -2;
    picSetup.pic_control.l0[7]                                     = -2;
    picSetup.pic_control.l1[0]                                     = 8;
    picSetup.pic_control.l1[1]                                     = 2;
    picSetup.pic_control.l1[2]                                     = 0;
    picSetup.pic_control.l1[3]                                     = 8;
    picSetup.pic_control.temp_dist_l0[0]                           = 1;
    picSetup.pic_control.temp_dist_l0[1]                           = 4;
    picSetup.pic_control.temp_dist_l0[2]                           = -2;
    picSetup.pic_control.temp_dist_l0[3]                           = 1;
    picSetup.pic_control.temp_dist_l0[4]                           = -1;
    picSetup.pic_control.temp_dist_l0[5]                           = -1;
    picSetup.pic_control.temp_dist_l0[6]                           = -1;
    picSetup.pic_control.temp_dist_l0[7]                           = -1;
    picSetup.pic_control.temp_dist_l1[0]                           = -2;
    picSetup.pic_control.temp_dist_l1[1]                           = 1;
    picSetup.pic_control.temp_dist_l1[2]                           = 4;
    picSetup.pic_control.temp_dist_l1[3]                           = -2;
    picSetup.pic_control.dist_scale_factor[0][1]                   = 64;
    picSetup.pic_control.dist_scale_factor[0][2]                   = -128;
    picSetup.pic_control.dist_scale_factor[1][0]                   = 1024;
    picSetup.pic_control.dist_scale_factor[1][2]                   = -512;
    picSetup.pic_control.dist_scale_factor[2][0]                   = -512;
    picSetup.pic_control.dist_scale_factor[2][1]                   = -128;
    picSetup.pic_control.dist_scale_factor[2][2]                   = 256;
    picSetup.pic_control.dist_scale_factor[2][3]                   = 0;
    picSetup.pic_control.dist_scale_factor[2][4]                   = 0;
    picSetup.pic_control.dist_scale_factor[2][5]                   = 0;
    picSetup.pic_control.dist_scale_factor[2][6]                   = 0;
    picSetup.pic_control.dist_scale_factor[2][7]                   = 0;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]                = 0x04020904;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]                = 0xf0f0f0f0;
    picSetup.pic_control.pic_order_cnt_lsb                         = 4;
    picSetup.pic_control.ref_pic_flag                              = 0;
    picSetup.pic_control.pic_type                                  = 1;
    picSetup.pic_control.nal_unit_type                             = 0;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]     = 0;
    picSetup.pic_control.short_term_rps.num_positive_pics          = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0]     = 1;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag   = 1;
    picSetup.pic_control.ref_pic_list_modification.NumPocTotalLwrr = 3;
    picSetup.pic_control.poc_entry_l0[2]                           = 4;
    picSetup.pic_control.poc_entry_l0[3]                           = 1;
    picSetup.pic_control.poc_entry_l0[4]                           = -1;
    picSetup.pic_control.poc_entry_l0[5]                           = -1;
    picSetup.pic_control.poc_entry_l0[6]                           = -1;
    picSetup.pic_control.poc_entry_l0[7]                           = -1;
    picSetup.pic_control.poc_entry_l1[0]                           = 4;
    picSetup.pic_control.poc_entry_l1[1]                           = 1;
    picSetup.pic_control.poc_entry_l1[2]                           = 0;
    picSetup.pic_control.poc_entry_l1[3]                           = 4;
    picSetup.pic_control.same_poc[0]                               = 262657;
    sliceControl.qp_avr                                            = 31;
    sliceControl.slice_type                                        = 1;
    sliceControl.num_ref_idx_active_override_flag                  = 0;
    sliceControl.num_ref_idx_l0_active_minus1                      = 3;
    sliceControl.num_ref_idx_l1_active_minus1                      = 3;

    WritePicSetupSurface(5, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.temp_dist_l0[0]                       = 2;
    picSetup.pic_control.temp_dist_l0[1]                       = 5;
    picSetup.pic_control.temp_dist_l0[2]                       = -1;
    picSetup.pic_control.temp_dist_l0[3]                       = 2;
    picSetup.pic_control.temp_dist_l1[0]                       = -1;
    picSetup.pic_control.temp_dist_l1[1]                       = 2;
    picSetup.pic_control.temp_dist_l1[2]                       = 5;
    picSetup.pic_control.temp_dist_l1[3]                       = -1;
    picSetup.pic_control.dist_scale_factor[0][1]               = 102;
    picSetup.pic_control.dist_scale_factor[0][2]               = -51;
    picSetup.pic_control.dist_scale_factor[1][0]               = 640;
    picSetup.pic_control.dist_scale_factor[1][2]               = -128;
    picSetup.pic_control.dist_scale_factor[2][0]               = -1280;
    picSetup.pic_control.dist_scale_factor[2][1]               = -512;
    picSetup.pic_control.pic_order_cnt_lsb                     = 5;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0] = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0] = 0;

    WritePicSetupSurface(6, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.l0[0]                                   = 8;
    picSetup.pic_control.l0[2]                                   = 0;
    picSetup.pic_control.l0[1]                                   = 2;
    picSetup.pic_control.l0[3]                                   = 8;
    picSetup.pic_control.l0[4]                                   = 2;
    picSetup.pic_control.l0[5]                                   = 0;
    picSetup.pic_control.l0[6]                                   = 8;
    picSetup.pic_control.l0[7]                                   = 2;
    picSetup.pic_control.l1[0]                                   = -2;
    picSetup.pic_control.l1[1]                                   = -2;
    picSetup.pic_control.l1[2]                                   = -2;
    picSetup.pic_control.l1[3]                                   = -2;
    picSetup.pic_control.temp_dist_l0[0]                         = 3;
    picSetup.pic_control.temp_dist_l0[1]                         = 6;
    picSetup.pic_control.temp_dist_l0[2]                         = 9;
    picSetup.pic_control.temp_dist_l0[3]                         = 3;
    picSetup.pic_control.temp_dist_l0[4]                         = 6;
    picSetup.pic_control.temp_dist_l0[5]                         = 9;
    picSetup.pic_control.temp_dist_l0[6]                         = 3;
    picSetup.pic_control.temp_dist_l0[7]                         = 6;
    picSetup.pic_control.temp_dist_l1[1]                         = -1;
    picSetup.pic_control.temp_dist_l1[2]                         = -1;
    picSetup.pic_control.dist_scale_factor[0][1]                 = 171;
    picSetup.pic_control.dist_scale_factor[0][2]                 = 85;
    picSetup.pic_control.dist_scale_factor[1][0]                 = 384;
    picSetup.pic_control.dist_scale_factor[1][2]                 = 128;
    picSetup.pic_control.dist_scale_factor[2][0]                 = 768;
    picSetup.pic_control.dist_scale_factor[2][1]                 = 512;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 9;
    picSetup.pic_control.pic_type                                = 0;
    picSetup.pic_control.ref_pic_flag                            = 1;
    picSetup.pic_control.nal_unit_type                           = 1;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 3;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 0;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 2;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[2]   = 2;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 7;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 0;
    picSetup.pic_control.poc_entry_l0[0]                         = 4;
    picSetup.pic_control.poc_entry_l0[2]                         = 0;
    picSetup.pic_control.poc_entry_l0[1]                         = 1;
    picSetup.pic_control.poc_entry_l0[3]                         = 4;
    picSetup.pic_control.poc_entry_l0[4]                         = 1;
    picSetup.pic_control.poc_entry_l0[5]                         = 0;
    picSetup.pic_control.poc_entry_l0[6]                         = 4;
    picSetup.pic_control.poc_entry_l0[7]                         = 1;
    picSetup.pic_control.poc_entry_l1[0]                         = -1;
    picSetup.pic_control.poc_entry_l1[1]                         = -1;
    picSetup.pic_control.poc_entry_l1[2]                         = -1;
    picSetup.pic_control.poc_entry_l1[3]                         = -1;
    sliceControl.qp_avr                                          = 28;
    sliceControl.slice_type                                      = 0;
    sliceControl.num_ref_idx_active_override_flag                = 1;
    sliceControl.num_ref_idx_l0_active_minus1                    = 7;
    sliceControl.num_ref_idx_l1_active_minus1                    = 0;

    WritePicSetupSurface(7, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.l0[3]                                     = 4;
    picSetup.pic_control.l0[4]                                     = -2;
    picSetup.pic_control.l0[5]                                     = -2;
    picSetup.pic_control.l0[6]                                     = -2;
    picSetup.pic_control.l0[7]                                     = -2;
    picSetup.pic_control.l1[0]                                     = 4;
    picSetup.pic_control.l1[1]                                     = 8;
    picSetup.pic_control.l1[2]                                     = 2;
    picSetup.pic_control.l1[3]                                     = 0;
    picSetup.pic_control.temp_dist_l0[0]                           = 1;
    picSetup.pic_control.temp_dist_l0[1]                           = 4;
    picSetup.pic_control.temp_dist_l0[2]                           = 7;
    picSetup.pic_control.temp_dist_l0[3]                           = -2;
    picSetup.pic_control.temp_dist_l0[4]                           = -1;
    picSetup.pic_control.temp_dist_l0[5]                           = -1;
    picSetup.pic_control.temp_dist_l0[6]                           = -1;
    picSetup.pic_control.temp_dist_l0[7]                           = -1;
    picSetup.pic_control.temp_dist_l1[0]                           = -2;
    picSetup.pic_control.temp_dist_l1[1]                           = 1;
    picSetup.pic_control.temp_dist_l1[2]                           = 4;
    picSetup.pic_control.temp_dist_l1[3]                           = 7;
    picSetup.pic_control.dist_scale_factor[0][1]                   = 146;
    picSetup.pic_control.dist_scale_factor[0][2]                   = -73;
    picSetup.pic_control.dist_scale_factor[0][3]                   = 37;
    picSetup.pic_control.dist_scale_factor[1][0]                   = 448;
    picSetup.pic_control.dist_scale_factor[1][2]                   = -128;
    picSetup.pic_control.dist_scale_factor[1][3]                   = 64;
    picSetup.pic_control.dist_scale_factor[2][0]                   = -896;
    picSetup.pic_control.dist_scale_factor[2][1]                   = -512;
    picSetup.pic_control.dist_scale_factor[2][3]                   = -128;
    picSetup.pic_control.dist_scale_factor[3][0]                   = 1792;
    picSetup.pic_control.dist_scale_factor[3][1]                   = 1024;
    picSetup.pic_control.dist_scale_factor[3][2]                   = -512;
    picSetup.pic_control.dist_scale_factor[3][3]                   = 256;
    picSetup.pic_control.dist_scale_factor[3][4]                   = 0;
    picSetup.pic_control.dist_scale_factor[3][5]                   = 0;
    picSetup.pic_control.dist_scale_factor[3][6]                   = 0;
    picSetup.pic_control.dist_scale_factor[3][7]                   = 0;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]                = 0x04020108;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]                = 0xf0f0f0f0;
    picSetup.pic_control.pic_order_cnt_lsb                         = 7;
    picSetup.pic_control.ref_pic_flag                              = 0;
    picSetup.pic_control.pic_type                                  = 1;
    picSetup.pic_control.nal_unit_type                             = 0;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]     = 0;
    picSetup.pic_control.short_term_rps.num_positive_pics          = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0]     = 1;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag   = 1;
    picSetup.pic_control.ref_pic_list_modification.NumPocTotalLwrr = 4;
    picSetup.pic_control.poc_entry_l0[3]                           = 2;
    picSetup.pic_control.poc_entry_l0[4]                           = -1;
    picSetup.pic_control.poc_entry_l0[5]                           = -1;
    picSetup.pic_control.poc_entry_l0[6]                           = -1;
    picSetup.pic_control.poc_entry_l0[7]                           = -1;
    picSetup.pic_control.poc_entry_l1[0]                           = 2;
    picSetup.pic_control.poc_entry_l1[1]                           = 4;
    picSetup.pic_control.poc_entry_l1[2]                           = 1;
    picSetup.pic_control.poc_entry_l1[3]                           = 0;
    picSetup.pic_control.same_poc[0]                               = 134480385;
    sliceControl.qp_avr                                            = 31;
    sliceControl.slice_type                                        = 1;
    sliceControl.num_ref_idx_active_override_flag                  = 0;
    sliceControl.num_ref_idx_l0_active_minus1                      = 3;
    sliceControl.num_ref_idx_l1_active_minus1                      = 3;

    WritePicSetupSurface(8, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.temp_dist_l0[0]                       = 2;
    picSetup.pic_control.temp_dist_l0[1]                       = 5;
    picSetup.pic_control.temp_dist_l0[2]                       = 8;
    picSetup.pic_control.temp_dist_l0[3]                       = -1;
    picSetup.pic_control.temp_dist_l1[0]                       = -1;
    picSetup.pic_control.temp_dist_l1[1]                       = 2;
    picSetup.pic_control.temp_dist_l1[2]                       = 5;
    picSetup.pic_control.temp_dist_l1[3]                       = 8;
    picSetup.pic_control.dist_scale_factor[0][1]               = 160;
    picSetup.pic_control.dist_scale_factor[0][2]               = -32;
    picSetup.pic_control.dist_scale_factor[0][3]               = 64;
    picSetup.pic_control.dist_scale_factor[1][0]               = 410;
    picSetup.pic_control.dist_scale_factor[1][2]               = -51;
    picSetup.pic_control.dist_scale_factor[1][3]               = 102;
    picSetup.pic_control.dist_scale_factor[2][0]               = -2048;
    picSetup.pic_control.dist_scale_factor[2][1]               = -1280;
    picSetup.pic_control.dist_scale_factor[2][3]               = -512;
    picSetup.pic_control.dist_scale_factor[3][0]               = 1024;
    picSetup.pic_control.dist_scale_factor[3][1]               = 640;
    picSetup.pic_control.dist_scale_factor[3][2]               = -128;
    picSetup.pic_control.pic_order_cnt_lsb                     = 8;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0] = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0] = 0;

    WritePicSetupSurface(9, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                             &qControl, &meControl);

    picSetup.pic_control.l0[0]                                   = 4;
    picSetup.pic_control.l0[1]                                   = 8;
    picSetup.pic_control.l0[3]                                   = 0;
    picSetup.pic_control.l0[2]                                   = 2;
    picSetup.pic_control.l0[4]                                   = 4;
    picSetup.pic_control.l0[5]                                   = 8;
    picSetup.pic_control.l0[6]                                   = 2;
    picSetup.pic_control.l0[7]                                   = 0;
    picSetup.pic_control.l1[0]                                   = -2;
    picSetup.pic_control.l1[1]                                   = -2;
    picSetup.pic_control.l1[2]                                   = -2;
    picSetup.pic_control.l1[3]                                   = -2;
    picSetup.pic_control.temp_dist_l0[0]                         = 3;
    picSetup.pic_control.temp_dist_l0[1]                         = 6;
    picSetup.pic_control.temp_dist_l0[2]                         = 9;
    picSetup.pic_control.temp_dist_l0[3]                         = 12;
    picSetup.pic_control.temp_dist_l0[4]                         = 3;
    picSetup.pic_control.temp_dist_l0[5]                         = 6;
    picSetup.pic_control.temp_dist_l0[6]                         = 9;
    picSetup.pic_control.temp_dist_l0[7]                         = 12;
    picSetup.pic_control.temp_dist_l1[1]                         = -1;
    picSetup.pic_control.temp_dist_l1[2]                         = -1;
    picSetup.pic_control.temp_dist_l1[3]                         = -1;
    picSetup.pic_control.dist_scale_factor[0][1]                 = 192;
    picSetup.pic_control.dist_scale_factor[0][2]                 = 64;
    picSetup.pic_control.dist_scale_factor[0][3]                 = 128;
    picSetup.pic_control.dist_scale_factor[1][0]                 = 341;
    picSetup.pic_control.dist_scale_factor[1][2]                 = 85;
    picSetup.pic_control.dist_scale_factor[1][3]                 = 171;
    picSetup.pic_control.dist_scale_factor[2][0]                 = 1024;
    picSetup.pic_control.dist_scale_factor[2][1]                 = 768;
    picSetup.pic_control.dist_scale_factor[2][3]                 = 512;
    picSetup.pic_control.dist_scale_factor[3][0]                 = 512;
    picSetup.pic_control.dist_scale_factor[3][1]                 = 384;
    picSetup.pic_control.dist_scale_factor[3][2]                 = 128;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 12;
    picSetup.pic_control.pic_type                                = 0;
    picSetup.pic_control.ref_pic_flag                            = 1;
    picSetup.pic_control.nal_unit_type                           = 1;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 4;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 0;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 2;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 2;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 15;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 0;
    picSetup.pic_control.poc_entry_l0[0]                         = 2;
    picSetup.pic_control.poc_entry_l0[1]                         = 4;
    picSetup.pic_control.poc_entry_l0[3]                         = 0;
    picSetup.pic_control.poc_entry_l0[2]                         = 1;
    picSetup.pic_control.poc_entry_l0[4]                         = 2;
    picSetup.pic_control.poc_entry_l0[5]                         = 4;
    picSetup.pic_control.poc_entry_l0[6]                         = 1;
    picSetup.pic_control.poc_entry_l0[7]                         = 0;
    picSetup.pic_control.poc_entry_l1[0]                         = -1;
    picSetup.pic_control.poc_entry_l1[1]                         = -1;
    picSetup.pic_control.poc_entry_l1[2]                         = -1;
    picSetup.pic_control.poc_entry_l1[3]                         = -1;
    sliceControl.qp_avr                                          = 28;
    sliceControl.slice_type                                      = 0;
    sliceControl.num_ref_idx_active_override_flag                = 1;
    sliceControl.num_ref_idx_l0_active_minus1                    = 7;
    sliceControl.num_ref_idx_l1_active_minus1                    = 0;

    WritePicSetupSurface(10, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.l0[3]                                   = 6;
    picSetup.pic_control.l0[4]                                   = -2;
    picSetup.pic_control.l0[5]                                   = -2;
    picSetup.pic_control.l0[6]                                   = -2;
    picSetup.pic_control.l0[7]                                   = -2;
    picSetup.pic_control.l1[0]                                   = 6;
    picSetup.pic_control.l1[1]                                   = 4;
    picSetup.pic_control.l1[2]                                   = 8;
    picSetup.pic_control.l1[3]                                   = 2;
    picSetup.pic_control.temp_dist_l0[0]                         = 1;
    picSetup.pic_control.temp_dist_l0[1]                         = 4;
    picSetup.pic_control.temp_dist_l0[2]                         = 7;
    picSetup.pic_control.temp_dist_l0[3]                         = -2;
    picSetup.pic_control.temp_dist_l0[4]                         = -1;
    picSetup.pic_control.temp_dist_l0[5]                         = -1;
    picSetup.pic_control.temp_dist_l0[6]                         = -1;
    picSetup.pic_control.temp_dist_l0[7]                         = -1;
    picSetup.pic_control.temp_dist_l1[0]                         = -2;
    picSetup.pic_control.temp_dist_l1[1]                         = 1;
    picSetup.pic_control.temp_dist_l1[2]                         = 4;
    picSetup.pic_control.temp_dist_l1[3]                         = 7;
    picSetup.pic_control.dist_scale_factor[0][1]                 = 37;
    picSetup.pic_control.dist_scale_factor[0][2]                 = -73;
    picSetup.pic_control.dist_scale_factor[0][3]                 = 146;
    picSetup.pic_control.dist_scale_factor[1][0]                 = 1792;
    picSetup.pic_control.dist_scale_factor[1][2]                 = -512;
    picSetup.pic_control.dist_scale_factor[1][3]                 = 1024;
    picSetup.pic_control.dist_scale_factor[2][0]                 = -896;
    picSetup.pic_control.dist_scale_factor[2][1]                 = -128;
    picSetup.pic_control.dist_scale_factor[2][3]                 = -512;
    picSetup.pic_control.dist_scale_factor[3][0]                 = 448;
    picSetup.pic_control.dist_scale_factor[3][1]                 = 64;
    picSetup.pic_control.dist_scale_factor[3][2]                 = -128;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0x04020108;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0xf0f0f0f0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 10;
    picSetup.pic_control.ref_pic_flag                            = 0;
    picSetup.pic_control.pic_type                                = 1;
    picSetup.pic_control.nal_unit_type                           = 0;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 3;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 0;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 0;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 7;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0]   = 1;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 1;
    picSetup.pic_control.poc_entry_l0[3]                         = 3;
    picSetup.pic_control.poc_entry_l0[4]                         = -1;
    picSetup.pic_control.poc_entry_l0[5]                         = -1;
    picSetup.pic_control.poc_entry_l0[6]                         = -1;
    picSetup.pic_control.poc_entry_l0[7]                         = -1;
    picSetup.pic_control.poc_entry_l1[0]                         = 3;
    picSetup.pic_control.poc_entry_l1[1]                         = 2;
    picSetup.pic_control.poc_entry_l1[2]                         = 4;
    picSetup.pic_control.poc_entry_l1[3]                         = 1;
    sliceControl.qp_avr                                          = 31;
    sliceControl.slice_type                                      = 1;
    sliceControl.num_ref_idx_active_override_flag                = 0;
    sliceControl.num_ref_idx_l0_active_minus1                    = 3;
    sliceControl.num_ref_idx_l1_active_minus1                    = 3;

    WritePicSetupSurface(11, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);
    picSetup.pic_control.temp_dist_l0[0]                       = 2;
    picSetup.pic_control.temp_dist_l0[1]                       = 5;
    picSetup.pic_control.temp_dist_l0[2]                       = 8;
    picSetup.pic_control.temp_dist_l0[3]                       = -1;
    picSetup.pic_control.temp_dist_l1[0]                       = -1;
    picSetup.pic_control.temp_dist_l1[1]                       = 2;
    picSetup.pic_control.temp_dist_l1[2]                       = 5;
    picSetup.pic_control.temp_dist_l1[3]                       = 8;
    picSetup.pic_control.dist_scale_factor[0][1]               = 64;
    picSetup.pic_control.dist_scale_factor[0][2]               = -32;
    picSetup.pic_control.dist_scale_factor[0][3]               = 160;
    picSetup.pic_control.dist_scale_factor[1][0]               = 1024;
    picSetup.pic_control.dist_scale_factor[1][2]               = -128;
    picSetup.pic_control.dist_scale_factor[1][3]               = 640;
    picSetup.pic_control.dist_scale_factor[2][0]               = -2048;
    picSetup.pic_control.dist_scale_factor[2][1]               = -512;
    picSetup.pic_control.dist_scale_factor[2][3]               = -1280;
    picSetup.pic_control.dist_scale_factor[3][0]               = 410;
    picSetup.pic_control.dist_scale_factor[3][1]               = 102;
    picSetup.pic_control.dist_scale_factor[3][2]               = -51;
    picSetup.pic_control.pic_order_cnt_lsb                     = 11;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0] = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0] = 0;

    WritePicSetupSurface(12, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.l0[0]                                   = 6;
    picSetup.pic_control.l0[1]                                   = 4;
    picSetup.pic_control.l0[2]                                   = 8;
    picSetup.pic_control.l0[3]                                   = 2;
    picSetup.pic_control.l0[4]                                   = 6;
    picSetup.pic_control.l0[5]                                   = 4;
    picSetup.pic_control.l0[6]                                   = 8;
    picSetup.pic_control.l0[7]                                   = 2;
    picSetup.pic_control.l1[0]                                   = -2;
    picSetup.pic_control.l1[1]                                   = -2;
    picSetup.pic_control.l1[2]                                   = -2;
    picSetup.pic_control.l1[3]                                   = -2;
    picSetup.pic_control.temp_dist_l0[0]                         = 3;
    picSetup.pic_control.temp_dist_l0[1]                         = 6;
    picSetup.pic_control.temp_dist_l0[2]                         = 9;
    picSetup.pic_control.temp_dist_l0[3]                         = 12;
    picSetup.pic_control.temp_dist_l0[4]                         = 3;
    picSetup.pic_control.temp_dist_l0[5]                         = 6;
    picSetup.pic_control.temp_dist_l0[6]                         = 9;
    picSetup.pic_control.temp_dist_l0[7]                         = 12;
    picSetup.pic_control.temp_dist_l1[1]                         = -1;
    picSetup.pic_control.temp_dist_l1[2]                         = -1;
    picSetup.pic_control.temp_dist_l1[3]                         = -1;
    picSetup.pic_control.dist_scale_factor[0][1]                 = 128;
    picSetup.pic_control.dist_scale_factor[0][2]                 = 64;
    picSetup.pic_control.dist_scale_factor[0][3]                 = 192;
    picSetup.pic_control.dist_scale_factor[1][0]                 = 512;
    picSetup.pic_control.dist_scale_factor[1][2]                 = 128;
    picSetup.pic_control.dist_scale_factor[1][3]                 = 384;
    picSetup.pic_control.dist_scale_factor[2][0]                 = 1024;
    picSetup.pic_control.dist_scale_factor[2][1]                 = 512;
    picSetup.pic_control.dist_scale_factor[2][3]                 = 768;
    picSetup.pic_control.dist_scale_factor[3][0]                 = 341;
    picSetup.pic_control.dist_scale_factor[3][1]                 = 171;
    picSetup.pic_control.dist_scale_factor[3][2]                 = 85;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 15;
    picSetup.pic_control.pic_type                                = 0;
    picSetup.pic_control.ref_pic_flag                            = 1;
    picSetup.pic_control.nal_unit_type                           = 1;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 4;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 0;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 2;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 2;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 15;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 0;
    picSetup.pic_control.poc_entry_l0[0]                         = 3;
    picSetup.pic_control.poc_entry_l0[1]                         = 2;
    picSetup.pic_control.poc_entry_l0[2]                         = 4;
    picSetup.pic_control.poc_entry_l0[3]                         = 1;
    picSetup.pic_control.poc_entry_l0[4]                         = 3;
    picSetup.pic_control.poc_entry_l0[5]                         = 2;
    picSetup.pic_control.poc_entry_l0[6]                         = 4;
    picSetup.pic_control.poc_entry_l0[7]                         = 1;
    picSetup.pic_control.poc_entry_l1[0]                         = -1;
    picSetup.pic_control.poc_entry_l1[1]                         = -1;
    picSetup.pic_control.poc_entry_l1[2]                         = -1;
    picSetup.pic_control.poc_entry_l1[3]                         = -1;
    sliceControl.qp_avr                                          = 28;
    sliceControl.slice_type                                      = 0;
    sliceControl.num_ref_idx_active_override_flag                = 1;
    sliceControl.num_ref_idx_l0_active_minus1                    = 7;
    sliceControl.num_ref_idx_l1_active_minus1                    = 0;

    WritePicSetupSurface(13, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);
    picSetup.pic_control.l0[3]                                   = 0;
    picSetup.pic_control.l0[4]                                   = -2;
    picSetup.pic_control.l0[5]                                   = -2;
    picSetup.pic_control.l0[6]                                   = -2;
    picSetup.pic_control.l0[7]                                   = -2;
    picSetup.pic_control.l1[0]                                   = 0;
    picSetup.pic_control.l1[1]                                   = 6;
    picSetup.pic_control.l1[2]                                   = 4;
    picSetup.pic_control.l1[3]                                   = 8;
    picSetup.pic_control.temp_dist_l0[0]                         = 1;
    picSetup.pic_control.temp_dist_l0[1]                         = 4;
    picSetup.pic_control.temp_dist_l0[2]                         = 7;
    picSetup.pic_control.temp_dist_l0[3]                         = -2;
    picSetup.pic_control.temp_dist_l0[4]                         = -1;
    picSetup.pic_control.temp_dist_l0[5]                         = -1;
    picSetup.pic_control.temp_dist_l0[6]                         = -1;
    picSetup.pic_control.temp_dist_l0[7]                         = -1;
    picSetup.pic_control.temp_dist_l1[0]                         = -2;
    picSetup.pic_control.temp_dist_l1[1]                         = 1;
    picSetup.pic_control.temp_dist_l1[2]                         = 4;
    picSetup.pic_control.temp_dist_l1[3]                         = 7;
    picSetup.pic_control.dist_scale_factor[0][1]                 = -512;
    picSetup.pic_control.dist_scale_factor[0][2]                 = -128;
    picSetup.pic_control.dist_scale_factor[0][3]                 = -896;
    picSetup.pic_control.dist_scale_factor[1][0]                 = -128;
    picSetup.pic_control.dist_scale_factor[1][2]                 = 64;
    picSetup.pic_control.dist_scale_factor[1][3]                 = 448;
    picSetup.pic_control.dist_scale_factor[2][0]                 = -512;
    picSetup.pic_control.dist_scale_factor[2][1]                 = 1024;
    picSetup.pic_control.dist_scale_factor[2][3]                 = 1792;
    picSetup.pic_control.dist_scale_factor[3][0]                 = -73;
    picSetup.pic_control.dist_scale_factor[3][1]                 = 146;
    picSetup.pic_control.dist_scale_factor[3][2]                 = 37;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0x04020108;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0xf0f0f0f0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 13;
    picSetup.pic_control.ref_pic_flag                            = 0;
    picSetup.pic_control.pic_type                                = 1;
    picSetup.pic_control.nal_unit_type                           = 0;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 3;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 0;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 0;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 7;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0]   = 1;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 1;
    picSetup.pic_control.poc_entry_l0[3]                         = 0;
    picSetup.pic_control.poc_entry_l0[4]                         = -1;
    picSetup.pic_control.poc_entry_l0[5]                         = -1;
    picSetup.pic_control.poc_entry_l0[6]                         = -1;
    picSetup.pic_control.poc_entry_l0[7]                         = -1;
    picSetup.pic_control.poc_entry_l1[0]                         = 0;
    picSetup.pic_control.poc_entry_l1[1]                         = 3;
    picSetup.pic_control.poc_entry_l1[2]                         = 2;
    picSetup.pic_control.poc_entry_l1[3]                         = 4;
    sliceControl.qp_avr                                          = 31;
    sliceControl.slice_type                                      = 1;
    sliceControl.num_ref_idx_active_override_flag                = 0;
    sliceControl.num_ref_idx_l0_active_minus1                    = 3;
    sliceControl.num_ref_idx_l1_active_minus1                    = 3;

    WritePicSetupSurface(14, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);
    picSetup.pic_control.temp_dist_l0[0]                       = 2;
    picSetup.pic_control.temp_dist_l0[1]                       = 5;
    picSetup.pic_control.temp_dist_l0[2]                       = 8;
    picSetup.pic_control.temp_dist_l0[3]                       = -1;
    picSetup.pic_control.temp_dist_l1[0]                       = -1;
    picSetup.pic_control.temp_dist_l1[1]                       = 2;
    picSetup.pic_control.temp_dist_l1[2]                       = 5;
    picSetup.pic_control.temp_dist_l1[3]                       = 8;
    picSetup.pic_control.dist_scale_factor[0][1]               = -1280;
    picSetup.pic_control.dist_scale_factor[0][2]               = -512;
    picSetup.pic_control.dist_scale_factor[0][3]               = -2048;
    picSetup.pic_control.dist_scale_factor[1][0]               = -51;
    picSetup.pic_control.dist_scale_factor[1][2]               = 102;
    picSetup.pic_control.dist_scale_factor[1][3]               = 410;
    picSetup.pic_control.dist_scale_factor[2][0]               = -128;
    picSetup.pic_control.dist_scale_factor[2][1]               = 640;
    picSetup.pic_control.dist_scale_factor[2][3]               = 1024;
    picSetup.pic_control.dist_scale_factor[3][0]               = -32;
    picSetup.pic_control.dist_scale_factor[3][1]               = 160;
    picSetup.pic_control.dist_scale_factor[3][2]               = 64;
    picSetup.pic_control.pic_order_cnt_lsb                     = 14;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0] = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0] = 0;

    WritePicSetupSurface(15, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);
    picSetup.pic_control.l0[0]                                   = 0;
    picSetup.pic_control.l0[1]                                   = 6;
    picSetup.pic_control.l0[2]                                   = 4;
    picSetup.pic_control.l0[4]                                   = 0;
    picSetup.pic_control.l0[3]                                   = 8;
    picSetup.pic_control.l0[5]                                   = 6;
    picSetup.pic_control.l0[6]                                   = 4;
    picSetup.pic_control.l0[7]                                   = 8;
    picSetup.pic_control.l1[0]                                   = -2;
    picSetup.pic_control.l1[1]                                   = -2;
    picSetup.pic_control.l1[2]                                   = -2;
    picSetup.pic_control.l1[3]                                   = -2;
    picSetup.pic_control.temp_dist_l0[0]                         = 3;
    picSetup.pic_control.temp_dist_l0[1]                         = 6;
    picSetup.pic_control.temp_dist_l0[2]                         = 9;
    picSetup.pic_control.temp_dist_l0[3]                         = 12;
    picSetup.pic_control.temp_dist_l0[4]                         = 3;
    picSetup.pic_control.temp_dist_l0[5]                         = 6;
    picSetup.pic_control.temp_dist_l0[6]                         = 9;
    picSetup.pic_control.temp_dist_l0[7]                         = 12;
    picSetup.pic_control.temp_dist_l1[1]                         = -1;
    picSetup.pic_control.temp_dist_l1[2]                         = -1;
    picSetup.pic_control.temp_dist_l1[3]                         = -1;
    picSetup.pic_control.dist_scale_factor[0][1]                 = 768;
    picSetup.pic_control.dist_scale_factor[0][2]                 = 512;
    picSetup.pic_control.dist_scale_factor[0][3]                 = 1024;
    picSetup.pic_control.dist_scale_factor[1][0]                 = 85;
    picSetup.pic_control.dist_scale_factor[1][2]                 = 171;
    picSetup.pic_control.dist_scale_factor[1][3]                 = 341;
    picSetup.pic_control.dist_scale_factor[2][0]                 = 128;
    picSetup.pic_control.dist_scale_factor[2][1]                 = 384;
    picSetup.pic_control.dist_scale_factor[2][3]                 = 512;
    picSetup.pic_control.dist_scale_factor[3][0]                 = 64;
    picSetup.pic_control.dist_scale_factor[3][1]                 = 192;
    picSetup.pic_control.dist_scale_factor[3][2]                 = 128;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 18;
    picSetup.pic_control.pic_type                                = 0;
    picSetup.pic_control.ref_pic_flag                            = 1;
    picSetup.pic_control.nal_unit_type                           = 1;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 4;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 0;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 2;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 2;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 15;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 0;
    picSetup.pic_control.poc_entry_l0[0]                         = 0;
    picSetup.pic_control.poc_entry_l0[1]                         = 3;
    picSetup.pic_control.poc_entry_l0[2]                         = 2;
    picSetup.pic_control.poc_entry_l0[4]                         = 0;
    picSetup.pic_control.poc_entry_l0[3]                         = 4;
    picSetup.pic_control.poc_entry_l0[5]                         = 3;
    picSetup.pic_control.poc_entry_l0[6]                         = 2;
    picSetup.pic_control.poc_entry_l0[7]                         = 4;
    picSetup.pic_control.poc_entry_l1[0]                         = -1;
    picSetup.pic_control.poc_entry_l1[1]                         = -1;
    picSetup.pic_control.poc_entry_l1[2]                         = -1;
    picSetup.pic_control.poc_entry_l1[3]                         = -1;
    sliceControl.qp_avr                                          = 28;
    sliceControl.slice_type                                      = 0;
    sliceControl.num_ref_idx_active_override_flag                = 1;
    sliceControl.num_ref_idx_l0_active_minus1                    = 7;
    sliceControl.num_ref_idx_l1_active_minus1                    = 0;

    WritePicSetupSurface(16, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.l0[3]                                   = 2;
    picSetup.pic_control.l0[4]                                   = -2;
    picSetup.pic_control.l0[5]                                   = -2;
    picSetup.pic_control.l0[6]                                   = -2;
    picSetup.pic_control.l0[7]                                   = -2;
    picSetup.pic_control.l1[0]                                   = 2;
    picSetup.pic_control.l1[1]                                   = 0;
    picSetup.pic_control.l1[2]                                   = 6;
    picSetup.pic_control.l1[3]                                   = 4;
    picSetup.pic_control.temp_dist_l0[0]                         = 1;
    picSetup.pic_control.temp_dist_l0[1]                         = 4;
    picSetup.pic_control.temp_dist_l0[2]                         = 7;
    picSetup.pic_control.temp_dist_l0[3]                         = -2;
    picSetup.pic_control.temp_dist_l0[4]                         = -1;
    picSetup.pic_control.temp_dist_l0[5]                         = -1;
    picSetup.pic_control.temp_dist_l0[6]                         = -1;
    picSetup.pic_control.temp_dist_l0[7]                         = -1;
    picSetup.pic_control.temp_dist_l1[0]                         = -2;
    picSetup.pic_control.temp_dist_l1[1]                         = 1;
    picSetup.pic_control.temp_dist_l1[2]                         = 4;
    picSetup.pic_control.temp_dist_l1[3]                         = 7;
    picSetup.pic_control.dist_scale_factor[0][1]                 = -512;
    picSetup.pic_control.dist_scale_factor[0][2]                 = 1792;
    picSetup.pic_control.dist_scale_factor[1][0]                 = -128;
    picSetup.pic_control.dist_scale_factor[1][2]                 = -896;
    picSetup.pic_control.dist_scale_factor[1][3]                 = -512;
    picSetup.pic_control.dist_scale_factor[2][0]                 = 37;
    picSetup.pic_control.dist_scale_factor[2][1]                 = -73;
    picSetup.pic_control.dist_scale_factor[2][3]                 = 146;
    picSetup.pic_control.dist_scale_factor[3][1]                 = -128;
    picSetup.pic_control.dist_scale_factor[3][2]                 = 448;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0x04020108;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0xf0f0f0f0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 16;
    picSetup.pic_control.ref_pic_flag                            = 0;
    picSetup.pic_control.pic_type                                = 1;
    picSetup.pic_control.nal_unit_type                           = 0;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 3;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 0;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 0;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 7;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0]   = 1;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 1;
    picSetup.pic_control.poc_entry_l0[1]                         = 3;
    picSetup.pic_control.poc_entry_l0[2]                         = 2;
    picSetup.pic_control.poc_entry_l0[3]                         = 1;
    picSetup.pic_control.poc_entry_l0[4]                         = -1;
    picSetup.pic_control.poc_entry_l0[5]                         = -1;
    picSetup.pic_control.poc_entry_l0[6]                         = -1;
    picSetup.pic_control.poc_entry_l0[7]                         = -1;
    picSetup.pic_control.poc_entry_l1[0]                         = 1;
    picSetup.pic_control.poc_entry_l1[1]                         = 0;
    picSetup.pic_control.poc_entry_l1[2]                         = 3;
    picSetup.pic_control.poc_entry_l1[3]                         = 2;
    sliceControl.qp_avr                                          = 31;
    sliceControl.slice_type                                      = 1;
    sliceControl.num_ref_idx_active_override_flag                = 0;
    sliceControl.num_ref_idx_l0_active_minus1                    = 3;
    sliceControl.num_ref_idx_l1_active_minus1                    = 3;

    WritePicSetupSurface(17, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.temp_dist_l0[0]                       = 2;
    picSetup.pic_control.temp_dist_l0[1]                       = 5;
    picSetup.pic_control.temp_dist_l0[2]                       = 8;
    picSetup.pic_control.temp_dist_l0[3]                       = -1;
    picSetup.pic_control.temp_dist_l1[0]                       = -1;
    picSetup.pic_control.temp_dist_l1[1]                       = 2;
    picSetup.pic_control.temp_dist_l1[2]                       = 5;
    picSetup.pic_control.temp_dist_l1[3]                       = 8;
    picSetup.pic_control.dist_scale_factor[0][1]               = -128;
    picSetup.pic_control.dist_scale_factor[0][2]               = 1024;
    picSetup.pic_control.dist_scale_factor[0][3]               = 640;
    picSetup.pic_control.dist_scale_factor[1][0]               = -512;
    picSetup.pic_control.dist_scale_factor[1][2]               = -2048;
    picSetup.pic_control.dist_scale_factor[1][3]               = -1280;
    picSetup.pic_control.dist_scale_factor[2][0]               = 64;
    picSetup.pic_control.dist_scale_factor[2][1]               = -32;
    picSetup.pic_control.dist_scale_factor[2][3]               = 160;
    picSetup.pic_control.dist_scale_factor[3][0]               = 102;
    picSetup.pic_control.dist_scale_factor[3][1]               = -51;
    picSetup.pic_control.dist_scale_factor[3][2]               = 410;
    picSetup.pic_control.pic_order_cnt_lsb                     = 17;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0] = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0] = 0;

    WritePicSetupSurface(18, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);
    picSetup.pic_control.l0[1]                                   = 0;
    picSetup.pic_control.l0[0]                                   = 2;
    picSetup.pic_control.l0[2]                                   = 6;
    picSetup.pic_control.l0[3]                                   = 4;
    picSetup.pic_control.l0[4]                                   = 2;
    picSetup.pic_control.l0[5]                                   = 0;
    picSetup.pic_control.l0[6]                                   = 6;
    picSetup.pic_control.l0[7]                                   = 4;
    picSetup.pic_control.l1[0]                                   = -2;
    picSetup.pic_control.l1[1]                                   = -2;
    picSetup.pic_control.l1[2]                                   = -2;
    picSetup.pic_control.l1[3]                                   = -2;
    picSetup.pic_control.temp_dist_l0[0]                         = 3;
    picSetup.pic_control.temp_dist_l0[1]                         = 6;
    picSetup.pic_control.temp_dist_l0[2]                         = 9;
    picSetup.pic_control.temp_dist_l0[3]                         = 12;
    picSetup.pic_control.temp_dist_l0[4]                         = 3;
    picSetup.pic_control.temp_dist_l0[5]                         = 6;
    picSetup.pic_control.temp_dist_l0[6]                         = 9;
    picSetup.pic_control.temp_dist_l0[7]                         = 12;
    picSetup.pic_control.temp_dist_l1[1]                         = -1;
    picSetup.pic_control.temp_dist_l1[2]                         = -1;
    picSetup.pic_control.temp_dist_l1[3]                         = -1;
    picSetup.pic_control.dist_scale_factor[0][1]                 = 128;
    picSetup.pic_control.dist_scale_factor[0][2]                 = 512;
    picSetup.pic_control.dist_scale_factor[0][3]                 = 384;
    picSetup.pic_control.dist_scale_factor[1][0]                 = 512;
    picSetup.pic_control.dist_scale_factor[1][2]                 = 1024;
    picSetup.pic_control.dist_scale_factor[1][3]                 = 768;
    picSetup.pic_control.dist_scale_factor[2][0]                 = 128;
    picSetup.pic_control.dist_scale_factor[2][1]                 = 64;
    picSetup.pic_control.dist_scale_factor[2][3]                 = 192;
    picSetup.pic_control.dist_scale_factor[3][0]                 = 171;
    picSetup.pic_control.dist_scale_factor[3][1]                 = 85;
    picSetup.pic_control.dist_scale_factor[3][2]                 = 341;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 21;
    picSetup.pic_control.pic_type                                = 0;
    picSetup.pic_control.ref_pic_flag                            = 1;
    picSetup.pic_control.nal_unit_type                           = 1;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 4;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 0;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 2;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 2;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 15;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 0;
    picSetup.pic_control.poc_entry_l0[1]                         = 0;
    picSetup.pic_control.poc_entry_l0[0]                         = 1;
    picSetup.pic_control.poc_entry_l0[2]                         = 3;
    picSetup.pic_control.poc_entry_l0[3]                         = 2;
    picSetup.pic_control.poc_entry_l0[4]                         = 1;
    picSetup.pic_control.poc_entry_l0[5]                         = 0;
    picSetup.pic_control.poc_entry_l0[6]                         = 3;
    picSetup.pic_control.poc_entry_l0[7]                         = 2;
    picSetup.pic_control.poc_entry_l1[0]                         = -1;
    picSetup.pic_control.poc_entry_l1[1]                         = -1;
    picSetup.pic_control.poc_entry_l1[2]                         = -1;
    picSetup.pic_control.poc_entry_l1[3]                         = -1;
    sliceControl.qp_avr                                          = 28;
    sliceControl.slice_type                                      = 0;
    sliceControl.num_ref_idx_active_override_flag                = 1;
    sliceControl.num_ref_idx_l0_active_minus1                    = 7;
    sliceControl.num_ref_idx_l1_active_minus1                    = 0;

    WritePicSetupSurface(19, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.l0[3]                                   = 8;
    picSetup.pic_control.l0[4]                                   = -2;
    picSetup.pic_control.l0[5]                                   = -2;
    picSetup.pic_control.l0[6]                                   = -2;
    picSetup.pic_control.l0[7]                                   = -2;
    picSetup.pic_control.l1[0]                                   = 8;
    picSetup.pic_control.l1[1]                                   = 2;
    picSetup.pic_control.l1[2]                                   = 0;
    picSetup.pic_control.l1[3]                                   = 6;
    picSetup.pic_control.temp_dist_l0[0]                         = 1;
    picSetup.pic_control.temp_dist_l0[1]                         = 4;
    picSetup.pic_control.temp_dist_l0[2]                         = 7;
    picSetup.pic_control.temp_dist_l0[3]                         = -2;
    picSetup.pic_control.temp_dist_l0[4]                         = -1;
    picSetup.pic_control.temp_dist_l0[5]                         = -1;
    picSetup.pic_control.temp_dist_l0[6]                         = -1;
    picSetup.pic_control.temp_dist_l0[7]                         = -1;
    picSetup.pic_control.temp_dist_l1[0]                         = -2;
    picSetup.pic_control.temp_dist_l1[1]                         = 1;
    picSetup.pic_control.temp_dist_l1[2]                         = 4;
    picSetup.pic_control.temp_dist_l1[3]                         = 7;
    picSetup.pic_control.dist_scale_factor[0][1]                 = 64;
    picSetup.pic_control.dist_scale_factor[0][2]                 = 448;
    picSetup.pic_control.dist_scale_factor[0][3]                 = -128;
    picSetup.pic_control.dist_scale_factor[1][0]                 = 1024;
    picSetup.pic_control.dist_scale_factor[1][2]                 = 1792;
    picSetup.pic_control.dist_scale_factor[1][3]                 = -512;
    picSetup.pic_control.dist_scale_factor[2][0]                 = 146;
    picSetup.pic_control.dist_scale_factor[2][1]                 = 37;
    picSetup.pic_control.dist_scale_factor[2][3]                 = -73;
    picSetup.pic_control.dist_scale_factor[3][0]                 = -512;
    picSetup.pic_control.dist_scale_factor[3][1]                 = -128;
    picSetup.pic_control.dist_scale_factor[3][2]                 = -896;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0x04020108;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0xf0f0f0f0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 19;
    picSetup.pic_control.ref_pic_flag                            = 0;
    picSetup.pic_control.pic_type                                = 1;
    picSetup.pic_control.nal_unit_type                           = 0;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 3;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 0;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 0;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 7;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0]   = 1;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 1;
    picSetup.pic_control.poc_entry_l0[3]                         = 4;
    picSetup.pic_control.poc_entry_l0[4]                         = -1;
    picSetup.pic_control.poc_entry_l0[5]                         = -1;
    picSetup.pic_control.poc_entry_l0[6]                         = -1;
    picSetup.pic_control.poc_entry_l0[7]                         = -1;
    picSetup.pic_control.poc_entry_l1[0]                         = 4;
    picSetup.pic_control.poc_entry_l1[1]                         = 1;
    picSetup.pic_control.poc_entry_l1[2]                         = 0;
    picSetup.pic_control.poc_entry_l1[3]                         = 3;
    sliceControl.qp_avr                                          = 31;
    sliceControl.slice_type                                      = 1;
    sliceControl.num_ref_idx_active_override_flag                = 0;
    sliceControl.num_ref_idx_l0_active_minus1                    = 3;
    sliceControl.num_ref_idx_l1_active_minus1                    = 3;

    WritePicSetupSurface(20, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.temp_dist_l0[0]                       = 2;
    picSetup.pic_control.temp_dist_l0[1]                       = 5;
    picSetup.pic_control.temp_dist_l0[2]                       = 8;
    picSetup.pic_control.temp_dist_l0[3]                       = -1;
    picSetup.pic_control.temp_dist_l1[0]                       = -1;
    picSetup.pic_control.temp_dist_l1[1]                       = 2;
    picSetup.pic_control.temp_dist_l1[2]                       = 5;
    picSetup.pic_control.temp_dist_l1[3]                       = 8;
    picSetup.pic_control.dist_scale_factor[0][1]               = 102;
    picSetup.pic_control.dist_scale_factor[0][2]               = 410;
    picSetup.pic_control.dist_scale_factor[0][3]               = -51;
    picSetup.pic_control.dist_scale_factor[1][0]               = 640;
    picSetup.pic_control.dist_scale_factor[1][2]               = 1024;
    picSetup.pic_control.dist_scale_factor[1][3]               = -128;
    picSetup.pic_control.dist_scale_factor[2][0]               = 160;
    picSetup.pic_control.dist_scale_factor[2][1]               = 64;
    picSetup.pic_control.dist_scale_factor[2][3]               = -32;
    picSetup.pic_control.dist_scale_factor[3][0]               = -1280;
    picSetup.pic_control.dist_scale_factor[3][1]               = -512;
    picSetup.pic_control.dist_scale_factor[3][2]               = -2048;
    picSetup.pic_control.pic_order_cnt_lsb                     = 20;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0] = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0] = 0;

    WritePicSetupSurface(21, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.l0[0]                                   = 8;
    picSetup.pic_control.l0[2]                                   = 0;
    picSetup.pic_control.l0[1]                                   = 2;
    picSetup.pic_control.l0[3]                                   = 6;
    picSetup.pic_control.l0[4]                                   = 8;
    picSetup.pic_control.l0[5]                                   = 2;
    picSetup.pic_control.l0[6]                                   = 0;
    picSetup.pic_control.l0[7]                                   = 6;
    picSetup.pic_control.l1[0]                                   = -2;
    picSetup.pic_control.l1[1]                                   = -2;
    picSetup.pic_control.l1[2]                                   = -2;
    picSetup.pic_control.l1[3]                                   = -2;
    picSetup.pic_control.temp_dist_l0[0]                         = 3;
    picSetup.pic_control.temp_dist_l0[1]                         = 6;
    picSetup.pic_control.temp_dist_l0[2]                         = 9;
    picSetup.pic_control.temp_dist_l0[3]                         = 12;
    picSetup.pic_control.temp_dist_l0[4]                         = 3;
    picSetup.pic_control.temp_dist_l0[5]                         = 6;
    picSetup.pic_control.temp_dist_l0[6]                         = 9;
    picSetup.pic_control.temp_dist_l0[7]                         = 12;
    picSetup.pic_control.temp_dist_l1[1]                         = -1;
    picSetup.pic_control.temp_dist_l1[2]                         = -1;
    picSetup.pic_control.temp_dist_l1[3]                         = -1;
    picSetup.pic_control.dist_scale_factor[0][1]                 = 171;
    picSetup.pic_control.dist_scale_factor[0][2]                 = 341;
    picSetup.pic_control.dist_scale_factor[0][3]                 = 85;
    picSetup.pic_control.dist_scale_factor[1][0]                 = 384;
    picSetup.pic_control.dist_scale_factor[1][2]                 = 512;
    picSetup.pic_control.dist_scale_factor[1][3]                 = 128;
    picSetup.pic_control.dist_scale_factor[2][0]                 = 192;
    picSetup.pic_control.dist_scale_factor[2][1]                 = 128;
    picSetup.pic_control.dist_scale_factor[2][3]                 = 64;
    picSetup.pic_control.dist_scale_factor[3][0]                 = 768;
    picSetup.pic_control.dist_scale_factor[3][1]                 = 512;
    picSetup.pic_control.dist_scale_factor[3][2]                 = 1024;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 24;
    picSetup.pic_control.pic_type                                = 0;
    picSetup.pic_control.ref_pic_flag                            = 1;
    picSetup.pic_control.nal_unit_type                           = 1;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 4;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 0;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 2;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 2;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 15;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 0;
    picSetup.pic_control.poc_entry_l0[0]                         = 4;
    picSetup.pic_control.poc_entry_l0[2]                         = 0;
    picSetup.pic_control.poc_entry_l0[1]                         = 1;
    picSetup.pic_control.poc_entry_l0[3]                         = 3;
    picSetup.pic_control.poc_entry_l0[4]                         = 4;
    picSetup.pic_control.poc_entry_l0[5]                         = 1;
    picSetup.pic_control.poc_entry_l0[6]                         = 0;
    picSetup.pic_control.poc_entry_l0[7]                         = 3;
    picSetup.pic_control.poc_entry_l1[0]                         = -1;
    picSetup.pic_control.poc_entry_l1[1]                         = -1;
    picSetup.pic_control.poc_entry_l1[2]                         = -1;
    picSetup.pic_control.poc_entry_l1[3]                         = -1;
    sliceControl.qp_avr                                          = 28;
    sliceControl.slice_type                                      = 0;
    sliceControl.num_ref_idx_active_override_flag                = 1;
    sliceControl.num_ref_idx_l0_active_minus1                    = 7;
    sliceControl.num_ref_idx_l1_active_minus1                    = 0;

    WritePicSetupSurface(22, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.l0[3]                                   = 4;
    picSetup.pic_control.l0[4]                                   = -2;
    picSetup.pic_control.l0[5]                                   = -2;
    picSetup.pic_control.l0[6]                                   = -2;
    picSetup.pic_control.l0[7]                                   = -2;
    picSetup.pic_control.l1[0]                                   = 4;
    picSetup.pic_control.l1[1]                                   = 8;
    picSetup.pic_control.l1[2]                                   = 2;
    picSetup.pic_control.l1[3]                                   = 0;
    picSetup.pic_control.temp_dist_l0[0]                         = 1;
    picSetup.pic_control.temp_dist_l0[1]                         = 4;
    picSetup.pic_control.temp_dist_l0[2]                         = 7;
    picSetup.pic_control.temp_dist_l0[3]                         = -2;
    picSetup.pic_control.temp_dist_l0[4]                         = -1;
    picSetup.pic_control.temp_dist_l0[5]                         = -1;
    picSetup.pic_control.temp_dist_l0[6]                         = -1;
    picSetup.pic_control.temp_dist_l0[7]                         = -1;
    picSetup.pic_control.temp_dist_l1[0]                         = -2;
    picSetup.pic_control.temp_dist_l1[1]                         = 1;
    picSetup.pic_control.temp_dist_l1[2]                         = 4;
    picSetup.pic_control.temp_dist_l1[3]                         = 7;
    picSetup.pic_control.dist_scale_factor[0][1]                 = 146;
    picSetup.pic_control.dist_scale_factor[0][2]                 = -73;
    picSetup.pic_control.dist_scale_factor[0][3]                 = 37;
    picSetup.pic_control.dist_scale_factor[1][0]                 = 448;
    picSetup.pic_control.dist_scale_factor[1][2]                 = -128;
    picSetup.pic_control.dist_scale_factor[1][3]                 = 64;
    picSetup.pic_control.dist_scale_factor[2][0]                 = -896;
    picSetup.pic_control.dist_scale_factor[2][1]                 = -512;
    picSetup.pic_control.dist_scale_factor[2][3]                 = -128;
    picSetup.pic_control.dist_scale_factor[3][0]                 = 1792;
    picSetup.pic_control.dist_scale_factor[3][1]                 = 1024;
    picSetup.pic_control.dist_scale_factor[3][2]                 = -512;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0x04020108;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0xf0f0f0f0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 22;
    picSetup.pic_control.ref_pic_flag                            = 0;
    picSetup.pic_control.pic_type                                = 1;
    picSetup.pic_control.nal_unit_type                           = 0;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 3;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 0;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 0;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 7;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0]   = 1;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 1;
    picSetup.pic_control.poc_entry_l0[3]                         = 2;
    picSetup.pic_control.poc_entry_l0[4]                         = -1;
    picSetup.pic_control.poc_entry_l0[5]                         = -1;
    picSetup.pic_control.poc_entry_l0[6]                         = -1;
    picSetup.pic_control.poc_entry_l0[7]                         = -1;
    picSetup.pic_control.poc_entry_l1[0]                         = 2;
    picSetup.pic_control.poc_entry_l1[1]                         = 4;
    picSetup.pic_control.poc_entry_l1[2]                         = 1;
    picSetup.pic_control.poc_entry_l1[3]                         = 0;
    sliceControl.qp_avr                                          = 31;
    sliceControl.slice_type                                      = 1;
    sliceControl.num_ref_idx_active_override_flag                = 0;
    sliceControl.num_ref_idx_l0_active_minus1                    = 3;
    sliceControl.num_ref_idx_l1_active_minus1                    = 3;

    WritePicSetupSurface(23, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.temp_dist_l0[0]                       = 2;
    picSetup.pic_control.temp_dist_l0[1]                       = 5;
    picSetup.pic_control.temp_dist_l0[2]                       = 8;
    picSetup.pic_control.temp_dist_l0[3]                       = -1;
    picSetup.pic_control.temp_dist_l1[0]                       = -1;
    picSetup.pic_control.temp_dist_l1[1]                       = 2;
    picSetup.pic_control.temp_dist_l1[2]                       = 5;
    picSetup.pic_control.temp_dist_l1[3]                       = 8;
    picSetup.pic_control.dist_scale_factor[0][1]               = 160;
    picSetup.pic_control.dist_scale_factor[0][2]               = -32;
    picSetup.pic_control.dist_scale_factor[0][3]               = 64;
    picSetup.pic_control.dist_scale_factor[1][0]               = 410;
    picSetup.pic_control.dist_scale_factor[1][2]               = -51;
    picSetup.pic_control.dist_scale_factor[1][3]               = 102;
    picSetup.pic_control.dist_scale_factor[2][0]               = -2048;
    picSetup.pic_control.dist_scale_factor[2][1]               = -1280;
    picSetup.pic_control.dist_scale_factor[2][3]               = -512;
    picSetup.pic_control.dist_scale_factor[3][0]               = 1024;
    picSetup.pic_control.dist_scale_factor[3][1]               = 640;
    picSetup.pic_control.dist_scale_factor[3][2]               = -128;
    picSetup.pic_control.pic_order_cnt_lsb                     = 23;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0] = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0] = 0;

    WritePicSetupSurface(24, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.l0[0]                                   = 4;
    picSetup.pic_control.l0[1]                                   = 8;
    picSetup.pic_control.l0[3]                                   = 0;
    picSetup.pic_control.l0[2]                                   = 2;
    picSetup.pic_control.l0[4]                                   = 4;
    picSetup.pic_control.l0[5]                                   = 8;
    picSetup.pic_control.l0[6]                                   = 2;
    picSetup.pic_control.l0[7]                                   = 0;
    picSetup.pic_control.l1[0]                                   = -2;
    picSetup.pic_control.l1[1]                                   = -2;
    picSetup.pic_control.l1[2]                                   = -2;
    picSetup.pic_control.l1[3]                                   = -2;
    picSetup.pic_control.temp_dist_l0[0]                         = 3;
    picSetup.pic_control.temp_dist_l0[1]                         = 6;
    picSetup.pic_control.temp_dist_l0[2]                         = 9;
    picSetup.pic_control.temp_dist_l0[3]                         = 12;
    picSetup.pic_control.temp_dist_l0[4]                         = 3;
    picSetup.pic_control.temp_dist_l0[5]                         = 6;
    picSetup.pic_control.temp_dist_l0[6]                         = 9;
    picSetup.pic_control.temp_dist_l0[7]                         = 12;
    picSetup.pic_control.temp_dist_l1[1]                         = -1;
    picSetup.pic_control.temp_dist_l1[2]                         = -1;
    picSetup.pic_control.temp_dist_l1[3]                         = -1;
    picSetup.pic_control.dist_scale_factor[0][1]                 = 192;
    picSetup.pic_control.dist_scale_factor[0][2]                 = 64;
    picSetup.pic_control.dist_scale_factor[0][3]                 = 128;
    picSetup.pic_control.dist_scale_factor[1][0]                 = 341;
    picSetup.pic_control.dist_scale_factor[1][2]                 = 85;
    picSetup.pic_control.dist_scale_factor[1][3]                 = 171;
    picSetup.pic_control.dist_scale_factor[2][0]                 = 1024;
    picSetup.pic_control.dist_scale_factor[2][1]                 = 768;
    picSetup.pic_control.dist_scale_factor[2][3]                 = 512;
    picSetup.pic_control.dist_scale_factor[3][0]                 = 512;
    picSetup.pic_control.dist_scale_factor[3][1]                 = 384;
    picSetup.pic_control.dist_scale_factor[3][2]                 = 128;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 27;
    picSetup.pic_control.pic_type                                = 0;
    picSetup.pic_control.ref_pic_flag                            = 1;
    picSetup.pic_control.nal_unit_type                           = 1;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 4;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 0;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 2;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 2;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 15;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 0;
    picSetup.pic_control.poc_entry_l0[0]                         = 2;
    picSetup.pic_control.poc_entry_l0[1]                         = 4;
    picSetup.pic_control.poc_entry_l0[3]                         = 0;
    picSetup.pic_control.poc_entry_l0[2]                         = 1;
    picSetup.pic_control.poc_entry_l0[4]                         = 2;
    picSetup.pic_control.poc_entry_l0[5]                         = 4;
    picSetup.pic_control.poc_entry_l0[6]                         = 1;
    picSetup.pic_control.poc_entry_l0[7]                         = 0;
    picSetup.pic_control.poc_entry_l1[0]                         = -1;
    picSetup.pic_control.poc_entry_l1[1]                         = -1;
    picSetup.pic_control.poc_entry_l1[2]                         = -1;
    picSetup.pic_control.poc_entry_l1[3]                         = -1;
    sliceControl.qp_avr                                          = 28;
    sliceControl.slice_type                                      = 0;
    sliceControl.num_ref_idx_active_override_flag                = 1;
    sliceControl.num_ref_idx_l0_active_minus1                    = 7;
    sliceControl.num_ref_idx_l1_active_minus1                    = 0;

    WritePicSetupSurface(25, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.l0[3]                                   = 6;
    picSetup.pic_control.l0[4]                                   = -2;
    picSetup.pic_control.l0[5]                                   = -2;
    picSetup.pic_control.l0[6]                                   = -2;
    picSetup.pic_control.l0[7]                                   = -2;
    picSetup.pic_control.l1[0]                                   = 6;
    picSetup.pic_control.l1[1]                                   = 4;
    picSetup.pic_control.l1[2]                                   = 8;
    picSetup.pic_control.l1[3]                                   = 2;
    picSetup.pic_control.temp_dist_l0[0]                         = 1;
    picSetup.pic_control.temp_dist_l0[1]                         = 4;
    picSetup.pic_control.temp_dist_l0[2]                         = 7;
    picSetup.pic_control.temp_dist_l0[3]                         = -2;
    picSetup.pic_control.temp_dist_l0[4]                         = -1;
    picSetup.pic_control.temp_dist_l0[5]                         = -1;
    picSetup.pic_control.temp_dist_l0[6]                         = -1;
    picSetup.pic_control.temp_dist_l0[7]                         = -1;
    picSetup.pic_control.temp_dist_l1[0]                         = -2;
    picSetup.pic_control.temp_dist_l1[1]                         = 1;
    picSetup.pic_control.temp_dist_l1[2]                         = 4;
    picSetup.pic_control.temp_dist_l1[3]                         = 7;
    picSetup.pic_control.dist_scale_factor[0][1]                 = 37;
    picSetup.pic_control.dist_scale_factor[0][2]                 = -73;
    picSetup.pic_control.dist_scale_factor[0][3]                 = 146;
    picSetup.pic_control.dist_scale_factor[1][0]                 = 1792;
    picSetup.pic_control.dist_scale_factor[1][2]                 = -512;
    picSetup.pic_control.dist_scale_factor[1][3]                 = 1024;
    picSetup.pic_control.dist_scale_factor[2][0]                 = -896;
    picSetup.pic_control.dist_scale_factor[2][1]                 = -128;
    picSetup.pic_control.dist_scale_factor[2][3]                 = -512;
    picSetup.pic_control.dist_scale_factor[3][0]                 = 448;
    picSetup.pic_control.dist_scale_factor[3][1]                 = 64;
    picSetup.pic_control.dist_scale_factor[3][2]                 = -128;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0x04020108;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0xf0f0f0f0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 25;
    picSetup.pic_control.ref_pic_flag                            = 0;
    picSetup.pic_control.pic_type                                = 1;
    picSetup.pic_control.nal_unit_type                           = 0;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 3;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 0;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 0;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 7;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0]   = 1;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 1;
    picSetup.pic_control.poc_entry_l0[3]                         = 3;
    picSetup.pic_control.poc_entry_l0[4]                         = -1;
    picSetup.pic_control.poc_entry_l0[5]                         = -1;
    picSetup.pic_control.poc_entry_l0[6]                         = -1;
    picSetup.pic_control.poc_entry_l0[7]                         = -1;
    picSetup.pic_control.poc_entry_l1[0]                         = 3;
    picSetup.pic_control.poc_entry_l1[1]                         = 2;
    picSetup.pic_control.poc_entry_l1[2]                         = 4;
    picSetup.pic_control.poc_entry_l1[3]                         = 1;
    sliceControl.qp_avr                                          = 31;
    sliceControl.slice_type                                      = 1;
    sliceControl.num_ref_idx_active_override_flag                = 0;
    sliceControl.num_ref_idx_l0_active_minus1                    = 3;
    sliceControl.num_ref_idx_l1_active_minus1                    = 3;

    WritePicSetupSurface(26, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);
    picSetup.pic_control.temp_dist_l0[0]                       = 2;
    picSetup.pic_control.temp_dist_l0[1]                       = 5;
    picSetup.pic_control.temp_dist_l0[2]                       = 8;
    picSetup.pic_control.temp_dist_l0[3]                       = -1;
    picSetup.pic_control.temp_dist_l1[0]                       = -1;
    picSetup.pic_control.temp_dist_l1[1]                       = 2;
    picSetup.pic_control.temp_dist_l1[2]                       = 5;
    picSetup.pic_control.temp_dist_l1[3]                       = 8;
    picSetup.pic_control.dist_scale_factor[0][1]               = 64;
    picSetup.pic_control.dist_scale_factor[0][2]               = -32;
    picSetup.pic_control.dist_scale_factor[0][3]               = 160;
    picSetup.pic_control.dist_scale_factor[1][0]               = 1024;
    picSetup.pic_control.dist_scale_factor[1][2]               = -128;
    picSetup.pic_control.dist_scale_factor[1][3]               = 640;
    picSetup.pic_control.dist_scale_factor[2][0]               = -2048;
    picSetup.pic_control.dist_scale_factor[2][1]               = -512;
    picSetup.pic_control.dist_scale_factor[2][3]               = -1280;
    picSetup.pic_control.dist_scale_factor[3][0]               = 410;
    picSetup.pic_control.dist_scale_factor[3][1]               = 102;
    picSetup.pic_control.dist_scale_factor[3][2]               = -51;
    picSetup.pic_control.pic_order_cnt_lsb                     = 26;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0] = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0] = 0;

    WritePicSetupSurface(27, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.l0[0]                                   = 6;
    picSetup.pic_control.l0[1]                                   = 4;
    picSetup.pic_control.l0[2]                                   = 8;
    picSetup.pic_control.l0[3]                                   = 2;
    picSetup.pic_control.l0[4]                                   = 6;
    picSetup.pic_control.l0[5]                                   = 4;
    picSetup.pic_control.l0[6]                                   = 8;
    picSetup.pic_control.l0[7]                                   = 2;
    picSetup.pic_control.l1[0]                                   = -2;
    picSetup.pic_control.l1[1]                                   = -2;
    picSetup.pic_control.l1[2]                                   = -2;
    picSetup.pic_control.l1[3]                                   = -2;
    picSetup.pic_control.temp_dist_l0[0]                         = 3;
    picSetup.pic_control.temp_dist_l0[1]                         = 6;
    picSetup.pic_control.temp_dist_l0[2]                         = 9;
    picSetup.pic_control.temp_dist_l0[3]                         = 12;
    picSetup.pic_control.temp_dist_l0[4]                         = 3;
    picSetup.pic_control.temp_dist_l0[5]                         = 6;
    picSetup.pic_control.temp_dist_l0[6]                         = 9;
    picSetup.pic_control.temp_dist_l0[7]                         = 12;
    picSetup.pic_control.temp_dist_l1[1]                         = -1;
    picSetup.pic_control.temp_dist_l1[2]                         = -1;
    picSetup.pic_control.temp_dist_l1[3]                         = -1;
    picSetup.pic_control.dist_scale_factor[0][1]                 = 128;
    picSetup.pic_control.dist_scale_factor[0][2]                 = 64;
    picSetup.pic_control.dist_scale_factor[0][3]                 = 192;
    picSetup.pic_control.dist_scale_factor[1][0]                 = 512;
    picSetup.pic_control.dist_scale_factor[1][2]                 = 128;
    picSetup.pic_control.dist_scale_factor[1][3]                 = 384;
    picSetup.pic_control.dist_scale_factor[2][0]                 = 1024;
    picSetup.pic_control.dist_scale_factor[2][1]                 = 512;
    picSetup.pic_control.dist_scale_factor[2][3]                 = 768;
    picSetup.pic_control.dist_scale_factor[3][0]                 = 341;
    picSetup.pic_control.dist_scale_factor[3][1]                 = 171;
    picSetup.pic_control.dist_scale_factor[3][2]                 = 85;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 30;
    picSetup.pic_control.pic_type                                = 0;
    picSetup.pic_control.ref_pic_flag                            = 1;
    picSetup.pic_control.nal_unit_type                           = 1;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 4;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 0;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 2;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 2;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 15;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 0;
    picSetup.pic_control.poc_entry_l0[0]                         = 3;
    picSetup.pic_control.poc_entry_l0[1]                         = 2;
    picSetup.pic_control.poc_entry_l0[2]                         = 4;
    picSetup.pic_control.poc_entry_l0[3]                         = 1;
    picSetup.pic_control.poc_entry_l0[4]                         = 3;
    picSetup.pic_control.poc_entry_l0[5]                         = 2;
    picSetup.pic_control.poc_entry_l0[6]                         = 4;
    picSetup.pic_control.poc_entry_l0[7]                         = 1;
    picSetup.pic_control.poc_entry_l1[0]                         = -1;
    picSetup.pic_control.poc_entry_l1[1]                         = -1;
    picSetup.pic_control.poc_entry_l1[2]                         = -1;
    picSetup.pic_control.poc_entry_l1[3]                         = -1;
    sliceControl.qp_avr                                          = 28;
    sliceControl.slice_type                                      = 0;
    sliceControl.num_ref_idx_active_override_flag                = 1;
    sliceControl.num_ref_idx_l0_active_minus1                    = 7;
    sliceControl.num_ref_idx_l1_active_minus1                    = 0;

    WritePicSetupSurface(28, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);
    picSetup.pic_control.l0[3]                                   = 0;
    picSetup.pic_control.l0[4]                                   = -2;
    picSetup.pic_control.l0[5]                                   = -2;
    picSetup.pic_control.l0[6]                                   = -2;
    picSetup.pic_control.l0[7]                                   = -2;
    picSetup.pic_control.l1[0]                                   = 0;
    picSetup.pic_control.l1[1]                                   = 6;
    picSetup.pic_control.l1[2]                                   = 4;
    picSetup.pic_control.l1[3]                                   = 8;
    picSetup.pic_control.temp_dist_l0[0]                         = 1;
    picSetup.pic_control.temp_dist_l0[1]                         = 4;
    picSetup.pic_control.temp_dist_l0[2]                         = 7;
    picSetup.pic_control.temp_dist_l0[3]                         = -2;
    picSetup.pic_control.temp_dist_l0[4]                         = -1;
    picSetup.pic_control.temp_dist_l0[5]                         = -1;
    picSetup.pic_control.temp_dist_l0[6]                         = -1;
    picSetup.pic_control.temp_dist_l0[7]                         = -1;
    picSetup.pic_control.temp_dist_l1[0]                         = -2;
    picSetup.pic_control.temp_dist_l1[1]                         = 1;
    picSetup.pic_control.temp_dist_l1[2]                         = 4;
    picSetup.pic_control.temp_dist_l1[3]                         = 7;
    picSetup.pic_control.dist_scale_factor[0][1]                 = -512;
    picSetup.pic_control.dist_scale_factor[0][2]                 = -128;
    picSetup.pic_control.dist_scale_factor[0][3]                 = -896;
    picSetup.pic_control.dist_scale_factor[1][0]                 = -128;
    picSetup.pic_control.dist_scale_factor[1][2]                 = 64;
    picSetup.pic_control.dist_scale_factor[1][3]                 = 448;
    picSetup.pic_control.dist_scale_factor[2][0]                 = -512;
    picSetup.pic_control.dist_scale_factor[2][1]                 = 1024;
    picSetup.pic_control.dist_scale_factor[2][3]                 = 1792;
    picSetup.pic_control.dist_scale_factor[3][0]                 = -73;
    picSetup.pic_control.dist_scale_factor[3][1]                 = 146;
    picSetup.pic_control.dist_scale_factor[3][2]                 = 37;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0x04020108;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0xf0f0f0f0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 28;
    picSetup.pic_control.ref_pic_flag                            = 0;
    picSetup.pic_control.pic_type                                = 1;
    picSetup.pic_control.nal_unit_type                           = 0;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 3;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 0;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 0;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 7;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0]   = 1;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 1;
    picSetup.pic_control.poc_entry_l0[3]                         = 0;
    picSetup.pic_control.poc_entry_l0[4]                         = -1;
    picSetup.pic_control.poc_entry_l0[5]                         = -1;
    picSetup.pic_control.poc_entry_l0[6]                         = -1;
    picSetup.pic_control.poc_entry_l0[7]                         = -1;
    picSetup.pic_control.poc_entry_l1[0]                         = 0;
    picSetup.pic_control.poc_entry_l1[1]                         = 3;
    picSetup.pic_control.poc_entry_l1[2]                         = 2;
    picSetup.pic_control.poc_entry_l1[3]                         = 4;
    sliceControl.qp_avr                                          = 31;
    sliceControl.slice_type                                      = 1;
    sliceControl.num_ref_idx_active_override_flag                = 0;
    sliceControl.num_ref_idx_l0_active_minus1                    = 3;
    sliceControl.num_ref_idx_l1_active_minus1                    = 3;

    WritePicSetupSurface(29, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);
    picSetup.pic_control.temp_dist_l0[0]                       = 2;
    picSetup.pic_control.temp_dist_l0[1]                       = 5;
    picSetup.pic_control.temp_dist_l0[2]                       = 8;
    picSetup.pic_control.temp_dist_l0[3]                       = -1;
    picSetup.pic_control.temp_dist_l1[0]                       = -1;
    picSetup.pic_control.temp_dist_l1[1]                       = 2;
    picSetup.pic_control.temp_dist_l1[2]                       = 5;
    picSetup.pic_control.temp_dist_l1[3]                       = 8;
    picSetup.pic_control.dist_scale_factor[0][1]               = -1280;
    picSetup.pic_control.dist_scale_factor[0][2]               = -512;
    picSetup.pic_control.dist_scale_factor[0][3]               = -2048;
    picSetup.pic_control.dist_scale_factor[1][0]               = -51;
    picSetup.pic_control.dist_scale_factor[1][2]               = 102;
    picSetup.pic_control.dist_scale_factor[1][3]               = 410;
    picSetup.pic_control.dist_scale_factor[2][0]               = -128;
    picSetup.pic_control.dist_scale_factor[2][1]               = 640;
    picSetup.pic_control.dist_scale_factor[2][3]               = 1024;
    picSetup.pic_control.dist_scale_factor[3][0]               = -32;
    picSetup.pic_control.dist_scale_factor[3][1]               = 160;
    picSetup.pic_control.dist_scale_factor[3][2]               = 64;
    picSetup.pic_control.pic_order_cnt_lsb                     = 29;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0] = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0] = 0;

    WritePicSetupSurface(30, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);
    picSetup.pic_control.l0[0]                                   = 0;
    picSetup.pic_control.l0[1]                                   = 6;
    picSetup.pic_control.l0[2]                                   = 4;
    picSetup.pic_control.l0[4]                                   = 0;
    picSetup.pic_control.l0[3]                                   = 8;
    picSetup.pic_control.l0[5]                                   = 6;
    picSetup.pic_control.l0[6]                                   = 4;
    picSetup.pic_control.l0[7]                                   = 8;
    picSetup.pic_control.l1[0]                                   = -2;
    picSetup.pic_control.l1[1]                                   = -2;
    picSetup.pic_control.l1[2]                                   = -2;
    picSetup.pic_control.l1[3]                                   = -2;
    picSetup.pic_control.temp_dist_l0[0]                         = 3;
    picSetup.pic_control.temp_dist_l0[1]                         = 6;
    picSetup.pic_control.temp_dist_l0[2]                         = 9;
    picSetup.pic_control.temp_dist_l0[3]                         = 12;
    picSetup.pic_control.temp_dist_l0[4]                         = 3;
    picSetup.pic_control.temp_dist_l0[5]                         = 6;
    picSetup.pic_control.temp_dist_l0[6]                         = 9;
    picSetup.pic_control.temp_dist_l0[7]                         = 12;
    picSetup.pic_control.temp_dist_l1[1]                         = -1;
    picSetup.pic_control.temp_dist_l1[2]                         = -1;
    picSetup.pic_control.temp_dist_l1[3]                         = -1;
    picSetup.pic_control.dist_scale_factor[0][1]                 = 768;
    picSetup.pic_control.dist_scale_factor[0][2]                 = 512;
    picSetup.pic_control.dist_scale_factor[0][3]                 = 1024;
    picSetup.pic_control.dist_scale_factor[1][0]                 = 85;
    picSetup.pic_control.dist_scale_factor[1][2]                 = 171;
    picSetup.pic_control.dist_scale_factor[1][3]                 = 341;
    picSetup.pic_control.dist_scale_factor[2][0]                 = 128;
    picSetup.pic_control.dist_scale_factor[2][1]                 = 384;
    picSetup.pic_control.dist_scale_factor[2][3]                 = 512;
    picSetup.pic_control.dist_scale_factor[3][0]                 = 64;
    picSetup.pic_control.dist_scale_factor[3][1]                 = 192;
    picSetup.pic_control.dist_scale_factor[3][2]                 = 128;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 33;
    picSetup.pic_control.pic_type                                = 0;
    picSetup.pic_control.ref_pic_flag                            = 1;
    picSetup.pic_control.nal_unit_type                           = 1;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 4;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 0;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 2;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 2;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 15;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 0;
    picSetup.pic_control.poc_entry_l0[0]                         = 0;
    picSetup.pic_control.poc_entry_l0[1]                         = 3;
    picSetup.pic_control.poc_entry_l0[2]                         = 2;
    picSetup.pic_control.poc_entry_l0[4]                         = 0;
    picSetup.pic_control.poc_entry_l0[3]                         = 4;
    picSetup.pic_control.poc_entry_l0[5]                         = 3;
    picSetup.pic_control.poc_entry_l0[6]                         = 2;
    picSetup.pic_control.poc_entry_l0[7]                         = 4;
    picSetup.pic_control.poc_entry_l1[0]                         = -1;
    picSetup.pic_control.poc_entry_l1[1]                         = -1;
    picSetup.pic_control.poc_entry_l1[2]                         = -1;
    picSetup.pic_control.poc_entry_l1[3]                         = -1;
    sliceControl.qp_avr                                          = 28;
    sliceControl.slice_type                                      = 0;
    sliceControl.num_ref_idx_active_override_flag                = 1;
    sliceControl.num_ref_idx_l0_active_minus1                    = 7;
    sliceControl.num_ref_idx_l1_active_minus1                    = 0;

    WritePicSetupSurface(31, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.l0[3]                                   = 2;
    picSetup.pic_control.l0[4]                                   = -2;
    picSetup.pic_control.l0[5]                                   = -2;
    picSetup.pic_control.l0[6]                                   = -2;
    picSetup.pic_control.l0[7]                                   = -2;
    picSetup.pic_control.l1[0]                                   = 2;
    picSetup.pic_control.l1[1]                                   = 0;
    picSetup.pic_control.l1[2]                                   = 6;
    picSetup.pic_control.l1[3]                                   = 4;
    picSetup.pic_control.temp_dist_l0[0]                         = 1;
    picSetup.pic_control.temp_dist_l0[1]                         = 4;
    picSetup.pic_control.temp_dist_l0[2]                         = 7;
    picSetup.pic_control.temp_dist_l0[3]                         = -2;
    picSetup.pic_control.temp_dist_l0[4]                         = -1;
    picSetup.pic_control.temp_dist_l0[5]                         = -1;
    picSetup.pic_control.temp_dist_l0[6]                         = -1;
    picSetup.pic_control.temp_dist_l0[7]                         = -1;
    picSetup.pic_control.temp_dist_l1[0]                         = -2;
    picSetup.pic_control.temp_dist_l1[1]                         = 1;
    picSetup.pic_control.temp_dist_l1[2]                         = 4;
    picSetup.pic_control.temp_dist_l1[3]                         = 7;
    picSetup.pic_control.dist_scale_factor[0][1]                 = -512;
    picSetup.pic_control.dist_scale_factor[0][2]                 = 1792;
    picSetup.pic_control.dist_scale_factor[1][0]                 = -128;
    picSetup.pic_control.dist_scale_factor[1][2]                 = -896;
    picSetup.pic_control.dist_scale_factor[1][3]                 = -512;
    picSetup.pic_control.dist_scale_factor[2][0]                 = 37;
    picSetup.pic_control.dist_scale_factor[2][1]                 = -73;
    picSetup.pic_control.dist_scale_factor[2][3]                 = 146;
    picSetup.pic_control.dist_scale_factor[3][1]                 = -128;
    picSetup.pic_control.dist_scale_factor[3][2]                 = 448;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0x04020108;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0xf0f0f0f0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 31;
    picSetup.pic_control.ref_pic_flag                            = 0;
    picSetup.pic_control.pic_type                                = 1;
    picSetup.pic_control.nal_unit_type                           = 0;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 3;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 0;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 0;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 7;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0]   = 1;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 1;
    picSetup.pic_control.poc_entry_l0[1]                         = 3;
    picSetup.pic_control.poc_entry_l0[2]                         = 2;
    picSetup.pic_control.poc_entry_l0[3]                         = 1;
    picSetup.pic_control.poc_entry_l0[4]                         = -1;
    picSetup.pic_control.poc_entry_l0[5]                         = -1;
    picSetup.pic_control.poc_entry_l0[6]                         = -1;
    picSetup.pic_control.poc_entry_l0[7]                         = -1;
    picSetup.pic_control.poc_entry_l1[0]                         = 1;
    picSetup.pic_control.poc_entry_l1[1]                         = 0;
    picSetup.pic_control.poc_entry_l1[2]                         = 3;
    picSetup.pic_control.poc_entry_l1[3]                         = 2;
    sliceControl.qp_avr                                          = 31;
    sliceControl.slice_type                                      = 1;
    sliceControl.num_ref_idx_active_override_flag                = 0;
    sliceControl.num_ref_idx_l0_active_minus1                    = 3;
    sliceControl.num_ref_idx_l1_active_minus1                    = 3;

    WritePicSetupSurface(32, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);
    picSetup.pic_control.temp_dist_l0[0]                       = 2;
    picSetup.pic_control.temp_dist_l0[1]                       = 5;
    picSetup.pic_control.temp_dist_l0[2]                       = 8;
    picSetup.pic_control.temp_dist_l0[3]                       = -1;
    picSetup.pic_control.temp_dist_l1[0]                       = -1;
    picSetup.pic_control.temp_dist_l1[1]                       = 2;
    picSetup.pic_control.temp_dist_l1[2]                       = 5;
    picSetup.pic_control.temp_dist_l1[3]                       = 8;
    picSetup.pic_control.dist_scale_factor[0][1]               = -128;
    picSetup.pic_control.dist_scale_factor[0][2]               = 1024;
    picSetup.pic_control.dist_scale_factor[0][3]               = 640;
    picSetup.pic_control.dist_scale_factor[1][0]               = -512;
    picSetup.pic_control.dist_scale_factor[1][2]               = -2048;
    picSetup.pic_control.dist_scale_factor[1][3]               = -1280;
    picSetup.pic_control.dist_scale_factor[2][0]               = 64;
    picSetup.pic_control.dist_scale_factor[2][1]               = -32;
    picSetup.pic_control.dist_scale_factor[2][3]               = 160;
    picSetup.pic_control.dist_scale_factor[3][0]               = 102;
    picSetup.pic_control.dist_scale_factor[3][1]               = -51;
    picSetup.pic_control.dist_scale_factor[3][2]               = 410;
    picSetup.pic_control.pic_order_cnt_lsb                     = 32;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0] = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0] = 0;

    WritePicSetupSurface(33, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);
    picSetup.pic_control.l0[1]                                   = 0;
    picSetup.pic_control.l0[0]                                   = 2;
    picSetup.pic_control.l0[2]                                   = 6;
    picSetup.pic_control.l0[3]                                   = 4;
    picSetup.pic_control.l0[4]                                   = 2;
    picSetup.pic_control.l0[5]                                   = 0;
    picSetup.pic_control.l0[6]                                   = 6;
    picSetup.pic_control.l0[7]                                   = 4;
    picSetup.pic_control.l1[0]                                   = -2;
    picSetup.pic_control.l1[1]                                   = -2;
    picSetup.pic_control.l1[2]                                   = -2;
    picSetup.pic_control.l1[3]                                   = -2;
    picSetup.pic_control.temp_dist_l0[0]                         = 3;
    picSetup.pic_control.temp_dist_l0[1]                         = 6;
    picSetup.pic_control.temp_dist_l0[2]                         = 9;
    picSetup.pic_control.temp_dist_l0[3]                         = 12;
    picSetup.pic_control.temp_dist_l0[4]                         = 3;
    picSetup.pic_control.temp_dist_l0[5]                         = 6;
    picSetup.pic_control.temp_dist_l0[6]                         = 9;
    picSetup.pic_control.temp_dist_l0[7]                         = 12;
    picSetup.pic_control.temp_dist_l1[1]                         = -1;
    picSetup.pic_control.temp_dist_l1[2]                         = -1;
    picSetup.pic_control.temp_dist_l1[3]                         = -1;
    picSetup.pic_control.dist_scale_factor[0][1]                 = 128;
    picSetup.pic_control.dist_scale_factor[0][2]                 = 512;
    picSetup.pic_control.dist_scale_factor[0][3]                 = 384;
    picSetup.pic_control.dist_scale_factor[1][0]                 = 512;
    picSetup.pic_control.dist_scale_factor[1][2]                 = 1024;
    picSetup.pic_control.dist_scale_factor[1][3]                 = 768;
    picSetup.pic_control.dist_scale_factor[2][0]                 = 128;
    picSetup.pic_control.dist_scale_factor[2][1]                 = 64;
    picSetup.pic_control.dist_scale_factor[2][3]                 = 192;
    picSetup.pic_control.dist_scale_factor[3][0]                 = 171;
    picSetup.pic_control.dist_scale_factor[3][1]                 = 85;
    picSetup.pic_control.dist_scale_factor[3][2]                 = 341;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 36;
    picSetup.pic_control.pic_type                                = 0;
    picSetup.pic_control.ref_pic_flag                            = 1;
    picSetup.pic_control.nal_unit_type                           = 1;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 4;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 0;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 2;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 2;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 15;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 0;
    picSetup.pic_control.poc_entry_l0[1]                         = 0;
    picSetup.pic_control.poc_entry_l0[0]                         = 1;
    picSetup.pic_control.poc_entry_l0[2]                         = 3;
    picSetup.pic_control.poc_entry_l0[3]                         = 2;
    picSetup.pic_control.poc_entry_l0[4]                         = 1;
    picSetup.pic_control.poc_entry_l0[5]                         = 0;
    picSetup.pic_control.poc_entry_l0[6]                         = 3;
    picSetup.pic_control.poc_entry_l0[7]                         = 2;
    picSetup.pic_control.poc_entry_l1[0]                         = -1;
    picSetup.pic_control.poc_entry_l1[1]                         = -1;
    picSetup.pic_control.poc_entry_l1[2]                         = -1;
    picSetup.pic_control.poc_entry_l1[3]                         = -1;
    sliceControl.qp_avr                                          = 28;
    sliceControl.slice_type                                      = 0;
    sliceControl.num_ref_idx_active_override_flag                = 1;
    sliceControl.num_ref_idx_l0_active_minus1                    = 7;
    sliceControl.num_ref_idx_l1_active_minus1                    = 0;

    WritePicSetupSurface(34, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.l0[3]                                   = 8;
    picSetup.pic_control.l0[4]                                   = -2;
    picSetup.pic_control.l0[5]                                   = -2;
    picSetup.pic_control.l0[6]                                   = -2;
    picSetup.pic_control.l0[7]                                   = -2;
    picSetup.pic_control.l1[0]                                   = 8;
    picSetup.pic_control.l1[1]                                   = 2;
    picSetup.pic_control.l1[2]                                   = 0;
    picSetup.pic_control.l1[3]                                   = 6;
    picSetup.pic_control.temp_dist_l0[0]                         = 1;
    picSetup.pic_control.temp_dist_l0[1]                         = 4;
    picSetup.pic_control.temp_dist_l0[2]                         = 7;
    picSetup.pic_control.temp_dist_l0[3]                         = -2;
    picSetup.pic_control.temp_dist_l0[4]                         = -1;
    picSetup.pic_control.temp_dist_l0[5]                         = -1;
    picSetup.pic_control.temp_dist_l0[6]                         = -1;
    picSetup.pic_control.temp_dist_l0[7]                         = -1;
    picSetup.pic_control.temp_dist_l1[0]                         = -2;
    picSetup.pic_control.temp_dist_l1[1]                         = 1;
    picSetup.pic_control.temp_dist_l1[2]                         = 4;
    picSetup.pic_control.temp_dist_l1[3]                         = 7;
    picSetup.pic_control.dist_scale_factor[0][1]                 = 64;
    picSetup.pic_control.dist_scale_factor[0][2]                 = 448;
    picSetup.pic_control.dist_scale_factor[0][3]                 = -128;
    picSetup.pic_control.dist_scale_factor[1][0]                 = 1024;
    picSetup.pic_control.dist_scale_factor[1][2]                 = 1792;
    picSetup.pic_control.dist_scale_factor[1][3]                 = -512;
    picSetup.pic_control.dist_scale_factor[2][0]                 = 146;
    picSetup.pic_control.dist_scale_factor[2][1]                 = 37;
    picSetup.pic_control.dist_scale_factor[2][3]                 = -73;
    picSetup.pic_control.dist_scale_factor[3][0]                 = -512;
    picSetup.pic_control.dist_scale_factor[3][1]                 = -128;
    picSetup.pic_control.dist_scale_factor[3][2]                 = -896;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0x04020108;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0xf0f0f0f0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 34;
    picSetup.pic_control.ref_pic_flag                            = 0;
    picSetup.pic_control.pic_type                                = 1;
    picSetup.pic_control.nal_unit_type                           = 0;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 3;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 0;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 0;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 7;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0]   = 1;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 1;
    picSetup.pic_control.poc_entry_l0[3]                         = 4;
    picSetup.pic_control.poc_entry_l0[4]                         = -1;
    picSetup.pic_control.poc_entry_l0[5]                         = -1;
    picSetup.pic_control.poc_entry_l0[6]                         = -1;
    picSetup.pic_control.poc_entry_l0[7]                         = -1;
    picSetup.pic_control.poc_entry_l1[0]                         = 4;
    picSetup.pic_control.poc_entry_l1[1]                         = 1;
    picSetup.pic_control.poc_entry_l1[2]                         = 0;
    picSetup.pic_control.poc_entry_l1[3]                         = 3;
    sliceControl.qp_avr                                          = 31;
    sliceControl.slice_type                                      = 1;
    sliceControl.num_ref_idx_active_override_flag                = 0;
    sliceControl.num_ref_idx_l0_active_minus1                    = 3;
    sliceControl.num_ref_idx_l1_active_minus1                    = 3;

    WritePicSetupSurface(35, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);
    picSetup.pic_control.temp_dist_l0[0]                       = 2;
    picSetup.pic_control.temp_dist_l0[1]                       = 5;
    picSetup.pic_control.temp_dist_l0[2]                       = 8;
    picSetup.pic_control.temp_dist_l0[3]                       = -1;
    picSetup.pic_control.temp_dist_l1[0]                       = -1;
    picSetup.pic_control.temp_dist_l1[1]                       = 2;
    picSetup.pic_control.temp_dist_l1[2]                       = 5;
    picSetup.pic_control.temp_dist_l1[3]                       = 8;
    picSetup.pic_control.dist_scale_factor[0][1]               = 102;
    picSetup.pic_control.dist_scale_factor[0][2]               = 410;
    picSetup.pic_control.dist_scale_factor[0][3]               = -51;
    picSetup.pic_control.dist_scale_factor[1][0]               = 640;
    picSetup.pic_control.dist_scale_factor[1][2]               = 1024;
    picSetup.pic_control.dist_scale_factor[1][3]               = -128;
    picSetup.pic_control.dist_scale_factor[2][0]               = 160;
    picSetup.pic_control.dist_scale_factor[2][1]               = 64;
    picSetup.pic_control.dist_scale_factor[2][3]               = -32;
    picSetup.pic_control.dist_scale_factor[3][0]               = -1280;
    picSetup.pic_control.dist_scale_factor[3][1]               = -512;
    picSetup.pic_control.dist_scale_factor[3][2]               = -2048;
    picSetup.pic_control.pic_order_cnt_lsb                     = 35;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0] = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0] = 0;

    WritePicSetupSurface(36, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.l0[0]                                   = 8;
    picSetup.pic_control.l0[2]                                   = 0;
    picSetup.pic_control.l0[1]                                   = 2;
    picSetup.pic_control.l0[3]                                   = 6;
    picSetup.pic_control.l0[4]                                   = 8;
    picSetup.pic_control.l0[5]                                   = 2;
    picSetup.pic_control.l0[6]                                   = 0;
    picSetup.pic_control.l0[7]                                   = 6;
    picSetup.pic_control.l1[0]                                   = -2;
    picSetup.pic_control.l1[1]                                   = -2;
    picSetup.pic_control.l1[2]                                   = -2;
    picSetup.pic_control.l1[3]                                   = -2;
    picSetup.pic_control.temp_dist_l0[0]                         = 3;
    picSetup.pic_control.temp_dist_l0[1]                         = 6;
    picSetup.pic_control.temp_dist_l0[2]                         = 9;
    picSetup.pic_control.temp_dist_l0[3]                         = 12;
    picSetup.pic_control.temp_dist_l0[4]                         = 3;
    picSetup.pic_control.temp_dist_l0[5]                         = 6;
    picSetup.pic_control.temp_dist_l0[6]                         = 9;
    picSetup.pic_control.temp_dist_l0[7]                         = 12;
    picSetup.pic_control.temp_dist_l1[1]                         = -1;
    picSetup.pic_control.temp_dist_l1[2]                         = -1;
    picSetup.pic_control.temp_dist_l1[3]                         = -1;
    picSetup.pic_control.dist_scale_factor[0][1]                 = 171;
    picSetup.pic_control.dist_scale_factor[0][2]                 = 341;
    picSetup.pic_control.dist_scale_factor[0][3]                 = 85;
    picSetup.pic_control.dist_scale_factor[1][0]                 = 384;
    picSetup.pic_control.dist_scale_factor[1][2]                 = 512;
    picSetup.pic_control.dist_scale_factor[1][3]                 = 128;
    picSetup.pic_control.dist_scale_factor[2][0]                 = 192;
    picSetup.pic_control.dist_scale_factor[2][1]                 = 128;
    picSetup.pic_control.dist_scale_factor[2][3]                 = 64;
    picSetup.pic_control.dist_scale_factor[3][0]                 = 768;
    picSetup.pic_control.dist_scale_factor[3][1]                 = 512;
    picSetup.pic_control.dist_scale_factor[3][2]                 = 1024;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 39;
    picSetup.pic_control.pic_type                                = 0;
    picSetup.pic_control.ref_pic_flag                            = 1;
    picSetup.pic_control.nal_unit_type                           = 1;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 4;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 0;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 2;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 2;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 15;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 0;
    picSetup.pic_control.poc_entry_l0[0]                         = 4;
    picSetup.pic_control.poc_entry_l0[2]                         = 0;
    picSetup.pic_control.poc_entry_l0[1]                         = 1;
    picSetup.pic_control.poc_entry_l0[3]                         = 3;
    picSetup.pic_control.poc_entry_l0[4]                         = 4;
    picSetup.pic_control.poc_entry_l0[5]                         = 1;
    picSetup.pic_control.poc_entry_l0[6]                         = 0;
    picSetup.pic_control.poc_entry_l0[7]                         = 3;
    picSetup.pic_control.poc_entry_l1[0]                         = -1;
    picSetup.pic_control.poc_entry_l1[1]                         = -1;
    picSetup.pic_control.poc_entry_l1[2]                         = -1;
    picSetup.pic_control.poc_entry_l1[3]                         = -1;
    sliceControl.qp_avr                                          = 28;
    sliceControl.slice_type                                      = 0;
    sliceControl.num_ref_idx_active_override_flag                = 1;
    sliceControl.num_ref_idx_l0_active_minus1                    = 7;
    sliceControl.num_ref_idx_l1_active_minus1                    = 0;

    WritePicSetupSurface(37, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.l0[3]                                   = 4;
    picSetup.pic_control.l0[4]                                   = -2;
    picSetup.pic_control.l0[5]                                   = -2;
    picSetup.pic_control.l0[6]                                   = -2;
    picSetup.pic_control.l0[7]                                   = -2;
    picSetup.pic_control.l1[0]                                   = 4;
    picSetup.pic_control.l1[1]                                   = 8;
    picSetup.pic_control.l1[2]                                   = 2;
    picSetup.pic_control.l1[3]                                   = 0;
    picSetup.pic_control.temp_dist_l0[0]                         = 1;
    picSetup.pic_control.temp_dist_l0[1]                         = 4;
    picSetup.pic_control.temp_dist_l0[2]                         = 7;
    picSetup.pic_control.temp_dist_l0[3]                         = -2;
    picSetup.pic_control.temp_dist_l0[4]                         = -1;
    picSetup.pic_control.temp_dist_l0[5]                         = -1;
    picSetup.pic_control.temp_dist_l0[6]                         = -1;
    picSetup.pic_control.temp_dist_l0[7]                         = -1;
    picSetup.pic_control.temp_dist_l1[0]                         = -2;
    picSetup.pic_control.temp_dist_l1[1]                         = 1;
    picSetup.pic_control.temp_dist_l1[2]                         = 4;
    picSetup.pic_control.temp_dist_l1[3]                         = 7;
    picSetup.pic_control.dist_scale_factor[0][1]                 = 146;
    picSetup.pic_control.dist_scale_factor[0][2]                 = -73;
    picSetup.pic_control.dist_scale_factor[0][3]                 = 37;
    picSetup.pic_control.dist_scale_factor[1][0]                 = 448;
    picSetup.pic_control.dist_scale_factor[1][2]                 = -128;
    picSetup.pic_control.dist_scale_factor[1][3]                 = 64;
    picSetup.pic_control.dist_scale_factor[2][0]                 = -896;
    picSetup.pic_control.dist_scale_factor[2][1]                 = -512;
    picSetup.pic_control.dist_scale_factor[2][3]                 = -128;
    picSetup.pic_control.dist_scale_factor[3][0]                 = 1792;
    picSetup.pic_control.dist_scale_factor[3][1]                 = 1024;
    picSetup.pic_control.dist_scale_factor[3][2]                 = -512;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0x04020108;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0xf0f0f0f0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 37;
    picSetup.pic_control.ref_pic_flag                            = 0;
    picSetup.pic_control.pic_type                                = 1;
    picSetup.pic_control.nal_unit_type                           = 0;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 3;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 0;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 0;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 7;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0]   = 1;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 1;
    picSetup.pic_control.poc_entry_l0[3]                         = 2;
    picSetup.pic_control.poc_entry_l0[4]                         = -1;
    picSetup.pic_control.poc_entry_l0[5]                         = -1;
    picSetup.pic_control.poc_entry_l0[6]                         = -1;
    picSetup.pic_control.poc_entry_l0[7]                         = -1;
    picSetup.pic_control.poc_entry_l1[0]                         = 2;
    picSetup.pic_control.poc_entry_l1[1]                         = 4;
    picSetup.pic_control.poc_entry_l1[2]                         = 1;
    picSetup.pic_control.poc_entry_l1[3]                         = 0;
    sliceControl.qp_avr                                          = 31;
    sliceControl.slice_type                                      = 1;
    sliceControl.num_ref_idx_active_override_flag                = 0;
    sliceControl.num_ref_idx_l0_active_minus1                    = 3;
    sliceControl.num_ref_idx_l1_active_minus1                    = 3;

    WritePicSetupSurface(38, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);
    picSetup.pic_control.temp_dist_l0[0]                       = 2;
    picSetup.pic_control.temp_dist_l0[1]                       = 5;
    picSetup.pic_control.temp_dist_l0[2]                       = 8;
    picSetup.pic_control.temp_dist_l0[3]                       = -1;
    picSetup.pic_control.temp_dist_l1[0]                       = -1;
    picSetup.pic_control.temp_dist_l1[1]                       = 2;
    picSetup.pic_control.temp_dist_l1[2]                       = 5;
    picSetup.pic_control.temp_dist_l1[3]                       = 8;
    picSetup.pic_control.dist_scale_factor[0][1]               = 160;
    picSetup.pic_control.dist_scale_factor[0][2]               = -32;
    picSetup.pic_control.dist_scale_factor[0][3]               = 64;
    picSetup.pic_control.dist_scale_factor[1][0]               = 410;
    picSetup.pic_control.dist_scale_factor[1][2]               = -51;
    picSetup.pic_control.dist_scale_factor[1][3]               = 102;
    picSetup.pic_control.dist_scale_factor[2][0]               = -2048;
    picSetup.pic_control.dist_scale_factor[2][1]               = -1280;
    picSetup.pic_control.dist_scale_factor[2][3]               = -512;
    picSetup.pic_control.dist_scale_factor[3][0]               = 1024;
    picSetup.pic_control.dist_scale_factor[3][1]               = 640;
    picSetup.pic_control.dist_scale_factor[3][2]               = -128;
    picSetup.pic_control.pic_order_cnt_lsb                     = 38;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0] = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0] = 0;

    WritePicSetupSurface(39, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.l0[0]                                   = 4;
    picSetup.pic_control.l0[1]                                   = 8;
    picSetup.pic_control.l0[3]                                   = 0;
    picSetup.pic_control.l0[2]                                   = 2;
    picSetup.pic_control.l0[4]                                   = 4;
    picSetup.pic_control.l0[5]                                   = 8;
    picSetup.pic_control.l0[6]                                   = 2;
    picSetup.pic_control.l0[7]                                   = 0;
    picSetup.pic_control.l1[0]                                   = -2;
    picSetup.pic_control.l1[1]                                   = -2;
    picSetup.pic_control.l1[2]                                   = -2;
    picSetup.pic_control.l1[3]                                   = -2;
    picSetup.pic_control.temp_dist_l0[0]                         = 3;
    picSetup.pic_control.temp_dist_l0[1]                         = 6;
    picSetup.pic_control.temp_dist_l0[2]                         = 9;
    picSetup.pic_control.temp_dist_l0[3]                         = 12;
    picSetup.pic_control.temp_dist_l0[4]                         = 3;
    picSetup.pic_control.temp_dist_l0[5]                         = 6;
    picSetup.pic_control.temp_dist_l0[6]                         = 9;
    picSetup.pic_control.temp_dist_l0[7]                         = 12;
    picSetup.pic_control.temp_dist_l1[1]                         = -1;
    picSetup.pic_control.temp_dist_l1[2]                         = -1;
    picSetup.pic_control.temp_dist_l1[3]                         = -1;
    picSetup.pic_control.dist_scale_factor[0][1]                 = 192;
    picSetup.pic_control.dist_scale_factor[0][2]                 = 64;
    picSetup.pic_control.dist_scale_factor[0][3]                 = 128;
    picSetup.pic_control.dist_scale_factor[1][0]                 = 341;
    picSetup.pic_control.dist_scale_factor[1][2]                 = 85;
    picSetup.pic_control.dist_scale_factor[1][3]                 = 171;
    picSetup.pic_control.dist_scale_factor[2][0]                 = 1024;
    picSetup.pic_control.dist_scale_factor[2][1]                 = 768;
    picSetup.pic_control.dist_scale_factor[2][3]                 = 512;
    picSetup.pic_control.dist_scale_factor[3][0]                 = 512;
    picSetup.pic_control.dist_scale_factor[3][1]                 = 384;
    picSetup.pic_control.dist_scale_factor[3][2]                 = 128;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 42;
    picSetup.pic_control.pic_type                                = 0;
    picSetup.pic_control.ref_pic_flag                            = 1;
    picSetup.pic_control.nal_unit_type                           = 1;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 4;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 0;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 2;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 2;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 15;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 0;
    picSetup.pic_control.poc_entry_l0[0]                         = 2;
    picSetup.pic_control.poc_entry_l0[1]                         = 4;
    picSetup.pic_control.poc_entry_l0[3]                         = 0;
    picSetup.pic_control.poc_entry_l0[2]                         = 1;
    picSetup.pic_control.poc_entry_l0[4]                         = 2;
    picSetup.pic_control.poc_entry_l0[5]                         = 4;
    picSetup.pic_control.poc_entry_l0[6]                         = 1;
    picSetup.pic_control.poc_entry_l0[7]                         = 0;
    picSetup.pic_control.poc_entry_l1[0]                         = -1;
    picSetup.pic_control.poc_entry_l1[1]                         = -1;
    picSetup.pic_control.poc_entry_l1[2]                         = -1;
    picSetup.pic_control.poc_entry_l1[3]                         = -1;
    sliceControl.qp_avr                                          = 28;
    sliceControl.slice_type                                      = 0;
    sliceControl.num_ref_idx_active_override_flag                = 1;
    sliceControl.num_ref_idx_l0_active_minus1                    = 7;
    sliceControl.num_ref_idx_l1_active_minus1                    = 0;

    WritePicSetupSurface(40, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.l0[3]                                   = 6;
    picSetup.pic_control.l0[4]                                   = -2;
    picSetup.pic_control.l0[5]                                   = -2;
    picSetup.pic_control.l0[6]                                   = -2;
    picSetup.pic_control.l0[7]                                   = -2;
    picSetup.pic_control.l1[0]                                   = 6;
    picSetup.pic_control.l1[1]                                   = 4;
    picSetup.pic_control.l1[2]                                   = 8;
    picSetup.pic_control.l1[3]                                   = 2;
    picSetup.pic_control.temp_dist_l0[0]                         = 1;
    picSetup.pic_control.temp_dist_l0[1]                         = 4;
    picSetup.pic_control.temp_dist_l0[2]                         = 7;
    picSetup.pic_control.temp_dist_l0[3]                         = -2;
    picSetup.pic_control.temp_dist_l0[4]                         = -1;
    picSetup.pic_control.temp_dist_l0[5]                         = -1;
    picSetup.pic_control.temp_dist_l0[6]                         = -1;
    picSetup.pic_control.temp_dist_l0[7]                         = -1;
    picSetup.pic_control.temp_dist_l1[0]                         = -2;
    picSetup.pic_control.temp_dist_l1[1]                         = 1;
    picSetup.pic_control.temp_dist_l1[2]                         = 4;
    picSetup.pic_control.temp_dist_l1[3]                         = 7;
    picSetup.pic_control.dist_scale_factor[0][1]                 = 37;
    picSetup.pic_control.dist_scale_factor[0][2]                 = -73;
    picSetup.pic_control.dist_scale_factor[0][3]                 = 146;
    picSetup.pic_control.dist_scale_factor[1][0]                 = 1792;
    picSetup.pic_control.dist_scale_factor[1][2]                 = -512;
    picSetup.pic_control.dist_scale_factor[1][3]                 = 1024;
    picSetup.pic_control.dist_scale_factor[2][0]                 = -896;
    picSetup.pic_control.dist_scale_factor[2][1]                 = -128;
    picSetup.pic_control.dist_scale_factor[2][3]                 = -512;
    picSetup.pic_control.dist_scale_factor[3][0]                 = 448;
    picSetup.pic_control.dist_scale_factor[3][1]                 = 64;
    picSetup.pic_control.dist_scale_factor[3][2]                 = -128;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0x04020108;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0xf0f0f0f0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 40;
    picSetup.pic_control.ref_pic_flag                            = 0;
    picSetup.pic_control.pic_type                                = 1;
    picSetup.pic_control.nal_unit_type                           = 0;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 3;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 0;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 0;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 7;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0]   = 1;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 1;
    picSetup.pic_control.poc_entry_l0[3]                         = 3;
    picSetup.pic_control.poc_entry_l0[4]                         = -1;
    picSetup.pic_control.poc_entry_l0[5]                         = -1;
    picSetup.pic_control.poc_entry_l0[6]                         = -1;
    picSetup.pic_control.poc_entry_l0[7]                         = -1;
    picSetup.pic_control.poc_entry_l1[0]                         = 3;
    picSetup.pic_control.poc_entry_l1[1]                         = 2;
    picSetup.pic_control.poc_entry_l1[2]                         = 4;
    picSetup.pic_control.poc_entry_l1[3]                         = 1;
    sliceControl.qp_avr                                          = 31;
    sliceControl.slice_type                                      = 1;
    sliceControl.num_ref_idx_active_override_flag                = 0;
    sliceControl.num_ref_idx_l0_active_minus1                    = 3;
    sliceControl.num_ref_idx_l1_active_minus1                    = 3;

    WritePicSetupSurface(41, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);
    picSetup.pic_control.temp_dist_l0[0]                       = 2;
    picSetup.pic_control.temp_dist_l0[1]                       = 5;
    picSetup.pic_control.temp_dist_l0[2]                       = 8;
    picSetup.pic_control.temp_dist_l0[3]                       = -1;
    picSetup.pic_control.temp_dist_l1[0]                       = -1;
    picSetup.pic_control.temp_dist_l1[1]                       = 2;
    picSetup.pic_control.temp_dist_l1[2]                       = 5;
    picSetup.pic_control.temp_dist_l1[3]                       = 8;
    picSetup.pic_control.dist_scale_factor[0][1]               = 64;
    picSetup.pic_control.dist_scale_factor[0][2]               = -32;
    picSetup.pic_control.dist_scale_factor[0][3]               = 160;
    picSetup.pic_control.dist_scale_factor[1][0]               = 1024;
    picSetup.pic_control.dist_scale_factor[1][2]               = -128;
    picSetup.pic_control.dist_scale_factor[1][3]               = 640;
    picSetup.pic_control.dist_scale_factor[2][0]               = -2048;
    picSetup.pic_control.dist_scale_factor[2][1]               = -512;
    picSetup.pic_control.dist_scale_factor[2][3]               = -1280;
    picSetup.pic_control.dist_scale_factor[3][0]               = 410;
    picSetup.pic_control.dist_scale_factor[3][1]               = 102;
    picSetup.pic_control.dist_scale_factor[3][2]               = -51;
    picSetup.pic_control.pic_order_cnt_lsb                     = 41;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0] = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0] = 0;

    WritePicSetupSurface(42, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.l0[0]                                   = 6;
    picSetup.pic_control.l0[1]                                   = 4;
    picSetup.pic_control.l0[2]                                   = 8;
    picSetup.pic_control.l0[3]                                   = 2;
    picSetup.pic_control.l0[4]                                   = 6;
    picSetup.pic_control.l0[5]                                   = 4;
    picSetup.pic_control.l0[6]                                   = 8;
    picSetup.pic_control.l0[7]                                   = 2;
    picSetup.pic_control.l1[0]                                   = -2;
    picSetup.pic_control.l1[1]                                   = -2;
    picSetup.pic_control.l1[2]                                   = -2;
    picSetup.pic_control.l1[3]                                   = -2;
    picSetup.pic_control.temp_dist_l0[0]                         = 3;
    picSetup.pic_control.temp_dist_l0[1]                         = 6;
    picSetup.pic_control.temp_dist_l0[2]                         = 9;
    picSetup.pic_control.temp_dist_l0[3]                         = 12;
    picSetup.pic_control.temp_dist_l0[4]                         = 3;
    picSetup.pic_control.temp_dist_l0[5]                         = 6;
    picSetup.pic_control.temp_dist_l0[6]                         = 9;
    picSetup.pic_control.temp_dist_l0[7]                         = 12;
    picSetup.pic_control.temp_dist_l1[1]                         = -1;
    picSetup.pic_control.temp_dist_l1[2]                         = -1;
    picSetup.pic_control.temp_dist_l1[3]                         = -1;
    picSetup.pic_control.dist_scale_factor[0][1]                 = 128;
    picSetup.pic_control.dist_scale_factor[0][2]                 = 64;
    picSetup.pic_control.dist_scale_factor[0][3]                 = 192;
    picSetup.pic_control.dist_scale_factor[1][0]                 = 512;
    picSetup.pic_control.dist_scale_factor[1][2]                 = 128;
    picSetup.pic_control.dist_scale_factor[1][3]                 = 384;
    picSetup.pic_control.dist_scale_factor[2][0]                 = 1024;
    picSetup.pic_control.dist_scale_factor[2][1]                 = 512;
    picSetup.pic_control.dist_scale_factor[2][3]                 = 768;
    picSetup.pic_control.dist_scale_factor[3][0]                 = 341;
    picSetup.pic_control.dist_scale_factor[3][1]                 = 171;
    picSetup.pic_control.dist_scale_factor[3][2]                 = 85;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 45;
    picSetup.pic_control.pic_type                                = 0;
    picSetup.pic_control.ref_pic_flag                            = 1;
    picSetup.pic_control.nal_unit_type                           = 1;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 4;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 0;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 2;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 2;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 15;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 0;
    picSetup.pic_control.poc_entry_l0[0]                         = 3;
    picSetup.pic_control.poc_entry_l0[1]                         = 2;
    picSetup.pic_control.poc_entry_l0[2]                         = 4;
    picSetup.pic_control.poc_entry_l0[3]                         = 1;
    picSetup.pic_control.poc_entry_l0[4]                         = 3;
    picSetup.pic_control.poc_entry_l0[5]                         = 2;
    picSetup.pic_control.poc_entry_l0[6]                         = 4;
    picSetup.pic_control.poc_entry_l0[7]                         = 1;
    picSetup.pic_control.poc_entry_l1[0]                         = -1;
    picSetup.pic_control.poc_entry_l1[1]                         = -1;
    picSetup.pic_control.poc_entry_l1[2]                         = -1;
    picSetup.pic_control.poc_entry_l1[3]                         = -1;
    sliceControl.qp_avr                                          = 28;
    sliceControl.slice_type                                      = 0;
    sliceControl.num_ref_idx_active_override_flag                = 1;
    sliceControl.num_ref_idx_l0_active_minus1                    = 7;
    sliceControl.num_ref_idx_l1_active_minus1                    = 0;

    WritePicSetupSurface(43, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);
    picSetup.pic_control.l0[3]                                   = 0;
    picSetup.pic_control.l0[4]                                   = -2;
    picSetup.pic_control.l0[5]                                   = -2;
    picSetup.pic_control.l0[6]                                   = -2;
    picSetup.pic_control.l0[7]                                   = -2;
    picSetup.pic_control.l1[0]                                   = 0;
    picSetup.pic_control.l1[1]                                   = 6;
    picSetup.pic_control.l1[2]                                   = 4;
    picSetup.pic_control.l1[3]                                   = 8;
    picSetup.pic_control.temp_dist_l0[0]                         = 1;
    picSetup.pic_control.temp_dist_l0[1]                         = 4;
    picSetup.pic_control.temp_dist_l0[2]                         = 7;
    picSetup.pic_control.temp_dist_l0[3]                         = -2;
    picSetup.pic_control.temp_dist_l0[4]                         = -1;
    picSetup.pic_control.temp_dist_l0[5]                         = -1;
    picSetup.pic_control.temp_dist_l0[6]                         = -1;
    picSetup.pic_control.temp_dist_l0[7]                         = -1;
    picSetup.pic_control.temp_dist_l1[0]                         = -2;
    picSetup.pic_control.temp_dist_l1[1]                         = 1;
    picSetup.pic_control.temp_dist_l1[2]                         = 4;
    picSetup.pic_control.temp_dist_l1[3]                         = 7;
    picSetup.pic_control.dist_scale_factor[0][1]                 = -512;
    picSetup.pic_control.dist_scale_factor[0][2]                 = -128;
    picSetup.pic_control.dist_scale_factor[0][3]                 = -896;
    picSetup.pic_control.dist_scale_factor[1][0]                 = -128;
    picSetup.pic_control.dist_scale_factor[1][2]                 = 64;
    picSetup.pic_control.dist_scale_factor[1][3]                 = 448;
    picSetup.pic_control.dist_scale_factor[2][0]                 = -512;
    picSetup.pic_control.dist_scale_factor[2][1]                 = 1024;
    picSetup.pic_control.dist_scale_factor[2][3]                 = 1792;
    picSetup.pic_control.dist_scale_factor[3][0]                 = -73;
    picSetup.pic_control.dist_scale_factor[3][1]                 = 146;
    picSetup.pic_control.dist_scale_factor[3][2]                 = 37;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0x04020108;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0xf0f0f0f0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 43;
    picSetup.pic_control.ref_pic_flag                            = 0;
    picSetup.pic_control.pic_type                                = 1;
    picSetup.pic_control.nal_unit_type                           = 0;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 3;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 0;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 0;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 7;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0]   = 1;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 1;
    picSetup.pic_control.poc_entry_l0[3]                         = 0;
    picSetup.pic_control.poc_entry_l0[4]                         = -1;
    picSetup.pic_control.poc_entry_l0[5]                         = -1;
    picSetup.pic_control.poc_entry_l0[6]                         = -1;
    picSetup.pic_control.poc_entry_l0[7]                         = -1;
    picSetup.pic_control.poc_entry_l1[0]                         = 0;
    picSetup.pic_control.poc_entry_l1[1]                         = 3;
    picSetup.pic_control.poc_entry_l1[2]                         = 2;
    picSetup.pic_control.poc_entry_l1[3]                         = 4;
    sliceControl.qp_avr                                          = 31;
    sliceControl.slice_type                                      = 1;
    sliceControl.num_ref_idx_active_override_flag                = 0;
    sliceControl.num_ref_idx_l0_active_minus1                    = 3;
    sliceControl.num_ref_idx_l1_active_minus1                    = 3;

    WritePicSetupSurface(44, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);
    picSetup.pic_control.temp_dist_l0[0]                       = 2;
    picSetup.pic_control.temp_dist_l0[1]                       = 5;
    picSetup.pic_control.temp_dist_l0[2]                       = 8;
    picSetup.pic_control.temp_dist_l0[3]                       = -1;
    picSetup.pic_control.temp_dist_l1[0]                       = -1;
    picSetup.pic_control.temp_dist_l1[1]                       = 2;
    picSetup.pic_control.temp_dist_l1[2]                       = 5;
    picSetup.pic_control.temp_dist_l1[3]                       = 8;
    picSetup.pic_control.dist_scale_factor[0][1]               = -1280;
    picSetup.pic_control.dist_scale_factor[0][2]               = -512;
    picSetup.pic_control.dist_scale_factor[0][3]               = -2048;
    picSetup.pic_control.dist_scale_factor[1][0]               = -51;
    picSetup.pic_control.dist_scale_factor[1][2]               = 102;
    picSetup.pic_control.dist_scale_factor[1][3]               = 410;
    picSetup.pic_control.dist_scale_factor[2][0]               = -128;
    picSetup.pic_control.dist_scale_factor[2][1]               = 640;
    picSetup.pic_control.dist_scale_factor[2][3]               = 1024;
    picSetup.pic_control.dist_scale_factor[3][0]               = -32;
    picSetup.pic_control.dist_scale_factor[3][1]               = 160;
    picSetup.pic_control.dist_scale_factor[3][2]               = 64;
    picSetup.pic_control.pic_order_cnt_lsb                     = 44;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0] = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0] = 0;

    WritePicSetupSurface(45, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);
    picSetup.pic_control.l0[0]                                   = 0;
    picSetup.pic_control.l0[1]                                   = 6;
    picSetup.pic_control.l0[2]                                   = 4;
    picSetup.pic_control.l0[4]                                   = 0;
    picSetup.pic_control.l0[3]                                   = 8;
    picSetup.pic_control.l0[5]                                   = 6;
    picSetup.pic_control.l0[6]                                   = 4;
    picSetup.pic_control.l0[7]                                   = 8;
    picSetup.pic_control.l1[0]                                   = -2;
    picSetup.pic_control.l1[1]                                   = -2;
    picSetup.pic_control.l1[2]                                   = -2;
    picSetup.pic_control.l1[3]                                   = -2;
    picSetup.pic_control.temp_dist_l0[0]                         = 3;
    picSetup.pic_control.temp_dist_l0[1]                         = 6;
    picSetup.pic_control.temp_dist_l0[2]                         = 9;
    picSetup.pic_control.temp_dist_l0[3]                         = 12;
    picSetup.pic_control.temp_dist_l0[4]                         = 3;
    picSetup.pic_control.temp_dist_l0[5]                         = 6;
    picSetup.pic_control.temp_dist_l0[6]                         = 9;
    picSetup.pic_control.temp_dist_l0[7]                         = 12;
    picSetup.pic_control.temp_dist_l1[1]                         = -1;
    picSetup.pic_control.temp_dist_l1[2]                         = -1;
    picSetup.pic_control.temp_dist_l1[3]                         = -1;
    picSetup.pic_control.dist_scale_factor[0][1]                 = 768;
    picSetup.pic_control.dist_scale_factor[0][2]                 = 512;
    picSetup.pic_control.dist_scale_factor[0][3]                 = 1024;
    picSetup.pic_control.dist_scale_factor[1][0]                 = 85;
    picSetup.pic_control.dist_scale_factor[1][2]                 = 171;
    picSetup.pic_control.dist_scale_factor[1][3]                 = 341;
    picSetup.pic_control.dist_scale_factor[2][0]                 = 128;
    picSetup.pic_control.dist_scale_factor[2][1]                 = 384;
    picSetup.pic_control.dist_scale_factor[2][3]                 = 512;
    picSetup.pic_control.dist_scale_factor[3][0]                 = 64;
    picSetup.pic_control.dist_scale_factor[3][1]                 = 192;
    picSetup.pic_control.dist_scale_factor[3][2]                 = 128;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 48;
    picSetup.pic_control.pic_type                                = 0;
    picSetup.pic_control.ref_pic_flag                            = 1;
    picSetup.pic_control.nal_unit_type                           = 1;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 4;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 0;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 2;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 2;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 15;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 0;
    picSetup.pic_control.poc_entry_l0[0]                         = 0;
    picSetup.pic_control.poc_entry_l0[1]                         = 3;
    picSetup.pic_control.poc_entry_l0[2]                         = 2;
    picSetup.pic_control.poc_entry_l0[4]                         = 0;
    picSetup.pic_control.poc_entry_l0[3]                         = 4;
    picSetup.pic_control.poc_entry_l0[5]                         = 3;
    picSetup.pic_control.poc_entry_l0[6]                         = 2;
    picSetup.pic_control.poc_entry_l0[7]                         = 4;
    picSetup.pic_control.poc_entry_l1[0]                         = -1;
    picSetup.pic_control.poc_entry_l1[1]                         = -1;
    picSetup.pic_control.poc_entry_l1[2]                         = -1;
    picSetup.pic_control.poc_entry_l1[3]                         = -1;
    sliceControl.qp_avr                                          = 28;
    sliceControl.slice_type                                      = 0;
    sliceControl.num_ref_idx_active_override_flag                = 1;
    sliceControl.num_ref_idx_l0_active_minus1                    = 7;
    sliceControl.num_ref_idx_l1_active_minus1                    = 0;

    WritePicSetupSurface(46, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    picSetup.pic_control.l0[3]                                   = 2;
    picSetup.pic_control.l0[4]                                   = -2;
    picSetup.pic_control.l0[5]                                   = -2;
    picSetup.pic_control.l0[6]                                   = -2;
    picSetup.pic_control.l0[7]                                   = -2;
    picSetup.pic_control.l1[0]                                   = 2;
    picSetup.pic_control.l1[1]                                   = 0;
    picSetup.pic_control.l1[2]                                   = 6;
    picSetup.pic_control.l1[3]                                   = 4;
    picSetup.pic_control.temp_dist_l0[0]                         = 1;
    picSetup.pic_control.temp_dist_l0[1]                         = 4;
    picSetup.pic_control.temp_dist_l0[2]                         = 7;
    picSetup.pic_control.temp_dist_l0[3]                         = -2;
    picSetup.pic_control.temp_dist_l0[4]                         = -1;
    picSetup.pic_control.temp_dist_l0[5]                         = -1;
    picSetup.pic_control.temp_dist_l0[6]                         = -1;
    picSetup.pic_control.temp_dist_l0[7]                         = -1;
    picSetup.pic_control.temp_dist_l1[0]                         = -2;
    picSetup.pic_control.temp_dist_l1[1]                         = 1;
    picSetup.pic_control.temp_dist_l1[2]                         = 4;
    picSetup.pic_control.temp_dist_l1[3]                         = 7;
    picSetup.pic_control.dist_scale_factor[0][1]                 = -512;
    picSetup.pic_control.dist_scale_factor[0][2]                 = 1792;
    picSetup.pic_control.dist_scale_factor[1][0]                 = -128;
    picSetup.pic_control.dist_scale_factor[1][2]                 = -896;
    picSetup.pic_control.dist_scale_factor[1][3]                 = -512;
    picSetup.pic_control.dist_scale_factor[2][0]                 = 37;
    picSetup.pic_control.dist_scale_factor[2][1]                 = -73;
    picSetup.pic_control.dist_scale_factor[2][3]                 = 146;
    picSetup.pic_control.dist_scale_factor[3][1]                 = -128;
    picSetup.pic_control.dist_scale_factor[3][2]                 = 448;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0x04020108;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0xf0f0f0f0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 46;
    picSetup.pic_control.ref_pic_flag                            = 0;
    picSetup.pic_control.pic_type                                = 1;
    picSetup.pic_control.nal_unit_type                           = 0;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 3;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 0;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 0;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 7;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0]   = 1;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 1;
    picSetup.pic_control.poc_entry_l0[3]                         = 1;
    picSetup.pic_control.poc_entry_l0[4]                         = -1;
    picSetup.pic_control.poc_entry_l0[5]                         = -1;
    picSetup.pic_control.poc_entry_l0[6]                         = -1;
    picSetup.pic_control.poc_entry_l0[7]                         = -1;
    picSetup.pic_control.poc_entry_l1[0]                         = 1;
    picSetup.pic_control.poc_entry_l1[1]                         = 0;
    picSetup.pic_control.poc_entry_l1[2]                         = 3;
    picSetup.pic_control.poc_entry_l1[3]                         = 2;
    sliceControl.qp_avr                                          = 31;
    sliceControl.slice_type                                      = 1;
    sliceControl.num_ref_idx_active_override_flag                = 0;
    sliceControl.num_ref_idx_l0_active_minus1                    = 3;
    sliceControl.num_ref_idx_l1_active_minus1                    = 3;

    WritePicSetupSurface(47, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);
    picSetup.pic_control.temp_dist_l0[0]                       = 2;
    picSetup.pic_control.temp_dist_l0[1]                       = 5;
    picSetup.pic_control.temp_dist_l0[2]                       = 8;
    picSetup.pic_control.temp_dist_l0[3]                       = -1;
    picSetup.pic_control.temp_dist_l1[0]                       = -1;
    picSetup.pic_control.temp_dist_l1[1]                       = 2;
    picSetup.pic_control.temp_dist_l1[2]                       = 5;
    picSetup.pic_control.temp_dist_l1[3]                       = 8;
    picSetup.pic_control.dist_scale_factor[0][1]               = -128;
    picSetup.pic_control.dist_scale_factor[0][2]               = 1024;
    picSetup.pic_control.dist_scale_factor[0][3]               = 640;
    picSetup.pic_control.dist_scale_factor[1][0]               = -512;
    picSetup.pic_control.dist_scale_factor[1][2]               = -2048;
    picSetup.pic_control.dist_scale_factor[1][3]               = -1280;
    picSetup.pic_control.dist_scale_factor[2][0]               = 64;
    picSetup.pic_control.dist_scale_factor[2][1]               = -32;
    picSetup.pic_control.dist_scale_factor[2][3]               = 160;
    picSetup.pic_control.dist_scale_factor[3][0]               = 102;
    picSetup.pic_control.dist_scale_factor[3][1]               = -51;
    picSetup.pic_control.dist_scale_factor[3][2]               = 410;
    picSetup.pic_control.pic_order_cnt_lsb                     = 47;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0] = 1;
    picSetup.pic_control.short_term_rps.delta_poc_s1_minus1[0] = 0;

    WritePicSetupSurface(48, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);
    picSetup.pic_control.l0[1]                                   = 0;
    picSetup.pic_control.l0[0]                                   = 2;
    picSetup.pic_control.l0[2]                                   = 6;
    picSetup.pic_control.l0[3]                                   = 4;
    picSetup.pic_control.l0[4]                                   = 2;
    picSetup.pic_control.l0[5]                                   = 0;
    picSetup.pic_control.l0[6]                                   = 6;
    picSetup.pic_control.l0[7]                                   = 4;
    picSetup.pic_control.l1[0]                                   = -2;
    picSetup.pic_control.l1[1]                                   = -2;
    picSetup.pic_control.l1[2]                                   = -2;
    picSetup.pic_control.l1[3]                                   = -2;
    picSetup.pic_control.temp_dist_l0[0]                         = 1;
    picSetup.pic_control.temp_dist_l0[1]                         = 4;
    picSetup.pic_control.temp_dist_l0[2]                         = 7;
    picSetup.pic_control.temp_dist_l0[3]                         = 10;
    picSetup.pic_control.temp_dist_l0[4]                         = 1;
    picSetup.pic_control.temp_dist_l0[5]                         = 4;
    picSetup.pic_control.temp_dist_l0[6]                         = 7;
    picSetup.pic_control.temp_dist_l0[7]                         = 10;
    picSetup.pic_control.temp_dist_l1[1]                         = -1;
    picSetup.pic_control.temp_dist_l1[2]                         = -1;
    picSetup.pic_control.temp_dist_l1[3]                         = -1;
    picSetup.pic_control.dist_scale_factor[0][1]                 = 64;
    picSetup.pic_control.dist_scale_factor[0][2]                 = 640;
    picSetup.pic_control.dist_scale_factor[0][3]                 = 448;
    picSetup.pic_control.dist_scale_factor[1][0]                 = 1024;
    picSetup.pic_control.dist_scale_factor[1][2]                 = 2560;
    picSetup.pic_control.dist_scale_factor[1][3]                 = 1792;
    picSetup.pic_control.dist_scale_factor[2][0]                 = 102;
    picSetup.pic_control.dist_scale_factor[2][1]                 = 26;
    picSetup.pic_control.dist_scale_factor[2][3]                 = 179;
    picSetup.pic_control.dist_scale_factor[3][0]                 = 146;
    picSetup.pic_control.dist_scale_factor[3][1]                 = 37;
    picSetup.pic_control.dist_scale_factor[3][2]                 = 366;
    picSetup.pic_control.diff_pic_order_cnt_zero[0]              = 0;
    picSetup.pic_control.diff_pic_order_cnt_zero[1]              = 0;
    picSetup.pic_control.pic_order_cnt_lsb                       = 49;
    picSetup.pic_control.pic_type                                = 0;
    picSetup.pic_control.ref_pic_flag                            = 1;
    picSetup.pic_control.nal_unit_type                           = 1;
    picSetup.pic_control.short_term_rps.num_negative_pics        = 4;
    picSetup.pic_control.short_term_rps.num_positive_pics        = 0;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[0]   = 0;
    picSetup.pic_control.short_term_rps.delta_poc_s0_minus1[3]   = 2;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s0_flag = 15;
    picSetup.pic_control.short_term_rps.used_by_lwrr_pic_s1_flag = 0;
    picSetup.pic_control.poc_entry_l0[1]                         = 0;
    picSetup.pic_control.poc_entry_l0[0]                         = 1;
    picSetup.pic_control.poc_entry_l0[2]                         = 3;
    picSetup.pic_control.poc_entry_l0[3]                         = 2;
    picSetup.pic_control.poc_entry_l0[4]                         = 1;
    picSetup.pic_control.poc_entry_l0[5]                         = 0;
    picSetup.pic_control.poc_entry_l0[6]                         = 3;
    picSetup.pic_control.poc_entry_l0[7]                         = 2;
    picSetup.pic_control.poc_entry_l1[0]                         = -1;
    picSetup.pic_control.poc_entry_l1[1]                         = -1;
    picSetup.pic_control.poc_entry_l1[2]                         = -1;
    picSetup.pic_control.poc_entry_l1[3]                         = -1;
    sliceControl.qp_avr                                          = 28;
    sliceControl.slice_type                                      = 0;
    sliceControl.num_ref_idx_active_override_flag                = 1;
    sliceControl.num_ref_idx_l0_active_minus1                    = 7;
    sliceControl.num_ref_idx_l1_active_minus1                    = 0;

    WritePicSetupSurface(49, picSetupSurface, &picSetup, &sliceControl, 1, &mdControl,
                         &qControl, &meControl);

    H265::VidParameterSet vps;
    vps.UnsetEmpty();
    vps.vps_video_parameter_set_id = 0;
    vps.vps_max_layers_minus1 = 0;
    vps.vps_max_sub_layers_minus1 = 0;
    vps.vps_temporal_id_nesting_flag = 1;
    vps.profile_tier_level.general_profile_space = 0;
    vps.profile_tier_level.general_tier_flag = 0;
    vps.profile_tier_level.general_profile_idc = 1;
    for (auto &f : vps.profile_tier_level.general_profile_compatibility_flag) { f = 0; }
    vps.profile_tier_level.SetMaxNumSubLayersMinus1(vps.vps_max_sub_layers_minus1);
    vps.profile_tier_level.general_profile_compatibility_flag[1] = 1;
    vps.profile_tier_level.general_progressive_source_flag = 0;
    vps.profile_tier_level.general_interlaced_source_flag = 0;
    vps.profile_tier_level.general_non_packed_constraint_flag = 0;
    vps.profile_tier_level.general_frame_only_constraint_flag = 0;
    vps.profile_tier_level.general_level_idc = 150;
    vps.vps_sub_layer_ordering_info_present_flag = 1;
    vps.vps_max_dec_pic_buffering_minus1[0] = 4;
    vps.vps_max_num_reorder_pics[0] = 1;
    vps.vps_max_latency_increase_plus1[0] = 0;
    vps.vps_max_layer_id = 0;
    vps.vps_num_layer_sets_minus1 = 0;
    vps.vps_timing_info_present_flag = 0;
    h265VpsNalus[H265_B_FRAMES_4K].CreateDataFromVPS(vps, 0, 1);

    H265::SeqParameterSet sps;
    sps.UnsetEmpty();
    sps.sps_video_parameter_set_id = 0;
    sps.sps_max_sub_layers_minus1 = 0;
    sps.sps_temporal_id_nesting_flag = 1;
    sps.profile_tier_level.general_profile_space = 0;
    sps.profile_tier_level.general_tier_flag = 0;
    sps.profile_tier_level.general_profile_idc = 1;
    for (auto &f : sps.profile_tier_level.general_profile_compatibility_flag) { f = 0; }
    sps.profile_tier_level.SetMaxNumSubLayersMinus1(sps.sps_max_sub_layers_minus1);
    sps.profile_tier_level.general_profile_compatibility_flag[1] = 1;
    sps.profile_tier_level.general_progressive_source_flag = 0;
    sps.profile_tier_level.general_interlaced_source_flag = 0;
    sps.profile_tier_level.general_non_packed_constraint_flag = 0;
    sps.profile_tier_level.general_frame_only_constraint_flag = 0;
    sps.profile_tier_level.general_level_idc = 150;
    sps.sps_seq_parameter_set_id = 0;
    sps.chroma_format_idc = 1;
    sps.pic_width_in_luma_samples = width;
    sps.pic_height_in_luma_samples = height;
    sps.conformance_window_flag = 1;
    sps.conf_win_left_offset = 0;
    sps.conf_win_right_offset = 0;
    sps.conf_win_top_offset = 0;
    sps.conf_win_bottom_offset = 8;
    sps.bit_depth_luma_minus8 = 0;
    sps.bit_depth_chroma_minus8 = 0;
    sps.log2_max_pic_order_cnt_lsb_minus4 = 4;
    sps.sps_sub_layer_ordering_info_present_flag = 1;
    sps.sps_max_dec_pic_buffering_minus1[0] = 4;
    sps.sps_max_num_reorder_pics[0] = 1;
    sps.sps_max_latency_increase_plus1[0] = 0;
    sps.log2_min_luma_coding_block_size_minus3 = 0;
    sps.log2_diff_max_min_luma_coding_block_size = 2;
    sps.log2_min_transform_block_size_minus2 = 0;
    sps.log2_diff_max_min_transform_block_size = 3;
    sps.max_transform_hierarchy_depth_inter = 3;
    sps.max_transform_hierarchy_depth_intra = 3;
    sps.scaling_list_enabled_flag = 0;
    sps.amp_enabled_flag = 1;
    sps.sample_adaptive_offset_enabled_flag = 1;
    sps.pcm_enabled_flag = 0;
    sps.short_term_ref_pic_sets.SetNumShortTermRefPicSets(1);
    auto &short_term_ref_pic_set = sps.short_term_ref_pic_sets.GetShortTermRefPicSet(0);
    short_term_ref_pic_set.num_negative_pics = 4;
    short_term_ref_pic_set.num_positive_pics = 0;
    short_term_ref_pic_set.delta_poc_s0_minus1[0] = 0;
    short_term_ref_pic_set.used_by_lwrr_pic_s0_flag[0] = 1;
    short_term_ref_pic_set.delta_poc_s0_minus1[1] = 0;
    short_term_ref_pic_set.used_by_lwrr_pic_s0_flag[1] = 0;
    short_term_ref_pic_set.delta_poc_s0_minus1[2] = 0;
    short_term_ref_pic_set.used_by_lwrr_pic_s0_flag[2] = 0;
    short_term_ref_pic_set.delta_poc_s0_minus1[3] = 0;
    short_term_ref_pic_set.used_by_lwrr_pic_s0_flag[3] = 0;
    sps.long_term_ref_pics_present_flag = 0;
    sps.sps_temporal_mvp_enabled_flag = 0;
    sps.strong_intra_smoothing_enabled_flag = 0;
    sps.vui_parameters_present_flag = 0;
    sps.sps_extension_present_flag = 0;
    h265SpsNalus[H265_B_FRAMES_4K].CreateDataFromSPS(sps, 0, 1);

    H265::PicParameterSet pps;
    pps.UnsetEmpty();
    pps.pps_pic_parameter_set_id = 0;
    pps.pps_seq_parameter_set_id = 0;
    pps.dependent_slice_segments_enabled_flag = 0;
    pps.output_flag_present_flag = 0;
    pps.num_extra_slice_header_bits = 0;
    pps.sign_data_hiding_enabled_flag = 0;
    pps.cabac_init_present_flag = 1;
    pps.num_ref_idx_l0_default_active_minus1 = 3;
    pps.num_ref_idx_l1_default_active_minus1 = 3;
    pps.init_qp_minus26 = 0;
    pps.constrained_intra_pred_flag = 0;
    pps.transform_skip_enabled_flag = 1;
    pps.lw_qp_delta_enabled_flag = 0;
    pps.pps_cb_qp_offset = 0;
    pps.pps_cr_qp_offset = 0;
    pps.pps_slice_chroma_qp_offsets_present_flag = 0;
    pps.weighted_pred_flag = 0;
    pps.weighted_bipred_flag = 0;
    pps.transquant_bypass_enabled_flag = 0;
    pps.tiles_enabled_flag = 0;
    pps.entropy_coding_sync_enabled_flag = 0;
    pps.pps_loop_filter_across_slices_enabled_flag = 1;
    pps.deblocking_filter_control_present_flag = 1;
    pps.deblocking_filter_override_enabled_flag = 0;
    pps.pps_deblocking_filter_disabled_flag = 0;
    pps.pps_beta_offset_div2 = 0;
    pps.pps_tc_offset_div2 = 0;
    pps.pps_scaling_list_data_present_flag = 0;
    pps.lists_modification_present_flag = 0;
    pps.log2_parallel_merge_level_minus2 = 0;
    pps.slice_segment_header_extension_present_flag = 0;
    pps.pps_extension_present_flag = 0;
    h265PpsNalus[H265_B_FRAMES_4K].CreateDataFromPPS(pps, 0, 1);

    return OK;
}

RC LWENCTest::GetEngineCount(UINT32* videoEncoderCount)
{
    if (GetBoundGpuSubdevice()->IsSOC())
    {
        channelClasses =
          CheetAh::SocPtr()->GetChannelClassList(m_LWENCAlloc.GetSupportedClass(GetBoundGpuDevice()));
        //Official VDK platform supports only one LWENC engine
        if (Platform::GetSimulationMode() == Platform::Fmodel)
        {
            *videoEncoderCount = 1;
        }
        else
        {
            *videoEncoderCount = static_cast<UINT32>(channelClasses.size());
        }
        return OK;
    }

    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS  engineParams = {0};

    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
             LW2080_CTRL_CMD_GPU_GET_ENGINES_V2,
             &engineParams,
             sizeof(engineParams)));

    for (UINT32 engine = 0; engine < engineParams.engineCount; engine++)
    {
        switch (engineParams.engineList[engine])
        {
            case LW2080_ENGINE_TYPE_LWENC0:
                *videoEncoderCount += 1;
                break;
            case LW2080_ENGINE_TYPE_LWENC1:
                *videoEncoderCount += 1;
                break;
            case LW2080_ENGINE_TYPE_LWENC2:
                *videoEncoderCount += 1;
                break;
        }
    }

    return rc;
}

UINT32 LWENCTest::GetSurfaceAlignment()
{
    return 256;
}

RC LWENCTest::InitializeEngines()
{
    RC rc;
    const bool bSoc = GetBoundGpuSubdevice()->IsSOC();
    const UINT32 alignment = GetSurfaceAlignment();
    m_pTestConfig->SetAllowMultipleChannels(true);

    UINT32 engNum = 0;
    UINT32 EngineMaskComp = ~m_EngineSkipMask;

    m_pGGSurfs = new GpuGoldenSurfaces(GetBoundGpuDevice());

    m_PerEngData.clear();
    for (UINT32 engine = 0; engNum < m_EngineCount; engine++)
    {
        if (!((1 << engine) & EngineMaskComp))
        {
            Printf(Tee::PriLow, "Skipping Engine number %d\n", engine);
            continue;
        }
        Printf(Tee::PriLow, "Using Engine number %d\n", engine);

        switch (m_LWENCAlloc.GetSupportedClass(GetBoundGpuDevice()))
        {
            case LWC0B7_VIDEO_ENCODER:
                m_PerEngData.push_back(make_unique<ClC0B7EngineData>());
                break;
            case LWD0B7_VIDEO_ENCODER:
                m_PerEngData.push_back(make_unique<ClD0B7EngineData>());
                break;
            case LWC1B7_VIDEO_ENCODER:
            case LWC2B7_VIDEO_ENCODER:
            case LWC3B7_VIDEO_ENCODER:
            case LWC4B7_VIDEO_ENCODER:
            case LWC5B7_VIDEO_ENCODER:
            case LWB4B7_VIDEO_ENCODER:
            case LWC7B7_VIDEO_ENCODER:
                m_PerEngData.push_back(make_unique<ClC1B7EngineData>());
                break;
            default:
                return RC::UNSUPPORTED_DEVICE;
                break;
        }

        if (!bSoc)
        {
            UINT32 engineId;
            CHECK_RC(m_LWENCAlloc.GetEngineId(GetBoundGpuDevice(),
                                              GetBoundRmClient(),
                                              engine,
                                              &engineId));
            CHECK_RC(m_pTestConfig->AllocateChannelWithEngine(&(m_PerEngData[engNum]->m_pCh),
                                                              &(m_PerEngData[engNum]->m_hCh),
                                                              &m_LWENCAlloc,
                                                              engineId));
            m_PerEngData[engNum]->m_Semaphore.SetWidth(0x10);
            m_PerEngData[engNum]->m_Semaphore.SetHeight(1);
            m_PerEngData[engNum]->m_Semaphore.SetColorFormat(ColorUtils::VOID32);
            m_PerEngData[engNum]->m_Semaphore.SetLocation(
                                                m_pTestConfig->NotifierLocation());
            m_PerEngData[engNum]->m_Semaphore.SetVASpace(m_VASpace);
            CHECK_RC(m_PerEngData[engNum]->m_Semaphore.Alloc(GetBoundGpuDevice()));
            CHECK_RC(SurfaceUtils::FillSurface(&m_PerEngData[engNum]->m_Semaphore, 0, 
                m_PerEngData[engNum]->m_Semaphore.GetBitsPerPixel(), GetBoundGpuSubdevice(), m_FillMethod));
            CHECK_RC(m_PerEngData[engNum]->m_Semaphore.Map());
            CHECK_RC(m_PerEngData[engNum]->m_Semaphore.BindGpuChannel(
                                                    m_PerEngData[engNum]->m_hCh));
        }
        else
        {
            // SOC uses WaitIdle instead of semaphores and since the LWENC engine is
            // not a GPU engine there is no allocation for it
            CHECK_RC(m_pTestConfig->AllocateChannel(&(m_PerEngData[engNum]->m_pCh),
                                                    &(m_PerEngData[engNum]->m_hCh)));
        }
        LWENCEngineData &surf = *m_PerEngData[engNum];

        surf.m_MappingType = m_MappingType;
        surf.m_VASpace     = m_VASpace;

        Memory::Location processingLocation = Memory::Optimal;
        if (m_AllocateInSysmem)
            processingLocation = Memory::Coherent;

        const UINT32 encodeStatusSize = static_cast<UINT32>(AlignUp<256>(sizeof(lwenc_stat_data_s)));
        surf.m_EncodeStatus.SetWidth(encodeStatusSize);
        surf.m_EncodeStatus.SetHeight(1);
        surf.m_EncodeStatus.SetColorFormat(ColorUtils::VOID32);
        surf.m_EncodeStatus.SetAlignment(alignment);
        surf.m_EncodeStatus.SetLocation(processingLocation);
        surf.m_EncodeStatus.SetVASpace(m_VASpace);
        CHECK_RC(surf.m_EncodeStatus.Alloc(GetBoundGpuDevice()));
        CHECK_RC(surf.m_EncodeStatus.Map());
        CHECK_RC(surf.m_EncodeStatus.BindGpuChannel(m_PerEngData[engNum]->m_hCh));

        const UINT32 taskStatusSize = sizeof(LwEnc_Status);
        surf.m_TaskStatus.SetWidth(taskStatusSize);
        surf.m_TaskStatus.SetHeight(1);
        surf.m_TaskStatus.SetColorFormat(ColorUtils::VOID32);
        surf.m_TaskStatus.SetAlignment(alignment);
        surf.m_TaskStatus.SetLocation(processingLocation);
        CHECK_RC(surf.m_TaskStatus.Alloc(GetBoundGpuDevice()));
        CHECK_RC(surf.m_TaskStatus.Map());
        CHECK_RC(surf.m_TaskStatus.BindGpuChannel(m_PerEngData[engNum]->m_hCh));

        const UINT32 IoRcProcessSize = static_cast<UINT32>(AlignUp<256>(sizeof(lwenc_persistent_state_s) * 3));
        surf.m_IoRcProcess.SetWidth(IoRcProcessSize);
        surf.m_IoRcProcess.SetHeight(1);
        surf.m_IoRcProcess.SetColorFormat(ColorUtils::VOID32);
        surf.m_IoRcProcess.SetAlignment(alignment);
        surf.m_IoRcProcess.SetLocation(processingLocation);
        surf.m_IoRcProcess.SetVASpace(m_VASpace);
        CHECK_RC(surf.m_IoRcProcess.Alloc(GetBoundGpuDevice()));
        CHECK_RC(surf.m_IoRcProcess.Map());
        CHECK_RC(surf.m_IoRcProcess.BindGpuChannel(m_PerEngData[engNum]->m_hCh));

        CHECK_RC(surf.InitRefPics(GetBoundGpuSubdevice(), processingLocation, alignment));

        //The below line according to CMOD driver code
        surf.m_IoHistory.SetWidth(HIST_BUF_SIZE(MAX_WIDTH, MAX_HEIGHT));
        surf.m_IoHistory.SetHeight(1);
        surf.m_IoHistory.SetColorFormat(ColorUtils::VOID32);
        surf.m_IoHistory.SetAlignment(alignment);
        surf.m_IoHistory.SetLocation(processingLocation);
        surf.m_IoHistory.SetVASpace(m_VASpace);
        CHECK_RC(surf.m_IoHistory.Alloc(GetBoundGpuDevice()));
        CHECK_RC(surf.m_IoHistory.BindGpuChannel(m_PerEngData[engNum]->m_hCh));

        for (UINT32 i = 0; i < NUM_STREAMS; i++)
        {
            CHECK_RC(surf.AllocateOutputStreamBuffer(i, alignment, processingLocation, GetBoundGpuDevice()));
        }   

        // souce from test_0000X.hdr
        surf.m_UcodeState.SetWidth(1600);
        surf.m_UcodeState.SetHeight(1);
        surf.m_UcodeState.SetColorFormat(ColorUtils::A8R8G8B8);
        surf.m_UcodeState.SetAlignment(alignment);
        surf.m_UcodeState.SetLocation(processingLocation);
        surf.m_UcodeState.SetVASpace(m_VASpace);
        CHECK_RC(surf.m_UcodeState.Alloc(GetBoundGpuDevice()));
        CHECK_RC(surf.m_UcodeState.BindGpuChannel(m_PerEngData[engNum]->m_hCh));

        // source from test_0000X.hdr
        surf.m_VP8EncStatIo.SetWidth(4992);
        surf.m_VP8EncStatIo.SetHeight(1);
        surf.m_VP8EncStatIo.SetColorFormat(ColorUtils::A8R8G8B8);
        surf.m_VP8EncStatIo.SetAlignment(alignment);
        surf.m_VP8EncStatIo.SetLocation(processingLocation);
        surf.m_VP8EncStatIo.SetVASpace(m_VASpace);
        CHECK_RC(surf.m_VP8EncStatIo.Alloc(GetBoundGpuDevice()));
        CHECK_RC(surf.m_VP8EncStatIo.BindGpuChannel(m_PerEngData[engNum]->m_hCh));

        for (UINT32 ColocIdx = 0;
             ColocIdx < sizeof(surf.m_ColocData)/sizeof(surf.m_ColocData[0]);
             ColocIdx++)
        {
            const UINT32 colocBufSize = COLOC_BUF_SIZE(MAX_WIDTH, MAX_HEIGHT);
            surf.m_ColocData[ColocIdx].SetWidth(colocBufSize / 4);
            surf.m_ColocData[ColocIdx].SetHeight(1);
            surf.m_ColocData[ColocIdx].SetColorFormat(ColorUtils::VOID32);
            surf.m_ColocData[ColocIdx].SetAlignment(alignment);
            surf.m_ColocData[ColocIdx].SetLocation(processingLocation);
            surf.m_ColocData[ColocIdx].SetVASpace(m_VASpace);
            CHECK_RC(surf.m_ColocData[ColocIdx].Alloc(GetBoundGpuDevice()));
            CHECK_RC(surf.m_ColocData[ColocIdx].BindGpuChannel(
                                                    m_PerEngData[engNum]->m_hCh));
        }

        for (UINT32 meIdx = 0; meIdx < NUMELEMS(surf.m_MepredData); ++meIdx)
        {
            const UINT32 mepredDataSize = (((MB_WIDTH(MAX_WIDTH)+3)&(~3)) * (MB_WIDTH(MAX_HEIGHT)+1));
            surf.m_MepredData[meIdx].SetWidth(mepredDataSize);
            surf.m_MepredData[meIdx].SetHeight(1);
            surf.m_MepredData[meIdx].SetColorFormat(ColorUtils::VOID32);
            surf.m_MepredData[meIdx].SetAlignment(alignment);
            surf.m_MepredData[meIdx].SetLocation(processingLocation);
            surf.m_MepredData[meIdx].SetVASpace(m_VASpace);
            CHECK_RC(surf.m_MepredData[meIdx].Alloc(GetBoundGpuDevice()));
            CHECK_RC(surf.m_MepredData[meIdx].BindGpuChannel(
                                                    m_PerEngData[engNum]->m_hCh));
        }

        for (UINT32 tempIdx = 0;
             tempIdx < NUMELEMS(surf.m_TemporalBuf); ++tempIdx)
        {
            const UINT32 temporalBufSize =
                TEMPORAL_BUF_SIZE(MAX_WIDTH, MAX_HEIGHT);
            surf.m_TemporalBuf[tempIdx].SetWidth(temporalBufSize / 4);
            surf.m_TemporalBuf[tempIdx].SetHeight(1);
            surf.m_TemporalBuf[tempIdx].SetColorFormat(ColorUtils::VOID32);
            surf.m_TemporalBuf[tempIdx].SetAlignment(alignment);
            surf.m_TemporalBuf[tempIdx].SetLocation(processingLocation);
            surf.m_TemporalBuf[tempIdx].SetVASpace(m_VASpace);
            CHECK_RC(surf.m_TemporalBuf[tempIdx].Alloc(GetBoundGpuDevice()));
            CHECK_RC(surf.m_TemporalBuf[tempIdx].BindGpuChannel(
                                                    m_PerEngData[engNum]->m_hCh));
        }

        surf.m_CounterBuf.SetWidth(COUNTER_BUF_SIZE / 4);
        surf.m_CounterBuf.SetHeight(1);
        surf.m_CounterBuf.SetColorFormat(ColorUtils::VOID32);
        surf.m_CounterBuf.SetAlignment(alignment);
        surf.m_CounterBuf.SetLocation(processingLocation);
        surf.m_CounterBuf.SetVASpace(m_VASpace);
        CHECK_RC(surf.m_CounterBuf.Alloc(GetBoundGpuDevice()));
        CHECK_RC(surf.m_CounterBuf.BindGpuChannel(m_PerEngData[engNum]->m_hCh));

        surf.m_ProbBuf.SetWidth(PROB_BUF_SIZE / 4);
        surf.m_ProbBuf.SetHeight(1);
        surf.m_ProbBuf.SetColorFormat(ColorUtils::VOID32);
        surf.m_ProbBuf.SetAlignment(alignment);
        surf.m_ProbBuf.SetLocation(processingLocation);
        surf.m_ProbBuf.SetVASpace(m_VASpace);
        CHECK_RC(surf.m_ProbBuf.Alloc(GetBoundGpuDevice()));
        CHECK_RC(surf.m_ProbBuf.BindGpuChannel(m_PerEngData[engNum]->m_hCh));

        const UINT32 combinedLineBufSize =
            COMBINED_LINE_BUF_SIZE(MAX_WIDTH, MAX_HEIGHT);
        surf.m_CombinedLineBuf.SetWidth(combinedLineBufSize / 4);
        surf.m_CombinedLineBuf.SetHeight(1);
        surf.m_CombinedLineBuf.SetColorFormat(ColorUtils::VOID32);
        surf.m_CombinedLineBuf.SetAlignment(alignment);
        surf.m_CombinedLineBuf.SetLocation(processingLocation);
        surf.m_CombinedLineBuf.SetVASpace(m_VASpace);
        CHECK_RC(surf.m_CombinedLineBuf.Alloc(GetBoundGpuDevice()));
        CHECK_RC(surf.m_CombinedLineBuf.BindGpuChannel(
                                            m_PerEngData[engNum]->m_hCh));

        const UINT32 filterLineBufSize =
            FILTER_LINE_BUF_SIZE(MAX_WIDTH, MAX_HEIGHT);
        surf.m_FilterLineBuf.SetWidth(filterLineBufSize / 4);
        surf.m_FilterLineBuf.SetHeight(1);
        surf.m_FilterLineBuf.SetColorFormat(ColorUtils::VOID32);
        surf.m_FilterLineBuf.SetAlignment(alignment);
        surf.m_FilterLineBuf.SetLocation(processingLocation);
        surf.m_FilterLineBuf.SetVASpace(m_VASpace);
        CHECK_RC(surf.m_FilterLineBuf.Alloc(GetBoundGpuDevice()));
        CHECK_RC(surf.m_FilterLineBuf.BindGpuChannel(
                                            m_PerEngData[engNum]->m_hCh));

        const UINT32 filterColLineBufSize =
            FILTER_COL_LINE_BUF_SIZE(MAX_WIDTH, MAX_HEIGHT);
        surf.m_FilterColLineBuf.SetWidth(filterColLineBufSize / 4);
        surf.m_FilterColLineBuf.SetHeight(1);
        surf.m_FilterColLineBuf.SetColorFormat(ColorUtils::VOID32);
        surf.m_FilterColLineBuf.SetAlignment(alignment);
        surf.m_FilterColLineBuf.SetLocation(processingLocation);
        surf.m_FilterColLineBuf.SetVASpace(m_VASpace);
        CHECK_RC(surf.m_FilterColLineBuf.Alloc(GetBoundGpuDevice()));
        CHECK_RC(surf.m_FilterColLineBuf.BindGpuChannel(
                                            m_PerEngData[engNum]->m_hCh));

        const UINT32 partitionBufSize =
            PARTITION_BUF_SIZE(MAX_WIDTH, MAX_HEIGHT);
        surf.m_PartitionBuf.SetWidth(partitionBufSize / 4);
        surf.m_PartitionBuf.SetHeight(1);
        surf.m_PartitionBuf.SetColorFormat(ColorUtils::VOID32);
        surf.m_PartitionBuf.SetAlignment(alignment);
        surf.m_PartitionBuf.SetLocation(processingLocation);
        surf.m_PartitionBuf.SetVASpace(m_VASpace);
        CHECK_RC(surf.m_PartitionBuf.Alloc(GetBoundGpuDevice()));
        CHECK_RC(surf.m_PartitionBuf.BindGpuChannel(
                                            m_PerEngData[engNum]->m_hCh));

        surf.m_PicSetup.SetWidth(MAX_NUM_FRAMES * maxControlStructsSize);
        surf.m_PicSetup.SetHeight(1);
        surf.m_PicSetup.SetColorFormat(ColorUtils::VOID32);
        surf.m_PicSetup.SetAlignment(alignment);
        surf.m_PicSetup.SetLocation(processingLocation);
        surf.m_PicSetup.SetVASpace(m_VASpace);
        CHECK_RC(surf.m_PicSetup.Alloc(GetBoundGpuDevice()));
        CHECK_RC(surf.m_PicSetup.Map());
        CHECK_RC(surf.m_PicSetup.BindGpuChannel(m_PerEngData[engNum]->m_hCh));
        
        //assuming size equal exactly to the size present in traces
        surf.m_CEAHintsData.SetWidth(CEA_HINTS_BUF_SIZE);
        surf.m_CEAHintsData.SetHeight(1);
        surf.m_CEAHintsData.SetColorFormat(ColorUtils::VOID32);    // assumption
        surf.m_CEAHintsData.SetAlignment(alignment);
        surf.m_CEAHintsData.SetLocation(processingLocation);
        CHECK_RC(surf.m_CEAHintsData.Alloc(GetBoundGpuDevice()));
        CHECK_RC(surf.m_CEAHintsData.Map());
        CHECK_RC(surf.m_CEAHintsData.BindGpuChannel(m_PerEngData[engNum]->m_hCh));

        if (!bSoc)
        {
            CHECK_RC(m_PerEngData[engNum]->m_pCh->SetObject(
                                                    s_ENC, m_LWENCAlloc.GetHandle()));
        }
        else
        {
            CHECK_RC(m_PerEngData[engNum]->m_pCh->SetClass(
                                  s_ENC,
                                   channelClasses[engNum]));
        }

        engNum += 1;
    }
    
    // Attach only outbitstream with index 0, then replace the surface with every new stream
    m_pGGSurfs->AttachSurface(&(m_PerEngData[0]->m_OutBitstream[0]), "C", 0);

    CHECK_RC(GetGoldelwalues()->SetSurfaces(m_pGGSurfs));
    m_pGolden->SetCheckCRCsUnique(false);
    GetGoldelwalues()->SetPrintCallback(&PrintCallback, this);

    DmaCopyAlloc alloc;
    DmaWrapper::PREF_TRANS_METH dmaMethod = DmaWrapper::COPY_ENGINE;
    if (!alloc.IsSupported(GetBoundGpuDevice()))
    {
        Printf(Tee::PriLow, "Copy Engine not supported. Using TwoD.\n");
        dmaMethod = DmaWrapper::TWOD;
    }
    if (GetBoundGpuSubdevice()->IsSOC())
    {
        dmaMethod = DmaWrapper::VIC;
    }
    CHECK_RC(m_dmaWrap.Initialize(
        GetTestConfiguration(),
        m_TestConfig.NotifierLocation(),
        dmaMethod));

    return rc;
}

void LWENCTest::GenWhiteNoise
(
    UINT08 *Yaddr,
    UINT32 Ypitch,
    UINT08 *UVaddr,
    UINT32 UVpitch,
    UINT32 width,
    UINT32 height,
    UINT32 randSeed
)
{
    Random rand;
    rand.SeedRandom(randSeed);

    // Note that we cannot just generate random Y, U, and V. First, not all YUV
    // values are valid RGB values. Second YUV are correlated, if generated
    // independently, they will produce a mostly grey image, because in this
    // case RGB values will be correlated.

    // Full size frame RGB values
    vector<UINT08> r(width * height);
    vector<UINT08> g(width * height);
    vector<UINT08> b(width * height);

    // Intermediate for resizing
    vector<float> uv(width * height);
    vector<float> vv(width * height);

    // All constants are taken from Recommendation ITU-R BT.709
    static constexpr float rgb2luma[] = { 0.2126f, 0.7152f, 0.0722f };
    static constexpr float rgb2u[] = { -0.2126f / 1.8556f, -0.7152f / 1.8556f,  0.9278f / 1.8556f };
    static constexpr float rgb2v[] = {  0.7874f / 1.5748f, -0.7152f / 1.5748f, -0.0722f / 1.5748f };
    static constexpr UINT08 lumaMin = 16;
    static constexpr UINT08 lumaMax = 235;
    static constexpr UINT08 uvMin = 16;
    static constexpr UINT08 uvMax = 240;
    static constexpr UINT08 uvZero = 128;

    // RGB values in ITU-R BT.709 are as well as luma also from 16 to 235, so
    // use lumaMin and lumaMax for random generation.
    auto gen = [&](){ return rand.GetRandom(lumaMin, lumaMax); };
    boost::generate(r, gen);
    boost::generate(g, gen);
    boost::generate(b, gen);

    UINT32 idx = 0;
    for (UINT32 y = 0; height > y; ++y, Yaddr += Ypitch)
    {
        for (UINT32 x = 0; width > x; ++x, ++idx)
        {
            // ITU-R BT.709 equations are for analog signals stretching from 0 to 1
            float er = (r[idx] - lumaMin) / float(lumaMax - lumaMin);
            float eg = (g[idx] - lumaMin) / float(lumaMax - lumaMin);
            float eb = (b[idx] - lumaMin) / float(lumaMax - lumaMin);

            // colwert to YUV according to ITU-R BT.709
            float luma = rgb2luma[0] * er + rgb2luma[1] * eg + rgb2luma[2] * eb;
            uv[idx]    =    rgb2u[0] * er +    rgb2u[1] * eg +    rgb2u[2] * eb;
            vv[idx]    =    rgb2v[0] * er +    rgb2v[1] * eg +    rgb2v[2] * eb;

            // back to digital range
            Yaddr[x] = Utility::RoundCast<UINT08>(luma * (lumaMax - lumaMin) + lumaMin);
        }
    }

    // Resize U and V
    for (UINT32 y = 0; height / 2 > y; ++y, UVaddr += UVpitch)
    {
        for (UINT32 x = 0; width / 2 > x; ++x)
        {
            // Callwlate simple average of 4 pixels for resizing. It's not
            // exactly correct, because chroma samples in YUV420 are located
            // like this:
            // x x x x x ...
            // o   o   o ...
            // x x x x x ...,
            // where x is luma sample, and o is chroma sample. But it doesn't
            // matter for our purposes.
            float u1 = uv[(2 * y    ) * width + x * 2];
            float u2 = uv[(2 * y + 1) * width + x * 2];
            float u3 = uv[(2 * y    ) * width + x * 2 + 1];
            float u4 = uv[(2 * y + 1) * width + x * 2 + 1];

            float u = (u1 + u2 + u3 + u4) / 4.0f;

            float v1 = vv[(2 * y    ) * width + x * 2];
            float v2 = vv[(2 * y + 1) * width + x * 2];
            float v3 = vv[(2 * y    ) * width + x * 2 + 1];
            float v4 = vv[(2 * y + 1) * width + x * 2 + 1];

            float v = (v1 + v2 + v3 + v4) / 4.0f;

            // back to digital range as defined in ITU-R BT.709
            UVaddr[2 * x] = Utility::RoundCast<UINT08>(u * (uvMax - uvMin) + uvZero);
            UVaddr[2 * x + 1] = Utility::RoundCast<UINT08>(v * (uvMax - uvMin) + uvZero);
        }
    }
}

void LWENCTest::GenWhiteNoiseThreadFunc(void *)
{
    Tasker::DetachThread detachThread;

    auto surfIdx = Tasker::GetThreadIdx();
    auto &Ysurf = get<0>(s_4KWhiteNoise[surfIdx]);
    auto &UVsurf = get<1>(s_4KWhiteNoise[surfIdx]);
    GenWhiteNoise
    (
        static_cast<UINT08*>(Ysurf.GetAddress()),
        Ysurf.GetPitch(),
        static_cast<UINT08*>(UVsurf.GetAddress()),
        UVsurf.GetPitch(),
        s_4KWhiteNoiseWidth,
        s_4KWhiteNoiseHeight,
        surfIdx // produce deterministic result
    );
}

RC LWENCTest::Setup()
{
    RC rc;
    UINT32 videoEncoderCount = 0;

    GpuSubdevice *gpuSubdevice = GetBoundGpuSubdevice();
    GpuDevice *gpuDevice = GetBoundGpuDevice();

    CHECK_RC(GpuTest::Setup());
    CHECK_RC(GetEngineCount(&videoEncoderCount));

    if (gpuSubdevice->IsSOC())
    {
        m_VASpace = Surface2D::TegraVASpace;
    }

    // totalEngines is the total number of LWENC engines on the chip which are
    // not masked, while m_EngineCount is 1 for golden generation and
    // totalEngines otherwise.
    UINT32 totalEngines;
    totalEngines = CountBits(
             ((UINT32)(1 << videoEncoderCount) - 1) & (~m_EngineSkipMask));

    if (totalEngines == 0)
    {
        Printf(Tee::PriError,
               "Number of Engines being tested is zero!! EngineSkipMask"
               " and floorswept engine values might be probable causes\n");
        return RC::ILWALID_INPUT;
    }

    m_EngineCount = ((m_pGolden->GetAction() == Goldelwalues::Check) ? totalEngines : 1);

    CHECK_RC(InitializeEngines());

    //When the SubDevice is an SOC, always using the CPU to fill the surface
    if (gpuSubdevice->IsSOC())
        m_FillMethod = SurfaceUtils::FillMethod::Cpufill;

    const UINT32 className = m_LWENCAlloc.GetSupportedClass(GetBoundGpuDevice());

    SetDefaultStreamSkipMask(gpuSubdevice, className, &m_StreamSkipMask);

    const UINT32 allStreamSkipMask = ((1 << NUM_STREAMS) - 1);
    if ((m_StreamSkipMask & allStreamSkipMask) == allStreamSkipMask)
    {
        Printf(Tee::PriError, "All supported streams are being skipped\n");
        return RC::ILWALID_TEST_INPUT;
    }

    if (0 != (s_StreamsUsing4kNoise & ~m_StreamSkipMask))
    {
        UINT64 freq = Xp::QueryPerformanceFrequency();
        UINT64 start, finish;

        start = Xp::QueryPerformanceCounter();
        for (auto &yuv : s_4KWhiteNoise)
        {
            auto &Ysurf = get<0>(yuv);
            auto &UVsurf = get<1>(yuv);

            const UINT32 pitchAlign = m_dmaWrap.GetPitchAlignRequirement();

            Ysurf.SetWidth(s_4KWhiteNoiseWidth);
            Ysurf.SetHeight(s_4KWhiteNoiseHeight);
            Ysurf.SetPitch(AlignUp(s_4KWhiteNoiseWidth, pitchAlign));
            Ysurf.SetColorFormat(ColorUtils::Y8);
            Ysurf.SetLocation(Memory::Optimal);
            Ysurf.SetProtect(Memory::ReadWrite);
            Ysurf.SetLayout(Surface2D::Pitch);
            Ysurf.SetType(LWOS32_TYPE_IMAGE);
            CHECK_RC(Ysurf.Alloc(gpuDevice));
            CHECK_RC(Ysurf.Map());

            UVsurf.SetWidth(s_4KWhiteNoiseWidth);
            UVsurf.SetHeight(s_4KWhiteNoiseHeight);
            UVsurf.SetPitch(AlignUp(s_4KWhiteNoiseWidth, pitchAlign));
            UVsurf.SetColorFormat(ColorUtils::Y8);
            UVsurf.SetLocation(Memory::Optimal);
            UVsurf.SetProtect(Memory::ReadWrite);
            UVsurf.SetLayout(Surface2D::Pitch);
            UVsurf.SetType(LWOS32_TYPE_IMAGE);
            CHECK_RC(UVsurf.Alloc(gpuDevice));
            CHECK_RC(UVsurf.Map());
        }
        auto threads = Tasker::CreateThreadGroup
        (
            &GenWhiteNoiseThreadFunc,
            nullptr,
            static_cast<UINT32>(NUMELEMS(s_4KWhiteNoise)),
            "4k YUV white noise generation",
            true,
            nullptr
        );
        CHECK_RC(Tasker::Join(threads));
        for (auto &yuv : s_4KWhiteNoise)
        {
            auto &Ysurf = get<0>(yuv);
            auto &UVsurf = get<1>(yuv);
            Ysurf.Unmap();
            UVsurf.Unmap();
        }
        finish = Xp::QueryPerformanceCounter();
        VerbosePrintf("Time to generate random surfaces = %lfs\n", double(finish - start) / freq);
    }

    return rc;
}

RC LWENCTest::Cleanup()
{
    StickyRC firstRC;

    m_LWENCAlloc.Free();
    m_dmaWrap.Cleanup();

    for (UINT32 engNum = 0; engNum < m_PerEngData.size(); engNum++)
    {
        LWENCEngineData &surf = *m_PerEngData[engNum];
        if (m_PerEngData[engNum]->m_pCh)
        {
            firstRC = m_pTestConfig->FreeChannel(m_PerEngData[engNum]->m_pCh);
            m_PerEngData[engNum]->m_pCh = 0;
        }
        m_PerEngData[engNum]->m_Semaphore.Free();
        surf.m_EncodeStatus.Free();
        surf.m_IoRcProcess.Free();
        surf.FreeRefPics();
        surf.m_IoHistory.Free();
         
        for (UINT32 i = 0; i < NUM_STREAMS; i++)
        {
            surf.m_OutBitstream[i].Free();
            surf.m_OutBitstreamRes[i].Free();
        }

        for (UINT32 colocIdx = 0; colocIdx < NUMELEMS(surf.m_ColocData); ++colocIdx)
        {
            surf.m_ColocData[colocIdx].Free();
        }
        for (UINT32 meIdx = 0; meIdx < NUMELEMS(surf.m_MepredData); ++meIdx)
        {
            surf.m_MepredData[meIdx].Free();
        }
        for (UINT32 tempIdx = 0;
             tempIdx < NUMELEMS(surf.m_TemporalBuf); ++tempIdx)
        {
            surf.m_TemporalBuf[tempIdx].Free();
        }
        surf.m_CounterBuf.Free();
        surf.m_ProbBuf.Free();
        surf.m_CombinedLineBuf.Free();
        surf.m_FilterLineBuf.Free();
        surf.m_FilterColLineBuf.Free();
        surf.m_PartitionBuf.Free();
        surf.m_PicSetup.Free();
        surf.m_UcodeState.Free();
        surf.m_VP8EncStatIo.Free();
        surf.m_CEAHintsData.Free();
        surf.m_TaskStatus.Free();
    }
    m_PerEngData.clear();
    m_YInput.Free();
    m_UVInput.Free();
    m_YInputPitchLinear.Free();
    m_UVInputPitchLinear.Free();

    for (auto &yuv : s_4KWhiteNoise)
    {
        get<0>(yuv).Free();
        get<1>(yuv).Free();
    }

    m_pGolden->ClearSurfaces();
    delete m_pGGSurfs;
    m_pGGSurfs = NULL;

    firstRC = GpuTest::Cleanup();

    return firstRC;
}

void LWENCTest::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);

    Printf(pri, "LWENC test Js Properties:\n");
    Printf(pri, "\tKeepRunning:\t\t\t%s\n", m_KeepRunning ? "true" : "false");
    Printf(pri, "\tSaveSurfaces:\t\t\t%s\n", m_SaveSurfaces ? "true" : "false");
    Printf(pri, "\tSaveStreams:\t\t\t%s\n", m_SaveStreams ? "true" : "false");
    Printf(pri, "\tAllocateInSysmem:\t\t%s\n", m_AllocateInSysmem ? "true" : "false");
    Printf(pri, "\tMaxFrames:\t\t\t%u\n", m_MaxFrames);
    Printf(pri, "\tStreamSkipMask:\t\t\t0x%08x\n", m_StreamSkipMask);
    Printf(pri, "\tEngineSkipMask:\t\t\t0x%08x\n", m_EngineSkipMask);
    Printf(pri, "\tStreamYUVInputFromFilesMask:\t0x%08x\n", m_StreamYUVInputFromFilesMask);
    Printf(pri, "\tYUVInputPath:\t\t\t%s\n", m_YUVInputPath.c_str());
}

void LWENCTest::SaveSurfaces(UINT32 streamIndex, UINT32 frameIdx, UINT32 engNum)
{
    if (!m_SaveSurfaces)
        return;
    SaveSurface(&m_YInputPitchLinear, "YInputPitch", streamIndex, frameIdx, engNum);
    SaveSurface(&m_UVInputPitchLinear, "UVInputPitch", streamIndex, frameIdx, engNum);
    SaveSurface(&(m_PerEngData[engNum]->m_EncodeStatus), "EncodeStatus",
                                                        streamIndex, frameIdx, engNum);
    SaveSurface(&(m_PerEngData[engNum]->m_IoRcProcess), "IoRcProcess",
                                                        streamIndex, frameIdx, engNum);
    m_PerEngData[engNum]->SaveRefSurfaces(streamIndex, frameIdx, engNum,
                                         &m_dmaWrap, GetBoundGpuSubdevice(), m_pTestConfig);
    SaveSurface(&(m_PerEngData[engNum]->m_IoHistory), "IoHistory",
                                                        streamIndex, frameIdx, engNum);
    SaveSurface(&(m_PerEngData[engNum]->m_ColocData[0]), "ColocData0",
                                                        streamIndex, frameIdx, engNum);
    SaveSurface(&(m_PerEngData[engNum]->m_ColocData[1]), "ColocData1",
                                                        streamIndex, frameIdx, engNum);
    SaveSurface(&(m_PerEngData[engNum]->m_MepredData[0]), "MepredData0",
                                                        streamIndex, frameIdx, engNum);
    SaveSurface(&(m_PerEngData[engNum]->m_MepredData[1]), "MepredData1",
                                                        streamIndex, frameIdx, engNum);
    SaveSurface(&(m_PerEngData[engNum]->m_CounterBuf), "CounterBuf",
                                                        streamIndex, frameIdx, engNum);
    SaveSurface(&(m_PerEngData[engNum]->m_ProbBuf), "ProbBuf",
                                                        streamIndex, frameIdx, engNum);
    SaveSurface(&(m_PerEngData[engNum]->m_CombinedLineBuf), "CombinedLineBuf",
                                                        streamIndex, frameIdx, engNum);
    SaveSurface(&(m_PerEngData[engNum]->m_FilterLineBuf), "FilterLineBuf",
                                                        streamIndex, frameIdx, engNum);
    SaveSurface(&(m_PerEngData[engNum]->m_FilterColLineBuf), "FilterColLineBuf",
                                                        streamIndex, frameIdx, engNum);
    SaveSurface(&(m_PerEngData[engNum]->m_PartitionBuf), "PartitionBuf",
                                                        streamIndex, frameIdx, engNum);
    SaveSurface(&(m_PerEngData[engNum]->m_PicSetup), "PicSetup",
                                                        streamIndex, frameIdx, engNum);
    SaveSurface(&(m_PerEngData[engNum]->m_OutBitstream[streamIndex]), "OutBitstream",
                                                        streamIndex, frameIdx, engNum);
    SaveSurface(&(m_PerEngData[engNum]->m_OutBitstreamRes[streamIndex]), "OutBitstreamRes",
                                                        streamIndex, frameIdx, engNum);
    SaveSurface(&(m_PerEngData[engNum]->m_UcodeState), "UcodeState",
                                                        streamIndex, frameIdx, engNum);
    SaveSurface(&(m_PerEngData[engNum]->m_VP8EncStatIo), "VP8EncStatIo",
                                                        streamIndex, frameIdx, engNum);
}

RC LWENCTest::FillYUVInput(UINT32 streamIndex, UINT32 frameIdx)
{
    RC rc;

    if (0 != ((1 << streamIndex) & s_StreamsUsing4kNoise))
    {
        return Fill4KNoise(streamIndex, frameIdx);
    }

    const UINT32 alignment = GetSurfaceAlignment();

    /* for VP8, data collected from *.hdr. Line of "
    FILE YUV_INPUT_LUMA_0000X.bin ...
    FILE YUV_INPUT_CHROMA_0000X.bin ...
    " */
    static const UINT32 lumaSurfacePitch[NUM_STREAMS] =
    {
        192,    // H264_BASELINE_CAVLC
        192,    // H264_HP_CABAC
        192,    // H264_MVC_HP_CABAC
        1920,   // H264_HD_HP_CABAC_MS
        192,    // H264_B_FRAMES
        192,    // H265_GENERIC
        384,    // H265_10BIT_444
        192,    // VP9_GENERIC
        192,    // VP8_GENERIC
        3840,   // H265_4K_40F
        192,    // H265_MULTIREF_BFRAME
        320,    // OPTICAL_FLOW
        320,    // STEREO
        3840,   // H265_B_FRAMES_4K
    };

    static const UINT32 lumaSurfaceHeight[NUM_STREAMS] =
    {
        144,    // H264_BASELINE_CAVLC
        144,    // H264_HP_CABAC
        144,    // H264_MVC_HP_CABAC
        1024,   // H264_HD_HP_CABAC_MS
        192,    // H264_B_FRAMES
        160,    // H265_GENERIC
        160,    // H265_10BIT_444
        192,    // VP9_GENERIC
        256,    // VP8_GENERIC
        2176,   // H265_4K_40F
        160,    // H265_MULTIREF_BFRAME
        96,     // OPTICAL_FLOW
        96,     // STEREO
        2176,   // H265_B_FRAMES_4K
    };

    static const UINT32 chromaSurfacePitch[NUM_STREAMS] =
    {
        192,    // H264_BASELINE_CAVLC
        192,    // H264_HP_CABAC
        192,    // H264_MVC_HP_CABAC
        1920,   // H264_HD_HP_CABAC_MS
        192,    // H264_B_FRAMES
        192,    // H265_GENERIC
        768,    // H265_10BIT_444
        192,    // VP9_GENERIC
        192,    // VP8_GENERIC
        3840,   // H265_4K_40F
        192,    // H265_MULTIREF_BFRAME
        320,    // OPTICAL_FLOW
        320,    // STEREO
        3840,   // H265_B_FRAMES_4K
    };

    static const UINT32 chromaSurfaceHeight[NUM_STREAMS] =
    {
        80,     // H264_BASELINE_CAVLC
        80,     // H264_HP_CABAC
        80,     // H264_MVC_HP_CABAC
        512,    // H264_HD_HP_CABAC_MS
        80,     // H264_B_FRAMES
        80,     // H265_GENERIC
        160,    // H265_10BIT_444
        96,     // VP9_GENERIC
        128,    // VP8_GENERIC
        1088,   // H265_4K_40F
        80,     // H265_MULTIREF_BFRAME
        96,     // OPTICAL_FLOW
        96,     // STEREO
        1088,   // H265_B_FRAMES_4K
    };

    UINT32 pitchAlign = m_dmaWrap.GetPitchAlignRequirement();

    if (frameIdx == 0)
    {
        m_YInputPitchLinear.Free();
        m_YInputPitchLinear.SetWidth(lumaSurfacePitch[streamIndex]);
        m_YInputPitchLinear.SetPitch(AlignUp(lumaSurfacePitch[streamIndex], pitchAlign));
        m_YInputPitchLinear.SetHeight(lumaSurfaceHeight[streamIndex]);
        m_YInputPitchLinear.SetColorFormat(ColorUtils::Y8);
        m_YInputPitchLinear.SetAlignment(alignment);
        m_YInputPitchLinear.SetLayout(Surface2D::Pitch);
        m_YInputPitchLinear.SetType(LWOS32_TYPE_VIDEO);
        m_YInputPitchLinear.SetLocation(Memory::Optimal);
        m_YInputPitchLinear.SetVASpace(m_VASpace);
        CHECK_RC(m_YInputPitchLinear.Alloc(GetBoundGpuDevice()));
        CHECK_RC(SurfaceUtils::FillSurface(&m_YInputPitchLinear, 0, m_YInputPitchLinear.GetBitsPerPixel(),
                                            GetBoundGpuSubdevice(), m_FillMethod));
        CHECK_RC(m_YInputPitchLinear.Map());

        m_UVInputPitchLinear.Free();
        m_UVInputPitchLinear.SetWidth(chromaSurfacePitch[streamIndex]);
        m_UVInputPitchLinear.SetPitch(AlignUp(chromaSurfacePitch[streamIndex], pitchAlign));
        m_UVInputPitchLinear.SetHeight(chromaSurfaceHeight[streamIndex]);
        m_UVInputPitchLinear.SetColorFormat(ColorUtils::Y8);
        m_UVInputPitchLinear.SetAlignment(alignment);
        m_UVInputPitchLinear.SetLayout(Surface2D::Pitch);
        m_UVInputPitchLinear.SetType(LWOS32_TYPE_VIDEO);
        m_UVInputPitchLinear.SetLocation(Memory::Optimal);
        m_UVInputPitchLinear.SetVASpace(m_VASpace);
        CHECK_RC(m_UVInputPitchLinear.Alloc(GetBoundGpuDevice()));
        CHECK_RC(SurfaceUtils::FillSurface(&m_UVInputPitchLinear, 0, m_UVInputPitchLinear.GetBitsPerPixel(),
                                            GetBoundGpuSubdevice(), m_FillMethod));        
        CHECK_RC(m_UVInputPitchLinear.Map());

        m_YInput.Free();
        m_YInput.SetWidth(AlignUp<256>(lumaSurfacePitch[streamIndex]));
        m_YInput.SetPitch(AlignUp(lumaSurfacePitch[streamIndex], pitchAlign));
        m_YInput.SetHeight(lumaSurfaceHeight[streamIndex]);
        m_YInput.SetColorFormat(ColorUtils::Y8);
        m_YInput.SetAlignment(alignment);
        m_YInput.SetLayout(Surface2D::BlockLinear);
        m_YInput.SetLogBlockHeight(1);
        m_YInput.SetType(LWOS32_TYPE_VIDEO);
        m_YInput.SetLocation(Memory::Optimal);
        m_YInput.SetVASpace(m_VASpace);
        CHECK_RC(m_YInput.Alloc(GetBoundGpuDevice()));

        CHECK_RC(SurfaceUtils::FillSurface(&m_YInput, 0, m_YInput.GetBitsPerPixel(),
                                            GetBoundGpuSubdevice(), m_FillMethod));

        for (UINT32 engNum = 0; engNum < m_EngineCount; engNum++)
        {
            CHECK_RC(m_YInput.BindGpuChannel(m_PerEngData[engNum]->m_hCh));
        }

        m_UVInput.Free();
        m_UVInput.SetWidth(AlignUp<256>(chromaSurfacePitch[streamIndex]));
        m_UVInput.SetPitch(AlignUp(chromaSurfacePitch[streamIndex], pitchAlign));
        m_UVInput.SetHeight(chromaSurfaceHeight[streamIndex]);
        m_UVInput.SetColorFormat(ColorUtils::Y8);
        m_UVInput.SetAlignment(alignment);
        m_UVInput.SetLayout(Surface2D::BlockLinear);
        m_UVInput.SetLogBlockHeight(1);
        m_UVInput.SetType(LWOS32_TYPE_VIDEO);
        m_UVInput.SetLocation(Memory::Optimal);
        m_UVInput.SetVASpace(m_VASpace);
        CHECK_RC(m_UVInput.Alloc(GetBoundGpuDevice()));
        
        CHECK_RC(SurfaceUtils::FillSurface(&m_UVInput, 0, m_UVInput.GetBitsPerPixel(),
                                            GetBoundGpuSubdevice(), m_FillMethod));

        for (UINT32 engNum = 0; engNum < m_EngineCount; engNum++)
        {
            CHECK_RC(m_UVInput.BindGpuChannel(m_PerEngData[engNum]->m_hCh));
        }
    }

    if ((1 << streamIndex) & m_StreamYUVInputFromFilesMask)
    {
        UINT32 lumaSize = lumaSurfacePitch[streamIndex] *
                          lumaSurfaceHeight[streamIndex];

        UINT32 chromaSize = chromaSurfacePitch[streamIndex] *
                            chromaSurfaceHeight[streamIndex];

        std::vector<UINT32> yBuf(lumaSize);
        std::vector<UINT32> uvBuf(chromaSize);

        string yFileName = StrPrintf("%s/FS%dF%dYInput.bin", m_YUVInputPath.c_str(), streamIndex, frameIdx);
        string uvFileName = StrPrintf("%s/FS%dF%dUVInput.bin", m_YUVInputPath.c_str(), streamIndex, frameIdx);

        FileHolder yFile(yFileName.c_str(), "rb");
        FileHolder uvFile(uvFileName.c_str(), "rb");

        if (0 == yFile.GetFile())
            return RC::CANNOT_OPEN_FILE;

        if (0 == uvFile.GetFile())
            return RC::CANNOT_OPEN_FILE;

        if (lumaSize != fread(&yBuf[0], 1, lumaSize, yFile.GetFile()))
            return RC::FILE_READ_ERROR;
        if (chromaSize != fread(&uvBuf[0], 1, chromaSize, uvFile.GetFile()))
            return RC::FILE_READ_ERROR;

        Platform::VirtualWr(m_YInputPitchLinear.GetAddress(), &yBuf[0], lumaSize);
        Platform::VirtualWr(m_UVInputPitchLinear.GetAddress(), &uvBuf[0], chromaSize);
    }
    else
    {
        GpuSubdevice* subdev = GetBoundGpuSubdevice();

        // By default creating input images synthetically.
        //   Background filled with static value the same in each frame,
        //      but changing from stream to stream for increased coverage.
        //   For now two also writing two rectangles moving from frame to frame.
        //      First rectangle is in luma section, second in chroma.
        //      Content of rectangle is random, but identical from frame to frame
        //         (for motion detection).

        CHECK_RC(SurfaceUtils::FillSurface(&m_YInputPitchLinear, (streamIndex & 1) ? 0x55AA55AA : 0xAA55AA55,
                                           m_YInputPitchLinear.GetBitsPerPixel(),
                                           subdev, m_FillMethod));
        CHECK_RC(SurfaceUtils::FillSurface(&m_UVInputPitchLinear, (streamIndex & 1) ? 0x55AA55AA : 0xAA55AA55,
                                            m_UVInputPitchLinear.GetBitsPerPixel(),
                                            subdev, m_FillMethod));

        CHECK_RC(m_YInputPitchLinear.FillPatternRect(
            9  + frameIdx, // RectX
            14 + frameIdx, // RectY
            10,            // RectWidth
            15,            // RectHeight
            1,             // ChunkWidth
            1,             // ChunkHeight
            "random",
            "seed=0",
            "",
            subdev->GetSubdeviceInst()));

        CHECK_RC(m_UVInputPitchLinear.FillPatternRect(
            49 - frameIdx, // RectX
            24 + frameIdx, // RectY
            11,            // RectWidth
            16,            // RectHeight
            1,             // ChunkWidth
            1,             // ChunkHeight
            "random",
            "seed=0",
            "",
            subdev->GetSubdeviceInst()));
    }

    CHECK_RC(m_dmaWrap.SetSurfaces(&m_YInputPitchLinear, &m_YInput));
    CHECK_RC(m_dmaWrap.Copy(
        0,
        0,
        lumaSurfacePitch[streamIndex],
        lumaSurfaceHeight[streamIndex],
        0,
        0,
        m_pTestConfig->TimeoutMs(),
        GetBoundGpuDevice()->GetDisplaySubdeviceInst()
    ));
    CHECK_RC(m_dmaWrap.SetSurfaces(&m_UVInputPitchLinear, &m_UVInput));
    CHECK_RC(m_dmaWrap.Copy(
        0,
        0,
        chromaSurfacePitch[streamIndex],
        chromaSurfaceHeight[streamIndex],
        0,
        0,
        m_pTestConfig->TimeoutMs(),
        GetBoundGpuDevice()->GetDisplaySubdeviceInst()
    ));

    return rc;
}

RC LWENCTest::Fill4KNoise(UINT32 streamIndex, UINT32 frameIdx)
{
    RC rc;

    if (0 == frameIdx)
    {
        const UINT32 pitchAlign = m_dmaWrap.GetPitchAlignRequirement();
        const UINT32 alignment = GetSurfaceAlignment();

        m_YInput.Free();
        m_YInput.SetWidth(s_4KWhiteNoiseWidth);
        m_YInput.SetPitch(AlignUp(s_4KWhiteNoiseWidth, pitchAlign));
        m_YInput.SetHeight(s_4KWhiteNoiseHeight);
        m_YInput.SetColorFormat(ColorUtils::Y8);
        m_YInput.SetAlignment(alignment);
        m_YInput.SetLayout(Surface2D::BlockLinear);
        m_YInput.SetLogBlockHeight(1);
        m_YInput.SetType(LWOS32_TYPE_VIDEO);
        m_YInput.SetLocation(Memory::Optimal);
        m_YInput.SetVASpace(m_VASpace);
        CHECK_RC(m_YInput.Alloc(GetBoundGpuDevice()));

        for (UINT32 engNum = 0; engNum < m_EngineCount; engNum++)
        {
            CHECK_RC(m_YInput.BindGpuChannel(m_PerEngData[engNum]->m_hCh));
        }

        m_UVInput.Free();
        m_UVInput.SetWidth(s_4KWhiteNoiseWidth);
        m_UVInput.SetPitch(AlignUp(s_4KWhiteNoiseWidth, pitchAlign));
        m_UVInput.SetHeight(s_4KWhiteNoiseHeight / 2);
        m_UVInput.SetColorFormat(ColorUtils::Y8);
        m_UVInput.SetAlignment(alignment);
        m_UVInput.SetLayout(Surface2D::BlockLinear);
        m_UVInput.SetLogBlockHeight(1);
        m_UVInput.SetType(LWOS32_TYPE_VIDEO);
        m_UVInput.SetLocation(Memory::Optimal);
        m_UVInput.SetVASpace(m_VASpace);
        CHECK_RC(m_UVInput.Alloc(GetBoundGpuDevice()));

        for (UINT32 engNum = 0; engNum < m_EngineCount; engNum++)
        {
            CHECK_RC(m_UVInput.BindGpuChannel(m_PerEngData[engNum]->m_hCh));
        }
    }

    auto surfNum = frameIdx % NUMELEMS(s_4KWhiteNoise);
    auto &Ysurf = get<0>(s_4KWhiteNoise[surfNum]);
    auto &UVsurf = get<1>(s_4KWhiteNoise[surfNum]);

    CHECK_RC(m_dmaWrap.SetSurfaces(&Ysurf, &m_YInput));
    CHECK_RC(m_dmaWrap.Copy(
        0,
        0,
        s_4KWhiteNoiseWidth,
        s_4KWhiteNoiseHeight,
        0,
        0,
        m_pTestConfig->TimeoutMs(),
        GetBoundGpuDevice()->GetDisplaySubdeviceInst()
    ));
    CHECK_RC(m_dmaWrap.SetSurfaces(&UVsurf, &m_UVInput));
    CHECK_RC(m_dmaWrap.Copy(
        0,
        0,
        s_4KWhiteNoiseWidth,
        s_4KWhiteNoiseHeight / 2,
        0,
        0,
        m_pTestConfig->TimeoutMs(),
        GetBoundGpuDevice()->GetDisplaySubdeviceInst()
    ));

    return rc;
}

RC LWENCTest::RunFrame
(
    UINT32 streamIndex,
    UINT32 frameIdx,
    UINT32 engNum,
    UINT32 className
)
{
    RC rc;
    GpuSubdevice* subdev = GetBoundGpuSubdevice();
    LWENCEngineData &surf = *m_PerEngData[engNum];

    if ((streamIndex == OPTICAL_FLOW || streamIndex == STEREO) && frameIdx == 0)
    {
        return OK;
    }

    Channel* &chan = m_PerEngData[engNum]->m_pCh;

    // Clear the status info that we read after the frame
    memset(surf.m_EncodeStatus.GetAddress(), 0, sizeof(lwenc_pic_stat_s));

    //Fill TaskStatus with 1 because we are checking if a member of this structure is 0
    //after encoding has been completed.
    if (streamIndex == OPTICAL_FLOW || streamIndex == STEREO)
    {
        CHECK_RC(SurfaceUtils::FillSurface(&surf.m_TaskStatus, 0, 
            surf.m_TaskStatus.GetBitsPerPixel(), subdev, m_FillMethod));
        CHECK_RC(chan->Write(
            s_ENC,
            LWC5B7_SET_APPLICATION_ID,
            DRF_NUM(C5B7, _SET_APPLICATION_ID, _ID, s_ApplicationId[streamIndex])));
        CHECK_RC(chan->WriteWithSurface(s_ENC, LWC5B7_SET_OUT_TASK_STATUS,
                                        surf.m_TaskStatus, 0, 8));
    }   
    else
    {
        CHECK_RC(chan->Write(s_ENC, LWC1B7_SET_APPLICATION_ID,
            DRF_NUM(C1B7, _SET_APPLICATION_ID, _ID, s_ApplicationId[streamIndex])));
    }

    UINT32 controlParams = 0;
    controlParams |= DRF_DEF(C1B7, _SET_CONTROL_PARAMS, _CODEC_TYPE, _H264) |
        DRF_NUM(C1B7, _SET_CONTROL_PARAMS, _FORCE_OUT_PIC, 1) |
        DRF_NUM(C1B7, _SET_CONTROL_PARAMS, _FORCE_OUT_COL, 0);
    UINT32 RcMode = 0;
    switch (streamIndex)
    {
        case H264_BASELINE_CAVLC:
        case H264_MVC_HP_CABAC:
        case H264_HD_HP_CABAC_MS:
        case H265_GENERIC:
        case H265_10BIT_444:
        case H265_4K_40F:
        case VP9_GENERIC:
        case VP8_GENERIC:
        case H264_HP_CABAC:
        case H265_B_FRAMES_4K:
            break;
        case H265_MULTIREF_BFRAME:
            controlParams |= DRF_NUM(C1B7, _SET_CONTROL_PARAMS, _FORCE_OUT_COL, 1) |
                DRF_NUM(C1B7, _SET_CONTROL_PARAMS, _SLICE_STAT_ON, 1) |
                DRF_NUM(C1B7, _SET_CONTROL_PARAMS, _MPEC_STAT_ON,  1);
            break;
        case H264_B_FRAMES:
            controlParams |= DRF_NUM(C1B7, _SET_CONTROL_PARAMS, _FORCE_OUT_COL, 1);
            break;
        case OPTICAL_FLOW:
        case STEREO:
            controlParams = DRF_NUM(C1B7, _SET_CONTROL_PARAMS, _MEONLY, 1);
            break;
        default:
            return RC::SOFTWARE_ERROR;
    }

    controlParams |= DRF_NUM(C1B7, _SET_CONTROL_PARAMS, _RCMODE, RcMode);

    CHECK_RC(chan->Write(s_ENC, LWC1B7_SET_CONTROL_PARAMS, controlParams));

    CHECK_RC(chan->Write(s_ENC, LWC1B7_SET_PICTURE_INDEX,
        DRF_NUM(C1B7, _SET_PICTURE_INDEX, _INDEX, frameIdx)));

    CHECK_RC(chan->WriteWithSurface(s_ENC, LWC1B7_SET_IN_LWR_PIC, m_YInput,
                                    0, 8, m_MappingType));

    CHECK_RC(chan->WriteWithSurface(s_ENC, LWC1B7_SET_IN_LWR_PIC_CHROMA_U, m_UVInput,
                                    0, 8, m_MappingType));

    CHECK_RC(chan->WriteWithSurface(s_ENC, LWC1B7_SET_OUT_ENC_STATUS,
        surf.m_EncodeStatus, 0, 8, m_MappingType));

    CHECK_RC(chan->WriteWithSurface(s_ENC, LWC1B7_SET_IO_RC_PROCESS,
        surf.m_IoRcProcess, 0, 8, m_MappingType));

    INT32 outSurfaceIdx = streamDescriptions[streamIndex].outPictureIndex[frameIdx];
    CHECK_RC(surf.AddOutSurfToPushBuffer(outSurfaceIdx));

    CHECK_RC(surf.AddMoCompSurfToPushBuffer(
                    s_MoCompIndexes[streamIndex][frameIdx]));

    CHECK_RC(chan->WriteWithSurface(s_ENC, LWC1B7_SET_IOHISTORY,
        surf.m_IoHistory, 0, 8));

    CHECK_RC(SurfaceUtils::FillSurface(&surf.m_OutBitstream[streamIndex], 0x55555555, 
            surf.m_OutBitstream[streamIndex].GetBitsPerPixel(), subdev, m_FillMethod));    
    CHECK_RC(chan->WriteWithSurface(s_ENC, LWC1B7_SET_OUT_BITSTREAM,
            surf.m_OutBitstream[streamIndex], 0, 8, m_MappingType));       

    const INT32 inColocIdx = colocInOutIndexes[streamIndex][frameIdx][0];

    if (inColocIdx >= 0)
    {
        CHECK_RC(chan->WriteWithSurface(s_ENC, LWC1B7_SET_IN_COLOC_DATA,
            surf.m_ColocData[inColocIdx], 0, 8, m_MappingType));
    }

    const INT32 outColocIdx = colocInOutIndexes[streamIndex][frameIdx][1];
    if (outColocIdx >= 0)
    {
        CHECK_RC(chan->WriteWithSurface(s_ENC, LWC1B7_SET_OUT_COLOC_DATA,
            surf.m_ColocData[outColocIdx], 0, 8, m_MappingType));
    }

    const INT32 inMeIdx = meInOutIndexes[streamIndex][frameIdx][0];

    if (inMeIdx >= 0)
    {
        CHECK_RC(chan->WriteWithSurface(s_ENC, LWC1B7_SET_IN_MEPRED_DATA,
            surf.m_MepredData[inMeIdx], 0, 8, m_MappingType));
    }

    // do nothing if not used
    const INT32 outMeIdx = meInOutIndexes[streamIndex][frameIdx][1];
    if (outMeIdx >= 0)
    {
        CHECK_RC(chan->WriteWithSurface(s_ENC, LWC1B7_SET_OUT_MEPRED_DATA,
           surf.m_MepredData[outMeIdx], 0, 8, m_MappingType));
    }

    // do nothing if not used
    const INT32 inTempIdx = s_TemporalLwrRefIndexes[streamIndex][frameIdx][0];
    if (inTempIdx >= 0)
    {
        CHECK_RC(chan->WriteWithSurface(s_ENC,
            LWC1B7_SET_IN_LWRRENT_TEMPORAL_BUF,
            surf.m_TemporalBuf[inTempIdx], 0, 8, m_MappingType));
    }

    // do nothing if not used
    const INT32 outTempIdx = s_TemporalLwrRefIndexes[streamIndex][frameIdx][1];
    if (outTempIdx >= 0)
    {
        CHECK_RC(chan->WriteWithSurface(s_ENC, LWC1B7_SET_IN_REF_TEMPORAL_BUF,
           surf.m_TemporalBuf[outTempIdx], 0, 8, m_MappingType));
    }

    // do nothing if not used
    switch (s_ApplicationId[streamIndex])
    {
        case LWC1B7_SET_APPLICATION_ID_ID_LWENC_VP9:
            CHECK_RC(surf.AddLastRefPicToPushBuffer(
                            refPictureIndexes[streamIndex][frameIdx][0]));
            CHECK_RC(chan->WriteWithSurface(s_ENC, LWC1B7_SET_OUT_COUNTER_BUF,
                                            surf.m_CounterBuf, 0, 8));
            CHECK_RC(chan->WriteWithSurface(s_ENC, LWC1B7_SET_IN_PROB_BUF,
                                            surf.m_ProbBuf, 0, 8));
            CHECK_RC(chan->WriteWithSurface(s_ENC, LWC1B7_SET_IN_COMBINED_LINE_BUF,
                                            surf.m_CombinedLineBuf, 0, 8));
            CHECK_RC(chan->WriteWithSurface(s_ENC, LWC1B7_SET_IN_FILTER_LINE_BUF,
                                            surf.m_FilterLineBuf, 0, 8));
            CHECK_RC(chan->WriteWithSurface(s_ENC, LWC1B7_SET_IN_FILTER_COL_LINE_BUF,
                                            surf.m_FilterColLineBuf, 0, 8));
            break;
        case LWC1B7_SET_APPLICATION_ID_ID_LWENC_VP8:
            for (UINT32 refPicIdx = 0; refPicIdx < MAX_REF_FRAMES; refPicIdx++)
            {
                const INT32 inSurfaceIdx =
                    refPictureIndexes[streamIndex][frameIdx][refPicIdx];
                if (inSurfaceIdx == -1) continue;
                switch (refPicIdx)
                {
                    case 0:
                        CHECK_RC(chan->WriteWithSurface(0, LWD0B7_SET_IN_REF_PIC_LAST,
                            surf.m_YUVRefPic[inSurfaceIdx], 0, 8, m_MappingType));
                        break;
                    case 1:
                        CHECK_RC(chan->WriteWithSurface(0, LWD0B7_SET_IN_REF_PIC_GOLDEN,
                            surf.m_YUVRefPic[inSurfaceIdx], 0, 8, m_MappingType));
                        break;
                    case 2:
                        CHECK_RC(chan->WriteWithSurface(0, LWD0B7_SET_IN_REF_PIC_ALTREF,
                            surf.m_YUVRefPic[inSurfaceIdx], 0, 8, m_MappingType));
                        break;
                    default:
                        break;
                }
            }
            CHECK_RC(chan->WriteWithSurface(s_ENC, LWD0B7_SET_IO_VP8_ENC_STATUS,
                surf.m_VP8EncStatIo, 0, 8));
            CHECK_RC(chan->WriteWithSurface(s_ENC, LWD0B7_SET_UCODE_STATE,
                surf.m_UcodeState, 0, 8));
            CHECK_RC(chan->WriteWithSurface(s_ENC, LWD0B7_SET_OUT_BITSTREAM_RES,
                surf.m_OutBitstreamRes[streamIndex], 0, 8));
            break;
        case LWC5B7_SET_APPLICATION_ID_ID_LWENC_OFS:
            CHECK_RC(chan->WriteWithSurface(s_ENC, LWC5B7_SET_IN_CEAHINTS_DATA,
                surf.m_CEAHintsData, 0, 8));
            //intentional fall-through; no break needed
        default:
            for (UINT32 refPicIdx = 0; refPicIdx < MAX_REF_FRAMES; refPicIdx++)
            {
                const INT32 inSurfaceIdx =
                    refPictureIndexes[streamIndex][frameIdx][refPicIdx];
                CHECK_RC(surf.AddRefPicToPushBuffer(inSurfaceIdx, refPicIdx));
            }
            break;
    }

    if (className == LWC1B7_VIDEO_ENCODER && !GetBoundGpuSubdevice()->IsSOC())
    {
        CHECK_RC(chan->WriteWithSurface(s_ENC, LWC1B7_SET_IN_PARTITION_BUF, surf.m_PartitionBuf, 0, 8));
    }
    else
    {
        if (className == LWC4B7_VIDEO_ENCODER)
        {
            CHECK_RC(chan->WriteWithSurface(s_ENC, LWC4B7_SET_IN_PARTITION_BUF, surf.m_PartitionBuf, 0, 8));
        }
        else
        {
            // On T186 SOC, the C1B7 class uses the C2B7 method number (!?!)
            CHECK_RC(chan->WriteWithSurface(s_ENC, LWC2B7_SET_IN_PARTITION_BUF, surf.m_PartitionBuf, 0, 8));
        }
    }

    CHECK_RC(chan->WriteWithSurface(s_ENC, LWC1B7_SET_IN_DRV_PIC_SETUP, surf.m_PicSetup, frameIdx*allControlStructsSizes[streamIndex], 8));

    CHECK_RC(StartEngine(streamIndex, frameIdx, engNum));

    m_AtleastOneFrameTested = true;
    return rc;
}

RC LWENCTest::StartEngine(UINT32 streamIdx, UINT32 frameIdx, UINT32 engNum)
{
    RC rc;
    UINT32 exelwteCmd;
    Channel *pChannel = m_PerEngData[engNum]->m_pCh;
    if (!GetBoundGpuSubdevice()->IsSOC())
    {
        const UINT32 payLoadValue = 0x5670000 + streamIdx * 0x100 + frameIdx;
        CHECK_RC(pChannel->WriteWithSurface(s_ENC, LWC0B7_SEMAPHORE_A,
            m_PerEngData[engNum]->m_Semaphore, 0, 32));
        CHECK_RC(pChannel->WriteWithSurface(s_ENC, LWC0B7_SEMAPHORE_B,
            m_PerEngData[engNum]->m_Semaphore, 0, 0));
        CHECK_RC(pChannel->Write(s_ENC, LWC0B7_SEMAPHORE_C, payLoadValue));

        exelwteCmd = DRF_DEF(C0B7, _EXELWTE, _NOTIFY, _ENABLE) |
                     DRF_DEF(C0B7, _EXELWTE, _NOTIFY_ON, _END) |
                     DRF_DEF(C0B7, _EXELWTE, _AWAKEN, _DISABLE);
    }
    else
    {
        exelwteCmd = DRF_DEF(C0B7, _EXELWTE, _NOTIFY, _DISABLE) |
                     DRF_DEF(C0B7, _EXELWTE, _NOTIFY_ON, _END) |
                     DRF_DEF(C0B7, _EXELWTE, _AWAKEN, _ENABLE);
    }
    CHECK_RC(pChannel->Write(s_ENC, LWC0B7_EXELWTE, exelwteCmd));

    return rc;
}

RC LWENCTest::WaitOnEngine(UINT32 streamIndex, UINT32 frameIdx, UINT32 engNum)
{
    RC rc;

    if (!GetBoundGpuSubdevice()->IsSOC())
    {
        UINT32 *semaphore = (UINT32 *)m_PerEngData[engNum]->m_Semaphore.GetAddress();
        const UINT32 payLoadValue = 0x5670000 + streamIndex * 0x100 + frameIdx;

        PollSemaphore_Args completionArgs =
        {
            semaphore,
            payLoadValue
        };

        CHECK_RC(POLLWRAP_HW(GpuPollSemaVal, &completionArgs,
            m_pTestConfig->TimeoutMs()));
    }
    else
    {
        CHECK_RC(m_PerEngData[engNum]->m_pCh->WaitIdle(m_pTestConfig->TimeoutMs()));
    }
    if (streamIndex == VP8_GENERIC)
    {
        lwenc_vp8_stat_data_s stat;
        Platform::VirtualRd(
            (UINT08*)m_PerEngData[engNum]->m_EncodeStatus.GetAddress(),
            &stat, sizeof(stat));
        if (stat.total_bit_count == 0)
        {
            Printf(Tee::PriError, "VP8 Encode failed: output buffer is empty\n");
            return RC::DATA_MISMATCH;
        }
    }
    else if (streamIndex == VP9_GENERIC)
    {
        lwenc_vp9_pic_stat_s stat;
        Platform::VirtualRd(
            (UINT08*)m_PerEngData[engNum]->m_EncodeStatus.GetAddress(),
            &stat, sizeof(stat));
        if (m_PerEngData[engNum]->m_OutBitstream[streamIndex].GetSize()< stat.total_byte_count)
        {
            Printf(Tee::PriWarn, "Output bit stream size=%llu is less than the total_byte_count=%u \n",
                m_PerEngData[engNum]->m_OutBitstream[streamIndex].GetSize(), stat.total_byte_count);
        }        
        if (stat.total_byte_count == 0)
        {
            Printf(Tee::PriError, "VP9 Encode failed: output buffer is empty\n");
            return RC::DATA_MISMATCH;
        }
    }
    else if (streamIndex == OPTICAL_FLOW || streamIndex == STEREO)
    {
        // New structure added to Optical Flow to verify if the encoding has been completed
        LwEnc_Status stat;
        Platform::VirtualRd(
            (UINT08*)m_PerEngData[engNum]->m_TaskStatus.GetAddress(),
            &stat, sizeof(stat));
        if (stat.status_task != 0)
        {
            Printf(Tee::PriError, "OPTICAL FLOW Encode failed: Error Code = %d\n",
                   stat.status_task);
            return RC::SOFTWARE_ERROR;
        }
    }
    // Verify that a nonzero number of bytes were encoded
    else
    {
        lwenc_pic_stat_s stat;
        Platform::VirtualRd(
                (UINT08*)m_PerEngData[engNum]->m_EncodeStatus.GetAddress() +
                offsetof(lwenc_stat_data_s, pic_stat),
                &stat, sizeof(stat));
        if (m_PerEngData[engNum]->m_OutBitstream[streamIndex].GetSize() * 8 < stat.total_bit_count)
        {
            Printf(Tee::PriWarn, "Output bit stream size=%llu is less than the total_bit_count=%u \n",
                m_PerEngData[engNum]->m_OutBitstream[streamIndex].GetSize() * 8, stat.total_bit_count);
        }
        if (stat.total_bit_count == 0)
        {
            Printf(Tee::PriError, "Encode failed: output buffer is empty\n");
            return RC::DATA_MISMATCH;
        }
    }

    return rc;
}

RC LWENCTest::SaveStreams(UINT32 streamIndex, UINT32 frameIdx, UINT32 engNum)
{
    RC rc;

    lwenc_pic_stat_s stat;
    Platform::VirtualRd((UINT08*)m_PerEngData[engNum]->m_EncodeStatus.GetAddress() +
        offsetof(lwenc_stat_data_s, pic_stat), &stat, sizeof(stat));
    Printf(Tee::PriLow, "LWENC: total_bit_count/8=%d, error_status=%d\n",
        stat.total_bit_count/8,
        stat.error_status);

    if (!m_SaveStreams)
        return OK;

    Surface2D::MappingSaver oldMapping(m_PerEngData[engNum]->m_OutBitstream[streamIndex]);
    if
    (
        m_PerEngData[engNum]->m_OutBitstream[streamIndex].IsMapped() ||
        OK == m_PerEngData[engNum]->m_OutBitstream[streamIndex].Map()
    )
    {
        if (spsNalus[streamIndex].IsValid() && ppsNalus[streamIndex].IsValid())
        {
            string fileName = StrPrintf("S%02ueng%02u.264", streamIndex, engNum);
            FileHolder file;
            size_t written;

            if (0 == frameIdx)
            {
                CHECK_RC(file.Open(fileName.c_str(), "wb"));
                written = fwrite(
                    spsNalus[streamIndex].GetEbspFromStartCode(),
                    spsNalus[streamIndex].GetEbspSizeWithStart(),
                    1,
                    file.GetFile());
                if (1 != written)
                {
                    Printf(Tee::PriError,
                            "Failed to write SPS for %s\n", fileName.c_str());
                    return RC::FILE_WRITE_ERROR;
                }

                written = fwrite(
                    ppsNalus[streamIndex].GetEbspFromStartCode(),
                    ppsNalus[streamIndex].GetEbspSizeWithStart(),
                    1,
                    file.GetFile());
                if (1 != written)
                {
                    Printf(Tee::PriError,
                            "Failed to write PPS for %s\n", fileName.c_str());
                    return RC::FILE_WRITE_ERROR;
                }

                if (H264_MVC_HP_CABAC == streamIndex)
                {
                    written = fwrite(
                        subsetSpsNalu.GetEbspFromStartCode(),
                        subsetSpsNalu.GetEbspSizeWithStart(),
                        1,
                        file.GetFile());
                    if (1 != written)
                    {
                        Printf(Tee::PriError,
                                "Failed to write subset SPS for %s\n", fileName.c_str());
                        return RC::FILE_WRITE_ERROR;
                    }
                    written = fwrite(
                        subsetPpsNalu.GetEbspFromStartCode(),
                        subsetPpsNalu.GetEbspSizeWithStart(),
                        1,
                        file.GetFile());
                    if (1 != written)
                    {
                        Printf(Tee::PriError,
                                "Failed to write subset PPS for %s\n", fileName.c_str());
                        return RC::FILE_WRITE_ERROR;
                    }
                }
            }
            else
            {
                CHECK_RC(file.Open(fileName.c_str(), "ab"));
            }
            size_t nalusSize = stat.total_bit_count / 8;
            vector<UINT08> buf(nalusSize);
            Platform::VirtualRd(
                static_cast<UINT08*>(m_PerEngData[engNum]->m_OutBitstream[streamIndex].GetAddress()),
                &buf[0],
                static_cast<UINT32>(buf.size())
            );
            written = fwrite(&buf[0], buf.size(), 1, file.GetFile());
            if (1 != written)
            {
                Printf(Tee::PriError,
                        "Failed to write subset NALU for %s\n", fileName.c_str());
                return RC::FILE_WRITE_ERROR;
            }
        }
        else if
        (
            h265VpsNalus[streamIndex].IsValid() &&
            h265SpsNalus[streamIndex].IsValid() &&
            h265PpsNalus[streamIndex].IsValid()
        )
        {
            string fileName = StrPrintf("S%02ueng%02u.265", streamIndex, engNum);
            FileHolder file;
            size_t written;

            if (0 == frameIdx)
            {
                CHECK_RC(file.Open(fileName.c_str(), "wb"));
                written = fwrite(
                    h265VpsNalus[streamIndex].GetEbspFromStartCode(),
                    h265VpsNalus[streamIndex].GetEbspSizeWithStart(),
                    1,
                    file.GetFile());
                if (1 != written)
                {
                    Printf(Tee::PriError,
                           "Failed to write VPS for %s\n", fileName.c_str());
                    return RC::FILE_WRITE_ERROR;
                }
                written = fwrite(
                    h265SpsNalus[streamIndex].GetEbspFromStartCode(),
                    h265SpsNalus[streamIndex].GetEbspSizeWithStart(),
                    1,
                    file.GetFile());
                if (1 != written)
                {
                    Printf(Tee::PriError,
                           "Failed to write SPS for %s\n", fileName.c_str());
                    return RC::FILE_WRITE_ERROR;
                }
                written = fwrite(
                    h265PpsNalus[streamIndex].GetEbspFromStartCode(),
                    h265PpsNalus[streamIndex].GetEbspSizeWithStart(),
                    1,
                    file.GetFile());
                if (1 != written)
                {
                    Printf(Tee::PriError,
                           "Failed to write PPS for %s\n", fileName.c_str());
                    return RC::FILE_WRITE_ERROR;
                }
            }
            else
            {
                CHECK_RC(file.Open(fileName.c_str(), "ab"));
            }
            size_t nalusSize = stat.total_bit_count / 8;
            vector<UINT08> buf(nalusSize);
            Platform::VirtualRd(
                static_cast<UINT08*>(m_PerEngData[engNum]->m_OutBitstream[streamIndex].GetAddress()),
                &buf[0],
                static_cast<UINT32>(buf.size())
            );
            written = fwrite(&buf[0], buf.size(), 1, file.GetFile());
            if (1 != written)
            {
                Printf(Tee::PriError,
                       "Failed to write NALUs of frame %u for %s\n", frameIdx, fileName.c_str());
                return RC::FILE_WRITE_ERROR;
            }
        }
    }

    return rc;
}

RC LWENCEngineData::AllocateOutputStreamBuffer (
    UINT32 streamIndex,
    UINT32 alignment,
    Memory::Location processingLocation,
    GpuDevice * gpuDevice)
{
    RC rc;
    m_OutBitstream[streamIndex].Free();
    // outBitstreamBufferSize is in bits
    m_OutBitstream[streamIndex].SetWidth(256);
    m_OutBitstream[streamIndex].SetHeight(outBitstreamBufferSize[streamIndex] / 256 / 8);
    m_OutBitstream[streamIndex].SetColorFormat(ColorUtils::Y8);
    m_OutBitstream[streamIndex].SetAlignment(alignment);
    m_OutBitstream[streamIndex].SetLocation(processingLocation);
    m_OutBitstream[streamIndex].SetVASpace(m_VASpace);

    CHECK_RC(m_OutBitstream[streamIndex].Alloc(gpuDevice));
    CHECK_RC(m_OutBitstream[streamIndex].Map());
    CHECK_RC(m_OutBitstream[streamIndex].BindGpuChannel(m_hCh));

    m_OutBitstreamRes[streamIndex].Free();
    // source from test_00000.hdr
    m_OutBitstreamRes[streamIndex].SetWidth(AlignUp<256>(outBitstreamBufferSize[streamIndex] / 8 / 4));
    m_OutBitstreamRes[streamIndex].SetHeight(1);
    m_OutBitstreamRes[streamIndex].SetColorFormat(ColorUtils::A8R8G8B8);    // assumption
    m_OutBitstreamRes[streamIndex].SetAlignment(alignment);
    m_OutBitstreamRes[streamIndex].SetLocation(processingLocation);
    m_OutBitstreamRes[streamIndex].SetVASpace(m_VASpace);
    CHECK_RC(m_OutBitstreamRes[streamIndex].Alloc(gpuDevice));
    CHECK_RC(m_OutBitstreamRes[streamIndex].BindGpuChannel(m_hCh));

    return rc;
}

RC LWENCTest::Run()
{
    const bool isBackground = m_KeepRunning;

    const auto keepRunning = [&]() -> bool
    {
        return ! isBackground || m_KeepRunning || ! m_AtleastOneFrameTested;
    };

    GpuSubdevice *pGpuSubdevice = GetBoundGpuSubdevice();

    RC rc;

    UINT32 startingLoopIdx = 0;
    UINT32 className = m_LWENCAlloc.GetSupportedClass(GetBoundGpuDevice());

    m_AtleastOneFrameTested = false;

    const auto advanceStream = [&](UINT32* pStreamIdx)
    {
        ++*pStreamIdx;
        if (*pStreamIdx >= NUM_STREAMS && isBackground && keepRunning() && m_AtleastOneFrameTested)
        {
            *pStreamIdx = 0;
            startingLoopIdx = 0;
        }
    };

    for (UINT32 streamIdx = 0;
         (streamIdx < NUM_STREAMS) && keepRunning();
         advanceStream(&streamIdx))
    {
        m_LwrStreamIdx = streamIdx;
        UINT32 loop = startingLoopIdx;
        startingLoopIdx += streamDescriptions[streamIdx].numFrames;
        UINT32 defaultSkipMask = 0;

        SetDefaultStreamSkipMask(pGpuSubdevice, className, &defaultSkipMask);

        if ((1 << streamIdx) & m_StreamSkipMask)
        {
            // Avoid 'Skipping stream' prints when required hardware for
            // stream is not available
            if (!((1 << streamIdx) & defaultSkipMask))
            {
                Printf(Tee::PriNormal, "Skipping stream %s (mask %u).\n",
                    streamDescriptions[streamIdx].name, 1U << streamIdx);
            }
            continue;
        }

        VerbosePrintf("Starting stream %s (mask %u):\n",
            streamDescriptions[streamIdx].name, 1U << streamIdx);

        {
            Tasker::DetachThread detach;
            for (UINT32 engNum = 0; engNum < m_EngineCount; engNum++)
            {
                CHECK_RC(m_PerEngData[engNum]->ClearSurfaces(pGpuSubdevice, m_FillMethod));
                switch (streamIdx)
                {
                    case H264_BASELINE_CAVLC:
                        CHECK_RC(PrepareControlStructuresH264BaselineCAVLC(
                            &(m_PerEngData[engNum]->m_PicSetup),
                            className,
                            outBitstreamBufferSize[streamIdx]));
                        break;
                    case H264_HP_CABAC:
                          CHECK_RC(PrepareControlStructuresH264HighprofileCABAC(
                            &(m_PerEngData[engNum]->m_PicSetup),
                            &(m_PerEngData[engNum]->m_IoRcProcess),
                            className,
                            outBitstreamBufferSize[streamIdx]));
                        break;
                    case H264_MVC_HP_CABAC:
                        CHECK_RC(PrepareControlStructuresH264MVCHighprofileCABAC(
                            &(m_PerEngData[engNum]->m_PicSetup),
                            className,
                            outBitstreamBufferSize[streamIdx]));
                        break;
                    case H264_HD_HP_CABAC_MS:
                        CHECK_RC(PrepareControlStructuresH264HDHighprofileCABAC(
                            &(m_PerEngData[engNum]->m_PicSetup),
                            &(m_PerEngData[engNum]->m_IoRcProcess),
                            className,
                            outBitstreamBufferSize[streamIdx]));
                        break;
                    case H264_B_FRAMES:
                        CHECK_RC(PrepareControlStructuresH264BFrames(
                            &(m_PerEngData[engNum]->m_PicSetup),
                            className,
                            outBitstreamBufferSize[streamIdx]));
                        break;
                    case H265_GENERIC:
                        CHECK_RC(PrepareControlStructuresH265Generic(
                            &(m_PerEngData[engNum]->m_PicSetup),
                            className,
                            outBitstreamBufferSize[streamIdx]));
                        break;
                    case H265_10BIT_444:
                        CHECK_RC(PrepareControlStructuresH265TenBit444(
                            &(m_PerEngData[engNum]->m_PicSetup),
                            className));
                        break;
                    case VP9_GENERIC:
                        CHECK_RC(PrepareControlStructuresVP9Generic(
                            &(m_PerEngData[engNum]->m_PicSetup),
                            className));
                        break;
                    /// added vp8
                    case VP8_GENERIC:
                        CHECK_RC(PrepareControlStructuresVP8Generic(
                            &(m_PerEngData[engNum]->m_PicSetup),
                            className));
                        break;
                    case H265_4K_40F:
                        CHECK_RC(PrepareControlStructuresH265FourK40F(
                            &(m_PerEngData[engNum]->m_PicSetup),
                            className));
                        break;
                    case H265_MULTIREF_BFRAME:
                        CHECK_RC(PrepareControlStructuresH265MultiRefBFrame(
                            &(m_PerEngData[engNum]->m_PicSetup),
                            className,
                            outBitstreamBufferSize[streamIdx]));
                        break;
                    case OPTICAL_FLOW:
                        CHECK_RC(PrepareControlStructuresOpticalFlow(
                            &(m_PerEngData[engNum]->m_PicSetup),
                            className,
                            streamIdx,
                            streamDescriptions[streamIdx].numFrames,
                            outBitstreamBufferSize[streamIdx]));
                        break;
                    case STEREO:
                        CHECK_RC(PrepareControlStructuresStereo(
                            &(m_PerEngData[engNum]->m_PicSetup),
                            className,
                            streamIdx,
                            streamDescriptions[streamIdx].numFrames,
                            outBitstreamBufferSize[streamIdx]));
                        break;
                    case H265_B_FRAMES_4K:
                        CHECK_RC(PrepareControlStructuresH265Frames4K(
                            &(m_PerEngData[engNum]->m_PicSetup),
                            className,
                            outBitstreamBufferSize[streamIdx]));
                        break;                        
                    default:
                        return RC::SOFTWARE_ERROR;
                }
            }
        }

        UINT32 numFrames;
        numFrames = min(m_MaxFrames, streamDescriptions[streamIdx].numFrames);

        VerbosePrintf("Number of frames = %d\n", numFrames);

        for (UINT32 frameIdx = 0;
             (frameIdx < numFrames) && keepRunning();
             frameIdx++)
        {
            m_LwrFrameIdx = frameIdx;

            {
                Tasker::DetachThread detach;

                for (UINT32 engNum = 0; engNum < m_EngineCount; engNum++)
                {
                    //In the case of Optical flow, for every frame the previous original frame
                    //needs to be provided as reference
                    //TODO: Move this outside LWENCTest class
                    if ((streamIdx == OPTICAL_FLOW || streamIdx == STEREO) && frameIdx > 0)
                    {
                        LWENCEngineData &surf = *m_PerEngData[engNum];
                        CHECK_RC(surf.CopyInputPicToRefPic(
                            &m_YInput,
                            &m_UVInput,
                            0,
                            GetBoundGpuDevice()->GetDisplaySubdeviceInst(),
                            &m_TestConfig,
                            &m_dmaWrap));
                    }
                }

                CHECK_RC(FillYUVInput(streamIdx, frameIdx));

                for (UINT32 engNum = 0; engNum < m_EngineCount; engNum++)
                {
                    CHECK_RC(RunFrame(streamIdx, frameIdx, engNum, className));
                }

                if ((streamIdx == OPTICAL_FLOW || streamIdx == STEREO) && frameIdx == 0)
                {
                    continue;
                }

                for (UINT32 engNum = 0; engNum < m_EngineCount; engNum++)
                {
                    CHECK_RC((m_PerEngData[engNum]->m_pCh)->Flush());
                }
            }

            for (UINT32 engNum = 0; engNum < m_EngineCount; engNum++)
            {
                CHECK_RC(WaitOnEngine(streamIdx, frameIdx, engNum));
            }
            for (UINT32 engNum = 0; engNum < m_EngineCount; engNum++)
            {
                m_pGGSurfs->ReplaceSurface(0, &(m_PerEngData[engNum]->m_OutBitstream[streamIdx]));
                m_pGolden->SetLoop(loop + frameIdx);
                CHECK_RC(SaveStreams(streamIdx, frameIdx, engNum));
                CHECK_RC(m_pGolden->Run());
                SaveSurfaces(streamIdx, frameIdx, engNum);
            }
            CHECK_RC(rc);
        }
    }

    CHECK_RC(m_pGolden->ErrorRateTest(GetJSObject()));

    if (!m_AtleastOneFrameTested)
    {
        return RC::NO_TESTS_RUN;
    }

    return rc;
}

void LWENCTest::PrintCallback(void* test)
{
    static_cast<LWENCTest*>(test)->Print(Tee::PriNormal);
}

void LWENCTest::Print(INT32 pri)
{
    Printf(pri, "Golden error in stream %s at frame %d.\n",
           streamDescriptions[m_LwrStreamIdx].name, m_LwrFrameIdx);
}

JS_CLASS_INHERIT(LWENCTest, GpuTest,
                 "LWENCTest test.");

CLASS_PROP_READWRITE(LWENCTest, KeepRunning, bool,
                     "The test will keep running as long as this flag is set");

CLASS_PROP_READWRITE(LWENCTest, SaveSurfaces, bool,
                     "Save complete content of all surface for each frame");

CLASS_PROP_READWRITE(LWENCTest, SaveStreams, bool,
                     "Save resulting H.264 streams in Annex B/H.264 format");

CLASS_PROP_READWRITE(LWENCTest, AllocateInSysmem, bool,
                     "Allocate all memory as coherent");

CLASS_PROP_READWRITE(LWENCTest, MaxFrames, UINT32,
    "Limits the number of rendered frames (default = all frames).");

CLASS_PROP_READWRITE(LWENCTest, StreamSkipMask, UINT32,
    "Bitmask of streams to skip. Use value 1 to skip first, 2 to skip second, "
    "3 to skip first two stream, etc. (default = 0).");

CLASS_PROP_READWRITE(LWENCTest, EngineSkipMask, UINT32,
    "Bits representing the engines which will be skipped in LWENCTest."
    "The high bits in the input represent the engines on which the test will be"
    "skipped. (default = 0)");

CLASS_PROP_READWRITE(LWENCTest, StreamYUVInputFromFilesMask, UINT32,
    "Bitmask of streams to enable reading YUV input from files "
    "in path specified by YUVInputPath. "
    "Use value 1 to enable first, 2 to enable second, "
    "3 to enable first two streams, etc. (default = 0)");

CLASS_PROP_READWRITE(LWENCTest, YUVInputPath, string,
    "Specify the path to the YUVInput binaries."
    "Default to current directory (runspace)");
