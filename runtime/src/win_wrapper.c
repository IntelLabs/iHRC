//===-- win_wrapper.c - Implementation of offload APIs   ---------------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#if !defined(_WIN32) && !defined(__linux__)
#error This code targets Windows or Linux only.
#endif

#include "impl.h"
#include <stdio.h>
#include <windows.h>
#include <assert.h>
#include "timer.h"


int env_scheduler_hint; // environment variable indicating scheduler to use

char *SCHEDULER_TYPE = NULL;


/*DLLEXPORT*/
void offload_(int numiters,
	      void* context,
		  void (fun)(void*,size_t, void *),
		  void* gContext,
		  gpu_function_t* gpu_function,
		  int scheduler_hint) {
#ifdef PRINT_RUNTIME_STATS
		unsigned long start_t, end_t;
#endif
  	int i, offset=0;	
#ifdef PRINT_RUNTIME_STATS
	num_offload_calls ++;
	start_t = time_in_microseconds();
#endif
	if (numiters != 0) {
		// Check for any environmental variable setup for runtime
		scheduler_hint =  (env_scheduler_hint == -1) ? scheduler_hint : env_scheduler_hint;
		DEBUG(printf("scheduler=%d env_scheduler=%d\n", scheduler_hint, env_scheduler_hint));
		DEBUG(printf("using scheduler=%d\n", scheduler_hint));
		//printf("using scheduler=%d numiters=%d\n", scheduler_hint, numiters);
		switch (scheduler_hint) {
			// GPU execution only
			case GPU: try_gpu_apply_f(numiters,0,NULL,fun,context,gpu_function, gContext); break;

			// Single core CPU execution
			case SINGLE_CORE_CPU: for(i=offset;i<numiters+offset;i++){fun(context,i, gContext);} break;

			// Multicore CPU execution using TBB runtime
			case MULTI_CORE_CPU_TBB: cpu_apply_f(numiters,0,fun, NULL, context, gContext); break;

			default: printf("HETERO Runtime Error: NO KNOWN SCHEDULER\n");
		}
	}
#ifdef PRINT_RUNTIME_STATS
	end_t = time_in_microseconds();
	total_time += (end_t - start_t);
#endif
	return;
}

/*DLLEXPORT*/
void offload_reduce_(int numiters,
	      void* context,
		  void (fun)(void*,size_t, void *),
		  void* gContext,
		  void (seq_join) (void*,void*),
	      gpu_function_t* gpu_function,
		  int scheduler_hint) {
	printf("HETERO Runtime Error: Reduction not yet supported\n");
}
