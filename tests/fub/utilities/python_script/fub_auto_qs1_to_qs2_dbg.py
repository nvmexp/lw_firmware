#!/usr/bin/elw python

#
# Copyright (c) 2018-2019, LWPU CORPORATION.  All rights reserved.
#
# LWPU CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from LWPU CORPORATION is strictly prohibited.
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

VERSION = "%VERSION%"

BAR0_SIZE = 16 * 1024 * 1024

LW_PMC_ENABLE = 0x200
LW_PMC_ENABLE_PRIV_RING = (0x1 << 5)
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
LW_PMC_ENABLE_PERFMON = (0x1 << 28)

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
    0x164000a1: {
        "name": "TU104 AUTO",
        "arch": "turing",
        "pmu_reset_in_pmc": False,
        "memory_clear_supported": True,
        "forcing_ecc_on_after_reset_supported": True,
        "lwdec": [],
        "lwenc": [],
        "other_falcons": ["sec"],
        "falcons_cfg": {
            "pmu": {
                "imem_size": 65536,
                "dmem_size": 65536,
                "imem_port_count": 1,
                "dmem_port_count": 4,
            },        
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

#I https://p4sw-swarm.lwpu.com/files/sw/rel/gfw_ucode/r1/v2/src/lwromdata.h
class FlashStatusHeader(FormattedTuple):
    namedtuple = namedtuple("flash_status_header", "identifier version header_size entry_size ledger_size")
    struct = Struct("=L4H")

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

                # Flash status ledger is present only on Turing+ (version 0x3 of IFR)
                flash_status_ledger_offset = 0

                if ifr_header.version <= 0x2:
                    #I https://wiki.lwpu.com/engwiki/index.php/VBIOS/VBIOS_Various_Topics/IFR#Version_0x01
                    image_len = self.read32(offset + 0x14)

                elif ifr_header.version == 0x3:
                    #I https://confluence.lwpu.com/display/GFWT/Redundant+Firmware%3A+Design#RedundantFirmware:Design-Navigation
                    flash_status_ledger_offset = self.read32(offset + ifr_header.total_data_size)
                    flash_status_header_offset = self.read32(offset + ifr_header.total_data_size + 4)

                    flash_status_header = self.read_formatted_tuple(FlashStatusHeader, flash_status_header_offset)

                    #I RFFS
                    #I https://p4sw-swarm.lwpu.com/files/sw/rel/gfw_ucode/r1/v2/src/lwromdata.h
                    if flash_status_header.identifier != 0x53464652:
                        raise GpuError("Unexpected flash status header identifier {}".format(flash_status_header))
                    if flash_status_header.version != 0x1:
                        raise GpuError("Unexpected flash status header version {}".format(flash_status_header))

                    rom_dir_offset = flash_status_ledger_offset + flash_status_header.ledger_size
                    rom_dir_sig = self.read32(rom_dir_offset)
                    rom_directory = self.read_formatted_tuple(RomDirectory, rom_dir_offset)

                    #I 'RFRD'
                    if rom_directory.identifier != 0x44524652:
                        raise GpuError("Unexpected rom directory identifier {}".format(rom_directory))

                    image_len = rom_directory.pci_option_rom_offset

                    # On Turing, inforom is outside of the chain of images.
                    # The corresponding ROM header can be found before the
                    # Inforom data, always 512 bytes before.
                    # Parse it first.
                    self._parse_rom_header(rom_directory.inforom_offset_physical - 512)
                else:
                    raise GpuError("Unexpected ifr version {}".format(ifr_header))


                self.ifr_image_size = image_len
                if flash_status_ledger_offset:
                    # Split IFR into pre, ledger and post sections as the
                    # ledger is expected to change while IFR pre and post are
                    # not.
                    ifr_pre_size = flash_status_ledger_offset - offset
                    ifr_post_size = image_len - ifr_pre_size - flash_status_header.ledger_size
                    ledger_size = flash_status_header.ledger_size
                    self.add_section(VbiosSection(self, "ifr_pre", offset, ifr_pre_size, ifr_header))
                    self.add_section(VbiosSection(self, "flash_status_ledger", offset + ifr_pre_size, ledger_size, flash_status_header))
                    self.add_section(VbiosSection(self, "ifr_post", offset + ifr_pre_size + ledger_size, ifr_post_size, ifr_header))
                else:
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
                raise GpuError("New {} overlapping existing {}".format(section, s))
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
        self.need_to_write_config_to_hw = True

    def __str__(self):
        return "%s offset %d (0x%x) incr %d incw %d max size %d (0x%x) control reg 0x%x = 0x%x" % (self.name,
                self.offset, self.offset, self.auto_inc_read, self.auto_inc_write,
                self.max_size, self.max_size,
                self.control_reg, self.falcon.gpu.read(self.control_reg))

    def configure(self, offset, inc_read=True, inc_write=True, selwre_imem=False):
        need_to_write = self.need_to_write_config_to_hw

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
        self.need_to_write_config_to_hw = False

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
        self.base_page = cpuctl & ~0xfff
        self.cpuctl = cpuctl
        self.pmc_enable_mask = pmc_enable_mask
        self.no_outside_reset = getattr(self, 'no_outside_reset', False)
        self.has_emem = getattr(self, 'has_emem', False)
        self._max_imem_size = None
        self._max_dmem_size = None
        self._max_emem_size = None
        self._imem_port_count = None
        self._dmem_port_count = None

        self.csb_offset_mailbox0 = getattr(self, 'csb_offset_mailbox0', 0x40)

        self.mem_ports = []
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

        self.emem_ports = []
        if self.has_emem:
            self.mem_spaces.append("emem")
            self._init_emem_ports()

        self.mem_ports = self.imem_ports + self.dmem_ports + self.emem_ports

    def _init_emem_ports(self):
        assert self.has_emem
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
    def mailbox0(self):
        return self.base_page + 0x40

    @property
    def mailbox1(self):
        return self.base_page + 0x44

    @property
    def sctl(self):
        return self.base_page + 0x240

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
        if self._max_imem_size != self.max_imem_size_from_hwcfg():
            raise GpuError("HWCFG imem doesn't match %d != %d" % (self._max_imem_size, self.max_imem_size_from_hwcfg()))

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
        if self._max_dmem_size != self.max_dmem_size_from_hwcfg():
            raise GpuError("HWCFG dmem doesn't match %d != %d" % (self._max_dmem_size, self.max_dmem_size_from_hwcfg()))

        return self._max_dmem_size

    @property
    def max_emem_size(self):
        if self._max_emem_size:
            return self._max_emem_size

        if self.name not in self.gpu.falcons_cfg or "emem_size" not in self.gpu.falcons_cfg[self.name]:
            error("Missing emem config for falcon %s, falling back to hwcfg", self.name)
            self._max_emem_size = self.max_emem_size_from_hwcfg()
        else:
            # Use the emem size provided in the GPU config
            self._max_emem_size = self.gpu.falcons_cfg[self.name]["emem_size"]

        # And make sure it matches HW
        if self._max_emem_size != self.max_emem_size_from_hwcfg():
            raise GpuError("HWCFG emem doesn't match %d != %d" % (self._max_emem_size, self.max_emem_size_from_hwcfg()))

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
        if self._dmem_port_count != self.dmem_port_count_from_hwcfg():
            raise GpuError("HWCFG dmem port count doesn't match %d != %d" % (self._dmem_port_count, self.dmem_port_count_from_hwcfg()))

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
        if self._imem_port_count != self.imem_port_count_from_hwcfg():
            raise GpuError("HWCFG imem port count doesn't match %d != %d" % (self._emem_port_count, self.emem_port_count_from_hwcfg()))

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

    def write_emem(self, data, phys_base, debug_write=False):
        self.write_port(self.emem_ports[0], data, phys_base, debug_write=debug_write)

    def read_emem(self, phys_base, size):
        return self.read_port(self.emem_ports[0], phys_base, size)

    def execute(self, bootvec=0, wait=True):
        self.gpu.write(self.bootvec, bootvec)
        self.gpu.write(self.dmactl, 0)
        self.gpu.write(self.cpuctl, 2)
        if wait:
            self.wait_for_halt()

    def wait_for_halt(self, timeout=5, sleep_interval=0):
        self.gpu.poll_register(self.name + " cpuctl", self.cpuctl, 0x10, timeout=timeout, sleep_interval=sleep_interval)

    def wait_for_stop(self, timeout=0.001):
        self.gpu.poll_register(self.name + " cpuctl", self.cpuctl, 0x1 << 5, timeout, sleep_interval=0)

    def wait_for_start(self, timeout=0.001):
        self.gpu.poll_register(self.name + " cpuctl", self.cpuctl, 0, timeout, sleep_interval=0)

    def sreset(self, timeout=0.001):
        self.gpu.write(self.cpuctl, 0x1 << 2)
        self.wait_for_halt(timeout)
        self.reset_mem_ports()

    def disable(self):
        if self.no_outside_reset:
            # No outside reset means best we can do is halt.
            self.halt()
        elif self.pmc_enable_mask:
            pmc_enable = self.gpu.read(LW_PMC_ENABLE)
            self.gpu.write(LW_PMC_ENABLE, pmc_enable & ~self.pmc_enable_mask)
        else:
            self.gpu.write(self.engine_reset, 1)

    def halt(self, wait_for_halt=True):
        self.gpu.write(self.cpuctl, 0x1 << 3)
        if wait_for_halt:
            self.wait_for_halt()

    def start(self, wait_for_start=True, timeout=0.001):
        self.gpu.write(self.cpuctl, 0x1 << 1)
        if wait_for_start:
            self.wait_for_start(timeout)

    def enable(self):
        if self.no_outside_reset:
            pass
        elif self.pmc_enable_mask:
            pmc_enable = self.gpu.read(LW_PMC_ENABLE)
            self.gpu.write(LW_PMC_ENABLE, pmc_enable | self.pmc_enable_mask)
        else:
            self.gpu.write(self.engine_reset, 0)

        self.gpu.poll_register(self.name + " dmactl", self.dmactl, value=0, timeout=1, mask=0x6)
        self.reset_mem_ports()

    def reset_mem_ports(self):
        for m in self.mem_ports:
            m.need_to_write_config_to_hw = True

    def reset(self):
        self.disable()
        self.enable()

    def is_halted(self):
        cpuctl = self.gpu.read(self.cpuctl)
        return cpuctl & 0x10 != 0

    def is_stopped(self):
        cpuctl = self.gpu.read(self.cpuctl)
        return cpuctl & (0x1 << 5) != 0

    def is_running(self):
        cpuctl = self.gpu.read(self.cpuctl)
        return cpuctl == 0

    def is_disabled(self):
        assert not self.no_outside_reset
        if self.pmc_enable_mask:
            pmc_enable = self.gpu.read(LW_PMC_ENABLE)
            return (pmc_enable & self.pmc_enable_mask) == 0
        else:
            return self.gpu.read(self.engine_reset) == 1

    def is_hsmode(self):
        return (self.gpu.read(self.sctl) & 0x2) != 0

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

        self.csb_offset_mailbox0 = 0x1000

        super(MsvldFalcon, self).__init__("msvld", LW_PMSVLD_FALCON_CPUCTL, gpu, pmc_enable_mask=pmc_enable_mask)

class MspppFalcon(GpuFalcon):
    def __init__(self, gpu):
        pmc_enable_mask = LW_PMC_ENABLE_MSPPP

        self.csb_offset_mailbox0 = 0x1000

        super(MspppFalcon, self).__init__("msppp", LW_PMSPPP_FALCON_CPUCTL, gpu, pmc_enable_mask=pmc_enable_mask)

class MspdecFalcon(GpuFalcon):
    def __init__(self, gpu):
        pmc_enable_mask = LW_PMC_ENABLE_MSPDEC

        self.csb_offset_mailbox0 = 0x1000

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

        self.csb_offset_mailbox0 = self.mailbox0

class GspFalcon(GpuFalcon):
    def __init__(self, gpu):
        if gpu.is_volta_plus:
            self.has_emem = True

        self.csb_offset_mailbox0 = 0x1000

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

        self.csb_offset_mailbox0 = 0x1000

        super(SecFalcon, self).__init__("sec", psec_cpuctl, gpu, pmc_enable_mask=pmc_enable_mask)

class FbFalcon(GpuFalcon):
    def __init__(self, gpu):

        self.no_outside_reset = True
        self.csb_offset_mailbox0 = 0x9a4040

        super(FbFalcon, self).__init__("fb", LW_PFBFALCON_FALCON_CPUCTL, gpu)

class LwDecFalcon(GpuFalcon):
    def __init__(self, gpu, lwdec=0):
        if gpu.is_turing_plus:
            cpuctl = LW_PLWDEC_FALCON_CPUCTL_TURING(lwdec)
        else:
            cpuctl = LW_PLWDEC_FALCON_CPUCTL_MAXWELL(lwdec)
        pmc_mask = LW_PMC_ENABLE_LWDEC(lwdec)

        self.csb_offset_mailbox0 = 0x1000

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

class GpuError(Exception):
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
    def is_maxwell_plus(self):
        return GPU_ARCHES.index(self.arch) >= GPU_ARCHES.index("maxwell")

    @property
    def is_pascal_plus(self):
        return GPU_ARCHES.index(self.arch) >= GPU_ARCHES.index("pascal")

    @property
    def is_pascal_10x_plus(self):
        return GPU_ARCHES.index(self.arch) >= GPU_ARCHES.index("pascal") and self.name != "P100"

    @property
    def is_volta(self):
        return GPU_ARCHES.index(self.arch) == GPU_ARCHES.index("volta")

    @property
    def is_volta_plus(self):
        return GPU_ARCHES.index(self.arch) >= GPU_ARCHES.index("volta")

    @property
    def is_turing(self):
        return GPU_ARCHES.index(self.arch) == GPU_ARCHES.index("turing")

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

    def poll_register(self, name, offset, value, timeout, sleep_interval=0.01, mask=0xffffffff, debug_print=False):
        timestamp = time.time()
        while True:
            try:
                if value >> 16 == 0xbadf:
                    reg = self.read_bad_ok(offset)
                else:
                    reg = self.read(offset)
            except:
                error("Failed to read falcon register %s (%s)", name, hex(offset))
                raise

            if reg & mask == value:
                if debug_print:
                    debug("Register %s (%s) = %s after %f secs", name, hex(offset), hex(value), time.time() - timestamp)
                return
            if time.time() - timestamp > timeout:
                raise GpuError("Timed out polling register %s (%s), value %s is not the expected %s. Timeout %f secs" % (name, hex(offset), hex(reg), hex(value), timeout))
            if sleep_interval > 0.0:
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
        else:
            turing_rom_offset = self.read(0xe218) & 0xfff
            if turing_rom_offset != 0:
                raise Exception("ROM offset {0} (0x{0:x}) not disabled on Turing, is it running the latest VBIOS?".format(turing_rom_offset))

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
        debug("%s", ucode)
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
            raise GpuError("Devinit status %s" % hex(devinit_status))

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
                raise GpuError("Bad data at offset %s = %s" % (hex(offset), hex(data)))
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
        if not self._is_read_good(reg, data):
            raise GpuError("gpu %s reg %s = %s, bad?" % (self, hex(reg), hex(data)))
        return data

    def read_bar1(self, offset):
        return self.bar1.read32(offset)

    def stop_preos(self):
        if self.arch == "kepler":
            return

        if self.is_turing_plus:
            self.poll_register("last_preos_started", 0x118234, 0x3ff, 5)

        if self.pmu.is_disabled():
            return

        if self.pmu.is_halted():
            return

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
                raise GpuError("Timed out waiting for preos to start %s" % hex(preos_status))
            preos_status = (self.read(preos_started_reg) >> 12) & 0xf

        preos_stop_reg = None
        if self.is_volta_plus:
            preos_stop_reg = self.vbios_scratch_register(1)

        # Stop preos if supported
        if preos_stop_reg:
            data = self.read(preos_stop_reg)
            self.write(preos_stop_reg, data | 0x200)
            self.pmu.wait_for_halt()

    # Init priv ring (internal bus)
    def init_priv_ring(self):
        self.write(0x12004c, 0x4)
        self.write(0x122204, 0x2)

    # Reset priv ring (internal bus)
    def reset_priv_ring(self):
        pmc_enable = self.read(LW_PMC_ENABLE)
        self.write(LW_PMC_ENABLE, pmc_enable & ~LW_PMC_ENABLE_PRIV_RING)
        self.write(LW_PMC_ENABLE, pmc_enable | LW_PMC_ENABLE_PRIV_RING)
        self.init_priv_ring()

    def disable_pgraph(self):
        pmc_enable = self.read(LW_PMC_ENABLE)
        self.write(LW_PMC_ENABLE, pmc_enable & ~LW_PMC_ENABLE_PGRAPH)

    def disable_perfmon(self):
        pmc_enable = self.read(LW_PMC_ENABLE)
        self.write(LW_PMC_ENABLE, pmc_enable & ~LW_PMC_ENABLE_PERFMON)

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

        # Dictionary of lists of memory ranges we don't want to modify. Indexed
        # by falcon name, and then memory space.
        # Lwrrently used for keeping a simple ucode present that's loaded by
        # _load_simple_ucodes().
        self.readonly_mem_ranges = {}

        self.falcon_state = {}

        self.op_counts = {}
        for falcon in self.gpu.falcons:
            self.falcon_state[falcon.name] = ""
            self.expected_mem[falcon.name] = {}
            self.readonly_mem_ranges[falcon.name] = {}
            self.op_counts[falcon] = {
                    "verified_state": 0,
                    "verified_start": 0,
                    "verified_stop": 0,
                }
            for m in falcon.mem_spaces:
                self.expected_mem[falcon.name][m] = []
                self.readonly_mem_ranges[falcon.name][m] = []
                self.op_counts[falcon][m] = {
                    "verified_words": 0,
                    "randomized_words": 0,
                }

        self.op_counts_global = {
                "verified_gpu_state": 0,
            }

    def verify_gpu_state(self):
        pmc_enable = self.gpu.read(LW_PMC_ENABLE)

        if pmc_enable & LW_PMC_ENABLE_PGRAPH != 0:
            raise GpuError("{} PGRAPH enabled 0x{:x}".format(self.gpu, pmc_enable))
        if pmc_enable & LW_PMC_ENABLE_PERFMON != 0:
            raise GpuError("{} PERFMON enabled 0x{:x}".format(self.gpu, pmc_enable))

        self.op_counts_global["verified_gpu_state"] += 1

    def verify_falcon_state(self, falcon):
        self.op_counts[falcon]["verified_state"] += 1

        if self.falcon_state[falcon.name] == "stopped":
            if not falcon.is_stopped():
                raise GpuError("Falcon %s expected to be stopped but is not" % falcon.name)
            return
        elif self.falcon_state[falcon.name] == "halted":
            if not falcon.is_halted():
                raise GpuError("Falcon %s expected to be halted but is not" % falcon.name)
            return
        elif self.falcon_state[falcon.name] == "running":
            if not falcon.is_running():
                raise GpuError("Falcon %s expected to be running but is not" % falcon.name)
            return
        elif self.falcon_state[falcon.name] == "disabled":
            if falcon.no_outside_reset:
                if not falcon.is_halted():
                    raise GpuError("Falcon %s expected to be halted but is not" % falcon.name)
                return
            if not falcon.is_disabled():
                raise GpuError("Falcon %s expected to be disabled but is enabled" % falcon.name)
            cpuctl = self.gpu.read_bad_ok(falcon.cpuctl)
            if cpuctl >> 16 != 0xbadf:
                raise GpuError("Falcon %s accessible when it should not be %s" % (falcon.name, hex(cpuctl)))
            return

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
        if self.falcon_state[falcon.name] == "disabled":
            return

        falcon.disable()

        self.falcon_state[falcon.name] = "disabled"

        # Disabling a falcon purges its mem
        for mem in falcon.mem_spaces:
            self.expected_mem[falcon.name][mem] = []
        debug("Falcon %s disabled", falcon.name)

    # Enable a particular falcon if not already enabled
    def enable_falcon(self, falcon):
        if self.falcon_state[falcon.name] != "disabled":
            return

        falcon.enable()
        self.falcon_state[falcon.name] = "halted"
        debug("Falcon %s enabled", falcon.name)

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

    def is_falcon_disabled(self, falcon):
        return self.falcon_state[falcon.name] == "disabled"

    def verify_falcon_mem(self, falcon, mem_space):
        if self.is_falcon_disabled(falcon):
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

        if falcon_mem != mem:
            raise GpuError("Falcon %s mismatched imem" % name)

        debug("Verified falcon %s imem %.1f KB at %.1f KB/s took %.1f ms port %s", name, kb, kb/t, t * 1000, port)

    def verify_falcon_random_mem_word(self, falcon):
        if self.is_falcon_disabled(falcon):
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

        if falcon_mem[0] != mem[offset]:
            raise GpuError("Falcon %s mismatched mem at %d (bytes %d 0x%x) %s != %s, mem port %s" % (name, offset, offset * 4, offset * 4, hex(falcon_mem[0]), hex(mem[offset]), port))

        self.op_counts[falcon][mem_space]["verified_words"] += 1

    def _is_mem_offset_readonly(self, falcon, mem_space, offset):
        for r in self.readonly_mem_ranges[falcon.name][mem_space]:
            if offset >= r[0] and offset < r[1]:
                return True

        return False

    def falcon_randomize_mem_word(self, falcon):
        if self.is_falcon_disabled(falcon):
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

        # Skip any offsets that are marked as readonly
        while self._is_mem_offset_readonly(falcon, mem_space, offset):
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
        self.gpu.disable_perfmon()

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

        self.verify_gpu_state()

        debug("Initial cleanup done")

    def _final_cleanup(self):
        debug("Final cleanup start")

        self.verify_falcons_memory()

        # Start any falcons that are stopped
        for falcon in self.gpu.falcons:
            if self.falcon_state[falcon.name] == "stopped":
                self.verify_falcon_start(falcon)

        self.verify_falcons_state()

        # Stop all falcons
        for falcon in self.gpu.falcons:
            self.verify_falcon_stop(falcon)

        self.verify_falcons_state()

        # Soft reset all falcons
        for falcon in self.gpu.falcons:
            falcon.sreset()
            self.falcon_state[falcon.name] = "halted"

        self.verify_falcons_state()

        # And finally disable all falcons
        for falcon in self.gpu.falcons:
            self.disable_falcon(falcon)
        self.verify_falcons_state()

        self.verify_gpu_state()

        debug("Final cleanup done")

    def status_print(self, iteration, time_start, iter_fn=info, detail_fn=debug):
        time_so_far = time.time() - time_start
        iter_fn("Iter %d %d iters/s", iteration, iteration / time_so_far)
        for falcon in self.gpu.falcons:
            counts = self.op_counts[falcon]
            detail_fn("Falcon %s verified state %d verified start %d stop %d random mem word %s",
                      falcon.name,
                      counts["verified_state"],
                      counts["verified_start"],
                      counts["verified_stop"],
                      ", ".join("%s verif %d update %d" % (m, counts[m]["verified_words"], counts[m]["randomized_words"]) for m in falcon.mem_spaces))
        detail_fn("gpu state verifications %d", self.op_counts_global["verified_gpu_state"])

    def _load_simple_ucodes(self):
        for falcon in self.gpu.falcons:

            # Simple ucode that keeps writing dmem[1] to the offset at dmem[0].
            # dmem[0] is loaded once, dmem[1] keeps being reloaded.
            # If dmem[1] is 0xdead9999 then falcon stops
            if falcon.gpu.is_maxwell_plus:
                #I 100:   89 01 00 03           mvi a9 0x30001;
                #I 104:   fe 98 00              wspr CSW a9;
                #I 107:   89 00 00 00           mvi a9 0x0;
                #I 10b:   bf 9e                 ldd.w a14 a9;
                #I 10d:   8f 04 00 00           mvi a15 0x4;
                #I 111:   dc 99 99 ad de        mvi a12 -0x21526667;
                #I 116:   dd 88 88 ad de        mvi a13 -0x21527778;
                #I 11b:   bf f9                 ldd.w a9 a15;
                #I 11d:   a6 9c                 cmp.w a9 a12;
                #I 11f:   f4 1b 0a              brne 0x129;
                #I 122:   f4 28 00              wait 0x0;
                #I 125:   3e 1b 01 00           jmp 0x11b;
                #I 129:   bf f9                 ldd.w a9 a15;
                #I 12b:   a6 9d                 cmp.w a9 a13;
                #I 12d:   f4 1b 05              brne 0x132;
                #I 130:   f8 02                 halt;
                #I 132:   bf f9                 ldd.w a9 a15;
                #I 134:   fa e9 01              stxb a14 a9;
                #I 137:   3e 1b 01 00           jmp 0x11b;
                imem = [0x03000189, 0x890098fe, 0xbf000000, 0x00048f9e, 0x9999dc00,
                        0x88dddead, 0xbfdead88, 0xf49ca6f9, 0x28f40a1b, 0x011b3e00,
                        0xa6f9bf00, 0x051bf49d, 0xf9bf02f8, 0x3e01e9fa, 0x0000011b]
            else:
                #I 100:   f0 97 01              mvi a9 0x1;
                #I 103:   f0 93 03              sethi a9 0x3;
                #I 106:   fe 98 00              wspr CSW a9;
                #I 109:   f1 97 00 00           mvi a9 0x0;
                #I 10d:   98 9e 00              ldd.w a14 a9 0x0;
                #I 110:   f1 f7 04 00           mvi a15 0x4;
                #I 114:   f1 c7 99 99           mvi a12 -0x6667;
                #I 118:   f1 c3 ad de           sethi a12 0xdead;
                #I 11c:   f1 d7 88 88           mvi a13 -0x7778;
                #I 120:   f1 d3 ad de           sethi a13 0xdead;
                #I 124:   98 f9 00              ldd.w a9 a15 0x0;
                #I 127:   b8 9c 06              cmp.w a9 a12;
                #I 12a:   f4 1b 0a              brne 0x134;
                #I 12d:   f4 28 00              wait 0x0;
                #I 130:   3e 24 01 00           jmp 0x124;
                #I 134:   98 f9 00              ldd.w a9 a15 0x0;
                #I 137:   b8 9d 06              cmp.w a9 a13;
                #I 13a:   f4 1b 05              brne 0x13f;
                #I 13d:   f8 02                 halt;
                #I 13f:   98 f9 00              ldd.w a9 a15 0x0;
                #I 142:   fa e9 01              stxb a14 a9;
                #I 145:   3e 24 01 00           jmp 0x124;
                imem = [0xf00197f0, 0x98fe0393, 0x0097f100, 0x009e9800, 0x0004f7f1, 0x9999c7f1,
                        0xdeadc3f1, 0x8888d7f1, 0xdeadd3f1, 0xb800f998, 0x1bf4069c, 0x0028f40a,
                        0x0001243e, 0xb800f998, 0x1bf4069d, 0x9802f805, 0xe9fa00f9, 0x01243e01]

            dmem = [falcon.csb_offset_mailbox0, 0xcafe]

            falcon.load_imem(imem, phys_base=0x0, virt_base=0x100)
            falcon.load_dmem(dmem, 0)

            # The ucode uses IMEMT 0x1 (for 0x100 IMEM VA), reset it to
            # something else so that it doesn't get overwritten as part of
            # random imem writes.
            falcon.gpu.write(falcon.imemt, 0)

            # Writing imem with tags requires writing a full block (0x100
            # bytes) and it's filled up with 0x0 beyond our imem.
            for i in range(0, 0x100 // 4):
                self.expected_mem[falcon.name]["imem"][i] = imem[i] if i < len(imem) else 0

            for i, mem_word in enumerate(dmem):
                self.expected_mem[falcon.name]["dmem"][i] = mem_word

            # Mark the imem and dmem ranges as readonly so they don't get overwritten
            self.readonly_mem_ranges[falcon.name]["imem"].append((0, len(imem)))
            self.readonly_mem_ranges[falcon.name]["dmem"].append((0, len(dmem)))

            falcon.execute(0x100, wait=False)

            self.gpu.poll_register(falcon.name + " mailbox0", falcon.mailbox0, 0xcafe, timeout=1.01, sleep_interval=0)
            self.falcon_state[falcon.name] = "running"

    def verify_falcon_start(self, falcon):
        assert self.falcon_state[falcon.name] != "running"

        # Randomize the word the simple ucode puts in the mailbox, but only go
        # up to 0xdead0000 as 0xdeadXXXX are the stop/halt commands.
        random_word = random.randint(0, 0xdead0000)
        falcon.load_dmem([random_word], phys_base=4)
        self.expected_mem[falcon.name]["dmem"][1] = random_word

        # Allow up to 100 us for falcon to start
        falcon.start(timeout=0.0001)

        # Allow up to 100 us for falcon to copy the value
        self.gpu.poll_register(falcon.name + " mailbox0", falcon.mailbox0, random_word, timeout=0.0001, sleep_interval=0)

        self.op_counts[falcon]["verified_start"] += 1

        self.falcon_state[falcon.name] = "running"

    def verify_falcon_stop(self, falcon):
        assert self.falcon_state[falcon.name] == "running"

        dmem_cmd = 0xdead9999

        falcon.load_dmem([dmem_cmd], phys_base=4)
        self.expected_mem[falcon.name]["dmem"][1] = dmem_cmd

        # Allow max 100us for falcon to transition
        falcon.wait_for_stop(timeout=0.0001)

        self.falcon_state[falcon.name] = "stopped"
        self.op_counts[falcon]["verified_stop"] += 1

    def verify_falcon_stop_or_start(self):
        falcon = self.random_falcon()

        if self.falcon_state[falcon.name] == "running":
            self.verify_falcon_stop(falcon)
        else:
            self.verify_falcon_start(falcon)

    def detect(self, iters=500000):
        debug("Malware detection start")
        total_time_start = time.time()

        self._initial_cleanup()
        self._load_simple_ucodes()

        operations = [
            "verify_falcon_state",
            "verify_falcon_mem_word",
            "randomize_falcon_mem_word",
            "verify_gpu_state",
            "verify_falcon_stop_or_start",
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
            elif op == "verify_gpu_state":
                self.verify_gpu_state()
            elif op == "verify_falcon_stop_or_start":
                self.verify_falcon_stop_or_start()
            else:
                assert 0, "Unhandled op %s" % op

            if i > 0 and i % 100000 == 0:
                self.status_print(i, t)

        self.status_print(i, t, detail_fn=info)
        debug("Malware detection loop done")
        self._final_cleanup()

        if self.gpu.is_volta_plus:
            self._run_sanity_ucode()

        if self.gpu.arch == "kepler" or self.gpu.arch == "maxwell":
            self.gpu.reset_priv_ring()

        total_time = time.time() - total_time_start
        info("No malware detected, took %.1f s", total_time)

    def _run_sanity_ucode(self):
        if self.gpu.is_turing:
            sanity_binary = T4_sanity_ucode
        elif self.gpu.is_volta:
            sanity_binary = V100_sanity_ucode
        else:
            assert 0, "Unsupported GPU"

        ucode = GfwUcode("sanity ucode", sanity_binary)

        self.gpu.sec.reset()

        self.gpu.load_ucode(self.gpu.sec, ucode)

        # Sanity ucode takes for 4 words of random data and copies them to a
        # specified location
        random_words_num = 4
        random_data = [random.randint(0, 0xffffffff) for r in range(random_words_num)]

        # Random location to copy the 4 words to, aligned to 4 words and
        # skipping first two offsets that would overlap with the input.
        random_dest = random.randrange(0, self.gpu.sec.max_emem_size // (random_words_num * 4) - 2) + 2

        # Destination offset in bytes
        random_dest *= random_words_num * 4

        self.gpu.sec.write_emem(random_data + [random_dest], phys_base=0)
        self.gpu.sec.execute(ucode.virtual_entry)

        sanity_result = self.gpu.sec.read_emem(random_dest, random_words_num * 4)
        mbox0 = self.gpu.read(self.gpu.sec.mailbox0)
        mbox1 = self.gpu.read(self.gpu.sec.mailbox1)

        if sanity_result != random_data:
            raise GpuError("Running sanity ucode failed, mbox0 0x%x mbox1 0x%x" % (mbox0, mbox1))

        if not self.gpu.sec.is_hsmode():
            raise GpuError("Sanity ucode didn't halt in HS mode mbox0 0x%x mbox1 0x%x" % (mbox0, mbox1))

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
    expected_diffs = set(["inforom", "inforom_backup", "erase_ledger", "flash_status_ledger"])

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

def update_fuses(gpu):
    if not gpu.is_turing_plus:
        return

    # Make sure GPU is fully initialized
    gpu.clear_memory()

    # Disable all falcons
    for f in gpu.falcons:
        f.disable()

    preconditions = [
        # Check device ids
        #I LW_FUSE_OPT_PCIE_DEVIDA   @(0x000214d8) = 0x00001EBC
        #I LW_FUSE_OPT_PCIE_DEVIDB   @(0x0002156c) = 0x00001EFC
        (0x214d8, 0xffffffff, 0x1EBC, "device id a"),
        (0x2156c, 0xffffffff, 0x1EFC, "device id b"),

        # Check that the fuse update microcode hasn't been used
        #I FUB self-revocation fuse is expected to be 0
        #I LW_FPF_OPT_SEC2_UCODE7_VERSION
        (0x820158, 0xffffffff, 0, "fuse update microcode not used before"),

        # Check that LW_FUSE_SPARE_BIT_37 is set to 1, to make sure AUTO SKU
        (0x21ED0, 0x1, 0x1, "FUSE_SPARE_BIT_37 is burnt"),
        
        # LW_FUSE_OPT_PKC_PK_ARRAY_INDEX should be 0x3
        (0x21748, 0x3, 0x3, "Correct PKC Index set"),

        # Check AES Verification is disabled on SEC2
        (0x841184, 0x1, 0x0, "AES validation disabled on SEC2"),
        
        # Check AES Verification is disabled on PMU
        (0x10b184, 0x1, 0x0, "AES validation disabled on PMU"),

        # Check AES Verification is disabled on GSP
        (0x111184 , 0x1, 0x0, "AES validation disabled on GSP"),

    ]

    # Check for fuses already being burnt
    # AHESASC -> LW_FPF_OPT_SEC2_UCODE1_VERSION @(0x00820140)
    ahesasc_fuse = gpu.read(0x820140)
    ahesasc_fuse_burnt = ahesasc_fuse & (0x1) != 0

    # ASB -> LW_FPF_OPT_GSP_UCODE1_VERSION @(0x008201C0)
    asb_fuse = gpu.read(0x8201C0)
    asb_fuse_burnt = asb_fuse & (0x1) != 0

    # FwSec -> LW_FPF_OPT_GSP_UCODE9_VERSION @(0x008201E0)
    fwsec_fuse = gpu.read(0x8201E0)
    fwsec_fuse_burnt = fwsec_fuse & (0x1) != 0

    # HULK -> LW_FPF_OPT_SEC2_UCODE10_VERSION @(0x00820164)
    hulk_fuse = gpu.read(0x820164)
    hulk_fuse_burnt = hulk_fuse & (0x1) != 0

    # ImageSelect -> LW_FPF_OPT_PMU_UCODE11_VERSION @(0x008201A8)
    imageselect_fuse = gpu.read(0x8201A8)
    imageselect_fuse_burnt = imageselect_fuse & (0x1) != 0

    # LWFLASH -> LW_FPF_OPT_PMU_UCODE8_VERSION @(0x0082019C)
    lwflash_fuse = gpu.read(0x82019C)
    lwflash_fuse_burnt = lwflash_fuse & (0x1) != 0

    
    info("Current fuse values AHESASC 0x{:x} burnt {} ASB 0x{:x} burnt {}".format(ahesasc_fuse, ahesasc_fuse_burnt, asb_fuse, asb_fuse_burnt))
    info("Current fuse values FWSEC 0x{:x} burnt {} HULK 0x{:x} burnt {} ImageSelect 0x{:x} burnt {} LWFLASH 0x{:x} burnt {}".format(fwsec_fuse, fwsec_fuse_burnt, hulk_fuse, hulk_fuse_burnt, imageselect_fuse, imageselect_fuse_burnt, lwflash_fuse, lwflash_fuse_burnt))
    if ahesasc_fuse_burnt and asb_fuse_burnt and fwsec_fuse_burnt and hulk_fuse_burnt and imageselect_fuse_burnt and lwflash_fuse_burnt:
        info("Fuses already burnt, exiting early")
        return

    # Check all other preconditions
    values = {}
    for cond in preconditions:
        values[cond[0]] = gpu.read(cond[0])

    for cond in preconditions:
        if (values[cond[0]] & cond[1]) != cond[2]:
            raise GpuError("Offset 0x{:x} unexpected value 0x{:x}, condtion '{}' unmet. All values {}".format(cond[0], values[cond[0]], cond[3],
                           ", ".join(["0x{:x} = 0x{:x}".format(o, values[o]) for o in values])))

    falcon = gpu.sec

    falcon.reset()

    gpu.load_ucode(falcon, DriverUcode("fub", T4_fub))

    falcon.execute()

    mbox0 = gpu.read(0x840040)
    mbox1 = gpu.read(0x840044)
    debug("After running fuse update microcode mbox0 0x{:x} mbox1 0x{:x}".format(mbox0, mbox1))

    # Check for fuses after burning

    # AHESASC -> LW_FPF_OPT_SEC2_UCODE1_VERSION @(0x00820140)
    ahesasc_fuse = gpu.read(0x820140)
    ahesasc_fuse_burnt = ahesasc_fuse & (0x1) != 0

    # ASB -> LW_FPF_OPT_GSP_UCODE1_VERSION @(0x008201C0)
    asb_fuse = gpu.read(0x8201C0)
    asb_fuse_burnt = asb_fuse & (0x1) != 0

    # FwSec -> LW_FPF_OPT_GSP_UCODE9_VERSION @(0x008201E0)
    fwsec_fuse = gpu.read(0x8201E0)
    fwsec_fuse_burnt = fwsec_fuse & (0x1) != 0

    # HULK -> LW_FPF_OPT_SEC2_UCODE10_VERSION @(0x00820164)
    hulk_fuse = gpu.read(0x820164)
    hulk_fuse_burnt = hulk_fuse & (0x1) != 0

    # ImageSelect -> LW_FPF_OPT_PMU_UCODE11_VERSION @(0x008201A8)
    imageselect_fuse = gpu.read(0x8201A8)
    imageselect_fuse_burnt = imageselect_fuse & (0x1) != 0

    # LWFLASH -> LW_FPF_OPT_PMU_UCODE8_VERSION @(0x0082019C)
    lwflash_fuse = gpu.read(0x82019C)
    lwflash_fuse_burnt = lwflash_fuse & (0x1) != 0

    info("New fuse values AHESASC 0x{:x} burnt {} ASB 0x{:x} burnt {}".format(ahesasc_fuse, ahesasc_fuse_burnt, asb_fuse, asb_fuse_burnt))
    info("New fuse values FWSEC 0x{:x} burnt {} HULK 0x{:x} burnt {} ImageSelect 0x{:x} burnt {} LWFLASH 0x{:x} burnt {}".format(fwsec_fuse, fwsec_fuse_burnt, hulk_fuse, hulk_fuse_burnt, imageselect_fuse, imageselect_fuse_burnt, lwflash_fuse, lwflash_fuse_burnt))
    if ahesasc_fuse_burnt and asb_fuse_burnt and fwsec_fuse_burnt and hulk_fuse_burnt and imageselect_fuse_burnt and lwflash_fuse_burnt:
        info("Fuses burnt successfully")
    else:
        raise GpuError("Failed to burn fuses, mbox0 0x{:x} mbox1 0x{:x}".format(mbox0, mbox1))


def print_topo():
    print("Topo:")
    for c in DEVICES:
        dev = DEVICES[c]
        if dev.is_root():
            print_topo_indent(dev, 1)

def sysfs_pci_rescan():
    with open("/sys/bus/pci/rescan", "w") as rf:
        rf.write("1")

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
                      help="""Update T4 fuses""")
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

    print("LWPU GPU Tools version {}".format(VERSION))

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

    if opts.gpu >= len(gpus):
        raise ValueError("GPU index out of bounds")

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

    if opts.update_fuses:
        try:
            update_fuses(gpu)
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


T4_fub = {
  "header": [
    0,
    256,
    65536,
    1280,
    1,
    256,
    65280,
    256,
    0,
  ],
  "data": [
    0x0004408f, 0x30f4ffbf, 0xfe02f9fc, 0x99900149, 0x499fa004, 0x99ce4200, 0x0195b600, 0xff0094f1,
    0x890094fe, 0x800264f1, 0xa0000440, 0x00477e09, 0x0149fe00, 0xbf049990, 0xa609bf9f, 0x070bf4f9,
    0x0001687e, 0x8f0405fb, 0xbf000440, 0x8e94bdff, 0xf4000220, 0xe9b5fc30, 0x80e9b581, 0xa00149fe,
    0x0100899f, 0xffffdd00, 0x9dff0fff, 0x000089c4, 0xd49dff01, 0x04840089, 0x0f009ef7, 0x0099b801,
    0x9ff7021d, 0xb8070f00, 0x02010099, 0x0f009ff7, 0x0099b801, 0x9ff70206, 0x00f14f00, 0xf7100049,
    0xdca6009f, 0xbc1d0df4, 0xcf9592dc, 0x1094b608, 0x0200008e, 0xfd05fefd, 0xfafe05f9, 0x01007e00,
    0xfe02f800, 0x9fbf0149, 0x00044089, 0xa4bd99bf, 0x0bf4f9a6, 0x01687e07, 0x0430f400, 0x000000f8,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x7cee829e, 0xe65b4c1d, 0x2150c5df, 0xad4f28e0, 0x6f2bcb19, 0xed1d3980, 0x7bffae2a, 0xb9966d46,
    0x14aba087, 0x735d3c38, 0x567e84f1, 0x1af910c3, 0xc48565e6, 0x17336cf7, 0xd434e092, 0xb00ed448,
    0x486e8f21, 0x6acbf7df, 0x4f7d1e20, 0x307a2350, 0xee634cad, 0x92c4569b, 0x35c92576, 0x283f8060,
    0x2c7d42dd, 0xc0bd90cc, 0x0f7dd6c4, 0xfdc24d5d, 0x6ca5ea0f, 0x1e1530f6, 0x4f7de478, 0xd0ec435e,
    0x58422224, 0xada63b1c, 0x304a384e, 0x25c82b72, 0x19debe7a, 0x883b17e5, 0x634de9e5, 0xc88e4a4d,
    0x5beaf98a, 0xbd1233bf, 0xff21ebad, 0x238d3cd2, 0xbe35da23, 0x4c0663fb, 0x3f4accde, 0xde5dcc8e,
    0xb460111f, 0xa9e4b67d, 0x7304cae4, 0x08e676c0, 0xa6bd2836, 0x7bd4b103, 0x50a8bbf3, 0x6f9aa70d,
    0xc78fc3f1, 0x08847d86, 0x2f58c117, 0x80c50b17, 0x457a27c9, 0xd376689b, 0x6ae38bcb, 0xad1cf2bd,
    0x3c1c754d, 0xe0b8d93e, 0x13d5e6e8, 0x041dca23, 0x57767a99, 0xcd2eae66, 0x2c1b254b, 0xbacd97da,
    0xfc145708, 0x875c80e6, 0x70a3ee45, 0x68926861, 0x181eef41, 0x3294cf28, 0x54cc6d5f, 0x5165296d,
    0x43e88e09, 0xaafba1d5, 0xba9178a9, 0xd3d1fd35, 0xf8b861dc, 0x26424c0a, 0x71073a28, 0xa71c33d9,
    0xd10d3b93, 0x16b83f8a, 0x490ead29, 0x06be4476, 0xc0274015, 0x29309a13, 0x5627553f, 0xe470c1dd,
    0x01648a3d, 0x3d8f08bb, 0xbb9b5c91, 0x553c46f8, 0x36fd5cb4, 0xafc75133, 0x7f75025a, 0x0ce5f1f1,
    0xe7010df2, 0xa0d8eb4e, 0x9788c2e0, 0x1ea7c4fb, 0x13496d44, 0x039c017e, 0xf818f26c, 0x98686b2b,
    0xe04f2c0a, 0xdb210169, 0x9f73c093, 0x4cef387b, 0x8e615362, 0xd1a9b101, 0x403917b9, 0x6a1dfc06,
    0x09954963, 0xb2ce29d1, 0xd8ca10f9, 0x9ce3d01b, 0xdf578c00, 0xbf1f7218, 0xc066f4e1, 0xfb3c2d50,
    0xd3b63a0d, 0x66e4133c, 0x3c894387, 0x84b97748, 0x24c14821, 0x9f8fb491, 0xd88fb9e4, 0xc6948a87,
    0x22b7d622, 0x2a379fc4, 0x10f7e58e, 0xa9e81179, 0x4c915fec, 0x48560230, 0xc52d2325, 0xc873a0fc,
    0x923dd1af, 0x5e0eefb8, 0x80a31d17, 0xd5211823, 0xe80cf12f, 0xf006ff80, 0x59ca8fad, 0x2b44b5da,
    0x55dd7bbc, 0xf2728a4d, 0xc134cfab, 0x513973cf, 0x37f86ce8, 0xf393c9cd, 0x8d89ab30, 0xce12e935,
    0x089258bd, 0xb4a81e6c, 0x1a74691a, 0xc5c49813, 0x99f05726, 0xce60119e, 0x24ec6fb9, 0x95436953,
    0x49376b99, 0xc6938658, 0x5c99c1e6, 0x1ff856fb, 0xed743a8b, 0x2cab1deb, 0x9a40c7d9, 0x3115030d,
    0xd3ca1756, 0x9cad057a, 0x93f83d32, 0xf627e848, 0x867f4bd7, 0x0c89afa7, 0x3e78d132, 0x897a2f8c,
    0x8297d7b5, 0x6f32495a, 0xcd61d747, 0xfc89c0af, 0xe8e25089, 0x711801b4, 0x25112a29, 0x210f6601,
    0xd5e5ea6b, 0xe44805fb, 0xd9cf59fb, 0x270aabf6, 0x7b56413f, 0xe9d58dd1, 0xdd094d3b, 0xf5ea9de2,
    0x2809e8b7, 0xe95e1b6c, 0x0dadd0fc, 0x0fc14f16, 0xbe56f5c7, 0x68e61c47, 0xd1d0fcd3, 0x5b9b06bf,
    0xead76d61, 0x5b9536b6, 0x888565fb, 0x3a32361a, 0x275f8d5a, 0x2380d1a7, 0x39960c5e, 0x8509d975,
    0x9c5ce5cd, 0xa0c103a0, 0x7185081b, 0xeb0d1c69, 0x12348add, 0x553ce680, 0x072c2805, 0xe97ebb56,
    0x6c41fbd7, 0x9f3a7e92, 0x80e96086, 0x46c145ba, 0x65a1ad92, 0xdfac6c0b, 0x02e7e212, 0xe5b57b98,
    0x98ca1fc5, 0x3d32124b, 0xaba9e85d, 0x013b5c08, 0x45f8f679, 0xfb8e6f9e, 0x0b758e98, 0x3a8f5286,
    0x060158ef, 0x52091398, 0xa3406e25, 0x8b255975, 0xae209cee, 0x293386ac, 0x1c4332c3, 0x13ad1e85,
    0xb85a52e1, 0x5143bc8e, 0x5665c821, 0x08415783, 0x100adb73, 0x03a61828, 0x87b0d594, 0xa1b14d53,
    0xcb40a0ea, 0xbc20e1ee, 0xec2bf7d5, 0x8b6c7010, 0x6d5b2255, 0x57956a63, 0x85fcf2fe, 0xdb9df726,
    0x3a542026, 0x45d29a27, 0xe38f22ff, 0x7303523b, 0x813d9bd6, 0x4dcc5b2b, 0x2e41a988, 0x9d246728,
    0xd3aafce0, 0x39534bf4, 0x87bd91ec, 0x94cb3325, 0x35a44707, 0xea248d3c, 0x9030b606, 0xafec79a7,
    0xe080e411, 0x791f2a9a, 0xf9e957ff, 0x863be597, 0x84efc3fe, 0x444d561b, 0xa0750a6e, 0xa3694d0d,
    0x80fc7912, 0x7bf37b42, 0xafe19053, 0xdcd7c23b, 0xff8d9f18, 0xac177977, 0x2c7e1c4a, 0x4716255a,
    0xad3075b5, 0xcd3831c8, 0x816d41b1, 0xfb8fdb1f, 0x45d3b2ce, 0xc39d7c75, 0x74397185, 0xe63a642b,
    0x9a277024, 0xb756a2c2, 0xbded7049, 0x1c6082df, 0x7fe1a3c8, 0x361ead2d, 0x04f32a60, 0xb9abd10c,
    0xee296e61, 0x50b4536d, 0x8ab13706, 0x60089898, 0xcc9c4192, 0x6e7988a2, 0x7cffe106, 0x624132bd,
    0x6d2ceaf8, 0x61b24075, 0xfb51c4e9, 0x63571332, 0x4e6c1219, 0x0aeb0858, 0x7fde6bf4, 0xe1e037be,
    0xde3ea3e6, 0x8af21a4b, 0xeb02e3a9, 0x440138d6, 0xda7b1212, 0xf8858c87, 0x35e7f218, 0xddc5a66f,
    0x4cfe9702, 0xe1617fe8, 0xdb2245b5, 0x95228760, 0xed5e4fb4, 0xf7ba62b0, 0x7ef53ab1, 0x2c22ad17,
    0x7fec691b, 0xe664fd73, 0x88c81f28, 0x8c77c43a, 0x748e0d2b, 0x5275dc2d, 0x919795c1, 0xbf04f7b8,
    0x36ea9d07, 0x15171592, 0x2deac159, 0xca9d1788, 0xd6c774c1, 0x894ad0b0, 0x1a49751b, 0x17cc005d,
    0x278cbbc0, 0x4ab4d10c, 0xd8e2beab, 0x69475462, 0x101fc5d4, 0xc94a76d0, 0x9dc4a175, 0xec6bbe67,
    0x8eb58cf4, 0xf808ef5b, 0x2b3fbae0, 0x38b79de0, 0xc8528ba9, 0x72fdc7cd, 0x43a2e11c, 0x2f1136e7,
    0x37f66494, 0x44ed2748, 0x8585430e, 0x83a1bcc6, 0x13655142, 0x47136541, 0x73f67ea0, 0x8164e613,
    0x3573b1b4, 0xe5ae4e22, 0x0ea3d321, 0x85f01fab, 0x7551f8c4, 0xbc744f3e, 0x578f4340, 0x56853301,
    0xbe4a696c, 0x2f403613, 0x9121481b, 0xe062884c, 0xdb1c9215, 0x1d26f8ba, 0x3be012ee, 0xb7d4cc84,
    0xd6c05c18, 0x68302f5a, 0x307c3920, 0xcde59ef1, 0x000fb320, 0x2251e55e, 0x07862da1, 0x40bb52de,
    0x033771da, 0x0843031c, 0xcb31c83d, 0xa901cfaf, 0xd49261eb, 0xd3a1a3b8, 0x591cc0f2, 0x7304b4dc,
    0xd5c5e88c, 0x7d5f0b7b, 0xbb725c2c, 0xfeb30bdf, 0x1d9ffe4d, 0xe2b9d933, 0x4b2d217e, 0x706c93bc,
    0xd8a71b32, 0x0146bf3f, 0x7bf3d716, 0xf27d6bb7, 0xc61f2f2c, 0x94dcd305, 0xc4625ff7, 0x7420bd9a,
    0x44e891b6, 0x9fdf9d65, 0x1b5e7907, 0x2d5f81f9, 0xefbea081, 0xabdbf3c9, 0x3b9ff26f, 0xf12cf228,
    0xee7f3368, 0x2c60f63a, 0x4ac9bf98, 0x40b00d0e, 0xb59235a6, 0x2a9fb15d, 0x1d5d0b6e, 0x3dfdbfbc,
    0x2878ae85, 0x086d7748, 0x79d00ce4, 0x3670550d, 0xc19adc79, 0x98bdb63c, 0x0a2eec95, 0x53aee4e2,
    0xab9522b4, 0xb46fdbf4, 0xd0d74de0, 0x21e920cd, 0x91af0cab, 0xa687a475, 0xa639c554, 0x7f6a5f79,
    0xc0cfb2bb, 0x3fd1ad89, 0x83246ca4, 0xaeccac63, 0x001d15f2, 0x9104c8e8, 0x06a7a778, 0x78f72c61,
    0xac1afd0c, 0xa4065e87, 0x665b0273, 0xa5818e32, 0x8a813f11, 0x6688fdb8, 0x85bbe7b2, 0x2d7f1c22,
    0xe27a625f, 0xc91c45c6, 0x328afc21, 0xc5a67499, 0xc6918726, 0xf48f03ea, 0xdcca3680, 0x5a07b895,
    0xf99a5706, 0x34d5a98d, 0x59f0275d, 0x905b92dd, 0x94d12f2f, 0xdea97727, 0x602c6ce6, 0x3d2ef9d2,
    0x8941e92c, 0xbbbddc00, 0xe2563663, 0xc0f35cef, 0x5c062997, 0x15c09b5a, 0xecea15fd, 0x20de6831,
    0xdafb4b44, 0xb8e5060b, 0x6c239ca4, 0x8eca7d19, 0xda04ce72, 0xd0d3fc63, 0x506f9f7d, 0xf4350da6,
    0xa828ec60, 0x6ad161cb, 0x9dd96cc9, 0xeca339e2, 0x92c4663b, 0x92f27fbe, 0xa7c5b3d7, 0x16faafbe,
    0x545cbe31, 0xc06a547d, 0x0cd27b64, 0x2b3c922b, 0x84574108, 0xfd303a4f, 0x1cdc8d1b, 0x6ffeb17c,
    0x200faf5d, 0xed1374b3, 0xb9f0648f, 0xc824f98d, 0x24e6982e, 0x02de6716, 0x05c7753b, 0xc76a66ae,
    0x48ffaf64, 0x07d0274a, 0x7e7bb8d8, 0x0bbe3522, 0x849eff24, 0x9a1aef42, 0x57e8180e, 0x5114b270,
    0x31cd5ce1, 0xb461ac28, 0x815043c4, 0xc824e548, 0xbf424dca, 0xfb60d671, 0xc212d699, 0x057cd41b,
    0xa9bf029a, 0xef1e1904, 0xf62e143f, 0x7ce011eb, 0x1ca23f1a, 0x66127b75, 0x7d38a3c0, 0x525bc7cf,
    0x2a4f532f, 0xf1545bea, 0x5bfa72c3, 0xe0a68da7, 0x826ca080, 0x119b599b, 0x0c093a60, 0xa35a8306,
    0xae741202, 0x216cd22e, 0xd3228227, 0x5fcf095f, 0x18d8c2d2, 0xf4837b95, 0xb412144c, 0xdf9bc4e8,
    0xb0d6242c, 0x7e560bfc, 0x1c109c4c, 0x6d2848fe, 0xc6513b10, 0x1396453d, 0xeaab8f88, 0xc0da2de2,
    0x5e5400d4, 0x0252775b, 0x1c721954, 0x23de55a2, 0x98d914f7, 0x1057e561, 0x42470569, 0x38b38873,
    0x89004d83, 0x599659f0, 0xb6a2e823, 0x7799ac29, 0xaaed2c89, 0x9850ab2b, 0xb663174d, 0xe029cf7f,
    0xdbac1a52, 0xf113fd1e, 0xdfa637c5, 0xc63f2677, 0x524437ad, 0x75d3b021, 0x0b54c53d, 0x869ba256,
    0x1229e53e, 0xf917a751, 0xcab42208, 0x0223d258, 0x8298a580, 0xa83beb6f, 0x9b66834f, 0x7232aef3,
    0xae209cee, 0x293386ac, 0x1c4332c3, 0x13ad1e85, 0x271b994b, 0xa585aa0f, 0x73ad0d4e, 0xea94914b,
    0xe9a35d19, 0xbd33b55c, 0xc0fd7844, 0xac46e574, 0x010b3f9f, 0x0205be10, 0x814b3c07, 0x4510e4be,
    0x7ff217b3, 0x18cc3f04, 0x7418e0bb, 0x74c3ace6, 0xd35f9470, 0xe5c58548, 0x2135e188, 0xa5ab325b,
    0xf06944fa, 0x95676dbb, 0x27a23dd4, 0x7dc14832, 0x8e29aa6a, 0xd0082d8d, 0x2348cff6, 0xf59ebcba,
    0x7230693f, 0xb89c9e08, 0xb7709a11, 0x6c2e885f, 0x50497498, 0xe192288d, 0x048300b5, 0x6d53e0b4,
    0x835097d3, 0xb61ed04b, 0x9cce556a, 0xffbcedf3, 0x1daa30fb, 0x7fbaf52e, 0xf538dc79, 0xce102651,
    0xa73f07fb, 0x2916e1e7, 0xefd0ba82, 0x1fd87eb4, 0x399fb289, 0x3ac61923, 0x36e820f6, 0x68a15b29,
    0xd431c088, 0x236fc5d0, 0x3712d847, 0xd0ac5f9c, 0xf78608a5, 0x055a97d5, 0x77223cad, 0x213a7b5f,
    0xd46be8bf, 0x61073e4a, 0xfdb6d594, 0xbe4e4d26, 0x050b3ece, 0x7232e1fe, 0xb86d97cc, 0xaf2533ed,
    0x0e533ee8, 0x1a6710ed, 0xc71e06c2, 0xa66f038b, 0x47cb5111, 0xb0ca2da6, 0xe2ef79a3, 0x82d1a638,
    0x998e4cb0, 0x28f056f3, 0x807ddcb9, 0xc29eb5c7, 0x879d1685, 0x150d32df, 0xed829657, 0xddac4835,
    0x9e77a58f, 0x6852e686, 0xdb7f75d3, 0x54479a1d, 0xbaae6e35, 0x4a3ca0d8, 0x96fd9d71, 0x2ebccc1f,
    0x5ab888bb, 0x3f238258, 0x17115bbd, 0x0d015921, 0x82e8ea80, 0x30ea145a, 0xaf24229e, 0x8246cc79,
    0x032b0946, 0x5314c5be, 0xaa22e495, 0x5a4eaf1e, 0xb22c988b, 0x85a954f3, 0x1be8cf0d, 0x11959ea7,
    0xef31a9e7, 0x0346647b, 0xd1aef311, 0xdf21c409, 0xa995b255, 0xf971c17a, 0x0909d3bd, 0xc373b4d3,
    0xbef5cbc5, 0x645e734a, 0x35622288, 0x9b3593af, 0xe5f4624d, 0x3d4c6e44, 0x570b0b0d, 0xc3d0aea6,
    0xbc2843ed, 0x22e92d77, 0x2018735e, 0xd0572ca1, 0xd61bfe57, 0x12bc0bfd, 0xbae357b1, 0xe719592c,
    0x032c41f7, 0x770dff65, 0xfb7dbdbc, 0xf3ba98d5, 0x3171a7a6, 0xfe36e306, 0x288fae94, 0xa0ee5d7c,
    0xffcd3562, 0x24a51058, 0xb3509877, 0xc1e57e25, 0x589f33e4, 0x0731a473, 0xff9cd5da, 0xa35cd20e,
    0x3b72fa17, 0x64cf31f2, 0xf12b9c1c, 0x1aa55c81, 0x33ee2f30, 0xfefaa582, 0xa315f2fa, 0x96b1dea2,
    0x11b47d27, 0x3b04d48b, 0xaf97d643, 0xfb6bcb4f, 0x8a242d48, 0x6ba8ac2f, 0x85fa4620, 0x7d86d06a,
    0xdd9bbb10, 0x52f59ca4, 0x4367a52f, 0x4d25e8eb, 0xb99e4d0c, 0x1f459a64, 0x1b00d3c4, 0xb50470db,
    0xddf54a16, 0x95df6abf, 0xbdae0ee0, 0x23626aca, 0x5c496864, 0xcaf23adb, 0x8feeabc3, 0x3e68f8a6,
    0x354c8b4e, 0xad953384, 0x657ce5d2, 0x33020ec5, 0x94745f08, 0xa9f24817, 0x54120328, 0xc278ef4d,
    0x6bded916, 0x398a241f, 0xa08d11e9, 0xeb76be84, 0xed2b5c3e, 0x19e1edd2, 0x0b8b1dfa, 0x288352f0,
    0x6e630d1f, 0xb3a843c1, 0x82cd68af, 0xde425512, 0x09d9a412, 0xcb81689e, 0xd6b8ca2c, 0xb1c41973,
    0xf6cb61cd, 0xf0212524, 0x4f22fc1b, 0xad963dbd, 0xf84dde78, 0x109120ad, 0xcc23f640, 0x43d36af6,
    0xfec7acb7, 0xefeb85f9, 0x738c690b, 0x10589e59, 0x8193e7d2, 0xb440dd28, 0xacf89966, 0xc9d908c7,
    0x98d914f7, 0x1057e561, 0x42470569, 0x38b38873, 0x1f7f1194, 0xc6715675, 0x1bcadd51, 0x6aca6a7f,
    0x96708ec4, 0xd0e3caf9, 0xffbec979, 0xf7889d69, 0xd6c5a996, 0x6d6ed39a, 0x8a7489e2, 0x263c2ec0,
    0x8d45dab0, 0x448b547d, 0xacaf3b4d, 0xfc468a8e, 0x19ad2d4c, 0xfe202b64, 0x7bfbcfb5, 0xdfcd9f4c,
    0x8298a580, 0xa83beb6f, 0x9b66834f, 0x7232aef3, 0xae209cee, 0x293386ac, 0x1c4332c3, 0x13ad1e85,
    0xd12a8792, 0x5c2f1a16, 0x6f9b2c41, 0xef020337, 0xe9a35d19, 0xbd33b55c, 0xc0fd7844, 0xac46e574,
    0xe3247f13, 0x50079794, 0xaf498e05, 0xe3241bac, 0x24a392bf, 0xe1786736, 0x5bb00d6c, 0x476d8f42,
    0xf1688f8a, 0x5a4374c4, 0x1c1f0ddc, 0x0d51bacc, 0x67584724, 0x830176da, 0x99da7b88, 0x08cc8a17,
    0x0764e5ba, 0x77a337b6, 0xf8ba292e, 0xdac7e6e1, 0x559df8d7, 0xb59c3579, 0x3c1d2a77, 0x3105999f,
    0x696cf34a, 0xd7032e72, 0x2b41d2fb, 0xe6f64b64, 0x67480bcf, 0x1f3dfd0c, 0xc16cdfab, 0xe4dd1b04,
    0xe259c821, 0x584f6b8f, 0xccc1b84c, 0xc7420239, 0x28eb86de, 0xbe56bb55, 0x5400cefa, 0x21736091,
    0xebb7611b, 0x68e2a203, 0xfbf40b74, 0x50d6bf95, 0x630c470f, 0x6e8257b7, 0x5717c28e, 0x7732f243,
    0x86894cbd, 0x6f8c4028, 0x260d7058, 0x1c78b2a4, 0xdfdaf15d, 0x1765cad0, 0x264bed4b, 0x89149842,
    0x8674658d, 0x1aba66b6, 0x3731fda2, 0xf1d216a3, 0xd5de948a, 0x07b91733, 0x80e814d5, 0x9a57fd46,
    0x54bc91ef, 0x05ee3b7e, 0xbe6032ae, 0xbdf61dd8, 0x9d07b9aa, 0xc6487e10, 0x40785b96, 0x549f643a,
    0x7d8a600e, 0xbb8ed47f, 0xa655c7ed, 0xaf24583d, 0x12a4dcec, 0xfd088516, 0x2df42814, 0x81daeb0a,
    0x7b86febc, 0x0ec1cd14, 0x4fe2e674, 0x64b2c5b1, 0xde9436c9, 0x62842f7e, 0xc3b86e0c, 0x157b682d,
    0xd9e27be7, 0xb88c12c2, 0xe1d35046, 0xf021ebb2, 0xef244ce0, 0xfca67165, 0xa13e85bc, 0xab89d21f,
    0x2b9398c9, 0xdb2600af, 0x1591cca8, 0x297223d7, 0xf49164a9, 0xd3fc6628, 0xa41c4b3c, 0x30bdbf17,
    0xb0f940ee, 0x84c912bc, 0x8a5a1d95, 0x4e7b9166, 0x0e948060, 0x7e184f21, 0x27470d5b, 0xa2f84e6e,
    0x42219840, 0x18e61d05, 0x956af162, 0x3355a400, 0x6e1518a0, 0xc477e60e, 0xfc7726ba, 0xa3fe1a4c,
    0xb390026b, 0x303808a9, 0xa35421ff, 0xefd6af79, 0xce834250, 0x592c5a77, 0x61e1cc42, 0x10a8d192,
    0x7f082baf, 0x6b3415fc, 0x8b095d28, 0xe0095c9c, 0x8d1f420e, 0xcfc91203, 0xc6b02c79, 0x80606b24,
    0x0e97c2fd, 0x06f50c67, 0xace352f8, 0x021dc0f7, 0xf0771b6b, 0x2f1fac23, 0x3e7e7d5f, 0x78950d8e,
    0x44ef03c8, 0xb6b46242, 0x639c28a8, 0x6208066b, 0xf89d0244, 0x3a2746a0, 0xea540b70, 0x087fc54b,
    0x012a8ca1, 0x2574f7c3, 0xa6ffd6a6, 0x7b248655, 0xa917ee31, 0x86b04ff7, 0xe0c70284, 0x21c9ef13,
    0xe30eb16b, 0xda7c732c, 0xc0153c91, 0x80faca02, 0xda3761e1, 0xe4e09af2, 0x57519f68, 0x4365905c,
    0x868dbe03, 0x95d3a678, 0xbdfd4f65, 0x30079760, 0x564ae419, 0xf7fd5ca3, 0x02a83afc, 0x4ab0e650,
    0x10ae9d67, 0xc2a06116, 0x061536d2, 0x2047f6c9, 0x46507c68, 0xbe3cc0aa, 0x400f0288, 0xefbca8d7,
    0xe1552298, 0x10c475a0, 0xa21eb1b3, 0xe8b1b7d6, 0xa559a0db, 0xa047c0c0, 0x8694e288, 0x45bbc0f9,
    0x90feefd4, 0xf9177519, 0x13dd5263, 0xa41bcc1a, 0x19081963, 0x8453cee6, 0xe7fc349a, 0x9fc1d16f,
    0xac2e7c29, 0x8dc3ade7, 0x07797f21, 0x8ebe1060, 0xe130466f, 0x3b5d2457, 0x33ea8286, 0x12857857,
    0x3300a945, 0x94ce7e9b, 0x134e5c91, 0x73c16a16, 0xccd2b96e, 0xd14ffb31, 0xdc8407e5, 0x8fba6afd,
    0x20addc72, 0x2c35472e, 0x202a7ed2, 0xf985fd45, 0xe5f4624d, 0x3d4c6e44, 0x570b0b0d, 0xc3d0aea6,
    0xdc634ae7, 0x74994e97, 0xb72660b5, 0xc3ad116e, 0x85e83a20, 0xf4e9fdb2, 0x5c8d3b72, 0x68421ce2,
    0xc7335491, 0xba63ef6e, 0x010ebf69, 0xa956f9e3, 0x63ec4f68, 0x8bd73fa3, 0x96723713, 0xf0951a78,
    0x78ccf10b, 0x2d413eb5, 0x83130a1d, 0xb6da31e1, 0x589f33e4, 0x0731a473, 0xff9cd5da, 0xa35cd20e,
    0x3b72fa17, 0x64cf31f2, 0xf12b9c1c, 0x1aa55c81, 0x76746a1f, 0x7aa3964d, 0x5a0fe6f6, 0xf6527c6a,
    0x8e0b1130, 0x9ebad085, 0x95a9128a, 0xd9ad37f4, 0xbd941035, 0xa4b79f61, 0x84b1a892, 0x73debb23,
    0x67584724, 0x830176da, 0x99da7b88, 0x08cc8a17, 0x28a0bc43, 0xadb68a68, 0x308bf37f, 0x7051044c,
    0xe843fbcc, 0x2f0afde2, 0x7830b2b3, 0x54e534f9, 0x076b2617, 0x3083f958, 0x4554def2, 0x42953e89,
    0x7da2c3d1, 0x0c4a62ca, 0xcdd6f3ae, 0xde93142c, 0x3765b69e, 0xf4bc50e9, 0xe3cc36b4, 0xf4a1d399,
    0x6bfdd227, 0x435a4a71, 0x04a0072a, 0xaea49e84, 0x99a3ce4d, 0xfc96e5b2, 0x4f7e13ad, 0x74f83142,
    0x4eaa99c1, 0x9cad8a27, 0xc11362dc, 0x074106ea, 0x5c0fe21c, 0xcd0ae506, 0x17b3a186, 0x4f7ae26a,
    0x35f211dd, 0xee2e840c, 0xad685b22, 0xe53c3881, 0x6d693cac, 0xd9825a5f, 0x8004f999, 0xc33780b0,
    0xb7ab7741, 0x57d7017a, 0xe92410ac, 0x046c98da, 0xbf86d499, 0x5c60bbb8, 0xe7241f0d, 0x3a7db568,
    0x87c31777, 0xa878ed99, 0x4b0abe6d, 0x81285f4f, 0x5950012d, 0x5e7e170d, 0x7971c612, 0xe72779f9,
    0x38bc37b1, 0xd9936b41, 0xbca9c405, 0xb6aaaa86, 0x4e798848, 0xecce6195, 0x0ddaefbb, 0x518f33ca,
    0x1c46cffa, 0x2969ec90, 0x0b1c9e3c, 0x9bbcf176, 0x982cfbee, 0x4a3d6b3d, 0xe4b93b60, 0x7d159ba8,
    0x31fafdb2, 0x5a5b56b8, 0xd82bf4e3, 0xdf175228, 0x494c059d, 0xe8b0ecd6, 0x7d7c3ab8, 0x8c14a1be,
    0x6c7694f5, 0xe0a07838, 0x968ecc45, 0xf35ceedc, 0x4ea673ab, 0xa4e7c24a, 0xa72e562a, 0x29fd0200,
    0x7f3b3c54, 0x308d0147, 0x35ed1d08, 0x0b3c21fe, 0xd283244d, 0xe241820b, 0xcd6c10a1, 0x32ac790a,
    0x2fd15885, 0x2236eeff, 0x944efb2c, 0xd0336508, 0xc8eacc3f, 0xbee7c188, 0x06257d54, 0x05f743e7,
    0x26b17ee7, 0x430848e4, 0x3031abb2, 0xc55a10e4, 0x284676b0, 0x5706fb43, 0x3c158d4a, 0x3cc7bd26,
    0xbe253b81, 0xdb9765f5, 0x8935e0db, 0xd788d38d, 0x2a81cd13, 0xa08b61f0, 0xa92bbfd1, 0xa0199602,
    0x72af448d, 0x82264a51, 0x64dce396, 0xf385519e, 0x8cc23c6a, 0x35a2c7b0, 0xb1423f77, 0x4aa22fee,
    0xc87b548c, 0xb321e6da, 0xfcd33bc0, 0xb0b35066, 0x8612f3ba, 0xaf5d87a6, 0xb86db208, 0x42adfff7,
    0xc6dcb548, 0x8e8bb6ca, 0x3114d860, 0x5fe96e65, 0xe03cf356, 0x0cac4dcf, 0x80721581, 0x4497d165,
    0xb30bd935, 0x247f0450, 0xefed0a6b, 0x64959cb7, 0x522d4b35, 0x37d4fa5c, 0x7c5f3c34, 0x2f43ebb3,
    0x67cc5ea5, 0xbfe56486, 0x32982f1c, 0xec67c02e, 0x9f72d1b7, 0x86667259, 0x78078581, 0x8513bfa2,
    0x7715b2fa, 0xa55b4a4d, 0x756f7c73, 0xf3587334, 0x5bd18d70, 0x236bbe78, 0xd94d2c50, 0xfbdf94bb,
    0xe414d027, 0x9372de72, 0x73a980ca, 0x7778b860, 0x0d9905be, 0x6789c9d2, 0x065a3b13, 0xe2dbc8e1,
    0x510e3915, 0xce8bd3d3, 0x81b1c149, 0xff15039a, 0xc6e1742a, 0x86ae3d08, 0x28fc30ea, 0xa9043bc2,
    0x23d0e736, 0x9e6c7dd4, 0xe098ae0e, 0x16b35ba9, 0x826ca080, 0x119b599b, 0x0c093a60, 0xa35a8306,
    0x877d57e0, 0xdead5f51, 0x0aeaa7a8, 0xb2320012, 0x49edf869, 0x4a1fef59, 0x3bca1b8c, 0x0df155fb,
    0xf49164a9, 0xd3fc6628, 0xa41c4b3c, 0x30bdbf17, 0x25c408df, 0x011f521c, 0x3abcde20, 0x068f8b99,
    0x99248d9a, 0x140159c3, 0xdd525cbc, 0xefd378eb, 0x370ffe8b, 0x16bf25cc, 0x67f947e1, 0x23433100,
    0xabbf98b9, 0xe16e901e, 0xac501e61, 0x8defbabf, 0xdd1cc9f7, 0x0ce71659, 0x5623df66, 0xfeb13e6d,
    0x2a5ce8a2, 0xdb6c606d, 0xd167fcf2, 0x095a828e, 0x92b15740, 0xdd4fc937, 0xf6212978, 0xce411030,
    0xdbd92846, 0x342307d9, 0xc24229b7, 0xf3ff8b93, 0xe3b7f52c, 0x047e6f0d, 0x3c8a8d0f, 0x317e9ef1,
    0x20530793, 0xf0ea4881, 0x6b0f975f, 0x82caaee4, 0x758ad427, 0xe59786a3, 0x15b061f8, 0x59335f79,
    0xb95c49c4, 0xe42c6d04, 0x5cb6e4aa, 0x2ec1fece, 0x33f9f03a, 0x1d7da607, 0x6a4994db, 0x9a31cb69,
    0xb0d6242c, 0x7e560bfc, 0x1c109c4c, 0x6d2848fe, 0xe3cb73ac, 0x62a6449f, 0x96f9caf1, 0x249ee14e,
    0x89ba1b90, 0xf7d84adf, 0xa7ae79c5, 0xa4427496, 0xd3680e33, 0x268d5afd, 0xac9a7370, 0x3e6a4818,
    0x8d1c9aed, 0x94931dd9, 0xadb46c08, 0x366a3fff, 0x97546a1e, 0x7a8095ae, 0xd866a4ec, 0xf9c1b494,
    0x0d9905be, 0x6789c9d2, 0x065a3b13, 0xe2dbc8e1, 0x35856d18, 0xa1411aba, 0x798b640c, 0x9ba6dc97,
    0xed9effec, 0xebd3a471, 0x48885014, 0x07a9fcf4, 0x9ef51687, 0xa6973781, 0xb126e809, 0xf05191f6,
    0x826ca080, 0x119b599b, 0x0c093a60, 0xa35a8306, 0xca95329b, 0xab25fcc6, 0x96ebe296, 0xc426ce3f,
    0x67952144, 0x92f1efba, 0x27c31949, 0xf4fa1042, 0xb0d6242c, 0x7e560bfc, 0x1c109c4c, 0x6d2848fe,
    0xc9f8cac3, 0xef501c17, 0x894236dd, 0x50c05416, 0x41655042, 0xd5c88feb, 0xa156a0f6, 0x9d86c65e,
    0xd3b0d9b3, 0x1674e526, 0x02aff634, 0x75ac69de, 0x38f2ed67, 0x0158a2fa, 0x3fa0ee6d, 0x0eb4f02f,
    0x8eb3c0f7, 0x3d722394, 0xc4b7c178, 0x55a09986, 0xa0fe2fa8, 0x0baf8032, 0x4697a98e, 0xd0e8ef6d,
    0x0da6eb08, 0x8a1c3ad9, 0x525ff1ab, 0xe96e200e, 0x9ca3922d, 0x17a91a71, 0x54869743, 0x588941d2,
    0x796bc15a, 0x0696d8e7, 0x46932b06, 0x5b083fb3, 0x183b77ca, 0xa7c49848, 0xd053a4e1, 0xde5f06fe,
    0x8b3da802, 0x8abee13b, 0x8f35850d, 0x80024d8a, 0x027aeb5e, 0xbf72fb92, 0x26243087, 0x44dc898a,
    0x7e881743, 0x1de4fb8e, 0x326df621, 0x9565929e, 0x55f1b52f, 0xb3c37e5f, 0xf24d3fd5, 0x60d8c8b9,
    0x478e594c, 0x21c937fa, 0xa5cebfb9, 0x223c9c19, 0x3b9bb132, 0xfa5715b3, 0xd6d89046, 0x33330d45,
    0xd1c45a3a, 0x10054629, 0x6d238bf3, 0x1ed56f28, 0x555b9525, 0xe4be3a10, 0x9e3ec624, 0x1d6f01d6,
    0x4d7670c9, 0xdc811f6a, 0x1b561556, 0x00cb31ad, 0xa9a80c3a, 0xa0e008e2, 0xebb473d0, 0x51329936,
    0xbbe9f807, 0xe133be35, 0x95dee217, 0x06c0d4be, 0xf525b7a5, 0x6e494c85, 0x5893f44a, 0x8219728f,
    0xfcc076bb, 0xe52eee6c, 0x368d715d, 0x09d6da15, 0x8298a580, 0xa83beb6f, 0x9b66834f, 0x7232aef3,
    0xae209cee, 0x293386ac, 0x1c4332c3, 0x13ad1e85, 0x8a01eb59, 0x993d50f8, 0x0854978d, 0x7ffe02e5,
    0xdff7a0ad, 0x49f75279, 0xfb137108, 0x899d6b5d, 0xb0d6242c, 0x7e560bfc, 0x1c109c4c, 0x6d2848fe,
    0x9fa1e2ec, 0xdadf21c1, 0xcd0c3440, 0x500b9f29, 0x128decf4, 0xa0013892, 0xf27d3631, 0xb594eaf7,
    0xe2047213, 0x020cf790, 0x871815d7, 0x6ce1ffb9, 0x520476a5, 0xd5e98c15, 0x2f6ff244, 0x5a474a1d,
    0x36770c0b, 0x1f62c347, 0x5095d189, 0x1c351f65, 0x5631420c, 0x2435be82, 0x4a891e69, 0xc5463982,
    0x74f32e9b, 0x5c5b280a, 0xd7327505, 0xdbd6b3ab, 0x7cf1f82d, 0xafe3fb02, 0x8b14e54b, 0x02b636a2,
    0x44e79636, 0x6f9600d8, 0x2fac9288, 0xf0768cb5, 0x4d561ece, 0x43eed644, 0xd6c63127, 0x53f1f6f6,
    0x4d7f16fb, 0xb62153a1, 0x112d036e, 0x66ea4af7, 0x7faaa61d, 0x10c46fe8, 0x540612ed, 0xa5418b5e,
    0x908566d9, 0xb2108f74, 0x5b1c47a1, 0xf83a6a07, 0xf6cd2825, 0x4f414d09, 0x6ef9fdd9, 0xea16a6ad,
    0x0c674d1c, 0xa42d04b2, 0x4b1c5168, 0xc1fd4d75, 0xe05762bd, 0x57c2dc40, 0x02fa5694, 0x27ed82b1,
    0xa1070c2e, 0x3d2742a6, 0xc09d9cca, 0xeceb7891, 0x172ef893, 0x2a49f28e, 0xee227d37, 0x310db7b2,
    0x695ba748, 0x09432647, 0x4f07be37, 0xb0bbb18c, 0x7f341d58, 0xf162b6da, 0xeaec0f02, 0x93ad50b4,
    0xfa08b8f6, 0x01760cd3, 0x4acbc3cd, 0xf68a165c, 0x12e16f09, 0xd68d9dbc, 0x773cd781, 0xe5fbd826,
    0xe6a96500, 0x7fe28e00, 0x73fbe538, 0x29b23ee0, 0x3c1f60a0, 0x0f095786, 0x6bbea855, 0x7ddf342e,
    0xf22added, 0x3306c6e8, 0x728662db, 0x84f2399a, 0x61c206d0, 0x3debf03d, 0x3677abf4, 0xef2dcfb9,
    0x1ceee364, 0x4c61fd07, 0x653dfe0c, 0x727d91d9, 0xae24feac, 0xf516e1ca, 0x5f1f12d8, 0x504c51fa,
    0xfa08b8f6, 0x01760cd3, 0x4acbc3cd, 0xf68a165c, 0x739975ba, 0xf9733eff, 0x2d790474, 0xe8962056,
    0x275c9286, 0x37654be8, 0xded5aba1, 0xbff37ce5, 0x1fe0de43, 0x4dd03521, 0x2cebbc58, 0xf5a60797,
    0xc8eacc3f, 0xbee7c188, 0x06257d54, 0x05f743e7, 0xed7163cc, 0x080195e9, 0xd54d3dea, 0xbad4d297,
    0x0b0c9dd7, 0x4b3776d0, 0x2d2e6281, 0x470c94bf, 0xcb0c2121, 0xd5ad123f, 0x7ac500a1, 0xf92040b8,
    0x19a4b37f, 0xdbdb7704, 0x5c7fdaab, 0xe0f4134d, 0xc12bce0f, 0xf7ad0e60, 0x69290748, 0x6a427a6d,
    0x6042ce50, 0x8e01f790, 0xe6e3b410, 0xedc16591, 0xb5e50542, 0xcc4583bc, 0x614c9d9f, 0xd3414f72,
    0x21c96551, 0xc899e5c4, 0xf5742a26, 0x2b423bc8, 0x4d27b8cc, 0x5826af4c, 0xbb4bbec3, 0x48483c8e,
    0x8b3da802, 0x8abee13b, 0x8f35850d, 0x80024d8a, 0x3f71de72, 0x55936735, 0xe948776c, 0x930f5c66,
    0x7da9c88b, 0x987be139, 0x20e378dc, 0xc73f3686, 0xd165a5cc, 0x821a66ef, 0x1477bda1, 0x7eddf71f,
    0x0740ea06, 0xa403cc81, 0xf389e270, 0xd8599141, 0x7ff217b3, 0x18cc3f04, 0x7418e0bb, 0x74c3ace6,
    0x9c468438, 0x61e28e71, 0x75467a6b, 0x230fc85e, 0xc2089d75, 0xb8fa67ce, 0x56ce495e, 0xc5b73ae2,
    0x525e809b, 0xcaa7d8e7, 0xf2ab82a7, 0x5a66481b, 0xb759d2f8, 0xa92ea0fa, 0x70c914f5, 0x424af72f,
    0xfb77ca1a, 0x463a0cbc, 0x7dc01a34, 0x541d3355, 0xee8dfb42, 0x9ab0ccbe, 0xa9cc31b9, 0x1019245a,
    0x85b2bb4a, 0xd8d23bf9, 0xa5d6966e, 0x3184f952, 0x4d7f16fb, 0xb62153a1, 0x112d036e, 0x66ea4af7,
    0x7faaa61d, 0x10c46fe8, 0x540612ed, 0xa5418b5e, 0x55022b91, 0xf3184107, 0x6d7b19f1, 0x3d1f3d94,
    0xb1bcb330, 0xccc4b2d3, 0xa9f6cf09, 0xdf3f6ad1, 0xd056289c, 0x13c31036, 0x4dd3ac70, 0x8d3588d7,
    0x44d06efb, 0x6d115a95, 0xf68855b0, 0x3dadf106, 0x58d27263, 0xdfd8ef4c, 0xa1e72c91, 0x44430d51,
    0x0ba69976, 0xad7d4a35, 0x2e56f650, 0xab6d4849, 0x6be9433a, 0x466f9a46, 0x23e633e5, 0xd6a9ac9c,
    0x8e03b7dd, 0x8cbc538b, 0xf93b9c25, 0xd5ba9c13, 0x4c0e3941, 0x251fa721, 0x76307e5b, 0xb3d25acc,
    0x7e0cbfb1, 0x70bd0866, 0xd22f7ddc, 0x65747c8f, 0x34e05db7, 0xbe8ac91b, 0x5d85500b, 0x4b697ca6,
    0xc5640b3c, 0x4bef97cb, 0xc2a47896, 0x393ab5d9, 0x51028bb1, 0xe8054366, 0x8a6816e5, 0xf5d51e3b,
    0x7768fe4a, 0xc5cbd882, 0x2b89e57d, 0xfa29dbeb, 0x1a5b73e4, 0xc285e161, 0x91da5ad9, 0x26bb5998,
    0x8f96dfaa, 0xe7d792b8, 0x6f42d562, 0xdf3ea195, 0x0dcd052a, 0x6fbe4848, 0x13d42fd9, 0xdbfa9abe,
    0x7377ac52, 0x46d4d86f, 0xff31ca31, 0xbf318d7c, 0x230aff21, 0x441e53ef, 0xbbb627f1, 0xa674c20c,
    0xa643c001, 0x83d09d8a, 0x5ca50aec, 0xc6c16b55, 0xae454108, 0x675d1760, 0xf24096f0, 0x3c8ddd54,
    0xf464608f, 0x0c0f18f8, 0xa7281aad, 0xde8ca02c, 0x5d157f85, 0x21bdbfad, 0x92161ad2, 0xba229382,
    0x6622a200, 0x79b04ae5, 0xbecf9c47, 0x82f55126, 0xb52e53da, 0xa1c49ff4, 0xbbc54ef6, 0x85f2d186,
    0x00a53497, 0x4144f2f9, 0x41eb0e0b, 0x67b1a3c2, 0x10f227c9, 0x5886efd0, 0xfd5792c1, 0x6ebe889d,
    0x3819a983, 0x8f69af95, 0x39c2b261, 0xd2049b8e, 0xe96f053d, 0x9fb74968, 0x3e22997a, 0x2bf64f8b,
    0xbed5c130, 0x4b054f53, 0xe38338c2, 0x664eae62, 0x4a58ff81, 0x2f119a4c, 0xedae04a5, 0x9eb9a4a2,
    0x3cb007d1, 0xe4bbab0e, 0x57e5e9c8, 0x9598f96d, 0xb9dbb877, 0xae93d4aa, 0xd0447220, 0x3e5e9685,
    0x7e134f51, 0x91426f7c, 0x6d682511, 0xff114291, 0xb629cdd3, 0xd92caa9a, 0x378aef39, 0x478887e9,
    0x523d478c, 0x285af551, 0x3a722e78, 0x0ef6aec4, 0xd357802e, 0x46dc04ff, 0x17bcf32c, 0xf00be88f,
    0x174b56b2, 0xfdb719d1, 0x0974f32a, 0xb1467ba9, 0x94246dd8, 0x056b76bc, 0x632e25f4, 0x7d75fccb,
    0x2645de66, 0xfe7e0fb7, 0x1be1d510, 0x1155f866, 0xe0a8cce8, 0xe0c6e46a, 0xc0cff0cd, 0xc1f58963,
    0xa80d68e5, 0xaad072e6, 0x271981d9, 0x528e059e, 0x4eaad183, 0x8902dbcc, 0xa32813fa, 0x7c3819f8,
    0x51c9ff2e, 0x57bdff96, 0xf0361379, 0x2d4b123a, 0x97f55d0b, 0x3a3a24db, 0xa061b882, 0xb7596e81,
    0x859b3c4c, 0x25969e4c, 0x042b9e9c, 0x1699fadf, 0x506b649c, 0x2d39e1a4, 0x071fff94, 0xd05763b6,
    0x35257ac6, 0x7da918e3, 0xa14e986c, 0xb6c19cea, 0x7f75dcfc, 0xa379c75b, 0x28c536d0, 0x9c36f6c8,
    0x5a406472, 0xfa39e103, 0xc212abba, 0xe5f57d5d, 0xc52fb2ba, 0x2bdbcbf5, 0x36db23fa, 0x0698e03f,
    0x490d428f, 0x75a0b9c4, 0x9176402c, 0x468e1d86, 0xdede9203, 0x157fc5c2, 0xbd6ce0ab, 0xcd11cf6b,
    0xed2b5c3e, 0x19e1edd2, 0x0b8b1dfa, 0x288352f0, 0x6e630d1f, 0xb3a843c1, 0x82cd68af, 0xde425512,
    0x1c8ef83b, 0x68f47bfa, 0x3bc9e741, 0x9586de86, 0x6d17d437, 0x428f2157, 0xc7022fa5, 0x1d091555,
    0x9649c06a, 0xc43f2ad2, 0xa6e908eb, 0x4d2762e4, 0xacca8bd0, 0x3fc1a642, 0x4435b9ae, 0xb25710d0,
    0x38bc37b1, 0xd9936b41, 0xbca9c405, 0xb6aaaa86, 0x8ac82cc2, 0x27766a5e, 0xb687504d, 0xab2b3711,
    0xa531a75f, 0x59f81673, 0xe32ec87e, 0x62aa810b, 0x4d6ec2f1, 0xf1ef2736, 0xda0195e2, 0xc7549a7a,
    0xe2b541dc, 0xff4435c5, 0x69e747b0, 0x9372aeb8, 0x9dd2f50c, 0x09ad265f, 0x298733a0, 0x721d9ece,
    0x52e35043, 0xa89439fc, 0x2b66f50c, 0x3838b93f, 0x0b0c9dd7, 0x4b3776d0, 0x2d2e6281, 0x470c94bf,
    0x2732f901, 0x2d4ca228, 0x19e1d5ad, 0xc361b294, 0x71ed9f3a, 0xc8d0f3c6, 0x385fee12, 0xfde04bf0,
    0xa828ec60, 0x6ad161cb, 0x9dd96cc9, 0xeca339e2, 0x92c4663b, 0x92f27fbe, 0xa7c5b3d7, 0x16faafbe,
    0xe1719ef8, 0x6b1d93a5, 0xddff095a, 0x8c46ab7b, 0x9b6eef58, 0xac98ed74, 0x050cd567, 0xdc810548,
    0x0c674d1c, 0xa42d04b2, 0x4b1c5168, 0xc1fd4d75, 0x542cc5f4, 0x6df42fed, 0xef4199c1, 0x8ae6f030,
    0x43064fff, 0x41b08be3, 0xd88bf2d7, 0x2984a948, 0x62d0113f, 0xfbb89466, 0xd8308203, 0x7c102695,
    0xc6c06f52, 0x3657b46b, 0x19e43668, 0x1d529d42, 0xe61fdadd, 0x71806bfc, 0x4e89b026, 0xd6ab73fd,
    0x93f06c37, 0x8f47165a, 0x6bf90437, 0xdfd21e68, 0xfa6d69f8, 0x94cc6f8a, 0xf99ebbcf, 0x4fba1248,
    0x7e0cbfb1, 0x70bd0866, 0xd22f7ddc, 0x65747c8f, 0x3d7732e5, 0xf3f797fd, 0xcde47cae, 0xfd16cddd,
    0x6729898b, 0x0f961a5c, 0xfd424146, 0xbd3cb3bc, 0x139ba011, 0xa578f49d, 0x1b7ef5dd, 0x545e3be5,
    0x758ad427, 0xe59786a3, 0x15b061f8, 0x59335f79, 0x9cfbc1ae, 0xf54e12e5, 0xb9cbc1c4, 0xd8ca8951,
    0xe55de0fe, 0xe1101162, 0x5313c2a7, 0xdb4976f8, 0xf486af9b, 0x44f46700, 0xf78fb4e2, 0x93aee5af,
    0x396218f8, 0x379672a2, 0x73dc50cf, 0xaaf5bbcd, 0x44d06efb, 0x6d115a95, 0xf68855b0, 0x3dadf106,
    0x5e18b54a, 0x33b79214, 0x0ea9160e, 0x2971a6ff, 0x3b771c84, 0x4b684474, 0xe4efa146, 0xc2538299,
    0x1b73ed93, 0x67835b68, 0xa70999ea, 0xf69b4de7, 0xb53e8abb, 0xc21dca7a, 0xc6d7e30f, 0xf5b7214b,
    0xc902a270, 0xa70d01bf, 0x35113c36, 0x81b14154, 0xa1be201d, 0x8f08fd0e, 0x17b3df5e, 0x0b3440f0,
    0x0da78a3f, 0x2e37a82b, 0x6f7d1b85, 0xea952116, 0xc072b1c1, 0x19c14c43, 0x0947a67c, 0xc0739a6b,
    0x27dd985d, 0xc964aedb, 0xb0bef5ae, 0x3418549d, 0x6af94395, 0xe8144d75, 0x69a48962, 0xb47f66a4,
    0x48227de5, 0x753764d5, 0x839d1d6b, 0x87c01c50, 0x49d28dd0, 0x245503e1, 0x4b280ac6, 0xa4d932b6,
    0xf486af9b, 0x44f46700, 0xf78fb4e2, 0x93aee5af, 0xc04a5bfd, 0x6384ccb4, 0xba818422, 0x010c16c0,
    0x952c89ca, 0x7ced53e0, 0x3358db0b, 0x757a9c32, 0xcf2f1fd8, 0xc26b5fc4, 0x386366d8, 0xd1433ef4,
    0x7600c71c, 0x22aa60c2, 0x7a42b3ad, 0xdc9bdea3, 0x4ddccb2e, 0xcfcede9f, 0x69a5b21b, 0x064c2559,
    0x94b65446, 0x2cfab1ca, 0xf8a73b68, 0x88757031, 0xa62c4ea7, 0x5ffa7c92, 0x21de5736, 0xcb59e095,
    0x06fd7792, 0xe8ce0e5a, 0xff23d766, 0x2b5d6078, 0xebf0f42a, 0x14b03b39, 0x33108ad3, 0xbea4a843,
    0x6cd7ec28, 0xbeb35368, 0xddedd529, 0x12147a09, 0xc6dcb548, 0x8e8bb6ca, 0x3114d860, 0x5fe96e65,
    0xb720fce1, 0xccc002de, 0x70e1a962, 0x0e543fe6, 0x089e2bd4, 0xcf9361db, 0x5d87661c, 0x9cc4400f,
    0x5bb75bb2, 0x3f9e6db7, 0xbbd3dc73, 0x1fba4730, 0xdd2aee54, 0xaf09d9a5, 0x36dab30f, 0x099b45e3,
    0x18243ec4, 0x4139a61f, 0xc7a069b5, 0xd47bc49b, 0x9db7cc7d, 0x4e48b6b3, 0x3ab94ba1, 0x6df4e7c5,
    0xde4071a6, 0xd0b797aa, 0xc64e5437, 0xb22ed02b, 0x81960082, 0xca03ca17, 0xbfadff3a, 0x8fcaaac6,
    0x9790b2bc, 0x44f7f51e, 0x6ce40dcb, 0x1c6436cf, 0xe3d353e7, 0xb7c61070, 0x90b131dc, 0x780cffd9,
    0xbcc90cdf, 0x58980b60, 0x68ff807a, 0xbefd3e34, 0x34cd5ec6, 0x8e905eb4, 0x3c74dd16, 0x2882efc3,
    0x9fc70ae2, 0x15878017, 0xc1a1a2c8, 0xfd6b8999, 0xb9f848d2, 0xb7c4088e, 0xbf9a4451, 0x6252a2c6,
    0xf89ca5bb, 0xee6e60e6, 0x2917db8c, 0xce2efaab, 0x118c1c5e, 0xcbd7c44d, 0x3a25c64d, 0x7e383297,
    0x8905518f, 0xcd1c4af8, 0x39991f6e, 0x5a0b64ec, 0x50c05f6f, 0x1a42cb09, 0x3ee86f93, 0x0f1fc225,
    0x0321fad3, 0x28e4d4b4, 0x7e1a2a00, 0x2fc19e57, 0xa9072d24, 0xb5a77982, 0x62e00fc0, 0x34d42ac5,
    0x83664548, 0x3774ecaf, 0xd118d26f, 0xc726d78d, 0x61c6fd7e, 0x98abece0, 0x8e6975a6, 0x6c7e1c2f,
    0x7a3e9407, 0x61ee3e71, 0x84e7e216, 0x5961a03e, 0xc980b02f, 0x325bc785, 0xdeabb9e9, 0x7c27e7a8,
    0x947d965e, 0xcd7073d0, 0xfbba9116, 0xb7a62196, 0x1ae77512, 0x1de2fb40, 0x7944933e, 0xc08e4ee9,
    0xb2fae1fc, 0x336957e8, 0x87ac09b5, 0x3cdf1333, 0x7a39fbc9, 0x5920384f, 0x01e77214, 0x252f4115,
    0x1e540e86, 0x9601d3ba, 0x1c45dbd8, 0x09cf3585, 0xd6aa9efe, 0x40351258, 0x2b81835c, 0x6082f080,
    0x3b3affa8, 0xd6900c09, 0x53336feb, 0xb6217840, 0x8905518f, 0xcd1c4af8, 0x39991f6e, 0x5a0b64ec,
    0x31e2e5e3, 0xb0a6808b, 0xa8d06ab2, 0x0d40e654, 0x149d890c, 0x1dc7e950, 0x75eba56d, 0xe1499a6c,
    0xb4afa446, 0xeee3c607, 0x31c578bb, 0x1e7e19e8, 0x6b2d0696, 0x423295b3, 0x27b7f8ed, 0xd52ca3d7,
    0x6fe92164, 0x4fcddb59, 0xace49489, 0xc6eaba5e, 0x37595cc2, 0x902a35c6, 0xe271d74d, 0x051e6aae,
    0x70424b92, 0xf2d19f69, 0x05b0b70d, 0xc39e7092, 0x7447d7fc, 0x967b27b5, 0x2a404a4c, 0x40387954,
    0xb8ab181e, 0x5ce589d1, 0xc0820cc9, 0x2f95e4ef, 0x08304b94, 0xd824d122, 0xa8e01c36, 0x63cce1aa,
    0x0a9f0f10, 0x97506ffc, 0x8bd44516, 0x0e77b0a2, 0x6fab8a02, 0xdfcad154, 0xb0dd31a3, 0xb78720c5,
    0xb586b508, 0x02f6d92f, 0x4188b2aa, 0x0a2ebbfa, 0xf76ab878, 0xd58d1bf3, 0x930c7011, 0xb95189d1,
    0x40077f92, 0xb434712e, 0x63bcfad6, 0xb86177a2, 0x502b60d2, 0xa1d18853, 0x356f7862, 0x56c84612,
    0xdbb0e0c7, 0x9994907d, 0xa78a2b5c, 0xc78cf663, 0xf1226c7b, 0x1fd44d01, 0xa6d87323, 0x40fb96fd,
    0x8c9f3793, 0x7eff7181, 0xf4ff8ffc, 0x7c1bd554, 0xf76ab878, 0xd58d1bf3, 0x930c7011, 0xb95189d1,
    0x217adc83, 0x0c8c5ae1, 0xb5437ff5, 0x97f60ab8, 0x68fc6d49, 0xa2984c22, 0x153edd20, 0xb6d2705d,
    0x9aaf65ce, 0x9b7f49a1, 0x43a93b63, 0x5d9b3e02, 0x3ed560e1, 0x356594ed, 0x18717a99, 0x69c9e7ae,
    0x13d37454, 0xbeb308c8, 0x46cf88a0, 0x8a989450, 0x06de709c, 0xb2b1e176, 0x1f28ff62, 0xf05351bb,
    0x19081963, 0x8453cee6, 0xe7fc349a, 0x9fc1d16f, 0xb8ab181e, 0x5ce589d1, 0xc0820cc9, 0x2f95e4ef,
    0x389f6b35, 0x6d6e5478, 0xc1ec6869, 0xb1701aad, 0xb0d6242c, 0x7e560bfc, 0x1c109c4c, 0x6d2848fe,
    0x8361c7a2, 0x1577e396, 0x5235669c, 0xaeabb01c, 0xd46ac9e9, 0xffa739ec, 0x299b6054, 0x217a6a06,
    0xfafbc350, 0x98c6b784, 0xb3647ab4, 0xf54c3c4b, 0xfde536bf, 0xc3c8eab4, 0x05d310ce, 0xe9dfe140,
    0x80c7cf64, 0x76bd34d8, 0x690ce67f, 0x096bd5b7, 0x9cfbc1ae, 0xf54e12e5, 0xb9cbc1c4, 0xd8ca8951,
    0x2bb1ef2b, 0x36dab291, 0x087d60e0, 0xed09c1a0, 0x49e49fab, 0x22c9650d, 0x2b5b4c5c, 0x3c3f74da,
    0x6bb91929, 0x9886843f, 0x3d702f02, 0x197e8b50, 0xead98e80, 0xfb9b5905, 0x83f24a9f, 0x6984fb07,
    0x7a03bc77, 0x3aae1d36, 0x1cb23fae, 0x3432b800, 0x0d6bd23f, 0x44e13f8a, 0xc9f01e9a, 0x00862584,
    0xd9ac7152, 0x60edd368, 0xa267536c, 0xce291037, 0x58d1270a, 0x475f6968, 0x40f61c7c, 0xbec79be9,
    0x2333fc64, 0x07d8c373, 0x5ae7a118, 0x5c5b3513, 0x1e22f4e6, 0x3f74f92d, 0xb99f99db, 0x0430b6c1,
    0xbea2f81f, 0x2055bba8, 0x555e64ac, 0x5d331ef8, 0xb85613dd, 0x695ca02e, 0x98b39976, 0x445c650e,
    0x76004594, 0xdd6460d7, 0x203f5720, 0x11140181, 0x9145b8e9, 0xa01a94b6, 0xd8b4131c, 0x2d9bc97e,
    0xc41d8485, 0x6af25ee1, 0x845d19c4, 0xf302f661, 0x02effb02, 0x42201ad1, 0x2da3ba35, 0xdb1a242d,
    0x0774c90f, 0x1e96e648, 0x3b85a4f8, 0x6e94e49b, 0xff0959aa, 0xdce47f65, 0xd4fa6bdb, 0xb32c3c21,
    0x9e604045, 0x740064a1, 0xd2a949a7, 0x56bb0e53, 0xafd1b5e1, 0xdb180e6f, 0x35afc818, 0x20916e66,
    0xb04a687b, 0xab8b605c, 0xd1943711, 0x587b1e9c, 0xab1bb1a2, 0x0c11e4d7, 0xc031ddc2, 0xef01f226,
    0x551f9d5b, 0x25dbfeb6, 0x3704db92, 0x2d7efdb7, 0x2333fc64, 0x07d8c373, 0x5ae7a118, 0x5c5b3513,
    0x2bfbfe48, 0x1c0e7305, 0x4c3b7b3c, 0x061bec7d, 0x953ad351, 0x4ad020ab, 0xcacfb0c9, 0x39d721f6,
    0xcd666a82, 0x04d1946b, 0x8fb4dace, 0x4ecd74a5, 0x25fa5da9, 0x6944793e, 0xf73d1c20, 0xb0068f6f,
    0xba886547, 0x79cbadae, 0x85336ade, 0xc7bf0f09, 0x6b3b7e02, 0x3ea557da, 0x7af50da7, 0xd8c5d23c,
    0xc78135ef, 0x5aa3428c, 0xf5ed89d7, 0x657df800, 0x02733267, 0xcb6aabf7, 0xf6cb006b, 0xe46d26eb,
    0x471349be, 0x99d65349, 0x9ce19327, 0xe3a73a83, 0xaef1c327, 0xa8ecbdb1, 0x7cc671ba, 0xa090d124,
    0x2f183c0a, 0xc48f6959, 0x6b0d3c2f, 0xcfd96e85, 0xe5881de8, 0xb8d0f8ed, 0xe5b6073b, 0xf1494db9,
    0x961cdb18, 0x6a915308, 0x9851481e, 0xb01f3983, 0xcfe8210a, 0x566c3fd8, 0xa17d151f, 0xf2a85d44,
    0xf84dde78, 0x109120ad, 0xcc23f640, 0x43d36af6, 0x70e06840, 0x66bb4e99, 0x4d84e5e8, 0x52f3c9f3,
    0x49e23980, 0xceb36376, 0x1ce352cd, 0xad187f50, 0x7600c71c, 0x22aa60c2, 0x7a42b3ad, 0xdc9bdea3,
    0x93507ccb, 0xb5c883d3, 0x456e2b50, 0xab4bc693, 0x94b65446, 0x2cfab1ca, 0xf8a73b68, 0x88757031,
    0x3fc40d1f, 0x1db46d1b, 0x63e9c541, 0x9fc04fa3, 0x06fd7792, 0xe8ce0e5a, 0xff23d766, 0x2b5d6078,
    0x8327695b, 0x9b6009f4, 0x2c8b65ff, 0x2ec4f44c, 0x6cd7ec28, 0xbeb35368, 0xddedd529, 0x12147a09,
    0xc6dcb548, 0x8e8bb6ca, 0x3114d860, 0x5fe96e65, 0xb720fce1, 0xccc002de, 0x70e1a962, 0x0e543fe6,
    0xc7e9bdb0, 0xeb082099, 0x03842caa, 0x62b3c317, 0x53d116ba, 0x1f7a40a6, 0xc5277340, 0x9378fe20,
    0x1a0810ba, 0x8eb3f2d1, 0x5a35b0c2, 0x768c6340, 0xe17e98e6, 0xf73a00b9, 0x70acb749, 0xcb642750,
    0x7f22c685, 0xa427c78a, 0xaa63ea19, 0x6daf3ed7, 0xc67acf9e, 0x16c0d1b8, 0x0db23f0f, 0x323d5844,
    0x6154c4f3, 0x8e002b5e, 0x7e189d99, 0x49de8675, 0x6adeb846, 0x2b501691, 0x6257ce5e, 0x15101cdb,
    0x83101e2a, 0xe5f1bd37, 0xade721e0, 0x44d05054, 0x9a90b1f6, 0xfe4281d1, 0xcc5239d1, 0xa03d0482,
    0x21fe03e6, 0xd76009c0, 0x7d6801d2, 0x9ed8576b, 0x4f91cb0b, 0x0bb6b8d4, 0x37a0a4bd, 0x53ffb241,
    0xb09bb3c6, 0xf35a1dcd, 0x4fa90736, 0x15889f19, 0x4d57a42d, 0xa3656ecf, 0x77924b86, 0x93b87796,
    0xb90a8efe, 0x20839ffc, 0xeddab832, 0x0463f326, 0xb175de9d, 0x9695cd85, 0x3903911c, 0x64ceb041,
    0x53d116ba, 0x1f7a40a6, 0xc5277340, 0x9378fe20, 0x4b9f07e2, 0x167ba063, 0x2a68b912, 0x3edc1b6e,
    0x307f8af9, 0x97c86b80, 0x2eb79869, 0x80ceceb7, 0x9b3bb063, 0x7cf69ee2, 0x6fbfc121, 0xecdfd75c,
    0x1a833e62, 0x8400ee1f, 0xddcffb7c, 0xc280ba5c, 0xe1aefd66, 0xf09c3ea6, 0xfddf989b, 0x176b1501,
    0xfa2f869f, 0xd4e1fd67, 0xe0f4127b, 0xbb7ec892, 0x0d9905be, 0x6789c9d2, 0x065a3b13, 0xe2dbc8e1,
    0x9ac758c0, 0xcfa6d5e0, 0x1a628b86, 0x0c7108f4, 0x35becf18, 0x04cc9ca2, 0x7f1216d9, 0xf7cd0e90,
    0x9f9b7e3f, 0xb3b806b0, 0xc6fa27c6, 0xdfef533a, 0xe20725bc, 0x5378b3b1, 0x5783a8d8, 0x11e25150,
    0xe38aab87, 0xeac4c4e3, 0x7ac7deb5, 0x1fa78b5b, 0x236f7911, 0x5f9d6554, 0xed9d0a2d, 0x3fd8fd47,
    0x3bd15c1f, 0xe000baa5, 0xd626d6b7, 0x0b896682, 0x8dee1e5d, 0xd5a99fb3, 0xb5b08359, 0xff426b61,
    0x0a0e7f1c, 0x1e701370, 0x9fa30b24, 0x5f1fc960, 0x234ffb6e, 0x0c018ec2, 0x69f18efd, 0xd7856a40,
    0x7feccbf5, 0xffa55ea3, 0x491710d4, 0xd160199f, 0x61b4bf4c, 0x1c0b79e7, 0x2e31aee0, 0x3fd906fe,
    0x7fed6ebe, 0x854af9f7, 0x3e44c5c8, 0xa6579a41, 0x81f16455, 0x9cb2514d, 0x1f1d5db9, 0xc59eefd8,
    0xe97687cc, 0x90e0b630, 0xab4feb87, 0xd8bc986a, 0xdc266e3b, 0xdc808507, 0x2e14df8f, 0x4d2a2620,
    0xbb552fd0, 0xc466acd8, 0x7af145b8, 0xc3f35892, 0xfd2c487e, 0xddfbcfdd, 0x0b92e067, 0x1362e66f,
    0x8cf28257, 0x1f6f3fad, 0x2f7903ef, 0x3126bac9, 0x048594bb, 0x8c8815f8, 0xb8378f6c, 0x6b92dc87,
    0x35becf18, 0x04cc9ca2, 0x7f1216d9, 0xf7cd0e90, 0x9ab3d8b5, 0x629c06e7, 0x75e3a63d, 0x70bbefd1,
    0xf49164a9, 0xd3fc6628, 0xa41c4b3c, 0x30bdbf17, 0xa2bf371d, 0xe1a6fcef, 0x4236e0ef, 0xbc027a6c,
    0x911f280c, 0x7bdcd9b9, 0xf4bb811a, 0xdee4d50e, 0x6b2d0696, 0x423295b3, 0x27b7f8ed, 0xd52ca3d7,
    0x5ead145a, 0xde86a9e5, 0x13c96065, 0xaf0ef47a, 0x37595cc2, 0x902a35c6, 0xe271d74d, 0x051e6aae,
    0x70424b92, 0xf2d19f69, 0x05b0b70d, 0xc39e7092, 0xf0697327, 0x796b867e, 0x25837705, 0xc380e663,
    0xb8ab181e, 0x5ce589d1, 0xc0820cc9, 0x2f95e4ef, 0xf67acdc5, 0x24343e2c, 0xd2bd9ef5, 0x83d75656,
    0x90feefd4, 0xf9177519, 0x13dd5263, 0xa41bcc1a, 0x19081963, 0x8453cee6, 0xe7fc349a, 0x9fc1d16f,
    0xb8ab181e, 0x5ce589d1, 0xc0820cc9, 0x2f95e4ef, 0x7d63f9d8, 0x0a3e2817, 0x7796cd2d, 0x62aacb7a,
    0xb0d6242c, 0x7e560bfc, 0x1c109c4c, 0x6d2848fe, 0xfbc53a8e, 0x0a4145f1, 0x35232e53, 0xc0584d8e,
    0xf933c8fa, 0xb36b8655, 0xeff30122, 0x98cf0ccd, 0xbea2f81f, 0x2055bba8, 0x555e64ac, 0x5d331ef8,
    0x612880a0, 0x9a7e1a3c, 0x2a40bc63, 0xb683ce84, 0x76004594, 0xdd6460d7, 0x203f5720, 0x11140181,
    0xa9860807, 0x51828662, 0x277b50f9, 0xd0c2a28f, 0x7c89ba26, 0x523c7f1f, 0x16328007, 0xeec5e1d8,
    0x02effb02, 0x42201ad1, 0x2da3ba35, 0xdb1a242d, 0x11378b98, 0x03dbee51, 0xbb43ff84, 0x6e1006b3,
    0x9eede62c, 0x2d270718, 0x89b5b7c7, 0x9fdb4a94, 0xe7666ba9, 0xf01d0443, 0xfe15a027, 0x6bc7c12e,
    0x02effb02, 0x42201ad1, 0x2da3ba35, 0xdb1a242d, 0x185cfa34, 0x5a33c43b, 0xb1886ea1, 0x6ade8a05,
    0x0c674d1c, 0xa42d04b2, 0x4b1c5168, 0xc1fd4d75, 0x5e1580fd, 0x2598c6a0, 0xe4b00620, 0x59c47049,
    0xdc9461c9, 0xee24e44a, 0xfbfeacf4, 0x0209e955, 0x44d06efb, 0x6d115a95, 0xf68855b0, 0x3dadf106,
    0x322be5ae, 0xc868c9a7, 0xff63a478, 0xce0219f7, 0x3b771c84, 0x4b684474, 0xe4efa146, 0xc2538299,
    0x0eb547c5, 0x42f106ec, 0x2c68f891, 0x13e341bc, 0xb53e8abb, 0xc21dca7a, 0xc6d7e30f, 0xf5b7214b,
    0xc902a270, 0xa70d01bf, 0x35113c36, 0x81b14154, 0x8525d631, 0xde0002f8, 0x6bd82177, 0xfbea1533,
    0xed6c7760, 0x93fc28dc, 0x8d15f2c0, 0xd2703cc2, 0x4bedae4f, 0xe972bbfa, 0x7994b736, 0x662a45f0,
    0x61f54024, 0x4f5e6000, 0x4646c0b6, 0xdb66aacb, 0xe90d4403, 0x8e972ebd, 0x657169ff, 0xc79d5f46,
    0x58d67dc4, 0x16ad41a4, 0xf2393433, 0xce8e9d20, 0xe58d112f, 0x9bc9dd59, 0xad290017, 0xb2ad56a8,
    0x3779a8d4, 0xd45db7a8, 0x7312c77f, 0x9c546d79, 0xf65ee18f, 0xfcebaa35, 0x06ed6958, 0xc0d441ce,
    0x7e4f346f, 0x802de39c, 0x66d6571c, 0xbef10488, 0xdc266e3b, 0xdc808507, 0x2e14df8f, 0x4d2a2620,
    0x41adb32e, 0x424a34c5, 0xc3e08502, 0x0543ab49, 0xef672d16, 0xf0748086, 0xc1f0f626, 0xb301d839,
    0x3ed849a4, 0xe25e7832, 0x3cc66ca2, 0x8ceeea2b, 0x194c9fd5, 0x773ae09e, 0xe7499c14, 0xb5a8fa43,
    0x5643cd19, 0xf85c429c, 0xe9703f80, 0x9bf1ba4f, 0x7bee96a4, 0xc2635057, 0x98ec566a, 0x8d84d959,
    0x91286891, 0xa05a232c, 0x3cc27ffe, 0xda18356b, 0xd7ef3130, 0x74958bca, 0x2e5c0b77, 0xcd13df7e,
    0x9649c06a, 0xc43f2ad2, 0xa6e908eb, 0x4d2762e4, 0x3ab3d72c, 0x7cd96533, 0x6e527037, 0x51f0dd03,
    0x38bc37b1, 0xd9936b41, 0xbca9c405, 0xb6aaaa86, 0xc2dd8e52, 0xfbf94126, 0x4a610850, 0x3429604e,
    0x3e443614, 0x8da8fa3a, 0x77b381ca, 0x6cc7743d, 0xfddff3fd, 0x4a0596a2, 0x1fab8f18, 0x0634d503,
    0xc895dc3b, 0xd5f1f98c, 0x5021c547, 0x3c611f1d, 0x6d57f5a6, 0xc162ea5b, 0x91a95db4, 0xc6aeef48,
    0x7083f524, 0x8ee9435d, 0x372be340, 0xc6bbc796, 0xcc66d6e4, 0xc6cd2b9f, 0xebe7b664, 0xb1ab7021,
    0x750302f3, 0x5c339527, 0xbbb2d268, 0x2dc563af, 0x9d8a601d, 0x051b5a9b, 0x5845f390, 0x78b0ecb1,
    0x28eb86de, 0xbe56bb55, 0x5400cefa, 0x21736091, 0x0f46e26a, 0x99a5fdb3, 0x2e0e47b9, 0x27c05012,
    0xc346a116, 0xed7e7857, 0xbac38cec, 0xbb0736fc, 0x35740e86, 0x7163c5bf, 0x478104b4, 0x66d37d2a,
    0xb2fd387e, 0x887ee175, 0x9d015e73, 0x24b3a6d7, 0x4bfac603, 0xda4c5801, 0xb89e6541, 0x0ea7d41a,
    0xc6e7d777, 0x1a88127d, 0x43e3b0ff, 0x9dfef65d, 0x0c203886, 0x7aa1f906, 0xd13307ee, 0xfb95540a,
    0x61c6fd7e, 0x98abece0, 0x8e6975a6, 0x6c7e1c2f, 0x56387734, 0xfcccb8d8, 0x08527ff5, 0x9117fe59,
    0xc2061e75, 0xd0bcb385, 0x2123c4c7, 0x8d73e6dd, 0x7b11c7b5, 0xab0f6fc2, 0x4f7050ff, 0x6141da7b,
    0xbc9d7158, 0x4c6d327f, 0xa659a2b5, 0x0f063bd8, 0x2deacd07, 0x8f732451, 0x584e30bc, 0xebe4cc5c,
    0x334a64c9, 0x9a3ae494, 0xe8eaca2d, 0xd0ee0142, 0x67a4ee42, 0x0006104a, 0x8ba80bf7, 0x7b97da99,
    0xf9a78235, 0x5268e98d, 0x7e34dcf2, 0xaef3d241, 0x19a4b37f, 0xdbdb7704, 0x5c7fdaab, 0xe0f4134d,
    0x89549ada, 0xa3c058cb, 0x1088a3e7, 0x9595c5e9, 0x35740e86, 0x7163c5bf, 0x478104b4, 0x66d37d2a,
    0x33ff83a7, 0x46a2acc3, 0x9e1e8b54, 0xc7a39374, 0x4bfac603, 0xda4c5801, 0xb89e6541, 0x0ea7d41a,
    0x1e798895, 0x1daae257, 0xe53289cb, 0xf49f0d3b, 0x0c203886, 0x7aa1f906, 0xd13307ee, 0xfb95540a,
    0x61c6fd7e, 0x98abece0, 0x8e6975a6, 0x6c7e1c2f, 0xcb958d54, 0x092124cf, 0x5c671756, 0xeaa3875f,
    0xc2061e75, 0xd0bcb385, 0x2123c4c7, 0x8d73e6dd, 0x49b01e1e, 0xd2980bec, 0x38e0b462, 0x8485d584,
    0xc7a34042, 0x0f6a9494, 0x69df6cde, 0xbae15ddc, 0x76bdebbf, 0xe3b01005, 0x43f9da0b, 0x7be7a85d,
    0xe32f4723, 0x7148aa1d, 0x5b5b8d6a, 0x99e6fefa, 0x3873144e, 0x49cffc46, 0xe331581c, 0x2debec32,
    0x44dceea1, 0x22a81db2, 0x8ab4c3b0, 0x51b2a550, 0xdca0e90d, 0x4939acc5, 0x8f42010d, 0x66a01449,
    0x990f8b9a, 0xe161b567, 0x00fa7fc1, 0xae802050, 0x1ef9808b, 0x1a3d058e, 0x1157c08c, 0x4b9b99b8,
    0xc8eacc3f, 0xbee7c188, 0x06257d54, 0x05f743e7, 0x33953ce9, 0x2c279e3f, 0xc3010905, 0xd67a8802,
    0x60f50bf7, 0x736d501a, 0x28b124ea, 0x22abaeb6, 0x2e307107, 0x29ba1eb9, 0x582fe0f4, 0x513ea0c3,
    0x0c9eee39, 0xf80bfa1a, 0x0949036a, 0x528a3343, 0xf12790de, 0x8b16ea45, 0xc77d6750, 0x55ad8d9b,
    0x7f1a43c1, 0x6bf9b3aa, 0xef6a7a76, 0x3ac766de, 0xa02cafc1, 0x409fb52c, 0x22405fda, 0x86c1cdd8,
    0xeb12d464, 0x470dc1ac, 0xef66be6b, 0x17ff463c, 0x0b3d7424, 0x1c8f26ee, 0xbbf79a93, 0x092d645e,
    0x63a2da92, 0xebcfffe4, 0x67aeb64c, 0x48135ab9, 0xf9019c93, 0x40d3eef4, 0x808e44a0, 0x0cf7568b,
    0x9f2233b7, 0xf73f25e8, 0x3d4727df, 0x35b57e10, 0xdfa113a5, 0x74ab21aa, 0xe68f745b, 0x329be574,
    0x80012587, 0x9c7549c5, 0xff8866fb, 0xab7bd2f7, 0x0b31e0a7, 0x876d6f50, 0x8cd24196, 0x29c8e461,
    0x5179b121, 0x856ea451, 0xaab73d92, 0xcbc67b0b, 0x80b4ee75, 0xb8526803, 0x164b55b3, 0xcf8fa2bc,
    0xf5ad84aa, 0xd27025f6, 0x5cd7b968, 0x4ae82f35, 0xf01ca1d4, 0x89c8bace, 0x0c82ec7f, 0xce9fd913,
    0xba7c7601, 0x56958e97, 0x9d9e0a0f, 0x3e757bb5, 0x6ba41fe9, 0x9ae69767, 0x03a63791, 0x20ab249d,
    0x516dec87, 0xb0e7495e, 0x1aca0065, 0x4b9eeba8, 0x9c94de48, 0x82cbbde1, 0x93faf473, 0x21fbf144,
    0x30b580bc, 0xf501ba81, 0x30e159c2, 0x268e2634, 0x7be68597, 0x17a8aee8, 0x5b34353e, 0x6da94ee2,
    0x589f33e4, 0x0731a473, 0xff9cd5da, 0xa35cd20e, 0x53af2c39, 0x86369f4b, 0x60c46d72, 0x68cd77d0,
    0xf800e6e7, 0x9f43417d, 0xc6cfed0c, 0x8b121900, 0xa44f7dc5, 0xb5ab40fd, 0x84688e72, 0x53a2ac21,
    0xc600df56, 0xd93181da, 0x1a168340, 0xa0c1033e, 0x4bfac603, 0xda4c5801, 0xb89e6541, 0x0ea7d41a,
    0xd9d677f4, 0x57fa1d90, 0xff6e8c53, 0x6f254624, 0x0c203886, 0x7aa1f906, 0xd13307ee, 0xfb95540a,
    0x61c6fd7e, 0x98abece0, 0x8e6975a6, 0x6c7e1c2f, 0xc84f8f84, 0xc9a11a66, 0xb5226f3d, 0x43226b1f,
    0xc2061e75, 0xd0bcb385, 0x2123c4c7, 0x8d73e6dd, 0xa01de48e, 0x9e68243c, 0x7037ddfd, 0x20d4d9ee,
    0xc7a34042, 0x0f6a9494, 0x69df6cde, 0xbae15ddc, 0x76bdebbf, 0xe3b01005, 0x43f9da0b, 0x7be7a85d,
    0xda2c24b6, 0xfacfcd21, 0xe0d07b1c, 0x87696792, 0xed2b5c3e, 0x19e1edd2, 0x0b8b1dfa, 0x288352f0,
    0x6e630d1f, 0xb3a843c1, 0x82cd68af, 0xde425512, 0x1c8ef83b, 0x68f47bfa, 0x3bc9e741, 0x9586de86,
    0xdc7dcccd, 0xb5c9a767, 0x7a909112, 0x3605c572, 0x9b0c6ada, 0xce218b5a, 0xd44e59eb, 0xefa320f2,
    0xf282b160, 0xfb1fd057, 0x6d1aabfd, 0x30c6459a, 0x67584724, 0x830176da, 0x99da7b88, 0x08cc8a17,
    0x5e3a7728, 0x64a4a4f2, 0xcf31d688, 0x10bf6339, 0x59d0a185, 0xf23cf454, 0x10625977, 0x95a8b0ad,
    0x3ae266d2, 0x6916229e, 0x6561452e, 0xf5d52f71, 0x3dfe2d2e, 0x1c2f4526, 0xd6142151, 0x24e722e6,
    0x1005146f, 0x469fa399, 0x5e4c88c7, 0xa6005259, 0x98587c79, 0x8b615201, 0xa7510390, 0xd122b113,
    0xd87736a5, 0x58813637, 0xcb580ba4, 0x8a26aca5, 0x2692bf96, 0xc004fdf3, 0xb3829a37, 0x4726cad5,
    0x0ffadea8, 0x4ac56e2f, 0x79dd8dcf, 0xecf659a8, 0xa54d56f5, 0xce4bb229, 0xdcda697c, 0xb2eff915,
    0x8e3eaac4, 0xedf68cff, 0x939d2bb7, 0x49dcd6db, 0x8033b36c, 0x450c7744, 0x4326ac89, 0x33fd750c,
    0xa8d8ac40, 0x36896d9b, 0xa885e2d9, 0x72b77f2c, 0xbb28d53e, 0x7c3c907c, 0xe3ce4f17, 0xb234bab8,
    0x1ceee364, 0x4c61fd07, 0x653dfe0c, 0x727d91d9, 0xae24feac, 0xf516e1ca, 0x5f1f12d8, 0x504c51fa,
    0xfa08b8f6, 0x01760cd3, 0x4acbc3cd, 0xf68a165c, 0x3a8f0985, 0x6457b467, 0xb187bfa4, 0xfc1d7bcc,
    0x7ae4b7f5, 0x54cf7a5f, 0x9787a459, 0x543b432b, 0x9cdf67ea, 0x13ad9f40, 0xa566cc3c, 0xf35f56a9,
    0x778ce706, 0xd67b4c60, 0xd76574c8, 0x49aebfd5, 0xa122154b, 0x0eb5f48c, 0xbc3a0d76, 0xa4246760,
    0x128e40c7, 0x6cfae4cb, 0x58dbd7e3, 0xd0870c18, 0xb9a0be4d, 0x51ca4eee, 0xea13ef9c, 0xc247d3aa,
    0xb4fb76d8, 0x4f824b54, 0xc387e9ec, 0x08b97122, 0x134832a2, 0xc1daec60, 0x7b6a27c5, 0x5976939d,
    0x1c8ef83b, 0x68f47bfa, 0x3bc9e741, 0x9586de86, 0x064e4292, 0x90a1f16e, 0xdf48b048, 0x647ab05f,
    0xed2b5c3e, 0x19e1edd2, 0x0b8b1dfa, 0x288352f0, 0x6e630d1f, 0xb3a843c1, 0x82cd68af, 0xde425512,
    0x1c8ef83b, 0x68f47bfa, 0x3bc9e741, 0x9586de86, 0x9e5b2ce6, 0x3fec8818, 0x5bda2728, 0x1e917121,
    0x996730d9, 0xe3d9a0b9, 0xcb679f47, 0x23b9011f, 0xc62fb104, 0x8ea50615, 0xa4f080bf, 0x59f1146d,
    0xf84dde78, 0x109120ad, 0xcc23f640, 0x43d36af6, 0xbd041cac, 0x6bfb784e, 0xee888cc3, 0x5814d823,
    0x2d684c8f, 0xe2cb7ddc, 0xb09b1e81, 0xd8991e8d, 0xc5d298e9, 0xae52166c, 0x843776cf, 0x60ac91ad,
    0x165800de, 0x4db80926, 0x669918ed, 0x6c047d6c, 0xd4c8b9fa, 0xa8c5ba07, 0x1273a8db, 0x1930b32e,
    0xa9bdf55e, 0x170dc938, 0x77858f75, 0x574d6724, 0x184e0be8, 0x00f5f7a2, 0x7441c6b5, 0x8c6f3bb6,
    0x36004141, 0x4f3c2f48, 0x9e171f8a, 0x21ae4f5a, 0xc1f36d23, 0xe93faf40, 0x3649ef66, 0xe5ffe2e5,
    0xa9bdf55e, 0x170dc938, 0x77858f75, 0x574d6724, 0x0e3d837f, 0x6156d8ab, 0x64c3ce26, 0x56297126,
    0x9766396a, 0x560341de, 0xfde39424, 0x3703091e, 0x36f5291c, 0x8226b8ea, 0x8c6e7a3f, 0x9909e17e,
    0x02ad2824, 0xf22f3f75, 0x8b7e6718, 0x596ffe8c, 0x29c6d96f, 0xf4abfdda, 0x08ff9300, 0x3d04ed75,
    0x8fdba040, 0x12c0426d, 0x121f7a10, 0x0b8a31fb, 0xbc9d7158, 0x4c6d327f, 0xa659a2b5, 0x0f063bd8,
    0xd7745c90, 0x8f37e1fb, 0xa30cf119, 0x6325ffc4, 0x8813b354, 0xf45abcdb, 0xeff2af43, 0xdb0857a7,
    0x427ab931, 0x1049bf55, 0xca7a6b60, 0x2bbea88a, 0xd07f0b50, 0x7200d197, 0xcff92931, 0x83dbc3b2,
    0x6bc05623, 0x0b8eb935, 0x87e8b373, 0x1ecc8151, 0xcda07e93, 0xaf547c1a, 0x309de38d, 0xf3effa11,
    0x15c77190, 0xcfdb02ea, 0x26765c83, 0xcbc51a5e, 0x0fbba0fd, 0x24e027ea, 0xd19603fe, 0x98e00af7,
    0x8cb74c94, 0x89641bb8, 0xc51d3f2e, 0x1117593b, 0xdddce464, 0x14bdcfa7, 0x9ca7a78d, 0xb8fbc2fb,
    0x4cb27464, 0x376d40e5, 0x8ee45788, 0x35bdc393, 0x92453e24, 0xd0eab626, 0x9592161b, 0x707d54f6,
    0xed3930ab, 0x9512c1c6, 0x1a6c5e07, 0x47f72928, 0xc4408da7, 0x0947a277, 0xf16a28bc, 0xe213fc67,
    0x0de318ac, 0x72674a0d, 0xcaff8a0d, 0x0a0861fb, 0x19a6dee6, 0x34b65a93, 0x1758c4dd, 0x3ba12e4b,
    0x714ad343, 0x972b340b, 0x262079dc, 0x2edf9be7, 0xfb02bed6, 0x5ccda504, 0xd65be572, 0xf75ebb75,
    0xda31611b, 0xdcb4e0b1, 0x3afa59c5, 0xd0ce82dd, 0xa1c33488, 0xf2290c63, 0xb73ebaad, 0x5ffa1400,
    0x22c70c61, 0x5649341f, 0x4cb6d07f, 0x4d92c522, 0x83b80406, 0xc9ad199d, 0xfc7b6608, 0xeaa8ff08,
    0x36004141, 0x4f3c2f48, 0x9e171f8a, 0x21ae4f5a, 0xc1f36d23, 0xe93faf40, 0x3649ef66, 0xe5ffe2e5,
    0xea684f38, 0xaacd3ccb, 0xada90927, 0x84582659, 0x6dc8f241, 0x01d2105c, 0xb82c825a, 0xa9b83cbb,
    0xf84dde78, 0x109120ad, 0xcc23f640, 0x43d36af6, 0xa8029a55, 0x63793988, 0xd6cfcbc5, 0x98d3f0cd,
    0x2295ee58, 0xf372aefa, 0x3496a2e2, 0x10aa60b8, 0x814bd331, 0x16f99646, 0x29a7dfc2, 0x145ed632,
    0xc45f1bf9, 0x20e1ebda, 0x14e710d8, 0xe9c65507, 0x392f8bfc, 0xb8aa7d2b, 0x34d166b3, 0x1cbd8827,
    0x3f232985, 0xd76f2943, 0xbc6afe25, 0xbdda44b0, 0x8de72ccc, 0x5aa5fc5e, 0xf7055cec, 0x6ad9eb53,
    0x0dd7fca0, 0x00cd0309, 0x504ebc1c, 0x73773f1e, 0x69ffc3d0, 0xe6f32c13, 0xf912b4e6, 0x56faae7d,
    0x07062f54, 0xba56a292, 0xc6bf3eb2, 0x18965b69, 0xccd5cacb, 0x5fdfa107, 0x4831ebc2, 0xa224d006,
    0x365a9b56, 0xbe7904b8, 0xe02495cc, 0xa23b0fdb, 0xfbdece50, 0xbdc5e75c, 0x72911b52, 0x446cd7a0,
    0x5cdf6c47, 0x97f80b85, 0x339576ba, 0x119571b3, 0xa18b37e2, 0x0ae96faa, 0xfbae0016, 0x997aa635,
    0x1e519f22, 0x0c0f8853, 0x45fef46b, 0xf6f783ea, 0x3183fdd9, 0x5baf832a, 0xe494758c, 0xae55f052,
    0xcd8b29dd, 0xd5a46eaf, 0x97acb594, 0x65ac0125, 0x70424b92, 0xf2d19f69, 0x05b0b70d, 0xc39e7092,
    0xdbe8bf3c, 0x07ae830d, 0x6c77cc33, 0x71e23665, 0x4a6c034a, 0xf1f1d242, 0xa203f2a9, 0xb146242c,
    0x6609b31b, 0x01a83f1a, 0xd7cd6bf1, 0xcdc05aac, 0x7bee96a4, 0xc2635057, 0x98ec566a, 0x8d84d959,
    0xc97e912f, 0xa87dbc76, 0x11ae0421, 0xdaab6e26, 0x5c1ffd75, 0xc61e6d40, 0x3eb03e6b, 0x58cb0db8,
    0xb0d6242c, 0x7e560bfc, 0x1c109c4c, 0x6d2848fe, 0x0f7bf9f3, 0x9506f22e, 0xefa61ce6, 0x0d876260,
    0xf0f15c51, 0x09bd4884, 0x29e2522f, 0x17759f02, 0x9d07b9aa, 0xc6487e10, 0x40785b96, 0x549f643a,
    0x0d2f4c6c, 0xc0e65855, 0x022b815c, 0x97a195f4, 0x0a87d0a7, 0x6e029978, 0xad0d66ad, 0xf45c4402,
    0x21a525c6, 0x1d0a89ca, 0x9241d25e, 0xf57d3183, 0x891a2db8, 0x5b344e59, 0x7f2387a0, 0xf6d49624,
    0xc6bb09ef, 0x5ba93f4d, 0x76f68e87, 0x830f4da7, 0xe74e682a, 0xa24d0c8f, 0xca9e6b6c, 0x0b29dcb0,
    0x2f183c0a, 0xc48f6959, 0x6b0d3c2f, 0xcfd96e85, 0xaa4e1bdf, 0xffbba8f3, 0x7ac46749, 0x8566071f,
    0x366cf451, 0x14fca048, 0xfa08224c, 0xfb161e7f, 0xcfaf4334, 0x43b8731d, 0x88dd6981, 0xd14b70ba,
    0xcc289ba3, 0x05eb9353, 0xfe988615, 0xfadd054c, 0x778ce706, 0xd67b4c60, 0xd76574c8, 0x49aebfd5,
    0xb14e5666, 0xf04c7237, 0xbb090055, 0x40650908, 0x2352151e, 0xbbf8efc7, 0xcd8b004b, 0x7098447b,
    0x9e247e3c, 0x58f36bc9, 0x2082ac80, 0xf2dc2f7e, 0x70424b92, 0xf2d19f69, 0x05b0b70d, 0xc39e7092,
    0xf48762ff, 0x57e34f19, 0xdcbddd63, 0xd4348910, 0xc80c9daa, 0x1773e9be, 0x4412d2c8, 0x46ea6063,
    0xcff90b44, 0x85b15c67, 0x2bc1562c, 0xd8b0790c, 0x1ceee364, 0x4c61fd07, 0x653dfe0c, 0x727d91d9,
    0xae24feac, 0xf516e1ca, 0x5f1f12d8, 0x504c51fa, 0x6c5f6817, 0x66dba6c9, 0x34d2f249, 0x5697d3d9,
    0x7becaff1, 0x20756c41, 0x2682b960, 0x302729ff, 0x0c674d1c, 0xa42d04b2, 0x4b1c5168, 0xc1fd4d75,
    0x770b5885, 0x6f6a2619, 0x3133a344, 0x9eea557c, 0xb5eec8e3, 0xe49bf441, 0x2bbe9182, 0xf29f0961,
    0x7afd3339, 0x679ac2f9, 0x8feb049b, 0xdec8fb4f, 0x5547892d, 0x3ca08c39, 0xebb6ce2f, 0x27a3f01f,
    0x1a819676, 0x81112bfa, 0x9a046a92, 0xe4be518b, 0x5eb3b751, 0x44f0276f, 0x3d462903, 0x31f3fcf1,
    0x3423f733, 0x185019a7, 0x0df24e33, 0x5f377f52, 0xd200b56d, 0xade2fca7, 0xbb3e53f0, 0x8bce8adb,
    0xb2fd1835, 0x4bfb71f4, 0x6f9969e8, 0x4c574f8b, 0x8a36b52b, 0x2698da91, 0xfd30518c, 0x0186c94c,
    0x46a2ecf5, 0x89aebb75, 0xcd8a6e7b, 0xd900ff28, 0x26c21264, 0x2b8f566f, 0x43cdb746, 0xedc88946,
    0xa4a97310, 0xa7179f33, 0x44c0d919, 0x271dc013, 0xf84dde78, 0x109120ad, 0xcc23f640, 0x43d36af6,
    0x41cd66ce, 0x2d995cee, 0x35430095, 0x6955c930, 0xf4e6384f, 0xe0c344ef, 0xece439fd, 0x8a8961d4,
    0x42219840, 0x18e61d05, 0x956af162, 0x3355a400, 0x5fd9e764, 0xf0c23fd6, 0x5502e3e3, 0xffbe898c,
    0xf0419a6a, 0xe13ba6a1, 0xac3de803, 0x9fd6971b, 0x9ea7b5aa, 0xb14ebb9e, 0x47f22ea9, 0xbe421dcd,
    0xe85b24b8, 0x0aab5938, 0x31aa2c91, 0xb4da5b73, 0xf9d2f2db, 0x3703f416, 0x552375a2, 0x324e5579,
    0x0e97c2fd, 0x06f50c67, 0xace352f8, 0x021dc0f7, 0xf0771b6b, 0x2f1fac23, 0x3e7e7d5f, 0x78950d8e,
    0x53fb7029, 0x5d1231df, 0x35db815e, 0x267f81c3, 0xb2f9f3e4, 0xfa1c366f, 0x3e5ee22f, 0x8a56af30,
    0x26f1a977, 0x4d8d0e93, 0xf4ed2f6a, 0xdcfd145e, 0x6b2d0696, 0x423295b3, 0x27b7f8ed, 0xd52ca3d7,
    0xaf1288ba, 0xb6eafae7, 0x2c2faa66, 0xafcda609, 0x200642ab, 0xbe69d1b7, 0x56ad549f, 0x2e03fda8,
    0x921d656d, 0x3114eeeb, 0xea0e1cfc, 0xff6f38f1, 0xd9ac7152, 0x60edd368, 0xa267536c, 0xce291037,
    0x7aa00bec, 0xa1742868, 0x3153fa41, 0x7fff0879, 0x606b3441, 0x9e00ec4c, 0xac21455e, 0x71d3db90,
    0xb08c22bb, 0xea59a776, 0x6eefeb9f, 0xabb37605, 0x77ef25e3, 0x1fecf8e3, 0xc6e678e3, 0x2a7f9fcf,
    0x3621bf94, 0x49296fb1, 0xfd532543, 0x91e2167c, 0xbe0cae6f, 0xa6a0322e, 0x4fff3bfd, 0xb148572f,
    0xe759df21, 0x5969085c, 0x6164d072, 0x48a64e4f, 0x8dad6fe0, 0xbf1849ca, 0x0c3b3130, 0xd8970b4c,
    0x738aa6e7, 0x5c4dfe49, 0xdc6e7bb4, 0xd8611c07, 0xd70bef7d, 0x5522dfa3, 0x9343ea8e, 0x8be19c0b,
    0x0c08dd24, 0xc8bcf500, 0x5fe1223e, 0x79b443b5, 0x60772015, 0x5af078ce, 0xf05b0326, 0xadc774e9,
    0x551a8d78, 0x8fe9f2e8, 0xdf8abd69, 0xb31ecc1b, 0xc6e10368, 0xf22c3650, 0x4ad8e02a, 0x0c54c7be,
    0x86a7335e, 0xe41ca35a, 0xc3fc121e, 0xe7e9723f, 0x417a56a0, 0x55d02cb9, 0xb953da70, 0x0f55ebff,
    0x3879105b, 0x78f0759e, 0x59cb0d5f, 0xec823843, 0x508f7851, 0x1ab9bc5e, 0x5faf0f37, 0x463809d0,
    0x77ec1340, 0xd758ac7c, 0x4505405c, 0xe3eb37a7, 0x3bf7a540, 0x68036684, 0xc23ec688, 0xb80b493a,
    0x910361f2, 0xbb95e238, 0x02693e02, 0xbef2178a, 0x5248997b, 0x66a99bc0, 0xee0b22f0, 0x51dd51e5,
    0x41313268, 0xc157991b, 0x687613b5, 0x0e484307, 0x6eabc957, 0x92b7237a, 0x3be66bef, 0x7ac02c02,
    0x1c1a2022, 0x1be7e8c7, 0x523b848f, 0xe76c6929, 0x6d99d1bd, 0xa57e5887, 0x0c0fd7c4, 0xa224ad64,
    0x2821e7c0, 0x8d6fd315, 0x56edca30, 0x8e2043f3, 0xd510ecc9, 0x6631a4f2, 0xb01b8031, 0x374200ab,
    0x4873e198, 0x74d880be, 0x2a97bd09, 0x5e36b2ec, 0xd85a41a9, 0x29d95f38, 0x6cc021f4, 0x27bbe716,
    0xb1316db2, 0xa88f8bc9, 0x1b33589c, 0x50b35b07, 0x67f61c0b, 0xe16026a7, 0x0575bd7f, 0x2f57628c,
    0xe43c6af5, 0x60a1b246, 0x3c345b9d, 0x0eb584d7, 0xe7aa74c4, 0x49a6edeb, 0x24392c6a, 0x27d70b0d,
    0x6962d9ad, 0x8f30ce62, 0x57b5c05a, 0xc793dd88, 0x1096e549, 0xf991f86d, 0x5cc53e0b, 0xf34b7880,
    0xd71caac1, 0x8d242858, 0x24875a7a, 0x76dac29f, 0x869890b9, 0x9445fcaf, 0x0a840c3f, 0x387e264e,
    0x4d8ecfdf, 0x602aa71a, 0xa6f3bd1f, 0x84979006, 0x572b8007, 0x5db21f98, 0x3440ead6, 0x610ce276,
    0x99d06c9d, 0x83de63cb, 0x16f78bcf, 0x536153c4, 0xd8160ad8, 0x17681d77, 0x90bd61e5, 0x6d19933c,
    0xcdb482ca, 0x4857c372, 0xf6a91e48, 0xdc466c76, 0x6d7054b2, 0x80230fd8, 0xc05c6ad5, 0x1608a878,
    0x3b98fafc, 0x28dd4ab8, 0x9fc194ac, 0x46b8d128, 0x06d6e53a, 0xd7d3e920, 0x880e1375, 0x828f2871,
    0x4cc12fb4, 0xe251391b, 0xd195f7c1, 0x92df9b28, 0x9aa8a738, 0x3a1cb9b4, 0xcc8ced94, 0x05f48817,
    0x01dec4f7, 0xa230311e, 0x33652dcd, 0xb077ca5b, 0x0c674d1c, 0xa42d04b2, 0x4b1c5168, 0xc1fd4d75,
    0xa86f2653, 0xd6d4e90d, 0xdc089aa4, 0xff1645cb, 0x3705127c, 0x7af785d9, 0x494d40cc, 0x97f0634a,
    0x47103e78, 0x66f61eef, 0x3de8cfe5, 0x6bec21b5, 0x0d7cb140, 0x8b0d86ec, 0x00c85c8a, 0x5e0fe146,
    0x91bef18b, 0x2a8626cf, 0xa5f88089, 0x10e7610c, 0x494c4470, 0x5500e4ed, 0xa9f72774, 0xda9b0f21,
    0xb2abbc6e, 0x3960260d, 0x5d646570, 0x1ec9d7d6, 0x0c53fc07, 0x1d5c8a0e, 0x23c081ca, 0xe7f5f03b,
    0x1b5a333c, 0x01bd01d1, 0x359f8b4c, 0xe7bb195f, 0xc6dcb548, 0x8e8bb6ca, 0x3114d860, 0x5fe96e65,
    0xe03cf356, 0x0cac4dcf, 0x80721581, 0x4497d165, 0x0bc81be0, 0xb6fdd082, 0xfc06b927, 0x188276ee,
    0xd849a506, 0xa18bd6be, 0xeaba74da, 0xfbbe48e7, 0xa5afc5c9, 0xa12f865e, 0xf5c62f41, 0x9b65b688,
    0xb060af89, 0x47f31a9f, 0x044d1cea, 0xb391ef4a, 0x54a60953, 0x4424e4f7, 0x68f807bd, 0x957eda1c,
    0x6b2d0696, 0x423295b3, 0x27b7f8ed, 0xd52ca3d7, 0x8f25cfd4, 0x5c914fee, 0x50006d77, 0x47d08495,
    0x47c90ac4, 0x1ff09366, 0x79d5e091, 0xe54da6b4, 0xebcbd28d, 0xb786dd91, 0x188468c8, 0xa4a2f222,
    0xac5875ef, 0x33c273f3, 0xeab2155b, 0xcd5b5aec, 0x2f05a3c9, 0x8ddbf889, 0xeef41f6e, 0x20da890a,
    0x5c0fe21c, 0xcd0ae506, 0x17b3a186, 0x4f7ae26a, 0x35f211dd, 0xee2e840c, 0xad685b22, 0xe53c3881,
    0xb159353a, 0xcdd8e5c6, 0xbe73fb86, 0x5c97da4e, 0x46f24599, 0xdd158ba8, 0xb433f274, 0xbfbd2f2c,
    0xc4ad50b2, 0x626f232c, 0x824e4729, 0x864c9883, 0x0b31e0a7, 0x876d6f50, 0x8cd24196, 0x29c8e461,
    0x779b6c7c, 0xadab4172, 0x211b0dca, 0x7230ad6d, 0x55dc9e66, 0xdaa32f61, 0x06f7ec4b, 0x48cb698b,
    0x60cf5946, 0xa2aa518e, 0x19af8697, 0x6391066f, 0x3384e290, 0xedd08ca7, 0x695249bb, 0x4e2b7bcc,
    0xe8fde3cd, 0xf7d06c17, 0x0f7476a2, 0xa399f185, 0xb40ec3cc, 0xbb47eb79, 0xf6046434, 0x1d1c3f95,
    0x10a94915, 0x40e3517b, 0x5aacaa54, 0x60892687, 0x7ae0d28c, 0x7b94e6e6, 0x53b7c633, 0xc7ca14b2,
    0x758ad427, 0xe59786a3, 0x15b061f8, 0x59335f79, 0xd04377f4, 0x01d900e4, 0x18a57e69, 0xc72cc699,
    0x8ebc642a, 0x854f4d53, 0xda6030a7, 0xa4c403fd, 0x0c674d1c, 0xa42d04b2, 0x4b1c5168, 0xc1fd4d75,
    0x557d7382, 0x6965eed2, 0x8985e677, 0x1d6a7dda, 0x84773c2d, 0x47d53b9e, 0xd3987c3e, 0x8cfddcee,
    0xccde8f8a, 0xc8150a82, 0x8b4d3a8c, 0x037dde03, 0xde4071a6, 0xd0b797aa, 0xc64e5437, 0xb22ed02b,
    0x8d47568b, 0xb09661de, 0x6a00cc72, 0x8d47f2c2, 0x582f590d, 0x29bf19c1, 0xc912d30e, 0xe45c82aa,
    0x1cc82077, 0xb5852bea, 0x3342bc31, 0x301628bb, 0xc3195adc, 0x1d9801e5, 0x2a487ab9, 0xb5fd73ab,
    0x9146975b, 0xcaae2e44, 0xfd4c474e, 0xff98cfb7, 0x2f183c0a, 0xc48f6959, 0x6b0d3c2f, 0xcfd96e85,
    0x65c33e0d, 0x126055b2, 0x3610d15d, 0x138e80d6, 0x180127df, 0x270caf9c, 0x0813a822, 0xdc48af84,
    0xf1318f8f, 0xcdaea486, 0xb4cfd183, 0xa3a51fea, 0x24d32cf4, 0x77f6659e, 0x6809f920, 0xddf8dc24,
    0x7600c71c, 0x22aa60c2, 0x7a42b3ad, 0xdc9bdea3, 0xad9e1ed1, 0x0920e9fa, 0x49d7fb72, 0x4a74324b,
    0x0c240f29, 0x713b27bd, 0xee40d758, 0x016641c6, 0xe741f2c7, 0xd14357b7, 0x0a36ec2a, 0xf5b5a475,
    0x73dfffba, 0x87ec069b, 0x8c73ad46, 0x575f19b1, 0xb95bbaae, 0x9c942a74, 0x2fd6505a, 0xc186b019,
    0xec5f71de, 0xf3216f91, 0x8d5bac50, 0x1ceb8176, 0x5f8e7e04, 0x512fb211, 0x66e84a23, 0x7cc532f4,
    0x5c0fe21c, 0xcd0ae506, 0x17b3a186, 0x4f7ae26a, 0x35f211dd, 0xee2e840c, 0xad685b22, 0xe53c3881,
    0xfd2a3818, 0xe9d97363, 0x8f9782bb, 0x48851852, 0xbf525968, 0x5ca76cd1, 0x0203f9ae, 0xa43a3234,
    0x04277202, 0x49a84fc6, 0x8d36b43c, 0x07c0fb7f, 0x4eb5bd32, 0xbdd5b635, 0x3f15d81f, 0x030eb4a7,
    0xae9c7e32, 0xa1a9f40d, 0x104d5077, 0xa8f74f80, 0x41f168cb, 0xbfea96b1, 0x6eb976f3, 0x9eb9e524,
    0x21a31308, 0x5534dd88, 0x2cbbca7b, 0x6becc062, 0x1ce51d2f, 0x3c9aaca4, 0x7c2b6d75, 0xa98a06ca,
    0x752243c9, 0x22317439, 0xddb2bc3e, 0x2c0e86e5, 0x143e3264, 0x8fc499e0, 0x1aca8e76, 0x115b9cb5,
    0x4d9877fe, 0xd2742e2e, 0x726be03a, 0x141e0cc1, 0xfb030017, 0x085ac2d1, 0x0eee98b5, 0xb090050c,
    0x0d6bd23f, 0x44e13f8a, 0xc9f01e9a, 0x00862584, 0xce9baf44, 0x6717ba4a, 0xd76902b1, 0xbd7fd55d,
    0x8931d007, 0x3a5114f9, 0x8b1fd4b6, 0xc8bb3fd8, 0xf49164a9, 0xd3fc6628, 0xa41c4b3c, 0x30bdbf17,
    0xd8c3794a, 0x9c527bc5, 0xeeb5bb1f, 0xa23b73a3, 0xef421f84, 0x8129fce7, 0x92ea3895, 0xfd02d7f5,
    0x12a5cde9, 0x88c16574, 0x8087a94e, 0xcad0557e, 0x1643154b, 0x18f6f38d, 0x17ca8604, 0x2d707534,
    0xbf2f062e, 0x3586389d, 0x0e4c45e9, 0xea05788f, 0x39002a32, 0x232a89a7, 0x43d02248, 0x474253e5,
    0x8862ea07, 0x18de69a8, 0x84851fc0, 0xc14590d0, 0xee903083, 0x4cef9e72, 0xf70e7aa2, 0xa2142402,
    0xfd21a57a, 0x5d92cc10, 0xbe03b3f4, 0xea3423c9, 0x8b3da802, 0x8abee13b, 0x8f35850d, 0x80024d8a,
    0x027aeb5e, 0xbf72fb92, 0x26243087, 0x44dc898a, 0x34e0057d, 0xe2fd1de9, 0x2bfe9e94, 0x604ed37a,
    0x6741543e, 0x953500f7, 0xeda7f0af, 0xc6bd59c5, 0x9b32d247, 0xc3ad3d61, 0x5a7da4ec, 0xe0a534e0,
    0x9ecf12aa, 0xaffc08e0, 0xdb992eec, 0x2ef46818, 0x1c43431c, 0x9a4cbc44, 0xc25d3d6e, 0x99790c9e,
    0xb06bbe52, 0x403ee656, 0x57ba02b6, 0x780a2433, 0x487155df, 0x70658e44, 0x6d732fa6, 0x0956f305,
    0x1f581d6d, 0x19a63848, 0xd4edc84a, 0x6d52bdcd, 0xa316ad90, 0xc8a93cda, 0x5b3e8a88, 0xca047e30,
    0x5729d216, 0x9b907a9f, 0xcd6424a3, 0x7643ce4e, 0xae9a13a1, 0xca9ef626, 0x8ec02774, 0x4c2c94f6,
    0x61aac0ea, 0xec88dbd7, 0xc1801f55, 0x54174039, 0xfe95cee3, 0xa1d38721, 0xda911c16, 0x812cfd13,
    0xfd9c6ff9, 0xdd7f6605, 0x2714d5fb, 0x057418e9, 0xb1e8ee59, 0x81c97d59, 0x0a1bfed7, 0x0809990f,
    0xf84dde78, 0x109120ad, 0xcc23f640, 0x43d36af6, 0xdf2d6821, 0xca92d40a, 0x52a6922a, 0x5c8d6068,
    0xd7f1706e, 0x2c350a6f, 0x379e9a08, 0xecd72f62, 0x80232ccb, 0xe77488b9, 0xff5c2406, 0x3fd2b49d,
    0xa3ffae75, 0x1aaa8746, 0x2aa117a9, 0x51fc79ed, 0x89639ecf, 0x59eec4e9, 0x382a894a, 0x01843675,
    0x8ef64cae, 0x526af088, 0x57d4c39b, 0xc2138643, 0xa4003780, 0x1dc40d9c, 0xb5f1bbbf, 0x4c086720,
    0x5b6c22e0, 0x45bc462c, 0x5df5ce3e, 0xef821e44, 0x22b05ec1, 0xf483b5fc, 0x39b5b61b, 0xe94822ce,
    0x6d0d7b39, 0x1ad036cd, 0x1c235520, 0xbdee8c25, 0x45671e4a, 0x6a91e961, 0xf87b1936, 0x9ff4f221,
    0xc069dfdf, 0x46c4aa9a, 0x2c81dfe0, 0x13d665d1, 0x1a8d3e99, 0x68dafcd1, 0xed4dc38b, 0xe52647f6,
    0x973ceb6c, 0x1ce67791, 0xe9ee8b1f, 0x3a092438, 0x34970696, 0x6c261217, 0x549c5143, 0x41b0363a,
    0xbf6d81fd, 0x523517a1, 0xb8fae854, 0xfb4f17c0, 0x68af14d9, 0xf1b79f82, 0x9e96114e, 0x94ff0408,
    0x5000408b, 0xbeabb3b9, 0x2b1947ae, 0x803ef529, 0x630b0801, 0x56dc10a0, 0x85d2f8da, 0x6034d1d9,
    0x495681db, 0x26b04140, 0xdfab4837, 0x7c8edabe, 0xb5f58b35, 0x6647f5d3, 0x0af58621, 0xf775dde0,
    0x75dfecb0, 0xcedcfca7, 0x54f0b786, 0xedadad65, 0xcd385ae2, 0x60ccea0c, 0x9e89c10f, 0xaf52a71b,
    0xa828ec60, 0x6ad161cb, 0x9dd96cc9, 0xeca339e2, 0x92c4663b, 0x92f27fbe, 0xa7c5b3d7, 0x16faafbe,
    0xf2328a53, 0xf554335b, 0xf1c92240, 0xe4e42d04, 0x5f628343, 0x3ea40f96, 0x521f95d7, 0xcdbe0eba,
    0x93407bda, 0x68947db3, 0x9ecefc0a, 0x66896824, 0x24e6982e, 0x02de6716, 0x05c7753b, 0xc76a66ae,
    0x31ff71ce, 0x70ec83fe, 0x1ab00a96, 0xaaff36d5, 0xcb9077dc, 0x5c88da97, 0xc20865e8, 0x513981d8,
    0x053be8b2, 0x8300b5b1, 0xd09eff98, 0xbe78e0ce, 0x5547892d, 0x3ca08c39, 0xebb6ce2f, 0x27a3f01f,
    0x00ca5de4, 0x8efb52f1, 0x874cc96b, 0xfcc62ba4, 0x30c91ac6, 0xe6a56df3, 0xc83ca3b0, 0x01e1ca55,
    0x43e17317, 0x55a74fd3, 0x471fda11, 0xe8e63a21, 0x0d6bd23f, 0x44e13f8a, 0xc9f01e9a, 0x00862584,
    0x8ca04a3f, 0x17d5e890, 0xdcdf0d11, 0xd8a6ef3b, 0xfa39f70a, 0xfc98950a, 0xc358c062, 0x5a75aafa,
    0xb0d6242c, 0x7e560bfc, 0x1c109c4c, 0x6d2848fe, 0x2c483479, 0xbd128fd9, 0xb7fc722e, 0x7aa16626,
    0xeb31e8a5, 0x1be21990, 0x720338ae, 0x45efd83d, 0xbea2f81f, 0x2055bba8, 0x555e64ac, 0x5d331ef8,
    0xbbb6fe7d, 0x190b00e6, 0x7ebc50fe, 0xf099ebb6, 0x35a78509, 0x15c7ba65, 0x93ad2864, 0xabd4e8f7,
    0xc0da76dd, 0x034ed90f, 0x344a3318, 0x6dbb8b67, 0x535b1a2f, 0xff6ac3ff, 0x793bc200, 0x88a0db9a,
    0x40df9a15, 0xa8834d2b, 0x31209910, 0x2335c7ca, 0xd087d5ef, 0x72d28cf2, 0xc5ffd164, 0x2f753672,
    0x048594bb, 0x8c8815f8, 0xb8378f6c, 0x6b92dc87, 0x2fe7f69a, 0x8903f17f, 0x4790e334, 0x876ab84b,
    0x7c8c3cdf, 0xb3377f65, 0x06659d0f, 0xd05ac2a5, 0xcdabd375, 0x64b42f8f, 0x9b08fb6a, 0x44d83255,
    0x7ff217b3, 0x18cc3f04, 0x7418e0bb, 0x74c3ace6, 0x1b3c4524, 0x869fbb06, 0xcfd1d6cc, 0xec6f1d06,
    0xb61210c0, 0x7d5ef155, 0xcca37d44, 0x94c1a03e, 0x12a5cde9, 0x88c16574, 0x8087a94e, 0xcad0557e,
    0x03f08e6b, 0xecfe0cde, 0x2a9118bd, 0x534d54c8, 0x9e8a06e4, 0x36d633a2, 0x26f63a9b, 0x9f29ab0a,
    0xeb49a197, 0x176bc6d0, 0x165ab062, 0x13866487, 0x5518cbe6, 0x84fc78b0, 0xb3d5fd6f, 0xbd0acdbf,
    0x6d0d7b39, 0x1ad036cd, 0x1c235520, 0xbdee8c25, 0x7543d9ab, 0xf427371e, 0x65934023, 0xcca241b2,
    0xe59c3a54, 0x9e8bdbb7, 0x92410b65, 0xbcd0d264, 0xf78608a5, 0x055a97d5, 0x77223cad, 0x213a7b5f,
    0xed0948f1, 0x8dfbc98d, 0x1a5c0b89, 0x390227be, 0x84f3d88e, 0x30a482b6, 0xe0d70859, 0xc993288f,
    0x1a833e62, 0x8400ee1f, 0xddcffb7c, 0xc280ba5c, 0x052d6b79, 0x41a0f75f, 0x786ff982, 0x6e3d1dac,
    0x49314aa1, 0xc5b3836b, 0x36f19e23, 0xa8901d1f, 0xf1a00fc8, 0xf0c366b5, 0xca77e94f, 0x915e1f74,
    0x3632b724, 0xd5a3e29d, 0x5504c489, 0xb56d081e, 0xa80495cf, 0x45582816, 0xdd73b273, 0x1a32ab25,
    0x6bc05623, 0x0b8eb935, 0x87e8b373, 0x1ecc8151, 0x8eaae465, 0x2426573c, 0x8f28a4aa, 0x08443004,
    0x45343aa5, 0x8b32f0f4, 0x2f1c7dd1, 0xf8ee0d3e, 0x2326fc2b, 0xd07a9cf2, 0x71968d74, 0x8f0de995,
    0xaa1de09b, 0xa19023b7, 0x5837b274, 0xc3719dcc, 0xa995b255, 0xf971c17a, 0x0909d3bd, 0xc373b4d3,
    0x782c10e6, 0x610bf6f8, 0x10cfa5ec, 0x4a494cce, 0xa95046e2, 0x35f559d0, 0x354e08d2, 0x6529ab7c,
    0xf7210484, 0x26e261e6, 0x7607b89c, 0xb856ad86, 0x593ae5bd, 0xe639a6fc, 0xb68b975d, 0x93cb6eeb,
    0x8d320637, 0xf9c2f761, 0x8fbfbb70, 0xfa46c6dd, 0x647b2723, 0xdb083081, 0x5007ca36, 0x95e0f1a9,
    0x6e132dce, 0x4b0012f7, 0x44be3c85, 0x3ee28743, 0xc6dcb548, 0x8e8bb6ca, 0x3114d860, 0x5fe96e65,
    0xe03cf356, 0x0cac4dcf, 0x80721581, 0x4497d165, 0x96b382c4, 0x02e4b5ed, 0xb1f8478d, 0x457c4fd6,
    0x11b47d27, 0x3b04d48b, 0xaf97d643, 0xfb6bcb4f, 0xdd7aa89b, 0x15d09ef6, 0x941f2347, 0x9a778443,
    0x89bae495, 0x30958f0a, 0x234d4c22, 0x4ec5d349, 0x1a833e62, 0x8400ee1f, 0xddcffb7c, 0xc280ba5c,
    0xb0f2f81a, 0x288d1075, 0x77178bfb, 0x4ccdf821, 0x200642ab, 0xbe69d1b7, 0x56ad549f, 0x2e03fda8,
    0x22463bc1, 0xa1a92042, 0xc9eb9674, 0x357d7b18, 0xd9ac7152, 0x60edd368, 0xa267536c, 0xce291037,
    0xd1126bee, 0x250c1e89, 0x9724dc20, 0xfa3f1dca, 0x8f43fd7d, 0xe3aeb4c5, 0x6989fa16, 0xf426c891,
    0x82dfaab1, 0x16a9f51b, 0xeb7399c1, 0xd581c593, 0x606bd45d, 0xd7822d68, 0x105d701d, 0xa9040672,
    0xc93f3dd6, 0x53f63b5f, 0xe722fdea, 0x22f0c078, 0x2093ad43, 0x0b5bdabf, 0x134e4f46, 0xf55c5713,
    0x7c4946d8, 0x93c67a13, 0x89f82c5b, 0x097b0778, 0xe23e5a13, 0x135c0c1d, 0xf827852d, 0xe95cac92,
    0x2c3a0e49, 0xea9290fc, 0x1be14919, 0xad398f9a, 0xed77f02a, 0x1c49236e, 0x3e5bf659, 0xb91e7f57,
    0x417956cc, 0xdcfa234c, 0x70830851, 0xe4b10c08, 0xc9afcf22, 0x270361ca, 0x0aa3b725, 0x12cf9c92,
    0xeb7d82d6, 0x8565387b, 0x2c10b4f3, 0x52987c50, 0xa4c81919, 0x66395fa2, 0x621d084e, 0x3e856048,
    0x4e81bf49, 0xe8899e97, 0x12352777, 0xb893a08b, 0x9e17b226, 0xb431cf20, 0xa7a6ea24, 0xb39b49b4,
    0x3d9acc14, 0xa273451c, 0xe654a3a3, 0xc4277a67, 0x5d5743cb, 0xb668b4f3, 0x0553b3ec, 0xf01e7faf,
    0x241da4f9, 0xee51aefc, 0xcb4fec63, 0xd4be9aba, 0x243257ee, 0xfda4578e, 0xb64ae812, 0xe68ed8e6,
    0x70cd3dcf, 0x16540a3a, 0x4c876f3e, 0x0fa173c5, 0x74178ca2, 0xfb6b2c15, 0xb16ce7ba, 0x8947464e,
    0xc31afa4a, 0xf2918034, 0xcb4e10c8, 0xe3d38728, 0x3a546e86, 0x43fd3b68, 0x15256fed, 0xac8a703d,
    0xd8d6f7fe, 0x8bce84fd, 0x40acf3d6, 0x3e2f81e5, 0xd92d7c65, 0x0ac94f83, 0xb99cb579, 0xce32501f,
    0xb4ea8555, 0x2577bb54, 0x622c8b99, 0xad492c94, 0x31e31420, 0x02c6fafc, 0x1468ab8c, 0x648d55d9,
    0xeb66fe0a, 0xb6722e32, 0xa35b1bb0, 0xd8b57a23, 0xcf99d91a, 0xb16158c5, 0x64080a41, 0xc9a379a7,
    0x25e28fb6, 0x438a2977, 0x55d00e5d, 0xd5e29fe8, 0x4257f4ab, 0x65245891, 0x61eddf7d, 0x470e45f3,
    0x71dec628, 0x2bf173d4, 0x53788d91, 0xf1329601, 0xc008d70c, 0xb583b95f, 0xaeecc2f9, 0x8e8e7a7f,
    0x36880f81, 0xf246ef7b, 0x75c2a3bd, 0xa9b8061b, 0x10963b9e, 0x73c17aba, 0xf9b19b94, 0xc9b8bea5,
    0x92d0e628, 0xe4dc91e5, 0x8551a02f, 0xd4d7baf3, 0xed2b5c3e, 0x19e1edd2, 0x0b8b1dfa, 0x288352f0,
    0x8d96c8f5, 0x077c0612, 0xf1168a23, 0x50c1f7c7, 0x737bb36d, 0x754ad054, 0x04080b7a, 0xbd7faf03,
    0x48aee077, 0xfd58c02c, 0x4711f93a, 0x81966c44, 0x8e47032f, 0x8108f454, 0xb17c5806, 0x3af18bfc,
    0x11e8a112, 0xe9ff1f92, 0xb30b8ca3, 0x5e37bce2, 0x3eafd6ab, 0x90b34761, 0xc814f5e4, 0x09464dd0,
    0x8298a580, 0xa83beb6f, 0x9b66834f, 0x7232aef3, 0x9391a786, 0xb63a28f7, 0xb5fc888a, 0x46e68e64,
    0x9b607031, 0x87f6076f, 0x2322595b, 0x6bdd51bc, 0x7879ea33, 0xcdab524d, 0x9abea1db, 0x887fe87c,
    0x6bc6fc1d, 0x803dd4fa, 0x3270b538, 0x52151e9f, 0x655dd749, 0x97940461, 0xd6ebd0b6, 0x71ed40ff,
    0x3aa56d88, 0xafd4302f, 0x08dfb883, 0x8eabd26c, 0x28eb86de, 0xbe56bb55, 0x5400cefa, 0x21736091,
    0xc293ce4d, 0x6cad2f10, 0xb8011a54, 0xe40376fc, 0xc0254b05, 0x25c88c9c, 0x2fca4c8c, 0x50becc12,
    0x2f5b7070, 0x3c447e02, 0x6d96791e, 0xa497ff43, 0xf08ff124, 0x18c84729, 0x7a0b6fdb, 0x5cba43ba,
    0xd2946549, 0x163e8813, 0xe7e8abc0, 0x06d2df62, 0x6af63280, 0xaaf5fd2d, 0xf1535bd4, 0xb5698c1c,
    0xd9e27be7, 0xb88c12c2, 0xe1d35046, 0xf021ebb2, 0x31bbb4a9, 0xa4156e88, 0x0254305c, 0x14001ec7,
    0x73d20b44, 0xbf30e58f, 0x22dcabe6, 0xd2b22506, 0x73cc3925, 0x3ae5370a, 0xcf8a2216, 0x09eca587,
    0xc2fd34bf, 0x58f7609b, 0xdb570f62, 0x1ce52d67, 0x2e024ca1, 0xe4db6e27, 0x38b77ae8, 0x5d4b5a83,
    0xed2b5c3e, 0x19e1edd2, 0x0b8b1dfa, 0x288352f0, 0xbbafcce5, 0xd6f2f30d, 0x08fd86e0, 0x148a8977,
    0x7853615a, 0x9911f711, 0x9ff13593, 0xa311e7e5, 0x6deaa954, 0x00ca5fcc, 0x6c19c770, 0x2bec7ac1,
    0x0bffaf63, 0x580db5d2, 0xefc0708b, 0x05601062, 0x77a157a9, 0xaa23c51d, 0x1775fbaa, 0x277426df,
    0x9b8cd68e, 0x97b87f2b, 0x27e2e168, 0xe6ce6676, 0x8298a580, 0xa83beb6f, 0x9b66834f, 0x7232aef3,
    0xb4933489, 0xd1276f35, 0x319a38e6, 0x4509d59e, 0xd1f6f883, 0x1e748117, 0xf57be146, 0x9dc49726,
    0x7a02fb3a, 0x9e450438, 0x04937169, 0xa5d8d120, 0x8364bf50, 0x60782796, 0x924e568d, 0xb6a08071,
    0x655dd749, 0x97940461, 0xd6ebd0b6, 0x71ed40ff, 0x5fe455ae, 0x85368f36, 0xd6fe481c, 0xdbf9bb1b,
    0x28eb86de, 0xbe56bb55, 0x5400cefa, 0x21736091, 0xe0864fa3, 0x827d09f2, 0xa554a91d, 0x93004623,
    0x19980c7c, 0xaa020884, 0xf502b312, 0x415693ff, 0xb62bdaaa, 0xc0790c4a, 0x830f6814, 0x81cd97a3,
    0xfbe5e584, 0xc10f7481, 0xc74f3250, 0x294d4314, 0xf218f62a, 0xbe36dd70, 0xbeaa0c3f, 0x9a66f7de,
    0xca7598a9, 0x74f01d27, 0x8a84c57e, 0xf93ee784, 0xd9e27be7, 0xb88c12c2, 0xe1d35046, 0xf021ebb2,
    0xb31da875, 0xb87a1f70, 0x87127fda, 0xbe9140b1, 0x73d20b44, 0xbf30e58f, 0x22dcabe6, 0xd2b22506,
    0x13dffa9c, 0x08a24f1a, 0x6b699f44, 0xed104423, 0xdf4463e5, 0x086465e0, 0xd768e20a, 0x7080ec42,
    0x761980c7, 0xba10181a, 0xe954f476, 0x56b94dae, 0xed2b5c3e, 0x19e1edd2, 0x0b8b1dfa, 0x288352f0,
    0xf9d4343f, 0x77d45fa9, 0xf2eb10df, 0x8193e066, 0xebcef4df, 0xed355845, 0x9130092e, 0x0b4b105c,
    0x6deaa954, 0x00ca5fcc, 0x6c19c770, 0x2bec7ac1, 0x675aa293, 0xe2c1356f, 0x02e99d4c, 0xca1f029b,
    0x81577606, 0x43aa5649, 0xd8acdcd2, 0x02eca489, 0xd6806d41, 0xdb7d473d, 0xb6727207, 0x5d8b8519,
    0x8298a580, 0xa83beb6f, 0x9b66834f, 0x7232aef3, 0xb093a737, 0x00143b04, 0x4b0790a7, 0x69a1813b,
    0x9c32bdd8, 0xc4d098c2, 0xf75bf47c, 0x3c13f44d, 0x0722b0c6, 0xf64fec4e, 0x08103306, 0xb3842891,
    0xf64d747f, 0x8b7a0dac, 0x00c576bb, 0x4b203250, 0x655dd749, 0x97940461, 0xd6ebd0b6, 0x71ed40ff,
    0x06be4598, 0xf50bcbdf, 0x60a20dbb, 0x718fe6a0, 0x28eb86de, 0xbe56bb55, 0x5400cefa, 0x21736091,
    0xb1517a39, 0x2856e717, 0xcb03d166, 0x928e9a11, 0x0c1052b6, 0x7115278b, 0xb9a5f3b0, 0xdee57825,
    0xc30ce87f, 0xd59607a8, 0xd7449022, 0x10efa4af, 0x2c4fabf2, 0x3e9cc178, 0x939f65f6, 0x5ab22589,
    0xc0ff5d23, 0xc07b4a44, 0x153becf7, 0xa0b00881, 0xf55dc326, 0x759b0fb1, 0xe39dd447, 0xa2bb10f5,
    0xa828ec60, 0x6ad161cb, 0x9dd96cc9, 0xeca339e2, 0xa98d03d9, 0x869a514b, 0xeb0938ba, 0x6af717d9,
    0xcf00effa, 0x95e537eb, 0xb7f75803, 0x70de0342, 0x7c75313e, 0x6f03cba0, 0x38ae51f5, 0x48a3d559,
    0x9fdbc372, 0x1cd32f3d, 0x557f3c10, 0xae4e2d0e, 0xe7816456, 0xd6528fa3, 0x1d94f55f, 0xc1f195ba,
    0xd19b31d8, 0x6d652ec0, 0x8407fdc7, 0xc811527a, 0x594b912b, 0x65ea6641, 0x3d11c579, 0x2c8d298e,
    0x7a62979b, 0x83e6ba1c, 0xdfc26c85, 0xf6443c4d, 0xd5ec2a78, 0x2c5d5e3d, 0xe53a6e53, 0x02058696,
    0x5da524d9, 0xa208a02b, 0xe34845fc, 0x63bfe737, 0x899e0294, 0xbef584bc, 0x532c25a2, 0x80b3a44a,
    0x655dd749, 0x97940461, 0xd6ebd0b6, 0x71ed40ff, 0xc904f20b, 0x294a27f9, 0xe3d398ff, 0xc17e37f6,
    0x28eb86de, 0xbe56bb55, 0x5400cefa, 0x21736091, 0xf0be68eb, 0x193a3a98, 0x2c9b20c3, 0xc1531237,
    0x0369c1b8, 0xa9e8ca16, 0x0ef80eb9, 0x174ab72a, 0x090308dc, 0xb2110926, 0xf263877d, 0x892532d1,
    0x6987d2ef, 0x85785f72, 0x72d2a4a1, 0x361bbafc, 0x4d16eacb, 0xd2acbc82, 0xd457d5d3, 0x929e7d9b,
    0xb622bf61, 0x51090ab7, 0x0e406ba3, 0xb872ca0b, 0xd9e27be7, 0xb88c12c2, 0xe1d35046, 0xf021ebb2,
    0xf2e0620d, 0x61b7063a, 0x894c9e2f, 0x920cc839, 0x59610e00, 0x454e231a, 0x19927835, 0x9487f3fe,
    0x5baaab32, 0xea3664bf, 0x0417be04, 0x23947046, 0x96d56bc7, 0x51bab037, 0xb6f60d5b, 0x177087c8,
    0xd8ba77d2, 0xfdbeacef, 0xc835412b, 0xb9065cd3, 0xb66dc83a, 0xc952d909, 0xccd13964, 0x9b3b999f,
    0x594b912b, 0x65ea6641, 0x3d11c579, 0x2c8d298e, 0x6c84c92e, 0x4ca8e67d, 0xaed9dae9, 0xcec2b3e8,
    0xc0679139, 0x1980b9f5, 0x3dff7f6c, 0x1e35a993, 0x1237ab07, 0xe9e19877, 0xf3030e26, 0x46e14ad8,
    0xcdf07e86, 0x03f1efa7, 0xf5d281b2, 0x522beb5e, 0xede190c5, 0x9245d133, 0x3497a0c8, 0xb34462cc,
    0x6609b31b, 0x01a83f1a, 0xd7cd6bf1, 0xcdc05aac, 0xf5928c1d, 0x63eb74d3, 0x9c3e6202, 0x5d6adeb7,
    0xca7b3287, 0xf9b481d3, 0xe12c9f2a, 0x3927e037, 0xd9316fdd, 0xab4bace9, 0xdbc639a9, 0x7c6791a9,
    0x17a1c86b, 0x7591a483, 0x7470df0f, 0x95ac64d1, 0xf672955e, 0x6f09d686, 0x33b81c5b, 0xcc35cfe1,
    0x7117b143, 0x7bf6999f, 0x3a80bc1f, 0x39d6fa96, 0xb0e9a556, 0xa06858d9, 0xcfa5c8ad, 0x8af570b3,
    0xd9e27be7, 0xb88c12c2, 0xe1d35046, 0xf021ebb2, 0xfcd9a86f, 0x2055a0de, 0x0d1e9c77, 0xa2aeaf8f,
    0x73d20b44, 0xbf30e58f, 0x22dcabe6, 0xd2b22506, 0x95ede01d, 0x99ad8909, 0x45ddebc8, 0x8d84bab1,
    0x4ea72b2d, 0xf63da102, 0x3e4a4386, 0xe655d2ec, 0xf9c9d845, 0xee3b94e5, 0xccbb4864, 0x0fd247e7,
    0xed2b5c3e, 0x19e1edd2, 0x0b8b1dfa, 0x288352f0, 0x595af1f6, 0x4f997c8a, 0x4d786ab0, 0xe56a8022,
    0xb03735c9, 0x67080dd1, 0xbc90bb4b, 0xff31524b, 0x189b92a9, 0x81dcb3a1, 0xfa03644d, 0x24f45731,
    0x43389087, 0x739f5a60, 0x72357569, 0x4c6894a3, 0x46574681, 0xfc35c3d8, 0xd10232bf, 0x774a238c,
    0x756902e5, 0x00720a2b, 0xa45999f3, 0xf5acd0c2, 0x6609b31b, 0x01a83f1a, 0xd7cd6bf1, 0xcdc05aac,
    0xf5928c1d, 0x63eb74d3, 0x9c3e6202, 0x5d6adeb7, 0x27dc49f8, 0xa7ed9711, 0xeb88ebb4, 0x947b85a3,
    0x9be4bf81, 0xf233b120, 0xb9a7f5b2, 0x518174aa, 0x37c273ce, 0x5756e69a, 0xaf03080b, 0xeb6fc3c6,
    0x8a81067c, 0x3b82e34a, 0x163f5ed8, 0x039f32e8, 0xfbcf6d55, 0x4178129b, 0x482cbf31, 0xd7a4767e,
    0x5ab888bb, 0x3f238258, 0x17115bbd, 0x0d015921, 0xf9d4f376, 0x96209b6b, 0x9d50d4fa, 0x36e1c4ea,
    0xe7dc18a0, 0xbb3bb007, 0xd0824227, 0x9e06cf6d, 0x91833e48, 0x15c5b47a, 0x88771353, 0xe1ef0e38,
    0xca0327dd, 0x3bc61204, 0x1f1787a4, 0xdf44af2c, 0xa77703e3, 0xa06f1adc, 0xfd7a60eb, 0x58005cc2,
    0x79f400b7, 0x51027885, 0xf406cc9f, 0xf96d69cd, 0xed2b5c3e, 0x19e1edd2, 0x0b8b1dfa, 0x288352f0,
    0x8e120f79, 0x066126ce, 0x07291cec, 0xdf93272a, 0x4032f070, 0xf3062993, 0x2a3af1f8, 0xc0624d1d,
    0x189b92a9, 0x81dcb3a1, 0xfa03644d, 0x24f45731, 0x3f82accc, 0xc9a80c16, 0x3b74fc59, 0x135844d9,
    0x6649ec41, 0xa0fc6de0, 0x7f7ffc76, 0x7ce15f20, 0x655dd749, 0x97940461, 0xd6ebd0b6, 0x71ed40ff,
    0x98e172c0, 0xa9817990, 0x101618d1, 0x9b3e8649, 0x28eb86de, 0xbe56bb55, 0x5400cefa, 0x21736091,
    0xe729d4c0, 0x82da1b9a, 0x4fd2f6bc, 0xbf6bcb1f, 0x2782b764, 0xd917177c, 0x6c99eb6d, 0xbf663972,
    0x4e469553, 0x5b667d3b, 0xf0afb910, 0xe74611b4, 0xf25e9f4a, 0xebdc361b, 0xd20e8077, 0x8e2cd3e5,
    0x5302189e, 0x99e6f5b0, 0xf2bcbda2, 0xe693d879, 0xbfe2c029, 0xb03c8f5d, 0x4d94cfef, 0xef7f5821,
    0x61aac0ea, 0xec88dbd7, 0xc1801f55, 0x54174039, 0x4614c907, 0x1b22749f, 0x451e8923, 0x8c2febeb,
    0x894aa6f8, 0xb7c113a0, 0xdb375a54, 0x0a9c66d5, 0xb36301d4, 0xb7e9e41a, 0x46a1a5f3, 0x79f8273a,
    0x88e42e3e, 0x5b8ea2a5, 0x9ee3fbe1, 0x4a6605c3, 0x878b5a25, 0x9e225f35, 0x77e72000, 0x8cb08e42,
    0xb316139a, 0xcb8b7552, 0xe05bdfb8, 0x54900072, 0x6609b31b, 0x01a83f1a, 0xd7cd6bf1, 0xcdc05aac,
    0x4563645c, 0xb2f21647, 0x9dd067e1, 0xddc552a1, 0xd2cdb696, 0x0ef62669, 0x740edf7f, 0x72fcbaab,
    0xd9316fdd, 0xab4bace9, 0xdbc639a9, 0x7c6791a9, 0x74840e19, 0x76894332, 0xc3bee2fe, 0x9ca1417e,
    0xd21b0106, 0x51882da7, 0x4325b2f1, 0x003997a2, 0x7b8a04e0, 0x14260491, 0x011ee540, 0xd9f3eea9,
    0xa107babf, 0xf1077011, 0x23032afc, 0x64d93135, 0xfb7b9579, 0x9d79dbc9, 0x003f105e, 0x4ea0c85e,
    0x38639a68, 0x05522af3, 0x33756fc7, 0xcdb6a4cb, 0x613bc2fd, 0xe68bf5b1, 0x6e58352c, 0x70f8f25c,
    0x1fdb25b6, 0x41ab8065, 0xe2511e49, 0x0638b6bf, 0x91ea4b57, 0xdf2566ac, 0x6689d7da, 0x921ca047,
    0x0bd659da, 0xa47eded4, 0x9db367c7, 0xd1a52537, 0x5f9b9547, 0x5dc86047, 0x222080fd, 0x34de568e,
    0x27fbd4f2, 0x45f798b6, 0xcbb9ac8e, 0xa5c89573, 0x1b9175e1, 0x38ee75a8, 0x3654bcb8, 0x09d6fd55,
    0xe5be77b4, 0xf4ba5aa6, 0xf86de7be, 0x414e90e9, 0xb0dc3d2e, 0x8d23e3b2, 0xb2902422, 0x8dc9e3b7,
    0x16e18c5a, 0xd2802b02, 0x308d6d7a, 0x74f400e2, 0xba0f4cbf, 0x7e73431d, 0x51576f96, 0xa8830cfe,
    0xa6665cdf, 0xd5c17751, 0x8c3d6a69, 0xfab19b4e, 0x62fc04a5, 0xdc13ab20, 0xfcbf567c, 0x95377a10,
    0x95d05ef1, 0x6e895c49, 0xcd85c640, 0xc8053b95, 0x826ca080, 0x119b599b, 0x0c093a60, 0xa35a8306,
    0xa90201e1, 0xd4abe13b, 0x9a095b38, 0x1341403b, 0x6bfa0726, 0x69a5f363, 0x1febb411, 0x0ecfd247,
    0xbeb79cac, 0x14d0652d, 0x08edbd6f, 0xc27519f6, 0x137b074b, 0xc8549309, 0x7bcab74b, 0x670a5837,
    0xb97aa92b, 0x7339623e, 0x2ee7479d, 0xad8417a5, 0x2f315a8b, 0xc5eff854, 0x60b07c3f, 0x91784810,
    0x90feefd4, 0xf9177519, 0x13dd5263, 0xa41bcc1a, 0x43573c2e, 0xbc2f2121, 0xc99c626b, 0x74709793,
    0x987a7346, 0x474d972b, 0x825e4a44, 0x8ebb6fc1, 0x3febf09e, 0x9f2e74af, 0xe3f521dd, 0x4fa3bba2,
    0x69a51d93, 0x5e061b3d, 0xdef3a703, 0x5fd7c68c, 0xf47d70c1, 0xebd0d511, 0x9f3342a6, 0x3205d748,
    0xb358c048, 0x7e45a6c0, 0x90993648, 0xcbb44bad, 0x5c0fe21c, 0xcd0ae506, 0x17b3a186, 0x4f7ae26a,
    0xd3b9793a, 0xc3551f18, 0xfc77567e, 0x8d696123, 0xf1b27371, 0xf897017c, 0x30efed0f, 0x47ed4b6f,
    0xf5404b47, 0x55f20e1c, 0x04b29236, 0x6774ff77, 0x1fafb13d, 0x6421fd78, 0xea059bb4, 0x95ae2cdf,
    0x6d087a56, 0x25bd02ac, 0xecc8d348, 0x88ef2ec2, 0xf40478ca, 0xb3a074e8, 0x430ce74a, 0xc81cd873,
    0x63a2da92, 0xebcfffe4, 0x67aeb64c, 0x48135ab9, 0x2afd044c, 0x6f69dd65, 0x037db23b, 0xe43db760,
    0xd9b26c4f, 0xdc64f2b3, 0x76705955, 0x33e6de4b, 0x96168870, 0xaf0ef746, 0x184146fa, 0xc0f1ae8b,
    0xef3a0fc5, 0x7a77d02f, 0x2e3e4a7a, 0x5b6069c3, 0x02391917, 0x20ab0ed3, 0xb4d09fb1, 0x51e6bd63,
    0xd34a4d28, 0xf0575a65, 0x93ebd856, 0x02fef466, 0x90feefd4, 0xf9177519, 0x13dd5263, 0xa41bcc1a,
    0x2cf2f4a9, 0xbad7b1e3, 0x0d5bb501, 0xb0cc4736, 0xe777c4c0, 0xada1c92b, 0x37ff3092, 0xd31dd279,
    0xe1e6c6a3, 0x0a916dfa, 0xde795db7, 0xed8802f0, 0x2007a3f8, 0x15279348, 0x58830277, 0xaba3cd91,
    0xb36301d4, 0xb7e9e41a, 0x46a1a5f3, 0x79f8273a, 0x1756cf09, 0xf2fa07b0, 0x49f58a67, 0x8175f90a,
    0xc6dcb548, 0x8e8bb6ca, 0x3114d860, 0x5fe96e65, 0x14530e4b, 0xd7281adb, 0x6f13af76, 0xc5d25c33,
    0xef14b578, 0x45598515, 0xccd7448c, 0x79094b83, 0x00dc0e3e, 0x32b05915, 0xdac305aa, 0xa078a0b3,
    0x75c06c3d, 0xe716618e, 0x7267d98e, 0x57c0dfd4, 0x9be4bf81, 0xf233b120, 0xb9a7f5b2, 0x518174aa,
    0x58b2239d, 0xc44f0dd6, 0xc68190a5, 0x47d6fd90, 0x8b3da802, 0x8abee13b, 0x8f35850d, 0x80024d8a,
    0x211a8082, 0xaac89d87, 0x99233351, 0x98996122, 0x01150fe2, 0x7d50a142, 0x2abd71ca, 0xeaa2360c,
    0xb0edeaea, 0x26279dd4, 0xb5b0076d, 0x44795836, 0x22ca0e7e, 0x3c9095d4, 0x73e11627, 0x05e0a5de,
    0x4142d834, 0x78912110, 0x07f57f43, 0x11d9ec8b, 0x74b73837, 0x63ac8ac2, 0x08ae1bdc, 0x86427a02,
    0x668d9074, 0xf8b6c63a, 0xed4d72aa, 0x42feb381, 0x0f5e24b9, 0x86854b94, 0xd15d00d3, 0x13524cdd,
    0x561d678e, 0x6025ae31, 0xb391d50c, 0xc2f9888e, 0xe0e1d0ce, 0x6bc6930b, 0x2c467665, 0xe97e0060,
    0xb7edb2fd, 0xdb3a6567, 0xb1622bb3, 0x75281bb7, 0x6609b31b, 0x01a83f1a, 0xd7cd6bf1, 0xcdc05aac,
    0x4563645c, 0xb2f21647, 0x9dd067e1, 0xddc552a1, 0xb58b018b, 0xcaaeb275, 0xfa1a562d, 0x53d147f6,
    0xd9316fdd, 0xab4bace9, 0xdbc639a9, 0x7c6791a9, 0x3fcb4ab6, 0x97e3d953, 0x315e767e, 0x3473bca3,
    0xb922e7b3, 0xa3dd0674, 0x809fc0a7, 0xeaf5d1fe, 0x31b89beb, 0x88bfb978, 0x9192e2be, 0x54043768,
    0xb90f39a0, 0xaa919d55, 0xd70cba4d, 0xf95231ba, 0x08fad250, 0x6d6cd214, 0x79910661, 0xe580fa6b,
    0xafb65f06, 0xc6d4eaa9, 0x8d682448, 0xbcada0ef, 0xb7908b96, 0x6534f8dd, 0x5d7b9f71, 0xfddff13a,
    0x4ff0d7fb, 0x83fc772d, 0x54543d1c, 0x387fdcd1, 0xb66a37d2, 0x2bc1b7d8, 0xaf7c6801, 0x96019da0,
    0xa64f2619, 0x08fbf9e5, 0x95deccc6, 0xfed46e9b, 0xf8f64f59, 0xc49e69d9, 0x322af980, 0x474e149e,
    0xb9f848d2, 0xb7c4088e, 0xbf9a4451, 0x6252a2c6, 0x84a7b3d7, 0xd306ec4c, 0xd0a451de, 0x6d74f71f,
    0x853ff66a, 0x8357ebd6, 0x00c720c3, 0x6549175d, 0x59fd7eda, 0xd0202507, 0xd42af06b, 0x74d6510c,
    0x1c459411, 0x5387e504, 0xdf41bcf5, 0x2639debd, 0x363a44a7, 0x2e4209ad, 0xe9cf400d, 0xd83e0385,
    0x6609b31b, 0x01a83f1a, 0xd7cd6bf1, 0xcdc05aac, 0x4563645c, 0xb2f21647, 0x9dd067e1, 0xddc552a1,
    0x5256c991, 0xe4251d73, 0xd120a783, 0xfa921b22, 0x9be4bf81, 0xf233b120, 0xb9a7f5b2, 0x518174aa,
    0x7d50e2fd, 0x573e0dfb, 0x59fc37da, 0x3c8ce016, 0x21773a14, 0x596d5604, 0x07fac55a, 0xebe6914e,
    0x21b80e50, 0x3f7a2216, 0x574926a8, 0x5c469d57, 0x6bc05623, 0x0b8eb935, 0x87e8b373, 0x1ecc8151,
    0xf0e682ae, 0xddfbc916, 0xa749d047, 0xf3ab9c54, 0x57d75267, 0xde1f2dfd, 0xd4ea7c66, 0x593ea1d1,
    0xdb44f15c, 0xfecc828d, 0x73fb1c2a, 0x3ed4801c, 0x754f7349, 0x5ae94249, 0xb2d9e1c6, 0x062bd67f,
    0x7420191b, 0xec44cde9, 0xa92b9d90, 0x2f9e105d, 0x9667a24b, 0x7a9b0f99, 0x69e7ce04, 0x963edff7,
    0x61aac0ea, 0xec88dbd7, 0xc1801f55, 0x54174039, 0xbf1f0d2a, 0x2efb3b6d, 0x0b02e786, 0xbbcb7a26,
    0xfe00449e, 0x14013537, 0xf1fefedc, 0x3074242c, 0xb36301d4, 0xb7e9e41a, 0x46a1a5f3, 0x79f8273a,
    0xb046ea08, 0x833860bc, 0x354c9e08, 0x9acde24f, 0xfaf411b0, 0x0f6ab8f5, 0x9786d1f0, 0x08af7003,
    0xd2798a6c, 0xb2821aaa, 0x3a907111, 0x521eea7b, 0x5ef780b1, 0x7a51b7a1, 0x7ccc11e3, 0xab54cdcb,
    0xa7c40909, 0xc0d6bd11, 0x5beefd92, 0x37f88ba2, 0xc1d1e54a, 0xa6318ac8, 0x01f986d3, 0xe706839e,
    0x113514eb, 0x61f845f1, 0x4bba5a03, 0x8c697b6e, 0x9be4bf81, 0xf233b120, 0xb9a7f5b2, 0x518174aa,
    0x313c30b2, 0x5a21d3fe, 0xa30e4b72, 0xe5c72a92, 0x8b3da802, 0x8abee13b, 0x8f35850d, 0x80024d8a,
    0x513def90, 0x73f8b33d, 0x7ae28856, 0xa8e8a037, 0xad0f0420, 0x5d1ede72, 0xd93b85be, 0xb2d3bf04,
    0xa64cad4d, 0x5a3087b9, 0x5d9a721d, 0xee09bdb4, 0x075d1641, 0x90141b14, 0x0329483b, 0x7720d25f,
    0x13e9991c, 0x26fac38a, 0x95746372, 0x0702eb2e, 0x1f892651, 0x85c8a584, 0x96ea3a92, 0x5b21cccf,
    0x6d0d7b39, 0x1ad036cd, 0x1c235520, 0xbdee8c25, 0x9893b55d, 0x00a8be86, 0x949d01fd, 0xa1faf87b,
    0xe66063d1, 0xf90db252, 0x027cc6d8, 0xde79535c, 0x8b78ebbf, 0xead61bee, 0xd6587310, 0xa76dc165,
    0x9f4258ce, 0x082c7de8, 0x42774db2, 0xc066bce5, 0x06ce47f2, 0xee01f15a, 0x4004f9c7, 0xca210924,
    0x158d51fc, 0x4436cecf, 0x8db2886c, 0xaa3232f1, 0x8d0a5c04, 0xe33e9918, 0x5823803f, 0xf0ef8622,
    0xb7034d7e, 0xe4df65bc, 0xa390cce1, 0x83463ad1, 0x3e1c7986, 0x73ce7ce6, 0x0119ea4d, 0x88235b78,
    0xde9d1828, 0x67c49acb, 0x8a42e061, 0xc754a9df, 0xbaaa1f35, 0xbff1b3f0, 0xc24a6993, 0x7c7cc22e,
    0x19b0d004, 0x72db4d5c, 0xd3f7e196, 0x084c98fb, 0x39ab947f, 0x7c57c46d, 0x934bae58, 0xfa7d2442,
    0x44f63e8c, 0x4b59e9a9, 0x35d24802, 0x33923932, 0x8d031670, 0x0f7a3e50, 0xf6f01f80, 0x70193805,
    0x66794b3d, 0xb6f7f8aa, 0x64442c8a, 0x8982abab, 0xa36eb30d, 0x02b34683, 0x60bf6951, 0x8cd2febc,
    0x03ee6dd6, 0x71f6b7ab, 0xde0942e6, 0x94a3c2e7, 0x61aac0ea, 0xec88dbd7, 0xc1801f55, 0x54174039,
    0x0e9059be, 0xfc4c20cc, 0x5712003a, 0x6c1cf904, 0x47654d82, 0x8be143c2, 0xd3438a60, 0x4affe8b6,
    0x931e9c2f, 0x749f7972, 0x11ebc8eb, 0x2224bd1f, 0xc28d9069, 0x09d60410, 0xca48f28b, 0x6cac97d4,
    0xc10b700d, 0x7319dbb6, 0x592d0125, 0xe3c89cbf, 0xc0ff5d23, 0xc07b4a44, 0x153becf7, 0xa0b00881,
    0x749ff5a9, 0xf8332cc2, 0xf3d1aae3, 0x72e19758, 0xa828ec60, 0x6ad161cb, 0x9dd96cc9, 0xeca339e2,
    0x81a014cf, 0x57c815bb, 0x304f1960, 0xce9eac33, 0x69b9c636, 0xd584efcb, 0xa5989829, 0x01a0de5a,
    0x36900188, 0xb1cf1f6e, 0xbdda717f, 0xfe4b7848, 0x423198e8, 0xa829a8b9, 0xce43bf53, 0x902981fa,
    0xa6fb234b, 0xa5c5f5d7, 0x701236a0, 0xcd2d5e81, 0xe24750cc, 0xf1836575, 0x1d0ded19, 0xcd21504f,
    0x4d7f16fb, 0xb62153a1, 0x112d036e, 0x66ea4af7, 0x245e6844, 0xd49d39bf, 0x5ffb0639, 0x10e02ca1,
    0x1a889a27, 0x308f4ef4, 0x243ba4d3, 0xf677c71f, 0x23efdb82, 0x4a780857, 0xbb0c6180, 0x5a4f5166,
    0x6609b31b, 0x01a83f1a, 0xd7cd6bf1, 0xcdc05aac, 0xf5928c1d, 0x63eb74d3, 0x9c3e6202, 0x5d6adeb7,
    0xfb8809ad, 0x0e4ea734, 0x050e7ba8, 0x56dc6e2b, 0xd9316fdd, 0xab4bace9, 0xdbc639a9, 0x7c6791a9,
    0xfd392efc, 0x8a2302b6, 0x7c4f9f98, 0xd3d92315, 0x36004141, 0x4f3c2f48, 0x9e171f8a, 0x21ae4f5a,
    0x574f4a27, 0x8facffe1, 0xe4ed6fce, 0xe01e1e9c, 0x17a31b24, 0x0c6862bf, 0x9bbca764, 0x13794e6a,
    0xa26dc651, 0xe256367e, 0x6e8681ef, 0x543114b3, 0x141bb349, 0xbe1cddf7, 0x16c503a4, 0xf2ff8763,
    0x594b912b, 0x65ea6641, 0x3d11c579, 0x2c8d298e, 0xfecfd3e1, 0x39590848, 0x18a052e8, 0x0b0ff1db,
    0x4e1e2a1e, 0x8da3a433, 0xd196f890, 0xddfeb899, 0xcbf26858, 0xbf444c38, 0x51149b53, 0xb967aadb,
    0xf19b4faf, 0x40b3f6fe, 0x0517b5f8, 0x62c5cfe9, 0x3e1c7986, 0x73ce7ce6, 0x0119ea4d, 0x88235b78,
    0x5ea9f776, 0x70bc760c, 0x99777ceb, 0xa0d05c43, 0x5f710310, 0x23a2536a, 0x053376b8, 0x1a33da11,
    0xa181cab3, 0xf688ef5a, 0x07101d78, 0x10721b01, 0x2828be3e, 0x432437c9, 0xfef594a5, 0x6109903d,
    0xb2d913d5, 0x9580c39b, 0xa118a6a5, 0xf2f5187d, 0x0d717fac, 0x5bfa6563, 0x1782d0f5, 0x8196673b,
    0xfee60a61, 0xeaec2065, 0x35741ed1, 0x60f0cc05, 0x05ce48af, 0x64f508e3, 0x1f404f67, 0x460c1720,
    0xed2b5c3e, 0x19e1edd2, 0x0b8b1dfa, 0x288352f0, 0xa7e68c52, 0xc756ec3c, 0xc958b279, 0xf56f61b1,
    0x7152298b, 0x2ded7b57, 0xa2f4888c, 0x0ffb897b, 0x189b92a9, 0x81dcb3a1, 0xfa03644d, 0x24f45731,
    0x975acad3, 0x9690e623, 0x5d036e37, 0x6075d828, 0x5e962b92, 0xf9b1be22, 0x420d8c2d, 0x292f757d,
    0xcc9da5b5, 0xb62c71f2, 0x52260aa8, 0xbb74d8c9, 0x6609b31b, 0x01a83f1a, 0xd7cd6bf1, 0xcdc05aac,
    0xf5928c1d, 0x63eb74d3, 0x9c3e6202, 0x5d6adeb7, 0x8f71ee03, 0x873a02be, 0xb1aa12df, 0x2d8aadb9,
    0xd9316fdd, 0xab4bace9, 0xdbc639a9, 0x7c6791a9, 0x130b47f7, 0xca1efe7e, 0x9c7717b2, 0xe4ff048c,
    0x74cdc0f4, 0x7a4cd794, 0xfc117a71, 0xe3303e8f, 0xd4bb405b, 0x7eccc4c6, 0xf5e6418d, 0xddebc3ae,
    0x5ab888bb, 0x3f238258, 0x17115bbd, 0x0d015921, 0x509031ad, 0xa0942a0b, 0xdf726308, 0x7ef9d1bd,
    0xc6e7d9dd, 0xbcb2eb1a, 0x5007da55, 0x90283a23, 0x237b40fc, 0xe0152018, 0x23ae6986, 0xc864639c,
    0x29a56e88, 0x0f0c904c, 0xe90ea866, 0x3b4647c4, 0xc0ff5d23, 0xc07b4a44, 0x153becf7, 0xa0b00881,
    0x3891a886, 0xf22cfe92, 0x4bc4d054, 0x6b50f3c7, 0xa828ec60, 0x6ad161cb, 0x9dd96cc9, 0xeca339e2,
    0x3ece6880, 0x5ff16d67, 0x45cadf32, 0x31a3dd2d, 0x5e9cd82b, 0xfab977cd, 0x1fe0f250, 0x4b8ce119,
    0x1844b05d, 0xf361433d, 0xe4f9e4d8, 0x1564909b, 0xa797f3d6, 0x59c9b8cc, 0x058007e0, 0xb0c7aeb8,
    0xb242e697, 0x6dbd2aa6, 0x4c3d2e18, 0xaf9db454, 0x54215182, 0x68316383, 0x99df4aac, 0x403cee4e,
    0x594b912b, 0x65ea6641, 0x3d11c579, 0x2c8d298e, 0x04bf3f34, 0xb800519f, 0xfee51511, 0xae1c17a8,
    0xeb6a3833, 0x658c6f0b, 0x0ad093bf, 0x4b67463d, 0x116631ab, 0x2334655d, 0x257eea0e, 0x1e24bb2a,
    0x5b98ed21, 0x6d9a9e5e, 0xfe7c6641, 0xde0b9a67, 0x59a6c595, 0x55f23c3d, 0xc5d9c264, 0x53bdafdf,
    0xfef0cf3e, 0xcd5a0dc5, 0x24bad394, 0x12b9d70b, 0xb36fd040, 0xa0ec3531, 0xe010ecd3, 0x0803e6d2,
    0xb2aee0aa, 0xb9a5a938, 0xc767ed6a, 0xb3cb8ecc, 0x149d2ab1, 0x6625f941, 0x732fb1ea, 0x2251f90c,
    0x028f0135, 0xb78eb755, 0x3f407e68, 0x800b5cbd, 0x686b259d, 0x42811ee9, 0x528667b2, 0x23e7ba75,
    0xd3bdc660, 0xf573ad5e, 0xaa7cda54, 0x4cbf82f0, 0x90feefd4, 0xf9177519, 0x13dd5263, 0xa41bcc1a,
    0xccec39dc, 0x9b0849d8, 0xd16555c4, 0x3fd637e1, 0x334ca86f, 0x3cb2ac76, 0x5adf91ba, 0x3856dcbb,
    0x27ff6c6f, 0x138195b9, 0x21446ad0, 0x6e4c9a64, 0x78a64228, 0xc9436d3e, 0xbeed8757, 0xee156945,
    0x59d55f01, 0xe2ae23e4, 0x174c141d, 0x32e8a2dc, 0xb3e31a49, 0x899ae0c3, 0xb0f1cb35, 0xbdeedbaf,
    0x02149284, 0x7bdf1526, 0x3d6db146, 0x33891770, 0x21e6fa24, 0xfc0bfb5b, 0x1e9fde29, 0x89a423eb,
    0xc837325f, 0x661cd667, 0x9c88b709, 0x39461e76, 0x446e2090, 0xcdb6c433, 0xe0770803, 0x0ba75a6c,
    0x0f61dc59, 0x74954d45, 0x406fcb8a, 0xafc2099b, 0xe6da2770, 0xc07ef14c, 0x3e93ae36, 0x0c1a5e7e,
    0xe6b3a6ec, 0x805545ab, 0x709b7eaa, 0x3b09a768, 0x8986d70c, 0x3a921ae1, 0x51fbc97e, 0x9b86d077,
    0x49716ce3, 0x10b3ae37, 0xe83480bc, 0xbac5372a, 0xf83949db, 0x505fd40c, 0x8c2f0d0d, 0x20d93f0b,
    0xd44f375c, 0x1bdfba00, 0xe5f0d248, 0xac7d318d, 0xfae5a96a, 0xb150e587, 0xd1970be2, 0xc3779509,
    0xdd3443fb, 0x7e847fea, 0x92799212, 0x4ad84dbb, 0x36004141, 0x4f3c2f48, 0x9e171f8a, 0x21ae4f5a,
    0x31e0f392, 0xcdfdd78d, 0xc7eda853, 0x53e2abc7, 0xe021f871, 0x04802868, 0xa254a615, 0x1dc3e6cb,
    0xa4a6647a, 0xd71a5b3f, 0xb917e979, 0xeaf0d4b9, 0xc72e9dfe, 0x4e5d850a, 0x2e7c7778, 0xbfcd19b5,
    0x4fd1000b, 0xf6b6ec68, 0xaf49566f, 0x5dfe8dc6, 0x2f183c0a, 0xc48f6959, 0x6b0d3c2f, 0xcfd96e85,
    0x81977dc3, 0x3d82de33, 0xa749153d, 0xfcf0703c, 0x0fabfc2f, 0x0f680792, 0x24891f46, 0x30a4a816,
    0xc1c9c751, 0x2deaf932, 0xf2c7b899, 0xc9c7e574, 0x93dd3076, 0x2c313686, 0x03aa35cd, 0x1754a831,
    0xbcebd606, 0xdf150f63, 0x04f06da9, 0xd3f6f0a5, 0x5378c672, 0xcfcd2339, 0xadf5a382, 0xd4ce1ff5,
    0x9d729eab, 0x5759599f, 0xd4bca88a, 0xdac7e6ec, 0x767d50a9, 0x564c5d36, 0xf1447f7d, 0xb094d79e,
    0xdda8a72e, 0xb58be483, 0xf2d62b17, 0x6bc0e6d8, 0x082087d8, 0xe43b1682, 0x5572e537, 0x642a9d6d,
    0x31632c2c, 0xbf5d4e1e, 0xcfd82bf4, 0xbdcf8836, 0x020a58aa, 0x2b3fbba3, 0x0fca9938, 0xa5662d24,
    0xcc2dfd14, 0xe81424fe, 0x5910a587, 0x112ce18b, 0xb9f848d2, 0xb7c4088e, 0xbf9a4451, 0x6252a2c6,
    0x8c35298e, 0xe5392f77, 0xa44a916a, 0xaab554b0, 0x4b6da16b, 0x3b3f0e85, 0x1e000789, 0xfb561457,
    0xf5c0645b, 0x11321a2e, 0x252d2244, 0xf1a6715d, 0x3e39acb0, 0xd6c50ea2, 0x7eea0891, 0x6bb921ad,
    0x8cf4d2a7, 0x2ec8fb42, 0x4fbb02e7, 0xbbb1ef93, 0x7345a948, 0x9035df61, 0x1e9ed48f, 0x2bf07faa,
    0x20b0e466, 0xcefe11ae, 0x9bbaafe9, 0x9da78c97, 0x6aa724a9, 0x2af0408b, 0xe4217389, 0xe4934624,
    0x837ed154, 0x7ce7db84, 0x49423111, 0x45427708, 0x01a8e238, 0x0293f12e, 0x878335aa, 0x00488447,
    0x6e1b8661, 0x77314770, 0x6982cd18, 0x65088eaf, 0x6551c0ab, 0x510dbee5, 0x3d1fe0e7, 0xe056f1d3,
    0x0a12e6a0, 0x77422231, 0x80d183d2, 0x05617e29, 0x1b30058b, 0x627b4e23, 0x6da83b0b, 0x8309865e,
    0xd234a4d0, 0x9d7e513d, 0xb6e9cd68, 0xaf8ed5fc, 0xacc55378, 0x33310f25, 0xdb3805f2, 0x7770e39f,
    0xfe6461d0, 0x14489460, 0xc41ca48e, 0x821ccdff, 0xa7a35d03, 0x974cfeee, 0xa89b85bf, 0x5f0ff548,
    0x362af229, 0xc0c58910, 0x3f39cfb1, 0x809ff186, 0x4362ac2a, 0x4b82f014, 0xe07d14f1, 0x39200487,
    0xd9518683, 0x9203b9f6, 0xab463532, 0x15388b44, 0x18eae745, 0xe89788b2, 0x884b0dce, 0x7b93b6ce,
    0x5c0fe21c, 0xcd0ae506, 0x17b3a186, 0x4f7ae26a, 0xc73e1d77, 0x722ce391, 0xfb37d579, 0x08cb8990,
    0x510d63e3, 0x90a5412f, 0x4b4ee76d, 0x96dc4a59, 0x95adcf1e, 0x94169a90, 0xe42c1320, 0x1eb77cff,
    0x338f5cad, 0x1a9f8f56, 0x209fd1cf, 0x2fc420e2, 0x9be4bf81, 0xf233b120, 0xb9a7f5b2, 0x518174aa,
    0x13ece3af, 0x92089d47, 0x1df79b13, 0x9d988a1e, 0x8b3da802, 0x8abee13b, 0x8f35850d, 0x80024d8a,
    0x5ce0b8fa, 0xba3fd91f, 0xd1cb8d60, 0x9bb7ea52, 0x1876badb, 0xaa27e480, 0xefbc4092, 0x52440952,
    0x8a97a752, 0x591c5f09, 0xafd50c1f, 0x166b074b, 0x07fd9d9a, 0xeec7e0b6, 0x327e4263, 0x51776037,
    0x29ac346e, 0x82b8adb2, 0x76628e04, 0x6d73cc09, 0xf508f1aa, 0x72387a94, 0x9371880d, 0x90f262b0,
    0x6d0d7b39, 0x1ad036cd, 0x1c235520, 0xbdee8c25, 0xfcb70a61, 0x424d0759, 0xc43683b9, 0x44184013,
    0xece8e776, 0x09975637, 0xa31054fa, 0x1cce8018, 0x92a2bd47, 0x305ba258, 0x65dd0d8e, 0x34c8ebc1,
    0x12e7e3e6, 0xbd7298fc, 0xfe887bea, 0x1882936b, 0xd0164362, 0xbf706e19, 0xaa320b1a, 0x850eb313,
    0x158d51fc, 0x4436cecf, 0x8db2886c, 0xaa3232f1, 0x22c49f97, 0x7e0eba69, 0xd177546f, 0xc7d87ef9,
    0xbc8f8ab6, 0x1c85b91b, 0x97fb5d3b, 0x9a0f8c3b, 0x3e1c7986, 0x73ce7ce6, 0x0119ea4d, 0x88235b78,
    0xa8f58f78, 0xcee54e8f, 0x213e9df2, 0x1854dd5e, 0x6404a335, 0x9543d13b, 0xc6109034, 0xfe69604f,
    0xac7a00ed, 0xfa84e755, 0x07ba595a, 0x98bdea63, 0x5c0fe21c, 0xcd0ae506, 0x17b3a186, 0x4f7ae26a,
    0xbd967814, 0x4022bae2, 0x88bcfd45, 0x247f1e42, 0x8e349401, 0xd6902791, 0xce1f33fb, 0x8343832e,
    0x3649244f, 0x09776aa8, 0x083defc5, 0x85f25fe2, 0x3b54ab1e, 0xe607ef9d, 0x63e3f30e, 0x06401ec7,
    0x9be4bf81, 0xf233b120, 0xb9a7f5b2, 0x518174aa, 0xce2a6fc4, 0x06e262fb, 0x12494dad, 0x177ba8b6,
    0x8b3da802, 0x8abee13b, 0x8f35850d, 0x80024d8a, 0x541ecca3, 0xdf0901d3, 0xaf526aa3, 0x3f41c4b4,
    0x54cba567, 0x54a1bba7, 0xff2be654, 0x5a3ebf41, 0x532279f3, 0x40eafae1, 0x13d76c0c, 0x4734aab3,
    0x33529537, 0x5a001ef6, 0x3fe2be8b, 0x885d2b98, 0x0316c6da, 0xd4d3a7f4, 0xaab49fee, 0x85a301c6,
    0xc8b259f9, 0xa550cffc, 0x6bebee34, 0x4da99f0d, 0x6d0d7b39, 0x1ad036cd, 0x1c235520, 0xbdee8c25,
    0x40f135fc, 0x230a17bc, 0xb54d13aa, 0x4eec9611, 0x7b8eba43, 0x05f71bd2, 0xeb18eb79, 0x83a6a7de,
    0x7a35db85, 0x48f74c46, 0x53216fff, 0x8e8cad2d, 0x1f5f7149, 0xec9beef3, 0x6c2cd45d, 0x0e3312ab,
    0x984429b2, 0xdfe35435, 0x9b974526, 0x979afb78, 0x158d51fc, 0x4436cecf, 0x8db2886c, 0xaa3232f1,
    0x06c9c3fd, 0x7b97f56f, 0xbe066b65, 0x2631b554, 0x358d3ef9, 0xa3d25321, 0x2e4988ee, 0x03f4d922,
    0x3e1c7986, 0x73ce7ce6, 0x0119ea4d, 0x88235b78, 0x6d66c337, 0x3b95fad4, 0x0042c3b5, 0xf2034aeb,
    0xa9bfb1fe, 0xbab36673, 0x0c30e7af, 0x4dbeeb4f, 0x516e2157, 0xf8eb0e5e, 0x6c752591, 0x3c116daa,
    0x5c0fe21c, 0xcd0ae506, 0x17b3a186, 0x4f7ae26a, 0x87a686fb, 0xc6da1959, 0x65e4d258, 0x5218f612,
    0x99a68131, 0xac7c6242, 0x22407584, 0x857fc7d8, 0xb3777867, 0x8f97e27f, 0x20a0ff4e, 0x54c6ae80,
    0x80b6877b, 0xafdbfd72, 0x7fb79964, 0xf3677558, 0x9be4bf81, 0xf233b120, 0xb9a7f5b2, 0x518174aa,
    0xfa5a42ae, 0x48ec9c8e, 0x0407fa84, 0xb58810fa, 0x6c7694f5, 0xe0a07838, 0x968ecc45, 0xf35ceedc,
    0xda9de496, 0x91fcdf3b, 0x109fcb38, 0x34cb5da0, 0xc38802ca, 0x20438b5a, 0x83259429, 0x27f41a1c,
    0x1bea4e0a, 0x71f6f2d9, 0x000edbd7, 0xec9c43a9, 0x14d05c98, 0x6cae4b10, 0x2b98548c, 0xb1b8f199,
    0x6609b31b, 0x01a83f1a, 0xd7cd6bf1, 0xcdc05aac, 0x4563645c, 0xb2f21647, 0x9dd067e1, 0xddc552a1,
    0xe4d74549, 0xdee68661, 0x377dbf8b, 0x19fd20df, 0x9be4bf81, 0xf233b120, 0xb9a7f5b2, 0x518174aa,
    0xdc449df9, 0x10d801b6, 0x571d8d6f, 0xe0200ff1, 0x307579f2, 0x1762575c, 0x4c312370, 0x8172a7f6,
    0x30efedb8, 0x100f390e, 0xb209bb4a, 0x7f7cbdc3, 0x6bc05623, 0x0b8eb935, 0x87e8b373, 0x1ecc8151,
    0xdac5e482, 0x25e1ee4f, 0x45855f55, 0x396acb3f, 0x0d46ee45, 0x08db6a68, 0xe160a5de, 0x737c3ecc,
    0xb8c259ce, 0x63321ec6, 0xb3430745, 0x3f9d372f, 0x8854a053, 0xc86ecb8d, 0xf1a376c1, 0xfcb9b4e9,
    0xc0ff5d23, 0xc07b4a44, 0x153becf7, 0xa0b00881, 0x59e40020, 0xbe90b195, 0xb114d512, 0xbc0384dc,
    0x9eede62c, 0x2d270718, 0x89b5b7c7, 0x9fdb4a94, 0xf7fcb63d, 0x26e3102b, 0xbcf5c9a4, 0xe2a9fac0,
    0x991aecbf, 0xbee05a97, 0x93b0b0b4, 0xa7e95779, 0x6f3474d3, 0x82aca76e, 0xe410aef9, 0xf2a1de17,
    0x1027db7b, 0x1d6a2743, 0xdd688e23, 0x3384fa66, 0xe870f432, 0xbf14c8cd, 0xbd9f2a82, 0x8c19bebf,
    0x05beca60, 0x52cc302d, 0x3bfc0ed9, 0x54d969d2, 0xb9f848d2, 0xb7c4088e, 0xbf9a4451, 0x6252a2c6,
    0xd581fe26, 0x113a56e4, 0x2db069b8, 0x20d9619d, 0xf2be599c, 0x59464609, 0xaca0ddc2, 0xdcf254e3,
    0xbc70ceb9, 0xbab47e16, 0x286ea72a, 0x441425de, 0x242ddf44, 0x045496a4, 0x4c149afa, 0x42e8cb9c,
    0x7426d794, 0x0b0521d2, 0xaa0c84e2, 0xdd978dbb, 0x6609b31b, 0x01a83f1a, 0xd7cd6bf1, 0xcdc05aac,
    0x4563645c, 0xb2f21647, 0x9dd067e1, 0xddc552a1, 0x8422add0, 0x11503da6, 0x13e2632f, 0xa80c5a70,
    0x9be4bf81, 0xf233b120, 0xb9a7f5b2, 0x518174aa, 0xab70f8ef, 0xf52c16b1, 0x65b0f19d, 0x2e8ba4dc,
    0xb7cce837, 0xc22560c5, 0xe13500ba, 0xaf723f71, 0x70d05920, 0x4bb534fc, 0x0e498f2f, 0xfaaa7d5a,
    0x6bc05623, 0x0b8eb935, 0x87e8b373, 0x1ecc8151, 0xd6c7f58c, 0xe8119ae8, 0x2c99fdd7, 0x93c706c4,
    0x18f6d3a7, 0x111e91bf, 0x4be15647, 0x81f10c4c, 0xc066943b, 0xb88101e1, 0x37117789, 0xe13a0224,
    0xdf6e72ed, 0xe2efe1e8, 0xe54d21eb, 0x84a32d62, 0xfee307a9, 0x49d9962d, 0xd61711f8, 0x84e6a406,
    0x51bbb4b4, 0xb5b2d98d, 0x795091c7, 0x545e70a8, 0x7b4cf3c3, 0xb33ffbfc, 0xeb4c5716, 0xb2318071,
    0x91e996a2, 0x98e95c9c, 0xf5de2863, 0x46b732e6, 0x1f46e71f, 0xda6eef72, 0xa2bf320c, 0x3897d421,
    0xb721ef6c, 0x07904b25, 0xab44196f, 0x6b66fe8c, 0x1e8ddcd1, 0xcb3416c9, 0xc9958b30, 0x67c38eea,
    0xe5a2e4eb, 0xb1316011, 0x764f154b, 0x2de07d4b, 0xf90447c6, 0xa9ff93bd, 0x8d61b983, 0x47e1a5c3,
    0x717e8cbd, 0xae4001ea, 0xe4ddc632, 0x062fd027, 0xc364f6c2, 0x32a1440c, 0x4a6bcd0b, 0xf2a025b6,
    0x7003ce23, 0x0318f711, 0x1b8245ba, 0x4f4a1b84, 0x0e39aae1, 0xdc49a6f6, 0xac972daf, 0xd2af1009,
    0x0141dc66, 0xddfbaf3c, 0x972a34a6, 0x70a6ae29, 0x979bac65, 0x31c288a2, 0xb88f09fd, 0x7bd90859,
    0xff641494, 0x493ede71, 0xa095602e, 0xb827de89, 0xb241d676, 0x040fac99, 0x0ba6d611, 0xd794fd69,
    0x11f6a3c4, 0xe5c1e7f9, 0x6d25141f, 0xd64aa209, 0x3bf2a950, 0x610e4231, 0xab95cc1b, 0x70a34ca7,
    0x393d4e9e, 0xa636a6eb, 0xf88aef8f, 0x3aacfd91, 0x40cdb65a, 0x9c8ea3b8, 0x3f614132, 0xa1228c41,
    0xf0eb99b1, 0xca387410, 0x2db5d016, 0x3d192698, 0xed29acff, 0x79fe205e, 0x75b03764, 0xcfb5106c,
    0x0e79ee55, 0x9b1d3a3d, 0xb072d738, 0x6bb7d645, 0xf9ed19a9, 0x88fbeec9, 0x1015690b, 0x6d10adff,
    0x85b58a26, 0x6ccd43e0, 0x97d38875, 0x2c4e8652, 0xbf45c95b, 0xadecc7d7, 0xc6437856, 0x8b720ebe,
    0x76503b76, 0xe70929c7, 0x3970ad7b, 0x375e5d8f, 0x2d808417, 0x13346836, 0x86eb73ed, 0xd8b0f74d,
    0x60a4fa80, 0x1e0d817b, 0xbf8ab1a6, 0xf2182e1a, 0xcacc88f3, 0x21447dcf, 0x34a9867d, 0x085dc3da,
    0xd4f9ae1c, 0xc8223cad, 0x718cabb4, 0x9152c9ac, 0xb3572d1a, 0x4485d590, 0xdbc56e41, 0x053d6cae,
    0x2dbc6fc7, 0x4990eb0a, 0xf3518264, 0x5ecb4e30, 0xac835e8d, 0x70cc86ed, 0xbc5e5fa0, 0x18a8f8e8,
    0x4fa22a7e, 0x0628499a, 0xcefc21ff, 0x13d9d203, 0x36e43458, 0x2451db63, 0x223fc6c1, 0xcb3cb3df,
    0x7d055ce3, 0x36447fc5, 0x6213205f, 0xf8915b22, 0xb4442398, 0xb392b7e7, 0xe5b76930, 0xf465ec76,
    0xbb8d27bc, 0x45ee8279, 0xa7cc9403, 0x3cdc8977, 0xbc996824, 0x4ba561dc, 0x5e252616, 0x93be46c1,
    0xf8c95cbb, 0xc8ef0287, 0xd36a326b, 0x41be20c0, 0x3cfa9d7c, 0xde945b32, 0x39d9e229, 0x5df9aa48,
    0xa8f12651, 0xd7cc8840, 0xf6694568, 0xe8acb869, 0x1447d915, 0x3fae9c93, 0xe6d6d765, 0x4052e53c,
    0x9a986471, 0xfe463415, 0x94e0fdf7, 0x6ff12dc3, 0x2452e86a, 0xeda57b9b, 0xde30379c, 0x3fa6779c,
    0xbe785f08, 0x3b1a4ec5, 0x57ac740a, 0x93287328, 0x827871df, 0x4363a7cc, 0x9e49a79a, 0xdf3c0709,
    0x801bcb43, 0xc6080362, 0x5e84f98c, 0xef7d7502, 0x11f7b53c, 0xe6421050, 0xb03b8a36, 0xad2f9459,
    0x367e4296, 0xaaa8a464, 0xdb5a2eb1, 0x78794af9, 0x275813e4, 0xf6cead82, 0xdd721e7f, 0x9e6e185a,
    0x1e824c82, 0x74344be0, 0x68eb60de, 0xbc24634c, 0x25fa5da9, 0x6944793e, 0xf73d1c20, 0xb0068f6f,
    0x97a7cf89, 0xf7bc3c2b, 0xf02f76cc, 0xd71a2b16, 0x69682459, 0x40106d56, 0xde857b1a, 0x7c44622c,
    0x96ad3001, 0xe1063061, 0x97c71250, 0xc2983610, 0xd7a7603f, 0x1aaa973e, 0xc4f4dbd0, 0x5a251f86,
    0xb40e12a9, 0xe0a866b3, 0x29edeca3, 0x0bbafce6, 0x5626c576, 0x9b6ff383, 0xd293ee62, 0x2bdaee9d,
    0x389c3d46, 0xe059e911, 0x8e225ab1, 0x51a5bbd5, 0xd0b58d33, 0x6b87f1df, 0x3c4ec475, 0xa6dd9d90,
    0x46c1750a, 0x46cd9ac4, 0xc1ade7cf, 0xed89fdb0, 0xdb15a5fb, 0xf888d4af, 0x3631b4db, 0x10b1ce17,
    0x20d1e3b0, 0xc33a853d, 0x6d0f6067, 0x790dddc7, 0x0321fad3, 0x28e4d4b4, 0x7e1a2a00, 0x2fc19e57,
    0x8cb18f70, 0x3854c750, 0x18c076b0, 0xf013ce0e, 0x18798b91, 0xbb38034c, 0x45b0dd1a, 0x723f0fbc,
    0x02cd3f2d, 0x4f4244ed, 0xc3e1009b, 0x3de2ece1, 0xed4de0c3, 0xf484d8c3, 0x1100064c, 0xfc86f281,
    0x83336f21, 0xe5067e3f, 0x49178d46, 0x02a0ef4d, 0x0a637737, 0x117526d9, 0xae34e6b2, 0xd0865fd9,
    0x12075501, 0x1e798553, 0x8f3aa0e8, 0xbdbdc64e, 0x563041fb, 0x9bcebe51, 0x407be402, 0xb4db9b2f,
    0xdd2c5e66, 0xfe8f321a, 0x185d49a9, 0x2470a1d9, 0x960776b5, 0xdea1d67e, 0xb53cbd10, 0xba500812,
    0xf370fc7b, 0x5fe845d9, 0xf47e39a6, 0x9331e6d0, 0xd1306a23, 0x34dff4b4, 0xd6e19118, 0x6bf6d035,
    0xdc7c2656, 0x93cd7d39, 0xa5afe151, 0x26af46ef, 0xd0eaae68, 0x5c3c6df7, 0x208b9fe5, 0x70448227,
    0x41c500a3, 0xc80ad576, 0x335211e3, 0xe6d8e6b7, 0xd557f996, 0x9204b065, 0x87c5e04b, 0x6c6637c7,
    0xd09a366e, 0xd1eba766, 0x9328f694, 0xd7b0d7cb, 0xb6853752, 0xb4038bcc, 0x7248dccf, 0x94028d02,
    0x09bd32d8, 0x6464092e, 0x3c14430b, 0xf55b3eb1, 0x8882b6c0, 0x3b6194a1, 0x40eee892, 0xcb51cb31,
    0xc1cfde22, 0x8c7cdd38, 0x521f223d, 0x0f0b67f7, 0x8b881036, 0x13b79245, 0xbea01322, 0x1f6f1f42,
    0x801bcb43, 0xc6080362, 0x5e84f98c, 0xef7d7502, 0x11f7b53c, 0xe6421050, 0xb03b8a36, 0xad2f9459,
    0x7bb3471d, 0x2a37fa46, 0x8a8dcdeb, 0x6899db38, 0x8d548adf, 0xc54ecab4, 0x59666d6b, 0xbe36a585,
    0xcb37be8e, 0x177ac832, 0x76a36b97, 0xd9e6edc7, 0x25fa5da9, 0x6944793e, 0xf73d1c20, 0xb0068f6f,
    0xaa8d5c45, 0x10544161, 0x3d8bf8b9, 0xcece8b85, 0x284676b0, 0x5706fb43, 0x3c158d4a, 0x3cc7bd26,
    0x19c32153, 0x17867c22, 0xb857f87d, 0x6e16f504, 0x75a7343e, 0x4d7dbea7, 0xb3cd3bd8, 0xb303b2c1,
    0xce29b636, 0x4b33e22c, 0xc02b64e5, 0xbb1672bb, 0x5d9a4469, 0x443aa157, 0x148646ff, 0x67a63790,
    0x28aa044d, 0x76fda714, 0xbae3e6f0, 0xe7ebff8b, 0x32015fb5, 0x8e43bde1, 0x2ac55d1b, 0x3ccd678c,
    0x794fa4eb, 0x3b5f1a69, 0x969f7eec, 0x86bb5ef1, 0x59403023, 0x5ae43aad, 0x39af4bb9, 0x8b4b4596,
    0x7bec58a3, 0x304bc7dc, 0xcd97b45f, 0xfe416e23, 0x12e1623c, 0x01d2cbb8, 0xcaac403c, 0xe00fba7c,
    0x1d3c4580, 0x8c819892, 0x95e5e775, 0x20ae9dff, 0x08fad250, 0x6d6cd214, 0x79910661, 0xe580fa6b,
    0xe0eaa387, 0x6cfffdad, 0xa5e211c9, 0xcb8f9677, 0x6456041c, 0x53be0a0a, 0xaa568a1c, 0xab3d9a84,
    0x6c53ff39, 0xee2e2bf0, 0xcd043ee8, 0x2f1a0483, 0x3ec71f26, 0xb8752e4c, 0x09a46a3b, 0xafe8b48f,
    0x158d51fc, 0x4436cecf, 0x8db2886c, 0xaa3232f1, 0x4b7da8aa, 0x4119c816, 0x91c4ed77, 0xb66c3f84,
    0x638e30cf, 0x5362222d, 0xc5496ead, 0xf34f596a, 0x92ae0098, 0xf51a7420, 0x793b3aa4, 0x7a93a6d4,
    0xf136ff67, 0x6bfa0574, 0x6f6d6999, 0xdccf9925, 0xcd3c25bd, 0xe6eaaab3, 0x7834a5d7, 0x74c34583,
    0x6bc05623, 0x0b8eb935, 0x87e8b373, 0x1ecc8151, 0x337841f2, 0x1cf2460f, 0x90cac15a, 0xbe3aa13e,
    0x90878e87, 0xc2896503, 0x1985bbc2, 0x0359b81d, 0x353d642e, 0x9a826efa, 0x6ce387be, 0xe9552193,
    0x93e33d24, 0x9948b779, 0x6f24fc6e, 0xea665971, 0xdc085352, 0xda266c7f, 0x77f8dd37, 0x48c5d4cd,
    0x90feefd4, 0xf9177519, 0x13dd5263, 0xa41bcc1a, 0x95d4dec5, 0x1cc6bbfd, 0x6465fd1d, 0xd372a973,
    0xef544185, 0xc95915d1, 0xf6ab33cd, 0x506fae62, 0x295a436f, 0x31668b64, 0xe4a7581c, 0x7f455722,
    0x6de13325, 0x0239e5d6, 0xcf05339f, 0x5e9bac8d, 0xc238f791, 0x7504e6a3, 0x4e99f2f7, 0x15275ae5,
    0x2f183c0a, 0xc48f6959, 0x6b0d3c2f, 0xcfd96e85, 0x76624f17, 0x58cf9244, 0x02d34a6e, 0x0cd7879e,
    0xf6ecb1d7, 0x65ab382d, 0xcd794cdc, 0x26c56657, 0xfdd1a39a, 0x5ceece45, 0xd1159154, 0xc3426fb3,
    0xd23de23f, 0x274d54fa, 0x0b5d8478, 0x34a92cdf, 0x5fd0f5bc, 0x3c3e2d03, 0x6f9fddb8, 0x904484af,
    0x6688a5ea, 0xfc47f47d, 0xd67a7a41, 0x9ccc0c8e, 0x9eede62c, 0x2d270718, 0x89b5b7c7, 0x9fdb4a94,
    0x2fd4f0b9, 0x5ecdcb5d, 0x471e2de3, 0x3e1512f6, 0xd56b08f1, 0x1863e1a6, 0xa9cbd58f, 0xe094ddbf,
    0x57c315ff, 0x13f8d6ac, 0x13b2bf36, 0x5b4d280d, 0xd76ac2d7, 0xf7bd1e64, 0xe577cdb6, 0x39a14f35,
    0xf88dd836, 0xe8a40ce6, 0x54de78d1, 0x18cf5757, 0x9e979137, 0xfeba9073, 0xda385419, 0x169ce7fb,
    0x86c4eb45, 0x3ad3e7d5, 0x8dcdd5e7, 0x8e79fbea, 0x652c4317, 0x4740903c, 0xf22e940c, 0x7d800b35,
    0x774c39c7, 0x317fc4f7, 0x6f3402aa, 0xb39d92ff, 0x40bc0fa3, 0x38e28f78, 0x3404d9c2, 0xf38598d4,
    0x26678d99, 0x31fa9925, 0xe8b44c05, 0xd6a342f7, 0xb3b7c682, 0x113f2edf, 0xdf2d84bc, 0x6558da6c,
    0xf7e053a3, 0x69ba9545, 0xde2674d3, 0x04b59f24, 0x35ac7986, 0xed06efee, 0x32c01410, 0xce8bc429,
    0x6ddb4b19, 0x06cbf60c, 0xab3ca0f9, 0x5476055a, 0x26d9254e, 0x699d2380, 0x7cd05fc2, 0x127e50e0,
    0x51b6c92b, 0x9d0f68fb, 0x9d80c6ec, 0x6e3ee5a4, 0x02f2a6a6, 0x652a544d, 0xd2d60a2d, 0x7237ed8e,
    0xc1207ccb, 0x31096146, 0x551853ff, 0xcc5376f9, 0x33ecfeee, 0xfabe9ec5, 0xd901669c, 0xf7790b31,
    0x1e06a744, 0xa25ac425, 0xca06f1e6, 0x9e383b7e, 0xe9229f2c, 0x009b56b3, 0xf5bd396c, 0x02fac4f2,
    0xa0b88880, 0x872b6a5d, 0x93df5dda, 0xc4419891, 0x5f54a3f8, 0x66748625, 0xe2c14820, 0x733663de,
    0xfcdb4e3b, 0xaa88eccc, 0x757ab66c, 0x2fb223f8, 0xc837a957, 0x50f8a56d, 0x7c35b767, 0xf5f1c20a,
    0xbfbc098d, 0xa4f4da51, 0x98502fbf, 0x6f302c0d, 0xcb1b2952, 0x0781d78b, 0x00444715, 0x8d3def57,
    0x8b7653ec, 0xf51c0fd3, 0x38ac4b76, 0x0f21e922, 0xa155307b, 0xe87a37e6, 0x0ebb5ef8, 0x2e0166e1,
    0x244204b2, 0x420d7833, 0x54a4cd85, 0xe485ed0e, 0x73c3bbfc, 0xd7f3179d, 0xdda2eb74, 0xc460197a,
    0xe1d5bc0e, 0x80ccab33, 0x646dda74, 0xe970c2bb, 0xd60d2d08, 0xfc4f2bc9, 0xbb29b60d, 0xed9a4a01,
    0xd6e3504e, 0xd67f258e, 0x06d22ecd, 0x73e156cd, 0x8fbe3318, 0x290addd4, 0x18615f32, 0xf0efe528,
    0x44a2238d, 0x47dfc62b, 0x4f36ad9c, 0x9a97e6d0, 0x21ab38c0, 0xc3a842cc, 0xa2fdc4bc, 0xc39aac41,
    0x1fdca3b0, 0xff1be877, 0x4485bbeb, 0x954adfd0, 0xe34a1059, 0x83779eab, 0x9d3336d3, 0x118e1658,
    0x3eb7fb8c, 0xd0c830d5, 0x9b81196e, 0xbe46b8a6, 0x858b392a, 0x2755b6e4, 0xe331974c, 0x5a5a3a96,
    0x48daf31c, 0xf9da5415, 0x3a694e46, 0x060994d1, 0xe2d3674f, 0xd5cb55de, 0xdee64557, 0x55dc0ce6,
    0xb4914531, 0xea9c619a, 0x50c0161e, 0xce55bedb, 0x2b6361e3, 0x2c8e1251, 0x79be3daa, 0xa56de244,
    0xf9ccaa1b, 0x6125f760, 0x7d67b9c6, 0x779091b3, 0x30f4aa1e, 0xa457922f, 0x721d051d, 0x1f4e94eb,
    0xd4241c57, 0xbee86736, 0x174f6e58, 0xd39f758a, 0xfe6a9d1c, 0x0ae2a8d3, 0xd85ccd29, 0x8210a769,
    0x7e4a6b99, 0x1477b7a7, 0xf0e7fbb0, 0x48ab2ad8, 0x3618b353, 0xb5073e29, 0xb8921aa5, 0xbeb378ea,
    0xf7323f5d, 0xa6a70ded, 0xb12ac279, 0x361f7ba3, 0xdf8c9171, 0x25f75c01, 0x882f5bba, 0x310e41f5,
    0xf494ef9d, 0xdd852242, 0x739b6c25, 0xe7afc879, 0xbb058b4a, 0x15a6b22a, 0x871c67c7, 0x8ceb38b8,
    0x555b9525, 0xe4be3a10, 0x9e3ec624, 0x1d6f01d6, 0xc1b47091, 0xade793b9, 0x3ac151d1, 0xea5a16a7,
    0x4974a60f, 0x4b990f8f, 0x1addc71d, 0x88e526e3, 0x156fa063, 0xf5e5722c, 0x6bfab773, 0x5d20996f,
    0xe34f55bd, 0x552cd81e, 0x3b8b3b81, 0x61508bc2, 0x547c010c, 0xf4a13837, 0x86c55e71, 0x1e5c19fd,
    0xfc3dfab7, 0x4992407f, 0xda7f9881, 0x4ac0e73f, 0xc44f58bb, 0xfdae5d5c, 0x4fb0720d, 0x37e173f3,
    0x45bf3ee9, 0x26898366, 0x78bc06e8, 0xb1484e9a, 0xbfb1dda7, 0xde41f1d0, 0x6010f64d, 0xfe79c07a,
    0xc92b12d5, 0xd2422ba0, 0xc4307c8c, 0xeb182bac, 0x5b28dafe, 0xeaf84145, 0x7144e40a, 0x17e9997b,
    0xac5d37a3, 0x37ad78e1, 0x4509d0b2, 0x60d47b72, 0x3095742f, 0x8498be3e, 0xc2dba396, 0x14a2b664,
    0x89263fd3, 0x6cb3f950, 0xc5d46b71, 0x99c3845c, 0x1f75b435, 0x0e14b3c9, 0xc4355a26, 0xe23714e9,
    0x2cf01cf4, 0x911fe879, 0x894456b4, 0x0adaba8c, 0x7c521062, 0xeeb2a972, 0x114d2bcb, 0xdc16190b,
    0x38bc37b1, 0xd9936b41, 0xbca9c405, 0xb6aaaa86, 0x51f7fb6a, 0x3d1c7843, 0xd4b7fa8c, 0x59f9613f,
    0x3d40ffb3, 0x3637d224, 0xa9ea23a6, 0x1cc09802, 0xbe4a550a, 0x081653b2, 0x1c75b932, 0x800b6f3c,
    0x1634c893, 0x6666970c, 0x59d1dd02, 0x4a76a012, 0x4e7e7c23, 0x5720402d, 0x5e1ca9d8, 0xd511efc6,
    0x14c9213c, 0x6e3bc7ee, 0x2496a451, 0x949a410f, 0x7a7b5bae, 0x30fb9174, 0x275cb32a, 0x16a3ca02,
    0xd1858554, 0x3759f101, 0x473cb6e5, 0x490ffb93, 0x399bd82f, 0x2aae1d97, 0x14675a6d, 0x91e7c59a,
    0xe330fd7b, 0xcdd518d3, 0xecc61789, 0x42c6241d, 0x7271b16c, 0x1f8c9a1a, 0x4cdec980, 0x5586cc30,
    0x833b847e, 0x8fb94728, 0xdbedeb94, 0x354f6dd6, 0x36004141, 0x4f3c2f48, 0x9e171f8a, 0x21ae4f5a,
    0x88bf6431, 0xc8f8c297, 0x6680c969, 0xb3b567f3, 0x0cfd8412, 0x76edc066, 0xd3fe03db, 0x6ae6b148,
    0xee6e570b, 0xa60f5e0c, 0x1959f41f, 0xbb2282ae, 0xae9d8486, 0x09810df1, 0xecb0fe75, 0xc87e22d0,
    0x98d914f7, 0x1057e561, 0x42470569, 0x38b38873, 0xe0fa5054, 0xec91b88d, 0xbbc7cc8d, 0x5f19b689,
    0x430c9440, 0xc62e2885, 0x4ba375a2, 0xe691f5fd, 0xb70dc8f1, 0x0d004c23, 0x09c97183, 0x7b2f7fac,
    0xbb0c840a, 0xe80189c7, 0x2c902fa3, 0xdc148f32, 0x7ad1bc06, 0x6d6c3291, 0x842f8658, 0xba653565,
    0x36cf497a, 0x7252f735, 0x6974f5c4, 0xd5017130, 0xa375e9a9, 0xb21c880d, 0x103e5d1c, 0x3e17fa26,
    0x7c88b39a, 0xcfe26caf, 0x5d0616d3, 0xf0efd116, 0x67ba694d, 0x6711eb94, 0x9f647c8f, 0xd5a73e9b,
    0xfb0851a8, 0xf99aeac3, 0x1bc4f82c, 0x99fb05de, 0x980cc1b2, 0x40ef7db9, 0x8568f3f2, 0x77ae2128,
    0x32b22d4e, 0x7a1e1ac3, 0xee737373, 0x13420c3e, 0xb95a9102, 0x80ec2c04, 0x50487c04, 0x8e42b0f3,
    0xbded4aa2, 0x93b6aecb, 0x0bc4ca2d, 0x1d560dbb, 0xadf9e0e4, 0x5e2ff0ea, 0xd939ab33, 0x76352e94,
    0xdc6589c0, 0x8a31a5fc, 0x4b937144, 0x82e94ba3, 0xf4414d23, 0xe7077aa1, 0x216b83c7, 0x5451b684,
    0x4bfac603, 0xda4c5801, 0xb89e6541, 0x0ea7d41a, 0x6140166b, 0xb156a213, 0x034cb078, 0xd001749d,
    0xb791ccee, 0xbffc2eda, 0x2ae80bd9, 0x42f5ae54, 0xa8cddb1c, 0x6f2bc811, 0x3c10a557, 0xd61e1bc7,
    0x3fa01e2b, 0x48d10bf6, 0x9bc8108e, 0xb670179c, 0x60909611, 0xb28e37ba, 0xc1d6168f, 0xeb7fdc41,
    0xccc3a3ba, 0x1d95b55b, 0xd59f1e06, 0xdbab40d6, 0x7bf47388, 0xa0d2d5ae, 0x39106114, 0x460adbf6,
    0x307c6274, 0x8f072288, 0x7c3b3928, 0x3a17948c, 0x51f8ed54, 0xcae445a2, 0x29c5d0c9, 0x860aac72,
    0x8b8aa383, 0x9cd21ab3, 0x18aaab91, 0x2fc40618, 0x365eb3ea, 0x8d0f005d, 0xea6348dc, 0x697bee7d,
    0x764814da, 0x00f23bbd, 0x6d32d26d, 0x982e006b, 0x6468c1aa, 0xc54ce8e3, 0x211b9c5c, 0x62d6e35e,
    0x2dd8ed0b, 0x45ef2703, 0xccda518a, 0xee0b69f2, 0x8118ab0c, 0x75f738b8, 0xc00a6ab9, 0xe7d9d1bf,
    0x7376dd09, 0x543d7793, 0xef92f897, 0x710e9e2a, 0x59bd1d0b, 0x3fc92da9, 0x94bf9c15, 0xb20b790f,
    0x0499ca32, 0x5d3e0083, 0xcff549de, 0x27f93bf6, 0x06fab182, 0xbc56fa3f, 0xcd27bf27, 0xba609a31,
    0x899b15f2, 0x60302929, 0xd9dc4b17, 0xd20e7fa6, 0x0616e592, 0xd71c25e6, 0xb492863b, 0xfe0e0e36,
    0x14d59edc, 0x28c72f69, 0x12086dcf, 0x19d49d85, 0xe2726c11, 0x039f4d15, 0x483a4d82, 0x0cfa9dad,
    0x51dbe603, 0x381c75a4, 0x8bcdca1a, 0xb4a4430f, 0xbcdc8700, 0x9d3a7560, 0xa5a134db, 0x8f600764,
    0x9781bb2b, 0x34116e6c, 0x22e6934b, 0x914cca6b, 0x06b12108, 0xbe1837cd, 0x8812b0e8, 0x1bb6f132,
    0xa89f59a8, 0xf42a5273, 0x543d8160, 0x7401b607, 0x92481af9, 0xb0038c01, 0x515b2205, 0x5b4932ea,
    0x13496c21, 0xffd26210, 0xa2ed16f2, 0xa089d42a, 0x07d03754, 0xcb5ed090, 0x87c27940, 0x136c62cd,
    0xba8838db, 0x9a334eb7, 0x317f29c6, 0xaeb15c17, 0x594b912b, 0x65ea6641, 0x3d11c579, 0x2c8d298e,
    0x7f6eac8f, 0x16118c79, 0x763de081, 0x07d1eb6c, 0xc2da3ad7, 0x1595220f, 0x96276495, 0xdc3b1fb9,
    0xde812b88, 0xe4949475, 0x8bd185e5, 0x97446807, 0xc98be8a5, 0x5b3e5a12, 0x5b0c9f48, 0x470d99c8,
    0x208339bf, 0x8a7119ae, 0x5bd0cf08, 0xa44b9e76, 0x6609b31b, 0x01a83f1a, 0xd7cd6bf1, 0xcdc05aac,
    0xf5928c1d, 0x63eb74d3, 0x9c3e6202, 0x5d6adeb7, 0x6f7082ce, 0x7125bd07, 0xb37201d8, 0x9740844a,
    0xd9316fdd, 0xab4bace9, 0xdbc639a9, 0x7c6791a9, 0x2c03fb25, 0x9bc9ea62, 0xbaad4a9e, 0x2e75d2fc,
    0x1cdf4e8e, 0x1fcd8ba6, 0xf1549a00, 0xc53ca4fe, 0x5ec42e16, 0x901b5cc8, 0x53c96240, 0xb419fd80,
    0x5ab888bb, 0x3f238258, 0x17115bbd, 0x0d015921, 0x708d50f8, 0xf0b3e72b, 0x7b9d9553, 0x712c56c1,
    0xf70ff745, 0x3a95c219, 0xea94bcef, 0x19cf5283, 0x99ca880d, 0x2359e506, 0x588b2313, 0x6a6b3a68,
    0xd841ffcf, 0xec0297f0, 0x9d3f0c72, 0x987ecaed, 0xc0ff5d23, 0xc07b4a44, 0x153becf7, 0xa0b00881,
    0x1d0e73d6, 0x3ababae2, 0xe85867fc, 0x8af4eb3f, 0xa828ec60, 0x6ad161cb, 0x9dd96cc9, 0xeca339e2,
    0xe305c6b1, 0xea3f0b0b, 0x798b0e38, 0x4c043247, 0x4b3699d5, 0x8b81b508, 0x561a8075, 0xef7f5c27,
    0x38470a9a, 0xc9204563, 0xf846d20e, 0x63eaa5f2, 0x5809fa8b, 0xb2a26bfc, 0xddc7bbdd, 0x1aaa7c10,
    0xf5d6b8d6, 0x7143b05a, 0x92e32d1f, 0xdd8658d3, 0xc26fc1c1, 0x86958f1f, 0xafcc9982, 0x90395e42,
    0x594b912b, 0x65ea6641, 0x3d11c579, 0x2c8d298e, 0xffa1998c, 0x2362ffc4, 0x699f49c0, 0x8a3ecfee,
    0x964228b2, 0x38083980, 0x8758c779, 0x52058f47, 0x7c2fab99, 0x58dc032d, 0x9955025b, 0x4697e419,
    0x2fd5eb5c, 0x0ce617ff, 0x4dd27ebc, 0xbfbb59ee, 0x13ee302b, 0x51e7c31d, 0xd0332601, 0x65bd86fe,
    0x6609b31b, 0x01a83f1a, 0xd7cd6bf1, 0xcdc05aac, 0xf5928c1d, 0x63eb74d3, 0x9c3e6202, 0x5d6adeb7,
    0xefa59ba4, 0x3ba4401f, 0xc92258b3, 0x7a91d8cc, 0xd9316fdd, 0xab4bace9, 0xdbc639a9, 0x7c6791a9,
    0x9cd02338, 0x3336235d, 0xfa50366c, 0x22840d99, 0x45117155, 0xfc5e93a4, 0xb32de591, 0xbc87b7b2,
    0x61ccabcd, 0x12202b97, 0xd6ada1cc, 0x3cb00fae, 0x62a09e9a, 0x24695f5f, 0xfc92728e, 0xae4260a6,
    0xbfa723d3, 0x697f9602, 0x61b538d0, 0x14a1ebd6, 0x5315d992, 0x2e9bfa82, 0xe2f59b4c, 0x9f3f0429,
    0x0d6bbf4b, 0x052b4ebc, 0x57755c7a, 0xdd63c184, 0x01f5f44f, 0x8858bc3d, 0x91187f68, 0x8f629bbc,
    0x94ff4a4c, 0xb24d2d3e, 0x69332a6d, 0xc83be425, 0x43345029, 0xbdc13b27, 0xdf7d40af, 0x629338a8,
    0x46834468, 0xff10e915, 0x961356d3, 0x04bdd485, 0x42219840, 0x18e61d05, 0x956af162, 0x3355a400,
    0xa134d0ca, 0x0d425cdb, 0x53ce5c4e, 0xc0ff8a06, 0x57bfb242, 0x9fffeeda, 0x1d92603b, 0x950664cb,
    0x13e3eb96, 0x43fa7fa6, 0x705df3f2, 0x087c038b, 0x0e97c2fd, 0x06f50c67, 0xace352f8, 0x021dc0f7,
    0xf0771b6b, 0x2f1fac23, 0x3e7e7d5f, 0x78950d8e, 0x20a95aee, 0x7d7edd30, 0xf7610f83, 0x9c02482c,
    0xbe05bdd2, 0xb4377227, 0x0dc6a73d, 0x64aafd34, 0xb952a490, 0x2b8194c7, 0x5178b818, 0x8b92c8ef,
    0x76103d20, 0x2e02288a, 0xc87ee5d8, 0x34850a15, 0x2860fea7, 0x9a82641c, 0xa6a83178, 0xa345a2b6,
    0x407ca758, 0x38b7ae5f, 0x708b73b9, 0x1196e80b, 0xdf639b36, 0x07739b91, 0x158debee, 0x36f185e9,
    0x68f4129d, 0x55ca03d8, 0x6798a119, 0x5213004a, 0xeb4d2462, 0xdc35927a, 0x2f08dc5c, 0x62bf47e5,
    0xbbd3db4d, 0xbb6de1d7, 0x2d97a462, 0xb7e47fd7, 0x6bfa0726, 0x69a5f363, 0x1febb411, 0x0ecfd247,
    0x1c7cff7f, 0x28b65b30, 0xa913e3a4, 0x22447505, 0x5a7884e8, 0x32b7402c, 0x0c6f681a, 0x86a0c22e,
    0xee0b9a98, 0xc46a5f8b, 0x24502a02, 0xa40fb9d6, 0x2a1aea32, 0xc04e4cbb, 0x67aa7e59, 0xf6c2272e,
    0x271078a1, 0x72074cf0, 0xaebfaac3, 0x8df1dd33, 0xfd151cbd, 0x944bbc0a, 0x0fe10c40, 0xb1b8bb62,
    0x0cef612a, 0x83dfff0f, 0xeadb2cf7, 0xe3493e7d, 0xe1885c93, 0xe8b49afc, 0x1fa41d0b, 0x99feb604,
    0x697c5ab7, 0x1fc445c2, 0x890630b1, 0x8a5ad9c6, 0xd5cf5a80, 0x2cb2cbd0, 0x31efac0c, 0xd92e7efa,
    0x03cc72cc, 0x5cafa6fe, 0x853641ae, 0xd74e4c0f, 0xa3ed6d0c, 0x65b5e9d0, 0xbdf0a13d, 0xfc204be6,
    0x145e5955, 0x76487517, 0xc59ac710, 0x0d233b47, 0x8e7999d9, 0x3b403c2d, 0x0add2c6d, 0x663db4ff,
    0x41166431, 0x2fe9cf48, 0x44c860f1, 0x26fa6542, 0xc6458bc9, 0x83a0cc22, 0xc649c5cb, 0x6a042250,
    0x534b60d6, 0x35f46634, 0x7a803a59, 0x20becf21, 0x834f2846, 0x2dc5cf07, 0xe0798a1a, 0xee817f5e,
    0x9bbb9c7f, 0xe797a8eb, 0x598e7255, 0x8cbcd4ec, 0x98304c37, 0x46b27e3f, 0x134313d6, 0x8dca3cab,
    0xa8e7d3b3, 0x75788bf7, 0x82bf7d88, 0xf96a0470, 0xf2ac6830, 0xce4469c5, 0x0686e350, 0x0c5dc013,
    0x0eb31d9a, 0x45cb7810, 0x60f38815, 0xcb9abea1, 0x999d9480, 0x2953078e, 0x7c8bfdb6, 0xa3350767,
    0x305f4067, 0x0eab96f9, 0x128e969b, 0x6849e399, 0x0a094a5e, 0x1fea2322, 0x7f0c99aa, 0x842d69ed,
    0x8baf8967, 0x7bd9045a, 0x9d9b1f85, 0x7a02a7d8, 0x4dbc263b, 0xbf360e97, 0x20bda855, 0xc99e16c2,
    0xe948cabf, 0x8bd28fa3, 0x6ca74665, 0xe5d1364b, 0x68e85fb3, 0x10c38d5b, 0xc6363844, 0x5929f0f3,
    0xf11e7c94, 0x8e324f43, 0x4cc7f14f, 0xd6cdcf8c, 0x0147a657, 0xfdec62e0, 0x5ca13a99, 0xd114dde2,
    0x5ab888bb, 0x3f238258, 0x17115bbd, 0x0d015921, 0x82e8ea80, 0x30ea145a, 0xaf24229e, 0x8246cc79,
    0x73d4bcda, 0x716deb7b, 0xb5546dfe, 0x7ebf6623, 0x397b2b3e, 0x5b8899a3, 0xfbba2f8f, 0xf354944d,
    0x1359baa0, 0xe6b48284, 0x7f9eb587, 0xec709cec, 0x12361acf, 0x0d18b8f2, 0x1858a0a3, 0xeb1ce86e,
    0xe6fd7124, 0x7b2bbd43, 0x36327aa3, 0xbb34f125, 0x689d3297, 0xe8fbd8bf, 0xb1340957, 0x500d7148,
    0xc4a4612a, 0xdfa8d5f1, 0x73e56646, 0x01f28770, 0x53f68ebe, 0x483f60e9, 0xf71fe5ca, 0x2e50be53,
    0x0adffb1a, 0x0b3b4f42, 0x5a666c03, 0xda509a48, 0x92f73ffe, 0x471e20e5, 0x65ae029e, 0x35e93920,
    0x615748b2, 0x3965cc1e, 0xdd723a91, 0x9b71899f, 0xa11bb0a4, 0x37d03eef, 0x04b05ecb, 0xb27c331e,
    0x8298a580, 0xa83beb6f, 0x9b66834f, 0x7232aef3, 0xae209cee, 0x293386ac, 0x1c4332c3, 0x13ad1e85,
    0x3d5174d1, 0x8b342616, 0xa4071ac2, 0xe2b1a91f, 0x76282aa3, 0x5f611100, 0x765e7a0f, 0x06ff0cd7,
    0x3c8d5312, 0x9aeb2a13, 0xee6bb453, 0xeb8e6290, 0xa917ee31, 0x86b04ff7, 0xe0c70284, 0x21c9ef13,
    0xc111acde, 0xdf7e2362, 0x130e2118, 0x9f2a1ef3, 0x482d0ab5, 0x7cf44a4b, 0xb72d95fc, 0x0e90d0c0,
    0xe7ab774b, 0x5a6d7ba2, 0xc9a1f167, 0x829de8c3, 0xf74093ec, 0x07818172, 0xaee65f9f, 0xb761a66d,
    0x22b4406e, 0x026940f2, 0xa8d3f4fd, 0xed604be7, 0x669760c6, 0x5441e115, 0xfd7fa8f8, 0x3f3c6903,
    0xd6bcf805, 0x87dcf6c6, 0x4bacde49, 0xa69f1f58, 0x9e96f08c, 0xd825ab1e, 0xd10e0b9b, 0xef6ecff1,
    0xbded4aa2, 0x93b6aecb, 0x0bc4ca2d, 0x1d560dbb, 0xb3917c8f, 0xc24921de, 0x6eeada01, 0x5f4d20e6,
    0xc067a60e, 0xa5b61991, 0xc2fb5482, 0x651f3145, 0xb0d6242c, 0x7e560bfc, 0x1c109c4c, 0x6d2848fe,
    0xf810a041, 0xc7285c18, 0xa8572109, 0x859c1c23, 0xcedd1917, 0xf5eaea86, 0x3e8367fc, 0xe44f6a23,
    0xc2e0e0aa, 0xaccecfd6, 0x4b455d89, 0x92b1a312, 0x47af89fb, 0xa02c7668, 0xe97c41b5, 0xcdf4acf4,
    0x9efb69f2, 0x49f62753, 0x323401a2, 0x403de0f1, 0xe7e093c3, 0xcecf84cd, 0x50875e8c, 0xb9ac540b,
    0xa9f0e9e1, 0xaaed8e29, 0x0b78cdad, 0x6e25c77f, 0xe4a586a6, 0x63e1e3a2, 0xef238fbf, 0x924587b2,
    0x91cafbba, 0xf163f257, 0xf7885cb0, 0x293c4a12, 0xa73f07fb, 0x2916e1e7, 0xefd0ba82, 0x1fd87eb4,
    0xace3e1e5, 0x401b2268, 0xdff2777d, 0x058482ff, 0x7a3ae62a, 0xc32b26bc, 0x3b5fc686, 0xd5ab2453,
    0xfbdece50, 0xbdc5e75c, 0x72911b52, 0x446cd7a0, 0xd6a4889b, 0x4ec0b0b0, 0xd5986e9d, 0x307cc7d0,
    0x7ddb01b6, 0x9ccf896e, 0x59c4db80, 0xb016f559, 0x42219840, 0x18e61d05, 0x956af162, 0x3355a400,
    0x292d23c8, 0x97d3f3d9, 0xb3dbe3c9, 0x2681ce27, 0xb8c58d82, 0xf2eb13d5, 0x96f06705, 0x4b217b21,
    0x3b0f8633, 0xb7d68d9f, 0xe2431340, 0x7b9b5c85, 0xfc30ee63, 0x2ee486f1, 0xf24736de, 0x39ed84c6,
    0x95e03616, 0x429c795f, 0x804151a5, 0x5b6ce9c2, 0x0e97c2fd, 0x06f50c67, 0xace352f8, 0x021dc0f7,
    0xf0771b6b, 0x2f1fac23, 0x3e7e7d5f, 0x78950d8e, 0x94d435ef, 0x3ed44f14, 0x8e3d5dff, 0xbc678987,
    0x91319c5a, 0xef24ec5b, 0xb5b776be, 0xfce9e4c1, 0x18ba2e71, 0x0bb59572, 0xe70707b6, 0x89d84c8e,
    0x4bfac603, 0xda4c5801, 0xb89e6541, 0x0ea7d41a, 0xf83ac73f, 0x2435828f, 0x9fa2e6e4, 0xfa05dbcb,
    0x7113f9bd, 0x72807aa6, 0x2866e868, 0x78981595, 0x1f76aa1d, 0xb80a689f, 0xf23b08c0, 0x1aea1026,
    0x943fd732, 0x120d22cd, 0xffc006c9, 0xe3302423, 0x98d2e173, 0xde62c363, 0xdbab08cf, 0xa09831a0,
    0x45d64535, 0x8dad053b, 0xa39b3552, 0xe66e9387, 0x7fe47476, 0x27678f81, 0x706c5088, 0xb8a59c80,
    0xc35fc8af, 0x576b8044, 0x13480bf5, 0x1d4b9366, 0xa8695c57, 0x23a6daad, 0x4e813718, 0x3b61f93a,
    0xb37b7a18, 0x2cfbeda1, 0x042f1d8b, 0xe0e97eb2, 0xf20e6a3d, 0xd556c7c7, 0xa63f23b6, 0xa32a0425,
    0xf49164a9, 0xd3fc6628, 0xa41c4b3c, 0x30bdbf17, 0xc65a99b7, 0x7ecc13d8, 0x99f45064, 0x70a917b5,
    0x731d10df, 0xa21f9eb5, 0xe746fa4e, 0x34d43be7, 0x81a452ac, 0xcc73ba0c, 0x1de7b7ca, 0xf509f5c0,
    0x2f9f63c2, 0x5f3852e1, 0xa8ec79f5, 0xe3577b48, 0x84e62579, 0x405dc081, 0xceffd627, 0xb8e11a38,
    0x4ccf160f, 0xcad81a91, 0xd245d622, 0x4d31f95f, 0x82e4182a, 0x8985ec06, 0xc60b011e, 0xe24f4b5c,
    0xc4463e32, 0xec34e4a6, 0x616c11a8, 0x69bdee54, 0xca955e9b, 0xf0ab42a8, 0x3233cb5e, 0xe2faaefa,
    0xae431dbe, 0xb0c51bce, 0x0622dc33, 0x325f1d13, 0x347ff8b7, 0xee9d22f4, 0x13ce5004, 0x9a0752c3,
    0xf284e2b2, 0xba9ef6e1, 0x6e141e98, 0xc5039ec1, 0x8da6c98a, 0xb5ce5f2c, 0xbcb9a9ed, 0xe35c812a,
    0x864bae68, 0xf6503265, 0xdf48971a, 0xa8b1c6bd, 0x0d597546, 0xc2564040, 0x26bed24a, 0x1777151c,
    0x88b7c9a6, 0x84c13cb3, 0x286ea000, 0x850a2b9c, 0xfd47b045, 0x6da595f0, 0xbcc5b4f2, 0xe14f6fd3,
    0x23eb6e4e, 0x442f4000, 0x6b561d5f, 0x722189dd, 0xa25ad93f, 0x396b3e98, 0xcd6abf55, 0x478b3c4b,
    0x26298ca2, 0xf3d15179, 0xad83cd9e, 0x190fec57, 0x9f3b38d1, 0x2e4787a1, 0x3d323dea, 0x52d45f4a,
    0xa1cae708, 0x3cf50b6d, 0xd73107c0, 0x322a4210, 0x721ecf74, 0xb84f7aac, 0x04d8077b, 0xa74dd0e1,
    0x63a2da92, 0xebcfffe4, 0x67aeb64c, 0x48135ab9, 0x0591c62a, 0x48309b30, 0x6b9e3675, 0x4cf33773,
    0x3dca82ae, 0xa868485e, 0xf5205366, 0x05d02e1b, 0x0f37b183, 0x37ee73c4, 0xaa3ee3e8, 0x7b0e0f08,
    0xf84dde78, 0x109120ad, 0xcc23f640, 0x43d36af6, 0x77e6f035, 0xb8ed17a0, 0x64f84e51, 0x12474797,
    0x761348c2, 0x01994658, 0xfb34c292, 0x79eb8286, 0xd7f06059, 0xc4814140, 0xeaa1861c, 0xdd8a266c,
    0x738e20c9, 0x11fdacab, 0xaaaf4b7e, 0x057087a1, 0x3d5e00c6, 0xba165228, 0xc99c9d15, 0x6def8684,
    0x59b43c2b, 0x71e1833b, 0xb02433c3, 0x786be755, 0xb6853752, 0xb4038bcc, 0x7248dccf, 0x94028d02,
    0x87d892ba, 0x6015ea02, 0xf1179444, 0x07bc1723, 0x4675272f, 0xa8fdd626, 0xf3fe545b, 0x63cb75d5,
    0x63a2da92, 0xebcfffe4, 0x67aeb64c, 0x48135ab9, 0x0591c62a, 0x48309b30, 0x6b9e3675, 0x4cf33773,
    0x2c79a018, 0x76edfde7, 0x7bb90d1d, 0x7d6a4995, 0x5227b86c, 0x476ad94a, 0x3bcb3dba, 0x4aa3d325,
    0xf84dde78, 0x109120ad, 0xcc23f640, 0x43d36af6, 0x625e6255, 0xd51645b6, 0x2084d82d, 0x84a34428,
    0x14615bd5, 0xb31000ac, 0x1c8d0645, 0x13487def, 0x8bfde723, 0xa236c02d, 0x745c0416, 0xe309b43a,
    0xe29dc9d5, 0x575fd477, 0x100f43f1, 0xb633e209, 0x3d5e00c6, 0xba165228, 0xc99c9d15, 0x6def8684,
    0x931f3476, 0x0bd206b1, 0xd4321994, 0x9a05e0b3, 0xd57abb58, 0x1027de4c, 0x7307cc16, 0xa7e0d9fa,
    0xebe344bb, 0x8ae0e9d4, 0xd46c45bf, 0xa3fbe7d8, 0xc9079188, 0xa27ea083, 0x5a3fc505, 0x6512a212,
    0x6d0d7b39, 0x1ad036cd, 0x1c235520, 0xbdee8c25, 0xfab33b03, 0x5f2cdf53, 0x8a116068, 0x4b367638,
    0x3617a848, 0x7575e8b5, 0xa9331e07, 0x6246d6dc, 0xfbdece50, 0xbdc5e75c, 0x72911b52, 0x446cd7a0,
    0xd47bd906, 0x3b40e764, 0x6eae034f, 0xc671f4d8, 0x84773c2d, 0x47d53b9e, 0xd3987c3e, 0x8cfddcee,
    0x9dcbf9cc, 0x473dfdd4, 0x350d4f87, 0x3f6fa8e6, 0xde4071a6, 0xd0b797aa, 0xc64e5437, 0xb22ed02b,
    0xd0a53d0e, 0x25705778, 0xafb290cf, 0x02030e54, 0xc5e30a62, 0x5bd6ef0d, 0x13b157c8, 0x0b6810a3,
    0xdaac1db2, 0x6f9ecb30, 0xbac50400, 0x5f96c81b, 0x3aea0e7e, 0x21a93e0e, 0x5f50853f, 0x1359093c,
    0x3434570d, 0x1f98e7b9, 0x2ee18a4a, 0xd35af0ad, 0x7d055ce3, 0x36447fc5, 0x6213205f, 0xf8915b22,
    0xb4442398, 0xb392b7e7, 0xe5b76930, 0xf465ec76, 0xfc03fb23, 0xf7ea51fb, 0xc294966d, 0x119c2422,
    0xcfaf4334, 0x43b8731d, 0x88dd6981, 0xd14b70ba, 0x50b53f58, 0x68dbaf66, 0x80c8a6d6, 0x6db39f2d,
    0x834f0956, 0xe6436150, 0x10230e98, 0x90f8aa4a, 0x19218c2f, 0xc4c17b35, 0x94a12a1f, 0xa163b95b,
    0xb86aa8b9, 0xbbc3120b, 0xba4426b4, 0xed57622c, 0xee6830b5, 0x0fe4d52e, 0x29c9bea6, 0x00c2a5f0,
    0x11c66171, 0x94f17202, 0x0a777ca2, 0xbeb3b5da, 0xd0d7c596, 0x71fff725, 0x09119dc4, 0xd94bfaa0,
    0xea2fc63c, 0x14a4fe56, 0x3c63260a, 0xf8453343, 0x61aac0ea, 0xec88dbd7, 0xc1801f55, 0x54174039,
    0x2190487c, 0xce13ad35, 0x3048ed7d, 0x38e35f97, 0xb4244c78, 0x3de552e2, 0xb2ee6b9f, 0xbf91c116,
    0xac6278ea, 0xdab4fa17, 0xc71e7a7a, 0xb58f0e35, 0x778ce706, 0xd67b4c60, 0xd76574c8, 0x49aebfd5,
    0x79c35ae2, 0x8fd0acff, 0xd6c99f35, 0x29576381, 0x1638eba0, 0x7155aa43, 0x3eec1735, 0x3ef1aff6,
    0xc347b33b, 0x1ac0e41e, 0x2c944af8, 0x5e8a9d43, 0x70424b92, 0xf2d19f69, 0x05b0b70d, 0xc39e7092,
    0x70de80b2, 0xd026d5b1, 0xc7423e9e, 0xc501ea68, 0x84d0c4ff, 0xfddf8ba1, 0xc3bae46d, 0xe9c7ec4b,
    0xfad32bf2, 0x9149ee41, 0xd4d9b015, 0x6a13cb35, 0xeef26cf6, 0x08c4fb98, 0xed1f6723, 0xb486cac5,
    0x70865971, 0x73a0c5a1, 0x6c27461b, 0x61db82d5, 0xb9f848d2, 0xb7c4088e, 0xbf9a4451, 0x6252a2c6,
    0x84e6cf64, 0xf2299d80, 0x2cc935ca, 0xfdb6f617, 0x41075444, 0x44b5694c, 0xee889fb5, 0xe59951f4,
    0x9766396a, 0x560341de, 0xfde39424, 0x3703091e, 0xa2b64b45, 0x8ea6c9ef, 0xa9d40d84, 0xcad04498,
    0x2e7608e4, 0x5218949f, 0xee7fcc43, 0x397d6333, 0x1052ee85, 0x663ac501, 0xfc286e70, 0x2d59d57d,
    0x4497df80, 0x07296b34, 0xe940cc74, 0xa905bd91, 0x04cc38c5, 0x6b3f912b, 0xbcee6cce, 0xf6ff3ba0,
    0x235b93ab, 0xd461a899, 0x8f39e163, 0xd641d9b8, 0xf04072bc, 0xba1046e9, 0xe5113b7e, 0xe3a7f8cd,
    0x4a24b8f8, 0x4be0be3e, 0xe97f11b3, 0xdc3dac83, 0xab2221b4, 0x78f7c57e, 0xe899c20c, 0xb4464cae,
    0x1ff68c86, 0x4d969fb8, 0x4abe6db1, 0xec751da1, 0xf4a8ec5f, 0x8df0c228, 0x9a26142d, 0xca707655,
    0x5a3d8df5, 0x67925c95, 0x7c352167, 0xeae8d17e, 0x6b961b24, 0xae91bd2f, 0x60bee5ef, 0xde835216,
    0xa00edbfd, 0xbc6657a4, 0xe1ccde5f, 0xb39b158f, 0xa6f4495b, 0x80489acd, 0x6976f8e8, 0x957556cd,
    0x42166ea3, 0x340195b0, 0x4615548b, 0x7e5495e7, 0x67918107, 0x8db8c1b9, 0x1d08ea80, 0x74aa9ba6,
    0x3f400d97, 0x179c64c4, 0xc757b570, 0x3d13aa67, 0x70c885f3, 0x525320b2, 0x33515090, 0x29d0374d,
    0xbc9d7158, 0x4c6d327f, 0xa659a2b5, 0x0f063bd8, 0x9c4ff9b7, 0x661d09b4, 0x48edd61e, 0x8211087f,
    0x9d79c25f, 0x6d9e054b, 0x06344334, 0xe9b2cd95, 0xb5987229, 0x4c549ad8, 0x5e1544e3, 0xc8cfdaf6,
    0x3dea0845, 0x2a12f995, 0x9ce60655, 0xf4154129, 0xd10937a3, 0xe68273bc, 0x7207643b, 0x85b18bf0,
    0x2a03bdee, 0x2d09e659, 0x4574a0a7, 0x369ee273, 0x44149559, 0xd886c5b6, 0x1d9bc00d, 0x19cea5c8,
    0x36004141, 0x4f3c2f48, 0x9e171f8a, 0x21ae4f5a, 0x639be031, 0x771fd0df, 0x95e27c1d, 0xa08ecbc9,
    0xa4f780a8, 0x37b1a727, 0x691f9c2c, 0x0056071d, 0x2458998b, 0x718fbff2, 0xbc97c3e1, 0x4aa6ec3d,
    0x33c3df88, 0x6da0c391, 0x08d98eca, 0x9abd7b56, 0x73d20b44, 0xbf30e58f, 0x22dcabe6, 0xd2b22506,
    0x49c4ac4a, 0x84ba0ee5, 0xd7ef1136, 0xb329aeb5, 0x4d7f16fb, 0xb62153a1, 0x112d036e, 0x66ea4af7,
    0x6b6f5ed5, 0x0febb4c5, 0x45e7be37, 0xd6338881, 0x0b8c8f69, 0x9fc32b69, 0x888eb8ff, 0xaacea397,
    0x86f0ddd1, 0x7a335acb, 0x9f790ae8, 0x1f1bfdd7, 0xe04b3a5d, 0x1f73eb92, 0x5b304727, 0x8b760af7,
    0x48aee077, 0xfd58c02c, 0x4711f93a, 0x81966c44, 0x3aa1f5a4, 0xa64a46eb, 0x282bc1a4, 0x545e3977,
    0x589f33e4, 0x0731a473, 0xff9cd5da, 0xa35cd20e, 0x6d4dbda8, 0x5060fdbd, 0xb4f23633, 0x33cbb7ea,
    0x97b1a7a4, 0x56698d4f, 0x99b8b301, 0x26f33d94, 0x004b3a7d, 0xa116d141, 0x5a12d81d, 0xfae6aea2,
    0x77649891, 0x4f56937d, 0x2e977501, 0x1feb9bf0, 0xef0e40e5, 0xf1b4cf6f, 0xe5c02cef, 0x137b871d,
    0xcc967a5e, 0x4ea656f7, 0x7292bff0, 0x41da8be8, 0x826ca080, 0x119b599b, 0x0c093a60, 0xa35a8306,
    0x9ccc0923, 0xcd77ba5a, 0x17e25f51, 0x58faf6ad, 0xc0ff5d23, 0xc07b4a44, 0x153becf7, 0xa0b00881,
    0x9b2cab48, 0x9788aeab, 0x2436f6d9, 0x983a48ad, 0x6855f31a, 0xc19353b8, 0x07710ee1, 0xdf95e6e0,
    0xe42c2edf, 0x97b2cdd3, 0x3279cd20, 0x2a76b787, 0x36004141, 0x4f3c2f48, 0x9e171f8a, 0x21ae4f5a,
    0xdfd0cee8, 0x2a49d02e, 0xa00307d8, 0xdb4afdb0, 0xba64cbbd, 0xd1b9e8ec, 0x29b92343, 0x8c8d62ef,
    0xbae03628, 0x0eb2b323, 0xf2ea7eae, 0xd93beff8, 0x61617cfa, 0x607e0291, 0x767a57e9, 0x280f7907,
    0x73d20b44, 0xbf30e58f, 0x22dcabe6, 0xd2b22506, 0x03f4e997, 0xfc6f322a, 0xc0db6cba, 0x401cbc75,
    0x4d7f16fb, 0xb62153a1, 0x112d036e, 0x66ea4af7, 0x2a791ae8, 0xaa20a8cb, 0x695be332, 0x953745a0,
    0xeaf9519a, 0x7b496e94, 0x0af39a3a, 0xb7985dc0, 0xab368f60, 0xbc1c193f, 0xe43aaab7, 0xbb314156,
    0x0b996b9f, 0x2593963f, 0xc5c3864b, 0xe58ea854, 0x84e0de0e, 0xc003a516, 0x475d0c42, 0x4d38895a,
    0xf38186bb, 0x4af0ebe5, 0x450327ff, 0x3b393862, 0x4d363bbe, 0x2499e6b6, 0x91c76e15, 0x1f1833b3,
    0x05c04b1a, 0x080e5854, 0x03441d3e, 0x6cc1b944, 0x907effac, 0xb9be1a85, 0x09a26929, 0x277ccc0c,
    0x96be6234, 0x096a3232, 0x6d2f5d09, 0x8569c4b6, 0x64e79a9f, 0x2f7be1a7, 0x939c3f1c, 0x2601df5b,
    0x8e269574, 0xcc17d910, 0x7a3a563f, 0x18bd399b, 0x563041fb, 0x9bcebe51, 0x407be402, 0xb4db9b2f,
    0x3c4b868e, 0x3206822b, 0x5bafb1df, 0xef434db1, 0xb7908b96, 0x6534f8dd, 0x5d7b9f71, 0xfddff13a,
    0x4c4da642, 0x5a7f4797, 0x9a70b6e1, 0x6aa588e8, 0xd599bfc7, 0x51b45723, 0x8cbd43b5, 0x1616e121,
    0x588d6546, 0x17ea8949, 0xc9f180fb, 0x91dfb612, 0x61aac0ea, 0xec88dbd7, 0xc1801f55, 0x54174039,
    0x2f01a110, 0x9e3960b3, 0xa5491823, 0xed9b0c3e, 0xb213eb8f, 0x1b8809df, 0x1cc727a0, 0xde6a0780,
    0xde146e96, 0x1641d404, 0xeb202f1b, 0xa38f919d, 0xc948d7ce, 0x712fd993, 0x692cdf90, 0xf14e843b,
    0x751de1f2, 0xb5da9650, 0xbfe94309, 0x000fefe6, 0xec58591e, 0xdc8d60e0, 0x2cb9c8a1, 0xe0e51e40,
    0x820e368a, 0xe0eef261, 0xd1eadfd6, 0x1f1c62b6, 0xb58a0b46, 0xf586a755, 0x968ad71c, 0x434989ba,
    0x7eb026a6, 0x66686426, 0x52cca441, 0xc39ace29, 0xcca9d591, 0x6838f4a6, 0xe8ddabcb, 0x1f656935,
    0x5523223a, 0x67e3950c, 0xdc1b8c8d, 0x4d354768, 0x3e1c7986, 0x73ce7ce6, 0x0119ea4d, 0x88235b78,
    0x990fc7bc, 0x71d048ae, 0x5b6130a0, 0xa8a78091, 0x7d055ce3, 0x36447fc5, 0x6213205f, 0xf8915b22,
    0xf6ea8f22, 0xd1f52781, 0xc96ecdc8, 0x619b92ee, 0x04ae7d04, 0xdbf62251, 0xe80f0a05, 0xd400b8c5,
    0xb8b39d74, 0xe369f63e, 0xe15bda0d, 0x0226e09a, 0x7aa0c1c2, 0x6f8d6c65, 0x01f79d85, 0x393f42cc,
    0x21d20aae, 0xf0978cc6, 0xb72f9846, 0xd2dfec95, 0xab13e951, 0x14e0d9f2, 0xf3eeffaf, 0x6796754d,
    0x563041fb, 0x9bcebe51, 0x407be402, 0xb4db9b2f, 0xe0e7af56, 0xecc9c422, 0x58ae5786, 0x5e6083c4,
    0xb7908b96, 0x6534f8dd, 0x5d7b9f71, 0xfddff13a, 0xf66ec620, 0xbf37b8dc, 0xbf45bd23, 0x8543f1af,
    0xd7bbb20d, 0x3f565fd7, 0xfae9f8a8, 0xe805813f, 0x64b4cc81, 0x1e9b2506, 0x81af7779, 0x1a073317,
    0x3304fd8d, 0x5d72c604, 0x71284210, 0x733ef329, 0xe562044e, 0x30d841c3, 0x4506a531, 0xa513c6c1,
    0xfe14f503, 0xd25c19cf, 0x433ead9c, 0x3079bdb1, 0x1c4c7ce8, 0xa0b0a697, 0x0ab96eb1, 0xb3f58e3e,
    0x08a4b863, 0xcbfb51f4, 0x615a4837, 0xdc7077c7, 0xda700626, 0x06396a47, 0x34df26f6, 0x06262531,
    0x56aa4598, 0xc98766cf, 0x3053665e, 0xad8d1f0b, 0xc6997b86, 0x25d8aad2, 0x39bfb5f5, 0xf6af9a68,
    0x7fd4bd73, 0x18333645, 0x230f538b, 0x226b33f9, 0xa903677f, 0xa2093b86, 0xdf9a3e27, 0xc8cc4a99,
    0x7d5b47f5, 0x0ffb3523, 0xd782eb5c, 0x18ccb2bf, 0xe60cd576, 0xa5fe81e6, 0xc5bbd19e, 0xae5ba504,
    0xfef067a8, 0xe57aece7, 0x5b19f982, 0xfcafccc0, 0x716e91f4, 0x093dca62, 0xead17a61, 0xde2faf7b,
    0xec52f6b1, 0x527bd3d8, 0x7bb76baf, 0xcd72b25e, 0x5d43714c, 0x1da6b7bd, 0xfe47562a, 0x45c07606,
    0x542872ff, 0xecfcc247, 0xb4bc4fa0, 0x96b3798a, 0x44195a65, 0x7cccf9e0, 0x4c3c612b, 0x00f5300e,
    0xc3ad8f9d, 0xf1b52d42, 0xe87f22e0, 0x6ae9ee3d, 0xa8ae51e5, 0x6cb6ccd7, 0x2c6d0435, 0xc1a68219,
    0xb57dc660, 0x90b14ba7, 0x6e840e85, 0x1767ba4e, 0xe8ac2567, 0x88df6c0d, 0x37e452a3, 0x1486611e,
    0xa36c4326, 0x0a16e543, 0x276937aa, 0x82c0bde6, 0x3ae3b596, 0x601d74eb, 0x75f01a3a, 0xb396d52e,
    0xc1af8030, 0x69a8029c, 0x1de7680e, 0xd676d0b4, 0x61429cfc, 0x24eb0741, 0xb4edf8ba, 0x8bf0edf4,
    0xcda9d895, 0x542c5f4f, 0x81b10acb, 0x7dab29a2, 0xa74e70b2, 0x099096ca, 0x54fd8dfa, 0x2d6615be,
    0x6609b31b, 0x01a83f1a, 0xd7cd6bf1, 0xcdc05aac, 0x4cc12fb4, 0xe251391b, 0xd195f7c1, 0x92df9b28,
    0xed815892, 0xe84e0ba3, 0x6ee4ae62, 0x90033b61, 0xa4eb539e, 0x5f40f0a6, 0x4ca83470, 0xfa07e9fa,
    0xb0d6242c, 0x7e560bfc, 0x1c109c4c, 0x6d2848fe, 0xa31610b9, 0x2372ecf6, 0xf22e2da9, 0x84fdb34a,
    0x88beecae, 0xfd014d1a, 0xf1e1fde9, 0x1109fedd, 0x9d07b9aa, 0xc6487e10, 0x40785b96, 0x549f643a,
    0x8f797e06, 0x1ac6c603, 0x2dad4339, 0x805c1117, 0xc883f4ad, 0xf2aad94e, 0x1c3b2b9d, 0x280f0829,
    0xcfdf074b, 0x26487b78, 0x50047143, 0x7d0ca23a, 0x42fcd0ce, 0x18d1b392, 0x469b2e7e, 0x8a4f3bea,
    0xb6c02fb6, 0x2567891e, 0xfc512e9e, 0x22cdc7d9, 0xdcb67356, 0x29a87673, 0x340ff89d, 0xaf7280cc,
    0xed2b5c3e, 0x19e1edd2, 0x0b8b1dfa, 0x288352f0, 0x6e630d1f, 0xb3a843c1, 0x82cd68af, 0xde425512,
    0x41187bdb, 0xe23303cd, 0x2d0ca7da, 0x69dca165, 0xd0083da6, 0x13719b36, 0x02eb1353, 0x0feecac1,
    0x778ce706, 0xd67b4c60, 0xd76574c8, 0x49aebfd5, 0x4369c9f9, 0xb8894a29, 0x94fb1a3c, 0x4f8edaad,
    0x1885cf18, 0x127a6ff5, 0xa408d293, 0x91bf7860, 0xc9fa3862, 0x235b591d, 0x3094d9cf, 0x9fa00236,
    0x70424b92, 0xf2d19f69, 0x05b0b70d, 0xc39e7092, 0x95504df6, 0x96dcaefb, 0xf17c1677, 0x6955cb4e,
    0xee92468d, 0xe624b11a, 0x5ad8eba3, 0xee8ac93a, 0xf07b6a0c, 0x9d02166c, 0x59b018c3, 0x435601ff,
    0xdf01aff1, 0xefb0a1d3, 0x8d92db59, 0x7b429c81, 0x6fe442f3, 0xb731e583, 0x1228d430, 0xa7229dc0,
    0xd60d2d08, 0xfc4f2bc9, 0xbb29b60d, 0xed9a4a01, 0xe6af56de, 0xe80f051d, 0x5eb2f33c, 0x867968ce,
    0x8c660d18, 0xb145d46c, 0xed88fd39, 0xaaec3893, 0x9766396a, 0x560341de, 0xfde39424, 0x3703091e,
    0x142ed841, 0x56cb14db, 0x2f251d4e, 0x1e8327b1, 0x86c02713, 0x8beb29c1, 0xdcb87ec8, 0xecf4c2ca,
    0x7efe70bf, 0x3f5e66a1, 0x1c7d6905, 0x06fc3c01, 0x38bc37b1, 0xd9936b41, 0xbca9c405, 0xb6aaaa86,
    0xfb327d0e, 0x3e1f2a14, 0x184f2487, 0xe7c6fb8e, 0x6b7c8f46, 0xeb8089bd, 0x6a5237da, 0x2bb748e9,
    0x9432f25d, 0xeb0464fd, 0xeea005d9, 0xccce89a4, 0x7737b13b, 0xbec139ee, 0x65522dd5, 0x687af9f8,
    0xb9da5ec6, 0x40cccb56, 0xe17a2ca0, 0xee07a7d2, 0x6c7694f5, 0xe0a07838, 0x968ecc45, 0xf35ceedc,
    0x4ea673ab, 0xa4e7c24a, 0xa72e562a, 0x29fd0200, 0x320ec54b, 0x4eda138c, 0x247565e3, 0xaa22005d,
    0x02f00853, 0x5c9b845a, 0xb22331f4, 0x14ab9145, 0x7ec9451d, 0xe3a3fb95, 0x4ba5c1fc, 0xf14e810b,
    0x076bcab4, 0x69363b3d, 0xec08b0fe, 0x49031ab7, 0xb820e18b, 0xc8132084, 0x73ac413e, 0xc1b9cf3b,
    0x22957911, 0xfaa8fd48, 0x7529b632, 0xe998294f, 0xc634245f, 0x3dffaa01, 0x5c40caab, 0xc7b623f9,
    0x3ef4278f, 0x1a10ec77, 0xaf869ff6, 0x3440b8a6, 0xbde2e81b, 0x5532c706, 0x40aa1e6f, 0x94a92a41,
    0x3508b35d, 0x807bc116, 0x791e739e, 0x235435c1, 0x1ceee364, 0x4c61fd07, 0x653dfe0c, 0x727d91d9,
    0xae24feac, 0xf516e1ca, 0x5f1f12d8, 0x504c51fa, 0x3fffeeba, 0x73f4f48d, 0x21e393ca, 0x7c6a7ba4,
    0x81033873, 0x138b0768, 0x5a63a993, 0x5bed737e, 0x0c674d1c, 0xa42d04b2, 0x4b1c5168, 0xc1fd4d75,
    0xf707a738, 0xbd66c58d, 0xa8ff0534, 0x14ea4c4b, 0x7d9809b0, 0x638de06a, 0x0a614baf, 0xbe05a4e3,
    0xc986fbdd, 0x15bdf22c, 0x8b8b68a8, 0x350e4c1b, 0x0d7cb140, 0x8b0d86ec, 0x00c85c8a, 0x5e0fe146,
    0x4385e9ea, 0xeacc934c, 0x4780607d, 0xe5023a4c, 0x707a6bbc, 0x09a5e9b3, 0x9deb5354, 0x1ee6b49c,
    0xb9d3eab4, 0xd2870299, 0xf2945f4f, 0xc4e4d274, 0x23c14f05, 0xbcf90e9a, 0xe5bf5bc1, 0x92620bc6,
    0x5dd67a29, 0x5f3a5b36, 0xd4ddf297, 0xbf32f799, 0x6f8991dc, 0x75704c6f, 0x178f28c0, 0xe96e1e1f,
    0x9ef5feb0, 0xe94a6410, 0x3b3ac0e9, 0x7107f424, 0x94aa02f8, 0xaa62f433, 0x1b583991, 0xae4f3aa2,
    0x5f731e00, 0x64738b01, 0xef02be10, 0xd1acc467, 0xfb5e7a4d, 0xafd653ac, 0x6c4bb76f, 0xaa89d7d3,
    0xbb3e13f8, 0x0e1fb14a, 0x8347ec8a, 0x785ff2ec, 0x6bb9c3fd, 0x1e4a9fda, 0x7fe4866b, 0x5d035714,
    0xe5f4624d, 0x3d4c6e44, 0x570b0b0d, 0xc3d0aea6, 0x18640c54, 0x0733e7ab, 0xac70aac3, 0x37aa1be0,
    0xc88a1e84, 0xd30dfb1f, 0x49c7a6d4, 0x18d9efa1, 0x409d0358, 0x6ad75487, 0x265fac34, 0xbf78212b,
    0xc4bfe6a8, 0xd30f006d, 0x25bf4c8d, 0xcceee075, 0x293edc9f, 0x1e8b7c7b, 0x2fdff387, 0xfd0748e5,
    0x021adc91, 0x14207c0e, 0x5f1461b1, 0xbd4e3f44, 0x12f318cb, 0x68ca7da5, 0xb4356156, 0x44bc4be9,
    0x17c0e644, 0x8279c7d3, 0xb278dcc1, 0xf0598dff, 0xd971893d, 0x96239e5d, 0x3f2c515a, 0xaac698ec,
    0x7ea5cdf3, 0xb2ac647c, 0xef5c72e0, 0xdf32c3b5, 0x3ab33dee, 0x9293dca7, 0x60dd2fe5, 0x5a57e179,
    0x42417e5a, 0x9049f906, 0x5397f2e2, 0xd43f93bc, 0xf5c46dc1, 0x22e637fb, 0x101ff24c, 0x711b542f,
    0x741445e1, 0x8de86054, 0x506a88f4, 0x2b01b641, 0xc959df61, 0xdf7e9e48, 0xb0602c7b, 0x051f3645,
    0xec5c5f1e, 0xa69bb716, 0x55271800, 0xb4bf56d4, 0x77c7ab16, 0x1ea1a410, 0xe796076a, 0x34318b66,
    0x08e42b38, 0xee0301dd, 0xf3bb1386, 0x7d595385, 0x8b03448b, 0x2a94ec3d, 0x18168245, 0x04a4c056,
    0x0af8ab59, 0xecd58333, 0x577fcc08, 0x9419b8fd, 0x85b58a26, 0x6ccd43e0, 0x97d38875, 0x2c4e8652,
    0x0e915d2d, 0x88025f94, 0x249043f7, 0x51dc4972, 0x2a63e1be, 0x652c7707, 0xc736793a, 0x277279c3,
    0xf49164a9, 0xd3fc6628, 0xa41c4b3c, 0x30bdbf17, 0xadcb3c93, 0xa104c2a2, 0xe3dd2a7f, 0x612231f2,
    0xc2e6dbdd, 0x27af3b89, 0x0965d59c, 0x5f65f154, 0x9c2b5461, 0x16941ea9, 0x3e5837bd, 0x6d75c8e3,
    0xc281e847, 0xa38738af, 0x5bd168e9, 0x2474d43b, 0x94392002, 0x3fecce9a, 0x3fe18979, 0xe3380959,
    0x42cab0c5, 0x27037644, 0x0109d399, 0x20ee9601, 0x2045c3a3, 0xe0f4c948, 0xc516414e, 0x2ad4209c,
    0x090866cf, 0x9277576f, 0x56e4c2c8, 0x3a0a4216, 0xcb2c2d69, 0x0e3b8d24, 0x6f4e43fd, 0xc108581e,
    0x0f6a8bb3, 0x6fe248c6, 0x768e16a3, 0x65bce750, 0x85b58a26, 0x6ccd43e0, 0x97d38875, 0x2c4e8652,
    0x13bf8b3a, 0xf44aaa32, 0x93f0fb80, 0x392aa1e6, 0x71fe8d1e, 0xf9e3a3ab, 0x059fd35e, 0x5a83e3e0,
    0xf49164a9, 0xd3fc6628, 0xa41c4b3c, 0x30bdbf17, 0x26526e61, 0xe09a2cff, 0x8675f4d4, 0x99e34d83,
    0x0f905b2d, 0xc11c5139, 0x038317c5, 0x5accd250, 0xe46964f7, 0xb85c7613, 0x68b1abac, 0x3f716d57,
    0x826b2bc5, 0xcbcf1944, 0xb430316d, 0x3643b972, 0x19b38a11, 0x17ff8830, 0xc23ccb91, 0xdcfe23de,
    0xb45306fa, 0x52c944c6, 0xcf130aec, 0xb159f814, 0x0a8c511c, 0x77a6edd7, 0x19a9dbf6, 0xfdeea2ed,
    0x4c1680c0, 0x89c93852, 0x451d3cb5, 0xf7a5e350, 0xdac8d1b7, 0xa063e605, 0x420c7dae, 0xee3ca83f,
    0x8b3da802, 0x8abee13b, 0x8f35850d, 0x80024d8a, 0x027aeb5e, 0xbf72fb92, 0x26243087, 0x44dc898a,
    0xb34fdf8a, 0xbf9266ce, 0x793a38da, 0x81a4f549, 0xf486af9b, 0x44f46700, 0xf78fb4e2, 0x93aee5af,
    0x4e4d4011, 0x8760b002, 0x7666cbb8, 0xba2ad8e5, 0x01292641, 0x61d2ac75, 0x629587de, 0xf948506c,
    0xd1d33211, 0x18d810ea, 0x028ee0c4, 0x2301b1be, 0xbea2f81f, 0x2055bba8, 0x555e64ac, 0x5d331ef8,
    0x7b8c9d8c, 0x048c1a1e, 0xc6844eb8, 0xf3c4e869, 0xd8f96a11, 0x38e06876, 0x252d82d4, 0xbd83e1b1,
    0x4b065a6e, 0x41b8b160, 0x4aac1465, 0x79689a53, 0xe2c89fc5, 0x2130f735, 0xc0b2e481, 0xb49f0efe,
    0xed6d0ea9, 0x58c27c4e, 0x8590fa1d, 0xb4db51d2, 0x820e368a, 0xe0eef261, 0xd1eadfd6, 0x1f1c62b6,
    0x6a6174ae, 0xd055c28d, 0x1765ac7f, 0xfd6cc8fd, 0x69463e3c, 0x78e7b23a, 0xf11f1132, 0xc7c1f0ec,
    0x5aeac795, 0x10ff6aad, 0x95c16112, 0x2f3fbd9b, 0x578fe7b1, 0x876f4a3d, 0xae23044b, 0xea2c599a,
    0x3cc33a93, 0x2cd93e1b, 0x0b2babd5, 0xf4e80b74, 0xf0dfd28e, 0xeb1c8156, 0xb827ffe6, 0x77a99ad8,
    0x5e41c889, 0x045268aa, 0xf8d88232, 0x90a2dda3, 0xa1833d24, 0xa9562595, 0x7d406788, 0x744589e2,
    0x9e37f16f, 0x74b6e93f, 0x930d3e5c, 0x76ec4b37, 0xc558302b, 0xb73e3f36, 0x66596416, 0xa181be9e,
    0x53dfd96c, 0xba37e2f1, 0x0d33f693, 0x15c8775d, 0x2c408617, 0x05b5e575, 0x83272d38, 0xcd54556f,
    0x563041fb, 0x9bcebe51, 0x407be402, 0xb4db9b2f, 0x58085f1b, 0x2d0f9887, 0xa854cb60, 0x75ccd7ce,
    0x846b170c, 0x1ad79179, 0x9f075fce, 0x31e9dc6d, 0x75347773, 0x8bf2329c, 0x3468b49c, 0x1da6b751,
    0x875fcc15, 0xdbdfe2bd, 0x37ddcde0, 0x90ea50b6, 0x4497df80, 0x07296b34, 0xe940cc74, 0xa905bd91,
    0xf224f0d5, 0xba3dac41, 0x0f29a169, 0x94afb881, 0x15eb8967, 0x87102236, 0xa77e8e02, 0xff35c608,
    0x6477018e, 0x5da345e2, 0xa4a08f3b, 0x37cabdc8, 0x203cd9ab, 0x2f2711a9, 0x2e691372, 0x73976483,
    0xea43a14c, 0x0f6a6fca, 0xd5f3356a, 0xff57f863, 0x1a92b144, 0xb7bde766, 0xeb8332c0, 0x9fa09678,
    0x2e6f8555, 0xc9f47cc0, 0x4be6fbcf, 0xcfaaf09a, 0x9eede62c, 0x2d270718, 0x89b5b7c7, 0x9fdb4a94,
    0xe7666ba9, 0xf01d0443, 0xfe15a027, 0x6bc7c12e, 0xa6ff5dbc, 0x0a4363ff, 0x38cb0c3c, 0xf093f56b,
    0x3c4dd5b8, 0x22ae29f1, 0x830cfbc5, 0x1264ff72, 0xe141cdeb, 0xaeeb8073, 0x590ca633, 0xeb416c14,
    0x0a8263af, 0xa57b9d52, 0x84c8d640, 0x74d0882e, 0x96381762, 0x2ca58e54, 0xc06f8c93, 0xe9094ce6,
    0x25fa5da9, 0x6944793e, 0xf73d1c20, 0xb0068f6f, 0xffcb642d, 0x0439a54f, 0xe1923a1e, 0x0a5ae776,
    0x9b6b165f, 0x50c7b6d8, 0xf79c3160, 0x94b6c633, 0x3187d275, 0x5885b8a1, 0x532a25dc, 0x2cd214ad,
    0x330b2885, 0xc7de8512, 0x8eb61bdc, 0x6adee97a, 0x6b5ec344, 0xb67c0fa0, 0x18aa075e, 0xd75c5730,
    0x8de336c3, 0x72468f45, 0xb8fbd921, 0x09c32283, 0x73da63e6, 0x2129b39b, 0xfe2e2ba7, 0x43d4b963,
    0x214d6072, 0xb76bc507, 0x0bc53705, 0x4052f955, 0x7d07c9e8, 0x4ec40964, 0x6b32f56a, 0x5b2625d9,
    0x96516c20, 0xae74825d, 0xada49dc7, 0x32ace6f3, 0xd6beb0b8, 0x40f3c646, 0xbba9bd53, 0x49cc9f6c,
    0x84c8f64d, 0xff52240a, 0xd29f261b, 0xf516b88a, 0x23b8b709, 0xc568024f, 0xa8858991, 0x3c176964,
    0x3b58d793, 0x51ec5b85, 0xbdcb3000, 0x3fcf0838, 0xadc51ccd, 0x164415d7, 0x9b87d9a6, 0x6c76583d,
    0x4cd5a33d, 0xd19aec78, 0x1af9e555, 0xa9829879, 0x7c4f0862, 0xa5851699, 0xbd5de5f6, 0xae8a1110,
    0x4ef13206, 0x89959ce3, 0x22403006, 0xb9375e14, 0x2f0c047b, 0x48cbb2e8, 0xdb44b37b, 0xd347aa31,
    0xec746555, 0x6174582c, 0xe840eded, 0xccd86301, 0x5e15a117, 0x4729e14d, 0x487e5f85, 0x772100db,
    0xddbc0c35, 0xe9e3fc4d, 0x5f380dfe, 0x4244b7fa, 0xf3e3956e, 0x55ad2d8e, 0xdf9d0daa, 0x2022d4f6,
    0x1f1058d8, 0xfda14b8f, 0x40ece8ce, 0x3b5600c0, 0x826ca080, 0x119b599b, 0x0c093a60, 0xa35a8306,
    0x18ee126f, 0xb07a9c7f, 0xccee8589, 0x6ff7f149, 0x6bfa0726, 0x69a5f363, 0x1febb411, 0x0ecfd247,
    0x5de6bb99, 0xd5deddac, 0x4ff046ef, 0x6b318460, 0x33c2263d, 0x61c39451, 0xf6ffbe5b, 0x1bd758c9,
    0x832149af, 0xfe17a5ab, 0x4b0488bb, 0xdc309d03, 0x36004141, 0x4f3c2f48, 0x9e171f8a, 0x21ae4f5a,
    0x2fa612e1, 0x831c3ddf, 0xc3d662d3, 0x4876684f, 0x3a2efca4, 0xb4c7f9e8, 0xf8e0bfc1, 0x87e0f8f2,
    0x3678a9d6, 0xddfbee74, 0x629dc922, 0x08777fd8, 0xb73981b1, 0x2941aba7, 0xaffde690, 0x006b8184,
    0x73d20b44, 0xbf30e58f, 0x22dcabe6, 0xd2b22506, 0xfd5b4c69, 0x128f0188, 0xaecbf89f, 0x39415be2,
    0x4d7f16fb, 0xb62153a1, 0x112d036e, 0x66ea4af7, 0x455d759b, 0x97ec3860, 0xfea9cacd, 0x729ea953,
    0x33373b71, 0xfb1f67e4, 0x5adf7d02, 0xf3689a90, 0x4f7294f5, 0x63f5407f, 0xffe44e3d, 0xa29c0d65,
    0x8ca8a5bb, 0x878445eb, 0xe0327608, 0x6b460529, 0x48aee077, 0xfd58c02c, 0x4711f93a, 0x81966c44,
    0x99278deb, 0xb2897639, 0x233a3f52, 0xd91d3d84, 0x589f33e4, 0x0731a473, 0xff9cd5da, 0xa35cd20e,
    0xbf1b083d, 0xf2788498, 0x690063cc, 0x983fbe35, 0x3e7a5190, 0xb2aa91d6, 0xc5f2d2f3, 0x4183e00b,
    0x60817ded, 0xe408a9ed, 0x4edcfd49, 0xcc92cb57, 0x0a0881ee, 0xea9b7875, 0x2f6af517, 0xa13b3ba5,
    0x7e2997e1, 0xcb0ea601, 0x4536cfbf, 0xe1fa01bb, 0x722f67ed, 0xe828a4c5, 0xd3c152b7, 0x18ecadac,
    0x826ca080, 0x119b599b, 0x0c093a60, 0xa35a8306, 0x60bfdd2a, 0x5a01e1af, 0x46ba5638, 0x2ced6e3a,
    0x6bfa0726, 0x69a5f363, 0x1febb411, 0x0ecfd247, 0xfe75f4e4, 0xb1d5ed1e, 0xf2ff817b, 0x8c412ed5,
    0x307061f1, 0x6b8f221f, 0x8c1b8bf5, 0x583c4477, 0x3512c700, 0x1ee26495, 0xc2dac802, 0xea9953a8,
    0x36004141, 0x4f3c2f48, 0x9e171f8a, 0x21ae4f5a, 0xebf717ee, 0xb68e5789, 0x98a6a65b, 0xd67173ad,
    0x2da4d8aa, 0x2cbb699f, 0x7eaf39c3, 0xb461547e, 0xb66ecf0b, 0xf4b11bed, 0x5b61ec58, 0x43ad9985,
    0xd20c0f97, 0x930fa0cb, 0x3d509731, 0x2f9e6c60, 0x4ea23ade, 0xdb1d3d21, 0xb0787825, 0x741aa249,
    0x0e90d014, 0xfea931c9, 0x117425f7, 0x5d032fd1, 0x40ad03df, 0xdda3f09b, 0xbd083bad, 0xfe30f55f,
    0x4871ab35, 0xf8d0ba95, 0x00921e42, 0x7f7414af, 0x399e890b, 0xa5c798e9, 0x6aa6fd19, 0xe21b1787,
    0xcf427bd7, 0xc8372594, 0x108a15b3, 0x48dafa9b, 0xd7258f39, 0x62e6651f, 0xc4978379, 0x3a52bce5,
    0x3abd28e3, 0x236f2d92, 0xbc7d4fdd, 0x49f3d2b2, 0x7d055ce3, 0x36447fc5, 0x6213205f, 0xf8915b22,
    0xcc86dfdd, 0x1699bbfb, 0x848e5bbe, 0x7900d141, 0x1b06b5d0, 0x6b016c49, 0x505c4a7d, 0x32f97810,
    0xc0aef023, 0xcdc869de, 0x1ce4e6a7, 0x4b1ac124, 0x2b5504e8, 0xe1c78d65, 0xf7735666, 0xb7e24625,
    0xb32bfd03, 0x2e18ffc4, 0xa639f49e, 0xc9864861, 0xc51bdfa2, 0x90adb2a7, 0xdfd452a8, 0x3aac16c4,
    0x563041fb, 0x9bcebe51, 0x407be402, 0xb4db9b2f, 0x4f95814c, 0x709dcaaf, 0x137c6735, 0xcbdc5703,
    0xb7908b96, 0x6534f8dd, 0x5d7b9f71, 0xfddff13a, 0xa9599971, 0x8b91fd9e, 0x20625230, 0xde3bd678,
    0x1b744263, 0xe23382fd, 0x50c890fe, 0x40dc44ec, 0x82be7304, 0xfc431a16, 0x746bba1b, 0xcd1057e8,
    0x61aac0ea, 0xec88dbd7, 0xc1801f55, 0x54174039, 0x24347c4d, 0x2a56e13a, 0x26b9b49c, 0x49e25865,
    0x27fc9855, 0x7f3d13a4, 0x705f3d25, 0x07819298, 0xad9895b8, 0x5aa58910, 0xe78790a7, 0x69566b57,
    0x458798e8, 0xd22cd0e0, 0x88d803da, 0x24e25911, 0x751de1f2, 0xb5da9650, 0xbfe94309, 0x000fefe6,
    0x32d7fd09, 0x093ff3c9, 0x885d46b5, 0x84f57355, 0x820e368a, 0xe0eef261, 0xd1eadfd6, 0x1f1c62b6,
    0xf713c2c1, 0x44a5dbbf, 0xda254081, 0xcd7f609a, 0x78a1bb42, 0x78969f3f, 0x1444ca46, 0x63cd93b1,
    0x31f0b852, 0x7c968628, 0xbbcf6ef0, 0x3d5e3c99, 0x795c8652, 0xb769197f, 0x3669da19, 0x978802ed,
    0x3e1c7986, 0x73ce7ce6, 0x0119ea4d, 0x88235b78, 0x313276bd, 0x75569ec6, 0xa42df187, 0xf6573af9,
    0x7d055ce3, 0x36447fc5, 0x6213205f, 0xf8915b22, 0xc62b6515, 0x0c361db5, 0xa8644231, 0x5e06c319,
    0xd3e0d4a6, 0xf87ecaf1, 0x1e990d59, 0x4c529a04, 0xa653d94a, 0xfc6b07e4, 0xd363efe3, 0x62a783b7,
    0x6542ed16, 0x3cf16ece, 0x8e9ef034, 0x41b2ee8d, 0xc0ff5d23, 0xc07b4a44, 0x153becf7, 0xa0b00881,
    0x13b1eb18, 0xe320a8ac, 0xe8d2d7ba, 0x39caf2a5, 0xbe9233f9, 0x7af53b1c, 0x6b9e7746, 0x5e6fb164,
    0x859c3204, 0xb985dfba, 0xf2fa170c, 0x00751568, 0xd60f29ed, 0x4d7fcce5, 0x8d8e4328, 0x9e724e46,
    0xf04fb8f6, 0x8cef05b6, 0xe2bdd3ae, 0x3bb304f5, 0xae7f4eb4, 0xda3963f8, 0x43f972bb, 0x01e3ceb6,
    0x86495623, 0x639ddc6b, 0x72c05670, 0x24abc426, 0x30df92ac, 0xe388aaac, 0x11ea132b, 0x5ca0e036,
    0xc89c04fd, 0x21cc4193, 0xb4d4d973, 0x86ce6fb4, 0xccfe14c1, 0xebc29540, 0xf2126605, 0xb65b4c77,
    0xcc6c5bc2, 0xc4caf7ee, 0x5b97c34d, 0x56dd9c94, 0x3928ee51, 0xe494c367, 0x9a17baaa, 0xa9bf6954,
    0x8836076d, 0xa829bd5c, 0xedababe0, 0x27ebc04a, 0x737c4664, 0x316cc9b8, 0x0aff1203, 0xccda7381,
    0x88ec4b1b, 0x1a6a84cd, 0xfacfe3f9, 0x6477101f, 0x40518fc6, 0x58904d1b, 0x18e98f02, 0x67c79af1,
    0xa43aa1f6, 0x799d4bde, 0xe0c69fe3, 0x2139b082, 0x12d1191f, 0xeda58bd3, 0xa3d0b79b, 0x3b74ad67,
    0xe95ea7a1, 0x45a2953c, 0xeb1bb039, 0x7e0ea68e, 0x71fccc67, 0x2ee8c041, 0xd922a0de, 0x305a42be,
    0xe524bb0f, 0x192a7ef1, 0x21f81b48, 0x7b261e57, 0x710107ee, 0xf0ad771a, 0x23f63666, 0xe5110c39,
    0x26b17f0f, 0xdaf903b7, 0xf771f331, 0xf70a3667, 0xfe7b089a, 0xbcdaec6f, 0xb2ce15ad, 0x3e81a2c0,
    0xb2beab51, 0x70ea0f86, 0x46b37eb5, 0x409c96a4, 0x0368d98c, 0xcb7380ae, 0x6713aadf, 0xab671ab7,
    0x0c4e6d87, 0xa3d57cb7, 0x2765ebbd, 0x091d5b03, 0x2dc6067d, 0x344f5570, 0x6245a1c7, 0x8cad8215,
    0x51d925b4, 0x60015c6f, 0x337be486, 0x8c0f8b11, 0x594b912b, 0x65ea6641, 0x3d11c579, 0x2c8d298e,
    0xa2e2c5dc, 0x502471d1, 0xced87e99, 0xfa9d670e, 0x2cbd5ab5, 0xa460861a, 0xb01bff54, 0x19d56588,
    0x9766396a, 0x560341de, 0xfde39424, 0x3703091e, 0x881bff7a, 0x78223fd2, 0xb1e543f3, 0x73dce341,
    0x67584724, 0x830176da, 0x99da7b88, 0x08cc8a17, 0x84d6c0e0, 0x874a1e0e, 0x3ca1205e, 0xe5a7c0f3,
    0x6ae73f01, 0x6e3a8001, 0x4d9dbf67, 0xb93df39a, 0xcf7c726c, 0x666ba992, 0x90759d6a, 0x5ada4cf1,
    0x369278cd, 0xb993e302, 0xe3078495, 0x5f896a13, 0x7819d2fc, 0x3f8ded34, 0x94f947b8, 0x91a3e26e,
    0x1eea969b, 0x9e542801, 0x3903b7ca, 0xfbae5932, 0x66bcc300, 0x9fc3833a, 0x775a196c, 0xdc9c3c70,
    0x658b4a72, 0x7a740a51, 0xcea3abff, 0x9980dd00, 0xd9e27be7, 0xb88c12c2, 0xe1d35046, 0xf021ebb2,
    0xddcc1367, 0x21b46a40, 0xd8cf055d, 0x281c11a2, 0x0e4b255c, 0x25e29f91, 0x6ebf1951, 0x73e4b607,
    0x75347773, 0x8bf2329c, 0x3468b49c, 0x1da6b751, 0x4596925b, 0x1ae4d66d, 0xa0e9e41d, 0x113bc46f,
    0x555b9525, 0xe4be3a10, 0x9e3ec624, 0x1d6f01d6, 0xef04b221, 0x7f775700, 0x9f12c3d6, 0xc2b6da06,
    0x7e967832, 0xfbd2e827, 0x041f24d6, 0x1def924f, 0x23b44ad8, 0xa3149cc3, 0x534283a5, 0x02e8fc96,
    0xb59f2da2, 0x5c210469, 0xfaca183f, 0x1edab0b4, 0xd5506ed6, 0x6b9c3d45, 0xa39814c3, 0xebb5eff2,
    0xc3be5c49, 0xfea1ba4c, 0xcad13ff5, 0x5944f8bb, 0x3e2bc399, 0xc05accd8, 0x9b0151c8, 0x40744cdf,
    0x8bed2a26, 0x2c5da90e, 0xe97a3262, 0x9c93e053, 0x59676ec4, 0x36f75d6a, 0x6fac9c05, 0x9c15467b,
    0xb361a2c8, 0xe95b626f, 0x8b4603e2, 0xbeb210d6, 0x779e4e26, 0x3435ad89, 0x948cd5b0, 0xbe9dc88e,
    0x99141595, 0xd0726cbd, 0x6fd0628e, 0x1d709266, 0xb311e130, 0x1bdd2770, 0x08700e15, 0xf2e59490,
    0x6f80ff7c, 0x522639e1, 0xd29c58c9, 0x3250a8dd, 0x42219840, 0x18e61d05, 0x956af162, 0x3355a400,
    0x93a3e357, 0x825cada0, 0xa5b65a03, 0xf330ba87, 0xd9ad5f89, 0x3d6bf7f5, 0x6798cd49, 0xc9486adc,
    0xe1299393, 0x7bd82f97, 0xe0919610, 0x4036f2cb, 0x2cc51c81, 0x05c0f6ce, 0xad4f6562, 0x26ed92c0,
    0x7b89394a, 0x8726ffdd, 0xc72f3de8, 0x34862e13, 0x0e97c2fd, 0x06f50c67, 0xace352f8, 0x021dc0f7,
    0xf0771b6b, 0x2f1fac23, 0x3e7e7d5f, 0x78950d8e, 0x64620506, 0x94d2f4dd, 0xda8fbca6, 0x6d24cc09,
    0x2f24f824, 0x55db8b30, 0x7fe0f3ee, 0x69d5e622, 0x36526472, 0x1b2d3335, 0xa42c5772, 0xa3c21043,
    0x768a77c1, 0x9c10702a, 0x8ce309ef, 0x56161326, 0xc2a5bb95, 0xde77fb49, 0x64ce4d1b, 0x56f5dac7,
    0x0a45a312, 0x2ef9d797, 0xbb73df21, 0x94eb48a0, 0x21c12806, 0x82114ea3, 0x5f2a4551, 0x96cf8e8f,
    0x7a8b905b, 0x8b74a504, 0x6b7f2923, 0xd4c47647, 0x9ac861a2, 0x47e0c6e3, 0xe079660d, 0x9af75d79,
    0x23d2a337, 0x2b88ec80, 0x9d4171c2, 0xaacff4ee, 0xd1f95133, 0x3c85a6ea, 0x1c90c34f, 0xa5e318b7,
    0xa73f07fb, 0x2916e1e7, 0xefd0ba82, 0x1fd87eb4, 0x50b97e5d, 0xbec3948d, 0xb786de2e, 0x8bb123a0,
    0xe614d508, 0x6429c2b2, 0xe07c09e1, 0x46499711, 0xfbdece50, 0xbdc5e75c, 0x72911b52, 0x446cd7a0,
    0xf2bc6cf0, 0x26faa510, 0x4f78ce02, 0xb12b5ceb, 0xe5f4624d, 0x3d4c6e44, 0x570b0b0d, 0xc3d0aea6,
    0x09550ec2, 0xbe490ded, 0x1ce784b1, 0x34cdf87e, 0xd7fbfc9e, 0x837f03ab, 0xcc8becdf, 0x4ba5855e,
    0x7c2d8b68, 0x04ec5ae2, 0x7ba1406a, 0xbc3ec645, 0x6b20373c, 0x516f1598, 0x6a2448fb, 0xef6f05e4,
    0x17ba308b, 0xe58f301b, 0x280cf936, 0x7e78b740, 0x0867074c, 0x9e33b82d, 0xbd2fc2ee, 0x843d360c,
    0xc68ce8a1, 0x5f05e1d2, 0x11012836, 0x00130b93, 0x45c875fd, 0x8a91c5e2, 0xabb285a2, 0xd56388ee,
    0x71c69f7d, 0x744f6819, 0x1b8ed87a, 0x17d51d8f, 0x466ea81d, 0x20c32245, 0x1e84b745, 0x89b0429e,
    0x8922d99e, 0x592c7cc1, 0xd559ccfd, 0x4a7bc77f, 0x345a1481, 0xc6d5ecda, 0x4a4640f7, 0xe5178bb5,
    0x62db9e6a, 0x4db7316f, 0x187a37e4, 0x750887da, 0x2173c889, 0xcbad21cc, 0x24645c61, 0xfae281a5,
    0x68af14d9, 0xf1b79f82, 0x9e96114e, 0x94ff0408, 0x6cc246cb, 0xa1b08743, 0x066c14b5, 0xd61d9f52,
    0xfa3ac67f, 0xca0332a3, 0xd6ed9db5, 0x1fe6fed9, 0x7d117baf, 0x00ba28a0, 0x67721fd0, 0xa2634e5d,
    0xf33b1398, 0x691b9bb3, 0xc95355a7, 0xdbc375e4, 0xe86eb7e5, 0xb63bae13, 0x57d383a2, 0xd202512b,
    0xdb7212b4, 0x6ef5af09, 0xe9183041, 0xedde046f, 0xc7e76499, 0x83fa9e7f, 0x44ba7562, 0xd05a300a,
    0x49c427a5, 0xe8488a21, 0x0ac18d45, 0xa8eb3830, 0x7b0028ed, 0xb8ab336f, 0xccf8c4cf, 0x4472d018,
    0x5caab532, 0xfa272d32, 0x947e97a2, 0x214571e8, 0x57a68135, 0x37d9c541, 0xf85f0c04, 0xc43bb229,
    0x8c5e82a1, 0x2e5e7e60, 0x515da1b4, 0x79010a2a, 0x10af1f3c, 0x9313c22b, 0xafd3352c, 0x725b4964,
    0x950bca2e, 0x50647118, 0x08761787, 0x37e2694b, 0xc7817165, 0xb0abcaad, 0x0e507651, 0xe23c1ae0,
    0x9ac1e626, 0x24a6d3a1, 0xf58150e7, 0xdfb8f24e, 0x3af8e450, 0xf2050dc2, 0xb4eae277, 0x1dd81917,
    0xedade33d, 0x9068bdcd, 0x140b6816, 0x13bf1435, 0x7041353e, 0x6c5b4bed, 0x9d9a020f, 0x70a456cd,
    0x946822fc, 0x0325d31e, 0x15225734, 0x0cf118eb, 0xb21a4d22, 0xc8dcc444, 0xd526d290, 0x28b0748b,
    0x3afd7bd8, 0xb7cc1676, 0x0928b699, 0x60256de0, 0x6992c2da, 0xd63dc04f, 0xb042da7f, 0x2f21680c,
    0x43571429, 0xbbd918ff, 0xf8702078, 0x97c9070a, 0xad062b0e, 0x8c37486b, 0x8433e4d3, 0x5e673d98,
    0x7600c71c, 0x22aa60c2, 0x7a42b3ad, 0xdc9bdea3, 0x5b861629, 0x0cf5e5e6, 0x3d65dd2a, 0x593b4429,
    0xad9729b4, 0x7c37d107, 0x2e677a95, 0x8b03e5fc, 0x86bf118c, 0x40de7bd2, 0x152999f4, 0x1ab5f9fc,
    0xb9d6c033, 0xc48b5782, 0x1345e6ed, 0xefab5dae, 0x41388757, 0x2e1d9d61, 0x445b89c4, 0xa070ed15,
    0x65cab585, 0x1f9271a4, 0xb533bd86, 0xf85034a6, 0x171a7125, 0xab8ec271, 0x5cce05df, 0xdf905872,
    0x75775d00, 0x96fcf5d5, 0x0c2ad1c0, 0xd55aea5e, 0xb21a4d22, 0xc8dcc444, 0xd526d290, 0x28b0748b,
    0x3afd7bd8, 0xb7cc1676, 0x0928b699, 0x60256de0, 0x0293aef1, 0xabde8c9c, 0xab628c70, 0x0b0c713c,
    0xc6c8f023, 0x870b66e4, 0x2fb6a5f5, 0xcca9a2b9, 0x154bd574, 0x976132c3, 0x9934e056, 0xb2b4749f,
    0x7600c71c, 0x22aa60c2, 0x7a42b3ad, 0xdc9bdea3, 0x52d952e1, 0x024ca50b, 0x2f4e00a1, 0x83708536,
    0x9bef5949, 0x5a6db78c, 0xccea4496, 0x98082ae5, 0x303b586a, 0x1d6979bb, 0xaa9c3e2c, 0x5b1c85d8,
    0x0230ca75, 0xb55428d5, 0x598fc845, 0x27d37514, 0x6c762408, 0x9fe60385, 0xad978840, 0xc3557aea,
    0xeb70388a, 0xbcd80fb5, 0x34e221b8, 0x99a39643, 0x44a89da8, 0x3c2e14e9, 0x620b6c35, 0x6a9a29cf,
    0x5c0fe21c, 0xcd0ae506, 0x17b3a186, 0x4f7ae26a, 0x35f211dd, 0xee2e840c, 0xad685b22, 0xe53c3881,
    0xf03adb6e, 0x5d67abca, 0x242f892f, 0x1fb8bd39, 0xa49bbc33, 0x3cf916f7, 0x2b5c05cf, 0x9fd88a35,
    0x877d2b57, 0xc4ba9208, 0x3d1ada08, 0x5748bdd5, 0x24e3254c, 0x583bb731, 0xdea5deee, 0xb9b2a9ba,
    0x037a7d61, 0xcbf0f7e1, 0x3074d46a, 0xafbc35d5, 0x7a65da49, 0x70c2f19c, 0xbe3bf5da, 0x077330a2,
    0x313653fa, 0x0166ce24, 0x84bd34d6, 0xac89981d, 0x72f3419b, 0x1976b87d, 0xc2830aa2, 0x63413375,
    0x2e808e88, 0x005af384, 0xed0831d3, 0xcd3b4fbf, 0x45a96d49, 0x234f30de, 0xae85c24f, 0x2368ee0f,
    0x2734df72, 0x4b5b083b, 0x20de875b, 0xc6e277d1, 0x61aac0ea, 0xec88dbd7, 0xc1801f55, 0x54174039,
    0x2190487c, 0xce13ad35, 0x3048ed7d, 0x38e35f97, 0x195f71a4, 0x9f43fed6, 0xfd94c89a, 0x3c77a445,
    0x863633b3, 0xbc154ccf, 0x1aded410, 0x6b572336, 0x778ce706, 0xd67b4c60, 0xd76574c8, 0x49aebfd5,
    0x9e38b7cb, 0xf398dc9d, 0xb3df4fa3, 0xdccd9184, 0xa2d0c79b, 0xffd6fe7b, 0x7f056a42, 0xea3ebdd5,
    0xde4071a6, 0xd0b797aa, 0xc64e5437, 0xb22ed02b, 0xb29cddb3, 0x6ed1428f, 0xb23f4979, 0x7c9d3a3d,
    0x2c1031fc, 0xef7c441e, 0xc83c9b1a, 0xa52a69b2, 0xbf0b1e0c, 0x8d765e68, 0x32e8ed03, 0xe3ba5fff,
    0xfdcdff3c, 0xa47ce614, 0xdf9b6bd3, 0xddef78f6, 0xfee7642b, 0x90fac40d, 0xf87c78ba, 0xea14491c,
    0x7d055ce3, 0x36447fc5, 0x6213205f, 0xf8915b22, 0xb4442398, 0xb392b7e7, 0xe5b76930, 0xf465ec76,
    0xf3e189b3, 0x2b688f3b, 0x38f15b79, 0x65c4c9d8, 0xc366e889, 0x59a4e690, 0x4375827e, 0xa6f52da2,
    0x7bc8a83f, 0x209fb7a9, 0xa04600a3, 0x50a5d487, 0x25fa5da9, 0x6944793e, 0xf73d1c20, 0xb0068f6f,
    0x99e87197, 0x2164933f, 0x3f174b82, 0xd0e0a454, 0xd71bf958, 0x7f9a9784, 0x73e63845, 0xd70937d1,
    0x030b02ea, 0x6d937670, 0x104bd2d5, 0xaa976953, 0xdd5b8c07, 0xef3ffa51, 0x1cd198a3, 0xc288cdb8,
    0x53280d0a, 0x2c6a5a6e, 0xbbeb3c04, 0xcf950aea, 0x0523a033, 0xedcb68d9, 0x79fb3d53, 0xd60bb0f0,
    0x52521657, 0xe7257dfc, 0xa67f09e5, 0x5cd6790c, 0x6bc05623, 0x0b8eb935, 0x87e8b373, 0x1ecc8151,
    0x8eaae465, 0x2426573c, 0x8f28a4aa, 0x08443004, 0x9d67a782, 0x2a76790e, 0x3d766c42, 0xc1c90a34,
    0x7c9ef58b, 0x634bdb03, 0x368ab648, 0x9108789a, 0x1d4361b5, 0xef544224, 0xcfa226b7, 0x3857251b,
    0xe922b03e, 0x239f7fbd, 0x83920b48, 0x1f800fc0, 0x97c1a343, 0xe384419b, 0xc558586e, 0xe4f42bfd,
    0x1a833e62, 0x8400ee1f, 0xddcffb7c, 0xc280ba5c, 0xd03de5d1, 0x793147e9, 0xb4a226f9, 0x72d01c6a,
    0x62b165eb, 0xd2c8ce82, 0x349f0588, 0x54ddaea8, 0x5cf954a4, 0xfe6b07e3, 0x91510095, 0x5d373403,
    0x2426dcf6, 0x199365bb, 0x6aa7deff, 0x4739d0f9, 0xffb8d398, 0x6259ee27, 0x4b8f39c6, 0xfd5be644,
    0x6bc05623, 0x0b8eb935, 0x87e8b373, 0x1ecc8151, 0xcda07e93, 0xaf547c1a, 0x309de38d, 0xf3effa11,
    0x15c77190, 0xcfdb02ea, 0x26765c83, 0xcbc51a5e, 0xa6ed8422, 0x966bd0d6, 0xa9921c3d, 0xf069d138,
    0x985a4bc2, 0x0043aded, 0xa5897422, 0x5f4b0e22, 0xf241e63f, 0xb8707fd7, 0x7d8771a4, 0x60efe47c,
    0x3f8cf93a, 0x4485b099, 0xd635a21d, 0x4a773bae, 0xcfc28811, 0x1c228ec7, 0x425c6b5a, 0xe2034504,
    0x19060427, 0x498b0185, 0xcdebe094, 0xce035094, 0xf1eb1879, 0x2ce2002d, 0x69146350, 0xb3afe355,
    0xcb368667, 0xc16bdde9, 0xee16fb32, 0xa8317788, 0x4152a001, 0x4f10b2bd, 0xad107d10, 0xaa80fa2a,
    0xa959ea4d, 0xd75f2fd1, 0x1f1d75c2, 0xe1aa617f, 0x448b6acf, 0x931f54f8, 0x23a2e044, 0xb4f5192c,
    0xdb9f30ac, 0x2fc6939e, 0x685c23ad, 0x79bab6b1, 0xa4bbb0c3, 0xdd510c56, 0xa8c03753, 0xac519bf6,
    0xad77de27, 0x55203cb0, 0x00ccf8c3, 0xfb97a8e6, 0x48aee077, 0xfd58c02c, 0x4711f93a, 0x81966c44,
    0xbfed05c1, 0x988f55ad, 0x0cf49aa9, 0x57e292ba, 0x589f33e4, 0x0731a473, 0xff9cd5da, 0xa35cd20e,
    0xe9ce1940, 0x5228bdd7, 0x02ff3f63, 0x7bc6da30, 0x4517e176, 0xf7a1e697, 0xb57456d1, 0xac71b264,
    0xe956a9d5, 0x1d702dcf, 0x1cd5d3c3, 0x77cb8158, 0xf6ff2db3, 0x2354bdf1, 0x948f49f5, 0x2cbc512c,
    0xd1a4539c, 0x94200bd8, 0x979b2c64, 0x94474a10, 0x7c68f86d, 0x4b985990, 0x1f135ba6, 0x49db1f94,
    0x826ca080, 0x119b599b, 0x0c093a60, 0xa35a8306, 0xb2b72bda, 0x61e055bb, 0x256f4600, 0xd27db660,
    0x6bfa0726, 0x69a5f363, 0x1febb411, 0x0ecfd247, 0x56d7bda2, 0x8b6d8688, 0x5ff8f69f, 0xfa603570,
    0xadc5eb04, 0x081860b7, 0xffe0d545, 0xfc9573b4, 0xd66d768b, 0x33933689, 0x9ead814b, 0x38a5e236,
    0x36004141, 0x4f3c2f48, 0x9e171f8a, 0x21ae4f5a, 0x8d5a69a3, 0x066b5505, 0x26cd9235, 0xd87e9a47,
    0xfb1c90fe, 0xe1eb69b0, 0x1e8a4493, 0x22344021, 0xb71367ab, 0xa6f502c5, 0x6c2ae1a7, 0xa5bf251d,
    0x2ee88717, 0x3832c325, 0xe7a3ba43, 0x19fb65ae, 0x73d20b44, 0xbf30e58f, 0x22dcabe6, 0xd2b22506,
    0x3687368c, 0xd1b0bb77, 0x3a8005e4, 0xc3c2e627, 0x4d7f16fb, 0xb62153a1, 0x112d036e, 0x66ea4af7,
    0xd4280ee1, 0x232d7247, 0xafa2f905, 0x12b06aec, 0x00541416, 0x0ac92c83, 0xdc9a5cfa, 0x7a4d195c,
    0x08623537, 0x2ad55b61, 0xa591a565, 0x2c4680b1, 0x89964561, 0x8708970e, 0xc07979c5, 0x7aa0a72b,
    0x6deaa954, 0x00ca5fcc, 0x6c19c770, 0x2bec7ac1, 0x27292b69, 0x59b9ad5f, 0xa7406c02, 0x165a328e,
    0x589f33e4, 0x0731a473, 0xff9cd5da, 0xa35cd20e, 0xdeb7cf5d, 0xd4c28a03, 0x3e7832d7, 0xbeb39647,
    0xd6247b91, 0x97f9c548, 0x71186987, 0x6a11e5cc, 0x1258b32c, 0x0368b9f5, 0xe4792ed8, 0x9bf59fb2,
    0xd86e633c, 0x76200f07, 0x91732c3d, 0xfd891b8f, 0xb05090de, 0x7450e368, 0x8574aefb, 0x9a301a20,
    0x34a83661, 0xa8e88e98, 0x277a0eea, 0x6e27112c, 0x826ca080, 0x119b599b, 0x0c093a60, 0xa35a8306,
    0xef33a30f, 0x9f1d8c08, 0xa3d8c08c, 0x5f90bf62, 0xc0ff5d23, 0xc07b4a44, 0x153becf7, 0xa0b00881,
    0xbb0a1dc3, 0x2c519a42, 0xda019a81, 0x422ab62b, 0x1386c124, 0x896ee279, 0xaa50ac6e, 0x5758f4f4,
    0x00d8efc4, 0xcefdb7a2, 0x9fccdd86, 0x460b57dd, 0x5a023b49, 0x67b8ddae, 0xe6ffb29d, 0x6e156ad2,
    0xe0ecde93, 0x70c5369e, 0xc149d30c, 0xe636fe94, 0x5361f8ac, 0x2f999ad2, 0x57c08d82, 0x9fdc87a7,
    0x15c43dbd, 0xe36c8fba, 0xe8866e37, 0x75c4cffa, 0xd87fcc1f, 0x22781272, 0x8f84f3d9, 0xdf6550d8,
    0x6f33bd6d, 0xdfd301ad, 0x5e498e3b, 0xd05666e5, 0x820e368a, 0xe0eef261, 0xd1eadfd6, 0x1f1c62b6,
    0x7378d462, 0x922e44a8, 0xd329707a, 0xb5226d65, 0x09020cfc, 0xfaec8e43, 0xbf8506ff, 0x640600da,
    0x65bb5657, 0xf5c641ce, 0xdc7f3b31, 0x55d90f33, 0xbda10461, 0x49fd0dca, 0x217f8908, 0x821b8b43,
    0xd7258f39, 0x62e6651f, 0xc4978379, 0x3a52bce5, 0x192026ef, 0x8dafdea7, 0xdaf35b2a, 0x3bd8667a,
    0x7d055ce3, 0x36447fc5, 0x6213205f, 0xf8915b22, 0x0ffa61fb, 0xc2c4f01f, 0xd9f88585, 0x6e5b6ef9,
    0x8851c6b7, 0x64868247, 0x2a082764, 0x342fb898, 0xe517c711, 0x6f68913e, 0x192f2108, 0x2e460cd7,
    0x65bbd325, 0x832d2ed4, 0x9f3634a1, 0xc11556a2, 0x7da8ae7c, 0x16a09016, 0x52d3ea3e, 0x4bd5b823,
    0x3bd9a631, 0xa4235186, 0x7e9b824c, 0x2298fd20, 0x563041fb, 0x9bcebe51, 0x407be402, 0xb4db9b2f,
    0x3728f58a, 0xd2b8fad0, 0xb3e27577, 0x03fd6707, 0x02391917, 0x20ab0ed3, 0xb4d09fb1, 0x51e6bd63,
    0x6c3ff031, 0x0d278921, 0xcde56b59, 0xaf0fcf17, 0x32b728e0, 0xacfcdbc6, 0x8ee7efca, 0x58d1d272,
    0x256abf5d, 0x64513fe2, 0xf3388712, 0xbd124cf8, 0x61aac0ea, 0xec88dbd7, 0xc1801f55, 0x54174039,
    0x8e1b6ff0, 0x90ab0b08, 0x97b30875, 0x347ef34b, 0x6d2baa85, 0x303d3c74, 0xfa02cf8a, 0x86cb453b,
    0xad9895b8, 0x5aa58910, 0xe78790a7, 0x69566b57, 0x57254ce5, 0xc9ecf402, 0xf491fce9, 0xea285cac,
    0x751de1f2, 0xb5da9650, 0xbfe94309, 0x000fefe6, 0x07c11734, 0xec8101a6, 0xf5962503, 0xec834dba,
    0x820e368a, 0xe0eef261, 0xd1eadfd6, 0x1f1c62b6, 0x2c50bccb, 0x52a47c6b, 0xeff50468, 0xdf990395,
    0x6b059152, 0x1ee46ce4, 0x7af1354a, 0x444cb387, 0xed1b0b80, 0xad7f5555, 0xb3cfb23e, 0x72f7b67a,
    0xaacd4e70, 0xdc044faa, 0x696da0dc, 0xee7cf769, 0xf668243d, 0xab6cb514, 0x708aea03, 0x77afb491,
    0x8abe7c21, 0x30200020, 0xa34594c3, 0xbc53369d, 0x6bc05623, 0x0b8eb935, 0x87e8b373, 0x1ecc8151,
    0xa3508d61, 0xf81cf1ea, 0x63e6c49c, 0x764c405f, 0x3d2115ef, 0xfb7f5eae, 0x52f054f4, 0xc5d99a57,
    0xaca37132, 0x993b0b87, 0xf93cfa04, 0x021eaaa7, 0xa695c669, 0xe46b88c7, 0x978fb5d2, 0x060e35ff,
    0x1926ee3c, 0x4a18051c, 0x004f5575, 0x1634979a, 0x6757f44f, 0x1a312218, 0xc5178a7e, 0x10ff201c,
    0x0835c9ba, 0x7ad95d2a, 0xacb27a0d, 0x1858416b, 0x364ea101, 0xac487837, 0xa5005c83, 0x75fa8bcb,
    0x83011338, 0xdac25c52, 0xa6998f58, 0xd7b76d14, 0x11084299, 0xc8af4155, 0xb0130db0, 0xef460718,
    0xcf3c370f, 0x31b46914, 0x6bec6a1f, 0x2b4702ed, 0x33d542b7, 0x0e1777b4, 0x59791c48, 0x639645ee,
    0x7c7d4bb5, 0x27e5dbdf, 0x28a2fc8e, 0xeef711f9, 0x92356c20, 0x9ba36cdb, 0x3e1859a0, 0xeb209ca7,
    0x2e161da1, 0x0b59bce1, 0x8bd62858, 0xca75fe66, 0x63029fe0, 0x423f73e1, 0x866c3934, 0xde1054fc,
    0x40aad1d2, 0x7ce5afca, 0x19aba18f, 0xd97f0bd0, 0xdd17e1b0, 0xd83b8b6b, 0xe78d7a47, 0xbed1235c,
    0x25da2d09, 0x69245739, 0x869192db, 0xff2a6b8d, 0xe04cb981, 0x3bc55d1b, 0x72bc538f, 0x33f82cf9,
    0x4c2134c0, 0x4ddf8ddb, 0xf0b02d3b, 0x609299cc, 0xae481cb1, 0x1f718815, 0xb2a6dd5c, 0x14572d9d,
    0xe4e7dc62, 0xed814264, 0x85a3bacc, 0x3a886c0e, 0x5c0d4d78, 0xb398cea3, 0xbf583337, 0xdc3c84ff,
    0x80b85eb6, 0xcaac1bfe, 0x076d6bb7, 0x1045791e, 0xd1cfe709, 0x1c4cd02d, 0xa7effc5e, 0x807b60e0,
    0xf59914ab, 0xf065af24, 0x108a7a93, 0x34f413fe, 0xa828ec60, 0x6ad161cb, 0x9dd96cc9, 0xeca339e2,
    0x92c4663b, 0x92f27fbe, 0xa7c5b3d7, 0x16faafbe, 0x97121666, 0x384039e4, 0xc4bc0f77, 0x3022b576,
    0x6c9761cf, 0xf8431952, 0x5f7cb987, 0xf9ac4a93, 0x1388ed99, 0xe825c04c, 0xec7f713f, 0x34950dbd,
    0x98d914f7, 0x1057e561, 0x42470569, 0x38b38873, 0xad3129a9, 0x39b5e886, 0x38d6d5dc, 0x3e382e99,
    0xabd3316c, 0x4832a369, 0xe2b3bd80, 0x4c1e3add, 0xee6bc14d, 0x06dbb43e, 0xac7d8b12, 0x1caf57a6,
    0x1561bad5, 0x328ab19f, 0x3ad32da3, 0x419146c2, 0x16cbbedb, 0x685eb475, 0x468e6328, 0x8ab370cb,
    0x43b2d2f4, 0xd2a52317, 0x4414008b, 0x7cecad50, 0xa9cbb6b5, 0xb95a7f6a, 0x8828f467, 0x42e7bfda,
    0x2445d467, 0x92fc18c0, 0xd46d7197, 0x40022afb, 0x28eb86de, 0xbe56bb55, 0x5400cefa, 0x21736091,
    0xebb7611b, 0x68e2a203, 0xfbf40b74, 0x50d6bf95, 0xe368d041, 0xfb59cbe6, 0xbcb6ad1f, 0x294a59f0,
    0xb5689c05, 0x9e656934, 0xccdb5aa6, 0x2b47e735, 0x07b325b8, 0xe533d508, 0x05f1e972, 0x2f475e6a,
    0xc8eacc3f, 0xbee7c188, 0x06257d54, 0x05f743e7, 0xd68570c2, 0xf475c4f5, 0xa86cc3cd, 0x9016155f,
    0x13f46e5c, 0x02e55c33, 0x9466d3a5, 0x9c1ba8fe, 0x02e5e659, 0x0af0f355, 0x1e6f57ba, 0xda31362c,
    0x633402bb, 0xa8589ee8, 0x402fb11c, 0x1d2211b3, 0xfee3c759, 0x0fad89d1, 0x0c1ca82d, 0xc54add87,
    0xcab65e45, 0x2cb37b33, 0x2bb77b9f, 0x3fcad26c, 0xd4b1e1e4, 0xf4d2bc4b, 0x914ce86a, 0x60b8cd61,
    0x5ab888bb, 0x3f238258, 0x17115bbd, 0x0d015921, 0xd6525815, 0xaf90eb37, 0xb80997b0, 0x846c473d,
    0x021ab39b, 0x146b7a4a, 0x4732efce, 0x1f28d47a, 0x08e2c954, 0xc28489f4, 0x9e539849, 0x57b494c6,
    0x5cc49cf5, 0x9db5bf2c, 0x99242aa1, 0xf0d79322, 0xcaad81ca, 0xb48bcc93, 0xaf5ecd71, 0x24bf72cc,
    0xe2f20089, 0xed5868a9, 0xd413258c, 0x444bcd61, 0x749da079, 0xf77dd777, 0xace8bc32, 0xf7c8e993,
    0xb3215bf6, 0x66b69f71, 0xe94762a3, 0xa62fbb81, 0xc9fe6c4c, 0xe45861c2, 0xb1e7e0d4, 0x40934fdf,
    0x6940c64f, 0x3e90dde5, 0x63556833, 0x222963a2, 0x212bec28, 0x8bf903c2, 0x0ce1dfab, 0x16a2e7ce,
    0xb0576b77, 0x52d5e2d4, 0xb5142897, 0x397794b1, 0x1ceee364, 0x4c61fd07, 0x653dfe0c, 0x727d91d9,
    0xae24feac, 0xf516e1ca, 0x5f1f12d8, 0x504c51fa, 0x64ecaccc, 0x795e67af, 0x4d3f1aac, 0xcbbbcbc0,
    0xe0b51954, 0x823d5d1b, 0x2aac1ba4, 0x7eb9dd7b, 0x0c674d1c, 0xa42d04b2, 0x4b1c5168, 0xc1fd4d75,
    0x4eebd160, 0x7b77f4a7, 0xe3acf154, 0xf8392b7d, 0xf90f02b9, 0x5969dccd, 0xb858f0d8, 0x7549822d,
    0x38bc37b1, 0xd9936b41, 0xbca9c405, 0xb6aaaa86, 0xa4e3047f, 0xbe56c307, 0x4cfb9c54, 0x30a6670c,
    0x6e59a1b5, 0x21c0c7e0, 0xdc12ee4d, 0x7e6c44d0, 0xadf9055d, 0x655d578c, 0x44f1775d, 0xf7225599,
    0xddc12802, 0xf0e8b44f, 0x0464166e, 0x6a0384f7, 0x11e1ac1c, 0xce8bccc7, 0x8fd20a66, 0x62a175ec,
    0x6c7694f5, 0xe0a07838, 0x968ecc45, 0xf35ceedc, 0x4ea673ab, 0xa4e7c24a, 0xa72e562a, 0x29fd0200,
    0xe4361c60, 0x50513220, 0xefb0a02f, 0x4555d510, 0xdfb5beb8, 0x8a6b1c17, 0x74147751, 0x219edf68,
    0xdba3ed9d, 0x321dd370, 0x014c0d20, 0xf1b6a24f, 0x68af14d9, 0xf1b79f82, 0x9e96114e, 0x94ff0408,
    0x09b90b5f, 0x3f81859b, 0xeedf8315, 0xc49b61e3, 0x2039d177, 0x4ea71386, 0x9da5ee8c, 0x1557576a,
    0xac9087e8, 0xf1310bbb, 0x98051a88, 0x7824cf76, 0x9e9cb844, 0x2d27c847, 0x4ce843be, 0xa9d94ed5,
    0xd728249e, 0x4fa017e5, 0x877845a7, 0xe799f575, 0x54bf5c9e, 0x41e212be, 0x8fe08f94, 0x40b09f66,
    0x2459ed01, 0x8e109cfe, 0xc6cdf13b, 0x074bcec8, 0x36004141, 0x4f3c2f48, 0x9e171f8a, 0x21ae4f5a,
    0x88bf6431, 0xc8f8c297, 0x6680c969, 0xb3b567f3, 0x29ce691c, 0x3367c6be, 0xb9d4798d, 0x3aef1ca2,
    0x1b7cdc61, 0xa4501504, 0x814b0423, 0xc5ec7019, 0xf84dde78, 0x109120ad, 0xcc23f640, 0x43d36af6,
    0xca802206, 0x7afa04fa, 0x2dcff13c, 0xbbbce2f3, 0x68ac148a, 0xafb7c7bb, 0xac0d71c6, 0xafd3e0b1,
    0xefbb11f8, 0x5d4da97f, 0xfe01e117, 0xca2a4e54, 0x7afd8d4e, 0x30169ffd, 0xdd943d96, 0x76d74e74,
    0x6a9c927c, 0x099d5ba0, 0x0bfabd97, 0xffb250d4, 0xef8effa3, 0x9be0d64b, 0x39647dd1, 0xe402ff66,
    0x44fbe831, 0x8f40b3b5, 0x09a763e5, 0x904e940e, 0x9a528326, 0x1607ddc7, 0x75ccd014, 0x4d93ff2e,
    0xeb890ed2, 0x39a7eb52, 0xad079177, 0x4f859720, 0xc4c90782, 0x737283fd, 0x58eba93e, 0xf1e56be6,
    0x23e4a83c, 0x4a7d1596, 0x3f385f71, 0xded47514, 0x8886615a, 0x04d62126, 0x44cb22e8, 0xd1ca21aa,
    0xfb266cae, 0x6eda0a52, 0x6efa31c6, 0x9a5a2b77, 0x795e4213, 0xedae8fde, 0x7022c6c3, 0xcc24fce2,
    0xca0f2ff3, 0x6cf567ff, 0xf90c0800, 0x87f832c3, 0x7329bf95, 0xcf9a3b00, 0xd6381b08, 0x0e4a9090,
    0x0777d1bb, 0xd7a6e3d6, 0x0c87e652, 0xcf79dd47, 0xc63afa8f, 0x7e9e3623, 0x7bb2c7b6, 0xbfa0be2f,
    0xebd59a60, 0xd6ee3303, 0x5e1e852e, 0x49ed1fcb, 0xecf3f34c, 0x170320e4, 0xd9999561, 0xc22115c5,
    0x25633df1, 0x5d5c74d1, 0x594ec806, 0xe267df79, 0xc74560bb, 0xdb3b9edf, 0x3a7b28c4, 0x14256b81,
    0x060eadff, 0x19691972, 0x5b358d1d, 0xea11d783, 0x14bb11ba, 0x3e05f815, 0xe2862407, 0x16efc7cf,
    0xf374b63c, 0x67ea62b7, 0x28732bf8, 0x084276c4, 0xb41590d6, 0x637b2a21, 0x410d6d94, 0x60075dfb,
    0xf55f5b06, 0xb9522f6c, 0x66ec7dbf, 0x46ad5453, 0x9f005f2b, 0xa4ec6055, 0x336c8fa2, 0x66e7327d,
    0x6b2d0696, 0x423295b3, 0x27b7f8ed, 0xd52ca3d7, 0x66a2bb95, 0xb08dd3b4, 0xe6c6a07c, 0x1f4231ac,
    0xd70fb861, 0x1f4fecc0, 0x93dd4e9a, 0xf85d61cf, 0x6955b840, 0xa4c69f84, 0xe97809b0, 0xad0df0d1,
    0x7c7629af, 0x47f0bd80, 0x0dd53d3c, 0x70c8f006, 0xd8b69de9, 0xcdd8e5d8, 0x27596f48, 0x90cb803d,
    0x937fb415, 0x47c7e7eb, 0x69180cf4, 0x3b33e5d9, 0xd39b4cb8, 0xd8f8cac1, 0x29797714, 0x04ba8b6a,
    0xfff22ec4, 0xaedd700b, 0x192ff5fd, 0x6e4a5810, 0x14bb11ba, 0x3e05f815, 0xe2862407, 0x16efc7cf,
    0xf374b63c, 0x67ea62b7, 0x28732bf8, 0x084276c4, 0x6dc5315b, 0xdb18e5df, 0x44738b68, 0x79740640,
    0xa7635919, 0xb28c9887, 0xddb6fa5f, 0xb399b304, 0x88f478fd, 0x1501bd81, 0xf73acb82, 0x5e80cb2b,
    0x6b2d0696, 0x423295b3, 0x27b7f8ed, 0xd52ca3d7, 0xe72bdc00, 0xc7875591, 0x0a160449, 0x6bd40767,
    0x5d8e93c7, 0x533a232b, 0x9f238c73, 0xb0a783c2, 0xac023cf4, 0xf252088b, 0x2cd5a7bd, 0x0961af62,
    0xc640dd08, 0xce4cc05d, 0x19ac9f55, 0xda9e9716, 0x09dc858e, 0x2bf272a3, 0x2ccf67b5, 0x69f63761,
    0xf551231b, 0x46a7e4b5, 0x49193eea, 0xc8107900, 0x8e459381, 0xaacab59c, 0xc23429bf, 0x880a9876,
    0x158d51fc, 0x4436cecf, 0x8db2886c, 0xaa3232f1, 0x888c94fc, 0x4033fe98, 0x7caa4a6b, 0xcc54def5,
    0x551a9b05, 0xd91e983d, 0xa0c40397, 0xd1939035, 0x3f9e87d8, 0xb85faec2, 0x3732519b, 0x0b138c80,
    0x0c674d1c, 0xa42d04b2, 0x4b1c5168, 0xc1fd4d75, 0x1e3a6e6d, 0x7a58bd06, 0x9c6b7050, 0x4afdd9c8,
    0x9142a25b, 0xde1ab3f3, 0x8ce7c6ba, 0xa52cd960, 0x86cd96b6, 0x2eb06983, 0x52ebf206, 0x7d3e2154,
    0xb26a7da3, 0xd7c8c2fb, 0xdb64e24d, 0x88ee97fc, 0xa1833d24, 0xa9562595, 0x7d406788, 0x744589e2,
    0x3c0b5406, 0x3aceef34, 0xe8455ece, 0xcbadad0d, 0xa84f0510, 0xfa3ae507, 0x6c05508f, 0x10b9689f,
    0x93bae998, 0xcc48e522, 0xccc8260e, 0x0832fd91, 0x626e4310, 0x73eeca39, 0x83d30cb8, 0x71e6b4f8,
    0x563041fb, 0x9bcebe51, 0x407be402, 0xb4db9b2f, 0xeaec6120, 0x7f02b6f2, 0xa9cf5467, 0x78074775,
    0xa00a4a3d, 0x398708a8, 0xf6b70a69, 0x9d2929d5, 0x75347773, 0x8bf2329c, 0x3468b49c, 0x1da6b751,
    0x619d4c80, 0xfc0801f7, 0xb16da47b, 0x7a98e4dd, 0x012204db, 0xeaba658c, 0xa12a7029, 0xd33a2603,
    0xbea2f81f, 0x2055bba8, 0x555e64ac, 0x5d331ef8, 0xdd340251, 0xaaafd132, 0xb82d00bf, 0x68926b8f,
    0x2a0ebaf4, 0x953c4742, 0xe3d6cc01, 0xa10dd04f, 0xb8da7e4e, 0x6df98815, 0x4f1d3b79, 0xdd48145c,
    0xb92c4a57, 0x909a5e83, 0xd586d253, 0x3a7dd23a, 0xf5b6721a, 0x0e104c97, 0x21cadb47, 0xe5e70cca,
    0x820e368a, 0xe0eef261, 0xd1eadfd6, 0x1f1c62b6, 0x6a6174ae, 0xd055c28d, 0x1765ac7f, 0xfd6cc8fd,
    0xc1296c98, 0xdf98ccbf, 0xd79d1b11, 0x23d66a24, 0xd4bcaf04, 0x4cbc1bdf, 0x5d72f05e, 0xe3b5e77d,
    0xef6cc36f, 0x0d855f2b, 0x3819b136, 0x04bb1c5a, 0x0b31e0a7, 0x876d6f50, 0x8cd24196, 0x29c8e461,
    0x3c33a000, 0x277b5acb, 0xf72309e4, 0x9c31f50f, 0x302e14c2, 0xb06bf7b6, 0xb1a9e3d3, 0x87b6316a,
    0x3d93874a, 0x44e83daf, 0x275cc773, 0x9c872cbe, 0xa52c2253, 0x339d5856, 0x25115bf5, 0x5323a499,
    0x3df20f40, 0x8992419e, 0x5efe70a6, 0x4e078843, 0x73af23d3, 0xb6aa2804, 0x4c2d4855, 0x517da06d,
    0x2deb15da, 0xe29772b8, 0x6a122a3d, 0xf58002f1, 0x6d96ddce, 0x5ad48169, 0x76999935, 0xf5248cd9,
    0x758ad427, 0xe59786a3, 0x15b061f8, 0x59335f79, 0x606e231d, 0xd1ad0459, 0x83432f3f, 0x4323cd30,
    0xe3509b4a, 0x2452223e, 0x77ea5b80, 0x582a8fbd, 0xb0d6242c, 0x7e560bfc, 0x1c109c4c, 0x6d2848fe,
    0xdad36e61, 0x2c2bfd9a, 0x7b71f268, 0x44a6f230, 0xb824fdd6, 0x627f19a9, 0x47258d14, 0x00ad8b9c,
    0x1316fa46, 0x7e94af0d, 0xc5204a08, 0xe89a636f, 0x50ce52a1, 0xe2e2c45b, 0xf1e216bf, 0xa22bd2e8,
    0x5944cdae, 0x9f10cdd1, 0x9ae450ec, 0x858aa012, 0x70424b92, 0xf2d19f69, 0x05b0b70d, 0xc39e7092,
    0xac1b6103, 0x057a829b, 0x94d6451e, 0xa76f3567, 0xb8d9bedb, 0x13aec4d4, 0x2192ce19, 0x1a65e6b7,
    0xeb74f24e, 0xd08220d6, 0x617974a9, 0x46fe7b74, 0xa1469d66, 0x1c748d70, 0x07a3634c, 0x8be4b29f,
    0x00c81556, 0xd34744dc, 0xfa5ba04a, 0x93aa4321, 0x3a945372, 0x9fca4cba, 0x69c06255, 0x1ef140a8,
    0xec36e197, 0xb17687b2, 0xebdd7675, 0xa710bbe9, 0x5fab1764, 0xd0de1400, 0xf9673dbd, 0xf1ab62f4,
    0xf71d01f7, 0x2408dff2, 0xbda1fa76, 0xd536e2d6, 0x46209c4e, 0xbaabc9d7, 0x784d4795, 0xbdfd5a21,
    0xe40d2760, 0x482e2023, 0x73a38a6d, 0x65b006cf, 0xe48fb259, 0x12c662bf, 0x24e8f492, 0x7c691c8f,
    0xe08bdf88, 0xf41e9213, 0xefb460fa, 0x9a710ee5, 0xae1571a1, 0x239a10fc, 0xceb8150b, 0x5380ebd8,
    0xa1c1da53, 0xd87f7207, 0x97049375, 0xab6316fc, 0x44c9982b, 0x2479d9d6, 0x20de73d0, 0x34d7a6c7,
    0xb3b25194, 0xafbe7b8b, 0x625aa680, 0x851e8f3c, 0x13ad36c0, 0x2f625315, 0x34137a07, 0x1ef282ab,
    0xf2764bd0, 0x8fccef92, 0x95b79707, 0x95821568, 0x73d20b44, 0xbf30e58f, 0x22dcabe6, 0xd2b22506,
    0xc28b81e6, 0x519b59e4, 0xe4e83918, 0x208ca0c1, 0x4d7f16fb, 0xb62153a1, 0x112d036e, 0x66ea4af7,
    0xfc9a1b80, 0x99113836, 0xad4ad798, 0x2aa4da24, 0xa559863c, 0x79b396de, 0xbdb11322, 0x52278f65,
    0x792cab85, 0x54104aa6, 0x66e13906, 0x97ee2124, 0x44e07065, 0x7ea1858b, 0xc44e6151, 0xdb587a0b,
    0x48aee077, 0xfd58c02c, 0x4711f93a, 0x81966c44, 0x51314618, 0xc61b3092, 0x80f67a7a, 0x290ac044,
    0x589f33e4, 0x0731a473, 0xff9cd5da, 0xa35cd20e, 0xb2d16b62, 0x4bb5ea2e, 0xf614d13f, 0x9a406bc0,
    0x5b929c19, 0xc4fe43ef, 0x9ad61556, 0xb02000fb, 0x0c788e7f, 0xc26bb92c, 0x6f98816d, 0x4f40cda2,
    0x063f9b96, 0x14f62cf3, 0xde280218, 0x815b2b21, 0xd51b05cb, 0x7c15e350, 0xc3ab8c59, 0x6476ca77,
    0x26f0aacf, 0xfa9e5327, 0xb8518080, 0x3b9b96a4, 0x826ca080, 0x119b599b, 0x0c093a60, 0xa35a8306,
    0x9105e7c6, 0x0393c638, 0x67dd218a, 0x4337aaae, 0xc0ff5d23, 0xc07b4a44, 0x153becf7, 0xa0b00881,
    0x353b257c, 0x77d443a6, 0xfe37812f, 0x50d78b1b, 0x42204b7f, 0x0be31ab0, 0x9ec3c358, 0xac3d17f4,
    0xff6961b1, 0x8fd75e26, 0x16ecb206, 0xf3ac013a, 0x36004141, 0x4f3c2f48, 0x9e171f8a, 0x21ae4f5a,
    0xf92c34da, 0x0e6d4580, 0x23fbafe1, 0xcd57b2f6, 0x02f61258, 0x727bfb2a, 0xf8f0aea4, 0xd93280d0,
    0xcca4d323, 0x4f5c567f, 0xf75a4da4, 0x407fa711, 0xd5d42bc3, 0x0670b532, 0x9e0b1943, 0x85b6e871,
    0x73d20b44, 0xbf30e58f, 0x22dcabe6, 0xd2b22506, 0x979f13e3, 0x3e78f75b, 0xbc1b2b00, 0x289dcfb9,
    0x4d7f16fb, 0xb62153a1, 0x112d036e, 0x66ea4af7, 0x276adb07, 0x9abde0f0, 0xfcaf07ae, 0xfaebf690,
    0xd07be370, 0xa24106b8, 0xf0456a78, 0x388c4c65, 0x88bdf473, 0x7c260ac4, 0xac5a9d0a, 0x801db793,
    0x2a66a2af, 0xc8aa4bdf, 0xc810e1e8, 0x4c27feaf, 0x6deaa954, 0x00ca5fcc, 0x6c19c770, 0x2bec7ac1,
    0xcff7be5f, 0x7cc7ed7b, 0xa7671daf, 0x4e93062a, 0x589f33e4, 0x0731a473, 0xff9cd5da, 0xa35cd20e,
    0x3678e523, 0xc43ff409, 0xb3ebbde7, 0xd3ff54b5, 0x9bfc8b3f, 0x3ec5aabf, 0xde8ecd37, 0x205e3784,
    0x885fd758, 0x8d8b2cc0, 0x654e3bee, 0x87fb5a76, 0x5fd4524d, 0x356ae495, 0xbd0666bd, 0x97d30215,
    0x76801776, 0x84413788, 0x59fea8ee, 0x136c0f68, 0x7fe06dc9, 0x100a3e6f, 0x78bee5e8, 0x4be8a770,
    0xd05f7784, 0xc2207e80, 0xba2aa2c1, 0x239a74b9, 0x0b254935, 0xd34764de, 0xf9eac6e5, 0x377e9176,
    0x3e43ceca, 0x795cc920, 0x9134a63d, 0xfbbfd7de, 0xad4d420a, 0xf75e5b19, 0x35cc6f49, 0xb9b24747,
    0x52158020, 0xd3105921, 0x06ab849d, 0x875d12a0, 0x61aac0ea, 0xec88dbd7, 0xc1801f55, 0x54174039,
    0xc4d8ef98, 0xd3e0f2dc, 0x7e93c64b, 0x24639ec5, 0xa56541b8, 0x047612e9, 0x3e13a114, 0x4645bac1,
    0xde146e96, 0x1641d404, 0xeb202f1b, 0xa38f919d, 0x321bf83a, 0xe0e2fa9d, 0x8658918c, 0x71736bf1,
    0xd87fcc1f, 0x22781272, 0x8f84f3d9, 0xdf6550d8, 0xac419152, 0xf7bff80a, 0xc9a9f533, 0x5bcbd65f,
    0x820e368a, 0xe0eef261, 0xd1eadfd6, 0x1f1c62b6, 0x5d0d16d0, 0x66638537, 0xb69977e0, 0x07795a3f,
    0x6d621c37, 0xa2e6a56f, 0x0dfdf78e, 0x8a9c3189, 0x586bb553, 0x55488c9d, 0xcc8a51df, 0x1b62a47f,
    0x0e825bae, 0xdfaa9cf7, 0x8bc1f3c8, 0x66723bb1, 0xd7258f39, 0x62e6651f, 0xc4978379, 0x3a52bce5,
    0xfdce2c30, 0x0f9d99f6, 0xa75f4e56, 0x8ab358a2, 0x7d055ce3, 0x36447fc5, 0x6213205f, 0xf8915b22,
    0xb14c5142, 0xdbbf187f, 0x5c3b16ea, 0x0a5ea7fe, 0x31e216a4, 0xb7d81845, 0xe201951c, 0xfc936119,
    0xd0034c9b, 0x65a6babc, 0xc8ab6bef, 0x8dbe1952, 0x51dec043, 0x2d8a8d2b, 0x164ed028, 0xced4205b,
    0xa25d966e, 0x5cfc66ed, 0x67931772, 0x8411ade6, 0x534a8805, 0xc6ba1c3c, 0xbf0a2def, 0x2e5a6908,
    0x563041fb, 0x9bcebe51, 0x407be402, 0xb4db9b2f, 0xdd62b314, 0xb2dfc321, 0x3976d2de, 0x1752c890,
    0x02391917, 0x20ab0ed3, 0xb4d09fb1, 0x51e6bd63, 0xe1851a6b, 0x8c30c191, 0xd7e4ccc8, 0x857a70e2,
    0x72bcde3c, 0xdc07853b, 0xcab2210b, 0xb859d167, 0x2de89882, 0xce285021, 0x7ba45889, 0x662f338f,
    0x61aac0ea, 0xec88dbd7, 0xc1801f55, 0x54174039, 0x0b560909, 0x5bba4fc6, 0x896d0fe8, 0x91a8aa32,
    0xe7dd8916, 0xb0f2b2bc, 0xa1509f72, 0xf40922e3, 0xde146e96, 0x1641d404, 0xeb202f1b, 0xa38f919d,
    0x7d6a0b09, 0x89e5fc9d, 0xf838fbec, 0xde44db9e, 0x384eae7c, 0x601cb27c, 0xa9f5da05, 0xe3aaabbd,
    0xd9e30435, 0x9a4232f3, 0x6ddbe067, 0xdd7206de, 0x6609b31b, 0x01a83f1a, 0xd7cd6bf1, 0xcdc05aac,
    0x49da6712, 0xe801b8d5, 0xde7746c2, 0x4f191040, 0x04b7d6fd, 0x28ea2f77, 0xa18f39e4, 0xfea3491a,
    0x9be4bf81, 0xf233b120, 0xb9a7f5b2, 0x518174aa, 0x4de304fb, 0xfc4cc44d, 0x44050d8e, 0xac279a5c,
    0x4afde8b3, 0xf4e88403, 0xa96b425f, 0xa185b822, 0xc7916625, 0x86336783, 0x3c52fa2c, 0x6b0cc17a,
    0x5fd87a73, 0xebdb4ce9, 0xef983a42, 0xbeb54506, 0x0f26766c, 0x82d630cd, 0x2e813ea7, 0xfa3934ee,
    0x96aab781, 0x9920c17f, 0xa02bb00b, 0x7673a879, 0xe64c02a6, 0x3892e35f, 0x17ce2e14, 0x6f1afc79,
    0x4b71b7e3, 0x51c68194, 0x6d332d60, 0x9832d2d6, 0x534b60d6, 0x35f46634, 0x7a803a59, 0x20becf21,
    0x834f2846, 0x2dc5cf07, 0xe0798a1a, 0xee817f5e, 0x4c730cc5, 0x9150dfbd, 0x91d9132b, 0x8f14f852,
    0x96c4cc79, 0x28c1b10e, 0x413b50fe, 0xc5796dbb, 0xa8e7d3b3, 0x75788bf7, 0x82bf7d88, 0xf96a0470,
    0xfa2fcfdc, 0x9880ffde, 0x1054621d, 0xd06bc431, 0x0eb31d9a, 0x45cb7810, 0x60f38815, 0xcb9abea1,
    0x999d9480, 0x2953078e, 0x7c8bfdb6, 0xa3350767, 0x305f4067, 0x0eab96f9, 0x128e969b, 0x6849e399,
    0x0a094a5e, 0x1fea2322, 0x7f0c99aa, 0x842d69ed, 0xab1238aa, 0x911961ba, 0xb523711d, 0xace44d1b,
    0x616f129d, 0x82fe2f4b, 0xdc301059, 0xcf11482c, 0x4f46fb87, 0xc4ad17e7, 0x79ff1834, 0x7caec42d,
    0x68e85fb3, 0x10c38d5b, 0xc6363844, 0x5929f0f3, 0xb5206e68, 0xcf179d95, 0x3e490187, 0x24c1533d,
    0xf834d1aa, 0x5cc3449c, 0x5f52eaa5, 0x1eedd305, 0x5ab888bb, 0x3f238258, 0x17115bbd, 0x0d015921,
    0x82e8ea80, 0x30ea145a, 0xaf24229e, 0x8246cc79, 0x73d4bcda, 0x716deb7b, 0xb5546dfe, 0x7ebf6623,
    0xa6fce72c, 0x83e9e7ad, 0x0bc26492, 0x255bb108, 0x1359baa0, 0xe6b48284, 0x7f9eb587, 0xec709cec,
    0x12361acf, 0x0d18b8f2, 0x1858a0a3, 0xeb1ce86e, 0xe6fd7124, 0x7b2bbd43, 0x36327aa3, 0xbb34f125,
    0x689d3297, 0xe8fbd8bf, 0xb1340957, 0x500d7148, 0xc4a4612a, 0xdfa8d5f1, 0x73e56646, 0x01f28770,
    0x5d9110d7, 0x0dcfe9f6, 0x47e1ebec, 0xa265bc8b, 0x233b751a, 0x554074ee, 0x053fe830, 0xde4237d9,
    0x92f73ffe, 0x471e20e5, 0x65ae029e, 0x35e93920, 0x615748b2, 0x3965cc1e, 0xdd723a91, 0x9b71899f,
    0xa11bb0a4, 0x37d03eef, 0x04b05ecb, 0xb27c331e, 0x8298a580, 0xa83beb6f, 0x9b66834f, 0x7232aef3,
    0xae209cee, 0x293386ac, 0x1c4332c3, 0x13ad1e85, 0x3d5174d1, 0x8b342616, 0xa4071ac2, 0xe2b1a91f,
    0x03f3219a, 0xf7643918, 0x7d93529a, 0xc0c9b4b7, 0x3c8d5312, 0x9aeb2a13, 0xee6bb453, 0xeb8e6290,
    0xa917ee31, 0x86b04ff7, 0xe0c70284, 0x21c9ef13, 0xc111acde, 0xdf7e2362, 0x130e2118, 0x9f2a1ef3,
    0x482d0ab5, 0x7cf44a4b, 0xb72d95fc, 0x0e90d0c0, 0x43782db4, 0x1a390bb4, 0xcd8b6a2d, 0xe15d603a,
    0xa0b8fdeb, 0x73ff20af, 0x1637aed4, 0x41e2d5e3, 0xb7e43cec, 0x2109ee9e, 0xfc211cd5, 0xf908a0d0,
    0x669760c6, 0x5441e115, 0xfd7fa8f8, 0x3f3c6903, 0xd6bcf805, 0x87dcf6c6, 0x4bacde49, 0xa69f1f58,
    0x9e96f08c, 0xd825ab1e, 0xd10e0b9b, 0xef6ecff1, 0xbded4aa2, 0x93b6aecb, 0x0bc4ca2d, 0x1d560dbb,
    0xb3917c8f, 0xc24921de, 0x6eeada01, 0x5f4d20e6, 0x75ec5a74, 0x7b6fbfe1, 0x33784f83, 0xb92840f0,
    0xb0d6242c, 0x7e560bfc, 0x1c109c4c, 0x6d2848fe, 0xf810a041, 0xc7285c18, 0xa8572109, 0x859c1c23,
    0xcedd1917, 0xf5eaea86, 0x3e8367fc, 0xe44f6a23, 0xc2e0e0aa, 0xaccecfd6, 0x4b455d89, 0x92b1a312,
    0x47af89fb, 0xa02c7668, 0xe97c41b5, 0xcdf4acf4, 0x9efb69f2, 0x49f62753, 0x323401a2, 0x403de0f1,
    0xe7e093c3, 0xcecf84cd, 0x50875e8c, 0xb9ac540b, 0x120fb058, 0x27c1cde6, 0x34a9d484, 0x50018005,
    0xe4a586a6, 0x63e1e3a2, 0xef238fbf, 0x924587b2, 0x149798eb, 0x08ed02fe, 0x59a8b774, 0x167c5bd2,
    0xa73f07fb, 0x2916e1e7, 0xefd0ba82, 0x1fd87eb4, 0xace3e1e5, 0x401b2268, 0xdff2777d, 0x058482ff,
    0x1b162a4c, 0x6f8520d7, 0x76c8eaa1, 0xe877a5d0, 0xfbdece50, 0xbdc5e75c, 0x72911b52, 0x446cd7a0,
    0xd6a4889b, 0x4ec0b0b0, 0xd5986e9d, 0x307cc7d0, 0x7ddb01b6, 0x9ccf896e, 0x59c4db80, 0xb016f559,
    0x42219840, 0x18e61d05, 0x956af162, 0x3355a400, 0x292d23c8, 0x97d3f3d9, 0xb3dbe3c9, 0x2681ce27,
    0xb8c58d82, 0xf2eb13d5, 0x96f06705, 0x4b217b21, 0x7d5db576, 0x78cc9cee, 0x70f25d45, 0x95bfcfc4,
    0xfc30ee63, 0x2ee486f1, 0xf24736de, 0x39ed84c6, 0xa1dd8642, 0x3019656a, 0x4f977066, 0x62b76fda,
    0x0e97c2fd, 0x06f50c67, 0xace352f8, 0x021dc0f7, 0xf0771b6b, 0x2f1fac23, 0x3e7e7d5f, 0x78950d8e,
    0x94d435ef, 0x3ed44f14, 0x8e3d5dff, 0xbc678987, 0xed9da5f5, 0xc57f00d9, 0xe7d0335b, 0x970a350d,
    0x18ba2e71, 0x0bb59572, 0xe70707b6, 0x89d84c8e, 0x4bfac603, 0xda4c5801, 0xb89e6541, 0x0ea7d41a,
    0xf83ac73f, 0x2435828f, 0x9fa2e6e4, 0xfa05dbcb, 0x7113f9bd, 0x72807aa6, 0x2866e868, 0x78981595,
    0x1f76aa1d, 0xb80a689f, 0xf23b08c0, 0x1aea1026, 0xfe13468b, 0x20590ad1, 0xe539dfe8, 0xb8aa76b3,
    0xb72394d5, 0xedfb711a, 0x266a833a, 0xcf80eead, 0x45d64535, 0x8dad053b, 0xa39b3552, 0xe66e9387,
    0x7fe47476, 0x27678f81, 0x706c5088, 0xb8a59c80, 0xc35fc8af, 0x576b8044, 0x13480bf5, 0x1d4b9366,
    0xa8695c57, 0x23a6daad, 0x4e813718, 0x3b61f93a, 0xb37b7a18, 0x2cfbeda1, 0x042f1d8b, 0xe0e97eb2,
    0x7a46d133, 0xd66a2002, 0x26bf221f, 0xf93c2d21, 0xf49164a9, 0xd3fc6628, 0xa41c4b3c, 0x30bdbf17,
    0xc65a99b7, 0x7ecc13d8, 0x99f45064, 0x70a917b5, 0x731d10df, 0xa21f9eb5, 0xe746fa4e, 0x34d43be7,
    0x81a452ac, 0xcc73ba0c, 0x1de7b7ca, 0xf509f5c0, 0x2f9f63c2, 0x5f3852e1, 0xa8ec79f5, 0xe3577b48,
    0x84e62579, 0x405dc081, 0xceffd627, 0xb8e11a38, 0x4ccf160f, 0xcad81a91, 0xd245d622, 0x4d31f95f,
    0xbef53341, 0x3a91b06b, 0xb477ea5e, 0x234689df, 0x2000cfa8, 0x3e797ed7, 0xdcbd5ef7, 0x987e3d5d,
    0x61ca967f, 0x681c2856, 0xb64b0c4a, 0x4e3b00e5, 0xae431dbe, 0xb0c51bce, 0x0622dc33, 0x325f1d13,
    0x347ff8b7, 0xee9d22f4, 0x13ce5004, 0x9a0752c3, 0xf284e2b2, 0xba9ef6e1, 0x6e141e98, 0xc5039ec1,
    0x8da6c98a, 0xb5ce5f2c, 0xbcb9a9ed, 0xe35c812a, 0x6f467031, 0xb8325e5e, 0x498fdce9, 0x457a7e94,
    0x0d597546, 0xc2564040, 0x26bed24a, 0x1777151c, 0x88b7c9a6, 0x84c13cb3, 0x286ea000, 0x850a2b9c,
    0xcaadd942, 0xb56cf076, 0xfc5bd561, 0xa156439b, 0x23eb6e4e, 0x442f4000, 0x6b561d5f, 0x722189dd,
    0xc2c939dc, 0xe5384c2a, 0x28490ddd, 0x7e907477, 0x26298ca2, 0xf3d15179, 0xad83cd9e, 0x190fec57,
    0x9f3b38d1, 0x2e4787a1, 0x3d323dea, 0x52d45f4a, 0xa1cae708, 0x3cf50b6d, 0xd73107c0, 0x322a4210,
    0x721ecf74, 0xb84f7aac, 0x04d8077b, 0xa74dd0e1, 0x63a2da92, 0xebcfffe4, 0x67aeb64c, 0x48135ab9,
    0x0591c62a, 0x48309b30, 0x6b9e3675, 0x4cf33773, 0x3dca82ae, 0xa868485e, 0xf5205366, 0x05d02e1b,
    0x718ebd1c, 0x47108650, 0xd8f85df0, 0x21bc5d22, 0xf84dde78, 0x109120ad, 0xcc23f640, 0x43d36af6,
    0x77e6f035, 0xb8ed17a0, 0x64f84e51, 0x12474797, 0x761348c2, 0x01994658, 0xfb34c292, 0x79eb8286,
    0xd7f06059, 0xc4814140, 0xeaa1861c, 0xdd8a266c, 0x738e20c9, 0x11fdacab, 0xaaaf4b7e, 0x057087a1,
    0x4c85a624, 0x739df348, 0x68b55cdd, 0x98fd1b26, 0xc024fb78, 0x9e01a1c3, 0xcbc98669, 0xfc1781cd,
    0xb6853752, 0xb4038bcc, 0x7248dccf, 0x94028d02, 0x87d892ba, 0x6015ea02, 0xf1179444, 0x07bc1723,
    0x4675272f, 0xa8fdd626, 0xf3fe545b, 0x63cb75d5, 0x63a2da92, 0xebcfffe4, 0x67aeb64c, 0x48135ab9,
    0x0591c62a, 0x48309b30, 0x6b9e3675, 0x4cf33773, 0x2c79a018, 0x76edfde7, 0x7bb90d1d, 0x7d6a4995,
    0x0435ecd6, 0xcbad21e3, 0x682bc87c, 0x7f260136, 0xf84dde78, 0x109120ad, 0xcc23f640, 0x43d36af6,
    0x625e6255, 0xd51645b6, 0x2084d82d, 0x84a34428, 0x14615bd5, 0xb31000ac, 0x1c8d0645, 0x13487def,
    0x8bfde723, 0xa236c02d, 0x745c0416, 0xe309b43a, 0xe29dc9d5, 0x575fd477, 0x100f43f1, 0xb633e209,
    0x4c85a624, 0x739df348, 0x68b55cdd, 0x98fd1b26, 0xd590fef5, 0x16256b9c, 0x95b92850, 0x3d41bd00,
    0xd57abb58, 0x1027de4c, 0x7307cc16, 0xa7e0d9fa, 0xebe344bb, 0x8ae0e9d4, 0xd46c45bf, 0xa3fbe7d8,
    0xc9079188, 0xa27ea083, 0x5a3fc505, 0x6512a212, 0x6d0d7b39, 0x1ad036cd, 0x1c235520, 0xbdee8c25,
    0xfab33b03, 0x5f2cdf53, 0x8a116068, 0x4b367638, 0xe794969b, 0x9e03a8da, 0x6c1d7ba0, 0x42b04d2b,
    0xfbdece50, 0xbdc5e75c, 0x72911b52, 0x446cd7a0, 0xd47bd906, 0x3b40e764, 0x6eae034f, 0xc671f4d8,
    0x84773c2d, 0x47d53b9e, 0xd3987c3e, 0x8cfddcee, 0x9dcbf9cc, 0x473dfdd4, 0x350d4f87, 0x3f6fa8e6,
    0xde4071a6, 0xd0b797aa, 0xc64e5437, 0xb22ed02b, 0xd0a53d0e, 0x25705778, 0xafb290cf, 0x02030e54,
    0xc5e30a62, 0x5bd6ef0d, 0x13b157c8, 0x0b6810a3, 0xabaaee96, 0x1fb270ce, 0x8c4321ce, 0x6c841210,
    0x3aea0e7e, 0x21a93e0e, 0x5f50853f, 0x1359093c, 0x1eb80e0d, 0x9c8c69a7, 0xf7d2d54d, 0x9b8ea970,
    0x7d055ce3, 0x36447fc5, 0x6213205f, 0xf8915b22, 0xb4442398, 0xb392b7e7, 0xe5b76930, 0xf465ec76,
    0x20e55631, 0x82e0a2c2, 0xd6eeef30, 0xd87292ef, 0xcfaf4334, 0x43b8731d, 0x88dd6981, 0xd14b70ba,
    0x50b53f58, 0x68dbaf66, 0x80c8a6d6, 0x6db39f2d, 0x834f0956, 0xe6436150, 0x10230e98, 0x90f8aa4a,
    0x19218c2f, 0xc4c17b35, 0x94a12a1f, 0xa163b95b, 0xb86aa8b9, 0xbbc3120b, 0xba4426b4, 0xed57622c,
    0xee6830b5, 0x0fe4d52e, 0x29c9bea6, 0x00c2a5f0, 0x11c66171, 0x94f17202, 0x0a777ca2, 0xbeb3b5da,
    0xf13f13d3, 0x8f94ac02, 0x06d77b2f, 0x066413dc, 0xdd875030, 0x68a0d84c, 0xd204b5b3, 0xe96a5114,
    0x61aac0ea, 0xec88dbd7, 0xc1801f55, 0x54174039, 0x2190487c, 0xce13ad35, 0x3048ed7d, 0x38e35f97,
    0xb4244c78, 0x3de552e2, 0xb2ee6b9f, 0xbf91c116, 0xf88f88c4, 0x36576fba, 0x56f53c98, 0xe72f79fb,
    0x778ce706, 0xd67b4c60, 0xd76574c8, 0x49aebfd5, 0x79c35ae2, 0x8fd0acff, 0xd6c99f35, 0x29576381,
    0x1638eba0, 0x7155aa43, 0x3eec1735, 0x3ef1aff6, 0xc347b33b, 0x1ac0e41e, 0x2c944af8, 0x5e8a9d43,
    0x70424b92, 0xf2d19f69, 0x05b0b70d, 0xc39e7092, 0x81f98545, 0x1755eadb, 0x959e264e, 0x158da270,
    0xfec1cc64, 0xcdcdee6f, 0x346183fd, 0x1584ca76, 0xfad32bf2, 0x9149ee41, 0xd4d9b015, 0x6a13cb35,
    0xeef26cf6, 0x08c4fb98, 0xed1f6723, 0xb486cac5, 0x70865971, 0x73a0c5a1, 0x6c27461b, 0x61db82d5,
    0xb9f848d2, 0xb7c4088e, 0xbf9a4451, 0x6252a2c6, 0x84e6cf64, 0xf2299d80, 0x2cc935ca, 0xfdb6f617,
    0x63505833, 0xcc181dcc, 0xe45d73d5, 0x12c995f8, 0x9766396a, 0x560341de, 0xfde39424, 0x3703091e,
    0xa2b64b45, 0x8ea6c9ef, 0xa9d40d84, 0xcad04498, 0x2e7608e4, 0x5218949f, 0xee7fcc43, 0x397d6333,
    0x1052ee85, 0x663ac501, 0xfc286e70, 0x2d59d57d, 0x4497df80, 0x07296b34, 0xe940cc74, 0xa905bd91,
    0x04cc38c5, 0x6b3f912b, 0xbcee6cce, 0xf6ff3ba0, 0x235b93ab, 0xd461a899, 0x8f39e163, 0xd641d9b8,
    0x3e0b5580, 0xed74351c, 0x028bd20d, 0x19428452, 0x4a24b8f8, 0x4be0be3e, 0xe97f11b3, 0xdc3dac83,
    0x1ab53834, 0xc39694f3, 0x676bf1e2, 0x0b0bef41, 0x1ff68c86, 0x4d969fb8, 0x4abe6db1, 0xec751da1,
    0xf4a8ec5f, 0x8df0c228, 0x9a26142d, 0xca707655, 0x5a3d8df5, 0x67925c95, 0x7c352167, 0xeae8d17e,
    0x6b961b24, 0xae91bd2f, 0x60bee5ef, 0xde835216, 0x240e4117, 0x8b3dbdd8, 0xf3d4360c, 0xf4e97d42,
    0xa6f4495b, 0x80489acd, 0x6976f8e8, 0x957556cd, 0x42166ea3, 0x340195b0, 0x4615548b, 0x7e5495e7,
    0x788c9ecc, 0x29ceafb6, 0xd801390e, 0x6ddec6bf, 0x53ea2061, 0xd1903e88, 0x2344650f, 0x7f88b31e,
    0xfca36f06, 0x5b82fe7e, 0xc45fde0f, 0x27d5d20c, 0xbc9d7158, 0x4c6d327f, 0xa659a2b5, 0x0f063bd8,
    0x76180e23, 0x4614eb57, 0x99190f8a, 0xec6c7047, 0x9d79c25f, 0x6d9e054b, 0x06344334, 0xe9b2cd95,
    0xae302cfd, 0x4af70ee3, 0x72cae138, 0xbb328a81, 0x3dea0845, 0x2a12f995, 0x9ce60655, 0xf4154129,
    0x23aa4454, 0x54b4700a, 0xa98de594, 0x6b86f05e, 0x2a03bdee, 0x2d09e659, 0x4574a0a7, 0x369ee273,
    0xe07fe398, 0xe47e85b3, 0x7e1477a8, 0x46e86d2e, 0x36004141, 0x4f3c2f48, 0x9e171f8a, 0x21ae4f5a,
    0x3fa75bb3, 0x3116cb39, 0xbd13b403, 0x9437d587, 0xa4f780a8, 0x37b1a727, 0x691f9c2c, 0x0056071d,
    0x2458998b, 0x718fbff2, 0xbc97c3e1, 0x4aa6ec3d, 0x8bd72821, 0xce186bd7, 0xd3de2f8e, 0x3fd15990,
    0x73d20b44, 0xbf30e58f, 0x22dcabe6, 0xd2b22506, 0x7efc7a4c, 0x6171d9c1, 0xe03dadd6, 0xb97c33c4,
    0x4d7f16fb, 0xb62153a1, 0x112d036e, 0x66ea4af7, 0xff88c097, 0xecad648b, 0x85aa63e5, 0xfbe7fc92,
    0x0b8c8f69, 0x9fc32b69, 0x888eb8ff, 0xaacea397, 0x953086e0, 0xcf320818, 0xb2d7846d, 0x4c21edd8,
    0xe04b3a5d, 0x1f73eb92, 0x5b304727, 0x8b760af7, 0x48aee077, 0xfd58c02c, 0x4711f93a, 0x81966c44,
    0xf2310df5, 0xe33efc3b, 0x097faad3, 0x3a34daa7, 0x589f33e4, 0x0731a473, 0xff9cd5da, 0xa35cd20e,
    0x5d90bb2f, 0x2597adfd, 0xdfa5b53c, 0xf833558f, 0x97b1a7a4, 0x56698d4f, 0x99b8b301, 0x26f33d94,
    0xe8fb38f7, 0xba89ab4c, 0x500c9db5, 0x1f6740b3, 0x77649891, 0x4f56937d, 0x2e977501, 0x1feb9bf0,
    0x9f5ef161, 0x3921d85e, 0x7cffb979, 0x34608b93, 0x49443286, 0xecd0c467, 0x20b4d922, 0xf171c8d2,
    0x826ca080, 0x119b599b, 0x0c093a60, 0xa35a8306, 0xa8a1116f, 0xcd6c006d, 0x90972d00, 0x1cd2da9a,
    0xc0ff5d23, 0xc07b4a44, 0x153becf7, 0xa0b00881, 0xf91e4e57, 0xe0cb8ba9, 0x9f3d756d, 0x8d906c17,
    0x6855f31a, 0xc19353b8, 0x07710ee1, 0xdf95e6e0, 0xdee40529, 0x0e765fda, 0xb683aba3, 0x98c2caf1,
    0x36004141, 0x4f3c2f48, 0x9e171f8a, 0x21ae4f5a, 0x0dde2e3b, 0x7f56cf5e, 0xf57a77ad, 0x8b860bbd,
    0xba64cbbd, 0xd1b9e8ec, 0x29b92343, 0x8c8d62ef, 0xbae03628, 0x0eb2b323, 0xf2ea7eae, 0xd93beff8,
    0x70b17720, 0x31514b34, 0xa014a628, 0x411b3cff, 0x73d20b44, 0xbf30e58f, 0x22dcabe6, 0xd2b22506,
    0xb3a86601, 0x6b99335d, 0xa5bb5186, 0xea085ec8, 0x4d7f16fb, 0xb62153a1, 0x112d036e, 0x66ea4af7,
    0x237eb7eb, 0x5ad11761, 0x15e3b089, 0xe92e8f65, 0xeaf9519a, 0x7b496e94, 0x0af39a3a, 0xb7985dc0,
    0x0a9934b0, 0xa4628959, 0xda25cfd3, 0x1a045989, 0xa5add0c7, 0x3bbe02b2, 0x07f45097, 0x0cbe87b6,
    0x484d5bf3, 0xd65a16f6, 0xe5e71a24, 0xae04e62a, 0x1894b6d9, 0x50c6040f, 0x3c942526, 0xf065817c,
    0xacaa7637, 0x1ce2dc0f, 0x198496b2, 0x27b4be86, 0x05c04b1a, 0x080e5854, 0x03441d3e, 0x6cc1b944,
    0xae8dba43, 0x47b30462, 0x072e3028, 0x6bf6aa9e, 0x96be6234, 0x096a3232, 0x6d2f5d09, 0x8569c4b6,
    0x64e79a9f, 0x2f7be1a7, 0x939c3f1c, 0x2601df5b, 0x5ad7750f, 0xee5e836e, 0x67c09d36, 0x95a5f49a,
    0x563041fb, 0x9bcebe51, 0x407be402, 0xb4db9b2f, 0x2348c88a, 0xa96392cf, 0x7a12d674, 0xd02c01de,
    0xb7908b96, 0x6534f8dd, 0x5d7b9f71, 0xfddff13a, 0x089ee3ff, 0x4596c5f4, 0x51663f59, 0xee323cd0,
    0xd599bfc7, 0x51b45723, 0x8cbd43b5, 0x1616e121, 0x3883a653, 0x994fbf7b, 0xf5bd58e1, 0x24efb3a6,
    0x61aac0ea, 0xec88dbd7, 0xc1801f55, 0x54174039, 0x0ec5fba4, 0xfda53eff, 0x0f844fa8, 0xeb36d3bb,
    0xb213eb8f, 0x1b8809df, 0x1cc727a0, 0xde6a0780, 0xde146e96, 0x1641d404, 0xeb202f1b, 0xa38f919d,
    0xa712f0b0, 0xfb7f0d7d, 0x07a1e3d5, 0x4cb4446a, 0x751de1f2, 0xb5da9650, 0xbfe94309, 0x000fefe6,
    0x7044b14c, 0xd65cd959, 0xa40f4221, 0x491e4b65, 0x820e368a, 0xe0eef261, 0xd1eadfd6, 0x1f1c62b6,
    0x5daa1bd3, 0x18669683, 0x5a081720, 0x4e8b65ca, 0x7eb026a6, 0x66686426, 0x52cca441, 0xc39ace29,
    0x7e81a3c3, 0x01f8143e, 0xb609bd49, 0x27c969ce, 0x116315b4, 0xaae8cc10, 0x080eaecf, 0x3233f02b,
    0x3e1c7986, 0x73ce7ce6, 0x0119ea4d, 0x88235b78, 0x6f43bc8f, 0x14ebe7cb, 0xba1e13b3, 0x0ab5d1c0,
    0x7d055ce3, 0x36447fc5, 0x6213205f, 0xf8915b22, 0xf3e42a60, 0x0e7266bc, 0x5865d726, 0xc2013c21,
    0x04ae7d04, 0xdbf62251, 0xe80f0a05, 0xd400b8c5, 0x9a4e3b59, 0x0640532f, 0xff40f587, 0x27666132,
    0x7aa0c1c2, 0x6f8d6c65, 0x01f79d85, 0x393f42cc, 0x21d20aae, 0xf0978cc6, 0xb72f9846, 0xd2dfec95,
    0x98cf490d, 0x2bb7d8b7, 0xb8a53618, 0xe5b26cac, 0x563041fb, 0x9bcebe51, 0x407be402, 0xb4db9b2f,
    0xecad5bbb, 0xb799d633, 0xad0aac37, 0x5df4e417, 0xb7908b96, 0x6534f8dd, 0x5d7b9f71, 0xfddff13a,
    0xcd525b93, 0xbcd0eb80, 0x1dbb328a, 0xbc3d35b8, 0xd7bbb20d, 0x3f565fd7, 0xfae9f8a8, 0xe805813f,
    0x64b4cc81, 0x1e9b2506, 0x81af7779, 0x1a073317, 0x7f52431c, 0x94b6d3d3, 0x3d20bc3e, 0x048e7557,
    0xe562044e, 0x30d841c3, 0x4506a531, 0xa513c6c1, 0x1a226b39, 0x123e2ca1, 0x3a9918b4, 0x51705873,
    0x1c4c7ce8, 0xa0b0a697, 0x0ab96eb1, 0xb3f58e3e, 0x9236a3fd, 0xabdbfde5, 0xa5cacd9d, 0x77be0f3a,
    0x858eb1bf, 0xbe16466c, 0xb5b8efd8, 0x7acad320, 0x56aa4598, 0xc98766cf, 0x3053665e, 0xad8d1f0b,
    0x1728707d, 0x53b9ad72, 0xbb4f2637, 0xadb5c79c, 0xf4b02fd4, 0x241a1127, 0x26495397, 0x328504ac,
    0x70021e3e, 0xd2607ee0, 0x451ee806, 0x9d11569b, 0x7dd6dae4, 0xddfc4fe5, 0xa4c67b2b, 0xebaab0d4,
    0xb3d20365, 0x80a21d23, 0x1fa803ec, 0x8aaf39f5, 0xfef067a8, 0xe57aece7, 0x5b19f982, 0xfcafccc0,
    0x716e91f4, 0x093dca62, 0xead17a61, 0xde2faf7b, 0xdc24cc19, 0xa9a634ae, 0xfea218f8, 0x48085911,
    0xd34f7212, 0x789404a0, 0x799c1eec, 0xfc0b71a9, 0x542872ff, 0xecfcc247, 0xb4bc4fa0, 0x96b3798a,
    0x44c8825a, 0xcfa0929a, 0x13ef6262, 0xc0165efd, 0xc3ad8f9d, 0xf1b52d42, 0xe87f22e0, 0x6ae9ee3d,
    0xa8ae51e5, 0x6cb6ccd7, 0x2c6d0435, 0xc1a68219, 0xb57dc660, 0x90b14ba7, 0x6e840e85, 0x1767ba4e,
    0xe8ac2567, 0x88df6c0d, 0x37e452a3, 0x1486611e, 0xa36c4326, 0x0a16e543, 0x276937aa, 0x82c0bde6,
    0x89b66007, 0xb68ba3a2, 0xce5f47e2, 0xe8422dbe, 0xc1af8030, 0x69a8029c, 0x1de7680e, 0xd676d0b4,
    0xe0fb1aea, 0x4f15c173, 0x73cdad7b, 0x04dc8835, 0xc1014214, 0xa756b314, 0x4b0a8bf5, 0x4abeb43b,
    0x28a1347c, 0x23102b84, 0xb416a254, 0x7c1de6a7, 0x6609b31b, 0x01a83f1a, 0xd7cd6bf1, 0xcdc05aac,
    0x4cc12fb4, 0xe251391b, 0xd195f7c1, 0x92df9b28, 0xed815892, 0xe84e0ba3, 0x6ee4ae62, 0x90033b61,
    0x26b9b068, 0x85def904, 0x02ac6922, 0x78b6a7e3, 0xb0d6242c, 0x7e560bfc, 0x1c109c4c, 0x6d2848fe,
    0xa31610b9, 0x2372ecf6, 0xf22e2da9, 0x84fdb34a, 0x88beecae, 0xfd014d1a, 0xf1e1fde9, 0x1109fedd,
    0x9d07b9aa, 0xc6487e10, 0x40785b96, 0x549f643a, 0x8f797e06, 0x1ac6c603, 0x2dad4339, 0x805c1117,
    0xf63b4ed8, 0x13af3624, 0x2da9dae8, 0x049c6ffb, 0xcfdf074b, 0x26487b78, 0x50047143, 0x7d0ca23a,
    0xfba55f31, 0x29f40332, 0x75a84626, 0x7107c33f, 0xb6c02fb6, 0x2567891e, 0xfc512e9e, 0x22cdc7d9,
    0xdcb67356, 0x29a87673, 0x340ff89d, 0xaf7280cc, 0xed2b5c3e, 0x19e1edd2, 0x0b8b1dfa, 0x288352f0,
    0x6e630d1f, 0xb3a843c1, 0x82cd68af, 0xde425512, 0x41187bdb, 0xe23303cd, 0x2d0ca7da, 0x69dca165,
    0xe01aad2a, 0x01a1bae4, 0x2023bf13, 0x61283449, 0x778ce706, 0xd67b4c60, 0xd76574c8, 0x49aebfd5,
    0x4369c9f9, 0xb8894a29, 0x94fb1a3c, 0x4f8edaad, 0x1885cf18, 0x127a6ff5, 0xa408d293, 0x91bf7860,
    0xc9fa3862, 0x235b591d, 0x3094d9cf, 0x9fa00236, 0x70424b92, 0xf2d19f69, 0x05b0b70d, 0xc39e7092,
    0x607c4eb0, 0xbd2bd6ad, 0xa0a58186, 0x10da723c, 0x99d79b2d, 0x26b3df10, 0x4086279b, 0x73d36f35,
    0xf07b6a0c, 0x9d02166c, 0x59b018c3, 0x435601ff, 0xdf01aff1, 0xefb0a1d3, 0x8d92db59, 0x7b429c81,
    0x6fe442f3, 0xb731e583, 0x1228d430, 0xa7229dc0, 0xd60d2d08, 0xfc4f2bc9, 0xbb29b60d, 0xed9a4a01,
    0xe6af56de, 0xe80f051d, 0x5eb2f33c, 0x867968ce, 0x2ccc210f, 0x02c0bcaf, 0x8983c28c, 0xeccfed43,
    0x9766396a, 0x560341de, 0xfde39424, 0x3703091e, 0x142ed841, 0x56cb14db, 0x2f251d4e, 0x1e8327b1,
    0x86c02713, 0x8beb29c1, 0xdcb87ec8, 0xecf4c2ca, 0x7efe70bf, 0x3f5e66a1, 0x1c7d6905, 0x06fc3c01,
    0x38bc37b1, 0xd9936b41, 0xbca9c405, 0xb6aaaa86, 0xfb327d0e, 0x3e1f2a14, 0x184f2487, 0xe7c6fb8e,
    0x6b7c8f46, 0xeb8089bd, 0x6a5237da, 0x2bb748e9, 0x329c769d, 0x48e292c9, 0x0d300af9, 0x13950880,
    0x7737b13b, 0xbec139ee, 0x65522dd5, 0x687af9f8, 0xf0d94e53, 0xc37f0a3d, 0xe9240ae2, 0x277f79aa,
    0x6c7694f5, 0xe0a07838, 0x968ecc45, 0xf35ceedc, 0x4ea673ab, 0xa4e7c24a, 0xa72e562a, 0x29fd0200,
    0xed9ccbd3, 0x6991cac5, 0x43072d47, 0x0bfe9556, 0x02f00853, 0x5c9b845a, 0xb22331f4, 0x14ab9145,
    0x7ec9451d, 0xe3a3fb95, 0x4ba5c1fc, 0xf14e810b, 0x076bcab4, 0x69363b3d, 0xec08b0fe, 0x49031ab7,
    0xb820e18b, 0xc8132084, 0x73ac413e, 0xc1b9cf3b, 0x22957911, 0xfaa8fd48, 0x7529b632, 0xe998294f,
    0xc634245f, 0x3dffaa01, 0x5c40caab, 0xc7b623f9, 0x3ef4278f, 0x1a10ec77, 0xaf869ff6, 0x3440b8a6,
    0xa63d78ff, 0xb8d70ea4, 0x0b03a0b7, 0xd39346fd, 0xd617a7d8, 0xf56ef573, 0x4fae7a7b, 0xa8f5bfd6,
    0x1ceee364, 0x4c61fd07, 0x653dfe0c, 0x727d91d9, 0xae24feac, 0xf516e1ca, 0x5f1f12d8, 0x504c51fa,
    0x3fffeeba, 0x73f4f48d, 0x21e393ca, 0x7c6a7ba4, 0xf589ffe9, 0xadb3e069, 0x13a8520f, 0x40121018,
    0x0c674d1c, 0xa42d04b2, 0x4b1c5168, 0xc1fd4d75, 0xf707a738, 0xbd66c58d, 0xa8ff0534, 0x14ea4c4b,
    0x7d9809b0, 0x638de06a, 0x0a614baf, 0xbe05a4e3, 0xc986fbdd, 0x15bdf22c, 0x8b8b68a8, 0x350e4c1b,
    0x0d7cb140, 0x8b0d86ec, 0x00c85c8a, 0x5e0fe146, 0xb3aa6e33, 0xf40a6dcf, 0xc69d8968, 0x9aa58088,
    0x3034f47b, 0xeecd51a7, 0xbe6fcbb7, 0x47e1520a, 0x44a58feb, 0x13e722c2, 0x246f2609, 0x6766ce19,
    0x23c14f05, 0xbcf90e9a, 0xe5bf5bc1, 0x92620bc6, 0x5dd67a29, 0x5f3a5b36, 0xd4ddf297, 0xbf32f799,
    0x6f8991dc, 0x75704c6f, 0x178f28c0, 0xe96e1e1f, 0x9ef5feb0, 0xe94a6410, 0x3b3ac0e9, 0x7107f424,
    0x8e817a01, 0x43e2f249, 0x2d9a769b, 0xd408b0f1, 0x5f731e00, 0x64738b01, 0xef02be10, 0xd1acc467,
    0xfb5e7a4d, 0xafd653ac, 0x6c4bb76f, 0xaa89d7d3, 0xbb3e13f8, 0x0e1fb14a, 0x8347ec8a, 0x785ff2ec,
    0x6bb9c3fd, 0x1e4a9fda, 0x7fe4866b, 0x5d035714, 0xe5f4624d, 0x3d4c6e44, 0x570b0b0d, 0xc3d0aea6,
    0x18640c54, 0x0733e7ab, 0xac70aac3, 0x37aa1be0, 0xc88a1e84, 0xd30dfb1f, 0x49c7a6d4, 0x18d9efa1,
    0x30fb0613, 0x065d0ea3, 0xa2b29b33, 0xe1dbfb2a, 0xc4bfe6a8, 0xd30f006d, 0x25bf4c8d, 0xcceee075,
    0x2c82904a, 0xdf4ed1dd, 0x54d905fb, 0xeb57c697, 0x021adc91, 0x14207c0e, 0x5f1461b1, 0xbd4e3f44,
    0x12f318cb, 0x68ca7da5, 0xb4356156, 0x44bc4be9, 0x17c0e644, 0x8279c7d3, 0xb278dcc1, 0xf0598dff,
    0xd971893d, 0x96239e5d, 0x3f2c515a, 0xaac698ec, 0x4c7d4a10, 0xf0d3d2d1, 0x56f4bdac, 0x7e231261,
    0x3ab33dee, 0x9293dca7, 0x60dd2fe5, 0x5a57e179, 0x42417e5a, 0x9049f906, 0x5397f2e2, 0xd43f93bc,
    0x1de02845, 0xfdd35875, 0x639980db, 0x0a2875c3, 0x741445e1, 0x8de86054, 0x506a88f4, 0x2b01b641,
    0x6afea955, 0xd80b20b5, 0x75b5bbf6, 0x41b1af76, 0xec5c5f1e, 0xa69bb716, 0x55271800, 0xb4bf56d4,
    0x77c7ab16, 0x1ea1a410, 0xe796076a, 0x34318b66, 0x08e42b38, 0xee0301dd, 0xf3bb1386, 0x7d595385,
    0x8b03448b, 0x2a94ec3d, 0x18168245, 0x04a4c056, 0x0af8ab59, 0xecd58333, 0x577fcc08, 0x9419b8fd,
    0x85b58a26, 0x6ccd43e0, 0x97d38875, 0x2c4e8652, 0x0e915d2d, 0x88025f94, 0x249043f7, 0x51dc4972,
    0x90b7c114, 0x009a7ab0, 0xdbe69e10, 0xdf636688, 0xf49164a9, 0xd3fc6628, 0xa41c4b3c, 0x30bdbf17,
    0xadcb3c93, 0xa104c2a2, 0xe3dd2a7f, 0x612231f2, 0xc2e6dbdd, 0x27af3b89, 0x0965d59c, 0x5f65f154,
    0x9c2b5461, 0x16941ea9, 0x3e5837bd, 0x6d75c8e3, 0xc281e847, 0xa38738af, 0x5bd168e9, 0x2474d43b,
    0x2609a0c2, 0x5fd937e8, 0x4a6ead2d, 0xdb645b09, 0x42cab0c5, 0x27037644, 0x0109d399, 0x20ee9601,
    0xc34321aa, 0x50e887f7, 0x9a4df829, 0xeef466a7, 0x090866cf, 0x9277576f, 0x56e4c2c8, 0x3a0a4216,
    0xcb2c2d69, 0x0e3b8d24, 0x6f4e43fd, 0xc108581e, 0x0f6a8bb3, 0x6fe248c6, 0x768e16a3, 0x65bce750,
    0x85b58a26, 0x6ccd43e0, 0x97d38875, 0x2c4e8652, 0x13bf8b3a, 0xf44aaa32, 0x93f0fb80, 0x392aa1e6,
    0xbec91d3d, 0xb8cb8b56, 0x34e8c285, 0x82f34289, 0xf49164a9, 0xd3fc6628, 0xa41c4b3c, 0x30bdbf17,
    0x26526e61, 0xe09a2cff, 0x8675f4d4, 0x99e34d83, 0x0f905b2d, 0xc11c5139, 0x038317c5, 0x5accd250,
    0xe46964f7, 0xb85c7613, 0x68b1abac, 0x3f716d57, 0x826b2bc5, 0xcbcf1944, 0xb430316d, 0x3643b972,
    0xf501e314, 0x48d5ab41, 0xe2c054b2, 0x18b4dc1e, 0xb45306fa, 0x52c944c6, 0xcf130aec, 0xb159f814,
    0xa5b2d93b, 0xa938231a, 0x6dad36d0, 0x0545f218, 0x4c1680c0, 0x89c93852, 0x451d3cb5, 0xf7a5e350,
    0xdac8d1b7, 0xa063e605, 0x420c7dae, 0xee3ca83f, 0x8b3da802, 0x8abee13b, 0x8f35850d, 0x80024d8a,
    0x027aeb5e, 0xbf72fb92, 0x26243087, 0x44dc898a, 0xb6e04219, 0x38710000, 0xd7d0e47c, 0x5b03c586,
    0xf486af9b, 0x44f46700, 0xf78fb4e2, 0x93aee5af, 0x4e4d4011, 0x8760b002, 0x7666cbb8, 0xba2ad8e5,
    0x01292641, 0x61d2ac75, 0x629587de, 0xf948506c, 0xd1d33211, 0x18d810ea, 0x028ee0c4, 0x2301b1be,
    0xbea2f81f, 0x2055bba8, 0x555e64ac, 0x5d331ef8, 0x7b8c9d8c, 0x048c1a1e, 0xc6844eb8, 0xf3c4e869,
    0xd8f96a11, 0x38e06876, 0x252d82d4, 0xbd83e1b1, 0xc2e6a198, 0x517048b2, 0xe1f3027e, 0x0a769cd5,
    0xe2c89fc5, 0x2130f735, 0xc0b2e481, 0xb49f0efe, 0xcf58fb11, 0xd345e061, 0xd8b06216, 0x5d0e91b3,
    0x820e368a, 0xe0eef261, 0xd1eadfd6, 0x1f1c62b6, 0x6a6174ae, 0xd055c28d, 0x1765ac7f, 0xfd6cc8fd,
    0x69463e3c, 0x78e7b23a, 0xf11f1132, 0xc7c1f0ec, 0x844db545, 0xb656e536, 0x1b02d6c4, 0x67ab79e5,
    0x578fe7b1, 0x876f4a3d, 0xae23044b, 0xea2c599a, 0x3cc33a93, 0x2cd93e1b, 0x0b2babd5, 0xf4e80b74,
    0xf0dfd28e, 0xeb1c8156, 0xb827ffe6, 0x77a99ad8, 0x5e41c889, 0x045268aa, 0xf8d88232, 0x90a2dda3,
    0xa1833d24, 0xa9562595, 0x7d406788, 0x744589e2, 0x9e37f16f, 0x74b6e93f, 0x930d3e5c, 0x76ec4b37,
    0xa08104a7, 0xfc977480, 0xafdc9158, 0x748c0542, 0x53dfd96c, 0xba37e2f1, 0x0d33f693, 0x15c8775d,
    0x6bfdff15, 0xfde28b11, 0x15d08726, 0x8394720a, 0x563041fb, 0x9bcebe51, 0x407be402, 0xb4db9b2f,
    0x58085f1b, 0x2d0f9887, 0xa854cb60, 0x75ccd7ce, 0x3bea2136, 0x170f0d2d, 0x5d04319f, 0xde3086bd,
    0x75347773, 0x8bf2329c, 0x3468b49c, 0x1da6b751, 0x875fcc15, 0xdbdfe2bd, 0x37ddcde0, 0x90ea50b6,
    0x4497df80, 0x07296b34, 0xe940cc74, 0xa905bd91, 0xf224f0d5, 0xba3dac41, 0x0f29a169, 0x94afb881,
    0x15eb8967, 0x87102236, 0xa77e8e02, 0xff35c608, 0x88340d6b, 0xfafb0e16, 0x9019f567, 0x596765b0,
    0x203cd9ab, 0x2f2711a9, 0x2e691372, 0x73976483, 0xbc2aab99, 0xeb465157, 0xbda6db90, 0xac78f298,
    0x1a92b144, 0xb7bde766, 0xeb8332c0, 0x9fa09678, 0x2e6f8555, 0xc9f47cc0, 0x4be6fbcf, 0xcfaaf09a,
    0x9eede62c, 0x2d270718, 0x89b5b7c7, 0x9fdb4a94, 0xe7666ba9, 0xf01d0443, 0xfe15a027, 0x6bc7c12e,
    0xaf79a052, 0x3224dd63, 0x63fcba4f, 0xde093d44, 0xa09e8ac8, 0x13d32f9f, 0x499e5ab4, 0x5cb60398,
    0xe141cdeb, 0xaeeb8073, 0x590ca633, 0xeb416c14, 0x0a8263af, 0xa57b9d52, 0x84c8d640, 0x74d0882e,
    0x96381762, 0x2ca58e54, 0xc06f8c93, 0xe9094ce6, 0x25fa5da9, 0x6944793e, 0xf73d1c20, 0xb0068f6f,
    0xffcb642d, 0x0439a54f, 0xe1923a1e, 0x0a5ae776, 0x9b6b165f, 0x50c7b6d8, 0xf79c3160, 0x94b6c633,
    0x00ad3970, 0x620a70af, 0x415b0491, 0xd256f72a, 0x330b2885, 0xc7de8512, 0x8eb61bdc, 0x6adee97a,
    0xcc7a94b7, 0xdf80ae0d, 0x7a322531, 0xb29e16f1, 0x8de336c3, 0x72468f45, 0xb8fbd921, 0x09c32283,
    0x73da63e6, 0x2129b39b, 0xfe2e2ba7, 0x43d4b963, 0x214d6072, 0xb76bc507, 0x0bc53705, 0x4052f955,
    0x7d07c9e8, 0x4ec40964, 0x6b32f56a, 0x5b2625d9, 0x422d58d3, 0xb08371c7, 0x969b5977, 0x7d566d8b,
    0xd6beb0b8, 0x40f3c646, 0xbba9bd53, 0x49cc9f6c, 0x84c8f64d, 0xff52240a, 0xd29f261b, 0xf516b88a,
    0xd386cb42, 0xc86b24a3, 0xc0f1cd1f, 0xcf08f81f, 0x3b58d793, 0x51ec5b85, 0xbdcb3000, 0x3fcf0838,
    0x52965afc, 0x61b5c5e0, 0xe0017cc5, 0x985a4d23, 0x4cd5a33d, 0xd19aec78, 0x1af9e555, 0xa9829879,
    0x22034c7a, 0xb37cb0c7, 0xd88f9fad, 0x33dfe162, 0x4ef13206, 0x89959ce3, 0x22403006, 0xb9375e14,
    0x3905d8c3, 0xc1e55d45, 0xdfcfad56, 0x6de847ef, 0xbd8c1ecd, 0x6adb274b, 0x3735b020, 0x02d1acf7,
    0x2d7534f3, 0xc08b7e97, 0xe0c4d65c, 0xc17fe440, 0xddbc0c35, 0xe9e3fc4d, 0x5f380dfe, 0x4244b7fa,
    0x2179e04d, 0xa165e59b, 0xe422ab24, 0xa412e353, 0xfa0ff61b, 0xeee64ba5, 0xb816f194, 0xc8154931,
    0x826ca080, 0x119b599b, 0x0c093a60, 0xa35a8306, 0xc4b5becf, 0x9228b775, 0x03b9f100, 0x3c467089,
    0x6bfa0726, 0x69a5f363, 0x1febb411, 0x0ecfd247, 0x13a92fc1, 0xd8d3adc2, 0xd8f5026d, 0x4756b020,
    0x33c2263d, 0x61c39451, 0xf6ffbe5b, 0x1bd758c9, 0xf5088fdc, 0x646eafa8, 0xa8aecae8, 0xcb322685,
    0x36004141, 0x4f3c2f48, 0x9e171f8a, 0x21ae4f5a, 0x9935e61c, 0x28804b29, 0x27b054f2, 0x1b7b2506,
    0x3a2efca4, 0xb4c7f9e8, 0xf8e0bfc1, 0x87e0f8f2, 0x3678a9d6, 0xddfbee74, 0x629dc922, 0x08777fd8,
    0x5b61e544, 0x409f7ab2, 0x01393a94, 0x38ec8378, 0x73d20b44, 0xbf30e58f, 0x22dcabe6, 0xd2b22506,
    0xa4e84501, 0x06e9d737, 0x6189a748, 0xd6c49d44, 0x4d7f16fb, 0xb62153a1, 0x112d036e, 0x66ea4af7,
    0x1b5bb01c, 0xbac68778, 0x70f46bf8, 0x1a90c99f, 0x33373b71, 0xfb1f67e4, 0x5adf7d02, 0xf3689a90,
    0xd7d16a9b, 0xcd03f2c1, 0xabd3d087, 0x41a3630f, 0x8ca8a5bb, 0x878445eb, 0xe0327608, 0x6b460529,
    0x48aee077, 0xfd58c02c, 0x4711f93a, 0x81966c44, 0x862aadfd, 0xd0b38378, 0x7db1d0de, 0x2fcee984,
    0x589f33e4, 0x0731a473, 0xff9cd5da, 0xa35cd20e, 0x9a18f4eb, 0x4ac7c98d, 0x20713376, 0xa4733c29,
    0x3e7a5190, 0xb2aa91d6, 0xc5f2d2f3, 0x4183e00b, 0x306cfa07, 0x35e57cd3, 0xbca0247c, 0x04f044d3,
    0x0a0881ee, 0xea9b7875, 0x2f6af517, 0xa13b3ba5, 0xed16bd22, 0x59c20dd9, 0x19ac24ff, 0x53745c63,
    0x68a010d0, 0x71f9dcef, 0x08546cca, 0x6cad222c, 0x826ca080, 0x119b599b, 0x0c093a60, 0xa35a8306,
    0x0ecba6c7, 0xb8a48a4c, 0xf5bb386e, 0x4b7572b8, 0x6bfa0726, 0x69a5f363, 0x1febb411, 0x0ecfd247,
    0x34bee115, 0x87a92a6a, 0xbba3c494, 0x748e304a, 0x307061f1, 0x6b8f221f, 0x8c1b8bf5, 0x583c4477,
    0xfd9384c6, 0xcba2249e, 0x3a351a8b, 0x17937acf, 0x36004141, 0x4f3c2f48, 0x9e171f8a, 0x21ae4f5a,
    0xe63c878c, 0xa04c3e33, 0x7956e27a, 0x87c46c78, 0x2da4d8aa, 0x2cbb699f, 0x7eaf39c3, 0xb461547e,
    0xb66ecf0b, 0xf4b11bed, 0x5b61ec58, 0x43ad9985, 0xa55c4e04, 0x0825ad8a, 0xa6d35599, 0x86b0bc3f,
    0x58b5de00, 0x8371fcc6, 0xed29efc5, 0xfbb73ef8, 0xd2aba15e, 0x2b888546, 0xab9a864f, 0xc309d7c0,
    0x968acdd8, 0x4bc56d99, 0x2f84a6c3, 0x69cacfda, 0x4871ab35, 0xf8d0ba95, 0x00921e42, 0x7f7414af,
    0x07cf6568, 0xd8026531, 0xf18daa35, 0x78a43ea6, 0x2a51a576, 0x7beeacf4, 0x84d55be1, 0x833f4c5b,
    0xd7258f39, 0x62e6651f, 0xc4978379, 0x3a52bce5, 0x90497bc3, 0xa17e01ba, 0x35f2d685, 0x07d78428,
    0x7d055ce3, 0x36447fc5, 0x6213205f, 0xf8915b22, 0xbb741504, 0xf479e926, 0xb7059659, 0x1a40ea26,
    0x1b06b5d0, 0x6b016c49, 0x505c4a7d, 0x32f97810, 0xb0c18f03, 0x09c6ee32, 0xe6b02565, 0x20055e80,
    0x2b5504e8, 0xe1c78d65, 0xf7735666, 0xb7e24625, 0xb32bfd03, 0x2e18ffc4, 0xa639f49e, 0xc9864861,
    0x1ceb1e9d, 0xa7c81a29, 0x24d120b3, 0xbaf043c8, 0x563041fb, 0x9bcebe51, 0x407be402, 0xb4db9b2f,
    0xade87023, 0x59afc97b, 0xf36a3fe0, 0x77ecfe55, 0xb7908b96, 0x6534f8dd, 0x5d7b9f71, 0xfddff13a,
    0xca5ab1f5, 0xac20959d, 0x608531c4, 0x43999644, 0x1b744263, 0xe23382fd, 0x50c890fe, 0x40dc44ec,
    0x8127715f, 0x4eab9d4c, 0xee1778f2, 0x33f09464, 0x61aac0ea, 0xec88dbd7, 0xc1801f55, 0x54174039,
    0x43177dbf, 0x83332d7d, 0x7482e2aa, 0x2117b7fd, 0x27fc9855, 0x7f3d13a4, 0x705f3d25, 0x07819298,
    0xad9895b8, 0x5aa58910, 0xe78790a7, 0x69566b57, 0xf29175b8, 0xb9d5b766, 0x5c6f0589, 0x96b8b043,
    0x751de1f2, 0xb5da9650, 0xbfe94309, 0x000fefe6, 0x2be7192c, 0x2b085bdc, 0x034d5818, 0x111e23c9,
    0x820e368a, 0xe0eef261, 0xd1eadfd6, 0x1f1c62b6, 0x756ca227, 0x04949f10, 0x2695d1eb, 0xf936332f,
    0x78a1bb42, 0x78969f3f, 0x1444ca46, 0x63cd93b1, 0x5ae1e70c, 0xd6aabeed, 0xcf9e505e, 0x9599b6d3,
    0x42bb6da3, 0xe6e285b5, 0xa7dee9be, 0x5d626902, 0x3e1c7986, 0x73ce7ce6, 0x0119ea4d, 0x88235b78,
    0xe211f4b0, 0xea7c5867, 0xbb65dd46, 0xdef6025b, 0x7d055ce3, 0x36447fc5, 0x6213205f, 0xf8915b22,
    0x277b51fc, 0x66506c94, 0xc3e61e27, 0x0ee286c6, 0xd3e0d4a6, 0xf87ecaf1, 0x1e990d59, 0x4c529a04,
    0x7767ed13, 0x390eeeb6, 0xcfc058bd, 0xa03c368d, 0xb5acbc3d, 0xb7574d49, 0x44e67007, 0xef87258d,
    0xc0ff5d23, 0xc07b4a44, 0x153becf7, 0xa0b00881, 0x65ee387b, 0x5f425ead, 0x9aab4abf, 0x07a71a6a,
    0xbe9233f9, 0x7af53b1c, 0x6b9e7746, 0x5e6fb164, 0xfd10ddd7, 0x46417086, 0xe29c7123, 0x6dfa4a4a,
    0xd60f29ed, 0x4d7fcce5, 0x8d8e4328, 0x9e724e46, 0x9828c6a7, 0xe16c3c33, 0xf29528ad, 0x1efcb5ba,
    0x20b4e1b5, 0x8a8d1837, 0xfb8ef2a4, 0xb0d0b94c, 0x86495623, 0x639ddc6b, 0x72c05670, 0x24abc426,
    0x2bcd8716, 0xfee68e67, 0x3b3d0c4a, 0x7e631c2b, 0xcae62ecb, 0x39613758, 0xc6166e51, 0x6cf1a7b8,
    0xccfe14c1, 0xebc29540, 0xf2126605, 0xb65b4c77, 0x83e800e5, 0x0c5a655d, 0x3e4094d4, 0x0b640c33,
    0xdedbcbd0, 0x7242c456, 0x791d5c3d, 0x485eb2e1, 0x6977cc15, 0x7f93530d, 0x30e52520, 0x8e4e43f4,
    0x1393e824, 0x0a28bddc, 0x1b071aa7, 0xc1538fed, 0x1393e824, 0x0a28bddc, 0x1b071aa7, 0xc1538fed,
    0x1393e824, 0x0a28bddc, 0x1b071aa7, 0xc1538fed, 0x1393e824, 0x0a28bddc, 0x1b071aa7, 0xc1538fed,
    0x1393e824, 0x0a28bddc, 0x1b071aa7, 0xc1538fed, 0x1393e824, 0x0a28bddc, 0x1b071aa7, 0xc1538fed,
    0x1393e824, 0x0a28bddc, 0x1b071aa7, 0xc1538fed, 0x1393e824, 0x0a28bddc, 0x1b071aa7, 0xc1538fed,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x9949a866, 0xac0d9214, 0xe122337d, 0x349f0f96, 0x1190493e, 0xdb5595e2, 0x1d7efdb7, 0xa23d43cd,
    0x154ba512, 0x988b488a, 0x34f2bc66, 0xdde527c6, 0x04c1c84d, 0x559bc637, 0xebe0a251, 0x86aa0955,
    0x881b43d9, 0xa40ad870, 0x0d8429ec, 0xeecdb65d, 0xfaddfcf3, 0xec5ddcc8, 0x4a8333f9, 0x1aa0f6cc,
    0xba4fdff9, 0xb5e559cd, 0x8f69512f, 0x2f8bc650, 0x525a4306, 0x7e3c411f, 0x8f4c433c, 0x28d27e19,
    0xfc48f2e6, 0x4f4cfc5b, 0xe4bbe2fd, 0x2318c6e5, 0x95b7f11d, 0x183e6263, 0x3bfd393f, 0x5c2484a0,
    0xc4b1e4da, 0xfe6bcf80, 0x23b4ff47, 0xcaf51492, 0xed0b4850, 0x68ca6493, 0x0eba3a0d, 0x60e4894d,
    0x47669738, 0x5adcdcd4, 0x9eade481, 0x8abe7a4d, 0x5d5684c1, 0x3d0381f5, 0xb5ac8846, 0xf0d5526b,
    0xed70f4a1, 0x850b0a0c, 0x1cfb89bb, 0x8882a982, 0xe0161904, 0xd0345e1a, 0x02653e24, 0x57298c21,
    0x3a9db6a3, 0x8f870f9f, 0x4ca4b7e5, 0xb650137b, 0xd720a48f, 0xa2b4c21f, 0x48d28ffa, 0x002083a2,
    0x4b5fb002, 0x761c52bb, 0x762b17d1, 0xd3f07b0e, 0x8dc91da0, 0x731fef69, 0xfb31d740, 0xf488eadd,
    0x5f18cbee, 0xec62bf31, 0x9f09cc33, 0xffea666c, 0x0a02e60a, 0xd51386f0, 0xf8f4bb01, 0xe65b304b,
    0x8502d929, 0x67e6dcb0, 0x28e66ab1, 0xf32290ec, 0x5c2cc31d, 0x069f22f8, 0x0f31461c, 0x29bd9cb0,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
  ],
  "signature": [
    0x9949a866,0xac0d9214,0xe122337d,0x349f0f96,0x1190493e,0xdb5595e2,0x1d7efdb7,0xa23d43cd,0x154ba512,0x988b488a,0x34f2bc66,0xdde527c6,0x04c1c84d,0x559bc637,0xebe0a251,0x86aa0955,0x881b43d9,0xa40ad870,0x0d8429ec,0xeecdb65d,0xfaddfcf3,0xec5ddcc8,0x4a8333f9,0x1aa0f6cc,0xba4fdff9,0xb5e559cd,0x8f69512f,0x2f8bc650,0x525a4306,0x7e3c411f,0x8f4c433c,0x28d27e19,0xfc48f2e6,0x4f4cfc5b,0xe4bbe2fd,0x2318c6e5,0x95b7f11d,0x183e6263,0x3bfd393f,0x5c2484a0,0xc4b1e4da,0xfe6bcf80,0x23b4ff47,0xcaf51492,0xed0b4850,0x68ca6493,0x0eba3a0d,0x60e4894d,0x47669738,0x5adcdcd4,0x9eade481,0x8abe7a4d,0x5d5684c1,0x3d0381f5,0xb5ac8846,0xf0d5526b,0xed70f4a1,0x850b0a0c,0x1cfb89bb,0x8882a982,0xe0161904,0xd0345e1a,0x02653e24,0x57298c21,0x3a9db6a3,0x8f870f9f,0x4ca4b7e5,0xb650137b,0xd720a48f,0xa2b4c21f,0x48d28ffa,0x002083a2,0x4b5fb002,0x761c52bb,0x762b17d1,0xd3f07b0e,0x8dc91da0,0x731fef69,0xfb31d740,0xf488eadd,0x5f18cbee,0xec62bf31,0x9f09cc33,0xffea666c,0x0a02e60a,0xd51386f0,0xf8f4bb01,0xe65b304b,0x8502d929,0x67e6dcb0,0x28e66ab1,0xf32290ec,0x5c2cc31d,0x069f22f8,0x0f31461c,0x29bd9cb0,
  ],
  "signature_location": [
    66080,
  ],
  "signature_index": [
    0,
  ],
}

if __name__ == "__main__":
    main()

