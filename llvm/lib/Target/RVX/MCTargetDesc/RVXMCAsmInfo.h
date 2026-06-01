#ifndef LLVM_LIB_TARGET_RVX_MCTARGETDESC_RVXMCASMINFO_H
#define LLVM_LIB_TARGET_RVX_MCTARGETDESC_RVXMCASMINFO_H

#include "llvm/MC/MCAsmInfoELF.h"


namespace llvm {

class Triple; 

class RVXMCAsmInfo : public MCAsmInfoELF{

    explicit RVXMCAsmInfo(const Triple &TT); 
    
}; 

}

#endif 
