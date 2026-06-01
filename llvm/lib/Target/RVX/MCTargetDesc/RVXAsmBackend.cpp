#include "RVXAsmBackend.h"
#include "RVXELFObjectWriter.h"  
#include "RVXFixupKinds.h"       

#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"       
#include "llvm/MC/MCContext.h"      
#include "llvm/MC/MCDirectives.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCFixupKindInfo.h"   
#include "llvm/MC/MCFragment.h"         
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"            
#include "llvm/Support/EndianStream.h"  
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>
#include <llvm-14/llvm/ADT/ArrayRef.h>
#include <llvm-14/llvm/MC/MCTargetOptions.h>

using namespace llvm

RVXAsmBackend::RVXAsmBackend(const MCSubtargetInfo *STI, uint8_t OSABI, 
                             bool is64Bit, const MCTargetOptions &Options)
    : MCAsmBackend(llvm::endianness::little), 
        STI(STI), 
        OSABI(OSABI), 
        Is64Bit(Is64Bit){}

std::unique_ptr<MCObjectTargetWriter> 
RVXAsmBackend::createObjectTargetWriter() const{
    return createRVXELFObjectTargetWriter(OSABI, Is64bit); 
}

const llvm::MCFixupKindInfo & RVXAsmBackend::getFixupKindInfo(MCFixup Kind) const{

    static const MCFixupKindInfo Infos[RVX::NumTargetFixupKinds] = {  
        {"fixup_rvx_hi20",   0,  32, 0},
        {"fixup_rvx_lo12_i", 0,  32, 0}, 
        {"fixup_rvx_lo12_s", 0,  32, 0}, 
        {"fixup_rvx_branch_pcrel_12", 0, 32, MCFixupKindInfo::FKF_IsPCRel}, 
        {"fixup_rvx_jal_20", 0, 32, MCFixupKindInfo::FKF_IsPCRel}, 
        {"fixup_rvx_call",   0, 64, MCFixupKindInfo::FKF_IsPCRel}, 
        {"fixup_rvx_call_plt", 0, 64, MCFixupKindInfo::FKF_IsPCRel}, 
        {"fixup_rvx_pcrel_hi20", 0, 32, MCFixupKindInfo::FKF_IsPCRel}, 
        {"fixup_rvx_pcrel_lo12_i",    0,  32,  MCFixupKindInfo::FKF_IsPCRel },
        { "fixup_rvx_pcrel_lo12_s",    0,  32,  MCFixupKindInfo::FKF_IsPCRel },
        { "fixup_rvx_32",              0,  32,  0 },
        { "fixup_rvx_64",              0,  64,  0 },
    };

    if(Kind < FirstTargetFixupKind)
        return MCAsmBackend::getFixupKindInfo(Kind); 

    assert(unsigned(Kind - FirstTargetFixupKind) < RVX::NumTargetFixupKinds &&
         "RVXAsmBackend::getFixupKindInfo: fixup kind out of range. "
         "The Infos[] table and the RVX::Fixups enum are out of sync — "
         "did you add a fixup kind to the enum but forget a table entry?");

    return Infos[Kind - FirstTargetFixupKind]; 
}

