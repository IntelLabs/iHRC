//===-- HeterotbbTransform.cpp - Analyze TBB code      -------------------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
/*
7/18/2011 Changing the signature of parallel_for_hetero as (int, void*)
7/18/2011 Support for static inheritance
*/
#include "llvm/Transforms/iHRC/HeterotbbTransform.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Attributes.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include <assert.h>

using namespace llvm;

/* Rewrite an instruction based on the mapping on ValueMap */
void HeterotbbTransform::rewrite_instruction(Instruction *OldInst, Instruction *NewInst, DenseMap<const Value *, Value *>& ValueMap) {
	int opIdx = 0;
	for (User::op_iterator i = OldInst->op_begin(), e = OldInst->op_end(); i != e; ++i, opIdx++) {
		Value *V = *i;
		if (ValueMap[V] != NULL) {
			NewInst->setOperand(opIdx, ValueMap[V]);
		}
	}
}


void HeterotbbTransform::gen_opt_code_per_f (Function* NF, Function* F){
	// Get the names of the parameters for old function
	Function::arg_iterator FI = F->arg_begin();
	Argument *classname = &*FI;
	FI++;
	Argument *numiters = &*FI;

	// Set the names of the parameters for new function
	Function::arg_iterator DestI = NF->arg_begin();
	DestI->setName(classname->getName()); 
	Argument *class_name = &(*DestI);
	//second argument
	DestI++;
	DestI->setName(numiters->getName());
	Argument *num_iters = &(*DestI);

#ifdef EXPLICIT_REWRITE
	DenseMap<const Value*, Value *> ValueMap;
#else
	ValueToValueMapTy ValueMap;
#endif

#if EXPLICIT_REWRITE
	//get the old basic block and create a new one
	Function::const_iterator BI = F->begin();
	const BasicBlock &FB = *BI;
	BasicBlock *NFBB = BasicBlock::Create(FB.getContext(), "", NF);
	if (FB.hasName()){
		NFBB->setName(FB.getName());
		//DEBUG(dbgs()<<FB.getName()<<"\n");
	}
	ValueMap[&FB] = NFBB;

	ValueMap[numiters] = num_iters;
	//must create a new instruction which casts i32* back to the class name
	CastInst *StrucRevCast = CastInst::Create(Instruction::BitCast, class_name, 
		classname->getType(), classname->getName(), NFBB);
	ValueMap[classname] = StrucRevCast;


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
	Function::const_iterator BI = F->begin();
	const BasicBlock &FB = *BI;
	BasicBlock *NFBB = BasicBlock::Create(FB.getContext(), "", NF);
	if (FB.hasName()){
		NFBB->setName(FB.getName());
	}
	ValueMap[&FB] = NFBB;
	CastInst *StrucRevCast = CastInst::Create(Instruction::BitCast, class_name, 
		classname->getType(), classname->getName(), NFBB);
	ValueMap[classname] = StrucRevCast;
	ValueMap[numiters] = num_iters;
	CloneFunctionWithExistingBBInto(NF, NFBB, F, ValueMap, "");
#endif
}


