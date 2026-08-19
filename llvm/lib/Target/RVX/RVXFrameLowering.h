#ifndef LLVM_LIB_TARGET_RVX_RVXFRAMELOWERING_H
#define LLVM_LIB_TARGET_RVX_RVXFRAMELOWERING_H

#include "llvm/CodeGen/TargetFrameLowering.h"

class MachineBasicBlock; 
class MachineFunction; 
class MachineInstr; 
class RVXSubtarget; 
class RegScavenger; 

class RVXFrameLowering : public TargetFrameLowering{

    const RVXSubtarget &STI; 

public: 

    explicit RVXFrameLowering(const RVXSubtarget &STI); 

    void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override; 
    void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override; 
    bool hasFP(const MachineFunction &MF) const override; 
    void determineCalleeSaves(MachineFunction &MF, BitVector &SavedRegs,
                              RegScavenger *RS) const override;

    StackOffset getFrameIndexReference(const MachineFunction &MF, int FI, 
                                       Register &FrameReg) const override; 

    int getOffsetOfLocalArea() const override { return 0; }

    bool enableShrinkWrapping(const MachineFunction &MF) const override {
        return false; // conservative: disable until tested
    }


}

