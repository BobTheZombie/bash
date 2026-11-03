# CPU arithmetic JIT and numeric extensions

LPM Bash retains GNU Bash's command parser and executor. Its core arithmetic
evaluator now uses guarded native code for frequently used expressions in
existing scripts. No compiler process, LLVM dependency, persistent code file,
or script rewriting is involved.

Upstream base: Bash 5.3 patch 20, commit
`9c465866b7d849378369ef700cbe2965aa9691e3`, mirrored from
https://git.savannah.gnu.org/git/bash.git through `tianon/mirror-bash`.

## Build and run

```bash
lpm/build.sh portable ../build-portable
lpm/build.sh zen2 ../build-zen2
../build-zen2/bash your-script.sh
```

Profiles: `portable` (x86-64 baseline), `zen2`, `zen3`, `zen4`, `native`,
`generic` (no architecture flags). Zen 2 targets LPM's Ryzen 4500U baseline.
Run architecture-specific binaries only on compatible CPUs. Native code
emission currently supports Linux/System V x86-64 with 64-bit intmax_t.
Other architectures continue through the interpreter. The generated scalar
code requires only baseline x86-64 instructions, including an ENDBR64 entry;
there is no AVX requirement in the transparent arithmetic path.

The build uses `-O3` and supports `CC`, `CFLAGS`, `LDFLAGS`, `PREFIX`, `JOBS`,
`LPM_LTO=1`, and the earlier optional GCC PGO modes. No fast-math flags are
used. Build flags are tuning choices, not promises of a speedup. PGO and LTO
combinations have not been validated for this change.

Optional two-pass GCC PGO, retaining the same absolute build directory:

```bash
LPM_PGO=generate lpm/build.sh zen2 ../build-zen2
# Run representative, trusted workloads using ../build-zen2/bash.
LPM_PGO=use lpm/build.sh zen2 ../build-zen2
```

## Transparent arithmetic acceleration

```bash
# No enable -f or script changes are required:
sum=0
for ((i=0; i<100000; i++)); do
    ((sum += i*3+7))
done
lpmjitctl stats
```

The `evalexp` integration serves `$((...))`, arithmetic commands, `let`, and
arithmetic loop expressions. A 64-entry, direct-mapped cache observes exact
expression text. After eight lookups, an eligible expression is compiled once
and reused until eviction or `lpmjitctl clear`. Unsupported syntax and failed
executable allocation are negatively cached. Every execution reloads and
checks variable state; compiled code holds no shell variable pointers.

Supported syntax:

- Canonical decimal constants and ASCII scalar variable names.
- Parentheses, unary `+`, `-`, `!`, `~`.
- Binary `+`, `-`, `*`, `&`, `^`, `|`, `<<`, `>>`.
- Signed comparisons `<`, `>`, `<=`, `>=`, `==`, `!=`.
- A single outer scalar assignment: `=`, `+=`, `-=`, `*=`, `&=`, `^=`, `|=`,
  `<<=`, `>>=`; or outer `x++`, `x--`, `++x`, `--x`.

The RHS cannot contain assignments or other side effects. Integer operations
wrap modulo 2^64; comparisons use signed values, right shifts are arithmetic,
and x86-64 shift counts use the low six bits, as in the reference build.

Guards accept only existing scalar variables whose current values are
canonical signed/unsigned decimal integers. Namerefs, arrays, recursive
arithmetic strings, unset/invisible variables, dynamic callbacks and custom
assignment callbacks fall back. Readonly or attributed write targets fall
back. A rejected guard never invokes a dynamic-value callback: `RANDOM`, for
example, is read only by the original evaluator. All native-path assignment
results are committed through Bash's existing `expr_bind_variable` function.

Division, remainder, exponentiation, short-circuit operators, ternaries,
commas, nested side effects, array expressions, alternate numeric bases and
unsupported syntax continue through Bash. Native execution is also bypassed
for nested recursive arithmetic, privileged/restricted shells, POSIX mode,
and compatibility levels below 5.3. These are runtime fallback conditions;
they do not change the expression's interpreted behavior.

Limits per cached expression: 511 bytes, 128 bytecode operations, 16 variable
names (63 bytes each), 32 recursive parsing levels. Cache misses/collisions
may reduce performance without changing results.

## Controls and memory

```bash
shopt -u arithmetic_jit
shopt -s arithmetic_jit
lpmjitctl stats
lpmjitctl clear
```

The shopt flag defaults to on. `stats` reports compilation, actual native
executions, guarded fallbacks, syntax rejection and unavailable executable
memory. Clearing releases cached code and resets counters. Disabling bypasses
the cache but retains entries until clear, eviction or process exit.

Code is emitted into writable, non-executable anonymous memory, then changed
to read-only/executable before use. It is never writable and executable at
once. Allocation/protection failure returns to Bash; no permission relaxation
is attempted. Generated code uses caller-saved registers and balanced stack
temporaries. Child processes inherit independent mapping/state snapshots;
exit and eviction release mappings. Trap dispatch, exit status, scope and
shell control flow remain managed by Bash.

## Earlier explicit extensions

The shared extension remains available for numeric batches:

```bash
enable -f ../build-portable/lpm-accel.so lpmjit
enable -f ../build-portable/lpm-accel.so lpmvec
lpmjit 'x*x+3*x+7' 0 1 2 3
lpmjit --interpret 'x*x+3*x+7' 0 1 2 3
lpmvec 3 7 0 1 2 3
lpmvec --gpu 3 7 0 1 2 3
```

These commands have their own explicit decimal-input contracts. The older
batch `lpmjit` supports `x`, decimal constants, parentheses, unary `+`/`-`,
and binary `+`/`-`/`*`; its code is released after each invocation. Its
interpreter fallback differs from the core cache's upstream Bash fallback.
`lpmvec` computes `input*multiplier+addend` modulo 2^64, using runtime AVX2
CPU dispatch and a scalar tail by default. Results print as signed values.

`--gpu` explicitly requires an OpenCL GPU and dynamically loads the runtime.
GPU setup, compilation, transfers and text output can exceed CPU compute
cost. There is no automatic GPU offload or CPU substitution on GPU failure.
Device execution remains unverified in this environment, including target
Renoir driver compatibility. This change does not modify that backend.

## Scope and validation

See VALIDATION.md and the test commands recorded there. This implementation
accelerates guarded arithmetic fragments in scripts. Whole shell functions,
loop bodies, pipelines and arbitrary commands are not compiled into native
control flow. It does not accelerate external programs launched by Bash.

All added code is GPL-3.0-or-later. Original Bash license/copyright notices
remain intact. Validate the target installation before replacing a system
shell.
