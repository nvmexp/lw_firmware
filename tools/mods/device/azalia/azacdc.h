/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_AZACDC_H
#define INCLUDED_AZACDC_H

#include "azadmae.h"
#include "azart.h"
#include <vector>
#include <list>

class AzaliaController;

// Vendor and code IDs for various known codecs.
// Codec Ids are obtained from relevant hw contact (Ex: Govendra Gupta)
#define AZA_CDC_VENDOR_ADI         0x11d4
#define AZA_CDC_VENDOR_SIGMATEL    0x8384
#define AZA_CDC_VENDOR_REALTEK     0x10ec
#define AZA_CDC_VENDOR_CMEDIA      0x434d
#define AZA_CDC_VENDOR_CONEXANT    0x14f1
#define AZA_CDC_VENDOR_CIRRUSLOGIC 0x1013
#define AZA_CDC_VENDOR_LWIDIA      0x10de
#define AZA_CDC_VENDOR_IDT         0x111d
#define AZA_CDC_VENDOR_VIA         0x1106
#define AZA_CDC_ADI_1986           0x0078
#define AZA_CDC_ADI_1986A          0x1986
#define AZA_CDC_ADI_1988A          0x1988
#define AZA_CDC_SIGMATEL_STAC9770  0x7670
#define AZA_CDC_SIGMATEL_STAC9772  0x7672
#define AZA_CDC_SIGMATEL_STAC9220  0x7680
#define AZA_CDC_REALTEK_ALC260     0x0260
#define AZA_CDC_REALTEK_ALC262     0x0262
#define AZA_CDC_REALTEK_ALC269     0x0269
#define AZA_CDC_REALTEK_ALC275     0x0275
#define AZA_CDC_REALTEK_ALC663     0x0663
#define AZA_CDC_REALTEK_ALC670     0x0670
#define AZA_CDC_REALTEK_ALC880     0x0880
#define AZA_CDC_REALTEK_ALC882     0x0882
#define AZA_CDC_REALTEK_ALC883     0x0883
#define AZA_CDC_REALTEK_ALC885     0x0885
#define AZA_CDC_REALTEK_ALC888     0x0888
#define AZA_CDC_REALTEK_ALC889     0x0889
#define AZA_CDC_CMEDIA_CMI9880     0x4980
#define AZA_CDC_CONEXANT_2054912Z  0x5045
#define AZA_CDC_CIRRUSLOGIC_C4207  0x4207
#define AZA_CDC_IDT_92HD81         0x7605
#define AZA_CDC_IDT_92HD81         0x7605
#define AZA_CDC_IDT_92HD89C3X5     0x76c0
#define AZA_CDC_VIA_VT1828S        0x4441
#define AZA_CDC_VIA_VT1812S        0x0448
#define AZA_CDC_VIA_VT2002P        0x0438

typedef struct
{
    const char* name;
    UINT16 vendorID;
    UINT16 deviceID;
    UINT32 output; // default output pin
    UINT32 input;  // default input pin
    UINT32 listening; // default pin for listening on manual tests that need headphones. Ideally this should be different from the output pin.
    FLOAT32 attenuation;
} CodecDefaultInformation;

