/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_FAKE_PROCESS_H
#define INCLUDED_FAKE_PROCESS_H
#include "core/include/types.h"

/**
 * @brief Provides simple process ID faking to RM
 *
 * This allows for tests to set the PID that will then be returned to RM
 * through osGetLwrrentProcess().  Tests can use this mechanism to test process
 * isolation and reporting in RM.
 *
 * Values are stored per-thread. A thread will inherit its initial value from
 * its parent.  Changing the PID in a thread will not affect its parent
 * thread's value.
 *
 * In RM 0 is a special value, it indicates a Root or Administrator process
 * with elevated privileges.
 *
 * This functionality is only included when INCLUDE_RMTEST is defined.
 */
namespace RMFakeProcess
{
    /**
     * @brief Get the faked process ID (0 if never set)
     *
     * @return current PID
     */
   UINT32 GetFakeProcessID();

   /**
    * @brief Set the fake process ID for the current thread.
    *
    * @param pid PID to set
    */
   void   SetFakeProcessID(UINT32 pid);

   /**
    * @brief A simple RAII implementation to allow using a given process ID within a given scope.
    *
    * Any child threads allocated while in the scope of a variable of this type
    * will inherit the parent's PID.  However when the variable goes out of
    * scope children threads' PID value will not be updated.
    */
   class AsFakeProcess
   {
       public:
           AsFakeProcess(UINT32);
           ~AsFakeProcess();
       private:
           AsFakeProcess(const AsFakeProcess&);
           AsFakeProcess& operator=(const AsFakeProcess&);
           UINT32 m_oldPid;
   };
}

#endif // INCLUDED_PROCESS_H
