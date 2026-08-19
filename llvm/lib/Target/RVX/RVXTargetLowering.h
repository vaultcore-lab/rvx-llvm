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

    const char *getTargetNodeName(unsigned opcode) const overide; 
}
    
}