const CodecDefaultInformation pCodecDefaults[] =
{
    {
        "UNKNOWN", 0xffff, 0x0,
        0x14, 0x18, 0x15, 1.0f
    }
    ,{
        "CMedia CMI9880", AZA_CDC_VENDOR_CMEDIA, AZA_CDC_CMEDIA_CMI9880,
        0xd, 0xb, 0xc, 1.0f
    }
    ,{
        // Output--colwerter 3 can't get to 0x1d, can't get sound out of 0x1a!
        // Listen--broken, but any other choice ends up breaking loopback
        "ADI1986", AZA_CDC_VENDOR_ADI, AZA_CDC_ADI_1986,
        0x1b, 0x20, 0x1d, 1.0f
    }
    ,{
        // Output--can't get sound out of 0x1b
        "ADI1986a", AZA_CDC_VENDOR_ADI, AZA_CDC_ADI_1986A,
        0x1a, 0x20, 0x1d, 1.0f
    }
    ,{
        "ADI1988a", AZA_CDC_VENDOR_ADI, AZA_CDC_ADI_1988A,
        0x11, 0x14, 0x14, 1.0f
    }
    ,{
        "RealTek ALC260", AZA_CDC_VENDOR_REALTEK, AZA_CDC_REALTEK_ALC260,
        0x10, 0x12, 0x15, 1.0f
    }
    ,{
        "RealTek ALC262", AZA_CDC_VENDOR_REALTEK, AZA_CDC_REALTEK_ALC262,
        0x14, 0x18, 0x15, 0.8f
    }
    ,{
        "RealTek ALC269", AZA_CDC_VENDOR_REALTEK, AZA_CDC_REALTEK_ALC269,
        0x14, 0x18, 0x15, 0.8f
    }
    ,{
        "RealTek ALC275", AZA_CDC_VENDOR_REALTEK, AZA_CDC_REALTEK_ALC275,
        0x15, 0x18, 0x15, 0.8f
    }
    ,{
        "RealTek ALC663", AZA_CDC_VENDOR_REALTEK, AZA_CDC_REALTEK_ALC663,
        0x14, 0x18, 0x15, 0.8f
    }
    ,{
        "RealTek ALC670", AZA_CDC_VENDOR_REALTEK, AZA_CDC_REALTEK_ALC670,
        0x15, 0x18, 0x15, 0.8f
    }
    ,{
        "RealTek ALC880", AZA_CDC_VENDOR_REALTEK, AZA_CDC_REALTEK_ALC880,
        0x14, 0x18, 0x15, 0.707f
    }
    ,{
        "RealTek ALC882", AZA_CDC_VENDOR_REALTEK, AZA_CDC_REALTEK_ALC882,
        0x14, 0x18, 0x15, 1.0f
    }
    ,{
        "RealTek ALC883", AZA_CDC_VENDOR_REALTEK, AZA_CDC_REALTEK_ALC883,
        0x14, 0x18, 0x15, 1.0f
    }
    ,{
        "RealTek ALC885", AZA_CDC_VENDOR_REALTEK, AZA_CDC_REALTEK_ALC885,
        0x14, 0x18, 0x15, 1.0f
    }
    ,{
        "RealTek ALC888", AZA_CDC_VENDOR_REALTEK, AZA_CDC_REALTEK_ALC888,
        0x14, 0x18, 0x15, 1.0f
    }
    ,{
        "RealTek ALC889", AZA_CDC_VENDOR_REALTEK, AZA_CDC_REALTEK_ALC889,
        0x14, 0x18, 0x15, 1.0f
    }
    ,{
        "Sigmatel 9770",  AZA_CDC_VENDOR_SIGMATEL, AZA_CDC_SIGMATEL_STAC9770,
        0xe, 0xf, 0xd, 1.0f
    }
    ,{
        // Listening--only one DAC so not very useful
        "Sigmatel 9772",  AZA_CDC_VENDOR_SIGMATEL, AZA_CDC_SIGMATEL_STAC9772,
        0xe, 0xf, 0xd, 1.0f
    }
    ,{
        "Sigmatel 9220",  AZA_CDC_VENDOR_SIGMATEL, AZA_CDC_SIGMATEL_STAC9220,
        0xf, 0xe, 0xd, 1.0f
    }
    ,{
        // Listen--I don't know that this will work properly, given that I'm not sure how it's used
        "Conexant 2054912Z", AZA_CDC_VENDOR_CONEXANT, AZA_CDC_CONEXANT_2054912Z,
        0x11, 0x14, 0x12, 0.707f
    }
   ,{
        "Cirrus Logic C4207", AZA_CDC_VENDOR_CIRRUSLOGIC, AZA_CDC_CIRRUSLOGIC_C4207,
        0x24, 0x2b, 0x12, 1.0f
    }

    ,{
        "VIA VT1828S", AZA_CDC_VENDOR_VIA, AZA_CDC_VIA_VT1828S,
        0x24, 0x2b, 0x12, 0.707f
    }
    ,{
        "VIA VT1812S", AZA_CDC_VENDOR_VIA, AZA_CDC_VIA_VT1812S,
        0x24, 0x2b, 0x12, 0.707f
    }
    ,{
        "VIA VT2002P", AZA_CDC_VENDOR_VIA, AZA_CDC_VIA_VT2002P,
        0x26, 0x2b, 0x12, 0.707f
    },
    {
        "IDT 92HD81", AZA_CDC_VENDOR_IDT, AZA_CDC_IDT_92HD81,
        0xa, 0xf, 0xe, 1.0
    }
    ,{
        "IDT 92HD89C3X5", AZA_CDC_VENDOR_IDT, AZA_CDC_IDT_92HD89C3X5,
        0xa, 0xf, 0xe, 1.0
    }
    ,{
        "LWPU CDC", AZA_CDC_VENDOR_LWIDIA, 0xffff,
        0, 0, 0, 1
    }
};

