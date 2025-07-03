/*
* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2001-2005 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

#ifndef _cl7476sc_h_
#define _cl7476sc_h_

#ifdef __cplusplus
extern "C" {
#endif

#include "lwtypes.h"

/*

    DO NOT EDIT - THIS FILE WAS GENERATED FROM THE FOLLOWING SUBCLASS MANUAL(S)
           usr_vp2_h264_decode.ref
           usr_vp2_h264_deblock.ref
           usr_vp2_scaler.ref
           usr_vp2_vc1_idct.ref
           usr_vp2_vc1_decode.ref
           usr_vp2_vc1_preprocess.ref
           usr_vp2_vc1_deblock.ref
           usr_vp2_scrambler.ref
           usr_vp2_histogram_collection.ref

*/

/* #define statements from class manual usr_vp2_h264_decode.ref */
#define LW7476_H264_DECODE_SET_CTRL_PARAM                                           (0x00000400)
#define LW7476_H264_DECODE_SET_CTRL_PARAM_RING_OUTPUT                               0:0
#define LW7476_H264_DECODE_SET_CTRL_PARAM_OFFSET_METHODS                            8:8
#define LW7476_H264_DECODE_SET_MB_COUNT                                             (0x00000404)
#define LW7476_H264_DECODE_SET_MB_COUNT_COUNT                                       31:0
#define LW7476_H264_DECODE_SET_CONTEXT_ID                                           (0x00000408)
#define LW7476_H264_DECODE_SET_CONTEXT_ID_CTX_ID_RES                                3:0
#define LW7476_H264_DECODE_SET_CONTEXT_ID_CTX_ID_PIC                                7:4
#define LW7476_H264_DECODE_SET_CONTEXT_ID_CTX_ID_MB                                 11:8
#define LW7476_H264_DECODE_SET_CONTEXT_ID_CTX_ID_IN                                 15:12
#define LW7476_H264_DECODE_SET_CONTEXT_ID_CTX_ID_OUT                                19:16
#define LW7476_H264_DECODE_SET_CONTEXT_ID_CTX_ID_FC                                 23:20
#define LW7476_H264_DECODE_SET_CONTEXT_ID_CTX_ID_GLB                                27:24
#define LW7476_H264_DECODE_SET_TILING_MODE                                          (0x0000040c)
#define LW7476_H264_DECODE_SET_TILING_MODE_TM_RES                                   3:0
#define LW7476_H264_DECODE_SET_TILING_MODE_TM_PIC                                   7:4
#define LW7476_H264_DECODE_SET_TILING_MODE_TM_MB                                    11:8
#define LW7476_H264_DECODE_SET_TILING_MODE_TM_IN                                    15:12
#define LW7476_H264_DECODE_SET_TILING_MODE_TM_OUT                                   19:16
#define LW7476_H264_DECODE_SET_TILING_MODE_TM_FC                                    23:20
#define LW7476_H264_DECODE_SET_TILING_MODE_TM_GLB                                   27:24
#define LW7476_H264_DECODE_SET_PIC_CONTROL_OFFSET                                   (0x00000410)
#define LW7476_H264_DECODE_SET_PIC_CONTROL_OFFSET_OFFSET                            31:0
#define LW7476_H264_DECODE_SET_MB_CONTROL_OFFSET                                    (0x00000414)
#define LW7476_H264_DECODE_SET_MB_CONTROL_OFFSET_OFFSET                             31:0
#define LW7476_H264_DECODE_SET_MB_CONTROL_SIZE                                      (0x00000418)
#define LW7476_H264_DECODE_SET_MB_CONTROL_SIZE_SIZE                                 31:0
#define LW7476_H264_DECODE_SET_RESIDUAL_OFFSET                                      (0x0000041c)
#define LW7476_H264_DECODE_SET_RESIDUAL_OFFSET_OFFSET                               31:0
#define LW7476_H264_DECODE_SET_RESIDUAL_SIZE                                        (0x00000420)
#define LW7476_H264_DECODE_SET_RESIDUAL_SIZE_SIZE                                   31:0
#define LW7476_H264_DECODE_SET_GLOBAL_OFFSET                                        (0x00000424)
#define LW7476_H264_DECODE_SET_GLOBAL_OFFSET_OFFSET                                 31:0
#define LW7476_H264_DECODE_SET_FLOW_CTRL_OFFSET                                     (0x00000428)
#define LW7476_H264_DECODE_SET_FLOW_CTRL_OFFSET_OFFSET                              31:0
#define LW7476_H264_DECODE_SET_PICTURE_INDEX                                        (0x0000042c)
#define LW7476_H264_DECODE_SET_PICTURE_INDEX_INDEX                                  31:0
#define LW7476_H264_DECODE_SET_BUFFER_MARGINS                                       (0x00000430)
#define LW7476_H264_DECODE_SET_BUFFER_MARGINS_MBCTRL_MARGIN                         15:0
#define LW7476_H264_DECODE_SET_BUFFER_MARGINS_RES_MARGIN                            31:16
#define LW7476_H264_DECODE_SET_OUTPUT_OFFSET                                        (0x00000434)
#define LW7476_H264_DECODE_SET_OUTPUT_OFFSET_OFFSET                                 31:0
#define LW7476_H264_DECODE_SET_INPUT_OFFSET                                         (0x00000438)
#define LW7476_H264_DECODE_SET_INPUT_OFFSET_OFFSET                                  31:0
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET0                                  (0x00000438)
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET0_OFFSET                           31:0
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET0                                  (0x0000043c)
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET0_OFFSET                           31:0
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET1                                  (0x00000440)
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET1_OFFSET                           31:0
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET1                                  (0x00000444)
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET1_OFFSET                           31:0
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET2                                  (0x00000448)
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET2_OFFSET                           31:0
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET2                                  (0x0000044c)
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET2_OFFSET                           31:0
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET3                                  (0x00000450)
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET3_OFFSET                           31:0
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET3                                  (0x00000454)
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET3_OFFSET                           31:0
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET4                                  (0x00000458)
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET4_OFFSET                           31:0
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET4                                  (0x0000045c)
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET4_OFFSET                           31:0
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET5                                  (0x00000460)
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET5_OFFSET                           31:0
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET5                                  (0x00000464)
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET5_OFFSET                           31:0
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET6                                  (0x00000468)
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET6_OFFSET                           31:0
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET6                                  (0x0000046c)
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET6_OFFSET                           31:0
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET7                                  (0x00000470)
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET7_OFFSET                           31:0
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET7                                  (0x00000474)
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET7_OFFSET                           31:0
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET8                                  (0x00000478)
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET8_OFFSET                           31:0
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET8                                  (0x0000047c)
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET8_OFFSET                           31:0
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET9                                  (0x00000480)
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET9_OFFSET                           31:0
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET9                                  (0x00000484)
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET9_OFFSET                           31:0
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET10                                 (0x00000488)
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET10_OFFSET                          31:0
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET10                                 (0x0000048c)
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET10_OFFSET                          31:0
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET11                                 (0x00000490)
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET11_OFFSET                          31:0
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET11                                 (0x00000494)
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET11_OFFSET                          31:0
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET12                                 (0x00000498)
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET12_OFFSET                          31:0
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET12                                 (0x0000049c)
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET12_OFFSET                          31:0
#define LW7476_H264_DECODE_SET_INPUT_FIELD13_OFFSET                                 (0x000004a0)
#define LW7476_H264_DECODE_SET_INPUT_FIELD13_OFFSET_OFFSET                          31:0
#define LW7476_H264_DECODE_SET_INPUT_FRAME13_OFFSET                                 (0x000004a4)
#define LW7476_H264_DECODE_SET_INPUT_FRAME13_OFFSET_OFFSET                          31:0
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET14                                 (0x000004a8)
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET14_OFFSET                          31:0
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET14                                 (0x000004ac)
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET14_OFFSET                          31:0
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET15                                 (0x000004b0)
#define LW7476_H264_DECODE_SET_INPUT_FIELD_OFFSET15_OFFSET                          31:0
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET15                                 (0x000004b4)
#define LW7476_H264_DECODE_SET_INPUT_FRAME_OFFSET15_OFFSET                          31:0
#define LW7476_H264_DECODE_APP_STATUS_DECODE_INIT                                   ((0x1500d000))
#define LW7476_H264_DECODE_APP_STATUS_WAIT_FOR_BSP                                  ((0x1500d002))
#define LW7476_H264_DECODE_APP_STATUS_DECODE_CHUNK                                  ((0x1500d004))
#define LW7476_H264_DECODE_APP_STATUS_DECODE_DONE                                   ((0x1500d00c))
#define LW7476_H264_DECODE_APP_STATUS_DECODE_END                                    ((0x1500d00d))
#define LW7476_H264_DECODE_RETURN_OK                                                (0x00000000)
#define LW7476_H264_DECODE_ERROR_UNABLE_TO_INIT_DEC                                 (0xdec00001)
#define LW7476_H264_DECODE_ERROR_SET_SURF_ATTRIBS                                   (0xdec00002)
#define LW7476_H264_DECODE_ERROR_CREATE_SURF_OUT                                    (0xdec00003)
#define LW7476_H264_DECODE_ERROR_CREATE_SURF_IN                                     (0xdec00004)
#define LW7476_H264_DECODE_ERROR_UNABLE_TO_INIT_RING                                (0xdec00005)
#define LW7476_H264_DECODE_ERROR_CTX_RESERVD_RES                                    (0xdec00100)
#define LW7476_H264_DECODE_ERROR_CTX_RESERVD_PIC                                    (0xdec00101)
#define LW7476_H264_DECODE_ERROR_CTX_RESERVD_MB                                     (0xdec00102)
#define LW7476_H264_DECODE_ERROR_CTX_RESERVD_IN                                     (0xdec00103)
#define LW7476_H264_DECODE_ERROR_CTX_RESERVD_OUT                                    (0xdec00104)
#define LW7476_H264_DECODE_ERROR_CTX_RESERVD_FC                                     (0xdec00105)
#define LW7476_H264_DECODE_ERROR_CTX_RESERVD_GLB                                    (0xdec00106)
#define LW7476_H264_DECODE_ERROR_CTX_ILWALID_TM_RES                                 (0xdec00200)
#define LW7476_H264_DECODE_ERROR_CTX_ILWALID_TM_PIC                                 (0xdec00201)
#define LW7476_H264_DECODE_ERROR_CTX_ILWALID_TM_MB                                  (0xdec00202)
#define LW7476_H264_DECODE_ERROR_CTX_ILWALID_TM_IN                                  (0xdec00203)
#define LW7476_H264_DECODE_ERROR_CTX_ILWALID_TM_OUT                                 (0xdec00204)
#define LW7476_H264_DECODE_ERROR_CTX_ILWALID_TM_FC                                  (0xdec00205)
#define LW7476_H264_DECODE_ERROR_CTX_ILWALID_TM_GLB                                 (0xdec00206)
#define LW7476_H264_DECODE_ERROR_ILWALID_PIC_OFFS                                   (0xdec00300)
#define LW7476_H264_DECODE_ERROR_ILWALID_MB_OFFS                                    (0xdec00301)
#define LW7476_H264_DECODE_ERROR_EXCEEDS_MAX_MBSIZE                                 (0xdec00302)
#define LW7476_H264_DECODE_ERROR_ILWALID_RES_OFFS                                   (0xdec00303)
#define LW7476_H264_DECODE_ERROR_EXCEEDS_MAXRESSIZE                                 (0xdec00304)
#define LW7476_H264_DECODE_ERROR_LNRMODE_MBCTRLWR_ZERO                              (0xdec00305)
#define LW7476_H264_DECODE_ERROR_MBCTRL_RING_TIMEOUT                                (0xdec00400)
#define LW7476_H264_DECODE_ERROR_MBWRITE_PTR_MISMATCH                               (0xdec00401)
#define LW7476_H264_DECODE_ERROR_ILWALID_MBCTRL_READ_PTR                            (0xdec00402)
#define LW7476_H264_DECODE_ERROR_ILWALID_MBCTRL_WRITE_PTR                           (0xdec00403)
#define LW7476_H264_DECODE_ERROR_ILWALID_RESIDUAL_READ_PTR                          (0xdec00404)
#define LW7476_H264_DECODE_ERROR_ILWALID_RESIDUAL_WRITE_PTR                         (0xdec00405)
#define LW7476_H264_DECODE_ERROR_DEC_BEYOND_MBCTRL_WRITE                            (0xdec00406)
#define LW7476_H264_DECODE_ERROR_MB_OUT_OF_ORDER                                    (0xdec00407)
#define LW7476_H264_DECODE_ERROR_PIC_OUT_OF_SYNC                                    (0xdec00408)

