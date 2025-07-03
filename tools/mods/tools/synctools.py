#!/usr/bin/elw python3

# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

# This file contains tools for copying directories from one Perforce
# server to another via a cron job.  It is lwrrently used by
# //sw/dev/gpu_drv/chips_a/drivers/common/amaplib/contrib_sw/syncfiles.pl
# and //sw/dev/gpu_drv/chips_a/diag/mods/tools/syncfiles.pl
#
import getpass
import os
import re
import subprocess
import sys
import tempfile

######################################################################
# This class manages a changelist that copies files from one Perforce
# server to another.  Typical usage is to create the object, call
# copy() one or more times, and then call submit().  Calling submit()
# more than once is not supported.
#
# This class does not create the Perforce client or edit files until
# submit() is called, mostly because it uses data from the copy()
# method(s) to build the clientspec.
#
class Changelist:
    ##################################################################
    # Constructor parameters are:
    # * srcPort: The Perforce port to copy from, eg "p4hw:2001"
    # * dstPort: The Perforce port to copy to, eg "p4sw:2006"
    #
    def __init__(self, srcPort, dstPort):
        self._srcPort = srcPort
        self._dstPort = dstPort
        self._spec = []            # Clientspec files passed to Client
        self._filesToAdd           = []  # List of (srcFile, dstFile) tuples to copy
        self._filesToDelete        = []  # List of files to delete from dstPort
        self._filesToCopy          = []  # List of (srcFile, dstFile) tuples to add
        self._filesToProcessForAdd = []  # List of (srcFiles, dstFile) tuples to proccess and add
        self._filesToProcess       = []  # List of (srcFiles, dstFile) tuples to proccess
        self._fileTypes            = {}  # Maps src files to their types (eg "text")

    ##################################################################
    # Sets up a process operation to be done when submit() is called to process
    # a list of files using a custom processing function into a destination file (allows
    # for the destination file name to be different than that of the source file)
    #
    def processFiles(self, srcFiles, dstFile, processFunc, allowDelete = True):
        processFiles = []
        for srcFile in srcFiles:
            (srcDir, lwrFiles) = self._listP4Files(self._srcPort, srcFile, None)
            for lwrFile in lwrFiles:
                processFiles += [os.path.join(srcDir, lwrFile)]
        (dstDir, dstFiles) = self._listP4Files(self._dstPort, dstFile, None)
        self._spec.append(dstFile)

        if len(dstFiles) and len(processFiles):
            srcData = [ self._readP4File(self._srcPort, srcFile) for srcFile in processFiles ]
            dstData = self._readP4File(self._dstPort, dstFile)
            processedData = processFunc(srcData, dstData)
            if processedData != dstData:
                self._filesToProcess += [(srcFiles, dstFile, processFunc)]
                return (srcFiles, dstFile, 'changed')
        elif len(processFiles):
            self._filesToProcessForAdd += [(srcFiles, dstFile, processFunc)]
            return (srcFiles, dstFile, 'added')
        elif len(dstFiles) and allowDelete:
            self._filesToDelete += [dstFile]
            return (srcFiles, dstFile, 'deleted')
        return None,None,None

    ##################################################################
    # Sets up a copy operation to be done when submit() is called to copy
    # a specific file (allows for the destination file name to be different
    # than that of the source file)
    #
    def copyFile(self, srcFile, dstFile, allowDelete = True):
        (srcDir, srcFiles) = self._listP4Files(self._srcPort, srcFile, None)
        (dstDir, dstFiles) = self._listP4Files(self._dstPort, dstFile, None)
        self._spec.append(dstFile)

        if len(dstFiles) and len(srcFiles):
            srcData = self._readP4File(self._srcPort, srcFile)
            dstData = self._readP4File(self._dstPort, dstFile)
            if srcData != dstData:
                self._filesToCopy += [(srcFile, dstFile)]
        elif len(srcFiles):
            self._filesToAdd += [(srcFile, dstFile)]
        elif len(dstFiles) and allowDelete:
            self._filesToDelete += [dstFile]

    ##################################################################
    # Sets up a copy operation to be done when submit() is called.
    # Dst files will be added, deleted, or edited as needed in order
    # to match the src.  Parameters are:
    # * srcGlob: Files(s) to copy from srcPort, typically a directory
    #   ending in "..." such as " "hw/lwgpu/clib/lwshared/amaplib/..."
    # * dstGlob: Where to copy the files into dstPort, eg
    #   "//sw/dev/gpu_drv/chips_hw/drivers/common/amaplib/..."
    # *ignore:  Optional regular-expression string that tells which
    #  files(s) should NOT be copied from srcGlob to dstGlob.  The
    #  expression must match the part of the filename(s) that follows
    # dirname(srcGlob) and/or dirname(dstGlob).
    #
    def copy(self, srcGlob, dstGlob, ignore = None):
        (srcDir, srcFiles) = self._listP4Files(self._srcPort, srcGlob, ignore)
        (dstDir, dstFiles) = self._listP4Files(self._dstPort, dstGlob, ignore)
        self._spec.append(dstGlob)

        for ii in set(srcFiles + dstFiles):
            srcFile = srcDir + ii
            dstFile = dstDir + ii
            if ii not in dstFiles:
                self._filesToAdd += [(srcFile, dstFile)]
            elif ii not in srcFiles:
                self._filesToDelete += [dstFile]
            else:
                srcData = self._readP4File(self._srcPort, srcFile)
                dstData = self._readP4File(self._dstPort, dstFile)
                if srcData != dstData:
                    self._filesToCopy += [(srcFile, dstFile)]

    ##################################################################
    # Submit the changes that were set up by copy(), if any.
    #
    def createCl(self, p4Root=None):
        if not (self._filesToAdd or self._filesToDelete or self._filesToCopy or
                self._filesToProcessForAdd or self._filesToProcess):
            print("No files have changed, skipping CL creation")
            return 0
        with Client(self._dstPort, self._spec, p4Root) as client:
            return self._addFilesToCL(client)

    def submit(self, p4Root=None):
        if not (self._filesToAdd or self._filesToDelete or self._filesToCopy or
                self._filesToProcessForAdd or self._filesToProcess):
            return
        with Client(self._dstPort, self._spec, p4Root) as client:
            clNum = self._addFilesToCL(client)
            self.doSubmit(self._dstPort, client.name(), clNum)

    ##################################################################
    # Submit one changelist.  Normally called from submit().  This
    # method is designed to be overridden, because different libraries
    # have different submit procedures.
    #
    def doSubmit(self, portName, clientName, clNum):
        submitCmd = ["as2", "-p", portName, "-c", clientName,
                     "submit", "-c", clNum, "-project", "sw_inf_sw_mods"]
        subprocess.run(submitCmd, check = True,
                       stdout = subprocess.PIPE,
                       stderr = subprocess.PIPE)

    ##################################################################
    # Returns a list of Perforce files.  Parameters are:
    # * port: The Perforce port to search, eg "p4hw:2001"
    # * glob: Describes the files(s) to list, typically as a directory
    #   ending in "..." such as " "hw/lwgpu/clib/lwshared/amaplib/..."
    # *ignore:  Optional regular-expression string that tells which
    #  files(s) should NOT be listed.  The  expression must match the
    #  part of the filename(s) that follows dirname(glob).
    #
    # The return value is of the form [p4Dir, [file, file, ...]]
    # where "p4Dir" is essentially dirname(glob), and each "file" is a
    # filenanme relative to p4Dir.
    #
    # As a side-effect, this method sets self._fileTypes[p4Dir + file]
    # to the Perforce filetype of each file.  See "p4 help filetypes".
    #
    # This method only returns regular files, not symlinks.
    #
    def _listP4Files(self, port, glob, ignore):
        files = []
        p4Dir = glob[:glob.rfind("/")] + "/"
        dirLen = len(p4Dir)
        reLine = re.compile(r"(.*)#\d+ - (\w+) change \d+ \((.+)\)")
        for line in runP4Cmd(["files", glob], port = port).splitlines():
            match = reLine.match(line)
            if match:
                fileName   = match.group(1)
                lastAction = match.group(2)
                fileType   = match.group(3)
                if lastAction != "delete" and fileType != "symlink":
                    files.append(fileName[dirLen:])
                    self._fileTypes[fileName] = fileType
        if ignore:
            reIgnore = re.compile(ignore)
            files = [ii for ii in files if not reIgnore.match(ii)]
        return (p4Dir, files)

    ##################################################################
    # Add files to CL:
    def _addFilesToCL(self, client):
        srcTot = runP4Cmd(["changes", "-m1", "-ssubmitted"],
                          port = self._srcPort).split()[1]
        desc  = "Change: new\n"
        desc += "Description:\n"
        desc += "\tRebase files from {} @ changelist {}\n".format(
                                                   self._srcPort, srcTot)
        desc += "\t\n"
        desc += "\tNot reviewed by svcmods\n"
        clNum = client.run(["change", "-i"], input = desc).split()[1]
        for (srcFile, dstFile) in self._filesToAdd:
            client.writeFile(dstFile,
                             self._readP4File(self._srcPort, srcFile))
            client.run(["add", "-c", clNum,
                        "-t", self._fileTypes[srcFile], dstFile])
        for dstFile in self._filesToDelete:
            client.run(["delete", "-c", clNum, dstFile])
        for (srcFile, dstFile) in self._filesToCopy:
            client.run(["edit", "-c", clNum,
                        "-t", self._fileTypes[srcFile], dstFile])
            client.writeFile(dstFile,
                             self._readP4File(self._srcPort, srcFile))
        for (srcFiles, dstFile, processFunc) in self._filesToProcessForAdd:
            srcData = [ self._readP4File(self._srcPort, srcFile) for srcFile in srcFiles ]
            processedData = processFunc(srcData)
            client.writeFile(dstFile, processedData)
            client.run(["add", "-c", clNum,
                        "-t", self._fileTypes[srcFiles[0]], dstFile])
        for (srcFiles, dstFile, processFunc) in self._filesToProcess:
            srcData = [ self._readP4File(self._srcPort, srcFile) for srcFile in srcFiles ]
            dstData = self._readP4File(self._dstPort, dstFile)
            processedData = processFunc(srcData, dstData)
            client.run(["edit", "-c", clNum,
                        "-t", self._fileTypes[srcFiles[0]], dstFile])
            client.writeFile(dstFile, processedData)
        return clNum

    ##################################################################
    # Read a Perforce file and return a "bytes" object containing the
    # file contents.
    #
    @staticmethod
    def _readP4File(port, srcFile):
        proc = subprocess.run(["p4", "-p", port, "print", "-q", srcFile],
                              stdout = subprocess.PIPE,
                              stderr = subprocess.PIPE,
                              check = True)
        return proc.stdout

