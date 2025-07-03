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

//------------------------------------------------------------------------------
/**
 * @file   glrandom.h
 * class GLRandomTest and its helpers.
 *
 * The GLRandom test is an OpenGL based random 3D graphics test.
 */

#ifndef INCLUDED_GLRANDOM_H
#define INCLUDED_GLRANDOM_H
// uncomment this line to create inconsistent programs to check error paths.
//#define CREATE_INCONSISTENT_SHADER 1

#include "opengl/modsgl.h"
#include "core/include/tee.h"
#include "core/include/jscript.h"
#include "core/include/fpicker.h"
#include "gpu/tests/gputest.h"
#include "core/utility/ptrnclss.h" // PatternClass
#include <stdio.h>         // for FILE
#include <deque>
#include <set>
#include <bitset>
#include "lwTrace.h"
#include "opengl/glgpugoldensurfaces.h"
#include "gpu/include/gcximpl.h"
#include "gpu/perf/pwrwait.h"
#include "gpu/include/rppgimpl.h"

#if defined(DEBUG)
    #define GLRANDOM_PICK_LOGGING 1
#endif

// Using low precision PI to get closer cross-platform values
// Bug 523830
#define LOW_PREC_PI 3.14
double LowPrecAtan2(double a, double b);

#define MAX_VIEWPORTS 4  // Divide multiple viewports based test (231) into fixed 4 viewports

namespace GLRandom
{
#include "gpu/js/glr_comm.h"

    ProgProperty& operator++(ProgProperty& p, int);
    ProgProperty& operator--(ProgProperty& p, int);

    // Misc constants:
    enum
    {
        MaxTxCoords      = 8     // lw20+ allow 8 tx coords
        ,MaxTxFetchers   = 32    // lw50+ allows 32 bound textures
        ,MaxTxUnits      = 4     // max shader stages/combiner tx input regs is 4
        ,MaxVxTxFetchers = 32    // lw50+ allows up to 4 bound vertex textures.
        ,MaxPatchVertices= 32    // gf100+, could query GL_MAX_PATCH_VERTICES
        ,MaxImageUnits   = 8     // gf100+, could query GL_MAX_IMAGE_UNITS_EXT
        ,TxFetcherBindless= 32   //
    };
    // Texture coordinates for a gpupgm
    struct TxcReq
    {
        UINT32  TxfMask;    //!< Mask of texture-fetchers using this coord.
        ProgProperty Prop;  //!< which vx/gm/frag property carries the texture coordinate data.

        TxcReq()
        : TxfMask(0),
          Prop(ppIlwalid)
        {
        }
        TxcReq(ProgProperty prop, UINT32 txfMask)
        : TxfMask(txfMask),
          Prop(prop)
        {
        }
    };

    enum ImageUnitAccess    //!< texture image r/w access
    {
        iuaNone      = 0,   //!< access is unknown at this time
        iuaRead      = 1,   //!< rsvd for read access
        iuaWrite     = 2,   //!< rsvd for write access
        iuaReadWrite = 3    //!< rsvd for read/write access
    };

    enum TextureViewState
    {
        IsDefault      = 0,  //!< Default texture(not sparse, not a view, not using a view)
        IsUsingView    = 1,  //!< this texure is uing a view (does not have its own storage)
        IsView         = 2,  //!< this texture can be used as view, shares storage with sparse texture
        IsSparse       = 3   //!< this is a sparse texture
    };

    struct ImageUnit
    {
        int unit;       //!< which of the 8 image units to use.
        int mmLevel;    //!< the level of detail to access.
        int access;     //!< 0=none, 1=read, 2=write, 3=readwrite
        int elems;      //!< the required number or RGBA elements (1-4)
        ImageUnit() :
            unit(-1), mmLevel(-1), access(iuaRead),elems(0) {}
        ImageUnit( int Unit, int Level, int Access, int Elems) :
            unit(Unit),mmLevel(Level),access(Access),elems(Elems) {}
        bool operator==( const ImageUnit &rhs) const
        {
            return (unit == rhs.unit && elems == rhs.elems &&
                    access == rhs.access);
        }
        bool operator!=( const ImageUnit &rhs) const
        {
            return (unit != rhs.unit || elems != rhs.elems ||
                    access != rhs.access);
        }
        bool operator<(const ImageUnit &rhs) const
        {
            if (unit != rhs.unit)
                return (unit < rhs.unit);
            if (access != rhs.access)
                return (access < rhs.access);
            return (elems < rhs.elems);
        }
    };

    //! Texture Fetcher requirements of a gpu program.
    //! ie. Texture bindings that must be made by RndTexturing::Send so
    //! that this program's TEX opcodes work right at runtime.
    //! Vertex, geometry, and fragment programs can read textures.
    //!
    //! For each possible texture-fetcher, a mask of bits from the enum
    //! TexAttribBits.
    struct TxfReq
    {
        unsigned int Attrs;    //!< Mask of TexAttribBits & fetcher state rqmts
        GLenum       Format;   //!< Format required
        ImageUnit IU;

        TxfReq() : Attrs(0), Format(0) {}
        TxfReq(int attrs) : Attrs(attrs), Format(0) {}
        TxfReq(int attrs, GLenum fmt) : Attrs(attrs), Format(fmt) {}
        TxfReq(int attrs, int img, int lev,  int access, int elems) :
            Attrs(attrs), Format(0), IU(img,lev,access,elems){}
        TxfReq(const TxfReq &rhs)
        {
            *this = rhs;
        }
        bool operator==(const TxfReq &rhs) const;
        bool operator!=(const TxfReq &rhs) const
        {
            return !(*this == rhs);
        }
        bool operator <(const TxfReq &rhs) const;
        TxfReq & operator=(const TxfReq &rhs);
    };
    enum InOrOut
    {
        ioIn         = 0x01,
        ioOut        = 0x02
    };

    // Typedefs for using a set<>. Used by RndTexturing & RndGpuPgms
    typedef set<GLRandom::TxfReq>       TxfSet;
    typedef TxfSet::iterator            TxfSetItr;
    typedef TxfSet::const_iterator      ConstTxfSetItr;

    struct PgmRequirements
    {
        //! Container for component-mask (xyzw) indexed by ProgProperty.
        typedef map<ProgProperty, int>      PropMap;
        typedef PropMap::iterator           PropMapItr;
        typedef PropMap::const_iterator     ConstPropMapItr;

        //! Container for texture-coord requirements indexed by PropProperty.
        typedef map<ProgProperty, TxcReq>   TxcMap;
        typedef TxcMap::iterator            TxcMapItr;
        typedef TxcMap::const_iterator      ConstTxcMapItr;

        //! Container for texture-fetcher requirements indexed by fetcher id.
        typedef map<int, TxfReq>            TxfMap;
        typedef TxfMap::iterator            TxfMapItr;
        typedef TxfMap::const_iterator      ConstTxfMapItr;

        PropMap Inputs;     //!< vertex/geometry/fragment properties read.

        PropMap Outputs;    //!< vertex/geometry/fragment properties written.

        TxcMap  InTxc;      //!< Properties used as input texture coordinates.

        TxcMap  OutTxc;     //!< Properties used as output texture coordinates.

        TxfMap  UsedTxf;    //!< Texture fetchers used by this program.

        TxfMap  RsvdTxf;    //!< Texture fetchers used by downstream programs.

        vector<string>  UsrComments;        //!< Unique comments to add to this program

        INT32 PrimitiveIn;  //!< Expected input primitive (Geometry/Tessellation only)
        int PrimitiveOut;   //!< Output primitive (Geometry/Tessellation only)
        int VxPerPatchIn;   //!< Expected input vertices per patch (tessellation)
        int VxPerPatchOut;  //!< Output vertices per patch (tessellation)
        float VxMultiplier; //!< Output vertices per input vertex (Geometry)
        bool NeedSbos;      //!< if true we need ShaderBufferObject support
        unsigned BindlessTxAttr;//!< bitmap of texture attributes used in bindless
                                //! texture instructions.
        int EndStrategy;   //! End strategy used in fragment shaders
        PgmRequirements()
        :   PrimitiveIn(-1)
            ,PrimitiveOut(-1)
            ,VxPerPatchIn(-1)
            ,VxPerPatchOut(-1)
            ,VxMultiplier(1.0F)
            ,NeedSbos(false)
            ,BindlessTxAttr(0)
            ,EndStrategy(-1)
        {
        }

        // return true if any fetcher using this texture coordinate is bound to the txAttr texture target.
        bool TxcRequires (ProgProperty prop, UINT32 txAttr, InOrOut io);
        void Print (Tee::Priority pri) const;
        void Print(Tee::PriDebugStub) const
        {
            Print(Tee::PriSecret);
        }
        string ToJsString () const;
        string PrimToJsString () const;
    };

    // Symbolic names for vertex inputs.
    enum VxAttribute
    {
        att_OPOS,      // object-space position (before modelview matrix application)
        att_1,         // was vertex "weight" (x only), now an extra.
        att_NRML,      // object-space normal for lighting
        att_COL0,      // primary color (or material)
        att_COL1,      // secondary color (alpha (w) forced to 0)
        att_FOGC,      // explicit fog coord (x only)
        att_6,         // no "old T&L" synonym.  depends on vertex program
        att_7,         // ...
        att_TEX0,      // texture unit 0 coord before tx matrix
        att_TEX1,      // ...
        att_TEX2,
        att_TEX3,
        att_TEX4,
        att_TEX5,
        att_TEX6,
        att_TEX7,      // == 15, last HW attribute
        att_EdgeFlag,  // == 16, edge flag is a special case

        att_NUM_ATTRIBUTES = 16,
        att_NUM_ATTRIBUTES_COUNTING_EDGE_FLAG = 17
    };

    struct Program
    {
        // Runtime enabled features on a per program basis. These bits cause additional OPTION
        // statements to be inserted in the OptionSequence().
        enum GLFeatures {
            Multisample,
            Fp16Atomics,
            Fp64,
            Fp64Atomics,
            I64Atomics,
            Fp32Atomics,
            ConservativeRaster,
            // add new features above this line
            NumGLFeatures
        };

        Program()
        {
            Init();
            Col0Alias = att_COL0;
            EndOfChecksummedArea = 0;
            Id = 0;
        }

        Program(const char * str, const PgmRequirements & rqmts)
        {
            Init();
            Col0Alias           = att_COL0;
            EndOfChecksummedArea= 0;
            Id                  = 0;
            PgmRqmt             = rqmts;
            Pgm                 = str;
        }

        Program(const Program& p)
        {
            Init();
            Target          = p.Target;
            UsingGLFeature  = p.UsingGLFeature;
            NumInstr        = p.NumInstr;
            SubArrayLen     = p.SubArrayLen;
            TessMode        = p.TessMode;
            Col0Alias       = p.Col0Alias;
            XfbStdAttrib    = p.XfbStdAttrib;
            XfbUsrAttrib    = p.XfbUsrAttrib;
            EndOfChecksummedArea = p.EndOfChecksummedArea;
            Id              = p.Id;
            PgmRqmt         = p.PgmRqmt;
            Pgm             = p.Pgm;
            Name            = p.Name;
        }

        // This takes into account any unpacked bytes that may fall inside the
        // checksummed data.
        void Init()
        {
            memset(this, 0, ((char*)&this->EndOfChecksummedArea - (char*)this));
        }
        // format version of this struct. If anything changes increment the
        // version number       Change
        // 0                    None
        // 1                    added PgmRqmt.NeedSbos
        // 2                    added PgmRqmt.BindlessTxAttr
        // 3                    moved SboAddr to program.local
        // 4                    moved SboTxAddrReg to program.local
        // 5                    added PgmRqmt.VxMultiplier
        enum { JSIFC_VERSION = 5};

        GLuint      Target;         //!< GL_FRAGMENT_PROGRAM_LW,GL_GEOMETRY_PROGRAM_LW, etc.
        bitset<NumGLFeatures> UsingGLFeature; // initializes to all zeros
        int         NumInstr;       //!< num. instructions(if randomly generated) or 0
        int         SubArrayLen;    //!< length of CALI subroutine array (if any)
        GLenum      TessMode;       //!< tessellation mode for tess-eval progs

        //! For full coverage of vertex programs, we get material color from
        //! various attributes: always att_COL0 if vertex programs not
        //! enabled, else one of 1,COL0,FOGC,NRML,6,7,TEX4,TEX5,TEX6,TEX7
        VxAttribute Col0Alias;

        //! XFB vertex attributes that can be streamed
        UINT32      XfbStdAttrib;   //!< bitmap of vertex program Std outputs
        UINT32      XfbUsrAttrib;   //!< bitmap of vertex program Generic outputs

        // Parts of the object above here are checksummed and must exactly match
        // run-to-run in debug builds.  Anything that has pointers or IDs etc
        // must be below here.
        char        EndOfChecksummedArea; //!< Unused except as placeholder.

        GLuint      Id;             //!< OpenGL program-object id, from glGenProgramsLW.
        PgmRequirements PgmRqmt;    //!< input/output requirements for this program
        string      Pgm;            //!< the program source code (empty if dummy)
        string      Name;           //!< name for printouts

