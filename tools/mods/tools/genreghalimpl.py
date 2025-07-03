#!/usr/bin/elw python3

# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import copy
import itertools
import argparse
import os
import re
import sys

# Prevent pyc files from cluttering the tree
sys.dont_write_bytecode = True

headers = [
    "dev_bus.h",
    "dev_bus_addendum.h",
    "dev_c2c.h",
    "dev_ce.h",
    "dev_ce0_pri.h",
    "dev_ce1_pri.h",
    "dev_ctrl.h",
    "dev_disp.h",
    "dev_disp_addendum.h",
    "dev_egress_ip.h",
    "dev_ext_devices.h",
    "dev_falcon_second_pri.h",
    "dev_falcon_v4.h",
    "dev_fb.h",
    "dev_fbfalcon_pri.h",
    "dev_fbpa.h",
    "dev_fbpa_addendum.h",
    "dev_fifo.h",
    "dev_flush.h",
    "dev_fpf.h",
    "dev_fsp_pri.h",
    "dev_fuse.h",
    "dev_fuse_addendum.h",
    "dev_gc6_island.h",
    "dev_gc6_island_addendum.h",
    "dev_graphics_nobundle.h",
    "dev_gsp.h",
    "dev_h2j_unlock.h",
    "dev_host.h",
    "dev_hshub.h",
    "dev_hshub_SW.h",
    "dev_ingress_ip.h",
    "dev_ioctrlmif_ip.h",
    "dev_ism.h",
    "dev_ist_seq_slv.h",
    "dev_kfuse_pri.h",
    "dev_ltc.h",
    "dev_master.h",
    "dev_mathslink_pcie_controller.h",
    "dev_mmu.h",
    "dev_mspdec_pri.h",
    "dev_msppp_pri.h",
    "dev_msvld_pri.h",
    "dev_multicasttstate_ip.h",
    "dev_npg_ip.h",
    "dev_nport_ip.h",
    "dev_lw_xp.h",
    "dev_lw_xp_addendum.h",
    "dev_lw_xpl.h",
    "dev_lw_xusb_bar2.h",
    "dev_lw_xusb_csb.h",
    "dev_lw_xusb_xhci.h",
    "dev_lw_xve.h",
    "dev_lw_xve3g_vf.h",    
    "dev_lwdec_pri.h",
    "dev_lwenc_pri_sw.h",
    "dev_lwjpg_pri.h",
    "dev_lwjpg_pri_sw.h",
    "dev_lwl.h",
    "dev_lwl_ip.h",
    "dev_lwldl_ip.h",
    "dev_lwlipt.h",
    "dev_lwlipt_ip.h",
    "dev_lwlipt_lnk_ip.h",
    "dev_lwlphyctl_ip.h",
    "dev_lwlpllctl_ip.h",
    "dev_lwlsaw_ip.h",
    "dev_lwltl.h",
    "dev_lwltlc.h",
    "dev_lwltlc_ip.h",
    "dev_lws_master.h",
    "dev_nxbar_tc_global_ip.h",
    "dev_ofa_pri.h",
    "dev_lw_uxl_pri_ctx.h",
    "dev_pbdma.h",
    "dev_perf.h",
    "dev_pmgr.h",
    "dev_pmgr_addendum.h",
    "dev_pri_ringmaster.h",
    "dev_pri_ringstation_fbp.h",
    "dev_pri_ringstation_gpc.h",
    "dev_pri_ringstation_sys.h",
    "dev_pwr_pri.h",
    "dev_ram.h",
    "dev_reductiontstate_ip.h",
    "dev_riscv_pri.h",
    "dev_runlist.h",
    "dev_sec_pri.h",
    "dev_smcarb.h",
    "dev_soe_ip.h",
    "dev_soe_ip_addendum.h",
    "dev_therm.h",
    "dev_therm_addendum.h",
    "dev_timer.h",
    "dev_top.h",
    "dev_trex_ip.h",
    "dev_trim.h",
    "dev_vm.h",
    "dev_vm_addendum.h",
    "dev_xtl_ep_pcfg_gpu.h",
    "dev_xtl_ep_pcfg_vf.h",
    "dev_xtl_ep_pri.h",
    "hwproject.h",
    "ioctrl_discovery.h",
    "pm_signals.h",
    "pri_lw_xal_ep.h"
    ]

TYPE_ADDRESS = 1
TYPE_FIELD   = 2
TYPE_VALUE   = 4

BITS_TYPE    = 3
BITS_VALUE   = 8
BITS_FIELD   = 5
BITS_ADDRESS = 31 - (BITS_TYPE + BITS_VALUE + BITS_FIELD)

POS_VALUE    = 0
POS_FIELD    = BITS_VALUE
POS_ADDRESS  = BITS_VALUE + BITS_FIELD
POS_TYPE     = BITS_VALUE + BITS_FIELD + BITS_ADDRESS

REGSPACE_UNICAST   = 0  # Must match reghaltable.h RegSpace enum
REGSPACE_MULTICAST = 1

cppNogenHeader = """
/* LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

""".lstrip()

cppHeader = """
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*!! GENERATED USING diag/mods/tools/genreghalimpl.py !!*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/

""".lstrip() + cppNogenHeader

hashHeader = """
####################################################
# GENERATED USING diag/mods/tools/genreghalimpl.py #
####################################################

#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

""".lstrip()

# Print an error message and exit.  The optional "line" parameter is
# used for file-parsing errors; it appends line number & content info
# to the error message.
#
def die(message, line=None):
    if line:
        message = "%s in line %d:\n%s" % (message, line[0], line[1])
    sys.stderr.write(message + "\n")
    sys.stderr.write("\n")
    sys.exit(1)

# Compare two strings in natural sorting order, such that "1_CLKS" <
# "32_CLKS" < "256_CLKS".  Return -1, 0, or +1 depending on whether
# str1 < str2, str1 == str2, or str1 > str2 respectively.
#
reNumSplit = re.compile("([0-9]+)")
def NaturalCompare(str1, str2):
    if str1 == str2:
        return 0
    m1 = reNumSplit.search(str1)
    m2 = reNumSplit.search(str2)
    # if there is no number or numbers are in different positions, compare lexicographically
    if (m1 is None or m2 is None) or (m1.span()[0] != m2.span()[0]):
        if str1 < str2:
            return -1
        else:
            return 1
    # if the numbers are in the beginning, compare them numerically
    if 0 == m1.span()[0]:
        n1 = int(m1.group(0))
        n2 = int(m2.group(0))
        if n1 != n2:
            if n1 < n2:
                return -1
            else:
                return 1
        else:
            # if the numbers are equal, continue comparing strings after numbers
            return NaturalCompare(str1[m1.span()[1]:], str2[m2.span()[1]:])
    # compare strings before the numbers
    s1 = str1[:m1.span()[0]]
    s2 = str2[:m2.span()[0]]
    # if they are not equal compare them lexicographically
    if s1 != s2:
        if s1 < s2:
            return -1
        else:
            return 1
    # if they are equal, continue comparing after them
    return NaturalCompare(str1[m1.span()[0]:], str2[m2.span()[0]:])

