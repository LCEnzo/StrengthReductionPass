# Strength Reduction Pass Project

The LLVM Strength Reduction Pass, developed at the Faculty of Mathematics of the University of Belgrade, is a compiler optimization project that transforms specific operations into more efficient ones. In particular, the optimization pass does strength reduction (replacing slower operations with faster ones) on induction variables in loops, and some arithmetic operations for powers of two. 

### Strength reduction for induction variables

* Explanation for induction variables - [**Presentation in Serbian**](inductionVariables.pdf).
* Explanation for implementation - [**In Serbian**](inductionVariables.md).

Compilation and invocation:
```
clang -S -fno-discard-value-names -emit-llvm -O0 -Xclang -disable-O0-optnone <input> -o <output.ll>
opt   -S -load path/to/lib/MatfStrengthReductionPass -mem2reg -matf-iv-sr -enable-new-pm=0 <input>
```

Check out directory `indVarTest` for a small example test.

### Replacing expansive operations with cheaper ones

* Multiplication by a power of 2 is replaced by left-shifting
* Dividision by a power of 2 is replaced by right-shifting
* Modulo by a power of 2 is replaced with logical and using a mask

Compilation and invocation:
```
clang -S -fno-discard-value-names -emit-llvm -O0 -Xclang -disable-O0-optnone <input> -o <output.ll>
clang -S -load path/to/lib/MatfStrengthReductionPass -matf-arit-sr -enable-new-pm=0 <input>
```

#### Test Script

`test.sh` compiles and runs C/C++ the programs in `TestPrograms` directory. Use the `-k` flag to keep the .ll files.

Programs are timed before and after the optimization pass, and outputs are compared to determine whether the opt is at least somewhat correct.

---

Contributors:
* Matija Lojović - 45/2018
* Radovan Božić - 172/2018
* Luka Colić - 31/2018
