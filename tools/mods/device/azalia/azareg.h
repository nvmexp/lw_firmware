/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef INCLUDED_AZA_REG_H
#define INCLUDED_AZA_REG_H

#define AZA                                 0x0003FFF:0x00000000 /* RW--D */
#define AZA_GCAP                                      0x00000000 /* R--4R */
#define AZA_GCAP_MAJ                                      31:24 /* C-IVF */
#define AZA_GCAP_MAJ_R1                              0x00000001 /* C-I-V */
#define AZA_GCAP_VMIN                                     23:16 /* C-IVF */
#define AZA_GCAP_VMIN_R0                             0x00000000 /* C-I-V */
#define AZA_GCAP_OSS                                      15:12 /* C-IVF */
#define AZA_GCAP_OSS_FOUR                            0x00000004 /* C-I-V */
#define AZA_GCAP_ISS                                       11:8 /* C-IVF */
#define AZA_GCAP_ISS_FOUR                            0x00000004 /* C-I-V */
#define AZA_GCAP_BSS                                        7:3 /* C-IVF */
#define AZA_GCAP_BSS_ZERO                            0x00000000 /* C-I-V */
#define AZA_GCAP_NSDO                                       1:1 /* C-IVF */
#define AZA_GCAP_NSDO_ONE                            0x00000000 /* C-I-V */
#define AZA_GCAP_NSDO_TWO                            0x00000001 /* ----V */
#define AZA_GCAP_64BIT_OK                                   0:0 /* C-IVF */
#define AZA_GCAP_64BIT_OK_YES                        0x00000001 /* C-I-V */
#define AZA_OUTPAY                                    0x00000004 /* R--4R */
#define AZA_OUTPAY_INPAY                                  22:16 /* C-IVF */
#define AZA_OUTPAY_INPAY_29WORDS                     0x0000001D /* C-I-V */
#define AZA_OUTPAY_OUTPAY                                   6:0 /* C-IVF */
#define AZA_OUTPAY_OUTPAY_60WORDS                    0x0000003C /* C-I-V */
#define AZA_GCTL                                      0x00000008 /* RW-4R */
#define AZA_GCTL_UNSOL                                      8:8 /* RWIVF */
#define AZA_GCTL_UNSOL_NOT_ACCEPTED                  0x00000000 /* RWI-V */
#define AZA_GCTL_UNSOL_ACCEPTED                      0x00000001 /* RW--V */
#define AZA_GCTL_FCNTRL                                     1:1 /* RWIVF */
#define AZA_GCTL_FCNTRL_DONT_INITIATE                0x00000000 /* RWI-V */
#define AZA_GCTL_FCNTRL_INITIATE                     0x00000001 /* RW--V */
#define AZA_GCTL_CRST                                       0:0 /* RWIVF */
#define AZA_GCTL_CRST_ASSERT                         0x00000000 /* RWI-V */
#define AZA_GCTL_CRST_DEASSERT                       0x00000001 /* RW--V */
#define AZA_WAKEEN                                    0x0000000C /* RW-4R */
#define AZA_WAKEEN_SDIWAKE2                               18:18 /* RWIVF */
#define AZA_WAKEEN_SDIWAKE2_NOWAKE                   0x00000000 /* R-I-V */
#define AZA_WAKEEN_SDIWAKE2_WAKE                     0x00000001 /* R---V */
#define AZA_WAKEEN_SDIWAKE2_W1C                      0x00000001 /* -W--C */
#define AZA_WAKEEN_SDIWAKE1                               17:17 /* RWIVF */
#define AZA_WAKEEN_SDIWAKE1_NOWAKE                   0x00000000 /* R-I-V */
#define AZA_WAKEEN_SDIWAKE1_WAKE                     0x00000001 /* R---V */
#define AZA_WAKEEN_SDIWAKE1_W1C                      0x00000001 /* -W--C */
#define AZA_WAKEEN_SDIWAKE0                               16:16 /* RWIVF */
#define AZA_WAKEEN_SDIWAKE0_NOWAKE                   0x00000000 /* R-I-V */
#define AZA_WAKEEN_SDIWAKE0_WAKE                     0x00000001 /* R---V */
#define AZA_WAKEEN_SDIWAKE0_W1C                      0x00000001 /* -W--C */
#define AZA_WAKEEN_SDIWEN2                                  2:2 /* RWIVF */
#define AZA_WAKEEN_SDIWEN2_DISABLE                   0x00000000 /* RWI-V */
#define AZA_WAKEEN_SDIWEN2_ENABLE                    0x00000001 /* RW--V */
#define AZA_WAKEEN_SDIWEN1                                  1:1 /* RWIVF */
#define AZA_WAKEEN_SDIWEN1_DISABLE                   0x00000000 /* RWI-V */
#define AZA_WAKEEN_SDIWEN1_ENABLE                    0x00000001 /* RW--V */
#define AZA_WAKEEN_SDIWEN0                                  0:0 /* RWIVF */
#define AZA_WAKEEN_SDIWEN0_DISABLE                   0x00000000 /* RWI-V */
#define AZA_WAKEEN_SDIWEN0_ENABLE                    0x00000001 /* RW--V */
#define AZA_GSTS                                      0x00000010 /* RW-4R */
#define AZA_GSTS_FSTS                                       1:1 /* RWIVF */
#define AZA_GSTS_FSTS_NOFLUSH                        0x00000000 /* R-I-V */
#define AZA_GSTS_FSTS_FLUSH                          0x00000001 /* R---V */
#define AZA_GSTS_FSTS_W1C                            0x00000001 /* -W--C */
#define AZA_OUTSTRMPAY                                0x00000018 /* R--4R */
#define AZA_OUTSTRMPAY_INSTRMPAY                          31:16 /* C-IVF */
#define AZA_OUTSTRMPAY_INSTRMPAY_NOLIMIT             0x00000000 /* C-I-V */
#define AZA_OUTSTRMPAY_OUTSTRMPAY                          15:0 /* C-IVF */
#define AZA_OUTSTRMPAY_OUTSTRMPAY_NOLIMIT            0x00000000 /* C-I-V */
#define AZA_INTCTL                                    0x00000020 /* RW-4R */
#define AZA_INTCTL_GIE                                    31:31 /* RWIVF */
#define AZA_INTCTL_GIE_DISABLE                       0x00000000 /* RWI-V */
#define AZA_INTCTL_GIE_ENABLE                        0x00000001 /* RW--V */
#define AZA_INTCTL_CIE                                    30:30 /* RWIVF */
#define AZA_INTCTL_CIE_DISABLE                       0x00000000 /* RWI-V */
#define AZA_INTCTL_CIE_ENABLE                        0x00000001 /* RW--V */
#define AZA_INTCTL_SIE7                                     7:7 /* RWIVF */
#define AZA_INTCTL_SIE7_DISABLE                      0x00000000 /* RWI-V */
#define AZA_INTCTL_SIE7_ENABLE                       0x00000001 /* RW--V */
#define AZA_INTCTL_SIE6                                     6:6 /* RWIVF */
#define AZA_INTCTL_SIE6_DISABLE                      0x00000000 /* RWI-V */
#define AZA_INTCTL_SIE6_ENABLE                       0x00000001 /* RW--V */
#define AZA_INTCTL_SIE5                                     5:5 /* RWIVF */
#define AZA_INTCTL_SIE5_DISABLE                      0x00000000 /* RWI-V */
#define AZA_INTCTL_SIE5_ENABLE                       0x00000001 /* RW--V */
#define AZA_INTCTL_SIE4                                     4:4 /* RWIVF */
#define AZA_INTCTL_SIE4_DISABLE                      0x00000000 /* RWI-V */
#define AZA_INTCTL_SIE4_ENABLE                       0x00000001 /* RW--V */
#define AZA_INTCTL_SIE3                                     3:3 /* RWIVF */
#define AZA_INTCTL_SIE3_DISABLE                      0x00000000 /* RWI-V */
#define AZA_INTCTL_SIE3_ENABLE                       0x00000001 /* RW--V */
#define AZA_INTCTL_SIE2                                     2:2 /* RWIVF */
#define AZA_INTCTL_SIE2_DISABLE                      0x00000000 /* RWI-V */
#define AZA_INTCTL_SIE2_ENABLE                       0x00000001 /* RW--V */
#define AZA_INTCTL_SIE1                                     1:1 /* RWIVF */
#define AZA_INTCTL_SIE1_DISABLE                      0x00000000 /* RWI-V */
#define AZA_INTCTL_SIE1_ENABLE                       0x00000001 /* RW--V */
#define AZA_INTCTL_SIE0                                     0:0 /* RWIVF */
#define AZA_INTCTL_SIE0_DISABLE                      0x00000000 /* RWI-V */
#define AZA_INTCTL_SIE0_ENABLE                       0x00000001 /* RW--V */
#define AZA_INTSTS                                    0x00000024 /* RW-4R */
#define AZA_INTSTS_GIS                                    31:31 /* R-IVF */
#define AZA_INTSTS_GIS_NO_INTR                       0x00000000 /* R-I-V */
#define AZA_INTSTS_GIS_INTR                          0x00000001 /* R---V */
#define AZA_INTSTS_CIS                                    30:30 /* R-IVF */
#define AZA_INTSTS_CIS_NO_INTR                       0x00000000 /* R-I-V */
#define AZA_INTSTS_CIS_INTR                          0x00000001 /* R---V */
#define AZA_INTSTS_SIS7                                     7:7 /* R-IVF */
#define AZA_INTSTS_SIS7_NOINT                        0x00000000 /* R-I-V */
#define AZA_INTSTS_SIS7_INT                          0x00000001 /* R---V */
#define AZA_INTSTS_SIS6                                     6:6 /* R-IVF */
#define AZA_INTSTS_SIS6_NOINT                        0x00000000 /* R-I-V */
#define AZA_INTSTS_SIS6_INT                          0x00000001 /* R---V */
#define AZA_INTSTS_SIS5                                     5:5 /* R-IVF */
#define AZA_INTSTS_SIS5_NOINT                        0x00000000 /* R-I-V */
#define AZA_INTSTS_SIS5_INT                          0x00000001 /* R---V */
#define AZA_INTSTS_SIS4                                     4:4 /* R-IVF */
#define AZA_INTSTS_SIS4_NOINT                        0x00000000 /* R-I-V */
#define AZA_INTSTS_SIS4_INT                          0x00000001 /* R---V */
#define AZA_INTSTS_SIS3                                     3:3 /* R-IVF */
#define AZA_INTSTS_SIS3_NOINT                        0x00000000 /* R-I-V */
#define AZA_INTSTS_SIS3_INT                          0x00000001 /* R---V */
#define AZA_INTSTS_SIS2                                     2:2 /* R-IVF */
#define AZA_INTSTS_SIS2_NOINT                        0x00000000 /* R-I-V */
#define AZA_INTSTS_SIS2_INT                          0x00000001 /* R---V */
#define AZA_INTSTS_SIS1                                     1:1 /* R-IVF */
#define AZA_INTSTS_SIS1_NOINT                        0x00000000 /* R-I-V */
#define AZA_INTSTS_SIS1_INT                          0x00000001 /* R---V */
#define AZA_INTSTS_SIS0                                     0:0 /* R-IVF */
#define AZA_INTSTS_SIS0_NOINT                        0x00000000 /* R-I-V */
#define AZA_INTSTS_SIS0_INT                          0x00000001 /* R---V */
#define AZA_WALCLK                                    0x00000030 /* R--4R */
#define AZA_WALCLK_COUNTER                                 31:0 /* R-IVF */
#define AZA_WALCLK_COUNTER_INIT                      0x00000000 /* R-I-V */
#define AZA_SSYNC                                     0x00000038 /* RW-4R */
#define AZA_SSYNC_SSYNC7                                    7:7 /* RWIVF */
#define AZA_SSYNC_SSYNC7_NOBLOCK                     0x00000000 /* RWI-V */
#define AZA_SSYNC_SSYNC7_BLOCK                       0x00000001 /* RW--V */
#define AZA_SSYNC_SSYNC6                                    6:6 /* RWIVF */
#define AZA_SSYNC_SSYNC6_NOBLOCK                     0x00000000 /* RWI-V */
#define AZA_SSYNC_SSYNC6_BLOCK                       0x00000001 /* RW--V */
#define AZA_SSYNC_SSYNC5                                    5:5 /* RWIVF */
#define AZA_SSYNC_SSYNC5_NOBLOCK                     0x00000000 /* RWI-V */
#define AZA_SSYNC_SSYNC5_BLOCK                       0x00000001 /* RW--V */
#define AZA_SSYNC_SSYNC4                                    4:4 /* RWIVF */
#define AZA_SSYNC_SSYNC4_NOBLOCK                     0x00000000 /* RWI-V */
#define AZA_SSYNC_SSYNC4_BLOCK                       0x00000001 /* RW--V */
#define AZA_SSYNC_SSYNC3                                    3:3 /* RWIVF */
#define AZA_SSYNC_SSYNC3_NOBLOCK                     0x00000000 /* RWI-V */
#define AZA_SSYNC_SSYNC3_BLOCK                       0x00000001 /* RW--V */
#define AZA_SSYNC_SSYNC2                                    2:2 /* RWIVF */
#define AZA_SSYNC_SSYNC2_NOBLOCK                     0x00000000 /* RWI-V */
#define AZA_SSYNC_SSYNC2_BLOCK                       0x00000001 /* RW--V */
#define AZA_SSYNC_SSYNC1                                    1:1 /* RWIVF */
#define AZA_SSYNC_SSYNC1_NOBLOCK                     0x00000000 /* RWI-V */
#define AZA_SSYNC_SSYNC1_BLOCK                       0x00000001 /* RW--V */
#define AZA_SSYNC_SSYNC0                                    0:0 /* RWIVF */
#define AZA_SSYNC_SSYNC0_NOBLOCK                     0x00000000 /* RWI-V */
#define AZA_SSYNC_SSYNC0_BLOCK                       0x00000001 /* RW--V */
#define AZA_CORBLBASE                                 0x00000040 /* RW-4R */
#define AZA_CORBLBASE_CORBLBASE                            31:7 /* RWIVF */
#define AZA_CORBLBASE_CORBLBASE_INIT                 0x00000000 /* RWI-V */
#define AZA_CORBUBASE                                 0x00000044 /* RW-4R */
#define AZA_CORBUBASE_CORBUBASE                            31:0 /* RWIVF */
#define AZA_CORBUBASE_CORBUBASE_INIT                 0x00000000 /* RWI-V */
#define AZA_CORBWP                                    0x00000048 /* RW-4R */
#define AZA_CORBWP_RST                                    31:31 /* -WIVF */
#define AZA_CORBWP_RST_INIT                                 0x0 /* --I-V */
#define AZA_CORBWP_CORBRP                                 23:16 /* R-IVF */
#define AZA_CORBWP_CORBRP_INIT                       0x00000000 /* R-I-V */
#define AZA_CORBWP_CORBWP                                   7:0 /* RWIVF */
#define AZA_CORBWP_CORBWP_INIT                       0x00000000 /* RWI-V */
#define AZA_CORBCTL                                   0x0000004C /* RW-4R */
#define AZA_CORBCTL_CORBSZCAP                             23:20 /* C-IVF */
#define AZA_CORBCTL_CORBSZCAP_INIT                   0x00000004 /* C-I-V */
#define AZA_CORBCTL_CORBSIZE                              17:16 /* C-IVF */
#define AZA_CORBCTL_CORBSIZE_2ENTRIES                0x00000000 /* ----V */
#define AZA_CORBCTL_CORBSIZE_16ENTRIES               0x00000001 /* ----V */
#define AZA_CORBCTL_CORBSIZE_256ENTRIES              0x00000002 /* C-I-V */
#define AZA_CORBCTL_CMEI                                    8:8 /* RWIVF */
#define AZA_CORBCTL_CMEI_NOERROR                     0x00000000 /* R-I-V */
#define AZA_CORBCTL_CMEI_ERROR                       0x00000001 /* R---V */
#define AZA_CORBCTL_CMEI_W1C                         0x00000001 /* -W--C */
#define AZA_CORBCTL_CORBRUN                                 1:1 /* RWIVF */
#define AZA_CORBCTL_CORBRUN_STOP                     0x00000000 /* RWI-V */
#define AZA_CORBCTL_CORBRUN_RUN                      0x00000001 /* RW--V */
#define AZA_CORBCTL_CMEIE                                   0:0 /* RWIVF */
#define AZA_CORBCTL_CMEIE_DISABLE                    0x00000000 /* RWI-V */
#define AZA_CORBCTL_CMEIE_ENABLE                     0x00000001 /* RW--V */
#define AZA_RIRBLBASE                                 0x00000050 /* RW-4R */
#define AZA_RIRBLBASE_RIRBLBASE                            31:7 /* RWIVF */
#define AZA_RIRBLBASE_RIRBLBASE_INIT                 0x00000000 /* RWI-V */
#define AZA_RIRBUBASE                                 0x00000054 /* RW-4R */
#define AZA_RIRBUBASE_RIRBUBASE                            31:0 /* RWIVF */
#define AZA_RIRBUBASE_RIRBUBASE_INIT                 0x00000000 /* RWI-V */
#define AZA_RIRBWP                                    0x00000058 /* RW-4R */
#define AZA_RIRBWP_RINTCNT                                23:16 /* RWIVF */
#define AZA_RIRBWP_RINTCNT_INIT                      0x00000000 /* RWI-V */
#define AZA_RIRBWP_RIRBWPRST                              15:15 /* -W-VF */
#define AZA_RIRBWP_RIRBWPRST_INIT                    0x00000000 /* ----V */
#define AZA_RIRBWP_RIRBWPRST_RESET_WP                0x00000001 /* -W--V */
#define AZA_RIRBWP_RIRBWP                                   7:0 /* R-IVF */
#define AZA_RIRBWP_RIRBWP_INIT                       0x00000000 /* R-I-V */
#define AZA_RIRBCTL                                   0x0000005C /* RW-4R */
#define AZA_RIRBCTL_RIRBSZCAP                             23:20 /* C-IVF */
#define AZA_RIRBCTL_RIRBSZCAP_INIT                   0x00000004 /* C-I-V */
#define AZA_RIRBCTL_RIRBSIZE                              17:16 /* C-IVF */
#define AZA_RIRBCTL_RIRBSIZE_2ENTRIES                0x00000000 /* ----V */
#define AZA_RIRBCTL_RIRBSIZE_16ENTRIES               0x00000001 /* ----V */
#define AZA_RIRBCTL_RIRBSIZE_256ENTRIES              0x00000002 /* C-I-V */
#define AZA_RIRBCTL_RIRBOIS                               10:10 /* RWIVF */
#define AZA_RIRBCTL_RIRBOIS_NOOVERRUN                0x00000000 /* R-I-V */
#define AZA_RIRBCTL_RIRBOIS_OVERRUN                  0x00000001 /* R---V */
#define AZA_RIRBCTL_RIRBOIS_W1C                      0x00000001 /* -W--C */
#define AZA_RIRBCTL_RINTFL                                  8:8 /* RWIVF */
#define AZA_RIRBCTL_RINTFL_NORESPONSE                0x00000000 /* R-I-V */
#define AZA_RIRBCTL_RINTFL_RESPONSE                  0x00000001 /* R---V */
#define AZA_RIRBCTL_RINTFL_W1C                       0x00000001 /* -W--C */
#define AZA_RIRBCTL_RIRBOIC                                 2:2 /* RWIVF */
#define AZA_RIRBCTL_RIRBOIC_NOINTR                   0x00000000 /* RWI-V */
#define AZA_RIRBCTL_RIRBOIC_INTR                     0x00000001 /* RW--V */
#define AZA_RIRBCTL_RIRBDMAEN                               1:1 /* RWIVF */
#define AZA_RIRBCTL_RIRBDMAEN_STOP                   0x00000000 /* RWI-V */
#define AZA_RIRBCTL_RIRBDMAEN_RUN                    0x00000001 /* RW--V */
#define AZA_RIRBCTL_RINTCTL                                 0:0 /* RWIVF */
#define AZA_RIRBCTL_RINTCTL_DISABLE                  0x00000000 /* RWI-V */
#define AZA_RIRBCTL_RINTCTL_ENABLE                   0x00000001 /* RW--V */
#define AZA_ICOI                                      0x00000060 /* -W-4R */
#define AZA_ICOI_ICW                                       31:0 /* -WIVF */
#define AZA_ICOI_ICW_INIT                            0x00000000 /* -WI-V */
#define AZA_IRII                                      0x00000064 /* R--4R */
#define AZA_IRII_IRR                                       31:0 /* R-IVF */
#define AZA_IRII_IRR_INIT                            0x00000000 /* R-I-V */
#define AZA_ICS                                       0x00000068 /* RW-4R */
#define AZA_ICS_IRRADD                                      7:4 /* R-IVF */
#define AZA_ICS_IRRADD_INIT                          0x00000000 /* R-I-V */
#define AZA_ICS_IRRUNSOL                                    3:3 /* R-IVF */
#define AZA_ICS_IRRUNSOL_INIT                        0x00000000 /* R-I-V */
#define AZA_ICS_ICVER                                       2:2 /* R-IVF */
#define AZA_ICS_ICVER_DEFAULT                        0x00000001 /* R-I-V */
#define AZA_ICS_IRV                                         1:1 /* RWIVF */
#define AZA_ICS_IRV_INIT                             0x00000000 /* R-I-V */
#define AZA_ICS_IRV_W1C                              0x00000001 /* -W--C */
#define AZA_ICS_ICB                                         0:0 /* R-IVF */
#define AZA_ICS_ICB_INIT                             0x00000000 /* R-I-V */
#define AZA_DPLBASE                                   0x00000070 /* RW-4R */
#define AZA_DPLBASE_DPLBASE                                31:7 /* RWIVF */
#define AZA_DPLBASE_DPLBASE_INIT                     0x00000000 /* RWI-V */
#define AZA_DPLBASE_ENABLE                                  0:0 /* RWIVF */
#define AZA_DPLBASE_ENABLE_INIT                      0x00000000 /* RWI-V */
#define AZA_DPUBASE                                   0x00000074 /* RW-4R */
#define AZA_DPUBASE_DPUBASE                                31:0 /* RWIVF */
#define AZA_DPUBASE_DPUBASE_INIT                     0x00000000 /* RWI-V */
#define AZA_OB_BUFSZ_NUM_OF_FRAMES                   0x00000080 /* RW-4R */
#define AZA_OB_BUFSZ_NUM_OF_FRAMES_8CHAN                  21:16 /* RWIVF */
#define AZA_OB_BUFSZ_NUM_OF_FRAMES_8CHAN_MIN               0x04 /* RW--V */
#define AZA_OB_BUFSZ_NUM_OF_FRAMES_8CHAN_MAX               0x0A /* RW--V */
#define AZA_OB_BUFSZ_NUM_OF_FRAMES_6CHAN                   13:8 /* RWIVF */
#define AZA_OB_BUFSZ_NUM_OF_FRAMES_6CHAN_MIN               0x04 /* RW--V */
#define AZA_OB_BUFSZ_NUM_OF_FRAMES_6CHAN_MAX               0x0D /* RW--V */
#define AZA_OB_BUFSZ_NUM_OF_FRAMES_2CHAN                    5:0 /* RWIVF */
#define AZA_OB_BUFSZ_NUM_OF_FRAMES_2CHAN_MIN               0x04 /* RW--V */
#define AZA_OB_BUFSZ_NUM_OF_FRAMES_2CHAN_MAX               0x28 /* RW--V */