        //! Print this program, with optional error-location marked.
        void Print(Tee::Priority pri, int errOffset = -1) const;
        void Print(Tee::PriDebugStub, int errOffset = -1) const
        {
            Print(Tee::PriSecret, errOffset);
        }
        void PrintToFile () const;
        void PrintToJsFile () const;
        string ToJsString () const;
        string QuotedJsText () const;
        bool IsDummy() const;
        bool IsTwoSided() const;
        bool ReadsPointSize() const;
        void CalcXfbAttributes();   //!< calc Xfb Attributes based on PgmRqmt.Outputs[]
    };

    // structure describing a special Shader buffer object.
    struct SboState
    {
        ColorUtils::Format colorFmt;// should be VOID32
        GLuint      Id;             //OpenGL ID for this Sbo
        GLuint64    GpuAddr;        //GPU address
        GLint       Rect[4];        //x,y,width,height

        SboState() : colorFmt(ColorUtils::VOID32), Id(0), GpuAddr(0) {}
    };

    enum SboAttribute
    {
        sboOffsetX  = 0, // indexes into the SboState.Rect[]
        sboOffsetY  = 1,
        sboWidth    = 2,
        sboHeight   = 3,
    };

    string TxAttrToString (UINT32 TxAttr);
    INT32  MapTxAttrToTxDim (UINT32 Attr);

}; // namespace GLRandom

class GLRandomTest;
class GLGoldenSurfaces;
class GpuPgmGen;
class Programs;

//------------------------------------------------------------------------------
// GLRandomHelper: base class for the GLRandom helper classes.
//
class GLRandomHelper
{
public:
    GLRandomHelper(GLRandomTest * pglr, int numPickers, const char * name);
    virtual ~GLRandomHelper();

    // FancyPicker utilities, non-virtual, implemented in RndHelper.
    RC    PickerFromJsval(int index, jsval value);
    RC    PickerToJsval(int index, jsval * pvalue);
    RC    PickToJsval(int index, jsval * pvalue);
    void  CheckInitialized();
    virtual void  SetContext(FancyPicker::FpContext * pFpCtx);

    const char * GetName() const;

    // Standard functions that all the derived classes may (or must) override:
    virtual RC   SetDefaultPicker(int picker) = 0; // set FancyPickers to default values
    virtual RC   InitOpenGL();                              // Check library/HW capabilities, etc.
    virtual RC   Restart();                                 // Do once-per-restart picks, prints, & sends.
    virtual void Pick() = 0;                                // Pick new random state.
    virtual void Print(Tee::Priority p)= 0;                 // Print current picks to screen & logfile.
    virtual RC   Send() = 0;                                // Send current picks to library/HW.
    virtual RC   Playback();                                // Playback capured vertices from previous send.
    virtual RC   UnPlayback();                              // Turn off GL modes turned on by Playback
    virtual RC   CleanupGLState();                          // Perform any per frame cleanup required
    virtual RC   Cleanup() = 0;                             // Free all allocated resources.

protected:
    FancyPickerArray           m_Pickers;
    FancyPicker::FpContext *   m_pFpCtx;
    const char *               m_Name;
    GLRandomTest *             m_pGLRandom;
};

//------------------------------------------------------------------------------
// GL state randomizer for misc. stuff.
//
class RndMisc : public GLRandomHelper
{
public:
    RndMisc(GLRandomTest * pglr);
    virtual ~RndMisc();
    virtual RC   SetDefaultPicker(int picker);
    virtual RC   InitOpenGL();
    virtual void Pick();
    virtual void Print(Tee::Priority p);
    virtual RC   Send();
    virtual RC   Restart();
    virtual RC   Cleanup();

    // This helper is funny in that it must pick some things _before_ all other
    // helpers, then other _after_ all others.
    // The actual Pick() routine is empty for this class.
    void PickFirst();
    void PickLast();

    // Get the current picks.
    UINT32   LogMode() const;
    UINT32   SkipMask() const;              // things to skip on every loop
    UINT32   RestartSkipMask() const;       // things to skip during restart
    UINT32   TraceAction() const;
    UINT32   RestartTraceAction() const;
    bool     Stop() const;
    bool     SuperVerbose() const;
    UINT32   DrawAction() const; //!< returns whether drawing vertices, pixels,
                                 //!< or 3d pixels

    bool     CountPixels() const;
    bool     Finish() const;
    const GLfloat * ClearColor();

    bool    XfbEnabled(UINT32 mode) const;  //!< returns true if XFB capture/playback has been enabled.
    UINT32  GetXfbMode() const;             //!< returns the picked XFB mode.

    bool    XfbIsCapturePossible() const;
    void    XfbCaptureNotPossible();        // Clears the internal flag indicating if XFB capture is possible

    bool    XfbIsPlaybackPossible() const;  // returns the cooperative flag to indicate if it is possible for
                                            // XFB to playback vertex attributes and yield consistent framebuffer
                                            // CRC values.
    void    XfbPlaybackNotPossible();
    void    XfbDisablePlayback();           // Disables the XFB playback process for the current loop.
                                            // If disabled, the original vertices will NOT be discarded
                                            // during the capture phase.
    UINT32 GetPwrWaitDelayMs() const;       // returns the random pickers values for how long to wait
                                            // for a GC5/GC6 enty.

private:
    GLfloat  m_ClearColor[4];

    // Flag to determine enable/disable XFB
    UINT32    m_XfbMode;

    // Cooperative flag used by all RndHelpers to determine if the
    // XFB's playback feature will generate consistent results between
    // golden and std runs for any given loop. The flag is defaulted to
    // the m_XFBEnabled state at the start of each loop.
    bool    m_XfbPlaybackPossible;

    // Cooperative flag to indicate if XFB capture is even possible for this
    // loop. Note: If capture is not possible then playback will not be consistent.
    bool    m_XfbCapturePossible;

    // Flag to indicate whether 3d pixels are supported
    bool    m_3dPixelsSupported;

};

//------------------------------------------------------------------------------
// GLRandom helper for  vertex data format.
//
// Picks what data we will send for each vertex.
//
// We consider a vertex to have up to 16 attributes, with each
// attribute being an array of 4 GLfloat.
// Some of these are only used as inputs to custom vertex programs,
// but others correspond to "traditional" OpenGL vertex data.
// If we are using traditional OpenGL rendering modes, we might have
// data formats other than GLfloat[4], such as GLubyte[3] or GLshort[2].
//
// This randomizer needs to Pick() fairly late, since it depends on
// the picks of several other randomizers.
//
class RndVertexFormat : public GLRandomHelper
{
public:
    RndVertexFormat(GLRandomTest * pglr);
    virtual ~RndVertexFormat();

    virtual RC   SetDefaultPicker(int picker);
    virtual void Pick();
    virtual void Print(Tee::Priority p);
    virtual RC   Send();
    virtual RC   Playback();
    virtual RC   Cleanup();

    int      Size(GLRandom::VxAttribute);  // 0 if attr disabled, 1,2,3,4 if enabled.
    GLenum   Type(GLRandom::VxAttribute);  // GL_FLOAT usually
    GLenum   IndexType();                  // for LW_element_array, vx index ushort or uint.

    // Equivalent to Size((VxAttribute)(att_TEX0 + txu)):
    GLint    TxSize(GLint txu);
    // Equivalent to Type((VxAttribute)(att_TEX0 + txu)):
    GLint    TxType(GLint txu);

private:
    GLint  m_Size[GLRandom::att_NUM_ATTRIBUTES_COUNTING_EDGE_FLAG];
    GLenum m_Type[GLRandom::att_NUM_ATTRIBUTES_COUNTING_EDGE_FLAG];
    GLenum m_IndexType;
};

//------------------------------------------------------------------------------
// GL state randomizer for texture unit settings.
//
// Generates texture data, loads textures, and programs texture units.
//
// Does not control texture coord generation (texgen modes are in RndGeometry,
// vertex tx coords are set in RndVertexes or RndBezierPatch).
//
// This randomizer needs to Pick very early, as other randomizers need
// to look up its texture coord requirements later.
//
class RndTexturing : public GLRandomHelper
{
public:
    RndTexturing(GLRandomTest * pglr);
    virtual ~RndTexturing();

    // Standard GL state randomizer functions:
    virtual RC   SetDefaultPicker(int picker);
    virtual RC   InitOpenGL();
    virtual RC   Restart();
    virtual void Pick();
    virtual void Print(Tee::Priority p);
    virtual RC   Send();
    virtual RC   CleanupGLState();
    virtual RC   Cleanup();
    virtual RC   ReportCoverage(GLenum fmt);

    //  Do any of the loaded texture objects meet these requirements?
    //
    // Returns true if any of the loaded texture objects have all the
    // texture-attribute bits in the mask rqdTxAttrs.
    // Used by the vx/fr program picker to make sure we will be able to
    // bind a texture later.
    bool AnyMatchingTxObjLoaded(const GLRandom::TxfReq & rqdTxAttrs);

    //  How many of s,t,r,q are needed by this texture unit.
    //
    // Called by RndPrograms when vertex and fragment programs are disabled.
    int TxuComponentsNeeded(int txu);

    //  Get the S,T,R coord scaling for this texture unit.
    //
    // Always 1.0 except for multisampled, 1D/2D/LWBEMAP array textures
    void ScaleSTRQ(int txu,
                    GLfloat * pScaleS,
                    GLfloat * pScaleT,
                    GLfloat * pScaleR,
                    GLfloat * pScaleQ
                    );

    // Set the filename for the .TGA file to load for texturing.
    void SetTgaTexImageFileName (string fname);

    // Set the max bit-depth of float textures that is filterable. (default 16)
    void SetMaxFilterableFloatDepth(int depth);

    // Pre-Tesla parts could only specify texture-border-color in BGRA8
    // format, which GL expands over the 0..1 range for float textures.
    // Border colors for float textures outside that range fall back to SW
    // rasterizer.
    // Tesla+ have rgbaF32 in hardware.
    void SetHasF32BorderColor (bool isTeslaPlus);

    // Do a checksum on the textureImage layers that were modified.
    UINT32 CheckTextureImageLayers();

    // Called by RndGpuPrograms to get the gpuAddr of the SBO that holds all of the
    // bindless texture handles.
    GLRandom::SboState * GetBindlessTextureState();

    enum
    {
        txWidth  = 0
        ,txHeight = 1
        ,txDepth  = 2
        ,txMaxDims = 3
    };

    struct TextureObjectState
    {
        GLint          Index;            // index of this texture in m_Textures.
        GLenum         Dim;              // GL_TEXTURE_2D or GL_TEXTURE_LWBE_MAP_LW, etc.
        GLuint         BaseSize[txMaxDims];// Size of one face/layer of miplevel 0 of texture.
        GLuint         NumDims;          // 1 for 1d and 1d-array, 2 for 2d/lwbe/2d-array,2d-lwbe, 3 for 3d
        GLuint         SparseLayers;     // num layers in the sparse texture
        GLuint         Layers;           // num layers in array tex, 0 otherwise
        GLuint         Faces;            // 6 for lwbe/array-lwbe, 1 otherwise
        GLuint         BaseMM;           // base mipmap level to use (highest res)
        GLuint         MaxMM;            // max mipmap level to use (lowest res)
        GLint          PatternCode;      // what image we loaded
        GLenum         InternalFormat;   // how the driver should store the texture in memory: RGB, A, RGBA, compression, bits per color, etc.
        GLenum         DepthTexMode;     // use depth as luminance/alpha/intensity
        GLboolean      IsNP2;
        GLboolean      HasBorder;        // texture data includes 1 texel border.
                                         // TODO : Remove this.  Redundant with the IsNoBorder bit in Attribs
        GLint          TextureType;      // Default/sparse/view/using view/
        ColorUtils::Format HwFmt;        // Hardware color format (color.h)

        // Based on InternalFormat, how to transfer texture to driver:
        GLenum         LoadFormat;       //  what order components are sent (i.e. bgra vs rgba)
        GLenum         LoadType;         //  data types (8bit vs. 16 bit, etc)
        GLenum         LoadBpt;          //  bytes per texel

        GLuint         LoadMinMM;        // Min MM level loaded to driver. (not zero in case of sparse textures)
        GLuint         LoadMaxMM;        // Max MM level loaded to driver.

                                         // Based on InternalFormat and Name, how can this texture be used?
        GLuint         Attribs;          // See the IsXXX masks above.

        GLuint         Id;               // GL texture object handle Id
        GLuint64       Handle;           // GL 64bit texture object handle from glGetTextureHandleLW()

        // Multisample specific handles
        GLuint         MsFboId;         // Framebuffer object handle
        GLuint         MsColorRboId;    // Renderbuffer object handle for color
        GLint          MsSamples;       // Number of samples in this texture image

        // Image unit specifics
        GLuint         DirtyLevelMask;  // Bitmap of mipmap levels that will be
                                        // written by an Image Unit.
        GLint          RsvdAccess;      // The type of access this texture was
                                        // created for.
        GLint          Access;          // How this textue is being accessed.
        GLint          SparseTexID;     // Which sparse texture is this texture using as view.
    };

    // Get the id of the texture bound to the specified fetcher.
    RC GetTextureId(int txfetcher, GLuint *pId);

    // Get the id of bindless texure that meets the requirements.
    RC GetBindlessTextureId(const GLRandom::TxfReq &txfReq, GLuint *pId);

    class TxFill
    {
    public:
        TxFill () {}
        virtual ~TxFill() {}

