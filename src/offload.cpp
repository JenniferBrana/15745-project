#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/IRBuilder.h"
#include "loop-exposed-vars.h"

namespace llvm {

    void valueTypes(std::vector<Value*> values, std::vector<Type*> &types) {
        for (size_t i = 0; i < values.size(); ++i) {
            types[i] = values[i]->getType();
        }
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
            FunctionType* funTy = FunctionType::get(Type::getInt64Ty(mod->getContext()),
                                                    ArrayRef<Type*>(), false);
            mainStartFunction = Function::Create(funTy,
                                                 linkage,
                                                 "main_start",
                                                 mod);
        }
        return mainStartFunction;
    }

    Function* mainEndFunction = NULL;
    
    Function* getMainEndFunction(Module* mod) {
        if (!mainEndFunction) {
            FunctionType* funTy = FunctionType::get(Type::getVoidTy(mod->getContext()),
                                                    ArrayRef<Type*>(Type::getInt64Ty(mod->getContext())), false);
            mainEndFunction = Function::Create(funTy,
                                                 linkage,
                                                 "main_end",
                                                 mod);
        }
        return mainEndFunction;
    }

    void modifyReturningBlocks(Function* fun, Function* endFunc, Value* thread_id) {
        for (BasicBlock& B : *fun) {
            Instruction* I = NULL;
            // Get last instruction
            for (Instruction& I2 : B) { I = &I2; }
            if (ReturnInst* ri = dyn_cast<ReturnInst>(I)) {
                CallInst::Create(endFunc, ArrayRef<Value*>(thread_id), "", ri);
                //ri->insertBefore();
            }
        }
    }

    void modifyMain(Module* mod) {
        /*Function* last = NULL;
        for (Function& F : mod->functions()) {
            last = &F;
        }
        errs() << "Last function = " << last->getName() << "\n";*/
        Function* mainFunc = mod->getFunction("main");
        Function* mainStartFunc = getMainStartFunction(mod);
        Function* mainEndFunc = getMainEndFunction(mod);
        BasicBlock* oldEntry = &mainFunc->getEntryBlock();
        BasicBlock* newEntry = BasicBlock::Create(mod->getContext(), "newEntry", mainFunc, oldEntry);
        IRBuilder<> builder(newEntry);
        CallInst* callMainStart = builder.CreateCall(mainStartFunc->getFunctionType(), mainStartFunc, ArrayRef<Value*>());
        builder.CreateBr(oldEntry);
        modifyReturningBlocks(mainFunc, mainEndFunc, callMainStart);
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
    }
}
