/* SPDX-License-Identifier: GPL-3.0-or-later
 * A bounded arithmetic expression JIT. It does not compile shell syntax.
 * All arithmetic is explicitly modulo 2^64; input is never passed to a compiler.
 */
#define _GNU_SOURCE
#include "accel.h"
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
struct parser { const char *s; struct lpm_program *p; unsigned depth; };
static void space(struct parser *s) { while (*s->s==' ' || *s->s=='\t' || *s->s=='\n') s->s++; }
static int emit(struct parser *s, int op, uint64_t v) {
  if(s->p->count==LPM_MAX_OPS) return -1;
  s->p->ops[s->p->count++]=(struct lpm_op){op,v}; return 0;
}
static int expr(struct parser *);
static int atom(struct parser *s) {
  int rc=-1; uint64_t n=0; const char *start;
  if (++s->depth>64) { s->depth--; return -1; }
  space(s);
  if (*s->s=='+' || *s->s=='-') {
    int neg=*s->s++=='-';
    rc=atom(s); if(!rc && neg) rc=emit(s,'~',0);
  } else if(*s->s=='(') {
    s->s++; rc=expr(s); space(s);
    if(*s->s!=')') rc=-1; else s->s++;
  } else if(*s->s=='x') { s->s++; rc=emit(s,'x',0); }
  else {
    start=s->s;
    while(*s->s>='0' && *s->s<='9') {
      unsigned d=*s->s++-'0';
      if(n>(UINT64_MAX-d)/10) goto done;
      n=n*10+d;
    }
    if(s->s!=start) rc=emit(s,'n',n);
  }
done: s->depth--; return rc;
}
static int product(struct parser *s) {
  if(atom(s)) return -1;
  space(s);
  while(*s->s=='*') { s->s++; if(atom(s)||emit(s,'*',0)) return -1; space(s); }
  return 0;
}
static int expr(struct parser *s) {
  if(product(s)) return -1;
  space(s);
  while(*s->s=='+' || *s->s=='-') {
    int op=*s->s++; if(product(s)||emit(s,op,0)) return -1; space(s);
  }
  return 0;
}
#if defined(__x86_64__) && !defined(_WIN32)
static int native(struct lpm_program *p) {
  /* Every bytecode emits at most 11 bytes. Use no callee-saved registers;
   * push/pop temporaries are balanced before returning. ENDBR64 supports IBT.
   */
  size_t i,n=0,cap=p->count*16+16; unsigned char *b;
  b=mmap(0,cap,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
  if(b==MAP_FAILED) return -1;
#define B(v) b[n++]=(v)
  B(0xf3); B(0x0f); B(0x1e); B(0xfa);
  for(i=0;i<p->count;i++) {
    switch(p->ops[i].op) {
      case 'n': B(0x48); B(0xb8); memcpy(b+n,&p->ops[i].value,8); n+=8; B(0x50); break;
      case 'x': B(0x57); break; /* push rdi */
      case '~': B(0x58); B(0x48); B(0xf7); B(0xd8); B(0x50); break;
      default:
        B(0x59); B(0x58); /* pop rcx; pop rax */
        if(p->ops[i].op=='*') { B(0x48); B(0x0f); B(0xaf); B(0xc1); }
        else { B(0x48); B(p->ops[i].op=='+'?0x01:0x29); B(0xc8); }
        B(0x50);
    }
  }
  B(0x58); B(0xc3);
#undef B
  if(mprotect(b,cap,PROT_READ|PROT_EXEC)) { munmap(b,cap); return -1; }
  __builtin___clear_cache((char *)b,(char *)b+n);
  p->code=b; p->code_size=cap; return 0;
}
#endif
int lpm_compile(struct lpm_program *p, const char *text, int use_native) {
  struct parser s={text,p,0}; memset(p,0,sizeof(*p));
  if(strlen(text)>4096 || expr(&s)) { p->count=0; return -1; }
  space(&s); if(*s.s) { p->count=0; return -1; }
#if defined(__x86_64__) && !defined(_WIN32)
  if(use_native) (void)native(p); /* executable-memory policy may require fallback */
#else
  (void)use_native;
#endif
  return 0;
}
uint64_t lpm_eval(const struct lpm_program *p, uint64_t x) {
  uint64_t st[LPM_MAX_OPS]; size_t n=0,i;
  if(p->code) return ((uint64_t (*)(uint64_t))p->code)(x);
  for(i=0;i<p->count;i++) switch(p->ops[i].op) {
    case 'n': st[n++]=p->ops[i].value; break;
    case 'x': st[n++]=x; break;
    case '~': st[n-1]=0-st[n-1]; break;
    case '+': st[n-2]+=st[n-1]; n--; break;
    case '-': st[n-2]-=st[n-1]; n--; break;
    case '*': st[n-2]*=st[n-1]; n--; break;
  }
  return st[0];
}
void lpm_destroy(struct lpm_program *p) {
  if(p->code) munmap(p->code,p->code_size);
  memset(p,0,sizeof(*p));
}
