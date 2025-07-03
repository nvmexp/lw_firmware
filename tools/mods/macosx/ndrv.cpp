 /*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------
//------------------------------------------

/*************************************************************
*                                                            *
*   NDRV interface, taken from                               *
*   //sw/misc/shuaulme/apps/ndrvPrivateCall/...              *
*                                                            *
*   This is Gamma disable specific, but there should be a    *
*    generic interface in case we need to use other NDRV     *
*    features                                                *
*                                                            *
*************************************************************/

#include "core/include/tee.h"
#include "core/include/rc.h"
#include "lwmisc.h"
#include "gpu/include/gpumgr.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "lwos.h"
#include "LWDAUserApi.h"
#include <string.h>
#include <vector>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include "class/cl0080.h" // LW01_DEVICE_0
#include "class/cl2080.h" // LW20_SUBDEVICE_0

#define cscPrivateControlCall 26    /* From IONDRVFramebuffer.h */

#define SET_GAMMA_ADJUST_DISABLER         1010
typedef struct
{
    LwU32  sel;
    LwU32  status;
} SET_GAMMA_ADJUST_DISABLER_PARAMS;

//
// EVO_MOD_CORE_RESOURCES_ACTION
// This flag indicates whether to release the core channel and
// associated resources from the driver (when MODS is taking over) or
// whether to restore the driver resources (when MODS exits)
//
#define EVO_MOD_CORE_RESOURCES            1025

#define LW_EVO_MOD_CORE_RESOURCES_ACTION            1:0
#define LW_EVO_MOD_CORE_RESOURCES_ACTION_RELEASE      0
#define LW_EVO_MOD_CORE_RESOURCES_ACTION_RESTORE      1
typedef struct
{
    LwU32   sel;
    LwU32   flags;
} EVO_MOD_CORE_RESOURCES_PARAMS;

enum
{
    kIONDRVControlCommand             = 128 + 4,
};

typedef struct
{
    string key;
    UINT32 value;
} matchEntry;

// we have only one LWDA connect here, we need one per instance...
static vector<io_service_t> lw_service;
static vector<io_connect_t> lw_connect;
static vector<matchEntry> matchParamList;

enum
{
    NDRV_PARAM_DIRECT = 128,
    NDRV_PARAM_PTR,
    NDRV_PARAM_PRIVATE
};

typedef struct
{
    LwU32      command;
    LwU32      code;
    IOReturn   result;
    LwU32      paramType;
    LwU32      paramSize;
    LwU32      params[256];
} ndrv_parameters_t;

static IOReturn NdrvControl
(
    LwU32 code,
    LwU32 type,
    void * data,
    LwU32 dataSize
)
{
    ndrv_parameters_t params;
    size_t theParamSize;
    IOReturn retval = kIOReturnSuccess;

    if ( dataSize < sizeof( params.params ) )
    {
        for (vector<io_connect_t>::iterator iter = lw_connect.begin();
            iter != lw_connect.end();
            iter++)
        {
            params.command = kIONDRVControlCommand;
            params.code = code;
            params.paramType = type;
            params.paramSize = dataSize;

            theParamSize = dataSize + sizeof(ndrv_parameters_t) - sizeof(params.params);
            memcpy( &params.params, data, dataSize );

            IOConnectCallStructMethod(*iter,
                                      LWRM_TRANSPORT_NDRV_API,
                                      &params, theParamSize,
                                      &params, &theParamSize);

            retval = params.result;
        }
    }
    else
    {
        Printf(Tee::PriLow,
               "NdrvControl: Invalid dataSize %u.  Should be less than %ld.",
               (UINT32)dataSize, sizeof(params.params));
        retval = kIOReturnError;
    }

    return retval;
}

static IOReturn GetServices()
{
    IOReturn kr;
    mach_port_t    device_master_port = MACH_PORT_NULL;
    __CFDictionary *matching          = NULL;
    io_iterator_t  iterator           = IO_OBJECT_NULL;
    io_service_t   service            = 0;

    kr = IOMasterPort(bootstrap_port, &device_master_port);
    if (kr)
    {
        Printf(Tee::PriNormal, "IOMasterPort failed with %x\n", kr);
        return kr;
    }

    matching = IOServiceMatching("LWDA");
    if ( ! matching)
    {
        Printf(Tee::PriNormal, "IOServiceMatching returned NULL\n");
        return 1;
    }

    kr = IOServiceGetMatchingServices(device_master_port, matching, &iterator);
    if (kr)
    {
        Printf(Tee::PriNormal, "IOServiceGetMatchingServices failed with %x\n", kr);
        return kr;
    }

    while ((service = IOIteratorNext(iterator)) != 0)
    {
        lw_service.push_back(service);
    }

    return kr;
}

