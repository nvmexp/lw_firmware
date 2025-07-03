/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "mdiag/tests/stdtest.h"

#include "verif2d.h"

#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"

#include "core/include/platform.h"
#include "lwos.h"
#include "core/include/channel.h"
#include "class/cl902d.h"
#include "gpu/include/notifier.h"

V2dClassObj::V2dClassObj( Verif2d *v2d )
{
    m_v2d = v2d;
    m_gpu = m_v2d->GetGpu();
    m_ch  = m_v2d->GetChannel();
    m_subch = NO_SUBCHANNEL;
    m_classId = NO_CLASS;
    m_idleAfterMethodWrite = false;
    m_srcSurface = 0;
    m_dstSurface = 0;
    m_lastLaunchSubchCount = 0;
    m_pEvent = 0;
    m_pNotifier = 0;
    m_WFIMode = WFI_POLL;
}

V2dClassObj::V2dClassObj( void )
{
    m_v2d = 0;
    m_gpu = 0;
    m_ch  = 0;
    m_subch = NO_SUBCHANNEL;
    m_classId = NO_CLASS;
    m_idleAfterMethodWrite = false;
    m_srcSurface = 0;
    m_dstSurface = 0;
    m_lastLaunchSubchCount = 0;
    m_pEvent = 0;
    m_pNotifier = 0;
    m_WFIMode = WFI_POLL;
}

V2dClassObj::~V2dClassObj()
{
    if (m_pNotifier)
    {
        delete m_pNotifier;
    }
}

void V2dClassObj::SetObject( void )
{
    if ( m_subch == NO_SUBCHANNEL )
        m_subch = m_v2d->AllocSubchannel( this );

    DebugPrintf("V2dClassObj::SetObj(%d, 0x%08x)\n", m_subch, m_handle);
    int rv = m_ch->SetObject( m_subch, m_handle );

    m_v2d->IncMethodWriteCount();

    m_v2d->SubchannelChange( m_subch );

    if ( ! rv )
        V2D_THROW( "SetObject failed of object of class " << HEX(4,m_classId) );
}

void V2dClassObj::MethodWrite( UINT32 methodAddr, UINT32 methodData )
{
    if ( m_subch == NO_SUBCHANNEL )
        SetObject();

    if ( methodAddr == NO_METHOD )
        V2D_THROW( "trying to write invalid method in class " << HEX(4,m_classId) );

    DebugPrintf("V2dClassObj::MethodWrite(%d, 0x%04x, 0x%08x\n", m_subch, methodAddr, methodData);
    int rv  = m_ch->MethodWrite( m_subch, methodAddr, methodData );

    m_v2d->IncMethodWriteCount();

    if ( m_subch != m_v2d->GetLastSubchannel() )
        m_v2d->SubchannelChange( m_subch );

    if ( ! rv )
        V2D_THROW( "MethodWrite failed: objclass=" << HEX(4,m_classId) << "methodAddr=" <<
                   HEX(8,methodAddr) << "methodData=" << HEX(8,methodData) );

    if ( m_idleAfterMethodWrite )
        rv = m_ch->WaitForIdle();

    if ( ! rv )
        V2D_THROW( "SetObject failed of object of class " << HEX(4,m_classId) );
}

