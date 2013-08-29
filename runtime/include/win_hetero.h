//===-- win_hetero - windows runtime specific data structures---------------===//
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

/*extern __declspec(dllexport)
	int init_ocl();*/

typedef enum {
	GPU = 0,
	SINGLE_CORE_CPU,
	MULTI_CORE_CPU_TBB,
} scheduler_hint_t;

extern
void offload(size_t numiters,
             void* context,
             void (fun)(void*,size_t));

#endif
