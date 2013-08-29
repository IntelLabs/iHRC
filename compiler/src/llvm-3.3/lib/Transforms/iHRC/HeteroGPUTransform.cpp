//===-- HeteroGPUTransform.cpp - Generate GPU specific code  --------------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#include "llvm/Transforms/iHRC/HeteroGPUTransform.h"


bool CPU_FLAG;
static cl::opt<bool>
cpu_f("cpu-flag", cl::init(false), cl::Hidden,
  cl::desc("Pass 1 to generate kernel for CPU"));

bool BDW_FLAG;
static cl::opt<bool>
bdw_f("bdw-flag", cl::init(false), cl::Hidden,
  cl::desc("Generate Code for BDW"));

bool EAGER_FLAG;
static cl::opt<bool>
eager_f("eager-flag", cl::init(false), cl::Hidden,
  cl::desc("Eagerly convert loads to GPU space (default is lazy)"));

bool ENABLE_BYTE_RW;
static cl::opt<bool>
enable_byte_rw_f("enable-byte-rw", cl::init(false), cl::Hidden,
  cl::desc("Enable Byte Read and Write"));

// PER NEW FUNCTION
// generate new sgv and lgv variables for each function you generate
// @sgv = internal constant [4 x i8] "200\00"
// @lgv = internal constant [0 x i8] zeroinitializer
// @lvgv = internal constant [0 x i8*] zeroinitializer
// save these
void HeteroGPUTransform::gen_annotation_per_f(Function* F) {
	Module *M = F->getParent();	

#ifdef OLD_ANNOT
	string sgvString = "sgv";
	string fgvString = "fgv";
	string lvgvString = "lvgv";

	string flags = "";
	// 2 for __global read write for first parameter, other 0
	flags += "2000";

	// generate annotation string GV
	Constant *annoStr = ConstantArray::get(F->getContext(), flags.c_str() , true);
	GlobalVariable *sGV = new GlobalVariable(*M, annoStr->getType(), true, GlobalVariable::InternalLinkage, annoStr, sgvString);

	Constant *fileStr = ConstantArray::get(F->getContext(), "", false);
	GlobalVariable *fGV = new GlobalVariable(*M, fileStr->getType(), true, GlobalVariable::InternalLinkage, fileStr, fgvString);

	SmallVector<Constant*, 8> CList;
	ArrayType *lvGVType = ArrayType::get(PointerType::get(Type::getInt8Ty(F->getContext()),0),CList.size());
	Constant *lvGVStr = ConstantArray::get(lvGVType, CList.data(),  CList.size());
	GlobalVariable *lvGV = new GlobalVariable(*M, lvGVStr->getType(), true, GlobalVariable::InternalLinkage, lvGVStr, lvgvString);

	// Create the ConstantStruct that is the global annotation.
	Constant *Fields[5] = {
		ConstantExpr::getBitCast(F, PointerType::get(Type::getInt8Ty(F->getContext()),0)),
		ConstantExpr::getBitCast(sGV, PointerType::get(Type::getInt8Ty(F->getContext()),0)),
		ConstantExpr::getBitCast(fGV, PointerType::get(Type::getInt8Ty(F->getContext()),0)),
		ConstantExpr::getBitCast(lvGV, PointerType::get(Type::getInt8Ty(F->getContext()),0)),
		ConstantInt::get(Type::getInt32Ty(F->getContext()), 0)
	};
	Annotations.push_back(ConstantStruct::get(F->getContext(), Fields, 5, false));
#else

	Function::arg_iterator FI = F->arg_begin();
	FI++;
	Argument *ThreadId = &*FI;

	opencl_metadata_t f_metadata;
	/* store the kernel function */
	f_metadata.function = F;

	/* For now, there is no hint for vector, add more later */
	f_metadata.vec_type_hint = NULL;

	/* For now, there is no workgroup size hint */
	f_metadata.work_group_size_hint[0] = 0;
	f_metadata.work_group_size_hint[1] = 0;
	f_metadata.work_group_size_hint[2] = 0;
	f_metadata.work_group_size_hint[3] = 0;

	/* For now, there is no work_group_size definition */
	f_metadata.reqd_work_group_size[0] = 0;
	f_metadata.reqd_work_group_size[1] = 0;
	f_metadata.reqd_work_group_size[2] = 0;
	f_metadata.reqd_work_group_size[3] = 0;

	/* For now, there is no local variable */
	f_metadata.locals.clear();

	/* Fill in the arguments of the function as arguments */
	string params = "";
	if (ThreadId->getType()->getPrimitiveSizeInBits() == 64)
		params += "unsigned int __attribute__((address_space(1))) *, unsigned int, unsigned int, unsigned long";
	else 
		params += "unsigned int __attribute__((address_space(1))) *, unsigned int, unsigned int, unsigned int";
	f_metadata.param_types = params.c_str();
	opencl_metadata.push_back(f_metadata);
#endif

}