// Verb and response format
#define AZA_CDC_CMD_CAD                 31:28
#define AZA_CDC_CMD_NID                 27:20
#define AZA_CDC_CMD_VERB                19:0
#define AZA_CDC_CMD_VERB_SHORT          19:16
#define AZA_CDC_CMD_VERB_SHORT_PAYLOAD  15:0
#define AZA_CDC_CMD_VERB_LONG           19:8
#define AZA_CDC_CMD_VERB_LONG_PAYLOAD   7:0
#define AZA_CDC_RESPONSE_UNSOL          4:4 // Offset in upper 32 bits
#define AZA_CDC_RESPONSE_UNSOL_ISUNSOL  0x1
#define AZA_CDC_RESPONSE_CAD            3:0 // Offset in upper 32 bits
#define AZA_CDC_RESPONSE_RESPONSE       31:0

// Most, but not all, of these parameters and verbs are specific to the audio function group.
// If a function group class is created, these may need to move.

// Codec Parameters
// Pulled from Azalia spec, Revision 1.0 (April 15, 2004)

#define AZA_CDCP_VENDOR_ID                0x00
#define AZA_CDCP_VENDOR_ID_VID           31:16
#define AZA_CDCP_VENDOR_ID_DID            15:0

#define AZA_CDCP_REV_ID                   0x02
#define AZA_CDCP_REV_ID_MAJOR            23:20
#define AZA_CDCP_REV_ID_MINOR            19:16
#define AZA_CDCP_REV_ID_RID               15:8
#define AZA_CDCP_REV_ID_SID                7:0

#define AZA_CDCP_SUB_NODE_CNT             0x04
#define AZA_CDCP_SUB_NODE_CNT_START      23:16
#define AZA_CDCP_SUB_NODE_CNT_TOTAL        7:0

#define AZA_CDCP_FGROUP_TYPE              0x05
#define AZA_CDCP_FGROUP_TYPE_UNSOL         8:8
#define AZA_CDCP_FGROUP_TYPE_TYPE          7:0
#define AZA_CDCP_FGROUP_TYPE_TYPE_AUDIO    0x1
#define AZA_CDCP_FGROUP_TYPE_TYPE_MODEM    0x2
#define AZA_CDCP_FGROUP_TYPE_TYPE_OTHER   0x10  // Not defined in the spec...where did this value come from?
#define AZA_CDCP_FGROUP_TYPE_TYPE_VEND    0xff

#define AZA_CDCP_AUDIOG_CAP                0x8
#define AZA_CDCP_AUDIOG_CAP_BEEP         16:16
#define AZA_CDCP_AUDIOG_CAP_IDELAY        11:8
#define AZA_CDCP_AUDIOG_CAP_ODELAY         3:0

