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

        void printBasicBlock(BasicBlock* BB, string msg) {
            errs() << "\n---------" << msg << "----------\n";
            for (auto &Instr : *BB) {
                errs() << Instr << "\n";
            }
            errs() << "-----------------------------------\n";
        }

        void printTuple(tuple <Value * ,int, int> t) {
            errs() << std::get<0>(t) << " "
                   << std::get<1>(t) << " "
                   << std::get<2>(t) << "\n\n";

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



                errs() << "\n--------------IV_SR--------------\n";
                for (auto &p : indVarMap) {
                    p.first->print(errs());
                    errs() << " =>  ("
                           << std::get<0>(p.second) << ", "
                           << std::get<1>(p.second) << ", "
                           << std::get<2>(p.second) << ")\n";
                }
                errs() << "\n";

                // mapa koja slika instr u phi cvor
                map < Value * , PHINode * > PhiMap;

                /*
                 *     %result = phi i32 [%a, %block1], [%b, %block2]
                 * This means that if the previous block was block1, choose value a.
                 * If the previous block was block2, choose value b.
                 *
                 * Why do we write like this? This is to prevent assigning result in two
                 * different blocks such as if block and else block. Because, we do not
                 * want to violate SSA principle. SSA helps compilers to apply variety of
                 * optimizations and it is a de-facto standard for the intermediate codes.
                 */


                /*
                 * Polazimo kroz instrukcije header-a, kada nadjiemo na instrukciju PHI
                 * to znaci da smo naisli na brojac, odnoso baznu indukcionu promenljvu.
                 *
                 * TODO: nju cemo kopirati i izmeniti da bi stavili druge indukcione promenljive.
                 *       zbog toga nam treba:
                 *          1) koja joj je pocetna vrednost kad dolazi iz %entry bloka da bi stavili nasu
                 *             (umesto 0 koja je za brojac, nasa bi bila 981);
                 *          2) kad dolazi iz dela za uvecavanje (for.inc) jer tu stavljamo nasu instrukciju
                 *             za uvecavanje (npr. %1 = add i64 %0, 2; iznad one postojece za brojac petlje - i)
                 */
                Value *preheader_val;

                // OLD: pozocija posle koje ubacujemo inicijalizaciju indukcione prom. (npr. j = 7)

                // Vratice nam poslednju instrukciju iz %entry bloka: `br label %for.cond`
                Instruction *insert_pos = b_preheader->getTerminator();

                printBasicBlock(b_preheader, "b_preheader");

                errs() << "---Insert position---\n";
                insert_pos->print(errs());
                errs() << "\n-------------------\n\n";


                /*
                 * UOPSTENO:
                 *           Mi indukcione promenljive koje smo pronasli pretvaramo u Phi instrukcije.
                 *           Ta Phi instrukcija ce se naci na samom pocetku petlje (u for.cond delu),
                 *           kako bi je napravili, iskoristicemo vec postojecu Phi instrukciju za brojac,
                 *           ali cemo je izmeniti kako nama odgovara.
                 *
                 *           %i.0 = phi i64 [ 0, %entry ], [ %add2, %for.inc ]
                 *
                 *           U petlji ispod menjamo [ 0, %entry ] deo. Zato prolazimo kroz header petlje,
                 *           kao i kroz mapu ind promenljivih. Kako racunamo novi broj umesto 0 je objasnjeno
                 *           dole.
                 *
                 *           Pravimo novu phi instrukciju koja ce ima dva bloka `phi [...] [...]`. Prvi
                 *           blok sadrzi tu novu iznacunatu vrednost i dosao je iz `%entry` bloka, odnosno
                 *           preheader-a:
                 *                        new_phi->addIncoming(new_incoming, b_preheader);
                 *
                 *           Posle petlje nasa Phi instrukcija izglda: %0 = phi i64 [ 981, %entry ].
                 *           U petlji ispod ove popunjavmo dugi deo Phi instrukcije, u %for.inc delu
                 *           stavljamo inkrement nase promenljive.
                 *
                 */

                for (auto &I: *header) { // idemo kroz instrukcije header-a
                    // we insert at the first phi node
                    if (PHINode * PN = dyn_cast<PHINode>(&I)) {
                        int num_income = PN->getNumIncomingValues(); // dohvatamo broj BB iz kojih mozemo da dodjemo u phi cvor
//                        assert(num_income == 2); // mora da ih bude dva
                        // find the preheader value of the phi node

                        errs() << "---PN->getNumIncomingValues()---\n";
                        errs() << num_income << "\n---------------\n";
                        for (int i = 0; i < num_income; i++) {
                            // u slucaju da smo dosli iz preheader-a
                            // phi i64 [ 0, %entry ] --> to je ovde %entry block
                            if (PN->getIncomingBlock(i) == b_preheader) {
                                // uzimamo vred. ind prom, npr. i = 2 ili i = 0
                                // phi i64 [ 0, %entry ] [...], ovde uzimamo 0
                                preheader_val = PN->getIncomingValue(i);

                                errs() << "---PN->getIncomingValue(i)---\n";
                                preheader_val->print(errs());
                                errs() << "\n-------------------\n";
                            } else {
                                // inace znaci da smo dosli iz poslednjeg dela petlje, kada se
                                // uvecava brojac petlje (i)

                                // pokazivac na BB for.inc tela petlje (poslednjeg dela petlje)
                                b_body = PN->getIncomingBlock(i); // pokazivac na BB tela petlje

                                printBasicBlock(b_body, "PN->getIncomingBlock(i)");
                            }
                        }


                        // Pravimo novu instrukicju na poziciji instrukicje I koju smo prosledili,
                        // to je zapravio PHI instrukcija. Da li to znaci da IZAND ili ISPOD dodajemo
                        // instrukcije -- nemam pojma.
                        IRBuilder<> head_builder(&I);

                        // da bi smo dodati novu inicijalnu vrednost ind promenljive koja nije brojac
                        IRBuilder<> preheader_builder(insert_pos);

                        for (auto &indvar: indVarMap) {
                            tuple < Value * , int, int > t = indvar.second;
                            // OLD: izbegavamo brojac petlje i one neptpune npr. (i, 3, 0) i proveravamo da li je
                            //      brojac petlje bas i

                            // Izbegavamo baznu indukcionu promenljvu --- brojac i => (i, 1, 0),
                            // takodje izbegavamo nepotpune clanove mape:
                            //    *) Ako imamo promenljvu j = 3*i + 5, nas algoritam ce cuvati:
                            //        1) j => (&i, 3, 0) --> mi znamo da nam ovo ne treba, jer je aditivni faktor = 0
                            //        2) j => (&i, 3, 5)
                            //       Svaka nasa instrukcija ce biti u obliku j = a*i + b, ne dozvoljavamo j = a*i!
                            if (get<0>(t) == PN && (get<1>(t) != 1 && get<2>(t) != 0)
                            ) {
//                                errs() << "----Prva petlja----\n";
//                                printTuple(t);
//                                errs() << "\n";

                                //!!!! Racunamo pocetnu vrednost bazne promenljive:

                                // OLD: Ako nam je npr. j = 2*i + 3, potrebno je prvo da napravimo instr mnozenja, zatim
                                // instr sabiranja kako bi smo postavili inicijalnu vrednost za j
                                // (npr. ako i != 0, na pocetku (krece od npr. 2))
                                Value *new_incoming = preheader_builder.CreateMul(preheader_val,
                                                                                  ConstantInt::getSigned(
                                                                                          preheader_val->getType(),
                                                                                          get<1>(t)));
//                                new_incoming->print(errs());
//                                errs() << "\n\n";

                                new_incoming = preheader_builder.CreateAdd(new_incoming,
                                                                           ConstantInt::getSigned(
                                                                                   preheader_val->getType(),
                                                                                   get<2>(t)));

//                                new_incoming->print(errs());
//                                errs() << "\n\n";

                                // Pravimo novi phi cvor koji zamenjuje stari i koji ce imati inicijalizaciju j-ta
                                PHINode *new_phi = head_builder.CreatePHI(preheader_val->getType(), 2);

                                errs() << "PHI:\n";
                                new_phi->print(errs());
                                errs() << "\n";

                                // menjamo entry, ondosno, dolazimo iz tog bloka
                                new_phi->addIncoming(new_incoming, b_preheader);
                                errs() << "PHI updated:\n";
                                new_phi->print(errs());
                                errs() << "\n";
                                PhiMap[indvar.first] = new_phi;
                            }
                        }
                    }
                }

                errs() << "---PHI MAPA---\n";
                for (auto &p : PhiMap) {
                    errs() << "Value: ";
                    p.first->print(errs());
                    errs() << "\n";
                    errs() << "Phi: ";
                    p.second->print(errs());
                    errs() << "\n";
                }
                errs() << "----------------\n";


                /*
                 * UOPSTENO:
                 *          Sad je potrebno da izracunamo drgui deo Phi instrukcije. Za to name je
                 *          potrebna nasa ind promenljiva koja nema aditivni faktor jednak nuli.
                 *
                 *          Taj drugi deo Phi instr ce biti instrukcija dodavanja konstantnog koraka.
                 *          Nju treba da stavimo u %for.inc bloku. Promenljivoj `b_body` smo vec
                 *          dodelili vrednost tog bloka u petlji iznad naredbom:
                 *              b_body = PN->getIncomingBlock(i)
                 *                  > %add2 = add nsw i64 %i.0, 1
                 *                  > br label %for.cond, !llvm.loop !6
                 *
                 *          Kad u b_body prvo treba da naidjemo na binarnu instrukciju i nju cemo
                 *          koristiti kao poziciju za nasu novu instrukciju. Detalji oko faktora
                 *          koji se koristi za sabiranje dati su dole.
                 *
                 *          Kad napravimo instrukciju sa odg faktorom. Treba je dodati u nasu Phi
                 *          instrukciju iz mape sa: `phi_val->addIncoming(new_incoming, b_body);`
                 *
                 */


                // prolazimo kroz mapu indukcionih promenljivih koje smo nasli iznad
                for (auto &indvar: indVarMap) {
                    tuple < Value * , int, int > t = indvar.second; // j => (i, a, b)

                    // OLD: proveravamo da li smo za tekucu promenljivu vec modifikovali preheader
                    //      i ako jesmo treba dalje da modifikujemo telo petlje

                    // Izbegavamo baznu indukcionu promenljvu --- brojac i => (i, 1, 0),
                    // takodje izbegavamo nepotpune clanove mape:
                    //     * Ako imamo promenljvu j = 3*i + 5, nas algoritam ce cuvati:
                    //        1) j => (&i, 3, 0) --> mi znamo da nam ovo ne treba, jer je aditivni faktor = 0
                    //        2) j => (&i, 3, 5)
                    //       Svaka nasa instrukcija ce biti u obliku j = a*i + b, ne dozvoljavamo j = a*i!
                    if (PhiMap.count(indvar.first) && (get<1>(t) != 1 && get<2>(t) != 0)) {
//                        errs() << "\n---Ind prom za koje treba napraviti nove instrukcije---\n";
//                        printTuple(t);
//                        errs() << "\n";


                        // Prolazimo kroz instrukcije %for.inc basic block-a
                        errs() << "Instrukcije (druga petlja, loc=368)\n";
                        for (auto &I: *b_body) {
                            I.print(errs());
                            errs() << "\n";

                            // da li je binarni
                            if (auto op = dyn_cast<BinaryOperator>(&I)) {
                                Value *lhs = op->getOperand(0); // TODO: da li moze bez ovoga
                                Value *rhs = op->getOperand(1);

                                /*
                                errs() << "Leva strana: ";
                                lhs->print(errs());
                                errs() << "\nDesna strana: ";
                                rhs->print(errs());
                                errs() << "\n\n";
                                 */

                                // FIXME: bolje objasnjenje
                                //  proveravamo da li je jedan od operanada nas brojac kako bismo
                                //  prepoznali instrukciju njegove inkrementacije
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

//                                    errs() << "\nZavrsni instrukcija\n";
//                                    new_incoming->print(errs());
//                                    errs() << "\n\n";

                                    // U telo petlje dodajemo novu instr sabiranja koja ce zameniti prethodno mnozenje
                                    // i sabiranje
                                    phi_val->addIncoming(new_incoming, b_body);
//                                    errs() << "\n\nZavrsna phi instrukcija\n";
//                                    phi_val->print(errs());
//                                    errs() << "\n\n";
                                }
                            }
                        }
                    }
                }

                errs() << "---PHI MAPA NA KRAJU---\n";
                for (auto &p : PhiMap) {
                    errs() << "Value: ";
                    p.first->print(errs());
                    errs() << "\n";
                    errs() << "Phi: ";
                    p.second->print(errs());
                    errs() << "\n";
                }
                errs() << "----------------\n";

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