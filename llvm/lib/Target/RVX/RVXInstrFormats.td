include "llvm/Target/Target.td"

class Format<bits<3> val> {
	bits<3> Value = val;
}

def FormatPseudo : Format<0>;
def FormatR      : Format<1>;
def FormatI      : Format<2>;
def FormatS      : Format<3>;
def FormatU      : Format<4>;
def FormatB      : Format<5>;
def FormatJ      : Format<6>;

//===----------------------------------------------------------------------===//
// Base RVX Instruction
//===----------------------------------------------------------------------===//

class RVXInst<
	dag outs, dag ins, string asmstr,
	list<dag> pattern, InstrItinClass itin, Format format
> : Instruction {

	field bits<32> Inst;

	let Namespace = "RVX";
	let Size = 4;
	let OutOperandList = outs;
	let InOperandList = ins;
	let AsmString = asmstr;
	let Pattern = pattern;
	let Itinerary = itin;
	let TSFlags{2-0} = format.Value;
	let DecoderNamespace = "RVX";
	field bits<32> SoftFail = 0;
}

//===----------------------------------------------------------------------===//
// R-type Instruction
//===----------------------------------------------------------------------===//

class RVXInstR<
	bits<7> opcode, bits<3> funct3, bits<7> funct7,
	dag outs, dag ins, string asmstr,
	list<dag> pattern, InstrItinClass itin
> : RVXInst<outs, ins, asmstr, pattern, itin, FormatR> {

	bits<5> rd;
	bits<5> rs1;
	bits<5> rs2;

	let Inst{31-25} = funct7;
	let Inst{24-20} = rs2;
	let Inst{19-15} = rs1;
	let Inst{14-12} = funct3;
	let Inst{11-7}  = rd;
	let Inst{6-0}   = opcode;
}

//===----------------------------------------------------------------------===//
// I-type Instruction
//===----------------------------------------------------------------------===//

class RVXInstI<
	bits<7> opcode, bits<3> funct3,
	dag outs, dag ins, string asmstr,
	list<dag> pattern, InstrItinClass itin
> : RVXInst<outs, ins, asmstr, pattern, itin, FormatI> {

	bits<5> rd;
	bits<5> rs1;
	bits<12> imm;

	let Inst{31-20} = imm;
	let Inst{19-15} = rs1;
	let Inst{14-12} = funct3;
	let Inst{11-7}  = rd;
	let Inst{6-0}   = opcode;
}

//===----------------------------------------------------------------------===//
// S-type Instruction
//===----------------------------------------------------------------------===//

class RVXInstS<
	bits<7> opcode, bits<3> funct3,
	dag outs, dag ins, string asmstr,
	list<dag> pattern, InstrItinClass itin
> : RVXInst<outs, ins, asmstr, pattern, itin, FormatS> {

	bits<5> rs1;
	bits<5> rs2;
	bits<12> imm;

	let Inst{31-25} = imm{11-5};
	let Inst{24-20} = rs2;
	let Inst{19-15} = rs1;
	let Inst{14-12} = funct3;
	let Inst{11-7}  = imm{4-0};
	let Inst{6-0}   = opcode;
}

//===----------------------------------------------------------------------===//
// B-type Instruction
//===----------------------------------------------------------------------===//

class RVXInstB<
	bits<7> opcode, bits<3> funct3,
	dag outs, dag ins, string asmstr,
	list<dag> pattern, InstrItinClass itin
> : RVXInst<outs, ins, asmstr, pattern, itin, FormatB> {

	bits<5> rs1;
	bits<5> rs2;
	bits<13> imm;

	let Inst{31}    = imm{12};
	let Inst{30-25} = imm{10-5};
	let Inst{24-20} = rs2;
	let Inst{19-15} = rs1;
	let Inst{14-12} = funct3;
	let Inst{11-8}  = imm{4-1};
	let Inst{7}     = imm{11};
	let Inst{6-0}   = opcode;
}

//===----------------------------------------------------------------------===//
// J-type Instruction
//===----------------------------------------------------------------------===//

class RVXInstJ<
	bits<7> opcode,
	dag outs, dag ins, string asmstr,
	list<dag> pattern, InstrItinClass itin
> : RVXInst<outs, ins, asmstr, pattern, itin, FormatJ> {

	bits<5> rd;
	bits<20> imm;

	let Inst{31}     = imm{19};
	let Inst{30-21}  = imm{9-0};
	let Inst{20}     = imm{10};
	let Inst{19-12}  = imm{18-11};
	let Inst{11-7}   = rd;
	let Inst{6-0}    = opcode;
}

//===----------------------------------------------------------------------===//
// U-type Instruction
//===----------------------------------------------------------------------===//

class RVXInstU<
	bits<7> opcode,
	dag outs, dag ins, string asmstr,
	list<dag> pattern, InstrItinClass itin
> : RVXInst<outs, ins, asmstr, pattern, itin, FormatU> {

	bits<5> rd;
	bits<20> imm;

	let Inst{31-12} = imm;
	let Inst{11-7}  = rd;
	let Inst{6-0}   = opcode;
}

//===----------------------------------------------------------------------===//
// Pseudo Instruction
//===----------------------------------------------------------------------===//

def IIPseudo : InstrItinClass;

class Pseudo<
	dag outs, dag ins,
	string asmstr = "", list<dag> pattern = []
> : RVXInst<outs, ins, asmstr, pattern, IIPseudo, FormatPseudo> {
	let isPseudo = 1;
	let isCodeGenOnly = 1;
}
