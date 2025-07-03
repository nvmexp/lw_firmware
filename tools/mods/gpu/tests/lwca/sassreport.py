#! /usr/bin/elw python

# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import os, sys, string, subprocess, re, glob, tempfile, struct

def parse_lwbin(counts, fname):
    """Disassemble and parse a single testname.lwbin file"""
    
    print('... disassembling %s' % fname)
    test = re.sub(r'.*t([0-9]+)_.*', r'\1',fname)
    cmd = 'fatdump --dump-sass ' + fname
    
    try:
        run_cmd_and_parse_sass(counts, cmd, test)
    except Exception as ex:
        print('Failed to extract SASS for %s: %s' % (fname, ex))
        print('Failed to run "%s"' % cmd)
        return usage()

def run_cmd_and_parse_sass(counts, cmd, test):
    """Run a subprocess to generate SASS text to a pipe and parse into counts."""
    
    p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE)
    parse_sass(p.stdout, counts, test)
    if (p.returncode):
        print('return code %d from "%s"' % (p.returncode, cmd))
        raise Exception('cmd error')

def parse_modslog_with_rawcode(counts, fname):
    """Extract each "rawcode" OpenGL shader dump from mods.log, parse it.
    
    Colwert each rawcode from hex-ascii to binary, relocate it and write it
    to the filesystem, then process with sass and parse like a .lwbin file.
    
    We track the current test number from the mods.log file and pass it to 
    the file-parser encoded in the temp binary filename.
    """
    flog = open(fname, 'r')
    code = None
    ucbase = 0
    test = ''
    profile = ''
    lnum = 0
    
    for line in flog:
        lnum = lnum + 1
        m = re.match(r'Enter .*\(test ([0-9]{1,3})\)', line)
        if (m):
            # Track current test number.
            test = m.group(1)
            continue
        
        if (re.match(r'rawcode:', line)):
            if (code == None):
                # Begin new rawcode dump
                code = []
            nums = string.split(line[8:])
            try:
                for num in nums:
                    # Colwert from ascii hex to binary, append to current dump.
                    i = int(num, 16)
                    if (i < 0):
                        raise Exception('range?')
                    code.append(int(num, 16))
            except Exception as ex:
                print('Rawcode corrupt at %d: "%s"' % (lnum, line))
            continue

        m = re.search(r'<<< ucode \(([0-9a-fA-F]+)\)', line)
        if m:
            # Store relocation data we'll need later
            ucbase = int(m.group(1), 16)
            continue
            
        if (code != None):
            # Completed rawcode dump.
            
            # Relocate (see //sw/apps/gpu/drivers/opengl/lw50dissall/lw50dissall.pl).
            nsections = code[2] & 0xffff
            for i in range(nsections):
                code[8 + 8*i + 2] -= ucbase
            
            # Create temp binary file, encoding the test number.
            tmpname = './t%s_%d_sassreport.bin' % (test, counts['numprogs'])
            f = open(tmpname, 'w+b')
            for w in code:
                f.write(struct.pack('<I', w))
            f.close()
            code = None
            #print '%d wrote %d words to %s' % (lnum, len(code), tmpname) # @@@
            
            # Disassemble the temp file and parse results into 'counts'
            cmd = 'sass2 -d %s' % tmpname
            counts['arch'] = 'sm_20'

            try:
                run_cmd_and_parse_sass(counts, cmd, test)
            except Exception as ex:
                print('Failed to extract SASS from pgm %d in mods.log : %s' % (
                        counts['numprogs'], ex))
                print('Failed to run "%s"' % cmd)
                return usage()
            os.system('rm %s' % tmpname)
            print('test %s, %d programs so far' % (test, counts['numprogs']))

