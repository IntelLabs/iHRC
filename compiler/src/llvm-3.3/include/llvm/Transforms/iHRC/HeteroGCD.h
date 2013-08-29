//===-- HeteroGCD.h - GCD specific code  ---------------------------------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#ifndef HETERO_GCD_H
#define HETERO_GCD_H

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Instructions.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Instructions.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/CFG.h"
#include "llvm/GlobalVariable.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/BasicBlock.h"
#include "llvm/ADT/UniqueVector.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/compiler.h"
#include "llvm/Support/debug.h"
#include <set>
#include <map>
#include <sstream>
#include <iostream>
#include <fstream>
#include <string>

using namespace std;
using namespace llvm;
//#define HETERO_GCD_HAS_BLOCK_COPY
//#define HETERO_GCD_ALLOC_TO_MALLOC

namespace {

	struct HeteroCommon {

		/* signature dispatch_apply_hetero(iters, queue, func_ptr) */
		static string DISPATCH_APPLY_HETERO_NAME;

		/* signature dispatch_apply_hetero_f(Q, (void *)c, func_ptr) */
		static string DISPATCH_APPLY_HETERO_F_NAME;

		/* hetero queue name*/
		static string DISPATCH_QUEUE_HETERO_NAME;

		/* Hetero Queue struct */
		static string DISPATCH_QUEUE_HETERO_STRUCT_NAME;

		/* Dispatch Queue struct */
		static string DISPATCH_QUEUE_STRUCT_NAME;

		/*  Built-in intrinsics for sync_fetch_add method */
		static string SYNC_FETCH_ADD_NAME;
		static string ATOMIC_DEC;
		static string ATOMIC_INC;

		/* Runtime API name for dispatch_apply_hetero as dispatch_apply_hetero_ */
		static string DISPATCH_APPLY_HETERO_F_RUNTIME_NAME;

#ifdef HETERO_GCD_HAS_BLOCK_COPY
		/* Block copy and release functions */
		static string BLOCK_COPY;
		static string BLOCK_RELEASE;
#endif

		/* For a given callsite, find the hetero_function */
		Function *get_hetero_function(Module &M, CallSite&);
		Function *get_hetero_function_f(Module &M, CallSite&);

		/* Is the function a built-in function */
		bool isBuiltinFunction(Function *);

	private:
		/* Checking for function signatures */
		bool is_dispatch_apply_hetero(Module &, CallSite&);
		bool is_dispatch_apply_hetero_f(Module &, CallSite&);
	};
}

string HeteroCommon::DISPATCH_QUEUE_HETERO_NAME="dispatch_queue_hetero";
string HeteroCommon::DISPATCH_APPLY_HETERO_NAME="dispatch_apply_hetero";
string HeteroCommon::DISPATCH_APPLY_HETERO_F_NAME="dispatch_apply_hetero_f";
string HeteroCommon::DISPATCH_QUEUE_HETERO_STRUCT_NAME="struct.dispatch_queue_hetero_s";
string HeteroCommon::DISPATCH_QUEUE_STRUCT_NAME="struct.dispatch_queue_s";
string HeteroCommon::DISPATCH_APPLY_HETERO_F_RUNTIME_NAME="dispatch_apply_hetero_f_";

#ifdef HETERO_GCD_HAS_BLOCK_COPY
string HeteroCommon::BLOCK_COPY="_Block_copy";
string HeteroCommon::BLOCK_RELEASE="_Block_release";
#endif

/* Built-in functions */
string HeteroCommon::SYNC_FETCH_ADD_NAME="sync_fetch_and_add";
string HeteroCommon::ATOMIC_DEC="atom_deci32g"; // define i32 @atom_decci32g(i32 addrspace(1)* %p)
string HeteroCommon::ATOMIC_INC="atom_inci32g"; // define i32 @atom_inci32g(i32 addrspace(1)* %p)


bool HeteroCommon::isBuiltinFunction(Function *f) {
	return (f->getNameStr().compare(SYNC_FETCH_ADD_NAME) == 0 || f->getNameStr().compare(ATOMIC_DEC) == 0 ||
							f->getNameStr().compare(ATOMIC_INC) == 0);
}
// check three arguments, pattern match on the name "dispatch_apply", 
// check second argument type is queue_hetero_type
// call void @_Z14dispatch_applyjP23dispatch_queue_hetero_sU13block_pointerFvjE(i32 1000, %struct.dispatch_queue_hetero_s* %tmp, void (i32)* %0) 
bool HeteroCommon::is_dispatch_apply_hetero(Module &M, CallSite& CS) {
	string called_func_name = CS.getCalledFunction()->getNameStr();
	Value *arg_1;
	if (called_func_name == DISPATCH_APPLY_HETERO_NAME && 
		CS.arg_size() == 3 && 
		(arg_1 = CS.getArgument(1)) && isa<PointerType>(arg_1->getType())) {
			return true;
	}
	return false;
}

