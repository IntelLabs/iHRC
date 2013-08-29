//===-- HeteroCPUTransform.cpp - Generate CPU/IA code  -------------------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#include "llvm/Transforms/iHRC/HeteroCPUTransform.h"
using namespace llvm;


// TODO
// static const char programIr[] = 
// "__kernel void foo(__global char *sharedAre,
//            private ptrdiff_t baseAddr,
//            private ptrdiff_t contextAddr) {
//  size_t = get_global_id(0);
// }";
// static gpu_program_t program = {programIr, 0, 0};
// LLVM IR:
// @programIr = internal constant [156 x i8] c"__kernel void foo(__global char *sharedAre,            
//                    private ptrdiff_t baseAddr, private ptrdiff_t contextAddr) {  size_t = get_global_id(0);}\00", 
//                    align 1 ; <[156 x i8]*> [#uses=1]
// 
// @program = internal global %struct._gpu_program { i8* getelementptr inbounds ([156 x i8]* @programIr, i32 0, i32 0), i32 0, i8* null }, 
//                    align 8 ; <%struct._gpu_program*> [#uses=2]
void HeteroCPUTransform::gen_init_globals(Module &M, string ghal, string opencl) {

	vector</*const*/ Type *> params;

	/* do the _gpu_program and _gpu_function exist */
	// %struct._gpu_program = type { i8*, i32, i8* }
	// %struct._gpu_function = type { %struct._gpu_program*, i8*, i8* }	
	gpuProgTy = M.getTypeByName(GPU_PROGRAM_STRUCT_NAME);
	gpuFuncTy = M.getTypeByName(GPU_FUNCTION_STRUCT_NAME);
	if (gpuFuncTy == NULL || gpuProgTy == NULL) {

		// %struct._gpu_program = type { i8*, i32, i8* }
		params.clear();
		params.push_back(PointerType::get(Type::getInt8Ty(M.getContext()),0));
		params.push_back(Type::getInt32Ty(M.getContext()));
		params.push_back(PointerType::get(Type::getInt8Ty(M.getContext()),0));

		/* for opencl code generation */
		params.push_back(PointerType::get(Type::getInt8Ty(M.getContext()),0));
		params.push_back(Type::getInt32Ty(M.getContext()));
		params.push_back(PointerType::get(Type::getInt8Ty(M.getContext()),0));

		gpuProgTy = StructType::get(M.getContext(), params, false); 

		// %struct._gpu_function = type { %struct._gpu_program*, i8*, i8* }
		params.clear();
		params.push_back(PointerType::get(gpuProgTy, 0));
		params.push_back(PointerType::get(Type::getInt8Ty(M.getContext()),0));
		params.push_back(PointerType::get(Type::getInt8Ty(M.getContext()),0));
		/* For opencl code generation */
		params.push_back(PointerType::get(Type::getInt8Ty(M.getContext()),0));

		// Added for Reduction Support

		//unsigned int reduction_object_size;
  		params.push_back(Type::getInt32Ty(M.getContext()));

		//void (*reduce_)(void*,void*);  // cpu join function, if we need to do sequential join
		/*vector<const Type *> joinParams;
		joinParams.clear();
  		joinParams.push_back(PointerType::get(Type::getInt8Ty(M.getContext()),0));
		joinParams.push_back(PointerType::get(Type::getInt8Ty(M.getContext()),0));
		FunctionType *joinNFTy = FunctionType::get(Type::getVoidTy(M.getContext()), joinParams, false);
		params.push_back(PointerType::get(joinNFTy,0));*/

		//const char* /*const*/ gpu_reduce_name; // name of the wrapper created in heterotbbpass, added as meta data
  		params.push_back(PointerType::get(Type::getInt8Ty(M.getContext()),0));

		//void* volatile gpu_reduce_binary; // binary version of gpu_reduce_name
		params.push_back(PointerType::get(Type::getInt8Ty(M.getContext()),0));

		//volatile float cpu_rate, gpu_rate;
		params.push_back(Type::getFloatTy(M.getContext()));
		params.push_back(Type::getFloatTy(M.getContext()));
		//volatile float mem_ratio, control_ratio;
		params.push_back(Type::getFloatTy(M.getContext()));
		params.push_back(Type::getFloatTy(M.getContext()));
		//scheduling value to indicate at k-th invocation of the same kernel we recompute rates
		params.push_back(Type::getInt32Ty(M.getContext()));

		//scheduler hint
		//params.push_back(Type::getInt32Ty(M.getContext()));
		// how many times statically the kernel is invoked
		params.push_back(Type::getInt32Ty(M.getContext()));

		gpuFuncTy = StructType::get(M.getContext(), params, false);
	}


	// %struct.dispatch_queue_hetero_s = type { %struct.dispatch_queue_s* }
	// %struct.dispatch_queue_s = type { i32 }
#ifdef HETERO_GCD_H
	if (heteroQueuePtrTy == NULL) {
		const Type *queueTy;
		const Type *heteroQueueTy;
		queueTy = M.getTypeByName(DISPATCH_QUEUE_STRUCT_NAME);
		heteroQueueTy = M.getTypeByName(DISPATCH_QUEUE_HETERO_STRUCT_NAME);
		if (queueTy == NULL || heteroQueueTy == NULL) {
			params.clear();
			params.push_back(Type::getInt32Ty(M.getContext()));
			queueTy = StructType::get(M.getContext(), params, false);

			params.clear();
			params.push_back(PointerType::get(queueTy, 0));
			heteroQueueTy = StructType::get(M.getContext(), params, false);
		}
		heteroQueuePtrTy = PointerType::get(heteroQueueTy, 0);
	}


	/* Create void (i8*, i32)* */
	params.clear();
	params.push_back(PointerType::get(Type::getInt8Ty(M.getContext()),0));
	params.push_back(intArgTy);
	funcPtrTy = PointerType::get(FunctionType::get(Type::getVoidTy(M.getContext()), params, false),0);

#endif

	// declare void @dispatch_apply_hetero(i32, %struct.dispatch_queue_hetero_s*, void (i32)*, %struct._gpu_function*)
	// declare void @dispatch_apply_hetero_f(i32, %struct.dispatch_queue_hetero_s*, void (i8*, i32)*, i8*, %struct._gpu_function*)
	params.clear();
	params.push_back(intArgTy);
#ifdef HETERO_GCD_H
	params.push_back(heteroQueuePtrTy);
#endif
	if (funcApplyFPtrTy != NULL) {
		params.push_back(contextPtrTy);
		params.push_back(funcApplyFPtrTy);
		params.push_back(gContextPtrTy);
	}
#ifdef HETERO_GCD_H
	else {
		params.push_back(PointerType::get(Type::getInt8Ty(M.getContext()),0));
		params.push_back(funcPtrTy);
	}
#endif
	
	
	// Add gpufunctype
	params.push_back(PointerType::get(gpuFuncTy, 0));
	// push scheduler hint
	params.push_back(Type::getInt32Ty(M.getContext()));


	FunctionType *NFTy = FunctionType::get(Type::getVoidTy(M.getContext()), params, false);
	
#ifdef HETERO_GCD_H
	Constant *dispatch_apply_hetero_f_const = M.getOrInsertFunction(DISPATCH_APPLY_HETERO_F_RUNTIME_NAME, NFTy);
#else
	Constant *dispatch_apply_hetero_f_const = M.getOrInsertFunction(OFFLOAD_RUNTIME_NAME, NFTy);
#endif
	dispatch_apply_hetero_f = cast<Function>(dispatch_apply_hetero_f_const);

	params.pop_back(); // pop scheduler_hint
	params.pop_back(); // pop gpu_functype

	// for reduction
	vector</*const*/ Type *> joinParams;
	joinParams.clear();
  	joinParams.push_back(PointerType::get(Type::getInt8Ty(M.getContext()),0));
	joinParams.push_back(PointerType::get(Type::getInt8Ty(M.getContext()),0));
	FunctionType *joinTy = FunctionType::get(Type::getVoidTy(M.getContext()), joinParams, false);
	params.push_back(PointerType::get(joinTy, 0));

	// Add gpufunctype
	params.push_back(PointerType::get(gpuFuncTy, 0));
	// push scheduler hint
	params.push_back(Type::getInt32Ty(M.getContext()));
	FunctionType *joinNFTy = FunctionType::get(Type::getVoidTy(M.getContext()), params, false);
	Constant *join_dispatch_apply_hetero_f_const = M.getOrInsertFunction(OFFLOAD_REDUCE_RUNTIME_NAME, joinNFTy);
	join_dispatch_apply_hetero_f = cast<Function>(join_dispatch_apply_hetero_f_const);

	
#ifdef HETERO_GCD_HAS_BLOCK_COPY
	/* You do not need these for function pointers... generate anyway... TODO optimize later... generate ssp signature along */
	/* Block_copy syntax */
	if (!insn2BlockMap.empty()) {
		vector<const Type *> BlockParams;
		BlockParams.push_back(PointerType::get(Type::getInt8Ty(M.getContext()),0));
		NFTy = FunctionType::get(PointerType::get(Type::getInt8Ty(M.getContext()),0), BlockParams, false);
		Constant *Block_copy_const = M.getOrInsertFunction(BLOCK_COPY, NFTy);
		Block_copy_func=cast<Function>(Block_copy_const);	
		NFTy = FunctionType::get(Type::getVoidTy(M.getContext()), BlockParams, false);
		Constant *Block_release_const = M.getOrInsertFunction(BLOCK_RELEASE, NFTy);
		Block_release_func=cast<Function>(Block_release_const);
	}
#endif	

	/* emit program containing ghal IR */
	Constant *ghalStr = ConstantDataArray::getString(M.getContext(), ghal.c_str(), true);
	programIr_ghal = new GlobalVariable(M, ghalStr->getType(), true, GlobalVariable::InternalLinkage, ghalStr, "__hetero_programIr_ghal");

	/* emit opencl code generation  -- remove later once opencl is stable*/
	Constant *openclStr = ConstantDataArray::getString(M.getContext(), opencl.c_str(), true);
	programIr_cl = new GlobalVariable(M, openclStr->getType(), true, GlobalVariable::InternalLinkage, openclStr, "__hetero_programIr_cl");

	Constant *Fields[6] = {
		/* i8* getelementptr inbounds ([156 x i8]* @programIr, i32 0, i32 0) */
		ConstantExpr::getBitCast(programIr_ghal, PointerType::get(Type::getInt8Ty(M.getContext()), 0)),
		/* i32 0 */
		ConstantInt::get(Type::getInt32Ty(M.getContext()), 0),
		/* i8* null */
		ConstantExpr::getNullValue(PointerType::get(Type::getInt8Ty(M.getContext()),0)),
		
		/* i8* getelementptr inbounds ([156 x i8]* @programIr, i32 0, i32 0) */
		ConstantExpr::getBitCast(programIr_cl, PointerType::get(Type::getInt8Ty(M.getContext()), 0)),
		/* i32 0 */
		ConstantInt::get(Type::getInt32Ty(M.getContext()), 0),
		/* i8* null */
		ConstantExpr::getNullValue(PointerType::get(Type::getInt8Ty(M.getContext()),0))
	};

	Type *ElemTy[6] = {
			PointerType::get(Type::getInt8Ty(M.getContext()),0),
			Type::getInt32Ty(M.getContext()),
			PointerType::get(Type::getInt8Ty(M.getContext()),0),
			PointerType::get(Type::getInt8Ty(M.getContext()),0),
			Type::getInt32Ty(M.getContext()),
			PointerType::get(Type::getInt8Ty(M.getContext()),0)
	};
	StructType *StructTy = StructType::get(M.getContext(), ElemTy, false);
	Constant *constStruct = ConstantStruct::get(StructTy, Fields);
#if defined(LLVM_3_1)
	Constant *constStruct = ConstantStruct::get(M.getContext(), Fields, 6, false);
#endif
	program = new GlobalVariable(M, gpuProgTy, false,
		GlobalValue::InternalLinkage, constStruct, "__hetero_program");
}