#define AZA_CDCP_AUDIOW_CAP                0x9
#define AZA_CDCP_AUDIOW_CAP_TYPE         23:20
#define AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOOUT  0x0
#define AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOIN   0x1
#define AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOMIX  0x2
#define AZA_CDCP_AUDIOW_CAP_TYPE_AUDIOSEL  0x3
#define AZA_CDCP_AUDIOW_CAP_TYPE_PIN       0x4
#define AZA_CDCP_AUDIOW_CAP_TYPE_POWER     0x5
#define AZA_CDCP_AUDIOW_CAP_TYPE_VOLUME    0x6
#define AZA_CDCP_AUDIOW_CAP_TYPE_BEEP      0x7
#define AZA_CDCP_AUDIOW_CAP_TYPE_VENDORDEF 0xf
#define AZA_CDCP_AUDIOW_CAP_DELAY        19:16
#define AZA_CDCP_AUDIOW_CAP_CHAN_CNT     15:13
#define AZA_CDCP_AUDIOW_CAP_CONTENT_PROT 12:12
#define AZA_CDCP_AUDIOW_CAP_SWAP         11:11
#define AZA_CDCP_AUDIOW_CAP_POWER        10:10
#define AZA_CDCP_AUDIOW_CAP_DIGITAL        9:9
#define AZA_CDCP_AUDIOW_CAP_CONLIST        8:8
#define AZA_CDCP_AUDIOW_CAP_UNSOL          7:7
#define AZA_CDCP_AUDIOW_CAP_PROC           6:6
#define AZA_CDCP_AUDIOW_CAP_STRIPE         5:5
#define AZA_CDCP_AUDIOW_CAP_FMTOVR         4:4
#define AZA_CDCP_AUDIOW_CAP_AMPOVR         3:3
#define AZA_CDCP_AUDIOW_CAP_OUTAMP         2:2
#define AZA_CDCP_AUDIOW_CAP_INAMP          1:1
#define AZA_CDCP_AUDIOW_CAP_STEREO         0:0
// These are not actually audio widget types, but are used
// as values in the node->type field
#define AZA_CDCP_AUDIOW_CAP_TYPE_ROOT     0x10
#define AZA_CDCP_AUDIOW_CAP_TYPE_AUDFGRP  0x11
#define AZA_CDCP_AUDIOW_CAP_TYPE_MODFGRP  0x12
#define AZA_CDCP_AUDIOW_CAP_TYPE_OTHFGRP  0x13
#define AZA_CDCP_AUDIOW_CAP_TYPE_ILWALID  0xff

#define AZA_CDCP_PCM_SUP                0xa
#define AZA_CDCP_PCM_SUP_B32          20:20
#define AZA_CDCP_PCM_SUP_B24          19:19
#define AZA_CDCP_PCM_SUP_B20          18:18
#define AZA_CDCP_PCM_SUP_B16          17:17
#define AZA_CDCP_PCM_SUP_B8           16:16
#define AZA_CDCP_PCM_SUP_R12          11:11
#define AZA_CDCP_PCM_SUP_R11          10:10
#define AZA_CDCP_PCM_SUP_R10            9:9
#define AZA_CDCP_PCM_SUP_R9             8:8
#define AZA_CDCP_PCM_SUP_R8             7:7
#define AZA_CDCP_PCM_SUP_R7             6:6
#define AZA_CDCP_PCM_SUP_R6             5:5
#define AZA_CDCP_PCM_SUP_R5             4:4
#define AZA_CDCP_PCM_SUP_R4             3:3
#define AZA_CDCP_PCM_SUP_R3             2:2
#define AZA_CDCP_PCM_SUP_R2             1:1
#define AZA_CDCP_PCM_SUP_R1             0:0