######################################################################
# This class creates a temporary Perforce client.  It is designed to
# be used with the "with" statement; the client gets deleted when the
# "with" statement ends.
#
class Client:
    ##################################################################
    # Constructor: Create and sync a temporary client in a temporary
    # directory.  The parameters are:
    # * port: The port to create the client on, such as "p4hw:2001"
    # * spec: List of files to put in the clientspec, such as
    #   ["//hw/lwgpu/clib/lwshared/amaplib/..."]
    #
    def __init__(self, port, spec, p4root=None):
        self._port = port
        if not p4root:
            self._tempClient = True
            self._root = tempfile.TemporaryDirectory()
            self._p4root = self._root.name
            self._name = "{}-{}".format(os.elwiron["P4USER"],
                                        os.path.basename(self._root.name))
            clientSpec  = "Client: {}\n".format(self._name)
            clientSpec += "Owner: {}\n".format(os.elwiron["P4USER"])
            clientSpec += "Description:\n"
            clientSpec += "\tTemporary client created by {}\n".format(sys.argv[0])
            clientSpec += "Root: {}\n".format(self._root.name)
            clientSpec += "View:\n"
            for ii in spec:
                clientSpec += "\t{} //{}{}\n".format(ii, self._name, ii[1:])
            self.run(["client", "-i"], input = clientSpec)
            self.run(["sync"])
        else:
            self._tempClient = False
            self._name = None
            self._p4root = p4root

    ##################################################################
    # Get the client's name
    #
    def name(self):
        return self._name

    ##################################################################
    # Run a perforce command on the client and return the output.
    # Parameters are:
    # * cmd: Args following "p4" on the cmd line, eg ["sync", "-f"]
    # * input/check:  Optional args passed to subprocess.run()
    #
    def run(self, cmd, input = None, check = True):
        return runP4Cmd(cmd, port = self._port, client = self._name,
                        input = input, check = check)

    ##################################################################
    # Write a file on the client, creating the file and its parent
    # directory if needed.  This method only modifies the file
    # contents; the caller needs to run commands such as "p4 edit" and
    # "p4 submit" to submit it to Perforce.
    #
    def writeFile(self, dstFile, data):
        dstPath = os.path.join(self._p4root,
                               dstFile[2:].replace("/", os.sep))
        os.makedirs(os.path.dirname(dstPath), exist_ok = True)
        with open(dstPath, mode = "wb") as dstFile:
            dstFile.write(data)

    ##################################################################
    # Delete the client, along with any pending edits or changelists
    # on the client.  This method is called automatically if the
    # caller uses a "with" statement.
    #
    def cleanup(self):
        if self._tempClient:
            self.run(["revert", "-k", "//..."], check = False)
            p4Changes = self.run(["changes", "-spending", "-c", self._name],
                                 check = False)
            for line in p4Changes.splitlines():
                self.run(["change", "-d", line.split()[1]], check = False)
            self.run(["client", "-d", self._name], check = False)
            self._root.cleanup()

    ##################################################################
    # Automatically called at the start of a "with" statement
    #
    def __enter__(self):
        return self

    ##################################################################
    # Automatically called at the end of a "with" statement
    #
    def __exit__(self, type, value, traceback):
        self.cleanup()