# Each Device object represents one device parsed from gpulist.h or lwlinklist.h
# Fields:
# - name: The device name, e.g. "GV100"
# - headerDir: The hwref subdirectory containing the dev_*.h files
# - displayDir: The hwref subdirectory containing the display dev_*.h files
# - fileName: The .cpp file to generate for this device
# - pmgrFileName: The .cpp file with PMGR mutexes to generate for this device
# - duplicate: Name of another device with same headerDir & displayDir, or None
#
class Device:
    def __init__(self, name, headerDir, dispDir, lwcfgSelector):
        self.name = name
        if headerDir == "None":
            self.headerDir = None
        else:
            self.headerDir = headerDir
        if dispDir == "None":
            self.dispDir = None
        else:
            self.dispDir = dispDir
        self.fileName  = "g_" + name.lower() + "_reghal.cpp"
        self.pmgrFileName  = "g_" + name.lower() + "_pmgrmutex.cpp"
        self.duplicate = None
        self.lwcfgSelector = lwcfgSelector
        self.hwref = None
    def getHwref(self):
        if self.hwref is None:
            self.hwref = Hwref(self)
        return self.hwref

class Hwref:
    class HeaderFile:
        def __init__(self, searchDir, relDir, baseName):
            self.baseName = baseName
            self.cppPath  = os.path.join(relDir, baseName).replace(os.sep, "/")
            self.fullPath = os.path.join(searchDir, relDir, baseName)

    class Entry:
        def __init__(self, firstWord, remainder):
            openParen = firstWord.find("(")
            if openParen < 0:
                self.name       = firstWord
                self.dimensions = 0
                self.definition = remainder
            elif firstWord.endswith(")"):
                self.name       = firstWord[:openParen]
                self.dimensions = firstWord[openParen:].count(",") + 1
                self.definition = remainder
            else:
                line            = firstWord + " " + remainder
                closeParen      = line.find(")") + 1
                self.name       = line[:openParen]
                self.dimensions = line[openParen:closeParen].count(",") + 1
                self.definition = line[closeParen:].lstrip()
            self._parsed = False
        def getValue(self):
            self._parse()
            return self._value
        def getComment(self):
            self._parse()
            return self._comment
        def _parse(self):
            if self._parsed:
                return
            commentStart = self.definition.find("/*")
            if commentStart < 0:
                commentStart = self.definition.find("//")
            if commentStart < 0:
                self._value = self.definition
                self._comment = ""
            else:
                self._value = self.definition[:commentStart].rstrip()
                self._comment = self.definition[commentStart:]
            self._parsed = True

    def __init__(self, device):
        self.headerFiles = []
        self.entries = {}
        searchDirs = [ os.path.join(options.driversPath, "common", "inc", "hwref"),
                       os.path.join(options.driversPath, "common", "inc", "swref") ]
        relDirs = []
        if device.headerDir:
            relDirs.append(device.headerDir)
        if device.dispDir:
            relDirs.append(os.path.join("disp", device.dispDir))
        for header in headers:
            for (searchDir, relDir) in itertools.product(searchDirs, relDirs):
                if os.path.isfile(os.path.join(searchDir, relDir, header)):
                    self.headerFiles.append(self.HeaderFile(searchDir,
                                                            relDir, header))
                    break
        for headerFile in self.headerFiles:
            with open(headerFile.fullPath, encoding = "utf-8", errors = "ignore") as fh:
                for line in fh:
                    words = line.split(None, 2)
                    if len(words) == 3 and words[0] == "#define":
                        newEntry = self.Entry(words[1], words[2])
                        self.entries[newEntry.name] = newEntry

    def dereference(self, name, *args):
        if name not in self.entries:
            return None
        baseName = name
        suffixes = list(args)
        maxDimensions = 0
        while True:
            if not suffixes:
                maxDimensions = max(maxDimensions,
                                    self.entries[baseName].dimensions)
            if suffixes and (baseName + suffixes[0]) in self.entries:
                baseName += suffixes[0]
                suffixes.pop(0)
            else:
                nextName = self.entries[baseName].getValue().strip('"')
                if nextName in self.entries:
                    baseName = nextName
                else:
                    break
        if suffixes:
            return None
        derefEntry = self.entries[baseName]
        if derefEntry.dimensions != maxDimensions:
            derefEntry = Entry(derefEntry.name, derefEntry.definition)
            derefEntry.dimensions = maxDimensions
        return derefEntry

