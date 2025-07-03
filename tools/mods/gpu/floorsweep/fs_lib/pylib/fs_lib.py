import ctypes
import os
import sys

# look at test_fs_lib.py for examples

class fs_lib_wrap:
  def __init__(self, lib):
    
    # export all the c functions to python
    self.lib = lib
    self.allocIsValidStruct = self.lib["allocIsValidStruct"]
    self.allocIsValidStruct.restype = ctypes.c_longlong
    self.freeIsValidStruct = self.lib["freeIsValidStruct"]
    self.freeIsValidStruct.argtypes = [ctypes.c_longlong]
    self.printFSCallStruct = self.lib["printFSCallStruct"]
    self.setChip = self.lib["setChip"]
    self.setChip.argtypes = [ctypes.c_longlong]
    self.setFBPMask = self.lib["setFBPMask"]
    self.setFBPMask.argtypes = [ctypes.c_longlong]
    self.setFBIOPerFBPMask = self.lib["setFBIOPerFBPMask"]
    self.setFBIOPerFBPMask.argtypes = [ctypes.c_longlong]
    self.setFBPAPerFBPMask = self.lib["setFBPAPerFBPMask"]
    self.setFBPAPerFBPMask.argtypes = [ctypes.c_longlong]
    self.sethalfFBPAPerFBPMask = self.lib["sethalfFBPAPerFBPMask"]
    self.sethalfFBPAPerFBPMask.argtypes = [ctypes.c_longlong]
    self.setLTCPerFBPMask = self.lib["setLTCPerFBPMask"]
    self.setLTCPerFBPMask.argtypes = [ctypes.c_longlong]
    self.setL2SlicePerFBPMask = self.lib["setL2SlicePerFBPMask"]
    self.setL2SlicePerFBPMask.argtypes = [ctypes.c_longlong]
    self.setGPCMask = self.lib["setGPCMask"]
    self.setGPCMask.argtypes = [ctypes.c_longlong]
    self.setPesPerGpcMask = self.lib["setPesPerGpcMask"]
    self.setPesPerGpcMask.argtypes = [ctypes.c_longlong]
    self.setRopPerGpcMask = self.lib["setPesPerGpcMask"]
    self.setRopPerGpcMask.argtypes = [ctypes.c_longlong]
    self.setCpcPerGpcMask = self.lib["setCpcPerGpcMask"]
    self.setCpcPerGpcMask.argtypes = [ctypes.c_longlong]
    self.setTpcPerGpcMask = self.lib["setTpcPerGpcMask"]
    self.setTpcPerGpcMask.argtypes = [ctypes.c_longlong]
    self.getFBPMask = self.lib["getFBPMask"]
    self.getFBPMask.argtypes = [ctypes.c_longlong]
    self.getFBIOPerFBPMask = self.lib["getFBIOPerFBPMask"]
    self.getFBIOPerFBPMask.argtypes = [ctypes.c_longlong]
    self.getFBPAPerFBPMask = self.lib["getFBPAPerFBPMask"]
    self.getFBPAPerFBPMask.argtypes = [ctypes.c_longlong]
    self.gethalfFBPAPerFBPMask = self.lib["gethalfFBPAPerFBPMask"]
    self.gethalfFBPAPerFBPMask.argtypes = [ctypes.c_longlong]
    self.getLTCPerFBPMask = self.lib["getLTCPerFBPMask"]
    self.getLTCPerFBPMask.argtypes = [ctypes.c_longlong]
    self.getL2SlicePerFBPMask = self.lib["getL2SlicePerFBPMask"]
    self.getL2SlicePerFBPMask.argtypes = [ctypes.c_longlong]
    self.getGPCMask = self.lib["getGPCMask"]
    self.getGPCMask.argtypes = [ctypes.c_longlong]
    self.getPesPerGpcMask = self.lib["getPesPerGpcMask"]
    self.getPesPerGpcMask.argtypes = [ctypes.c_longlong]
    self.getRopPerGpcMask = self.lib["getPesPerGpcMask"]
    self.getRopPerGpcMask.argtypes = [ctypes.c_longlong]
    self.getCpcPerGpcMask = self.lib["getCpcPerGpcMask"]
    self.getCpcPerGpcMask.argtypes = [ctypes.c_longlong]
    self.getTpcPerGpcMask = self.lib["getTpcPerGpcMask"]
    self.getTpcPerGpcMask.argtypes = [ctypes.c_longlong]
    self.isFBPFSValidC = self.lib["isFSValid"]
    self.isGPCFSValidC = self.lib["isFSValid"]
    self.isFSValidC = self.lib["isFSValid"]
    self.isFSValidC.argtypes = [ctypes.c_longlong]
    self.getErrMsgC = self.lib["getErrMsg"]
    self.getErrMsgC.argtypes = [ctypes.c_longlong]
    self.getErrMsgC.restype = ctypes.c_char_p
    self.funcDownBin = self.lib["funcDownBin"]
    self.funcDownBin.argtypes = [ctypes.c_longlong, ctypes.c_longlong]
    self.downBin = self.lib["downBin"]
    self.downBin.argtypes = [ctypes.c_longlong, ctypes.c_longlong, ctypes.c_longlong]
    self.downBinSpares = self.lib["downBinSpares"]
    self.downBinSpares.argtypes = [ctypes.c_longlong, ctypes.c_longlong, ctypes.c_longlong, ctypes.c_longlong]

    self.allocSKUStruct = self.lib["allocSKUStruct"]
    self.allocSKUStruct.restype = ctypes.c_longlong
    self.allocFSConfigStruct = self.lib["allocFSConfigStruct"]
    self.allocFSConfigStruct.restype = ctypes.c_longlong
    self.setUGPUImbalance = self.lib["setUGPUImbalance"]
    self.setUGPUImbalance.argtypes = [ctypes.c_longlong]
    self.addToSkyline = self.lib["addToSkyline"]
    self.addToSkyline.argtypes = [ctypes.c_longlong]
    self.freeFSConfigStruct = self.lib["freeFSConfigStruct"]
    self.freeFSConfigStruct.argtypes = [ctypes.c_longlong]
    self.freeSKUConfigStruct = self.lib["freeSKUConfigStruct"]
    self.freeSKUConfigStruct.argtypes = [ctypes.c_longlong]
    self.addmustEnable = self.lib["addmustEnable"]
    self.addmustEnable.argtypes = [ctypes.c_longlong]
    self.addmustDisable = self.lib["addmustDisable"]
    self.addmustDisable.argtypes = [ctypes.c_longlong]
    self.addMinMaxRepair = self.lib["addMinMaxRepair"]
    self.addMinMaxRepair.argtypes = [ctypes.c_longlong]
    self.addConfigToSKU = self.lib["addConfigToSKU"]
    self.addConfigToSKU.argtypes = [ctypes.c_longlong, ctypes.c_longlong]
    self.isInSKU = self.lib["isInSKU"]
    self.isInSKU.argtypes = [ctypes.c_longlong, ctypes.c_longlong]
    self.canFitSKU = self.lib["canFitSKU"]
    self.canFitSKU.argtypes = [ctypes.c_longlong, ctypes.c_longlong]

