//===-- HeteroVirtual.cpp - Virtual Function handling   -------------------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#include "llvm/Transforms/iHRC/HeteroVirtual.h"

using namespace llvm;
using namespace std;

/**
 *    TODO:
 *			1) Virtual functions inside join function call need to be taken care of
 *
 *
 *
 */

/* Rewrite an instruction based on the mapping on ValueMap */
void HeteroVirtual::rewrite_instruction(Instruction *OldInst, Instruction *NewInst, DenseMap<const Value *, Value *>& ValueMap) {
	int opIdx = 0;
	for (User::op_iterator i = OldInst->op_begin(), e = OldInst->op_end(); i != e; ++i, opIdx++) {
		Value *V = *i;
		if (ValueMap[V] != NULL) {
			NewInst->setOperand(opIdx, ValueMap[V]);
		}
	}
}

/* Standard procedure to clone a function -- TODO move this to an utility class */
void HeteroVirtual::clone_function (Function* NF, Function* F){
	DenseMap<const Value*, Value *> ValueMap;

	// Get the names of the parameters for old function
	for(Function::arg_iterator FI = F->arg_begin(), FE=F->arg_end(), DI=NF->arg_begin(); FE!=FI; ++FI,++DI){
		DI->setName(FI->getName());
		ValueMap[FI]=DI;
	}

	for (Function::const_iterator BI=F->begin(),BE = F->end();BI != BE; ++BI) {
		const BasicBlock &FBB = *BI;
		BasicBlock *NFBB = BasicBlock::Create(FBB.getContext(), "", NF);
		ValueMap[&FBB] = NFBB;
		if (FBB.hasName()){
			NFBB->setName(FBB.getName());
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
			for (User::op_iterator i = II->op_begin(), e = II->op_end(); i != e; ++i, opIdx++) {
				Value *V = *i;
				if (ValueMap[V] != NULL) {
					II->setOperand(opIdx, ValueMap[V]);
				}
			}
		}
	}
}

/**
 * Inputs: kernel function, list of global functions accessed inside kernel, a global struct created for the global kernels which will act as a global context
 * create a new function
 * Algorithm:
 * create a new function for the kernel
 * find the switch statements inserted by prior passes
 * Find the function ptr, replace it by accesses from the global context
 * TODO -- The list of instructions to look at can come from prior pases than pattern recognition
 *
 */
Function *HeteroVirtual::transform_globals_per_f(Function *f, vector<Function *> &virtual_funcs, /*const*/ Type *globals_struct) {
	vector</*const*/ Type *> params;
	vector<Instruction *> toDelete;
	params.clear();
	const FunctionType *ftype = f->getFunctionType();
	for(Function::arg_iterator FI = f->arg_begin(), FE=f->arg_end(); FE!=FI; ++FI){
		params.push_back(FI->getType());
	}
	//params.push_back(PointerType::get(globals_struct,0));
	params.push_back(PointerType::get(Type::getInt8Ty(f->getContext()),0)); // last argument is void *

	/*const*/ Type *RetTy = ftype->getReturnType();
	FunctionType *NFty = FunctionType::get(RetTy,params, false);
	Function *nf = Function::Create(NFty, f->getLinkage(), f->getName()+"_global_hetero");
	nf->copyAttributesFrom(f);
	f->getParent()->getFunctionList().insert(f, nf);

#if EXPLICIT_REWRITE
	clone_function(nf,f);
#else
	ValueToValueMapTy VMap;
	for(Function::arg_iterator FI = f->arg_begin(), FE=f->arg_end(), DI=nf->arg_begin(); FE!=FI; ++FI,++DI){
		DI->setName(FI->getName());
		VMap[FI]=DI;
	}
	CloneFunctionWithExistingBBInto(nf, NULL, f, VMap);
#endif
	
	// global_context = Add cast from i8* argument to globals_struct *
	Function::arg_iterator ArgI = nf->arg_begin();
	ArgI++;	ArgI++;
	Argument *void_global_context = &(*ArgI);
	void_global_context->setName("global_context");

	if (globals_struct != NULL) { // avoid creating cast instruction if this was a NULL
		Function::iterator BBI = nf->begin();
		BasicBlock::iterator II = BBI->begin();
		CastInst *global_context = CastInst::Create(Instruction::BitCast, void_global_context, 
			PointerType::get(globals_struct,0), "", &(*II));
		//DEBUG(dbgs() << "transform_globals_per_f::cast instruction=");
		//global_context->dump();
		for (Function::iterator BBE = nf->end(); BBI != BBE; ++BBI) {
			for (BasicBlock::iterator INSNI = BBI->begin(), INSNE = BBI->end(); INSNI != INSNE; ++INSNI) {
				BitCastInst* BCI;
				Value *function_ptr;
				Function *virtual_func;
				if ((BCI = dyn_cast<BitCastInst>(INSNI)) && ((function_ptr = BCI->getOperand(0)) &&
					(virtual_func = dyn_cast<Function>(function_ptr)))) {
						//DEBUG(dbgs() << "transform_globals_per_f::BCI="); BCI->dump();
						//DEBUG(dbgs() << "transform_globals_per_f::virtual func=" << virtual_func->getNameStr() << "\n"); 
						// idx = find the index of virtual_func in virtual_funcs
						int position = std::find(virtual_funcs.begin(), virtual_funcs.end(), virtual_func) - virtual_funcs.begin();
						// generate getelementptr global_context, idx
						//%a = getelementptr inbounds %struct.abc_t* %s, i32 0, i32 0
						Value *Idxs[2] = {ConstantInt::get(Type::getInt32Ty(f->getContext()), 0), 
							ConstantInt::get(Type::getInt32Ty(f->getContext()), position)};
						Value *gep = GetElementPtrInst::CreateInBounds(global_context, Idxs, /*Idxs+2,*/"",BCI);
						//DEBUG(dbgs() << "transform_globals_per_f::GEP="); gep->dump();

						// generate load 
						//%tmp3 = load i32 (i32)** %a, align 4, !tbaa !0
						LoadInst *li = new LoadInst(gep, "", false, 4, BCI);
						//DEBUG(dbgs() << "transform_globals_per_f::LOAD="); li->dump();

						// cast this to char *
						CastInst *newBCI = CastInst::Create(Instruction::BitCast, li, 
							PointerType::get(Type::getInt8Ty(f->getContext()),0), "", BCI);
						//DEBUG(dbgs() << "transform_globals_per_f::newBCI="); newBCI->dump();
						BCI->replaceAllUsesWith(newBCI);
						// Mark old BCi for delete in the future
						toDelete.push_back(BCI);
				}
			}
		}
		while(!toDelete.empty()) {
			Instruction *g = toDelete.back();
			toDelete.pop_back();	
			g->eraseFromParent();
		}
	}
	return nf;
}



/**
 *  Find global function names used inside kernel -- using pattern match on bitcast
 *  save the functions. Create a new structure that contains the types of the global functions
 *  Currently it can have redundant entries, should be optimized in future -- TODO
 */
void HeteroVirtual::find_globals_per_f(Function *f, vector<Function *> &virtual_funcs, vector</*const*/ Type *> &params) {
	for (Function::iterator BBI = f->begin(), BBE = f->end(); BBI != BBE; ++BBI) {
		for (BasicBlock::iterator INSNI = BBI->begin(), INSNE = BBI->end(); INSNI != INSNE; ++INSNI) {
			BitCastInst* BCI;
			Value *function_ptr;
			Function *virtual_func;
			if ((BCI = dyn_cast<BitCastInst>(INSNI)) && ((function_ptr = BCI->getOperand(0)) &&
				(virtual_func = dyn_cast<Function>(function_ptr)))) {
					//DEBUG(dbgs() << "find_globals_per_f::Virtual Function Call Insn=");
					//BCI->dump();
					/*const*/ PointerType *func_type = virtual_func->getType();
					params.push_back(func_type);
					//DEBUG(dbgs() << "find_globals_per_f::Virtual Function Type=");
					//func_type->dump();
					virtual_funcs.push_back(virtual_func);
			}
		}
	}
	
}