# Each RegDesc represents a register address, field, or value parsed
# from reghal.def.
# Fields:
# - line: The line from reghal.def, as a tuple (line_number, line_contents).
#       Used for error messages
# - name: The name from reghal.def
# - regType: One of TYPE_ADDRESS, TYPE_FIELD, or TYPE_VALUE
# - modsName: The name of the address/type/field starting with MODS_
# - targets: Possible name/domains of the register/field/value, starting
#       with LW_.  Do not use externally; use getTargets() instead.
# - addressId, fieldId, valueId: Used to construct the enum value
# - domain: The lwlink domain, defaults to "RAW"
# - virtualMirror: A tuple ("LW_VIRTUAL_XXX", N) if -virtual_mirror
#       was used, or None.  On virtual machines, we replace the first
#       N chars of the register name with "LW_VIRTUAL_XXX".
# - arrayLimits: List that gives the limit of each array index, [] for scalars.
#       Only valid for TYPE_ADDRESS.
# - arrayReplace: List with the same number of elements as arrayLimits.
#       Contains None for ordinary indexes, and a pair [OLD, NEW] for
#       -replace_array indexes.
# - strideLwNames: List with the same number of elements as arrayLimits.
#       Contains None for ordinary indexes, and a stride "register" name
#       for indexes to multiply by the stride value.
# - replaceStrings: List with other replacements to perform.  Each
#       element is a pair [OLD, NEW] where NEW presumably contains
#       strings such as ${device}.
# - flags: List of RegHalTable::Flags flags.
#
class RegDesc:
    def __init__(self, line, name, parent, prevSibling):
        self.line = line
        if parent:
            _name = "_" + name
            if parent.regType == TYPE_ADDRESS:
                self.regType = TYPE_FIELD
            else:
                assert parent.regType == TYPE_FIELD
                self.regType = TYPE_VALUE
            self.name           = name
            self.modsName       = parent.modsName + _name
            self.targets        = [RegDesc.Target(ii.lwName + _name, ii.domain)
                                   for ii in parent.targets]
            self.addressId      = parent.addressId
            self.fieldId        = parent.fieldId
            self.valueId        = parent.valueId
            self.arrayLimits    = []
            self.arrayReplace   = []
            self.strideLwNames  = []
            self.replaceStrings = copy.deepcopy(parent.replaceStrings)
            self.virtualMirror  = parent.virtualMirror
            self.capabilityId   = parent.capabilityId
            self.flags          = copy.copy(parent.flags)
            for oldNew in parent.arrayReplace:
                self.replaceTargets(self.targets, oldNew, 0)
        else:
            self.name           = name
            self.regType        = TYPE_ADDRESS
            self.modsName       = name.replace("LW_", "MODS_", 1)
            self.targets        = [RegDesc.Target(name, "RAW")]
            self.addressId      = 1
            self.fieldId        = 0
            self.valueId        = 0
            self.arrayLimits    = []
            self.arrayReplace   = []
            self.strideLwNames  = []
            self.replaceStrings = []
            self.virtualMirror  = None
            self.capabilityId   = "0"
            self.flags          = []

        if prevSibling:
            # Enforce sorting order
            if self.name == prevSibling.name:
                die("Duplicate name", line)
            elif NaturalCompare(self.name, prevSibling.name) < 0:
                die(("Value '%s' in line %d does not follow sorting order!\n"
                     + "It is less than the preceding '%s' in line %d:\n%s")
                    % (self.name, line[0], prevSibling.name, prevSibling.line[0], line[1]))
            # Use next available ID
            if self.regType == TYPE_ADDRESS:
                self.addressId = prevSibling.addressId + 1
            elif self.regType == TYPE_FIELD:
                self.fieldId = prevSibling.fieldId + 1
            else:
                self.valueId = prevSibling.valueId + 1

    class Target:
        def __init__(self, lwName, domain):
            self.lwName = lwName
            self.domain = domain
            self.overrodeDomain = False

    def setDomain(self, domain):
        assert self.regType == TYPE_ADDRESS
        if self.targets[-1].overrodeDomain:
            die("Multiple domains", self.line)
        self.targets[-1].domain = domain.upper()
        self.targets[-1].overrodeDomain = True

    def setVirtualMirror(self, virtualMirror):
        assert self.regType == TYPE_ADDRESS
        if self.virtualMirror:
            die("Multiple -virtual_mirror or -virtual_export flags", self.line)
        elif self.modsName.startswith("MODS_VIRTUAL_"):
            die("-virtual_mirror not allowed on LW_VIRTUAL_ register",
                self.line)
        elif not virtualMirror.startswith("LW_VIRTUAL_"):
            die("-virtual_mirror argument must be a LW_VIRTUAL_ register",
                self.line)
        self.virtualMirror = (virtualMirror, self.name)

    def setVirtualExport(self):
        if self.virtualMirror:
            die("Multiple -virtual_mirror or -virtual_export flags", self.line)
        elif self.modsName.startswith("MODS_VIRTUAL_"):
            die("-virtual_export not allowed on LW_VIRTUAL_ register",
                self.line)
        self.virtualMirror = ("LW_", "LW_")

    def addReplaceArray(self, oldNew, limit):
        self.arrayLimits.append(int(limit))
        oldToStride = oldNew.split("*=")
        if len(oldToStride) == 2:
            self.arrayReplace.append([oldToStride[0], "%d"])
            self.strideLwNames.append(oldToStride[1])
        else:
            oldNew = oldNew.split("=")
            if len(oldNew) != 2:
                die("Bad -replace_array args, must be of form 'OLD=NEW LIMIT' or 'OLD*=STRIDE_REGISTER_NAME LIMIT'\n" +
                    "such as '-replace_array _x=_%d 8' or '-replace_array x*=LW_FBPA_PRI_STRIDE 8'", self.line)
            self.arrayReplace.append(oldNew)
            self.strideLwNames.append(None)

    def addReplaceString(self, oldNew):
        oldNew = oldNew.split("=")
        if len(oldNew) != 2:
            die("Bad -replace_string arg, must be of form 'OLD=NEW'\n" +
                "such as '-replace_string _device=_${device}'", self.line)
        self.replaceStrings.append(oldNew)

    def addAlias(self, alias):
        if self.regType == TYPE_ADDRESS:
            domain = self.targets[-1].domain
            self.targets.append(RegDesc.Target(alias, domain))
        else:
            newTargets = []
            for target in self.targets:
                if target.lwName.endswith("_" + self.name):
                    baseName = target.lwName[0:-len(self.name)]
                    newTargets.append(RegDesc.Target(baseName + alias,
                                                     target.domain))
            self.targets.extend(newTargets)

    def addCapabilityId(self, capabilityId):
        if self.capabilityId != "0":
            die("Multiple -cap flags", self.line)
        elif not capabilityId.startswith("LW_"):
            die("-cap string must start with LW_", self.line)
        self.capabilityId = "static_cast<UINT32>(MODS_{})".format(capabilityId[3:])

    def addArrayLimit(self, limit):
        self.arrayLimits.append(int(limit))
        self.arrayReplace.append(None)
        self.strideLwNames.append(None)

    def finishParsing(self):
        for (old, new) in self.replaceStrings:
            for target in self.targets:
                if target.lwName.find(old) < 0:
                    msg = "Cannot find '{}' in '{}'".format(old, target.lwName)
                    die(msg, self.line)
        for oldNew in self.arrayReplace:
            if oldNew is not None:
                for target in self.targets:
                    if target.lwName.find(oldNew[0]) < 0:
                        msg = "Cannot find '{}' in '{}'".format(oldNew[0],
                                                                target.lwName)
                        die(msg, self.line)

    # Return a list of all possible ways an address, field, or value
    # can be specified in hwref.  The return value is an array of elements
    # with the following properties:
    # * retval[i].indexTuple - A tuple representing an element of the array,
    #   such as (1,2) for a 2-D array, ("0xFFFFU", 1) for an array
    #   size, ("0xFFFEU", 1, 2) for the priv-level register for array
    #   element (1,2), or () for a scalar.
    # * retval[i].targets - An array of possible hwref entries for the
    #   aforementioned indexTuple:
    #   - retval[i].targets[j].lwName - A possible way the array
    #     element (or scalar) can be represented in hwref.
    #   - retval[i].targets[j].condition - An expression for an #if
    #     statement that tells whether lwName is valid, typically
    #     something like "defined(lwName)".
    #   - retval[i].targets[j].domain - The domain to use when reading
    #     or writing lwName; defaults to "RAW".
    #
    def getTargets(self, device):
        allIndexTuples = [()]
        for ii in range(len(self.arrayLimits)):
            allIndexTuples = [jj + (kk,) for jj in allIndexTuples
                                         for kk in range(self.arrayLimits[ii])]
        if self.regType == TYPE_ADDRESS:
            return self.getTargetsForAddress(device, allIndexTuples)
        else:
            return self.getTargetsForFieldOrValue(device, allIndexTuples)

    # The return value of getTargets() consists of an array of IndexedTargets
    class IndexedTargets:
        class Target:
            def __init__(self, condition, lwName, domain):
                self.condition = condition
                self.lwName    = lwName
                self.domain    = domain
        def __init__(self, indexTuple):
            self.indexTuple = indexTuple
            self.targets    = []
        def append(self, condition, name, domain):
            self.targets.append(self.Target(condition, name, domain))

    # Used by getTargets()
    def getTargetsForAddress(self, device, allIndexTuples):
        retval = []
        srcTargets = copy.deepcopy(self.targets)
        self.applyReplaceStrings(srcTargets, device)
        hwref = device.getHwref()

        # Array dimensions
        arrayTargets = copy.deepcopy(srcTargets)
        for repl in self.arrayReplace:
            self.replaceTargets(arrayTargets, repl, 0)
        suffixIndexCount = 0
        for ii in range(len(self.arrayReplace)):
            if not self.arrayReplace[ii]:
                suffixIndexCount += 1
                newRetvalEntry = RegDesc.IndexedTargets(("0xFFFFU", ii + 1))
                for target in arrayTargets:
                    lwName = "{}__SIZE_{}".format(target.lwName,
                                                  suffixIndexCount)
                    condition = "defined({})".format(lwName)
                    newRetvalEntry.append(condition, lwName, target.domain)
                retval.append(newRetvalEntry)

        # Addresses
        for indexTuple in allIndexTuples:
            # Get register names & stride at this index
            registerTargets = copy.deepcopy(srcTargets)
            strideOffset = ""
            suffixIndexes = []
            for ii in range(len(self.arrayReplace)):
                if self.strideLwNames[ii]:
                    self.replaceTargets(registerTargets,
                                        self.arrayReplace[ii], 0)
                    strideOffset += "{} * {} + ".format(indexTuple[ii],
                                                        self.strideLwNames[ii])
                elif self.arrayReplace[ii]:
                    self.replaceTargets(registerTargets,
                                        self.arrayReplace[ii], indexTuple[ii])
                else:
                    suffixIndexes.append(indexTuple[ii])

            # Get priv register
            for target in registerTargets:
                privRegister = hwref.dereference(target.lwName,
                                                 "__PRIV_LEVEL_MASK")
                if privRegister:
                    privName = privRegister.name
                    offset = self.getRegisterOffset(privName) + strideOffset
                    if privRegister.dimensions > 0:
                        privIndexes = suffixIndexes[:privRegister.dimensions]
                        privName += "("
                        privName += ",".join(str(ii) for ii in privIndexes)
                        privName += ")"
                    newRetvalEntry = RegDesc.IndexedTargets(("0xFFFEU",) +
                                                            indexTuple)
                    newRetvalEntry.append(None, offset + privName,
                                          target.domain)
                    retval.append(newRetvalEntry)
                    break

            # Get register
            newRetvalEntry = RegDesc.IndexedTargets(indexTuple)
            for target in registerTargets:
                offset = self.getRegisterOffset(target.lwName) + strideOffset
                if len(suffixIndexes) > 0 and all([ii == 0 for ii in suffixIndexes]):
                    # Some arrays aren't always arrays, so treat it as a
                    # scalar if there is no __SIZE_1 value
                    condition = "defined({}) && !defined({}__SIZE_1)".format(
                            target.lwName, target.lwName)
                    newRetvalEntry.append(condition, offset + target.lwName,
                                          target.domain)

                condition = "defined({})".format(target.lwName)
                suffix = ""
                effSuffixIndexes = suffixIndexes
                if target is not registerTargets[0]:
                    # old aliases might have extra dimensions
                    register = hwref.dereference(target.lwName)
                    if register and register.dimensions > len(effSuffixIndexes):
                        effSuffixIndexes += [0] * (register.dimensions -
                                                   len(effSuffixIndexes))
                if len(effSuffixIndexes) > 0:
                    condition += " && defined({}__SIZE_1)".format(target.lwName)
                    suffix += "("
                    suffix += ", ".join(str(ii) for ii in suffixIndexes)
                    suffix += ")"
                newRetvalEntry.append(condition,
                                      offset + target.lwName + suffix,
                                      target.domain)
            retval.append(newRetvalEntry)

        return retval

    # Used by getTargets
    def getTargetsForFieldOrValue(self, device, allIndexTuples):
        retval = []
        srcTargets = copy.deepcopy(self.targets)
        self.applyReplaceStrings(srcTargets, device)

        # Array dimensions
        for ii in range(len(self.arrayLimits)):
            newRetvalEntry = RegDesc.IndexedTargets(("0xFFFFU", ii + 1))
            for target in srcTargets:
                lwName = "{}__SIZE_{}".format(target.lwName, ii + 1)
                condition = "defined({})".format(lwName)
                newRetvalEntry.append(condition, lwName, target.domain)
            retval.append(newRetvalEntry)

        # Fields or values
        for indexTuple in allIndexTuples:
            suffix = ""
            if len(indexTuple) > 0:
                suffix += "("
                suffix += ", ".join([str(jj) for jj in indexTuple])
                suffix += ")"
            newRetvalEntry = RegDesc.IndexedTargets(indexTuple)
            for target in srcTargets:
                condition = "defined({})".format(target.lwName)
                if len(indexTuple) > 0:
                    condition += "defined({})".format(target.lwName)
                if self.regType == TYPE_FIELD:
                    lwName = "MODS_FIELD_RANGE({})".format(target.lwName)
                else:
                    lwName = target.lwName
                newRetvalEntry.append(condition, lwName + suffix, target.domain)
            retval.append(newRetvalEntry)
        return retval

    def applyReplaceStrings(self, targets, device):
        for (old, new) in self.replaceStrings:
            new = new.replace("${device}", device.name.lower())
            new = new.replace("${DEVICE}", device.name.upper())
            self.replaceTargets(targets, (old, new))

    @staticmethod
    def replaceTargets(targets, oldNew, index = None):
        if oldNew is not None:
            (old, new) = oldNew
            if index is None:
                for target in targets:
                    target.lwName = target.lwName.replace(old, new, 1)
            else:
                for target in targets:
                    target.lwName = target.lwName.replace(old, new % index, 1)

    def getRegisterOffset(self, lwName):
        if lwName.startswith("LW_VIRTUAL_"):
            if "PHYSICAL_ONLY" in self.flags:
                return "(0?MODS_VIRTUAL_FUNCTION_FULL_PHYS_OFFSET) + "
        return ""

    # Used when one RegDesc object should be expanded in several different
    # ways when we write the reghal entries (e.g. one entry for
    # physical machines and one for virtual).  Returns a list or tuple
    # of variant RegDesc objects, or None if this object doesn't have
    # any variants.
    def getVariants(self):
        if self.modsName.startswith("MODS_VIRTUAL_") and not self.flags:
            physReg = copy.deepcopy(self)
            virtReg = copy.deepcopy(self)
            physReg.flags.append("PHYSICAL_ONLY")
            virtReg.flags.append("VIRTUAL_ONLY")
            return (physReg, virtReg)
        if self.virtualMirror:
            physReg = copy.deepcopy(self)
            virtReg = copy.deepcopy(self)
            for target in virtReg.targets:
                if target.lwName.startswith(self.virtualMirror[1]):
                    target.lwName = (
                            self.virtualMirror[0] +
                            target.lwName[len(self.virtualMirror[1]):])
                else:
                    target.lwName = None
            virtReg.targets = [ii for ii in virtReg.targets
                               if ii.lwName is not None]
            physReg.virtualMirror = None
            virtReg.virtualMirror = None
            physReg.flags.append("PHYSICAL_ONLY")
            virtReg.flags.append("VIRTUAL_ONLY")
            return (physReg, virtReg)
        return None

