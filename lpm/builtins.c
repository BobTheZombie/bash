/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <config.h>
#include "loadables.h"
#include "accel.h"
#include <inttypes.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
/* Strict decimal arguments; leading zeros never imply octal. */
static int number(const char *s,uint64_t *v) {
  int neg=0; uint64_t n=0;
  if(*s=='-' || *s=='+') neg=*s++=='-';
  if(!*s) return -1;
  for(;*s;s++) { unsigned d=(unsigned char)*s-'0';
    if(d>9 || n>(UINT64_MAX-d)/10) return -1;
    n=n*10+d;
  }
  *v=neg?0-n:n; return 0;
}
static int inputs(WORD_LIST *list,uint64_t **out,size_t *count) {
  WORD_LIST *l; size_t n=0,i=0; uint64_t *a;
  for(l=list;l;l=l->next) { if(n==SIZE_MAX/sizeof(*a)) return -1; n++; }
  if(!n) return -1;
  a=malloc(n*sizeof(*a)); if(!a) return -1;
  for(l=list;l;l=l->next) if(number(l->word->word,&a[i++])) { free(a); return -1; }
  *out=a; *count=n; return 0;
}
static int output(uint64_t *a,size_t n) {
  size_t i;
  for(i=0;i<n;i++) {
    /* Format signed two's-complement values without implementation-defined casts. */
    if(a[i]>>63) printf("-%" PRIu64 "\n",0-a[i]); else printf("%" PRIu64 "\n",a[i]);
  }
  return sh_chkwrite(EXECUTION_SUCCESS);
}
static int jit_builtin(WORD_LIST *list) {
  struct lpm_program p; uint64_t *a; size_t n,i; int native=1,rc;
  if(list && !strcmp(list->word->word,"--interpret")) { native=0; list=list->next; }
  if(!list || !list->next) { builtin_usage(); return EX_USAGE; }
  if(lpm_compile(&p,list->word->word,native)) {
    builtin_error("unsupported expression (use decimal, x, +, -, *, parentheses)"); return EXECUTION_FAILURE;
  }
  if(inputs(list->next,&a,&n)) { lpm_destroy(&p); builtin_error("expected decimal integer inputs"); return EXECUTION_FAILURE; }
  for(i=0;i<n;i++) a[i]=lpm_eval(&p,a[i]);
  lpm_destroy(&p); rc=output(a,n); free(a); return rc;
}
static int vec_builtin(WORD_LIST *list) {
  uint64_t m,b,*a; size_t n; int gpu=0,rc;
  if(list && !strcmp(list->word->word,"--gpu")) { gpu=1; list=list->next; }
  if(!list || !list->next || !list->next->next) { builtin_usage(); return EX_USAGE; }
  if(number(list->word->word,&m) || number(list->next->word->word,&b) || inputs(list->next->next,&a,&n)) {
    builtin_error("expected decimal multiplier, addend and inputs"); return EXECUTION_FAILURE;
  }
  if(gpu) {
    if(lpm_gpu_affine(a,n,m,b)) { free(a); builtin_error("OpenCL GPU execution unavailable or failed; no results emitted"); return EXECUTION_FAILURE; }
  } else lpm_affine(a,n,m,b);
  rc=output(a,n); free(a); return rc;
}
static char *jit_doc[]={"Evaluate a bounded integer expression over decimal inputs.","Native x86-64 JIT with interpreter fallback; arithmetic wraps modulo 2^64.","This is an explicit numeric extension, not a whole-script Bash compiler.",0};
static char *vec_doc[]={"Compute input * MULTIPLIER + ADDEND modulo 2^64.","Runtime AVX2 dispatch, or explicit OpenCL GPU execution with --gpu.",0};
struct builtin lpmjit_struct={"lpmjit",jit_builtin,BUILTIN_ENABLED,jit_doc,"lpmjit [--interpret] EXPR X...",0};
struct builtin lpmvec_struct={"lpmvec",vec_builtin,BUILTIN_ENABLED,vec_doc,"lpmvec [--gpu] MULTIPLIER ADDEND X...",0};
