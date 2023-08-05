# Strength Reduction Pass Project

### Strength reduction for induction variables

* Explanation for induction variables - [**presentation in serbian**](inductionVariables.pdf).
* Explanation for implementation - [**in serbian**](inductionVariables.md).

Compilation and invocation:
```
clang -S -fno-discard-value-names -emit-llvm -O0 -Xclang -disable-O0-optnone <input> -o <output.ll>
opt   -S -load path/to/lib/MatfStrengthReductionPass -mem2reg -matf-iv-sr -enable-new-pm=0 <input>
```

Check out directory `indVarTest` for small test.


### Replacing expansive operation with cheaper one

* Multiplication is replaced by shifting to the left
* Dividing is replaced by right-shifting
* Replacing modulo operation with faster one

Compilation and invocation:
```
clang -S -fno-discard-value-names -emit-llvm -O0 -Xclang -disable-O0-optnone <input> -o <output.ll>
clang -S -load path/to/lib/MatfStrengthReductionPass -matf-arit-sr -enable-new-pm=0 <input>
```

#### Test Script

`test.sh` compiles and runs C/C++ the programs in `TestPrograms` directory. Use the `-k` flag to keep the .ll files.

Programs are timed before and after the optimization pass, and outputs are compared to determine whether the opt is at least somewhat correct.

---

By:
* Matija Lojović - 45/2018
* Radovan Božić - 172/2018
* Luka Colić - 31/2018
