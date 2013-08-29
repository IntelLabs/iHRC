//===-- win_hetero_runtime.h - windows runtime specific APIs---------------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#ifndef WIN_HETERO_RUNTIME_H
#define WIN_HETERO_RUNTIME_H

#include "win_hetero.h"
#include "hetero_runtime.h"

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else 
// Linux
#define DLLIMPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

DLLEXPORT
void offload_(int numiters,
              void* context,
              void (fun)(void*,size_t, void *),
			  void *gContext, // for global symbols to kernel to handle virtual functions
			  gpu_function_t* gpu_function,
			  int scheduler_hint);

DLLEXPORT
void offload_reduce_(int numiters,
              void* context,
              void (fun)(void*,size_t, void *),
			  void *gContext, // for global symbols to kernel to handle virtual functions
			  void (seq_join) (void*,void*),
              gpu_function_t* gpu_function,
			  int scheduler_hint);

DLLEXPORT
double kernel_time_in_micro_seconds();

DLLEXPORT
void hetero_init();

DLLEXPORT
void hetero_cleanup();

DLLEXPORT
int init_ocl();

#ifdef __cplusplus
}
#endif

#endif /* WIN_HETERO_RUNTIME_H */
