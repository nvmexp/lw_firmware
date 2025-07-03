#!/usr/bin/python3

import os
import sys
import argparse
import abc
import pathlib
import re
import ast

from pprint      import pprint
from xml.etree   import ElementTree
from typing      import List, Set
from vfield_util import MacroParser
from vfield_util import VFieldIdParser
from vfield_util import VFieldTableParser
from vfield_util import groupChipsByVfieldTbl
from vfield_util import generateVfieldCTbl
from vfield_util import generateChipGroup
RegexPattern = str

### Configurable Options ###
IGNORE_CORES = [
    r'core[3456](r[a-z0-9]+)?',  r'core70', r'core75', # very old and unsupported (Fermi and prior)
    r'core80', r'core82', r'core84', # Maxwell (we are targeting Pascal and later)
]

IGNORE_CHIPS = [
]

INC_FILES = [
    'dev_fuse.inc',
    'dev_gc6_island.inc',
    'dev_bus.inc'
]

P4ROOT     = pathlib.Path(os.elwiron.get('P4ROOT'))
HWREF_PATH = P4ROOT / 'sw/dev/gpu_drv/chips_a/drivers/common/inc/hwref'
### End of Configurable Options ###

def eprint(fmt: str, *pos, **kwargs):
    if 'file' not in kwargs:
        kwargs['file'] = sys.stdout
    print(fmt.format(*pos), **kwargs)

# group 1 => /architecture/chip/header_file
REGX_DEFINE_PATH = r'^; The following definitions are from .+\\(.+\\.+\\.+)\.'

def loadDefines(chipdir):
    definesMap = {}

    def findDefineFile(incFile):
        with open(incFile, 'r') as f:
            for line in f.readlines():
                m = re.search(pattern=REGX_DEFINE_PATH, string=line)
                if m:
                    # group 1 is path
                    return  m.group(1)

    parser = MacroParser()
    for incFile in INC_FILES:
        definePath = HWREF_PATH / findDefineFile(chipdir / 'inc' / incFile)

        # Chip doesn't exist, return empty dict
        if not definePath.exists():
            return dict()

        definesMap.update(parser.parse(definePath))

    return definesMap

def scanAndParseChip(biosroot         : pathlib.Path,
                     chipdir          : pathlib.Path,
                     vfieldIdMap                    ):
    """
    Scans and parses dev_.*.h files that hold required macro definitions and
    then parses vfieldtbl_sci.asm to get actual VFIELD addresses

    Returns:
        Map{VfieldName : Address}
    """

    assert chipdir.is_dir()

    definesMap  = loadDefines(chipdir)
    # For now no need to parse scratch.xml file
    # So we can assume that these all have same value(0x1F = 31):
    # 1) SRT_BOARD_BINNING_ID_PRIVIDX
    # 2) SRT_BOARD_BINNING_REVISION_PRIVIDX
    # 3) SRT_BOARD_BINNING_VALUE_PRIVIDX
    definesMap['SRT_BOARD_BINNING_ID_PRIVIDX']       = 0x1F
    definesMap['SRT_BOARD_BINNING_REVISION_PRIVIDX'] = 0x1F
    definesMap['SRT_BOARD_BINNING_VALUE_PRIVIDX']    = 0x1F

    parser          = VFieldTableParser(definesMap, vfieldIdMap)
    vfieldValuesMap = parser.parse(chipdir / '../../core/vfieldtbl_sci.asm')

    return vfieldValuesMap

def scanAndParseAllChips(biosroot    : pathlib.Path      ,
                         ignore_cores: List[RegexPattern],
                         ignore_chips: List[RegexPattern]):
    """
    Returns dict of chip -> vfield id -> reg addr.
    """
    chips       = {}
    vfieldIdParser = VFieldIdParser()
    for coredir in biosroot.iterdir():
        # Skip directories that aren't core
        if not coredir.is_dir():
            continue
        if not coredir.name.startswith('core'):
            continue

        core = coredir.name

        # Skip if we're ignoring this core
        if any(re.match(pat, core) for pat in ignore_cores):
            eprint('=== Ignoring {} ===', core)
            continue

        chiproot = coredir / 'chip'
        if not chiproot.is_dir():
            eprint('Note: {} has no "chip" directory', core)
            continue

        vfieldIdMap    = vfieldIdParser.parse(coredir / 'inc' / 'vfielddef.inc')
        for chipdir in chiproot.iterdir():
            # We're looking for chip directories
            if not chipdir.is_dir():
                continue

            chip = chipdir.name

            # Skip if we're ignoring this chip
            if any(re.match(pat, chip) for pat in ignore_chips):
                eprint('=== Ignoring {} chip {} ===', core, chip)
                continue

            eprint('')
            eprint('=== Scanning {} chip {} ===', core, chip)

            
            # We want it reversed so we can map ID to value
            ilw_vfield_map = { v : k for k, v in vfieldIdMap.items() }
            # Get Max ID to know how many entries we need

            chip_vals  = scanAndParseChip(biosroot=biosroot,
                                          chipdir=chipdir,
                                          vfieldIdMap=vfieldIdMap)

            if chip_vals == {}:
                eprint('Warning: nothing found for {}', chip)
            else:
                # Replace symbolic names of IDs with their actual ID values
                # Basically create mapping VFIELD_ID : REG_ADDR
                chips[chip] = { k : chip_vals[v] for k, v in ilw_vfield_map.items() 
                                if v in chip_vals }

    return chips

def myMain():
    if not P4ROOT:
        raise Exception('P4ROOT does not exist!')

    p4root = P4ROOT
    if not p4root.is_dir():
        raise Exception('P4ROOT is not a directory!')

    biosroot = P4ROOT / 'sw/main/bios'
    if not biosroot.exists():
        raise Exception('$P4ROOT/sw/main/bios does not exist.\n Please map it to your workspace')

    chips = scanAndParseAllChips(biosroot=biosroot,
                                 ignore_cores=IGNORE_CORES,
                                 ignore_chips=IGNORE_CHIPS)

    print()
    print('Generating tables\n')
    # Group chips by VFIELD_ID => REG_ADDR
    variants = groupChipsByVfieldTbl(chips)
    for vfield_tbl, chip_list in variants.items():
        # Colwert it back to dict
        vfield_tbl = dict(vfield_tbl)

        # Find the highest Vfield ID.
        # this will be the size of table
        num_vfields = max(vfield_tbl.keys()) + 1
        lookup_tbl = ['0x00000000'] * num_vfields
        for vfield_id, reg_addr in vfield_tbl.items():
            lookup_tbl[vfield_id] = reg_addr

        print('Generated table for:\n {}'.format(
            generateChipGroup(chip_list)
        ))

        print(generateVfieldCTbl(lookup_tbl))
        print()


if __name__ == "__main__":
    try:
        myMain()
    except KeyboardInterrupt:
        sys.exit(130)
    except Exception as e:
        print('Error: {}'.format(e))