def parse_sass(lines, counts, test):    
    """Parse sass text, update counts.  Lwrrently supports only Fermi."""
    
    counts['numprogs'] = counts['numprogs'] + 1
    counts['tests'].add(test)
    
    for line in lines:
        # remove comments
        line = re.sub(r'/\*[0-9a-fx ]+\*/', '', line);
        line = re.sub(r'#.*$', '', line);

        # strip leading and trailing {-style SHINT notation from 
        # lwdisasm_internal.
        line = re.sub(r'\{', '', line)
        line = re.sub(r'\}', '', line)
        
        # strip leading and trailing wspace
        line = line.strip()
        
        # skip lines that are not code
        if (not line.endswith(';') or line.startswith(';')):
            continue
        
        # strip leading branch-labels
        line = re.sub(r'L[0-9a-f]{4,4}:[ ]+', '', line)
        
        # force all numbered registers and branch labels to be the same
        line = re.sub(r'R[0-9]{1,2}([^P])', r'R%d\1', line);
        line = re.sub(r'BB[0-9]{1,2}', r'BB%d', line);
        line = re.sub(r'L[0-9a-f]{4,4}', r'L%x', line);
        line = re.sub(r'.L_[0-9]{1,4}', r'L%x', line);

        # Colwert symbolic labels to be the same
        line = re.sub(r'\`\(\(\S+', r'`((LABEL', line);

        # handle hints from lwdisasm_internal
        line = re.sub(r' +\(\*', r' (*', line);
        line = re.sub(r'\"WAIT[0-9]+\"', r'"WAIT%x"', line);

        # force all predicate prefixes to be the same
        line = re.sub(r'\@\!?P[T0-9]', r'%pred', line)
        
        # force literals to be the same
        line = re.sub(r' (-{0,1})[0-9]\.[0-9]+e[+\-][0-9]+', r' \1%g', line);
        line = re.sub(r' (-{0,1})[0-9]\.[0-9]+', r' \1%f', line);
        line = re.sub(r'([ \[+]{1})(-{0,1})0x[0-9a-f]{1,16}', r'\1\2%x', line);
        line = re.sub(r'([ \[\+]{1})(-{0,1})[0-9]{1,10}', r'\1\2%d', line);
        
        count_strings(counts, line, test)

def count_strings(counts, s, test):
    """Update counts to store a parsed instruction string and test number."""
    
    if s in counts:
        counts[s]['n'] = counts[s]['n'] + 1
        counts[s]['tests'].add(test)
    else:
        counts[s] = { 'n':1, 'tests':set([test]) }
        counts['roots'].add(ins_root(s))

