#ifndef LLVM_LIB_TARGET_RVX_RVXREGISTERINFO_H
#define LLVM_LIB_TARGET_RVX_RVXREGISTERINFO_H

#include "Subtarget.h"
#include <cstdint>
#include <llvm-14/llvm/ADT/BitVector.h>
#include <llvm-14/llvm/CodeGen/MachineMemOperand.h>
#include <llvm-14/llvm/CodeGen/SelectionDAGNodes.h>
#include <llvm-14/llvm/CodeGen/TargetSubtargetInfo.h>
#include <llvm-14/llvm/MC/MCRegister.h>
#define GET_REGINFO_HEADER
#include "RVXGenRegisterInfo.inc"


namespace llvm {

class RVXSubtarget; 
class MachineFunction; 
class MachineInstr; 
class RegScavenger; 

class RVXRegisterInfo : public RVXGenRegisterInfo {

public:

    RVXRegisterInfo(const RVXSubtarget &ST); 
    
    const MCPhysReg *getCalleeSavedRegs(const MachineFunction *MF) const override;
    const uint32_t *getCallPreservedMask(const MachineFunction &MF,
                                         CallingConv::ID CC) const override; 

    /*bitvector of registers that are permanently unavailable for
     * register allocation */ 
    BitVector getReservedRegs(const MachineFunction &MF) const override; 

    /*replace a frame index pseudo-operand in MI with real base + offser pair */ 
    void eliminateFrameIndex(MachineBasicBlock::iterator II, int SPAdj, 
                             unsigned FIOperanddNum, 
                             RegScavenger *RS = nullptr) const override; 

    Register getFrameRegister(const MachineFunction &MF) const override;
    
    bool requiresRegisterScavenging(const MachineFunction &MF) const override;

    bool trackLivenessAfterRegAlloc(const MachineFunction &MF) const override; 
}; 
    
}

#endif 
