#!/usr/bin/python3
import os
import sys
import re
import subprocess

groupNames="MMODE RISCV_CTL PIC TIMER HOSTIF DMA PMB DIO KEY DEBUG SHA KMEM BROM ROM_PATCH IOPMP NOACCESS SCP FBIF FALCON_ONLY PRGN_CTL SCRATCH_GROUP0 SCRATCH_GROUP1 SCRATCH_GROUP2 SCRATCH_GROUP3 PLM HUB_DIO".split()

mappingDir=os.elwiron['P4ROOT'] +"/sw/lwriscv/main/tools/device-map-analyzer/db"

if (len(sys.argv) != 4):
    print("Usage:", sys.argv[0], "<chip: ampere-ga102, hopper-gh100, t23x-t234> <engine> <objdump file>")
    sys.exit(0)

chipName=sys.argv[1]
engineName=sys.argv[2]
objdumpFile=sys.argv[3]

class Register:
    def __init__(self, name, address, group):
        self.name=name
        self.address=int(address)
        self.group=group

    def __str__(self):
        return "{} 0x{:x}".format(self.name, self.address)

class Group:
    def __init__(self,name):
        self.name=name
        self.regs=[]
        self.regs_sz={}

    def __str__(self):
        return "("+self.name+":"+str(len(self.regs))+")"

class Engine:

    def __init__(self, name):
        self.groups=[] # groups
        self.addrs={} # reverse register lookup (by address)
        self.names={} # register lookup by name (string)
        self.name=name

    def __str__(self):
        return self.name + ": " + ", ".join(map(str,self.groups))


e = Engine(engineName)

def bar():
    print("="*79)

def parseManuals():
    # Create register mappings
    for g in groupNames:
        groupFile=mappingDir + "/" + chipName + "-" + engineName + "-" + g
        groupFileSz=groupFile + "-sz"
        group = Group(g)
        e.groups.append(group)
        print("Processing", groupFile)
        gf = open(groupFileSz, "r")
        lines=gf.readlines()
        for i in lines:
            i = i.split()
            if (len(i)==2):
                group.regs_sz[i[0]]=i[1]
        # print("Got SZ registers: ",group.regs_sz)
        gf = open(groupFile, "r")
        lines = gf.readlines()
        print("Tentative", len(lines),"registers")
        for regLine in lines:
            regStr=regLine.split()
            if (len(regStr)!=2):
                raise  Exception("WTF, regstr:"+regStr)

            if regStr[0].find("(i)") >= 0:
                regName=regStr[0][0:regStr[0].find("(i)")]
                regNameSz=regName+"__SIZE_1"
                # print("Complex reg:",regName)
                if group.regs_sz[regNameSz]:
                    # print("Entry count",group.regs_sz[regNameSz])
                    for i in range(0, int(group.regs_sz[regNameSz])):
                        name=regStr[0].replace("(i)","({0})".format(i))
                        addr=eval(regStr[1])
                        reg=Register(name, addr, group)
                        group.regs.append(reg)
                        if addr in e.addrs:
                            raise Exception("Address", addr, "already defined.")
                        else:
                            e.addrs[addr]=reg
                            e.names[name]=reg
                        # print(reg)
            else:
                name=regStr[0]
                # Skip plm
                if "__PRIV_LEVEL_MASK" in name:
                   # print("Skipping",name)
                    continue
                addr=eval(regStr[1])
                reg=Register(name, addr, group)
                group.regs.append(reg)
                if addr in e.addrs:
                    raise Exception("Address", addr, "already defined.")
                else:
                    e.addrs[addr]=reg
                    e.names[name]=reg
                # print(reg)
        print("Real", len(group.regs),"registers")

parseManuals()
bar()
print("Total", len(e.addrs),"registers")
print(e)