// add these at the end 
// %0 = type { i8*, i8*, i8*, i8*, i32 }
// @llvm.global.annotations = appending global [1 x %0] {} 
//@llvm.global.annotations = appending global [1 x %0] [%0 { i8* bitcast (void (i32 addrspace(1)*, i32 addrspace(1)*, i32)* @stream to i8*), 
//                           i8* getelementptr inbounds ([4 x i8]* @sgv, i32 0, i32 0), 
//                           i8* getelementptr inbounds ([0 x i8]* @fgv, i32 0, i32 0), 
//                           i8* bitcast ([0 x i8*]* @lvgv to i8*), i32 0 }], 
//                           section "llvm.metadata" ; <[1 x %0]*> [#uses=0]
void HeteroGPUTransform::gen_final_annotation(Module &M) {
#ifdef OLD_ANNOT
	Constant *Array = ConstantArray::get(ArrayType::get(Annotations[0]->getType(),
		Annotations.size()), Annotations);
	GlobalValue *gv = new GlobalVariable(M, Array->getType(), false,
		GlobalValue::AppendingLinkage, Array,
		"llvm.global.annotations");
	gv->setSection("llvm.metadata");	
#else
	LLVMContext &ctx = M.getContext(); 
	Constant *Zero = Constant::getNullValue(Type::getInt32Ty(ctx));
	Constant *TwoZeros[] = { Zero, Zero };

	/* Create the type for opecl_metadata_type */
	std::vector</*const*/ llvm::Type*> types;
	types.clear();
	types.push_back(Type::getInt8PtrTy(ctx));
	types.push_back(Type::getInt8PtrTy(ctx));
	types.push_back(ArrayType::get(IntegerType::get(ctx, 32), 4));
	types.push_back(ArrayType::get(IntegerType::get(ctx, 32), 4));
	types.push_back(Type::getInt8PtrTy(ctx));
	types.push_back(Type::getInt8PtrTy(ctx));
	StructType* MetadataType = StructType::create/*get*/(ctx, types, /*true*/"opencl_metadata_type");
	//M.addTypeName("opencl_metadata_type", MetadataType);

	vector<Constant*> mElements;
	for(unsigned int i = 0; i < opencl_metadata.size(); i++){
		vector<Constant*> sElements;

		/* Casted function */
		Constant *CastedFunction = ConstantExpr::getBitCast(cast<Constant>(opencl_metadata[i].function), Type::getInt8PtrTy(ctx));

		/* Null as vector_type_hints */
		Constant *vth = ConstantPointerNull::get(Type::getInt8PtrTy(ctx));

		/* work_group_size_hint is set*/
		vector<Constant*> aElements;
		aElements.push_back(ConstantInt::get(Type::getInt32Ty(ctx), opencl_metadata[i].work_group_size_hint[0], false));
		aElements.push_back(ConstantInt::get(Type::getInt32Ty(ctx), opencl_metadata[i].work_group_size_hint[1], false));
		aElements.push_back(ConstantInt::get(Type::getInt32Ty(ctx), opencl_metadata[i].work_group_size_hint[2], false));
		aElements.push_back(ConstantInt::get(Type::getInt32Ty(ctx), opencl_metadata[i].work_group_size_hint[3], false));
		Constant *wgsh = ConstantArray::get(ArrayType::get(Type::getInt32Ty(ctx), 4), aElements);

		/* reqd_qork_group_size is set*/
		aElements.clear();
		aElements.push_back(ConstantInt::get(Type::getInt32Ty(ctx), opencl_metadata[i].reqd_work_group_size[0], false));
		aElements.push_back(ConstantInt::get(Type::getInt32Ty(ctx), opencl_metadata[i].reqd_work_group_size[1], false));
		aElements.push_back(ConstantInt::get(Type::getInt32Ty(ctx), opencl_metadata[i].reqd_work_group_size[2], false));
		aElements.push_back(ConstantInt::get(Type::getInt32Ty(ctx), opencl_metadata[i].reqd_work_group_size[3], false));
		Constant *rwgs = ConstantArray::get(ArrayType::get(Type::getInt32Ty(ctx), 4),aElements);

		/* Set locals here */
		vector<Constant *> Locals;
		/*for(unsigned int j = 0; j < opencl_metadata[i].locals.size(); j++)
		Locals.push_back(ConstantExpr::getBitCast( ConstantExpr::getGetElementPtr(opencl_metadata[i].locals[j], TwoZeros, 1), Type::getInt8PtrTy(ctx)));    */
		Locals.push_back(ConstantPointerNull::get(Type::getInt8PtrTy(ctx)));
		Constant *localsConstant = ConstantArray::get(ArrayType::get(Type::getInt8PtrTy(ctx),Locals.size()), Locals);
		/* Global variable for locals declarations */
		GlobalValue *localsGlobal = new GlobalVariable(M, localsConstant->getType(), false, GlobalValue::AppendingLinkage, localsConstant,
			string("opencl_") + opencl_metadata[i].function->getName()+string("_locals"));
		localsGlobal->setSection("llvm.metadata");

		/* param_type string is created and its global variable is also created*/
		Constant *paramsConstant = ConstantDataArray::getString(ctx, StringRef(opencl_metadata[i].param_types), true);
		GlobalValue *paramsGlobal =new GlobalVariable(M, paramsConstant->getType(), false, GlobalValue::AppendingLinkage, paramsConstant,
			string("opencl_") + opencl_metadata[i].function->getName()+string("_parameters"));
		paramsGlobal->setSection("llvm.metadata");

		sElements.push_back(CastedFunction);
		sElements.push_back(vth);
		sElements.push_back(wgsh);
		sElements.push_back(rwgs);
		sElements.push_back(ConstantExpr::getBitCast(ConstantExpr::getGetElementPtr(localsGlobal, TwoZeros, 1), Type::getInt8PtrTy(ctx)));
		sElements.push_back(ConstantExpr::getGetElementPtr(paramsGlobal, TwoZeros, 2));

		/* push everything onto metadata elements */
		mElements.push_back(ConstantStruct::get(MetadataType, sElements));
	}

	/* Create the final opencl_metadata */
	Constant *Array = ConstantArray::get(ArrayType::get(MetadataType, mElements.size()), mElements);
	GlobalValue *gv = new GlobalVariable(M, Array->getType(), false, GlobalValue::AppendingLinkage, Array,"opencl_metadata");
	gv->setSection("llvm.metadata");
#endif
}

/* Rewrite an instruction based on the mapping on ValueMap */
void HeteroGPUTransform::rewrite_instruction(Instruction *OldInst, Instruction *NewInst, DenseMap<const Value *, Value *>& ValueMap) {
	int opIdx = 0;
	for (User::op_iterator i = OldInst->op_begin(), e = OldInst->op_end(); i != e; ++i, opIdx++) {
		Value *V = *i;
		if (ValueMap[V] != NULL) {
			NewInst->setOperand(opIdx, ValueMap[V]);
		}
	}
}

/* 
Given a cpuPtr,. this routine converts it to a gpuPtr
Perform svm operations for a load operation:
x = load mem
if type of mem is a ptr, then computer svm[mem] which is the gpu location of mem
replace the load with the new svm[mem]
Perform svm operations for a store operation:
store x, mem
if type of mem is a ptr, then computer svm[mem] which is the gpu location of mem
replace the store with the new svm[mem]
*/
Value *HeteroGPUTransform::cpuPtrToGPUPtr(Value *cpuptr, Function *oldF, IRBuilder<> &Builder, Value *svm, Value *svmInChar) {
	if (ENABLE_BYTE_RW) {
		///*const*/ Type *elemType = cast<PointerType>(cpuptr->getType())->getElementType();
		//int sizeInBytes = (TD->getTypeStoreSizeInBits(elemType))/8;

		//if (sizeInBytes < 4) { // handles char, short, etc.
		string name_orig = cpuptr->getName().str() + "_CASTINT";
		string name = name_orig+"_temp_";
		//CastInst *ptrInt = CastInst::Create(Instruction::PtrToInt, cpuptr, TD->getIntPtrType(oldF->getContext()), MANGLE(name), insertBefore);
		Value *ptrInt = Builder.CreatePtrToInt(cpuptr, TD->getIntPtrType(oldF->getContext()), MANGLE(name));

		// %Aptr2 = getelementptr inbounds i8* %svm, i32 %Aptr1
		name = cpuptr->getName().str() + "_GPURAW";
		//GetElementPtrInst *ptrGpuRaw = GetElementPtrInst::CreateInBounds(svmInChar, ptrInt, MANGLE(name) , insertBefore);
		Value *ptrGpuRaw = Builder.CreateInBoundsGEP(svmInChar, ptrInt, MANGLE(name));

		// %Aptr3  = bitcast i8* %Aptr2 to i32**
		name = cpuptr->getName().str() + "_GPU";
		/*CastInst *ptrGpu = CastInst::Create(Instruction::BitCast, ptrGpuRaw, 
		cpuptr->getType(), MANGLE(name), NewBB);*/
		//CastInst *ptrGpu = CastInst::Create(Instruction::BitCast, ptrGpuRaw, 
		//	PointerType::get((dyn_cast<PointerType>(cpuptr->getType())->getElementType()), 4), MANGLE(name), insertBefore);
		Value *ptrGpu = Builder.CreateBitCast(ptrGpuRaw, 
						PointerType::get((dyn_cast<PointerType>(cpuptr->getType())->getElementType()), 4), MANGLE(name));
		return ptrGpu;	
		//}
	} else {
		// %Aptr1 = ptrtoint i32** %Aptr to i32
		string name_orig = cpuptr->getName().str() + "_CASTINT";
		string name = name_orig+"_temp_";
		/*CastInst *ptrInt = CastInst::Create(Instruction::PtrToInt, cpuptr, 
		Type::getInt64Ty(oldF->getContext()), MANGLE(name), NewBB);*/


		//CastInst *ptrInt_temp = CastInst::Create(Instruction::PtrToInt, cpuptr, 
		//	Type::getInt32Ty(oldF->getContext()), MANGLE(name), insertBefore);
		Value *ptrInt_temp = Builder.CreatePtrToInt(cpuptr, 
			Type::getInt32Ty(oldF->getContext()), MANGLE(name));

		//Value *const2= ConstantInt::get(Type::getInt64Ty(oldF->getContext()),2);
		Value *const2= ConstantInt::get(ptrInt_temp->getType(),2 /*3*/);

		//BinaryOperator *ptrInt =BinaryOperator::CreateAShr(ptrInt_temp,const2, MANGLE(name_orig), insertBefore);
		Value *ptrInt = Builder.CreateAShr(ptrInt_temp,const2, MANGLE(name_orig));


		// %Aptr2 = getelementptr inbounds i8* %svm, i32 %Aptr1
		name = cpuptr->getName().str() + "_GPURAW";
		//GetElementPtrInst *ptrGpuRaw = GetElementPtrInst::CreateInBounds(svm, ptrInt, MANGLE(name), insertBefore);
		Value *ptrGpuRaw = Builder.CreateInBoundsGEP(svm, ptrInt, MANGLE(name));
		// %Aptr3  = bitcast i8* %Aptr2 to i32**
		name = cpuptr->getName().str() + "_GPU";
		/*CastInst *ptrGpu = CastInst::Create(Instruction::BitCast, ptrGpuRaw, 
		cpuptr->getType(), MANGLE(name), NewBB);*/
		//CastInst *ptrGpu = CastInst::Create(Instruction::BitCast, ptrGpuRaw, 
		//	PointerType::get((dyn_cast<PointerType>(cpuptr->getType())->getElementType()), 0), MANGLE(name), insertBefore);
		Value *ptrGpu = Builder.CreateBitCast(ptrGpuRaw, 
					PointerType::get((dyn_cast<PointerType>(cpuptr->getType())->getElementType()), 0), MANGLE(name));
		return ptrGpu;	
	}
}