/**
 * create an internal offload signature to retain the old offload syntax for the added new global_context
 */
Function *HeteroVirtual::create_internal_offload_signature(Function *offload_old) {
	const FunctionType *FTy = offload_old->getFunctionType();
	vector</*const*/ Type*> Params;
	/*const*/ Type *RetTy = FTy->getReturnType();
	string name = "_offload";
	for (Function::arg_iterator I = offload_old->arg_begin(), E = offload_old->arg_end(); I != E; ++I)
		Params.push_back(I->getType());
	FunctionType *NFTy = FunctionType::get(RetTy, Params, false);
	Function *offload_new = Function::Create(NFTy, offload_old->getLinkage(), name);
	offload_new->copyAttributesFrom(offload_old);
	offload_old->getParent()->getFunctionList().insert(offload_old, offload_new);
	return offload_new;
}

/*
 * Create the new signature for offload with a new void * as global context
 */
Function *HeteroVirtual::create_new_offload_signature(Function *offload_old) {
	
	const FunctionType *FTy = offload_old->getFunctionType();
	vector</*const*/ Type*> Params;
	/*const*/ Type *RetTy = FTy->getReturnType();
	
	// the function pointer type now changes to foo(void *, size_t tid, void *)
	// offlaod (N, ctx, void (*)(void *, size_t, void *), g_ctx)
	Function::arg_iterator I = offload_old->arg_begin();
	Params.push_back(I->getType()); // iter
	++I;
	Params.push_back(I->getType()); // original context
	++I;
	const PointerType *ptrType = dyn_cast<PointerType>(I->getType());
	const FunctionType *FFTy = dyn_cast<FunctionType>(ptrType->getElementType()); // function ptr
	std::vector</*const*/ Type*> fparams(FFTy->param_begin(), FFTy->param_end()); // push all its arguments
	fparams.push_back(PointerType::get(Type::getInt8Ty(offload_old->getContext()),0)); // push global context
	FunctionType *fpNFTy = FunctionType::get(FFTy->getReturnType(), fparams, false); 
	Params.push_back(PointerType::get(fpNFTy, 0));
	++I;
	
	/* global context */
	Params.push_back(PointerType::get(Type::getInt8Ty(offload_old->getContext()),0));

	FunctionType *NFTy = FunctionType::get(RetTy, Params, false);
	Function *offload_new = Function::Create(NFTy, offload_old->getLinkage(), OFFLOAD_NAME);
	offload_new->copyAttributesFrom(offload_old);
	// append the function to module now
	offload_old->getParent()->getFunctionList().insert(offload_old, offload_new);
	return offload_new;
}