/* #define statements from class manual usr_vp2_h264_deblock.ref */
#define LW7476_H264_DEBLOCK_SET_CONTEXT_IDS1                                        (0x00000400)
#define LW7476_H264_DEBLOCK_SET_CONTEXT_IDS1_PIC_CTX                                3:0
#define LW7476_H264_DEBLOCK_SET_CONTEXT_IDS1_MB_CTX                                 11:8
#define LW7476_H264_DEBLOCK_SET_CONTEXT_IDS1_FB_IN                                  19:16
#define LW7476_H264_DEBLOCK_SET_CONTEXT_IDS1_TILE_I                                 23:20
#define LW7476_H264_DEBLOCK_SET_CONTEXT_IDS1_FB_OUT                                 27:24
#define LW7476_H264_DEBLOCK_SET_CONTEXT_IDS1_TILE_O                                 31:28
#define LW7476_H264_DEBLOCK_SET_PIC_CONTROL_OFFSET                                  (0x00000404)
#define LW7476_H264_DEBLOCK_SET_PIC_CONTROL_OFFSET_OFFSET                           31:0
#define LW7476_H264_DEBLOCK_SET_MB_CONTROL_OFFSET                                   (0x00000408)
#define LW7476_H264_DEBLOCK_SET_MB_CONTROL_OFFSET_OFFSET                            31:0
#define LW7476_H264_DEBLOCK_SET_OFFSET_IN                                           (0x0000040c)
#define LW7476_H264_DEBLOCK_SET_OFFSET_IN_OFFSET                                    31:0
#define LW7476_H264_DEBLOCK_SET_OFFSET_OUT_FLD                                      (0x00000410)
#define LW7476_H264_DEBLOCK_SET_OFFSET_OUT_FLD_OFFSET                               31:0
#define LW7476_H264_DEBLOCK_SET_OFFSET_OUT_FRM                                      (0x00000414)
#define LW7476_H264_DEBLOCK_SET_OFFSET_OUT_FRM_OFFSET                               31:0
#define LW7476_H264_DEBLOCK_APP_STATUS_DEBLOCK_START                                (0x16000001)
#define LW7476_H264_DEBLOCK_APP_STATUS_DEBLOCK_FRAME                                (0x16000010)
#define LW7476_H264_DEBLOCK_APP_STATUS_DEBLOCK_FIELD                                (0x16000011)
#define LW7476_H264_DEBLOCK_APP_STATUS_DEBLOCK_MBAFF                                (0x16000012)
#define LW7476_H264_DEBLOCK_APP_STATUS_DEBLOCK_BYPASS                               (0x16000013)
#define LW7476_H264_DEBLOCK_APP_STATUS_DEBLOCK_DONE                                 (0x16000014)
#define LW7476_H264_DEBLOCK_APP_STATUS_DEBLOCK_END                                  (0x16000002)
#define LW7476_H264_DEBLOCK_INPUT_PITCH_ZERO                                        (0x04001)
#define LW7476_H264_DEBLOCK_FIELD_OUTPUT_PITCH_ZERO                                 (0x04002)
#define LW7476_H264_DEBLOCK_FRAME_OUTPUT_PITCH_ZERO                                 (0x04003)
#define LW7476_H264_DEBLOCK_INPUT_HEIGHT_ZERO                                       (0x04004)
#define LW7476_H264_DEBLOCK_FIELD_OUTPUT_HEIGHT_ZERO                                (0x04005)
#define LW7476_H264_DEBLOCK_FRAME_OUTPUT_HEIGHT_ZERO                                (0x04006)
#define LW7476_H264_DEBLOCK_LUMA_WIDTH_ZERO                                         (0x04007)
#define LW7476_H264_DEBLOCK_LUMA_HEIGHT_ZERO                                        (0x04008)
#define LW7476_H264_DEBLOCK_PIC_SIZE_IN_MBS_ZERO                                    (0x04009)
#define LW7476_H264_DEBLOCK_INPUT_PITCH_NOT_MULP_OF_64                              (0x04010)
#define LW7476_H264_DEBLOCK_FIELD_OUTPUT_PITCH_NOT_MULP_OF_64                       (0x04011)
#define LW7476_H264_DEBLOCK_FRAME_OUTPUT_PITCH_NOT_MULP_OF_64                       (0x04012)
#define LW7476_H264_DEBLOCK_INPUT_HEIGHT_NOT_MULP_OF_32                             (0x04013)
#define LW7476_H264_DEBLOCK_FIELD_OUTPUT_HEIGHT_NOT_MULP_OF_32                      (0x04014)
#define LW7476_H264_DEBLOCK_FRAME_OUTPUT_HEIGHT_NOT_MULP_OF_16                      (0x04015)
#define LW7476_H264_DEBLOCK_ILWALID_PICCTRL_ENTRY_BYPASS_DEBLOCK                    (0x04021)
#define LW7476_H264_DEBLOCK_ILWALID_PICCTRL_ENTRY_MBAFF                             (0x04022)
#define LW7476_H264_DEBLOCK_ILWALID_PICCTRL_ENTRY_FIELD_PIC                         (0x04023)
#define LW7476_H264_DEBLOCK_ILWALID_PICCTRL_ENTRY_BOTTOM_FIELD                      (0x04024)
#define LW7476_H264_DEBLOCK_ILWALID_PICCTRL_ENTRY_ENABLE_FRAME_OUTPUT               (0x04025)
#define LW7476_H264_DEBLOCK_ILWALID_CTXDMA_ID_PICCTRL                               (0x04031)
#define LW7476_H264_DEBLOCK_ILWALID_CTXDMA_ID_MBCTRL                                (0x04032)
#define LW7476_H264_DEBLOCK_ILWALID_CTXDMA_ID_INPUT_BUFFER                          (0x04033)
#define LW7476_H264_DEBLOCK_ILWALID_CTXDMA_ID_OUTPUT_BUFFER                         (0x04034)
#define LW7476_H264_DEBLOCK_ILWALID_TILING_INPUT_BUFFER                             (0x04041)
#define LW7476_H264_DEBLOCK_ILWALID_TILING_OUTPUT_BUFFER                            (0x04042)

