#ifndef LLVM_LIB_TARGET_RVX_MCTARGETDESC_RVXELFOBJECTWRITER_H
#define LLVM_LIB_TARGET_RVX_MCTARGETDESC_RVXELFOBJECTWRITER_H

#include "llvm/MC/MCELFObjectWriter.h"

namespace llvm{

class MCFixup; 
class MCValue; 

class RVXELFObjectTargetWriter : public MCELFObjectTargetWriter{
public:

    RVXELFObjectTargetWriter(bool Is64Bit, uint8_t OSABI);

    ~RVXELFObjectTargetWriter() override = default;

    unsigned getRelocType(MCContext &Ctx, const MCValue &Target,
                          const MCFixup &Fixup, bool IsPCRel) const override;

    bool needsRelocateWithSymbol(const MCValue &Val, const MCSymbol &Sym,
                                 unsigned Type) const override;
};

std::unique_ptr<MCObjectTargetWriter>
createRVXELFObjectTargetWriter(uint8_t OSABI, bool Is64Bit);
}
#endif // LLVM_LIB_TARGET_RVX_MCTARGETDESC_RVXELFOBJECTWRITER_H