/*
* rename each offload call to add the new global context
* if there is no global_context from the analysis, then add void
* 
*/
void HeteroVirtual::find_offload_and_rename(Module &M) {
	Function *offload_func = M.getFunction(OFFLOAD_NAME);
	if (offload_func != NULL) {

		/* create a new offload for the ond one as "_offload" */
		Function *new_offload = create_internal_offload_signature(offload_func); 
		offload_func->replaceAllUsesWith(new_offload);
		offload_func->eraseFromParent();
		

		offload_func = new_offload;
		//DEBUG(dbgs() << "\nfind_offload_and_rename::new offload func=" << offload_func->getNameStr() << "\n");
		Function *new_offload_func = create_new_offload_signature(offload_func); // now offload with global context
		//DEBUG(dbgs() << "\nfind_offload_and_rename::rewritten offload func=" << new_offload_func->getNameStr() << "\n");
		
		vector<Function *> virtual_funcs;
		vector<Value *> toDeleteInsn, toDeleteFunc;
		DenseMap<Function *, /*const*/ Type *> kernel_global_type_map;
		DenseMap<Function *, Function *> kernel_new_kernel_map;

		/* visit all the call sites of offload and rename it to new offload with global_context*/
		for (Value::use_iterator i = offload_func->use_begin(), e = offload_func->use_end(); i != e; ++i) {
			Instruction *call;
			DEBUG(dbgs() << "Next _offload function:");
			//i->dump();
			//i->stripPointerCasts();
			//i->dump();
			if ((call = dyn_cast<InvokeInst>(*i)) || (call = dyn_cast<CallInst>(*i))) {
				CallSite CI(cast<Instruction>(call));
				int type;

				//CI->dump();

				virtual_funcs.clear();
				Function *kernel = NULL;
				Value *arg_2 = CI.getArgument(2);
				if (arg_2 != NULL) kernel = dyn_cast<Function>(arg_2);
				else continue;

				//Function *kernel = get_hetero_function_f(M, CI, &type);
				//DEBUG(dbgs()  << "find_offload_and_rename::Processing Kernel=" << kernel->getName());

				//handle join
				Function *join_kernel = NULL;
				
				MDNode *gpuMDNode = call->getMetadata("join_gpu");
				if (gpuMDNode != NULL) {
					//DEBUG(dbgs() << "find_offload_and_rename:: found having join\n");
					virtual_funcs.clear();
					MDString *gpuMDString = dyn_cast<MDString>(gpuMDNode->getOperand(0));
					join_kernel = M.getFunction(gpuMDString->getString().str().c_str());
					//DEBUG(dbgs()  << "find_offload_and_rename::Processing Join Kernel=" << join_kernel->getName());
				}

				//if (type == 1) { // not a join function
				

				Value *global_context;
				/*const*/ Type *global_struct_type;
				Function *new_kernel;
				Function *new_join_kernel;
				if (kernel_new_kernel_map[kernel] == NULL) {

					vector</*const*/ Type *> params;
					/* find all global function synbols iside the kernel and create a structure out of this,
					also find all virtual funcs inside code */
					global_struct_type = NULL;
						
					params.clear();
					virtual_funcs.clear(); // Redundant check later TODO

					find_globals_per_f(kernel, virtual_funcs, params);
					if (join_kernel != NULL) 
						find_globals_per_f(kernel, virtual_funcs, params);
					if(!params.empty()) 
						global_struct_type = StructType::get(M.getContext(), params, false); 
						
					kernel_global_type_map[kernel] = global_struct_type;

					/* now transform the kernel to take the new global_strct and load from the new global context instead of old*/
					kernel_new_kernel_map[kernel] = new_kernel = transform_globals_per_f(kernel, virtual_funcs, global_struct_type);
					add_inline_attributes(new_kernel);
					//TODO verify this
					toDeleteFunc.push_back(kernel);
					
					if (join_kernel != NULL) {
						kernel_new_kernel_map[join_kernel] = new_join_kernel = transform_globals_per_f(join_kernel, virtual_funcs, global_struct_type);
						add_inline_attributes(new_join_kernel);
						//TODO verify this
						toDeleteInsn.push_back(join_kernel);
					}
					
				}
				else {
					global_struct_type = kernel_global_type_map[kernel];
					new_kernel = kernel_new_kernel_map[kernel];
					new_join_kernel = kernel_new_kernel_map[join_kernel];
				}
				if ((global_struct_type != NULL) && (new_kernel != NULL)) {
					SmallVector<Value*, 16> Args;
					Args.clear();	
					
					// offload site
					// malloc global_context using global_struct_type
					// %0 = tail call i8* @scalable_malloc(i32 4096)
					// find the size of the struct
					int global_struct_size=0;
					for(unsigned int j = 0;j < global_struct_type->getNumContainedTypes();j++){
						/*const*/ Type *ty = global_struct_type->getContainedType(j);
						int t_size = TD->getTypeStoreSizeInBits(ty); 
						assert(t_size != 0);
						if(t_size==0){
							DEBUG(dbgs()<<"Error:: Size Can't be 0\n");
							ty->dump();
						}
						global_struct_size += t_size;
					}
					global_struct_size /= 8; // in bytes
					//DEBUG(dbgs() << "find_offload_and_rename::Size of object in bytes=" << global_struct_size);

					// TODO: must be using shared malloc, but use scalable malloc for now!! assuming tbb malloc is used
					Function *malloc_f = M.getFunction("scalable_malloc");
					if (malloc_f == NULL) malloc_f = M.getFunction("malloc_shared");
					assert(malloc_f != NULL);
					Function::arg_iterator malloc_f_iterator = malloc_f->arg_begin();
					Argument *malloc_size_arg = &*malloc_f_iterator;
					Constant *size = ConstantInt::get(malloc_size_arg->getType(), global_struct_size);
					Args.push_back(size);
					CallInst *malloc = cast<CallInst>(CallInst::Create(malloc_f, Args, "", call));
					malloc->setTailCall();

					//DEBUG(dbgs() << "find_offload_and_rename::Malloc call=");
					//malloc->dump();


					// for each element in virtual_funcs, assign them one by one to global_context
					// %b = getelementptr inbounds i8* %call, i32 4
					// %0 = bitcast i8* %b to i32 (i32, i32)**
					// store i32 (i32, i32)* @_Z3barii, i32 (i32, i32)** %0, align 4, !tbaa !0
					int idx = 0;
					for (vector<Function *>::iterator its = virtual_funcs.begin(), ite = virtual_funcs.end(); its != ite; its++, idx++) {
						Function *virtual_func = *its;
						// %b = getelementptr inbounds i8* %call, i32 4
						/*Value *Idxs[1] = {ConstantInt::get(Type::getInt32Ty(M.getContext()), 0), 
							ConstantInt::get(Type::getInt32Ty(M.getContext()), idx)};*/
						Value *Idxs[1] = {ConstantInt::get(Type::getInt32Ty(M.getContext()), idx)};
						Value *gep = GetElementPtrInst::CreateInBounds(malloc, Idxs, /*Idxs+1,*/ "",call);
						//DEBUG(dbgs() << "find_offload_and_rename::Load the global function name as GEP=");
						//gep->dump();

						// %0 = bitcast i8* %b to i32 (i32, i32)**
						/*const*/ Type *elem_type = global_struct_type->getContainedType(idx);
						CastInst *BCI = CastInst::Create(Instruction::BitCast, gep, PointerType::get(elem_type,0), "", call);

						//DEBUG(dbgs() << "find_offload_and_rename::Generate CAST=");
						//BCI->dump();


						// store i32 (i32, i32)* @_Z3barii, i32 (i32, i32)** %0, align 4, !tbaa !0
						// TODO verify if the alignment should be 8 or 4!!!!
						StoreInst *s = new StoreInst(virtual_func, BCI, false, TD->getPointerPrefAlignment(), call);	
						//DEBUG(dbgs() << "find_offload_and_rename::Store the global function name as STORE=");
						//s->dump();
					}
					global_context = malloc;
				}

				else { // generate null as globa_context if the kernel did not have any virtual function
						global_context =
						ConstantExpr::getNullValue(PointerType::get(Type::getInt8Ty(M.getContext()), 0));
				}

				SmallVector<Value*, 16> Args;
				// replace _offload with offload call
				Args.clear(); 
#if defined(LLVM_3_2)
				SmallVector<AttributeWithIndex, 8> AttrVec;
				const AttrListPtr &OldCallPAL = CI.getAttributes();
				Attributes attrs = OldCallPAL.getRetAttributes();
				if (attrs.hasAttributes()) 
					AttrVec.push_back(AttributeWithIndex::get(0, attrs));
#else
				SmallVector<AttributeSet, 8> AttrVec;
				const AttributeSet &OldCallPAL = CI.getAttributes();
				// Add any return attributes.
				if (OldCallPAL.hasAttributes(AttributeSet::ReturnIndex))
					AttrVec.push_back(AttributeSet::get(M.getContext(),
                                                OldCallPAL.getRetAttributes()));
#endif
				int Index=0;

				CallSite::arg_iterator CallI = CI.arg_begin();
				Args.push_back(*CallI);  // num iters
				//if (Attributes Attrs = OldCallPAL.getParamAttributes(Index))
				//		AttrVec.push_back(AttributeWithIndex::get(Args.size(), Attrs));
				++CallI; ++Index;
				Args.push_back(*CallI); // context
				//if (Attributes Attrs = OldCallPAL.getParamAttributes(Index))
				//		AttrVec.push_back(AttributeWithIndex::get(Args.size(), Attrs));
				++CallI; ++Index;
				Args.push_back(new_kernel); // function ptr -- to the new one
				//if (Attributes Attrs = OldCallPAL.getParamAttributes(Index))
				//		AttrVec.push_back(AttributeWithIndex::get(Args.size(), Attrs));
				
				/*for (CallSite::arg_iterator CallI = CI.arg_begin(), CallE = CI.arg_end(); CallI != CallE; ++CallI, Index++) {
					Args.push_back(*CallI);          // copy the old arguments
					if (Attributes Attrs = OldCallPAL.getParamAttributes(Index))
						AttrVec.push_back(AttributeWithIndex::get(Args.size(), Attrs));
				}*/
				Args.push_back(global_context);
				Instruction *NewCallInst;
				if (InvokeInst *invoke = dyn_cast<InvokeInst>(call)) {
					NewCallInst = InvokeInst::Create(new_offload_func, invoke->getNormalDest(), invoke->getUnwindDest(), Args, "", call);
					cast<InvokeInst>(NewCallInst)->setCallingConv(CI.getCallingConv());
#if defined(LLVM_3_2)
					cast<InvokeInst>(NewCallInst)->setAttributes(AttrListPtr::get(call->getContext(), AttrVec)); 
#endif
					cast<InvokeInst>(NewCallInst)->setAttributes(AttributeSet::get(call->getContext(), AttrVec));
				} else {
					NewCallInst = CallInst::Create(new_offload_func, Args, "", call);
					cast<CallInst>(NewCallInst)->setCallingConv(CI.getCallingConv());
#if defined(LLVM_3_2)
					cast<CallInst>(NewCallInst)->setAttributes(AttrListPtr::get(call->getContext(), AttrVec));
#endif
					cast<CallInst>(NewCallInst)->setAttributes(AttributeSet::get(call->getContext(), AttrVec));
					if (cast<CallInst>(call)->isTailCall()) cast<CallInst>(NewCallInst)->setTailCall();
				}
				/* propagate join meta data */
				MDNode *cpuMDNode = call->getMetadata("join_cpu");
				MDNode *sizeMDNode = call->getMetadata("object_size");
				MDNode *schedulerhintMDNode = call->getMetadata("scheduler_hint");
				
				if (cpuMDNode != NULL)
					NewCallInst->setMetadata("join_cpu", cpuMDNode);
				if (sizeMDNode != NULL)
					NewCallInst->setMetadata("object_size", sizeMDNode);
				if (schedulerhintMDNode != NULL) {
					DEBUG(dbgs() << "found scheduler hint !!");
					NewCallInst->setMetadata("scheduler_hint", schedulerhintMDNode);
				}
				if (gpuMDNode != NULL) {
					Value *Elts1[] = {MDString::get(M.getContext(), new_join_kernel->getName())}; 
					MDNode *Node1 = MDNode::get(M.getContext(), Elts1); 
					NewCallInst->setMetadata("join_gpu", Node1);
				}

				//DEBUG(dbgs() << "find_offload_and_rename::New offload instruction=");
				//NewCallInst->dump();
				toDeleteInsn.push_back(call);
			//}
			} // if this is an offload call site
		}  // for all usages of offload

		
		/* first delete call instructions */
		while(!toDeleteInsn.empty()) {
			Value *g = toDeleteInsn.back();
			toDeleteInsn.pop_back();
			Instruction *insn;
			if(insn = dyn_cast<Instruction>(g)) {
				insn->eraseFromParent();
			}
		}

		while(!toDeleteFunc.empty()) {
			Value *g = toDeleteFunc.back();
			toDeleteFunc.pop_back();	
			Function *func;
			if((func = dyn_cast<Function>(g)) && (func->use_empty()))
				//DEBUG(dbgs() << "Deleted Func=" << func->getName());
				func->eraseFromParent();
		}
	}
}


