include "RVXInstrFormats.td"

//===----------------------------------------------------------------------===//
// Itineraries
//===----------------------------------------------------------------------===//

// Temporary itinerary classes.
// RVXSchedule.td is intentionally not used yet.

def IIAlu   : InstrItinClass;
def IIC_Any : InstrItinClass;


//===----------------------------------------------------------------------===//
// Immediate operands
//===----------------------------------------------------------------------===//

def simm12 : Operand<XLenVT>,
    ImmLeaf<XLenVT, [{ return isInt<12>(Imm); }]> {
  let DecoderMethod = "decodeImmOperand<12>";
}

def simm20u : Operand<XLenVT>,
    ImmLeaf<XLenVT, [{ return isInt<32>(Imm) && !(Imm & 0xfff); }]> {
  let DecoderMethod = "decodeImmOperand<20>";
}

def simm32 : Operand<XLenVT>,
    ImmLeaf<XLenVT, [{ return isInt<32>(Imm); }]>;


//===----------------------------------------------------------------------===//
// Integer arithmetic instructions
//===----------------------------------------------------------------------===//

class ArithLogicR<
    bits<7> opcode,
    bits<3> funct3,
    bits<7> funct7,
    string instr,
    RegisterClass RC
> : RVXInstR<
    opcode,
    funct3,
    funct7,
    (outs RC:$rd),
    (ins RC:$rs1, RC:$rs2),
    !strconcat(instr, "\t$rd, $rs1, $rs2"),
    [],
    IIAlu
> {
  let isReMaterializable = 1;
}

class ArithLogicI<
    bits<7> opcode,
    bits<3> funct3,
    string instr,
    Operand Op,
    RegisterClass RC
> : RVXInstI<
    opcode,
    funct3,
    (outs RC:$rd),
    (ins RC:$rs1, Op:$imm),
    !strconcat(instr, "\t$rd, $rs1, $imm"),
    [],
    IIAlu
> {
  let isReMaterializable = 1;
}

class ArithLogicU<
    bits<7> opcode,
    string instr,
    RegisterClass RC,
    Operand Op
> : RVXInstU<
    opcode,
    (outs RC:$rd),
    (ins Op:$imm),
    !strconcat(instr, "\t$rd, $imm"),
    [],
    IIAlu
> {
  let isReMaterializable = 1;
}


//===----------------------------------------------------------------------===//
// Call sequence nodes
//===----------------------------------------------------------------------===//

def SDT_CallSeqStart :
    SDCallSeqStart<[SDTCisVT<0, XLenVT>,
                    SDTCisVT<1, XLenVT>]>;

def SDT_CallSeqEnd :
    SDCallSeqEnd<[SDTCisVT<0, XLenVT>,
                  SDTCisVT<1, XLenVT>]>;

def CallSeqStart :
    SDNode<"ISD::CALLSEQ_START",
           SDT_CallSeqStart,
           [SDNPHasChain, SDNPOutGlue]>;