# Write a .d file so that the Makefile knows when to rebuild
def WriteDependenciesFile(options, dependencies):
    if not options.dependencies_file:
        return
    deps = [os.path.abspath(dep).replace("\\", "/") for dep in dependencies]
    with open(options.dependencies_file, "w") as fh:
        fh.write("{}: \\\n".format(options.destFilePath))
        for dep in deps:
            fh.write(" {} \\\n".format(dep))
        fh.write("\n")
        for dep in deps:
            fh.write("{} :\n".format(dep))

parser = argparse.ArgumentParser()
parser.add_argument("-v", "--verbose", action="store_true",
                    help="enable verbose mode (default = False)")
parser.add_argument("-d", "--dependencies-file",
                    help="file to store makefile dependencies into")
parser.add_argument("modsPath", help="path to diag/mods directory")
parser.add_argument("driversPath", help="path to drivers directory")
parser.add_argument("destFilePath", help="full path to generated file")
options = parser.parse_args()

destFile = re.sub(".*[\\\\/]", "", options.destFilePath)

reComment = re.compile(" *#.*")
reTabs    = re.compile("\t")
reIndent  = re.compile("^ *")
reSpaces  = re.compile(" +")
reNumber  = re.compile("^[0-9]+$")

regHalDef = open(os.path.join(options.modsPath, "gpu", "reghal", "reghal.def"), "r").readlines()
regHalDef = list(zip(range(1, len(regHalDef) + 1), regHalDef))
regHalDef = [(line[0], reComment.sub("", line[1]).rstrip()) for line in regHalDef]
regHalDef = [(line[0], line[1]) for line in regHalDef if line[1] != ""]

