// 15-745 F23 Project
// Group: Colin McDonald, Jennifer Brana
////////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <stack>
//#include <vector.h>
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

    bool isRecDefined(Instruction* I) {
        return uses_instr_operands(I, I);
    }

    std::vector<LoadInst*> collectStreams(PHINode* indvar, Loop* L) {
        std::vector<LoadInst*> streams = std::vector<LoadInst*>();
        for (Loop::block_iterator bi = L->block_begin(), be = L->block_end(); bi != be; ++bi) {
            BasicBlock* b = *bi;
            for (Instruction &i : *b) {
                if (LoadInst* li = dyn_cast<LoadInst>(&i)) {
                    errs() << "Load " << *li << "\n";
                    if (uses_instr(indvar, li)) {
                        errs() << "Uses indvar!\n";
                        streams.push_back(li);
                    }
                }
            }
        }
        return streams;
    }

    bool findReductionVar(BasicBlock* B, LoadInst* load) {
        // TODO: make sure when seeking that each instr is still within B
        for (Instruction& I : *B) {
            bool a = uses_instr_operands(&I, &I);
            bool b = uses_instr(load, &I);
            errs() << I << ": uses itself = " << a << "\n";
            errs() << I << ": uses indvar = " << b << "\n";
            if (a && b) {
                errs() << "This is a reduction! " << I << "\n";
                return true;
            }
        }
        return false;
    }

    BasicBlock* getBody(Loop* L) {
        for (Loop::block_iterator bi = L->block_begin(), be = L->block_end(); bi != be; ++bi) {
            BasicBlock* B = *bi;
            return B;
        }
        return NULL;
    }

    void getIndRedVars(Loop* L, std::vector<PHINode*>& inds, std::vector<PHINode*>& reds) {
        std::vector<PHINode*> recPHIs = std::vector<PHINode*>();
        BasicBlock* body = getBody(L);
        Instruction* bodyLastInstr = NULL;
        for (Instruction& I : *body) {
            bodyLastInstr = &I;
            if (PHINode* phi = dyn_cast<PHINode>(&I)) {
                if (isRecDefined(phi)) { // phi->isUsedOutsideOfBlock(body) && 
                    recPHIs.push_back(phi);
                    errs() << "For PHI " << *phi << ":\n";
                    for (Value* v : phi->operand_values()) {
                        if (Instruction* i = dyn_cast<Instruction>(v)) {
                            if (i->isUsedOutsideOfBlock(body)) {
                                errs() << "Used for " << *i << "\n";
                            } else {
                                errs() << "Not used outside of block: " << *i << "\n";
                            }
                        }
                    }
                }
            }
        }

        for (PHINode* phi : recPHIs) {
            // If the loop conditional (last line of the body) uses phi,
            // it is an induction var. Otherwise, reduction var
            if (uses_instr(phi, bodyLastInstr)) {
                inds.push_back(phi);
            } else {
                reds.push_back(phi);
            }
        }
    }

    bool pointerChaseLoop(Loop* L) {
        errs() << "NO IND VAR\n";
        std::vector<PHINode*> inds = std::vector<PHINode*>();
        std::vector<PHINode*> reds = std::vector<PHINode*>();
        getIndRedVars(L, inds, reds);
        for (PHINode* phi : inds) {
            errs() << "Found ind var: " << *phi << "\n";
        }
        for (PHINode* phi : reds) {
            errs() << "Found red var: " << *phi << "\n";
        }
        return false;
    }

    bool affineLoop(Loop* L, PHINode* indvar, ScalarEvolution& SE) {
        errs() << "IND VAR: " << *indvar << "\n";

        Loop::LoopBounds* bounds = L->getBounds(SE).getPointer();
        if (bounds) {
            Value& initialValue = bounds->getInitialIVValue();
            Value& finalValue = bounds->getFinalIVValue();
            const ICmpInst::Predicate& pred = bounds->getCanonicalPredicate(); // Enum
            Instruction& stepInst = bounds->getStepInst();
            Value* stepValue = bounds->getStepValue();
            Loop::LoopBounds::Direction dir = bounds->getDirection();
                
            errs() << "Initial: " << initialValue << "\n";
            errs() << "Final: " << finalValue << "\n";
            errs() << "Pred: " << pred << "\n";
            errs() << "Step inst: " << stepInst << "\n";
            if (stepValue) errs() << "Step value: " << *stepValue << "\n";
            if (dir == Loop::LoopBounds::Direction::Increasing) {
                errs() << "Direction: increasing\n";
            } else if (dir == Loop::LoopBounds::Direction::Decreasing) {
                errs() << "Direction: decreasing\n";
            } else {
                errs() << "Direction: unknown\n";
            }
        } else {
            errs() << "NO BOUNDS\n";
        }
            
        std::vector<LoadInst*> streamVars = collectStreams(indvar, L);

        for (LoadInst* load : streamVars) {
            BasicBlock* body = getBody(L);
            errs() << *body << "\n";
            bool is_reduction = body && findReductionVar(getBody(L), load);
        }

        return true;
    }

    class IdentifyStreams : public LoopPass {
    public:
        static char ID;
        IdentifyStreams() : LoopPass(ID) {}

        virtual bool runOnLoop(Loop *L, LPPassManager& LPM) {
            errs() << "--------------------------------------------------\n";
            errs() << "Loop: " << *L << "\n";
            errs() << "Body: " << *getBody(L) << "\n";

            auto &SE = getAnalysis<ScalarEvolutionWrapperPass>().getSE();

            PHINode* indvar = L->getInductionVariable(SE);

            InductionDescriptor inddesc;
            L->getInductionDescriptor(SE, inddesc);
            errs() << "Induction descriptor kind = " << inddesc.getKind() << "\n";

            /*for (User* u : indvar->users()) {
                errs() << "USER: " << *u << "\n";
            }*/

            if (!indvar) {
                return pointerChaseLoop(L);
            } else {
                return affineLoop(L, indvar, SE);
            }
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
