#include "RVXELFObjectWriter.h"
#include "RVXFixupKinds.h"             // RVX::fixup_rvx_xxx enum
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/ErrorHandling.h"
#include <llvm-14/llvm/MC/MCELFObjectWriter.h>

using llvm{


RVXELFObjectTargetWriter::RVXELFObjectTargetWriter(bool Is64Bit, uin8_t OSABI)
    : MCELFObjectTargetWriter(
        Is64Bit, 
        OSABI, 
        ELF::EM_RISCV, 
        true; 
    ){}


unsigned RVXELFObjectTargetWriter::getRelocType(MCContext &CTX, 
                                                const MCValue &Target, 
                                                const MCFixup &Fixup, 
                                                bool IsPCRel) const {

    MCFixupKind Kind = Fixup,getKind(); 
    
    if(Kind == FK_Data_4)
        return ELF::R_RISCV_32; 

    if (Kind == FK_Data_8)
        return ELF::R_RISCV_64;

    if (Kind == FK_PCRel_4)
        return ELF::R_RISCV_32_PCREL;

    switch(Kind){
        
        case RVX::fixup_rvx_hi20:
            return ELF::R_RISCV_HI20;

        case RVX::fixup_rvx_lo12_i:
            return ELF::R_RISCV_LO12_I;

        case RVX::fixup_rvx_lo12_s:
            return ELF::R_RISCV_LO12_S;
    
        case RVX::fixup_rvx_branch_pcrel_12:
            assert(IsPCRel && "fixup_rvx_branch_pcrel_12 must be PC-relative");
            return ELF::R_RISCV_BRANCH;

        case RVX::fixup_rvx_call:
            assert(IsPCRel && "fixup_rvx_call must be PC-relative");
            return ELF::R_RISCV_CALL;

        case RVX::fixup_rvx_call_plt:
            assert(IsPCRel && "fixup_rvx_call_plt must be PC-relative");
            return ELF::R_RISCV_CALL_PLT;

        case RVX::fixup_rvx_pcrel_hi20:
            assert(IsPCRel && "fixup_rvx_pcrel_hi20 must be PC-relative");
            return ELF::R_RISCV_PCREL_HI20;

        case RVX::fixup_rvx_pcrel_lo12_i:
            return ELF::R_RISCV_PCREL_LO12_I;

        case RVX::fixup_rvx_pcrel_lo12_s:
            return ELF::R_RISCV_PCREL_LO12_S;


        case RVX::fixup_rvx_32:
            return ELF::R_RISCV_32;

        case RVX::fixup_rvx_64:
            return ELF::R_RISCV_64;
    
        default:
            Ctx.reportError(Fixup.getLoc(),
                    "RVXELFObjectWriter: unrecognised fixup kind " +
                    Twine(unsigned(Kind)) +
                    " — add a case to getRelocType() in RVXELFObjectWriter.cpp");
            return ELF::R_RISCV_NONE;

    }

}
bool RVXELFObjectTargetWriter::needsRelocateWithSymbol(
        const MCValue &Val, const MCSymbol &Sym, unsigned Type) const {
  
    switch (Type) {
    
    case ELF::R_RISCV_CALL:
    case ELF::R_RISCV_CALL_PLT:
        // Call relocations must reference the symbol directly so the linker
        // can apply PLT redirection and linker relaxation correctly.
        return true;

    default:
        // All other relocation types can use section-relative references.
        // This produces smaller relocation tables and is the preferred format
        // for local relocations within the same translation unit.
        return false;
  }
}

std::unique_ptr<MCObjectTargetWriter>
    llvm::createRVXELFObjectTargetWriter(uint8_t OSABI, bool Is64Bit) {

    return std::make_unique<RVXELFObjectTargetWriter>(Is64Bit, OSABI);
}