void V2dClassObj::Init( ClassId classId )
{
    UINT32 retClass;
    ArgReader* pParams = m_v2d->GetParams();
    UINT32 subch_id = pParams->ParamUnsigned( "-subchannel_id",
                                              V2dClassObj::NO_SUBCHANNEL );
    if ( subch_id != V2dClassObj::NO_SUBCHANNEL )
    {
        m_ch->SetTraceSubchNum(classId & 0xFFFF, subch_id);
    }
    m_handle = m_ch->CreateObject(classId & 0xFFFF, NULL, &retClass);
    m_classId = static_cast<ClassId>(retClass);
    if ( ! m_handle )
        V2D_THROW( "GPU object creation of class " << HEX(4,classId) << "failed" );
    DebugPrintf("V2dClassObj::Init: CreateObj(0x%08x) handle 0x%08x\n", m_classId, m_handle);

    m_v2d->AddHandleObjectPair( m_handle, this );

    if (m_WFIMode != WFI_POLL && m_WFIMode != WFI_SLEEP && m_WFIMode != WFI_HOST)
    {
        LwRm* pLwRm = m_ch->GetLwRmPtr();
        UINT32  event_handle;

        // Allocate event
        m_pEvent = Tasker::AllocEvent();
        if (!m_pEvent)
        {
            V2D_THROW("failed to create tasker event" );
        }
        void* const pOsEvent = Tasker::GetOsEvent(
                m_pEvent,
                pLwRm->GetClientHandle(),
                pLwRm->GetDeviceHandle(m_v2d->GetLWGpu()->GetGpuDevice()));
        if (pLwRm->AllocEvent(
                GetHandle(),
                &event_handle,
                LW01_EVENT_OS_EVENT,
                0,
                pOsEvent) != OK)
        {
            V2D_THROW(" Failed to AllocEvent");
        }

        // Allocate notifier area
        m_pNotifier = new DmaBuffer;
        if (!m_pNotifier)
        {
            V2D_THROW("failed to alloc dmabuffer" );
        }
        m_pNotifier->SetArrayPitch( 32 );
        m_pNotifier->SetColorFormat(ColorUtils::Y8);
        m_pNotifier->SetLocation(Platform::GetSimulationMode() == Platform::Amodel ? Memory::Fb : Memory::Coherent);
        if (m_pNotifier->Alloc(m_v2d->GetLWGpu()->GetGpuDevice(), pLwRm) != OK)
        {
            V2D_THROW("Failed to alloc notifier");
        }
        if (m_pNotifier->Map() != OK)
        {
            V2D_THROW("Failed to map notifier");
        }

        if (pLwRm->BindContextDma(m_ch->GetModsChannel()->GetHandle(),
                m_pNotifier->GetCtxDmaHandle()) != OK)
        {
            V2D_THROW("Failed to bind notifier contextdma");
        }
    }
}

void V2dClassObj::AddMethod( string name, UINT32 address, map<string,UINT32> * enumerants )
{
    m_methodMap[ name ] = new Method( address, enumerants );
}

void V2dClassObj::AddField( string name, UINT32 address, UINT32 high, UINT32 low, map<string,UINT32> * enumerants )
{
    m_methodMap[ name ] = new Method( address, enumerants, high, low );
}

void V2dClassObj::AddSurface( string name )
{
    m_surfaceParams.push_back( name );
}

//
// things that are done after every launch for every object
//
void V2dClassObj::PostLaunch( void )
{
}

//
// called from the scripting language to cause a class to render
//
void V2dClassObj::LaunchScript( void )
{
    PreLaunch();
    FlushShadowWritesToGpu();
    PostLaunch();
}

//
// we need to buffer up the shadow writes to handle the case where individual field are written separately
// the separate fields are merged into the shadow state, and the combined method is only written once to the GPU
// Multiple writes to a method must be done by using a different launch (or a data generator style of launch)
// No simulation is done here, so it's suitable for ::Init functions which set up initial state
//
// This is the only function that call the MethodWrite function directly.
//
//
void V2dClassObj::FlushShadowWritesToGpu( void )
{
    UINT32 methodAddr, methodData;

    // do the launch
    // find all dirty methods and write them out
    // XXX todo: make sure that a launch method is written, right now we rely on the user to rewrite the
    // launch method manually to ensure the class really launches, otherwise we just simply write method data
    // to the gpu with no side effects
    //
    deque<UINT32>::iterator i;
    for ( i = m_dirtyMethods.begin(); i != m_dirtyMethods.end(); i++ )
    {
        methodAddr = *i;
        methodData = GetShadowData( methodAddr );
        MethodWrite( methodAddr, methodData );
    }

    // if there were methods written to the GPU, update this object's launch subchannel count so that it
    // knows when it has (and has not) been switched away from
    //
    if ( m_dirtyMethods.size() > 0 )
        m_lastLaunchSubchCount = m_v2d->GetSubchChangeCount();

    m_dirtyMethods.clear();
}