if len([line[1] for line in regHalDef if reTabs.search(line[1]) != None]) > 0:
    die("Invalid reghal.def file format! Tabs found!")

# Build list of devices for each GPU from gpulist.h
devices = []
f = open(os.path.join(options.modsPath, "gpu", "include", "gpulist.h"))
reDefineNewGpu = re.compile(r'\s*DEFINE_(NEW|SIM)_GPU\s*\(\s*(\S[^)]*\S)\s*\)')
reDefineLwcfgGpu = re.compile(r'\s*DEFINE_GPU\s*\(\s*(\S[^)]*\S)\s*\)')
reCommaSeparator = re.compile(r'\s*,\s*')
ignoreNextLine = False
for line in f.readlines():
    m = reDefineNewGpu.match(line)
    n = reDefineLwcfgGpu.match(line) if not m else None
    if (m or n) and not ignoreNextLine:
        gpuArgs = reCommaSeparator.split(m.group(2) if m else n.group(1))
        if n:
            lwcfgSelector = gpuArgs[0]
            gpuArgs = gpuArgs[1:]
        else:
            lwcfgSelector = None
        if len(gpuArgs) >= 15:
            devices.append(Device(gpuArgs[3], gpuArgs[13], gpuArgs[14], lwcfgSelector))
    ignoreNextLine = line.rstrip().endswith("\\")
f.close()

# Build list of devices for each non-gpu lwlink device from lwlinklist.h
f = open(os.path.join(options.modsPath, "core", "include", "lwlinklist.h"))
reDefineDevice = re.compile(r'\s*DEFINE_(ARM_MFG_LWL|LWL|SIM_LWL)_DEV\s*\(\s*(\S[^)]*\S)\s*\)')
reDefineLwcfgDevice = re.compile(r'\s*DEFINE_LWL\s*\(\s*(\S[^)]*\S)\s*\)')
reCommaSeparator = re.compile(r'\s*,\s*')
ignoreNextLine = False
for line in f.readlines():
    m = reDefineDevice.match(line)
    n = reDefineLwcfgDevice.match(line) if not m else None
    if (m or n) and not ignoreNextLine:
        devArgs = reCommaSeparator.split(m.group(2) if m else n.group(1))
        if n:
            lwcfgSelector = "LWSWITCH_IMPL_" + devArgs[0]
            devArgs = devArgs[1:]
        else:
            lwcfgSelector = None
        devices.append(Device(devArgs[3], devArgs[5], devArgs[6], lwcfgSelector))
    ignoreNextLine = line.rstrip().endswith("\\")
f.close()

# Mark duplicate GPU devices which share headers with other GPUs (e.g. kickers)
for ii in range(len(devices)):
    if devices[ii].duplicate:
        continue
    for jj in range(ii + 1, len(devices)):
        if devices[ii].headerDir == devices[jj].headerDir:
            if devices[ii].dispDir != devices[jj].dispDir:
                if "AMODEL" not in [devices[ii].name, devices[jj].name]:
                    die("Invalid display headers directory for " +
                        devices[ii].name + "!\n" +
                        "It should be the same as for " + devices[jj].name)
            # Avoid dependency of T194 LWLink on T194, because T194 LWLink
            # is needed in linuxmfg builds where T194 is disabled.
            if devices[jj].name == "T194MFGLWL":
                continue
            devices[jj].duplicate = devices[ii].name

devices = sorted(devices, key=lambda device: device.name)

####################
# Parse reghal.def #
####################

modsRegHal  = []
indentStack = []
lwrRegAddress  = None
lwrRegField    = None
lwrRegValue    = None

