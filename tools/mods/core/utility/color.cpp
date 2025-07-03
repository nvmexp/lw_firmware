/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/tee.h"
#include "core/include/platform.h"
#include "core/include/color.h"
#include "core/include/utility.h"
#include "core/include/massert.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include <memory>
#include <string.h>

static bool s_UseFP16DivideWorkaround = false;
void ColorUtils::SetFP16DivideWorkaround (bool b)
{
    s_UseFP16DivideWorkaround = b;
}
bool ColorUtils::GetFP16DivideWorkaround()
{
    return s_UseFP16DivideWorkaround;
}
UINT32 ColorUtils::GetFP16Divide()
{
    if (s_UseFP16DivideWorkaround)
        return 0x3f;   // pre-gf11x
    else
        return 65535;  // gf11x+
}

enum ColorSpace
{
    NoColor,
    RgbInt,
    RgbFloat16,
    RgbFloat32,
    Ycc,
    Yuv
};

// These are in low-to-high bit order, assuming little-endian.

#define ELM_NONE { ColorUtils::elNone,    ColorUtils::elNone,    ColorUtils::elNone,  ColorUtils::elNone    }
#define ELM_A    { ColorUtils::elAlpha,   ColorUtils::elNone,    ColorUtils::elNone,  ColorUtils::elNone    }
#define ELM_R    { ColorUtils::elRed,     ColorUtils::elNone,    ColorUtils::elNone,  ColorUtils::elNone    }
#define ELM_RG   { ColorUtils::elRed,     ColorUtils::elGreen,   ColorUtils::elNone,  ColorUtils::elNone    }
#define ELM_GR   { ColorUtils::elGreen,   ColorUtils::elRed,     ColorUtils::elNone,  ColorUtils::elNone    }
#define ELM_RGB  { ColorUtils::elRed,     ColorUtils::elGreen,   ColorUtils::elBlue,  ColorUtils::elNone    }
#define ELM_BGR  { ColorUtils::elBlue,    ColorUtils::elGreen,   ColorUtils::elRed,   ColorUtils::elNone    }
#define ELM_RBG  { ColorUtils::elRed,     ColorUtils::elBlue,    ColorUtils::elGreen, ColorUtils::elNone    }
#define ELM_RGBA { ColorUtils::elRed,     ColorUtils::elGreen,   ColorUtils::elBlue,  ColorUtils::elAlpha   }
#define ELM_ARGB { ColorUtils::elAlpha,   ColorUtils::elRed,     ColorUtils::elGreen, ColorUtils::elBlue    }
#define ELM_ABGR { ColorUtils::elAlpha,   ColorUtils::elBlue,    ColorUtils::elGreen, ColorUtils::elRed     }
#define ELM_BGRA { ColorUtils::elBlue,    ColorUtils::elGreen,   ColorUtils::elRed,   ColorUtils::elAlpha   }
#define ELM_BGRx { ColorUtils::elBlue,    ColorUtils::elGreen,   ColorUtils::elRed,   ColorUtils::elIgnored }
#define ELM_RGBx { ColorUtils::elRed,     ColorUtils::elGreen,   ColorUtils::elBlue,  ColorUtils::elIgnored }
#define ELM_xBGR { ColorUtils::elIgnored, ColorUtils::elBlue,    ColorUtils::elGreen, ColorUtils::elRed     }
#define ELM_xRGB { ColorUtils::elIgnored, ColorUtils::elRed,     ColorUtils::elGreen, ColorUtils::elBlue    }
#define ELM_OBGR { ColorUtils::elOther,   ColorUtils::elBlue,    ColorUtils::elGreen, ColorUtils::elRed     }
#define ELM_GBGR { ColorUtils::elGreen,   ColorUtils::elBlue,    ColorUtils::elGreen, ColorUtils::elRed     }
#define ELM_BGRG { ColorUtils::elBlue,    ColorUtils::elGreen,   ColorUtils::elRed,   ColorUtils::elGreen   }
#define ELM_O    { ColorUtils::elOther,   ColorUtils::elNone,    ColorUtils::elNone,  ColorUtils::elNone    }
#define ELM_OO   { ColorUtils::elOther,   ColorUtils::elOther,   ColorUtils::elNone,  ColorUtils::elNone    }
#define ELM_OOOO { ColorUtils::elOther,   ColorUtils::elOther,   ColorUtils::elOther, ColorUtils::elOther   }
#define ELM_D    { ColorUtils::elDepth,   ColorUtils::elNone,    ColorUtils::elNone,  ColorUtils::elNone    }
#define ELM_DO   { ColorUtils::elDepth,   ColorUtils::elOther,   ColorUtils::elNone,  ColorUtils::elNone    }
#define ELM_SD   { ColorUtils::elStencil, ColorUtils::elDepth,   ColorUtils::elNone,  ColorUtils::elNone    }
#define ELM_DS   { ColorUtils::elDepth,   ColorUtils::elStencil, ColorUtils::elNone,  ColorUtils::elNone    }
#define ELM_DSx  { ColorUtils::elDepth,   ColorUtils::elStencil, ColorUtils::elIgnored,ColorUtils::elNone    }
#define ELM_DSO  { ColorUtils::elDepth,   ColorUtils::elStencil, ColorUtils::elOther, ColorUtils::elNone    }
#define ELM_Dx   { ColorUtils::elDepth,   ColorUtils::elIgnored, ColorUtils::elNone,  ColorUtils::elNone    }
#define ELM_S    { ColorUtils::elStencil, ColorUtils::elNone,    ColorUtils::elNone,  ColorUtils::elNone    }

struct ColorFormatRec
{
    const char *       Name;
    UINT32             BytesPerPixel;
    UINT32             WordSize;
    bool               IsFloat;
    bool               IsDepth;
    bool               IsSigned;
    bool               IsNormalized;
    UINT32             PixelElements;
    ColorSpace         Space;
    ColorUtils::Element Elements[ColorUtils::MaxElementsPerFormat];
};

