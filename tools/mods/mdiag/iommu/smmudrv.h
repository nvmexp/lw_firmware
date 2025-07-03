/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_SMMUDRV_H
#define INCLUDED_SMMUDRV_H

#include "iommudrv.h"

// Hopper ATS is implemented through Smmu driver directly.
// The interfaces defined in Smmu driver for mods:
// //hw/lwmobile/ip/clusters/mss_cluster/2.0/drv/drvapi/include/smmuv3_mods_api.h

// This class is a wrapper class for interfaces in smmu driver,
// which includes ATS/Inline smmu mapping related implementation.
class SmmuDrv : public IommuDrv
{
public:
    enum SmmuPageSize
    {
        SmmuPageSize_4_KB   =   IommuPageSize_4_KB,
        SmmuPageSize_64_KB  =  IommuPageSize_64_KB,
        SmmuPageSize_2_MB   =   IommuPageSize_2_MB,
        SmmuPageSize_512_MB = IommuPageSize_512_MB,
    };

    enum AtsPermission
    {
        ReadOnly,
        WriteOnly,
        ReadWrite
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

    virtual RC AllocDevVaSpace(const Pcie * pcie);
    virtual RC AllocProcessVaSpace(const Pcie * pcie, UINT32 pasid);
    virtual ~SmmuDrv();

    static void * s_SmmuDrvModule;

    friend IommuDrv* IommuDrv::GetIommuDrvPtr();

protected:
    // SmmuDrv object only can be created through IommuDrv::GetIommuDrvPtr()
    SmmuDrv();

private:
    RC Init();
    RC Cleanup();

    UINT32 GetBdf(const Pcie * pcie);
    RC ValidateTranslationConfig();
    AtsPermission ColwertAtsPermission(bool atsReadPermission, bool atsWritePermission);

    TransConfig m_SmmuStage;
    SmmuPageSize m_SmmuS2PageSize;
    bool m_BypassS1TranslationForInline;
};

#endif

