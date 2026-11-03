/* SPDX-License-Identifier: GPL-3.0-or-later
 * Shell state guards for the native arithmetic engine. Always guard every
 * lookup; compiled code contains no SHELL_VAR pointers or cached values.
 */
#include "config.h"
#include "bashansi.h"
#include "shell.h"
#include "flags.h"
#include "builtins/common.h"
#include "lpm-arith.h"
#include "lpm-jit.h"
#include <inttypes.h>
#include <string.h>
int arithmetic_jit=1;
static uint64_t executed,guard_failures;
static int decimal(const char *text,uint64_t *v) {
  int negative=0; uint64_t n=0;
  if(!text || !*text || strnlen(text,23)>=23) return 0;
  if(*text=='+' || *text=='-') negative=*text++=='-';
  if(!*text || (*text=='0' && text[1])) return 0;
  while(*text) {
    unsigned d=(unsigned char)*text++-'0';
    if(d>9 || n>(UINT64_MAX-d)/10) return 0;
    n=n*10+d;
  }
  *v=negative?0-n:n; return 1;
}
int lpm_jit_eval(const char *text,intmax_t *result,const char **target,intmax_t *assignment) {
  const struct lj_program *p; SHELL_VAR *v; uint64_t values[LJ_VARS],answer;
  int allowed=att_exported|att_local|att_readonly|att_integer|att_imported|att_nounset;
  if(!arithmetic_jit || sizeof(intmax_t)!=sizeof(uint64_t) ||
     privileged_mode || posixly_correct || shell_compatibility_level<53) return 0;
#if defined (RESTRICTED_SHELL)
  if(restricted) return 0;
#endif
  p=lj_lookup(text); if(!p) return 0;
  for(size_t i=0;i<p->nvars;i++) {
    /* The lookup explicitly suppresses dynamic_value callbacks and nameref
     * following. A failed guard must be observationally inert. */
    v=find_variable_for_jit(p->vars[i]);
    if(!v || (v->attributes & ~allowed) || v->dynamic_value || v->assign_func ||
       !decimal(value_cell(v),&values[i])) goto deopt;
    if((int)i==p->target && (v->attributes & ~(att_exported|att_local|att_imported))) goto deopt;
  }
  answer=((uint64_t (*)(uint64_t *))p->code)(values);
  memcpy(result,&answer,sizeof(answer));
  *target=0;
  if(p->target>=0) {
    *target=p->vars[p->target];
    memcpy(assignment,&values[p->target],sizeof(*assignment));
  }
  executed++; return 1;
deopt:
  guard_failures++; return 0;
}
int lpm_jit_control(WORD_LIST *list) {
  const struct lj_stats *s;
  if(!list || list->next) { builtin_usage(); return EX_USAGE; }
  if(!strcmp(list->word->word,"clear")) {
    lj_clear(); executed=guard_failures=0; return EXECUTION_SUCCESS;
  }
  if(strcmp(list->word->word,"stats")) { builtin_usage(); return EX_USAGE; }
  s=lj_statistics();
  printf("enabled=%d lookups=%" PRIu64 " compiled=%" PRIu64 " hits=%" PRIu64
         " executed=%" PRIu64 " guard_fallbacks=%" PRIu64 " rejected=%" PRIu64
         " unavailable=%" PRIu64 "\n",arithmetic_jit,s->lookups,s->compiled,s->hits,
         executed,guard_failures,s->rejected,s->unavailable);
  return sh_chkwrite(EXECUTION_SUCCESS);
}
void lpm_jit_shutdown(void) { lj_clear(); }
