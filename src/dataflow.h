// 15-745 S16 Assignment 2: dataflow.h
// Group: Colin McDonald, Jennifer Brana
////////////////////////////////////////////////////////////////////////////////

#ifndef __CLASSICAL_DATAFLOW_H__
#define __CLASSICAL_DATAFLOW_H__

#include <stdio.h>
#include <iostream>
#include <queue>
#include <vector>

#include "llvm/IR/Instructions.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/IR/CFG.h"

//using namespace llvm;

namespace llvm {

    void print_bitvector(llvm::raw_ostream& os, BitVector bv);    

    BitVector* cloneBitVector(BitVector b);
    
    enum Direction {
        Forward,
        Backward
    };

    /*enum InitValue {
        Top,
        Bot
    };*/

	// Add definitions (and code, depending on your strategy) for your dataflow
	// abstraction here.
    
    class BlockState {
        public:
        BitVector* in;
        BitVector* out;
        bool todo;
        
        BlockState(unsigned int domsize, bool defval) {
            in = new BitVector(domsize, defval);
            out = new BitVector(domsize, defval);
            todo = true;
        }
        ~BlockState() { delete in; delete out; }
    };

    DenseMap<Value*, Value*> phiMap(Function* fun);

    void dataflow_meet_intersection(BitVector& a, BitVector b);
    void dataflow_meet_union(BitVector& a, BitVector b);
    
    //template<typename Domain> // can be value, var, instr, ...
    struct Framework {
        //public:
        Direction direction;
        unsigned int domainSize; // how many entries in BitVector?
        bool boundary; // boundary condition (entry / exit)
        bool interior; // for interior points (in / out, for each node's initial value)
        void (*transfer)(BitVector& b, BasicBlock* block);
        void (*meet)(BitVector &a, BitVector b);

        Framework(Direction direction,
                  unsigned int domainSize,
                  bool boundary,
                  bool interior,
                  void (*transfer)(BitVector&, BasicBlock*),
                  void (*meet)(BitVector &, BitVector)) :
            direction(direction),
            domainSize(domainSize),
            boundary(boundary),
            interior(interior),
            transfer(transfer),
            meet(meet) {};
    };

    DenseMap<BasicBlock*, BlockState*> analyze(Framework framework, Function* fun);

    std::vector<BasicBlock*> functionBlocks(Function* fun);
}

#endif
