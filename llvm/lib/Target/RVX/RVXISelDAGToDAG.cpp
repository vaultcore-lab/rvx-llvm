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


