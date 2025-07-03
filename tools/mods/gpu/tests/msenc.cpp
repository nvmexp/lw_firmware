/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2016,2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// MSENC test

#include "core/include/channel.h"
#include "gpu/include/gpudev.h"
#include "gputest.h"
#include "gpu/include/gralloc.h"
#include "lwos.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "core/include/fileholder.h"
#include "gpu/utility/surf2d.h"
#include "class/cl90b7.h" // LW90B7_VIDEO_ENCODER

// See http://lwbugs/812061 or Hemant Malhotra for background of creating this test

using Utility::AlignUp;
using Utility::AlignDown;

const UINT32 MAX_NUM_FRAMES = 8;
const UINT32 MAX_REF_FRAMES = 5;

struct StreamDescription
{
    const char *Name;
    UINT32 NumFrames;
    UINT32 OutPictureIndex[MAX_NUM_FRAMES]; // index to m_YUVRefPic array
};

enum StreamIndexes
{
    H264_BASELINE_CAVLC = 0,
    H264_HP_CABAC = 1,
    H264_MVC_HP_CABAC = 2,
    H264_HD_HP_CABAC_MS = 3,
    NUM_STREAMS
};

// Streams (short movies) picked up by the video team to exercise
//  various compression schemes and provide sufficient engine coverage.
static const StreamDescription StreamDescriptions[NUM_STREAMS] =
{
    // Name                                      NumF  OutPicIdx
    { "H264 Baseline CAVLC",                        4, {0, 1, 0, 1} },
    { "H264 High profile CABAC",                    8, {0, 0, 1, 1, 2, 2, 2, 2} },
    { "H264 MVC High profile CABAC",                8, {0, 1, 2, 3, 4, 4, 4, 4} },
    { "H264 HD High profile CABAC multiple slices", 4, {0, 1, 2, 2} },
};

// Indexes to reference picture surfaces used during current frame:
//  "-1" means reference picture not used,
//  value >= 0 is an index to m_YUVRefPic array
static const INT32 RefPictureIndexes[NUM_STREAMS][MAX_NUM_FRAMES][MAX_REF_FRAMES] =
{
    // H264_BASELINE_CAVLC:
    {
        { -1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1},
        { -1,  1, -1, -1, -1},
        { -1, -1,  0, -1, -1}
    },
    // H264_HP_CABAC:
    {
        { -1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1},
        {  0,  1, -1, -1, -1},
        {  0,  1, -1, -1, -1},
        {  0,  1, -1, -1, -1},
        {  0,  1, -1, -1, -1},
        {  0,  1, -1, -1, -1}
    },
    // H264_MVC_HP_CABAC:
    {
        { -1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1},
        {  2,  1,  2, -1, -1}, // Using "2" twice is not a typo, but normal
                               //   according to the video team
        {  0, -1,  2, -1, -1},
        { -1,  1, -1,  3, -1},
        {  0, -1,  2, -1, -1},
        { -1,  1, -1,  3, -1}
    },
    // H264_HD_HP_CABAC_MS:
    {
        { -1, -1, -1, -1, -1},
        {  0, -1, -1, -1, -1},
        {  0,  1, -1, -1, -1},
        {  0,  1, -1, -1, -1}
    },
};

// Indexes to coloc surfaces used during current frame:
// { input, output}
//  "-1" means surface not used,
//  value >= 0 is an index to m_ColocData array
static const INT32 ColocInOutIndexes[NUM_STREAMS][MAX_NUM_FRAMES][2] =
{
    { { -1,  0}, {  0,  1}, {  1,  0}, {  0,  1} },
    { { -1,  0}, {  0,  1}, {  0,  0}, {  1,  1},
      {  0, -1}, {  1, -1}, {  0, -1}, {  1, -1} },
    { { -1,  0}, {  0,  1}, {  0,  0}, {  1,  1},
      {  0, -1}, {  1, -1}, {  0, -1}, {  1, -1} },
    { { -1,  0}, {  0,  1}, {  1, -1}, {  1, -1} },
};

// Indexes to me surfaces used during current frame:
// { input, output}
//  "-1" means surface not used,
//  value >= 0 is an index to m_MepredData array
static const INT32 MeInOutIndexes[NUM_STREAMS][MAX_NUM_FRAMES][2] =
{
    { { -1,  0}, {  0,  1}, {  1,  0}, {  0,  1} },
    { { -1,  0}, {  0,  1}, {  1,  0}, {  0,  1},
      {  1,  0}, {  0,  1}, {  1,  0}, {  0,  1} },
    { { -1, -1}, {  0,  0}, {  0,  1}, {  0,  2},
      {  1,  0}, {  2,  1}, {  0,  2}, {  1,  0} },
    { { -1,  0}, {  0,  1}, {  1,  0}, {  0,  1} },
};

#include "../../../drivers/common/msenc/drv2ucode/lwenc_drv.h"

typedef lwenc_h264_drv_pic_setup_s  msenc_h264_drv_pic_setup_s;
typedef lwenc_h264_slice_control_s  msenc_h264_slice_control_s;
typedef lwenc_h264_md_control_s     msenc_h264_md_control_s;
typedef lwenc_h264_quant_control_s  msenc_h264_quant_control_s;
typedef lwenc_h264_me_control_s     msenc_h264_me_control_s;
typedef lwenc_persistent_state_s    msenc_persistent_state_s;
typedef lwenc_stat_data_s           msenc_stat_data_s;
typedef lwenc_pic_stat_s            msenc_pic_stat_s;

const UINT32 AllControlStructsSize = static_cast<UINT32>(
    AlignUp<256>(sizeof(msenc_h264_drv_pic_setup_s)) +
    AlignUp<256>(sizeof(msenc_h264_slice_control_s)) +
    AlignUp<256>(sizeof(msenc_h264_md_control_s)) +
    AlignUp<256>(sizeof(msenc_h264_quant_control_s)) +
    AlignUp<256>(sizeof(msenc_h264_me_control_s)));

const UINT32 SliceControlOffset =
    AlignUp<256>(static_cast<UINT32>(sizeof(msenc_h264_drv_pic_setup_s)));
const UINT32 MdControlOffset = SliceControlOffset +
    AlignUp<256>(static_cast<UINT32>(sizeof(msenc_h264_slice_control_s)));
const UINT32 QuantControlOffset = MdControlOffset +
    AlignUp<256>(static_cast<UINT32>(sizeof(msenc_h264_md_control_s)));
const UINT32 MeControlOffset = QuantControlOffset +
    AlignUp<256>(static_cast<UINT32>(sizeof(msenc_h264_quant_control_s)));

const UINT32 OutBitstreamBufferSize = 0x4000;

class MSENCTest: public GpuTest
{
public:
    MSENCTest();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    virtual bool IsSupported();
    virtual bool IsSupportedVf();

    SETGET_PROP(SaveSurfaces, bool);
    SETGET_PROP(YUVInputFromFiles, bool);
    SETGET_PROP(AllocateInSysmem, bool);
    SETGET_PROP(MaxFrames, UINT32);
    SETGET_PROP(StreamSkipMask, UINT32);

private:
    enum
    {
        s_ENC = 0,
    };

    GpuTestConfiguration * m_pTestConfig;
    GpuGoldenSurfaces *    m_pGGSurfs;
    Goldelwalues *         m_pGolden;

    bool         m_SaveSurfaces;
    bool         m_YUVInputFromFiles;
    bool         m_AllocateInSysmem;
    UINT32       m_MaxFrames;
    UINT32       m_StreamSkipMask;

    UINT32       m_LwrStreamIdx;
    UINT32       m_LwrFrameIdx;

    LwRm::Handle m_hCh;
    Channel *    m_pCh;
    EncoderAlloc m_MSENCAlloc;

    Surface2D    m_Semaphore;

    Surface2D    m_YUVInput;
    Surface2D    m_EncodeStatus;
    Surface2D    m_IoRcProcess;
    Surface2D    m_YUVRefPic[MAX_REF_FRAMES];
    Surface2D    m_IoHistory;
    Surface2D    m_OutBitstream;
    Surface2D    m_ColocData[2];
    Surface2D    m_MepredData[3];
    Surface2D    m_PicSetup;

    void SaveSurfaces(UINT32 StreamIndex, UINT32 FrameIdx);
    RC FillYUVInput(UINT32 StreamIndex, UINT32 FrameIdx);
    RC RunFrame(UINT32 StreamIndex, UINT32 FrameIdx);
    static void PrintCallback(void* test);
    void Print(INT32 pri);
};

struct PollSemaphore_Args
{
    UINT32 *Semaphore;
    UINT32  ExpectedValue;
};

static bool GpuPollSemaVal
(
    void * Args
)
{
    PollSemaphore_Args * pArgs = static_cast<PollSemaphore_Args*>(Args);

    return (MEM_RD32(pArgs->Semaphore) == pArgs->ExpectedValue);
}

MSENCTest::MSENCTest()
: m_pGGSurfs(NULL)
, m_SaveSurfaces(false)
, m_YUVInputFromFiles(false)
, m_AllocateInSysmem(false)
, m_MaxFrames(~0U)
, m_StreamSkipMask(0)
, m_LwrStreamIdx(0)
, m_LwrFrameIdx(0)
, m_hCh(0)
, m_pCh(0)
{
    SetName("MSENCTest");
    m_pTestConfig = GetTestConfiguration();
    m_pGolden = GetGoldelwalues();

    m_MSENCAlloc.SetOldestSupportedClass(LW90B7_VIDEO_ENCODER);
    m_MSENCAlloc.SetNewestSupportedClass(LW90B7_VIDEO_ENCODER);
}

bool MSENCTest::IsSupported()
{
    // Not supported on CheetAh
    if (GetBoundGpuSubdevice()->IsSOC())
    {
        return false;
    }

    return m_MSENCAlloc.IsSupported(GetBoundGpuDevice());
}

//------------------------------------------------------------------------------
bool MSENCTest::IsSupportedVf()
{
    return !(GetBoundGpuSubdevice()->IsSMCEnabled());
}

static void WritePicSetupSurface
(
    UINT32 FrameIdx,
    Surface2D *PicSetupSurface,
    const msenc_h264_drv_pic_setup_s *PicSetup,
    const msenc_h264_slice_control_s *SliceControl,
    const msenc_h264_md_control_s    *MDControl,
    const msenc_h264_quant_control_s *QControl,
    const msenc_h264_me_control_s    *MEcontrol
)
{
    UINT08 *BaseAddress = (UINT08*)PicSetupSurface->GetAddress();
    BaseAddress += FrameIdx * AllControlStructsSize;

    Platform::VirtualWr(BaseAddress, PicSetup,
        sizeof(msenc_h264_drv_pic_setup_s));

    Platform::VirtualWr(BaseAddress +
        PicSetup->pic_control.slice_control_offset, SliceControl,
        sizeof(msenc_h264_slice_control_s));

    Platform::VirtualWr(BaseAddress +
        PicSetup->pic_control.md_control_offset, MDControl,
        sizeof(msenc_h264_md_control_s));

    Platform::VirtualWr(BaseAddress +
        PicSetup->pic_control.q_control_offset, QControl,
        sizeof(msenc_h264_quant_control_s));

    Platform::VirtualWr(BaseAddress +
        PicSetup->pic_control.me_control_offset, MEcontrol,
        sizeof(msenc_h264_me_control_s));
}

