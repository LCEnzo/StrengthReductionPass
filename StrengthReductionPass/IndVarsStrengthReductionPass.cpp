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

        map<Value*, InductionVarInfo> inductionMap;

        void PrintInductionTable() {
            errs() << "--------------IV_SR--------------\n";
            for (auto [val, info] : inductionMap) {
                errs() << "Instruction:" << *val << "\n";
                errs() << "Parent:      " << *info.parent << "\n";
                if (info.isPhi) {
                    errs() << "phi: (" << *info.preheaderValue << "; ";
                } else {
                    errs() << "IndVar: (";
                }
                errs() << "*" << info.multiplicativeStep << ", ";
                errs() << "+" << info.additiveStep << ")\n\n";
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
            Value *newIncomingValue = instructionBuilder.CreateMul(preHeaderValue,ConstantInt::getSigned(preHeaderValue->getType(), indVarInfo.multiplicativeStep));
            newIncomingValue = instructionBuilder.CreateAdd(newIncomingValue, ConstantInt::getSigned(preHeaderValue->getType(), indVarInfo.additiveStep));

            return newIncomingValue;
        }

        bool runOnFunction(Function &F) override {
            LoopInfo &loopInfo = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

            for (auto *loop: loopInfo) {

                BasicBlock *header = loop->getHeader();
                BasicBlock *preheaderBasicBlock = loop->getLoopPreheader();

                BasicBlock* incrementBasicBlock;
                // Important if loop counter doesn't start from 1, ex: for (i = 3; i < 10; i++)
                for (auto &Instr: *header) {
                    if (auto *phiNode = dyn_cast<PHINode>(&Instr)) {

                        InductionVarInfo indInfo;
                        indInfo.isPhi = true;
                        indInfo.multiplicativeStep = 1;
                        indInfo.additiveStep = 0;
                        indInfo.parent = &Instr;


                        int n = phiNode->getNumIncomingValues();
                        for (int i = 0; i < n; i++) {
                            auto incomingBlock = phiNode->getIncomingBlock(i);
                            if (incomingBlock == preheaderBasicBlock) {
//                                auto *preheaderValue = phiNode->getIncomingValue(i);
//                                indInfo.preheaderValue = GetConstantInt(preheaderValue);
                                indInfo.preheaderValue = phiNode->getIncomingValue(i);
                                preHeaderValue = phiNode->getIncomingValue(i);
                            }
                            else {
                                // Odmah cuvamo blok gde treba da stavimo nasu instrukciju
                                // mnozenja (%for.inc blok)
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
//                            Value *operands[2];
//                            operands[0] = Instr.getOperand(0); // left
//                            operands[1] = Instr.getOperand(1); // right

//                            errs() << "Levi: " << inductionMap.count(left) << "\n";
//                            errs() << "\t" << *left << "\n";
//                            errs() << "Desni: " << inductionMap.count(right) << "\n";
//                            errs() << "\t" << *right << "\n";

                            // check if some induction variable is already in map
                            if (inductionMap.count(left)|| inductionMap.count(right)) {
                                if (isa<MulOperator>(Instr)) {
//                                    errs() << "multiplication:\t " << Instr << "\n";
                                    int index = IsConstantInt(left) ? 0 : 1; // koji je index const

                                    /*
                                    int constValue;
                                    if (index) {
                                        constValue = GetConstantInt(left);
                                    } else {
                                        constValue = GetConstantInt(right);
                                    }
                                     Da se ovde izracunaj faktori ili tako nes,
                                     pa posle da se samo provera da li je mnozenje ili deljenje, pa da se
                                     samo doda

                                     Za jednu instrukciju sve gledamo:
                                     - da li je binarana
                                     - da li postoji vec u manpi neka prom
                                     - da li je jedna od njih konstanta, ako jeste pretvori
                                     - ako je mnoznje i ako je u mapi popuni
                                     - isto za sabiranje
                                     - ne treba ovoliko provera i ponavaljanja
                                     */


                                    // j = mul %i, const
                                    if (inductionMap.count(left) && IsConstantInt(right)) {
                                        InductionVarInfo oldInfo = inductionMap[left];

                                        int newFactor = GetConstantInt(right) *
                                                        oldInfo.multiplicativeStep;


                                        InductionVarInfo info;
                                        info.parent = oldInfo.parent;
                                        info.isPhi = false;
                                        info.multiplicativeStep = newFactor;
                                        info.additiveStep = oldInfo.additiveStep;

                                        inductionMap[&Instr] = info;
                                    }
                                        // j = mul const, %i
                                    else if (inductionMap.count(right) && IsConstantInt(left)) {
                                        InductionVarInfo oldInfo = inductionMap[right];

                                        int newFactor = GetConstantInt(left) *
                                                oldInfo.multiplicativeStep;

                                        InductionVarInfo info;
                                        info.parent = oldInfo.parent;
                                        info.isPhi = false;
                                        info.multiplicativeStep = newFactor;
                                        info.additiveStep = oldInfo.additiveStep;

                                        inductionMap[&Instr] = info;
                                    }


                                }
                                else if (isa<AddOperator>(Instr)) {
//                                    errs() << "addition:\t " << Instr << "\n";

                                    // j = add %i, const
                                    if (inductionMap.count(left) && IsConstantInt(right)) {
                                        InductionVarInfo oldInfo = inductionMap[left];
                                        int newFactor = GetConstantInt(right) +
                                                oldInfo.additiveStep;

                                        InductionVarInfo info;
                                        info.parent = oldInfo.parent;
                                        info.isPhi = false;
                                        info.additiveStep = newFactor;
                                        info.multiplicativeStep = oldInfo.multiplicativeStep;

                                        inductionMap[&Instr] = info;
                                    }
                                        // j = add const, %i
                                    else if (inductionMap.count(right) && IsConstantInt(left)) {
                                        InductionVarInfo oldInfo = inductionMap[right];
                                        int newFactor = GetConstantInt(left) +
                                                oldInfo.additiveStep;

                                        InductionVarInfo info;
                                        info.parent = oldInfo.parent;
                                        info.isPhi = false;
                                        info.additiveStep = newFactor;
                                        info.multiplicativeStep = oldInfo.multiplicativeStep;

                                        inductionMap[&Instr] = info;
                                    }
                                }


                            }
                        } // if binaryOP
                    } // end BB
                } // end loopBlocks


                PrintInductionTable();


                // mapa koja slika instr u phi cvor
                map<Value*, PHINode*> PhiMap;

                for (auto &I: *header) { // idemo kroz instrukcije header-a
                    // we insert at the first phi node
                    if (isa<PHINode>(&I)) {
                        IRBuilder<> phiBuilder(&I);
                        for (auto &indvar: inductionMap) {
                            // avoid counter and unfinished (i, a, 0)
                            if ((indvar.second.multiplicativeStep != 1 && indvar.second.additiveStep != 0)) {

//                                Value *newIncomingValue = instrBuilder.CreateMul(preHeaderValue,ConstantInt::getSigned(preHeaderValue->getType(),indvar.second.multiplicativeStep));
//                                newIncomingValue = instrBuilder.CreateAdd(newIncomingValue,ConstantInt::getSigned(preHeaderValue->getType(), indvar.second.additiveStep));
                                Value *newIncomingValue = CalculateNewIncomingValue(I, indvar.second);
                                errs() << "New incoming value: " << *newIncomingValue << "\n";

                                //Pravimo novi phi cvor koji zamenjuje stari i koji ce imati inicijalizaciju j-ta
                                PHINode *newPhiNode = phiBuilder.CreatePHI(preHeaderValue->getType(), 2);

                                // menjamo entry, ondosno, dolazimo iz tog bloka
                                newPhiNode->addIncoming(newIncomingValue, preheaderBasicBlock);
                                PhiMap[indvar.first] = newPhiNode;
                            }
                        }
                    }
                }

                // prolazimo kroz mapu indukcionih promenljivih koje smo nasli iznad
                for (auto &indvar: inductionMap) {
                    if (PhiMap.count(indvar.first) &&
                        (indvar.second.multiplicativeStep != 1 && indvar.second.additiveStep != 0)) {
                        errs() << "IndVar parent: " << *indvar.second.parent << "\n";
                        // Prolazimo kroz instrukcije %for.inc basic block-a
                        for (auto &I: *incrementBasicBlock) {
                            if (auto op = dyn_cast<BinaryOperator>(&I)) {
                                Value *lhs = op->getOperand(0); // TODO: da li moze bez ovoga
                                Value *rhs = op->getOperand(1);
//                                errs() << "Instrukcija: ";
//                                I.print(errs());
//                                errs() << "\nLeva strana: ";
//                                lhs->print(errs());
//                                errs() << "\nDesna strana: ";
//                                rhs->print(errs());
//                                errs() << "\n\n";


                                // We are checking if one of the operands is a counter,
                                // so we can recognize the increment instruction
                                if (lhs == indvar.second.parent || rhs == indvar.second.parent) {
                                    IRBuilder<> instructionBuilder(&I);
                                    // za basic ind var for counter increment instruction
                                    InductionVarInfo incIndVar = inductionMap[&I];

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

                                    errs() << "New increment instruction:" << *newIncrementInstruction << "\n";

                                    phiVal->addIncoming(newIncrementInstruction, incrementBasicBlock);
                                    errs() << "Final phi instruction:   ";
                                    phiVal->print(errs());
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