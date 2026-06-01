#ifndef LLVM_LIB_TARGET_RVX_MCTARGETDESC_RVXASMBACKEND_H
#define LLVM_LIB_TARGET_RVX_MCTARGETDESC_RVXASMBACKEND_H

#include "RVXFixupKinds.h"             
#include "llvm/MC/MCAsmBackend.h"      
#include "llvm/MC/MCFixupKindInfo.h"   
#include "llvm/MC/MCSubtargetInfo.h"   
#include "llvm/MC/MCTargetOptions.h"   
#include <cstdint>
#include <llvm-14/llvm/ADT/ArrayRef.h>
#include <llvm-14/llvm/MC/MCFixup.h>
#include <llvm-14/llvm/MC/MCInst.h>
#include <memory>


namespace  llvm{

class MCAssembler; 
class MCObjectTargetWriter;
class MCRegisterInfo; 
class raw_ostream

class RVXAsmBackend : public MCAsmBackend{

    const MCSubtargetInfo &STI; 

    const MCTargetOptions &TargetOptions;

    uint8_t OSABI; 
    bool is64bIT; 

public:

    RVXAsmBackend(const MCSubtargetInfo &STI, const MCTargetOptions &Options); 
    ~RVXAsmBackend() override = default;

    void applyFixup(const MCAssembler &Asm, const MCFixup &Fixup, 
                    const MCValue &Target, MutableArrayRef<char> Data,
                    uint64_t Value, bool IsResolved, 
                    const MCSubtargetInfo *STI) const override; 

    /*called once by the MCObjecStreamer when it needs to write the object file */ 
    std::unique_ptr<MCObjectTargetWriter>
        createObjectTargetWriter() const override; 

    /*return MCFixupKindInfo metadata for a given fixup kind */ 
    const MCFixupKindInfo &getFixupKindInfo(MCFixupKind Kind) const override; 

    /*return number of target specific fixup kinds*/ 
    unsigned getNumFixupKinds() const override{
        return RVX::NumTargetFixupKinds; 
    }

    bool mayNeedRelaxation(const MCInst &Inst,
                         const MCSubtargetInfo &STI) const override;


    bool fixupNeedsRelaxation(const MCFixup &Fixup, uint64_t Value,
                             const MCRelaxableFragment *DF,
                             const MCAsmLayout &Layout) const override;

    void relaxInstruction(MCInst &Inst,
                        const MCSubtargetInfo &STI) const override;
    /*fills 'Count' bytes with NOP instruction in the output stream 'OS'
    * called when assembler needs to insert alignment */ 
    bool writeNopData(raw_ostream &OS, uint64_t Count, 
                      Const MCSubtargetInfo *STI) const override; 

    /*if fixup should be emitted as an elf relocation*/ 
    bool shouldForceRelocation(const MCAssembler &Asm, const MCFixup &Fixup, 
                               const MCValue &Target, 
                               const MCSubtargetInfo *STI) override; 
}; 
}

#endif 
