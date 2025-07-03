/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef VASPACE_H
#define VASPACE_H

#include <string>
using std::string;
#include "mdiag/utils/types.h"
#include "core/include/lwrm.h"
class LWGpuResource;

class VaSpace
{
public:
    enum class Index
    {
        NEW,
        FLA
    };

    VaSpace(LWGpuResource * pGpuRes,
            LwRm* pLwRm,
            UINT32 testId,
            string name);
    string GetName() const { return m_Name; }
    ~VaSpace();
    LwRm::Handle GetHandle();
    void SetRange(UINT64 base, UINT64 size);
    UINT32 GetPasid() const { return m_Pasid; }
    void SetAtsEnabled() { m_AtsEnabled = true; }
    bool IsAtsEnabled() const { return m_AtsEnabled; }
    void SetIndex(Index index) { m_Index = index; }
    Index GetIndex() { return m_Index; }
    LwRm * GetLwRmPtr() const { return m_pLwRm;   }
private:
    RC ControlFlaRange(UINT32 mode);

    LWGpuResource * m_pGpuResource;
    LwRm* m_pLwRm;
    UINT32 m_TestId;
    string m_Name;
    LwRm::Handle m_Handle;
    UINT32 m_Pasid;
    bool m_AtsEnabled;
    Index m_Index = Index::NEW;
    UINT64 m_Base = 0;
    UINT64 m_Size = 0;
};
#endif

