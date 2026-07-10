#include "RVXInstrInfo.h"
#include "RVXSubtarget.h"
#include "RVXMachineFunctionInfo.h"

#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h" 
#include "llvm/CodeGen/MachineMemOperand.h"   
#include "llvm/Support/ErrorHandling.h"

#define GET_INSTRINFO_CTOR_DTOR
#include "RVXGenInstrInfo.inc"

using namespace llvm; 

RVXInstrInfo::RVXInstrInfo(const RVXSubtarget &STI) 
    :   RVXGenInstrInfo(RVX::ADJCALLSTACKDOWN, 
                      RVX::ADJCALLSTACKUP), 
        RI(STI), 
        STI(STI) {}


void RVXInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator MI, 
                               const DebugLoc &DL,
                               MCRegister DstReg, 
                               MCRegister SrcReg,
                               bool IsKill) const{

    if(DstReg == SrcReg)
        return; 

    if(RVX::GPRRegClass.contains(DstReg, SrcReg)){
        BuildMI(MBB, MI, DL, get(RVX::ADDI), DstReg)
            .addReg(SrcReg, getKillRegState(IsKill))
            .addImm(0); 
        return; 
    }

    //fp register to pf regsiter 
    //we use FSGNJ, dst, src, src 
    if(RVX::FPR32RegClass.contains(DstReg, SrcReg)){
        BuildMI(MBB, MI, DL, get(RVX::FSGNJ_S), DstReg)
            .addReg(SrcReg, getKillRegState(IsKill))
            .addReg(SrcReg, getKillRegState(IsKill));
        return; 
    }

    if(RVX::FPR64RegClass.contains(DstReg, SrcReg)){
        BuildMI(MBB, MI, DL, get(RVX::FSGNJ_D), DstReg)
            .addReg(SrcReg, getKillRegState(IsKill))
            .addReg(SrcReg, getKillRegState(IsKill)); 
        return; 
    }

    if (RVX::GRPRegsRegClass.contains(DstReg) &&
        RVX::FPR32RegClass.contains(SrcReg)) {
    
        // f-reg → x-reg (single-precision): FMV.X.W dst, src
        BuildMI(MBB, MI, DL, get(RVX::FMV_X_W), DstReg)
            .addReg(SrcReg, getKillRegState(IsKill));
        return;
    }

    if (RVX::FPR32RegClass.contains(DstReg) &&
        RVX::GRPRegsRegClass.contains(SrcReg)) {
        // x-reg → f-reg (single-precision): FMV.W.X dst, src
        BuildMI(MBB, MI, DL, get(RVX::FMV_W_X), DstReg)
            .addReg(SrcReg, getKillRegState(IsKill));
        return;
    }

    llvm_unreachable("RVXInstrInfo::copyPhysReg: unhandled register class "
                   "combination. Add a case for the register classes "
                   "of DstReg and SrcReg.");
}

void RVXInstrInfo::storeRegToStackSlot(MachineBasicBlock &MBB, 
                                       MachineBasicBlock::iterator MI, 
                                       Register SrcReg, bool IsKill, 
                                       int FI, 
                                       const TargetRegisterClass *RC, 
                                       const TargetRegisterInfo *TRI, 
                                       Register VReg) const{

    DebugLoc DL; 

    MachineFunction *MF = MBB->getParent(); 
    MachineFrameInfo &MFI = MF->getFrameInfo(); 

    MachineMemOperand *MMO = MF->getMachineMemOperand(
        MachinePointerInfo::getFixedStack(*MF, FI), 
        MFI.getObjectSize(FI), 
        MFI.getObjectAlign(FI)); 
    ); 

    if(RVX::GPRRegsClass.hasSubClassEq(RC)){

        unsigned Opc = STI.is64Bit() ? RVX::SD : RVX::SW; 
        BuildMI(MBB, MI, DL, get(Opc))
            .addReg(SrcReg, getKillRegState(IsKill))
            .addFrameIndex(FI)
            .addImm(0)
            .addMemOperand(MMO);  

    } else if (RVX::FPR32RegClass.hasSubClassEq(RC)) {
        // Single-precision float spill (F extension): FSW
        BuildMI(MBB, MI, DL, get(RVX::FSW))
            .addReg(SrcReg, getKillRegState(IsKill))
            .addFrameIndex(FI)
            .addImm(0)
            .addMemOperand(MMO);
    } else if (RVX::FPR64RegClass.hasSubClassEq(RC)) {
        // Double-precision float spill (D extension): FSD
        BuildMI(MBB, MI, DL, get(RVX::FSD))
            .addReg(SrcReg, getKillRegState(IsKill))
            . addFrameIndex(FI)
            .addImm(0)
            .addMemOperand(MMO);
    } else {
        llvm_unreachable("RVXInstrInfo::storeRegToStackSlot: unsupported "
                     "register class — add a case for this RC.");
    }
}

