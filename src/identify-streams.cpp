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

    class IdentifyStreams : public LoopPass {
    public:
        static char ID;
        IdentifyStreams() : LoopPass(ID) {}

        virtual bool runOnLoop(Loop *L, LPPassManager& LPM) {
            errs() << "--------------------------------------------------\n";
            errs() << "Loop: " << *L << "\n";

            auto &SE = getAnalysis<ScalarEvolutionWrapperPass>().getSE();

            PHINode* indvar = L->getInductionVariable(SE);

            /*for (User* u : indvar->users()) {
                errs() << "USER: " << *u << "\n";
            }*/

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
        }

    private:
    };

    char IdentifyStreams::ID = 0;
    RegisterPass<IdentifyStreams> X("identify-streams", "Identify Streams");
}