def getFSLib():
  # path to the fs_lib.so library
  if sys.platform == 'win32':
    fs_lib_path = os.path.dirname(os.path.realpath(__file__)) + "/../out/build/x64-Debug/lib/fs_lib.dll"
  else:
    fs_lib_path = os.path.dirname(os.path.realpath(__file__)) + "/../build/lib/fs_lib.so"
  clib = ctypes.cdll[fs_lib_path]
  fslib = fs_lib_wrap(clib)
  return fslib

fs_lib = getFSLib()


# enable is a 0
def enableBit(mask, idx):
  mask &= ~(1 << idx)
  return mask

# disable is a 1
def disableBit(mask, idx):
  mask |= (1 << idx)
  return mask

# enable a bit in an array of masks
def enableArrayBit(masks, idx, bit_idx):
  while (idx >= len(masks)):
    masks.append(0)
  masks[idx] = enableBit(masks[idx], bit_idx)

# disable a bit in an array of masks
def disableArrayBit(masks, idx, bit_idx):
  while (idx >= len(masks)):
    masks.append(0)
  masks[idx] = disableBit(masks[idx], bit_idx)

# get a bit from a list of masks
def getArrayBitEnable(masks, idx, bit_idx):
  # this bounds check is needed because the list is only populated when enableArrayBit or disableArrayBit are called
  if idx >= len(masks):
    return True
  return ((masks[idx] >> bit_idx) & 1) == 0

