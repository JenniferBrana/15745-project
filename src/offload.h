#ifndef OFFLOAD_H
#define OFFLOAD_H

#include "llvm/Analysis/LoopInfo.h"

namespace llvm{
    void offloadToEngine(Loop* L);
    void modifyMain(Module* mod);
}

#endif
