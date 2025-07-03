/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 *
 * This file implements all of the OpenGL coverage objects used by MODS.
 *
*/

#include "core/include/coverage.h"
#include "opengl/modsgl.h"
#include "opengl/mglcoverage.h"
#include "gpu/js/glr_comm.h"
#include "core/include/utility.h"

#ifdef INCLUDE_OGL
#include "GL/gl.h"
#include "GL/glext.h"
#endif
using namespace TestCoverage;

//--------------------------------------------------------------------------------------------------
// HwTextureCoverageObject implementation
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
HwTextureCoverageObject::HwTextureCoverageObject() :
    CoverageObject(TestCoverage::Hardware, hwTexture, "HwTextureCoverage")
{
    Initialize();
}

//---------------------------------------------------------------------------------------------
// Delete this instance of the coverage object.
/*virtual*/ HwTextureCoverageObject::~HwTextureCoverageObject()
{
}

//---------------------------------------------------------------------------------------------
// Overrides for the Hardware ISA coverage object
RC HwTextureCoverageObject::Initialize()
{
    // Add a unique record for each coverage.
    // Hardware Coverage Records
    if (!m_Initialized)
    {
        // This is a list of internal texture formats that we know how to test. There may be others
        // supported in various OpenGL extensions but we lwrrently have support implemented for the
        // ones below.
        // Note: Some formats aren't supported by all GPUs and automatically be filtered out.
        // TODO: Audit all of the known internal formats and add any missing defines to this list.
        UINT32 internalFormats[] =
        {
            GL_RG32UI,
            GL_R32UI,
            GL_RGB5,
            GL_RGB5_A1,
            GL_ALPHA8,
            GL_R11F_G11F_B10F_EXT,
            GL_RGB10_A2,
            GL_DEPTH_COMPONENT16,
            GL_DEPTH_COMPONENT24,
            GL_RGBA8,
            GL_RGB8,
            GL_LUMINANCE8_ALPHA8,
            GL_LUMINANCE8,
            GL_RGBA16,
            GL_RGB16,
            GL_LUMINANCE16_ALPHA16,
            GL_LUMINANCE16,
            GL_RGBA16F_ARB,
            GL_RGB16F_ARB,
            GL_LUMINANCE_ALPHA16F_ARB,
            GL_LUMINANCE16F_ARB,
            GL_RGBA32F_ARB,
            GL_RGB32F_ARB,
            GL_LUMINANCE_ALPHA32F_ARB,
            GL_LUMINANCE32F_ARB,
            GL_SIGNED_RGBA8_LW,
            GL_SIGNED_RGB8_LW,
            GL_SIGNED_LUMINANCE8_ALPHA8_LW,
            GL_SIGNED_LUMINANCE8_LW,
            GL_RGBA16_SNORM,
            GL_RGB16_SNORM,
            GL_RG16_SNORM,
            GL_R16_SNORM,
            GL_RGBA8I_EXT,
            GL_RGB8I_EXT,
            GL_LUMINANCE_ALPHA8I_EXT,
            GL_LUMINANCE8I_EXT,
            GL_RGBA16I_EXT,
            GL_RGB16I_EXT,
            GL_LUMINANCE_ALPHA16I_EXT,
            GL_LUMINANCE16I_EXT,
            GL_RGBA32I_EXT,
            GL_RGB32I_EXT,
            GL_LUMINANCE_ALPHA32I_EXT,
            GL_LUMINANCE32I_EXT,
            GL_RGBA8UI_EXT,
            GL_RGB8UI_EXT,
            GL_LUMINANCE_ALPHA8UI_EXT,
            GL_LUMINANCE8UI_EXT,
            GL_RGBA16UI_EXT,
            GL_RGB16UI_EXT,
            GL_LUMINANCE_ALPHA16UI_EXT,
            GL_LUMINANCE16UI_EXT,
            GL_RGBA32UI_EXT,
            GL_RGB32UI_EXT,
            GL_LUMINANCE_ALPHA32UI_EXT,
            GL_LUMINANCE32UI_EXT,
            GL_ALPHA16,
            GL_ALPHA16F_ARB,
            GL_ALPHA32F_ARB,
            GL_INTENSITY16F_ARB,
            GL_INTENSITY32F_ARB,
            GL_RGBA4,
            GL_RGB9_E5_EXT,
            GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
            GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
            GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
            GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
            GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT,
            GL_COMPRESSED_LUMINANCE_LATC1_EXT,
            GL_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_EXT,
            GL_COMPRESSED_SIGNED_LUMINANCE_LATC1_EXT,
            GL_COMPRESSED_R11_EAC,
            GL_COMPRESSED_SIGNED_R11_EAC,
            GL_COMPRESSED_RG11_EAC,
            GL_COMPRESSED_SIGNED_RG11_EAC,
            GL_COMPRESSED_RGB8_ETC2,
            GL_COMPRESSED_SRGB8_ETC2,
            GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,
            GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2,
            GL_COMPRESSED_RGBA8_ETC2_EAC,
            GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,
            GL_STENCIL_INDEX8,
            GL_COMPRESSED_RGBA_ASTC_4x4_KHR,
            GL_COMPRESSED_RGBA_ASTC_5x4_KHR,
            GL_COMPRESSED_RGBA_ASTC_5x5_KHR,
            GL_COMPRESSED_RGBA_ASTC_6x5_KHR,
            GL_COMPRESSED_RGBA_ASTC_6x6_KHR,
            GL_COMPRESSED_RGBA_ASTC_8x5_KHR,
            GL_COMPRESSED_RGBA_ASTC_8x6_KHR,
            GL_COMPRESSED_RGBA_ASTC_8x8_KHR,
            GL_COMPRESSED_RGBA_ASTC_10x5_KHR,
            GL_COMPRESSED_RGBA_ASTC_10x6_KHR,
            GL_COMPRESSED_RGBA_ASTC_10x8_KHR,
            GL_COMPRESSED_RGBA_ASTC_10x10_KHR,
            GL_COMPRESSED_RGBA_ASTC_12x10_KHR,
            GL_COMPRESSED_RGBA_ASTC_12x12_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR,
            GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR
        };

        UINT64 key;
        for (UINT32 ii = 0; ii < NUMELEMS(internalFormats); ii++)
        {

            const mglTexFmtInfo * pFmtInfo = mglGetTexFmtInfo(internalFormats[ii]);
            if (0 == pFmtInfo || (0 == pFmtInfo->InternalFormat))
            {
                // Not a color format known to mods.
                continue;
            }
            if (! mglExtensionSupported(pFmtInfo->TexExtension))
            {
                //Extension is not supported by the hardware.
                continue;
            }

            key = (static_cast<UINT64>(m_Component) << 32) | internalFormats[ii];
            m_Records[key] = CoverageRecord();
        }
        // Register this object with the database
        //CoverageDatabase * pDB = CoverageDatabase::Instance();
        //pDB->RegisterObject(this);
        m_Initialized = true;
    }
    return OK;
}

