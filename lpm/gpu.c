/* SPDX-License-Identifier: GPL-3.0-or-later
 * Minimal OpenCL 1.2 host ABI declarations. No SDK/build-time dependency.
 * The runtime and device compiler are discovered only on explicit request.
 */
#include "accel.h"
#include <dlfcn.h>
#include <stdlib.h>
typedef int32_t cl_int;
typedef uint32_t cl_uint;
typedef uint64_t cl_ulong;
typedef struct _cl_platform_id *cl_platform_id;
typedef struct _cl_device_id *cl_device_id;
typedef struct _cl_context *cl_context;
typedef struct _cl_command_queue *cl_command_queue;
typedef struct _cl_program *cl_program;
typedef struct _cl_kernel *cl_kernel;
typedef struct _cl_mem *cl_mem;
typedef void (*context_notify)(const char *,const void *,size_t,void *);
typedef void (*program_notify)(cl_program,void *);
struct api {
  cl_int (*platforms)(cl_uint,cl_platform_id *,cl_uint *);
  cl_int (*devices)(cl_platform_id,cl_ulong,cl_uint,cl_device_id *,cl_uint *);
  cl_context (*context)(const intptr_t *,cl_uint,const cl_device_id *,context_notify,void *,cl_int *);
  cl_command_queue (*queue)(cl_context,cl_device_id,cl_ulong,cl_int *);
  cl_program (*program)(cl_context,cl_uint,const char **,const size_t *,cl_int *);
  cl_int (*build)(cl_program,cl_uint,const cl_device_id *,const char *,program_notify,void *);
  cl_kernel (*kernel)(cl_program,const char *,cl_int *);
  cl_mem (*buffer)(cl_context,cl_ulong,size_t,void *,cl_int *);
  cl_int (*arg)(cl_kernel,cl_uint,size_t,const void *);
  cl_int (*run)(cl_command_queue,cl_kernel,cl_uint,const size_t *,const size_t *,const size_t *,cl_uint,const void *,void *);
  cl_int (*read)(cl_command_queue,cl_mem,cl_uint,size_t,size_t,void *,cl_uint,const void *,void *);
  cl_int (*finish)(cl_command_queue);
  cl_int (*release_mem)(cl_mem);
  cl_int (*release_kernel)(cl_kernel);
  cl_int (*release_program)(cl_program);
  cl_int (*release_queue)(cl_command_queue);
  cl_int (*release_context)(cl_context);
};
int lpm_gpu_affine(uint64_t *a,size_t n,uint64_t m,uint64_t b) {
  struct api f={0}; void *lib=0; uint64_t *out=0; cl_platform_id *ps=0;
  cl_context c=0; cl_command_queue q=0; cl_program p=0; cl_kernel k=0;
  cl_mem buf=0; cl_device_id dev=0; cl_uint np=0,i; cl_int err; int rc=-1;
  const char *src="__kernel void affine(__global ulong *a, ulong m, ulong b) { size_t i=get_global_id(0); a[i]=a[i]*m+b; }";
  if(!n) return 0;
  if(n>SIZE_MAX/sizeof(*a)) return -1;
  lib=dlopen("libOpenCL.so.1",RTLD_NOW|RTLD_LOCAL); if(!lib) goto done;
#define LOAD(member,name) do { *(void **)(&f.member)=dlsym(lib,name); if(!f.member) goto done; } while(0)
  LOAD(platforms,"clGetPlatformIDs"); LOAD(devices,"clGetDeviceIDs");
  LOAD(context,"clCreateContext"); LOAD(queue,"clCreateCommandQueue");
  LOAD(program,"clCreateProgramWithSource"); LOAD(build,"clBuildProgram");
  LOAD(kernel,"clCreateKernel"); LOAD(buffer,"clCreateBuffer");
  LOAD(arg,"clSetKernelArg"); LOAD(run,"clEnqueueNDRangeKernel");
  LOAD(read,"clEnqueueReadBuffer"); LOAD(finish,"clFinish");
  LOAD(release_mem,"clReleaseMemObject"); LOAD(release_kernel,"clReleaseKernel");
  LOAD(release_program,"clReleaseProgram"); LOAD(release_queue,"clReleaseCommandQueue");
  LOAD(release_context,"clReleaseContext");
#undef LOAD
  if(f.platforms(0,0,&np) || !np || np>1024) goto done;
  ps=calloc(np,sizeof(*ps)); if(!ps || f.platforms(np,ps,0)) goto done;
  for(i=0;i<np;i++) if(!f.devices(ps[i],4,1,&dev,0)) break; /* GPU devices only */
  if(i==np) goto done;
  c=f.context(0,1,&dev,0,0,&err); if(!c || err) goto done;
  q=f.queue(c,dev,0,&err); if(!q || err) goto done;
  p=f.program(c,1,&src,0,&err); if(!p || err) goto done;
  if(f.build(p,1,&dev,"-cl-std=CL1.2",0,0)) goto done;
  k=f.kernel(p,"affine",&err); if(!k || err) goto done;
  buf=f.buffer(c,1|32,n*sizeof(*a),a,&err); if(!buf || err) goto done;
  if(f.arg(k,0,sizeof(buf),&buf) || f.arg(k,1,sizeof(m),&m) || f.arg(k,2,sizeof(b),&b)) goto done;
  out=malloc(n*sizeof(*a)); if(!out) goto done;
  if(f.run(q,k,1,0,&n,0,0,0,0) || f.read(q,buf,1,0,n*sizeof(*a),out,0,0,0)) goto done;
  /* Release an in-flight kernel before its queue/context even on failure. */
  for(size_t j=0;j<n;j++) a[j]=out[j];
  rc=0;
done:
  if(q) f.finish(q);
  if(buf) f.release_mem(buf);
  if(k) f.release_kernel(k);
  if(p) f.release_program(p);
  if(q) f.release_queue(q);
  if(c) f.release_context(c);
  free(ps); free(out); if(lib) dlclose(lib); return rc;
}
