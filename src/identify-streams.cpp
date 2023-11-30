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
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
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

#include "loop-exposed-vars.h"

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

    /*BasicBlock* getBody(Loop* L) {
        for (Loop::block_iterator bi = L->block_begin(), be = L->block_end(); bi != be; ++bi) {
            BasicBlock* B = *bi;
            return B;
        }
        return NULL;
    }*/

    void getIndRedVars(Loop* L, std::vector<PHINode*>& inds, std::vector<PHINode*>& reds) {
        std::vector<PHINode*> recPHIs = std::vector<PHINode*>();
        BasicBlock* body = getLoopBody(L);
        Instruction* bodyLastInstr = NULL;
        for (Instruction& I : *body) {
            bodyLastInstr = &I;
            if (PHINode* phi = dyn_cast<PHINode>(&I)) {
                // phi->isUsedOutsideOfBlock(body)
                // ^^^ not useful, because the phi is used by another
                // instruction used outside the block, but is not itself
                // used outside the block
                if (isRecDefined(phi)) {
                    recPHIs.push_back(phi);
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

    void valueTypes(std::vector<Value*> values, std::vector<Type*> &types) {
        for (size_t i = 0; i < values.size(); ++i) {
            types[i] = values[i]->getType();
        }
    }

    void offloadToEngine(Loop* L) {
        Module* loopMod = getLoopModule(L);
        Function* loopFun = getLoopFunction(L);
        BasicBlock* loopBody = getLoopBody(L);
        BasicBlock* loopEntry = getLoopEntry(L);
        LLVMContext& ctxt = loopFun->getContext();

        // loop must have only 1 entry and exit, to have gotten here
        std::vector<BasicBlock*> exits;
        loopExitBlocks(L, exits);
        BasicBlock* exitBlock = exits[0];

        // Value* is either Instruction* or Argument*
        std::vector<Value*> downwardsExposed;
        std::vector<Value*> upwardsExposed;
        llvm::loopExposedVars(L, downwardsExposed, upwardsExposed);
        Type* inDataTp = NULL;
        std::vector<Type*> downwardsExposedTp = std::vector<Type*>(downwardsExposed.size());
        std::vector<Type*> upwardsExposedTp = std::vector<Type*>(upwardsExposed.size());
        valueTypes(downwardsExposed, downwardsExposedTp);
        valueTypes(upwardsExposed, upwardsExposedTp);
        std::vector<Type*> exposedTp;
        exposedTp.insert(exposedTp.end(), upwardsExposedTp.begin(), upwardsExposedTp.end());
        exposedTp.insert(exposedTp.end(), downwardsExposedTp.begin(), downwardsExposedTp.end());

        //ArrayRef<Type*> inDataTpArray = ArrayRef<Type*>(upwardsExposedTp);
        //ArrayRef<Type*> outDataTpArray = ArrayRef<Type*>(downwardsExposedTp);
        ArrayRef<Type*> dataTpArray = ArrayRef<Type*>(exposedTp);
        //StructType* inStruct = StructType::create(ctxt, inDataTpArray);//, "uli_in_data_struct");
        //StructType* outStruct = StructType::create(ctxt, outDataTpArray);//, "uli_out_data_struct");
        StructType* dataStruct = StructType::create(ctxt, dataTpArray);
            
        //errs() << "instruct = " << *inStruct << "\n";
        //errs() << "outstruct = " << *outStruct << "\n";

        for (Value* v : downwardsExposed) { errs() << "down-exposed: " << *v << "\n"; }
        for (Value* v : upwardsExposed) { errs() << "  up-exposed: " << *v << "\n"; }
            
        Type* voidTy = Type::getVoidTy(ctxt);
        Type* intTy = Type::getInt32Ty(ctxt);
        Type *voidPtrTy = llvm::PointerType::getUnqual(voidTy);
        std::vector<Type*> paramsVoidPtr = {voidPtrTy};
        FunctionType* funTy = FunctionType::get(voidTy, ArrayRef<Type*>(paramsVoidPtr), false);
        Function* engFunc = Function::Create(funTy, GlobalValue::LinkageTypes::PrivateLinkage, "identify_streams_eng_func", loopMod);
        //engFunc->
            
        // Create block that sets up the input data struct
        BasicBlock *offloadInitBlock = BasicBlock::Create(ctxt, "offload.init", loopFun, nullptr);

        IRBuilder<> inplaceBuilder(offloadInitBlock);
        
        AllocaInst* dataAlloca = inplaceBuilder.CreateAlloca(dataStruct, nullptr, "data");
        for (int i = 0; i < upwardsExposed.size(); ++i) {
            Value* v = upwardsExposed[i];
            Value* gep = inplaceBuilder.CreateStructGEP(dataStruct, dataAlloca, i, v->getName() + ".in");
            StoreInst* storeValue = inplaceBuilder.CreateStore(v, gep);
        }

        std::vector<Type*> uliFunTyArgs = {intTy, voidPtrTy, voidPtrTy};
        FunctionType* uliFunTy = FunctionType::get(voidTy, ArrayRef<Type*>(uliFunTyArgs), false);
        Value* engFuncVoidPtr = inplaceBuilder.CreatePtrToInt(engFunc, voidPtrTy, engFunc->getName() + ".voidptr");
        Value* dataVoidPtr = inplaceBuilder.CreatePtrToInt(dataAlloca, voidPtrTy, dataAlloca->getName() + ".voidptr");
        std::vector<Value*> uliArgs = {inplaceBuilder.getInt32(1), engFuncVoidPtr, dataVoidPtr};
        CallInst* callUli = inplaceBuilder.CreateCall(uliFunTy, loopFun, ArrayRef<Value*>(uliArgs));

        BasicBlock* loopPred = L->getLoopPredecessor();
        Instruction* last;
        for (Instruction& I : *loopPred) { last = &I; }
        last->replaceSuccessorWith(loopEntry, offloadInitBlock);
        
        /* ============================== */

        // Move loop to other function
        BasicBlock* engFuncEntry = BasicBlock::Create(ctxt, "entry", engFunc);
        // Only one arg, data:
        Value* engFuncData = &*engFunc->arg_begin();
        IRBuilder<> engFuncBuilder(engFuncEntry);
        
        engFuncBuilder.CreateRetVoid();

        /* ============================== */

        // TODO: move loop blocks

        // After moving the loop to engFun, now we can do this:
        for (int i = 0; i < downwardsExposed.size(); ++i) {
            Value* v = downwardsExposed[i];
            Value* gep = inplaceBuilder.CreateStructGEP(dataStruct, dataAlloca, i + upwardsExposed.size(), v->getName() + ".ptr");
            LoadInst* loadValue = inplaceBuilder.CreateLoad(downwardsExposedTp[i], gep, v->getName() + ".out");
            loopFun->replaceUsesOfWith(v, loadValue);
        }
        inplaceBuilder.CreateBr(exitBlock);
        exitBlock->replacePhiUsesWith(exitBlock->getSinglePredecessor(), offloadInitBlock);
        //loopBody->replaceSuccessorsPhiUsesWith(offloadInitBlock);
        errs() << "Offload block: " << *offloadInitBlock << "\n";
    }

    bool pointerChaseLoop(Loop* L) {
        std::vector<PHINode*> inds = std::vector<PHINode*>();
        std::vector<PHINode*> reds = std::vector<PHINode*>();
        getIndRedVars(L, inds, reds);
        if (inds.size() > 0 && reds.size() > 0) {
            for (PHINode* phi : inds) {
                errs() << "Found ind var: " << *phi << "\n";
            }
            for (PHINode* phi : reds) {
                errs() << "Found red var: " << *phi << "\n";
            }
            offloadToEngine(L);
            return true;
        } else {
            errs() << "Not a pointer-chasing reduction loop\n";
        }
        return false;
    }

    bool affineLoop(Loop* L, PHINode* indvar, ScalarEvolution& SE) {
        errs() << "IND VAR: " << *indvar << "\n";
        
        std::vector<PHINode*> inds;
        std::vector<PHINode*> reds;
        getIndRedVars(L, inds, reds);
        // TODO: this misidentifies the ind var as a red var
        if (reds.size() > 0) {
            for (PHINode* phi : inds) {
                errs() << "Found ind var: " << *phi << "\n";
            }
            for (PHINode* phi : reds) {
                errs() << "Found red var: " << *phi << "\n";
            }
        }

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
            BasicBlock* body = getLoopBody(L);
            errs() << *body << "\n";
            bool is_reduction = body && findReductionVar(getLoopBody(L), load);
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
            errs() << "Body: " << *getLoopBody(L) << "\n";

            auto &SE = getAnalysis<ScalarEvolutionWrapperPass>().getSE();

            PHINode* indvar = L->getInductionVariable(SE);

            InductionDescriptor inddesc;
            L->getInductionDescriptor(SE, inddesc);

            // Can only offload if there is a single entry and exit
            std::vector<BasicBlock*> exits;
            loopExitBlocks(L, exits);
            if (exits.size() != 1 && L->getLoopPredecessor()) {
                errs() << "Loop doesn't have a single entry or exit\n";
                return false;
            }

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
