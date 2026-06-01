#ifndef LLVM_LIB_TARGET_RVX_RVXINSTPRINTER_H
#define LLVM_LIB_TARGET_RVX_RVXINSTPRINTER_H

#include "llvm/MC/MCInstPrinter.h"
#include <llvm-14/llvm/ADT/Optional.h>
#include <llvm-14/llvm/ADT/StringRef.h>

namespace llvm{

class MCOperand; 
class MCSubtargetInfo; 

class RVXInstPrinter : public MCInstPrinter{
public:
    RVXInstPrinter(const MCAsmInfo &MAI, 
                   const MCInstrInfo &MII, 
                   const MCRegisterInfo &MRI)
        : MCInstPrinter(MAI, MII, MRI) {}


    /*top level entry point called by the MC streamer /objdump */ 
    void printInst(const MCInst *MI, uint64_t Address, StringRef Annot,
                   const MCSubtargetInfo &sSTI, raw_ostream &OS) override; 

    /*prints register number as it's ABI name to OS */ 
    void printRegName(raw_ostream &OS, MCRegister RegNo) const override;

    /*called by printInstruction for each operand placeholder in AsmString */ 
    void printOperand(const MCInst *MI, unsigned OpNo, raw_ostream &OS); 

    void printMemeOperand(const MCInst *MI, unsigned OpNo, raw_ostream &OS); 

    void printCSRSystemRegister(const MCInst *MI, unsigned OpNo,
                               raw_ostream &OS);

    void printFenceArg(const MCInst *MI, unsigned OpNo, raw_ostream &OS);

    std::pair<const char *, uint64_t> getMneumonic(const MCInst &MI) override; 
    void printInstruction(const MCInst *MI, uint64_t Address,
                          raw_ostream &O); 
    void printAliasInstr(const MCInst *MI, uint64_t Address, 
                         raw_ostream &O); 

    static const char *getRegisterName(MCRegister Reg); 
    static const char *getRegisterName(MCRegister Reg, unsigned AltIdx); 
}

}