void HeteroCPUTransform::compute_ratios_for_scheduling(Function *F) {
	unsigned int mem_count=0, insn_count=0, control_count=0;
	for (Function::iterator BBI = F->begin(), BBE = F->end(); BBI != BBE; ++BBI) {
		unsigned int loop_depth = LI->getLoopDepth(BBI);
		double static_freq = pow((double)10.0, (int)loop_depth);
		for (BasicBlock::iterator INSNI = BBI->begin(), INSNE = BBI->end(); INSNI != INSNE; ++INSNI) {
			if (isa<LoadInst>(INSNI) || isa<StoreInst>(INSNI)) {
				mem_count += static_freq;
			}
			else if (isa<TerminatorInst>(INSNI)) {
				control_count += static_freq;
			}
			insn_count += static_freq;
		}
	}
	DEBUG(dbgs() << "\nfunc=" << F->getName() << " mem_count=" << mem_count << " control_count=" << control_count << " insn_count=" << insn_count);
	mem_ratio = (float)mem_count / (float)insn_count;
	control_ratio = (float)control_count / (float)insn_count;
	DEBUG(dbgs() << "\nfunc=" << F->getName() << "  mem_ratio=" << mem_ratio << "  control_ratio=" << control_ratio); 
}
//static gpu_function_t foo_gpu_code = {&program, "foo", 0};
// LLVM IR:
//@.str1 = private constant [4 x i8] c"foo\00"      ; <[4 x i8]*> [#uses=1]
//@foo_gpu_code = internal global %struct._gpu_function { %struct._gpu_program* @program, i8* getelementptr inbounds ([4 x i8]* @.str1, 
//                                                       i32 0, i32 0), i8* null }, align 8 ; <%struct._gpu_function*> [#uses=1]
//
// Additionally, all the call sites of F in terms of dispatch_apply_f or dispatch_apply, 
// need rewrite to include the fifth argument as foo_gpu_code
void HeteroCPUTransform::gen_globals_transform_per_f (Function *F) {

	Module *M = F->getParent();
	//@.str1 = private constant [4 x i8] c"foo\00"      ; <[4 x i8]*> [#uses=1
	string name = MANGLE(F->getName().str());
	Constant *funcNameStr = ConstantDataArray::getString(F->getContext(), name.c_str() , true);
	GlobalVariable *str1 = new GlobalVariable(*M, funcNameStr->getType(), true, GlobalVariable::PrivateLinkage, funcNameStr, ".str1_"+name);

	int size;
	map<Function *, int>::iterator iter = kernel2sizeMap.find(F);
	GlobalVariable *str2;
	if (iter != kernel2sizeMap.end()) {
		size = (*iter).second;
		map<Function *, Function *>::iterator it = kernel2gpujoinMap.find(F);
		Function *joinF = (*it).second;
		string joinname = MANGLE(joinF->getName().str());
		Constant *joinfuncNameStr = ConstantDataArray::getString(F->getContext(), joinname.c_str() , true);
		str2 = new GlobalVariable(*M, joinfuncNameStr->getType(), true, GlobalVariable::PrivateLinkage, joinfuncNameStr, ".str2_"+joinname);
	}
	else {
		size = 0;
		string joinname = "";
		Constant *joinfuncNameStr = ConstantDataArray::getString(F->getContext(), joinname.c_str() , true);
		str2 = new GlobalVariable(*M, joinfuncNameStr->getType(), true, GlobalVariable::PrivateLinkage, joinfuncNameStr, ".str2_"+joinname);
	}
	
	map<Function *, int>::iterator iterr = kernel2numinvokeMap.find(F);
	int numinvoke = 0;
	if (iterr != kernel2numinvokeMap.end()) 
		numinvoke = (*iterr).second;
	
	//APInt sizeInt(32,size); // 32-bit integer and value is size;
	//get compile-time ratios
	LI = &getAnalysis<LoopInfo>(*F);
	compute_ratios_for_scheduling(F);

	unsigned int field_length = 13;
	Constant *Fields[13] = {
		/* %struct._gpu_program* @program  */
		program,
		/* i8* getelementptr inbounds ([4 x i8]* @.str1, i32 0, i32 0) */
		ConstantExpr::getBitCast(str1, PointerType::get(Type::getInt8Ty(F->getContext()), 0)),
		/* i8* null */
		ConstantExpr::getNullValue(PointerType::get(Type::getInt8Ty(F->getContext()), 0)),
		/* i8* null */
		ConstantExpr::getNullValue(PointerType::get(Type::getInt8Ty(F->getContext()), 0)),
		/* object size */
		ConstantInt::get(Type::getInt32Ty(F->getContext()), size),
		/* i8* null */
		ConstantExpr::getBitCast(str2, PointerType::get(Type::getInt8Ty(F->getContext()), 0)),
		/* i8* null */
		ConstantExpr::getNullValue(PointerType::get(Type::getInt8Ty(F->getContext()), 0)),
		/* cpu_rate */
		ConstantFP::get(Type::getFloatTy(F->getContext()), 0.0f),
		/* gpu_rate */
		ConstantFP::get(Type::getFloatTy(F->getContext()), 0.0f),
		/* mem_ratio */
		ConstantFP::get(Type::getFloatTy(F->getContext()), mem_ratio),
		/* control_ratio */
		ConstantFP::get(Type::getFloatTy(F->getContext()), control_ratio),
		/* scheduling the randomized k-th time you invoke the recomputation of rates */
		ConstantInt::get(Type::getInt32Ty(F->getContext()), 0),
		/* num_times_kernel_invoked */
		ConstantInt::get(Type::getInt32Ty(F->getContext()), numinvoke),
	};

	Type *ElemTy[13] = {
			program->getType(),
			PointerType::get(Type::getInt8Ty(F->getContext()),0),
			PointerType::get(Type::getInt8Ty(F->getContext()),0),
			PointerType::get(Type::getInt8Ty(F->getContext()),0),
			Type::getInt32Ty(F->getContext()),
			PointerType::get(Type::getInt8Ty(F->getContext()),0),
			PointerType::get(Type::getInt8Ty(F->getContext()),0),
			Type::getFloatTy(F->getContext()),
			Type::getFloatTy(F->getContext()),
			Type::getFloatTy(F->getContext()),
			Type::getFloatTy(F->getContext()),
			Type::getInt32Ty(F->getContext()),
			Type::getInt32Ty(F->getContext())
	};

	StructType *StructTy = StructType::get(F->getContext(), ElemTy, false);

	//@foo_gpu_code = internal global %struct._gpu_function { %struct._gpu_program* @program, i8* getelementptr inbounds ([4 x i8]* @.str1, 
	//                                                       i32 0, i32 0), i8* null }, align 8 ; <%struct._gpu_function*> [#uses=1]
	string funcGPUCodeName=name+"_gpu_code";
#if defined(LLVM_3_1)
	Constant *constStruct = ConstantStruct::get(F->getContext(), Fields, field_length, false);
#endif
	Constant *constStruct = ConstantStruct::get(StructTy, Fields);
	GlobalVariable *funcGpuCode = new GlobalVariable(*M, gpuFuncTy, false,
		GlobalValue::InternalLinkage, constStruct, funcGPUCodeName);

	// update the call sites of foo to make a new call to dispatch_apply_hetero/dispatch_apply_hetero_f
	// extern void dispatch_apply_hetero(int numiters,
	//			  dispatch_queue_hetero_t queue,
	//			  dispatch_apply_block_t block,
	//			  gpu_function_t* gpu_function);
	// extern void dispatch_apply_hetero_f(int numiters,
	//			    dispatch_queue_hetero_t queue,
	//			    void* context,
	//			    void (fun)(void*,size_t),
	//			    gpu_function_t* gpu_function);	
	//F->dump();
	handleCall(F, funcGpuCode);
}

