// 15-745 F23 Project
// Group: Colin McDonald, Jennifer Brana
////////////////////////////////////////////////////////////////////////////////

//#include <stdlib.h>
#include <cstdlib>
#include <stack>
//#include <vector.h>
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Pass.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/PassManager.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/InitializePasses.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/IVUsers.h"
#include "llvm/Analysis/IVDescriptors.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Transforms/Utils/ScalarEvolutionExpander.h"

#include "loop-exposed-vars.h"
#include "offload.h"

using namespace llvm;

namespace llvm {

    bool uses_instr_operands(Instruction* seek, Instruction* i);
    bool uses_instr(Instruction* seek, Instruction* i);

    void push_instr_operands(Instruction* i, std::stack<Instruction*>& todo) {
        if (i != NULL) {
            for (Value* v : i->operand_values()) {
                if (Instruction* i2 = dyn_cast<Instruction>(v)) {
                    todo.push(i2);
                }
            }
        }
    }

    bool uses_instr_safe(Instruction* seek, Instruction* i, bool onlyOperands) {
        std::set<Instruction*> visited;
        std::stack<Instruction*> todo;
        if (onlyOperands) {
            push_instr_operands(i, todo);
        } else {
            todo.push(i);
        }

        while (!todo.empty()) {
            Instruction* i = todo.top();
            todo.pop();
            if (visited.insert(i).second) {
                if (i == seek) {
                    return true;
                } else {
                    push_instr_operands(i, todo);
                }
            }
        }
        return false;
    }

    bool uses_instr_operands(Instruction* seek, Instruction* i) {
        return uses_instr_safe(seek, i, true);
    }

    bool uses_instr(Instruction* seek, Instruction* i) {
        return uses_instr_safe(seek, i, false);
    }

    void isRecDefinedWithLoadPushOperands(Instruction* i,
                                          bool iLoaded,
                                          std::set<std::pair<Instruction*, bool>> &visited,
                                          std::stack<std::pair<Instruction*, bool>> &todo) {
        if (i == NULL) { return; }
        for (Value* v : i->operand_values()) {
            if (Instruction* i2 = dyn_cast<Instruction>(v)) {
                bool loaded = iLoaded || isa<LoadInst>(i2);
                std::pair<Instruction*, bool> p = {i2, loaded};
                if (visited.insert(p).second) {
                    todo.push(p);
                }
            }
        }
    }

    // Determines if an instruction is recursively defined,
    // along some path with a load instr
    // (used to determine if a phi var is pointer-chasing
    bool isRecDefinedWithLoad(Instruction* seek) {
        std::stack<std::pair<Instruction*, bool>> todo;
        std::set<std::pair<Instruction*, bool>> visited;
        isRecDefinedWithLoadPushOperands(seek, false, visited, todo);

        while (!todo.empty()) {
            std::pair<Instruction*, bool> top = todo.top();
            todo.pop();
            Instruction* i = top.first;
            bool iLoaded = top.second;
            if (i == seek && iLoaded) {
                return true;
            } else {
                isRecDefinedWithLoadPushOperands(i, iLoaded, visited, todo);
            }
        }
        return false;
    }

    bool isRecDefined(Instruction* I) {
        return uses_instr_operands(I, I);
    }

    std::vector<LoadInst*> collectStreams(PHINode* indvar, Loop* L) {
        std::vector<LoadInst*> streams = std::vector<LoadInst*>();
        for (Loop::block_iterator bi = L->block_begin(), be = L->block_end(); bi != be; ++bi) {
            BasicBlock* b = *bi;
            for (Instruction &i : *b) {
                if (LoadInst* li = dyn_cast<LoadInst>(&i)) {
                    if (uses_instr(indvar, li)) {
                        streams.push_back(li);
                    }
                }
            }
        }
        return streams;
    }

