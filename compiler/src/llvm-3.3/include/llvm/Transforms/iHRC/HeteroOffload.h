//===-- HeteroOffload.h - internal API for offload code generation  ------===//
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
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
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
		static string OFFLOAD_NAME;
		static string OFFLOAD_RUNTIME_NAME;
		static string OFFLOAD_REDUCE_RUNTIME_NAME;

		/* For a given callsite, find the hetero_function */
		Function *get_hetero_function_f(Module &M, CallSite&,int *type);

		/* Is the function a built-in function */
		bool isBuiltinFunction(Value *);
	};

}

string HeteroCommon::OFFLOAD_NAME="offload";
string HeteroCommon::OFFLOAD_RUNTIME_NAME="offload_";
string HeteroCommon::OFFLOAD_REDUCE_RUNTIME_NAME="offload_reduce_";

bool HeteroCommon::isBuiltinFunction(Value *v) {
      if (isa<Function>(v)) {
		  if (v->getName().substr(0,6).compare("native")  == 0) return true;
		  if (v->getName().substr(0,11).compare("llvm.memcpy")  == 0) return true;
		  if (v->getName().compare("llvm.pow.f32")  == 0) return true;
		  if (Instruction *I = dyn_cast<Instruction>(v)){
				I->getMetadata("builtin");
				if(!I)return true;
		  }

		  /* IMPORTANT NOTE BEFORE EDITING -- Maintain the order, first the f versions and then without f */
		  const char *builtin[] = {
			  "sqrtf", "cosf", "acosf", "fabsf",
			  "sinf", "asinf", "logf", "log10f", 
			  "expf", "fmodf", "exp", "sqrt", "acos", 
			  "fabs", "log10", "get_global_id", "get_local_id", 
			  "get_group_id", "barrier"};
		  vector<string> vec(builtin, /*end(builtin)*/builtin+19);
		  vector<string>::iterator it = find(vec.begin(), vec.end(), v->getName());
		  return (it !=  vec.end());
			
      }
      return false;
}


// check three arguments, pattern match on the name "dispatch_apply_f", 
// check second argument type is queue_hetero_type 
// call void @_Z16dispatch_apply_fP23dispatch_queue_hetero_sPvPFvS1_jE(%struct.dispatch_queue_hetero_s* %tmp9, i8* %0, void (i8*, i32)* @stream_inside)
// find the last argument of dispatch_apply_f, its a function pointer
// find the corresponding function definition, only one target for our case
// call void @_Z16dispatch_apply_fP23dispatch_queue_hetero_sPvPFvS1_jE(%struct.dispatch_queue_hetero_s* %tmp9, i8* %0, void (i8*, i32)* @stream_inside)
Function *HeteroCommon::get_hetero_function_f(Module &M, CallSite &CS,int *type) {
	if (CS.getCalledFunction() != NULL) {
		Function *f=CS.getCalledFunction();
		string called_func_name = f->getName();
		if (called_func_name == OFFLOAD_NAME /*&& 
			CS.arg_size() == 3*/) {
				MDNode *Node = CS->getMetadata("join_gpu");
				if(!Node)*type=1;
				else *type=2;
				Value *arg_2 = CS.getArgument(2);
				if (arg_2 != NULL) return dyn_cast<Function>(arg_2);
		}
	}
	return NULL;
}
#endif