#define AZA_OB_STR_FIFO_MONITOR_EN                   0x000000A4 /* RW-4R */
#define AZA_OB_STR_FIFO_MONITOR_EN_STR(i)                   i:i /* RWIVF */

#define AZA_OB_STR_FIFO_MONITOR0                     0x000000A8 /* R--4R */
#define AZA_OB_STR_FIFO_MONITOR0_STR1                     31:16 /* R-IVF */
#define AZA_OB_STR_FIFO_MONITOR0_STR0                      15:0 /* R-IVF */
#define AZA_OB_STR_FIFO_MONITOR1                     0x000000AC /* R--4R */
#define AZA_OB_STR_FIFO_MONITOR1_STR3                     31:16 /* R-IVF */
#define AZA_OB_STR_FIFO_MONITOR1_STR2                      15:0 /* R-IVF */
#define AZA_OB_STR_FIFO_MONITOR2                     0x00000074 /* R--4R */
#define AZA_OB_STR_FIFO_MONITOR2_STR5                     31:16 /* R-IVF */
#define AZA_OB_STR_FIFO_MONITOR2_STR4                      15:0 /* R-IVF */

#define AZA_SDCTL(i)                      (0x00000080+(i)*0x20) /* RW-4A */
#define AZA_SDCTL__SIZE_1                                     8 /*       */
#define AZA_SDCTL_FIFORDY                                 29:29 /* R-IVF */
#define AZA_SDCTL_FIFORDY_NOTREADY                   0x00000000 /* R-I-V */
#define AZA_SDCTL_FIFORDY_READY                      0x00000001 /* R---V */
#define AZA_SDCTL_DESE                                    28:28 /* RWIVF */
#define AZA_SDCTL_DESE_NOERROR                       0x00000000 /* R-I-V */
#define AZA_SDCTL_DESE_ERROR                         0x00000001 /* R---V */
#define AZA_SDCTL_DESE_W1C                           0x00000001 /* -W--C */
#define AZA_SDCTL_FIFOE                                   27:27 /* RWIVF */
#define AZA_SDCTL_FIFOE_NOERROR                      0x00000000 /* R-I-V */
#define AZA_SDCTL_FIFOE_ERROR                        0x00000001 /* R---V */
#define AZA_SDCTL_FIFOE_W1C                          0x00000001 /* -W--C */
#define AZA_SDCTL_BCIS                                    26:26 /* RWIVF */
#define AZA_SDCTL_BCIS_NOTCOMPLETE                   0x00000000 /* R-I-V */
#define AZA_SDCTL_BCIS_COMPLETE                      0x00000001 /* R---V */
#define AZA_SDCTL_BCIS_W1C                           0x00000001 /* -W--C */
#define AZA_SDCTL_STRM                                    23:20 /* RWIVF */
#define AZA_SDCTL_STRM_UNUSED                        0x00000000 /* RWI-V */
#define AZA_SDCTL_DIR                                     19:19 /* C-IVF */
#define AZA_SDCTL_DIR_UNIDIR                         0x00000000 /* C-I-V */
#define AZA_SDCTL_DIR_INPUT                          0x00000000 /* ----V */
#define AZA_SDCTL_DIR_OUTPUT                         0x00000001 /* ----V */
#define AZA_SDCTL_TP                                      18:18 /* RWIVF */
#define AZA_SDCTL_TP_LOW                             0x00000000 /* RWI-V */
#define AZA_SDCTL_TP_HIGH                            0x00000001 /* RW--V */
#define AZA_SDCTL_STRIPE                                  17:16 /* RWIVF */
#define AZA_SDCTL_STRIPE_1SDO                        0x00000000 /* RWI-V */
#define AZA_SDCTL_STRIPE_2SDO                        0x00000001 /* RW--V */
#define AZA_SDCTL_DEIE                                      4:4 /* RWIVF */
#define AZA_SDCTL_DEIE_DISABLE                       0x00000000 /* RWI-V */
#define AZA_SDCTL_DEIE_ENABLE                        0x00000001 /* RW--V */
#define AZA_SDCTL_FEIE                                      3:3 /* RWIVF */
#define AZA_SDCTL_FEIE_DISABLE                       0x00000000 /* RWI-V */
#define AZA_SDCTL_FEIE_ENABLE                        0x00000001 /* RW--V */
#define AZA_SDCTL_IOCE                                      2:2 /* RWIVF */
#define AZA_SDCTL_IOCE_DISABLE                       0x00000000 /* RWI-V */
#define AZA_SDCTL_IOCE_ENABLE                        0x00000001 /* RW--V */
#define AZA_SDCTL_RUN                                       1:1 /* RWIVF */
#define AZA_SDCTL_RUN_STOP                           0x00000000 /* RWI-V */
#define AZA_SDCTL_RUN_RUN                            0x00000001 /* RW--V */
#define AZA_SDCTL_SRST                                      0:0 /* RWIVF */
#define AZA_SDCTL_SRST_NOTRESET                      0x00000000 /* RWI-V */
#define AZA_SDCTL_SRST_RESET                         0x00000001 /* RW--V */
#define AZA_SDPICB(i)                     (0x00000084+(i)*0x20) /* R--4A */
#define AZA_SDPICB__SIZE_1                                    8 /*       */
#define AZA_SDPICB_LPIB                                    31:0 /* R-IVF */
#define AZA_SDPICB_LPIB_INIT                         0x00000000 /* R-I-V */
#define AZA_SDCBL(i)                      (0x00000088+(i)*0x20) /* RW-4A */
#define AZA_SDCBL__SIZE_1                                     8 /*       */
#define AZA_SDCBL_CBL                                      31:0 /* RWIVF */
#define AZA_SDCBL_CBL_INIT                           0x00000000 /* RWI-V */
#define AZA_SDLVI(i)                      (0x0000008C+(i)*0x20) /* RW-4A */
#define AZA_SDLVI__SIZE_1                                     8 /*       */
#define AZA_SDLVI_LVI                                       7:0 /* RWIVF */
#define AZA_SDLVI_LVI_INIT                           0x00000000 /* RWI-V */
#define AZA_SDFMT(i)                      (0x00000090+(i)*0x20) /* R--4A */
#define AZA_SDFMT__SIZE_1                                     8 /*       */
#define AZA_SDFMT_BASE                                    30:30 /* RWIVF */
#define AZA_SDFMT_BASE_48KHZ                         0x00000000 /* RWI-V */
#define AZA_SDFMT_BASE_44KHZ                         0x00000001 /* RW--V */
#define AZA_SDFMT_MULT                                    29:27 /* RWIVF */
#define AZA_SDFMT_MULT_X1                            0x00000000 /* RWI-V */
#define AZA_SDFMT_MULT_X2                            0x00000001 /* RW--V */
#define AZA_SDFMT_MULT_X3                            0x00000002 /* RW--V */
#define AZA_SDFMT_MULT_X4                            0x00000003 /* RW--V */
#define AZA_SDFMT_DIV                                     26:24 /* RWIVF */
#define AZA_SDFMT_DIV_X1                             0x00000000 /* RWI-V */
#define AZA_SDFMT_DIV_X2                             0x00000001 /* RW--V */
#define AZA_SDFMT_DIV_X3                             0x00000002 /* RW--V */
#define AZA_SDFMT_DIV_X4                             0x00000003 /* RW--V */
#define AZA_SDFMT_DIV_X5                             0x00000004 /* RW--V */
#define AZA_SDFMT_DIV_X6                             0x00000005 /* RW--V */
#define AZA_SDFMT_DIV_X7                             0x00000006 /* RW--V */
#define AZA_SDFMT_DIV_X8                             0x00000007 /* RW--V */
#define AZA_SDFMT_BITS                                    22:20 /* RWIVF */
#define AZA_SDFMT_BITS_8BITS                         0x00000000 /* RWI-V */
#define AZA_SDFMT_BITS_16BITS                        0x00000001 /* RW--V */
#define AZA_SDFMT_BITS_20BITS                        0x00000002 /* RW--V */
#define AZA_SDFMT_BITS_24BITS                        0x00000003 /* RW--V */
#define AZA_SDFMT_BITS_32BITS                        0x00000004 /* RW--V */
#define AZA_SDFMT_CHAN                                    19:16 /* RWIVF */
#define AZA_SDFMT_CHAN_1CHAN                         0x00000000 /* RWI-V */
#define AZA_SDFMT_FIFOS                                    15:0 /* R-IVF */
#define AZA_SDFMT_FIFOS_INIT                         0x00000020 /* R-I-V */
#define AZA_SDBDPL(i)                     (0x00000098+(i)*0x20) /* RW-4A */
#define AZA_SDBDPL__SIZE_1                                    8 /*       */
#define AZA_SDBDPL_BDLLBASE                                31:7 /* RWIVF */
#define AZA_SDBDPL_BDLLBASE_INIT                     0x00000000 /* RWI-V */
#define AZA_SDBDPU(i)                     (0x0000009C+(i)*0x20) /* RW-4A */
#define AZA_SDBDPU__SIZE_1                                    8 /*       */
#define AZA_SDBDPU_BDLUBASE                                31:0 /* RWIVF */
#define AZA_SDBDPU_BDLUBASE_INIT                     0x00000000 /* RWI-V */
#define AZA_WALCLKA                                   0x00002030 /* R--4R */
#define AZA_WALCLKA_COUNTER                                31:0 /* R-IVF */
#define AZA_WALCLKA_COUNTER_INIT                     0x00000000 /* R-I-V */
#define AZA_SDLPIBA(i)                    (0x00002084+(i)*0x20) /* R--4A */
#define AZA_SDLPIBA__SIZE_1                                   8 /*       */
#define AZA_SDLPIBA_LPIBA                                  31:0 /* R-IVF */
#define AZA_SDLPIBA_LPIBA_INIT                       0x00000000 /* R-I-V */

