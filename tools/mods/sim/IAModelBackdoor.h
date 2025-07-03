/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2008,2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// DO NOT EDIT
// See https://wiki.lwpu.com/engwiki/index.php/MODS/sim_linkage#How_to_change_ifspec

#ifndef _IAMODEL_BACKDOOR_H_
#define _IAMODEL_BACKDOOR_H_

#include "ITypes.h"
#include "IIface.h"

/// Please, please, please increment the version number with each change
/// We couldn't find an automatic way for this to happen.
/// Version 6: Added 2 GetClassID() functions on IAModelToResman.
/// Version 7: Added dynamic contextDMAs
/// Version 8: Added errorNotifierContextDMA in the channel creation functions
/// Version 9: Added command line args on the setup() function
/// Version 10: Broken the backdoor interface into a common part, and then mods/SoftAmodel
///             specific parts
/// Version 11: Added a new function for mods to ask the amodel to do additional verification
///             It's needed for FermiPerfsim to be able to do it's own perf verification and
///             have trace_3d report it properly in addition to the image CRC checking.
/// Version 12: Added a new function for mods to ask the amodel for the character to be appended
///             on CRC keys. 'a' for amodel, or 'p' for perfsim.
/// Version 13: Added ResourceType and AllocAttribs on amodel_mapping_info which is used by SoftAmodel/DirectAModel
///
/// Version 13: Added GetModelIdentifierString() which mods would want soon, but haven't bumped up the version
///             to reduce disruption, since mods doesn't check the version, and drivers don't use this part of the interface...
/// Version 13: Removed GetModelIdentifier() which mods now stopped using but haven't bumped up the version
///             to reduce disruption, since mods doesn't check the version, and drivers don't use this part of the interface...

#define AMODEL_BACKDOOR_VERSION 13

///////////////////////////////////////////////////////////////////////////////////////////
// Summary of interfaces defined in this file
// ------------------------------------------
// The initial intention was that there is a single interafce which both mods
// and RM/SoftAmodel will use. The evolutation of the hookups cause these to
// actually have very little overlap, so at some point we split into 2 separate
// interfaces so that changes in one would not affect the other.
// There is also an interface that is used by amodel to call back into the
// RM/SoftAmodel code, which is not used by mods.
// - IAModelBackdoorSoftAmodel - For SoftAmodel to call into amodel
// - IAModelBackdoor - For mods to call into amodel.
// - IAModelBackdoorCommon - Shared functions between the 2 above.
// - IAModelToResman - Callback for amodel into SoftAmodel.
///////////////////////////////////////////////////////////////////////////////////////////

/// This structure is used by SoftGPU and AModel
typedef struct amodel_dma_info {
    LwU032 ClassId;       // Unused??
    LwU032 Target;        // one of LW_DMA_TARGET_NODE_{LWM, PCI, AGP}
    LwU032 Limit;         // size - 1 in bytes (if 1024 buffer then would be 0x3FF)
    LwU032 Base;          // User Mode Virtual Address of Base
    LwU032 Protect;       // One of LW_DMA_ACCESS_READ_{ONLY, AND_WRITE}
    LwU032 ** PageTable;  // NULL means linear mapping; full meaning not yet defined
    bool   dynamic;       // If true, mappings need to be queried separately
} AMODEL_DMA_INFO, *P_AMODEL_DMA_INFO;

typedef struct amodel_mapping_info {
    LwU032 Base;          // User Mode Virtual Address of Base
    LwU064 Length;
    LwU064 Offset;        // Offset that corresponds to base addr
    LwU032 ResourceType;    // the resource type: one of LWOS32_TYPE_*. from //sw/main/sdk/lwpu/inc/lwos.h
    LwU032 AllocAttribs;    // the alloc attrs: a combination of LWOS32_ATTR_*. from //sw/main/sdk/lwpu/inc/lwos.h
} AMODEL_MAPPING_INFO, *P_AMODEL_MAPPING_INFO;

/// Through this interface AModel talks back to RM.
class IAModelToResman {
public:
    /// Class with virtual methods should have virtual destructor
    virtual ~IAModelToResman(void) { }

    /// Store in *pDmaInfo the definition of the DMA context with the given (channel id, handle).
    /// AModel makes this call whenever it fails to find a dma context with the given handle
    /// in its (AModel's) hash table.
    /// Returns 0 for success.
    virtual LwU032 GetDMAInfo(LwU032 chID, LwU032 handle, P_AMODEL_DMA_INFO pDmaInfo) = 0;
    /// Amodel calls this method to notify RM that the specified register has been
    ///  updated with the specified new value.
    virtual void UpdateRegister(LwU032 chID, LwU032 registerAddress, LwU032 newValue) = 0;

    enum ErrorFlags {
        ERRORFLAGS_NONE=0,
        ERRORFLAGS_KILL_CHANNEL=1,  // Don't want anymore methods from this channel
        ERRORFLAGS_KILL_AMODEL=2,   // Amodel is hosed don't bother sending anything else
    };
    /// Notify resman of an error that happened. We expect it to stop sending methods
    /// on this channel since they would very likely keep producing error messages.
    virtual void Error(const char * msg, ErrorFlags flags) = 0;