static RC PrepareControlStructuresH264BaselineCAVLC(Surface2D *PicSetupSurface)
{
    msenc_h264_drv_pic_setup_s PicSetup;
    memset(&PicSetup, 0, sizeof(PicSetup));

    PicSetup.magic = LW_MSENC_DRV_MAGIC_VALUE;
    PicSetup.refpic_cfg.frame_width_minus1  = 176-1;
    PicSetup.refpic_cfg.frame_height_minus1 = 144-1;
    PicSetup.refpic_cfg.sfc_pitch           = 176;
    PicSetup.refpic_cfg.chroma_top_frm_offset = 99;
    PicSetup.input_cfg.frame_width_minus1  = 176-1;
    PicSetup.input_cfg.frame_height_minus1 = 144-1;
    PicSetup.input_cfg.sfc_pitch           = 192;
    PicSetup.input_cfg.chroma_top_frm_offset = 108;
    PicSetup.input_cfg.block_height = 2;
    PicSetup.outputpic_cfg.frame_width_minus1  = 176-1;
    PicSetup.outputpic_cfg.frame_height_minus1 = 144-1;
    PicSetup.outputpic_cfg.sfc_pitch           = 176;
    PicSetup.outputpic_cfg.chroma_top_frm_offset = 99;
    PicSetup.outputpic_cfg.chroma_bot_offset = 99;
    PicSetup.outputpic_cfg.tiled_16x16 = 1;
    PicSetup.sps_data.profile_idc = 0x00000042;
    PicSetup.sps_data.level_idc = 0x0000001e;
    PicSetup.sps_data.chroma_format_idc = 0x00000001;
    PicSetup.sps_data.pic_order_cnt_type = 0x00000002;
    PicSetup.sps_data.log2_max_frame_num_minus4 = 0x00000004;
    PicSetup.sps_data.log2_max_pic_order_cnt_lsb_minus4 = 0x00000004;
    PicSetup.pps_data.deblocking_filter_control_present_flag = 1;
    PicSetup.rate_control.hrd_type = 0x02;
    PicSetup.rate_control.QP[0] = 0x1c;
    PicSetup.rate_control.QP[1] = 0x1f;
    PicSetup.rate_control.QP[2] = 0x19;
    PicSetup.rate_control.minQP[0] = 0x00;
    PicSetup.rate_control.minQP[1] = 0x00;
    PicSetup.rate_control.minQP[2] = 0x00;
    PicSetup.rate_control.maxQP[0] = 0x33;
    PicSetup.rate_control.maxQP[1] = 0x33;
    PicSetup.rate_control.maxQP[2] = 0x33;
    PicSetup.rate_control.maxQPD = 0x06;
    PicSetup.rate_control.baseQPD = 0x03;
    PicSetup.rate_control.rhopbi[0] = 0x00000100;
    PicSetup.rate_control.rhopbi[1] = 0x00000100;
    PicSetup.rate_control.rhopbi[2] = 0x00000100;
    PicSetup.rate_control.framerate = 0x00001e00;
    PicSetup.rate_control.buffersize = 0x000fa000;
    PicSetup.rate_control.nal_cpb_size = 0x000fa000;
    PicSetup.rate_control.nal_bitrate = 0x0007d000;
    PicSetup.rate_control.gop_length = 0x0000000a;
    for (UINT32 idx = 0; idx <= 7; idx++)
    {
        PicSetup.pic_control.l0[idx] = -2;
        PicSetup.pic_control.l1[idx] = -2;
    }
    PicSetup.pic_control.slice_control_offset = SliceControlOffset;
    PicSetup.pic_control.md_control_offset    = MdControlOffset;
    PicSetup.pic_control.q_control_offset     = QuantControlOffset;
    PicSetup.pic_control.me_control_offset    = MeControlOffset;
    PicSetup.pic_control.hist_buf_size = 0x00003700;
    PicSetup.pic_control.bitstream_buf_size = OutBitstreamBufferSize;
    PicSetup.pic_control.pic_type = 0x00000003;
    PicSetup.pic_control.ref_pic_flag = 1;
    PicSetup.pic_control.ipcm_rewind_enable = 1;

    msenc_h264_slice_control_s slice_control;
    memset(&slice_control, 0, sizeof(slice_control));

    slice_control.num_mb = 0x00000063;
    slice_control.qp_avr = 0x00000019;

    msenc_h264_md_control_s md_control;
    memset(&md_control, 0, sizeof(md_control));

    md_control.intra_luma4x4_mode_enable = 0x000001ff;
    md_control.intra_luma16x16_mode_enable = 0x0000000f;
    md_control.intra_chroma_mode_enable = 0x0000000f;
    md_control.l0_part_16x16_enable = 0x00000001;
    md_control.l0_part_16x8_enable = 0x00000001;
    md_control.l0_part_8x16_enable = 0x00000001;
    md_control.l0_part_8x8_enable = 0x00000001;
    md_control.l1_part_16x16_enable = 0x00000001;
    md_control.l1_part_16x8_enable = 0x00000001;
    md_control.l1_part_8x16_enable = 0x00000001;
    md_control.l1_part_8x8_enable = 0x00000001;
    md_control.bi_part_16x16_enable = 0x00000001;
    md_control.bi_part_16x8_enable = 0x00000001;
    md_control.bi_part_8x16_enable = 0x00000001;
    md_control.bi_part_8x8_enable = 0x00000001;
    md_control.intra_nxn_bias_multiplier = 0x0018;
    md_control.intra_most_prob_bias_multiplier = 0x0004;
    md_control.mv_cost_enable = 0x00000001;
    md_control.ip_search_mode = 0x00000007;

    msenc_h264_quant_control_s q_control;
    memset(&q_control, 0, sizeof(q_control));

    q_control.qpp_run_vector_4x4 = 0x056b;
    q_control.qpp_run_vector_8x8[0] = 0xaaff;
    q_control.qpp_run_vector_8x8[1] = 0xffaa;
    q_control.qpp_run_vector_8x8[2] = 0x000f;
    q_control.qpp_luma8x8_cost = 0x0f;
    q_control.qpp_luma16x16_cost = 0x0f;
    q_control.qpp_chroma_cost = 0x0f;
    q_control.quant_intra_sat_flag = 0x01;
    q_control.quant_inter_sat_flag = 0x01;
    q_control.quant_intra_sat_limit = 0x0000080f;
    q_control.quant_inter_sat_limit = 0x0000080f;
    q_control.dz_4x4_YI[0x0] = 0x03ff;
    q_control.dz_4x4_YI[0x1] = 0x036e;
    q_control.dz_4x4_YI[0x2] = 0x0334;
    q_control.dz_4x4_YI[0x3] = 0x02aa;
    q_control.dz_4x4_YI[0x4] = 0x036e;
    q_control.dz_4x4_YI[0x5] = 0x0334;
    q_control.dz_4x4_YI[0x6] = 0x02aa;
    q_control.dz_4x4_YI[0x7] = 0x0200;
    q_control.dz_4x4_YI[0x8] = 0x0334;
    q_control.dz_4x4_YI[0x9] = 0x02aa;
    q_control.dz_4x4_YI[0xa] = 0x0200;
    q_control.dz_4x4_YI[0xb] = 0x019a;
    q_control.dz_4x4_YI[0xc] = 0x02aa;
    q_control.dz_4x4_YI[0xd] = 0x0200;
    q_control.dz_4x4_YI[0xe] = 0x019a;
    q_control.dz_4x4_YI[0xf] = 0x019a;
    q_control.dz_4x4_YP[0x0] = 0x02aa;
    q_control.dz_4x4_YP[0x1] = 0x024a;
    q_control.dz_4x4_YP[0x2] = 0x0222;
    q_control.dz_4x4_YP[0x3] = 0x01c8;
    q_control.dz_4x4_YP[0x4] = 0x024a;
    q_control.dz_4x4_YP[0x5] = 0x0222;
    q_control.dz_4x4_YP[0x6] = 0x01c8;
    q_control.dz_4x4_YP[0x7] = 0x0156;
    q_control.dz_4x4_YP[0x8] = 0x0222;
    q_control.dz_4x4_YP[0x9] = 0x01c8;
    q_control.dz_4x4_YP[0xa] = 0x0156;
    q_control.dz_4x4_YP[0xb] = 0x0124;
    q_control.dz_4x4_YP[0xc] = 0x01c8;
    q_control.dz_4x4_YP[0xd] = 0x0156;
    q_control.dz_4x4_YP[0xe] = 0x0124;
    q_control.dz_4x4_YP[0xf] = 0x0112;
    q_control.dz_4x4_CI = 0x02aa;
    q_control.dz_4x4_CP = 0x0156;

    for (UINT32 idx = 0; idx <= 0xf; idx++)
    {
        q_control.dz_8x8_YI[idx] = 0x02aa;
        q_control.dz_8x8_YP[idx] = 0x0156;
    }

    msenc_h264_me_control_s me_control;
    memset(&me_control, 0, sizeof(me_control));

    me_control.me_predictor_mode = 0x00000001;
    me_control.refinement_mode = 0x00000001;
    me_control.lambda_mode = 0x00000001;
    me_control.const_lambda = 0x00000004;
    me_control.refine_on_search_enable = 0x00000001;
    me_control.limit_mv.mv_limit_enable = 0x00000001;
    me_control.limit_mv.left_mvx_int = -2048; //0x800;
    me_control.limit_mv.top_mvy_int = -256; //0x300;
    me_control.limit_mv.right_mvx_frac = 0x00000003;
    me_control.limit_mv.right_mvx_int = 0x000007ff;
    me_control.limit_mv.bottom_mvy_frac = 0x00000003;
    me_control.limit_mv.bottom_mvy_int = 0x000000ff;
    me_control.predsrc.self_spatial_search = 0x00000001;
    me_control.predsrc.self_spatial_refine = 0x00000001;
    me_control.predsrc.self_spatial_enable = 0x00000001;
    me_control.predsrc.const_mv_search = 0x00000001;
    me_control.predsrc.const_mv_refine = 0x00000001;
    me_control.predsrc.const_mv_enable = 0x00000001;
    me_control.shape0.bitmask[0] = 0x7e7e7e7e;
    me_control.shape0.bitmask[1] = 0x7e7e7e7e;

    WritePicSetupSurface(0, PicSetupSurface, &PicSetup, &slice_control,
        &md_control, &q_control, &me_control);

    PicSetup.pic_control.l0[0] = 0;

    for (UINT32 idx = 0; idx <= 7; idx++)
    {
        PicSetup.pic_control.temp_dist_l0[idx] = -1;
        PicSetup.pic_control.temp_dist_l1[idx] = -1;
    }
    PicSetup.pic_control.temp_dist_l0[0] = 0x01;

    for (UINT32 refdix1 = 0; refdix1 <= 7; refdix1++)
    {
        for (UINT32 refdix0 = 0; refdix0 <= 7; refdix0++)
        {
            PicSetup.pic_control.dist_scale_factor[refdix1][refdix0] =
                (refdix0 == 0) ? 0x0100 : 0xffff;
        }
    }
    PicSetup.pic_control.diff_pic_order_cnt_zero[0] = 0xfefefefe;
    PicSetup.pic_control.diff_pic_order_cnt_zero[1] = 0xfefefefe;

    PicSetup.pic_control.frame_num = 0x0001;
    PicSetup.pic_control.pic_order_cnt_lsb = 0x0002;
    PicSetup.pic_control.pic_type = 0x00000000;

    slice_control.qp_avr = 0x0000001c;

    WritePicSetupSurface(1, PicSetupSurface, &PicSetup, &slice_control,
        &md_control, &q_control, &me_control);

    PicSetup.pic_control.l0[0] = 2;
    PicSetup.pic_control.frame_num = 0x0002;
    PicSetup.pic_control.pic_order_cnt_lsb = 0x0004;

    WritePicSetupSurface(2, PicSetupSurface, &PicSetup, &slice_control,
        &md_control, &q_control, &me_control);

    PicSetup.pic_control.l0[0] = 4;
    PicSetup.pic_control.frame_num = 0x0003;
    PicSetup.pic_control.pic_order_cnt_lsb = 0x0006;

    WritePicSetupSurface(3, PicSetupSurface, &PicSetup, &slice_control,
        &md_control, &q_control, &me_control);

    return OK;
}

