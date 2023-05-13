#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <map>

using namespace llvm;

namespace
{
    struct StrengthReductionPass : public FunctionPass
    {
        std::vector<Instruction *> InstructionsToRemove;

        static char ID; // Pass identification, replacement for typeid
        StrengthReductionPass() : FunctionPass(ID) {}

        // required
        void getAnalysisUsage(AnalysisUsage &AU) const override {
            AU.setPreservesCFG();
            AU.addRequired<LoopInfoWrapperPass>();
            AU.addRequired<TargetLibraryInfoWrapperPass>();
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


        bool runOnFunction(Function &F) override
        {
            LoopInfo &loopInfo = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

            // each induction variable consists of 3 things:
            //   1. basic induction variable;
            //   2. multiplicative factor;
            //   3. additive factor;
            // example:
            //          loop counter i:  i => (i, 1, 0)
            //          j = 3*i + 4:     j => (i, 3, 4)
            //          k = 2*j + 1:     k => (i, 2*3, 4+1)
            std::map<Value*, std::tuple<Value*, int, int>> indVarMap;

            for (auto *loop : loopInfo) {

                BasicBlock *header = loop->getHeader();

                for (auto &Instr : *header) {
                    if (isa<PHINode>(&Instr)) { // loop counter
                        errs() << "\nPhi instruction found!\n";
                        errs() << Instr << "\n\n";

                        indVarMap[&Instr] = std::make_tuple(&Instr, 1, 0);
                    }
                }

                auto loopBlocks = loop->getBlocks();
                for (auto BB : loopBlocks) {
                    for (auto &Instr : *BB) {

                        if (isa<BinaryOperator>(Instr)) {
                            Value *left = Instr.getOperand(0);
                            Value *right = Instr.getOperand(1);

                            // check if some induction variable is already in map
                            if (indVarMap.count(left) > 0 || indVarMap.count(right) > 0) {

                                if (isa<MulOperator>(Instr)) {
                                    errs() << "multiplication:\t " << Instr << "\n";

                                    // j = mul %i, const
                                    if (indVarMap.count(left) > 0 && IsConstantInt(right)) {
                                        // map.insert(i, oldFactor * value(right), oldFactor);
                                    }
                                    // j = mul const, %i
                                    else if (indVarMap.count(right) > 0 && IsConstantInt(left)) {
                                        // map.insert(i, oldFactor * value(left), oldFactor);
                                    }


                                } else if (isa<AddOperator>(Instr)) {
                                    errs() << "addition:\t " << Instr << "\n";

                                    // j = add %i, const
                                    if (indVarMap.count(left) > 0 && IsConstantInt(right)) {
                                        // map.insert(i, oldFactor, oldFactor + value(right));
                                        errs() << "left var: " << left << "\n";
                                    }
                                    // j = add const, %i
                                    else if (indVarMap.count(right) > 0 && IsConstantInt(left)) {
                                        // map.insert(i, oldFactor, oldFactor + value(left));
                                        errs() << "right var: " << left << "\n";

                                    }
                                }


                            }
                        }
                    }
                }

            }

            return true;
        }
    };
}

char StrengthReductionPass::ID = 0;
static RegisterPass<StrengthReductionPass> X("matf-sr", "MATF strength reduction");