def CallSeqEnd :
    SDNode<"ISD::CALLSEQ_END",
           SDT_CallSeqEnd,
           [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;


//===----------------------------------------------------------------------===//
// Call stack adjustment pseudos
//===----------------------------------------------------------------------===//

let Defs = [SP], Uses = [SP], hasSideEffects = 1 in {

  def ADJCALLSTACKDOWN : Pseudo<
      (outs),
      (ins simm12:$amt1, simm12:$amt2),
      "",
      [(CallSeqStart timm:$amt1, timm:$amt2)]
  >;

  def ADJCALLSTACKUP : Pseudo<
      (outs),
      (ins simm12:$amt1, simm12:$amt2),
      "",
      [(CallSeqEnd timm:$amt1, timm:$amt2)]
  >;
}


//===----------------------------------------------------------------------===//
// Integer register-register arithmetic
//===----------------------------------------------------------------------===//

def ADD :
    ArithLogicR<
      0b0110011, 0b000, 0b0000000,
      "add", GPR>;

def SUB :
    ArithLogicR<
      0b0110011, 0b000, 0b0100000,
      "sub", GPR>;

def SLL :
    ArithLogicR<
      0b0110011, 0b001, 0b0000000,
      "sll", GPR>;

def SLT :
    ArithLogicR<
      0b0110011, 0b010, 0b0000000,
      "slt", GPR>;

def SLTU :
    ArithLogicR<
      0b0110011, 0b011, 0b0000000,
      "sltu", GPR>;

def XOR :
    ArithLogicR<
      0b0110011, 0b100, 0b0000000,
      "xor", GPR>;

def SRL :
    ArithLogicR<
      0b0110011, 0b101, 0b0000000,
      "srl", GPR>;

def SRA :
    ArithLogicR<
      0b0110011, 0b101, 0b0100000,
      "sra", GPR>;

def OR :
    ArithLogicR<
      0b0110011, 0b110, 0b0000000,
      "or", GPR>;

def AND :
    ArithLogicR<
      0b0110011, 0b111, 0b0000000,
      "and", GPR>;


//===----------------------------------------------------------------------===//
// Integer register-immediate arithmetic
//===----------------------------------------------------------------------===//

def ADDI :
    ArithLogicI<
      0b0010011, 0b000,
      "addi", simm12, GPR>;

def SLTI :
    ArithLogicI<
      0b0010011, 0b010,
      "slti", simm12, GPR>;

def SLTIU :
    ArithLogicI<
      0b0010011, 0b011,
      "sltiu", simm12, GPR>;

def XORI :
    ArithLogicI<
      0b0010011, 0b100,
      "xori", simm12, GPR>;

def ORI :
    ArithLogicI<
      0b0010011, 0b110,
      "ori", simm12, GPR>;

def ANDI :
    ArithLogicI<
      0b0010011, 0b111,
      "andi", simm12, GPR>;


//===----------------------------------------------------------------------===//
// Shift-immediate instructions
//===----------------------------------------------------------------------===//

def SLLI :
    ArithLogicI<
      0b0010011, 0b001,
      "slli", simm12, GPR>;

def SRLI :
    ArithLogicI<
      0b0010011, 0b101,
      "srli", simm12, GPR>;

def SRAI :
    ArithLogicI<
      0b0010011, 0b101,
      "srai", simm12, GPR>;


//===----------------------------------------------------------------------===//
// Upper-immediate instructions
//===----------------------------------------------------------------------===//

def LUI :
    ArithLogicU<
      0b0110111,
      "lui",
      GPR,
      simm20u>;

def AUIPC :
    ArithLogicU<
      0b0010111,
      "auipc",
      GPR,
      simm20u>;


//===----------------------------------------------------------------------===//
// Loads
//
// These intentionally have empty SelectionDAG patterns for now.
// The instruction records and encodings can be generated independently.
// Proper load patterns will be added once the memory DAG lowering is
// implemented.
//===----------------------------------------------------------------------===//

def LB :
    RVXInstI<
      0b0000011,
      0b000,
      (outs GPR:$rd),
      (ins GPR:$rs1, simm12:$imm),
      "lb\t$rd, $imm($rs1)",
      [],
      IIC_Any>;

def LH :
    RVXInstI<
      0b0000011,
      0b001,
      (outs GPR:$rd),
      (ins GPR:$rs1, simm12:$imm),
      "lh\t$rd, $imm($rs1)",
      [],
      IIC_Any>;

def LW :
    RVXInstI<
      0b0000011,
      0b010,
      (outs GPR:$rd),
      (ins GPR:$rs1, simm12:$imm),
      "lw\t$rd, $imm($rs1)",
      [],
      IIC_Any>;

def LBU :
    RVXInstI<
      0b0000011,
      0b100,
      (outs GPR:$rd),
      (ins GPR:$rs1, simm12:$imm),
      "lbu\t$rd, $imm($rs1)",
      [],
      IIC_Any>;

def LHU :
    RVXInstI<
      0b0000011,
      0b101,
      (outs GPR:$rd),
      (ins GPR:$rs1, simm12:$imm),
      "lhu\t$rd, $imm($rs1)",
      [],
      IIC_Any>;

def LD :
    RVXInstI<
      0b0000011,
      0b011,
      (outs GPR:$rd),
      (ins GPR:$rs1, simm12:$imm),
      "ld\t$rd, $imm($rs1)",
      [],
      IIC_Any>;


//===----------------------------------------------------------------------===//
// Stores
//===----------------------------------------------------------------------===//

def SB :
    RVXInstS<
      0b0100011,
      0b000,
      (outs),
      (ins GPR:$rs1, GPR:$rs2, simm12:$imm),
      "sb\t$rs2, $imm($rs1)",
      [],
      IIC_Any>;

def SH :
    RVXInstS<
      0b0100011,
      0b001,
      (outs),
      (ins GPR:$rs1, GPR:$rs2, simm12:$imm),
      "sh\t$rs2, $imm($rs1)",
      [],
      IIC_Any>;

def SW :
    RVXInstS<
      0b0100011,
      0b010,
      (outs),
      (ins GPR:$rs1, GPR:$rs2, simm12:$imm),
      "sw\t$rs2, $imm($rs1)",
      [],
      IIC_Any>;

def SD :
    RVXInstS<
      0b0100011,
      0b011,
      (outs),
      (ins GPR:$rs1, GPR:$rs2, simm12:$imm),
      "sd\t$rs2, $imm($rs1)",
      [],
      IIC_Any>;


//===----------------------------------------------------------------------===//
// Conditional branches
//===----------------------------------------------------------------------===//

def BEQ :
    RVXInstB<
      0b1100011,
      0b000,
      (outs),
      (ins GPR:$rs1, GPR:$rs2, simm32:$imm),
      "beq\t$rs1, $rs2, $imm",
      [],
      IIC_Any>;

def BNE :
    RVXInstB<
      0b1100011,
      0b001,
      (outs),
      (ins GPR:$rs1, GPR:$rs2, simm32:$imm),
      "bne\t$rs1, $rs2, $imm",
      [],
      IIC_Any>;

def BLT :
    RVXInstB<
      0b1100011,
      0b100,
      (outs),
      (ins GPR:$rs1, GPR:$rs2, simm32:$imm),
      "blt\t$rs1, $rs2, $imm",
      [],
      IIC_Any>;

def BGE :
    RVXInstB<
      0b1100011,
      0b101,
      (outs),
      (ins GPR:$rs1, GPR:$rs2, simm32:$imm),
      "bge\t$rs1, $rs2, $imm",
      [],
      IIC_Any>;

def BLTU :
    RVXInstB<
      0b1100011,
      0b110,
      (outs),
      (ins GPR:$rs1, GPR:$rs2, simm32:$imm),
      "bltu\t$rs1, $rs2, $imm",
      [],
      IIC_Any>;

def BGEU :
    RVXInstB<
      0b1100011,
      0b111,
      (outs),
      (ins GPR:$rs1, GPR:$rs2, simm32:$imm),
      "bgeu\t$rs1, $rs2, $imm",
      [],
      IIC_Any>;


//===----------------------------------------------------------------------===//
// Jumps
//===----------------------------------------------------------------------===//

def JAL :
    RVXInstJ<
      0b1101111,
      (outs GPR:$rd),
      (ins simm32:$imm),
      "jal\t$rd, $imm",
      [],
      IIC_Any>;

def JALR :
    RVXInstI<
      0b1100111,
      0b000,
      (outs GPR:$rd),
      (ins GPR:$rs1, simm12:$imm),
      "jalr\t$rd, $imm($rs1)",
      [],
      IIC_Any>;


//===----------------------------------------------------------------------===//
// Fence
//===----------------------------------------------------------------------===//

def fencearg : Operand<XLenVT> {
  let PrintMethod = "printFenceArg";
  let ParserMatchClass = "FenceArg";
  let EncoderMethod = "getFenceArgOpValue";
  let DecoderMethod = "decodeFenceArg";
  let ParserMethod = "parseFenceArg";
}

def uimm8_fence : Operand<XLenVT>,
    ImmLeaf<XLenVT, [{ return isUInt<8>(Imm); }]> {
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeUImmOperand<8>";
}

def uimm12_fence : Operand<XLenVT>,
    ImmLeaf<XLenVT, [{ return isUInt<12>(Imm); }]> {
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeUImmOperand<12>";
}

def FENCE :
    RVXInstI<
      0b0001111,
      0b000,
      (outs),
      (ins uimm12_fence:$fm),
      "fence\t$fm",
      [],
      IIC_Any
    > {
  let rd = 0;
  let rs1 = 0;
  let hasSideEffects = 1;
  let isBarrier = 1;
  let mayLoad = 1;
  let mayStore = 1;
}