/* #define statements from class manual usr_vp2_scaler.ref */
#define LW7476_SCALER_SET_SOURCE_CONTEXT_ID                                         (0x00000400)
#define LW7476_SCALER_SET_SOURCE_CONTEXT_ID_VALUE                                   3:0
#define LW7476_SCALER_SET_SOURCE_CONTEXT_ID_TILE_MODE                               7:4
#define LW7476_SCALER_SET_SOURCE_CONTEXT_IMAGE_OFFSET                               (0x00000404)
#define LW7476_SCALER_SET_SOURCE_CONTEXT_IMAGE_OFFSET_VALUE                         31:0
#define LW7476_SCALER_SET_SOURCE_CONTEXT_IMAGE_CHROMA_OFFSET                        (0x00000408)
#define LW7476_SCALER_SET_SOURCE_CONTEXT_IMAGE_CHROMA_OFFSET_VALUE                  31:0
#define LW7476_SCALER_SET_SOURCE_CONTEXT_IMAGE_CHROMA2_OFFSET                       (0x0000040C)
#define LW7476_SCALER_SET_SOURCE_CONTEXT_IMAGE_CHROMA2_OFFSET_VALUE                 31:0
#define LW7476_SCALER_SET_DESTINATION_CONTEXT_ID                                    (0x00000410)
#define LW7476_SCALER_SET_DESTINATION_CONTEXT_ID_VALUE                              3:0
#define LW7476_SCALER_SET_DESTINATION_CONTEXT_TILE_MODE                             7:4
#define LW7476_SCALER_SET_DESTINATION_CONTEXT_IMAGE_OFFSET                          (0x00000414)
#define LW7476_SCALER_SET_DESTINATION_CONTEXT_IMAGE_OFFSET_VALUE                    31:0
#define LW7476_SCALER_SET_DESTINATION_CONTEXT_IMAGE_CHROMA_OFFSET                   (0x00000418)
#define LW7476_SCALER_SET_DESTINATION_CONTEXT_IMAGE_CHROMA_OFFSET_VALUE             31:0
#define LW7476_SCALER_IMAGE_FORMAT_ARGB                                             (0x0000)
#define LW7476_SCALER_IMAGE_FORMAT_AYUV                                             (0x0001)
#define LW7476_SCALER_IMAGE_FORMAT_LW12                                             (0x0002)
#define LW7476_SCALER_IMAGE_FORMAT_YUY2                                             (0x0003)
#define LW7476_SCALER_IMAGE_FORMAT_UYVY                                             (0x0004)
#define LW7476_SCALER_IMAGE_FORMAT_YV12                                             (0x0005)
#define LW7476_SCALER_IMAGE_FORMAT_LW11                                             (0x0006)
#define LW7476_SCALER_IMAGE_FORMAT_USE_8X8FILTER                                    (0x0)
#define LW7476_SCALER_IMAGE_FORMAT_USE_5X8FILTER                                    (0x1)
#define LW7476_SCALER_SET_IMAGE_FORMATS                                             (0x0000041C)
#define LW7476_SCALER_SET_SOURCE_IMAGE_FORMAT                                       15:0
#define LW7476_SCALER_SET_DESTINATION_IMAGE_FORMAT                                  30:16
#define LW7476_SCALER_SET_IMAGE_FORMAT_SELECT_FILTER                                31:31
#define LW7476_SCALER_SET_SOURCE_START_POSITION_X                                   (0x00000420)
#define LW7476_SCALER_SET_SOURCE_START_POSITION_X_VALUE                             31:0
#define LW7476_SCALER_SET_SOURCE_START_POSITION_Y                                   (0x00000424)
#define LW7476_SCALER_SET_SOURCE_START_POSITION_Y_VALUE                             31:0
#define LW7476_SCALER_SET_SOURCE_WIDTH                                              (0x00000428)
#define LW7476_SCALER_SET_SOURCE_WIDTH_VALUE                                        31:0
#define LW7476_SCALER_SET_SOURCE_HEIGHT                                             (0x0000042C)
#define LW7476_SCALER_SET_SOURCE_HEIGHT_VALUE                                       31:0
#define LW7476_SCALER_SET_SOURCE_PITCH                                              (0x00000430)
#define LW7476_SCALER_SET_SOURCE_PITCH_VALUE                                        31:0
#define LW7476_SCALER_SET_DESTINATION_START_POSITION_X                              (0x00000434)
#define LW7476_SCALER_SET_DESTINATION_START_POSITION_X_VALUE                        31:0
#define LW7476_SCALER_SET_DESTINATION_START_POSITION_Y                              (0x00000438)
#define LW7476_SCALER_SET_DESTINATION_START_POSITION_Y_VALUE                        31:0
#define LW7476_SCALER_SET_DESTINATION_WIDTH                                         (0x0000043C)
#define LW7476_SCALER_SET_DESTINATION_WIDTH_VALUE                                   31:0
#define LW7476_SCALER_SET_DESTINATION_HEIGHT                                        (0x00000440)
#define LW7476_SCALER_SET_DESTINATION_HEIGHT_VALUE                                  31:0
#define LW7476_SCALER_SET_DESTINATION_PITCH                                         (0x00000444)
#define LW7476_SCALER_SET_DESTINATION_PITCH_VALUE                                   31:0
#define LW7476_SCALER_SET_HORIZONTAL_SCALE_FACTOR                                   (0x00000448)
#define LW7476_SCALER_SET_HORIZONTAL_SCALE_FACTOR_VALUE                             31:0
#define LW7476_SCALER_SET_VERTICAL_SCALE_FACTOR                                     (0x0000044C)
#define LW7476_SCALER_SET_VERTICAL_SCALE_FACTOR_VALUE                               31:0
#define LW7476_SCALER_SET_DESTINATION_IMAGE_ALPHA                                   (0x00000450)
#define LW7476_SCALER_SET_DESTINATION_IMAGE_ALPHA_VALUE                             7:0
#define LW7476_SCALER_SET_DESTINATION_BRL                                           (0x00000454)
#define LW7476_SCALER_SET_DESTINATION_BRL_VALUE                                     15:0
#define LW7476_SCALER_SET_DESTINATION_GAMMA                                         (0x00000458)
#define LW7476_SCALER_SET_DESTINATION_GAMMA_VALUE                                   15:0
#define LW7476_SCALER_SET_PROCAMP_MATRIX_00                                         (0x0000045C)
#define LW7476_SCALER_SET_PROCAMP_MATRIX_00_VALUE                                   15:0
#define LW7476_SCALER_SET_PROCAMP_MATRIX_01                                         (0x00000460)
#define LW7476_SCALER_SET_PROCAMP_MATRIX_01_VALUE                                   15:0
#define LW7476_SCALER_SET_PROCAMP_MATRIX_02                                         (0x00000464)
#define LW7476_SCALER_SET_PROCAMP_MATRIX_02_VALUE                                   15:0
#define LW7476_SCALER_SET_PROCAMP_MATRIX_10                                         (0x00000468)
#define LW7476_SCALER_SET_PROCAMP_MATRIX_10_VALUE                                   15:0
#define LW7476_SCALER_SET_PROCAMP_MATRIX_11                                         (0x0000046C)
#define LW7476_SCALER_SET_PROCAMP_MATRIX_11_VALUE                                   15:0
#define LW7476_SCALER_SET_PROCAMP_MATRIX_12                                         (0x00000470)
#define LW7476_SCALER_SET_PROCAMP_MATRIX_12_VALUE                                   15:0
#define LW7476_SCALER_SET_PROCAMP_MATRIX_20                                         (0x00000474)
#define LW7476_SCALER_SET_PROCAMP_MATRIX_20_VALUE                                   15:0
#define LW7476_SCALER_SET_PROCAMP_MATRIX_21                                         (0x00000478)
#define LW7476_SCALER_SET_PROCAMP_MATRIX_21_VALUE                                   15:0
#define LW7476_SCALER_SET_PROCAMP_MATRIX_22                                         (0x0000047C)
#define LW7476_SCALER_SET_PROCAMP_MATRIX_22_VALUE                                   15:0

