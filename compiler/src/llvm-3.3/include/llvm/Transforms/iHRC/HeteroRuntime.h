//===-- HeteroRuntime.h - API for iHRC runtime integration  -------------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#ifndef HETERO_RUNTIME_H
#define HETERO_RUNTIME_H

namespace {
	struct HeteroRuntime {
		/* gpu_program_s */
		static string GPU_PROGRAM_STRUCT_NAME;

		/* gpu_function_s */
		static string GPU_FUNCTION_STRUCT_NAME;
	};
}
string HeteroRuntime::GPU_PROGRAM_STRUCT_NAME="struct._gpu_program";
string HeteroRuntime::GPU_FUNCTION_STRUCT_NAME="struct._gpu_function";
#endif
