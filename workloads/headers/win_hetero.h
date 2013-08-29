//===-- HeteroCPUTransform.cpp - Generate CPU/IA code  -------------------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===

#ifndef WIN_HETERO_H
#define WIN_HETERO_H

#include <stdlib.h>

typedef enum {
	GPU = 0,
	SINGLE_CORE_CPU,
	MULTI_CORE_CPU_TBB,
	MULTI_CORE_CPU_WS,
	MULTI_CORE_CPU_VEC,
	CPU_GPU_DYNAMIC_PERF_TBB,
	CPU_GPU_DYNAMIC_PERF_WS,
	CPU_GPU_STATIC_PERF,
	CPU_GPU_ENERGY_STATIC,
	CPU_GPU_ENERGY_DYNAMIC
}scheduler_hint_t;

extern "C" void dummy_runtime_func(int t);
//extern void parallel_for_hetero(int numiters,const Body &body);

template<typename Body>
#ifdef _GNUC_
__attribute__((noinline))
#endif
void parallel_for_hetero(int numiters,/*const */Body &body, int t=GPU){
	for(int i=0; i < numiters; i++){
		body(i);
	}
	dummy_runtime_func(t);
}

#endif