static RC PrepareControlStructuresH264HighprofileCABAC
(
    Surface2D *PicSetupSurface,
    Surface2D *IoRcProcess
)
{
    msenc_h264_drv_pic_setup_s PicSetup;
    memset(&PicSetup, 0, sizeof(PicSetup));

    PicSetup.magic = LW_MSENC_DRV_MAGIC_VALUE;
    PicSetup.refpic_cfg.frame_width_minus1  = 176-1;
    PicSetup.refpic_cfg.frame_height_minus1 = 80-1;
    PicSetup.refpic_cfg.sfc_pitch           = 176;
    PicSetup.refpic_cfg.luma_top_frm_offset   = 0;
    PicSetup.refpic_cfg.luma_bot_offset       = 55;
    PicSetup.refpic_cfg.chroma_top_frm_offset = 110;
    PicSetup.refpic_cfg.chroma_bot_offset     = 143;
    PicSetup.input_cfg.frame_width_minus1  = 176-1;
    PicSetup.input_cfg.frame_height_minus1 = 80-1;
    PicSetup.input_cfg.sfc_pitch           = 192;
    PicSetup.input_cfg.chroma_top_frm_offset = 60;
    PicSetup.input_cfg.chroma_bot_offset     = 60;
    PicSetup.input_cfg.block_height = 2;
    PicSetup.outputpic_cfg.frame_width_minus1  = 176-1;
    PicSetup.outputpic_cfg.frame_height_minus1 = 80-1;
    PicSetup.outputpic_cfg.sfc_pitch           = 176;
    PicSetup.outputpic_cfg.luma_top_frm_offset   = 0;
    PicSetup.outputpic_cfg.luma_bot_offset       = 55;
    PicSetup.outputpic_cfg.chroma_top_frm_offset = 110;
    PicSetup.outputpic_cfg.chroma_bot_offset     = 143;
    PicSetup.outputpic_cfg.tiled_16x16 = 1;
    PicSetup.sps_data.profile_idc = 0x00000064;
    PicSetup.sps_data.level_idc = 0x00000029;
    PicSetup.sps_data.chroma_format_idc = 0x00000001;
    PicSetup.sps_data.log2_max_frame_num_minus4 = 0x00000004;
    PicSetup.sps_data.log2_max_pic_order_cnt_lsb_minus4 = 0x00000004;
    PicSetup.sps_data.frame_mbs_only = 1;
    PicSetup.pps_data.entropy_coding_mode_flag = 1;
    PicSetup.pps_data.num_ref_idx_l0_active_minus1 = 3;
    PicSetup.pps_data.deblocking_filter_control_present_flag = 1;
    PicSetup.pps_data.transform_8x8_mode_flag = 1;
    PicSetup.rate_control.hrd_type = 0x02;
    PicSetup.rate_control.QP[0] = 0x1c;
    PicSetup.rate_control.QP[1] = 0x1f;
    PicSetup.rate_control.QP[2] = 0x19;
    PicSetup.rate_control.maxQP[0] = 0x33;
    PicSetup.rate_control.maxQP[1] = 0x33;
    PicSetup.rate_control.maxQP[2] = 0x33;
    PicSetup.rate_control.maxQPD = 0x06;
    PicSetup.rate_control.baseQPD = 0x03;
    PicSetup.rate_control.rhopbi[0] = 0x0000011e;
    PicSetup.rate_control.rhopbi[1] = 0x0000013d;
    PicSetup.rate_control.rhopbi[2] = 0x00000100;
    PicSetup.rate_control.framerate = 0x00001e00;
    PicSetup.rate_control.buffersize = 0x0007d000;
    PicSetup.rate_control.nal_cpb_size = 0x0007d000;
    PicSetup.rate_control.nal_bitrate = 0x0003e800;
    PicSetup.rate_control.gop_length = 0x0000000a;
    PicSetup.rate_control.Np = 3;
    PicSetup.rate_control.Bmin = 0x00000745;
    PicSetup.rate_control.R = 0x0000009b;
    for (UINT32 idx = 0; idx <= 7; idx++)
    {
        PicSetup.pic_control.l0[idx] = -2;
        PicSetup.pic_control.l1[idx] = -2;
    }
    PicSetup.pic_control.max_slice_size       = 0x000001f4;
    PicSetup.pic_control.slice_control_offset = SliceControlOffset;
    PicSetup.pic_control.md_control_offset    = MdControlOffset;
    PicSetup.pic_control.q_control_offset     = QuantControlOffset;
    PicSetup.pic_control.me_control_offset    = MeControlOffset;
    PicSetup.pic_control.hist_buf_size = 0x00002100;
    PicSetup.pic_control.bitstream_buf_size = OutBitstreamBufferSize;
    PicSetup.pic_control.pic_struct = 0x00000001;
    PicSetup.pic_control.pic_type = 0x00000003;
    PicSetup.pic_control.ref_pic_flag = 1;
    PicSetup.pic_control.slice_mode = 1;
    PicSetup.pic_control.ipcm_rewind_enable = 1;

    msenc_h264_slice_control_s slice_control;
    memset(&slice_control, 0, sizeof(slice_control));

    slice_control.num_mb = 0x00000037;
    slice_control.qp_avr = 0x00000014;

    msenc_h264_md_control_s md_control;
    memset(&md_control, 0, sizeof(md_control));

    md_control.intra_luma4x4_mode_enable = 0x000001ff;
    md_control.intra_luma8x8_mode_enable = 0x000001ff;
    md_control.intra_luma16x16_mode_enable = 0x0000000f;
    md_control.intra_chroma_mode_enable = 0x0000000f;
    md_control.l0_part_16x16_enable = 0x00000001;
    md_control.l0_part_16x8_enable = 0x00000001;
    md_control.l0_part_8x16_enable = 0x00000001;
    md_control.l0_part_8x8_enable = 0x00000001;
    md_control.l1_part_16x16_enable = 0x00000001;
    md_control.l1_part_16x8_enable = 0x00000001;
    md_control.l1_part_8x16_enable = 0x00000001;
    md_control.l1_part_8x8_enable = 0x00000001;
    md_control.bi_part_16x16_enable = 0x00000001;
    md_control.bi_part_16x8_enable = 0x00000001;
    md_control.bi_part_8x16_enable = 0x00000001;
    md_control.bi_part_8x8_enable = 0x00000001;
    md_control.bdirect_mode = 0x00000002;
    md_control.intra_nxn_bias_multiplier = 0x0018;
    md_control.intra_most_prob_bias_multiplier = 0x0004;
    md_control.mv_cost_enable = 0x00000001;
    md_control.ip_search_mode = 0x00000007;

    msenc_h264_quant_control_s q_control;
    memset(&q_control, 0, sizeof(q_control));

    q_control.qpp_run_vector_4x4 = 0x056b;
    q_control.qpp_run_vector_8x8[0] = 0xaaff;
    q_control.qpp_run_vector_8x8[1] = 0xffaa;
    q_control.qpp_run_vector_8x8[2] = 0x000f;
    q_control.qpp_luma8x8_cost = 0x0f;
    q_control.qpp_luma16x16_cost = 0x0f;
    q_control.qpp_chroma_cost = 0x0f;
    q_control.dz_4x4_YI[0x0] = 0x03ff;
    q_control.dz_4x4_YI[0x1] = 0x036e;
    q_control.dz_4x4_YI[0x2] = 0x0334;
    q_control.dz_4x4_YI[0x3] = 0x02aa;
    q_control.dz_4x4_YI[0x4] = 0x036e;
    q_control.dz_4x4_YI[0x5] = 0x0334;
    q_control.dz_4x4_YI[0x6] = 0x02aa;
    q_control.dz_4x4_YI[0x7] = 0x0200;
    q_control.dz_4x4_YI[0x8] = 0x0334;
    q_control.dz_4x4_YI[0x9] = 0x02aa;
    q_control.dz_4x4_YI[0xa] = 0x0200;
    q_control.dz_4x4_YI[0xb] = 0x019a;
    q_control.dz_4x4_YI[0xc] = 0x02aa;
    q_control.dz_4x4_YI[0xd] = 0x0200;
    q_control.dz_4x4_YI[0xe] = 0x019a;
    q_control.dz_4x4_YI[0xf] = 0x019a;
    q_control.dz_4x4_YP[0x0] = 0x02aa;
    q_control.dz_4x4_YP[0x1] = 0x024a;
    q_control.dz_4x4_YP[0x2] = 0x0222;
    q_control.dz_4x4_YP[0x3] = 0x01c8;
    q_control.dz_4x4_YP[0x4] = 0x024a;
    q_control.dz_4x4_YP[0x5] = 0x0222;
    q_control.dz_4x4_YP[0x6] = 0x01c8;
    q_control.dz_4x4_YP[0x7] = 0x0156;
    q_control.dz_4x4_YP[0x8] = 0x0222;
    q_control.dz_4x4_YP[0x9] = 0x01c8;
    q_control.dz_4x4_YP[0xa] = 0x0156;
    q_control.dz_4x4_YP[0xb] = 0x0124;
    q_control.dz_4x4_YP[0xc] = 0x01c8;
    q_control.dz_4x4_YP[0xd] = 0x0156;
    q_control.dz_4x4_YP[0xe] = 0x0124;
    q_control.dz_4x4_YP[0xf] = 0x0112;
    q_control.dz_4x4_CI = 0x02aa;
    q_control.dz_4x4_CP = 0x0156;

    for (UINT32 idx = 0; idx <= 0xf; idx++)
    {
        q_control.dz_8x8_YI[idx] = 0x02aa;
        q_control.dz_8x8_YP[idx] = 0x0156;
    }

    msenc_h264_me_control_s me_control;
    memset(&me_control, 0, sizeof(me_control));

    me_control.refinement_mode = 0x00000001;
    me_control.refine_on_search_enable = 0x00000001;
    me_control.limit_mv.mv_limit_enable = 0x00000001;
    me_control.limit_mv.left_mvx_int = -2048; //0x800;
    me_control.limit_mv.top_mvy_int  = -512; // 0x200;
    me_control.limit_mv.right_mvx_frac = 0x00000003;
    me_control.limit_mv.right_mvx_int  = 0x000007ff;
    me_control.limit_mv.bottom_mvy_frac = 0x00000003;
    me_control.limit_mv.bottom_mvy_int  = 0x000001ff;
    me_control.predsrc.self_temporal_stamp_l0 = 0x00000001;
    me_control.predsrc.self_temporal_stamp_l1 = 0x00000001;
    me_control.predsrc.self_temporal_search = 0x00000001;
    me_control.predsrc.self_temporal_refine = 0x00000001;
    me_control.predsrc.self_temporal_enable = 0x00000001;
    me_control.predsrc.coloc_stamp_l0 = 0x00000002;
    me_control.predsrc.coloc_stamp_l1 = 0x00000002;
    me_control.predsrc.coloc_search = 0x00000001;
    me_control.predsrc.coloc_refine = 0x00000001;
    me_control.predsrc.coloc_enable = 0x00000001;
    me_control.predsrc.self_spatial_search = 0x00000001;
    me_control.predsrc.self_spatial_refine = 0x00000001;
    me_control.predsrc.self_spatial_enable = 0x00000001;
    me_control.predsrc.const_mv_stamp_l0 = 0x00000001;
    me_control.predsrc.const_mv_stamp_l1 = 0x00000001;
    me_control.predsrc.const_mv_search = 0x00000001;
    me_control.predsrc.const_mv_refine = 0x00000001;
    me_control.predsrc.const_mv_enable = 0x00000001;
    me_control.shape1.bitmask[0] = 0x7f3e1c08;
    me_control.shape1.bitmask[1] = 0x081c3e3e;
    me_control.shape2.bitmask[0] = 0xff7e3c18;
    me_control.shape2.bitmask[1] = 0x183c7eff;

    WritePicSetupSurface(0, PicSetupSurface, &PicSetup, &slice_control,
        &md_control, &q_control, &me_control);

    PicSetup.pic_control.l0[0] = 0;

    for (UINT32 idx = 0; idx <= 7; idx++)
    {
        PicSetup.pic_control.temp_dist_l0[idx] = -1;
        PicSetup.pic_control.temp_dist_l1[idx] = -1;
    }
    PicSetup.pic_control.temp_dist_l0[0] = 0;

    for (UINT32 refdix1 = 0; refdix1 <= 7; refdix1++)
    {
        for (UINT32 refdix0 = 0; refdix0 <= 7; refdix0++)
        {
            PicSetup.pic_control.dist_scale_factor[refdix1][refdix0] =
                (refdix0 == 0) ? 0x0100 : 0xffff;
        }
    }
    PicSetup.pic_control.diff_pic_order_cnt_zero[0] = 0xfefefefe;
    PicSetup.pic_control.diff_pic_order_cnt_zero[1] = 0xfefefefe;

    PicSetup.pic_control.frame_num = 0x0000;
    PicSetup.pic_control.pic_order_cnt_lsb = 0x0001;
    PicSetup.pic_control.pic_struct = 0x00000002;
    PicSetup.pic_control.pic_type = 0x00000000;

    slice_control.qp_avr = 0x00000032;
    slice_control.num_ref_idx_active_override_flag = 1;

    WritePicSetupSurface(1, PicSetupSurface, &PicSetup, &slice_control,
        &md_control, &q_control, &me_control);

    PicSetup.pic_control.l0[1] = 1;
    PicSetup.pic_control.temp_dist_l0[0] = 3;
    PicSetup.pic_control.temp_dist_l0[1] = 2;
    for (UINT32 refdix1 = 0; refdix1 <= 7; refdix1++)
    {
        PicSetup.pic_control.dist_scale_factor[refdix1][1] = 0x0100;
    }
    PicSetup.pic_control.diff_pic_order_cnt_zero[0] = 0xfcfcfcfc;
    PicSetup.pic_control.diff_pic_order_cnt_zero[1] = 0xfcfcfcfc;

    PicSetup.pic_control.frame_num = 0x0001;
    PicSetup.pic_control.pic_order_cnt_lsb = 0x0006;
    PicSetup.pic_control.pic_struct = 0x00000001;

    slice_control.num_ref_idx_l0_active_minus1 = 1;

    WritePicSetupSurface(2, PicSetupSurface, &PicSetup, &slice_control,
        &md_control, &q_control, &me_control);

    PicSetup.pic_control.l0[0] = 1;
    PicSetup.pic_control.l0[1] = 2;
    PicSetup.pic_control.l0[2] = 0;
    PicSetup.pic_control.temp_dist_l0[1] = 0;
    PicSetup.pic_control.temp_dist_l0[2] = 3;
    for (UINT32 refdix1 = 0; refdix1 <= 7; refdix1++)
    {
        PicSetup.pic_control.dist_scale_factor[refdix1][2] = 0x0100;
    }
    PicSetup.pic_control.diff_pic_order_cnt_zero[0] = 0xf8f8f8f8;
    PicSetup.pic_control.diff_pic_order_cnt_zero[1] = 0xf8f8f8f8;

    PicSetup.pic_control.pic_order_cnt_lsb = 0x0007;
    PicSetup.pic_control.pic_struct = 0x00000002;

    slice_control.num_ref_idx_l0_active_minus1 = 2;

    WritePicSetupSurface(3, PicSetupSurface, &PicSetup, &slice_control,
        &md_control, &q_control, &me_control);

    PicSetup.pic_control.l0[0] = 0;
    PicSetup.pic_control.l0[1] = 1;
    PicSetup.pic_control.l0[2] = 2;
    PicSetup.pic_control.l0[3] = 3;
    PicSetup.pic_control.l1[0] = 2;
    PicSetup.pic_control.temp_dist_l0[0] = 1;
    PicSetup.pic_control.temp_dist_l0[2] = -2;
    PicSetup.pic_control.temp_dist_l0[3] = -3;
    PicSetup.pic_control.temp_dist_l1[0] = -2;
    for (UINT32 refdix1 = 0; refdix1 <= 7; refdix1++)
    {
        for (UINT32 refdix0 = 0; refdix0 <= 7; refdix0++)
        {
            PicSetup.pic_control.dist_scale_factor[refdix1][refdix0] = -1;
        }
    }
    PicSetup.pic_control.dist_scale_factor[0][0] = 0x0055;
    PicSetup.pic_control.dist_scale_factor[0][1] = 0x0033;
    PicSetup.pic_control.dist_scale_factor[0][3] = 0x03ff;

    PicSetup.pic_control.diff_pic_order_cnt_zero[0] = 0xf0f0f004;
    PicSetup.pic_control.diff_pic_order_cnt_zero[1] = 0xf0f0f0f0;

    PicSetup.pic_control.frame_num = 0x0002;
    PicSetup.pic_control.pic_order_cnt_lsb = 0x0002;
    PicSetup.pic_control.pic_struct   = 0x00000001;
    PicSetup.pic_control.pic_type     = 0x00000001;
    PicSetup.pic_control.ref_pic_flag = 0x00000000;

    slice_control.num_ref_idx_active_override_flag = 0;
    slice_control.num_ref_idx_l0_active_minus1 = 3;

    WritePicSetupSurface(4, PicSetupSurface, &PicSetup, &slice_control,
        &md_control, &q_control, &me_control);

    PicSetup.pic_control.l0[0] = 1;
    PicSetup.pic_control.l0[1] = 0;
    PicSetup.pic_control.l0[2] = 3;
    PicSetup.pic_control.l0[3] = 2;
    PicSetup.pic_control.l1[0] = 3;
    PicSetup.pic_control.temp_dist_l0[1] = 1;
    PicSetup.pic_control.temp_dist_l0[3] = -2;
    PicSetup.pic_control.dist_scale_factor[0][1] = 0x006e;
    PicSetup.pic_control.dist_scale_factor[0][3] = -768;

    PicSetup.pic_control.pic_order_cnt_lsb = 0x0003;
    PicSetup.pic_control.pic_struct   = 0x00000002;

    WritePicSetupSurface(5, PicSetupSurface, &PicSetup, &slice_control,
        &md_control, &q_control, &me_control);

    PicSetup.pic_control.l0[0] = 0;
    PicSetup.pic_control.l0[1] = 1;
    PicSetup.pic_control.l0[2] = 2;
    PicSetup.pic_control.l0[3] = 3;
    PicSetup.pic_control.l1[0] = 2;
    PicSetup.pic_control.temp_dist_l0[0] = 2;
    PicSetup.pic_control.temp_dist_l0[2] = -1;
    PicSetup.pic_control.temp_dist_l1[0] = -1;
    PicSetup.pic_control.dist_scale_factor[0][0] = 0x00ab;
    PicSetup.pic_control.dist_scale_factor[0][1] = 0x009a;
    PicSetup.pic_control.dist_scale_factor[0][3] = 0x0300;

    PicSetup.pic_control.pic_order_cnt_lsb = 0x0004;
    PicSetup.pic_control.pic_struct = 0x00000001;

    WritePicSetupSurface(6, PicSetupSurface, &PicSetup, &slice_control,
        &md_control, &q_control, &me_control);

    PicSetup.pic_control.l0[0] = 1;
    PicSetup.pic_control.l0[1] = 0;
    PicSetup.pic_control.l0[2] = 3;
    PicSetup.pic_control.l0[3] = 2;
    PicSetup.pic_control.l1[0] = 3;
    PicSetup.pic_control.temp_dist_l0[1] = 2;
    PicSetup.pic_control.temp_dist_l0[3] = -1;
    PicSetup.pic_control.dist_scale_factor[0][1] = 0x00b7;
    PicSetup.pic_control.dist_scale_factor[0][3] = -256;

    PicSetup.pic_control.pic_order_cnt_lsb = 0x0005;
    PicSetup.pic_control.pic_struct = 0x00000002;

    WritePicSetupSurface(7, PicSetupSurface, &PicSetup, &slice_control,
        &md_control, &q_control, &me_control);

    msenc_persistent_state_s RcProcess;
    memset(&RcProcess, 0, sizeof(RcProcess));

    RcProcess.nal_cpb_fullness = 0x00190000;
    RcProcess.Wpbi[0] = 0x0000010f;
    RcProcess.Wpbi[1] = 0x0000012c;
    RcProcess.Wpbi[2] = 0x000000f2;
    for (UINT32 idx = 0; idx < 3; idx++)
    {
        RcProcess.Whist[0][idx] = 0x0000010f;
        RcProcess.Whist[1][idx] = 0x0000012c;
        RcProcess.Whist[2][idx] = 0x000000f2;
    }
    RcProcess.np = 0x00000030;
    RcProcess.nb = 0x00000060;

    Platform::VirtualWr(IoRcProcess->GetAddress(), &RcProcess,
        sizeof(RcProcess));

    return OK;
}

