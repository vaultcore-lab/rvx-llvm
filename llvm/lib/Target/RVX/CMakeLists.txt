add_subdirectory(TargetInfo)
add_subdirectory(MCTargetDesc)
add_subdirectory(AsmParser)

add_llvm_target(RVX
  # Sources will be added here as the backend is implemented:
  # RVXTargetMachine.cpp
  # RVXSubtarget.cpp
  # RVXInstrInfo.cpp
  # RVXRegisterInfo.cpp
  # RVXFrameLowering.cpp
  # RVXTargetLowering.cpp
  # RVXISelDAGToDAG.cpp
  # RVXAsmPrinter.cpp

  # These are LLVM component libraries that RVXCodeGen depends on.
  # LLVM component names map to LLVMXXX library targets.
  #
  # Analysis         — various IR analysis passes (alias analysis, etc.)
  # AsmPrinter       — base AsmPrinter infrastructure (MachineInstr → MCInst)
  # CodeGen          — register allocator, prologue/epilogue, scheduler
  # Core             — IR types, Module, Function, BasicBlock
  # MC               — MCInst, MCStreamer, MCContext, MCSymbol
  # SelectionDAG     — SelectionDAG instruction selection framework
  # Support          — raw_ostream, Error, StringRef, etc.
  # Target           — TargetMachine, TargetLowering base classes
  # TransformUtils   — IR transformation utilities
  LINK_COMPONENTS
    Analysis
    AsmPrinter
    CodeGen
    CodeGenTypes
    Core
    MC
    SelectionDAG
    Support
    Target
    TransformUtils

  LINK_LIBS
    LLVMRVXDesc      # MC layer  (MCTargetDesc/)
    LLVMRVXInfo      # Target registry entry (TargetInfo/)
)
