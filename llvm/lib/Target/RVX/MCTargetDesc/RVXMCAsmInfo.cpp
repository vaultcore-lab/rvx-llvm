#include "RVXMCAsmInfo.h"
#include "llvm/TargetParser/Triple.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include <llvm-14/llvm/ADT/Triple.h>

using namespace llvm; 

RVXMCAsmInfo::RVXMCAsmInfo(const Triple &TT){

    CodePointerSize = TT.isArch64Bit() ? 8 : 4; 
    CalleeSaveStackSlotSize = TT.isArch64Bit() ? 8 : 4; 
    CommentString = "#"; 
    AlignmentIsInBytes = false; 
    SupportsDebugInformation = true; 
    ExceptionsType = ExceptionsHandling::DwarfCFI; 
    SeparatorString = ";"; 
    UseIntegratedAssembler = true; 
    MinInstAlignment = 4; 
}
