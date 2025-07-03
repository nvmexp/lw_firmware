# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

from floorsweep_libs import Exceptions

import ctypes
import os
import zipfile
import shutil

_fs_lib = None

def GetFsLib():
    global _fs_lib
    if _fs_lib == None:
        InitFsLib()
    return _fs_lib

def InitFsLib():
    global _fs_lib
    fs_lib_file = "libfsegg.so"
    fs_egg_path = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
    fs_lib_path = os.path.join("libfsegg",fs_lib_file)
    with zipfile.ZipFile(fs_egg_path) as zip_file:
        if fs_lib_path not in zip_file.namelist():
            raise Exceptions.FileNotFound("{} not present in this build of floorsweep.egg".format(fs_lib_file))
        file = zip_file.open(fs_lib_path)
        with open(fs_lib_file, 'wb') as outfile:
            shutil.copyfileobj(file, outfile)

    _fs_lib = FsLibWrap(fs_lib_file)

class FsLibWrap:
    def __init__(self, fs_lib_file):
        self.fs_lib_file = fs_lib_file
        self.lib = ctypes.cdll.LoadLibrary(self.fs_lib_file)

        self.AllocIsValidStruct = self.lib["allocIsValidStruct"]
        self.FreeIsValidStruct = self.lib["freeIsValidStruct"]
        self.PrintFSCallStruct = self.lib["printFSCallStruct"]
        self.SetChip = self.lib["setChip"]
        self.SetModeFuncValid = self.lib["setModeFuncValid"]
        self.SetFBPMask = self.lib["setFBPMask"]
        self.SetFBIOPerFBPMask = self.lib["setFBIOPerFBPMask"]
        self.SetFBPAPerFBPMask = self.lib["setFBPAPerFBPMask"]
        self.SetHalfFBPAPerFBPMask = self.lib["sethalfFBPAPerFBPMask"]
        self.SetLTCPerFBPMask = self.lib["setLTCPerFBPMask"]
        self.SetL2SlicePerFBPMask = self.lib["setL2SlicePerFBPMask"]
        self.SetGPCMask = self.lib["setGPCMask"]
        self.SetPesPerGpcMask = self.lib["setPesPerGpcMask"]
        self.SetCpcPerGpcMask = self.lib["setCpcPerGpcMask"]
        self.SetTpcPerGpcMask = self.lib["setTpcPerGpcMask"]
        self.IsFSValid = self.lib["isFSValid"]
        self.GetErrMsg = self.lib["getErrMsg"]
        self.GetErrMsg.restype = ctypes.c_char_p

    def __del__(self):
        os.unlink(self.fs_lib_file)

class FsLibStruct:
    def __init__(self, chip):
        self.fslib = GetFsLib()
        self.c_pointer = self.fslib.AllocIsValidStruct()
        c_chip = ctypes.create_string_buffer(chip.encode('ascii'))
        self.fslib.SetChip(self.c_pointer, c_chip)
        self.fslib.SetModeFuncValid(self.c_pointer)

    def __del__(self):
        self.fslib.FreeIsValidStruct(self.c_pointer)

    def Print(self):
        self.fslib.PrintFSCallStruct(self.c_pointer)

    def SetFBPMask(self, mask):
        self.fslib.SetFBPMask(self.c_pointer, mask)
    def SetFBIOPerFBPMask(self, idx, mask):
        self.fslib.SetFBIOPerFBPMask(self.c_pointer, idx, mask)
    def SetFBPAPerFBPMask(self, idx, mask):
        self.fslib.SetFBPAPerFBPMask(self.c_pointer, idx, mask)
    def SetHalfFBPAPerFBPMask(self, idx, mask):
        self.fslib.SetHalfFBPAPerFBPMask(self.c_pointer, idx, mask)
    def SetLTCPerFBPMask(self, idx, mask):
        self.fslib.SetLTCPerFBPMask(self.c_pointer, idx, mask)
    def SetL2SlicePerFBPMask(self, idx, mask):
        self.fslib.SetL2SlicePerFBPMask(self.c_pointer, idx, mask)
    def SetGPCMask(self, mask):
        self.fslib.SetGPCMask(self.c_pointer, mask)
    def SetPesPerGpcMask(self, idx, mask):
        self.fslib.SetPesPerGpcMask(self.c_pointer, idx, mask)
    def SetCpcPerGpcMask(self, idx, mask):
        self.fslib.SetCpcPerGpcMask(self.c_pointer, idx, mask)
    def SetTpcPerGpcMask(self, idx, mask):
        self.fslib.SetTpcPerGpcMask(self.c_pointer, idx, mask)
    def IsFSValid(self):
        return self.fslib.IsFSValid(self.c_pointer)
    def GetErrMsg(self):
        c_str = self.fslib.GetErrMsg(self.c_pointer)
        return c_str.decode('utf-8').strip()
