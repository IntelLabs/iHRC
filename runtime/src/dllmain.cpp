//===-- dllmain.cpp - runtime initialization and runtime arguments   ----===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===

#include <windows.h>
#include <stdio.h>

extern "C" {
#include "impl.h"
};


#ifdef CPU_TBB_RUNTIME
#include <tbb/task_scheduler_init.h>
using namespace tbb;
#endif



void find_chunk_params() {
	SCHEDULER_TYPE = getenv("SCHEDULER");
}

void hetero_init() 
{
#if defined CPU_TBB_RUNTIME 
    tbb::task_scheduler_init::automatic;
#endif
    // initialize OpenCL GPU runtime
    if (init_ocl() != CL_SUCCESS) {
		printf("ERROR: Could not initialize opencl runtime!\n");
		exit(0);
	}

    // find if CPU and GPU chunks are specified in command line
    find_chunk_params();

    //initialize env_scheduler_hint
    env_scheduler_hint = -1;
    if(SCHEDULER_TYPE != NULL) {
	    env_scheduler_hint = atoi(SCHEDULER_TYPE);
    }

#ifdef PRINT_RUNTIME_STATS
	num_offload_calls = 0;
	total_kernel_compile_time = 0;
	total_kernel_execution_time = 0;
	total_time = 0;
	dynamic_perf_gpu_pct_sum = 0;
#endif
}

void hetero_cleanup() 
{
#ifdef PRINT_RUNTIME_STATS
	printf("\n\n### Printing Runtime Statistics::\n"); 
	printf("### Num offload calls=%d\n", num_offload_calls);
	printf("### Time: kernel_compile_time=%0.2fs kernel_exec_time=%2fs total_time=%.2fs \n", 
        (float)((total_kernel_compile_time)   / 1000000.0f), 
        (float)((total_kernel_execution_time) / 1000000.0f), 
        (float)((total_time)                  / 1000000.0f));
#endif
	ocl_cleanup();
}
