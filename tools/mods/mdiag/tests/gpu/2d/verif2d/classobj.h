/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2013, 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _VERIF2D_CLASSOBJ_H_
#define _VERIF2D_CLASSOBJ_H_

class Verif2d;
class V2dGpu;
class LWGpuChannel;

#include <deque>
#include "class/cl902d.h"
#include "gpu/include/notifier.h"

#define V2DREF_MASK(fld)    (0xFFFFFFFF>>(31-((1?fld) % 32)+((0?fld) % 32)))
#define V2DREF_SHIFT(fld)    ((0?fld) % 32)
#define V2DREF_FLD(fld,value) ( (value & V2DREF_MASK(fld)) << V2DREF_SHIFT(fld) )

// for defining methods and method subfields
//
class Method
{
    friend class V2dClassObj;

public:
    Method( UINT32 a, map<string,UINT32> * e );
    Method( UINT32 a, map<string,UINT32> * e, UINT32 h, UINT32 l );

    UINT32 GetAddress( void ) { return m_address; }
    UINT32 GetHigh( void ) { return m_high; }
    UINT32 GetLow( void ) { return m_low; }
    map<string,UINT32> *  GetEnumerants( void ) { return m_enumerants; }

private:
    UINT32 m_address;
    UINT32 m_high;
    UINT32 m_low;
    map<string,UINT32> * m_enumerants;
};

class V2dClassObj
{
public:
    V2dClassObj( void );
    V2dClassObj( Verif2d *v2d );
    virtual ~V2dClassObj( void );

    enum NoMethod
    {
        NO_METHOD = 0xFFFFFFFF
    };

    enum NoSubchannel
    {
        NO_SUBCHANNEL = 0xFFFFFFFF
    };

    enum ClassId
    {
        _LW_NULL_CLASS = 0x00000030,
        _FERMI_TWOD_A = FERMI_TWOD_A,
        NO_CLASS = 0xFFFFFFFF,
    };

    // each derived classobject overrides this function to initialize its own method addresses
    // (but still uses this one for the generic work that it does)
    virtual void Init( ClassId classId );

    virtual UINT32 GetHandle( void ) { return m_handle; }
    virtual void LoseSubChannel( void ) { m_subch = NO_SUBCHANNEL; }
    virtual void AddMethod( string name, UINT32 address, map<string,UINT32> * enumerants = 0 );
    virtual void AddField( string name, UINT32 address, UINT32 high, UINT32 low, map<string,UINT32> * enumerants = 0 );

    virtual void LaunchScript( void );

    virtual bool IsNullObject( void ) { return ( m_classId == _LW_NULL_CLASS ); }

    // if you override the parent class's PreLaunch or PostLaunch, you MUST call your parent's version
    // before going into your own implementation
    //
    virtual void PreLaunch( void ) { }   // stuff to do before every launch
    virtual void PostLaunch( void );

    virtual void NamedMethodOrFieldWriteShadow( string name, UINT32 data );
    virtual void NamedMethodOrFieldWriteShadow( string name, string enumData );
    virtual UINT32 GetNamedMethodOrFieldShadow( string methodName );

    virtual void SetSurfaces( map<string,V2dSurface*> & argMap )
        {
            V2D_THROW( "internal error: in top level SetSurfaces, missing implementation on a class probably" );
        }
    virtual void SetSurfaces( V2dSurface* src, V2dSurface* dst )
        {
            V2D_THROW( "internal error: in top level SetSurfaces, missing implementation on a class probably" );
        }
    virtual void ResetSurfaces()
        {
            V2D_THROW( "internal error: in top level ResetSurfaces, missing implementation on a class probably" );
        }
    virtual void SendInlineData( map<string,UINT32> & argMap )
        {
            V2D_THROW( "internal error: in top level SendInlineData, missing implementation on a class probably" );
        }
    virtual void SendInlineDataArray( map<string,UINT32> & argMap )
        {
            V2D_THROW( "internal error: in top level SendInlineData, missing implementation on a class probably" );
        }
    virtual void SetRenderEnable( map<string,UINT32> & argMap )
        {
            V2D_THROW( "internal error: in top level SetRenderEnable, missing implementation on a class probably" );
        }
    virtual void AddSurface( string name );
    virtual void FlushShadowWritesToGpu( void );
    virtual bool IsMethodDirty( UINT32 methodAddr );
    void SetDirtyFirst( UINT32 methodAddr );
    ClassId GetClassId() const { return m_classId; }

    virtual void WaitForIdle();
    void         SetWFIMode(WfiMethod mode) { m_WFIMode = mode; }
    WfiMethod    GetWFIMode() const { return m_WFIMode; }

protected:
    LWGpuChannel *m_ch;
    Verif2d *m_v2d;
    V2dGpu *m_gpu;

    ClassId m_classId;
    UINT32 m_handle;
    UINT32 m_subch;

    bool m_idleAfterMethodWrite;

    // map script tag strings to method definition
    map<string,Method *> m_methodMap;
    map<UINT32,UINT32> m_methodShadowDataMap; // shadows all method writes to all methods (keeps only most recent one)
    deque<UINT32> m_dirtyMethods; // which methods' shadow data has changed and not yet been sent to gpu

    virtual void SetObject( void );
    virtual void MethodWrite( UINT32 methodAddr, UINT32 methodData );
    virtual void MethodOrFieldWriteShadow( Method *m, UINT32 data );
    UINT32 GetShadowData( UINT32 methodAddr );
    void SetShadowData( UINT32 methodAddr, UINT32 methodData );
    void SetDirty( UINT32 methodAddr );

    vector<string> m_surfaceParams;
    V2dSurface *m_srcSurface;
    V2dSurface *m_dstSurface;

    // Records the subchannel change counter's value at the time of the last launch of this object.
    // If the current (global) subchannel change counter is not equal to this value, then a subchannel change or a
    // setobject has oclwrred since the last launch of this object, and all of this object's volatile state
    // is invalid.   Knowing this is useful for managing the surfaces of some classes, since we maintain the illusion
    // surfaces are static via the setsurfaces() function, and some classes use volatile methods (scaled, dvd) to
    // hold the input or output "surfaces".   Those classes which volatile surface methods must implement the
    // PreLaunch() function (which must begin by calling its parent class's PreLaunch()) and checks the
    // subchannel change counter.  If the counter has changed since the last launch of this class, then the volatile
    // surface methods are resent.   Care must be taken in updating the shadow state in this case, as we don't
    // want to clobber explicit sets of methods/fields which make up the surface.
    //
    UINT32 m_lastLaunchSubchCount;

    ModsEvent *m_pEvent;
    DmaBuffer *m_pNotifier;
    WfiMethod  m_WFIMode;
};

#define FIELD_RANGE( range )     ( 1 ? range ), ( 0 ? range )

#endif // _VERIF2D_CLASSOBJ_H_
