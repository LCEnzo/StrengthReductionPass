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
using namespace std;

namespace
{
    struct IndVarsStrengthReductionPass : public FunctionPass {
        static char ID; // Pass identification, replacement for typeid
        IndVarsStrengthReductionPass() : FunctionPass(ID) {}

        Value* preHeaderValue;
        std::vector<Instruction *> InstructionsToRemove;
        map<Value*, InductionVarInfo> inductionMap;

        // required
        void getAnalysisUsage(AnalysisUsage &AU) const override {
            AU.setPreservesCFG();
            AU.addRequired<LoopInfoWrapperPass>();
            AU.addRequired<TargetLibraryInfoWrapperPass>();
        }

        bool IsConstantInt(Value *Instr) {
            return isa<ConstantInt>(Instr);
        }

        int GetConstantInt(Value *Operand) {
            ConstantInt *Constant = dyn_cast<ConstantInt>(Operand);
            return Constant->getSExtValue();
        }

        void PrintBasicBlock(BasicBlock* BB, string msg) {
            errs() << "\n---------" << msg << "----------\n";
            for (auto &Instr : *BB) {
                errs() << Instr << "\n";
            }
            errs() << "-----------------------------------\n";
        }

        struct InductionVarInfo {
            Value *parent = nullptr;
            int additiveStep;
            int multiplicativeStep;
            bool isPhi;
            Value *preheaderValue;
        };

        void PrintInductionTable() {
            errs() << "--------------IV_SR--------------\n";
            for (auto [val, info] : inductionMap) {
                errs() << "Instruction:" << *val << "\n";
                errs() << "Parent:     " << *info.parent << "\n";
                if (info.isPhi) {
                    errs() << "phi: (" << *info.preheaderValue << "; ";
                } else {
                    errs() << "IndVar: (";
                }
                errs() << "*" << info.multiplicativeStep << ", ";
                errs() << "+" << info.additiveStep << ")\n";
            }
            errs() << "---------------------------------\n";
        }

        void PrintPhiMap(map<Value*, PHINode*> &PhiMap) {
            errs() << "---PHI MAP---\n";
            for (auto &p : PhiMap) {
                errs() << "Value: ";
                p.first->print(errs());
                errs() << "\n";
                errs() << "Phi: ";
                p.second->print(errs());
                errs() << "\n";
            }
            errs() << "-------------\n";
        }

        Value *CalculateNewIncomingValue(Instruction &position, InductionVarInfo indVarInfo) {

            IRBuilder<> instructionBuilder(&position);

            // It's important if counter starts from zero or something else.
            // That's why we're multiplying initial value and multiplicative step.
            Value *newIncomingValue = instructionBuilder.CreateMul(preHeaderValue,ConstantInt::getSigned(preHeaderValue->getType(), indVarInfo.multiplicativeStep));
            newIncomingValue = instructionBuilder.CreateAdd(newIncomingValue, ConstantInt::getSigned(preHeaderValue->getType(), indVarInfo.additiveStep));

            return newIncomingValue;
        }

