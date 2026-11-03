/* SPDX-License-Identifier: GPL-3.0-or-later
 * Test-only preload: emulate a process policy that denies executable memory.
 */
#define _GNU_SOURCE
#include <sys/mman.h>
#include <dlfcn.h>
#include <errno.h>
int mprotect(void *address,size_t length,int protection) {
  int (*real_mprotect)(void *,size_t,int);
  if(protection&PROT_EXEC) { errno=EPERM; return -1; }
  *(void **)(&real_mprotect)=dlsym(RTLD_NEXT,"mprotect");
  return real_mprotect(address,length,protection);
}
