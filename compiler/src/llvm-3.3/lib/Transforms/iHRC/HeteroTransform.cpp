//===-- HeteroTransform.cpp - Check if GPU compatible    -------------------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#include "llvm/Transforms/iHRC/HeteroTransform.h"
#include <iostream>
#include <fstream>
#include <llvm/IR/LLVMContext.h>


void HeteroTransform::get_hetero_functions(Module &M,int par_type) {
	// Collect initial set of hetero functions
//	DEBUG(dbgs()<<"Hello:");
	int type=0;
for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I){
		if (!I->isDeclaration()) {
			for (Function::iterator BBI = I->begin(), BBE = I->end(); BBI != BBE; ++BBI) {
				for (BasicBlock::iterator INSNI = BBI->begin(), INSNE = BBI->end(); INSNI != INSNE; ++INSNI) {
					if (isa<CallInst>(INSNI) || isa<InvokeInst>(INSNI)) {
						CallSite CI(cast<Instruction>(INSNI));
						Function *hetero_function;
						//DEBUG(dbgs()<<"Caller"<<INSNI->getName()<<"\n");
						if (CI.getCalledFunction() == NULL) continue;
						if (hetero_function = get_hetero_function_f(M, CI,&type)) {
							if(type==par_type){//type is 1 for parallel_for and 2 for parallel_reduce
								DEBUG(dbgs()<<"Type "<<par_type<<":Func :"<<hetero_function->getName()<<"\n");
								entry_hetero_function_set.insert(hetero_function);
								if(type==2){//add the join function as well
									MDNode *node = INSNI->getMetadata("join_gpu");
									if(!node)DEBUG(dbgs()<<"Has no join function\n");
									else{
										//node->getOperand(0)->dump();
										MDString *str=cast<MDString>(node->getOperand(0));
										DEBUG(dbgs()<<"Adding "<<str->getString()<<"\n");
										entry_join_function_set.insert(M.getFunction(str->getString()));
									}
								}
							}
						}
#ifdef HETERO_GCD_H
						else if(hetero_function = get_hetero_function(M, CI)) {
							DEBUG(dbgs()<<"Func"<<hetero_function->getName()<<"\n");
							entry_hetero_function_set.insert(hetero_function);
#ifdef HETERO_GCD_ALLOC_TO_MALLOC
							block2insnMap.insert(pair<Function *, Instruction *>(hetero_function, INSNI));
#endif
						}
#endif
					}
				}
			}
		}
	}
}

#ifdef HETERO_GCD_ALLOC_TO_MALLOC
/* Rewrite alloca to malloc */
void HeteroTransform::rewrite_alloca(CallSite &CS) {
	Instruction *OldCall = CS.getInstruction();
	OldCall->dump();
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
			ReplaceInstWithInst(context->getParent()->getInstList(), ii, 
				new MallocInst(context->getAllocatedType(), 
				context->getArraySize(), context->getAlignment(), ""));
			context1=&(*ii);
			for (Value::use_iterator i = context1->use_begin(), e = context1->use_end(); i != e; ++i) {
				Instruction *insn = dyn_cast<Instruction>(*i);
				GetElementPtrInst *GEP, *GEP1;
				LoadInst *LI;
				CastInst *BC;
				if ((GEP = dyn_cast<GetElementPtrInst>(insn)) && 
					isa<ConstantInt>(GEP->getOperand(2)) && 
					((cast<ConstantInt>(GEP->getOperand(2)))->uge(5))) {
						for (Value::use_iterator I = insn->use_begin(), E = insn->use_end(); I != E; ++I) {
							if ((SI = dyn_cast<StoreInst>(*I)) && 
								/* store is storing something that comes from alloca */
								((alloc = dyn_cast<AllocaInst>(SI->getOperand(0))) ||
								/* store is storing something from a load instruction */
								(((LI = dyn_cast<LoadInst>(SI->getOperand(0))) || 
								/* store is storing something from a bitcast instruction that comes from load and which leads to store */
								  ((BC = dyn_cast<CastInst>(SI->getOperand(0))) && 
								   (LI = dyn_cast<LoadInst>(BC->getOperand(0))) )) && 
								 (GEP1 = dyn_cast<GetElementPtrInst>(LI->getPointerOperand())) &&
								 (alloc = dyn_cast<AllocaInst>(GEP1->getPointerOperand()))))){ 
								BasicBlock::iterator iii(alloc);
								ReplaceInstWithInst(alloc->getParent()->getInstList(), iii, 
									new MallocInst(alloc->getAllocatedType(), 
									alloc->getArraySize(), alloc->getAlignment(), ""));
							}
						}
				}
			}
	}
}
#endif