/**
 * For the given call instruction for a virtual function call, find the base class including all possible targets
 * Deepak wrote a hand coded version of this function -- manually picked second element of vtable
 * TODO make it more generic (RAJ)
 * Find the base class from call instruction (base_ptr)
 * find its (base_ptr's) contained type,, it should be "class.base"
 * so, the associated class hierarchy node would be _ZTI4base
 * classes = Find all of its derived classes from inverse hierarchy for the above class hierarchy node
 * for each class in classes do
 * get vtable_offset+2 offset, match the operands and add the function to list
 * 
 */
void HeteroVirtual::get_possible_targets(Module &M, const Type *p, 
						int vtable_offset, CallInst *call, Value *base_ptr, SetVector<Function *>& possible_targets) {
	
	/* The below code tries to find the base class name from base_ptr */
	/* This is based on string manipulations which can cause problems for general scenarios -- TODO*/
	const PointerType  *base_ptr_type; 
	const Type *base_type;

	//DEBUG(dbgs() << "get_possible_target:: base_ptr=");
	//base_ptr->dump();

	unsigned int num_operands=call->getNumArgOperands();
	if ((base_ptr_type = dyn_cast<PointerType>(base_ptr->getType())) && 
		(base_type = base_ptr_type->getElementType()) && base_type->isStructTy()) {
			
			//DEBUG(dbgs()<< "\nget_possible_target:: base_class type=");
			//base_type->dump();

			//string type_string = M.getTypeName(base_type);
			string type_string;
			raw_string_ostream rso(type_string);
			base_type->print(rso);
			//DEBUG(dbgs()<< "\nget_possible_target:: base_class type name="<< type_string);
			

			strtok((char *)(type_string.c_str()), " .");
			char *base_name = strtok(NULL, " .");
			int base_length = strlen(base_name);
			string base_class_name("_ZTI");
			base_class_name += utostr(base_length);
			base_class_name += base_name;
			
			//DEBUG(dbgs()<<"get_possible_target:: base_class_name="  << base_class_name <<"\n");


			GlobalVariable *base_class = M.getGlobalVariable(base_class_name);
			SetVector<Value *> worklist;
			worklist.insert(base_class);

			/* keep adding derived classes to worklist and process them one by one until done */
			while(!worklist.empty()) {
				Value *derived = worklist.back();
				worklist.pop_back();

				//DEBUG(dbgs()<<"get_possible_target:: next_derived_class_name=");
				//derived->dump();

				GlobalVariable *derived_class_rti, *derived_vtable;
				if ((derived_class_rti = dyn_cast<GlobalVariable>(derived)) && (rti_to_vtable[derived_class_rti] != NULL)
					&& (derived_vtable = dyn_cast<GlobalVariable>(rti_to_vtable[derived_class_rti]))) {
					
					ConstantArray *Array = dyn_cast<ConstantArray>(derived_vtable->getInitializer());//get the elements in the virtual table
					assert(Array->getType()->getNumElements()>1);//there must be alteast more than one element in the vtable
					ConstantExpr *expr=dyn_cast<ConstantExpr>(Array->getOperand(1));// the first element is a pointer to the hierarchy global variable

					// TODO perform a cross verfication with RTI and vtable information

					expr=dyn_cast<ConstantExpr>(Array->getOperand(vtable_offset+2));
					Function *func=dyn_cast<Function>(expr->getOperand(0));
					
					/* match the function arguments with that of call  -- is there a void casting issue here -- TODO*/
					bool flag=true;
					if(func->arg_size()==num_operands){//function prototype must match
						Function::arg_iterator a= func->arg_begin();
						a++;
						for(unsigned int i=1;i<num_operands;i++){
							if(a->getType()!=call->getArgOperand(i)->getType()){
								flag=false;
								break;
							}
							a++;
						}
					}
					
					if (flag) { // found a target
						possible_targets.insert(func);
						//DEBUG(dbgs() << "get_possible_target:: Found next target as:" << func->getName());
					}

					// iterate class hierarchy and find all derived classes of this derived class
					pair<multimap<Value *,Value *>::iterator,multimap<Value *,Value *>::iterator> ret;
					ret = inverse_hierarchy.equal_range(derived_class_rti);
					for (multimap<Value *, Value *>::iterator I = ret.first; I != ret.second; ++I) {
						Value *next_derived = (*I).second; 
						//DEBUG(dbgs()<<"get_possible_target:: derived_class_name="  << derived->getName() <<"\n");
						worklist.insert(next_derived);
					}
				}
			}
	}
}


/**
 * generate switch statements for all possible targets
 *
 */
