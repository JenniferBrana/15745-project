// 15-745 F23 Project
// Group: Colin McDonald, Jennifer Brana
////////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <stack>
#include <vector>
#include "llvm/ADT/BitVector.h"
//#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/IR/PassManager.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/InitializePasses.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/IVUsers.h"
#include "llvm/Analysis/IVDescriptors.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Transforms/Utils/ScalarEvolutionExpander.h"

#include "dataflow.h"

using namespace llvm;

namespace llvm {

    std::vector<BasicBlock*> getLoopBlocks(Loop* L) {
        std::vector<BasicBlock*> blocks;
        for (Loop::block_iterator bi = L->block_begin(), be = L->block_end(); bi != be; ++bi) {
            blocks.push_back(*bi);
        }
        return blocks;
    }

    BasicBlock* getLoopBody(Loop* L) {
        return *L->block_begin();
    }
    
    // Get the loop entry block
    BasicBlock* getLoopEntry(Loop* L) {
        BasicBlock* pred = L->getLoopPredecessor();
        if (!pred) { return nullptr; }
        BasicBlock* entry = nullptr;
        
        for (Loop::block_iterator bi = L->block_begin(), be = L->block_end(); bi != be; ++bi) {
            BasicBlock *B = *bi;
            for (BasicBlock *pred : predecessors(B)) {
                if (!L->contains(pred)) {
                    if (entry != nullptr && entry != B) {
                        return nullptr; // more than one entry!?
                    } else {
                        entry = B;
                    }
                }
            }
        }

        return entry;
    }

    Function* getLoopFunction(Loop* L) {
        return getLoopBody(L)->getParent();
    }

    Module* getLoopModule(Loop* L) {
        return getLoopFunction(L)->getParent();
    }

    DenseMap<Value*, unsigned int> domainMap;
    std::vector<Value*> domainVec;
    unsigned int domainSize;
    // Records info at each program point
    //DenseMap<Value*, BitVector> programPointInfo;

    // Initializes domainMap, domainVec, and domainSize for a given function
    void initializeDomainMapVec(Function* f) {
        domainSize = 0;
        domainMap = DenseMap<Value*, unsigned int>();
        domainVec = std::vector<Value*>();
        //programPointInfo = DenseMap<Value*, BitVector>();
        for (BasicBlock &b : *f) {
            for (Instruction &i : b) {
                // If a phi node, insert that into the domain
                // and then map all operands to it as well
                /*if (PHINode* phi = dyn_cast<PHINode>(&i)) {
                    // Insert phi into map, append to vec
                    domainMap.insert({phi, domainSize});
                    domainVec.push_back(phi);
                    // Now map all operands to this same value:
                    for (User::op_iterator OI = i.op_begin(); OI != i.op_end(); ++OI) {
                        Value* val = *OI;
                        if (isa<Instruction>(val) || isa<Argument>(val)) {
                            domainMap.insert({val, domainSize});
                        }
                    }
                    ++domainSize;
                } else {*/
                    domainMap.insert({&i, domainSize});
                    domainVec.push_back(&i);
                    ++domainSize;
                    // Iterate through all operands, adding them to the domain
                    for (User::op_iterator OI = i.op_begin(); OI != i.op_end(); ++OI) {
                        Value *val = *OI;
                        if (isa<Instruction>(val) || isa<Argument>(val)) {
                            if (!domainMap.count(val)) { // contains
                                domainMap.insert({val, domainSize});
                                domainVec.push_back(val);
                                ++domainSize;
                            }
                        }
                    }
                //}
            }
        }
    }

    void transferUpward(BitVector &in, BasicBlock* block) {
        for (BasicBlock::reverse_iterator rit = block->rbegin(), rend = block->rend(); rit != rend; ++rit) {
            // e.g. x = y + z;
            Instruction* instr = &*rit;

            // Kill x
            if (domainMap.count(instr)) { // contains
                in.reset(domainMap[instr]);
            }

            // Add y and z to gen
            for (User::op_iterator OI = instr->op_begin(); OI != instr->op_end(); ++OI) {
                Value *val = *OI;
                if (isa<Instruction>(val) || isa<Argument>(val)) {
                    in.set(domainMap[val]);
                }
            }

            //programPointInfo[instr] = in;
        }
    }

    void compactDomainValues(BitVector b, std::vector<Value*>& compacted) {
        for (unsigned int i = 0; i < domainSize; ++i) {
            if (b[i]) compacted.push_back(domainVec[i]);
        }
    }

    void loopExitBlocks(Loop* L, std::vector<BasicBlock*>& blocks) {
        for (Loop::block_iterator bi = L->block_begin(), be = L->block_end(); bi != be; ++bi) {
            BasicBlock *b = *bi;
            for (BasicBlock* s : successors(b)) {
                if (!L->contains(s)) { blocks.push_back(s); }
            }
        }
    }
    
    void loopExitingBlocks(Loop* L, std::vector<BasicBlock*>& blocks) {
        for (Loop::block_iterator bi = L->block_begin(), be = L->block_end(); bi != be; ++bi) {
            bool exiting = false;
            BasicBlock *b = *bi;
            for (BasicBlock* s : successors(b)) {
                exiting = exiting || !L->contains(s);
            }
            if (exiting) {
                blocks.push_back(b);
            }
        }
    }

    BitVector occursInLoop(Loop* L) {
        BitVector a = BitVector(domainSize, false);
        for (Loop::block_iterator bi = L->block_begin(), be = L->block_end(); bi != be; ++bi) {
            BasicBlock *b = *bi;
            for (BasicBlock* s : successors(b)) {
                for (Instruction &i : *s) {
                    a.set(domainMap[&i]);
                }
            }
        }
        return a;
    }

    // Returns tuple (downwardsExposed, upwardsExposed)
    // Each value being either an Instruction or an Argument
    void loopExposedVars(Loop* L,
                         std::vector<Value*>& downwardsExposed,
                         std::vector<Value*>& upwardsExposed) {
        // Loop body block
        BasicBlock* body = getLoopBody(L);

        // Get the function in which L occurs
        Function* F = body->getParent();
        
        // Blocks that exit the loop
        //std::vector<BasicBlock*> exitingBlocks;
        //loopExitingBlocks(L, exitingBlocks);

        // Blocks that loop exits go to
        std::vector<BasicBlock*> exitBlocks;
        loopExitBlocks(L, exitBlocks);

        initializeDomainMapVec(F);
        
        Framework fw = Framework(Direction::Backward,
                                 domainSize,
                                 false,
                                 false,
                                 &transferUpward,
                                 &dataflow_meet_union);
        DenseMap<BasicBlock*, BlockState*> result = llvm::analyze(fw, F);

        BlockState bodyState = *result[body];
        
        // Meet all exiting blocks
        BitVector downwardsExposedBV = BitVector(domainSize);
        for (BasicBlock* B : exitBlocks) {
            //errs() << "Exit block: " << *B << "\n";
            downwardsExposedBV |= *result[B]->in;
        }

        BitVector inLoop = occursInLoop(L);
        BitVector outLoop = BitVector(inLoop);
        outLoop = outLoop.flip();
        
        downwardsExposedBV &= inLoop;

        BitVector upwardsExposedBV = *bodyState.out;
        upwardsExposedBV &= outLoop;

        compactDomainValues(downwardsExposedBV, downwardsExposed);
        compactDomainValues(upwardsExposedBV, upwardsExposed);
    }

    

}
