/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007,2013-2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// RndPixels: OpenGL Randomizer for pixel data formats.
//
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

#include "glrandom.h"   // declaration of our namespace
#include "core/include/massert.h"
#include "core/include/utility.h"

using namespace GLRandom;

// Constructor
RndPixels::RndPixels(GLRandomTest * pglr)
: GLRandomHelper(pglr, RND_PIXEL_NUM_PICKERS, "Pixels")
{
    memset(&m_Info, 0, sizeof(m_Info));

    // Initialize internals to safe values. See InitOpenGL() for final values
    m_WindowRect[wrWidth] = 640;
    m_WindowRect[wrHeight] = 480;
    m_BufferDepth[0] = 0; //!< min buffer depth
    m_BufferDepth[1] = 16; //!<max buffer depth
}

//------------------------------------------------------------------------------
//! \brief Destructor
RndPixels::~RndPixels()
{
    delete [](GLfloat*)m_Info.Data; //!< its OK to delete a NULL pointer
}

//------------------------------------------------------------------------------
//! \brief Perform OpenGL and object specific initialization.
RC RndPixels::InitOpenGL()
{
    RC rc = OK;
    m_Info.Data = new GLfloat[MaxRectHeight * MaxRectWidth * 4];

    if ( m_Info.Data )
        memset(m_Info.Data, 0, sizeof(GLfloat)*MaxRectHeight*MaxRectWidth*4);
    else
    {
        Printf(Tee::PriHigh,
               "RndPixels:Failed to allocate %d bytes for pixel buffer\n",
                (GLint)(MaxRectHeight * MaxRectWidth * 4 * sizeof(GLfloat)));
        rc = RC::CANNOT_ALLOCATE_MEMORY;
    }

    glGetIntegerv(GL_SCISSOR_BOX, m_WindowRect); //!< Get the viewport params
    glGetIntegerv(GL_DEPTH_RANGE, m_BufferDepth);

    // Don't reset the pickers if they were set from JS
    if (!m_Pickers[RND_PIXEL_POS_X].WasSetFromJs())
        SetDefaultPicker(RND_PIXEL_POS_X);
    if (!m_Pickers[RND_PIXEL_POS_Y].WasSetFromJs())
        SetDefaultPicker(RND_PIXEL_POS_Y);
    if (!m_Pickers[RND_PIXEL_POS_Z].WasSetFromJs())
        SetDefaultPicker(RND_PIXEL_POS_Z);

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Setup the default values for first to last picker
RC RndPixels::SetDefaultPicker( int picker)
{
    switch (picker)
    {
        default:
            return RC::BAD_PICKER_INDEX;

        case RND_PIXEL_FMT:
            //
            m_Pickers[RND_PIXEL_FMT].ConfigRandom();
            m_Pickers[RND_PIXEL_FMT].AddRandItem(1,GL_STENCIL_INDEX   );
            m_Pickers[RND_PIXEL_FMT].AddRandItem(1,GL_DEPTH_COMPONENT );
            m_Pickers[RND_PIXEL_FMT].AddRandItem(1,GL_RED             );
            m_Pickers[RND_PIXEL_FMT].AddRandItem(1,GL_GREEN           );
            m_Pickers[RND_PIXEL_FMT].AddRandItem(1,GL_BLUE            );
            m_Pickers[RND_PIXEL_FMT].AddRandItem(1,GL_ALPHA           );
            m_Pickers[RND_PIXEL_FMT].AddRandItem(1,GL_RGB             );
            m_Pickers[RND_PIXEL_FMT].AddRandItem(1,GL_RGBA            );
            m_Pickers[RND_PIXEL_FMT].AddRandItem(1,GL_LUMINANCE       );
            m_Pickers[RND_PIXEL_FMT].AddRandItem(1,GL_LUMINANCE_ALPHA );

            m_Pickers[RND_PIXEL_FMT].AddRandItem(1,GL_BGR             );
            m_Pickers[RND_PIXEL_FMT].AddRandItem(1,GL_BGRA            );
            m_Pickers[RND_PIXEL_FMT].CompileRandom();
            break;

        case RND_PIXEL_DATA_TYPE:
            m_Pickers[RND_PIXEL_DATA_TYPE].ConfigRandom();
            m_Pickers[RND_PIXEL_DATA_TYPE].AddRandItem(1, GL_BITMAP        );
            m_Pickers[RND_PIXEL_DATA_TYPE].AddRandItem(1, GL_BYTE          );
            m_Pickers[RND_PIXEL_DATA_TYPE].AddRandItem(1, GL_UNSIGNED_BYTE );
            m_Pickers[RND_PIXEL_DATA_TYPE].AddRandItem(1, GL_SHORT         );
            m_Pickers[RND_PIXEL_DATA_TYPE].AddRandItem(1, GL_UNSIGNED_SHORT);
            m_Pickers[RND_PIXEL_DATA_TYPE].AddRandItem(1, GL_INT           );
            m_Pickers[RND_PIXEL_DATA_TYPE].AddRandItem(1, GL_UNSIGNED_INT  );
            m_Pickers[RND_PIXEL_DATA_TYPE].AddRandItem(1, GL_FLOAT         );
            m_Pickers[RND_PIXEL_DATA_TYPE].AddRandItem(1, GL_UNSIGNED_BYTE_3_3_2        );
            m_Pickers[RND_PIXEL_DATA_TYPE].AddRandItem(1, GL_UNSIGNED_BYTE_2_3_3_REV    );
            m_Pickers[RND_PIXEL_DATA_TYPE].AddRandItem(1, GL_UNSIGNED_SHORT_5_6_5       );
            m_Pickers[RND_PIXEL_DATA_TYPE].AddRandItem(1, GL_UNSIGNED_SHORT_5_6_5_REV   );
            m_Pickers[RND_PIXEL_DATA_TYPE].AddRandItem(1, GL_UNSIGNED_SHORT_4_4_4_4     );
            m_Pickers[RND_PIXEL_DATA_TYPE].AddRandItem(1, GL_UNSIGNED_SHORT_4_4_4_4_REV );
            m_Pickers[RND_PIXEL_DATA_TYPE].AddRandItem(1, GL_UNSIGNED_SHORT_5_5_5_1     );
            m_Pickers[RND_PIXEL_DATA_TYPE].AddRandItem(1, GL_UNSIGNED_SHORT_1_5_5_5_REV );
            m_Pickers[RND_PIXEL_DATA_TYPE].AddRandItem(1, GL_UNSIGNED_INT_8_8_8_8       );
            m_Pickers[RND_PIXEL_DATA_TYPE].AddRandItem(1, GL_UNSIGNED_INT_8_8_8_8_REV   );
            m_Pickers[RND_PIXEL_DATA_TYPE].AddRandItem(1, GL_UNSIGNED_INT_10_10_10_2    );
            m_Pickers[RND_PIXEL_DATA_TYPE].AddRandItem(1, GL_UNSIGNED_INT_2_10_10_10_REV);
            m_Pickers[RND_PIXEL_DATA_TYPE].CompileRandom();
            break;

        case RND_PIXEL_WIDTH:
             // width of the pixel rectangle
             m_Pickers[RND_PIXEL_WIDTH].ConfigRandom();
             m_Pickers[RND_PIXEL_WIDTH].AddRandItem(95, MinRectWidth);
             m_Pickers[RND_PIXEL_WIDTH].AddRandRange(5,
                                                     MinRectWidth,
                                                     MaxRectWidth);
             m_Pickers[RND_PIXEL_WIDTH].CompileRandom();
            break;

        case RND_PIXEL_HEIGHT:
            // height of the pixel rectangle
            m_Pickers[RND_PIXEL_HEIGHT].ConfigRandom();
            m_Pickers[RND_PIXEL_HEIGHT].AddRandItem(95, MinRectHeight);
            m_Pickers[RND_PIXEL_HEIGHT].AddRandRange(5,
                                                     MinRectHeight,
                                                     MaxRectHeight);
            m_Pickers[RND_PIXEL_HEIGHT].CompileRandom();
            break;

        case RND_PIXEL_POS_X:
            { // enforce scope to prevent warnings
            int maxX = m_WindowRect[wrWidth] - MaxRectWidth - 1;
            // the rightmost position of the pixel rectangle
            m_Pickers[RND_PIXEL_POS_X].ConfigRandom();
            m_Pickers[RND_PIXEL_POS_X].AddRandRange(1, 0, maxX);
            m_Pickers[RND_PIXEL_POS_X].CompileRandom();
            }
            break;

        case RND_PIXEL_POS_Y:
            { // top most position of the pixel rectangle
            int maxY = m_WindowRect[wrHeight] - MaxRectHeight - 1;
            m_Pickers[RND_PIXEL_POS_Y].ConfigRandom();
            m_Pickers[RND_PIXEL_POS_Y].AddRandRange(1, 0, maxY);
            m_Pickers[RND_PIXEL_POS_Y].CompileRandom();
            }
            break;

        case RND_PIXEL_POS_Z:
            m_Pickers[RND_PIXEL_POS_Z].ConfigRandom();
            m_Pickers[RND_PIXEL_POS_Z].AddRandRange(1,
                                                    m_BufferDepth[0],
                                                    m_BufferDepth[1]);
            m_Pickers[RND_PIXEL_POS_Z].CompileRandom();
            break;

        case RND_PIXEL_DATA_BYTE:
            //
            m_Pickers[RND_PIXEL_DATA_BYTE].ConfigRandom();
            m_Pickers[RND_PIXEL_DATA_BYTE].AddRandRange(1, 0, 0xff);
            m_Pickers[RND_PIXEL_DATA_BYTE].CompileRandom();
            break;

        case RND_PIXEL_DATA_SHORT:
            //
            m_Pickers[RND_PIXEL_DATA_SHORT].ConfigRandom();
            m_Pickers[RND_PIXEL_DATA_SHORT].AddRandRange(1, 0, 0xffff);
            m_Pickers[RND_PIXEL_DATA_SHORT].CompileRandom();
            break;

        case RND_PIXEL_DATA_INT:
            //
            m_Pickers[RND_PIXEL_DATA_INT].ConfigRandom();
            m_Pickers[RND_PIXEL_DATA_INT].AddRandRange(1, 0, 0xffffffff);
            m_Pickers[RND_PIXEL_DATA_INT].CompileRandom();
            break;

        case RND_PIXEL_DATA_FLOAT:
            //
            m_Pickers[RND_PIXEL_DATA_FLOAT].FConfigRandom();
            m_Pickers[RND_PIXEL_DATA_FLOAT].FAddRandRange(1, 0.0f, 1.0f);
            m_Pickers[RND_PIXEL_DATA_FLOAT].CompileRandom();
            break;

        case RND_PIXEL_FMT_RGBA:
            // Special processing when the data type is GL_BITMAP
            m_Pickers[RND_PIXEL_FMT_RGBA].ConfigRandom();
            m_Pickers[RND_PIXEL_FMT_RGBA].AddRandItem(1, GL_RGBA);
            m_Pickers[RND_PIXEL_FMT_RGBA].AddRandItem(1, GL_BGRA);
            m_Pickers[RND_PIXEL_FMT_RGBA].CompileRandom();
            break;

        case RND_PIXEL_FMT_STENCIL:
            // Special processing when the data type is GL_BITMAP
            m_Pickers[RND_PIXEL_FMT_STENCIL].ConfigRandom();
            m_Pickers[RND_PIXEL_FMT_STENCIL].AddRandItem(1,GL_COLOR_INDEX);
            m_Pickers[RND_PIXEL_FMT_STENCIL].AddRandItem(1,GL_STENCIL_INDEX);
            m_Pickers[RND_PIXEL_FMT_STENCIL].CompileRandom();
            break;

        case RND_PIXEL_DATA_ALIGNMENT:
            m_Pickers[RND_PIXEL_DATA_ALIGNMENT].ConfigRandom();
            m_Pickers[RND_PIXEL_DATA_ALIGNMENT].AddRandItem(5,1);   //!< 50%
            m_Pickers[RND_PIXEL_DATA_ALIGNMENT].AddRandItem(2,2);   //!< 20%
            m_Pickers[RND_PIXEL_DATA_ALIGNMENT].AddRandItem(2,4);   //!< 20%
            m_Pickers[RND_PIXEL_DATA_ALIGNMENT].AddRandItem(1,8);   //!< 10%
            m_Pickers[RND_PIXEL_DATA_ALIGNMENT].CompileRandom();
            break;

        case RND_PIXEL_FILL_MODE:
            m_Pickers[RND_PIXEL_FILL_MODE].ConfigRandom();
            m_Pickers[RND_PIXEL_FILL_MODE].AddRandItem(5,RND_PIXEL_FILL_MODE_solid);  //!< 50%
            m_Pickers[RND_PIXEL_FILL_MODE].AddRandItem(2,RND_PIXEL_FILL_MODE_stripes);//!< 20%
            m_Pickers[RND_PIXEL_FILL_MODE].AddRandItem(3,RND_PIXEL_FILL_MODE_random); //!< 30%
            m_Pickers[RND_PIXEL_FILL_MODE].CompileRandom();
            break;

        case RND_PIXEL_3D_NUM_TEXTURES:
            // Number of textures to reserve for 3d pixels
            m_Pickers[RND_PIXEL_3D_NUM_TEXTURES].ConfigRandom();
            m_Pickers[RND_PIXEL_3D_NUM_TEXTURES].AddRandRange(1, 1, (UINT32)MaxTextures);
            m_Pickers[RND_PIXEL_3D_NUM_TEXTURES].CompileRandom();
            break;

        case RND_PIXEL_3D_TEX_FORMAT:
            // Leave out any depth (not supported), unsigned int (uninteresting
            // results), and int (uninteresting results) formats
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].ConfigRandom();
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_RGB5);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_RGB5_A1);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_ALPHA8);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_R11F_G11F_B10F_EXT);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_RGB10_A2);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_RGBA8);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_RGB8);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_LUMINANCE8_ALPHA8);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_LUMINANCE8);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_RGBA16);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_RGB16);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_LUMINANCE16_ALPHA16);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_LUMINANCE16);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_RGBA16F_ARB);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_RGB16F_ARB);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_LUMINANCE_ALPHA16F_ARB);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_LUMINANCE16F_ARB);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_RGBA32F_ARB);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_RGB32F_ARB);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_LUMINANCE_ALPHA32F_ARB);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_LUMINANCE32F_ARB);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_SIGNED_RGBA8_LW);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_SIGNED_RGB8_LW);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_SIGNED_LUMINANCE8_ALPHA8_LW);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_SIGNED_LUMINANCE8_LW);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_RGBA16_SNORM);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_RGB16_SNORM);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_RG16_SNORM);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_R16_SNORM);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_ALPHA16);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_ALPHA16F_ARB);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_ALPHA32F_ARB);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_INTENSITY16F_ARB);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_INTENSITY32F_ARB);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_RGBA4);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_RGB9_E5_EXT);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_COMPRESSED_LUMINANCE_LATC1_EXT);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_EXT );
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_COMPRESSED_SIGNED_LUMINANCE_LATC1_EXT);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_COMPRESSED_R11_EAC);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_COMPRESSED_SIGNED_R11_EAC);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_COMPRESSED_RG11_EAC);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_COMPRESSED_SIGNED_RG11_EAC);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_COMPRESSED_RGB8_ETC2);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_COMPRESSED_SRGB8_ETC2);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_COMPRESSED_RGBA8_ETC2_EAC);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].AddRandItem(1, GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC);
            m_Pickers[RND_PIXEL_3D_TEX_FORMAT].CompileRandom();
            break;

        case RND_PIXEL_3D_S0:
            // (S0, T0) and (S1, T1) define corners of the texture sampling
            // rectangle used for 3D pixels
            //
            // S0 coordinage of texture sampling rectangle
            m_Pickers[RND_PIXEL_3D_S0].FConfigRandom();
            m_Pickers[RND_PIXEL_3D_S0].FAddRandRange(1, 0.0f, 1.0f);
            m_Pickers[RND_PIXEL_3D_S0].CompileRandom();
            break;

        case RND_PIXEL_3D_T0:
            // T0 coordinage of texture sampling rectangle
            m_Pickers[RND_PIXEL_3D_T0].FConfigRandom();
            m_Pickers[RND_PIXEL_3D_T0].FAddRandRange(1, 0.0f, 1.0f);
            m_Pickers[RND_PIXEL_3D_T0].CompileRandom();
            break;

        case RND_PIXEL_3D_S1:
            // S1 coordinage of texture sampling rectangle
            m_Pickers[RND_PIXEL_3D_S1].FConfigRandom();
            m_Pickers[RND_PIXEL_3D_S1].FAddRandRange(1, 0.0f, 1.0f);
            m_Pickers[RND_PIXEL_3D_S1].CompileRandom();
            break;

        case RND_PIXEL_3D_T1:
            // T1 coordinage of texture sampling rectangle
            m_Pickers[RND_PIXEL_3D_T1].FConfigRandom();
            m_Pickers[RND_PIXEL_3D_T1].FAddRandRange(1, 0.0f, 1.0f);
            m_Pickers[RND_PIXEL_3D_T1].CompileRandom();
            break;
    }
    return OK;
}