void HeteroCPUTransform::handleCall(Function *F, Value *gpuCode) {
	pair<multimap<Function *,Instruction *>::iterator,multimap<Function *,Instruction *>::iterator> ret;
#ifdef HETERO_GCD_H
	ret = insn2BlockMap.equal_range(F);
	for (multimap<Function *, Instruction *>::iterator I = ret.first; I != ret.second; ++I) {
		Instruction *Call = (*I).second;
		CallSite CI(cast<Instruction>(Call));
#ifdef HETERO_GCD_ALLOC_TO_MALLOC
		rewrite_alloca(CI);
#endif
		rewrite_call(CI, dispatch_apply_hetero_f, F, gpuCode);
	}
#endif
	ret = insn2FuncMap.equal_range(F);
	for (multimap<Function *, Instruction *>::iterator I = ret.first; I != ret.second; ++I) {
		Instruction *Call = (*I).second;
		CallSite CI(cast<Instruction>(Call));

		// if the called instruction has join metadata, then generate offload_reduce_
		// else use the following rewrite_call
		MDNode *gpuMDNode = Call->getMetadata("join_gpu");
		if (gpuMDNode != NULL) {
			rewrite_call(CI, join_dispatch_apply_hetero_f, NULL, gpuCode, true);
		}
		else rewrite_call(CI, dispatch_apply_hetero_f, NULL, gpuCode, false);
	}
}