static RC PrepareControlStructuresH264MVCHighprofileCABAC
(
    Surface2D *PicSetupSurface
)
{
    msenc_h264_drv_pic_setup_s PicSetup;
    memset(&PicSetup, 0, sizeof(PicSetup));

    PicSetup.magic = LW_MSENC_DRV_MAGIC_VALUE;
    PicSetup.refpic_cfg.frame_width_minus1  = 176-1;
    PicSetup.refpic_cfg.frame_height_minus1 = 144-1;
    PicSetup.refpic_cfg.sfc_pitch           = 176;
    PicSetup.refpic_cfg.chroma_top_frm_offset = 99;
    PicSetup.input_cfg.frame_width_minus1  = 176-1;
    PicSetup.input_cfg.frame_height_minus1 = 144-1;
    PicSetup.input_cfg.sfc_pitch           = 192;
    PicSetup.input_cfg.chroma_top_frm_offset = 108;
    PicSetup.input_cfg.block_height = 2;
    PicSetup.outputpic_cfg.frame_width_minus1  = 176-1;
    PicSetup.outputpic_cfg.frame_height_minus1 = 144-1;
    PicSetup.outputpic_cfg.sfc_pitch           = 176;
    PicSetup.outputpic_cfg.chroma_top_frm_offset = 99;
    PicSetup.outputpic_cfg.chroma_bot_offset = 99;
    PicSetup.outputpic_cfg.tiled_16x16 = 1;
    PicSetup.sps_data.profile_idc = 0x00000080;
    PicSetup.sps_data.level_idc = 0x00000029;
    PicSetup.sps_data.chroma_format_idc = 0x00000001;
    PicSetup.sps_data.log2_max_frame_num_minus4 = 0x00000004;
    PicSetup.sps_data.log2_max_pic_order_cnt_lsb_minus4 = 0x00000004;
    PicSetup.sps_data.stereo_mvc_enable = 0x00000001;
    PicSetup.pps_data.entropy_coding_mode_flag = 1;
    PicSetup.pps_data.num_ref_idx_l0_active_minus1 = 3;
    PicSetup.pps_data.deblocking_filter_control_present_flag = 1;
    PicSetup.pps_data.transform_8x8_mode_flag = 1;
    PicSetup.rate_control.hrd_type = 0x02;
    PicSetup.rate_control.QP[0] = 0x1c;
    PicSetup.rate_control.QP[1] = 0x1f;
    PicSetup.rate_control.QP[2] = 0x19;
    PicSetup.rate_control.minQP[0] = 0x00;
    PicSetup.rate_control.minQP[1] = 0x00;
    PicSetup.rate_control.minQP[2] = 0x00;
    PicSetup.rate_control.maxQP[0] = 0x33;
    PicSetup.rate_control.maxQP[1] = 0x33;
    PicSetup.rate_control.maxQP[2] = 0x33;
    PicSetup.rate_control.maxQPD = 0x06;
    PicSetup.rate_control.baseQPD = 0x03;
    PicSetup.rate_control.rhopbi[0] = 0x00000100;
    PicSetup.rate_control.rhopbi[1] = 0x00000100;
    PicSetup.rate_control.rhopbi[2] = 0x00000100;
    PicSetup.rate_control.framerate = 0x00001e00;
    PicSetup.rate_control.buffersize = 0x001f4000;
    PicSetup.rate_control.nal_cpb_size = 0x001f4000;
    PicSetup.rate_control.nal_bitrate = 0x000fa000;
    PicSetup.rate_control.gop_length = 0x0000000a;
    for (UINT32 idx = 0; idx <= 7; idx++)
    {
        PicSetup.pic_control.l0[idx] = -2;
        PicSetup.pic_control.l1[idx] = -2;
    }
    PicSetup.pic_control.slice_control_offset = SliceControlOffset;
    PicSetup.pic_control.md_control_offset    = MdControlOffset;
    PicSetup.pic_control.q_control_offset     = QuantControlOffset;
    PicSetup.pic_control.me_control_offset    = MeControlOffset;
    PicSetup.pic_control.hist_buf_size = 0x00003700;
    PicSetup.pic_control.bitstream_buf_size = OutBitstreamBufferSize;
    PicSetup.pic_control.pic_type = 0x00000003;
    PicSetup.pic_control.ref_pic_flag = 1,
    PicSetup.pic_control.base_view = 1,
    PicSetup.pic_control.anchor_pic_flag = 1,
    PicSetup.pic_control.inter_view_flag = 1;
    PicSetup.pic_control.lwr_interview_ref_pic = -2;
    PicSetup.pic_control.prev_interview_ref_pic = -2;

    msenc_h264_slice_control_s slice_control;
    memset(&slice_control, 0, sizeof(slice_control));

    slice_control.num_mb = 0x00000063;
    slice_control.qp_avr = 0x00000019;

    msenc_h264_md_control_s md_control;
    memset(&md_control, 0, sizeof(md_control));

    md_control.intra_luma4x4_mode_enable = 0x000001ff;
    md_control.intra_luma8x8_mode_enable = 0x000001ff;
    md_control.intra_luma16x16_mode_enable = 0x0000000f;
    md_control.intra_chroma_mode_enable = 0x0000000f;
    md_control.l0_part_16x16_enable = 0x00000001;
    md_control.l0_part_16x8_enable = 0x00000001;
    md_control.l0_part_8x16_enable = 0x00000001;
    md_control.l0_part_8x8_enable = 0x00000001;
    md_control.l1_part_16x16_enable = 0x00000001;
    md_control.l1_part_16x8_enable = 0x00000001;
    md_control.l1_part_8x16_enable = 0x00000001;
    md_control.l1_part_8x8_enable = 0x00000001;
    md_control.bi_part_16x16_enable = 0x00000001;
    md_control.bi_part_16x8_enable = 0x00000001;
    md_control.bi_part_8x16_enable = 0x00000001;
    md_control.bi_part_8x8_enable = 0x00000001;
    md_control.bdirect_mode = 0x00000001;
    md_control.bskip_enable = 0x00000001;
    md_control.pskip_enable = 0x00000001;
    md_control.intra_nxn_bias_multiplier = 0x0018;
    md_control.intra_most_prob_bias_multiplier = 0x0004;
    md_control.mv_cost_enable = 0x00000001;
    md_control.ip_search_mode = 0x00000007;

    msenc_h264_quant_control_s q_control;
    memset(&q_control, 0, sizeof(q_control));

    q_control.qpp_run_vector_4x4 = 0x056b;
    q_control.qpp_run_vector_8x8[0] = 0xaaff;
    q_control.qpp_run_vector_8x8[1] = 0xffaa;
    q_control.qpp_run_vector_8x8[2] = 0x000f;
    q_control.qpp_luma8x8_cost = 0x0f;
    q_control.qpp_luma16x16_cost = 0x0f;
    q_control.qpp_chroma_cost = 0x0f;
    q_control.dz_4x4_YI[0x0] = 0x03ff;
    q_control.dz_4x4_YI[0x1] = 0x036e;
    q_control.dz_4x4_YI[0x2] = 0x0334;
    q_control.dz_4x4_YI[0x3] = 0x02aa;
    q_control.dz_4x4_YI[0x4] = 0x036e;
    q_control.dz_4x4_YI[0x5] = 0x0334;
    q_control.dz_4x4_YI[0x6] = 0x02aa;
    q_control.dz_4x4_YI[0x7] = 0x0200;
    q_control.dz_4x4_YI[0x8] = 0x0334;
    q_control.dz_4x4_YI[0x9] = 0x02aa;
    q_control.dz_4x4_YI[0xa] = 0x0200;
    q_control.dz_4x4_YI[0xb] = 0x019a;
    q_control.dz_4x4_YI[0xc] = 0x02aa;
    q_control.dz_4x4_YI[0xd] = 0x0200;
    q_control.dz_4x4_YI[0xe] = 0x019a;
    q_control.dz_4x4_YI[0xf] = 0x019a;
    q_control.dz_4x4_YP[0x0] = 0x02aa;
    q_control.dz_4x4_YP[0x1] = 0x024a;
    q_control.dz_4x4_YP[0x2] = 0x0222;
    q_control.dz_4x4_YP[0x3] = 0x01c8;
    q_control.dz_4x4_YP[0x4] = 0x024a;
    q_control.dz_4x4_YP[0x5] = 0x0222;
    q_control.dz_4x4_YP[0x6] = 0x01c8;
    q_control.dz_4x4_YP[0x7] = 0x0156;
    q_control.dz_4x4_YP[0x8] = 0x0222;
    q_control.dz_4x4_YP[0x9] = 0x01c8;
    q_control.dz_4x4_YP[0xa] = 0x0156;
    q_control.dz_4x4_YP[0xb] = 0x0124;
    q_control.dz_4x4_YP[0xc] = 0x01c8;
    q_control.dz_4x4_YP[0xd] = 0x0156;
    q_control.dz_4x4_YP[0xe] = 0x0124;
    q_control.dz_4x4_YP[0xf] = 0x0112;
    q_control.dz_4x4_CI = 0x02aa;
    q_control.dz_4x4_CP = 0x0156;

    for (UINT32 idx = 0; idx <= 0xf; idx++)
    {
        q_control.dz_8x8_YI[idx] = 0x02aa;
        q_control.dz_8x8_YP[idx] = 0x0156;
    }

    msenc_h264_me_control_s me_control;
    memset(&me_control, 0, sizeof(me_control));

    me_control.refinement_mode = 0x00000001;
    me_control.refine_on_search_enable = 0x00000001;
    me_control.limit_mv.mv_limit_enable = 0x00000001;
    me_control.limit_mv.left_mvx_int = -2048; //0x800;
    me_control.limit_mv.top_mvy_int = -512; //0x200;
    me_control.limit_mv.right_mvx_frac = 0x00000003;
    me_control.limit_mv.right_mvx_int = 0x000007ff;
    me_control.limit_mv.bottom_mvy_frac = 0x00000003;
    me_control.limit_mv.bottom_mvy_int = 0x000001ff;
    me_control.predsrc.self_temporal_search = 0x00000001;
    me_control.predsrc.self_temporal_refine = 0x00000001;
    me_control.predsrc.self_temporal_enable = 0x00000001;
    me_control.predsrc.coloc_stamp_l0 = 0x00000002;
    me_control.predsrc.coloc_stamp_l1 = 0x00000002;
    me_control.predsrc.coloc_search = 0x00000001;
    me_control.predsrc.coloc_refine = 0x00000001;
    me_control.predsrc.coloc_enable = 0x00000001;
    me_control.predsrc.self_spatial_stamp_l0 = 0x00000001;
    me_control.predsrc.self_spatial_stamp_l1 = 0x00000001;
    me_control.predsrc.self_spatial_search = 0x00000001;
    me_control.predsrc.self_spatial_refine = 0x00000001;
    me_control.predsrc.self_spatial_enable = 0x00000001;
    me_control.predsrc.const_mv_search = 0x00000001;
    me_control.predsrc.const_mv_refine = 0x00000001;
    me_control.predsrc.const_mv_enable = 0x00000001;
    me_control.shape0.bitmask[0] = 0x7e7e7e7e;
    me_control.shape0.bitmask[1] = 0x7e7e7e7e;
    me_control.shape1.bitmask[0] = 0x7f3e1c08;
    me_control.shape1.bitmask[1] = 0x081c3e3e;
    me_control.shape2.bitmask[0] = 0xff7e3c18;
    me_control.shape2.bitmask[1] = 0x183c7eff;

    WritePicSetupSurface(0, PicSetupSurface, &PicSetup, &slice_control,
        &md_control, &q_control, &me_control);

    PicSetup.pps_data.pic_param_set_id = 1;

    PicSetup.pic_control.l0[0] = 0;

    for (UINT32 idx = 0; idx <= 7; idx++)
    {
        PicSetup.pic_control.temp_dist_l0[idx] = -1;
        PicSetup.pic_control.temp_dist_l1[idx] = -1;
    }
    PicSetup.pic_control.temp_dist_l0[0] = 0;

    for (UINT32 refdix1 = 0; refdix1 <= 7; refdix1++)
    {
        for (UINT32 refdix0 = 0; refdix0 <= 7; refdix0++)
        {
            PicSetup.pic_control.dist_scale_factor[refdix1][refdix0] = -1;
        }
    }
    PicSetup.pic_control.diff_pic_order_cnt_zero[0] = 0xfefefefe;
    PicSetup.pic_control.diff_pic_order_cnt_zero[1] = 0xfefefefe;

    PicSetup.pic_control.pic_type = 0x00000000;
    PicSetup.pic_control.base_view = 0,
    PicSetup.pic_control.view_id = 1,
    PicSetup.pic_control.inter_view_flag = 0;

    slice_control.qp_avr = 0x0000001c;
    slice_control.num_ref_idx_active_override_flag = 1;

    WritePicSetupSurface(1, PicSetupSurface, &PicSetup, &slice_control,
        &md_control, &q_control, &me_control);

    PicSetup.pps_data.pic_param_set_id = 0;

    PicSetup.pic_control.temp_dist_l0[0] = 3;
    for (UINT32 refdix1 = 0; refdix1 <= 7; refdix1++)
    {
        PicSetup.pic_control.dist_scale_factor[refdix1][0] = 0x0100;
    }

    PicSetup.pic_control.frame_num = 0x0001;
    PicSetup.pic_control.pic_order_cnt_lsb = 0x0006;
    PicSetup.pic_control.base_view = 1,
    PicSetup.pic_control.view_id = 0,
    PicSetup.pic_control.anchor_pic_flag = 0,
    PicSetup.pic_control.inter_view_flag = 1;

    WritePicSetupSurface(2, PicSetupSurface, &PicSetup, &slice_control,
        &md_control, &q_control, &me_control);

    PicSetup.pps_data.pic_param_set_id = 1;

    PicSetup.pic_control.l0[0] = 2;
    PicSetup.pic_control.l0[1] = 4;
    PicSetup.pic_control.temp_dist_l0[1] = 0;

    PicSetup.pic_control.diff_pic_order_cnt_zero[0] = 0xfcfcfcfc;
    PicSetup.pic_control.diff_pic_order_cnt_zero[1] = 0xfcfcfcfc;

    PicSetup.pic_control.base_view = 0,
    PicSetup.pic_control.view_id = 1,
    PicSetup.pic_control.inter_view_flag = 0;
    PicSetup.pic_control.lwr_interview_ref_pic = 4;
    PicSetup.pic_control.prev_interview_ref_pic = 0;

    slice_control.num_ref_idx_l0_active_minus1 = 1;

    WritePicSetupSurface(3, PicSetupSurface, &PicSetup, &slice_control,
        &md_control, &q_control, &me_control);

    PicSetup.pps_data.pic_param_set_id = 0;

    PicSetup.pic_control.l0[0] = 0;
    PicSetup.pic_control.l1[0] = 4;
    PicSetup.pic_control.temp_dist_l0[0] = 1;
    PicSetup.pic_control.temp_dist_l0[1] = -2;
    PicSetup.pic_control.temp_dist_l1[0] = -2;
    for (UINT32 refdix1 = 0; refdix1 <= 7; refdix1++)
    {
        for (UINT32 refdix0 = 0; refdix0 <= 7; refdix0++)
        {
            PicSetup.pic_control.dist_scale_factor[refdix1][refdix0] = -1;
        }
    }
    PicSetup.pic_control.dist_scale_factor[0][0] = 0x0055;

    PicSetup.pic_control.diff_pic_order_cnt_zero[0] = 0xfcfcfc02;
    PicSetup.pic_control.diff_pic_order_cnt_zero[1] = 0xfcfcfcfc;

    PicSetup.pic_control.frame_num = 0x0002;
    PicSetup.pic_control.pic_order_cnt_lsb = 0x0002;
    PicSetup.pic_control.pic_type = 0x00000001;
    PicSetup.pic_control.ref_pic_flag = 0,
    PicSetup.pic_control.base_view = 1,
    PicSetup.pic_control.view_id = 0,
    PicSetup.pic_control.lwr_interview_ref_pic = -2;
    PicSetup.pic_control.prev_interview_ref_pic = -2;

    slice_control.qp_avr = 0x0000001f;

    WritePicSetupSurface(4, PicSetupSurface, &PicSetup, &slice_control,
        &md_control, &q_control, &me_control);

    PicSetup.pps_data.pic_param_set_id = 1;

    PicSetup.pic_control.l0[0] = 2;
    PicSetup.pic_control.l0[1] = 6;
    PicSetup.pic_control.l1[0] = 6;

    PicSetup.pic_control.base_view = 0,
    PicSetup.pic_control.view_id = 1,

    WritePicSetupSurface(5, PicSetupSurface, &PicSetup, &slice_control,
        &md_control, &q_control, &me_control);

    PicSetup.pps_data.pic_param_set_id = 0;

    PicSetup.pic_control.l0[0] = 0;
    PicSetup.pic_control.l0[1] = 4;
    PicSetup.pic_control.l1[0] = 4;
    PicSetup.pic_control.temp_dist_l0[0] = 2;
    PicSetup.pic_control.temp_dist_l0[1] = -1;
    PicSetup.pic_control.temp_dist_l1[0] = -1;
    PicSetup.pic_control.dist_scale_factor[0][0] = 0x00ab;

    PicSetup.pic_control.pic_order_cnt_lsb = 0x0004;
    PicSetup.pic_control.base_view = 1,
    PicSetup.pic_control.view_id = 0,

    WritePicSetupSurface(6, PicSetupSurface, &PicSetup, &slice_control,
        &md_control, &q_control, &me_control);

    PicSetup.pps_data.pic_param_set_id = 1;

    PicSetup.pic_control.l0[0] = 2;
    PicSetup.pic_control.l0[1] = 6;
    PicSetup.pic_control.l1[0] = 6;

    PicSetup.pic_control.base_view = 0,
    PicSetup.pic_control.view_id = 1,

    WritePicSetupSurface(7, PicSetupSurface, &PicSetup, &slice_control,
        &md_control, &q_control, &me_control);

    return OK;
}

