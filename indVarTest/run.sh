llprog="primer.ll"
cprog="primer.c"

lib="../../../../../build/lib/MatfStrengthReductionPass.so"
pass="-matf-iv-sr"

rm -f "primer_mem.ll" "primer_opt" "primer_orig"

clang -fno-discard-value-names  \
      -S -emit-llvm -O0 -Xclang \
      -disable-O0-optnone       \
      "$cprog" -o "$llprog"


opt -S \
  -mem2reg "$llprog" -o "primer_mem.ll"


opt -S               \
    -load "$lib"     \
    -mem2reg         \
    "$pass"          \
    -dce             \
    -enable-new-pm=0 \
    "$llprog"        \
    -o "primer_opt.ll"
    #>/dev/null 

clang "$llprog" -lm       -o "primer_orig"
clang "primer_opt.ll" -lm -o "primer_opt"

