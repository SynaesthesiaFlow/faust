/************************************************************************
 ************************************************************************
    FAUST compiler
    Copyright (C) 2003-2015 GRAME, Centre National de Creation Musicale
    ---------------------------------------------------------------------
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 ************************************************************************
 ************************************************************************/

#ifndef _FIR_INTERPRETER_OPTIMIZER_H
#define _FIR_INTERPRETER_OPTIMIZER_H

#include <string>
#include <assert.h>
#include <iostream>
#include <map>

#include "interpreter_bytecode.hh"

// Block optimizer : compact list of instructions in a more efficient single one

static std::map<FIRInstruction::Opcode, FIRInstruction::Opcode> gFIRMath2Heap;
static std::map<FIRInstruction::Opcode, FIRInstruction::Opcode> gFIRMath2Stack;
static std::map<FIRInstruction::Opcode, FIRInstruction::Opcode> gFIRMath2StackValue;
static std::map<FIRInstruction::Opcode, FIRInstruction::Opcode> gFIRMath2Value;
static std::map<FIRInstruction::Opcode, FIRInstruction::Opcode> gFIRMath2ValueInvert;

static std::map<FIRInstruction::Opcode, FIRInstruction::Opcode> gFIRExtendedMath2Heap;
static std::map<FIRInstruction::Opcode, FIRInstruction::Opcode> gFIRExtendedMath2Stack;
static std::map<FIRInstruction::Opcode, FIRInstruction::Opcode> gFIRExtendedMath2StackValue;
static std::map<FIRInstruction::Opcode, FIRInstruction::Opcode> gFIRExtendedMath2Value;
static std::map<FIRInstruction::Opcode, FIRInstruction::Opcode> gFIRExtendedMath2ValueInvert;

//=======================
// Optimization
//=======================

template <class T> struct FIRInstructionOptimizer;

// Copy (= identity) optimizer (used to test...)
template <class T>
struct FIRInstructionCopyOptimizer : public FIRInstructionOptimizer<T>  {
    
    FIRInstructionCopyOptimizer()
    {
        //std::cout << "FIRInstructionCopyOptimizer" << std::endl;
    }
    
    virtual FIRBasicInstruction<T>* rewrite(InstructionIT cur, InstructionIT& end)
    {
        end = cur + 1;
        return (*cur)->copy();
    }
};

// Cast optimizer
template <class T>
struct FIRInstructionCastOptimizer : public FIRInstructionOptimizer<T>  {
    
    
    FIRInstructionCastOptimizer()
    {
        //std::cout << "FIRInstructionCastOptimizer" << std::endl;
    }
    
    virtual FIRBasicInstruction<T>* rewrite(InstructionIT cur, InstructionIT& end)
    {
        FIRBasicInstruction<T>* inst1 = *cur;
        FIRBasicInstruction<T>* inst2 = *(cur + 1);
        
        if (inst1->fOpcode == FIRInstruction::kLoadInt && inst2->fOpcode == FIRInstruction::kCastReal) {
            end = cur + 2;
            return new FIRBasicInstruction<T>(FIRInstruction::kCastRealHeap, 0, 0, inst1->fOffset1, 0);
        } else if (inst1->fOpcode == FIRInstruction::kLoadReal && inst2->fOpcode == FIRInstruction::kCastInt) {
            end = cur + 2;
            return new FIRBasicInstruction<T>(FIRInstruction::kCastIntHeap, 0, 0, inst1->fOffset1, 0);
        } else {
            end = cur + 1;
            return (*cur)->copy();
        }
    }
};

// Rewrite indexed Load/Store as simple Load/Store
template <class T>
struct FIRInstructionLoadStoreOptimizer : public FIRInstructionOptimizer<T> {
    
    
    FIRInstructionLoadStoreOptimizer()
    {
        //std::cout << "FIRInstructionLoadStoreOptimizer" << std::endl;
    }
    
    FIRBasicInstruction<T>* rewrite(InstructionIT cur, InstructionIT& end)
    {
        FIRBasicInstruction<T>* inst1 = *cur;
        FIRBasicInstruction<T>* inst2 = *(cur + 1);
        
        if (inst1->fOpcode == FIRInstruction::kIntValue && inst2->fOpcode == FIRInstruction::kLoadIndexedReal) {
            end = cur + 2;
            return new FIRBasicInstruction<T>(FIRInstruction::kLoadReal, 0, 0, inst1->fIntValue + inst2->fOffset1, 0);
        } else if (inst1->fOpcode == FIRInstruction::kIntValue && inst2->fOpcode == FIRInstruction::kLoadIndexedInt) {
            end = cur + 2;
            return new FIRBasicInstruction<T>(FIRInstruction::kLoadInt, 0, 0, inst1->fIntValue + inst2->fOffset1, 0);
        } else if (inst1->fOpcode == FIRInstruction::kIntValue && inst2->fOpcode == FIRInstruction::kStoreIndexedReal) {
            end = cur + 2;
            return new FIRBasicInstruction<T>(FIRInstruction::kStoreReal, 0, 0, inst1->fIntValue + inst2->fOffset1, 0);
        } else if (inst1->fOpcode == FIRInstruction::kIntValue && inst2->fOpcode == FIRInstruction::kStoreIndexedInt) {
            end = cur + 2;
            return new FIRBasicInstruction<T>(FIRInstruction::kStoreInt, 0, 0, inst1->fIntValue + inst2->fOffset1, 0);
        } else {
            end = cur + 1;
            return (*cur)->copy();
        }
    }
    
};

// Rewrite heap Load/Store as Move or direct Value store
template <class T>
struct FIRInstructionMoveOptimizer : public FIRInstructionOptimizer<T> {
    
    FIRInstructionMoveOptimizer()
    {
        //std::cout << "FIRInstructionMoveOptimizer" << std::endl;
    }
    
    FIRBasicInstruction<T>* rewrite(InstructionIT cur, InstructionIT& end)
    {
        FIRBasicInstruction<T>* inst1 = *cur;
        FIRBasicInstruction<T>* inst2 = *(cur + 1);
        
        // Optimize Heap Load/Store as Move
        if (inst1->fOpcode == FIRInstruction::kLoadReal && inst2->fOpcode == FIRInstruction::kStoreReal) {
            end = cur + 2;
            return new FIRBasicInstruction<T>(FIRInstruction::kMoveReal, 0, 0, inst2->fOffset1, inst1->fOffset1);   // reverse order
        } else if (inst1->fOpcode == FIRInstruction::kLoadInt && inst2->fOpcode == FIRInstruction::kStoreInt) {
            end = cur + 2;
            return new FIRBasicInstruction<T>(FIRInstruction::kMoveInt, 0, 0, inst2->fOffset1, inst1->fOffset1);    // reverse order
        // Optimize Value Load/Store as direct value Store
        } else if (inst1->fOpcode == FIRInstruction::kRealValue && inst2->fOpcode == FIRInstruction::kStoreReal) {
            end = cur + 2;
            return new FIRBasicInstruction<T>(FIRInstruction::kStoreRealValue, 0, inst1->fRealValue, inst2->fOffset1, 0);
        } else if (inst1->fOpcode == FIRInstruction::kIntValue && inst2->fOpcode == FIRInstruction::kStoreInt) {
            end = cur + 2;
            return new FIRBasicInstruction<T>(FIRInstruction::kStoreIntValue, inst1->fIntValue, 0, inst2->fOffset1, 0);
        } else {
            end = cur + 1;
            return (*cur)->copy();
        }
    }
    
};


/*
 opcode 12 kMoveReal int 0 real 0 offset1 120322 offset2 120321
 opcode 12 kMoveReal int 0 real 0 offset1 120321 offset2 120320
 opcode 12 kMoveReal int 0 real 0 offset1 120325 offset2 120324
 opcode 12 kMoveReal int 0 real 0 offset1 120324 offset2 120323
 opcode 12 kMoveReal int 0 real 0 offset1 120328 offset2 120327
 opcode 12 kMoveReal int 0 real 0 offset1 120327 offset2 120326
 
 ==>  opcode 13 kBlockPairMoveReal int 0 real 0 offset1 120321 offset2 120327
*/

template <class T>
struct FIRInstructionBlockMoveOptimizer : public FIRInstructionOptimizer<T> {
    