void HeteroVirtual::modify_basic_block(Module &M,BasicBlock *BBI,CallInst *call, SetVector<Function *>& possible_targets){

	BasicBlock *parent,*true_child;
	parent=BBI;

	//DEBUG(dbgs() << "CallInst=");
	//call->dump();
	//DEBUG(dbgs() << "Function Before=");
	//BBI->getParent()->dump();
	//DEBUG(dbgs() << "Basic Block containing the virtual function call=");
	//BBI->dump();
	
	BasicBlock* new_BB=parent->splitBasicBlock(call, "continuation");

	SmallVector<Value*, 16> Args; // Argument lists to the new call
#if defined(LLVM_3_2)
	SmallVector<AttributeWithIndex, 8> AttrVec; // Attributes list to the new call
#endif
	SmallVector<AttributeSet, 8> AttrVec;
	SmallVector<Instruction *, 16> Returns; // return values for phi
	//call->dump();
	
	/* This check is unnecessary as parent->back() will yield an unconditinal branch */
	BranchInst *brnch = dyn_cast<BranchInst>(&parent->back());
	if(!brnch && brnch->isConditional()){
		DEBUG(dbgs()<<"modify_basic_block::Not an Unconditional branch! ERROR!\n");
		return; 
	}


	if(possible_targets.size()==0){
		DEBUG(dbgs()<<"modify_basic_block::Function List Cannot be empty! ERROR!\n");
		return; 
	}

	brnch->eraseFromParent();
	
	
	//cast the virtual function
	CastInst *ptrCast = CastInst::Create(Instruction::BitCast, call->getCalledValue(), 
							 PointerType::get(Type::getInt8Ty(M.getContext()), 0), "pointer_cast",parent);
	
	//DEBUG(dbgs() << "modify_basic_block::ptrCast=");
	//ptrCast->dump();

	Function *fun=possible_targets.back();
	possible_targets.pop_back();
	//DEBUG(dbgs()<<"\nmodify_basic_block::Next possible target func=\n");
	//fun->dump();
	//cast the function pointer
	CastInst *CallCast = CastInst::Create(Instruction::BitCast, fun, 
										  PointerType::get(Type::getInt8Ty(M.getContext()), 0), fun->getName()+"_call_cast",parent);

	//DEBUG(dbgs() << "modify_basic_block::callCast=");
	//CallCast->dump();

	//create the compare instruction
	CmpInst *cmp= CmpInst::Create(Instruction::ICmp, CmpInst::ICMP_EQ , ptrCast, CallCast, fun->getName()+"_fun_c", parent);
	//DEBUG(dbgs() << "modify_basic_block::cmpInst=");
	//cmp->dump();

	/*************///Single jump Statement which will have the function call
	BasicBlock *b1=BasicBlock::Create(M.getContext(),fun->getName()+"_call", parent->getParent());	
	true_child=b1;
	//cast the this pointer to the calling class this object
	CastInst *arg1 = CastInst::Create(Instruction::BitCast, call->getArgOperand(0), fun->arg_begin()->getType(), fun->getName()+"_arg_cast", b1);
	//DEBUG(dbgs() << "modify_basic_block::castInst=");
	//arg1->dump();

	//rewrite the indirect call site to the direct call site now
	Args.push_back(arg1);
	for(unsigned int i=1;i<call->getNumArgOperands();i++)Args.push_back(call->getArgOperand(i));	
	Instruction *NewCall = CallInst::Create(fun, Args, "", b1);
	
	//DEBUG(dbgs() << "modify_basic_block::Added new call instruction as:");
	//NewCall->dump();

	cast<CallInst>(NewCall)->setCallingConv(call->getCallingConv());
#if defined(LLVM_3_2)
	const AttrListPtr &OldCallPAL = call->getAttributes();	
	// Add any return attributes.
	Attributes attrs = OldCallPAL.getRetAttributes();
	if (attrs.hasAttributes()) 
		AttrVec.push_back(AttributeWithIndex::get(0, attrs));
	cast<CallInst>(NewCall)->setAttributes(AttrListPtr::get(call->getContext(), AttrVec));
#endif
	const AttributeSet &OldCallPAL = call->getAttributes();
	// Add any return attributes.
	if (OldCallPAL.hasAttributes(AttributeSet::ReturnIndex))
			AttrVec.push_back(AttributeSet::get(M.getContext(),
                                                OldCallPAL.getRetAttributes()));
	cast<CallInst>(NewCall)->setAttributes(AttributeSet::get(call->getContext(), AttrVec));
	AttrVec.clear();
	if (CallInst *c=dyn_cast<CallInst>(call)){
		if(c->isTailCall()) cast<CallInst>(NewCall)->setTailCall();
	}	
	BranchInst *Branch_continue= BranchInst::Create(new_BB, b1)	;
	
	//result is hold in NewCall
	Returns.push_back(NewCall);

	/************* For all other target functions do it in a  loop ****/
	while(!possible_targets.empty()){
		
		Function *fun=possible_targets.back();
		possible_targets.pop_back();
		Args.clear();
		//DEBUG(dbgs()<<"modify_basic_block::Next possible target func=" << fun->getName());

		//create a basic block for false child
		BasicBlock *cb=BasicBlock::Create(M.getContext(),fun->getName()+"_check", BBI->getParent());

        BranchInst *BranchIns= BranchInst::Create(true_child,cb,cmp,parent);
		//re-assign parent to false child
		parent=cb;
		CallCast = CastInst::Create(Instruction::BitCast, fun, 
										   PointerType::get(Type::getInt8Ty(M.getContext()), 0), fun->getName()+"_call_cast",cb);

	   //create a compare instruction
		cmp= CmpInst::Create(Instruction::ICmp, CmpInst::ICMP_EQ , ptrCast, CallCast, fun->getName()+"fun_c", cb);

		 b1=BasicBlock::Create(M.getContext(),fun->getName()+"_call", BBI->getParent());
		
		//cast the this pointer to the calling class
		arg1 = CastInst::Create(Instruction::BitCast, call->getArgOperand(0), fun->arg_begin()->getType(), fun->getName()+"_arg_cast", b1);
		//rewrite the indirect call site to the direct call site now
		Args.push_back(arg1);
		for(unsigned int i=1;i<call->getNumArgOperands();i++)Args.push_back(call->getArgOperand(i));
		Instruction *NewCall = CallInst::Create(fun, Args, "", b1);
		cast<CallInst>(NewCall)->setCallingConv(call->getCallingConv());
#if defined(LLVM_3_2)
		const AttrListPtr &OldCallPAL = call->getAttributes();	
		// Add any return attributes.
		Attributes attrs = OldCallPAL.getRetAttributes();
		if (attrs.hasAttributes()) 
			AttrVec.push_back(AttributeWithIndex::get(0, attrs));
		cast<CallInst>(NewCall)->setAttributes(AttrListPtr::get(call->getContext(), AttrVec));
#endif
		const AttributeSet &OldCallPAL = call->getAttributes();
		// Add any return attributes.
		if (OldCallPAL.hasAttributes(AttributeSet::ReturnIndex))
			AttrVec.push_back(AttributeSet::get(M.getContext(),
                                                OldCallPAL.getRetAttributes()));
		cast<CallInst>(NewCall)->setAttributes(AttributeSet::get(call->getContext(), AttrVec));
		AttrVec.clear();
		if (CallInst *c=dyn_cast<CallInst>(call)){
			if(c->isTailCall()) cast<CallInst>(NewCall)->setTailCall();
		}
		//DEBUG(dbgs() << "modify_basic_block::Added new call instruction as:");
		//NewCall->dump();
		Returns.push_back(NewCall);
	
		BranchInst *Branch_continue= BranchInst::Create(new_BB, b1)	;
		true_child=b1;
	}
	BranchInst *BranchIns= BranchInst::Create(true_child,new_BB,cmp,parent);
	
	// if the call instruction returned a result, create a phi and then merge the results
	// replace all uses of the return calues
	if (!call->getType()->isVoidTy()) {
		// insert a phi
		PHINode *phi = PHINode::Create(call->getType(), Returns.size(),"", call);
		phi->addIncoming(Constant::getNullValue(call->getType()), parent); // null on the last false branch
		for (SmallVector<Instruction *, 16>::iterator II = Returns.begin(), IE=Returns.end(); II!= IE; II++) {
			Instruction *insn = *II;
			phi->addIncoming(insn, insn->getParent());
		}
		call->replaceAllUsesWith(phi);
	}

	//DEBUG(dbgs() << "Function After=");
	//BBI->getParent()->dump();
	
	//call->eraseFromParent();
}



/**
 * get class hierarchy and create maps for runtime_type information to vtables
 */
void HeteroVirtual::get_class_hierarchy(Module &M) {
	//get the class heirarchy
	//DEBUG(dbgs()<< "\nget_class_hierarchy::Print Class Hierarchy and Vtable mapping\n");
	for (Module::global_iterator I = M.global_begin(), E = M.global_end(); I != E; ++I){
		//get the heirarchy info globals
		/* 
		 * @_ZTI7derived = linkonce_odr unnamed_addr constant %1 { i8* bitcast (i8** getelementptr inbounds (i8** @_ZTVN10__cxxabiv120__si_class_type_infoE, i64 2) to i8*), i8* getelementptr inbounds ([9 x i8]* @_ZTS7derived, i32 0, i32 0), i8* bitcast (%0* @_ZTI4base to i8*) }
		 */
		if (I->getName().substr(0,4).compare("_ZTI")==0) {
			ConstantStruct *st = dyn_cast<ConstantStruct>(I->getInitializer());
			//DEBUG(dbgs()<<"Heir#" <<st->getType()->getNumElements()<<"\n");
			//Get all the elements in the global struct
			for (unsigned i = 0, e = st->getType()->getNumElements(); i != e; ++i) {
				ConstantExpr *expr=dyn_cast<ConstantExpr>(st->getOperand(i));
				//get the first operand
				Constant *c_name=expr->getOperand(0);
				StringRef Name = c_name->getName();
				if(Name.substr(0,4).compare("_ZTI")==0){
					//DEBUG(dbgs()<<"base:"<< Name <<" ---> derived:" << I->getName()<< "\n");
					hierarchy.insert(pair<Value *, Value *>(I, c_name));
					inverse_hierarchy.insert(pair<Value *, Value *>(c_name, I));
				}
			}
			//DEBUG(dbgs()<<"End Heir#\n\n");			
		}

		/** Create a mapping from _ZTI to _ZTV variables 
		 * @_ZTV7derived = linkonce_odr unnamed_addr constant [5 x i8*] [i8* null, i8* bitcast (%1* @_ZTI7derived to i8*), i8* bitcast (void (%class.derived*)* @_ZN7derived3fooEv to i8*), i8* bitcast (void (%class.derived*)* @_ZN7derived3gooEv to i8*), i8* bitcast (void (%class.base*)* @_ZN4base3barEv to i8*)]
		 * @_ZTI7derived = linkonce_odr unnamed_addr constant %1 { i8* bitcast (i8** getelementptr inbounds (i8** @_ZTVN10__cxxabiv120__si_class_type_infoE, i64 2) to i8*), i8* getelementptr inbounds ([9 x i8]* @_ZTS7derived, i32 0, i32 0), i8* bitcast (%0* @_ZTI4base to i8*) }
		 */
		else if (I->getName().substr(0,4).compare("_ZTV")==0 && I->getName().substr(0,5).compare("_ZTVN")!=0) {
			ConstantArray *Array = dyn_cast<ConstantArray>(I->getInitializer());//get the elements in the virtual table
			assert(Array->getType()->getNumElements()>1);//there must be alteast more than one element in the vtable
			ConstantExpr *expr=dyn_cast<ConstantExpr>(Array->getOperand(1)); // gives the RTI type information
			Constant *c_name=expr->getOperand(0); // must be a _ZTI information
			StringRef Name = c_name->getName();
			if(Name.substr(0,4).compare("_ZTI")==0){
				//DEBUG(dbgs()<<"vtable for:"<< Name <<" is:" << I->getName()<< "\n");
				rti_to_vtable[c_name] = I;	
			}
		}
	}
}




