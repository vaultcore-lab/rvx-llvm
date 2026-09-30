defvar XLEN32 = 4;

// Callee-saved registers
def CSR_ILP32_LP64 : CalleeSavedRegs<
  (add RA, FP, S1, S2, S3, S4, S5, S6, S7, S8, S9, S10, S11)
>;

def GPRArgRegs : RegisterClass<"RVX", [XLenVT], 32,
  (add A0, A1, A2, A3, A4, A5, A6, A7)
>;

def CC_RVX : CallingConv<[
  CCIfType<[i8, i16], CCPromoteToType<XLenVT>>,

  CCIfType<[XLenVT],
           CCAssignToReg<[A0, A1, A2, A3, A4, A5, A6, A7]>>,

  CCIfType<[XLenVT],
           CCAssignToStack<XLEN32, XLEN32>>
]>;

def RetCC_RVX : CallingConv<[
  CCIfType<[XLenVT], CCAssignToReg<[A0, A1]>>
]>;