void RVXAsmBackend::applyFixup(const MCAssembler &Asm, const MCFixup &Fixup, 
                               const MCValue &Target, 
                               MutableArrayRef<char> Data, uint64_t Value,
                               bool IsResolved, 
                               const MCSubtargetInfo *STI) const{

    MCFixupKind Kind = Fixup.getKind(); 
    unsigned Offset = Fixup.getOffset(); 

    if(!IsResolved)
        return; 
    if(Value == 0)
        return; 

    switch(Kind)
    {
        /*Hi20 - LUI /AUIPC upper 20-but field [31:12]*/ 
        case RVX::fixup_rvx_hi20: 
        case RVX::fixup_rvx_pcrel_hi20: {

            /*for pcrel_hi20, Value is already (symbol_address - AUIPC) */ 
            uint64_t Hi20 = ((Value + 0x800) >> 12) & 0xFFFFF; 
            
            uint32_t Instr = support::endian::read<uint32_t>(
                Data.data() + Offset, llvm::endianness::little);

            Instr &= ~0xFFFFF000u; 
            Instr |= (Hi20 & 0xFFFFF) << 12; 
            support::endian::write<uint32_t>(Data.data() + Offset, Instr, 
                                             llvm::endianesss::little); 
            break; 
        }

        /*LO12_I - I-type contigous 12-bit*/ 
        case RVX::fixup_rvx_lo12_i: 
        case RVX::fixup_rvx_pcrel_lo12_i: {

            /*extract lower 12 bit*/ 
            uint64_t Lo12 = Value & 0xFFF; 
            uint32_t Instr = support::endian::read<uint32_t>(
                Data.data() + Offset, llvm::endianness::little); 

            /*clear bits[31:20] */ 
            Instr &= 0xFFF00000u; 

            Instr |= (Lo12 & 0xFFF) << 20;

            support::endian::write<uint32_t>(Data.data() + Offset, Instr, 
                                             llvm::endianness::little); 
            break; 
        }
        /*LO12_S S-type split 12-bit field ([31:25] and [11:7]) */ 
        case RVX::fixup_rvx_lo12_s: 
        case RVX::fixup_rvx_pcrel_lo12_s:{

            uint64_t = Lo12 = Value & 0xFFF; 

            uint32_t Instr = support::endian::read<uint32_t>(
                Data.data() + Offset, llvm::endianness::little);

            Instr &= ~(0xFEOOOOOOu | 0x0000F80u);
            Instr |= ((Lo12 >> 5) & 0x7F) << 25; 
            Instr |= (Lo12 & 0x1F) << 7; 

            support::endian::write<uint32_t>(Data.data() + Offset, Instr,
                                             llvm::endianness::little);
            break;
        }

        /*BRANCH_PCREL_12 -B TYPE scrambled 12-bit branch offset*/ 
        case RVX::fixup_rvx_branch_pcrel_12: {
           
            int64_t SignedValue = static_cast<int64_t>(Value); 
    
            /*branch targeta must be 2-byte akigned. bit O of the offset must be 0*/ 
            if(SignedValue & 1){
                Asm.getContext().reportError(Fixup.getloc(), 
                                             "branch target offset is not 2-byte aligned"); 
                return;
            }
            
            /*signed 13-bit range*/ 
            if (SignedValue < -4096 || SignedValue > 4094) {
                Asm.getContext().reportError(Fixup.getLoc(),
                                             "branch target out of range of B-type instruction "
                                             "(must be within ±4096 bytes)");
                return;
            }

            uint64_t Imm = (static_cast<uint64_t>(SignedValue) >> 1) & 0x1FFF;

            uint32_t Instr = support::endian::read<uint32_t>(
                Data.data() + Offset, llvm::endianness::little);

            // Clear all B-type immediate fields, preserving rs2, rs1, funct3, opcode.
            //   Fields to clear: bit 31, bits [30:25], bits [11:8], bit 7
            //   Combined mask: 0xFE000F80 (bits 31,30:25 = FE000000; bits 11:7 = F80)
            Instr &= ~0xFE000F80u;

            // Place original offset bit 12 (Imm bit 11) → instruction bit 31
            Instr |= ((Imm >> 11) & 0x1) << 31;

            // Place original offset bits [10:5] (Imm bits [9:4]) → instruction bits [30:25]
            Instr |= ((Imm >> 4) & 0x3F) << 25;

            // Place original offset bits [4:1] (Imm bits [3:0]) → instruction bits [11:8]
            Instr |= ((Imm >> 0) & 0xF) << 8;

            // Place original offset bit 11 (Imm bit 10) → instruction bit 7
            Instr |= ((Imm >> 10) & 0x1) << 7;

            support::endian::write<uint32_t>(Data.data() + Offset, Instr,
                                      llvm::endianness::little);
            break;
        }

        /* JAL_20 — J-type scrambled 20-bit jump offset*/ 
        case RVX::fixup_rvx_jal_20: {
            int64_t SignedValue = static_cast<int64_t>(Value);

            // JAL targets must be 2-byte aligned (same rule as branches).
            if (SignedValue & 1) {
                Asm.getContext().reportError(Fixup.getLoc(),
                "JAL target offset is not 2-byte aligned");
            return;
            }

            // 21-bit signed range (bit 0 implicit): [-1MiB, +1MiB-2].
            if (SignedValue < -1048576 || SignedValue > 1048574) {
            Asm.getContext().reportError(Fixup.getLoc(),
                "JAL target out of range (must be within ±1MiB)");
            return;
            }

              // Shift right by 1 to remove the implicit LSB.
            // After >>1, Imm holds 20 bits representing a 21-bit byte offset.
            uint64_t Imm = (static_cast<uint64_t>(SignedValue) >> 1) & 0xFFFFF;

            uint32_t Instr = support::endian::read<uint32_t>(
            Data.data() + Offset, llvm::endianness::little);

            // Clear J-type immediate fields: bits [31:12], preserving rd and opcode.
            Instr &= ~0xFFFFF000u;

            // Place original offset bit 20 (Imm bit 19) → instruction bit 31
            Instr |= ((Imm >> 19) & 0x1) << 31;

            // Place original offset bits [10:1] (Imm bits [9:0]) → instruction bits [30:21]
            Instr |= ((Imm >> 0) & 0x3FF) << 21;

            // Place original offset bit 11 (Imm bit 10) → instruction bit 20
            Instr |= ((Imm >> 10) & 0x1) << 20;

            // Place original offset bits [19:12] (Imm bits [18:11]) → instruction bits [19:12]
            Instr |= ((Imm >> 11) & 0xFF) << 12;

            support::endian::write<uint32_t>(Data.data() + Offset, Instr,
                                      llvm::endianness::little);
            break;
        }

        /*CALL/CALL PLT - AUIPC +JALR */ 
        case RVX::fixup_rvx_call:
        case RVX::fixup_rvx_call_plt: {

            uint64_t Hi20 = ((Value + 0x800) >> 12) & 0xFFFFF; 
            uint64_t Lo12 = Value & 0xFFF; 

            uint32_t AUIPCInstr = support::endian::read<uint32_t>(
                Data.data() + Offset, llvm::endianness::little); 

            /*clear bits [31:12]*/ 
            AUIPCInstr &= ~0xFFFFF000u; 
            AUIPCInstr |= Hi20 << 12; 
            
            support::endian::write<uint32_t>(Data.data() + Offset, AUIPCInstr,
                                             llvm::endianness::little);
            // Patch the JALR instruction at Data[Offset + 4] (second 4 bytes).
            uint32_t JALRInstr = support::endian::read<uint32_t>(
                Data.data() + Offset + 4, llvm::endianness::little);
            JALRInstr &= ~0xFFF00000u;    // clear bits [31:20]
            JALRInstr |= Lo12 << 20;      // set imm[11:0]
            //
            support::endian::write<uint32_t>(Data.data() + Offset + 4, JALRInstr,
                                             llvm::endianness::little);
            break;
        }

        case RVX::fixup_rvx_32: {
            // Write a 4-byte absolute address into a data section (.word symbol).
            // No bit manipulation needed — just write the value directly.
            support::endian::write<uint32_t>(Data.data() + Offset,
                                             static_cast<uint32_t>(Value),
                                             llvm::endianness::little);
            break;
        }     

        case RVX::fixup_rvx_64: {
            support::endian::write<uint64_t>(Data.data() + Offset, Value,
                                             llvm::endianness::little);
            break;
        }

        default:
            llvm_unreachable("RVXAsmBackend::applyFixup — unhandled fixup kind. "
                     "Did you add a new fixup kind without adding a case here?");
    }
}

