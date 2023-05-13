# Strength Reduction Pass Project

**Strength reduction of induction variables**

Explanation: ...

Algorithm: ...

Tasks: ...

Compilation and invocation:

```
clang -S -fno-discard-value-names -emit-llvm -O0 -Xclang -disable-O0-optnone
opt   -S -load lib/LLVMStrengthReductionPass.so -mem2reg -matf-sr -enable-new-pm=0 <input>
```

By:
* Matija Lojović - 45/2018
* Radovan Bozić - 172/2018
* Luka Colić - 31/2018