# populate the fields in this class and they will be passed to the fs_lib c code
class IsFSValidCallStruct:
  def __init__(self, chip_name):
    self.chip = chip_name

    #these fields can be set in the python code
    self.fbpMask = 0
    self.fbioPerFbpMasks = []
    self.fbpaPerFbpMasks = []
    self.halfFbpaPerFbpMasks = []
    self.ltcPerFbpMasks = []
    self.l2SlicePerFbpMasks = []

    self.gpcMask = 0
    self.ropPerGpcMasks = []
    self.pesPerGpcMasks = []
    self.cpcPerGpcMasks = []
    self.tpcPerGpcMasks = []

    self.msg = ""

  # FBP functions
  def setEnableFBP(self, idx):
    self.fbpMask = enableBit(self.fbpMask, idx)
  def setDisableFBP(self, idx):
    self.fbpMask = disableBit(self.fbpMask, idx)
  def getFBPEnable(self, idx):
    return ((self.fbpMask >> idx) & 1) == 0

  # FBIO functions
  def setEnableFBIO(self, fbp_idx, idx):
    enableArrayBit(self.fbioPerFbpMasks, fbp_idx, idx)

  def setDisableFBIO(self, fbp_idx, idx):
    disableArrayBit(self.fbioPerFbpMasks, fbp_idx, idx)

  def getFBIOEnable(self, fbp_idx, idx):
    return getArrayBitEnable(self.fbioPerFbpMasks, fbp_idx, idx)

  # FBPA functions
  def setEnableFBPA(self, fbp_idx, idx):
    enableArrayBit(self.fbpaPerFbpMasks, fbp_idx, idx)

  def setDisableFBPA(self, fbp_idx, idx):
    disableArrayBit(self.fbpaPerFbpMasks, fbp_idx, idx)

  def getFBPAEnable(self, fbp_idx, idx):
    return getArrayBitEnable(self.fbpaPerFbpMasks, fbp_idx, idx)

  # LTC functions
  def setEnableLTC(self, fbp_idx, idx):
    enableArrayBit(self.ltcPerFbpMasks, fbp_idx, idx)

  def setDisableLTC(self, fbp_idx, idx):
    disableArrayBit(self.ltcPerFbpMasks, fbp_idx, idx)

  def getLTCEnable(self, fbp_idx, idx):
    return getArrayBitEnable(self.ltcPerFbpMasks, fbp_idx, idx)

  # L2Slice functions
  def setEnableL2Slice(self, fbp_idx, idx):
    enableArrayBit(self.l2SlicePerFbpMasks, fbp_idx, idx)

  def setDisableL2Slice(self, fbp_idx, idx):
    disableArrayBit(self.l2SlicePerFbpMasks, fbp_idx, idx)
  
  def getL2SliceEnable(self, fbp_idx, idx):
    return getArrayBitEnable(self.l2SlicePerFbpMasks, fbp_idx, idx)

  # GPC functions
  def setEnableGPC(self, idx):
    self.gpcMask = enableBit(self.gpcMask, idx)

  def setDisableGPC(self, idx):
    self.gpcMask = disableBit(self.gpcMask, idx)

  def getGPCEnable(self, idx):
    return ((self.gpcMask >> idx) & 1) == 0

  # PES functions
  def setEnablePES(self, gpc_idx, idx):
    enableArrayBit(self.pesPerGpcMasks, gpc_idx, idx)

  def setDisablePES(self, gpc_idx, idx):
    disableArrayBit(self.pesPerGpcMasks, gpc_idx, idx)

  def getPESEnable(self, gpc_idx, idx):
    return getArrayBitEnable(self.pesPerGpcMasks, gpc_idx, idx)

  # ROP functions
  def setEnableROP(self, gpc_idx, idx):
    enableArrayBit(self.ropPerGpcMasks, gpc_idx, idx)

  def setDisableROP(self, gpc_idx, idx):
    disableArrayBit(self.ropPerGpcMasks, gpc_idx, idx)

  def getROPEnable(self, gpc_idx, idx):
    return getArrayBitEnable(self.ropPerGpcMasks, gpc_idx, idx)

  # CPC functions
  def setEnableCPC(self, gpc_idx, idx):
    enableArrayBit(self.cpcPerGpcMasks, gpc_idx, idx)

  def setDisableCPC(self, gpc_idx, idx):
    disableArrayBit(self.cpcPerGpcMasks, gpc_idx, idx)

  def getCPCEnable(self, gpc_idx, idx):
    return getArrayBitEnable(self.cpcPerGpcMasks, gpc_idx, idx)

  # TPC functions
  def setEnableTPC(self, gpc_idx, idx):
    enableArrayBit(self.tpcPerGpcMasks, gpc_idx, idx)

  def setDisableTPC(self, gpc_idx, idx):
    disableArrayBit(self.tpcPerGpcMasks, gpc_idx, idx)

  def getTPCEnable(self, gpc_idx, idx):
    return getArrayBitEnable(self.tpcPerGpcMasks, gpc_idx, idx)


  def makeCStruct(self):
    c_pointer = fs_lib.allocIsValidStruct()
    fs_lib.setChip(c_pointer, self.chip.encode("utf8"))

    fs_lib.setFBPMask(c_pointer, self.fbpMask)
    for i in range(0, len(self.fbioPerFbpMasks)):
      fs_lib.setFBIOPerFBPMask(c_pointer, i, self.fbioPerFbpMasks[i])
    for i in range(0, len(self.fbpaPerFbpMasks)):
      fs_lib.setFBPAPerFBPMask(c_pointer, i, self.fbpaPerFbpMasks[i])
    for i in range(0, len(self.halfFbpaPerFbpMasks)):
      fs_lib.sethalfFBPAPerFBPMask(c_pointer, i, self.halfFbpaPerFbpMasks[i])
    for i in range(0, len(self.ltcPerFbpMasks)):
      fs_lib.setLTCPerFBPMask(c_pointer, i, self.ltcPerFbpMasks[i]) 
    for i in range(0, len(self.l2SlicePerFbpMasks)):
      fs_lib.setL2SlicePerFBPMask(c_pointer, i, self.l2SlicePerFbpMasks[i]) 

    fs_lib.setGPCMask(c_pointer, self.gpcMask)
    for i in range(0, len(self.pesPerGpcMasks)):
      fs_lib.setPesPerGpcMask(c_pointer, i, self.pesPerGpcMasks[i]) 
    for i in range(0, len(self.cpcPerGpcMasks)):
      fs_lib.setCpcPerGpcMask(c_pointer, i, self.cpcPerGpcMasks[i]) 
    for i in range(0, len(self.tpcPerGpcMasks)):
      fs_lib.setTpcPerGpcMask(c_pointer, i, self.tpcPerGpcMasks[i]) 
    for i in range(0, len(self.ropPerGpcMasks)):
      fs_lib.setRopPerGpcMask(c_pointer, i, self.ropPerGpcMasks[i]) 
    return c_pointer

  @classmethod
  def freeCStruct(cls, c_pointer=None):
    if (c_pointer is None):
      return
    fs_lib.freeIsValidStruct(c_pointer)

  def readCStruct(self, c_pointer):
    self.fbpMask = fs_lib.getFBPMask(c_pointer)
    self.fbioPerFbpMasks = [0xffffffff]*32
    for i in range(0, len(self.fbioPerFbpMasks)):
      self.fbioPerFbpMasks[i] = fs_lib.getFBIOPerFBPMask(c_pointer, i)
    self.fbpaPerFbpMasks = [0xffffffff]*32
    for i in range(0, len(self.fbpaPerFbpMasks)):
      self.fbpaPerFbpMasks[i] = fs_lib.getFBPAPerFBPMask(c_pointer, i)
    self.halfFbpaPerFbpMasks = [0xffffffff]*32
    for i in range(0, len(self.halfFbpaPerFbpMasks)):
      self.halfFbpaPerFbpMasks[i] = fs_lib.gethalfFBPAPerFBPMask(c_pointer, i)
    self.ltcPerFbpMasks = [0xffffffff]*32
    for i in range(0, len(self.ltcPerFbpMasks)):
      self.ltcPerFbpMasks[i] = fs_lib.getLTCPerFBPMask(c_pointer, i) 
    self.l2SlicePerFbpMasks = [0xffffffff]*32
    for i in range(0, len(self.l2SlicePerFbpMasks)):
      self.l2SlicePerFbpMasks[i] = fs_lib.getL2SlicePerFBPMask(c_pointer, i) 

    self.gpcMask = fs_lib.getGPCMask(c_pointer)
    self.pesPerGpcMasks = [0xffffffff]*32
    for i in range(0, len(self.pesPerGpcMasks)):
      self.pesPerGpcMasks[i] = fs_lib.getPesPerGpcMask(c_pointer, i) 
    self.cpcPerGpcMasks = [0xffffffff]*32
    for i in range(0, len(self.cpcPerGpcMasks)):
      self.cpcPerGpcMasks[i] = fs_lib.getCpcPerGpcMask(c_pointer, i) 
    self.tpcPerGpcMasks = [0xffffffff]*32
    for i in range(0, len(self.tpcPerGpcMasks)):
      self.tpcPerGpcMasks[i] = fs_lib.getTpcPerGpcMask(c_pointer, i) 
    self.ropPerGpcMasks = [0xffffffff]*32
    for i in range(0, len(self.ropPerGpcMasks)):
      self.ropPerGpcMasks[i] = fs_lib.getRopPerGpcMask(c_pointer, i) 

  def printSummary(self):
    c_pointer = self.makeCStruct()
    fs_lib.printFSCallStruct(c_pointer)

  def isFBPFSValid(self):
    c_pointer = self.makeCStruct()
    return fs_lib.isFBPFSValidC(c_pointer)

  def isGPCFSValid(self):
    c_pointer = self.makeCStruct()
    return fs_lib.isGPCFSValidC(c_pointer)

  def isFSValid(self):
    c_pointer = self.makeCStruct()
    isvalid = fs_lib.isFSValidC(c_pointer)
    self.msg = self.getErrMsgFromC(c_pointer)
    self.freeCStruct(c_pointer)
    return isvalid

  def isInSKU(self, skuconfig):
    isvalid_ptr = self.makeCStruct()
    sku_ptr = skuconfig.makeCStruct()

    is_in_sku = fs_lib.isInSKU(isvalid_ptr, sku_ptr)
    self.msg = self.getErrMsgFromC(isvalid_ptr)

    self.freeCStruct(isvalid_ptr)
    skuconfig.freeCStruct(sku_ptr)
    
    return is_in_sku

  def canFitSKU(self, skuconfig):
    isvalid_ptr = self.makeCStruct()
    sku_ptr = skuconfig.makeCStruct()

    can_fit_sku = fs_lib.canFitSKU(isvalid_ptr, sku_ptr)
    self.msg = self.getErrMsgFromC(isvalid_ptr)

    self.freeCStruct(isvalid_ptr)
    skuconfig.freeCStruct(sku_ptr)
    
    return can_fit_sku

  def funcDownBin(self):
    c_pointer = self.makeCStruct()
    output_c_pointer = fs_lib.allocIsValidStruct()

    success = fs_lib.funcDownBin(c_pointer, output_c_pointer)
    self.freeCStruct(c_pointer)
    outputStruct = IsFSValidCallStruct(self.chip)
    outputStruct.readCStruct(output_c_pointer)
    outputStruct.msg = self.getErrMsgFromC(output_c_pointer)
    outputStruct.freeCStruct(output_c_pointer)
    return outputStruct

  def downBin(self, skuconfig):
    isvalid_ptr = self.makeCStruct()
    output_ptr = fs_lib.allocIsValidStruct()
    output_spares_ptr = fs_lib.allocIsValidStruct()
    sku_ptr = skuconfig.makeCStruct()
    success = fs_lib.downBinSpares(isvalid_ptr, sku_ptr, output_ptr, output_spares_ptr)
    self.freeCStruct(isvalid_ptr)
    skuconfig.freeCStruct(sku_ptr)
    outputStruct = IsFSValidCallStruct(self.chip)
    outputStruct.readCStruct(output_ptr)
    outputStruct.msg = self.getErrMsgFromC(output_ptr)
    outputStruct.freeCStruct(output_ptr)
    outputSparesStruct = IsFSValidCallStruct(self.chip)
    outputSparesStruct.readCStruct(output_spares_ptr)
    outputSparesStruct.freeCStruct(output_spares_ptr)
    return outputStruct, outputSparesStruct

  @classmethod
  def getErrMsgFromC(cls, c_pointer):
    buf = fs_lib.getErrMsgC(c_pointer)
    return (ctypes.c_char_p(buf).value.decode("utf8")).strip()

  def getErrMsg(self):
    return self.msg


