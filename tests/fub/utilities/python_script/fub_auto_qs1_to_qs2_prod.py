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
    0xbbd79cfa, 0x56e20b67, 0x1fbdf859, 0x1f11ed41, 0x0337fc3a, 0x71a24b12, 0xe256b762, 0x1cbfd08e,
    0xf0aede0d, 0xdde2a1d5, 0x07cad9e3, 0xd0b6ba34, 0x02494c0e, 0xf5b5493a, 0x5a625f63, 0x7fb1d50b,
    0xe8db443a, 0x5b1282ea, 0x5ccd0807, 0xd6c5881d, 0x5eded6e1, 0x29bdad3c, 0x1ab6963b, 0x3245387c,
    0x7ca7df72, 0x89423fab, 0x1d5f81df, 0x980f831f, 0x3a75c5d6, 0x435bf6fd, 0x6c157c34, 0x00a16796,
    0xc105e23d, 0x929bb00e, 0x458baa7f, 0xd105d7da, 0x2580d07b, 0xb68b2914, 0xc6ca5a57, 0xe16bdda1,
    0xaeca12ec, 0x7c6da941, 0xc366acbb, 0x3a9468d1, 0x005d1f0b, 0xe77fc35d, 0x2e61165c, 0x0028c6ca,
    0x5fe692fc, 0xba9bec47, 0x3c92f658, 0xeda28865, 0xc311ebdb, 0x7eda8350, 0xa6fe1993, 0x1c368eb9,
    0x6962fb6c, 0xd8ba0fef, 0x58ff51fd, 0x6dba0fdc, 0xa42696d0, 0xd77bb9ea, 0xa10a3769, 0x9097327f,
    0x078b43bf, 0xb50ede17, 0x8885f070, 0x00d70eff, 0x99b251cd, 0x37244fbb, 0xf2fa77af, 0x987f4003,
    0xf8e9c0b0, 0x1520876b, 0x06aef724, 0x5602a32b, 0x802d130c, 0x0f81d66c, 0x8672548c, 0x8881c8f6,
    0x3d48ebbc, 0xaa7b099c, 0x0b4453c0, 0x5d958d29, 0xb411af74, 0x13e1154f, 0x31636a96, 0xd9cfe98b,
    0xd03dd86b, 0xb8cd4462, 0x4b526a33, 0xf01a2edb, 0xfcd95a07, 0x985906e9, 0x1369ac24, 0x47ef4c97,
    0xe3a11b6b, 0x27fa0d26, 0x62ef499c, 0xe7d8d62c, 0x4a2b7be4, 0x29b7530a, 0x7b61beae, 0xa68db537,
    0x19c9732a, 0x5e167c7b, 0x53d19254, 0x1f4724e6, 0xb9163d80, 0x0e92ff34, 0x479dc278, 0x303a0f07,
    0xe8f1020e, 0x4d893fea, 0x635292b8, 0x40f58990, 0x269eef31, 0xb14a71f0, 0x09705fb4, 0xfd1f3dbf,
    0xa3f13211, 0xbe316608, 0xe8f3de80, 0x4445c700, 0xa78275c1, 0xd7485908, 0x007cb6bf, 0x668794d1,
    0xedc09030, 0xc14b46d5, 0x0e8aea06, 0x509279b6, 0xd5f80800, 0x55c312a4, 0x87ecf3fb, 0x62024999,
    0xded56728, 0x6a39cf5e, 0xdaf75297, 0x2351662f, 0xf4876481, 0x56f30224, 0x1d06abe7, 0x1a35eb3c,
    0x055bc401, 0xf55ff9ea, 0x6c8c7092, 0x764f5a90, 0x76df4f90, 0xed1fb2d5, 0x27c1aa3d, 0x266c1639,
    0xdf1935c0, 0x9c130332, 0xc1ab581c, 0xcc13ff16, 0x1b76c708, 0xf683674e, 0xcbd2900b, 0x2d367151,
    0xb6b469bf, 0xafeaa652, 0xecbeb35f, 0x7c95a88a, 0x3d61e996, 0x42276bc2, 0xb861eba1, 0x54e86253,
    0x37e07990, 0x9c3c640b, 0x4bb3b52d, 0x081f5da2, 0x2be4f905, 0x91bf5e0c, 0x747b6c6b, 0x0e36f234,
    0xafe056d0, 0x26700c37, 0x11cad88a, 0x17e05718, 0xfea31c00, 0x8e38b1d5, 0xea70082c, 0xf6568d36,
    0xa1db3c3b, 0xa29ac9fe, 0xaba80152, 0x00e7c9ac, 0xfe80f785, 0x201475bb, 0x6c32738d, 0xea0ffa14,
    0xe6adb980, 0x8c692065, 0x8ae00229, 0xc4f46de5, 0xe6c5d3fb, 0x89f7c0a0, 0x4d54cf5b, 0x3fdc4b3a,
    0x1291219b, 0xd8950e07, 0xb53e1960, 0x00d7f086, 0x8288b8ec, 0x9c68374f, 0x5f4afff9, 0x17651c44,
    0xb779964e, 0x8d8d0d67, 0x0757cf0c, 0x49517dec, 0x1afe2ae3, 0xf6a3e489, 0xb6e8fca1, 0x57f0ab54,
    0xac5c35bf, 0x2f0c3854, 0x9f57b8cd, 0xc5e5406d, 0x385bfb07, 0x8986831f, 0x4151dab1, 0x702f689b,
    0x74bac81d, 0xa4f506ce, 0x69c10c03, 0x42179517, 0xf9912edf, 0xa858f2f6, 0x0160285a, 0xc6843d74,
    0xd2ab12ba, 0xbfb91e7d, 0xc11de4e3, 0x3728c60b, 0xc76f0ca9, 0x45fe790b, 0x666774c2, 0x291a661c,
    0x85727cd8, 0x57ad353e, 0x6b8cc850, 0x676b224f, 0x6d344017, 0x187cc853, 0x3ed8b1a6, 0xa2dd0fb6,
    0x7bca89dc, 0x869fa80c, 0x68203394, 0x6864e56e, 0x63981146, 0x6cc732f9, 0xbb4fd200, 0xc8d1a736,
    0xe8ccdb32, 0x47c81a66, 0x9c867d61, 0x348e483f, 0x24090465, 0xb2f9454a, 0x38470a9d, 0xb3a728be,
    0x1382324f, 0x7e884c8a, 0x26d29ce3, 0x3690db06, 0x49242cc7, 0x2ad2214c, 0x581e6959, 0x39619209,
    0x6cd62a7b, 0xc55292e2, 0x27326839, 0x991aa880, 0xfb79f340, 0xed8ac198, 0xe4da9353, 0x9d8bb4c1,
    0x82f1e801, 0xc8552dec, 0x52f5e955, 0x220d2e74, 0x8e830721, 0x2dc9c18c, 0xd335830e, 0x601975dc,
    0xbd6fc5b9, 0x63251351, 0xe20bd1ec, 0xfbcf2c55, 0xafdaee9d, 0xd2fbea0f, 0xe6fb2659, 0x9f3543ac,
    0x3972140f, 0x6a13490e, 0xc1db8753, 0x78bc7646, 0x1f6f3c6f, 0x59d57620, 0x17c96ac3, 0xd932a1a2,
    0x088d6b6f, 0x62a31f88, 0xdab95389, 0x4c1146e7, 0x7bde010f, 0x98c76b21, 0xce8d7bdc, 0xf6781433,
    0x4493e3b8, 0xa06a87b1, 0xed67c7d0, 0xf4210a82, 0x5c7de89c, 0x3f245862, 0x00d8132e, 0x99e5c55a,
    0x9af19251, 0xc4ba279c, 0x2ffde3d3, 0xcf31d007, 0x46c5b73b, 0x7dc6fd38, 0x636e7ca1, 0x8bf59f5a,
    0x492fc2cd, 0xdeab6a86, 0xfca0ea1e, 0xafadb059, 0x31d646b1, 0xbb449cb8, 0x45404428, 0xc863ef27,
    0x6e58c152, 0xc16752c2, 0x8ec4efab, 0x47352776, 0xb3f0c12b, 0xaf3f7f25, 0x63b4a98a, 0x271b52d7,
    0x99ec1c01, 0x276e69f0, 0x77809f2d, 0x7ad9d7fe, 0x039b4a9b, 0x5f7b1608, 0x38f4ad74, 0xdf26ec03,
    0x703722ad, 0x5e01aca0, 0x47a0e985, 0xa1b2afe6, 0x30b5561e, 0x33b4a3a8, 0x8b729ded, 0xe9cdd010,
    0x65018002, 0x3fd45755, 0x836f2bbe, 0xf9fb85ea, 0x68f43e1d, 0x0ecda59a, 0x6f337cb4, 0xab61f7cc,
    0x0aba2756, 0xef1e0577, 0xdd4ca546, 0x4d0fc728, 0xd6544844, 0xa9f25ab4, 0x801f37de, 0x7511c3e2,
    0x2d8373b1, 0xad9d2499, 0x2879fd78, 0xe07472d4, 0x51cde9bf, 0xa1e9b038, 0xdf4c70cb, 0xa8715171,
    0x5d615bee, 0x318dca79, 0xee496f7f, 0xc06104e2, 0xeac8b58a, 0x5466cc1c, 0x66554a7d, 0x16a68ac3,
    0x97310d06, 0x9e464ffc, 0x5d250f29, 0x29db4094, 0xbe26075b, 0xe4717c2d, 0x7d2389af, 0xccb3bc02,
    0x01ec7dd2, 0x7d79dc26, 0xb873d49f, 0x340eef4b, 0x7ef7aa31, 0x09c420d9, 0x8a551800, 0x04e73a47,
    0x61625750, 0xf0445219, 0x8accb529, 0x1ef48e02, 0xaaac05be, 0x5650a37d, 0x11652905, 0x7d0a0d72,
    0x6d5f587b, 0x91f56fa1, 0x0f907cbf, 0xca2d7a93, 0x9a38d746, 0x14dcc8c8, 0x1a1d5654, 0xb9a99343,
    0x111d89c6, 0x10763b6f, 0x8f106696, 0xc692e0f0, 0x6582b746, 0xcdd2562e, 0xdaf0439f, 0x28fa5c7d,
    0xe23cd3e6, 0x4f4d2607, 0x7978d344, 0x96e83755, 0x42566726, 0xa4c45a60, 0xbc224db1, 0x9fde8f34,
    0xa20dc640, 0x86d1047f, 0xf322143a, 0xab4c0dd8, 0xcbd2c009, 0x2ffa6246, 0x26c16309, 0x8b3a05ae,
    0xfc223d86, 0xc0521491, 0xbe9d0c7e, 0x8a2713f6, 0xf1b9594d, 0x14590461, 0x5bb67fd7, 0x4d0653b2,
    0x0ab27e73, 0x6aa57805, 0xfd83ccb3, 0x7d6b1035, 0x49714c0c, 0x2f4c842d, 0xf5949eb0, 0x3e615e70,
    0x74c52efe, 0xaf07fe06, 0x316447d3, 0xf1c8c367, 0x1a46da6e, 0x195da675, 0xb8258a37, 0x91098f56,
    0x883e7adc, 0x470b4a08, 0xd3e1d53e, 0x6b210123, 0xdc3b2bd5, 0x7e36a7bd, 0x08dd042c, 0x639561c8,
    0xf5a469ee, 0x77966ec9, 0x05aca3b6, 0x8d208294, 0x7434c7b7, 0xae071380, 0x4fa96920, 0xbdbfaa1b,
    0x90be27e2, 0x4a73f0ae, 0x6d6c89c3, 0x42fc0c99, 0xb94d6d44, 0xd6d02293, 0x3354af58, 0xc2f190ec,
    0x935c42ea, 0x8c726648, 0xf34fa0fa, 0x42390a64, 0x74e8e411, 0xfd6ae47c, 0x845820c0, 0xf2ceed59,
    0xda38cf2d, 0x3c3b1a43, 0x61c58e68, 0xd30ac059, 0xb1b993e0, 0x9030a625, 0x8bad7474, 0xa6a2973a,
    0x12461d88, 0x367e59cd, 0x5ff21b1d, 0x34648811, 0x71124949, 0x8ebc0ffb, 0x5cbe2f97, 0x630f3337,
    0x27826eae, 0xf58cbf67, 0xdf00289f, 0x4b88b155, 0xc775d284, 0x6c2b2ae6, 0x643c90a6, 0x212917c9,
    0x6a8fe53b, 0x23f3e303, 0x29290d18, 0xcb9b8b21, 0xb1d212b0, 0x1c9d710a, 0x1ec89f47, 0xc3639f84,
    0xf1a7b9e2, 0x8561947c, 0x9af673b6, 0x9a2004e3, 0xc0046687, 0x1d25ba43, 0x7edf3e9f, 0xa951eade,
    0x9451139e, 0xb6da19fa, 0x77a0217f, 0x841e116b, 0x3e164ef4, 0x48e0781c, 0x91ee4d48, 0x0324721a,
    0xfc2ecf46, 0x67175611, 0x94592095, 0xdd3c09fe, 0xc0ca04a3, 0xa89f91b7, 0x09039b86, 0xd1c91358,
    0x793a350e, 0xffb1f19a, 0xc8a162c1, 0xc5f9e114, 0x1620deca, 0x130bd278, 0x00f6f4d1, 0x54be6b5e,
    0xf57aa4a0, 0xd3c64284, 0xdab77748, 0xf318f40a, 0xe2162d41, 0x2f625563, 0xa2dfe93a, 0x27a7c047,
    0xd0c1c218, 0x3d6d07f3, 0xa279c1c7, 0x2ebbaa0b, 0x005d6569, 0xa392313f, 0x6c22b2f1, 0xeccd1ad3,
    0x8405a83d, 0x03292c8e, 0xf2043253, 0x4858762a, 0xef0f8f48, 0xf79a0854, 0xa914b632, 0xce669165,
    0x321cb9a5, 0x689b7947, 0x1919b94d, 0xce60a3ee, 0x31bfe259, 0x3378a802, 0xd8dd2696, 0xa33b100b,
    0xcd8f5cb2, 0x3266a982, 0x6365ad7c, 0x61f803a2, 0x360155d1, 0x3bf329e0, 0xba76b711, 0x942c342b,
    0xb50d841c, 0xbf8f95f2, 0xd9362aac, 0xabc6df82, 0x044eca3f, 0x4af52006, 0x7b95bf27, 0xa40e1cce,
    0x6d344017, 0x187cc853, 0x3ed8b1a6, 0xa2dd0fb6, 0x32efcbc1, 0x59965d46, 0x4aca65c4, 0x546c8fe7,
    0x02013006, 0x34a8e825, 0xcbdabe42, 0xc2f94ae7, 0x3489a014, 0x852bb519, 0x5a6328b5, 0x5f57fae2,
    0xff55919d, 0xb9548607, 0xa7e7d475, 0xa74837c4, 0x3323f3ce, 0xb2335333, 0x01365f8c, 0x12110ec9,
    0xc1cc9ed0, 0xd6ef29c7, 0x95a4fe61, 0x92093680, 0x59bb3af4, 0x66456c25, 0xd9a9ecb0, 0x930a271a,
    0xb2d6a091, 0x29ead77d, 0xb4942a67, 0x6c581dda, 0x706758d5, 0x2169970f, 0xc016c2f5, 0x8f5a13a7,
    0x99483113, 0x8c3c66f5, 0x70b9b3b0, 0x122ab5f2, 0x93f639b2, 0x21b57b80, 0xe3e5e37d, 0xb1e9a884,
    0x8a0f8685, 0x6f763f36, 0xca383a81, 0x6137ea7a, 0xa2ae9a90, 0x42e28ce7, 0x7b9ad8f5, 0x5cf5a988,
    0x9d6cfd98, 0x19d6412a, 0xe2117da7, 0x6ed23b28, 0x6027ac8e, 0x0cdcd1ff, 0x5b631f52, 0x3376cfba,
    0xb2692da0, 0xf99350ef, 0x52a5b2c8, 0xa11a6b4b, 0xcae7345f, 0x38a783d4, 0xf9e88b80, 0x109e42a8,
    0x2bfbb793, 0x21cd829b, 0x0c42fb2e, 0x55ab6b42, 0xd2bd0160, 0x212f2f07, 0x0da0db1f, 0x31102828,
    0x3650cbd8, 0x0fd17cf7, 0xab2724c5, 0x93d52c79, 0xfe4f4cbb, 0x514b6963, 0xbfe30761, 0xca77df3e,
    0x6904da93, 0x4f17738c, 0x5c323f07, 0x16e6834c, 0x077bca31, 0x3dfd690e, 0x7b51e73e, 0xaffca2e0,
    0x769818e4, 0x62f591b4, 0xe951c0eb, 0x2ec4ada5, 0xc969af4b, 0xbb06e7ab, 0x3fd67b17, 0x1d036c89,
    0xb3cc6c91, 0xa73f44d5, 0x7f26011d, 0xf6d2fb8c, 0x56f71173, 0xd44bb4df, 0xea4807da, 0x7a4d2134,
    0x4aafd913, 0x854b2e42, 0x37a12209, 0xda6fa05f, 0xb16293ef, 0x3c611467, 0x7c2125fd, 0xdd665573,
    0xb22825ef, 0x7b6cbd2b, 0x1e2cdd50, 0x7e92949b, 0x034e042d, 0x7b3b248f, 0xfc166195, 0x5c8150cb,
    0x54c2a506, 0xd377c6bb, 0x72841a9c, 0xcc795d8b, 0x137e5798, 0xa1594563, 0x0a66e453, 0xc3729afb,
    0xf58a5728, 0xa11b30e2, 0x766cc013, 0x30dd0157, 0x8a273724, 0x5ca88e1a, 0x8e3f6adf, 0x3a5fc90b,
    0x892ade17, 0xc00273a6, 0x300bd67c, 0x3a13969d, 0x9b594bad, 0x435aeb63, 0x4d1a8fc6, 0x83cbb81a,
    0xcb99e2c9, 0x9f30c5d6, 0x2dcfc7ac, 0x2c4987a3, 0x9c089634, 0x678bbc85, 0x59fdbf8e, 0x57dd35cc,
    0xcfd28f5c, 0xd9dcacf8, 0x5fd98901, 0x6f410b83, 0x901af34f, 0x8ef09db5, 0xc78dd823, 0x3a33d3f3,
    0xf9a696d1, 0x6ac0e99c, 0x1bc2f740, 0xa67ba8b4, 0xd2f0393d, 0x539708d2, 0x45127124, 0xcb2faf0e,
    0x568f146a, 0x2e2aed2c, 0x33ee0972, 0x390c9e63, 0x46cd6417, 0xba726d2a, 0xbb2b26b6, 0x893faa28,
    0xd97fbbbd, 0xab22a795, 0xfd80752d, 0x4aacbc6b, 0x87c267f8, 0xad5e583b, 0x7c2c661e, 0x64a0d317,
    0x071f0f76, 0x024af587, 0x9b3f9c3b, 0xfc2e0447, 0x2616270b, 0x83de0439, 0x2d9b8fbb, 0xb25a8d2e,
    0x545d243d, 0x808657c9, 0x59c29c27, 0x933fc18b, 0x80ad008e, 0xcd6b973f, 0x8bfd7cdc, 0xdf378481,
    0xa6f1a251, 0x7dfb58a5, 0xd73f7cd9, 0x2369c37e, 0xf172f5bf, 0x2f7f1374, 0x7c610cbe, 0x25970aac,
    0xe7662c99, 0x7b7c183d, 0xa9f61bb6, 0x0d9332d4, 0xdb8e4aa4, 0x45e15036, 0x784c8c65, 0xa9630f4e,
    0xef0f8f48, 0xf79a0854, 0xa914b632, 0xce669165, 0x59af8332, 0xad4a8ea7, 0x74e775f8, 0x30ea4dce,
    0x98606b4a, 0x17e2de59, 0x82354167, 0x49da623d, 0x80d51889, 0xa6ee6706, 0x07c1b14d, 0xae338f73,
    0xb6a85861, 0xaac66c34, 0xd44c3009, 0xd4aa7115, 0x5370aa91, 0x236e4ff2, 0xdd82bd56, 0x784e9849,
    0x044eca3f, 0x4af52006, 0x7b95bf27, 0xa40e1cce, 0x6d344017, 0x187cc853, 0x3ed8b1a6, 0xa2dd0fb6,
    0xbb9465af, 0x09c75e8b, 0x25703dc4, 0xd448f8d0, 0x02013006, 0x34a8e825, 0xcbdabe42, 0xc2f94ae7,
    0x780fb1d1, 0x4447de75, 0x64d344f2, 0x7733a09d, 0xa95ee2fc, 0x164c20ff, 0xed8daa50, 0x9a05eb2b,
    0x1e583980, 0xbeffb5f4, 0x0585d945, 0xdacaa0eb, 0xec6839a3, 0x46734025, 0x2f31901f, 0x063f3238,
    0x645fe662, 0x105895ae, 0x666d4ca9, 0x62dea4c1, 0x249f33cb, 0x0b48083b, 0x4217f9e0, 0xc51e93c0,
    0x8e174ddb, 0x2e4696b6, 0x953999aa, 0x660fd989, 0x1b96cb67, 0xe7adf8a6, 0x2e07a251, 0x0a49f367,
    0xd0957454, 0x93609a74, 0x0985e617, 0x09cc202d, 0xa9b052cb, 0x3caa17a4, 0x916e20ff, 0xf15d4f66,
    0xe5c8f300, 0xf229c847, 0x354481f1, 0xd5b5a5c0, 0xa48dc32b, 0xfd43a5f6, 0x52ca2812, 0xb9167c55,
    0xc34d2820, 0xdfd2c9fd, 0x67a57469, 0xb6865aab, 0xb5e702d2, 0xc5f0238f, 0xf69cee2c, 0x5421c3d1,
    0xfa9a997a, 0x816409ce, 0x56eb9de6, 0x639b6020, 0xd2bbb422, 0xe4ecca92, 0xc8e98b9f, 0x49b22c73,
    0x7d75cce0, 0xee7879df, 0xbfffa0f6, 0xe5fa8046, 0xcf9598be, 0x37ba13e3, 0x724c3f26, 0x6b9da667,
    0xc69f6467, 0x58d4d714, 0x937e95a1, 0x6ad41923, 0x3787fd7b, 0xb9f6523e, 0xc5ed7109, 0xe5da163a,
    0x6971b41a, 0x3b5891fc, 0x7629b630, 0x9e1d5a4a, 0x1321f168, 0xcc9f9c5a, 0xb2eea104, 0x1ba8db9d,
    0xc531f316, 0x2f5d64e0, 0xc49796f4, 0x14d7a4b8, 0x2a89803c, 0xdc221353, 0x589611f1, 0x31a1b36f,
    0x92715af0, 0xce8c0621, 0x53fa7019, 0x630e136b, 0xb4a2f546, 0x6aef95a7, 0x36bbe947, 0x16bdc21a,
    0x6bf51c36, 0x5e1d5dbf, 0xc1d2f978, 0x2520dcbc, 0xa382a2d0, 0x92aaa720, 0xa7ceeb1d, 0x9da8dcab,
    0xbd0eade1, 0x3fb21fc6, 0x02fb676c, 0xbc3b8644, 0x7c169dd5, 0x02f03032, 0x8a64f7bd, 0xe5465784,
    0xf18d8dee, 0xafd95053, 0x82123cd1, 0xbde8b416, 0xe719197a, 0x9b28f434, 0xecbd90e1, 0xdfc7b075,
    0x9c4e3570, 0x548d4f9a, 0x77203516, 0x2754cfc4, 0x7cd8c9c9, 0xb830c2ef, 0xbe9ce3f7, 0xd57dbcd3,
    0x3431cc28, 0xcbc92d73, 0x14e6fd05, 0x49383115, 0xe4baad23, 0x010034a9, 0x9bb9c79c, 0x42f81cfa,
    0xbd821f45, 0x48a19bd4, 0xb4f0ea99, 0x4d4d868b, 0x2249a4ff, 0x948b83ee, 0x2d2c5805, 0xe9d3ecd2,
    0x06748a9b, 0xd42e4129, 0x6967baab, 0xe653035b, 0x66b20c5a, 0x1ed32bb1, 0x958eae7e, 0xfda1c5e1,
    0xad40a5d7, 0xf031009c, 0x50b8e4b0, 0x22db33f8, 0xa5ace5c3, 0xa92b2338, 0x0a181937, 0xd8fe5d13,
    0x73ccfae0, 0x889490c6, 0xec04ca22, 0xe31627b1, 0x44ec91d8, 0x40f72a60, 0x5c924e98, 0x6e6371f1,
    0xd5a5a34c, 0x3c097f48, 0xd6019735, 0x9a1f4186, 0xe6489353, 0x8eb3cb51, 0xf69ac5ab, 0x0772de40,
    0xde39c285, 0x19427665, 0xdebfdbb6, 0xfb48b003, 0x6263739b, 0x6783d383, 0xa16e9db0, 0xd53bb012,
    0x8aa28427, 0x93462f8a, 0xd0978354, 0x37641c58, 0xdcd9a5bb, 0x4b85f2d2, 0x870e08bd, 0x31f129a6,
    0xae9265bc, 0x470836fa, 0xcd14ce65, 0xd676941d, 0x29445402, 0xaace9564, 0x780c104a, 0x36fdd66f,
    0xb5eb6dad, 0x7e130545, 0xf36a3bea, 0x533042bb, 0xb39c435e, 0x1c33cd39, 0x4a981bf7, 0xe7fc28d0,
    0xc66bab12, 0x0a89c97a, 0xbf506801, 0x63aa5445, 0x034e042d, 0x7b3b248f, 0xfc166195, 0x5c8150cb,
    0xe9db0e93, 0x6228655e, 0x87653631, 0x40775aee, 0xd3aa5997, 0x12227829, 0x1af50095, 0xe028be08,
    0x0e65df1c, 0x0e448257, 0x1a894174, 0xe0343fd9, 0xe36efac2, 0xdc556b88, 0x0860475b, 0x9f9ab643,
    0xfc0df6b4, 0xcb25c091, 0x3ca85c16, 0x8134dd54, 0x9b594bad, 0x435aeb63, 0x4d1a8fc6, 0x83cbb81a,
    0xcb99e2c9, 0x9f30c5d6, 0x2dcfc7ac, 0x2c4987a3, 0x3dd5e02e, 0xd616fdeb, 0x3dd780c9, 0x003bcc35,
    0x7dc76d2e, 0x681b33e5, 0x8fc73906, 0x69ac7515, 0x33bd571d, 0xdab17f76, 0xed3765ac, 0x59507663,
    0xec6839a3, 0x46734025, 0x2f31901f, 0x063f3238, 0x0e41a5ee, 0x48df0253, 0x5a236907, 0x823ddce5,
    0x74f888a5, 0xbdad72eb, 0xa0db38b0, 0xc5f43195, 0x672b08b1, 0xbb5dff24, 0x1507a0ec, 0x1aceb11e,
    0xbc1be533, 0x03e0860e, 0x439be94f, 0xbe1a3b64, 0x207d0b50, 0x4a8db8c7, 0xff31f2b1, 0xb59f6b9f,
    0x8e48d464, 0x4aed562a, 0xc73ff813, 0xc7f76d79, 0x0bf79f8f, 0xe99e22f6, 0x389ad475, 0xd63a7845,
    0xe2f9b6b4, 0x5cfe8e7b, 0xc1de17d5, 0xe87cf504, 0x3ea0936e, 0x02fd2802, 0xa5756dab, 0x728a853b,
    0xbd9e3340, 0x96f009b7, 0x12dc279f, 0xaac13256, 0x8f946495, 0x99be21f9, 0xe9edd2dc, 0xb1a43a32,
    0x00631a2e, 0x5950c64b, 0xbe03053e, 0xee84b4ad, 0xa3804c6b, 0xa996928a, 0xdc92bf04, 0xffb53df1,
    0x0d413f4e, 0xd2253cef, 0x999ba1fc, 0xb3ba1f5d, 0xcc3ed428, 0x8bbe3a8d, 0xb2d2b76e, 0x76ec8bd3,
    0x77db3b34, 0x84ba7806, 0x4f18e17d, 0xef521f3f, 0x4f6c6505, 0xae28cd16, 0x19e8e04b, 0x598581d7,
    0x3536e08a, 0x5989223b, 0xe19cde01, 0x7538f512, 0xa28c0fb8, 0x80756ef4, 0x0f6ce0ab, 0xb6ec560d,
    0x4f22a906, 0x11ef396c, 0xb8dda098, 0xf90efa38, 0x47c9da40, 0x6f3f9f19, 0x40b38f8b, 0xa8d9e2d9,
    0x1876f9e0, 0x7d9aafea, 0x08defb18, 0x0ddda72e, 0x9968280b, 0xc8eb2100, 0x1d3486fd, 0xd0f5cfe0,
    0xd3a6486e, 0xbfee5aab, 0x9dcb18e2, 0x7ebca34b, 0x227f8bbe, 0x4a2ffaad, 0x7e857e9b, 0xf3683d34,
    0x291118f0, 0x5f802994, 0xaffd6e75, 0x35164482, 0x40fc8826, 0x7c1ba298, 0x89603776, 0x0913abbc,
    0xff016a5b, 0xa54047aa, 0xe1c0e8e7, 0xdb09f66e, 0x69dd57c3, 0xf9b0aaa2, 0x38b3908a, 0x0b166d46,
    0xdaf466a4, 0x0e7eb499, 0x5d7d2f69, 0x2fcf8c38, 0xdcd575bb, 0x3168668f, 0x8d401ed8, 0xa913536b,
    0xa7692ccf, 0xeb6be3d3, 0x5f5809ae, 0x03db61fd, 0xd4e46c9e, 0xbd299c0f, 0x3cdfe0b3, 0x44d1d93d,
    0xd42fd475, 0x0b3520bf, 0x0fc13008, 0x36bde307, 0x73b2988f, 0xdd857e7a, 0x604e34f0, 0x41da9c48,
    0x99b78255, 0xf7272a23, 0x1f1b299f, 0x766fc407, 0x386a44a0, 0x2cb5fd7b, 0x34fe69d7, 0x463f6f3d,
    0x5b2a57bb, 0xb1e18437, 0xddd79847, 0xb89280df, 0x5ba559e0, 0xb43f80e8, 0x798b052f, 0x7d94d42e,
    0xf671f9a0, 0x18e86d1a, 0x626a17f6, 0x18ccb8e3, 0xdad2a7fe, 0xa21cf406, 0xa9cce75c, 0xb707b3d8,
    0x7235e569, 0x9ad71b26, 0x35c65287, 0x4b936f23, 0xddde654d, 0x98b71f2e, 0x36e0ad71, 0x80620f2a,
    0x9ff03b7c, 0xe685b944, 0xac3a7591, 0xc8665092, 0xaecef296, 0xa5c3e8ac, 0x8a378e54, 0xc216571a,
    0xe6e3dbea, 0xdb357974, 0x31ad379d, 0x18a0e9a0, 0xdc9af05c, 0xf2a7161d, 0xd315c37f, 0xf863632b,
    0x4c389a1a, 0x7c224121, 0x445753b3, 0xb0b06795, 0x1620deca, 0x130bd278, 0x00f6f4d1, 0x54be6b5e,
    0x9c0c2959, 0xc14ca862, 0x150ca539, 0x8fcc07cb, 0xe392f778, 0x412f68eb, 0xe729d20e, 0xae713b5c,
    0xb4a2f546, 0x6aef95a7, 0x36bbe947, 0x16bdc21a, 0xa4252d99, 0xe21fda4e, 0xc836f578, 0x0cf5582d,
    0xfa45f2ff, 0x9d97aac6, 0x6a441206, 0xc1331c0f, 0x576dab78, 0x6a97dd26, 0xe70d6978, 0xc8ab7e11,
    0x247791d8, 0xd3a85ffb, 0xca079e02, 0xf6038a6c, 0x96031e66, 0x70d70233, 0xf1015b19, 0x486416df,
    0xdb4a6276, 0x74445a9f, 0xaa257c10, 0x2d9fec35, 0x4c8ca33f, 0xeeba64a4, 0xefed1018, 0xab93b080,
    0x351db086, 0x44e2c82f, 0xea6c9c18, 0xb82aa113, 0xf7bdf166, 0x2f37684c, 0x4141b9d7, 0x6b8f5e36,
    0xe448ddaa, 0xfdd96307, 0x10ae58b6, 0x31586bdc, 0xee8fbfd9, 0x4ce3a6da, 0x3f11d396, 0xeadef9a1,
    0x389d32a4, 0x74817f2a, 0xdd121e15, 0xaf570eb5, 0x33320819, 0x06b94b58, 0xd7100079, 0x8cd7b189,
    0xd0c1c218, 0x3d6d07f3, 0xa279c1c7, 0x2ebbaa0b, 0x0eceeacc, 0x76315593, 0xb40ad090, 0x66be0826,
    0xee2e53ae, 0xf8038737, 0x2b2d02de, 0x463cb128, 0x7ea16345, 0xce4a60f8, 0x800eddfb, 0xb9e0b3b0,
    0x368da95e, 0x7b099b7e, 0xd00ca9e7, 0x6f3f3aef, 0xbb47d1af, 0x9514aa7e, 0x2f68de06, 0x1259efb6,
    0xaecef296, 0xa5c3e8ac, 0x8a378e54, 0xc216571a, 0xa2ce66ce, 0xa05990ff, 0xe32c98e5, 0x22255699,
    0x592e2989, 0xcab9d252, 0x50dcae4a, 0x19ed2509, 0x1a71a4b0, 0xbd912016, 0x422f1140, 0x09ab3077,
    0x1620deca, 0x130bd278, 0x00f6f4d1, 0x54be6b5e, 0x32ccafa0, 0xb9fe1e05, 0xa6f86975, 0xd4eaf10f,
    0xd0ffbb1e, 0x36db6b17, 0xa8a25806, 0x8402ddfb, 0xd0c1c218, 0x3d6d07f3, 0xa279c1c7, 0x2ebbaa0b,
    0xce215141, 0x17d15d74, 0xcdfa7e20, 0xff4b20e4, 0x67b49e82, 0xafc01376, 0x9e31b2b4, 0xbb778342,
    0x9ba443d5, 0xd52781e7, 0x64c558d9, 0xcab44eab, 0x6ac96c23, 0xb519e11a, 0xa8519b46, 0xa21b086e,
    0x9c3571b6, 0x8387cc3f, 0x32a86fac, 0x71a4a26e, 0xbaa81608, 0x5689bc89, 0xc4bfb986, 0x5e1d9769,
    0xb3a834d2, 0x440df335, 0x4f1180de, 0x260090b1, 0xa780cac4, 0xf21fd732, 0x14c61ede, 0xa214b137,
    0x3d5b560d, 0x09fab7a9, 0x2c33090f, 0xb905845f, 0x0711af4a, 0x99f9eeeb, 0xc5c434fa, 0x163278e5,
    0xe645ce7a, 0x27b451cc, 0x2141ec4c, 0x23452f7a, 0xf3331f13, 0x363875e3, 0x91f9a441, 0x1e92fa1f,
    0xa035ea01, 0x450a6f71, 0x20775da9, 0x9f47778f, 0x4fabe1b3, 0x8683081c, 0x0fab9d57, 0x55304c76,
    0x5451ee0f, 0x1f85116f, 0x103821d2, 0x604b8b33, 0x6581c050, 0x67d388a7, 0x5e272a5e, 0xf54ab887,
    0xe9ed0755, 0x7769a3ed, 0xc30eac28, 0x23a914d6, 0xb959f7e5, 0x3af36062, 0x3b50df96, 0x2d79101d,
    0xc7017ce2, 0x7b0aaf83, 0x2e111178, 0xaa7e90ad, 0x9abd00e1, 0xcd803a75, 0x7c4c65d9, 0xac7c3180,
    0x1625bdd8, 0x76432ce0, 0x0d5e1cc6, 0xee6a9b09, 0x07bbbc9e, 0xcb62693d, 0xe685943a, 0x92b4b2dc,
    0x9787b9c7, 0x49d5adfb, 0xf27b0943, 0xeaac2ec8, 0x044eca3f, 0x4af52006, 0x7b95bf27, 0xa40e1cce,
    0x6d344017, 0x187cc853, 0x3ed8b1a6, 0xa2dd0fb6, 0x465b7a53, 0x54102e16, 0x19c6dcdc, 0xe23279fc,
    0x4c7c367e, 0x19e88b68, 0x636fd4e1, 0x1d3789d6, 0xd0c1c218, 0x3d6d07f3, 0xa279c1c7, 0x2ebbaa0b,
    0x9ed9a025, 0xf04c5255, 0xb57a9607, 0x093b05be, 0x557b5aa5, 0x4d612416, 0xf6dedcf1, 0x0f3c0896,
    0x7e4324ed, 0x356fd83c, 0x82fc9868, 0x6fa15bbe, 0x5824839f, 0xf0a8fd4c, 0xe9810c6a, 0xf4202daa,
    0xf16a4037, 0x1a2c2298, 0x9abe6ffe, 0x02d568ea, 0xbc5b2907, 0x35cda11e, 0xf24637d5, 0x6a3d3fc8,
    0x191df01a, 0xba1cc765, 0x7f17f412, 0xf0f0106f, 0x9f463b1b, 0x83c1a380, 0x0808c169, 0x39a1924f,
    0x0f33d12c, 0xa0bf6711, 0x33e46c72, 0x8d48e712, 0xc5c6785e, 0x5cc3c935, 0x63f7695f, 0x10bffa5e,
    0x851474c4, 0x25778f8c, 0x30c1f10a, 0x6f3b9f58, 0x75c0d223, 0x2cb0fad3, 0x19867e88, 0x2501f48c,
    0xa8b12157, 0xde1f9fda, 0xce9c905c, 0xef47808a, 0x971c0fa4, 0x150529d3, 0xe046ff89, 0x3cbbe90c,
    0xd14d02d1, 0x944e38d5, 0x88a48a94, 0x2f6de591, 0xf98872e9, 0x307e5f4e, 0x12b35b76, 0xc56b3f3f,
    0x277cf61f, 0x0b2142f0, 0xe9ff9f6f, 0x333ea9e2, 0xaa8a30d6, 0x5cb4e98e, 0x29ad65ab, 0x41296952,
    0x4e5ce240, 0x2fd464e1, 0xccd6fdf4, 0xd4cba953, 0xdc9f85c4, 0x25e86a76, 0x28bd251c, 0x416b9dd0,
    0x42dd408b, 0x5099666d, 0x778b00b9, 0xce5cbd78, 0xcce61b03, 0x6b5af8d1, 0xa302c1c2, 0xf6ce4c01,
    0x189bea7b, 0x70ecaef2, 0xff2cd9fe, 0x6c7bedf5, 0x616a8178, 0x184e0792, 0xb24b9b44, 0xc745a46e,
    0x6ee38e31, 0x5b962bd0, 0xc6ebe7bb, 0xaca2fb62, 0x08b7df25, 0x072b824a, 0xd5bbef00, 0x63fe4316,
    0x8f058a69, 0xce29abaf, 0x6d02fdca, 0x6c78bd53, 0xa71e25f6, 0xeb246914, 0xc6f67180, 0x27b7634f,
    0x42dd408b, 0x5099666d, 0x778b00b9, 0xce5cbd78, 0x5005fc9d, 0x4ddb86b5, 0xa72fe18d, 0xe0270592,
    0x2f91d9a8, 0x9fbe8835, 0xd5c9afd8, 0x692238f8, 0xc7eba37c, 0xf0faf0d6, 0x82565a0c, 0xa3a27e87,
    0x40fc8826, 0x7c1ba298, 0x89603776, 0x0913abbc, 0xc8f9b46d, 0xa83cbb54, 0xda28c0f6, 0x4a46717b,
    0xee0ffd9b, 0xae19eb4b, 0x4ac56182, 0x7c84f035, 0x4252eada, 0xda982d83, 0x972d2a90, 0x9c7cdea5,
    0x9b4fdebd, 0x16c9fdca, 0x8b926439, 0xd74d2de4, 0x6049c85c, 0xec3884a6, 0x83b01b64, 0x21ebca0b,
    0x8a1c8eee, 0x720160f8, 0x450344b3, 0x708aba07, 0xd1691e95, 0x9ce874fd, 0x773c911a, 0xd3b91c18,
    0x64aeebc5, 0x52a69e4d, 0x95ffae49, 0x18384a5d, 0xf597bc8f, 0xa1036bdb, 0x0d7ac228, 0x7eb75318,
    0xe645ce7a, 0x27b451cc, 0x2141ec4c, 0x23452f7a, 0x71e6be76, 0xa0e04d81, 0x1a01b3be, 0x0d879805,
    0x8ac2b64b, 0x9628dfac, 0x5867c7f4, 0x501e19b0, 0x80a7bab4, 0x075dd801, 0x293eee13, 0x4990aede,
    0x5713099b, 0x762ac379, 0xd3b5d1e7, 0xf06f2e26, 0xff55919d, 0xb9548607, 0xa7e7d475, 0xa74837c4,
    0x20cbe371, 0x94e8f791, 0xd2626059, 0xccdfe6be, 0x7912173b, 0xd38048fc, 0x66ffcda2, 0xbd1ea230,
    0xb68ae7d2, 0x1b4dc762, 0x840a4aa0, 0x0c7340de, 0x5be06a08, 0xb802d531, 0xa86bc52d, 0x56493732,
    0x26212bef, 0x11d654f2, 0xebce4426, 0x48971f04, 0x4626347f, 0x61ae6ca8, 0xeb0a312a, 0xe1142f6c,
    0x8468a160, 0xb7c3561b, 0xd1a29f25, 0xd7d861fe, 0x851474c4, 0x25778f8c, 0x30c1f10a, 0x6f3b9f58,
    0x75c0d223, 0x2cb0fad3, 0x19867e88, 0x2501f48c, 0xc866fef9, 0x9edfa989, 0x610f272e, 0x29ef2df2,
    0x8d7b7fe7, 0xd007f632, 0x219871f3, 0xaa9f4e39, 0xb416b9d0, 0x9179a813, 0x3d392551, 0xf20cbbe7,
    0x40d2983e, 0x01e1f594, 0x9f90ccb1, 0xde9cd390, 0x43823143, 0xd3d0f449, 0xe938a231, 0xb78d3350,
    0x5e98addc, 0x33932187, 0x30fb2191, 0xf857230b, 0xe1073e0b, 0x7ce3370a, 0xb3be4849, 0x72d25f11,
    0xdafe8aa5, 0x3396e5f1, 0xd2bbba9d, 0xef1fc0e9, 0x77de7e89, 0x462fe03c, 0xa70a243b, 0x7b3008e7,
    0x5cbc1164, 0xee817380, 0xb8f01202, 0x5922a421, 0xe04ddcd8, 0x414b14e9, 0x21b5987e, 0x9cf1acd1,
    0x2e46e3f9, 0x589acd74, 0x4726a038, 0x535846d0, 0xe01adc08, 0x9931c9b3, 0xfc1151d3, 0x7c3483be,
    0x2e02d754, 0x4caf8e34, 0x34c59015, 0x90c6b68e, 0x026703f1, 0x48151c9c, 0xbea0a51f, 0x82745133,
    0x74d457e0, 0x6b9d3e55, 0xb3036200, 0x48c5eee0, 0x6628a2ef, 0xc5489ed8, 0x05078418, 0x70f7b61c,
    0x488eae4f, 0xd2f76864, 0x8091a102, 0x83de2533, 0xdc2bb6c3, 0xa65cfd3e, 0xef273874, 0x24cbecdd,
    0x3bfa3a25, 0x4e794c76, 0xa487d2bb, 0x3773e7d1, 0xbd0b8a9c, 0xc3ba2678, 0x12a28cf8, 0xa4806ba0,
    0x963f469e, 0x224fdac5, 0xa3b71e65, 0xa3d3d53c, 0x2d0de8f6, 0x39258df4, 0x9d290b60, 0x34bb126d,
    0xe8aa206a, 0xb6491b44, 0x8f07d051, 0xb23bc1de, 0xd07abf21, 0xb25fbfad, 0x14e3f2c4, 0xc26d34d9,
    0xd8f7f128, 0xb2db5fd8, 0x89114879, 0x00770f97, 0x99ebb0c5, 0x11bdd53e, 0xa7339c6f, 0x787bd5ef,
    0xdb8714b7, 0xb2c5211a, 0xa98a2862, 0x24c56185, 0xd4ecc75a, 0xa27d2766, 0xb2dc57c2, 0xad0ed2cf,
    0x42b5b33d, 0x8d0ec8b6, 0x11a79885, 0x63f49b6c, 0x48ee5816, 0x9e7f3bfe, 0xe7cb9c53, 0x24d7f12b,
    0x6e1056f2, 0xa72c2099, 0xa585ed58, 0x1988c03c, 0x8ba6584d, 0x6197093c, 0xf0e51f57, 0xb0915d2c,
    0xeb5d71ff, 0xb65df286, 0xb2636824, 0x0811951c, 0x5767762e, 0x73c26ac7, 0x78a4a9e7, 0x77023afa,
    0x755459e1, 0xb04ca846, 0x5d8b1c05, 0x87fbb836, 0xb5dd94c1, 0x84269802, 0xe6c1ff77, 0x7660d720,
    0xf665938c, 0xbb5faa81, 0x72e7765b, 0x6b500d6d, 0x3620c108, 0xa2bf4a15, 0xe4277cfa, 0x34375097,
    0x0665370d, 0x91bf512d, 0xaf0b0210, 0xa940ea01, 0xfbc92842, 0x1d93beff, 0x77f4a9cc, 0x5bea5283,
    0x23fb426d, 0x34d7a0d9, 0x749626af, 0x4e77819a, 0xa10730cb, 0x316a0521, 0xfde12beb, 0x86567e0b,
    0x865e0e77, 0xfdf0cd4c, 0xe8a4a4db, 0x837b85bf, 0x8b71fd18, 0x38597b36, 0xa77045e3, 0xe4b56c24,
    0xc56cc7d6, 0x3795570e, 0x95f521be, 0x773285d9, 0x08089d43, 0x19ba34ef, 0x00751f17, 0xa69d0f4c,
    0xfd03032b, 0x222d05bc, 0x5fcba007, 0x6d947531, 0xf158fd87, 0x461e2b4e, 0x547f01ab, 0xcc429aca,
    0xf5834719, 0xa5bdb02a, 0x58d74b16, 0xaf961883, 0x0bfa3c05, 0x01d7d3ec, 0xfcc34b7f, 0xd260a076,
    0x31fdf3ef, 0xb16ce9d2, 0x53213df9, 0xa0e66f20, 0xc9cfad8f, 0xc0348191, 0x0abc0d0b, 0x1d7ae6ba,
    0x2616270b, 0x83de0439, 0x2d9b8fbb, 0xb25a8d2e, 0x545d243d, 0x808657c9, 0x59c29c27, 0x933fc18b,
    0x0e10d4c2, 0x197a2c1c, 0x13b38de4, 0x967990bb, 0x5f875eb2, 0xf066eb07, 0x530ce5c1, 0x17be41ed,
    0xcbd14d5b, 0xa9d4aa7a, 0xacdfabe4, 0x9d5f7ad0, 0xced1360a, 0x462fa670, 0x3fa4cd79, 0x6756419c,
    0x77db3b34, 0x84ba7806, 0x4f18e17d, 0xef521f3f, 0x561b97ed, 0x969e1bfa, 0x0e684b1c, 0xf3427085,
    0xe1d59826, 0x5fc0c997, 0xea283938, 0x84a87f2e, 0xc72ce1fc, 0x1fb8efba, 0xb77d532e, 0x0ced4f88,
    0xd50cc7d3, 0x57b7b6a1, 0x7684b046, 0xd452c9cd, 0x62abd143, 0xd216fdc7, 0xd5f79f60, 0xa2f9d3a7,
    0x92d03524, 0x4b719931, 0xbe64634d, 0x4dde7b44, 0xee0ffd9b, 0xae19eb4b, 0x4ac56182, 0x7c84f035,
    0x239a0f81, 0x5b2463c4, 0xea3245c1, 0xde0dc47a, 0xf8755720, 0x96944d14, 0x218c8de8, 0x4844fd94,
    0x12461d88, 0x367e59cd, 0x5ff21b1d, 0x34648811, 0x71124949, 0x8ebc0ffb, 0x5cbe2f97, 0x630f3337,
    0xa888f5be, 0xd99b921c, 0x6c9a2c8b, 0x7e002825, 0x6bb4e02d, 0x6c20bbd4, 0xdcbbb25d, 0x1b0dda9a,
    0xd14d02d1, 0x944e38d5, 0x88a48a94, 0x2f6de591, 0xeef7a8b7, 0x166dec77, 0x639775f6, 0xc7b7394e,
    0x99064c42, 0x6abb2fc0, 0x36ac20fe, 0x99fd728a, 0xefb2bf12, 0xf2e8a2cb, 0x07fe6e87, 0xa5d83e26,
    0xe33163b3, 0x2747f481, 0x0fabcdbf, 0xfc88ee91, 0xf06798a9, 0x80a869ee, 0xf047ec34, 0x27d09c73,
    0x1d731767, 0x7d74074e, 0xb63c32fa, 0xfd80ef1f, 0x87dfc057, 0x12fd3b14, 0x5baae9a3, 0xb066e68c,
    0x5cbc1164, 0xee817380, 0xb8f01202, 0x5922a421, 0x9952dd94, 0xfaf306fe, 0x9454634d, 0xa96d2ed5,
    0xe6b20276, 0x2078da10, 0x2187381e, 0x3844c3f3, 0x904962d9, 0x9835076f, 0xb21e03c0, 0x7e6e9a54,
    0xee8fbfd9, 0x4ce3a6da, 0x3f11d396, 0xeadef9a1, 0x0996abff, 0x2d600a44, 0xcfc35680, 0x74756fcc,
    0x97c3fc47, 0x788ca328, 0xe31cc15d, 0x6c56f17e, 0xb01e2b3e, 0xec703842, 0xdd67299e, 0x2e4cc3ba,
    0x927c9707, 0x8fb6b661, 0x2e1cfd80, 0x85035c1a, 0x40d2983e, 0x01e1f594, 0x9f90ccb1, 0xde9cd390,
    0xd71ff237, 0xbfda1d42, 0xd237c1a6, 0xeaa551ca, 0xdf01c8b5, 0x65089d96, 0x4b563543, 0xb4164489,
    0x92947f5d, 0x0087f846, 0xbe0dc123, 0xee05db55, 0x47479018, 0x25a8a1bb, 0x12fec7ce, 0x152ec70a,
    0x2fdbc7d6, 0x51960755, 0xf8475135, 0x4c072b48, 0x7dcad637, 0xf5dafb63, 0xe0879cab, 0xbae7a2c4,
    0x7a14c474, 0x8f919895, 0xdf817eb0, 0x2f5bce21, 0x2ac6ac14, 0xf4a2ea4b, 0xdb2bcce5, 0x4d85eddf,
    0x356d452e, 0x64f783d5, 0x4fa10108, 0xa7a24639, 0xcda7ff9e, 0x9fb04116, 0xa675dbff, 0x7eecf875,
    0x8f0f4a9c, 0xe8d72fe6, 0xd84964f9, 0xf62154b7, 0xd7a7a5e8, 0xb9452f0d, 0x3e83125b, 0x75eddd99,
    0xb01e2b3e, 0xec703842, 0xdd67299e, 0x2e4cc3ba, 0x9d9cf420, 0x2f3d6d35, 0xdea9a47b, 0x84f7ae23,
    0xdc23f6e9, 0x1e50c2fd, 0x604c4f54, 0x39b8cdd3, 0x2a340425, 0x701e4c1a, 0xc678f268, 0xa88d4511,
    0xa6b24d87, 0xf2588578, 0x0139e3aa, 0x73d2d704, 0xbdf91fae, 0xe6d903f2, 0xc124fdda, 0x815692cc,
    0xda6f9b01, 0x109ab037, 0x0835c50e, 0x1f30af85, 0xec1b9339, 0x629bf3a2, 0x691f7b04, 0x075a2854,
    0xe323e239, 0x3f8d6deb, 0x91feddf1, 0xed4a02cb, 0x29cd83fc, 0x581925f7, 0x4853fb97, 0xe747ff03,
    0x3f9f8682, 0xdd6cb389, 0x9d1008e5, 0x15491340, 0x99b78255, 0xf7272a23, 0x1f1b299f, 0x766fc407,
    0x34365825, 0x4121eaed, 0xb838d876, 0x93cc4162, 0x208f2be5, 0x1c836ccd, 0x59575a50, 0x4078a313,
    0x64880838, 0xfb97836a, 0x0002f0b0, 0x3ec4bc6c, 0xb6002355, 0xbc68dbcd, 0x509d6cb3, 0x7262aa57,
    0x389a4410, 0x6888f710, 0xea2da858, 0x812107d7, 0xe8940910, 0xf3e2784a, 0x6af18c76, 0x189104a0,
    0xa15cbd00, 0xfa44dc68, 0xa069cc63, 0xd04cf8d8, 0x2641d28a, 0x1af7998f, 0x2f6716d5, 0xf230ec27,
    0x4b62c77d, 0x8cbccea9, 0x945a2d7c, 0xce31147f, 0xb79c66dc, 0x7b4ea0a7, 0xa803c265, 0x7a0ef8ce,
    0x5ed85544, 0x02ff5744, 0xb0d63deb, 0x3d9e0c8c, 0xfc7e34e5, 0x280372f7, 0x79ba1716, 0x8295eb82,
    0x54c3f203, 0xbab9cbf4, 0x22ba62e8, 0x4e4de1c0, 0x7c0c2615, 0x2cd9717f, 0xd6afbd0a, 0xacbfdb26,
    0x8e5829ed, 0x7d6baa73, 0x0d7b8d60, 0x650b3a6e, 0x2a6793db, 0x387b5722, 0x9b7a9229, 0xa704ff8f,
    0x3d32ef7e, 0x9fa7e503, 0xa1a7e23e, 0xb4c4a61f, 0xc2feffca, 0xb0200ea8, 0x60211630, 0xc548cf36,
    0x6e3b20b6, 0x966fe2d1, 0xe5174459, 0x5db81045, 0x69e057fd, 0xa58e8dca, 0xb7ac350f, 0xeb842bef,
    0x250280bb, 0x83113ca1, 0xd977719f, 0xef1f6175, 0x0d2cfaaa, 0xfff8cced, 0xafd430bb, 0x47a627ac,
    0xf898a8c8, 0x3b870bd7, 0x72aeab77, 0x0c48c706, 0xb6fbf886, 0x1bd36357, 0x883e25f3, 0xc8366bed,
    0xd09dbe4e, 0x5b54fcfa, 0x84a9b760, 0x7a95aaa4, 0xfb8e1efd, 0x8b6d2e4a, 0xb22e221b, 0x7b488d36,
    0x8f1b1907, 0xe88b3632, 0xb141e7fe, 0x5bacc822, 0x3a51b7bc, 0x78ab7ccd, 0xaa57255f, 0x1ab899b3,
    0x71317c0a, 0x5eff53c4, 0x94c42198, 0x4be8d7aa, 0xd73f35b9, 0xa5403ef1, 0x01f35051, 0x8f2323ef,
    0x1f2b3678, 0xc7684343, 0x12a89029, 0x333671c7, 0x3d32ef7e, 0x9fa7e503, 0xa1a7e23e, 0xb4c4a61f,
    0x3543400a, 0xbd4342c8, 0x6ada002e, 0x5184705d, 0xdf3e8de0, 0x1ee3f5b9, 0x1922fc5e, 0xf74a8d93,
    0x031038f3, 0x37bd2591, 0x1c55e965, 0x84d1164a, 0x8feddb1c, 0x7fc43b42, 0x4f8d2338, 0x5b674d94,
    0x6f9ce51a, 0x5b733e17, 0x7c454dc3, 0x05df535a, 0x6f333767, 0x4ca3a0e8, 0x2ae20728, 0x1110efd0,
    0x5bc947cf, 0x835efbcf, 0x01045835, 0xffc0712b, 0x9239f17b, 0xd13fc8af, 0x5233ce50, 0xc80b85e2,
    0x261c4864, 0x7447520c, 0x610f6fd6, 0x2baaa7e0, 0xc01b2b18, 0xaa6d541e, 0x6983df90, 0x1f897cbf,
    0xae3ae37c, 0xa269fe96, 0x6940f38b, 0xab168e8d, 0x1880e6ed, 0x28be33d4, 0x68150935, 0x7fcdff3e,
    0xe2152904, 0x14cfa688, 0xfbe884cb, 0xb20b3a79, 0x5dfc418c, 0x16969d5f, 0x817341df, 0x3b3e88cc,
    0x33ab1f3b, 0xe1a26559, 0xdca6aa08, 0x838e1af5, 0xfcd134de, 0xa7203af7, 0x231ba103, 0x7ce3971b,
    0xca87b632, 0x458aa658, 0x65a2bf80, 0xe7589c61, 0x761e4150, 0xb4962194, 0xd319d5e2, 0xe44e4191,
    0x4e5ca580, 0x9b2d08f7, 0x5cde794b, 0x4fb77360, 0x5dfc418c, 0x16969d5f, 0x817341df, 0x3b3e88cc,
    0x89c6aa7c, 0xa315f27d, 0x349c23d8, 0x496894ae, 0x739d2d43, 0xe05189ac, 0xb5060a42, 0x6fdefa4d,
    0xd08012fc, 0x8e587fac, 0xd7d2ca0c, 0x1d0d1448, 0x18562312, 0xd3c76c86, 0x59e7585c, 0x468fe982,
    0x8b4564e0, 0xd16e9758, 0xcde58636, 0x222302da, 0x4e0a3509, 0x0f7e4fbc, 0xab90ce84, 0xacc7f6d7,
    0xdcd9a5bb, 0x4b85f2d2, 0x870e08bd, 0x31f129a6, 0x261c4864, 0x7447520c, 0x610f6fd6, 0x2baaa7e0,
    0x8c77b9da, 0xb0ec8bc6, 0x756bece4, 0x3f3865ce, 0xd0c1c218, 0x3d6d07f3, 0xa279c1c7, 0x2ebbaa0b,
    0x7a39d4b1, 0x703c2535, 0x167bdb5d, 0x9b901372, 0xc2ba4435, 0xb9a17ecc, 0x5ba2bcbf, 0xb94965d0,
    0x0dc45cbc, 0x883ff9f1, 0xd779a8a5, 0xca5c4b2c, 0x2be0d198, 0x82c9f5e4, 0x5111bdb3, 0x32c75fc6,
    0xa878e7cb, 0x2be08185, 0x23aedfec, 0x7f02919e, 0x0996abff, 0x2d600a44, 0xcfc35680, 0x74756fcc,
    0x3a5dd8ba, 0x65527a21, 0xfb9e72e3, 0x7ee053f7, 0xf0905614, 0xdd0bf224, 0xcdc4eced, 0xb45f279e,
    0xb20c5766, 0x3dbf7d83, 0x90d0286c, 0x54a9a45d, 0xda4a2705, 0x1fead968, 0x4623ffd7, 0xeee0de08,
    0x93d9a7a6, 0x02cba713, 0x400ef65f, 0xdb2c33a5, 0x7979f093, 0x851c1f1c, 0x716e1f5b, 0x638b9587,
    0x8e65d770, 0x9d3f9305, 0x4be0302b, 0xc3fbfc31, 0x5f6cf199, 0x11738889, 0x00d59f30, 0xbe86d152,
    0x1a872a8c, 0x67f24576, 0x4a6a47bf, 0x8c5510ca, 0xaa467472, 0x4616c6fb, 0xecaa59ea, 0xc86ac140,
    0x6bfdcb02, 0x3fcca8a3, 0x62b406e0, 0xd6409222, 0x6e17f110, 0x2048e313, 0x3e29d9ab, 0x65861928,
    0xed013951, 0xac1b0623, 0xa8f5b55e, 0xe48e5e60, 0x78ea8f65, 0xf586d8b9, 0xa3285038, 0xc6e0256a,
    0x9751c3fe, 0xc3ac7596, 0x05d47bee, 0xcca7bf03, 0x98c251fc, 0xeda4ad00, 0x40d2af9d, 0xff4914ca,
    0x8091b5d5, 0x8e5b3e43, 0x77a1741f, 0xe047ced5, 0x202e7743, 0x709207ab, 0xbcbc517e, 0x1e89d9c3,
    0xa9cde705, 0x5a7b91bd, 0x94dcb292, 0xe8675379, 0x594aba00, 0xbbb7ed7e, 0x3baee509, 0x5bbb08c6,
    0x1adc7604, 0xcdfd4715, 0x3a225206, 0xe84558e3, 0xa7c4a69e, 0x27e1e482, 0x99ced6b5, 0x8695f764,
    0xb5aa77dc, 0x1e1f6ac3, 0x70d42d14, 0x7bf54a9c, 0x1a872a8c, 0x67f24576, 0x4a6a47bf, 0x8c5510ca,
    0xfb2a388b, 0x678ee498, 0xedb009f6, 0xb92a160b, 0xe148e3f3, 0x856ebadc, 0x3f0918eb, 0x44b4f4ee,
    0xbaeac3cc, 0x84ac1b53, 0x73c1fc96, 0xf4f93175, 0x4a8347fd, 0x1c2d926b, 0xdac93fb1, 0xc1d388c0,
    0x67e069f2, 0x91e18d3c, 0x410a44c8, 0x5e6f2b08, 0x6badeefb, 0xa7206ac3, 0x2a463f06, 0x7a4e3579,
    0x04f578cd, 0x325ccdca, 0xa11fb672, 0x7515a29d, 0xe13f2663, 0xd556943f, 0x7632f8c1, 0x01e34b9f,
    0x9ebb24c9, 0x08ecfc4d, 0x1e048a9c, 0xf375f2d3, 0x173e3e0a, 0xa58d65a0, 0xcbcb3076, 0xda165ae4,
    0x47a00359, 0x3368e82a, 0xa034c152, 0xa6b56ecc, 0xbad1eb3c, 0x863c23cc, 0x21229be7, 0x05eb8c90,
    0x41714d15, 0x9b970001, 0x6d9a25b1, 0xd20efe0e, 0x1f84c00f, 0x665d286b, 0x13749998, 0x11fe2af1,
    0xf172f5bf, 0x2f7f1374, 0x7c610cbe, 0x25970aac, 0x954ae7bf, 0x05ffd2a7, 0xa2fa0038, 0x89f632cb,
    0x1dc4bd1a, 0x54a6f545, 0x571d5829, 0x53c2d84d, 0xa6b24d87, 0xf2588578, 0x0139e3aa, 0x73d2d704,
    0x349ed76b, 0x24987df3, 0x73b843ee, 0xa88538a3, 0xda6f9b01, 0x109ab037, 0x0835c50e, 0x1f30af85,
    0x0843d5b4, 0x878cd279, 0xdb0d247e, 0x186a0e3f, 0xe323e239, 0x3f8d6deb, 0x91feddf1, 0xed4a02cb,
    0x04d45ead, 0x962b6791, 0xbc788eb3, 0xb5d6d934, 0x3f9f8682, 0xdd6cb389, 0x9d1008e5, 0x15491340,
    0x99b78255, 0xf7272a23, 0x1f1b299f, 0x766fc407, 0x34365825, 0x4121eaed, 0xb838d876, 0x93cc4162,
    0xf37f1577, 0x33f202aa, 0xc611625b, 0xc3d7cda2, 0x6fbd8eaa, 0x6fb28edc, 0xf738e360, 0xc2e12d43,
    0xe54102b2, 0x32780df4, 0xc8fbe791, 0x636a2a1f, 0x96941588, 0x1ac785a0, 0x9575e879, 0x13b514e7,
    0x35c78fa5, 0x60b9dfbd, 0x3dea3626, 0x6ef6db22, 0x878011e9, 0x50c7a0fb, 0xdce65131, 0xc9143bda,
    0xf3e2547b, 0xe30fa535, 0xdf4f20cb, 0x94904aa1, 0xc68b9a44, 0xcf9f1cd4, 0x58c66eb8, 0xe0863974,
    0xb743a323, 0x7cc081d8, 0x9d931f6b, 0x2eb3bfd5, 0x89bf81b6, 0x0004562a, 0xc4234b41, 0x75203dbe,
    0xc4e03e61, 0xb7d8e007, 0x63d0e7be, 0x990665b3, 0x35bdbce1, 0xf3fa6d47, 0x4b8803a2, 0xb83e690b,
    0x87b0d940, 0x06a17a06, 0xdac5d776, 0x5a4f0f3b, 0xa91ab2b8, 0x59d1dfee, 0x99947138, 0xfa5b2cfb,
    0x5fe257fd, 0x0574ddb4, 0xd711d97e, 0x6a985716, 0x24f6779e, 0x76f9c0eb, 0x0f5cd034, 0x5befec57,
    0x6fbd8eaa, 0x6fb28edc, 0xf738e360, 0xc2e12d43, 0x35d5618c, 0x62d5220b, 0xfc1f27c5, 0x0ee8ac54,
    0xd73d6488, 0x69db0372, 0x908efffb, 0xb28f5391, 0xd1695b40, 0x8e01fc0b, 0x1a0005a8, 0x7d2171ae,
    0x4124aa4a, 0xbe9a4339, 0x2b03fe7f, 0x96fb174f, 0xcc549f83, 0xd0e44cf8, 0x4a4d6398, 0xf248a788,
    0xe85b7620, 0x44dd4428, 0x7777a6b7, 0xe8a8d6de, 0xaecef296, 0xa5c3e8ac, 0x8a378e54, 0xc216571a,
    0xfca4daf4, 0xbd24dd63, 0x0802acc8, 0x64047ad8, 0x0d9c313e, 0x1c5d262a, 0xf75dcd40, 0x59826100,
    0x48d59732, 0x48c0baa3, 0xe6b3818f, 0x79ab6193, 0x01e164cc, 0x30f3ce00, 0x8bf21ad3, 0xd92a46bc,
    0x974b2302, 0x7cf478d0, 0xca69f907, 0x96094db7, 0xafab382a, 0x8b46fdc4, 0x701a34cb, 0x61973559,
    0xfc7971e0, 0xf83efb35, 0xa5ff9728, 0x46f37f57, 0x19164f52, 0xc8bcbfd0, 0xd944f6aa, 0x18b87377,
    0x3c706f05, 0x98dfde7f, 0x01a0316b, 0x110a0c0e, 0xa1364e85, 0x2feb00a6, 0xa89f7659, 0x82a8c1e5,
    0x4ff6986d, 0x8f33e826, 0xe63a44db, 0xf6ee9609, 0x3d16dec1, 0x785e89d5, 0x25d31127, 0xfe06873b,
    0x8c6a3de6, 0x2512f122, 0x0125de84, 0x9b5b1274, 0x956d6741, 0x3415fd37, 0x41cf24ce, 0x66ab986e,
    0x3280749c, 0xb0814b05, 0xfc4f7b0a, 0x7fe43f76, 0x3127a6b8, 0x05302208, 0xb3333191, 0xbcd09dc8,
    0x6164af02, 0x2095153a, 0x1485b943, 0x15fe7493, 0xc0edb8ef, 0x59c041d7, 0xc9edd7d1, 0x47d8fe1d,
    0xf583603a, 0xc680430a, 0xc1b73af9, 0xd9856d6b, 0x480724a0, 0x575e5663, 0xfd059905, 0xa19bf09b,
    0x0d9c313e, 0x1c5d262a, 0xf75dcd40, 0x59826100, 0xdf1b9387, 0x1464242a, 0xa69601fd, 0xd3c5ccca,
    0xb4a2f546, 0x6aef95a7, 0x36bbe947, 0x16bdc21a, 0x22c9c6ab, 0x4b320ea2, 0x6a62c1af, 0xf36ec8ea,
    0xc0b52822, 0x75dd8039, 0x795c5e04, 0xea715813, 0x8feddb1c, 0x7fc43b42, 0x4f8d2338, 0x5b674d94,
    0x9c688a34, 0x5aaac4f2, 0x4bd8d7bd, 0xf1c92f8f, 0x6f333767, 0x4ca3a0e8, 0x2ae20728, 0x1110efd0,
    0x5bc947cf, 0x835efbcf, 0x01045835, 0xffc0712b, 0xfa8406b2, 0xe4719892, 0x81afa80b, 0xd0ba3e9b,
    0x261c4864, 0x7447520c, 0x610f6fd6, 0x2baaa7e0, 0x9c9cb129, 0xf1b21401, 0xacd1fa76, 0x0e4ec5ea,
    0x8aa28427, 0x93462f8a, 0xd0978354, 0x37641c58, 0xdcd9a5bb, 0x4b85f2d2, 0x870e08bd, 0x31f129a6,
    0x261c4864, 0x7447520c, 0x610f6fd6, 0x2baaa7e0, 0xaaa202eb, 0x4f3d1633, 0xd609f73b, 0xb50b72db,
    0xd0c1c218, 0x3d6d07f3, 0xa279c1c7, 0x2ebbaa0b, 0xcfcf35a5, 0x0043e0a1, 0xc515dfd3, 0x45770eda,
    0xe13dbe8b, 0x8882f234, 0xa77e9ef1, 0x983ec8ab, 0x6bfdcb02, 0x3fcca8a3, 0x62b406e0, 0xd6409222,
    0x807f5d95, 0xd5c076a8, 0x9b350f27, 0x80e1f433, 0xed013951, 0xac1b0623, 0xa8f5b55e, 0xe48e5e60,
    0x085277b9, 0x929e49f1, 0xe457df11, 0x4d4f7457, 0xfb377eda, 0x457658d5, 0xda4830c5, 0x800b7755,
    0x98c251fc, 0xeda4ad00, 0x40d2af9d, 0xff4914ca, 0x02182659, 0xc5c023ad, 0xf0427934, 0x3c2acf61,
    0x55a1979f, 0x7fd9aaf8, 0x8acf3bee, 0x2a8e5953, 0xd8aca2fe, 0x067d889b, 0x246cebac, 0x6e68ee0d,
    0x98c251fc, 0xeda4ad00, 0x40d2af9d, 0xff4914ca, 0x40035cc0, 0x5370b205, 0x4b89c465, 0xcb39159f,
    0xd14d02d1, 0x944e38d5, 0x88a48a94, 0x2f6de591, 0x1c18fd60, 0x71b9fdb9, 0xadd6cd6c, 0x8ae0b553,
    0xdc5f33ca, 0x02012a4e, 0xae5ec25b, 0xbc2b3d2d, 0x40d2983e, 0x01e1f594, 0x9f90ccb1, 0xde9cd390,
    0xbd5b7a48, 0x386008bd, 0x44f96533, 0x8f39ff8f, 0xdf01c8b5, 0x65089d96, 0x4b563543, 0xb4164489,
    0x57c6d33b, 0x10514572, 0xda9a35b8, 0xdf2f3c30, 0x47479018, 0x25a8a1bb, 0x12fec7ce, 0x152ec70a,
    0x2fdbc7d6, 0x51960755, 0xf8475135, 0x4c072b48, 0x16cc0717, 0x529ff101, 0x1f70d3d0, 0xf2c6af30,
    0x40986616, 0xada24406, 0xce4106c8, 0xdbca9068, 0x840c65e0, 0xd2531f9a, 0xa8387656, 0x9b5118fd,
    0x21928dba, 0x38b5c282, 0xb44f2bd5, 0xc2fbeed5, 0xbb455d49, 0x3a22c2f6, 0x39e61e9a, 0x2d8b06e5,
    0x02757c0a, 0xfac62b66, 0xe24b79da, 0x98d4e108, 0x80f638ca, 0xa05374d0, 0x473b0da3, 0x8b3f8802,
    0xacb6b68e, 0x6d8b41d1, 0xa2f95772, 0x10d830bb, 0x120b9ae7, 0xb3c1d33c, 0xf6d071d1, 0x1d307f59,
    0x58fc4c3b, 0x5b217598, 0xd5f23a58, 0xb6b49f0a, 0x3127a6b8, 0x05302208, 0xb3333191, 0xbcd09dc8,
    0x01257b25, 0xb4366c2f, 0x07f836e2, 0x55e47256, 0xb16b411a, 0x357ca7c9, 0x22222501, 0x450fa0a6,
    0x07f61407, 0xced23c7d, 0xdab436d5, 0x21779c9f, 0xf93a58bd, 0x5d88031e, 0xbce4c5e5, 0x04630acd,
    0x96c372db, 0x021070f2, 0x83075c00, 0x56d8334d, 0xbda2e097, 0x11c74a95, 0x69b06855, 0x0934adbf,
    0xfbcfd84e, 0x132e7558, 0xc32575be, 0xbbedc11f, 0x6deb27a2, 0xf6e8fe00, 0xf60d4f87, 0xbbd83a14,
    0xcbd14d5b, 0xa9d4aa7a, 0xacdfabe4, 0x9d5f7ad0, 0x57f7e305, 0x0bb27b1f, 0xc9fb800b, 0x0b948833,
    0x77db3b34, 0x84ba7806, 0x4f18e17d, 0xef521f3f, 0x0d09fa9d, 0xab9a332e, 0x4f5a0b4c, 0x41a776e8,
    0x5002f418, 0xac8ad721, 0xa584f23b, 0x6f5ac769, 0x0be18ab7, 0x72742d65, 0xe23e2058, 0xb04f6e0d,
    0x371c683d, 0x2b46eb31, 0xda0150a7, 0x4faf185b, 0x47deb7da, 0x80ea344f, 0x3cc8f9b4, 0x3f5830a7,
    0x26dd145b, 0xb38d6827, 0xa32894a0, 0x8876864f, 0x6717d9c5, 0x9853b297, 0x740fe527, 0x8058e939,
    0xee95bf75, 0xc8cf15d2, 0xd83eab34, 0x7cc26daa, 0xabd325b6, 0x5958801a, 0x53b38cef, 0xb4772340,
    0xa9b052cb, 0x3caa17a4, 0x916e20ff, 0xf15d4f66, 0xeb59f532, 0x8a8036a8, 0x130dacc5, 0x94c7e940,
    0x7a427d0f, 0xc2c12b50, 0x253f4fee, 0x83c1044a, 0xda396d80, 0x938444b7, 0xfc6cc123, 0xd0ac6c1e,
    0x0e1ab1ab, 0x1bc16f61, 0x8ba8c54c, 0x47aae7d7, 0x1d7956b2, 0xb6450c18, 0xa35e8e5a, 0xbd70fa7a,
    0x765ed7f1, 0xbed1aad2, 0x4171ac30, 0xb5c141b8, 0x46501233, 0xc377c153, 0xd2c59cdb, 0x9a687e10,
    0x0d2cfaaa, 0xfff8cced, 0xafd430bb, 0x47a627ac, 0x4c0934c8, 0x4cb9a5e2, 0xf080c840, 0xe2584057,
    0x8f4ebecc, 0x9e27712c, 0xf6e6e31b, 0x62661899, 0xff02aed5, 0xc3770c5a, 0x1a9e0aeb, 0x5356387c,
    0x145d98bf, 0xb428f5c6, 0x97f420c3, 0x32ce57d9, 0x28165ad4, 0x08126502, 0x699aa738, 0x61ac39e7,
    0x70012e8a, 0x16d4dbf5, 0xa3836205, 0x0eb824e6, 0x98956687, 0xb77629dd, 0x6f6fb053, 0xbcdaa20b,
    0x44b1bd38, 0x09558b97, 0x9e12f626, 0xc0fa2728, 0x9b4fdebd, 0x16c9fdca, 0x8b926439, 0xd74d2de4,
    0x94de07bc, 0x1c110d37, 0xe8803f24, 0xd0e32ebe, 0xda396d80, 0x938444b7, 0xfc6cc123, 0xd0ac6c1e,
    0x918c1631, 0x3e2993e8, 0x00fa308c, 0x7472b248, 0x1d7956b2, 0xb6450c18, 0xa35e8e5a, 0xbd70fa7a,
    0xdeb7e155, 0xaef32b51, 0x68cf7900, 0x3d137a10, 0x46501233, 0xc377c153, 0xd2c59cdb, 0x9a687e10,
    0x0d2cfaaa, 0xfff8cced, 0xafd430bb, 0x47a627ac, 0x381e7d16, 0x300714ac, 0x66b1c2c9, 0x614823b0,
    0x8f4ebecc, 0x9e27712c, 0xf6e6e31b, 0x62661899, 0x584216a9, 0x35f38be5, 0xf664b7cf, 0x2415721e,
    0x3866a1de, 0xad72f118, 0x6fcaa75b, 0xd106e479, 0x29f33f77, 0xcb0cb695, 0xadd6ec70, 0xd4acd477,
    0x3d69c68c, 0x18e031ff, 0x380fba97, 0x284ea4bd, 0xd0c4c2a2, 0x3c4a4ee4, 0x790a252b, 0xbf703ef9,
    0xec3488b2, 0xb62f8dd0, 0x1751d756, 0xc4545ba3, 0x6f6a189f, 0xa0945704, 0x59f4a1c4, 0x997c294f,
    0x15786cd4, 0xe1f54ad9, 0x6ddb2a9a, 0xd27f8052, 0x1b6bc465, 0xb6de7036, 0x4747256a, 0xfa67444e,
    0x40fc8826, 0x7c1ba298, 0x89603776, 0x0913abbc, 0xb45be529, 0x5094f068, 0xd280cae8, 0x7c31d972,
    0x989ca208, 0x1cac6399, 0xab9ed034, 0xa102b47e, 0xae2becb9, 0xe120d4f3, 0x11c99141, 0xca7dc4cd,
    0x08054e89, 0x9dab1b01, 0xaf452e13, 0xcd0df7a1, 0xf5ab8c4c, 0x87e93ff4, 0xd1186496, 0x91301320,
    0x9197a18c, 0x2d743c1d, 0x06925eac, 0x92842da4, 0xe1561391, 0xd56f3add, 0x687176b9, 0x1738ed1b,
    0x0a131a85, 0x3fa93d7e, 0x08c5471a, 0x3c56cc46, 0x9c18e1e2, 0x33410033, 0xeca0b3a4, 0x56366654,
    0x19ff1e66, 0x84f46e8a, 0xe7dfcae3, 0xd0f386f9, 0x538dbf5f, 0x4dff5c2d, 0xe95ae68e, 0x2c22cff2,
    0x3962f5d2, 0x1ebd3f73, 0x439e59b4, 0xf56f8386, 0x2558bec4, 0x303ef5a5, 0xb1a4617b, 0x0ff1b4ae,
    0xb9d54e59, 0xd6a179c6, 0x9b32a0f8, 0xb70b4d26, 0x81b8369c, 0x03e1b03d, 0x2a48bbda, 0xcfa92acc,
    0xd2ea034a, 0x4a5127b2, 0xcf8fbf1e, 0x9f8dfbb8, 0xb985b937, 0x1193f672, 0x37542ae4, 0x4ea33995,
    0xb220779f, 0x54942c5b, 0x5a544695, 0x539233fc, 0xe26ba872, 0xf2d92cde, 0xed9dafd9, 0xb827eb35,
    0x57899f72, 0xea6d249f, 0xecaa156b, 0xda2a1bdf, 0x8e3726b5, 0xb77ab467, 0x862465e9, 0x54867f3c,
    0x3566cc13, 0xa9dae45d, 0xfd9260c9, 0x1862e470, 0xc499f1c8, 0xd1153c17, 0xb978546b, 0x29323dd3,
    0x55de753b, 0x24c1d1ec, 0xfe9302d4, 0x65c8cbbe, 0x0b8ffdd0, 0x1defd57a, 0x53314675, 0xce10824f,
    0x9b594bad, 0x435aeb63, 0x4d1a8fc6, 0x83cbb81a, 0xb0836ea9, 0x778e85ed, 0x17f1b083, 0x86636ab6,
    0x6eddebfd, 0x7d6ea7fd, 0xf83a8698, 0xed5d9ad0, 0xd57395a6, 0x677acd53, 0x70b62cc8, 0xdce2fff3,
    0x9ae3ddda, 0xf7e95694, 0xd9246246, 0x9828e762, 0x1d7956b2, 0xb6450c18, 0xa35e8e5a, 0xbd70fa7a,
    0x86c77f7e, 0x27a86a33, 0x7c0ddfef, 0xbdb529ed, 0x46501233, 0xc377c153, 0xd2c59cdb, 0x9a687e10,
    0x0d2cfaaa, 0xfff8cced, 0xafd430bb, 0x47a627ac, 0x570967d2, 0x7bb6a3d1, 0x59813d99, 0x1e1972e7,
    0x8f4ebecc, 0x9e27712c, 0xf6e6e31b, 0x62661899, 0x66dd8e09, 0x0af4a046, 0xfeb04a14, 0x6981e570,
    0x3866a1de, 0xad72f118, 0x6fcaa75b, 0xd106e479, 0x29f33f77, 0xcb0cb695, 0xadd6ec70, 0xd4acd477,
    0x30382ee5, 0x700dd889, 0x7bebcc89, 0xacf16191, 0x2616270b, 0x83de0439, 0x2d9b8fbb, 0xb25a8d2e,
    0x545d243d, 0x808657c9, 0x59c29c27, 0x933fc18b, 0x0e10d4c2, 0x197a2c1c, 0x13b38de4, 0x967990bb,
    0xe6ad4a12, 0x2c5f98dd, 0xb6f84faf, 0xb134e83c, 0xf7f7c52c, 0xdfa46918, 0x2d8f0a72, 0x70edb14f,
    0x30d78709, 0x5545dfbe, 0x704fdb90, 0x1f8273bf, 0xec6839a3, 0x46734025, 0x2f31901f, 0x063f3238,
    0x9385b0b3, 0xfe9bc91b, 0x48eb7b62, 0x57d01d85, 0x20db6aba, 0x4f0b12a2, 0xa12845b5, 0x94c9a0e1,
    0x8d3c3c1d, 0x0f943fa4, 0xe955b4c4, 0x85cae7d3, 0xee2396a5, 0xe916f352, 0x1d6578bb, 0x0868bbd8,
    0xe9e357aa, 0xfca66867, 0x26190862, 0x31566606, 0x3464e0a4, 0xeed1ce6a, 0xb08042f7, 0x75ac6247,
    0x79cd3e32, 0x54d5bf3b, 0x761c114b, 0x6f78bbd6, 0x32371ffb, 0x3c9709ef, 0xc1e770bd, 0xb3a8f0dd,
    0xf230e836, 0x2ecf1aea, 0x32e8eb2a, 0x7f69655a, 0x9f421ae5, 0x7aa6a908, 0x3e92f38a, 0x2021cfce,
    0x388e7396, 0x16c30cfd, 0x38a2e2dd, 0x5bf85605, 0x620a2799, 0x1bf3ace5, 0x30f37a69, 0x20cb495d,
    0xdafff406, 0x1b3fc3f9, 0x941d3c73, 0xb465263d, 0x52106be8, 0x5161ad97, 0x419ef1ab, 0x4d25df7f,
    0x8f058a69, 0xce29abaf, 0x6d02fdca, 0x6c78bd53, 0xa71e25f6, 0xeb246914, 0xc6f67180, 0x27b7634f,
    0x42dd408b, 0x5099666d, 0x778b00b9, 0xce5cbd78, 0xc0514e8f, 0x6a26b3ec, 0x1f7ce079, 0x5ac58e2f,
    0x8d0127b9, 0x44e4e960, 0x2eb0b25c, 0x58d816e7, 0x22263664, 0x70a3cff8, 0xd99714a7, 0x4a0b07fc,
    0xfbfa46b7, 0xa06d0848, 0x47eb5b54, 0x1c508bd8, 0x1ee755a3, 0xb95ad96c, 0x0dfa486b, 0xd7d97a4a,
    0x826de74d, 0xf135e663, 0xaf5c777a, 0xb41f67f9, 0xa5666479, 0x1cf53fc3, 0xe34cea79, 0x47de0ffd,
    0x528111af, 0x446500ea, 0x93762254, 0x03b99166, 0x9bdf370f, 0xccc70e96, 0x6db02591, 0xba279c2c,
    0x0e10d4c2, 0x197a2c1c, 0x13b38de4, 0x967990bb, 0xd7800043, 0x161689af, 0x7a42ddf4, 0x85f12f09,
    0x2616270b, 0x83de0439, 0x2d9b8fbb, 0xb25a8d2e, 0x545d243d, 0x808657c9, 0x59c29c27, 0x933fc18b,
    0x0e10d4c2, 0x197a2c1c, 0x13b38de4, 0x967990bb, 0xeec38c41, 0xcd84771a, 0xd83f2b3e, 0x803d8bbc,
    0xca2ce3e6, 0xde7c0337, 0x921324aa, 0x90e9e7d1, 0x94470859, 0x2d277148, 0xe88ce946, 0x842f4b33,
    0xf172f5bf, 0x2f7f1374, 0x7c610cbe, 0x25970aac, 0x5a061242, 0xa0751a92, 0xc458b79e, 0x426752cd,
    0x6e505248, 0xb0d83c89, 0x0652e489, 0x06ac0f23, 0x45e2e00a, 0x1b587c86, 0x2b012785, 0xc097755f,
    0xf2dc6d08, 0x1dd6a315, 0x22745b79, 0xed7f95d6, 0x0d268e65, 0x831b2f0e, 0x2f3620b8, 0x7e979d83,
    0x21ca41d8, 0xf5b41e4f, 0x8c974a1e, 0x1e3ba35d, 0x5415135a, 0xc5b20114, 0x2c52eb26, 0xe95f8c19,
    0x3725303a, 0x125c083b, 0xf5e46c27, 0x45e86085, 0x1ad91d3e, 0x9f54ee77, 0x1e47c449, 0x2ea6c8f7,
    0x21ca41d8, 0xf5b41e4f, 0x8c974a1e, 0x1e3ba35d, 0x86baadac, 0x2683e636, 0x0be42747, 0xa1aecac9,
    0x5e5729e4, 0x7d028ed7, 0x7791c7ba, 0x864bee09, 0x8d054249, 0x9ff2d37a, 0x89c17855, 0x8c416a4d,
    0xbba11106, 0x48187904, 0xdcd02005, 0x0339a5fc, 0xd5e09090, 0x4f97c72e, 0x6707abad, 0x284c7420,
    0x71c9d1f3, 0xe0ef5618, 0x67ea5740, 0x49f48422, 0x145d98bf, 0xb428f5c6, 0x97f420c3, 0x32ce57d9,
    0x3a276daa, 0x28d33c9c, 0x5fd372c7, 0x591cd55d, 0xbe40494a, 0x1d642b87, 0x17561743, 0xfeac823d,
    0xaeb33fd8, 0x914e5b9c, 0x46d8ab1c, 0xcd1d9db3, 0xccb8c901, 0x1e8d760f, 0xd2a3728a, 0x49ca89aa,
    0xa48dcef5, 0xe5d8bc60, 0x1b9c18f9, 0xa53aa0f7, 0x3e65a03a, 0xeb03b277, 0xca0fe841, 0x563c7979,
    0x9e9fb54b, 0xed514902, 0x766dc20c, 0x997cb3e6, 0x42fe9ac9, 0x92520264, 0x5c9a6a52, 0x9c7f07b3,
    0x71c0de3e, 0xaf480bc5, 0x061bdc4b, 0xa4250623, 0xba18ff9a, 0x2d844500, 0x332432c5, 0x75338d20,
    0xec8d9638, 0x4ee1d4bf, 0xda41dc02, 0xb908111d, 0x91541444, 0x4a138c25, 0xf973f500, 0x1ff91148,
    0x27175a7d, 0xe11bbabc, 0x192b3b46, 0xe8b45866, 0xe440e09d, 0x74bd3acd, 0x0a2abccf, 0xcd4bd2e4,
    0x006e2f6b, 0x894f131c, 0x67d7d06f, 0x88c32bd8, 0x41361345, 0x50e59d9d, 0xe70bad21, 0x649651ec,
    0x397dd3eb, 0x38bfe659, 0xe19f5455, 0x9e7c6507, 0x8ac582cb, 0x25cbfdf1, 0x4bdc3ca7, 0x40eea6dd,
    0x8ce8ce6f, 0xf25959bf, 0x48ee6b35, 0xc0ad2f49, 0xf0e49240, 0x93d05296, 0x54868d58, 0xa2872197,
    0xf5b72266, 0xcbf124c8, 0x7b4e62b1, 0xfdc511b7, 0x1cd0d593, 0x04ef4c33, 0x119140a7, 0xed720a91,
    0x3725303a, 0x125c083b, 0xf5e46c27, 0x45e86085, 0x1ad91d3e, 0x9f54ee77, 0x1e47c449, 0x2ea6c8f7,
    0x11f9cc9e, 0x1116ffeb, 0x00bf7ab9, 0xed45fcd4, 0xbc63b454, 0xd1473ca1, 0x21cde647, 0x76796475,
    0xf172f5bf, 0x2f7f1374, 0x7c610cbe, 0x25970aac, 0x2e878481, 0x9c95a338, 0xce655630, 0xc0094cb1,
    0xb1e343f1, 0x0446fed1, 0x7a4a6b16, 0xa33700cb, 0x143130be, 0xb628af10, 0x35ea55c4, 0xc2d69c64,
    0xa55fd6f7, 0x2870f7b6, 0xcb649081, 0xa006c5db, 0x3947e121, 0x2ffe52a1, 0x81006407, 0x81ccf99f,
    0xc0fa4cce, 0xfc821176, 0x4e0362bb, 0x6bd50529, 0x053e2eaa, 0xa77b5924, 0x427c742a, 0xfda757e7,
    0xe481fc90, 0xd92bf785, 0x9bf7da42, 0x1c2e5bdb, 0xa1cbfe4d, 0xe5fb04cc, 0x795a5430, 0x444c052f,
    0x8bd3dfe8, 0x05cf0337, 0x419052f7, 0xbe477ab8, 0x3e794b8c, 0xb6eabc4a, 0x16bb5d08, 0x3eb938c6,
    0x4c507fbf, 0xefa69556, 0x05d359a7, 0xcd4e5c6b, 0xd8130874, 0xbbc71d4f, 0xa51d2400, 0x7e45e671,
    0xbc8b2af9, 0x268924a8, 0x634f1d40, 0x6dafb478, 0x12bbaa11, 0xbf7431ff, 0x03962cf0, 0x7d07f300,
    0x0dc6e23b, 0xb383fd0e, 0x298f457d, 0xbbd32a6c, 0x80394491, 0x57861cea, 0x09963c1f, 0x433f84a9,
    0xc2db81db, 0xaa325ca1, 0xb70adee4, 0xf380dcdb, 0x5bc947cf, 0x835efbcf, 0x01045835, 0xffc0712b,
    0x083ff025, 0x3dc30b3f, 0x54ff410e, 0x5429967e, 0x1c1c0215, 0xa2303796, 0x76ce4958, 0xda7a01c4,
    0x926df55f, 0x0b2fd9a2, 0x9c9fb176, 0xa2aded08, 0xbda2e097, 0x11c74a95, 0x69b06855, 0x0934adbf,
    0xe2ded4d7, 0x6bccdbf5, 0x51bf74aa, 0x59b7100c, 0x4e754686, 0xbe528b72, 0x1352c6f1, 0xf70dc145,
    0xd0c1c218, 0x3d6d07f3, 0xa279c1c7, 0x2ebbaa0b, 0xe8205bf8, 0x12dcee00, 0xe038793a, 0xf1c8cb8e,
    0xdd364c15, 0x70d57a4d, 0x772fa7bb, 0x5fbebe47, 0xcf9598be, 0x37ba13e3, 0x724c3f26, 0x6b9da667,
    0x63d46dbd, 0xe01582a3, 0x1de95959, 0x2af682c3, 0x578d0581, 0x70b04d96, 0xe06ccbf3, 0x4c3b91d3,
    0x08da13b0, 0x66e63106, 0xb3ad0525, 0x2957b882, 0x7145d16a, 0x6b2347f4, 0xc3307b6e, 0x03350e17,
    0x8d7c078d, 0x33c6ddf2, 0xbab258b7, 0x9690da2e, 0xdc08a242, 0x9cdf4451, 0xeb154844, 0xf69b644c,
    0x47a00359, 0x3368e82a, 0xa034c152, 0xa6b56ecc, 0xab9ee256, 0x4c570265, 0x802daf71, 0xa72bc8fb,
    0x79180be1, 0x8e06db49, 0xa51bf279, 0xbe5e9fe4, 0xd4a604e7, 0x626a2093, 0xc2889757, 0xe32b5cc8,
    0x35acf13b, 0x0f94eee1, 0x460d6b4b, 0x70cceb1a, 0xfbfa46b7, 0xa06d0848, 0x47eb5b54, 0x1c508bd8,
    0x2638455d, 0x4e7b23dd, 0x0bcc7c4c, 0xf3e63b02, 0x56ce7c57, 0x36b429fe, 0x54b20283, 0xb43932fd,
    0x7082ff23, 0xae330b4f, 0xb0390727, 0xc744d9e6, 0x5bc947cf, 0x835efbcf, 0x01045835, 0xffc0712b,
    0xefc6e0fa, 0xb874982c, 0x1a341598, 0x115cbef9, 0x21ef69f0, 0x9579e6d5, 0xbff88e60, 0x4d5d13e1,
    0xbaa4a6cf, 0x7b5b7756, 0x746fbe6f, 0xe53f70bb, 0x8f058a69, 0xce29abaf, 0x6d02fdca, 0x6c78bd53,
    0xa71e25f6, 0xeb246914, 0xc6f67180, 0x27b7634f, 0xd53ae2ac, 0xbaac07df, 0xb709814a, 0x32cc0fc6,
    0xb5552755, 0x647aed94, 0x53cfce1b, 0x919eea9f, 0xd14d02d1, 0x944e38d5, 0x88a48a94, 0x2f6de591,
    0x5161a381, 0xa23a5b85, 0x894439ac, 0x874c170f, 0x216c4aed, 0x11a9452e, 0xc41e8ef4, 0xd54e673b,
    0x653a2df6, 0xae618e53, 0xfb092130, 0x0556afb3, 0x3483f8f1, 0x025ea417, 0xbf36f768, 0x9d91d9c0,
    0x17152afb, 0xfae01200, 0xcfff6e8a, 0x809b2ed9, 0x42f81893, 0xbb76e2da, 0x62c42b55, 0xc7569215,
    0x85e928ce, 0x944367d2, 0xdda980b8, 0x1ba6bbbe, 0x0e41e65d, 0x3d5f6aa2, 0x35cb56ce, 0x8e5215cc,
    0x8503d4c5, 0xe55f520b, 0xb828f841, 0x4c42d87e, 0x6ac85eb7, 0xf1bea200, 0xea3c6545, 0xad322c79,
    0xdb53d358, 0x3a3bf608, 0xa1915f5a, 0xc2374358, 0x24344880, 0x3dbe3302, 0x06bde44c, 0x62144675,
    0x63c1f618, 0x4ac69ec7, 0x19d789c4, 0x37d98f01, 0xf172f5bf, 0x2f7f1374, 0x7c610cbe, 0x25970aac,
    0x31e3e473, 0xf1883c60, 0xdbb6a84c, 0xf69da2f6, 0xd9c5f30f, 0xfeaa39c5, 0x7d74d582, 0x12352289,
    0xbd0eade1, 0x3fb21fc6, 0x02fb676c, 0xbc3b8644, 0x9601bf64, 0x3c719ac9, 0x7cb82fbf, 0xf6efb7c0,
    0x090605a3, 0x3c736ede, 0x55246761, 0xd9e77e16, 0x91168e9b, 0xd16ba324, 0x9301cd5a, 0x9082de32,
    0x1f4eaeed, 0x98ceb904, 0x306de4c5, 0x34654bfb, 0x0ab76e78, 0xa51a1889, 0xf540c6f8, 0xcdb0dbcd,
    0x3431cc28, 0xcbc92d73, 0x14e6fd05, 0x49383115, 0xe4baad23, 0x010034a9, 0x9bb9c79c, 0x42f81cfa,
    0x84e8d3a8, 0x91771a8b, 0xcbc505bd, 0xbcf27650, 0x05aa39fe, 0x430d9a5a, 0x04c25573, 0xbf3f199c,
    0x3efc7b1e, 0x70efab6e, 0x7914b6ca, 0x56e3bad7, 0x8feddb1c, 0x7fc43b42, 0x4f8d2338, 0x5b674d94,
    0x8c445876, 0x2499f020, 0x6c3814bc, 0xb716a408, 0x84e6b290, 0x6d9c9bc0, 0xc345c13f, 0x423ad236,
    0x4294d9d4, 0x10eff12a, 0xc7838fa4, 0xb5533e8a, 0x8e65d770, 0x9d3f9305, 0x4be0302b, 0xc3fbfc31,
    0xc6f8076f, 0x259ead65, 0x77146ed3, 0xd12e52d1, 0x303b5b80, 0xccae9610, 0x22dc877f, 0xbe100991,
    0xd9f9393c, 0x1ce7f9e6, 0xdf679880, 0xb13b1466, 0xff868829, 0x955fdec8, 0xca3b49cb, 0x04680640,
    0xa1128bbb, 0xb134ddcc, 0x76845fb2, 0x72bb57e8, 0x5445ef9c, 0xa88ed074, 0x051fd7db, 0x12ec5f3f,
    0xbb985d15, 0x1938b533, 0x90aedd0d, 0x9b98a2d7, 0xf4d5fa10, 0x90844934, 0x332b0aa9, 0xbcd5344a,
    0x86f2d700, 0x0fba49fe, 0xf7903707, 0x48715d40, 0xba16fb58, 0x6e4c60dc, 0xfa1e6eb7, 0x85afb02a,
    0xa312f7cf, 0xccbee419, 0x87687b91, 0x09de7467, 0x1b860623, 0x6570d2b5, 0x7e4d041a, 0x9093b6af,
    0xc50f1f8a, 0x60ffd48c, 0xcb29f864, 0xa18322c6, 0x63cbb721, 0xae3ada11, 0x9f551fd9, 0xb7cb2c87,
    0x0c94fcac, 0xc375f694, 0xec1d7743, 0x991b8cd2, 0x71a6ec54, 0x849ee79e, 0x408ed3f7, 0x25821af3,
    0x49042868, 0x61aa04eb, 0xc34556ea, 0xd725b889, 0x2e349bd7, 0x595b3051, 0xee4abd36, 0xa903db12,
    0x36fea599, 0x190825d9, 0x4c699b0b, 0x219d4471, 0xbc1f035f, 0x36735df1, 0x0ea302d2, 0x5c81b40e,
    0x36676a2e, 0x19385c4c, 0x0a7df6db, 0xf03a863a, 0x2b5b70ad, 0xa0a81812, 0xc859de29, 0x284285b9,
    0x098787a1, 0x53f0fbb9, 0xfdb3b517, 0xac2df769, 0xc2704352, 0x710a6cd4, 0x883b3c67, 0x722fbb8c,
    0xe5db8517, 0x39715ab7, 0x0446eb85, 0x1a6a59e6, 0xef0321a9, 0xeeb2b251, 0xaefeef89, 0xfd468207,
    0xa8af4a02, 0x362e4f2f, 0x4e2b9784, 0x98487c14, 0xe3caa36a, 0xce45443e, 0x9fbfb7eb, 0x4205e6b6,
    0xb0fcc809, 0xff4c831e, 0x0dafd240, 0xc4110fec, 0x011900f9, 0xd4b44599, 0x7f7eca9e, 0x8bdd1513,
    0xf03e2c26, 0xc6758098, 0x67104e02, 0x83854ff0, 0xa5bc65ed, 0xd6d9a537, 0xbbbcef88, 0x7c58b5da,
    0xc5729d4d, 0xf4a655bc, 0x603f640f, 0x401fe508, 0x1dfedf4c, 0x3d6d8afe, 0x1a6af00a, 0x4f5ad1ce,
    0x7b3534db, 0xae143d14, 0x5f64a97d, 0x47ed5514, 0xcf770ace, 0xad884d52, 0xb64383b8, 0x8b57ea80,
    0xcd0c88a7, 0xf9ae94d9, 0xc7d15f2c, 0xb6a7477c, 0x1b887a29, 0xef3c79a4, 0x9e7f1d97, 0x92aeb020,
    0xb7f99879, 0xdacd0e9f, 0xf0c278da, 0x8a4d84ae, 0x453108dc, 0x58bac03d, 0xd2b456d8, 0xf07a1bf4,
    0x986c096f, 0xb88cece8, 0x974e556c, 0x39b457bf, 0xec169380, 0xcac0f638, 0xa4b3354d, 0x4533f5b5,
    0x4d211da6, 0x67c50dc5, 0xd1f82e6f, 0x73a2fa68, 0xcac04b03, 0xcd2c0660, 0x4581ca03, 0x31e3be2d,
    0x00192e14, 0x5ad40ca4, 0xd0799de5, 0x1d9c7aaf, 0x2313780c, 0x42f4f6d6, 0x4ad7feae, 0x3d7b1d7e,
    0x7d598a52, 0x9a5e61f2, 0x23832831, 0x4b643975, 0x0cb21c13, 0x101dc346, 0xd6137603, 0x036839b5,
    0x24408087, 0x3287574b, 0xd3e21008, 0x5c7b5f73, 0xd14d02d1, 0x944e38d5, 0x88a48a94, 0x2f6de591,
    0xfbaa9f95, 0xd53c6355, 0xcc812d31, 0x06f54ac5, 0x7b750458, 0x28cc33c7, 0x594a01ed, 0x56caf480,
    0x1a0e0f99, 0x0486c153, 0x968a2e03, 0x20ccb17a, 0xfa528bf1, 0xbe7ea6ce, 0xe96b51dc, 0x5a58e518,
    0x8bec3b2e, 0xf75f6ad1, 0x4aa1e422, 0x632fc3f2, 0x2a199eb3, 0x5f1fe921, 0xe0a67994, 0x50ec3d86,
    0x91409ea8, 0x198cdae4, 0xbdaa9824, 0x7f69fc47, 0xe20fe02e, 0x15d265e6, 0xb17b2582, 0x92da2e03,
    0xdd8dc75f, 0x05aae5cc, 0x22c54bbc, 0x6a85bd33, 0x99b78255, 0xf7272a23, 0x1f1b299f, 0x766fc407,
    0x386a44a0, 0x2cb5fd7b, 0x34fe69d7, 0x463f6f3d, 0x2660bbfe, 0xa2ecfecf, 0x81a93aa3, 0x7c577116,
    0x33e95594, 0xa01792fa, 0xaf95dd90, 0xca67fbaa, 0x57a4afee, 0x13bf4d02, 0x1bfca79e, 0xf8f33c67,
    0x36a6dfe2, 0xd4976081, 0x7f985ec0, 0xb6f6076b, 0x6a132301, 0x65c4959f, 0x5e1ea2be, 0x025fe73d,
    0x8feddb1c, 0x7fc43b42, 0x4f8d2338, 0x5b674d94, 0x39cda227, 0x4d2713db, 0x197662b8, 0xb63dfd37,
    0x24bcb1ce, 0xed59fbd2, 0x48cf6872, 0xfc71947d, 0x6bc59f5a, 0xc398dd47, 0x98ae21fe, 0xa2b2f470,
    0xb2c52e6b, 0x4181e990, 0x26ab3306, 0x4e9d8bd4, 0x4b1c670d, 0x2d7bea38, 0x766a629b, 0x6eaad367,
    0x3ea0936e, 0x02fd2802, 0xa5756dab, 0x728a853b, 0xbd9e3340, 0x96f009b7, 0x12dc279f, 0xaac13256,
    0x0e85112d, 0xd3c05047, 0x7efbc500, 0x8a9c6b46, 0x3e2c811e, 0x032173a6, 0xe180162b, 0xd7c760c9,
    0x436c61f4, 0xee6d739c, 0xeca3dff4, 0x5d985573, 0x81b8369c, 0x03e1b03d, 0x2a48bbda, 0xcfa92acc,
    0x5fd87552, 0xd649ba1c, 0x4d3184fc, 0x7bd68332, 0xe908d6eb, 0xea6d6eb7, 0x455ab503, 0x20d21fbc,
    0x0a623898, 0xed4456a7, 0x8f325cc1, 0x6ded620e, 0x1b3eeb64, 0x70f1d3f4, 0xe5b56de5, 0x4e5a9032,
    0x86d9cbd3, 0x77f2ccf9, 0x40c03edc, 0xcd95becb, 0x5f7dac58, 0xac86852f, 0x582366bd, 0x6ee2ffea,
    0x611fe605, 0xd8757bb0, 0xbf77b6f7, 0xdbd3b19c, 0xcb9dbea6, 0xa6f1e334, 0x97c82c7a, 0x1538a6c6,
    0xee8fbfd9, 0x4ce3a6da, 0x3f11d396, 0xeadef9a1, 0xbd7f622a, 0xba47bf5f, 0x3e1a007e, 0x0dc3b14e,
    0x1842766a, 0x5af04d28, 0x517db324, 0x40a7a03d, 0xd14d02d1, 0x944e38d5, 0x88a48a94, 0x2f6de591,
    0x9fecdcd7, 0x24a62571, 0xa5a1d176, 0x28962998, 0xb1395e00, 0x471d05a3, 0xecf90f43, 0x6f6d2bc5,
    0x5266e7c2, 0x4472bb81, 0xd7b6dcda, 0x4966faa1, 0xa15cbd00, 0xfa44dc68, 0xa069cc63, 0xd04cf8d8,
    0x6399fd39, 0xaaa6b03e, 0xa322b0ed, 0xc33f610c, 0x767f6470, 0x2f340d35, 0x56a1bdea, 0x7f98838d,
    0x1ae61c29, 0x4b5741dc, 0x385b8ba8, 0x7b41778c, 0x9413469c, 0xe19a5229, 0x3acf595d, 0x17610213,
    0xf61df2ac, 0x3e5c0c94, 0x58d57a03, 0x340cf98b, 0x47a00359, 0x3368e82a, 0xa034c152, 0xa6b56ecc,
    0x254471bb, 0xe6657fba, 0x1ce77c21, 0x3066b865, 0xd6832153, 0xb7a492bd, 0xa24329a7, 0x4f134889,
    0x1b520068, 0xccb19f78, 0x169b6666, 0xcd58d634, 0x06b9ce98, 0x4c3250a6, 0xaca58425, 0xa2e1a21d,
    0xa6b24d87, 0xf2588578, 0x0139e3aa, 0x73d2d704, 0xb185a863, 0x2b4d6881, 0x58bc21ad, 0xf3bb361a,
    0xd2fab99c, 0x086a7623, 0x3033596f, 0x06edfcfd, 0xe0717a81, 0x29882ed1, 0x6d26706b, 0x915f126f,
    0x8822dcb6, 0x84063752, 0xabfd88aa, 0xdcda97df, 0x4c81b46d, 0x4f6425b5, 0x53f4c5fa, 0xdfe6de17,
    0x858cf977, 0xfcda9e52, 0xd653899e, 0x399d96f9, 0xd8294249, 0xd3f8ff7c, 0xc6fbbc5a, 0xd10021ce,
    0x3ea0936e, 0x02fd2802, 0xa5756dab, 0x728a853b, 0xbd9e3340, 0x96f009b7, 0x12dc279f, 0xaac13256,
    0x64eb57c0, 0x8a3fa871, 0xc284328f, 0xad24e2af, 0xc07695ec, 0x98034c1b, 0x3fc3f655, 0x3b46a084,
    0xa3521570, 0x22887276, 0xc8e3ece6, 0x324cf006, 0xde0b146a, 0xfc693549, 0xec171854, 0xb882ac6a,
    0x67cd3b53, 0xcc6decf2, 0x7e5e475a, 0x1202d0e9, 0x0eb97de3, 0xe9ffaf6c, 0x288a4694, 0x27fc2ea0,
    0x1c16ed72, 0x743abae1, 0x6264e409, 0x7d91f11f, 0x7805b4ce, 0xe8a86dc8, 0xd14f4345, 0xbce7d011,
    0x414a5ffb, 0x67740f3e, 0x33fef389, 0xc0f716c2, 0xd87e73cb, 0x98d78bb5, 0xfc71fa74, 0xc04f4169,
    0xcdea7564, 0xf9242173, 0xfb31d559, 0xe0fb31fc, 0xa8eec5e6, 0x36bd46f1, 0x03a88fcd, 0x9b5aef20,
    0x7979f093, 0x851c1f1c, 0x716e1f5b, 0x638b9587, 0x9237045b, 0xcf8495f9, 0xbb92e023, 0x184264a6,
    0xf1e4cbf7, 0x2ae3f81d, 0x48a22da7, 0x562d3a51, 0xb4a2f546, 0x6aef95a7, 0x36bbe947, 0x16bdc21a,
    0xba91b9b0, 0x7556c405, 0xf25880fa, 0xf5547ca1, 0x53ed106b, 0xbdde7b5f, 0x5a9ede6f, 0xb3eb592d,
    0x04dab04a, 0x11af992f, 0xbcb18304, 0x79d89750, 0x3af2b461, 0xf604b182, 0x080fce37, 0x1eab5a1e,
    0xeb639410, 0x4a4a4909, 0xf1facfac, 0xc07510e9, 0x03a17cb7, 0x767c4205, 0xc3eb4eb2, 0x703dc4fb,
    0x6a3d9789, 0xb00a7d69, 0xb5cc30a7, 0x74dd3496, 0xd29bac87, 0x832daae5, 0x0a9ce5d2, 0x7856a0e8,
    0xdc852d90, 0x7680cea0, 0x03672be5, 0x210cbc44, 0xe645ce7a, 0x27b451cc, 0x2141ec4c, 0x23452f7a,
    0xf3331f13, 0x363875e3, 0x91f9a441, 0x1e92fa1f, 0xe2083a19, 0x4233acc4, 0x1debf03a, 0x92fc3954,
    0xa4729614, 0xc31a6980, 0xc97909b1, 0x7e1755fc, 0xdcda0263, 0xa335948e, 0x06e278ed, 0x1418df15,
    0x8bf2a980, 0x5ea30238, 0xece0be8a, 0x76695239, 0x3455c9af, 0x371c0565, 0x97da4fb0, 0xda44c90c,
    0xf249fb9b, 0x62ee219c, 0xd7c94f88, 0x33503b5b, 0x3e938781, 0xc4c4474c, 0x230d37cd, 0xee33fd4d,
    0x3830c632, 0x764ae5ea, 0xcea96041, 0x263a9f8c, 0xe767b88f, 0x3f21f13a, 0x60b3d6cf, 0xb1b74839,
    0x8c9eb5a1, 0xddbc36ba, 0xf33934b6, 0xe16774b2, 0xab264382, 0xf917b1c3, 0x61db68c6, 0xd9d41726,
    0xc1c6996a, 0xce13ab6c, 0x6054fba9, 0xdcb701e0, 0xd15f7f78, 0x274caf9a, 0xca19fdb1, 0x8e6f955d,
    0x7184ed1d, 0x808784f6, 0xce9e4aae, 0x4cedfe30, 0x791715e9, 0x2031a7b3, 0x14708a46, 0x5b05151c,
    0xf172f5bf, 0x2f7f1374, 0x7c610cbe, 0x25970aac, 0x7439dd64, 0x0aa70259, 0xe19cce41, 0x6619919c,
    0xeb78a0c4, 0x68cf6235, 0xaf964f19, 0x215ef029, 0x608ae3a4, 0xf62e4974, 0xa9248b7e, 0x15117ba2,
    0x8cf3f1fe, 0x36d5c51f, 0xf4427c9a, 0x08405c5e, 0xdfbab057, 0x2542be45, 0x261eedb0, 0x6dc613a5,
    0xd10867fe, 0xcce68b92, 0x4edc501e, 0x4b945475, 0x72e7f6a9, 0x852030d8, 0x2e250df4, 0x0d9919db,
    0x86a652bb, 0x23dea28b, 0x0e06ba72, 0x5c8dc4c9, 0xb7fe875e, 0x681d8645, 0xd60fc0a2, 0x33fad0cc,
    0x237a6f70, 0xf0c996bf, 0xdd86bf1e, 0x9c3091df, 0x6d68d020, 0x1ac4affb, 0xd92b5720, 0xa3f50df1,
    0x91f3ab39, 0x139cd59d, 0x346b9dc4, 0xf2f1f671, 0x062cc6e9, 0xa2231ef7, 0x287838e5, 0xbe9905b6,
    0x1032d3d3, 0x78a50cc8, 0xa7c7be44, 0x0c5e8828, 0x4f084491, 0xaf72d73e, 0x09fcfa2f, 0xeb4e09d5,
    0x67385a51, 0xf14edeec, 0x714e7de4, 0x89af5b42, 0xd6b0fad6, 0xcfb39806, 0x5bf25155, 0xd3df6a29,
    0x3f385aec, 0x28aeecc9, 0x885c524a, 0x7ed45288, 0x7b0a3bf4, 0xbd1eb5f1, 0x7084e6ee, 0x2469633e,
    0xbdf586aa, 0xa4af4faa, 0x0d966583, 0xd1011384, 0xc864c640, 0x4bc318eb, 0x01c67234, 0x119d7db9,
    0x3f26218b, 0x3dc663fc, 0x62d58b7f, 0x4a668ff0, 0x30aa10d2, 0x6ef37be2, 0x17adb268, 0x17f6654b,
    0x12461d88, 0x367e59cd, 0x5ff21b1d, 0x34648811, 0x71124949, 0x8ebc0ffb, 0x5cbe2f97, 0x630f3337,
    0x21f9cf8f, 0x35e8dfb3, 0x1b15c596, 0xb1b2b9b6, 0xbaae4e60, 0xc1b51b61, 0xf8ed4a34, 0xa811d248,
    0xb861c10d, 0x1d0a1997, 0x8a9cdca6, 0x5aa883c6, 0xb1d212b0, 0x1c9d710a, 0x1ec89f47, 0xc3639f84,
    0x668873e6, 0xb56fc9de, 0xd351ead0, 0x4281f3d3, 0x53f9c082, 0xfaed2c96, 0x50275512, 0xeb75d8e7,
    0xa9dcaa96, 0x0708c6d4, 0xab0c5ed3, 0xb6e0f597, 0x3483f8f1, 0x025ea417, 0xbf36f768, 0x9d91d9c0,
    0x4c5945c2, 0x5ebfe660, 0x6990dc73, 0xd71b6f79, 0x53907ac1, 0x7a5b3d86, 0x857928ae, 0xb12b9e12,
    0x88dd2770, 0x513040fc, 0x8ea65788, 0x02972655, 0x7979f093, 0x851c1f1c, 0x716e1f5b, 0x638b9587,
    0x2160d58f, 0x3b9bef1c, 0x07d6dded, 0x0052c33a, 0x5eacc1c9, 0xe15b0b9d, 0xfbaed420, 0xad4d7614,
    0xd0c1c218, 0x3d6d07f3, 0xa279c1c7, 0x2ebbaa0b, 0x5609512b, 0x983fea1b, 0xa1d9ee13, 0x75f22340,
    0xb3fe6966, 0x4ec76785, 0x1a1984ab, 0x4f0f1126, 0x6bfdcb02, 0x3fcca8a3, 0x62b406e0, 0xd6409222,
    0xb4a6b904, 0x358551fd, 0x2e61d936, 0x6ae488eb, 0x717fbfe1, 0x8556e8b4, 0x839a7295, 0x5719644c,
    0xc9a149b3, 0x0b59d604, 0xc5401225, 0x1dff33dc, 0x32373ae9, 0x05175de2, 0x03dbb65b, 0x51eced70,
    0xb24e3f38, 0xed9ada63, 0x097446d9, 0x819d6ac5, 0xd68fc8f9, 0xb91592b7, 0x981e9424, 0x56e317e9,
    0x480724a0, 0x575e5663, 0xfd059905, 0xa19bf09b, 0xfac36a31, 0x8ea79a84, 0x1a75d94b, 0xb6ade50e,
    0xb26b6f86, 0x4f52492e, 0xdfca74bc, 0x5c33e6ff, 0x24b8c226, 0xfdbf928f, 0x8235bcc3, 0x23a7fcd1,
    0xff55919d, 0xb9548607, 0xa7e7d475, 0xa74837c4, 0x75592e47, 0xc75cfb9a, 0x1b8d9c0f, 0xab5017d3,
    0xa8d8ee34, 0x37b4931b, 0xb00e8c7a, 0xd76c65e5, 0x04dab04a, 0x11af992f, 0xbcb18304, 0x79d89750,
    0xaf6d58dd, 0xf5764b1a, 0xd9bc5b79, 0xa49688bb, 0x848206ff, 0x8aed0900, 0x066e52fe, 0xac03df9a,
    0x4101e14c, 0xf76ffcc2, 0x76a937c4, 0x3b60ac3a, 0x7aff318f, 0x830bb3e1, 0xf2de9754, 0xdde263d8,
    0x237a6f70, 0xf0c996bf, 0xdd86bf1e, 0x9c3091df, 0x2e086ab2, 0xeafac53f, 0x55355cdb, 0x67e5cf14,
    0x748b17f2, 0xa40dfcc8, 0xa81e985f, 0x8d18c49f, 0x6027ac8e, 0x0cdcd1ff, 0x5b631f52, 0x3376cfba,
    0x154567ac, 0xf5b0984a, 0xe1818574, 0x48c190aa, 0x36233cc8, 0x88ffee42, 0x395c4e0b, 0xf86a4056,
    0x4124aa4a, 0xbe9a4339, 0x2b03fe7f, 0x96fb174f, 0x96223533, 0x960c1a45, 0x689650c4, 0x90cd4153,
    0x3d8437bd, 0x984417a0, 0xbcbf4105, 0x61ab7e21, 0x055ba29f, 0xc2f59c06, 0x8820bb1a, 0x88f8666b,
    0xfb444cae, 0x185e18e4, 0xe1f7090d, 0x2f8e0cc0, 0xbb26c7c4, 0xc3f6ecf1, 0x0d8da413, 0xdf9b8a0d,
    0xa48dcef5, 0xe5d8bc60, 0x1b9c18f9, 0xa53aa0f7, 0xa33f6aea, 0xa66450ef, 0x66f5213b, 0xb5016bc5,
    0xe6d82a1e, 0xfeb33d86, 0x620cdb51, 0x0d599545, 0xde8dd400, 0x608f6a83, 0x6eb1d1d8, 0xfe979507,
    0x7c884ee0, 0xe14ca6c9, 0x21c9d76a, 0x7b8d6eac, 0xb16293ef, 0x3c611467, 0x7c2125fd, 0xdd665573,
    0xabeb0879, 0x8335207c, 0x0c76e75d, 0xd5e1c895, 0xd776ff00, 0x8992055f, 0x703645e4, 0x0136e37e,
    0xafdca225, 0xc3f1f941, 0x01c9201a, 0x46c158ad, 0xad264186, 0xac24b625, 0x140d2746, 0x5282f188,
    0x653c293c, 0xc8957405, 0x78706740, 0xdd05a7f8, 0x26b6312b, 0xd848c21d, 0x08ebc902, 0xc2c858a3,
    0x7460f961, 0x3e25176f, 0x054c27ec, 0x66875041, 0x99b78255, 0xf7272a23, 0x1f1b299f, 0x766fc407,
    0x386a44a0, 0x2cb5fd7b, 0x34fe69d7, 0x463f6f3d, 0x0e43287b, 0x0b658539, 0x20f8211a, 0x8a461932,
    0xcfd28f5c, 0xd9dcacf8, 0x5fd98901, 0x6f410b83, 0x72a77190, 0x947c0458, 0xde183a66, 0x8aeb3331,
    0x46b303dd, 0x8bc7adfd, 0xf46428eb, 0xb885edcf, 0x4124aa4a, 0xbe9a4339, 0x2b03fe7f, 0x96fb174f,
    0x61d53d8b, 0x21649620, 0xd29baaaa, 0x428d16b1, 0x84e6b290, 0x6d9c9bc0, 0xc345c13f, 0x423ad236,
    0x26fed64b, 0x7d8fdf0a, 0x4354fa37, 0xd1c51fb3, 0x8e65d770, 0x9d3f9305, 0x4be0302b, 0xc3fbfc31,
    0x3f31d857, 0x2761be31, 0xf6be78c0, 0x7ee81918, 0xb3ee57b7, 0x1a780793, 0x3bee6658, 0x159b25db,
    0xd13b7144, 0xb33b2914, 0xcf1c41e4, 0x2abbcf37, 0x2f653a9e, 0x5017e376, 0x6a56e548, 0xa1ca0d79,
    0x94d75a6f, 0x30d2a8e2, 0x74dd6640, 0x07ac2bba, 0x70fd0e1b, 0xa08d6947, 0x9e494523, 0x963a5c5e,
    0xd0af53cb, 0xf06fc05f, 0x2cfca684, 0x513f2a54, 0x513edd8e, 0xe2fce647, 0xcd082a1e, 0x8a17aba4,
    0x3e5f0474, 0x376d442a, 0x776facf1, 0x85d49026, 0xcf637129, 0x6b41cb28, 0x31d2bf12, 0x83968e42,
    0x2d65b27c, 0x0033ecee, 0x0ad10d60, 0x4f8b556a, 0x5187b972, 0xb3d531e8, 0x3b0e572c, 0x21a30306,
    0x9cfd02e2, 0x39ed3d60, 0xe4373c51, 0xbbcbdc09, 0x9b1e61a7, 0x6a0ccee2, 0x07150e5d, 0x8ed2f069,
    0x2fde75b8, 0x815e329f, 0xe9bd0eaf, 0xa5624837, 0x2398a873, 0x94ff0fe6, 0x994aff57, 0x04748ccc,
    0x502c4f27, 0x9b04d973, 0xadadd4bc, 0xaf11b2e2, 0x28adf267, 0xe39c5760, 0x4b780b86, 0x03616f28,
    0x934645b0, 0xc6250d45, 0x28dc13a4, 0xa4302db1, 0x27fcaca8, 0x12ce8b7b, 0xf6355f66, 0xe2dd5e73,
    0xd84d326c, 0xc0a1cbdd, 0x3ffe67f0, 0x3af7baca, 0x08d59e9f, 0xccb14de8, 0x939ff3e8, 0x6fb7c43d,
    0x1a60422b, 0x71d34184, 0xa02b5bfe, 0xe2549202, 0xce1e5559, 0x1a5f32cd, 0xa011c7da, 0xba3a82ba,
    0x6b441b78, 0x7adfc6ce, 0x4ebee970, 0x3237178f, 0xaeaa2ac2, 0x7d0ea5b6, 0x79a6e2a2, 0xbab63dca,
    0xcec71456, 0xa60e6a94, 0x4eae42d9, 0xae939f71, 0xf210be26, 0x3a630120, 0xc6742caa, 0x13633184,
    0x20998d7c, 0xc9510dcf, 0xff92aa24, 0xd52479ca, 0xb63b1334, 0x52cce934, 0xc731f3cf, 0xe5b73eee,
    0x8ff3aa20, 0xb2ca62e9, 0x1ae36e3e, 0x5841edd6, 0x31cf6384, 0x87e09b62, 0xc1e9cabb, 0x76ec5723,
    0x87f017c3, 0x0f4e00a7, 0xc6c5c35e, 0xb40e61d8, 0x0a49329a, 0x3aef5783, 0x892f771b, 0x0b8aaa7a,
    0xfa22813d, 0x8b242ffa, 0xeadb19ce, 0x8b742b75, 0x64f15f11, 0x64d06785, 0x1b5ca07a, 0x40163c9f,
    0xcd8e44f0, 0xfccde9c2, 0xdec55c01, 0xe1a58c12, 0x2616270b, 0x83de0439, 0x2d9b8fbb, 0xb25a8d2e,
    0x0b7aafb2, 0x36912729, 0xba2a16a9, 0x4f8d0b85, 0x6d028217, 0xf0db63d4, 0x62c26b2c, 0x2634d8b2,
    0x0059fb4e, 0x36e24610, 0x824ac6bf, 0x7256b54b, 0x2a8950ae, 0xa412f75e, 0xa478bd2a, 0xdf4e296d,
    0x41ebd5ff, 0xb7c00ef6, 0xaa60e564, 0x549c636c, 0xda688763, 0xe7553898, 0x731cae46, 0x29dcfda6,
    0x044eca3f, 0x4af52006, 0x7b95bf27, 0xa40e1cce, 0xe8ef495e, 0x7d2f3ced, 0x7d62bb6c, 0x842a0c7d,
    0x674d8751, 0xd2f6f6f7, 0x9fc179aa, 0x460955b0, 0x5f327e69, 0x8f67891a, 0x082926d4, 0x29ba9e8a,
    0x62788055, 0xc8e98575, 0x5e3b5050, 0x2f9a73d4, 0x694029b0, 0x5717b996, 0x11fccf82, 0x06f102ab,
    0x16f7fa70, 0x7b9ce479, 0xf0a38969, 0x8c9c6903, 0xa9b052cb, 0x3caa17a4, 0x916e20ff, 0xf15d4f66,
    0x615c8fab, 0x83b33fae, 0x35f034ad, 0xca5949a9, 0x9dff5905, 0xef2c6d31, 0xd2879dc2, 0xc460481a,
    0x47df0173, 0x8525ba9c, 0x8d907439, 0x21928b03, 0xd1b6ee41, 0x17167f09, 0x59418c1d, 0x2cb35777,
    0x2969295d, 0x12f7de8c, 0xb5f7d92b, 0x8f064689, 0x9dfa25f9, 0x3c2f363a, 0x94a23ae0, 0xe6a15963,
    0xc531f316, 0x2f5d64e0, 0xc49796f4, 0x14d7a4b8, 0xd63d2316, 0x15c5a86b, 0xeeb8fbd8, 0x4f4049f3,
    0x6fb99121, 0x12262aab, 0xb0c67037, 0xbe84909e, 0xf8bc1f4b, 0x28e1c4d4, 0xdf9baa7d, 0xef2f89e6,
    0x48d67d5b, 0x3f0d8de5, 0xcc22ed88, 0x572b4a77, 0x607f8627, 0x0e31d505, 0xb2482ca4, 0x5f7a9f1a,
    0x2616270b, 0x83de0439, 0x2d9b8fbb, 0xb25a8d2e, 0xe82c582e, 0xe57ff79d, 0x71ce4830, 0xcd984adf,
    0x5277b118, 0x6c0e44a9, 0x4513e2cc, 0x428f55c9, 0xc0f427df, 0xaf626b45, 0x5fefb076, 0x16093944,
    0x8d96be47, 0x2b336477, 0x185d4da6, 0xbb254378, 0xf08aab27, 0xa6563035, 0x28342dbb, 0xe1203007,
    0x1d787845, 0xe5b6a065, 0x84ee305f, 0x82f45c16, 0x044eca3f, 0x4af52006, 0x7b95bf27, 0xa40e1cce,
    0x4f41dc2e, 0xe7f3d9ec, 0xe960b192, 0xd7af94ec, 0x3841d2da, 0xf9ee1cb1, 0xa08a0331, 0x7b83e540,
    0xe268fba3, 0x407db269, 0xdee8c9c2, 0x32c4233f, 0x157e9732, 0x95bfc462, 0x010853dc, 0x5c33aadb,
    0x694029b0, 0x5717b996, 0x11fccf82, 0x06f102ab, 0x25346623, 0xeb769b9e, 0x31f92c5f, 0xb3b254d9,
    0xa9b052cb, 0x3caa17a4, 0x916e20ff, 0xf15d4f66, 0xf15cffd8, 0x91a6ba5c, 0x5793e83f, 0xfde3a234,
    0x723a2859, 0x23d49107, 0xd131b4c3, 0xf315c978, 0x0f45c990, 0xf0fefb15, 0xa69c9f39, 0x3809353b,
    0x5fb74d40, 0x83b5ce9b, 0x8885722d, 0x03979fe0, 0x893945a5, 0xf5dbf8c4, 0x047404e3, 0xd1e01bd6,
    0x95222053, 0x89fc69f9, 0xacd47c2f, 0x0f92bdb3, 0xc531f316, 0x2f5d64e0, 0xc49796f4, 0x14d7a4b8,
    0xabac1ad6, 0x25ef8410, 0xa7b899de, 0x5a63e5ae, 0x6fb99121, 0x12262aab, 0xb0c67037, 0xbe84909e,
    0xc39fdc9d, 0x4a038b0f, 0xf7ace084, 0xa5f72c7b, 0x9644eac5, 0x7f40e9ec, 0x5b0e7572, 0x9a3ca137,
    0x13308abe, 0xee2f7b05, 0xda065f25, 0xa05dcda1, 0x2616270b, 0x83de0439, 0x2d9b8fbb, 0xb25a8d2e,
    0x2f3b5a83, 0xe1e501eb, 0x95002e3e, 0xfb2c1719, 0x1d9fced5, 0x550565ce, 0xc266f0dc, 0x33d6e228,
    0xc0f427df, 0xaf626b45, 0x5fefb076, 0x16093944, 0x529ea0a5, 0x5a4427de, 0x18dfc8e7, 0x32349b52,
    0xda7f1c95, 0x1c20cb06, 0xddad64ea, 0xed224f6b, 0x9d95abf8, 0xb8b524f9, 0xd5ff9121, 0x4037ef99,
    0x044eca3f, 0x4af52006, 0x7b95bf27, 0xa40e1cce, 0xc2507985, 0x2edf92ec, 0x3aca87f4, 0xefa9f492,
    0xbd7146bc, 0x436c9c4b, 0xf528204d, 0x27754689, 0xec94ebe4, 0x271ad8d0, 0x3df62cbe, 0x729003c0,
    0xfced8523, 0xd58c9208, 0x1fb744e4, 0xb3eabb81, 0x694029b0, 0x5717b996, 0x11fccf82, 0x06f102ab,
    0x010f4784, 0xc8d8b04c, 0x2676527d, 0x97f68dcb, 0xa9b052cb, 0x3caa17a4, 0x916e20ff, 0xf15d4f66,
    0x071daedd, 0xdba9d6e6, 0xd0e6a893, 0xf4499ab3, 0x4d638a9e, 0x6bf13512, 0x443bf11f, 0x263dd6e1,
    0x04fcdc34, 0xec9ce814, 0xa8a1f6ad, 0x32e8d4ee, 0x502e61fd, 0x553c1f6f, 0xa31b0475, 0x0017ca9b,
    0x9f4e67c8, 0xd060ffd6, 0x5093fa23, 0x7f6b7cd9, 0x07c590b1, 0x026d2c1e, 0x5d311fdd, 0x675be16a,
    0x12461d88, 0x367e59cd, 0x5ff21b1d, 0x34648811, 0x9841b985, 0xeb701c6b, 0x895e3c8d, 0x135e3d7a,
    0x347a2b5f, 0xf5c27edf, 0x767f6d9c, 0x0024fe7b, 0x1010122a, 0x998fd502, 0x44b633ce, 0x6318f59c,
    0xc430072a, 0x10efaeb9, 0x8300ed25, 0x6027e68f, 0x3481de22, 0x98acfb9f, 0xedbaaaf7, 0x36afd511,
    0x293d872b, 0xa35991fc, 0xedb06056, 0x648e230b, 0xd0b72992, 0x05a88439, 0xb0b29950, 0xc685ac78,
    0xac07cea1, 0x86274cd8, 0x253a57c2, 0x7c35b71e, 0x5e8fe137, 0x37604b8f, 0x89e409a4, 0x953bfcbe,
    0x96579d7c, 0x0418d7ab, 0xc9b587e5, 0xc9159b46, 0x0c87a90d, 0xd0ba4955, 0x203f6baa, 0xb1ca41dc,
    0x694029b0, 0x5717b996, 0x11fccf82, 0x06f102ab, 0x10a424c7, 0x3d3cf088, 0x3c477639, 0xd1709905,
    0xa9b052cb, 0x3caa17a4, 0x916e20ff, 0xf15d4f66, 0x77cc6bb8, 0xb7a71f94, 0x7eed655a, 0x21bd1aee,
    0x830847a6, 0x0a8288d9, 0xe3b51b6b, 0x1fb75f8b, 0x2c9bc8b4, 0xae08cc4c, 0xda5dad6f, 0x794e62ea,
    0x690b27da, 0x754da8db, 0xff89cc24, 0x52dd3984, 0x06f75ff9, 0x4aae19d8, 0x7ccee83c, 0x666b54a5,
    0xd59822e8, 0x73ff9359, 0x01f646ab, 0xb162d867, 0xc531f316, 0x2f5d64e0, 0xc49796f4, 0x14d7a4b8,
    0x6a4a773d, 0x627e5493, 0xd5491ef7, 0x6d2535fb, 0xd83b40ea, 0x824b7f2e, 0x7a5a7f8d, 0x2ddf6465,
    0x42f7db49, 0x4eef15a3, 0x55bbaf83, 0x7679c23d, 0x61ebaef1, 0x04a18329, 0x8a0442f1, 0xdd1e7712,
    0xbe872a64, 0x99ed0cab, 0x843303d6, 0x4357d45e, 0x2d268bfc, 0xcbd02b8e, 0xdb8f90e2, 0x3dfe3cb8,
    0xd0b72992, 0x05a88439, 0xb0b29950, 0xc685ac78, 0x79b2cc8d, 0xeb9dcd6b, 0x11374214, 0xcbc572bf,
    0x843ad151, 0x8864f2c7, 0x78a26d21, 0x3a963520, 0x42b7b77f, 0x429a0931, 0x0d788069, 0x868056b7,
    0x2a45fc07, 0x76bfa1f9, 0xe55087e2, 0x2ff3f547, 0x508d84d5, 0x40ac0205, 0x5fcdb7dc, 0x28f3e95c,
    0x926df55f, 0x0b2fd9a2, 0x9c9fb176, 0xa2aded08, 0xc1285271, 0x13cbb554, 0xad8441c1, 0x3a8c3501,
    0x3a70bc01, 0x5bbc106e, 0x0eaf4d56, 0x46a96c55, 0x5e65a8bb, 0xb12f4b0d, 0xea5de28d, 0x5f179dcf,
    0xd643ba24, 0x75eb7482, 0x235bb52c, 0x2c378e42, 0xc53839ea, 0x5c2aa9c5, 0x5e55a68e, 0x1130ff59,
    0x27f18cfa, 0xcb9dbe3d, 0x2e6551bb, 0x9d11490c, 0x31dc95d3, 0xe0fc52ab, 0xd7e4ab91, 0xe8ab98dd,
    0xc531f316, 0x2f5d64e0, 0xc49796f4, 0x14d7a4b8, 0x11dfd546, 0x3e0a6828, 0x963b4597, 0x2a23d769,
    0x6fb99121, 0x12262aab, 0xb0c67037, 0xbe84909e, 0x9cf52110, 0x1539406b, 0xe5877596, 0x7f46f29a,
    0x6ecbf876, 0x015c338f, 0x1c8b907b, 0x2ffe13cd, 0x8023463d, 0x39f886a5, 0x7a4d8eb8, 0x0c5abbca,
    0x2616270b, 0x83de0439, 0x2d9b8fbb, 0xb25a8d2e, 0x42a737a5, 0x46e7279b, 0x0d6af56a, 0x2d0c78d6,
    0x296e7933, 0x36685696, 0x37827d28, 0x0aa08d8b, 0x76699ffb, 0x15d7785d, 0x72f76aff, 0x47bdcf0b,
    0xeabe2fbb, 0x4e008e04, 0x08f2d9a2, 0x9a15054d, 0xbce49613, 0x3ac1cd28, 0x8deaf913, 0xb1b2dd58,
    0x8b2dad91, 0x52ebc2d2, 0x725ecab3, 0xf303d70f, 0x926df55f, 0x0b2fd9a2, 0x9c9fb176, 0xa2aded08,
    0xc1285271, 0x13cbb554, 0xad8441c1, 0x3a8c3501, 0xf4687087, 0x0ae55a1a, 0x4afd3ca0, 0xe405cc77,
    0x447f372c, 0x4ccb5bb8, 0x469affff, 0x36e8516c, 0x9c1dabd4, 0xe399a278, 0xcfc39641, 0xd480e678,
    0x9a19baa8, 0xc50fbb21, 0x9ccc01be, 0x602370c6, 0x6cb3e565, 0x2465bfc2, 0x6d1d1768, 0xa9e12ee8,
    0x769818e4, 0x62f591b4, 0xe951c0eb, 0x2ec4ada5, 0x76edca51, 0xa5b9f21b, 0xc66e3005, 0xfc5cf259,
    0xab7baa25, 0x5f7b9f0d, 0x55d09b27, 0x07c3f6f7, 0xb9aa17f7, 0xe412a214, 0xf481a8f8, 0x63be6e09,
    0xe3e92fc5, 0xa6cc0726, 0xa52580be, 0xf5cbcdc1, 0x766c0614, 0xe6ac790f, 0xa6411204, 0xf862456d,
    0x4a3a0b7e, 0x7c6fdff3, 0x47c992f2, 0xe876e67f, 0x2616270b, 0x83de0439, 0x2d9b8fbb, 0xb25a8d2e,
    0xae0940b6, 0x0726d513, 0x9fffc8a0, 0x2299674f, 0x845b7856, 0x23e273af, 0x25fbb6ef, 0x0acc1ba0,
    0x76699ffb, 0x15d7785d, 0x72f76aff, 0x47bdcf0b, 0xefa1b83e, 0xbca23a48, 0x1c821d2a, 0x6ca05b8b,
    0xa4e28abc, 0xc1f7205b, 0xd7d0622c, 0x178dda1d, 0x694029b0, 0x5717b996, 0x11fccf82, 0x06f102ab,
    0xcd211e8d, 0xfee27c75, 0xe1c6a56b, 0x751af089, 0xa9b052cb, 0x3caa17a4, 0x916e20ff, 0xf15d4f66,
    0x63b449c4, 0x59a77780, 0x4b98224f, 0x47a5435f, 0x046f6b01, 0x2425db8d, 0xdbe65388, 0x439c2ded,
    0x66bd1ad2, 0x45ecdfb7, 0xd078754c, 0xd0781062, 0xd0c411e1, 0x98964ccd, 0x5e224de7, 0xaf62339b,
    0xe00e7208, 0x207cf8d1, 0x434ce934, 0xc293864b, 0x1d608639, 0x69a354d2, 0x3a7e8d9c, 0x8014635a,
    0xc1c6996a, 0xce13ab6c, 0x6054fba9, 0xdcb701e0, 0x0ebf900e, 0xc0bcabe8, 0x656bf2a1, 0x6a02b557,
    0xec277322, 0xd68a42f5, 0x1194d988, 0xeea1b493, 0x17e7de9a, 0x2e8d32f6, 0x93d4f62b, 0x65af4c11,
    0x751b5ec8, 0xb88badce, 0x0aa3a4d2, 0x61a813d2, 0x96289fe8, 0x5a17d2c7, 0x626a95bb, 0xc956c3e8,
    0xa858bdbc, 0x4fee7828, 0xe3efc078, 0x293a3018, 0x926df55f, 0x0b2fd9a2, 0x9c9fb176, 0xa2aded08,
    0xd463f400, 0x2642aff2, 0x6b56c4a4, 0x307493b5, 0xa8a2fa03, 0x75c6e2b9, 0x430a5c6d, 0x3b69d44e,
    0x5e65a8bb, 0xb12f4b0d, 0xea5de28d, 0x5f179dcf, 0xc2506a0c, 0x39288284, 0xea8f6551, 0x08a6f064,
    0xb14b2622, 0x9a09f17b, 0xcf2aac8b, 0xadb61a64, 0x3fb97526, 0x90b72057, 0x030d5830, 0x6406e9e7,
    0xe08cc5bc, 0xd4131847, 0xe62db58f, 0xe0c64bb9, 0x618e2481, 0x6f43261c, 0xd3ebca24, 0xe15b77af,
    0x0c589ef1, 0xf6c600cc, 0xdc689646, 0x958a606a, 0xa8c5ed10, 0xc19a8d41, 0x96f486b4, 0xb6904160,
    0xd16a7dfa, 0xf07fb55b, 0x34952e08, 0xe5cb4370, 0x71d929ef, 0x374675a4, 0xd002d5b8, 0x13ab410f,
    0x335794cd, 0xd62197f9, 0x1007b3f3, 0x15286cb4, 0xc9da27c3, 0x2fb0e0f0, 0x078b8d24, 0xd2437039,
    0xb4a4df7e, 0xd4f381a7, 0x1775f32d, 0xed9f34d2, 0x7247944c, 0xe4935a80, 0xeefd3868, 0xe77fff80,
    0x32362994, 0x8f6d807d, 0x7730f591, 0xe563fae5, 0x292c0d8e, 0x588f1ede, 0xa0d604e2, 0x50e41f35,
    0xebcf08f2, 0x23ab63f3, 0x11d00741, 0x657693ec, 0xc7c01e82, 0x3aa2a40f, 0x8eaecce7, 0x3a887935,
    0xd85f8458, 0x6c13cb7f, 0x6db232ee, 0x859784e8, 0xf59c388d, 0xa9ddf73a, 0x2a4aabd0, 0xebf13e80,
    0xf65c4253, 0x4dac9b3c, 0xdd707250, 0x1223d396, 0x1620deca, 0x130bd278, 0x00f6f4d1, 0x54be6b5e,
    0x3b7cda3c, 0x166dfeff, 0x9767bc5b, 0x01acf8ab, 0x61d5e2fc, 0x01d50b4c, 0x9bde1f6d, 0xa2acb160,
    0xe28c684e, 0x1e2d10e4, 0xf043f16f, 0x3ac21d9f, 0x348eb23e, 0xc8f6e230, 0x96676bb1, 0x4f948b5d,
    0x1ec66ad6, 0x9dc039eb, 0x33f257f2, 0x6c529283, 0xff4eac62, 0x0ec48e32, 0x29176a14, 0xd21d44f3,
    0x8aa28427, 0x93462f8a, 0xd0978354, 0x37641c58, 0x17632099, 0xff280683, 0x983b9225, 0x898c4728,
    0xbf63ee06, 0xe3cda051, 0x05d9b824, 0xf4b19f84, 0xc5bc2baa, 0xd76dfb04, 0x9f064d79, 0x9236c53e,
    0x2ddd267a, 0x7a18ec0d, 0x6ac882c6, 0xfe2f749c, 0xb2aaeff0, 0x8185282c, 0xb40efb61, 0xe37b3652,
    0xdc738fa9, 0x422dd009, 0xb01307e7, 0x4ff67d3c, 0x3ea0936e, 0x02fd2802, 0xa5756dab, 0x728a853b,
    0x65291b09, 0x0daf60a0, 0xe26ae898, 0x2d0a6d92, 0x158b95a7, 0x8687e22e, 0x22bdc0cb, 0x9133f72c,
    0xceb69e52, 0xaead183e, 0xaaf04b96, 0x40bc2794, 0x83db818f, 0x9e516b33, 0x59f8cdda, 0xe2c6afd2,
    0x541ea262, 0x99c2719b, 0x990da7c4, 0xacad9c17, 0x05c6c601, 0xf483419a, 0x6e82f809, 0x2edba549,
    0x19ff1e66, 0x84f46e8a, 0xe7dfcae3, 0xd0f386f9, 0xf960c786, 0x5a622f92, 0x210f9cf1, 0xe68fc853,
    0xe987d61f, 0xe3a2ab15, 0xa9e1d9c1, 0xd3c76963, 0x060d6375, 0xe2547251, 0x950c1764, 0x1d4fe23c,
    0xa0e49be5, 0xfbe25293, 0x835ac4e2, 0xb6945f5f, 0x19b6a4d6, 0x31992d18, 0x38a64e3f, 0x34ad2280,
    0xd74d9c38, 0x8eecaabc, 0xb79baae1, 0xa149d811, 0x8aa28427, 0x93462f8a, 0xd0978354, 0x37641c58,
    0x6af87c64, 0x8ae39ae2, 0xe55596b8, 0x3d66a27e, 0xe28343e0, 0xd0a57f60, 0xf453be11, 0x7138b758,
    0x19e3f472, 0x3c6f6644, 0x731a9eec, 0xdfab9035, 0x5f65e54f, 0xd41a2c84, 0x1de33b16, 0x7495c8fa,
    0x17e7de9a, 0x2e8d32f6, 0x93d4f62b, 0x65af4c11, 0xaf3f2017, 0xfa1babfd, 0x56296bce, 0x9611763a,
    0x99b78255, 0xf7272a23, 0x1f1b299f, 0x766fc407, 0x502c46ab, 0x2b8fd0ce, 0xbea7721b, 0x229bc505,
    0xd38d313f, 0x822ed416, 0x44585346, 0x2d073f47, 0x15291235, 0xedd11539, 0x42614fb8, 0x47e2c03c,
    0x05772416, 0xf1bb8db9, 0x9634e68c, 0x8689fae7, 0x447f372c, 0x4ccb5bb8, 0x469affff, 0x36e8516c,
    0x6faf2967, 0xd30d11bc, 0xca12f596, 0xb9e78c21, 0xe645ce7a, 0x27b451cc, 0x2141ec4c, 0x23452f7a,
    0xfa5ee1e5, 0x26cdda59, 0x2cfbe54b, 0x259c59a9, 0xd851f4eb, 0xaff855eb, 0x167ee6bf, 0x60aff485,
    0x14fab0f5, 0x3af0afe3, 0x4c055688, 0x6d6c66a3, 0x5a0fb0af, 0x69b30043, 0xf38104da, 0xa861b62e,
    0x63c1228a, 0xd7db00b6, 0x64d02838, 0x5a3dc1ea, 0x88287cd1, 0x2268e874, 0xd8318269, 0x82457f4e,
    0x012d8c7b, 0x3a02091e, 0xcd01f565, 0x4862a47f, 0x6914f354, 0x48c4e3b9, 0x9d093923, 0xae457743,
    0x46909a4e, 0x7b719311, 0x7f56c6b1, 0xd77980fa, 0x6e69a0a6, 0x5ac6995e, 0xc7704ccd, 0xcfda61ca,
    0xb840d4f5, 0xb124c878, 0x2a2c8346, 0x332beedb, 0x926df55f, 0x0b2fd9a2, 0x9c9fb176, 0xa2aded08,
    0xd463f400, 0x2642aff2, 0x6b56c4a4, 0x307493b5, 0xfa9f0554, 0x1cec2e94, 0x5dda1667, 0xa1201e9e,
    0x5e65a8bb, 0xb12f4b0d, 0xea5de28d, 0x5f179dcf, 0x968d4834, 0x9cb6dca2, 0x6fa8b0dd, 0x193ac92d,
    0xf6d40ee8, 0x4b9cd0f3, 0x98c11dd5, 0xbc4f6e98, 0xd73f7a83, 0xdbb79d57, 0x15f5af23, 0x14e04f8f,
    0x0928e89e, 0x5b47315a, 0x6d6a395b, 0xd3c30b8b, 0xf72cc904, 0xf685a5b4, 0x105fab7e, 0xec3a6489,
    0x83fd6565, 0xc65ba732, 0x912d4514, 0x3b8fa639, 0x2f5432bb, 0x0ad32e66, 0x574884ec, 0x3ec6c2bc,
    0xedf1acbf, 0x576ea091, 0xa9234a3f, 0xa0e5eddd, 0xc3e55e70, 0x2919175d, 0xd68c3b79, 0xceb1c426,
    0x53321174, 0xe877ef2c, 0x48683d0f, 0x52e0f7bf, 0x86ec8f02, 0x1063380e, 0xae6f786b, 0xbdfb4e87,
    0x7c0c2615, 0x2cd9717f, 0xd6afbd0a, 0xacbfdb26, 0x79626f0a, 0x1e11314f, 0x333acc2a, 0x1c33a85d,
    0x20b7d2f1, 0x8a494044, 0xf3637904, 0x571d9a11, 0x3c2151e2, 0x884f59f0, 0xea140b86, 0x548209e0,
    0x96663ea7, 0xe29689a7, 0x68240e24, 0xa2fd0754, 0x1a17a9c3, 0x8ca3134e, 0x937bec23, 0xbd6bd9f2,
    0x926df55f, 0x0b2fd9a2, 0x9c9fb176, 0xa2aded08, 0xd463f400, 0x2642aff2, 0x6b56c4a4, 0x307493b5,
    0x70f2bb42, 0xa311b876, 0x5b0eedcc, 0xd0e5e2f3, 0x447f372c, 0x4ccb5bb8, 0x469affff, 0x36e8516c,
    0xe00540fb, 0x1d213af8, 0x14b23f82, 0x72bf457f, 0x7433fc7b, 0xe6686a0e, 0xc8d43155, 0xdcd34510,
    0x9e568feb, 0xa219d715, 0x75e28606, 0x2e004688, 0xa48dcef5, 0xe5d8bc60, 0x1b9c18f9, 0xa53aa0f7,
    0x3e960a0f, 0x4456d1ff, 0x462da5b2, 0x2d3a2593, 0xad2bb20a, 0x7a1cf17a, 0xc8485518, 0x38651f4c,
    0x4a47fc6e, 0xd22417b7, 0x41a35bde, 0xcb91a3ed, 0x6148adb5, 0x93e98824, 0x58860dd5, 0xe3e87dcf,
    0x99900666, 0x608e7ea0, 0x32fa96b8, 0xc4053ebe, 0x957f0fb6, 0x88409a49, 0x082bf5fe, 0x7bd5e0ef,
    0xc1c6996a, 0xce13ab6c, 0x6054fba9, 0xdcb701e0, 0x3b8ef659, 0x60fd6957, 0xbde09647, 0x027de70e,
    0x27236906, 0x96f67379, 0x44684bed, 0x5c3a4302, 0x17e7de9a, 0x2e8d32f6, 0x93d4f62b, 0x65af4c11,
    0xc1aab57a, 0xf501aa96, 0xe41ce9ea, 0x0f3d9324, 0x3031d271, 0xd324e150, 0x45b323a1, 0xe37e2ae1,
    0x68a5aa34, 0x8fa1dd30, 0x3a430f11, 0xa4d54bc9, 0x63b2d37a, 0x41af7c8c, 0x98900417, 0x6bd9d32e,
    0xf982a442, 0x774f4b8b, 0x89147d0c, 0x4b3a7cc3, 0xf133d383, 0x30c5bc25, 0x407fe310, 0x57aac9e2,
    0xbd244f5a, 0x42107644, 0x2e2cba0c, 0x4b4ac0e6, 0x447f372c, 0x4ccb5bb8, 0x469affff, 0x36e8516c,
    0x4581773f, 0x06bba39c, 0xf2a010a1, 0x09e0a143, 0xe645ce7a, 0x27b451cc, 0x2141ec4c, 0x23452f7a,
    0x6a9c18fe, 0x21cd1972, 0x2cd65194, 0xeedd3827, 0xbbd4bc7d, 0x3a006c18, 0x68c2ab4e, 0xabfcf18d,
    0x9eb6923a, 0x0b9ee6bd, 0xc99f639c, 0xda73a965, 0x2383f709, 0xed714f1e, 0xc73a1ed8, 0xa0f9d408,
    0x77ca11ea, 0x6c1f9041, 0x520e9039, 0xf413269e, 0xd21d81fa, 0x8cfc5b8d, 0xd5be2f8a, 0x7a516617,
    0x237a6f70, 0xf0c996bf, 0xdd86bf1e, 0x9c3091df, 0x79ef963e, 0x42f11bf4, 0x675fb4ab, 0x54e4be8d,
    0x33013ce2, 0x315ef75c, 0x39218d4c, 0x1ecae19b, 0x55657e3a, 0xd604cb3b, 0x859f607d, 0xbd18d6ac,
    0x405f400d, 0x8f99908d, 0xcba75501, 0x4801eb5d, 0xd9d1def5, 0xd02e5a5c, 0xed21529f, 0xcfd7cf9b,
    0x7d97355c, 0xcad0fb10, 0xd44b87a2, 0x17d80a89, 0x99028f49, 0xeffedc6d, 0xdd399e70, 0x6b725208,
    0xc4e1c667, 0x6307e947, 0xcb58813a, 0x0ac7afad, 0xaafddc0b, 0xb6718dfa, 0x30f6ce84, 0x44947a8f,
    0x04ebe956, 0xca5ea389, 0x453ab00e, 0x3c06476a, 0x09d86b2c, 0xb5d13678, 0x97323d79, 0x84d47153,
    0x71cb7f77, 0x820c77c3, 0xf6d921fc, 0x564ac2e4, 0x04211c51, 0xf471c8d7, 0x5751a6e0, 0xd2916def,
    0xd1eb3dc3, 0xc2017bdc, 0x24c42abe, 0x3c1c96d4, 0xe1ee5c70, 0x7a2b6a1c, 0x0492ce28, 0xfbb2168c,
    0x91e782e0, 0xcfcf0852, 0x9cddc05f, 0x92ebf6bc, 0x42ecd53c, 0x826922b2, 0x89274796, 0xdaafefbb,
    0x713ba538, 0xefab2952, 0x9914a015, 0xf0c51e41, 0xc1c6996a, 0xce13ab6c, 0x6054fba9, 0xdcb701e0,
    0xe40bf04c, 0x6fbce20d, 0x4bc915b1, 0x9571b784, 0x20f1d314, 0x31625f8f, 0x35ca3b13, 0xac94967f,
    0x5aaec32e, 0x5a9e25aa, 0x5958aa23, 0xa7ef7dc0, 0xca1cf901, 0xb4aa7b22, 0xb5414b03, 0x6955282c,
    0xa13b362d, 0x26e65fb7, 0x4eb9ead6, 0x0231fc3e, 0x9f4e67c8, 0xd060ffd6, 0x5093fa23, 0x7f6b7cd9,
    0x269c090e, 0xb5c001b6, 0x47969a93, 0x7763edc7, 0x12461d88, 0x367e59cd, 0x5ff21b1d, 0x34648811,
    0x4a4c9062, 0x2d80c3a3, 0x26eb5318, 0xe39dac97, 0x0a440030, 0xe0382f9a, 0xf28f73a6, 0x62358482,
    0x9530dd53, 0x8f2ffcea, 0x412a9289, 0xe5f28d7f, 0xd0692281, 0x6c76ac9b, 0x10dc7a96, 0x81fc749c,
    0xddd70aa8, 0xa6583027, 0x11ec1728, 0xccd72c13, 0x7da26574, 0x0f365b8e, 0xb51ddcc8, 0x59871a51,
    0x851474c4, 0x25778f8c, 0x30c1f10a, 0x6f3b9f58, 0x4a65800a, 0xf920df0b, 0xe7751d65, 0x44d0b7a4,
    0x904ae4d9, 0xd17f6500, 0x23848649, 0x7d406048, 0x524db958, 0x560a19b6, 0x498952ea, 0x32a4474e,
    0x926df55f, 0x0b2fd9a2, 0x9c9fb176, 0xa2aded08, 0xc1285271, 0x13cbb554, 0xad8441c1, 0x3a8c3501,
    0x37053482, 0x3e0808bb, 0xe0629cf6, 0x5ed98d4e, 0x5e65a8bb, 0xb12f4b0d, 0xea5de28d, 0x5f179dcf,
    0x82b5153c, 0x290f13de, 0x0a9dc09a, 0x83119ecc, 0x3725303a, 0x125c083b, 0xf5e46c27, 0x45e86085,
    0x8d862460, 0x1e8734d6, 0x79147424, 0x1be40bcf, 0xc896f44f, 0x0a6cbbf3, 0x944ca227, 0x975137e1,
    0xd660c96e, 0x1c1a3e9e, 0x6a7ad1a0, 0xf3d187ba, 0x89f17dbb, 0xf54777c8, 0xe0a8efe7, 0x24e8f90a,
    0xd0b72992, 0x05a88439, 0xb0b29950, 0xc685ac78, 0xfd4aef88, 0xe63ef6ff, 0xabab239a, 0x7a43634c,
    0xb28d4646, 0x2fe6d899, 0xee2c9f49, 0x41af43cc, 0xa06dcf37, 0x3c1314a4, 0x7eeae5d3, 0x5a7a8381,
    0x067803fa, 0x6e6a1e29, 0x790e4a45, 0x7c14be40, 0xaafddc0b, 0xb6718dfa, 0x30f6ce84, 0x44947a8f,
    0x7baf2276, 0x04b4feaf, 0x54123c45, 0x24918df9, 0x578fac84, 0x1dc7b2c7, 0x09620e82, 0x8984f644,
    0xe1feddc2, 0xd01046c9, 0x4b899ef2, 0x496a4e8a, 0x40df11f4, 0x6463cda7, 0x20b290ef, 0x9abdd74c,
    0x78a9e681, 0x2e7390f2, 0x4b268c0d, 0x5dad2f62, 0x620e7b31, 0x3484c210, 0x313b7d62, 0x05375746,
    0x6660f4fd, 0xd805d2f1, 0x07810fc2, 0x16ae3672, 0x97b9a272, 0x70e8b0db, 0x3018f5ed, 0x4d57cb0f,
    0x2616270b, 0x83de0439, 0x2d9b8fbb, 0xb25a8d2e, 0xc767c0ac, 0xf4289e0a, 0x9a6ab7a6, 0xcbdb6355,
    0xdefee17e, 0x71b25d0f, 0x2202fe05, 0xc36a0e7e, 0x76699ffb, 0x15d7785d, 0x72f76aff, 0x47bdcf0b,
    0xb99ca718, 0x205da3db, 0x70b282a2, 0xbcb55c6b, 0xcd510a82, 0xc4dbb4c9, 0x4d7e003c, 0x985134e9,
    0xa5b5c48d, 0x4b51fcd9, 0x64e19d74, 0x8552e424, 0x926df55f, 0x0b2fd9a2, 0x9c9fb176, 0xa2aded08,
    0xc1285271, 0x13cbb554, 0xad8441c1, 0x3a8c3501, 0x6e9cb347, 0x9ff269ed, 0x5b701b69, 0x05448b2f,
    0x5e65a8bb, 0xb12f4b0d, 0xea5de28d, 0x5f179dcf, 0xccfbb35a, 0x2c05875d, 0xb82977a8, 0x5baa5db2,
    0x574be898, 0x9c613dc0, 0xcb0623df, 0x64a57c03, 0xd834941d, 0xdfd8b1aa, 0x4e148281, 0x172a4ecd,
    0x769818e4, 0x62f591b4, 0xe951c0eb, 0x2ec4ada5, 0xb1102937, 0x729b4fb7, 0x5cacac0e, 0x0a31e6bd,
    0x80e103fa, 0xbcbf7b91, 0xa84c27af, 0xdf59ad31, 0xda6f7b11, 0x4cc7a179, 0x246e5361, 0x33c1b6d9,
    0x1b1396d0, 0x11c309ad, 0xefe3d0ca, 0x4bbcf466, 0x9f4e67c8, 0xd060ffd6, 0x5093fa23, 0x7f6b7cd9,
    0x6c2299aa, 0x42b1b779, 0x0aab37a0, 0x4b980747, 0x12461d88, 0x367e59cd, 0x5ff21b1d, 0x34648811,
    0xc740d213, 0x2322a648, 0x337fcdac, 0xbf2496d5, 0xa27f643c, 0x2a95cda3, 0xef5468e2, 0xd373a1a5,
    0x4ce7346d, 0x321f379d, 0xfcaa2600, 0xf42bb2d4, 0x8f011458, 0x004df5b0, 0x06ea75b8, 0x937b7461,
    0x3230102e, 0xcb7311c9, 0x221c82dc, 0xd49ce274, 0x0783fcb2, 0x08f81879, 0x662d643e, 0x5ae6861e,
    0xd0b72992, 0x05a88439, 0xb0b29950, 0xc685ac78, 0xd8ce4915, 0xf5dc7d41, 0x03613652, 0x1bc33db3,
    0x84451ad2, 0x3d1ea6d5, 0x7c6cd337, 0xb3d4286c, 0x41e1b769, 0x842c6fab, 0x94a48e13, 0xe99f223b,
    0x5a218e8e, 0xfc37f2cb, 0xdc01b6a6, 0x8ab5772e, 0x139e38d1, 0x375ba7b2, 0xc90869a5, 0x826a88e9,
    0x7820f68c, 0xe307deae, 0xdae609c9, 0x40203e81, 0xfde56926, 0x7cce75d2, 0x6e0a5685, 0xfa964007,
    0x5a0f8278, 0xec8d8c9f, 0xfd0d2cd6, 0x72162d19, 0x490c3990, 0x4638187b, 0x3e363956, 0xe6da4641,
    0xadcfd916, 0xcf1afece, 0xa79f8850, 0xe58b4cc9, 0x7c487066, 0x76a1ff2b, 0x7688090e, 0x127ed39b,
    0x0c9ac7ff, 0x7768946a, 0x166bed73, 0xe21f33cd, 0x8aa28427, 0x93462f8a, 0xd0978354, 0x37641c58,
    0x0b8bb057, 0xc5a3098f, 0xac39209f, 0x46f83223, 0x1113a109, 0x65c28b2b, 0x6fc130cf, 0x283d1e4f,
    0x5afc3fd2, 0x30e61663, 0xf0c41e40, 0xc2191a0c, 0x52f3a812, 0x6bfbe4cd, 0x4d6c76be, 0x734bffff,
    0x2e54735c, 0x7fbf28b7, 0x40a46944, 0xae27466e, 0x0efa800b, 0xf62aea17, 0xcddd7816, 0x3a765c2a,
    0x0f70e968, 0x671dec5d, 0x9e96daa1, 0x0da85028, 0xfd783027, 0x8d8eddc8, 0xacb9fdb6, 0xe2534f76,
    0x8ab63d1b, 0xe5016349, 0xffac7677, 0xfdbd0dce, 0x2ae1f4ef, 0xbc28093a, 0x694e6df5, 0xfda25b0d,
    0x36f442a6, 0x48e39788, 0xb9817f81, 0x669d84a3, 0xd2a84763, 0x768c510f, 0xd2fca3fa, 0x3395ca9b,
    0xe3f9362f, 0x56f795a7, 0xbd7353b7, 0xf814a2eb, 0x7d55ab24, 0xfad293f3, 0x8830ced8, 0x0401abba,
    0xd8cdc4b1, 0x78591797, 0xa4f7890f, 0x33c63fd5, 0xbd95aa5d, 0xee744da7, 0xb74e7150, 0x99fb282b,
    0x4eb6eea6, 0x5e6a3273, 0xce377aff, 0x4cff8f6f, 0x60d675f7, 0x02918d66, 0xe4c65bc2, 0x97382a0a,
    0x1e3eeeb3, 0xb258716a, 0x6328212b, 0x70a4dab0, 0x3725303a, 0x125c083b, 0xf5e46c27, 0x45e86085,
    0x56e972d1, 0x7c819139, 0x827a236a, 0x096bf6d5, 0xc8314d86, 0xdd13b0c6, 0x410ca57a, 0x30fe5109,
    0x7c5c112d, 0x09b16fd1, 0x92467cc6, 0xbc5588ec, 0xa4ff7138, 0x48d8e9bb, 0xa5da29ac, 0x8141024d,
    0x883f471e, 0x7d9a7a9b, 0x5efee6cd, 0x26f9192b, 0x47a00359, 0x3368e82a, 0xa034c152, 0xa6b56ecc,
    0x77b291cd, 0x8833a692, 0x5682ec5f, 0x0a7138fb, 0xbe78bee7, 0x51e74268, 0x70ff55d5, 0xd6238215,
    0x1e9eeb9a, 0xe9ffff76, 0xc1c61528, 0x0e1b701d, 0xec08d72c, 0xe859a923, 0x713b6459, 0xd0896d39,
    0x2885d34b, 0x78fc1101, 0xfaf5a5bf, 0x63f73fba, 0x54caa60e, 0x31f8ece9, 0x0178c661, 0x21e7d1b9,
    0x275692ed, 0x5743dda8, 0x670ecc89, 0x1874f6d0, 0x3aafad21, 0x53e66ad7, 0x1e34c1c7, 0xa6fead36,
    0x8bfd1d63, 0xa60c387c, 0x93e484ef, 0xedf3197f, 0x5547159d, 0x5c1a4970, 0x43cda79e, 0x3abd7f32,
    0xf9dc802f, 0x67a45d8a, 0xa5963cdf, 0xbd3cccd5, 0x478e0db0, 0xbc46eb8e, 0x5448c2b9, 0xfd4371b9,
    0xabe34454, 0x276e1fd4, 0x425d584b, 0x8214816f, 0x7c0c2615, 0x2cd9717f, 0xd6afbd0a, 0xacbfdb26,
    0x86b47b4c, 0xdbd9cb50, 0x16922b86, 0xd95241c5, 0x8c465823, 0x3589084a, 0x74437ba4, 0x74f1a37c,
    0x5b984a8b, 0x9a77e241, 0xa5cbd1d8, 0x8878bb5e, 0x475a8a23, 0x3a1a2e28, 0x3e0e18b4, 0xa52a06a7,
    0x121436c0, 0x90a3ee1c, 0xd5b2a269, 0xdb08fbe9, 0x07df386e, 0x33dd406a, 0xbad87a2d, 0xf095b857,
    0x3a6b63b3, 0x4db8d6c1, 0xbdceda5b, 0x5cd30a39, 0x2fd7de7c, 0xcb088264, 0xb893f0d1, 0x3d5a1404,
    0x2c1f7972, 0x6f4816e1, 0xea09a347, 0xd4f439d4, 0x8e3a0a51, 0xa6a29424, 0xdc28d231, 0x0adf23c8,
    0x142e5c5d, 0xdf2645d1, 0x7ae80255, 0x263f68c7, 0xdf910a58, 0xaefb87d1, 0xfbcbc56c, 0x32fed2db,
    0xfb5cd596, 0xd6688e51, 0x1a2aa445, 0xc6b480ae, 0xf0219125, 0xad6c9718, 0xe8b3d1e7, 0xc5bece92,
    0x98d1cba0, 0x64ba5447, 0x10b2271d, 0xe939c65b, 0xdf6f458d, 0xa36cde1b, 0xd93f040b, 0x189289c8,
    0xd2ab2646, 0x572e481b, 0x57912af9, 0x9f4c1b79, 0x08418a0b, 0x1277e590, 0x0d7b325e, 0x33d697ec,
    0x9a6d3521, 0xbffad0b6, 0x9ff5d1e9, 0x6f475c9d, 0xd18e5168, 0x6956d3ed, 0x2b148f3a, 0xd5e86e58,
    0xff94f4b8, 0x76517f30, 0x28c87de4, 0x74431f33, 0xb10258b3, 0x0ffedc10, 0xda320f7d, 0xbaa6abfa,
    0x3ea0936e, 0x02fd2802, 0xa5756dab, 0x728a853b, 0x9e172ece, 0x5a1b5929, 0x17b57ac4, 0x2e0c071f,
    0x121cc368, 0x5ec821fa, 0x56ba81fe, 0xe38361f2, 0xe51e96be, 0x8e942749, 0xbe3f795f, 0x6f220567,
    0x62391ad9, 0x95efef99, 0xbb2d6919, 0xbb7c765a, 0x447f372c, 0x4ccb5bb8, 0x469affff, 0x36e8516c,
    0x53871686, 0x5cd55efd, 0x227d29f2, 0x2b0e0642, 0xe645ce7a, 0x27b451cc, 0x2141ec4c, 0x23452f7a,
    0xc4168de0, 0x25368729, 0x6b381532, 0x1a8b165d, 0xf82ca1b5, 0x484e2f06, 0xd4e470a3, 0xf88e0290,
    0x0e0ee6d6, 0x5d66e816, 0xb5ae5bf5, 0xf5deac95, 0xad9f7f35, 0xd45e9ba1, 0xf7efba13, 0x80e16f32,
    0xabb06f22, 0xdb511182, 0x4f9936e8, 0xf2201209, 0x5ff7b080, 0x03a7dd50, 0xd0ad48be, 0x4307fda0,
    0x237a6f70, 0xf0c996bf, 0xdd86bf1e, 0x9c3091df, 0xe34a561f, 0x9e9adf72, 0x8f3fa4b8, 0x9e008667,
    0x350996ff, 0x01f530d3, 0x9e16d3b9, 0xbeb9b3bb, 0x7082e367, 0x0836d658, 0xdc2cd192, 0x860d0c06,
    0x749cc915, 0xec961b10, 0x1d63ac2e, 0xc93fddb1, 0x3ce0a1b6, 0x63477fd5, 0xcb847cd8, 0xdcc87c1f,
    0x7d97355c, 0xcad0fb10, 0xd44b87a2, 0x17d80a89, 0x4cf3c9fb, 0xcc432747, 0xd175e6c8, 0x0eac48ee,
    0xc556ba73, 0xdfb96a62, 0xf0c5a2d4, 0xe1dcbd2b, 0xaafddc0b, 0xb6718dfa, 0x30f6ce84, 0x44947a8f,
    0x36495e31, 0x60ee32bc, 0xe662a18d, 0x784017ba, 0x33186b4b, 0xe30dfa6a, 0xd94bdfee, 0x000d3177,
    0xbe23ed43, 0x2deb84b5, 0x3ab050b1, 0x13fd3073, 0x3ea0936e, 0x02fd2802, 0xa5756dab, 0x728a853b,
    0x246800f7, 0x2d094b4e, 0x800dc7f3, 0x3c5578e5, 0x4d555267, 0x5ae403f9, 0x05a042cb, 0x6bedbcb0,
    0xf4f1d820, 0x12e4e836, 0x20eece7b, 0xf4d5dcd7, 0x1bb9e21c, 0xfe7eeff3, 0x8278b999, 0x8f966d5a,
    0x447f372c, 0x4ccb5bb8, 0x469affff, 0x36e8516c, 0x86eb9c1f, 0x5ce0118e, 0x8b678913, 0xd3ac9335,
    0xe645ce7a, 0x27b451cc, 0x2141ec4c, 0x23452f7a, 0xe48ed083, 0x12f4f6a3, 0xfd90a9bf, 0xd2709a83,
    0xe17e8759, 0xafe32211, 0xe562380e, 0xd362220a, 0xe45a2dbb, 0x08006758, 0xc48fc30a, 0x3b884149,
    0x83599962, 0x3be2bf5d, 0x8d3155ec, 0xf83d744e, 0x3d0f9909, 0x8e705381, 0x2f9aacf6, 0x4bfcde1e,
    0xfc09a876, 0x7fec1fbf, 0x35aff167, 0xbcffd1e0, 0x237a6f70, 0xf0c996bf, 0xdd86bf1e, 0x9c3091df,
    0x25c3fea0, 0x4ede2b70, 0xf8e1f4e8, 0xb20c9538, 0x2e1ba9ab, 0x47e0d345, 0xf022d737, 0xd09d6006,
    0x9ca27d21, 0x117279b3, 0xbf6663e6, 0xe4b6a423, 0x0b619f8c, 0x02055e31, 0xa8f01a6a, 0xa557b891,
    0x9e6f8f33, 0x00c50798, 0x2fc3eb68, 0xc86c6f89, 0x7d97355c, 0xcad0fb10, 0xd44b87a2, 0x17d80a89,
    0x0fd183f5, 0xaf2e7640, 0x42d4dbce, 0x64df160e, 0x7ac39593, 0xfd108029, 0x8786d346, 0x938dafa7,
    0xaafddc0b, 0xb6718dfa, 0x30f6ce84, 0x44947a8f, 0x7bb8bead, 0x1e5a353b, 0x8a173baf, 0x1cd6e25d,
    0x8c3ea773, 0x8dc662cc, 0x1e6de9da, 0xaf8f926f, 0xec707760, 0xcc7d45b8, 0xc467a773, 0x675c24eb,
    0x3ea0936e, 0x02fd2802, 0xa5756dab, 0x728a853b, 0x8dd3b9fb, 0xd8c8b7b2, 0xe29f9d79, 0x69dce2cd,
    0x118a06fa, 0x487f5f39, 0xb0489105, 0x487b59c8, 0x021adc12, 0xe821395f, 0x14e227c4, 0xd984af45,
    0x8b84e612, 0xbe11e69c, 0xd8f4df36, 0xbdaa8757, 0x447f372c, 0x4ccb5bb8, 0x469affff, 0x36e8516c,
    0x0ce7700c, 0x587e54fb, 0x9e542efc, 0x9a2c676e, 0x1876f9e0, 0x7d9aafea, 0x08defb18, 0x0ddda72e,
    0x13e7fa83, 0x6e823cdc, 0xcb83df0c, 0xc104a652, 0xf070f66d, 0x196619f0, 0x993c5808, 0x05f54637,
    0x4370cf00, 0x86452e0c, 0xd288fe83, 0xee1b1901, 0xef913fb5, 0x36ee052e, 0xab81e9ae, 0x03aebf5f,
    0x926df55f, 0x0b2fd9a2, 0x9c9fb176, 0xa2aded08, 0xd463f400, 0x2642aff2, 0x6b56c4a4, 0x307493b5,
    0x0efae561, 0x0fb780b4, 0x83084b59, 0x9d6a167f, 0x447f372c, 0x4ccb5bb8, 0x469affff, 0x36e8516c,
    0x67c0c476, 0x49c72fe1, 0xf4586927, 0x244cf77a, 0xf3ed5184, 0xa121262d, 0x34709e88, 0xd2241c97,
    0x233b0028, 0x0d82fba1, 0x02755875, 0xed6ff7f8, 0xa48dcef5, 0xe5d8bc60, 0x1b9c18f9, 0xa53aa0f7,
    0x962d2983, 0xe0f1c025, 0x41afe4a1, 0x6df65f5c, 0x05795480, 0xe19b9659, 0xfe665712, 0xbba3a300,
    0x533f6acc, 0xf0532b28, 0x572cbe66, 0xef0ae216, 0x2cffc8cd, 0x2ade1c1a, 0x9e911155, 0x1d9b257a,
    0x9f4e67c8, 0xd060ffd6, 0x5093fa23, 0x7f6b7cd9, 0x5f3000fa, 0x5378d69e, 0x5801e944, 0xb4e465bb,
    0x55a1979f, 0x7fd9aaf8, 0x8acf3bee, 0x2a8e5953, 0xf9c078b0, 0x1225204d, 0x4ec893e0, 0xd1292a4c,
    0xedcccf7a, 0xb99ec1b2, 0x5a87d166, 0xba751087, 0x79a2defd, 0xbea442d7, 0x5f9a8088, 0xcb6baa02,
    0x1fac0555, 0x94af2175, 0xbf21a205, 0x3ca08b88, 0xae495db6, 0x76bdb813, 0x92e1795b, 0x5558440c,
    0xc22ab1f3, 0x3bbf521d, 0x954c9cdc, 0x31472350, 0x7c0c2615, 0x2cd9717f, 0xd6afbd0a, 0xacbfdb26,
    0x66317017, 0x541ca69c, 0x166a4c90, 0xb37d0d7a, 0x08ceccec, 0x715845e1, 0x0839a00e, 0x8407c351,
    0xa69c4732, 0xfaef5ea6, 0x800cc41b, 0xd35a89d0, 0x0a828488, 0x3f9a1e9c, 0x5abe85e2, 0x7971b067,
    0xd48fe4ac, 0x47b491fa, 0x18ab7ca4, 0x2cee7377, 0x926df55f, 0x0b2fd9a2, 0x9c9fb176, 0xa2aded08,
    0xd463f400, 0x2642aff2, 0x6b56c4a4, 0x307493b5, 0xec13ac53, 0x5dd951f4, 0xf2771050, 0x7744e9d4,
    0x447f372c, 0x4ccb5bb8, 0x469affff, 0x36e8516c, 0x78e40b9d, 0x7f2a2e85, 0x5caba860, 0x93ca8c74,
    0x90bc1ec0, 0x3d2349a5, 0xbbdc9bb8, 0x78a8dbf9, 0xb0e69b6b, 0x91ff1575, 0x5e7acb01, 0xaecece23,
    0xa48dcef5, 0xe5d8bc60, 0x1b9c18f9, 0xa53aa0f7, 0xd3840044, 0xb3d67ade, 0x0a479c84, 0x41297e81,
    0xcff3cd1e, 0x5b40b2c9, 0x72d6e968, 0x181568dc, 0xdbf2ff87, 0xfaf9126d, 0x1a03fa00, 0x8ae3ecff,
    0xb6de8c2a, 0xa7fa0997, 0xa75e4bb1, 0x534599f0, 0xc399dcb7, 0xc9b6afe0, 0x10f26a7c, 0x72d0ec5e,
    0x4b609f1a, 0x31d2b8b4, 0x786988d9, 0xae0bcf93, 0x34f0f04c, 0xbaf1a334, 0x27be5565, 0xc6a0dfc4,
    0xef8f5251, 0xe3197c13, 0x6b565d7d, 0x07ac4a79, 0x08a1db6c, 0x7ccdeb33, 0x2e279b88, 0xcac70394,
    0x0791ccb6, 0xe2acd74e, 0x962c4a9e, 0x86d28f7c, 0x06f3c435, 0xa18d07dc, 0xe18d45a3, 0x79369e25,
    0x0da516ec, 0xb3bdf89c, 0xfee33d15, 0x1c4f1ca8, 0x1e9818c4, 0x73100473, 0xbe188417, 0x3c27ecf2,
    0xbae8d5c5, 0x03d96b42, 0xba806aef, 0x637f2215, 0xe9d9c337, 0xd4651a91, 0x2103a220, 0xce40a840,
    0x4c8575c6, 0x856716ef, 0xcfcac209, 0xc2e3572c, 0x2cc52a19, 0xb551b4e1, 0x76e1c24b, 0x66798f71,
    0xd539e2ec, 0x52c10782, 0xf292452b, 0x50774010, 0xda563111, 0x055b01ae, 0x7fbcbcb9, 0xfd04a301,
    0x99e823ab, 0x86578882, 0xf3df66d2, 0x5d4381e8, 0x041b7f57, 0x8052477e, 0xbc55058c, 0xa746c1f9,
    0x31c04680, 0x705c9018, 0x11fb63ce, 0x9294ad33, 0xa9a17ca3, 0xa2ad9035, 0x0b082ba9, 0xb3e5af2a,
    0x7cfde7c0, 0xf93b3e6a, 0x45cc6c85, 0x555fcbcc, 0x6741b3cd, 0xd3324d98, 0xf202813c, 0xe26cc2d3,
    0xb12feea2, 0xbea3a47a, 0x3ede61b9, 0x40a78226, 0xbbf28903, 0x63b6fc9f, 0x924849f6, 0xb2d6b5a1,
    0x902f5fad, 0x52227cdf, 0x40c8eb17, 0xe139e76c, 0xd72e2290, 0x931566d2, 0xd3ef0d1c, 0xf7eaf993,
    0xd3e78047, 0x829186a7, 0x42fc6d0e, 0xdfdfae74, 0x9a796996, 0xcda5832b, 0x15bfe405, 0x9e93a223,
    0x6084b97d, 0xff6f9800, 0xebe28050, 0x90d6c402, 0xbf2d53c6, 0x91977b6b, 0xb6437076, 0x6ae2f885,
    0xeaf3d357, 0x716032c2, 0xce0b94ea, 0x0fd2917d, 0x1a61495b, 0xd7fc8aaa, 0x9329fab1, 0x7ccfa83f,
    0x04a3645a, 0xc71939b1, 0x7348e65e, 0x2b47239a, 0x549187cd, 0xc8edddec, 0xe8551c58, 0xecb19d62,
    0xf9aad98d, 0xd86d7bcb, 0xa1d34719, 0x4af7e67f, 0xd3f0b123, 0xbbe31172, 0x00b1561b, 0x1dea19a0,
    0x8a77d594, 0xa8320c01, 0x2c5d3ee6, 0x8739d5b8, 0x752ff671, 0x89fa4bd8, 0xb41f1e43, 0x514e4a8e,
    0x19076345, 0x2b81a695, 0x8b5653d9, 0x614b54de, 0x79d95afd, 0x3c700a3e, 0x1731c1cd, 0xf877cef5,
    0x6e1524f4, 0x3ede085f, 0x3ad5e2d3, 0x82b5ba17, 0x676f8c59, 0x76be3ae0, 0x20c090b0, 0x1f8bf512,
    0x50505204, 0xc1925905, 0x3c797988, 0x612c6be4, 0x131a1a74, 0xf70f5d65, 0xf5c55a81, 0xec35c909,
    0xaada4d79, 0x551cd02a, 0x0027a3db, 0x83a25728, 0x1a33c905, 0x4bad723a, 0x9cf78cd2, 0x6fb6b075,
    0xe56f365f, 0x66f5b9c2, 0x593afe26, 0x945efabc, 0xb7737673, 0x8c832caf, 0xcb21c221, 0x66472721,
    0x8a2654ec, 0xa087d8cf, 0xbf40e5a7, 0xb2bdd97f, 0xcadda674, 0xab0d850c, 0xe5098b78, 0xa79644a4,
    0x8f3b6006, 0xce4f9173, 0x76b0249d, 0x09c1a085, 0x9586e4e0, 0xec1c8e1b, 0x5d50a36f, 0xb1cd06d8,
    0x2b194b23, 0x890f03de, 0x075b4f87, 0x9565bb90, 0x5f84c640, 0x4bf6c58f, 0x518ac5f2, 0x51a8a84d,
    0xf45c556d, 0x344a4d61, 0xf3a00232, 0x9ec08a01, 0x4a8347fd, 0x1c2d926b, 0xdac93fb1, 0xc1d388c0,
    0x60d721dd, 0xd8d02844, 0x4e9b14ba, 0x14efa303, 0x03d4548d, 0x64e71366, 0x4f1b4295, 0x8bff8719,
    0xdfbf247d, 0x7a8abb45, 0x2c9d1deb, 0xbe6c9de8, 0x767cd230, 0xe8529956, 0xf747ed08, 0xcbf8075e,
    0x58fadcec, 0xec117314, 0x6121fa89, 0x311bf503, 0x4298bb98, 0x467c3fa8, 0x786921bf, 0x736556ce,
    0x300ef50a, 0x94acc9f5, 0x9376ac3f, 0x94b8e00a, 0x029b9be4, 0x0112e13f, 0xe3ed4192, 0xb551d358,
    0x9bdecded, 0xe50907ba, 0x6b612b20, 0xc862b4b9, 0x25d98cb9, 0x304c8c72, 0x5a1efc68, 0x67d5948d,
    0x1b392db5, 0x17795751, 0x90a154d1, 0x2ec12eae, 0x6e3b20b6, 0x966fe2d1, 0xe5174459, 0x5db81045,
    0x87927875, 0x6d1d7953, 0x5affcaa5, 0xdaca9a8b, 0x19fad2a1, 0x44453cc2, 0x1378575e, 0x3b0eb8fd,
    0xd55fc959, 0x0d2976f4, 0x3eda8c81, 0xcd8022cf, 0x29a6858b, 0x91c09e86, 0x64f61530, 0x7a713144,
    0xe75b5a02, 0x5de3ca3c, 0x9b8dc07f, 0x4c93db72, 0xc51da55a, 0xc8e53d85, 0xa1fa8b9a, 0x8607f561,
    0xfc7d597d, 0xa77e13b6, 0xb954f523, 0x57919fa5, 0x587aa1ab, 0xd0def359, 0x6f72a64f, 0x97075ed2,
    0xf989b95a, 0xbcc67d4d, 0x6ba4362c, 0x09f1a4bc, 0x50e6e76a, 0x633cb05a, 0x98973793, 0x47eb4990,
    0xd3887c10, 0x96e43769, 0x83aac7ce, 0xecbfb48e, 0xdecb6c82, 0xf5c65251, 0x1c519407, 0x88cc5669,
    0xe3e1e535, 0x5fd22eab, 0x94fbe54e, 0x022beea7, 0x2ced0c5b, 0xb10d9c4f, 0x85e9f696, 0x329c55d7,
    0x6ef72eaf, 0x831f8d07, 0x767011f1, 0xc2e77c72, 0x33163d4b, 0x7a7897fd, 0xe7deb088, 0xad53fc4f,
    0x583348fa, 0xbdc7a812, 0x14e5e831, 0x0899378f, 0x3dda9a43, 0x4a647c95, 0x831c4983, 0xfd335dbd,
    0x0fea840c, 0xf1e65c3b, 0x03924721, 0x3bed7f64, 0x50fa3941, 0xc4facde5, 0x7211c58a, 0xaf534164,
    0xf95bcfd9, 0xbdbb7477, 0x57d26c85, 0xb1fee327, 0x59c97240, 0xa9860020, 0x0efce37b, 0x62d26c49,
    0x8f3b6006, 0xce4f9173, 0x76b0249d, 0x09c1a085, 0x9586e4e0, 0xec1c8e1b, 0x5d50a36f, 0xb1cd06d8,
    0xdd3bc575, 0x5d74504d, 0xd09189cd, 0xf9b4f33c, 0xbdd0063e, 0x8cf66bbd, 0x8ae1c164, 0x8abb5edc,
    0xeee52aaa, 0x4b540985, 0x9cb70c36, 0x864385f1, 0x4a8347fd, 0x1c2d926b, 0xdac93fb1, 0xc1d388c0,
    0x33a740dc, 0x23e50793, 0xcccc49d7, 0xc39d09c8, 0x69dd57c3, 0xf9b0aaa2, 0x38b3908a, 0x0b166d46,
    0x6ea7b2fe, 0x55aa4830, 0x3570fdb3, 0x3d107d1c, 0x8eb47c2f, 0x19940fe4, 0x71fc1fc3, 0x8da18a92,
    0x373496ba, 0x955ea4ac, 0xa8b8d4a0, 0xe4828dc5, 0xa4739578, 0x1a9cc821, 0x53c939b4, 0x0aafa5d6,
    0x68cdc6a8, 0x40624dd1, 0xbc703db0, 0xfe9693a3, 0xd8c0df6f, 0x2bc133c2, 0xdf052dae, 0xcb17932f,
    0x6363368a, 0x4ba59592, 0x38014211, 0xe2174d3f, 0xc0306451, 0xefbd77c5, 0x063a1fa4, 0x33af9ee1,
    0xd9a6f9a0, 0xb99d74cb, 0x235ddfbd, 0x28f81ede, 0xf24e3a7e, 0x8ec7710a, 0xda969bb8, 0xc81c4aba,
    0x2d0c2bc8, 0x7f631fe5, 0xea28b59c, 0x49751037, 0xf72cc904, 0xf685a5b4, 0x105fab7e, 0xec3a6489,
    0x7edf1615, 0x5acb39ad, 0xe9b9d44b, 0x364051a6, 0xbac382f5, 0x01ea7852, 0x36472f25, 0xee11bf26,
    0xb54caf21, 0xc036b90a, 0xabcdf336, 0x62dc1d93, 0xa40dc0db, 0x69e7e43b, 0xc6c2111a, 0x28d3b015,
    0x7d97355c, 0xcad0fb10, 0xd44b87a2, 0x17d80a89, 0x9afa12f8, 0xb84a0e7e, 0x2d7d1675, 0x9457b5f6,
    0x6dd7ddd6, 0x2c04e87b, 0x1311f571, 0xc561dab0, 0xee3db7a4, 0xb0a9b6d6, 0x0d9d9343, 0x3c886883,
    0xacd60f12, 0xd2540238, 0xde1f5c12, 0x85655457, 0x0cd46ac1, 0xe1aac9a5, 0x3c7ad335, 0xf5963093,
    0xa48dcef5, 0xe5d8bc60, 0x1b9c18f9, 0xa53aa0f7, 0x881bc078, 0xe83ea033, 0xf510f3a7, 0xe15a3729,
    0x06096beb, 0x947cec50, 0x6503e6d4, 0x3ba2af24, 0x4000dca3, 0xc129adc4, 0x751b6d7b, 0xfb412d97,
    0xcd0a862b, 0xa8341205, 0x24b95a9b, 0x9e5e0ed3, 0xd3cfd056, 0x80fa6c91, 0xc1fafce9, 0x87a7d29f,
    0x8aa28427, 0x93462f8a, 0xd0978354, 0x37641c58, 0xa438cc0b, 0x89c026d1, 0xff19b5c2, 0xcdb0976f,
    0x87ee7710, 0x67ef0c1a, 0xf1d846c9, 0xe0cd7919, 0x16f23689, 0xb3ef99f4, 0xac07b0e6, 0x75de393f,
    0xebbf166b, 0xedf7301f, 0x2d2aa423, 0xd8296261, 0x8b3adad0, 0xe0d0cf7e, 0xcb61bb50, 0x41668d7b,
    0x47a00359, 0x3368e82a, 0xa034c152, 0xa6b56ecc, 0x7d860b80, 0xe0989324, 0xcb3f852e, 0xa1f16c22,
    0x5f3275d7, 0x2e6da113, 0x806dd23a, 0x8ec6fbba, 0x40f26d2b, 0xb0226b65, 0x8c2ba70b, 0x7e5b0901,
    0x44aa6a56, 0x1088ce23, 0x7b3e7865, 0x399390fd, 0xf6557816, 0x4f8fe9e9, 0xd943c785, 0xe4a00ac2,
    0x3f09415b, 0xc50ae536, 0x37e03947, 0x31925a01, 0x55a1979f, 0x7fd9aaf8, 0x8acf3bee, 0x2a8e5953,
    0xc7acbc29, 0x7b33ea4e, 0xaa0eb160, 0xc5e38fdb, 0x651e88dc, 0x2309f6e7, 0xe99775a8, 0x6f4a3c4f,
    0xecdb3b4a, 0xb6218472, 0xfbf9d28e, 0x33d07a6e, 0xedc52da8, 0x51747d93, 0x39239c31, 0x7bf82973,
    0xbebdaf3f, 0x0f00e897, 0x909a4ed2, 0x57e755c1, 0xe9066c53, 0xdcd4ed5b, 0x82838527, 0x44b97302,
    0xe4874b30, 0xe3072571, 0x4900dc51, 0xbb718801, 0x1408ae83, 0x7a011561, 0x4c530604, 0xf4e7af10,
    0x6c47187f, 0xd1179f3d, 0x66926916, 0x20f0c771, 0x89f91028, 0xf4744dfd, 0x507711f0, 0xd8240828,
    0xf8b22177, 0x9c766294, 0x8a50d991, 0x902b1958, 0x6e225e22, 0x761dbcfb, 0x75f927ee, 0x3b4640bd,
    0x6118f987, 0x388177c4, 0xd43a3324, 0xfe915bfc, 0x6f34170e, 0x042a1e3b, 0xc25b3664, 0x7b5cd07e,
    0xf53f75e1, 0x32a662c3, 0x5f7fc90b, 0x4fc63575, 0x2b51b433, 0x5b798c5a, 0xdb5b1970, 0xaed928ed,
    0x2ba41fe2, 0xd1cb8848, 0x5e353e6b, 0x075217b1, 0x8a05123f, 0x7a39f6ec, 0xcf9d7c08, 0xe81bb5e0,
    0xf910b860, 0x51894dba, 0x48c3711d, 0xd5bd2533, 0x1f006ead, 0xd1102d2a, 0x604b82eb, 0xa7905cea,
    0xca9c4226, 0x437e1315, 0x18a794c6, 0x460d8444, 0xad29fca1, 0xd47e3fa3, 0xd439ed06, 0x9e630979,
    0x4d09cbd4, 0x1243f734, 0x76356050, 0x304e0f0c, 0x13967f56, 0x8ffc00d2, 0xee34a571, 0xd0388ffe,
    0x7c69f747, 0x0f3b8b2e, 0x6ff08312, 0x87e3aa3e, 0x006a6e8e, 0x9ae9d13a, 0xf1ca8037, 0x3b946bcf,
    0x4ce09c0f, 0x48f6a3e3, 0x16760942, 0x4d3b4696, 0x2ab6eb82, 0x75cb2a1f, 0x10de7a8e, 0xb26b7cf5,
    0xf2ccfaf8, 0xdab12fbc, 0x0c747a48, 0x92f990a2, 0xae682b71, 0x5a5c75d6, 0x3490a91e, 0x3f22f072,
    0x2eed2e61, 0xb30c94ef, 0x8807b014, 0x7d7b7a1e, 0x2b30b466, 0x3ecb1502, 0x0082093c, 0x4cdd28c1,
    0xbc07b422, 0xd31d5fa2, 0x69804955, 0x3690941e, 0xf673110f, 0xc1bb826d, 0x3d51e592, 0xf38ed1ea,
    0x00efece3, 0x625ed66b, 0xf3675415, 0x59fcd977, 0xdffdb581, 0x45a86c5b, 0x482da5e0, 0xbe990d7c,
    0xb614d5b7, 0x89a1ee8c, 0x9961175a, 0x7180ae35, 0xcca1299c, 0x8f23c874, 0x3bb27406, 0xa62ac0c1,
    0xa629c38d, 0x12ac128d, 0x821f90db, 0xef6b0b08, 0x95295ea8, 0x9aa164f5, 0xb7b827a9, 0xefa8ee40,
    0xe7408d02, 0x956d440c, 0x6f857564, 0xdd3a9071, 0x42716e92, 0x8989ce00, 0x14b13927, 0xaf4165e6,
    0xd28947dc, 0xad586aca, 0xeb965ff1, 0x472ce76d, 0x71443a33, 0xd17ceb75, 0x2ec50de6, 0xd4cddf90,
    0x060c472a, 0x1f787487, 0x19d26853, 0x61df8791, 0xed04f83a, 0xd0880f67, 0xc6ffb210, 0xd00914fc,
    0x55dae6f8, 0xdd759a75, 0x351d9384, 0xc814af06, 0x43b5e204, 0xcbbd03d1, 0x38690fa1, 0x8d75f4c8,
    0x7d6a1338, 0x3b8f13cf, 0x4fa28d2f, 0x46eca266, 0xcf93ae99, 0x1e17cef3, 0x0ce9a6b9, 0x8e0e8353,
    0x2b9dd366, 0x0069d828, 0x53498598, 0xe118988e, 0x35e83d35, 0x039c2712, 0xd68b34b4, 0x7d465f09,
    0x43fca3f6, 0xa427aab1, 0xf793bf1b, 0x8e36b311, 0xb93595a6, 0xb03b7914, 0x3052187c, 0xd6d8ff9b,
    0xd63847bf, 0xcfe34153, 0xd1425e8c, 0x097204f5, 0xd0433260, 0x37f56c7d, 0xe417b7ec, 0xb6ab1de5,
    0xb959f7e5, 0x3af36062, 0x3b50df96, 0x2d79101d, 0xcaa60fd5, 0x0faf315f, 0x731b5bfc, 0xe25edb13,
    0xb335ae08, 0xb6476b9b, 0x73a835b7, 0x71f51d56, 0x111bacf5, 0xa3d8c10d, 0xea8324ee, 0xd80d180a,
    0x3a1a77cd, 0xdeea9405, 0xd30bd33b, 0x67bafd1d, 0x30513637, 0x59efdb66, 0xf0662c86, 0xc31cece3,
    0xec4afe74, 0x8981c9af, 0x06123876, 0xb4abf869, 0x873c78c9, 0xd989cc0a, 0x6b4d6823, 0x0bc7e13b,
    0x42a3ba79, 0xffaa93a2, 0xf29d13d6, 0x6ab9295f, 0xdb5cf5e0, 0x2c6b888a, 0xc5117e8a, 0x0d50a768,
    0x075e24d5, 0xd88f1b5f, 0xdd0707d8, 0xbb543fc8, 0x5a17e961, 0x0299a82e, 0xe060ce10, 0x1e49988b,
    0x549345a3, 0x6b55a54d, 0x14a3bc42, 0xb92617a6, 0xbc3c7238, 0x34df4430, 0xccb139ea, 0x72cff65b,
    0x4a001b6d, 0x96ed0ee0, 0x7ff10e69, 0x07f68d70, 0x443d3aed, 0x93e72468, 0xe70dca07, 0xd6d4e228,
    0xbe0c63ba, 0xa8cb8d17, 0xb0f656c7, 0xc94d605f, 0x451559aa, 0xf0c190db, 0xa850d488, 0x730e425b,
    0x77db3b34, 0x84ba7806, 0x4f18e17d, 0xef521f3f, 0x2b55ae86, 0x375e623c, 0x3854e8d0, 0x1adf8453,
    0xcd6df269, 0x2fb944a6, 0x243a88c9, 0x485235c3, 0x067c73a8, 0xffb5100c, 0x1473b8d8, 0x9274e174,
    0xdf3817f6, 0x10a09c2c, 0x6b097f38, 0x1cf1cf84, 0xa362444c, 0x614bb12a, 0xbefbd9f7, 0x34a484f0,
    0xaaedca38, 0x8299743b, 0x703daece, 0x202cbad3, 0x013b61f7, 0x325b9ce1, 0x974a72d5, 0x91147a47,
    0x60b45af7, 0xb55e0d2a, 0xbcd9de31, 0xeac9a12e, 0xf692d406, 0x43a921bd, 0xb1ed662f, 0x316cf3fe,
    0xc352d38f, 0x2d0b364a, 0x3e0f910f, 0xb0ab22e7, 0x4be84b44, 0xbba9fcdc, 0xb03de6ce, 0x26055989,
    0xe655a601, 0x81600435, 0xe9b54efc, 0x50d07b3c, 0x3725303a, 0x125c083b, 0xf5e46c27, 0x45e86085,
    0xf1496cfb, 0xabfacf3c, 0xe6519ba9, 0x672e6ef4, 0xbf160476, 0xbef79d03, 0x4d1b63b1, 0x8f95fbb5,
    0xcde21f74, 0xaec107a3, 0x2a2b4842, 0x06353012, 0xbfc2663a, 0x47fc0bc6, 0x87097bce, 0xfe18fba8,
    0xef0f8f48, 0xf79a0854, 0xa914b632, 0xce669165, 0xed10d0d6, 0x4d954b03, 0xb95cb1ca, 0x0863c7ca,
    0x4bc904b1, 0x0b6bb2eb, 0x0af1c94f, 0x3843be8a, 0x106866bc, 0x04d1fd33, 0x6a46635c, 0x29c88156,
    0xccbef8cb, 0xd1fba172, 0xff10f083, 0xdc928812, 0x7fd66751, 0xa52f8228, 0x0d4828d9, 0xfccc1d54,
    0xbde13bbb, 0x46889bf5, 0xbd9bb5d8, 0x1a0f17b4, 0xc1e398fc, 0x414e0fbc, 0xe0894ceb, 0xedfa2b71,
    0x3a0d0289, 0xd93c1cef, 0x1c81922b, 0xa902c2d7, 0x12a4169d, 0x41f3a1f9, 0xc502ec9b, 0xbface832,
    0x3792d2aa, 0x9011f0da, 0x5e2fb0d5, 0xb3e638d7, 0x9549ae87, 0x0e68d0d7, 0x503fc8e9, 0x79656df4,
    0xa61459ed, 0xf99240a3, 0x46ca75a7, 0x1a0b09c0, 0xf6b09431, 0xe8534db4, 0xc52e1472, 0xf978066e,
    0xd8adcd88, 0xe356f283, 0x940a3821, 0xff74fb31, 0xd0ec2544, 0x829f5cc6, 0xc552f99a, 0x46ce2786,
    0x92181c26, 0xa9c75c43, 0x9f9eb9c6, 0x61fc6722, 0x5ba84f94, 0x1daa87a2, 0xae887fda, 0xb6a7e9ae,
    0x1d7956b2, 0xb6450c18, 0xa35e8e5a, 0xbd70fa7a, 0x59d58fdd, 0x6e15ee14, 0x3c4b10cf, 0x35437359,
    0xfc6d43d9, 0xb94f5083, 0x3a6b91a7, 0x15fac32a, 0xdd900ec7, 0x253ecfdd, 0x04108fd2, 0x6f5337a3,
    0xc347c775, 0xb313e402, 0x58fd4511, 0xfe3b48d9, 0xf3f4c7ae, 0x49a26329, 0x022e1711, 0xf975409f,
    0x084c9249, 0x4b662ce1, 0xcbde49eb, 0xba4ffd6a, 0xb00c81da, 0xefe597d3, 0xac797848, 0x21923aff,
    0x06cb606d, 0xf35c3ddd, 0x1a0980f1, 0x579b53c2, 0xd3374a63, 0x13952e59, 0x76032f9d, 0x0fda7dfa,
    0x41dde54a, 0x3cb1210e, 0x49e771bb, 0x91247253, 0x17f02c2f, 0x026db992, 0x59f233f8, 0xfe032d92,
    0x91bcf413, 0xbca9defe, 0xa0664920, 0xe7d43dfc, 0xcbf565f5, 0x28c065ed, 0x5ed92f5b, 0x3007b8ef,
    0xc51c09d1, 0x9b0411cd, 0x60138515, 0x0d6428f6, 0x192bb9aa, 0xc33cb8b0, 0x659066a7, 0xcee224e0,
    0xde19556d, 0x85e1a505, 0xaa6582bb, 0xd85087b1, 0xb0191bed, 0x9973cb5b, 0xec028e48, 0x3d6bae35,
    0x62f17e88, 0xc7e1e484, 0x31e2b867, 0x5059d835, 0x93d38469, 0x170e3303, 0x8d432776, 0x6eb7bb7e,
    0x1dc5dc1b, 0x4e6d4a12, 0xc8ace6ce, 0x5c30b7b4, 0xe7883dca, 0x96d32bb7, 0x695ca3b4, 0xa557160d,
    0x4d1d319f, 0x36c1579f, 0x0f72bb27, 0x83ddc7d5, 0x031c20d3, 0xedec2dd2, 0x3099fe1d, 0x4924c33e,
    0xe03c787f, 0x73c04350, 0x4b6d2a3a, 0xe9689515, 0xccc34331, 0xc1417f90, 0x40c0c84f, 0xbd3ac9ab,
    0x18a9ff74, 0xe173cfac, 0xffc01a98, 0x12215626, 0xbc97b664, 0xad2e07c8, 0x596a362f, 0x300ae140,
    0x9a07c126, 0x2cbca2fe, 0x5f7940f5, 0xa88e3701, 0xf34afdb3, 0xf2553053, 0xd03d4ea1, 0x0370ea34,
    0x4cf31370, 0x3dab2756, 0x67b9fdd7, 0x3994d528, 0x74e7e560, 0x2dce1e0d, 0xc2786108, 0xaed41b87,
    0x0a5f07b7, 0xeae839ca, 0xe1999ad2, 0x018f6328, 0xd0b72992, 0x05a88439, 0xb0b29950, 0xc685ac78,
    0xea794a8f, 0xbf6ac1e0, 0x6ecf7a62, 0xd1438551, 0xb639d4c0, 0x8be8c716, 0xfd212bdd, 0x7cb185ed,
    0xfe913962, 0x6f98e1ca, 0x9f2c295b, 0xe3c8381b, 0xcad22e21, 0x88057cd3, 0xede8c4ab, 0xfd3c3d28,
    0x53016ce5, 0x73a4dc67, 0x8f4b9653, 0xbf69380e, 0x926df55f, 0x0b2fd9a2, 0x9c9fb176, 0xa2aded08,
    0xc1285271, 0x13cbb554, 0xad8441c1, 0x3a8c3501, 0x4e0d3064, 0xfca695cd, 0x0f9e4f3e, 0xc2e81ca1,
    0x5e65a8bb, 0xb12f4b0d, 0xea5de28d, 0x5f179dcf, 0xcaa8e0ff, 0xa685ae80, 0xcbb69d67, 0xfdbb95dd,
    0xb28c86a7, 0xc7ca3aed, 0x73924305, 0x88b856e9, 0x1a30372a, 0x44488dc9, 0xba0027ea, 0xed529db8,
    0x769818e4, 0x62f591b4, 0xe951c0eb, 0x2ec4ada5, 0x922d6c86, 0xf15a0d0f, 0xf1aad020, 0x5337ba1c,
    0x32d7ead0, 0x1e358f76, 0xd74b8ed2, 0x6f67d034, 0xf4136a32, 0x6ddc4500, 0x003336c2, 0x39605280,
    0x4838b495, 0x057f1284, 0x969892bf, 0xff3741cd, 0x9f4e67c8, 0xd060ffd6, 0x5093fa23, 0x7f6b7cd9,
    0x4ffbac38, 0xef13be27, 0xd8e32453, 0xe7956d62, 0x12461d88, 0x367e59cd, 0x5ff21b1d, 0x34648811,
    0x9335a37d, 0xff88452b, 0x03ca3d5d, 0x647c73dc, 0x9c2755ad, 0x3d564bd7, 0x8776a2cb, 0x54a2f819,
    0x2463f223, 0xbc63e919, 0xd8fb021f, 0xcf3d42d4, 0x912a46da, 0x4959f0b1, 0x8f3fec7f, 0x1aed19bd,
    0x31488582, 0xcb65a2bf, 0x6eddedd7, 0xe5415fdf, 0xcd253494, 0x2233f611, 0xe741c161, 0x791c99fd,
    0xd0b72992, 0x05a88439, 0xb0b29950, 0xc685ac78, 0x91bfa09a, 0x2def2d11, 0x881eca0f, 0xd68ab133,
    0x6b6b7428, 0xcd20732d, 0x786b6055, 0x886be2a4, 0x04c338c2, 0x80118d71, 0x22d04260, 0xb7549bb4,
    0xe397e2d8, 0x5d5f3bf7, 0xe7f64631, 0x1a7eb2ae, 0x35ad9cb4, 0x825732e2, 0x1e3838e8, 0x25c1fe15,
    0x926df55f, 0x0b2fd9a2, 0x9c9fb176, 0xa2aded08, 0xc1285271, 0x13cbb554, 0xad8441c1, 0x3a8c3501,
    0xd35ffa1e, 0x93e21223, 0xdf2e43af, 0xb31c6bf4, 0x5e65a8bb, 0xb12f4b0d, 0xea5de28d, 0x5f179dcf,
    0x7701dc45, 0x5075ae66, 0x38724ab9, 0x366e697d, 0xb590afab, 0xaa37c67c, 0x250a7f94, 0x18e5d198,
    0xb0ff82ba, 0x26c6bb92, 0x30c59de3, 0x23c2ec71, 0xe369a0e5, 0x8b5f1c55, 0xd7c1de62, 0xf9dbe878,
    0x1af42ee1, 0x45369132, 0x029554f3, 0xcc8ff9ec, 0xd6c13859, 0x020f00ef, 0xc3c9a8e2, 0x5ee80b8c,
    0x7256efe3, 0xbd7363a7, 0xdd488ca7, 0xa16d67a2, 0x42aa20ab, 0x0552dd6a, 0x3b1651c2, 0xd1377481,
    0x8ca7a37e, 0x5cc1e389, 0x9375c6c6, 0xbba83adc, 0x9c5823ab, 0xbd789f11, 0xc79e152e, 0x3449552d,
    0x76db80c1, 0xad039012, 0x1452e6bd, 0x2ae84a19, 0xbd0eade1, 0x3fb21fc6, 0x02fb676c, 0xbc3b8644,
    0xa5fd09f4, 0x969a903d, 0xfb800d09, 0xcb3d74f3, 0x8d551684, 0x6efc7c2f, 0x7b43ad44, 0x683565e0,
    0xd440303e, 0x60b1fd78, 0x956e2596, 0xa073f5c1, 0x3431cc28, 0xcbc92d73, 0x14e6fd05, 0x49383115,
    0xe4baad23, 0x010034a9, 0x9bb9c79c, 0x42f81cfa, 0xcf104209, 0xa4fa0f9b, 0xa3f8afe9, 0x0e9fde34,
    0xf934253f, 0x4f48fabd, 0x7d25d7a9, 0xdce2a876, 0x6c678892, 0x865dad0c, 0x9df9c109, 0x81063a31,
    0xd3fb9236, 0xab63f82e, 0x96dd3dc0, 0xbf48f4fc, 0xe4dfee11, 0x18a9f6dc, 0x7cc86584, 0x2bea5d83,
    0x7652663e, 0x29d00051, 0x147f3517, 0xe4bfeaea, 0x1d783c06, 0xbb971149, 0x181eb21c, 0xaa6efbd4,
    0x02dfc6dc, 0x8c034481, 0xff54858a, 0x30bf98a9, 0x9ac7e80a, 0xc7e692ec, 0xd9adc0cb, 0x505944c4,
    0xf630a7f1, 0x3ff2676a, 0xce236adf, 0x3349d4d7, 0x61d5e2fc, 0x01d50b4c, 0x9bde1f6d, 0xa2acb160,
    0x0e137318, 0x4fdb30f7, 0xfea4fd59, 0x8bc33810, 0x7988a54d, 0xaac50168, 0x0d80f3ee, 0xa56f90ab,
    0xdfdfd9d6, 0x23a3244f, 0x4c8085d8, 0x3d5264cf, 0xef0272b3, 0x30bc2c76, 0x78d2a3e1, 0xe2a4c445,
    0x9f8bbcfc, 0xa7f1f98a, 0x4e20b3a7, 0xfa9b2aa7, 0x34f26dba, 0xfa7fdfa6, 0x2690ec11, 0x598a6d3f,
    0x10520e8c, 0x0bf01d68, 0x6d8a42a0, 0x21067319, 0xad50ce36, 0x70da2bc1, 0x17e07252, 0x003c60ab,
    0xc9062389, 0x9f4b27e9, 0x039cd626, 0xab270f66, 0x6d8fb767, 0xea69e108, 0x4a33fb93, 0x99a3408a,
    0x406afff3, 0xaf8919c1, 0xa711e27d, 0x720d65e9, 0x484bb54e, 0xda27814b, 0xf3723abc, 0xfea36ba2,
    0x69db8603, 0xb8c68628, 0x747523b3, 0xc7622296, 0x4cb9a9a8, 0xa3678d5b, 0xf7327b9e, 0xe66d9af8,
    0x4a526ff9, 0x99a622f9, 0x97936551, 0xbc94968e, 0x78c89384, 0x21f7074b, 0x826ff6fb, 0x538b5474,
    0x804f90ac, 0x4d4893a1, 0xab194dcd, 0x3bfdf710, 0xa9745b69, 0x6596c186, 0x378d9dd9, 0x0091a5e7,
    0xf02ccfc6, 0xb8856554, 0x3fb4416a, 0x4889ca74, 0xe01a66f7, 0x044cdb9c, 0x4924caf4, 0xc4389574,
    0x1973d846, 0xc05cf004, 0x3a8e304e, 0xf02338d4, 0xfaece45c, 0x6ce9f47a, 0x2ae413ad, 0xb06148ad,
    0x8ba335de, 0x3388f9f0, 0xc41a3be5, 0x27fab961, 0xb173e407, 0x888aff74, 0x83212447, 0xd06d8a11,
    0x7e1d2f5b, 0x5ff0e80e, 0xfb183bc3, 0xf4f31f4b, 0x40e2621d, 0x5293bb4c, 0x26e5243f, 0x500b5b25,
    0x9a315d1c, 0x0ab2607b, 0x1749afb0, 0x0890830b, 0x55621115, 0xf8c8e320, 0x9e289acb, 0xb46f07df,
    0xbe623a76, 0x480cc4b3, 0x48e0c524, 0xc3601b02, 0xa6b6ea4d, 0x2d73df71, 0xe710b943, 0x011a1c1a,
    0xdcd15850, 0xe1d96197, 0xf4e74417, 0xc2e6db8e, 0xa4a65ede, 0xa4654603, 0x49b46cf7, 0xecd98553,
    0x769818e4, 0x62f591b4, 0xe951c0eb, 0x2ec4ada5, 0xc969af4b, 0xbb06e7ab, 0x3fd67b17, 0x1d036c89,
    0x9271ce8c, 0x95a6dbaf, 0x9e64f3fb, 0xa2a644b7, 0x3e388680, 0xcde52e0a, 0x57bb7f3c, 0x78f0ac20,
    0xce007d3c, 0x378f17e9, 0x7e3be895, 0x6419ba6d, 0xa1df8180, 0xf8d4f04a, 0x7406a693, 0x71920648,
    0x0c4b884a, 0x327ab872, 0x1a6541a6, 0xda658895, 0x57fbe110, 0x214397a2, 0xab243f75, 0x8aac1148,
    0x225abab8, 0xcdf83460, 0x3126edd1, 0x6b73819a, 0x81a59bb8, 0x5a120ce3, 0x4d1fff9b, 0x7b6642fd,
    0x2c67d08a, 0xf8932d4d, 0xe826f549, 0x78a92737, 0xa6e9f8d5, 0xe96313cb, 0x231751e7, 0x68a8315a,
    0x2a66fadf, 0xa2736258, 0x08a77cee, 0xbc0da0a6, 0x49b2ac9d, 0x517fcef6, 0x039b8363, 0x1679d472,
    0x044eca3f, 0x4af52006, 0x7b95bf27, 0xa40e1cce, 0x6d344017, 0x187cc853, 0x3ed8b1a6, 0xa2dd0fb6,
    0xb7344cd3, 0xb771d01c, 0xaf117055, 0xd770dea4, 0x2e72e309, 0x9497a84d, 0x1b8b4b36, 0x7aa9b3fe,
    0xe0da172d, 0x00d3b409, 0x94fbf2bc, 0x0d20c9e0, 0x66b20c5a, 0x1ed32bb1, 0x958eae7e, 0xfda1c5e1,
    0x2eeb4f98, 0x94315a21, 0x569968bf, 0xc126f4b1, 0xae6dc3eb, 0x0a0980a8, 0x89c9b9f2, 0xb668a352,
    0xa0e2b5ee, 0x0366f1a8, 0xc36c5215, 0x35e97386, 0x9f80e0fd, 0x78fae1f8, 0x6245f4e2, 0xa02d6b66,
    0xb46d9e1d, 0xb48dd426, 0x66319dd0, 0x5c5c013c, 0xe5f33dc7, 0xde74b539, 0xc8edd931, 0x75a861c9,
    0x9d4c0de8, 0x74530fb9, 0x540f8c2d, 0x17a760d6, 0x888da776, 0xfc6a3984, 0x3ecf90e6, 0xb421a044,
    0xd8adcd88, 0xe356f283, 0x940a3821, 0xff74fb31, 0xf6fd7551, 0x6919bab8, 0x7df949b6, 0x641f30b6,
    0x97bb327f, 0xd0905406, 0x104aae3f, 0x2623e4b4, 0xd0c1c218, 0x3d6d07f3, 0xa279c1c7, 0x2ebbaa0b,
    0x9e7fafc6, 0x70c3a6a3, 0xec69c5ea, 0xf15c2e11, 0x410f185f, 0x90aa56d9, 0xdbfe99a1, 0xd2f656b4,
    0xc1dd21e8, 0xf505d649, 0xb08c3b23, 0x1cff39db, 0x4dcbe033, 0xd0b90414, 0x0e5f4abe, 0xc69d7d0d,
    0x32d2e7cc, 0x64dbc723, 0x826da654, 0xbf2ed7e0, 0xf981d9fb, 0xccb09ec0, 0x8c4e6af0, 0x8941fbee,
    0xf209c4e0, 0x9ba4c855, 0x6ae8b04f, 0x2d774e17, 0x093c225a, 0xd45de7ca, 0xea42e340, 0x5a97bcb2,
    0xa8668d49, 0xc8f41973, 0x5fd87166, 0x4417232d, 0x8a0f8685, 0x6f763f36, 0xca383a81, 0x6137ea7a,
    0x5dc57607, 0x6fc7837c, 0xfa2f7982, 0x51b34e10, 0x697e055a, 0x9b070dbd, 0x18f07d51, 0x8f28acfd,
    0xd8130874, 0xbbc71d4f, 0xa51d2400, 0x7e45e671, 0x1d0499ae, 0xa464c609, 0x71ed1365, 0xda07472f,
    0x087b9b32, 0x14838532, 0x52705c41, 0x3805ba34, 0xbd0eade1, 0x3fb21fc6, 0x02fb676c, 0xbc3b8644,
    0xfb9a2d10, 0x81865452, 0xc5edb204, 0x01df48d0, 0xadaadf64, 0x21d5f855, 0x9b90d59f, 0x81d91f87,
    0xd40de906, 0x4b47717d, 0x31614929, 0xa595fb48, 0x5dc1db41, 0xfa161aea, 0x68e19521, 0x3f909697,
    0x2ca2285a, 0x19bba135, 0x8ca416e4, 0xa568afe9, 0x3431cc28, 0xcbc92d73, 0x14e6fd05, 0x49383115,
    0xe4baad23, 0x010034a9, 0x9bb9c79c, 0x42f81cfa, 0x595c8d88, 0x1ec68d07, 0x9f2fa021, 0xd575cb76,
    0x199c2cf4, 0x58f8e801, 0x048a1f79, 0xd6ea5f84, 0x53060c59, 0x408409f8, 0x1f24c997, 0x800157c8,
    0x1d7956b2, 0xb6450c18, 0xa35e8e5a, 0xbd70fa7a, 0x5c468f95, 0x3835496e, 0x82d26b48, 0x5e618ff1,
    0xa097d736, 0x53274ccb, 0x42246129, 0xdd5bf64a, 0xb2fed30e, 0xfad6efee, 0xa862427e, 0x1b7e026a,
    0xc4af1589, 0x3edd7af1, 0x918909f5, 0xff7f1d07, 0x67fb0321, 0x5a8de1b4, 0xad15445f, 0xa56af537,
    0x5abff967, 0x2ce0ec6f, 0xbccd7a68, 0xd8a6c5ab, 0x4edc1731, 0xdfe4af12, 0x42dfc311, 0x23dc34a5,
    0x1f1d9fb0, 0x76641ebb, 0x967f8840, 0x6f4f5292, 0x25ff8f09, 0x246fc378, 0x661e4b15, 0xa3bcc8b0,
    0x779a60c3, 0xad25d736, 0x8f6dce29, 0xad8b3a6a, 0x752d7144, 0x4a2be109, 0x039e1878, 0xf26ac560,
    0xb4a2f546, 0x6aef95a7, 0x36bbe947, 0x16bdc21a, 0xf9ed556c, 0x2a65d00f, 0x52590c68, 0x5879b237,
    0xd49b0f15, 0x9f7546ec, 0xe34f118a, 0x768e3e41, 0xc23251a4, 0xe82fd53b, 0xbbff6118, 0x0cdfe74f,
    0x810121db, 0x59a302e5, 0x126c539b, 0xedda860f, 0xb80ef089, 0x90ea8943, 0xe402b2d9, 0x6d9112b6,
    0xa34113e4, 0x2d52c991, 0x341ed5d6, 0x1ee76265, 0x384ee97b, 0xffb0758e, 0xa30cf15e, 0x1ea20968,
    0xa461b1c6, 0x5c224634, 0x6e496c0a, 0x9c478e41, 0x8dc2f03b, 0x5dc60cdb, 0x8503a03e, 0x13ab5f36,
    0x4cba0370, 0xf1080821, 0x7e496b1d, 0x3d214863, 0x793ae025, 0x98570c8e, 0x899056f4, 0x6d9ff495,
    0x7a0c7517, 0x873569b9, 0x1d24ecff, 0x9208a78f, 0xbd907e3d, 0xf9f35884, 0xf18b2ba6, 0xa0d88d66,
    0xe3a00182, 0x7decfb43, 0x1ebb04db, 0xfefed5e6, 0x508d8bf5, 0x1d0debd6, 0x6af855b3, 0x3db75716,
    0xb676feed, 0x936c4e5c, 0xa8dbaa96, 0x9f32459e, 0x1def53ae, 0xa49ef76f, 0xbd35848f, 0x2d325fc5,
    0x9c4c64ef, 0xfff4062b, 0x8438d359, 0x7c9f1cc6, 0x8583ef51, 0x9d001a3c, 0xa1a9962f, 0x854eba61,
    0xa3a238a4, 0x2914987e, 0x7ca7922d, 0x09cf274c, 0x31d8bf9c, 0xe92eb657, 0x1b356a48, 0x6d369db6,
    0xa213eeb8, 0xe5a99129, 0x3f421880, 0x9b78a79f, 0xdca6c016, 0x9e3539ab, 0x3bbb2ad4, 0xb5dd5a85,
    0x19ff1e66, 0x84f46e8a, 0xe7dfcae3, 0xd0f386f9, 0x8d7159fb, 0xa542eb43, 0x9606e1d8, 0xdbd81882,
    0xe9839382, 0x7eb1209f, 0x0ba20c18, 0x72e3e581, 0xc51bde9a, 0x24d6bd0b, 0xff9af23a, 0x07347888,
    0xf172f5bf, 0x2f7f1374, 0x7c610cbe, 0x25970aac, 0x3705072b, 0x2a6c5e2e, 0xcbac1192, 0xd2bd0fba,
    0x91fedb36, 0x04ee213c, 0x36dad41b, 0xddfeddd1, 0xbdc89715, 0xda56f1ed, 0xcc741200, 0x92d10035,
    0x9c5fa37b, 0x54490f4e, 0xcff0879b, 0x876db0da, 0xd9133c70, 0x7152f4df, 0xf42fae5a, 0x42223845,
    0x4725c76d, 0xaf320485, 0x6b07882c, 0x29e9c197, 0x3dda9a43, 0x4a647c95, 0x831c4983, 0xfd335dbd,
    0xeddfb4f4, 0xe6eaec9b, 0x60c8e524, 0x24e9206c, 0x130b08bf, 0x6f177b84, 0x9442c9af, 0xa48e4467,
    0x19ff1e66, 0x84f46e8a, 0xe7dfcae3, 0xd0f386f9, 0x8d7159fb, 0xa542eb43, 0x9606e1d8, 0xdbd81882,
    0xcfff321e, 0x25aa993c, 0xc4264746, 0xa4f73090, 0xf977d3d0, 0xa50fecde, 0xec7821b1, 0xd8a1ee6a,
    0xf172f5bf, 0x2f7f1374, 0x7c610cbe, 0x25970aac, 0xeb1cdee5, 0x3e8cd2e9, 0x5e11610d, 0x4315b9fb,
    0x5f063c42, 0x7268c87f, 0x9f37a7ae, 0x1d0f654e, 0x797e521b, 0x537aa3bf, 0x65eb451f, 0x0df3f5eb,
    0x01c3f2c2, 0x19eac3e0, 0x1f094775, 0x4689e1a6, 0xd9133c70, 0x7152f4df, 0xf42fae5a, 0x42223845,
    0x4934f2b6, 0x05d987d4, 0xbe1ac885, 0xa7baf343, 0x069ab97d, 0x766ac555, 0xa5b1d250, 0x2e50f319,
    0x2da80a91, 0x0b70a91c, 0xa2f82aa2, 0x70becdfb, 0x7c775be0, 0x6b1d9685, 0x09b80b02, 0x249b3584,
    0x237a6f70, 0xf0c996bf, 0xdd86bf1e, 0x9c3091df, 0xcd4bb6d8, 0x24d13fd3, 0x000abe88, 0x74601c37,
    0x2c5c13f8, 0x535bd657, 0x4008b337, 0xaec5979d, 0xd8130874, 0xbbc71d4f, 0xa51d2400, 0x7e45e671,
    0x2b89c033, 0xd815fdc0, 0x2aa75e0d, 0xc8ecc992, 0xb1395e00, 0x471d05a3, 0xecf90f43, 0x6f6d2bc5,
    0x68ee68ee, 0xe77729e8, 0x66e154d4, 0x90c32209, 0xa15cbd00, 0xfa44dc68, 0xa069cc63, 0xd04cf8d8,
    0x7f861da9, 0xe826df86, 0xbd6b9d0b, 0xebdec8bb, 0x842a1433, 0x303847a1, 0x91ea4642, 0x518101c9,
    0x9c5b2c30, 0x55cbf4f3, 0xad8a798d, 0x527998bb, 0x827b0451, 0x418bf003, 0xba717032, 0x6dbd3e35,
    0xf4504c64, 0x5a597a9e, 0xfe832b83, 0x7e86a0f0, 0x19076345, 0x2b81a695, 0x8b5653d9, 0x614b54de,
    0x79d95afd, 0x3c700a3e, 0x1731c1cd, 0xf877cef5, 0xef42d9cd, 0x080e528c, 0xedbd3928, 0x6c36e469,
    0xd4a604e7, 0x626a2093, 0xc2889757, 0xe32b5cc8, 0x50e27441, 0x459392ad, 0x367d9abf, 0x97117e9b,
    0x71050b96, 0xb2fb905c, 0xa3cccae0, 0x9ebba418, 0x2ef78371, 0x0882be41, 0xaf0497dc, 0x25677c0e,
    0xa71eb5b0, 0x6836f8d8, 0x1c5d9cf9, 0x8c888c59, 0x65a1eec9, 0x9255085b, 0x24a78b7a, 0x80c72618,
    0xa24a90df, 0xcb000b20, 0xaacc8dc5, 0x5b7be4cc, 0x26ba93d2, 0x0d846d66, 0x114490a7, 0xa3ac1e45,
    0xcbc71713, 0x98826b3c, 0x321f5a36, 0x21e2c665, 0xc1c6996a, 0xce13ab6c, 0x6054fba9, 0xdcb701e0,
    0x9cb31f50, 0x363f969f, 0x0a884097, 0x0fc3c4f3, 0x553d940e, 0x125ec9bc, 0x869cb394, 0x5ca607bc,
    0x89b1a3c2, 0x78c0c084, 0x8fdf2181, 0xbefd645d, 0xfbfa46b7, 0xa06d0848, 0x47eb5b54, 0x1c508bd8,
    0x4a43a706, 0x794193bb, 0x0adf0c1a, 0x648e72d9, 0x6f1880b7, 0xbf19e321, 0x1fbb08bb, 0x592e1ac5,
    0xb20a18ec, 0x6656c3b1, 0x115a7128, 0xac47fd92, 0x5bc947cf, 0x835efbcf, 0x01045835, 0xffc0712b,
    0xe2cffa55, 0xf5876e27, 0xdde1fb0a, 0x1b4de13f, 0xda5b28df, 0x5104247a, 0xb0028914, 0x33854b88,
    0x21fcf922, 0xffcdbc76, 0x8ee3f52a, 0x252eb496, 0x93476050, 0xa9d73706, 0x99173e39, 0x82b30995,
    0xdad76827, 0x6c0e7ec8, 0x090cc013, 0x889b25dc, 0x7c0c2615, 0x2cd9717f, 0xd6afbd0a, 0xacbfdb26,
    0x249efed0, 0xbcd543ce, 0xed2090b6, 0x0d5dd803, 0x512295e7, 0x5ebde92e, 0xd2661669, 0x099f106a,
    0x5e5729e4, 0x7d028ed7, 0x7791c7ba, 0x864bee09, 0xd770c6cf, 0x7d4b6f66, 0x76a36d44, 0x3f12ae7a,
    0x35564e5b, 0xd834c062, 0xd7d84fa8, 0x6cf8aa15, 0x669d624e, 0xdb270bd9, 0xa27ba201, 0x47fbf140,
    0xed53c1d6, 0x17efdd31, 0x665acd8f, 0xaccd659c, 0xc2c2c31b, 0x5acd55fe, 0xf063b89d, 0x5704f002,
    0x50939ed4, 0x8b410676, 0x82c65f33, 0x5d2c614d, 0xdd6e7bfd, 0x599ef5c3, 0xde7932d6, 0x845d9366,
    0x23f08c5a, 0x833287a0, 0xe5e186bc, 0xf2f92653, 0x10d669b8, 0x575befb7, 0xc14cc31e, 0x242a3fe8,
    0x801bfcb7, 0x748752dc, 0x513930f8, 0x5227f96e, 0xd1d94312, 0xd3090fc3, 0xab85dbd2, 0x3c2ac6dc,
    0x3fc9dd7f, 0x09e91294, 0xf6bc7dae, 0x254079a7, 0xbc1a1ae2, 0x369d4d1c, 0x20148f7a, 0x4519db82,
    0x0c92277c, 0xe7381924, 0x9b08d01a, 0x15368835, 0xbcd5c323, 0xfc0cf35d, 0xde317191, 0x5441ca8d,
    0xe0e719db, 0xdbf47d21, 0xc652490d, 0x9dc74d60, 0xe5701b6a, 0x9e64f20f, 0xf83fcf8e, 0x533ea8f2,
    0x16641001, 0x74a5270e, 0x4850e63a, 0x4aa90692, 0x0f6cafe4, 0xec40fd88, 0x312fba8a, 0xce943861,
    0x145d98bf, 0xb428f5c6, 0x97f420c3, 0x32ce57d9, 0x5edcc226, 0xfa5e4d3a, 0x3f6e0991, 0x10eb1830,
    0x80320288, 0x835ff077, 0x9b96f56d, 0x1a7e6be9, 0xb7588655, 0xfad64698, 0x4f49dd0a, 0x9d90605f,
    0xa92cd798, 0xa7a17935, 0x2100c470, 0xaddea739, 0x1c3d2062, 0x574425c4, 0x95775827, 0x08aca600,
    0xa1a5f39b, 0x50a827cd, 0x813c5225, 0xada17c86, 0x9195e4c5, 0x2f4c87ea, 0x2645e98a, 0x03e5a133,
    0x3725303a, 0x125c083b, 0xf5e46c27, 0x45e86085, 0x5b295a4e, 0x700f8482, 0xeb9cb08f, 0xb0a5a599,
    0x36333a89, 0x8e6c8d08, 0x894273be, 0x9209808a, 0xf5e3ada1, 0xd798cf3b, 0x0da31317, 0x38a65095,
    0xb78daf4a, 0x87fcd02c, 0xbf366037, 0xced64669, 0x6fb99121, 0x12262aab, 0xb0c67037, 0xbe84909e,
    0xf289f3dd, 0x3b1b0069, 0xecf35936, 0x2662339e, 0x851474c4, 0x25778f8c, 0x30c1f10a, 0x6f3b9f58,
    0xbec21d69, 0xb28ce9e2, 0xcc6f497a, 0x31b43f9b, 0x40b98e01, 0xd0b130fb, 0x7cabb016, 0xe70f1b7d,
    0xeed1322a, 0xf3232fc8, 0x4daed3bf, 0x458a8f15, 0x024cbba5, 0xe8582165, 0x618a67c7, 0x677db8f3,
    0x0059fb4e, 0x36e24610, 0x824ac6bf, 0x7256b54b, 0xf73c2719, 0x211094b5, 0x6613d55f, 0x12a3bd67,
    0x9b594bad, 0x435aeb63, 0x4d1a8fc6, 0x83cbb81a, 0xea7f15db, 0x0c347cf0, 0xcdf35275, 0xd6221634,
    0x1ee8c458, 0x0e5190e7, 0xe4fd1d3b, 0xa0e71485, 0x15791184, 0xa9a60be8, 0x36fc773e, 0x43cb034a,
    0xb2f10e30, 0xda53a021, 0x932c276d, 0x40c6812c, 0x98082b94, 0x687b5c6c, 0x0a7b4d1e, 0xd7e78525,
    0x5475bb6d, 0xfd4be30b, 0x4c5bc2b2, 0x02fcc44e, 0x1620deca, 0x130bd278, 0x00f6f4d1, 0x54be6b5e,
    0x6151a57e, 0x5f5bba91, 0x8815fd2a, 0xf0434185, 0x9f4e67c8, 0xd060ffd6, 0x5093fa23, 0x7f6b7cd9,
    0xf386464b, 0x3a4483aa, 0x114a4dc6, 0x2febe9ce, 0xe54ecf6d, 0x05d7a19c, 0x14f55dca, 0xd4b8456c,
    0xeffc9c8c, 0xfe3604f2, 0x73c43a92, 0x27877ecb, 0x3725303a, 0x125c083b, 0xf5e46c27, 0x45e86085,
    0xcd027250, 0x807f6858, 0xf0330512, 0xa3687620, 0x5b708dd3, 0xf67e2c1b, 0x5c1f0fdb, 0xffe5b527,
    0x59d39c33, 0x991f293b, 0x573d7a8f, 0x78b3e2cc, 0x5cd31bfd, 0xf3e5b6d9, 0x1af00eec, 0xe4a56c94,
    0x6fb99121, 0x12262aab, 0xb0c67037, 0xbe84909e, 0xd0539b43, 0x8e9dcf1e, 0xfb569807, 0xa2eaf343,
    0x851474c4, 0x25778f8c, 0x30c1f10a, 0x6f3b9f58, 0x73675240, 0x0b624d05, 0xf22baabb, 0x8cbd27ab,
    0xbd258c9d, 0xd167f64f, 0x1ae921bf, 0xb4e4047c, 0x4e01053e, 0x4551590a, 0x862d9758, 0xa03d9d82,
    0x014d4cc9, 0x982f62d9, 0xc3b57ece, 0xca793dae, 0x5093f3a1, 0x270e2db8, 0x7fb63776, 0xa69fac61,
    0x16417238, 0x4df6ff46, 0x9ce234cc, 0xe3d6bf36, 0x81edf991, 0x1b742606, 0x7b8ea447, 0x70c09921,
    0x16cdf2c3, 0xd2fc6c7b, 0xf2f19d91, 0xbccb92f4, 0x49371a0a, 0x97218a8b, 0xd1ce8b1b, 0xe192e94d,
    0x23acd074, 0x9f4ecc20, 0x38075010, 0xd3c13de1, 0xbcfa34a3, 0x3a4cbf98, 0x0cdd9c97, 0xf62ce267,
    0x7976bd62, 0x8234b853, 0x3b1bcb2d, 0x20227be2, 0x587aa1ab, 0xd0def359, 0x6f72a64f, 0x97075ed2,
    0xfa0d333e, 0xa198fb3f, 0xe05ce3e9, 0x9ed40a96, 0x2f5432bb, 0x0ad32e66, 0x574884ec, 0x3ec6c2bc,
    0xfae926ce, 0x722db5a4, 0xe878efb7, 0xce332292, 0xade7a222, 0x024e22df, 0x1d23dc3f, 0xf70cd34f,
    0xb842923e, 0x97bd8e04, 0xf9dd365f, 0x3acbc167, 0xc1c6996a, 0xce13ab6c, 0x6054fba9, 0xdcb701e0,
    0x12096f83, 0x9ef619b4, 0xb6d2e507, 0xdab90bd2, 0x2310dd68, 0x17aa2dbd, 0x1dcec707, 0x4fb89ed0,
    0x05394f2f, 0xad81d524, 0xcb180005, 0x7e16bd22, 0x08db71dc, 0xd3d38213, 0xdac2615d, 0xf283b6a1,
    0x786f4fb0, 0xa1a69dae, 0xa18796eb, 0x270df262, 0xfde16692, 0xfe5b2e2d, 0xb3452ee0, 0x1e50f8a1,
    0xb55d6610, 0x604ba50a, 0x5f84a05c, 0x16726b0f, 0x2432be32, 0x0db5ea16, 0x104994e7, 0xb71a31d6,
    0x3c6b11de, 0xc18fe54d, 0x4baa9fa0, 0x1cacea16, 0xd44b39ec, 0xad55d931, 0x1f1371c5, 0x365d0b76,
    0xe9fa131d, 0xde554fe0, 0xb31a4543, 0x821ae4cd, 0xaafddc0b, 0xb6718dfa, 0x30f6ce84, 0x44947a8f,
    0xf669a080, 0x0eb99473, 0xb329e102, 0xbc164efa, 0x19076345, 0x2b81a695, 0x8b5653d9, 0x614b54de,
    0x453b08ef, 0xc8786e69, 0x5d6a3d62, 0x3c0fd290, 0xf2f15116, 0x9e6c58f3, 0x126929f8, 0x8b6fd81d,
    0x0e776a2d, 0xcbb78be0, 0xb8027a82, 0x42df4b3b, 0x18335e58, 0x7e945bd6, 0xc5babcbd, 0xdaeaf571,
    0x19405719, 0xcba3334a, 0x1ed38a2e, 0xafb9f6c1, 0x2749ae18, 0xacf630b2, 0xe879021f, 0xe547e37c,
    0x587aa1ab, 0xd0def359, 0x6f72a64f, 0x97075ed2, 0xe358c058, 0x7507c283, 0x62f4b1dc, 0xab238e4d,
    0x2f5432bb, 0x0ad32e66, 0x574884ec, 0x3ec6c2bc, 0xdb557f56, 0xdf956ac8, 0xeb71c52e, 0x836d16ce,
    0x5afae51d, 0x644d6600, 0xbce25dcc, 0x30032b35, 0x47076336, 0xe1134490, 0x22c92919, 0xa394f90a,
    0x167d516b, 0x41a32237, 0xdd0b131b, 0x60fa55a7, 0xd5ea60f1, 0x12c09f68, 0x2ac4cb00, 0x39b65a42,
    0x53657d77, 0xaad9ee3d, 0xf36b011f, 0x41c52f6a, 0x54891b3a, 0x24bc2100, 0xfe49428d, 0xfea80c1b,
    0x0240a733, 0xd19c96e2, 0xd2157571, 0x59ccdf8a, 0x7529c1f4, 0x45fd2938, 0x46dc1c52, 0x6e6ba90d,
    0xc5a6949d, 0x3160b392, 0xe0f134eb, 0x92555f2c, 0x83b664f0, 0xd05aebad, 0x126636a0, 0xb98b4004,
    0x5de39581, 0x22de0068, 0x964b5a43, 0x863c242e, 0x40ca31a1, 0xf4ba0a0f, 0x2bdf7645, 0x2c646a08,
    0x7d717125, 0xde03f638, 0x592c47cc, 0x817ba6e2, 0x3fa171c2, 0xe8103692, 0xb799ac6a, 0x037ca3c0,
    0x12d56618, 0xeab0e06b, 0x75412421, 0xad799112, 0xc404885e, 0xf069fed6, 0xaef32eac, 0x8ba2df71,
    0xa3666dff, 0x23a16c52, 0x323bea6b, 0xd68ff56d, 0xe1f1729b, 0xb6769ec8, 0xf037e9d2, 0x6db46e99,
    0xef198234, 0xf1092a64, 0xa1dd5e59, 0x36445045, 0xe27f4f22, 0x02ce5f17, 0x754b1286, 0x5b0577a4,
    0xabc7eb94, 0x3d57a48b, 0x7c3d275f, 0x56f9445e, 0x76042f43, 0x7a2219a3, 0x802ae1f6, 0x6af55e16,
    0x2bb7c50f, 0x08516300, 0x2a65bd18, 0xe694f410, 0x8a596437, 0x09d2b2bf, 0x3e272e78, 0x6a3f667c,
    0x2dd85874, 0x99424146, 0xb15e177e, 0x2ef2ceb8, 0xc8e7977e, 0xd34cac4f, 0xdc93abf2, 0x86011411,
    0x310fc3ec, 0xc4a9b431, 0x322a08df, 0x608e0262, 0x3598d419, 0x3be84af8, 0xc9b1ac0b, 0x6c238e05,
    0x13eea8eb, 0x8f0f04ab, 0xe606f644, 0x67c4311d, 0x04ac4a61, 0x2da22b11, 0xbf3c109f, 0x6ff4c97b,
    0x926df55f, 0x0b2fd9a2, 0x9c9fb176, 0xa2aded08, 0x7d598a52, 0x9a5e61f2, 0x23832831, 0x4b643975,
    0x34bf14ae, 0x2920a99e, 0xc43a1c60, 0xa1878b3e, 0x04663bdd, 0xc175479a, 0x9b1f1da3, 0x0c798dd6,
    0xd0c1c218, 0x3d6d07f3, 0xa279c1c7, 0x2ebbaa0b, 0x96c7eb74, 0xa490f7bb, 0xf6fae193, 0xe1ac655f,
    0x63cd9b01, 0x0e2639ab, 0x48c31c35, 0xd309fd45, 0xcf9598be, 0x37ba13e3, 0x724c3f26, 0x6b9da667,
    0xcba6695e, 0x00da669c, 0x69508690, 0xfff03434, 0xb1aa8b55, 0x012b3056, 0xd8ec46e9, 0xa984984b,
    0xf9c59a25, 0x648a1a87, 0x045af8b4, 0xb4aa18ab, 0xd37b3ee4, 0x02de4fba, 0x90e2072f, 0x31977a0c,
    0xe577c333, 0x2b9eeff7, 0xfd03d18f, 0x63074c4e, 0xbdbf708b, 0x32faab5b, 0xab7fdaae, 0x5ea659fd,
    0x2616270b, 0x83de0439, 0x2d9b8fbb, 0xb25a8d2e, 0x545d243d, 0x808657c9, 0x59c29c27, 0x933fc18b,
    0x3139068a, 0x85a0a636, 0xaa5d2a0c, 0xee966c45, 0xcd3ea41c, 0xea5ef480, 0x44a26a38, 0x93210bc8,
    0xfbfa46b7, 0xa06d0848, 0x47eb5b54, 0x1c508bd8, 0xa1e9a78e, 0xa4463f89, 0x22f46cdd, 0x3368a8ec,
    0x87cab7bc, 0xe45bc849, 0x2f7530d6, 0x80156286, 0x3e63d285, 0x2fce1476, 0xa7b33cc9, 0x672e9f78,
    0x5bc947cf, 0x835efbcf, 0x01045835, 0xffc0712b, 0xb779ba78, 0xf45364f8, 0x9a8b10bc, 0x364aab17,
    0xa7d779c1, 0x33655599, 0xd2c4a821, 0xddf26242, 0x21160448, 0x7da84580, 0x5f138a08, 0xbbfbb890,
    0xa74c1508, 0xa015fbce, 0xa116b615, 0x277b7633, 0x72963544, 0x1c00543c, 0xbb79f7ec, 0x04f2b4f3,
    0xf673110f, 0xc1bb826d, 0x3d51e592, 0xf38ed1ea, 0x11a85e99, 0x0ec494db, 0x99c15337, 0xb9cb38c7,
    0x42b0e984, 0xda27a637, 0xf24eb631, 0x79dcff6f, 0x5e5729e4, 0x7d028ed7, 0x7791c7ba, 0x864bee09,
    0xb2c5e988, 0xeebe2474, 0xb23e0413, 0x7a2202f6, 0x12ee24d1, 0xfdf2f08b, 0x2947d260, 0xaabfc35c,
    0xaccf5579, 0x9ef7d17d, 0x226a7e0e, 0x6f4362a1, 0x77db3b34, 0x84ba7806, 0x4f18e17d, 0xef521f3f,
    0xad5a5d78, 0xd0327428, 0x6ecf8aef, 0x2abb4ab3, 0xd42ff68b, 0x9fb2f447, 0x9af3a74c, 0x542e5ae4,
    0xa149e11b, 0x57164b63, 0xaa970668, 0xf31d3884, 0x41b5c58e, 0xff60fb2d, 0x58b1850d, 0x4524b30f,
    0x2346ab18, 0x1dd05c31, 0x84559c1f, 0x959992af, 0x1876f9e0, 0x7d9aafea, 0x08defb18, 0x0ddda72e,
    0x9968280b, 0xc8eb2100, 0x1d3486fd, 0xd0f5cfe0, 0x02e2adc6, 0xba45c263, 0x21d16b05, 0x22aefdc0,
    0x59ebdde0, 0x1797a5a3, 0xc4f2aee2, 0xd4272a5b, 0xb04285e9, 0x0e7e081e, 0xa7b7d60c, 0xa56b0b8c,
    0xf13bb615, 0x6fcca562, 0x473abf0b, 0xf639ddf8, 0xc32acfcd, 0x3ccadd1d, 0x8dfe46b5, 0xd482be5a,
    0x73e1bf42, 0x3b23e2f6, 0x41de5b82, 0x31bdbea6, 0x4960db71, 0x37596f1b, 0xcd4f0e6c, 0xb27194c2,
    0xc869fd49, 0x5b466064, 0x68dd5147, 0xb47df73f, 0xc7a2d9ab, 0x89040b90, 0x3f9c50cd, 0x30c3035e,
    0x1403f13d, 0x46cc104d, 0x2263c8ad, 0x2f8153b7, 0x8f058a69, 0xce29abaf, 0x6d02fdca, 0x6c78bd53,
    0xa71e25f6, 0xeb246914, 0xc6f67180, 0x27b7634f, 0x43ce96ca, 0x0ef5434e, 0x7b7f1062, 0x6ec1af06,
    0x4d15ada9, 0x82e4acfb, 0xae6b6632, 0x64cb4f96, 0xd14d02d1, 0x944e38d5, 0x88a48a94, 0x2f6de591,
    0x68c905a7, 0x995b0a13, 0x5f725df0, 0x06061b07, 0xcb575fc1, 0xda858f36, 0x374979f7, 0x56e039da,
    0xa9ab6ad1, 0x05638c43, 0x01ed3752, 0xa1efc0a1, 0xfa528bf1, 0xbe7ea6ce, 0xe96b51dc, 0x5a58e518,
    0x87092de6, 0xc1f71304, 0x037446c8, 0xa8a2aaca, 0xd93ffbe4, 0x0c44d7d3, 0xa7ecc0d8, 0x2ef48b5a,
    0xc012ecc6, 0xb5c3fc44, 0x58d175a1, 0x41f415e3, 0x60727422, 0x00b8e607, 0x995a94f0, 0x5637e24f,
    0xe80c98dd, 0x9351fac8, 0xb59b7db9, 0x126dca17, 0xfd4443ee, 0xc63953dd, 0x831a98b3, 0xc86033aa,
    0x981ed615, 0x9f17b143, 0x34361112, 0x3c33bab6, 0x252fec24, 0x37879803, 0xcaaf2514, 0x09fc25f7,
    0x31c32476, 0xdc7f5c22, 0x642ec087, 0xe096cbf3, 0xab1fe574, 0x182fdcaa, 0xbe5e9270, 0xd58f3cc1,
    0x5cb94824, 0x8d436292, 0x4232f86c, 0x7d0ac4f9, 0xc06c8b10, 0xf9cbe302, 0x1cffa93f, 0x2663314c,
    0x034e042d, 0x7b3b248f, 0xfc166195, 0x5c8150cb, 0xfe7ca8ca, 0x632239ad, 0xc93287e7, 0x9cf0a93b,
    0xd3ceb81b, 0xafcf6bb3, 0xb0719693, 0xcc5b76cd, 0xfc18d3b2, 0x7e56e4b7, 0xf97253d5, 0x3741199c,
    0xa20f3601, 0xf56c6af2, 0xab7546b2, 0xd0063899, 0x61d09f9c, 0x9bb22150, 0x992a57dd, 0x7989044c,
    0x7030c6fa, 0xfb4c29a3, 0x65053ee1, 0xcecf7b44, 0xc65628a8, 0xc29e00b0, 0xf2a94723, 0x10b4cc10,
    0x934b1ae6, 0xd16c81ae, 0xb963853a, 0xcd859db8, 0x0054474b, 0x13ebf3d8, 0x26988631, 0xc0e33dfd,
    0xc1eeb22b, 0x1b438ad0, 0xab2a0a2b, 0xcfca2e7d, 0x032a6b4b, 0x9a1c3c7f, 0xd279f22a, 0xc57489ac,
    0x459360e8, 0x09dfcae1, 0xe4a7f818, 0xa414ed52, 0x3ba743ee, 0x314771de, 0x32d0f5e8, 0x6f4ad80b,
    0x426e3666, 0xa97e0ba4, 0xed109c94, 0xc2382a59, 0x50004d77, 0x16021ff3, 0xb885bccc, 0xabb88fd3,
    0xbe871803, 0xccf00dc5, 0x21ec5567, 0x1b60f13e, 0xcb455d9f, 0x6bc48747, 0x858454e3, 0x5ba07874,
    0x45f8301e, 0x99632aa1, 0x442c0bb6, 0xe38e719e, 0x9a484af4, 0xb74ea604, 0x132425f7, 0x5a040cae,
    0x820de4dd, 0xf29469e7, 0x5e39d366, 0x5e210741, 0xd3e78047, 0x829186a7, 0x42fc6d0e, 0xdfdfae74,
    0x6b477b3a, 0xfac8db80, 0xaa773b2f, 0x440652b8, 0x9ac022dd, 0xf8e2a126, 0x09b8bb98, 0xb993c435,
    0xb4a2f546, 0x6aef95a7, 0x36bbe947, 0x16bdc21a, 0xd048499b, 0x803cfc7a, 0xa94fb52c, 0xaac78c14,
    0x3d8738a6, 0xa2a4cc9c, 0x9ecdfd85, 0x45d28d89, 0x8cc01f7b, 0x6d0f730e, 0x909013d2, 0xca79c2a2,
    0x2d280a19, 0x77f3374a, 0x93c12227, 0x8da0e5ab, 0x6b48b590, 0xa133d047, 0xa8ccd3c4, 0xbfabac77,
    0x27cc806b, 0x892be639, 0x89c0b438, 0x4562b181, 0xb12427cc, 0x493bb399, 0x1008c942, 0x9ac7b9e7,
    0x5ae61aee, 0x3048fb0f, 0xce9b863a, 0x5282846a, 0x3a0021d2, 0xfb3aeca6, 0xa336e594, 0x48697642,
    0x2d72246b, 0x488779e8, 0x853d976f, 0x8ce566b2, 0xd3e78047, 0x829186a7, 0x42fc6d0e, 0xdfdfae74,
    0x72631866, 0x7323002b, 0x1610c8ed, 0x71c7569a, 0xf6267517, 0x11835c03, 0x9312939d, 0xbf310c95,
    0xb4a2f546, 0x6aef95a7, 0x36bbe947, 0x16bdc21a, 0x09e0caa9, 0x0116e81a, 0xd976650d, 0x963c57e2,
    0x48945149, 0x1c320717, 0xeb667ac6, 0x1f591e60, 0xff53bb24, 0xaae41fee, 0xf0ca7e02, 0x8730b70f,
    0xc4632f7d, 0xd39d604d, 0xbf4b9816, 0x81632c19, 0xeb89b20b, 0xf4edfdfd, 0x0ccd1235, 0xe7ec8cf8,
    0x3bef0abf, 0x391b5f5a, 0xc2130cfb, 0x930e442c, 0xd11828ca, 0xfe7c605e, 0x4e7583f2, 0xc9fa943f,
    0x2a81d4b1, 0x91e2eb6b, 0xfddc551f, 0xf762ba64, 0xc756dbdc, 0x74abb1e6, 0xd8f6b02d, 0x427db064,
    0xe645ce7a, 0x27b451cc, 0x2141ec4c, 0x23452f7a, 0xf3331f13, 0x363875e3, 0x91f9a441, 0x1e92fa1f,
    0x78d14eb3, 0x491035cb, 0x77cbacf0, 0x5b38ef57, 0xb01e2b3e, 0xec703842, 0xdd67299e, 0x2e4cc3ba,
    0x646cf41b, 0xed5334f2, 0x88a5c240, 0xbe68e4cb, 0xb8632fbf, 0x7aad96b3, 0x049ccb11, 0x6f46ba00,
    0xcac5b735, 0x944a3fae, 0x51e2dd94, 0x6e666ae1, 0x6bfdcb02, 0x3fcca8a3, 0x62b406e0, 0xd6409222,
    0xec9041b8, 0x028980d3, 0x800415f2, 0x047644cb, 0x237b7883, 0x7eb75811, 0xe910fca2, 0x0ab1c335,
    0x472831a5, 0xb8bc1ec8, 0x30234d01, 0xfd2bbe5a, 0x7232fe71, 0x554435d6, 0x7d0e6dfa, 0x748cb629,
    0x05b9edea, 0x52e500db, 0xa4824b75, 0x5ccfd648, 0xb55d6610, 0x604ba50a, 0x5f84a05c, 0x16726b0f,
    0x89781266, 0xa63311d1, 0x9f4ab515, 0x26c2cb87, 0xb1b10af8, 0xb710b0c5, 0x6cac59b7, 0xfa042e25,
    0xd32a4567, 0xebd6efaf, 0x69099ec2, 0xb4ff2ee4, 0x41d55fa1, 0xf7fe5b19, 0x04c528ab, 0x1e10b184,
    0xd77f508d, 0xfb048020, 0x605db2d2, 0x7570ee68, 0xdf362ccf, 0x2d6841c5, 0x585a9ac6, 0x590227be,
    0x685782d0, 0x1951ad35, 0xc2a00d05, 0x6267892f, 0xdbee9960, 0x1ad86118, 0xabf89eba, 0x922137dc,
    0x37af8bc5, 0x4f99cba7, 0xa3f40604, 0xc35d6602, 0x7525f032, 0x5067cdcf, 0x2d264863, 0x3a414b62,
    0x5ba02192, 0x6c10f29f, 0x0611b483, 0x6f36e1fd, 0x9357077d, 0xc7ab6318, 0x5791af37, 0xd0763764,
    0x587aa1ab, 0xd0def359, 0x6f72a64f, 0x97075ed2, 0xa0fb72b2, 0xe3edd6ca, 0x59ef9a2b, 0x85d0f5b6,
    0xca836aa9, 0x37eb75d3, 0x7afb28de, 0xc6d7be47, 0x73cc0837, 0xe359b96a, 0x2337fdd1, 0x99bd923f,
    0xbde57769, 0xa7668786, 0x7562f404, 0xa9e0dc23, 0xed53c1d6, 0x17efdd31, 0x665acd8f, 0xaccd659c,
    0xf426a209, 0x383cd559, 0xe6bf3bcf, 0xa2b19d95, 0xb2e91d10, 0x6d225e5b, 0x77b9fce0, 0x39178420,
    0xc28483a2, 0x57050712, 0x0dbf7f18, 0xf202d9de, 0x5be1b1b6, 0x0c3f231f, 0x0b640ac4, 0xdcf53fa7,
    0x5398b6a4, 0x32ec7547, 0x550adf1a, 0xac2cc149, 0xe1c936f0, 0x6bb87ac9, 0x72f3b9e7, 0x1e8a907b,
    0xfd9da609, 0xc9bf6289, 0x0c3ef16a, 0xdc069ab7, 0x55a1979f, 0x7fd9aaf8, 0x8acf3bee, 0x2a8e5953,
    0xd8aca2fe, 0x067d889b, 0x246cebac, 0x6e68ee0d, 0x250c31bd, 0x3bba980a, 0x2e682171, 0x911bddbd,
    0x1e80a22f, 0xb06c2fbd, 0x0373a58b, 0x70f44cc4, 0x92563780, 0xdaa966ef, 0x6d3fc431, 0x66ed9841,
    0x19a865b0, 0xf0da54ef, 0x0259b281, 0x493f6af2, 0x79091394, 0x1ceda3f9, 0xa560ab89, 0x76ce589a,
    0x4a8347fd, 0x1c2d926b, 0xdac93fb1, 0xc1d388c0, 0xf41cf9e3, 0xa49dd6f0, 0xe642f106, 0xba965a64,
    0xefe44ba0, 0x015394be, 0x90dceac9, 0x0efb883c, 0xf544984f, 0x794159fd, 0xd9a82c1c, 0x7b25f146,
    0xa476f189, 0x5f5130fa, 0x28436cc2, 0xa85f815b, 0xffacf2a8, 0x1995d1df, 0x44d2eaac, 0x332a5b7f,
    0x0023c45d, 0x9989168a, 0xd4027e51, 0xa9ddddad, 0xd53f91b5, 0x204ff87a, 0x648392f3, 0x3a6040e7,
    0xfcfd5b75, 0x470d6e3a, 0x924215ee, 0xf90eb1a9, 0xa9bed499, 0x78c9960e, 0x3c8609b8, 0x10b6119b,
    0xa9eaf726, 0x57dd88a1, 0x3ac4fd31, 0x372c58c6, 0x871cf502, 0x982b5891, 0x1e933b84, 0x27f376f9,
    0x573db25b, 0x2eb31bc5, 0x1ac21dfe, 0x3eae00a7, 0x25f93487, 0xc8ac0287, 0xafed3696, 0xafc6ed3d,
    0x79eacebe, 0x4d233a7d, 0xbdd91c31, 0x5c58ebaa, 0x70cb77a0, 0x0161094e, 0x8076e9a5, 0xa20386a3,
    0x861994a0, 0xc8f0ad99, 0x6846bdd6, 0xd026aed2, 0x142d48a3, 0x8ca9e35a, 0x46697e79, 0x2089da52,
    0xeded9bed, 0x0c09c91b, 0xa282c347, 0x4ca61840, 0x4b280249, 0x3f93fd47, 0xead5c941, 0xc517fcdc,
    0x02de61f9, 0xc8e57978, 0xc7aa5cc6, 0xf8e22a2e, 0xf9628f71, 0x2919a9db, 0x8134bdb5, 0x9c940db6,
    0x52a759cf, 0x4d1fc940, 0x7988b54f, 0x2c07e3e1, 0x9263cb06, 0xf5fb900f, 0x15448e87, 0x9cda2ef1,
    0xc7e06ed1, 0xa386b48a, 0x11d6d4e7, 0xafc43d3b, 0x1620deca, 0x130bd278, 0x00f6f4d1, 0x54be6b5e,
    0x99e2540b, 0xbc825b22, 0x56449e11, 0x16bb6763, 0x61d5e2fc, 0x01d50b4c, 0x9bde1f6d, 0xa2acb160,
    0x6fa72a90, 0x9abb4414, 0x641c23a9, 0x305ed868, 0xaa81a101, 0xe3e79a27, 0x2f959d69, 0x478dba70,
    0x8ca5b8a5, 0x279e8602, 0x2d44025f, 0x487005b1, 0x3725303a, 0x125c083b, 0xf5e46c27, 0x45e86085,
    0x212f8dc3, 0x0bc8491c, 0x5d8e9154, 0xa52608c1, 0xd6103b75, 0x0818ae21, 0x6364667f, 0xa3691d05,
    0x4242241c, 0xc92a2704, 0xa621f532, 0xbcc892e2, 0xafdc916d, 0x535797b0, 0x2a952f1f, 0x3ed03f56,
    0x6fb99121, 0x12262aab, 0xb0c67037, 0xbe84909e, 0xa44ad469, 0xf801f20a, 0x0c029c4e, 0x9302f1bc,
    0x851474c4, 0x25778f8c, 0x30c1f10a, 0x6f3b9f58, 0x45367b56, 0x13fc084c, 0xa6237fc6, 0x4f85d3cd,
    0xedc7e424, 0xaf37555d, 0x6f53f772, 0x5635b89e, 0x539a74e7, 0x86e3fa00, 0xd15bb6b6, 0x15c5c19d,
    0x441cdace, 0x2d02099e, 0x17722cdf, 0xb013b7fc, 0x0059fb4e, 0x36e24610, 0x824ac6bf, 0x7256b54b,
    0x3fb8cc34, 0x389e991c, 0x5705f51f, 0xcc3dae6a, 0x9b594bad, 0x435aeb63, 0x4d1a8fc6, 0x83cbb81a,
    0x672725f0, 0x219af358, 0x1c3eb043, 0xde4b3d90, 0x693d5def, 0x407af60f, 0x6b5d0a29, 0x25518a03,
    0x06ed3e7e, 0xff37f280, 0x9bdd766e, 0x565c6f0d, 0x73575ff5, 0x0d64604f, 0xe898051b, 0x2dbad05e,
    0x8e16ece1, 0xf65f8e2b, 0xa27da9ac, 0x7cb160b0, 0x95ab3600, 0x18714254, 0x000762b1, 0xa0ad3672,
    0x1620deca, 0x130bd278, 0x00f6f4d1, 0x54be6b5e, 0x2fce3139, 0x090cbd34, 0x20385ca9, 0x781649fa,
    0x61d5e2fc, 0x01d50b4c, 0x9bde1f6d, 0xa2acb160, 0xacb7ecce, 0x9f3c5cce, 0xccfb0d42, 0x41ba4c07,
    0x35c688cc, 0xb6d9dece, 0xc804ab52, 0x4a90271e, 0x44c4d386, 0xba0f40d6, 0xca731917, 0x70457767,
    0x3725303a, 0x125c083b, 0xf5e46c27, 0x45e86085, 0x65ee8d75, 0x99424fd9, 0x71f45e12, 0x5d9389a6,
    0x72b75808, 0x60623757, 0xf9572e92, 0xd355b401, 0x27f63f23, 0x0b11b791, 0x921345bb, 0x14bd25fa,
    0xa6b49794, 0x2b07784b, 0xd3e4ca0a, 0x6309c7e9, 0x34b983e7, 0x244638b1, 0x0900c03b, 0xa09ee3ba,
    0x0fe04aba, 0xd554e27c, 0xe188eb64, 0xf8acc748, 0xa58dc39e, 0xac8b09a1, 0x7fa66697, 0x82b47868,
    0xea0eeedd, 0xf06eae0c, 0xe08a2df8, 0xbe77bc6e, 0xbb79b2af, 0xd2fba1fa, 0x54003568, 0x9eaa46da,
    0x431b1614, 0xa33c0e9a, 0x835ea04c, 0x1614dbc6, 0x958e61a8, 0xa043cf9d, 0xeabe5d1b, 0xcfb38b73,
    0xeb84aee8, 0x96226265, 0xc27ec53f, 0x4be81061, 0x19076345, 0x2b81a695, 0x8b5653d9, 0x614b54de,
    0x40caf805, 0x4df3289b, 0x8ab3e488, 0x454b3aee, 0xb1e5f270, 0x0c9d0b49, 0x068d2eed, 0x834982ec,
    0x3d485cbe, 0xabcfe3c1, 0x078106eb, 0x84604310, 0xcb214e2c, 0x0361d9d3, 0x44608118, 0xa96d2505,
    0x720725e9, 0x143b081c, 0x2488cf05, 0x9bb0cd3b, 0xb7d40857, 0x8ed5fe82, 0x5d869263, 0x7076cc60,
    0x587aa1ab, 0xd0def359, 0x6f72a64f, 0x97075ed2, 0x2bb10016, 0x82bea5a8, 0xa834b1f0, 0xb8c5b143,
    0x2f5432bb, 0x0ad32e66, 0x574884ec, 0x3ec6c2bc, 0xc587d597, 0x51dcfd57, 0xc1cd837c, 0xb21e1b78,
    0x10da5894, 0xd6e268ae, 0xe57edb72, 0xaf622647, 0xbd87e5e5, 0x6f353e4e, 0x3be5e0e9, 0x9b2c142c,
    0xc1c6996a, 0xce13ab6c, 0x6054fba9, 0xdcb701e0, 0x2da4e98b, 0x54c4d679, 0x03480b31, 0x515f4c77,
    0x5083e24a, 0x46be4ff4, 0x70a5664a, 0x84748a19, 0xd3407f56, 0xe615b671, 0x1f566d2c, 0x6669eefa,
    0x8ce525dd, 0x32071449, 0x1df44000, 0x9d0f75f4, 0x786f4fb0, 0xa1a69dae, 0xa18796eb, 0x270df262,
    0x175b9d75, 0x963f244c, 0x4d0e331b, 0x54fc1d20, 0xb55d6610, 0x604ba50a, 0x5f84a05c, 0x16726b0f,
    0x7e3cee4c, 0x4f1604d4, 0x0be6b18c, 0x34926307, 0x671e548c, 0x987aab02, 0x68a50a4f, 0xb064ec4b,
    0xe2f0e586, 0xd81ca716, 0xd7b5908a, 0x658633fc, 0xc894824a, 0xee233e68, 0x5a3ad428, 0x91e1f7de,
    0xaafddc0b, 0xb6718dfa, 0x30f6ce84, 0x44947a8f, 0x10fe04c0, 0x06b76f10, 0x788c2e83, 0x642703b7,
    0x19076345, 0x2b81a695, 0x8b5653d9, 0x614b54de, 0x3b3b5a01, 0x4e14db4b, 0xa04e4c40, 0x54b00317,
    0x666f1115, 0x4b1fa42d, 0xd0f2c415, 0xf8a73ed0, 0xb5f10d36, 0x732fea52, 0x1abb03f7, 0x1e7e100f,
    0x6b4628bf, 0xf624f2e9, 0x6ba2dd64, 0x6c24676a, 0x9f4e67c8, 0xd060ffd6, 0x5093fa23, 0x7f6b7cd9,
    0xb2dd192c, 0x972afeba, 0x7ef2a65a, 0xdf038de5, 0xdde446e9, 0xce9b42af, 0x9bf4fc14, 0x3ba6d872,
    0x20d72fc8, 0x6ccf754d, 0xca1ab7d7, 0x3ee85ea5, 0xb7bdc4a1, 0x8d9fa278, 0xdd561faf, 0xc60a97d0,
    0xd04b9b36, 0xee24b294, 0x5868a222, 0xbdd595e1, 0x8b582675, 0x1bb9e4f5, 0x2756824b, 0x7bba9824,
    0x96a26bf7, 0xe9dc7609, 0x1deba201, 0x59d7906e, 0xd4accb96, 0xe5328a15, 0xcf2b870a, 0xa9231a23,
    0x0e0d85ff, 0x84540ec0, 0x0b178e1e, 0x00765fa2, 0x67fbd883, 0x4358e63d, 0xe8932c1e, 0xa429b16c,
    0x1c93b647, 0x5fbfd60b, 0xb2622288, 0x9c352cd8, 0x6bc43935, 0x365e3325, 0xc90941b0, 0x374c0107,
    0x376c6d56, 0x0e88181f, 0x4c7221b9, 0xb29368b7, 0x1baf395a, 0xfdaddfe6, 0xb0dcb7d0, 0x62038c39,
    0x8f9b1192, 0x33c5a100, 0xe70ba333, 0x0cf7fd12, 0x68531454, 0xaab70c26, 0xb8fb9fd4, 0x87ed8c7e,
    0x754dff44, 0xb4aeec3c, 0x37224c78, 0x8c218e33, 0x3b4f5732, 0x8347fe4d, 0x6fc9ed61, 0x36665327,
    0x455311c2, 0x8653267c, 0x61d5ba0a, 0xd0c1adcc, 0x5a013326, 0x4b301145, 0x7f1d82f9, 0xad775deb,
    0x0ef8cad3, 0xa8857486, 0x413aedc7, 0xf7699648, 0xaa85a093, 0xbdce86ab, 0xc906fa65, 0x3b587ad3,
    0xb7a798b2, 0x7f9811b6, 0xb27ce946, 0x0a8f41b4, 0x0567bb2d, 0x4ceef4e9, 0x13bf4d5b, 0x7211bbc1,
    0x52cf66ad, 0x85c6792b, 0xe3e3eba7, 0xf7e1f0be, 0xd4739937, 0x71933907, 0xba660e97, 0xaa2c3f98,
    0x04555da7, 0x4951db65, 0x45e5fa63, 0xa8a8d5cf, 0x5bacbbd2, 0x0eb6367c, 0xdab67f6e, 0xe61b34c0,
    0xd271e5ac, 0x6d85b3a7, 0x9fe2fc39, 0x0ca61473, 0xd0b72992, 0x05a88439, 0xb0b29950, 0xc685ac78,
    0x3044811d, 0x632b00af, 0x3db33fc2, 0xa8876b99, 0x05ac106e, 0xcdfe7140, 0x48da7aac, 0xd9c4614b,
    0x5e5729e4, 0x7d028ed7, 0x7791c7ba, 0x864bee09, 0x8324648d, 0x461c37c3, 0x0cb46134, 0xdd0bf9fb,
    0xec6839a3, 0x46734025, 0x2f31901f, 0x063f3238, 0xed0872a3, 0x6183a76c, 0x16d33bcc, 0x2834609a,
    0xf8ce04d4, 0x2d3c29fb, 0x70dc0224, 0x2301c4b4, 0xfb152350, 0xe330225c, 0x27c7fc5d, 0xfec9f62a,
    0xcadf5ef9, 0x076af0ba, 0x29b978af, 0x1dea73fa, 0xb84d0480, 0xda7b873e, 0xe6dbdeb0, 0xb1281099,
    0xb572eed7, 0x5ee0075c, 0xaaa9ddc6, 0x4436057a, 0x14f0dc2a, 0xdc493808, 0x63fd2cd8, 0x68b2709d,
    0x36561147, 0x3e2532b7, 0xc7cf1de6, 0xbf23c201, 0xc531f316, 0x2f5d64e0, 0xc49796f4, 0x14d7a4b8,
    0xfe49bf39, 0x5376e304, 0xdf031d2c, 0x39058c05, 0xdac31f86, 0x55f5f342, 0x82767e15, 0xcc1c8b0b,
    0x73cc0837, 0xe359b96a, 0x2337fdd1, 0x99bd923f, 0x6e0cea40, 0x015ba9bd, 0xeca47d9a, 0xf4dce9d7,
    0xb959f7e5, 0x3af36062, 0x3b50df96, 0x2d79101d, 0x26d932f5, 0xc83a6fab, 0x20902ab5, 0x617821d2,
    0x5c39a513, 0x6029f3d4, 0x20c982b0, 0x62b466fb, 0x41abec1f, 0x92e66522, 0x27d05fd7, 0x2efd6ad7,
    0x91400d6c, 0x08fb149b, 0x3946ade8, 0xd6c8827d, 0x1651a195, 0xd33e6ea9, 0x166c397f, 0x799c1b2b,
    0xde2ee6af, 0xe3dc90c2, 0x071342f5, 0xa30e2a78, 0xcab21995, 0xbf3e7df4, 0x040fd89a, 0xa9bcb265,
    0x35623ac2, 0x60d5bcb7, 0x3d965f94, 0x346cbb4c, 0x6dcb7938, 0xf9f98ba6, 0x7ffc7654, 0x8dc3b21a,
    0x3eca8d96, 0xc9349f75, 0x157a28b3, 0xb234cf03, 0x3137535d, 0x13afef86, 0x232b5aec, 0xd5a5ddc7,
    0x505c598b, 0x39798dce, 0xe564af7a, 0x2f9d8d40, 0x9e6fb5d2, 0x640d3195, 0x8b9f6aad, 0x05c7d0aa,
    0xbdb47ad3, 0x8cb3d872, 0x90ccacb9, 0xa964f202, 0xbd0eade1, 0x3fb21fc6, 0x02fb676c, 0xbc3b8644,
    0xe73f3e5f, 0x721dbe8d, 0x7fd89259, 0x703e692f, 0x3d3b4fa2, 0x7519ff23, 0x1fdbf191, 0xb18a21d3,
    0x379d7d0c, 0xa50b3de5, 0xd27e63da, 0xad821511, 0x08760864, 0x7c36e8ce, 0xddb99b8e, 0x1ce38730,
    0xb6cbd4a4, 0x38489e67, 0x00a8474d, 0x81c6cea2, 0x3431cc28, 0xcbc92d73, 0x14e6fd05, 0x49383115,
    0xe4baad23, 0x010034a9, 0x9bb9c79c, 0x42f81cfa, 0x3e67f1a3, 0xfb98dbec, 0x65775707, 0x8df8998d,
    0x50356578, 0xf8508899, 0x41904aa3, 0x15ae057f, 0x5c4ef471, 0x944e47d6, 0xaec8ec75, 0xb187dc05,
    0xaa54362d, 0xda0dce69, 0x132dd671, 0xaee83726, 0xa9bee51f, 0x5b25179d, 0xac2a6212, 0x26bbbc01,
    0x695ea21f, 0xabfa0ee5, 0x5e88e01a, 0x502e380f, 0x7a6d028c, 0xc2841b01, 0xb9a84027, 0xee118109,
    0xe35b0a78, 0x5e38668b, 0xf29a2508, 0x3b232f71, 0x209b2166, 0xddc36288, 0x95377860, 0x7c05a27e,
    0xcf223c18, 0x2a578826, 0xfcd4fad9, 0xfcbf0da3, 0xd3d51107, 0x86adcf66, 0xe8ae6191, 0x82c7fa10,
    0x8a0f8685, 0x6f763f36, 0xca383a81, 0x6137ea7a, 0x3379d0f2, 0xb19e0dbd, 0x5fce36a5, 0x9e9eb9c0,
    0xe0b16900, 0xe0de8b7d, 0xa3dead4b, 0x2da80c42, 0xd8130874, 0xbbc71d4f, 0xa51d2400, 0x7e45e671,
    0xc0315405, 0xfc6a0b55, 0xb142e018, 0xbd99753e, 0x034e042d, 0x7b3b248f, 0xfc166195, 0x5c8150cb,
    0x7dc819b9, 0xa8dc33ef, 0x932ae93f, 0x8b7da96c, 0x2afbcf5f, 0x5043a0d3, 0x7c743153, 0x61ed6087,
    0x1d7cc149, 0x279876d5, 0x065ad952, 0x4e0993e0, 0x16bb0b2f, 0x9b493148, 0x45f93d36, 0x6cd0a15f,
    0xf6a69213, 0xa6e243af, 0x4ee55200, 0x71fdabce, 0x817a60ea, 0xcd4bf27e, 0x43031561, 0xf0a628a7,
    0x4fa54eec, 0x2d9723a4, 0x2253d0ea, 0x65b12a07, 0x6c10cff5, 0x49e84ded, 0x538a25b5, 0xb8ec7c15,
    0xdd343dcf, 0xdb6cdd97, 0x8be99b87, 0x9f7755ad, 0x4a816b99, 0xbb6528c7, 0x60557c46, 0x1b16da11,
    0xd9d7a7d1, 0xbc884595, 0xc785ee51, 0xf33e5147, 0xf3ecc4eb, 0xb1a8171e, 0x0388415c, 0xd98c6bfa,
    0x03894802, 0x8431dca3, 0xd17fa85f, 0xa57bcf46, 0x912fa139, 0xb7d8e1f0, 0x9d1872a1, 0xcfc7f028,
    0xd6b0fad6, 0xcfb39806, 0x5bf25155, 0xd3df6a29, 0x43b7116f, 0x6dfb9c67, 0xdd84ceb1, 0xa996c649,
    0x182fdd2f, 0x5f734e0c, 0x799c2a11, 0x67d5698c, 0x5f8720c6, 0x7d30f757, 0x346280e6, 0x3efae8d5,
    0xf91c7d81, 0xfe1a64fa, 0x58fada51, 0xdd47a7b9, 0xae5e00e0, 0x47532eb5, 0x28c5a074, 0x7413d3be,
    0x78987203, 0x76af9788, 0xb94868db, 0xf0bf024c, 0xc03cf65d, 0x7b1127d9, 0x0cf13197, 0x734be2cc,
    0x747322ed, 0x09da7532, 0xc1403947, 0xddd510bb, 0x67821d97, 0x325ccd51, 0xd3f1f343, 0xca19e519,
    0xc9411925, 0x92e5cd2c, 0xd0fba9b7, 0x59426a6a, 0x768d4e03, 0xe135d9a5, 0x4f64ca43, 0x75811f47,
    0xd6bd145e, 0x4adb7875, 0x5098d720, 0xa7d9fad3, 0xe6ce9ed4, 0xf597da52, 0x99e93db7, 0x22700707,
    0x3dec3b83, 0x521cee92, 0x06d5abb8, 0x5f2a9feb, 0x1ab1ad2b, 0x7d612bf8, 0xd07da197, 0xf81a10cf,
    0xb4dbbabd, 0xeb8e6582, 0x16f8f5d5, 0x57ea23fa, 0x8a63c12d, 0x52ed8ffc, 0x435bee1a, 0xcb054ae0,
    0x99fb6de5, 0x8b3e8ff3, 0x5dccf9ea, 0x763bbe97, 0x0850a879, 0x082d8bc0, 0x1bcc62d6, 0x00a0923a,
    0xf6e86b76, 0x87e73ed5, 0x9c2b66f1, 0xa803eaf1, 0x60c66124, 0x21b67a90, 0x419e8aee, 0xd83027e1,
    0x0c16aadb, 0xc06f1fd9, 0x480a331f, 0x63754290, 0x35185b26, 0x69101c84, 0xd0329a1e, 0x74d5cf14,
    0x0c6cb078, 0x37a47a76, 0x6472c8ed, 0xb12af172, 0x1e9b7acc, 0x6cd7b623, 0x6330afb1, 0x90789b8c,
    0xa6b24d87, 0xf2588578, 0x0139e3aa, 0x73d2d704, 0xde6fe29a, 0x28371386, 0xbd26d213, 0xb3ff212e,
    0xfabc86cb, 0xa63219cb, 0xce957ee4, 0x3b1961d1, 0x2ca2c32c, 0xd3152d93, 0x67ad5fc8, 0xa8c720dd,
    0x0cffcadc, 0x490bd5c5, 0xbd5e2fb8, 0xbbe2ba8d, 0x58df28c6, 0xcf6615cb, 0xee9b10a1, 0x5736ad95,
    0x683c189c, 0x0b59c16f, 0xee5f85c0, 0x110d214f, 0xad84fccb, 0x66e7ca95, 0xab3a299a, 0x0d7f8f1a,
    0xcf58a12e, 0x0e07eee8, 0x75136827, 0xfc03e486, 0x60c66124, 0x21b67a90, 0x419e8aee, 0xd83027e1,
    0x0c16aadb, 0xc06f1fd9, 0x480a331f, 0x63754290, 0x84061a62, 0x51b67bdc, 0x47ef2813, 0xeb51e8b6,
    0x30f291bb, 0x9c939439, 0x3ab8d824, 0xa37cf988, 0x5756d0c1, 0xf7535ec9, 0xf7fe0652, 0x66f5364e,
    0xa6b24d87, 0xf2588578, 0x0139e3aa, 0x73d2d704, 0xee338d3a, 0xa3c85450, 0x16f274ff, 0x47ec67bf,
    0x46fc9dbb, 0xa6d0cd1e, 0x5b351bef, 0xa61f6d81, 0x931e037d, 0xa1aaf78d, 0x3c5d1ccb, 0x120abc56,
    0x51e1d718, 0xdb07efd5, 0x3274c1b9, 0x7b5f025e, 0xf317e4e9, 0xda817d5f, 0xe8834709, 0xb6dc2872,
    0xc2dc555f, 0x8c724ceb, 0x0e7479e5, 0x1a8adf0a, 0xcd11646d, 0xe626b5b5, 0xf6599639, 0x8ca587b6,
    0x3ea0936e, 0x02fd2802, 0xa5756dab, 0x728a853b, 0xbd9e3340, 0x96f009b7, 0x12dc279f, 0xaac13256,
    0x35a92a92, 0xab1a82cf, 0xb479a927, 0x9c5efd5e, 0x67c99de9, 0xe15513c5, 0x97d01cde, 0x15929ead,
    0x5c1c2051, 0x74a9eeee, 0x8a59c85b, 0x506494f9, 0x3e544781, 0x402d6128, 0xd5c9710d, 0xf4b24202,
    0xd1a9f4cb, 0x6bc5514c, 0xde24bfe7, 0x574b46c1, 0x37d5e12a, 0xa1532818, 0x67472e85, 0xe97138c2,
    0x72c850e4, 0x54dbb682, 0x054fef24, 0xd2e8e10d, 0x51f21c8b, 0x832f25c4, 0xc4b68a28, 0x4beb0b0a,
    0x2ce23e3d, 0x5895190b, 0x1c1667a1, 0x263839d3, 0xacad12df, 0xbc4c0944, 0x1774150e, 0xeb5f0eff,
    0x79f03c60, 0xb7aa23fb, 0x38508cc5, 0xb1afb620, 0xc1c6996a, 0xce13ab6c, 0x6054fba9, 0xdcb701e0,
    0x9cb31f50, 0x363f969f, 0x0a884097, 0x0fc3c4f3, 0x55b05980, 0x03efdab2, 0x901e6937, 0xfe4844f2,
    0x732025fd, 0x5bbdf572, 0x38c0b20a, 0x87615f03, 0xfbfa46b7, 0xa06d0848, 0x47eb5b54, 0x1c508bd8,
    0x72683e6c, 0x8d010f1a, 0x0453505d, 0x86a10951, 0xe3068edc, 0xaff2bd71, 0xd0b69159, 0x30d4bad9,
    0xa15cbd00, 0xfa44dc68, 0xa069cc63, 0xd04cf8d8, 0xf63de902, 0x4a26e20b, 0x94d1ad26, 0x210e9d9d,
    0x7c82b22a, 0x23913b6e, 0xe9017701, 0xa337dc4a, 0xa2dac041, 0x171c146c, 0x4ef67ca8, 0xed1a8fb0,
    0x54a5f4c1, 0xfa9d783e, 0xdbfeeb10, 0xa549794f, 0x1c6253d5, 0x16bc8993, 0x8aff37dc, 0xefc81b97,
    0x19076345, 0x2b81a695, 0x8b5653d9, 0x614b54de, 0x79d95afd, 0x3c700a3e, 0x1731c1cd, 0xf877cef5,
    0xa33f7411, 0xccd989c7, 0x4312a3f9, 0xbaefa6f8, 0xf2b08a1b, 0xff465e9d, 0xb0291781, 0xd77f9e26,
    0x4b07ee51, 0x193ef4a8, 0x3afff82c, 0x3843abdd, 0x4a8347fd, 0x1c2d926b, 0xdac93fb1, 0xc1d388c0,
    0xfe5b36d5, 0x4412c940, 0x63f7b1bd, 0x6e1edc9f, 0x385fbaa9, 0xb0920f8b, 0x2f3c0b16, 0x3c173822,
    0xd8788c10, 0xfc04edad, 0xc48e030e, 0x92a019e9, 0xa3aed8a0, 0xa9e6f2ec, 0x94df2f1d, 0xf99481f0,
    0x0eff8bb8, 0xd6b33ead, 0x3969d2ad, 0x56559e39, 0x130c1be9, 0x69ca39f9, 0xb62a2a05, 0x3ee159db,
    0x5adf2268, 0x82d39d5d, 0xdd6f114a, 0xd288fdef, 0xa48dcef5, 0xe5d8bc60, 0x1b9c18f9, 0xa53aa0f7,
    0xa33f6aea, 0xa66450ef, 0x66f5213b, 0xb5016bc5, 0x604286d9, 0x931b8480, 0x1def378d, 0x7994ea87,
    0x3695d90f, 0x8a57c60e, 0xa0ba4518, 0xd6472ad5, 0xbb17d757, 0x26250344, 0x7cbc50d4, 0xd3376398,
    0x04141296, 0x04bd765f, 0xa4864e9d, 0xb1f019cb, 0x98be1e5c, 0x7844eacc, 0x29b10b08, 0x6cbc7211,
    0x4124aa4a, 0xbe9a4339, 0x2b03fe7f, 0x96fb174f, 0x9e7c0a1b, 0x861d3c4e, 0x330b2f22, 0x57e252d0,
    0xe8e7ab4a, 0x38d6e1d8, 0x0405ffe2, 0x75e0c02a, 0x5384af97, 0x9514b804, 0x8653e547, 0xd1b650ee,
    0xf0915935, 0x0eb429ae, 0x59b68682, 0xf7c69c19, 0x809a688f, 0xb055288b, 0x2b932b5e, 0xd42ad041,
    0xa48dcef5, 0xe5d8bc60, 0x1b9c18f9, 0xa53aa0f7, 0x3e65a03a, 0xeb03b277, 0xca0fe841, 0x563c7979,
    0x9e9fb54b, 0xed514902, 0x766dc20c, 0x997cb3e6, 0x19c7b8ce, 0xe44fae08, 0x8622a9f1, 0x6dbbf3f9,
    0x04909e3b, 0xfc3f7fc0, 0x74a157ea, 0xb41dbe6f, 0x0ff528d5, 0x36cc8482, 0x19c9e803, 0x67818978,
    0x39827ba7, 0x72a11c36, 0x0ba002e9, 0x8a01cf81, 0xbe3b0f7f, 0x76c72a6c, 0x66080182, 0x9043061d,
    0x1d4f2fdf, 0xcd57e50a, 0x3ba0f380, 0x4b19b79a, 0x0c7f2f26, 0xe3438caa, 0x4e6689f1, 0xff1d0536,
    0xb969a8e4, 0x3b820ffa, 0xad2052f1, 0x2c7f2e6a, 0xa799336d, 0x5933c49e, 0x88f99dc3, 0xea68f91b,
    0x8d30bfa7, 0x2eb304e2, 0x63c984f2, 0xca40aab9, 0x64450a07, 0x96e1ac06, 0xf3fecbf6, 0xf74a91f6,
    0x99523547, 0x4f038847, 0x109c64d9, 0x75773d9c, 0xd2b4c75a, 0x68132435, 0x5d9699d1, 0xfde6ad6f,
    0x70714f25, 0x1170015d, 0xa7e12a9a, 0xde92377f, 0x0059fb4e, 0x36e24610, 0x824ac6bf, 0x7256b54b,
    0xc766808e, 0x28da6a93, 0x555290cd, 0x0593f61e, 0x9b594bad, 0x435aeb63, 0x4d1a8fc6, 0x83cbb81a,
    0x0fca0bab, 0xc0462392, 0xca7226d7, 0xb1dc2563, 0xaf5788cd, 0xdddc43b1, 0xdcb5639c, 0x1874cc30,
    0x332b7bac, 0xba93485f, 0x2862d97d, 0x900e9f87, 0x8450e8c0, 0xea65fdc6, 0x3ce2866d, 0xa298dffe,
    0x45775489, 0x9ed374dc, 0x1ee3b7bc, 0x222d5151, 0x6daacfc7, 0xe1776070, 0xb415e1b8, 0x2822e467,
    0x1620deca, 0x130bd278, 0x00f6f4d1, 0x54be6b5e, 0x4945fb0b, 0xda4437a5, 0x627132db, 0x4d2cd6e3,
    0x61d5e2fc, 0x01d50b4c, 0x9bde1f6d, 0xa2acb160, 0x1870f285, 0x9c63aa4a, 0xbdd2ff4e, 0x53d6af01,
    0x5dd54303, 0x913552e2, 0xefc7f706, 0x0e2a65e9, 0x137c47d2, 0x792e6f62, 0xf42a6f05, 0x6b229929,
    0x3725303a, 0x125c083b, 0xf5e46c27, 0x45e86085, 0x04304e75, 0x81ccfbeb, 0xe8e928c8, 0x1a172056,
    0xdb6ed34c, 0xbe946cb8, 0x68f16792, 0x163d2c25, 0xd58143ba, 0x6bef5b3b, 0x73527a22, 0xc51be256,
    0x4c234ee7, 0xcf372d68, 0xca8ade5e, 0x080b3777, 0x6fb99121, 0x12262aab, 0xb0c67037, 0xbe84909e,
    0x78e552d4, 0x48e0bd47, 0x74a101be, 0x3b1cbe9d, 0x851474c4, 0x25778f8c, 0x30c1f10a, 0x6f3b9f58,
    0xb06509b5, 0x1c34e331, 0xb0418710, 0x189912a9, 0xb8c77eba, 0xfc555aa9, 0x530d2ffa, 0x2cbf454a,
    0x6327aeaf, 0x02f9ee9a, 0x822f702b, 0x8d3d5f7d, 0x982ee286, 0xef0290fc, 0x47073784, 0x4f8209e9,
    0xc0f427df, 0xaf626b45, 0x5fefb076, 0x16093944, 0x60ab8699, 0x3f722a74, 0x4b38d226, 0xed69af5f,
    0x9b594bad, 0x435aeb63, 0x4d1a8fc6, 0x83cbb81a, 0x7d079c64, 0x56b0a35b, 0x7b927444, 0xefb7acf4,
    0x1a8923d7, 0xb7897ddc, 0x6ad3fac6, 0xe001ff12, 0xf99e4512, 0x5bdc9db5, 0x8feaf633, 0x55b089a4,
    0x7876526f, 0x0ba94247, 0x83660271, 0xf2850905, 0x4adc8443, 0xb568153f, 0x0b400c8a, 0x299bdd25,
    0xaf42742f, 0x0d356754, 0xa40f8ee1, 0x564f4433, 0x1620deca, 0x130bd278, 0x00f6f4d1, 0x54be6b5e,
    0xb28e8018, 0xcbe47c1e, 0x445f904c, 0x38245c3f, 0x9f4e67c8, 0xd060ffd6, 0x5093fa23, 0x7f6b7cd9,
    0xe67f2029, 0xf18c1848, 0x627a0982, 0x359f00aa, 0x6f902288, 0xe72b2d20, 0xdc3ecc53, 0x947d4adc,
    0xc45cd28b, 0x5a3f218f, 0x0d416ec2, 0xefd2f525, 0xd71a0d89, 0x17cc1231, 0xc1a9db61, 0x176ba7d6,
    0x2480433f, 0xaf4ae942, 0x53a829cd, 0x1bec59c2, 0xfce5a2a3, 0x13e12353, 0x3dd1c1e3, 0x467fff84,
    0xf0df9b9b, 0x50462290, 0x57e5f879, 0xe15f28fa, 0xb012db74, 0x5584d8e4, 0x42672055, 0x4c35db19,
    0x7f0bdd9b, 0x50be1645, 0x622358dc, 0x385feae0, 0xb55d6610, 0x604ba50a, 0x5f84a05c, 0x16726b0f,
    0xddbdee96, 0xc64fc59e, 0xf6ae6eb8, 0x30b33173, 0x740769ec, 0x3f4255fd, 0x2f789f79, 0x31e93eb8,
    0x42c08621, 0x0a664e12, 0xa7f22b1c, 0x64f99264, 0x88a7110f, 0xcb0613bf, 0x9e61fe02, 0xe3a40169,
    0x958e61a8, 0xa043cf9d, 0xeabe5d1b, 0xcfb38b73, 0x8e851e20, 0xed6f33b6, 0x22d3d9a3, 0x59c35282,
    0x19076345, 0x2b81a695, 0x8b5653d9, 0x614b54de, 0x8e1a2690, 0xcf7a1cfd, 0x06a89b29, 0x13890859,
    0xd2bbe852, 0x6b61b62d, 0xf267e381, 0xd33addb3, 0x6fdb9a05, 0x0d88f2d1, 0x7ad1db7a, 0xcd165a31,
    0xced6cd90, 0x55fb257d, 0x46d4aee3, 0x9499c63d, 0xdf509ff0, 0xe5677d29, 0xa17c20c9, 0x91cb9b99,
    0xfb1d045e, 0xefb04d48, 0x7621e315, 0x8105c50b, 0x587aa1ab, 0xd0def359, 0x6f72a64f, 0x97075ed2,
    0x6f0a5c5a, 0x130e9b74, 0x96d5e66b, 0xa05218a3, 0x19b6a4d6, 0x31992d18, 0x38a64e3f, 0x34ad2280,
    0xa916c1d3, 0x52ea0b5e, 0x42ee7752, 0x63762377, 0x538b7398, 0x951a6929, 0xc0f15243, 0x85b38410,
    0x39787574, 0xdca4ec66, 0x28a40824, 0x6fb8f1e3, 0xc1c6996a, 0xce13ab6c, 0x6054fba9, 0xdcb701e0,
    0xfd24df5f, 0x9bd0e8f6, 0x0240bf9e, 0x933676ec, 0x95d5583c, 0xf2ee29ff, 0x72b8c0a6, 0xe678220f,
    0xd3407f56, 0xe615b671, 0x1f566d2c, 0x6669eefa, 0xcc3b3295, 0x0e6124e8, 0xa90b1c6c, 0xca2b64a9,
    0x786f4fb0, 0xa1a69dae, 0xa18796eb, 0x270df262, 0xa8199389, 0xb41c297b, 0xc4b76336, 0x055f8e06,
    0xb55d6610, 0x604ba50a, 0x5f84a05c, 0x16726b0f, 0x9c9da357, 0x336deca3, 0x2446fcd3, 0x1d9b7fe5,
    0x5bd1cbc9, 0xbcbca0a3, 0xd9083933, 0x6205a7f5, 0xda9ee4be, 0x5a1669f6, 0x37140877, 0x58b00583,
    0xff35173d, 0xa523cb21, 0xb3b2ec8f, 0xf047d882, 0xb42a1b8b, 0xde29ec14, 0x41997b64, 0xdf7d6c74,
    0x70b541e5, 0xb55e5666, 0x530c4824, 0xd998c34f, 0xa48dcef5, 0xe5d8bc60, 0x1b9c18f9, 0xa53aa0f7,
    0x126bc925, 0x8ff00462, 0x8105d8e9, 0x4c980935, 0x49db87d4, 0x6b0ed5c6, 0x9859f426, 0x79259397,
    0x50ba07e5, 0xbc1101e1, 0x18892dc9, 0x28696d81, 0xfb8ea01d, 0x4882cb01, 0xe0dee655, 0x95ad247a,
    0xa811ed10, 0x12a9eedd, 0xe79b8f3e, 0xf7b7a6f2, 0xf639e1d2, 0x78cab987, 0x4c988c01, 0x53d02a21,
    0x95ffd7db, 0x926d2310, 0xe9b86322, 0xa4db3848, 0xae281ddc, 0x0202716f, 0x4978f3ce, 0x651d70f5,
    0x0e387bae, 0x1dbb9d11, 0xba98c774, 0x884bde85, 0x217e3cfc, 0x45e5cec3, 0xfc23cca5, 0xb24f68c9,
    0xb3a19caf, 0xf63f73ce, 0x0bb26cd1, 0xcbd320d5, 0x20efe3b3, 0x6b6cc0c7, 0xbeccd3c4, 0x4d76be44,
    0xf7cf6338, 0x4168b84b, 0x7a7c540a, 0xb7cf5739, 0xd6ae04dc, 0x0fb58250, 0xccd6db04, 0xceaf0505,
    0xe8d7d038, 0xadcc135b, 0x0cf6a75c, 0xdb203733, 0x5d096eb6, 0x88a1325d, 0x8b5b4374, 0x3f644cc7,
    0x61fb453d, 0x92a8304f, 0xb02c254f, 0xa02bb3de, 0x654c2814, 0xfb08921c, 0x06b01ea1, 0x82f339d6,
    0xaf78a94d, 0x57839ab1, 0xa87af3f8, 0x753b241a, 0x234070fd, 0x82ff9585, 0xfb56012e, 0x5619771e,
    0x13784882, 0x4c4d086f, 0x46c5bdaa, 0x0518c860, 0x49387479, 0xa97a1670, 0x3f4dced3, 0x8d037398,
    0x9cfb89e0, 0xb2df11a3, 0x7ef257df, 0x41afec60, 0x839d3610, 0x5562b509, 0x6687fc64, 0x2bb35b2e,
    0xfafbbb86, 0x0dc9b21a, 0xd3ddd18b, 0x18cb75f6, 0x6c3e7c94, 0x398f3676, 0x8e07bdda, 0xd11ecccb,
    0xe28a6155, 0x63ed6eb0, 0x9bb6128f, 0x925b56c4, 0x12461d88, 0x367e59cd, 0x5ff21b1d, 0x34648811,
    0x71124949, 0x8ebc0ffb, 0x5cbe2f97, 0x630f3337, 0x674ae3b0, 0xc3f6f8a5, 0x0056ef22, 0xf68a5e56,
    0x06d3ef61, 0xbea7800f, 0x3b9c5ed1, 0x25f8e0e5, 0xce8c1dee, 0x54b2ff17, 0xd184370e, 0xfcbb87c5,
    0xef0f8f48, 0xf79a0854, 0xa914b632, 0xce669165, 0x2993bbf7, 0x2b2687ec, 0x76b999ab, 0xdb8196a9,
    0xca03c68f, 0x60b20ae5, 0x50ed667d, 0x3cabbca9, 0xf36e4cbb, 0x06965893, 0x17a6881f, 0x115a4ab6,
    0xc8fdabf4, 0xff053fef, 0xb3443074, 0x4d4ff2b7, 0xc754a047, 0x24047cad, 0xa96dcf18, 0x62c28448,
    0x5a784aa2, 0xad0f8a2f, 0x3f77ff7b, 0xa2d4ab79, 0x92d75173, 0xa7942620, 0xbc63a22d, 0x666ab8c4,
    0xca85256a, 0xbfe1209c, 0x09156dcd, 0xacfab7e4, 0xa9b052cb, 0x3caa17a4, 0x916e20ff, 0xf15d4f66,
    0xe5c8f300, 0xf229c847, 0x354481f1, 0xd5b5a5c0, 0x4b92afc9, 0xa9ee1537, 0x32ccf59e, 0x422226e6,
    0xc9058fc2, 0x0ffb0759, 0x0c281f24, 0xd1550020, 0x3eabc180, 0x010df4dd, 0x5eac4734, 0xc9e25b01,
    0x40fc8826, 0x7c1ba298, 0x89603776, 0x0913abbc, 0x3846321b, 0x538204d8, 0x398117bb, 0x706a7d23,
    0x4e9aabfe, 0x15c9bf0d, 0x6481ffc5, 0xbcee8cc2, 0xa79c310c, 0x2b2181ca, 0xe4056c62, 0x28ac1536,
    0x7dc5ab5e, 0xa5c96f4c, 0x22657962, 0x66070e25, 0x76192d71, 0xb0c2a808, 0x6c02690d, 0xa2df3b10,
    0x9cc675c4, 0xccf3d3ec, 0x87c58103, 0x296bdc88, 0x123f4fc8, 0x988aa30d, 0x88674bef, 0xae492962,
    0x769818e4, 0x62f591b4, 0xe951c0eb, 0x2ec4ada5, 0xb32aee64, 0xce47ae03, 0x917a01c2, 0xf55a5181,
    0xbaee7849, 0x3e6c020d, 0x54a8fac5, 0xcdbcc5c2, 0x1822c931, 0x67046c12, 0x9ce7e6c9, 0xa1b1a156,
    0x131e56f4, 0xb2e01b6d, 0xbc52f6d1, 0xe18e8717, 0xdd8ee313, 0xad3b7ce2, 0xce134aa5, 0x7fb528c8,
    0x3456ba46, 0xbb85a898, 0xf8f91474, 0x6367e29a, 0xd146b065, 0x2176242a, 0x29786336, 0xf8f203c6,
    0x1ea7d901, 0xd9342682, 0x713fe429, 0x34f22677, 0x2be8ad23, 0xd705e110, 0x07dce103, 0x1fea05de,
    0xd86af941, 0x7c447838, 0xb72253a7, 0xa2babc97, 0xb30c0316, 0x1c8e4f3e, 0x893ae6e0, 0x29bc0c04,
    0x3724d672, 0x430a24e9, 0x2a298428, 0xb76cdb97, 0x8f058a69, 0xce29abaf, 0x6d02fdca, 0x6c78bd53,
    0xa71e25f6, 0xeb246914, 0xc6f67180, 0x27b7634f, 0xc9b74a64, 0x9645c2a9, 0xe4e9ad91, 0x3ad757b8,
    0xf7518492, 0xadb03b9a, 0x81111db9, 0xe4221a29, 0xd14d02d1, 0x944e38d5, 0x88a48a94, 0x2f6de591,
    0xf466b132, 0x86745733, 0xb34d1ce6, 0x9ec46767, 0xa67e2323, 0x250523e6, 0x1ce38821, 0x18e9f354,
    0x77db3b34, 0x84ba7806, 0x4f18e17d, 0xef521f3f, 0xca73e3be, 0x6f2cc9d4, 0x9d368029, 0x5f664736,
    0xd23995ba, 0xc6d7999b, 0x14c1f882, 0x993bf9a9, 0x9ea8f329, 0x4f429be1, 0x7d6f090f, 0xe3cb32ea,
    0xc0e90e9b, 0x3efeeee0, 0xbc663f14, 0x0383588f, 0xdbb52cfe, 0xaa6a8b9f, 0x32ff48b8, 0x0442e137,
    0x1876f9e0, 0x7d9aafea, 0x08defb18, 0x0ddda72e, 0x9968280b, 0xc8eb2100, 0x1d3486fd, 0xd0f5cfe0,
    0x7342a4ea, 0x2716d929, 0x2d57fc2b, 0x1b9a54eb, 0x798e12f5, 0xe0e4d22d, 0x7e1e4445, 0x5fd454d6,
    0x91c847b0, 0x52d3d661, 0xc33b61e7, 0x1c252964, 0xd6b0fad6, 0xcfb39806, 0x5bf25155, 0xd3df6a29,
    0x16ffc598, 0x3edbe630, 0x342903c5, 0xa5b2669e, 0x705e0a6e, 0x332a6d96, 0x5a597ad7, 0xde060acc,
    0x4de95ed1, 0x7efc6661, 0xf2a0c0c3, 0x10e9c9d7, 0x0b2bcfa5, 0xba3037aa, 0xa65d722a, 0xe740f8d5,
    0x535227a0, 0xb5e08aa8, 0x7f5ec2b9, 0x2d4b8263, 0xb82351da, 0xcff26b56, 0x02ef4071, 0xbf2e2492,
    0xf88f663a, 0x11324ab7, 0xc7d47ad2, 0x8100730d, 0x3725303a, 0x125c083b, 0xf5e46c27, 0x45e86085,
    0xf1496cfb, 0xabfacf3c, 0xe6519ba9, 0x672e6ef4, 0x431920b7, 0xaa245aee, 0xe69f97ca, 0x3160963b,
    0x73a1ad06, 0xbe4c0ee6, 0x6865061e, 0x0a882e42, 0xf172f5bf, 0x2f7f1374, 0x7c610cbe, 0x25970aac,
    0xeaff030f, 0x9c4ab437, 0x670d30ed, 0xeb265db4, 0x879204fc, 0x221b81fa, 0x8fe8ecdd, 0x17105487,
    0x78d51c22, 0x74506aeb, 0x1235bbe8, 0xf78b36b4, 0x29cd37f9, 0x3b7f1bb7, 0x7c2b3746, 0x3db6f83d,
    0x6e8e474d, 0xe324b60b, 0xf3472a3b, 0x93e4bbb1, 0x93c02a3b, 0xd52b8975, 0xc5eccc0e, 0xf2a3e4a3,
    0xe06234c7, 0x63ba6b59, 0xbcdb8ec1, 0x0dca36d5, 0x6e05562b, 0xead2d04f, 0x2ba89a42, 0x63cdf465,
    0x2b6976e5, 0x999e08d7, 0x0f2bac2d, 0xa0cda1be, 0xe0f67bd2, 0xcf4a64cb, 0x83aa0919, 0x7f686c62,
    0x6175c488, 0xd9736b9e, 0x09e92a6c, 0xc19c825c, 0xc7094e16, 0x0e77bf6e, 0xde280fe8, 0xfa4a2377,
    0x342a4417, 0x0d259b0e, 0x6442a2e1, 0x4ec57d2d, 0xc874bca8, 0x219d086f, 0x7213f14f, 0x689c0699,
    0xb94f93c2, 0x1e3ebc92, 0xfbec062c, 0x85817ca9, 0x95c016f2, 0x62a76725, 0x3872e23e, 0x71cd6ff6,
    0xa6fda195, 0xd4d9de1e, 0x7248afcd, 0x8687ce5a, 0x9cbc5a44, 0x56c3e76f, 0x6c72c873, 0xa70d820a,
    0x5d444994, 0xcdae8c19, 0xf24445b0, 0xdb4c1e54, 0x60b76b8b, 0x3ebb564b, 0x37a34012, 0x9e3438d5,
    0xc39ea20e, 0x21909bf4, 0x707e29e9, 0x0c7175a2, 0x7889582d, 0x0cb3d7bc, 0x07a8a696, 0x78c0dfa5,
    0x69020c52, 0x31be3942, 0x33ba3422, 0x57770096, 0xe4924bf3, 0xac60532e, 0xa0efa870, 0xd4ccb747,
    0x90d7bc30, 0x6c556b25, 0x12347872, 0xb3f5cbb6, 0x3cdee95f, 0x58d7fb28, 0xa105dc45, 0x59e274f2,
    0xe5c4f00c, 0x81a240a4, 0x3dcd3760, 0x7f3ecadc, 0x64ab1126, 0x4d585405, 0x5c1c344d, 0xfb6168c1,
    0x8feddb1c, 0x7fc43b42, 0x4f8d2338, 0x5b674d94, 0x0029e645, 0x335966a1, 0x13893136, 0xa71e7008,
    0x6768b329, 0x802ac09a, 0x98a07980, 0x7dd2666f, 0x4d4d375b, 0x3191bd9a, 0x6f83b519, 0xe092ad47,
    0x435fd24f, 0x80a94017, 0xd8da73d3, 0xc88f4d94, 0x0168a9e7, 0xd8c1db4b, 0x97ad76a9, 0xaef9773f,
    0x6fa4f99a, 0x9df8a327, 0xb94dd024, 0x4740f020, 0x6a60591e, 0xaa6dc83f, 0x00dc2ca5, 0xe4419784,
    0x4dc6a6f0, 0xd8e76876, 0xc9aca20d, 0x80817abb, 0xe4924bf3, 0xac60532e, 0xa0efa870, 0xd4ccb747,
    0x90d7bc30, 0x6c556b25, 0x12347872, 0xb3f5cbb6, 0x9619b72f, 0xb9795dc1, 0x82923ddd, 0x2dfb7e59,
    0x10bc7270, 0x16bfbef0, 0xab797340, 0x316393ab, 0xeed8ff9f, 0x7fa958ed, 0xcff06e93, 0xbb075cf9,
    0x8feddb1c, 0x7fc43b42, 0x4f8d2338, 0x5b674d94, 0x2d037f7e, 0xb713e426, 0x9fc3b38c, 0x348e2e34,
    0x9b6da6d1, 0x6c5798a7, 0x7b2fe087, 0x402e9033, 0x14fe37d0, 0xd97d55fd, 0x97ae13df, 0x02b07007,
    0x2dacb863, 0x6a185adf, 0xcf0ee683, 0x4701d8e8, 0xee409f9b, 0x99d37d7a, 0x96e05e77, 0x90bf07a6,
    0x5806a931, 0xc8468eff, 0x508e0a45, 0x330e2b35, 0x31bd255f, 0x03401b7d, 0xbd7297ec, 0x50a1ae2a,
    0x7d97355c, 0xcad0fb10, 0xd44b87a2, 0x17d80a89, 0x610fe27b, 0xa2a09435, 0xa887b02f, 0x9bdba8aa,
    0x2e56afad, 0x2e7d5553, 0xf50736be, 0x34377b52, 0x3cc0c568, 0x570aa5f1, 0xb34fcddf, 0x45f68ab9,
    0xd14d02d1, 0x944e38d5, 0x88a48a94, 0x2f6de591, 0xdc1d9c9f, 0x2a4ae59f, 0xbde92625, 0xd3c95f57,
    0xffc89296, 0xd5612b55, 0x35aac879, 0xfeb3e800, 0xb131c195, 0xbeda34c9, 0x17631105, 0x88ef09e3,
    0x80ce2591, 0x021cd166, 0x52475969, 0xaee3da34, 0xdbee9960, 0x1ad86118, 0xabf89eba, 0x922137dc,
    0x4f3d112d, 0xd7c1c66a, 0xe8bce84f, 0x0fb6cee7, 0x71072f87, 0x320d7f7b, 0xf7f0a247, 0x036244f0,
    0xb0655d48, 0x6bee1099, 0x9eaa3760, 0xe479559d, 0x4cb37bbf, 0x253823a9, 0x976c6a16, 0x82ef32d4,
    0x587aa1ab, 0xd0def359, 0x6f72a64f, 0x97075ed2, 0x9761ab67, 0x0626e0e9, 0x8bbf8b2d, 0xdfe777a4,
    0x45695b19, 0x0f2fe18b, 0xfcda8b8e, 0xcb2009ae, 0x73cc0837, 0xe359b96a, 0x2337fdd1, 0x99bd923f,
    0xfe730dc8, 0x71d69014, 0xde23f4ad, 0x1c66b7cb, 0x8e248bde, 0x296de501, 0xd5db014f, 0x30d3faf5,
    0x6bfdcb02, 0x3fcca8a3, 0x62b406e0, 0xd6409222, 0xfa2e5b4f, 0xc0acc7ec, 0xba090aba, 0xebe284f6,
    0xda8bf3cf, 0x0a8db738, 0x8f4e5305, 0x44b9a59c, 0x8ab618eb, 0xe137d995, 0x01f42a09, 0x3c439809,
    0x0483a403, 0xebcfdb23, 0x2ea6e982, 0x248535ec, 0x9840cc5e, 0xdf3e8c71, 0xdadeb4ba, 0x5754ac51,
    0xb55d6610, 0x604ba50a, 0x5f84a05c, 0x16726b0f, 0x89781266, 0xa63311d1, 0x9f4ab515, 0x26c2cb87,
    0x0d1f174d, 0xa96a9ad9, 0x152d3832, 0x7687c91f, 0x5c850495, 0xb7d7fc70, 0x29782ff2, 0x62cef5d5,
    0x8bcae0ac, 0x777f11e0, 0x2c834d05, 0x237e0fb0, 0x81b8369c, 0x03e1b03d, 0x2a48bbda, 0xcfa92acc,
    0xff5d1137, 0x17c9a7fa, 0x79bd5c32, 0x2bb1449e, 0xa5a2123f, 0x0249086b, 0x131dd485, 0x8680baa5,
    0x8f77d355, 0x40d28ec8, 0x3177b1f6, 0x6b175fdc, 0x904cf284, 0x91936731, 0xe91dfc5d, 0x8b252d24,
    0x07ebb0d9, 0xd066b7e5, 0x7a193eb6, 0x70c5b543, 0x637ddeb3, 0x962e1e93, 0x8c1b0997, 0x6ab52fef,
    0x266b3f4d, 0x5f669fd3, 0x26033524, 0x375015c9, 0x034f6027, 0xcba4f8b4, 0xb7e571b8, 0xe421faff,
    0xee8fbfd9, 0x4ce3a6da, 0x3f11d396, 0xeadef9a1, 0xbc8bb71f, 0x85c5c6d4, 0x60951f26, 0x0b7a1043,
    0xfe5e1aed, 0x0461db35, 0x1b872ff7, 0xbf2e4aaf, 0xd0c1c218, 0x3d6d07f3, 0xa279c1c7, 0x2ebbaa0b,
    0xb8201157, 0x0860d2c9, 0x8ea79cfc, 0x28952512, 0xb20c16ec, 0xd2e04cb4, 0x30347658, 0xee7580ff,
    0xea0b2fc7, 0x50c315ba, 0x53e445dc, 0x96847ae6, 0xa18ee1dd, 0x431e4d04, 0x6681d286, 0x09494fef,
    0x78e26736, 0x446f3512, 0xde854c69, 0x162b4c87, 0x5bc947cf, 0x835efbcf, 0x01045835, 0xffc0712b,
    0x5d28d372, 0xf7d33d8a, 0x4d848118, 0xff98574b, 0xb1d7d6ab, 0x465160d2, 0xe1f4ca80, 0x345fff5b,
    0x5aa6c119, 0x231b3f27, 0xf8faf96a, 0x65b2fc66, 0x91f9071c, 0xc342c50f, 0xe180000c, 0x6728ac5c,
    0xe637b680, 0xe5bd8ce8, 0x9c7cbb0e, 0xeedd8040, 0x9f3b0026, 0x62ee851b, 0xaec51530, 0x67f54129,
    0xbaa98a93, 0xa9ac697c, 0x79cdf01e, 0x01647814, 0xe7344fce, 0x2262e3cd, 0xa0a5ead7, 0xb2d0bbd4,
    0x731f87e8, 0x419ce0ce, 0x6f33f9c6, 0x56529a0d, 0xbc3cfcca, 0x2cd57e51, 0x8a7780b1, 0x7eedb3b5,
    0x5c1bcb96, 0x13781eef, 0xd1332818, 0xac141746, 0x46f85226, 0x18a62be0, 0x5221fece, 0x6d64fbc8,
    0x2d8ee6ee, 0x4983926c, 0x8d1f0672, 0x732b0951, 0x8f1fec67, 0x80c96f95, 0x92db9b14, 0xbf8d7274,
    0xfb5d0bc3, 0x96792d5c, 0x36beb30c, 0xca236263, 0x7fa67e6b, 0x5db838a5, 0x046c4da9, 0x9510d362,
    0x9e760515, 0x2a06c2f8, 0x0461d56b, 0x5412130c, 0x0f13fc50, 0x7c4dc21c, 0x4e9fc17a, 0xc5ec3e71,
    0x2e89892a, 0xbb453d65, 0x7f1644f5, 0xba5c9abc, 0x6fb99121, 0x12262aab, 0xb0c67037, 0xbe84909e,
    0x81bf0c3c, 0xbf467ba9, 0xb4e9e5f5, 0x219e48b5, 0x851474c4, 0x25778f8c, 0x30c1f10a, 0x6f3b9f58,
    0x549b4911, 0x278d4545, 0x37c26b05, 0x9163612d, 0xcb3b3726, 0xc59ab2a3, 0xe7483f19, 0x2a28a1bf,
    0xb03da494, 0xc4b2f154, 0x301b7ff3, 0x67d13fe1, 0x5011ee48, 0x95f250b0, 0xdab6ef3d, 0x6351d4ad,
    0x0059fb4e, 0x36e24610, 0x824ac6bf, 0x7256b54b, 0x69659795, 0x5f7e2391, 0xb2ea96b7, 0x51b6accb,
    0x9b594bad, 0x435aeb63, 0x4d1a8fc6, 0x83cbb81a, 0x4ab691d1, 0x4cc8c5b6, 0x1c3ac714, 0xea87942f,
    0x02b351b2, 0xe13589e9, 0x0a543c09, 0x252eee81, 0x91a17716, 0x30c16fb9, 0x7d6f8bf0, 0x3b545f86,
    0xc6bd854e, 0x6a5e5797, 0xf2ad6c2e, 0x4141a7e0, 0x1b744194, 0xc5b84712, 0xd479a402, 0xe04263f0,
    0x91f1ce9e, 0xcb505e7e, 0x3adf964a, 0x496bf3c9, 0x1620deca, 0x130bd278, 0x00f6f4d1, 0x54be6b5e,
    0x38db16ae, 0x90071d6e, 0x5f31d543, 0xa4c9646e, 0x9f4e67c8, 0xd060ffd6, 0x5093fa23, 0x7f6b7cd9,
    0xd203f983, 0x9d777913, 0x47b997e3, 0x9454b07a, 0xa54333cf, 0x7d136156, 0xf375bd6e, 0x88b283f2,
    0x2a6f0b2c, 0xaa7eb4d2, 0x0396d8fa, 0xd1e70863, 0x3725303a, 0x125c083b, 0xf5e46c27, 0x45e86085,
    0x79cd4fd2, 0x4239e845, 0x8e72bacf, 0xca853309, 0x9cb2eedc, 0x1ae52389, 0xd788453a, 0x5b1f4261,
    0x8065be60, 0x62136739, 0x9f69a1c2, 0x6c14a173, 0x07820e7e, 0x3d130b2e, 0x298bcd53, 0xdb751701,
    0x6fb99121, 0x12262aab, 0xb0c67037, 0xbe84909e, 0x894c5f07, 0x8991982f, 0x609a66d7, 0xd8fa7eb4,
    0x851474c4, 0x25778f8c, 0x30c1f10a, 0x6f3b9f58, 0x7436311a, 0xa88b71bd, 0xac7aae5f, 0x6c0d2dbb,
    0x9c39f479, 0x62a82e6c, 0x98cd0c06, 0xc034b9de, 0xca1c506e, 0xb60b38b4, 0x23a01a6a, 0x34d1b569,
    0x3b83f258, 0xee95ee43, 0xe6ec9432, 0x76714f42, 0xc0f427df, 0xaf626b45, 0x5fefb076, 0x16093944,
    0x2ec3ca29, 0xcb0fea00, 0xcac1713c, 0x2c862db4, 0x9b594bad, 0x435aeb63, 0x4d1a8fc6, 0x83cbb81a,
    0x733fa5bc, 0xe863bebb, 0xa37aae3d, 0x891a9286, 0x77f3edbb, 0x9c0cdefe, 0xfd89e3da, 0x6ee7c134,
    0xd2401d8e, 0xbeec4106, 0xe4fee060, 0x8931aeb6, 0x82795876, 0x4aa267be, 0xe7de8485, 0xc3064aef,
    0x2d961c1d, 0x9f61d5bc, 0x88e7b61a, 0xcdfc849d, 0xad81d551, 0xd08245ef, 0x9e44a696, 0x1c807909,
    0x2e4bb743, 0x2a0ee353, 0xa032a847, 0xe3d2d13e, 0x135834a6, 0x6be96c4e, 0xeb81d8cd, 0x2d5d8ac9,
    0x8dc2853d, 0xc19bfcbc, 0xcedc3fb0, 0xc07fbc2a, 0xc5acb91e, 0x5d813a29, 0x730b9c06, 0xc87079dd,
    0x93d17a72, 0x1bf0eb20, 0x1155811c, 0xf6de7978, 0xc1c6996a, 0xce13ab6c, 0x6054fba9, 0xdcb701e0,
    0x0f3527db, 0xca3f413c, 0xc58ab43f, 0x03289208, 0xe310dad8, 0x9622bfc7, 0x70dda00a, 0xf8b3188c,
    0x05394f2f, 0xad81d524, 0xcb180005, 0x7e16bd22, 0x73007a1a, 0xc7c5307c, 0xa6b201bf, 0xd7035cca,
    0xb012db74, 0x5584d8e4, 0x42672055, 0x4c35db19, 0x1ba420c8, 0xf4aa8c0d, 0xaeb14aa6, 0x6c89b4ae,
    0xb55d6610, 0x604ba50a, 0x5f84a05c, 0x16726b0f, 0xab3ec968, 0xd68a8e7a, 0xade37465, 0x2d16c7f3,
    0x55359416, 0xa6a5967a, 0xc93cd4e4, 0x86e0fc83, 0x193a2f72, 0x63fd762e, 0x9069b1c4, 0x4b541395,
    0xa8aed9c0, 0x3dd57852, 0x16e37991, 0x85701b9c, 0x958e61a8, 0xa043cf9d, 0xeabe5d1b, 0xcfb38b73,
    0xdf0e5fbd, 0x46c5e09c, 0x58cb6c3e, 0x5209c578, 0x19076345, 0x2b81a695, 0x8b5653d9, 0x614b54de,
    0xf84bd7e7, 0x8cf3be59, 0x1ebef4f6, 0x98e38021, 0x0b6da3ba, 0x194b9fe4, 0x1fe8939e, 0x74d4e836,
    0x3192f725, 0x3888bfe0, 0x1489280e, 0x311598d4, 0xbe1af978, 0x83c82f98, 0x7f6aebcd, 0x4978b172,
    0xbb0051bf, 0xd721991f, 0x4c4f3f60, 0xbc074887, 0xc32db2f9, 0x28bd27b6, 0x659b87d7, 0xcf0b5285,
    0x587aa1ab, 0xd0def359, 0x6f72a64f, 0x97075ed2, 0x76bde486, 0x7ce2d6d5, 0xd578e09e, 0xab01a0cd,
    0x19b6a4d6, 0x31992d18, 0x38a64e3f, 0x34ad2280, 0x19b7efc1, 0x9c7c2f7a, 0xaafde1b9, 0x088d7955,
    0x26f813fd, 0x62328b51, 0x7049fcab, 0x3a40a5e1, 0x396e198f, 0xb8d6ae23, 0x742c886c, 0x0fe133e0,
    0xc1c6996a, 0xce13ab6c, 0x6054fba9, 0xdcb701e0, 0x113e6037, 0xedc65227, 0x607f01f5, 0x8a7d81b0,
    0x3a57e489, 0x5c4dbaaf, 0xf3944e99, 0xef336bb8, 0x05394f2f, 0xad81d524, 0xcb180005, 0x7e16bd22,
    0x577a624f, 0x771665d9, 0x4dc87663, 0xe3bd2cdb, 0x5800482c, 0xc51cd75b, 0x291c905f, 0xe60ee768,
    0xaff18076, 0xf7da959e, 0xe1d8ef24, 0x2087d289, 0x926df55f, 0x0b2fd9a2, 0x9c9fb176, 0xa2aded08,
    0x6536b2a7, 0x95b5ebef, 0x04be2687, 0x77a0cf96, 0xcac09de4, 0x729ee01c, 0x0abb5caa, 0xf2592fde,
    0x447f372c, 0x4ccb5bb8, 0x469affff, 0x36e8516c, 0xa9c60144, 0x1fd0d08d, 0x2fa79d4d, 0xb0693880,
    0xbbc2b414, 0x68c8ccc4, 0xdb43d1a5, 0xf60de489, 0x746ed8c7, 0xd7cf2312, 0x7613e78a, 0xc348c40c,
    0x19ec81b4, 0xf32a0b11, 0xa0a352ee, 0x5fd28ac4, 0x62e645b4, 0x37b13928, 0xded6480b, 0xddcbfbbd,
    0xb1179dc3, 0x5ea4846f, 0xb6d43ed9, 0x63f22fa2, 0x865b7dcb, 0x009c9616, 0x0b7a3e62, 0xa145085c,
    0x8f18a0ae, 0x54b3f7ee, 0xc193adf5, 0x320111e0, 0x804f90ac, 0x4d4893a1, 0xab194dcd, 0x3bfdf710,
    0xa9745b69, 0x6596c186, 0x378d9dd9, 0x0091a5e7, 0x219549ad, 0xab26c790, 0x953245b0, 0xd98e0bb2,
    0xf066c662, 0x938f84bf, 0x987449a2, 0xdbb40633, 0x1973d846, 0xc05cf004, 0x3a8e304e, 0xf02338d4,
    0x77d1ff23, 0x8dedc76f, 0x91683cf5, 0x1947fe24, 0x8ba335de, 0x3388f9f0, 0xc41a3be5, 0x27fab961,
    0xb173e407, 0x888aff74, 0x83212447, 0xd06d8a11, 0x7e1d2f5b, 0x5ff0e80e, 0xfb183bc3, 0xf4f31f4b,
    0x40e2621d, 0x5293bb4c, 0x26e5243f, 0x500b5b25, 0xde6d2b18, 0xfe025048, 0x748a025b, 0xfc53da91,
    0xcfc6fc2d, 0x470eeca9, 0xf2cf331c, 0x2da30b8e, 0x09d0c23f, 0x46ca4d7d, 0xf442b9aa, 0x4102abf3,
    0xa6b6ea4d, 0x2d73df71, 0xe710b943, 0x011a1c1a, 0x27fd980b, 0xefd89a61, 0xf3b1d3da, 0x4909ce46,
    0xba1e2df4, 0x6ce6e42f, 0xa431e1be, 0x6d7f5347, 0x769818e4, 0x62f591b4, 0xe951c0eb, 0x2ec4ada5,
    0xc969af4b, 0xbb06e7ab, 0x3fd67b17, 0x1d036c89, 0x9271ce8c, 0x95a6dbaf, 0x9e64f3fb, 0xa2a644b7,
    0xf00bc873, 0x2caec183, 0x95c792c0, 0x680ea434, 0xce007d3c, 0x378f17e9, 0x7e3be895, 0x6419ba6d,
    0xa1df8180, 0xf8d4f04a, 0x7406a693, 0x71920648, 0x0c4b884a, 0x327ab872, 0x1a6541a6, 0xda658895,
    0x57fbe110, 0x214397a2, 0xab243f75, 0x8aac1148, 0x225abab8, 0xcdf83460, 0x3126edd1, 0x6b73819a,
    0x96c3a16b, 0xdeb0a25e, 0x03d78ef0, 0x5dea34a8, 0x3e6dfd4d, 0xfcf0b4f3, 0x70ea979d, 0x261ba2ef,
    0xa6e9f8d5, 0xe96313cb, 0x231751e7, 0x68a8315a, 0x2a66fadf, 0xa2736258, 0x08a77cee, 0xbc0da0a6,
    0x49b2ac9d, 0x517fcef6, 0x039b8363, 0x1679d472, 0x044eca3f, 0x4af52006, 0x7b95bf27, 0xa40e1cce,
    0x6d344017, 0x187cc853, 0x3ed8b1a6, 0xa2dd0fb6, 0xb7344cd3, 0xb771d01c, 0xaf117055, 0xd770dea4,
    0x9ec65410, 0x1d0a5fc4, 0x1b536a86, 0x1450daea, 0xe0da172d, 0x00d3b409, 0x94fbf2bc, 0x0d20c9e0,
    0x66b20c5a, 0x1ed32bb1, 0x958eae7e, 0xfda1c5e1, 0x2eeb4f98, 0x94315a21, 0x569968bf, 0xc126f4b1,
    0xae6dc3eb, 0x0a0980a8, 0x89c9b9f2, 0xb668a352, 0xa8f859ba, 0xe4d7210d, 0xe008dad9, 0x63ecf3da,
    0xb3c3371d, 0x3239a681, 0x156ac697, 0xc7003cac, 0xfdad74d9, 0x8b0c68b0, 0xfde3f2f3, 0x919911b6,
    0xe5f33dc7, 0xde74b539, 0xc8edd931, 0x75a861c9, 0x9d4c0de8, 0x74530fb9, 0x540f8c2d, 0x17a760d6,
    0x888da776, 0xfc6a3984, 0x3ecf90e6, 0xb421a044, 0xd8adcd88, 0xe356f283, 0x940a3821, 0xff74fb31,
    0xf6fd7551, 0x6919bab8, 0x7df949b6, 0x641f30b6, 0x94cd445c, 0x0ae71cd3, 0x2f8a7584, 0xa3ad1165,
    0xd0c1c218, 0x3d6d07f3, 0xa279c1c7, 0x2ebbaa0b, 0x9e7fafc6, 0x70c3a6a3, 0xec69c5ea, 0xf15c2e11,
    0x410f185f, 0x90aa56d9, 0xdbfe99a1, 0xd2f656b4, 0xc1dd21e8, 0xf505d649, 0xb08c3b23, 0x1cff39db,
    0x4dcbe033, 0xd0b90414, 0x0e5f4abe, 0xc69d7d0d, 0x32d2e7cc, 0x64dbc723, 0x826da654, 0xbf2ed7e0,
    0xf981d9fb, 0xccb09ec0, 0x8c4e6af0, 0x8941fbee, 0x77ef028f, 0x9ea81c02, 0xafee4c7f, 0x4211e67d,
    0x093c225a, 0xd45de7ca, 0xea42e340, 0x5a97bcb2, 0xc0b2d215, 0xb291c8ec, 0x3e9776e2, 0x4d0b92a5,
    0x8a0f8685, 0x6f763f36, 0xca383a81, 0x6137ea7a, 0x5dc57607, 0x6fc7837c, 0xfa2f7982, 0x51b34e10,
    0xcfd69e17, 0xf564bb89, 0x40d35203, 0x52084689, 0xd8130874, 0xbbc71d4f, 0xa51d2400, 0x7e45e671,
    0x1d0499ae, 0xa464c609, 0x71ed1365, 0xda07472f, 0x087b9b32, 0x14838532, 0x52705c41, 0x3805ba34,
    0xbd0eade1, 0x3fb21fc6, 0x02fb676c, 0xbc3b8644, 0xfb9a2d10, 0x81865452, 0xc5edb204, 0x01df48d0,
    0xadaadf64, 0x21d5f855, 0x9b90d59f, 0x81d91f87, 0xbd4ac962, 0x43d5a01a, 0x852f76d5, 0x76019280,
    0x5dc1db41, 0xfa161aea, 0x68e19521, 0x3f909697, 0xd602ab0d, 0x15d64025, 0xc6764a0d, 0x41320432,
    0x3431cc28, 0xcbc92d73, 0x14e6fd05, 0x49383115, 0xe4baad23, 0x010034a9, 0x9bb9c79c, 0x42f81cfa,
    0x595c8d88, 0x1ec68d07, 0x9f2fa021, 0xd575cb76, 0xfeb71906, 0x15462da0, 0x9574ec29, 0x73eaf562,
    0x53060c59, 0x408409f8, 0x1f24c997, 0x800157c8, 0x1d7956b2, 0xb6450c18, 0xa35e8e5a, 0xbd70fa7a,
    0x5c468f95, 0x3835496e, 0x82d26b48, 0x5e618ff1, 0xa097d736, 0x53274ccb, 0x42246129, 0xdd5bf64a,
    0xb2fed30e, 0xfad6efee, 0xa862427e, 0x1b7e026a, 0x4ee936a3, 0x205c5f27, 0x38fa2439, 0x880e1321,
    0x54cb4b9b, 0x9e690ca1, 0x047702b0, 0x9e834eaf, 0x5abff967, 0x2ce0ec6f, 0xbccd7a68, 0xd8a6c5ab,
    0x4edc1731, 0xdfe4af12, 0x42dfc311, 0x23dc34a5, 0x1f1d9fb0, 0x76641ebb, 0x967f8840, 0x6f4f5292,
    0x25ff8f09, 0x246fc378, 0x661e4b15, 0xa3bcc8b0, 0x779a60c3, 0xad25d736, 0x8f6dce29, 0xad8b3a6a,
    0x6f9628a4, 0x28a2f4e6, 0xd0300472, 0xb7810e60, 0xb4a2f546, 0x6aef95a7, 0x36bbe947, 0x16bdc21a,
    0xf9ed556c, 0x2a65d00f, 0x52590c68, 0x5879b237, 0xd49b0f15, 0x9f7546ec, 0xe34f118a, 0x768e3e41,
    0xc23251a4, 0xe82fd53b, 0xbbff6118, 0x0cdfe74f, 0x810121db, 0x59a302e5, 0x126c539b, 0xedda860f,
    0xb80ef089, 0x90ea8943, 0xe402b2d9, 0x6d9112b6, 0xa34113e4, 0x2d52c991, 0x341ed5d6, 0x1ee76265,
    0xf61ebf70, 0x532b04b6, 0x04e35877, 0xea20736d, 0x3c1d3342, 0x4ac39dab, 0x8474ac07, 0x33e5fe06,
    0x96a885aa, 0x97f1d612, 0x90c82d22, 0x7da755a3, 0x4cba0370, 0xf1080821, 0x7e496b1d, 0x3d214863,
    0x793ae025, 0x98570c8e, 0x899056f4, 0x6d9ff495, 0x7a0c7517, 0x873569b9, 0x1d24ecff, 0x9208a78f,
    0xbd907e3d, 0xf9f35884, 0xf18b2ba6, 0xa0d88d66, 0xa592e85f, 0x1377761b, 0xcf6b73d0, 0xde92a29f,
    0x508d8bf5, 0x1d0debd6, 0x6af855b3, 0x3db75716, 0xb676feed, 0x936c4e5c, 0xa8dbaa96, 0x9f32459e,
    0xed1b31d4, 0x2dd3ab88, 0xd46b47bf, 0x4c2fbba6, 0x9c4c64ef, 0xfff4062b, 0x8438d359, 0x7c9f1cc6,
    0x000b9c1f, 0x82177b92, 0xf80f1dbd, 0x0688d2df, 0xa3a238a4, 0x2914987e, 0x7ca7922d, 0x09cf274c,
    0x31d8bf9c, 0xe92eb657, 0x1b356a48, 0x6d369db6, 0xa213eeb8, 0xe5a99129, 0x3f421880, 0x9b78a79f,
    0xdca6c016, 0x9e3539ab, 0x3bbb2ad4, 0xb5dd5a85, 0x19ff1e66, 0x84f46e8a, 0xe7dfcae3, 0xd0f386f9,
    0x8d7159fb, 0xa542eb43, 0x9606e1d8, 0xdbd81882, 0xe9839382, 0x7eb1209f, 0x0ba20c18, 0x72e3e581,
    0x3fadab76, 0x10734dee, 0x5d3c34e1, 0x85f3ce90, 0xf172f5bf, 0x2f7f1374, 0x7c610cbe, 0x25970aac,
    0x3705072b, 0x2a6c5e2e, 0xcbac1192, 0xd2bd0fba, 0x91fedb36, 0x04ee213c, 0x36dad41b, 0xddfeddd1,
    0xbdc89715, 0xda56f1ed, 0xcc741200, 0x92d10035, 0x9c5fa37b, 0x54490f4e, 0xcff0879b, 0x876db0da,
    0xfcea7520, 0x731835d7, 0xef0828aa, 0x8e159cc2, 0x015ab4da, 0x4307124b, 0xb51a4fb4, 0x1c40548c,
    0x3dda9a43, 0x4a647c95, 0x831c4983, 0xfd335dbd, 0xeddfb4f4, 0xe6eaec9b, 0x60c8e524, 0x24e9206c,
    0x130b08bf, 0x6f177b84, 0x9442c9af, 0xa48e4467, 0x19ff1e66, 0x84f46e8a, 0xe7dfcae3, 0xd0f386f9,
    0x8d7159fb, 0xa542eb43, 0x9606e1d8, 0xdbd81882, 0xcfff321e, 0x25aa993c, 0xc4264746, 0xa4f73090,
    0x74e8dfcc, 0xed7a0d0c, 0xec3be76b, 0x3b93f860, 0xf172f5bf, 0x2f7f1374, 0x7c610cbe, 0x25970aac,
    0xeb1cdee5, 0x3e8cd2e9, 0x5e11610d, 0x4315b9fb, 0x5f063c42, 0x7268c87f, 0x9f37a7ae, 0x1d0f654e,
    0x797e521b, 0x537aa3bf, 0x65eb451f, 0x0df3f5eb, 0x01c3f2c2, 0x19eac3e0, 0x1f094775, 0x4689e1a6,
    0xfcea7520, 0x731835d7, 0xef0828aa, 0x8e159cc2, 0x98ead7b4, 0xe5c9b741, 0x9af338de, 0x1fee216f,
    0x069ab97d, 0x766ac555, 0xa5b1d250, 0x2e50f319, 0x2da80a91, 0x0b70a91c, 0xa2f82aa2, 0x70becdfb,
    0x7c775be0, 0x6b1d9685, 0x09b80b02, 0x249b3584, 0x237a6f70, 0xf0c996bf, 0xdd86bf1e, 0x9c3091df,
    0xcd4bb6d8, 0x24d13fd3, 0x000abe88, 0x74601c37, 0x3d8c24cb, 0x5007ab92, 0x7f4e163a, 0xbafc476d,
    0xd8130874, 0xbbc71d4f, 0xa51d2400, 0x7e45e671, 0x2b89c033, 0xd815fdc0, 0x2aa75e0d, 0xc8ecc992,
    0xb1395e00, 0x471d05a3, 0xecf90f43, 0x6f6d2bc5, 0x68ee68ee, 0xe77729e8, 0x66e154d4, 0x90c32209,
    0xa15cbd00, 0xfa44dc68, 0xa069cc63, 0xd04cf8d8, 0x7f861da9, 0xe826df86, 0xbd6b9d0b, 0xebdec8bb,
    0x842a1433, 0x303847a1, 0x91ea4642, 0x518101c9, 0x05094285, 0xc381ef7f, 0x371dcd5e, 0x3436d2b7,
    0x827b0451, 0x418bf003, 0xba717032, 0x6dbd3e35, 0x0338bc47, 0xe4be593b, 0xd83d7d0b, 0xac8bd364,
    0x19076345, 0x2b81a695, 0x8b5653d9, 0x614b54de, 0x79d95afd, 0x3c700a3e, 0x1731c1cd, 0xf877cef5,
    0x41b349af, 0x30db35ec, 0x8bc46e56, 0x6f9aabae, 0xd4a604e7, 0x626a2093, 0xc2889757, 0xe32b5cc8,
    0x50e27441, 0x459392ad, 0x367d9abf, 0x97117e9b, 0x71050b96, 0xb2fb905c, 0xa3cccae0, 0x9ebba418,
    0x2ef78371, 0x0882be41, 0xaf0497dc, 0x25677c0e, 0xa71eb5b0, 0x6836f8d8, 0x1c5d9cf9, 0x8c888c59,
    0x65a1eec9, 0x9255085b, 0x24a78b7a, 0x80c72618, 0xa24a90df, 0xcb000b20, 0xaacc8dc5, 0x5b7be4cc,
    0x1a91fcd3, 0xc6d5c8ec, 0xb3d6593a, 0x2fc2a5f9, 0xbb45ce7f, 0x300f332d, 0x57f6792f, 0xacf4a538,
    0xc1c6996a, 0xce13ab6c, 0x6054fba9, 0xdcb701e0, 0x9cb31f50, 0x363f969f, 0x0a884097, 0x0fc3c4f3,
    0x553d940e, 0x125ec9bc, 0x869cb394, 0x5ca607bc, 0x34a9de00, 0x6b2f1775, 0xb971d683, 0x9d01525c,
    0xfbfa46b7, 0xa06d0848, 0x47eb5b54, 0x1c508bd8, 0x4a43a706, 0x794193bb, 0x0adf0c1a, 0x648e72d9,
    0x6f1880b7, 0xbf19e321, 0x1fbb08bb, 0x592e1ac5, 0xb20a18ec, 0x6656c3b1, 0x115a7128, 0xac47fd92,
    0x5bc947cf, 0x835efbcf, 0x01045835, 0xffc0712b, 0x928457a9, 0xa47717a0, 0x792eb00c, 0x0b3041b4,
    0x29ed8705, 0x3bbb3fb5, 0x1bf99117, 0x41097ea7, 0x21fcf922, 0xffcdbc76, 0x8ee3f52a, 0x252eb496,
    0x93476050, 0xa9d73706, 0x99173e39, 0x82b30995, 0xdad76827, 0x6c0e7ec8, 0x090cc013, 0x889b25dc,
    0x7c0c2615, 0x2cd9717f, 0xd6afbd0a, 0xacbfdb26, 0x249efed0, 0xbcd543ce, 0xed2090b6, 0x0d5dd803,
    0xd5256fd5, 0xa3c58ee9, 0xc25df05a, 0x4ca0332e, 0x5e5729e4, 0x7d028ed7, 0x7791c7ba, 0x864bee09,
    0xd770c6cf, 0x7d4b6f66, 0x76a36d44, 0x3f12ae7a, 0x35564e5b, 0xd834c062, 0xd7d84fa8, 0x6cf8aa15,
    0x669d624e, 0xdb270bd9, 0xa27ba201, 0x47fbf140, 0xed53c1d6, 0x17efdd31, 0x665acd8f, 0xaccd659c,
    0xc2c2c31b, 0x5acd55fe, 0xf063b89d, 0x5704f002, 0x50939ed4, 0x8b410676, 0x82c65f33, 0x5d2c614d,
    0x70c16ee1, 0xf71ef6a4, 0x3dfe1b37, 0x6a7cb395, 0x23f08c5a, 0x833287a0, 0xe5e186bc, 0xf2f92653,
    0xd8260b4b, 0x784f3b70, 0x3de9a5b5, 0x1ec6cb4a, 0x801bfcb7, 0x748752dc, 0x513930f8, 0x5227f96e,
    0xd1d94312, 0xd3090fc3, 0xab85dbd2, 0x3c2ac6dc, 0x3fc9dd7f, 0x09e91294, 0xf6bc7dae, 0x254079a7,
    0xbc1a1ae2, 0x369d4d1c, 0x20148f7a, 0x4519db82, 0x3cab2990, 0x68dda6eb, 0x103031c0, 0x9c332fe4,
    0xbcd5c323, 0xfc0cf35d, 0xde317191, 0x5441ca8d, 0xe0e719db, 0xdbf47d21, 0xc652490d, 0x9dc74d60,
    0x54a511ff, 0x8964fc12, 0x0f803833, 0x46bb8cc3, 0x5f7ece40, 0x23e4a4c0, 0x12ae2e61, 0xac933244,
    0x2b53d6a0, 0x37b613fa, 0xdf7c4f4a, 0xb6c64d42, 0x145d98bf, 0xb428f5c6, 0x97f420c3, 0x32ce57d9,
    0x574497da, 0xdc6834fc, 0x0f4cf7b2, 0x5a27c6ac, 0x80320288, 0x835ff077, 0x9b96f56d, 0x1a7e6be9,
    0x66ae96b1, 0x4d7d14b7, 0x709daae1, 0xd4d5213d, 0xa92cd798, 0xa7a17935, 0x2100c470, 0xaddea739,
    0xb2d0ca71, 0x7f71c9ea, 0xe0bcf23e, 0x9f87c0c2, 0xa1a5f39b, 0x50a827cd, 0x813c5225, 0xada17c86,
    0x3c221738, 0x1e19814d, 0xa66d9728, 0xa5e485dc, 0x3725303a, 0x125c083b, 0xf5e46c27, 0x45e86085,
    0x80654f39, 0x26cb09af, 0xa42d8e42, 0x702e4a72, 0x36333a89, 0x8e6c8d08, 0x894273be, 0x9209808a,
    0xf5e3ada1, 0xd798cf3b, 0x0da31317, 0x38a65095, 0x63c76ee2, 0xde269d91, 0xf2759121, 0x0dcc0a95,
    0x6fb99121, 0x12262aab, 0xb0c67037, 0xbe84909e, 0xd390e5a1, 0x84588ee2, 0xb7d6d230, 0x15ab19d7,
    0x851474c4, 0x25778f8c, 0x30c1f10a, 0x6f3b9f58, 0x0d553b60, 0x82494848, 0x8e82206f, 0x5ea8c62d,
    0x40b98e01, 0xd0b130fb, 0x7cabb016, 0xe70f1b7d, 0x9539aaaa, 0xc63c9568, 0x51595eb8, 0xe15b7b66,
    0x024cbba5, 0xe8582165, 0x618a67c7, 0x677db8f3, 0x0059fb4e, 0x36e24610, 0x824ac6bf, 0x7256b54b,
    0x6a98929e, 0x5ac366ce, 0x2ae21842, 0x1c38ca59, 0x9b594bad, 0x435aeb63, 0x4d1a8fc6, 0x83cbb81a,
    0xa7f0e520, 0xa6a273e1, 0xb398a7d4, 0x99c8055c, 0x1ee8c458, 0x0e5190e7, 0xe4fd1d3b, 0xa0e71485,
    0x41aedb79, 0x49b915c5, 0xecb711b5, 0x6ed7d791, 0xb2f10e30, 0xda53a021, 0x932c276d, 0x40c6812c,
    0xdb4e34c5, 0xf0745ef6, 0x9a908746, 0xcb08cbe4, 0xddd13c1d, 0xc591ee71, 0x3f6a8bc5, 0xa23cd625,
    0x1620deca, 0x130bd278, 0x00f6f4d1, 0x54be6b5e, 0x368f37af, 0xe6019819, 0x6127b2de, 0xcf2d097e,
    0x9f4e67c8, 0xd060ffd6, 0x5093fa23, 0x7f6b7cd9, 0x00da471e, 0x1c1b877c, 0x19668906, 0xadd6445e,
    0xe54ecf6d, 0x05d7a19c, 0x14f55dca, 0xd4b8456c, 0xa6bcb531, 0xc84297a7, 0x2b86864d, 0xcf7685f4,
    0x3725303a, 0x125c083b, 0xf5e46c27, 0x45e86085, 0xd1ced8ed, 0x3b0d2abb, 0xa56b054b, 0x1ac34d12,
    0x5b708dd3, 0xf67e2c1b, 0x5c1f0fdb, 0xffe5b527, 0x59d39c33, 0x991f293b, 0x573d7a8f, 0x78b3e2cc,
    0xf7d106d5, 0x7373b964, 0xd1f5cc0a, 0xbb9137eb, 0x6fb99121, 0x12262aab, 0xb0c67037, 0xbe84909e,
    0x8ba01559, 0xd176b833, 0x01ebe92d, 0xd4d6852f, 0x851474c4, 0x25778f8c, 0x30c1f10a, 0x6f3b9f58,
    0x1617f17b, 0x431041cc, 0x188128ab, 0x635f929c, 0xbd258c9d, 0xd167f64f, 0x1ae921bf, 0xb4e4047c,
    0x97ce554e, 0x33a2f7ac, 0x7fcbab99, 0x7959c76f, 0x3b315564, 0x43a2f04b, 0xf8870c2b, 0xd40f3cb7,
    0x663ac1b5, 0x93ab00f0, 0x21b0a776, 0xb407a331, 0xc854869b, 0x113404f4, 0x128a89fb, 0x2a55f25b,
    0xaf1dcb3b, 0xeee17527, 0x4f52b0a6, 0xb3adb194, 0x16cdf2c3, 0xd2fc6c7b, 0xf2f19d91, 0xbccb92f4,
    0x3ad76c84, 0x32f98743, 0x57b8bb24, 0x370fa2aa, 0x23acd074, 0x9f4ecc20, 0x38075010, 0xd3c13de1,
    0xbcfa34a3, 0x3a4cbf98, 0x0cdd9c97, 0xf62ce267, 0xd814f300, 0xe60ca615, 0xf3c73d75, 0x7ec3591b,
    0x587aa1ab, 0xd0def359, 0x6f72a64f, 0x97075ed2, 0x31a428df, 0x47c016ae, 0x00222215, 0x2a8601cb,
    0x2f5432bb, 0x0ad32e66, 0x574884ec, 0x3ec6c2bc, 0xbb432f3f, 0x9fafed80, 0x7237ab71, 0x57c842e9,
    0xade7a222, 0x024e22df, 0x1d23dc3f, 0xf70cd34f, 0xd28999ea, 0x0c83a1e5, 0x56a732b9, 0xfff78729,
    0xc1c6996a, 0xce13ab6c, 0x6054fba9, 0xdcb701e0, 0x5b19f3ad, 0x1af67f34, 0x7c9cd68f, 0x224e851e,
    0x2310dd68, 0x17aa2dbd, 0x1dcec707, 0x4fb89ed0, 0x05394f2f, 0xad81d524, 0xcb180005, 0x7e16bd22,
    0xa4164351, 0xb05f1110, 0xafff2040, 0xf923aec7, 0x786f4fb0, 0xa1a69dae, 0xa18796eb, 0x270df262,
    0xc4b752a0, 0x5a101aec, 0x3e1ee881, 0x5fc7b71f, 0xb55d6610, 0x604ba50a, 0x5f84a05c, 0x16726b0f,
    0xe68c7a75, 0x9c4075f9, 0x82cf7520, 0x26f36934, 0x3c6b11de, 0xc18fe54d, 0x4baa9fa0, 0x1cacea16,
    0xea361b18, 0xe5aca908, 0xf516b8d8, 0x86829703, 0x0ac50d99, 0xcce57ace, 0x193bc8a6, 0x4b66b2cf,
    0xaafddc0b, 0xb6718dfa, 0x30f6ce84, 0x44947a8f, 0x0d262b63, 0x0b76e41f, 0x8f566d50, 0x81cd2e3a,
    0x19076345, 0x2b81a695, 0x8b5653d9, 0x614b54de, 0x5d6e6b8a, 0xf1efad25, 0x7024719b, 0x8cc887a6,
    0xf2f15116, 0x9e6c58f3, 0x126929f8, 0x8b6fd81d, 0xcedd794a, 0xdfa79d0c, 0x0ef8deea, 0x917ef1a7,
    0x18335e58, 0x7e945bd6, 0xc5babcbd, 0xdaeaf571, 0x19405719, 0xcba3334a, 0x1ed38a2e, 0xafb9f6c1,
    0x26558f11, 0xd87316c1, 0x4cb6c7f8, 0x68e87329, 0x587aa1ab, 0xd0def359, 0x6f72a64f, 0x97075ed2,
    0x8cb16a92, 0xb0db1dc7, 0xe363dc86, 0x84c54cd1, 0x2f5432bb, 0x0ad32e66, 0x574884ec, 0x3ec6c2bc,
    0x2739a60f, 0xca7368dc, 0x6506fd27, 0x48dbd76a, 0x5afae51d, 0x644d6600, 0xbce25dcc, 0x30032b35,
    0x47076336, 0xe1134490, 0x22c92919, 0xa394f90a, 0xce4a6bd8, 0x5b5b07d5, 0xbcc58f3c, 0x94dcb683,
    0xd5ea60f1, 0x12c09f68, 0x2ac4cb00, 0x39b65a42, 0xe9a6057b, 0x195b9112, 0xcc077ddc, 0x7a45ef98,
    0x54891b3a, 0x24bc2100, 0xfe49428d, 0xfea80c1b, 0x61c737ae, 0x6f5f95c7, 0x51c4414d, 0xbc69a61e,
    0xa1164934, 0x92dd6366, 0x8e89be31, 0x27bffc41, 0xc5a6949d, 0x3160b392, 0xe0f134eb, 0x92555f2c,
    0x94dee2a9, 0x50c0e4fe, 0x8e411a7d, 0x89ea887d, 0xfa9fd1d7, 0xaecb6e40, 0xec3ac99e, 0x5307e65c,
    0x60eb316c, 0x3261c5a5, 0x293506c1, 0x28e5f21f, 0x9a1f5fae, 0xc66bf602, 0x7019bc4b, 0x03098693,
    0x84dc3740, 0x5d7ed1e3, 0x88bcbaa0, 0xa4b644f7, 0x12d56618, 0xeab0e06b, 0x75412421, 0xad799112,
    0xc404885e, 0xf069fed6, 0xaef32eac, 0x8ba2df71, 0xb68a166b, 0x1c2f63c4, 0x67200b68, 0xf3a8b29a,
    0xa120cc0d, 0x19d7d9a8, 0xc8075b09, 0x6dcbbda7, 0xef198234, 0xf1092a64, 0xa1dd5e59, 0x36445045,
    0x72061cb6, 0x404feb57, 0xdfb8f383, 0x74a31b6d, 0xabc7eb94, 0x3d57a48b, 0x7c3d275f, 0x56f9445e,
    0x76042f43, 0x7a2219a3, 0x802ae1f6, 0x6af55e16, 0x2bb7c50f, 0x08516300, 0x2a65bd18, 0xe694f410,
    0x8a596437, 0x09d2b2bf, 0x3e272e78, 0x6a3f667c, 0x2dd85874, 0x99424146, 0xb15e177e, 0x2ef2ceb8,
    0xd8d84830, 0x12b051bd, 0x34e04b61, 0xa9bba72a, 0x310fc3ec, 0xc4a9b431, 0x322a08df, 0x608e0262,
    0xdb9c7637, 0xcd5d76e3, 0x63ed7a03, 0x6d3fcf91, 0x8828576f, 0x286117a6, 0xd54956d9, 0x3fa61184,
    0x3991341a, 0xc564f161, 0x579457af, 0x17eeda79, 0x926df55f, 0x0b2fd9a2, 0x9c9fb176, 0xa2aded08,
    0x7d598a52, 0x9a5e61f2, 0x23832831, 0x4b643975, 0x34bf14ae, 0x2920a99e, 0xc43a1c60, 0xa1878b3e,
    0xdbbb3c63, 0x3084fce8, 0xc3c01d4e, 0xe946ff3c, 0xd0c1c218, 0x3d6d07f3, 0xa279c1c7, 0x2ebbaa0b,
    0x96c7eb74, 0xa490f7bb, 0xf6fae193, 0xe1ac655f, 0x63cd9b01, 0x0e2639ab, 0x48c31c35, 0xd309fd45,
    0xcf9598be, 0x37ba13e3, 0x724c3f26, 0x6b9da667, 0xcba6695e, 0x00da669c, 0x69508690, 0xfff03434,
    0x8837c99f, 0xda97f9c0, 0x64a69052, 0xb6ac98fa, 0xf9c59a25, 0x648a1a87, 0x045af8b4, 0xb4aa18ab,
    0x07181bd6, 0xd1c1559a, 0x4f42ad10, 0xc90872a5, 0xe577c333, 0x2b9eeff7, 0xfd03d18f, 0x63074c4e,
    0xbdbf708b, 0x32faab5b, 0xab7fdaae, 0x5ea659fd, 0x2616270b, 0x83de0439, 0x2d9b8fbb, 0xb25a8d2e,
    0x545d243d, 0x808657c9, 0x59c29c27, 0x933fc18b, 0x3139068a, 0x85a0a636, 0xaa5d2a0c, 0xee966c45,
    0x1fb0241a, 0xec9ee926, 0xbd74582b, 0xc6db7661, 0xfbfa46b7, 0xa06d0848, 0x47eb5b54, 0x1c508bd8,
    0xa1e9a78e, 0xa4463f89, 0x22f46cdd, 0x3368a8ec, 0x87cab7bc, 0xe45bc849, 0x2f7530d6, 0x80156286,
    0x3e63d285, 0x2fce1476, 0xa7b33cc9, 0x672e9f78, 0x5bc947cf, 0x835efbcf, 0x01045835, 0xffc0712b,
    0xfb90a3fc, 0x28456819, 0x3ba51ddf, 0x300cfcb4, 0xb693256e, 0x862b727d, 0x4546ef74, 0x867766c6,
    0x21160448, 0x7da84580, 0x5f138a08, 0xbbfbb890, 0xa74c1508, 0xa015fbce, 0xa116b615, 0x277b7633,
    0x72963544, 0x1c00543c, 0xbb79f7ec, 0x04f2b4f3, 0xf673110f, 0xc1bb826d, 0x3d51e592, 0xf38ed1ea,
    0x11a85e99, 0x0ec494db, 0x99c15337, 0xb9cb38c7, 0xaf40fc68, 0x6b21cd1d, 0xd55acb81, 0xada27801,
    0x5e5729e4, 0x7d028ed7, 0x7791c7ba, 0x864bee09, 0xb2c5e988, 0xeebe2474, 0xb23e0413, 0x7a2202f6,
    0x12ee24d1, 0xfdf2f08b, 0x2947d260, 0xaabfc35c, 0xaccf5579, 0x9ef7d17d, 0x226a7e0e, 0x6f4362a1,
    0x77db3b34, 0x84ba7806, 0x4f18e17d, 0xef521f3f, 0xad5a5d78, 0xd0327428, 0x6ecf8aef, 0x2abb4ab3,
    0xd42ff68b, 0x9fb2f447, 0x9af3a74c, 0x542e5ae4, 0x6ef1b839, 0x40e74b37, 0xdf3d0abd, 0x4d521144,
    0x41b5c58e, 0xff60fb2d, 0x58b1850d, 0x4524b30f, 0x286ae1d2, 0xd2f3aa7e, 0x9f5f83eb, 0xc943ffb1,
    0x1876f9e0, 0x7d9aafea, 0x08defb18, 0x0ddda72e, 0x9968280b, 0xc8eb2100, 0x1d3486fd, 0xd0f5cfe0,
    0x0f908cb7, 0xb4dd1711, 0xca7edf20, 0x364036df, 0x59ebdde0, 0x1797a5a3, 0xc4f2aee2, 0xd4272a5b,
    0xb04285e9, 0x0e7e081e, 0xa7b7d60c, 0xa56b0b8c, 0xf13bb615, 0x6fcca562, 0x473abf0b, 0xf639ddf8,
    0xc32acfcd, 0x3ccadd1d, 0x8dfe46b5, 0xd482be5a, 0x73e1bf42, 0x3b23e2f6, 0x41de5b82, 0x31bdbea6,
    0x4960db71, 0x37596f1b, 0xcd4f0e6c, 0xb27194c2, 0xc869fd49, 0x5b466064, 0x68dd5147, 0xb47df73f,
    0x583153be, 0x552a5ac1, 0x1ff3cde3, 0x91d2a1d9, 0x48d14df0, 0x473bc41a, 0xa33fe506, 0xd990e61b,
    0x8f058a69, 0xce29abaf, 0x6d02fdca, 0x6c78bd53, 0xa71e25f6, 0xeb246914, 0xc6f67180, 0x27b7634f,
    0x43ce96ca, 0x0ef5434e, 0x7b7f1062, 0x6ec1af06, 0x35ddc711, 0x6a089bc2, 0x7aec7438, 0xf752fca8,
    0xd14d02d1, 0x944e38d5, 0x88a48a94, 0x2f6de591, 0x68c905a7, 0x995b0a13, 0x5f725df0, 0x06061b07,
    0xcb575fc1, 0xda858f36, 0x374979f7, 0x56e039da, 0xa9ab6ad1, 0x05638c43, 0x01ed3752, 0xa1efc0a1,
    0xfa528bf1, 0xbe7ea6ce, 0xe96b51dc, 0x5a58e518, 0x5fcf7104, 0x19615016, 0x5649d2d9, 0x1cf4dfff,
    0x9c764002, 0x2e4f5080, 0x3ed30ca0, 0x2be840f6, 0x5a46e774, 0xbe32a7bb, 0x4a217d5f, 0x9e47337e,
    0x60727422, 0x00b8e607, 0x995a94f0, 0x5637e24f, 0xe80c98dd, 0x9351fac8, 0xb59b7db9, 0x126dca17,
    0xfd4443ee, 0xc63953dd, 0x831a98b3, 0xc86033aa, 0x981ed615, 0x9f17b143, 0x34361112, 0x3c33bab6,
    0xf3a8baf6, 0x3ac0967a, 0xb1871c1f, 0xa20ba88d, 0x31c32476, 0xdc7f5c22, 0x642ec087, 0xe096cbf3,
    0xab1fe574, 0x182fdcaa, 0xbe5e9270, 0xd58f3cc1, 0x5cb94824, 0x8d436292, 0x4232f86c, 0x7d0ac4f9,
    0xc06c8b10, 0xf9cbe302, 0x1cffa93f, 0x2663314c, 0x034e042d, 0x7b3b248f, 0xfc166195, 0x5c8150cb,
    0xfe7ca8ca, 0x632239ad, 0xc93287e7, 0x9cf0a93b, 0xd3ceb81b, 0xafcf6bb3, 0xb0719693, 0xcc5b76cd,
    0x8e04c826, 0x179d7522, 0x9b0a5000, 0x1a074f1a, 0xa20f3601, 0xf56c6af2, 0xab7546b2, 0xd0063899,
    0xfc4ed4f4, 0xda16ec25, 0xd1aa29ab, 0x636415f6, 0x7030c6fa, 0xfb4c29a3, 0x65053ee1, 0xcecf7b44,
    0xc65628a8, 0xc29e00b0, 0xf2a94723, 0x10b4cc10, 0x934b1ae6, 0xd16c81ae, 0xb963853a, 0xcd859db8,
    0x0054474b, 0x13ebf3d8, 0x26988631, 0xc0e33dfd, 0xbadbdde5, 0xb637b3b9, 0x3b455109, 0x1d7d411b,
    0x032a6b4b, 0x9a1c3c7f, 0xd279f22a, 0xc57489ac, 0x459360e8, 0x09dfcae1, 0xe4a7f818, 0xa414ed52,
    0x560569c2, 0x174fc088, 0x924155fe, 0x8d8d9d5e, 0x426e3666, 0xa97e0ba4, 0xed109c94, 0xc2382a59,
    0x76bfbb06, 0xee821448, 0x8ea69633, 0xafc6ea3c, 0xbe871803, 0xccf00dc5, 0x21ec5567, 0x1b60f13e,
    0xcb455d9f, 0x6bc48747, 0x858454e3, 0x5ba07874, 0x45f8301e, 0x99632aa1, 0x442c0bb6, 0xe38e719e,
    0x9a484af4, 0xb74ea604, 0x132425f7, 0x5a040cae, 0x820de4dd, 0xf29469e7, 0x5e39d366, 0x5e210741,
    0xd3e78047, 0x829186a7, 0x42fc6d0e, 0xdfdfae74, 0x6b477b3a, 0xfac8db80, 0xaa773b2f, 0x440652b8,
    0xb8c32afc, 0x8a7119dc, 0xade6a21b, 0xfd981196, 0xb4a2f546, 0x6aef95a7, 0x36bbe947, 0x16bdc21a,
    0xd048499b, 0x803cfc7a, 0xa94fb52c, 0xaac78c14, 0x3d8738a6, 0xa2a4cc9c, 0x9ecdfd85, 0x45d28d89,
    0x8cc01f7b, 0x6d0f730e, 0x909013d2, 0xca79c2a2, 0x2d280a19, 0x77f3374a, 0x93c12227, 0x8da0e5ab,
    0x6a0c9bbb, 0xfbe4f166, 0x1955703b, 0x88fb0186, 0x27cc806b, 0x892be639, 0x89c0b438, 0x4562b181,
    0x79f73e68, 0x7621788f, 0xabdd9dd7, 0xd00b4b38, 0x5ae61aee, 0x3048fb0f, 0xce9b863a, 0x5282846a,
    0x3a0021d2, 0xfb3aeca6, 0xa336e594, 0x48697642, 0x2d72246b, 0x488779e8, 0x853d976f, 0x8ce566b2,
    0xd3e78047, 0x829186a7, 0x42fc6d0e, 0xdfdfae74, 0x72631866, 0x7323002b, 0x1610c8ed, 0x71c7569a,
    0x7f361cb0, 0x0cc243b5, 0xbfce1352, 0x1088533c, 0xb4a2f546, 0x6aef95a7, 0x36bbe947, 0x16bdc21a,
    0x09e0caa9, 0x0116e81a, 0xd976650d, 0x963c57e2, 0x48945149, 0x1c320717, 0xeb667ac6, 0x1f591e60,
    0xff53bb24, 0xaae41fee, 0xf0ca7e02, 0x8730b70f, 0xc4632f7d, 0xd39d604d, 0xbf4b9816, 0x81632c19,
    0x5e1d598a, 0x0bf9c822, 0x139b26af, 0x0885f82f, 0x3bef0abf, 0x391b5f5a, 0xc2130cfb, 0x930e442c,
    0x45b2d003, 0x6a140208, 0x1bd3bd83, 0x0df9c52f, 0x2a81d4b1, 0x91e2eb6b, 0xfddc551f, 0xf762ba64,
    0xc756dbdc, 0x74abb1e6, 0xd8f6b02d, 0x427db064, 0xe645ce7a, 0x27b451cc, 0x2141ec4c, 0x23452f7a,
    0xf3331f13, 0x363875e3, 0x91f9a441, 0x1e92fa1f, 0x56fbd3d2, 0xe7e1e5a1, 0xb4b8766b, 0xf35bfc05,
    0xb01e2b3e, 0xec703842, 0xdd67299e, 0x2e4cc3ba, 0x646cf41b, 0xed5334f2, 0x88a5c240, 0xbe68e4cb,
    0xb8632fbf, 0x7aad96b3, 0x049ccb11, 0x6f46ba00, 0xcac5b735, 0x944a3fae, 0x51e2dd94, 0x6e666ae1,
    0x6bfdcb02, 0x3fcca8a3, 0x62b406e0, 0xd6409222, 0xec9041b8, 0x028980d3, 0x800415f2, 0x047644cb,
    0x237b7883, 0x7eb75811, 0xe910fca2, 0x0ab1c335, 0x0dad2e6f, 0xed918250, 0xfef03f02, 0x85c97866,
    0x7232fe71, 0x554435d6, 0x7d0e6dfa, 0x748cb629, 0x2188fa6e, 0x0e21e4c7, 0xcb6af7bb, 0x4be43da5,
    0xb55d6610, 0x604ba50a, 0x5f84a05c, 0x16726b0f, 0x89781266, 0xa63311d1, 0x9f4ab515, 0x26c2cb87,
    0xb1b10af8, 0xb710b0c5, 0x6cac59b7, 0xfa042e25, 0xae777958, 0x551749bc, 0x05bb00f8, 0x314ba5b1,
    0x41d55fa1, 0xf7fe5b19, 0x04c528ab, 0x1e10b184, 0xd77f508d, 0xfb048020, 0x605db2d2, 0x7570ee68,
    0xdf362ccf, 0x2d6841c5, 0x585a9ac6, 0x590227be, 0x685782d0, 0x1951ad35, 0xc2a00d05, 0x6267892f,
    0xdbee9960, 0x1ad86118, 0xabf89eba, 0x922137dc, 0x37af8bc5, 0x4f99cba7, 0xa3f40604, 0xc35d6602,
    0x69bcb56b, 0xf24b47cb, 0x87706947, 0xfc16f895, 0x5ba02192, 0x6c10f29f, 0x0611b483, 0x6f36e1fd,
    0x87a36f6d, 0x16f95204, 0x30e433f8, 0xf83c1f90, 0x587aa1ab, 0xd0def359, 0x6f72a64f, 0x97075ed2,
    0xa0fb72b2, 0xe3edd6ca, 0x59ef9a2b, 0x85d0f5b6, 0x966a302e, 0xd9498509, 0x581fd968, 0x19314782,
    0x73cc0837, 0xe359b96a, 0x2337fdd1, 0x99bd923f, 0xbde57769, 0xa7668786, 0x7562f404, 0xa9e0dc23,
    0xed53c1d6, 0x17efdd31, 0x665acd8f, 0xaccd659c, 0xf426a209, 0x383cd559, 0xe6bf3bcf, 0xa2b19d95,
    0xb2e91d10, 0x6d225e5b, 0x77b9fce0, 0x39178420, 0xe415405c, 0x14a86ba3, 0x737f2898, 0xe1ec8d42,
    0x5be1b1b6, 0x0c3f231f, 0x0b640ac4, 0xdcf53fa7, 0x1c5aecdf, 0x526c9295, 0x8fc826ee, 0x7c8e42f2,
    0xe1c936f0, 0x6bb87ac9, 0x72f3b9e7, 0x1e8a907b, 0xfd9da609, 0xc9bf6289, 0x0c3ef16a, 0xdc069ab7,
    0x55a1979f, 0x7fd9aaf8, 0x8acf3bee, 0x2a8e5953, 0xd8aca2fe, 0x067d889b, 0x246cebac, 0x6e68ee0d,
    0x310df900, 0xea5a7825, 0xc40f0de0, 0xa3cfb1b0, 0x570eb40e, 0xca6f3486, 0x69a4eefb, 0xf94ff61d,
    0x92563780, 0xdaa966ef, 0x6d3fc431, 0x66ed9841, 0x19a865b0, 0xf0da54ef, 0x0259b281, 0x493f6af2,
    0x79091394, 0x1ceda3f9, 0xa560ab89, 0x76ce589a, 0x4a8347fd, 0x1c2d926b, 0xdac93fb1, 0xc1d388c0,
    0xf41cf9e3, 0xa49dd6f0, 0xe642f106, 0xba965a64, 0xefe44ba0, 0x015394be, 0x90dceac9, 0x0efb883c,
    0xa505b589, 0x02cbfaa3, 0xef7de800, 0x2645c7f6, 0xa476f189, 0x5f5130fa, 0x28436cc2, 0xa85f815b,
    0x89daef8f, 0xcc1452d0, 0xeb434e4b, 0x28e59fdb, 0x0023c45d, 0x9989168a, 0xd4027e51, 0xa9ddddad,
    0xd53f91b5, 0x204ff87a, 0x648392f3, 0x3a6040e7, 0xfcfd5b75, 0x470d6e3a, 0x924215ee, 0xf90eb1a9,
    0xa9bed499, 0x78c9960e, 0x3c8609b8, 0x10b6119b, 0x9f3b4c2e, 0x30c46446, 0xfc9b80e4, 0x66fa2ed8,
    0x871cf502, 0x982b5891, 0x1e933b84, 0x27f376f9, 0x573db25b, 0x2eb31bc5, 0x1ac21dfe, 0x3eae00a7,
    0x132ea0fd, 0x5bfaf370, 0x18e5a1c1, 0xa18bd1f0, 0x79eacebe, 0x4d233a7d, 0xbdd91c31, 0x5c58ebaa,
    0x9e88b38e, 0x51046b97, 0x09ccb32f, 0x6042b41f, 0x861994a0, 0xc8f0ad99, 0x6846bdd6, 0xd026aed2,
    0xc1b4e382, 0xe265e3f4, 0x593be0c9, 0x79bd308d, 0xeded9bed, 0x0c09c91b, 0xa282c347, 0x4ca61840,
    0xac45525f, 0xac6717b1, 0x88d4c4c6, 0xe6e766cc, 0x1ce3ae8c, 0x27381bfa, 0x14d206f2, 0xc7033424,
    0xfd2ac03a, 0x5722e240, 0x4717b1dc, 0x50d0ae78, 0x52a759cf, 0x4d1fc940, 0x7988b54f, 0x2c07e3e1,
    0xa85cdf71, 0x1a660162, 0x351ade68, 0xcb4870a9, 0x9f8f6d23, 0xf0449d5c, 0xc9e688af, 0x0d2539a3,
    0x1620deca, 0x130bd278, 0x00f6f4d1, 0x54be6b5e, 0xfa22691a, 0x57c10532, 0x822589f2, 0x150b6780,
    0x61d5e2fc, 0x01d50b4c, 0x9bde1f6d, 0xa2acb160, 0xcd124416, 0x6ba4b044, 0x0a1c6955, 0x88812638,
    0xaa81a101, 0xe3e79a27, 0x2f959d69, 0x478dba70, 0xfbb9604f, 0x7961925e, 0x1b9c1bd5, 0x31fa32b4,
    0x3725303a, 0x125c083b, 0xf5e46c27, 0x45e86085, 0x3d23b94a, 0x09add26b, 0x926bcf60, 0xdacc17c3,
    0xd6103b75, 0x0818ae21, 0x6364667f, 0xa3691d05, 0x4242241c, 0xc92a2704, 0xa621f532, 0xbcc892e2,
    0x3d5fdcb9, 0xcb2d0554, 0xa94d5077, 0xbffdd6e4, 0x6fb99121, 0x12262aab, 0xb0c67037, 0xbe84909e,
    0x3356ff0a, 0x88835c23, 0x2b3096d9, 0x25e8137a, 0x851474c4, 0x25778f8c, 0x30c1f10a, 0x6f3b9f58,
    0x332ceda7, 0xd10e1b3e, 0x43a45ddc, 0xfada390d, 0xedc7e424, 0xaf37555d, 0x6f53f772, 0x5635b89e,
    0xf5d8c47c, 0x4fb81d38, 0x37acb9f8, 0xb8f6dcbb, 0x441cdace, 0x2d02099e, 0x17722cdf, 0xb013b7fc,
    0x0059fb4e, 0x36e24610, 0x824ac6bf, 0x7256b54b, 0xe491068d, 0xd191ac2e, 0x35287392, 0xa35479ca,
    0x9b594bad, 0x435aeb63, 0x4d1a8fc6, 0x83cbb81a, 0x79620dd8, 0xddf45466, 0x559cfca3, 0xda047166,
    0x693d5def, 0x407af60f, 0x6b5d0a29, 0x25518a03, 0x64013c2b, 0xd0c78cd6, 0xa47c5ef1, 0x9785b4d1,
    0x73575ff5, 0x0d64604f, 0xe898051b, 0x2dbad05e, 0xd9559f76, 0x53d78d05, 0x43c0164d, 0x22c78d2d,
    0xcf64835b, 0xaae27852, 0xc8a4ab40, 0x6a8e0785, 0x1620deca, 0x130bd278, 0x00f6f4d1, 0x54be6b5e,
    0x8b242b15, 0x215b2637, 0xad27e5e6, 0x965ffeef, 0x61d5e2fc, 0x01d50b4c, 0x9bde1f6d, 0xa2acb160,
    0x0ee3ae35, 0xc8b818be, 0x93df90d5, 0x187318fc, 0x35c688cc, 0xb6d9dece, 0xc804ab52, 0x4a90271e,
    0x446c14ef, 0xf9b7ad3d, 0xe47f82f9, 0x619e36a7, 0x3725303a, 0x125c083b, 0xf5e46c27, 0x45e86085,
    0xd4bdcaeb, 0x96aee9ea, 0x4f842691, 0x60220035, 0x72b75808, 0x60623757, 0xf9572e92, 0xd355b401,
    0x27f63f23, 0x0b11b791, 0x921345bb, 0x14bd25fa, 0x5c3ab868, 0x3292f40f, 0xdec6d966, 0xa107b2aa,
    0x165ad27c, 0x04ba06e2, 0xb3019275, 0x8bd4fc6c, 0x9b820ac1, 0x40878713, 0x03e1c046, 0x03d42196,
    0x81830da2, 0x0835e836, 0x32270853, 0xe479e2d3, 0xea0eeedd, 0xf06eae0c, 0xe08a2df8, 0xbe77bc6e,
    0xdb2a5913, 0xf1e16383, 0x53e6510e, 0x9639d47a, 0xa06bd28c, 0x71cc99ba, 0x842e45ed, 0x6849b78c,
    0x958e61a8, 0xa043cf9d, 0xeabe5d1b, 0xcfb38b73, 0x361e03aa, 0x45efcd55, 0xc8e1db3e, 0x3ab7aac6,
    0x19076345, 0x2b81a695, 0x8b5653d9, 0x614b54de, 0x2a806bd1, 0x835c71bc, 0x5f64bfe6, 0xf6df4ad3,
    0xb1e5f270, 0x0c9d0b49, 0x068d2eed, 0x834982ec, 0xcd8b4999, 0x603b6cfc, 0xd8978d7b, 0xcac16f85,
    0xcb214e2c, 0x0361d9d3, 0x44608118, 0xa96d2505, 0x720725e9, 0x143b081c, 0x2488cf05, 0x9bb0cd3b,
    0xc872f127, 0xc3a6aaef, 0x4c66cb5a, 0xb661f6c5, 0x587aa1ab, 0xd0def359, 0x6f72a64f, 0x97075ed2,
    0x39ac8f7c, 0x0b0ddf05, 0x8181bf68, 0xc919f8ef, 0x2f5432bb, 0x0ad32e66, 0x574884ec, 0x3ec6c2bc,
    0x099c5a5e, 0xdbf1407f, 0x18a2c386, 0x9881c1b5, 0x10da5894, 0xd6e268ae, 0xe57edb72, 0xaf622647,
    0x9881baf7, 0x512bdd90, 0xeb3f9c78, 0x64c5e7f3, 0xc1c6996a, 0xce13ab6c, 0x6054fba9, 0xdcb701e0,
    0xcd452134, 0xce41c22a, 0xaa54c2dc, 0x9d42dbf9, 0x5083e24a, 0x46be4ff4, 0x70a5664a, 0x84748a19,
    0xd3407f56, 0xe615b671, 0x1f566d2c, 0x6669eefa, 0x18b30757, 0x5800b054, 0x776bff01, 0x329fd31e,
    0x786f4fb0, 0xa1a69dae, 0xa18796eb, 0x270df262, 0x4161852c, 0xeb729882, 0xc9f4c3fa, 0x4c91e467,
    0xb55d6610, 0x604ba50a, 0x5f84a05c, 0x16726b0f, 0x53c08910, 0x029e302d, 0x7a740ee6, 0x7a7d840a,
    0x671e548c, 0x987aab02, 0x68a50a4f, 0xb064ec4b, 0xe70e662d, 0xe92fd764, 0x6836da33, 0x1fbc6186,
    0x21933aae, 0xc1bc4d25, 0xd1d35786, 0xe7393336, 0xaafddc0b, 0xb6718dfa, 0x30f6ce84, 0x44947a8f,
    0x7298b13e, 0x5a4367eb, 0xedbbcb2b, 0xe8daea70, 0x19076345, 0x2b81a695, 0x8b5653d9, 0x614b54de,
    0xbabdf364, 0x1eed2668, 0xe6bf2e5f, 0xdaea5edc, 0x666f1115, 0x4b1fa42d, 0xd0f2c415, 0xf8a73ed0,
    0xa39817df, 0xc670fc89, 0xd559b152, 0xf93bd7a8, 0xe34e01fb, 0xf0b7431c, 0x7f43c7ba, 0x680c0d9f,
    0x9f4e67c8, 0xd060ffd6, 0x5093fa23, 0x7f6b7cd9, 0xf3041797, 0xc5930cbb, 0x50a4858f, 0x20af82b5,
    0xdde446e9, 0xce9b42af, 0x9bf4fc14, 0x3ba6d872, 0x256ee684, 0x02461915, 0xfbceb152, 0xe039adf3,
    0xb7bdc4a1, 0x8d9fa278, 0xdd561faf, 0xc60a97d0, 0xf7d49b4c, 0x455dd671, 0x3f30523e, 0x287d0b41,
    0xa4a8d629, 0x087ead88, 0x61e7a5d7, 0x2621c3e2, 0x96a26bf7, 0xe9dc7609, 0x1deba201, 0x59d7906e,
    0x000a9fa9, 0x10b1d23c, 0x2de63da4, 0xdf5ec226, 0xcfff0804, 0x27c4c58a, 0xb28fec9c, 0xa7c66227,
    0x67fbd883, 0x4358e63d, 0xe8932c1e, 0xa429b16c, 0x4ee9b36f, 0x4f9cd34b, 0x51abb459, 0x8fcafbe9,
    0x23a86980, 0xee1d21e3, 0xd17b3f5c, 0x65b8fbcc, 0xac2d2ca2, 0xca83a9c9, 0x26a5d6db, 0x49484c1c,
    0x3f2babb7, 0xe350e869, 0xd6701309, 0x0e7350d8, 0x3f2babb7, 0xe350e869, 0xd6701309, 0x0e7350d8,
    0x3f2babb7, 0xe350e869, 0xd6701309, 0x0e7350d8, 0x3f2babb7, 0xe350e869, 0xd6701309, 0x0e7350d8,
    0x3f2babb7, 0xe350e869, 0xd6701309, 0x0e7350d8, 0x3f2babb7, 0xe350e869, 0xd6701309, 0x0e7350d8,
    0x3f2babb7, 0xe350e869, 0xd6701309, 0x0e7350d8, 0x3f2babb7, 0xe350e869, 0xd6701309, 0x0e7350d8,
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
    0x10942df2, 0x9ee00506, 0x94e6ac33, 0xf0accab2, 0xea0809af, 0xe55c5c0d, 0xec164119, 0x31ed2584,
    0x59c995f4, 0xdb639bb8, 0x5f950153, 0x39769403, 0x0e5959d3, 0xca0002d5, 0x5f14fa7b, 0x69d16cd7,
    0xbdc3890f, 0x3b895932, 0x2d322f2e, 0xd79afb11, 0x23ca2605, 0x8c010242, 0xadbff909, 0xcb401adc,
    0xf0f22ee0, 0x0370afbb, 0x513453e2, 0x7c6874ee, 0x09f0b62a, 0xba5120dc, 0x9025dece, 0xfeb5d93e,
    0xebf679b7, 0x06826e54, 0xf3b91a64, 0xe54d7635, 0x3ca9969b, 0xe0528cef, 0x7a18b915, 0x251ee49d,
    0x565e240e, 0xc7e5076f, 0xa0fc5357, 0xd303e741, 0x9f856eac, 0x19545bb9, 0x73919fca, 0xb5d154e9,
    0x778c873b, 0x2ee4fb1a, 0x6558ddd8, 0xc31f6247, 0x1ac2f686, 0x3ba03ca2, 0x1130e9ae, 0xd005d64f,
    0xb6da4273, 0x6d98bf21, 0x4f08a54f, 0xba38d6b4, 0x79267924, 0x1bcab656, 0x3113614f, 0xcb155490,
    0x54784606, 0xff205313, 0x3ae89e0d, 0x9bcf01dd, 0x91de1b83, 0x8da9a522, 0xd09a093c, 0xfbedc2f9,
    0xe0bd2d57, 0xa006442c, 0xa2358601, 0xcf641beb, 0x0d5e0a6e, 0xb33a11d9, 0xa2dc7fe0, 0x3ba0d0b2,
    0x05b51f91, 0x4453a233, 0xb07052b9, 0xbf425699, 0x17f6cb3c, 0xccd49779, 0x77cefb2d, 0xc8ca3a0a,
    0x88089efa, 0xcf472cf9, 0xeb775434, 0x24e53a46, 0xcb85e314, 0x9e3ab4ef, 0xbfc8919a, 0x8f032a6d,
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
    0x10942df2,0x9ee00506,0x94e6ac33,0xf0accab2,0xea0809af,0xe55c5c0d,0xec164119,0x31ed2584,0x59c995f4,0xdb639bb8,0x5f950153,0x39769403,0x0e5959d3,0xca0002d5,0x5f14fa7b,0x69d16cd7,0xbdc3890f,0x3b895932,0x2d322f2e,0xd79afb11,0x23ca2605,0x8c010242,0xadbff909,0xcb401adc,0xf0f22ee0,0x0370afbb,0x513453e2,0x7c6874ee,0x09f0b62a,0xba5120dc,0x9025dece,0xfeb5d93e,0xebf679b7,0x06826e54,0xf3b91a64,0xe54d7635,0x3ca9969b,0xe0528cef,0x7a18b915,0x251ee49d,0x565e240e,0xc7e5076f,0xa0fc5357,0xd303e741,0x9f856eac,0x19545bb9,0x73919fca,0xb5d154e9,0x778c873b,0x2ee4fb1a,0x6558ddd8,0xc31f6247,0x1ac2f686,0x3ba03ca2,0x1130e9ae,0xd005d64f,0xb6da4273,0x6d98bf21,0x4f08a54f,0xba38d6b4,0x79267924,0x1bcab656,0x3113614f,0xcb155490,0x54784606,0xff205313,0x3ae89e0d,0x9bcf01dd,0x91de1b83,0x8da9a522,0xd09a093c,0xfbedc2f9,0xe0bd2d57,0xa006442c,0xa2358601,0xcf641beb,0x0d5e0a6e,0xb33a11d9,0xa2dc7fe0,0x3ba0d0b2,0x05b51f91,0x4453a233,0xb07052b9,0xbf425699,0x17f6cb3c,0xccd49779,0x77cefb2d,0xc8ca3a0a,0x88089efa,0xcf472cf9,0xeb775434,0x24e53a46,0xcb85e314,0x9e3ab4ef,0xbfc8919a,0x8f032a6d,
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

