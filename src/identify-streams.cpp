// 15-745 F23 Project
// Group: Colin McDonald, Jennifer Brana
////////////////////////////////////////////////////////////////////////////////

#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/IR/PassManager.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/InitializePasses.h"

#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/IVUsers.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/MemorySSAUpdater.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Analysis/ScalarEvolutionNormalization.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Utils/ScalarEvolutionExpander.h"

#define MYINITIALIZE_PASS_END(passName, arg, name, cfg, analysis)                \
  PassInfo *PI = new PassInfo(                                                 \
      name, arg, &passName::ID,                                                \
      PassInfo::NormalCtor_t(callDefaultCtor<passName>), cfg, analysis);       \
  Registry.registerPass(*PI, true);                                            \
  return PI;                                                                   \
  }                                                                            \
  static llvm::once_flag Initialize##passName##PassFlag;                       \
  void initialize##passName##Pass(PassRegistry &Registry) {                    \
    llvm::call_once(Initialize##passName##PassFlag,                            \
                    initialize##passName##PassOnce, std::ref(Registry));       \
  }

using namespace llvm;

namespace llvm {
    //void initializeIdentifyStreamsPass(PassRegistry& pr);

    class IdentifyStreams : public LoopPass {
    public:
        static char ID;
        //IdentifyStreams() : LoopPass(ID) { initializeIdentifyStreamsPass(*PassRegistry::getPassRegistry());};
        IdentifyStreams() : LoopPass(ID) {}

        virtual bool runOnLoop(Loop *L, LPPassManager& LPM) {
            errs() << "Loop: " << *L << "\n";

            IVUsers &IU = getAnalysis<IVUsersWrapperPass>().getIU();
            auto &SE = getAnalysis<ScalarEvolutionWrapperPass>().getSE();
            //auto &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
            //auto &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
            //const auto &TTI = getAnalysis<TargetTransformInfoWrapperPass>().getTTI(*L->getHeader()->getParent());
            //auto &AC = getAnalysis<AssumptionCacheTracker>().getAssumptionCache(*L->getHeader()->getParent());
            //auto &TLI = getAnalysis<TargetLibraryInfoWrapperPass>().getTLI(*L->getHeader()->getParent());
            //auto *MSSAAnalysis = getAnalysisIfAvailable<MemorySSAWrapperPass>();
            //MemorySSA *MSSA = nullptr;
            //if (MSSAAnalysis)
            //    MSSA = &MSSAAnalysis->getMSSA();


            //auto SAR = getAnalysis<LoopStandardAnalysisResults>();

            errs() << "IND VAR: " << *L->getInductionVariable(SE) << "\n";

            for (IVStrideUse &u : IU) {
                const SCEV* s = IU.getStride(u, L);
                if (const SCEVAddRecExpr* e = dyn_cast<SCEVAddRecExpr>(s)) {
                    errs() << "AddRec scev: " << *e << "\n";
                } else if (const SCEVAddExpr* e = dyn_cast<SCEVAddExpr>(s)) {
                    errs() << "Add scev: " << *e << "\n";
                } else if (const SCEVAddRecExpr* e = dyn_cast<SCEVAddRecExpr>(s)) {
                    errs() << "Not scev: " << *u << "\n";
                }
            }

            PHINode* phi = L->getCanonicalInductionVariable();
            if (phi) {
                errs() << "YES PHI!!" << *phi << "\n";
                errs() << "SCEV: " << *SE.getSCEV((Value*) phi) << "\n";
            } else {
                errs() << "NO PHI :(\n";
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
 
            //AU.addRequired<LoopInfoWrapperPass>();
            //AU.addPreserved<LoopInfoWrapperPass>();
            AU.addRequiredID(LoopSimplifyID);
            //AU.addRequired<DominatorTreeWrapperPass>();
            //AU.addPreserved<DominatorTreeWrapperPass>();
            AU.addRequired<ScalarEvolutionWrapperPass>();
            AU.addPreserved<ScalarEvolutionWrapperPass>();
            //AU.addRequired<AssumptionCacheTracker>();
            //AU.addRequired<TargetLibraryInfoWrapperPass>();
            AU.addRequiredID(LoopSimplifyID);
            AU.addRequired<IVUsersWrapperPass>();
            AU.addPreserved<IVUsersWrapperPass>();
            AU.addRequired<TargetTransformInfoWrapperPass>();
            //AU.addPreserved<MemorySSAWrapperPass>();
        }

    private:
    };

    char IdentifyStreams::ID = 0;

    //INITIALIZE_PASS_DEPENDENCY(LoopRotatePass);
    //INITIALIZE_PASS_DEPENDENCY(IndVarSimplifyPass);

    //
    /*INITIALIZE_PASS_BEGIN(IdentifyStreams, "identify-streams", "Identify Streams", false, false)
    //INITIALIZE_PASS_DEPENDENCY(TargetTransformInfoWrapperPass)
    //INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
    INITIALIZE_PASS_DEPENDENCY(ScalarEvolutionWrapperPass)
    INITIALIZE_PASS_DEPENDENCY(IVUsersWrapperPass)
    INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
    INITIALIZE_PASS_DEPENDENCY(LoopSimplify)

    MYINITIALIZE_PASS_END(IdentifyStreams, "identify-streams", "Identify Streams", false, false)*/
    RegisterPass<IdentifyStreams> X("identify-streams", "Identify Streams");
}