        virtual void Fill
        (
            GLubyte * pBuf,
            GLuint    w,
            GLuint    h
        ) = 0;
    };

private:
    struct TextureFetcherState
    {
        GLint     BoundTexture;          // index into m_Textures[]; -1 if fetcher disabled
        GLboolean Enable;                // do the traditional enable (false for frag-prog bindings)
        GLenum    Wrap[3];               // clamp(uses border), clamp to edge, repeat
        GLenum    MinFilter;             // nearest, linear, (X mmap nearest, linear)
        GLenum    MagFilter;             // nearest, linear
        GLenum    CompareMode;           // perform shadow comparison
        GLenum    ShadowFunc;            // shadow comparison function
        GLenum    ReductionMode;         // how to combine filtered texels

        GLfloat   MinLambda;             // limit mipmap chooser/interpolator factor
        GLfloat   MaxLambda;
        GLfloat   BiasLambda;            // adjust mipmap chooser/interpolator factor
        GLfloat   BorderValues[4];
        GLfloat   MaxAnisotropy;         // 0.0 to GL_MAX_TEXTURE_ANISOTROPY_LW
        bool      UseSeamlessLwbe;       // AMD_SEAMLESS_LWBEMAP_PER_TEXTURE
    };
    struct TextureUnitState
    {
        TextureFetcherState  Fetchers[GLRandom::MaxTxFetchers];
    };

    // Local storage of the bindless texture handles. Used for quick copy into the SBO.
    // The 1st TiNUM_Indicies*NumTxHandles entries are for random access.
    // This number *must* be less than the number of available fetchers
    GLuint64 m_TxHandles[GLRandom::TiNUM_Indicies*GLRandom::NumTxHandles];

    // RndTexturing private data:

    PatternClass m_PatternClass;

    TextureUnitState  m_Txu;

    string   m_TgaTexImageName;

    TextureObjectState * m_Textures;
    TextureObjectState m_SparseTextures[RND_TX_NUM_SPARSE_TEX];

    UINT32   m_NumTextures;             // Number of loaded textures (valid part of m_Textures).
    UINT32   m_MaxTxBytes;              // Max bytes of tx mem to use this restart.
    UINT32   m_MaxTextures;             // Max number of texture objects this restart (size of m_Textures array)
    int      m_MaxFilterableFloatDepth; // Max bit-depth of float texture that may be filtered
    bool     m_HasF32BorderColor;
    bool     m_UseBindlessTextures = false;     // Use bindless textures on the frame

    GLRandom::SboState  m_TxSbo;        // SBO to store bindless texture object handles.

    // RndTexturing private functions:
    RC   GenerateTextures();
    RC   InitViewTextures();
    RC   CreateSparseTextures();
    void FreeTextures();

    void PickTextureObject (TextureObjectState * pTx, const GLRandom::TxfReq &RqdTxAttrs);
    void PickTextureParameters( TextureFetcherState * pTxf, const TextureObjectState * pTx, bool bIsNoClamp, bool bNeedShadow);
    RC BindImageUnit(GLint txf, TextureObjectState * pTx);
    RC SetTextureParameters( GLint txf, const TextureFetcherState * pTxf, TextureObjectState * pTx );
    RC ShrinkTex (TextureObjectState * pTx);
    RC ShrinkTexToFit (TextureObjectState * pTx, UINT64 txMemoryAvailable,INT32 requiredMMLevel, bool *usingNewTxMemory);
    RC LoadAstcTexture (TextureObjectState * pTx);
    RC LoadTexture (TextureObjectState * pTx);
    RC UseTextureViews (TextureObjectState * pTx);
    UINT32 NumLayersForView (const TextureObjectState * pTx);
    bool CanUseTextureView (const TextureObjectState * pTx, UINT32 viewID);
    TxFill * TxFillFactory (TextureObjectState * pTx);
    void PrintTextureObject (Tee::Priority pri, const RndTexturing::TextureObjectState * pTx,GLRandom::TxfReq rqdTxAttrs,UINT64 txMemoryAvailable,const char * msg);
    void PrintTextureObject (Tee::Priority pri, const RndTexturing::TextureObjectState * pTx) const;
    void PrintTextureObject(Tee::PriDebugStub, const RndTexturing::TextureObjectState *pTx) const
    {
        return PrintTextureObject(Tee::PriSecret, pTx);
    }
    void PrintTextureUnit(Tee::Priority p, int txu) const;
    void PrintBindlessTextures(Tee::Priority pri);

    void PrintTextureData(Tee::Priority pri,
                          const TextureFetcherState * pTxf,
                          const TextureObjectState * pTx) const;
    void PrintTextureImageRaw(GLuint mm,            //! MipMap level to print
                              GLuint w,             //! texture width
                              GLuint h,             //! texture height
                              GLuint d,             //! texture depth
                              GLubyte *pBuf,        //! pointer to the data
                              const TextureObjectState * pTx);

    RC   FillTexLwbe (const TextureObjectState * pTx, GLubyte * pBuf, TxFill * pTxFill);
    RC   FillTexLwbeArray (const TextureObjectState * pTx, GLubyte * pBuf, TxFill * pTxFill);
    RC   FillTex1D   (const TextureObjectState * pTx, GLubyte * pBuf, TxFill * pTxFill);
    RC   FillTex2D   (const TextureObjectState * pTx, GLubyte * pBuf, TxFill * pTxFill);
    RC   FillTex3D   (const TextureObjectState * pTx, GLubyte * pBuf, TxFill * pTxFill);
    void ShrinkTexDimToUnity(const TextureObjectState * pTx, GLuint * mipsize) const;
    bool SkippingTxLoads (const TextureObjectState * pTx, GLuint * mipsize, GLuint mmLevel) const;
    RC   AllocateTexture (const TextureObjectState * pTx);
    void GetFormatProperties(GLenum InternalFormat, GLenum * pLoadFormat, GLenum * pLoadType, GLenum * pLoadBpt, GLuint * pAttribs) const;
    void TexSetDim (TextureObjectState * pTx);

    string       StrPatternCode(GLint patternCode) const;
    const char * StrTxFilter(GLenum filt) const;
    const char * StrTexDim(GLenum dim) const;
    const char * StrTexEdge(GLenum mode) const;
    const char * StrDataType(GLenum t) const;
    const char * StrLwllMode(GLenum c) const;
    const char * StrTexInternalFormat(GLenum ifmt) const;

    RC   RenderCircles(TextureObjectState * pTx);
    RC   RenderSquares(TextureObjectState * pTx);
    RC   RenderPixels(TextureObjectState * pTx, GLuint fillMode);
    RC   RenderDebugFill(TextureObjectState * pTx);
    RC   SetupMsFbo(TextureObjectState * pTx);
    RC   GenerateMsTexture(TextureObjectState * pTx);
    RC   CheckImageUnitRequirements(int txf,TextureObjectState *pTx);
    bool ValidateTexture(int Txf, GLRandom::TxfReq& TxfReq, UINT32 TxIdx);
    bool TextureMatchesRequirements(const GLRandom::TxfReq& TxfReq, UINT32 TxIdx);
    RC   LoadBindlessTextures();
    RC   LoadBindlessTextureHandles();
    RC   UnloadBindlessTextureHandles();
    void GetTxState( UINT32 txHandleIdx, TextureFetcherState ** ppTxf,TextureObjectState ** ppTx);

};

//------------------------------------------------------------------------------
// GLRandom helper for geometry.
//
// Picks the modelview, projection, texture, and skinning matrices.
// Picks vertex programs.
// Picks misc. geometry options like normal scaling and front/back face lwlling.
// Picks the x,y,z bounding box for the current draw, and polygon facing.
// Picks user clip planes.
//
class RndGeometry : public GLRandomHelper
{
public:
    RndGeometry(GLRandomTest * pglr);
    virtual ~RndGeometry();

    virtual RC   SetDefaultPicker(int picker);
    virtual RC   Restart();
    virtual void Pick();
    virtual void Print(Tee::Priority p);
    virtual RC   Send();
    virtual RC   Cleanup();

    // Bounding Box for current draw, in world coordinates.
    struct BBox
    {
        GLint left;
        GLint bottom;
        GLint deep;        // "far" would be a better name, but MSVC reserves that...
        GLint width;
        GLint height;
        GLint depth;
    };

    // Read back current picks:
    void  GetBBox(BBox* p) const;    // get object-space bounding box for current draw.
    bool  NormalizeEnabled() const;  // normal scaling to 1.0 in HW enabled
    bool  TxGenEnabled(int txu) const;// texture coords will be generated by HW
    bool  FaceFront() const;         // true if normals (and polygons) should be drawn front facing
    bool  ForceBigSimple() const;    // if true, generate vx/patch coords that are easy to eyeball-check.

    // true if polygons should be drawn with vx in clockwise order.
    // based on front/back drawing and cw/ccw == front.
    bool DrawClockWise() const;
    bool IsCCW() const;

private:

    // Texture matrixes and coordinate generation (traditional GL, not vertex-program):
    struct TxGeometryStatus
    {
        GLint   GenEnabled;       // if 0, use host-generated per-vertex texture coordinates
        GLenum  GenMode[4];       // obj linear, eye linear, sphere, normal, reflection
        GLfloat GenPlane[4][4];   // plane coords for each tx coord
        GLint   MatrixOperation;
        GLfloat MxRotateDegrees;
        GLfloat MxRotateAxis[3];
        GLfloat MxTranslate[2];
        GLfloat MxScale[2];
    };
    // All our current status info:
    struct GeometryStatus
    {
        // Misc.
        GLboolean   CwIsFront;
        GLboolean   FaceFront;
        GLuint      SpriteEnableMask;
        GLboolean   UsePerspective;    // Use perspective projection (else orthographic)
        GLenum      LwllFace;          // Should front, back, neither, or both faces of poly's be lwlled?
        GLint       Normalize;         // see VertexNormalOptions enum above
        GLboolean   ForceBigSimple;    // Should vertexe & patch coords be chosen to be easy to eyeball-check?
        GLuint      UserClipPlaneMask; // Which user clip planes to enable.
        GLint       ModelViewOperation;

        // Bounding box
        GLint Ystart;            // starting Y position for this screen draw
        GLint Ydir;              // filling from bottom to top (1) or top to bottom (-1)
        GLint Yoffset;           // how much of worldHeight have we filled so far
        GLint Xstart;            // starting X position for this row of bboxes
        GLint Xdir;              // filling this row from left to right (1) or right to left (-1)
        GLint Xoffset;           // how much of worldWidth have we filled this row so far
        BBox  bbox;              // current picked bbox.

        // Texture matrixes
        TxGeometryStatus Tx[GLRandom::MaxTxCoords];
    };

    GeometryStatus m_Geom;

    // private functions:

    void ResetBBox();
    void NextBBox();
    void PrintBBox(Tee::Priority pri) const;
    const char * StrTxGenMode(GLenum gm) const;
    const char * StrTxMxOperation(int op) const;
    const char * StrModelViewOperation (GLint op) const;
};

//------------------------------------------------------------------------------
// GLRandom helper for vertex data.
//
// Picks the actual vertex data we will send, the send methods, etc.
//
// This randomizer needs to Pick() very late, since it depends on the picks
// of many other randomizers.
//
class RndVertexes : public GLRandomHelper
{
public:
    RndVertexes(GLRandomTest * pglr);
    virtual ~RndVertexes();

    virtual RC   SetDefaultPicker(int picker);
    virtual RC   InitOpenGL();
    virtual void Pick();
    virtual void Print(Tee::Priority p);
    virtual RC   Send();
    virtual RC   Playback();
    virtual RC   Cleanup();

    // Report what primitive we will be drawing.
    GLenum      Primitive() const;
    void        GetXfbAttrFlags(UINT32 *pXfbStdAttr, UINT32 *pXfbUsrAttr);
    bool        ActivateBubble() {return m_Pickers[RND_VX_ACTIVATE_BUBBLE].GetPick() == GL_TRUE; }

    enum
    {
        MaxVertexes = 300
    };
private:
    enum
    {
        MaxIndexes  = MaxVertexes * 3
        ,MaxCompactedVertexSize = 4*4 +     // opos
                                  3*4 + 4 + // nrml + pad
                                  1*4 + 4 + // fogc + pad
                                  4*4*8 +   // tex0..tex7
                                  4*1 + 4 + // col0 + pad
                                  3*1 + 5 + // col1 + pad
                                  1*1 + 7   // EdgeFlag + pad
        ,MaxXfbPrimitiveSize =   3*(     // max vertexes per primitive
                                  4*4 +   // POSITION
                                  4*4 +   // PRIMARY_COLOR
                                  4*4 +   // SECONDARY_COLOR_LW
                                  4*4 +   // Back PRIMARY_COLOR
                                  4*4 +   // Back SECONDARY_COLOR_LW
                                  1*4 +   // FOG_COORDINATE
                                8*4*4 + // TEXTURE_COORD_LW, tex0..tex7
                                  1*4 +   // VERTEX_ID
                                  1*4 +   // PRIMITIVE_ID_LW
                                  32*4)    // GENERIC_ATTRIB_LW

        ,MaxXfbAttributes = 32   // a realistic number, the max used so far is 12

