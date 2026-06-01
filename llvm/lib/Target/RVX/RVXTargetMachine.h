#ifndef LLVM_LIB_TARGET_RVX_RVXTARGETMACHINE_H
#define LLVM_LIB_TARGET_RVX_RVXTARGETMACHINE_H

#include "llvm/Target/TargetMachine.h"
#include <llvm-14/llvm/ADT/Triple.h>
#include <llvm-14/llvm/Target/TargetOptions.h>
#include <memory>
#include <optional>
#include "RVXSubtarget.h"

namespace llvm{

class RVXTargetMachine; 
class TargetPassConfig;

class RVXTargetMachine : public LLVMTargetMachine{

    mutable std::unique_ptr<RVXSubtarget> Subtarget;

public:

    RVXTargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                     StringRef FS, const TargetOptions &Options, 
                     std::optional<Reloc::Model> RM, 
                     std::optional<CodeModel::Model> CM, 
                     CodeGenOptLevel OL, bool JIT); 

    const RVXSubtarget *getSubtargetImpl(const Function F) const override; 
    const RVXSubtarget *getSubtargetImpl()const {return Subtarget.get(); }

    TargetPassConfig *createPassConfig(PassManagerBade &PM) override; 

    TargetTransformInfo TargetTransformInfo(const Function &F) const override; 
    
    const RVXInstrInfo *getInstrInfo() const {
        return getSubtargetImpl()->getInstrInfo();
    }

    const RVXFrameLowering *getFrameLowering() const {
        return getSubtargetImpl()->getFrameLowering();
    }

    const RVXTargetLowering *getTargetLowering() const {
        return getSubtargetImpl()->getTargetLowering();
    }
    
    const RVXRegisterInfo *getRegisterInfo() const {
        return getSubtargetImpl()->getRegisterInfo();
    }

private:
    mutable StringMap<std::unique_ptr<RVXSubtarget>> SubtargetMap;
}; 

class RVXPassConfig : public TargetPassConfig {
public:

    RVXPassConfig(RVXTargetMachine &TM, PassManagerBase &PM); 
    RVXTargetMachine &RVXTargetMachine()const; 

    //runs before instruction selection, 
    //Target-specifc IR-level optimizations
    bool addPreISel() override; 

    bool addInstSelector()override; 

    //runs before register allocation and before asm/obj emmision 
    void addPreEmitPass() override; 
}; 
}

#endif 