#define AZA_CDCP_STREAM_FMT_SUP         0xb
#define AZA_CDCP_STREAM_FMT_SUP_AC3     2:2
#define AZA_CDCP_STREAM_FMT_SUP_F32     1:1
#define AZA_CDCP_STREAM_FMT_SUP_PCM     0:0

#define AZA_CDCP_PIN_CAP                0xc
#define AZA_CDCP_PIN_CAP_EAPD         16:16
#define AZA_CDCP_PIN_CAP_VREF          15:8
#define AZA_CDCP_PIN_CAP_HDMI           7:7
#define AZA_CDCP_PIN_CAP_BALANCE        6:6
#define AZA_CDCP_PIN_CAP_INPUT          5:5
#define AZA_CDCP_PIN_CAP_OUTPUT         4:4
#define AZA_CDCP_PIN_CAP_HEADPHONE      3:3
#define AZA_CDCP_PIN_CAP_PRESENCE       2:2
#define AZA_CDCP_PIN_CAP_TRIGGER        1:1
#define AZA_CDCP_PIN_CAP_IMPEDENCE      0:0

#define AZA_CDCP_INAMP_CAP              0xd
#define AZA_CDCP_OUTAMP_CAP            0x12
#define AZA_CDCP_AMP_CAP_MUTE         31:31
#define AZA_CDCP_AMP_CAP_STEPSIZE     22:16
#define AZA_CDCP_AMP_CAP_NUMSTEPS      14:8
#define AZA_CDCP_AMP_CAP_OFFSET         6:0

#define AZA_CDCP_CON_LIST_LEN           0xe
#define AZA_CDCP_CON_LIST_LEN_LONG      7:7
#define AZA_CDCP_CON_LIST_LEN_NUM       6:0

#define AZA_CDCP_POWER_STATE_SUP        0xf
#define AZA_CDCP_POWER_STATE_SUP_D3     3:3
#define AZA_CDCP_POWER_STATE_SUP_D2     2:2
#define AZA_CDCP_POWER_STATE_SUP_D1     1:1
#define AZA_CDCP_POWER_STATE_SUP_D0     0:0

#define AZA_CDCP_PROC_CAP              0x10
#define AZA_CDCP_PROC_CAP_NUMCOEFF     15:8
#define AZA_CDCP_PROC_CAP_BENIGN        0:0

#define AZA_CDCP_GPIO_CNT              0x11
#define AZA_CDCP_GPIO_CNT_GPIWAKE     31:31
#define AZA_CDCP_GPIO_CNT_GPIUNSOL    30:30
#define AZA_CDCP_GPIO_CNT_NUMGPIS     23:16
#define AZA_CDCP_GPIO_CNT_NUMGPOS      15:8
#define AZA_CDCP_GPIO_CNT_NUMGPIOS      7:0

#define AZA_CDCP_VOL_CAP               0x13
#define AZA_CDCP_VOL_CAP_DELTA          7:7
#define AZA_CDCP_VOL_CAP_NUMSTEPS       6:0

