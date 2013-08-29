//===-- HeteroOMPTransform.h - API for OMP related code generation  ------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#ifndef HETERO_OMP_TRANSFROM_H
#define HETERO_OMP_TRANSFORM_H

#include "llvm/Transforms/iHRC/Hetero.h"

namespace {

	struct HeteroOMPTransform : public ModulePass, Hetero{
		static char ID; 

		static string OMP_PARALLEL_START_NAME;
		static string OMP_PARALLEL_END_NAME;
		static string OMP_GET_NUM_THREADS_NAME;
		static string OMP_GET_THREAD_NUM_NAME;

		HeteroOMPTransform() : ModulePass(ID) {}
		void getAnalysisUsage(AnalysisUsage &AU) const { }
		bool runOnModule(Module &M);


	private:
		
		/* Function declaration for offload */
		Constant *offloadFnTy;
		Constant *mallocFnTy;

		DenseMap<Function *, Function *> oldF2newF;

		void init_malloc_shared_type(Module &);
		void init_offload_type(Module &, Function *);
		void rewrite_omp_call_sites(Module &);
		Value *find_loop_upper_bound(Value *);
		Function *convert_to_kernel_function(Module &, Function *);
		void gen_code_per_f (Function* , Function*, Instruction *);
		void rewrite_instruction(Instruction *, Instruction *, DenseMap<const Value *, Value *>& );


	};
}
string HeteroOMPTransform::OMP_PARALLEL_START_NAME = "GOMP_parallel_start";
string HeteroOMPTransform::OMP_PARALLEL_END_NAME = "GOMP_parallel_end";
string HeteroOMPTransform::OMP_GET_NUM_THREADS_NAME = "omp_get_num_threads";
string HeteroOMPTransform::OMP_GET_THREAD_NUM_NAME = "omp_get_thread_num";
char HeteroOMPTransform::ID = 0;
ModulePass *llvm::createHeteroOMPTransformPass() { return new HeteroOMPTransform(); }
//static RegisterPass<HeteroOMPTransform> X("heteroomp", "Lower OpenMP constructs to iHRC", false, false);


#endif