class UsedReg:
    def __init__(self,reg):
        self.reg=reg
        self.stores=set()
        self.loads=set()
        self.rogues=set()
    def access(self, function, isLoad):
        if isLoad:
            self.loads.add(function)
        else:
            self.stores.add(function)
    def taintedAccess(self, function):
        self.rogues.add(function)
    def isTainted(self):
        return len(self.rogues) > 0
    # cleanup tainted set
    def taintedClean(self):
        self.rogues = self.rogues - (self.loads & self.stores)


usedRegs={} # address : usedReg

# Parse file
f = open(objdumpFile,"r")
lines=f.readlines()
rx_functionName = re.compile(r"[\da-f]+ <(\S+)>:\n")

#  1000fa:       b0f52023                sw      a5,-1280(a0) # 1403b00 <lsbHeaders+0x127ee08>
#  1001e8:       10072503                lw      a0,256(a4) # 1401100 <lsbHeaders+0x127c408>
rx_loads  = re.compile(r"\s*[\da-f]+:\s+[\da-f]+\s+lw\s+[^#]+# ([\da-f]+)")
rx_stores = re.compile(r"\s*[\da-f]+:\s+[\da-f]+\s+sw\s+[^#]+# ([\da-f]+)")
rx_registerName = re.compile(r"(LW_PRGNLCL_[A-Za-z_]+)")

lwrrentFunction="?"
for line in lines:
    m = rx_functionName.fullmatch(line)
    if m:
        lwrrentFunction = m.group(1)
        # print("Function:", lwrrentFunction)

    address=0
    isLoad=False

    m = rx_loads.match(line)
    if m:
        address=int("0x"+m.group(1), 16)
        isLoad=True

    m = rx_stores.match(line)
    if m:
        address=int("0x"+m.group(1), 16)
        isLoad=False

    if address in e.addrs:
        reg = e.addrs[address]
        if not address in usedRegs:
            u = UsedReg(reg)
            usedRegs[address]=u
        else:
            u = usedRegs[address]
        u = usedRegs[address]
        u.access(lwrrentFunction, isLoad)

    # Now check for registers we didn't catch
    m = rx_registerName.search(line)
    if m:
        name=m.group(1)
        if name in e.names:
            reg = e.names[name]
            if reg.address in usedRegs:
                u = usedRegs[reg.address]
                u.taintedAccess(lwrrentFunction)
            else:
                u = UsedReg(reg)
                usedRegs[reg.address] = u
                u.taintedAccess(lwrrentFunction)

# Now grep source for outliers
bar()
print("Checking source for outliers...")
ret = subprocess.call(["/bin/grep", "LW_P"+str.upper(e.name), objdumpFile])

bar()
print("Accessed registers.. per group")
allGroups=set(e.groups)
ldGroups=set()
stGroups=set()

for group in e.groups:
    for reg in group.regs:
        if reg.address in usedRegs:
            u = usedRegs[reg.address]
            u.taintedClean()
            print(group.name,"/",reg.name)
            if len(u.loads):
                print("\tLoad  in",", ".join(u.loads))
                ldGroups.add(group)
            if len(u.stores):
                print("\tStore in",", ".join(u.stores))
                stGroups.add(group)
            if u.isTainted():
                print("\tTainted in",", ".join(u.rogues))
                # We don't know if it's load or store :(
                stGroups.add(group)
                ldGroups.add(group)

usedGroups=set.union(ldGroups, stGroups)
unusedGroups=allGroups - usedGroups
ldOnlyGroups=ldGroups-stGroups
stOnlyGroups=stGroups-ldGroups

bar()
if len(unusedGroups):
    print("Unused groups:", ",".join(map(str,unusedGroups)))

if len(ldOnlyGroups):
    print("Read-only groups:", ",".join(map(str,ldOnlyGroups)))

if len(stOnlyGroups):
    print("Write-only groups:", ",".join(map(str,stOnlyGroups)))



#  1223baa:       123b33                lw      a0,256(a4) # 1401100 <zzz+zzz>
