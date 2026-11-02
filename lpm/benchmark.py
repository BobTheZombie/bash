#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Report medians for one opt-in numeric workload. Includes startup/output costs."""
import pathlib, statistics, subprocess, sys, time
build=pathlib.Path(sys.argv[1]).resolve()
shell=str(build/'bash'); plugin=str(build/'lpm-accel.so')
script='enable -f "$1" lpmjit; enable -f "$1" lpmvec; shift; '
values=list(map(str,range(10000)))
cases={
 'bash arithmetic loop': ('shift; for x; do printf "%s\\n" "$((x*x+3*x+7))"; done',[]),
 'JIT batch': (script+'lpmjit "x*x+3*x+7" "$@"',[]),
 'interpreted bytecode batch': (script+'lpmjit --interpret "x*x+3*x+7" "$@"',[]),
 'SIMD affine batch (different expression)': (script+'lpmvec 3 7 "$@"',[]),
}
for label,(body,_) in cases.items():
 times=[]
 for _ in range(9):
  t=time.perf_counter()
  subprocess.run([shell,'--noprofile','--norc','-c',body,'_',plugin,*values],stdout=subprocess.DEVNULL,check=True)
  times.append(time.perf_counter()-t)
 print(f'{label}: median {statistics.median(times)*1000:.3f} ms (9 runs)')