enum AzaliaCodecVerbs
{
    VERB_GET_PARAMETER             = 0xF00,
    VERB_GET_CONNECTION_SELECT     = 0xF01,
    VERB_SET_CONNECTION_SELECT     = 0x701,
    VERB_GET_CON_LIST_ENTRY        = 0xF02,
    VERB_GET_PROCESSING_STATE      = 0xF03,
    VERB_SET_PROCESSING_STATE      = 0x703,
    VERB_GET_COEFFICIENT_INDEX     = 0xD00,
    VERB_SET_COEFFICIENT_INDEX     = 0x500,
    VERB_GET_PROCESSING_COEF       = 0xC00,
    VERB_SET_PROCESSING_COEF       = 0x400,
    VERB_GET_AMPLIFIER_GAIN_MUTE   = 0xB00,
    VERB_SET_AMPLIFIER_GAIN_MUTE   = 0x300,
    VERB_GET_STREAM_FORMAT         = 0xA00,
    VERB_SET_STREAM_FORMAT         = 0x200,
    VERB_GET_DIGITAL_COLW          = 0xF0D,
    VERB_SET_DIGITAL_COLW_BYTE1    = 0x70D,
    VERB_SET_DIGITAL_COLW_BYTE2    = 0x70E,
    VERB_GET_POWER_STATE           = 0xF05,
    VERB_SET_POWER_STATE           = 0x705,
    VERB_GET_STREAM_CHANNEL_ID     = 0xF06,
    VERB_SET_STREAM_CHANNEL_ID     = 0x706,
    VERB_GET_SDI_SELECT            = 0xF04,
    VERB_SET_SDI_SELECT            = 0x704,
    VERB_GET_PIN_WIDGET_CONTROL    = 0xF07,
    VERB_SET_PIN_WIDGET_CONTROL    = 0x707,
    VERB_GET_UNSOLICITED_ENABLE    = 0xF08,
    VERB_SET_UNSOLICITED_ENABLE    = 0x708,
    VERB_GET_PIN_SENSE             = 0xF09,
    VERB_SET_PIN_SENSE             = 0x709,
    VERB_GET_EAPD_BTL_ENABLE       = 0xF0C,
    VERB_SET_EAPD_BTL_ENABLE       = 0x70C,
    VERB_GET_BEEP_GEN_CONTROL      = 0xF0A,
    VERB_SET_BEEP_GEN_CONTROL      = 0x70A,
    VERB_GET_VOLUME_KNOB_CONTROL   = 0xF0F,
    VERB_SET_VOLUME_KNOB_CONTROL   = 0x70F,
    VERB_GET_SUBSYSTEM_ID          = 0xF20,
    VERB_SET_SUBSYSTEM_ID_BYTE0    = 0x720,
    VERB_SET_SUBSYSTEM_ID_BYTE1    = 0x721,
    VERB_SET_SUBSYSTEM_ID_BYTE2    = 0x722,
    VERB_SET_SUBSYSTEM_ID_BYTE3    = 0x723,
    VERB_GET_STRIPE_CONTROL        = 0xF24,
    VERB_SET_STRIPE_CONTROL        = 0x724,
    VERB_GET_COLW_CHAN_CNT         = 0xF2D,
    VERB_SET_COLW_CHAN_CNT         = 0x72D,
    VERB_GET_CONFIG_DEFAULT        = 0xF1C,
    VERB_SET_CONFIG_DEFAULT_BYTE0  = 0x71C,
    VERB_SET_CONFIG_DEFAULT_BYTE1  = 0x71D,
    VERB_SET_CONFIG_DEFAULT_BYTE2  = 0x71E,
    VERB_SET_CONFIG_DEFAULT_BYTE3  = 0x71F,
    VERB_GET_DIP_TRANSMIT_CONTROL  = 0xF32,
    VERB_SET_DIP_TRANSMIT_CONTROL  = 0x732,
    VERB_RESET                     = 0x7FF,
    // GT21x Specific
    VERB_SET_AUDIO_FORMAT          = 0xF78
};

