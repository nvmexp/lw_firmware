#!/usr/bin/elw python

#
# Copyright (c) 2018-2019, LWPU CORPORATION. All rights reserved.
# LWPU CONFIDENTIAL. Prepared and Provided Under NDA.
#

#
# This is a testing tool for system error behaviour in light of misbehaved
# GPUs. The tool is able to misprogram the GPU and inspect/modify various PCIe
# state to check whether the errors are reported and handled as expected.
# See main() at the bottom of this file for an overview of what this script
# implements. Most of the code is generic interaction with PCIe devices and
# definitions of various PCIe config space settings, including vendor specific
# ones.
#

from __future__ import print_function
import os
import mmap
import struct
from struct import Struct
import time
import sys
import random
import hashlib
import optparse
import traceback
from logging import debug, info, warn, error
import logging
from collections import namedtuple

BAR0_SIZE = 16 * 1024 * 1024

LW_PMC_ENABLE = 0x200
LW_PMC_ENABLE_HOST = (0x1 << 8)
LW_PMC_ENABLE_PWR = (0x1 << 13)
LW_PMC_ENABLE_PGRAPH = (0x1 << 12)
LW_PMC_ENABLE_MSVLD = (0x1 << 15)
LW_PMC_ENABLE_MSPDEC = (0x1 << 17)
LW_PMC_ENABLE_MSENC = (0x1 << 18)
LW_PMC_ENABLE_MSPPP = (0x1 << 1)
LW_PMC_ENABLE_LWLINK = (0x1 << 25)
LW_PMC_ENABLE_SEC = (0x1 << 14)
LW_PMC_ENABLE_PDISP = (0x1 << 30)

def LW_PMC_ENABLE_LWDEC(lwdec):
    if lwdec == 0:
        return (0x1 << 15)
    elif lwdec == 1:
        return (0x1 << 16)
    elif lwdec == 2:
        return (0x1 << 20)
    else:
        assert 0, "Unhandled lwdec %d" % lwdec

def LW_PMC_ENABLE_LWENC(lwenc):
    if lwenc == 0:
        return (0x1 << 18)
    elif lwenc == 1:
        return (0x1 << 19)
    elif lwenc == 2:
        return (0x1 << 4)
    else:
        assert 0, "Unhandled lwenc %d" % lwenc

LW_PMC_BOOT_0 = 0x0
LW_PROM_DATA = 0x300000
LW_PROM_DATA_SIZE_KEPLER = 524288
LW_PROM_DATA_SIZE_PASCAL = 1048576

LW_BAR0_WINDOW_CFG = 0x1700
LW_BAR0_WINDOW_CFG_TARGET_SYSMEM_COHERENT = 0x2000000
LW_BAR0_WINDOW = 0x700000

LW_PPWR_FALCON_DMACTL = 0x10a10c

def LW_PPWR_FALCON_IMEMD(i):
    return 0x10a184 + i * 16

def LW_PPWR_FALCON_IMEMC(i):
    return 0x10a180 + i * 16

LW_PPWR_FALCON_IMEMC_AINCW_TRUE = 1 << 24
LW_PPWR_FALCON_IMEMC_AINCR_TRUE = 1 << 25
LW_PPWR_FALCON_IMEMC_SELWRE_ENABLED = 1 << 28

def LW_PPWR_FALCON_IMEMT(i):
    return 0x10a188 + i * 16

def LW_PPWR_FALCON_DMEMC(i):
    return 0x0010a1c0 + i * 8

LW_PPWR_FALCON_BOOTVEC = 0x10a104
LW_PPWR_FALCON_CPUCTL = 0x10a100
LW_PPWR_FALCON_HWCFG = 0x10a108
LW_PPWR_FALCON_HWCFG1 = 0x10a12c
LW_PPWR_FALCON_ENGINE_RESET = 0x10a3c0

LW_PMSVLD_FALCON_CPUCTL = 0x84100
LW_PMSPDEC_FALCON_CPUCTL = 0x85100
LW_PMSPPP_FALCON_CPUCTL = 0x86100
LW_PMSENC_FALCON_CPUCTL = 0x1c2100
LW_PHDAFALCON_FALCON_CPUCTL = 0x1c3100
LW_PMINION_FALCON_CPUCTL = 0xa06100
LW_PDISP_FALCON_CPUCTL = 0x627100

# LWDECs
def LW_PLWDEC_FALCON_CPUCTL_TURING(lwdec):
    return 0x830100 + lwdec * 0x4000

def LW_PLWDEC_FALCON_CPUCTL_MAXWELL(lwdec):
    return 0x84100 + lwdec * 0x4000

# LWENCs
def LW_PLWENC_FALCON_CPUCTL(lwenc):
    return 0x1c8100 + lwenc * 0x4000

# GSP
LW_PGSP_FALCON_CPUCTL = 0x110100

# SEC
LW_PSEC_FALCON_CPUCTL_MAXWELL = 0x87100
LW_PSEC_FALCON_CPUCTL_TURING = 0x840100

# FB FALCON
LW_PFBFALCON_FALCON_CPUCTL = 0x9a4100

SYS_DEVICES = "/sys/bus/pci/devices/"

# For ffs()
import ctypes
libc = ctypes.cdll.LoadLibrary('libc.so.6')

def sysfs_find_parent(device):
    device = os.path.basename(device)
    for device_dir in os.listdir(SYS_DEVICES):
        dev_path = os.path.join(SYS_DEVICES, device_dir)
        for f in os.listdir(dev_path):
            if f == device:
                return dev_path
    return None

def find_gpus():
    gpus = []
    other = []
    dev_paths = []
    for device_dir in os.listdir("/sys/bus/pci/devices/"):
        dev_path = os.path.join("/sys/bus/pci/devices/", device_dir)
        bdf = device_dir
        vendor = open(os.path.join(dev_path, "vendor")).readlines()
        vendor = vendor[0].strip()
        if vendor != "0x10de":
            continue
        cls = open(os.path.join(dev_path, "class")).readlines()
        cls = cls[0].strip()
        if cls != "0x030000" and cls != "0x030200":
            continue
        dev_paths.append(dev_path)

    def devpath_to_id(dev_path):
        bdf = os.path.basename(dev_path)
        return int(bdf.replace(":","").replace(".",""), base=16)

    dev_paths = sorted(dev_paths, key=devpath_to_id)
    for dev_path in dev_paths:
        gpu = None
        try:
            gpu = Gpu(dev_path=dev_path)
        except UnknownGpuError as err:
            error("Unknown Lwpu device %s: %s", dev_path, str(err))
            dev = LwidiaDevice(dev_path=dev_path)
            other.append(dev)
            continue
        except Exception as err:
            _, _, tb = sys.exc_info()
            traceback.print_tb(tb)
            error("GPU %s broken: %s", dev_path, str(err))
            gpu = BrokenGpu(dev_path=dev_path)
        gpus.append(gpu)

    return (gpus, other)

class FormattedTuple(object):
    namedtuple = None
    struct = None

    @classmethod
    def _make(cls, data):
        size = cls._size()
        return cls.namedtuple._make(cls.struct.unpack_from(data))

    @classmethod
    def _size(cls):
        return cls.struct.size

def _struct_fmt(size):
   if size == 1:
       return "B"
   elif size == 2:
       return "H"
   elif size == 4:
       return "I"
   else:
       assert 0, "Unhandled size %d" % size

def int_from_data(data, size):
    fmt = _struct_fmt(size)
    return struct.unpack(fmt, data)[0]

def data_from_int(integer, size):
    fmt = _struct_fmt(size)
    return struct.pack(fmt, integer)

class FileRaw(object):
    def __init__(self, path, offset, size):
        self.fd = os.open(path, os.O_RDWR | os.O_SYNC)
        self.base_offset = offset
        self.size = size

    def write(self, offset, data, size):
        os.lseek(self.fd, offset, os.SEEK_SET)
        os.write(self.fd, data_from_int(data, size))

    def write8(self, offset, data):
        self.write(offset, data, 1)

    def write16(self, offset, data):
        self.write(offset, data, 2)

    def write32(self, offset, data):
        self.write(offset, data, 4)

    def read(self, offset, size):
        os.lseek(self.fd, offset, os.SEEK_SET)
        data = os.read(self.fd, size)
        assert data, "offset %s size %d %s" % (hex(offset), size, data)
        return int_from_data(data, size)

    def read8(self, offset):
        return self.read(offset, 1)

    def read16(self, offset):
        return self.read(offset, 2)

    def read32(self, offset):
        return self.read(offset, 4)

    def read_format(self, fmt, offset):
        size = struct.calcsize(fmt)
        os.lseek(self.fd, offset, os.SEEK_SET)
        data = os.read(self.fd, size)
        return struct.unpack(fmt, data)

