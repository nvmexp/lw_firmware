# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import os
import re
import math
import errno

from . import Datalogger
from . import Utilities
from . import Exceptions

_use_lwpex = True
_pci_location = ()
_chip         = ''
_gpu_index    = 'no_lwpex'
_gpc_mask = -1
_gpc_max_count = 0
_tpc_mask_str_lst = []
_fbp_mask = -1
_fbp_max_count = 0
_l2_mask_str_lst = []

def GetFsInfo():
    global _gpc_mask, _gpc_max_count, _tpc_mask_str_lst, _fbp_mask, _fbp_max_count, _l2_mask_str_lst
    try:
        with open(Utilities.GetModsLogLocation(), 'r') as mods_log:
            match_gpc_mask = re.compile('GPC\s*Mask\s*: (0x\S+) \((\d) GPCs\)').match
            match_tpc_mask = re.compile('TPC\s*Mask\s*: \[(.*)\]').match
            match_fbp_mask = re.compile('FB\s*Mask\s*: (0x\S+) \((\d) FB Partitions\)').match
            match_l2_mask = re.compile('L2\s*Mask\s*: \[(.*)\]').match
            for line in mods_log:
                match_gpc = match_gpc_mask(line)
                match_fbp = match_fbp_mask(line)
                if match_gpc:
                    _gpc_mask = int(match_gpc.group(1), 16)
                if match_fbp:
                    _fbp_mask = int(match_fbp.group(1), 16)
                match_tpc = match_tpc_mask(line)
                if match_tpc:
                    tpc_mask_str = match_tpc.group(1)
                    _tpc_mask_str_lst = tpc_mask_str.split()
                    _gpc_max_count = len(_tpc_mask_str_lst)
                match_l2 = match_l2_mask(line)
                if match_l2:
                    l2_mask_str = match_l2.group(1)
                    _l2_mask_str_lst = l2_mask_str.split()
                    _fbp_max_count = len(_l2_mask_str_lst)
    except OSError as e:
        if e.errno == errno.ENOENT:
            Datalogger.PrintLog('Log file {} not found'.format(Utilities.GetModsLogLocation()))
            raise Exceptions.FileNotFoundError()
        else:
            raise

    if _gpc_mask == -1:
        Datalogger.PrintLog('Cannot get GPC mask')
    if _tpc_mask_str_lst == []:
        Datalogger.PrintLog('Cannot get TPC mask')
    if _fbp_mask == -1:
        Datalogger.PrintLog('Cannot get FBP mask')
    if _l2_mask_str_lst == []:
        Datalogger.PrintLog('Cannot get L2 mask')

def GetPciInfo():
    global _pci_location, _chip, _gpu_index
    pci_location = ()
    did = 0
    ecid = ''
    raw_ecid = ''
    gpu_index = ''
    try:
        with open(Utilities.GetModsLogLocation(), 'r') as mods_log:
            match_pci_location = re.compile('PCI Location\s*: 0x(\S+), 0x(\S+), 0x(\S+), 0x(\S+)').match
            match_did = re.compile('GPU DID\s*: (\S+)').match
            for line in mods_log:
                m = match_pci_location(line)
                if m:
                    pci_location = (int(m.group(1),16),
                                    int(m.group(2),16),
                                    int(m.group(3),16),
                                    int(m.group(4),16))
                    continue
                m = match_did(line)
                if m and not did:
                    did = int(m.group(1),16)
                    continue
    except OSError as e:
        if e.errno == errno.ENOENT:
            Datalogger.PrintLog('Log file {} not found'.format(Utilities.GetModsLogLocation()))
            raise Exceptions.FileNotFoundError()
        else:
            raise

    if not pci_location:
        Datalogger.PrintLog('Cannot get pci location')

    if not did:
        Datalogger.PrintLog('Cannot get GPU DID')
    else:
        lwpex_index = 0
        lwpex_list = Utilities.CheckSubprocessOutput('./lwpex l').decode("utf-8")
        search_lw_dev = re.compile('\S+\s*\S+\s*\S+\s*00\s*10DE\s*(\S+)\s*Display controller').search
        for line in lwpex_list.splitlines():
            m = search_lw_dev(line)
            if m:
                if int(m.group(1),16) == did:
                    gpu_index = 'gpu{}'.format(lwpex_index)
                else:
                    lwpex_index = lwpex_index + 1
    if not gpu_index:
        Datalogger.PrintLog('Cannot get gpu index')

    if not pci_location and not gpu_index:
        Datalogger.PrintLog('No info to reset GPU using lwpex')
        raise Exceptions.FAException(2)

    _pci_location = pci_location
    _gpu_index    = gpu_index

def InitGpuResetHelper(use_lwpex, reset_on_init, mods_args, log_file, timeout):
    global _use_lwpex
    _use_lwpex = use_lwpex
    # This is mainly to accept args similar to 'only_pci_dev ..' or 'only_family' 
    # from the arg list + any other arguments which are needed for a basic MODS run.
    # -notest and should override actual test related arguments, if any
    additional_args = ' -disable_passfail_msg -notest -no_gold' +\
                        (' -reset_device' if reset_on_init else '')
    reset_args = mods_args.replace('gpugen.js', 'gputest.js')
    mods_reset_init = './mods -a -l {} {} {}'.format(log_file, reset_args, additional_args)
    Datalogger.PrintLog('... Initialising Gpu Reset')
    Utilities.ModsCall(mods_reset_init, timeout, False)
    if _use_lwpex:
        GetPciInfo()
    GetFsInfo()

def ResetGpu():
    if _use_lwpex:
        if _gpu_index == 'no_lwpex':
            return 0
        return_code = 1
        if os.path.exists('lwpex2'):
            if _pci_location:
                return_code = Utilities.RunSubprocess('./lwpex2 -b {:x}:{:x}:{:x}.{:x} --reset'.\
                                                       format(*_pci_location), 10000)
        if return_code != 0:
            if _gpu_index:
                return_code = Utilities.RunSubprocess('./lwpex hot_reset {}'.format(_gpu_index), 10)
        if return_code != 0:
            Datalogger.PrintLog('Reset FAILED. Could not reset GPU using lwpex')
            raise Exceptions.FAException(2)
