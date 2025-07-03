/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _VERIF2D_SURF_H_
#define _VERIF2D_SURF_H_

class Verif2d;
class V2dGpu;

#include <vector>

class LWGpuResource;
class V2dClassObj;

class BuffInfo;

class V2dSurface
{
public:

    void Init( void );    // create the surface in the channel
    // Clear (constant, random, image, file, color ramp, etc.)

    enum ColorFormat
    {
        Y8                    = 0x01,
        X1R5G5B5_Z1R5G5B5     = 0x02,
        X1R5G5B5_O1R5G5B5     = 0x03,
        R5G6B5                = 0x04,
        Y16                   = 0x05,
        X8R8G8B8_Z8R8G8B8     = 0x06,
        X8R8G8B8_O8R8G8B8     = 0x07,
        X1A7R8G8B8_Z1A7R8G8B8 = 0x08,
        X1A7R8G8B8_O1A7R8G8B8 = 0x09,
        A8R8G8B8              = 0x0A,
        Y32                   = 0x0B, //    Color formats up to and including
        X8B8G8R8_Z8B8G8R8     = 0x0C, //    0x0E match lw30_context_surfaces_2d
        X8B8G8R8_O8B8G8R8     = 0x0D, //    enums.  0x0F and beyond must be
        A8B8G8R8              = 0x0E, // <- handled specially.
        CR8YB8CB8YA8          = 0x0F,
        YB8CR8YA8CB8          = 0x10,
        A8CR8CB8Y8            = 0x11,
        A4CR6YB6A4CB6YA6      = 0x12,
        A2R10G10B10           = 0x13,
        A2B10G10R10           = 0x14,
        A1R5G5B5              = 0x15,
        AY8                   = 0x16,
        Y1_8X8                = 0x17,
        A8RL8GL8BL8           = 0x18,
        A8BL8GL8RL8           = 0x19,
        X8RL8GL8BL8           = 0x20,
        X8BL8GL8RL8           = 0x21,
    };

    enum ClearPattern
    {
        XY_COORD_GRID,
        XY_ALL_COLORS,
        XY_ALL_COLORS2,
        CONST,
        GRAY_RAMP_HORZ,
        GRAY_RAMP_VERT,
        GRAY_COARSE_RAMP_VERT,
        GRAY_RAMP_CHECKER,
        YUV_TEST_PATTERN1,
        YUV_TEST_PATTERN2,
        RANDOM,
        BORDER,
        GPU,            // doesn't touch GPU data, copies GPU data to shadow
        ALPHA_CONST,
        ALPHA_RAMP_HORZ,
        ALPHA_RAMP_VERT,
        ALPHA_RANDOM,
        ALPHA_BORDER,
        RGB,
        COLOR_RAMP_HORZ,
        COLOR_RAMP_VERT,
        CORNER_DIFFERENTIAL,
        HORZ_DIFFERENTIAL,
        FONT_TEST_PATTERN,
    };

    enum Status
    {
        NOT_READY,        // parameters set up, surface not yet created
        ERROR,            // error in allocation of surface in the channel
        READY             // surface ready for use
    };

    enum Flags
    {
        LINEAR = 0,       // default
        TILED = 1,
        SWIZZLED = 2,
        COMPRESSED = 4,
    };

    enum ImageDumpFormat
    {
        NONE = 0, // default
        TGA  = 1,
    };

    enum Label
    {
        NO_LABEL = 0,
        LABEL_A = 1,
        LABEL_B = 2
    };

    static int ColorFormatToSize( ColorFormat format );

    // v2d script blocklinear surface:
    V2dSurface(Verif2d *v2d,
               UINT32 width, UINT32 height, UINT32 depth,
               string format, Flags flags,
               UINT64 offset, INT64 limit,
               const string &where, ArgReader *pParams,
               UINT32 log2blockHeight, UINT32 log2blockDepth,
               Label label,
               UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);

    // v2d script pitch surface:
    V2dSurface(Verif2d *v2d,
               UINT32 width, UINT32 height,
               string format, Flags flags,
               UINT64 offset, INT64 limit, UINT32 pitch,
               const string &where, ArgReader *pParams,
               UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);

    // t5d 3d target surface:
    V2dSurface(Verif2d *v2d,
               UINT32 width, UINT32 height, UINT32 depth,
               string format, Flags flags,
               UINT64 offset, UINT32 pitch,
               uintptr_t baseAddr, UINT32 handle, bool preExisting,
               const string &where, ArgReader *pParams,
               UINT32 log2blockHeight, UINT32 log2blockDepth,
               UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);

    ~V2dSurface( void );

