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

