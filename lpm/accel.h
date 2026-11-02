/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LPM_ACCEL_H
#define LPM_ACCEL_H
#include <stddef.h>
#include <stdint.h>
#define LPM_MAX_OPS 1024
struct lpm_op { unsigned char op; uint64_t value; };
struct lpm_program {
  struct lpm_op ops[LPM_MAX_OPS];
  size_t count;
  void *code;
  size_t code_size;
};
int lpm_compile(struct lpm_program *, const char *, int);
uint64_t lpm_eval(const struct lpm_program *, uint64_t);
void lpm_destroy(struct lpm_program *);
void lpm_affine(uint64_t *, size_t, uint64_t, uint64_t);
/* Returns zero on success. Never modifies input on failure. */
int lpm_gpu_affine(uint64_t *, size_t, uint64_t, uint64_t);
#endif
