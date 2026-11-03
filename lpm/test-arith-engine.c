/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "lpm-arith.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t state=47021;
static uint64_t rng(void) { state^=state<<13; state^=state>>7; state^=state<<17; return state; }
static void check(const char *text,uint64_t x,uint64_t y,uint64_t expected,
                  const char *target,uint64_t stored) {
  struct lj_program p; uint64_t values[LJ_VARS]={0};
  assert(!lj_parse(&p,text)); assert(!lj_native(&p)); assert(p.code);
  for(size_t i=0;i<p.nvars;i++) values[i]=!strcmp(p.vars[i],"x")?x:y;
  assert(((uint64_t (*)(uint64_t *))p.code)(values)==expected);
  if(target) { assert(p.target>=0); assert(!strcmp(p.vars[p.target],target)); assert(values[p.target]==stored); }
  else assert(p.target<0);
  lj_release(&p);
}
int main(void) {
#if defined(__x86_64__) && !defined(_WIN32)
  for(int i=0;i<250;i++) {
    uint64_t x=rng(),y=rng(),k=y&63;
    check("x+y*7",x,y,x+y*7,0,0); check("x-y",x,y,x-y,0,0);
    check("x*y",x,y,x*y,0,0); check("x&y",x,y,x&y,0,0);
    check("x|y",x,y,x|y,0,0); check("x^y",x,y,x^y,0,0);
    check("-x",x,y,0-x,0,0); check("~x",x,y,~x,0,0);
    check("!x",x,y,!x,0,0); check("!0",x,y,1,0,0);
    check("x<<y",x,y,x<<k,0,0);
    check("x>>y",x,y,k?(x>>k)|((x>>63)?UINT64_MAX<<(64-k):0):x,0,0);
    check("x<y",x,y,(x^(UINT64_C(1)<<63))<(y^(UINT64_C(1)<<63)),0,0);
    check("x>=y",x,y,(x^(UINT64_C(1)<<63))>=(y^(UINT64_C(1)<<63)),0,0);
    check("x==y",x,y,x==y,0,0); check("x!=y",x,y,x!=y,0,0);
    check("x=x+y",x,y,x+y,"x",x+y); check("x+=y",x,y,x+y,"x",x+y);
    check("x++",x,y,x,"x",x+1); check("--x",x,y,x-1,"x",x-1);
  }
  for(int j=0;j<10000;j++) {
    struct lj_program p; char text[600]; size_t n=rng()%sizeof(text);
    static const char alphabet[]="xy0123456789+-*<>=!~&|^() \r\n_[]$?:,/";
    for(size_t i=0;i<n;i++) text[i]=alphabet[rng()%(sizeof(alphabet)-1)];
    text[n]=0;
    if(!lj_parse(&p,text)) { uint64_t v[LJ_VARS]={0}; assert(!lj_native(&p)); (void)((uint64_t (*)(uint64_t *))p.code)(v); }
    lj_release(&p);
  }
  for(int j=0;j<300;j++) {
    char text[80]; snprintf(text,sizeof(text),"x+%d",j);
    for(int i=0;i<9;i++) (void)lj_lookup(text);
  }
  assert(lj_statistics()->compiled>=300); lj_clear(); assert(!lj_statistics()->compiled);
  puts("Native arithmetic operators, assignments, parser bounds and cache eviction passed");
#else
  puts("Native arithmetic engine test requires System V x86-64");
#endif
}