######################################################################
# Run a Perforce command and return the output.  Parameters are:
# * cmd: Args following "p4" on the cmd line, eg ["sync", "-f"]
# * port: The perforce port, eg "p4hw:2001"
# * client: The perforce client, if any
# * input/check:  Optional args passed to subprocess.run()
#
def runP4Cmd(cmd, port = None, client = None, input = None, check = True):
    fullCmd = ["p4"]
    if port:
        fullCmd += ["-p", port]
    if client:
        fullCmd += ["-c", client]
    fullCmd += cmd
    proc = subprocess.run(fullCmd, universal_newlines = True,
                          stdout = subprocess.PIPE, stderr = subprocess.PIPE,
                          input = input, check = check)
    return proc.stdout

######################################################################
# Wrapper around main function for cron job
#
def cronWrapper(mainFunction):
    # Initialize the environment vars at the start of the cron job
    #
    os.elwiron["P4USER"] = getpass.getuser()
    os.elwiron["PATH"] = "/bin:/usr/bin:/home/utils/bin:/home/lw/bin"

    # Call the main function, and print error messages on an exception
    #
    try:
        rc = mainFunction()
    except subprocess.CalledProcessError as err:
        print("Exit code {} from command {}".format(err.returncode, err.cmd))
        if err.stderr:
            print("STDERR:")
            print(str(err.stderr, encoding = "utf-8", errors = "ignore"))
        if err.stdout:
            print("STDOUT:")
            print(str(err.stdout, encoding = "utf-8", errors = "ignore"))
        rc = 1
    except Exception as err:
        if (str(err)):
            print(err)
        rc = 1
    sys.exit(rc)