static RC PrepareControlStructuresH264HDHighprofileCABAC
(
    Surface2D *PicSetupSurface,
    Surface2D *IoRcProcess
)
{
    msenc_h264_drv_pic_setup_s PicSetup;
    memset(&PicSetup, 0, sizeof(PicSetup));

    PicSetup.magic = LW_MSENC_DRV_MAGIC_VALUE;
    PicSetup.refpic_cfg.frame_width_minus1  = 1920-1;
    PicSetup.refpic_cfg.frame_height_minus1 = 1088-1;
    PicSetup.refpic_cfg.sfc_pitch           = 1920;
    PicSetup.refpic_cfg.chroma_top_frm_offset = 8160;

    PicSetup.input_cfg.frame_width_minus1  = 1920-1;
    PicSetup.input_cfg.frame_height_minus1 = 1088-1;
    PicSetup.input_cfg.sfc_pitch           = 1920;
    PicSetup.input_cfg.chroma_top_frm_offset = 8160;
    PicSetup.input_cfg.block_height = 2;

    PicSetup.outputpic_cfg.frame_width_minus1  = 1920-1;
    PicSetup.outputpic_cfg.frame_height_minus1 = 1088-1;
    PicSetup.outputpic_cfg.sfc_pitch           = 1920;
    PicSetup.outputpic_cfg.chroma_top_frm_offset = 8160;
    PicSetup.outputpic_cfg.chroma_bot_offset     = 8160;
    PicSetup.outputpic_cfg.tiled_16x16 = 1;

    PicSetup.sps_data.profile_idc = 0x00000064;
    PicSetup.sps_data.level_idc = 0x00000029;
    PicSetup.sps_data.chroma_format_idc = 0x00000001;
    PicSetup.sps_data.log2_max_frame_num_minus4 = 0x00000004;
    PicSetup.sps_data.log2_max_pic_order_cnt_lsb_minus4 = 0x00000004;

    PicSetup.pps_data.entropy_coding_mode_flag = 1;
    PicSetup.pps_data.num_ref_idx_l0_active_minus1 = 2;
    PicSetup.pps_data.deblocking_filter_control_present_flag = 1;
    PicSetup.pps_data.transform_8x8_mode_flag = 1;

    PicSetup.rate_control.hrd_type = 0x02;
    PicSetup.rate_control.QP[0] = 0x2b;
    PicSetup.rate_control.QP[1] = 0x2e;
    PicSetup.rate_control.QP[2] = 0x28;
    PicSetup.rate_control.maxQP[0] = 0x33;
    PicSetup.rate_control.maxQP[1] = 0x33;
    PicSetup.rate_control.maxQP[2] = 0x33;
    PicSetup.rate_control.maxQPD = 0x06;
    PicSetup.rate_control.baseQPD = 0x03;
    PicSetup.rate_control.rhopbi[0] = 0x00000113;
    PicSetup.rate_control.rhopbi[1] = 0x00000126;
    PicSetup.rate_control.rhopbi[2] = 0x00000100;
    PicSetup.rate_control.framerate = 0x00001d00;
    PicSetup.rate_control.buffersize = 0x003e8000;
    PicSetup.rate_control.nal_cpb_size = 0x003e8000;
    PicSetup.rate_control.nal_bitrate = 0x001f4000;
    PicSetup.rate_control.gop_length = 0x0000000f;
    PicSetup.rate_control.Np = 4;
    PicSetup.rate_control.Bmin = 0x00000064;
    PicSetup.rate_control.Ravg = 0x00000008;
    PicSetup.rate_control.R = 0x00000008;

    for (UINT32 idx = 0; idx <= 7; idx++)
    {
        PicSetup.pic_control.l0[idx] = -2;
        PicSetup.pic_control.l1[idx] = -2;
    }
    PicSetup.pic_control.slice_control_offset = SliceControlOffset;
    PicSetup.pic_control.md_control_offset    = MdControlOffset;
    PicSetup.pic_control.q_control_offset     = QuantControlOffset;
    PicSetup.pic_control.me_control_offset    = MeControlOffset;
    PicSetup.pic_control.hist_buf_size = 0x00102c00;
    PicSetup.pic_control.bitstream_buf_size = OutBitstreamBufferSize;
    PicSetup.pic_control.pic_type = 0x00000003;
    PicSetup.pic_control.ref_pic_flag = 1;

    msenc_h264_slice_control_s slice_control;
    memset(&slice_control, 0, sizeof(slice_control));

    slice_control.num_mb = 0x00001fe0;
    slice_control.qp_avr = 0x00000026;

    msenc_h264_md_control_s md_control;
    memset(&md_control, 0, sizeof(md_control));

    md_control.intra_luma4x4_mode_enable = 0x000001ff;
    md_control.intra_luma8x8_mode_enable = 0x000001ff;
    md_control.intra_luma16x16_mode_enable = 0x0000000f;
    md_control.intra_chroma_mode_enable = 0x0000000f;
    md_control.l0_part_16x16_enable = 0x00000001;
    md_control.l0_part_16x8_enable = 0x00000001;
    md_control.l0_part_8x16_enable = 0x00000001;
    md_control.l0_part_8x8_enable = 0x00000001;
    md_control.l1_part_16x16_enable = 0x00000001;
    md_control.l1_part_16x8_enable = 0x00000001;
    md_control.l1_part_8x16_enable = 0x00000001;
    md_control.l1_part_8x8_enable = 0x00000001;
    md_control.bi_part_16x16_enable = 0x00000001;
    md_control.bi_part_16x8_enable = 0x00000001;
    md_control.bi_part_8x16_enable = 0x00000001;
    md_control.bi_part_8x8_enable = 0x00000001;
    md_control.bdirect_mode = 0x00000002;
    md_control.bskip_enable = 0x00000001;
    md_control.pskip_enable = 0x00000001;
    md_control.intra_nxn_bias_multiplier = 0x0018;
    md_control.intra_most_prob_bias_multiplier = 0x0004;
    md_control.mv_cost_enable = 0x00000001;
    md_control.early_intra_mode_control = 0x00000001;
    md_control.early_intra_mode_type_16x16dc = 0x00000001;
    md_control.early_intra_mode_type_16x16h = 0x00000001;
    md_control.early_intra_mode_type_16x16v = 0x00000001;
    md_control.ip_search_mode = 0x00000007;

    msenc_h264_quant_control_s q_control;
    memset(&q_control, 0, sizeof(q_control));

    q_control.qpp_run_vector_4x4 = 0x056b;
    q_control.qpp_run_vector_8x8[0] = 0xaaff;
    q_control.qpp_run_vector_8x8[1] = 0xffaa;
    q_control.qpp_run_vector_8x8[2] = 0x000f;
    q_control.qpp_luma8x8_cost = 0x0f;
    q_control.qpp_luma16x16_cost = 0x0f;
    q_control.qpp_chroma_cost = 0x0f;
    q_control.dz_4x4_YI[0x0] = 0x03ff;
    q_control.dz_4x4_YI[0x1] = 0x036e;
    q_control.dz_4x4_YI[0x2] = 0x0334;
    q_control.dz_4x4_YI[0x3] = 0x02aa;
    q_control.dz_4x4_YI[0x4] = 0x036e;
    q_control.dz_4x4_YI[0x5] = 0x0334;
    q_control.dz_4x4_YI[0x6] = 0x02aa;
    q_control.dz_4x4_YI[0x7] = 0x0200;
    q_control.dz_4x4_YI[0x8] = 0x0334;
    q_control.dz_4x4_YI[0x9] = 0x02aa;
    q_control.dz_4x4_YI[0xa] = 0x0200;
    q_control.dz_4x4_YI[0xb] = 0x019a;
    q_control.dz_4x4_YI[0xc] = 0x02aa;
    q_control.dz_4x4_YI[0xd] = 0x0200;
    q_control.dz_4x4_YI[0xe] = 0x019a;
    q_control.dz_4x4_YI[0xf] = 0x019a;
    q_control.dz_4x4_YP[0x0] = 0x02aa;
    q_control.dz_4x4_YP[0x1] = 0x024a;
    q_control.dz_4x4_YP[0x2] = 0x0222;
    q_control.dz_4x4_YP[0x3] = 0x01c8;
    q_control.dz_4x4_YP[0x4] = 0x024a;
    q_control.dz_4x4_YP[0x5] = 0x0222;
    q_control.dz_4x4_YP[0x6] = 0x01c8;
    q_control.dz_4x4_YP[0x7] = 0x0156;
    q_control.dz_4x4_YP[0x8] = 0x0222;
    q_control.dz_4x4_YP[0x9] = 0x01c8;
    q_control.dz_4x4_YP[0xa] = 0x0156;
    q_control.dz_4x4_YP[0xb] = 0x0124;
    q_control.dz_4x4_YP[0xc] = 0x01c8;
    q_control.dz_4x4_YP[0xd] = 0x0156;
    q_control.dz_4x4_YP[0xe] = 0x0124;
    q_control.dz_4x4_YP[0xf] = 0x0112;
    q_control.dz_4x4_CI = 0x02aa;
    q_control.dz_4x4_CP = 0x0156;

    for (UINT32 idx = 0; idx <= 0xf; idx++)
    {
        q_control.dz_8x8_YI[idx] = 0x02aa;
        q_control.dz_8x8_YP[idx] = 0x0156;
    }

    msenc_h264_me_control_s me_control;
    memset(&me_control, 0, sizeof(me_control));

    me_control.refinement_mode = 0x00000001;
    me_control.refine_on_search_enable = 0x00000001;
    me_control.limit_mv.mv_limit_enable = 0x00000001;
    me_control.limit_mv.left_mvx_int = -2048;  //0x800;
    me_control.limit_mv.top_mvy_int = -512;  //0x200;
    me_control.limit_mv.right_mvx_frac = 0x00000003;
    me_control.limit_mv.right_mvx_int = 0x000007ff;
    me_control.limit_mv.bottom_mvy_frac = 0x00000003;
    me_control.limit_mv.bottom_mvy_int = 0x000001ff;
    me_control.predsrc.coloc_stamp_l0 = 0x00000001;
    me_control.predsrc.coloc_stamp_l1 = 0x00000001;
    me_control.predsrc.coloc_search = 0x00000001;
    me_control.predsrc.coloc_refine = 0x00000001;
    me_control.predsrc.coloc_enable = 0x00000001;
    me_control.predsrc.self_spatial_search = 0x00000001;
    me_control.predsrc.self_spatial_refine = 0x00000001;
    me_control.predsrc.self_spatial_enable = 0x00000001;
    me_control.predsrc.const_mv_stamp_l0 = 0x00000002;
    me_control.predsrc.const_mv_stamp_l1 = 0x00000002;
    me_control.predsrc.const_mv_search = 0x00000001;
    me_control.predsrc.const_mv_refine = 0x00000001;
    me_control.predsrc.const_mv_enable = 0x00000001;
    me_control.shape0.bitmask[0] = 0x3c3c0000;
    me_control.shape0.bitmask[1] = 0x00003c3c;
    me_control.shape1.bitmask[0] = 0x3e1c0800;
    me_control.shape1.bitmask[1] = 0x0000081c;
    me_control.shape2.bitmask[0] = 0x3c3c1800;
    me_control.shape2.bitmask[1] = 0x0000183c;

    WritePicSetupSurface(0, PicSetupSurface, &PicSetup, &slice_control,
        &md_control, &q_control, &me_control);

    PicSetup.pic_control.l0[0] = 0;

    for (UINT32 idx = 0; idx <= 7; idx++)
    {
        PicSetup.pic_control.temp_dist_l0[idx] = -1;
        PicSetup.pic_control.temp_dist_l1[idx] = -1;
    }
    PicSetup.pic_control.temp_dist_l0[0] = 3;

    for (UINT32 refdix1 = 0; refdix1 <= 7; refdix1++)
    {
        for (UINT32 refdix0 = 0; refdix0 <= 7; refdix0++)
        {
            PicSetup.pic_control.dist_scale_factor[refdix1][refdix0] =
                (refdix0 == 0) ? 0x0100 : 0xffff;
        }
    }
    PicSetup.pic_control.diff_pic_order_cnt_zero[0] = 0xfefefefe;
    PicSetup.pic_control.diff_pic_order_cnt_zero[1] = 0xfefefefe;

    PicSetup.pic_control.frame_num = 0x0001;
    PicSetup.pic_control.pic_order_cnt_lsb = 0x0006;
    PicSetup.pic_control.pic_type = 0x00000000;
    PicSetup.pic_control.ref_pic_flag = 0x00000001;

    slice_control.qp_avr = 0x00000029;
    slice_control.num_ref_idx_active_override_flag = 1;

    WritePicSetupSurface(1, PicSetupSurface, &PicSetup, &slice_control,
        &md_control, &q_control, &me_control);

    PicSetup.pic_control.l0[1] = 2;
    PicSetup.pic_control.l1[0] = 2;
    PicSetup.pic_control.temp_dist_l0[0] = 1;
    PicSetup.pic_control.temp_dist_l0[1] = -2;
    PicSetup.pic_control.temp_dist_l1[0] = -2;
    for (UINT32 refdix1 = 1; refdix1 <= 7; refdix1++)
    {
        PicSetup.pic_control.dist_scale_factor[refdix1][0] = -1;
    }
    PicSetup.pic_control.dist_scale_factor[0][0] = 0x0055;

    PicSetup.pic_control.diff_pic_order_cnt_zero[0] = 0xfcfcfc02;
    PicSetup.pic_control.diff_pic_order_cnt_zero[1] = 0xfcfcfcfc;

    PicSetup.pic_control.frame_num = 0x0002;
    PicSetup.pic_control.pic_order_cnt_lsb = 0x0002;
    PicSetup.pic_control.pic_type = 0x00000001;
    PicSetup.pic_control.ref_pic_flag = 0x00000000;

    slice_control.qp_avr = 0x0000002b;
    slice_control.num_ref_idx_l0_active_minus1 = 1;

    WritePicSetupSurface(2, PicSetupSurface, &PicSetup, &slice_control,
        &md_control, &q_control, &me_control);

    PicSetup.pic_control.temp_dist_l0[0] = 2;
    PicSetup.pic_control.temp_dist_l0[1] = -1;
    PicSetup.pic_control.temp_dist_l1[0] = -1;
    PicSetup.pic_control.dist_scale_factor[0][0] = 0x00ab;

    PicSetup.pic_control.pic_order_cnt_lsb = 0x0004;

    WritePicSetupSurface(3, PicSetupSurface, &PicSetup, &slice_control,
        &md_control, &q_control, &me_control);

    msenc_persistent_state_s RcProcess;
    memset(&RcProcess, 0, sizeof(RcProcess));

    RcProcess.nal_cpb_fullness = 0x00c80000;
    RcProcess.Wpbi[0] = 0x00000017;
    RcProcess.Wpbi[1] = 0x00000018;
    RcProcess.Wpbi[2] = 0x00000015;
    for (UINT32 idx = 0; idx < 3; idx++)
    {
        RcProcess.Whist[0][idx] = 0x00000017;
        RcProcess.Whist[1][idx] = 0x00000018;
        RcProcess.Whist[2][idx] = 0x00000015;
    }
    RcProcess.np = 0x00000040;
    RcProcess.nb = 0x000000a0;
    RcProcess.Bavg = 0x000006c9;

    Platform::VirtualWr(IoRcProcess->GetAddress(), &RcProcess, sizeof(RcProcess));

    return OK;
}

