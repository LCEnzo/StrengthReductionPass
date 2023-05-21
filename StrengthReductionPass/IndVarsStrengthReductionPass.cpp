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
#include <unordered_map>

using namespace llvm;

namespace
{
    struct IndVarsStrengthReductionPass : public FunctionPass
    {
        std::vector<Instruction *> InstructionsToRemove;

        static char ID; // Pass identification, replacement for typeid
        IndVarsStrengthReductionPass() : FunctionPass(ID) {}

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
            std::unordered_map<Value*, std::tuple<Value*, int, int>> indVarMap;

            for (auto *loop : loopInfo) {

                BasicBlock *header = loop->getHeader();

                for (auto &Instr: *header) {
                    if (isa<PHINode>(&Instr)) { // loop counter
                        errs() << "\nPhi instruction found!\n";
                        errs() << Instr << "\n\n";

                        indVarMap[&Instr] = std::make_tuple(&Instr, 1, 0);
                    }
                }

                auto loopBlocks = loop->getBlocks();

                // Is this really necessary because if some var is combination of other induction var
                // it only can be below, so with one iteration through loop we can store it in a map
                bool changed = true;
                while (changed) {

                    auto oldSize = indVarMap.size();
                    for (auto BB: loopBlocks) {
                        for (auto &Instr: *BB) {

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
                                            std::tuple < Value * , int, int > old = indVarMap[left];
                                            int newFactor = GetConstantInt(right) * std::get<1>(old);
                                            indVarMap[&Instr] = std::make_tuple(
                                                    std::get<0>(old), // Value*
                                                    newFactor,           // new multiplicative factor
                                                    std::get<2>(old)  // old additive factor
                                            );
                                        }
                                            // j = mul const, %i
                                        else if (indVarMap.count(right) > 0 && IsConstantInt(left)) {
                                            // map.insert(i, oldFactor * value(left), oldFactor);
                                            std::tuple < Value * , int, int > old = indVarMap[right];
                                            int newFactor = GetConstantInt(left) * std::get<1>(old);
                                            indVarMap[&Instr] = std::make_tuple(
                                                    std::get<0>(old), // Value*
                                                    newFactor,        // new multiplicative factor
                                                    std::get<2>(old)  // old additive factor
                                            );
                                        }


                                    } else if (isa<AddOperator>(Instr)) {
                                        errs() << "addition:\t " << Instr << "\n";

                                        // j = add %i, const
                                        if (indVarMap.count(left) > 0 && IsConstantInt(right)) {
                                            // map.insert(i, oldFactor, oldFactor + value(right));
                                            std::tuple < Value * , int, int > old = indVarMap[left];
                                            int newFactor = GetConstantInt(right) + std::get<2>(old);
                                            indVarMap[&Instr] = std::make_tuple(
                                                    std::get<0>(old), // Value*
                                                    std::get<1>(old), // old multiplicative factor
                                                    newFactor         // new additive factor
                                            );
                                        }
                                            // j = add const, %i
                                        else if (indVarMap.count(right) > 0 && IsConstantInt(left)) {
                                            // map.insert(i, oldFactor, oldFactor + value(left));
                                            std::tuple < Value * , int, int > old = indVarMap[right];
                                            int newFactor = GetConstantInt(left) + std::get<2>(old);
                                            indVarMap[&Instr] = std::make_tuple(
                                                    std::get<0>(old), // Value*
                                                    std::get<1>(old), // old multiplicative factor
                                                    newFactor         // new additive factor
                                            );

                                        }
                                    }


                                }
                            } // if binaryOP
                        } // end BB
                    } // end loopBlocks

                    if (oldSize == indVarMap.size())
                        changed = false;
                }
            }

            errs() << "\n--------------IV_SR--------------\n";
            for (auto &p : indVarMap) {
                errs() << p.first << " =>  ("
                       << std::get<0>(p.second) << ", "
                       << std::get<1>(p.second) << ", "
                       << std::get<2>(p.second) << ")\n";
            }

            return true;
        }
    };
}

char IndVarsStrengthReductionPass::ID = 0;
static RegisterPass<IndVarsStrengthReductionPass> X("matf-iv-sr", "MATF strength reduction of induction variables");