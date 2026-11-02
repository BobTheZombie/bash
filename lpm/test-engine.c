/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "accel.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t state=47021;
static uint64_t rng(void) { state^=state<<13; state^=state>>7; state^=state<<17; return state; }
int main(void) {
  struct lpm_program p; uint64_t a[1027],copy[1027];
  assert(!lpm_compile(&p,"-(x*18446744073709551615)+7",1));
#if defined(__x86_64__)
  assert(p.code); /* Ensure differential tests don't only cover interpreter. */
#endif
  for(int i=0;i<100000;i++) { uint64_t x=rng(); assert(lpm_eval(&p,x)==x+7); }
  lpm_destroy(&p);
  for(int j=0;j<100;j++) {
    uint64_t m=rng(),b=rng(); size_t n=rng()%1027;
    for(size_t i=0;i<1027;i++) a[i]=copy[i]=rng();
    lpm_affine(a,n,m,b);
    for(size_t i=0;i<1027;i++) assert(a[i]==(i<n?copy[i]*m+b:copy[i]));
  }
  lpm_affine(0,0,1,1);
  assert(lpm_compile(&p,"x/2",1));
  puts("Native JIT execution and 100 randomized SIMD batches passed");
}
