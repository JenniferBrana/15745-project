// 15-745 S16 Assignment 2: dataflow.cpp
// Group: Colin McDonald, Jennifer Brana
////////////////////////////////////////////////////////////////////////////////

#include "dataflow.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>

namespace llvm {

	// Add code for your dataflow abstraction here.

    void print_bitvector(llvm::raw_ostream& os, BitVector bv) {
        for (int i = 0; i < bv.size(); ++i) {
            os << bv[i];
        }
    }

    void dataflow_meet_intersection(BitVector& a, BitVector b) { a &= b; }
    void dataflow_meet_union(BitVector& a, BitVector b) { a |= b; }

    BitVector* cloneBitVector(BitVector b) {
        BitVector* c = new BitVector(b.size());
        *c |= b;
        return c;
    }

    DenseMap<Value*, Value*> phiMap(Function* fun) {
        DenseMap<Value*, Value*> m = DenseMap<Value*, Value*>();
        for (auto& bb : *fun) {
            for (auto& i : bb) {
                if (PHINode* phi = dyn_cast<PHINode>(&i)) {
                    Value* target = &i;
                    // Need to consider case where we essentially "nest" phi functions
                    if (m.count(&i)) {
                        target = m[&i];
                    }
                    for (auto ii = i.op_begin(), ie = i.op_end(); ii != ie; ++ii) {
                        Value* op = ii->get();
                        m.insert({op, target});
                    }
                }
            }
        }
        return m;
    }

    /*std::vector<BasicBlock*> functionBlocks(Function* fun) {
        BasicBlock* entry = fun->getEntryBlock();
        std::vector<BasicBlock*> blocks;
        blocks.push_back(entry);
        for (BasicBlock &b : *fun) {
            if (&b != entry) { blocks.push_back(&b); }
        }
        return blocks;
    }*/

    DenseMap<BasicBlock*, BlockState*> analyze(Framework framework, Function* fun) {
        // Calculate phi mappings
        //DenseMap<StringRef, StringRef> phi = phiMap(fun);

        // Determine number of basic blocks and elements in domain
        unsigned int numBBs = fun->size();

        // Initialize state
        DenseMap<BasicBlock*, BlockState*> state = DenseMap<BasicBlock*, BlockState*>(numBBs);
        for (BasicBlock &b : *fun) {
            state[&b] = new BlockState(framework.domainSize, framework.interior);
        }
            
        // Initialize queue
        std::queue<BasicBlock*> W = std::queue<BasicBlock*>();
        if (framework.direction == Forward) {
            // Add entry block to W
            BasicBlock* entry = &fun->getEntryBlock();
            W.push(entry);
            state[entry] = new BlockState(framework.domainSize, framework.boundary);
        } else {
            // Add all blocks with a return instruction to W
            for (BasicBlock &b : *fun) {
                // Return instructions always occur at end of block
                if (isa<llvm::ReturnInst>(b.getTerminator())) {
                    W.push(&b);
                    state[&b] = new BlockState(framework.domainSize, framework.boundary);
                }
            }
        }
        
        // Now push the rest of the blocks
        for (BasicBlock &b : *fun) {
            W.push(&b);
        }

        // Loop until worklist is empty
        while (!W.empty()) {
            BasicBlock* b = W.front();
            W.pop();
            BlockState* s = state[b];
            // If b is not marked todo, no need to run this again
            if (!s->todo) continue;
            s->todo = false;
            if (framework.direction == Forward) {
                // First let's calculate in[b]
                BitVector* in = s->in;
                bool first = true;
                for (BasicBlock* pred : predecessors(b)) {
                    if (first) {
                        first = false;
                        // Now overwrite the data stored in `in` with pred's out
                        *in = *(state[pred]->out);
                    } else {
                        framework.meet(*in, *(state[pred]->out));
                    }
                }
                    
                // Now compute out[b]
                BitVector out = BitVector(framework.domainSize);
                out = *in;
                framework.transfer(out, b);
                    
                // If we updated out with this iteration
                if (out != *s->out) {
                    *s->out = out;
                    for (BasicBlock* succ : successors(b)) {
                        state[succ]->todo = true;
                        W.push(succ);
                    }
                }
            } else { // backward
                // First let's calculate out[b]
                BitVector* out = s->out;
                bool first = true;
                for (BasicBlock* succ : successors(b)) {
                    if (first) {
                        first = false;
                        *out = *(state[succ]->in);
                    } else {
                        framework.meet(*out, *(state[succ]->in));
                    }
                }
                    
                // Now compute in[b]
                BitVector in = BitVector(framework.domainSize);
                in = *out;
                framework.transfer(in, b);
                    
                // If we updated out with this iteration
                if (in != *s->in) {
                    *s->in = in;
                    for (BasicBlock* pred : predecessors(b)) {
                        state[pred]->todo = true;
                        W.push(pred);
                    }
                }
            }
        }
        
        return state;
    }
    
}


/*int main(int argc, char* argv[]) {
    llvm::outs() << "Hello!\n";
    return 0;
}*/