    FIRInstructionBlockMoveOptimizer()
    {
        //std::cout << "FIRInstructionBlockMoveOptimizer" << std::endl;
    }
    
    FIRBasicInstruction<T>* rewrite(InstructionIT cur, InstructionIT& end)
    {
        FIRBasicInstruction<T>* inst = *cur;
        InstructionIT next = cur;
        
        int begin_move = -1;
        int end_move = -1;
        int last_offset = -1;
        
        while (inst->fOpcode != FIRInstruction::kReturn && inst->fOpcode == FIRInstruction::kMoveReal) {
            if ((inst->fOffset1 == inst->fOffset2 + 1) && ((last_offset == -1) || (inst->fOffset1 == last_offset + 2))) {
                if (begin_move == -1) { begin_move = inst->fOffset2; }
                last_offset = end_move = inst->fOffset1;
                inst = *(++next);
            } else {
                break;
            }
        }
        
        if (begin_move != -1 && end_move != -1 && ((end_move - begin_move) > 4)) {
            //std::cout << "FIRInstructionBlockMoveOptimizer " << begin_move  << " " << end_move << std::endl;
            end = next;
            return new FIRBasicInstruction<T>(FIRInstruction::kBlockPairMoveReal, 0, 0, begin_move, end_move);
        } else {
            end = cur + 1;
            return (*cur)->copy();
        }
    }
    
};

/*
 opcode 12 kMoveReal int 0 real 0 offset1 120322 offset2 120321
 opcode 12 kMoveReal int 0 real 0 offset1 120321 offset2 120320
 opcode 12 kMoveReal int 0 real 0 offset1 120325 offset2 120324
 opcode 12 kMoveReal int 0 real 0 offset1 120324 offset2 120323
 
 ==>
 
 opcode 14 kPairMoveReal int 0 real 0 offset1 120322 offset2 120321
 opcode 14 kPairMoveReal int 0 real 0 offset1 120325 offset2 120324
*/

template <class T>
struct FIRInstructionPairMoveOptimizer : public FIRInstructionOptimizer<T> {
    
    FIRInstructionPairMoveOptimizer()
    {
        //std::cout << "FIRInstructionPairMoveOptimizer" << std::endl;
    }
    
    FIRBasicInstruction<T>* rewrite(InstructionIT cur, InstructionIT& end)
    {
        FIRBasicInstruction<T>* inst1 = *cur;
        FIRBasicInstruction<T>* inst2 = *(cur + 1);
        
        if (inst1->fOpcode == FIRInstruction::kMoveReal
            && inst2->fOpcode == FIRInstruction::kMoveReal
            && (inst1->fOffset1 == (inst1->fOffset2 + 1))
            && (inst2->fOffset1 == (inst2->fOffset2 + 1))
            && (inst2->fOffset1 == inst1->fOffset2)) {
            end = cur + 2;
            //std::cout << "FIRInstructionPairMoveOptimizer" << inst1->fOffset1 << " " << inst2->fOffset1 << std::endl;
            return new FIRBasicInstruction<T>(FIRInstruction::kPairMoveReal, 0, 0, inst1->fOffset1, inst2->fOffset1);
        } else if (inst1->fOpcode == FIRInstruction::kMoveInt
                && inst2->fOpcode == FIRInstruction::kMoveInt
                && (inst1->fOffset1 == (inst1->fOffset2 + 1))
                && (inst2->fOffset1 == (inst2->fOffset2 + 1))
                && (inst2->fOffset1 == inst1->fOffset2)) {
                end = cur + 2;
                //std::cout << "FIRInstructionPairMoveOptimizer" << inst1->fOffset1 << " " << inst2->fOffset1 << std::endl;
                return new FIRBasicInstruction<T>(FIRInstruction::kPairMoveInt, 0, 0, inst1->fOffset1, inst2->fOffset1);
        } else {
            end = cur + 1;
            return (*cur)->copy();
        }
    }
    
};

// Rewrite math operations as 'heap', 'stack' or 'Value' versions
template <class T>
struct FIRInstructionMathOptimizer : public FIRInstructionOptimizer<T> {
    
    FIRInstructionMathOptimizer()
    {
        //std::cout << "FIRInstructionMathOptimizer" << std::endl;
    }
    
