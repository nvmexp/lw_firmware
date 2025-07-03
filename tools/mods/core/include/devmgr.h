/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007, 2015, 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_DEVMGR_H
#define INCLUDED_DEVMGR_H

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

#ifndef INCLUDED_RC_H
#include "rc.h"
#endif

#ifndef INCLUDED_DEVICE_H
#include "device.h"
#endif

#ifndef _cl0080_h_
#include "class/cl0080.h" // LW0080_MAX_DEVICES
#endif

/*
 * This it the device manager base class.
 */

class DevMgr
{
  public:
    DevMgr();
    virtual ~DevMgr();

    virtual RC InitializeAll() = 0;
    virtual RC ShutdownAll() = 0;

    //! Optional function which can be implemented to perform exit procedures
    //! which differ from a usual ShutdownAll (i.e. unlinking for the GPU
    //! device manager).  This isn't called uniformly, and it is the
    //! responsibility of the implementor to ensure that it's called from
    //! the appropriate place in the code for the particular device manager.
    virtual void OnExit(){};

    virtual UINT32 NumDevices() = 0;

    virtual RC FindDevices() = 0;
    virtual void FreeDevices() = 0;

    virtual RC GetDevice(UINT32 index, Device **device) = 0;

    string GetName() const { return m_Name; }

  protected:
    void SetName(string name){ m_Name = name; }

  private:
    string m_Name;
};

#endif // INCLUDED_DEVMGR_H
