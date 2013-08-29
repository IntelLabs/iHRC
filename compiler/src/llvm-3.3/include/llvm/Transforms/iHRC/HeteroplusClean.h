//===-- HeteroplusClean.h - API for code cleanup after iHRC  -------------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#ifndef HETEROPLUS_CLEAN_H
#define HETEROPLUS_CLEAN_H

#include "llvm/Transforms/iHRC/Hetero.h"

namespace {

	struct HeteroplusClean : public ModulePass, Hetero{

		static char ID; 
		HeteroplusClean() : ModulePass(ID) {}
		void getAnalysisUsage(AnalysisUsage &AU) const { }
		bool runOnModule(Module &M);

	private:
		/* Set of final hetero functions after constraints are checked*/
		set<Function *> hetero_function_set;
		std::vector<Function *> trans_hetero_function_set; 
		void delete_globals(Module &);
		void correct_store_alignment(Module &);
		void delete_non_hetero_funcs(Module &);
		void delete_func_aliases(Module &);
	};
}
char HeteroplusClean::ID = 0;
ModulePass *llvm::createHeteroplusCleanPass() { return new HeteroplusClean(); }
//static RegisterPass<HeteroplusClean> X("heteroclean", "Cleanup pass before OpenCL code generation", false, false);
#endif