#if 0
/* Rewrite alloca to malloc */
void HeteroCPUTransform::rewrite_alloca(CallSite &CS) {
	Instruction *OldCall = CS.getInstruction();
	Function *F = CS.getCalledFunction();
	Value *arg_2 = CS.getArgument(2);
	BitCastInst* BCI;
	Value *context1;
	AllocaInst *context;
	AllocaInst *alloc;
	StoreInst *SI;
	if (arg_2 != NULL && (((BCI = dyn_cast<BitCastInst>(arg_2)) && (context1 = BCI->getOperand(0)) && 
		(context = dyn_cast<AllocaInst>(context1))) || 
		(context = dyn_cast<AllocaInst>(arg_2)) /*||*/
		// %conv16 = bitcast %0* %tmp7 to i8*              ; <i8*> [#uses=2]
		// %call17 = call i8* @_Block_copy(i8* %conv16) nounwind ssp ; <i8*> [#uses=1]
		// %conv18 = bitcast i8* %call17 to void (i32)*    ; <void (i32)*> [#uses=1]
		//((BCI = dyn_cast<BitCastInst>(arg_2)) && (context1 = BCI->getOperand(0)) && CI = dyn_cast<CallInst> ) ||
		)) {
			BasicBlock::iterator ii(context);
			context->dump();
			ReplaceInstWithInst(context->getParent()->getInstList(), ii, 
				new MallocInst(context->getAllocatedType(), 
				context->getArraySize(), context->getAlignment(), ""));
			context1=&(*ii);
			context1->dump();
			for (Value::use_iterator i = context1->use_begin(), e = context1->use_end(); i != e; ++i) {
				Instruction *insn = dyn_cast<Instruction>(*i);
				GetElementPtrInst *GEP, *GEP1;
				LoadInst *LI;
				if ((GEP = dyn_cast<GetElementPtrInst>(insn)) && 
					isa<ConstantInt>(GEP->getOperand(2)) && 
					((cast<ConstantInt>(GEP->getOperand(2)))->uge(5))) {
						insn->dump();
						for (Value::use_iterator I = insn->use_begin(), E = insn->use_end(); I != E; ++I) {
							if ((SI = dyn_cast<StoreInst>(*I)) && 
								((alloc = dyn_cast<AllocaInst>(SI->getOperand(0))) ||
								((LI = dyn_cast<LoadInst>(SI->getOperand(0))) && 
								 (GEP1 = dyn_cast<GetElementPtrInst>(LI->getPointerOperand())) &&
								 (alloc = dyn_cast<AllocaInst>(GEP1->getPointerOperand()))))){ 
								alloc->dump();
								BasicBlock::iterator iii(alloc);
								ReplaceInstWithInst(alloc->getParent()->getInstList(), iii, 
									new MallocInst(alloc->getAllocatedType(), 
									alloc->getArraySize(), alloc->getAlignment(), ""));
								(&(*iii))->dump();
							}
						}
				}
			}
	}
}