Instruction *HeteroTransform::rewriteCallSite(Instruction *OldCall, Function *NF) {
	CallSite CS(OldCall);
	Instruction *NewCall;
	SmallVector<Value*, 16> Args; // Argument lists to the new call
	SmallVector<AttributeSet, 8> AttrVec;
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
		AttrVec.push_back(AttributeSet::get(NF->getContext(),
                OldCallPAL.getRetAttributes()));

	CallSite::arg_iterator AI = CS.arg_begin();
	unsigned Index = 1;
	for (CallSite::arg_iterator I = CS.arg_begin(), E = CS.arg_end(); I != E; ++I, ++AI, ++Index) {
		Args.push_back(*AI);          // copy the old arguments
		AttributeSet Attrs = OldCallPAL.getParamAttributes(Index);
		if (Attrs.hasAttributes(Index)) {
			AttrBuilder B(Attrs, Index);
			AttrVec.push_back(AttributeSet::get(NF->getContext(), Args.size(), B));
		}
#if defined(LLVM_3_2)
		Attributes attrs = OldCallPAL.getParamAttributes(Index);
		if (attrs.hasAttributes())
			AttrVec.push_back(AttributeWithIndex::get(Args.size(), attrs));
#endif
	}
	if (InvokeInst *invoke = dyn_cast<InvokeInst>(OldCall)) {
		//NF->dump();
		//NewCall = InvokeInst::Create(NF, invoke->getNormalDest(), invoke->getUnwindDest(), Args.begin(), Args.end(), "", OldCall);
		//cast<InvokeInst>(NewCall)->setCallingConv(CS.getCallingConv());
		//cast<InvokeInst>(NewCall)->setAttributes(AttrListPtr::get(AttrVec.begin(), AttrVec.end()));
		NewCall = CallInst::Create(NF, Args, "", OldCall);
		cast<CallInst>(NewCall)->setCallingConv(CS.getCallingConv());
#if defined(LLVM_3_2)
		cast<CallInst>(NewCall)->setAttributes(AttrListPtr::get(NF->getContext(), AttrVec));
#endif
		cast<CallInst>(NewCall)->setAttributes(AttributeSet::get(NF->getContext(), AttrVec));
		//if (cast<CallInst>(OldCall)->isTailCall()) cast<CallInst>(NewCall)->setTailCall();
	} else {
		NewCall = CallInst::Create(NF, Args, "", OldCall);
		cast<CallInst>(NewCall)->setCallingConv(CS.getCallingConv());
#if defined(LLVM_3_2)
		cast<CallInst>(NewCall)->setAttributes(AttrListPtr::get(NF->getContext(), AttrVec));
#endif
		cast<CallInst>(NewCall)->setAttributes(AttributeSet::get(NF->getContext(), AttrVec));
		if (cast<CallInst>(OldCall)->isTailCall()) cast<CallInst>(NewCall)->setTailCall();
	}

	/*DEBUG(dbgs() << "Old Call=");
	OldCall->dump();
	DEBUG(dbgs() << "New Call=");
	NewCall->dump();*/
	return NewCall;
}


Function *HeteroTransform::createNewFunctionDeclaration(Function *F, string name) {
	Module *M = F->getParent();
	Function *NF;
	if (NF = M->getFunction(name)) {
		return NF;
	}
	else {
		const FunctionType *FTy = F->getFunctionType();
		/*const*/ Type *RetTy = FTy->getReturnType();
		if (RetTy->getTypeID() == Type::DoubleTyID) {
				errs() << "\nWARNING: Function " << F->getName() << " has double type operands.\n";
				return NULL;	
		}
		vector</*const*/ Type*> Params;
		for (Function::arg_iterator I = F->arg_begin(), E = F->arg_end(); I != E; ++I) {
			if (I->getType()->getTypeID() == Type::DoubleTyID) {
				errs() << "\nWARNING: Function " << F->getName() << " has double type operands.\n";
				return NULL;	
			}
			Params.push_back(I->getType());
		}
		FunctionType *NFTy = FunctionType::get(RetTy, Params, false);
		NF = Function::Create(NFTy, F->getLinkage(), name);
		NF->copyAttributesFrom(F);
		M->getFunctionList().insert(F, NF);
		return NF;
	}
}

