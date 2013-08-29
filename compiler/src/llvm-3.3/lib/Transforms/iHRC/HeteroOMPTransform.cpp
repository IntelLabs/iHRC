//===-- HeteroOMPTransform.cpp - Analyze OpenMP code    -------------------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#include "llvm/Transforms/iHRC/HeteroOMPTransform.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"
using namespace llvm;


/* Rewrite an instruction based on the mapping on ValueMap */
void HeteroOMPTransform::rewrite_instruction(Instruction *OldInst, Instruction *NewInst, DenseMap<const Value *, Value *>& ValueMap) {
	int opIdx = 0;
	for (User::op_iterator i = OldInst->op_begin(), e = OldInst->op_end(); i != e; ++i, opIdx++) {
		Value *V = *i;
		if (ValueMap[V] != NULL) {
			NewInst->setOperand(opIdx, ValueMap[V]);
		}
	}
}

/**
 * Generate code for
 */
void HeteroOMPTransform::gen_code_per_f (Function* NF, Function* F, Instruction *max_threads){
	
	Function::arg_iterator FI = F->arg_begin();
	Argument *ctxname = &*FI;

	Function::arg_iterator DestI = NF->arg_begin();
	DestI->setName(ctxname->getName()); 
	Argument *ctx_name = &(*DestI);
	DestI++;
	DestI->setName("tid");
	Argument *num_iters = &(*DestI);

#ifdef EXPLICIT_REWRITE
	DenseMap<const Value*, Value *> ValueMap;
#else
	ValueToValueMapTy ValueMap;
#endif

	//get the old basic block and create a new one
	Function::const_iterator BI = F->begin();
	const BasicBlock &FB = *BI;
	BasicBlock *NFBB = BasicBlock::Create(FB.getContext(), "", NF);
	if (FB.hasName()){
		NFBB->setName(FB.getName());
	}
	ValueMap[&FB] = NFBB;

	//ValueMap[numiters] = num_iters;
	ValueMap[ctxname] = ctx_name;

#if EXPLICIT_REWRITE
	for (BasicBlock::const_iterator II = FB.begin(), IE = FB.end(); II != IE; ++II) {
		Instruction *NFInst = II->clone(/*F->getContext()*/);
		//	DEBUG(dbgs()<<*II<<"\n");
		if (II->hasName()) NFInst->setName(II->getName());
		const Instruction *FInst = &(*II);
		rewrite_instruction((Instruction *)FInst, NFInst, ValueMap);
		NFBB->getInstList().push_back(NFInst);
		ValueMap[II] = NFInst;
	}
	BI++;

	for (Function::const_iterator /*BI=F->begin(),*/BE = F->end();BI != BE; ++BI) {
		const BasicBlock &FBB = *BI;
		BasicBlock *NFBB = BasicBlock::Create(FBB.getContext(), "", NF);
		ValueMap[&FBB] = NFBB;
		if (FBB.hasName()){
			NFBB->setName(FBB.getName());
			//DEBUG(dbgs()<<NFBB->getName()<<"\n");
		}
		for (BasicBlock::const_iterator II = FBB.begin(), IE = FBB.end(); II != IE; ++II) {
			Instruction *NFInst = II->clone(/*F->getContext()*/);
			if (II->hasName()) NFInst->setName(II->getName());
			const Instruction *FInst = &(*II);
			rewrite_instruction((Instruction *)FInst, NFInst, ValueMap);
			NFBB->getInstList().push_back(NFInst);
			ValueMap[II] = NFInst;
		}
	}
	// Remap the instructions again to take care of forward jumps
	for (Function::iterator BB = NF->begin(), BE=NF->end(); BB != BE; ++ BB) {
		for (BasicBlock::iterator II = BB->begin(); II != BB->end(); ++II){
			int opIdx = 0;
			//DEBUG(dbgs()<<*II<<"\n");
			for (User::op_iterator i = II->op_begin(), e = II->op_end(); i != e; ++i, opIdx++) {
				Value *V = *i;
				if (ValueMap[V] != NULL) {
					II->setOperand(opIdx, ValueMap[V]);
				}
			}
		}
	}
#else
	SmallVector<ReturnInst*, 8> Returns;  // Ignore returns cloned.
	CloneFunctionInto(NF, F, ValueMap, false, Returns, "");
#endif

	//max_threads->dump();
	/* Remap openmp omp_num_threads() and omp_thread_num() */ 
	/*
	 * define internal void @_Z20initialize_variablesiPfS_.omp_fn.4(i8* nocapture %.omp_data_i) nounwind ssp {
     * entry:
     * %0 = bitcast i8* %.omp_data_i to i32*           ; <i32*> [#uses=1]
     * %1 = load i32* %0, align 8                      ; <i32> [#uses=2]
     * %2 = tail call i32 @omp_get_num_threads() nounwind readnone ; <i32> [#uses=2]
     * %3 = tail call i32 @omp_get_thread_num() nounwind readnone ; <i32> [#uses=2]
	   %4 = sdiv i32 %1, %2
	   %5 = mul nsw i32 %4, %2
       %6 = icmp ne i32 %5, %1
       %7 = zext i1 %6 to i32
	 */
	vector<Instruction *> toDelete;
	for (Function::iterator BB = NF->begin(), BE=NF->end(); BB != BE; ++ BB) {
		for (BasicBlock::iterator II = BB->begin(); II != BB->end(); ++II){
			if (isa<CallInst>(II)) {
				CallSite CI(cast<Instruction>(II));
				if (CI.getCalledFunction() != NULL){ 
					string called_func_name = CI.getCalledFunction()->getName();
					if (called_func_name == OMP_GET_NUM_THREADS_NAME && CI.arg_size() == 0) {
						II->replaceAllUsesWith(ValueMap[max_threads]);
						toDelete.push_back(II);
					}
					else if (called_func_name == OMP_GET_THREAD_NUM_NAME && CI.arg_size() == 0) {
						II->replaceAllUsesWith(num_iters);
						toDelete.push_back(II);
					}
				}
			}
		}
	}


	/* Delete the last branch instruction of the first basic block -- Assuming it is safe */
	Function::iterator nfBB = NF->begin();
	TerminatorInst *lastI = nfBB->getTerminator();
	BranchInst *bI;
	BasicBlock *returnBlock;
	if ((bI = dyn_cast<BranchInst>(lastI)) && bI->isConditional() && 
		(returnBlock = bI->getSuccessor(1)) && 
		(returnBlock->getName() == "return")) {
		/* modify to a unconditional branch to next basic block and not return */
		Instruction *bbI = BranchInst::Create(bI->getSuccessor(0),lastI);
		bbI->dump();
		toDelete.push_back(lastI);
	}

	//NF->dump();
	while(!toDelete.empty()) {
		Instruction *g = toDelete.back();
		//g->replaceAllUsesWith(UndefValue::get(g->getType()));
		toDelete.pop_back();
		g->eraseFromParent();
	}

	//NF->dump();
}



