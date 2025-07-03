# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

from floorsweep_libs import Datalogger
from floorsweep_libs import Exceptions
from floorsweep_libs import FsInfo
from floorsweep_libs.TestInfo import TestInfo
from floorsweep_libs.SkuConfig import SkuConfig
from .GpuInfo import GpuInfo

class GP10xGpuInfo(GpuInfo):
    def __init__(self, *args, **kwargs):
        self._sku_pes_config = None

        super(GP10xGpuInfo,self).__init__(*args, **kwargs)

    def ParseSkuInfo(self, skuinfo):
        if skuinfo:
            try:
                super(GP10xGpuInfo,self).ParseSkuInfo(skuinfo)
                fsinfo = self.GetFsInfo()
                self._sku_pes_config = SkuConfig(skuinfo['PES_CONFIG'],fsinfo.pes_per_gpc,fsinfo.MAX_PES)
            except Exceptions.FloorsweepException:
                raise
            except Exception as e:
                raise Exceptions.SoftwareError("Malformed skuinfo ({!s})".format(e))

    def CheckSkuTpc(self):
        if not self.sku:
            return -1
        sup = super(GP10xGpuInfo,self).CheckSkuTpc()
        fsinfo = self.GetFsInfo()
        pes = self._sku_pes_config.CheckConfig(fsinfo.pes_disable_masks)
        if pes > 0:
            raise Exceptions.IncorrectPes()      
        return min(sup,pes)

    def PrintSkuInfo(self):
        super(GP10xGpuInfo,self).PrintSkuInfo()
        Datalogger.PrintLog("... SKU_PES_DISABLE_CONFIG    : {!s}".format(self._sku_pes_config))

class GP100GpuInfo(GP10xGpuInfo):
    def __init__(self, *args, **kwargs):
        super(GP100GpuInfo,self).__init__(*args, **kwargs)
    
    def PushFsInfoLwlink(self, fsinfo):
        Exceptions.ASSERT(isinstance(fsinfo,FsInfo.GP100FsInfo))
        self._fs_stack.append(fsinfo.CloneLwlink())
        self.GetFsInfo().CheckSanity()

    @Datalogger.LogFunction
    def FloorsweepLwlink(self, testlist):
        Exceptions.ASSERT(isinstance(self.GetFsInfo(),FsInfo.GP100FsInfo))
        self.ClearFsStack()        
        Datalogger.PrintLog("OLD FUSE INFO :")
        self.GetFsInfo().PrintFuseInfo()
        
        self.ResetGpu()
        
        try:
            for single_lwlink in self.GetFsInfo().IterateLwlink():
                Datalogger.PrintLog("Iteration : {}".format(single_lwlink))
                for test in testlist:
                    Exceptions.ASSERT(isinstance(test, TestInfo))
                    try:            
                        return_code = test.Run(single_lwlink)
                    except (Exceptions.ModsTimeout, Exceptions.UnknownModsError, Exceptions.CommandError):
                        pass
                    else:
                        if return_code == 0:
                            continue
                    finally:
                        self.ResetGpu()
                    
                    self.PushFsInfoLwlink(~single_lwlink)
        except:
            self.ClearFsStack()
            raise
        
        self.ApplyFsStack()

        Datalogger.PrintLog("NEW FUSE INFO :")
        fsinfo = self.GetFsInfo()
        fsinfo.PrintFuseInfo()
        fsinfo.LogFuseInfo()
        
class GP104GpuInfo(GP10xGpuInfo):
    def __init__(self, *args, **kwargs):
        super(GP104GpuInfo,self).__init__(*args, **kwargs)
    
    def PushFsInfoKappa(self, fsinfo):
        self._fs_stack.append(fsinfo.CloneKappa())
        
    @Datalogger.LogFunction
    def KappaBin(self, testlist):
        Exceptions.ASSERT(isinstance(testlist, list))
        self.ClearFsStack()
        Datalogger.PrintLog("OLD FUSE INFO :")
        self.GetFsInfo().PrintFuseInfo()  
                
        self.ResetGpu()
        
        try:
            for kappa_bin in self.GetFsInfo().IterateKappa():
                kappa_str = str(kappa_bin)
                Datalogger.PrintLog("Iteration : {}".format(kappa_str[kappa_str.find("kappa"):]))
                for test in testlist:
                    Exceptions.ASSERT(isinstance(test, TestInfo))
                    try:
                        return_code = test.Run(kappa_bin)
                    except (Exceptions.ModsTimeout, Exceptions.UnknownModsError, Exceptions.CommandError):
                        pass
                    else:
                        if return_code == 0:
                            continue
                    finally:
                        self.ResetGpu()
                    
                    # The GPU failed a test at this kappa bin, previously set kappa is highest
                    raise Exceptions.SkuMatchEarlyExit()
                
                # The GPU passed all tests for this kappa bin, set it as the current best    
                self.PushFsInfoKappa(kappa_bin)
        except Exceptions.SkuMatchEarlyExit:
            pass
        except:
            self.ClearFsStack()
            raise
    
        self.ApplyFsStack()
        
        Datalogger.PrintLog("NEW FUSE INFO :")
        fsinfo = self.GetFsInfo()
        fsinfo.PrintFuseInfo()
        fsinfo.LogFuseInfo()

class GP107GpuInfo(GP10xGpuInfo):
    def __init__(self, *args, **kwargs):
        super(GP107GpuInfo,self).__init__(*args, **kwargs)
        if self.sku:
            self._fsinfo.MakeFBConsistent(self._sku_fbp_config)

    def FloorsweepFB(self, testlist, run_connectivity, only_fbp, iterate_cold_fbp, fb_max_tpc, *args):
        if not self.sku  or self._sku_fbp_config.CheckConfig(0x1) == 0 or self._sku_fbp_config.CheckConfig(0x2) == 0:
            # The SKU calls for one whole FBP to be disabled
            # The simplest algorithm would be to force interate_cold_fbp
            return super(GP107GpuInfo,self).FloorsweepFB(testlist, run_connectivity, only_fbp, True, fb_max_tpc, *args)
        else:
            # Otherwise don't iterate_cold_fbp so it tries the other two possible configurations
            if iterate_cold_fbp:
                Datalogger.PrintLog("Ignoring --iterate_cold_fbp")
            return super(GP107GpuInfo,self).FloorsweepFB(testlist, run_connectivity, only_fbp, False, fb_max_tpc, *args)