/**
* find if any kernel function has virtual call or not 
*/
bool HeteroVirtual::translate_virtual_calls_to_switch(Module &M) {
	Function *offload_func = M.getFunction(OFFLOAD_NAME);
	if (offload_func != NULL) {

		vector<Instruction *> virtual_call_sites;

		for (Value::use_iterator i = offload_func->use_begin(), e = offload_func->use_end(); i != e; ++i) {
			Instruction *call;
			if ((call = dyn_cast<InvokeInst>(*i)) || (call = dyn_cast<CallInst>(*i))) {
				CallSite CI(cast<Instruction>(call));
				int type;
				Function *kernel = get_hetero_function_f(M, CI, &type);
				//DEBUG(dbgs()  << "\ntranslate_virtual_calls_to_switch::Processing Kernel=" << kernel->getNameStr() << "\n");

				//%0 = bitcast %class.base* %b to void (%class.base*)***
				//%vtable = load void (%class.base*)*** %0, align 8
				//%vfn = getelementptr inbounds void (%class.base*)** %vtable, i64 1
				//%1 = load void (%class.base*)** %vfn, align 8
				//tail call void %1(%class.base* %b)
				for (Function::iterator BBI = kernel->begin(), BBE = kernel->end(); BBI != BBE; ++BBI) {
					for (BasicBlock::iterator INSNI = BBI->begin(), INSNE = BBI->end(); INSNI != INSNE; ++INSNI) {
						CallInst *call; InvokeInst *invoke; Value *call_ptr; 
						if (call = dyn_cast<CallInst>(INSNI)) {
							// It is not a direct call -- need to be expanded
							if ((call->getCalledFunction() == NULL) && (call_ptr = call->getCalledValue())) {  // TODO: more pattern matches in future
								//DEBUG(dbgs() << "translate_virtual_calls_to_switch::Virtual Function Call Insn=");
								//call->dump();
								//DEBUG(dbgs() << "translate_virtual_calls_to_switch::Function of virtual call=");
								//call_ptr->dump();
								virtual_call_sites.push_back(call);						
							}
						}
						else if (invoke = dyn_cast<InvokeInst>(INSNI)) {
							if ((invoke->getCalledFunction() == NULL) && (call_ptr = invoke->getCalledValue())) { // TODO: more pattern matches in future
								//DEBUG(dbgs() << "translate_virtual_calls_to_switch::Virtual Function Call Insn=");
								//invoke->dump();
								//DEBUG(dbgs() << "translate_virtual_calls_to_switch::Function of virtual call=");
								//call_ptr->dump();
								assert(false || "translate_virtual_calls_to_switch::TODO: should handle in future");
								virtual_call_sites.push_back(invoke);
							}
						}
					}
				}

			}
		}

		/* found virtual call sites */
		if (!virtual_call_sites.empty()) {

			/* get the class hierarchy */
			get_class_hierarchy(M);

			const Type *tyype;
			int offset_int=-1;

			vector<Value *> toDelete;
			for (vector<Instruction *>::iterator it = virtual_call_sites.begin(), itt=virtual_call_sites.end(); it != itt; it++) {
				Instruction *insn = *it;
				CallInst *call; Value *call_ptr; InvokeInst *invoke;
				if (call = dyn_cast<CallInst>(insn)) {
					call_ptr = call->getCalledValue();
				}
				else if(invoke = dyn_cast<InvokeInst>(insn)) { // do not handle invoke for now
					assert(false || "translate_virtual_calls_to_switch::TODO: should handle in future");
					call_ptr = invoke->getCalledValue();
				}

				/* Pattern matching on virtual function code: THIS CODE CAN BREAK IF THE PATTERN DOES NOT MATCH -- TODO
				%0 = bitcast %class.base* %b to void (%class.base*)***
				%vtable = load void (%class.base*)*** %0, align 8
				%vfn = getelementptr inbounds void (%class.base*)** %vtable, i64 1
				%1 = load void (%class.base*)** %vfn, align 8 
				tail call void %1(%class.base* %b)

				or
				
				%0 = bitcast %class.base* %tmp to i32 (%class.base*)***
				%vtable = load i32 (%class.base*)*** %0, align 8
				%1 = load i32 (%class.base*)** %vtable, align 8
				%call = tail call i32 %1(%class.base* %tmp) */

				User* virtual_function,*pointer_to_virtual_function, *vtable, *cast_base, *ptr_base;
				if ((virtual_function = dyn_cast<User>(call_ptr)) && (pointer_to_virtual_function = 
					dyn_cast<User>(virtual_function->getOperand(0)))) {

						//DEBUG(dbgs() << "translate_virtual_calls_to_switch::virtual_function: ");
						//virtual_function->dump();
						//DEBUG(dbgs() << "translate_virtual_calls_to_switch::type of virtual function: ");
						tyype=virtual_function->getType();
						//tyype->dump();
						//DEBUG(dbgs() << "translate_virtual_calls_to_switch::pointer to virtual function: ");
						//pointer_to_virtual_function->dump();
							
						/* %vfn = getelementptr inbounds void (%class.base*)** %vtable, i64 1 */
						/* you got either vfn or vtable if the offset is zero */
						GetElementPtrInst *GEPP;
						LoadInst *loadP;

						if (GEPP = dyn_cast<GetElementPtrInst>(pointer_to_virtual_function)) {
							//DEBUG(dbgs() << "Found a GEP\n");
							vtable = dyn_cast<User>(pointer_to_virtual_function->getOperand(0));
							Value* vtable_offset=pointer_to_virtual_function->getOperand(1);
							assert(isa<ConstantInt>(vtable_offset));
							ConstantInt *offset=cast<ConstantInt>(vtable_offset);
							offset_int=offset->getValue().getSExtValue();
						}
						else if (loadP = dyn_cast<LoadInst>(pointer_to_virtual_function)) {
							vtable = pointer_to_virtual_function;
							offset_int = 0;
						}
						else {
							DEBUG(dbgs() << "Error!!! virtual function detection pattern not found");
							//return false;
						}

						if ((cast_base = dyn_cast<User>(vtable->getOperand(0))) && 
							(ptr_base = dyn_cast<User>(cast_base->getOperand(0)))) {
				
								//DEBUG(dbgs() << "translate_virtual_calls_to_switch::cast_base=");
								//cast_base->dump();
								//DEBUG(dbgs() << "translate_virtual_calls_to_switch::ptr_base=");
								//ptr_base->dump();

								//DEBUG(dbgs() << "translate_virtual_calls_to_switch::virtual table offset: " << offset_int);
								SetVector<Function *> possible_targets;
							
								/* find all targets for the given virtual call site */
								get_possible_targets(M, tyype, offset_int, call, ptr_base, possible_targets); 

								/* modify the IR to generate switch statements for each possible targets */
								modify_basic_block(M,insn->getParent(),call, possible_targets);

								toDelete.push_back(call);

								//DEBUG(dbgs() << "Print new kernel function::");
								//insn->getParent()->getParent()->dump();
						}
						else {
							DEBUG(dbgs() << "Error!!! virtual function detection pattern not found");
							//return false;
						}

				} // outer pattern match
			} //iterate over all virtual calls
			while(!toDelete.empty()) {
				Value *g = toDelete.back();
				toDelete.pop_back();
				Instruction *insn;
				if(insn = dyn_cast<Instruction>(g))
					insn->eraseFromParent();
			}
			return true; // there are virtual calls
		} // end if (!virtual_call_sites.empty()) 
	}	
	return false; // no virtual call
}


	/* Algorithm:
		f = create a new function "define void @promote_vtable()"

		for each vtable in vatbles:_ZTV* && !_ZTVN* global variables, do
			size = parse vtable to find the number of elements
			
			create a global variable: 
			@_ZTV4base_hetero = global i8** null, align 8
			
			Add the following instructions in function "f"
			%base = scalable_malloc(8*size) // for 32-bit it will be 4*size
			%base_cast = bitcast i8* %base to i8**
			store i8** %base_cast, i8*** @_ZTV4base_hetero, align 8

			for every element in vtable, do

			    create the following instructions in f:
				%idx1 = getelementptr inbounds i8* %base, i64 8
				%cast_idx1 =  bitcast i8* %idx1 to i8**
				store i8* bitcast (%0* @_ZTI4base to i8*), i8** %cast_idx1, align 8
			

			endfor
			create a map from _ZTV4base to _ZTV4base_hetero
		endfor

		add a call to "f" in the entry main method of the program // has implications

		search for the following pattern in the entire program:
		;store i32 (...)** bitcast (i8** getelementptr inbounds ([5 x i8*]* @_ZTV4base, i64 0, i64 2) to i32 (...)**), i32 (...)*** %0, align 8

		and replace with the following
		%base_load = load i8*** @_ZTV4base_hetero, align 8
		%base_idx = getelementptr inbounds i8** %base_load, i64 2
		%base_cast =  bitcast i8** %base_idx to i32 (...)**
		store i32 (...)** %base_cast, i32 (...)*** %0, align 8

		*/
