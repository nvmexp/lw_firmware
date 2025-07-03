#!/usr/bin/elw python3
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import os
import sys
import json
import re

# This represents the BKV configuration for each UPHY version.  There can be different BKV
# arrays for different protocol speeds.  The speeds for LwLink should be either strings of speeds
# in the LwLink::SystemLineRate define or ALL.  Pcie speeds should be either strings of speeds
# from Pci::PcieLinkSpeed enum or ALL
g_BkvConfigs = [
    {
        "uphy_version" : 6.1,
        "src_files" : ["//dev/mixsig/uphy/6.1/rel/5.1/vmod/dlm/LW_UPHY_DLM_gold_ref_pkg.json",
                       "//dev/mixsig/uphy/6.1/rel/5.1/vmod/clm/LW_UPHY_CLM_gold_ref_pkg.json"],
        "dst_file" : "//sw/mods/shared_files/uphy/bkv/uphy_v6.1.bkv.json",
        "configs" :
        [
            {
                "protocol" : "LWHS",
                "CLM" : [ { "name" : "LW_UPHY_CLM_LWHS_gold_ref", "speeds" : [ "ALL" ] } ], 
                "DLM" : [ { "name" : "LW_UPHY_DLM_LWHS_PAM4_106P25G_gold_ref", "speeds" : [ "ALL" ] } ]
            },
            {
                "protocol" : "PCIE",
                "CLM" : [ { "name" : "LW_UPHY_CLM_PCIE_G1_G2_G3_G4_G5_gold_ref", "speeds" : [ "ALL" ] } ], 
                "DLM" : [ { "name" : "LW_UPHY_DLM_PCIE_G1_G2_G3_G4_G5_gold_ref", "speeds" : [ "ALL" ] } ]
            }
        ]
    }
];

# The UPHY teams use hex numbers in their JSON which are not supported in the JSON spec, if initial
# parsing of the JSON fails then colwert any hex data in the source to decimal and retry
def GetJsonData(file_data):
    bkv_data = dict()
    try:
        bkv_data = json.loads(file_data)
    except json.decoder.JSONDecodeError as js_err:
        print('Failed decode bkv file data : {}'.format(str(js_err)))
    except Exception as e:
        print('Failed process bkv file data: {}'.format(str(e)))
        return dict()

    if bkv_data:
        return ColwertRegsToNumbers(bkv_data)

    line_data = file_data.decode("utf-8", "ignore").rstrip().splitlines()
    for line in range(len(line_data)):
        m = re.match('(.*)addr" :[ ]+(\d+).*wdata" : ([0-9A-Fx]+).*wmask" : ([0-9A-Fx]+).*block" : ([0-9A-Fx]+)(.*)',
                     line_data[line])
        if m:
            line_data[line] = '{}addr" :  {}, "wdata" : {}, "wmask" : {}, "block" : {}{}\n'.format(m.group(1),
                                                                                                   m.group(2),
                                                                                                   int(m.group(3), 0),
                                                                                                   int(m.group(4), 0),
                                                                                                   int(m.group(5), 0),
                                                                                                   m.group(6))
    try:
        bkv_data = json.loads('\n'.join(line_data))
    except Exception as e:
        print('Failed process bkv file data: {}'.format(str(e)))
        return dict()
    return ColwertRegsToNumbers(bkv_data)

def ColwertRegsToNumbers(bkv_data):
    if bkv_data.get('protocols', None):
        for protocol_data in bkv_data['protocols']:
            if protocol_data.get('inits', None):
                for init in protocol_data['inits']:
                    if init.get('registers', None):
                        for register in init['registers']:
                            for k,v in register.items():
                                register[k] = int(v, 0)
    return bkv_data

