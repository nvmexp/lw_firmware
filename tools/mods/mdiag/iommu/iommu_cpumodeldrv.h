/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_IOMMU_CPUMODELDRV_H
#define INCLUDED_IOMMU_CPUMODELDRV_H

#include "iommudrv.h"

// Mods emulates NMMU(Nest MMU, for IBM p9) with cpu model.
// This class is designed for mods' implementation for p9 ATS.
class IommuCpuModelDrv : public IommuDrv
{
public:
    virtual RC CreateAtsMapping
    (
        const string & surfaceName,
        const string & addressSpaceName,
        const string & aperture,
        const Pcie * pcie,
        UINT32 testId,
        UINT32 pasid,
        UINT64 atsPageSize,
        UINT64 virtAddr,
        UINT64 physAddr,
        bool atsReadPermission,
        bool atsWritePermission
    );

    virtual RC UpdateAtsMapping
    (
        const string & addressSpaceName,
        const string & aperture,
        const Pcie * pcie,
        UINT32 pasid,
        UINT64 virtAddr,
        UINT64 physAddr,
        MappingType updateType,
        bool atsReadPermission,
        bool atsWritePermission
    );

    virtual RC UpdateAtsPermission
    (
        const string & addressSpaceName,
        const string & permissionType,
        const Pcie * pcie,
        UINT32 pasid,
        UINT64 virtAddr,
        UINT64 physAddr,
        bool permissiolwalue,
        bool atsReadPermission,
        bool atsWritePermission
    );

    virtual RC SendGmmuMapping
    (
        const string & surfaceName,
        const string & addressSpaceName,
        const string & aperture,
        UINT32 testId,
        UINT32 pasid,
        UINT32 pageSize,
        UINT64 virtAddr,
        UINT64 physAddr,
        bool doUnmap
    );

    virtual ~IommuCpuModelDrv();

    friend IommuDrv* IommuDrv::GetIommuDrvPtr();

protected:
    // IommuCpuModelDrv object only can be created through IommuDrv::GetIommuDrvPtr()
    IommuCpuModelDrv();

private:
    UINT32 m_Gfid;
};

#endif