    FIRBasicInstruction<T>* rewrite(InstructionIT cur, InstructionIT& end)
    {
        FIRBasicInstruction<T>* inst1 = *cur;
        FIRBasicInstruction<T>* inst2 = *(cur + 1);
        FIRBasicInstruction<T>* inst3 = *(cur + 2);
        
        //======
        // HEAP
        //======
        
            //======
            // MATH
            //======
        
        // kLoadReal OP kLoadReal ==> Heap version
        if (inst1->fOpcode == FIRInstruction::kLoadReal && inst2->fOpcode == FIRInstruction::kLoadReal && FIRInstruction::isMath(inst3->fOpcode)) {
            //std::cout << "kLoadReal op kLoadReal ==> Heap version" << gFIRInstructionTable[inst2->fOpcode] << endl;
            end = cur + 3;
            return new FIRBasicInstruction<T>(gFIRMath2Heap[inst3->fOpcode], 0, 0, inst2->fOffset1, inst1->fOffset1);
            
        // kLoadInt OP kLoadInt ==> Heap version
        } else if (inst1->fOpcode == FIRInstruction::kLoadInt && inst2->fOpcode == FIRInstruction::kLoadInt && FIRInstruction::isMath(inst3->fOpcode)) {
            //std::cout << "kLoadRInt op kLoadInt ==> Heap version" << gFIRInstructionTable[inst2->fOpcode] << endl;
            end = cur + 3;
            return new FIRBasicInstruction<T>(gFIRMath2Heap[inst3->fOpcode], 0, 0, inst2->fOffset1, inst1->fOffset1);
            
            //===============
            // EXTENDED MATH
            //===============
            
        // kLoadReal EXTENDED-OP kLoadReal ==> Heap version
        } else  if (inst1->fOpcode == FIRInstruction::kLoadReal && inst2->fOpcode == FIRInstruction::kLoadReal && FIRInstruction::isExtendedBinaryMath(inst3->fOpcode)) {
            //std::cout << "kLoadReal op kLoadReal ==> Heap version" << gFIRInstructionTable[inst2->fOpcode] << endl;
            end = cur + 3;
            return new FIRBasicInstruction<T>(gFIRExtendedMath2Heap[inst3->fOpcode], 0, 0, inst2->fOffset1, inst1->fOffset1);
            
        // kLoadInt EXTENDED-OP kLoadInt ==> Heap version
        } else if (inst1->fOpcode == FIRInstruction::kLoadInt && inst2->fOpcode == FIRInstruction::kLoadInt && FIRInstruction::isExtendedBinaryMath(inst3->fOpcode)) {
            //std::cout << "kLoadRInt op kLoadInt ==> Heap version" << gFIRInstructionTable[inst2->fOpcode] << endl;
            end = cur + 3;
            return new FIRBasicInstruction<T>(gFIRExtendedMath2Heap[inst3->fOpcode], 0, 0, inst2->fOffset1, inst1->fOffset1);
        
        //=======
        // VALUE
        //=======
            
            //======
            // MATH
            //======
            
        // kLoadReal OP kRealValue ==> Value version
        } else if (inst1->fOpcode == FIRInstruction::kLoadReal && inst2->fOpcode == FIRInstruction::kRealValue && FIRInstruction::isMath(inst3->fOpcode)) {
            //std::cout << "kLoadReal op kRealValue ==> Value version" << gFIRInstructionTable[inst2->fOpcode] << endl;
            end = cur + 3;
            return new FIRBasicInstruction<T>(gFIRMath2Value[inst3->fOpcode], 0, inst2->fRealValue, inst1->fOffset1, 0);
          
        // kRealValue OP kLoadReal ==> Value version (special case for non-commutative operation)
        } else if (inst1->fOpcode == FIRInstruction::kRealValue && inst2->fOpcode == FIRInstruction::kLoadReal && FIRInstruction::isMath(inst3->fOpcode)) {
            //std::cout << "kRealValue op kLoadReal ==> Value version" << gFIRInstructionTable[inst2->fOpcode] << endl;
            end = cur + 3;
            return new FIRBasicInstruction<T>(gFIRMath2ValueInvert[inst3->fOpcode], 0, inst1->fRealValue, inst2->fOffset1, 0);
           
        // kLoadInt OP kIntValue ==> Value version
        } else if (inst1->fOpcode == FIRInstruction::kLoadInt && inst2->fOpcode == FIRInstruction::kIntValue && FIRInstruction::isMath(inst3->fOpcode)) {
            //std::cout << "kLoadInt op kIntValue ==> Value version" << gFIRInstructionTable[inst2->fOpcode] << endl;
            end = cur + 3;
            return new FIRBasicInstruction<T>(gFIRMath2Value[inst3->fOpcode], inst2->fIntValue, 0, inst1->fOffset1, 0);
            
        // kIntValue OP kLoadInt ==> Value version (special case for non-commutative operation)
        } else if (inst1->fOpcode == FIRInstruction::kIntValue && inst2->fOpcode == FIRInstruction::kLoadInt && FIRInstruction::isMath(inst3->fOpcode)) {
            //std::cout << "kIntValue op kLoadInt ==> Value version" << gFIRInstructionTable[inst2->fOpcode] << endl;
            end = cur + 3;
            return new FIRBasicInstruction<T>(gFIRMath2ValueInvert[inst3->fOpcode], inst1->fIntValue, 0, inst2->fOffset1, 0);
            
            //===============
            // EXTENDED MATH
            //===============
            
        // kLoadReal EXTENDED-OP kRealValue ==> Value version
        } else if (inst1->fOpcode == FIRInstruction::kLoadReal && inst2->fOpcode == FIRInstruction::kRealValue && FIRInstruction::isExtendedBinaryMath(inst3->fOpcode)) {
            //std::cout << "kLoadReal op kRealValue ==> Value version" << gFIRInstructionTable[inst2->fOpcode] << endl;
            end = cur + 3;
            return new FIRBasicInstruction<T>(gFIRExtendedMath2Value[inst3->fOpcode], 0, inst2->fRealValue, inst1->fOffset1, 0);
            
        // kRealValue EXTENDED-OP kLoadReal ==> Value version (special case for non-commutative operation)
        } else if (inst1->fOpcode == FIRInstruction::kRealValue && inst2->fOpcode == FIRInstruction::kLoadReal && FIRInstruction::isExtendedBinaryMath(inst3->fOpcode)) {
            //std::cout << "kRealValue op kLoadReal ==> Value version" << gFIRInstructionTable[inst2->fOpcode] << endl;
            end = cur + 3;
            return new FIRBasicInstruction<T>(gFIRExtendedMath2ValueInvert[inst3->fOpcode], 0, inst1->fRealValue, inst2->fOffset1, 0);
            
        // kLoadInt EXTENDED-OP kIntValue ==> Value version
        } else if (inst1->fOpcode == FIRInstruction::kLoadInt && inst2->fOpcode == FIRInstruction::kIntValue && FIRInstruction::isExtendedBinaryMath(inst3->fOpcode)) {
            //std::cout << "kLoadInt op kIntValue ==> Value version" << gFIRInstructionTable[inst2->fOpcode] << endl;
            end = cur + 3;
            return new FIRBasicInstruction<T>(gFIRExtendedMath2Value[inst3->fOpcode], inst2->fIntValue, 0, inst1->fOffset1, 0);
            
        // kIntValue EXTENDED-OP kLoadInt ==> Value version (special case for non-commutative operation)
        } else if (inst1->fOpcode == FIRInstruction::kIntValue && inst2->fOpcode == FIRInstruction::kLoadInt && FIRInstruction::isExtendedBinaryMath(inst3->fOpcode)) {
            //std::cout << "kIntValue op kLoadInt ==> Value version" << gFIRInstructionTable[inst2->fOpcode] << endl;
            end = cur + 3;
            return new FIRBasicInstruction<T>(gFIRExtendedMath2ValueInvert[inst3->fOpcode], inst1->fIntValue, 0, inst2->fOffset1, 0);
        
        //=======
        // STACK
        //=======
            
            //======
            // MATH
            //======
            
        // kLoadReal/kLoadInt binary OP ==> Stack version
        } else if (((inst1->fOpcode == FIRInstruction::kLoadReal) || (inst1->fOpcode == FIRInstruction::kLoadInt)) && FIRInstruction::isMath(inst2->fOpcode)) {
            //std::cout << "kLoadReal/kLoadInt binary op ==> Stack version " << gFIRInstructionTable[inst2->fOpcode] << endl;
            end = cur + 2;
            return new FIRBasicInstruction<T>(gFIRMath2Stack[inst2->fOpcode], 0, 0, inst1->fOffset1, 0);
        
        // kRealValue binary OP ==> Stack/Value version
        } else if ((inst1->fOpcode == FIRInstruction::kRealValue) && FIRInstruction::isMath(inst2->fOpcode)) {
            //std::cout << "kRealValue binary op ==> Stack/Value version " << gFIRInstructionTable[inst2->fOpcode] << endl;
            end = cur + 2;
            return new FIRBasicInstruction<T>(gFIRMath2StackValue[inst2->fOpcode], 0, inst1->fRealValue);
          
        // kIntValue binary OP ==> Stack/Value version
        } else if (((inst1->fOpcode == FIRInstruction::kRealValue) || (inst1->fOpcode == FIRInstruction::kIntValue)) && FIRInstruction::isMath(inst2->fOpcode)) {
            //std::cout << "kRealValue binary op ==> Stack/Value version " << gFIRInstructionTable[inst2->fOpcode] << endl;
            end = cur + 2;
            return new FIRBasicInstruction<T>(gFIRMath2StackValue[inst2->fOpcode], inst1->fIntValue, 0);
        
            //===============
            // EXTENDED MATH
            //===============
      
        // kLoadReal/kLoadInt binary EXTENDED-OP ==> Stack version
        } else if (((inst1->fOpcode == FIRInstruction::kLoadReal) || (inst1->fOpcode == FIRInstruction::kLoadInt)) && FIRInstruction::isExtendedBinaryMath(inst2->fOpcode)) {
            //std::cout << "kLoadReal/kLoadInt binary op ==> Stack version " << gFIRInstructionTable[inst2->fOpcode] << endl;
            end = cur + 2;
            return new FIRBasicInstruction<T>(gFIRExtendedMath2Stack[inst2->fOpcode], 0, 0, inst1->fOffset1, 0);
            
        // kRealValue EXTENDED-OP ==> Stack/Value version
        } else if ((inst1->fOpcode == FIRInstruction::kRealValue) && FIRInstruction::isExtendedBinaryMath(inst2->fOpcode)) {
            //std::cout << "kRealValue binary op ==> Stack/Value version " << gFIRInstructionTable[inst2->fOpcode] << endl;
            end = cur + 2;
            return new FIRBasicInstruction<T>(gFIRExtendedMath2StackValue[inst2->fOpcode], 0, inst1->fRealValue);
     
        // kIntValue binary EXTENDED-OP ==> Stack/Value version
        } else if ((inst1->fOpcode == FIRInstruction::kIntValue) && FIRInstruction::isExtendedBinaryMath(inst2->fOpcode)) {
            //std::cout << "kRealValue binary op ==> Stack/Value version " << gFIRInstructionTable[inst2->fOpcode] << endl;
            end = cur + 2;
            return new FIRBasicInstruction<T>(gFIRExtendedMath2StackValue[inst2->fOpcode], inst1->fIntValue, 0);
            
        //=====================
        // UNARY EXTENDED MATH
        //=====================
            
        // kLoadReal unary  ==> Heap version
        } else if (inst1->fOpcode == FIRInstruction::kLoadReal && FIRInstruction::isExtendedUnaryMath(inst2->fOpcode)) {
            //std::cout << "kLoadReal unary ==> Heap version " << gFIRInstructionTable[inst2->fOpcode] << endl;
            end = cur + 2;
            return new FIRBasicInstruction<T>(gFIRExtendedMath2Heap[inst2->fOpcode], 0, 0, inst1->fOffset1, 0);
            
        } else {
            end = cur + 1;
            return (*cur)->copy();
        }
    }
    
};


//=======================
// Partial evaluation
//=======================

//typedef std::map<int, T>    RealMap;
//typedef std::map<int, int>  IntMap;