static ColorFormatRec ColorFormatTable[ColorUtils::Format_NUM_FORMATS] =
{
    // Name                        bpp ws, isFlt isDpth isSign isNorm elms space
    { "LWFMT_NONE",                  0, 0, false, false, false, false, 0, NoColor,    ELM_NONE },
    { "R5G6B5",                      2, 2, false, false, false, true,  3, RgbInt,     ELM_BGR  },
    { "A8R8G8B8",                    4, 4, false, false, false, true,  4, RgbInt,     ELM_BGRA },
    { "R8G8B8A8",                    4, 4, false, false, false, true,  4, RgbInt,     ELM_ABGR },
    { "A2R10G10B10",                 4, 4, false, false, false, true,  4, RgbInt,     ELM_ABGR },
    { "R10G10B10A2",                 4, 4, false, false, false, true,  4, RgbInt,     ELM_ABGR },
    { "CR8YB8CB8YA8",                2, 4, false, false, false, true,  3, Ycc,        ELM_OOOO },
    { "YB8CR8YA8CB8",                2, 4, false, false, false, true,  3, Ycc,        ELM_OOOO },
    { "Z1R5G5B5",                    2, 2, false, false, false, true,  4, RgbInt,     ELM_BGR  },
    { "Z24S8",                       4, 4, false, true,  false, true,  2, NoColor,    ELM_SD   },
    { "Z16",                         2, 2, false, true,  false, true,  1, NoColor,    ELM_D    },
    { "RF16",                        2, 2, true,  false, true,  false, 1, RgbFloat16, ELM_R    },
    { "RF32",                        4, 4, true,  false, true,  false, 1, RgbFloat32, ELM_R    },
    { "RF16_GF16_BF16_AF16",         8, 2, true,  false, true,  false, 4, RgbFloat16, ELM_RGBA },
    { "RF32_GF32_BF32_AF32",        16, 4, true,  false, true,  false, 4, RgbFloat32, ELM_RGBA },
    { "Y8",                          1, 1, false, false, false, true,  1, RgbInt,     ELM_O    },
    { "B8_G8_R8",                    3, 1, false, false, false, true,  3, RgbInt,     ELM_BGR  },
    { "Z24",                         3, 1, false, true,  false, true,  1, NoColor,    ELM_O    },
    { "I8",                          1, 1, false, false, false, false, 1, NoColor,    ELM_O    },
    { "VOID8",                       1, 1, false, false, false, false, 1, NoColor,    ELM_O    },
    { "VOID16",                      2, 2, false, false, false, false, 1, NoColor,    ELM_O    },
    { "A2V10Y10U10",                 4, 4, false, false, false, true,  4, Yuv,        ELM_OOOO },
    { "A2U10Y10V10",                 4, 4, false, false, false, true,  4, Yuv,        ELM_OOOO },
    { "VE8YO8UE8YE8",                2, 4, false, false, false, true,  3, Yuv,        ELM_OOOO },
    { "UE8YO8VE8YE8",                2, 4, false, false, false, true,  3, Yuv,        ELM_OOOO },
    { "YO8VE8YE8UE8",                2, 4, false, false, false, true,  3, Yuv,        ELM_OOOO },
    { "YO8UE8YE8VE8",                2, 4, false, false, false, true,  3, Yuv,        ELM_OOOO },
    { "YE16_UE16_YO16_VE16",         8, 2, false, false, false, true,  4, Yuv,        ELM_OOOO },
    { "YE10Z6_UE10Z6_YO10Z6_VE10Z6", 8, 2, false, false, false, true,  4, Yuv,        ELM_OOOO },
    { "UE16_YE16_VE16_YO16",         8, 2, false, false, false, true,  4, Yuv,        ELM_OOOO },
    { "UE10Z6_YE10Z6_VE10Z6_YO10Z6", 8, 2, false, false, false, true,  4, Yuv,        ELM_OOOO },
    { "VOID32",                      4, 4, false, false, false, false, 1, NoColor,    ELM_O    },
    { "CPST8",                       1, 1, false, false, false, false, 1, NoColor,    ELM_O    },
    { "CPSTY8CPSTC8",                2, 2, false, false, false, false, 2, NoColor,    ELM_OO   },
    { "AUDIOL16_AUDIOR16",           4, 2, false, false, false, false, 2, NoColor,    ELM_OO   },
    { "AUDIOL32_AUDIOR32",           8, 4, false, false, false, false, 2, NoColor,    ELM_OO   },
    { "A2B10G10R10",                 4, 4, false, false, false, true,  4, RgbInt,     ELM_RGBA },
    { "A8B8G8R8",                    4, 4, false, false, false, true,  4, RgbInt,     ELM_RGBA },
    { "A1R5G5B5",                    2, 2, false, false, false, true,  4, RgbInt,     ELM_BGRA },
    { "Z8R8G8B8",                    4, 4, false, false, false, true,  4, RgbInt,     ELM_BGRx },
    { "Z32",                         4, 4, false, true,  false, true,  1, NoColor,    ELM_D    },
    { "X8R8G8B8",                    4, 4, false, false, false, true,  3, RgbInt,     ELM_BGRx },
    { "X1R5G5B5",                    2, 2, false, false, false, true,  3, RgbInt,     ELM_BGRx },
    { "AN8BN8GN8RN8",                4, 4, false, false, true,  true,  4, RgbInt,     ELM_RGBA },
    { "AS8BS8GS8RS8",                4, 4, false, false, true,  false, 4, RgbInt,     ELM_RGBA },
    { "AU8BU8GU8RU8",                4, 4, false, false, false, false, 4, RgbInt,     ELM_RGBA },
    { "X8B8G8R8",                    4, 4, false, false, false, true,  3, RgbInt,     ELM_RGBx },
    { "A8RL8GL8BL8",                 4, 4, false, false, false, true,  4, RgbInt,     ELM_BGRA },
    { "X8RL8GL8BL8",                 4, 4, false, false, false, true,  3, RgbInt,     ELM_BGRx },
    { "A8BL8GL8RL8",                 4, 4, false, false, false, true,  4, RgbInt,     ELM_RGBA },
    { "X8BL8GL8RL8",                 4, 4, false, false, false, true,  3, RgbInt,     ELM_RGBx },
    { "RF32_GF32_BF32_X32",         16, 4, true,  false, true,  false, 3, RgbFloat32, ELM_RGBx },
    { "RS32_GS32_BS32_AS32",        16, 4, false, false, true,  false, 4, RgbInt,     ELM_RGBA },
    { "RS32_GS32_BS32_X32",         16, 4, false, false, true,  false, 3, RgbInt,     ELM_RGBx },
    { "RU32_GU32_BU32_AU32",        16, 4, false, false, false, false, 4, RgbInt,     ELM_RGBA },
    { "RU32_GU32_BU32_X32",         16, 4, false, false, false, false, 3, RgbInt,     ELM_RGBx },
    { "R16_G16_B16_A16",             8, 2, false, false, false, true,  4, RgbInt,     ELM_RGBA },
    { "RN16_GN16_BN16_AN16",         8, 2, false, false, true,  true,  4, RgbInt,     ELM_RGBA },
    { "RU16_GU16_BU16_AU16",         8, 2, false, false, false, false, 4, RgbInt,     ELM_RGBA },
    { "RS16_GS16_BS16_AS16",         8, 2, false, false, true,  false, 4, RgbInt,     ELM_RGBA },
    { "RF16_GF16_BF16_X16",          8, 2, true,  false, true,  false, 3, RgbFloat16, ELM_RGBA },
    { "RF32_GF32",                   8, 4, true,  false, true,  false, 2, RgbFloat32, ELM_RG   },
    { "RS32_GS32",                   8, 4, false, false, true,  false, 2, RgbInt,     ELM_RG   },
    { "RU32_GU32",                   8, 4, false, false, false, false, 2, RgbInt,     ELM_RG   },
    { "RS32",                        4, 4, false, false, true,  false, 1, RgbInt,     ELM_R    },
    { "RU32",                        4, 4, false, false, false, false, 1, RgbInt,     ELM_R    },
    { "AU2BU10GU10RU10",             4, 4, false, false, false, false, 4, RgbInt,     ELM_RGBA },
    { "RF16_GF16",                   4, 2, true,  false, true,  false, 2, RgbFloat16, ELM_RG   },
    { "RS16_GS16",                   4, 2, false, false, true,  false, 2, RgbInt,     ELM_RG   },
    { "RN16_GN16",                   4, 2, false, false, true,  true,  2, RgbInt,     ELM_RG   },
    { "RU16_GU16",                   4, 2, false, false, false, false, 2, RgbInt,     ELM_RG   },
    { "R16_G16",                     4, 2, false, false, false, true,  2, RgbInt,     ELM_RG   },
    { "BF10GF11RF11",                4, 4, true,  false, false, false, 3, NoColor,    ELM_RGB  },
    { "G8R8",                        2, 2, false, false, false, true,  2, RgbInt,     ELM_RG   },
    { "GN8RN8",                      2, 2, false, false, true,  true,  2, RgbInt,     ELM_RG   },
    { "GS8RS8",                      2, 2, false, false, true,  false, 2, RgbInt,     ELM_RG   },
    { "GU8RU8",                      2, 2, false, false, false, false, 2, RgbInt,     ELM_RG   },
    { "R16",                         2, 2, false, false, false, true,  1, RgbInt,     ELM_R    },
    { "RN16",                        2, 2, false, false, true,  true,  1, RgbInt,     ELM_R    },
    { "RS16",                        2, 2, false, false, true,  false, 1, RgbInt,     ELM_R    },
    { "RU16",                        2, 2, false, false, false, false, 1, RgbInt,     ELM_R    },
    { "R8",                          1, 1, false, false, false, true,  1, RgbInt,     ELM_R    },
    { "RN8",                         1, 1, false, false, true,  true,  1, RgbInt,     ELM_R    },
    { "RS8",                         1, 1, false, false, true,  false, 1, RgbInt,     ELM_R    },
    { "RU8",                         1, 1, false, false, false, false, 1, RgbInt,     ELM_R    },
    { "A8",                          1, 1, false, false, false, true,  1, NoColor,    ELM_A    },
    { "ZF32",                        4, 4, true,  true,  true,  false, 1, NoColor,    ELM_D    },
    { "S8Z24",                       4, 4, false, true,  false, true,  2, NoColor,    ELM_DS   },
    { "X8Z24",                       4, 4, false, true,  false, true,  1, NoColor,    ELM_Dx   },
    { "V8Z24",                       4, 4, false, true,  false, true,  2, NoColor,    ELM_DO   },
    { "ZF32_X24S8",                  8, 4, true,  true,  true,  false, 3, NoColor,    ELM_DSx  },
    { "X8Z24_X16V8S8",               8, 4, false, true,  false, true,  3, NoColor,    ELM_DSO  },
    { "ZF32_X16V8X8",                8, 4, true,  true,  true,  false, 2, NoColor,    ELM_DSO  },
    { "ZF32_X16V8S8",                8, 4, true,  true,  true,  false, 3, NoColor,    ELM_DSO  },
    { "S8",                          1, 1, false, false, false, true,  1, NoColor,    ELM_S    },
    { "X2BL10GL10RL10_XRBIAS",       4, 4, false, false, true,  true,  3, RgbInt,     ELM_RGB  },
    { "R16_G16_B16_A16_LWBIAS",      8, 2, false, false, true,  true,  4, RgbInt,     ELM_RGBA },
    { "X2BL10GL10RL10_XVYCC",        4, 4, false, false, true,  true,  3, RgbInt,     ELM_RGB  },

    // YUV color formats
    { "Y8_U8__Y8_V8_N422",           4, 4, false, false, false, true,  4, Yuv,        ELM_OOOO },
    { "U8_Y8__V8_Y8_N422",           4, 4, false, false, false, true,  4, Yuv,        ELM_OOOO },
    { "Y8___U8V8_N444",              4, 4, false, false, false, true,  3, Yuv,        ELM_OOOO },
    { "Y8___U8V8_N422",              4, 4, false, false, false, true,  3, Yuv,        ELM_OOOO },
    { "Y8___U8V8_N422R",             4, 4, false, false, false, true,  3, Yuv,        ELM_OOOO },
    { "Y8___V8U8_N420",              4, 4, false, false, false, true,  3, Yuv,        ELM_OOOO },
    { "Y8___U8___V8_N444",           4, 4, false, false, false, true,  3, Yuv,        ELM_OOOO },
    { "Y8___U8___V8_N420",           4, 4, false, false, false, true,  3, Yuv,        ELM_OOOO },
    { "Y10___U10V10_N444",           4, 4, false, false, false, true,  3, Yuv,        ELM_OOOO },
    { "Y10___U10V10_N422",           4, 4, false, false, false, true,  3, Yuv,        ELM_OOOO },
    { "Y10___U10V10_N422R",          4, 4, false, false, false, true,  3, Yuv,        ELM_OOOO },
    { "Y10___V10U10_N420",           4, 4, false, false, false, true,  3, Yuv,        ELM_OOOO },
    { "Y10___U10___V10_N444",        4, 4, false, false, false, true,  3, Yuv,        ELM_OOOO },
    { "Y10___U10___V10_N420",        4, 4, false, false, false, true,  3, Yuv,        ELM_OOOO },

    { "Y12___U12V12_N444",           6, 4, false, false, false, true,  3, Yuv,        ELM_OOOO },
    { "Y12___U12V12_N422",           6, 4, false, false, false, true,  3, Yuv,        ELM_OOOO },
    { "Y12___U12V12_N422R",          6, 4, false, false, false, true,  3, Yuv,        ELM_OOOO },
    { "Y12___V12U12_N420",           6, 4, false, false, false, true,  3, Yuv,        ELM_OOOO },
    { "Y12___U12___V12_N444",        6, 4, false, false, false, true,  3, Yuv,        ELM_OOOO },
    { "Y12___U12___V12_N420",        6, 4, false, false, false, true,  3, Yuv,        ELM_OOOO },

    // Unique cheetah formats
    { "A8Y8U8V8",                    4, 4, false, false, false, true,  4, Yuv,        ELM_OOOO },
    { "I1",                          1, 1, false, false, false, false, 1, NoColor,    ELM_O    }, // Faked, less than byte format not supported
    { "I2",                          1, 1, false, false, false, false, 1, NoColor,    ELM_O    }, // Faked, less than byte format not supported
    { "I4",                          1, 1, false, false, false, false, 1, NoColor,    ELM_O    }, // Faked, less than byte format not supported
    { "A4R4G4B4",                    2, 2, false, false, false, true,  4, RgbInt,     ELM_BGRA },
    { "R4G4B4A4",                    2, 2, false, false, false, true,  4, RgbInt,     ELM_ABGR },
    { "B4G4R4A4",                    2, 2, false, false, false, true,  4, RgbInt,     ELM_ARGB },
    { "R5G5B5A1",                    2, 2, false, false, false, true,  4, RgbInt,     ELM_ABGR },
    { "B5G5R5A1",                    2, 2, false, false, false, true,  4, RgbInt,     ELM_ARGB },
    { "A8R6x2G6x2B6x2",              4, 4, false, false, false, true,  4, RgbInt,     ELM_BGRA },
    { "A8B6x2G6x2R6x2",              4, 4, false, false, false, true,  4, RgbInt,     ELM_RGBA },
    { "X1B5G5R5",                    2, 2, false, false, false, true,  3, RgbInt,     ELM_RGBx },
    { "B5G5R5X1",                    2, 2, false, false, false, true,  3, RgbInt,     ELM_xRGB },
    { "R5G5B5X1",                    2, 2, false, false, false, true,  3, RgbInt,     ELM_xBGR },
    { "U8",                          1, 1, false, false, false, true,  1, RgbInt,     ELM_O    },
    { "V8",                          1, 1, false, false, false, true,  1, RgbInt,     ELM_O    },
    { "CR8",                         1, 1, false, false, false, true,  1, RgbInt,     ELM_O    },
    { "CB8",                         1, 1, false, false, false, true,  1, RgbInt,     ELM_O    },
    { "U8V8",                        2, 2, false, false, false, true,  2, RgbInt,     ELM_OO   },
    { "V8U8",                        2, 2, false, false, false, true,  2, RgbInt,     ELM_OO   },
    { "CR8CB8",                      2, 2, false, false, false, true,  2, RgbInt,     ELM_OO   },
    { "CB8CR8",                      2, 2, false, false, false, true,  2, RgbInt,     ELM_OO   },

    // Texture specific formats
    { "R32_G32_B32_A32",            16, 4, false, false, false, true,  4, RgbInt,     ELM_RGBA },
    { "R32_G32_B32",                 4, 4, false, false, false, true,  3, RgbInt,     ELM_RGB  },
    { "R32_G32",                     8, 4, false, false, false, true,  2, RgbInt,     ELM_RG   },
    { "R32_B24G8",                   8, 4, false, false, false, true,  3, RgbInt,     ELM_RBG  },
    { "G8R24",                       4, 4, false, false, false, true,  2, RgbInt,     ELM_GR   },
    { "G24R8",                       4, 4, false, false, false, true,  2, RgbInt,     ELM_GR   },
    { "R32",                         4, 4, false, false, false, true,  1, RgbInt,     ELM_R    },
    { "A4B4G4R4",                    2, 2, false, false, false, true,  4, RgbInt,     ELM_ABGR },
    { "A5B5G5R1",                    2, 2, false, false, false, true,  4, RgbInt,     ELM_ABGR },
    { "A1B5G5R5",                    2, 2, false, false, false, true,  4, RgbInt,     ELM_ABGR },
    { "B5G6R5",                      2, 2, false, false, false, true,  4, RgbInt,     ELM_BGR  },
    { "B6G5R5",                      2, 2, false, false, false, true,  4, RgbInt,     ELM_BGR  },
    { "Y8_VIDEO",                    1, 1, false, false, false, true,  1, RgbInt,     ELM_O    },
    { "G4R4",                        1, 1, false, false, false, true,  2, RgbInt,     ELM_GR   },
    { "R1",                          1, 1, false, false, false, true,  1, RgbInt,     ELM_R    },
    { "E5B9G9R9_SHAREDEXP",          4, 4, false, false, false, true,  4, RgbInt,     ELM_OBGR },
    { "G8B8G8R8",                    4, 4, false, false, false, true,  4, RgbInt,     ELM_GBGR },
    { "B8G8R8G8",                    4, 4, false, false, false, true,  4, RgbInt,     ELM_BGRG },
    { "DXT1",                        8, 4, false, false, false, false, 0, NoColor,    ELM_OOOO },
    { "DXT23",                      16, 4, false, false, false, false, 0, NoColor,    ELM_OOOO },
    { "DXT45",                      16, 4, false, false, false, false, 0, NoColor,    ELM_OOOO },
    { "DXN1",                        8, 4, false, false, false, false, 0, NoColor,    ELM_OOOO },
    { "DXN2",                       16, 4, false, false, true,  false, 0, NoColor,    ELM_OOOO },
    { "BC6H_SF16",                  16, 4, true,  false, true,  false, 0, NoColor,    ELM_OOOO },
    { "BC6H_UF16",                  16, 4, true,  false, false, false, 0, NoColor,    ELM_OOOO },
    { "BC7U",                       16, 4, false, false, false, false, 0, NoColor,    ELM_OOOO },
    { "X4V4Z24__COV4R4V",            4, 4, false, true,  false, true,  1, NoColor,    ELM_Dx   },
    { "X4V4Z24__COV8R8V",            4, 4, false, true,  false, true,  1, NoColor,    ELM_Dx   },
    { "V8Z24__COV4R12V",             4, 4, false, true,  false, true,  2, NoColor,    ELM_DO   },
    { "X8Z24_X20V4S8__COV4R4V",      8, 4, false, true,  false, true,  1, NoColor,    ELM_DSx  },
    { "X8Z24_X20V4S8__COV8R8V",      8, 4, false, true,  false, true,  1, NoColor,    ELM_DSx  },
    { "ZF32_X20V4X8__COV4R4V",       8, 4, true,  true,  true,  false, 1, NoColor,    ELM_Dx   },
    { "ZF32_X20V4X8__COV8R8V",       8, 4, true,  true,  true,  false, 1, NoColor,    ELM_Dx   },
    { "ZF32_X20V4S8__COV4R4V",       8, 4, true,  true,  true,  false, 1, NoColor,    ELM_DSx  },
    { "ZF32_X20V4S8__COV8R8V",       8, 4, true,  true,  true,  false, 1, NoColor,    ELM_DSx  },
    { "X8Z24_X16V8S8__COV4R12V",     8, 4, false, true,  false, true,  1, NoColor,    ELM_DSx  },
    { "ZF32_X16V8X8__COV4R12V",      8, 4, true,  true,  true,  false, 1, NoColor,    ELM_Dx   },
    { "ZF32_X16V8S8__COV4R12V",      8, 4, true,  true,  true,  false, 1, NoColor,    ELM_DSx  },
    { "V8Z24__COV8R24V",             4, 4, false, true,  false, true,  1, NoColor,    ELM_DO   },
    { "X8Z24_X16V8S8__COV8R24V",     8, 4, false, true,  false, true,  1, NoColor,    ELM_DSx  },
    { "ZF32_X16V8X8__COV8R24V",      8, 4, true,  true,  true,  false, 1, NoColor,    ELM_Dx   },
    { "ZF32_X16V8S8__COV8R24V",      8, 4, true,  true,  true,  false, 1, NoColor,    ELM_DSx  },
    { "B8G8R8A8",                    4, 4, false, false, false, true,  4, RgbInt,     ELM_ARGB },
    { "ASTC_2D_4X4",                16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_2D_5X4",                16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_2D_5X5",                16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_2D_6X5",                16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_2D_6X6",                16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_2D_8X5",                16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_2D_8X6",                16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_2D_8X8",                16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_2D_10X5",               16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_2D_10X6",               16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_2D_10X8",               16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_2D_10X10",              16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_2D_12X10",              16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_2D_12X12",              16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_SRGB_2D_4X4",           16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_SRGB_2D_5X4",           16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_SRGB_2D_5X5",           16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_SRGB_2D_6X5",           16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_SRGB_2D_6X6",           16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_SRGB_2D_8X5",           16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_SRGB_2D_8X6",           16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_SRGB_2D_8X8",           16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_SRGB_2D_10X5",          16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_SRGB_2D_10X6",          16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_SRGB_2D_10X8",          16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_SRGB_2D_10X10",         16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_SRGB_2D_12X10",         16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ASTC_SRGB_2D_12X12",         16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },

    { "ETC2_RGB",                    8, 8, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ETC2_RGB_PTA",                8, 8, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "ETC2_RGBA",                  16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "EAC",                         8, 8, false, false, false, false, 0, RgbInt,     ELM_OOOO },
    { "EACX2",                      16,16, false, false, false, false, 0, RgbInt,     ELM_OOOO },

    // End of texture formats

    // Name                        bpp ws, isFlt isDpth isSign isNorm elms space
};

bool ColorUtils::FormatIsOutOfRange (ColorUtils::Format fmt)
{
    if (fmt >= ColorUtils::Format_NUM_FORMATS)
    {
        Printf(Tee::PriHigh, "*** ERROR: ColorUtils::Format %d is not recognized\n",
                int(fmt));
        MASSERT(!"Unsupported ColorUtils::Format");
        return true;
    }
    return false;
}

const char * ColorUtils::ElementToString (int el)
{
    const char * elNames[elNUM_ELEMENT_TYPES] =
    {
        "Red"
        ,"Green"
        ,"Blue"
        ,"Alpha"
        ,"Depth"
        ,"Stencil"
        ,"Other"
        ,"Ignored"
        ,"None"
    };

    if (el >= elRed && el < elNUM_ELEMENT_TYPES)
        return elNames[el];

    return "?";
}

// Return a string with the color format name.
string ColorUtils::FormatToString (ColorUtils::Format fmt)
{
    if (FormatIsOutOfRange(fmt))
        return string("lwfmt_none");

    return string(ColorFormatTable[fmt].Name);
}

UINT32 ColorUtils::WordSize (ColorUtils::Format fmt)
{
    if (FormatIsOutOfRange(fmt))
        return 0;

    return ColorFormatTable[fmt].WordSize;
}

bool ColorUtils::IsFloat (ColorUtils::Format fmt)
{
    if (FormatIsOutOfRange(fmt))
        return false;

    return ColorFormatTable[fmt].IsFloat;
}

bool ColorUtils::IsDepth (ColorUtils::Format fmt)
{
    if (FormatIsOutOfRange(fmt))
        return false;

    return ColorFormatTable[fmt].IsDepth;
}

bool ColorUtils::IsSigned (ColorUtils::Format fmt)
{
    if (FormatIsOutOfRange(fmt))
        return false;

    return ColorFormatTable[fmt].IsSigned;
}

ColorUtils::Element ColorUtils::ElementAt (ColorUtils::Format fmt, int idx)
{
    if (FormatIsOutOfRange(fmt))
        return elNone;
    if (MaxElementsPerFormat <= idx)
        return elNone;

    return ColorFormatTable[fmt].Elements[idx];
}

//! \brief Return true if HW normalizes values to the 0:1 range.
//!
//! Returns false for formats where HW treats values literally.
bool ColorUtils::IsNormalized (ColorUtils::Format fmt)
{
    if (FormatIsOutOfRange(fmt))
        return false;

    return ColorFormatTable[fmt].IsNormalized;
}

bool ColorUtils::IsRgb(ColorUtils::Format fmt)
{
    if (FormatIsOutOfRange(fmt))
        return false;

    if (ColorFormatTable[fmt].Space == RgbInt ||
        ColorFormatTable[fmt].Space == RgbFloat16 ||
        ColorFormatTable[fmt].Space == RgbFloat32)
    {
        return true;
    }
    if (fmt == BF10GF11RF11)
    {
        // This is marked "NoColor" in ColorFormatTable, so that
        // IsCompatibleColorSpace will work correctly, but for other purposes
        // this is an RGB format.
        return true;
    }
    return false;
}

bool ColorUtils::IsYuv(ColorUtils::Format fmt)
{
    if (FormatIsOutOfRange(fmt))
        return false;

    return (ColorFormatTable[fmt].Space == Yuv) ? true : false;
}

bool ColorUtils::IsYuvSemiPlanar(ColorUtils::Format fmt)
{
    switch (fmt)
    {
        case Y8___U8V8_N444:
        case Y8___U8V8_N422:
        case Y8___U8V8_N422R:
        case Y8___V8U8_N420:
        case Y10___U10V10_N444:
        case Y10___U10V10_N422:
        case Y10___U10V10_N422R:
        case Y10___V10U10_N420:
        case Y12___U12V12_N444:
        case Y12___U12V12_N422:
        case Y12___U12V12_N422R:
        case Y12___V12U12_N420:
            return true;
        default:
            return false;
    }
}

bool ColorUtils::IsYuvPlanar(ColorUtils::Format fmt)
{
    switch (fmt)
    {
        case Y8___U8___V8_N444:
        case Y8___U8___V8_N420:
        case Y10___U10___V10_N444:
        case Y10___U10___V10_N420:
        case Y12___U12___V12_N444:
        case Y12___U12___V12_N420:
            return true;
        default:
            return false;
    }
}

UINT32 ColorUtils::GetYuvBpc(ColorUtils::Format fmt)
{
    switch (fmt)
    {
        case Y8_U8__Y8_V8_N422:
        case U8_Y8__V8_Y8_N422:
        case Y8___U8V8_N444:
        case Y8___U8V8_N422:
        case Y8___U8V8_N422R:
        case Y8___V8U8_N420:
        case Y8___U8___V8_N444:
        case Y8___U8___V8_N420:
            return 8;
        case Y10___U10V10_N444:
        case Y10___U10V10_N422:
        case Y10___U10V10_N422R:
        case Y10___V10U10_N420:
        case Y10___U10___V10_N444:
        case Y10___U10___V10_N420:
            return 10;
        case Y12___U12V12_N444:
        case Y12___U12V12_N422:
        case Y12___U12V12_N422R:
        case Y12___V12U12_N420:
        case Y12___U12___V12_N444:
        case Y12___U12___V12_N420:
            return 12;
        default:
            Printf(Tee::PriError, "Invalid YUV format\n");
            MASSERT(0);
            return 0;
    }
}

// Return a string with the PNG color format name.
string ColorUtils::PNGFormatToString(ColorUtils::PNGFormat fmt)
{
    switch (fmt)
    {
        case C1:
            return string("c1");
        case C2:
            return string("c2");
        case C4:
            return string("c4");
        case C8:
            return string("c8");
        case C16:
            return string("c16");
        case C8C8:
            return string("c8c8");
        case C16C16:
            return string("c16c16");
        case C8C8C8:
            return string("c8c8c8");
        case C10C10C10:
            return string("c10c10c10");
        case C12C12C12:
            return string("c12c12c12");
        case C16C16C16:
            return string("c16c16c16");
        case C8C8C8C8:
            return string("c8c8c8c8");
        case C10C10C10C2:
            return string("c10c10c10c2");
        case C16C16C16C16:
            return string("c16c16c16c16");
        case LWPNGFMT_NONE:
        default:
            return string("lwpngfmt_none");
    }
}

// Return the color format named by the given string.
ColorUtils::Format ColorUtils::StringToFormat(const string& str)
{
    const char * instr = str.c_str();

    int i;
    for (i = 0; i < Format_NUM_FORMATS; i++)
    {
        if (0 == strcmp(instr, ColorFormatTable[i].Name))
            return Format(i);
    }

    Printf(Tee::PriHigh, "*** ERROR: ColorUtils::Format %s is not recognized\n",
            instr);

    return LWFMT_NONE;
}

// Return the PNG color format named by the given string.
ColorUtils::PNGFormat ColorUtils::StringToPNGFormat(const string& str)
{
    if (str == "c1")
        return C1;
    else if (str == "c2")
        return C2;
    else if (str == "c4")
        return C4;
    else if (str == "c8")
        return C8;
    else if (str == "c16")
        return C16;
    else if (str == "c8c8")
        return C8C8;
    else if (str == "C16C16")
        return C16C16;
    else if (str == "C8C8C8")
        return C8C8C8;
    else if (str == "c10c10c10")
        return C10C10C10;
    else if (str == "c12c12c12")
        return C12C12C12;
    else if (str == "C16C16C16")
        return C16C16C16;
    else if (str == "c8c8c8c8")
        return C8C8C8C8;
    else if (str == "c10c10c10c2")
        return C10C10C10C2;
    else if (str == "c16c16c16c16")
        return C16C16C16C16;
    else
        return LWPNGFMT_NONE;
}

ColorUtils::Format ColorUtils::ColorDepthToFormat(UINT32 ColorDepth)
{
    switch (ColorDepth)
    {
        case 8:
            return Y8;
        case 15:
            return Z1R5G5B5;
        case 16:
            return R5G6B5;
        case 24:
            return B8_G8_R8;
        case 30:
            return Y10___U10___V10_N444;
        case 32:
            return A8R8G8B8;
        case 36:
            return Y12___U12___V12_N444;
        case 64:
            return R16_G16_B16_A16;

        default:
            MASSERT(0);
            return LWFMT_NONE;
    }
}

ColorUtils::Format ColorUtils::ZDepthToFormat(UINT32 ZDepth)
{
    switch (ZDepth)
    {
        case 16:
            return Z16;
        case 24:
            return Z24;
        case 32:
            return Z24S8;

        default:
            MASSERT(0);
            return LWFMT_NONE;
    }
}

// Return number of bytes per pixel
UINT32 ColorUtils::PixelBytes( ColorUtils::Format fmt )
{
    if (FormatIsOutOfRange(fmt))
        return 0;

    return ColorFormatTable[fmt].BytesPerPixel;
}

// Return number of bytes per pixel in .tga file
UINT32 ColorUtils::TgaPixelBytes( ColorUtils::Format fmt )
{
    switch (fmt)
    {
        case R5G6B5:
            return 3;

        case VE8YO8UE8YE8:
        case YO8VE8YE8UE8:
        case A1R5G5B5:
        case Z1R5G5B5:
        case X1R5G5B5:
            return 4;

        default:
            return PixelBytes(fmt);
    }
}

// Return number of bytes per pixel in .tga file
void ColorUtils::ColwertToTga( const char * SrcBuf,
                               char * DstBuf,
                               ColorUtils::Format fmt,
                               UINT32 pixels)
{
    switch (fmt)
    {
        case R5G6B5:
            for (UINT32 i = 0, j = 0, u = 0; u < pixels; ++u)
            {
                // B
                DstBuf[j] = ((SrcBuf[i] & 0x1F) << 3) | ((SrcBuf[i] >> 2) & 7);
                // G
                DstBuf[j+1] = ((SrcBuf[i] & 0xE0) >> 3) | ((SrcBuf[i + 1] & 0x07) << 5) | ((SrcBuf[i+1] >> 1) & 3);
                // R
                DstBuf[j+2] = (SrcBuf[i + 1] & 0xF8) | ((SrcBuf[i+1] >> 5) & 7);

                i += 2;
                j += 3;
            }
            return;
        case A1R5G5B5:
            for (UINT32 i = 0, j = 0, u = 0; u < pixels; ++u)
            {
                // B
                DstBuf[j] = ((SrcBuf[i] & 0x1F) << 3) | ((SrcBuf[i] >> 2) & 7);
                // G
                DstBuf[j+1] = ((SrcBuf[i] & 0xE0) >> 2) | ((SrcBuf[i + 1] & 0x03) << 6) |
                    ((SrcBuf[i+1] & 0x03) << 1) | ((SrcBuf[i] & 0x08) >> 7);
                // R
                DstBuf[j+2] = ((SrcBuf[i + 1] & 0x7C) << 1) | ((SrcBuf[i+1] >> 4) & 0x7);
                // A
                DstBuf[j+3] = (SrcBuf[i+1] & 0x80) >> 7;

                i += 2;
                j += 4;
            }
            return;

        case Z1R5G5B5:
        case X1R5G5B5:
            for (UINT32 i = 0, j = 0, u = 0; u < pixels; ++u)
            {
                // B
                DstBuf[j] = ((SrcBuf[i] & 0x1F) << 3) | ((SrcBuf[i] >> 2) & 7);
                // G
                DstBuf[j+1] = ((SrcBuf[i] & 0xE0) >> 2) | ((SrcBuf[i + 1] & 0x03) << 6) |
                    ((SrcBuf[i+1] & 0x03) << 1) | ((SrcBuf[i] & 0x08) >> 7);
                // R
                DstBuf[j+2] = ((SrcBuf[i + 1] & 0x7C) << 1) | ((SrcBuf[i+1] >> 4) & 0x7);

                i += 2;
                j += 4;
            }
            return;

        default:
            if (fmt < Format_NUM_FORMATS)
            {
                Platform::MemCopy(DstBuf, SrcBuf, PixelBytes(fmt) * pixels);
            }
            else
            {
                Printf(Tee::PriHigh,"*** ERROR in ColwertToTga:  %s is unknown format\n",
                    FormatToString(fmt).c_str());
                MASSERT(0);
            }
            return;
    }
}

// Return number of bytes per PNG pixel
UINT32 ColorUtils::PixelBytes(ColorUtils::PNGFormat fmt)
{
    switch (fmt)
    {
        case C1:
        case C2:
        case C4:
        case C8:
            return 1;

        case C16:
        case C8C8:
            return 2;

        case C8C8C8:
            return 3;

        case C16C16:
        case C8C8C8C8:
        case C10C10C10:
        case C10C10C10C2:
            return 4;

        case C16C16C16:
        case C12C12C12:
            return 6;

        case C16C16C16C16:
            return 8;

        case C32C32C32C32:
            return 16;

        default:
            MASSERT(0);
            /* fall through */
        case LWPNGFMT_NONE:
            return 0;
    }
}

// Return number of bits per pixel
UINT32 ColorUtils::PixelBits(ColorUtils::Format fmt)
{
    return ColorUtils::PixelBytes(fmt) * 8;
}

// Return number of bits per PNG pixel
UINT32 ColorUtils::PixelBits(ColorUtils::PNGFormat fmt)
{
    switch (fmt)
    {
        case C1:
            return 1;

        case C2:
            return 2;

        case C4:
            return 4;

        case C8:
            return 8;

        case C16:
        case C8C8:
            return 16;

        case C8C8C8:
            return 24;

        case C10C10C10:
            return 30;

        case C16C16:
        case C8C8C8C8:
        case C10C10C10C2:
            return 32;

        case C12C12C12:
            return 36;

        case C16C16C16:
            return 48;

        case C16C16C16C16:
            return 64;

        default:
            MASSERT(0);
            /* fall through */
        case LWPNGFMT_NONE:
            return 0;
    }
}

// Returns number of Z bits of a Z-buffer format
UINT32 ColorUtils::ZBits(Format fmt)
{
    switch (fmt)
    {
        case Z16:
            return 16;

        case Z24S8:
        case Z24:
        case S8Z24:
        case X8Z24:
        case V8Z24:
        case X8Z24_X16V8S8:
        case X4V4Z24__COV4R4V:
        case X4V4Z24__COV8R8V:
        case V8Z24__COV4R12V:
        case X8Z24_X20V4S8__COV4R4V:
        case X8Z24_X20V4S8__COV8R8V:
        case X8Z24_X16V8S8__COV4R12V:
        case V8Z24__COV8R24V:
        case X8Z24_X16V8S8__COV8R24V:
            return 24;

        case Z32:
        case ZF32:
        case ZF32_X24S8:
        case ZF32_X16V8X8:
        case ZF32_X16V8S8:
        case ZF32_X20V4X8__COV4R4V:
        case ZF32_X20V4X8__COV8R8V:
        case ZF32_X20V4S8__COV4R4V:
        case ZF32_X20V4S8__COV8R8V:
        case ZF32_X16V8X8__COV4R12V:
        case ZF32_X16V8S8__COV4R12V:
        case ZF32_X16V8X8__COV8R24V:
        case ZF32_X16V8S8__COV8R24V:
            return 32;

        default:
            return 0;
    }
}

// Returns number of stencil bits of a Z-buffer format
UINT32 ColorUtils::StencilBits(Format fmt)
{
    switch (fmt)
    {
        case Z24S8:
        case S8Z24:
        case X8Z24_X16V8S8:
        case ZF32_X24S8:
        case ZF32_X16V8S8:
        case X8Z24_X20V4S8__COV4R4V:
        case X8Z24_X20V4S8__COV8R8V:
        case ZF32_X20V4S8__COV4R4V:
        case ZF32_X20V4S8__COV8R8V:
        case X8Z24_X16V8S8__COV4R12V:
        case ZF32_X16V8S8__COV4R12V:
        case X8Z24_X16V8S8__COV8R24V:
        case ZF32_X16V8S8__COV8R24V:
        case S8:
            return 8;

        default:
            return 0;
    }
}

// Return number of elements per pixel
UINT32 ColorUtils::PixelElements(ColorUtils::Format fmt)
{
    if (FormatIsOutOfRange(fmt))
        return 0;

    return ColorFormatTable[fmt].PixelElements;
}

// Return number of elements per PNG pixel
UINT32 ColorUtils::PixelElements(ColorUtils::PNGFormat fmt)
{
    switch (fmt)
    {
        case C1:
        case C2:
        case C4:
        case C8:
        case C16:
            return 1;

        case C8C8:
        case C16C16:
            return 2;

        case C8C8C8:
        case C10C10C10:
        case C12C12C12:
        case C16C16C16:
            return 3;

        case C8C8C8C8:
        case C10C10C10C2:
        case C16C16C16C16:
            return 4;

        default:
            MASSERT(0);
            return 0;
    }
}

// Return whether two formats are in the same colorspace.
// (i.e. both integer rgb, both float rgb)  Formats in the
// same colorspace can be colwerted to/from each other
// using only bit-shift operations.
bool ColorUtils::IsCompatibleColorSpace(ColorUtils::Format fmt0, ColorUtils::Format fmt1)
{
    // simple case - formats are equal
    if (fmt0 == fmt1)
        return true;

    if (FormatIsOutOfRange(fmt0) || FormatIsOutOfRange(fmt1))
        return false;

    return (ColorFormatTable[fmt0].Space ==
            ColorFormatTable[fmt1].Space);
}

// Return the PNG file format that should be
// used to store data of the given color format.
ColorUtils::PNGFormat ColorUtils::FormatToPNGFormat(ColorUtils::Format fmt)
{
    switch (fmt)
    {
        case R5G6B5:
            return C8C8C8;          // encode as 8-bit RGB

        case A8R8G8B8:
        case R8G8B8A8:
        case VE8YO8UE8YE8:
        case YO8VE8YE8UE8:
        case AUDIOL16_AUDIOR16:
        case A8B8G8R8:
        case Z8R8G8B8:
        case B8G8R8A8:
        case A1R5G5B5:
            return C8C8C8C8;        // encode as 8-bit RGBA

        case A2R10G10B10:
        case R10G10B10A2:
        case A2V10Y10U10:
        case A2U10Y10V10:
        case A2B10G10R10:
        case AU2BU10GU10RU10:
            return C10C10C10C2;     // encode as 10-bit RGBA

        case CR8YB8CB8YA8:
        case YB8CR8YA8CB8:
            return C8C8C8C8;        // encode as 8-bit, 4 components

        case Z1R5G5B5:
            return C8C8C8C8;        // encode as 8-bit RGBA with Z in A

        case Z24S8:
            return C8C8C8C8;        // pack into 4 bytes (no PNG standard fits)

        case Z16:
        case RF16:
        case VOID16:
        case CPSTY8CPSTC8:
            return C16;             // encode as 16-bit grayscale

        case RF32:
        case VOID32:
        case Z32:
            return C8C8C8C8;        // pack into 4 bytes (no PNG standard fits)

        case RF16_GF16_BF16_AF16:
        case YE16_UE16_YO16_VE16:
        case YE10Z6_UE10Z6_YO10Z6_VE10Z6:
        case UE16_YE16_VE16_YO16:
        case UE10Z6_YE10Z6_VE10Z6_YO10Z6:
        case AUDIOL32_AUDIOR32:
            return C16C16C16C16;    // encode as 16-bit RGBA

        case Y8:
        case U8:
        case V8:
        case I8:
        case CPST8:
            return C8;              // encode as 8-bit grayscale

        case B8_G8_R8:
            return C8C8C8;          // encode as 8-bit RGB

        case Z24:
            return C8C8C8;          // pack into 3-bytes (no PNG standard fits)

        case AN8BN8GN8RN8:
        case AS8BS8GS8RS8:
        case AU8BU8GU8RU8:
        case A8RL8GL8BL8:
        case A8BL8GL8RL8:
            return C8C8C8C8;        // encode as 8-bit RGBA

        case X8R8G8B8:
        case X1R5G5B5:
        case X8B8G8R8:
        case X8RL8GL8BL8:
        case X8BL8GL8RL8:
            return C8C8C8;          // encode as 8-bit RGB

        case R16_G16_B16_A16:
        case RN16_GN16_BN16_AN16:
        case RU16_GU16_BU16_AU16:
        case RS16_GS16_BS16_AS16:
        case RF16_GF16_BF16_X16:
            return C16C16C16C16;    // encode as 16-bit RGBA

        case RF32_GF32_BF32_AF32:
        case RF32_GF32_BF32_X32:
        case RS32_GS32_BS32_AS32:
        case RS32_GS32_BS32_X32:
        case RU32_GU32_BU32_AU32:
        case RU32_GU32_BU32_X32:
            return C32C32C32C32;    // encode as 32-bit RGBA

        case RF32_GF32:
        case RS32_GS32:
        case RU32_GU32:
            return C16C16C16C16;    // encode as 8 bytes (no PNG standard fits)

        case RS32:
        case RU32:
            return C8C8C8C8;        // encode as 4 bytes (no PNG standard fits)

        case RF16_GF16:
        case RS16_GS16:
        case RN16_GN16:
        case RU16_GU16:
        case R16_G16:
            return C16C16;          // encode as 16-bit pair

        case G8R8:
        case GN8RN8:
        case GS8RS8:
        case GU8RU8:
        case U8V8:
        case V8U8:
            return C8C8;            // encode as 8-bit pair

        case R16:
        case RN16:
        case RS16:
        case RU16:
            return C16;

        case R8:
        case RN8:
        case RS8:
        case RU8:
        case A8:
        case S8:
            return C8;

        case ZF32:
        case S8Z24:
        case X8Z24:
        case V8Z24:
            return C8C8C8C8;        // encode as 4 bytes (no PNG standard fits)

        case ZF32_X24S8:
        case X8Z24_X16V8S8:
        case ZF32_X16V8X8:
        case ZF32_X16V8S8:
            return C16C16C16C16;    // encode as 8 bytes (no PNG standard fits)

        case Y8_U8__Y8_V8_N422:
        case U8_Y8__V8_Y8_N422:
            return C8C8C8C8;

        case Y8___U8V8_N444:
        case Y8___U8V8_N422:
        case Y8___U8V8_N422R:
        case Y8___V8U8_N420:
        case Y8___U8___V8_N444:
        case Y8___U8___V8_N420:
            return C8C8C8;

        case Y10___U10V10_N444:
        case Y10___U10V10_N422:
        case Y10___U10V10_N422R:
        case Y10___V10U10_N420:
        case Y10___U10___V10_N444:
        case Y10___U10___V10_N420:
            return C10C10C10;

        case Y12___U12V12_N444:
        case Y12___U12V12_N422:
        case Y12___U12V12_N422R:
        case Y12___V12U12_N420:
        case Y12___U12___V12_N444:
        case Y12___U12___V12_N420:
            return C12C12C12;

        // FIXME: Texture formats are not covered

        default:                    // other formats are not supported by PNG
            return LWPNGFMT_NONE;
    }
}

// Fill all channels with the alpha channel value.
// This does nothing unless the format has R,G,B, and A channels.
void ColorUtils::CopyAlphaToRgb(char* pixel, ColorUtils::Format fmt)
{
    switch (fmt)
    {
        case A8R8G8B8:
        case AN8BN8GN8RN8:
        case AS8BS8GS8RS8:
        case AU8BU8GU8RU8:
        case A8RL8GL8BL8:
        case A8BL8GL8RL8:
        {
            UINT32 *pix = (UINT32*)(pixel);
            UINT32 A = (*pix) >> 24;
            *pix = (A << 24) | (A << 16) | (A << 8) | A;
            break;
        }

        case R8G8B8A8:
        {
            UINT32 *pix = (UINT32*)(pixel);
            UINT32 A = (*pix) & 0x000000FF;
            *pix = A | (A >> 8) | (A >> 16) | (A >> 24);
            break;
        }

        case A2R10G10B10:
        case AU2BU10GU10RU10:
        {
            UINT32 *pix = (UINT32*)(pixel);
            UINT32 A    = (*pix) >> 22;
            *pix        = (A << 22) | (A << 20) | (A << 10) | A;
            break;
        }

        case R10G10B10A2:
        {
            UINT32 *pix = (UINT32*)(pixel);
            UINT32 A    = ((*pix) & 0x00000003) << 8;
            *pix        = (A >> 8) | (A << 2) | (A << 12) | (A << 22);
            break;
        }

        case RF16_GF16_BF16_AF16:
        case R16_G16_B16_A16:
        case RN16_GN16_BN16_AN16:
        case RU16_GU16_BU16_AU16:
        case RS16_GS16_BS16_AS16:
        {
            UINT16 *pix = (UINT16*)(pixel);
            pix[0] = pix[3];
            pix[1] = pix[3];
            pix[2] = pix[3];
            break;
        }

        case RF32_GF32_BF32_AF32:
        case RS32_GS32_BS32_AS32:
        case RU32_GU32_BU32_AU32:
        {
            UINT32 *pix = (UINT32*)(pixel);
            pix[0] = pix[3];
            pix[1] = pix[3];
            pix[2] = pix[3];
            break;
        }

        case A8:
            break;

        // FIXME: Texture formats are not covered

        default:
            if (fmt >= Format_NUM_FORMATS)
            {
                MASSERT(0);
            }
            break;
    }
}

// MSB replicate x to fill out 32 bits of precision.
// x should originally fill the lower bits of a 32 bit value (right-justified).
// The return value has its MSB left-justified.
static inline UINT32 MSBReplicate(UINT32 x, int bits)
{
    switch (bits)
    {
        case 0:
            return 0;
        case 1:
            return x ? 0xffffffff : 0;
        case 2:
            return x * 0x55555555;
        case 5:
            return (x * 0x08421084) | (x >> 3);
        case 6:
            return (x * 0x04104104) | (x >> 4);
        case 8:
            return (x << 24) | (x << 16) | (x << 8) | x;
        case 10:
            return (x << 22) | (x << 12) | (x << 2) | (x >> 8);
        case 16:
            return (x << 16) | x;
        case 32:
            return x;
        default:
            x <<= (32 - bits);
            while (bits < 32)
            {
                x |= (x >> bits);
            }
            return x;
    }
}

// Extract the bitfield hiBit:loBit from the value, and return a
// 32-bit number containing the bitfield in left-justified replicated
// form.
// Example: MSBReplicateField(0x12345678, 15, 8) == 0x56565656
static inline UINT32 MSBReplicateField(UINT32 value, int hiBit, int loBit)
{
    const int numBits = hiBit - loBit + 1;
    return MSBReplicate((value >> loBit) & ((1 << numBits) - 1), numBits);
}

// Take a replicated number created by MSBReplicateField(), and copy
// it into the bitfield hiBit:loBit.
static inline UINT32 MSBRetrieveField(UINT32 value, int hiBit, int loBit)
{
    const UINT32 mask = ((1 << (hiBit - loBit + 1)) - 1) << loBit;
    return (value >> (31 - hiBit)) & mask;
}

// Colwert from one color format to another.
UINT32 ColorUtils::Colwert(UINT32 in,
                           ColorUtils::Format from,
                           ColorUtils::Format to)
{
    if (from == to)
        return in;

    /* V, Y and U components are usually not colwertable to RGBA
     * The only time when we may need to do that is to dump
     * FB in this format into a .tga file
     * Convention:
     *  V -> R
     *  Y -> G
     *  U -> B
     */

    UINT32 R = 0;           // 32-bit red extracted from in
    UINT32 G = 0;           // 32-bit green extracted from in
    UINT32 B = 0;           // 32-bit blue extracted from in
    UINT32 A = 0xffffffff;  // 32-bit alpha extracted from in
    UINT32 out;

    // @@@ float formats are missing!

    switch (from)
    {
        case Y8:
        case I8:
        case CPST8:
        case S8:
        {
            R = MSBReplicateField(in, 7, 0);
            G = R;
            B = R;
            break;
        }
        case Z16:
        {
            // If this is repacked into Z24S8, Z will be <<8, and stencil will be 0.
            // If this is repacked into a 24-bit .TGA file, alpha is discarded.
            A = 0;
            R = MSBReplicateField(in, 15, 8);
            G = MSBReplicateField(in,  7, 0);
            B = 0;
            break;
        }
        case R16:
        {
            R = MSBReplicateField(in, 15, 0);
            B = 0;
            break;
        }
        case R16_G16:
        {
            R = MSBReplicateField(in, 31, 16);
            G = MSBReplicateField(in, 15,  0);
            B = 0;
            break;
        }
        case R5G6B5:
        case VOID16: // Treat VOID16 as R5G6B5 for display tests
        {
            R = MSBReplicateField(in, 15, 11);
            G = MSBReplicateField(in, 10,  5);
            B = MSBReplicateField(in,  4,  0);
            break;
        }
        case U8V8:
        {
            R = MSBReplicateField(in, 15, 8);
            G = MSBReplicateField(in,  7, 0);
            B = 0;
            break;
        }
        case X8R8G8B8:
        {
            R  = MSBReplicateField(in, 23, 16);
            G  = MSBReplicateField(in, 15,  8);
            B  = MSBReplicateField(in,  7,  0);
            break;
        }
        case A8R8G8B8:
        {
            A  = MSBReplicateField(in, 31, 24);
            R  = MSBReplicateField(in, 23, 16);
            G  = MSBReplicateField(in, 15,  8);
            B  = MSBReplicateField(in,  7,  0);
            break;
        }
        case R8G8B8A8:
        case Z24S8:
        case Z32:
        {
            R  = MSBReplicateField(in, 31, 24);
            G  = MSBReplicateField(in, 23, 16);
            B  = MSBReplicateField(in, 15,  8);
            A  = MSBReplicateField(in,  7,  0);
            break;
        }
        case A2R10G10B10:
        {
            A  = MSBReplicateField(in, 31, 30);
            R  = MSBReplicateField(in, 29, 20);
            G  = MSBReplicateField(in, 19, 10);
            B  = MSBReplicateField(in,  9, 00);
            break;
        }
        case R10G10B10A2:
        {
            R = MSBReplicateField(in, 31, 22);
            G = MSBReplicateField(in, 21, 12);
            B = MSBReplicateField(in, 11,  2);
            A = MSBReplicateField(in,  1,  0);
            break;
        }
        case VOID32:
        {
            A = R = G = B = in;
            break;
        }
        case A2B10G10R10:
        case A2U10Y10V10:
        case X2BL10GL10RL10_XRBIAS:
        case X2BL10GL10RL10_XVYCC:
        {
            A = MSBReplicateField(in, 31, 30);
            B = MSBReplicateField(in, 29, 20);
            G = MSBReplicateField(in, 19, 10);
            R = MSBReplicateField(in,  9,  0);
            break;
        }

        case A8B8G8R8:
        {
            A = MSBReplicateField(in, 31, 24);
            B = MSBReplicateField(in, 23, 16);
            G = MSBReplicateField(in, 15,  8);
            R = MSBReplicateField(in,  7,  0);
            break;
        }
        case Z1R5G5B5:
        case A1R5G5B5:
        {
            A = MSBReplicateField(in, 15, 15);
            R = MSBReplicateField(in, 14, 10);
            G = MSBReplicateField(in,  9,  5);
            B = MSBReplicateField(in,  4,  0);
            break;
        }
        case Z8R8G8B8:
        {
            R = MSBReplicateField(in, 23, 16);
            G = MSBReplicateField(in, 15,  8);
            B = MSBReplicateField(in,  7,  0);
            break;
        }
        case A2V10Y10U10:
        {
            A = MSBReplicateField(in, 31, 30);
            R = MSBReplicateField(in, 29, 20);
            G = MSBReplicateField(in, 19, 10);
            B = MSBReplicateField(in,  9,  0);
            break;
        }
        case VE8YO8UE8YE8:
        case YO8VE8YE8UE8:
        {
            A = MSBReplicateField(in, 31, 24);
            R = MSBReplicateField(in, 23, 16);
            G = MSBReplicateField(in, 15,  8);
            B = MSBReplicateField(in,  7,  0);
            break;
        }
        case Y8_U8__Y8_V8_N422:
        {
            G = MSBReplicateField(in, 31, 24);
            B = MSBReplicateField(in, 23, 16);
            A = MSBReplicateField(in, 15,  8);
            R = MSBReplicateField(in,  7,  0);
            break;
        }
        case U8_Y8__V8_Y8_N422:
        {
            B = MSBReplicateField(in, 31, 24);
            G = MSBReplicateField(in, 23, 16);
            R = MSBReplicateField(in, 15,  8);
            A = MSBReplicateField(in,  7,  0);
            break;
        }
        case Y8___U8V8_N444:
        case Y8___U8V8_N422:
        case Y8___U8V8_N422R:
        case Y8___U8___V8_N444:
        case Y8___U8___V8_N420:
        {
            G = MSBReplicateField(in, 23, 16);
            B = MSBReplicateField(in, 15, 8);
            R = MSBReplicateField(in, 7, 0);
            break;
        }
        case Y8___V8U8_N420:
        {
            G = MSBReplicateField(in, 23, 16);
            R = MSBReplicateField(in, 15, 8);
            B = MSBReplicateField(in, 7, 0);
            break;
        }
        case Y10___U10V10_N444:
        case Y10___U10V10_N422:
        case Y10___U10V10_N422R:
        case Y10___U10___V10_N444:
        case Y10___U10___V10_N420:
        {
            G = MSBReplicateField(in, 29, 20);
            B = MSBReplicateField(in, 19, 10);
            R = MSBReplicateField(in, 9, 0);
            break;
        }
        case Y10___V10U10_N420:
        {
            G = MSBReplicateField(in, 29, 20);
            R = MSBReplicateField(in, 19, 10);
            B = MSBReplicateField(in, 9, 0);
            break;
        }
        case Y12___U12V12_N444:
        case Y12___U12V12_N422:
        case Y12___U12V12_N422R:
        case Y12___U12___V12_N444:
        case Y12___U12___V12_N420:
        {
            G = MSBReplicateField(in, 35, 24);
            B = MSBReplicateField(in, 23, 12);
            R = MSBReplicateField(in, 11, 0);
            break;
        }
        case Y12___V12U12_N420:
        {
            G = MSBReplicateField(in, 35, 24);
            R = MSBReplicateField(in, 23, 12);
            B = MSBReplicateField(in, 11, 0);
            break;
        }
        default:
            MASSERT(!"Unsupported FROM format in ColorUtils::Colwert");
            return 0;
    }

    switch (to)
    {
        case Y8:
        case I8:
        case CPST8:
        case S8:
        {
            out = MSBRetrieveField(R, 7, 0);
            break;
        }
        case R5G6B5:
        case VOID16: // Treat VOID16 as R5G6B5 for display tests
        {
            out = (MSBRetrieveField(R, 15, 11) |
                   MSBRetrieveField(G, 10,  5) |
                   MSBRetrieveField(B,  4,  0));
            break;
        }
        case Z16:
        {
            out = (MSBRetrieveField(R, 15,  8) |
                   MSBRetrieveField(G,  7,  0));
            break;
        }
        case U8V8:
        case V8U8:
        {
            out = (MSBRetrieveField(R, 15, 8) |
                   MSBRetrieveField(G,  7, 0));
            break;
        }
        case R16:
        {
            out = MSBRetrieveField(R, 15, 0);
            break;
        }
        case R16_G16:
        {
            out = (MSBRetrieveField(R, 31, 16) |
                   MSBRetrieveField(G, 15,  0));
            break;
        }
        case X8R8G8B8:
        {
            out = (MSBRetrieveField(R, 23, 16) |
                   MSBRetrieveField(G, 15,  8) |
                   MSBRetrieveField(B,  7,  0));
            break;
        }
        case X8B8G8R8:
        {
            out = (MSBRetrieveField(B, 23, 16) |
                   MSBRetrieveField(G, 15,  8) |
                   MSBRetrieveField(R,  7,  0));
            break;
        }
        case A8R8G8B8:
        {
            out = (MSBRetrieveField(A, 31, 24) |
                   MSBRetrieveField(R, 23, 16) |
                   MSBRetrieveField(G, 15,  8) |
                   MSBRetrieveField(B,  7,  0));
            break;
        }
        case R8G8B8A8:
        case Z24S8:
        case Z32:
        {
            out = (MSBRetrieveField(R, 31, 24) |
                   MSBRetrieveField(G, 23, 16) |
                   MSBRetrieveField(B, 15,  8) |
                   MSBRetrieveField(A,  7,  0));
            break;
        }
        case A2R10G10B10:
        {
            out = (MSBRetrieveField(A, 31, 30) |
                   MSBRetrieveField(R, 29, 20) |
                   MSBRetrieveField(G, 19, 10) |
                   MSBRetrieveField(B,  9,  0));
            break;
         }
         case R10G10B10A2:
         {
            out = (MSBRetrieveField(R, 31, 22) |
                   MSBRetrieveField(G, 21, 12) |
                   MSBRetrieveField(B, 11,  2) |
                   MSBRetrieveField(A,  1,  0));
             break;
        }
        case VOID32:
        {
            out = G;
            break;
        }
        case A2B10G10R10:
        case A2U10Y10V10:
        case X2BL10GL10RL10_XRBIAS:
        case X2BL10GL10RL10_XVYCC:
        {
            out = (MSBRetrieveField(A, 31, 30) |
                   MSBRetrieveField(B, 29, 20) |
                   MSBRetrieveField(G, 19, 10) |
                   MSBRetrieveField(R,  9,  0));
             break;
         }
        case A8B8G8R8:
        {
            out = (MSBRetrieveField(A, 31, 24) |
                   MSBRetrieveField(B, 23, 16) |
                   MSBRetrieveField(G, 15,  8) |
                   MSBRetrieveField(R,  7,  0));
            break;
        }
        case A1R5G5B5:
        case Z1R5G5B5:
        {
            out = (MSBRetrieveField(A, 15, 15) |
                   MSBRetrieveField(R, 14, 10) |
                   MSBRetrieveField(G,  9,  5) |
                   MSBRetrieveField(B,  4,  0));
            break;
        }
        case R5G5B5A1:
        {
            out = (MSBRetrieveField(A,  0,  0) |
                   MSBRetrieveField(R, 15, 11) |
                   MSBRetrieveField(G, 10,  6) |
                   MSBRetrieveField(B,  5,  1));
            break;
        }
        case Z8R8G8B8:
        {
            out = (MSBRetrieveField(R, 23, 16) |
                   MSBRetrieveField(G, 15,  8) |
                   MSBRetrieveField(B,  7,  0));
            break;
        }
        case R4G4B4A4:
        {
            out = (MSBRetrieveField(A,  3,  0) |
                   MSBRetrieveField(R, 15, 12) |
                   MSBRetrieveField(G, 11,  8) |
                   MSBRetrieveField(B,  7,  4));
            break;
        }
        case VE8YO8UE8YE8:
        case YO8VE8YE8UE8:
        {
            out = (MSBRetrieveField(A, 31, 24) |
                   MSBRetrieveField(R, 23, 16) |
                   MSBRetrieveField(G, 15,  8) |
                   MSBRetrieveField(B,  7,  0));
            break;
        }
        case Y8_U8__Y8_V8_N422:
        {
            out = (MSBRetrieveField(G, 31, 24) |
                   MSBRetrieveField(B, 23, 16) |
                   MSBRetrieveField(A, 15,  8) |
                   MSBRetrieveField(R,  7,  0));
            break;
        }
        case U8_Y8__V8_Y8_N422:
        {
            out = (MSBRetrieveField(B, 31, 24) |
                   MSBRetrieveField(G, 23, 16) |
                   MSBRetrieveField(R, 15,  8) |
                   MSBRetrieveField(A,  7,  0));
            break;
        }
        case Y8___U8V8_N444:
        case Y8___U8V8_N422:
        case Y8___U8V8_N422R:
        case Y8___U8___V8_N444:
        case Y8___U8___V8_N420:
        {
            out = (MSBRetrieveField(G, 23, 16) |
                   MSBRetrieveField(B, 15, 8) |
                   MSBRetrieveField(R, 7,  0));
            break;
        }
        case Y8___V8U8_N420:
        {
            out = (MSBRetrieveField(G, 23, 16) |
                   MSBRetrieveField(R, 15, 8) |
                   MSBRetrieveField(B, 7,  0));
            break;
        }
        case Y10___U10V10_N444:
        case Y10___U10V10_N422:
        case Y10___U10V10_N422R:
        case Y10___U10___V10_N444:
        case Y10___U10___V10_N420:
        {
            out = (MSBRetrieveField(G, 29, 20) |
                   MSBRetrieveField(B, 19, 10) |
                   MSBRetrieveField(R, 9, 0));
            break;
        }
        case Y10___V10U10_N420:
        {
            out = (MSBRetrieveField(G, 29, 20) |
                   MSBRetrieveField(R, 19, 10) |
                   MSBRetrieveField(B, 9, 0));
            break;
        }
        case Y12___U12V12_N444:
        case Y12___U12V12_N422:
        case Y12___U12V12_N422R:
        case Y12___U12___V12_N444:
        case Y12___U12___V12_N420:
        {
            out = (MSBRetrieveField(G, 35, 24) |
                   MSBRetrieveField(B, 23, 12) |
                   MSBRetrieveField(R, 11, 0));
            break;
        }
        case Y12___V12U12_N420:
        {
            out = (MSBRetrieveField(G, 35, 24) |
                   MSBRetrieveField(R, 23, 12) |
                   MSBRetrieveField(B, 11, 0));
            break;
        }
        default:
            MASSERT(!"Unsupported TO format in ColorUtils::Colwert");
            return 0;
   }
   return out;
}

// Read pixel data from a PNG buffer.
// Read in big-endian order; all PNG files are big-endian.
// Normalize the return value to 0 - 0xffffffff, replicating bits as needed.
//
static inline UINT32 ReadPng08(const void *pBuffer, UINT32 index)
{
    UINT32 byte0 = ((const UINT08*)pBuffer)[index];
    return (byte0 << 24) + (byte0 << 16) + (byte0 << 8) + (byte0);
}

static inline UINT32 ReadPng16(const void *pBuffer, UINT32 index)
{
    UINT32 byte0 = ((const UINT08*)pBuffer)[2 * index];
    UINT32 byte1 = ((const UINT08*)pBuffer)[2 * index + 1];
    return (byte0 << 24) + (byte1 << 16) + (byte0 << 8) + (byte1);
}

static inline UINT32 ReadPng32(const void *pBuffer, UINT32 index)
{
    UINT32 byte0 = ((const UINT08*)pBuffer)[4 * index];
    UINT32 byte1 = ((const UINT08*)pBuffer)[4 * index + 1];
    UINT32 byte2 = ((const UINT08*)pBuffer)[4 * index + 2];
    UINT32 byte3 = ((const UINT08*)pBuffer)[4 * index + 3];
    return (byte0 << 24) + (byte1 << 16) + (byte2 << 8) + (byte3);
}

// Colwert a single pixel from PNG file storage color format.
// Integer color values are truncated when colwerting from
// PNG formats with higher color depth.
void ColorUtils::Colwert(const char *in,
                         ColorUtils::PNGFormat from,
                         char *out,
                         ColorUtils::Format to)
{
    // pixel components
    UINT32 c0 = 0;
    UINT32 c1 = 0;
    UINT32 c2 = 0;
    UINT32 c3 = 0;

    // extract components of in
    switch (from)
    {
        // one component cases - replicate the first component
        // in the second two and make the fourth its max value
        // this makes possible grayscale -> RGB or RGBA colwersion
        case C1:
        {
            c0 = ReadPng08(in, 0) & 0x80000000;
            c1 = c0;
            c2 = c0;
            c3 = 0xFFFFFFFF;
            break;
        }
        case C2:
        {
            c0 = ReadPng08(in, 0) & 0xC0000000;
            c1 = c0;
            c2 = c0;
            c3 = 0xFFFFFFFF;
            break;
        }
        case C4:
        {
            c0 = ReadPng08(in, 0) & 0xF0000000;
            c1 = c0;
            c2 = c0;
            c3 = 0xFFFFFFFF;
            break;
        }
        case C8:
        {
            c0 = ReadPng08(in, 0);
            c1 = c0;
            c2 = c0;
            c3 = 0xFFFFFFFF;
            break;
        }
        case C16:
        {
            c0 = ReadPng16(in, 0);
            c1 = c0;
            c2 = c0;
            c3 = 0xFFFFFFFF;
            break;
        }
        // two component cases - just extract the components
        case C8C8:
        {
            c0 = ReadPng08(in, 0);
            c1 = ReadPng08(in, 1);
            c2 = 0x00000000;
            c3 = 0x00000000;
            break;
        }
        case C16C16:
        {
            c0 = ReadPng16(in, 0);
            c1 = ReadPng16(in, 1);
            c2 = 0x00000000;
            c3 = 0x00000000;
            break;
        }
        // three component cases - extract the components and
        // set the forth component to to its max value
        // this makes possible RGB -> RGBA colwersion
        case C8C8C8:
        {
            c0 = ReadPng08(in, 0);
            c1 = ReadPng08(in, 1);
            c2 = ReadPng08(in, 2);
            c3 = 0xFFFFFFFF;
            break;
        }
        case C16C16C16:
        {
            c0 = ReadPng16(in, 0);
            c1 = ReadPng16(in, 1);
            c2 = ReadPng16(in, 2);
            c3 = 0xFFFFFFFF;
            break;
        }
        // four component cases - just extract the components
        case C8C8C8C8:
        {
            c0 = ReadPng08(in, 0);
            c1 = ReadPng08(in, 1);
            c2 = ReadPng08(in, 2);
            c3 = ReadPng08(in, 3);
            break;
        }
        case C10C10C10C2:
        {
            const UINT32 tmp = ReadPng32(in, 0);
            c0 = (tmp)       & 0xFFC00000;
            c1 = (tmp << 10) & 0xFFC00000;
            c2 = (tmp << 20) & 0xFFC00000;
            c3 = (tmp << 30) & 0xC0000000;
            break;
        }
        case C10C10C10:
        {
            const UINT32 tmp = ReadPng32(in, 0);
            c0 = (tmp)       & 0xFFC00000;
            c1 = (tmp << 10) & 0xFFC00000;
            c2 = (tmp << 20) & 0xFFC00000;
            c3 = 0xFFFFFFFF;
            break;
        }
        case C12C12C12:
        {
            const UINT32 tmp0 = ReadPng32(in, 0);
            const UINT32 tmp1 = ReadPng08(in, 4);
            c0 = (tmp0)       & 0xFFF00000;
            c1 = (tmp0 << 12) & 0xFFF00000;
            c2 = ((tmp0 << 24) & 0xFFF00000) & tmp1;
            c3 = 0xFFFFFFFF;
            break;
        }
        case C16C16C16C16:
        {
            c0 = ReadPng16(in, 0);
            c1 = ReadPng16(in, 1);
            c2 = ReadPng16(in, 2);
            c3 = ReadPng16(in, 3);
            break;
        }
        case C32C32C32C32:
        {
            c0 = ReadPng32(in, 0);
            c1 = ReadPng32(in, 1);
            c2 = ReadPng32(in, 2);
            c3 = ReadPng32(in, 3);
            break;
        }
        default:
            // invalid or unsupported format specified
            MASSERT(0);
            break;
    }

    switch (to)
    {
        case X8R8G8B8:
        {
            c3 = 0; // 0 out Alpha channel and intentionally fall through to
                    // A8R8G8B8 case
        }
        case A8R8G8B8:
        {
            UINT32 temp = c3;
            c3 = c2; c2 = c1; c1 = c0;
            c0 = temp;
            break;
        }
        case A8B8G8R8:
        {
            swap(c0, c3);
            swap(c1, c2);
            break;
        }
        case A2R10G10B10:
        {
            UINT32 temp = c3;
            c3 = c2; c2 = c1; c1 = c0;
            c0 = temp; // Now, c0 contains alpha
            break;
        }
        case A2B10G10R10:
        case A2U10Y10V10:
        case X2BL10GL10RL10_XRBIAS:
        case X2BL10GL10RL10_XVYCC:
        {
            swap(c0, c3);
            swap(c1, c2);
            break;
        }
        case CR8YB8CB8YA8:
        {
            UINT32 temp;
            temp = c0; c0 = c1; c1 = temp;
            temp = c2; c2 = c3; c3 = temp;
            break;
        }
        case Z1R5G5B5:
        {
            UINT32 temp = c3;
            c3 = c2; c2 = c1; c1 = c0;
            c0 = temp;
            break;
        }
        case B8_G8_R8:
        {
            UINT32 temp = c0;
            c0 = c2;
            c2 = temp;
            break;
        }
        default:
            // components already in correct order
            break;
    }

    // place components into out
    switch (to)
    {
        case R5G6B5:
        case VOID16:
        {
            UINT16 *out_ptr = (UINT16*)(out);
            *out_ptr = (UINT16)(
                    ((c0 & 0xF8000000) >> 16) |
                    ((c1 & 0xFC000000) >> 21) |
                    ((c2 & 0xF8000000) >> 27));
            break;
        }
        case A1R5G5B5:
        {
            UINT16 *out_ptr = (UINT16*)(out);
            *out_ptr = (UINT16)(
                    ((c0 & 0xF8000000) >> 17) |
                    ((c1 & 0xF8000000) >> 22) |
                    ((c2 & 0xF8000000) >> 27) |
                    ((c3 & 0x80000000) >> 16));
            break;
        }
        case X8R8G8B8:
        case A8B8G8R8:
        case A8R8G8B8:
        case R8G8B8A8:
        case CR8YB8CB8YA8:
        case YB8CR8YA8CB8:
        case Z24S8:
        case RF32:
        case Z32:
        case Y8_U8__Y8_V8_N422:
        case U8_Y8__V8_Y8_N422:
        {
            UINT32 *out_ptr = (UINT32*)(out);
            *out_ptr =  (c0 & 0xFF000000) |
                ((c1 & 0xFF000000) >> 8) |
                ((c2 & 0xFF000000) >> 16) |
                ((c3 & 0xFF000000) >> 24);
            break;
        }
        case R10G10B10A2:
        {
            UINT32 *out_ptr = (UINT32*)(out);
            *out_ptr =  (c0 & 0xFFC00000)        |
                ((c1 & 0xFFC00000) >> 10) |
                ((c2 & 0xFFC00000) >> 20) |
                ((c3 & 0xC0000000) >> 30);
            break;
        }
        case A2B10G10R10:
        case A2U10Y10V10:
        case A2R10G10B10:
        case X2BL10GL10RL10_XRBIAS:
        case X2BL10GL10RL10_XVYCC:
        {
            UINT32 *out_ptr = (UINT32*)(out);
            *out_ptr =  (c0 & 0xC0000000)        |
                ((c1 & 0xFFC00000) >>  2) |
                ((c2 & 0xFFC00000) >> 12) |
                ((c3 & 0xFFC00000) >> 22);
            break;
        }
        case Z1R5G5B5:
        {
            UINT16 *out_ptr = (UINT16*)(out);
                *out_ptr = (UINT16)(
                    ((c0 & 0x80000000) >> 16) |
                    ((c1 & 0xF8000000) >> 17) |
                    ((c2 & 0xF8000000) >> 22) |
                    ((c3 & 0xF8000000) >> 27));
            break;
        }
        case Z16:
        {
            UINT16 *out_ptr = (UINT16*)(out);
            *out_ptr = (UINT16)(
                 ((c0 & 0xFF000000) >> 16) |
                 ((c1 & 0xFF000000) >> 24));
            break;
        }
        case RF16:
        case R16:
        {
            UINT16 *out_ptr = (UINT16*)(out);
            *out_ptr = (UINT16)((c0 & 0xFFFF0000) >> 16);
            break;
        }
        case R16_G16:
        {
            UINT32 *out_ptr = (UINT32*)(out);
            *out_ptr = (((c0 & 0xFFFF0000) >> 0) |
                        ((c1 & 0xFFFF0000) >> 16));
            break;
        }
        case RF16_GF16_BF16_AF16:
        {
            /*
             * the code here divides by 0x3f instead of 65535 because of bug 213896
             * so far this has been wnf all the way through gf10x (see bug 272301)
             * if gf11x fixes this (a deeper pipe is being developed) then this code needs
             * to be modified
             * effectively what's happening is that both FB and display multiply by 1024
             * so this code needs to divide by (65536 / 1024) - 1 == 63 == 0x3f
             * added the comment to avoid debugging this (as we did in bug 488947)
             */
            const UINT32 div = GetFP16Divide(); // 0x3f (or 65536 on gf11x+)
            UINT16 *out_ptr = (UINT16*)(out);
            out_ptr[0] = Utility::Float32ToFloat16((float)((c0 & 0xFFFF0000) >> 16)/div);
            out_ptr[1] = Utility::Float32ToFloat16((float)((c1 & 0xFFFF0000) >> 16)/div);
            out_ptr[2] = Utility::Float32ToFloat16((float)((c2 & 0xFFFF0000) >> 16)/div);
            out_ptr[3] = Utility::Float32ToFloat16((float)((c3 & 0xFFFF0000) >> 16)/div);
            break;
        }
        case Y8:
        case I8:
        case S8:
        {
            *((UINT08*)(out)) = (UINT08)((c0 & 0xFF000000) >> 24);
            break;
        }
        case B8_G8_R8:
        case Y8___U8V8_N444:
        case Y8___U8V8_N422:
        case Y8___U8V8_N422R:
        case Y8___V8U8_N420:
        case Y8___U8___V8_N444:
        case Y8___U8___V8_N420:
        case Z24:
        {
            UINT08 *out_ptr = (UINT08*)(out);
            out_ptr[2] = (UINT08)((c0 & 0xFF000000) >> 24);
            out_ptr[1] = (UINT08)((c1 & 0xFF000000) >> 24);
            out_ptr[0] = (UINT08)((c2 & 0xFF000000) >> 24);
            break;
        }
        case U8V8:
        {
            UINT08 *out_ptr = (UINT08*)(out);
            out_ptr[1] = (UINT08)((c0 & 0xFF000000) >> 24);
            out_ptr[0] = (UINT08)((c1 & 0xFF000000) >> 24);
            break;
        }
        case RF32_GF32_BF32_AF32:
        {
            float *out_ptr = (float*)(out);
            out_ptr[0] = (float)((c0 & 0xFFFF0000) >> 16)/0x3f;
            out_ptr[1] = (float)((c1 & 0xFFFF0000) >> 16)/0x3f;
            out_ptr[2] = (float)((c2 & 0xFFFF0000) >> 16)/0x3f;
            out_ptr[3] = (float)((c3 & 0xFFFF0000) >> 16)/0x3f;
            break;
        }
        case R16_G16_B16_A16:
        case RS16_GS16_BS16_AS16:
        case RU16_GU16_BU16_AU16:
        case R16_G16_B16_A16_LWBIAS:
        {
            UINT16 * out_ptr = (UINT16*)(out);
            out_ptr[0] = ((c0 & 0xFFFF0000) >> 16);
            out_ptr[1] = ((c1 & 0xFFFF0000) >> 16);
            out_ptr[2] = ((c2 & 0xFFFF0000) >> 16);
            out_ptr[3] = ((c3 & 0xFFFF0000) >> 16);
            break;
        }
        case Y10___U10V10_N444:
        case Y10___U10V10_N422:
        case Y10___U10V10_N422R:
        case Y10___V10U10_N420:
        case Y10___U10___V10_N444:
        case Y10___U10___V10_N420:
        {
            UINT32 *out_ptr = (UINT32*)(out);
            *out_ptr =  (c0 & 0xFFC00000) |
                ((c1 & 0xFFC00000) >>  10)  |
                ((c2 & 0xFFC00000) >> 20);
            break;
        }
        case Y12___U12V12_N444:
        case Y12___U12V12_N422:
        case Y12___U12V12_N422R:
        case Y12___V12U12_N420:
        case Y12___U12___V12_N444:
        case Y12___U12___V12_N420:
        {
            UINT16 * out_ptr = (UINT16*)(out);
            out_ptr[0] = ((c0 & 0xFFF00000) >> 20);
            out_ptr[1] = ((c1 & 0xFFF00000) >> 20);
            out_ptr[2] = ((c2 & 0xFFF00000) >> 20);
            break;
        }
        default:
            // invalid or unsupported format specified
            Printf(Tee::PriHigh,"*** ERROR: Can not colwert to %s format\n",
                    FormatToString(to).c_str());
            MASSERT(0);
            break;
    }
}

// Write pixel data to a big-endian PNG buffer.  The input value varies
// from 0 to 0xffffffff.
//
static inline void WritePng08(void *pBuffer, UINT32 index, UINT32 value)
{
    ((UINT08*)pBuffer)[index] = value >> 24;
}

static inline void WritePng16(void *pBuffer, UINT32 index, UINT32 value)
{
    ((UINT08*)pBuffer)[2 * index]     = (UINT08)(value >> 24);
    ((UINT08*)pBuffer)[2 * index + 1] = (UINT08)(value >> 16);
}

static inline void WritePng32(void *pBuffer, UINT32 index, UINT32 value)
{
    ((UINT08*)pBuffer)[4 * index]     = (UINT08)(value >> 24);
    ((UINT08*)pBuffer)[4 * index + 1] = (UINT08)(value >> 16);
    ((UINT08*)pBuffer)[4 * index + 2] = (UINT08)(value >> 8);
    ((UINT08*)pBuffer)[4 * index + 3] = (UINT08)(value);
}

// Colwert a single pixel to PNG file storage color format.
// Integer color values are MSB-extended when colwerting
// to PNG formats with higher color depth.
void ColorUtils::Colwert(const char *in,
                         ColorUtils::Format from,
                         char *out,
                         ColorUtils::PNGFormat to)
{
    // pixel components
    UINT32 c0 = 0;
    UINT32 c1 = 0;
    UINT32 c2 = 0;
    UINT32 c3 = 0;

    // original precision of components
    int pc0 = 0;
    int pc1 = 0;
    int pc2 = 0;
    int pc3 = 0;

    // extract components of in
    switch (from)
    {
        case R5G6B5:
        case VOID16:
        {
            // set original precision
            pc0 = 5;
            pc1 = 6;
            pc2 = 5;
            pc3 = 32;
            // extract components
            UINT16 x = *((const UINT16*)(in));
            c0 = (UINT32)((x & 0xF800) >> (pc1 + pc2));
            c1 = (UINT32)((x & 0x07E0) >> (pc2));
            c2 = (UINT32)((x & 0x001F));
            c3 = 0xFFFFFFFF;
            break;
        }
        case A1R5G5B5:
        {
            // set original precision
            pc0 = 5;
            pc1 = 5;
            pc2 = 5;
            pc3 = 1;
            // extract components
            UINT16 x = *((const UINT16*)(in));
            c0 = (UINT32)((x & 0x7c00) >> (pc0 + pc1));
            c1 = (UINT32)((x & 0x03e0) >> (pc0));
            c2 = (UINT32)((x & 0x001F));
            c3 = (UINT32)((x & 0x8000) >> 15);
            break;
        }
        case X8R8G8B8:
        case A8R8G8B8:
        case R8G8B8A8:
        case B8G8R8A8:
        case CR8YB8CB8YA8:
        case YB8CR8YA8CB8:
        case Z24S8:
        case RF32:
        case A8B8G8R8:
        case Z32:
        case RU32:
        case Y8_U8__Y8_V8_N422:
        case U8_Y8__V8_Y8_N422:
        {
            // set original precision
            pc0 = 8;
            pc1 = 8;
            pc2 = 8;
            pc3 = 8;
            // extract components
            UINT32 x = *((const UINT32*)(in));
            c0 = (x & 0xFF000000) >> (pc1 + pc2 + pc3);
            c1 = (x & 0x00FF0000) >> (pc2 + pc3);
            c2 = (x & 0x0000FF00) >> (pc3);
            c3 = (x & 0x000000FF);
            break;
        }
        case X8Z24:
        {
            // set original precision
            pc0 = 8;
            pc1 = 8;
            pc2 = 8;
            pc3 = 32;
            // extract components
            UINT32 x = *((const UINT32*)(in));
            c0 = (x & 0x00FF0000) >> (pc1 + pc2);
            c1 = (x & 0x0000FF00) >> (pc2);
            c2 = (x & 0x000000FF);
            c3 = 0xFFFFFFFF;
            break;
        }
        case R10G10B10A2 :
        {
            // set original precision
            pc0 = 10;
            pc1 = 10;
            pc2 = 10;
            pc3 = 2;  // c3 contains alpha

            // c0 = R, c1 = G, c2 = B, c3 = A

            // extract components
            UINT32 x = *((const UINT32*)(in));
            c0 = (x & 0xFFC00000) >> (pc1 + pc2 + pc3);
            c1 = (x & 0x003FF000) >> (pc2 + pc3);
            c2 = (x & 0x00000FFC) >> (pc3);
            c3 = (x & 0x00000003);
            break;
        }
        case A2R10G10B10 :
        {
            // set original precision
            pc0 = 10;
            pc1 = 10;
            pc2 = 10;
            pc3 = 2;  // c3 contains alpha

            // c0 = R, c1 = G, c2 = B, c3 = A

            // extract components
            UINT32 x = *((const UINT32*)(in));
            c0 = (x & 0x3ff00000) >> (pc1+pc2);
            c1 = (x & 0xffc00) >> pc2;
            c2 = (x & 0x3ff);
            c3 = (x & 0xc0000000) >> (pc0 + pc1 + pc2);
            break;
        }
        case A2B10G10R10:
        case A2U10Y10V10:
        case X2BL10GL10RL10_XRBIAS:
        case X2BL10GL10RL10_XVYCC:
        {
            // set original precision
            pc0 = 10;
            pc1 = 10;
            pc2 = 10;
            pc3 = 2;  // c3 contains alpha

            // c0 = R, c1 = G, c2 = B, c3 = A

            // extract components
            UINT32 x = *((const UINT32*)(in));
            c2 = (x & 0x3ff00000) >> (pc1+pc2);
            c1 = (x & 0xffc00) >> pc2;
            c0 = (x & 0x3ff);
            c3 = (x & 0xc0000000) >> (pc0 + pc1 + pc2);
            break;
        }
        case Z1R5G5B5:
        {
            // set original precision
            pc0 = 1;
            pc1 = 5;
            pc2 = 5;
            pc3 = 5;
            // extract components
            UINT16 x = *((const UINT16*)(in));
            c0 = (x & 0x8000) >> (pc1 + pc2 + pc3);
            c1 = (x & 0x7C00) >> (pc2 + pc3);
            c2 = (x & 0x03E0) >> (pc3);
            c3 = (x & 0x001F);
            break;
        }
        case Z16:
        {
            // set original precision
            pc0 = 8;
            pc1 = 8;
            pc2 = 0;
            pc3 = 0;
            // extract components
            UINT16 x = *((const UINT16*)(in));
            c0 = (x & 0xFF00) >> (pc1);
            c1 = (x & 0x00FF);
            c2 = 0x00000000;
            c3 = 0x00000000;
            break;
        }
        case RF16:
        case R16:
        {
            // set original precision
            pc0 = 16;
            pc1 = 32;
            pc2 = 32;
            pc3 = 32;
            // extract components
            c0 = *((const UINT16*)(in));
            c1 = 0x00000000;
            c2 = 0x00000000;
            c3 = 0xFFFFFFFF;
            break;
        }
        case R16_G16:
        {
            // set original precision
            pc0 = 16;
            pc1 = 16;
            pc2 = 32;
            pc3 = 32;
            // extract components
            const UINT16 *x = (const UINT16*)(in);
            c0 = x[1];
            c1 = x[0];
            c2 = 0x00000000;
            c3 = 0xFFFFFFFF;
            break;
        }
        case RF16_GF16_BF16_AF16:
        {
            // set original precision
            pc0 = 16;
            pc1 = 16;
            pc2 = 16;
            pc3 = 16;
            // extract components
            const UINT16 *x = (const UINT16*)(in);
            c0 = x[0];
            c1 = x[1];
            c2 = x[2];
            c3 = x[3];
            break;
        }

        case R16_G16_B16_A16:
        case RS16_GS16_BS16_AS16:
        case RU16_GU16_BU16_AU16:
        {
            // set original precision
            pc0 = 16;
            pc1 = 16;
            pc2 = 16;
            pc3 = 16;
            const UINT16 *x = (const UINT16*)(in);
            c0 = x[0];  // c0 = R
            c1 = x[1];  // c1 = G
            c2 = x[2];  // c2 = B
            c3 = x[3];  // c3 = A
            break;
        }
        case Y8:
        case I8:
        case R8:
        case RN8:
        case RS8:
        case RU8:
        case A8:
        case S8:
        {
            // set original precision
            pc0 = 8;
            pc1 = 8;
            pc2 = 8;
            pc3 = 32;
            // extract components
            c0 = *((const UINT08*)(in));
            c1 = c0;
            c2 = c0;
            c3 = 0xFFFFFFFF;
            break;
        }
        case U8:
        case V8:
        {
            pc0 = 8;
            // Rest of componets are irrelavant
            // Set precision to 32 to avoid unnecessary bit operations
            pc1 = 32;
            pc2 = 32;
            pc3 = 32;
            // extract components
            c0 = *((const UINT08*)(in));
            c1 = 0xFFFFFFFF;
            c2 = 0xFFFFFFFF;
            c3 = 0xFFFFFFFF;
            break;
        }
        case B8_G8_R8:
        case Z24:
        case Y8___U8V8_N444:
        case Y8___U8V8_N422:
        case Y8___U8V8_N422R:
        case Y8___V8U8_N420:
        case Y8___U8___V8_N444:
        case Y8___U8___V8_N420:
        {
            // set original precision
            pc0 = 8;
            pc1 = 8;
            pc2 = 8;
            pc3 = 32;
            const UINT08 *x = (const UINT08*)(in);
            c0 = x[2];
            c1 = x[1];
            c2 = x[0];
            c3 = 0xFFFFFFFF;
            break;
        }
        case Y10___U10V10_N444:
        case Y10___U10V10_N422:
        case Y10___U10V10_N422R:
        case Y10___V10U10_N420:
        case Y10___U10___V10_N444:
        case Y10___U10___V10_N420:
        {
            pc0 = 10;
            pc1 = 10;
            pc2 = 10;
            pc3 = 32;
            // extract components
            UINT32 x = *((const UINT32*)(in));
            c2 = (x & 0x3ff00000) >> (pc1+pc2);
            c1 = (x & 0xffc00) >> pc2;
            c0 = (x & 0x3ff);
            c3 = 0xffffffff;
            break;
        }
        case Y12___U12V12_N444:
        case Y12___U12V12_N422:
        case Y12___U12V12_N422R:
        case Y12___V12U12_N420:
        case Y12___U12___V12_N444:
        case Y12___U12___V12_N420:
        {
            // set original precision
            pc0 = 12;
            pc1 = 12;
            pc2 = 12;
            pc3 = 32;
            // extract components
            const UINT16 *x = (const UINT16*)(in);
            c0 = (x[0] & 0xfff0) >> 4;
            c1 = (x[1] & 0xfff0) >> 4;
            c2 = (x[2] & 0xfff0) >> 4;
            c3 = 0xffffffff;
            break;
        }
        case U8V8:
        case V8U8:
        {
            pc0 = 8;
            pc1 = 8;
            pc2 = 32;
            pc3 = 32;
            const UINT08 *x = (const UINT08*)(in);
            c0 = x[1];
            c1 = x[0];
            c2 = 0;
            c3 = 0xffffffff;
            break;
        }
        case RF32_GF32_BF32_AF32:
        default:
            // invalid or unsupported format specified
            Printf(Tee::PriHigh,"*** ERROR: Can not colwert from %s format\n",
                    FormatToString(from).c_str());
            MASSERT(0);
            break;
    }

    switch (from)
    {
        case X8R8G8B8:
        {
            c0 = 0xFFFFFFFF; // Set alpha to opaque and intentionally fall
                             // through to the A8R8G8B8 case
        }
        case A8R8G8B8:
        {
            UINT32 temp = c0;
            c0 = c1; c1 = c2; c2 = c3;
            c3 = temp;
            break;
        }
        case B8G8R8A8:
        {
            swap(c0, c2);
            break;
        }
        case A8B8G8R8:
        {
            swap(c0, c3);
            swap(c1, c2);
            break;
        }
        case CR8YB8CB8YA8:
        {
            UINT32 temp;
            temp = c0; c0 = c1; c1 = temp;
            temp = c2; c2 = c3; c3 = temp;
            break;
        }
        case Z1R5G5B5:
        {
            UINT32 temp = c0;
            c0 = c1; c1 = c2; c2 = c3;
            c3 = temp;
            pc0 = 5;
            pc3 = 1;
            break;
        }
        case B8_G8_R8:
        {
            UINT32 temp = c0;
            c0 = c2;
            c2 = temp;
            break;
        }
        default:
            // components already in correct order
            break;
    }

    // msb-replicate each component out to 32-bits
    c0 = MSBReplicate(c0, pc0);
    c1 = MSBReplicate(c1, pc1);
    c2 = MSBReplicate(c2, pc2);
    c3 = MSBReplicate(c3, pc3);

    // place components into out
    // truncate bits as needed
    switch (to)
    {
        case C1:
        {
            WritePng08(out, 0, c0 & 0x80000000);
            break;
        }
        case C2:
        {
            WritePng08(out, 0, c0 & 0xC0000000);
            break;
        }
        case C4:
        {
            WritePng08(out, 0, c0 & 0xF0000000);
            break;
        }
        case C8:
        {
            WritePng08(out, 0, c0);
            break;
        }
        case C16:
        {
            WritePng16(out, 0, c0);
            break;
        }
        case C8C8:
        {
            WritePng08(out, 0, c0);
            WritePng08(out, 1, c1);
            break;
        }
        case C16C16:
        {
            WritePng16(out, 0, c0);
            WritePng16(out, 1, c1);
            break;
        }
        case C8C8C8:
        {
            WritePng08(out, 0, c0);
            WritePng08(out, 1, c1);
            WritePng08(out, 2, c2);
            break;
        }
        case C16C16C16:
        {
            WritePng16(out, 0, c0);
            WritePng16(out, 1, c1);
            WritePng16(out, 2, c2);
            break;
        }
        case C8C8C8C8:
        {
            WritePng08(out, 0, c0);
            WritePng08(out, 1, c1);
            WritePng08(out, 2, c2);
            WritePng08(out, 3, c3);
            break;
        }
        case C10C10C10C2:
        {
            WritePng32(out, 0, (((c0)       & 0xFFC00000) |
                                ((c1 >> 10) & 0x003FF000) |
                                ((c2 >> 20) & 0x00000FFC) |
                                ((c3 >> 30) & 0x00000003)));
            break;
        }
        case C10C10C10:
        {
            WritePng32(out, 0, (((c0)       & 0xFFC00000) |
                                ((c1 >> 10) & 0x003FF000) |
                                ((c2 >> 20) & 0x00000FFC)));
            break;
        }
        case C12C12C12:
        {
            WritePng32(out, 0, (((c0)       & 0xFFF0000) |
                                ((c1 >> 12) & 0x000FFF00) |
                                ((c2 >> 24) & 0x000000FF)));
            WritePng08(out, 4, (c2 & 0x0000000F));
            break;
        }
        case C16C16C16C16:
        {
            WritePng16(out, 0, c0);
            WritePng16(out, 1, c1);
            WritePng16(out, 2, c2);
            WritePng16(out, 3, c3);
            break;
        }
        case C32C32C32C32:
        {
            WritePng32(out, 0, c0);
            WritePng32(out, 1, c1);
            WritePng32(out, 2, c2);
            WritePng32(out, 3, c3);
            break;
        }
        default:
            // invalid or unsupported format specified
            MASSERT(0);
            break;
    }
}

// Colwert from one color format to another.
// Returns number of pixels written to DstBuf.
UINT32 ColorUtils::Colwert
(
    const char *         SrcBuf,
    ColorUtils::Format   SrcFmt,
    char *               DstBuf,
    ColorUtils::Format   DstFmt,
    UINT32               NumPixels
)
{
    MASSERT(SrcBuf);
    MASSERT(DstBuf);

    // Special easy case: no change.
    if (SrcFmt == DstFmt)
    {
        Platform::MemCopy(DstBuf, SrcBuf, NumPixels*ColorUtils::PixelBytes(SrcFmt));
        return NumPixels;
    }

    ColorUtils::PNGFormat pix_format = ColorUtils::C32C32C32C32;
    UINT32 pix_size = ColorUtils::PixelBytes(pix_format);
    UINT32 in_size = ColorUtils::PixelBytes(SrcFmt);

    UINT08* pix_buffer = new UINT08[NumPixels * ColorUtils::PixelBytes(pix_format)];

    for (UINT32 n = 0, i = 0, j = 0; n < NumPixels; ++n, i+= in_size, j+=pix_size)
    {
        ColorUtils::Colwert((const char*) &SrcBuf[i], SrcFmt,
                                  (char*) &pix_buffer[j], pix_format);
    }

    UINT32 out_size = ColorUtils::PixelBytes(DstFmt);

    for (UINT32 n = 0, i = 0, j = 0; n < NumPixels; ++n, i+= pix_size, j+=out_size)
    {
        ColorUtils::Colwert((const char*) &pix_buffer[i], pix_format,
                                  (char*) &DstBuf[j], DstFmt);
    }

    delete [] pix_buffer;

    return NumPixels;
}

// Colwert data from a PNG file storage color format.
// Returns the number of pixels written to DstBuf.
UINT32 ColorUtils::Colwert(const char *SrcBuf,
                           ColorUtils::PNGFormat SrcFmt,
                           char *DstBuf,
                           ColorUtils::Format DstFmt,
                           UINT32 NumPixels)
{
    UINT32 SrcFmtBytes = ColorUtils::PixelBytes(SrcFmt);
    UINT32 DstFmtBytes = ColorUtils::PixelBytes(DstFmt);
    for (UINT32 x = 0; x < NumPixels; x++)
    {
        ColorUtils::Colwert(SrcBuf, SrcFmt, DstBuf, DstFmt);
        SrcBuf += SrcFmtBytes;
        DstBuf += DstFmtBytes;
    }
    return NumPixels;
}

// Colwert data to a PNG file storage color format.
// Returns the number of pixels written to DstBuf.
UINT32 ColorUtils::Colwert(const char *SrcBuf,
                           ColorUtils::Format SrcFmt,
                           char *DstBuf,
                           ColorUtils::PNGFormat DstFmt,
                           UINT32 NumPixels)
{
    UINT32 SrcFmtBytes = ColorUtils::PixelBytes(SrcFmt);
    UINT32 DstFmtBytes = ColorUtils::PixelBytes(DstFmt);
    for (UINT32 x = 0; x < NumPixels; x++)
    {
        ColorUtils::Colwert(SrcBuf, SrcFmt, DstBuf, DstFmt);
        SrcBuf += SrcFmtBytes;
        DstBuf += DstFmtBytes;
    }
    return NumPixels;
}

//
// ColorUtils::YCrCbColwert
//
// Colwert pixels to YCrCb formats
//

/*

"601" equations

Yt = 0.299 * R + 0.587 * G + 0.114 * B
Cb = 0.5643 * (B - Yt)
Cr = 0.7133 * (R - Yt)

"709" equations

Yt = 0.2126 * R + 0.7152 * G + 0.0722 * B
Cb = 0.5389 * (B - Yt)
Cr = 0.6350 * (R - Yt)

Where :

R, G and B are the input color components normalised to a 0.00 to 1.00 range.
R = Rin / 255.0;
G = Gin / 255.0;
B = Bin / 255.0;

Y, Cb and Cr are the new color components expressed as floating point values.

To colwert these to 8-bit values in the framestore, you have to apply the following adjustment :

Yout    = (Yt * 219) + 16
Cb_out = (Cb * 224) + 128
Cr_out  = (Cr * 224) + 128

*/

UINT32 ColorUtils::YCrCbColwert( const char* SrcBuf,
                                 Format      SrcFmt,
                                 char*       DstBuf,
                                 Format      DstFmt,
                                 UINT32      NumPixels,
                                 YCrCbType   ycrcb_type)
{
    const UINT32 small_buf_pixels = 2;
    UINT32 small_buf[small_buf_pixels];
    unique_ptr<UINT32> big_buf;
    UINT32 *buf;
    UINT32 *dst_buf = (UINT32*)DstBuf;

    if (NumPixels > small_buf_pixels)
    {
        buf = new UINT32[NumPixels];
        unique_ptr<UINT32> tmp(buf);
        big_buf = std::move(tmp); // auto-delete on exit
    }
    else
    {
        buf = small_buf;
    }

    Colwert(SrcBuf, SrcFmt, (char*)buf, A8R8G8B8, NumPixels);

    if (NumPixels % 2)
    {
        // must be even number of pixels for 422 formats
        Printf(Tee::PriHigh,"*** ERROR: YCrCbColwert: must be an even number of pixels\n");
        MASSERT(0);
    }

    INT32 R_e, G_e, B_e;
    INT32 R_o, G_o, B_o;
    INT32 Yt_e, Cb_e, Cr_e;
    INT32 Yt_o;
    INT32 yout_e, yout_o, cb_out, cr_out;

    for (unsigned int i = 0, j=0; i < NumPixels; i+=2, j++ )
    {
        R_e = ((buf[i] & 0xff0000) >> 16);
        G_e = ((buf[i] & 0xff00) >> 8 );
        B_e = (buf[i] & 0xff);

        R_o = ((buf[i+1] & 0xff0000) >> 16);
        G_o = ((buf[i+1] & 0xff00) >> 8 );
        B_o = (buf[i+1] & 0xff);

        if (ycrcb_type == CCIR601)
        {
            Yt_e = (2990 * R_e + 5870 * G_e + 1140 * B_e) / 10000;
            Cb_e = (5643 * (B_e - Yt_e)) / 10000;
            Cr_e = (7133 * (R_e - Yt_e)) / 10000;
            Yt_o = (2990 * R_o + 5870 * G_o + 1140 * B_o) / 10000;
        }
        else
        {
            Yt_e = (2126 * R_e + 7152 * G_e + 722 * B_e) / 10000;
            Cb_e = (5389 * (B_e - Yt_e)) / 10000;
            Cr_e = (6350 * (R_e - Yt_e)) / 10000;
            Yt_o = (2126 * R_o + 7152 * G_o + 722 * B_o) / 10000;
        }

        UINT32 c0, c1, c2, c3;

        yout_e    = ( (Yt_e * 219) / (255) ) + 16 ;
        yout_o    = ( (Yt_o * 219) / (255) ) + 16;
        cb_out    = ( (Cb_e * 224) / (255) ) + 128;
        cr_out    = ( (Cr_e * 224) / (255) ) + 128;

        if (DstFmt == VE8YO8UE8YE8)
        {
            c0 = (UINT32)cr_out;
            c1 = (UINT32)yout_o;
            c2 = (UINT32)cb_out;
            c3 = (UINT32)yout_e;
        }
        else if (DstFmt == YO8VE8YE8UE8)
        {
            c0 = (UINT32)yout_o;
            c1 = (UINT32)cr_out;
            c2 = (UINT32)yout_e;
            c3 = (UINT32)cb_out;
        }
        else
        {
            Printf(Tee::PriHigh,"*** ERROR: YCrCbColwert(): invalid destination format %s\n",
                   FormatToString(DstFmt).c_str());

            // set these to 0 to avoid unitialized variable warnings
            c0 = c1 = c2 = c3 = 0;
        }

        dst_buf[j] = (c0 << 24 |
                      c1 << 16 |
                      c2 << 8 |
                      c3);
    }

    return NumPixels;
}

ColorUtils::YCrCbType ColorUtils::StringToCrCbType(const string& str)
{
    if ( str == "CCIR601" ) {
        return CCIR601;
    } else if ( str == "CCIR709" ) {
        return CCIR709;
    } else {
        Printf(Tee::PriHigh, "*** ERROR: %s YCrCb type is not recognized\n", str.c_str());
        return YCRCBTYPE_NONE;
    }
}

UINT32 ColorUtils::ColwertYUV422ToYUV444(UINT08* inbuf, UINT08* outbuf,
                                        UINT32 NumInPixels, ColorUtils::Format Format)
{
    UINT32 Size = NumInPixels;
    UINT32* yuv422 = (UINT32*)inbuf;
    UINT32* yuv444 = (UINT32*)outbuf;
    UINT32 ye8, ue8, ve8, yo8, uo8, vo8;
    UINT32 argb_e, argb_o;

    for (UINT32 i = 0, j = 0; j < Size; i++, j+=2) {
        if (Format == ColorUtils::VE8YO8UE8YE8) {
            /* VE8YO8UE8YE8 */
            vo8 = ve8 = (yuv422[i] >> 24) & 0xff;
            yo8 = (yuv422[i] >> 16) & 0xff;
            uo8 = ue8 = (yuv422[i] >> 8 ) & 0xff;
            ye8 = (yuv422[i] & 0xff);
        } else {
            /* YO8VE8YE8UE8 */
            yo8 = (yuv422[i] >> 24) & 0xff;
            vo8 = ve8 = (yuv422[i] >> 16) & 0xff;
            ye8 = (yuv422[i] >> 8 ) & 0xff;
            uo8 = ue8 = (yuv422[i] & 0xff);
        }

        argb_e =  ( 0xff000000 |
                    ve8 << 16 |
                    ye8 << 8 |
                    ue8  );
        argb_o = ( 0xff000000 |
                    vo8 << 16 |
                    yo8 << 8 |
                    uo8  );

        yuv444[j]   = argb_e;
        yuv444[j+1] = argb_o;
    }
    return Size;
}

UINT32 ColorUtils::ColwertYUV444ToYUV422(UINT08* inbuf, UINT08* outbuf,
                                         UINT32 NumInPixels, ColorUtils::Format Format)
{
    UINT32 Size = NumInPixels;
    UINT32* yuv422 = (UINT32*)outbuf;
    UINT32* yuv444 = (UINT32*)inbuf;
    UINT32 ye8, ue8, ve8, yo8;

    for (UINT32 i = 0, j = 0; j < Size; i++, j+=2) {
        /* VE8YO8UE8YE8 */
        ve8 = (yuv444[j] >> 16) & 0xff;
        ye8 = (yuv444[j] >> 8 ) & 0xff;
        ue8 = (yuv444[j] & 0xff);
        yo8 = (yuv444[j+1] >> 8) & 0xff;

        if (Format == ColorUtils::VE8YO8UE8YE8) {
            yuv422[i] =  ( ve8 << 24 |
                           yo8 << 16 |
                           ue8 << 8 |
                           ye8  );
        } else {
            /* YO8VE8YE8UE8 */
            yuv422[i] =  ( yo8 << 24 |
                           ve8 << 16 |
                           ye8 << 8 |
                           ue8  );
        }
    }
    return Size;
}

UINT64 ColorUtils::EncodePixel (Format pixelFormat,
                                UINT32 redCr,
                                UINT32 greenYo,
                                UINT32 blueCb,
                                UINT32 alphaYe)
{
    UINT64 Out = 0;

    switch (pixelFormat)
    {
        case I8:
        {
            Out = redCr & 0xFF;
            break;
        }
        case VOID16:
        case R5G6B5:
        {
            Out = ((redCr   & 0x1F) << 11) |
                  ((greenYo & 0x3F) <<  5) |
                  ((blueCb  & 0x1F) <<  0);
            break;
        }
        case A1R5G5B5:
        {
            Out = ((alphaYe &  0x1) << 15) |
                  ((redCr   & 0x1F) << 10) |
                  ((greenYo & 0x1F) <<  5) |
                  ((blueCb  & 0x1F) <<  0);
            break;
        }
        case A8R8G8B8:
        {
            Out = ((alphaYe & 0xFF) << 24) |
                  ((redCr   & 0xFF) << 16) |
                  ((greenYo & 0xFF) <<  8) |
                  ((blueCb  & 0xFF) <<  0);
            break;
        }
        case A8B8G8R8:
        {
            Out = ((alphaYe & 0xFF) << 24) |
                  ((redCr   & 0xFF) <<  0) |
                  ((greenYo & 0xFF) <<  8) |
                  ((blueCb  & 0xFF) << 16);
            break;
        }
        case X8R8G8B8:
        {
            Out = ((redCr   & 0xFF) << 16) |
                  ((greenYo & 0xFF) << 8) |
                  ((blueCb  & 0xFF));
            break;
        }
        case X8B8G8R8:
        {
            Out = ((blueCb   & 0xFF) << 16) |
                  ((greenYo & 0xFF) << 8) |
                  ((redCr  & 0xFF));
            break;
        }
        case A2R10G10B10:
        {
            Out = ((alphaYe & 0x003) << 30) |
                  ((redCr   & 0x3FF) << 20) |
                  ((greenYo & 0x3FF) << 10) |
                  ((blueCb  & 0x3FF) <<  0);
            break;
        }
        case A2B10G10R10:
        case X2BL10GL10RL10_XRBIAS:
        case X2BL10GL10RL10_XVYCC:
        {
            Out = ((alphaYe & 0x003) << 30) |
                  ((redCr   & 0x3FF) <<  0) |
                  ((greenYo & 0x3FF) << 10) |
                  ((blueCb  & 0x3FF) << 20);
            break;
        }
        case VE8YO8UE8YE8:
        {
            Out = ((redCr   & 0xFF) << 24) |
                  ((greenYo & 0xFF) << 16) |
                  ((blueCb  & 0xFF) <<  8) |
                  ((alphaYe & 0xFF) <<  0);
            break;
        }
        case YO8VE8YE8UE8:
        {
            Out = ((greenYo & 0xFF) << 24) |
                  ((redCr   & 0xFF) << 16) |
                  ((alphaYe & 0xFF) <<  8) |
                  ((blueCb  & 0xFF) <<  0);
            break;
        }
        case R16_G16_B16_A16:
        case R16_G16_B16_A16_LWBIAS:
        case RF16_GF16_BF16_AF16:
        {
            Out = (((UINT64) alphaYe & 0xFFFF) << 48) |
                  (((UINT64) redCr   & 0xFFFF) <<  0) |
                  (((UINT64) greenYo & 0xFFFF) << 16) |
                  (((UINT64) blueCb  & 0xFFFF) << 32);
            break;
        }
        default:
        {
            // invalid or unsupported format specified
            Printf(Tee::PriHigh,"*** ERROR: Can not encode to %s format\n",
                    FormatToString(pixelFormat).c_str());
            MASSERT(0);
            break;
        }
    }

    return (Out);
}

void ColorUtils::CompBitsRGBA (Format fmt,
                               UINT32 *pRedCompBits,
                               UINT32 *pGreenCompBits,
                               UINT32 *pBlueCompBits,
                               UINT32 *pAlphaCompBits)
{
    UINT32 redCompBits, greenCompBits, blueCompBits, alphaCompBits;

    redCompBits = 0;
    greenCompBits = 0;
    blueCompBits = 0;
    alphaCompBits = 0;

    switch (fmt)
    {
        case I8:
        {
            redCompBits = 8;
            greenCompBits = 0;
            blueCompBits = 0;
            alphaCompBits = 0;
            break;
        }
        case R4G4B4A4:
        {
            redCompBits = 4;
            greenCompBits = 4;
            blueCompBits = 4;
            alphaCompBits = 4;
            break;
        }
        case VOID16:
        case R5G6B5:
        {
            redCompBits = 5;
            greenCompBits = 6;
            blueCompBits = 5;
            alphaCompBits = 0;
            break;
        }
        case A1R5G5B5:
        case R5G5B5A1:
        {
            redCompBits = 5;
            greenCompBits = 5;
            blueCompBits = 5;
            alphaCompBits = 1;
            break;
        }
        case A8R8G8B8:
        case A8B8G8R8:
        {
            redCompBits = 8;
            greenCompBits = 8;
            blueCompBits = 8;
            alphaCompBits = 8;
            break;
        }
        case X8R8G8B8:
        case X8B8G8R8:
        {
            redCompBits = 8;
            greenCompBits = 8;
            blueCompBits = 8;
            alphaCompBits = 0;
            break;
        }
        case A2B10G10R10:
        case A2R10G10B10:
        case X2BL10GL10RL10_XRBIAS:
        case X2BL10GL10RL10_XVYCC:
        {
            redCompBits = 10;
            greenCompBits = 10;
            blueCompBits = 10;
            alphaCompBits = 2;
            break;
        }
        case R16_G16_B16_A16:
        case R16_G16_B16_A16_LWBIAS:
        case RF16_GF16_BF16_AF16:
        {
            redCompBits = 16;
            greenCompBits = 16;
            blueCompBits = 16;
            alphaCompBits = 16;
            break;
        }
        default:
        {
            // invalid or unsupported format specified
            Printf(Tee::PriError, "Cannot get component bits for %s format\n",
                    FormatToString(fmt).c_str());
            MASSERT(0);
            break;
        }
    }

    if (pRedCompBits) *pRedCompBits = redCompBits;
    if (pGreenCompBits) *pGreenCompBits = greenCompBits;
    if (pBlueCompBits) *pBlueCompBits = blueCompBits;
    if (pAlphaCompBits) *pAlphaCompBits = alphaCompBits;
}

namespace
{
struct Component
{
    INT32 CrOrR = 0;
    INT32 YOrG = 0;
    INT32 CbOrB = 0;
};

using namespace ColorUtils;

RC GetRgbComponentsByBits
(
    const char *srcBuf,
    UINT32 bpc,
    Format inputFmt,
    Component *pOutRgb
)
{
    switch (bpc)
    {
        case 8:
        {
            UINT32 buf[2];
            ColorUtils::Colwert(srcBuf, inputFmt, (char*)buf, ColorUtils::A8R8G8B8, 2);
            pOutRgb->CrOrR = ((*buf & 0xff0000) >> 16);
            pOutRgb->YOrG = ((*buf & 0xff00) >> 8);
            pOutRgb->CbOrB = (*buf & 0xff);
            break;
        }
        case 10:
        {
            UINT32 buf[2];
            ColorUtils::Colwert(srcBuf, inputFmt, (char*)buf, ColorUtils::A2R10G10B10, 2);
            pOutRgb->CrOrR = ((*buf & 0x3ff00000) >> 20);
            pOutRgb->YOrG = ((*buf & 0xffc00) >> 10);
            pOutRgb->CbOrB = (*buf & 0x3ff);
            break;
        }
        case 12:
        {
            UINT64 buf[2];
            ColorUtils::Colwert(srcBuf, inputFmt, (char*)buf, ColorUtils::R16_G16_B16_A16, 2);
            // Fetch 16 bits R/G/B components
            pOutRgb->CrOrR = (*buf & 0xffff);
            pOutRgb->YOrG = ((*buf & 0xffff0000) >> 16);
            pOutRgb->CbOrB = ((*buf & 0xffff00000000) >> 32);

            // Scale it down to 12 bits
            pOutRgb->CrOrR = (pOutRgb->CrOrR * 4095 / 65535);
            pOutRgb->YOrG = (pOutRgb->YOrG * 4095 / 65535);
            pOutRgb->CbOrB = (pOutRgb->CbOrB * 4095 / 65535);
            break;
        }
        default:
            Printf(Tee::PriError, "Unsupported bpc format\n");
            return RC::UNSUPPORTED_FUNCTION;
    }

    return RC::OK;
}

// Colwersion used by LwDisplay class C3
RC ApplyYuvColwersionC3
(
    YCrCbType type,
    const Component& inRgb,
    Component *pOutYuv
)
{
    // Implement me
    return RC::OK;
}

// Colwersion used by LwDisplay class C5
RC ApplyYuvColwersionC5
(
    YCrCbType type,
    const Component& inRgb,
    Component *pOutYuv
)
{
    // Ilwerse of colwersion matrix based on Section 5.5.2.1.4 of
    // https://p4viewer.lwpu.com/get///hw/doc/mmplex/display/4.0/specifications/LWDisplay_PreComp_Pipe_IAS.docx
    if (type == CCIR601)
    {
        pOutYuv->YOrG = (2990 * inRgb.CrOrR + 5870 * inRgb.YOrG + 1140 * inRgb.CbOrB) / 10000;
        pOutYuv->CbOrB = (5643 * (inRgb.CbOrB - pOutYuv->YOrG)) / 10000;
        pOutYuv->CrOrR = (7133 * (inRgb.CrOrR - pOutYuv->YOrG)) / 10000;
    }
    else if (type == CCIR709)
    {
        pOutYuv->YOrG = (2126 * inRgb.CrOrR + 7152 * inRgb.YOrG + 722 * inRgb.CbOrB) / 10000;
        pOutYuv->CbOrB = (5389 * (inRgb.CbOrB - pOutYuv->YOrG)) / 10000;
        pOutYuv->CrOrR = (6350 * (inRgb.CrOrR - pOutYuv->YOrG)) / 10000;
    }
    else if (type == CCIR2020)
    {
        pOutYuv->YOrG = (2627 * inRgb.CrOrR + 6780 * inRgb.YOrG + 593 * inRgb.CbOrB) / 10000;
        pOutYuv->CbOrB = (5315 * (inRgb.CbOrB - pOutYuv->YOrG)) / 10000;
        pOutYuv->CrOrR = (6782 * (inRgb.CrOrR - pOutYuv->YOrG)) / 10000;
    }
    else
    {
        MASSERT(!"Invalid YUV color space");
        Printf(Tee::PriError, "Invalid YUV color space type\n");
        return RC::SOFTWARE_ERROR;
    }
    return RC::OK;
}

RC ApplyYuvColwersion
(
    YCrCbType type,
    const Component& inRgb,
    Component *pOutYuv,
    UINT32 classCode
)
{
    RC rc;

    if (classCode >= 0xC570)
    {
        CHECK_RC(ApplyYuvColwersionC5(type, inRgb, pOutYuv));
    }
    else if (classCode >= 0xC370)
    {
        CHECK_RC(ApplyYuvColwersionC3(type, inRgb, pOutYuv));
    }
    else
    {
        MASSERT(!"Display class not supported");
        Printf(Tee::PriError, "%s : Display class not supported\n", MODS_FUNCTION);
        return RC::UNSUPPORTED_FUNCTION;
    }

    return rc;
}

RC EncodeYuvBits(Component *pInOutYuv, UINT32 bpc)
{
    switch (bpc)
    {
        case 8:
            pInOutYuv->YOrG  = ((pInOutYuv->YOrG  * 219) / 255) + 16;
            pInOutYuv->CbOrB = ((pInOutYuv->CbOrB * 224) / 255) + 128;
            pInOutYuv->CrOrR = ((pInOutYuv->CrOrR * 224) / 255) + 128;
            break;
        case 10:
            pInOutYuv->YOrG  = ((pInOutYuv->YOrG  * 876) / 1023) + 64;
            pInOutYuv->CbOrB = ((pInOutYuv->CbOrB * 896) / 1023) + 512;
            pInOutYuv->CrOrR = ((pInOutYuv->CrOrR * 896) / 1023) + 512;
            break;
        case 12:
            pInOutYuv->YOrG  = ((pInOutYuv->YOrG  * 3504) / 4095) + 256;
            pInOutYuv->CbOrB = ((pInOutYuv->CbOrB * 3584) / 4095) + 2048;
            pInOutYuv->CrOrR = ((pInOutYuv->CrOrR * 3584) / 4095) + 2048;
            break;
        default:
            Printf(Tee::PriError, "Unsupported bpc format\n");
            return RC::UNSUPPORTED_FUNCTION;
    }

    return RC::OK;
}

RC GetPixelByFormat
(
    const Component& inYuv,
    Format fmt,
    UINT32 bpc,
    UINT32 *pPixel
)
{
    switch (bpc)
    {
        case 8:
        {
            UINT32 byte0 = 0;
            UINT32 byte1 = 0;
            UINT32 byte2 = 0;
            UINT32 byte3 = 0;

            if (fmt == Y8)
            {
                byte0 = static_cast<UINT32>(inYuv.YOrG);
            }
            else if (fmt == U8)
            {
                byte0 = static_cast<UINT32>(inYuv.CbOrB);
            }
            else if (fmt == V8)
            {
                byte0 = static_cast<UINT32>(inYuv.CrOrR);
            }
            else if (fmt == U8V8)
            {
                byte0 = static_cast<UINT32>(inYuv.CbOrB);
                byte1 = static_cast<UINT32>(inYuv.CrOrR);
            }
            else if (fmt == ColorUtils::Y8_U8__Y8_V8_N422)
            {
                byte0 = static_cast<UINT32>(inYuv.YOrG);
                byte1 = static_cast<UINT32>(inYuv.CbOrB);
                byte2 = static_cast<UINT32>(inYuv.YOrG);
                byte3 = static_cast<UINT32>(inYuv.CrOrR);
            }
            else if (fmt == ColorUtils::U8_Y8__V8_Y8_N422)
            {
                byte0 = static_cast<UINT32>(inYuv.CbOrB);
                byte1 = static_cast<UINT32>(inYuv.YOrG);
                byte2 = static_cast<UINT32>(inYuv.CrOrR);
                byte3 = static_cast<UINT32>(inYuv.YOrG);
            }
            else
            {
                Printf(Tee::PriError, "Unsupported color format\n");
                return RC::UNSUPPORTED_COLORFORMAT;
            }

            *pPixel = (byte3 << 24 | byte2 << 16 | byte1 << 8 | byte0);
            break;
        }
        case 10:
        {
            UINT32 word0 = 0;
            UINT32 word1 = 0;
            if (fmt == R16)
            {
                // Color is stored in most significant 10 bits i.e, bits 15:6 in 16 bits storage
                word0 = static_cast<UINT32>(inYuv.YOrG) << 6;
            }
            else if (fmt == R16_G16)
            {
                word0 = static_cast<UINT32>(inYuv.CbOrB) << 6;
                word1 = static_cast<UINT32>(inYuv.CrOrR) << 6;
            }
            else
            {
                Printf(Tee::PriError, "Unsupported color format\n");
                return RC::UNSUPPORTED_COLORFORMAT;
            }
            *pPixel = (word1 << 16 | word0);
            break;
        }
        case 12:
        {
            UINT32 word0 = 0;
            UINT32 word1 = 0;

            if (fmt == R16)
            {
                // Color is stored in most significant 12 bits i.e, bits 15:4 in 16 bits storage
                word0 = static_cast<UINT32>(inYuv.YOrG) << 4;
            }
            else if (fmt == R16_G16)
            {
                word0 = static_cast<UINT32>(inYuv.CbOrB) << 4;
                word1 = static_cast<UINT32>(inYuv.CrOrR) << 4;
            }
            else
            {
                Printf(Tee::PriError, "Unsupported color format\n");
                return RC::UNSUPPORTED_COLORFORMAT;
            }

            *pPixel = (word1 << 16 | word0);
            break;
        }
        default:
            MASSERT(!"Unsupported bpc format");
            Printf(Tee::PriError, "Unsupported bpc format\n");
            return RC::UNSUPPORTED_FUNCTION;
    }

    return RC::OK;
}

RC ColwertRgbToYuv
(
    const char *srcBuf,
    char *dstBuf,
    UINT32 numPixels,
    Format inputFmt,
    Format outputFmt,
    YCrCbType outputCS,
    Format planeFormat,
    UINT32 classCode
)
{
    RC rc;
    Component inRgb;
    Component outYuv;
    UINT32 *pixel = reinterpret_cast<UINT32*>(dstBuf);
    // Check if pointer is 4-byte aligned
    MASSERT((reinterpret_cast<uintptr_t>(pixel) % 4) == 0);

    if (numPixels > 1)
    {
        // TODO: Update for multiple pixels
        MASSERT(numPixels > 1);
        return RC::SOFTWARE_ERROR;
    }

    UINT32 bpc = (planeFormat != ColorUtils::LWFMT_NONE) ?
        GetYuvBpc(planeFormat) : GetYuvBpc(outputFmt);

    for (UINT32 idx = 0; idx < numPixels; ++idx)
    {
        CHECK_RC(GetRgbComponentsByBits(srcBuf, bpc, inputFmt, &inRgb));

        CHECK_RC(ApplyYuvColwersion(outputCS, inRgb, &outYuv, classCode));

        CHECK_RC(EncodeYuvBits(&outYuv, bpc));

        CHECK_RC(GetPixelByFormat(outYuv, outputFmt, bpc, pixel));
    }

    return rc;
}
} // anonymous namespace

RC ColorUtils::TransformPixels
(
    const char *srcBuf,
    char *dstBuf,
    UINT32 numPixels,
    Format inputFmt,
    Format outputFmt,
    YCrCbType outputCS,
    Format planeFormat,
    UINT32 classCode
)
{
    RC rc;
    if (ColorUtils::IsRgb(inputFmt) && outputCS != YCrCbType::YCRCBTYPE_NONE)
    {
        CHECK_RC(ColwertRgbToYuv(srcBuf, dstBuf, numPixels,
            inputFmt, outputFmt, outputCS, planeFormat, classCode));
    }
    else
    {
        Printf(Tee::PriError, "%s to %s colwersion is not supported\n",
            FormatToString(inputFmt).c_str(), FormatToString(outputFmt).c_str());
        return RC::UNSUPPORTED_FUNCTION;
    }
    return rc;
}

//-----------------------------------------------------------------------------
// JavaScript interface
JS_CLASS(Color);
static SObject Color_Object
(
    "Color",
    ColorClass,
    0,
    0,
    "Color formats."
);

// Constants
#define COLOR_FORMAT(x) \
    static SProperty Color_##x \
    ( \
        Color_Object, \
        #x, \
        0, \
        ColorUtils::x, \
        0, \
        0, \
        JSPROP_READONLY, \
        #x " color format" \
    )

COLOR_FORMAT(R5G6B5);
COLOR_FORMAT(A8R8G8B8);
COLOR_FORMAT(R8G8B8A8);
COLOR_FORMAT(A2R10G10B10);
COLOR_FORMAT(R10G10B10A2);
COLOR_FORMAT(CR8YB8CB8YA8);
COLOR_FORMAT(YB8CR8YA8CB8);
COLOR_FORMAT(Z1R5G5B5);
COLOR_FORMAT(Z24S8);
COLOR_FORMAT(Z16);
COLOR_FORMAT(RF16);
COLOR_FORMAT(RF32);
COLOR_FORMAT(RF16_GF16_BF16_AF16);
COLOR_FORMAT(RF32_GF32_BF32_AF32);
COLOR_FORMAT(Y8);
COLOR_FORMAT(B8_G8_R8);
COLOR_FORMAT(Z24);
COLOR_FORMAT(I8);
COLOR_FORMAT(VOID8);
COLOR_FORMAT(VOID16);
COLOR_FORMAT(A2V10Y10U10);
COLOR_FORMAT(A2U10Y10V10);
COLOR_FORMAT(VE8YO8UE8YE8);
COLOR_FORMAT(UE8YO8VE8YE8);
COLOR_FORMAT(YO8VE8YE8UE8);
COLOR_FORMAT(YO8UE8YE8VE8);
COLOR_FORMAT(YE16_UE16_YO16_VE16);
COLOR_FORMAT(YE10Z6_UE10Z6_YO10Z6_VE10Z6);
COLOR_FORMAT(UE16_YE16_VE16_YO16);
COLOR_FORMAT(UE10Z6_YE10Z6_VE10Z6_YO10Z6);
COLOR_FORMAT(VOID32);
COLOR_FORMAT(CPST8);
COLOR_FORMAT(CPSTY8CPSTC8);
COLOR_FORMAT(AUDIOL16_AUDIOR16);
COLOR_FORMAT(AUDIOL32_AUDIOR32);
COLOR_FORMAT(A2B10G10R10);
COLOR_FORMAT(A8B8G8R8);
COLOR_FORMAT(A1R5G5B5);
COLOR_FORMAT(Z8R8G8B8);
COLOR_FORMAT(Z32);
COLOR_FORMAT(X8R8G8B8);
COLOR_FORMAT(X1R5G5B5);
COLOR_FORMAT(AN8BN8GN8RN8);
COLOR_FORMAT(AS8BS8GS8RS8);
COLOR_FORMAT(AU8BU8GU8RU8);
COLOR_FORMAT(X8B8G8R8);
COLOR_FORMAT(A8RL8GL8BL8);
COLOR_FORMAT(X8RL8GL8BL8);
COLOR_FORMAT(A8BL8GL8RL8);
COLOR_FORMAT(X8BL8GL8RL8);
COLOR_FORMAT(RF32_GF32_BF32_X32);
COLOR_FORMAT(RS32_GS32_BS32_AS32);
COLOR_FORMAT(RS32_GS32_BS32_X32);
COLOR_FORMAT(RU32_GU32_BU32_AU32);
COLOR_FORMAT(RU32_GU32_BU32_X32);
COLOR_FORMAT(R16_G16_B16_A16);
COLOR_FORMAT(RN16_GN16_BN16_AN16);
COLOR_FORMAT(RU16_GU16_BU16_AU16);
COLOR_FORMAT(RS16_GS16_BS16_AS16);
COLOR_FORMAT(RF16_GF16_BF16_X16);
COLOR_FORMAT(RF32_GF32);
COLOR_FORMAT(RS32_GS32);
COLOR_FORMAT(RU32_GU32);
COLOR_FORMAT(RS32);
COLOR_FORMAT(RU32);
COLOR_FORMAT(AU2BU10GU10RU10);
COLOR_FORMAT(RF16_GF16);
COLOR_FORMAT(RS16_GS16);
COLOR_FORMAT(RN16_GN16);
COLOR_FORMAT(RU16_GU16);
COLOR_FORMAT(R16_G16);
COLOR_FORMAT(BF10GF11RF11);
COLOR_FORMAT(G8R8);
COLOR_FORMAT(GN8RN8);
COLOR_FORMAT(GS8RS8);
COLOR_FORMAT(GU8RU8);
COLOR_FORMAT(R16);
COLOR_FORMAT(RN16);
COLOR_FORMAT(RS16);
COLOR_FORMAT(RU16);
COLOR_FORMAT(R8);
COLOR_FORMAT(RN8);
COLOR_FORMAT(RS8);
COLOR_FORMAT(RU8);
COLOR_FORMAT(A8);
COLOR_FORMAT(ZF32);
COLOR_FORMAT(S8Z24);
COLOR_FORMAT(X8Z24);
COLOR_FORMAT(V8Z24);
COLOR_FORMAT(ZF32_X24S8);
COLOR_FORMAT(X8Z24_X16V8S8);
COLOR_FORMAT(ZF32_X16V8X8);
COLOR_FORMAT(ZF32_X16V8S8);
COLOR_FORMAT(S8);
COLOR_FORMAT(X2BL10GL10RL10_XRBIAS);
COLOR_FORMAT(R16_G16_B16_A16_LWBIAS);
COLOR_FORMAT(X2BL10GL10RL10_XVYCC);

// CheetAh scanout formats
COLOR_FORMAT(A8Y8U8V8);
COLOR_FORMAT(I1);
COLOR_FORMAT(I2);
COLOR_FORMAT(I4);
COLOR_FORMAT(A4R4G4B4);
COLOR_FORMAT(R4G4B4A4);
COLOR_FORMAT(B4G4R4A4);
COLOR_FORMAT(R5G5B5A1);
COLOR_FORMAT(B5G5R5A1);
COLOR_FORMAT(A8R6x2G6x2B6x2);
COLOR_FORMAT(A8B6x2G6x2R6x2);
COLOR_FORMAT(X1B5G5R5);
COLOR_FORMAT(B5G5R5X1);
COLOR_FORMAT(R5G5B5X1);
COLOR_FORMAT(U8);
COLOR_FORMAT(V8);
COLOR_FORMAT(CR8);
COLOR_FORMAT(CB8);
COLOR_FORMAT(U8V8);
COLOR_FORMAT(V8U8);
COLOR_FORMAT(CR8CB8);
COLOR_FORMAT(CB8CR8);

// YUV color formats
COLOR_FORMAT(Y8_U8__Y8_V8_N422);
COLOR_FORMAT(U8_Y8__V8_Y8_N422);
COLOR_FORMAT(Y8___U8V8_N444);
COLOR_FORMAT(Y8___U8V8_N422);
COLOR_FORMAT(Y8___U8V8_N422R);
COLOR_FORMAT(Y8___V8U8_N420);
COLOR_FORMAT(Y8___U8___V8_N444);
COLOR_FORMAT(Y8___U8___V8_N420);
COLOR_FORMAT(Y10___U10V10_N444);
COLOR_FORMAT(Y10___U10V10_N422);
COLOR_FORMAT(Y10___U10V10_N422R);
COLOR_FORMAT(Y10___V10U10_N420);
COLOR_FORMAT(Y10___U10___V10_N444);
COLOR_FORMAT(Y10___U10___V10_N420);
COLOR_FORMAT(Y12___U12V12_N444);
COLOR_FORMAT(Y12___U12V12_N422);
COLOR_FORMAT(Y12___U12V12_N422R);
COLOR_FORMAT(Y12___V12U12_N420);
COLOR_FORMAT(Y12___U12___V12_N444);
COLOR_FORMAT(Y12___U12___V12_N420);

// Texture specific formats
COLOR_FORMAT(R32_G32_B32_A32);
COLOR_FORMAT(R32_G32_B32);
COLOR_FORMAT(R32_G32);
COLOR_FORMAT(R32_B24G8);
COLOR_FORMAT(G8R24);
COLOR_FORMAT(G24R8);
COLOR_FORMAT(R32);
COLOR_FORMAT(A4B4G4R4);
COLOR_FORMAT(A5B5G5R1);
COLOR_FORMAT(A1B5G5R5);
COLOR_FORMAT(B5G6R5);
COLOR_FORMAT(B6G5R5);
COLOR_FORMAT(Y8_VIDEO);
COLOR_FORMAT(G4R4);
COLOR_FORMAT(R1);
COLOR_FORMAT(E5B9G9R9_SHAREDEXP);
COLOR_FORMAT(G8B8G8R8);
COLOR_FORMAT(B8G8R8G8);
COLOR_FORMAT(DXT1);
COLOR_FORMAT(DXT23);
COLOR_FORMAT(DXT45);
COLOR_FORMAT(DXN1);
COLOR_FORMAT(DXN2);
COLOR_FORMAT(BC6H_SF16);
COLOR_FORMAT(BC6H_UF16);
COLOR_FORMAT(BC7U);
COLOR_FORMAT(X4V4Z24__COV4R4V);
COLOR_FORMAT(X4V4Z24__COV8R8V);
COLOR_FORMAT(V8Z24__COV4R12V);
COLOR_FORMAT(X8Z24_X20V4S8__COV4R4V);
COLOR_FORMAT(X8Z24_X20V4S8__COV8R8V);
COLOR_FORMAT(ZF32_X20V4X8__COV4R4V);
COLOR_FORMAT(ZF32_X20V4X8__COV8R8V);
COLOR_FORMAT(ZF32_X20V4S8__COV4R4V);
COLOR_FORMAT(ZF32_X20V4S8__COV8R8V);
COLOR_FORMAT(X8Z24_X16V8S8__COV4R12V);
COLOR_FORMAT(ZF32_X16V8X8__COV4R12V);
COLOR_FORMAT(ZF32_X16V8S8__COV4R12V);
COLOR_FORMAT(V8Z24__COV8R24V);
COLOR_FORMAT(X8Z24_X16V8S8__COV8R24V);
COLOR_FORMAT(ZF32_X16V8X8__COV8R24V);
COLOR_FORMAT(ZF32_X16V8S8__COV8R24V);
COLOR_FORMAT(B8G8R8A8);
COLOR_FORMAT(ASTC_2D_4X4);
COLOR_FORMAT(ASTC_2D_5X4);
COLOR_FORMAT(ASTC_2D_5X5);
COLOR_FORMAT(ASTC_2D_6X5);
COLOR_FORMAT(ASTC_2D_6X6);
COLOR_FORMAT(ASTC_2D_8X5);
COLOR_FORMAT(ASTC_2D_8X6);
COLOR_FORMAT(ASTC_2D_8X8);
COLOR_FORMAT(ASTC_2D_10X5);
COLOR_FORMAT(ASTC_2D_10X6);
COLOR_FORMAT(ASTC_2D_10X8);
COLOR_FORMAT(ASTC_2D_10X10);
COLOR_FORMAT(ASTC_2D_12X10);
COLOR_FORMAT(ASTC_2D_12X12);
COLOR_FORMAT(ASTC_SRGB_2D_4X4);
COLOR_FORMAT(ASTC_SRGB_2D_5X4);
COLOR_FORMAT(ASTC_SRGB_2D_5X5);
COLOR_FORMAT(ASTC_SRGB_2D_6X5);
COLOR_FORMAT(ASTC_SRGB_2D_6X6);
COLOR_FORMAT(ASTC_SRGB_2D_8X5);
COLOR_FORMAT(ASTC_SRGB_2D_8X6);
COLOR_FORMAT(ASTC_SRGB_2D_8X8);
COLOR_FORMAT(ASTC_SRGB_2D_10X5);
COLOR_FORMAT(ASTC_SRGB_2D_10X6);
COLOR_FORMAT(ASTC_SRGB_2D_10X8);
COLOR_FORMAT(ASTC_SRGB_2D_10X10);
COLOR_FORMAT(ASTC_SRGB_2D_12X10);
COLOR_FORMAT(ASTC_SRGB_2D_12X12);
COLOR_FORMAT(ETC2_RGB);
COLOR_FORMAT(ETC2_RGB_PTA);
COLOR_FORMAT(ETC2_RGBA);
COLOR_FORMAT(EAC);
COLOR_FORMAT(EACX2);

JS_CLASS(YCrCbType);
static SObject YCrCbType_Object
(
    "YCrCbType",
    YCrCbTypeClass,
    0,
    0,
    "YCrCbType formats."
);

// Constants
#define YCRCB_TYPE(x) \
    static SProperty YCrCbType_##x \
    ( \
        YCrCbType_Object, \
        #x, \
        0, \
        ColorUtils::x, \
        0, \
        0, \
        JSPROP_READONLY, \
        #x " YCrCbType format" \
    )

YCRCB_TYPE(CCIR601);
YCRCB_TYPE(CCIR709);

#define COLOR_METHOD(name,args,help) \
   static SMethod Color_##name       \
   (                                 \
      Color_Object,                  \
      #name,                         \
      C_Color_##name,                \
      args,                          \
      help                           \
   )

C_(Color_ColorDepthToFormat)
{
   JavaScriptPtr pJs;

   // Check the arguments.
   UINT32     Depth;
   JSObject * pReturlwals;
   if
   (
         (NumArguments != 2)
      || (OK != pJs->FromJsval(pArguments[0], &Depth))
      || (OK != pJs->FromJsval(pArguments[1], &pReturlwals))
   )
   {
      JS_ReportError(pContext, "Usage: Color.ColorDepthToFormat(Depth, [Format])");
      return JS_FALSE;
   }

   UINT32 Format = ColorUtils::ColorDepthToFormat(Depth);
   RETURN_RC(pJs->SetElement(pReturlwals, 0, Format));
}
COLOR_METHOD(ColorDepthToFormat, 2, "Get Color format for a given depth.");

P_(Color_Get_FP16DivideWorkaround)
{
    bool b = ColorUtils::GetFP16DivideWorkaround();
    JavaScriptPtr()->ToJsval(b, pValue);
    return JS_TRUE;
}
P_(Color_Set_FP16DivideWorkaround)
{
    bool b;
    JavaScriptPtr()->FromJsval(*pValue, &b);
    ColorUtils::SetFP16DivideWorkaround(b);
    return JS_TRUE;
}
static SProperty Color_FP16DivideWorkaround
(
    Color_Object,
    "FP16DivideWorkaround",
    0,
    true,
    Color_Get_FP16DivideWorkaround,
    Color_Set_FP16DivideWorkaround,
    0,
    "Enable/disable pre-gf11x FP16 WAR for x1024 FP16 values"
);