class FSConfig:
  def __init__(self, minEnableCount, maxEnableCount, minEnablePerGroup=-1, enableCountPerGroup=-1, minRepairCount=0, maxRepairCount=0, mustEnableList=[], mustDisableList=[],
               maxTPLwGPUImbalance = None, skyline=[]):
    self.minEnableCount = minEnableCount
    self.maxEnableCount = maxEnableCount
    self.minEnablePerGroup = minEnablePerGroup
    self.enableCountPerGroup = enableCountPerGroup
    self.minRepairCount = minRepairCount
    self.maxRepairCount = maxRepairCount
    self.mustEnableList = mustEnableList
    self.mustDisableList = mustDisableList
    self.maxTPLwGPUImbalance = maxTPLwGPUImbalance
    self.skyline = skyline

  def makeCStruct(self):
    config_pointer = fs_lib.allocFSConfigStruct(self.minEnableCount, self.maxEnableCount, self.minEnablePerGroup, self.enableCountPerGroup)
    
    for mustEnable in self.mustEnableList:
      fs_lib.addmustEnable(config_pointer, mustEnable)

    for mustDisable in self.mustDisableList:
      fs_lib.addmustDisable(config_pointer, mustDisable)

    # only matters on gh100
    if self.maxTPLwGPUImbalance != None:
      fs_lib.setUGPUImbalance(config_pointer, self.maxTPLwGPUImbalance)

    # only matters on gh100
    for tpc_count in self.skyline:
      fs_lib.addToSkyline(config_pointer, tpc_count)

    fs_lib.addMinMaxRepair(config_pointer, self.minRepairCount, self.maxRepairCount)

    return config_pointer

  @classmethod
  def freeCStruct(cls, config_pointer):
    fs_lib.freeFSConfigStruct(config_pointer)
    
class SkuConfig:
  def __init__(self, name, id, configs_map):
    self.name = name
    self.id = id
    self.fsconfigs = configs_map

  def makeCStruct(self):
    sku_pointer = fs_lib.allocSKUStruct(self.name.encode("utf8"), self.id)
    for config_name in self.fsconfigs:
      fsconfig = self.fsconfigs[config_name]
      fsconfig_pointer = fsconfig.makeCStruct()
      fs_lib.addConfigToSKU(sku_pointer, fsconfig_pointer, config_name.encode("utf8"))
      fsconfig.freeCStruct(fsconfig_pointer) # the addConfigToSKU copies the data, so we can now free this chunk
    return sku_pointer

  @classmethod
  def freeCStruct(cls, sku_pointer):
    fs_lib.freeSKUConfigStruct(sku_pointer)
