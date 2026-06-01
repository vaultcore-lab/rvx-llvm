include "RVXInstrFormats.td"
include "RVXRegisterInfo.td"

def simm12 : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isInt<12>(Imm);}]>{
    let DecoderMethod = "decodeImmOperand<12>"; 
}

def simm20u : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isInt<32>(Imm) & !(Imm & 0xfff);}]>{
        let DecoderMethod = "decodeImmOperand<20>"
    }
def simm32: Operand<XLenVT>, ImmLeaf<XLenVT, [{return isInt<32>(Imm);}]>;

class ArithLogicR< 
    bits<7> opcode, bits<3> funct3, bits<7> funct7, 
    string instr, SDNode OpNode, RegisterClass RC>
        : RVXInstR< opcode, funct3, funct7, (outs RC:$rd), (ins RC:$rs1, RC:$rs2), 
        !strconcat(instr, "\t$rd, $rs1, $rs2"), 
        [(set RC:$rd, (OpNode RC:$rs1, $rs2))], 
        IIAlu
        >{
        let isReMaterializable = 1; 
}

class ArithLogicI<
    bits<7> opcode, bits<3> funct3, string instr, SDNode OpNode, Operand Op, RegisterClass RC
    >: RVXInstI<
    opcode, funct3, (outs RC:$rd), (ins RC:$rs1, Op:$imm),
    !strconcat(instr, "\t$rd, $rs1, $imm"),
    [(set RC:$rd, (OpNode RC:$rs1, Op:$imm))],
    IIAlu
    > {
    let isReMaterializable = 1;
}
class ArithLogicU<
     bits<7> opcode, string instr, RegisterClass RC, Operand Op
    >: RVXInstU<
    opcode, (outs RC:$rd), (ins Op:$imm),
    !strconcat(instr, "\t$rd, $imm"),
    [], IIAlu
    > {
    let isReMaterializable = 1;
}

def SDT_CallSeqStart : SDCallSeqStart<[SDTCisVT<0, XLenVT>, SDTCisVT<1, XLenVT>]>;
def SDT_CallSeqEnd   : SDCallSeqEnd  <[SDTCisVT<0, XLenVT>, SDTCisVT<1, XLenVT>]>;
def CallSeqStart : SDNode<"ISD::CALLSEQ_START", SDT_CallSeqStart, [SDNPHasChain, SDNPOutGlue]>;
def CallSeqEnd   : SDNode<"ISD::CALLSEQ_END",   SDT_CallSeqEnd,   [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;

let Defs = [SP], Uses = [SP], hasSideEffects = 1 in {
    def ADJCALLSTACKDOWN : Pseudo<
    (outs), 
    (ins simm12::$amt1, simm12:$amt2),
    "",
    [(CallSeqStart timm:$amt1), timm:$amt2)]>;
    def ADJCALLSTACKUP   : Pseudo<
    (outs), 
    (ins simm12:$amt1, simm12:$amt2), 
    "", 
    [(CallSeqEnd timm:$amt1, timm:$amt2)]>; 
}

def ADD  : ArithLogicR<0b0110011, 0b000, 0b0000000, "add", add, GPR>;
def SUB  : ArithLogicR<0b0110011, 0b000, 0b0100000, "sub", sub, GPR>;
def SLL  : ArithLogicR<0b0110011, 0b001, 0b0000000, "sll", shl, GPR>;
def SLT  : ArithLogicR<0b0110011, 0b010, 0b0000000, "slt", setcc, GPR>;
def SLTU : ArithLogicR<0b0110011, 0b011, 0b0000000, "sltu", setcc, GPR>;
def XOR  : ArithLogicR<0b0110011, 0b100, 0b0000000, "xor", xor, GPR>;
def SRL  : ArithLogicR<0b0110011, 0b101, 0b0000000, "srl", lshr, GPR>;
def SRA  : ArithLogicR<0b0110011, 0b101, 0b0100000, "sra", ashr, GPR>;
def OR   : ArithLogicR<0b0110011, 0b110, 0b0000000, "or",  or_, GPR>;
def AND  : ArithLogicR<0b0110011, 0b111, 0b0000000, "and", and_, GPR>;