#endif

// Rewrite the call instruction to call the hetero function now
// preserve attributes of arguments
// preserve calling convention
void HeteroCPUTransform::rewrite_call(CallSite &CS, Function *NF, Function *blockF, Value *gpuCode, bool hasReduce) {

	Instruction *OldCall = CS.getInstruction();
	Function *F = CS.getCalledFunction();
	Instruction *NewCall; // New Call Instruction created
	SmallVector<Value*, 16> Args; // Argument lists to the new call
#if defined(LLVM_3_2)
	SmallVector<AttributeWithIndex, 8> AttrVec; // Attributes list to the new call
#endif
	SmallVector<AttributeSet, 8> AttrVec;

#ifdef HETERO_GCD_HAS_BLOCK_COPY
	Value *copiedBlockPtr=NULL;
#endif

	//NF->dump();

	// Any attributes (parameter attribute list PAL) of the 
	// dispatch_apply/dispatch_apply_f is 
#if defined(LLVM_3_2)
	const AttrListPtr &OldCallPAL = CS.getAttributes();
	// Add any return attributes.
	Attributes attrs = OldCallPAL.getRetAttributes();
	if (attrs.hasAttributes()) 
		AttrVec.push_back(AttributeWithIndex::get(0, attrs));
#endif
	const AttributeSet &OldCallPAL = CS.getAttributes();
	// Add any return attributes.
	if (OldCallPAL.hasAttributes(AttributeSet::ReturnIndex))
		AttrVec.push_back(AttributeSet::get(F->getContext(),
              OldCallPAL.getRetAttributes()));

	CallSite::arg_iterator AI = CS.arg_begin();
	unsigned Index = 1;
	if (blockF == NULL) { // dispatch_apply_hetero_f case
		for (CallSite::arg_iterator I = CS.arg_begin(), E = CS.arg_end(); I != E; ++I, ++AI, ++Index) {
			Args.push_back(*AI);          // copy the old arguments
			//DEBUG(dbgs() << "Arg" << Index << " type=");
			//(*AI)->getType()->dump();
#if defined(LLVM_3_2)
			Attributes Attrs = OldCallPAL.getParamAttributes(Index);
			if (Attrs.hasAttributes())
				AttrVec.push_back(AttributeWithIndex::get(Args.size(), Attrs));
#endif
			AttributeSet Attrs = OldCallPAL.getParamAttributes(Index);
			if (Attrs.hasAttributes(Index)) {
				AttrBuilder B(Attrs, Index);
				AttrVec.push_back(AttributeSet::get(F->getContext(), Args.size(), B));
			}
		}
	}
#ifdef HETERO_GCD_H
	else { //dispatch_apply_hetero case
		// call void @_Z14dispatch_apply_hetero(i32 1000, %struct.dispatch_queue_hetero_s* %tmp, void (i32)* %0)
		// call void @_Z16dispatch_apply_hetero_f(i32 1000, %struct.dispatch_queue_hetero_s* %tmp9, i8* %0, 
		//													void (i8*, i32)* @stream_inside)
		Args.push_back(*AI);
		if (Attributes Attrs = OldCallPAL.getParamAttributes(Index))
			AttrVec.push_back(AttributeWithIndex::get(Args.size(), Attrs));

		AI++; Index++; 

		Args.push_back(*AI);
		if (Attributes Attrs = OldCallPAL.getParamAttributes(Index))
			AttrVec.push_back(AttributeWithIndex::get(Args.size(), Attrs));
		AI++; Index++;
		
		
#ifdef HETERO_GCD_HAS_BLOCK_COPY
		SmallVector<Value*, 16> BlockArgs; // Argument lists to the new call

		// %conv16 = bitcast %0* %tmp7 to i8*              ; <i8*> [#uses=2]
		CastInst *castContext1 = CastInst::Create(Instruction::BitCast, *AI, 
			PointerType::get(Type::getInt8Ty(F->getContext()),0), "", OldCall);
		BlockArgs.push_back(castContext1);
		
		// %call17 = call i8* @_Block_copy(i8* %conv16) nounwind ssp ; <i8*> [#uses=1]
		// set nounwind and ssp later -- TODO
		Instruction *blockCopyCallInst = CallInst::Create(Block_copy_func, BlockArgs.begin(), BlockArgs.end(), "", OldCall);
		
		// %conv18 = bitcast i8* %call17 to void (i32)*    ; <void (i32)*> [#uses=1]
		CastInst *castContext = CastInst::Create(Instruction::BitCast, blockCopyCallInst, 
			PointerType::get(Type::getInt8Ty(F->getContext()),0), "", OldCall);
		copiedBlockPtr = castContext;
#else
		CastInst *castContext = CastInst::Create(Instruction::BitCast, *AI, 
			PointerType::get(Type::getInt8Ty(F->getContext()),0), "", OldCall);

#endif
		Args.push_back(castContext);

		assert(funcPtrTy != NULL);
		CastInst *castFunc = CastInst::Create(Instruction::BitCast, blockF, funcPtrTy, "", OldCall);
		Args.push_back(castFunc);

	}
#endif
	// Add join function pointer if there is one
	if (hasReduce) {
		int type;
		Function *kernel = get_hetero_function_f(*(F->getParent()), CS, &type);
		//DEBUG(dbgs() << "Looking for Function=" << kernel->getNameStr());
		map<Function *,Function *>::iterator iter = kernel2cpujoinMap.find(kernel);
		vector</*const*/ Type *> joinParams;
		joinParams.clear();
  		joinParams.push_back(PointerType::get(Type::getInt8Ty(F->getContext()),0));
		joinParams.push_back(PointerType::get(Type::getInt8Ty(F->getContext()),0));
		FunctionType *joinNFTy = FunctionType::get(Type::getVoidTy(F->getContext()), joinParams, false);
		
		if (iter != kernel2cpujoinMap.end()) {
			Function *seq_join = (Function *)((*iter).second);
			//cast this function to void fun(void *, void *)
			//DEBUG(dbgs() << "\nseq_join name=" << seq_join->getNameStr());
			//DEBUG(dbgs() << "\nseq_join type=");
			//seq_join->getType()->dump();
			CastInst *castJoin = CastInst::Create(Instruction::BitCast, seq_join, PointerType::get(joinNFTy,0), "", OldCall);
			Args.push_back(castJoin);
		}
		else {
			// set null
			Constant *joinNull = ConstantExpr::getNullValue(PointerType::get(joinNFTy, 0));
			Args.push_back(joinNull);
		}
	}
	
	//DEBUG(dbgs() << "\ngpuCode type=");
	//gpuCode->getType()->dump();
	// Add the new argument to gpuCode
	Args.push_back(gpuCode);

	map<Instruction *, int>::iterator iter1 = insn2schedulerhintMap.find(OldCall);
	int scheduler_hint = 0;
	if (iter1 != insn2schedulerhintMap.end()) {
		scheduler_hint = (*iter1).second;
	}
	Args.push_back(ConstantInt::get(Type::getInt32Ty(F->getContext()), scheduler_hint));
	
	//DEBUG(dbgs() <<"\nExpectedType=");
	//NF->dump();

	if (InvokeInst *invoke = dyn_cast<InvokeInst>(OldCall)) {
		NewCall = InvokeInst::Create(NF, invoke->getNormalDest(), invoke->getUnwindDest(), Args, "", OldCall);
		cast<InvokeInst>(NewCall)->setCallingConv(CS.getCallingConv());
#if defined(LLVM_3_2)
		cast<InvokeInst>(NewCall)->setAttributes(AttrListPtr::get(NF->getContext(), AttrVec));
#endif
		cast<InvokeInst>(NewCall)->setAttributes(AttributeSet::get(NF->getContext(), AttrVec));
	} else {
		NewCall = CallInst::Create(NF, Args, "", OldCall);
		cast<CallInst>(NewCall)->setCallingConv(CS.getCallingConv());
#if defined(LLVM_3_2)
		cast<CallInst>(NewCall)->setAttributes(AttrListPtr::get(NF->getContext(), AttrVec));
#endif
		cast<CallInst>(NewCall)->setAttributes(AttributeSet::get(NF->getContext(), AttrVec));
		if (cast<CallInst>(OldCall)->isTailCall()) cast<CallInst>(NewCall)->setTailCall();
	}

	if (!OldCall->use_empty()) { 
		OldCall->replaceAllUsesWith(NewCall);
		NewCall->takeName(OldCall);
	}
	/* delete the OldCall's join function */
	MDNode *gpuMDNode = OldCall->getMetadata("join_gpu");
	MDString *gpuMDString; 
	if (gpuMDNode != NULL&& 
		(gpuMDString = dyn_cast<MDString>(gpuMDNode->getOperand(0)))) {
		Function *gpu_join_func = NF->getParent()->getFunction(gpuMDString->getString().str().c_str());
		if(gpu_join_func != NULL)
			gpu_join_func->eraseFromParent();
	}
	/* Now delet the old call */
	OldCall->eraseFromParent();
	
#ifdef HETERO_GCD_HAS_BLOCK_COPY
	if (copiedBlockPtr) {
		SmallVector<Value*, 16> BlockArgs;
		BlockArgs.push_back(copiedBlockPtr);
		BasicBlock::iterator it(NewCall); it++;
		if (it != NewCall->getParent()->end())
			CallInst::Create(Block_release_func, BlockArgs.begin(), BlockArgs.end(), "", &(*it));
		else 
			CallInst::Create(Block_release_func, BlockArgs.begin(), BlockArgs.end(), "", NewCall->getParent());
	}
#endif
}