/* cleanup exception blocks, rewrite 
	sqrtf   to  native_sqrt
	acosf   to  acos
	fabsf   to  fabs
	sinf	to  native_sin
	cosf	to	native_cos
	expf	to	native_exp
	logf	to  native_log
	tanf	to	native_tan
	*/
	
void HeteroTransform::cleanup_exception_builtin(Function *hetero_func) {

	std::vector<Instruction *> toDel;

	/* STEP 1: cleanup llvm.lifetime.start and llvm.lifetime.end */ 
	for (inst_iterator I = inst_begin(hetero_func), E = inst_end(hetero_func); I != E; ++I) {
		Instruction *INSNI = &*I;
		if (INSNI != NULL && isa<CallInst>(INSNI) || isa<InvokeInst>(INSNI)) {
				CallSite CI(INSNI);
				Function *fun = CI.getCalledFunction();
				DEBUG(dbgs() << "Function Called=" << fun->getName());
				if ((fun->getName().compare("llvm.lifetime.start") == 0) ||
					(fun->getName().compare("llvm.lifetime.end") == 0)) {
						toDel.push_back(INSNI);
				}
		}
	}

	deleteSet(toDel);
	
	toDel.clear();

	/* STEP 2: rewrite exp and fabs to expf and fabsf with eliminating double types around */
	std::vector<Instruction *> replace_this;
	std::vector<Instruction *> with_this;
	std::vector<BasicBlock *> toDelete;
	

	//DEBUG(dbgs() << "\nCleanup:: Function Name:" << hetero_func->getNameStr());

	map<string,string> rewrite_map;
	rewrite_map["exp"] = "expf";
	rewrite_map["fabs"] = "fabsf";

	replace_this.clear();
	with_this.clear();
	toDel.clear();

	
	map<CmpInst *, tuple_t *>  cmp2InsMap;
	cmp2InsMap.clear();
	for (Function::iterator BBI = hetero_func->begin(), BBE = hetero_func->end(); BBI != BBE; ++BBI) {
		for (BasicBlock::iterator INSNI = BBI->begin(), INSNE = BBI->end(); INSNI != INSNE; ++INSNI) {
			if (isa<CallInst>(INSNI) || isa<InvokeInst>(INSNI)) {
				CallSite CI(cast<Instruction>(INSNI));
				Function *fun = CI.getCalledFunction();
				if (isBuiltinFunction(fun)) {
					map<string,string>::iterator it = rewrite_map.find(fun->getName());
					if(it != rewrite_map.end()) {
					//for(int i=0; i< entries; i++) {
						CastInst *fpExtArgument = NULL;
						CastInst *fpTruncReturn = NULL;
						CmpInst *fpCmp = NULL;
						CastInst *fpCmpfpExt = NULL;

						/* handling a simple scenario for now: y = fpext float x to double; ...;  z = exp (double y); .... p = fptrunc double z to float */
						/* There could be complicated scenarios and there is no easy way to handle this */
						if (/*(fun->getName().compare(table[i][0])  == 0) && */
							/* one argument */ (CI.arg_size() == 1) && 
							/* argument is double type */ (CI.getArgument(0)->getType()->isDoubleTy()) && 
							/* return type is double type */ (fun->getReturnType()->isDoubleTy()) &&
							/* argument comes from fpext instruction */ (fpExtArgument = dyn_cast<CastInst>(CI.getArgument(0))) && (fpExtArgument->getOperand(0)->getType()->isFloatTy()) && (fpExtArgument->getNumUses() == 1) &&
							/* returntype goes to fptrunc */ (((INSNI->getNumUses() == 1) && 
							(((fpTruncReturn = dyn_cast<CastInst>(*(INSNI->use_begin()))) && (fpTruncReturn->getType()->isFloatTy()))  || 
							((fpCmp = dyn_cast<CmpInst>(*(INSNI->use_begin()))) && 
							(((fpCmp->getOperand(0) == INSNI) && (fpCmpfpExt = dyn_cast<CastInst>(fpCmp->getOperand(1))) && (fpCmpfpExt->getNumUses() == 1)) || 
							(((fpCmp->getOperand(1) == INSNI) && (fpCmpfpExt = dyn_cast<CastInst>(fpCmp->getOperand(0))) && (fpCmpfpExt->getNumUses() == 1) ))))
							)) || (INSNI->getNumUses() == 0))){

								//DEBUG(dbgs() << "\n\nINSN:" << *INSNI);
								//DEBUG(dbgs() << "\nfpExt:" << *fpExtArgument);
								//if (fpCmp != NULL) DEBUG(dbgs() << "\nfpCmp:" << *fpCmp);
								//if (fpTruncReturn != NULL) DEBUG(dbgs() << "\nfpTrunc:" << *fpTruncReturn);

								Instruction *NewCall;

								//if (INSNI->getNumUses() != 0) {
									Function *expf = fun->getParent()->getFunction(/*table[i][1]*/it->second);
									if (expf == NULL) {
										vector</*const*/ Type *> params;
										params.clear();
										params.push_back(Type::getFloatTy(fun->getContext()));
										FunctionType *NFTy = FunctionType::get(Type::getFloatTy(fun->getContext()), params, false);
										Constant *expf_const = fun->getParent()->getOrInsertFunction(/*table[i][1]*/it->second, NFTy);
										expf = dyn_cast<Function>(fun->getParent()->getFunction(/*table[i][1]*/it->second));
									}

									SmallVector<Value*, 1> Args; // Argument lists to the new call
									Args.push_back(fpExtArgument->getOperand(0));
									
									if (InvokeInst *invoke = dyn_cast<InvokeInst>(INSNI)) {
										NewCall = CallInst::Create(expf, Args, "", INSNI);
										BasicBlock *unwind_bb;
										if (unwind_bb = dyn_cast<BasicBlock>(invoke->getUnwindDest())) {
											BranchInst *BI = BranchInst::Create(invoke->getNormalDest(), INSNI);
										}
									}
									else {
										NewCall = CallInst::Create(expf, Args, "", INSNI);
									}
								//}
								
								//if (fpExtArgument->getNumUses() == 1)
								toDel.push_back(fpExtArgument);
								toDel.push_back(INSNI);

								if (fpTruncReturn != NULL) {
									replace_this.push_back(fpTruncReturn);
									with_this.push_back(NewCall);
								}
								else if(fpCmp != NULL) {
									map<CmpInst *, tuple_t *>::iterator itt = cmp2InsMap.find(fpCmp);
									tuple_t *pairs;
									if (itt != cmp2InsMap.end()) {
										pairs = (*itt).second;
									}
									else {
										pairs = (tuple_t *)malloc(sizeof(tuple_t));
										pairs->Op1 = NULL;
										pairs->Op2 = NULL;
									}
									if (fpCmp->getOperand(0) == INSNI) {
										pairs->Op1 = NewCall;
									}
									else {
										pairs->Op2 = NewCall;
									}
									cmp2InsMap.insert(pair<CmpInst *, tuple_t *>(fpCmp, pairs));
									//DEBUG(dbgs() << "\nFunction call::" << *INSNI);
									//DEBUG(dbgs() << "\nCompare Ins::" << *fpCmp);
									//DEBUG(dbgs() << "\nNewCall::" << *NewCall);
									//if (pairs->Op1 != NULL) DEBUG(dbgs() << "\nOp1::" << *(pairs->Op1));
									//if (pairs->Op2 != NULL) DEBUG(dbgs() << "\nOp2::" << *(pairs->Op2));
								}
								else {
									//DEBUG(dbgs() << "\n fpExt:" << *fpExtArgument);
									//DEBUG(dbgs() << "\n fpExt:" << *INSNI);
								}
								break;
						}
					//}
					}
				}
			}
		}
	}
	
	
	/* iterate over fCmpInst and replace them */
	for(map<CmpInst *, tuple_t *>::iterator it = cmp2InsMap.begin(); it != cmp2InsMap.end(); it++) {
		CmpInst *fCmp = (*it).first;
		tuple_t *pairs = (*it).second;

		//DEBUG(dbgs() << "\nOld Compare Ins::" << *fCmp);
		
		Value *Op1 = pairs->Op1;
		Value *Op2 = pairs->Op2;
		if (Op1 == NULL) {
			Instruction *oldOp1;
			if (oldOp1 = dyn_cast<CastInst>(fCmp->getOperand(0))) {
				Op1 = oldOp1->getOperand(0);
				toDel.push_back(oldOp1);
			}
		}
		else if (Op2 == NULL) {
			Instruction *oldOp2;
			if (oldOp2 = dyn_cast<CastInst>(fCmp->getOperand(1))) {
				Op2 = oldOp2->getOperand(0);
				toDel.push_back(oldOp2);
			}
		}
		//DEBUG(dbgs() << "\nOp1::" << *(Op1));
		//DEBUG(dbgs() << "\nOp2::" << *(Op2));
		CmpInst *newCmp = new FCmpInst(fCmp, CmpInst::Predicate(fCmp->getPredicate()), Op1, Op2, "");
		//DEBUG(dbgs() << "\nNew Compare Ins::" << *newCmp);
		fCmp->replaceAllUsesWith(newCmp);
		toDel.push_back(fCmp);//fCmp->eraseFromParent();
		delete pairs;
	}

	while(!replace_this.empty()) {
		Instruction *old_f = replace_this.back(); replace_this.pop_back();
		Instruction *new_f = with_this.back(); with_this.pop_back();
		old_f->replaceAllUsesWith(new_f);
		old_f->eraseFromParent();
		//toDel.push_back(old_f);
	}
	
	


	deleteSet(toDel);
	simplifyCFG(hetero_func);

	//hetero_func->dump();	
	cmp2InsMap.clear();


	/* STEP 3: Rewrite the instrinsic to correponding opencl mapping */
	map<string, string> conversion_map;
	conversion_map["sqrtf"] = "native_sqrt";
	conversion_map["acosf"] = "native_acos";
	conversion_map["fabsf"] = "native_fabs";
	conversion_map["fmodf"] = "native_fmod";
	conversion_map["sinf"] = "native_sin";
	conversion_map["cosf"] = "native_cos";
	conversion_map["expf"] = "native_exp";
	conversion_map["logf"] = "native_log";
	conversion_map["log10f"] = "native_log10";
	conversion_map["powf"] = "native_powr";
	conversion_map["native_atomic_cmpxchg"] = "native_atomic_cmpxchg";
	conversion_map["native_atomic_set"] = "native_atomic_set";
	conversion_map["native_atomic_sub"] = "native_atomic_sub";
	
	replace_this.clear();
	with_this.clear();
	toDelete.clear();
	for (Function::iterator BBI = hetero_func->begin(), BBE = hetero_func->end(); BBI != BBE; ++BBI) {
		for (BasicBlock::iterator INSNI = BBI->begin(), INSNE = BBI->end(); INSNI != INSNE; ++INSNI) {
			if (isa<CallInst>(INSNI) || isa<InvokeInst>(INSNI)) {
				CallSite CI(cast<Instruction>(INSNI));
				Function *fun = CI.getCalledFunction();
				if (isBuiltinFunction(fun)) {
					Instruction *NewInsn;
					Function *NewFuncDecl;
					//DEBUG(dbgs() << "\nNext Call Site:");
					//fun->dump();
					/* handle llvm_cpy instructions */
					if (fun->getName().substr(0,11).compare("llvm.memcpy")  == 0) {
						if ((NewFuncDecl = dyn_cast<Function>(createNewFunctionDeclaration(fun, "native_llvm_memcpy"))) &&
							(NewInsn = dyn_cast<Instruction>(rewriteCallSite(INSNI, NewFuncDecl)))) {
							replace_this.push_back(INSNI);
							with_this.push_back(NewInsn);
						}
					} 
					else {
						map<string, string>::iterator it = conversion_map.find(fun->getName());
						if (it != conversion_map.end()) {
						//for(int i=0; i<num_of_entries; i++) {
						//	if((fun->getName().compare(conversion_table[i][0]) == 0)) { 
							if ((NewFuncDecl = dyn_cast<Function>(createNewFunctionDeclaration(fun, it->second/*conversion_table[i][1]*/))) &&
									(NewInsn = dyn_cast<Instruction>(rewriteCallSite(INSNI, NewFuncDecl)))) {
									//DEBUG(dbgs() << "\nOld Builtin Call insn:");
									//INSNI->dump();
									//DEBUG(dbgs() << "\nNew Builtin Call insn:");
									//NewInsn->dump();
									replace_this.push_back(INSNI);
									with_this.push_back(NewInsn);
								}
								//break;
							//}
						//}
						}
					}
				}
			}
		}
	}

	while(!replace_this.empty()) {
		Instruction *old_f = replace_this.back(); replace_this.pop_back();
		Instruction *new_f = with_this.back(); with_this.pop_back();
		InvokeInst *invoke;
		BasicBlock *unwind_bb;
		if ((invoke = dyn_cast<InvokeInst>(old_f)) && (unwind_bb = dyn_cast<BasicBlock>(invoke->getUnwindDest()))) {
			BranchInst *BI = BranchInst::Create(invoke->getNormalDest(), old_f);
		}
		old_f->replaceAllUsesWith(new_f);
		old_f->eraseFromParent();
	}

	// Remove basic blocks that have no predecessors (except the entry block)...
	// or that just have themself as a predecessor.  These are unreachable.
	simplifyCFG(hetero_func);

	//hetero_func->dump();
}