/* #define statements from class manual usr_vp2_vc1_idct.ref */
#define LW7476_VC1_IDCT_SET_FRAME_SIZE                                              (0x00000400)
#define LW7476_VC1_IDCT_SET_FRAME_SIZE_WIDTH                                        15:0
#define LW7476_VC1_IDCT_SET_FRAME_SIZE_HEIGHT                                       31:16
#define LW7476_VC1_IDCT_ERROR_HEIGHT_NOT_MULT_OF_EIGHT                              (0x00004001)
#define LW7476_VC1_IDCT_ERROR_WIDTH_NOT_MULT_OF_EIGHT                               (0x00004002)
#define LW7476_VC1_IDCT_ERROR_ZERO_HEIGHT                                           (0x00004003)
#define LW7476_VC1_IDCT_ERROR_ZERO_WIDTH                                            (0x00004004)
#define LW7476_VC1_IDCT_SET_IN_FRAME_OFFSET                                         (0x00000404)
#define LW7476_VC1_IDCT_SET_IN_FRAME_OFFSET_OFFSET                                  31:0
#define LW7476_VC1_IDCT_ERROR_IN_FRAME_OFFSET_ILWALID                               (0x00004041)
#define LW7476_VC1_IDCT_SET_IDCT_MODE_OFFSET                                        (0x00000408)
#define LW7476_VC1_IDCT_SET_IDCT_MODE_OFFSET_OFFSET                                 31:0
#define LW7476_VC1_IDCT_ERROR_IDCT_MODE_OFFSET_ILWALID                              (0x00004081)
#define LW7476_VC1_IDCT_SET_CBP_OFFSET                                              (0x0000040c)
#define LW7476_VC1_IDCT_SET_CBP_OFFSET_OFFSET                                       31:0
#define LW7476_VC1_IDCT_ERROR_CBP_OFFSET_ILWALID                                    (0x000040c1)
#define LW7476_VC1_IDCT_SET_OUT_FRAME_OFFSET                                        (0x00000410)
#define LW7476_VC1_IDCT_SET_OUT_FRAME_OFFSET_OFFSET                                 31:0
#define LW7476_VC1_IDCT_ERROR_OUT_FRAME_OFFSET_ILWALID                              (0x00004101)
#define LW7476_VC1_IDCT_SET_LUT_OFFSET                                              (0x00000414)
#define LW7476_VC1_IDCT_SET_LUT_OFFSET_OFFSET                                       31:0
#define LW7476_VC1_IDCT_ERROR_LUT_OFFSET_ILWALID                                    (0x00004141)
#define LW7476_VC1_IDCT_SET_CTX_DMA_INDEX                                           (0x00000418)
#define LW7476_VC1_IDCT_SET_CTX_DMA_INDEX_INFRAME                                   3:0
#define LW7476_VC1_IDCT_SET_CTX_DMA_INDEX_IDCTMODE                                  7:4
#define LW7476_VC1_IDCT_SET_CTX_DMA_INDEX_CBP                                       11:8
#define LW7476_VC1_IDCT_SET_CTX_DMA_INDEX_OUTFRAME                                  15:12
#define LW7476_VC1_IDCT_SET_CTX_DMA_INDEX_LUT                                       19:16
#define LW7476_VC1_IDCT_ERROR_INFRAME_INDEX_ILWALID                                 (0x00004181)
#define LW7476_VC1_IDCT_ERROR_IDCTMODE_INDEX_ILWALID                                (0x00004182)
#define LW7476_VC1_IDCT_ERROR_CBP_INDEX_ILWALID                                     (0x00004183)
#define LW7476_VC1_IDCT_ERROR_OUTFRAME_INDEX_ILWALID                                (0x00004184)
#define LW7476_VC1_IDCT_ERROR_LUT_INDEX_ILWALID                                     (0x00004185)