RC MSENCTest::Setup()
{
#if !defined(DISABLE_VERIF_FEATURES) && !defined(LW_MACINTOSH_OSX)
    // Please make sure that you use #include "90b7_110.h" in
    //   .../drivers/resman/kernel/msenc/v01/90b7_110.c
    // and locally disable this check.
    // This test is not supporting the arch ucode anymore.
    MASSERT(!"Error: MSENC test can be only run with production ucode!");
    Printf(Tee::PriError, "Error: MSENC test can be only run with production ucode!\n");
    return RC::SOFTWARE_ERROR;
#else

    RC rc;

    CHECK_RC(GpuTest::Setup());

    CHECK_RC(m_pTestConfig->AllocateChannel(&m_pCh, &m_hCh));

    CHECK_RC(m_MSENCAlloc.Alloc(m_hCh, GetBoundGpuDevice()));

    UINT32 subdev = GetBoundGpuSubdevice()->GetSubdeviceInst();

    m_Semaphore.SetWidth(0x10);
    m_Semaphore.SetHeight(1);
    m_Semaphore.SetColorFormat(ColorUtils::VOID32);
    m_Semaphore.SetLocation(m_pTestConfig->NotifierLocation());
    CHECK_RC(m_Semaphore.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_Semaphore.Fill(0, subdev));
    CHECK_RC(m_Semaphore.Map());
    CHECK_RC(m_Semaphore.BindGpuChannel(m_hCh));

    Memory::Location ProcessingLocation = Memory::Optimal;
    if (m_AllocateInSysmem)
        ProcessingLocation = Memory::Coherent;

    m_EncodeStatus.SetWidth(sizeof(msenc_stat_data_s)/4);
    m_EncodeStatus.SetHeight(1);
    m_EncodeStatus.SetColorFormat(ColorUtils::VOID32);
    m_EncodeStatus.SetAlignment(256);
    m_EncodeStatus.SetLocation(ProcessingLocation);
    CHECK_RC(m_EncodeStatus.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_EncodeStatus.Map());
    CHECK_RC(m_EncodeStatus.BindGpuChannel(m_hCh));

    m_IoRcProcess.SetWidth(sizeof(msenc_persistent_state_s)/4);
    m_IoRcProcess.SetHeight(1);
    m_IoRcProcess.SetColorFormat(ColorUtils::VOID32);
    m_IoRcProcess.SetAlignment(256);
    m_IoRcProcess.SetLocation(ProcessingLocation);
    CHECK_RC(m_IoRcProcess.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_IoRcProcess.Map());
    CHECK_RC(m_IoRcProcess.BindGpuChannel(m_hCh));

    for (UINT32 RefPicIdx = 0;
         RefPicIdx < sizeof(m_YUVRefPic)/sizeof(m_YUVRefPic[0]);
         RefPicIdx++)
    {
        // This is the size that H264_HD_HP_CABAC_MS stream needs.
        // All other streams will use a smaller part of this allocation.
        m_YUVRefPic[RefPicIdx].SetWidth(1920/4);
        m_YUVRefPic[RefPicIdx].SetHeight(1632); // Luma up to line 1087
                                                // Chroma at lines >= 1088
        m_YUVRefPic[RefPicIdx].SetColorFormat(ColorUtils::A8R8G8B8);
        m_YUVRefPic[RefPicIdx].SetAlignment(256);
        m_YUVRefPic[RefPicIdx].SetType(LWOS32_TYPE_VIDEO);
        m_YUVRefPic[RefPicIdx].SetLocation(ProcessingLocation);
        CHECK_RC(m_YUVRefPic[RefPicIdx].Alloc(GetBoundGpuDevice()));
        CHECK_RC(m_YUVRefPic[RefPicIdx].BindGpuChannel(m_hCh));
    }

    m_IoHistory.SetWidth(1059840/4); // MAX(PicSetup.pic_control.hist_buf_size)
                                     //         from all streams
    m_IoHistory.SetHeight(1);
    m_IoHistory.SetColorFormat(ColorUtils::VOID32);
    m_IoHistory.SetAlignment(256);
    m_IoHistory.SetLocation(ProcessingLocation);
    CHECK_RC(m_IoHistory.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_IoHistory.Fill(0, subdev));
    CHECK_RC(m_IoHistory.BindGpuChannel(m_hCh));

    m_OutBitstream.SetWidth(256/4);  // Size should be big enough to fit output
    m_OutBitstream.SetHeight(OutBitstreamBufferSize/256); // from each frame
    m_OutBitstream.SetColorFormat(ColorUtils::A8R8G8B8);
    m_OutBitstream.SetAlignment(256);
    m_OutBitstream.SetLocation(ProcessingLocation);

    CHECK_RC(m_OutBitstream.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_OutBitstream.Map());
    CHECK_RC(m_OutBitstream.BindGpuChannel(m_hCh));

    for (UINT32 ColocIdx = 0;
         ColocIdx < sizeof(m_ColocData)/sizeof(m_ColocData[0]);
         ColocIdx++)
    {
        m_ColocData[ColocIdx].SetWidth(522240/4); // Based on arch traces
        m_ColocData[ColocIdx].SetHeight(1);
        m_ColocData[ColocIdx].SetColorFormat(ColorUtils::VOID32);
        m_ColocData[ColocIdx].SetAlignment(256);
        m_ColocData[ColocIdx].SetLocation(ProcessingLocation);
        CHECK_RC(m_ColocData[ColocIdx].Alloc(GetBoundGpuDevice()));
        CHECK_RC(m_ColocData[ColocIdx].BindGpuChannel(m_hCh));
    }

    for (UINT32 MeIdx = 0;
         MeIdx < sizeof(m_MepredData)/sizeof(m_MepredData[0]);
         MeIdx++)
    {
        m_MepredData[MeIdx].SetWidth(33120/4); // Based on arch traces
        m_MepredData[MeIdx].SetHeight(1);
        m_MepredData[MeIdx].SetColorFormat(ColorUtils::VOID32);
        m_MepredData[MeIdx].SetAlignment(256);
        m_MepredData[MeIdx].SetLocation(ProcessingLocation);
        CHECK_RC(m_MepredData[MeIdx].Alloc(GetBoundGpuDevice()));
        CHECK_RC(m_MepredData[MeIdx].BindGpuChannel(m_hCh));
    }

    m_PicSetup.SetWidth(MAX_NUM_FRAMES*AllControlStructsSize/4);
    m_PicSetup.SetHeight(1);
    m_PicSetup.SetColorFormat(ColorUtils::VOID32);
    m_PicSetup.SetAlignment(256);
    m_PicSetup.SetLocation(ProcessingLocation);
    CHECK_RC(m_PicSetup.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_PicSetup.Map());
    CHECK_RC(m_PicSetup.BindGpuChannel(m_hCh));

    m_pGGSurfs = new GpuGoldenSurfaces(GetBoundGpuDevice());
    m_pGGSurfs->AttachSurface(&m_OutBitstream, "C", 0);
    CHECK_RC(GetGoldelwalues()->SetSurfaces(m_pGGSurfs));
    m_pGolden->SetCheckCRCsUnique(true);
    GetGoldelwalues()->SetPrintCallback(&PrintCallback, this);

    return rc;
#endif
}