/* read a ghal file */
string HeteroCPUTransform::read_text_file(string filename) {
	string ret="";
	string line;
	ifstream textfile (filename.c_str());
	if (textfile.is_open()){
		while (! textfile.eof()){
			getline (textfile,line);
			ret += line;
			ret.erase(remove(ret.begin(), ret.end(), '\t'), ret.end()); 
			ret += "\n";
		}
		textfile.close();
	}
	return ret;
}


void HeteroCPUTransform::extract_type_info(Module &M) {
	intArgTy = NULL;
	contextPtrTy = NULL;
	funcApplyFPtrTy = NULL;
	int type=0;
#ifdef HETERO_GCD_H
	heteroQueuePtrTy = NULL;
	funcApplyPtrTy = NULL;
#endif
	Function *offload_func = M.getFunction(OFFLOAD_NAME);
	if (offload_func != NULL) {
		for (Value::use_iterator i = offload_func->use_begin(), e = offload_func->use_end(); i != e; ++i) {
			Instruction *call;
			if ((call = dyn_cast<InvokeInst>(*i)) || (call = dyn_cast<CallInst>(*i))) {
				CallSite CI(cast<Instruction>(call));
				int type;
				Function *hetero_function = get_hetero_function_f(M, CI, &type);
				if (hetero_function != NULL) {
							if (funcApplyFPtrTy == NULL) {
								intArgTy = (CI.getArgument(0))->getType();
#ifdef HETERO_GCD_H
								heteroQueuePtrTy = cast<PointerType>((CI.getArgument(1))->getType()); 
								funcApplyFPtrTy = cast<PointerType>((CI.getArgument(3))->getType());
								contextPtrTy = cast<PointerType>((CI.getArgument(2))->getType());
#else 
								funcApplyFPtrTy = cast<PointerType>((CI.getArgument(2))->getType());
								contextPtrTy = cast<PointerType>((CI.getArgument(1))->getType());
								gContextPtrTy = cast<PointerType>((CI.getArgument(3))->getType());
#endif
							}
							entry_hetero_function_set.insert(hetero_function);
							insn2FuncMap.insert(pair<Function *, Instruction *>(hetero_function, call));
							BasicBlock *bb = call->getParent();
							Function *FF = bb->getParent();
							LI = &getAnalysis<LoopInfo>(*FF);
							unsigned int loop_depth = LI->getLoopDepth(bb);
							map<Function *, int>::iterator iterr = kernel2numinvokeMap.find(hetero_function);
							int count = loop_depth;
							if (iterr != kernel2numinvokeMap.end()) 
								count += (*iterr).second;
							kernel2numinvokeMap.insert(pair<Function *, int>(hetero_function, count));
						}
#ifdef HETERO_GCD_H
						else if(hetero_function = get_hetero_function(M, CI)) {
							if (funcApplyPtrTy == NULL) {
								intArgTy = (CI.getArgument(0))->getType();
								heteroQueuePtrTy = cast<PointerType>((CI.getArgument(1))->getType()); 
								funcApplyPtrTy = cast<PointerType>((CI.getArgument(2))->getType());
							}
							entry_hetero_function_set.insert(hetero_function);
							insn2BlockMap.insert(pair<Function *, Instruction *>(hetero_function, INSNI));
						}
#endif
			}
		}
	}
#if 0
	// Collect initial set of hetero functions
	for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I){
		if (!I->isDeclaration()) {
			for (Function::iterator BBI = I->begin(), BBE = I->end(); BBI != BBE; ++BBI) {
				for (BasicBlock::iterator INSNI = BBI->begin(), INSNE = BBI->end(); INSNI != INSNE; ++INSNI) {
					if (isa<CallInst>(INSNI) || isa<InvokeInst>(INSNI)) {
						CallSite CI(cast<Instruction>(INSNI));
						Function *hetero_function;
						if (CI.getCalledFunction() == NULL) continue;
						if (hetero_function = get_hetero_function_f(M, CI,&type)) {
	
							if (funcApplyFPtrTy == NULL) {
								intArgTy = (CI.getArgument(0))->getType();
#ifdef HETERO_GCD_H
								heteroQueuePtrTy = cast<PointerType>((CI.getArgument(1))->getType()); 
								funcApplyFPtrTy = cast<PointerType>((CI.getArgument(3))->getType());
								contextPtrTy = cast<PointerType>((CI.getArgument(2))->getType());
#else 
								funcApplyFPtrTy = cast<PointerType>((CI.getArgument(2))->getType());
								contextPtrTy = cast<PointerType>((CI.getArgument(1))->getType());
#endif
							}
							entry_hetero_function_set.insert(hetero_function);
							insn2FuncMap.insert(pair<Function *, Instruction *>(hetero_function, INSNI));
						}
#ifdef HETERO_GCD_H
						else if(hetero_function = get_hetero_function(M, CI)) {
							if (funcApplyPtrTy == NULL) {
								intArgTy = (CI.getArgument(0))->getType();
								heteroQueuePtrTy = cast<PointerType>((CI.getArgument(1))->getType()); 
								funcApplyPtrTy = cast<PointerType>((CI.getArgument(2))->getType());
							}
							entry_hetero_function_set.insert(hetero_function);
							insn2BlockMap.insert(pair<Function *, Instruction *>(hetero_function, INSNI));
						}
#endif
					}
				}
			}
		}
	}