void HeterotbbTransform::copy_function (Function* NF, Function* F){
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
	//NF->dump();

}
void HeterotbbTransform::edit_template_function (Module &M,Function* F,Function* new_join,GlobalVariable *old_gb,Value *gb){

	SmallVector<Value*, 16> Args; // Argument lists to the new call
	vector<Instruction *> toDelete;
	//	old_gb->dump();
	//	gb->dump();
	Constant *Ids[2];

	for (Function::iterator BI=F->begin(),BE = F->end();BI != BE; ++BI) {
		for (BasicBlock::iterator II = BI->begin(), IE = BI->end(); II != IE; ++II) {
			GetElementPtrInst *GEP;
			GlobalVariable *op;
			if (isa<CallInst>(II) || isa<InvokeInst>(II)) {
				CallSite CI(cast<Instruction>(II));
				//replace dummy reduce with new reduce
				if(CI.getCalledFunction()->getName().equals("__join_reduce_hetero")){
					Args.clear();
					CastInst *newarg1 = CastInst::Create(Instruction::BitCast, CI.getArgument(0), new_join->arg_begin()->getType(), "arg1",CI.getInstruction());
					Args.push_back(newarg1);
					CastInst *newarg2 = CastInst::Create(Instruction::BitCast, CI.getArgument(1), new_join->arg_begin()->getType(), "arg2", CI.getInstruction());					
					Args.push_back(newarg2);

					//no need to set attributes
					Instruction *NewCall = CallInst::Create(new_join, Args, "", CI.getInstruction());
					cast<CallInst>(NewCall)->setCallingConv(CI.getCallingConv());
					toDelete.push_back(CI.getInstruction());
					DEBUG(dbgs()<<"Joins Replaced\n");
				}
			}

			/*
			%arrayidx18 = getelementptr inbounds i32 addrspace(3)* getelementptr 
			inbounds ([192 x i32] addrspace(3)* @opencl_kernel_join_name_local_arr, i32 0, i32 0), 
			i64 %idxprom1
			*/
			if((GEP = dyn_cast<GetElementPtrInst>(II)) /*&& 
													   (op = dyn_cast<GlobalVariable>(GEP->getOperand(0)))*/ /*&& 
													   (op->getName().equals("opencl_kernel_join_name_local_arr"))*/) {
														   //II->dump();
														   Value *val= II->getOperand(0);
														   if(Constant *op=dyn_cast<ConstantExpr>(val)){
															   //II->dump();
															   //II->getOperand(1)->dump();

															   /*Ids[0]=cast<Constant>(op->getOperand(1));
															   Ids[1]=cast<Constant>(op->getOperand(1));
															   Constant *new_op = ConstantExpr::getInBoundsGetElementPtr(cast<Constant>(gb),Ids,2);
															   new_op->dump();
															   Instruction *inst = GetElementPtrInst::CreateInBounds(new_op, II->getOperand(1), II->getName()+"_temp",II);
															   Value *Elts[] = {MDString::get(M.getContext(), "local_access")}; 
															   MDNode *Node = MDNode::get(M.getContext(), Elts); 
															   inst->setMetadata("local_access",Node);
															   inst->dump();
															   II->replaceAllUsesWith(inst);
															   toDelete.push_back(II);
															   */

															   Value *Idxs[2] = {ConstantInt::get(Type::getInt32Ty(M.getContext()), 0), 
																   ConstantInt::get(Type::getInt32Ty(M.getContext()), 0)};
															   //gb->getType()->dump();
															   //gb->dump();
															   Instruction *inst_= GetElementPtrInst::CreateInBounds(gb, Idxs, /*Idxs+2,*/ II->getName()+"_temp_",II);
															   //inst_->dump();
															   Instruction *inst= GetElementPtrInst::CreateInBounds(inst_, II->getOperand(1), II->getName()+"_temp",II);
															   Value *Elts[] = {MDString::get(M.getContext(), inst->getName())}; 
															   MDNode *Node = MDNode::get(M.getContext(), Elts); 
															   inst->setMetadata("local_access",Node);

															   //inst->dump();
															   II->replaceAllUsesWith(inst);
															   toDelete.push_back(II);

														   }
			}
		}
	}
	while(!toDelete.empty()) {
		Instruction *g = toDelete.back();
		toDelete.pop_back();

		g->eraseFromParent();
	}

}
Function* HeterotbbTransform::create_new_join(Module &M,Function *join){

	//reduce->dump();
	if(!templat){
		DEBUG(dbgs()<<"NO Template Found\n");
		return NULL;
	}
	//join_main->dump();
	DEBUG(dbgs()<<"Objext size array"<<object_size_hetero<<"\n");

	//create a global with 64*object/4 size
	GlobalVariable *gb = M.getGlobalVariable("opencl_kernel_join_name_local_arr",true);
	//gb->dump();
	Value *val=gb->getOperand(0);
	//if(isa<ArrayType>(val->getType()))DEBUG(dbgs()<<"YES\n");
	//since we are creating an integer array, the size gets divided by 4

	// Do not make it a global variable -- make it a local variable with annotation for local
	int local_size = 64*object_size_hetero;
	/*const*/ ArrayType *arr= ArrayType::get(Type::getInt32Ty(M.getContext()),(64*object_size_hetero)/4);

	/*vector<Constant *> Initializer;
	APInt zero(32,0);
	for(int i=0;i<(16*object_size_hetero);i++){
	Initializer.push_back(ConstantInt::get(M.getContext(),zero));
	}
	Constant *init = ConstantArray::get(arr, Initializer);
	GlobalVariable *new_gb = new GlobalVariable(M, arr, false, GlobalVariable::InternalLinkage,init, "__hetero_local_"+join->getName()+"__local__arr",gb,false,3);
	new_gb->setAlignment(gb->getAlignment());
	DEBUG(dbgs()<<"Global Created\n");
	new_gb->dump();
	*/
	vector</*const*/ Type *> params;
	int temp_size=0;
	object_size_hetero=0;

	//	void join(class.name *,class.name *)
	//re-write join
	const FunctionType *FTy = join->getFunctionType();
	Function::arg_iterator ArgI = join->arg_begin();
	//	class.name *
	params.push_back(PointerType::get((dyn_cast<PointerType>(ArgI->getType())->getElementType()),3));
	params.push_back(PointerType::get((dyn_cast<PointerType>(ArgI->getType())->getElementType()),3));

	/*const*/ Type *RetTy = FTy->getReturnType();
	FunctionType *NFty = FunctionType::get(RetTy,params, false);
	Function *NF=Function::Create(NFty, join->getLinkage(), join->getName()+"_inline");
	NF->copyAttributesFrom(join);
#if EXPLICIT_REWRITE
	copy_function(NF,join);
#else
	ValueToValueMapTy VMap;
	for(Function::arg_iterator FI = join->arg_begin(), FE=join->arg_end(), DI=NF->arg_begin(); FE!=FI; ++FI,++DI){
		DI->setName(FI->getName());
		VMap[FI]=DI;
	}
	CloneFunctionWithExistingBBInto(NF, NULL, join, VMap);
#endif
	//NF->removeFnAttr(Attributes::get(NF->getContext(), Attribute::NoInline));
	NF->addFnAttr(Attribute::AlwaysInline);
	join->getParent()->getFunctionList().insert(join, NF);

	params.clear();
	const FunctionType *FTemp = templat->getFunctionType();
	//create a new template
	for(Function::arg_iterator FI = templat->arg_begin(), FE=templat->arg_end(); FE!=FI; ++FI){
		params.push_back(FI->getType());
	}
	//	templat->replaceUsesOfWith(reduce,NF);
	RetTy = FTy->getReturnType();
	NFty = FunctionType::get(RetTy,params, false);
	Function *templat_copy =Function::Create(NFty, join->getLinkage(), join->getName()+"_hetero");
	templat_copy->copyAttributesFrom(templat);
#if EXPLICIT_REWRITE
	copy_function(templat_copy,templat);
#else
	ValueToValueMapTy VMapp;
	for(Function::arg_iterator FI = templat->arg_begin(), FE=templat->arg_end(), DI=templat_copy->arg_begin(); FE!=FI; ++FI,++DI){
		DI->setName(FI->getName());
		VMapp[FI]=DI;
	}
	CloneFunctionWithExistingBBInto(templat_copy, NULL, templat, VMapp);
#endif

	/* create a local variable with the following type */
	Function::iterator BI = templat_copy->begin();
	BasicBlock::iterator II = BI->begin();
	Instruction *insn = &(*II);
	Constant *l_size = ConstantInt::get(Type::getInt32Ty(M.getContext()), local_size);
	Instruction *new_gb_ = new AllocaInst(arr, l_size, gb->getAlignment(), "hetero_local", insn);
	//new_gb_->dump();
	Value *Elts[] = {MDString::get(M.getContext(), new_gb_->getName())}; 
	MDNode *Node = MDNode::get(M.getContext(), Elts); 
	new_gb_->setMetadata("local",Node);

	Instruction *new_gb= CastInst::Create(Instruction::BitCast, new_gb_, 
		PointerType::get(arr,3), "hetero_local_cast", insn);
	//new_gb->dump();
	Value *Elts1[] = {MDString::get(M.getContext(), new_gb->getName())}; 
	MDNode *Node1 = MDNode::get(M.getContext(), Elts1); 
	new_gb->setMetadata("local_cast",Node1);

	edit_template_function(M,templat_copy,NF,gb,new_gb);
	templat->getParent()->getFunctionList().insert(templat, templat_copy);

	return templat_copy;
}