/* #define statements from class manual usr_vp2_vc1_decode.ref */
#define LW7476_VC1_DECODE_SET_CTX_DMA_INDEX                                         (0x00000400)
#define LW7476_VC1_DECODE_SET_CTX_DMA_INDEX_PICTURE_CONTROL_BUFFER                  3:0
#define LW7476_VC1_DECODE_SET_CTX_DMA_INDEX_MB_CONTROL_BUFFER                       7:4
#define LW7476_VC1_DECODE_SET_CTX_DMA_INDEX_RESIDUAL_CONTROL_BUFFER                 11:8
#define LW7476_VC1_DECODE_SET_CTX_DMA_INDEX_COEFFICIENT_BUFFER                      15:12
#define LW7476_VC1_DECODE_SET_CTX_DMA_INDEX_OUTPUT_BUFFER                           19:16
#define LW7476_VC1_DECODE_SET_CTX_DMA_INDEX_ZERO_SURFACE_BUFFER                     23:20
#define LW7476_VC1_DECODE_SET_CTX_DMA_INDEX_FORWARD_REFERENCE_BUFFER                27:24
#define LW7476_VC1_DECODE_SET_CTX_DMA_INDEX_BACKWARD_REFERENCE_BUFFER               31:28
#define LW7476_VC1_DECODE_SET_TILING                                                (0x00000404)
#define LW7476_VC1_DECODE_SET_TILING_PICTURE_CONTROL_BUFFER                         3:0
#define LW7476_VC1_DECODE_SET_TILING_MB_CONTROL_BUFFER                              7:4
#define LW7476_VC1_DECODE_SET_TILING_RESIDUAL_CONTROL_BUFFER                        11:8
#define LW7476_VC1_DECODE_SET_TILING_COEFFICIENT_BUFFER                             15:12
#define LW7476_VC1_DECODE_SET_TILING_OUTPUT_BUFFER                                  19:16
#define LW7476_VC1_DECODE_SET_TILING_ZERO_SURFACE_BUFFER                            23:20
#define LW7476_VC1_DECODE_SET_TILING_FORWARD_REFERENCE_BUFFER                       27:24
#define LW7476_VC1_DECODE_SET_TILING_BACKWARD_REFERENCE_BUFFER                      31:28
#define LW7476_VC1_DECODE_SET_PICTURE_CONTROL_OFFSET                                (0x00000408)
#define LW7476_VC1_DECODE_SET_PICTURE_CONTROL_OFFSET_MSB32                          31:0
#define LW7476_VC1_DECODE_SET_MACROBLOCK_CONTROL_OFFSET                             (0x0000040c)
#define LW7476_VC1_DECODE_SET_MACROBLOCK_CONTROL_OFFSET_MSB32                       31:0
#define LW7476_VC1_DECODE_SET_COEFFICIENT_BUFFER_OFFSET                             (0x00000410)
#define LW7476_VC1_DECODE_SET_COEFFICIENT_BUFFER_OFFSET_MSB32                       31:0
#define LW7476_VC1_DECODE_SET_RESIDUAL_BUFFER_OFFSET                                (0x00000414)
#define LW7476_VC1_DECODE_SET_RESIDUAL_BUFFER_OFFSET_MSB32                          31:0
#define LW7476_VC1_DECODE_SET_OUTPUT_BUFFER_OFFSET                                  (0x00000418)
#define LW7476_VC1_DECODE_SET_OUTPUT_BUFFER_OFFSET_MSB32                            31:0
#define LW7476_VC1_DECODE_SET_OB_BUFFER_OFFSET                                      (0x0000041c)
#define LW7476_VC1_DECODE_SET_OB_BUFFER_OFFSET_MSB32                                31:0
#define LW7476_VC1_DECODE_SET_ZERO_SURFACE_BUFFER_OFFSET                            (0x00000420)
#define LW7476_VC1_DECODE_SET_ZERO_SURFACE_BUFFER_OFFSET_MSB32                      31:0
#define LW7476_VC1_DECODE_SET_OB_CTRL_BUFFER_OFFSET                                 (0x00000424)
#define LW7476_VC1_DECODE_SET_OB_CTRL_BUFFER_OFFSET_MSB32                           31:0
#define LW7476_VC1_DECODE_SET_FORWARD_REFERENCE_BUFFER_OFFSET                       (0x00000428)
#define LW7476_VC1_DECODE_SET_FORWARD_REFERENCE_BUFFER_OFFSET_MSB32                 31:0
#define LW7476_VC1_DECODE_SET_BACKWARD_REFERENCE_BUFFER_OFFSET                      (0x0000042c)
#define LW7476_VC1_DECODE_SET_BACKWARD_REFERENCE_BUFFER_OFFSET_MSB32                31:0
#define LW7476_VC1_DECODE_SET_CTX_DMA_INDEX2                                        (0x00000430)
#define LW7476_VC1_DECODE_SET_CTX_DMA_INDEX2_OB_BUFFER                              3:0
#define LW7476_VC1_DECODE_APP_STATUS_DECODE_START                                   (0x12000001)
#define LW7476_VC1_DECODE_APP_STATUS_DECODE_PROGRESSIVE_I_IDCT                      (0x12000010)
#define LW7476_VC1_DECODE_APP_STATUS_DECODE_PROGRESSIVE_P_IDCT                      (0x12000011)
#define LW7476_VC1_DECODE_APP_STATUS_DECODE_PROGRESSIVE_B_IDCT                      (0x12000012)
#define LW7476_VC1_DECODE_APP_STATUS_DECODE_PROGRESSIVE_I_NOIDCT                    (0x12000013)
#define LW7476_VC1_DECODE_APP_STATUS_DECODE_PROGRESSIVE_P_NOIDCT                    (0x12000014)
#define LW7476_VC1_DECODE_APP_STATUS_DECODE_PROGRESSIVE_B_NOIDCT                    (0x12000015)
#define LW7476_VC1_DECODE_APP_STATUS_DECODE_FLD_INT_I_IDCT                          (0x12000020)
#define LW7476_VC1_DECODE_APP_STATUS_DECODE_FLD_INT_P_IDCT                          (0x12000021)
#define LW7476_VC1_DECODE_APP_STATUS_DECODE_FLD_INT_B_IDCT                          (0x12000022)
#define LW7476_VC1_DECODE_APP_STATUS_DECODE_FLD_INT_I_NOIDCT                        (0x12000023)
#define LW7476_VC1_DECODE_APP_STATUS_DECODE_FLD_INT_P_NOIDCT                        (0x12000024)
#define LW7476_VC1_DECODE_APP_STATUS_DECODE_FLD_INT_B_NOIDCT                        (0x12000025)
#define LW7476_VC1_DECODE_APP_STATUS_DECODE_FRM_INT_I_IDCT                          (0x12000030)
#define LW7476_VC1_DECODE_APP_STATUS_DECODE_FRM_INT_P_IDCT                          (0x12000031)
#define LW7476_VC1_DECODE_APP_STATUS_DECODE_FRM_INT_B_IDCT                          (0x12000032)
#define LW7476_VC1_DECODE_APP_STATUS_DECODE_FRM_INT_I_NOIDCT                        (0x12000033)
#define LW7476_VC1_DECODE_APP_STATUS_DECODE_FRM_INT_P_NOIDCT                        (0x12000034)
#define LW7476_VC1_DECODE_APP_STATUS_DECODE_FRM_INT_B_NOIDCT                        (0x12000035)
#define LW7476_VC1_DECODE_APP_STATUS_DECODE_DONE                                    (0x12000040)
#define LW7476_VC1_DECODE_APP_STATUS_OBFILTER_START                                 (0x12000050)
#define LW7476_VC1_DECODE_APP_STATUS_OBFILTER_DONE                                  (0x12000051)
#define LW7476_VC1_DECODE_APP_STATUS_DECODE_END                                     (0x12000002)
#define LW7476_VC1_DECODE_ERROR_ILWALID_CTXDMA_DETECTED                             (0x00004001)
#define LW7476_VC1_DECODE_ERROR_PIC_WIDTH_ZERO                                      (0x00004002)
#define LW7476_VC1_DECODE_ERROR_PIC_HEIGHT_ZERO                                     (0x00004003)
#define LW7476_VC1_DECODE_ERROR_BUF_PITCH_ZERO                                      (0x00004004)
#define LW7476_VC1_DECODE_ERROR_BUF_HEIGHT_ZERO                                     (0x00004005)
#define LW7476_VC1_DECODE_ERROR_BUF_PITCH_NOT_MULT_OF_64                            (0x00004006)
#define LW7476_VC1_DECODE_ERROR_BUF_HEIGHT_NOT_MULT_OF_32                           (0x00004007)