RC MSENCTest::Cleanup()
{
    StickyRC FirstRC;

    m_MSENCAlloc.Free();

    if (m_pCh)
    {
        FirstRC = m_pTestConfig->FreeChannel(m_pCh);
        m_pCh = 0;
    }

    m_Semaphore.Free();
    m_EncodeStatus.Free();
    m_IoRcProcess.Free();
    for (UINT32 RefPicIdx = 0;
         RefPicIdx < sizeof(m_YUVRefPic)/sizeof(m_YUVRefPic[0]);
         RefPicIdx++)
    {
        m_YUVRefPic[RefPicIdx].Free();
    }
    m_IoHistory.Free();
    m_OutBitstream.Free();
    for (UINT32 ColocIdx = 0;
         ColocIdx < sizeof(m_ColocData)/sizeof(m_ColocData[0]);
         ColocIdx++)
    {
        m_ColocData[ColocIdx].Free();
    }
    for (UINT32 MeIdx = 0;
         MeIdx < sizeof(m_MepredData)/sizeof(m_MepredData[0]);
         MeIdx++)
    {
        m_MepredData[MeIdx].Free();
    }
    m_PicSetup.Free();
    m_YUVInput.Free();

    m_pGolden->ClearSurfaces();
    delete m_pGGSurfs;
    m_pGGSurfs = NULL;

    FirstRC = GpuTest::Cleanup();

    return FirstRC;
}

static void SaveSurface
(
    Surface2D *Surface,
    const char *SurfaceName,
    UINT32 StreamIndex,
    UINT32 FrameIdx
)
{
    if (Surface->GetSize() == 0)
        return;

    char Filename[32];
    UINT08 Buffer[4096];
    sprintf(Filename, "S%dF%d%s.bin", StreamIndex, FrameIdx, SurfaceName);
    FileHolder File(Filename, "wb");
    if (File.GetFile() == 0)
    {
        Printf(Tee::PriHigh, "Error: Unable to open file %s for write!\n",
            Filename);
        return;
    }
    Surface2D::MappingSaver oldMapping(*Surface);
    if (!Surface->IsMapped() && OK != Surface->Map())
    {
        Printf(Tee::PriHigh,
            "Error: Unable to dump SurfaceName surface - unable to map the surface!\n");
        return;
    }
    UINT32 Size = (UINT32)Surface->GetSize();
    for (UINT32 Offset = 0; Offset < Size; Offset+=sizeof(Buffer))
    {
        UINT32 TransferSize = (Size - Offset) < sizeof(Buffer) ?
            (Size - Offset) : sizeof(Buffer);\
            Platform::VirtualRd((UINT08*)Surface->GetAddress() + Offset,
                Buffer, TransferSize);
        if (TransferSize != fwrite(Buffer, 1, TransferSize, File.GetFile()))
        {
            Printf(Tee::PriHigh, "Error: Unable to save the surface!\n");
            return;
        }
    }
}

void MSENCTest::SaveSurfaces(UINT32 StreamIndex, UINT32 FrameIdx)
{
    if (!m_SaveSurfaces)
        return;
    SaveSurface(&m_YUVInput, "YUVInput", StreamIndex, FrameIdx);
    SaveSurface(&m_EncodeStatus, "EncodeStatus", StreamIndex, FrameIdx);
    SaveSurface(&m_IoRcProcess, "IoRcProcess", StreamIndex, FrameIdx);
    SaveSurface(&m_YUVRefPic[0], "YUVRefPic0", StreamIndex, FrameIdx);
    SaveSurface(&m_YUVRefPic[1], "YUVRefPic1", StreamIndex, FrameIdx);
    SaveSurface(&m_YUVRefPic[2], "YUVRefPic2", StreamIndex, FrameIdx);
    SaveSurface(&m_IoHistory, "IoHistory", StreamIndex, FrameIdx);
    SaveSurface(&m_ColocData[0], "ColocData0", StreamIndex, FrameIdx);
    SaveSurface(&m_ColocData[1], "ColocData1", StreamIndex, FrameIdx);
    SaveSurface(&m_MepredData[0], "MepredData0", StreamIndex, FrameIdx);
    SaveSurface(&m_MepredData[1], "MepredData1", StreamIndex, FrameIdx);
    SaveSurface(&m_PicSetup, "PicSetup", StreamIndex, FrameIdx);
    SaveSurface(&m_OutBitstream, "OutBitstream", StreamIndex, FrameIdx);
}

RC MSENCTest::FillYUVInput(UINT32 StreamIndex, UINT32 FrameIdx)
{
    RC rc;

    static const UINT32 InputSurfacePitch[NUM_STREAMS]
        = { 192, 192, 192, 1920 };
    static const UINT32 InputSurfaceHeight[NUM_STREAMS]
        = { 224, 128, 224, 1632 };
    static const UINT32 ChromaStartLine[NUM_STREAMS]
        = { 144,  80, 144, 1088 };

    if (FrameIdx == 0)
    {
        m_YUVInput.Free();
        m_YUVInput.SetWidth(InputSurfacePitch[StreamIndex]);
        m_YUVInput.SetHeight(InputSurfaceHeight[StreamIndex]);
        m_YUVInput.SetColorFormat(ColorUtils::Y8);
        m_YUVInput.SetAlignment(256);
        m_YUVInput.SetLayout(Surface2D::Pitch);
        m_YUVInput.SetType(LWOS32_TYPE_VIDEO);
        m_YUVInput.SetLocation(Memory::Coherent);
        CHECK_RC(m_YUVInput.Alloc(GetBoundGpuDevice()));
        CHECK_RC(m_YUVInput.Map());
        CHECK_RC(m_YUVInput.BindGpuChannel(m_hCh));
    }

    if (m_YUVInputFromFiles)
    {
        UINT32 SurfaceSize = InputSurfacePitch[StreamIndex] *
                             InputSurfaceHeight[StreamIndex];

        std::vector<UINT32> Buf(SurfaceSize);
        char Filename[32];
        sprintf(Filename, "FS%dF%dYUVInput.bin", StreamIndex, FrameIdx);
        FileHolder File(Filename, "rb");
        if (File.GetFile() == 0)
            return RC::CANNOT_OPEN_FILE;

        if (SurfaceSize != fread(&Buf[0], 1, SurfaceSize, File.GetFile()))
            return RC::FILE_READ_ERROR;
        Platform::VirtualWr(m_YUVInput.GetAddress(), &Buf[0], SurfaceSize);
        return rc;
    }

    UINT32 subdev = GetBoundGpuSubdevice()->GetSubdeviceInst();

    // By default creating input images synthetically.
    //   Background filled with static value the same in each frame,
    //      but changing from stream to stream for increased coverage.
    //   For now two also writing two rectangles moving from frame to frame.
    //      First rectangle is in luma section, second in chroma.
    //      Content of rectangle is random, but identical from frame to frame
    //         (for motion detection).

    CHECK_RC(m_YUVInput.Fill( (StreamIndex & 1) ? 0x55AA55AA : 0xAA55AA55,
        subdev));

    CHECK_RC(m_YUVInput.FillPatternRect(
        9+FrameIdx,  // RectX
        14+FrameIdx, // RectY
        10,          // RectWidth
        15,          // RectHeight
        1,           // ChunkWidth
        1,           // ChunkHeight
        "random",
        "seed=0",
        "",
        subdev));

    CHECK_RC(m_YUVInput.FillPatternRect(
        39-FrameIdx, // RectX
        ChromaStartLine[StreamIndex]+24+FrameIdx, // RectY
        11,          // RectWidth
        16,          // RectHeight
        1,           // ChunkWidth
        1,           // ChunkHeight
        "random",
        "seed=0",
        "",
        subdev));

    return OK;
}