/**
 * Convert the function F into kernel function with desired signature (char *, size_t)
 */
Function *HeteroOMPTransform::convert_to_kernel_function(Module &M, Function *F) {
	//F->dump();
	//printf("Convert Function= %s to a kernel function\n", F->getName());

	if (oldF2newF[F] != NULL) return oldF2newF[F];

	/* See if you can find max_threads from the kernel code */
	Function::arg_iterator FI = F->arg_begin();
	Argument *ctx_name = &*FI;
	//printf("Context Name="); ctx_name->dump();
	LoadInst *max_threads = NULL;
	for (Value::use_iterator kI = ctx_name->use_begin(), kE = ctx_name->use_end(); kI != kE; ++kI) {
		Instruction *insn = dyn_cast<Instruction>(*kI);
		CastInst *BCI;
		//insn->dump();
		if (BCI = dyn_cast<BitCastInst>(insn)) {
			BasicBlock::iterator iI(insn);
			iI++;
			if (iI != insn->getParent()->end()) {
				max_threads = dyn_cast<LoadInst>(iI);
				if (max_threads == NULL) return NULL;
				else break;
			}else return NULL;
		}
	}
	//printf("MAX_THREAD = %s\n", max_threads);
	//max_threads->dump();

	vector</*const*/ Type *> params;
	const FunctionType *FTy = F->getFunctionType();
	params.push_back(PointerType::get(Type::getInt8Ty(M.getContext()),0));
//#ifndef IVB_64
	params.push_back(Type::getInt32Ty(M.getContext()));
//#else
//	params.push_back(Type::getInt64Ty(M.getContext()));
//#endif
	/*const*/ Type *RetTy = FTy->getReturnType();
	FunctionType *NFty = FunctionType::get(RetTy,params, false);
	Function *NF = Function::Create(NFty, F->getLinkage(), /*F->getName()+"_hetero"*/ "omp_kernel");
	NF->copyAttributesFrom(F);
	gen_code_per_f(NF,F, max_threads);
	F->getParent()->getFunctionList().insert(F, NF);
	oldF2newF[F] = NF;

	DEBUG(dbgs()<<"Function F=\n" + F->getName() + 
		"\nis successfully converted to \nNewF=" + NF->getName());
										
	//NF->dump();
	return NF;
}

