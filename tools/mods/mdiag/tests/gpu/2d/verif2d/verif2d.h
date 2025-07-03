/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2013,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _VERIF_2D_H_
#define _VERIF_2D_H_

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/2d/lwgpu_2dtest.h"
#include "mdiag/utils/buffinfo.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <list>
#define HEX(width, value) setw(width) << setfill('0') << hex << value << dec << " "
struct V2DException
{
    string Message;
    bool StatusSet;

    V2DException(string str, bool status) :
        Message(str), StatusSet(status) {}
};

#define V2D_THROW(x) \
{\
    stringstream ss; \
    ss << x; \
    throw V2DException(ss.str(), false); \
}

#define V2D_THROW_NOUPDATE(x) \
{\
    stringstream ss; \
    ss << x; \
    throw V2DException(ss.str(), true); \
}

#include "surf.h"
#include "coord.h"
#include "v2dgpu.h"
#include "classobj.h"
#include "twod.h"
#include "null.h"
#include "lex.h"
#include "nodes.h"
#include "parse.h"
#include "func.h"

class LWGpuChannel;
class DmaReader;
class GrRandom;

class Verif2d
{
public:
    Verif2d( LWGpuChannel *ch, LWGpuResource *lwgpu,
             Test *test, ArgReader *pParams);
    Verif2d( LWGpuChannel *ch, LWGpuResource *lwgpu, V2dSurface::Flags,
             Test *test, ArgReader *pParams);
    ~Verif2d( void );
    void Init(V2dGpu::GpuType gpuType);
    V2dGpu *GetGpu( void ) { return m_gpu; }
    LWGpuChannel *GetChannel( void ) { return m_ch; }
    LWGpuResource *GetLWGpu( void ) { return m_lwgpu; }
    Test *GetTest( void ) { return m_test; }
    void SetSrcRect( UINT32 x, UINT32 y, UINT32 width, UINT32 height );
    void SetDstRect( UINT32 x, UINT32 y, UINT32 width, UINT32 height );
    void LoseLastSubchAllocObjSubchannel( void );
    UINT32 AllocSubchannel( V2dClassObj *obj );
    UINT32 FreeSubchannel( V2dClassObj *obj );
    V2dRect const & GetSrcRect(void) { V2dRect const &srcRef = m_srcRect; return srcRef; }
    V2dRect const & GetDstRect(void) { V2dRect const &dstRef = m_dstRect; return dstRef; }
    void SaveDstSurfaceToTGAfile( string fileName );
    void ClearDstSurface( string clearPatternStr, UINT32 clearPatternData );
    GrRandom *m_grRandom;
    Buf *m_buf;
    void WaitForIdle( void );
    bool CompareDstSurfaceToCRC( string fileName, string crcStr );
    V2dSurface *GetDstSurface( void ) { return m_dstSurface; }
    V2dSurface *GetSrcSurface( void ) { return m_srcSurface; }
    void ChangeSrcSurface( V2dSurface *surf ) { m_srcSurface = surf; }
    void ChangeDstSurface( V2dSurface *surf ) { m_dstSurface = surf; }
    void SetCtxSurfType( string surfType );
    V2dClassObj *NewClassObj( string className );