//typedef typename std::map<int, T>    RealMap;
//typedef typename std::map<int, int>  IntMap;


// Constant Values Optimizer : propagate Int and Real constant values
template <class T>
struct FIRInstructionConstantValueMap2Heap : public FIRInstructionOptimizer<T>  {
    
    std::map<int, int>& fIntMap;
    std::map<int, T>& fRealMap;
    
    FIRInstructionConstantValueMap2Heap(std::map<int, int>& int_map, std::map<int, T>& real_map)
    :fIntMap(int_map), fRealMap(real_map)
    {
        //std::cout << "FIRInstructionConstantValueMap2Heap" << std::endl;
    }
    
    virtual FIRBasicInstruction<T>* rewrite(InstructionIT cur, InstructionIT& end)
    {
        FIRBasicInstruction<T>* inst = *cur;
        
        if (inst->fOpcode == FIRInstruction::kLoadInt && fIntMap.find(inst->fOffset1) != fIntMap.end()) {
            end = cur + 1;
            return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, fIntMap[inst->fOffset1] , 0);
        } else if (inst->fOpcode == FIRInstruction::kLoadReal && fRealMap.find(inst->fOffset1) != fRealMap.end()) {
            end = cur + 1;
            return new FIRBasicInstruction<T>(FIRInstruction::kRealValue, 0, fRealMap[inst->fOffset1]);
        } else {
            end = cur + 1;
            return (*cur)->copy();
        }
    }
};
    
// Constant Values Optimizer : propagate Int and Real constant values
template <class T>
struct FIRInstructionConstantValueHeap2Map : public FIRInstructionOptimizer<T>  {
    
    std::map<int, int>& fIntMap;
    std::map<int, T>& fRealMap;
    
    FIRInstructionConstantValueHeap2Map(std::map<int, int>& int_map, std::map<int, T>& real_map)
    :fIntMap(int_map), fRealMap(real_map)
    {
        //std::cout << "FIRInstructionConstantValueHeap2Map" << std::endl;
    }
    
    virtual FIRBasicInstruction<T>* rewrite(InstructionIT cur, InstructionIT& end)
    {
        FIRBasicInstruction<T>* inst1 = *cur;
        FIRBasicInstruction<T>* inst2 = *(cur + 1);
        
        if (inst1->fOpcode == FIRInstruction::kRealValue && inst2->fOpcode == FIRInstruction::kStoreReal) {
            end = cur + 2;
            // Add a new entry in real_map
            fRealMap[inst2->fOffset1] = inst1->fRealValue;
            return new FIRBasicInstruction<T>(FIRInstruction::kNop);
        } else if (inst1->fOpcode == FIRInstruction::kIntValue && inst2->fOpcode == FIRInstruction::kStoreInt) {
            end = cur + 2;
            // Add a new entry in int_map
            fIntMap[inst2->fOffset1] = inst1->fIntValue;
            return new FIRBasicInstruction<T>(FIRInstruction::kNop);
        } else {
            end = cur + 1;
            return (*cur)->copy();
        }
    }
};

// Math computation
template <class T>
struct FIRInstructionMathComputeOptimizer : public FIRInstructionOptimizer<T>  {
    
    FIRInstructionMathComputeOptimizer()
    {
        //std::cout << "FIRInstructionMathComputeOptimizer" << std::endl;
    }
    
    FIRBasicInstruction<T>* rewriteBinaryRealMath(FIRBasicInstruction<T>* inst1,
                                                  FIRBasicInstruction<T>* inst2,
                                                  FIRBasicInstruction<T>* inst3)
    {
        
        switch (inst3->fOpcode) {
                
            case FIRInstruction::kAddReal:
                return new FIRBasicInstruction<T>(FIRInstruction::kRealValue, 0, inst1->fRealValue + inst2->fRealValue);
                
            case FIRInstruction::kSubReal:
                return new FIRBasicInstruction<T>(FIRInstruction::kRealValue, 0, inst1->fRealValue - inst2->fRealValue);
                
            case FIRInstruction::kMultReal:
                return new FIRBasicInstruction<T>(FIRInstruction::kRealValue, 0, inst1->fRealValue * inst2->fRealValue);
                
            case FIRInstruction::kDivReal:
                return new FIRBasicInstruction<T>(FIRInstruction::kRealValue, 0, inst1->fRealValue / inst2->fRealValue);
                
            case FIRInstruction::kRemReal:
                //return new FIRBasicInstruction<T>(FIRInstruction::kRealValue, 0, inst1->fRealValue % inst2->fRealValue);
                assert(false);
                return new FIRBasicInstruction<T>(FIRInstruction::kNop);
                
            case FIRInstruction::kGTReal:
                return new FIRBasicInstruction<T>(FIRInstruction::kRealValue, 0, inst1->fRealValue > inst2->fRealValue);
                
            case FIRInstruction::kLTReal:
                return new FIRBasicInstruction<T>(FIRInstruction::kRealValue, 0, inst1->fRealValue < inst2->fRealValue);
                
            case FIRInstruction::kGEReal:
                return new FIRBasicInstruction<T>(FIRInstruction::kRealValue, 0, inst1->fRealValue >= inst2->fRealValue);
                
            case FIRInstruction::kLEReal:
                return new FIRBasicInstruction<T>(FIRInstruction::kRealValue, 0, inst1->fRealValue <= inst2->fRealValue);
                
            case FIRInstruction::kEQReal:
                return new FIRBasicInstruction<T>(FIRInstruction::kRealValue, 0, inst1->fRealValue == inst2->fRealValue);
                
            case FIRInstruction::kNEReal:
                return new FIRBasicInstruction<T>(FIRInstruction::kRealValue, 0, inst1->fRealValue != inst2->fRealValue);
                
            default:
                assert(false);
                break;
        }
        
    }
    
    FIRBasicInstruction<T>* rewriteBinaryIntMath(FIRBasicInstruction<T>* inst1,
                                                  FIRBasicInstruction<T>* inst2,
                                                  FIRBasicInstruction<T>* inst3)
    {
        switch (inst3->fOpcode) {
                
            case FIRInstruction::kAddInt:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, inst1->fIntValue + inst2->fIntValue, 0);
                
            case FIRInstruction::kSubInt:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, inst1->fIntValue - inst2->fIntValue, 0);
                