//--------------------------------------------------------------------------------------------------
// Colwert the hardware texture format enumeration to a string.
string HwTextureCoverageObject::ComponentEnumToString(UINT32 component)
{
    string prefix("HW_TEX_");
    #define HW_TEXTURE_COV_ENUM_STRING(x)   case x: return prefix + #x
    switch (component)
    {
        default: return "Unknown";
        HW_TEXTURE_COV_ENUM_STRING(GL_RG32UI                           );
        HW_TEXTURE_COV_ENUM_STRING(GL_R32UI                            );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGB5                             );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGB5_A1                          );
        HW_TEXTURE_COV_ENUM_STRING(GL_ALPHA8                           );
        HW_TEXTURE_COV_ENUM_STRING(GL_R11F_G11F_B10F_EXT               );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGB10_A2                         );
        HW_TEXTURE_COV_ENUM_STRING(GL_DEPTH_COMPONENT16                );
        HW_TEXTURE_COV_ENUM_STRING(GL_DEPTH_COMPONENT24                );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGBA8                            );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGB8                             );
        HW_TEXTURE_COV_ENUM_STRING(GL_LUMINANCE8_ALPHA8                );
        HW_TEXTURE_COV_ENUM_STRING(GL_LUMINANCE8                       );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGBA16                           );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGB16                            );
        HW_TEXTURE_COV_ENUM_STRING(GL_LUMINANCE16_ALPHA16              );
        HW_TEXTURE_COV_ENUM_STRING(GL_LUMINANCE16                      );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGBA16F_ARB                      );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGB16F_ARB                       );
        HW_TEXTURE_COV_ENUM_STRING(GL_LUMINANCE_ALPHA16F_ARB           );
        HW_TEXTURE_COV_ENUM_STRING(GL_LUMINANCE16F_ARB                 );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGBA32F_ARB                      );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGB32F_ARB                       );
        HW_TEXTURE_COV_ENUM_STRING(GL_LUMINANCE_ALPHA32F_ARB           );
        HW_TEXTURE_COV_ENUM_STRING(GL_LUMINANCE32F_ARB                 );
        HW_TEXTURE_COV_ENUM_STRING(GL_SIGNED_RGBA8_LW                  );
        HW_TEXTURE_COV_ENUM_STRING(GL_SIGNED_RGB8_LW                   );
        HW_TEXTURE_COV_ENUM_STRING(GL_SIGNED_LUMINANCE8_ALPHA8_LW      );
        HW_TEXTURE_COV_ENUM_STRING(GL_SIGNED_LUMINANCE8_LW             );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGBA16_SNORM                     );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGB16_SNORM                      );
        HW_TEXTURE_COV_ENUM_STRING(GL_RG16_SNORM                       );
        HW_TEXTURE_COV_ENUM_STRING(GL_R16_SNORM                        );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGBA8I_EXT                       );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGB8I_EXT                        );
        HW_TEXTURE_COV_ENUM_STRING(GL_LUMINANCE_ALPHA8I_EXT            );
        HW_TEXTURE_COV_ENUM_STRING(GL_LUMINANCE8I_EXT                  );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGBA16I_EXT                      );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGB16I_EXT                       );
        HW_TEXTURE_COV_ENUM_STRING(GL_LUMINANCE_ALPHA16I_EXT           );
        HW_TEXTURE_COV_ENUM_STRING(GL_LUMINANCE16I_EXT                 );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGBA32I_EXT                      );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGB32I_EXT                       );
        HW_TEXTURE_COV_ENUM_STRING(GL_LUMINANCE_ALPHA32I_EXT           );
        HW_TEXTURE_COV_ENUM_STRING(GL_LUMINANCE32I_EXT                 );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGBA8UI_EXT                      );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGB8UI_EXT                       );
        HW_TEXTURE_COV_ENUM_STRING(GL_LUMINANCE_ALPHA8UI_EXT           );
        HW_TEXTURE_COV_ENUM_STRING(GL_LUMINANCE8UI_EXT                 );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGBA16UI_EXT                     );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGB16UI_EXT                      );
        HW_TEXTURE_COV_ENUM_STRING(GL_LUMINANCE_ALPHA16UI_EXT          );
        HW_TEXTURE_COV_ENUM_STRING(GL_LUMINANCE16UI_EXT                );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGBA32UI_EXT                     );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGB32UI_EXT                      );
        HW_TEXTURE_COV_ENUM_STRING(GL_LUMINANCE_ALPHA32UI_EXT          );
        HW_TEXTURE_COV_ENUM_STRING(GL_LUMINANCE32UI_EXT                );
        HW_TEXTURE_COV_ENUM_STRING(GL_ALPHA16                          );
        HW_TEXTURE_COV_ENUM_STRING(GL_ALPHA16F_ARB                     );
        HW_TEXTURE_COV_ENUM_STRING(GL_ALPHA32F_ARB                     );
        HW_TEXTURE_COV_ENUM_STRING(GL_INTENSITY16F_ARB                 );
        HW_TEXTURE_COV_ENUM_STRING(GL_INTENSITY32F_ARB                 );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGBA4                            );
        HW_TEXTURE_COV_ENUM_STRING(GL_RGB9_E5_EXT                      );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_RGB_S3TC_DXT1_EXT     );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT    );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_RGBA_S3TC_DXT3_EXT    );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT    );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT        );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_LUMINANCE_LATC1_EXT              );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_EXT );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_SIGNED_LUMINANCE_LATC1_EXT       );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_R11_EAC                          );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_SIGNED_R11_EAC                   );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_RG11_EAC                         );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_SIGNED_RG11_EAC                  );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_RGB8_ETC2                        );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_SRGB8_ETC2                       );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2    );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2   );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_RGBA8_ETC2_EAC                   );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC            );
        HW_TEXTURE_COV_ENUM_STRING(GL_STENCIL_INDEX8                   );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_4x4_KHR     );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_5x4_KHR     );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_5x5_KHR     );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_6x5_KHR     );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_6x6_KHR     );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_8x5_KHR     );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_8x6_KHR     );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_8x8_KHR     );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_10x5_KHR    );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_10x6_KHR    );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_10x8_KHR    );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_10x10_KHR   );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_12x10_KHR   );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_RGBA_ASTC_12x12_KHR   );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR   );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR   );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR   );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR   );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR   );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR   );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR   );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR   );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR  );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR  );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR  );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR );
        HW_TEXTURE_COV_ENUM_STRING(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR );
    }
}

