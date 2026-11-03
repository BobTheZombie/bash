# Integrated CPU arithmetic JIT validation

Local environment: Linux x86-64, GCC 13.3.0. The independent reference was
built from the previous published commit, `99d4560`, using the same portable
`-O3` flags and allocator configuration. Results below do not claim Ryzen
hardware or GPU measurements.

- Portable and Zen 2 builds completed.
- Differential integration suite compared JIT on, JIT off and the independent
  reference, including exit status, stdout and diagnostics. Each profile's
  run observed 1,065 compiled entries and 11,893 native evaluations across
  generated expression tests, plus assignment/state/error/control cases.
- Cases include every supported operator and assignment, overflow, shifts,
  variable type/value changes, arrays, namerefs, readonly and integer
  attributes, local scope, temporary environments, subshells, command
  substitutions, RANDOM sequencing, nounset, errexit, DEBUG/RETURN traps,
  POSIX/compatibility mode, invalid syntax after warmup, disabling and clear.
- Standalone native engine tests passed under ASan/UBSan: randomized operators
  and assignments, 10,000 parser-bound/fuzz inputs, and 300 cached expression
  evictions. Leak detection was disabled because process inspection is
  blocked in this environment; no LeakSanitizer result is claimed.
- A preload that denies executable `mprotect` produced the correct loop
  result through Bash: zero native executions and three unavailable entries.
- Added engine and shell guards compiled with `-Wall -Wextra -Werror`.
- The original batch arithmetic/SIMD differential suite still passed.
- Full upstream suites completed on the integrated portable build and the
  reference. Arithmetic output matched. Additional differences were the new
  builtin/help layout, shopt/completion/BASHOPTS listings and timing-dependent
  broken-pipe diagnostics, alongside pre-existing environment/locale/terminal
  differences. Expected listings were updated only for the new public API.
  Upstream's harness may exit zero despite printed diffs; logs were inspected.
- PGO/LTO combinations, non-x86 architectures and GPU execution are not
  validated by this change.

A 300,000-iteration existing Bash loop, `((sum+=i*3+7))`, was timed with
arithmetic JIT off and on in the same portable executable. Seven runs per
mode alternated execution order; output was checked against `135001650000`.

| Mode | Median wall time |
|---|---:|
| Interpreter | 0.479049 s |
| Integrated native arithmetic | 0.278768 s |

The observed ratio was 1.718x for this workload. It does not establish a
speedup for I/O-heavy scripts, external commands or arbitrary shell programs.

Reproduce from the repository root:

```bash
lpm/build.sh portable ../build-portable
python3 lpm/test-integrated.py ../build-portable/bash /path/to/reference/bash
python3 lpm/test.py ../build-portable/bash ../build-portable/lpm-accel.so
cc -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer -I. lpm/test-arith-engine.c lpm-arith.c -o ../test-arith
../test-arith
make -C ../build-portable tests
python3 lpm/benchmark-integrated.py ../build-portable/bash
```

## Original batch-only milestone


Environment: Linux x86-64, GCC 13.3.0. Upstream source commit is recorded in
README.md. All results below are local runs, not GitHub Actions results.

- Unmodified reference Bash: built with upstream defaults, except
  `--without-bash-malloc`, to match the fork's allocator configuration.
- Portable Bash: built with `-O3 -g -march=x86-64 -mtune=generic`.
- Zen 2 Bash: built with `-O3 -g -march=znver2 -mtune=znver2`.
- Arithmetic differential tests: 250 generated expressions, 14 inputs each,
  native vs bytecode vs Bash; passed on portable and Zen 2 builds.
- AVX2 batch lengths, full-width multiplication, scalar tails and rejected
  expression/argument tests: passed on both builds.
- Standalone engine test: native code allocation asserted; 100,000 JIT calls
  and 100 randomized SIMD batches matched unsigned scalar results.
- Engine test repeated under ASan and UBSan: passed with
  `ASAN_OPTIONS=detect_leaks=0`. LeakSanitizer itself cannot run in this
  environment because its process inspection is blocked. No leak check is
  claimed.
- Added extension built with `-Wall -Wextra -Werror`: passed.
- Upstream `make tests` on reference and portable builds completed. Both had
  environment-dependent expected-output differences. Their normalized logs
  differed only in timing-dependent broken-pipe messages in `run-histexp`.
  See `validation/`; a zero test harness exit code alone was not used as
  evidence of a clean suite.
- OpenCL runtime discovery/failure behavior: passed. No GPU was available;
  kernel compilation, GPU arithmetic, driver behavior and performance remain
  untested. The CPU test results do not validate the GPU path.
- PGO, LTO, non-x86 platforms and Zen 3/4 builds: not validated.

One portable-build numeric microbenchmark, median of nine runs with 10,000
values, including shell startup, argument processing and discarded text output:

| Workload | Median time |
|---|---:|
| Bash loop, `x*x+3*x+7` | 38.686 ms |
| Native JIT batch, same expression | 11.636 ms |
| Bytecode batch, same expression | 10.785 ms |
| SIMD affine batch, different expression `3*x+7` | 10.070 ms |

This demonstrates a benefit from the explicit batch API in this workload.
The JIT did not beat bytecode here. The affine row is not an equivalent
expression comparison. These results do not establish faster execution of
existing Bash scripts, gains on Ryzen hardware, or gains from GPU offload.