def report_counts(counts, sass, reportname, revision, rev_counts):
    """Print a report of the instructions we saw."""

    arch = counts['arch']
    all_roots = get_roots(arch)

    used_roots = counts['roots']
    missed_roots = None
    unexpected_roots = None

    # Kepler has LDSLK and STSLWL at the same instruction codes where
    # Fermi has LDS_LDU and LD_LDU.
    # Some SASS tools use the old Fermi names by mistake.
    # Alias these to each other to prevent false coverage reports.
    def alias_codes(used_roots):
        if (arch == 'sm_30'):
            if ('LDSLK' in used_roots):
                used_roots.add('LDS_LDU')
            if ('LDS_LDU' in used_roots):
                used_roots.add('LDSLK')
            if ('STSLWL' in used_roots):
                used_roots.add('LD_LDU')
            if ('LD_LDU' in used_roots):
                used_roots.add('STSLWL')
        
    # Alias instruction codes to prevent false "lost coverage".
    alias_codes(used_roots)

    if rev_counts != None:
        rev_used_roots = rev_counts['roots']
        # Alias instruction codes to prevent false "gained coverage".
        alias_codes(rev_used_roots)

    rev_missed_roots = None
    rev_unexpected_roots = None

    if all_roots != None:
        missed_roots = all_roots - used_roots
        unexpected_roots = used_roots - all_roots
        if revision != None:
            rev_missed_roots = all_roots - rev_used_roots
            rev_unexpected_roots = rev_used_roots - all_roots

    cov_gained = used_roots - rev_used_roots
    cov_lost = rev_used_roots - used_roots

    divider = "----------------------------------------------------------------------------\n"
    
    f = open(reportname, 'w')
    if sass != None:
        f.write('SASS File: %s\n' % os.path.basename(sass))
    f.write('Architecture: %s\n' % arch)
    f.write('\n')
    f.write(divider)
    f.write('Summary\n')
    f.write(divider)

    if all_roots == None:
        f.write('Unsupported architecture: Cannot report missed/unexpected instructions\n\n')

    # report the coverage of the current sass file
    f.write('Current Coverage\n')
    f.write('# of Tests: %d\n' % (len(counts['tests'])))
    f.write('# of Shaders: %d\n' % (counts['numprogs']))
    f.write('# of Instructions Used: %d\n' % (len(used_roots)))
    if all_roots != None:
        f.write('# of Instructions Missed: %d\n' % len(missed_roots))
        f.write('# of Unexpected Instructions: %d\n' % len(unexpected_roots))
    f.write('\n')

    if revision != None:
        # report coverage of a previous revision of the sass file
        f.write('Coverage of Revision #%s\n' % revision)
        f.write('# of Tests: %d\n' % (len(rev_counts['tests'])))
        f.write('# of Shaders: %d\n' % (rev_counts['numprogs']))
        f.write('# of Instructions Used: %d\n' % (len(rev_used_roots)))
        if all_roots != None:
            f.write('# of Instructions Missed: %d\n' % len(rev_missed_roots))
            f.write('# of Unexpected Instructions: %d\n' % len(rev_unexpected_roots))
        f.write('\n')

        # comparison between current and previous sass file
        f.write('Coverage gained (%d): %s\n' % (len(cov_gained),','.join(sorted(cov_gained))))
        f.write('Coverage lost (%d): %s\n' % (len(cov_lost),','.join(sorted(cov_lost))))
        f.write('\n')

    f.write(divider)
    f.write('Instructions Used\n')
    f.write(divider)
    f.write('%s\n' % (','.join(sorted(used_roots))))
    f.write('\n')

    if all_roots != None:
        f.write(divider)
        f.write('Instructions Missed\n')
        f.write(divider)
        f.write('%s\n' % (','.join(sorted(missed_roots))))
        f.write('\n')

        f.write(divider)
        f.write('Unexpected Instructions\n')
        f.write(divider)
        f.write('%s\n' % (','.join(sorted(unexpected_roots))))
        f.write('\n')

    f.write(divider)
    f.write('Tests\n')
    f.write(divider)
    f.write('%s\n' % (','.join(sorted(counts['tests']))))
    f.write('\n')

    f.write(divider)
    f.write("Detailed Instruction Breakdown\n")
    f.write(divider)
    f.write('Instruction             , Count,            Test, Long Form\n')
    keywords = set(['numprogs', 'roots', 'tests', 'arch'])
    x = []
    for txt in list(counts.keys()):
        if (txt in keywords):
            continue
        x.append('%-24s, %5d, %15s, %s\n' % (
                ins_base(txt), 
                counts[txt]['n'],
                ' '.join(counts[txt]['tests']),
                txt))
    f.writelines(sorted(x))

    f.close
    print('Wrote report to %s' % reportname)