//------------------------------------------------------------------------------
// Do once-per-restart picks, prints, & sends.
//
RC RndPixels::Restart()
{
    m_Pickers.Restart();

    // Pick a random number of textures to allocate for use by 3d pixels
    UINT32 numTextures = m_Pickers[RND_PIXEL_3D_NUM_TEXTURES].Pick();
    UINT32 retry;

    m_Txf.clear();
    for (UINT32 i = 0; i < numTextures; i++)
    {
        TxfReq texfReq;
        retry = 0;
        do
        {
            // 3d pixels requires 2d textures, also choose random formats that
            // provide interesting results
            texfReq.Attrs  = Is2d;
            texfReq.Format = m_Pickers[RND_PIXEL_3D_TEX_FORMAT].Pick();
        } while ((retry++ < 10) && (m_Txf.count(texfReq)));
        m_Txf.insert(texfReq);
    }

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Pick a new set of random values
void RndPixels::Pick()
{
    // Zero the pixel info only up to the CRCBoundary so that the CRCs are
    // consistent.  Do not zero past the boundary to avoid killing the static
    // pixel data pointer for normal pixels
    memset(&m_Info, 0, offsetof(PixelInfo,CRCBoundary));

    m_Info.Type = m_pGLRandom->m_Misc.DrawAction();

    // Common picks used in both types of pixels
    m_Info.RectHeight       = m_Pickers[RND_PIXEL_HEIGHT].Pick();
    m_Info.RectWidth        = m_Pickers[RND_PIXEL_WIDTH].Pick();
    m_Info.PosX             = m_Pickers[RND_PIXEL_POS_X].Pick();
    m_Info.PosY             = m_Pickers[RND_PIXEL_POS_Y].Pick();
    m_Info.PosZ             = m_Pickers[RND_PIXEL_POS_Z].Pick();

    // Find a texture for 3d pixels first
    if (RND_TSTCTRL_DRAW_ACTION_3dpixels == m_Info.Type)
    {
        bool bTextureFound = false;

        // Search for a texture that was requested.  If one is not found then
        // look for any 2D texture (could potentially happen if the format
        // extension is not supported on the current GPU).  If no 2d texture is
        // found, then revert to normal pixels
        vector<GLRandom::TxfReq> txfReq(m_Txf.begin(),m_Txf.end());
        vector<UINT32> txfIndecies;
        for (UINT32 i = 0; i < txfReq.size(); i++)
            txfIndecies.push_back(i);

        m_pFpCtx->Rand.Shuffle(static_cast<UINT32>(txfIndecies.size()),
                               &txfIndecies[0]);

        if (m_pGLRandom->m_GpuPrograms.UseBindlessTextures())
        {
            GLuint texId;
            for (UINT32 i = 0; (i < txfIndecies.size()) && !bTextureFound; i++)
            {
                if (OK == m_pGLRandom->m_Texturing.GetBindlessTextureId(txfReq[txfIndecies[i]],
                                                                        &texId))
                {
                    m_Info.BindlessTexId = texId;
                    bTextureFound = true;
                    break;
                }
            }
            if (!bTextureFound)
            {
                TxfReq twoDReq(Is2d);
                if (OK == m_pGLRandom->m_Texturing.GetBindlessTextureId(twoDReq,
                                                                        &texId))
                {
                    m_Info.BindlessTexId = texId;
                }
                else
                {
                    Printf(Tee::PriLow,
                           "3D pixels requested, but no 2D textures - reverting"
                           " to normal pixels\n");
                    m_Info.Type = RND_TSTCTRL_DRAW_ACTION_pixels;
                }
            }
        }
        else
        {
            for (UINT32 i = 0; (i < txfIndecies.size()) && !bTextureFound; i++)
            {
                if (m_pGLRandom->m_Texturing.AnyMatchingTxObjLoaded(txfReq[txfIndecies[i]]))
                {
                    m_TexfReq = txfReq[txfIndecies[i]];
                    bTextureFound = true;
                    break;
                }
            }

            if (!bTextureFound)
            {
                TxfReq twoDReq(Is2d);
                if (m_pGLRandom->m_Texturing.AnyMatchingTxObjLoaded(twoDReq))
                {
                    m_TexfReq = twoDReq;
                }
                else
                {
                    Printf(Tee::PriLow,
                           "3D pixels requested, but no 2D textures - reverting"
                           " to normal pixels\n");
                    m_Info.Type = RND_TSTCTRL_DRAW_ACTION_pixels;
                }
            }
        }
    }

    if (RND_TSTCTRL_DRAW_ACTION_pixels == m_Info.Type)
    {
        m_Info.DataFormat       = m_Pickers[RND_PIXEL_FMT].Pick();
        m_Info.DataType         = m_Pickers[RND_PIXEL_DATA_TYPE].Pick();
        m_Info.StencilOrColor   = m_Pickers[RND_PIXEL_FMT_STENCIL].Pick();
        m_Info.RGBAOrBGRA       = m_Pickers[RND_PIXEL_FMT_RGBA].Pick();
        m_Info.Alignment        = m_Pickers[RND_PIXEL_DATA_ALIGNMENT].Pick();
        m_Info.FillMode         = m_Pickers[RND_PIXEL_FILL_MODE].Pick();

        // Now that all the Pickers have run we need to make sure
        // the data format is valid for the data type.
        m_Info.DataFormat       = GetValidFormat(m_Info.DataType);

        FillRect();
    }
    else
    {
        m_Info.Texf      =
            m_pFpCtx->Rand.GetRandom(0, m_pGLRandom->NumTxFetchers() - 1);
        m_Info.TexNormS0 = m_Pickers[RND_PIXEL_3D_S0].FPick();
        m_Info.TexNormT0 = m_Pickers[RND_PIXEL_3D_T0].FPick();
        m_Info.TexNormS1 = m_Pickers[RND_PIXEL_3D_S1].FPick();
        m_Info.TexNormT1 = m_Pickers[RND_PIXEL_3D_T1].FPick();
    }

    // Write a single golden crc value for the data upto m_Info.Data
    // plus the actual pixel data.
    m_pGLRandom->LogData(RND_PXL, &m_Info, offsetof(PixelInfo,CRCBoundary));
    if (RND_TSTCTRL_DRAW_ACTION_pixels == m_Info.Type)
    {
        m_pGLRandom->LogData(RND_PXL, m_Info.Data,
                             m_Info.TotDataBytes);
    }
}

//------------------------------------------------------------------------------
//! \brief Send the pixel rectangle to the HW
RC RndPixels::Send()
{
    RC rc = OK;
    // we draw vertexes or pixels, not both
    if (RND_TSTCTRL_DRAW_ACTION_pixels == m_Info.Type)
    {
        glWindowPos3i(m_Info.PosX, m_Info.PosY, m_Info.PosZ);
        CHECK_RC(mglGetRC());

        // Alignment can be on 1,2,4, or 8 byte boundries
        glPixelStorei(GL_UNPACK_ALIGNMENT, m_Info.Alignment);
        glPixelStorei(GL_PACK_ALIGNMENT, m_Info.Alignment);

        glDrawPixels(m_Info.RectWidth,
                     m_Info.RectHeight,
                     m_Info.DataFormat,
                     m_Info.DataType,
                     m_Info.Data);
    }
    else if (RND_TSTCTRL_DRAW_ACTION_3dpixels == m_Info.Type)
    {
        // For 3D pixels the Z value must be normalized.
        float posZ = static_cast<float>(m_Info.PosZ) / (m_BufferDepth[1]-m_BufferDepth[0]);

        GLuint texId;
        if (m_pGLRandom->m_GpuPrograms.UseBindlessTextures())
        {
            texId = m_Info.BindlessTexId;
        }
        else
        {
            CHECK_RC(m_pGLRandom->m_Texturing.GetTextureId(m_Info.Texf,
                                                           &texId));
        }
        glDrawTextureLW(texId,
                        0,
                        static_cast<GLfloat>(m_Info.PosX),
                        static_cast<GLfloat>(m_Info.PosY),
                        static_cast<GLfloat>(m_Info.PosX + m_Info.RectWidth - 1),
                        static_cast<GLfloat>(m_Info.PosY + m_Info.RectHeight - 1),
                        posZ,
                        m_Info.TexNormS0,
                        m_Info.TexNormT0,
                        m_Info.TexNormS1,
                        m_Info.TexNormT1);
    }

    CHECK_RC(mglGetRC());
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Print test specific info.
void RndPixels::Print
(
    Tee::Priority pri   //! the specific Tee priority to use when printing
)
{
    if (RND_TSTCTRL_DRAW_ACTION_pixels == m_Info.Type)
    {
        const char * fillmodes[]  =
        {
            "solid",
            "stripes",
            "random"
        };

        Printf (pri,
                "RndPixels: Type:%-30.30s Fmt:%-20.20s %s Width:%d Height:%d X:%d Y:%d Z:%d\n",
                mglEnumToString(m_Info.DataType),
                mglEnumToString(m_Info.DataFormat),
                fillmodes[m_Info.FillMode],
                m_Info.RectWidth,
                m_Info.RectHeight,
                m_Info.PosX,
                m_Info.PosY,
                m_Info.PosZ);

        //PrintRawBytes(pri);
    }
    else if (RND_TSTCTRL_DRAW_ACTION_3dpixels == m_Info.Type)
    {
        // For 3D pixels the Z value must be normalized.
        float posZ = static_cast<float>(m_Info.PosZ) / (m_BufferDepth[1]-m_BufferDepth[0]);
        Printf (pri,
                "Rnd3dPixels: %s:%d Width:%d Height:%d X:%d Y:%d Z:%.3f S0:%.3f T0:%.3f S1:%.3f T1:%.3f\n",
                m_pGLRandom->m_GpuPrograms.UseBindlessTextures() ? "TxID" : "Txf",
                m_pGLRandom->m_GpuPrograms.UseBindlessTextures() ? m_Info.BindlessTexId : m_Info.Texf,
                m_Info.RectWidth,
                m_Info.RectHeight,
                m_Info.PosX,
                m_Info.PosY,
                posZ,
                m_Info.TexNormS0,
                m_Info.TexNormT0,
                m_Info.TexNormS1,
                m_Info.TexNormT1);
    }
}

//------------------------------------------------------------------------------
//! \brief Print raw pixel data
void RndPixels::PrintRawBytes
(
    Tee::Priority pri   //! the specific Tee priority to use when printing
) const
{
    // output the raw data
    GLubyte *   buf = (GLubyte*)m_Info.Data;
    Printf(pri,"%dx%d Pixel Buffer:\n",m_Info.RectWidth, m_Info.RectHeight);
    for (int row = 0; row < m_Info.RectHeight; row++)
    {
        for (int element = 0; element < m_Info.ElementsPerRow; element++)
        {
            switch (m_Info.ElementSize)
            {
                case 0: Printf(pri,"%2.2x ",*buf);            buf += 1; break;
                case 1: Printf(pri,"%2.2x ",*buf);            buf += 1; break;
                case 2: Printf(pri,"%4.4x ",*(GLushort*)buf); buf += 2; break;
                case 4:
                    if (m_Info.DataType == GL_FLOAT)
                        Printf(pri,"%1.3f ",*(GLfloat*)buf);
                    else
                        Printf(pri,"%8.8x ",*(GLuint*)buf);
                    buf += 4;
                    break;
            }
        }
        Printf(pri,"\n");
        // must be on proper alignment for each new row
        buf -= m_Info.ElementSize;
        buf = (GLubyte*)(((LwUPtr)buf + m_Info.Alignment) & ~(m_Info.Alignment-1));
    }
}

//------------------------------------------------------------------------------
//! \brief
RC RndPixels::Cleanup()
{

    delete [](GLfloat*)m_Info.Data;
    m_Info.Data = NULL;

    return OK;
}

//------------------------------------------------------------------------------
//  What texture-object attributes are needed?
//
// RndTexturing calls this to find out what kind of texture object it must
// bind to this texture-fetcher in each draw.
// If this texture-fetcher index is out of range, or if pixels will not
// read from this fetcher, returns 0.
UINT32 RndPixels::TxAttrNeeded(int txfetcher,GLRandom::TxfReq *pTxfReq)
{
    if ((RND_TSTCTRL_DRAW_ACTION_3dpixels == m_Info.Type) &&
        (txfetcher == (int)m_Info.Texf))
    {
        if (nullptr != pTxfReq)
            *pTxfReq = m_TexfReq;
        return m_TexfReq.Attrs;
    }
    return 0;
}

//------------------------------------------------------------------------------
// Returns true if the specified texture attributes are needed by bindless
// textures for this loop
bool RndPixels::BindlessTxAttrNeeded(UINT32 txAttr)
{
    if (RND_TSTCTRL_DRAW_ACTION_3dpixels == m_Info.Type)
    {
        return !!(txAttr & Is2d);
    }
    return false;
}

//------------------------------------------------------------------------------
// Returns true if the specified texture attributes are referenced by bindless
// textures for this frame
bool RndPixels::BindlessTxAttrRef(UINT32 Attr)
{
    return !!(Attr & Is2d);
}

//------------------------------------------------------------------------------
//! \brief Return a valid format for the given data type
GLenum RndPixels::GetValidFormat
(
    GLenum type //!< the type of data to evaluate
) const
{
    switch (type)
    {
        default:
            Printf(Tee::PriHigh, "Invalid data type:0x%x\n",type);
            return m_Info.DataFormat;

        case GL_BITMAP                     ://!<format must be GL_COLOR_INDEX
            if ( m_pGLRandom->m_Fragment.IsStencilEnabled())//!< or GL_STENCIL_INDEX
                return GL_COLOR_INDEX;
            else
                return m_Info.StencilOrColor;

        case GL_UNSIGNED_BYTE_3_3_2        ://!< must be GL_RGB
        case GL_UNSIGNED_BYTE_2_3_3_REV    ://!< must be GL_RGB
        case GL_UNSIGNED_SHORT_5_6_5       ://!< must be GL_RGB
        case GL_UNSIGNED_SHORT_5_6_5_REV   ://!< must be GL_RGB
            return GL_RGB;

        case GL_UNSIGNED_SHORT_4_4_4_4     ://!< must be GL_RGBA or GL_BGRA
        case GL_UNSIGNED_SHORT_4_4_4_4_REV ://!< must be GL_RGBA or GL_BGRA
        case GL_UNSIGNED_SHORT_5_5_5_1     ://!< must be GL_RGBA or GL_BGRA
        case GL_UNSIGNED_SHORT_1_5_5_5_REV ://!< must be GL_RGBA or GL_BGRA
        case GL_UNSIGNED_INT_8_8_8_8       ://!< must be GL_RGBA or GL_BGRA
        case GL_UNSIGNED_INT_8_8_8_8_REV   ://!< must be GL_RGBA or GL_BGRA
        case GL_UNSIGNED_INT_10_10_10_2    ://!< must be GL_RGBA or GL_BGRA
        case GL_UNSIGNED_INT_2_10_10_10_REV://!< must be GL_RGBA or GL_BGRA
            return m_Info.RGBAOrBGRA;

        case GL_BYTE                       :
        case GL_UNSIGNED_BYTE              :
        case GL_SHORT                      :
        case GL_UNSIGNED_SHORT             :
        case GL_FLOAT                      :
        case GL_INT                        :
        case GL_UNSIGNED_INT               ://!< any fmt is valid
            return m_Info.DataFormat;
    }

}
//------------------------------------------------------------------------------
//! \brief Return the number of RGBA elements per pixel
void RndPixels::GetElementsPerPixel
(
    GLint *pElements //!< the memory location to store the result
)
{
    switch (m_Info.DataType)
    {
        // each bit represnts a pixel so the real value s/b 1/8
        case GL_BITMAP                     :
            *pElements = 0;
            break;
        // each element represents a complete RGBA
        case GL_UNSIGNED_BYTE_3_3_2        :
        case GL_UNSIGNED_BYTE_2_3_3_REV    :
        case GL_UNSIGNED_SHORT_5_6_5       :
        case GL_UNSIGNED_SHORT_5_6_5_REV   :
        case GL_UNSIGNED_SHORT_4_4_4_4     :
        case GL_UNSIGNED_SHORT_4_4_4_4_REV :
        case GL_UNSIGNED_SHORT_5_5_5_1     :
        case GL_UNSIGNED_SHORT_1_5_5_5_REV :
        case GL_UNSIGNED_INT_8_8_8_8       :
        case GL_UNSIGNED_INT_8_8_8_8_REV   :
        case GL_UNSIGNED_INT_10_10_10_2    :
        case GL_UNSIGNED_INT_2_10_10_10_REV:
            *pElements = 1;
            break;

        case GL_BYTE                       :
        case GL_UNSIGNED_BYTE              :
        case GL_SHORT                      :
        case GL_UNSIGNED_SHORT             :
        case GL_FLOAT                      :
        case GL_INT                        :
        case GL_UNSIGNED_INT               :
            switch (m_Info.DataFormat)
            {
                case GL_COLOR_INDEX    :
                case GL_STENCIL_INDEX  :
                case GL_DEPTH_COMPONENT:
                case GL_RED            :
                case GL_GREEN          :
                case GL_BLUE           :
                case GL_ALPHA          :
                case GL_LUMINANCE      : //!< LUMINANCE ->RGB
                    *pElements = 1;
                    break;
                case GL_LUMINANCE_ALPHA: //!< LUMINANCE ->RGB, ALPHA ->ALPHA
                    *pElements = 2;
                    break;
                case GL_RGB            :
                case GL_BGR            :
                    *pElements = 3;
                    break;
                case GL_RGBA           :
                case GL_BGRA           :
                    *pElements = 4;
                    break;
                default:
                    Printf(Tee::PriHigh, "Unrecognized format:0x%x\n",m_Info.DataFormat);
                    *pElements = 0;
            }
            break;
        default:
            // error condition
            Printf(Tee::PriHigh,"Unrecognized type:0x%x\n",m_Info.DataType);
            *pElements = 0;
            break;
    }
    return;
}

//------------------------------------------------------------------------------
//! \brief Return the size (in bytes) of each memory unit for this rectangle
void RndPixels::GetElementSize
(
    GLint * pSize //!< memory location to store the result
)
{
    switch (m_Info.DataType)
    {
        case GL_UNSIGNED_INT_8_8_8_8       :
        case GL_UNSIGNED_INT_8_8_8_8_REV   :
        case GL_UNSIGNED_INT_10_10_10_2    :
        case GL_UNSIGNED_INT_2_10_10_10_REV:
        case GL_FLOAT                      :
        case GL_INT                        :
        case GL_UNSIGNED_INT               :
            *pSize = sizeof(GLint);
            break;
        case GL_UNSIGNED_SHORT_5_6_5       :
        case GL_UNSIGNED_SHORT_5_6_5_REV   :
        case GL_UNSIGNED_SHORT_4_4_4_4     :
        case GL_UNSIGNED_SHORT_4_4_4_4_REV :
        case GL_UNSIGNED_SHORT_5_5_5_1     :
        case GL_UNSIGNED_SHORT_1_5_5_5_REV :
        case GL_SHORT                      :
        case GL_UNSIGNED_SHORT             :
            *pSize = sizeof(GLshort);
            break;
        case GL_UNSIGNED_BYTE_3_3_2        :
        case GL_UNSIGNED_BYTE_2_3_3_REV    :
        case GL_BYTE                       :
        case GL_UNSIGNED_BYTE              :
        case GL_BITMAP                     : //!< special case
            *pSize = sizeof(GLbyte);
            break;
        default                            : //!< should never get here
            *pSize = 0;
    }
}
//------------------------------------------------------------------------------
//! \brief Fill the pixel rectangle with data in the correct format.
void RndPixels::FillRect(void)
{
    GLint       w = m_Info.RectWidth;

    // Unit size can be 1,2,or 4 bytes
    GetElementSize(&m_Info.ElementSize);

    // Units can be 0,1,2,3,or 4
    GetElementsPerPixel(&m_Info.ElementsPerPixel);

    // handle special case GL_BITMAP & numUnits = 0
    if (m_Info.DataType == GL_BITMAP)
        m_Info.ElementsPerRow = (w+7)/8;
    else
        m_Info.ElementsPerRow = m_Info.ElementsPerPixel * w;

    // pre-initialize the data with know values so the crc callwlation
    // will be correct when generating/comparing golden values.
    // Required if the alignment is not 1
    m_Info.TotDataBytes = (m_Info.ElementsPerRow * m_Info.ElementSize +
                           m_Info.Alignment-1) & ~(m_Info.Alignment-1);
    m_Info.TotDataBytes *= m_Info.RectHeight;
    memset(m_Info.Data, 0x80, m_Info.TotDataBytes);

    // Fill the pixel rectangle with random data
    // Note: we use separate pickers for each data type to prevent
    //      too many black pixels.
    switch (m_Info.ElementSize)
    {
        case 1:
            FillRectWithBytes();
            break;
        case 2:
            FillRectWithShorts();
            break;
        case 4:
            if (m_Info.DataType == GL_FLOAT)
                FillRectWithFloats();
            else
                FillRectWithInts();
            break;
        default: //!< should never get here
            MASSERT(0);
            break;
    }
}

//------------------------------------------------------------------------------
//! \brief Fill this rect. with random 8 bit values
void RndPixels::FillRectWithBytes()
{
    GLint       alignment = m_Info.Alignment;
    GLubyte *   buf = (GLubyte*)m_Info.Data;
    GLubyte     value = 0;

    if( m_Info.FillMode == RND_PIXEL_FILL_MODE_solid)
        value = static_cast<GLubyte>(m_Pickers[RND_PIXEL_DATA_BYTE].Pick());

    for (int row = 0; row < m_Info.RectHeight; row++)
    {
        if( m_Info.FillMode == RND_PIXEL_FILL_MODE_stripes)
            value = static_cast<GLubyte>(m_Pickers[RND_PIXEL_DATA_BYTE].Pick());

        for (int element = 0; element < m_Info.ElementsPerRow; element++)
        {
            if( m_Info.FillMode == RND_PIXEL_FILL_MODE_random)
                value = static_cast<GLubyte>(m_Pickers[RND_PIXEL_DATA_BYTE].Pick());

            *buf = value;
            buf++;
        }
        // must be on proper alignment for each new row
        buf = (GLubyte*)(((LwUPtr)buf + alignment-1) & ~(alignment-1));
    }

    MASSERT(m_Info.TotDataBytes == ((LwUPtr)buf - (LwUPtr)m_Info.Data));
}

//------------------------------------------------------------------------------
//! \brief Fill this rect. with random 16 bit values
void RndPixels::FillRectWithShorts()
{
    GLint       alignment = m_Info.Alignment;
    GLushort *  buf = (GLushort*)m_Info.Data;
    GLushort    value = 0;

    if( m_Info.FillMode == RND_PIXEL_FILL_MODE_solid)
        value = static_cast<GLushort>(m_Pickers[RND_PIXEL_DATA_SHORT].Pick());

    for (int row = 0; row < m_Info.RectHeight; row++)
    {
        if( m_Info.FillMode == RND_PIXEL_FILL_MODE_stripes)
            value = static_cast<GLushort>(m_Pickers[RND_PIXEL_DATA_SHORT].Pick());

        for (int element = 0; element < m_Info.ElementsPerRow; element++)
        {
            if( m_Info.FillMode == RND_PIXEL_FILL_MODE_random)
                value = static_cast<GLushort>(m_Pickers[RND_PIXEL_DATA_SHORT].Pick());

            *buf = value;
            buf++;
        }
        // must be on proper alignment for each new row
        buf = (GLushort*)(((LwUPtr)buf + alignment-1) & ~(alignment-1));
    }

    MASSERT(m_Info.TotDataBytes == ((LwUPtr)buf - (LwUPtr)m_Info.Data));
}

//------------------------------------------------------------------------------
//! \brief Fill this rect. with random 32 bit values
void RndPixels::FillRectWithInts()
{
    GLint       alignment = m_Info.Alignment;
    GLuint *    buf = (GLuint*)m_Info.Data;
    GLuint      value = 0;

    if( m_Info.FillMode == RND_PIXEL_FILL_MODE_solid)
        value = static_cast<GLuint>(m_Pickers[RND_PIXEL_DATA_INT].Pick());

    for (int row = 0; row < m_Info.RectHeight; row++)
    {
        if( m_Info.FillMode == RND_PIXEL_FILL_MODE_stripes)
            value = static_cast<GLuint>(m_Pickers[RND_PIXEL_DATA_INT].Pick());

        for (int element = 0; element < m_Info.ElementsPerRow; element++)
        {
            if( m_Info.FillMode == RND_PIXEL_FILL_MODE_random)
                value = static_cast<GLuint>(m_Pickers[RND_PIXEL_DATA_INT].Pick());

            *buf = value;
            buf++;
        }
        // must be on proper alignment for each new row
        buf = (GLuint*)(((LwUPtr)buf + alignment-1) & ~(alignment-1));
    }

    MASSERT(m_Info.TotDataBytes == ((LwUPtr)buf - (LwUPtr)m_Info.Data));
}

//------------------------------------------------------------------------------
//! \brief Fill this rect. with random 32 bit values
void RndPixels::FillRectWithFloats()
{
    GLint       alignment = m_Info.Alignment;
    GLfloat *   buf = (GLfloat*)m_Info.Data;
    GLfloat     value = 0.0f;

    if( m_Info.FillMode == RND_PIXEL_FILL_MODE_solid)
        value = m_Pickers[RND_PIXEL_DATA_FLOAT].FPick();

    for (int row = 0; row < m_Info.RectHeight; row++)
    {
        if( m_Info.FillMode == RND_PIXEL_FILL_MODE_stripes)
            value = m_Pickers[RND_PIXEL_DATA_FLOAT].FPick();

        for (int element = 0; element < m_Info.ElementsPerRow; element++)
        {
            if( m_Info.FillMode == RND_PIXEL_FILL_MODE_random)
                value = m_Pickers[RND_PIXEL_DATA_FLOAT].FPick();

            *buf = value;
            buf++;
        }
        // must be on proper alignment for each new row
        buf = (GLfloat*)(((LwUPtr)buf + alignment-1) & ~(alignment-1));
    }

    MASSERT(m_Info.TotDataBytes == ((LwUPtr)buf - (LwUPtr)m_Info.Data));
}