    // context state manipulation functions
    Function *GetFunctionTranslator( void ) { return &m_function; }
    bool HandleExists( UINT32 handle );    // is there an existing object with this handle?
    void AddHandleObjectPair( UINT32 handle, V2dClassObj *obj );
    V2dClassObj *GetObjectFromHandle( UINT32 handle );
    bool HandleExistsSurf( UINT32 handle );    // is there an existing surface object with this handle?
    void AddHandleObjectPairSurf( UINT32 handle, V2dSurface *obj );
    V2dSurface *GetObjectFromHandleSurf( UINT32 handle );
    V2dSurface::ColorFormat GetColorFormatFromString( string format );
    UINT32 GetLastSubchannel( void ) { return m_lastSubchannel; }
    UINT32 GetSubchChangeCount( void ) { return m_subchannelChangeCount; }
    void SubchannelChange( UINT32 newSubch )
        {
            m_subchannelChangeCount += 1;
            m_lastSubchannel = newSubch;
        }
    // returns a 32-bit random number
    UINT32 Random( void );
    void SetImageDumpFormat( V2dSurface::ImageDumpFormat f );
    V2dSurface::ImageDumpFormat GetImageDumpFormat();
    void SetDoDmaCheck( bool d ) { m_doDmaCheck = d; }
    bool GetDoDmaCheck() { return m_doDmaCheck; }
    void SetDoDmaCheckCE( bool d ) { m_doDmaCheckCE = d; }
    bool GetDoDmaCheckCE() { return m_doDmaCheckCE; }
    void SetDoGpuCrcCalc( bool d ) { m_doGpuCrcCalc = d; }
    bool GetDoGpuCrcCalc() { return m_doGpuCrcCalc; }
    void SetCrcData(CrcMode m, CrcProfile *p, string s)
        { m_crcMode = m; m_crcProfile = p; m_crcSection = s; }
    CrcMode GetCrcMode() { return m_crcMode; }
    CrcProfile* GetCrcProfile() { return m_crcProfile; }
    const char* GetCrcSection() { return m_crcSection.c_str(); }
    void SetScriptDir(string s) { m_scriptDir = s; }
    string GetScriptDir() const { return m_scriptDir; }
    void SetSectorPromotion(const string &s) { m_sectorPromotion = s; }
    const string& GetSectorPromotion() const { return m_sectorPromotion; }
    void IncMethodWriteCount() { m_methodWriteCount++; }
    int GetMethodWriteCount() { return m_methodWriteCount; }
    ArgReader *GetParams(void) { return m_pParams; }

    string get_Csurface_format_string( int surf3d_C_surface_format ) const;
    string get_Zsurface_format_string( int surf3d_C_surface_format ) const;

    V2dSurface::Flags m_dstFlags;

    // all allocated surfaces, so we can free them after the test has finished
    vector<V2dSurface*> m_surfaces;
    BuffInfo m_buffInfo;

    V2dClassObj::ClassId m_classId;

    typedef list<string>     CrcChain;
    void AddCrc(const string& crc) { m_CrcChain.push_back(crc); }
    const CrcChain* GetCrcChain() const { return &m_CrcChain; }

    DmaReader* GetDmaReader();
    DmaReader* GetGpuCrcCallwlator();

private:
    V2dSurface *m_dstSurface;
    V2dSurface *m_srcSurface;
    LWGpuChannel *m_ch;
    LWGpuResource *m_lwgpu;
    ArgReader *m_pParams;
    Test *m_test;
    V2dGpu *m_gpu;
    V2dRect m_srcRect;
    V2dRect m_dstRect;
    V2dClassObj *m_lastSubchAllocObj;
    V2dSurface::ImageDumpFormat m_imageDumpFormat;
    bool m_doDmaCheck;
    bool m_doDmaCheckCE;
    bool m_doGpuCrcCalc;
    CrcMode m_crcMode;
    CrcProfile *m_crcProfile;
    string m_crcSection;
    string m_scriptDir;
    string m_sectorPromotion;

    vector<V2dClassObj *> m_classObjects;
    vector<V2dClassObj *> m_subchannelObjects;

    UINT32 m_subchannelChangeCount;
    UINT32 m_lastSubchannel;
    int m_methodWriteCount;

    void UpdateCtxSurfAllRenderObjects( void );
    void InitSurfStringMaps( void );
    void InitClassnameMap( void );
    void FlushGpuCache( void );

    map<string, V2dSurface::ColorFormat> m_SurfStringMap;
    map<int,string>                      m_surf3d_C_enum_to_string;
    map<int,string>                      m_surf3d_Z_enum_to_string;
    Function m_function;
    map<string,V2dClassObj::ClassId> m_classNameMap;
    map<UINT32,V2dClassObj*> m_handleToObjectMap;
    map<UINT32,V2dSurface*> m_handleToObjectMapSurf;

    CrcChain                 m_CrcChain;

    DmaReader* m_pDmaReader;
    DmaReader* m_pGpuCrcCallwlator;
};

#endif // _VERIF_2D_H_
