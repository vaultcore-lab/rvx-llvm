add_llvm_component_library(LLVMRVXInfo
  TargetInfo.cpp    
  TargetInfo.h

  # LLVM framework dependencies:
  LINK_COMPONENTS
    MC
    Support
    TargetParser

  ADD_TO_COMPONENT
    RVX
)
