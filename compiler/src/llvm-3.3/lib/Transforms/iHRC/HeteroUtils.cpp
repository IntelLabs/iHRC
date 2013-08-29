//===-- HeteroUtils.cpp - Function for cloning methods and instructions ---===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#include "llvm/Transforms/iHRC/HeteroUtils.h"
using namespace llvm;


/// RemapInstruction - Convert the instruction operands from referencing the
/// current values into those specified by ValueMap.
///
void llvm::RewriteInstruction(Instruction *INSN, ValueToValueMapTy &ValueMap,
                            RemapFlags Flags, ValueMapTypeRemapper *TypeMap){
  // Remap operands.
  for (User::op_iterator oper = INSN->op_begin(), E = INSN->op_end(); oper != E; ++oper) {
    Value *Val = MapValue(*oper, ValueMap, Flags, TypeMap);
    // If we aren't ignoring missing entries, assert that something happened.
    if (Val != 0)
      *oper = Val;
  }

  // Remap phi nodes' incoming blocks.
  if (PHINode *Phi = dyn_cast<PHINode>(INSN)) {
    for (unsigned i = 0, e = Phi->getNumIncomingValues(); i != e; ++i) {
      Value *Val = MapValue(Phi->getIncomingBlock(i), ValueMap, Flags);
      // If we aren't ignoring missing entries, assert that something happened.
      if (Val != 0)
        Phi->setIncomingBlock(i, cast<BasicBlock>(Val));
    }
  }

  // Remap attached metadata.
  SmallVector<std::pair<unsigned, MDNode *>, 4> MetaDatas;
  INSN->getAllMetadata(MetaDatas);
  for (SmallVectorImpl<std::pair<unsigned, MDNode *> >::iterator
       MI = MetaDatas.begin(), ME = MetaDatas.end(); MI != ME; ++MI) {
    MDNode *OldMeta = MI->second;
    MDNode *NewMeta = MapValue(OldMeta, ValueMap, Flags, TypeMap);
    if (NewMeta != OldMeta)
      INSN->setMetadata(MI->first, NewMeta);
  }
}

// Clone OldFunction into NewFunction, transforming the old arguments into references to
// ValueMap values.
//
void llvm::CloneFunctionWithExistingBBInto(Function *NewFunction, BasicBlock *NewFunctionBB, const Function *OldFunction,
                             ValueToValueMapTy &ValueMap, const Twine &NameSuffix) {

  // if there is no existing basicblock then call clone function and  be done!
  if (NewFunctionBB == NULL) {
	  SmallVector<ReturnInst*, 8> Returns;  // Ignore returns cloned.
	  CloneFunctionInto(NewFunction, OldFunction, ValueMap, false, Returns, "");
	  return;
  }

  // Copy the first basic block of OldFunction into NewFunctionBB
  Function::const_iterator BI = OldFunction->begin();
  const BasicBlock &FB = *BI;

  // Add basic block mapping.
  ValueMap[&FB] = NewFunctionBB;

  for (BasicBlock::const_iterator II = FB.begin(), IE = FB.end(); II != IE; ++II) {
		Instruction *NFInst = II->clone(/*F->getContext()*/);
		//	DEBUG(dbgs()<<*II<<"\n");
		if (II->hasName()) NFInst->setName(II->getName());
		//const Instruction *FInst = &(*II);
		//rewrite_instruction((Instruction *)FInst, NFInst, ValueMap);
		NewFunctionBB->getInstList().push_back(NFInst);
		ValueMap[II] = NFInst;
  }


  // Advance the iterator for basic block
  BI++;
  for (Function::const_iterator BE = OldFunction->end(); BI != BE; ++BI) {

    const BasicBlock &BBlock = *BI;

    // Create a new basic block and copy instructions into it!
    BasicBlock *ClonedBB = CloneBasicBlock(&BBlock, ValueMap, NameSuffix, NewFunction);

    // Add basic block mapping.
    ValueMap[&BBlock] = ClonedBB;

    if (BBlock.hasAddressTaken()) {
      Constant *OldBBAddress = BlockAddress::get(const_cast<Function*>(OldFunction),
                                              const_cast<BasicBlock*>(&BBlock));
      ValueMap[OldBBAddress] = BlockAddress::get(NewFunction, ClonedBB);                                         
    }

    // Note return instructions for the caller.
	//if (ReturnInst *RI = dyn_cast<ReturnInst>(CBB->getTerminator()))
    //  Returns.push_back(RI);
  }

  // Loop over all of the instructions in the function, fixing up operand
  // references as we go.  This uses ValueMap to do all the hard work.
  for (Function::iterator BBlock = cast<BasicBlock>(ValueMap[OldFunction->begin()]),
         BE = NewFunction->end(); BBlock != BE; ++BBlock)
    // Loop over all instructions, fixing each one as we find it...
    for (BasicBlock::iterator II = BBlock->begin(); II != BBlock->end(); ++II)
      RewriteInstruction(II, ValueMap);
}
