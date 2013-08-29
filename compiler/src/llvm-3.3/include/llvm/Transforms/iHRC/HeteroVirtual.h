//===-- HeteroRuntime.h - API for virtual function integration  -------------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#ifndef HETERO_VIRTUAL_H
#define HETERO_VIRTUAL_H
#include "llvm/Transforms/iHRC/Hetero.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/iHRC/HeteroUtils.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include "llvm/ADT/SetVector.h"
//#include "llvm/Target/TargetData.h"
#include "llvm/IR/DataLayout.h"

namespace llvm{

	struct HeteroVirtual : public ModulePass, Hetero{

		static char ID; 
		DataLayout *TD;
		HeteroVirtual() : ModulePass(ID), TD(0) {}
		void getAnalysisUsage(AnalysisUsage &AU) const { }
		bool runOnModule(Module &);

		multimap<Value *,Value*> hierarchy;
		multimap<Value *,Value*> inverse_hierarchy;
		DenseMap<Value *,Value*> rti_to_vtable;	
		

	private:
		
		void modify_basic_block(Module &M,BasicBlock *BBI,CallInst *call, SetVector<Function *>& );
		void get_class_hierarchy(Module &M);
		void get_possible_targets(Module &,const Type *, int,CallInst *, Value *, SetVector<Function *>& ) ;
#if 0
		void find_and_transform_globals(Module &);
#endif
		void find_offload_and_rename(Module &);
		Function *create_new_offload_signature(Function *);
		Function *create_internal_offload_signature(Function *);
		void find_globals_per_f(Function *, vector<Function *> &, vector</*const*/ Type *> &);
		Function *transform_globals_per_f(Function *, vector<Function *> &, /*const*/ Type *);
		void clone_function (Function *, Function *);
		void rewrite_instruction(Instruction *, Instruction *, DenseMap<const Value *, Value *>& );
		bool translate_virtual_calls_to_switch(Module &);
		void create_map_from_type_to_vtable(Module &M);
		void allocate_vtables_in_shared_region(Module &);
		void add_inline_attributes(Function *);

	};
}
char HeteroVirtual::ID = 0;
ModulePass *llvm::createHeteroVirtualPass() { return new HeteroVirtual(); }
//static RegisterPass<HeteroVirtual> X("heterovirtual", "Hello World Pass", false, false);


#endif
