**Strength reduction of induction variables**

*Objasnjenje*:

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

**Algoritam**:

Posto radimo sa **registrima** memorije, moramo prvo da ukljucimo opciju `-mem2reg` kada pozivamo `opt`.  
Kako naci indukcione promenljive?

1. Osnovnu indukcionu promenljivu nalazimo u phi cvoru (dobija se `-mem2reg`
   opt) header-a petlje.
2. Prolazimo kroz sve instrukcije petlje da bi nasli preostale indukcione
   promenljive
3. Ako nadjemo `k = b * j` gde je `j` induction promenljiva sa trojkom `<i, c, d>`
   onda `k => <i, b*c, d>` cuvamo
4. Ako nadjemo `k = j + b` gde je indukciona promenljiva sa trojkom `<i, c, d>`,
   onda `k => <i, c, d + b>` cuvamo

Kako primeniti strength reduction?

1. Prolazimo kroz instrukcije header-a petlje, jer je potrebno tu dodati nasu novu
   promenljivu i izracunati joj pocetnu vrednost.
2. Zatim se prolazi kroz indukcione promenljive koje smo cuvali u mapi i prolazi se kroz
   dela petlje gde se nalaze instrukcije za uvecanje brojaca (to je u kodu promenljiva)
   `incrementBasicBlock`.
3. Potrebno je odna izracunati za koliko se uvecava nasa nova prmenljiva (iz koraka `1.`).

**Pogledati izvorni fajl [IndVarsStrengthReductionPass.cpp](StrengthReductionPass/IndVarsStrengthReductionPass.cpp),
komentari pruzaju dodatna i detaljnija objasnjena.**

Ako je `j` indukciona promenljiva (npr. `j = 3*i + 1`) potrebno je nju staviti
van petlje, na isto mesto kao i brojac petlje (pogledati **Objasnjenje** gore).

> **Brojac se nalazi u header-u petlje i dat je kao phi instrukcija, znaci da i
> nasa promenljiva `j` mora biti phi instrukcija**!

Preciznije, kako mi ne mozemo da nadjemo pocetak header bloka, uopste pocetak
basic block-a (mislim da je pominjao na casu ovo), nego samo
terminator/poslednju instrukciju, moramo sa `getTerminator()` da dohvatimo kraj
preheader bloka i odmah posle njega da ubacimo napravljenu phi instrukciju.
Slika za pomoc:

![](./imgs/izvorni.png)


Znacenje *phi* instrukcije (valjda):

`%1 = phi i32 [1, %entry], [%2, %for.inc]`

* Ako je kontrola toka dosla iz `%entry` bloka, onda ce promenljiva `%1` imati
  vrednost `1` (const).
* Ako je kontrola toka dosla iz `%for.inc` bloka, promenljiva `%1` ce imati
  vrednost promenljive `%2`.

Posto nemamo funkciju koja vraca samo telo petlje bez header-a, mora manuelno da
se te instrukcije cuvaju negde prolazeci kroz celu petlju (header i telo su
spojeni, vidi sliku gore).

Pogledati funkcije `getIncomingValue()` i `getIncomingBlock()` koje su vezane za
*phi* blok!

![](./imgs/poredjenje.png)

> Kako bi trebalo da izgleda.

**Tasks**:

- [x] Postaviti pronadjenju indukcionu promenljivu na svoje mesto.
- [x] Induk prom dodeliti dobre vrednosti (za blok iz koga se dolazi, kao i
  vrednot) unutar phi instrukcije
- [x] Proci kroz blok petlje i zameniti instrukcije
- [x] Obrisati instukcije u bloku petlje koje se ne koriste (moze i sa `-dce` opcijom)
- [x] Dodati da radi sa vise indukcionih promenljivih
- [ ] ~~Ugnjezdene ind prom (`j = a*i + c, k = c*j + d`) (nismo uspeli)~~


## Literatura
* [LLVM Loop Terminology (and Canonical
  Forms)](https://releases.llvm.org/11.0.0/docs/LoopTerminology.html)
* [Loop Optimizations and Pointer Analysis, University of Texas
  ](https://www.cs.utexas.edu/~pingali/CS380C/2019/lectures/strengthReduction.pdf)
* [The CS 6120 Course Blog: Strength Reduction Pass in LLVM
  ](https://www.cs.cornell.edu/courses/cs6120/2019fa/blog/strength-reduction-pass-in-llvm/)
