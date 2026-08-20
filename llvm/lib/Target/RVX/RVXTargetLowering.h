#ifndef LLVM_LIB_TARGET_RVX_RVXTARGETLOWERING_H
#define LLVM_LIB_TARGET_RVX_RVXTARGETLOWERING_H

#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {

class RVXSubtarget;     // feature bits, is64Bit(), XLenVT
class RVXTargetMachine; // relocation model, code model 
//
class RVXTargetLowering : public TargetLowering{
    
    const RVXSubtarget &STI; 

public: 

    RVXTargetLowering(const TargetMachine &TM, cosnt RVXSubtarget &STI); 

    const char *getTargetNodeName(unsigned opcode) const override; 

    //lower illegal Nodes to legal one 
    SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const overide; 

    //create Nodes for arguments 
    //copy from register to SDNodes 
    SDValue LowerFormalArguments(
        SDValue Chain, ClallingConv::ID ClallConv, bool isVarArg, 
        const SmallVectorImpl<ISD:::InputArg> &Ins, const SDLoc &DL, 
        SelectionDAG &DAG, 
        SmallVectorImpl<SDValue> &InVals) const override; 

    //lower function calls to SDNodes
    SDValue LowerCall(TargetLowering::CallLoweringInfo &CLI,
                    SmallVectorImpl<SDValue> &InVals) const override;

    //lower return to SDNodes
    SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
                      SelectionDAG &DAG) const override;

    SDValue LowerGlobalAddress(SDValue Op, SelectionDAG &DAG) const; 

    SDValue LowerBlockAddress(SDValue Op, SelectionDAG &DAG) const; 

    SDValue LowerExternalSymbol(SDValue Op, SelectionDAG &DAG) const;

    SDValue LowerVASTART(SDValue Op, SelectionDAG &DAG) const;
    
    SDValue LowerSELECT(SDValue Op, SelectionDAG &DAG) const;

    SDValue LowerBRCOND(SDValue Op, SelectionDAG &DAG) const;

    SDValue LowerFrameIndex(SDValue Op, SelectionDAG &DAG) const;

    DValue LowerJumpTable(SDValue Op, SelectionDAG &DAG) const;

    SDValue LowerConstantPool(SDValue Op, SelectionDAG &DAG) const;

    TargetLowering::ConstraintType 
    getConstraintType(StringRef Constraint) const override;

    unsigned 
    getInlineAsmMemConstraint(StringRef ConstraintCode) const override;

    bool isLegalAddressingMode(const DataLayout &DL, const AddrMode &AM,
                              Type *Ty, unsigned AS,
                              Instruction *I) const override;

    bool isLegalICmpImmediate(int64_t Imm) const override;

    bool isLegalAddImmediate(int64_t Imm) const override;

    bool isTruncateFree(Type *SrcTy, Type *DstTy) const override;
    bool isTruncateFree(EVT SrcVT, EVT DstVT) const override;

    bool isZExtFree(SDValue Val, EVT VT2) const override;


private: 
    MVT getXLenVT() const; 

     SDValue lowerSymbolAddress(SDValue Op, SelectionDAG &DAG,
                              SDValue TargetNode) const;

    int getReturnAddressFrameIndex(MachineFunction &MF) const;

    void analyzeInputArgs(MachineFunction &MF, CCState &CCInfo,
                        const SmallVectorImpl<ISD::InputArg> &Ins,
                        bool IsRet) const;
    void analyzeOutputArgs(MachineFunction &MF, CCState &CCInfo,
                         const SmallVectorImpl<ISD::OutputArg> &Outs,
                         bool IsRet, CallLoweringInfo *CLI) const;
}; 
   


}