/* #define statements from class manual usr_vp2_vc1_preprocess.ref */
#define LW7476_VC1_PREPROCESS_SET_CTX_DMA_INDEX_AND_TILING                          (0x00000400)
#define LW7476_VC1_PREPROCESS_SET_CTX_DMA_INDEX_INPUT                               3:0
#define LW7476_VC1_PREPROCESS_SET_TILING_INPUT                                      7:4
#define LW7476_VC1_PREPROCESS_SET_CTX_DMA_INDEX_OUTPUT                              11:8
#define LW7476_VC1_PREPROCESS_SET_TILING_OUTPUT                                     15:12
#define LW7476_VC1_PREPROCESS_SET_RR                                                19:16
#define LW7476_VC1_PREPROCESS_ENABLE_IC_TOP_FIELD                                   20:20
#define LW7476_VC1_PREPROCESS_ENABLE_IC_BOTTOM_FIELD                                24:24
#define LW7476_VC1_PREPROCESS_RANGE_MAPY_FLAG                                       28:28
#define LW7476_VC1_PREPROCESS_RANGE_MAPUV_FLAG                                      30:30
#define LW7476_VC1_PREPROCESS_SET_INPUT_OFFSET                                      (0x00000404)
#define LW7476_VC1_PREPROCESS_SET_INPUT_OFFSET_MSB32                                31:0
#define LW7476_VC1_PREPROCESS_SET_OUTPUT_OFFSET                                     (0x00000408)
#define LW7476_VC1_PREPROCESS_SET_OUTPUT_OFFSET_MSB32                               31:0
#define LW7476_VC1_PREPROCESS_SET_IC_TOP_FIELD                                      (0x0000040c)
#define LW7476_VC1_PREPROCESS_SET_IC_TOP_FIELD_LUMSCALE                             15:0
#define LW7476_VC1_PREPROCESS_SET_IC_TOP_FIELD_LUMSHIFT                             31:16
#define LW7476_VC1_PREPROCESS_SET_IC_BOTTOM_FIELD                                   (0x00000410)
#define LW7476_VC1_PREPROCESS_SET_IC_BOTTOM_FIELD_LUMSCALE                          15:0
#define LW7476_VC1_PREPROCESS_SET_IC_BOTTOM_FIELD_LUMSHIFT                          31:16
#define LW7476_VC1_PREPROCESS_SET_PIC_DIMENSION_BOTTOM_FIELD                        (0x00000414)
#define LW7476_VC1_PREPROCESS_SET_PIC_DIMENSION_BOTTOM_FIELD_PICWIDTH               15:0
#define LW7476_VC1_PREPROCESS_SET_PIC_DIMENSION_BOTTOM_FIELD_PICHEIGHT              31:16
#define LW7476_VC1_PREPROCESS_SET_BUFFER_DIMENSION_BOTTOM_FIELD                     (0x00000418)
#define LW7476_VC1_PREPROCESS_SET_BUFFER_DIMENSION_BOTTOM_FIELD_BUFFERPITCH         15:0
#define LW7476_VC1_PREPROCESS_SET_BUFFER_DIMENSION_BOTTOM_FIELD_BUFFERHEIGHT        31:16
#define LW7476_VC1_PREPROCESS_SET_RANGE_MAP_VALUES                                  (0x0000041c)
#define LW7476_VC1_PREPROCESS_SET_RANGE_MAPY_VAL                                    15:0
#define LW7476_VC1_PREPROCESS_SET_RANGE_MAPUV_VAL                                   31:16
#define LW7476_H264_PREPROCESS_APP_STATUS_PREPROCESS_START                          (0x11000001)
#define LW7476_H264_PREPROCESS_APP_STATUS_PREPROCESS_IC_RM                          (0x11000010)
#define LW7476_H264_PREPROCESS_APP_STATUS_PREPROCESS_IC_RR_TOP                      (0x11000011)
#define LW7476_H264_PREPROCESS_APP_STATUS_PREPROCESS_IC_RR_BOT                      (0x11000012)
#define LW7476_H264_PREPROCESS_APP_STATUS_PREPROCESS_END                            (0x11000002)
#define LW7476_H264_PREPROCESS_ILWALID_CTXDMA_ID_INPUT                              (0x04000)
#define LW7476_H264_PREPROCESS_ILWALID_CTXDMA_ID_MBCTRL                             (0x04001)
#define LW7476_H264_PREPROCESS_ILWALID_TILING_INPUT_BUFFER                          (0x04002)
#define LW7476_H264_PREPROCESS_ILWALID_TILING_OUTPUT_BUFFER                         (0x04003)
#define LW7476_H264_PREPROCESS_BUFFER_PITCH_ZERO                                    (0x04004)
#define LW7476_H264_PREPROCESS_BUFFER_HEIGHT_ZERO                                   (0x04005)
#define LW7476_H264_PREPROCESS_PIC_WIDTH_ZERO                                       (0x04006)
#define LW7476_H264_PREPROCESS_PIC_HEIGHT_ZERO                                      (0x04007)
#define LW7476_H264_PREPROCESS_BUFFER_PITCH_NOT_MULP_OF_64                          (0x04008)
#define LW7476_H264_PREPROCESS_BUFFER_HEIGHT_NOT_MULP_OF_32                         (0x04009)

