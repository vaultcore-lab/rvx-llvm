#include "RVXRegisterInfo.h"
#include "RVXFrameLowering.h"
#include "RVXInstrInfo.h"   
#include "RVXMachineFunctionInfo.h" 
#include "RVXSubtarget.h"       
#include "Subtarget.h"

#include "llvm/CodeGen/MachineFrameInfo.h"   
#include "llvm/CodeGen/MachineFunction.h"    
#include "llvm/CodeGen/MachineInstrBuilder.h" 
#include "llvm/CodeGen/RegisterScavenging.h" 
#include "llvm/Support/ErrorHandling.h"
#include <cstdint>
#include <llvm-14/llvm/ADT/BitVector.h>
#include <llvm-14/llvm/IR/CallingConv.h>
#include <llvm-14/llvm/MC/MCRegister.h>

#define GET_REGINFO_TARGET_DESC
#include "RVXGenRegisterInfo.inc"

using namespace llvm;

RegisterInfo::RVXRegisterInfo(const RVXSubtarget &ST)
    : RVXGenRegisterInfo(RVX:X1){}

const MCPhysReg *
RVXRegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const
{
    const RVXSubtarget &STI = MF->getSubtarget<RVXSubtarget>(); 

    switch(MF->getFunction().getCallingConv()){
        default:
        
            if(STI.hasStdExtD() || STI.hasStdExtF())
                return CSR_ILP32_LP64F_SaveList; 
        
            return CSR_ILP32_LP64_SaveList; 
        case CallingConv::GHC: 
            return CSR_NoRegs_SaveList;    
    }
}

const uint32_t * 
RVXRegisterInfo::getCallPreservedMask(const MachineFunction &MF, 
                                      CallingConv::ID CC) const {
    const RVXSubtarget &STI = MF.getSubtarget<RVXSubtarget>(); 

    switch(CC){
        default:
            if(STI.hasStdExtD() || STI.hasStdExtF())
                return CSR_ILP32F_LP64F_RegMask; 
            return CSR_ILP32_LP32_RegMask; 
        
        case CallingConv::GHC:
            return CSR_NoRegs_RegMask; 
    }
}

/*unavaibale for allocations*/ 
BitVector 
RVXRegisterInfo::getReservedRegs(const MachineFunction &MF) const{

    const RVXSubtarget &STI = MF.getSubtarget<RVXSubtarget>(); 
    const RVXFrameLowering *TFI = getFrameLowering(MF); 
    
    /*BitVector size of total physical registers */ 
    BitVector Reserved(getNumRegs());

    /*reserved registers*/ 

    //zero register 
    markSuperRegs(Reserved, RVX::X0); 

    //stack pointer */ 
    markSuperRegs(Reserved, RVX::X2); 
    
    //global pointer 
    markSuperRegsR(Reserved, RVX::X3);

    //thread pointer 
    markSuperRegs(Reserved, RVX::X4);

    if(TFI->hadFP(MF))
        markSuperRegs(Reserved, RVX::X8); 
    
    assert(checkAllSuperRegsMarked(Reserved)); 

    return Reserved; 
}

/*eliminare frame index*/ 

