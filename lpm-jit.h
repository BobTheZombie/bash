/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LPM_JIT_H
#define LPM_JIT_H
/* Include shell.h before this header. */
extern int arithmetic_jit;
int lpm_jit_eval(const char *,intmax_t *,const char **,intmax_t *);
int lpm_jit_control(WORD_LIST *);
void lpm_jit_shutdown(void);
#endif