for line in regHalDef:
    # Handle indentation
    level = len(reIndent.search(line[1]).group(0))
    if len(indentStack) == 0:
        indentStack.append(level)
    if level > indentStack[-1]:
        indentStack.append(level)
        if len(indentStack) > 3:
            die("Invalid indentation level", line)
    elif level < indentStack[-1]:
        indentStack.pop()
        if len(indentStack) == 2 and level < indentStack[-1]:
            indentStack.pop()
        if len(indentStack) == 0 or level != indentStack[-1]:
            die("Invalid indentation level", line)
    level = len(indentStack) - 1

    # Parse register name/field/value
    value = reSpaces.sub(" ", line[1]).strip().split(" ")

    # Add register definition
    if level == 0:
        # Possible values of the fields following the register name:
        # - nothing (no field) - in this case this is only a single register
        # - N (integer) - this is an array register, we support reading the
        #   array's size and *at most* N indices from 0 to N-1.  For the
        #   degenerate case of N=0, we only support the array's size.
        # - -domain XXX (string) - This register belongs to domain XXX.
        #   The default is "-domain raw".
        # - -virtual_mirror XXX: Used when we should use a different
        #   register XXX on virtual machines.
        # - -replace_array OLD=NEW N - This is an array register, in
        #   which we index into the array by replacing part of the
        #   register name.  For example, if the register name is
        #   given as "LW_GPCx_FOO", then "-replace_array GPCx=GPC%d 8"
        #   means that indexes 0 to 7 will access registers
        #   LW_GPC0_FOO to LW_GPC7_FOO.
        # - -replace_string OLD=NEW - Replace each instance of OLD
        #   with NEW, which may contain the following variables:
        #   ${device} The device name in lowercase, eg "tu100"
        #   ${DEVICE} The device name in uppercase, eg "TU100"
        #
        lwrRegAddress = RegDesc(line, value[0], None, lwrRegAddress)
        lwrRegField = None
        lwrRegValue = None
        ii = 1
        while ii < len(value):
            if value[ii] == "-domain" and ii + 1 < len(value):
                lwrRegAddress.setDomain(value[ii + 1])
                ii += 2
            elif value[ii] == "-virtual_mirror" and ii + 1 < len(value):
                lwrRegAddress.setVirtualMirror(value[ii + 1])
                ii += 2
            elif value[ii] == "-virtual_export":
                lwrRegAddress.setVirtualExport()
                ii += 1
            elif value[ii] == "-replace_array" and ii + 2 < len(value):
                lwrRegAddress.addReplaceArray(value[ii + 1], value[ii + 2])
                ii += 3
            elif value[ii] == "-replace_string" and ii + 1 < len(value):
                lwrRegAddress.addReplaceString(value[ii + 1])
                ii += 2
            elif value[ii] == "-alias" and ii + 1 < len(value):
                lwrRegAddress.addAlias(value[ii + 1])
                ii += 2
            elif value[ii] == "-cap" and ii + 1 < len(value):
                lwrRegAddress.addCapabilityId(value[ii + 1])
                ii += 2
            elif reNumber.match(value[ii]):
                lwrRegAddress.addArrayLimit(value[ii])
                ii += 1
            else:
                die("Invalid register address", line)
        lwrRegAddress.finishParsing()
        modsRegHal.append(lwrRegAddress)

    # Add register field definition
    elif level == 1:
        lwrRegField = RegDesc(line, value[0], lwrRegAddress, lwrRegField)
        lwrRegValue = None
        ii = 1
        while ii < len(value):
            if value[ii] == "-replace_string" and ii + 1 < len(value):
                lwrRegField.addReplaceString(value[ii + 1])
                ii += 2
            elif value[ii] == "-virtual_export":
                lwrRegField.setVirtualExport()
                ii += 1
            elif value[ii] == "-alias" and ii + 1 < len(value):
                lwrRegField.addAlias(value[ii + 1])
                ii += 2
            elif reNumber.match(value[ii]):
                lwrRegField.addArrayLimit(value[ii])
                ii += 1
            else:
                die("Invalid register field", line)
        lwrRegField.finishParsing()
        modsRegHal.append(lwrRegField)

    # Add register field value definition
    else:
        lwrRegValue = RegDesc(line, value[0], lwrRegField, lwrRegValue)
        ii = 1
        while ii < len(value):
            if value[ii] == "-replace_string" and ii + 1 < len(value):
                lwrRegValue.addReplaceString(value[ii + 1])
                ii += 2
            elif value[ii] == "-virtual_export":
                lwrRegValue.setVirtualExport()
                ii += 1
            elif value[ii] == "-alias" and ii + 1 < len(value):
                lwrRegValue.addAlias(value[ii + 1])
                ii += 2
            elif reNumber.match(value[ii]):
                lwrRegValue.addArrayLimit(value[ii])
                ii += 1
            else:
                die("Invalid register field value", line)
        lwrRegValue.finishParsing()
        modsRegHal.append(lwrRegValue)

########################
# Generate output file #
########################

def GenEnumString(regdesc, regType):
    if (regdesc.regType != regType):
        return ""
    modsRegName = regdesc.modsName
    valueName   = ((regdesc.addressId << POS_ADDRESS) |
                   (regdesc.fieldId   << POS_FIELD  ) |
                   (regdesc.valueId   << POS_VALUE  ) |
                   (regdesc.regType   << POS_TYPE   ))
    valign = " "
    if len(modsRegName) < 50:
        valign = (50 - len(modsRegName)) * " "
    return "    _" + modsRegName + valign + "= " + hex(valueName) + ",\n"

def GenMacroString(regdesc, regType, className):
    if (regdesc.regType != regType):
        return ""
    modsRegName = regdesc.modsName
    valign = " "
    if len(modsRegName) < 50:
        valign = (50 - len(modsRegName)) * " "
    return "#define " + modsRegName + valign + className + "::_" + modsRegName + "\n"

def GetBitMask(offset, bits):
    return ((1 << bits) - 1) << offset