// Reloads DstReg from stack slot FI. Mirror image of storeRegToStackSlot.
void RVXInstrInfo::loadRegFromStackSlot(MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator MI,
                                         Register DstReg, int FI,
                                         const TargetRegisterClass *RC,
                                         const TargetRegisterInfo *TRI,
                                         Register VReg) const {
    DebugLoc DL;
    if (MI != MBB.end())
        DL = MI->getDebugLoc();

    MachineFunction *MF = MBB.getParent();
    MachineFrameInfo &MFI = MF->getFrameInfo();

    MachineMemOperand *MMO = MF->getMachineMemOperand(
        MachinePointerInfo::getFixedStack(*MF, FI),
        MachineMemOperand::MOLoad,
        MFI.getObjectSize(FI),
        MFI.getObjectAlign(FI));

    if (RVX::GRPRegsRegClass.hasSubClassEq(RC)) {
        unsigned Opc = STI.is64Bit() ? RVX::LD : RVX::LW;
        BuildMI(MBB, MI, DL, get(Opc), DstReg)
            .addFrameIndex(FI)
            .addImm(0)
            .addMemOperand(MMO);
    } else if (RVX::FPR32RegClass.hasSubClassEq(RC)) {
        BuildMI(MBB, MI, DL, get(RVX::FLW), DstReg)
            .addFrameIndex(FI)
            .addImm(0)
            .addMemOperand(MMO);
    } else if (RVX::FPR64RegClass.hasSubClassEq(RC)) {
        BuildMI(MBB, MI, DL, get(RVX::FLD), DstReg)
            .addFrameIndex(FI)
            .addImm(0)
            .addMemOperand(MMO);
    } else {
        llvm_unreachable("RVXInstrInfo::loadRegFromStackSlot: unsupported "
                     "register class.");
  }
}

bool RVXInstrInfo::analyzeBranch(MachineBasicBlock &MBB, 
                                 MachineBasicBlock *&TBB, 
                                 MachineBasicBlock *&FBB, 
                                 SmallVector<MachineOperand> &Cond, 
                                 bool AllowModify) const {

    MachineBasicBlock::iterator I = MBB.getNonDebugInstr(); 
    if(I == MBB.end())
        return false; 

    if(!I->isBranch())
        return false;

    MachineBasicBlock::iterator LastBranch = I; 
    MachineBasicBlock::iterator SecondLastBranch = MBB.end(); 

    //check if there are two branches the end 
    if(I != MBB.begin()){
        --I; 
        if(I->isUnconditionalBranch() || I->isConditionalBranch())
            SecondLastBranch = I; 
    }

    MachineBasicBlock::iterator Branch = (SecondLastBranch |= MBB.end() ? SecondLastBranch : LastBranch); 

    unsigned Opc = Branch->getOpcode(); 

    //we only handling conditional and unconditional branches. not indirect
    
   // unconditional branch (JAL, x0, target)
    if(Opc == RVX::JAL && Branch->getOperand(0).getReg() == RVX:X0){
        TBB = Branch->getOperand(1).get(1).getMBB(); 
        if(AllowModify){
            if(MBB.isLayoutSuccessor(TBB)){
                Branch->eraseFromParent(); 
                TBB = nullptr; 
            }
        }
        return false 
    }

    if(Branch->isConditionalBranch()){

        TBB = Branch->getOperand(Branch->getNumOperands()-1).getMBB(); 

        //record condition for potential reversal or re-insertion 
        // Convention: Cond[0] = the opcode of the branch, Cond[1] = rs1, Cond[2] = rs2.
        Cond.push_back(MachineOperand::CreateImm(Opc));
        for (unsigned i = 0; i < Branch->getNumOperands() - 1; ++i)
            Cond.push_back(Branch->getOperand(i));

        // If there was a second branch (the unconditional one), decode its target.
        if (SecondLastBranch != MBB.end())
            FBB = LastBranch->getOperand(LastBranch->getNumOperands() - 1).getMBB();

        return false;
    }

    return true; 
}

