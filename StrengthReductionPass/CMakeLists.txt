add_llvm_library( LLVMStrengthReductionPass MODULE
  ArithmeticStrengthReductionPass.cpp

  PLUGIN_TOOL
  opt
  )
