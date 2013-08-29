//===-- HeteroGlobalsTransform.h - API for global variable code generation---===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#ifndef HETEROTBB_TRANSFROM_H
#define HETEROGLOBALS_TRANSFORM_H
#include "llvm/Target/TargetData.h"
#include "llvm/Transforms/iHRC/Hetero.h"

namespace {

	struct HeteroGlobalsTransform : public ModulePass, Hetero {

		static char ID; 
		TargetData *TD;
		HeteroGlobalsTransform() : ModulePass(ID), TD(0) {}
		void getAnalysisUsage(AnalysisUsage &AU) const { }
		bool runOnModule(Module &M);
		
	private:
		void find_and_transform_globals(Module &);
		void find_offload_and_rename(Module &);
		Function *create_new_offload_signature(Function *);
		Function *create_internal_offload_signature(Function *);
		const Type *find_globals_per_f(Function *, vector<Function *> &);
		Function *transform_globals_per_f(Function *, vector<Function *> &, const Type *);
		void clone_function (Function *, Function *);
		void rewrite_instruction(Instruction *, Instruction *, DenseMap<const Value *, Value *>& );
		
	};
}
char HeteroGlobalsTransform::ID = 0;
ModulePass *llvm::createHeteroGlobalsTransformPass() { return new HeteroGlobalsTransform(); }


#endif