        // A polygon, triangle-strip, or triangle-fan are the worst case
        // for generating additional vertices during trianglation. For N verts
        // you will get N-2 triangles or 3*(N-2) vertices.
        // Given 900 indexes we will be allocating 851,304 bytes.
        // Note: We havent accounted for Tessellation Shaders yet so just double the
        // size until we can come up with a better formula.
        ,MaxXfbCaptureBytes = (3*(MaxIndexes-2) * MaxXfbPrimitiveSize)*3
    };

    // Vertex data
    // Our internal copy.  This gets copied to a randomly determined data layout
    // before being used for vertex arrays (see VertexLayout below).
    //
    // This structure is 4-byte aligned.
    //
    struct Vertex
    {
        GLfloat   Coords[4];                // vertex x,y,z,w coords
        GLfloat   Normal[3];                // normal x,y,z coords
        GLfloat   FogCoord;                 // Fog coord
        GLfloat   Tx[GLRandom::MaxTxCoords][4];     // texture s,t,r,q coords (per texture coord set)
        GLubyte   Color[4];                 // vertex primary color (material color if lighing enabled)
        GLubyte   SecColor[3];              // vertex secondary color (not used if lighting enabled)
        GLboolean EdgeFlag;                 // edge flag for wireframe mode

        GLshort   sCoords[4];               // pre-colwerted to short versions...
        GLshort   sNormal[3];               // (actually, may be half-floats)
        GLubyte   sNormal_pad[2];
    };

    // VertexLayout
    //
    // A randomly determined data layout, for vertex arrays.
    //
    // Some parts of this structure are not random, for example:
    //
    //  (SecColor) Type=GL_UNSIGNED_BYTE, NumElements=3, NumBytes=3, PadBytes=1
    //  (sNormal)  Type=GL_SHORT,         NumElements=3, NumBytes=6, PadBytes=2
    //
    // We use the PadBytes field to make sure all elements start at 4-byte
    // aligned offsets, and that the Stride is always a multiple of 4.
    //
    // Grouping:
    //
    // Each "group" is a data structure.
    //
    // Usually, all the attributes are in group 0, and effectively all we've done
    // is to randomly reorder the Vertex structure and squeeze out the
    // the vertex attributes that aren't sent on this draw.
    //
    // m_LwrrentVertexBuf will contain an array of m_NumVertexes of {group 0},
    // followed by an array of m_NumVertexes of {group 1}, etc.
    //
    // All the vertex attributes in the same group will have the same Stride
    // (since Stride is just sizeof(group)), but have different Offsets based
    // on what order they appear in the group structure.
    //
    struct VertexLayout
    {
        GLenum Type;            // data type
        GLuint NumElements;     // number of elements
        GLuint NumBytes;        // number of bytes
        GLuint PadBytes;        // bytes of padding
        GLuint Group;           // which group this is in (see above)
        GLuint Offset;          // byte offset from beginning of the vertex data buffer
        GLuint Stride;          // byte from vertex n's data to vertex n+1's data
    };

    VertexLayout m_VxLayout[GLRandom::att_NUM_ATTRIBUTES_COUNTING_EDGE_FLAG];
    GLuint       m_VxLayoutOrder[GLRandom::att_NUM_ATTRIBUTES_COUNTING_EDGE_FLAG];
    GLuint       m_CompactedVxSize;

    Vertex * m_Vertexes;          // build the data here

                                  // Later, copy to a vertex array buffer:
    GLubyte * m_LwrrentVertexBuf; // points to the current one in use.

    UINT32    m_PciVertexHandle;
    GLuint    m_VxBufBytes;
    GLubyte * m_PciVertexes;      // copy in PCI (cached) memory

    GLuint  * m_VertexIndexes;    // reordered indexes into vertex arrays
    GLubyte * m_LwrrentIndexBuf;  // copy of (uint or ushort) indexes in pci/agp/fb memory
    GLboolean m_UsePrimRestart;   // do/don't use GL_LW_primitive_restart
    GLsizei   m_InstanceCount;    // for glDrawArraysInstanced or glDrawElementsInstanced
    vector<Vertex> m_DeferredVertexes;

    GLuint      m_Method;               // one of SendDrawMethodOptions, above
    GLenum      m_Primitive;            // what primitive to draw
    GLboolean   m_UseVBO;               // use vertex buffer objects with vertex arrays
    GLuint      m_VertexesPerSend;      // how many vertexes per Send function?
    GLuint      m_NumVertexes;          // how many unique vertexes are in m_Vertexes this loop
    GLuint      m_NumIndexes;           // how many vertexes we will send from m_VertexIndexes (>= m_NumVertexes)

    // This structure defines the data format for declaring how an attribute gets
    // streamed into a vertex buffer object. For each attribute you stream, you must specify
    // a)the attribute (GL_POSITION, GL_PRIMARY_COLOR, TEXTURE_COORD_LW, etc),
    // b)the number of components (1-4) for the attribute and,
    // c)the index if this attribute is indexed ie. texture coordinates are indexed 0-7, otherwise 0.
    //
    struct XfbAttrRec {                   // Transform Feedback Attribute Description Entry
        GLint attrib;                   // enum corresponding to the attribute(GL_POSITION, GL_PRIMARY_COLOR_LW, etc)
        GLint numComponents;            // number of components(1-4) for the attrib, ie. xy, xyz,xyzw, rgba, etc.
        GLint index;                    // index(0-7) for the enumerant corresponding to more than one real attribute
    };
    XfbAttrRec  m_XfbAttribs[MaxXfbAttributes]; // Holds a list of attribute descriptions to stream to a vbo.
    GLuint      m_NumXfbAttribs;           // number of valid attributes in m_TFAEntries[]
    GLboolean   m_XfbPrintRequest;      // Normal printing can't be done with feedback because the information
                                        // is not available until after the Send(). This flag gets set by Print()
                                        // and then cleared by the Send() once the data has been printed.
    Tee::Priority m_XfbPrintPri;        // Printing priority for VBO printing
    GLuint      m_XfbVbo[2];            // vertex buffer objects for Transform Feedback
    GLuint      m_XfbQuery[2];          // query objects for Transform feedback (prims generated/written)
    GLint       m_XfbVerticesWritten;   // number of vertices streamed into the XFB vbo.
    // Private functions:
    RC      AllocateXfbObjects();
    GLenum  GetXfbPrimitive(GLenum primitive) const;
    GLint   GetXfbVertices(GLenum primitive, GLint primsWritten);
    void    SetupXfbPointers();
    RC      SetupXfbAttributes();
    void    PrintVBO( Tee::Priority pri,
                        GLvoid * Data,
                        GLint primSent,
                        GLint primWritten,
                        GLint numAttribs,
                        XfbAttrRec * pAttribs);

    void FillVertexData();
    void PickVertexesForPoints();
    void PickVertexesForLines();
    void PickVertexesForLineLoops();
    void PickVertexesForLineStrips();
    void PickVertexesForTriangles();
    void PickVertexesForTriangleStrips();
    void PickVertexesForTriangleFans();
    void PickVertexesForQuads();
    void PickVertexesForQuadStrips();
    void PickVertexesForPolygons();
    void PickVertexesForPatches();
    void PickVertexesForSolid();
    GLenum AdjustPrimForPgm(GLenum prim);

    void CleanupVertexes();

    bool TriangleIsClockWise(int vx0, int vx1, int vx2);
    void FixFacingBySwappingVx12(int vx0, int vx1, int vx2);
    void FixFacingByTransformingVx2(int vx0, int vx1, int vx2);
    void FixQuadVx3(int vx0, int vx1, int vx2, int vx3);
    void RandomFillTxCoords();
    void GenerateTexCoords(bool IsFirst, GLint vx0, GLint vx1, GLint vx2, GLint vx3 = -1);
    void Generate2dTexCoords(bool IsFirst, GLint txu, GLint vx0, GLint vx1, GLint vx2, GLint vx3 = -1);
    void GenerateColwexPolygon(GLuint * pVx, GLuint lwx, bool isTriangleFan);

    void SetupShuffledIndexArray();
    void SetupInorderIndexArray();
    void PickVertexLayout();
    void UnShuffleVertexes();
    void CopyVertexes(bool bLogGoldelwalues);
    void CopyIndexes();
    void SetupVertexArrays(GLuint vboId);
    void TurnOffVertexArrays();

    void SendDrawBeginEnd(int firstIndex);
    void SndArrayElement (GLint vx);
    void SndV2fv (GLint vx);
    void SndV3fv (GLint vx);
    void SndV4fv (GLint vx);
    void SndV2sv (GLint vx);
    void SndV3sv (GLint vx);
    void SndV4sv (GLint vx);
    void SndV2hv (GLint vx);
    void SndV3hv (GLint vx);
    void SndV4hv (GLint vx);
    void SndV2f  (GLint vx);
    void SndV3f  (GLint vx);
    void SndV4f  (GLint vx);
    void SndV2s  (GLint vx);
    void SndV3s  (GLint vx);
    void SndV4s  (GLint vx);
    void SndV2h  (GLint vx);
    void SndV3h  (GLint vx);
    void SndV4h  (GLint vx);
    void SndN3fv (GLint vx);
    void SndN3sv (GLint vx);
    void SndN3hv (GLint vx);
    void SndN3f  (GLint vx);
    void SndN3s  (GLint vx);
    void SndN3h  (GLint vx);
    void SndC4bv (GLint vx);
    void SndC4b  (GLint vx);
    void SndE1bv (GLint vx);
    void SndE1b  (GLint vx);
    void SndTxcv (GLint vx);
    void SndTxc  (GLint vx);
    void SndSecC3b (GLint vx);
    void SndSecC3bv (GLint vx);
    void SndFogf(GLint vx);
    void SndFogfv(GLint vx);

    const char * StrPrimitive(GLenum prim) const;
    const char * StrSendDrawMethod(GLenum meth) const;
    const char * StrTypeName(GLenum tn) const;
    const char * StrAttrName(GLuint at) const;
    int GetLayerIndex(UINT32 TxAttr) const;
    int GetShadowIndex(UINT32 TxAttr) const;

};

//------------------------------------------------------------------------------
// GLRandom helper for traditional light & material stuff.
//
// When vertex programs are enabled by RndGeometry, this stuff is irrelevant.
//
// This randomizer needs to Pick() after RndTexturing.
//
class RndLighting : public GLRandomHelper
{
public:
    RndLighting(GLRandomTest * pglr);
    virtual ~RndLighting();

    virtual RC   SetDefaultPicker(int picker);
    virtual RC   Restart();
    virtual void Pick();
    virtual void Print(Tee::Priority p);
    virtual RC   Send();
    virtual RC   Cleanup();
    virtual GLboolean IsEnabled() const;
    GLenum       GetShadeModel() const;
    void     CompressLights(bool b);   // Renumber lights to use conselwtive from GL_LIGHT_0.

private:
    struct LightSource
    {
        GLint       CompressedIndex;     // order of enabled lights (for the CompressLights feature)
        GLint       Type;                // directional / pointsource / spotlight
        GLuint      IsBackLight;         // if true, aimed mostly at the back surfaces of polygons
        GLfloat     Vec[4];              // location, or direction for infinate (directional) sources
        GLfloat     ColorDS[4];          // diffuse and spelwlar color
        GLfloat     ColorA[4];           // ambient color
        GLfloat     SpotDirection[4];    // normal vector
        GLint       SpotExponent;        // falloff from center to cutoff
        GLint       SpotLwtoff;          // Degrees, 180 == omnidirectional
        GLfloat     Attenuation[3];      // Atn = A[0] + Distance*A[1] + Distance*Distance*A[2]
    };
    struct LightingState
    {
        GLuint   Enabled;                // Is lighting enabled?
        GLuint   EnabledSources[8];      // Which light sources are turned on?
        GLenum   LocalViewer;            // Should spelwlar callwlate eye normal per-vertex, or globally?
        GLenum   TwoSided;               // light both sides of polygons?
        GLenum   ColorControl;           // delay application of spelwlar color until after texturing?
        GLfloat  GlobalAmbientColor[4];
        GLenum   ShadeModel;             // flat (one color per facet) or smooth (interpolated between vertexes)
        GLenum   ColorMaterial;          // which material color (diffuse/spelwlar/ambient/emission) varies per vertex?
        GLint    Shininess;              // "tightness" of the spelwlar highlights (exponent).
        GLfloat  MaterialColor[4];
        GLfloat  EmissiveMaterialColor[4];
        GLboolean CompressLights;
    };

    LightSource    m_LightSources[8];
    LightingState  m_Lighting;

    // Private functions:

    void PrintLightSource(Tee::Priority pri, int L) const;
    const char * StrMaterial(GLenum mat) const;
};

//------------------------------------------------------------------------------
// GLRandom helper for fog.
//
class RndFog : public GLRandomHelper
{
public:
    RndFog(GLRandomTest * pglr);
    virtual ~RndFog();

    virtual RC   SetDefaultPicker(int picker);
    virtual void Pick();
    virtual void Print(Tee::Priority p);
    virtual RC   Send();
    virtual RC   Cleanup();
    virtual GLboolean   IsEnabled() const;
private:
    struct FogState
    {
        GLint    Enabled;    // 1 or 0
        GLenum   Mode;       // GL_LINEAR, GL_EXP, or GL_EXP2
        GLfloat  Density;    // for EXP and EXP2
        GLfloat  Color[4];   //
        GLfloat  Start;      // eye-space Z distance (>0) for LINEAR
        GLfloat  End;        // eye-space Z distance (>0) for LINEAR
        GLenum   CoordSource;// GL_FOG_COORDINATE/GL_FRAGMENT_Z
        GLenum   DistMode;   // GL_EYE_RADIAL_LW/GL_EYE_PLANE_LW/GL_EYE_PLANE_ABSOLUTE_LW
    };

