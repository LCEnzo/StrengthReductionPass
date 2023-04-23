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
    // Hello - The first implementation, without getAnalysisUsage.
    struct StrengthReductionPass : public FunctionPass
    {
        std::vector<Instruction *> InstructionsToRemove;

        static char ID; // Pass identification, replacement for typeid
        StrengthReductionPass() : FunctionPass(ID) {}

        bool IsBinaryOp(Instruction *Instr)
        {
            return isa<BinaryOperator>(Instr);
        }

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

        void HandleBinaryOp(Instruction *Instr)
        {
            int ValueLeft, ValueRight;
            if (IsConstantInt(Instr->getOperand(0)))
            {
                ValueLeft = GetConstantInt(Instr->getOperand(0));
                if (isPowerOfTwo(ValueLeft))
                {
                    int powOfTwo = findPowerOfTwo(ValueLeft);

                    errs() << ValueLeft << " is " << powOfTwo << " pow of two.\n";

                    if (isa<MulOperator>(Instr))
                    {

                        IRBuilder<> builder(&*Instr);
                        Value *newInst = builder.CreateShl(Instr->getOperand(1), powOfTwo);
                        Instr->replaceAllUsesWith(newInst);
                        InstructionsToRemove.push_back(Instr);
                    }

                    if (isa<SDivOperator>(Instr))
                    {
                        IRBuilder<> builder(&*Instr);
                        Value *newInst = builder.CreateAShr(Instr->getOperand(1), powOfTwo);
                        Instr->replaceAllUsesWith(newInst);
                        InstructionsToRemove.push_back(Instr);
                    }
                }
            }

            if (IsConstantInt(Instr->getOperand(1)))
            {
                ValueRight = GetConstantInt(Instr->getOperand(1));
                if (isPowerOfTwo(ValueRight))
                {
                    int powOfTwo = findPowerOfTwo(ValueRight);

                    errs() << ValueRight << " is " << powOfTwo << " pow of two.\n";

                    if (isa<MulOperator>(Instr))
                    {
                        IRBuilder<> builder(&*Instr);
                        Value *newInst = builder.CreateShl(Instr->getOperand(0), powOfTwo);
                        Instr->replaceAllUsesWith(newInst);
                        InstructionsToRemove.push_back(Instr);
                    }

                    else if (isa<SDivOperator>(Instr))
                    {
                        IRBuilder<> builder(&*Instr);
                        Value *newInst = builder.CreateAShr(Instr->getOperand(0), powOfTwo);
                        Instr->replaceAllUsesWith(newInst);
                        InstructionsToRemove.push_back(Instr);
                    }
                }
            }
        }

        void IterateThroughFunction(Function &F)
        {
            for (BasicBlock &BB : F)
            {
                for (Instruction &Instr : BB)
                {
                    if (IsBinaryOp(&Instr))
                    {
                        HandleBinaryOp(&Instr);
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