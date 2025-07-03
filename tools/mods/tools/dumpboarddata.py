#!/usr/bin/elw python

##############################################################################
# LWIDIA_COPYRIGHT_BEGIN                                                     #
#                                                                            #
# Copyright 2019 by LWPU Corporation.  All rights reserved.  All           #
# information contained herein is proprietary and confidential to LWPU     #
# Corporation.  Any use, reproduction, or disclosure without the written     #
# permission of LWPU Corporation is prohibited.                            #
#                                                                            #
# LWIDIA_COPYRIGHT_END                                                       #
##############################################################################
# This script is used for dumping boarddata.xml for the purpose of           #
# putting board identifiers in diag/mods/gpu/js/eud.js.                      #
# We removed fuse files and boards.db files from EUD MODS and now we're      #
# using BoardId and PciDeviceId to identify supported boards.  These values  #
# are defined unambiguously in boarddata.xml.                                #
##############################################################################

from __future__ import print_function # Python 2 compatibility
import os
import sys
import xml.parsers.expat

p4root = os.getelw("P4ROOT")

if p4root == None:
    print("P4ROOT elw var is not set!", file=sys.stderr)
    sys.exit(1)

bd_path = os.path.join(p4root, "sw", "main", "bios", "common", "etc", "boarddata.xml")
if not os.path.isfile(bd_path):
    print("You must sync //sw/main/bios/common/etc/boarddata.xml", file=sys.stderr)
    sys.exit(1)

class Board:
    def __init__(self, id):
        self.id       = id
        self.pciDevId = None
        self.desc     = None

board = None
capturing_desc = False

def StartElement(name, attrs):
    global board
    global capturing_desc
    if name == "board":
        assert board == None
        assert not capturing_desc
        board = Board(attrs["id"])
    elif name == "pci":
        assert board != None
        assert not capturing_desc
        board.pciDevid = attrs["deviceid"]
    elif name == "description":
        capturing_desc = True

def EndElement(name):
    global board
    global capturing_desc
    if name == "board":
        assert board != None
        assert not capturing_desc
        print("{ boardId: " + board.id + ", pciDevid: " + board.pciDevid + " }, // " + board.desc.strip())
        board = None
    elif name == "description":
        capturing_desc = False

def CharData(data):
    if capturing_desc:
        if board.desc == None:
            board.desc = data
        else:
            board.desc += data

p = xml.parsers.expat.ParserCreate()

p.StartElementHandler  = StartElement
p.EndElementHandler    = EndElement
p.CharacterDataHandler = CharData

with open(bd_path, "r") as f:
    p.Parse(f.read())
