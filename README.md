# LPM Bash

GNU Bash 5.3 with a guarded CPU arithmetic JIT, CPU-specific build profiles,
and optional SIMD/OpenCL numeric extensions. Upstream's README, COPYING and
notices remain in the source tree.

```bash
lpm/build.sh zen2 ../build-zen2
../build-zen2/bash your-script.sh
```

The native arithmetic path is enabled by default on eligible executions. It
compiles frequently used expressions from existing `$((...))`, `((...))`,
`let` and arithmetic `for` loops. No script annotations or loadable builtin
are required for this path. Variable state is checked on every execution;
unsupported expressions and states retain Bash's interpreter.

```bash
shopt -u arithmetic_jit  # disable native arithmetic
shopt -s arithmetic_jit  # enable native arithmetic
lpmjitctl stats         # inspect actual compilation/execution counters
lpmjitctl clear         # release cached code and reset counters
```

See [implementation and build notes](lpm/README.md) and
[validation results](lpm/VALIDATION.md).

This is an arithmetic JIT, not a whole-program shell compiler. Command
execution, pipelines, redirections and control flow continue through Bash.
GPU execution remains an explicit, experimental numeric extension.