/* #define statements from class manual usr_vp2_vc1_deblock.ref */
#define LW7476_VC1_DEBLOCK_SET_CTX_DMA_INDEX                                        (0x00000400)
#define LW7476_VC1_DEBLOCK_SET_CTX_DMA_INDEX_CONTROL_FLAG                           3:0
#define LW7476_VC1_DEBLOCK_SET_CTX_DMA_INDEX_PICTURE_BUFFER                         11:8
#define LW7476_VC1_DEBLOCK_SET_PICTURE_BUFFER_TILING_MODE                           15:12
#define LW7476_VC1_DEBLOCK_SET_PIC_OFFSET                                           (0x00000404)
#define LW7476_VC1_DEBLOCK_SET_PIC_OFFSET_OFFSET                                    31:0
#define LW7476_VC1_DEBLOCK_SET_CONTROL_FLAG_OFFSET                                  (0x00000408)
#define LW7476_VC1_DEBLOCK_SET_CONTROL_FLAG_OFFSET_MSB32                            31:0
#define LW7476_VC1_DEBLOCK_SET_PIC_DIMENSION                                        (0x0000040C)
#define LW7476_VC1_DEBLOCK_SET_PIC_DIMENS_WIDTH                                     15:0
#define LW7476_VC1_DEBLOCK_SET_PIC_DIMENS_HEIGHT                                    31:16
#define LW7476_VC1_DEBLOCK_SET_PICBUFFER_DIMENSION                                  (0x00000410)
#define LW7476_VC1_DEBLOCK_SET_PICBUFFER_DIMENSION_PITCH                            15:0
#define LW7476_VC1_DEBLOCK_SET_PICBUFFER_DIMENSION_HEIGHT                           31:16
#define LW7476_VC1_DEBLOCK_SET_FILTER_STRENGTH                                      (0x00000414)
#define LW7476_VC1_DEBLOCK_SET_FILTER_STRENGTH_STRENGTH                             31:0
#define LW7476_VC1_DEBLOCK_PICTURE_TYPE_INTERLACEFIELD_BOT                          (3)
#define LW7476_VC1_DEBLOCK_PICTURE_TYPE_INTERLACEFIELD_TOP                          (2)
#define LW7476_VC1_DEBLOCK_PICTURE_TYPE_INTERLACEFRAME                              (1)
#define LW7476_VC1_DEBLOCK_PICTURE_TYPE_PROGRESSIVEFRAME                            (0)
#define LW7476_VC1_DEBLOCK_PICTURE_TYPE_INTERLACEFIELD                              (2)
#define LW7476_VC1_DEBLOCK_SET_PICTURE_TYPE                                         (0x00000418)
#define LW7476_VC1_DEBLOCK_SET_PICTURE_TYPE_TYPE                                    31:0
#define LW7476_VC1_DEBLOCK_APP_STATUS_DEBLOCK_START                                 (0x13000001)
#define LW7476_VC1_DEBLOCK_APP_STATUS_DEBLOCK_PROGRESSIVE                           (0x13000010)
#define LW7476_VC1_DEBLOCK_APP_STATUS_DEBLOCK_FLD_INT                               (0x13000011)
#define LW7476_VC1_DEBLOCK_APP_STATUS_DEBLOCK_FRM_INT                               (0x13000012)
#define LW7476_VC1_DEBLOCK_APP_STATUS_DEBLOCK_DONE                                  (0x13000013)
#define LW7476_VC1_DEBLOCK_APP_STATUS_DEBLOCK_END                                   (0x13000002)
#define LW7476_VC1_DEBLOCK_ERROR_PICTURE_BUFFER_INDEX_ILWALID                       (0x00004001)
#define LW7476_VC1_DEBLOCK_ERROR_PICTURE_BUFFER_TILING_MODE_ILWALID                 (0x00004002)
#define LW7476_VC1_DEBLOCK_ERROR_CONTROL_FLAG_INDEX_ILWALID                         (0x00004003)
#define LW7476_VC1_DEBLOCK_ERROR_BUFFER_ACCESS_FAILED                               (0x00004004)
#define LW7476_VC1_DEBLOCK_ERROR_PIC_HEIGHT_ZERO                                    (0x00004011)
#define LW7476_VC1_DEBLOCK_ERROR_PIC_WIDTH_ZERO                                     (0x00004012)
#define LW7476_VC1_DEBLOCK_ERROR_PICBUFFER_HEIGHT_NOT_MULT_OF_32                    (0x00004021)
#define LW7476_VC1_DEBLOCK_ERROR_PICBUFFER_WIDTH_NOT_MULT_OF_64                     (0x00004022)
#define LW7476_VC1_DEBLOCK_ERROR_PICBUFFER_HEIGHT_ZERO                              (0x00004023)
#define LW7476_VC1_DEBLOCK_ERROR_PICBUFFER_WIDTH_ZERO                               (0x00004024)
#define LW7476_VC1_DEBLOCK_ERROR_FILTER_STRENGTH_ZERO                               (0x00004031)
#define LW7476_VC1_DEBLOCK_ERROR_PICTURE_TYPE_ILWALID                               (0x00004041)

