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
        std::vector<Instruction *> InstructionsToRemove;

        static char ID; // Pass identification, replacement for typeid
        IndVarsStrengthReductionPass() : FunctionPass(ID) {}

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


        bool runOnFunction(Function &F) override {
            LoopInfo &loopInfo = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

            // each induction variable consists of 3 things:
            //   1. basic induction variable;
            //   2. multiplicative factor;
            //   3. additive factor;
            // example:
            //          loop counter i:  i => (i, 1, 0)
            //          j = 3*i + 4:     j => (i, 3, 4)
            //          k = 2*j + 1:     k => (i, 2*3, 4+1)
            std::unordered_map < Value * , std::tuple < Value *, int, int >> indVarMap;


            for (auto *loop: loopInfo) {

                BasicBlock *header = loop->getHeader();
                BasicBlock *b_preheader = loop->getLoopPreheader();
//                BasicBlock* b_header = loop->getHeader();
                BasicBlock* b_body;

                for (auto &Instr: *header) {
                    if (isa<PHINode>(&Instr)) { // loop counter
//                        errs() << "\nPhi instruction found!\n";
                        errs() << Instr << "\n\n";

                        indVarMap[&Instr] = std::make_tuple(&Instr, 1, 0);
                    }
                }

                auto loopBlocks = loop->getBlocks();

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



//                errs() << "\n--------------IV_SR--------------\n";
//                for (auto &p : indVarMap) {
//                    errs() << p.first << " =>  ("
//                           << std::get<0>(p.second) << ", "
//                           << std::get<1>(p.second) << ", "
//                           << std::get<2>(p.second) << ")\n";
//                }

                // mapa koja slika instr u phi cvor
                map < Value * , PHINode * > PhiMap;

                Value *preheader_val;
                Instruction *insert_pos = b_preheader->getTerminator(); // pozocija posle koje ubacujemo inicijalizaciju prom. j = 7
                for (auto &I: *header) { // idemo kroz instrukcije header-a
                    // we insert at the first phi node
                    if (PHINode * PN = dyn_cast<PHINode>(&I)) {
                        int num_income = PN->getNumIncomingValues(); // dohvatamo broj BB iz kojih mozemo da dodjemo u phi cvor
                        assert(num_income == 2); // mora da ih bude dva
                        // find the preheader value of the phi node
                        for (int i = 0; i < num_income; i++) {
                            // u slucaju da smo dosli iz preheader-a
                            if (PN->getIncomingBlock(i) == b_preheader) {
                                // uzimamo vred. ind prom, npr. i = 2
                                preheader_val = PN->getIncomingValue(i);
                            } else {
                                // inace znaci da smo dosli iz tela petlje izimamo telo ili instr?? TODO
                                b_body = PN->getIncomingBlock(i); // pokazivac na BB tela petlje
                            }
                        }


                        // TODO
                        IRBuilder<> head_builder(&I);

                        // da bi smo dodati novu inicijalnu vrednost ind promenljive koja nije brojac
                        IRBuilder<> preheader_builder(insert_pos);

                        for (auto &indvar: indVarMap) {
                            tuple < Value * , int, int > t = indvar.second;
                            // izbegavamo brojac petlje i one neptpune npr. (i, 3, 0) i proveravamo da li je
                            // brojac petlje bas i
                            if (get<0>(t) == PN && (get<1>(t) != 1 && get<2>(t) != 0)
                            ) { // not a basic indvar
                                // calculate the new indvar according to the preheader value

//                                errs() << std::get<0>(t) << " " << std::get<1>(t) << " " << std::get<2>(t) << "\n";
//                                errs() << "-------Prva petlja----------";
                                // Ako nam je npr. j = 2*i + 3, potrebno je prvo da napravimo instr mnozenja, zatim
                                // instr sabiranja kako bi smo postavili inicijalnu vrednost za j
                                // (npr. ako i != 0, na pocetku (krece od npr. 2))
                                Value *new_incoming = preheader_builder.CreateMul(preheader_val,
                                                                                  ConstantInt::getSigned(
                                                                                          preheader_val->getType(),
                                                                                          get<1>(t)));


                                new_incoming = preheader_builder.CreateAdd(new_incoming,
                                                                           ConstantInt::getSigned(
                                                                                   preheader_val->getType(),
                                                                                   get<2>(t)));

                                // Pravimo novi phi cvor koji zamenjuje stari i koji ce imati inicijalizaciju j-ta
                                PHINode *new_phi = head_builder.CreatePHI(preheader_val->getType(), 2);

                                // menjamo entry, ondosno, dolazimo iz tog bloka
                                new_phi->addIncoming(new_incoming, b_preheader);
                                PhiMap[indvar.first] = new_phi;
                            }
                        }
                    }
                }

//                errs() << "\n\n";
                // modify the new body block by inserting cheaper computation
                for (auto &indvar: indVarMap) {
                    tuple < Value * , int, int > t = indvar.second; // j => (i, a, b)
                    // proveravamo da li smo za tekucu promenljivu vec modifikovali preheader i ako jesmo
                    // treba dalje da modifikujemo telo petlje
                    if (PhiMap.count(indvar.first) && (get<1>(t) != 1 && get<2>(t) != 0)) { // not a basic indvar

//                        errs() << std::get<0>(t) << " " << std::get<1>(t) << " " << std::get<2>(t) << "\n";



                        // prolazimo kroz instr tela petlje
                        for (auto &I: *b_body) {
                            // da li je binarni
                            if (auto op = dyn_cast<BinaryOperator>(&I)) {
                                Value *lhs = op->getOperand(0);
                                Value *rhs = op->getOperand(1);

                                // proveravamo da li je jedan od operanada nas brojac kako bismo
                                // prepoznali instrukciju njegove inkrementacije
                                if (lhs == get<0>(t) || rhs == get<0>(t)) {


                                    IRBuilder<> body_builder(&I);
                                    // za basic ind var
                                    tuple < Value * , int, int > t_basic = indVarMap[&I];
                                    // (i, 1, b)
                                    // potrebno je da vidimo za koliko se inkrementira brojac, da bi
                                    // izracunali za koliko treba da povecamo nasu promenljivu
                                    // nas add faktor * za_koliko_se_inc_brojac

                                    // i -> (i, 1, 1);
                                    // j -> (i, 3, 3);
                                    //               1      *        3        =      3
                                    int new_val = get<1>(t) * get<2>(t_basic);

                                    PHINode *phi_val = PhiMap[indvar.first];

                                    Value *new_incoming = body_builder.CreateAdd(phi_val,
                                                                                 ConstantInt::getSigned(
                                                                                         phi_val->getType(), new_val));

                                    // U telo petlje dodajemo novu instr sabiranja koja ce zameniti prethodno mnozenje
                                    // i sabiranje
                                    phi_val->addIncoming(new_incoming, b_body);
                                }
                            }
                        }
                    }
                }

                // replace all the original uses with phi-node
                for (auto &phi_val: PhiMap) {
                    (phi_val.first)->replaceAllUsesWith(phi_val.second);
                }

            } // finish all loops


            // do another round of optimization
//            FPM.doInitialization();
//            changed = FPM.run(F);
//            FPM.doFinalization();

            return true;
        }




    };
}

char IndVarsStrengthReductionPass::ID = 0;
static RegisterPass<IndVarsStrengthReductionPass> X("matf-iv-sr", "MATF strength reduction of induction variables");