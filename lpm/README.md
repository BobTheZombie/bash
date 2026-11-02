# LPM Bash acceleration: experimental first milestone

This source fork retains GNU Bash's parser, executor, builtins and upstream
license. Ordinary scripts execute through the upstream interpreter. Extensions
are explicitly loaded; this is **not a transparent whole-script JIT** and does
not accelerate external programs launched by a script.

Upstream snapshot: `tianon/mirror-bash`, commit
`9c465866b7d849378369ef700cbe2965aa9691e3` (Bash 5.3 patch 20).
Canonical upstream: https://git.savannah.gnu.org/git/bash.git

## Build

```bash
lpm/build.sh portable ../build-portable
lpm/build.sh zen2 ../build-zen2
```

Profiles: `portable` (x86-64 baseline), `zen2`, `zen3`, `zen4`, `native` and
`generic` (no architecture flags; for other architectures). Zen 2 is appropriate
for the LPM Ryzen 4500U target. Architecture-specific executables must only run
on CPUs supporting the instructions selected by the compiler. AVX2 in the
portable numeric extension uses runtime CPU feature detection and a scalar
fallback. Native JIT emission is currently Linux/System V x86-64 only; other
platforms use the bytecode interpreter. This milestone has been tested on Linux
x86-64 only.

`CC`, `CFLAGS`, `LDFLAGS`, `PREFIX`, `JOBS` and `LPM_LTO=1` customize builds.
No fast-math flags or global ISA changes are applied by default. `-O3` and CPU
profiles are tuning choices, not proof of a speedup. The build script cleans
objects before rebuilding so changes of optimization flags take effect.
Bash's supported upstream configure/Make build remains in use.

Optional GCC profile-guided build (same absolute directory in both passes):

```bash
LPM_PGO=generate lpm/build.sh zen2 ../build-zen2
make -C ../build-zen2 tests
# Also run representative, trusted LPM build scripts with that generated Bash.
LPM_PGO=use lpm/build.sh zen2 ../build-zen2
```

Do not train a PGO build on untrusted scripts. PGO/LTO build combinations are
available for evaluation but are not yet validated in this milestone. Missing
profile warnings mean some paths were not trained; evaluate the resulting
binary against your actual workload.

## Numeric JIT

```bash
bash=../build-portable/bash
"$bash" --noprofile --norc
# Commands below run inside the newly built shell:
enable -f ../build-portable/lpm-accel.so lpmjit
lpmjit 'x*x+3*x+7' 0 1 2 3  # 7, 11, 17, 25 on separate lines
lpmjit --interpret 'x*x+3*x+7' 0 1 2 3
```

Expressions accept decimal constants, `x`, parentheses, unary `+`/`-` and
binary `+`, `-`, `*`. Arguments are strict decimal integers (optional sign).
Constants have to fit unsigned 64 bits; leading zeros remain decimal. Results
wrap modulo 2^64 and print as signed two's complement, matching ordinary Bash
arithmetic for the supported decimal operations. This extension deliberately
does not accept Bash's octal/hex syntax, variable side effects, division,
substitutions or general shell syntax. Invalid inputs fail without numeric
output. Numeric extensions therefore have their own explicit input contract;
they do not replace `$((...))`.

The expression is parsed once into bounded bytecode per builtin invocation.
On x86-64, a small emitter creates native code in writable memory and changes
it to executable/read-only before use (never writable and executable at once).
Generated code uses only caller-saved registers and balanced temporary stack
slots, with ENDBR64 at entry for IBT. If executable allocation is denied,
execution falls back to bytecode. Memory is released before output. There is
no compiler subprocess, persistent code cache or code loaded from disk.

Limits: 4096 expression bytes, 1024 bytecode operations, 64 recursive parsing
levels. Inputs must fit the argument/memory limits of the shell process. A
large batch amortizes compile cost. Short batches may be faster interpreted.

## SIMD and GPU batch arithmetic

```bash
enable -f ../build-portable/lpm-accel.so lpmvec
lpmvec 3 7 0 1 2 3          # input*3+7, on the CPU
lpmvec --gpu 3 7 0 1 2 3    # explicitly require an OpenCL GPU
```

CPU evaluation uses a runtime-dispatched AVX2 implementation of exact 64-bit
multiplication/addition, including high/low limb products and a scalar tail.
It works without alignment constraints or out-of-bounds vector loads.

The GPU backend dynamically loads `libOpenCL.so.1`, selects an OpenCL GPU,
compiles a fixed OpenCL 1.2 unsigned integer kernel, uploads the batch and
returns results only after successful readback. There is no user-supplied GPU
source. Failure returns nonzero without numeric output or silent CPU fallback.
No OpenCL SDK is needed at build time; execution needs an installed GPU driver
and an OpenCL runtime supporting its GPU. AMD vendor support must be verified
on the target Renoir machine. This is an **experimental, unverified device
path**, not a measured GPU optimization. There is no device/kernel cache or
automatic threshold: setup, transfer and text output can cost more than the
arithmetic. CPU is the default.

## Validation

```bash
python3 lpm/test.py ../build-portable/bash ../build-portable/lpm-accel.so
cc -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Ilpm \
  lpm/test-engine.c lpm/jit.c lpm/vector.c -o ../test-engine
../test-engine
make -C ../build-portable tests
python3 lpm/benchmark.py ../build-portable
```

The differential suite compares generated native code, interpreted bytecode,
Bash arithmetic and Python integer results, checks rejected inputs, and tests
SIMD lengths/tails and overflow. The engine test asserts that native code was
actually generated on x86-64. When a GPU is unavailable, only its failure path
is tested; no device correctness or performance conclusion follows.
GitHub Actions repeats portable/native builds, sanitizers and upstream tests.
Upstream's test harness prints diffs and can still return zero: inspect its
log. Locale, terminal, root-user and environment differences require comparison
with an unmodified build from the same upstream commit.

## Remaining work

A production Bash script JIT must integrate with parsed commands and support
guarded deoptimization for dynamic variables, arrays, traps, DEBUG/RETURN,
`set -e`/`-u`, scope, redirections, command substitutions and job control.
This milestone provides no automatic hot-loop detection or native compilation
of existing scripts. Those features need dedicated design and differential
coverage before interpreter replacement. Next steps are a persistent bounded
expression cache, target-machine PGO/benchmarks, real-device OpenCL validation,
and then guarded compilation of carefully selected arithmetic ASTs.

All added code is GPL-3.0-or-later; see upstream COPYING. The original Bash
README and copyright notices remain intact. Do not replace the system's
`/bin/bash` with this experimental build until target-system validation.