    FogState m_Fog;

    const char * StrFogMode(GLenum fm) const;
    const char * StrFogCoordSource(GLenum fc) const;
    const char * StrFogDistMode(GLenum fdm) const;
};

//------------------------------------------------------------------------------
// GLRandom helper for poly/line/point parameters.
//
class RndPrimitiveModes : public GLRandomHelper
{
public:
    RndPrimitiveModes(GLRandomTest * pglr);
    virtual ~RndPrimitiveModes();

    virtual RC   SetDefaultPicker(int picker);
    virtual void Pick();
    virtual void Print(Tee::Priority p);
    virtual RC   Send();
    virtual RC   Playback();
    virtual RC   Cleanup();
    bool         LineStippleEnabled() const;
    GLenum       PolygonBackMode() const;
    GLenum       PolygonFrontMode() const;

private:
    struct PolygonStatus
    {
        GLenum    FrontMode;
        GLenum    BackMode;
        GLenum    Smooth;
        GLenum    Stipple;
        GLboolean OffsetFill;
        GLboolean OffsetLine;
        GLboolean OffsetPoint;
        GLfloat   OffsetFactor;
        GLfloat   OffsetUnits;
    };
    struct LineStatus
    {
        GLfloat  Width;
        GLenum   Smooth;
        GLenum   Stipple;
        GLushort StippleRepeat;
    };
    struct PointStatus
    {
        GLfloat  Size;
        GLenum   Sprite;
        GLenum   SpriteRMode;
        GLfloat  MinSize;
        GLfloat  FadeThreshold;
        GLboolean EnableAttenuation;
    };

    GLubyte        m_StippleBuf[32*32 / 8];
    PolygonStatus  m_Polygon;
    LineStatus     m_Lines;
    PointStatus    m_Points;

    void PickBitmap(GLubyte * buf, GLint w, GLint h);
};

//------------------------------------------------------------------------------
// GLRandom helper for fragment ops: stencil, z, alpha, blend, etc.
//
class RndFragment : public GLRandomHelper
{
public:
    RndFragment(GLRandomTest * pglr);
    virtual ~RndFragment();

    virtual RC   SetDefaultPicker(int picker);
    virtual RC   InitOpenGL();
    virtual RC   Restart();
    virtual void Pick();
    virtual void Print(Tee::Priority p);
    virtual RC   Send();
    virtual RC   CleanupGLState();
    virtual RC   Cleanup();

    bool IsStencilEnabled();
    // Set the max bit-depth of float surface that is blendable. (default 0)
    void SetMaxBlendableFloatSize(int depth);
    void SetScissorOverride(GLint x, GLint y, GLsizei w, GLsizei h);

    float GetVportXIntersect() const;
    float GetVportYIntersect() const;
    float GetVportSeparation() const;
    void SetFullScreelwiewport();

private:
    struct StencilCtrl
    {
        GLenum Func;
        GLenum OpFail;
        GLenum OpZFail;
        GLenum OpZPass;
        GLint  ReadMask;
        GLint  Ref;
        GLuint WriteMask;
    };
    //! Picks that can vary per color-buffer.
    struct CBufPicks
    {
        GLboolean   BlendEnabled;
        GLenum      BlendSourceFunc[2];
        GLenum      BlendDestFunc[2];
        GLenum      BlendEquation[2];

        UINT32      ARGBMask;                 // 4-bit mask LSBit == blue-write-enable

        GLboolean   AdvBlendEnabled;
        GLenum      AdvBlendOverlapMode;
        GLenum      AdvBlendPremultipliedSrc;
        GLenum      AdvBlendEquation;
    };
    struct FragmentStatus
    {
        GLenum      AlphaTestFunc;            // what test (0 == disabled)
        GLfloat     AlphaTestRef;

        GLfloat     BlendColor[4];

        GLboolean   DitherEnabled;

        GLenum      LogicOp;                  // what operation (0 == disabled)

        GLenum      DepthFunc;                // what test (0 == disabled)
        GLboolean   DepthMask;                // do/don't write Z if depth test passes
        GLboolean   DepthClamp;               // LW_depth_clamp enabled
        GLboolean   DepthBoundsTest;          // EXT_depth_bounds_test enabled
        GLfloat     DepthBounds[2];           // min,max depth (0,1] if DepthBoundsTest

        GLboolean   StencilEnabled;
        GLboolean   Stencil2Side;
        StencilCtrl Stencil[2];               // if ! EXT_stencil_two_side, use only [0] entry.

        GLboolean   MultisampleEnabled;       // do/don't enable multisampling
        GLboolean   AlphaToCoverageEnabled;   // do/don't enable alpha-to-coverage
        GLboolean   AlphaToOneEnabled;        // do/don't enable alpha-to-one
        GLboolean   SampleCoverageEnabled;    // do/don't enable sample-to-coverage
        GLfloat     SampleCoverage;           // sample coverage fraction
        GLboolean   SampleCoverageIlwert;     // ilwert sample coverage

        GLenum      Scissor;                  // 0 == disabled, 1 == enabled
        GLint       ViewportOffset[2];        // Viewport offset x, y
        GLboolean   RasterDiscard;            // GL_RASTERIZER_DISCARD_LW
        GLboolean   ShadingRateControlEnable; // do/don't enable coarse/fine shading
        GLboolean   ShadingRateImageEnable;   // do/don't use a texture image to control the
                                              // shading rate

    };

    FragmentStatus m_Fragment;
    vector<GLubyte> m_ShadingRates;           //!< texture data for the shading rate image
    CBufPicks*     m_CBufPicks;               //!< For each color surface.

    GLint          m_WindowRect[4];           // The default full-screen viewport.
    int            m_MaxBlendableFloatSize;   // Max bit-depth of float surface that may be blended.

    GLint          m_ScissorOverrideXY[2];    // nonrandom scissor x,y
    GLsizei        m_ScissorOverrideWH[2];    // nonrandom scissor w,h

    float          m_VportXIntersect;   // X Intersection point of 4 viewports in case of multiple viewports
    float          m_VportYIntersect;   // Y Intersection point of 4 viewports in case of multiple viewports
    float          m_VportsSeparation;  // Separation between the 4 viewports
    GLenum         m_VportSwizzleX;     // swizzle/negation on the x coordinate before viewport transformation
    GLenum         m_VportSwizzleY;     // swizzle/negation on the y coordinate before viewport transformation
    GLenum         m_VportSwizzleZ;     // swizzle/negation on the z coordinate before viewport transformation
    GLenum         m_VportSwizzleW;     // swizzle/negation on the w coordinate before viewport transformation

    bool           m_EnableConservativeRaster; // Enable conservative raster
    bool           m_EnableConservativeRasterDilate; // Enable conservative raster dilation
    bool           m_EnableConservativeRasterMode; // Enable conservative raster mode
    float          m_DilateValueNormalized;    // Dilate constant in conservative raster
    GLenum         m_RasterMode;               // Snap mode in conservative raster
    float          m_SubpixelPrecisionBiasX;   // subpixel bias
    float          m_SubpixelPrecisionBiasY;   // subpixel bias

    // Shading rate variables
    GLuint          m_SRTex = 0;

    // private functions:
    void PickAdvancedBlending(UINT32 mrtIdx);
    const char * StrAlphaTestFunc(GLenum atf) const;
    const char * StrBlendFunction(GLenum bf) const;
    const char * StrBlendEquation(GLenum beq) const;
    const char * StrLogicOp(GLenum lop) const;
    const char * StrStencilOp(GLenum sop) const;

    RC   SetMultipleViewports();
    void SetVportSeparation(float separation);
    RC SetupShadingRateTexture();
    void DisplayShadingRateImage(GLuint width, GLuint height);
};

//------------------------------------------------------------------------------
// GLRandom helper for pixel parameters.
//
class RndPixels : public GLRandomHelper
{
public:
    RndPixels
    (
        GLRandomTest * pglr
    );
    virtual ~RndPixels();
    virtual RC   InitOpenGL();

    virtual RC   SetDefaultPicker( int picker);
    virtual RC   Restart();
    virtual void Pick();
    virtual void Print
    (
        Tee::Priority p
    );
    virtual RC   Send();
    virtual RC   Cleanup();

    //------------------------------------------------------------------------------
    //  What texture-object attributes are needed?
    //
    // RndTexturing calls this to find out what kind of texture object it must
    // bind to this texture-fetcher in each draw.
    // If this texture-fetcher index is out of range, or if pixels will not
    // read from this fetcher, returns 0.
    UINT32 TxAttrNeeded(int txfetcher, GLRandom::TxfReq *pTxfReq);
    void   GetAllRefTextureAttribs(GLRandom::TxfSet *pTxfReq) { *pTxfReq = m_Txf; }
    bool   BindlessTxAttrNeeded(UINT32 txAttr);
    bool   BindlessTxAttrRef(UINT32 Attr);

private:
    enum
    {
        MinRectWidth = 8,           //GL_BITMAP represents 8 pixels in 1 byte
        MinRectHeight = 8,
        MaxRectWidth = 64,
        MaxRectHeight = 64,
        MaxDataTypes = 20,
        MaxDataFmts = 13,
        MaxTextures = 4
    };
    //! Enums to use when indexing into an array that represents a window
    //! rectangle.
    //! This should really go someplace more common so other RandomHelpers
    //! can use it.
    enum glWindowRect
    {
        wrPosX  = 0,
        wrPoxY  = 1,
        wrWidth = 2,
        wrHeight= 3
    };

    struct PixelInfo
    {
        UINT32      Type;           //!< type of pixels (3d vs. normal)
        GLenum      DataType;       //!< integral type of each data element
        GLenum      DataFormat;     //!< format of pixel data
        GLenum      StencilOrColor; //!< use GL_STENCIL_INDEX or GL_COLOR_INDEX
        GLenum      RGBAOrBGRA;     //!< use GL_RGBA or GL_BGRA
        GLint       RectWidth;      //!< width of current rectangle
        GLint       RectHeight;     //!< height of current rectangle
        GLuint      PosX;           //!< left edge in window coordinates
        GLuint      PosY;           //!< top edge in window coordinates
        GLuint      PosZ;           //!< buffer depth
        GLint       ElementSize;    //!< size in bytes of each pixel element
        GLint       ElementsPerPixel;//!< number of elements per pixel
        GLint       ElementsPerRow; //!< number of elements per row
        GLint       Alignment;      //!< memory alignment per pixel row.
        GLuint      TotDataBytes;   //!< Bytes sent to glDriver for this rectangle
        GLuint      FillMode;       //!< type of pattern to use when filling the rectangle

        GLuint      Texf;           //!< Texture fetcher to use (3d pixels)
        GLfloat     TexNormS0;      //!< Normalized corner 0 s coordinate of texture sampling rectangle (3d pixels)
        GLfloat     TexNormT0;      //!< Normalized corner 0 t coordinate of texture sampling rectangle (3d pixels)
        GLfloat     TexNormS1;      //!< Normalized corner 1 s coordinate of texture sampling rectangle (3d pixels)
        GLfloat     TexNormT1;      //!< Normalized corner 1 t coordinate of texture sampling rectangle (3d pixels)

        GLubyte     CRCBoundary;    //!< dummy member to avoid alignment issues
                                    //!< on 64-bit platforms during CRC-ing

                                    // member variables from Data on don't get CRC'd
        GLuint      BindlessTexId;  //!< Texture id used with bindless textures (3d pixels)
        GLvoid *    Data;           //!< the pixel data
        GLuint      DrawTime;       //!< Time in usec to complete the glDrawPixels cmd
    };

    GLint            m_WindowRect[4];   //!< The default full-screen viewport.
    GLint            m_BufferDepth[2];  //!< Min/Max values of the depth buffer
    PixelInfo        m_Info;            //!< pixel data for each draw
    GLRandom::TxfSet m_Txf;             //!< Texture fetcher requirements for
                                        //!< 3d pixels (contains from 1 to
                                        //!< MaxTextures entries)
    GLRandom::TxfReq m_TexfReq;         //!< Texture fetcher requirements for
                                        //!< the current loop (placing a struct
                                        //!< with a constructor inside PixelInfo
                                        //!< causes problems)

    void        PrintRawBytes           //!< Print raw pixel data.
                (
                    Tee::Priority pri   //!< Use this priority when printing
                ) const;

    GLenum      GetValidFormat  //!< return a valid format for the given data type
                (
                    GLenum type //!< the type of data to evaluate
                )const;

    void        GetElementsPerPixel //!< Number of RGBA elements per pixel
                (
                    GLint *pUnits   //!< memory location to store the result
                );

    void        GetElementSize   //!< Get the size in bytes of each element
                (
                    GLint *pSize    //!< memory location to store the result
                );

    void        FillRect(void);     //!< Create random pixel data

    // Individual fill routines for speed
    void        FillRectWithBytes();   //!< Fill pixel rect. with 8 bit values
    void        FillRectWithShorts();  //!< Fill pixel rect. with 16 bit values
    void        FillRectWithInts();    //!< Fill pixel rect. with 32 bit values
    void        FillRectWithFloats();  //!< Fill pixel rect. with 32 bit values
};

