# Strength Reduction Pass Project

**Strength reduction of induction variables**

*Explanation*:

Petlju hocemo da ubrzamo tako sto cemo skupe operacije (mnozenje)
zameniti jefitnijim (sabiranje). U primeru ispod mnogo je bolje izdvojiti
promenljivu `j` iznad petlje kao `j = 2` i da se svaki put u petlji racuna kao
`j = j + 6`.

```
i = 0;                      i = 0;
while (i < 10) {            j = 2;
    j = 3*i + 2;            while (i < 10) {
    a[j] = a[j] - 2;            a[j] = a[j] - 2;
    i = i + 2;                  i = i + 2;
                                j = j + 6;
}                           }
```

U ovom slucaju `j` je *induction variable* jer zavisi od brojaca petlje (`j =
3*i + 2`). Kod kompajlera to je linearna funkcija `f(i) = a*i + b` gde je `a`
multiplikativni faktor, a `b` aditivni. *Kombinacija vise induction promenljivih
je takodje induction promenljiva*. Na primer ako imamo da je `i` induction
promenljiva i `j = a*i + b`, kao i da je `k = c*j + d` tada ce `k` biti takodje
induction promenljiva sa faktorom mnozenja `a*c`, dok ce faktor sabiranja biti
jednak `b + d`.

Induction promenljive mozemo da predstavimo kao trojku `<x, y, z>`, gde je *x -
osnovna indukciona promenljiva*, *y - faktor mnozenja*, *z - faktor sabiranja*.
* Brojac `i` kao `i => <i, 1, 0>`
* Promenljivu `j = a*i + b` kao `j => <i, a, b>`
* Promenljivu `k = c*j + d` kao `k => <i, a*c, b + d>`
* itd.

**Algorithm**:

Kako naci indukcione promenljive?

1. Osnovnu indukcionu promenljivu nalazimo u phi cvoru (dobija se `-mem2reg`
   opt) header-a petlje.
2. Prolazimo kroz sve instrukcije petlje da bi nasli preostale indukcione
   promenljive
  3. Ako nadjemo `k = b * j` gde je `j` induction promenljiva sa trojkom `<i, c, d>`
     onda `k => <i, b*c, d>` cuvamo
  4. Ako nadjemo `k = j + b` gde je indukciona promenljiva sa trojkom `<i, c, d>`,
     onda `k => <i, c, d + b>` cuvamo
5. Zaustavljamo se kada nema vise promenljivih, npr. velicina strukture gde
   smestamo se ne menja.

Kako primeniti strength reduction? **doraditi...**

1. Potrebno je dodati induction promenljive u preheader ili header?? i
   izracunati im pocetnu vrednost
2. Zatim ubacimo nove instrukcije u telo petlje.

Problemi:
* Sta ako ima vise petlji, samim tim ima vise osnovnih indukcionih
  promenljivih?
* Jel menjamo phi node novim ili ne?
* Gde tacno treba staviti: preheader ili header petlje?

**Tasks**: ...

Compilation and invocation:

```
clang -S -fno-discard-value-names -emit-llvm -O0 -Xclang -disable-O0-optnone
opt   -S -load lib/LLVMStrengthReductionPass.so -mem2reg -matf-sr -enable-new-pm=0 <input>
```

Literatura:
* [LLVM Loop Terminology (and Canonical
  Forms)](https://releases.llvm.org/11.0.0/docs/LoopTerminology.html)
* [Loop Optimizations and Pointer Analysis, University of Texas
  ](https://www.cs.utexas.edu/~pingali/CS380C/2019/lectures/strengthReduction.pdf)

---

By:
* Matija Lojović - 45/2018
* Radovan Bozić - 172/2018
* Luka Colić - 31/2018
