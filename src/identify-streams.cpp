// 15-745 F23 Project
// Group: Colin McDonald, Jennifer Brana
////////////////////////////////////////////////////////////////////////////////

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/IR/PassManager.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/InitializePasses.h"

#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/IVUsers.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Transforms/Utils/ScalarEvolutionExpander.h"

using namespace llvm;

namespace llvm {

    struct {
        Value* initialValue;
        Value* finalValue;
        Value* stepValue;
    } LoopStream;

    class IdentifyStreams : public LoopPass {
    public:
        static char ID;
        IdentifyStreams() : LoopPass(ID) {}

        virtual bool runOnLoop(Loop *L, LPPassManager& LPM) {
            errs() << "Loop: " << *L << "\n";

            IVUsers &IU = getAnalysis<IVUsersWrapperPass>().getIU();
            auto &SE = getAnalysis<ScalarEvolutionWrapperPass>().getSE();

            errs() << "IND VAR: " << *L->getInductionVariable(SE) << "\n";

            for (IVStrideUse &u : IU) {
                const SCEV* s = IU.getStride(u, L);
                if (const SCEVAddRecExpr* e = dyn_cast<SCEVAddRecExpr>(s)) {
                    errs() << "AddRec scev: " << *e << "\n";
                } else if (const SCEVAddExpr* e = dyn_cast<SCEVAddExpr>(s)) {
                    errs() << "Add scev: " << *e << "\n";
                } else {
                    errs() << "Not scev: " << *u << "\n";
                }
            }

            Loop::LoopBounds* bounds = L->getBounds(SE).getPointer();
            if (bounds) {
                Value& initialValue = bounds->getInitialIVValue();
                Value& finalValue = bounds->getFinalIVValue();
                const ICmpInst::Predicate& pred = bounds->getCanonicalPredicate();
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
            
            BasicBlock* condBB = *(L->block_begin());
            PHINode* indvar;
            PHINode* redvar;
            for (Instruction &i : *condBB) {
                if (PHINode* phi = dyn_cast<PHINode>(&i)) {
                    
                }
            }

            for (Loop::block_iterator bi = L->block_begin(), be = L->block_end(); bi != be; ++bi) {
                BasicBlock* b = *bi;
                for (Instruction &i : *b) {
                    
                }
                errs() << *b << "\n";
            }
            
            return true;
        }

        virtual void getAnalysisUsage(AnalysisUsage& AU) const {
            AU.addPreservedID(LoopSimplifyID);
            AU.addRequiredID(LoopSimplifyID);
            AU.addRequired<ScalarEvolutionWrapperPass>();
            AU.addPreserved<ScalarEvolutionWrapperPass>();
            AU.addRequiredID(LoopSimplifyID);
            AU.addRequired<IVUsersWrapperPass>();
            AU.addPreserved<IVUsersWrapperPass>();
        }

    private:
    };

    char IdentifyStreams::ID = 0;
    RegisterPass<IdentifyStreams> X("identify-streams", "Identify Streams");
}
