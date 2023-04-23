#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"

using namespace llvm;

namespace
{
    struct StrengthReductionPass : public FunctionPass
    {
        std::vector<Instruction *> InstructionsToRemove;

        static char ID; // Pass identification, replacement for typeid
        StrengthReductionPass() : FunctionPass(ID) {}

        bool IsConstantInt(Value *Instr)
        {
            return isa<ConstantInt>(Instr);
        }

        int GetConstantInt(Value *Operand)
        {
            ConstantInt *Constant = dyn_cast<ConstantInt>(Operand);
            return Constant->getSExtValue();
        }

        bool isPowerOfTwo(int n)
        {
            return (n > 0) && ((n & (n - 1)) == 0);
        }

        int findPowerOfTwo(int value)
        {
            int num = 0;
            while (value > 1)
            {
                value /= 2;
                num++;
            }

            return num;
        }

        void reduceMult(Instruction *Instr) 
        {
            IRBuilder<> builder(&*Instr);
            bool isConstLeft = IsConstantInt(Instr->getOperand(0));
            bool isConstRight = IsConstantInt(Instr->getOperand(1));

            int OpValue, powOfTwo;

            if(isConstLeft) {
                OpValue = GetConstantInt(Instr->getOperand(0));
                
                if (isPowerOfTwo(OpValue)) {
                    powOfTwo = findPowerOfTwo(OpValue);
                    Value *newInst = builder.CreateShl(Instr->getOperand(1), powOfTwo);
                    Instr->replaceAllUsesWith(newInst);
                    InstructionsToRemove.push_back(Instr);
                }
            } else if (isConstRight) {
                OpValue = GetConstantInt(Instr->getOperand(1));

                if (isPowerOfTwo(OpValue)) {
                    powOfTwo = findPowerOfTwo(OpValue);
                    Value *newInst = builder.CreateShl(Instr->getOperand(0), powOfTwo);
                    Instr->replaceAllUsesWith(newInst);
                    InstructionsToRemove.push_back(Instr);
                }
            }
        }

        void reduceDiv(Instruction *Instr) 
        {
            IRBuilder<> builder(&*Instr);
            bool isConstRight = IsConstantInt(Instr->getOperand(1));

            int OpValue, powOfTwo;

            if (isConstRight) {
                OpValue = GetConstantInt(Instr->getOperand(1));

                if (isPowerOfTwo(OpValue)) {
                    powOfTwo = findPowerOfTwo(OpValue);
                    Value *newInst = builder.CreateAShr(Instr->getOperand(0), powOfTwo);
                    Instr->replaceAllUsesWith(newInst);
                    InstructionsToRemove.push_back(Instr);
                }
            }
        }

        void IterateThroughFunction(Function &F)
        {
            for (BasicBlock &BB : F)
            {
                for (Instruction &Instr : BB)
                {
                    if(isa<SDivOperator>(Instr)) {
                        reduceDiv(&Instr);
                    }

                    if (isa<MulOperator>(Instr)) {
                        reduceMult(&Instr);
                    }
                }
            }

            for (Instruction *Instr : InstructionsToRemove)
            {
                Instr->eraseFromParent();
            }
        }

        bool runOnFunction(Function &F) override
        {
            IterateThroughFunction(F);
            return true;
        }
    };
}

char StrengthReductionPass::ID = 0;
static RegisterPass<StrengthReductionPass> X("matf-sr", "MATF strength reduction");