Function* HeterotbbTransform::write_new_hetero_kernel(Module &M, Function *f,int type){

	//	Function *f = CS.getCalledFunction();
	vector</*const*/ Type *> params;
	int temp_size=0;
	object_size_hetero=0;
	char buf[16];
	//	void kernel(class.name *,i32)
	const FunctionType *FTy = f->getFunctionType();
	//new function will have i8*
	params.push_back(PointerType::get(Type::getInt8Ty(M.getContext()),0));
	//i32
	//#ifndef IVB_64
	params.push_back(Type::getInt32Ty(M.getContext()));
	//#else
	//	params.push_back(Type::getInt64Ty(M.getContext()));
	//#endif
	/*const*/ Type *RetTy = FTy->getReturnType();
	//we need to get the size of the object to make copies. We just need to count the size of each of the elements
	if(type==2){	
		const PointerType *objtype=dyn_cast<PointerType>(f->arg_begin()->getType());
		const StructType *struct_obj=dyn_cast<StructType>(objtype->getContainedType(0));
		//struct_obj->dump();
		if(struct_obj){
			for(unsigned int i=0;i<struct_obj->getNumContainedTypes();i++){
				/*const*/ Type *ty=struct_obj->getContainedType(i);
				temp_size=TD->getTypeStoreSizeInBits(ty);
				if(temp_size==0){
					DEBUG(dbgs()<<"Size Can't be 0\n");
					ty->dump();
				}
				object_size_hetero+=temp_size;
			}
			object_size_hetero/=8;
			DEBUG(dbgs()<<"Object Size in Bytes: "<<object_size_hetero<<"\n");
			sprintf(buf,"%d",object_size_hetero);
		}
		else{
			DEBUG(dbgs()<<"Object is not of StructType\n Cannot determine size\n");
			objtype->dump();
		}
	}
	FunctionType *NFty = FunctionType::get(RetTy,params, false);
	Function *NF;
	if(type==1)
		NF= Function::Create(NFty, f->getLinkage(), f->getName()+"_hetero");
	else if(type==2)
		NF= Function::Create(NFty, f->getLinkage(), f->getName()+"_hetero_reduce_"+buf);

	NF->copyAttributesFrom(f);

	gen_opt_code_per_f(NF,f);
	f->getParent()->getFunctionList().insert(f, NF);

	object_sizes[NF]= object_size_hetero;

	return NF;

}
void HeterotbbTransform::rewrite_call_site(Module &M,  CallSite &CS,Function *NF,int type){
	//	create_hetero_clone_void(f);
	Instruction *OldCall = CS.getInstruction();
	Instruction *NewCall; // New Call Instruction created
	SmallVector<Value*, 16> Args; // Argument lists to the new call
	

	//DEBUG(dbgs() << "Old Call Instruction:");
	//OldCall->dump();

	// Any attributes (parameter attribute list PAL) of the 
	// parallel_for_hetero is 
#if defined(LLVM_3_2)
	SmallVector<AttributeWithIndex, 8> AttrVec; // Attributes list to the new call
	const AttrListPtr &OldCallPAL = CS.getAttributes();

	// Add any return attributes.
	Attributes attrs = OldCallPAL.getRetAttributes();
	if (attrs.hasAttributes()) 
		AttrVec.push_back(AttributeWithIndex::get(0, attrs));
#endif
	SmallVector<AttributeSet, 8> AttrVec;
	const AttributeSet &OldCallPAL = CS.getAttributes();
	// Add any return attributes.
	if (OldCallPAL.hasAttributes(AttributeSet::ReturnIndex))
		AttrVec.push_back(AttributeSet::get(NF->getContext(),
              OldCallPAL.getRetAttributes()));

	CallSite::arg_iterator AI = CS.arg_begin();
	Args.push_back(CS.getArgument(0)); // num_iters
	//Args.push_back(CS.getArgument(1));
	//params.push_back(CS.getArgument(1)->getType());

	//create a new cast from class_name to i8* before the old instruction site
	CastInst *StrucCast = CastInst::Create(Instruction::BitCast, CS.getArgument(1), 
		PointerType::get(Type::getInt8Ty(M.getContext()), 0), "temp_cast", OldCall);
	//push the type into the argument list
	Args.push_back(StrucCast);
	//push the function as third argument
	Args.push_back(NF);
	//NF->getType()->dump();
	
	
	vector</*const*/ Type *> params;
	const FunctionType *FTy = NF->getFunctionType();
	//#ifndef IVB_64
	params.push_back(Type::getInt32Ty(M.getContext()));
	/*#else
	params.push_back(Type::getInt64Ty(M.getContext()));
	#endif*/
	params.push_back(PointerType::get(Type::getInt8Ty(M.getContext()),0));
	params.push_back(NF->getType());
	//NF->dump();
	//NF->getType()->dump();
	//params.push_back(Type::getInt32Ty(M.getContext()));

	/*const*/ Type *RetTy = FTy->getReturnType();
	
	FunctionType *NFty = FunctionType::get(RetTy,params, false);
	//NF->getType()->dump();
	//NFty->dump();
	
	Constant *hetero_f_const;
	//if (hetero_f_const == NULL) {
	hetero_f_const = /*cast<Function>*/(M.getOrInsertFunction("offload", NFty));
	//}
	//hetero_f_const->dump();

	NewCall = CallInst::Create(hetero_f_const, Args, "", OldCall);
	//NewCall->dump();
	cast<CallInst>(NewCall)->setCallingConv(CS.getCallingConv());
	//cast<CallInst>(NewCall)->setAttributes(AttrListPtr::get(NF->getContext(),AttrVec));
	cast<CallInst>(NewCall)->setAttributes(AttributeSet::get(NF->getContext(), AttrVec));
	if (CallInst *c=dyn_cast<CallInst>(OldCall)){
		if(c->isTailCall()) cast<CallInst>(NewCall)->setTailCall();
	}
	//NewCall->dump();
	char buf[32];
	ConstantInt *ci;
	if (ci = dyn_cast<ConstantInt>(CS.getArgument(2))) {
		sprintf(buf,"%d",ci->getZExtValue());
	}
	else {
		DEBUG(dbgs() << "scheduler_hint is not supplied and assumed 0");
		sprintf(buf,"%d",0);
	}
	Value *e2[] = {MDString::get(M.getContext(),buf)}; 
	MDNode *n2 = MDNode::get(M.getContext(), e2); 
	NewCall->setMetadata("scheduler_hint",n2);
	
	if(type==2){//add meta data for reduction and rename it
		if (templat == NULL) {
			templat = M.getFunction("kernel_join_name");
			templat_join = M.getFunction("__join_reduce_hetero");
		}
		Function *join=get_join_func(M,CS);
		DEBUG(dbgs() << "join function=");
		join->dump();
		Value *Elts[] = {MDString::get(M.getContext(), join->getName())}; 
		MDNode *Node = MDNode::get(M.getContext(), Elts); 
		NewCall->setMetadata("join_cpu",Node);
		DEBUG(dbgs()<<"join found="<<join->getName()<<"\n");
		Function *Njoin=create_new_join(M,join);
		Value *Elts1[] = {MDString::get(M.getContext(), Njoin->getName())}; 
		MDNode *Node1 = MDNode::get(M.getContext(), Elts1); 
		NewCall->setMetadata("join_gpu",Node1);

		char buffer[32];
		sprintf(buffer,"%d",object_sizes[NF]);
		Value *Elts2[] = {MDString::get(M.getContext(),buffer )}; 
		MDNode *Node2 = MDNode::get(M.getContext(), Elts2); 
		NewCall->setMetadata("object_size",Node2);
	}
	//NewCall->stripPointerCasts();
	//DEBUG(dbgs() << "Newly created instruction:");
	//NewCall->dump();
}
void HeterotbbTransform::rewrite_invoke_site(Module &M,  CallSite &CS,Function *NF,int type){
	//	create_hetero_clone_void(f);
	Instruction *OldCall = CS.getInstruction();
	Instruction *NewCall; // New Call Instruction created
	SmallVector<Value*, 16> Args; // Argument lists to the new call
	
	//DEBUG(dbgs() << "Old Call Instruction:");
	//OldCall->dump();

	// Any attributes (parameter attribute list PAL) of the 
	// parallel_for_hetero is 
#if defined(LLVM_3_2)
	SmallVector<AttributeWithIndex, 8> AttrVec; // Attributes list to the new call
	const AttrListPtr &OldCallPAL = CS.getAttributes();

	// Add any return attributes.
	Attributes attrs = OldCallPAL.getRetAttributes();
	if (attrs.hasAttributes()) 
		AttrVec.push_back(AttributeWithIndex::get(0, attrs));
#endif
	SmallVector<AttributeSet, 8> AttrVec;
	const AttributeSet &OldCallPAL = CS.getAttributes();
	// Add any return attributes.
	if (OldCallPAL.hasAttributes(AttributeSet::ReturnIndex))
		AttrVec.push_back(AttributeSet::get(NF->getContext(),
              OldCallPAL.getRetAttributes()));

	CallSite::arg_iterator AI = CS.arg_begin();
	Args.push_back(CS.getArgument(0)); // num_iters
	//Args.push_back(CS.getArgument(1));
	//params.push_back(CS.getArgument(1)->getType());

	//create a new cast from class_name to i8* before the old instruction site
	CastInst *StrucCast = CastInst::Create(Instruction::BitCast, CS.getArgument(1), 
		PointerType::get(Type::getInt8Ty(M.getContext()), 0), "temp_cast", OldCall);
	//push the type into the argument list
	Args.push_back(StrucCast); // struct
	//push the function as third argument
	Args.push_back(NF);
	//NF->dump();
	//NF->getType()->dump();
	//Args.push_back(CS.getArgument(2));

	vector</*const*/ Type *> params;
	const FunctionType *FTy = NF->getFunctionType();
	//#ifndef IVB_64
	params.push_back(Type::getInt32Ty(M.getContext()));
	/*#else
	params.push_back(Type::getInt64Ty(M.getContext()));
	#endif*/
	params.push_back(PointerType::get(Type::getInt8Ty(M.getContext()),0));
	params.push_back(NF->getType());
	//params.push_back(Type::getInt32Ty(M.getContext()));

	/*const*/ Type *RetTy = FTy->getReturnType();

	FunctionType *NFty = FunctionType::get(RetTy,params, false);
	//NF->getType()->dump();
	//NFty->dump();

	Constant *hetero_f_const;
	//if (hetero_f_const == NULL) {
	hetero_f_const = /*cast<Function>*/(M.getOrInsertFunction("offload", NFty));
	//}
	//hetero_f_const->dump();

	NewCall = InvokeInst::Create(hetero_f_const,cast<InvokeInst>(OldCall)->getNormalDest(),cast<InvokeInst>(OldCall)->getUnwindDest(), Args, "", OldCall);
	cast<InvokeInst>(NewCall)->setCallingConv(CS.getCallingConv());
	//cast<InvokeInst>(NewCall)->setAttributes(AttrListPtr::get(NF->getContext(), AttrVec));
	cast<InvokeInst>(NewCall)->setAttributes(AttributeSet::get(NF->getContext(), AttrVec));
	//NewCall->dump();
	//NewCall = CallInst::Create(hetero_f_const, Args.begin(), Args.end(), "", OldCall);
	//NewCall->dump();
	//cast<CallInst>(NewCall)->setCallingConv(CS.getCallingConv());
	//cast<CallInst>(NewCall)->setAttributes(AttrListPtr::get(AttrVec.begin(), AttrVec.end()));
	//if (CallInst *c=dyn_cast<CallInst>(OldCall)){
	//	if(c->isTailCall()) cast<CallInst>(NewCall)->setTailCall();
	//}

	char buf[32];
	ConstantInt *ci;
	//DEBUG(dbgs() << "original scheduler_hint=");
	//CS.getArgument(2)->dump();
	if (ci = dyn_cast<ConstantInt>(CS.getArgument(2))) {
		DEBUG(dbgs() << "scheduler_hint=" << ci->getZExtValue());
		sprintf(buf,"%d",ci->getZExtValue());
	}
	else {
		DEBUG(dbgs() << "scheduler_hint is not supplied and assumed 0");
		sprintf(buf,"%d",0);
	}
	Value *e2[] = {MDString::get(M.getContext(),buf)}; 
	MDNode *n2 = MDNode::get(M.getContext(), e2); 
	NewCall->setMetadata("scheduler_hint",n2);
	
	if(type==2){//add meta data for reduction
		Function *join=get_join_func(M,CS);
		Value *Elts[] = {MDString::get(M.getContext(), join->getName())}; 
		MDNode *Node = MDNode::get(M.getContext(), Elts); 
		NewCall->setMetadata("join_cpu",Node);

		Function *Njoin=create_new_join(M,join);
		Value *Elts1[] = {MDString::get(M.getContext(), Njoin->getName())}; 
		MDNode *Node1 = MDNode::get(M.getContext(), Elts1); 
		NewCall->setMetadata("join_gpu",Node1);

		char buffer[32];
		sprintf(buffer,"%d",object_sizes[NF]);
		Value *Elts2[] = {MDString::get(M.getContext(),buffer )}; 
		MDNode *Node2 = MDNode::get(M.getContext(), Elts2); 
		NewCall->setMetadata("object_size",Node2);
	}
	//NewCall->stripPointerCasts();
	//DEBUG(dbgs() << "Newly created instruction:");
	//NewCall->dump();

}