def get_roots(arch):
    roots = None
    if arch == 'sm_30':
        roots = set([
            'FFMA','FFMA32I','FADD'
            ,'FADD32I','FCMP','FMUL'
            ,'FMUL32I','FMNMX','FSWZ'
            ,'FCCO','FSET','FSETP'
            ,'FCHK','IPA'
            ,'RRO','MUFU','DFMA'
            ,'DADD','DMUL','DMNMX'
            ,'DSET','DSETP','IMAD'
            ,'IMAD32I','IMADSP','IMUL'
            ,'IMUL32I','IADD','IADD32I'
            ,'ISCADD','ISCADD32I','SHR'
            ,'IMNMX','BFE','BFI'
            ,'SHR','SHL','SHF'
            ,'LOP','LOP32I','FLO'
            ,'ISET','ISETP','ICMP'
            ,'POPC'
            ,'VMAD','VADD','VABSDIFF'
            ,'VMNMX','VSET','VSETP'
            ,'VSHL','VSHR','VADD2'
            ,'VABSDIFF2','VMNMX2','VSET2'
            ,'VSEL2','VADD4','VABSDIFF4'
            ,'VMNMX4','VSET4','VSEL4'
            ,'F2F','F2I','I2F'
            ,'I2I'
            ,'MOV','MOV32I','SEL'
            ,'PRMT','SHFL'
            ,'P2R','R2P','CSET'
            ,'CSETP','PSET','PSETP'
            ,'TEX','TLD','TLD4'
            ,'TXD','TXA','TXQ'
            ,'TMML','LDG','CCTLT'
            ,'STP','TEXDEPBAR'
            ,'VILD','AL2P','ALD'
            ,'AST','OUT','PIXLD'
            ,'LDC','LD','LDL'
            ,'LDS','LDSLK','ST'
            ,'STL','STS','STSLWL'
            ,'ATOM','RED','CCTL'
            ,'CCTLL','CCTLS','MEMBAR'
            ,'SUCLAMP','SUBFM','SUEAU'
            ,'SULDGA','SUSTGA'
            ,'BRA','BRX','JMP'
            ,'JMX','CAL','JCAL'
            ,'EXIT','RET','KIL'
            ,'BRK','CONT','LONGJMP'
            ,'SSY','PBK','PCNT'
            ,'PRET','PLONGJMP','SAM'
            ,'RAM','BPT','RTT'
            ,'IDE'
            ,'NOP','SHINT','S2R'
            ,'B2R','R2B','LEPC'
            ,'BAR','VOTE','SETCRSPTR'
            ,'GETCRSPTR','SETLMEMBASE','GETLMEMBASE'
            ,'ISAD'])
            
    elif arch == 'sm_20':
        roots = set([
            'ALD','AST','ATOM','B2R'
            ,'BAR','BFE','BFI','BPT'
            ,'BRA','BRK','BRX','CAL'
            ,'CCTL','CCTLL','CONT','CSET'
            ,'CSETP','DADD','DFMA','DMNMX'
            ,'DMUL','DSET','DSETP','EXIT'
            ,'F2F','F2I','FADD','FADD32I'
            ,'FCCO','FCMP','FFMA','FFMA32I'
            ,'FLO','FMNMX','FMUL','FMUL32I'
            ,'FSET','FSETP','FSWZ','I2F'
            ,'I2I','IADD','IADD32I','ICMP'
            ,'IMAD','IMAD32I','IMNMX','IMUL'
            ,'IMUL32I','IPA','ISAD','ISCADD'
            ,'ISCADD32I','ISET','ISETP','JCAL'
            ,'JMP','JMX','KIL','LD'
            ,'LD_LDU','LDC','LDL','LDLK'
            ,'LDS','LDS_LDU','LDSLK','LDU'
            ,'LEPC','LONGJMP','LOP','LOP32I'
            ,'MEMBAR','MOV','MOV32I','MUFU'
            ,'NOP','OUT','P2R','PBK'
            ,'PCNT','PIXLD','PLONGJMP','POPC'
            ,'PRET','PRMT','PSET','PSETP'
            ,'R2P','RAM','RED','RET'
            ,'RRO','RTT','S2R','SAM'
            ,'SEL','SHL','SHR','SSY'
            ,'ST','STL','STP','STS'
            ,'STSUL','STUL','SULD','SULEA'
            ,'SUQ','SURED','SUST','TEX'
            ,'TLD','TLD4','TMML','TXA'
            ,'TXD','TXQ','VABSDIFF','VABSDIFF2'
            ,'VABSDIFF4','VADD','VADD2','VADD4'
            ,'VILD','VMAD','VMNMX','VMNMX2'
            ,'VMNMX4','VOTE','VSEL2','VSEL4'
            ,'VSET','VSET2','VSET4','VSETP'
            ,'VSHL','VSHR'])
    return roots

def ins_base(ins):
    """Extract the instruction name and suffix from a line of SASS assembly."""
    if (0 == ins.find(r'%pred', 0)):
        ins = ins[5:]
        ins = ins.strip()
    return re.sub(r'^\s*(\S+) .*$', r'\1', ins)

def ins_root(ins):
    """Extract just the instruction name from a line of SASS assembly."""
    return re.sub(r'^([A-Z0-9_]+).*$', r'\1', ins_base(ins))

if __name__ == "__main__":
    sys.exit(main())