#endif
}


void HeteroCPUTransform::extract_join_info(Module &M) {
	Function *offload_func = M.getFunction(OFFLOAD_NAME);
	if (offload_func != NULL) {
		for (Value::use_iterator i = offload_func->use_begin(), e = offload_func->use_end(); i != e; ++i) {
			Instruction *call;
			if ((call = dyn_cast<InvokeInst>(*i)) || (call = dyn_cast<CallInst>(*i))) {
				CallSite CI(cast<Instruction>(call));
				int type;
				Function *kernel = get_hetero_function_f(M, CI, &type);
				MDNode *cpuMDNode = call->getMetadata("join_cpu");
				MDNode *gpuMDNode = call->getMetadata("join_gpu");
				MDNode *sizeMDNode = call->getMetadata("object_size");
				MDNode *schedulerhintMDNode = call->getMetadata("scheduler_hint");
				MDString *cpuMDString, *gpuMDString, *sizeMDString, *schedulerhintMDString; 
				if (cpuMDNode != NULL && gpuMDNode != NULL && sizeMDNode != NULL &&
					(cpuMDString = dyn_cast<MDString>(cpuMDNode->getOperand(0))) && 
					(gpuMDString = dyn_cast<MDString>(gpuMDNode->getOperand(0))) &&
					(sizeMDString = dyn_cast<MDString>(sizeMDNode->getOperand(0)))) {					
					Function *cpu_join_func = (Function *)(M.getFunction(cpuMDString->getString()/*.str().c_str()*/));
					Function *gpu_join_func = (Function *)(M.getFunction(gpuMDString->getString()/*.str().c_str()*/));
					if (cpu_join_func != NULL && gpu_join_func != NULL) {
						int size = atoi(sizeMDString->getString().str().c_str());
						kernel2gpujoinMap.insert(pair<Function *, Function *>(kernel, gpu_join_func));
						kernel2cpujoinMap.insert(pair<Function *, Function *>(kernel, cpu_join_func));
						kernel2sizeMap.insert(pair<Function *, int>(kernel, size));
					}

				}
				if (schedulerhintMDNode != NULL && (schedulerhintMDString = dyn_cast<MDString>(schedulerhintMDNode->getOperand(0)))) {
					int sched_opt = atoi(schedulerhintMDString->getString().str().c_str());
					insn2schedulerhintMap.insert(pair<Instruction *, int>(call, sched_opt));
				}
			} 
		}
	}
}