#define AZA_CDCV_GCONFIG_DEFAULT_PORT        31:30
#define AZA_CDCV_GCONFIG_DEFAULT_PORT_JACK     0x0
#define AZA_CDCV_GCONFIG_DEFAULT_PORT_NONE     0x1
#define AZA_CDCV_GCONFIG_DEFAULT_PORT_FIXED    0x2
#define AZA_CDCV_GCONFIG_DEFAULT_PORT_BOTH     0x3
#define AZA_CDCV_GCONFIG_DEFAULT_LOC         29:24
#define AZA_CDCV_GCONFIG_DEFAULT_LOCHI       29:28
#define AZA_CDCV_GCONFIG_DEFAULT_LOCHI_EXT     0x0
#define AZA_CDCV_GCONFIG_DEFAULT_LOCHI_INT     0x1
#define AZA_CDCV_GCONFIG_DEFAULT_LOCHI_SEP     0x2
#define AZA_CDCV_GCONFIG_DEFAULT_LOCHI_OTH     0x3
#define AZA_CDCV_GCONFIG_DEFAULT_LOCLO       27:24
#define AZA_CDCV_GCONFIG_DEFAULT_LOCLO_NA      0x0
#define AZA_CDCV_GCONFIG_DEFAULT_LOCLO_REAR    0x1
#define AZA_CDCV_GCONFIG_DEFAULT_LOCLO_FRONT   0x2
#define AZA_CDCV_GCONFIG_DEFAULT_LOCLO_LEFT    0x3
#define AZA_CDCV_GCONFIG_DEFAULT_LOCLO_RIGHT   0x4
#define AZA_CDCV_GCONFIG_DEFAULT_LOCLO_TOP     0x5
#define AZA_CDCV_GCONFIG_DEFAULT_LOCLO_BOTTOM  0x6
#define AZA_CDCV_GCONFIG_DEFAULT_LOCLO_SPEC7   0x7
#define AZA_CDCV_GCONFIG_DEFAULT_LOCLO_SPEC8   0x8
#define AZA_CDCV_GCONFIG_DEFAULT_LOCLO_SPEC9   0x9
#define AZA_CDCV_GCONFIG_DEFAULT_DEVICE      23:20
#define AZA_CDCV_GCONFIG_DEFAULT_CONNECTION  19:16
#define AZA_CDCV_GCONFIG_DEFAULT_COLOR       15:12
#define AZA_CDCV_GCONFIG_DEFAULT_COLOR_UNKNOWN 0x0
#define AZA_CDCV_GCONFIG_DEFAULT_COLOR_BLACK   0x1
#define AZA_CDCV_GCONFIG_DEFAULT_COLOR_GREY    0x2
#define AZA_CDCV_GCONFIG_DEFAULT_COLOR_BLUE    0x3
#define AZA_CDCV_GCONFIG_DEFAULT_COLOR_GREEN   0x4
#define AZA_CDCV_GCONFIG_DEFAULT_COLOR_RED     0x5
#define AZA_CDCV_GCONFIG_DEFAULT_COLOR_ORANGE  0x6
#define AZA_CDCV_GCONFIG_DEFAULT_COLOR_YELLOW  0x7
#define AZA_CDCV_GCONFIG_DEFAULT_COLOR_PURPLE  0x8
#define AZA_CDCV_GCONFIG_DEFAULT_COLOR_PINK    0x9
#define AZA_CDCV_GCONFIG_DEFAULT_COLOR_WHITE   0xe
#define AZA_CDCV_GCONFIG_DEFAULT_COLOR_OTHER   0xf
#define AZA_CDCV_GCONFIG_DEFAULT_MISC         11:8
#define AZA_CDCV_GCONFIG_DEFAULT_MISC_JDOVR    0x1
#define AZA_CDCV_GCONFIG_DEFAULT_ASSOC         7:4
#define AZA_CDCV_GCONFIG_DEFAULT_SEQUENCE      3:0

// Pin Widget Control Fields
#define AZA_CDC_PINW_CTRL_IN_HDMI_CODING_TYPE  1:0
#define AZA_CDC_PINW_CTRL_IN_VREF_EN           2:0
#define AZA_CDC_PINW_CTRL_IN_ENABLE            5:5
#define AZA_CDC_PINW_CTRL_OUT_ENABLE           6:6
#define AZA_CDC_PINW_CTRL_HPHN_ENABLE          7:7

// Power State
#define AZA_CDC_NODE_POWER_STATE_ACT           7:4
#define AZA_CDC_NODE_POWER_STATE_SET           3:0