if destFile == "mods_reg_hal.h":
    f = open(options.destFilePath, "w+")
    f.write(cppHeader)
    f.write('#pragma once\n')
    f.write("\n")
    f.write("#include \"core/include/types.h\"\n")
    f.write("\n")
    f.write("enum ModsGpuRegFlags : UINT32\n")
    f.write("{\n")
    f.write("    MODS_REGISTER_ADDRESS                   = 0x10000000,\n")
    f.write("    MODS_REGISTER_FIELD                     = 0x20000000,\n")
    f.write("    MODS_REGISTER_VALUE                     = 0x40000000,\n")
    f.write("\n")
    f.write("    MODS_REGISTER_ADDRESS_MASK              = 0x%X,\n" % GetBitMask(POS_ADDRESS, BITS_ADDRESS))
    f.write("    MODS_REGISTER_FIELD_MASK                = 0x%X,\n" % GetBitMask(POS_FIELD, BITS_FIELD))
    f.write("    MODS_REGISTER_VALUE_MASK                = 0x%X,\n" % GetBitMask(POS_VALUE, BITS_VALUE))
    f.write("    MODS_REGISTER_TYPE_MASK                 = 0x%X,\n" % GetBitMask(POS_TYPE, BITS_TYPE))
    f.write("\n")
    f.write("    MODS_REGISTER_ADDRESS_BITS              = %d,\n" % BITS_ADDRESS)
    f.write("    MODS_REGISTER_FIELD_BITS                = %d,\n" % BITS_FIELD)
    f.write("    MODS_REGISTER_VALUE_BITS                = %d,\n" % BITS_VALUE)
    f.write("    MODS_REGISTER_TYPE_BITS                 = %d\n"  % BITS_TYPE)
    f.write("};\n")
    f.write("\n")
    f.write("enum class ModsGpuRegAddress : UINT32\n")
    f.write("{\n")
    f.write("".join([GenEnumString(ii, TYPE_ADDRESS) for ii in modsRegHal]))
    f.write("    _MODS_REGISTER_ADDRESS_NULL                        = 0\n")
    f.write("};\n")
    f.write("\n")
    f.write("".join([GenMacroString(ii, TYPE_ADDRESS, "ModsGpuRegAddress") for ii in modsRegHal]))
    f.write("#define MODS_REGISTER_ADDRESS_NULL                        ModsGpuRegAddress::_MODS_REGISTER_ADDRESS_NULL\n")
    f.write("\n")
    f.write("enum class ModsGpuRegField : UINT32\n")
    f.write("{\n")
    f.write("".join([GenEnumString(ii, TYPE_FIELD) for ii in modsRegHal]))
    f.write("    _MODS_REGISTER_FIELD_NULL                          = 0\n")
    f.write("};\n")
    f.write("\n")
    f.write("".join([GenMacroString(ii, TYPE_FIELD, "ModsGpuRegField") for ii in modsRegHal]))
    f.write("#define MODS_REGISTER_FIELD_NULL                          ModsGpuRegField::_MODS_REGISTER_FIELD_NULL\n")
    f.write("\n")
    f.write("enum class ModsGpuRegValue : UINT32\n")
    f.write("{\n")
    f.write("".join([GenEnumString(ii, TYPE_VALUE) for ii in modsRegHal]))
    f.write("    _MODS_REGISTER_VALUE_NULL                          = 0\n")
    f.write("};\n")
    f.write("\n")
    f.write("".join([GenMacroString(ii, TYPE_VALUE, "ModsGpuRegValue") for ii in modsRegHal]))
    f.write("#define MODS_REGISTER_VALUE_NULL                          ModsGpuRegValue::_MODS_REGISTER_VALUE_NULL\n")
    f.write("\n")
    f.write("struct RawModsGpuRegAddressMap\n")
    f.write("{\n")
    f.write("    ModsGpuRegAddress value;\n")
    f.write("    const char*       name;\n")
    f.write("};\n")
    f.write("\n")
    f.write("struct RawModsGpuRegFieldMap\n")
    f.write("{\n")
    f.write("    ModsGpuRegField value;\n")
    f.write("    const char*     name;\n")
    f.write("};\n")
    f.write("\n")
    f.write("struct RawModsGpuRegValueMap\n")
    f.write("{\n")
    f.write("    ModsGpuRegValue value;\n")
    f.write("    const char*     name;\n")
    f.write("};\n")
    f.write("\n")
    sys.exit(0)

######################################################################
# Generate reghalconst.js
######################################################################

if destFile == "reghalconst.js":
    allowlist = []
    allowlistFile = os.path.join(options.modsPath, "gpu", "reghal",
                                 "allowlist.def")
    with open(allowlistFile) as fh:
        prevAllowlistEntry = None
        lineNum = 0
        for line in fh:
            lineNum += 1
            allowlistEntry = reComment.sub("", line).strip()
            if not allowlistEntry:
                continue
            if prevAllowlistEntry:
                if allowlistEntry == prevAllowlistEntry:
                    die("{}: Duplicate name '{}' in line {}".format(
                        allowlistFile, allowlistEntry, lineNum))
                if NaturalCompare(allowlistEntry, prevAllowlistEntry) < 0:
                    die(("{}: Value '{}' in line {} does not follow sorting" +
                         "order!  It is less than the preceding value '{}'")
                         .format(allowlistEntry, lineNum, prevAllowlistEntry))
            allowlist.append(allowlistEntry.replace("LW_", "MODS_", 1))
            prevAllowlistEntry = allowlistEntry

    with open(options.destFilePath, "w") as f:
        f.write(cppNogenHeader)
        f.write("#ifndef INCLUDED_REGHALCONST_JS\n")
        f.write("#define INCLUDED_REGHALCONST_JS\n")
        f.write("\n")
        f.write("Import(\"reghalinternal.js\", {}, false);\n")
        f.write("\n")
        f.write("if (typeof(RegHalConst) === \"undefined\")\n")
        f.write("{\n")
        f.write("    var RegHalConst = {")
        separator = "\n"
        for regDesc in modsRegHal:
            symbol = regDesc.modsName
            if symbol not in allowlist:
                continue
            value = ((regDesc.addressId << POS_ADDRESS) |
                     (regDesc.fieldId   << POS_FIELD  ) |
                     (regDesc.valueId   << POS_VALUE  ) |
                     (regDesc.regType   << POS_TYPE   ))
            f.write(separator)
            separator = ",\n"
            f.write("        {}: 0x{:08x}".format(symbol[5:], value))
        f.write("\n")
        f.write("    };\n")
        f.write("}\n")
        f.write("\n")
        f.write("#endif // INCLUDED_REGHALCONST_JS\n")
    sys.exit(0)

######################################################################
# Generate reghalinternal.js
######################################################################

if destFile == "reghalinternal.js":
    with open(options.destFilePath, "w") as f:
        f.write(cppHeader);
        f.write("#ifndef INCLUDED_REGHALINTERNAL_JS\n")
        f.write("#define INCLUDED_REGHALINTERNAL_JS\n")
        f.write("\n")
        f.write("if (typeof(RegHalConst) === \"undefined\")\n")
        f.write("{\n")
        f.write("    var RegHalConst = {\n")
        for regDesc in modsRegHal:
            symbol = regDesc.modsName
            value = ((regDesc.addressId << POS_ADDRESS) |
                     (regDesc.fieldId   << POS_FIELD  ) |
                     (regDesc.valueId   << POS_VALUE  ) |
                     (regDesc.regType   << POS_TYPE   ))
            if not symbol.startswith("MODS_"):
                raise ValueError("Symbol did not start with MODS_: " + symbol)
            f.write("        {}: 0x{:08x},\n".format(symbol[5:], value))
        f.write("        REGISTER_ADDRESS_NULL: 0,\n")
        f.write("        REGISTER_FIELD_NULL:   0,\n")
        f.write("        REGISTER_VALUE_NULL:   0,\n")
        f.write("        UNICAST:   0,\n")
        f.write("        MULTICAST: 1\n")
        f.write("    };\n")
        f.write("}\n")
        f.write("\n")
        f.write("#endif // INCLUDED_REGHALINTERNAL_JS\n")
    sys.exit(0)

