#ifndef LLVM_LIB_TARGET_RVX_RVXISELDAGTODAG_H
#define LLVM_LIB_TARGET_RVX_RVXISELDAGTODAG_H

#include "llvm/CodeGen/SelectionDAGISel.h"

#include "RVXSubtarget.h"
   

namespace  llvm{

class RVXTargetMachine; 

class RVXDAGToDAGISEel : public SelectionDAGISel{
    const RVXSubtarget &Subtarget = nullptr; 

public: 

    explicit RVXDAGToDAGISEel(RVXTargetMachine &TM, CodeGenOptLevel OptLevel); 

    StringRef getPassName() cojst override{
        return "RVX DAG->DAG Instruction Selection"; 
    }

    bool runOnMachineFunction(MachineFunction &MF) override; 

    void Select(SDNode *N) override; 

    bool SelectAddrRegImm(SDValue Addr, SDValue &Base, SDValue &Offset);

    bool SelectAddrFrameIndex(SDValue Addr, SDValue &Base, SDValue &Offset); 

    bool SelectInlineAsmMemoryOperand(const SDValue &Op, InlineAsm::ConstraintCode ConstraintCode, 
                                      std::vector<SDValue> &OutsOps) override; 

private: 
    // This method is used in Select() when we see an ISD::Constant node
    // that the TableGen patterns couldn't handle (because the constant is
    // too large for a simple immediate field).
    SDNode *selectImm(SDNode *N, const SDLoc &DL, int64_t Imm, MVT VT);


    SDValue getImm(const SDNode *Node, uint64_t Imm); 

    // Returns true if ALL users of SDNode N consume only the high 32 bits
    // of N's result. Used on RV64 to decide whether a 32-bit operation can
    // be replaced by its 64-bit equivalent without additional zero/sign
    // extension, because the upper bits don't matter to any consumer.
    bool hasAllHUsers(SDNode *N) const;

    bool isOrEquivalentToAdd(const SDNode *N) const;

    void SelectCode(SDNode *N);

}; 

FunctionPass *createRVXISelDag(RVXTargetMachine &TM, 
                                   CodeGenOptLevel OptLevel); 
}
#endif 