static IOReturn GetConnect(io_service_t service,
                           io_connect_t *connect)
{
    IOReturn kr;

    // Try to allocate a MODS-specific LWDA UserClient (0x80000001).
    kr = IOServiceOpen(service, mach_task_self(), 0x80000001, connect);
    if (kr)
    {
        Printf(Tee::PriNormal, "IOServiceOpen(0x80000001) failed with 0x%x; trying legacy fallback...\n", kr);

        // Try to allocate a general-purpose LWDA UserClient (0x80000000).
        kr = IOServiceOpen(service, mach_task_self(), 0x80000000, connect);
        if (kr)
        {
            Printf(Tee::PriNormal, "IOServiceOpen(0x80000000) failed with 0x%x\n", kr);
        }
    }

    return kr;
}

//Internal routine to generate a key-value list to validate I/O registry against
static void InitPropertyList()
{
    if (matchParamList.size() == 0)
    {
        // Create a list to match the I/O registry against
        matchEntry matchParam;

        matchParam.key = string("LWDA,DeviceClass");
        matchParam.value = (UINT32)LW01_DEVICE_0;
        matchParamList.push_back(matchParam);

        matchParam.key = string("LWDA,SubDeviceClass");
        matchParam.value = (UINT32)LW20_SUBDEVICE_0;
        matchParamList.push_back(matchParam);
    }
    return;
}

// Helper routine to validate "services" in the I/O registry database
// Each service is mapped to a display head.
// This function verifies the Device and SubDevice class IDs published
// by RM are correct to validate each display service
static bool IsValidGpuService(io_service_t device)
{
    CFIndex numBytes = 0;
    UINT32 valueFound = 0;
    UINT32 hits = 0; // counter to track if both properties matched
    UINT08 *pDataBytes = NULL;
    CFTypeRef data = NULL;

    InitPropertyList();

    for (UINT32 idx = 0; idx < matchParamList.size() ; idx++)
    {
        CFStringRef paramStr = CFStringCreateWithCString(kCFAllocatorDefault,
                                                         (matchParamList[idx].key).c_str(),
                                                         kCFStringEncodingMacRoman);

        // Get I/O registry entry based on key
        data = IORegistryEntrySearchCFProperty(device,
                                               kIOServicePlane,
                                               paramStr,
                                               kCFAllocatorDefault,
                                               kIORegistryIterateRelwrsively | kIORegistryIterateParents);
        CFRelease(paramStr);

        if (!data ||  // null check
            (CFGetTypeID(data) != CFDataGetTypeID())) // type check
        {
            Printf(Tee::PriLow,
                   "** Could not find %s **\n",
                   matchParamList[idx].key.c_str());
            break;
        }

        numBytes = CFDataGetLength((CFDataRef)data);
        pDataBytes = (UINT08 *)malloc((size_t)numBytes);

        CFDataGetBytes((CFDataRef)data,
                       CFRangeMake(0,numBytes),
                       (UInt8 *)pDataBytes);

        if (numBytes == 4)
        {
            // Construct 32-bit int from the data bytes read
            valueFound =  (*(pDataBytes + 0) |
                           *(pDataBytes + 1) << 8  |
                           *(pDataBytes + 2) << 16 |
                           *(pDataBytes + 3) << 24);
        }
        else
        {
            Printf(Tee::PriLow,
                   "** Only 4-byte values supported, found %d bytes **\n",
                   (UINT32)numBytes);
            break;
        }

        // Free references for next registry lookup
        free(pDataBytes);
        CFRelease(data);
        data = pDataBytes = NULL;

        if (valueFound != matchParamList[idx].value)
        {
            Printf(Tee::PriHigh,
                   "Incorrect value 0x%08x for key %s, expected 0x%08x \n",
                   valueFound,
                   matchParamList[idx].key.c_str(),
                   matchParamList[idx].value);

            break;
        }
        else
            hits++;
    }

    // Free any allocated resources
    if (data)
        CFRelease(data);
    if (pDataBytes)
        free(pDataBytes);

    if (hits == matchParamList.size()) // all params matched
        return true;
    else
        return false;
}