//------------------------------------------------------------------------------
// GL state randomizer for vertex, geometry and fragment programs.
//
class RndGpuPrograms : public GLRandomHelper
{
public:
    enum XfbStdAttrBits
    {
        Pos         = 0x00000001,   // result.position
        Col0        = 0x00000002,   // result.color.front.primary
        Col1        = 0x00000004,   // result.color.front.secondary
        BkCol0      = 0x00000008,   // result.color.back.primary
        BkCol1      = 0x00000010,   // result.color.back.secondary
        Fog         = 0x00000020,   // result.fogcoord
        PtSz        = 0x00000040,   // result.pointsize
        Nrml        = 0x00000080,   // result.normal
        Tex0        = 0x00000100,   // result.texcoord[0]
        Tex1        = 0x00000200,   // result.texcoord[1]
        Tex2        = 0x00000400,   // result.texcoord[2]
        Tex3        = 0x00000800,   // result.texcoord[3]
        Tex4        = 0x00001000,   // result.texcoord[4]
        Tex5        = 0x00002000,   // result.texcoord[5]
        Tex6        = 0x00004000,   // result.texcoord[6]
        Tex7        = 0x00008000,   // result.texcoord[7]

        AllTex      = 0x0000ff00,   // all of the texcoord[]
        AllStd      = 0x0000ff7f,   // all of the above attributes

        Clp0        = 0x00010000,   // result.clip[0]
        Clp1        = 0x00020000,   // result.clip[1]
        Clp2        = 0x00040000,   // result.clip[2]
        Clp3        = 0x00080000,   // result.clip[3]
        Clp4        = 0x00100000,   // result.clip[4]
        Clp5        = 0x00200000,   // result.clip[5]
        AllClp      = 0x003f0000,   // all of the clip[]

        VertId      = 0x01000000,   // result.vertexid
        PrimId      = 0x02000000,   // result.primid
    };
    enum XfbUsrAttrBits
    {
        Usr0        = 0x00000001,   // result.attrib[0]
        Usr1        = 0x00000002,   // result.attrib[1]
        Usr2        = 0x00000004,   // result.attrib[2]
        Usr3        = 0x00000008,   // result.attrib[3]
        Usr4        = 0x00000010,   // result.attrib[4]
        Usr5        = 0x00000020,   // result.attrib[5]
        Usr6        = 0x00000040,   // result.attrib[6]
        Usr7        = 0x00000080,   // result.attrib[7]
        Usr8        = 0x00000100,   // result.attrib[8]
        Usr9        = 0x00000200,   // result.attrib[9]
        Usr10       = 0x00000400,   // result.attrib[10]
        Usr11       = 0x00000800,   // result.attrib[11]
        Usr12       = 0x00001000,   // result.attrib[12]
        Usr13       = 0x00002000,   // result.attrib[13]
        Usr14       = 0x00004000,   // result.attrib[14]
        Usr15       = 0x00008000,   // result.attrib[15]
        Usr16       = 0x00010000,   // result.attrib[16]
        Usr17       = 0x00020000,   // result.attrib[17]
        Usr18       = 0x00040000,   // result.attrib[18]
        Usr19       = 0x00080000,   // result.attrib[19]
        Usr20       = 0x00100000,   // result.attrib[20]
        Usr21       = 0x00200000,   // result.attrib[21]
        Usr22       = 0x00400000,   // result.attrib[22]
        Usr23       = 0x00800000,   // result.attrib[23]
        Usr24       = 0x01000000,   // result.attrib[24]
        Usr25       = 0x02000000,   // result.attrib[25]
        Usr26       = 0x04000000,   // result.attrib[26]
        Usr27       = 0x08000000,   // result.attrib[27]
        Usr28       = 0x10000000,   // result.attrib[28]
        Usr29       = 0x20000000,   // result.attrib[29]
        Usr30       = 0x40000000,   // result.attrib[30]
        Usr31       = 0x80000000,   // result.attrib[31]
    };

    // constants for the Shader Buffer Objects
    enum
    {
        sboRWElemSize = 32,   // we need 256bits to cover the U64X4 data format.
        sboRead     = 0,
        sboWrite    = 1,
        sboMaxNum   = 2,
    };

    RndGpuPrograms(GLRandomTest * pglr);
    virtual                 ~RndGpuPrograms();
    virtual RC              SetDefaultPicker(int picker);
    virtual RC              InitOpenGL();
    virtual RC              Restart();
    virtual void            Pick();
    virtual void            Print(Tee::Priority p);
    virtual RC              Send();
    virtual RC              Cleanup();
    virtual RC              Playback();
    virtual RC              UnPlayback();

    // Read back current picks:
    bool VxProgEnabled() const;     // true if vertex programs enabled.
    bool TessProgEnabled() const;   // true if tess eval or control program enabled.
    bool GmProgEnabled() const;     // true if geometry programs enabled.
    bool FrProgEnabled() const;     // true if fragment programs enabled.
    int  EndStrategy() const;       // returns the EndStrategy of the current fragment shader
    GLenum ExpectedPrim() const;    // get required input primitive for program.
    int ExpectedVxPerPatch() const; // get #vertices per patch (or 0 w/o tessellation)
    GLenum GetFinalPrimOut() const; // returns the final output primitive type
    float  GetVxMultiplier() const; // returns output vertexes per input vertex

    bool UseBindlessTextures() const; // true if we are using bindless textures this frame.
    //  Get vx attrib index to use for passing in primary color.
    //
    // For full coverage of vertex programs, we get material color from
    // various attributes:
    //  - always att_COL0 if vertex programs not enabled,
    //  - else one of 1,COL0,6,7,TEX4,TEX5,TEX6,TEX7
    GLRandom::VxAttribute Col0Alias() const;

    // Get the XFB attribute flags for the current vertex program.
    void GetXfbAttrFlags( UINT32 *pXfbStdAttr, UINT32 *pXfbUsrAttr);

    void GetSboState( GLRandom::SboState * pSbo, int sbo);

    //------------------------------------------------------------------------------
    //  What texture-object attributes are needed?
    //
    // RndTexturing calls this to find out what kind of texture object it must
    // bind to this texture-fetcher in each draw.
    // If this texture-fetcher index is out of range, or if no program will read
    // from this fetcher, returns 0.
    UINT32                  TxAttrNeeded(int txfetcher,GLRandom::TxfReq *pTxfReq = NULL);
    bool                    BindlessTxAttrNeeded(UINT32 txAttr);
    UINT32                  VxAttrNeeded(int attrib) const;
    int                     GetInTxAttr(int pgmProp);
    void                    GetAllRefTextureAttribs(GLRandom::TxfSet *pTxfReq);
    bool                    BindlessTxAttrRef(UINT32 Attr);

    //  How many of the s,t,r,q components are needed for this tex-coord.
    //
    // Called by RndGeometry for picking tx-coord-gen modes (when vxprogs are
    // off) and by RndVertexFormat and RndVertexes to determine what to send
    // per-vertex.  This function ignores tx-coord-gen.
    //
    // It is OK if extra components are sent.
    //
    // If vertex or fragment programs are enabled, returns how many of s,t,r,q
    // will be read as inputs by the vertex program (or fragment program if
    // vertex programs are disabled).
    //
    // If both program modes are disabled, calls RndTexturing to get the coords
    // needed by the traditional GL texture unit.
    int                     TxCoordComponentsNeeded(int txu);

    // Returns which components are read by all tex-coords.
    // Meaning that these are safe to use by PointSprites - for bug 2582077.
    UINT32                  ComponentsReadByAllTxCoords() const;

    // Add a user-specified program.
    RC AddUserVertexProgram
    (
        GLRandom::PgmRequirements &Rqmts,   //!< The input/output requirements
        GLRandom::VxAttribute Col0Alias,    //!< Which program attrib we will
                                            //!< read for pri-color
        const char * Pgm                    //!< the program source code
    );
    RC AddUserTessCtrlProgram
    (
        GLRandom::PgmRequirements &Rqmts,   //!< The input/output requirements
        const char * Pgm                    //!< the program source code
    );
    RC AddUserTessEvalProgram
    (
        GLRandom::PgmRequirements &Rqmts,   //!< The input/output requirements
        GLenum tessMode,                    //!< i.e. GL_ISOLINES or GL_QUADS
        const char * Pgm                    //!< the program source code
    );
    RC AddUserGeometryProgram
    (
        GLRandom::PgmRequirements &Rqmts,   //!< The input/output requirements
        const char * Pgm                    //!< the program source code
    );
    RC AddUserFragmentProgram
    (
        GLRandom::PgmRequirements &Rqmts,   //!< input/output requirements
        const char * PgmString              //!< the program source code
    );

    RC AddUserDummyProgram( GLRandom::pgmKind pk); //!< adds a dummy program to keep matched sets intact
    RC WriteLwrrentProgramsToJSFile();
    RC CreateBugJsFile();
    void SetIsLwrrVportIndexed(bool vportIndexed);
    bool GetIsLwrrVportIndexed() const;
    void FreeUserPrograms();    //!< Delete all user-specified vx/fr programs.
    RC   PrintSbos(int sboMask, int startLine, int endLine = -1);

private:
    // Private types:

    // State common to all or most program types:
    struct ProgState
    {
        bool        Enable;        // Program enabled
        int         Light0;        // Light 0 index (x4 + 12 for const reg idx)
        int         Light1;        // Light 1 index (x4 + 12 for const reg idx)
        int         TxLookup[4];   // randomly generated program parameters for const reg idx
        int         PgmElwIdx[4];  // randomly generated program parameters for const reg idx
        GLfloat     PtSize;        // point size for vertex programs
        GLuint      TmpArrayIdxs[4]; // indexes for tmpArray
        GLuint      SubIdxs[4];    // subroutine indexes for CALI programs
        GLuint64    AtomicMask[GLRandom::ammNUM]; // masks used for complementry atomic operations.
        int         TxHandleIdx[4];// texture object handle indexes.
        GLfloat     Random[4];     // four randomly picked values, lwrrently used for SboWindowRect
                                   // when not using SBOs.
    };

    // State that is particular to one program type, collected into a struct
    // so that it can be colweniently cleared and logged.
    struct ExtraState
    {
        bool    VxTwoSided;    // two-sided lighting enabled
        bool    VxSendPtSize;  // enable point size output.
        bool    VxClampColor;  // enable/disable vertex color clamp
        GLfloat TessOuter[4];  // tess-control outer-edge tessellation rates
        GLfloat TessInner[2];  // tess-control inner tessellation rates
        GLfloat TessBulge;     // tess-eval position "bulge" factor.
    };

    // private data:

    ProgState  m_State[GLRandom::PgmKind_NUM];
    ExtraState m_ExtraState;
    GLRandom::SboState   m_Sbos[sboMaxNum];

    // Object IDs for each program's PaBO
    GLuint     m_PaBO[GLRandom::PgmKind_NUM];

    // Containers for all lwrrently-loaded programs:
    //   (vtx, geom, tessctrl, tesseval, frag) X (fixed, random, user).
    // Also, tracks which program is chosen this loop.
    Programs * m_Progs;

    // Program-gen objects, to be used during InitOpenGL (for fixed programs)
    // and during Restart (for random programs).
    GpuPgmGen * m_Gen[GLRandom::PgmKind_NUM];

    bool                    m_LogRndPgms;
    bool                    m_LogToJsFile;
    bool                    m_StrictPgmLinking;
    bool                    m_VerboseOutput;
    int                     m_MultisampleNeeded;
    int                     m_SboMask;  // bitmask of SBO requirements for this frame.

    int                     m_LwrrentLayer;           // Current layer we are rendering to (in case of Layered rendering)
    bool                    m_IsLwrrVportIndexed;     // Current viewport is chosen as index (or mask)
    int                     m_LwrrViewportIdxOrMask;  // Value of current viewport index/mask we are rendering to (in case of Layered rendering)
    std::map<UINT32, bool>  m_BugJsFileCreated;       // Keep track of each glbug*.js files created
                                                      // so we dont repeat our work.
    // private functions:
    void                    PickEnablesAndBindings();
    void                    PickTessCtrlElw();
    void                    PickTessEvalElw();
    void                    PickAtomicModifierMasks();
    void                    CommonPickPgmElw(GLRandom::pgmKind pk);
    void                    PickFragmentElwPerFrame();
    RC                      CreateSbos();
    RC                      InitSboData(int sboMask = (1<<sboMaxNum)-1);
    void                    UpdateSboPgmElw(int pgmKind);
    void                    UpdateSboLocalElw(GLenum target);
    RC                      CreatePaBOs();
    RC                      InitPaBO();

    RC CheckPgmRequirements
    (
        deque<GLRandom::Program*> *pgmChain,
        GLRandom::Program * pProposedNextUpstream
    );
    RC CheckPgmPrims
    (
        deque<GLRandom::Program*> *pChain,
        string * pErrorMessage
    );

    virtual void            SetContext(FancyPicker::FpContext * pFpCtx);
    UINT32                  GetMostImportantTxAttr( UINT32 attr1, UINT32 attr2);
    RC                      GenerateRandomPrograms();

    RC                      LoadProgram (GLRandom::Program * pProg);