//--------------------------------------------------------------------------------------------------
// HwIsaCoverageObject implementation
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
HwIsaCoverageObject::HwIsaCoverageObject() :
    CoverageObject(TestCoverage::Hardware, hwIsa,"HwISACoverage")
{
    Initialize();
}

//--------------------------------------------------------------------------------------------------
// Free up resources of the singleton object.
/*virtual*/ HwIsaCoverageObject::~HwIsaCoverageObject()
{
}

//---------------------------------------------------------------------------------------------
// Overrides for the Hardware ISA coverage object
// Note: GLRandom tests use fancy pickers to randomly choose what hardware ISA instruction should
//       be used for a given instruction. This object will keep track of how many times each
//       instruction gets used. For consistency we create a new set of defines to identify these
//       instructions.
RC HwIsaCoverageObject::Initialize()
{
    // Add a unique record for each coverage.
    // Hardware Coverage Records
    #define HW_ISA_ABS          RND_GPU_PROG_OPCODE_abs
    #define HW_ISA_ADD          RND_GPU_PROG_OPCODE_add
    #define HW_ISA_AND          RND_GPU_PROG_OPCODE_and
    #define HW_ISA_BRK          RND_GPU_PROG_OPCODE_brk
    #define HW_ISA_CAL          RND_GPU_PROG_OPCODE_cal
    #define HW_ISA_CEIL         RND_GPU_PROG_OPCODE_cei
    #define HW_ISA_CMP          RND_GPU_PROG_OPCODE_cmp
    #define HW_ISA_CONT         RND_GPU_PROG_OPCODE_con
    #define HW_ISA_COS          RND_GPU_PROG_OPCODE_cos
    #define HW_ISA_DIV          RND_GPU_PROG_OPCODE_div
    #define HW_ISA_DP2          RND_GPU_PROG_OPCODE_dp2
    #define HW_ISA_DP2A         RND_GPU_PROG_OPCODE_dp2a
    #define HW_ISA_DP3          RND_GPU_PROG_OPCODE_dp3
    #define HW_ISA_DP4          RND_GPU_PROG_OPCODE_dp4
    #define HW_ISA_DPH          RND_GPU_PROG_OPCODE_dph
    #define HW_ISA_DST          RND_GPU_PROG_OPCODE_dst
    #define HW_ISA_ELSE         RND_GPU_PROG_OPCODE_else
    #define HW_ISA_ENDIF        RND_GPU_PROG_OPCODE_endif
    #define HW_ISA_ENDREP       RND_GPU_PROG_OPCODE_endrep
    #define HW_ISA_EX2          RND_GPU_PROG_OPCODE_ex2
    #define HW_ISA_FLR          RND_GPU_PROG_OPCODE_flr
    #define HW_ISA_FRC          RND_GPU_PROG_OPCODE_frc
    #define HW_ISA_I2F          RND_GPU_PROG_OPCODE_i2f
    #define HW_ISA_IF           RND_GPU_PROG_OPCODE_if
    #define HW_ISA_LG2          RND_GPU_PROG_OPCODE_lg2
    #define HW_ISA_LIT          RND_GPU_PROG_OPCODE_lit
    #define HW_ISA_LRP          RND_GPU_PROG_OPCODE_lrp
    #define HW_ISA_MAD          RND_GPU_PROG_OPCODE_mad
    #define HW_ISA_MAX          RND_GPU_PROG_OPCODE_max
    #define HW_ISA_MIN          RND_GPU_PROG_OPCODE_min
    #define HW_ISA_MOD          RND_GPU_PROG_OPCODE_mod
    #define HW_ISA_MOV          RND_GPU_PROG_OPCODE_mov
    #define HW_ISA_MUL          RND_GPU_PROG_OPCODE_mul
    #define HW_ISA_NOT          RND_GPU_PROG_OPCODE_not
    #define HW_ISA_NRM          RND_GPU_PROG_OPCODE_nrm
    #define HW_ISA_OR           RND_GPU_PROG_OPCODE_or
    #define HW_ISA_PK2H         RND_GPU_PROG_OPCODE_pk2h
    #define HW_ISA_PK2US        RND_GPU_PROG_OPCODE_pk2us
    #define HW_ISA_PK4B         RND_GPU_PROG_OPCODE_pk4b
    #define HW_ISA_PK4UB        RND_GPU_PROG_OPCODE_pk4ub
    #define HW_ISA_POW          RND_GPU_PROG_OPCODE_pow
    #define HW_ISA_RCC          RND_GPU_PROG_OPCODE_rcc
    #define HW_ISA_RCP          RND_GPU_PROG_OPCODE_rcp
    #define HW_ISA_REP          RND_GPU_PROG_OPCODE_rep
    #define HW_ISA_RET          RND_GPU_PROG_OPCODE_ret
    #define HW_ISA_RFL          RND_GPU_PROG_OPCODE_rfl
    #define HW_ISA_ROUND        RND_GPU_PROG_OPCODE_round
    #define HW_ISA_RSQ          RND_GPU_PROG_OPCODE_rsq
    #define HW_ISA_SAD          RND_GPU_PROG_OPCODE_sad
    #define HW_ISA_SCS          RND_GPU_PROG_OPCODE_scs
    #define HW_ISA_SEQ          RND_GPU_PROG_OPCODE_seq
    #define HW_ISA_SFL          RND_GPU_PROG_OPCODE_sfl
    #define HW_ISA_SGE          RND_GPU_PROG_OPCODE_sge
    #define HW_ISA_SGT          RND_GPU_PROG_OPCODE_sgt
    #define HW_ISA_SHL          RND_GPU_PROG_OPCODE_shl
    #define HW_ISA_SHR          RND_GPU_PROG_OPCODE_shr
    #define HW_ISA_SIN          RND_GPU_PROG_OPCODE_sin
    #define HW_ISA_SLE          RND_GPU_PROG_OPCODE_sle
    #define HW_ISA_SLT          RND_GPU_PROG_OPCODE_slt
    #define HW_ISA_SNE          RND_GPU_PROG_OPCODE_sne
    #define HW_ISA_SSG          RND_GPU_PROG_OPCODE_ssg
    #define HW_ISA_STR          RND_GPU_PROG_OPCODE_str
    #define HW_ISA_SUB          RND_GPU_PROG_OPCODE_sub
    #define HW_ISA_SWZ          RND_GPU_PROG_OPCODE_swz
    #define HW_ISA_TEX          RND_GPU_PROG_OPCODE_tex
    #define HW_ISA_TRUNC        RND_GPU_PROG_OPCODE_trunc
    #define HW_ISA_TXB          RND_GPU_PROG_OPCODE_txb
    #define HW_ISA_TXD          RND_GPU_PROG_OPCODE_txd
    #define HW_ISA_TXF          RND_GPU_PROG_OPCODE_txf
    #define HW_ISA_TXL          RND_GPU_PROG_OPCODE_txl
    #define HW_ISA_TXP          RND_GPU_PROG_OPCODE_txp
    #define HW_ISA_TXQ          RND_GPU_PROG_OPCODE_txq
    #define HW_ISA_UP2H         RND_GPU_PROG_OPCODE_up2h
    #define HW_ISA_UP2US        RND_GPU_PROG_OPCODE_up2us
    #define HW_ISA_UP4B         RND_GPU_PROG_OPCODE_up4b
    #define HW_ISA_UP4UB        RND_GPU_PROG_OPCODE_up4ub
    #define HW_ISA_X2D          RND_GPU_PROG_OPCODE_x2d
    #define HW_ISA_XOR          RND_GPU_PROG_OPCODE_xor
    #define HW_ISA_XPD          RND_GPU_PROG_OPCODE_xpd
    #define HW_ISA_TXG          RND_GPU_PROG_OPCODE_txg
    #define HW_ISA_TEX2         RND_GPU_PROG_OPCODE_tex2
    #define HW_ISA_TXB2         RND_GPU_PROG_OPCODE_txb2
    #define HW_ISA_TXL2         RND_GPU_PROG_OPCODE_txl2
    #define HW_ISA_TXFMS        RND_GPU_PROG_OPCODE_txfms
    #define HW_ISA_BFE          RND_GPU_PROG_OPCODE_bfe
    #define HW_ISA_BFI          RND_GPU_PROG_OPCODE_bfi
    #define HW_ISA_BFR          RND_GPU_PROG_OPCODE_bfr
    #define HW_ISA_BTC          RND_GPU_PROG_OPCODE_btc
    #define HW_ISA_BTFL         RND_GPU_PROG_OPCODE_btfl
    #define HW_ISA_BTFM         RND_GPU_PROG_OPCODE_btfm
    #define HW_ISA_TXGO         RND_GPU_PROG_OPCODE_txgo
    #define HW_ISA_CVT          RND_GPU_PROG_OPCODE_cvt
    #define HW_ISA_PK64         RND_GPU_PROG_OPCODE_pk64
    #define HW_ISA_UP64         RND_GPU_PROG_OPCODE_up64
    #define HW_ISA_LDC          RND_GPU_PROG_OPCODE_ldc
    #define HW_ISA_CALI         RND_GPU_PROG_OPCODE_cali
    #define HW_ISA_LOADIM       RND_GPU_PROG_OPCODE_loadim
    #define HW_ISA_STOREIM      RND_GPU_PROG_OPCODE_storeim
    #define HW_ISA_MEMBAR       RND_GPU_PROG_OPCODE_membar
    #define HW_ISA_DDX          RND_GPU_PROG_FR_OPCODE_ddx
    #define HW_ISA_DDY          RND_GPU_PROG_FR_OPCODE_ddy
    #define HW_ISA_KIL          RND_GPU_PROG_FR_OPCODE_kil
    #define HW_ISA_LOD          RND_GPU_PROG_FR_OPCODE_lod
    #define HW_ISA_IPAC         RND_GPU_PROG_FR_OPCODE_ipac
    #define HW_ISA_IPAO         RND_GPU_PROG_FR_OPCODE_ipao
    #define HW_ISA_IPAS         RND_GPU_PROG_FR_OPCODE_ipas
    #define HW_ISA_ATOM         RND_GPU_PROG_FR_OPCODE_atom
    #define HW_ISA_LOAD         RND_GPU_PROG_FR_OPCODE_load
    #define HW_ISA_STORE        RND_GPU_PROG_FR_OPCODE_store
    #define HW_ISA_ATOMIM       RND_GPU_PROG_FR_OPCODE_atomim
    #define HW_ISA_NUM_OPCODES  RND_GPU_PROG_FR_OPCODE_NUM_OPCODES

    if (!m_Initialized)
    {
        UINT64 key;
        for (UINT32 subComp = HW_ISA_ABS; subComp < HW_ISA_NUM_OPCODES; subComp++)
        {
            key = (static_cast<UINT64>(m_Component) << 32) | subComp;
            m_Records[key] = CoverageRecord();
        }
        m_Initialized = true;
    }
    return OK;
}