#define AZA_PCI_CFG_18                               0x00000048 /* RW-4R */
#define AZA_PCI_CFG_18_PME_STATUS                         15:15 /* RWXVF */
#define AZA_PCI_CFG_18_PME_STATUS_NOT_ACTIVE         0x00000000 /* R---V */
#define AZA_PCI_CFG_18_PME_STATUS_ACTIVE             0x00000001 /* R---V */
#define AZA_PCI_CFG_18_PME_STATUS_WRITE_CLEAR        0x00000001 /* -W--C */
#define AZA_PCI_CFG_18_DATA_SCALE                         14:13 /* C-IVF */
#define AZA_PCI_CFG_18_DATA_SCALE_NONE               0x00000000 /* C-I-V */
#define AZA_PCI_CFG_18_DATA_SELECT                         12:9 /* C-IVF */
#define AZA_PCI_CFG_18_DATA_SELECT_NONE              0x00000000 /* C-I-V */
#define AZA_PCI_CFG_18_PME                                  8:8 /* RWXVF */
#define AZA_PCI_CFG_18_PME_DISABLE                   0x00000000 /* RW--V */
#define AZA_PCI_CFG_18_PME_ENABLE                    0x00000001 /* RW--V */
#define AZA_PCI_PCI_CFG_18_PM_STATE                         1:0 /* RWIVF */
#define AZA_PCI_CFG_18_PM_STATE_D0                   0x00000000 /* RWI-V */
#define AZA_PCI_CFG_18_PM_STATE_D1                   0x00000001 /* RW--V */
#define AZA_PCI_CFG_18_PM_STATE_D2                   0x00000002 /* RW--V */
#define AZA_PCI_CFG_18_PM_STATE_D3                   0x00000003 /* RW--V */