###################################################################################################
# Generate mods_reg_hal_*.cpp - separate files are necessary because the data segment is so large #
###################################################################################################
if destFile == "mods_reg_hal_regs.cpp":
    import jsreghaltemplates
    addressSymbols = [ii.modsName for ii in modsRegHal if ii.regType == TYPE_ADDRESS]
    with open(options.destFilePath, "w") as f:
        f.write(cppHeader)
        f.write("#include \"gpu/reghal/reghal.h\"\n")
        f.write(jsreghaltemplates.makeStringTables("ModsGpuRegAddress", addressSymbols))
    sys.exit(0)

if destFile == "mods_reg_hal_fields.cpp":
    import jsreghaltemplates
    fieldSymbols   = [ii.modsName for ii in modsRegHal if ii.regType == TYPE_FIELD]
    with open(options.destFilePath, "w") as f:
        f.write(cppHeader)
        f.write("#include \"gpu/reghal/reghal.h\"\n")
        f.write(jsreghaltemplates.makeStringTables("ModsGpuRegField", fieldSymbols))
    sys.exit(0)

if destFile == "mods_reg_hal_values.cpp":
    import jsreghaltemplates
    valueSymbols   = [ii.modsName for ii in modsRegHal if ii.regType == TYPE_VALUE]
    with open(options.destFilePath, "w") as f:
        f.write(cppHeader)
        f.write("#include \"gpu/reghal/reghal.h\"\n")
        f.write(jsreghaltemplates.makeStringTables("ModsGpuRegValue", valueSymbols))
    sys.exit(0)

###############################
# Generate reghal_makesrc.inc #
###############################

if destFile == "reghal_makesrc.inc":
    f = open(options.destFilePath, "w")
    f.write(hashHeader)
    f.write("gen_reg_hal_files =\n")
    f.write("chip_gen_reg_hal_files =\n")
    for d in devices:
        if d.duplicate != None:
            continue
        if d.lwcfgSelector != None:
            f.write("ifeq \"$(LWCFG_GLOBAL_" + d.lwcfgSelector + ")\" \"1\"\n")
        f.write("chip_gen_reg_hal_files += " + d.fileName + "\n")
        f.write("gen_reg_hal_files += " + d.pmgrFileName + "\n")
        if d.lwcfgSelector != None:
            f.write("endif\n")
    f.close()
    sys.exit(0)

#########################################
# Generate reghal tables for every chip #
#########################################

def GenRegString(regdesc, device):
    variants = regdesc.getVariants()
    if variants:
        return "".join([GenRegString(ii, device) for ii in variants])

    modsName32 = "static_cast<UINT32>({})".format(regdesc.modsName)
    flags  = "|".join("RegHalTable::FLAG_" + ii for ii in regdesc.flags) or "0"
    retval = ""

    for indexedTargets in regdesc.getTargets(device):
        strIndexTuple = "{{" + ", ".join(
                [str(ii) for ii in (indexedTargets.indexTuple)]) + "}}"
        ifOrElif = "if"
        indent = ""
        for target in indexedTargets.targets:
            if target.condition is not None:
                retval += "#       {} {}\n".format(ifOrElif, target.condition)
                ifOrElif = "elif"
                indent = "    "
            retval += "        {}{{{}, {}, RegHalDomain::{}, {}, {}, {}}},\n".format(
                    indent, modsName32, strIndexTuple, target.domain,
                    regdesc.capabilityId, flags, target.lwName)
        if ifOrElif == "elif":
            retval += "#       endif\n"

    return retval

def HaveHeader(hdr):
    haveHeader = os.path.isfile(os.path.join(options.driversPath, "common", "inc", "hwref" , hdr))
    if not haveHeader:
        haveHeader = os.path.isfile(os.path.join(options.driversPath, "common", "inc", "swref" , hdr))
    if options.verbose and not haveHeader:
        print(destFile + " : " + hdr + " not found")
    return haveHeader

device = [d for d in devices if (d.fileName == destFile) or (d.pmgrFileName == destFile)]
if len(device) != 1:
    die("Unable to find register definition HAL for " + destFile)
device = device[0]
hwref = device.getHwref()

isPmgrMutex = destFile.endswith("_pmgrmutex.cpp")
bGenerateHwMutexIds = any(headerFile.baseName == "dev_pmgr_addendum.h"
                          for headerFile in hwref.headerFiles) and isPmgrMutex

f = open(options.destFilePath, "w+")
dependencies = []
f.write("/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/\n")
f.write("/*!! AUTO GENERATED USING diag/mods/tools/genreghalimpl.py !!*/\n")
f.write("/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/\n")
f.write("\n")
f.write('#include "gpu/reghal/reghaltable.h"\n')
dependencies.append(os.path.join(options.modsPath, "gpu/reghal/reghaltable.h"))
f.write("\n")
for headerFile in hwref.headerFiles:
    f.write('#include "' + headerFile.cppPath + '"\n')
    dependencies.append(headerFile.fullPath)
f.write('#include "gpu/include/pmgrmutex.h"\n') # Needs to come after addendum
dependencies.append(os.path.join(options.modsPath, "gpu/include/pmgrmutex.h"))
if bGenerateHwMutexIds:
    f.write('#include "../../arch/lwalloc/common/inc/lw_mutex.h"\n')
    f.write("\n")
    f.write("#if !defined(LW_MUTEX_ID_DISP_SCRATCH)\n")
    f.write("    #define LW_MUTEX_ID_DISP_SCRATCH LW_MUTEX_ID_DISP_SELWRE_WR_SCRATCH\n")
    f.write("#endif\n")
    dependencies.append(os.path.join(options.driversPath,
                        "resman/arch/lwalloc/common/inc/lw_mutex.h"))
f.write("\n")

if not isPmgrMutex:
    f.write("#define MODS_FIELD_RANGE(r) (((1?r)<<16) | (0?r))\n")
    f.write("#define MODS_VIRTUAL_FUNCTION_FULL_PHYS_OFFSET LW_VIRTUAL_FUNCTION_FULL_PHYS_OFFSET\n")
    f.write("\n")
    f.write("namespace\n")
    f.write("{\n")
    f.write("    const RegHalTable::Element regHalTable[] = {\n")
    f.write("".join([GenRegString(ii, device) for ii in modsRegHal]))
    f.write("        { static_cast<UINT32>(MODS_REGISTER_ADDRESS_NULL) }\n") # Dummy entry
    f.write("    };\n")
    f.write("}\n")
    f.write("\n")

for dev in [device] + [d for d in devices if d.duplicate == device.name]:
    if isPmgrMutex:
        f.write("DECLARE_PMGR_MUTEX_IDS(" + dev.name + ")\n")
    else:
        f.write("DECLARE_REGHAL_TABLE(" + dev.name + ", regHalTable)\n")
WriteDependenciesFile(options, dependencies)