/*
Given a gpuptr, this routine converts it to a cpu ptr
*/
Value *HeteroGPUTransform::gpuPtrToCPUPtr(Value *gpuptr, Function *oldF, IRBuilder<> &Builder, Value *negSvm, Value *negSvmInChar) {

	if (ENABLE_BYTE_RW) {
		// %Aptr1 = ptrtoint i32** %Aptr to i32
		string name = gpuptr->getName().str() + "_REV_CASTINT";

		/*CastInst *ptrInt = CastInst::Create(Instruction::PtrToInt, gpuptr, 
			Type::getInt64Ty(oldF->getContext()), MANGLE(name), NewBB);*/
		//CastInst *ptrInt = CastInst::Create(Instruction::PtrToInt, gpuptr, 
		//	TD->getIntPtrType(oldF->getContext()), MANGLE(name), insertBefore);
		Value *ptrInt = Builder.CreatePtrToInt(gpuptr, TD->getIntPtrType(oldF->getContext()), MANGLE(name));

		// %Aptr2 = getelementptr inbounds i8* %svm, i32 %Aptr1
		name = gpuptr->getName().str() + "_CPURAW";
		//GetElementPtrInst *ptrCpuRaw = GetElementPtrInst::CreateInBounds(negSvmInChar, ptrInt, MANGLE(name), insertBefore);
		Value *ptrCpuRaw = Builder.CreateInBoundsGEP(negSvmInChar, ptrInt, MANGLE(name));
		// %Aptr3  = bitcast i8* %Aptr2 to i32**
		name = gpuptr->getName().str() + "_CPU";
		//CastInst *ptrCpu = CastInst::Create(Instruction::BitCast, ptrCpuRaw, 
		//	gpuptr->getType(), MANGLE(name), insertBefore);
		Value *ptrCpu = Builder.CreateBitCast(ptrCpuRaw, 
			gpuptr->getType(), MANGLE(name));
		return ptrCpu;
	} else {

		///*const*/ Type *elemType = cast<PointerType>(gpuptr->getType())->getElementType();
		//int sizeInBytes = (TD->getTypeStoreSizeInBits(elemType))/8;

		//if (sizeInBytes < 4) { // handles char, short, etc.
		string name_orig = gpuptr->getName().str() + "_REV_CASTINT";
		string name = name_orig+"_temp_";
		//CastInst *ptrInt_temp = CastInst::Create(Instruction::PtrToInt, gpuptr, TD->getIntPtrType(oldF->getContext()), MANGLE(name), insertBefore);
		Value *ptrInt_temp = Builder.CreatePtrToInt(gpuptr, TD->getIntPtrType(oldF->getContext()), MANGLE(name));

		//Value *const2= ConstantInt::get(Type::getInt64Ty(oldF->getContext()),2);
		Value *const2= ConstantInt::get(ptrInt_temp->getType(),2 /*3*/);

		//BinaryOperator *ptrInt =BinaryOperator::CreateAShr(ptrInt_temp,const2, MANGLE(name_orig), insertBefore);
		Value *ptrInt = Builder.CreateAShr(ptrInt_temp,const2, MANGLE(name_orig));

		// %Aptr2 = getelementptr inbounds i8* %svm, i32 %Aptr1
		name = gpuptr->getName().str() + "_CPURAW";
		//GetElementPtrInst *ptrGpuRaw = GetElementPtrInst::CreateInBounds(negSvmInChar, ptrInt, MANGLE(name), insertBefore);
		Value *ptrGpuRaw = Builder.CreateInBoundsGEP(negSvmInChar, ptrInt, MANGLE(name));

		// %Aptr3  = bitcast i8* %Aptr2 to i32**
		name = gpuptr->getName().str() + "_CPU";
		/*CastInst *ptrGpu = CastInst::Create(Instruction::BitCast, ptrGpuRaw, 
		cpuptr->getType(), MANGLE(name), NewBB);*/
		//CastInst *ptrCpu = CastInst::Create(Instruction::BitCast, ptrGpuRaw, 
		//	PointerType::get((dyn_cast<PointerType>(gpuptr->getType())->getElementType()), 4), MANGLE(name), insertBefore);
		Value *ptrCpu = Builder.CreateBitCast(ptrGpuRaw, 
			PointerType::get((dyn_cast<PointerType>(gpuptr->getType())->getElementType()), 4), MANGLE(name));
		return ptrCpu;	
	}
}



// NF from F and its foo_content_t
// typedef struct foo_context_s {
//  int* A1; // XXX Do we need to put __global
//} foo_context_t;
//__kernel void fi0h_block_ocl(__global char *sharedArea,
//                          __private ptrdiff_t baseAddr,
//                          __private ptrdiff_t contextAddr){
//  
//  size_t i = get_global_id(0);
//  char *svm= (char *)sharedArea - baseAddr;
//  char *contextoffset = svm + contextAddr;
//  foo_context_t* pContext = (foo_context_t*)contextoffset;
//  int* A1 = (int *) pContext->A1; // do we need to declare A1 as __global in the typedef?
//  A1[i] = i;
//}

// generate code for NF from F
// define void @fi0h_block_ocl(i8 addrspace(1)* %svm, i32 %svmBase, i32 %contextAddr) nounwind {
//entry:
//  %call = tail call i32 @get_global_id(i32 0) nounwind ; <i32> [#uses=2]
//  %svmCast = bitcast i8 addrspace(1)* %sharedArea to i8*
//  %sub.ptr.neg = sub i32 0, %baseArea                 ; <i32> [#uses=1]
//  %svm = getelementptr inbounds i8* %svmCast, i32 %sub.ptr.neg ; <i8*> [#uses=1]

