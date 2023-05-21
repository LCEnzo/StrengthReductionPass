add_llvm_library( LLVMStrengthReductionPass MODULE
  ArithmeticStrengthReductionPass.cpp
  IndVarsStrengthReductionPass.cpp

  PLUGIN_TOOL
  opt
  )