bool HeteroCPUTransform::runOnModule(Module &M) {
	bool localChange = false;
	
	GlobalVariable *annot[2] ={ M.getGlobalVariable("hetero.string.annotations"),M.getGlobalVariable("reduce_hetero.string.annotations")};
	for(int an=0;an<2;an++){
		if (annot[an] != NULL) {

#ifdef OLD_CODE
			ConstantStruct *Struct = dyn_cast<ConstantStruct>(annot[an]->getInitializer());

			for (unsigned i = 0, e = Struct->getType()->getNumElements(); i != e; ++i) {
				ConstantStruct *element = cast<ConstantStruct>(Struct->getOperand(i));
				ConstantDataArray *Array = dyn_cast<ConstantDataArray>(element->getOperand(0));

				string str= Array->getAsString();
				//Some how getAsString puts an extraneous ' '(space) in the end of the string.
				//This is causing a problem in M.getFunction()
				//Removing the last space
				//	string func_name= str.substr(0,str.length()-1);
				if(str.length()==0){
					DEBUG(dbgs()<<"Error in function name extraction \n");
					return false;
				}
				//			DEBUG(dbgs()<<func_name<<"\n");
				Function *hetero_f=M.getFunction(str.c_str());
				if(hetero_f==NULL){
					DEBUG(dbgs()<<"ERROR in getFunction from module"<<"\n");
					return false;
				}
				DEBUG(dbgs()<<"Hetro_funcs:"<<hetero_f->getName()<<"\n");
				hetero_function_set.insert(hetero_f);
			}
#endif
			if (annot != NULL) {
				ConstantDataArray *Array = dyn_cast<ConstantDataArray>(annot[an]->getInitializer());
				//Array->dump();
				istringstream str(Array->getAsString());
				vector<string> hetero_f_s;
				copy(istream_iterator<string>(str),
						istream_iterator<string>(),
						back_inserter<vector<string> >(hetero_f_s));
				for (vector<string>::iterator it = hetero_f_s.begin(), ie = hetero_f_s.end(); it != ie; it++) {
					Function *hetero_f = M.getFunction(*it);
					if (hetero_f != NULL) {
						DEBUG(dbgs()<<"Hetro_funcs:"<<hetero_f->getName()<<"\n");
						hetero_function_set.insert(hetero_f);
					}
				}
			}	
			/* Delete the annotation and create a new one for backend */
			annot[an]->eraseFromParent();
		}
	}
	extract_type_info(M);
	extract_join_info(M);
													
	if (!entry_hetero_function_set.empty()) {

		string ghal = read_text_file(GHAL_FILE_NAME);
		string gpu_opencl = read_text_file(GPU_OPENCL_FILE_NAME);
		string cpu_opencl = read_text_file(CPU_OPENCL_FILE_NAME);
		//DEBUG(dbgs()<<"\n*****GHAL**********\n"<<ghal<<"found \n");
		//DEBUG(dbgs()<<"\n*****OPENCL**********\n"<<opencl<<"found \n");
		localChange = true;

		// Add init annotations for program and programIr 
		if(gpu_opencl.empty())
			gen_init_globals(M, ghal, cpu_opencl);
		else 
			gen_init_globals(M, gpu_opencl, cpu_opencl);

		// for each hetero function create a new function prototype and add the function
		for (set<Function *>::iterator it = entry_hetero_function_set.begin(), itt=entry_hetero_function_set.end(); 
			it != itt; it++) {
				Function *F = *it;
				if (hetero_function_set.find(F) == hetero_function_set.end()) {
					// potentiaal bug since gpuCodeis not defined which is used in handleCall -- TODO
					Constant *gpuNull = ConstantExpr::getNullValue(PointerType::get(/*Type::getInt8Ty(F->getContext())*/gpuFuncTy, 0));
					handleCall(F, gpuNull);
					// TODO need to handle graceful shutdown for join functions
				}
				else{ 
					gen_globals_transform_per_f (F);
				}
		}
		//M.dump();
	}

	/* delete all join_gpu functions */
	/*Function *offload_func = M.getFunction(OFFLOAD_RUNTIME_NAME);
	if (offload_func != NULL) {
		for (Value::use_iterator i = offload_func->use_begin(), e = offload_func->use_end(); i != e; ++i) {
			Instruction *call;
			if ((call = dyn_cast<InvokeInst>(*i)) || (call = dyn_cast<CallInst>(*i))) {
				CallSite CI(cast<Instruction>(call));
				MDNode *gpuMDNode = call->getMetadata("join_gpu");
				MDString *gpuMDString; 
				if (gpuMDNode != NULL&& 
					(gpuMDString = dyn_cast<MDString>(gpuMDNode->getOperand(0)))
					) {
					Function *gpu_join_func = M.getFunction(gpuMDString->getString().str().c_str());
					if(gpu_join_func != NULL)
					gpu_join_func->eraseFromParent();
				}
			}
		}
	}*/
	return localChange;
}