# Generate a MODS Uphy BKV JSON file.  The difference between the HW BKV files and the MODS files
# is that the MODS files track all BKV versions ever submitted to allow for test flexibility
# while the HW files only have the latest BKV values
#
# This functino is used by a CRON job in order to automatically update MODS files so that MODS
# always has the latest BKV versions
#
# The HW BKV file has a top level BKV version and each protocol (LWHS, PCIE, etc) also has its
# own version. Whenever BKVs are upated the top level BKV version is always changed, however only
# protocol versions which have had their values changed will have their corresponding version
# updated
def GenerateUphyBkvJson(bkv_file_data, mods_bkv_file_data=None):

    bkv_json_data = [GetJsonData(bkv_data) for bkv_data in bkv_file_data]

    for json_data in bkv_json_data:
        if not json_data:
            raise('JSON data not found in source files')

    mods_bkv_data = dict()
    if mods_bkv_file_data:
        try:
            mods_bkv_data = json.loads(mods_bkv_file_data);
        except Exception as e:
            raise('Failed parse MODS BKV JSON : {}'.format(str(e)))

    # Both sets of data end up in the same MODS file
    for bkv_data in bkv_json_data:
        reg_type = ''
        if bkv_data['name'] == 'LW_UPHY_DLM':
            reg_type = 'DLM'
        elif bkv_data['name'] == 'LW_UPHY_CLM':
            reg_type = 'CLM'
        else:
            raise('Unknown bkv type'.format(bkv_data['name']))

        # Perform at least a minimal sanity check on the bkv data
        if bkv_data.get('bkv_major', None) is None or bkv_data.get('bkv_minor', None) is None or \
           bkv_data.get('protocols', None) is None or bkv_data.get('version_major', None) is None or \
           bkv_data.get('version_minor', None) is None:
            raise('BKV file {} invalid file format'.format(bkv_file))

        bkv_version = float(str(bkv_data['bkv_major']) + '.' + str(bkv_data['bkv_minor']))
        bkv_configs = []
        uphy_version = float(str(bkv_data['version_major']) + '.' + str(bkv_data['version_minor']))

        if mods_bkv_data.get('uphy_version', None) is None:
            mods_bkv_data['uphy_version'] = uphy_version
        if mods_bkv_data.get('protocols', None) is None:
            mods_bkv_data['protocols'] = dict()

        # Locate the BKV config for the lwrrnet uphy version.  The BKV files tend to have extra
        # unused data in them that MODS does not care about
        for bkv_config in g_BkvConfigs:
            if bkv_config['uphy_version'] == uphy_version:
                bkv_configs = bkv_config['configs']

        if not len(bkv_config):
            raise('BKV config for UPHY {} not found'.format(uphy_version))

        for config in bkv_configs:

            # If the MODS BKV data has not been created then initialize it so it can be
            # created
            if mods_bkv_data['protocols'].get(config['protocol'], None) is None:
                mods_bkv_data['protocols'][config['protocol']] = []
            mods_bkv_protocol = mods_bkv_data['protocols'][config['protocol']]

            # Find the protocol (e.g. LWHS, PCIE, USB, etc) specific BKV data
            protocol_bkv_data = dict()
            for bkv_protocol in bkv_data['protocols']:
                if bkv_protocol['name'] == config['protocol']:
                    protocol_bkv_data = bkv_protocol

            if not protocol_bkv_data:
                raise('Unable to find {} protocol in {}'.format(config['protocol'], bkv_file))

            # Sanity check the protocol data
            if protocol_bkv_data.get('bkv_major', None) is None or \
               protocol_bkv_data.get('bkv_minor', None) is None or \
               protocol_bkv_data.get('inits', None) is None:
                raise('Invalid {} protocol definition in {}'.format(config['protocol'], bkv_file))

            protocol_bkv_version = float(str(protocol_bkv_data['bkv_major']) + '.' +
                                        str(protocol_bkv_data['bkv_minor']))

            # Grab the BKV data for each register block type
            for reg_block in config[reg_type]:

                # Find the desired BKV init values from the HW file
                protocol_bkv_init = dict()
                for protocol_init in protocol_bkv_data['inits']:
                    if protocol_init['name'] == reg_block['name']:
                        protocol_bkv_init = protocol_init

                if not protocol_bkv_init:
                    raise('{} not found in {} protocol inits in {}'.format(reg_block['name'], config['protocol'], bkv_file))

                # Find the MODS BKV entry in the MODS data structure
                mods_bkv_entry = dict()
                for bkv_entry in mods_bkv_protocol:
                    if bkv_entry['name'] == reg_block['name']:
                        mods_bkv_entry = bkv_entry
                        print('Found entry for {}'.format(bkv_entry['name']))
                        break;

                # If one does not yet exist ccreate a new one by simply copying the one from
                # the HW file
                if not mods_bkv_entry:
                    mods_bkv_entry['name'] = reg_block['name']
                    mods_bkv_entry['type'] = reg_type
                    mods_bkv_entry['speeds'] = reg_block['speeds']
                    mods_bkv_entry['bkv_data'] = [ { 'protocol_version' : protocol_bkv_version,
                                                     'bkv_versions' : [ bkv_version ],
                                                     'registers' : protocol_bkv_init['registers'] } ]
                    mods_bkv_protocol.append(mods_bkv_entry)
                    continue

                # Find the MODS protocol BKV version if it exists
                mods_bkv_data_entry = dict()
                for bkv_data_entry in mods_bkv_entry['bkv_data']:
                    if bkv_data_entry['protocol_version'] == protocol_bkv_version:
                        mods_bkv_data_entry = bkv_data_entry
                        print('Found entry for protocol_version {}'.format(bkv_data_entry['protocol_version']))
                        break

                # If the protocol BKV version has not yet been captured by MODS then create it
                if not mods_bkv_data_entry:
                    # Sort all the MODS protocol BKVs by their version
                    sorted_bkv_data = sorted(mods_bkv_entry['bkv_data'], key=lambda x: x['protocol_version'])

                    # In order to optimize storage, the register list for each protocol BKV version
                    # is a delta from the previous version so create the list of registers by
                    # merging together all the previous register lists
                    prev_registers = []
                    for prev_bkv_data in sorted_bkv_data:
                        if not len(prev_registers):
                            prev_registers = prev_bkv_data['registers']
                        else:
                            # This line merges the current register list into the previous list
                            # overwriting any registers with the same address with the data from
                            # the current list
                            prev_registers = list({x['addr']:x for x in prev_registers + prev_bkv_data['registers']}.values())

                    # Remove any registers that are already specified from the previous register
                    # lists from the current list and create a new entry
                    new_registers = [x for x in protocol_bkv_init['registers'] if x not in prev_registers]
                    mods_bkv_entry['bkv_data'].append({ 'protocol_version' : protocol_bkv_version,
                                                        'bkv_versions' : [ bkv_version ],
                                                        'registers' : new_registers })
                elif bkv_version not in mods_bkv_data_entry['bkv_versions']:
                    # Otherwise this protocol version has already been parsed in a previous
                    # BKV version so add the current BKV version to the list of BKV versions
                    # which make use of this protocol version
                    mods_bkv_data_entry['bkv_versions'].append(bkv_version)

    # Colwert the data to a JSON string and return it
    try:
        return json.dumps(mods_bkv_data, indent=4).encode("utf-8", "ignore");
    except Exception as e:
        raise('Failed to colwert bkv data to json : {}'.format(str(e)))
