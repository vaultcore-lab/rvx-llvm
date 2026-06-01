include "RVXInstrFormats.td"
include "RVXRegisterInfo.td"

defvar XLEN32 = 4;

// Callee-saved registers
def CSR_ILP32_LP64 : CalleeSavedRegs<
  (add X1,X8,X9,X18,X19,X20,X21,X22,X23,X24,X25,X26,X27)
>;

def GPRArgRegs : RegisterClass<"RVX", [XLenVT], 32,
  (add X10,X11,X12,X13,X14,X15,X16,X17)>;

def CC_RVX : CallingConv<[

  CCIfType<[i8,i16], CCPromoteToType<XLenVT>>,

  CCIfType<[XLenVT], CCAssignToReg<[X10,X11,X12,X13,X14,X15,X16,X17]>>,

  CCIfType<[XLenVT], CCAssignToStack<XLEN32,XLEN32>>
]>;

def RetCC_RVX : CallingConv<[
  CCIfType<[XLenVT], CCAssignToReg<[X10,X11]>>
]>;