/** returns the first calledfunction from paralle_for_hetero */
Function* HeterotbbTransform::get_hetero_func(Module &M, CallSite &CS){

	Function *f = CS.getCalledFunction();
	for (Function::iterator BBI = f->begin(), BBE = f->end(); BBI != BBE; ++BBI) {
		for (BasicBlock::iterator INSNI = BBI->begin(), INSNE = BBI->end(); INSNI != INSNE; ++INSNI) {
			if (isa<CallInst>(INSNI) || isa<InvokeInst>(INSNI)) {
				CallSite CI(cast<Instruction>(INSNI));
				//	CastInst *StrucCast = CastInst::Create(Instruction::BitCast, CI.getArgument(1), 
				//			PointerType::get(Type::getInt8Ty(M.getContext()), 0), "temp_cast", INSNI);

				return CI.getCalledFunction();
			}
		}
	}
	return NULL;
}

/** Assumes the join function as the seond function call in parallel-reduce-hetero implementation*/
/** very pathetic for now -- TODO */
Function* HeterotbbTransform::get_join_func(Module &M, CallSite &CS){

	Function *f = CS.getCalledFunction();
	int count=0;
	for (Function::iterator BBI = f->begin(), BBE = f->end(); BBI != BBE; ++BBI) {
		for (BasicBlock::iterator INSNI = BBI->begin(), INSNE = BBI->end(); INSNI != INSNE; ++INSNI) {
			if (isa<CallInst>(INSNI) || isa<InvokeInst>(INSNI)){
				count++;
				if(count==2){
					CallSite CI(cast<Instruction>(INSNI));			
					return CI.getCalledFunction();
				}
			}
		}
	}
	return NULL;
}