        bool runOnFunction(Function &F) override {
            LoopInfo &loopInfo = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

            for (auto *loop: loopInfo) {

                BasicBlock *headerBasicBlock = loop->getHeader();
                BasicBlock *preHeaderBasicBlock = loop->getLoopPreheader();
                BasicBlock* incrementBasicBlock;

                for (auto &Instr: *headerBasicBlock) {
                    if (auto *phiNode = dyn_cast<PHINode>(&Instr)) {

                        InductionVarInfo indInfo;
                        indInfo.isPhi = true;
                        indInfo.multiplicativeStep = 1;
                        indInfo.additiveStep = 0;
                        indInfo.parent = &Instr;

                        int n = phiNode->getNumIncomingValues();
                        for (int i = 0; i < n; i++) {
                            auto incomingBlock = phiNode->getIncomingBlock(i);
                            if (incomingBlock == preHeaderBasicBlock) {
                                indInfo.preheaderValue = phiNode->getIncomingValue(i);

                                // Important if loop counter doesn't start from 1, ex: for (i = 3; i < 10; i++)
                                preHeaderValue = phiNode->getIncomingValue(i);
                            }
                            else {
                                // %for.inc block, we need it for later
                                incrementBasicBlock = incomingBlock;
                            }
                        }

                        inductionMap[&Instr] = indInfo;
                    }
                }

                PrintInductionTable();
                PrintBasicBlock(incrementBasicBlock, "--Increment BB--");

                auto loopBlocks = loop->getBlocks();
                for (auto BB: loopBlocks) {
                    for (auto &Instr: *BB) {
                        if (isa<BinaryOperator>(Instr)) {
                            Value *left = Instr.getOperand(0);
                            Value *right = Instr.getOperand(1);

                            /*
                            errs() << "Levi: " << inductionMap.count(left) << "\n";
                            errs() << "\t" << *left << "\n";
                            errs() << "Desni: " << inductionMap.count(right) << "\n";
                            errs() << "\t" << *right << "\n";
                             */

                            // check if some induction variable is already in map
                            if (inductionMap.count(left)|| inductionMap.count(right)) {

                                Value* constant = nullptr;
                                Value* variable = nullptr;

                                // Which one is constant?
                                // We always assume that there will be one!
                                if (IsConstantInt(left)) {
                                    variable = right;
                                    constant = left;
                                } else if (IsConstantInt(right)) {
                                    variable = left;
                                    constant = right;
                                }

                                InductionVarInfo indVarOld = inductionMap[variable];
                                int newFactor = GetConstantInt(constant);

                                InductionVarInfo newIndVarInfo;
                                newIndVarInfo.parent = indVarOld.parent;
                                newIndVarInfo.isPhi = false;

                                // We want to avoid division, subtraction with basic induction variable,
                                // because our algorithm only works with multiplication and addition!
                                if (isa<MulOperator>(Instr)) {
                                    newIndVarInfo.multiplicativeStep = newFactor * indVarOld.multiplicativeStep;
                                    newIndVarInfo.additiveStep = indVarOld.additiveStep;

                                    inductionMap[&Instr] = newIndVarInfo;
                                } else if (isa<AddOperator>(Instr)) {
                                    newIndVarInfo.additiveStep = newFactor + indVarOld.additiveStep;
                                    newIndVarInfo.multiplicativeStep = indVarOld.multiplicativeStep;

                                    inductionMap[&Instr] = newIndVarInfo;
                                }
                            }
                        } // if binaryOP
                    } // end BB
                } // end loopBlocks


                PrintInductionTable();


                // Map for new phi nodes
                // Instruction -> Phi Node
                map<Value*, PHINode*> PhiMap;

                /*
                 * We're making new phi instruction for every induction variable
                 * that is not basic or unfinished.
                 *
                 * Our phi instruction will have two incoming values. One from %entry block
                 * and one from %for.inc, in this loop we're making just the first part
                 * (from %entry block).
                 *
                 * New phi instr must be placed before already existing (phi instr for counter).
                 * That's why we're passing instruction `I` to CalculateNewIncomingValue func.
                 */
                for (auto &I: *headerBasicBlock) { // idemo kroz instrukcije header-a
                    if (isa<PHINode>(&I)) {
                        IRBuilder<> phiBuilder(&I);
                        for (auto &indvar: inductionMap) {
                            // avoid counter and unfinished (i, a, 0)
                            if ((indvar.second.multiplicativeStep != 1 && indvar.second.additiveStep != 0)) {

                                Value *newIncomingValue = CalculateNewIncomingValue(I, indvar.second);
                                errs() << "New incoming value: " << *newIncomingValue << "\n";

                                PHINode *newPhiNode = phiBuilder.CreatePHI(preHeaderValue->getType(), 2);

                                // Incoming block in %entry
                                // After this phi instr should look something like:  phi i64 [ <num>, %entry ]
                                newPhiNode->addIncoming(newIncomingValue, preHeaderBasicBlock);
                                PhiMap[indvar.first] = newPhiNode;
                            }
                        }
                    }
                }

                PrintPhiMap(PhiMap);


                /*
                 * In this loop, we're finishing phi instructions by adding second part
                 * to them, avoiding unfinished, (i, a, 0), indVar.
                 */
                for (auto &indvar: inductionMap) {
                    if (PhiMap.count(indvar.first) &&
                        (indvar.second.multiplicativeStep != 1 && indvar.second.additiveStep != 0)) {
//                        errs() << "IndVar parent: " << *indvar.second.parent << "\n";
                        // Prolazimo kroz instrukcije %for.inc basic block-a
                        for (auto &I: *incrementBasicBlock) {
                            if (auto op = dyn_cast<BinaryOperator>(&I)) {
                                Value *lhs = op->getOperand(0);
                                Value *rhs = op->getOperand(1);
                                /*
                                errs() << "Instrukcija: ";
                                I.print(errs());
                                errs() << "\nLeva strana: ";
                                lhs->print(errs());
                                errs() << "\nDesna strana: ";
                                rhs->print(errs());
                                errs() << "\n\n";
                                 */

                                // We are checking if one of the operands is a counter,
                                // so we can recognize the increment instruction
                                if (lhs == indvar.second.parent || rhs == indvar.second.parent) {

                                    // for basic counter increment instruction
                                    InductionVarInfo incIndVar = inductionMap[&I];
                                    IRBuilder<> instructionBuilder(&I);

                                    // We are calculating new increment by multiplying counter
                                    // increment and ind var multiplicative factor
                                    // example:
                                    //     i -> (i, 1, 2);
                                    //     j -> (i, 2, 981);
                                    //     newIncrement = 2 * 2 = 4
                                    int newIncrement = incIndVar.additiveStep * indvar.second.multiplicativeStep;

                                    // Left hand side is phi instruction!
                                    PHINode *phiVal = PhiMap[indvar.first];
                                    Value *newIncrementInstruction = instructionBuilder.CreateAdd(phiVal,ConstantInt::getSigned(
                                                                                         phiVal->getType(), newIncrement));

                                    // Adding second part of phi instruction:
                                    //    phi i64 [ <num>, %entry ], [ %newIncrementInstruction, %for.inc ]
                                    phiVal->addIncoming(newIncrementInstruction, incrementBasicBlock);

                                    errs() << "Final phi instruction:   ";
                                    phiVal->print(errs());
                                    errs() << "New increment instruction:" << *newIncrementInstruction << "\n";
                                    errs() << "\n\n";
                                }
                            }
                        }
                    }
                }

                PrintPhiMap(PhiMap);

                // replace all the original uses with phi-node
                for (auto &phi_val: PhiMap) {
                    (phi_val.first)->replaceAllUsesWith(phi_val.second);
                }


            } // finish all loops

            return true;
        }
    };
}

char IndVarsStrengthReductionPass::ID = 0;
static RegisterPass<IndVarsStrengthReductionPass> X("matf-iv-sr", "MATF strength reduction of induction variables");