/* SPDX-License-Identifier: GPL-3.0-or-later
 * Guarded shell arithmetic JIT engine. No shell state or callbacks in code.
 * Unsupported syntax is rejected before it can produce any side effects.
 */
#define _GNU_SOURCE
#include "lpm-arith.h"
#include <string.h>
#include <sys/mman.h>

/* Tokens above ASCII are intentionally internal, never machine opcodes. */
enum { NUM=128, VAR, SHL, SHR, LE, GE, EQ, NE, INC, DEC, ADDSET,
       SUBSET, MULSET, ANDSET, XORSET, ORSET, SHLSET, SHRSET,
       NEG, INV, NOT, STORE, DROP, BAD };
struct parser {
  const char *text;
  int token, index;
  uint64_t number;
  unsigned depth;
  struct lj_program *p;
};
static int letter(int c) {
  return (c>='a' && c<='z') || (c>='A' && c<='Z') || c=='_';
}
static int digit(int c) { return c>='0' && c<='9'; }
static void lex(struct parser *s) {
  const char *a; size_t n,i; uint64_t value=0;
  while (*s->text==' ' || *s->text=='\t' || *s->text=='\n') s->text++;
  a=s->text; s->token=(unsigned char)*a;
  if (!*a) return;
  if (digit(*a)) {
    while(digit(*s->text)) {
      unsigned d=*s->text++-'0';
      if(value>(UINT64_MAX-d)/10) { s->token=BAD; return; }
      value=value*10+d;
    }
    /* Bash interprets leading zeros as octal, not decimal. */
    s->token=(*a=='0' && s->text-a>1)?BAD:NUM;
    s->number=value; return;
  }
  if(letter(*a)) {
    while(letter(*s->text) || digit(*s->text)) s->text++;
    n=s->text-a;
    if(n>=LJ_NAME) { s->token=BAD; return; }
    for(i=0;i<s->p->nvars;i++)
      if(strlen(s->p->vars[i])==n && !memcmp(s->p->vars[i],a,n)) break;
    if(i==s->p->nvars) {
      if(i==LJ_VARS) { s->token=BAD; return; }
      memcpy(s->p->vars[i],a,n); s->p->vars[i][n]=0; s->p->nvars++;
    }
    s->token=VAR; s->index=(int)i; return;
  }
#define TOKEN(str,kind) if(!strncmp(a,str,sizeof(str)-1)) { s->text+=sizeof(str)-1; s->token=kind; return; }
  TOKEN("<<=",SHLSET); TOKEN(">>=",SHRSET);
  TOKEN("++",INC); TOKEN("--",DEC);
  TOKEN("+=",ADDSET); TOKEN("-=",SUBSET); TOKEN("*=",MULSET);
  TOKEN("&=",ANDSET); TOKEN("^=",XORSET); TOKEN("|=",ORSET);
  TOKEN("<<",SHL); TOKEN(">>",SHR); TOKEN("<=",LE); TOKEN(">=",GE);
  TOKEN("==",EQ); TOKEN("!=",NE);
  TOKEN("&&",BAD); TOKEN("||",BAD); TOKEN("**",BAD);
#undef TOKEN
  s->text++;
}
static int emit(struct parser *s,int op,uint64_t v) {
  if(s->p->count==LJ_OPS) return -1;
  s->p->ops[s->p->count++]=(struct lj_op){op,v}; return 0;
}
static int precedence(int token) {
  switch(token) {
    case '|': return 1; case '^': return 2; case '&': return 3;
    case EQ: case NE: return 4;
    case '<': case '>': case LE: case GE: return 5;
    case SHL: case SHR: return 6;
    case '+': case '-': return 7;
    case '*': return 8;
    default: return 0;
  }
}
static int expr(struct parser *,int);
static int atom(struct parser *s) {
  int token=s->token,rc=-1;
  if(++s->depth>32) { s->depth--; return -1; }
  if(token==NUM || token==VAR) {
    rc=emit(s,token,token==NUM?s->number:(uint64_t)s->index); lex(s);
  } else if(token=='+' || token=='-' || token=='~' || token=='!') {
    lex(s); rc=atom(s);
    if(!rc && token!='+') rc=emit(s,token=='-'?NEG:token=='~'?INV:NOT,0);
  } else if(token=='(') {
    lex(s); rc=expr(s,1);
    if(s->token!=')') rc=-1; else lex(s);
  }
  s->depth--; return rc;
}
static int expr(struct parser *s,int min) {
  int prec,op;
  if(atom(s)) return -1;
  while((prec=precedence(s->token))>=min) {
    op=s->token; lex(s);
    if(expr(s,prec+1) || emit(s,op,0)) return -1;
  }
  return 0;
}
static int compound(int token) {
  switch(token) {
    case ADDSET:return '+'; case SUBSET:return '-'; case MULSET:return '*';
    case ANDSET:return '&'; case XORSET:return '^'; case ORSET:return '|';
    case SHLSET:return SHL; case SHRSET:return SHR;
    default:return 0;
  }
}
int lj_parse(struct lj_program *p,const char *text) {
  struct parser s={text,0,0,0,0,p},start; int id,op,token;
  memset(p,0,sizeof(*p)); p->target=-1;
  if(!text || strnlen(text,LJ_TEXT)==LJ_TEXT) return -1;
  lex(&s); start=s;
  if(s.token==INC || s.token==DEC) {
    op=s.token==INC?'+':'-'; lex(&s);
    if(s.token!=VAR) return -1;
    id=s.index; lex(&s);
    if(s.token) return -1;
    p->target=id;
    return emit(&s,VAR,id) || emit(&s,NUM,1) || emit(&s,op,0) || emit(&s,STORE,id)?-1:0;
  }
  if(s.token==VAR) {
    id=s.index; lex(&s); token=s.token;
    if(token==INC || token==DEC) {
      lex(&s); if(s.token) return -1;
      p->target=id;
      return emit(&s,VAR,id) || emit(&s,VAR,id) || emit(&s,NUM,1) ||
        emit(&s,token==INC?'+':'-',0) || emit(&s,STORE,id) || emit(&s,DROP,0)?-1:0;
    }
    if(token=='=' || compound(token)) {
      op=compound(token); p->target=id; lex(&s);
      if(op && emit(&s,VAR,id)) return -1;
      if(expr(&s,1) || s.token || (op && emit(&s,op,0)) || emit(&s,STORE,id)) return -1;
      return 0;
    }
  }
  s=start;
  return expr(&s,1) || s.token?-1:0;
}
int lj_native(struct lj_program *p) {
#if defined(__x86_64__) && !defined(_WIN32)
  size_t i,n=0,cap=p->count*32+16; unsigned char *b;
  if(!p->count || p->count>LJ_OPS) return -1;
  b=mmap(0,cap,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
  if(b==MAP_FAILED) return -1;
#define B(v) b[n++]=(v)
#define DISP(v) do { uint32_t disp=(uint32_t)(v)*8; memcpy(b+n,&disp,4); n+=4; } while(0)
  B(0xf3); B(0x0f); B(0x1e); B(0xfa); /* ENDBR64 */
  for(i=0;i<p->count;i++) {
    int op=p->ops[i].op,cc=0;
    switch(op) {
      case NUM: B(0x48); B(0xb8); memcpy(b+n,&p->ops[i].value,8); n+=8; B(0x50); break;
      case VAR: B(0x48); B(0x8b); B(0x87); DISP(p->ops[i].value); B(0x50); break;
      case STORE: B(0x48); B(0x8b); B(0x04); B(0x24); B(0x48); B(0x89); B(0x87); DISP(p->ops[i].value); break;
      case DROP: B(0x58); break;
      case NEG: case INV: B(0x58); B(0x48); B(0xf7); B(op==NEG?0xd8:0xd0); B(0x50); break;
      case NOT:
        B(0x58); B(0x48); B(0x85); B(0xc0); B(0x0f); B(0x94); B(0xc0);
        B(0x0f); B(0xb6); B(0xc0); B(0x50); break;
      default:
        B(0x59); B(0x58); /* right -> rcx, left -> rax */
        if(op=='*') { B(0x48); B(0x0f); B(0xaf); B(0xc1); }
        else if(op==SHL || op==SHR) { B(0x48); B(0xd3); B(op==SHL?0xe0:0xf8); }
        else if(op=='+' || op=='-' || op=='&' || op=='^' || op=='|') {
          B(0x48); B(op=='+'?0x01:op=='-'?0x29:op=='&'?0x21:op=='^'?0x31:0x09); B(0xc8);
        } else {
          switch(op) { case '<':cc=0x9c;break; case '>':cc=0x9f;break;
            case LE:cc=0x9e;break;case GE:cc=0x9d;break;case EQ:cc=0x94;break;case NE:cc=0x95;break; }
          if(!cc) { munmap(b,cap); return -1; }
          B(0x48); B(0x39); B(0xc8); B(0x0f); B(cc); B(0xc0); B(0x0f); B(0xb6); B(0xc0);
        }
        B(0x50);
    }
  }
  B(0x58); B(0xc3);
#undef DISP
#undef B
  if(mprotect(b,cap,PROT_READ|PROT_EXEC)) { munmap(b,cap); return -1; }
  __builtin___clear_cache((char *)b,(char *)b+n);
  p->code=b; p->size=cap; return 0;
#else
  (void)p; return -1;
#endif
}
void lj_release(struct lj_program *p) {
  if(p->code) munmap(p->code,p->size);
  memset(p,0,sizeof(*p));
}
#define CACHE_SIZE 64
#define HOT_COUNT 8
struct entry { char text[LJ_TEXT]; unsigned hot; int done; struct lj_program p; };
static struct entry cache[CACHE_SIZE];
static struct lj_stats stats;
const struct lj_program *lj_lookup(const char *text) {
  size_t n; uint64_t h=UINT64_C(14695981039346656037); struct entry *e;
  stats.lookups++;
  if(!text || !(n=strnlen(text,LJ_TEXT)) || n==LJ_TEXT) return 0;
  for(size_t i=0;i<n;i++) h=(h^(unsigned char)text[i])*UINT64_C(1099511628211);
  e=&cache[h%CACHE_SIZE];
  if(strcmp(e->text,text)) {
    lj_release(&e->p); memset(e,0,sizeof(*e)); memcpy(e->text,text,n+1);
  }
  if(e->done) { if(e->p.code) { stats.hits++; return &e->p; } return 0; }
  if(++e->hot<HOT_COUNT) return 0;
  e->done=1;
  if(lj_parse(&e->p,text)) { stats.rejected++; return 0; }
  if(lj_native(&e->p)) { stats.unavailable++; return 0; }
  stats.compiled++; return &e->p;
}
void lj_clear(void) {
  for(size_t i=0;i<CACHE_SIZE;i++) lj_release(&cache[i].p);
  memset(cache,0,sizeof(cache)); memset(&stats,0,sizeof(stats));
}
const struct lj_stats *lj_statistics(void) { return &stats; }
