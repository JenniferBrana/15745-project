// 15-745 F23 Project
// Group: Colin McDonald, Jennifer Brana
////////////////////////////////////////////////////////////////////////////////

#ifndef LOOP_EXPOSED_VARS_H
#define LOOP_EXPOSED_VARS_H

#include <stdlib.h>
#include <vector>
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Analysis/LoopInfo.h"

using namespace llvm;

namespace llvm {

    std::vector<BasicBlock*> getLoopBlocks(Loop* L);
    BasicBlock* getLoopBody(Loop* L);
    BasicBlock* getLoopEntry(Loop* L);
    Function*   getLoopFunction(Loop* L);
    Module*     getLoopModule(Loop* L);
    void loopExitBlocks(Loop* L, std::vector<BasicBlock*>& blocks);
    void loopExitingBlocks(Loop* L, std::vector<BasicBlock*>& blocks);

    // Returns tuple (downwardsExposed, upwardsExposed)
    // Each value being either an Instruction or an Argument
    void loopExposedVars(Loop* L,
                         std::vector<Value*>& downwardsExposed,
                         std::vector<Value*>& upwardsExposed);
}

#endif
