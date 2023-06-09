add_llvm_library( MatfStrengthReductionPass MODULE
  ArithmeticStrengthReductionPass.cpp
  IndVarsStrengthReductionPass.cpp

  PLUGIN_TOOL
  opt
  )
