include "llvm/Target/Target.td"


def IsRV32 : Predicate<"Subtarget->IsRV32()">; 
def IsRV64 : Predicate<"Subtarget->IsRV64()">; 

def RV32 : HwMode<[IsRV32]>;
def RV64 : HwMode<[IsRV64]>; 


