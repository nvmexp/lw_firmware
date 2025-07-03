# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2016,2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

from floorsweep_libs import Exceptions
from functools import reduce

class SkuConfig(object):
    def __init__(self, config, subgroup_size=0, max_config_size=0):
        self.attr = config['ATTRIBUTE']
        self.val  = config['VAL']
        
        if self.attr == 'MATCHVAL':
            if isinstance(self.val,list):
                # The value is already an array of subgroup masks like Volta+ TPC
                return
            if subgroup_size == 0:
                # The config is just a single mask like GPC
                return
            # expand into array of subgroup masks
            config_val = self.val
            config_list = []
            subgroup_mask = (1 << subgroup_size) - 1
            for i in range(0,max_config_size,subgroup_size):
                submask = (config_val >> i) & subgroup_mask
                config_list.append(submask)
            self.val = config_list
            return
        elif self.attr == 'BITCOUNT':
            pass
        elif self.attr == 'DONTCARE':
            pass
        else:
            raise Exceptions.SoftwareError("Unrecognized fuse config attribute: {}".format(self.attr))

    def __str__(self):
        if self.attr == 'MATCHVAL':
            if isinstance(self.val,list):
                return "MATCHVAL = {}".format(['{:#x}'.format(x) for x in reversed(self.val)])
            else:
                return "MATCHVAL = {:#x}".format(self.val)
        elif self.attr == 'BITCOUNT':
            return "BITCOUNT = {:#x}".format(self.val)
        elif self.attr == 'DONTCARE':
            return "DON'T CARE"
        else:
            raise Exceptions.SoftwareError("Unrecognized sku config attribute: {}".format(self.attr))
        
    def CheckConfig(self, fs_mask):
        if self.attr == 'MATCHVAL':
            sku_mask = self.val
            if fs_mask == sku_mask:
                return 0
            if isinstance(fs_mask,list):
                greater = False
                for (fs,sku) in zip(fs_mask,sku_mask):
                    if ((fs^sku)&fs):
                        greater = True
                return 1 if greater else -1
            else:
                if not ((fs_mask^sku_mask)&fs_mask):
                    return -1
                else:
                    return 1
        elif self.attr == 'BITCOUNT':
            sku_count = self.val
            if isinstance(fs_mask,list):
                fs_count = reduce(lambda x,y: x + bin(y).count('1'), fs_mask, 0) 
            else:
                fs_count = bin(fs_mask).count('1')
            if fs_count == sku_count:
                return 0
            elif fs_count < sku_count:
                return -1
            else:
                return 1
        elif self.attr == 'DONTCARE':
            return 0
        else:
            raise Exceptions.SoftwareError("Unrecognized sku config attribute: {}".format(self.attr))
