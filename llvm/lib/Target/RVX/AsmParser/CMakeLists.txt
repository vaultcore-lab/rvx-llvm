# We define a SEPARATE tablegen target (RVXAsmParserTableGen) rather than
# adding to RVXCommonTableGen. This is important because:
#   a) The asm matcher .inc is only needed by the AsmParser library —
#      other libraries don't need it and shouldn't have to wait for it.
#   b) It keeps the dependency graph clean: RVXCommonTableGen is for the
#      MC layer; the asm parser has its own tablegen outputs.
#   c) Parallel builds benefit from finer-grained tablegen dependencies.

set(LLVM_TARGET_DEFINITIONS ../RVX.td)

# -gen-asm-matcher reads:
#   - AsmString fields on instruction definitions → mnemonic matching table
#   - ParserMatchClass on operand definitions     → operand parsing dispatch
#   - MnemonicAlias / InstAlias entries           → alias expansion
#   - RegisterClass definitions                   → register name tables
tablegen(LLVM RVXGenAsmMatcher.inc -gen-asm-matcher)
add_public_tablegen_target(RVXAsmParserTableGen)

add_llvm_component_library(LLVMRVXAsmParser

  RVXAsmParser.cpp    # the full assembly parser implementation

  # The AsmParser needs BOTH its own .inc AND the common ones (for register
  # and instruction enums that RVXAsmParser.cpp uses via #include).
  DEPENDS
    RVXAsmParserTableGen    # generates RVXGenAsmMatcher.inc
    RVXCommonTableGen       # generates RVXGenRegisterInfo.inc etc.
                            # (needed for register number enums in the parser)

  # MC       — MCParser, MCInst, MCStreamer, MCAsmParser base classes
  # MCParser — MCAsmParser, MCAsmLexer, MCAsmParserExtension
  # Support  — StringRef, raw_ostream
  LINK_COMPONENTS
    MC
    MCParser
    Support
    TargetParser

  # The AsmParser needs RVXDesc to create MCInsts (it uses the MC register
  # and instruction info during operand validation), and RVXInfo for
  # TheRVXTarget (so it can register itself with the TargetRegistry).
  LINK_LIBS
    LLVMRVXDesc
    LLVMRVXInfo

  ADD_TO_COMPONENT
    RVX
)