    void getIndRedVars(Loop* L, std::vector<PHINode*>& inds, std::vector<PHINode*>& reds) {
        std::vector<PHINode*> recPHIs = std::vector<PHINode*>();
        BasicBlock* latch = L->getLoopLatch();
        BasicBlock* body = getLoopBody(L);
        if (!latch || !body) { return; }
        
        Instruction* latchLastInstr = NULL;
        for (Instruction& I : *latch) {
            latchLastInstr = &I;
        }

        for (Instruction& I : *body) {
            if (PHINode* phi = dyn_cast<PHINode>(&I)) {
                // phi->isUsedOutsideOfBlock(latch)
                // ^^^ not useful, because the phi is used by another
                // instruction used outside the block, but is not itself
                // used outside the block
                if (isRecDefined(phi)) {
                    recPHIs.push_back(phi);
                }
            }
        }

        for (PHINode* phi : recPHIs) {
            // If the loop conditional (last line of the latch) uses the phi,
            // it is an induction var. Otherwise, it is a reduction var
            if (uses_instr(phi, latchLastInstr)) {
                inds.push_back(phi);
            } else {
                reds.push_back(phi);
            }
        }
    }

    bool pointerChaseLoop(Loop* L, std::vector<PHINode*> inds, std::vector<PHINode*> reds) {
        bool pointerChased = false;
        for (PHINode* phi : inds) {
            pointerChased |= isRecDefinedWithLoad(phi);
        }
        return pointerChased;
    }

    bool affineLoop(Loop* L, PHINode* indvar, ScalarEvolution& SE, std::vector<PHINode*> inds, std::vector<PHINode*> reds) {
        if (indvar == nullptr) { return false; }
        Loop::LoopBounds* bounds = L->getBounds(SE).getPointer();
        std::vector<LoadInst*> streamVars = collectStreams(indvar, L);
        return bounds != nullptr && streamVars.size() > 0;
    }

    bool singleEntryExit(Loop* L) {
        // Can only offload if there is a single entry and exit
        std::vector<BasicBlock*> exits;
        loopExitBlocks(L, exits);
        return exits.size() == 1 && L->getLoopPredecessor();
    }

    bool shouldOffload(Loop* L, ScalarEvolution& SE) {
        // Loop must have single entry and exit
        if (!singleEntryExit(L)) {
            return false;
        }
        
        std::vector<PHINode*> inds;
        std::vector<PHINode*> reds;
        getIndRedVars(L, inds, reds);

        //for (PHINode* phi : inds) { errs() << "Found ind var: " << *phi << "\n"; }
        //for (PHINode* phi : reds) { errs() << "Found red var: " << *phi << "\n"; }
        // Loop must have both induction and reduction variables
        if (inds.size() == 0 || reds.size() == 0) { return false; }

        Function* fun = getLoopFunction(L);
        const StringRef fname = fun->getName();

        PHINode* indvar = L->getInductionVariable(SE);
        //InductionDescriptor inddesc;
        //L->getInductionDescriptor(SE, inddesc);
        if (affineLoop(L, indvar, SE, inds, reds)) {
            errs() << "Offload affine reduction loop in function " << fname << "\n";
            return true;
        }
        
        if (pointerChaseLoop(L, inds, reds)) {
            errs() << "Offload pointer-chasing reduction loop in function " << fname << "\n";
            return true;
        }
        return false;
    }

    class IdentifyStreams : public LoopPass {
    public:
        static char ID;
        IdentifyStreams() : LoopPass(ID) {}
        std::set<BasicBlock*> doneAlready;

        virtual bool runOnLoop(Loop *L, LPPassManager& LPM) {
            ScalarEvolution &SE = getAnalysis<ScalarEvolutionWrapperPass>().getSE();
            BasicBlock* body = getLoopBody(L);
            if (!doneAlready.count(body) && shouldOffload(L, SE)) {
                // If this is the first offload, insert pthread stuff into the main function
                if (doneAlready.size() == 0) { modifyMain(getLoopModule(L)); }
                doneAlready.insert(body);
                offloadToEngine(L);
                return true;
            }

            return false;
        }

        virtual void getAnalysisUsage(AnalysisUsage& AU) const {
            AU.addPreservedID(LoopSimplifyID);
            AU.addRequiredID(LoopSimplifyID);
            AU.addRequired<ScalarEvolutionWrapperPass>();
            AU.addPreserved<ScalarEvolutionWrapperPass>();
            AU.addRequiredID(LoopSimplifyID);
        }

    private:
    };

    char IdentifyStreams::ID = 0;
    RegisterPass<IdentifyStreams> X("identify-streams", "Identify Streams");
}
