#!/usr/bin/python
#
# This script generates a pageable text section of num_pages in size and
# goes thru the following steps:
#   - initialize the section to zero
#   - at a random offset in each page inserts a sequence of instructions
#   - the sequence is:  addiw (increment a counter) and then a branch
#   - the branch is a jal instruction in every page but the last; that is a ret
#   - the task thread jumps to the sequence of instructions in the first page
#   - control is returned to the task thread after the ret in the last page
#   - the counter is checked to ensure that the number of branches matches
#
import os
import sys
import random

random.seed(100)

if len(sys.argv) != 2:
    sys.stderr.write("Invalid arg count: %d\n" % len(sys.argv))
    sys.exit(1)

output_c_file = sys.argv[1]
output_h_file = output_c_file.replace(".c", ".h", 1)

#
# Size in (pages) of pageable text section.
#
num_pages=128

#
# Instruction formats.
#
op_ret=0x00008067    # ret
op_nop=0x0000001b    # addiw x0, x0, 0
op_add=0x0015051b    # addiw a0, a0, 1

#
# Function to generate jal instruction given an immediate and dest reg.
#
# Mapping of immediate bit to corresponding jal opcode bits:
#   imm[20]    -> jal[31]
#   imm[10:1]  -> jal[30:21]
#   imm[11]    -> jal[20]
#   imm[19:12] -> jal[19:12]
#
#   jal[6:0]=0x6f (opcode)
#   jal[11:7]=rd  (register holding ra)
#
def op_jal(imm, rd):
    if imm < 0:
        assert(imm >= -(1<<20))
        imm = ((0xFFFFFFFF ^ -imm) + 1) & 0x1FFFFF
    else:
        assert(imm < (1<<20))
    val = ((((imm >> 20) & 1) << 31) |
           (((imm >> 1) & 0x3ff) << 21) |
           (((imm >> 11) & 1) << 20) |
           (((imm >> 12) & 0xff) << 12) |
           0x6f | (rd << 7))
    return val

#
# Initialize pageable text section with zero to start.
#
paged_text_section = [ 0 for i in range(num_pages*1024) ]

per_page_offsets = [ 0 for i in range(num_pages) ]

instr_seq_offsets = [ ]
instr_seq = [ ]

page_num = 0

done = False
while not done:
    offset = random.randint(0, ((num_pages * 0x400) - 1))

    #
    # Our (32bit) offset must allow for two instructions to be inserted in the page.
    #
    if ((offset % 0x3ff) > 0x3fe):
        continue

    #
    # If this page already has an instruction sequence then skip it.
    #
    page_num = offset / 0x400
    if (per_page_offsets[page_num] != 0):
        continue

    #
    # Append instruction sequence offset to list.
    #
    instr_seq_offsets.append(offset);

    #
    # Save (32bit register) offset.  This is where the instruction sequence
    # will be placed.
    #
    per_page_offsets[page_num] = offset

    #
    # Break when all pages have been patched.
    #
    if 0 not in per_page_offsets:
        break

#
# Generate the addi and jal instructions for all but the last page.
#
for i, offset in enumerate(instr_seq_offsets):
    if i < (len(instr_seq_offsets) - 1):
        instr_seq.append(op_add)
        instr_seq.append(op_jal(((instr_seq_offsets[i + 1] - (offset + 1)) << 2), 0))

#
# Generate the addi and ret instructions for the last page.
#
instr_seq.append(op_add)
instr_seq.append(op_ret)

#
# Patch up pageable text.  Add instruction first (which increments counter
# stored in a0) followed by the jal (or ret if last page).
#
for i, offset in enumerate(instr_seq_offsets):
    paged_text_section[offset] = instr_seq[(i * 2)]
    paged_text_section[offset + 1] = instr_seq[(i * 2) + 1]

#
# Open c and h files.
# Note we put the declarations that we need to be able to get at in
# test-driver.cpp into a header file that can be #included where needed.
#
pageable_text_c_file = open(output_c_file, "w")
pageable_text_h_file = open(output_h_file, "w")

#
# Header file first.
#
pageable_text_h_file.write("//\n")
pageable_text_h_file.write("// %s\n" % output_h_file)
pageable_text_h_file.write("//\n")
pageable_text_h_file.write("// Contains declarations needed in both pageable_text.c and test-driver.cpp\n");
pageable_text_h_file.write("//\n")

pageable_text_h_file.write("#ifdef __cplusplus\n");
pageable_text_h_file.write("extern \"C\" {\n");
pageable_text_h_file.write("#endif\n\n");

pageable_text_h_file.write("LwU32 numPages = 0x%x;\n\n" % num_pages)

#
# Emit instruction sequence offsets.  These will be used to verify
# expected trap addresses.
#
pageable_text_h_file.write("//\n")
pageable_text_h_file.write("// Offsets (one per page) into pageable text section where\n")
pageable_text_h_file.write("// patched instruction sequences are placed.\n")
pageable_text_h_file.write("//\n")

pageable_text_h_file.write("LwU64 instrSeqOffsets[] =\n")
pageable_text_h_file.write("{\n")
for offset in instr_seq_offsets:
    pageable_text_h_file.write("    0x%016x,\n" % offset)
pageable_text_h_file.write("};\n\n")

pageable_text_h_file.write("#ifdef __cplusplus\n");
pageable_text_h_file.write("}\n");
pageable_text_h_file.write("#endif\n");

#
# Source file now.
#
pageable_text_c_file.write("//\n")
pageable_text_c_file.write("// %s\n" % output_c_file)
pageable_text_c_file.write("//\n")
pageable_text_c_file.write("// Contains pageable text section contents.\n");
pageable_text_c_file.write("//\n\n")

pageable_text_c_file.write("#include \"libos.h\"\n\n");
pageable_text_c_file.write("#include \"%s\"\n\n" % os.path.basename(output_h_file))

#
# Emit instruction sequences.  These will be used to verify
# expected instruction flow.
#
pageable_text_c_file.write("//\n")
pageable_text_c_file.write("// Instruction sequence.\n")
pageable_text_c_file.write("// These should be present in the testFuncAsm array that is emitted below.\n")
pageable_text_c_file.write("//\n")

pageable_text_c_file.write("LwU32 instrSeq[] =\n")
pageable_text_c_file.write("{\n")
for i, offset in enumerate(instr_seq_offsets):
    pageable_text_c_file.write("    0x%08x," % instr_seq[(i * 2)])
    pageable_text_c_file.write("    // offset 0x%08x\n" % (offset << 2));
    pageable_text_c_file.write("    0x%08x," % instr_seq[(i * 2) + 1])
    pageable_text_c_file.write("    // offset 0x%08x\n" % ((offset + 1) << 2));
pageable_text_c_file.write("};\n\n")

#
# Pageable text section.
#
pageable_text_c_file.write("__attribute__ ((section (\".paged_text\"))) LwU32 testFuncAsm[] =\n")
pageable_text_c_file.write("{\n")

for i in xrange(len(paged_text_section)):
    if ((i % 8) == 0):
        pageable_text_c_file.write("    ")

    pageable_text_c_file.write("0x%08x, " % paged_text_section[i])

    if (((i+1) % 8) == 0):
        pageable_text_c_file.write("    // 0x%08x - 0x%08x\n" % (((i - 7) * 4), (((i + 1 ) * 4) - 1)))

pageable_text_c_file.write("};\n")