//Inser branch instructon at the end of MBB 
unsigned RVXInstrInfo::insertBranch(MachineBasicBlock &MBB, 
                                    MachineBasicBlock *TBB, 
                                    MachineBasicBlock *FBB, 
                                    ArrayRef<MachineOperand> Cond, 
                                    const DebugLoc &DL, 
                                    int *BytesAdded) const{


    assert(!BytesAdded && "instructon sized not implemented"); 
    assert(TBB && "insertBranch must not be tol to inseert a fallthrough"); 

    if(Cond.empty()){
        //unconditional branch 
        BuildMI(&MBB, DL, get(RVX::JAL))
            .addReg(RVX::X0)
            .addMBB(TBB); 
        return 1; 
    }

    assert(Cond.size() == 3 && "Expected [opcode, rs1, rs2] in Cond"); 
    unsigned Opc = Cond[0].getImm(); 
    BuildMI(&MBB, DL, get(Opc))
        .add(Cond[1])
        .add(Cond[2])
        .addMBB(TBB); 

    if(FBB){
        //insert uncoditonal branch to FBB 
        BuildMI(&MBB, DL, get(RVX::JAL))
            .addReg(RVX:X0)
            .addReg(FBB);
        return 2; 
    }

    return 1; 
}

unsigned RVXInstrInfo::removeBranch(MachineBasicBlock &MBB,
                                     int *BytesRemoved) const {
    assert(!BytesRemoved && "instruction sizes not implemented");

    MachineBasicBlock::iterator I = MBB.getLastNonDebugInstr();
    
    if (I == MBB.end())
        return 0;

     unsigned Count = 0;
     // Remove up to two branch instructions from the end.
    while (I->isBranch()) {
        I->eraseFromParent();
        ++Count;
        I = MBB.getLastNonDebugInstr();
        if (I == MBB.end())
            break;
    }
    return Count;
}
// Inverts the branch condition stored in Cond.
// Cond[0] holds the RISC-V branch opcode; we swap it for its logical inverse.
bool RVXInstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert(Cond.size() == 3 && "Expected [opcode, rs1, rs2]");

  // Map each branch opcode to its logical inverse.
  // The inverse of BEQ is BNE (jump if NOT equal).
  // The inverse of BLT is BGE (jump if NOT less than = greater or equal).
  // etc.
  switch (Cond[0].getImm()) {
  case RVX::BEQ:  Cond[0].setImm(RVX::BNE);  return false;
  case RVX::BNE:  Cond[0].setImm(RVX::BEQ);  return false;
  case RVX::BLT:  Cond[0].setImm(RVX::BGE);  return false;
  case RVX::BGE:  Cond[0].setImm(RVX::BLT);  return false;
  case RVX::BLTU: Cond[0].setImm(RVX::BGEU); return false;
  case RVX::BGEU: Cond[0].setImm(RVX::BLTU); return false;
  default:
    // Unknown opcode — cannot reverse.
    return true;
  }
}

bool RVXInstrInfo::isAsCheapAsAMove(const MachineInstr &MI) const {
    // ADDI rd, rs, 0 is the canonical RISC-V copy (MV alias).
    // ORI and SLLI with immediate 0 are also effective no-ops / copies.
    switch (MI.getOpcode()) {
        case RVX::ADDI:
        case RVX::ORI:
        case RVX::XORI:
            // These are copies when the immediate is 0.
            return MI.getOperand(2).isImm() && MI.getOperand(2).getImm() == 0;
        default:
            return false;
    }
}

