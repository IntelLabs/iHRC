//===-- HeteroplusOffload.h - API for offload runtime integration  -------------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#ifndef HETERO_OFFLOAD_H
#define HETERO_OFFLOAD_H

#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/IR/Instructions.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/CFG.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/ADT/UniqueVector.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <set>
#include <map>
#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"

using namespace std;
using namespace llvm;

namespace {
	struct HeteroCommon {
		//static string OFFLOAD_NAME;
		static string OFFLOAD_RUNTIME_NAME;
		static string PARALLEL_FOR_HETERO;
		static string PARALLEL_REDUCE_HETERO;
		/* For a given callsite, find the hetero_function */
		string *get_hetero_function_f(Module &M, CallSite&);
		int is_hetero_function_f(Module &M, CallSite&);

		/* Is the function a built-in function */
		bool isBuiltinFunction(Function *);
	};

}

//string HeteroCommon::OFFLOAD_NAME="offload";
string HeteroCommon::OFFLOAD_RUNTIME_NAME="offload_";
string HeteroCommon::PARALLEL_FOR_HETERO="_Z19parallel_for_hetero";
string HeteroCommon::PARALLEL_REDUCE_HETERO="_Z22parallel_reduce_hetero";
/*
bool HeteroCommon::isBuiltinFunction(Function *f) {


	if(f->getName()=="llvm.IGHAL.native.sqrtf"||f->getName()=="sqrt")return true;
	return false;
}
*/
// check three arguments, pattern match on the name "dispatch_apply_f", 
// check second argument type is queue_hetero_type 
// call void @_Z16dispatch_apply_fP23dispatch_queue_hetero_sPvPFvS1_jE(%struct.dispatch_queue_hetero_s* %tmp9, i8* %0, void (i8*, i32)* @stream_inside)
// find the last argument of dispatch_apply_f, its a function pointer
// find the corresponding function definition, only one target for our case
// call void @_Z16dispatch_apply_fP23dispatch_queue_hetero_sPvPFvS1_jE(%struct.dispatch_queue_hetero_s* %tmp9, i8* %0, void (i8*, i32)* @stream_inside)
/*string *HeteroCommon::get_hetero_function_f(Module &M, CallSite &CS) {
	if (CS.getCalledFunction() != NULL) {
		string called_func_name = CS.getCalledFunction()->getNameStr();
		DEBUG(dbgs()<<"FUN: "<<called_func_name<<"\n");
		if (called_func_name.substr(0,23) == PARALLEL_FOR_HETERO && 
			CS.arg_size() == 2) {
				//Value *arg_1 = CS.getArgument(1);
				//if (arg_1 != NULL)
					DEBUG(dbgs()<<"Offload: "<<called_func_name<<"\n");
					return new string(called_func_name);
					//return dyn_cast<Function>(arg_2);
		}
	}
	return NULL;
}*/
int HeteroCommon::is_hetero_function_f(Module &M, CallSite &CS) {
	if (CS.getCalledFunction() != NULL) {
		string called_func_name = CS.getCalledFunction()->getName().str();
		//DEBUG(dbgs()<<"FUN: "<<called_func_name<<"\n");
		if (called_func_name.substr(0,23) == PARALLEL_FOR_HETERO&& CS.arg_size() == 3) {
					DEBUG(dbgs()<<"Parallel_for_hetero: "<<called_func_name<<"\n");
					return 1;
		}	 
		if (called_func_name.substr(0,26) == PARALLEL_REDUCE_HETERO&& CS.arg_size() == 3) {
					DEBUG(dbgs()<<"Parallel_reduce_hetero: "<<called_func_name<<"\n");
					return 2;
		}

	}
	return 0;
}
#endif
