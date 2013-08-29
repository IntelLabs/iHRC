//===-- HeteroRuntime.h - API for GPU code generation check -------------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#ifndef HETERO_TRANSFROM_H
#define HETERO_TRANSFORM_H

#include "llvm/Transforms/iHRC/Hetero.h"
#include <llvm/Bitcode/ReaderWriter.h>
//#include <llvm/support/raw_os_ostream.h>
#include "llvm/Support/ToolOutputFile.h"

namespace {

	struct HeteroTransform : public ModulePass, Hetero {

		static char ID; 
		HeteroTransform() : ModulePass(ID) {}
		void getAnalysisUsage(AnalysisUsage &AU) const { }
		bool runOnModule(Module &M);

		typedef struct tuple_s {
			Value *Op1;
			Value *Op2;
		}tuple_t;

		/* Set of initial entry hetero functions */
		set<Function *> entry_hetero_function_set;
		/* Set of final hetero functions after compiler restrictions are checked*/
		set<Function *> hetero_function_set;
		set<Function *> entry_join_function_set;
		set<Function *> join_function_set;
		std::string ErrorInfo;
		OwningPtr<tool_output_file> Out;

#ifdef HETERO_GCD_ALLOC_TO_MALLOC
		multimap<Function *, Instruction *> block2insnMap;
#endif

		//Constant *expf_const; // holds the declaration of expf

	private:

		/* ensure compiler restrictions are met */
		void check_compiler_restrictions(int i);

		bool check_compiler_restrictions_perf(Function *hetero_func);

		/* cleanup exception blocks, rewrite sqrtf to native_sqrt, cosf to native_cos, acosf to native_acos*/
		Instruction *rewriteCallSite(Instruction *OldCall, Function *NF);
		Function *createNewFunctionDeclaration(Function *F, string name);
		void cleanup_exception_builtin(Function *hetero_func);

		/* is the call a hetero function call*/
		void get_hetero_functions(Module &M,int type);

		void simplifyCFG(Function *hetero_func);
		void deleteSet(vector<Instruction *> &toDelete);

#ifdef HETERO_GCD_ALLOC_TO_MALLOC
		/* rewrite calls from alloca to malloc for blocks and blck variables */
		void rewrite_alloca(CallSite &);
#endif

		/*  Add annotations for functions that need transformation */
		void gen_annotations(Module &M,Module *M_string,set<Function *> hetero_function_set,string);

	};
}
char HeteroTransform::ID = 0;
ModulePass *llvm::createHeteroTransformPass() { return new HeteroTransform(); }
//static RegisterPass<HeteroTransform> X("hetero", "Hetero Pass to find kernels suitable code OpenCL code generation", false, false);


#endif