    void                    LogFixedPrograms();
    UINT32                  LogProgramRequirements( int index, GLRandom::PgmRequirements &PgmRqmt);
    RC AddUserProgram
    (
        GLRandom::pgmKind pk,
        GLRandom::Program & tmp,
        GLRandom::pgmSource ps = GLRandom::psUser
    );

    RC BindProgram(GLRandom::Program * pProg);

    // Debug routines
    void                    DebugPrintPropMap(GLRandom::PgmRequirements::PropMap & rPropMap);
    void                    DebugPrintTxcMap(GLRandom::PgmRequirements::TxcMap & rTxcMap);
};

//------------------------------------------------------------------------------
// The GLRandom test itself.
//
class GLRandomTest : public GpuTest
{
public:
    GLRandomTest();
    virtual ~GLRandomTest();

    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual RC InitFromJs();
    virtual void PrintJsProperties(Tee::Priority pri);

    UINT32  GetCoverageLevel();
    // Override the ModsTest SetPicker, GetPick, & GetPicker
    virtual RC SetPicker(UINT32 idx, jsval v);
    virtual UINT32 GetPick(UINT32 idx);
    virtual RC GetPicker(UINT32 idx, jsval * v);
    virtual UINT32 GetNumPickers()
                    {return RND_NUM_RANDOMIZERS * RND_BASE_OFFSET;}
    virtual RC SetDefaultPickers(UINT32 first, UINT32 last);

    virtual Goldelwalues *  GetGoldelwalues();
    // FancyPicker setup: JS->C++.
    RC PickerFromJsval(int index, jsval value);

    // FancyPicker setup: C++->JS.
    RC PickerToJsval(int index, jsval * value);

    // FancyPicker most recent pick: C++->JS.
    RC PickToJsval(int index, jsval * value);

    // Print current picks to screen and logfile.  Goldelwalues callback.
    void Print(Tee::Priority pri);

    // Get the #pixels that passed Ztest since last clear.  Goldelwalues callback.
    UINT32 GetZPassPixelCount();

    RC FreeUserPrograms();

    void GetFrameErrCounts
            (
            UINT32 * pGood,
            UINT32 * pSoft,
            UINT32 * pHardMix,
            UINT32 * pHardAll,
            UINT32 * pOther,
            UINT32 * pTotalTries,
            UINT32 * pTotalFails
            ) const;

    // Stuff GLRandom supplies to its GLRandomHelpers:
    enum ExtensionId
    {
        ExtARB_depth_texture
        ,ExtARB_multisample
        ,ExtARB_shadow
        ,ExtARB_texture_border_clamp
        ,ExtARB_texture_non_power_of_two
        ,ExtATI_texture_float
        ,ExtEXT_blend_equation_separate
        ,ExtEXT_blend_func_separate
        ,ExtEXT_depth_bounds_test
        ,ExtEXT_fog_coord
        ,ExtEXT_packed_float
        ,ExtEXT_point_parameters
        ,ExtEXT_secondary_color
        ,ExtEXT_shadow_funcs
        ,ExtEXT_stencil_two_side
        ,ExtEXT_texture3D
        ,ExtEXT_texture_lwbe_map
        ,ExtEXT_texture_edge_clamp
        ,ExtEXT_texture_filter_anisotropic
        ,ExtEXT_texture_integer
        ,ExtEXT_texture_lod_bias
        ,ExtEXT_texture_mirror_clamp
        ,ExtEXT_texture_shared_exponent
        ,ExtIBM_texture_mirrored_repeat
        ,ExtLW_depth_clamp
        ,ExtLW_float_buffer
        ,ExtLW_fog_distance
        ,ExtLW_fragment_program
        ,ExtLW_fragment_program2            // must be after fragment_program
        ,ExtLW_fragment_program3            // must be after fragment_program2
        ,ExtLW_half_float
        ,ExtLW_light_max_exponent
        ,ExtLW_occlusion_query
        ,ExtLW_point_sprite
        ,ExtLW_primitive_restart
        ,ExtLW_vertex_program
        ,ExtLW_vertex_program1_1
        ,ExtLW_vertex_program2
        ,ExtLW_vertex_program3
        ,ExtSGIS_generate_mipmap
        ,ExtLW_transform_feedback
        ,ExtLW_gpu_program4                 // must be after vertex_program3
        ,ExtARB_vertex_buffer_object
        ,ExtLW_gpu_program4_0               // must be after gpu_program4
        ,ExtLW_gpu_program4_1               // must be after gpu_program4_0
        ,ExtLW_explicit_multisample
        ,ExtARB_draw_buffers
        ,ExtEXT_draw_buffers2
        ,ExtARB_draw_buffers_blend
        ,ExtEXT_texture_array
        ,ExtARB_texture_lwbe_map_array
        ,ExtEXTX_framebuffer_mixed_formats  // EXTX is not a typo!
        ,ExtLW_gpu_program5                 // must be after gpu_program_4_1
        ,ExtLW_gpu_program_fp64
        ,ExtLW_tessellation_program5        // must be after gpu_program5
        ,ExtLW_parameter_buffer_object2
        ,ExtLW_bindless_texture
        ,ExtAMD_seamless_lwbemap_per_texture
        ,ExtGL_LW_blend_equation_advanced_coherent
        ,ExtARB_draw_instanced
        ,ExtARB_fragment_layer_viewport
        ,ExtARB_viewport_array
        ,ExtLW_viewport_array2
        ,ExtLW_geometry_shader_passthrough
        ,ExtLW_shader_atomic_fp16_vector
        ,ExtEXT_texture_filter_minmax
        ,ExtARB_texture_view
        ,ExtARB_sparse_texture
        ,ExtARB_compute_variable_group_size //for CRC check using compute shader
        ,ExtLW_shader_atomic_float
        ,ExtLW_shader_atomic_float64
        ,ExtLW_shader_atomic_int64
        ,ExtKHR_texture_compression_astc_ldr
        ,ExtLW_fragment_shader_barycentric
        ,ExtGL_LW_shading_rate_image
        ,ExtGL_LW_conservative_raster
        ,ExtGL_LW_conservative_raster_pre_snap
        ,ExtGL_LW_conservative_raster_pre_snap_triangles
        ,ExtGL_LW_conservative_raster_underestimation
        ,ExtGL_LW_conservative_raster_dilate
        ,ExtGL_LW_draw_texture
        ,ExtNUM_EXTENSIONS
        ,ExtNO_SUCH_EXTENSION = ExtNUM_EXTENSIONS
    };
    GLboolean   HasExt(ExtensionId ex) const;

    /**
    * @{
    * @verbatim
    *        TxUnits                  how many texture combiner results/shader stages
    *        |  TxCoords              how many texture coords used by the rasterizer
    *        |  |  TxFetchers         how many bound textures at one time
    *        |  |  |  VtxTexFetchers  how many bound vertex textures at one time
    *        |  |  |  |  Combiners    how many texture result general combiner stages
    *        |  |  |  |  |
    *  lw4   2  2  2  0  0   TNT/TNT2 has 2 fixed-function texture units.
    *  lw1x  2  2  2  0  2   GF1/GF2  has 2 fixed-function texture units plus combiner.
    *  lw2x  4  4  4  0  8   GF3/Xbox has 4 shader-stages/texture-units plus combiner.
    *  lw3x  4  8 16  0  8   lw30 is like lw2x, but in fragment-program mode can use 8 coords, 16 bound textures
    *  lw4x  4  8 16  4  8   adds vertex textures
    */
    GLint   NumTxUnits() const;
    GLint   NumTxCoords() const;
    GLint   NumTxFetchers() const;
    GLint   NumVtxTexFetchers() const;
    //@}
    GLfloat MaxMaxAnisotropy() const;
    GLint   MaxShininess() const;
    GLint   MaxSpotExponent() const;
    GLuint  Max2dTextureSize() const;
    GLuint  MaxLwbeTextureSize() const;
    GLuint  MaxArrayTextureLayerSize() const;
    GLuint  Max3dTextureSize() const;
    GLuint  MaxUserClipPlanes() const;

    UINT32  ColorSurfaceFormat(UINT32 MrtIndex) const;
    UINT32  NumColorSurfaces() const;
    UINT32  NumColorSamples() const;
    UINT32  DispWidth() const;
    UINT32  DispHeight() const;

    void PickNormalfv(GLfloat * fv, bool PointsFront);

    // Pick repeatability checking.
    //
    // We can log CRC's of our internal random picks and other generated
    // data (i.e. textures) to verify that our randomizer is repeatable.
    //
    // This is very handy when the rendered images aren't repeatable and
    // we need to track down the cause (mods vs. GL driver vs. hardware...).
    //
    // This code is stubbed out if GLRANDOM_PICK_LOGGING is not defined.
    enum eLogMode
    {
        Skip,
        Store,
        Check
    };
    bool        IsGLInitialized() const;
    void        SetLogMode(eLogMode lm);
    eLogMode    GetLogMode()const;
    void        LogBegin(size_t numItems, string (*pItemToString)(uint32)=nullptr);
    UINT32      LogData(int item, const void * data, size_t dataSize);
    RC          LogFinish(const char * label);
    UINT32      StartLoop() const;
    UINT32      LoopsPerFrame() const;
    bool        LogShaderCombinations() const;
    UINT32      PrintXfbBuffer() const;
    void        GetSboSize(GLint * pW, GLint * pH) const;
    bool        IsFsaa() const;
    RC          ActivateBubble();

public:
    // Each helper has a pointer to this GLRandomTest object.
    // Our helpers are public so that they can find each other.
    RndMisc                 m_Misc;
    RndGpuPrograms          m_GpuPrograms;
    RndTexturing            m_Texturing;
    RndGeometry             m_Geometry;
    RndLighting             m_Lighting;
    RndFog                  m_Fog;
    RndFragment             m_Fragment;
    RndPrimitiveModes       m_PrimitiveModes;
    RndVertexFormat         m_VertexFormat;
    RndVertexes             m_Vertexes;
    RndPixels               m_Pixels;

private:
    // Maintain two differnt arrays of helpers so that pick order can differ
    // from send order
    vector<GLRandomHelper *>  m_PickHelpers;
    vector<GLRandomHelper *>  m_SendHelpers;
    mglTestContext    m_mglTestContext;
    GLGpuGoldenSurfaces * m_pGSurfs;

    enum eLogState
    {
        lsUnknown,
        lsBegin,
        lsLog,
        lsFinish
    };
    // debugging vars to determine if we are misusing the golden values.
    int               m_LogState;
    string            (*m_LogItemToString)(uint32) = nullptr;

    // JavaScript-settable properties:
    //TODO debug remove before check-in
    bool              m_UseSanitizeRSQ;
    bool              m_DisableReverseFrames; // Disable running frames in reverse with gputest.js
    UINT32            m_FullScreen;           // full screen (vs window)
    bool              m_DoubleBuffer;         // double buffer (vs render to front buffer)
    bool              m_PrintOnSwDispatch;    // if true, print (like superverbose) draws that hit sw rasterizer
    bool              m_PrintOlwpipeDispatch; // if true, print (like superverbose) draws that hit vpipe dispatch
    bool              m_DoTimeQuery;          // if true, use EXT_timer_query to measure & report render time
    FLOAT64           m_RenderTimePrintThreshold;  // if m_DoTimeQuery, and loop exceeds threshold, print.
    bool              m_CompressLights;       // if true, renumber lights to use 0..n-1 conselwtive always. (lw10,15,11,17)
    bool              m_ClearLines;           // if true, draw a bunch of lines after each clear.
    vector<UINT32>    m_ColorSurfaceFormats;  // if !0, use this surface format regardless of TestConfiguration.DisplayDepth.
    bool              m_ContinueOnFrameError; // continue running if an error oclwrred in a frame?
    string            m_TgaTexImageFileName;  // filename of .TGA file for texture image
    int               m_MaxFilterableFloatTexDepth;
    int               m_MaxBlendableFloatSize;
    bool              m_RenderToSysmem;       // Render offscreen FrameBufferObjects in Sysmem
    bool              m_ForceFbo;             // Draw to offscreen FrameBufferObject.
    UINT32            m_FboWidth;             // Width in pixels of FBO.
    UINT32            m_FboHeight;            // Height in pixels of FBO.
    bool              m_UseSbos;              // Use ShaderBufferObjects & special instructions
    UINT32            m_PrintSbos;            // 0=none, 1=read, 2=write, 3=read & write
    INT32             m_SboWidth;             // Width in pixels of SBO(each pixel is 256 bits).
    INT32             m_SboHeight;            // Height in pixels of FBO(each pixel is 256 bits).
    bool              m_HasF32BorderColor;    // Tesla+ have full border color range
    bool              m_VerboseGCx;           // If true show verbose GCx messages
    UINT32            m_DoGCxCycles;          // Perform GCx cycles between vertices
    UINT32            m_ForceGCxPstate;       // If non-zero force to pstate prior to GCx entry.
    UINT32            m_PciConfigSpaceRestoreDelayMs = 0;
    bool              m_DoRppgCycles;         // Wait for RPPG entry in between vertices.
                                              // Value is a mask of RPPG types to wait for.
    bool              m_DoTpcPgCycles;        // Wait for TPC-PG entry in between vertices,
                                              // mutually exclusive with RPPG
    UINT32            m_ForceRppgMask;        // Explicitly request which RPPG types to check
    bool              m_LogShaderCombinations;// Log each shader combination into a separate
                                              // *.js file (Debug feature)
    bool              m_LogShaderOnFailure;   // Use when regressing frames.
    bool              m_ShaderReplacement;    // Enable shader replacement with content read from files
    static INT32      s_TraceSetupRefCount;   // Guard against conlwrrent devices trying to setup
                                              // the trace environment more than once.
    UINT32            m_DoPgCycles = LowPowerPgWait::PgDisable;
    FLOAT64           m_DelayAfterBubbleMs = 0;
    INT32             m_TraceLevel;
    UINT32            m_TraceOptions;
    UINT32            m_TraceMask0;
    UINT32            m_TraceMask1;
    INT32             m_DisplayWidth;      // height in pixels     (from m_TstCfg)
    INT32             m_DisplayHeight;     // width in pixels      (from m_TstCfg)
    UINT32            m_DisplayDepth;      // color depth in bits     (from m_TstCfg)
    UINT32            m_ZDepth;            // Z buffer depth in bits  (from m_TstCfg)
    UINT32            m_LoopsPerFrame;     // loops per frame (from m_TstCfg)
    UINT32            m_LoopsPerGolden;    // loops per golden check (from m_Golden)
    INT32             m_NumLayersInFBO;    // Num Layers in layered FBO
    UINT32            m_PrintXfbBuffer;    // Transform Feedback capture buffer
    INT32             m_InjectErrorAtFrame;
    INT32             m_InjectErrorAtSurface;
    bool              m_PrintFeatures;
    bool              m_VerboseCrcData;     // if true crc values are printed for each item.
    INT32             m_CrcLoopNum;         // loop number to dump raw CrcData
    INT32             m_CrcItemNum;         // crcItem number to dump.
    UINT32            m_LoopsPerProgram = 0;
    // Bad frame retries (aka "soft errors"):
    //
    // Every N loops, we checksum the image, then clear the screen.
    // We call this a "frame", and this test is designed so that each
    // frame is independent of all others.
    //
    // If one frame fails, we rerun just that frame a few times, and if
    // all retries pass, it is a soft error.
    //
    // We allow zero hard errors, but may allow one or more soft errors.
    //
    UINT32 m_FrameRetries;
    UINT32 m_AllowedSoftErrors;