/** Added by Raj for adding inline attributes for functions called by kernel */
void HeterotbbTransform::add_inline_attributes(Function *F) {
	vector<Function *> trans_hetero_function_set;
	trans_hetero_function_set.push_back(F);
	unsigned int size = (unsigned int)trans_hetero_function_set.size();
	for(unsigned int i=0; i<size; i++) {
		Function *F = trans_hetero_function_set.at(i);
		//DEBUG(dbgs() << "Start Function=" << F->getName());
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
							//ff->removeFnAttr(Attributes::get(F->getContext(), Attributes::NoInline));
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

void HeterotbbTransform::rewrite_CPP(Module &M) {
	// Collect initial set of hetero functions
	vector<Instruction *> toDelete;
	DenseMap<Function*, Function *> FunctionMap[2];
	templat = NULL;
	templat_join = NULL;
	for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I){

		//if (isa<UnaryInst>(I))

		if (!I->isDeclaration()) {
			//DEBUG(dbgs() << "Func"<<*I<<"\n\n");
			for (Function::iterator BBI = I->begin(), BBE = I->end(); BBI != BBE; ++BBI) {
				for (BasicBlock::iterator INSNI = BBI->begin(), INSNE = BBI->end(); INSNI != INSNE; ++INSNI) {
					if (isa<CallInst>(INSNI) || isa<InvokeInst>(INSNI)) {
						//DEBUG(dbgs()<<*INSNI<<"\n");
						CallSite CI(cast<Instruction>(INSNI));
						//	errs()<<"INS"<<INSNI<<"\n";

						if (CI.getCalledFunction() == NULL) continue;
						int type =0;
						type = is_hetero_function_f(M, CI);//return a 1 for parallel_for and 2 for parallel_reduce
						if (type){ 

							//CI->dump();
							DEBUG(dbgs() << type<<":Hetero_fun "<<CI.getCalledFunction()->getName()<<"\n");
							//get the kernel body
							Function *f= get_hetero_func(M,CI);

							//@_ZNK12diff_testingclEi
							if(f!=NULL)//should never be null if the body is called.
							{
								DEBUG(dbgs() << " Kernel: "<<f->getName()<<"\n");
								//create new function for the kernel
								Function *nf=NULL; 
								if(FunctionMap[type-1].find(f)==FunctionMap[type-1].end()){
									nf = write_new_hetero_kernel(M,f,type);
									FunctionMap[type-1][f]=nf;
									DEBUG(dbgs() << " New Kernel Created: "<<nf->getName()<<" created\n\n");
									/* Added by Raj to add inline attributes recursively*/
									add_inline_attributes(nf);
								}
								else{
									nf=FunctionMap[type-1][f];
									DEBUG(dbgs() << " Kernel Exists: "<<nf->getName()<<" created\n\n");
								}
								//rewrite the hetero call site to offload
								if(isa<CallInst>(INSNI))
									rewrite_call_site(M,CI,nf,type);
								else if(isa<InvokeInst>(INSNI)) //must be invoke
									rewrite_invoke_site(M,CI,nf,type);
								else{
									DEBUG(dbgs()<<"ERROR\n");
								}

								
								//delete the old call site
								toDelete.push_back(INSNI);
							}
							else{
								DEBUG(dbgs() << " Parallel for/reduce hetero do not call any functions\n\n");
							}
						}
						/*
						#ifdef HETERO_GCD_H
						else if(hetero_function = get_hetero_function(M, CI)) {
						entry_hetero_function_set.insert(hetero_function);
						#ifdef HETERO_GCD_ALLOC_TO_MALLOC
						block2insnMap.insert(pair<Function *, Instruction *>(hetero_function, INSNI));
						#endif
						}
						#endif
						*/
					}
				}
			}
		}
	}
	while(!toDelete.empty()) {
		Instruction *g = toDelete.back();
		toDelete.pop_back();
		//		g->replaceAllUsesWith(UndefValue::get(g->getType()));
		g->eraseFromParent();
	}
	/* delete the template functions */
	if (templat_join != NULL) {
		templat_join->replaceAllUsesWith(UndefValue::get(templat_join->getType()));
		templat_join->eraseFromParent();
	}
	if (templat != NULL) {
		templat->replaceAllUsesWith(UndefValue::get(templat->getType()));
		templat->eraseFromParent();
	}
	//erase the annotation
	GlobalVariable *annot = M.getGlobalVariable("opencl_metadata");
	if (annot != NULL) annot->eraseFromParent();
	annot = M.getGlobalVariable("opencl_metadata_type");
	if (annot != NULL) annot->eraseFromParent();
	annot = M.getGlobalVariable("opencl_kernel_join_name_locals");
	if (annot != NULL) annot->eraseFromParent();
	annot = M.getGlobalVariable("opencl_kernel_join_name_parameters");
	if (annot != NULL) annot->eraseFromParent();
	annot = M.getGlobalVariable("opencl_kernel_join_name_local_arr");
	if (annot != NULL) annot->eraseFromParent();
}

bool HeterotbbTransform::runOnModule(Module &M) {

	bool localChange = true;
	TD = getAnalysisIfAvailable<DataLayout>();
	//hetero_f_const = NULL;
	rewrite_CPP(M);
	
#if 0
	Function *offload_func = M.getFunction("offload");
	for (Value::use_iterator i = offload_func->use_begin(), e = offload_func->use_end(); i != e; ++i) {
			Instruction *call;
			DEBUG(dbgs() << "Next _offload function:");
			i->dump();
			if ((call = dyn_cast<InvokeInst>(*i)) || (call = dyn_cast<CallInst>(*i))) {
				CallSite CI(cast<Instruction>(call));
				CI->dump();
			}
	}
#endif
	//M.dump();
	return localChange;
}