    UINT32 GetHandle()        const { return m_dmaBuffer.GetMemHandle(); }
    UINT32 GetCtxDmaHandle()  const { return m_ctxDmaHandle; }
    ColorFormat GetFormat( void ) { return m_colorFormat; }
    string GetFormatString( void ) { return m_colorFormatString; }
    UINT32 GetPitch( void ) { return m_pitch; }
    UINT32 GetWidth( void ) { return m_width; }
    UINT32 GetHeight( void ) { return m_height; }
    UINT32 GetDepth( void ) { return m_depth; }
    UINT32 GetBpp( void ) { return m_bytesPerPixel; }
    UINT64 GetAddress( UINT32 x, UINT32 y, UINT32 z );
    bool GetSwizzled( void ) { return m_swizzled; }
    const string& GetWhere( void ) { return m_where; }
    UINT64 GetOffset( void ) { return m_offset; }
    void SaveToTGAfile( string fileName, UINT32 layer );
    void LoadFromTGAfile( string fileName );
    void Clear( string clearPattern, vector<UINT32> clearData );
    void SetPixel( UINT32 x, UINT32 y, UINT32 z, UINT32 data );
    void SetPixel_8byte( UINT32 x, UINT32 y, UINT32 z,
                         UINT32 data0, UINT32 data1, int granularity );
    void SetPixel_16byte( UINT32 x, UINT32 y, UINT32 z,
                          UINT32 data0, UINT32 data1,
                          UINT32 data2, UINT32 data3 );
    void SetPixel8888( UINT32 x, UINT32 y, UINT32 z,
                             UINT08 a, UINT08 r, UINT08 g, UINT08 b );
    void SetPixelAlpha( UINT32 x, UINT32 y, UINT32 z, UINT08 a );
    UINT32 GetPixel( UINT32 x, UINT32 y, UINT32 z );            // reads real surface
    UINT32 CalcCRC( void );
    UINT32 CalcCrlwsingGpu( void );
    uintptr_t CEReadBeforeCalcCRC( UINT08* pDst );
    Verif2d *GetV2d( void ) { return m_v2d; }
    bool isBlocklinear() const { return (m_blocklinear_map != NULL); }
    unsigned Log2BlockHeightGob() const { return  m_Log2blockHeight; }
    unsigned Log2BlockDepthGob() const { return m_Log2blockDepth; }
    int Granularity();

    void AddToTable(BuffInfo* buffInfo);
    RC ChangePteKind(INT32 pteKind) { return m_dmaBuffer.ChangePteKind(pteKind); }

private:
    LWGpuChannel *m_ch;
    Verif2d *m_v2d;
    V2dGpu *m_gpu;
    MdiagSurf m_dmaBuffer;
    UINT32 m_width;
    UINT32 m_height;
    UINT32 m_depth;
    UINT32 m_pitch;
    UINT64 m_offset;
    INT64  m_limit;
    UINT64 m_sizeInBytes;
    UINT32 m_bytesPerPixel;
    UINT32 m_bl_attr;
    UINT32 m_ctxDmaHandle;
    void* m_mappingAddress;

    ColorFormat m_colorFormat;
    string m_colorFormatString;
    bool m_tiled;
    bool m_swizzled;
    bool m_compressed;
    ArgReader *m_pParams;
    string m_where;
    Status m_status;
    map<const string, LW50Raster*> m_rasters;
    LW50Raster* m_lwrrRaster;

    BlockLinear *m_blocklinear_map;
    unsigned m_Log2blockHeight;
    unsigned m_Log2blockDepth;

    bool m_mapped;
    void MapBuffer();

    Status GetStatus( void ) { return m_status; }

    bool m_preExisting;  // surface was already created and initialized elsewhere.  When v2d
                         // creates a surface object for a preExisting surface, it fills in
                         // the members right from the initiazliation parameters, doesn't
                         // change anything (like adjusting the pitch to an acceptable multiple),
                         // and doesn't set up, e.g., tiled regions

    UINT32 m_subdev;

    Label m_label;

    bool m_cacheMode;
    vector<UINT08> m_cacheBuf;

    void Clear_XY_Coord_Grid( void );
    void Clear_Gray_Ramp( bool vert, bool color, bool alpha, bool coarse = false, UINT32 scale = 1 );
    void Clear_Gray_Ramp_Checker( bool color, bool alpha );
    void Clear_YUV_Test_Pattern( int pattern );
    void Clear_RGB( void );
    void Clear_Color_Ramp(bool vert);
    void Clear_XY_All_Colors( UINT32 size );
    void Clear_XY_All_Colors2( UINT32 size );
    void Clear_Differential(bool corner,UINT08 da, UINT08 dr,UINT08 dg, UINT08 db);
    void Clear_Font_Test_Pattern( UINT32 size );

    static void InitClearPatternMaps( void );
    static map<string,ClearPattern> *m_clearPatternMap;
    void FlushCache();
    void InitCache(ClearPattern& p);

    void DumpRawSurface();

    void ReadSurfaceToNaiveBL(UINT08 *pData);
    void ReadSurfaceToPitch(vector<UINT08>& pitchBuffer, UINT32& pitch, bool chiplibTrace = false);
};

#endif // _VERIF2D_SURF_H_