// Gain Mute Set Payload
#define AZA_CDCV_SET_GAINMUTE_OUT_AMP          15:15
#define AZA_CDCV_SET_GAINMUTE_IN_AMP           14:14
#define AZA_CDCV_SET_GAINMUTE_LEFT_AMP         13:13
#define AZA_CDCV_SET_GAINMUTE_RIGHT_AMP        12:12
#define AZA_CDCV_SET_GAINMUTE_INDEX            11:8
#define AZA_CDCV_SET_GAINMUTE_MUTE             7:7
#define AZA_CDCV_SET_GAINMUTE_GAIN             6:0

class AzaliaController;

class AzaliaCodec
{
public:

    typedef struct
    {
        UINT16 id;
        UINT16 fgroup_id;
        UINT32 type;
        vector<UINT16> clist;
        UINT32 caps;
        UINT32 specialCaps;
    } Node;

    AzaliaCodec(AzaliaController* pAzalia, UINT08 Addr);
    ~AzaliaCodec();

    RC Initialize();
    RC PowerUpAllWidgets();
    RC SetDIPTransmitControl(UINT32 NodeID, bool Enable);

    RC GetVendorDeviceID(UINT16* pVid, UINT16* pDid);

    RC SubmitVerb(UINT32 NodeID, UINT32 VerbID, UINT32 Payload);

    RC SendCommand(UINT32 NodeID, UINT32 VerbID,
                   UINT32 Payload, UINT32* pResponse = NULL);

    RC SendCommands(UINT32* pCommands, UINT32 nCommands, UINT64* pResponses);

    // TODO: Implement SendICommand
    //RC SendICommand(UINT32 NodeID, UINT32 VerbID,
    //                UINT32 Payload, UINT32* Response = NULL);

    const AzaliaCodec::Node* GetNode(UINT16 WidgetID);
    const vector<AzaliaCodec::Node>* GetNodeList();
    RC PrintInfoNode(Tee::Priority Pri, UINT16 NodeID);

    static RC ConstructVerb(UINT32* pCommand, UINT08 CodecAddr, UINT32 NodeID,
                            UINT32 VerbID, UINT32 Payload);

    static const char* FunctionGroupTypeToString(UINT32 Type);
    static const char* NodeTypeToString(UINT32 Type);

    RC FindRoute(AzaliaRoute** ppRoute, AzaliaDmaEngine::DIR Dir,
                 AzaliaFormat* pFormat, bool IsMutable, UINT32 PinNum);

    RC MapPins();
    RC GetCodecDefaults(const CodecDefaultInformation** pCDInfo);
    bool IsFormatSupported(UINT32 Dir, UINT32 Rate, UINT32 Size);
    bool IsSupportRoutableInputPath();

    // Unsolicited responses
    RC EnableUnsolicitedResponsesOnAllPins(bool IsEnable);
    RC EnableUnsolicitedResponses(bool IsEnable);
    RC GetUnsolRespRoutes(bool CheckInput, bool CheckOutput,
                          vector<AzaliaRoute*>* pRoutes, UINT32 RespTag);

    RC PrintInputRoutes();
    RC PrintOutputRoutes();
private:

    RC EnumerateFunctionGroups();
    RC EnumerateNodes();
    RC PrintInfoNodeList();

    RC GenerateAllOutputRoutes();
    RC GenerateAllInputRoutes();
    RC GenerateRoutesRelwrsively(list<AzaliaRoute>* pRList,
                                 UINT32 DestType, UINT32 LwrrDepth);

    AzaliaController* m_pAza;
    UINT08 m_Addr;
    UINT16 m_Vid;
    UINT16 m_Did;
    UINT16 m_DefaultInfoIndex;

    vector<Node> m_Nodes;
    vector<UINT16> m_AudioGroupBaseNodes;
    vector<UINT16> m_ModemGroupBaseNodes;
    list<AzaliaRoute> m_InputRoutes;
    list<AzaliaRoute> m_OutputRoutes;
};

#endif // INCLUDED_AZACDC_H

