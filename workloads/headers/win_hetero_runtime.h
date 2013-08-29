//===-- HeteroCPUTransform.cpp - Generate CPU/IA code  -------------------===//
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
#define DLLIMPORT __declspec(dllimport)
#else 
// Linux
#define DLLIMPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

DLLIMPORT
void offload_(int numiters,
              void* context,
              void (fun)(void*,size_t, void *),
			  void *gContext, // for global symbols to kernel to handle virtual functions
			  void (seq_join) (void*,void*),
              gpu_function_t* gpu_function);
DLLIMPORT
void offload_reduce_(int numiters,
              void* context,
              void (fun)(void*,size_t, void *),
			  void *gContext, // for global symbols to kernel to handle virtual functions
			  void (seq_join) (void*,void*),
              gpu_function_t* gpu_function);


DLLIMPORT
void hetero_init();

DLLIMPORT
void hetero_cleanup();


#ifdef __cplusplus
}
#endif

#endif /* WIN_HETERO_RUNTIME_H */
