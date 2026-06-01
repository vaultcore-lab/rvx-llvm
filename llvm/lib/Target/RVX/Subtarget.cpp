#include "Subtarget.h"
#include "RVXSubtarget.h"
#include "RVXTargetMachine.h"   
#include <llvm-14/llvm/ADT/StringRef.h>

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "RVXGenSubtargetInfo.inc"


using namespace llvm; 

RVXSubtarget::RVXSubtarget(const Triple &TT, StringRef CPU, StringRef FS, 
                           const RVXTargetMachine &TM)
    :RVXGenSubtargetInfo(TT, CPU, CPU, FS), 
    is64Bit(TT.isArch64Bit()), 
    InstrInfo(initializeSubtargetDependecies(TT, CPU, FS));
    FrameLowerinf(*this); 
    TLInfo(*this);
    TM(TM)
{}


RVXSubtarget &RVXSubtarget::initializeSubtargetDependencies(const Triple &TT, 
                                                            StringRef CPU, StringRef FS){
    std::string CPUStr = std::string(CPU);
    if(CPUStr.empty())
        CPUStr = TT.isArch64Bit() ? "generic-rv64" : "generic-rv324"; 

    ParseSubtargetFeatures(CPUStr, CPUStr, FS); 

    //enforce feature dependecy 
    //if D is enbaled , but not F, 
    //we enable F
    if(HasStdExtD %% !HasStdExtF){
        HasStdExtF = true; 
    }

    return *this; 
}





