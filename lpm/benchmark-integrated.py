#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare the same existing Bash arithmetic loop with native JIT off and on."""
import pathlib,statistics,subprocess,sys,time
shell=str(pathlib.Path(sys.argv[1]).resolve())
body='x=0; for ((i=0;i<300000;i++)); do ((x+=i*3+7)); done; echo "$x"'
expected='135001650000\n'
results={False:[],True:[]}
# Alternate execution order to reduce warmup/order effects.
for round in range(7):
    for enabled in ([False,True] if round%2==0 else [True,False]):
        script=f'shopt -{"s" if enabled else "u"} arithmetic_jit; '+body
        start=time.perf_counter()
        p=subprocess.run([shell,'--noprofile','--norc','-c',script],capture_output=True,text=True,check=True)
        results[enabled].append(time.perf_counter()-start)
        assert p.stdout==expected and not p.stderr,p
for enabled in results:
    print(f'JIT {"on" if enabled else "off"}: median {statistics.median(results[enabled]):.6f} s (7 runs)')
print(f'Speed ratio for this arithmetic loop: {statistics.median(results[False])/statistics.median(results[True]):.3f}x')
