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
                    //errs() << "Load " << *li << "\n";
                    if (uses_instr(indvar, li)) {
                        //errs() << "Uses indvar!\n";
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
            //errs() << I << ": uses itself = " << a << "\n";
            //errs() << I << ": uses indvar = " << b << "\n";
            if (a && b) {
                //errs() << "This is a reduction! " << I << "\n";
                return true;
            }
        }
        return false;
    }

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

    Function* uliFunction = NULL;

    GlobalValue::LinkageTypes linkage = Function::ExternalLinkage;
    
    Function* getULIfunction(Module* mod) {
        if (!uliFunction) {
            Type* int64ty = Type::getInt64Ty(mod->getContext());
            Type* voidTy = Type::getVoidTy(mod->getContext());
            Type *voidPtrTy = llvm::PointerType::getUnqual(voidTy);

            std::vector<Type*> params = {int64ty, voidPtrTy, voidPtrTy};
            FunctionType* funTy = FunctionType::get(int64ty, ArrayRef<Type*>(params), false);

            uliFunction = Function::Create(funTy,
                                           linkage,
                                           "send_request_uli",
                                           mod);
        }
        return uliFunction;
    }

    Function* mainStartFunction = NULL;
    
    Function* getMainStartFunction(Module* mod) {
        if (!mainStartFunction) {
            FunctionType* funTy = FunctionType::get(Type::getVoidTy(mod->getContext()),
                                                    ArrayRef<Type*>(), false);
            mainStartFunction = Function::Create(funTy,
                                                 linkage,
                                                 "main_start",
                                                 mod);
        }
        return mainStartFunction;
    }

    void modifyMain(Module* mod) {
        /*Function* last = NULL;
        for (Function& F : mod->functions()) {
            last = &F;
        }
        errs() << "Last function = " << last->getName() << "\n";*/
        Function* mainFunc = mod->getFunction("main");
        Function* mainStartFunc = getMainStartFunction(mod);
        BasicBlock* oldEntry = &mainFunc->getEntryBlock();
        BasicBlock* newEntry = BasicBlock::Create(mod->getContext(), "newEntry", mainFunc, oldEntry);
        IRBuilder<> builder(newEntry);
        CallInst* callMainStart = builder.CreateCall(mainStartFunc->getFunctionType(), mainStartFunc, ArrayRef<Value*>());
        builder.CreateBr(oldEntry);
        errs() << "New entry: " << *newEntry << "\n";
    }

    void replaceBlockSuccessor(BasicBlock* B, BasicBlock* oldBlock, BasicBlock* newBlock) {
        // Before: B -> oldBlock
        // After:  B -> newBlock
        Instruction* last;
        for (Instruction &I : *B) { last = &I; }
        last->replaceSuccessorWith(oldBlock, newBlock);
    }

    void replaceUsesOfWithInFunction(Function* F, Value* oldValue, Value* newValue) {
        for (BasicBlock &B : *F) {
            for (Instruction &I : B) {
                I.replaceUsesOfWith(oldValue, newValue);
            }
        }
    }

    void offloadToEngine(Loop* L) {
        // Determine loop info
        Module* loopMod = getLoopModule(L);
        Function* loopFun = getLoopFunction(L);
        BasicBlock* loopBody = getLoopBody(L);
        BasicBlock* loopEntry = getLoopEntry(L);
        BasicBlock* loopPred = L->getLoopPredecessor();
        //errs() << "Loop body: " << *loopBody << "\n";
        //errs() << "Loop entry: " << *loopEntry << "\n";
        //errs() << "Loop pred: " << *loopPred << "\n";

        std::vector<BasicBlock*> loopBlocks = getLoopBlocks(L);
        LLVMContext& ctxt = loopFun->getContext();

        // loop must have only 1 entry and exit, to have gotten here
        std::vector<BasicBlock*> exits;
        loopExitBlocks(L, exits);
        BasicBlock* loopExit = exits[0];

        //errs() << "Loop exit: " << *loopExit << "\n";

        //errs() << "Original function: " << *loopFun << "\n";

        // Blocks inside the loop that exit
        std::vector<BasicBlock*> loopExitings;
        loopExitingBlocks(L, loopExitings);

        // Determine upwards- and downwards-exposed variables
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

        // Make struct to store exposed vars
        ArrayRef<Type*> dataTpArray = ArrayRef<Type*>(exposedTp);
        StructType* dataStruct = StructType::create(ctxt, dataTpArray);
            
        //for (Value* v : downwardsExposed) { errs() << "down-exposed: " << *v << "\n"; }
        //for (Value* v : upwardsExposed) { errs() << "  up-exposed: " << *v << "\n"; }
        
        // Create engine function, to which we will offload the loop
        Type* voidTy = Type::getVoidTy(ctxt);
        Type* intTy = Type::getInt32Ty(ctxt);
        Type *voidPtrTy = llvm::PointerType::getUnqual(voidTy);
        std::vector<Type*> paramsVoidPtr = {voidPtrTy};
        FunctionType* funTy = FunctionType::get(voidTy, ArrayRef<Type*>(paramsVoidPtr), false);
        Function* engFunc = Function::Create(funTy, GlobalValue::LinkageTypes::PrivateLinkage, "identify_streams_eng_func", loopMod);
            
        // Create block that sets up the input data struct
        // This replaces the loop in its original context
        BasicBlock *offloadBlock = BasicBlock::Create(ctxt, "offload.init", loopFun, nullptr);
        IRBuilder<> inplaceBuilder(offloadBlock);
        
        // Store exposed variables in data struct, which is later passed to engine function
        AllocaInst* dataAlloca = inplaceBuilder.CreateAlloca(dataStruct, nullptr, "data");
        for (int i = 0; i < upwardsExposed.size(); ++i) {
            Value* v = upwardsExposed[i];
            Value* gep = inplaceBuilder.CreateStructGEP(dataStruct, dataAlloca, i, v->getName() + ".in");
            StoreInst* storeValue = inplaceBuilder.CreateStore(v, gep);
        }

        // Create ULI function call and args
        std::vector<Type*> uliFunTyArgs = {intTy, voidPtrTy, voidPtrTy};
        FunctionType* uliFunTy = FunctionType::get(voidTy, ArrayRef<Type*>(uliFunTyArgs), false);
        Value* engFuncVoidPtr = inplaceBuilder.CreatePtrToInt(engFunc, voidPtrTy, engFunc->getName() + ".voidptr");
        Value* dataVoidPtr = inplaceBuilder.CreatePtrToInt(dataAlloca, voidPtrTy, dataAlloca->getName() + ".voidptr");
        std::vector<Value*> uliArgs = {inplaceBuilder.getInt32(1), engFuncVoidPtr, dataVoidPtr};
        Function* uliFun = getULIfunction(loopMod);
        CallInst* callUli = inplaceBuilder.CreateCall(uliFunTy, uliFun, ArrayRef<Value*>(uliArgs));

        // Make predecessor to loop instead go to the 
        replaceBlockSuccessor(loopPred, loopEntry, offloadBlock);

        // ================================================== //
        // Move loop to engine function

        // Create entry and exit blocks in engine function,
        // which load and store exposed vars respectively
        BasicBlock* engFuncEntry = BasicBlock::Create(ctxt, "entry", engFunc);
        BasicBlock* engFuncExit = BasicBlock::Create(ctxt, "exit", engFunc);
        IRBuilder<> entryBuilder(engFuncEntry);
        IRBuilder<> exitBuilder(engFuncExit);

        // Engine function has one arg, data:
        Value* engFuncData = &*engFunc->arg_begin();
        
        // Load values from data struct in entry block
        std::vector<Value*> upwardsExposedDataValues;
        for (int i = 0; i < upwardsExposed.size(); ++i) {
            Value* gep = entryBuilder.CreateStructGEP(dataStruct, engFuncData, i, upwardsExposed[i]->getName() + ".ptr");
            LoadInst* dataValue = entryBuilder.CreateLoad(upwardsExposedTp[i], gep, upwardsExposed[i]->getName() + ".in");
            upwardsExposedDataValues.push_back(dataValue);
        }
        // Now jump to loop entry
        entryBuilder.CreateBr(loopEntry);
        // And replace phi node operand labels with
        // the new predecessor block (engFuncEntry)
        loopEntry->replacePhiUsesWith(loopPred, engFuncEntry);

        // Move loop to engFunc
        for (BasicBlock* B : loopBlocks) {
            B->moveBefore(engFuncExit);
            replaceBlockSuccessor(B, loopExit, engFuncExit);
        }
        
        // Store updated exposed vars into data struct in exit block
        for (int i = 0; i < downwardsExposed.size(); ++i) {
            Value* v = downwardsExposed[i];
            Value* gep = exitBuilder.CreateStructGEP(dataStruct, engFuncData, upwardsExposed.size() + i, v->getName() + ".ptr");
            StoreInst* storeValue = exitBuilder.CreateStore(v, gep);
        }
        // Engine function has void return
        exitBuilder.CreateRetVoid();

        // Replace original exposed vars with new vars loaded from data struct
        for (int i = 0; i < upwardsExposed.size(); ++i) {
            replaceUsesOfWithInFunction(engFunc, upwardsExposed[i], upwardsExposedDataValues[i]);
            //engFunc->replaceUsesOfWith(upwardsExposed[i], upwardsExposedDataValues[i]);
        }

        // TODO: may need to also move exit block to engine function? Not sure...

        // ================================================== //
        // Back where the loop was originally:
        // Load updated values from data struct
        for (int i = 0; i < downwardsExposed.size(); ++i) {
            Value* v = downwardsExposed[i];
            Value* gep = inplaceBuilder.CreateStructGEP(dataStruct, dataAlloca, i + upwardsExposed.size(), v->getName() + ".ptr");
            LoadInst* loadValue = inplaceBuilder.CreateLoad(downwardsExposedTp[i], gep, v->getName() + ".out");
            
            // For some reason, loopFun->replaceUsesOfWith(v, loadValue) doesn't work...
            replaceUsesOfWithInFunction(loopFun, v, loadValue);
        }
        // Jump where the loop exits to
        inplaceBuilder.CreateBr(loopExit);
        // Replace phi labels
        for (BasicBlock* B : loopExitings) {
            loopExit->replacePhiUsesWith(B, offloadBlock);
        }
        //loopExit->replacePhiUsesWith(loopExit->getSinglePredecessor(), offloadBlock);
        //loopBody->replaceSuccessorsPhiUsesWith(offloadBlock);
        //errs() << "Offload block: " << *offloadBlock << "\n";
        //errs() << "Generated function: " << *engFunc << "\n";
        //errs() << "Updated function: " << *loopFun << "\n";

        errs() << "Offloaded loop in function " << loopFun->getName() << "\n";
    }


    bool pointerChaseLoop(Loop* L) {
        std::vector<PHINode*> inds = std::vector<PHINode*>();
        std::vector<PHINode*> reds = std::vector<PHINode*>();
        getIndRedVars(L, inds, reds);
        return inds.size() > 0 && reds.size() > 0;
    }

    bool affineLoop(Loop* L, PHINode* indvar, ScalarEvolution& SE) {
        if (indvar == nullptr) { return false; }
        //errs() << "IND VAR: " << *indvar << "\n";
        
        std::vector<PHINode*> inds;
        std::vector<PHINode*> reds;
        getIndRedVars(L, inds, reds);
        // TODO: this misidentifies the ind var as a red var
        /*if (reds.size() > 0) {
            for (PHINode* phi : inds) { errs() << "Found ind var: " << *phi << "\n"; }
            for (PHINode* phi : reds) { errs() << "Found red var: " << *phi << "\n"; }
        }*/

        Loop::LoopBounds* bounds = L->getBounds(SE).getPointer();
        return bounds != nullptr && reds.size() > 0;
        /*if (bounds) {
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

        return true;*/
    }

    class IdentifyStreams : public LoopPass {
    public:
        static char ID;
        IdentifyStreams() : LoopPass(ID) {}
        std::set<BasicBlock*> doneAlready;

        virtual bool runOnLoop(Loop *L, LPPassManager& LPM) {
            //errs() << "--------------------------------------------------\n";
            //errs() << "Loop: " << *L << "\n";
            //errs() << "Body: " << *getLoopBody(L) << "\n";

            auto &SE = getAnalysis<ScalarEvolutionWrapperPass>().getSE();

            PHINode* indvar = L->getInductionVariable(SE);

            InductionDescriptor inddesc;
            L->getInductionDescriptor(SE, inddesc);

            // Can only offload if there is a single entry and exit
            std::vector<BasicBlock*> exits;
            loopExitBlocks(L, exits);
            if (exits.size() != 1 && L->getLoopPredecessor()) {
                //errs() << "Loop doesn't have a single entry or exit\n";
                return false;
            }

            BasicBlock* body = getLoopBody(L);
            if ((pointerChaseLoop(L) || affineLoop(L, indvar, SE)) && doneAlready.count(body) == 0) {                
                // If this is the first offload, insert pthread stuff into the main function
                if (doneAlready.size() == 0) {
                    modifyMain(getLoopModule(L));
                }
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
