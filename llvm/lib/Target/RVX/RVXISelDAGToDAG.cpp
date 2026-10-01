#include "RVXISelDAGToDAG.h"
#include "RVX.h"                     // pass creation function declarations
#include "RVXMachineFunctionInfo.h"  // RVX-specific per-function state
#include "RVXRegisterInfo.h"         // register class definitions (GRPRegs etc.)
#include "RVXSubtarget.h"            // feature bit queries
#include "RVXTargetMachine.h"        // RVXTargetMachine definition

#include "llvm/CodeGen/MachineFrameInfo.h"   // frame index queries
#include "llvm/CodeGen/MachineFunction.h"    // MF.getFrameInfo()
#include "llvm/CodeGen/SelectionDAGISel.h"   // SelectionDAGISel base class
#include "llvm/CodeGen/SelectionDAGNodes.h"  // SDNode, SDValue, FrameIndexSDNode
#include "llvm/IR/IntrinsicsRISCV.h"         // RISC-V intrinsic IDs
#include "llvm/Support/Debug.h"              // LLVM_DEBUG, dbgs()
#include "llvm/Support/KnownBits.h"          // for isOrEquivalentToAdd
#include "llvm/Support/MathExtras.h"         // isInt<N>(), isUInt<N>()
#include "llvm/Support/raw_ostream.h"

#include "RVXGenDAGISel.inc"

using namespace llvm; 

#define DEBUG_TYPE "rvx-isel" 

RVXDAGToDAGISel::RVXDAGToDAGISel(RVXTargetMachine &TM,
                                   CodeGenOptLevel OptLevel)
    : SelectionDAGISel(TM, OptLevel) {}


bool RVXDAGToDAGISel::runOnMachineFunction(MachineFunction &MF) {
  Subtarget = &MF.getSubtarget<RVXSubtarget>();

  return SelectionDAGISel::runOnMachineFunction(MF);
}

SDValue RVXDAGToDAGISel::getImm(const SDNode *Node, uint64_t Imm){
    return CurDAG->getTargetConstant(Imm, SDLoc(Node), Subtarget->getXLenVT()); 
}

