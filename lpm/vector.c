/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "accel.h"
#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
#include <immintrin.h>
__attribute__((target("avx2")))
static void avx2(uint64_t *a,size_t n,uint64_t m,uint64_t b) {
  size_t i=0; __m256i vm=_mm256_set1_epi64x(m),vb=_mm256_set1_epi64x(b);
  for(;n-i>=4;i+=4) {
    __m256i x=_mm256_loadu_si256((const __m256i *)(a+i));
    __m256i low=_mm256_mul_epu32(x,vm);
    __m256i cross=_mm256_add_epi64(_mm256_mul_epu32(_mm256_srli_epi64(x,32),vm),
                                  _mm256_mul_epu32(x,_mm256_srli_epi64(vm,32)));
    __m256i y=_mm256_add_epi64(vb,_mm256_add_epi64(low,_mm256_slli_epi64(cross,32)));
    _mm256_storeu_si256((__m256i *)(a+i),y);
  }
  for(;i<n;i++) a[i]=a[i]*m+b;
}
#endif
void lpm_affine(uint64_t *a,size_t n,uint64_t m,uint64_t b) {
  size_t i;
#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
  if(__builtin_cpu_supports("avx2")) { avx2(a,n,m,b); return; }
#endif
  for(i=0;i<n;i++) a[i]=a[i]*m+b;
}
