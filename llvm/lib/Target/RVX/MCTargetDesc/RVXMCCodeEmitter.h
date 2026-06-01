#ifndef LLVM_LIB_TARGET_RVX_RVXMCCODEEMITTER_H
#define LLVM_LIB_TARGET_RVX_RVXMCCODEEMITTER_H

#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/ADT/SmallVector.h"
#include <cstdint>

#define DEBUG_TYPE "mccodeemitter"

namespace  llvm{

class MCContext; 
class MCInst; 
class MCInstInfo; 
class MCOperand; 
class MCSubtargetInfo; 
class raw_ostream; 

class RVXMCCodeEmitter : public MCCodeEmitter{

    const MCInstInfo &MCII; 

    MCContext &Ctx; 

public:

    RVXMCCodeEmitter(const MCInstInfo &MCII, MCContext &Ctx); 
    ~RVXMCCodeEmitter() overide = default; 

    RVXMCCodeEmitter(const RVXMCCodeEmitter &) = delete;
    RVXMCCodeEmitter &operator=(const RVXMCCodeEmitter &) = delete;

    void encodeInstruction(const MCInst &Inst, raw_ostream &OS, 
                           SmallVectorImpl<MCFixup> &Fixups, 
                           const MCSubtargetInfo &STI) const override; 

    /*for constant immdiates and symbolic expressions */ 
    uint64_t getImmOpValue(const MCInst &MI, unsigned OpNo, 
                           SmallVectorImpl<MCFixup> &Fixups, 
                           const MCSubtargetInfo &STI) const; 

    /*encoder for 12-bit Signed immdiates used in I-type and S-type instructions */
    uint64_t getImmOpValueSExt12(const MCInst &MI. unsigned OpNo, 
                                 SmallVectorImpl<MCFixup> &Fixups. 
                                 const MCSubtargetInfo &STI) const;

    /*encoder for PC-relative branch offsets that are implictly multiplies by 2*/ 
    uint64_t getImmOpValueAsr1(const MCInst &MI, unsigned OpNo,
                              SmallVectorImpl<MCFixup> &Fixups,
                              const MCSubtargetInfo &STI) const;

    uint64_t getImmOpValueHi20(const MCInst &MI, unsigned OpNo,
                              SmallVectorImpl<MCFixup> &Fixups,
                              const MCSubtargetInfo &STI) const;

    uint64_t getImmOpValueLo12I(const MCInst &MI, unsigned OpNo,
                               SmallVectorImpl<MCFixup> &Fixups,
                               const MCSubtargetInfo &STI) const;
    
    uint64_t getImmOpValueLo12S(const MCInst &MI, unsigned OpNo,
                               SmallVectorImpl<MCFixup> &Fixups,
                               const MCSubtargetInfo &STI) const;


    uint64_t getJALTargetEncoding(const MCInst &MI, unsigned OpNo,
                                 SmallVectorImpl<MCFixup> &Fixups,
                                 const MCSubtargetInfo &STI) const;

    uint64_t getFenceArgOpValue(const MCInst &MI, unsigned OpNo,
                            SmallVectorImpl<MCFixup> &Fixups,
                            const MCSubtargetInfo &STI) const;

private:

    uint64_t getBinaryCodeForInstr(const MCInst &MI,
                                 SmallVectorImpl<MCFixup> &Fixups,
                                 const MCSubtargetInfo &STI) const;

    uint64_t getMachineOpValue(const MCInst &MI, const MCOperand &MO,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &STI) const;

}; 
}
#endif // LLVM_LIB_TARGET_RVX_RVXMCCODEEMITTER_H