void HeteroTransform::deleteSet(vector<Instruction *> &toDelete) {
	while(!toDelete.empty()) {
		Instruction *old_f = toDelete.back(); 
		toDelete.pop_back();
		//DEBUG(dbgs() << "\n Delete:" << *old_f);
		old_f->eraseFromParent();
	}
}
void HeteroTransform::simplifyCFG(Function *hetero_func) {
	bool changed = true;
	vector<BasicBlock *> toDelete;
	toDelete.clear();
	while(changed) {
		changed = false;
		for (Function::iterator BB = hetero_func->begin(), BBE = hetero_func->end(); BB != BBE; ++BB) {
			if (((pred_begin(BB) == pred_end(BB)) &&
				(BB != hetero_func->getEntryBlock())) ||
				(BB->getSinglePredecessor() == BB)) {
				//DEBUG(dbgs() << "Removing BB: \n" << *BB);
				toDelete.push_back(BB);			
				changed = true;
			}
		}
		while(!toDelete.empty()) {
			BasicBlock *BB = toDelete.back(); toDelete.pop_back();
			DeleteDeadBlock(BB);
		}
	}
}

/* Check for restrictions in the initial prototype 
This is in accordance with the design document
*/
bool HeteroTransform::check_compiler_restrictions_perf(Function *hetero_func) {

	for (Function::iterator BBI = hetero_func->begin(), BBE = hetero_func->end(); BBI != BBE; ++BBI) {
		for (BasicBlock::iterator INSNI = BBI->begin(), INSNE = BBI->end(); INSNI != INSNE; ++INSNI) {
			if (isa<CallInst>(INSNI) || isa<InvokeInst>(INSNI)) {
				CallSite CI(cast<Instruction>(INSNI));
				Function *fun = CI.getCalledFunction();
				if (fun == NULL || (!isBuiltinFunction(fun) /*&& startSet.find(fun) == startSet.end()*/)) { // not invoking any non gen functions
					errs() << "\nWARNING: Function " << hetero_func->getName() << " calls other functions '"<< fun->getName()<<"'.\n";
				return false;
				}
			}
			/* Removing this restriction for local allocations in private space */
			/*else if (isa<AllocaInst>(INSNI)) { // no alloca instructions
				errs() << "\nWARNING: Function " << hetero_func->getName() << " has alloca instructions.\n";
				return false;
			}*/
			for (User::op_iterator i = INSNI->op_begin(), e = INSNI->op_end(); i != e; ++i) {
				Value *V = *i;
				if (isa<GlobalValue>(V)&&!isBuiltinFunction(V)) {
					V->dump();
					errs() << "\nWARNING: Function " << hetero_func->getName() << " has global data accesses.\n";
					return false;
				}
				else if (V->getType()->getTypeID() == Type::DoubleTyID) {
					errs() << "\nWARNING: Function " << hetero_func->getName() << " has double type operands.\n";
				return false;	
				}
			}
		}
	}
	return true;
}
void HeteroTransform::check_compiler_restrictions(int type) {

	if(type==1){
		for (set<Function *>::iterator it = entry_hetero_function_set.begin(), itt=entry_hetero_function_set.end(); it != itt; it++) {
			Function* hetero_func = *it;
			cleanup_exception_builtin(hetero_func);
			if(check_compiler_restrictions_perf(hetero_func))
				hetero_function_set.insert(*it);
		}
	}
	else if(type==2){
		//for reduce operator and join must have equal number of functions
		for (set<Function *>::iterator it = entry_hetero_function_set.begin(),jt=entry_join_function_set.begin(), itt=entry_hetero_function_set.end(); it != itt; it++,jt++) {
			Function* hetero_func = *it;
			cleanup_exception_builtin(hetero_func);
			Function* hetero_join = *jt;
			cleanup_exception_builtin(hetero_join);
			if(check_compiler_restrictions_perf(hetero_func)&&check_compiler_restrictions_perf(hetero_join)){
				join_function_set.insert(*jt);
				hetero_function_set.insert(*it);
			}
		}	
	}

}