/* find the vtables copy them to shared region */
void HeteroVirtual::allocate_vtables_in_shared_region(Module &M) {

	/* create a new function */
	FunctionType *promoteVTableFTy = FunctionType::get(Type::getVoidTy(M.getContext()), false);
	Function *vtable_func = Function::Create(promoteVTableFTy, GlobalVariable::PrivateLinkage, "__promote_vtable_hetero__");
	BasicBlock *promoteBB = BasicBlock::Create(M.getContext(), "", vtable_func);

	DenseMap<Value *, Value *> old_new_vtable_map;
	SetVector<Value *> vtables;

	for (Module::global_iterator I = M.global_begin(), E = M.global_end(); I != E; ++I){
		 if (I->getName().substr(0,4).compare("_ZTV")==0 && I->getName().substr(0,5).compare("_ZTVN")!=0) {

			 //I->dump();
			
			 vtables.insert(I);

			 /* create a global variable */
			 string nameString = "__";
			 nameString += I->getName();
			 nameString += "_hetero__";

			 /* LLVM does not accept void *; change to i8** */
			 /*const*/ Type *void_star_star = PointerType::get(PointerType::get(Type::getInt8Ty(M.getContext()), 0), 0);
			 Constant *Null = Constant::getNullValue(void_star_star);

			 //Should the linkage be: LinkOnceODRLinkage
			 GlobalVariable *vtableShared = new GlobalVariable(M, void_star_star, false,
				 GlobalVariable::InternalLinkage, Null, nameString);
			 vtableShared->setAlignment(TD->getPointerPrefAlignment()); // uses Preferred Alignment but not ABI alignment
			 
			 
			 ConstantArray *list = dyn_cast<ConstantArray>(I->getInitializer());//get the elements in the virtual table
			 unsigned int ptr_size = (TD->getPointerSize());
			 unsigned int list_size = list->getType()->getNumElements();
			 //DEBUG(dbgs() << "pointer_size=" << ptr_size << " list_size=" << list_size);


			 /* Add the following instructions in function "f"
			    %base = scalable_malloc(8*size) // for 32-bit it will be 4*size
			    %base_cast = bitcast i8* %base to i8**
			    store i8** %base_cast, i8*** @_ZTV4base_hetero, align 8 */

			 Function *malloc_f = M.getFunction("scalable_malloc");
			 if (malloc_f == NULL) {
				 malloc_f = M.getFunction("malloc_shared");
			 }
			 assert(malloc_f != NULL);

			 Function::arg_iterator malloc_f_iterator = malloc_f->arg_begin();
			 Argument *malloc_size_arg = &*malloc_f_iterator;
			 
			 
			 Constant *size = ConstantInt::get(malloc_size_arg->getType(), ptr_size*list_size);
			 CallInst *malloc = cast<CallInst>(CallInst::Create(malloc_f, size, "", promoteBB));
			 malloc->setTailCall();
			 //DEBUG(dbgs() << "allocate_vtables_in_shared_region::Malloc call=");
			 //malloc->dump();


 			 //%base_cast = bitcast i8* %base to i8**
			 CastInst *BCI = CastInst::Create(Instruction::BitCast, malloc, void_star_star, "", promoteBB);
			 //DEBUG(dbgs() << "allocate_vtables_in_shared_region::Generate CAST=");
			 //BCI->dump();


			 //store i8** %base_cast, i8*** @_ZTV4base_hetero, align 8
			 StoreInst *s = new StoreInst(BCI, vtableShared,  false, TD->getPointerPrefAlignment(), promoteBB);	
			 //DEBUG(dbgs() << "allocate_vtables_in_shared_region::Store the global function name as STORE=");
			 //s->dump();

			 /* for every element in vtable, do

			    create the following instructions in f:
				%idx1 = getelementptr inbounds i8* %base, i64 8
				%cast_idx1 =  bitcast i8* %idx1 to i8**
				store i8* bitcast (%0* @_ZTI4base to i8*), i8** %cast_idx1, align 8
				*/
			 //list->dump();
			 //DEBUG(dbgs() << "list num elements=" << list->getType()->getNumElements());
			for (unsigned i = 1, e = list->getType()->getNumElements(); i != e; ++i) {
				//ConstantStruct *element = cast<ConstantStruct>(list->getOperand(i));
				ConstantExpr *expr=dyn_cast<ConstantExpr>(list->getOperand(i)); // gives the RTI type information
				Constant *global=expr->getOperand(0); 
				int offset = i * TD->getPointerSize();

				//%idx1 = getelementptr inbounds i8* %base, i64 8
				Value *Idxs[1] = {ConstantInt::get(Type::getInt32Ty(M.getContext()), offset)};
				Value *gep = GetElementPtrInst::CreateInBounds(malloc, Idxs, /*Idxs+1,*/"",promoteBB);
				//DEBUG(dbgs() << "allocate_vtables_in_shared_region::Load the global function name as GEP=");
				//gep->dump();

				//%cast_idx1 =  bitcast i8* %idx1 to i8**
				CastInst *BCI = CastInst::Create(Instruction::BitCast, gep, void_star_star, "", promoteBB);
				//DEBUG(dbgs() << "allocate_vtables_in_shared_region::Generate CAST=");
				//BCI->dump();

				//store i8* bitcast (%0* @_ZTI4base to i8*), i8** %cast_idx1, align 8
				StoreInst *ss = new StoreInst(expr, BCI, false, TD->getPointerPrefAlignment(), promoteBB);	
				//DEBUG(dbgs() << "allocate_vtables_in_shared_region::Store the global function name as STORE=");
				//ss->dump();
				
			 }
			 
			//create a map from _ZTV4base to _ZTV4base_hetero
			old_new_vtable_map[I] = vtableShared;		 
			
		 } // end if if over all ZTV
	} //end for

	ReturnInst *ret = ReturnInst::Create(M.getContext(), promoteBB);

	// add a call to "f" in the entry main method of the program // has implications
	Function *main_function = M.getFunction("main");
	if(main_function == NULL) {
		main_function = M.getFunction("WinMain");
	}
	assert(main_function != NULL);


	M.getFunctionList().insert(main_function, vtable_func);
	
	Function::const_iterator BI = main_function->begin();
	const BasicBlock& mainBB = *BI;
	Instruction *first = (Instruction *)(&(mainBB.front()));
	Instruction *call = CallInst::Create(vtable_func, "", first);

	
	/*search for the following pattern in the entire program:  TODO WHOLE PROGRAM ANALYSIS
	  ;store i32 (...)** bitcast (i8** getelementptr inbounds ([5 x i8*]* @_ZTV4base, i64 0, i64 2) to i32 (...)**), i32 (...)*** %0, align 8
	  or
	  store i32 (...)** bitcast (i8** getelementptr inbounds ([5 x i8*]* @_ZTV4base, i64 0, i64 2) to i32 (...)**), i32 (...)*** %1, align 8
	  or
	  %2 = bitcast i8* %call9 to i8***
      store i8** getelementptr inbounds ([5 x i8*]* @_ZTV7derived, i64 0, i64 2), i8*** %2, align 8
	  
	  
	  and replace with the following
	  %base_load = load i8*** @_ZTV4base_hetero, align 8
	  %base_idx = getelementptr inbounds i8** %base_load, i64 2
	  %base_cast =  bitcast i8** %base_idx to i32 (...)**
	  store i32 (...)** %base_cast, i32 (...)*** %0, align 8*/
	vector<Instruction *> toDelete;
	for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I){
		if (!I->isDeclaration()) {
			for (Function::iterator BBI = I->begin(), BBE = I->end(); BBI != BBE; ++BBI) {
				for (BasicBlock::iterator INSNI = BBI->begin(), INSNE = BBI->end(); INSNI != INSNE; ++INSNI) {
					Value *ptr;
					StoreInst *store;
					User *storeOp, *bc, *GEP, *global;
					GlobalVariable *gv;
					ConstantInt *CI, *CII;
				
					//INSNI->dump();

					if ((store = dyn_cast<StoreInst>(INSNI)) && 
						(((bc = dyn_cast<User>(store->getOperand(0))) &&
						(GEP = dyn_cast<User>(bc->getOperand(0))) && 
						(GEP->getNumOperands() == 3) && 
						(global = dyn_cast<User>(GEP->getOperand(0))) && 
						(gv = dyn_cast<GlobalVariable>(global)) && 
						(CI = dyn_cast<ConstantInt>(GEP->getOperand(1))) &&
						(CI->getSExtValue() == 0) &&
						(CII = dyn_cast<ConstantInt>(GEP->getOperand(2))) && 
						(CII->getSExtValue() == 2)) || 
						((GEP = dyn_cast<User>(store->getOperand(0))) && 
						(GEP->getNumOperands() == 3)) &&
						(global = dyn_cast<User>(GEP->getOperand(0))) && 
						(gv = dyn_cast<GlobalVariable>(global)) && 
						(CI = dyn_cast<ConstantInt>(GEP->getOperand(1))) &&
						(CI->getSExtValue() == 0) &&
						(CII = dyn_cast<ConstantInt>(GEP->getOperand(2))) && 
						(CII->getSExtValue() == 2))) {

							ptr = dyn_cast<Value>(gv);
							//DEBUG(dbgs() << "allocate_vtables_in_shared_region::ptr to search in vtable=");
							//ptr->dump();

							SetVector<Value *>::iterator it = find(vtables.begin(), vtables.end(), ptr);
							if (it != vtables.end()) {
								//%base_load = load i8*** @_ZTV4base_hetero, align 8
								assert(old_new_vtable_map[ptr] != NULL);
								Value *new_vtable = old_new_vtable_map[ptr];
								LoadInst *li = new LoadInst(new_vtable, "", false, TD->getPointerPrefAlignment(), INSNI);	
								//DEBUG(dbgs() << "allocate_vtables_in_shared_region::Load=");
								//li->dump();

								//%base_idx = getelementptr inbounds i8** %base_load, i64 2
								Value *Idxs[1] = {ConstantInt::get(Type::getInt32Ty(M.getContext()), 2)};
								GetElementPtrInst *newGEP = GetElementPtrInst::CreateInBounds(li, Idxs, /*Idxs+1,*/ "" , INSNI);
								//DEBUG(dbgs() << "allocate_vtables_in_shared_region::newGEP=");
								//newGEP->dump();

								/*const*/ Type *t = store->getOperand(0)->getType();
								//%base_cast =  bitcast i8** %base_idx to i32 (...)**
								CastInst *cast = CastInst::Create(Instruction::BitCast, newGEP, t, "", INSNI);
								//DEBUG(dbgs() << "allocate_vtables_in_shared_region::cast=");
								//cast->dump();

								//store i32 (...)** %base_cast, i32 (...)*** %0, align 8*/
								StoreInst *st = new StoreInst(cast, store->getPointerOperand(), store->isVolatile(), store->getAlignment(), INSNI);
								//DEBUG(dbgs() << "allocate_vtables_in_shared_region::store=");
								//st->dump();

								//GEP->replaceAllUsesWith(newGEP);
								toDelete.push_back(INSNI);
							} // found in vtable
						}
				} // for each insn
			}
		}
	}	

	while(!toDelete.empty()) {
		Instruction *g = toDelete.back();
		toDelete.pop_back();
		g->eraseFromParent();
	}
}