def ADDI  : ArithLogicI<0b0010011, 0b000, "addi", add, simm12, GPR>;
def SLTI  : ArithLogicI<0b0010011, 0b010, "slti", setcc, simm12, GPR>;
def SLTIU : ArithLogicI<0b0010011, 0b011, "sltiu", setcc, simm12, GPR>;
def XORI  : ArithLogicI<0b0010011, 0b100, "xori", xor, simm12, GPR>;
def ORI   : ArithLogicI<0b0010011, 0b110, "ori",  or_, simm12, GPR>;
def ANDI  : ArithLogicI<0b0010011, 0b111, "andi", and_, simm12, GPR>;

def SLLI  : ArithLogicI<0b0010011, 0b001, "slli", shl, simm12, GPR>;
def SRLI  : ArithLogicI<0b0010011, 0b101, "srli", lshr, simm12, GPR>;
def SRAI  : ArithLogicI<0b0010011, 0b101, "srai", ashr, simm12, GPR>;

def LUI   : ArithLogicU<0b0110111, "lui",  GPR, simm20u>;
def AUIPC : ArithLogicU<0b0010111, "auipc",GPR, simm20u>;

def LB  : RVXInstI<0b0000011, 0b000, outs, ins, "lb", [load], IIC_Any>;
def LH  : RVXInstI<0b0000011, 0b001, outs, ins, "lh", [load], IIC_Any>;
def LW  : RVXInstI<0b0000011, 0b010, outs, ins, "lw", [load], IIC_Any>;
def LBU : RVXInstI<0b0000011, 0b100, outs, ins, "lbu", [load], IIC_Any>;
def LHU : RVXInstI<0b0000011, 0b101, outs, ins, "lhu", [load], IIC_Any>;
def LD  : RVXInstI<0b0000011, 0b011, outs, ins, "ld", [load], IIC_Any>;

def SB : RVXInstS<0b0100011, 0b000, outs, ins, "sb", [store], IIC_Any>;
def SH : RVXInstS<0b0100011, 0b001, outs, ins, "sh", [store], IIC_Any>;
def SW : RVXInstS<0b0100011, 0b010, outs, ins, "sw", [store], IIC_Any>;
def SD : RVXInstS<0b0100011, 0b011, outs, ins, "sd", [store], IIC_Any>;

def BEQ  : RVXInstB<0b1100011, 0b000, outs, ins, "beq", [branch], IIC_Any>;
def BNE  : RVXInstB<0b1100011, 0b001, outs, ins, "bne", [branch], IIC_Any>;
def BLT  : RVXInstB<0b1100011, 0b100, outs, ins, "blt", [branch], IIC_Any>;
def BGE  : RVXInstB<0b1100011, 0b101, outs, ins, "bge", [branch], IIC_Any>;
def BLTU : RVXInstB<0b1100011, 0b110, outs, ins, "bltu", [branch], IIC_Any>;
def BGEU : RVXInstB<0b1100011, 0b111, outs, ins, "bgeu", [branch], IIC_Any>;

def JAL  : RVXInstJ<0b1101111, outs, ins, "jal", [jump], IIC_Any>;
def JALR : RVXInstI<0b1100111, 0b000, outs, ins, "jalr", [jump], IIC_Any>;

def fencearg : Operand<XLenVT> {
    let PrintMethod      = "printFenceArg";   
    let ParserMatchClass = "FenceArg";           
    let EncoderMethod    = "getFenceArgOpValue"; 
    let DecoderMethod    = "decodeFenceArg";     
    let ParserMethod     = "parseFenceArg";   
}

def uimm8_fence : Operand<XLenVT>, ImmLeaf<XLenVT, [{ return isUInt<8>(Imm); }]> {
    let EncoderMethod = "getImmOpValue";
    let DecoderMethod = "decodeUImmOperand<8>";
}

def uimm12_fence : Operand<XLenVT>,
    ImmLeaf<XLenVT, [{return isUInt<12>(Imm);}]> {
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeUImmOperand<12>";
}

def FENCE : RVXInstI<0b0001111, 0b000,
	(), (ins uimm12_fence:$fm),
	"fence\t$fm", [], NoItin> {
	let rd  = 0;
	let rs1 = 0;
	let hasSideEffects = 1;
	let isBarrier      = 1;
	let mayLoad        = 1;
	let mayStore       = 1;
}

def RVXAsmWriter : AsmWriter{
    let AsmWriterClassName = "RVXInsrPrinter";     
}