void RVXRegisterInfo::eliminateFramIndex(MachineBasicBlock::iterator II, 
                                         int SPAdj, 
                                         unsigned FIOperandNum, 

                                         RegisterScavenger *RS) const{

    assert(SPAdj == 0 && "Unexpected SP adjustment in RVX frame index elimination"); 

    MachineInstr &MI = *II; 
    MachineFunction &MF = *MI.getParent()->getParent; 
    MachineFrameInfo &MFI = MF.getFrameInfo(); 
    const RVXSubtarget &STI = MF.getSubtarget<RVXSubtarget>(); 
    const RVXFrameLowering *TFI = 
        static_cast<const RVXFrameLowering *>(STI.getFrameLowering()); 
    const DebugLoc &DL = MI.getDebugLoc(); 

    int FrameIndex = MI.getOperand(FIOperandNum).getIndex; 

    Regsiter FrameReg; 
    int Offset; 

    if(TFI->hasFP(MF)){

        //if using frame pointer, base is fp 
        FrameReg = RVX::X8; 
        Offset = MFI.getObjectOffset(FrameIndex) - 
            TFI->getOffsetLocalArea() + 
            MFI.getOffsetAdjustment(); 
    }else{
        //using stack pointer as base
        FrameReg = RVX::X2; 
        Offset = MFI.getObjectOffset(FrameIndex) + MFI.getStackSize();
    }

    //patch base register 
    MI.getOperand(FIOperandNum).ChangeToRegister(FrameReg, /*isDef = false*/); 

    //patch Offset
    
    //offset fits in 12 bits 
    if(isInt<12>(Offset)){
        MI.getOperand(FIOperandNum -1).ChangeToImmediate(Offset); 
        return; 
    }

    //offset does not fit in 12 bits 

    assert(RS && "Register scavenger not available for large frame offset. "
               "requiresRegisterScavenging()?");


    //need to scavenge a free register 

    const TargetRegisterClass *RC = &RVX::GPRRegsRegClass; 
    Register ScratchReg = RS->scavengeRegister(RC, II, SPAdj); 

    assert(ScratchReg && "No register available for frame index elimination. "
                       "Increase the number of emergency spill slots or "
                       "reduce the frame size.");


    if(isInt<12?(Offset)){

        BuildMI(*MI, getParent(), II, DL, TII->get(RVX::ADDI), ScratchReg)
            .addImm(offset); 
    }else{

        int64_t Hi = ((int64_t)Offset + 0x800) >> 12; 
        int64_t Lo = Offset - (HI << 12); 


        // LUI scratch, Hi   — loads Hi into scratch[31:12], zeros scratch[11:0]
        BuildMI(*MI.getParent(), II, DL, TII->get(RVX::LUI), ScratchReg)
            .addImm(Hi);

        // ADDI scratch, scratch, Lo  — adds the low 12-bit signed offset
        BuildMI(*MI.getParent(), II, DL, TII->get(RVX::ADDI), ScratchReg)
            .addReg(ScratchReg, RegState::Kill) // kill: scratch is redefined here
            .addImm(Lo);

        // ADD scratch, scratch, FrameReg  — adds the base register
        BuildMI(*MI.getParent(), II, DL, TII->get(RVX::ADD), ScratchReg)
            .addReg(ScratchReg, RegState::Kill)
            .addReg(FrameReg);
    }

    // Replace the frame index operand with ScratchReg.
    MI.getOperand(FIOperandNum).ChangeToRegister(ScratchReg, /*isDef=*/false,
                                                /*isImp=*/false,
                                                /*isKill=*/true);
    // Set the offset to 0 — the full offset is now baked into ScratchReg.
    MI.getOperand(FIOperandNum - 1).ChangeToImmediate(0);

}

Register RVXRegisterInfo::getFrameRegister(const MachineFunction &MF)const{
    const RVXFrameLowering *TFI  = 
    static_cast<const RVXFrameLowering*>(getFrameLowering(MF)); 
    return TFI->hasFP(MF) ? RVX::X8 : RVX::X2; 
}

bool RVXRegisterInfo::requiresRegisterScavenging(
    const MachineFunction &MF) const {
    // Enable scavenging whenever the function has a stack frame.
    return MF.getFrameInfo().hasStackObjects();
}

bool RVXRegisterInfo::requiresFrameIndexScavenging(
    const MachineFunction &MF) const {
  return requiresRegisterScavenging(MF);
}

bool RVXRegisterInfo::trackLivenessAfterRegAlloc(
    const MachineFunction &MF) const {
  // Liveness must be tracked after register allocation if we use scavenging.
  return requiresRegisterScavenging(MF);
}





