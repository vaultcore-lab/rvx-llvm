set(LLVM_TARGET_DEFINITIONS ../RVX.td)

tablegen(LLVM RVXGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM RVXGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM RVXGenSubtargetInfo.inc -gen-subtarget)
tablegen(LLVM RVXGenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM RVXGenAsmWriter.inc -gen-asm-writer)
add_public_tablegen_target(RVXCommonTableGen)


add_llvm_component_library(LLVMRVXDesc

  RVXAsmBackend.cpp        # fixup application, relaxation, NOP fill
  RVXELFObjectWriter.cpp   # fixup kind → ELF R_RISCV_xxx relocation type
  RVXInstPrinter.cpp       # MCInst → human-readable asm text
  RVXMCAsmInfo.cpp         # assembly syntax conventions (comment char, etc.)
  RVXMCCodeEmitter.cpp     # MCInst → binary bytes + fixup recording
  RVXMCFixupKinds.cpp      # fixup kind support functions (if not header-only)
  RVXMCTargetDesc.cpp      # LLVMInitializeRVXTargetMC() registration hub

  # DEPENDS ensures all .inc files are generated before any .cpp in this
  # library is compiled. Without this, the #include "RVXGenXxx.inc" lines
  # in the .cpp files would fail with "file not found" errors during a
  # parallel build where tablegen hasn't finished yet.
  DEPENDS
    RVXCommonTableGen       # generates all RVXGen*.inc files

  # These are the LLVM component libraries that RVXDesc's source files
  # directly include headers from or call functions in.
  #
  # BinaryFormat  — ELF.h with R_RISCV_xxx constants, ELF class definitions
  # MC            — MCInst, MCStreamer, MCCodeEmitter, MCAsmBackend, etc.
  # MCELFObjectWriter — MCELFObjectTargetWriter base class
  # Support       — raw_ostream, StringRef, Error, endian helpers
  # TargetParser  — Triple parsing
  LINK_COMPONENTS
    BinaryFormat
    MC
    MCELFObjectWriter
    Support
    TargetParser

  # RVXDesc depends on RVXInfo for TheRVXTarget (the Target object that
  # LLVMInitializeRVXTargetMC() passes to TargetRegistry::RegisterXxx calls).
  LINK_LIBS
    LLVMRVXInfo

  # Register this library as part of the RVX component group.
  ADD_TO_COMPONENT
    RVX
)