            case FIRInstruction::kMultInt:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, inst1->fIntValue * inst2->fIntValue, 0);
                
            case FIRInstruction::kDivInt:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, inst1->fIntValue / inst2->fIntValue, 0);
                
            case FIRInstruction::kRemInt:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, inst1->fIntValue % inst2->fIntValue, 0);
                
            case FIRInstruction::kLshInt:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, inst1->fIntValue << inst2->fIntValue, 0);
                
            case FIRInstruction::kRshInt:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, inst1->fIntValue >> inst2->fIntValue, 0);
                
            case FIRInstruction::kGTInt:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, inst1->fIntValue > inst2->fIntValue, 0);
                
            case FIRInstruction::kLTInt:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, inst1->fIntValue < inst2->fIntValue, 0);
                
            case FIRInstruction::kGEInt:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, inst1->fIntValue >= inst2->fIntValue, 0);
                
            case FIRInstruction::kLEInt:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, inst1->fIntValue <= inst2->fIntValue, 0);
                
            case FIRInstruction::kEQInt:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, inst1->fIntValue == inst2->fIntValue, 0);
                
            case FIRInstruction::kNEInt:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, inst1->fIntValue != inst2->fIntValue, 0);
                
            case FIRInstruction::kANDInt:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, inst1->fIntValue & inst2->fIntValue, 0);
                
            case FIRInstruction::kORInt:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, inst1->fIntValue | inst2->fIntValue, 0);
                
            case FIRInstruction::kXORInt:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, inst1->fIntValue ^ inst2->fIntValue, 0);
                
            default:
                assert(false);
                break;
        }
    }
    
    FIRBasicInstruction<T>* rewriteExtendedBinaryRealMath(FIRBasicInstruction<T>* inst1,
                                                          FIRBasicInstruction<T>* inst2,
                                                          FIRBasicInstruction<T>* inst3)
    {
        switch (inst3->fOpcode) {
                
            case FIRInstruction::kAtan2f:
                return new FIRBasicInstruction<T>(FIRInstruction::kRealValue, 0, std::atan2(inst1->fRealValue, inst2->fRealValue));
                
            case FIRInstruction::kFmodf:
                return new FIRBasicInstruction<T>(FIRInstruction::kRealValue, 0, std::fmod(inst1->fRealValue, inst2->fRealValue));
                
            case FIRInstruction::kPowf:
                return new FIRBasicInstruction<T>(FIRInstruction::kRealValue, 0, std::pow(inst1->fRealValue, inst2->fRealValue));
                
            case FIRInstruction::kMaxf:
                return new FIRBasicInstruction<T>(FIRInstruction::kRealValue, 0, std::max(inst1->fRealValue, inst2->fRealValue));
                
            case FIRInstruction::kMinf:
                return new FIRBasicInstruction<T>(FIRInstruction::kRealValue, 0, std::min(inst1->fRealValue, inst2->fRealValue));
        
            default:
                assert(false);
                break;
        }
    }
    
    FIRBasicInstruction<T>* rewriteExtendedBinaryIntMath(FIRBasicInstruction<T>* inst1,
                                                          FIRBasicInstruction<T>* inst2,
                                                          FIRBasicInstruction<T>* inst3)
    {
        switch (inst3->fOpcode) {
                
            case FIRInstruction::kMax:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, std::max(inst1->fIntValue, inst2->fIntValue), 0);
                
            case FIRInstruction::kMin:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, std::min(inst1->fIntValue, inst2->fIntValue), 0);
                
            default:
                assert(false);
                break;
        }
    }
    
    FIRBasicInstruction<T>* rewriteUnaryRealMath(FIRBasicInstruction<T>* inst1,
                                                FIRBasicInstruction<T>* inst2)
    {
        switch (inst2->fOpcode) {
                
                
            case FIRInstruction::kAbsf:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, 0, std::fabs(inst1->fIntValue));
                
            case FIRInstruction::kAcosf:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, 0, std::acos(inst1->fIntValue));
                
            case FIRInstruction::kAsinf:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, 0, std::asin(inst1->fIntValue));
                
            case FIRInstruction::kAtanf:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, 0, std::atan(inst1->fIntValue));
                
            case FIRInstruction::kCeilf:
               return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, 0, std::ceil(inst1->fIntValue));
                
            case FIRInstruction::kCosf:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, 0, std::cos(inst1->fIntValue));
                
            case FIRInstruction::kCoshf:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, 0, std::cosh(inst1->fIntValue));
                
            case FIRInstruction::kExpf:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, 0, std::exp(inst1->fIntValue));
                
            case FIRInstruction::kFloorf:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, 0, std::floor(inst1->fIntValue));
                
            case FIRInstruction::kLogf:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, 0, std::log(inst1->fIntValue));
                
            case FIRInstruction::kLog10f:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, 0, std::log10(inst1->fIntValue));
                
            case FIRInstruction::kRoundf:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, 0, std::round(inst1->fIntValue));
                
            case FIRInstruction::kSinf:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, 0, std::sin(inst1->fIntValue));
                
            case FIRInstruction::kSinhf:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, 0, std::sinh(inst1->fIntValue));
                
            case FIRInstruction::kSqrtf:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, 0, std::sqrt(inst1->fIntValue));
                
            case FIRInstruction::kTanf:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, 0, std::tan(inst1->fIntValue));
                
            case FIRInstruction::kTanhf:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, 0, std::tanh(inst1->fIntValue));
                
            default:
                assert(false);
                break;
        }
    }
    
    FIRBasicInstruction<T>* rewriteUnaryIntMath(FIRBasicInstruction<T>* inst1,
                                                 FIRBasicInstruction<T>* inst2)
    {
        switch (inst2->fOpcode) {
                
            case FIRInstruction::kAbs:
                return new FIRBasicInstruction<T>(FIRInstruction::kIntValue, std::abs(inst1->fIntValue), 0);
                
            default:
                assert(false);
                break;
        }

    }
    
    virtual FIRBasicInstruction<T>* rewrite(InstructionIT cur, InstructionIT& end)
    {
        FIRBasicInstruction<T>* inst1 = *cur;
        FIRBasicInstruction<T>* inst2 = *(cur + 1);
        FIRBasicInstruction<T>* inst3 = *(cur + 2);
        
        if (inst1->fOpcode == FIRInstruction::kRealValue
            && inst2->fOpcode == FIRInstruction::kRealValue
            && (FIRInstruction::isMath(inst3->fOpcode) || FIRInstruction::isMath(inst3->fOpcode))) {
            
            end = cur + 3;
            return rewriteBinaryRealMath(inst1, inst2, inst3);
            
        } else if (inst1->fOpcode == FIRInstruction::kIntValue
                   && inst2->fOpcode == FIRInstruction::kIntValue
                   && (FIRInstruction::isMath(inst3->fOpcode) || FIRInstruction::isMath(inst3->fOpcode))) {
            
            end = cur + 3;
            return rewriteBinaryIntMath(inst1, inst2, inst3);
            
        } else if (inst1->fOpcode == FIRInstruction::kRealValue
                   && inst2->fOpcode == FIRInstruction::kRealValue
                   && (FIRInstruction::isMath(inst3->fOpcode) || FIRInstruction::isExtendedBinaryMath(inst3->fOpcode))) {
            
            end = cur + 3;
            return rewriteExtendedBinaryRealMath(inst1, inst2, inst3);
            
        } else if (inst1->fOpcode == FIRInstruction::kIntValue
                   && inst2->fOpcode == FIRInstruction::kIntValue
                   && (FIRInstruction::isMath(inst3->fOpcode) || FIRInstruction::isExtendedBinaryMath(inst3->fOpcode))) {
            
            end = cur + 3;
            return rewriteExtendedBinaryIntMath(inst1, inst2, inst3);
            
        } else if (inst1->fOpcode == FIRInstruction::kRealValue
                   && FIRInstruction::isExtendedUnaryMath(inst2->fOpcode)) {
            
            end = cur + 2;
            return rewriteUnaryRealMath(inst1, inst2);
            
        } else if (inst1->fOpcode == FIRInstruction::kIntValue
                   && FIRInstruction::isExtendedUnaryMath(inst2->fOpcode)) {
            
            end = cur + 2;
            return rewriteUnaryIntMath(inst1, inst2);
            
        } else {
            end = cur + 1;
            return (*cur)->copy();
        }
    };
};


template <class T>
struct FIRInstructionOptimizer {
    
    // Rewrite a sequence of instructions starting from 'cur' to 'end' in a new single instruction.
    // Update 'end' so that caller can move at the correct next location
    virtual FIRBasicInstruction<T>* rewrite(InstructionIT cur, InstructionIT& end)
    {
        return 0;
    }
    
    // Return an optimized block by traversing it (including sub-blocks) with an 'optimizer'
    static FIRBlockInstruction<T>* optimize_aux(FIRBlockInstruction<T>* cur_block, FIRInstructionOptimizer<T>& optimizer)
    {
        assert(cur_block);
        
        // Block should have at least 2 instructions...
        if (cur_block->size() < 2) {
            return cur_block->copy();
        }
        
        FIRBlockInstruction<T>* new_block = new FIRBlockInstruction<T>();
        InstructionIT next, cur = cur_block->fInstructions.begin();
        
        do {
            FIRBasicInstruction<T>* inst = *cur;
            if (inst->fOpcode == FIRInstruction::kLoop) {
                new_block->push(new FIRBasicInstruction<T>(inst->fOpcode,
                                                           inst->fIntValue, inst->fRealValue,
                                                           inst->fOffset1, inst->fOffset2,
                                                           optimize_aux(inst->fBranch1, optimizer),
                                                           0));
                cur++;
            } else if (inst->fOpcode == FIRInstruction::kIf
                       || inst->fOpcode == FIRInstruction::kSelectReal
                       || inst->fOpcode == FIRInstruction::kSelectInt) {
                new_block->push(new FIRBasicInstruction<T>(inst->fOpcode,
                                                           inst->fIntValue, inst->fRealValue,
                                                           inst->fOffset1, inst->fOffset2,
                                                           optimize_aux(inst->fBranch1, optimizer),
                                                           optimize_aux(inst->fBranch2, optimizer)));
                cur++;
            } else if (inst->fOpcode == FIRInstruction::kCondBranch) {
                // Special case for loops : branch to new_block
                new_block->push(new FIRBasicInstruction<T>(FIRInstruction::kCondBranch, 0, 0, 0, 0, new_block, 0));
                cur++;
            } else {
                new_block->push(optimizer.rewrite(cur, next));
                cur = next;
            }
        } while (cur != cur_block->fInstructions.end());
        
        return new_block;
    }
    