void HeteroTransform::gen_annotations(Module &M,Module *M_string,set<Function *> function_set,string Annotation_Name) {
#ifdef OLD_CODE
	vector<Constant *> Annotations_string;
#endif
	string hetero_function_annot_string;
	for (set<Function *>::iterator it=function_set.begin(), itt=function_set.end(); it != itt; it++) {
		Function *F = *it;
#ifdef OLD_CODE
		Constant *annoStr[1] = {
					ConstantDataArray::getString(F->getContext(), F->getName() , true)
		};
		Annotations_string.push_back(ConstantStruct::get(F->getContext(), annoStr, 1, false));
#endif
		string new_string(F->getName());
		hetero_function_annot_string.append(new_string + " ");
#ifdef HETERO_GCD_ALLOC_TO_MALLOC
		pair<multimap<Function *,Instruction *>::iterator,multimap<Function *,Instruction *>::iterator> ret;
		ret = block2insnMap.equal_range(F);
		for (multimap<Function *, Instruction *>::iterator I = ret.first; I != ret.second; ++I) {
			Instruction *Call = (*I).second;
			CallSite CI(cast<Instruction>(Call));
			rewrite_alloca(CI);
		}
#endif
	}
#ifdef OLD_CODE
	Constant *Struct_string1 = ConstantStruct::get(M.getContext(), Annotations_string,false);
	Constant *Struct_string2 = ConstantStruct::get(M_string->getContext(), Annotations_string,false);
	//add String annotations to the current module
	GlobalValue *globalv = new GlobalVariable(M, Struct_string1->getType(), false,
		GlobalValue::WeakODRLinkage, Struct_string1,
		Annotation_Name);

	//add String annotations to the new module
	GlobalValue *globalv_str = new GlobalVariable(*M_string, Struct_string2->getType(), false,
		GlobalValue::WeakODRLinkage, Struct_string2,
		Annotation_Name);
#endif
	Constant *annoStr = ConstantDataArray::getString(M.getContext(), hetero_function_annot_string , true);
	//add String annotations to the current module
	GlobalValue *globalv = new GlobalVariable(M, annoStr->getType(), false,
		GlobalValue::WeakODRLinkage, annoStr,
		Annotation_Name);
	
	//add String annotations to the new module
	annoStr = ConstantDataArray::getString(M_string->getContext(), hetero_function_annot_string , true);
	GlobalValue *globalv_str = new GlobalVariable(*M_string, annoStr->getType(), false,
		GlobalValue::WeakODRLinkage, annoStr,
		Annotation_Name);
	
}

