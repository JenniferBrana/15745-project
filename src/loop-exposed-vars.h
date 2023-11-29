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
    // Returns tuple (downwardsExposed, upwardsExposed)
    // Each value being either an Instruction or an Argument
    void loopExposedVars(Loop* L,
                         std::vector<Value*>& downwardsExposed,
                         std::vector<Value*>& upwardsExposed);
}

#endif
