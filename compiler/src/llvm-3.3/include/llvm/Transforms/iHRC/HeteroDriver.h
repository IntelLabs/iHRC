//===-- HeteroDriver.h - Driver code to integrate with opt  --------------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#ifndef LLVM_TRANSFORMS_CONCORD_DRIVER_H
#define LLVM_TRANSFORMS_CONCORD_DRIVER_H
#include "llvm/PassManager.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Pass.h"

extern ModulePass *createHeteroVirtualPass();
extern ModulePass *createHeteroTransformPass();
extern ModulePass *createHeterotbbTransformPass();
extern ModulePass *createHeteroGPUTransformPass();
extern ModulePass *createHeteroCPUTransformPass();
extern ModulePass *createHeteroplusCleanPass();
extern ModulePass *createHeteroOMPTransformPass();

static cl::opt<bool>
HeterotbbTransform_f("heterotbb", cl::desc("C++/TBB frontend programs"));
static cl::opt<bool>
HeteroOMPTransform_f("heteroomp", cl::desc("C++ programs with OpenMP constructs"));
static cl::opt<bool>
HeteroTransform_f("hetero", cl::desc("determine hetero functions and add annotations"));
static cl::opt<bool>
HeteroGPUTransform_f("heterogpu", cl::desc("gpu transformation"));
static cl::opt<bool>
HeteroCPUTransform_f("heterocpu", cl::desc("cpu transformation"));
static cl::opt<bool>
HeteroplusClean_f("heteroplusclean", cl::desc("cleanup unnecessary functions and symbols"));
static cl::opt<bool>
HeteroVirtual_f("heterovirtual", cl::desc("Pseudo de-vitrualize virtual calls in C++"));

namespace llvm {

class Pass;

static inline void addMyPass(PassManagerBase &PM, Pass *P) {
  // Add the pass to the pass manager...
  PM.add(P);

  // If we are verifying all of the intermediate steps, add the verifier...
  /*if (VerifyEach)*/ PM.add(createVerifierPass());
}

static void hetero_driver(PassManager &Passes) {
	if (HeteroTransform_f) {
		addMyPass(Passes, createHeteroTransformPass());
	}
    else if (HeterotbbTransform_f) {
		addMyPass(Passes, createAlwaysInlinerPass());
		addMyPass(Passes, createHeterotbbTransformPass());
		addMyPass(Passes, createAlwaysInlinerPass());
		addMyPass(Passes, createHeteroVirtualPass());
		addMyPass(Passes, createAlwaysInlinerPass());
	}
	else if (HeteroVirtual_f) {
		addMyPass(Passes, createHeteroVirtualPass());
		addMyPass(Passes, createAlwaysInlinerPass());
	}
	else if (HeteroOMPTransform_f) {
		addMyPass(Passes, createHeteroOMPTransformPass());
	}
	else if (HeteroGPUTransform_f) {
		addMyPass(Passes, createHeteroGPUTransformPass());
		//addMyPass(Passes, createLoopUnrollPass());
		//addMyPass(Passes, createInstructionCombiningPass());
	}
    else if (HeteroCPUTransform_f) {
		addMyPass(Passes, createHeteroCPUTransformPass());
	}
	else if (HeteroplusClean_f) {
		addMyPass(Passes, createHeteroplusCleanPass());
	}
}
} // End llvm namespace

#endif