void HeteroGPUTransform::gen_lazy_opt_code_per_f (Function* NF, Function* F,int type){

	// Set the names of the parameters
	Function::arg_iterator DestI = NF->arg_begin();
	string nameString = "svmGPUBase";
	DestI->setName(MANGLE(nameString));
	Argument *svmArea = &(*DestI);
	DestI++;
	nameString = "svmCPUBase";
	DestI->setName(MANGLE(nameString)); 
	Argument *svmBase = &(*DestI);
	DestI++;
	nameString = "contextCPUAddr";
	DestI->setName(MANGLE(nameString));
	Argument *contextAddr = &(*DestI);

	DestI++;
	nameString = "globalContextCPUAddr";
	DestI->setName(MANGLE(nameString));
	Argument *globalContextAddr = &(*DestI);

/****/
	DestI++;
	nameString = "offset";
	DestI->setName(MANGLE(nameString));
	Argument *contextOffset = &(*DestI);
/****/
	Function::arg_iterator FI = F->arg_begin();
	Argument *FContext = &*FI; FI++;
	Argument *ThreadId = &*FI; FI++;
	Argument *GContext = &*FI;  // Global Context

	//DEBUG(dbgs()<<"old func=" << F->getName() << " new func=" << NF->getName() << "\n");

	// create a new basic block for the first basic block
	Function::const_iterator BI = F->begin();
	const BasicBlock& FStartBB = *BI;
	BasicBlock *NFStartBB = BasicBlock::Create(FStartBB.getContext(), "", NF);
	if (FStartBB.hasName()) NFStartBB->setName((MANGLE(FStartBB.getName())));

	// ; thread id
	// %call = call i32 @get_global_id(i32 0)  	
	/*const*/ IntegerType *intPtrType = TD->getIntPtrType(F->getContext());
	Function *getGlobalIdFunc = F->getParent()->getFunction(GET_GLOBAL_ID_NAME);
	if(getGlobalIdFunc == NULL) {
		Constant *getGlobalIdFuncConst = F->getParent()->getOrInsertFunction(GET_GLOBAL_ID_NAME,
			/* return type */     /*Type::getInt32Ty(F->getContext())*/ intPtrType,
			/* actual 1 type */   ThreadId->getType(),
			NULL);
		getGlobalIdFunc = cast<Function>(getGlobalIdFuncConst);
		getGlobalIdFunc->setCallingConv(F->getCallingConv());
	}
	
	Value *param = ConstantInt::get(ThreadId->getType()/*Type::getInt32Ty(F->getContext())*/ /*intPtrType*/, 0);
	Value *call;

	/* Sometimes the call is a 64-bit integer */
/****/
	nameString = "tid1";
/****/
	
	call = CallInst::Create(getGlobalIdFunc, param, MANGLE(nameString), NFStartBB);
	unsigned int tid_size = ThreadId->getType()->getPrimitiveSizeInBits();
	unsigned int get_global_id_return_size = getGlobalIdFunc->getReturnType()->getPrimitiveSizeInBits();
	if (tid_size > get_global_id_return_size) {
		CastInst *tidCast = CastInst::Create(Instruction::ZExt, call, 
			ThreadId->getType(), MANGLE(nameString), NFStartBB);
		call = tidCast; 
	}
	else if (tid_size < get_global_id_return_size) {
		//CallInst *call_ = CallInst::Create(getGlobalIdFuncConst, param, MANGLE(nameString), NFStartBB);
		CastInst *tidCast = CastInst::Create(Instruction::Trunc, call, 
			ThreadId->getType(), MANGLE(nameString), NFStartBB);
		call = tidCast; 
	}
	else  {
		//CallInst *call_ = CallInst::Create(getGlobalIdFuncConst, param, MANGLE(nameString), NFStartBB);
		//call = call_;
	}

	/*call->dump();
	CastInst *call32 = CastInst::Create(Instruction::BitCast, call, 
		Type::getInt32Ty(F->getContext()), MANGLE(nameString), NFStartBB);
	call32->dump();*/
	
	//call = CallInst::Create(getGlobalIdFuncConst, param, MANGLE(nameString), NFStartBB); 
	/****/
	//nameString = "offset_zext";
	//CastInst *contextOffsetZextInst = CastInst::Create(Instruction::ZExt, contextOffset, 
	//		call->getType(), MANGLE(nameString), NFStartBB);
	nameString = "tid";
	BinaryOperator *calle =BinaryOperator::Create(Instruction::Add,call,contextOffset/*ZextInst*/,MANGLE(nameString), NFStartBB);
	
	//GetElementPtrInst *dummyAdd =GetElementPtrInst::CreateInBounds(svmArea,calle,"", NFStartBB);
	//BinaryOperator *dummyNeg =BinaryOperator::CreateNeg(calle, "", NFStartBB);
	//GetElementPtrInst *svmAreaM =GetElementPtrInst::CreateInBounds(dummyAdd,dummyNeg,"", NFStartBB);
	
/****/
	//  ; cast svmArea to char * 
	//  %svmCast = bitcast i8 addrspace(1)* %svmArea to i8*
	// Remove this -- since this is not required
	//nameString = "svmGPUBaseCast";
	CastInst *svmCast = CastInst::Create(Instruction::BitCast, svmArea, 
		PointerType::get(Type::getInt8Ty(F->getContext()), 0), MANGLE(nameString), NFStartBB);

	//  ; 0-svmBase 
	//  %negSvmBase = sub i32 0, %svmBase
	nameString = "negSvmCPUBase_temp_";
	BinaryOperator *negSvmBase_temp =BinaryOperator::CreateNeg(svmBase, MANGLE(nameString), NFStartBB);

	
	GetElementPtrInst *svmInChar = NULL;
	GetElementPtrInst *svm = NULL;

	if (ENABLE_BYTE_RW) {
		nameString = "svmConstInChar";
		svmInChar = GetElementPtrInst::CreateInBounds(svmCast, negSvmBase_temp, MANGLE(nameString), NFStartBB);
	}
	else {
		nameString = "negSvmCPUBase_";
		Value *const2= ConstantInt::get(Type::getInt32Ty(F->getContext())/*intPtrType*/, 2/*3*/);
		BinaryOperator *negSvmBase =BinaryOperator::CreateAShr(negSvmBase_temp,const2, MANGLE(nameString), NFStartBB);
		//  ; svmCast-svmBase
		//  %svm = getelementptr inbounds i8* %svmCast, i32 %negSvmBase
		nameString = "svmConst";
		svm = GetElementPtrInst::CreateInBounds(/*svmCast*/ svmArea, negSvmBase, MANGLE(nameString), NFStartBB);
	}
	
#ifdef EXPLICIT_REWRITE
	DenseMap<const Value*, Value *> ValueMap;
#else
	ValueToValueMapTy ValueMap;
#endif
	
	//change context to local context by updating the this pointer for the reduce case
	BinaryOperator *context_;
	if(type==2){
		//string name=F->getName();
		//int size=atoi(name.substr(name.find_last_of("_hetero_reduce_")+1).c_str());
		nameString = "size_";
		int size;
		map<Function *, int>::iterator iter = kernel2sizeMap.find(F);
		if (iter != kernel2sizeMap.end()) {
			size = (*iter).second;
		}else size=0;
		//DEBUG(dbgs()<< "\nLooking for Function=" << F->getName()<< " Object Size="<<size << "\n");
	
		Value *const_size= ConstantInt::get(Type::getInt32Ty(F->getContext()),size);
		//offset = tid*size;
		BinaryOperator *add_offset =BinaryOperator::CreateMul(calle,const_size, MANGLE(nameString), NFStartBB);

		nameString = "contextCPU_temp_";
		//context = context_base+offset
		context_ =BinaryOperator::CreateAdd(contextAddr,add_offset, MANGLE(nameString), NFStartBB);
	}

	// ; convert contextAddr to a ptr
	// %pContext = inttoptr i32 %contextAddr to %struct.foo_context_s*
	nameString = "contextCPU_";
	Value *context; 
	if(type==2){
		context= CastInst::Create(Instruction::IntToPtr, context_, 
			FContext->getType()/*PointerType::get(intPtrType,0)*/, MANGLE(nameString), NFStartBB);
	}else{
		context = CastInst::Create(Instruction::IntToPtr, contextAddr, 
			FContext->getType()/*PointerType::get(intPtrType,0)*/, MANGLE(nameString), NFStartBB);
	}
	ValueMap[FContext] = context;

	// add the global context casting
	nameString = "globalContextCPU_";
	Value *gcntxt = CastInst::Create(Instruction::IntToPtr, globalContextAddr, 
			GContext->getType()/*PointerType::get(intPtrType,0)*/, MANGLE(nameString), NFStartBB);
	ValueMap[GContext] = gcntxt;

/****/
//	ValueMap[ThreadId] = call;
	ValueMap[ThreadId] = calle;

	ValueMap[&FStartBB] = NFStartBB;
#if EXPLICIT_REWRITE	
	/****/
	for (BasicBlock::const_iterator III = FStartBB.begin(), IIE = FStartBB.end(); III != IIE; ++III) {
		Instruction *NFInst = III->clone(/*F->getContext()*/);
		if (III->hasName()) NFInst->setName(MANGLE(III->getName()));
		const Instruction *FInst = &(*III);
		rewrite_instruction((Instruction *)FInst, NFInst, ValueMap);
		const PointerType *ptr;
		LoadInst *li;
		StoreInst *si;
		//cout << "Old Insn=";
		//FInst->dump();
		if ((li = dyn_cast<LoadInst>(NFInst)) && 
			((ptr = dyn_cast<PointerType>(li->getPointerOperand()->getType())) && (ptr->getAddressSpace() != 3)))
			li->setOperand(0, cpuPtrToGPUPtr(li->getPointerOperand(), F, NFStartBB, svm, svmInChar));
		else if ((si = dyn_cast<StoreInst>(NFInst)) && 
				((ptr = dyn_cast<PointerType>(si->getPointerOperand()->getType())) && (ptr->getAddressSpace() != 3))) 
			si->setOperand(1, cpuPtrToGPUPtr(si->getPointerOperand(), F, NFStartBB, svm, svmInChar));
		NFStartBB->getInstList().push_back(NFInst);
		//cout << "New Insn=";
		//NFInst->dump();
		ValueMap[III] = NFInst;
	}
	//ValueMap[&FStartBB] = NFStartBB;

	// Rest of the basic block rewrite
	BI++;
	for (Function::const_iterator BE = F->end();BI != BE; ++BI) {
		const BasicBlock &FBB = *BI;
		BasicBlock *NFBB = BasicBlock::Create(FBB.getContext(), "", NF);
		if (FBB.hasName()) NFBB->setName(MANGLE(FBB.getName()));
		for (BasicBlock::const_iterator II = FBB.begin(), IE = FBB.end(); II != IE; ++II) {
			Instruction *NFInst = II->clone(/*F->getContext()*/);
			if (II->hasName()) NFInst->setName(MANGLE(II->getName()));
			const Instruction *FInst = &(*II);
			rewrite_instruction((Instruction *)FInst, NFInst, ValueMap);
			const PointerType *ptr;
			LoadInst *li;
			StoreInst *si;
			if ((li = dyn_cast<LoadInst>(NFInst)) && 
				((ptr = dyn_cast<PointerType>(li->getPointerOperand()->getType())) && (ptr->getAddressSpace() != 3)))
				li->setOperand(0, cpuPtrToGPUPtr(li->getPointerOperand(), F, NFBB, svm, svmInChar));
			else if ((si = dyn_cast<StoreInst>(NFInst)) && 
				((ptr = dyn_cast<PointerType>(si->getPointerOperand()->getType())) && (ptr->getAddressSpace() != 3))) 
				si->setOperand(1, cpuPtrToGPUPtr(si->getPointerOperand(), F, NFBB, svm, svmInChar));
			NFBB->getInstList().push_back(NFInst);
			ValueMap[II] = NFInst;
		}
		ValueMap[&FBB] = NFBB;
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
	//call->replaceAllUsesWith(call32);
#else
	CloneFunctionWithExistingBBInto(NF, NFStartBB, F, ValueMap);
	for (Function::iterator b = NF->begin(), be = NF->end(); b != be; ++b) {
		for (BasicBlock::iterator I = b->begin(), E = b->end(); I != E; ++I) {
			const PointerType *ptr;
			LoadInst *li;
			StoreInst *si;
			Instruction *insn = &*I;
			if ((li = dyn_cast<LoadInst>(insn)) && 
				((ptr = dyn_cast<PointerType>(li->getPointerOperand()->getType())) && (ptr->getAddressSpace() != 3))) {
					IRBuilder<> Builder(b);
					Builder.SetInsertPoint(li);
					Value *liGPU = cpuPtrToGPUPtr(li->getPointerOperand(), F, Builder, svm, svmInChar);
					li->setOperand(0, liGPU);
			}
			else if ((si = dyn_cast<StoreInst>(insn)) && 
				((ptr = dyn_cast<PointerType>(si->getPointerOperand()->getType())) && (ptr->getAddressSpace() != 3))) {
					IRBuilder<> Builder(b);
					Builder.SetInsertPoint(si);
					Value *siGPU = cpuPtrToGPUPtr(si->getPointerOperand(), F, Builder, svm, svmInChar);
					si->setOperand(1, siGPU);
			}
		}
	}
#endif
}
//added by Deepak.
//The cpu does not need address translations. So generate code without them.
void HeteroGPUTransform::gen_cpu_code_per_f (Function* NF, Function* F){
	// Get the names of the parameters for old function
	Function::arg_iterator DestI = NF->arg_begin();
	string nameString = "svmGPUBase";
	DestI->setName(MANGLE(nameString));
	Argument *svmArea = &(*DestI);
	DestI++;
	nameString = "svmCPUBase";
	DestI->setName(MANGLE(nameString)); 
	Argument *svmBase = &(*DestI);
	DestI++;
	nameString = "contextCPUAddr";
	DestI->setName(MANGLE(nameString));
	Argument *contextAddr = &(*DestI);

	DestI++;
	nameString = "globalContextCPUAddr";
	DestI->setName(MANGLE(nameString));
	Argument *globalContextAddr = &(*DestI);

/****/
	DestI++;
	nameString = "offset";
	DestI->setName(MANGLE(nameString));
	Argument *contextOffset = &(*DestI);
/****/
	Function::arg_iterator FI = F->arg_begin();
	Argument *FContext = &*FI; FI++;
	Argument *ThreadId = &*FI; FI++;
	Argument *GContext = &*FI;  // Global Context

	// create a new basic block for the first basic block
	Function::const_iterator BI = F->begin();
	const BasicBlock& FB = *BI;
	BasicBlock *NFBB = BasicBlock::Create(FB.getContext(), "", NF);
	if (FB.hasName()) NFBB->setName((MANGLE(FB.getName())));

	// ; thread id
	// %call = call i32 @get_global_id(i32 0)  	
	/*const*/ IntegerType *intPtrType = TD->getIntPtrType(F->getContext());
	Constant *getGlobalIdFuncConst = F->getParent()->getOrInsertFunction(GET_GLOBAL_ID_NAME,
		/* return type */     /*Type::getInt32Ty(F->getContext())*/ intPtrType,
		/* actual 1 type */   ThreadId->getType(),
		NULL);
	Function *getGlobalIdFunc = cast<Function>(getGlobalIdFuncConst);
	getGlobalIdFunc->setCallingConv(F->getCallingConv());
	Value *param = ConstantInt::get(ThreadId->getType()/*Type::getInt32Ty(F->getContext())*/ /*intPtrType*/, 0);
	Value *call;

	/* Sometimes the call is a 64-bit integer */
/****/
	nameString = "tid1";
/****/
	/*param->dump();
	ThreadId->dump();
	getGlobalIdFunc->getReturnType()->dump();*/

	unsigned int tid_size = ThreadId->getType()->getPrimitiveSizeInBits();
	unsigned int get_global_id_return_size = getGlobalIdFunc->getReturnType()->getPrimitiveSizeInBits();
	if (tid_size > get_global_id_return_size) {
		CallInst *call_ = CallInst::Create(getGlobalIdFuncConst, param, MANGLE(nameString), NFBB);
		CastInst *tidCast = CastInst::Create(Instruction::ZExt, call_, 
			ThreadId->getType(), MANGLE(nameString), NFBB);
		call = tidCast; 
	}
	else if (tid_size < get_global_id_return_size) {
		CallInst *call_ = CallInst::Create(getGlobalIdFuncConst, param, MANGLE(nameString), NFBB);
		CastInst *tidCast = CastInst::Create(Instruction::Trunc, call_, 
			ThreadId->getType(), MANGLE(nameString), NFBB);
		call = tidCast; 
	}
	else  {
		CallInst *call_ = CallInst::Create(getGlobalIdFuncConst, param, MANGLE(nameString), NFBB);
		call=call_;
	}
	//call = CallInst::Create(getGlobalIdFuncConst, param, MANGLE(nameString), NFStartBB); 
	/****/
	//nameString = "offset_zext";
	//CastInst *contextOffsetZextInst = CastInst::Create(Instruction::ZExt, contextOffset, 
	//		call->getType(), MANGLE(nameString), NFStartBB);
	nameString = "tid";
	BinaryOperator *calle =BinaryOperator::Create(Instruction::Add,call,contextOffset/*ZextInst*/,MANGLE(nameString), NFBB);
/****/
	//  ; cast svmArea to char * 
	//  %svmCast = bitcast i8 addrspace(1)* %svmArea to i8*
	// Remove this -- since this is not required
	//nameString = "svmGPUBaseCast";
	//CastInst *svmCast = CastInst::Create(Instruction::BitCast, svmArea, 
	//	PointerType::get(Type::getInt8Ty(F->getContext()), 0), MANGLE(nameString), NFStartBB);

	// ; convert contextAddr to a ptr
	// %pContext = inttoptr i32 %contextAddr to %struct.foo_context_s*
	nameString = "contextCPU";
	CastInst *context = CastInst::Create(Instruction::IntToPtr, contextAddr, 
		FContext->getType()/*PointerType::get(intPtrType,0)*/, MANGLE(nameString), NFBB);

	nameString = "gContextCPU";
	CastInst *gCntxt = CastInst::Create(Instruction::IntToPtr, globalContextAddr, 
		GContext->getType()/*PointerType::get(intPtrType,0)*/, MANGLE(nameString), NFBB);

#ifdef EXPLICIT_REWRITE
	DenseMap<const Value*, Value *> ValueMap;
#else
	ValueToValueMapTy ValueMap;
#endif

	ValueMap[FContext] = context;
	ValueMap[GContext] = gCntxt;
/****/
//	ValueMap[ThreadId] = call;
	ValueMap[ThreadId] = calle;
/****/
	ValueMap[&FB] = NFBB;

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
	//NF->dump();
#else
	CloneFunctionWithExistingBBInto(NF, NFBB, F, ValueMap);
#endif
}

//The cpu does not need address translations. So generate code without them.
void HeteroGPUTransform::gen_bdw_code_per_f (Function* NF, Function* F){
	// Get the names of the parameters for old function
	Function::arg_iterator DestI = NF->arg_begin();
	string nameString = "svmGPUBase";
	DestI->setName(MANGLE(nameString));
	Argument *svmArea = &(*DestI);
	DestI++;
	nameString = "svmCPUBase";
	DestI->setName(MANGLE(nameString)); 
	Argument *svmBase = &(*DestI);
	DestI++;
	nameString = "contextCPUAddr";
	DestI->setName(MANGLE(nameString));
	Argument *contextAddr = &(*DestI);

	DestI++;
	nameString = "globalContextCPUAddr";
	DestI->setName(MANGLE(nameString));
	Argument *globalContextAddr = &(*DestI);

/****/
	DestI++;
	nameString = "offset";
	DestI->setName(MANGLE(nameString));
	Argument *contextOffset = &(*DestI);
/****/

	Function::arg_iterator FI = F->arg_begin();
	Argument *FContext = &*FI; FI++;
	Argument *ThreadId = &*FI; FI++;
	Argument *GContext = &*FI;  // Global Context

	// create a new basic block for the first basic block
	Function::const_iterator BI = F->begin();
	const BasicBlock& FB = *BI;
	BasicBlock *NFBB = BasicBlock::Create(FB.getContext(), "", NF);
	if (FB.hasName()) NFBB->setName((MANGLE(FB.getName())));

	// ; thread id
	// %call = call i32 @get_global_id(i32 0)  	
	/*const*/ IntegerType *intPtrType = TD->getIntPtrType(F->getContext());
	Constant *getGlobalIdFuncConst = F->getParent()->getOrInsertFunction(GET_GLOBAL_ID_NAME,
		/* return type */     /*Type::getInt32Ty(F->getContext())*/ intPtrType,
		/* actual 1 type */   ThreadId->getType(),
		NULL);
	Function *getGlobalIdFunc = cast<Function>(getGlobalIdFuncConst);
	getGlobalIdFunc->setCallingConv(F->getCallingConv());
	Value *param = ConstantInt::get(ThreadId->getType()/*Type::getInt32Ty(F->getContext())*/ /*intPtrType*/, 0);
	Value *call;

	/* Sometimes the call is a 64-bit integer */
/****/
	nameString = "tid1";
/****/
	/*param->dump();
	ThreadId->dump();
	getGlobalIdFunc->getReturnType()->dump();*/

	unsigned int tid_size = ThreadId->getType()->getPrimitiveSizeInBits();
	unsigned int get_global_id_return_size = getGlobalIdFunc->getReturnType()->getPrimitiveSizeInBits();
	if (tid_size > get_global_id_return_size) {
		CallInst *call_ = CallInst::Create(getGlobalIdFuncConst, param, MANGLE(nameString), NFBB);
		CastInst *tidCast = CastInst::Create(Instruction::ZExt, call_, 
			ThreadId->getType(), MANGLE(nameString), NFBB);
		call = tidCast; 
	}
	else if (tid_size < get_global_id_return_size) {
		CallInst *call_ = CallInst::Create(getGlobalIdFuncConst, param, MANGLE(nameString), NFBB);
		CastInst *tidCast = CastInst::Create(Instruction::Trunc, call_, 
			ThreadId->getType(), MANGLE(nameString), NFBB);
		call = tidCast; 
	}
	else  {
		CallInst *call_ = CallInst::Create(getGlobalIdFuncConst, param, MANGLE(nameString), NFBB);
		call=call_;
	}
	//call = CallInst::Create(getGlobalIdFuncConst, param, MANGLE(nameString), NFStartBB); 
	/****/
	//nameString = "offset_zext";
	//CastInst *contextOffsetZextInst = CastInst::Create(Instruction::ZExt, contextOffset, 
	//		call->getType(), MANGLE(nameString), NFStartBB);
	nameString = "tid";
	BinaryOperator *calle =BinaryOperator::Create(Instruction::Add,call,contextOffset/*ZextInst*/,MANGLE(nameString), NFBB);
/****/
	//  ; cast svmArea to char * 
	//  %svmCast = bitcast i8 addrspace(1)* %svmArea to i8*
	// Remove this -- since this is not required
	//nameString = "svmGPUBaseCast";
	//CastInst *svmCast = CastInst::Create(Instruction::BitCast, svmArea, 
	//	PointerType::get(Type::getInt8Ty(F->getContext()), 0), MANGLE(nameString), NFStartBB);

	// ; convert contextAddr to a ptr
	// %pContext = inttoptr i32 %contextAddr to %struct.foo_context_s*
	nameString = "contextCPU";
	CastInst *context = CastInst::Create(Instruction::IntToPtr, contextAddr, 
		FContext->getType()/*PointerType::get(intPtrType,0)*/, MANGLE(nameString), NFBB);

	nameString = "gContextCPU";
	CastInst *gCntxt = CastInst::Create(Instruction::IntToPtr, globalContextAddr, 
		GContext->getType()/*PointerType::get(intPtrType,0)*/, MANGLE(nameString), NFBB);

#ifdef EXPLICIT_REWRITE
	DenseMap<const Value*, Value *> ValueMap;
#else
	ValueToValueMapTy ValueMap;
#endif

	ValueMap[FContext] = context;
	ValueMap[GContext] = gCntxt;
/****/
//	ValueMap[ThreadId] = call;
	ValueMap[ThreadId] = calle;
/****/
	ValueMap[&FB] = NFBB;

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
	//NF->dump();
#else
	CloneFunctionWithExistingBBInto(NF, NFBB, F, ValueMap);
#endif
}



// PER NEW FUNCTION
//(Step 2)create a new function with changed parameter type as
void HeteroGPUTransform::gen_new_function_per_f(Function* F,int type) {

	const FunctionType *FTy = F->getFunctionType();

	// Process the parameters
	vector</*const*/ Type*> Params;

	// Fill in new parameter types for __global char *, const ptrdiff_t
	// Add any new function attributes.
	// define void @fi0h_block_ocl(i8 addrspace(1)* %sharedArea, i32 %baseAddr, i32 %contextAddr) nounwind {
	
	/*const*/ IntegerType *intPtrType = TD->getIntPtrType(F->getContext());
	Function::arg_iterator FI = F->arg_begin();
	FI++;
	Argument *ThreadId = &*FI;
	

	// svm gpu base
	//Params.push_back(PointerType::get(Type::getInt32Ty(F->getContext()), 1));
	Params.push_back(PointerType::get(Type::getInt32Ty(F->getContext()), 1));


	if (BDW_FLAG) {
		Params.push_back(intPtrType); // cpu base
		Params.push_back(intPtrType); // context
		Params.push_back(intPtrType); // global context
	}
	else if (CPU_FLAG) {
		Params.push_back(intPtrType); // cpu base
		Params.push_back(intPtrType); // context
		Params.push_back(intPtrType); // global context
	}
	else {
		//Params.push_back(PointerType::get(intPtrType, 1));
		Params.push_back(Type::getInt32Ty(F->getContext())); // cpu_base
		//Params.push_back(intPtrType); 
		Params.push_back(Type::getInt32Ty(F->getContext())); // user_context
		//Params.push_back(intPtrType); 
		Params.push_back(Type::getInt32Ty(F->getContext()));  // global _context
		//Params.push_back(intPtrType); 
	}
	
	/****/
	Params.push_back(/*Type::getInt32Ty(F->getContext())*/ThreadId->getType()); //offset
	/****/

	/*const*/ Type *RetTy = FTy->getReturnType();
	string name = MANGLE(F->getName().str());

	// Construct the new function type using the new arguments.
	FunctionType *NFTy = FunctionType::get(RetTy, Params, false);

	// Create the new function body and insert it into the module.
	Function *NF = Function::Create(NFTy, F->getLinkage(), name);

	//NF->copyAttributesFrom(F);

	// generate code for NF from F
	/*if (OPTIMIZE_FLAG)
		//gen_high_opt_code_per_f (NF, F);
	else */
		if (BDW_FLAG) {
			gen_bdw_code_per_f (NF, F);
		}
		else if(CPU_FLAG) {
			gen_cpu_code_per_f (NF, F);
		}
		else {
				gen_lazy_opt_code_per_f (NF, F,type);
		}

	// append the function to module now
	F->getParent()->getFunctionList().insert(F, NF);

	//NF->dump();

	// Add new annotations for sgv, lgv, lvgv and update llvm.global.annotations
#ifdef NEED_ANNOT
	gen_annotation_per_f(NF);
#endif

	DEBUG(dbgs() << "New function created=" << NF->getName());
	// add to list
	gen_function_set.insert(NF);

}




/* Delete non hetero function declarations/definitions */
void HeteroGPUTransform::delete_non_hetero_funcs(Module &M) {
	vector<Function *> toDelete;
	map<Function *, unsigned> repeatNo;
	for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I)
		if (!((I->isDeclaration() && 
			((I->getName().compare(GET_GLOBAL_ID_NAME)  == 0) || 
			(isBuiltinFunction(I)))) || 
			(gen_function_set.find(I) != gen_function_set.end())))  {
				toDelete.push_back(I);
				repeatNo.insert(make_pair(I, 0));
		}

		// Can have problems for recursive functions
		// TODO -- check LLVM if it has a clean way of deleting function
		// Quick fix: if a function is three times in the queue then let it be
		while(!toDelete.empty()) {
			vector<Function *>::iterator it=toDelete.begin();
			Function *f = toDelete.back();
			toDelete.pop_back();
			//errs() << "\n ToDelete Function=" << f->getName();
			int num_use=0;
			for (Value::use_iterator i = f->use_begin(), e = f->use_end(); i!= e; ++i, num_use++);
			if (num_use == 0) { 
				//errs() << "\nDelete";
				f->eraseFromParent();
			}
			else{
				std::map<Function *, unsigned>::iterator iter = repeatNo.find(f);
				unsigned repeat = (unsigned)iter->second;
				//errs() << "\nRepeat=" << repeat;
				if (repeat <= 3) {
					toDelete.insert(toDelete.begin(), f);
					repeatNo.erase(iter);
					repeatNo.insert(make_pair(f, repeat+1));
				}
			}
		}
}
bool  HeteroGPUTransform::Get_Hetero_Functions(Module &M,GlobalVariable *annot){

//		for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I){
//			Function *f = cast<Function>(I);
//			DEBUG(dbgs()<<f->getName()<<"\n");
//		}
//		ConstantArray *Array = dyn_cast<ConstantArray>(annot->getInitializer());
#ifdef OLD_CODE
		ConstantStruct *Struct = dyn_cast<ConstantStruct>(annot->getInitializer());

		for (unsigned i = 0, e = Struct->getType()->getNumElements(); i != e; ++i) {
			ConstantStruct *element = cast<ConstantStruct>(Struct->getOperand(i));
			ConstantDataArray *Array = dyn_cast<ConstantDataArray>(element->getOperand(0));

			string str= Array->getAsString();
		//Some how getAsString puts an extraneous ' '(space) in the end of the string.
		//This is causing a problem in M.getFunction()
		//Removing the last space
			//string func_name= str.substr(0,str.length()-1);
			if(str.length()==0){
				DEBUG(dbgs()<<"Error in function name extraction \n");
				return false;
			}
			//DEBUG(dbgs()<<func_name<<"\n");
			Function *hetero_f=M.getFunction(str.c_str());
			if(hetero_f==NULL){
				DEBUG(dbgs()<<"ERROR in getFunction from module"<<"\n");
				return false;
			}
			DEBUG(dbgs()<<"Hetro_functions: "<<hetero_f->getName()<<"\n");
			hetero_function_set.insert(hetero_f);
		}
#endif
		if (annot != NULL) {
			ConstantDataArray *Array = dyn_cast<ConstantDataArray>(annot->getInitializer());
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
		return true;
}

/* Find join method and perform pointer transformation for them */
void HeteroGPUTransform::find_join_methods(Module &M) {
	Function *offload_func = M.getFunction(OFFLOAD_NAME);
	if (offload_func != NULL) {
		for (Value::use_iterator i = offload_func->use_begin(), e = offload_func->use_end(); i != e; ++i) {
			Instruction *call;
			if ((call = dyn_cast<InvokeInst>(*i)) || (call = dyn_cast<CallInst>(*i))) {
				CallSite CI(cast<Instruction>(call));
				int type;
				Function *kernel = get_hetero_function_f(M, CI, &type);
				MDNode *gpuMDNode = call->getMetadata("join_gpu");
				MDNode *sizeMDNode = call->getMetadata("object_size");
				MDString *gpuMDString, *sizeMDString; 
				if (gpuMDNode != NULL && sizeMDNode != NULL && 
					(gpuMDString = dyn_cast<MDString>(gpuMDNode->getOperand(0))) &&
					(sizeMDString = dyn_cast<MDString>(sizeMDNode->getOperand(0)))) {
						//gpuMDString->dump();
						//sizeMDString->dump();
					Function *gpu_join_func = M.getFunction(gpuMDString->getString().str().c_str());
					int size = atoi(sizeMDString->getString().str().c_str());
					//DEBUG(dbgs() << "\nSet Function=" << kernel->getName() << " Size=" << size << "\n");
					join_function_set.insert(gpu_join_func);
					kernel2sizeMap.insert(pair<Function *, int>(kernel, size));
				}
			} 
		}
	}
}

bool HeteroGPUTransform::runOnModule(Module &M) {
	bool localChange = false;

	/* Initialize the target architecture */
	TD = getAnalysisIfAvailable<DataLayout>();
	CPU_FLAG = cpu_f;
	EAGER_FLAG = eager_f;
	BDW_FLAG = bdw_f;
	ENABLE_BYTE_RW = enable_byte_rw_f;

	//Transformations for parallel_for_hetero
	//	GlobalVariable *annot = M.getGlobalVariable("llvm.hetero.annotations");
	GlobalVariable *annot = M.getGlobalVariable("hetero.string.annotations");
	if (annot != NULL) {
		if(!Get_Hetero_Functions(M,annot)) 
			DEBUG(dbgs() << "\nParallel_for_hetero annotation exists but have no functions\n");
		/* Delete the annotation and create a new one for backend */
		annot->eraseFromParent();

		if (!hetero_function_set.empty()) {
			localChange = true;	
			// for each hetero function create a new function prototype and add the function
			for (set<Function *>::iterator it = hetero_function_set.begin(), itt=hetero_function_set.end(); it != itt; it++) 
				gen_new_function_per_f (*it,1);
			// Add final annotations
			//gen_final_annotation(M);
			// delete non-gen functions
			//delete_non_hetero_funcs(M);
			//M.dump();
		}
	}
	//else DEBUG(dbgs() << "NO PARALLEL_FOR_HETERO ANNOTATIONS FOUND\n");

	find_join_methods(M);

	hetero_function_set.clear();
	//Transformations for parallel_reduce_hetero
	annot = M.getGlobalVariable("reduce_hetero.string.annotations");
	if (annot != NULL) {
		if(!Get_Hetero_Functions(M,annot))
			DEBUG(dbgs() << "\nParallel_reduce_hetero annotation exists but have no functions\n");
		/* Delete the annotation and create a new one for backend */
		annot->eraseFromParent();

		if (!hetero_function_set.empty()) {
			localChange = true;	
			// for each hetero function create a new function prototype and add the function
			for (set<Function *>::iterator it = hetero_function_set.begin(), itt=hetero_function_set.end(); it != itt; it++) 
				gen_new_function_per_f (*it,2);
			// Add final annotations
			//gen_final_annotation(M);
			// delete non-gen functions
			//delete_non_hetero_funcs(M);
			//M.dump();
		}
	}
	//else DEBUG(dbgs() << "NO PARALLEL_REDUCE_HETERO ANNOTATIONS FOUND\n");

	hetero_function_set.clear();
	annot = M.getGlobalVariable("join_hetero.string.annotations");
	if (annot != NULL) {
		if(!Get_Hetero_Functions(M,annot)) 
			DEBUG(dbgs() << "\nParallel_join_hetero annotation exists but have no functions\n");
		/* Delete the annotation and create a new one for backend */
		annot->eraseFromParent();

		if (!hetero_function_set.empty()) {
			localChange = true;	
			// for each hetero function create a new function prototype and add the function
			for (set<Function *>::iterator it = hetero_function_set.begin(), itt=hetero_function_set.end(); it != itt; it++) 
				gen_new_function_per_f (*it,1);
			// Add final annotations
			//gen_final_annotation(M);
			// delete non-gen functions
			//delete_non_hetero_funcs(M);
			//M.dump();
		}
	}
	//else DEBUG(dbgs() << "\nNO PARALLEL_JOIN_HETERO ANNOTATIONS FOUND\n");

	/*join_function_set.clear();
	find_join_methods(M);
	//Transformations for parallel_reduce_hetero
	if (!join_function_set.empty()) {
			localChange = true;	
			// for each hetero function create a new function prototype and add the function
			for (set<Function *>::iterator it = join_function_set.begin(), itt=join_function_set.end(); it != itt; it++) 
				gen_new_function_per_f (*it,1);
			// Add final annotations
			//gen_final_annotation(M);
			// delete non-gen functions
			//delete_non_hetero_funcs(M);
			//M.dump();
		}
	}
	else DEBUG("NO PARALLEL_REDUCE_JOIN ANNOTATIONS FOUND");*/

#ifdef NEED_ANNOT
	gen_final_annotation(M);
#else
	string hetero_function_annot_string;
	for (set<Function *>::iterator it = gen_function_set.begin(), ie = gen_function_set.end(); it != ie; it++) {
		Function *func = *it;
		string new_string(func->getName());
		hetero_function_annot_string.append(new_string + " ");
	}
	Constant *annoStr = ConstantDataArray::getString(M.getContext(), hetero_function_annot_string , true);
	new GlobalVariable(M, annoStr->getType(), false,
		GlobalValue::WeakODRLinkage, annoStr, "hetero.annot.string");
#endif


	//M.dump();
	return localChange;
}