/* #define statements from class manual usr_vp2_scrambler.ref */
#define LW7476_SCRAMBLER_SET_CTX_DMA_INDEX                                          (0x00000400)
#define LW7476_SCRAMBLER_SET_CTX_DMA_INDEX_INPUT_BUFFER                             3:0
#define LW7476_SCRAMBLER_SET_CTX_DMA_INDEX_OUTPUT_BUFFER                            7:4
#define LW7476_SCRAMBLER_SET_BUFFER_IN_SIZE                                         (0x00000404)
#define LW7476_SCRAMBLER_SET_BUFFER_IN_SIZE_VALUE                                   31:0
#define LW7476_SCRAMBLER_SET_BUFFER_IN_OFFSET                                       (0x00000408)
#define LW7476_SCRAMBLER_SET_BUFFER_IN_OFFSET_VALUE                                 31:0
#define LW7476_SCRAMBLER_SET_BUFFER_OUT_OFFSET                                      (0x0000040c)
#define LW7476_SCRAMBLER_SET_BUFFER_OUT_OFFSET_VALUE                                31:0
#define LW7476_SCRAMBLER_SET_WRAPPING_KEY_0                                         (0x00000410)
#define LW7476_SCRAMBLER_SET_WRAPPING_KEY_1                                         (0x00000414)
#define LW7476_SCRAMBLER_SET_WRAPPING_KEY_2                                         (0x00000418)
#define LW7476_SCRAMBLER_SET_WRAPPING_KEY_3                                         (0x0000041c)
#define LW7476_SCRAMBLER_SET_WRAPPING_KEY_VALUE                                     31:0
#define LW7476_SCRAMBLER_SET_WRAPPED_SEED_0                                         (0x00000420)
#define LW7476_SCRAMBLER_SET_WRAPPED_SEED_1                                         (0x00000424)
#define LW7476_SCRAMBLER_SET_WRAPPED_SEED_2                                         (0x00000428)
#define LW7476_SCRAMBLER_SET_WRAPPED_SEED_3                                         (0x0000042c)
#define LW7476_SCRAMBLER_SET_WRAPPED_SEED_4                                         (0x00000430)
#define LW7476_SCRAMBLER_SET_WRAPPED_SEED_5                                         (0x00000434)
#define LW7476_SCRAMBLER_SET_WRAPPED_SEED_6                                         (0x00000438)
#define LW7476_SCRAMBLER_SET_WRAPPED_SEED_7                                         (0x0000043c)
#define LW7476_SCRAMBLER_SET_WRAPPED_SEED_8                                         (0x00000440)
#define LW7476_SCRAMBLER_SET_WRAPPED_SEED_9                                         (0x00000444)
#define LW7476_SCRAMBLER_SET_WRAPPED_SEED_10                                        (0x00000448)
#define LW7476_SCRAMBLER_SET_WRAPPED_SEED_11                                        (0x0000044c)
#define LW7476_SCRAMBLER_SET_WRAPPED_SEED_12                                        (0x00000450)
#define LW7476_SCRAMBLER_SET_WRAPPED_SEED_13                                        (0x00000454)
#define LW7476_SCRAMBLER_SET_WRAPPED_SEED_14                                        (0x00000458)
#define LW7476_SCRAMBLER_SET_WRAPPED_SEED_15                                        (0x0000045c)
#define LW7476_SCRAMBLER_SET_WRAPPED_SEED_16                                        (0x00000460)
#define LW7476_SCRAMBLER_SET_WRAPPED_SEED_17                                        (0x00000464)
#define LW7476_SCRAMBLER_SET_WRAPPED_SEED_18                                        (0x00000468)
#define LW7476_SCRAMBLER_SET_WRAPPED_SEED_19                                        (0x0000046c)
#define LW7476_SCRAMBLER_SET_WRAPPED_SEED_VALUE                                     31:0

/* #define statements from class manual usr_vp2_histogram_collection.ref */
#define LW7476_HISTOGRAM_COLLECTION_SET_CONTEXT_ID                                  (0x00000400)
#define LW7476_HISTOGRAM_COLLECTION_SET_CONTEXT_ID_PIC_ID                           3:0
#define LW7476_HISTOGRAM_COLLECTION_SET_CONTEXT_ID_PIC_TILE                         7:4
#define LW7476_HISTOGRAM_COLLECTION_SET_CONTEXT_ID_RSLT_ID                          11:8
#define LW7476_HISTOGRAM_COLLECTION_SET_YUV_IMAGE_FORMAT                            18:17
#define LW7476_HISTOGRAM_COLLECTION_SET_PICTURE_TYPE                                20:19
#define LW7476_HISTOGRAM_COLLECTION_SET_PIC_OFFSET                                  (0x00000404)
#define LW7476_HISTOGRAM_COLLECTION_SET_PIC_OFFSET_OFFSET                           31:0
#define LW7476_HISTOGRAM_COLLECTION_SET_PIC_DIMENSION                               (0x00000408)
#define LW7476_HISTOGRAM_COLLECTION_SET_PIC_DIMENSION_WIDTH                         15:0
#define LW7476_HISTOGRAM_COLLECTION_SET_PIC_DIMENSION_HEIGHT                        31:16
#define LW7476_HISTOGRAM_COLLECTION_SET_RESULT_OFFSET                               (0x00000434)
#define LW7476_HISTOGRAM_COLLECTION_SET_RESULT_OFFSET_VALUE                         31:0
#define LW7476_HISTOGRAM_COLLECTION_ERROR_PICTURE_INDEX_ILWALID                     (0x00004001)
#define LW7476_HISTOGRAM_COLLECTION_ERROR_PICTURE_TILING_MODE_ILWALID               (0x00004002)
#define LW7476_HISTOGRAM_COLLECTION_ERROR_PIC_HEIGHT_NOT_MULT_OF_16                 (0x00004011)
#define LW7476_HISTOGRAM_COLLECTION_ERROR_PIC_WIDTH_NOT_MULT_OF_16                  (0x00004012)
#define LW7476_HISTOGRAM_COLLECTION_ERROR_PIC_HEIGHT_ZERO                           (0x00004013)
#define LW7476_HISTOGRAM_COLLECTION_ERROR_PIC_WIDTH_ZERO                            (0x00004014)

#ifdef __cplusplus
};     /* extern "C" */
#endif

#endif /* _cl7476sc_h_ */