bool RVXAsmBackend::shouldForceRelocation(const MCAssmbler &Asm, 
                                          const MCFixup &Fixup, 
                                          const MCValue &Target, 
                                          const MCSubtargetInfo *STI){

    switch(Fixup.getKind()){
        case RVX::fixup_rvx_call:
        case RVX::fixup_rvx_call_plt:
            // Always force these to ELF relocations so the linker can perform
            // relaxation (replacing AUIPC+JALR with a shorter sequence when the
            // target is close enough).
            return true;
        default:
            return false;
    }
}

/*We conservately return true for any instruction that has a PC-relative 
* operand, since we won't know if the target is in range until layout */ 
bool RVXAsmBackend::mayNeedRelaxation(const MCInst &Inst, 
                                      const MCSubtargetInfo &STI) const{

    for(const MCOperand &Op : Inst){
        if(Op.isExpr())
            return true; 
    }
    return false; 
}

bool RVXAsmBackend::fixupNeedsRelaxation(const MCFixup &Fixup,
                                        uint64_t Value,
                                        const MCRelaxableFragment *DF,
                                        const MCAsmLayout &Layout) const {

  /* For branch fixups: check if the value fits in a signed 13-bit field.
  * If not, the short branch must be relaxed to a long branch (or branch+JAL).*/ 

  if (Fixup.getKind() == RVX::fixup_rvx_branch_pcrel_12) {
        int64_t SVal = static_cast<int64_t>(Value);
        return SVal < -4096 || SVal > 4094;
  }

  return false;
}

void RVXAsmBackend::relaxInstruction(MCInst &Inst,
                                      const MCSubtargetInfo &STI) const {

    report_fatal_error("RVXAsmBackend::relaxInstruction — relaxation not yet "
                     "implemented for opcode " + Twine(Inst.getOpcode()) +
                     ". Add a case to relaxInstruction() for each compressed "
                     "instruction that needs a longer-form fallback.");
}
bool RVXAsmBackend::writeNopData(raw_ostream &OS, uint64_t Count, 
                                 const MCSubtargetInfo *STI) const{

    /*If count is not a multiplier of 4*/ 
    if(Count % 4 != 0)
        return false; 

    for(uint64_t i = 0; i < Count; i +=4)
        support::endian::write<uint32_t>(OS, 0x00000013, llvm::endianness::little); 

    return true; 
}

MCAsmBackend *LLVM::createRVXAsmBackend(const Target &T, 
                                        const MCSubTargetInfo &STI, 
                                        const MCRegisterInfo *MRI, 
                                        const MCTargetOptions &Option)
{
    const Triple &TT = STI.getTargetTriple(); 
    uint8_t OSABI = MCELFObjectTargetWriter::getOSABI(TT.getOS()); 
    bool is Is64Bit = TT.isArch64Bit(); 
    return new RVXAsmBackend(STI, OSABI, Is64Bit, Options); 
}