RC MSENCTest::RunFrame(UINT32 StreamIndex, UINT32 FrameIdx)
{
    RC rc;
    UINT32 subdev = GetBoundGpuSubdevice()->GetSubdeviceInst();
    UINT64 SemaphoreOffset = m_Semaphore.GetCtxDmaOffsetGpu();
    UINT32 *Semaphore = (UINT32 *)m_Semaphore.GetAddress();

    m_EncodeStatus.Fill(0, subdev);
    m_OutBitstream.Fill(0x55555555, subdev);

    CHECK_RC(m_pCh->Write(s_ENC, LW90B7_SET_APPLICATION_ID,
        DRF_DEF(90B7, _SET_APPLICATION_ID, _ID, _MSENC_H264)));

    UINT32 RcMode = 0;
    switch (StreamIndex)
    {
        case H264_BASELINE_CAVLC:
        case H264_MVC_HP_CABAC:
        case H264_HD_HP_CABAC_MS:
            break;
        case H264_HP_CABAC:
            RcMode = 0x12;
            break;
        default:
            return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(m_pCh->Write(s_ENC, LW90B7_SET_CONTROL_PARAMS,
        DRF_DEF(90B7, _SET_CONTROL_PARAMS, _CODEC_TYPE, _H264) |
        DRF_NUM(90B7, _SET_CONTROL_PARAMS, _FORCE_OUT_PIC, 1) |
        DRF_NUM(90B7, _SET_CONTROL_PARAMS, _RCMODE, RcMode)));

    CHECK_RC(m_pCh->Write(s_ENC, LW90B7_SET_PICTURE_INDEX,
        DRF_NUM(90B7, _SET_PICTURE_INDEX, _INDEX, FrameIdx)));

    CHECK_RC(m_pCh->Write(s_ENC, LW90B7_SET_IN_LWR_PIC,
        DRF_NUM(90B7, _SET_IN_LWR_PIC, _OFFSET,
        UINT32(m_YUVInput.GetCtxDmaOffsetGpu()/256))));

    CHECK_RC(m_pCh->Write(s_ENC, LW90B7_SET_OUT_ENC_STATUS,
        DRF_NUM(90B7, _SET_OUT_ENC_STATUS, _OFFSET,
        UINT32(m_EncodeStatus.GetCtxDmaOffsetGpu()/256))));

    CHECK_RC(m_pCh->Write(s_ENC, LW90B7_SET_IO_RC_PROCESS,
        DRF_NUM(90B7, _SET_IO_RC_PROCESS, _OFFSET,
        UINT32(m_IoRcProcess.GetCtxDmaOffsetGpu()/256))));

    UINT32 OutSurfaceIdx =
        StreamDescriptions[StreamIndex].OutPictureIndex[FrameIdx];

    CHECK_RC(m_pCh->Write(s_ENC, LW90B7_SET_OUT_REF_PIC,
        DRF_NUM(90B7, _SET_OUT_REF_PIC, _OFFSET,
        UINT32(m_YUVRefPic[OutSurfaceIdx].GetCtxDmaOffsetGpu()/256))));

    CHECK_RC(m_pCh->Write(s_ENC, LW90B7_SET_IOHISTORY,
        DRF_NUM(90B7, _SET_IOHISTORY, _OFFSET,
        UINT32(m_IoHistory.GetCtxDmaOffsetGpu()/256))));

    CHECK_RC(m_pCh->Write(s_ENC, LW90B7_SET_OUT_BITSTREAM,
        DRF_NUM(90B7, _SET_OUT_BITSTREAM, _OFFSET,
        UINT32(m_OutBitstream.GetCtxDmaOffsetGpu()/256))));

    INT32 InColocIdx = ColocInOutIndexes[StreamIndex][FrameIdx][0];
    UINT32 InColocOffset = (InColocIdx == -1) ?
        0x00000000 :
        UINT32(m_ColocData[InColocIdx].GetCtxDmaOffsetGpu()/256);
    CHECK_RC(m_pCh->Write(s_ENC, LW90B7_SET_IN_COLOC_DATA,
        DRF_NUM(90B7, _SET_IN_COLOC_DATA, _OFFSET, InColocOffset)));

    INT32 OutColocIdx = ColocInOutIndexes[StreamIndex][FrameIdx][1];
    UINT32 OutColocOffset = (OutColocIdx == -1) ?
        0x00000000 :
        UINT32(m_ColocData[OutColocIdx].GetCtxDmaOffsetGpu()/256);
    CHECK_RC(m_pCh->Write(s_ENC, LW90B7_SET_OUT_COLOC_DATA,
        DRF_NUM(90B7, _SET_OUT_COLOC_DATA, _OFFSET, OutColocOffset)));

    INT32 InMeIdx = MeInOutIndexes[StreamIndex][FrameIdx][0];
    UINT32 InMeOffset = (InMeIdx == -1) ?
        0x00000000 :
        UINT32(m_MepredData[InMeIdx].GetCtxDmaOffsetGpu()/256);
    CHECK_RC(m_pCh->Write(s_ENC, LW90B7_SET_IN_MEPRED_DATA,
        DRF_NUM(90B7, _SET_IN_MEPRED_DATA, _OFFSET, InMeOffset)));

    INT32 OutMeIdx = MeInOutIndexes[StreamIndex][FrameIdx][1];
    UINT32 OutMeOffset = (OutMeIdx == -1) ?
        0x00000000 :
        UINT32(m_MepredData[OutMeIdx].GetCtxDmaOffsetGpu()/256);
    CHECK_RC(m_pCh->Write(s_ENC, LW90B7_SET_OUT_MEPRED_DATA,
        DRF_NUM(90B7, _SET_OUT_MEPRED_DATA, _OFFSET, OutMeOffset)));

    for (UINT32 RefPicIdx = 0; RefPicIdx < MAX_REF_FRAMES; RefPicIdx++)
    {
        INT32 InSurfaceIdx =
            RefPictureIndexes[StreamIndex][FrameIdx][RefPicIdx];

        UINT32 InSurfaceOffset = (InSurfaceIdx == -1) ?
            0x00000000 :
            UINT32(m_YUVRefPic[InSurfaceIdx].GetCtxDmaOffsetGpu()/256);

        CHECK_RC(m_pCh->Write(s_ENC, LW90B7_SET_IN_REF_PIC0 + RefPicIdx*4,
            DRF_NUM(90B7, _SET_IN_REF_PIC0, _OFFSET, InSurfaceOffset)));
    }

    CHECK_RC(m_pCh->Write(s_ENC, LW90B7_SET_IN_DRV_PIC_SETUP,
        DRF_NUM(90B7, _SET_IN_DRV_PIC_SETUP, _OFFSET,
        UINT32( (m_PicSetup.GetCtxDmaOffsetGpu() +
                 FrameIdx*AllControlStructsSize)/256) )));

    CHECK_RC(m_pCh->Write(s_ENC, LW90B7_SEMAPHORE_A,
        DRF_NUM(90B7, _SEMAPHORE_A, _UPPER,
            0xFF & LwU64_HI32(SemaphoreOffset))));
    CHECK_RC(m_pCh->Write(s_ENC, LW90B7_SEMAPHORE_B,
        DRF_NUM(90B7, _SEMAPHORE_B, _LOWER, LwU64_LO32(SemaphoreOffset))));

    const UINT32 PayLoadValue = 0x5670000 + StreamIndex*0x100 + FrameIdx;

    CHECK_RC(m_pCh->Write(s_ENC, LW90B7_SEMAPHORE_C, PayLoadValue));

    CHECK_RC(m_pCh->Write(s_ENC, LW90B7_EXELWTE,
        DRF_DEF(90B7, _EXELWTE, _NOTIFY, _ENABLE) |
        DRF_DEF(90B7, _EXELWTE, _NOTIFY_ON, _END) |
        DRF_DEF(90B7, _EXELWTE, _AWAKEN, _DISABLE)));

    CHECK_RC(m_pCh->Flush());

    PollSemaphore_Args CompletionArgs =
    {
        Semaphore,
        PayLoadValue
    };

    CHECK_RC(POLLWRAP_HW(GpuPollSemaVal, &CompletionArgs,
        m_pTestConfig->TimeoutMs()));

    msenc_pic_stat_s stat;
    Platform::VirtualRd((UINT08*)m_EncodeStatus.GetAddress() +
        offsetof(msenc_stat_data_s, pic_stat), &stat, sizeof(stat));
    Printf(Tee::PriLow, "MSENC: total_bit_count/8=%d, error_status=%d\n",
        stat.total_bit_count/8,
        stat.error_status);

    CHECK_RC(m_pGolden->Run());

    return rc;
}

RC MSENCTest::Run()
{
    RC rc;

    UINT32 StartingLoopIdx = 0;
    CHECK_RC(m_pCh->SetObject(s_ENC, m_MSENCAlloc.GetHandle()));
    for (UINT32 StreamIdx = 0; StreamIdx < NUM_STREAMS; StreamIdx++)
    {
        m_LwrStreamIdx = StreamIdx;
        UINT32 Loop = StartingLoopIdx;
        StartingLoopIdx += StreamDescriptions[StreamIdx].NumFrames;

        if ((1<<StreamIdx) & m_StreamSkipMask)
        {
            Printf(Tee::PriNormal, "Skipping stream %s.\n",
                StreamDescriptions[StreamIdx].Name);
            continue;
        }

        Printf(Tee::PriLow, "Starting stream %s:\n",
            StreamDescriptions[StreamIdx].Name);

        m_pGolden->SetLoop(Loop);

        switch (StreamIdx)
        {
            case H264_BASELINE_CAVLC:
                CHECK_RC(PrepareControlStructuresH264BaselineCAVLC(
                    &m_PicSetup));
                break;
            case H264_HP_CABAC:
                CHECK_RC(PrepareControlStructuresH264HighprofileCABAC(
                    &m_PicSetup, &m_IoRcProcess));
                break;
            case H264_MVC_HP_CABAC:
                CHECK_RC(PrepareControlStructuresH264MVCHighprofileCABAC(
                    &m_PicSetup));
                break;
            case H264_HD_HP_CABAC_MS:
                CHECK_RC(PrepareControlStructuresH264HDHighprofileCABAC(
                    &m_PicSetup, &m_IoRcProcess));
                break;
            default:
                return RC::SOFTWARE_ERROR;
        }

        UINT32 NumFrames =
            (m_MaxFrames < StreamDescriptions[StreamIdx].NumFrames) ?
            m_MaxFrames :
            StreamDescriptions[StreamIdx].NumFrames;

        for (UINT32 FrameIdx = 0; FrameIdx < NumFrames; FrameIdx++)
        {
            m_LwrFrameIdx = FrameIdx;
            CHECK_RC(FillYUVInput(StreamIdx, FrameIdx));

            rc = RunFrame(StreamIdx, FrameIdx);

            SaveSurfaces(StreamIdx, FrameIdx);

            CHECK_RC(rc);
        }
    }

    CHECK_RC(m_pGolden->ErrorRateTest(GetJSObject()));
    return rc;
}

void MSENCTest::PrintCallback(void* test)
{
    static_cast<MSENCTest*>(test)->Print(Tee::PriHigh);
}

void MSENCTest::Print(INT32 pri)
{
    Printf(pri, "Golden error in stream %s at frame %d.\n",
           StreamDescriptions[m_LwrStreamIdx].Name, m_LwrFrameIdx);
}

JS_CLASS_INHERIT(MSENCTest, GpuTest,
                 "MSENCTest test.");

CLASS_PROP_READWRITE(MSENCTest, SaveSurfaces, bool,
                     "Save complete content of all surface for each frame");

CLASS_PROP_READWRITE(MSENCTest, YUVInputFromFiles, bool,
                     "Read YUV input from files instead of generating on the fly");

CLASS_PROP_READWRITE(MSENCTest, AllocateInSysmem, bool,
                     "Allocate all memory as coherent");

CLASS_PROP_READWRITE(MSENCTest, MaxFrames, UINT32,
    "Limits the number of rendered frames (default = all frames).");

CLASS_PROP_READWRITE(MSENCTest, StreamSkipMask, UINT32,
    "Bitmask of streams to skip. Use value 1 to skip first, 2 to skip second, 3 to skip first two stream, etc. (default = 0).");