/** Added by Raj for adding inline attributes for functions called by kernel */
void HeteroVirtual::add_inline_attributes(Function *F) {
	vector<Function *> trans_hetero_function_set;
	trans_hetero_function_set.push_back(F);
	unsigned int size = (unsigned int)trans_hetero_function_set.size();
	for(unsigned int i=0; i<size; i++) {
		Function *F = trans_hetero_function_set.at(i);
		//DEBUG(dbgs() << "Start Function=" << F->getNameStr());
		for (Function::iterator BI = F->begin(), BE = F->end();BI != BE; ++BI) {
			for (BasicBlock::iterator II = BI->begin(), IE = BI->end(); II != IE; ++II) {
				Instruction *Call;
				if ((Call = dyn_cast<CallInst>(II)) || (Call = dyn_cast<InvokeInst>(II))) {
					CallSite CI(Call);
					Function *ff;
					//DEBUG(dbgs() << "Next Call Insn=");
					//Call->dump();
					if ((ff = dyn_cast<Function>(CI.getCalledFunction())) && (!(ff->isDeclaration()))) {
						Instruction *NewInsn;
						vector<Function *>::iterator it = std::find(trans_hetero_function_set.begin(), trans_hetero_function_set.end(), ff);
						if (it == trans_hetero_function_set.end()) {
							//ff->removeFnAttr(llvm::Attribute::NoInline);
							//ff->removeFnAttr(Attributes::get(F->getContext(), Attribute::NoInline));
							ff->addFnAttr(Attribute::AlwaysInline);
							trans_hetero_function_set.push_back(ff);
							size++;
						}
					}
				}
			}
		}
	}
}


//ASSUMPTION: is that inlining has happened already; only virtual calls are left;
bool HeteroVirtual::runOnModule(Module &M) {
	TD = getAnalysisIfAvailable<DataLayout>();
	//ASSUMPTION: is that inlining has happened already; only virtual calls are left;
	//M.dump();
	/* Step 1: find virtual functions in kernel and transform them to switch statements */
	if (translate_virtual_calls_to_switch(M)) {
		/* Step 2: Allocate vtables in shared address space */
		allocate_vtables_in_shared_region(M);
	}
	//M.dump();
	/* Step 3: create global context and embed in the offload function */
	find_offload_and_rename(M);
	//Function *f = M.getFunction("main");
	//f->dump();
	//M.dump();
	return true;
}


