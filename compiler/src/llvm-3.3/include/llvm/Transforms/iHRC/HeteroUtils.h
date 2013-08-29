//===-- HeteroRuntime.h - API for cloning instructions and basic blocks---===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#ifndef LLVM_TRANSFORMS_IPO_HETEROUTILS_H
#define LLVM_TRANSFORMS_IPO_HETEROUTILS_H

#include "llvm/ADT/ValueMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/ValueHandle.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/CFG.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/ADT/UniqueVector.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <set>
#include <map>
#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"

namespace llvm {

class Module;
class Function;
class Instruction;
class Pass;
class LPPassManager;
class BasicBlock;
class Value;
class CallInst;
class InvokeInst;
class ReturnInst;
class CallSite;
class Trace;
class CallGraph;
class DataLayout;
class Loop;
class LoopInfo;
class AllocaInst;


void RewriteInstruction(Instruction *I, ValueToValueMapTy &VMap,
                            RemapFlags Flags = RF_None,
                        ValueMapTypeRemapper *TypeMapper = 0);
void CloneFunctionWithExistingBBInto(Function *NewFunc, BasicBlock *NewFuncBB, const Function *OldFunc, 
					ValueToValueMapTy &VMap, const Twine &NameSuffix = "");

} // End llvm namespace

#endif