// check three arguments, pattern match on the name "dispatch_apply_f", 
// check second argument type is queue_hetero_type 
// call void @_Z16dispatch_apply_fP23dispatch_queue_hetero_sPvPFvS1_jE(%struct.dispatch_queue_hetero_s* %tmp9, i8* %0, void (i8*, i32)* @stream_inside)
bool HeteroCommon::is_dispatch_apply_hetero_f(Module &M, CallSite& CS) {	
	string called_func_name = CS.getCalledFunction()->getNameStr();
	Value *arg_1;
	if (called_func_name == DISPATCH_APPLY_HETERO_F_NAME && 
		CS.arg_size() == 4 && 
		(arg_1 = CS.getArgument(1)) && isa<PointerType>(arg_1->getType())) {
			return true;		
	}
	return false;
}


// check three arguments, pattern match on the name "dispatch_apply", 
// check second argument type is queue_hetero_type
// call void @_Z14dispatch_applyjP23dispatch_queue_hetero_sU13block_pointerFvjE(i32 1000, %struct.dispatch_queue_hetero_s* %tmp, void (i32)* %0) 
// Algo:
// Find the last argument %0,
// Find %tmp1 from bitcast instruction
// Find uses of %tmp1
// Find %block.tmp1 = getelementptr %tmp1, i32 0, i32 3
// Find store funcPtr, %block.tmp1
// Find the function pointed to by funcPtr and get its signature
/* %tmp1 = alloca %1, align 4                      ; <%1*> [#uses=9]
store i32* %output, i32** %output.addr, align 4
store i32* %input, i32** %input.addr, align 4
store i32 %multiplier, i32* %multiplier.addr, align 4
%tmp = load %struct.dispatch_queue_hetero_s** @Q, align 4 ; <%struct.dispatch_queue_hetero_s*> [#uses=1]
%block.tmp = getelementptr inbounds %1* %tmp1, i32 0, i32 0 ; <i8**> [#uses=1]
store i8* bitcast (i8** @_NSConcreteStackBlock to i8*), i8** %block.tmp
%block.tmp2 = getelementptr inbounds %1* %tmp1, i32 0, i32 1 ; <i32*> [#uses=1]
store i32 1073741824, i32* %block.tmp2
%block.tmp3 = getelementptr inbounds %1* %tmp1, i32 0, i32 2 ; <i32*> [#uses=1]
store i32 0, i32* %block.tmp3
%block.tmp4 = getelementptr inbounds %1* %tmp1, i32 0, i32 3 ; <void (%struct.__block_literal_1*, i32)**> [#uses=1]
store void (%struct.__block_literal_1*, i32)* @__stream_block_invoke_0, void (%struct.__block_literal_1*, i32)** %block.tmp4
%tmp5 = getelementptr inbounds %1* %tmp1, i32 0, i32 5 ; <i32**> [#uses=1]
%tmp6 = load i32** %output.addr, align 4        ; <i32*> [#uses=1]
store i32* %tmp6, i32** %tmp5
%tmp7 = getelementptr inbounds %1* %tmp1, i32 0, i32 6 ; <i32**> [#uses=1]
%tmp8 = load i32** %input.addr, align 4         ; <i32*> [#uses=1]
store i32* %tmp8, i32** %tmp7
%tmp9 = getelementptr inbounds %1* %tmp1, i32 0, i32 7 ; <i32*> [#uses=1]
%tmp10 = load i32* %multiplier.addr, align 4    ; <i32> [#uses=1]
store i32 %tmp10, i32* %tmp9
%block.tmp11 = getelementptr inbounds %1* %tmp1, i32 0, i32 4 ; <i8**> [#uses=1]
store i8* bitcast (%0* @__block_descriptor_tmp to i8*), i8** %block.tmp11
%0 = bitcast %1* %tmp1 to void (i32)*           ; <void (i32)*> [#uses=1]
call void @_Z14dispatch_applyjP23dispatch_queue_hetero_sU13block_pointerFvjE(i32 1000, %struct.dispatch_queue_hetero_s* %tmp, void (i32)* %0)

Another pattern:
entry:
%tmp3 = alloca %1, align 4                      ; <%1*> [#uses=7]
%call = call %struct.dispatch_queue_hetero_s* (...)* @dispatch_queue_hetero_create() nounwind ; <%struct.dispatch_queue_hetero_s*> [#uses=1]
%call1 = call i8* (...)* @getSharedRegion() nounwind ; <i8*> [#uses=2]
%0 = bitcast i8* %call1 to i32*                 ; <i32*> [#uses=1]
%block.tmp = getelementptr inbounds %1* %tmp3, i32 0, i32 0 ; <i8**> [#uses=1]
store i8* bitcast (i8** @_NSConcreteStackBlock to i8*), i8** %block.tmp, align 4
%block.tmp4 = getelementptr inbounds %1* %tmp3, i32 0, i32 1 ; <i32*> [#uses=1]
store i32 1073741824, i32* %block.tmp4, align 4
%block.tmp5 = getelementptr inbounds %1* %tmp3, i32 0, i32 2 ; <i32*> [#uses=1]
store i32 0, i32* %block.tmp5, align 4
%block.tmp6 = getelementptr inbounds %1* %tmp3, i32 0, i32 3 ; <void (%struct.__block_literal_1*, i32)**> [#uses=1]
store void (%struct.__block_literal_1*, i32)* @__main_block_invoke_0, void (%struct.__block_literal_1*, i32)** %block.tmp6, align 4
%tmp7 = getelementptr inbounds %1* %tmp3, i32 0, i32 5 ; <i32**> [#uses=1]
store i32* %0, i32** %tmp7, align 4
%block.tmp9 = getelementptr inbounds %1* %tmp3, i32 0, i32 4 ; <i8**> [#uses=1]
store i8* bitcast (%0* @__block_descriptor_tmp to i8*), i8** %block.tmp9, align 4
%call10 = call i32 (...)* @dispatch_apply(i32 16, %struct.dispatch_queue_hetero_s* %call, %1* %tmp3) nounwind ; <i32> [#uses=0]
*/
Function *HeteroCommon::get_hetero_function(Module &M, CallSite &CS) {
	if (CS.getCalledFunction() != NULL) {
		string called_func_name = CS.getCalledFunction()->getNameStr();
		Value *arg_1;
		if (called_func_name == DISPATCH_APPLY_HETERO_NAME && 
			CS.arg_size() == 3 && 
			(arg_1 = CS.getArgument(1)) && isa<PointerType>(arg_1->getType())) {
				Value *arg_2 = CS.getArgument(2);
				BitCastInst* BCI;
				Value *context;
				StoreInst *SI;
				if (arg_2 != NULL && (((BCI = dyn_cast<BitCastInst>(arg_2)) && (context = BCI->getOperand(0))) || 
					                  (context = dyn_cast<AllocaInst>(arg_2)) /*||*/
									  // %conv16 = bitcast %0* %tmp7 to i8*              ; <i8*> [#uses=2]
									  // %call17 = call i8* @_Block_copy(i8* %conv16) nounwind ssp ; <i8*> [#uses=1]
									  // %conv18 = bitcast i8* %call17 to void (i32)*    ; <void (i32)*> [#uses=1]
									  //((BCI = dyn_cast<BitCastInst>(arg_2)) && (context1 = BCI->getOperand(0)) && CI = dyn_cast<CallInst> ) ||
									 )) {
					for (Value::use_iterator i = context->use_begin(), e = context->use_end(); i != e; ++i) {
						Instruction *insn = dyn_cast<Instruction>(*i);
						GetElementPtrInst *GEP;
						if ((GEP = dyn_cast<GetElementPtrInst>(insn)) && 
							isa<ConstantInt>(GEP->getOperand(2)) && 
							((cast<ConstantInt>(GEP->getOperand(2)))->equalsInt(3))) {
								for (Value::use_iterator I = insn->use_begin(), E = insn->use_end(); I != E; ++I) {
									if ((SI = dyn_cast<StoreInst>(*I))) { 
										Value *op_0 = SI->getOperand(0);
										return dyn_cast<Function>(op_0);
									}
								}
						}
					}
				}
		}
	}
	return NULL;
}

