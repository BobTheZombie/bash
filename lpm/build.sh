#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
src=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
profile=${1:-portable}
build=${2:-"$src/../build-$profile"}
case $profile in
  portable) cpu='-march=x86-64 -mtune=generic' ;;
  zen2) cpu='-march=znver2 -mtune=znver2' ;;
  zen3) cpu='-march=znver3 -mtune=znver3' ;;
  zen4) cpu='-march=znver4 -mtune=znver4' ;;
  native) cpu='-march=native -mtune=native' ;;
  generic) cpu='' ;;
  *) echo 'Profiles: portable zen2 zen3 zen4 native generic' >&2; exit 2 ;;
esac
mkdir -p -- "$build"
build=$(cd -- "$build" && pwd)
if [[ $build == "$src" ]]; then echo 'Use an out-of-tree build directory' >&2; exit 2; fi
export CFLAGS="-O3 -g $cpu ${CFLAGS:-}"
export LDFLAGS="${LDFLAGS:-}"
if [[ ${LPM_LTO:-0} == 1 ]]; then CFLAGS+=' -flto'; LDFLAGS+=' -flto'; fi
cd -- "$build"
# Optional two-pass, workload-trained GCC PGO. Use the same build directory.
case ${LPM_PGO:-off} in
  off) ;;
  generate) CFLAGS+=" -fprofile-generate=$build/pgo"; LDFLAGS+=" -fprofile-generate=$build/pgo" ;;
  use) CFLAGS+=" -fprofile-use=$build/pgo -fprofile-correction"; LDFLAGS+=" -fprofile-use=$build/pgo" ;;
  *) echo 'LPM_PGO must be off, generate or use' >&2; exit 2 ;;
esac
"$src/configure" --prefix="${PREFIX:-/usr}" --without-bash-malloc
make clean
make -j"${JOBS:-4}"
# Build the shared extension against exactly the configured shell's headers.
"${CC:-cc}" $CFLAGS -Wall -Wextra -Wno-unused-parameter -fPIC -shared \
  -DHAVE_CONFIG_H -I. -I"$src" -I"$src/include" -I"$src/builtins" \
  -I"$src/examples/loadables" "$src/lpm/builtins.c" "$src/lpm/jit.c" \
  "$src/lpm/vector.c" "$src/lpm/gpu.c" $LDFLAGS -ldl -o lpm-accel.so
printf 'Built %s/bash and %s/lpm-accel.so\n' "$build" "$build"