    // Apply an optimizer on the block, return the optimized new block, then delete the original one
    static FIRBlockInstruction<T>* optimize(FIRBlockInstruction<T>* cur_block, FIRInstructionOptimizer<T>& optimizer)
    {
        FIRBlockInstruction<T>* new_block = optimize_aux(cur_block, optimizer);
        delete cur_block;
        return new_block;
    }
    
    // Specialize a block
    static FIRBlockInstruction<T>* specialize(FIRBlockInstruction<T>* cur_block, std::map<int, int>& int_map, std::map<int, T>& real_map)
    {
        /*
        std::map<int, int> fIntMap = int_map;
        std::map<int, T> fRealMap = real_map;
         */
        
        typename std::map<int, T>::iterator it;
        
        FIRInstructionConstantValueMap2Heap<T> map_2_heap(int_map, real_map);
        FIRInstructionMathComputeOptimizer<T> math_compute;
        FIRInstructionConstantValueHeap2Map<T> heap_2_map(int_map, real_map);
        
        int cur_block_size = cur_block->size();
        int new_block_size = cur_block->size();
        
        std::cout << "real_map" << std::endl;
        
        for (it = real_map.begin(); it != real_map.end(); it++) {
             std::cout << "real_map offset " << (*it).first << " value " << (*it).second << std::endl;
        }
        
        std::cout << "specialize size " << cur_block_size << std::endl;
        cur_block->write(&std::cout);
        
        do {

            std::cout << "LOOP specialize size " << cur_block->size() << std::endl;
            cur_block_size = new_block_size;
            
            std::cout << "FIRInstructionConstantValueMap2Heap" << std::endl;
            cur_block = optimize(cur_block, map_2_heap);
            cur_block->write(&std::cout);
            
            std::cout << "FIRInstructionMathComputeOptimizer" << std::endl;
            cur_block = optimize(cur_block, math_compute);
            cur_block->write(&std::cout);
            
            std::cout << "FIRInstructionConstantValueHeap2Map" << std::endl;
            cur_block = optimize(cur_block, heap_2_map);
            cur_block->write(&std::cout);
            
            new_block_size = cur_block->size();
            
            std::cout << "new_block_size " << new_block_size << " " << cur_block_size << std::endl;
            
        } while (new_block_size < cur_block_size);
        
        for (it = real_map.begin(); it != real_map.end(); it++) {
            std::cout << "real_map offset " << (*it).first << " value " << (*it).second << std::endl;
        }
        
        // Cleanup kNop instructions
        FIRBlockInstruction<T>* res_block = new FIRBlockInstruction<T>();
        InstructionIT it1;
        for (it1 = cur_block->fInstructions.begin(); it1 != cur_block->fInstructions.end(); it1++) {
            if ((*it1)->fOpcode != FIRInstruction::kNop) {   // Special case for loops
                res_block->push((*it1)->copy());
            }
        }
        
        return res_block;
    }
   
    static FIRBlockInstruction<T>* optimizeBlock(FIRBlockInstruction<T>* block)
    {
        // 1) optimize indexed 'heap' load/store in normal load/store
        FIRInstructionLoadStoreOptimizer<T> opt1;
        block = FIRInstructionOptimizer<T>::optimize(block, opt1);
        
        //cout << "block size = " << block->size() << endl;
        
        // 2) optimize simple 'heap' load/store in move
        FIRInstructionMoveOptimizer<T> opt2;
        block = FIRInstructionOptimizer<T>::optimize(block, opt2);
        
        //cout << "block size = " << block->size() << endl;
        
        // 3) optimize moves in block move
        FIRInstructionBlockMoveOptimizer<T> opt3;
        block = FIRInstructionOptimizer<T>::optimize(block, opt3);
        
        //cout << "block size = " << block->size() << endl;
        
        // 4) optimize 2 moves in pair move
        FIRInstructionPairMoveOptimizer<T> opt4;
        block = FIRInstructionOptimizer<T>::optimize(block, opt4);
        
        //cout << "block size = " << block->size() << endl;
        
        // 5) optimize 'cast' in heap cast
        FIRInstructionCastOptimizer<T> opt5;
        block = FIRInstructionOptimizer<T>::optimize(block, opt5);
        
        //cout << "block size = " << block->size() << endl;
        
        // 6) optimize 'heap' and 'value' math operations
        FIRInstructionMathOptimizer<T> opt6;
        block = FIRInstructionOptimizer<T>::optimize(block, opt6);
        
        //cout << "block size = " << block->size() << endl;
        
        return block;
    }
    
