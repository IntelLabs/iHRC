//===-- HeteroGPUTransform.h - API for GPU related code generation  ------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#ifndef HETERO_GPU_TRANSFORM_H
#define HETERO_GPU_TRANSFORM_H

//#include "llvm/Target/TargetData.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Transforms/iHRC/Hetero.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/iHRC/HeteroUtils.h"
#include "llvm/IR/IRBuilder.h"

using namespace llvm;

namespace{
	/// Hetero GPU Transformation Pass
	struct HeteroGPUTransform : public ModulePass, Hetero {
		static char ID; 
		/*TargetData*/DataLayout * TD;
		HeteroGPUTransform() : ModulePass(ID), TD(0) {}
		void getAnalysisUsage(AnalysisUsage &AU) const { }
		bool runOnModule(Module &M);
		/*TargetData*/ //DataLayout *getTargetData() const { return TD; }

	private:
		
		static bool OPTIMIZE_FLAG;

		/* Set of annotations to be generated */
#ifdef OLD_ANNOT
		vector<Constant *> Annotations;
		vector<Constant *> HeteroAnnotations;
#else
		typedef struct opecl_metadata_s {
			Function *function;
			Type *vec_type_hint;
			int work_group_size_hint[4];
			int reqd_work_group_size[4];
			vector<GlobalValue *>  locals;
			string  param_types;
		}opencl_metadata_t;
		vector<opencl_metadata_t> opencl_metadata;
#endif

		/* Set of base Pointers needing pointer adjustments */
		set<Value *> NeedCPURep;
		set<Value *> NeedGPURep;

		/* set of generated functions */
		set<Function *> gen_function_set;

		/* set of join functions */
		set<Function *> join_function_set;
		/* kernel function to cpu_join function map */
		map<Function *, int> kernel2sizeMap;
		void find_join_methods(Module &); 

		/* delete unnecessary code sequences */
		void delete_non_hetero_funcs(Module &);


		/* Code gen routines */
		bool Get_Hetero_Functions(Module &M,GlobalVariable *annot);
		void gen_new_function_per_f(Function *,int type);
		void gen_final_annotation(Module &);
		void gen_annotation_per_f(Function *);
		void gen_lazy_opt_code_per_f(Function *, Function *,int type);
		void gen_cpu_code_per_f(Function *, Function *);
		void gen_bdw_code_per_f(Function *, Function *);
		//void gen_high_opt_code_per_f(Function *, Function *);

		/* rewrite the instructions for new kernel */
		void rewrite_instruction(Instruction *, Instruction *, DenseMap<const Value *, Value *>&);

		/* convert ptrs from cpu to gpu and gpu to cpu */
		//Value* cpuPtrToGPUPtr(Value *, Function *, BasicBlock *, Value *, Value *, Value *);
		Value* cpuPtrToGPUPtr(Value *, Function *, IRBuilder<> &Builder, Value *, Value *);
		Value* gpuPtrToCPUPtr(Value *, Function *, IRBuilder<> &Builder, Value *, Value *);


		/* Optimization code generation */
		/*void findBasePtrs(Function *);
		bool** Make2DBoolArray(int, int);
		void Free2DBoolArray(bool **, int);
		void Print2DBoolArray(bool **, int , int);*/

		/* Set of final hetero functions after constraints are checked*/
		set<Function *> hetero_function_set;
	};
}

bool HeteroGPUTransform::OPTIMIZE_FLAG=false;
char HeteroGPUTransform::ID = 0;
ModulePass *llvm::createHeteroGPUTransformPass() { return new HeteroGPUTransform(); }
//static RegisterPass<HeteroGPUTransform> X("heterogpu", "Pointer transformation before OpenCL code generation", false, false);
#endif
