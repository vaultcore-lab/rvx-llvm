#ifndef LLVM_LIB_TARGET_RVX_RVXINSTRINFO_H
#define LLVM_LIB_TARGET_RVX_RVXINSTRINFO_H

#define GET_INSTRINFO_HEADER
#include "RVXGenInstrInfo.inc"

namespace  llvm{

class RVXSubtarget; 
class MachineBasicBlock; 
class MachineInstr; 

class RVXInstrInfo : public RVXGenInstrInfo{

    RVXRegisterInfo RI; 
    const RVXSubtarget &STI; 

public:

    explicit RVXInstrInfo(const RVXSubtarget &STI); 

    const RVXRegisterInfo &getRegisterInfo()const {
        return RI; 
    }

    //Register-to-Regsiter copy 
    void copyPhysReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, 
                     const DebugLoc &DL, MCRegister DstReg, MCRegister SrcReg, 
                     bool IsKill) const overide; 

    /*insert store instruction before MI that writes SrcReg to stack slot*/ 
    void storeRegToStackSlot(MachineBasicBlock &MBB, 
                             MachineBasicBlock::iterator MI, 
                             Register SrcReg, bool IsKill, int FI, 
                             const TargetRegisterClass &RC, 
                             const TargetRegisterInfo *TRI,
                             Register VReg) const overide; 

    /*insert load instruction before MI that reads from the stack slot in FI into DstReg*/ 
    void loadRegFromStackSlot(MachineBasicBlock &MBB, 
                              MachineBasicBlock::iterator MI, 
                              Register DstReg, int FI, 
                              const TargetRegisterClass *RC, 
                              const TargetRegisterInfo *TRI, 
                              Register VReg) const overide; 

    bool analyzeBranch(MachineBasicBlock &MBB, MachineBasicBlock *&TBB, 
                       MachineBasicBlock *&FBB, 
                       SmallVectorImpl<MachineOperand) &Cond, 
                       bool AllowModify) const overide; 

    unsigned insertBranch(MachineBasicBlock &MBB, MachineBasicBlock *TBB, 
                          MachineBasicBlock *FBB, 
                          ArrayRef<MachineOperand> cond, 
                          const DebugLoc &DL, 
                          int *BytesAdded = nullptr) const overide; 

    unsigned removeBranch(MachineBasicBlock &MBB, 
                          int *BytesRemoved = nullptr) const overide; 
    
    /*flips the branch condition in-palce
     * BEQ -> BNE*/ 
    bool reverseBranchCondition(
        SmallVectorImpl<MachineOperand> &Cond) const override; 


    bool isAsCheapAsAMove(const MachineInstr &MI) const overide; 
}; 
  
}

#endif 