//Called for every SDNode
//special case are handled here, everything else falls 
//through SelectCode 
void RVXDAGToDAGISel::Select(SDNode *N){
    if(N->isMachineOpcode()){
        N->setNodeId(-1); 
        return; 
    }

    SDLoc DL(N); 
    MVT XLenVT = Subtarget->getXLenVT(); 

    switch(N->getOpcode()){
        
        //integer materialzation 
        case ISD::Constant:{
            auto *ConsNode = cast<ConstantSDNode>(N); 
            int64_t Imm = ConstNode->getSExtValue(); 
            MVT VT = N->getSimpleValueType(0); 

            if(isInt<12>(Imm))
                break; 

            if((Imm & 0xFFF) == 0 && isInt<32>(Imm)){
                SDNode *Result = CurDAG->getMachineNode(
                    RVX::LUI, DL, VT, CurDAG->getTargetConstant(Imm >> 12, DL, VT)); 
                ReplaceNode(N, Result);
                return; 
            }
            if(isInt<32>(Imm)){
                SDNode *Result == selectImm(N, DL, Imm, VT); 
                ReplaceNode(N, Result); 
                return; 
            }

            if (Subtarget->is64Bit()) {
                SDNode *Result = selectImm(N, DL, Imm, VT);
                ReplaceNode(N, Result);
                return;
            }

            break; 
        }

        case ISD::FrameIndex:{
            int FI = cast<FrameIndexSDNode(N)->getIndex(); 

            SDValue TFI = CurDAG->getTargetFrameIndex(
                FI, getTargetLowering()->getPointerTy(CurDAG->getDataLayout());
            
            usigned Opc = Subtarget->is64Bit() ? RVX::ADDI : RVX::ADDI; 

            SDNode *FrameAddr = CurDAG->getMachineNode(
                Opc, DL, XLenVT, 
                TFI, 
                CurDAG->getTargetConstant(0, DL, XLenVT)); 
            ReplaceNode(N, FrameAddr); 
            return; 
        }

        case RVXISD::SELECT_CC:{
            break; 
        }

        case ISD::LOAD:{
            break; 
        }

        default:
            break; 
    }

    SelectCode(N); 
}

SDNode *RVXDAGToDAGISel::selectImm(SDNode *N, const SDLoc &DL, 
                                   int64_t Imm, MVT VT){
    if(isInt<12>(Imm)){
        return CurDAG->getMachineNode(RVX::ADDI, DL, VT, 
                                      CurDAG->getRegister(RVX::X0, VT), 
                                      CurDAG->getTargetConstant(Imm, DL, VT)); 
    }

    if (isInt<32>(Imm)) {
        // Build LUI first.
        SDNode *LUIOp = CurDAG->getMachineNode(
        RVX::LUI, DL, VT,
        CurDAG->getTargetConstant(Hi, DL, VT)); // imm = hi20
    
        // If lo12 is 0, LUI alone is sufficient.
        if (Lo == 0)
            return LUIOp;

        // Build ADDI using the LUI result as rs1.
        return CurDAG->getMachineNode(
            RVX::ADDI, DL, VT,
            SDValue(LUIOp, 0),                        // rs1 = result of LUI
            CurDAG->getTargetConstant(Lo, DL, VT));   // imm = lo12
    }

    assert(Subtarget->is64Bit() && "64-bit constant on non-RV64 target?");

    int64_t Lo32 = SignExtend64<32>(Imm & 0xFFFFFFFF); 
    int64_t hi32 = (Imm >> 32) && 0xFFFFFFFF; 

    SDNode *Result = nullptr; 

    if (Hi32 != 0) {
        // Materialise the upper 32 bits using the 32-bit strategy above.
        // On RV64, LUI+ADDI sign-extends to 64 bits, so we treat Hi32 as
        // a sign-extended 32-bit value that occupies bits [63:32] after SLLI.
        int64_t HiHi = ((Hi32 + 0x800) >> 12) & 0xFFFFF;
        int64_t HiLo = Hi32 - (HiHi << 12);

        // LUI upper_reg, HiHi — loads bits [31:12] of Hi32
        SDNode *UpperLUI = CurDAG->getMachineNode(
            RVX::LUI, DL, MVT::i64,
            CurDAG->getTargetConstant(HiHi, DL, MVT::i64));

        SDNode *UpperVal = UpperLUI;
        if (HiLo != 0) {
            // ADDI upper_reg, upper_reg, HiLo — completes Hi32
            UpperVal = CurDAG->getMachineNode(
                RVX::ADDI, DL, MVT::i64,
                SDValue(UpperLUI, 0),
                CurDAG->getTargetConstant(HiLo, DL, MVT::i64));
        }

        // SLLI upper_reg, upper_reg, 32 — shift left by 32 to reach bits [63:32]
        Result = CurDAG->getMachineNode(
            RVX::SLLI, DL, MVT::i64,
            SDValue(UpperVal, 0),
            CurDAG->getTargetConstant(32, DL, MVT::i64));
    }

    if (Lo32 != 0) {
        // Materialise the lower 32 bits.
        SDNode *LowerVal;
        if (isInt<12>(Lo32)) {
        // Small lower half: ADDI lower_reg, x0, Lo32
        LowerVal = CurDAG->getMachineNode(
            RVX::ADDI, DL, MVT::i64,
            CurDAG->getRegister(RVX::X0, MVT::i64),
            CurDAG->getTargetConstant(Lo32, DL, MVT::i64));
        } else {
        // Larger lower half: LUI + ADDI for lower 32 bits
        int64_t LoHi = ((Lo32 + 0x800) >> 12) & 0xFFFFF;
        int64_t LoLo = Lo32 - (LoHi << 12);
        SDNode *LoLUI = CurDAG->getMachineNode(
            RVX::LUI, DL, MVT::i64,
            CurDAG->getTargetConstant(LoHi, DL, MVT::i64));
        LowerVal = CurDAG->getMachineNode(
            RVX::ADDI, DL, MVT::i64,
            SDValue(LoLUI, 0),
            CurDAG->getTargetConstant(LoLo, DL, MVT::i64));
        }

        if (Result) {
            // Combine upper and lower halves with ADD (OR would also work for
            // non-overlapping bits, but ADD is simpler and always correct).
            Result = CurDAG->getMachineNode(
                RVX::ADD, DL, MVT::i64,
                SDValue(Result, 0),
                SDValue(LowerVal, 0));
        } else {
            // No upper half — lower half IS the result.
            Result = LowerVal;
        }
    }

    // Edge case: Imm == 0 (both halves zero).
    // Return ADDI x0, x0, 0 which equals 0.
    if (!Result) {
        Result = CurDAG->getMachineNode(
            RVX::ADDI, DL, MVT::i64,
            CurDAG->getRegister(RVX::X0, MVT::i64),
            CurDAG->getTargetConstant(0, DL, MVT::i64));
    }
    return Result; 
}

bool RVXDAGToDAGISel::SelectAddrRegImm(SDValue Addr, 
                                       SDValue &Base, 
                                       SDValue &Offset)
{
    SDLoc DL(Addr);
    MVT XLenVT = Subtarget->getXLenVT();

     // (frameindex N)  →  Base = FI, Offset = 0
  // The frame index is passed directly as the base. eliminateFrameIndex()
  // will later replace it with (sp + real_offset).
    if (auto *FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
        Base = CurDAG->getTargetFrameIndex(
            FIN->getIndex(),
            getTargetLowering()->getPointerTy(CurDAG->getDataLayout()));
        Offset = CurDAG->getTargetConstant(0, DL, XLenVT);
        return true;
    }

    if (Addr.getOpcode() == ISD::ADD) {
        SDValue LHS = Addr.getOperand(0);
        SDValue RHS = Addr.getOperand(1);

        // Check if RHS is a compile-time constant that fits in 12 bits.
        if (auto *CN = dyn_cast<ConstantSDNode>(RHS)) {
            int64_t CVal = CN->getSExtValue();
            if (isInt<12>(CVal)) {
                // Match: base = LHS, offset = CVal
                Base = LHS;
                Offset = CurDAG->getTargetConstant(CVal, DL, XLenVT);
                return true;
            }
        }
    }

    if (auto *CN = dyn_cast<ConstantSDNode>(LHS)) {
      int64_t CVal = CN->getSExtValue();
      if (isInt<12>(CVal)) {
        Base = RHS;
        Offset = CurDAG->getTargetConstant(CVal, DL, XLenVT);
        return true;
      }
    }


    if (auto *FIN = dyn_cast<FrameIndexSDNode>(LHS)) {
        if (auto *CN = dyn_cast<ConstantSDNode>(RHS)) {
            int64_t CVal = CN->getSExtValue();
            if (isInt<12>(CVal)) {
                Base = CurDAG->getTargetFrameIndex(
                    FIN->getIndex(),
                    getTargetLowering()->getPointerTy(CurDAG->getDataLayout()));
                Offset = CurDAG->getTargetConstant(CVal, DL, XLenVT);
                return true;
            }
        }
    }

      if (Addr.getOpcode() == ISD::OR && isOrEquivalentToAdd(Addr.getNode())) {
    SDValue LHS = Addr.getOperand(0);
    SDValue RHS = Addr.getOperand(1);
    if (auto *CN = dyn_cast<ConstantSDNode>(RHS)) {
      int64_t CVal = CN->getSExtValue();
      if (isInt<12>(CVal)) {
        Base = LHS;
        Offset = CurDAG->getTargetConstant(CVal, DL, XLenVT);
        return true;
      }
    }
  }

  Base = Addr;
  Offset = CurDAG->getTargetConstant(0, DL, XLenVT);
  return true

}

bool RVXDAGToDAGISel::SelectAddrFrameIndex(SDValue Addr,
                                            SDValue &Base,
                                            SDValue &Offset) {
  SDLoc DL(Addr);
  MVT XLenVT = Subtarget->getXLenVT();

  if (auto *FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
    Base = CurDAG->getTargetFrameIndex(
        FIN->getIndex(),
        getTargetLowering()->getPointerTy(CurDAG->getDataLayout()));
    Offset = CurDAG->getTargetConstant(0, DL, XLenVT);
    return true;
  }
  return false; 
}

bool RVXDAGToDAGISel::SelectInlineAsmMemoryOperand(
    const SDValue &Op,
    InlineAsm::ConstraintCode ConstraintCode,
    std::vector<SDValue> &OutOps) {
  SDValue Base, Offset;

  switch (ConstraintCode) {
  case InlineAsm::ConstraintCode::m:
    // Generic memory constraint: use full (base + imm12) addressing.
    if (!SelectAddrRegImm(Op, Base, Offset))
      return true; // failed to match — report error
    OutOps.push_back(Base);
    OutOps.push_back(Offset);
    return false; // success

  case InlineAsm::ConstraintCode::A:
    // Atomic memory constraint: RISC-V atomic instructions (LR/SC/AMO)
    // take only a base register with no offset field. We must ensure
    // the address is in a register with no folded offset.
    //
    // For "A", just pass the address as-is (it must already be a register).
    // If it's an ADD or complex expression, SelectAddrRegImm would give us
    // (reg, offset), but we can only use (reg, 0) for atomics. So we
    // ignore the offset and just take the raw address in a register.
    OutOps.push_back(Op);  // just the address register, no offset
    return false; // success

  default:
    // Unknown constraint code — report failure. The inline assembler will
    // produce a diagnostic.
    return true;
  }
}

bool RVXDAGToDAGISel::hasAllHUsers(SDNode *N) const {
  for (SDNode *User : N->uses()) {
    (void)User;
    return false;
  }
  return true;
}

bool RVXDAGToDAGISel::isOrEquivalentToAdd(const SDNode *N) const {
  assert(N->getOpcode() == ISD::OR && "Expected OR node");

  // KnownBits tracks which bits of an SDValue are known to be 0 or 1.
  // For OR to be equivalent to ADD, no bit position can have a known-1
  // in BOTH operands (otherwise OR and ADD would give different results
  // for that bit due to the carry in ADD).
  KnownBits Known0 =
      CurDAG->computeKnownBits(N->getOperand(0), /*Depth=*/0);
  KnownBits Known1 =
      CurDAG->computeKnownBits(N->getOperand(1), /*Depth=*/0);

  // If (Known0.One & Known1.One) == 0, no bit is 1 in both operands,
  // so OR and ADD produce the same result (no carries in the addition).
  return (Known0.One & Known1.One).isZero();
}

FunctionPass *llvm::createRVXISelDag(RVXTargetMachine &TM,
                                      CodeGenOptLevel OptLevel) {
  return new RVXDAGToDAGISel(TM, OptLevel);
}


