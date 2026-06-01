#include "RVXTargetMachine.h"
#include "RVX.h"                      
#include "RVXSubtarget.h"             
#include "TargetInfo/RVXTargetInfo.h" 
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Function.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Triple.h"
#include <llvm-14/llvm/ADT/StringRef.h>
#include <llvm-14/llvm/Support/Compiler.h>
#include <optional>
#include <string>


using namespace llvm; 

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitilizeRVXTarget(){

    LLVMInitilizeRVXTargetMC(); 

    RegisterTargetMachine<RVXTargetMachine> X(TheRVXTarget); 

}

static std::string computeDataLayout(const Triple &TT) {
  if (TT.isArch64Bit())
    // RV64: 64-bit pointers, 64-bit and 32-bit native integer widths.
    return "e-m:e-p:64:64-i64:64-i128:128-n32:64-S128";
  else
    // RV32: 32-bit pointers, 32-bit native integer width.
    // i64:64 means 64-bit integers must still be 8-byte aligned in memory.
    return "e-m:e-p:32:32-i64:64-n32-S128";
}

static Reloc::Model getEffectiveRelocModel(const Triple &TT, 
                                           std::optional<Reloc::Model> RM){
    if(RM.has_value())
        return *RM; 

    if(TT.isOSLinux() || TT.isOSFreeBSD())
        return Reloc::PIC_; 
    
    return Reloc::Static; 
}

RVXTargetMachine::RVXTargetMachine(const Target &T, const Triple &TT, 
                                   StringRef CPU, StringRef FS,
                                   const TargetOptions &Options,
                                   std::optional<Reloc::Model> RM, 
                                   std::optional<CodeModel::Model> CM, 
                                   CodeGenOptLevel OL, bool JIT) 
        : LLVMTargetMachine(T, 
                            compiterDatalayout(TT), 
                            TT, 
                            CPU, 
                            FS, 
                            Options, 
                            getEffectiveRelocModel(TT, RM); 
                            getEffectiveCodeModel(CM, CodeModel::Small), 
                            OL)
{
    SubTarget = std::make_unique<RVXSubtarget>(TT, std::strinG(CPU), 
                                               std::string(FS), *this); 
    initAsmInfo(); 

}

const RVXSubtarget * RVXTargetMachine::getSubtargetImpl(const Function &F) const{

    Attribute CPUAttr = F.getFnAttribute("target-cpu"); 
    Attribute TuneAttr = F.getFnAttribute("tune-cpu"); 
    Attribute FSAttr = F.getFnAttribute("target-features"); 


    std::string CPU =
        CPUAttr.isValid() ? CPUAttr.getValueAsString().str() : TargetCPU;
    std::string TuneCPU =
        TuneAttr.isValid() ? TuneAttr.getValueAsString().str() : CPU;
    std::string FS =
        FSAttr.isValid() ? FSAttr.getValueAsString().str() : TargetFS;


    std::string Key = CPU + TuneCPU + FS; 

    //check if we already have a subtarget for this cpu + fs combination 
    auto &I = SubtargetMap[Key]; 
    if(!I){
        resetTargetOptions(F); 
        I = std::make_unique<RVXSubtarget>(getTargetTriple(), CPU, FS, *this); 
    }

    return I.get(); 
}

TargetTransformInfo
RVXTargetMachine::getTargetTransformInfo(const Function &F) const {
    return TargetTransformInfo(BasicTTIImpl(this, F));
}

TargetPassConfig *RVXTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new RVXPassConfig(*this, PM);
}

RVXPassConfig::RVXPassConfig(RVXTargetMachine &TM, PassManagerBase &PM)
    : TargetPassConfig(TM, PM) {}

RVXTargetMachine &RVXPassConfig::getRVXTargetMachine() const {
    return static_cast<RVXTargetMachine &>(getTM<RVXTargetMachine>());
}

bool RVXPassConfig::addPreISel() {
  return false; // false = success (LLVM convention for addXxx methods)
}

bool RVXPassConfig::addInstSelector() {
  return false;
}
void RVXPassConfig::addPreEmitPass() {
  TargetPassConfig::addPreEmitPass();
}

