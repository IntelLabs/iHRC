//===-- HeteroDriverHeader.h - Driver code to integrate with opt  ---------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#ifndef LLVM_TRANSFORMS_CONCORD_H
#define LLVM_TRANSFORMS_CONCORD_H

#include "llvm/Pass.h"
using namespace llvm;
namespace llvm {
class ModulePass;
// Declare hetero transformation passes
// These are defined in respective passes
ModulePass *createHeteroVirtualPass();
ModulePass *createHeteroTransformPass();
ModulePass *createHeterotbbTransformPass();
ModulePass *createHeteroGPUTransformPass();
ModulePass *createHeteroCPUTransformPass();
ModulePass *createHeteroplusCleanPass();
ModulePass *createHeteroOMPTransformPass();
}
#endif
