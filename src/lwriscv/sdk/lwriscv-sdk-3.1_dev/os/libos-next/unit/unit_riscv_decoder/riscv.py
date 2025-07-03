import random


def random_register():
    return random.choice(
        random.choice([ [ "x0"], 
                    [ "a0", "a1",  "a2",   "a3",  "a4", "a5", "a6",  "a7" ],
                    [ "ra" ],
                    [ "sp" ],
                    [ "t0", "t1",  "t2", "t3",  "t4", "t5", "t6" ],
                    [ "s0",  "s1", "s2",  "s3", "s4", "s5",  "s6", "s7",  "s8", "s9", "s10", "s11" ],
                    [ "gp", "tp" ]
    ]))

def random_compressible_register():
    return random.choice(["s0", "s1",  "a0", "a1",  "a2",   "a3",  "a4", "a5", "a6" ])

def random_loadstore():
    instr = random.choice([ "lb", "lh", "lw", "ld", "lbu", "lhu", "lwu", "sb", "sh", "sw", "sd" ])
    rd    = random_register()
    rs2   = random_register()
    imm   = random.randint(-2048, 2047)
    print("%s %s, %d(%s)" % (instr, rd, imm, rs2))


def random_li():
    rd    = random_register()
    imm   = random.randint(-2048, 2047)
    print("li %s, %d" % (rd, imm))

def random_lui():
    instr = random.choice([ "auipc", "lui" ])
    rd    = random_register()
    imm   = random.randint(0, (2**20)-1)
    print("%s %s, %d" % (instr, rd, imm))

def random_arith():
    instr = random.choice([ "div", "divw", "divu", "divuw", "rem", "remu", 
        "srlw", "sraw", "sll", "srl", "sra", "sllw", "sub", "subw", "addw",
        "mulw", "mul", "sub", "subw", "addw", "xor", "or", "and", "add",
        "addw", "remw", "remuw"])
    rd    = random_register()
    rs1   = random_register()
    rs2   = random_register()
    print("%s %s, %s, %s" % (instr, rd, rs1, rs2))

def random_likely_compressible_arith():
    instr = random.choice([ "sub", "xor", "or", "and", "subw", "addw"])
    rd    = random_compressible_register()
    rs1   = rd
    rs2   = random_compressible_register()
    print("%s %s, %s, %s" % (instr, rd, rs1, rs2))


def random_shift_immediate():
    instr = random.choice([ "srli", "srliw", "srai", "sraiw", "slli", "slliw"])
    rd    = random_register()
    rs1   = random_register()
    if instr[-1] == "w":
        imm   = random.randint(0, 31)
    else:
        imm   = random.randint(0, 63)
    print("%s %s, %s, %d" % (instr, rd, rs1, imm))

def random_compressible_addi4spn():
    rd    = random_compressible_register()
    imm   = random.randint(0, 255)*4
    print("addi %s, sp, %d" % (rd, imm))

def random_compressible_stackstore():
    instr = random.choice([ "lw", "ld", "sw", "sd" ])
    rd    = random_compressible_register()
    if instr[-1] == "w":
        imm   = random.randint(0, 0xC0) &~ 3
    else:
        imm   = random.randint(0, 0x1C0) &~ 7
    print("%s %s, %d(%s)" % (instr, rd, imm, "sp"))


def random_compressible_loadstore():
    instr = random.choice([ "lw", "ld", "sw", "sd" ])
    rd    = random_compressible_register()
    rs2   = random_compressible_register()
    if instr[-1] == "w":
        imm   = random.randint(0, 0x7F) &~ 3
    else:
        imm   = random.randint(0, 0xFF) &~ 7
    print("%s %s, %d(%s)" % (instr, rd, imm, rs2))

def random_arith_immediate():
    instr = random.choice([ "slti", "sltiu", "xori", "ori", "addi", "addiw", "andi" ])
    rd    = random_register()
    rs1   = random_register()
    imm   = random.randint(-2048, 2047)
    print("%s %s, %s, %d" % (instr, rd, rs1, imm))

def random_likey_compressible_arith_immediate():
    instr = random.choice([ "addi", "addiw", "andi" ])
    rd    = random_compressible_register()
    rs1   = random_compressible_register()
    imm   = random.randint(-0x3F, 0x3F)
    print("%s %s, %s, %d" % (instr, rd, rs1, imm))

def random_sys():
    instr = random.choice([ "ebreak", "mret", "sret", "wfi", "ecall" ])
    print("%s" % (instr))

#  RISCV_JAL,
#  RISCV_JALR,
#  RISCV_BEQ,
#  RISCV_BNE,
#  RISCV_BLT,
#  RISCV_BGE,
#  RISCV_BLTU,
#  RISCV_BGEU,
#  RISCV_CSRRW,
#  RISCV_CSRRS,
#  RISCV_CSRRWI,
#  RISCV_CSRRSI,
#  RISCV_CSRRCI,
def random_instruction():
    gen = random.choice([ 
        random_arith, 
        random_arith_immediate, 
        random_shift_immediate, 
        random_loadstore, 
        random_sys,
        random_li,
        random_lui,
        random_compressible_addi4spn,
        random_compressible_stackstore,
        random_compressible_loadstore,
        random_likey_compressible_arith_immediate,
        random_likely_compressible_arith
    ])
    gen()



for i in range(0,1000000):
    random_instruction()
