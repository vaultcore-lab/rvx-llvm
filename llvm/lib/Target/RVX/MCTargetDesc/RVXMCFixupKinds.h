#ifndef LLVM_LIB_TARGET_RVX_MCTARGETDESC_RVXFIXUPKINDS_H
#define LLVM_LIB_TARGET_RVX_MCTARGETDESC_RVXFIXUPKINDS_H

#include "llvm/MC/MCFixup.h"

namespace llvm{
namespace  RVX{

enum Fixups{

    //20 bit fixup corresponding to %hi(foo) for instruction lui 
    fixup_rvx_hi20 = FirstTargetFixupKind, 
    //12-bit fixup corresponding to %lo(foo) for instructions like addi 
    fixup_rvx_lo12_i, 
    // 12-bit fixup corresponding to %lo(foo) for S-type store instructions 
    fixup_rvx_lo12_s, 
    // 12-bit fixup corresponding to signed encoding for PC-relative branch offset 
    fixup_rvx_branch_pcrel_12, 
    //20 bit fixup for PC-relative jump offset
    fixup_rvx_jal_20, 
    // Call psudo-instruction which expands 
    // to AUIPX ra, %pcrel_hi(symbol) JALR, ra, ra %pcrel_lo(symbol)
    fixup_rvx_call, 
    //Call when target is a PLT 
    fixup_rvx_call_plt, 
    fixup_rvx_pcrel_hi20, 
    fixup_rvx_pcrel_lo12_i, 
    fixup_rvx_hpcrel_lo12_s,  
    fixup_rvx_32, 
    fixup_rvx_64, 
    NumTargetFixupKinda
}; 
}

}
#endif 
