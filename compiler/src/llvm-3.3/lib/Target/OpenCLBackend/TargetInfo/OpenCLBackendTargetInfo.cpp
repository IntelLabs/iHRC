//===-- OpenCLBackendTargetInfo.cpp - OpenCl backend integration  -------------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===

#include "OpenCLTargetMachine.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/TargetRegistry.h"
using namespace llvm;

Target llvm::TheOpenCLBackendTarget;

static unsigned OpenCLBackend_TripleMatchQuality(const std::string &TT) {
  // This class always works, but shouldn't be the default in most cases.
  return 1;
}

extern "C" void LLVMInitializeOpenCLBackendTargetInfo() { 
  TargetRegistry::RegisterTarget(TheOpenCLBackendTarget, "opencl", "OpenCL backend",&OpenCLBackend_TripleMatchQuality );
}

extern "C" void LLVMInitializeOpenCLBackendTargetMC() {}
