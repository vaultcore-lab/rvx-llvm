include "llvm/Target/Target.td"

// Subtarget modes
def IsRV32 : Predicate<"Subtarget->IsRV32()">;
def IsRV64 : Predicate<"Subtarget->IsRV64()">;

def RV32 : HwMode<[IsRV32]>;
def RV64 : HwMode<[IsRV64]>;

// Register information
include "RVXRegisterInfo.td"

// Calling convention
include "RVXCallingConv.td"

// Instructions
include "RVXInstrInfo.td"
