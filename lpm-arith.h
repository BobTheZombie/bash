/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LPM_ARITH_H
#define LPM_ARITH_H
#include <stdint.h>
#include <stddef.h>
#define LJ_TEXT 512
#define LJ_OPS 128
#define LJ_VARS 16
#define LJ_NAME 64
struct lj_op { unsigned char op; uint64_t value; };
struct lj_program {
  struct lj_op ops[LJ_OPS];
  char vars[LJ_VARS][LJ_NAME];
  size_t count, nvars;
  int target;
  void *code;
  size_t size;
};
struct lj_stats { uint64_t lookups, compiled, hits, rejected, unavailable; };
/* Returned code/cache entries must never be kept across another lookup/clear. */
const struct lj_program *lj_lookup(const char *);
void lj_clear(void);
const struct lj_stats *lj_statistics(void);
int lj_parse(struct lj_program *,const char *);
int lj_native(struct lj_program *);
void lj_release(struct lj_program *);
#endif
