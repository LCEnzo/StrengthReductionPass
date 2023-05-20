#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$(realpath $SCRIPT_DIR/../../../../build)"
TEST_PROGRAMS_DIR="${SCRIPT_DIR}/TestPrograms"
OPT_PASS_DIR="${BUILD_DIR}/lib"
LLVM_BIN_DIR="${BUILD_DIR}/bin"

delete_ll_files=1

# Number of times to run each program
NUM_RUNS=10

# Parse input arguments
while getopts "k" opt; do
  case $opt in
    k)
      delete_ll_files=0
      ;;
    *)
      echo "Usage: $0 [-k]"
      echo "  -k  Keep .ll files"
      exit 1
      ;;
  esac
done

test_program() {
    local program_name=$1
    local ext=$2

    # Compile the test program into LLVM IR
    "$LLVM_BIN_DIR"/clang -O0 -S -emit-llvm -Xclang -disable-llvm-passes "$program_name.$ext" -o "${program_name}.ll"

    # Apply the optimization pass to create optimized IR
    "$LLVM_BIN_DIR"/opt -load "$OPT_PASS_DIR"/"LLVMStrengthReductionPass.so" -arit-matf-sr -enable-new-pm=0 "${program_name}.ll" -S -o "${program_name}_optimized.ll"

    # Compile the LLVM IR (with and without optimization) into executables
    "$LLVM_BIN_DIR"/clang "${program_name}.ll" -o "${program_name}" -lm
    "$LLVM_BIN_DIR"/clang "${program_name}_optimized.ll" -o "${program_name}_optimized" -lm

    # Run the executables and measure their performance, and save outputs for comparison
    echo ""
    echo "Running ${program_name} without optimization:"
    # Done to have output for diff
    ./"${program_name}" > "${program_name}_output_original.txt"

    time (for i in $(seq 1 $NUM_RUNS); do
        "./${program_name}" >/dev/null 2>&1
    done)

    echo ""
    echo "Running ${program_name} with optimization:"
    ./"${program_name}_optimized" > "${program_name}_output_optimized.txt"

    time (for i in $(seq 1 $NUM_RUNS); do
        "./${program_name}_optimized" >/dev/null 2>&1
    done)

    # Compare the outputs to see if the optimization changes the behavior of the program
    diff "${program_name}_output_original.txt" "${program_name}_output_optimized.txt"

    # Check the exit status of the 'diff' command
    if [ $? -ne 0 ]; then
        echo ""
        echo -e "\033[31mThe outputs of the unoptimized and optimized ${program_name} programs are different."
        echo -e "The optimization pass is not correct.\033[0m"
    fi

    echo "───────────────────────────────────────"

    rm "${program_name}" "${program_name}_optimized" # "${program_name}_output_original.txt" "${program_name}_output_optimized.txt"

    # Conditionally delete .ll files based on the delete_ll_files variable
    if [ $delete_ll_files -eq 1 ]; then
        rm "${program_name}.ll" "${program_name}_optimized.ll"
    fi
}

# --------     MAIN     -------- #

pushd $TEST_PROGRAMS_DIR > /dev/null

echo "───────────────────────────────────────"

# Initialize an array of file extensions
for ext in c cpp c++; do
    for test_program in $TEST_PROGRAMS_DIR/*.$ext; do
        if [ -f "$test_program" ]; then
            program_name=$(basename "$test_program" .$ext)
            echo "$program_name.$ext"

            test_program $program_name $ext
        fi
    done
done

popd > /dev/null