//
// return the shadowed (last written) value for this object at the given method value
// if there is no shadowed data, return zero
//

UINT32 V2dClassObj::GetShadowData( UINT32 methodAddr )
{
    map<UINT32,UINT32>::iterator i = m_methodShadowDataMap.find( methodAddr );
    if ( i == m_methodShadowDataMap.end() )
        return 0;

    return (*i).second;
}

void V2dClassObj::SetShadowData( UINT32 methodAddr, UINT32 methodData )
{
    m_methodShadowDataMap[ methodAddr ] = methodData;
}

UINT32 V2dClassObj::GetNamedMethodOrFieldShadow( string methodName )
{
    map<string,Method *>::iterator i = m_methodMap.find( methodName );
    if ( i == m_methodMap.end() )
        V2D_THROW( "V2dClassObj::GetNamedMethodOrFieldShadow(string): illegal method string " << methodName );

    Method *m = (*i).second;
    UINT32 data = GetShadowData( m->m_address );
    UINT32 highMask = 0xFFFFFFFF >> (31 - m->m_high);
    data &= highMask;
    data >>= m->m_low;

    return data;
}

//
// write the method to the object's shadow state
// and queue the method write to the GPU
//

void V2dClassObj::MethodOrFieldWriteShadow( Method *m, UINT32 data )
{
    // store and update only those bits defined in the field

    UINT32 lowMask = 0xFFFFFFFF << m->m_low;
    UINT32 highMask = 0xFFFFFFFF >> (31 - m->m_high);
    UINT32 mask = lowMask & highMask;
    data <<= m->m_low;

    // check to make sure that the provided data actually fits into the given bitfield
    //
#if 0
    if ( (data & ~mask) != 0 )
        V2D_THROW( "given data " << data << "extends outside of defined bitfield "
                   << m->m_high << ":" << m->m_low );
#else
    data &= mask;
#endif // #if 0

    UINT32 shadowData = GetShadowData( m->m_address );
    shadowData &= ~mask;                // zero the data in this bitfield
    UINT32 newShadowData = shadowData | data;   // OR in the new data
    SetShadowData( m->m_address, newShadowData );
    SetDirty( m->m_address );     // important!  If not marked dirty, the data won't be written to the gpu
}

//
// writes a method to the shadow state of an object via the method name, and queues it for real write
// to the gpu
//
void V2dClassObj::NamedMethodOrFieldWriteShadow( string name, UINT32 data )
{
    map<string,Method *>::iterator i = m_methodMap.find( name );
    if ( i == m_methodMap.end() )
        V2D_THROW( "V2dClassObj::NamedMethodOrFieldWriteShadow(string, UINT32): illegal method string " << name );

    Method *m = (*i).second;
    MethodOrFieldWriteShadow( m, data );
}

//
// first, the enumerated field string is first colwerted to its integral value,
// then we write the method to the shadow state of the object via the method name, and queues
// the method write for writing to the gpu
//
void V2dClassObj::NamedMethodOrFieldWriteShadow( string name, string enumData )
{

    map<string,Method *>::iterator i = m_methodMap.find( name );
    if ( i == m_methodMap.end() )
        V2D_THROW( "V2dClassObj::NamedMethodOrFieldWriteShadow(string, string): illegal method string " << name );

    Method *m = (*i).second;

    if(m->m_enumerants == 0)
        V2D_THROW( "no enumerants in call to " << name );

    map<string,UINT32>::iterator j = m->m_enumerants->find( enumData );

    //if enumerant not found
    if ( j == m->m_enumerants->end() )
        {
        string legalEnumerants="";

        map<string,UINT32>::const_iterator j = m->m_enumerants->begin();
        for(j=j; j!= m->m_enumerants->end(); j++)
            {
            legalEnumerants = legalEnumerants + (*j).first;
            legalEnumerants = legalEnumerants + ",";
            }
        V2D_THROW( "illegal enumerant: " << enumData<< ". Legal enumerants for method "<< (*i).first<< " are "<<legalEnumerants);
        }

    UINT32 enumVal = (*j).second;
    MethodOrFieldWriteShadow( m, enumVal );
}