/*
 * Very sloppy implementation for quick prototyping
 * // TODO Assumption is that the first field contains the number of iterations -- if not, then modify source for now
 */
Value *HeteroOMPTransform::find_loop_upper_bound(Value *context) {

	// TODO Assumption is that the first field contains the number of iterations -- if not, then modify source for now
	for (Value::use_iterator i = context->use_begin(), e = context->use_end(); i != e; ++i) {
		Instruction *insn = dyn_cast<Instruction>(*i);
		GetElementPtrInst *GEP; StoreInst *SI;
		if ((GEP = dyn_cast<GetElementPtrInst>(insn)) && 
			isa<ConstantInt>(GEP->getOperand(2)) && 
			((cast<ConstantInt>(GEP->getOperand(2)))->equalsInt(0))) { /// README:NOTE THE ASSUMPTION THAT THE FIRST ELEMENT IN THE CONTEXT IS MAX ITERATION OF PARALLEL LOOP
				for (Value::use_iterator I = insn->use_begin(), E = insn->use_end(); I != E; ++I) {
					if ((SI = dyn_cast<StoreInst>(*I))) { 
						Value *op_0 = SI->getOperand(0);
						return op_0;
					}
				}
		}
	}
	return NULL;
}

/*
 * Rewrite OpenMP call sites and their associated kernel functions  -- the folloiwng pattern
   call void @GOMP_parallel_start(void (i8*)* @_Z20initialize_variablesiPfS_.omp_fn.4, i8* %.omp_data_o.5571, i32 0) nounwind
  call void @_Z20initialize_variablesiPfS_.omp_fn.4(i8* %.omp_data_o.5571) nounwind
  call void @GOMP_parallel_end() nounwind
 */