    enum OverlayFormat {
        OVERLAY_FORMAT_RGB32=0,
        OVERLAY_FORMAT_ARGB16=1,
    };
    /// Use overlay engine to display a rendered surface
    virtual void SetOverlay(
        LwU032 ctxDMAHandle, LwU032 offset,         // Where is the source
        LwU032 xIn, LwU032 yIn,     // Starting point within source
        LwU032 width, LwU032 height, LwU032 pitch, OverlayFormat format, // Properties of the source
        LwU032 screenPosX, LwU032 screenPosY       // Where on the screen will the overlay be displayed
        ) = 0;
    /// Stop using the overlay to display a rendered surface
    virtual void DisableOverlay() = 0;

    /// Call for getting information on a hardware object. When amodel can't find the object in
    /// it's own table, it can call this with channel and handle, and will get a class ID
    /// Returns 0 for success
    virtual LwU032 GetClassId(LwU032 chID, LwU032 handle, LwU032 &classID) = 0;
    /// Call for getting handle ob object bound to a specific subchannel
    /// Returns 0 for success
    virtual LwU032 GetSubchannelObject(LwU032 chID, LwU032 subchannel, LwU032 &handle) = 0;

    /// Use this to ask for a mapping associated with a dynamic contextDMA. Note that the mapping will
    /// not necessarily start at the same offset we are interested in, but only include it.
    /// Returns 0 for success.
    virtual LwU032 GetMapping(LwU032 chID, LwU032 contextDMAHandle, LwU064 offset, LwU064 length, P_AMODEL_MAPPING_INFO pMappingInfo ) = 0;
};

///
/// This interface provides a way to feed an AModel with some info that does not
/// go through the other chiplib interfaces. It lets it do the job without having
/// to parse the HW hash table and object state in instance memory.
/// This is defined to provide needs of mdiag,MODS, and the liveRM hookup. Some functions
/// may be completely unused by some of these clients.
///
class IAModelBackdoorCommon : public IIfaceObject {
public:
    /// AllocChannelDma is called by MDiag or RM to create a new channel
    /// If ErrorNotifierCtxDma!=0 a notifier write to it will occur if amodel
    /// detected an interrupt. See notifier spec below.
    virtual void AllocChannelDma(LwU032 ChID, LwU032 Class, LwU032 CtxDma, LwU032 ErrorNotifierCtxDma = 0 ) = 0;
    /// Free the channel and all objects associated with it
    virtual void FreeChannel(LwU032 ChID) = 0;
    /// AllocContextDma is called by MODS (and never by RM/SoftAmodel) to provide full details for a newly created DMA context
    /// The parameter values have the same meaning as in struct amodel_dma_info above.
    /// When running with SoftAmodel/RM, we will query for contextDMA details when we have a
    /// reference but can't find it's details.
    virtual void AllocContextDma(LwU032 ChID, LwU032 Handle, LwU032 Class, LwU032 target, LwU032 Limit,
                                 LwU032 Base, LwU032 Protect, LwU032 *PageTable) = 0;
    /// AllocObject is called by MDiag and RM each time a new object is created in any channel
    virtual void AllocObject(LwU032 ChID, LwU032 Handle, LwU032 Class) = 0;
    /// Release a HW object. It may have been allocated either through AllocObject or AllocContextDMA.
    virtual void FreeObject(LwU032 ChID, LwU032 Handle) = 0;

};

class IAModelBackdoorSoftAmodel : public IAModelBackdoorCommon  {
public:
    /// Setup is called by RM (and never by mdiag) and provides an interface that AModel can use
    /// to talk back to RM, and the memory mode (simulated or pass-through).
    virtual void Setup( IAModelToResman *pResman, LwU032 amodelVersion, char** argv, int argc ) = 0;

    /// ProcessMethod is called by RM (and never by MDiag) to execute the specified method.
    virtual void ProcessMethod(LwU032 ChID, LwU032 Subch, LwU032 MethodAddr, LwU032 MethodData ) = 0;

    // For dynamic contextDMAs we may have to ilwalidate part of the memory mappings associated
    // with them. (full ilwalidation can be done by FreeObject() )
    virtual void IlwalidateMapping (LwU032 ChID, LwU032 contextDMAHandle, LwU064 offset, LwU064 length ) = 0;

    // Let us know that we are about to get unloaded so we can do any cleanup/flushing
    // before it's too late.
    virtual void ShutDown() = 0;
};

class IAModelBackdoor : public IAModelBackdoorCommon {
public:
    // The GpFifo version of allocating a channel.
    /// If ErrorNotifierCtxDma!=0 a notifier write to it will occur if amodel
    /// detected an interrupt. See notifier spec below.
    virtual void AllocChannelGpFifo(LwU032 ChID, LwU032 Class, LwU032 CtxDma, LwU064 GpFifoOffset,
        LwU032 GpFifoEntries, LwU032 ErrorNotifierCtxDma = 0 ) = 0;

    /// Give the chiplib a chance to do any additional verification steps it maye want, and report
    /// true if all is good, or false if a failure detected. A file name will be provided so that
    /// we can use it's path to look for additional reference files if needed.
    /// If any errors are detected, the chiplib is responsible for printing out relevant details
    /// explaining what exactly is failing.
    virtual bool PassAdditionalVerification( const char *traceFileName ) = 0;

    /// Return a string that will be appended on the crc keys.
    /// Lwrrently supported:
    /// "a" - amodel
    /// "p" - fermiperfsim by default, but controlled via knob
    virtual const char *GetModelIdentifierString() = 0;
};

/*
    Notifier structure used to report an amodel error

        BYTES   Meaning     Value
        0-7     Timer       0
        8-11    info32      13 (RM_RCH_GR_ERROR_SW_NOTIFY)
        12-13   info16      0
        14-15   status      0xffff
*/

#endif
