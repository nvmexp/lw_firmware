/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// DO NOT EDIT
// See https://wiki.lwpu.com/engwiki/index.php/MODS/sim_linkage#How_to_change_ifspec

#ifndef _IIDLIST_H_
#define _IIDLIST_H_

enum IID_TYPE {
    IID_QUERY_IFACE         = 0,
    IID_CHIP_IFACE          = 1,
    IID_MEMORY_IFACE        = 2,
    IID_IO_IFACE            = 3,
    IID_SYSSPEC_IFACE       = 4,
    IID_MSG_IFACE           = 5,
    IID_XMLCOMM_IFACE       = 6,
    IID_PRNTSVC_IFACE       = 7,
    IID_INTERRUPT_IFACE     = 8,
    IID_RAW_FB_IFACE        = 9,
    IID_BUFF_BACKDOOR_IFACE = 10,
    IID_RAM_TABLE_IFACE     = 11,
    IID_REGISTER_MAP_IFACE  = 12,
    IID_MAP_MEMORY_IFACE    = 13,
    IID_MSG_IFACE2          = 14,
    IID_VERSION_QUERY_IFACE = 15,
    IID_BUSMEM_IFACE        = 16,
    IID_LWSURFACE_IFACE     = 17,
    IID_TESTREPORT_IFACE    = 18,
    IID_CRCPROFILE_IFACE    = 19,
    IID_GPUVERIF_IFACE      = 20,
    IID_VIDMODEL_IFACE      = 21,
    IID_SIMULATION_IFACE    = 22,
    IID_HWMEM_IFACE         = 23,
    IID_BASHSTATE_IFACE     = 24,
    IID_REGTRACKER_IFACE    = 25,
    IID_SIMULATION2_IFACE   = 26,
    IID_EXCEPTION_IFACE     = 27,
    IID_GPUSERVICELIST_IFACE = 28,
    IID_QUEUEFACTORY_IFACE  = 29,
    IID_SOCKETDECODER_IFACE = 30,
    IID_AMODEL_BACKDOOR_IFACE = 31,
    IID_EXCEPTION2_IFACE     = 32,
    IID_MEMALLOC64_IFACE         = 33,
    IID_SIM_INTERFACE_IFACE  = 34,
    IID_SIM_INTERFACE_FACTORY_IFACE = 35,
    IID_AMODEL_BACKDOOR_IFACE_SOFTAMODEL = 36,
    IID_MULTIHEAP_IFACE         = 37,
    IID_GPUESCAPE_IFACE         = 38,
    IID_MAP_MEMORYEXT_IFACE     = 39,
    IID_GPUESCAPE2_IFACE        = 40,
    IID_DEVICE_DISCOVERY_IFACE  = 41,
    IID_INTERRUPT2_IFACE        = 42,
    IID_INTERRUPT3_IFACE        = 43,
    IID_INTERRUPT_MGR_IFACE     = 44,
    IID_INTERRUPT4_IFACE        = 45,
    IID_INTERRUPT2A_IFACE       = 46,
    IID_CLOCK_MGR_IFACE         = 47,
    IID_PCI_DEV_IFACE           = 48,
    IID_MULTIHEAP2_IFACE        = 49,
    IID_INTERRUPT_MASK_IFACE    = 50,
    IID_PPC_IFACE               = 51,
    IID_INTERRUPT_MGR2_IFACE    = 52,
    IID_CPUMODEL_IFACE          = 53,
    IID_CPUMODEL_CALLBACKS_IFACE = 54,
    IID_LWLINK_IFACE            = 55,
    IID_LWLINK_CALLBACKS_IFACE  = 56,
    IID_CPUMODEL2_IFACE          = 57,
    IID_CPUMODEL_CALLBACKS2_IFACE = 58,
    IID_INTERRUPT_MGR3_IFACE    = 59,
    IID_MAP_DMA_IFACE           = 60,
    IID_PCI_DEV2_IFACE          = 61,
    IID_LAST_IFACE          = 0xFFFF
};

#endif //  _IIDLIST_H_