// check three arguments, pattern match on the name "dispatch_apply_f", 
// check second argument type is queue_hetero_type 
// call void @_Z16dispatch_apply_fP23dispatch_queue_hetero_sPvPFvS1_jE(%struct.dispatch_queue_hetero_s* %tmp9, i8* %0, void (i8*, i32)* @stream_inside)
// find the last argument of dispatch_apply_f, its a function pointer
// find the corresponding function definition, only one target for our case
// call void @_Z16dispatch_apply_fP23dispatch_queue_hetero_sPvPFvS1_jE(%struct.dispatch_queue_hetero_s* %tmp9, i8* %0, void (i8*, i32)* @stream_inside)
Function *HeteroCommon::get_hetero_function_f(Module &M, CallSite &CS) {
	if (CS.getCalledFunction() != NULL) {
		string called_func_name = CS.getCalledFunction()->getNameStr();
		Value *arg_1;
		if (called_func_name == DISPATCH_APPLY_HETERO_F_NAME && 
			CS.arg_size() == 4 && 
			(arg_1 = CS.getArgument(1)) && isa<PointerType>(arg_1->getType())) {
				Value *arg_3 = CS.getArgument(3); // This is the function ptr
				if (arg_3 != NULL) 
					return dyn_cast<Function>(arg_3);
		}
	}
	return NULL;
}
#endif
