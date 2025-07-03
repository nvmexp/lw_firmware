#!/usr/bin/elw python
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('def_file')
parser.add_argument('c_file')
parser.add_argument('exports', metavar='exports', type=str, nargs=argparse.REMAINDER,
                   help='additional functions to export')
args = parser.parse_args()

exports = []

with open(args.def_file) as f:
    for line in f:
        line = line.strip()
        if not line or line.startswith(';') or line.startswith('EXPORTS'):
            continue
        exports.append(line)

for e in args.exports:
    exports.append(e)

with open(args.c_file, 'w') as f:
    for e in exports:
        f.write('extern "C" void %s(void) {}\n' % e)