bool HeteroTransform::runOnModule(Module &M) {
	bool localChange = false;
	//create file
	Out.reset(new tool_output_file("kernel_annotations.bc", ErrorInfo,
                                   raw_fd_ostream::F_Binary));
	//new module for kernel annotations
	Module *M_string = new Module("Kernel_annotations",getGlobalContext());

	int types=2;
	for(int i=1;i<=types;i++){
	// parallel_for_hetero annotations
		get_hetero_functions(M,i);//of type parallel for or reduce
		//Do not do anything if no such hetero function found
		if (!entry_hetero_function_set.empty()) {
			check_compiler_restrictions(i);
			if (!hetero_function_set.empty()) {
				localChange = true;
				if(i==1)
					gen_annotations(M,M_string,hetero_function_set,"hetero.string.annotations");
				else if(i==2){
					gen_annotations(M,M_string,hetero_function_set,"reduce_hetero.string.annotations");
					gen_annotations(M,M_string,join_function_set,"join_hetero.string.annotations");
				}
			}
		}
	entry_hetero_function_set.clear();
	hetero_function_set.clear();
	join_function_set.clear();
	}
	//write and close the file
	//raw_ostream &out=Out->os();
	//requires M_string to be a pointer
	WriteBitcodeToFile(M_string,(Out->os()));
    Out->keep();

	return localChange;
}
