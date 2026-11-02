# First milestone validation

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
