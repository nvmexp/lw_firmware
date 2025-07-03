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

#ifndef INCLUDED_IOMMUDRV_H
#define INCLUDED_IOMMUDRV_H

#include <memory>

class RC;
class Pcie;

// This class is an abstract class designed for Iommu layer,
// which defines basic iommu interfaces.
class IommuDrv
{
public:
    enum MappingType
    {
        UnmapPage,
        RemapPage
    };

    enum TransConfig
    {
        BYPASS=0x0,
        STAGE1_ONLY,
        STAGE2_ONLY,
        STAGE1_STAGE2
    };

    enum IommuPageSize : UINT64
    {
        IommuPageSize_4_KB   =   4_KB,
        IommuPageSize_64_KB  =  64_KB,
        IommuPageSize_2_MB   =   2_MB,
        IommuPageSize_512_MB = 512_MB,
    };

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
    ) = 0;

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
    ) = 0;

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
    ) = 0;

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

    // Initialize smmu stage 2 translation per device/Stream 
    virtual RC AllocDevVaSpace(const Pcie * pcie);

    // Initialize smmu per context/process/substream 
    virtual RC AllocProcessVaSpace(const Pcie * pcie, UINT32 pasid);

    virtual ~IommuDrv();
   
    static IommuDrv* GetIommuDrvPtr();
    static void Destroy();
private:
    static unique_ptr<IommuDrv> s_IommuDrv;
};

#endif

