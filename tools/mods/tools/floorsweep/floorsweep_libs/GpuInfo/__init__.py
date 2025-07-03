# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import os
import re
import json
import glob

from floorsweep_libs import GpuList
from floorsweep_libs import Utilities
from floorsweep_libs import Datalogger
from floorsweep_libs import Exceptions

from . import GpuInfo
from . import GP10xGpuInfo
from . import GA100GpuInfo
from . import GA10xGpuInfo
from . import GH100GpuInfo
GpuInfo = GpuInfo.GpuInfo
GP100GpuInfo = GP10xGpuInfo.GP100GpuInfo
GP104GpuInfo = GP10xGpuInfo.GP104GpuInfo
GP107GpuInfo = GP10xGpuInfo.GP107GpuInfo
GP10xGpuInfo = GP10xGpuInfo.GP10xGpuInfo
GA100GpuInfo = GA100GpuInfo.GA100GpuInfo
GA10xGpuInfo = GA10xGpuInfo.GA10xGpuInfo
GH100GpuInfo = GH100GpuInfo.GH100GpuInfo

@Datalogger.LogFunction
def CreateGpuInfo(sku, fuse_post, no_lwpex):
    initial_json_glob = sorted(glob.glob('mods*.jso'))
    if sku:
        Utilities.ModsCall('fuseblow.js', '-disable_passfail_msg -skip_rm_state_init -display_fuses -fb_broken'
                                          ' -dump_fs_info {} -devid_fs_war -json {}'.format(sku, fuse_post), 60)
    else:
        Utilities.ModsCall('fuseblow.js', '-disable_passfail_msg -skip_rm_state_init -dump_fuses -fb_broken'
                                          ' -devid_fs_war', 60)

    fuseinfo = {}
    pci_location = ()
    chip_name = ""
    did = 0
    ecid = ""
    raw_ecid = ""
    with open(Utilities.GetModsLogLocation()) as mods_log:
        search_fuse = re.compile("Fuse (\S+)\s*:\s*(\S+)").search
        match_pci_location = re.compile("PCI Location\s*: 0x(\S+), 0x(\S+), 0x(\S+), 0x(\S+)").match
        match_chip_name = re.compile("Device Id\s*: (\S+)").match
        match_did = re.compile("GPU DID\s*: (\S+)").match
        match_ecid = re.compile("^ECID\s*: (\S+)").match
        match_rawecid = re.compile("Raw ECID\s*: (\S+)").match
        for line in mods_log:
            m = search_fuse(line)
            if m:
                fuseinfo[m.group(1)] = m.group(2)
                continue
            m = match_pci_location(line)
            if m:
                pci_location = (int(m.group(1),16),
                                int(m.group(2),16),
                                int(m.group(3),16),
                                int(m.group(4),16))
                continue
            m = match_chip_name(line)
            if m:
                chip_name = m.group(1).strip()
                continue
            m = match_did(line)
            if m and not did:
                did = int(m.group(1),16)
                continue
            m = match_ecid(line)
            if m:
                ecid = m.group(1)
                continue
            m = match_rawecid(line)
            if m:
                raw_ecid = m.group(1)
                continue
    if not fuseinfo:
        raise Exceptions.SoftwareError("Cannot get fuse info")
    if not pci_location:
        raise Exceptions.SoftwareError("Cannot get pci location")
    if not chip_name:
        raise Exceptions.SoftwareError("Cannot get chip name")
    if not did:
        raise Exceptions.SoftwareError("Cannot get GPU DID")

    gpu_index = "no_lwpex"
    if not no_lwpex:
        gpu_index = ""
        lwpex_index = 0
        lwpex_list = Utilities.CheckSubprocessOutput('./lwpex l')
        search_lw_dev = re.compile("\S+\s*\S+\s*\S+\s*00\s*10DE\s*(\S+)\s*Display controller").search
        for line in lwpex_list.splitlines():
            m = search_lw_dev(line)
            if m:
                if int(m.group(1),16) == did:
                    gpu_index = "gpu{}".format(lwpex_index)
                else:
                    lwpex_index = lwpex_index + 1
        if not gpu_index:
            raise Exceptions.SoftwareError("Cannot get gpu index")

    skuinfo = {}
    if sku:
        json_glob = sorted(glob.glob('mods*.jso'))
        if not json_glob or len(json_glob) <= len(initial_json_glob):
            raise Exceptions.SoftwareError("Cannot get sku info")
        latest_json = json_glob[-1]
        with open(latest_json) as results:
            info = json.load(results)
            for entry in info:
                if entry['__tag__'] == 'DumpFsInfo':
                    skuinfo = entry
                    break
        if not skuinfo:
            raise Exceptions.SoftwareError("Cannot get sku info")

    Datalogger.LogInfo(fuseinfo=fuseinfo)
    Datalogger.LogInfo(pci_location=pci_location)
    Datalogger.LogInfo(chip_name=chip_name)
    Datalogger.LogInfo(did="{:#x}".format(did))
    Datalogger.LogInfo(ecid=ecid)
    Datalogger.LogInfo(raw_ecid=raw_ecid)
    Datalogger.LogInfo(skuinfo=skuinfo)

    fsinfo_class, gpuinfo_class = GpuList.GetGpuClasses(chip_name)
    fsinfo = fsinfo_class(fuseinfo = fuseinfo)
    gpuinfo = gpuinfo_class(fsinfo, chip_name, pci_location, gpu_index, sku, skuinfo)

    return gpuinfo