bool V2dClassObj::IsMethodDirty( UINT32 methodAddr )
{
    deque<UINT32>::iterator i;

    for ( i = m_dirtyMethods.begin(); i != m_dirtyMethods.end(); i++ )
    {
        if ( *i == methodAddr )
            return true;                 // yes, it's dirty
    }

    return false;       // no, it's not dirty
}

//
// add the given method to the head of the list of dirty methods
//

void V2dClassObj::SetDirtyFirst( UINT32 methodAddr )
{
    m_dirtyMethods.push_front( methodAddr );
}

void V2dClassObj::SetDirty( UINT32 methodAddr )
{
    deque<UINT32>::iterator iter = find(m_dirtyMethods.begin(), m_dirtyMethods.end(), methodAddr);

    if (iter != m_dirtyMethods.end())
    {
        m_dirtyMethods.erase(iter);
    }

    m_dirtyMethods.push_back( methodAddr );
}

Method::Method( UINT32 a, map<string,UINT32> * e )
{
    m_address = a;
    m_enumerants = e;
    m_high = 31;
    m_low = 0;
}

Method::Method( UINT32 a, map<string,UINT32> * e, UINT32 high, UINT32 low )
{
    m_address = a;
    m_enumerants = e;
    m_high = high;
    m_low = low;
}

void V2dClassObj::WaitForIdle()
{
    RC rc = OK;

    if (m_WFIMode != WFI_POLL && m_WFIMode != WFI_SLEEP && m_WFIMode != WFI_HOST)
    {
        vector<UINT08> data(m_pNotifier->GetSize(), 0xff);
        Platform::VirtualWr(m_pNotifier->GetAddress(), &data[0], data.size());
        
        MethodWrite(LW902D_SET_NOTIFY_A, LwU64_HI32(m_pNotifier->GetCtxDmaOffsetGpu()));
        MethodWrite(LW902D_SET_NOTIFY_B, LwU64_LO32(m_pNotifier->GetCtxDmaOffsetGpu()));
        MethodWrite(LW902D_NOTIFY, (m_WFIMode==WFI_NOTIFY)?
            LW902D_NOTIFY_TYPE_WRITE_ONLY:LW902D_NOTIFY_TYPE_WRITE_THEN_AWAKEN);
        MethodWrite(LW902D_NO_OPERATION, 0);
        
        if (m_ch->MethodFlushRC() != OK)
        {
            V2D_THROW("Failed to flush methods");
        }
    }

    switch( m_WFIMode ) {
        case  WFI_POLL:
        case  WFI_HOST:
            rc = m_ch->WaitForIdleRC();
            break;
        case  WFI_SLEEP:
            Tasker::Sleep(10000);
            break;
        case WFI_NOTIFY:
            rc = Notifier::WaitExternal(
                m_ch->GetModsChannel(),
                m_pNotifier->GetAddress(),
                20000);          // hardcode timeout to 20 sec.
            break;
        case WFI_INTR:
        case WFI_INTR_NONSTALL:
            rc = Tasker::WaitOnEvent(m_pEvent, Tasker::FixTimeout(20000));
            if (rc == OK)
            {
                rc = Notifier::WaitExternal(
                    m_ch->GetModsChannel(),
                    m_pNotifier->GetAddress(),
                    20000);          // hardcode timeout to 20 sec.
            }
            break;
    }

    if (rc != OK)
    {
        V2D_THROW("Wait for idle failed %s" << rc.Message());
    }
}