//--------------------------------------------------------------------------------------------------
// Colwert the hardware isa instruction define to a string.
string HwIsaCoverageObject::ComponentEnumToString(UINT32 component)
{
    #define HW_ISA_COV_ENUM_STRING(x)   case x: return (prefix + #x)
    string prefix("HW_ISA_");
    switch (component)
    {
        default: return "Unknown";
        HW_ISA_COV_ENUM_STRING(HW_ISA_ABS    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_ADD    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_AND    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_BRK    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_CAL    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_CEIL   );
        HW_ISA_COV_ENUM_STRING(HW_ISA_CMP    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_CONT   );
        HW_ISA_COV_ENUM_STRING(HW_ISA_COS    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_DIV    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_DP2    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_DP2A   );
        HW_ISA_COV_ENUM_STRING(HW_ISA_DP3    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_DP4    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_DPH    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_DST    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_ELSE   );
        HW_ISA_COV_ENUM_STRING(HW_ISA_ENDIF  );
        HW_ISA_COV_ENUM_STRING(HW_ISA_ENDREP );
        HW_ISA_COV_ENUM_STRING(HW_ISA_EX2    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_FLR    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_FRC    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_I2F    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_IF     );
        HW_ISA_COV_ENUM_STRING(HW_ISA_LG2    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_LIT    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_LRP    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_MAD    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_MAX    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_MIN    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_MOD    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_MOV    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_MUL    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_NOT    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_NRM    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_OR     );
        HW_ISA_COV_ENUM_STRING(HW_ISA_PK2H   );
        HW_ISA_COV_ENUM_STRING(HW_ISA_PK2US  );
        HW_ISA_COV_ENUM_STRING(HW_ISA_PK4B   );
        HW_ISA_COV_ENUM_STRING(HW_ISA_PK4UB  );
        HW_ISA_COV_ENUM_STRING(HW_ISA_POW    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_RCC    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_RCP    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_REP    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_RET    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_RFL    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_ROUND  );
        HW_ISA_COV_ENUM_STRING(HW_ISA_RSQ    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_SAD    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_SCS    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_SEQ    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_SFL    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_SGE    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_SGT    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_SHL    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_SHR    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_SIN    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_SLE    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_SLT    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_SNE    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_SSG    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_STR    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_SUB    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_SWZ    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_TEX    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_TRUNC  );
        HW_ISA_COV_ENUM_STRING(HW_ISA_TXB    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_TXD    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_TXF    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_TXL    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_TXP    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_TXQ    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_UP2H   );
        HW_ISA_COV_ENUM_STRING(HW_ISA_UP2US  );
        HW_ISA_COV_ENUM_STRING(HW_ISA_UP4B   );
        HW_ISA_COV_ENUM_STRING(HW_ISA_UP4UB  );
        HW_ISA_COV_ENUM_STRING(HW_ISA_X2D    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_XOR    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_XPD    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_TXG    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_TEX2   );
        HW_ISA_COV_ENUM_STRING(HW_ISA_TXB2   );
        HW_ISA_COV_ENUM_STRING(HW_ISA_TXL2   );
        HW_ISA_COV_ENUM_STRING(HW_ISA_TXFMS  );
        HW_ISA_COV_ENUM_STRING(HW_ISA_BFE    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_BFI    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_BFR    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_BTC    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_BTFL   );
        HW_ISA_COV_ENUM_STRING(HW_ISA_BTFM   );
        HW_ISA_COV_ENUM_STRING(HW_ISA_TXGO   );
        HW_ISA_COV_ENUM_STRING(HW_ISA_CVT    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_PK64   );
        HW_ISA_COV_ENUM_STRING(HW_ISA_UP64   );
        HW_ISA_COV_ENUM_STRING(HW_ISA_LDC    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_CALI   );
        HW_ISA_COV_ENUM_STRING(HW_ISA_LOADIM );
        HW_ISA_COV_ENUM_STRING(HW_ISA_STOREIM);
        HW_ISA_COV_ENUM_STRING(HW_ISA_MEMBAR );
        HW_ISA_COV_ENUM_STRING(HW_ISA_DDX    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_DDY    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_KIL    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_LOD    );
        HW_ISA_COV_ENUM_STRING(HW_ISA_IPAC   );
        HW_ISA_COV_ENUM_STRING(HW_ISA_IPAO   );
        HW_ISA_COV_ENUM_STRING(HW_ISA_IPAS   );
        HW_ISA_COV_ENUM_STRING(HW_ISA_ATOM   );
        HW_ISA_COV_ENUM_STRING(HW_ISA_LOAD   );
        HW_ISA_COV_ENUM_STRING(HW_ISA_STORE  );
        HW_ISA_COV_ENUM_STRING(HW_ISA_ATOMIM );
    }
}

