////===-- OpenCLTargetMachine.h - target Machine for OpenCl backend-----------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
//
// This file declares the TargetMachine that is used by the openCL backend.
//
//===----------------------------------------------------------------------===//

#ifndef OPENCLTARGETMACHINE_H
#define OPENCLTARGETMACHINE_H

#include "llvm/Target/TargetMachine.h"
#include "llvm/IR/DataLayout.h"
namespace llvm {

	struct OpenCLTargetMachine : public TargetMachine {
		
		/* constructor */
		OpenCLTargetMachine(const Target &Trgt, StringRef SR, StringRef CPU_S, StringRef FSS, const TargetOptions &Opts,
					Reloc::Model RM, CodeModel::Model CM,
					CodeGenOpt::Level OL)
				: TargetMachine(Trgt, SR, CPU_S, FSS, Opts) {}

		/* Get data layout */
		virtual const DataLayout *getDataLayout() const { return 0; }

		/* Add passes to emit code */
		virtual bool addPassesToEmitFile(PassManagerBase &PassMG,
									formatted_raw_ostream &OutStrm,
									CodeGenFileType FileType,
									bool DisableVerifyFlag,
									AnalysisID Start,
									AnalysisID Stop);

		
	};

		extern Target TheOpenCLBackendTarget;

} // End llvm namespace


#endif