void HeteroOMPTransform::rewrite_omp_call_sites(Module &M) {
	SmallVector<Instruction *, 16> toDelete;
	DenseMap<Value *, Value *> ValueMap;
	
	for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I){
		if (!I->isDeclaration()) {
			
			for (Function::iterator BBI = I->begin(), BBE = I->end(); BBI != BBE; ++BBI) {
				bool match = false;
				for (BasicBlock::iterator INSNI = BBI->begin(), INSNE = BBI->end(); INSNI != INSNE; ++INSNI) {
					if (isa<CallInst>(INSNI)) {
						CallSite CI(cast<Instruction>(INSNI));
						if (CI.getCalledFunction() != NULL){ 
							string called_func_name = CI.getCalledFunction()->getName();
							if (called_func_name == OMP_PARALLEL_START_NAME && CI.arg_size() == 3) {
								// change alloc to malloc_shared
								// %5 = call i8* @_Z13malloc_sharedm(i64 20)       ; <i8*> [#uses=5]
								// %6 = bitcast i8* %5 to float*                   ; <float*> [#uses=2]
								AllocaInst *AllocCall;
								Value *arg_0 = CI.getArgument(0); // function
								Value *arg_1 = CI.getArgument(1);  // context
								Value *loop_ub = NULL;
								Function *function;
								BitCastInst* BCI;
								Function *kernel_function;
								BasicBlock::iterator iI(*INSNI); 
								//BasicBlock::iterator iJ = iI+1; 
								iI++; iI++;
								//BasicBlock::iterator iK = iI;  
								CallInst /**next,*/ *next_next; 
								if (arg_0 != NULL && arg_1 != NULL /*&& (next = dyn_cast<CallInst>(*iJ))*/ 
									&& (next_next = dyn_cast<CallInst>(iI)) && (next_next->getCalledFunction() != NULL) 
									&& (next_next->getCalledFunction()->getName() == OMP_PARALLEL_END_NAME)
									&& (BCI = dyn_cast<BitCastInst>(arg_1)) && (AllocCall = dyn_cast<AllocaInst>(BCI->getOperand(0))) 
									&& (function = dyn_cast<Function>(arg_0)) && (loop_ub = find_loop_upper_bound (AllocCall)) 
									&& (kernel_function=convert_to_kernel_function (M, function))){
									
										SmallVector<Value*, 16> Args;
										Args.push_back(AllocCall->getArraySize());
										Instruction *MallocCall = CallInst::Create(mallocFnTy, Args, "", AllocCall);
										CastInst *MallocCast = CastInst::Create(Instruction::BitCast, MallocCall, AllocCall->getType(), "", AllocCall);
										ValueMap[AllocCall] = MallocCast;
										//AllocCall->replaceAllUsesWith(MallocCall);
										// Add offload function
										Args.clear();
										Args.push_back(loop_ub);
										Args.push_back(BCI);
										Args.push_back(kernel_function);
										if (offloadFnTy == NULL) {
											init_offload_type(M, kernel_function);
										}
										
										Instruction *call = CallInst::Create(offloadFnTy, Args, "", INSNI);
										
										if (find(toDelete.begin(), toDelete.end(), AllocCall) == toDelete.end()){
											toDelete.push_back(AllocCall);
										}
										toDelete.push_back(&(*INSNI));
										match = true;
								}
							}
							else if (called_func_name == OMP_PARALLEL_END_NAME && CI.arg_size() == 0 && match) {
								toDelete.push_back(&(*INSNI));
								match = false;
							}
							else if (match) {
								toDelete.push_back(&(*INSNI));
							}
						}
					}
				}
			}
		}

	}

	/* Replace AllocCalls by MallocCalls */
	for(DenseMap<Value *, Value *>::iterator I = ValueMap.begin(), E = ValueMap.end(); I != E; I++) {
		I->first->replaceAllUsesWith(I->second);
	}

	/* delete the instructions for get_omp_num_thread and get_omp_thread_num */
	while(!toDelete.empty()) {
		Instruction *g = toDelete.back();
		toDelete.pop_back();
		g->eraseFromParent();
	}

}

/* Create the signature for offload function */
void HeteroOMPTransform::init_offload_type(Module &M, Function *F) {	
	vector</*const*/ Type *> params;
//#ifndef IVB_64
	params.push_back(Type::getInt32Ty(M.getContext()));
//#else
//	params.push_back(Type::getInt64Ty(M.getContext()));
//#endif
	params.push_back(PointerType::get(Type::getInt8Ty(M.getContext()),0));
	params.push_back(F->getType());
	/*const*/ Type *RetTy = F->getFunctionType()->getReturnType();
	FunctionType *NFty = FunctionType::get(RetTy, params, false);
	offloadFnTy = M.getOrInsertFunction("offload", NFty);
}

/* create the signature for malloc_shared */
void HeteroOMPTransform::init_malloc_shared_type(Module &M) {
	vector</*const*/ Type *> params;
//#ifndef IVB_64
	params.push_back(Type::getInt32Ty(M.getContext()));
//#else
//	params.push_back(Type::getInt64Ty(M.getContext()));
//#endif
	/*const*/ Type *RetTy = PointerType::get(Type::getInt8Ty(M.getContext()),0);
	FunctionType *NFty = FunctionType::get(RetTy, params, false);
	mallocFnTy = M.getOrInsertFunction("scalable_malloc", NFty);
}

/* run on every module */
bool HeteroOMPTransform::runOnModule(Module &M) {
	bool localChange = true;
	init_malloc_shared_type(M);
	offloadFnTy = NULL;
	rewrite_omp_call_sites(M);
	return localChange;
}