//--------------------------------------------------------------------------------------------------
// OpenGL's Extension coverage object
//--------------------------------------------------------------------------------------------------
SwOpenGLExtCoverageObject::SwOpenGLExtCoverageObject() :
    CoverageObject(TestCoverage::Software, TestCoverage::swOpenGLExt,"SwOpenGLExtCoverage")
{
    Initialize();
}

//--------------------------------------------------------------------------------------------------
//
RC SwOpenGLExtCoverageObject::Initialize()
{
    const char * extensions = (const char*) glGetString(GL_EXTENSIONS);
    if (NULL != extensions)
    {
        string s;
        // Parse the master string into a list of sub-strings. Once sub-string for each extension.
        do
        {
            const char * end = strchr(extensions, ' ');
            const char * next = end;
            if (NULL != end)
            {
                s.assign(extensions, end-extensions);
                next = end + 1;
            }
            else
            {
                s = extensions;
            }
            m_Ext.push_back(s);

            extensions = next;
        }
        while (NULL != extensions && '\0' != *extensions);
    }

    // Use the m_Ext index value as the subComponent when creating new coverage records.
    UINT64 key = 0;
    for (unsigned ii = 0; ii < m_Ext.size(); ii++)
    {
        key = (static_cast<UINT64>(m_Component) << 32) | ii;
        m_Records[key] = CoverageRecord();
    }
    return (OK);
}

//--------------------------------------------------------------------------------------------------
// Return the OpenGL extension string for this component.
string SwOpenGLExtCoverageObject::ComponentEnumToString(UINT32 component)
{
    if (component < m_Ext.size())
    {
        return m_Ext[component];
    }
    else
    {
        return "Unknown";
    }
}

//--------------------------------------------------------------------------------------------------
// Free up resources of the singleton object.
/*virtual*/ SwOpenGLExtCoverageObject::~SwOpenGLExtCoverageObject()
{
}

