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
#include <optional>

using namespace llvm;

namespace
{
    struct StrengthReductionPass : public FunctionPass
    {
        std::vector<Instruction *> InstructionsToRemove;

        static char ID; // Pass identification, replacement for typeid
        StrengthReductionPass() : FunctionPass(ID) {}

        bool runOnFunction(Function &F) override
        {
            bool modified = false;

            InstructionsToRemove.clear();

            for (BasicBlock &BB : F)
            {
                for (Instruction &Instr : BB)
                {
                    // TODO add support for signed operations. Currently, we return 
                    // as if there were no constants in the case of a negative constant.
                    if(isa<SDivOperator>(Instr) || isa<UDivOperator>(Instr)) {
                        reduceDiv(&Instr);
                    }

                    if (isa<MulOperator>(Instr)) {
                        reduceMult(&Instr);
                    }

                    if(Instr.getOpcode() == llvm::Instruction::BinaryOps::URem || Instr.getOpcode() == llvm::Instruction::BinaryOps::SRem) {
                        reduceModulo(&Instr);
                    }
                }
            }

            for (Instruction *Instr : InstructionsToRemove)
            {
                Instr->eraseFromParent();
                modified = true;
            }

            return modified;
        }

        void reduceMult(Instruction *Instr) 
        {
            IRBuilder<> builder(&*Instr);

            auto leftOp = TryGetConstantInt(Instr->getOperand(0));
            auto rightOp = TryGetConstantInt(Instr->getOperand(1));

            if(leftOp) {
                if (isPowerOfTwo(*leftOp)) {
                    int64_t powOfTwo = findPowerOfTwo(*leftOp);
                    Value *newInst = builder.CreateShl(Instr->getOperand(1), powOfTwo);
                    Instr->replaceAllUsesWith(newInst);
                    InstructionsToRemove.push_back(Instr);
                }
            } else if (rightOp) {
                if (isPowerOfTwo(*rightOp)) {
                    int64_t powOfTwo = findPowerOfTwo(*rightOp);
                    Value *newInst = builder.CreateShl(Instr->getOperand(0), powOfTwo);
                    Instr->replaceAllUsesWith(newInst);
                    InstructionsToRemove.push_back(Instr);
                }
            }
        }

        void reduceDiv(Instruction *Instr) 
        {
            IRBuilder<> builder(&*Instr);

            auto rightOp = TryGetConstantInt(Instr->getOperand(1));

            if (rightOp) {
                if (isPowerOfTwo(*rightOp)) {
                    int64_t powOfTwo = findPowerOfTwo(*rightOp);
                    Value *newInst = builder.CreateAShr(Instr->getOperand(0), powOfTwo);
                    Instr->replaceAllUsesWith(newInst);
                    InstructionsToRemove.push_back(Instr);
                }
            }
        }

        void reduceModulo(Instruction *Instr)
        {
            IRBuilder<> builder(&*Instr);

            auto rightOp = TryGetConstantInt(Instr->getOperand(1));

            if (rightOp) {
                if (isPowerOfTwo(*rightOp)) {
                    int64_t powOfTwo = findPowerOfTwo(*rightOp);
                    Value *mask = builder.getInt32((1 << powOfTwo) - 1);
                    Value *newInst = builder.CreateAnd(Instr->getOperand(0), mask);
                    Instr->replaceAllUsesWith(newInst);
                    InstructionsToRemove.push_back(Instr);
                }
            }
        }

        bool isPowerOfTwo(int64_t n)
        {
            return (n > 0) && ((n & (n - 1)) == 0);
        }

        int64_t findPowerOfTwo(int64_t value)
        {
            int64_t num = 0;
            while (value > 1)
            {
                value /= 2;
                num++;
            }

            return num;
        }

        std::optional<int64_t> TryGetConstantInt(Value *Operand)
        {
            if(isa<ConstantInt>(Operand)) {
                return dyn_cast<ConstantInt>(Operand)->getSExtValue();
            }

            return std::nullopt;
        }
    };
}

char StrengthReductionPass::ID = 0;
static RegisterPass<StrengthReductionPass> X("matf-sr", "MATF strength reduction");