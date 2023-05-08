# Strength Reduction Pass Project

By:
* Matija Lojović - 45/2018
* Radovan Bozić - 172/2018
* Luka Colić - 31/2018

---

`test.sh` compiles and runs C/C++ the programs in `TestPrograms` direcctory. 

Programs are timed before and after the optimization pass, and outputs are compared to determine whether the opt is at least somewhat correct.

Before using the script, change the following variable to point to the build folder of your local LLVM. E.g.
```sh
BUILD_DIR="/home/qcerris-user/KK/llvm-project/build"
```
