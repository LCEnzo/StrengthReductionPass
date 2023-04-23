add_llvm_library( LLVMStrengthReductionPass MODULE
  StrengthReductionPass.cpp

  PLUGIN_TOOL
  opt
  )