    static void initTables()
    {
        // Initializations for FIRInstructionMathOptimizer pass
        
        //===============
        // Math
        //===============
        
        // Init heap opcode
        for (int i = FIRInstruction::kAddReal; i <= FIRInstruction::kXORInt; i++) {
            gFIRMath2Heap[FIRInstruction::Opcode(i)]
            = FIRInstruction::Opcode(i + (FIRInstruction::kAddRealHeap - FIRInstruction::kAddReal));
            //std::cout << gFIRInstructionTable[i + (FIRInstruction::kAddRealHeap - FIRInstruction::kAddReal)] << std::endl;
        }
        
        // Init stack opcode
        for (int i = FIRInstruction::kAddReal; i <= FIRInstruction::kXORInt; i++) {
            gFIRMath2Stack[FIRInstruction::Opcode(i)]
            = FIRInstruction::Opcode(i + (FIRInstruction::kAddRealStack - FIRInstruction::kAddReal));
            //std::cout << gFIRInstructionTable[i + (FIRInstruction::kAddRealStack - FIRInstruction::kAddReal)] << std::endl;
        }
        
        // Init stack/value opcode
        for (int i = FIRInstruction::kAddReal; i <= FIRInstruction::kXORInt; i++) {
            gFIRMath2StackValue[FIRInstruction::Opcode(i)]
            = FIRInstruction::Opcode(i + (FIRInstruction::kAddRealStackValue - FIRInstruction::kAddReal));
            //std::cout << gFIRInstructionTable[i + (FIRInstruction::kAddRealStackValue - FIRInstruction::kAddReal)] << std::endl;
        }
        
        // Init Value opcode
        for (int i = FIRInstruction::kAddReal; i <= FIRInstruction::kXORInt; i++) {
            gFIRMath2Value[FIRInstruction::Opcode(i)]
            = FIRInstruction::Opcode(i + (FIRInstruction::kAddRealValue - FIRInstruction::kAddReal));
            //std::cout << gFIRInstructionTable[i + (FIRInstruction::kAddRealValue - FIRInstruction::kAddReal)] << std::endl;
        }
        
        // Init Value opcode (non commutative operation)
        for (int i = FIRInstruction::kAddReal; i <= FIRInstruction::kXORInt; i++) {
            gFIRMath2ValueInvert[FIRInstruction::Opcode(i)]
            = FIRInstruction::Opcode(i + (FIRInstruction::kAddRealValue - FIRInstruction::kAddReal));
            //std::cout << gFIRInstructionTable[i + (FIRInstruction::kAddRealValue - FIRInstruction::kAddReal)] << std::endl;
        }
        
        // Manually set inverted versions
        gFIRMath2ValueInvert[FIRInstruction::kSubReal] = FIRInstruction::kSubRealValueInvert;
        gFIRMath2ValueInvert[FIRInstruction::kSubInt] = FIRInstruction::kSubIntValueInvert;
        gFIRMath2ValueInvert[FIRInstruction::kDivReal] = FIRInstruction::kDivRealValueInvert;
        gFIRMath2ValueInvert[FIRInstruction::kDivInt] = FIRInstruction::kDivIntValueInvert;
        gFIRMath2ValueInvert[FIRInstruction::kRemReal] = FIRInstruction::kRemRealValueInvert;
        gFIRMath2ValueInvert[FIRInstruction::kRemInt] = FIRInstruction::kRemIntValueInvert;
        gFIRMath2ValueInvert[FIRInstruction::kLshInt] = FIRInstruction::kLshIntValueInvert;
        gFIRMath2ValueInvert[FIRInstruction::kRshInt] = FIRInstruction::kRshIntValueInvert;
        gFIRMath2ValueInvert[FIRInstruction::kGTInt] = FIRInstruction::kGTIntValueInvert;
        gFIRMath2ValueInvert[FIRInstruction::kLTInt] = FIRInstruction::kLTIntValueInvert;
        gFIRMath2ValueInvert[FIRInstruction::kGEInt] = FIRInstruction::kGEIntValueInvert;
        gFIRMath2ValueInvert[FIRInstruction::kLEInt] = FIRInstruction::kLEIntValueInvert;
        gFIRMath2ValueInvert[FIRInstruction::kGTReal] = FIRInstruction::kGTRealValueInvert;
        gFIRMath2ValueInvert[FIRInstruction::kLTReal] = FIRInstruction::kLTRealValueInvert;
        gFIRMath2ValueInvert[FIRInstruction::kGEReal] = FIRInstruction::kGERealValueInvert;
        gFIRMath2ValueInvert[FIRInstruction::kLEReal] = FIRInstruction::kLERealValueInvert;
        
        //===============
        // EXTENDED math
        //===============
        
        // Init unary extended math heap opcode
        for (int i = FIRInstruction::kAbs; i <= FIRInstruction::kTanhf; i++) {
            gFIRExtendedMath2Heap[FIRInstruction::Opcode(i)]
            = FIRInstruction::Opcode(i + (FIRInstruction::kAbsHeap - FIRInstruction::kAbs));
            //std::cout << gFIRInstructionTable[i + (FIRInstruction::kAddRealHeap - FIRInstruction::kAddReal)] << std::endl;
        }
        
        // Init binary extended math heap opcode
        for (int i = FIRInstruction::kAtan2f; i <= FIRInstruction::kMinf; i++) {
            gFIRExtendedMath2Heap[FIRInstruction::Opcode(i)]
            = FIRInstruction::Opcode(i + (FIRInstruction::kAtan2fHeap - FIRInstruction::kAtan2f));
            //std::cout << gFIRInstructionTable[i + (FIRInstruction::kAddRealHeap - FIRInstruction::kAddReal)] << std::endl;
        }
        
        
        // Init binary extended math stack opcode
        for (int i = FIRInstruction::kAtan2f; i <= FIRInstruction::kMinf; i++) {
            gFIRExtendedMath2Stack[FIRInstruction::Opcode(i)]
            = FIRInstruction::Opcode(i + (FIRInstruction::kAtan2fStack - FIRInstruction::kAtan2f));
            //std::cout << gFIRInstructionTable[i + (FIRInstruction::kAddRealHeap - FIRInstruction::kAddReal)] << std::endl;
        }
        
        // Init binary extended math stack/value opcode
        for (int i = FIRInstruction::kAtan2f; i <= FIRInstruction::kMinf; i++) {
            gFIRExtendedMath2StackValue[FIRInstruction::Opcode(i)]
            = FIRInstruction::Opcode(i + (FIRInstruction::kAtan2fStackValue - FIRInstruction::kAtan2f));
            //std::cout << gFIRInstructionTable[i + (FIRInstruction::kAddRealHeap - FIRInstruction::kAddReal)] << std::endl;
        }
        
        // Init unary math Value opcode
        for (int i = FIRInstruction::kAtan2f; i <= FIRInstruction::kMinf; i++) {
            gFIRExtendedMath2Value[FIRInstruction::Opcode(i)]
            = FIRInstruction::Opcode(i + (FIRInstruction::kAtan2fValue - FIRInstruction::kAtan2f));
            //std::cout << gFIRInstructionTable[i + (FIRInstruction::kAddRealHeap - FIRInstruction::kAddReal)] << std::endl;
        }
        
        // Init unary math Value opcode : non commutative operations
        for (int i = FIRInstruction::kAtan2f; i <= FIRInstruction::kPowf; i++) {
            gFIRExtendedMath2ValueInvert[FIRInstruction::Opcode(i)]
            = FIRInstruction::Opcode(i + (FIRInstruction::kAtan2fValueInvert - FIRInstruction::kAtan2f));
            //std::cout << gFIRInstructionTable[i + (FIRInstruction::kAddRealHeap - FIRInstruction::kAddReal)] << std::endl;
        }
        
        // Test
        
        /*
         std::cout << "gFIRExtendedMath2Heap" << std::endl;
         for (int i = FIRInstruction::kAbs; i <= FIRInstruction::kTanhf; i++) {
         if (FIRInstruction::gFIRExtendedMath2Heap.find(FIRInstruction::Opcode(i)) !=  FIRInstruction::gFIRExtendedMath2Heap.end()) {
         std::cout   << gFIRInstructionTable[FIRInstruction::Opcode(i)] << " ==> "
         << gFIRInstructionTable[FIRInstruction::gFIRExtendedMath2Heap[FIRInstruction::Opcode(i)]] << std::endl;
         }
         }
         std::cout << "gFIRExtendedMath2Heap" << std::endl << std::endl;
         for (int i = FIRInstruction::kAbsHeap; i <= FIRInstruction::kMinf; i++) {
         if (FIRInstruction::gFIRExtendedMath2Heap.find(FIRInstruction::Opcode(i)) !=  FIRInstruction::gFIRExtendedMath2Heap.end()) {
         std::cout   << gFIRInstructionTable[FIRInstruction::Opcode(i)] << " ==> "
         << gFIRInstructionTable[FIRInstruction::gFIRExtendedMath2Heap[FIRInstruction::Opcode(i)]] << std::endl;
         }
         }
         
         std::cout << "gFIRExtendedMath2Stack" << std::endl << std::endl;
         for (int i = FIRInstruction::kAbsHeap; i <= FIRInstruction::kMinf; i++) {
         if (FIRInstruction::gFIRExtendedMath2Stack.find(FIRInstruction::Opcode(i)) !=  FIRInstruction::gFIRExtendedMath2Stack.end()) {
         std::cout   << gFIRInstructionTable[FIRInstruction::Opcode(i)] << " ==> "
         << gFIRInstructionTable[FIRInstruction::gFIRExtendedMath2Stack[FIRInstruction::Opcode(i)]] << std::endl;
         }
         }
         
         std::cout << "gFIRExtendedMath2StackValue" << std::endl << std::endl;
         for (int i = FIRInstruction::kAbsHeap; i <= FIRInstruction::kMinf; i++) {
         if (FIRInstruction::gFIRExtendedMath2StackValue.find(FIRInstruction::Opcode(i)) !=  FIRInstruction::gFIRExtendedMath2StackValue.end()) {
         std::cout   << gFIRInstructionTable[FIRInstruction::Opcode(i)] << " ==> "
         << gFIRInstructionTable[FIRInstruction::gFIRExtendedMath2StackValue[FIRInstruction::Opcode(i)]] << std::endl;
         }
         }
         
         std::cout << "gFIRExtendedMath2Value" << std::endl << std::endl;
         for (int i = FIRInstruction::kAbsHeap; i <= FIRInstruction::kMinf; i++) {
         if (FIRInstruction::gFIRExtendedMath2Value.find(FIRInstruction::Opcode(i)) !=  FIRInstruction::gFIRExtendedMath2Value.end()) {
         std::cout   << gFIRInstructionTable[FIRInstruction::Opcode(i)] << " ==> "
         << gFIRInstructionTable[FIRInstruction::gFIRExtendedMath2Value[FIRInstruction::Opcode(i)]] << std::endl;
         }
         }
         
         std::cout << "gFIRExtendedMath2ValueInvert" << std::endl << std::endl;
         for (int i = FIRInstruction::kAbsHeap; i <= FIRInstruction::kMinf; i++) {
         if (FIRInstruction::gFIRExtendedMath2ValueInvert.find(FIRInstruction::Opcode(i)) !=  FIRInstruction::gFIRExtendedMath2ValueInvert.end()) {
         std::cout   << gFIRInstructionTable[FIRInstruction::Opcode(i)] << " ==> "
         << gFIRInstructionTable[FIRInstruction::gFIRExtendedMath2ValueInvert[FIRInstruction::Opcode(i)]] << std::endl;
         }
         }
         */
    }
};

    
/*

template <class T>
struct FIRInstructionSpecializer {
    
    
    // Rewrite a sequence of instructions starting from 'cur' to 'end' in a new single instruction.
    // Update 'end' so that caller can move at the correct next location
    virtual FIRBasicInstruction<T>* specialize(InstructionIT cur, InstructionIT& end)
    {
        return 0;
    }
    
    static FIRBlockInstruction<T>* specializeBlock(FIRBlockInstruction<T>* cur_block, IntMap& int_map, RealMap& real_map)
    {
        FIRBlockInstruction<T>* new_block = new FIRBlockInstruction<T>();
        
        InstructionIT cur = cur_block->fInstructions.begin();
        
        FIRBasicInstruction<T>* res = 0;
        
        FIRBasicInstruction<T>* inst1 = *cur;
        FIRBasicInstruction<T>* inst2 = *(cur + 1);
        FIRBasicInstruction<T>* inst3 = *(cur + 2);
        
        int consumed;
        
        do {
            
            
        }
        
        while ((res = specializeInstruction(inst1, inst2, inst3, int_map, real_map, consumed)) {
            inst1 = res;
            if (
        }
        
        
        if (res) {
            
        }
       
        if (inst->fOpcode == FIRInstruction::kLoop) {
            new_block->push(new FIRBasicInstruction<T>(inst->fOpcode,
                                                       inst->fIntValue, inst->fRealValue,
                                                       inst->fOffset1, inst->fOffset2,
                                                       specializeBlock(inst->fBranch1, int_map, real_map),
                                                       0));
            cur++;
        } else if (inst->fOpcode == FIRInstruction::kIf
                   || inst->fOpcode == FIRInstruction::kSelectReal
                   || inst->fOpcode == FIRInstruction::kSelectInt) {
            new_block->push(new FIRBasicInstruction<T>(inst->fOpcode,
                                                       inst->fIntValue, inst->fRealValue,
                                                       inst->fOffset1, inst->fOffset2,
                                                       specializeBlock(inst->fBranch1, int_map, real_map),
                                                       specializeBlock(inst->fBranch2, int_map, real_map)));
            cur++;
        } else if (inst->fOpcode == FIRInstruction::kCondBranch) {
            // Special case for loops : branch to new_block
            new_block->push(new FIRBasicInstruction<T>(FIRInstruction::kCondBranch, 0, 0, 0, 0, new_block, 0));
            cur++;
        } else {
            //new_block->push(optimizer.rewrite(cur, next));
            specializeInstruction(
            cur = next;
        }
        
    }
    
    static FIRBasicInstruction<T>* specializeInstruction(FIRBasicInstruction<T>* inst1
                                                         FIRBasicInstruction<T>* inst2
                                                         FIRBasicInstruction<T>* inst3,
                                                         IntMap& int_map,
                                                         RealMap& real_map
                                                         int& consumed)
    {
        
        FIRBasicInstruction<T>* res = 0;
        
        if (inst1->fOpcode == FIRInstruction::kLoadReal) {
            
            if (real_map.find(inst1->fOffset1) != real_map.end()) {
                res = FIRBasicInstruction<T>(FIRInstruction::kRealValue, 0, real_map[inst1->fOffset1]);
                consumed = 1;
            }
            
        } else if (inst1->fOpcode == FIRInstruction::kLoadInt) {
            
            if (int_map.find(inst1->fOffset1) != int_map.end()) {
                res = FIRBasicInstruction<T>(FIRInstruction::kIntValue, int_map[inst1->fOffset1], 0);
                consumed = 1;
            }
            
        } else if (inst1->fOpcode == FIRInstruction::kRealValue
                   inst2->fOpcode == FIRInstruction::kStoreReal) {
            
            // Add a new entry in real_map
            real_map[inst2->fOffset1] = inst1->fRealValue;
            consumed = 2;
            
        } else if (inst1->fOpcode == FIRInstruction::kIntValue
                   inst2->fOpcode == FIRInstruction::kStoreInt) {
            
            // Add a new entry in int_map
            int_map[inst2->fOffset1] = inst1->fIntValue;
            consumed = 2;
            
        // kLoadIndexed
        } else if (inst1->fOpcode == FIRInstruction::kIntValue
                   inst2->fOpcode == FIRInstruction::kLoadIndexedReal) {
            
            if (int_map.find(inst2->fOffset1) != int_map.end()) {
                res = FIRBasicInstruction<T>(FIRInstruction::kLoadReal, 0, 0, int_map[inst2->fOffset1] + inst1->fIntValue);
                consumed = 2;
            }
            
        } else if (inst1->fOpcode == FIRInstruction::kIntValue
                   inst2->fOpcode == FIRInstruction::kLoadIndexedInt) {
            
            if (int_map.find(inst2->fOffset1) != int_map.end()) {
                res = FIRBasicInstruction<T>(FIRInstruction::kLoadInt, 0, 0, int_map[inst2->fOffset1] + inst1->fIntValue);
                consumed = 2;
            }
            
        // kStoreIndexed
        } else if (inst1->fOpcode == FIRInstruction::kIntValue
                   inst2->fOpcode == FIRInstruction::kStoreIndexedReal) {
            
            if (int_map.find(inst2->fOffset1) != int_map.end()) {
                res = FIRBasicInstruction<T>(FIRInstruction::kStoreReal, 0, 0, int_map[inst2->fOffset1] + inst1->fIntValue);
                consumed = 2;
            }
            
        } else if (inst1->fOpcode == FIRInstruction::kIntValue
                   inst2->fOpcode == FIRInstruction::kStoreIndexedInt) {
            
            if (int_map.find(inst2->fOffset1) != int_map.end()) {
                res = FIRBasicInstruction<T>(FIRInstruction::kStoreInt, 0, 0, int_map[inst2->fOffset1] + inst1->fIntValue);
                consumed = 2;
            }
           
        // TODO : Input/output
            
        } else if (inst1->fOpcode == FIRInstruction::kRealValue
                   && inst2->fOpcode == FIRInstruction::kRealValue
                   && (isMath(inst3->fOpcode) || isExtendedBinaryMath(inst3->fOpcode))) {
            
            res = specializeBinaryMath(inst1, inst2, inst3, int_map, real_map);
            consumed = 3;
            
        } else if (inst1->fOpcode == FIRInstruction::kIntValue
                   && inst2->fOpcode == FIRInstruction::kIntValue
                   && (isMath(inst3->fOpcode) || isExtendedBinaryMath(inst3->fOpcode))) {
            
            res = specializeBinaryMath(inst1, inst2, inst3, int_map, real_map);
            consumed = 3;
            
        } else if (inst1->fOpcode == FIRInstruction::kRealValue
                   && isExtendedUnaryMath(inst2->fOpcode)) {
            
            res = specializeUnaryMath(inst1, inst2, int_map, real_map);
            consumed = 2;
            
        } else if (inst1->fOpcode == FIRInstruction::kIntValue
                   && isExtendedUnaryMath(inst2->fOpcode)) {
            
            res = specializeUnaryMath(inst1, inst2, int_map, real_map);
            consumed = 2;
            
        } else if (inst1->fOpcode == FIRInstruction::kIntValue && inst2->fOpcode == FIRInstruction::kCastReal) {
            
            res = FIRBasicInstruction<T>(FIRInstruction::kRealValue, 0, T(inst1->fIntValue));
            consumed = 1;
            
        } else if (inst1->fOpcode == FIRInstruction::kRealValue && inst2->fOpcode == FIRInstruction::kCastInt) {
            
            res = FIRBasicInstruction<T>(FIRInstruction::kIntValue, int(inst1->fIntValue), 0);
            consumed = 1;
         
        // SelectReal/SelectInt
        } else if (inst1->fOpcode == FIRInstruction::kIntValue
                   && ((inst2->fOpcode == FIRInstruction::kSelectReal)
                       ||(inst2->fOpcode == FIRInstruction::kSelectInt))) {
            
            if (inst1->fIntValue) {
                specializeBlock((*it)->fBranch1, int_map, real_map);
                consumed = 1;
            } else {
                specializeBlock((*it)->fBranch2, int_map, real_map);
                consumed = 1;
            }
         
        }
        
        
        return res;
    }

};
         */


#endif