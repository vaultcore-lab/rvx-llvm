#ifndef LLVM_LIB_TARGET_RVX_RVXSUBTARGET_H
#define LLVM_LIB_TARGET_RVX_RVXSUBTARGET_H

#include <llvm-14/llvm/ADT/StringRef.h>
#include <llvm-14/llvm/ADT/Triple.h>
#define GET_SUBTARGETINFO_HEADER
#include "RVXGenSubtargetInfo.inc"

#include "RVXFrameLowering.h"   
#include "RVXInstrInfo.h"       
#include "RVXTargetLowering.h"  
#include "llvm/CodeGen/SelectionDAGTargetInfo.h" 
#include "llvm/CodeGen/TargetSubtargetInfo.h" 
#include "llvm/IR/DataLayout.h"

namespace llvm{

class StringRef; 
class RVXTargetMachine; 

class RVXSubtarget : public RVXGenSubtargetInfo {

    bool HasStdExtM = false; 
    bool HasStdExtA = false; 
    bool HasStdExtF = false; 
    bool HasStdExtD = false; 
    bool HasStdExtC = false; 

    bool Is64Bit; 

    RVXInstrInfo InstrInfo; 
    RVXFrameLowering FrameLowering; 
    RVXTargetLowering TLInfo; 
    SelectionDAGTargetInfo TSInfo; 
    const RVXTargetMachine &TM; 

public:

    RVXSubtarget(const Triple &TT, StringRef CPU, StringRef FS, 
                 const RVXTargetMachine &TM); 

    void ParseSubtargetFeatures(StringRef CPU, StringRef TuneCPU, StringRef FS); 

    /*returns the CodeGen-level instructio info table*/ 
    const RVXInstrInfo *getInstrInfo() const override {return InstrInfo; }

    /*returnss the register info table */ 
    const RVXRegisterInfo *getRegisterInfo() const override{
        returb &InstrInfo.getRegisterInfo(); 
    }

    const RVXFrameLowering *getFrameLowring() const override{
        return &FrameLowering; 
    }

    const SelectionDAGTargetInfo *getSelectionDAGInfo() const override{
        return &TSInfo; 
    }

    
    bool hasStdExtM() const { return HasStdExtM; }
    bool hasStdExtA() const { return HasStdExtA; }
    bool hasStdExtF() const { return HasStdExtF; }
    bool hasStdExtD() const { return HasStdExtD; }
    bool hasStdExtC() const { return HasStdExtC; }
        
    bool is64Bit() const { return Is64Bit; }
    bool is32Bit() const { return !Is64Bit; }

    unsigned getXLen() const { return Is64Bit ? 64 : 32; }
    MVT getXLenVT() const {return Is64Bit ? MVT::i64 : MVT::i32;}
    
    RVXSubtarget &initializeSubtargetDependencies(const Triple &TT,
                                                  StringRef CPU,
                                                  StringRef FS);
}; 
}

#endif 
