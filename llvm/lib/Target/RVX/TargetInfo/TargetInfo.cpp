
#include "RVX.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/TargetRegistry.h"
#include <llvm-14/llvm/ADT/Triple.h>


using namespace llvm; 

Target &llvm::getTheRVX32Target(){
    static Target TheRVX32Target; 
    return TheRVX32Target;
}

Target &llvm::getTheRVX64Target(){
    static Target TheRVX64Target; 
    return TheRVX64Target; 
}

extern "C" void LLVMInitializeRVXTargetInfo()
{
    RegisterTarget<Triple::rvx32, true>
        X(getTheRVX32Target(), "rvx32", "rvx 32-bit", "RVX"); 
    RegisterTarget<Triple::rvx64, true>
        Y(getTheRVX64Target(), "rvx64", "rvx 64-bit", "RVX"); 
}