// CheetAh specific defines
#define T30_HDA_DFPCI_BEN_0                              0x000001B8
#define T30_HDA_DFPCI_BEN_0_EN_DFPCI_BEN                      31:31

// Need to enable STR5 to get the correct FIFO size
#define AZA_OBCYA_STR_FIFO_MONITOR_EN              0x000000A4 /* RW-4R */
#define AZA_OBCTL_STR_FIFO_MONITOR_EN              0x000000A4 /* RW-4R */
#define AZA_OBCYA_STR_FIFO_MONITOR_EN_STR5                5:5 /* RWIVF */
#define AZA_OBCYA_STR_FIFO_MONITOR_EN_STR5_DIS     0x00000000 /* RWI-V */

// AZALIA CONTROLLER REGISTER CODES
// Some Azalia registers are addressable in non-word alignments. We define
// constants here that should be used throughout the code to address the
// Azalia registers.
//
// NOTE: the ref manuals still define the registers in 32-bit chunks. In
// order to keep the bit values from the ref files valid (and thus allow
// us to use the drf macros), register reads and writes will still be
// offset in accordance with the values in the ref files. So, for
// example, if VMIN is set to 0xff in hardware, then RegRd(VMIN) will
// return 0x00ff0000.
#define GCAP        AZA_GCAP
#define VMIN        (AZA_GCAP + 2)
#define VMAJ        (AZA_GCAP + 3)
#define OUTPAY      AZA_OUTPAY
#define INPAY       (AZA_OUTPAY + 2)
#define GCTL        AZA_GCTL
#define WAKEEN      AZA_WAKEEN
#define STATESTS    (AZA_WAKEEN + 2)
#define GSTS        AZA_GSTS
#define OUTSTRMPAY  AZA_OUTSTRMPAY
#define INSTRMPAY   (AZA_OUTSTRMPAY + 2)
#define INTCTL      AZA_INTCTL
#define INTSTS      AZA_INTSTS
#define WALCLK      AZA_WALCLK
#define SSYNC       AZA_SSYNC
#define CORBLBASE   AZA_CORBLBASE
#define CORBUBASE   AZA_CORBUBASE
#define CORBWP      AZA_CORBWP
#define CORBRP      (AZA_CORBWP + 2)
#define CORBCTL     AZA_CORBCTL
#define CORBSTS     (AZA_CORBCTL + 1)
#define CORBSIZE    (AZA_CORBCTL + 2)
#define RIRBLBASE   AZA_RIRBLBASE
#define RIRBUBASE   AZA_RIRBUBASE
#define RIRBWP      AZA_RIRBWP
#define RINTCNT     (AZA_RIRBWP + 2)
#define RIRBCTL     AZA_RIRBCTL
#define RIRBSTS     (AZA_RIRBCTL + 1)
#define RIRBSIZE    (AZA_RIRBCTL + 2)
#define ICOI        AZA_ICOI
#define IRII        AZA_IRII
#define ICS         AZA_ICS
#define DPIBLBASE   AZA_DPLBASE
#define DPIBUBASE   AZA_DPUBASE
#define OB_BUFSZ_NUM_FRAMES AZA_OB_BUFSZ_NUM_OF_FRAMES
#define SD_CTLX(i)  AZA_SDCTL(i)
#define SD_CTLY(i)  (AZA_SDCTL(i) + 2)
#define SD_STS(i)   (AZA_SDCTL(i) + 3)
#define SD_PICB(i)  AZA_SDPICB(i)
#define SD_CBL(i)   AZA_SDCBL(i)
#define SD_LVI(i)   AZA_SDLVI(i)
#define SD_FIFOD(i) AZA_SDFMT(i)
#define SD_FMT(i)   (AZA_SDFMT(i) + 2)
#define SD_BDPL(i)  AZA_SDBDPL(i)
#define SD_BDPU(i)  AZA_SDBDPU(i)
#define WALCLKA     AZA_WALCLKA
#define SDLPIBA(i)  AZA_SDLPIBA(i)
#endif // INCLUDED_AZA_REG_H