/*
 * Initialize the mappings and open the control device
 */
static bool init_api_layer(void)
{
    IOReturn             kr;
    io_connect_t         connect;
    int                  counter;

    kr = GetServices();
    if (kr != kIOReturnSuccess)
        return false;

    counter = 0;
    for (vector<io_service_t>::iterator iter = lw_service.begin();
         iter != lw_service.end();
         iter++)
    {
        if (!IsValidGpuService(*iter))
            continue;

        kr = GetConnect(*iter, &connect);
        if (kr != kIOReturnSuccess)
        {
            Printf(Tee::PriHigh, "NDRV: failed to connect to NDRV %d\n", counter);
            return false;
        }

        lw_connect.push_back(connect);

        counter++;
    }

    // Fail we could not find any LWDA user client
    if (0 == lw_connect.size())
        return false;

    return true;
}

//--------------------------------------------------------------------
//! \brief Connect to the RM/NDRV
//!
//! This function establishes a connection to the resman.  It must be
//! called before MODS runs any GPU tests.
//!
RC NdrvInit()
{
    if (lw_connect.size() == 0)
    {
        // Initialize the mappings and open the control device
        //
        if ( ! init_api_layer() )
        {
            Printf(Tee::PriHigh, "ERR: LWDA could not be found ... \n");
            return RC::SOFTWARE_ERROR;
        }
    }

    return OK;
}

//--------------------------------------------------------------------
//! \brief Close connection to RM/NDRV
//!
//! This function closes the connection to RM/NDRV established by
//! NdrvInit. It must be closed before OS graphics APIs are expected
//! to resume (they will block at the RM/NDRV API while MODS is
//! active).
//!
RC NdrvClose()
{
    while (lw_connect.size() > 0)
    {
        io_connect_t connection = lw_connect.back();
        if (kIOReturnSuccess != IOServiceClose(connection))
        {
            Printf(Tee::PriHigh, "ERR: failed to close LWDA connection\n");
        }
        lw_connect.pop_back();
    }

    return OK;
}

//--------------------------------------------------------------------
//! \brief Release or restore the driver's core channel
//!
//! There can only be one EVO core channel at a time, so MODS must
//! release the driver's core channel before MODS can create its own.
//! This function should be called on MODS entry/exit to
//! release/restore the driver's core channel, respectively.
//!
//! This function is optional if MODS doesn't do any display control,
//! such as when mods is called with -null_display or -h.
//!
//! \param release true to release the driver's channel, false to
//! restore it.
//!
RC NdrvReleaseCoreChannel(bool release)
{
    IOReturn kr;
    EVO_MOD_CORE_RESOURCES_PARAMS params = {0};

    params.sel = EVO_MOD_CORE_RESOURCES;
    if (release)
        params.flags = DRF_DEF(_EVO_MOD, _CORE_RESOURCES, _ACTION, _RELEASE);
    else
        params.flags = DRF_DEF(_EVO_MOD, _CORE_RESOURCES, _ACTION, _RESTORE);

    kr = NdrvControl(cscPrivateControlCall, NDRV_PARAM_PRIVATE,
                     &params, sizeof(params));
    if (kr != kIOReturnSuccess)
    {
        Printf(Tee::PriHigh, "NDRV: failed to %s the Evo Core Channel (kr = 0x%x)\n",
               release ? "release" : "restore", kr);
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

// Todo: replace with a more abstract interface.
//  pNDRV->Set(SET_GAMMA_ADJUST_DISABLER) or something
RC NdrvGamma(bool disabled)
{
    IOReturn      kr;

    SET_GAMMA_ADJUST_DISABLER_PARAMS theParam = {0};

    theParam.sel = SET_GAMMA_ADJUST_DISABLER;
    theParam.status = (LwU32)disabled;

    //calling the NDRV to set the gamma adjustement disabler
    kr = NdrvControl(cscPrivateControlCall, NDRV_PARAM_PRIVATE,
                     &theParam, sizeof(theParam));
    if (kr != kIOReturnSuccess)
    {
        Printf(Tee::PriHigh,
               "NDRV: failed to connect to NDRV for Gamma Disable (kr = 0x%x)\n",
               kr);
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}
