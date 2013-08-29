//===-- HeterotbbTransform.h - API for TBB integration  -------------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===/
#ifndef HETEROTBB_TRANSFROM_H
#define HETEROTBB_TRANSFORM_H
//#include "llvm/Target/TargetData.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Transforms/iHRC/Heteroplus.h"
#include "llvm/Transforms/iHRC/HeteroUtils.h"
#include "llvm/Transforms/iHRC/HeteroDriverHeader.h"

namespace {

	struct HeterotbbTransform : public ModulePass, Hetero{

		static char ID; 
		/*TargetData*/ DataLayout *TD;
		HeterotbbTransform() : ModulePass(ID), TD(0) {}
		void getAnalysisUsage(AnalysisUsage &AU) const { }
		bool runOnModule(Module &M);
		int object_size_hetero;
		DenseMap<Value *,int> object_sizes;
		Function *templat; // template function for join
		Function *templat_join; // template function for join
		//Constant *hetero_f_const;
		
	private:

		/* is the call a hetero function call*/
		Function* get_hetero_func(Module &M,CallSite &CS);
		Function* get_join_func(Module &M,CallSite &CS);
		Function* write_new_hetero_kernel(Module &M, Function *f,int type);
		void rewrite_call_site(Module &M,  CallSite &CS,Function *NF,int type);
		void rewrite_invoke_site(Module &M,  CallSite &CS,Function *NF,int type);
		Function* create_new_join(Module &M,Function *join);
		void copy_function (Function* NF, Function* F);
		void edit_template_function (Module &M,Function* F,Function *reduce,GlobalVariable *old_gb,Value *gb);
		void rewrite_CPP(Module &M);
		void gen_opt_code_per_f(Function *, Function *);
		void rewrite_instruction(Instruction *, Instruction *, DenseMap<const Value *, Value *>&);
		void add_inline_attributes(Function *);

	};
}
char HeterotbbTransform::ID = 0;
ModulePass *llvm::createHeterotbbTransformPass() { return new HeterotbbTransform(); }
//static RegisterPass<HeterotbbTransform> X("heterotbb", "Lower the Intel Thread Building Block constructs", false, false);


#endif
