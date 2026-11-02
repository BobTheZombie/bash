#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Differential tests against Bash arithmetic and independent Python integers."""
import os, pathlib, random, subprocess, sys
shell = str(pathlib.Path(sys.argv[1]).resolve())
plugin = str(pathlib.Path(sys.argv[2]).resolve())
r = random.Random(47021)
mask = (1 << 64) - 1

def signed(x):
    x &= mask
    return x - (1 << 64) if x >> 63 else x

def run(script, *args, check=True):
    return subprocess.run([shell, '--noprofile', '--norc', '-c', script, '_', plugin, *map(str,args)],
                          text=True, capture_output=True, check=check)

load = 'enable -f "$1" lpmjit; enable -f "$1" lpmvec; shift; '
def ast(depth=0):
    if depth > 3 or r.randrange(4) == 0:
        return r.choice(['x', str(r.getrandbits(64)), str(r.randrange(50))])
    if r.randrange(5) == 0:
        return '-(' + ast(depth+1) + ')'
    return '(' + ast(depth+1) + r.choice(['+', '-', '*']) + ast(depth+1) + ')'

for i in range(250):
    expr = ast()
    values = [0, 1, -1, (1 << 63)-1, -(1 << 63), mask] + [r.getrandbits(64) for _ in range(8)]
    native = run(load + 'lpmjit "$@"', expr, *values).stdout
    interpreted = run(load + 'lpmjit --interpret "$@"', expr, *values).stdout
    bash = run('shift; e=$1; shift; for x; do printf "%s\\n" "$(( ' + expr + ' ))"; done', expr, *values).stdout
    assert native == interpreted == bash, (expr, native, interpreted, bash)

for n in [1, 2, 3, 4, 5, 7, 8, 9, 255, 256, 257, 4097]:
    values = [r.getrandbits(64) for _ in range(n)]
    m, b = r.getrandbits(64), r.getrandbits(64)
    got = list(map(int, run(load + 'lpmvec "$@"', m, b, *values).stdout.split()))
    assert got == [signed(x*m+b) for x in values], (n, m, b)

for bad in ['', 'x/2', 'x**2', 'x=2', 'x++', 'y', '1; echo BAD', '$(id)',
            '0x10', '18446744073709551616', '(' * 65 + 'x' + ')' * 65,
            'x+' * 1500 + 'x', 'x x', '()', '+', 'x\x01']:
    got = run(load + 'lpmjit "$@"', bad, 1, check=False)
    assert got.returncode and not got.stdout, (bad, got)
for bad in ['1a', '', '--1', '18446744073709551616']:
    got = run(load + 'lpmjit x 1 "$1"', bad, check=False)
    assert got.returncode and not got.stdout
    got = run(load + 'lpmvec 1 0 1 "$1"', bad, check=False)
    assert got.returncode and not got.stdout
# A device is optional. If present, verify its actual results; otherwise demand
# an explicit failure with no numeric output. No silent CPU substitution.
got = run(load + 'lpmvec --gpu 18446744073709551615 7 0 1 -1 4294967297', check=False)
if got.returncode == 0:
    assert list(map(int, got.stdout.split())) == [7, 6, 8, -4294967290]
    print('OpenCL GPU smoke test passed')
else:
    assert not got.stdout and 'OpenCL GPU' in got.stderr
    print('OpenCL GPU unavailable: error path passed; device execution untested')
assert run(load + 'lpmjit "x*2+1" 3 5').stdout == '7\n11\n'
print('3500 JIT input evaluations matched interpreter and Bash; SIMD and rejection tests passed')
