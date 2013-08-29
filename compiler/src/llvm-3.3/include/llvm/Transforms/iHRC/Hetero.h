//===-- Hetero.h - high-level API for Hetero code generation  -----------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#ifndef HETERO_H
#define HETERO_H

//#include "llvm/Transforms/IPO/HeteroGCD.h"
#include "llvm/Transforms/iHRC/HeteroOffload.h"
#include "llvm/Transforms/iHRC/HeteroDriverHeader.h"



#define MANGLE(x) "__hetero_" + x + "_"
//#define MANGLE(x) x 


namespace {

	struct Hetero : HeteroCommon {		
		/* Name Mangle string */
		static string NAME_MANGLE_STRING;
		/* GET_GLOBAL_ID function */
		static string GET_GLOBAL_ID_NAME;
	};
}
string Hetero::NAME_MANGLE_STRING="__hetero__";
string Hetero::GET_GLOBAL_ID_NAME="get_global_id";

#endif