class FileMap(object):
    def __init__(self, path, offset, size):
        with open(path, "r+b") as f:
            import ctypes
            import numpy
            libc.mmap.restype = ctypes.c_void_p
            prot = mmap.PROT_READ | mmap.PROT_WRITE
            mapped = libc.mmap(ctypes.c_void_p(None), ctypes.c_size_t(size), ctypes.c_int(prot),
                                  ctypes.c_int(mmap.MAP_SHARED), ctypes.c_int(f.fileno()), ctypes.c_size_t(offset))
            mapped_8 = ctypes.cast(mapped, ctypes.POINTER(ctypes.c_uint8))
            self.map_8 = numpy.ctypeslib.as_array(mapped_8, shape=(size,))
            mapped_16 = ctypes.cast(mapped, ctypes.POINTER(ctypes.c_uint16))
            self.map_16 = numpy.ctypeslib.as_array(mapped_16, shape=(size//2,))
            mapped_32 = ctypes.cast(mapped, ctypes.POINTER(ctypes.c_uint32))
            self.map_32 = numpy.ctypeslib.as_array(mapped_32, shape=(size//4,))

    def write32(self, offset, data):
        self.map_32[offset // 4] = data

    def read(self, offset, size):
        if size == 1:
            return self.map_8[offset // 1]
        elif size == 2:
            return self.map_16[offset // 2]
        elif size == 4:
            return self.map_32[offset // 4]
        else:
            assert 0, "Unhandled size %d" % size

    def read8(self, offset):
        return self.read(offset, 1)

    def read16(self, offset):
        return self.read(offset, 2)

    def read32(self, offset):
        return self.read(offset, 4)

GPU_ARCHES = ["kepler", "maxwell", "pascal", "volta", "turing"]
GPU_MAP = {
    0x0e40a0a2: {
        "name": "K520",
        "arch": "kepler",
        "pmu_reset_in_pmc": True,
        "memory_clear_supported": False,
        "forcing_ecc_on_after_reset_supported": False,
        "lwdec": [0],
        "lwenc": [],
        "other_falcons": [],
    },
    0x0f22d0a1: {
        "name": "K80",
        "arch": "kepler",
        "pmu_reset_in_pmc": True,
        "memory_clear_supported": False,
        "forcing_ecc_on_after_reset_supported": False,
        "lwdec": [],
        "lwenc": [],
        "other_falcons": ["msvld", "mspdec", "msppp", "msenc", "hda", "disp"],
        "falcons_cfg": {
            "pmu": {
                "imem_size": 24576,
                "dmem_size": 24576,
                "imem_port_count": 1,
                "dmem_port_count": 4,
            },
            "msvld": {
                "imem_size": 8192,
                "dmem_size": 4096,
                "imem_port_count": 1,
                "dmem_port_count": 1,
            },
            "msppp": {
                "imem_size": 2560,
                "dmem_size": 2048,
                "imem_port_count": 1,
                "dmem_port_count": 1,
            },
            "msenc": {
                "imem_size": 16384,
                "dmem_size": 6144,
                "imem_port_count": 1,
                "dmem_port_count": 1,
            },
            "mspdec": {
                "imem_size": 5120,
                "dmem_size": 4096,
                "imem_port_count": 1,
                "dmem_port_count": 1,
            },
            "hda": {
                "imem_size": 4096,
                "dmem_size": 4096,
                "imem_port_count": 1,
                "dmem_port_count": 1,
            },
            "disp": {
                "imem_size": 16384,
                "dmem_size": 8192,
                "imem_port_count": 1,
                "dmem_port_count": 4,
            },
        },
    },
    0x124320a1: {
        "name": "M60",
        "arch": "maxwell",
        "pmu_reset_in_pmc": True,
        "memory_clear_supported": False,
        "forcing_ecc_on_after_reset_supported": False,
        "lwdec": [0],
        "lwenc": [0, 1],
        "other_falcons": ["sec"],
        "falcons_cfg": {
            "pmu": {
                "imem_size": 49152,
                "dmem_size": 40960,
                "imem_port_count": 1,
                "dmem_port_count": 4,
            },
            "lwdec0": {
                "imem_size": 8192,
                "dmem_size": 6144,
                "imem_port_count": 1,
                "dmem_port_count": 1,
            },
            "lwenc0": {
                "imem_size": 16384,
                "dmem_size": 12288,
                "imem_port_count": 1,
                "dmem_port_count": 1,
            },
            "lwenc1": {
                "imem_size": 16384,
                "dmem_size": 12288,
                "imem_port_count": 1,
                "dmem_port_count": 1,
            },
            "sec": {
                "imem_size": 32768,
                "dmem_size": 16384,
                "imem_port_count": 1,
                "dmem_port_count": 1,
            },
        },
    },
    0x130000a1: {
        "name": "P100",
        "arch": "pascal",
        "pmu_reset_in_pmc": True,
        "memory_clear_supported": True,
        "forcing_ecc_on_after_reset_supported": False,
        "lwdec": [0],
        "lwenc": [],
        "other_falcons": [],
    },
    0x134000a1: {
        "name": "P4",
        "arch": "pascal",
        "pmu_reset_in_pmc": False,
        "memory_clear_supported": False,
        "forcing_ecc_on_after_reset_supported": False,
        "lwdec": [0],
        "lwenc": [],
        "other_falcons": [],
    },
    0x132000a1: {
        "name": "P40",
        "arch": "pascal",
        "pmu_reset_in_pmc": False,
        "memory_clear_supported": False,
        "forcing_ecc_on_after_reset_supported": False,
        "lwdec": [0],
        "lwenc": [],
        "other_falcons": [],
    },
    0x140000a1: {
        "name": "V100",
        "arch": "volta",
        "pmu_reset_in_pmc": False,
        "memory_clear_supported": True,
        "forcing_ecc_on_after_reset_supported": False,
        "lwdec": [0],
        "lwenc": [0, 1, 2],
        "other_falcons": ["sec", "gsp", "fb", "minion"],
        "falcons_cfg": {
            "pmu": {
                "imem_size": 65536,
                "dmem_size": 65536,
                "imem_port_count": 1,
                "dmem_port_count": 4,
            },
            "lwdec0": {
                "imem_size": 10240,
                "dmem_size": 24576,
                "imem_port_count": 1,
                "dmem_port_count": 1,
            },
            "lwenc0": {
                "imem_size": 16384,
                "dmem_size": 16384,
                "imem_port_count": 1,
                "dmem_port_count": 1,
            },
            "lwenc1": {
                "imem_size": 16384,
                "dmem_size": 16384,
                "imem_port_count": 1,
                "dmem_port_count": 1,
            },
            "lwenc2": {
                "imem_size": 16384,
                "dmem_size": 16384,
                "imem_port_count": 1,
                "dmem_port_count": 1,
            },
            "gsp": {
                "imem_size": 65536,
                "dmem_size": 65536,
                "emem_size": 8192,
                "imem_port_count": 1,
                "dmem_port_count": 1,
            },
            "sec": {
                "imem_size": 65536,
                "dmem_size": 65536,
                "emem_size": 8192,
                "imem_port_count": 1,
                "dmem_port_count": 1,
            },
            "fb": {
                "imem_size": 16384,
                "dmem_size": 16384,
                "imem_port_count": 1,
                "dmem_port_count": 1,
            },
            "minion": {
                "imem_size": 8192,
                "dmem_size": 4096,
                "imem_port_count": 1,
                "dmem_port_count": 4,
            },
        }
    },
    0x164000a1bad: {
        "name": "T4",
        "arch": "turing",
        "pmu_reset_in_pmc": False,
        "memory_clear_supported": True,
        "forcing_ecc_on_after_reset_supported": True,
        "lwdec": [],
        "lwenc": [0],
        "other_falcons": ["sec", "gsp", "fb", "minion"],
        "falcons_cfg": {
            "pmu": {
                "imem_size": 65536,
                "dmem_size": 65536,
                "imem_port_count": 1,
                "dmem_port_count": 4,
            },
            "lwdec0": {
                "imem_size": 32768,
                "dmem_size": 32768,
                "imem_port_count": 1,
                "dmem_port_count": 1,
            },
            "lwdec1": {
                "imem_size": 32768,
                "dmem_size": 32768,
                "imem_port_count": 1,
                "dmem_port_count": 1,
            },
            "lwenc0": {
                "imem_size": 32768,
                "dmem_size": 32768,
                "imem_port_count": 1,
                "dmem_port_count": 1,
            },
            "gsp": {
                "imem_size": 65536,
                "dmem_size": 65536,
                "emem_size": 8192,
                "imem_port_count": 1,
                "dmem_port_count": 1,
            },
            "sec": {
                "imem_size": 65536,
                "dmem_size": 65536,
                "emem_size": 8192,
                "imem_port_count": 1,
                "dmem_port_count": 1,
            },
            "fb": {
                "imem_size": 65536,
                "dmem_size": 65536,
                "imem_port_count": 1,
                "dmem_port_count": 4,
            },
            "minion": {
                "imem_size": 16384,
                "dmem_size": 8192,
                "imem_port_count": 1,
                "dmem_port_count": 4,
            },
        },
    },
    0x164000a1: {
        "name": "TU104_Apna",
        "arch": "turing",
        "pmu_reset_in_pmc": False,
        "memory_clear_supported": True,
        "forcing_ecc_on_after_reset_supported": True,
        "lwdec": [],
        "lwenc": [],
        "other_falcons": ["sec"],
        "falcons_cfg": {
            "sec": {
                "imem_size": 65536,
                "dmem_size": 65536,
                "emem_size": 8192,
                "imem_port_count": 1,
                "dmem_port_count": 1,
            },
        },
    },
}

PCI_CFG_SPACE_SIZE = 256
PCI_CFG_SPACE_EXP_SIZE = 4096

PCI_CAPABILITY_LIST = 0x34
# PCI Express
PCI_CAP_ID_EXP = 0x10

CAP_ID_MASK = 0xff

# Advanced Error Reporting
PCI_EXT_CAP_ID_ERR = 0x01

# Uncorrectable Error Status
PCI_ERR_UNCOR_STATUS = 4
# Uncorrectable Error Mask
PCI_ERR_UNCOR_MASK = 8
# Uncorrectable Error Severity
PCI_ERR_UNCOR_SEVER = 12

class Bitfield(object):
    """Wrapper around bitfields, see PciUncorrectableErrors for an example"""
    fields = {}

    def __init__(self, raw, name=None):
        self.raw = raw
        if name is None:
            name = self.__class__.__name__
        self.name = name

    def __field_get_mask(self, field):
        bits = self.__class__.fields[field]
        if isinstance(bits, int):
            return bits

        assert isinstance(bits, tuple)
        high_bit = bits[0]
        low_bit = bits[1]

        mask = (1 << (high_bit - low_bit + 1)) - 1
        mask <<= low_bit
        return mask

    def __field_get_shift(self, field):
        mask = self.__field_get_mask(field)
        assert mask != 0
        return libc.ffs(mask) - 1

    def __getitem__(self, field):
        mask = self.__field_get_mask(field)
        shift = self.__field_get_shift(field)
        return (self.raw & mask) >> shift

    def __setitem__(self, field, val):
        mask = self.__field_get_mask(field)
        shift = self.__field_get_shift(field)

        val = val << shift
        assert (val & ~mask) == 0

        self.raw = (self.raw & ~mask) | val

    def __str__(self):
        return self.name + " " + str(self.values()) + " raw " + hex(self.raw)

    def values(self):
        vals = {}
        for f in self.__class__.fields:
            vals[f] = self[f]

        return vals

    def non_zero(self):
        ret = {}
        for k, v in self.values().items():
            if v != 0:
                ret[k] = v
        return ret

    def non_zero_fields(self):
        ret = []
        for k, v in self.values().items():
            if v != 0:
                ret.append(k)
        return ret


class PciUncorrectableErrors(Bitfield):
    size = 4
    fields = {
    # Undefined
    "UND": 0x00000001,

    # Data Link Protocol
    "DLP": 0x00000010,

    # Surprise Down
    "SURPDN":  0x00000020,

    # Poisoned TLP
    "POISON_TLP": 0x00001000,

    # Flow Control Protocol
    "FCP": 0x00002000,

    # Completion Timeout
    "COMP_TIME": 0x00004000,

    # Completer Abort
    "COMP_ABORT": 0x00008000,

    # Unexpected Completion
    "UNX_COMP": 0x00010000,

    # Receiver Overflow
    "RX_OVER": 0x00020000,

    # Malformed TLP
    "MALF_TLP": 0x00040000,

    # ECRC Error Status
    "ECRC": 0x00080000,

    # Unsupported Request
    "UNSUP": 0x00100000,

    # ACS Violation
    "ACSV": 0x00200000,

    # internal error
    "INTN": 0x00400000,

    # MC blocked TLP
    "MCBTLP": 0x00800000,

    # Atomic egress blocked
    "ATOMEG": 0x01000000,

    # TLP prefix blocked
    "TLPPRE": 0x02000000,
    }

    def __str__(self):
        # Print only the non zero bits
        return "%s %s" % (self.name, str(self.non_zero_fields()))

PCI_EXP_DEVCAP2 = 36
PCI_EXP_DEVCTL2 = 40
class PciDevCtl2(Bitfield):
    size = 2
    fields = {
        # Completion Timeout Value
        "COMP_TIMEOUT":         0x000f,

        # Completion Timeout Disable
        "COMP_TMOUT_DIS":       0x0010,

        # Alternative Routing-ID
        "ARI":                  0x0020,

        # Set Atomic requests
        "ATOMIC_REQ":           0x0040,

        # Block atomic egress
        "ATOMIC_EGRESS_BLOCK":  0x0080,

        # Allow IDO for requests
        "IDO_REQ_EN":           0x0100,

        # Allow IDO for completions
        "IDO_CMP_EN":           0x0200,

        # Enable LTR mechanism
        "LTR_EN":               0x0400,

        # Enable OBFF Message type A
        "OBFF_MSGA_EN":         0x2000,

        # Enable OBFF Message type B
        "OBFF_MSGB_EN":         0x4000,

        # OBFF using WAKE# signaling
        "OBFF_WAKE_EN":         0x6000,
    }

class DeviceField(object):
    """Wrapper for a device register/setting defined by a bitfield class and
    accessible with dev.read()/write() at the specified offset"""
    def __init__(self, bitfield_class, dev, offset, name=None):
        self.dev = dev
        self.offset = offset
        self.bitfield_class = bitfield_class
        self.size = bitfield_class.size
        if name is None:
            name = bitfield_class.__name__
        self.name = name
        self._read()

    def _read(self):
        raw = self.dev.read(self.offset, self.size)
        self.value = self.bitfield_class(raw, name=self.name)
        return self.value

    def _write(self):
        self.dev.write(self.offset, self.value.raw, self.size)

    def __getitem__(self, field):
        self._read()
        return self.value[field]

    def __setitem__(self, field, val):
        self._read()
        self.value[field] = val
        self._write()

    def write_only(self, field, val):
        """Write to the device with only the field set as specified. Useful for W1C bits"""

        bf = self.bitfield_class(0)
        bf[field] = val
        self.dev.write(self.offset, bf.raw, self.size)
        self._read()

    def __str__(self):
        self._read()
        return str(self.value)

PCI_COMMAND = 0x04
class PciCommand(Bitfield):
    size = 2
    fields = {
        "MEMORY": 0x0002,
        "MASTER": 0x0004,
        "PARITY": 0x0040,
        "SERR":   0x0100,
    }

PCI_EXP_DEVCTL = 8
class PciDevCtl(Bitfield):
    size = 4
    fields = {
        # /* Correctable Error Reporting En. */
        "CERE": 0x0001,

        # /* Non-Fatal Error Reporting Enable */
        "NFERE": 0x0002,

        # /* Fatal Error Reporting Enable */
        "FERE": 0x0004,

        # /* Unsupported Request Reporting En. */
        "URRE": 0x0008,

        # /* Enable relaxed ordering */
        "RELAX_EN": 0x0010,
        # /* Max_Payload_Size */
        "PAYLOAD": 0x00e0,

        # /* Extended Tag Field Enable */
        "EXT_TAG": 0x0100,

        # /* Phantom Functions Enable */
        "PHANTOM": 0x0200,

        # /* Auxiliary Power PM Enable */
        "AUX_PME": 0x0400,

        # /* Enable No Snoop */
        "NOSNOOP_EN": 0x0800,

        # /* Max_Read_Request_Size */
        #"READRQ_128B  0x0000 /* 128 Bytes */
        #"READRQ_256B  0x1000 /* 256 Bytes */
        #"READRQ_512B  0x2000 /* 512 Bytes */
        #"READRQ_1024B 0x3000 /* 1024 Bytes */
        "READRQ": 0x7000,

        # /* Bridge Configuration Retry / FLR */
        "BCR_FLR": 0x8000,
    }

PCI_EXP_LNKCAP = 12
class PciLinkCap(Bitfield):
    size = 4
    fields = {
        # Maximum Link Width
        "MLW":   0x000003f0,

        # Surprise Down Error Reporting Capable
        "SDERC": 0x00080000,

        # Port Number
        "PN":    0xff000000,
    }

    def __str__(self):
        return "{ Link cap " + str(self.values()) + " raw " + hex(self.raw) + " }"



# Link Control
PCI_EXP_LNKCTL = 16
class PciLinkControl(Bitfield):
    size = 2
    fields = {
        # ASPM Control
        "ASPMC": 0x0003,

        # Read Completion Boundary
        "RCB": 0x0008,

        # Link Disable
        "LD": 0x0010,

        # Retrain Link
        "RL": 0x0020,

        # Common Clock Configuration
        "CCC": 0x0040,

        # Extended Synch
        "ES": 0x0080,

        # Hardware Autonomous Width Disable
        "HAWD": 0x0200,

        # Enable clkreq
        "CLKREQ_EN": 0x100,

        # Link Bandwidth Management Interrupt Enable
        "LBMIE": 0x0400,

        # Lnk Autonomous Bandwidth Interrupt Enable
        "LABIE": 0x0800,
    }

    def __str__(self):
        return "{ Link control " + str(self.values()) + " raw " + hex(self.raw) + " }"

# Link Status
PCI_EXP_LNKSTA = 18
class PciLinkStatus(Bitfield):
    size = 2
    fields = {
        # Current Link Speed
        # CLS_2_5GB 0x01 Current Link Speed 2.5GT/s
        # CLS_5_0GB 0x02 Current Link Speed 5.0GT/s
        "CLS": 0x000f,

        # Nogotiated Link Width
        "NLW": 0x03f0,

        # Link Training
        "LT": 0x0800,

        # Slot Clock Configuration
        "SLC": 0x1000,

        # Data Link Layer Link Active
        "DLLLA": 0x2000,

        # Link Bandwidth Management Status
        "LBMS": 0x4000,

        # Link Autonomous Bandwidth Status */
        "LABS": 0x8000,
    }

    def __str__(self):
        return "{ Link status " + str(self.values()) + " raw " + hex(self.raw) + " }"

DEVICES = { }

class Device(object):
    def __init__(self):
        self.parent = None
        self.children = []

    def is_hidden(self):
        return True

    def has_aer(self):
        return False

    def is_bridge(self):
        return False

    def is_root(self):
        return self.parent == None

    def is_gpu(self):
        return False

    def is_plx(self):
        return False

    def is_intel(self):
        return False

class PciDevice(Device):
    @staticmethod
    def _open_config(dev_path):
        dev_path_config = os.path.join(dev_path, "config")
        return FileRaw(dev_path_config, 0, os.path.getsize(dev_path_config))

    @staticmethod
    def find_class_for_device(dev_path):
        config = PciDevice._open_config(dev_path)
        vendor = config.read16(0x0)
        if vendor == 0x10de:
            return Gpu
        elif vendor == 0x10b5:
            return PlxBridge
        elif vendor == 0x8086:
            return IntelRootPort
        elif vendor == 0x1014:
            return PciBridge
        else:
            assert(False)
            return None

    @staticmethod
    def init_dispatch(dev_path):
        cls = PciDevice.find_class_for_device(dev_path)
        if cls:
            return cls(dev_path)
        return None

    @staticmethod
    def find_or_init(dev_path):
        if dev_path == None:
            if -1 not in DEVICES:
                DEVICES[-1] = Device()
            return DEVICES[-1]
        bdf = os.path.basename(dev_path)
        if bdf in DEVICES:
            return DEVICES[bdf]
        dev = PciDevice.init_dispatch(dev_path)
        DEVICES[bdf] = dev
        return dev

    def __init__(self, dev_path):
        self.parent = None
        self.children = []
        self.dev_path = dev_path
        self.bdf = os.path.basename(dev_path)
        self.config = self._open_config(dev_path)
        self.vendor = self.config.read16(0)
        self.device = self.config.read16(2)
        self.cfg_space_broken = False
        self._init_caps()
        self._init_bars()
        if not self.cfg_space_broken:
            self.command = DeviceField(PciCommand, self.config, PCI_COMMAND)
            self.devctl = DeviceField(PciDevCtl, self.config, self.caps[PCI_CAP_ID_EXP] + PCI_EXP_DEVCTL)
            self.devctl2 = DeviceField(PciDevCtl2, self.config, self.caps[PCI_CAP_ID_EXP] + PCI_EXP_DEVCTL2)
            if self.has_exp():
                self.link_cap = DeviceField(PciLinkCap, self.config, self.caps[PCI_CAP_ID_EXP] + PCI_EXP_LNKCAP)
                self.link_ctl = DeviceField(PciLinkControl, self.config, self.caps[PCI_CAP_ID_EXP] + PCI_EXP_LNKCTL)
                self.link_status = DeviceField(PciLinkStatus, self.config, self.caps[PCI_CAP_ID_EXP] + PCI_EXP_LNKSTA)
            if self.has_aer():
                self.uncorr_status = DeviceField(PciUncorrectableErrors, self.config, self.ext_caps[PCI_EXT_CAP_ID_ERR] + PCI_ERR_UNCOR_STATUS, name="UNCOR_STATUS")
                self.uncorr_mask   = DeviceField(PciUncorrectableErrors, self.config, self.ext_caps[PCI_EXT_CAP_ID_ERR] + PCI_ERR_UNCOR_MASK, name="UNCOR_MASK")
                self.uncorr_sever  = DeviceField(PciUncorrectableErrors, self.config, self.ext_caps[PCI_EXT_CAP_ID_ERR] + PCI_ERR_UNCOR_SEVER, name="UNCOR_SEVER")
        self.parent = PciDevice.find_or_init(sysfs_find_parent(dev_path))

    def is_hidden(self):
        return False

    def has_aer(self):
        return PCI_EXT_CAP_ID_ERR in self.ext_caps

    def has_exp(self):
        return PCI_CAP_ID_EXP in self.caps

    def reinit(self):
        return PciDevice.init_dispatch(self.dev_path)

    def get_root_port(self):
        dev = self.parent
        while dev.parent != None and not dev.parent.is_hidden():
            dev = dev.parent
        return dev

    def _init_bars(self):
        self.bars = []
        resources = open(os.path.join(self.dev_path, "resource")).readlines()
        for bar_line in resources:
            bar_line = bar_line.split(" ")
            addr = int(bar_line[0], base=16)
            end = int(bar_line[1], base=16)
            if addr != 0:
                size = end - addr + 1
                self.bars.append((addr, size))

    def _init_caps(self):
        self.caps = {}
        self.ext_caps = {}
        cap_offset = self.config.read8(PCI_CAPABILITY_LIST)
        data = 0
        if cap_offset == 0xff:
            self.cfg_space_broken = True
            error("Broken device %s", self.dev_path)
            return
        while cap_offset != 0:
            data = self.config.read32(cap_offset)
            cap_id = data & CAP_ID_MASK
            self.caps[cap_id] = cap_offset
            cap_offset = (data >> 8) & 0xff

        self._init_ext_caps()


    def _init_ext_caps(self):
        if self.config.size <= PCI_CFG_SPACE_SIZE:
            return

        offset = PCI_CFG_SPACE_SIZE
        header = self.config.read32(PCI_CFG_SPACE_SIZE)
        while offset != 0:
            cap = header & 0xffff
            self.ext_caps[cap] = offset

            offset = (header >> 20) & 0xffc
            header = self.config.read32(offset)

    def __str__(self):
        return "PCI %s %s:%s" % (self.bdf, hex(self.vendor), hex(self.device))

    def set_command_memory(self, enable):
        self.command["MEMORY"] = 1 if enable else 0

    def cfg_read8(self, offset):
        return self.config.read8(offset)

    def cfg_read32(self, offset):
        return self.config.read32(offset)

    def cfg_write32(self, offset, data):
        self.config.write32(offset, data)

    def sanity_check_cfg_space(self):
        # Use an offset unlikely to be intercepted in case of virtualization
        vendor = self.config.read16(0xf0)
        return vendor != 0xffff

    def sanity_check_cfg_space_bars(self):
        """Check whether BAR0 is configured"""
        bar0 = self.config.read32(LW_XVE_BAR0)
        if bar0 == 0:
            return False
        if bar0 == 0xffffffff:
            return False
        return True

    def sysfs_remove(self):
        remove_path = os.path.join(self.dev_path, "remove")
        if not os.path.exists(remove_path):
            debug("%s remove not present: '%s'", self, remove_path)
        with open(remove_path, "w") as rf:
            rf.write("1")

PCI_BRIDGE_CONTROL = 0x3e
class PciBridgeControl(Bitfield):
    size = 1
    fields = {
            # Enable parity detection on secondary interface
            "PARITY": 0x01,

            # The same for SERR forwarding
            "SERR": 0x02,

            # Enable ISA mode
            "ISA": 0x04,

            # Forward VGA addresses
            "VGA": 0x08,

            # Report master aborts
            "MASTER_ABORT": 0x20,

            # Secondary bus reset (SBR)
            "BUS_RESET": 0x40,

            # Fast Back2Back enabled on secondary interface
            "FAST_BACK": 0x80,
    }

    def __str__(self):
        return "{ Bridge control " + str(self.values()) + " raw " + hex(self.raw) + " }"


class PciBridge(PciDevice):
    def __init__(self, dev_path):
        super(PciBridge, self).__init__(dev_path)
        self.bridge_ctl = DeviceField(PciBridgeControl, self.config, PCI_BRIDGE_CONTROL)
        if self.parent:
            self.parent.children.append(self)

    def is_bridge(self):
        return True

    def _set_link_disable(self, disable):
        self.link_ctl["LD"] = 1 if disable else 0
        debug("%s %s link disable, %s", self, "setting" if disable else "unsetting", self.link_ctl)

    def _set_sbr(self, reset):
        self.bridge_ctl["BUS_RESET"] = 1 if reset else 0
        debug("%s %s bus reset, %s",
              self, "setting" if reset else "unsetting", self.bridge_ctl)

    def toggle_link(self):
        self._set_link_disable(True)
        time.sleep(0.1)
        self._set_link_disable(False)
        time.sleep(0.1)

    def toggle_sbr(self, sleep_after=True):
        self._set_sbr(True)
        time.sleep(0.1)
        self._set_sbr(False)
        if sleep_after:
            time.sleep(0.1)


# Uncorrectable Error Detect Status Mask
# This register masks PCIe link related uncorrectable errors from causing the associated
# AER status bit to be set.
# Same bitfield layout as in AER: PciUncorrectableErrors
INTEL_ROOT_PORT_UNCEDMASK = 0x218

# Root Port Error Detect Status Mask
#
# This register masks the associated error messages (received from PCIe link and NOT
# the virtual ones generated internally), from causing the associated status bits in AER to
# be set.
INTEL_ROOT_PORT_RPEDMASK = 0x220
class IntelRootPortErrorDetectStatusMask(Bitfield):
    size = 4
    fields = {
            # 2:2 RWS 0x0 fatal_error_detected_status_mask:
            "fatal_error_detected_status_mask": (2, 2),

            # 1:1 RWS 0x0 non_fatal_error_detected_status_mask:
            "non_fatal_error_detected_status_mask": (1, 1),

            # 0:0 RWS 0x0 correctable_error_detected_status_mask:
            "correctable_error_detected_status_mask": (0, 0),
    }

# XP Uncorrectable Error Detect Mask
#
# This register masks other uncorrectable errors from causing the associated
# XPUNCERRSTS status bit to be set.
INTEL_ROOT_PORT_XPUNCEDMASK = 0x224
class IntelXpUncorrectableErrorDetectMask(Bitfield):
    size = 4
    fields = {
        "outbound_poisoned_data_detect_mask": (9, 9),
        "received_msi_writes_greater_than_a_dword_data_detect_mask": (8, 8),
        "received_pcie_completion_with_ur_detect_mask": (6, 6),
        "received_pcie_completion_with_ca_detect_mask": (5, 5),
        "sent_completion_with_unsupported_request_detect_mask": (4, 4),
        "sent_completion_with_completer_abort_detect_mask": (3, 3),
        "outbound_switch_fifo_data_parity_error_detect_mask": (1, 1),
    }

# XP Global Error Status
#
# This register captures a concise summary of the error logging in AER registers so that
# sideband system management software can view the errors independent of the main
# OS that might be controlling the AER errors.
INTEL_ROOT_PORT_XPGLBERRSTS = 0x230
class IntelXpGlobalErrorStatus(Bitfield):
    size = 4
    fields = {
            # 2:2 RW1CS 0x0 pcie_aer_correctable_error:
            # A PCIe correctable error (ERR_COR message received from externally or
            # through a virtual ERR_COR message generated internally) was detected
            # anew. Note that if that error was masked in the PCIe AER, it is not reported
            # in this field. Software clears this bit by writing a 1 and at that s tage, only
            # 'subsequent' PCIe unmasked correctable errors will set this bit.Conceptually,
            # per the flow of PCI Express Base Spec 2.0 defined Error message control,
            # this bit is set by the ERR_COR message that is enabled to cause a System
            # Error notification.
            "pcie_aer_correctable_error": (2, 2),

            # 1:1 RW1CS 0x0 pcie_aer_non_fatal_error:
            # A PCIe non-fatal error (ERR_NONFATAL message received from externally or
            # through a virtual ERR_NONFATAL message generated internally) was
            # detected anew. Note that if that error was masked in the PCIe AER, it is not
            # reported in this field. Software clears this bit by writing a 1 and at that stage
            # only 'subsequent' PCIe unmasked non-fatal errors will set this bit again.
            "pcie_aer_non_fatal_error": (1, 1),

            # 0:0 RW1CS 0x0 pcie_aer_fatal_error:
            # A PCIe fatal error (ERR_FATAL message received from externally or through
            # a virtual ERR_FATAL message generated internally) was detected anew. Note
            # that if that error was masked in the PCIe AER, it is not reported in this field.
            # Software clears this bit by writing a 1 and at that stage, only 'subsequent'
            # PCIe unmasked fatal errors will set this bit.
            "pcie_aer_fatal_error": (0, 0),
    }

class IntelRootPort(PciBridge):
    def __init__(self, dev_path):
        super(IntelRootPort, self).__init__(dev_path)
        self.intel_uncedmask = DeviceField(PciUncorrectableErrors, self.config, INTEL_ROOT_PORT_UNCEDMASK, "INTEL_UNCEDMASK")
        self.intel_rpedmask = DeviceField(IntelRootPortErrorDetectStatusMask, self.config, INTEL_ROOT_PORT_RPEDMASK)
        self.intel_xpunced_mask = DeviceField(IntelXpUncorrectableErrorDetectMask, self.config, INTEL_ROOT_PORT_XPUNCEDMASK)
        self.intel_xpglberrsts = DeviceField(IntelXpGlobalErrorStatus, self.config, INTEL_ROOT_PORT_XPGLBERRSTS)

    def is_intel(self):
        return True

    def __str__(self):
        return "Intel root port %s" % self.bdf

PLX_EGRESS_CONTROL = 0xf30

class PlxEgressControl(Bitfield):
    size = 2
    fields = {
            # Egress Credit Timeout Enable
            # 0 = Egress Credit Timeout mechanism is disabled.
            # 1 = Egress Credit Timeout mechanism is enabled. The timeout period is
            # selected in field [12:11] (Egress Credit Timeout Value). Status is reflected
            # in bit 16 (Egress Credit Timeout Status).
            # If the Egress Credit Timer is enabled and expires (due to lack of Flow
            # Control credits from the connected device), the Port brings down its
            # Link. This event generates a Surprise Down Uncorrectable error, for
            # Downstream Ports. For Upstream Port Egress Credit Timeout, the connected
            # Upstream device detects the Surprise Down event.
            #
            # LWPU note: Surprise Down Uncorrectable error is not generated
            # if SDERC is disabled in PCI-e link caps (see PciLinkCap) of the
            # port.
            "CREDIT_TIMEOUT_ENABLE": (10, 10),

            # Egress Credit Timeout Value
            # 00b = 1 ms
            # 01b = 512 ms
            # 10b = 1s
            # 11b = Reserved
            "CREDIT_TIMEOUT_VALUE": (12, 11),
    }
    def __str__(self):
        return "{ PLX egress control " + str(self.values()) + " raw " + hex(self.raw) + " }"

class PlxEgressControlExtended(Bitfield):
    size = 2
    fields = {
            # Egress Credit Timeout Enable
            # 0 = Egress Credit Timeout mechanism is disabled.
            # 1 = Egress Credit Timeout mechanism is enabled. The timeout period is
            # selected in field [12:11] (Egress Credit Timeout Value). Status is reflected
            # in bit 16 (Egress Credit Timeout Status).
            # If the Egress Credit Timer is enabled and expires (due to lack of Flow
            # Control credits from the connected device), the Port brings down its
            # Link. This event generates a Surprise Down Uncorrectable error, for
            # Downstream Ports. For Upstream Port Egress Credit Timeout, the connected
            # Upstream device detects the Surprise Down event.
            #
            # LWPU note: Surprise Down Uncorrectable error is not generated
            # if SDERC is disabled in PCI-e link caps (see PciLinkCap) of the
            # port.
            "CREDIT_TIMEOUT_ENABLE": (10, 10),

            # Egress Credit Timeout Value
            # 000b = 1 ms
            # 001b = 2 ms
            # 010b = 4 ms
            # 011b = 8 ms
            # 100b = 16 ms
            # 101b = 32 ms
            # 110b = 512 ms
            # 111b = 1 s
            "CREDIT_TIMEOUT_VALUE": (13, 11),
    }
    def __str__(self):
        return "{ PLX egress control ext " + str(self.values()) + " raw " + hex(self.raw) + " }"


PLX_EGRESS_STATUS = 0xf32
class PlxEgressStatus(Bitfield):
    size = 2
    fields = {
            # Egress Credit Timeout Status
            # 0 = No timeout
            # 1 = Timeout
            "CREDIT_TIMEOUT_STATUS": (0, 0),

            # Egress Credit Timeout VC&T
            # Egress Credit timeout for Virtual Channel and Type.
            # 00b = Posted
            # 01b = Non-Posted
            # 10b = Completion
            # 11b = Reserved
            "CREDIT_TIMEOUT_TYPE": (2, 1),
    }

    def __str__(self):
        return "{ PLX egress status " + str(self.values()) + " raw " + hex(self.raw) + " }"


class PlxBridge(PciBridge):
    def __init__(self, dev_path):
        super(PlxBridge, self).__init__(dev_path)

        if self.plx_has_extended_egress_control():
            self.plx_egress_control = DeviceField(PlxEgressControlExtended, self.config, PLX_EGRESS_CONTROL)
        else:
            self.plx_egress_control = DeviceField(PlxEgressControl, self.config, PLX_EGRESS_CONTROL)
        self.plx_egress_status = DeviceField(PlxEgressStatus, self.config, PLX_EGRESS_STATUS)

    def __str__(self):
        return "PLX %s" % self.bdf

    def is_plx(self):
        return True

    def plx_has_extended_egress_control(self):
        return self.device in [0x9781, 0x9797]

    def plx_check_timeout(self):
        assert self.is_plx, "Not plx %s" % str(self)
        return self.plx_egress_status["CREDIT_TIMEOUT_STATUS"] == 1

    def plx_clear_timeout(self):
        assert self.is_plx, "Not plx %s" % str(self)
        if not self.plx_check_timeout():
            return
        # Write 1 to clear
        self.plx_egress_status["CREDIT_TIMEOUT_STATUS"] = 1
        debug("%s cleared timeout, new %s", self, self.plx_egress_status)

    def plx_is_timeout_enabled(self):
        return self.plx_egress_control["CREDIT_TIMEOUT_ENABLE"] == 1

    def plx_set_timeout(self, timeout_ms):
        if timeout_ms == 0:
            self.plx_egress_control["CREDIT_TIMEOUT_ENABLE"] = 0
        elif self.plx_has_extended_egress_control():
            # Egress Credit Timeout Value
            # 000b = 1 ms
            # 001b = 2 ms
            # 010b = 4 ms
            # 011b = 8 ms
            # 100b = 16 ms
            # 101b = 32 ms
            # 110b = 512 ms
            # 111b = 1 s
            settings = {
                   1: 0b000,
                   2: 0b001,
                   4: 0b010,
                   8: 0b011,
                  16: 0b100,
                  32: 0b101,
                 512: 0b110,
                1024: 0b111,
            }
            timeout_val = settings.get(timeout_ms, None)
            assert timeout_val != None, "Unexpected timeout %d" % timeout_ms
            self.plx_egress_control["CREDIT_TIMEOUT_ENABLE"] = 1
            self.plx_egress_control["CREDIT_TIMEOUT_VALUE"] = timeout_val
        else:
            self.plx_egress_control["CREDIT_TIMEOUT_ENABLE"] = 1
            if timeout_ms == 1:
                timeout_val = 0
            elif timeout_ms == 512:
                timeout_val = 1
            elif timeout_ms == 1024:
                timeout_val = 2
            else:
                assert False, "Unexpected timeout %d" % timeout_ms

            self.plx_egress_control["CREDIT_TIMEOUT_ENABLE"] = 1
            self.plx_egress_control["CREDIT_TIMEOUT_VALUE"] = timeout_val

        info("%s %s egress timeout (%d ms), new %s", self, "enabling" if timeout_ms > 0 else "disabling", timeout_ms, self.plx_egress_control)

LW_XVE_DEV_CTRL = 0x4
LW_XVE_BAR0 = 0x10
LW_XVE_BAR1_LO = 0x14
LW_XVE_BAR1_HI = 0x18
LW_XVE_BAR2_LO = 0x1c
LW_XVE_BAR2_HI = 0x20
LW_XVE_BAR3 = 0x24
LW_XVE_VCCAP_CTRL0 = 0x114

GPU_CFG_SPACE_OFFSETS = [
    LW_XVE_DEV_CTRL,
    LW_XVE_BAR0,
    LW_XVE_BAR1_LO,
    LW_XVE_BAR1_HI,
    LW_XVE_BAR2_LO,
    LW_XVE_BAR2_HI,
    LW_XVE_BAR3,
    LW_XVE_VCCAP_CTRL0,
]

class BrokenGpu(PciDevice):
    def __init__(self, dev_path):
        super(BrokenGpu, self).__init__(dev_path)
        self.cfg_space_working = False
        self.bars_configured = False
        self.cfg_space_working = self.sanity_check_cfg_space()
        error("Config space working %s", str(self.cfg_space_working))
        if self.cfg_space_working:
            self.bars_configured = self.sanity_check_cfg_space_bars()

        if self.parent:
            self.parent.children.append(self)

    def is_gpu(self):
        return True

    def is_broken_gpu(self):
        return True

    def reset_with_sbr(self):
        assert self.parent.is_bridge()
        self.parent.toggle_sbr()
        return self.sanity_check_cfg_space()

    def __str__(self):
        return "GPU %s [broken, cfg space working %d bars configured %d]" % (self.bdf, self.cfg_space_working, self.bars_configured)

class LwidiaDevice(PciDevice):
    def __init__(self, dev_path):
        super(LwidiaDevice, self).__init__(dev_path)

        self.bar0_addr = self.bars[0][0]

        if self.parent:
            self.parent.children.append(self)

    def is_gpu(self):
        return False

    def is_broken_gpu(self):
        return False

    def is_unknown(self):
        return True

    def reset_with_sbr(self):
        pass

    def __str__(self):
        return "Lwpu %s BAR0 0x%x devid %s" % (self.bdf, self.bar0_addr, hex(self.device))

#I 31 30         24 23   20 19   16 15            8 7             0
#I .-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-.
#I |                   IFR SIGNATURE ("LWGI")                      | LW_PBUS_IFR_FMT_FIXED0
#I +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 
#I |P|  FIXED DATA SIZE [30:16]    | VERSION[15:8] | CHECKSUM[7:0] | LW_PBUS_IFR_FMT_FIXED1
#I +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 
#I |P|0 0 0 0 0 0 0 0 0 0 0|         TOTAL DATA SIZE[19:0]         | LW_PBUS_IFR_FMT_FIXED2
#I +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 
#I |                   IFR XVE SUBSYSTEM                           | LW_PBUS_IFR_FMT_FIXED3
#I `-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-'
class IfrHeader(FormattedTuple):
    namedtuple = namedtuple("ifr_header", "signature checksum version fixed_data_size total_data_size xve_subsystem")
    struct = Struct("=LBBHLL")

#Istruct PCI_EXP_ROM_HEADER       //  PCI Expansion ROM Header
#I{
#I    LwU16   romSignature;       //  00h: ROM Signature
#I    LwU8    reserved [0x16];    //  02h: Reserved (processor architecture unique data)
#I    LwU16   pciDataStructPtr;   //  18h: Pointer to PCI Data Structure
#I};
#I#define PCI_EXP_ROM_SIGNATURE           ( 0xAA55 )      // {0x55,0xAA}
#I#define PCI_EXP_ROM_SIGNATURE_LW        ( 0x4E56 )      // {'V','N'}
#I#define PCI_EXP_ROM_SIGNATURE_LW2       ( 0xBB77 )      // {0x77,0xBB}
class RomHeader(FormattedTuple):
    namedtuple = namedtuple("rom_header", "signature ptr")
    struct = Struct("=H22xH")

#Istruct PCI_DATA_STRUCT          //  PCI Data Structure
#I{
#I    LwU32   signature;          //  00h: Signature, the string "PCIR"
#I    LwU16   vendorID;           //  04h: Vendor Identification
#I    LwU16   deviceID;           //  06h: Device Identification
#I    LwU16   deviceListPtr;      //  08h: Device List Pointer
#I    LwU16   pciDataStructLen;   //  0Ah: PCI Data Structure Length
#I    LwU8    pciDataStructRev;   //  0Ch: PCI Data Structure Revision
#I    LwU8    classCode[3];       //  0Dh: Class Code
#I    LwU16   imageLen;           //  10h: Image Length (units of 512 bytes)
#I    LwU16   vendorRomRev;       //  12h: Revision Level of the Vendor's ROM
#I    LwU8    codeType;           //  14h: Code Type
#I    LwU8    lastImage;          //  15h: Last Image Indicator
#I    LwU16   maxRuntimeImageLen; //  16h: Maximum Run-time Image Length (units of 512 bytes)
#I    LwU16   configUtilityPtr;   //  18h: Pointer to Configuration Utility Code Header
#I    LwU16   dmtfClpEntryPtr;    //  1Ah; Pointer to DMTF CLP Entry Point
#I};
#I
#I#define PCI_DATA_STRUCT_SIGNATURE       ( 0x52494350 )  // {'P','C','I','R'}
#I#define PCI_DATA_STRUCT_SIGNATURE_LW    ( 0x5344504E )  // {'N','P','D','S'}
#I#define PCI_DATA_STRUCT_SIGNATURE_LW2   ( 0x53494752 )  // {'R','G','I','S'}
#I#define PCI_LAST_IMAGE                  ( 0x80 )        // Bit 7
class PciData(FormattedTuple):
    namedtuple = namedtuple("pci_data", "signature vendor device device_list data_len data_rev class_code image_len vendor_rom_dev code_type last_image max_runtime_image_len config_ptr dmtf_pointer")
    struct = Struct("=LHHHHB3sHHBBHHH")

#Istruct LW_PCI_DATA_EXT          //  LWPU PCI Data Extension
#I{
#I    LwU32   signature;          //  00h: Signature, the string "NPDE"
#I    LwU16   lwPciDataExtRev;    //  04h: LWPU PCI Data Extension Revision
#I    LwU16   lwPciDataExtLen;    //  06h: LWPU PCI Data Extension Length
#I    LwU16   subimageLen;        //  08h: Sub-image Length
#I    LwU8    privLastImage;      //  0Ah: Private Last Image Indicator
#I    LwU8    flags0;             //  0Bh: Flags byte 0
#I};
#Idefine LW_PCI_DATA_EXT_SIGNATURE   ( 0x4544504E )  // {'N','P','D','E'}
#Idefine LW_PCI_DATA_EXT_REVISION    ( 0x0100 )      // 1.0
#Idefine LW_PCI_DATA_EXT_FLAGS_CHECKSUM_DISABLED 0x04

LW_PCI_DATA_EXT_SIGNATURE = 0x4544504E # {'N','P','D','E'}
class LwPciDataExt(FormattedTuple):
    namedtuple = namedtuple("lw_pci_data", "signature rev len subimage_len priv_last_image flags")
    struct = Struct("=LHHHBB")

#I https://confluence.lwpu.com/display/GFWT/Redundant+Firmware%3A+Design#RedundantFirmware:Design-ROMDirectory
class RomDirectory(FormattedTuple):
    namedtuple = namedtuple("rom_directory", "identifier version structure_size pci_option_rom_offset pci_option_rom_size inforom_offset_physical inforom_size bootloader_offset secondary_base")
    struct = Struct("=LHH6L")

class NbsiHeader(FormattedTuple):
    namedtuple = namedtuple("nbsi_header", "signature size num_globs dir_rev")
    struct = Struct("=LLBB")

class NbsiGlobHeader(FormattedTuple):
    namedtuple = namedtuple("nbsi_glob_header", "hash type size ver")
    struct = Struct("=QHLH")

class BitBiosData(FormattedTuple):
    namedtuple = namedtuple("bit_bios_data", "bios_version bios_oem_version_number")
    struct = Struct("=LB")

def chrify(data, size):
    chrs = ""
    for i in range(0, size):
        char = (data >> (i * 8)) & 0xff
        chrs = chr(char) + chrs
    return chrs

class VbiosSection(object):
    def __init__(self, vbios, name, offset, size, header):
        self.vbios = vbios
        self.name = name
        self.offset = offset
        self.size = size
        self.header = header

    def __str__(self):
        return "Section {0} at offset {1} (0x{1:x}) size {2} (0x{2:x}) header {3}".format(self.name, self.offset, self.size, self.header)

class GpuVbios(object):
    def __init__(self, data):
        self.data = bytearray(data)
        self.size = len(self.data)
        self.sections = {}

        self._init_images()
        self._init_bit_tokens()

    def _init_bit_tokens(self):
        BIT_HEADER_ID = 0xb8ff
        BIT_HEADER_SIGNATURE = 0x00544942 # "BIT\0"
        #I bios_U016 Id;            // BMP=0x7FFF/BIT=0xB8FF
        #I bios_U032 Signature;     // 0x00544942 - BIT Data Structure Signature
        #I bios_U016 BCD_Version;   // BIT Version - 0x0100 for 1.00
        #I bios_U008 HeaderSize;    // This version is 12 bytes long
        #I bios_U008 TokenSize;     // This version has 6 byte long Tokens
        #I bios_U008 TokenEntries;  // Number of Entries
        #I bios_U008 HeaderChksum;  // 0 Checksum of the header
        BitHeader = namedtuple("bit_header", "id sig bcd_version header_size token_size token_entries chksum")
        bit_header_fmt = "=HLHBBBB"

        #I bios_U008 TokenId;
        #I bios_U008 DataVersion;
        #I bios_U016 DataSize;
        #I bios_U016 DataPtr;
        BitToken = namedtuple("bit_token", "id version size ptr")
        bit_token_fmt = "=BBHH"

        for offset in range(self.size - 2):
            if self.read16(offset) != BIT_HEADER_ID:
                continue
            if self.read32(offset+2) != BIT_HEADER_SIGNATURE:
                continue
            self.bit_offset = offset
            self.bit_header = BitHeader._make(struct.unpack(bit_header_fmt, self.data[self.bit_offset : self.bit_offset + 12]))
            break

        assert(self.bit_offset)

        self.bit_tokens = []
        for token in range(self.bit_header.token_entries):
            token_offset = self.bit_offset + self.bit_header.header_size + token * self.bit_header.token_size
            token_data = self.data[token_offset : token_offset + self.bit_header.token_size]
            token = BitToken._make(struct.unpack(bit_token_fmt, token_data))
            #I All bit token pointers are offset by the IFR image, if present
            token = token._replace(ptr=token.ptr + self.ifr_image_size)
            self.bit_tokens.append(token)

        for token in self.bit_tokens:
            if chr(token.id) == "B":
                self._init_bios_data(token)
            elif chr(token.id) == "p":
                if token.version == 2:
                    self._init_ucodes(token)
            elif chr(token.id) == "I":
                self._init_table_ptrs(token)

    def _init_bios_data(self, bios_data_token):
        bios_data = self.read_formatted_tuple(BitBiosData, bios_data_token.ptr)
        version_bytes = []
        for i in range(0, 4):
            version_bytes.append(bios_data.bios_version >> (i * 8) & 0xff)
        version_bytes.reverse()
        version_bytes.append(bios_data.bios_oem_version_number)
        self.version = "{:02X}.{:02X}.{:02X}.{:02X}.{:02X}".format(*version_bytes)

    def _parse_rom_header(self, offset):
        rom_header = self.read_formatted_tuple(RomHeader, offset)

        pci_data_offset = offset + rom_header.ptr
        pci_data = self.read_formatted_tuple(PciData, pci_data_offset)

        image_len = pci_data.image_len
        last_image = pci_data.last_image & 0x80 != 0

        lw_pci_data_ext_offset = pci_data_offset + pci_data.data_len
        lw_pci_data_ext_offset = (lw_pci_data_ext_offset + 15) & ~0xf
        lw_pci_data_ext = self.read_formatted_tuple(LwPciDataExt, lw_pci_data_ext_offset)
        if lw_pci_data_ext.signature == LW_PCI_DATA_EXT_SIGNATURE:
            image_len = lw_pci_data_ext.subimage_len
            last_image = (lw_pci_data_ext.priv_last_image & 0x80) != 0

        if pci_data.code_type == 3:
            self.uefi_image_size = image_len * 512

        if pci_data.code_type == 0x70:
            #I https://wiki.lwpu.com/engwiki/index.php/VBIOS/Layout/PCI_Expansion_ROM
            # The NBSI header could be in one of two places, check both
            nbsi_offsets = [offset + self.read16(offset + 0x16), pci_data_offset + pci_data.data_len]
            for o in nbsi_offsets:
                nbsi_sig = self.read32(o)
                #I 0x4e425349 'ISBN'
                if nbsi_sig == 0x4e425349:
                    nbsi_header = self.read_formatted_tuple(NbsiHeader, o)

                    glob_header_offset = o + NbsiHeader._size() + nbsi_header.num_globs * 2
                    for g in range(0, nbsi_header.num_globs):
                        glob_header = self.read_formatted_tuple(NbsiGlobHeader, glob_header_offset)
                        glob_header_type = chrify(glob_header.type, 2)

                        add_section = None
                        if glob_header_type == "IR":
                            add_section = "inforom"
                        elif glob_header_type == "IB":
                            add_section = "inforom_backup"
                        elif glob_header_type == "EL":
                            add_section = "erase_ledger"

                        if add_section:
                            section = VbiosSection(self, add_section, glob_header_offset, glob_header.size, glob_header)
                            self.add_section(section)

                        glob_header_offset += glob_header.size

        return (image_len * 512, last_image)

    def _init_images(self):
        self.ifr_image_size = 0
        self.uefi_image_size = 0

        offset = 0

        last_image = False
        while not last_image:
            if self.read32(offset) == 0x4947564e:
                ifr_header = self.read_formatted_tuple(IfrHeader, offset)

                if ifr_header.version <= 0x2:
                    #I https://wiki.lwpu.com/engwiki/index.php/VBIOS/VBIOS_Various_Topics/IFR#Version_0x01
                    image_len = self.read32(offset + 0x14)

                if ifr_header.version == 0x3:
                    #I https://confluence.lwpu.com/display/GFWT/Redundant+Firmware%3A+Design#RedundantFirmware:Design-Navigation
                    rom_dir_offset = self.read32(offset + ifr_header.total_data_size)
                    rom_dir_offset += 4096
                    rom_dir_sig = self.read32(rom_dir_offset)
                    #I 'RFRD'
                    if rom_dir_sig == 0x44524652:
                        rom_directory = self.read_formatted_tuple(RomDirectory, rom_dir_offset)
                        image_len = rom_directory.pci_option_rom_offset

                        # On Turing, inforom is outside of the chain of images.
                        # The corresponding ROM header can be found before the
                        # Inforom data, always 512 bytes before.
                        # Parse it first.
                        self._parse_rom_header(rom_directory.inforom_offset_physical - 512)

                self.ifr_image_size = image_len
                self.add_section(VbiosSection(self, "ifr", offset, image_len, ifr_header))

                offset += image_len
                continue

            (image_len, last_image) = self._parse_rom_header(offset)
            offset += image_len

    def _init_table_ptrs(self, token):
        #Istruct BIT_DATA_LWINIT_PTRS_V1
        #I{
        #I   bios_U016 InitScriptTablePtr;      // Init script table pointer
        #I   bios_U016 MacroIndexTablePtr;      // Macro index table pointer
        #I   bios_U016 MacroTablePtr;           // Macro table pointer
        #I   bios_U016 ConditionTablePtr;       // Condition table pointer
        #I   bios_U016 IoConditionTablePtr;     // IO Condition table pointer
        #I   bios_U016 IoFlagConditionTablePtr; // IO Flag Condition table pointer
        #I   bios_U016 InitFunctionTablePtr;    // Init Function table pointer
        #I   bios_U016 VBIOSPrivateTablePtr;    // VBIOS private table pointer
        #I   bios_U016 DataArraysTablePtr;      // Data arrays table pointer
        #I   bios_U016 PCIESettingsScriptPtr;   // PCI-E settings script pointer
        #I   bios_U016 DevinitTablesPtr;        // Pointer to tables required by Devinit opcodes
        #I   bios_U016 DevinitTablesSize;       // Size of tables required by Devinit opcodes
        #I   bios_U016 BootScriptsPtr;          // Pointer to Devinit Boot Scripts
        #I   bios_U016 BootScriptsSize;         // Size of Devinit Boot Scripts
        #I   bios_U016 LwlinkConfigDataPtr;     // Pointer to LWLink Config Data
        #I}

        LwinitPtrs = namedtuple("lwinit_ptrs", "init_script_table_ptr macro_index_table_ptr macro_table_ptr "
                                                "condition_table_ptr io_condition_table_ptr io_flag_condition_table_ptr "
                                                "init_function_table_ptr vbios_private_table_ptr data_arrays_table_ptr "
                                                "pcie_settings_script_ptr "
                                                "devinit_tables_ptr devinit_tables_size boot_scripts_ptr boot_scripts_size "
                                                "lwlink_config_data_ptr ")
        lwinit_ptrs_fmt = "=15H"

        self.lwinit_ptrs = LwinitPtrs._make(self.read_format(lwinit_ptrs_fmt, token.ptr))

    def _init_ucodes(self, ucode_token):
        exp_rom_offset = self.uefi_image_size
        ucode_table_offset = self.read32(ucode_token.ptr) + exp_rom_offset + self.ifr_image_size

        #I // Version 1.0 of Falcon Ucode Table Structure
        #Itypedef struct
        #I{
        #I    bios_U008 Version;                  // Falcon Ucode table version (0x01)
        #I    bios_U008 HeaderSize;               // Size of Ucode Tbl Header in bytes(6)
        #I    bios_U008 EntrySize;                // Size of Ucode Tbl Entry in Bytes(6)
        #I    bios_U008 EntryCount;               // Number of table Entries
        #I    bios_U008 DescVersion;              // Version of Falcon Ucode Descriptor
        #I    bios_U008 DescSize;                 // Size of Falcon Ucode Descriptor
        #I} FALCON_UCODE_TABLE_HDR_V1;

        UcodeTableHdr = namedtuple("ucode_table_hdr", "version header_size entry_size entry_count desc_version desc_size")
        ucode_table_hdr_fmt = "=BBBBBB"

        #I#define FALCON_UCODE_TABLE_HDR_V1_VERSION   1
        #I#define FALCON_UCODE_TABLE_HDR_V1_SIZE_6    6
        #I#define FALCON_UCODE_TABLE_HDR_V1_6_FMT     "6b"
        #I
        #I// Version 1.0 of Falcon Ucode Table Entry
        #Itypedef struct
        #I{
        #I    bios_U008 ApplicationID;            // ID of application implemented by ucode image
        #I    bios_U008 TargetID;                 // ID of Falcon Instance targeted by ucode image
        #I    bios_U032 DescPtr;                  // Pointer to Falcon Ucode Descriptor
        #I} FALCON_UCODE_TABLE_ENTRY_V1;
        UcodeTableEntry = namedtuple("ucode_table_entry", "app_id target_id desc_ptr")
        ucode_table_entry_fmt = "=BBL"

        #I typedef struct
        #I{
        #I    union _VerInd
        #I    {
        #I        FALCON_UCODE_DESC_HEADER Hdr;   // Header struct
        #I        bios_U032 StoredSize;           // Size of stored data (Please read the wiki to find why StoredSize is here)
        #I    } HdrSzUn ;                         // Header and Size union
        #I    bios_U032 UncompressedSize;         // Size of uncompressed ucode image
        #I    bios_U032 VirtualEntry;             // Code Entry Point
        #I    bios_U032 InterfaceOffset;          // DMEM address to App Interface Table
        #I    bios_U032 IMEMPhysBase;             // Physical Addr at which to load IMEM segment
        #I    bios_U032 IMEMLoadSize;             // Size of data to load into IMEM
        #I    bios_U032 IMEMVirtBase;             // Virtual Address of IMEM segment to load
        #I    bios_U032 IMEMSecBase;              // Virtual Address of secure IMEM segment
        #I    bios_U032 IMEMSecSize;              // Size of secure IMEM segment
        #I    bios_U032 DMEMOffset;               // Offset into store (uncompressed) image at
        #I                                        // which DMEM starts
        #I    bios_U032 DMEMPhysBase;             // Physical Address at which to load DMEM segment
        #I    bios_U032 DMEMLoadSize;             // Size of data to load into DMEM
        #I} FALCON_UCODE_DESC_V1;
        UcodeDescV1 = namedtuple("ucode_desc_v1", "size uncompressed_size virtual_entry interface_offset imem_phys_base imem_load_size imem_virt_base imem_sec_base imem_sec_size dmem_offset dmem_phys_base dmem_load_size")
        ucode_desc_v1_fmt = "=12L"

        table_hdr = UcodeTableHdr._make(self.read_format(ucode_table_hdr_fmt, ucode_table_offset))
        #Idebug("Table hdr %s", table_hdr)
        for entry_index in range(table_hdr.entry_count):
            entry_offset = ucode_table_offset + table_hdr.header_size + entry_index * table_hdr.entry_size
            ucode_entry = UcodeTableEntry._make(self.read_format(ucode_table_entry_fmt, entry_offset))
            if ucode_entry.app_id == 0:
                continue
            ucode_desc_offset = self.ifr_image_size + ucode_entry.desc_ptr + exp_rom_offset
            desc_header = self.read32(ucode_desc_offset)
            if desc_header & 1 == 1:
                #Idebug("Unhandled entry %s desc %s", ucode_entry, hex(desc_header))
                continue
            ucode_desc = UcodeDescV1._make(self.read_format(ucode_desc_v1_fmt, ucode_desc_offset))
            #Idebug(" entry offset %s entry %s desc %s", hex(entry_offset), ucode_entry, ucode_desc)
            if ucode_entry.app_id == 4:
                self._init_devinit(ucode_desc_offset + table_hdr.desc_size, ucode_desc)

    def _init_devinit(self, offset, desc):
        self.devinit_ucode_desc = desc
        self.devinit_ucode_offset = offset

        #I{
        #I    LwU8 version;                       // Header version
        #I    LwU8 headerSize;                    // Header size
        #I    LwU8 entrySize;                     // Application interface entry size
        #I    LwU8 entryCount;                    // Number of entries
        #I} FALCON_APPLICATION_INTERFACE_HEADER_V1, *PFALCON_APPLICATION_INTERFACE_HEADER_V1;
        AppInterfaceHeader = namedtuple("app_interface_header", "version header_size entry_size entry_count")
        app_interface_header_fmt = "=BBBB"

        #I{
        #I    bios_U032 InterfaceID;    // ID of Application Interface
        #I    bios_U032 InterfacePtr;   // Pointer to Application Interface structure
        #I} FLCN_APPIF_TABLE_ENTRY_V1;
        AppInterfaceEntry = namedtuple("app_interface_entry", "interface_id interface_ptr")
        app_interface_entry_fmt = "=LL"

        #I// DEVINIT ENGINE APP IF
        #Itypedef struct
        #I{
        #I    /*
        #I    LwU16   version;        // Devinit Interface version (0x1)
        #I    LwU16   structSize;     // Devinit Interface size (56)
        #I    */
        #I    LwU32   field0;
        #I#define LW_FLCN_APPIF_DEVENG_FLD0_VERSION           15:0
        #I#define LW_FLCN_APPIF_DEVENG_FLD0_VERSION_1         1
        #I#define LW_FLCN_APPIF_DEVENG_FLD0_STRUCT_SIZE       31:16
        #I#define LW_FLCN_APPIF_DEVENG_FLD0_STRUCT_SIZE_56    56
        #I    /*
        #I    LwU16   appVersion;
        #I    LwU16   reserved06;
        #I    */
        #I    LwU32   field1;
        #I#define LW_FLCN_APPIF_DEVENG_FLD1_APP_VERSION       15:0
        #I#define LW_FLCN_APPIF_DEVENG_FLD1_RSVD06            31:16
        #I    LwU32   tablesPhysBase; // DMEM address at which to load Devinit Tables
        #I    LwU32   tablesVirtBase; // Source tables virt address
        #I    LwU32   scriptPhysBase; // DMEM address at which to load Devinit Script
        #I    LwU32   scriptVirtBase; // Devinit script virt address;
        #I    LwU32   scriptVirtEntry;// Source script addr at which to begin exec
        #I    /*
        #I    LwU16   scriptSize;     // Size of Devinit script
        #I    LwU8    memStrapDataCnt;// Number of strap data entries implemented by XMEMSEL
        #I    LwU8    reserved28;
        #I    */
        #I    LwU32   field7;
        #I#define LW_FLCN_APPIF_DEVENG_FLD7_SCRIPT_SIZE       15:0
        #I#define LW_FLCN_APPIF_DEVENG_FLD7_STRAP_COUNT       23:16
        #I#define LW_FLCN_APPIF_DEVENF_FLD7_RSVD28            31:16
        #I    LwU32   memInfoTblVirtBase; // addr of the MemInfo Tbl
        #I    LwU32   emptyScriptVirtBase;// addr of the empty script
        #I    LwU32   condTblVirtBase;    // addr of the Condition tbl
        #I    LwU32   ioCondTblVirtBase;  // addr of the io condition tbl
        #I    LwU32   dataTblVirtBase;    // addr of data arrays tbl
        #I    LwU32   gpioTblVirtBase;    // addr of gpio assignment tbl
        #I} FLCN_APPIF_DEVENG;
        FalconAppIf = namedtuple("flcn_app_interface", "version struct_size app_version tables_phys_base tables_virt_base script_phys_base script_virt_base script_virt_entry script_size mem_strap_data_cnt mem_info_tbl_virt_base empty_script_virt_base cond_tbl_virt_base data_tbl_virt_base gpio_tbl_virt_base")
        falcon_app_if_fmt = "=HHHxx LLLL HBx LLLLLL"

        ucode_src = offset + desc.dmem_offset
        bios_dmem_offset = offset + desc.dmem_offset
        interface_hdr = AppInterfaceHeader._make(self.read_format(app_interface_header_fmt, bios_dmem_offset + desc.interface_offset))
        entry_offset = ucode_src + desc.interface_offset + interface_hdr.header_size
        interface_entry = AppInterfaceEntry._make(self.read_format(app_interface_entry_fmt, entry_offset))
        self.devinit_app_if = FalconAppIf._make(self.read_format(falcon_app_if_fmt, bios_dmem_offset + interface_entry.interface_ptr))

    def add_section(self, section):
        for name in self.sections:
            s = self.sections[name]
            if s.offset <= section.offset and s.offset + s.size > section.offset:
                assert 0, "New {} overlapping existing {}".format(section, s)
        self.sections[section.name] = section

    def find_section_with_offset(self, offset):
        for name in self.sections:
            s = self.sections[name]
            if s.offset <= offset and s.offset + s.size > offset:
                return s

        return None

    def read_format(self, fmt, offset):
        size = struct.calcsize(fmt)
        return struct.unpack(fmt, self.data[offset : offset + size])

    def read_formatted_tuple(self, fmtTuple, offset):
        size = fmtTuple._size()
        return fmtTuple._make(self.data[offset : offset + size])

    def read(self, offset, size):
        data = self.data[offset : offset + size]
        return int_from_data(data, size)

    def read8(self, offset):
        return self.read(offset, 1)

    def read16(self, offset):
        return self.read(offset, 2)

    def read32(self, offset):
        return self.read(offset, 4)

    def read32_array(self, offset, size_in_bytes):
        data = []
        for i in range(0, size_in_bytes, 4):
            data.append(self.read32(offset + i))

        return data

class GpuMemPort(object):
    def __init__(self, name, mem_control_reg, max_size, falcon):
        self.name = name
        self.control_reg = mem_control_reg
        self.data_reg = self.control_reg + LW_PPWR_FALCON_IMEMD(0) - LW_PPWR_FALCON_IMEMC(0)
        self.offset = 0
        self.max_size = max_size
        self.auto_inc_read = False
        self.auto_inc_write = False
        self.selwre_imem = False
        self.falcon = falcon

    def __str__(self):
        return "%s offset %d (0x%x) incr %d incw %d max size %d (0x%x) control reg 0x%x = 0x%x" % (self.name,
                self.offset, self.offset, self.auto_inc_read, self.auto_inc_write,
                self.max_size, self.max_size,
                self.control_reg, self.falcon.gpu.read(self.control_reg))

    def configure(self, offset, inc_read=True, inc_write=True, selwre_imem=False):
        need_to_write = False

        if offset != self.offset:
            self.offset = offset
            need_to_write = True

        if self.auto_inc_read != inc_read:
            self.auto_inc_read = inc_read
            need_to_write = True

        if self.auto_inc_write != inc_write:
            self.auto_inc_write = inc_write
            need_to_write = True

        if self.selwre_imem != selwre_imem:
            self.selwre_imem = selwre_imem
            need_to_write = True

        if not need_to_write:
            return

        memc_value = offset
        if inc_read:
            memc_value |= LW_PPWR_FALCON_IMEMC_AINCR_TRUE
        if inc_write:
            memc_value |= LW_PPWR_FALCON_IMEMC_AINCW_TRUE
        if selwre_imem:
            memc_value |= LW_PPWR_FALCON_IMEMC_SELWRE_ENABLED

        self.falcon.gpu.write(self.control_reg, memc_value)

    def handle_offset_wraparound(self):
        if self.offset == self.max_size:
            self.configure(0, self.auto_inc_read, self.auto_inc_write, self.selwre_imem)

    def read(self, size):
        data = []
        for offset in range(0, size, 4):
            # MEM could match 0xbadf... so use read_bad_ok()
            data.append(self.falcon.gpu.read_bad_ok(self.data_reg))

        if self.auto_inc_read:
            self.offset += size

        self.handle_offset_wraparound()

        return data

    def write(self, data, debug_write=False):
        for d in data:
            if debug_write:
                control = self.falcon.gpu.read(self.control_reg)
                debug("Writing data %s = %s offset %s, control %s", hex(self.data_reg), hex(d), hex(self.offset), hex(control))
            self.falcon.gpu.write(self.data_reg, d)
            if self.auto_inc_write:
                self.offset += 4

        self.handle_offset_wraparound()

class GpuImemPort(GpuMemPort):
    def __init__(self, name, mem_control_reg, max_size, falcon):
        super(GpuImemPort, self).__init__(name, mem_control_reg, max_size, falcon)
        self.imemt_reg = self.control_reg + LW_PPWR_FALCON_IMEMT(0) - LW_PPWR_FALCON_IMEMC(0)

    def write_with_tags(self, data, virt_base, debug_write=False):
        self.falcon.gpu.write(0x840040, 1234)
        for data_32 in data:
            if virt_base & 0xff == 0:
                if debug_write:
                    debug("Writing tag %s = %s offset %s", hex(self.imemt_reg), hex(virt_base), hex(self.offset))
                self.falcon.gpu.write(self.imemt_reg, virt_base >> 8)

            if debug_write:
                control = self.falcon.gpu.read(self.control_reg)
                debug("Writing data %s = %s offset %s, control %s", hex(self.data_reg), hex(data_32), hex(self.offset), hex(control))
            self.falcon.gpu.write(self.data_reg, data_32)

            virt_base += 4
            self.offset += 4

        while virt_base & 0xff != 0:
            self.falcon.gpu.write(self.data_reg, 0)
            virt_base += 4
            self.offset += 4

        self.handle_offset_wraparound()

class GpuFalcon(object):
    def __init__(self, name, cpuctl, gpu, pmc_enable_mask=None):
        self.name = name
        self.gpu = gpu
        self.cpuctl = cpuctl
        self.pmc_enable_mask = pmc_enable_mask
        self.no_outside_reset = getattr(self, 'no_outside_reset', False)
        self.has_emem = getattr(self, 'has_emem', False)
        self._max_imem_size = None
        self._max_dmem_size = None
        self._max_emem_size = None
        self._imem_port_count = None
        self._dmem_port_count = None

        self.enable()
        self.mem_spaces = ["imem", "dmem"]

        self.imem_ports = []
        for p in range(0, self.imem_port_count):
            name = self.name + "_imem_%d" % p
            mem_control_reg = self.imemc + p * 16
            max_size = self.max_imem_size
            self.imem_ports.append(GpuImemPort(name, mem_control_reg, max_size, self))

        self.dmem_ports = []
        for p in range(0, self.dmem_port_count):
            name = self.name + "_dmem_%d" % p
            mem_control_reg = self.dmemc + p * 8
            max_size = self.max_dmem_size
            self.dmem_ports.append(GpuMemPort(name, mem_control_reg, max_size, self))

        if self.has_emem:
            self.mem_spaces.append("emem")
            self._init_emem_ports()

    def _init_emem_ports(self):
        assert self.has_emem
        self.emem_ports = []
        name = self.name + "_emem_0"
        self.emem_ports.append(GpuMemPort(name, self.cpuctl + 0x9c0, self.max_emem_size, self))

    @property
    def imemc(self):
        return self.cpuctl + LW_PPWR_FALCON_IMEMC(0) - LW_PPWR_FALCON_CPUCTL

    @property
    def dmemc(self):
        return self.cpuctl + LW_PPWR_FALCON_DMEMC(0) - LW_PPWR_FALCON_CPUCTL

    @property
    def bootvec(self):
        return self.cpuctl + LW_PPWR_FALCON_BOOTVEC - LW_PPWR_FALCON_CPUCTL

    @property
    def dmactl(self):
        return self.cpuctl + LW_PPWR_FALCON_DMACTL - LW_PPWR_FALCON_CPUCTL

    @property
    def engine_reset(self):
        return self.cpuctl + LW_PPWR_FALCON_ENGINE_RESET - LW_PPWR_FALCON_CPUCTL

    @property
    def hwcfg(self):
        return self.cpuctl + LW_PPWR_FALCON_HWCFG - LW_PPWR_FALCON_CPUCTL

    @property
    def hwcfg1(self):
        return self.cpuctl + LW_PPWR_FALCON_HWCFG1 - LW_PPWR_FALCON_CPUCTL

    @property
    def hwcfg_emem(self):
        return self.cpuctl + 0x9bc

    @property
    def dmemd(self):
        return self.dmemc + LW_PPWR_FALCON_IMEMD(0) - LW_PPWR_FALCON_IMEMC(0)

    @property
    def imemd(self):
        return self.imemc + LW_PPWR_FALCON_IMEMD(0) - LW_PPWR_FALCON_IMEMC(0)

    @property
    def imemt(self):
        return self.imemc + LW_PPWR_FALCON_IMEMT(0) - LW_PPWR_FALCON_IMEMC(0)

    @property
    def max_imem_size(self):
        if self._max_imem_size:
            return self._max_imem_size

        if self.name not in self.gpu.falcons_cfg:
            error("Missing imem/dmem config for falcon %s, falling back to hwcfg", self.name)
            self._max_imem_size = self.max_imem_size_from_hwcfg()
        else:
            # Use the imem size provided in the GPU config
            self._max_imem_size = self.gpu.falcons_cfg[self.name]["imem_size"]

        # And make sure it matches HW
        assert self._max_imem_size == self.max_imem_size_from_hwcfg(), "%d != %d" % (self._max_imem_size, self.max_imem_size_from_hwcfg())

        return self._max_imem_size

    @property
    def max_dmem_size(self):
        if self._max_dmem_size:
            return self._max_dmem_size

        if self.name not in self.gpu.falcons_cfg:
            error("Missing imem/dmem config for falcon %s, falling back to hwcfg", self.name)
            self._max_dmem_size = self.max_dmem_size_from_hwcfg()
        else:
            # Use the dmem size provided in the GPU config
            self._max_dmem_size = self.gpu.falcons_cfg[self.name]["dmem_size"]

        # And make sure it matches HW
        assert self._max_dmem_size == self.max_dmem_size_from_hwcfg(), "%d != %d" % (self._max_dmem_size, self.max_dmem_size_from_hwcfg())

        return self._max_dmem_size

    @property
    def max_emem_size(self):
        if self._max_emem_size:
            return self._max_emem_size

        if self.name not in self.gpu.falcons_cfg or "max_emem" not in self.gpu.falcons_cfg[self.name]:
            error("Missing emem config for falcon %s, falling back to hwcfg", self.name)
            self._max_emem_size = self.max_emem_size_from_hwcfg()
        else:
            # Use the emem size provided in the GPU config
            self._max_emem_size = self.gpu.falcons_cfg[self.name]["emem_size"]

        # And make sure it matches HW
        assert self._max_emem_size == self.max_emem_size_from_hwcfg(), "%d != %d" % (self._max_emem_size, self.max_emem_size_from_hwcfg())

        return self._max_emem_size

    @property
    def dmem_port_count(self):
        if self._dmem_port_count:
            return self._dmem_port_count

        if self.name not in self.gpu.falcons_cfg or "dmem_port_count" not in self.gpu.falcons_cfg[self.name]:
            error("%s missing dmem port count for falcon %s, falling back to hwcfg", self.gpu, self.name)
            self._dmem_port_count = self.dmem_port_count_from_hwcfg()
        else:
            # Use the dmem port count provided in the GPU config
            self._dmem_port_count = self.gpu.falcons_cfg[self.name]["dmem_port_count"]

        # And make sure it matches HW
        assert self._dmem_port_count == self.dmem_port_count_from_hwcfg(), "%d != %d" % (self._dmem_port_count, self.dmem_port_count_from_hwcfg())

        return self._dmem_port_count

    @property
    def imem_port_count(self):
        if self._imem_port_count:
            return self._imem_port_count

        if self.name not in self.gpu.falcons_cfg or "imem_port_count" not in self.gpu.falcons_cfg[self.name]:
            error("%s missing imem port count for falcon %s, falling back to hwcfg", self.gpu, self.name)
            self._imem_port_count = self.imem_port_count_from_hwcfg()
        else:
            # Use the imem port count provided in the GPU config
            self._imem_port_count = self.gpu.falcons_cfg[self.name]["imem_port_count"]

        # And make sure it matches HW
        assert self._imem_port_count == self.imem_port_count_from_hwcfg(), "%d != %d" % (self._imem_port_count, self.imem_port_count_from_hwcfg())

        return self._imem_port_count

    def max_imem_size_from_hwcfg(self):
        hwcfg = self.gpu.read(self.hwcfg)
        return (hwcfg & 0x1ff) * 256

    def max_dmem_size_from_hwcfg(self):
        hwcfg = self.gpu.read(self.hwcfg)
        return ((hwcfg >> 9) & 0x1ff) * 256

    def max_emem_size_from_hwcfg(self):
        assert self.has_emem
        hwcfg = self.gpu.read(self.hwcfg_emem)
        return (hwcfg & 0x1ff) * 256

    def imem_port_count_from_hwcfg(self):
        hwcfg = self.gpu.read(self.hwcfg1)
        return ((hwcfg >> 8) & 0xf)

    def dmem_port_count_from_hwcfg(self):
        hwcfg = self.gpu.read(self.hwcfg1)
        return ((hwcfg >> 12) & 0xf)

    def get_mem_ports(self, mem):
        if mem == "imem":
            return self.imem_ports
        elif mem == "dmem":
            return self.dmem_ports
        elif mem == "emem":
            assert self.has_emem
            return self.emem_ports
        else:
            assert 0, "Unknown mem %s" % mem

    def get_mem_port(self, mem, port=0):
        return self.get_mem_ports(mem)[port]

    def load_imem(self, data, phys_base, virt_base, secure=False, virtual_tag=True, debug_load=False):
        self.imem_ports[0].configure(offset=phys_base, selwre_imem=secure)
        if virtual_tag:
            self.imem_ports[0].write_with_tags(data, virt_base=virt_base, debug_write=debug_load)
        else:
            self.imem_ports[0].write(data, debug_write=debug_load)

    def read_port(self, port, phys_base, size):
        port.configure(offset=phys_base)
        return port.read(size)

    def write_port(self, port, data, phys_base, debug_write=False):
        port.configure(offset=phys_base)
        port.write(data, debug_write)

    def read_imem(self, phys_base, size):
        return self.read_port(self.imem_ports[0], phys_base, size)

    def load_dmem(self, data, phys_base, debug_load=False):
        self.write_port(self.dmem_ports[0], data, phys_base, debug_write=debug_load)

    def read_dmem(self, phys_base, size):
        return self.read_port(self.dmem_ports[0], phys_base, size)

    def execute(self, bootvec=0, wait=True):
        self.gpu.write(self.bootvec, bootvec)
        self.gpu.write(self.dmactl, 0)
        self.gpu.write(self.cpuctl, 2)
        if wait:
            self.wait_for_halt()

    def wait_for_halt(self):
        self.gpu.poll_register(self.name + " cpuctl", self.cpuctl, 0x10, 5)

    def disable(self):
        if self.no_outside_reset:
            # No outside reset means best we can do is to reset through cpuctl
            # HRESET and wait for a HALT
            self.gpu.write(self.cpuctl, 0x1 << 3)
            self.wait_for_halt()
        elif self.pmc_enable_mask:
            pmc_enable = self.gpu.read(LW_PMC_ENABLE)
            self.gpu.write(LW_PMC_ENABLE, pmc_enable & ~self.pmc_enable_mask)
        else:
            self.gpu.write(self.engine_reset, 1)

    def enable(self):
        if self.no_outside_reset:
            pass
        elif self.pmc_enable_mask:
            pmc_enable = self.gpu.read(LW_PMC_ENABLE)
            self.gpu.write(LW_PMC_ENABLE, pmc_enable | self.pmc_enable_mask)
        else:
            self.gpu.write(self.engine_reset, 0)

        self.gpu.poll_register(self.name + " dmactl", self.dmactl, value=0, timeout=1, mask=0x6)

    def reset(self):
        self.disable()
        self.enable()

    def is_halted(self):
        cpuctl = self.gpu.read(self.cpuctl)
        return cpuctl & 0x10 != 0

    def is_disabled(self):
        assert not self.no_outside_reset
        if self.pmc_enable_mask:
            pmc_enable = self.gpu.read(LW_PMC_ENABLE)
            return (pmc_enable & self.pmc_enable_mask) == 0
        else:
            return self.gpu.read(self.engine_reset) == 1

class PmuFalcon(GpuFalcon):
    def __init__(self, gpu):
        if gpu.is_pmu_reset_in_pmc:
            pmc_enable_mask = LW_PMC_ENABLE_PWR
        else:
            pmc_enable_mask = None
        super(PmuFalcon, self).__init__("pmu", LW_PPWR_FALCON_CPUCTL, gpu, pmc_enable_mask=pmc_enable_mask)

    def reset(self):
        self.gpu.stop_preos()
        super(PmuFalcon, self).reset()

class MsvldFalcon(GpuFalcon):
    def __init__(self, gpu):
        pmc_enable_mask = LW_PMC_ENABLE_MSVLD
        super(MsvldFalcon, self).__init__("msvld", LW_PMSVLD_FALCON_CPUCTL, gpu, pmc_enable_mask=pmc_enable_mask)

class MspppFalcon(GpuFalcon):
    def __init__(self, gpu):
        pmc_enable_mask = LW_PMC_ENABLE_MSPPP
        super(MspppFalcon, self).__init__("msppp", LW_PMSPPP_FALCON_CPUCTL, gpu, pmc_enable_mask=pmc_enable_mask)

class MspdecFalcon(GpuFalcon):
    def __init__(self, gpu):
        pmc_enable_mask = LW_PMC_ENABLE_MSPDEC
        super(MspdecFalcon, self).__init__("mspdec", LW_PMSPDEC_FALCON_CPUCTL, gpu, pmc_enable_mask=pmc_enable_mask)

class MsencFalcon(GpuFalcon):
    def __init__(self, gpu):
        pmc_enable_mask = LW_PMC_ENABLE_MSENC
        super(MsencFalcon, self).__init__("msenc", LW_PMSENC_FALCON_CPUCTL, gpu, pmc_enable_mask=pmc_enable_mask)

class HdaFalcon(GpuFalcon):
    def __init__(self, gpu):
        self.no_outside_reset = True
        super(HdaFalcon, self).__init__("hda", LW_PHDAFALCON_FALCON_CPUCTL, gpu)

class DispFalcon(GpuFalcon):
    def __init__(self, gpu):
        pmc_enable_mask = LW_PMC_ENABLE_PDISP
        super(DispFalcon, self).__init__("disp", LW_PDISP_FALCON_CPUCTL, gpu, pmc_enable_mask=pmc_enable_mask)

class GspFalcon(GpuFalcon):
    def __init__(self, gpu):
        if gpu.is_volta_plus:
            self.has_emem = True

        super(GspFalcon, self).__init__("gsp", LW_PGSP_FALCON_CPUCTL, gpu)

class SecFalcon(GpuFalcon):
    def __init__(self, gpu):
        if gpu.is_turing_plus:
            psec_cpuctl = LW_PSEC_FALCON_CPUCTL_TURING
        else:
            psec_cpuctl = LW_PSEC_FALCON_CPUCTL_MAXWELL

        if gpu.arch == "maxwell":
            pmc_enable_mask = LW_PMC_ENABLE_SEC
        else:
            pmc_enable_mask = None

        if gpu.is_pascal_10x_plus:
            self.has_emem = True

        super(SecFalcon, self).__init__("sec", psec_cpuctl, gpu, pmc_enable_mask=pmc_enable_mask)

class FbFalcon(GpuFalcon):
    def __init__(self, gpu):
        self.no_outside_reset = True
        super(FbFalcon, self).__init__("fb", LW_PFBFALCON_FALCON_CPUCTL, gpu)

class LwDecFalcon(GpuFalcon):
    def __init__(self, gpu, lwdec=0):
        if gpu.is_turing_plus:
            cpuctl = LW_PLWDEC_FALCON_CPUCTL_TURING(lwdec)
        else:
            cpuctl = LW_PLWDEC_FALCON_CPUCTL_MAXWELL(lwdec)
        pmc_mask = LW_PMC_ENABLE_LWDEC(lwdec)
        super(LwDecFalcon, self).__init__("lwdec%s" % lwdec, cpuctl, gpu, pmc_enable_mask=pmc_mask)

class LwEncFalcon(GpuFalcon):
    def __init__(self, gpu, lwenc=0):
        pmc_mask = LW_PMC_ENABLE_LWENC(lwenc)
        super(LwEncFalcon, self).__init__("lwenc%s" % lwenc, LW_PLWENC_FALCON_CPUCTL(lwenc), gpu, pmc_enable_mask=pmc_mask)

class MinionFalcon(GpuFalcon):
    def __init__(self, gpu):
        pmc_mask = LW_PMC_ENABLE_LWLINK
        super(MinionFalcon, self).__init__("minion", LW_PMINION_FALCON_CPUCTL, gpu, pmc_enable_mask=pmc_mask)

class UnknownGpuError(Exception):
    pass

class BrokenGpuError(Exception):
    pass

class GfwUcodeDescLegacy(FormattedTuple):
    #I $falc_imag_header_v2->{'version'},
    #I $falc_imag_header_v2->{'header_size'},
    #I $falc_imag_header_v2->{'imem_size'},
    #I $falc_imag_header_v2->{'dmem_size'},
    #I $falc_imag_header_v2->{'sec_start'},
    #I $falc_imag_header_v2->{'sec_low_size'},
    #I $falc_imag_header_v2->{'sec_entire_size'},
    #I $falc_imag_header_v2->{'dmem_alt_size'} );
    namedtuple = namedtuple("gfw_ucode_desc_legacy", "version header_size imem_size dmem_size sec_start sec_low_size sec_entire_size dmem_alt_size")
    struct = Struct("=8L")

class GfwUcodeDescV2(FormattedTuple):
    #I $falcon_ucode_descriptor_v2->{'byFlags'},
    #I $falcon_ucode_descriptor_v2->{'byVersion'},
    #I $falcon_ucode_descriptor_v2->{'wStructSize'},
    #I $falcon_ucode_descriptor_v2->{'dwStoredSize'},
    #I $falcon_ucode_descriptor_v2->{'dwUncompressedSize'},
    #I $falcon_ucode_descriptor_v2->{'dwVirtualEntry'},
    #I $falcon_ucode_descriptor_v2->{'dwInterfaceOffset'},
    #I $falcon_ucode_descriptor_v2->{'dwImemPhysicalBase'},
    #I $falcon_ucode_descriptor_v2->{'dwImemLoadSize'},
    #I $falcon_ucode_descriptor_v2->{'dwImemVirtualBase'},
    #I $falcon_ucode_descriptor_v2->{'dwImemSelwreBase'},
    #I $falcon_ucode_descriptor_v2->{'dwImemSelwreSize'},
    #I $falcon_ucode_descriptor_v2->{'dwDmemOffset'},
    #I $falcon_ucode_descriptor_v2->{'dwDmemPhysicalBase'},
    #I $falcon_ucode_descriptor_v2->{'dwDmemLoadSize'},
    #I $falcon_ucode_descriptor_v2->{'dwAltImemLoadSize'},
    #I $falcon_ucode_descriptor_v2->{'dwAltDmemLoadSize'} );
    namedtuple = namedtuple("gfw_ucode_desc_v2", "flags version struct_size stored_size uncompressed_size virtual_entry interface_offset imem_phys_base imem_load_size imem_virtual_base imem_selwre_base imem_selwre_size dmem_offset dmem_physical_base dmem_load_size alt_imem_load_size alt_dmem_load_size")
    struct = Struct("=BBH14L")

class GpuUcode(object):
    def __init__(self, name, binary):
        self.name = name
        self.binary = binary

    @property
    def imem_ns_size(self):
        return len(self.imem_ns) * 4

    @property
    def imem_sec_size(self):
        return len(self.imem_sec) * 4

    @property
    def dmem_size(self):
        return len(self.dmem) * 4

    def __str__(self):
        return "Ucode %s (imem_ns size %d virt 0x%x phys 0x%x, imem_sec size %d virt 0x%x phys 0x%x, dmem size %d base 0x%x)" % (self.name,
                self.imem_ns_size, self.imem_ns_virt_base, self.imem_ns_phys_base,
                self.imem_sec_size, self.imem_sec_virt_base, self.imem_sec_phys_base,
                self.dmem_size, self.dmem_phys_base)

class DriverUcode(GpuUcode):
    def __init__(self, name, binary):
        super(DriverUcode, self).__init__(name, binary)

        self._patch_signature()

        size = binary["header"][1]
        self.imem_ns = binary["data"][0 : size // 4]
        self.imem_ns_virt_base = 0
        self.imem_ns_phys_base = 0

        offset = binary["header"][5]
        size = binary["header"][6]
        self.imem_sec = binary["data"][offset // 4 : (offset + size) // 4]
        self.imem_sec_virt_base = offset
        self.imem_sec_phys_base = offset

        offset = binary["header"][2]
        size = binary["header"][3]
        mem = binary["data"][offset // 4 : (offset + size) // 4]
        self.dmem = binary["data"][offset // 4 : (offset + size) // 4]
        self.dmem_phys_base = 0

        self.virtual_entry = 0

    def _patch_signature(self):
        binary = self.binary
        image = binary["data"]
        for index, location in enumerate(binary["signature_location"]):
            signature_index = binary["signature_index"][index]
            for offset in range(4):
                image[location // 4 + offset] = binary["signature"][signature_index * 4 + offset]

class GfwUcode(GpuUcode):
    def __init__(self, name, binary):
        super(GfwUcode, self).__init__(name, binary)

        if binary[0] & 0x1 == 0x1:
            self._init_v2()
        else:
            assert 0, "Unknown ucode format"

    def _init_v2(self):
        desc = GfwUcodeDescV2._make(self.binary)
        self.desc = desc

        assert desc.flags & 0x1

        # The ucode data is everything but the descriptor, strip it
        ucode_data_bytes = self.binary[desc.struct_size : ]

        # Colwert ucode data to an array of 4-byte ints
        self.ucode_data = []
        for i in range(0, len(ucode_data_bytes), 4):
            self.ucode_data.append(int_from_data(ucode_data_bytes[i : i + 4], 4))

        non_selwre_size = desc.imem_load_size - desc.imem_selwre_size

        self.imem_ns = self.ucode_data[:non_selwre_size // 4]
        self.imem_ns_phys_base = desc.imem_phys_base
        self.imem_ns_virt_base = desc.imem_virtual_base

        if desc.imem_selwre_size != 0:
            selwre_start = non_selwre_size // 4
            selwre_end = selwre_start + desc.imem_selwre_size // 4
            self.imem_sec = self.ucode_data[selwre_start : selwre_end]
            self.imem_sec_phys_base = desc.imem_phys_base + non_selwre_size
            self.imem_sec_virt_base = desc.imem_selwre_base

        self.dmem = self.ucode_data[desc.dmem_offset // 4 : desc.dmem_offset // 4 + desc.dmem_load_size // 4]
        self.dmem_phys_base = desc.dmem_physical_base

        self.virtual_entry = desc.virtual_entry

class Gpu(PciDevice):
    def __init__(self, dev_path):
        self.name = "?"
        self.bar0_addr = 0

        super(Gpu, self).__init__(dev_path)

        if not self.sanity_check_cfg_space():
            debug("%s sanity check of config space failed", self)
            raise BrokenGpuError()

        # Enable MMIO
        self.set_command_memory(True)

        self.bar0_addr = self.bars[0][0]
        self.bar1_addr = self.bars[1][0]
        self.bar0 = FileMap("/dev/mem", self.bar0_addr, BAR0_SIZE)
        # Map just a small part of BAR1 as we don't need it all
        self.bar1 = FileMap("/dev/mem", self.bar1_addr, 1024 * 1024)
        self.pmcBoot0 = self.read(LW_PMC_BOOT_0)

        if self.pmcBoot0 == 0xffffffff:
            debug("%s sanity check of bar0 failed", self)
            raise BrokenGpuError()

        if self.pmcBoot0 not in GPU_MAP:
            raise UnknownGpuError("GPU %s %s bar0 %s" % (self.bdf, hex(self.pmcBoot0), hex(self.bar0_addr)))

        gpu_props = GPU_MAP[self.pmcBoot0]
        self.name = gpu_props["name"]
        self.arch = gpu_props["arch"]
        self.is_pmu_reset_in_pmc = gpu_props["pmu_reset_in_pmc"]
        self.is_memory_clear_supported = gpu_props["memory_clear_supported"]
        self.is_forcing_ecc_on_after_reset_supported = gpu_props["forcing_ecc_on_after_reset_supported"]
        self.sanity_check()
        self._save_cfg_space()
        if self.parent:
            self.parent.children.append(self)

        self.init_priv_ring()

        self.bar0_window_base = 0
        self.bios = None
        self.falcons_cfg = gpu_props.get("falcons_cfg", {})
        self.falcons = []

        self.pmu = PmuFalcon(self)
        self.falcons.append(self.pmu)
        if 0 in gpu_props["lwdec"]:
            self.lwdec = LwDecFalcon(self, 0)
            self.falcons.append(self.lwdec)

        for lwdec in gpu_props["lwdec"]:
            # lwdec 0 added above
            if lwdec == 0:
                continue
            self.falcons.append(LwDecFalcon(self, lwdec))
        for lwenc in gpu_props["lwenc"]:
            self.falcons.append(LwEncFalcon(self, lwenc))
        if "msvld" in gpu_props["other_falcons"]:
            self.falcons.append(MsvldFalcon(self))
        if "msppp" in gpu_props["other_falcons"]:
            self.falcons.append(MspppFalcon(self))
        if "msenc" in gpu_props["other_falcons"]:
            self.falcons.append(MsencFalcon(self))
        if "mspdec" in gpu_props["other_falcons"]:
            self.falcons.append(MspdecFalcon(self))
        if "hda" in gpu_props["other_falcons"]:
            self.falcons.append(HdaFalcon(self))
        if "disp" in gpu_props["other_falcons"]:
            self.falcons.append(DispFalcon(self))
        if "gsp" in gpu_props["other_falcons"]:
            self.falcons.append(GspFalcon(self))
        if "sec" in gpu_props["other_falcons"]:
            self.sec = SecFalcon(self)
            self.falcons.append(self.sec)
        if "fb" in gpu_props["other_falcons"]:
            self.falcons.append(FbFalcon(self))
        if "minion" in gpu_props["other_falcons"]:
            self.falcons.append(MinionFalcon(self))

    @property
    def is_pascal_plus(self):
        return GPU_ARCHES.index(self.arch) >= GPU_ARCHES.index("pascal")

    @property
    def is_pascal_10x_plus(self):
        return GPU_ARCHES.index(self.arch) >= GPU_ARCHES.index("pascal") and self.name != "P100"

    @property
    def is_volta_plus(self):
        return GPU_ARCHES.index(self.arch) >= GPU_ARCHES.index("volta")

    @property
    def is_turing_plus(self):
        return GPU_ARCHES.index(self.arch) >= GPU_ARCHES.index("turing")

    def is_gpu(self):
        return True

    def vbios_scratch_register(self, index):
        if self.is_turing_plus:
            return 0x1400 + index * 4
        else:
            return 0x1580 + index * 4

    def poll_register(self, name, offset, value, timeout, sleep_interval=0.01, mask=0xffffffff):
        timestamp = time.time()
        while True:
            try:
                reg = self.read(offset)
            except:
                error("Failed to read falcon register %s (%s)", name, hex(offset))
                raise

            if reg & mask == value:
                debug("Register %s (%s) = %s after %f secs", name, hex(offset), hex(value), time.time() - timestamp)
                return
            if time.time() - timestamp > timeout:
                assert False, "Timed out polling register %s (%s), value %s is not the expected %s. Timeout %f secs" % (name, hex(offset), hex(reg), hex(value), timeout)
            time.sleep(sleep_interval)

    def load_vbios(self):
        if self.bios:
            return

        self._load_bios()

    def reload_vbios(self):
        self._load_bios()

    def _load_bios(self):
        self.stop_preos()

        # Prior to Turing, we can disable the ROM offset directly, but only
        # after pre-OS has stopped.
        if not self.is_turing_plus:
            self.write(0xe208, 0)

        if self.is_pascal_plus:
            prom_data_size = LW_PROM_DATA_SIZE_PASCAL
        else:
            prom_data_size = LW_PROM_DATA_SIZE_KEPLER

        bios_data = bytearray()
        for offset in range(prom_data_size):
            data = self.bar0.read8(LW_PROM_DATA + offset)
            bios_data.append(data)
        self.bios = GpuVbios(bios_data)

    def load_ucode(self, falcon, ucode):
        debug("%s loading ucode %s on %s", self, ucode.name, falcon.name)
        debug("%s", ucode.imem_ns)
        falcon.reset()

        falcon.load_imem(ucode.imem_ns, phys_base=ucode.imem_ns_phys_base, virt_base=ucode.imem_ns_virt_base)

        #I Load secure imem
        if ucode.imem_sec_size != 0:
            falcon.load_imem(ucode.imem_sec, phys_base=ucode.imem_sec_phys_base, virt_base=ucode.imem_sec_phys_base, secure=True)

        #I Load dmem
        falcon.load_dmem(ucode.dmem, ucode.dmem_phys_base)
        debug("%s loaded ucode %s on %s", self, ucode.name, falcon.name)

    def unlock_memory(self):
        if not self.is_turing_plus:
            return

        self.load_ucode(self.lwdec, DriverUcode("scrubber", T4_scrubber))

        #Ilocked_range = self.get_mmu_lock_range()
        #Idebug("Before locked global %s range [%s, %s)", locked_range[0], hex(locked_range[1]), hex(locked_range[2]))

        self.lwdec.execute()

        #Ilocked_range = self.get_mmu_lock_range()
        #Idebug("After locked global %s range [%s, %s)", locked_range[0], hex(locked_range[1]), hex(locked_range[2]))

    def update_fuses(self):
        if not self.is_turing_plus:
            return

        self.sec.reset()

        self.load_ucode(self.sec, DriverUcode("fub", T4_fub))

        debug("FUB on SEC: mailbox0 before %s", hex(self.read(0x840040)))
        debug("FUB on SEC: mailbox1 before %s", hex(self.read(0x840040)))

        self.sec.execute()

        debug("FUB on SEC: mailbox0 after %s", hex(self.read(0x840040)))
        debug("FUB on SEC: mailbox1 after %s", hex(self.read(0x840044)))


    def run_devinit(self):
        if self.is_turing_plus:
            return

        assert self.arch != "kepler"

        devinit_status = self.read(0x15b0)
        if devinit_status == 0x5:
            info("GPU devinit already finished")
            return

        self._load_bios()

        self.pmu.reset()
        #I debug("Devinit offset %s desc %s", hex(self.bios.devinit_ucode_offset), self.bios.devinit_ucode_desc)

        #I Load imem
        ucode_src = self.bios.devinit_ucode_offset
        desc = self.bios.devinit_ucode_desc
        non_selwre_size = desc.imem_load_size - desc.imem_sec_size

        imem_data = self.bios.read32_array(self.bios.devinit_ucode_offset, non_selwre_size)
        self.pmu.load_imem(imem_data, phys_base=desc.imem_phys_base, virt_base=desc.imem_virt_base)

        #I Load secure imem
        if desc.imem_sec_size != 0:
            imem_data = self.bios.read32_array(self.bios.devinit_ucode_offset + non_selwre_size, desc.imem_sec_size)
            self.pmu.load_imem(imem_data, phys_base=desc.imem_phys_base + non_selwre_size, virt_base=desc.imem_virt_base + non_selwre_size, secure=True)

        #I Load dmem
        dmem_data = self.bios.read32_array(self.bios.devinit_ucode_offset + desc.dmem_offset, desc.dmem_load_size)
        self.pmu.load_dmem(dmem_data, desc.dmem_phys_base)

        tables_data = self.bios.read32_array(self.bios.ifr_image_size + self.bios.lwinit_ptrs.devinit_tables_ptr, self.bios.lwinit_ptrs.devinit_tables_size)
        self.pmu.load_dmem(tables_data, self.bios.devinit_app_if.tables_phys_base)
        lwinit_data = self.bios.read32_array(self.bios.ifr_image_size + self.bios.lwinit_ptrs.boot_scripts_ptr, self.bios.lwinit_ptrs.boot_scripts_size)
        self.pmu.load_dmem(lwinit_data, self.bios.devinit_app_if.script_phys_base)

        timestamp = time.time()
        self.pmu.execute(desc.virtual_entry)

        devinit_status = self.read(0x15b0)
        if devinit_status != 0x5:
            error("Devinit status %s", hex(devinit_status))
            assert 0

        info("GPU devinit finished")

    def is_broken_gpu(self):
        return False

    def _save_cfg_space(self):
        self.saved_cfg_space = {}
        for offset in GPU_CFG_SPACE_OFFSETS:
            if offset >= self.config.size:
                continue
            self.saved_cfg_space[offset] = self.config.read32(offset)
            #debug("%s saving cfg space %s = %s", self, hex(offset), hex(self.saved_cfg_space[offset]))

    def _restore_cfg_space(self):
        assert self.saved_cfg_space
        for offset in GPU_CFG_SPACE_OFFSETS:
            old = self.config.read32(offset)
            new = self.saved_cfg_space[offset]
            #debug("%s restoring cfg space %s = %s to %s", self, hex(offset), hex(old), hex(new))
            self.config.write32(offset, new)

    def sanity_check(self):
        if not self.sanity_check_cfg_space():
            debug("%s sanity check of config space failed", self)
            return False

        boot = self.read(LW_PMC_BOOT_0)
        if boot == 0xffffffff:
            debug("%s sanity check of mmio failed", self)
            return False

        return True

    def reset_with_link_disable(self):
        self.parent.toggle_link()
        if not self.sanity_check_cfg_space():
            return False

        self._restore_cfg_space()

        # Enable MMIO
        self.set_command_memory(True)

        return self.sanity_check()

    def reset_with_sbr(self):
        assert self.parent.is_bridge()
        self.parent.toggle_sbr()

        self._restore_cfg_space()
        self.set_command_memory(True)
        return self.sanity_check()

    def get_memory_size(self):
        config = self.read(0x100ce0)
        scale = config & 0xf
        mag = (config >> 4) & 0x3f
        size = mag << (scale + 20)
        ecc_tax = (config >> 30) & 1
        if ecc_tax:
            size = size * 15 // 16
        return size

    def get_ecc_state(self):
        config = self.read(0x100ce0)
        ecc_on = (config >> 30) & 1
        return ecc_on

    def query_final_ecc_state(self):
        # To get the final ECC state, we need to wait for the memory to be
        # fully initialized. clear_memory() guarantees that.
        self.clear_memory()

        return self.get_ecc_state()

    def verify_memory_is_zero(self, stride=64 * 1024):
        self.unlock_memory()

        fb_size = self.get_memory_size()

        start = time.time()
        last_debug = time.time()
        for offset in range(0, fb_size, stride):
            data = self.bar0_window_read(offset, 4)
            if data != 0:
                error("Bad data at offset %s = %s", hex(offset), hex(data))
                assert 0
            if time.time() - last_debug > 1:
                last_debug = time.time()
                mbs = offset / (1024 * 1024.)
                t = time.time() - start
                time_left = (fb_size - offset) / (mbs / t) / (1024 * 1024.)
                debug("So far verified %.1f MB, %.1f MB/s, time %.1f s, stride %d, time left ~%.1f s", mbs, mbs/t, t, stride, time_left)

        mbs = fb_size / (1024 * 1024.)
        t = time.time() - start
        info("Verified memory to be zero, %.1f MB/s, total %.1f MB, total time %.1f s, stride %d", mbs/t, mbs, t, stride)

    def clear_memory_test(self, stride=64 * 1024):
        # Make sure the memory is cleared and accessible
        self.clear_memory()
        if self.is_turing_plus:
            self.unlock_memory()

        fb_size = self.get_memory_size()

        start = time.time()
        last_debug = time.time()

        # Write non-zero to all of the memory (at stride)
        for offset in range(0, fb_size, stride):
            self.bar0_window_write32(1024 * 1024, 0xcafe1234)
            if time.time() - last_debug > 1:
                last_debug = time.time()
                mbs = offset / (1024 * 1024.)
                t = time.time() - start
                time_left = (fb_size - offset) / (mbs / t) / (1024 * 1024.)
                debug("So far written %.1f MB, %.1f MB/s, time %.1f s, stride %d, time left ~%.1f s", mbs, mbs/t, t, stride, time_left)

        # Reset the GPU and perform the memory clearing steps
        self.reset_with_sbr()
        self.clear_memory()

        # Verify that memory was cleared
        self.verify_memory_is_zero(stride=stride)

    def clear_memory(self):
        assert self.is_memory_clear_supported

        if not self.is_turing_plus:
            # Before Turing, devinit has to be kicked off manually. devinit
            # starts the memory clearing process
            self.run_devinit()
        self.wait_for_memory_clear()

    def wait_for_memory_clear(self):
        assert self.is_memory_clear_supported

        if self.is_turing_plus:
            # Turing does multiple clears asynchronously and we need to wait
            # for the last one to start first
            self.poll_register("memory_clear_started", 0x118234, 0x3ff, 5)

        self.poll_register("memory_clear_finished", 0x100b20, 0x1, 5)

    def force_ecc_on_after_reset(self):
        assert self.is_forcing_ecc_on_after_reset_supported

        scratch = self.read(0x118f78)
        self.write_verbose(0x118f78, scratch | 0x01000000)
        info("%s forced ECC to be enabled after next reset", self)

    def config_bar0_window(self, addr, sysmem=False):
        new_window = addr >> 16
        new_window &= ~0xf
        if sysmem:
            new_window |= LW_BAR0_WINDOW_CFG_TARGET_SYSMEM_COHERENT
        if self.bar0_window_base != new_window:
            #Idebug("%s bar0 window moving to %s" % (str(self), hex(new_window)))
            self.bar0_window_base = new_window
            self.write(LW_BAR0_WINDOW_CFG, new_window)
        bar0_window_addr = LW_BAR0_WINDOW + (addr & 0xfffff)
        #Idebug("%s bar0 window for addr %s returning %s" % (str(self), hex(addr), hex(bar0_window_addr)))
        return bar0_window_addr

    def bar0_window_read(self, address, size):
        offset = self.config_bar0_window(address)
        return self.bar0.read(offset, size)

    def bar0_window_write32(self, address, data):
        offset = self.config_bar0_window(address)
        return self.bar0.write32(offset, data)

    def write(self, reg, data):
        self.bar0.write32(reg, data)

    def write_verbose(self, reg, data):
        old = self.read(reg)
        debug("%s writing %s = %s (old %s diff %s)", self, hex(reg), hex(data), hex(old), hex(data ^ old))
        self.bar0.write32(reg, data)

    def _is_read_good(self, reg, data):
        return data >> 16 != 0xbadf

    def read_bad_ok(self, reg):
        data = self.bar0.read32(reg)
        return data

    def check_read(self, reg):
        data = self.bar0.read32(reg)
        return self._is_read_good(reg, data)

    def read(self, reg):
        data = self.bar0.read32(reg)
        assert self._is_read_good(reg, data), "gpu %s reg %s = %s, bad?" % (self, hex(reg), hex(data))
        return data

    def read_bar1(self, offset):
        return self.bar1.read32(offset)

    def stop_preos(self):
        if self.arch == "kepler":
            return

        if self.is_turing_plus:
            self.poll_register("last_preos_started", 0x118234, 0x3ff, 5)

        if self.is_turing_plus:
            # On Turing+, pre-OS ucode disables the offset applied to PROM_DATA
            # when requested through a scratch register. Do it always for simplicity.
            #I http://lwbugs/2642913/19
            reg = self.vbios_scratch_register(1)
            reg_value = self.read(reg)
            reg_value |= (0x1 << 11)
            self.write(reg, reg_value)

        preos_started_reg = self.vbios_scratch_register(6)
        start = time.time()
        preos_status = (self.read(preos_started_reg) >> 12) & 0xf
        while preos_status == 0:
            if time.time() - start > 5:
                error("Timed out waiting for preos to start %s", hex(preos_status))
                assert 0
            preos_status = (self.read(preos_started_reg) >> 12) & 0xf

        preos_stop_reg = None
        if self.is_volta_plus:
            preos_stop_reg = self.vbios_scratch_register(1)

        # Stop preos if supported
        if preos_stop_reg:
            data = self.read(preos_stop_reg)
            self.write(preos_stop_reg, data | 0x200)
            if not self.pmu.is_disabled():
                self.pmu.wait_for_halt()

    # Init priv ring (internal bus)
    def init_priv_ring(self):
        self.write(0x12004c, 0x4)
        self.write(0x122204, 0x2)

    def disable_pgraph(self):
        pmc_enable = self.read(LW_PMC_ENABLE)
        self.write(LW_PMC_ENABLE, pmc_enable & ~LW_PMC_ENABLE_PGRAPH)

    def is_pgraph_disabled(self):
        pmc_enable = self.read(LW_PMC_ENABLE)
        return (pmc_enable & LW_PMC_ENABLE_PGRAPH) == 0

    def print_falcons(self):
        self.init_priv_ring()
        for falcon in self.falcons:
            falcon.enable()
        print("\"" + self.name + "\": {")
        for falcon in self.falcons:
            name = falcon.name
            print("    \"" + name + "\": {")
            print("        \"imem_size\": " + str(falcon.max_imem_size) + ",")
            print("        \"dmem_size\": " + str(falcon.max_dmem_size) + ",")
            if falcon.has_emem:
                print("        \"emem_size\": " + str(falcon.max_emem_size) + ",")
            print("        \"imem_port_count\": " + str(falcon.imem_port_count) + ",")
            print("        \"dmem_port_count\": " + str(falcon.dmem_port_count) + ",")
            print("    },")
        print("},")

    def __str__(self):
        return "GPU %s %s BAR0 0x%x" % (self.bdf, self.name, self.bar0_addr)

    def __eq__(self, other):
        return self.bar0_addr == other.bar0_addr

class MalwareDetector(object):
    def __init__(self, gpu):
        self.gpu = gpu

        # Dictionary of the expected mem contents, indexed by falcon name.
        self.expected_mem = {}

        # Set of falcons that is disabled
        self.disabled_falcons = set()

        self.op_counts = {}
        for falcon in self.gpu.falcons:
            self.expected_mem[falcon.name] = {}
            self.op_counts[falcon] = {
                    "verified_state": 0,
                }
            for m in falcon.mem_spaces:
                self.expected_mem[falcon.name][m] = []
                self.op_counts[falcon][m] = {
                    "verified_words": 0,
                    "randomized_words": 0,
                }

        self.op_counts_global = {
                "verified_pgraph": 0,
            }

    def verify_pgraph_disabled(self):
        assert self.gpu.is_pgraph_disabled(), "%s PGRAPH enabled" % self.gpu
        self.op_counts_global["verified_pgraph"] += 1
        #debug("Verified PGRAPH to be disabled")

    def verify_falcon_state(self, falcon):
        self.op_counts[falcon]["verified_state"] += 1

        if falcon.no_outside_reset:
            # For falcons with no outside reset we can only verify that the
            # falcon is halted. We don't run any code on the falcons (yet) so
            # the falcon is expected to be halted in both cases.
            assert falcon.is_halted(), "Falcon %s expected to be halted but is not" % falcon.name
            #debug("Falcon %s verified to be halted" % falcon.name)
            return

        if falcon.name in self.disabled_falcons:
            assert falcon.is_disabled(), "Falcon %s expected to be disabled but is enabled" % falcon.name
            cpuctl = self.gpu.read_bad_ok(falcon.cpuctl)
            assert cpuctl >> 16 == 0xbadf, "Falcon %s accessible when it should not be %s" % (falcon.name, hex(cpuctl))
            #debug("Falcon %s verified to be disabled %s", falcon.name, hex(cpuctl))
        else:
            # We don't run any code on the falcons (yet) so the falcon is
            # expected to be halted whenever it's enabled.
            assert falcon.is_halted(), "Falcon %s expected to be halted but is not" % falcon.name
            assert not falcon.is_disabled(), "Falcon %s expected to be enabled but is disabled" % falcon.name
            #debug("Falcon %s verified to be enabled" % falcon.name)

    def random_disable_falcons(self, each_falcon_chance=1.0):
        for falcon in self.gpu.falcons:
            if random.uniform(0, 1) > each_falcon_chance:
                continue
            self.disable_falcon(falcon)

    # Return a list of all falcons in random order
    def shuffled_falcons(self):
        shuffled_falcons = list(self.gpu.falcons)
        random.shuffle(shuffled_falcons)
        return shuffled_falcons

    def random_falcon(self):
        return random.choice(self.gpu.falcons)

    # Return a list of all falcon, memory_space (imem or dmem) pairs in random order
    def shuffled_falcon_mem_pairs(self):
        shuffled_falcon_mem_pairs = [(f, m) for f in self.gpu.falcons for m in f.mem_spaces]

        random.shuffle(shuffled_falcon_mem_pairs)
        return shuffled_falcon_mem_pairs

    # Verify state of all falcons is expected (i.e. disabled or enabled). Each
    # falcon is verified with the each_falcon_chance.
    def verify_falcons_state(self, each_falcon_chance=1.0):
        for falcon in self.shuffled_falcons():
            if random.uniform(0, 1) <= each_falcon_chance:
                self.verify_falcon_state(falcon)

    # Verify state of a random falcon
    def verify_random_falcon_state(self):
        self.verify_falcon_state(self.random_falcon())

    def verify_random_falcon_mem_word(self):
        falcon = self.random_falcon()
        self.verify_falcon_random_mem_word(falcon)

    def randomize_falcon_mem_word(self):
        falcon = self.random_falcon()
        self.falcon_randomize_mem_word(falcon)

    # Disable a particular falcon if not already disabled
    def disable_falcon(self, falcon):
        if falcon.name in self.disabled_falcons:
            return

        falcon.disable()
        self.disabled_falcons.add(falcon.name)

        # Disabling a falcon purges its mem
        for mem in falcon.mem_spaces:
            self.expected_mem[falcon.name][mem] = []
        debug("Falcon %s disabled", falcon.name)

    # Enable a particular falcon if not already enabled
    def enable_falcon(self, falcon):
        if falcon.name not in self.disabled_falcons:
            return
        falcon.enable()
        self.disabled_falcons.remove(falcon.name)

    def randomize_falcon_mem(self, falcon, mem_space):
        self.enable_falcon(falcon)

        name = falcon.name
        port = falcon.get_mem_port(mem_space)

        max_mem_size = port.max_size
        mem = []
        for i in range(max_mem_size // 4):
            mem.append(random.randint(0, 0xffffffff))

        t = time.time()
        port.configure(0, inc_write=True)
        port.write(mem)
        t = time.time() - t
        kb = len(mem) * 4/1024.

        self.expected_mem[name][mem_space] = mem

        debug("Randomized falcon %s %.1f KB mem at %.1f KB/s took %.1f ms, port %s", name, kb, kb/t, t * 1000, port)

    def verify_falcon_mem(self, falcon, mem_space):
        if falcon.name in self.disabled_falcons:
            return

        name = falcon.name
        mem = self.expected_mem[name][mem_space]
        port = falcon.get_mem_port(mem_space)

        if len(mem) == 0:
            return

        t = time.time()
        port.configure(0)
        falcon_mem = port.read(len(mem) * 4)
        t = time.time() - t
        kb = len(mem) * 4/1024.

        assert falcon_mem == mem, "Falcon %s mismatched imem" % name
        debug("Verified falcon %s imem %.1f KB at %.1f KB/s took %.1f ms port %s", name, kb, kb/t, t * 1000, port)

    def verify_falcon_random_mem_word(self, falcon):
        if falcon.name in self.disabled_falcons:
            return

        name = falcon.name
        mem_space = random.choice(falcon.mem_spaces)
        mem = self.expected_mem[name][mem_space]
        ports = falcon.get_mem_ports(mem_space)

        if len(mem) == 0:
            return

        port = random.choice(ports)

        autoincr = port.auto_inc_read
        offset = port.offset // 4

        if offset == len(mem):
            offset = 0

        # 0.001 chance to flip auto inc on reads
        if random.uniform(0, 1) < 0.001:
            autoincr = not autoincr

        # 0.001 chance to pick a new offset
        if random.uniform(0, 1) < 0.001:
            offset = random.randrange(0, len(mem))

        # If needed, reconfigure offset and auto inc on read. Preserve auto inc on write.
        port.configure(offset * 4, inc_read=autoincr, inc_write=port.auto_inc_write)

        falcon_mem = port.read(4)

        assert falcon_mem[0] == mem[offset], "Falcon %s mismatched mem at %d (bytes %d 0x%x) %s != %s, mem port %s" % (name, offset, offset * 4, offset * 4, hex(falcon_mem[0]), hex(mem[offset]), port)

        self.op_counts[falcon][mem_space]["verified_words"] += 1

    def falcon_randomize_mem_word(self, falcon):
        if falcon.name in self.disabled_falcons:
            return

        name = falcon.name
        mem_space = random.choice(falcon.mem_spaces)
        mem = self.expected_mem[name][mem_space]
        ports = falcon.get_mem_ports(mem_space)

        if len(mem) == 0:
            return

        port = random.choice(ports)

        autoincw = port.auto_inc_write
        offset = port.offset // 4

        if offset == len(mem):
            offset = 0

        # 0.001 chance to flip auto inc on writes
        if random.uniform(0, 1) < 0.001:
            autoincw = not autoincw

        # 0.001 chance to pick a new offset
        if random.uniform(0, 1) < 0.001:
            offset = random.randrange(0, len(mem))

        # If needed, reconfigure offset and auto inc on read. Preserve auto inc on write.
        port.configure(offset * 4, inc_read=port.auto_inc_read, inc_write=autoincw)

        mem_word = random.randint(0, 0xffffffff)
        port.write([mem_word])

        mem[offset] = mem_word

        self.op_counts[falcon][mem_space]["randomized_words"] += 1

    # Each falcon, memory pair is randomized with the chance of
    # each_falcon_memory_chance param.
    def randomize_falcons_memory(self, each_falcon_memory_chance=1.0):
        for falcon_mem in self.shuffled_falcon_mem_pairs():
            if random.uniform(0, 1) > each_falcon_memory_chance:
                continue
            falcon, mem = falcon_mem
            self.randomize_falcon_mem(falcon, mem)

    # Each falcon, memory pair is verified with the chance of
    # each_falcon_memory_chance param.
    def verify_falcons_memory(self, each_falcon_memory_chance=1.0):
        for falcon_mem in self.shuffled_falcon_mem_pairs():
            if random.uniform(0, 1) > each_falcon_memory_chance:
                continue
            falcon, mem = falcon_mem
            self.verify_falcon_mem(falcon, mem)

    def _initial_cleanup(self):
        # Disable all falcons first
        debug("Initial cleanup start")

        if self.gpu.is_memory_clear_supported:
            # Leverage GPU init performed by clearing memory to speed up some of
            # the operations, if it's implemented.
            self.gpu.clear_memory()

        self.gpu.disable_pgraph()

        self.gpu.init_priv_ring()

        # Reset the PMU explicitly to get rid of the expected preos ucode
        # running on it
        self.gpu.pmu.reset()

        for falcon in self.gpu.falcons:
            self.disable_falcon(falcon)
        self.verify_falcons_state()

        self.randomize_falcons_memory()
        self.verify_falcons_memory()

        self.verify_falcons_state()

        self.verify_pgraph_disabled()

        debug("Initial cleanup done")

    def _final_cleanup(self):
        debug("Final cleanup start")

        self.verify_falcons_memory()

        # Disable all falcons as the last step
        for falcon in self.gpu.falcons:
            self.disable_falcon(falcon)
        self.verify_falcons_state()

        self.verify_pgraph_disabled()

        debug("Final cleanup done")

    def status_print(self, iteration, time_start, iter_fn=info, detail_fn=debug):
        time_so_far = time.time() - time_start
        iter_fn("Iter %d %d iters/s", iteration, iteration / time_so_far)
        for falcon in self.gpu.falcons:
            counts = self.op_counts[falcon]
            detail_fn("Falcon %s verified state %d random mem word %s",
                      falcon.name,
                      counts["verified_state"],
                      ", ".join("%s verif %d update %d" % (m, counts[m]["verified_words"], counts[m]["randomized_words"]) for m in falcon.mem_spaces))
        detail_fn("pgraph verifications %d", self.op_counts_global["verified_pgraph"])


    def detect(self, iters=500000):
        debug("Malware detection start")
        total_time_start = time.time()

        self._initial_cleanup()
        operations = [
            "verify_falcon_state",
            "verify_falcon_mem_word",
            "randomize_falcon_mem_word",
            "verify_pgraph_disabled",
        ]

        debug("Malware detection loop start")
        t = time.time()
        for i in range(iters):
            op = random.choice(operations)
            if op == "verify_falcon_state":
                self.verify_random_falcon_state()
            elif op == "verify_falcon_mem_word":
                self.verify_random_falcon_mem_word()
            elif op == "randomize_falcon_mem_word":
                self.randomize_falcon_mem_word()
            elif op == "verify_pgraph_disabled":
                self.verify_pgraph_disabled()
            else:
                assert 0, "Unhandled op %s" % op

            if i > 0 and i % 100000 == 0:
                self.status_print(i, t)

        self.status_print(i, t, detail_fn=info)
        debug("Malware detection loop done")
        self._final_cleanup()

        total_time = time.time() - total_time_start
        info("No malware detected, took %.1f s", total_time)

def trigger_mmio_errors(gpu):
    if gpu.is_turing_plus:
        return trigger_mmio_errors_turing_plus(gpu)
    else:
        return trigger_mmio_errors_legacy(gpu)

def trigger_mmio_errors_legacy(gpu):
    pmc_enable = gpu.read(LW_PMC_ENABLE)

    # Disable some GPU timeouts
    gpu.write_verbose(LW_PMC_ENABLE, pmc_enable | LW_PMC_ENABLE_HOST)
    gpu.write_verbose(0x2a04, 0)
    gpu.write_verbose(0x1538, 0)
    gpu.write_verbose(0x171c, 1)

    # Do BAR1 reads and flushes until sanity check fails
    i = 0
    while gpu.sanity_check():
        gpu.read_bar1(0x0)
        gpu.write(0x70000, 0x0)
        i += 1
        if i >= 1000:
            error("GPU still responding after %d iters, giving up", i)
            return False

    info("GPU stopped responding to MMIO after %d iters", i)
    return True

def trigger_mmio_errors_turing_plus(gpu):
    gpu.write_verbose(0x88004, 0x100002)

    i = 0
    while gpu.sanity_check():
        gpu.write(0x1704, 0x20000000)
        gpu.read(0x700000)
        i += 1
        if i >= 1000:
            error("GPU still responding after %d iters, giving up", i)
            return False

    info("GPU stopped responding to MMIO after %d iters", i)
    return True

def trigger_cfg_space_errors(gpu):
    i = 0
    while gpu.sanity_check_cfg_space():
        gpu.read_bar1(0x0)
        i += 1
        if i >= 1000:
            error("GPU still responding after %d iters, giving up", i)
            return False

    info("GPU stopped responding to cfg space after additional %d iters", i)
    return True

def check_surpdn(gpu):
    if gpu.parent.is_hidden():
        return
    if not gpu.parent.has_aer():
        return
    if gpu.parent.uncorr_status["SURPDN"] == 1:
        if opts.clear_errors:
            info("%s clearing SURPDN", gpu.parent)
            gpu.parent.uncorr_status.write_only("SURPDN", 1)
        else:
            info("%s SURPDN pending %s", gpu.parent, gpu.parent.uncorr_status)

def check_cpl_timeout(gpu):
    root_port = gpu.get_root_port()
    if not root_port.has_aer():
        return

    if root_port.uncorr_status["COMP_TIME"] == 1:
        info("%s completion timeout: %s", root_port, root_port.uncorr_status)
        if opts.clear_errors:
            root_port.uncorr_status.write_only("COMP_TIME", 1)
            info("%s clearing completion timeout %s", root_port, root_port.uncorr_status)

def check_plx_timeout(gpu):
    if not gpu.parent.is_plx():
        return
    plx = gpu.parent
    if plx.plx_check_timeout():
        info("%s PLX timeout pending %s", gpu.parent, gpu.parent.plx_egress_status)
        if opts.clear_errors:
            info("%s clearing PLX timeout as requested", plx)
            plx.plx_clear_timeout()
        return True
    return False

def trigger_plx_timeout(gpu):
    plx = gpu.parent
    assert plx.is_plx()
    assert plx.plx_is_timeout_enabled()
    # Check whether the surprise down error reporting is supported
    plx_surpdn = plx_surpdn = plx.link_cap['SDERC'] == 1

    check_surpdn(gpu)

    if not plx_surpdn:
        info("PLX doesn't support surprise down error reporting")

    i = 0
    while not plx.plx_check_timeout():
        gpu.read_bar1(0x0)
        i += 1
        if i >= 1000:
            error("PLX timeout not triggered after %d iters, giving up", i)
            return

    info("PLX timeout after %d iters %s", i, plx.plx_egress_status)



def trigger_more_errors(gpu):
    import time
    i = 0
    while not gpu.sanity_check_cfg_space():
        time.sleep(0.1)
        gpu.read_bar1(0x0)
        i += 1
        if i >= 1000:
            error("GPU still broken after %d iters, giving up", i)
            return False

    info("GPU recovered after %d * 0.1s iters", i)
    return True

def trigger_gpu_errors(gpu):
    cpl_timeout_masked = False
    plx_timeout_enabled = False
    root_port = gpu.get_root_port()
    if gpu.parent.is_plx():
        plx = gpu.parent
        plx_timeout_enabled = plx.plx_is_timeout_enabled()

    if root_port.is_intel():
        cpl_timeout_masked = root_port.intel_uncedmask["COMP_TIME"] == 1
        if cpl_timeout_masked:
            info("%s completion timeout masked, not expecting it in AER status, full mask %s", root_port, root_port.intel_uncedmask)

    if root_port.has_aer():
        if opts.clear_errors and root_port.uncorr_status.value.raw != 0:
            warn("%s clearing current error status %s", root_port, root_port.uncorr_status)
            root_port.uncorr_status._write()
        info("%s %s", root_port, root_port.uncorr_status)

    mmio_error = trigger_mmio_errors(gpu)
    if not mmio_error:
        return

    if opts.trigger_errors in ["cfgspace", "plx-timeout", "dont-stop"]:
        trigger_cfg_space_errors(gpu)
    if opts.trigger_errors in ["plx-timeout", "dont-stop"]:
        if plx_timeout_enabled:
            trigger_plx_timeout(gpu)
    if opts.trigger_errors in ["dont-stop"]:
        trigger_more_errors(gpu)


    check_cpl_timeout(gpu)
    hit_plx_timeout = check_plx_timeout(gpu)
    check_surpdn(gpu)

    root_port = gpu.get_root_port()
    if not opts.recover_broken_gpu:
        warn("Leaving %s broken", gpu)
        return

    if gpu.parent.is_hidden():
        error("Cannot recover the GPU as the upstream port is hidden, leaving it broken")
        return

    if hit_plx_timeout:
        # Triggering the PLX timeout seems to leave the link in a bad state.
        # Toggle link disable to put it back in a good state.
        # TODO: Understand this better, possibly ask PLX/Broadcom what's expected exactly.
        plx.toggle_link()

    # Recover the GPU with an SBR
    gpu.reset_with_sbr()

    if gpu.sanity_check():
        info("%s recovered after SBR", gpu)
    else:
        error("%s not recovered after an SBR", gpu)

def print_topo_indent(root, indent):
    if root.is_hidden():
        indent = indent - 1
    else:
        print(" " * indent, root)
    for c in root.children:
        print_topo_indent(c, indent + 1)

class GpuVbiosMismatchError(Exception):
    pass

def compare_vbios(gpu_vbios, golden_vbios):
    if gpu_vbios.version != golden_vbios.version:
        raise GpuVbiosMismatchError("BIOS version mismatch %s != %s, failing early" % (gpu_vbios.version, golden_vbios.version))
    if len(gpu_vbios.data) != len(golden_vbios.data):
        raise GpuVbiosMismatchError("BIOS length mismatch %d != %d" % (len(gpu_vbios.data), len(golden_vbios.data)))

    diffs = {}
    for s in golden_vbios.sections:
        diffs[s] = 0
    diffs["unknown"] = 0

    # Sections of the VBIOS with expected differences
    expected_diffs = set(["inforom", "inforom_backup", "erase_ledger"])

    for index, golden_byte in enumerate(golden_vbios.data):
        if golden_byte != gpu_vbios.data[index]:
            section = golden_vbios.find_section_with_offset(index)
            debug("Mismatch in section %s at offset %d (0x%x) byte 0x%s != 0x%s", section, index, index, golden_byte, gpu_vbios.data[index])
            if section is None:
                diffs["unknown"] += 1
            else:
                count = diffs.get(section.name, 0)
                diffs[section.name] = count + 1

    for s in diffs:
        if diffs[s] != 0 and s not in expected_diffs:
            raise GpuVbiosMismatchError("Mismatches outside of expected sections %s" % diffs)

    info("GPU VBIOS %s, GOLDEN %s, no unexpected mismatches %s", gpu_vbios.version, golden_vbios.version, diffs)

def print_topo():
    print("Topo:")
    for c in DEVICES:
        dev = DEVICES[c]
        if dev.is_root():
            print_topo_indent(dev, 1)

def sysfs_pci_rescan():
    with open("/sys/bus/pci/rescan", "w") as rf:
        rf.write("1")

def parse_simple_c_arrays(lines):
    import re

    arrays = {}
    lwrrent_array = None

    in_array = False
    array_prog_start = re.compile(".*LwU32 (?P<name>[a-z0-9_]+)\[\] = {$")

    for l in lines:
        if lwrrent_array:
            if l.strip() == "};":
                lwrrent_array = None
                continue
            # Strip any comments
            l = l.split("//")[0]
            arrays[lwrrent_array] += [int(d, 0) for d in l.split(",") if d.strip() != ""]
        else:
            match = array_prog_start.match(l)
            if match:
                in_array = True
                name = match.group("name")
                arrays[name] = []
                lwrrent_array = name

    return arrays

def parse_ucode_headers(data_lines, sig_lines):
    import re
    ucode = {}

    data_arrays = parse_simple_c_arrays(data_lines)
    for name in data_arrays:
        if name.find("ucode_data") != -1:
            ucode["data"] = data_arrays[name]
        elif name.find("ucode_header") != -1:
            ucode["header"] = data_arrays[name]

    sig_arrays = parse_simple_c_arrays(sig_lines)
    for name in sig_arrays:
        if name.find("patch_location") != -1:
            ucode["signature_location"] = sig_arrays[name]
        elif name.find("patch_signature") != -1:
            ucode["signature_index"] = sig_arrays[name]
        elif name.find("sig_dbg") != -1:
            ucode["signature_dbg"] = sig_arrays[name]
        elif name.find("sig_prod") != -1:
            ucode["signature_prod"] = sig_arrays[name]

    return ucode

def main():
    argp = optparse.OptionParser(usage="usage: %prog [options]")
    argp.add_option("--gpu", type="int", default=-1)
    argp.add_option("--log", type="choice", choices=['debug', 'info', 'warning', 'error', 'critical'], default='info')

    argp.add_option("--mask-cpl-timeout-in-aer", choices=['0', '1'],
                      help="Mask (1) or unmask (0) completion timeouts in AER. If not specified, leave the mask as-is.")

    argp.add_option("--intel-mask-cpl-timeout-in-uncedmask", choices=['0', '1'],
                      help="Mask (1) or unmask (0) completion timeouts in in Intel's uncedmask. See INTEL_ROOT_PORT_UNCEDMASK.  If not specified, leave the mask as-is.")
    argp.add_option("--plx-timeout", type="choice", choices=['0', '1', '2', '4', '8', '16', '32', '512', '1024'],
                      help="Configure the specified PLX timeout (in ms, 0 means disabled) on the PLX downstream port directly upstream of the selected GPU. Leave as-is, if not specified. Note that timeout settings of 2, 4, 8, 16 and 32 ms are only supported on newer PLX switches.")

    argp.add_option("--recover-broken-gpu", action='store_true', default=False,
                      help="""Attempt recovering a broken GPU (unresposnive config space or MMIO) by performing an SBR. If the GPU is
broken from the beginning and hence correct config space wasn't saved then
reenumarate it in the OS by sysfs remove/rescan to restore BARs etc.""")
    argp.add_option("--monitor-gpu", action='store_true', default=False,
                      help="""Monitor GPU and recover it with SBR when noticed to be broken""")
    argp.add_option("--reset-with-sbr", action='store_true', default=False,
                      help="Reset the GPU with SBR and restore its config space settings, before any other actions")
    argp.add_option("--query-ecc-state", action='store_true', default=False,
                      help="Query the ECC state of the GPU")
    argp.add_option("--random-seed", type='int',
                      help="Set the random seed")
    argp.add_option("--save-vbios", help="Save the entire VBIOS image")
    argp.add_option("--compare-vbios", help="Compare the entire VBIOS image")
    argp.add_option("--detect-malware", action='store_true', default=False,
                      help="""Attempt to disable any malware that might be running on the GPU and fail if anything unexpected is observed.
This option performs some operations in random order controlled by a random seed.
The chosen random seed is printed during exelwtion and can be set explicitly via the --random-seed option.
Can be combined with --compare-vbios option to compare the VBIOS image to a golden one after the malware detection/cleansing step.
The script will exit with 0 on success and 1 on any error.""")
    argp.add_option("--update-fuses", action='store_true', default=False,
                      help="""Update GPU fuses, if new values are available""")
    argp.add_option("--fub-ucode-headers", nargs=2)
    argp.add_option("--fub-ucode-type", type="choice", choices=["prod", "dbg"], default="dbg")
    argp.add_option("--clear-memory", action='store_true', default=False,
                      help="Clear the contents of the GPU memory. Supported on Pascal+ GPUs. Assumes the GPU has been reset with SBR prior to this operation and can be comined with --reset-with-sbr if not.")
    argp.add_option("--verify-memory-is-zero", action='store_true', default=False,
                      help="Verify that memory is zero after being cleared, performed at stride specified with --verify-memory-access-stride")
    argp.add_option("--verify-memory-access-stride", type='int', default=64 * 1024, help="The access stride used for verifying memory and the memory clear test")
    argp.add_option("--clear-memory-test", action='store_true', default=False,
                      help="Test memory clearing by writing non-0 values to the memory, resetting the GPU, clearing its memory and then verifying that memory is zero. "
                           "Accesses are performed at at stride specified with --verify-memory-access-stride")
    argp.add_option("--force-ecc-on-after-reset", action='store_true', default=False,
                    help="Force ECC to be enabled after a subsequent GPU reset")
    argp.add_option("--trigger-errors", type="choice", choices=["mmio", "cfgspace", "plx-timeout", "dont-stop"],
            help="""Misprogram the GPU. Set to "mmio" to cause errors until MMIO stops respodning, "cfgspace" to cause errors until cfg space stops responding, "plx-timeout" to cause errors until PLX timeout or "dont-stop" to continue for a while until GPU recovers (e.g. by systems software performing reovery)""")
    argp.add_option("--clear-errors", action='store_true', default=False, help="Clear various errors when encountered")

    global opts
    (opts, args) = argp.parse_args()
    if len(args) != 0:
        print("ERROR: Exactly zero positional argument expected.")
        argp.print_usage()
        return 1

    logging.basicConfig(level=getattr(logging, opts.log.upper()),
                        format='%(asctime)s.%(msecs)03d %(levelname)-8s %(message)s',
                        datefmt='%Y-%m-%d,%H:%M:%S')

    gpus, other = find_gpus()
    print("GPUs:")
    for i, g in enumerate(gpus):
        print(" ", i, g)
    print("Other:")
    for i, o in enumerate(other):
        print(" ", i, o)

    print_topo()

    if opts.gpu == -1:
        info("No GPU specified, select GPU with --gpu")
        return 0

    assert opts.gpu < len(gpus)

    gpu = gpus[opts.gpu]
    info("Selected %s", gpu)

    if opts.random_seed:
        seed = opts.random_seed
    else:
        seed = random.randrange(sys.maxsize)

    random.seed(seed)
    info("Random seed %s", hex(seed))

    if gpu.parent.is_plx():
        check_plx_timeout(gpu)
        if opts.plx_timeout:
            gpu.parent.plx_set_timeout(int(opts.plx_timeout))
        else:
            debug("%s PLX timeout %s %s", gpu.parent, "enabled" if gpu.parent.plx_is_timeout_enabled() else "disabled", gpu.parent.plx_egress_control)

    check_cpl_timeout(gpu)
    check_surpdn(gpu)

    root_port = gpu.get_root_port()
    if root_port.has_aer():
        if opts.mask_cpl_timeout_in_aer:
            root_port.uncorr_mask["COMP_TIME"] = int(opts.mask_cpl_timeout_in_aer)
            debug("%s %s completion timeouts in AER %s",
                    root_port,
                    "masked" if opts.mask_cpl_timeout_in_aer == "1" else "unmasked",
                    root_port.uncorr_mask)
        else:
            debug("%s %s", root_port, root_port.uncorr_mask)


    if root_port.is_intel():
        if opts.intel_mask_cpl_timeout_in_uncedmask:
            root_port.intel_uncedmask["COMP_TIME"] = int(opts.intel_mask_cpl_timeout_in_uncedmask)
            debug("%s %s completion timeouts in UNCEDMASK %s",
                    root_port,
                    "masked" if opts.intel_mask_cpl_timeout_in_uncedmask == "1" else "unmasked",
                    root_port.intel_uncedmask)
        else:
            debug("%s %s", root_port, root_port.intel_uncedmask)

    if gpu.is_broken_gpu():
        # If the GPU is broken, try to recover it if requested,
        # otherwise just exit immediately
        if opts.recover_broken_gpu:
            if gpu.parent.is_hidden():
                error("Cannot recover the GPU as the upstream port is hidden")
                return

            # Reset the GPU with SBR and if successful,
            # remove and rescan it to recover BARs
            if gpu.reset_with_sbr():
                gpu.sysfs_remove()
                sysfs_pci_rescan()
                gpu = gpu.reinit()
                if gpu.is_broken_gpu():
                    error("Failed to recover %s", gpu)
                else:
                    info("Recovered %s", gpu)
            else:
                error("Failed to recover %s %s", gpu, gpu.parent.link_status)
        return

    if opts.monitor_gpu:
        i = 0
        while True:
            i = i + 1
            if not gpu.sanity_check():
                info("Attempting to recover the GPU with SBR")
                gpu.reset_with_sbr()
                gpu.reset_with_sbr()
                if gpu.sanity_check():
                    info("Recovered the GPU with SBR")
                else:
                    error("Failed to recover the GPU")
                    return
            elif i % 1 == 0:
                info("GPU still ok")
            time.sleep(3)

    # Reset the GPU with SBR, if requested
    if opts.reset_with_sbr:
        if not gpu.parent.is_bridge():
            error("Cannot reset the GPU with SBR as the upstream bridge is not accessible")
        else:
            gpu.reset_with_sbr()

    if opts.query_ecc_state:
        ecc_state = gpu.query_final_ecc_state()
        info("%s ECC is %s", gpu, "enabled" if ecc_state else "disabled")

    if opts.clear_memory:
        if gpu.is_memory_clear_supported:
            gpu.clear_memory()
            if opts.verify_memory_is_zero:
                gpu.verify_memory_is_zero(opts.verify_memory_access_stride)

        else:
            error("Clearing memory not supported on %s", gpu)

    if opts.clear_memory_test:
        if gpu.is_memory_clear_supported:
            gpu.clear_memory_test(opts.verify_memory_access_stride)
        else:
            error("Clearing memory not supported on %s", gpu)

    if opts.detect_malware:
        detector = MalwareDetector(gpu)
        try:
            detector.detect()
        except Exception as err:
            _, _, tb = sys.exc_info()
            traceback.print_tb(tb)
            error("%s", str(err))
            error("Malware detection failed. This could mean a bug in the detection or malware running on the GPU.")
            error("Please save the EEPROM image with --save-vbios and provide it together with the error log to LWPU.")
            sys.exit(1)

    if opts.fub_ucode_headers:
        with open(opts.fub_ucode_headers[0], "r") as ucode_file:
            ucode_data_lines = ucode_file.readlines()
        with open(opts.fub_ucode_headers[1], "r") as ucode_file:
            ucode_sig_lines = ucode_file.readlines()

        ucode = parse_ucode_headers(ucode_data_lines, ucode_sig_lines)

        T4_fub["header"] = ucode["header"]
        T4_fub["data"] = ucode["data"]
        T4_fub["signature_location"] = ucode["signature_location"]
        T4_fub["signature_index"] = ucode["signature_index"]
        T4_fub["signature"] = ucode["signature_" + opts.fub_ucode_type]

    if opts.update_fuses:
        try:
            gpu.update_fuses()
        except Exception as err:
            _, _, tb = sys.exc_info()
            traceback.print_tb(tb)
            error("%s", str(err))
            error("%s Updating fuses failed", gpu)
            error("Please provide the error log to LWPU.")
            sys.exit(1)
        sys.exit(0)

    if opts.force_ecc_on_after_reset:
        if gpu.is_forcing_ecc_on_after_reset_supported:
            gpu.force_ecc_on_after_reset()
        else:
            error("Forcing ECC on after reset not supported on %s", gpu)

    if opts.save_vbios:
        with open(opts.save_vbios, "wb") as vbios_file:
            gpu.reload_vbios()
            vbios_file.write(gpu.bios.data)
        info("Saved GPU VBIOS %s to %s", gpu.bios.version, opts.save_vbios)

    if opts.compare_vbios:
        with open(opts.compare_vbios, "rb") as vbios_file:
            vbios_gold_data = vbios_file.read()

            # Reload GPU vbios in case it was loaded already
            gpu.reload_vbios()

            try:
                compare_vbios(gpu.bios, GpuVbios(vbios_gold_data))
            except Exception as err:
                _, _, tb = sys.exc_info()
                traceback.print_tb(tb)
                error("%s", str(err))
                error("Comparing GPU VBIOS found unexpected differences.")
                error("Please save the EEPROM image with --save-vbios and provide it together with the error log to LWPU.")
                sys.exit(1)

    # Trigger the errors and check for system behaviour
    if opts.trigger_errors:
        trigger_gpu_errors(gpu)


T4_scrubber = {
  "header": [
    0,
    256,
    3584,
    256,
    1,
    256,
    3328,
    256,
    0,
  ],
  "data": [
    0xcf420049, 0x95b60099, 0x0094f101, 0x0094feff, 0x0000187e, 0x00f802f8, 0xffffffdd, 0x01008e0f,
    0x04edfd00, 0x8f023cf4, 0x89060000, 0xfd000000, 0xf4bd059f, 0xf806f9fa, 0x00008903, 0x08ef9502,
    0x8905f9fd, 0xfd000e00, 0x9ebb049d, 0x1094b602, 0xfe05f9fd, 0x007e00fa, 0xa4bd0001, 0x000000f8,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x2551c48d, 0xe4fc2ce1, 0xe5bfb1e3, 0x05c9cdf1, 0x139d77c3, 0x7a651a01, 0x07f5902c, 0x94ba8049,
    0x270e26eb, 0xf2ff8265, 0x02dbeaaa, 0xc53ea029, 0xb8262053, 0xa5b2eb80, 0x0ab6b54f, 0x4abd3605,
    0x5fc8f90f, 0xf6279b3b, 0x9becd417, 0x5bb2eaa2, 0x9afdbe31, 0x28597b41, 0xa8c447a9, 0x181a4fb1,
    0xeac1f595, 0x24260cfc, 0xb6703c21, 0xd19529bd, 0x0fb09108, 0xaf7b751b, 0xbebd5089, 0x135051a2,
    0xc51f271f, 0x39a22681, 0xc270079c, 0x574b08c7, 0x30fc18e8, 0x3f321713, 0xe988d2b0, 0x7d21870c,
    0xc6bbea10, 0x2f5d0f33, 0xfc120417, 0xb827f60a, 0x15e6dad9, 0xa4fde871, 0xbbf5deaa, 0xbe6b334a,
    0x11f59bb1, 0x8dfe010a, 0xaa696350, 0xd0a66007, 0x66780bbe, 0xbe17ab45, 0x3dc57c2b, 0x69c4ca6e,
    0x68ade975, 0x01ba7d0a, 0x64324a70, 0xb86754f8, 0xba56838d, 0x4f52025d, 0xb7223347, 0x895d72b5,
    0xf7edb46c, 0x2d7c0ecf, 0xc1ca01ed, 0xfe93fde5, 0xece28d74, 0xbdfb46b1, 0xf1e1d79c, 0xa0563cef,
    0x393f0bd5, 0xd37f0da9, 0x71ec65db, 0x7a2935ea, 0x4a5e96ad, 0xb1f34b16, 0x85667673, 0x3f3b9563,
    0x5b776397, 0x5c7e9ffd, 0xa9a227a1, 0x54e2708c, 0x7b43d93d, 0x756e9479, 0xf32bde31, 0x575dd51b,
    0x6742c348, 0x6e5c0dee, 0xb9bbca5e, 0x33a45c80, 0x2f91d6c6, 0xfa8cb521, 0x425729d8, 0x3658b7d6,
    0xd72e8e48, 0x0440994d, 0xf3ff35f4, 0xf796a39e, 0x934a3086, 0xe4fa4457, 0x77bfac58, 0xd270d076,
    0xcfdc8012, 0x506e192a, 0x5554780a, 0x6ac638dc, 0x122d6841, 0x5a28487e, 0xc7dde45b, 0x0cad82c9,
    0x1f0b031f, 0x70d26ae4, 0x633a7095, 0x9f6dc2e4, 0x0bdf5dd4, 0x0dd0e97d, 0xfa02045d, 0x8a680de8,
    0x29f430aa, 0xca0a55cf, 0x5c674b3a, 0x9977a2cb, 0x537112cf, 0x3d953373, 0xee0c1d69, 0xd4534f77,
    0x3738aa8a, 0x2153dd1f, 0x7c2f502a, 0xd7128b86, 0x2079db1f, 0x0bac588d, 0xac3edbd1, 0x6f5ebc26,
    0xcc6bba47, 0x807f1766, 0xcf758e88, 0x8842f9c9, 0xb76787c9, 0x35b141ff, 0x0d366d2a, 0xe953c204,
    0xcab613c4, 0xc5bd99fa, 0xc1bff5bf, 0xa96796cc, 0xb79f719e, 0x2331fa3e, 0xa891ac51, 0xb5c9043b,
    0x383d2dd4, 0x92366bee, 0xdff05dae, 0x3480bae7, 0x89823f89, 0x07f58cdf, 0xaa1197b1, 0x7be14973,
    0xce04bb7e, 0x5b08b755, 0x85ea69a3, 0xa078d3aa, 0x11258b43, 0xb2f5cc93, 0x91bbd319, 0x5f27aac6,
    0x0bbe65e8, 0xe717f69d, 0x84e08019, 0x1cff42e5, 0x82a972a5, 0xeb38c156, 0xb7ebd0f2, 0xda1cc0ec,
    0xdcfedeac, 0x018c0f86, 0x89218208, 0x11d2a575, 0x479eeec2, 0xff932f48, 0x8fff3a07, 0xc04a62c1,
    0x1aec24a4, 0x65c9254d, 0xb77fd464, 0x956bc35b, 0x21e377a5, 0x32feb8c3, 0xfefcb1e1, 0x2c2aec8e,
    0x9147af76, 0xd391377b, 0x95626fa4, 0xfc71f387, 0xfff03100, 0xe0e2bd42, 0xfbc232cb, 0xf88e3100,
    0x126e9857, 0x6302c0a8, 0xe60ee3c5, 0x4ba0d1c9, 0x3760030b, 0xff59d9fe, 0x45639297, 0x8889f650,
    0x22fe6ada, 0xf2625efa, 0xd1ea2770, 0x56040081, 0xe985a104, 0x90db13b7, 0xef574a5f, 0x3634efab,
    0xda0976e0, 0xcc8a6e1c, 0x25f530fd, 0x2844dee9, 0x924c0b38, 0x527c78a0, 0x13025beb, 0x4237f474,
    0x6a201548, 0xbe9d243e, 0xba31db5f, 0x84866afa, 0xa2489d83, 0xe2d718d9, 0x20818560, 0xddc408e9,
    0xcbda3b96, 0x00b1feb8, 0x5690649b, 0x9344ee4f, 0xbf170ccc, 0x409c782a, 0x7d3f12eb, 0xd3019e25,
    0xc28f5fee, 0x9c24a3d4, 0x6389a96a, 0xa48bee3a, 0x90f78b9e, 0x8f4236b6, 0x6796637b, 0x1ddae11c,
    0xbd531b6f, 0x75b13209, 0x413b2419, 0x0917f86c, 0xdcdaab83, 0x76a42759, 0x46a94b03, 0xd4471c79,
    0xd3a19c0d, 0x88ff5eda, 0xccee34a3, 0xdd49cdcc, 0x8a9e5deb, 0x663eda40, 0x6a9f9381, 0x44960ab1,
    0x085eaf81, 0x9f4f088d, 0x0caf8c75, 0xd2947c17, 0x89943b1b, 0x84d2cb18, 0x45afcfa1, 0xae8ba10e,
    0xbae9d1d4, 0xd93d1a52, 0xda61d85a, 0x90534c5d, 0x63bbfca3, 0xc0ca768f, 0xd47df3c9, 0x2532b0f5,
    0x7d3505b6, 0xceb3951d, 0x9b55c91f, 0x43c53b1b, 0x91298a0a, 0xa9e52f49, 0xb9ea6241, 0xd18ed147,
    0xdbb62e07, 0xe7ea94a8, 0x265bfbed, 0xc7afd6c0, 0x0a3fa49d, 0x5feaf58f, 0x8ddbdf28, 0x48cb3e2e,
    0xb51102f5, 0x8ec2b3c1, 0x5b7957a3, 0xaf38c77c, 0x990ecfe7, 0xaff8460a, 0xd99643ae, 0x6af04e32,
    0x87cce2b8, 0x85be8376, 0x41a1f2b6, 0x1fb7c9f5, 0x9cb3ebda, 0xe78e0fe2, 0x218a426e, 0x8c6ff449,
    0x14b0e26e, 0xd1cbb92b, 0x659457c1, 0x89fd2407, 0xdb25c589, 0x1ac653ec, 0xec2068fc, 0xb089d565,
    0x353551fb, 0x68bdde20, 0x0b675225, 0xa9cf069f, 0xf0ec6f42, 0x3b72c219, 0x3fd83384, 0x657fc512,
    0x10603bcc, 0x96bd19df, 0x937d4106, 0xc71de7fc, 0xd0ed8d21, 0xb22b4a19, 0xb6f302be, 0xd64a10fc,
    0x0e6c08c6, 0xebb72ae8, 0x26c5c6bc, 0x6dd43f2b, 0xa4f37b8f, 0xfd6c3095, 0x1aa7bd4b, 0x7dca274f,
    0x26804bcf, 0xea58627f, 0xff108cd7, 0xf640d039, 0xb36cd081, 0x5d046edc, 0xaecdaecf, 0x9e30a87e,
    0x86a031b1, 0x2cacb44f, 0x337e32bc, 0x257ee551, 0x4fef6f4c, 0x13cd610c, 0x122c1fb4, 0xf2a10291,
    0xe4a30acc, 0x5f2e47dc, 0x6717764d, 0x3b4abfc8, 0x667d51a8, 0xf34b221e, 0x539b91af, 0xfe4d9986,
    0xa9079f1c, 0xa11f609b, 0x7255def7, 0xbe09237e, 0xa9612f43, 0x854ac386, 0x11335e92, 0xbed6fba3,
    0x7bff7dfd, 0xc82eff4e, 0xadbbd1a4, 0x1bfcbd8c, 0x8d7a2f2b, 0xfa3e38b2, 0xab0381e8, 0xfd7e3e16,
    0xc4455a58, 0x185cb5d9, 0x0901c61e, 0xf2efcdb7, 0x1fe79645, 0xf07bc4ae, 0x8495066c, 0x8990276f,
    0x9171c71d, 0x4153c44d, 0x2594b13d, 0x0a0bbd57, 0x21f0feed, 0xe8de6e90, 0x90aa7763, 0x5a1e67be,
    0x69e7c4f1, 0x03d94cb3, 0x7b3d802d, 0xf6dbbff1, 0xe6fb905f, 0x9a7f8116, 0xa934b8d2, 0xb1f15c68,
    0x66bee744, 0x5327d30c, 0x3186a1ad, 0x16149e9d, 0x416d06f7, 0x2e3f9967, 0xab3682c1, 0xe35f7e7a,
    0x51c19693, 0x0e9c356b, 0x2817a2dd, 0x3471eecf, 0x759ebc21, 0x35cf5ce1, 0x904147eb, 0xf2bf7e3e,
    0x74e91ea1, 0x24674a91, 0x3065a4f9, 0x40e0c204, 0x722c0674, 0x6ee20418, 0x3fe7b8d9, 0xc61ce82b,
    0xa803a462, 0x30864d28, 0xe699941d, 0x77ba0af0, 0x743dbbc2, 0x0933a17b, 0x4adf8004, 0xd12ab5ed,
    0xb60532f4, 0xb7ae44b6, 0xc05655c8, 0xb3a6bbf0, 0x6bb7c48f, 0xa9e142ec, 0xc99035c5, 0x1d458a73,
    0x69be72ce, 0x39c76ed7, 0x3cb313e0, 0xed7a16e8, 0x7f469ab5, 0x8b220777, 0x0f18e459, 0xd3cc8cd8,
    0xe4d60e7f, 0x29af0967, 0x68c5d0c4, 0xd9feae40, 0x69a802a9, 0x105ca93c, 0xa994331e, 0x3c5d6d52,
    0x3a412002, 0x62e8df2c, 0x820f36cb, 0xbec99e37, 0x16c32c41, 0xa675e601, 0x2baa1336, 0x678fc50d,
    0x9b7991fb, 0x5bd13d48, 0x786909b4, 0xc7c5cd6e, 0x89150cc6, 0xcdfdbb7c, 0x39245035, 0x1a0b68c9,
    0xffeb7104, 0x88f74a14, 0x7358541d, 0xbf22b200, 0x1d197e01, 0xde950463, 0x6517c313, 0xc120834e,
    0x1f5abda1, 0x711b46dc, 0x5109b8dc, 0x596b3de0, 0xec43edea, 0xe4c2ff5b, 0x6273d332, 0xc3a3b6f5,
    0x8244ee61, 0x6d0b105e, 0x1f09514c, 0x3e591301, 0xe3eb9429, 0x0ce5001f, 0x2051f77d, 0x052372a5,
    0x694742c6, 0xbcd154f2, 0x9be56efd, 0x085e0910, 0xab5db6f3, 0x39d9e45b, 0xa769bd06, 0xec225c6c,
    0xb8e185cb, 0xe760e75d, 0x575fb122, 0x8c23a45a, 0x146fa27a, 0x10f707f9, 0xd87c73f9, 0x7694ee56,
    0x8ee682c9, 0x5528a67e, 0xf7e6d4e5, 0x6c0fa0cc, 0x06f3c823, 0x7ac484a4, 0x2db96740, 0x063ab87b,
    0xd04b047f, 0x721bba19, 0xbb0b9644, 0xed385e94, 0x13461e9e, 0x3e0364b3, 0x951f298a, 0xe7e8d75a,
    0x13c75ec1, 0xfb954eeb, 0x27795f7e, 0x180ee378, 0xd78015fd, 0x2221f267, 0xfd77c093, 0x1090c5ee,
    0x77e8756b, 0x7d402e47, 0x1439048d, 0x53027dba, 0x9afddff9, 0xa984a05f, 0xb820584d, 0xbba45eac,
    0xd8c1a916, 0xddbd26c8, 0xc74aae2a, 0xe8811b2f, 0xc9d90845, 0x6b2e977b, 0xdb0f9eb3, 0xe6b2d7fd,
    0x9f05fc98, 0xb2e1c1c7, 0x74e622e8, 0x2143eee6, 0xb29326f6, 0x84421c24, 0x2e893c60, 0x0752cf52,
    0x8ff8b38c, 0x3d775cf0, 0x0089915e, 0x8ce8d077, 0xfd23c185, 0xaf0b329b, 0x3e02391b, 0x5d13d98b,
    0x5ad9f799, 0x3744e929, 0xf413f2a0, 0x839b385d, 0x18b8ef7b, 0x77d1d1ab, 0xea0c3ca3, 0x368c3405,
    0x6ea8f1d9, 0x6918321a, 0x4e83ffea, 0xcd3b4824, 0x6ee7ff1d, 0x4669b408, 0x700121df, 0xb536dbc4,
    0x1dcc4cab, 0xc40070f9, 0x60d6ca11, 0x6adedf48, 0xaac31df6, 0x71ea70bf, 0xf5650b7f, 0xc0f5e1b7,
    0x299d9f56, 0xa773b0d8, 0x3bdc0ac8, 0x994d66ef, 0xf24c5462, 0x38a727d1, 0x8bfa185a, 0x5659a99c,
    0xd10e8e83, 0xbd085088, 0xcb575576, 0x63302a3c, 0xb9fc0ba3, 0x7a7804d4, 0x1d37e573, 0xf2092611,
    0xc9b9a8de, 0x0be0d4ac, 0xff6fc471, 0x03b512a8, 0x049ca705, 0x64855dd3, 0x112e403b, 0x3e643b14,
    0xfec3132b, 0x3310e8b4, 0x7a1d0b90, 0x4e76177e, 0x73a9a48d, 0x2e50913b, 0x6331df5e, 0x27c6f1ee,
    0x989772f9, 0x7decc01f, 0x0c4659c4, 0x5b4c0380, 0x7bba3658, 0x5ac66687, 0x8a171507, 0xf5794a79,
    0x78e70b53, 0x065efcfa, 0x7a5b93dc, 0x82fe92c2, 0xaa8de8b2, 0xb63d71fe, 0x08535cc0, 0x38e2715c,
    0x11ad8e02, 0x7ed24fd5, 0x8e75874d, 0x82398f3a, 0xccb369e4, 0x73e98410, 0x3693723b, 0x7a1bf36b,
    0x12fab958, 0x7940a24a, 0xc8fca698, 0x43bed575, 0x6a1b7b8c, 0xef312f09, 0x68e5c91d, 0x65dba981,
    0x2b18001f, 0x1d3f6afb, 0x2c86ff53, 0xc38d59ca, 0x14c42076, 0xa56ff17e, 0xc96a7928, 0x7fb182b3,
    0x8dda6f63, 0x7ca4f247, 0x02d34c32, 0x84bd93e8, 0x1cfe88d3, 0xabd0ac90, 0xeccba2b3, 0xa38a54a0,
    0x905946ff, 0x8175fda0, 0x9646c7bf, 0x4047e1f3, 0xbdb26fe0, 0x60593834, 0x39262ca7, 0xdf38c137,
    0x5a32dea9, 0x4991cfd0, 0xc3e87921, 0x58843970, 0x4639bed3, 0x80bc4477, 0x591b4ce5, 0x38f8c975,
    0x8e134825, 0x53922dfe, 0x7593da3e, 0x934a52df, 0x873ec535, 0x586735c8, 0xef5573f3, 0x31961e8d,
    0x9f2d5aee, 0x4bc6fb4e, 0xd7629b9b, 0xacc65df9, 0xbfcbddde, 0x631a6021, 0x6b45cba3, 0x8e2dcae7,
    0xadc5708b, 0x5704b659, 0xe4d316d0, 0x494d8d80, 0x5d44c3d3, 0x7177e701, 0x69c3a7d4, 0x9b97c446,
    0x51431181, 0xefa0a830, 0x25fd11d9, 0xe05e2f01, 0xbb2fc503, 0x5f10096a, 0x80ff5ed8, 0x08e072e5,
    0x21190bd7, 0x3daaf992, 0xfe0e39ba, 0xeadeb744, 0x84eb39eb, 0x3b4156ac, 0x3ffbb654, 0xeca524cd,
    0x157d6cc0, 0xd3c15e3b, 0x45715546, 0x0040292a, 0xc9c5ff84, 0x449b19be, 0x1d3bda22, 0x3ab425c1,
    0x09be1344, 0x158ded74, 0x1bf5714d, 0x1a75eb30, 0xb923356e, 0x5fab87e0, 0x1c985c69, 0xf5b9f41a,
    0x000b5f16, 0x03f8cda4, 0x36f73f88, 0x2265b86a, 0xa150a300, 0xe9cbe5fd, 0x4c805264, 0x6b4475be,
    0xe796a095, 0x2d3d0641, 0x4a624e69, 0xc43b5720, 0x44623ebb, 0x84991260, 0x14bce860, 0xc49768d7,
    0xf24cd5be, 0x7260f2a5, 0xecb7e2ed, 0xbe3f5c98, 0xddfb12da, 0x0e14fbc1, 0x665ca0ad, 0xb37fb9c3,
    0x34c0f67b, 0x5886efde, 0x12671629, 0xd077ea6e, 0x076ea0f8, 0xbad31419, 0xa36bbf98, 0x2feddd3d,
    0x87fff262, 0x73397287, 0xf82e957e, 0x30b2763e, 0x0a85fc99, 0xd243f858, 0xe9066bc9, 0xbc8ecc79,
    0xbc4b896a, 0x501d013c, 0x95c0a007, 0x9e060eda, 0x241a98c7, 0xbd87d2e3, 0x905334ff, 0x7eb9093d,
    0x3f2babb7, 0xe350e869, 0xd6701309, 0x0e7350d8, 0x3f2babb7, 0xe350e869, 0xd6701309, 0x0e7350d8,
    0x3f2babb7, 0xe350e869, 0xd6701309, 0x0e7350d8, 0x3f2babb7, 0xe350e869, 0xd6701309, 0x0e7350d8,
    0x3f2babb7, 0xe350e869, 0xd6701309, 0x0e7350d8, 0x3f2babb7, 0xe350e869, 0xd6701309, 0x0e7350d8,
    0x3f2babb7, 0xe350e869, 0xd6701309, 0x0e7350d8, 0x3f2babb7, 0xe350e869, 0xd6701309, 0x0e7350d8,
    0x1eac0c83, 0xbc3a0b05, 0x17fdb921, 0x706db2eb, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
  ],
  "signature": [
    0x1eac0c83, 0xbc3a0b05, 0x17fdb921, 0x726db2eb,
  ],
  "signature_location": [
    3584,
  ],
  "signature_index": [
    0,
  ],
}

T4_fub = {
  "header": [
    0,
    256,
    1536,
    256,
    1,
    256,
    1280,
    256,
    0,
  ],
  "data": [
    0x0001717e, 0x8f0405fb, 0xbf000010, 0xfc30f4ff, 0xde0149fe, 0x0fffffff, 0x00899fa0, 0x9eff0001,
    0x060089d4, 0xe49eff00, 0x8f023cf4, 0x89060000, 0xfd000000, 0xf4bd059f, 0xf806f9fa, 0x49410f03,
    0x9ff71000, 0x89010f00, 0xf7046700, 0x070f009f, 0x010099b8, 0x009ff702, 0x0df4eda6, 0x92edbc1d,
    0xb608df95, 0x008e1094, 0xfefd0200, 0x05f9fd05, 0x7e00fafe, 0xfe000100, 0x9fbf0149, 0x00001089,
    0xa4bd99bf, 0x0bf4f9a6, 0x01717e07, 0x0430f400, 0x000000f8, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    #I Secure starts here
    0x799eaae5, 0x265a4313, 0x315e4d8b, 0x0ce6da26, 0xeb6d1f81, 0xca93a4e0, 0x28d17dea, 0xca8358d3,
    0x925c56e0, 0xbc79fdc4, 0x483eb5d9, 0x58eb1d42, 0x6f8cc14b, 0xf349cbe9, 0xa134af9d, 0xdc1788ba,
    0x57c61e63, 0x3ad2727f, 0xd17762a6, 0xb3bf5e96, 0x271ba8f1, 0xe18e3516, 0xeb75ff3f, 0x612bb544,
    0xac3d2db1, 0x0a12384e, 0xa705c993, 0x46026cdb, 0xa0ea0eaa, 0xac696978, 0xba1ec1d6, 0x36e8d404,
    0x77640927, 0x279552d5, 0x0afbfde6, 0x3926aca0, 0x20d7a090, 0x152ae1c7, 0x54e2c20b, 0x1cd849e2,
    0x0ad337dd, 0xdace17c5, 0x6104073c, 0x56308518, 0xf9dce37f, 0xc7a6f79b, 0x42cf56f8, 0xf3507022,
    0x59b73d6a, 0x71037d04, 0xd755dfda, 0x73420f8e, 0x4d2e7d53, 0xd2881aba, 0xd52c0693, 0x4a330238,
    0xa7560e41, 0xca40d322, 0xb8220a29, 0x0845d893, 0xa36c94f2, 0x9724aa57, 0x236a0243, 0x959a8eb2,
    0x6a0debc9, 0xc34b1004, 0x79c5f5a5, 0x89e92572, 0xc4e4575d, 0xf7b24851, 0xf4df7e2c, 0xf9c22c29,
    0x1d6ec351, 0x7aa9560e, 0x10482025, 0x5384b43e, 0x2643655f, 0xd7bda1c7, 0x350db54b, 0x4cca79f5,
    0x6f3360dd, 0x39c5910d, 0xa310d87b, 0xbf46b8e7, 0x2a84146b, 0x501a3460, 0xcbba190a, 0x14f2fd37,
    0x279038f2, 0x23d8fd48, 0x8d513ab3, 0x132b0df3, 0x3026fec2, 0xd69663b6, 0x16e0a5da, 0x4590b2d7,
    0x79025053, 0xe90dac80, 0x702d2b26, 0x18440385, 0x7917d82d, 0x7382e0f1, 0xabe54bd8, 0x337ab542,
    0xa730465d, 0xee755ff3, 0x361595d8, 0xe9090d3d, 0xf95f59a2, 0x90369f1b, 0xccb09511, 0x7e8c6610,
    0x7c3d2140, 0xeacd7e7a, 0x7643d306, 0xe1e1de34, 0x6b9bb7a2, 0xb7176e67, 0xd5d06120, 0x847ce635,
    0xbfd06a70, 0xcf6f8444, 0x53d3ad94, 0x9484ec82, 0x3c718e29, 0x1a1f332b, 0x23718b9f, 0xb3cec2a6,
    0x48550fe8, 0x57c19399, 0x265803f3, 0x88bd7132, 0x164b11e1, 0xe9eb5721, 0xe2e5f196, 0x43a3005b,
    0xd0cdbcb3, 0xe5e5b7f4, 0x7c496349, 0xc5412f4d, 0x8644f42b, 0xd63c4591, 0x35dd217f, 0x5ee4793c,
    0xb4f9d29e, 0xa406d74c, 0x684fac3d, 0x08006228, 0xbeed18d5, 0xa2af0646, 0x402defc6, 0x2f807a9d,
    0xaaaa10fc, 0x890cb212, 0xdeb0463d, 0xd4b06d52, 0xe2c89708, 0x9f2728c4, 0x90b5c70b, 0xd66bb439,
    0x5c1269cd, 0x591ada2f, 0xee8f89f5, 0x4cb70226, 0x60986647, 0xc4bb1cf5, 0xf38185c5, 0x8bb948b1,
    0x95797450, 0x791717c5, 0x202af5fe, 0xe1a0bf58, 0x3a0c0694, 0x8f332aab, 0xe8387bc5, 0x50065af5,
    0x30459e6d, 0x1339e3ce, 0x8ba67891, 0x0abb97cd, 0x6babe6b1, 0xe3fa32ac, 0x968d028f, 0x03e3c812,
    0xea55308b, 0x6d5efe1e, 0x8fa04f6a, 0x4cf2909a, 0xb5643ace, 0xf868ce33, 0x03176ad1, 0x88dca643,
    0x5014f634, 0xb7e1f694, 0x2376d920, 0xd8ad316b, 0x78d75866, 0x2a77dd3d, 0xacb56dd4, 0xf1930dc3,
    0x588d65da, 0x6a62a323, 0x87ad4f3c, 0x31716643, 0x4e665ec3, 0xe6be8551, 0x0ec1a6b7, 0x0b436a8b,
    0x28c63646, 0xd70d1d43, 0x30b3857b, 0x70643327, 0x91f4ee89, 0x2ad7e1b7, 0x43b4e8f5, 0x9d35258d,
    0x9c7d94fc, 0x1a64e60f, 0x3da4130b, 0xe487d95d, 0x0d21a79c, 0xdd307058, 0xda90a5e3, 0x3904c1e2,
    0xc941319b, 0xa6824176, 0xdbe9e15b, 0x41a26457, 0xfb0d833f, 0x40d4033f, 0xe121cd7f, 0x69704ed0,
    0x554f31af, 0xbb9fb2c8, 0x4dcef5f8, 0x535ba244, 0x1c497ecd, 0x07375c9b, 0x7f0c19e5, 0xb71d90b3,
    0xd9ebbdb2, 0x708b9563, 0x8c0897cf, 0x18b05654, 0xe6012fac, 0x4d396a5c, 0x65fde19a, 0x4a1a7a48,
    0x861931db, 0x96c62234, 0xe518a75f, 0x2c71ecba, 0x5db13224, 0x8719a65d, 0x4cc93d61, 0x90b19eae,
    0xf91b2cd4, 0x7a16562b, 0x069773bd, 0xb7286ae1, 0x69a3e969, 0x4a4e42e6, 0x7426bcaa, 0xcfa27771,
    0xa1631f5b, 0x8cde16a1, 0x3608f162, 0xc6ddb462, 0x48dd5e5c, 0xc7dfb812, 0xe061aa8a, 0x1242976a,
    0x6071f267, 0x3d2cace4, 0x0fbf10ff, 0xa59a1c10, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
  ],
  "signature": [
    0x6071f267, 0x3d2cace4, 0x0fbf10ff, 0xa59a1c10,
  ],
  "signature_location": [
    1536,
  ],
  "signature_index": [
    0,
  ],
}


if __name__ == "__main__":
    main()
