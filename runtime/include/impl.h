//===-- impl.h - API for entire runtime   ---------------------------------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#ifndef IMPL_H
#define IMPL_H


/* Windows or Linux 
 */

#include <CL/cl.h>
#include "win_hetero_runtime.h"

#ifdef __linux__
#include "CL/cl_intel.h"
#endif

struct dispatch_queue_hetero_s;
typedef struct dispatch_queue_hetero_s *dispatch_queue_hetero_t;
struct dispatch_queue_hetero_s {
    cl_command_queue clCommandQueue;
};





//#define SVM_SIZE (128 << 20)
//#define SVM_SIZE (1UL << 28) // 256MB
#define SVM_SIZE (392167424) // 400 MB
#ifndef PAGE_SIZE
#define PAGE_SIZE (1 << 12)
#endif
#define PAGE_ALIGNMENT_MASK (~(PAGE_SIZE-1))

//#define DEBUG 1
#ifdef DEBUG
#define DEBUG(X) {X;}
#else
#define DEBUG(X)
#endif

extern cl_command_queue ClCommandQueue;

extern char *SCHEDULER_TYPE;
extern int env_scheduler_hint;

#ifdef PRINT_RUNTIME_STATS
	extern unsigned long total_kernel_compile_time; // gpu kernel compilation time
	extern unsigned long total_kernel_execution_time; // gpu kernel execution time obtained from opencl profiler
	extern unsigned long total_kernel_time; // gpu_time
	extern unsigned long total_time; // cpu + gpu time
	extern unsigned long num_offload_calls;
	extern double dynamic_perf_gpu_pct_sum;
#endif


//enum scheduler_t {STATIC, DYNAMIC_PROBE, DYNAMIC_RUN};

extern
int init_ocl();

extern void ocl_cleanup();

extern 
void* compile_gpu_module(gpu_program_t* gpu_program,
			 dispatch_queue_hetero_t queue);
extern
int try_gpu_apply_f(int numiters,
					int offset,
		    dispatch_queue_hetero_t queue,
		    void (fun)(void*,size_t, void *),
		    void* context,
		    gpu_function_t* gpu_function,
			void *gContext);
extern
double try_gpu_apply_timer_f(int numiters,
					int offset,
		    dispatch_queue_hetero_t queue,
		    void (fun)(void*,size_t, void *),
		    void* context,
		    gpu_function_t* gpu_function,
			void *gContext);


extern double get_kernel_time_in_micro_seconds(void);

extern int cl_finish(void);

extern
void cpu_apply_f(int numiters,
		int offset,
		void (fun)(void*,size_t, void *),
		void (seq_join) (void*,void*),
		void* context,
		void *gContext);

extern
double cpu_apply_timer_f(int numiters,
		int offset,
		void (fun)(void*,size_t, void *),
		void (seq_join) (void*,void*),
		void* context,
		void *gContext);

extern
int try_gpu_reduce_f(int numiters,
			int offset,
		    dispatch_queue_hetero_t queue,
		    void (fun)(void*,size_t, void *),
			void (seq_join) (void*,void*),
		    void* context,
		    gpu_function_t* gpu_function,
			void *gContext);

#endif /* IMPL_H */