    // Sequence of Random seeds, one per frame.
    // If the test runs out of seeds or the sequence is empty, we just
    // use TestConfiguration.Seed() + frame-starting-loop-number.
    vector<UINT32> m_FrameSeeds;

    // Number of milliseconds to replay the frame at high speed.
    // If >0, we first record the frame as a display list and then
    // execute that display list N times.
    UINT32   m_FrameReplayMs;

    bool     m_DumpOnAssert;

    //! Maximum output vertexes per draw, after any amplification by
    //! tessellation, DrawElementsInstanced, geometry program etc.
    //! Needed to control draw time and XFB capture buffers.
    //! Settable from JS.
    UINT32   m_MaxOutputVxPerDraw;

    //! Maximum ratio of output vertexes per input vertexes.
    //! Needed to control draw time.
    //! Settable from JS.
    UINT32   m_MaxVxMultiplier;

    // OpenGL object IDs
    GLuint   m_ClearLinesDL;
    GLuint   m_OcclusionQueryId;
    GLuint   m_RunningZpassCount;

    mglTimerQuery * m_pTimer;

    FancyPicker::FpContext * m_pFpCtx   = nullptr;
    FancyPickerArray *       m_pPickers = nullptr;

    MglDispatchMode m_DispatchMode;  // (return from GL driver) sw/hw/vpipe mode used for last draw
    GLfloat  m_SWRenderTimeSecs;       // Duration of last draw in seconds.
    GLfloat  m_HWRenderTimeSecs;       // Duration of last draw in seconds.
    double   m_SWRenderTimeSecsFrameTotal;
    double   m_HWRenderTimeSecsFrameTotal;
    bool     m_IsDrawing;     // True when (we think) the HW is busy.

    struct FrameErrCounts
    {
        UINT32 Good;          // num frames that passed on the first try
        UINT32 Soft;          // num frames that miscompared on first try,
                              //   but were OK on all retries.
        UINT32 HardMix;       // num frames that miscompared on the first try, but had a
                              //   mixture of OK and miscompare on retries.
        UINT32 HardAll;       // num frames that miscompared on the first try, and
                              //   on all retries.
        UINT32 Other;         // num frames that hung or had a non-retryable error on
                              //   the first try on any retry.
        UINT32 TotalTries;    // total number of times we tried a frame.
        UINT32 TotalFails;    // total number of tries that failed.
    };
    FrameErrCounts m_FrameErrs;

    // Collection of all the loop-numbers that miscompared in most recent run.
    set<UINT32> m_MiscomparingLoops;

    // Data for debug-checking that our pickers themselves are repeatable.
    eLogMode       m_LogMode;
    vector<UINT32> m_LogItemCrcs;
    UINT32         m_PlaybackCount = 0;
    UINT32         m_CaptureCount  = 0;

    struct Features
    {
        GLboolean   HasExt[ ExtNUM_EXTENSIONS ];     // Which extensions are present in this GL driver/HW combo
        GLint       NumTxUnits;                      // How many simultaneous texture units are available in HW
        GLint       NumTxCoords;
        GLint       NumTxFetchers;
        GLint       NumVtxTexFetchers;
        GLfloat     MaxMaxAnisotropy;
        GLint       MaxShininess;
        GLint       MaxSpotExponent;
        GLuint      Max2dTextureSize;
        GLuint      MaxLwbeTextureSize;
        GLuint      Max3dTextureSize;
        GLuint      MaxUserClipPlanes;
        GLuint      MaxArrayTextureLayers;
    };
    Features m_Features;

    // lwTracing setup struct for debug.
    LW_TRACE_INFO m_RegistryTraceInfo;
    LW_TRACE_INFO m_MyTraceInfo;
    UINT32        m_LwrrentTraceAction;
    bool          m_PrintOnError = true;
    unique_ptr<GCxBubble> m_GCxBubble;
    UINT32        m_CoverageLevel;          //0=none, 1=basic, 2=detailed
    unique_ptr<RppgBubble> m_RppgBubble;
    unique_ptr<TpcPgBubble> m_TpcPgBubble;
    std::unique_ptr<LowPowerPgBubble> m_LowPowerPgBubble;
    // private functions:

    RC   GetFeatures();
    RC   CleanupGLState();
    RC   DoFrame(UINT32 frame);
    RC   DoReplayedFrame(UINT32 beginLoop, UINT32 endLoop);
    bool NeedGoldenRun();
    RC   GoldenRun(bool alreadySwapped);
    RC   RandomizeOncePerRestart(UINT32 Seed);
    RC   RandomizeOncePerDraw();
    RC   ReportExtCoverage(const char * pExt);

    RC   PickOneLoop();
    void PushGLState();
    void PopGLState();
    RC   SendOneLoop(UINT32 skipMask);
    void DoClearLines(UINT32 TestWidth, UINT32 TestHeight);
    const char *   StrDispatchMode(MglDispatchMode m) const;
    RC   ClearSurfs();
    RC   DrawClearLines();
    void SetupTrace();
    void ConfigTrace(UINT32 action);
    void CleanupTrace();

    // Functions for allowing JS to inspect glrandom's state during Run:
    JSBool LoopGetter(JSContext * cx, JSObject * obj, jsval id, jsval *vp);
    JSBool FrameGetter(JSContext * cx, JSObject * obj, jsval id, jsval *vp);
    JSBool FrameSeedGetter(JSContext * cx, JSObject * obj, jsval id, jsval *vp);

    RC HelperFromPickerIndex(int index, GLRandomHelper **ppHelper);

public:
    SETGET_PROP(FullScreen, UINT32);
    SETGET_PROP(DoubleBuffer, bool);
    SETGET_PROP(PrintOnSwDispatch, bool);
    SETGET_PROP(PrintOlwpipeDispatch, bool);
    SETGET_PROP(DoTimeQuery, bool);
    SETGET_PROP(RenderTimePrintThreshold, double);
    SETGET_PROP(CompressLights, bool);
    SETGET_PROP(ClearLines, bool);
    SETGET_PROP(ContinueOnFrameError, bool);
    SETGET_PROP(FrameRetries, UINT32);
    SETGET_PROP(AllowedSoftErrors, UINT32);
    SETGET_PROP(TgaTexImageFileName, string);
    SETGET_PROP(MaxFilterableFloatTexDepth, UINT32);
    SETGET_PROP(MaxBlendableFloatSize, UINT32);
    SETGET_PROP(RenderToSysmem,bool);
    SETGET_PROP(ForceFbo, bool);
    SETGET_PROP(FboWidth, UINT32);
    SETGET_PROP(FboHeight, UINT32);
    SETGET_PROP(UseSbos, bool);
    SETGET_PROP(PrintSbos, UINT32);
    SETGET_PROP(SboWidth, UINT32);
    SETGET_PROP(SboHeight, UINT32);
    SETGET_PROP(HasF32BorderColor, bool);
    SETGET_PROP(VerboseGCx, bool);
    SETGET_PROP(DoGCxCycles, UINT32);
    SETGET_PROP(PciConfigSpaceRestoreDelayMs, UINT32);
    SETGET_PROP(DoRppgCycles, bool);
    SETGET_PROP(DoTpcPgCycles, bool);
    SETGET_PROP(DoPgCycles, UINT32);
    SETGET_PROP(DelayAfterBubbleMs, FLOAT64);
    SETGET_PROP(ForceRppgMask, UINT32);
    SETGET_PROP(ForceGCxPstate, UINT32);
    SETGET_PROP(LogShaderCombinations, bool);
    SETGET_PROP(LogShaderOnFailure, bool);
    SETGET_PROP(ShaderReplacement, bool);
    SETGET_PROP(TraceLevel,UINT32);
    SETGET_PROP(TraceOptions, UINT32);
    SETGET_PROP(TraceMask0, UINT32);
    SETGET_PROP(TraceMask1, UINT32);
    SETGET_PROP(FrameReplayMs, UINT32);
    SETGET_PROP(DumpOnAssert, bool);
    SETGET_PROP(MaxOutputVxPerDraw, UINT32);
    SETGET_PROP(MaxVxMultiplier, UINT32);
    SETGET_PROP(NumLayersInFBO, INT32);
    SETGET_PROP(PrintXfbBuffer, UINT32);
    SETGET_PROP(InjectErrorAtFrame, INT32);
    SETGET_PROP(InjectErrorAtSurface, INT32);
    SETGET_PROP(PrintFeatures, bool);
    SETGET_PROP(VerboseCrcData, bool);
    SETGET_PROP(CrcLoopNum, INT32);
    SETGET_PROP(CrcItemNum, INT32);
    SETGET_PROP(LoopsPerProgram, UINT32);
    SETGET_PROP(DisableReverseFrames, bool);
    SETGET_PROP(UseSanitizeRSQ, bool);

    jsval ColorSurfaceFormatsToJsval() const;
    void ColorSurfaceFormatsFromJsval(jsval jsv);
    const vector<UINT32> & GetFrameSeeds() const;
    void SetFrameSeeds(const vector<UINT32> &v);
    void SetVportsSeparation(float separation);

    RC MarkGoldenRecs();
    const set<UINT32> & GetMiscomparingLoops() const;
    UINT32 GetSeed (UINT32 loop) const;
    UINT32 GetFrame() const;
    UINT32 GetFrameSeed() const;
    bool   GetIsDrawing() const;
};

// These defines are simply a way to shorten the variable name. They are used
// in the GpuPgmGen::OpData tables and we want to keep the length of the
// tables down to a minimum.
// Note: vp3fp2 & vnafp2 lwrrently are intended for CheetAh class GPUs and full
// support for these extensions is not implemented. Flow control,TXD, & PK
// instructions are missing.
//               [0] = Vertex shaders                       [1] = all other shaders (tessCtrl,TessEval,Geo,Frag)
#define gpu4    {GLRandomTest::ExtLW_gpu_program4,          GLRandomTest::ExtLW_gpu_program4}
#define gpu4_1  {GLRandomTest::ExtLW_gpu_program4_1,        GLRandomTest::ExtLW_gpu_program4_1}
#define expms   {GLRandomTest::ExtLW_explicit_multisample,  GLRandomTest::ExtLW_explicit_multisample}
#define gpu5    {GLRandomTest::ExtLW_gpu_program5,          GLRandomTest::ExtLW_gpu_program5}
#define vp3fp2  {GLRandomTest::ExtLW_vertex_program3,       GLRandomTest::ExtLW_fragment_program2}
#define vnafp2  {GLRandomTest::ExtNO_SUCH_EXTENSION,        GLRandomTest::ExtLW_fragment_program2}
#define vp4fp2  {GLRandomTest::ExtLW_gpu_program4,          GLRandomTest::ExtLW_fragment_program2}

#define extGpu5     GLRandomTest::ExtLW_gpu_program5
#define extGpu4     GLRandomTest::ExtLW_gpu_program4
#define extGpu4_1   GLRandomTest::ExtLW_gpu_program4_1
#define extVp3      GLRandomTest::ExtLW_vertex_program3
#define extVp2      GLRandomTest::ExtLW_vertex_program2
#define extFp2      GLRandomTest::ExtLW_fragment_program2
#define extFp       GLRandomTest::ExtLW_fragment_program
#endif // INCLUDED_GLRANDOM_H
